/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#include "skeletontools.h"

#include "nifskope.h"
#include "glview.h"
#include "gl/glscene.h"
#include "model/nifmodel.h"
#include "wwskin.h"

#include <functional>

#include <QAction>
#include "nifsnapshot.h"

#include <QCheckBox>
#include <QInputDialog>
#include <QMenu>
#include <QShortcut>
#include <QMessageBox>
#include <QDockWidget>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QToolButton>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <QHash>
#include <QSet>

/*! \file skeletontools.cpp
 * \brief Skeleton Manager workspace dock — SKELETON_AND_POSE_PLAN.md Part A.
 *
 * Blender's Armature Edit Mode + Bone properties + the Outliner's armature view,
 * in NifSkope's flat visual language.
 *
 * **Phase 1 of §A.7 only: read-only.** Hierarchy, bone/deforming/unused
 * classification, influence and weight counts, selection sync, rest-pose toggle.
 * It writes nothing, which is the whole point of doing it first — the dangerous
 * work (§A.3 bone transforms with inverse-bind rebinding) waits for its gauntlet.
 *
 * Deliberately NOT here yet, in the plan's order: validation + prune (phase 2),
 * bone transform + rebind (phase 3), the persistent skeleton reference slot and
 * octahedral viewport bones (phase 4). The report already computes the two
 * validation findings that are free (dangling skin bones, duplicate names) and
 * surfaces them, because the analysis pass has to detect them anyway.
 */

// ---------------------------------------------------------------------------
// analysis (model layer, shared with the CLI)
// ---------------------------------------------------------------------------

int SkeletonReport::deformingCount() const
{
	int n = 0;
	for ( const SkeletonBoneInfo & b : bones )
		if ( b.verts > 0 )
			n++;
	return n;
}

int SkeletonReport::unusedCount() const
{
	int n = 0;
	for ( const SkeletonBoneInfo & b : bones )
		if ( b.isUnusedBone() )
			n++;
	return n;
}

namespace
{

//! A shape's skin instance. Both spellings exist across versions.
QModelIndex skelSkinInstance( const NifModel * nif, const QModelIndex & shape )
{
	int link = nif->getLink( shape, "Skin" );
	if ( link < 0 )
		link = nif->getLink( shape, "Skin Instance" );
	return nif->getBlockIndex( link );
}

bool skelIsTriShape( const NifModel * nif, const QModelIndex & index )
{
	return nif && index.isValid()
		&& ( nif->blockInherits( index, "NiTriBasedGeom" ) || nif->blockInherits( index, "BSTriShape" ) );
}

/*! Bone node block numbers a shape is skinned to, in bone-index order.
 *
 * Order is load-bearing: `Bone Indices` on a vertex indexes into this list, so a
 * reordered list silently rebinds the mesh. -1 marks an entry that does not
 * resolve to a block (see SkeletonReport::danglingSkinBones).
 */
QList<int> skelSkinBoneBlocks( const NifModel * nif, const QModelIndex & shape )
{
	QList<int> blocks;
	QModelIndex iSkin = skelSkinInstance( nif, shape );
	if ( !iSkin.isValid() )
		return blocks;

	// Probing for a nested "Bones" would hit array element zero, because array
	// elements repeat their field name — the same trap riggingBoneNames() notes.
	QModelIndex iBones = nif->getIndex( iSkin, "Bones" );
	if ( !iBones.isValid() )
		return blocks;

	for ( int n = 0; n < nif->rowCount( iBones ); n++ ) {
		const int link = nif->getLink( nif->getIndex( iBones, n ) );
		blocks << ( nif->getBlockIndex( link ).isValid() ? link : -1 );
	}
	return blocks;
}

} // namespace

SkeletonReport skeletonAnalyse( const NifModel * nif, float threshold )
{
	SkeletonReport report;
	if ( !nif )
		return report;

	const int blockCount = nif->getBlockCount();

	// --- collect nodes and their parenting ---------------------------------
	// Parent comes from the Children arrays rather than any stored back-link:
	// the scene graph is the authority, and a node can be referenced as a bone
	// while sitting anywhere in the tree.
	QHash<int, int> parentOf;
	QList<int> nodeBlocks;
	for ( int b = 0; b < blockCount; b++ ) {
		QModelIndex idx = nif->getBlockIndex( b );
		if ( !idx.isValid() || !nif->blockInherits( idx, "NiAVObject" ) )
			continue;
		if ( skelIsTriShape( nif, idx ) )
			continue;			// geometry is not skeleton
		nodeBlocks << b;
	}
	QSet<int> isNode( nodeBlocks.begin(), nodeBlocks.end() );
	for ( int b : nodeBlocks ) {
		QModelIndex iChildren = nif->getIndex( nif->getBlockIndex( b ), "Children" );
		if ( !iChildren.isValid() )
			continue;
		for ( int n = 0; n < nif->rowCount( iChildren ); n++ ) {
			const int child = nif->getLink( nif->getIndex( iChildren, n ) );
			if ( child >= 0 && isNode.contains( child ) )
				parentOf.insert( child, b );
		}
	}

	// --- accumulate skin influence ----------------------------------------
	struct Accum { int shapes = 0; int verts = 0; double weight = 0.0; bool inSkin = false; };
	QHash<int, Accum> accum;
	QSet<QString> dangling;

	for ( int b = 0; b < blockCount; b++ ) {
		QModelIndex shape = nif->getBlockIndex( b );
		if ( !skelIsTriShape( nif, shape ) )
			continue;
		QModelIndex iSkin = skelSkinInstance( nif, shape );
		if ( !iSkin.isValid() )
			continue;

		const QList<int> boneBlocks = skelSkinBoneBlocks( nif, shape );
		if ( boneBlocks.isEmpty() )
			continue;
		report.skinnedShapes++;

		const QString shapeName = nif->get<QString>( shape, "Name" );
		for ( int i = 0; i < boneBlocks.size(); i++ ) {
			if ( boneBlocks.at( i ) < 0 ) {
				dangling << QObject::tr( "%1: skin bone %2 does not resolve to a block" )
					.arg( shapeName.isEmpty() ? QObject::tr( "<shape %1>" ).arg( b ) : shapeName )
					.arg( i );
				continue;
			}
			Accum & a = accum[boneBlocks.at( i )];
			a.inSkin = true;
			a.shapes++;
		}

		// FO4 path: per-vertex weights live on the shape's Vertex Data rows.
		QModelIndex iVD = nif->getIndex( shape, "Vertex Data" );
		if ( iVD.isValid() && nif->rowCount( iVD ) > 0 ) {
			const int nv = nif->rowCount( iVD );
			for ( int v = 0; v < nv; v++ ) {
				QModelIndex iVertex = nif->getIndex( iVD, v );
				QModelIndex iW = nif->getIndex( iVertex, "Bone Weights" );
				QModelIndex iI = nif->getIndex( iVertex, "Bone Indices" );
				if ( !iW.isValid() || !iI.isValid() )
					continue;
				const int nw = qMin( nif->rowCount( iW ), nif->rowCount( iI ) );
				for ( int j = 0; j < nw; j++ ) {
					const float w = nif->get<float>( nif->getIndex( iW, j ) );
					const int bi = nif->get<quint8>( nif->getIndex( iI, j ) );
					if ( w <= threshold || bi < 0 || bi >= boneBlocks.size() )
						continue;
					const int boneBlock = boneBlocks.at( bi );
					if ( boneBlock < 0 )
						continue;
					Accum & a = accum[boneBlock];
					a.verts++;
					a.weight += double( w );
				}
			}
			continue;
		}

		// Classic path: NiSkinData carries a Vertex Weights list per bone, in the
		// same order as the Bones array.
		QModelIndex iData = nif->getBlockIndex( nif->getLink( iSkin, "Data" ) );
		QModelIndex iBoneList = iData.isValid() ? nif->getIndex( iData, "Bone List" ) : QModelIndex();
		if ( !iBoneList.isValid() )
			continue;
		const int nb = qMin( nif->rowCount( iBoneList ), int( boneBlocks.size() ) );
		for ( int i = 0; i < nb; i++ ) {
			if ( boneBlocks.at( i ) < 0 )
				continue;
			QModelIndex iVW = nif->getIndex( nif->getIndex( iBoneList, i ), "Vertex Weights" );
			if ( !iVW.isValid() )
				continue;
			Accum & a = accum[boneBlocks.at( i )];
			for ( int v = 0; v < nif->rowCount( iVW ); v++ ) {
				const float w = nif->get<float>( nif->getIndex( iVW, v ), "Weight" );
				if ( w <= threshold )
					continue;
				a.verts++;
				a.weight += double( w );
			}
		}
	}

	report.danglingSkinBones = QStringList( dangling.begin(), dangling.end() );
	report.danglingSkinBones.sort();

	// --- emit in hierarchy order, parents before children ------------------
	QHash<int, QList<int>> childrenOf;
	QList<int> roots;
	for ( int b : nodeBlocks ) {
		const int p = parentOf.value( b, -1 );
		if ( p < 0 )
			roots << b;
		else
			childrenOf[p] << b;
	}
	if ( !roots.isEmpty() )
		report.rootBlock = roots.first();

	QHash<QString, int> nameCount;
	std::function<void( int, int )> emitNode = [&]( int block, int depth ) {
		SkeletonBoneInfo info;
		info.block = block;
		info.name = nif->get<QString>( nif->getBlockIndex( block ), "Name" );
		info.parent = parentOf.value( block, -1 );
		info.depth = depth;
		const Accum a = accum.value( block );
		info.inSkin = a.inSkin;
		info.shapes = a.shapes;
		info.verts = a.verts;
		info.weight = a.weight;
		report.bones << info;
		if ( !info.name.isEmpty() )
			nameCount[info.name]++;
		for ( int c : childrenOf.value( block ) )
			emitNode( c, depth + 1 );
	};
	for ( int r : roots )
		emitNode( r, 0 );

	// A node can be referenced as a bone without being reachable from a root
	// (orphaned by an earlier edit). Report it rather than dropping it silently.
	QSet<int> emitted;
	for ( const SkeletonBoneInfo & b : report.bones )
		emitted << b.block;
	for ( int b : nodeBlocks ) {
		if ( emitted.contains( b ) )
			continue;
		SkeletonBoneInfo info;
		info.block = b;
		info.name = nif->get<QString>( nif->getBlockIndex( b ), "Name" );
		info.depth = 0;
		const Accum a = accum.value( b );
		info.inSkin = a.inSkin;
		info.shapes = a.shapes;
		info.verts = a.verts;
		info.weight = a.weight;
		report.bones << info;
		if ( !info.name.isEmpty() )
			nameCount[info.name]++;
	}

	for ( auto it = nameCount.constBegin(); it != nameCount.constEnd(); ++it )
		if ( it.value() > 1 )
			report.duplicateNames << it.key();
	report.duplicateNames.sort();

	return report;
}

// ---------------------------------------------------------------------------
// dock
// ---------------------------------------------------------------------------

namespace
{

enum SkelFilter { FilterAll = 0, FilterBones, FilterDeforming, FilterUnused };

/*! Boxed filter-button styling.
 *
 * nifskope_ui.cpp's wwBoxedButtonQss() is `static`, so it cannot be shared;
 * this reproduces the same look from the same source of truth. Every colour
 * comes from skinVars via wwSkinColor() — no literals, per the skin rule.
 */
QString skelBoxedButtonQss()
{
	return QStringLiteral(
		"QToolButton { background: %1; color: %2; border: 1px solid %3;"
		" border-radius: 3px; padding: 3px 8px; }"
		"QToolButton:hover { background: %4; }"
		"QToolButton:checked { background: %5; color: %6; border-color: %7; }" )
		.arg( wwSkinColor( "bgBtn" ), wwSkinColor( "text" ), wwSkinColor( "border" ),
			wwSkinColor( "bgBtnHover" ), wwSkinColor( "accentBg" ),
			wwSkinColor( "accentText" ), wwSkinColor( "accent" ) );
}

} // namespace

QDockWidget * tlCreateSkeletonManagerDock( NifModel * nif, QMainWindow * mw, GLView * ogl )
{
	auto * skope = qobject_cast<NifSkope *>( mw );

	auto * dock = new QDockWidget( QObject::tr( "Skeleton Manager" ), mw );
	dock->setObjectName( QStringLiteral( "SkeletonManagerDock" ) );
	dock->setAllowedAreas( Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea );

	auto * panel = new QWidget( dock );
	auto * layout = new QVBoxLayout( panel );
	layout->setContentsMargins( 6, 6, 6, 6 );
	layout->setSpacing( 5 );

	auto * rootLabel = new QLabel( QObject::tr( "Open a NIF to inspect its skeleton." ), panel );
	rootLabel->setObjectName( QStringLiteral( "SkeletonRootLabel" ) );
	rootLabel->setWordWrap( true );
	layout->addWidget( rootLabel );

	auto * search = new QLineEdit( panel );
	search->setObjectName( QStringLiteral( "SkeletonSearch" ) );
	search->setClearButtonEnabled( true );
	search->setPlaceholderText( QObject::tr( "Search bones..." ) );
	layout->addWidget( search );

	// All | Bones | Deforming | Unused — the plan's filter row. Boxed buttons,
	// same family as the Panels / Workspaces selectors.
	auto * filterRow = new QHBoxLayout();
	filterRow->setSpacing( 4 );
	QList<QToolButton *> filterButtons;
	const QStringList filterNames = {
		QObject::tr( "All" ), QObject::tr( "Bones" ),
		QObject::tr( "Deforming" ), QObject::tr( "Unused" )
	};
	const QStringList filterTips = {
		QObject::tr( "Every node in the file" ),
		QObject::tr( "Nodes a skin lists as a bone" ),
		QObject::tr( "Bones that actually move vertices" ),
		QObject::tr( "Bones a skin lists but no vertex uses — prunable" )
	};
	for ( int i = 0; i < filterNames.size(); i++ ) {
		auto * b = new QToolButton( panel );
		b->setText( filterNames.at( i ) );
		b->setToolTip( filterTips.at( i ) );
		b->setCheckable( true );
		b->setChecked( i == FilterAll );
		b->setAutoRaise( false );
		b->setStyleSheet( skelBoxedButtonQss() );
		b->setObjectName( QStringLiteral( "SkeletonFilter%1" ).arg( i ) );
		filterRow->addWidget( b );
		filterButtons << b;
	}
	filterRow->addStretch( 1 );
	layout->addLayout( filterRow );

	auto * tree = new QTreeWidget( panel );
	tree->setObjectName( QStringLiteral( "SkeletonTree" ) );
	tree->setColumnCount( 4 );
	tree->setHeaderLabels( { QObject::tr( "Bone" ), QObject::tr( "Shapes" ),
		QObject::tr( "Verts" ), QObject::tr( "Weight" ) } );
	tree->setRootIsDecorated( true );
	tree->setUniformRowHeights( true );
	// Multi-select, so Select Hierarchy / Mirror / Similar and Isolate have
	// something to act on.
	tree->setSelectionMode( QAbstractItemView::ExtendedSelection );
	tree->setContextMenuPolicy( Qt::CustomContextMenu );
	tree->header()->setStretchLastSection( false );
	tree->header()->setSectionResizeMode( 0, QHeaderView::Stretch );
	for ( int c = 1; c < 4; c++ )
		tree->header()->setSectionResizeMode( c, QHeaderView::ResizeToContents );
	layout->addWidget( tree, 1 );

	// Rest pose: the same toggle edit mode already exposes, surfaced here where
	// the plan says it belongs (§A.1.3). Driven through the viewport action so
	// there stays one source of truth for it.
	auto * restPose = new QCheckBox( QObject::tr( "Rest pose" ), panel );
	restPose->setObjectName( QStringLiteral( "SkeletonRestPose" ) );
	restPose->setToolTip( QObject::tr(
		"Show the skeleton's bind pose instead of the current animated pose." ) );
	layout->addWidget( restPose );

	auto * findings = new QLabel( panel );
	findings->setObjectName( QStringLiteral( "SkeletonFindings" ) );
	findings->setWordWrap( true );
	findings->setVisible( false );
	layout->addWidget( findings );

	auto * footer = new QLabel( panel );
	footer->setObjectName( QStringLiteral( "SkeletonFooter" ) );
	footer->setStyleSheet( QStringLiteral( "color: %1; padding: 2px;" ).arg( wwSkinColor( "textMuted" ) ) );
	layout->addWidget( footer );

	dock->setWidget( panel );

	// --- population --------------------------------------------------------
	// Guard against the two-way selection loop the Pose Manager also has to
	// guard: selecting a row selects the block, which echoes back as
	// currentNifIndexChanged and would re-enter refresh().
	auto * syncing = new bool( false );
	// Isolate (Blender's `/`): when non-empty, only these bones are shown, in the
	// tree AND the viewport. Heap-allocated so the lambdas share one set.
	auto * isolated = new QSet<int>();
	// Length of a bone created by Extrude, in the parent's local +Y. A fixed
	// value rather than something derived: the characteristic bone size lives in
	// GLView and is not exposed, and a new bone is meant to be moved anyway.
	const float boneLength = 5.0f;
	panel->setProperty( "skeletonSyncing", false );

	auto currentFilter = [filterButtons]() -> int {
		for ( int i = 0; i < filterButtons.size(); i++ )
			if ( filterButtons.at( i )->isChecked() )
				return i;
		return FilterAll;
	};

	auto refresh = [=]() mutable {
		tree->clear();
		findings->setVisible( false );

		NifModel * model = skope ? skope->getNifModel() : nif;
		if ( !model || model->getBlockCount() < 1 ) {
			rootLabel->setText( QObject::tr( "Open a NIF to inspect its skeleton." ) );
			footer->clear();
			return;
		}

		const SkeletonReport report = skeletonAnalyse( model );
		if ( report.bones.isEmpty() ) {
			rootLabel->setText( QObject::tr( "This file has no scene nodes." ) );
			footer->clear();
			return;
		}

		if ( report.rootBlock >= 0 ) {
			const QString rootName = model->get<QString>( model->getBlockIndex( report.rootBlock ), "Name" );
			rootLabel->setText( QObject::tr( "Skeleton root   %1  (block %2)" )
				.arg( rootName.isEmpty() ? QObject::tr( "<unnamed>" ) : rootName )
				.arg( report.rootBlock ) );
		} else {
			rootLabel->setText( QObject::tr( "No root node found." ) );
		}

		const int filter = currentFilter();
		const QString needle = search->text().trimmed();

		// Coloured text, not badges — the Block Details visual rule.
		const QColor colText = QColor::fromString( wwSkinColor( "text" ) );
		const QColor colMuted = QColor::fromString( wwSkinColor( "textMuted" ) );
		// An unused bone wants an attention colour. This was `textBright`, which
		// reads as amber from its name but is actually #f2f3f5 — near-white, i.e.
		// indistinguishable from normal text. `accent` is the palette's orange.
		const QColor colBright = QColor::fromString( wwSkinColor( "accent" ) );

		QHash<int, QTreeWidgetItem *> itemOf;
		int shown = 0;
		for ( const SkeletonBoneInfo & b : report.bones ) {
			switch ( filter ) {
			case FilterBones:      if ( !b.inSkin ) continue; break;
			case FilterDeforming:  if ( b.verts < 1 ) continue; break;
			case FilterUnused:     if ( !b.isUnusedBone() ) continue; break;
			default: break;
			}
			if ( !needle.isEmpty() && !b.name.contains( needle, Qt::CaseInsensitive ) )
				continue;
			if ( !isolated->isEmpty() && !isolated->contains( b.block ) )
				continue;

			// Parent the row when its parent is also shown; otherwise promote it
			// to the top level so a filtered view never hides matches behind a
			// filtered-out ancestor (the Block List type-chip rule).
			QTreeWidgetItem * parentItem = itemOf.value( b.parent, nullptr );
			auto * item = parentItem ? new QTreeWidgetItem( parentItem ) : new QTreeWidgetItem( tree );
			itemOf.insert( b.block, item );
			shown++;

			item->setText( 0, b.name.isEmpty()
				? QObject::tr( "<unnamed> [%1]" ).arg( b.block ) : b.name );
			item->setText( 1, b.shapes > 0 ? QString::number( b.shapes ) : QString() );
			item->setText( 2, b.verts > 0 ? QString::number( b.verts ) : QString() );
			item->setText( 3, b.verts > 0 ? QString::number( b.weight, 'f', 2 ) : QString() );
			item->setData( 0, Qt::UserRole, b.block );
			item->setTextAlignment( 1, Qt::AlignRight | Qt::AlignVCenter );
			item->setTextAlignment( 2, Qt::AlignRight | Qt::AlignVCenter );
			item->setTextAlignment( 3, Qt::AlignRight | Qt::AlignVCenter );

			QColor fg = colText;
			QString tip = QObject::tr( "block %1" ).arg( b.block );
			if ( b.isNotABone() ) {
				fg = colMuted;
				tip = QObject::tr( "block %1 — not referenced by any skin" ).arg( b.block );
			} else if ( b.isUnusedBone() ) {
				fg = colBright;
				tip = QObject::tr( "block %1 — listed as a bone by %2 shape(s) but no vertex is weighted to it" )
					.arg( b.block ).arg( b.shapes );
			}
			for ( int c = 0; c < 4; c++ ) {
				item->setForeground( c, fg );
				item->setToolTip( c, tip );
			}
		}
		tree->expandAll();

		QStringList problems;
		for ( const QString & d : report.danglingSkinBones )
			problems << d;
		for ( const QString & n : report.duplicateNames )
			problems << QObject::tr( "duplicate node name '%1'" ).arg( n );
		if ( !problems.isEmpty() ) {
			findings->setStyleSheet( QStringLiteral( "color: %1;" ).arg( wwSkinColor( "danger" ) ) );
			findings->setText( problems.join( QStringLiteral( "\n" ) ) );
			findings->setVisible( true );
		}

		if ( !isolated->isEmpty() )
			rootLabel->setText( rootLabel->text()
				+ QObject::tr( "   —  ISOLATED (%1 bone(s))" ).arg( isolated->size() ) );
		footer->setText( QObject::tr( "%1 node(s) shown · %2 bone(s), %3 deforming, %4 unused · %5 skinned shape(s)" )
			.arg( shown )
			.arg( report.deformingCount() + report.unusedCount() )
			.arg( report.deformingCount() )
			.arg( report.unusedCount() )
			.arg( report.skinnedShapes ) );
	};

	// ---- selection colouring -------------------------------------------
	// The Block List convention, which the Pose Manager also follows: the ACTIVE
	// row is orange text, other selected rows are reddish-orange on the theme's
	// blue highlight. Colours come from skinVars, and the blue is the palette's
	// own Highlight rather than a new literal.
	auto paintSelection = [=]() {
		const QColor active = QColor::fromString( wwSkinColor( "accentText" ) );
		const QColor secondary = QColor::fromString( wwSkinColor( "danger" ) );
		const QColor blue = tree->palette().color( QPalette::Highlight );
		QTreeWidgetItem * cur = tree->currentItem();
		std::function<void( QTreeWidgetItem * )> walk = [&]( QTreeWidgetItem * it ) {
			const bool sel = it->isSelected();
			for ( int c = 0; c < 4; c++ ) {
				if ( !sel ) {
					it->setBackground( c, QBrush() );
				} else {
					it->setBackground( c, blue );
					it->setForeground( c, it == cur ? active : secondary );
				}
			}
			for ( int i = 0; i < it->childCount(); i++ )
				walk( it->child( i ) );
		};
		for ( int i = 0; i < tree->topLevelItemCount(); i++ )
			walk( tree->topLevelItem( i ) );
	};

	// ---- helpers used by the menu --------------------------------------
	auto selectedBlocks = [=]() {
		QList<int> out;
		for ( QTreeWidgetItem * it : tree->selectedItems() ) {
			const int b = it->data( 0, Qt::UserRole ).toInt();
			if ( b >= 0 && !out.contains( b ) )
				out << b;
		}
		return out;
	};
	auto itemForBlock = [=]( int block ) -> QTreeWidgetItem * {
		std::function<QTreeWidgetItem *( QTreeWidgetItem * )> walk =
			[&]( QTreeWidgetItem * it ) -> QTreeWidgetItem * {
			if ( it->data( 0, Qt::UserRole ).toInt() == block )
				return it;
			for ( int i = 0; i < it->childCount(); i++ )
				if ( QTreeWidgetItem * hit = walk( it->child( i ) ) )
					return hit;
			return nullptr;
		};
		for ( int i = 0; i < tree->topLevelItemCount(); i++ )
			if ( QTreeWidgetItem * hit = walk( tree->topLevelItem( i ) ) )
				return hit;
		return nullptr;
	};
	auto selectBlocks = [=]( const QList<int> & blocks, bool additive ) {
		if ( !additive )
			tree->clearSelection();
		for ( int b : blocks ) {
			if ( QTreeWidgetItem * it = itemForBlock( b ) ) {
				it->setSelected( true );
				QTreeWidgetItem * p = it->parent();
				while ( p ) { p->setExpanded( true ); p = p->parent(); }
			}
		}
	};

	QObject::connect( tree, &QTreeWidget::customContextMenuRequested, panel,
		[=]( const QPoint & pos ) mutable {
		NifModel * model = skope ? skope->getNifModel() : nif;
		if ( !model )
			return;
		const QList<int> sel = selectedBlocks();
		QTreeWidgetItem * under = tree->itemAt( pos );
		const int primary = under ? under->data( 0, Qt::UserRole ).toInt()
			: ( sel.isEmpty() ? -1 : sel.first() );
		if ( primary < 0 )
			return;
		const QString primaryName = model->get<QString>( model->getBlockIndex( primary ), "Name" );

		QMenu menu;

		// --- selection (read-only, always safe) ---
		menu.addSection( QObject::tr( "Select" ) );
		QAction * aHier = menu.addAction( QObject::tr( "Select Hierarchy" ) );
		aHier->setToolTip( QObject::tr( "Add this bone's whole subtree to the selection" ) );
		QAction * aMirror = menu.addAction( QObject::tr( "Select Mirror" ) );
		QAction * aSimilar = menu.addAction( QObject::tr( "Select Similar" ) );
		aSimilar->setToolTip( QObject::tr( "Bones in the same family — same name with different trailing digits" ) );
		menu.addSeparator();
		QAction * aIsolate = menu.addAction( isolated->isEmpty()
			? QObject::tr( "Isolate Selected" ) : QObject::tr( "Clear Isolate" ) );
		aIsolate->setToolTip( QObject::tr( "Show only the selected bones, in the tree and the viewport" ) );

		// --- edits ---
		menu.addSection( QObject::tr( "Edit" ) );
		QAction * aRename = menu.addAction( QObject::tr( "Rename..." ) );
		QAction * aFlip = menu.addAction( QObject::tr( "Flip Name L/R" ) );
		const QString flipped = skeletonFlipBoneName( primaryName );
		aFlip->setEnabled( flipped != primaryName );
		aFlip->setToolTip( flipped != primaryName
			? QObject::tr( "Rename to %1" ).arg( flipped )
			: QObject::tr( "This bone has no L/R side in its name" ) );
		QAction * aAdd = menu.addAction( QObject::tr( "Add Child Bone (Extrude)" ) );
		QAction * aBatch = menu.addAction( QObject::tr( "Batch Rename..." ) );
		aBatch->setToolTip( QObject::tr( "Find and replace across every selected bone" ) );
		aBatch->setEnabled( !sel.isEmpty() );
		menu.addSeparator();
		QAction * aDup = menu.addAction( QObject::tr( "Duplicate Mirrored" ) );
		aDup->setEnabled( flipped != primaryName );
		QAction * aSym = menu.addAction( QObject::tr( "Symmetrize (overwrite other side)" ) );
		aSym->setEnabled( flipped != primaryName );
		QAction * aSymTree = menu.addAction( QObject::tr( "Symmetrize Subtree" ) );
		aSymTree->setEnabled( flipped != primaryName );
		menu.addSeparator();
		QAction * aParentKeep = menu.addAction( QObject::tr( "Parent to Active — Keep Transform" ) );
		QAction * aParentOffset = menu.addAction( QObject::tr( "Parent to Active — Keep Offset" ) );
		const bool canParent = ( sel.size() >= 2 );
		aParentKeep->setEnabled( canParent );
		aParentOffset->setEnabled( canParent );
		if ( !canParent ) {
			const QString why = QObject::tr( "Select the bones to move, then Ctrl-click the new parent last" );
			aParentKeep->setToolTip( why );
			aParentOffset->setToolTip( why );
		}
		QAction * aClearParent = menu.addAction( QObject::tr( "Clear Parent" ) );
		menu.addSeparator();
		QAction * aDissolve = menu.addAction( QObject::tr( "Dissolve" ) );
		aDissolve->setToolTip( QObject::tr( "Remove this bone; its children are adopted by its parent and do not move" ) );
		QAction * aDelete = menu.addAction( QObject::tr( "Delete Bone and Children" ) );

		QAction * chosen = menu.exec( tree->viewport()->mapToGlobal( pos ) );
		if ( !chosen )
			return;

		// ---- selection actions -----------------------------------------
		if ( chosen == aHier ) {
			QList<int> add;
			QList<int> stack = sel.isEmpty() ? QList<int>{ primary } : sel;
			while ( !stack.isEmpty() ) {
				const int b = stack.takeLast();
				if ( add.contains( b ) )
					continue;
				add << b;
				for ( int c : model->getChildLinks( b ) )
					if ( c >= 0 )
						stack << c;
			}
			selectBlocks( add, true );
			paintSelection();
			return;
		}
		if ( chosen == aMirror ) {
			QList<int> add;
			for ( int b : ( sel.isEmpty() ? QList<int>{ primary } : sel ) ) {
				const QString flip = skeletonFlipBoneName(
					model->get<QString>( model->getBlockIndex( b ), "Name" ) );
				if ( flip.isEmpty() )
					continue;
				for ( int o = 0; o < model->getBlockCount(); o++ ) {
					if ( model->get<QString>( model->getBlockIndex( o ), "Name" ) == flip )
						add << o;
				}
			}
			selectBlocks( add, true );
			paintSelection();
			return;
		}
		if ( chosen == aSimilar ) {
			QList<int> add;
			for ( int b : ( sel.isEmpty() ? QList<int>{ primary } : sel ) ) {
				const QStringList fam = skeletonSimilarNames( model,
					model->get<QString>( model->getBlockIndex( b ), "Name" ) );
				for ( int o = 0; o < model->getBlockCount(); o++ ) {
					if ( fam.contains( model->get<QString>( model->getBlockIndex( o ), "Name" ) ) )
						add << o;
				}
			}
			selectBlocks( add, true );
			paintSelection();
			return;
		}
		if ( chosen == aIsolate ) {
			if ( isolated->isEmpty() ) {
				for ( int b : ( sel.isEmpty() ? QList<int>{ primary } : sel ) )
					isolated->insert( b );
			} else {
				isolated->clear();
			}
			if ( ogl )
				ogl->setSkeletonIsolated( *isolated );
			refresh();
			return;
		}

		// ---- edits -----------------------------------------------------
		// Every structural change to a file with a ragdoll leaves that ragdoll
		// stale: it is a compiled Havok packfile binding bodies to nodes, and
		// there is no encoder for it. Say so plainly rather than silently
		// producing a rig whose physics no longer matches its skeleton.
		auto confirmRagdoll = [&]( const QString & what ) {
			if ( !skeletonFileHasRagdoll( model ) )
				return true;
			return QMessageBox::warning( panel, QObject::tr( "Ragdoll will be stale" ),
				QObject::tr( "%1\n\nThis file has a bhkRagdollSystem — a compiled Havok "
					"packfile that binds physics bodies to bone nodes. NifSkope can read it "
					"but cannot rebuild it, so after this change the ragdoll will no longer "
					"match the skeleton.\n\nProceed anyway?" ).arg( what ),
				QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel ) == QMessageBox::Yes;
		};

		QString err;
		if ( chosen == aRename || chosen == aFlip ) {
			QString newName = flipped;
			if ( chosen == aRename ) {
				bool ok = false;
				newName = QInputDialog::getText( panel, QObject::tr( "Rename bone" ),
					QObject::tr( "New name for '%1':" ).arg( primaryName ),
					QLineEdit::Normal, primaryName, &ok );
				if ( !ok || newName.isEmpty() || newName == primaryName )
					return;
			}
			// Renaming only rewrites by-NAME references; the ragdoll binds by node,
			// so a rename does not invalidate it. No warning needed here.
			nifSnapshotOp( model, QObject::tr( "Rename bone" ), [&]() {
				skeletonRenameBone( model, primary, newName, &err );
			} );
			if ( !err.isEmpty() )
				QMessageBox::warning( panel, QObject::tr( "Rename failed" ), err );
			refresh();
			return;
		}
		if ( chosen == aAdd ) {
			bool ok = false;
			const QString name = QInputDialog::getText( panel, QObject::tr( "Add child bone" ),
				QObject::tr( "Name for the new bone under '%1':" ).arg( primaryName ),
				QLineEdit::Normal, primaryName + QStringLiteral( "_new" ), &ok );
			if ( !ok || name.isEmpty() )
				return;
			nifSnapshotOp( model, QObject::tr( "Add child bone" ), [&]() {
				skeletonAddChildBone( model, primary, name, boneLength );
			} );
			refresh();
			return;
		}
		if ( chosen == aBatch ) {
			bool ok = false;
			const QString spec = QInputDialog::getText( panel, QObject::tr( "Batch rename" ),
				QObject::tr( "Replace text across %1 selected bone(s).\nEnter  find=replace" )
					.arg( sel.size() ),
				QLineEdit::Normal, QString(), &ok );
			if ( !ok || !spec.contains( QLatin1Char( '=' ) ) )
				return;
			const QString findText = spec.section( QLatin1Char( '=' ), 0, 0 );
			const QString replText = spec.section( QLatin1Char( '=' ), 1 );
			if ( findText.isEmpty() )
				return;
			int changed = 0;
			nifSnapshotOp( model, QObject::tr( "Batch rename bones" ), [&]() {
				for ( int b : sel ) {
					const QString old = model->get<QString>( model->getBlockIndex( b ), "Name" );
					if ( !old.contains( findText ) )
						continue;
					QString nn = old;
					nn.replace( findText, replText );
					if ( nn != old && skeletonRenameBone( model, b, nn, &err ) )
						changed++;
				}
			} );
			refresh();
			if ( changed < 1 )
				QMessageBox::information( panel, QObject::tr( "Batch rename" ),
					QObject::tr( "No selected bone contained '%1'." ).arg( findText ) );
			return;
		}
		if ( chosen == aDup || chosen == aSym || chosen == aSymTree ) {
			const bool overwrite = ( chosen != aDup );
			const bool subtree = ( chosen == aSymTree );
			nifSnapshotOp( model, chosen == aDup ? QObject::tr( "Duplicate mirrored" )
												 : QObject::tr( "Symmetrize" ), [&]() {
				skeletonMirrorBone( model, primary, subtree, overwrite, &err );
			} );
			if ( !err.isEmpty() )
				QMessageBox::warning( panel, QObject::tr( "Mirror failed" ), err );
			refresh();
			return;
		}
		if ( chosen == aParentKeep || chosen == aParentOffset ) {
			// Blender's convention: the ACTIVE bone (Ctrl-clicked last) is the new
			// parent, everything else selected becomes its child.
			const int newParent = sel.last();
			if ( !confirmRagdoll( QObject::tr( "Reparenting %1 bone(s)." ).arg( sel.size() - 1 ) ) )
				return;
			nifSnapshotOp( model, QObject::tr( "Parent bones" ), [&]() {
				for ( int b : sel ) {
					if ( b == newParent )
						continue;
					QString e;
					if ( !skeletonReparent( model, b, newParent, chosen == aParentKeep, &e ) && err.isEmpty() )
						err = e;
				}
			} );
			if ( !err.isEmpty() )
				QMessageBox::warning( panel, QObject::tr( "Parent failed" ), err );
			refresh();
			return;
		}
		if ( chosen == aClearParent ) {
			if ( !confirmRagdoll( QObject::tr( "Unparenting %1." ).arg( primaryName ) ) )
				return;
			nifSnapshotOp( model, QObject::tr( "Clear parent" ), [&]() {
				skeletonReparent( model, primary, -1, true, &err );
			} );
			if ( !err.isEmpty() )
				QMessageBox::warning( panel, QObject::tr( "Clear parent failed" ), err );
			refresh();
			return;
		}
		if ( chosen == aDissolve || chosen == aDelete ) {
			const bool isDelete = ( chosen == aDelete );
			const QString what = isDelete
				? QObject::tr( "Deleting '%1' and everything under it." ).arg( primaryName )
				: QObject::tr( "Dissolving '%1'." ).arg( primaryName );
			// A bone carrying skin weights cannot be removed without remapping
			// every vertex's bone indices, which is phase 2 work. Refuse instead
			// of corrupting the mesh.
			const SkeletonReport rep = skeletonAnalyse( model );
			for ( const SkeletonBoneInfo & bi : rep.bones ) {
				if ( bi.block == primary && bi.verts > 0 ) {
					QMessageBox::warning( panel, QObject::tr( "Bone is in use" ),
						QObject::tr( "'%1' has %2 vertices weighted to it. Removing it would need "
							"every vertex's bone indices remapped, which is not implemented yet — "
							"so this is refused rather than corrupting the mesh." )
							.arg( primaryName ).arg( bi.verts ) );
					return;
				}
			}
			if ( QMessageBox::warning( panel, QObject::tr( "Remove bone" ), what
					+ QObject::tr( "\n\nThis cannot be undone beyond one undo step. Continue?" ),
					QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel ) != QMessageBox::Yes )
				return;
			if ( !confirmRagdoll( what ) )
				return;
			nifSnapshotOp( model, isDelete ? QObject::tr( "Delete bone" ) : QObject::tr( "Dissolve bone" ),
				[&]() {
					if ( isDelete )
						skeletonDeleteSubtree( model, primary, &err );
					else
						skeletonDissolve( model, primary, &err );
				} );
			if ( !err.isEmpty() )
				QMessageBox::warning( panel,
					isDelete ? QObject::tr( "Delete failed" ) : QObject::tr( "Dissolve failed" ), err );
			refresh();
			return;
		}
	} );

	// selecting a row selects the block, like every other manager
	QObject::connect( tree, &QTreeWidget::itemSelectionChanged, panel, [=]() {
		if ( *syncing || !skope )
			return;
		const QList<QTreeWidgetItem *> sel = tree->selectedItems();
		if ( sel.isEmpty() )
			return;
		NifModel * model = skope->getNifModel();
		const int block = sel.first()->data( 0, Qt::UserRole ).toInt();
		if ( !model || block < 0 )
			return;
		*syncing = true;
		skope->select( model->getBlockIndex( block ) );
		*syncing = false;
		paintSelection();
	} );

	for ( QToolButton * b : filterButtons ) {
		QObject::connect( b, &QToolButton::clicked, panel, [=]() mutable {
			for ( QToolButton * other : filterButtons )
				other->setChecked( other == b );
			refresh();
		} );
	}
	QObject::connect( search, &QLineEdit::textChanged, panel, [=]( const QString & ) mutable { refresh(); } );

	// Rest pose. There is no standalone action to defer to: `restPoseBlock` is
	// currently set only as an edit-mode side effect (glview.cpp:15493/15517),
	// which is precisely what §A.1.3 asks to surface here. So this drives the
	// scene field directly.
	//
	// It is per-shape, not global — restWorldTrans() derives from one skinned
	// shape's bind data — so the dock has to choose one. Rules: the selected
	// shape if it is skinned, else the file's only skinned shape. With several
	// and none selected it refuses rather than silently picking, because "rest
	// pose of which mesh?" has a different answer per mesh.
	QObject::connect( restPose, &QCheckBox::toggled, panel, [=]( bool on ) {
		if ( !ogl )
			return;
		Scene * sc = ogl->getScene();
		NifModel * model = skope ? skope->getNifModel() : nif;
		if ( !sc || !model )
			return;

		int target = -1;
		if ( on ) {
			QList<int> skinned;
			for ( int b = 0; b < model->getBlockCount(); b++ ) {
				QModelIndex idx = model->getBlockIndex( b );
				if ( skelIsTriShape( model, idx ) && skelSkinInstance( model, idx ).isValid() )
					skinned << b;
			}
			const QList<QTreeWidgetItem *> sel = tree->selectedItems();
			const int selBlock = sel.isEmpty() ? -1 : sel.first()->data( 0, Qt::UserRole ).toInt();
			if ( skinned.contains( selBlock ) )
				target = selBlock;
			else if ( skinned.size() == 1 )
				target = skinned.first();

			if ( target < 0 ) {
				restPose->setToolTip( skinned.isEmpty()
					? QObject::tr( "This file has no skinned mesh, so it has no rest pose." )
					: QObject::tr( "Several skinned meshes — select one first; rest pose is per-mesh." ) );
				QSignalBlocker block( restPose );
				restPose->setChecked( false );
				return;
			}
		}

		sc->restPoseBlock = target;
		sc->transformDirty = true;		// rest-pose switch changes worldTrans derivation
		ogl->update();
	} );

	if ( skope ) {
		QObject::connect( skope, &NifSkope::currentNifIndexChanged, panel,
			[=]( const QModelIndex & index ) mutable {
				if ( *syncing )
					return;
				// Selecting a collision body in the Collision Manager selects its
				// bhkNPCollisionObject, which is not a bone and would leave this
				// tree pointing at nothing. Follow it to the node it targets - the
				// bone whose collision it is - so the two docks track each other.
				NifModel * model = skope->getNifModel();
				if ( model ) {
					const QModelIndex iBlock = model->getBlockIndex( index );
					if ( model->blockInherits( iBlock, "bhkNiCollisionObject" )
						|| model->blockInherits( iBlock, "bhkNPCollisionObject" ) ) {
						const int target = model->getLink( iBlock, "Target" );
						if ( model->isValidBlockNumber( target ) ) {
							*syncing = true;
							skope->select( model->getBlockIndex( target ) );
							*syncing = false;
						}
					}
				}
				refresh();
			} );
	}

	// Ctrl+P — Blender's Set Parent. Opens the same two-way choice Blender does
	// rather than picking one silently, because Keep Transform vs Keep Offset is
	// exactly where a careless reparent deforms a rig. Scoped to the tree widget so
	// it cannot shadow anything global.
	auto * parentShortcut = new QShortcut( QKeySequence( Qt::CTRL | Qt::Key_P ), tree );
	parentShortcut->setContext( Qt::WidgetWithChildrenShortcut );
	QObject::connect( parentShortcut, &QShortcut::activated, panel, [=]() mutable {
		NifModel * model = skope ? skope->getNifModel() : nif;
		if ( !model )
			return;
		const QList<int> sel = selectedBlocks();
		if ( sel.size() < 2 ) {
			QMessageBox::information( panel, QObject::tr( "Set Parent" ),
				QObject::tr( "Select the bones to move, then Ctrl-click the new parent last." ) );
			return;
		}
		const int newParent = sel.last();

		QMenu m;
		m.addSection( QObject::tr( "Set Parent to %1" )
			.arg( model->get<QString>( model->getBlockIndex( newParent ), "Name" ) ) );
		QAction * keep = m.addAction( QObject::tr( "Keep Transform" ) );
		QAction * offset = m.addAction( QObject::tr( "Keep Offset" ) );
		QAction * chosen = m.exec( QCursor::pos() );
		if ( chosen != keep && chosen != offset )
			return;

		if ( skeletonFileHasRagdoll( model )
			&& QMessageBox::warning( panel, QObject::tr( "Ragdoll will be stale" ),
				QObject::tr( "Reparenting %1 bone(s).\n\nThis file has a bhkRagdollSystem, which "
					"NifSkope can read but not rebuild, so its physics will no longer match the "
					"skeleton.\n\nProceed anyway?" ).arg( sel.size() - 1 ),
				QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel ) != QMessageBox::Yes )
			return;

		QString err;
		nifSnapshotOp( model, QObject::tr( "Parent bones" ), [&]() {
			for ( int b : sel ) {
				if ( b == newParent )
					continue;
				QString e;
				if ( !skeletonReparent( model, b, newParent, chosen == keep, &e ) && err.isEmpty() )
					err = e;
			}
		} );
		if ( !err.isEmpty() )
			QMessageBox::warning( panel, QObject::tr( "Parent failed" ), err );
		refresh();
	} );

	// The armature draws while this dock is up and stops when it is put away, so
	// the viewport is never left with bones over a mesh the user moved on from.
	QObject::connect( dock, &QDockWidget::visibilityChanged, panel, [=]( bool visible ) mutable {
		if ( ogl )
			ogl->setSkeletonView( visible );
		if ( visible )
			refresh();
	} );

	refresh();

	// Register with a dock area and start hidden, exactly as the UV Editor dock
	// does. Without this the dock shows at startup and steals width from the GL
	// viewport — which changed the framebuffer size and turned all seven
	// render-regression baselines into "size mismatch" the first time round.
	// activateWorkspace() shows it when the Skeleton workspace is chosen.
	mw->addDockWidget( Qt::RightDockWidgetArea, dock );
	dock->hide();
	return dock;
}
