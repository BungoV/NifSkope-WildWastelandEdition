/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#include "nifskope.h"
#include "glview.h"
#include "nifmerge.h"
#include "model/nifmodel.h"
#include "spells/animationsetup.h"
#include "ui/settingsdialog.h"
#include "wwskin.h"

#include <QAction>
#include <QCheckBox>
#include <QComboBox>
#include <QDockWidget>
#include <QFileDialog>
#include <QMenu>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QSlider>
#include <QTimer>
#include <QVBoxLayout>
#include <QDir>
#include <QRegularExpression>
#include <QSettings>
#include <QStandardPaths>

/*! \file posetools.cpp
 * \brief Pose Manager workspace dock.
 *
 * Blender's Pose Mode + Pose Library, in NifSkope's flat visual language. The
 * posing ENGINE already exists — Shape::updateBoneTransforms reads the live
 * bone node transform, so selecting a bone and pressing G/R/S deforms the mesh
 * — so this dock is two things over the shared AnimSetup pose API:
 *
 *   - a **bone list** that drives block-list / viewport selection, so a bone is
 *     one click away from G/R/S (this is the practical form of "bone picking";
 *     clickable octahedral bones in the 3D view are Skeleton Manager work),
 *   - a **pose library**: save the current pose, apply one with a blend slider,
 *     delete. Poses are ordinary one-key NiControllerSequences, so they also
 *     show up in the Timeline.
 */

namespace
{

//! Section-heading label, matching the other manager docks.
QLabel * heading( const QString & text, QWidget * parent )
{
	auto * l = new QLabel( text, parent );
	l->setStyleSheet( QStringLiteral( "QLabel { font-weight: 600; }" ) );
	return l;
}

//! Settings key for the NifSkope library root. Shared with the General →
//! NifSkope Library settings pane (its "libraryFolder" line edit humanizes to
//! this exact key), so the dock's Folder... button and the settings dialog stay
//! in sync.
static const char * const kLibraryKey = "Settings/Library/Library Folder";

//! The NifSkope library root — a single folder where NifSkope keeps user files
//! (the pose library today, other things later, each in its own subfolder).
//! Defaults to a "NifSkope" folder under the user's Documents, overridable via
//! QSettings, and created on use.
QString nifskopeLibraryFolder()
{
	QSettings settings;
	QString dir = settings.value( QLatin1String( kLibraryKey ) ).toString();
	if ( dir.isEmpty() ) {
		const QString docs = QStandardPaths::writableLocation( QStandardPaths::DocumentsLocation );
		dir = QDir( docs.isEmpty() ? QDir::homePath() : docs ).filePath( QStringLiteral( "NifSkope" ) );
	}
	QDir().mkpath( dir );
	return dir;
}

void setNifskopeLibraryFolder( const QString & dir )
{
	QSettings settings;
	settings.setValue( QLatin1String( kLibraryKey ), dir );
}

//! Where pose files live: a "Poses" subfolder of the library root, so other
//! features can claim their own subfolders without colliding.
QString poseLibraryFolder()
{
	const QString dir = QDir( nifskopeLibraryFolder() ).filePath( QStringLiteral( "Poses" ) );
	QDir().mkpath( dir );
	return dir;
}

} // namespace

QDockWidget * tlCreatePoseManagerDock( NifModel * nif, QMainWindow * mw, GLView * ogl )
{
	auto * skope = qobject_cast<NifSkope *>( mw );

	auto * dock = new QDockWidget( QObject::tr( "Pose Manager" ), mw );
	dock->setObjectName( QStringLiteral( "PoseManagerDock" ) );
	dock->setAllowedAreas( Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea );

	auto * panel = new QWidget( dock );
	auto * layout = new QVBoxLayout( panel );
	layout->setContentsMargins( 6, 6, 6, 6 );
	layout->setSpacing( 5 );

	// search bar at the very top (Blender's bone search)
	auto * boneSearch = new QLineEdit( panel );
	boneSearch->setObjectName( QStringLiteral( "PoseBoneSearch" ) );
	boneSearch->setClearButtonEnabled( true );
	boneSearch->setPlaceholderText( QObject::tr( "Search bones..." ) );
	layout->addWidget( boneSearch );

	auto * status = new QLabel( QObject::tr( "Open a skinned mesh to pose its skeleton." ), panel );
	status->setWordWrap( true );
	layout->addWidget( status );

	// bring in a skeleton (or another armour piece) from a file OR a game archive
	// (the archive path reuses the existing NIF Browser)
	auto * loadSkelBtn = new QPushButton( QObject::tr( "Load skeleton..." ), panel );
	loadSkelBtn->setObjectName( QStringLiteral( "PoseLoadSkeletonButton" ) );
	loadSkelBtn->setToolTip( QObject::tr(
		"Merge a skeleton (or armour piece) from another NIF — from a file or a "
		"game archive. Bones with matching names are shared, so the merged pieces "
		"pose as one skeleton." ) );
	auto * loadSkelMenu = new QMenu( loadSkelBtn );
	QAction * loadFromFileAct = loadSkelMenu->addAction( QObject::tr( "From file..." ) );
	QAction * loadFromArchiveAct = loadSkelMenu->addAction( QObject::tr( "From game archive..." ) );
	loadFromArchiveAct->setObjectName( QStringLiteral( "PoseLoadSkeletonArchiveAction" ) );
	loadSkelBtn->setMenu( loadSkelMenu );
	layout->addWidget( loadSkelBtn );

	// ---- bones ------------------------------------------------------------
	layout->addWidget( heading( QObject::tr( "Bones" ), panel ) );

	// display: which bones, names, relationship lines, weight overlay
	auto * dispRow = new QHBoxLayout;
	auto * filterCombo = new QComboBox( panel );
	filterCombo->setObjectName( QStringLiteral( "PoseBoneFilter" ) );
	filterCombo->addItems( { QObject::tr( "All bones" ), QObject::tr( "Deforming" ),
		QObject::tr( "Face sculpt" ) } );
	filterCombo->setToolTip( QObject::tr( "Which bones are drawn and pickable" ) );
	auto * namesChk = new QCheckBox( QObject::tr( "Names" ), panel );
	namesChk->setToolTip( QObject::tr( "Show bone names in the viewport" ) );
	auto * relChk = new QCheckBox( QObject::tr( "Links" ), panel );
	relChk->setObjectName( QStringLiteral( "PoseRelationsToggle" ) );
	relChk->setChecked( true );
	relChk->setToolTip( QObject::tr( "Show dashed parent-relationship lines" ) );
	auto * weightsChk = new QCheckBox( QObject::tr( "Weights" ), panel );
	weightsChk->setObjectName( QStringLiteral( "PoseWeightsToggle" ) );
	weightsChk->setToolTip( QObject::tr( "Highlight the vertices the hovered / selected bone affects (heat by weight)" ) );
	dispRow->addWidget( filterCombo, 1 );
	dispRow->addWidget( namesChk );
	dispRow->addWidget( relChk );
	dispRow->addWidget( weightsChk );
	layout->addLayout( dispRow );

	auto * boneList = new QListWidget( panel );
	boneList->setObjectName( QStringLiteral( "PoseBoneList" ) );
	boneList->setToolTip( QObject::tr( "Click a bone to select it, then G / R / S in the viewport to pose it.\n"
		"Ctrl / Shift-click selects several bones to transform together." ) );
	boneList->setSelectionMode( QAbstractItemView::ExtendedSelection );
	boneList->setMinimumHeight( 140 );
	layout->addWidget( boneList, 1 );

	// reset the selected bone (or all) to the pose captured on entering Pose Mode
	auto * resetRow = new QHBoxLayout;
	auto * resetBtn = new QPushButton( QObject::tr( "Reset bone" ), panel );
	resetBtn->setObjectName( QStringLiteral( "PoseResetBoneButton" ) );
	resetBtn->setToolTip( QObject::tr( "Restore the selected bone to its rest transform" ) );
	auto * resetAllBtn = new QPushButton( QObject::tr( "Reset all" ), panel );
	resetAllBtn->setToolTip( QObject::tr( "Restore every bone to the rest pose" ) );
	auto * resetWhat = new QComboBox( panel );
	resetWhat->setObjectName( QStringLiteral( "PoseResetChannel" ) );
	resetWhat->addItems( { QObject::tr( "All" ), QObject::tr( "Rotation" ),
		QObject::tr( "Location" ), QObject::tr( "Scale" ) } );
	resetWhat->setToolTip( QObject::tr( "Which transform channels a reset clears" ) );
	resetRow->addWidget( resetBtn );
	resetRow->addWidget( resetAllBtn );
	resetRow->addWidget( resetWhat );
	layout->addLayout( resetRow );

	auto * mirrorBtn = new QPushButton( QObject::tr( "Mirror to other side (X)" ), panel );
	mirrorBtn->setObjectName( QStringLiteral( "PoseMirrorButton" ) );
	mirrorBtn->setToolTip( QObject::tr( "Copy the selected bone's pose to its L/R counterpart, mirrored across X" ) );
	layout->addWidget( mirrorBtn );

	// pin: lock the selected bone(s) so other transforms / mirror / proportional
	// editing can't move them
	auto * pinRow = new QHBoxLayout;
	auto * pinBtn = new QPushButton( QObject::tr( "Pin / Unpin" ), panel );
	pinBtn->setObjectName( QStringLiteral( "PosePinButton" ) );
	pinBtn->setToolTip( QObject::tr( "Lock the selected bone(s) so they don't move or get affected by other transforms" ) );
	auto * unpinAllBtn = new QPushButton( QObject::tr( "Unpin all" ), panel );
	pinRow->addWidget( pinBtn );
	pinRow->addWidget( unpinAllBtn );
	layout->addLayout( pinRow );

	// non-destructive: the real bone nodes are restored on exit; the pose lives
	// in a file / the library. "Bake" commits it into the bones.
	auto * ndRow = new QHBoxLayout;
	auto * ndChk = new QCheckBox( QObject::tr( "Keep original bones (non-destructive)" ), panel );
	ndChk->setObjectName( QStringLiteral( "PoseNonDestructive" ) );
	ndChk->setChecked( true );
	ndChk->setToolTip( QObject::tr( "Leaving Pose Mode restores the real bone transforms; the pose persists only in files / the library. Turn off, or Bake, to keep it in the bones." ) );
	auto * bakeBtn = new QPushButton( QObject::tr( "Bake to bones" ), panel );
	bakeBtn->setToolTip( QObject::tr( "Commit the current pose into the bone transforms (exiting won't restore them)" ) );
	ndRow->addWidget( ndChk, 1 );
	ndRow->addWidget( bakeBtn );
	layout->addLayout( ndRow );

	// ---- pose library (folder of Outfit Studio .xml pose files) -----------
	auto * libGroup = new QGroupBox( QObject::tr( "Pose library" ), panel );
	auto * libLayout = new QVBoxLayout( libGroup );
	libLayout->setContentsMargins( 6, 8, 6, 6 );
	libLayout->setSpacing( 5 );

	// folder row: the NifSkope library root (poses live in its Poses subfolder)
	auto * folderRow = new QHBoxLayout;
	auto * folderLabel = new QLabel( libGroup );
	folderLabel->setObjectName( QStringLiteral( "PoseLibraryFolderLabel" ) );
	folderLabel->setStyleSheet( QStringLiteral( "QLabel { color: %1; }" ).arg( wwSkinColor( "textMuted" ) ) );
	auto * folderBtn = new QPushButton( QObject::tr( "Folder..." ), libGroup );
	folderBtn->setToolTip( QObject::tr( "Choose the NifSkope library folder — poses are stored in its Poses subfolder. "
		"Also settable in Settings → General → NifSkope Library." ) );
	folderBtn->setMaximumWidth( 70 );
	folderRow->addWidget( folderLabel, 1 );
	folderRow->addWidget( folderBtn );
	libLayout->addLayout( folderRow );

	auto * poseList = new QListWidget( libGroup );
	poseList->setObjectName( QStringLiteral( "PosePoseList" ) );
	poseList->setToolTip( QObject::tr( "Pose files in the library folder. Select one and Apply (with Blend), or Save the current pose here." ) );
	poseList->setMinimumHeight( 90 );
	libLayout->addWidget( poseList );

	auto * blendRow = new QHBoxLayout;
	blendRow->addWidget( new QLabel( QObject::tr( "Blend" ), libGroup ) );
	auto * blend = new QSlider( Qt::Horizontal, libGroup );
	blend->setObjectName( QStringLiteral( "PoseBlendSlider" ) );
	blend->setRange( 0, 100 );
	blend->setValue( 100 );
	blend->setToolTip( QObject::tr( "Apply strength: 100%% replaces the current pose, less blends toward it." ) );
	blendRow->addWidget( blend, 1 );
	auto * blendVal = new QLabel( QStringLiteral( "100%" ), libGroup );
	blendVal->setMinimumWidth( 36 );
	blendRow->addWidget( blendVal );
	libLayout->addLayout( blendRow );

	auto * btnRow = new QHBoxLayout;
	auto * applyBtn = new QPushButton( QObject::tr( "Apply" ), libGroup );
	auto * saveBtn = new QPushButton( QObject::tr( "Save current..." ), libGroup );
	auto * delBtn = new QPushButton( QObject::tr( "Delete" ), libGroup );
	btnRow->addWidget( applyBtn );
	btnRow->addWidget( saveBtn );
	btnRow->addWidget( delBtn );
	libLayout->addLayout( btnRow );

	// Outfit Studio pose XML files (BodySlide) — the blend slider applies here too
	auto * osRow = new QHBoxLayout;
	auto * importOsBtn = new QPushButton( QObject::tr( "Import pose file..." ), libGroup );
	importOsBtn->setObjectName( QStringLiteral( "PoseImportOsButton" ) );
	importOsBtn->setToolTip( QObject::tr( "Load an Outfit Studio pose (.xml) and apply it (uses the Blend strength)" ) );
	auto * exportOsBtn = new QPushButton( QObject::tr( "Export pose file..." ), libGroup );
	exportOsBtn->setToolTip( QObject::tr( "Save the current pose as an Outfit Studio pose (.xml)" ) );
	osRow->addWidget( importOsBtn );
	osRow->addWidget( exportOsBtn );
	libLayout->addLayout( osRow );

	layout->addWidget( libGroup );

	// ---- behaviour --------------------------------------------------------

	// Guard against the list<->viewport selection feedback loop: while we push a
	// selection from one side to the other, the return signal must not bounce.
	auto syncing = std::make_shared<bool>( false );

	// A refresh needs to survive block-renumbering (save/apply insert or remove
	/* The ACTIVE pose. A pose is written into the bone transforms, so it survives
	 * on its own for bones that already exist — but a piece merged in afterwards
	 * brings bones that were never posed, and the rig would come apart at exactly
	 * the seam you just added. Remembering which pose is on lets it be re-applied
	 * over the whole rig after every merge, so "load a pose and it stays loaded"
	 * holds no matter what arrives later. Cleared only by picking Default (rest).
	 */
	auto activePose = std::make_shared<QString>();
	auto activeBlend = std::make_shared<float>( 1.0f );

	// blocks), so the list carries node NAMES and resolves them per action.
	auto refresh = [=]() {
		NifModel * m = skope ? skope->getNifModel() : nif;
		boneList->clear();
		poseList->clear();
		if ( !m || m->getBlockCount() == 0 ) {
			status->setText( QObject::tr( "Open a skinned mesh to pose its skeleton." ) );
			return;
		}

		// remember which library pose was selected so a refresh (fired on every
		// model edit) doesn't drop the user's selection out from under them
		const QString selectedPose = poseList->currentItem()
			? poseList->currentItem()->data( Qt::UserRole ).toString() : QString();

		/* ...and which BONE was selected, which had no equivalent.
		 *
		 * Picking a bone in the viewport emits poseBonePicked and then clicked;
		 * clicked reaches NifSkope::select -> currentNifIndexChanged -> this
		 * refresh(), which opens with boneList->clear(). So picking a bone in the
		 * 3D view un-selected it in the panel, every time. The pose list directly
		 * above was already stashed and restored by name; the bone list never was.
		 */
		const QString selectedBone = boneList->currentItem()
			? boneList->currentItem()->data( Qt::UserRole ).toString() : QString();

		const QVector<int> bones = AnimSetup::poseBoneNodes( m );
		const QString filter = boneSearch->text().trimmed();
		for ( int b : bones ) {
			const QString name = m->get<QString>( m->getBlockIndex( b ), "Name" );
			if ( name.isEmpty() )
				continue;
			if ( !filter.isEmpty() && !name.contains( filter, Qt::CaseInsensitive ) )
				continue;
			// a pinned bone shows a lock marker; UserRole keeps the raw name
			const bool pinned = ogl && ogl->poseIsPinned( b );
			auto * item = new QListWidgetItem( pinned ? ( QStringLiteral( "🔒 " ) + name ) : name, boneList );
			item->setData( Qt::UserRole, name );	// resolve to a block on click
			if ( pinned )
				item->setForeground( QBrush( QColor( 0x9a, 0x9a, 0xa2 ) ) );
			if ( !selectedBone.isEmpty() && name == selectedBone )
				boneList->setCurrentItem( item );
		}

		// library = the .xml pose files in <library>/Poses (base name shown, full
		// path in UserRole). The label shows the library root's name; the tooltip
		// gives the actual Poses folder.
		const QString folder = poseLibraryFolder();
		folderLabel->setText( QDir( nifskopeLibraryFolder() ).dirName() );
		folderLabel->setToolTip( QObject::tr( "Poses stored in: %1" ).arg( folder ) );
		QDir dir( folder );
		int nposes = 0;
		// Default sits above the files and is not one: it is how you turn the
		// active pose OFF. Without a row for it there is no way back to rest
		// short of Reset all, which says nothing about the pose still being on.
		auto * defaultItem = new QListWidgetItem( QObject::tr( "Default (rest pose)" ), poseList );
		defaultItem->setData( Qt::UserRole, QString() );
		defaultItem->setForeground( QBrush( QColor( 0x9a, 0x9a, 0xa2 ) ) );
		for ( const QFileInfo & fi : dir.entryInfoList( { QStringLiteral( "*.xml" ) },
				QDir::Files, QDir::Name ) ) {
			const bool isActive = ( fi.absoluteFilePath() == *activePose );
			auto * it = new QListWidgetItem(
				isActive ? QStringLiteral( "● " ) + fi.completeBaseName() : fi.completeBaseName(),
				poseList );
			it->setData( Qt::UserRole, fi.absoluteFilePath() );
			if ( isActive )
				it->setForeground( QBrush( QColor( 0xFF, 0x9D, 0x00 ) ) );
			if ( !selectedPose.isEmpty() && fi.absoluteFilePath() == selectedPose )
				poseList->setCurrentItem( it );   // restore the selection by path
			nposes++;
		}
		if ( activePose->isEmpty() )
			poseList->setCurrentItem( defaultItem );

		status->setText( bones.isEmpty()
			? QObject::tr( "No skinned geometry — nothing to pose." )
			: QObject::tr( "%1 bone(s), %2 pose file(s).%3" ).arg( bones.size() ).arg( nposes )
				.arg( activePose->isEmpty() ? QString()
					: QObject::tr( " Pose: %1." ).arg( QFileInfo( *activePose ).completeBaseName() ) ) );
	};

	// resolve a node name to its current block (block numbers shift under edits)
	auto blockForName = [=]( const QString & name ) -> int {
		NifModel * m = skope ? skope->getNifModel() : nif;
		if ( !m )
			return -1;
		for ( int b = 0; b < m->getBlockCount(); b++ ) {
			QModelIndex i = m->getBlockIndex( b );
			if ( m->blockInherits( i, "NiAVObject" ) && m->get<QString>( i, "Name" ) == name )
				return b;
		}
		return -1;
	};

	// select bone(s) in the list -> select them in the viewport, ready for
	// G/R/S. Multiple rows select multiple bones (transformed together).
	QObject::connect( boneList, &QListWidget::itemSelectionChanged, panel, [=]() {
		NifModel * m = skope ? skope->getNifModel() : nif;
		if ( !m || !ogl || *syncing )
			return;
		const QList<QListWidgetItem *> sel = boneList->selectedItems();
		if ( sel.isEmpty() )
			return;
		QSet<int> blocks;
		for ( QListWidgetItem * it : sel ) {
			const int b = blockForName( it->data( Qt::UserRole ).toString() );
			if ( b >= 0 )
				blocks.insert( b );
		}
		if ( blocks.isEmpty() )
			return;
		const int active = blockForName( boneList->currentItem()
			? boneList->currentItem()->data( Qt::UserRole ).toString() : sel.last()->data( Qt::UserRole ).toString() );
		ogl->setObjectSelection( blocks, active >= 0 ? active : *blocks.constBegin() );

		// colour like the Block List: active = orange text, other selected =
		// blue row background (the rest cleared)
		const QColor orange( 0xFF, 0x9D, 0x00 );
		const QColor blueRow( 0x2b, 0x3b, 0x5c );
		QSet<QListWidgetItem *> selSet( sel.constBegin(), sel.constEnd() );
		for ( int r = 0; r < boneList->count(); r++ ) {
			QListWidgetItem * it = boneList->item( r );
			const int b = blockForName( it->data( Qt::UserRole ).toString() );
			const bool isActive = ( b == active );
			const bool isSel = selSet.contains( it );
			it->setForeground( isActive ? QBrush( orange ) : QBrush() );
			it->setBackground( ( isSel && !isActive ) ? QBrush( blueRow ) : QBrush() );
		}
	} );

	// bring in a skeleton / armour piece from another NIF (the merge that shares
	// same-named bones, so the pieces pose as one skeleton). Both the file and
	// archive paths funnel through this: check there's a document, then report.
	auto currentTarget = [=]() -> NifModel * {
		NifModel * m = skope ? skope->getNifModel() : nif;
		if ( !m || m->getBlockCount() == 0 ) {
			QMessageBox::information( panel, QObject::tr( "Load skeleton" ),
				QObject::tr( "Open a NIF first, then merge a skeleton into it." ) );
			return nullptr;
		}
		return m;
	};
	auto reportMerge = [=]( const QString & label, bool ok, const NifMergeResult & r ) {
		if ( !ok ) {
			QMessageBox::warning( panel, QObject::tr( "Load skeleton" ), r.error );
			return;
		}
		status->setText( QObject::tr( "Merged %1: %2 bone(s) shared, %3 added, %4 shape(s)." )
			.arg( label ).arg( r.nodesReused ).arg( r.nodesAdded ).arg( r.shapesAdded ) );
		if ( r.nodesReused == 0 )
			QMessageBox::information( panel, QObject::tr( "Load skeleton" ),
				QObject::tr( "No bones matched by name, so the merged pieces do not share a "
					"skeleton — posing them as one rig will not work." ) );
		// A rig binds bones by NAME, so a repeated name silently sends a pose to
		// the wrong node and the rig folds up. It should never happen; say so
		// loudly if it does, because the symptom names no cause.
		else if ( !r.duplicateNames.isEmpty() )
			QMessageBox::warning( panel, QObject::tr( "Load skeleton" ),
				QObject::tr( "%1 bone name(s) now appear on more than one node:\n\n%2\n\n"
					"Posing will address only one of each pair. Undo the merge." )
					.arg( r.duplicateNames.size() )
					.arg( r.duplicateNames.mid( 0, 12 ).join( QStringLiteral( ", " ) )
						+ ( r.duplicateNames.size() > 12 ? QObject::tr( ", ..." ) : QString() ) ) );
		/* Put the active pose back over the whole rig. The bones that were
		 * already here kept it — it lives in their transforms — but everything
		 * that just arrived is at bind, and a half-posed rig is worse than an
		 * unposed one. Re-applying costs nothing when nothing new matched.
		 */
		if ( ogl && !activePose->isEmpty() ) {
			QString poseError;
			const int n = ogl->poseImportOutfitStudio( *activePose, *activeBlend, &poseError );
			if ( n > 0 )
				status->setText( status->text() + QObject::tr( " Re-applied pose '%1' over %2 bone(s)." )
					.arg( QFileInfo( *activePose ).completeBaseName() ).arg( n ) );
		}
		refresh();
	};

	// From file: the classic file-dialog path
	QObject::connect( loadFromFileAct, &QAction::triggered, panel, [=]() {
		NifModel * m = currentTarget();
		if ( !m )
			return;
		const QString path = QFileDialog::getOpenFileName( panel,
			QObject::tr( "Load skeleton / armour piece" ), m->getFolder(),
			QObject::tr( "NIF files (*.nif)" ) );
		if ( path.isEmpty() )
			return;
		NifMergeResult r;
		const bool ok = nifMergeFile( m, path, true, r );
		reportMerge( QFileInfo( path ).fileName(), ok, r );
	} );

	// From game archive: reuse the existing NIF Browser to pick a NIF from a
	// BSA/BA2, extract its bytes, and merge them (no unpack to disk needed)
	QObject::connect( loadFromArchiveAct, &QAction::triggered, panel, [=]() {
		NifModel * m = currentTarget();
		if ( !m )
			return;
		if ( !skope ) {
			QMessageBox::information( panel, QObject::tr( "Load skeleton" ),
				QObject::tr( "The NIF Browser is only available from the main window." ) );
			return;
		}
		QByteArray bytes;
		QString label;
		if ( !skope->pickNifFromBrowser( panel, bytes, label ) )
			return;
		NifMergeResult r;
		const bool ok = nifMergeData( m, bytes, label, true, r );
		reportMerge( label, ok, r );
	} );

	// picking a bone in the viewport highlights it in the list
	if ( ogl )
		QObject::connect( ogl, &GLView::poseBonePicked, panel, [=]( int block ) {
			NifModel * m = skope ? skope->getNifModel() : nif;
			if ( !m || block < 0 )
				return;
			const QString name = m->get<QString>( m->getBlockIndex( block ), "Name" );
			// reflect the viewport pick in the list without bouncing back
			*syncing = true;
			for ( int r = 0; r < boneList->count(); r++ )
				if ( boneList->item( r )->data( Qt::UserRole ).toString() == name ) {
					boneList->setCurrentRow( r );
					break;
				}
			*syncing = false;
		} );

	QObject::connect( boneSearch, &QLineEdit::textChanged, panel, [=]() { refresh(); } );

	// channel bitmask from the combo: All=7, Rotation=1, Location=2, Scale=4
	auto resetChannels = [resetWhat]() {
		switch ( resetWhat->currentIndex() ) {
		case 1: return 1;
		case 2: return 2;
		case 3: return 4;
		default: return 7;
		}
	};
	QObject::connect( resetBtn, &QPushButton::clicked, panel, [=]() {
		if ( !ogl ) return;
		QListWidgetItem * sel = boneList->currentItem();
		if ( !sel ) return;
		const int b = blockForName( sel->data( Qt::UserRole ).toString() );
		if ( b >= 0 )
			ogl->poseResetBone( b, resetChannels() );
	} );
	QObject::connect( resetAllBtn, &QPushButton::clicked, panel, [=]() {
		if ( ogl )
			ogl->poseResetBone( -1, resetChannels() );
	} );
	QObject::connect( mirrorBtn, &QPushButton::clicked, panel, [=]() {
		if ( !ogl ) return;
		QListWidgetItem * sel = boneList->currentItem();
		if ( !sel ) return;
		const int b = blockForName( sel->data( Qt::UserRole ).toString() );
		if ( b >= 0 && ogl->poseMirrorBone( b ) < 0 )
			status->setText( QObject::tr( "No L/R counterpart found for that bone." ) );
	} );

	// display toggles drive the viewport; the filter also changes the bone list
	QObject::connect( filterCombo, QOverload<int>::of( &QComboBox::currentIndexChanged ),
		panel, [=]( int idx ) {
			if ( ogl ) ogl->setPoseBoneFilter( idx );
			refresh();
		} );
	QObject::connect( namesChk, &QCheckBox::toggled, panel,
		[=]( bool on ) { if ( ogl ) ogl->setPoseShowBoneNames( on ); } );
	QObject::connect( relChk, &QCheckBox::toggled, panel,
		[=]( bool on ) { if ( ogl ) ogl->setPoseShowRelations( on ); } );
	QObject::connect( weightsChk, &QCheckBox::toggled, panel,
		[=]( bool on ) { if ( ogl ) ogl->setPoseShowWeights( on ); } );

	// pin / unpin the selected bone(s)
	QObject::connect( pinBtn, &QPushButton::clicked, panel, [=]() {
		if ( !ogl ) return;
		for ( QListWidgetItem * it : boneList->selectedItems() ) {
			const int b = blockForName( it->data( Qt::UserRole ).toString() );
			if ( b >= 0 )
				ogl->poseTogglePin( b );
		}
		refresh();
	} );
	QObject::connect( unpinAllBtn, &QPushButton::clicked, panel, [=]() {
		if ( ogl ) ogl->poseClearPins();
		refresh();
	} );

	// non-destructive toggle + bake
	QObject::connect( ndChk, &QCheckBox::toggled, panel,
		[=]( bool on ) { if ( ogl ) ogl->setPoseNonDestructive( on ); } );
	QObject::connect( bakeBtn, &QPushButton::clicked, panel, [=]() {
		if ( ogl ) ogl->poseBakeToBones();
	} );

	QObject::connect( blend, &QSlider::valueChanged, blendVal, [=]( int v ) {
		blendVal->setText( QStringLiteral( "%1%" ).arg( v ) );
	} );

	// choose the NifSkope library root (poses live in its Poses subfolder)
	QObject::connect( folderBtn, &QPushButton::clicked, panel, [=]() {
		const QString dir = QFileDialog::getExistingDirectory( panel,
			QObject::tr( "NifSkope library folder" ), nifskopeLibraryFolder() );
		if ( !dir.isEmpty() ) {
			setNifskopeLibraryFolder( dir );
			refresh();
		}
	} );

	// Save current pose as a new .xml file in the library folder
	QObject::connect( saveBtn, &QPushButton::clicked, panel, [=]() {
		if ( !ogl )
			return;
		bool ok = false;
		const QString name = QInputDialog::getText( panel, QObject::tr( "Save pose" ),
			QObject::tr( "Pose name:" ), QLineEdit::Normal,
			QStringLiteral( "Pose" ), &ok ).trimmed();
		if ( !ok || name.isEmpty() )
			return;
		QString safe = name;
		safe.replace( QRegularExpression( QStringLiteral( "[\\\\/:*?\"<>|]" ) ), QStringLiteral( "_" ) );
		const QString path = QDir( poseLibraryFolder() ).filePath( safe + QStringLiteral( ".xml" ) );
		QString error;
		if ( !ogl->poseExportOutfitStudio( path, name, &error ) )
			QMessageBox::warning( panel, QObject::tr( "Save pose" ), error );
		else
			status->setText( QObject::tr( "Saved pose '%1' to the library." ).arg( name ) );
		refresh();
	} );

	// Apply the selected library pose file (with the blend strength). Double-
	// clicking a pose applies it too.
	auto applyLibraryPose = [=]( QListWidgetItem * sel ) {
		if ( !ogl || !sel )
			return;
		const QString path = sel->data( Qt::UserRole ).toString();
		// Default (rest): the only way to turn the active pose off
		if ( path.isEmpty() ) {
			activePose->clear();
			ogl->poseResetBone( -1, 7 );		// every channel, every bone
			status->setText( QObject::tr( "Back to the rest pose." ) );
			refresh();
			return;
		}
		QString error;
		const int n = ogl->poseImportOutfitStudio( path, blend->value() / 100.0f, &error );
		if ( n <= 0 ) {
			QMessageBox::warning( panel, QObject::tr( "Apply pose" ), error );
			return;
		}
		*activePose = path;
		*activeBlend = blend->value() / 100.0f;
		status->setText( QObject::tr( "Applied '%1': %2 bone(s) posed.%3" )
			.arg( QFileInfo( path ).completeBaseName() ).arg( n )
			.arg( error.isEmpty() ? QString() : QStringLiteral( " " ) + error ) );
		refresh();
	};
	QObject::connect( applyBtn, &QPushButton::clicked, panel,
		[=]() { applyLibraryPose( poseList->currentItem() ); } );
	QObject::connect( poseList, &QListWidget::itemDoubleClicked, panel,
		[=]( QListWidgetItem * it ) { applyLibraryPose( it ); } );

	// Delete the selected library pose file (with confirmation)
	QObject::connect( delBtn, &QPushButton::clicked, panel, [=]() {
		QListWidgetItem * sel = poseList->currentItem();
		if ( !sel )
			return;
		const QString path = sel->data( Qt::UserRole ).toString();
		if ( QMessageBox::question( panel, QObject::tr( "Delete pose" ),
				QObject::tr( "Delete the pose file '%1'?" ).arg( sel->text() ) ) != QMessageBox::Yes )
			return;
		QFile::remove( path );
		refresh();
	} );

	// Outfit Studio pose XML import/export (uses the blend slider on import)
	QObject::connect( importOsBtn, &QPushButton::clicked, panel, [=]() {
		if ( !ogl ) return;
		const QString path = QFileDialog::getOpenFileName( panel,
			QObject::tr( "Import Outfit Studio pose" ), poseLibraryFolder(),
			QObject::tr( "Pose XML (*.xml)" ) );
		if ( path.isEmpty() ) return;
		QString error;
		const int n = ogl->poseImportOutfitStudio( path, blend->value() / 100.0f, &error );
		if ( n <= 0 )
			QMessageBox::warning( panel, QObject::tr( "Import pose" ), error );
		else {
			// an imported pose is the active one too, so it survives later merges
			*activePose = path;
			*activeBlend = blend->value() / 100.0f;
			status->setText( QObject::tr( "Imported %1: %2 bone(s) posed." )
				.arg( QFileInfo( path ).fileName() ).arg( n ) );
			if ( !error.isEmpty() )   // partial: some bones not in this skeleton
				status->setText( status->text() + QStringLiteral( " " ) + error );
			refresh();
		}
	} );
	QObject::connect( exportOsBtn, &QPushButton::clicked, panel, [=]() {
		if ( !ogl ) return;
		QString path = QFileDialog::getSaveFileName( panel,
			QObject::tr( "Export Outfit Studio pose" ),
			QDir( poseLibraryFolder() ).filePath( QStringLiteral( "pose.xml" ) ),
			QObject::tr( "Pose XML (*.xml)" ) );
		if ( path.isEmpty() ) return;
		if ( !path.endsWith( QStringLiteral( ".xml" ), Qt::CaseInsensitive ) )
			path += QStringLiteral( ".xml" );
		QString error;
		if ( !ogl->poseExportOutfitStudio( path, QFileInfo( path ).completeBaseName(), &error ) )
			QMessageBox::warning( panel, QObject::tr( "Export pose" ), error );
		else
			status->setText( QObject::tr( "Exported pose to %1" ).arg( QFileInfo( path ).fileName() ) );
	} );

	// keep in step with the document, like the other manager docks
	if ( skope ) {
		QObject::connect( skope, &NifSkope::currentNifIndexChanged, panel, [=]( const QModelIndex & ) { refresh(); } );
		QObject::connect( skope, &NifSkope::completeLoading, panel,
			[=]( bool, QString & ) { QTimer::singleShot( 0, panel, [=]() { refresh(); } ); } );
		// live-update the library when the folder is changed in Settings
		// (General -> Poses); both write the same Pose Library Folder key
		if ( SettingsDialog * opt = skope->getOptions() )
			QObject::connect( opt, &SettingsDialog::saveSettings, panel,
				[=]() { QTimer::singleShot( 0, panel, [=]() { refresh(); } ); } );
	}
	QObject::connect( nif, &QAbstractItemModel::modelReset, panel,
		[=]() { QTimer::singleShot( 0, panel, [=]() { refresh(); } ); } );
	if ( nif->undoStack )
		QObject::connect( nif->undoStack, &QUndoStack::indexChanged, panel,
			[=]() { QTimer::singleShot( 0, panel, [=]() { refresh(); } ); } );
	// Showing the Pose workspace enters Pose Mode (skeleton drawn, bones
	// clickable); hiding it leaves. Matches how the paint docks drive their
	// viewport modes.
	QObject::connect( dock, &QDockWidget::visibilityChanged, panel, [=]( bool visible ) {
		if ( ogl ) {
			if ( visible && !ogl->poseModeActive() )
				ogl->setPoseMode( true );
			else if ( !visible && ogl->poseModeActive() )
				ogl->setPoseMode( false );
		}
		if ( visible )
			refresh();
	} );

	refresh();
	dock->setWidget( panel );
	/* Docked and hidden, like every sibling. This was returning an UNDOCKED
	 * QDockWidget; skeletontools.cpp documents the startup bug that same
	 * omission caused there.
	 */
	mw->addDockWidget( Qt::RightDockWidgetArea, dock );
	dock->hide();
	return dock;
}
