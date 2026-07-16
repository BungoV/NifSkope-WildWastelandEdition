#include "spellbook.h"
#include "blocks.h"
#include "mesh.h"
#include "nifsnapshot.h"
#include "nifskope.h"
#include "glview.h"
#include "ui/widgets/colorwheel.h"

#include "model/nifmodel.h"

#include <QApplication>
#include <QCoreApplication>
#include <QCheckBox>
#include <QElapsedTimer>
#include <QFile>
#include <QComboBox>
#include <QDockWidget>
#include <QDropEvent>
#include <QDir>
#include <QEventLoop>
#include <QFileDialog>
#include <QFileInfo>
#include <QFloat16>
#include <QGroupBox>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QMainWindow>
#include <QMap>
#include <QMenu>
#include <QMessageBox>
#include <QPainter>
#include <QPushButton>
#include <QProgressDialog>
#include <QSet>
#include <QSignalBlocker>
#include <QScrollArea>
#include <QShortcut>
#include <QSlider>
#include <QStyledItemDelegate>
#include <QStringList>
#include <QTimer>
#include <QTemporaryFile>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <algorithm>
#include <array>
#include <bit>
#include <climits>
#include <cmath>
#include <functional>
#include <limits>
#include <map>
#include <numeric>
#include <memory>

class RiggingBoneSelectionDelegate final : public QStyledItemDelegate
{
public:
	explicit RiggingBoneSelectionDelegate( QTreeWidget * view )
		: QStyledItemDelegate( view ), tree( view ) {}
	void paint( QPainter * painter, const QStyleOptionViewItem & option,
		const QModelIndex & index ) const override
	{
		QStyleOptionViewItem styled( option );
		const bool selected = bool( styled.state & QStyle::State_Selected );
		QModelIndex current = tree ? tree->currentIndex() : QModelIndex();
		const bool primary = selected && current.isValid()
			&& current.row() == index.row() && current.parent() == index.parent();
		styled.state &= ~QStyle::State_HasFocus;
		if ( selected ) {
			styled.palette.setColor( QPalette::Highlight,
				primary ? QColor( 74, 122, 176 ) : QColor( 43, 66, 95 ) );
			styled.palette.setColor( QPalette::HighlightedText,
				primary ? QColor( 255, 157, 0 ) : QColor( 255, 114, 0 ) );
		}
		QStyledItemDelegate::paint( painter, styled, index );
	}
private:
	QTreeWidget * tree = nullptr;
};

class RiggingBoneDropTree final : public QTreeWidget
{
public:
	using QTreeWidget::QTreeWidget;
	std::function<void( QTreeWidget * )> donorDrop;
protected:
	void dragEnterEvent( QDragEnterEvent * event ) override
	{
		if ( event->source() && event->source() != this ) event->acceptProposedAction();
		else QTreeWidget::dragEnterEvent( event );
	}
	void dragMoveEvent( QDragMoveEvent * event ) override
	{
		if ( event->source() && event->source() != this ) event->acceptProposedAction();
		else QTreeWidget::dragMoveEvent( event );
	}
	void dropEvent( QDropEvent * event ) override
	{
		if ( auto * source = qobject_cast<QTreeWidget *>( event->source() );
			source && source != this && donorDrop ) {
			donorDrop( source );
			event->acceptProposedAction();
			return;
		}
		QTreeWidget::dropEvent( event );
	}
};

// Rigging — bone & weight transfer feature.
// Design + validated algorithms: BONE_WEIGHT_TRANSFER_PLAN.md and
// tools/rigging_prototype/. This file is the NifSkope-side home for the
// "Rigging" spell page and (later) the Rigging workspace manager dock.
//
// Implemented so far (read-only, no model mutation):
//   * List Skin Bones          — bones a shape is bound to.
//   * Compare Bones with Donor  — cross-file bone-set diff (shared / donor-only
//                                 / target-only), the pre-transfer preview.

// ---------------------------------------------------------------------------
// Shared helpers (work on any NifModel — target OR a loaded donor, per the
// cross-file architectural rule: never assume "the active document").
// ---------------------------------------------------------------------------

//! Skin instance behind a shape: "Skin" (FO4 BSSkin::Instance) or
//! "Skin Instance" (classic NiSkinInstance).
static QModelIndex riggingSkinInstance( const NifModel * nif, const QModelIndex & shape )
{
	int link = nif->getLink( shape, "Skin" );
	if ( link < 0 )
		link = nif->getLink( shape, "Skin Instance" );
	return nif->getBlockIndex( link );
}

//! Bone node names a shape is skinned to, in bone-index order.
static QStringList riggingBoneNames( const NifModel * nif, const QModelIndex & shape )
{
	QStringList names;
	QModelIndex iSkin = riggingSkinInstance( nif, shape );
	if ( !iSkin.isValid() )
		return names;

	// Both BSSkin::Instance and NiSkinInstance store Bones directly. Array
	// elements repeat the field name, so probing for a nested "Bones" would
	// accidentally select element zero and make the list appear empty.
	QModelIndex iBones = nif->getIndex( iSkin, "Bones" );
	if ( !iBones.isValid() )
		return names;

	for ( int n = 0; n < nif->rowCount( iBones ); n++ ) {
		QModelIndex iNode = nif->getBlockIndex( nif->getLink( nif->getIndex( iBones, n ) ) );
		names << ( iNode.isValid() ? nif->get<QString>( iNode, "Name" ) : QString() );
	}
	return names;
}

static bool riggingIsTriShape( const NifModel * nif, const QModelIndex & index )
{
	return nif && index.isValid()
		&& ( nif->blockInherits( index, "NiTriBasedGeom" ) || nif->blockInherits( index, "BSTriShape" ) );
}

static bool riggingIsSkinnedShape( const NifModel * nif, const QModelIndex & index )
{
	return riggingIsTriShape( nif, index ) && riggingSkinInstance( nif, index ).isValid();
}

static float riggingTransformDelta( const Transform & a, const Transform & b );
static bool riggingNodeWorld( const NifModel * nif, int node, int root, Transform & world );

//! All skinned shapes in a model, with display labels.
static QList<int> riggingSkinnedShapes( const NifModel * nif, QStringList & labels )
{
	QList<int> shapes;
	for ( int b = 0; b < nif->getBlockCount(); b++ ) {
		QModelIndex idx = nif->getBlockIndex( b );
		if ( !riggingIsSkinnedShape( nif, idx ) )
			continue;
		QString nm = nif->get<QString>( idx, "Name" );
		labels << QString( "%1 [%2]" ).arg( nm.isEmpty() ? Spell::tr( "<shape>" ) : nm ).arg( b );
		shapes << b;
	}
	return shapes;
}

// The combined workflow reuses the individually-tested spells while supplying
// one donor choice, suppressing their intermediate confirmations, and tracking
// whether each step reached its success path. NifSkope runs spells on the UI
// thread, so a small scoped state is sufficient here.
struct RiggingWorkflowState
{
	bool active = false;
	bool stepSucceeded = false;
	QString donorFile;
	int donorShape = -1;
	QVector<QVector<QPair<QString, float>>> preparedWeights;
};

static RiggingWorkflowState riggingWorkflow;

static std::unique_ptr<QTemporaryFile> riggingSessionDonorFile;
static QString riggingSessionDonorLabel;

static QString riggingDonorDisplayName( const QString & fileName )
{
	if ( riggingSessionDonorFile && fileName == riggingSessionDonorFile->fileName()
		&& !riggingSessionDonorLabel.isEmpty() )
		return riggingSessionDonorLabel;
	return QFileInfo( fileName ).fileName();
}

static QString riggingChooseDonorFile( const NifModel * nif )
{
	if ( riggingWorkflow.active )
		return riggingWorkflow.donorFile;
	riggingSessionDonorFile.reset();
	riggingSessionDonorLabel.clear();
	NifSkope * receiver = NifSkope::documentForModel( nif );
	// Selected workspace members supply donor geometry whether they are real
	// windows or data-only background documents; both carry a NifModel and a
	// display path.
	QList<QPair<NifModel *, QString>> openDonors;
	QStringList choices;
	for ( const auto & entry : NifSkope::selectedWorkspaceModels() ) {
		if ( !entry.first || entry.first == nif
			|| entry.second.isEmpty() || entry.first->getBlockCount() <= 0 )
			continue;
		openDonors << entry;
		choices << Spell::tr( "Open document — %1" )
			.arg( QFileInfo( entry.second ).fileName() );
	}
	if ( !openDonors.isEmpty() ) {
		choices << Spell::tr( "Browse for another NIF..." );
		bool ok = false;
		QString selected = QInputDialog::getItem( receiver, Spell::tr( "Choose Donor NIF" ),
			Spell::tr( "Donor source:" ), choices, 0, false, &ok );
		if ( !ok ) return QString();
		int openIndex = choices.indexOf( selected );
		if ( openIndex >= 0 && openIndex < openDonors.size() ) {
			const QPair<NifModel *, QString> donorEntry = openDonors.at( openIndex );
			riggingSessionDonorFile = std::make_unique<QTemporaryFile>(
				QDir::tempPath() + QStringLiteral( "/nifskope-open-donor-XXXXXX.nif" ) );
			riggingSessionDonorFile->setAutoRemove( true );
			if ( !riggingSessionDonorFile->open()
				|| !donorEntry.first->save( *riggingSessionDonorFile ) ) {
				QMessageBox::warning( receiver, Spell::tr( "Choose Donor NIF" ), Spell::tr(
					"The open donor document could not be captured. Its file was not changed." ) );
				riggingSessionDonorFile.reset();
				return QString();
			}
			riggingSessionDonorFile->flush();
			riggingSessionDonorLabel = QFileInfo( donorEntry.second ).fileName();
			return riggingSessionDonorFile->fileName();
		}
	}
	return QFileDialog::getOpenFileName( nullptr, Spell::tr( "Choose Donor NIF" ),
		nif->getFolder(), Spell::tr( "NIF files (*.nif)" ) );
}

static QString riggingChooseReferenceSkeletonFile( const NifModel * nif )
{
	return QFileDialog::getOpenFileName( nullptr, Spell::tr( "Choose Game Reference Skeleton NIF" ),
		nif->getFolder(), Spell::tr( "NIF files (*.nif)" ) );
}

static int riggingChooseDonorShape( const QList<int> & shapes, const QStringList & labels,
	const QString & title )
{
	if ( shapes.isEmpty() )
		return -1;
	if ( riggingWorkflow.active )
		return shapes.contains( riggingWorkflow.donorShape ) ? riggingWorkflow.donorShape : -1;
	if ( shapes.size() == 1 )
		return shapes.first();

	bool ok = false;
	QString chosen = QInputDialog::getItem( nullptr, title, Spell::tr( "Donor shape:" ), labels, 0, false, &ok );
	return ok ? shapes.at( qMax( 0, labels.indexOf( chosen ) ) ) : -1;
}

static void riggingWorkflowStepSucceeded()
{
	if ( riggingWorkflow.active )
		riggingWorkflow.stepSucceeded = true;
}

// ---------------------------------------------------------------------------
// Geometry + transfer core. Ported from tools/rigging_prototype/ (validated in
// Python AND C++: self-transfer exact, inverse-bind ~1e-5, C++ reproduces Python
// exactly). FO4 BSTriShape (BSVertexData) only for now — classic NiSkinData TBD.
// ---------------------------------------------------------------------------

struct RigShape
{
	QVector<Vector3> pos;                              // per-vertex position (shape space)
	QVector<Triangle> tris;
	QVector<QVector<QPair<QString, float>>> skin;      // per-vertex (boneName, weight)
	QStringList bones;
	bool valid = false;
};

enum RiggingReceiverRowType
{
	RiggingReceiverBoneRow = 0,
	RiggingReceiverSegmentRow,
	RiggingReceiverSubsegmentRow
};

static constexpr int RiggingReceiverRowTypeRole = Qt::UserRole + 10;
static constexpr int RiggingReceiverSegmentRole = Qt::UserRole + 11;
static constexpr int RiggingReceiverSubsegmentRole = Qt::UserRole + 12;

static bool riggingIsReceiverBoneRow( const QTreeWidgetItem * item )
{
	return item && item->data( 0, RiggingReceiverRowTypeRole ).toInt() == RiggingReceiverBoneRow;
}

struct RigSegmentRange
{
	int segment = -1;
	int subsegment = -1;
	int startTriangle = 0;
	int triangleCount = 0;
	quint32 parentArrayIndex = std::numeric_limits<quint32>::max();
	quint32 userIndex = std::numeric_limits<quint32>::max();
	quint32 boneId = std::numeric_limits<quint32>::max();
	bool hasSharedData = false;
};

struct RigSegmentSharedEntry
{
	quint32 userIndex = std::numeric_limits<quint32>::max();
	quint32 boneId = std::numeric_limits<quint32>::max();
	QVector<float> cutOffsets;
};

struct RigSegmentDefinition
{
	RigSegmentSharedEntry parent;
	QVector<RigSegmentSharedEntry> children;
};

//! Read FO4 BSSubIndexTriShape segment ranges. Start Index is an index-buffer
//! offset, hence the division by three used by NifSkope's existing renderer.
static QVector<RigSegmentRange> riggingReadSegmentRanges( const NifModel * nif,
	const QModelIndex & shape )
{
	QVector<RigSegmentRange> result;
	QModelIndex segments = nif->getIndex( shape, "Segment" );
	if ( !segments.isValid() || !nif->isArray( segments ) )
		return result;

	QModelIndex shared = nif->getIndex( shape, "Segment Data" );
	QModelIndex perSegment = nif->getIndex( shared, "Per Segment Data" );
	auto addSharedData = [&]( RigSegmentRange & range ) {
		if ( !perSegment.isValid() || !nif->isArray( perSegment )
			|| range.parentArrayIndex >= quint32( nif->rowCount( perSegment ) ) ) return;
		QModelIndex data = nif->getIndex( perSegment, int( range.parentArrayIndex ) );
		range.userIndex = nif->get<quint32>( data, "User Index" );
		range.boneId = nif->get<quint32>( data, "Bone ID" );
		range.hasSharedData = true;
	};

	for ( int segmentIndex = 0; segmentIndex < nif->rowCount( segments ); segmentIndex++ ) {
		QModelIndex segment = nif->getIndex( segments, segmentIndex );
		RigSegmentRange parent;
		parent.segment = segmentIndex;
		parent.startTriangle = int( nif->get<quint32>( segment, "Start Index" ) / 3U );
		parent.triangleCount = int( nif->get<quint32>( segment, "Num Primitives" ) );
		parent.parentArrayIndex = nif->get<quint32>( segment, "Parent Array Index" );
		addSharedData( parent );
		result.append( parent );

		QModelIndex subsegments = nif->getIndex( segment, "Sub Segment" );
		if ( !subsegments.isValid() || !nif->isArray( subsegments ) ) continue;
		for ( int subIndex = 0; subIndex < nif->rowCount( subsegments ); subIndex++ ) {
			QModelIndex subsegment = nif->getIndex( subsegments, subIndex );
			RigSegmentRange child;
			child.segment = segmentIndex;
			child.subsegment = subIndex;
			child.startTriangle = int( nif->get<quint32>( subsegment, "Start Index" ) / 3U );
			child.triangleCount = int( nif->get<quint32>( subsegment, "Num Primitives" ) );
			child.parentArrayIndex = nif->get<quint32>( subsegment, "Parent Array Index" );
			addSharedData( child );
			result.append( child );
		}
	}
	return result;
}

static QVector<RigSegmentDefinition> riggingReadSegmentDefinitions( const NifModel * nif,
	const QModelIndex & shape )
{
	QVector<RigSegmentDefinition> definitions;
	const int count = int( nif->get<quint32>( shape, "Num Segments" ) );
	definitions.resize( qMax( count, 0 ) );
	QModelIndex shared = nif->getIndex( shape, "Segment Data" );
	QModelIndex perData = nif->getIndex( shared, "Per Segment Data" );
	auto readEntry = [&]( quint32 arrayIndex ) {
		RigSegmentSharedEntry entry;
		if ( !perData.isValid() || arrayIndex >= quint32( nif->rowCount( perData ) ) ) return entry;
		QModelIndex data = nif->getIndex( perData, int( arrayIndex ) );
		entry.userIndex = nif->get<quint32>( data, "User Index" );
		entry.boneId = nif->get<quint32>( data, "Bone ID" );
		QModelIndex cuts = nif->getIndex( data, "Cut Offsets" );
		if ( cuts.isValid() )
			for ( int i = 0; i < nif->rowCount( cuts ); i++ )
				entry.cutOffsets.append( nif->get<float>( nif->getIndex( cuts, i ) ) );
		return entry;
	};
	QModelIndex segments = nif->getIndex( shape, "Segment" );
	for ( int segment = 0; segment < definitions.size() && segment < nif->rowCount( segments ); segment++ ) {
		QModelIndex row = nif->getIndex( segments, segment );
		definitions[segment].parent = readEntry( nif->get<quint32>( row, "Parent Array Index" ) );
		QModelIndex children = nif->getIndex( row, "Sub Segment" );
		for ( int child = 0; child < nif->rowCount( children ); child++ )
			definitions[segment].children.append( readEntry(
				nif->get<quint32>( nif->getIndex( children, child ), "Parent Array Index" ) ) );
	}
	return definitions;
}

static void riggingReadTriangleMembership( const NifModel * nif, const QModelIndex & shape,
	QVector<int> & segmentOf, QVector<int> & subsegmentOf )
{
	const int triangles = nif->rowCount( nif->getIndex( shape, "Triangles" ) );
	segmentOf.fill( 0, triangles );
	subsegmentOf.fill( -1, triangles );
	for ( const RigSegmentRange & range : riggingReadSegmentRanges( nif, shape ) ) {
		const int first = qBound( 0, range.startTriangle, triangles );
		const int last = qBound( first, first + range.triangleCount, triangles );
		for ( int triangle = first; triangle < last; triangle++ ) {
			segmentOf[triangle] = range.segment;
			if ( range.subsegment >= 0 ) subsegmentOf[triangle] = range.subsegment;
		}
	}
}

//! Reorder triangles into contiguous segment/subsegment ranges and rebuild every
//! dependent FO4 range/shared-data array. Each triangle has one exclusive owner.
static bool riggingWriteSegmentLayout( NifModel * nif, const QModelIndex & shape,
	const QVector<RigSegmentDefinition> & definitions, const QVector<int> & segmentOf,
	const QVector<int> & subsegmentOf, QString & error )
{
	if ( !nif->blockInherits( shape, "BSSubIndexTriShape" ) || definitions.isEmpty() ) {
		error = Spell::tr( "Segment editing requires a BSSubIndexTriShape with Segment 0." );
		return false;
	}
	QModelIndex trianglesIndex = nif->getIndex( shape, "Triangles" );
	QVector<Triangle> original;
	for ( int i = 0; i < nif->rowCount( trianglesIndex ); i++ )
		original.append( nif->get<Triangle>( nif->getIndex( trianglesIndex, i ) ) );
	if ( segmentOf.size() != original.size() || subsegmentOf.size() != original.size() ) {
		error = Spell::tr( "Triangle membership no longer matches the shape topology." );
		return false;
	}
	QVector<QVector<QVector<int>>> groups( definitions.size() );
	for ( int segment = 0; segment < definitions.size(); segment++ )
		groups[segment].resize( definitions.at( segment ).children.size() + 1 ); // 0 = parent-only
	for ( int triangle = 0; triangle < original.size(); triangle++ ) {
		int segment = qBound( 0, segmentOf.at( triangle ), definitions.size() - 1 );
		int child = subsegmentOf.at( triangle );
		if ( child < 0 || child >= definitions.at( segment ).children.size() ) child = -1;
		groups[segment][child + 1].append( triangle );
	}
	QVector<Triangle> ordered;
	ordered.reserve( original.size() );
	struct Range { int start = 0; int count = 0; QVector<QPair<int,int>> children; };
	QVector<Range> ranges( definitions.size() );
	for ( int segment = 0; segment < definitions.size(); segment++ ) {
		ranges[segment].start = ordered.size();
		for ( int childGroup = 0; childGroup < groups[segment].size(); childGroup++ ) {
			const int start = ordered.size();
			for ( int oldTriangle : groups[segment][childGroup] ) ordered.append( original.at( oldTriangle ) );
			if ( childGroup > 0 ) ranges[segment].children.append(
				qMakePair( start, ordered.size() - start ) );
		}
		ranges[segment].count = ordered.size() - ranges[segment].start;
	}
	const int total = std::accumulate( definitions.cbegin(), definitions.cend(),
		definitions.size(), []( int sum, const RigSegmentDefinition & d ) { return sum + d.children.size(); } );
	nif->set<quint32>( shape, "Num Segments", quint32( definitions.size() ) );
	nif->set<quint32>( shape, "Total Segments", quint32( total ) );
	nif->set<quint32>( shape, "Num Primitives", quint32( ordered.size() ) );
	nif->updateArraySize( shape, "Segment" );
	QModelIndex segments = nif->getIndex( shape, "Segment" );
	quint32 sharedIndex = 0;
	for ( int segment = 0; segment < definitions.size(); segment++ ) {
		QModelIndex row = nif->getIndex( segments, segment );
		nif->set<quint32>( row, "Start Index", quint32( ranges.at( segment ).start * 3 ) );
		nif->set<quint32>( row, "Num Primitives", quint32( ranges.at( segment ).count ) );
		nif->set<quint32>( row, "Parent Array Index", sharedIndex++ );
		nif->set<quint32>( row, "Num Sub Segments", quint32( definitions.at( segment ).children.size() ) );
		nif->updateArraySize( row, "Sub Segment" );
		QModelIndex children = nif->getIndex( row, "Sub Segment" );
		for ( int child = 0; child < definitions.at( segment ).children.size(); child++ ) {
			QModelIndex sub = nif->getIndex( children, child );
			nif->set<quint32>( sub, "Start Index", quint32( ranges.at( segment ).children.at( child ).first * 3 ) );
			nif->set<quint32>( sub, "Num Primitives", quint32( ranges.at( segment ).children.at( child ).second ) );
			nif->set<quint32>( sub, "Parent Array Index", sharedIndex++ );
		}
	}
	nif->setArray<Triangle>( shape, "Triangles", ordered );
	if ( total > definitions.size() ) {
		QModelIndex shared = nif->getIndex( shape, "Segment Data" );
		if ( !shared.isValid() ) {
			error = Spell::tr( "NifModel did not expose Segment Data after enabling subsegments." );
			return false;
		}
		nif->set<quint32>( shared, "Num Segments", quint32( definitions.size() ) );
		nif->set<quint32>( shared, "Total Segments", quint32( total ) );
		nif->updateArraySize( shared, "Segment Starts" );
		nif->updateArraySize( shared, "Per Segment Data" );
		QModelIndex starts = nif->getIndex( shared, "Segment Starts" );
		QModelIndex data = nif->getIndex( shared, "Per Segment Data" );
		int out = 0;
		auto writeEntry = [&]( const RigSegmentSharedEntry & entry ) {
			QModelIndex row = nif->getIndex( data, out++ );
			nif->set<quint32>( row, "User Index", entry.userIndex );
			nif->set<quint32>( row, "Bone ID", entry.boneId );
			nif->set<quint32>( row, "Num Cut Offsets", quint32( qMin( entry.cutOffsets.size(), 8 ) ) );
			nif->updateArraySize( row, "Cut Offsets" );
			for ( int i = 0; i < qMin( entry.cutOffsets.size(), 8 ); i++ )
				nif->set<float>( nif->getIndex( nif->getIndex( row, "Cut Offsets" ), i ), entry.cutOffsets.at( i ) );
		};
		for ( int segment = 0; segment < definitions.size(); segment++ ) {
			nif->set<quint32>( nif->getIndex( starts, segment ), quint32( out ) );
			RigSegmentSharedEntry parent = definitions.at( segment ).parent;
			// The leading shared entry is the top-level segment marker. With no
			// bone hash its User Index must track the current Segment array index,
			// including after a segment before it was deleted.
			if ( parent.boneId == std::numeric_limits<quint32>::max() )
				parent.userIndex = quint32( segment );
			writeEntry( parent );
			for ( const RigSegmentSharedEntry & child : definitions.at( segment ).children ) writeEntry( child );
		}
	}
	return true;
}

//! Read an FO4 BSTriShape's geometry + skin via NifModel (handles half-float packing).
static RigShape riggingReadShape( const NifModel * nif, const QModelIndex & shape )
{
	RigShape s;
	s.bones = riggingBoneNames( nif, shape );

	QModelIndex iVD = nif->getIndex( shape, "Vertex Data" );
	if ( !iVD.isValid() || !nif->isArray( iVD ) )
		return s;                                      // not an FO4 BSTriShape layout

	int nv = nif->rowCount( iVD );
	s.pos.reserve( nv );
	s.skin.reserve( nv );
	for ( int i = 0; i < nv; i++ ) {
		QModelIndex iVertex = nif->getIndex( iVD, i );
		Vector3 position = nif->get<Vector3>( iVertex, "Vertex" );
		for ( int axis = 0; axis < 3; axis++ )
			if ( !std::isfinite( position[axis] ) )
				return s;
		s.pos << position;

		QVector<QPair<QString, float>> infl;
		QModelIndex iW = nif->getIndex( iVertex, "Bone Weights" );
		QModelIndex iI = nif->getIndex( iVertex, "Bone Indices" );
		if ( iW.isValid() && iI.isValid() ) {
			int nw = qMin( nif->rowCount( iW ), nif->rowCount( iI ) );
			for ( int j = 0; j < nw; j++ ) {
				float w = nif->get<float>( nif->getIndex( iW, j ) );
				int bi = nif->get<quint8>( nif->getIndex( iI, j ) );
				if ( w > 0.0001f && bi >= 0 && bi < s.bones.size() )
					infl << qMakePair( s.bones.at( bi ), w );
			}
		}
		s.skin << infl;
	}

	QModelIndex iT = nif->getIndex( shape, "Triangles" );
	if ( iT.isValid() )
		for ( int t = 0; t < nif->rowCount( iT ); t++ ) {
			Triangle triangle = nif->get<Triangle>( nif->getIndex( iT, t ) );
			if ( triangle.v1() >= nv || triangle.v2() >= nv || triangle.v3() >= nv )
				return s;
			s.tris << triangle;
		}

	s.valid = ( nv > 0 && !s.tris.isEmpty() );
	return s;
}

//! Closest point on triangle abc to p, with barycentric coords (Ericson).
static Vector3 riggingClosestOnTri( const Vector3 & p, const Vector3 & a, const Vector3 & b, const Vector3 & c, float bary[3] )
{
	Vector3 ab = b - a, ac = c - a, ap = p - a;
	if ( Vector3::crossproduct( ab, ac ).squaredLength() <= 1.0e-20f ) {
		float da = ( p - a ).squaredLength(), db = ( p - b ).squaredLength();
		float dc = ( p - c ).squaredLength();
		if ( da <= db && da <= dc ) { bary[0] = 1; bary[1] = 0; bary[2] = 0; return a; }
		if ( db <= dc ) { bary[0] = 0; bary[1] = 1; bary[2] = 0; return b; }
		bary[0] = 0; bary[1] = 0; bary[2] = 1; return c;
	}
	float d1 = Vector3::dotproduct( ab, ap ), d2 = Vector3::dotproduct( ac, ap );
	if ( d1 <= 0 && d2 <= 0 ) { bary[0] = 1; bary[1] = 0; bary[2] = 0; return a; }
	Vector3 bp = p - b; float d3 = Vector3::dotproduct( ab, bp ), d4 = Vector3::dotproduct( ac, bp );
	if ( d3 >= 0 && d4 <= d3 ) { bary[0] = 0; bary[1] = 1; bary[2] = 0; return b; }
	float vc = d1 * d4 - d3 * d2;
	if ( vc <= 0 && d1 >= 0 && d3 <= 0 ) { float v = d1 / ( d1 - d3 ); bary[0] = 1 - v; bary[1] = v; bary[2] = 0; return a + ab * v; }
	Vector3 cp = p - c; float d5 = Vector3::dotproduct( ab, cp ), d6 = Vector3::dotproduct( ac, cp );
	if ( d6 >= 0 && d5 <= d6 ) { bary[0] = 0; bary[1] = 0; bary[2] = 1; return c; }
	float vb = d5 * d2 - d1 * d6;
	if ( vb <= 0 && d2 >= 0 && d6 <= 0 ) { float w = d2 / ( d2 - d6 ); bary[0] = 1 - w; bary[1] = 0; bary[2] = w; return a + ac * w; }
	float va = d3 * d6 - d5 * d4;
	if ( va <= 0 && ( d4 - d3 ) >= 0 && ( d5 - d6 ) >= 0 ) { float w = ( d4 - d3 ) / ( ( d4 - d3 ) + ( d5 - d6 ) ); bary[0] = 0; bary[1] = 1 - w; bary[2] = w; return b + ( c - b ) * w; }
	float den = 1.0f / ( va + vb + vc ), v = vb * den, w = vc * den;
	bary[0] = 1 - v - w; bary[1] = v; bary[2] = w; return a + ab * v + ac * w;
}

//! Transfer donor skin onto target positions: per target vertex, closest point on the
//! nearest donor triangle, barycentric-blend the 3 donor verts' weights, top-4 + normalize.
static QVector<QVector<QPair<QString, float>>> riggingTransfer( const RigShape & donor,
	const QVector<Vector3> & targetPos, QVector<float> * outSnap = nullptr,
	const std::function<bool( int, int )> & progress = {}, bool * outCancelled = nullptr )
{
	if ( outCancelled )
		*outCancelled = false;
	QVector<QVector<int>> incident( donor.pos.size() );
	for ( int t = 0; t < donor.tris.size(); t++ ) {
		const Triangle & tr = donor.tris[t];
		incident[tr.v1()] << t; incident[tr.v2()] << t; incident[tr.v3()] << t;
	}

	QVector<QVector<QPair<QString, float>>> out;
	out.reserve( targetPos.size() );
	for ( int targetVertex = 0; targetVertex < targetPos.size(); targetVertex++ ) {
		if ( progress && ( targetVertex % 16 ) == 0
			&& !progress( targetVertex, targetPos.size() ) ) {
			if ( outCancelled )
				*outCancelled = true;
			return {};
		}
		const Vector3 & p = targetPos.at( targetVertex );
		int nv = 0; float best = 1e30f;
		for ( int i = 0; i < donor.pos.size(); i++ ) {
			float d = ( donor.pos[i] - p ).squaredLength();
			if ( d < best ) { best = d; nv = i; }
		}

		float bb = 1e30f, bbary[3] = { 1, 0, 0 };
		int bt = incident[nv].isEmpty() ? -1 : incident[nv].first();
		for ( int t : incident[nv] ) {
			const Triangle & tr = donor.tris[t];
			float bary[3];
			Vector3 cpt = riggingClosestOnTri( p, donor.pos[tr.v1()], donor.pos[tr.v2()], donor.pos[tr.v3()], bary );
			float d = ( cpt - p ).squaredLength();
			if ( d < bb ) { bb = d; bt = t; bbary[0] = bary[0]; bbary[1] = bary[1]; bbary[2] = bary[2]; }
		}

		std::map<QString, float> acc;
		if ( bt >= 0 ) {
			const Triangle & tr = donor.tris[bt];
			quint16 tv[3] = { tr.v1(), tr.v2(), tr.v3() };
			for ( int s = 0; s < 3; s++ )
				for ( const auto & pr : donor.skin[tv[s]] )
					acc[pr.first] += bbary[s] * pr.second;
		} else {
			// A valid but isolated donor vertex has no incident face. Preserve a
			// normalized result by falling back to that nearest vertex's skin.
			bb = best;
			for ( const auto & pr : donor.skin[nv] )
				acc[pr.first] += pr.second;
		}

		QVector<QPair<QString, float>> v;
		for ( const auto & kv : acc ) v << qMakePair( kv.first, kv.second );
		std::sort( v.begin(), v.end(), []( const QPair<QString, float> & a, const QPair<QString, float> & b ) { return a.second > b.second; } );
		if ( v.size() > 4 ) v.resize( 4 );
		float sum = 0; for ( const auto & pr : v ) sum += pr.second;
		if ( sum > 0 ) for ( auto & pr : v ) pr.second /= sum;
		out << v;
		if ( outSnap ) *outSnap << std::sqrt( bb );
	}
	if ( progress )
		progress( targetPos.size(), targetPos.size() );
	return out;
}

//! Run the expensive surface mapping with responsive application-modal progress.
//! Progress is immediate so cancellation remains available even for medium meshes;
//! the dialog closes automatically when the callback reaches the final vertex.
static QVector<QVector<QPair<QString, float>>> riggingTransferWithProgress( const RigShape & donor,
	const QVector<Vector3> & targetPos, const QString & title, QVector<float> * outSnap,
	bool * cancelled )
{
	QProgressDialog dialog( Spell::tr( "Mapping target vertices to the donor surface..." ),
		Spell::tr( "Cancel" ), 0, targetPos.size() );
	dialog.setWindowTitle( title );
	dialog.setWindowModality( Qt::ApplicationModal );
	dialog.setMinimumDuration( 0 );
	dialog.setAutoClose( true );
	dialog.setAutoReset( false );

	auto update = [&]( int vertex, int total ) {
		dialog.setLabelText( Spell::tr( "Mapping target vertex %1 of %2..." ).arg( vertex ).arg( total ) );
		dialog.setValue( vertex );
		QCoreApplication::processEvents( QEventLoop::AllEvents, 20 );
		return !dialog.wasCanceled();
	};
	return riggingTransfer( donor, targetPos, outSnap, update, cancelled );
}

struct RiggingSnapStats
{
	float median = 0.0f;
	float percentile95 = 0.0f;
	float maximum = 0.0f;
	float donorSpan = 0.0f;
};

//! Summarize closest-surface distances relative to the donor's bounding-box diagonal.
//! The relative measure remains meaningful across NIFs authored at different scales.
static RiggingSnapStats riggingSnapStats( const RigShape & donor, const QVector<float> & distances )
{
	RiggingSnapStats stats;
	if ( donor.pos.isEmpty() || distances.isEmpty() )
		return stats;

	Vector3 low = donor.pos.first(), high = donor.pos.first();
	for ( const Vector3 & position : donor.pos ) {
		for ( int axis = 0; axis < 3; axis++ ) {
			low[axis] = qMin( low[axis], position[axis] );
			high[axis] = qMax( high[axis], position[axis] );
		}
	}
	stats.donorSpan = ( high - low ).length();

	QVector<float> sorted = distances;
	std::sort( sorted.begin(), sorted.end() );
	stats.median = sorted.at( sorted.size() / 2 );
	stats.percentile95 = sorted.at( qBound( 0,
		int( std::ceil( 0.95 * double( sorted.size() - 1 ) ) ), sorted.size() - 1 ) );
	stats.maximum = sorted.last();
	return stats;
}

//! 0 = close, 1 = caution, 2 = poor. Thresholds are intentionally relative:
//! a few isolated vertices may be farther away without condemning an otherwise close mesh.
static int riggingSnapQuality( const RiggingSnapStats & stats )
{
	if ( stats.donorSpan <= std::numeric_limits<float>::epsilon() )
		return 2;
	float p95 = stats.percentile95 / stats.donorSpan;
	float maximum = stats.maximum / stats.donorSpan;
	if ( p95 <= 0.01f && maximum <= 0.05f )
		return 0;
	if ( p95 <= 0.03f && maximum <= 0.15f )
		return 1;
	return 2;
}

static QString riggingSnapSummary( const RiggingSnapStats & stats )
{
	float scale = stats.donorSpan > std::numeric_limits<float>::epsilon()
		? 100.0f / stats.donorSpan : 0.0f;
	QString quality = riggingSnapQuality( stats ) == 0 ? Spell::tr( "Close" )
		: ( riggingSnapQuality( stats ) == 1 ? Spell::tr( "Caution" ) : Spell::tr( "Poor" ) );
	return Spell::tr( "Surface snap: median %1 (%2%), 95th %3 (%4%), max %5 (%6%) of donor span\n"
		"Geometry match: %7" )
		.arg( stats.median, 0, 'g', 4 ).arg( stats.median * scale, 0, 'g', 3 )
		.arg( stats.percentile95, 0, 'g', 4 ).arg( stats.percentile95 * scale, 0, 'g', 3 )
		.arg( stats.maximum, 0, 'g', 4 ).arg( stats.maximum * scale, 0, 'g', 3 )
		.arg( quality );
}

// ---------------------------------------------------------------------------

//! List the bones a skinned shape is bound to.
class spRiggingListBones final : public Spell
{
public:
	QString name() const override final { return Spell::tr( "List Skin Bones" ); }
	QString page() const override final { return Spell::tr( "Rigging" ); }
	bool constant() const override final { return true; }

	bool isApplicable( const NifModel * nif, const QModelIndex & index ) override final
	{
		return riggingIsTriShape( nif, index );
	}

	QModelIndex cast( NifModel * nif, const QModelIndex & index ) override final
	{
		QStringList bones = riggingBoneNames( nif, index );
		if ( bones.isEmpty() ) {
			QMessageBox::information( nullptr, Spell::tr( "Skin Bones" ),
				Spell::tr( "This shape has no skin bones (no NiSkinInstance / BSSkin::Instance)." ) );
		} else {
			QStringList lines;
			for ( int i = 0; i < bones.size(); i++ )
				lines << QString( "%1: %2" ).arg( i ).arg( bones.at( i ).isEmpty() ? Spell::tr( "<unnamed>" ) : bones.at( i ) );
			QMessageBox::information( nullptr, Spell::tr( "Skin Bones" ),
				Spell::tr( "%1 skin bones:\n\n" ).arg( bones.size() ) + lines.join( QStringLiteral( "\n" ) ) );
		}
		return index;
	}
};

REGISTER_SPELL( spRiggingListBones )

//! Compare this shape's bones against a donor shape in another NIF.
//! The pre-transfer preview: what would be added / already matches / is unused.
class spRiggingCompareBones final : public Spell
{
public:
	QString name() const override final { return Spell::tr( "Compare Bones with Donor..." ); }
	QString page() const override final { return Spell::tr( "Rigging" ); }
	bool constant() const override final { return true; }

	bool isApplicable( const NifModel * nif, const QModelIndex & index ) override final
	{
		return riggingIsTriShape( nif, index );
	}

	QModelIndex cast( NifModel * nif, const QModelIndex & index ) override final
	{
		const QStringList targetList = riggingBoneNames( nif, index );

		QString fileName = riggingChooseDonorFile( nif );
		if ( fileName.isEmpty() )
			return index;

		NifModel donor;
		if ( !donor.loadFromFile( fileName ) ) {
			QMessageBox::warning( nullptr, name(), Spell::tr( "Could not load the donor NIF." ) );
			return index;
		}

		QStringList labels;
		QList<int> shapes = riggingSkinnedShapes( &donor, labels );
		if ( shapes.isEmpty() ) {
			QMessageBox::information( nullptr, name(), Spell::tr( "The donor NIF has no skinned shapes." ) );
			return index;
		}

		int donorShape = riggingChooseDonorShape( shapes, labels, name() );
		if ( donorShape < 0 )
			return index;

		const QStringList donorList = riggingBoneNames( &donor, donor.getBlockIndex( donorShape ) );

		QSet<QString> target( targetList.begin(), targetList.end() );
		QSet<QString> src( donorList.begin(), donorList.end() );
		target.remove( QString() );
		src.remove( QString() );

		QStringList shared, donorOnly, targetOnly;
		for ( const QString & b : src ) ( target.contains( b ) ? shared : donorOnly ) << b;
		for ( const QString & b : target ) if ( !src.contains( b ) ) targetOnly << b;
		shared.sort(); donorOnly.sort(); targetOnly.sort();

		auto section = [](const QString & head, const QStringList & l) {
			return QString( "%1 (%2):\n%3\n" ).arg( head ).arg( l.size() )
				.arg( l.isEmpty() ? QStringLiteral( "  —" ) : QStringLiteral( "  " ) + l.join( QStringLiteral( "\n  " ) ) );
		};

		QString msg = section( Spell::tr( "Shared" ), shared )
			+ QStringLiteral( "\n" ) + section( Spell::tr( "Donor only (would be added on transfer)" ), donorOnly )
			+ QStringLiteral( "\n" ) + section( Spell::tr( "Target only (unused by donor)" ), targetOnly );

		QMessageBox box( QMessageBox::Information, name(),
			Spell::tr( "Target %1 bones vs donor %2 bones." ).arg( target.size() ).arg( src.size() ) );
		box.setDetailedText( msg );
		box.exec();
		return index;
	}
};

REGISTER_SPELL( spRiggingCompareBones )

//! Compute (but do NOT write) a bone-weight transfer from a donor shape and report
//! statistics. Read-only dry run of the transfer core before the write path lands.
class spRiggingPreviewTransfer final : public Spell
{
public:
	QString name() const override final { return Spell::tr( "Preview Transfer from Donor..." ); }
	QString page() const override final { return Spell::tr( "Rigging" ); }
	bool constant() const override final { return true; }

	bool isApplicable( const NifModel * nif, const QModelIndex & index ) override final
	{
		return nif && nif->getBSVersion() == 130
			&& riggingIsTriShape( nif, index ) && nif->getIndex( index, "Vertex Data" ).isValid();
	}

	QModelIndex cast( NifModel * nif, const QModelIndex & index ) override final
	{
		RigShape target = riggingReadShape( nif, index );
		if ( target.pos.isEmpty() ) {
			QMessageBox::warning( nullptr, name(), Spell::tr( "Target is not a readable FO4 BSTriShape." ) );
			return index;
		}

		QString fileName = riggingChooseDonorFile( nif );
		if ( fileName.isEmpty() )
			return index;

		NifModel donorNif;
		if ( !donorNif.loadFromFile( fileName ) ) {
			QMessageBox::warning( nullptr, name(), Spell::tr( "Could not load the donor NIF." ) );
			return index;
		}
		if ( donorNif.getBSVersion() != nif->getBSVersion() ) {
			QMessageBox::warning( nullptr, name(), Spell::tr( "Donor and target use different BS versions." ) );
			return index;
		}

		QStringList labels;
		QList<int> shapes = riggingSkinnedShapes( &donorNif, labels );
		if ( shapes.isEmpty() ) {
			QMessageBox::information( nullptr, name(), Spell::tr( "The donor NIF has no skinned shapes." ) );
			return index;
		}
		int donorShape = riggingChooseDonorShape( shapes, labels, name() );
		if ( donorShape < 0 )
			return index;

		RigShape donor = riggingReadShape( &donorNif, donorNif.getBlockIndex( donorShape ) );
		if ( !donor.valid ) {
			QMessageBox::warning( nullptr, name(), Spell::tr( "Donor shape is not a readable FO4 BSTriShape." ) );
			return index;
		}
		Transform targetSpace, donorSpace;
		int targetRoot = nif->getLink( riggingSkinInstance( nif, index ), "Skeleton Root" );
		int donorRoot = donorNif.getLink( riggingSkinInstance( &donorNif, donorNif.getBlockIndex( donorShape ) ), "Skeleton Root" );
		if ( !riggingNodeWorld( nif, nif->getBlockNumber( index ), targetRoot, targetSpace )
			|| !riggingNodeWorld( &donorNif, donorShape, donorRoot, donorSpace )
			|| riggingTransformDelta( targetSpace, donorSpace ) > 0.001f ) {
			QMessageBox::warning( nullptr, name(),
				Spell::tr( "Donor and target shapes are not in the same skeleton-root-relative space. "
					"Transfer was not run." ) );
			return index;
		}

		QVector<float> snap;
		bool cancelled = false;
		auto result = riggingTransferWithProgress( donor, target.pos, name(), &snap, &cancelled );
		if ( cancelled ) {
			QMessageBox::information( nullptr, name(),
				Spell::tr( "Transfer preview cancelled. Nothing was changed." ) );
			return index;
		}

		// stats
		QSet<QString> usedBones;
		int histo[5] = { 0, 0, 0, 0, 0 };
		for ( const auto & v : result ) {
			histo[qMin( v.size(), 4 )]++;
			for ( const auto & pr : v ) usedBones << pr.first;
		}
		RiggingSnapStats snapStats = riggingSnapStats( donor, snap );

		QString msg = Spell::tr( "Target vertices: %1\nDonor: %2 verts, %3 tris, %4 bones\n\n"
			"Bones used by transfer: %5\nInfluences/vertex: 1→%6  2→%7  3→%8  4→%9\n"
			"%10\n\n(Preview only — nothing written.)" )
			.arg( target.pos.size() ).arg( donor.pos.size() ).arg( donor.tris.size() ).arg( donor.bones.size() )
			.arg( usedBones.size() )
			.arg( histo[1] ).arg( histo[2] ).arg( histo[3] ).arg( histo[4] )
			.arg( riggingSnapSummary( snapStats ) );
		QMessageBox::information( nullptr, name(), msg );
		return index;
	}
};

REGISTER_SPELL( spRiggingPreviewTransfer )

//! Transfer weights from a donor onto the target, writing into the target's EXISTING
//! bone slots. Influences to bones the target lacks are dropped + renormalized (run
//! "Compare Bones with Donor" first). No structural change — only per-vertex Bone
//! Weights/Indices are rewritten, so it's undoable and safe. Full bone-adding transfer
//! (new bone nodes + BSSkin::BoneData) is a separate, later action.
class spRiggingTransferWeights final : public Spell
{
public:
	QString name() const override final { return Spell::tr( "Transfer Weights (existing bones)..." ); }
	QString page() const override final { return Spell::tr( "Rigging" ); }
	bool undoable() const override final { return true; }

	bool isApplicable( const NifModel * nif, const QModelIndex & index ) override final
	{
		return nif && nif->getBSVersion() == 130
			&& riggingIsSkinnedShape( nif, index ) && nif->getIndex( index, "Vertex Data" ).isValid();
	}

	QModelIndex cast( NifModel * nif, const QModelIndex & index ) override final
	{
		RigShape target = riggingReadShape( nif, index );
		if ( target.pos.isEmpty() || target.bones.isEmpty() ) {
			QMessageBox::warning( nullptr, name(), Spell::tr( "Target is not a readable skinned FO4 BSTriShape." ) );
			return index;
		}
		QMap<QString, int> boneIndex;
		for ( int i = 0; i < target.bones.size(); i++ ) {
			if ( !target.bones.at( i ).isEmpty() ) {
				if ( boneIndex.contains( target.bones.at( i ) ) ) {
					QMessageBox::warning( nullptr, name(),
						Spell::tr( "Target has duplicate skin-bone name '%1'." ).arg( target.bones.at( i ) ) );
					return index;
				}
				boneIndex.insert( target.bones.at( i ), i );
			}
		}
		if ( target.bones.size() > 256 ) {
			QMessageBox::warning( nullptr, name(), Spell::tr( "Target exceeds the uint8 limit of 256 skin bones." ) );
			return index;
		}

		QString fileName = riggingChooseDonorFile( nif );
		if ( fileName.isEmpty() )
			return index;
		NifModel donorNif;
		if ( !donorNif.loadFromFile( fileName ) ) {
			QMessageBox::warning( nullptr, name(), Spell::tr( "Could not load the donor NIF." ) );
			return index;
		}
		if ( donorNif.getBSVersion() != nif->getBSVersion() ) {
			QMessageBox::warning( nullptr, name(), Spell::tr( "Donor and target use different BS versions." ) );
			return index;
		}
		QStringList labels;
		QList<int> shapes = riggingSkinnedShapes( &donorNif, labels );
		if ( shapes.isEmpty() ) {
			QMessageBox::information( nullptr, name(), Spell::tr( "The donor NIF has no skinned shapes." ) );
			return index;
		}
		int donorShape = riggingChooseDonorShape( shapes, labels, name() );
		if ( donorShape < 0 )
			return index;
		RigShape donor = riggingReadShape( &donorNif, donorNif.getBlockIndex( donorShape ) );
		if ( !donor.valid ) {
			QMessageBox::warning( nullptr, name(), Spell::tr( "Donor shape is not a readable FO4 BSTriShape." ) );
			return index;
		}
		Transform targetSpace, donorSpace;
		int targetRoot = nif->getLink( riggingSkinInstance( nif, index ), "Skeleton Root" );
		int donorRoot = donorNif.getLink( riggingSkinInstance( &donorNif, donorNif.getBlockIndex( donorShape ) ), "Skeleton Root" );
		if ( !riggingNodeWorld( nif, nif->getBlockNumber( index ), targetRoot, targetSpace )
			|| !riggingNodeWorld( &donorNif, donorShape, donorRoot, donorSpace )
			|| riggingTransformDelta( targetSpace, donorSpace ) > 0.001f ) {
			QMessageBox::warning( nullptr, name(),
				Spell::tr( "Donor and target shapes are not in the same skeleton-root-relative space. "
					"Weights were not changed." ) );
			return index;
		}

		QVector<QVector<QPair<QString, float>>> result;
		if ( riggingWorkflow.active
			&& riggingWorkflow.preparedWeights.size() == target.pos.size() ) {
			result = riggingWorkflow.preparedWeights;
		} else {
			bool cancelled = false;
			result = riggingTransferWithProgress( donor, target.pos, name(), nullptr, &cancelled );
			if ( cancelled ) {
				if ( !riggingWorkflow.active )
					QMessageBox::information( nullptr, name(),
						Spell::tr( "Weight transfer cancelled. Nothing was changed." ) );
				return index;
			}
		}

		QModelIndex iVD = nif->getIndex( index, "Vertex Data" );
		int nv = qMin<int>( nif->rowCount( iVD ), result.size() );
		for ( int i = 0; i < nv; i++ ) {
			QModelIndex iVertex = nif->getIndex( iVD, i );
			QModelIndex iW = nif->getIndex( iVertex, "Bone Weights" );
			QModelIndex iI = nif->getIndex( iVertex, "Bone Indices" );
			if ( !iW.isValid() || !iI.isValid()
				|| nif->rowCount( iW ) != 4 || nif->rowCount( iI ) != 4 ) {
				QMessageBox::warning( nullptr, name(),
					Spell::tr( "Target vertex %1 does not have exactly four skin slots." ).arg( i ) );
				return index;
			}
		}
		int droppedVerts = 0;
		bool boundsUpdated = false;
		bool changed = nifSnapshotOp( nif, Spell::tr( "Transfer weights onto %1 vertices" ).arg( nv ), [&]() {
			bool previousHold = nif->holdUpdates( true );
			for ( int i = 0; i < nv; i++ ) {
				QModelIndex iVertex = nif->getIndex( iVD, i );
				QModelIndex iW = nif->getIndex( iVertex, "Bone Weights" );
				QModelIndex iI = nif->getIndex( iVertex, "Bone Indices" );

				QVector<QPair<int, float>> keep;
				for ( const auto & pr : result.at( i ) ) {
					auto it = boneIndex.constFind( pr.first );
					if ( it != boneIndex.constEnd() )
						keep << qMakePair( it.value(), pr.second );
				}
				float sum = 0; for ( const auto & pr : keep ) sum += pr.second;
				if ( sum <= 0 ) { droppedVerts++; continue; }
				for ( auto & pr : keep ) pr.second /= sum;

				for ( int j = 0; j < 4; j++ ) {
					float w = ( j < keep.size() ) ? keep.at( j ).second : 0.0f;
					int bi = ( j < keep.size() ) ? keep.at( j ).first : 0;
					nif->set<float>( nif->getIndex( iW, j ), w );
					nif->set<quint8>( nif->getIndex( iI, j ), quint8( bi ) );
				}
			}
			boundsUpdated = spUpdateBounds::calculateBoneBounds( nif, QPersistentModelIndex( index ) );
			nif->holdUpdates( previousHold );
		} );
		if ( !changed ) {
			QMessageBox::warning( nullptr, name(), Spell::tr( "Could not create an undo snapshot for weight transfer." ) );
			return index;
		}

		QString warn;
		if ( droppedVerts )
			warn = Spell::tr( "\n\n%1 vertices had no matching target bone and were left unchanged "
				"(donor uses bones the target lacks — see \"Compare Bones with Donor\")." ).arg( droppedVerts );
		if ( !boundsUpdated )
			warn += Spell::tr( "\n\nBone bounds could not be recalculated; run Mesh > Update Bounds manually." );
		riggingWorkflowStepSucceeded();
		if ( !riggingWorkflow.active )
			QMessageBox::information( nullptr, name(),
				Spell::tr( "Transferred weights onto %1 vertices from donor \"%2\"." )
					.arg( nv ).arg( riggingDonorDisplayName( fileName ) ) + warn );
		return index;
	}
};

REGISTER_SPELL( spRiggingTransferWeights )

//! Largest component-wise difference between two skin-to-bone transforms.
static float riggingTransformDelta( const Transform & a, const Transform & b )
{
	if ( !std::isfinite( a.scale ) || !std::isfinite( b.scale ) )
		return std::numeric_limits<float>::infinity();
	float d = std::fabs( a.scale - b.scale );
	for ( int i = 0; i < 3; i++ ) {
		if ( !std::isfinite( a.translation[i] ) || !std::isfinite( b.translation[i] ) )
			return std::numeric_limits<float>::infinity();
		d = qMax( d, std::fabs( a.translation[i] - b.translation[i] ) );
		for ( int j = 0; j < 3; j++ ) {
			if ( !std::isfinite( a.rotation( i, j ) ) || !std::isfinite( b.rotation( i, j ) ) )
				return std::numeric_limits<float>::infinity();
			d = qMax( d, std::fabs( a.rotation( i, j ) - b.rotation( i, j ) ) );
		}
	}
	return d;
}

//! Scene-graph parent through a NiNode Children array (not merely any Ref).
static int riggingSceneParent( const NifModel * nif, int block )
{
	int found = -1;
	for ( int parent = 0; parent < nif->getBlockCount(); parent++ ) {
		QModelIndex iParent = nif->getBlockIndex( parent );
		if ( !nif->blockInherits( iParent, "NiNode" )
			|| !nif->getLinkArray( iParent, "Children" ).contains( block ) )
			continue;
		if ( found >= 0 )
			return -2; // multiple scene parents are ambiguous
		found = parent;
	}
	return found;
}

//! Collect only NiNode descendants reached through scene Children links.
static void riggingCollectNodeTree( const NifModel * nif, int block, QSet<int> & out )
{
	if ( !nif->isValidBlockNumber( block ) || out.contains( block ) )
		return;
	QModelIndex iNode = nif->getBlockIndex( block );
	if ( !nif->blockInherits( iNode, "NiNode" ) )
		return;
	out << block;
	for ( int child : nif->getLinkArray( iNode, "Children" ) )
		riggingCollectNodeTree( nif, child, out );
}

//! Node transform accumulated from a declared skeleton root to a descendant.
static bool riggingNodeWorld( const NifModel * nif, int node, int root, Transform & world )
{
	QVector<int> chain;
	QSet<int> seen;
	int current = node;
	while ( nif->isValidBlockNumber( current ) && !seen.contains( current ) ) {
		seen << current;
		if ( !nif->blockInherits( nif->getBlockIndex( current ), "NiAVObject" ) )
			return false;
		chain << current;
		if ( current == root )
			break;
		current = riggingSceneParent( nif, current );
	}
	if ( chain.isEmpty() || chain.last() != root )
		return false;

	world = Transform();
	// Exclude the root's own local transform: compare skeleton-root-relative poses.
	for ( int i = chain.size() - 2; i >= 0; i-- )
		world = world * Transform( nif, nif->getBlockIndex( chain[i] ) );
	return true;
}

//! Full authored transform from model space to a scene node. Unlike
//! riggingNodeWorld(), this includes the top-level node and is used only to
//! place copied donor preview geometry over the rendered target shape.
static bool riggingNodeAbsoluteWorld( const NifModel * nif, int node, Transform & world )
{
	QVector<int> chain;
	QSet<int> seen;
	int current = node;
	while ( nif->isValidBlockNumber( current ) && !seen.contains( current ) ) {
		seen << current;
		if ( !nif->blockInherits( nif->getBlockIndex( current ), "NiAVObject" ) )
			return false;
		chain << current;
		int parent = riggingSceneParent( nif, current );
		if ( parent == -2 )
			return false;
		current = parent;
	}
	if ( chain.isEmpty() )
		return false;
	world = Transform();
	for ( int i = chain.size() - 1; i >= 0; i-- )
		world = world * Transform( nif, nif->getBlockIndex( chain[i] ) );
	return true;
}

//! Import only the donor NiNodes needed by its nonzero skin weights. This is a
//! deliberately minimal scene-graph merge: no branch serialization, controllers,
//! collision, extra data, sibling geometry, or unrelated descendants.
class spRiggingImportBoneNodes final : public Spell
{
public:
	QString name() const override final { return Spell::tr( "Import Donor Bone Nodes..." ); }
	QString page() const override final { return Spell::tr( "Rigging" ); }
	bool undoable() const override final { return true; }

	bool isApplicable( const NifModel * nif, const QModelIndex & index ) override final
	{
		return nif && nif->getBSVersion() == 130
			&& riggingIsSkinnedShape( nif, index )
			&& nif->isNiBlock( riggingSkinInstance( nif, index ), "BSSkin::Instance" );
	}

	QModelIndex cast( NifModel * nif, const QModelIndex & index ) override final
	{
		QString fileName = riggingChooseDonorFile( nif );
		if ( fileName.isEmpty() )
			return index;

		NifModel donor;
		if ( !donor.loadFromFile( fileName ) ) {
			QMessageBox::warning( nullptr, name(), Spell::tr( "Could not load the donor NIF." ) );
			return index;
		}
		if ( donor.getBSVersion() != nif->getBSVersion() ) {
			QMessageBox::warning( nullptr, name(), Spell::tr( "Donor and target use different BS versions." ) );
			return index;
		}

		QStringList labels;
		QList<int> shapes = riggingSkinnedShapes( &donor, labels );
		if ( shapes.isEmpty() ) {
			QMessageBox::information( nullptr, name(), Spell::tr( "The donor NIF has no skinned shapes." ) );
			return index;
		}
		int donorShape = riggingChooseDonorShape( shapes, labels, name() );
		if ( donorShape < 0 )
			return index;

		QModelIndex iDonorShape = donor.getBlockIndex( donorShape );
		QModelIndex iDonorSkin = riggingSkinInstance( &donor, iDonorShape );
		QModelIndex iDonorBones = donor.getIndex( iDonorSkin, "Bones" );
		QModelIndex iTargetSkin = riggingSkinInstance( nif, index );
		int donorRoot = donor.getLink( iDonorSkin, "Skeleton Root" );
		int targetRoot = nif->getLink( iTargetSkin, "Skeleton Root" );
		QSet<int> donorTree, targetTree;
		riggingCollectNodeTree( &donor, donorRoot, donorTree );
		riggingCollectNodeTree( nif, targetRoot, targetTree );
		if ( donorTree.isEmpty() || targetTree.isEmpty()
			|| !donor.blockInherits( donor.getBlockIndex( donorRoot ), "NiNode" )
			|| !nif->blockInherits( nif->getBlockIndex( targetRoot ), "NiNode" )
			|| !iDonorBones.isValid() ) {
			QMessageBox::warning( nullptr, name(), Spell::tr( "Donor or target has no readable NiNode skeleton root/tree." ) );
			return index;
		}

		int donorBoneRows = donor.rowCount( iDonorBones );
		if ( donor.get<quint32>( iDonorSkin, "Num Bones" ) != quint32( donorBoneRows )
			|| donorBoneRows > 256 ) {
			QMessageBox::warning( nullptr, name(), Spell::tr( "Donor Num Bones is inconsistent or exceeds 256." ) );
			return index;
		}

		// Preserve slot identity: duplicate names must not collapse before they are
		// diagnosed, and only slots with a positive representable weight are needed.
		QSet<int> usedSlots;
		QModelIndex iVD = donor.getIndex( iDonorShape, "Vertex Data" );
		for ( int v = 0; v < donor.rowCount( iVD ); v++ ) {
			QModelIndex iVertex = donor.getIndex( iVD, v );
			QModelIndex iW = donor.getIndex( iVertex, "Bone Weights" );
			QModelIndex iI = donor.getIndex( iVertex, "Bone Indices" );
			if ( !iW.isValid() || !iI.isValid() || donor.rowCount( iW ) != donor.rowCount( iI ) )
				continue;
			for ( int s = 0; s < donor.rowCount( iW ); s++ ) {
				float w = donor.get<float>( donor.getIndex( iW, s ) );
				int slot = donor.get<quint8>( donor.getIndex( iI, s ) );
				if ( std::isfinite( w ) && w > 0.0f && slot >= 0 && slot < donorBoneRows )
					usedSlots << slot;
			}
		}
		if ( usedSlots.isEmpty() ) {
			QMessageBox::information( nullptr, name(), Spell::tr( "The donor shape has no positive skin weights." ) );
			return index;
		}

		QMap<QString, QVector<int>> targetByName;
		QMap<QString, QSet<QString>> targetByFoldedName;
		for ( int node : targetTree ) {
			QString nodeName = nif->get<QString>( nif->getBlockIndex( node ), "Name" );
			if ( nodeName.isEmpty() )
				continue;
			targetByName[nodeName] << node;
			targetByFoldedName[nodeName.toCaseFolded()] << nodeName;
		}

		QMap<int, int> sourceToTarget;
		sourceToTarget.insert( donorRoot, targetRoot ); // roots map by role, not name
		QSet<int> plannedSet;
		QVector<int> plan; // parent-before-child donor block numbers
		QMap<QString, int> requiredNames;
		QMap<QString, QString> requiredFoldedNames;
		QSet<QString> usedBoneNames;
		QSet<QString> targetBoundNames;
		for ( const QString & targetBone : riggingBoneNames( nif, index ) )
			if ( !targetBone.isEmpty() ) targetBoundNames << targetBone;
		int missingBindings = 0;
		QString preflightError;

		QList<int> orderedSlots( usedSlots.begin(), usedSlots.end() );
		std::sort( orderedSlots.begin(), orderedSlots.end() );
		for ( int slot : orderedSlots ) {
			int sourceBone = donor.getLink( donor.getIndex( iDonorBones, slot ) );
			QModelIndex iSourceBone = donor.getBlockIndex( sourceBone );
			QString boneName = donor.get<QString>( iSourceBone, "Name" );
			if ( !donorTree.contains( sourceBone ) || !donor.blockInherits( iSourceBone, "NiNode" )
				|| boneName.isEmpty() ) {
				preflightError = Spell::tr( "Used donor bone slot %1 is invalid, unnamed, or outside Skeleton Root." ).arg( slot );
				break;
			}
			if ( usedBoneNames.contains( boneName ) ) {
				preflightError = Spell::tr( "Used donor bones contain duplicate name '%1'." ).arg( boneName );
				break;
			}
			usedBoneNames << boneName;
			QString foldedBoneName = boneName.toCaseFolded();
			if ( requiredFoldedNames.contains( foldedBoneName )
				&& requiredFoldedNames.value( foldedBoneName ) != boneName ) {
				preflightError = Spell::tr( "Used donor bones contain case-colliding names '%1' and '%2'." )
					.arg( requiredFoldedNames.value( foldedBoneName ), boneName );
				break;
			}
			requiredNames.insert( boneName, sourceBone );
			requiredFoldedNames.insert( foldedBoneName, boneName );
			if ( !targetBoundNames.contains( boneName ) )
				missingBindings++;

			QVector<int> upward;
			QSet<int> seen;
			int current = sourceBone;
			while ( !sourceToTarget.contains( current ) ) {
				if ( seen.contains( current ) ) {
					preflightError = Spell::tr( "Cycle detected in donor bone hierarchy at '%1'." ).arg( boneName );
					break;
				}
				seen << current;
				QModelIndex iSource = donor.getBlockIndex( current );
				if ( !donorTree.contains( current ) || donor.itemName( iSource ) != QStringLiteral( "NiNode" ) ) {
					preflightError = Spell::tr( "Bone path for '%1' contains an unsupported node subtype or leaves Skeleton Root." ).arg( boneName );
					break;
				}
				QString sourceName = donor.get<QString>( iSource, "Name" );
				if ( sourceName.isEmpty() ) {
					preflightError = Spell::tr( "Bone path for '%1' contains an unnamed node." ).arg( boneName );
					break;
				}

				const QVector<int> matches = targetByName.value( sourceName );
				if ( matches.size() > 1 ) {
					preflightError = Spell::tr( "Target skeleton contains multiple nodes named '%1'." ).arg( sourceName );
					break;
				}
				if ( matches.size() == 1 ) {
					Transform donorPose, targetPose;
					if ( !riggingNodeWorld( &donor, current, donorRoot, donorPose )
						|| !riggingNodeWorld( nif, matches.first(), targetRoot, targetPose )
						|| riggingTransformDelta( donorPose, targetPose ) > 0.001f ) {
						preflightError = Spell::tr( "Existing target node '%1' has an incompatible root-relative rest pose." ).arg( sourceName );
						break;
					}
					sourceToTarget.insert( current, matches.first() );
					break;
				}
				QSet<QString> foldedMatches = targetByFoldedName.value( sourceName.toCaseFolded() );
				if ( !foldedMatches.isEmpty() ) {
					preflightError = Spell::tr( "Target contains a case-only name collision for '%1'." ).arg( sourceName );
					break;
				}

				Transform local( &donor, iSource );
				if ( !std::isfinite( local.scale ) || std::fabs( local.scale ) <= 1.0e-8f
					|| !std::isfinite( riggingTransformDelta( local, Transform() ) ) ) {
					preflightError = Spell::tr( "Donor node '%1' has an invalid transform." ).arg( sourceName );
					break;
				}
				upward << current;
				int parent = riggingSceneParent( &donor, current );
				if ( parent == -2 ) {
					preflightError = Spell::tr( "Donor node '%1' has multiple scene parents." ).arg( sourceName );
					break;
				}
				if ( parent < 0 ) {
					preflightError = Spell::tr( "Donor bone path for '%1' is detached from Skeleton Root." ).arg( boneName );
					break;
				}
				current = parent;
			}
			if ( !preflightError.isEmpty() )
				break;

			for ( int i = upward.size() - 1; i >= 0; i-- ) {
				int source = upward[i];
				if ( plannedSet.contains( source ) )
					continue;
				QString sourceName = donor.get<QString>( donor.getBlockIndex( source ), "Name" );
				if ( requiredNames.contains( sourceName ) && requiredNames.value( sourceName ) != source ) {
					preflightError = Spell::tr( "Planned donor hierarchy contains duplicate name '%1'." ).arg( sourceName );
					break;
				}
				QString foldedSourceName = sourceName.toCaseFolded();
				if ( requiredFoldedNames.contains( foldedSourceName )
					&& requiredFoldedNames.value( foldedSourceName ) != sourceName ) {
					preflightError = Spell::tr( "Planned donor hierarchy contains case-colliding names '%1' and '%2'." )
						.arg( requiredFoldedNames.value( foldedSourceName ), sourceName );
					break;
				}
				requiredNames.insert( sourceName, source );
				requiredFoldedNames.insert( foldedSourceName, sourceName );
				plannedSet << source;
				plan << source;
			}
			if ( !preflightError.isEmpty() )
				break;
		}

		if ( !preflightError.isEmpty() ) {
			QMessageBox::warning( nullptr, name(), preflightError + Spell::tr( "\n\nNo nodes were imported." ) );
			return index;
		}
		int targetBoundCount = riggingBoneNames( nif, index ).size();
		if ( targetBoundCount + missingBindings > 256 ) {
			QMessageBox::warning( nullptr, name(),
				Spell::tr( "Import would prepare %1 new bindings, exceeding the 256-bone limit (%2 existing)." )
					.arg( missingBindings ).arg( targetBoundCount ) );
			return index;
		}
		if ( plan.isEmpty() ) {
			riggingWorkflowStepSucceeded();
			if ( !riggingWorkflow.active )
				QMessageBox::information( nullptr, name(),
					Spell::tr( "Every donor bone node used by this shape already exists compatibly under the target Skeleton Root." ) );
			return index;
		}

		QStringList planNames;
		for ( int source : plan )
			planNames << donor.get<QString>( donor.getBlockIndex( source ), "Name" );
		if ( !riggingWorkflow.active && QMessageBox::question( nullptr, name(),
			Spell::tr( "Import %1 minimal NiNode(s)?\n\n%2\n\n"
				"Only Name, Flags, and Transform are copied." )
				.arg( plan.size() ).arg( planNames.join( QStringLiteral( "\n" ) ) ) ) != QMessageBox::Yes )
			return index;

		bool imported = true;
		bool snapshot = nifSnapshotOp( nif, Spell::tr( "Import %1 donor bone node(s)" ).arg( plan.size() ), [&]() {
			bool previousHold = nif->holdUpdates( true );
			for ( int source : plan ) {
				int sourceParent = riggingSceneParent( &donor, source );
				if ( !sourceToTarget.contains( sourceParent ) ) {
					imported = false;
					break;
				}
				QModelIndex iSource = donor.getBlockIndex( source );
				QModelIndex iNew = nif->insertNiBlock( "NiNode", -1 );
				if ( !iNew.isValid() ) {
					imported = false;
					break;
				}
				nif->assignString( iNew, "Name", donor.get<QString>( iSource, "Name" ), false );
				nif->set<quint32>( iNew, "Flags", donor.get<quint32>( iSource, "Flags" ) );
				Transform( &donor, iSource ).writeBack( nif, iNew );
				int newBlock = nif->getBlockNumber( iNew );
				if ( !addLink( nif, nif->getBlockIndex( sourceToTarget.value( sourceParent ) ),
					"Children", newBlock ) ) {
					imported = false;
					break;
				}
				sourceToTarget.insert( source, newBlock );
			}
			nif->holdUpdates( previousHold );
		} );
		if ( !snapshot || !imported ) {
			QMessageBox::warning( nullptr, name(),
				Spell::tr( "Node import could not be completed. Use Undo if any nodes were created." ) );
			return index;
		}

		riggingWorkflowStepSucceeded();
		if ( !riggingWorkflow.active )
			QMessageBox::information( nullptr, name(),
				Spell::tr( "Imported %1 minimal donor bone node(s). Next run Bind Donor Bones (existing nodes)." )
					.arg( plan.size() ) );
		return index;
	}
};

REGISTER_SPELL( spRiggingImportBoneNodes )

//! Extend a target BSSkin binding with donor bones whose NiNodes already exist in
//! the target file. BoneData is copied only when shared bones prove that donor and
//! target use the same bind space. Run Import Donor Bone Nodes first when needed.
class spRiggingBindExistingNodes final : public Spell
{
public:
	QString name() const override final { return Spell::tr( "Bind Donor Bones (existing nodes)..." ); }
	QString page() const override final { return Spell::tr( "Rigging" ); }
	bool undoable() const override final { return true; }

	bool isApplicable( const NifModel * nif, const QModelIndex & index ) override final
	{
		return nif && nif->getBSVersion() == 130
			&& riggingIsSkinnedShape( nif, index )
			&& nif->isNiBlock( riggingSkinInstance( nif, index ), "BSSkin::Instance" );
	}

	QModelIndex cast( NifModel * nif, const QModelIndex & index ) override final
	{
		QString fileName = riggingChooseDonorFile( nif );
		if ( fileName.isEmpty() )
			return index;

		NifModel donor;
		if ( !donor.loadFromFile( fileName ) ) {
			QMessageBox::warning( nullptr, name(), Spell::tr( "Could not load the donor NIF." ) );
			return index;
		}
		if ( donor.getBSVersion() != nif->getBSVersion() ) {
			QMessageBox::warning( nullptr, name(), Spell::tr( "Donor and target use different BS versions." ) );
			return index;
		}

		QStringList labels;
		QList<int> shapes = riggingSkinnedShapes( &donor, labels );
		if ( shapes.isEmpty() ) {
			QMessageBox::information( nullptr, name(), Spell::tr( "The donor NIF has no skinned shapes." ) );
			return index;
		}
		int donorShape = riggingChooseDonorShape( shapes, labels, name() );
		if ( donorShape < 0 )
			return index;

		QModelIndex iDonorShape = donor.getBlockIndex( donorShape );
		RigShape donorShapeData = riggingReadShape( &donor, iDonorShape );
		QSet<QString> usedDonorBones;
		for ( const auto & skin : donorShapeData.skin )
			for ( const auto & influence : skin )
				usedDonorBones << influence.first;

		QModelIndex iTargetSkin = riggingSkinInstance( nif, index );
		QModelIndex iDonorSkin = riggingSkinInstance( &donor, iDonorShape );
		QModelIndex iTargetBones = nif->getIndex( iTargetSkin, "Bones" );
		QModelIndex iDonorBones = donor.getIndex( iDonorSkin, "Bones" );
		QModelIndex iTargetData = nif->getBlockIndex( nif->getLink( iTargetSkin, "Data" ) );
		QModelIndex iDonorData = donor.getBlockIndex( donor.getLink( iDonorSkin, "Data" ) );
		QModelIndex iTargetList = nif->getIndex( iTargetData, "Bone List" );
		QModelIndex iDonorList = donor.getIndex( iDonorData, "Bone List" );
		if ( !nif->isNiBlock( iTargetData, "BSSkin::BoneData" )
			|| !donor.isNiBlock( iDonorData, "BSSkin::BoneData" )
			|| !iTargetBones.isValid() || !iDonorBones.isValid()
			|| !iTargetList.isValid() || !iDonorList.isValid() ) {
			QMessageBox::warning( nullptr, name(), Spell::tr( "Donor or target has incomplete BSSkin data." ) );
			return index;
		}
		int targetBoneRows = nif->rowCount( iTargetBones );
		int donorBoneRows = donor.rowCount( iDonorBones );
		int targetDataRows = nif->rowCount( iTargetList );
		int donorDataRows = donor.rowCount( iDonorList );
		if ( nif->get<quint32>( iTargetSkin, "Num Bones" ) != quint32( targetBoneRows )
			|| nif->get<quint32>( iTargetData, "Num Bones" ) != quint32( targetDataRows )
			|| targetBoneRows != targetDataRows
			|| donor.get<quint32>( iDonorSkin, "Num Bones" ) != quint32( donorBoneRows )
			|| donor.get<quint32>( iDonorData, "Num Bones" ) != quint32( donorDataRows )
			|| donorBoneRows != donorDataRows ) {
			QMessageBox::warning( nullptr, name(),
				Spell::tr( "Donor or target bone counts do not match their arrays/BoneData. "
					"Run Validate FO4 Skin and repair the file first." ) );
			return index;
		}
		if ( targetBoneRows > 256 || donorBoneRows > 256
			|| nif->get<quint32>( iTargetSkin, "Num Scales" ) != 0
			|| donor.get<quint32>( iDonorSkin, "Num Scales" ) != 0 ) {
			QMessageBox::warning( nullptr, name(),
				Spell::tr( "The donor or target exceeds 256 bones or uses unsupported BSSkin Scales." ) );
			return index;
		}
		for ( int i = 0; i < targetBoneRows; i++ ) {
			if ( !nif->blockInherits( nif->getBlockIndex( nif->getLink( nif->getIndex( iTargetBones, i ) ) ), "NiNode" ) ) {
				QMessageBox::warning( nullptr, name(), Spell::tr( "Target contains an invalid/non-NiNode bone link." ) );
				return index;
			}
		}
		for ( int i = 0; i < donorBoneRows; i++ ) {
			if ( !donor.blockInherits( donor.getBlockIndex( donor.getLink( donor.getIndex( iDonorBones, i ) ) ), "NiNode" ) ) {
				QMessageBox::warning( nullptr, name(), Spell::tr( "Donor contains an invalid/non-NiNode bone link." ) );
				return index;
			}
		}
		int targetSkinOwners = 0, targetDataOwners = 0;
		int targetSkinBlock = nif->getBlockNumber( iTargetSkin );
		int targetDataBlock = nif->getBlockNumber( iTargetData );
		for ( int b = 0; b < nif->getBlockCount(); b++ ) {
			QModelIndex iBlock = nif->getBlockIndex( b );
			if ( riggingIsTriShape( nif, iBlock ) && nif->getLink( iBlock, "Skin" ) == targetSkinBlock )
				targetSkinOwners++;
			if ( nif->isNiBlock( iBlock, "BSSkin::Instance" ) && nif->getLink( iBlock, "Data" ) == targetDataBlock )
				targetDataOwners++;
		}
		if ( targetSkinOwners != 1 || targetDataOwners != 1 ) {
			QMessageBox::warning( nullptr, name(),
				Spell::tr( "Target Skin or BoneData is shared; binding would also alter another shape." ) );
			return index;
		}

		QStringList targetNames = riggingBoneNames( nif, index );
		QStringList donorNames = riggingBoneNames( &donor, iDonorShape );
		QMap<QString, int> targetSlot, donorSlot;
		QStringList duplicateSkinNames;
		for ( int i = 0; i < targetNames.size(); i++ ) {
			if ( targetNames[i].isEmpty() )
				continue;
			if ( targetSlot.contains( targetNames[i] ) ) duplicateSkinNames << targetNames[i];
			else targetSlot.insert( targetNames[i], i );
		}
		for ( int i = 0; i < donorNames.size(); i++ ) {
			if ( donorNames[i].isEmpty() )
				continue;
			if ( donorSlot.contains( donorNames[i] ) ) duplicateSkinNames << donorNames[i];
			else donorSlot.insert( donorNames[i], i );
		}
		if ( !duplicateSkinNames.isEmpty() ) {
			duplicateSkinNames.removeDuplicates();
			duplicateSkinNames.sort();
			QMessageBox::warning( nullptr, name(),
				Spell::tr( "Duplicate names make bone matching ambiguous:\n%1" )
					.arg( duplicateSkinNames.join( QStringLiteral( "\n" ) ) ) );
			return index;
		}

		// Shared BoneData must agree. If it does, donor records are valid in the
		// target bind space too; shared node poses must agree as well.
		int targetRoot = nif->getLink( iTargetSkin, "Skeleton Root" );
		int donorRoot = donor.getLink( iDonorSkin, "Skeleton Root" );
		int compared = 0;
		float maxDelta = 0.0f, maxPoseDelta = 0.0f;
		for ( auto it = donorSlot.constBegin(); it != donorSlot.constEnd(); ++it ) {
			auto targetIt = targetSlot.constFind( it.key() );
			if ( targetIt == targetSlot.constEnd()
				|| it.value() >= donor.rowCount( iDonorList )
				|| targetIt.value() >= nif->rowCount( iTargetList ) )
				continue;
			Transform a( &donor, donor.getIndex( iDonorList, it.value() ) );
			Transform b( nif, nif->getIndex( iTargetList, targetIt.value() ) );
			maxDelta = qMax( maxDelta, riggingTransformDelta( a, b ) );
			int donorNode = donor.getLink( donor.getIndex( iDonorBones, it.value() ) );
			int targetNode = nif->getLink( nif->getIndex( iTargetBones, targetIt.value() ) );
			Transform donorPose, targetPose;
			if ( !riggingNodeWorld( &donor, donorNode, donorRoot, donorPose )
				|| !riggingNodeWorld( nif, targetNode, targetRoot, targetPose ) )
				maxPoseDelta = std::numeric_limits<float>::infinity();
			else
				maxPoseDelta = qMax( maxPoseDelta, riggingTransformDelta( donorPose, targetPose ) );
			compared++;
		}
		if ( compared == 0 || maxDelta > 0.001f || maxPoseDelta > 0.001f ) {
			QMessageBox::warning( nullptr, name(), compared == 0
				? Spell::tr( "No shared BoneData entry exists to verify a compatible bind space." )
				: Spell::tr( "Donor and target shared bindings differ (BoneData delta %1, pose delta %2). "
					"Binding records were not copied." )
					.arg( maxDelta, 0, 'g', 4 ).arg( maxPoseDelta, 0, 'g', 4 ) );
			return index;
		}

		QSet<int> targetTree, donorTree;
		riggingCollectNodeTree( nif, targetRoot, targetTree );
		riggingCollectNodeTree( &donor, donorRoot, donorTree );
		if ( targetTree.isEmpty() || donorTree.isEmpty() ) {
			QMessageBox::warning( nullptr, name(), Spell::tr( "Donor or target has no readable skeleton-root node tree." ) );
			return index;
		}

		// Target NiNodes by name, constrained to this skin's Skeleton Root tree.
		// Duplicate names are deliberately ineligible for automatic binding.
		QMap<QString, int> targetNodes;
		QSet<QString> ambiguousNodes;
		for ( int b : targetTree ) {
			QModelIndex iNode = nif->getBlockIndex( b );
			QString nodeName = nif->get<QString>( iNode, "Name" );
			if ( nodeName.isEmpty() )
				continue;
			if ( targetNodes.contains( nodeName ) )
				ambiguousNodes << nodeName;
			else
				targetNodes.insert( nodeName, b );
		}

		QStringList addNames, unavailable, incompatiblePose;
		for ( const QString & bone : usedDonorBones ) {
			if ( targetSlot.contains( bone ) )
				continue;
			if ( !donorSlot.contains( bone ) || !targetNodes.contains( bone ) || ambiguousNodes.contains( bone ) )
				unavailable << bone;
			else {
				int donorIndex = donorSlot.value( bone );
				int donorNode = donor.getLink( donor.getIndex( iDonorBones, donorIndex ) );
				Transform donorWorld, targetWorld;
				if ( !donorTree.contains( donorNode )
					|| !riggingNodeWorld( &donor, donorNode, donorRoot, donorWorld )
					|| !riggingNodeWorld( nif, targetNodes.value( bone ), targetRoot, targetWorld )
					|| riggingTransformDelta( donorWorld, targetWorld ) > 0.001f )
					incompatiblePose << bone;
				else
					addNames << bone;
			}
		}
		addNames.sort();
		unavailable.sort();
		incompatiblePose.sort();
		if ( !incompatiblePose.isEmpty() ) {
			QMessageBox::warning( nullptr, name(),
				Spell::tr( "Matching target nodes have a different skeleton-root-relative rest pose:\n%1\n\n"
					"No bindings were changed." ).arg( incompatiblePose.join( QStringLiteral( "\n" ) ) ) );
			return index;
		}
		if ( riggingWorkflow.active && !unavailable.isEmpty() ) {
			QMessageBox::warning( nullptr, name(),
				Spell::tr( "The combined transfer requires an unambiguous target NiNode for every used donor bone.\n\n"
					"Missing nodes:\n%1" ).arg( unavailable.join( QStringLiteral( "\n" ) ) ) );
			return index;
		}
		if ( addNames.isEmpty() ) {
			if ( unavailable.isEmpty() )
				riggingWorkflowStepSucceeded();
			if ( !riggingWorkflow.active || !unavailable.isEmpty() )
				QMessageBox::information( nullptr, name(), unavailable.isEmpty()
					? Spell::tr( "All donor bones used by the mesh are already bound to the target." )
					: Spell::tr( "No missing donor bone has an unambiguous matching NiNode in the target.\n\nMissing nodes:\n%1" )
						.arg( unavailable.join( QStringLiteral( "\n" ) ) ) );
			return index;
		}
		int oldCount = nif->rowCount( iTargetBones );
		if ( oldCount + addNames.size() > 256 ) {
			QMessageBox::warning( nullptr, name(), Spell::tr( "Binding these bones would exceed the uint8 limit of 256 bones." ) );
			return index;
		}

		QString confirm = Spell::tr( "Bind %1 donor bone(s) whose NiNodes already exist in the target?\n\n%2" )
			.arg( addNames.size() ).arg( addNames.join( QStringLiteral( "\n" ) ) );
		if ( !unavailable.isEmpty() )
			confirm += Spell::tr( "\n\n%1 additional donor bone(s) have no unique target NiNode and will remain unbound." )
				.arg( unavailable.size() );
		if ( !riggingWorkflow.active && QMessageBox::question( nullptr, name(), confirm ) != QMessageBox::Yes )
			return index;

		int newCount = oldCount + addNames.size();
		bool changed = nifSnapshotOp( nif, Spell::tr( "Bind %1 donor bone(s)" ).arg( addNames.size() ), [&]() {
			QPersistentModelIndex pSkin = iTargetSkin;
			QPersistentModelIndex pData = iTargetData;
			bool previousHold = nif->holdUpdates( true );
			nif->set<quint32>( pSkin, "Num Bones", quint32( newCount ) );
			iTargetBones = nif->getIndex( pSkin, "Bones" );
			nif->updateArraySize( iTargetBones );
			nif->set<quint32>( pData, "Num Bones", quint32( newCount ) );
			iTargetList = nif->getIndex( pData, "Bone List" );
			nif->updateArraySize( iTargetList );

			for ( int n = 0; n < addNames.size(); n++ ) {
				const QString & bone = addNames[n];
				int targetIndex = oldCount + n;
				int sourceIndex = donorSlot.value( bone );
				nif->setLink( nif->getIndex( iTargetBones, targetIndex ), targetNodes.value( bone ) );

				QModelIndex sourceEntry = donor.getIndex( iDonorList, sourceIndex );
				QModelIndex targetEntry = nif->getIndex( iTargetList, targetIndex );
				QModelIndex sourceSphere = donor.getIndex( sourceEntry, "Bounding Sphere" );
				QModelIndex targetSphere = nif->getIndex( targetEntry, "Bounding Sphere" );
				nif->set<Vector3>( targetSphere, "Center", donor.get<Vector3>( sourceSphere, "Center" ) );
				nif->set<float>( targetSphere, "Radius", donor.get<float>( sourceSphere, "Radius" ) );
				Transform sourceTransform( &donor, sourceEntry );
				sourceTransform.writeBack( nif, targetEntry );
			}
			nif->holdUpdates( previousHold );
		} );
		if ( !changed ) {
			QMessageBox::warning( nullptr, name(), Spell::tr( "Could not create an undo snapshot for the binding change." ) );
			return index;
		}

		riggingWorkflowStepSucceeded();
		if ( !riggingWorkflow.active )
			QMessageBox::information( nullptr, name(),
				Spell::tr( "Bound %1 donor bone(s). Run Validate FO4 Skin, then Transfer Weights (existing bones)." )
					.arg( addNames.size() ) );
		return index;
	}
};

REGISTER_SPELL( spRiggingBindExistingNodes )

// ---------------------------------------------------------------------------
// FO4 CustomizationRemapData. Each vertex contributes the exact packed skin
// record used by the game: four float16 weights followed by four uint8 bone
// indices. This snapshots the shape's current standard-skeleton binding.
// ---------------------------------------------------------------------------

static QByteArray riggingCustomizationRemapBlob( const NifModel * nif,
	const QModelIndex & shape, QString * error )
{
	QModelIndex iVD = nif->getIndex( shape, "Vertex Data" );
	if ( !iVD.isValid() || !nif->isArray( iVD ) ) {
		if ( error ) *error = Spell::tr( "The shape has no readable FO4 vertex data." );
		return {};
	}

	QByteArray blob;
	blob.reserve( nif->rowCount( iVD ) * 12 );
	for ( int i = 0; i < nif->rowCount( iVD ); i++ ) {
		QModelIndex iVertex = nif->getIndex( iVD, i );
		QModelIndex iW = nif->getIndex( iVertex, "Bone Weights" );
		QModelIndex iI = nif->getIndex( iVertex, "Bone Indices" );
		if ( !iW.isValid() || !iI.isValid()
			|| nif->rowCount( iW ) != 4 || nif->rowCount( iI ) != 4 ) {
			if ( error )
				*error = Spell::tr( "Vertex %1 does not have exactly four skin slots." ).arg( i );
			return {};
		}

		char record[12] = {};
		for ( int slot = 0; slot < 4; slot++ ) {
			float weight = nif->get<float>( nif->getIndex( iW, slot ) );
			quint16 half = std::bit_cast<quint16>( qfloat16( weight ) );
			record[slot * 2] = char( half & 0xff );
			record[slot * 2 + 1] = char( half >> 8 );
			record[8 + slot] = char( half != 0
				? nif->get<quint8>( nif->getIndex( iI, slot ) ) : 0 );
		}
		blob.append( record, sizeof( record ) );
	}
	return blob;
}

//! Read-only integrity check for the FO4 skin structures touched by transfer.
class spRiggingValidateFO4Skin final : public Spell
{
public:
	QString name() const override final { return Spell::tr( "Validate FO4 Skin" ); }
	QString page() const override final { return Spell::tr( "Rigging" ); }
	bool constant() const override final { return true; }

	bool isApplicable( const NifModel * nif, const QModelIndex & index ) override final
	{
		return nif && nif->getBSVersion() == 130
			&& riggingIsSkinnedShape( nif, index )
			&& nif->getIndex( index, "Vertex Data" ).isValid();
	}

	QModelIndex cast( NifModel * nif, const QModelIndex & index ) override final
	{
		QStringList problems;
		QModelIndex iSkin = riggingSkinInstance( nif, index );
		QModelIndex iBones = nif->getIndex( iSkin, "Bones" );
		int boneRows = iBones.isValid() ? nif->rowCount( iBones ) : 0;
		quint32 declaredBones = nif->get<quint32>( iSkin, "Num Bones" );
		if ( !nif->isNiBlock( iSkin, "BSSkin::Instance" ) )
			problems << Spell::tr( "Skin block is not BSSkin::Instance." );
		if ( !iBones.isValid() )
			problems << Spell::tr( "BSSkin::Instance has no Bones array." );
		if ( declaredBones != quint32( boneRows ) )
			problems << Spell::tr( "Skin Num Bones is %1, but the Bones array has %2 rows." )
				.arg( declaredBones ).arg( boneRows );
		if ( boneRows > 256 )
			problems << Spell::tr( "The skin has %1 bones; vertex indices can address at most 256." ).arg( boneRows );

		int skinBlock = nif->getBlockNumber( iSkin );
		int skinOwners = 0;
		for ( int parent = 0; parent < nif->getBlockCount(); parent++ ) {
			QModelIndex iOwner = nif->getBlockIndex( parent );
			if ( riggingIsTriShape( nif, iOwner )
				&& ( nif->getLink( iOwner, "Skin" ) == skinBlock
					|| nif->getLink( iOwner, "Skin Instance" ) == skinBlock ) )
				skinOwners++;
		}
		if ( skinOwners != 1 )
			problems << Spell::tr( "BSSkin::Instance is referenced by %1 shapes; expected exactly one." ).arg( skinOwners );

		int skeletonRoot = nif->getLink( iSkin, "Skeleton Root" );
		QModelIndex iRoot = nif->getBlockIndex( skeletonRoot );
		QSet<int> skeletonTree;
		riggingCollectNodeTree( nif, skeletonRoot, skeletonTree );
		if ( !nif->blockInherits( iRoot, "NiAVObject" ) || skeletonTree.isEmpty() )
			problems << Spell::tr( "Skeleton Root is not a readable scene node." );

		QModelIndex iScales = nif->getIndex( iSkin, "Scales" );
		quint32 declaredScales = nif->get<quint32>( iSkin, "Num Scales" );
		int scaleRows = iScales.isValid() ? nif->rowCount( iScales ) : 0;
		if ( declaredScales != quint32( scaleRows ) )
			problems << Spell::tr( "Num Scales is %1, but Scales has %2 rows." )
				.arg( declaredScales ).arg( scaleRows );
		if ( declaredScales != 0 )
			problems << Spell::tr( "Nonempty BSSkin Scales is unsupported by the rigging write path." );

		int invalidBoneLinks = 0, wrongBoneTypes = 0, unreachableBones = 0, unnamedBones = 0;
		QSet<int> boneLinks;
		QSet<QString> boneNames, duplicateBoneNames;
		for ( int i = 0; i < boneRows; i++ ) {
			int link = nif->getLink( nif->getIndex( iBones, i ) );
			QModelIndex iBone = nif->getBlockIndex( link );
			if ( !iBone.isValid() ) {
				invalidBoneLinks++;
				continue;
			}
			if ( !nif->blockInherits( iBone, "NiNode" ) )
				wrongBoneTypes++;
			if ( !skeletonTree.contains( link ) )
				unreachableBones++;
			boneLinks << link;
			QString boneName = nif->get<QString>( iBone, "Name" );
			if ( boneName.isEmpty() )
				unnamedBones++;
			else if ( boneNames.contains( boneName ) )
				duplicateBoneNames << boneName;
			else
				boneNames << boneName;
		}
		if ( invalidBoneLinks )
			problems << Spell::tr( "%1 bone links are invalid." ).arg( invalidBoneLinks );
		if ( wrongBoneTypes )
			problems << Spell::tr( "%1 bone links do not reference NiNode blocks." ).arg( wrongBoneTypes );
		if ( unreachableBones )
			problems << Spell::tr( "%1 bones are not reachable below Skeleton Root." ).arg( unreachableBones );
		if ( boneLinks.size() + invalidBoneLinks != boneRows )
			problems << Spell::tr( "The skin contains duplicate bone links." );
		if ( unnamedBones )
			problems << Spell::tr( "%1 linked bones have no name." ).arg( unnamedBones );
		if ( !duplicateBoneNames.isEmpty() ) {
			QStringList names( duplicateBoneNames.begin(), duplicateBoneNames.end() );
			names.sort();
			problems << Spell::tr( "Duplicate skin-bone names: %1." ).arg( names.join( QStringLiteral( ", " ) ) );
		}
		bool hasSculptBones = false;
		for ( const QString & boneName : boneNames )
			hasSculptBones = hasSculptBones || boneName.startsWith( QStringLiteral( "skin_bone_" ), Qt::CaseInsensitive );

		QModelIndex iBoneData = nif->getBlockIndex( nif->getLink( iSkin, "Data" ) );
		QModelIndex iBoneList = nif->getIndex( iBoneData, "Bone List" );
		int dataRows = iBoneList.isValid() ? nif->rowCount( iBoneList ) : 0;
		quint32 declaredDataBones = iBoneData.isValid()
			? nif->get<quint32>( iBoneData, "Num Bones" ) : 0;
		if ( !nif->isNiBlock( iBoneData, "BSSkin::BoneData" ) )
			problems << Spell::tr( "Skin Data does not link to BSSkin::BoneData." );
		if ( declaredDataBones != quint32( dataRows ) )
			problems << Spell::tr( "BoneData Num Bones is %1, but Bone List has %2 rows." )
				.arg( declaredDataBones ).arg( dataRows );
		if ( dataRows != boneRows )
			problems << Spell::tr( "Skin Bones has %1 rows, but BoneData has %2." )
				.arg( boneRows ).arg( dataRows );
		int boneDataOwners = 0;
		int boneDataBlock = nif->getBlockNumber( iBoneData );
		for ( int parent = 0; parent < nif->getBlockCount(); parent++ ) {
			QModelIndex iOwner = nif->getBlockIndex( parent );
			if ( nif->isNiBlock( iOwner, "BSSkin::Instance" )
				&& nif->getLink( iOwner, "Data" ) == boneDataBlock )
				boneDataOwners++;
		}
		if ( boneDataOwners != 1 )
			problems << Spell::tr( "BSSkin::BoneData is referenced by %1 skin instances; expected exactly one." )
				.arg( boneDataOwners );

		int invalidBoneData = 0;
		for ( int i = 0; i < dataRows; i++ ) {
			QModelIndex iEntry = nif->getIndex( iBoneList, i );
			Transform t( nif, iEntry );
			bool valid = std::isfinite( t.scale ) && std::fabs( t.scale ) > 1.0e-8f;
			for ( int r = 0; r < 3; r++ ) {
				valid = valid && std::isfinite( t.translation[r] );
				for ( int c = 0; c < 3; c++ )
					valid = valid && std::isfinite( t.rotation( r, c ) );
			}
			QModelIndex iSphere = nif->getIndex( iEntry, "Bounding Sphere" );
			Vector3 center = nif->get<Vector3>( iSphere, "Center" );
			float radius = nif->get<float>( iSphere, "Radius" );
			valid = valid && std::isfinite( radius ) && radius >= 0.0f;
			for ( int c = 0; c < 3; c++ )
				valid = valid && std::isfinite( center[c] );
			if ( !valid ) invalidBoneData++;
		}
		if ( invalidBoneData )
			problems << Spell::tr( "%1 BoneData entries contain non-finite/invalid transforms or bounds." )
				.arg( invalidBoneData );

		QModelIndex iVD = nif->getIndex( index, "Vertex Data" );
		int vertices = nif->rowCount( iVD );
		int badSlots = 0, badIndices = 0, badSums = 0, unweighted = 0, duplicateIndices = 0;
		for ( int i = 0; i < vertices; i++ ) {
			QModelIndex iVertex = nif->getIndex( iVD, i );
			QModelIndex iW = nif->getIndex( iVertex, "Bone Weights" );
			QModelIndex iI = nif->getIndex( iVertex, "Bone Indices" );
			if ( !iW.isValid() || !iI.isValid()
				|| nif->rowCount( iW ) != 4 || nif->rowCount( iI ) != 4 ) {
				badSlots++;
				continue;
			}
			float sum = 0.0f;
			bool invalidWeights = false;
			bool duplicateActiveIndex = false;
			QSet<int> activeIndices;
			for ( int slot = 0; slot < 4; slot++ ) {
				float w = nif->get<float>( nif->getIndex( iW, slot ) );
				int bi = nif->get<quint8>( nif->getIndex( iI, slot ) );
				if ( !std::isfinite( w ) || w < 0.0f ) {
					invalidWeights = true;
					w = 0.0f;
				}
				if ( w > 0.0f ) {
					if ( bi >= boneRows )
						badIndices++;
					if ( activeIndices.contains( bi ) )
						duplicateActiveIndex = true;
					activeIndices << bi;
				}
				sum += w;
			}
			if ( duplicateActiveIndex )
				duplicateIndices++;
			if ( sum <= 0.0001f )
				unweighted++;
			else if ( invalidWeights || std::fabs( sum - 1.0f ) > 0.01f )
				badSums++;
		}
		if ( badSlots )
			problems << Spell::tr( "%1 vertices do not have four weight/index slots." ).arg( badSlots );
		if ( badIndices )
			problems << Spell::tr( "%1 nonzero influences use an out-of-range bone index." ).arg( badIndices );
		if ( badSums )
			problems << Spell::tr( "%1 vertices contain invalid weights or do not sum to 1." ).arg( badSums );
		if ( unweighted )
			problems << Spell::tr( "%1 vertices have no nonzero weight." ).arg( unweighted );
		if ( duplicateIndices )
			problems << Spell::tr( "%1 vertices repeat a nonzero bone index across slots." ).arg( duplicateIndices );

		int remapBytes = -1, remapCount = 0;
		QByteArray remapData;
		for ( int link : nif->getChildLinks( nif->getBlockNumber( index ) ) ) {
			QModelIndex iExtra = nif->getBlockIndex( link, "NiBinaryExtraData" );
			if ( iExtra.isValid()
				&& nif->get<QString>( iExtra, "Name" ) == QStringLiteral( "CustomizationRemapData" ) ) {
				remapCount++;
				if ( remapCount == 1 ) {
					remapData = nif->get<QByteArray>( iExtra, "Binary Data" );
					remapBytes = remapData.size();
				}
			}
		}
		if ( remapCount > 1 )
			problems << Spell::tr( "%1 CustomizationRemapData blocks are attached; expected at most one." )
				.arg( remapCount );
		if ( remapBytes >= 0 && remapBytes != vertices * 12 )
			problems << Spell::tr( "CustomizationRemapData has %1 bytes; expected %2." )
				.arg( remapBytes ).arg( vertices * 12 );
		// On a standard-skeleton mesh the blob is a direct snapshot and can be
		// compared byte-for-byte. On a faceBones mesh it intentionally describes
		// the *other* (standard) binding, so equality to current sculpt weights is
		// neither expected nor required.
		if ( remapCount == 1 && remapBytes == vertices * 12 && !hasSculptBones ) {
			QString encodeError;
			QByteArray expected = riggingCustomizationRemapBlob( nif, index, &encodeError );
			if ( expected.isEmpty() )
				problems << Spell::tr( "Could not regenerate RemapData for comparison: %1" ).arg( encodeError );
			else if ( remapData != expected )
				problems << Spell::tr( "CustomizationRemapData is stale: its bytes do not match current vertex skinning." );
		}

		QString summary = Spell::tr( "%1 vertices, %2 skin bones, %3 BoneData entries." )
			.arg( vertices ).arg( boneRows ).arg( dataRows );
		if ( remapBytes >= 0 )
			summary += Spell::tr( "\nCustomizationRemapData: %1 bytes." ).arg( remapBytes );
		else
			summary += Spell::tr( "\nCustomizationRemapData: not attached." );

		QMessageBox box( problems.isEmpty() ? QMessageBox::Information : QMessageBox::Warning,
			name(), summary + ( problems.isEmpty()
				? Spell::tr( "\n\nNo structural skin problems found." )
				: Spell::tr( "\n\nFound %1 problem(s)." ).arg( problems.size() ) ) );
		if ( !problems.isEmpty() )
			box.setDetailedText( problems.join( QStringLiteral( "\n" ) ) );
		box.exec();
		return index;
	}
};

REGISTER_SPELL( spRiggingValidateFO4Skin )

//! Rebuild BSSkin bind transforms against an explicit game/reference skeleton.
//! This is deliberately opt-in and preserves the authored raw vertices: in the
//! chosen skeleton's reference pose every bone skin matrix becomes identity.
class spRiggingRebindReferenceSkeleton final : public Spell
{
public:
	QString name() const override final { return Spell::tr( "Rebind to Reference Skeleton..." ); }
	QString page() const override final { return Spell::tr( "Rigging" ); }
	bool undoable() const override final { return true; }

	bool isApplicable( const NifModel * nif, const QModelIndex & index ) override final
	{
		return nif && nif->getBSVersion() == 130 && riggingIsSkinnedShape( nif, index )
			&& nif->isNiBlock( riggingSkinInstance( nif, index ), "BSSkin::Instance" );
	}

	QModelIndex cast( NifModel * nif, const QModelIndex & index ) override final
	{
		QString fileName = riggingChooseReferenceSkeletonFile( nif );
		if ( fileName.isEmpty() )
			return index;

		NifModel reference;
		if ( !reference.loadFromFile( fileName ) ) {
			QMessageBox::warning( nullptr, name(), Spell::tr( "Could not load the reference skeleton NIF." ) );
			return index;
		}
		if ( reference.getBSVersion() != nif->getBSVersion() ) {
			QMessageBox::warning( nullptr, name(), Spell::tr(
				"The mesh and reference skeleton use different Bethesda stream versions." ) );
			return index;
		}

		QModelIndex iSkin = riggingSkinInstance( nif, index );
		QModelIndex iBones = nif->getIndex( iSkin, "Bones" );
		QModelIndex iData = nif->getBlockIndex( nif->getLink( iSkin, "Data" ) );
		QModelIndex iBoneList = nif->getIndex( iData, "Bone List" );
		const int boneCount = iBones.isValid() ? nif->rowCount( iBones ) : 0;
		if ( !nif->isNiBlock( iData, "BSSkin::BoneData" ) || !iBoneList.isValid()
			|| nif->rowCount( iBoneList ) != boneCount || boneCount <= 0 ) {
			QMessageBox::warning( nullptr, name(), Spell::tr(
				"The receiver skin and BoneData arrays are incomplete or have different lengths. Run Validate FO4 Skin first." ) );
			return index;
		}
		if ( nif->get<quint32>( iSkin, "Num Scales" ) != 0 ) {
			QMessageBox::warning( nullptr, name(), Spell::tr(
				"Rebinding skins with BSSkin Scales is not supported." ) );
			return index;
		}

		// Shared BoneData must be duplicated before it can be changed safely. Do
		// not silently alter other shapes that the user did not select.
		const int dataBlock = nif->getBlockNumber( iData );
		int dataOwners = 0;
		for ( int b = 0; b < nif->getBlockCount(); b++ ) {
			QModelIndex block = nif->getBlockIndex( b );
			if ( nif->isNiBlock( block, "BSSkin::Instance" )
				&& nif->getLink( block, "Data" ) == dataBlock )
				dataOwners++;
		}
		if ( dataOwners != 1 ) {
			QMessageBox::warning( nullptr, name(), Spell::tr(
				"This BoneData block is shared by %1 skin instances. Rebinding was stopped so other meshes are not changed." )
				.arg( dataOwners ) );
			return index;
		}

		const int targetRoot = nif->getLink( iSkin, "Skeleton Root" );
		QModelIndex iTargetRoot = nif->getBlockIndex( targetRoot );
		const QString rootName = nif->get<QString>( iTargetRoot, "Name" );
		if ( !iTargetRoot.isValid() || rootName.isEmpty() ) {
			QMessageBox::warning( nullptr, name(), Spell::tr(
				"The receiver Skeleton Root is invalid or unnamed." ) );
			return index;
		}

		QVector<int> referenceRoots;
		for ( int b = 0; b < reference.getBlockCount(); b++ ) {
			QModelIndex block = reference.getBlockIndex( b );
			if ( reference.blockInherits( block, "NiNode" )
				&& reference.get<QString>( block, "Name" ) == rootName )
				referenceRoots.append( b );
		}
		if ( referenceRoots.size() != 1 ) {
			QMessageBox::warning( nullptr, name(), Spell::tr(
				"Expected one reference NiNode named '%1', but found %2. Choose the skeleton used by this mesh/game profile." )
				.arg( rootName ).arg( referenceRoots.size() ) );
			return index;
		}
		const int referenceRoot = referenceRoots.first();
		QSet<int> referenceTree;
		riggingCollectNodeTree( &reference, referenceRoot, referenceTree );

		QHash<QString, int> referenceByName;
		QSet<QString> duplicateReferenceNames;
		for ( int b : referenceTree ) {
			QModelIndex block = reference.getBlockIndex( b );
			QString boneName = reference.get<QString>( block, "Name" );
			if ( boneName.isEmpty() )
				continue;
			if ( referenceByName.contains( boneName ) )
				duplicateReferenceNames.insert( boneName );
			else
				referenceByName.insert( boneName, b );
		}

		Transform shapePose;
		const int shapeBlock = nif->getBlockNumber( index );
		if ( !riggingNodeWorld( nif, shapeBlock, targetRoot, shapePose ) ) {
			QMessageBox::warning( nullptr, name(), Spell::tr(
				"The receiver shape is not reachable below its declared Skeleton Root." ) );
			return index;
		}

		QVector<Transform> expected;
		expected.reserve( boneCount );
		QHash<QString, Transform> currentReferenceSkin;
		QStringList missing, ambiguous;
		int changed = 0;
		float maximumDelta = 0.0f;
		for ( int slot = 0; slot < boneCount; slot++ ) {
			QModelIndex bone = nif->getBlockIndex( nif->getLink( nif->getIndex( iBones, slot ) ) );
			QString boneName = nif->get<QString>( bone, "Name" );
			if ( boneName.isEmpty() || !referenceByName.contains( boneName ) ) {
				missing << ( boneName.isEmpty() ? Spell::tr( "<unnamed slot %1>" ).arg( slot ) : boneName );
				expected.append( Transform() );
				continue;
			}
			if ( duplicateReferenceNames.contains( boneName ) ) {
				ambiguous << boneName;
				expected.append( Transform() );
				continue;
			}
			Transform referencePose;
			if ( !riggingNodeWorld( &reference, referenceByName.value( boneName ), referenceRoot, referencePose ) ) {
				missing << boneName;
				expected.append( Transform() );
				continue;
			}
			Transform bind = referencePose.inverted() * shapePose;
			expected.append( bind );
			Transform currentBind( nif, nif->getIndex( iBoneList, slot ) );
			currentReferenceSkin.insert( boneName, shapePose.inverted() * referencePose * currentBind );
			float delta = riggingTransformDelta( currentBind, bind );
			maximumDelta = qMax( maximumDelta, delta );
			if ( delta > 0.0001f )
				changed++;
		}

		missing.removeDuplicates();
		ambiguous.removeDuplicates();
		if ( !missing.isEmpty() || !ambiguous.isEmpty() ) {
			QString details;
			if ( !missing.isEmpty() )
				details += Spell::tr( "Missing reference bones:\n%1" ).arg( missing.join( QStringLiteral( "\n" ) ) );
			if ( !ambiguous.isEmpty() )
				details += ( details.isEmpty() ? QString() : QStringLiteral( "\n\n" ) )
					+ Spell::tr( "Duplicate reference bone names:\n%1" ).arg( ambiguous.join( QStringLiteral( "\n" ) ) );
			QMessageBox box( QMessageBox::Warning, name(), Spell::tr(
				"The reference skeleton cannot unambiguously bind all %1 receiver bones." ).arg( boneCount ) );
			box.setDetailedText( details );
			box.exec();
			return index;
		}

		if ( changed == 0 ) {
			QMessageBox::information( nullptr, name(), Spell::tr(
				"All %1 bind transforms already match '%2' within tolerance. No NIF data was changed." )
				.arg( boneCount ).arg( QFileInfo( fileName ).fileName() ) );
			return index;
		}

		// Report the consequence in the units artists actually care about: how
		// far the chosen game skeleton would move the currently-authored vertices.
		RigShape geometry = riggingReadShape( nif, index );
		float maximumVertexMove = 0.0f;
		double totalVertexMove = 0.0;
		int measuredVertices = 0;
		if ( geometry.valid && geometry.skin.size() == geometry.pos.size() ) {
			for ( int vertex = 0; vertex < geometry.pos.size(); vertex++ ) {
				Vector3 evaluated;
				float sum = 0.0f;
				for ( const auto & influence : geometry.skin.at( vertex ) ) {
					if ( !( influence.second > 0.0f ) || !currentReferenceSkin.contains( influence.first ) )
						continue;
					evaluated += ( currentReferenceSkin.value( influence.first ) * geometry.pos.at( vertex ) )
						* influence.second;
					sum += influence.second;
				}
				if ( !( sum > 0.0f ) )
					continue;
				evaluated /= sum;
				float distance = ( evaluated - geometry.pos.at( vertex ) ).length();
				maximumVertexMove = qMax( maximumVertexMove, distance );
				totalVertexMove += distance;
				measuredVertices++;
			}
		}
		const double averageVertexMove = measuredVertices > 0
			? totalVertexMove / double( measuredVertices ) : 0.0;

		QString prompt = Spell::tr(
			"Reference: %1\nSkeleton Root: %2\nMatched bones: %3\nTransforms to replace: %4\nMaximum component difference: %5\n\n"
			"Current reference-pose vertex movement: maximum %6, average %7 (%8 measured vertices)\n\n"
			"Rebind while preserving the raw mesh geometry? In the selected reference pose, skinning will leave those raw vertices in place. This is one Undo step." )
			.arg( QFileInfo( fileName ).fileName() ).arg( rootName ).arg( boneCount ).arg( changed )
			.arg( maximumDelta, 0, 'g', 6 ).arg( maximumVertexMove, 0, 'g', 6 )
			.arg( averageVertexMove, 0, 'g', 6 ).arg( measuredVertices );
		if ( QMessageBox::question( nullptr, name(), prompt, QMessageBox::Yes | QMessageBox::Cancel,
			QMessageBox::Cancel ) != QMessageBox::Yes )
			return index;

		bool boundsUpdated = false;
		bool snapshot = nifSnapshotOp( nif, Spell::tr( "Rebind skin to %1" ).arg( rootName ), [&]() {
			for ( int slot = 0; slot < expected.size(); slot++ )
				expected.at( slot ).writeBack( nif, nif->getIndex( iBoneList, slot ) );
			boundsUpdated = spUpdateBounds::calculateBoneBounds( nif, QPersistentModelIndex( index ) );
		} );
		if ( !snapshot ) {
			QMessageBox::warning( nullptr, name(), Spell::tr( "The rebind could not be stored as an Undo step." ) );
			return index;
		}
		QMessageBox::information( nullptr, name(), boundsUpdated
			? Spell::tr( "Rebound %1 bones and recalculated bone bounds." ).arg( boneCount )
			: Spell::tr( "Rebound %1 bones. Bone bounds could not be recalculated; run Update Bounds before export." ).arg( boneCount ) );
		return index;
	}
};

REGISTER_SPELL( spRiggingRebindReferenceSkeleton )

//! Generate/update the FO4 CustomizationRemapData skin snapshot on a shape.
class spRiggingGenerateCustomizationRemapData final : public Spell
{
public:
	QString name() const override final { return Spell::tr( "Generate CustomizationRemapData" ); }
	QString page() const override final { return Spell::tr( "Rigging" ); }
	bool undoable() const override final { return true; }

	bool isApplicable( const NifModel * nif, const QModelIndex & index ) override final
	{
		return nif && nif->getBSVersion() == 130
			&& riggingIsSkinnedShape( nif, index )
			&& nif->getIndex( index, "Vertex Data" ).isValid();
	}

	QModelIndex cast( NifModel * nif, const QModelIndex & index ) override final
	{
		for ( const QString & bone : riggingBoneNames( nif, index ) ) {
			if ( bone.startsWith( QStringLiteral( "skin_bone_" ), Qt::CaseInsensitive ) ) {
				QMessageBox::warning( nullptr, name(),
					Spell::tr( "This shape is already bound to face-sculpt bones. "
						"CustomizationRemapData must be generated from its standard-skeleton skinning "
						"before sculpt-bone transfer." ) );
				return index;
			}
		}
		QString error;
		QByteArray blob = riggingCustomizationRemapBlob( nif, index, &error );
		if ( blob.isEmpty() ) {
			QMessageBox::warning( nullptr, name(), error.isEmpty()
				? Spell::tr( "Could not encode the shape's skin data." ) : error );
			return index;
		}

		QModelIndex iRemap;
		for ( int link : nif->getChildLinks( nif->getBlockNumber( index ) ) ) {
			QModelIndex iExtra = nif->getBlockIndex( link, "NiBinaryExtraData" );
			if ( iExtra.isValid()
				&& nif->get<QString>( iExtra, "Name" ) == QStringLiteral( "CustomizationRemapData" ) ) {
				iRemap = iExtra;
				break;
			}
		}

		bool created = !iRemap.isValid();
		if ( !created && nif->get<QByteArray>( iRemap, "Binary Data" ) == blob ) {
			riggingWorkflowStepSucceeded();
			if ( !riggingWorkflow.active )
				QMessageBox::information( nullptr, name(),
					Spell::tr( "CustomizationRemapData is already current: %1 vertices, %2 bytes." )
						.arg( blob.size() / 12 ).arg( blob.size() ) );
			return iRemap;
		}
		if ( created && !nif->getIndex( index, "Extra Data List" ).isValid() ) {
			QMessageBox::warning( nullptr, name(),
				Spell::tr( "The target shape has no attachable Extra Data List." ) );
			return index;
		}

		QPersistentModelIndex pShape = index;
		QPersistentModelIndex pRemap = iRemap;
		bool applied = true;
		bool changed = nifSnapshotOp( nif, created
			? Spell::tr( "Create CustomizationRemapData" )
			: Spell::tr( "Update CustomizationRemapData" ), [&]() {
			if ( !pRemap.isValid() ) {
				QModelIndex added = nif->insertNiBlock( "NiBinaryExtraData",
					nif->getBlockNumber( pShape ) + 1 );
				if ( !added.isValid() ) {
					applied = false;
					return;
				}
				pRemap = added;
				if ( !addLink( nif, pShape, "Extra Data List", nif->getBlockNumber( pRemap ) )
					|| !nif->set<QString>( pRemap, "Name", QStringLiteral( "CustomizationRemapData" ) ) ) {
					applied = false;
					return;
				}
			}
			applied = nif->set<QByteArray>( pRemap, "Binary Data", blob );
		} );
		if ( !changed || !applied ) {
			QMessageBox::warning( nullptr, name(),
				Spell::tr( "Could not create an undoable CustomizationRemapData change. "
					"Use Undo if a partial block was created." ) );
			return index;
		}
		iRemap = pRemap;
		riggingWorkflowStepSucceeded();
		if ( !riggingWorkflow.active )
			QMessageBox::information( nullptr, name(),
				Spell::tr( "%1 CustomizationRemapData: %2 vertices, %3 bytes." )
					.arg( created ? Spell::tr( "Created" ) : Spell::tr( "Updated" ) )
					.arg( blob.size() / 12 ).arg( blob.size() ) );
		return iRemap;
	}
};

REGISTER_SPELL( spRiggingGenerateCustomizationRemapData )

//! Complete standard-skeleton to face-bone transfer. The existing step spells
//! remain independently useful, but this is the normal user path (and the
//! command the Rigging workspace invokes): choose the donor once, capture the
//! remap, import nodes, bind bones, and transfer weights as one undo snapshot.
class spRiggingTransferBonesAndWeights final : public Spell
{
public:
	QString name() const override final { return Spell::tr( "Transfer Bones and Weights..." ); }
	QString page() const override final { return Spell::tr( "Rigging" ); }
	bool undoable() const override final { return true; }

	bool isApplicable( const NifModel * nif, const QModelIndex & index ) override final
	{
		if ( !nif || !nif->undoStack || nif->getBSVersion() != 130
			|| !riggingIsSkinnedShape( nif, index )
			|| !nif->isNiBlock( riggingSkinInstance( nif, index ), "BSSkin::Instance" )
			|| !nif->getIndex( index, "Vertex Data" ).isValid() )
			return false;
		for ( const QString & bone : riggingBoneNames( nif, index ) )
			if ( bone.startsWith( QStringLiteral( "skin_bone_" ), Qt::CaseInsensitive ) )
				return false;
		return true;
	}

	QModelIndex cast( NifModel * nif, const QModelIndex & index ) override final
	{
		if ( riggingWorkflow.active )
			return index;

		QString fileName = riggingChooseDonorFile( nif );
		if ( fileName.isEmpty() )
			return index;
		NifModel donor;
		if ( !donor.loadFromFile( fileName ) ) {
			QMessageBox::warning( nullptr, name(), Spell::tr( "Could not load the donor NIF." ) );
			return index;
		}
		if ( donor.getBSVersion() != nif->getBSVersion() ) {
			QMessageBox::warning( nullptr, name(), Spell::tr( "Donor and target use different BS versions." ) );
			return index;
		}

		QStringList labels;
		QList<int> shapes = riggingSkinnedShapes( &donor, labels );
		if ( shapes.isEmpty() ) {
			QMessageBox::warning( nullptr, name(), Spell::tr( "The donor NIF has no skinned shapes." ) );
			return index;
		}
		int donorShape = riggingChooseDonorShape( shapes, labels, name() );
		if ( donorShape < 0 )
			return index;
		RigShape donorShapeData = riggingReadShape( &donor, donor.getBlockIndex( donorShape ) );
		if ( !donorShapeData.valid ) {
			QMessageBox::warning( nullptr, name(), Spell::tr( "Donor shape is not a readable FO4 BSTriShape." ) );
			return index;
		}

		RigShape targetShapeData = riggingReadShape( nif, index );
		if ( !targetShapeData.valid ) {
			QMessageBox::warning( nullptr, name(), Spell::tr( "Target shape is not a readable FO4 BSTriShape." ) );
			return index;
		}
		Transform targetSpace, donorSpace;
		int targetRoot = nif->getLink( riggingSkinInstance( nif, index ), "Skeleton Root" );
		int donorRoot = donor.getLink( riggingSkinInstance( &donor, donor.getBlockIndex( donorShape ) ), "Skeleton Root" );
		if ( !riggingNodeWorld( nif, nif->getBlockNumber( index ), targetRoot, targetSpace )
			|| !riggingNodeWorld( &donor, donorShape, donorRoot, donorSpace )
			|| riggingTransformDelta( targetSpace, donorSpace ) > 0.001f ) {
			QMessageBox::warning( nullptr, name(),
				Spell::tr( "Donor and target shapes are not in the same skeleton-root-relative space. "
					"No changes were made." ) );
			return index;
		}

		QVector<float> snapDistances;
		bool mappingCancelled = false;
		auto preparedWeights = riggingTransferWithProgress( donorShapeData, targetShapeData.pos,
			name(), &snapDistances, &mappingCancelled );
		if ( mappingCancelled ) {
			QMessageBox::information( nullptr, name(),
				Spell::tr( "Transfer cancelled during geometry analysis. Nothing was changed." ) );
			return index;
		}
		if ( preparedWeights.size() != targetShapeData.pos.size() ) {
			QMessageBox::warning( nullptr, name(),
				Spell::tr( "Could not prepare weights for every target vertex. No changes were made." ) );
			return index;
		}
		RiggingSnapStats snapStats = riggingSnapStats( donorShapeData, snapDistances );
		QString snapSummary = riggingSnapSummary( snapStats );

		QSet<QString> targetBones;
		for ( const QString & bone : targetShapeData.bones )
			if ( !bone.isEmpty() ) targetBones << bone;
		QSet<QString> donorUsedBones;
		for ( const auto & vertex : donorShapeData.skin )
			for ( const auto & influence : vertex )
				if ( !influence.first.isEmpty() && influence.second > 0.0f ) donorUsedBones << influence.first;
		bool donorHasSculptBones = false;
		for ( const QString & bone : donorUsedBones )
			donorHasSculptBones = donorHasSculptBones
				|| bone.startsWith( QStringLiteral( "skin_bone_" ), Qt::CaseInsensitive );
		QSet<QString> sharedBones = donorUsedBones;
		sharedBones.intersect( targetBones );
		QSet<QString> newBindings = donorUsedBones;
		newBindings.subtract( targetBones );
		QSet<QString> targetOnlyBones = targetBones;
		targetOnlyBones.subtract( donorUsedBones );
		if ( targetShapeData.bones.size() + newBindings.size() > 256 ) {
			QMessageBox::warning( nullptr, name(),
				Spell::tr( "The transfer would require %1 skin-bone slots, exceeding the uint8 limit of 256." )
					.arg( targetShapeData.bones.size() + newBindings.size() ) );
			return index;
		}

		QSet<int> targetTree;
		riggingCollectNodeTree( nif, targetRoot, targetTree );
		QSet<QString> targetNodeNames;
		for ( int node : targetTree ) {
			QString nodeName = nif->get<QString>( nif->getBlockIndex( node ), "Name" );
			if ( !nodeName.isEmpty() ) targetNodeNames << nodeName;
		}
		QSet<QString> missingUsedNodes = donorUsedBones;
		missingUsedNodes.subtract( targetNodeNames );

		bool hasRemap = false;
		for ( int link : nif->getChildLinks( nif->getBlockNumber( index ) ) ) {
			QModelIndex extra = nif->getBlockIndex( link, "NiBinaryExtraData" );
			if ( extra.isValid()
				&& nif->get<QString>( extra, "Name" ) == QStringLiteral( "CustomizationRemapData" ) ) {
				hasRemap = true;
				break;
			}
		}
		auto sortedNames = []( const QSet<QString> & names ) {
			QStringList result;
			for ( const QString & value : names ) result << value;
			result.sort( Qt::CaseInsensitive );
			return result;
		};
		auto detailSection = [&]( const QString & heading, const QSet<QString> & names ) {
			QStringList values = sortedNames( names );
			return heading + QStringLiteral( "\n" )
				+ ( values.isEmpty() ? Spell::tr( "<none>" ) : values.join( QStringLiteral( "\n" ) ) );
		};

		QString donorLabel = labels.value( shapes.indexOf( donorShape ), riggingDonorDisplayName( fileName ) );
		QString targetLabel = nif->get<QString>( index, "Name" );
		if ( targetLabel.isEmpty() ) targetLabel = Spell::tr( "<unnamed target>" );
		int expectedRemapBytes = targetShapeData.pos.size() * 12;
		QString summary = Spell::tr(
			"Target: %1\nDonor: %2\n\n"
			"Target geometry: %3 vertices, %4 triangles\n"
			"Donor geometry: %5 vertices, %6 triangles\n"
			"Donor skin: %7 used bones (%8 total)\n"
			"Shared used bindings: %9\n"
			"New bone bindings: %10\n"
			"Used donor bone nodes absent from target: %11\n"
			"CustomizationRemapData: %12 (%13 bytes)\n"
			"%14\n\n"
			"Transfer weights onto %3 target vertices as one Undo step?" )
			.arg( targetLabel, donorLabel )
			.arg( targetShapeData.pos.size() ).arg( targetShapeData.tris.size() )
			.arg( donorShapeData.pos.size() ).arg( donorShapeData.tris.size() )
			.arg( donorUsedBones.size() ).arg( donorShapeData.bones.size() )
			.arg( sharedBones.size() ).arg( newBindings.size() ).arg( missingUsedNodes.size() )
			.arg( hasRemap ? Spell::tr( "update" ) : Spell::tr( "create" ) ).arg( expectedRemapBytes )
			.arg( snapSummary );
		QString details = detailSection( Spell::tr( "New bindings:" ), newBindings )
			+ QStringLiteral( "\n\n" ) + detailSection( Spell::tr( "Shared used bindings:" ), sharedBones )
			+ QStringLiteral( "\n\n" ) + detailSection( Spell::tr( "Target-only bindings:" ), targetOnlyBones )
			+ QStringLiteral( "\n\n" ) + detailSection( Spell::tr( "Used donor bone nodes absent from target:" ), missingUsedNodes )
			+ Spell::tr( "\n\nGeometry guidance:\n"
				"Close: 95th percentile <= 1% and maximum <= 5% of donor span.\n"
				"Caution: 95th percentile <= 3% and maximum <= 15%.\n"
				"Poor: exceeds those limits; distant vertices may map to unrelated surfaces." );
		QMessageBox confirmation( QMessageBox::Question, name(), summary,
			QMessageBox::Yes | QMessageBox::Cancel );
		confirmation.setInformativeText( riggingSnapQuality( snapStats ) == 0
			? Spell::tr( "Geometry was analyzed before mutation. Missing hierarchy nodes are imported only as required." )
			: Spell::tr( "Caution: distant target vertices can copy weights from unrelated donor areas. "
				"Review the snap statistics and use Preview Transfer before continuing if the meshes are not intentional variants." ) );
		confirmation.setDetailedText( details );
		confirmation.setDefaultButton( QMessageBox::Cancel );
		if ( confirmation.exec() != QMessageBox::Yes )
			return index;

		auto serialize = [nif]( QByteArray & data ) {
			data.clear();
			QBuffer buffer( &data );
			return buffer.open( QIODevice::WriteOnly ) && nif->save( buffer );
		};
		auto restore = [nif]( const QByteArray & data ) {
			QBuffer buffer;
			buffer.setData( data );
			return buffer.open( QIODevice::ReadOnly ) && nif->load( buffer );
		};
		auto nodeCount = []( const NifModel * model ) {
			int count = 0;
			for ( int b = 0; b < model->getBlockCount(); b++ )
				if ( model->blockInherits( model->getBlockIndex( b ), "NiNode" ) ) count++;
			return count;
		};

		QByteArray before;
		if ( !serialize( before ) ) {
			QMessageBox::warning( nullptr, name(), Spell::tr( "Could not capture the target before transfer." ) );
			return index;
		}
		int targetBlock = nif->getBlockNumber( index );
		int oldNodes = nodeCount( nif );
		int oldBones = riggingBoneNames( nif, index ).size();
		QUndoStack * undo = nif->undoStack;
		nif->undoStack = nullptr; // inner steps serialize, but only the outer command enters history
		QPersistentModelIndex target = index;

		riggingWorkflow.active = true;
		riggingWorkflow.donorFile = fileName;
		riggingWorkflow.donorShape = donorShape;
		riggingWorkflow.preparedWeights = preparedWeights;
		QString failedStep;
		auto runStep = [&]( Spell & step ) {
			riggingWorkflow.stepSucceeded = false;
			step.cast( nif, target );
			if ( !riggingWorkflow.stepSucceeded )
				failedStep = step.name();
			return riggingWorkflow.stepSucceeded;
		};

		spRiggingGenerateCustomizationRemapData generate;
		spRiggingImportBoneNodes importNodes;
		spRiggingBindExistingNodes bindBones;
		spRiggingTransferWeights transferWeights;
		// Face-sculpt transfer needs the original standard-skeleton snapshot, so
		// generate RemapData before replacing the weights. A standard-only donor
		// has no alternate sculpt binding; generate after the write so the blob
		// reflects the exact float16 values NifModel packed into Vertex Data.
		bool completed = donorHasSculptBones
			? ( runStep( generate )
				&& runStep( importNodes )
				&& runStep( bindBones )
				&& runStep( transferWeights ) )
			: ( runStep( importNodes )
				&& runStep( bindBones )
				&& runStep( transferWeights )
				&& runStep( generate ) );

		riggingWorkflow = RiggingWorkflowState();
		if ( !completed ) {
			bool restored = restore( before );
			nif->undoStack = undo;
			QMessageBox::warning( nullptr, name(), restored
				? Spell::tr( "The \"%1\" step did not complete. The target was restored to its pre-transfer state." )
					.arg( failedStep )
				: Spell::tr( "The \"%1\" step failed and the target could not be restored automatically. "
					"Close this file without saving and reopen the original target." ).arg( failedStep ) );
			return nif->getBlockIndex( targetBlock );
		}

		QByteArray after;
		if ( !serialize( after ) ) {
			bool restored = restore( before );
			nif->undoStack = undo;
			QMessageBox::warning( nullptr, name(), restored
				? Spell::tr( "Could not capture the completed transfer. The target was restored." )
				: Spell::tr( "Could not capture the completed transfer or restore the target automatically. "
					"Close this file without saving and reopen the original target." ) );
			return nif->getBlockIndex( targetBlock );
		}
		nif->undoStack = undo;
		undo->push( new NifSnapshotCommand( nif, before, after,
			Spell::tr( "Transfer bones and weights from %1" ).arg( riggingDonorDisplayName( fileName ) ) ) );

		QModelIndex result = target;
		int remapBytes = 0;
		for ( int link : nif->getChildLinks( nif->getBlockNumber( result ) ) ) {
			QModelIndex extra = nif->getBlockIndex( link, "NiBinaryExtraData" );
			if ( extra.isValid()
				&& nif->get<QString>( extra, "Name" ) == QStringLiteral( "CustomizationRemapData" ) ) {
				remapBytes = nif->get<QByteArray>( extra, "Binary Data" ).size();
				break;
			}
		}
		int vertices = nif->rowCount( nif->getIndex( result, "Vertex Data" ) );
		QMessageBox::information( nullptr, name(),
			Spell::tr( "Completed the transfer from \"%1\" as one Undo step.\n\n"
				"Imported nodes: %2\nBound bones: %3\nTransferred vertices: %4\n"
				"CustomizationRemapData: %5 bytes" )
				.arg( riggingDonorDisplayName( fileName ) )
				.arg( qMax( 0, nodeCount( nif ) - oldNodes ) )
				.arg( qMax( 0, riggingBoneNames( nif, result ).size() - oldBones ) )
				.arg( vertices ).arg( remapBytes ) );
		return result;
	}
};

REGISTER_SPELL( spRiggingTransferBonesAndWeights )

// ---------------------------------------------------------------------------
// Manual FO4 weight painting. Brush sampling lives in GLView; these helpers
// produce deterministic, normalized four-slot records and apply one snapshot
// per completed mouse stroke.
// ---------------------------------------------------------------------------

struct RigPaintVertex
{
	int vertex = -1;
	std::array<float, 4> weights = {};
	std::array<quint8, 4> bones = {};
};

static FloatVector4 riggingWeightHeatColor( float weight )
{
	float w = qBound( 0.0f, weight, 1.0f );
	float r = 0.0f, g = 0.0f, b = 0.0f;
	if ( w < 0.25f ) {
		float t = w * 4.0f;
		g = t; b = 1.0f;
	} else if ( w < 0.5f ) {
		float t = ( w - 0.25f ) * 4.0f;
		g = 1.0f; b = 1.0f - t;
	} else if ( w < 0.75f ) {
		float t = ( w - 0.5f ) * 4.0f;
		r = t; g = 1.0f;
	} else {
		float t = ( w - 0.75f ) * 4.0f;
		r = 1.0f; g = 1.0f - t;
	}
	return FloatVector4( r, g, b, 0.82f );
}

static float riggingPaintStrokeAlpha( float brushAmount, float strength, int mode, bool accumulate )
{
	brushAmount = qMax( 0.0f, brushAmount );
	strength = qBound( 0.0f, strength, 1.0f );
	if ( !accumulate )
		return strength * qMin( brushAmount, 1.0f );
	if ( mode <= 1 )
		return strength * brushAmount; // Add/Subtract are deliberately allowed to build to the clamp.
	// Repeated Replace/Smooth dabs compound toward their target instead of
	// overshooting it. Fractional falloff acts as a fractional dab.
	return 1.0f - float( std::pow( qMax( 0.0f, 1.0f - strength ), brushAmount ) );
}

static bool riggingBuildPaintStroke( const NifModel * nif, const QModelIndex & shape,
	int selectedBone, const QMap<int, float> & brush, int mode, float paintWeight,
	float strength, bool accumulate, QVector<RigPaintVertex> & result, QString & error )
{
	result.clear();
	QModelIndex iVD = nif->getIndex( shape, "Vertex Data" );
	QStringList boneNames = riggingBoneNames( nif, shape );
	int nv = nif->rowCount( iVD );
	if ( !iVD.isValid() || nv <= 0 || selectedBone < 0 || selectedBone >= boneNames.size()
		|| selectedBone > 255 ) {
		error = Spell::tr( "The selected FO4 skin bone cannot be painted." );
		return false;
	}

	QVector<QMap<int, float>> source( nv );
	QVector<std::array<float, 4>> oldWeights( nv );
	QVector<std::array<quint8, 4>> oldBones( nv );
	for ( int vertex = 0; vertex < nv; vertex++ ) {
		QModelIndex iVertex = nif->getIndex( iVD, vertex );
		QModelIndex iW = nif->getIndex( iVertex, "Bone Weights" );
		QModelIndex iI = nif->getIndex( iVertex, "Bone Indices" );
		if ( !iW.isValid() || !iI.isValid()
			|| nif->rowCount( iW ) != 4 || nif->rowCount( iI ) != 4 ) {
			error = Spell::tr( "Vertex %1 does not have exactly four skin slots." ).arg( vertex );
			return false;
		}
		for ( int slot = 0; slot < 4; slot++ ) {
			float weight = qBound( 0.0f, nif->get<float>( nif->getIndex( iW, slot ) ), 1.0f );
			int bone = nif->get<quint8>( nif->getIndex( iI, slot ) );
			oldWeights[vertex][slot] = weight;
			oldBones[vertex][slot] = quint8( bone );
			if ( weight > 0.0f && bone >= 0 && bone < boneNames.size() )
				source[vertex][bone] += weight;
		}
	}

	QVector<QSet<int>> neighbours;
	if ( mode == 3 ) {
		neighbours.resize( nv );
		RigShape geometry = riggingReadShape( nif, shape );
		if ( !geometry.valid ) {
			error = Spell::tr( "The target topology could not be read for smoothing." );
			return false;
		}
		for ( const Triangle & triangle : geometry.tris ) {
			int v[3] = { triangle.v1(), triangle.v2(), triangle.v3() };
			for ( int edge = 0; edge < 3; edge++ ) {
				neighbours[v[edge]].insert( v[( edge + 1 ) % 3] );
				neighbours[v[( edge + 1 ) % 3]].insert( v[edge] );
			}
		}
	}

	paintWeight = qBound( 0.0f, paintWeight, 1.0f );
	strength = qBound( 0.0f, strength, 1.0f );
	for ( auto brushIt = brush.constBegin(); brushIt != brush.constEnd(); ++brushIt ) {
		int vertex = brushIt.key();
		if ( vertex < 0 || vertex >= nv )
			continue;
		float alpha = riggingPaintStrokeAlpha( brushIt.value(), strength, mode, accumulate );
		if ( alpha <= 0.0f )
			continue;

		QMap<int, float> weights = source.at( vertex );
		float current = weights.value( selectedBone, 0.0f );
		float target = current;
		if ( mode == 0 )
			target = qMin( 1.0f, current + paintWeight * alpha );
		else if ( mode == 1 )
			target = qMax( 0.0f, current - paintWeight * alpha );
		else if ( mode == 2 )
			target = current + ( paintWeight - current ) * alpha;
		else {
			float average = current;
			if ( !neighbours.at( vertex ).isEmpty() ) {
				average = 0.0f;
				for ( int neighbour : neighbours.at( vertex ) )
					average += source.at( neighbour ).value( selectedBone, 0.0f );
				average /= float( neighbours.at( vertex ).size() );
			}
			target = current + ( average - current ) * alpha;
		}
		target = qBound( 0.0f, target, 1.0f );

		float otherSum = 0.0f;
		for ( auto it = weights.constBegin(); it != weights.constEnd(); ++it )
			if ( it.key() != selectedBone ) otherSum += it.value();
		if ( otherSum <= 1.0e-8f ) {
			// A vertex must remain normalized. A sole influence cannot be reduced
			// until another bone is added to it.
			weights.clear();
			weights[selectedBone] = 1.0f;
		} else {
			float otherScale = ( 1.0f - target ) / otherSum;
			for ( auto it = weights.begin(); it != weights.end(); ++it )
				if ( it.key() != selectedBone ) it.value() *= otherScale;
			if ( target > 1.0e-8f ) weights[selectedBone] = target;
			else weights.remove( selectedBone );
		}

		QVector<QPair<int, float>> ordered;
		for ( auto it = weights.constBegin(); it != weights.constEnd(); ++it )
			if ( it.value() > 1.0e-8f ) ordered.append( qMakePair( it.key(), it.value() ) );
		std::sort( ordered.begin(), ordered.end(), []( const auto & a, const auto & b ) {
			return a.second != b.second ? a.second > b.second : a.first < b.first;
		} );
		if ( ordered.size() > 4 ) ordered.resize( 4 );
		float sum = 0.0f;
		for ( const auto & influence : ordered ) sum += influence.second;
		if ( sum <= 1.0e-8f )
			continue;

		RigPaintVertex painted;
		painted.vertex = vertex;
		for ( int slot = 0; slot < ordered.size(); slot++ ) {
			painted.weights[slot] = ordered.at( slot ).second / sum;
			painted.bones[slot] = quint8( ordered.at( slot ).first );
		}
		bool changed = false;
		for ( int slot = 0; slot < 4; slot++ )
			changed = changed || painted.bones[slot] != oldBones.at( vertex )[slot]
				|| std::fabs( painted.weights[slot] - oldWeights.at( vertex )[slot] ) > 1.0e-7f;
		if ( changed ) result.append( painted );
	}
	return true;
}

static int riggingApplyPaintStroke( NifModel * nif, const QModelIndex & shape,
	int selectedBone, const QMap<int, float> & brush, int mode, float paintWeight,
	float strength, bool accumulate, bool & boundsUpdated, QString & error )
{
	QVector<RigPaintVertex> changes;
	if ( !riggingBuildPaintStroke( nif, shape, selectedBone, brush, mode,
		paintWeight, strength, accumulate, changes, error ) )
		return -1;
	if ( changes.isEmpty() )
		return 0;

	QModelIndex iVD = nif->getIndex( shape, "Vertex Data" );
	QModelIndex remap;
	for ( int link : nif->getChildLinks( nif->getBlockNumber( shape ) ) ) {
		QModelIndex extra = nif->getBlockIndex( link, "NiBinaryExtraData" );
		if ( extra.isValid()
			&& nif->get<QString>( extra, "Name" ) == QStringLiteral( "CustomizationRemapData" ) ) {
			remap = extra;
			break;
		}
	}
	bool sculptSkin = false;
	for ( const QString & bone : riggingBoneNames( nif, shape ) )
		if ( bone.startsWith( QStringLiteral( "skin_bone_" ), Qt::CaseInsensitive ) ) sculptSkin = true;

	bool writesOk = true;
	boundsUpdated = false;
	QStringList modeNames = { Spell::tr( "Add" ), Spell::tr( "Subtract" ),
		Spell::tr( "Replace" ), Spell::tr( "Smooth" ) };
	bool snapshot = nifSnapshotOp( nif, Spell::tr( "%1 weights on %2 vertices" )
		.arg( modeNames.value( mode, Spell::tr( "Paint" ) ) ).arg( changes.size() ), [&]() {
		bool previousHold = nif->holdUpdates( true );
		for ( const RigPaintVertex & painted : changes ) {
			QModelIndex iVertex = nif->getIndex( iVD, painted.vertex );
			QModelIndex iW = nif->getIndex( iVertex, "Bone Weights" );
			QModelIndex iI = nif->getIndex( iVertex, "Bone Indices" );
			for ( int slot = 0; slot < 4; slot++ ) {
				writesOk = nif->set<float>( nif->getIndex( iW, slot ), painted.weights[slot] ) && writesOk;
				writesOk = nif->set<quint8>( nif->getIndex( iI, slot ), painted.bones[slot] ) && writesOk;
			}
		}
		boundsUpdated = spUpdateBounds::calculateBoneBounds( nif, QPersistentModelIndex( shape ) );
		// For standard-only skins the blob mirrors the live weights. Face-sculpt
		// skins intentionally retain the pre-transfer standard-skeleton snapshot.
		if ( remap.isValid() && !sculptSkin ) {
			QString remapError;
			QByteArray blob = riggingCustomizationRemapBlob( nif, shape, &remapError );
			writesOk = !blob.isEmpty() && nif->set<QByteArray>( remap, "Binary Data", blob ) && writesOk;
			if ( blob.isEmpty() && error.isEmpty() ) error = remapError;
		}
		nif->holdUpdates( previousHold );
	} );
	if ( !snapshot || !writesOk ) {
		if ( error.isEmpty() ) error = Spell::tr( "The weight stroke could not be stored as an Undo snapshot." );
		return -1;
	}
	return changes.size();
}

static bool riggingReadVertexColors( NifModel * nif, const QModelIndex & shape,
	QVector<Color4> & colors, QString & error )
{
	colors.clear();
	error.clear();
	if ( !nif || !nif->blockInherits( shape, "BSTriShape" ) ) {
		error = Spell::tr( "Vertex Paint currently requires a BSTriShape." );
		return false;
	}
	QModelIndex iVD = nif->getIndex( shape, "Vertex Data" );
	BSVertexDesc desc = nif->get<BSVertexDesc>( shape, "Vertex Desc" );
	if ( !iVD.isValid() || !desc.HasFlag( VertexAttribute::VA_COLOR ) ) {
		error = Spell::tr( "This mesh does not have an initialized Vertex Colors channel." );
		return false;
	}
	const int count = nif->rowCount( iVD );
	colors.reserve( count );
	for ( int vertex = 0; vertex < count; vertex++ ) {
		QModelIndex color = nif->getIndex( nif->getIndex( iVD, vertex ), "Vertex Colors" );
		if ( !color.isValid() ) {
			error = Spell::tr( "Vertex %1 has no readable Vertex Colors field." ).arg( vertex );
			colors.clear();
			return false;
		}
		colors.append( Color4( nif->get<ByteColor4>( color ) ) );
	}
	return true;
}

//! Add the packed RGBA field to FO4 vertex records when necessary. Opaque white
//! is neutral for existing materials and gives both RGB and Alpha paint a safe
//! starting value. The descriptor resize and initialization are one Undo step.
static bool riggingEnsureVertexColors( NifModel * nif, const QModelIndex & shape, QString & error )
{
	error.clear();
	if ( !nif || !nif->blockInherits( shape, "BSTriShape" ) ) {
		error = Spell::tr( "Vertex Paint currently requires a BSTriShape." );
		return false;
	}
	BSVertexDesc desc = nif->get<BSVertexDesc>( shape, "Vertex Desc" );
	if ( desc.HasFlag( VertexAttribute::VA_COLOR ) )
		return true;

	bool writesOk = true;
	bool snapshot = nifSnapshotOp( nif, Spell::tr( "Initialize vertex colors" ), [&]() {
		bool previousHold = nif->holdUpdates( true );
		desc.SetFlag( VertexAttribute::VA_COLOR );
		desc.ResetAttributeOffsets( nif->getBSVersion() );
		if ( nif->blockInherits( shape, "BSDynamicTriShape" ) )
			desc.MakeDynamic();
		writesOk = nif->set<BSVertexDesc>( shape, "Vertex Desc", desc ) && writesOk;
		const quint32 numVerts = nif->get<quint32>( shape, "Num Vertices" );
		const quint32 numTris = nif->get<quint32>( shape, "Num Triangles" );
		QModelIndex dataSize = nif->getIndex( shape, "Data Size" );
		if ( dataSize.isValid() )
			writesOk = nif->set<quint32>( dataSize, desc.GetVertexSize() * numVerts + 6U * numTris ) && writesOk;
		QModelIndex iVD = nif->getIndex( shape, "Vertex Data" );
		if ( iVD.isValid() )
			nif->updateArraySize( iVD );
		for ( int vertex = 0; vertex < nif->rowCount( iVD ); vertex++ ) {
			QModelIndex color = nif->getIndex( nif->getIndex( iVD, vertex ), "Vertex Colors" );
			writesOk = color.isValid()
				&& nif->set<ByteColor4>( color, ByteColor4( FloatVector4( 1.0f ) ) ) && writesOk;
		}
		nif->holdUpdates( previousHold );
	} );
	if ( !snapshot || !writesOk ) {
		error = Spell::tr( "The Vertex Colors channel could not be initialized as one Undo step." );
		return false;
	}
	return true;
}

static int riggingCommitVertexColors( NifModel * nif, const QModelIndex & shape,
	const QVector<Color4> & colors, const QSet<int> & changed, QString & error )
{
	error.clear();
	QModelIndex iVD = nif ? nif->getIndex( shape, "Vertex Data" ) : QModelIndex();
	if ( !iVD.isValid() || colors.size() != nif->rowCount( iVD ) ) {
		error = Spell::tr( "The vertex-color stroke no longer matches the target mesh." );
		return -1;
	}
	QVector<int> ordered = changed.values();
	std::sort( ordered.begin(), ordered.end() );
	bool writesOk = true;
	bool snapshot = nifSnapshotOp( nif, Spell::tr( "Paint vertex colors on %1 vertices" ).arg( ordered.size() ), [&]() {
		bool previousHold = nif->holdUpdates( true );
		for ( int vertex : ordered ) {
			if ( vertex < 0 || vertex >= colors.size() ) continue;
			QModelIndex color = nif->getIndex( nif->getIndex( iVD, vertex ), "Vertex Colors" );
			writesOk = color.isValid()
				&& nif->set<ByteColor4>( color, ByteColor4( FloatVector4( colors.at( vertex ) ) ) ) && writesOk;
		}
		nif->holdUpdates( previousHold );
	} );
	if ( !snapshot || !writesOk ) {
		error = Spell::tr( "The vertex-color stroke could not be stored as one Undo step." );
		return -1;
	}
	return ordered.size();
}

struct RiggingContextBoneRef
{
	int donorBlock = -1;
	int donorSlot = -1;
};

struct RiggingContextSegmentRef
{
	int donorBlock = -1;
	int segment = -1;
	int subsegment = -1;
};

//! Transfer one binary segment channel from one or more selected secondary
//! meshes. Each receiver face is compared with the complete donor surfaces;
//! membership is copied from the nearest donor face. Faces removed from the
//! active receiver channel safely fall back to Segment 0.
static int riggingTransferContextSegments( NifModel * nif, int targetBlock,
	int targetSegment, int targetSubsegment, const QVector<RiggingContextSegmentRef> & requested,
	QString & error )
{
	error.clear();
	QModelIndex targetIndex = nif->getBlockIndex( targetBlock );
	if ( !nif->blockInherits( targetIndex, "BSSubIndexTriShape" ) ) {
		error = Spell::tr( "The receiver is not a BSSubIndexTriShape." );
		return -1;
	}
	QVector<RigSegmentDefinition> definitions = riggingReadSegmentDefinitions( nif, targetIndex );
	if ( targetSegment < 0 || targetSegment >= definitions.size()
		|| ( targetSubsegment >= 0 && targetSubsegment >= definitions.at( targetSegment ).children.size() ) ) {
		error = Spell::tr( "The active receiver segment no longer exists." );
		return -1;
	}
	RigShape target = riggingReadShape( nif, targetIndex );
	Transform targetWorld;
	if ( !target.valid || !riggingNodeAbsoluteWorld( nif, targetBlock, targetWorld ) ) {
		error = Spell::tr( "The receiver geometry or transform could not be read." );
		return -1;
	}

	QMap<int, QVector<RiggingContextSegmentRef>> byDonor;
	for ( const RiggingContextSegmentRef & ref : requested )
		if ( ref.donorBlock >= 0 && ref.donorBlock != targetBlock && ref.segment >= 0 )
			byDonor[ref.donorBlock].append( ref );
	if ( byDonor.isEmpty() ) return 0;

	struct DonorFace { Vector3 a, b, c; bool member = false; };
	QVector<DonorFace> donorFaces;
	for ( auto donorIt = byDonor.constBegin(); donorIt != byDonor.constEnd(); ++donorIt ) {
		QModelIndex donorIndex = nif->getBlockIndex( donorIt.key() );
		if ( !nif->blockInherits( donorIndex, "BSSubIndexTriShape" ) ) {
			error = Spell::tr( "Secondary block %1 is not a BSSubIndexTriShape." ).arg( donorIt.key() );
			return -1;
		}
		RigShape donor = riggingReadShape( nif, donorIndex );
		Transform donorWorld;
		if ( !donor.valid || !riggingNodeAbsoluteWorld( nif, donorIt.key(), donorWorld ) ) {
			error = Spell::tr( "Secondary block %1 has unreadable geometry or transforms." ).arg( donorIt.key() );
			return -1;
		}
		QVector<int> segmentOf, subsegmentOf;
		riggingReadTriangleMembership( nif, donorIndex, segmentOf, subsegmentOf );
		for ( int triangle = 0; triangle < donor.tris.size(); triangle++ ) {
			const Triangle & tri = donor.tris.at( triangle );
			bool member = false;
			for ( const RiggingContextSegmentRef & ref : donorIt.value() ) {
				if ( segmentOf.value( triangle ) == ref.segment
					&& ( ref.subsegment < 0 || subsegmentOf.value( triangle ) == ref.subsegment ) ) {
					member = true;
					break;
				}
			}
			donorFaces.append( { donorWorld * donor.pos.at( tri.v1() ),
				donorWorld * donor.pos.at( tri.v2() ), donorWorld * donor.pos.at( tri.v3() ), member } );
		}
	}
	if ( donorFaces.isEmpty() ) {
		error = Spell::tr( "The selected donors contain no readable triangles." );
		return -1;
	}

	QVector<int> segmentOf, subsegmentOf;
	riggingReadTriangleMembership( nif, targetIndex, segmentOf, subsegmentOf );
	int changed = 0;
	for ( int triangle = 0; triangle < target.tris.size(); triangle++ ) {
		const Triangle & tri = target.tris.at( triangle );
		Vector3 center = targetWorld * ( ( target.pos.at( tri.v1() )
			+ target.pos.at( tri.v2() ) + target.pos.at( tri.v3() ) ) * ( 1.0f / 3.0f ) );
		float bestDistance = std::numeric_limits<float>::max();
		bool nearestMember = false;
		for ( const DonorFace & face : donorFaces ) {
			float bary[3];
			Vector3 closest = riggingClosestOnTri( center, face.a, face.b, face.c, bary );
			float distance = ( center - closest ).squaredLength();
			if ( distance < bestDistance ) {
				bestDistance = distance;
				nearestMember = face.member;
			}
		}
		const bool currentMember = segmentOf.value( triangle ) == targetSegment
			&& ( targetSubsegment < 0 || subsegmentOf.value( triangle ) == targetSubsegment );
		if ( nearestMember ) {
			if ( segmentOf[triangle] != targetSegment || subsegmentOf[triangle] != targetSubsegment ) changed++;
			segmentOf[triangle] = targetSegment;
			subsegmentOf[triangle] = targetSubsegment;
		} else if ( currentMember && ( segmentOf[triangle] != 0 || subsegmentOf[triangle] != -1 ) ) {
			segmentOf[triangle] = 0;
			subsegmentOf[triangle] = -1;
			changed++;
		}
	}
	if ( changed == 0 ) return 0;
	bool snapshot = nifSnapshotOp( nif, Spell::tr( "Transfer binary segment membership on %1 face(s)" ).arg( changed ), [&]() {
		return riggingWriteSegmentLayout( nif, targetIndex, definitions, segmentOf, subsegmentOf, error );
	} );
	if ( !snapshot ) {
		if ( error.isEmpty() ) error = Spell::tr( "Segment membership could not be stored as one Undo step." );
		return -1;
	}
	return changed;
}

struct RiggingContextBoneCopy
{
	QString name;
	int node = -1;
	Vector3 sphereCenter;
	float sphereRadius = 0.0f;
	Transform transform;
};

//! Append selected bindings from secondary meshes in the current NIF to the
//! primary mesh. Donors are never modified. Requiring one shared skeleton root
//! makes the operation an exact binding copy rather than a scene-graph import.
static int riggingBindContextBones( NifModel * nif, int targetBlock,
	const QVector<RiggingContextBoneRef> & requested, QString & error )
{
	error.clear();
	QModelIndex iTargetShape = nif->getBlockIndex( targetBlock );
	QModelIndex iTargetSkin = riggingSkinInstance( nif, iTargetShape );
	QModelIndex iTargetBones = nif->getIndex( iTargetSkin, "Bones" );
	QModelIndex iTargetData = nif->getBlockIndex( nif->getLink( iTargetSkin, "Data" ) );
	QModelIndex iTargetList = nif->getIndex( iTargetData, "Bone List" );
	if ( nif->getBSVersion() != 130 || !riggingIsSkinnedShape( nif, iTargetShape )
		|| !nif->isNiBlock( iTargetSkin, "BSSkin::Instance" )
		|| !nif->isNiBlock( iTargetData, "BSSkin::BoneData" )
		|| !iTargetBones.isValid() || !iTargetList.isValid() ) {
		error = Spell::tr( "The primary receiver does not have complete FO4 BSSkin data." );
		return -1;
	}

	int targetCount = nif->rowCount( iTargetBones );
	if ( targetCount != nif->rowCount( iTargetList )
		|| nif->get<quint32>( iTargetSkin, "Num Bones" ) != quint32( targetCount )
		|| nif->get<quint32>( iTargetData, "Num Bones" ) != quint32( targetCount )
		|| targetCount > 256 || nif->get<quint32>( iTargetSkin, "Num Scales" ) != 0 ) {
		error = Spell::tr( "The receiver bone counts are inconsistent, exceed 256, or use unsupported BSSkin Scales." );
		return -1;
	}

	int targetSkinBlock = nif->getBlockNumber( iTargetSkin );
	int targetDataBlock = nif->getBlockNumber( iTargetData );
	int targetSkinOwners = 0, targetDataOwners = 0;
	for ( int b = 0; b < nif->getBlockCount(); b++ ) {
		QModelIndex iBlock = nif->getBlockIndex( b );
		if ( riggingIsTriShape( nif, iBlock ) && nif->getLink( iBlock, "Skin" ) == targetSkinBlock )
			targetSkinOwners++;
		if ( nif->isNiBlock( iBlock, "BSSkin::Instance" ) && nif->getLink( iBlock, "Data" ) == targetDataBlock )
			targetDataOwners++;
	}
	if ( targetSkinOwners != 1 || targetDataOwners != 1 ) {
		error = Spell::tr( "The receiver Skin or BoneData is shared; adding a binding would also alter another mesh." );
		return -1;
	}

	int targetRoot = nif->getLink( iTargetSkin, "Skeleton Root" );
	QSet<int> targetTree;
	riggingCollectNodeTree( nif, targetRoot, targetTree );
	if ( targetTree.isEmpty() ) {
		error = Spell::tr( "The receiver has no readable NiNode skeleton-root tree." );
		return -1;
	}

	QStringList targetNames = riggingBoneNames( nif, iTargetShape );
	QMap<QString, int> targetSlots;
	for ( int slot = 0; slot < targetNames.size(); slot++ ) {
		const QString & name = targetNames.at( slot );
		if ( name.isEmpty() ) {
			error = Spell::tr( "Receiver bone slot %1 is unnamed." ).arg( slot );
			return -1;
		}
		if ( targetSlots.contains( name ) ) {
			error = Spell::tr( "Receiver contains duplicate bone name '%1'." ).arg( name );
			return -1;
		}
		int node = nif->getLink( nif->getIndex( iTargetBones, slot ) );
		if ( !targetTree.contains( node ) || !nif->blockInherits( nif->getBlockIndex( node ), "NiNode" ) ) {
			error = Spell::tr( "Receiver bone '%1' is invalid or outside Skeleton Root." ).arg( name );
			return -1;
		}
		targetSlots.insert( name, slot );
	}

	QMap<int, QVector<int>> slotsByDonor;
	for ( const RiggingContextBoneRef & ref : requested )
		if ( ref.donorBlock >= 0 && ref.donorSlot >= 0 )
			slotsByDonor[ref.donorBlock].append( ref.donorSlot );
	if ( slotsByDonor.isEmpty() )
		return 0;

	QMap<QString, RiggingContextBoneCopy> additions;
	for ( auto donorIt = slotsByDonor.constBegin(); donorIt != slotsByDonor.constEnd(); ++donorIt ) {
		int donorBlock = donorIt.key();
		if ( donorBlock == targetBlock )
			continue;
		QModelIndex iDonorShape = nif->getBlockIndex( donorBlock );
		QModelIndex iDonorSkin = riggingSkinInstance( nif, iDonorShape );
		QModelIndex iDonorBones = nif->getIndex( iDonorSkin, "Bones" );
		QModelIndex iDonorData = nif->getBlockIndex( nif->getLink( iDonorSkin, "Data" ) );
		QModelIndex iDonorList = nif->getIndex( iDonorData, "Bone List" );
		if ( !riggingIsSkinnedShape( nif, iDonorShape )
			|| !nif->isNiBlock( iDonorSkin, "BSSkin::Instance" )
			|| !nif->isNiBlock( iDonorData, "BSSkin::BoneData" )
			|| !iDonorBones.isValid() || !iDonorList.isValid() ) {
			error = Spell::tr( "Secondary mesh at block %1 has incomplete FO4 BSSkin data." ).arg( donorBlock );
			return -1;
		}
		int donorCount = nif->rowCount( iDonorBones );
		if ( donorCount != nif->rowCount( iDonorList )
			|| nif->get<quint32>( iDonorSkin, "Num Bones" ) != quint32( donorCount )
			|| nif->get<quint32>( iDonorData, "Num Bones" ) != quint32( donorCount )
			|| donorCount > 256 || nif->get<quint32>( iDonorSkin, "Num Scales" ) != 0 ) {
			error = Spell::tr( "Secondary mesh at block %1 has inconsistent bone counts or unsupported scales." ).arg( donorBlock );
			return -1;
		}
		if ( nif->getLink( iDonorSkin, "Skeleton Root" ) != targetRoot ) {
			error = Spell::tr( "Secondary mesh at block %1 does not share the receiver's Skeleton Root." ).arg( donorBlock );
			return -1;
		}

		QStringList donorNames = riggingBoneNames( nif, iDonorShape );
		QMap<QString, int> donorSlots;
		for ( int slot = 0; slot < donorNames.size(); slot++ ) {
			const QString & name = donorNames.at( slot );
			if ( name.isEmpty() || donorSlots.contains( name ) ) {
				error = name.isEmpty()
					? Spell::tr( "Secondary block %1 has an unnamed bone at slot %2." ).arg( donorBlock ).arg( slot )
					: Spell::tr( "Secondary block %1 contains duplicate bone name '%2'." ).arg( donorBlock ).arg( name );
				return -1;
			}
			donorSlots.insert( name, slot );
		}

		int compared = 0;
		float maxDelta = 0.0f;
		for ( auto nameIt = donorSlots.constBegin(); nameIt != donorSlots.constEnd(); ++nameIt ) {
			auto targetIt = targetSlots.constFind( nameIt.key() );
			if ( targetIt == targetSlots.constEnd() )
				continue;
			int donorNode = nif->getLink( nif->getIndex( iDonorBones, nameIt.value() ) );
			int targetNode = nif->getLink( nif->getIndex( iTargetBones, targetIt.value() ) );
			if ( donorNode != targetNode ) {
				error = Spell::tr( "Shared bone '%1' resolves to different NiNodes." ).arg( nameIt.key() );
				return -1;
			}
			Transform donorTransform( nif, nif->getIndex( iDonorList, nameIt.value() ) );
			Transform targetTransform( nif, nif->getIndex( iTargetList, targetIt.value() ) );
			maxDelta = qMax( maxDelta, riggingTransformDelta( donorTransform, targetTransform ) );
			compared++;
		}
		if ( compared == 0 || maxDelta > 0.001f ) {
			error = compared == 0
				? Spell::tr( "Secondary block %1 has no shared bone for bind-space validation." ).arg( donorBlock )
				: Spell::tr( "Secondary block %1 has incompatible shared BoneData (delta %2)." )
					.arg( donorBlock ).arg( maxDelta, 0, 'g', 4 );
			return -1;
		}

		QSet<int> uniqueSlots;
		for ( int slot : donorIt.value() )
			uniqueSlots << slot;
		for ( int slot : uniqueSlots ) {
			if ( slot < 0 || slot >= donorCount ) {
				error = Spell::tr( "Secondary block %1 no longer has bone slot %2." ).arg( donorBlock ).arg( slot );
				return -1;
			}
			const QString & name = donorNames.at( slot );
			if ( targetSlots.contains( name ) )
				continue;
			int node = nif->getLink( nif->getIndex( iDonorBones, slot ) );
			if ( !targetTree.contains( node ) || !nif->blockInherits( nif->getBlockIndex( node ), "NiNode" ) ) {
				error = Spell::tr( "Donor bone '%1' is invalid or outside the shared Skeleton Root." ).arg( name );
				return -1;
			}
			QModelIndex sourceEntry = nif->getIndex( iDonorList, slot );
			QModelIndex sourceSphere = nif->getIndex( sourceEntry, "Bounding Sphere" );
			if ( !sourceEntry.isValid() || !sourceSphere.isValid() || !Transform::canConstruct( nif, sourceEntry ) ) {
				error = Spell::tr( "Donor BoneData for '%1' is incomplete." ).arg( name );
				return -1;
			}
			RiggingContextBoneCopy copy;
			copy.name = name;
			copy.node = node;
			copy.sphereCenter = nif->get<Vector3>( sourceSphere, "Center" );
			copy.sphereRadius = nif->get<float>( sourceSphere, "Radius" );
			copy.transform = Transform( nif, sourceEntry );
			if ( !std::isfinite( copy.sphereCenter.squaredLength() )
				|| !std::isfinite( copy.sphereRadius ) || copy.sphereRadius < 0.0f
				|| !std::isfinite( riggingTransformDelta( copy.transform, Transform() ) ) ) {
				error = Spell::tr( "Donor BoneData for '%1' contains non-finite values." ).arg( name );
				return -1;
			}
			if ( additions.contains( name ) ) {
				const RiggingContextBoneCopy & prior = additions.value( name );
				Vector3 centerDelta = prior.sphereCenter - copy.sphereCenter;
				if ( prior.node != copy.node || riggingTransformDelta( prior.transform, copy.transform ) > 0.001f
					|| centerDelta.squaredLength() > 1.0e-6f
					|| std::fabs( prior.sphereRadius - copy.sphereRadius ) > 0.001f ) {
					error = Spell::tr( "Selected donors disagree about binding data for bone '%1'." ).arg( name );
					return -1;
				}
			} else {
				additions.insert( name, copy );
			}
		}
	}

	if ( additions.isEmpty() )
		return 0;
	if ( targetCount + additions.size() > 256 ) {
		error = Spell::tr( "Adding %1 bones to the receiver's %2 would exceed the 256-bone limit." )
			.arg( additions.size() ).arg( targetCount );
		return -1;
	}

	const int newCount = targetCount + additions.size();
	bool changed = nifSnapshotOp( nif,
		Spell::tr( "Add %1 selected bone binding(s)" ).arg( additions.size() ), [&]() {
			QPersistentModelIndex pSkin = iTargetSkin;
			QPersistentModelIndex pData = iTargetData;
			bool previousHold = nif->holdUpdates( true );
			nif->set<quint32>( pSkin, "Num Bones", quint32( newCount ) );
			iTargetBones = nif->getIndex( pSkin, "Bones" );
			nif->updateArraySize( iTargetBones );
			nif->set<quint32>( pData, "Num Bones", quint32( newCount ) );
			iTargetList = nif->getIndex( pData, "Bone List" );
			nif->updateArraySize( iTargetList );

			int offset = 0;
			for ( auto it = additions.constBegin(); it != additions.constEnd(); ++it, ++offset ) {
				const RiggingContextBoneCopy & copy = it.value();
				int targetSlot = targetCount + offset;
				nif->setLink( nif->getIndex( iTargetBones, targetSlot ), copy.node );
				QModelIndex targetEntry = nif->getIndex( iTargetList, targetSlot );
				QModelIndex targetSphere = nif->getIndex( targetEntry, "Bounding Sphere" );
				nif->set<Vector3>( targetSphere, "Center", copy.sphereCenter );
				nif->set<float>( targetSphere, "Radius", copy.sphereRadius );
				copy.transform.writeBack( nif, targetEntry );
			}
			nif->holdUpdates( previousHold );
		} );
	if ( !changed ) {
		error = Spell::tr( "Could not create an Undo snapshot for the binding change." );
		return -1;
	}
	return additions.size();
}

//! Author a brand-new NiNode beside the active/template bone and append a new
//! zero-influence receiver binding. Name, node, and BSSkin structure are stored
//! together in one snapshot; no donor node is reused.
static int riggingAddNewReceiverBone( NifModel * nif, int targetBlock,
	int templateSlot, const QString & requestedName, QString & error )
{
	error.clear();
	const QString newName = requestedName.trimmed();
	QModelIndex shape = nif->getBlockIndex( targetBlock );
	QModelIndex skin = riggingSkinInstance( nif, shape );
	QModelIndex bones = nif->getIndex( skin, "Bones" );
	QModelIndex data = nif->getBlockIndex( nif->getLink( skin, "Data" ) );
	QModelIndex boneList = nif->getIndex( data, "Bone List" );
	if ( newName.isEmpty() || !nif->isNiBlock( skin, "BSSkin::Instance" )
		|| !nif->isNiBlock( data, "BSSkin::BoneData" )
		|| !bones.isValid() || !boneList.isValid() ) {
		error = Spell::tr( "A non-empty name and complete receiver BSSkin data are required." );
		return -1;
	}
	QStringList names = riggingBoneNames( nif, shape );
	for ( const QString & name : names )
		if ( name.compare( newName, Qt::CaseInsensitive ) == 0 ) {
			error = Spell::tr( "The receiver already contains a bone named '%1'." ).arg( newName );
			return -1;
		}
	const int count = nif->rowCount( bones );
	if ( count != nif->rowCount( boneList ) || count >= 256
		|| nif->get<quint32>( skin, "Num Bones" ) != quint32( count )
		|| nif->get<quint32>( data, "Num Bones" ) != quint32( count )
		|| templateSlot < 0 || templateSlot >= count ) {
		error = Spell::tr( "The receiver bone arrays are inconsistent, full, or have no valid template bone." );
		return -1;
	}
	int skinOwners = 0, dataOwners = 0;
	const int skinBlock = nif->getBlockNumber( skin );
	const int dataBlock = nif->getBlockNumber( data );
	for ( int block = 0; block < nif->getBlockCount(); block++ ) {
		QModelIndex candidate = nif->getBlockIndex( block );
		if ( riggingIsTriShape( nif, candidate ) && nif->getLink( candidate, "Skin" ) == skinBlock ) skinOwners++;
		if ( nif->isNiBlock( candidate, "BSSkin::Instance" )
			&& nif->getLink( candidate, "Data" ) == dataBlock ) dataOwners++;
	}
	if ( skinOwners != 1 || dataOwners != 1 ) {
		error = Spell::tr( "The receiver Skin or BoneData is shared; adding a binding would restructure another mesh." );
		return -1;
	}
	const int templateNode = nif->getLink( nif->getIndex( bones, templateSlot ) );
	QModelIndex sourceNode = nif->getBlockIndex( templateNode );
	QModelIndex sourceEntry = nif->getIndex( boneList, templateSlot );
	if ( !nif->blockInherits( sourceNode, "NiNode" ) || !Transform::canConstruct( nif, sourceNode )
		|| !Transform::canConstruct( nif, sourceEntry ) ) {
		error = Spell::tr( "The active receiver bone cannot be used as a transform template." );
		return -1;
	}
	const int parent = riggingSceneParent( nif, templateNode );
	if ( parent < 0 ) {
		error = Spell::tr( "The active bone must have exactly one NiNode parent before a sibling bone can be created." );
		return -1;
	}
	Transform nodeTransform( nif, sourceNode );
	Transform skinTransform( nif, sourceEntry );
	const quint32 flags = nif->get<quint32>( sourceNode, "Flags" );
	bool writesOk = true;
	int newNode = -1;
	bool snapshot = nifSnapshotOp( nif, Spell::tr( "Add new receiver bone '%1'" ).arg( newName ), [&]() {
		QPersistentModelIndex pSkin = skin, pData = data;
		bool previousHold = nif->holdUpdates( true );
		QModelIndex created = nif->insertNiBlock( "NiNode", -1 );
		writesOk = created.isValid() && writesOk;
		if ( created.isValid() ) {
			nif->assignString( created, "Name", newName, false );
			writesOk = nif->set<quint32>( created, "Flags", flags ) && writesOk;
			nodeTransform.writeBack( nif, created );
			newNode = nif->getBlockNumber( created );
			writesOk = addLink( nif, nif->getBlockIndex( parent ), "Children", newNode ) && writesOk;
		}
		writesOk = nif->set<quint32>( pSkin, "Num Bones", quint32( count + 1 ) ) && writesOk;
		bones = nif->getIndex( pSkin, "Bones" );
		nif->updateArraySize( bones );
		writesOk = nif->set<quint32>( pData, "Num Bones", quint32( count + 1 ) ) && writesOk;
		boneList = nif->getIndex( pData, "Bone List" );
		nif->updateArraySize( boneList );
		if ( newNode >= 0 ) {
			writesOk = nif->setLink( nif->getIndex( bones, count ), newNode ) && writesOk;
			QModelIndex entry = nif->getIndex( boneList, count );
			QModelIndex sphere = nif->getIndex( entry, "Bounding Sphere" );
			writesOk = nif->set<Vector3>( sphere, "Center", Vector3() ) && writesOk;
			writesOk = nif->set<float>( sphere, "Radius", 0.0f ) && writesOk;
			skinTransform.writeBack( nif, entry );
		}
		nif->holdUpdates( previousHold );
	} );
	if ( !snapshot || !writesOk || newNode < 0 ) {
		error = Spell::tr( "The new NiNode and receiver binding could not be created as one Undo step." );
		return -1;
	}
	return count;
}

//! Remove selected receiver bindings and their vertex influences. Skeleton
//! NiNodes are intentionally retained because other shapes can share them.
//! The operation refuses to orphan a vertex with no remaining influence.
static int riggingRemoveContextBones( NifModel * nif, int targetBlock,
	const QSet<int> & requestedSlots, bool & boundsUpdated, QString & error )
{
	error.clear();
	boundsUpdated = false;
	QModelIndex shape = nif->getBlockIndex( targetBlock );
	QModelIndex skin = riggingSkinInstance( nif, shape );
	QModelIndex bones = nif->getIndex( skin, "Bones" );
	QModelIndex data = nif->getBlockIndex( nif->getLink( skin, "Data" ) );
	QModelIndex boneList = nif->getIndex( data, "Bone List" );
	QModelIndex vertexData = nif->getIndex( shape, "Vertex Data" );
	if ( nif->getBSVersion() != 130 || !riggingIsSkinnedShape( nif, shape )
		|| !nif->isNiBlock( skin, "BSSkin::Instance" )
		|| !nif->isNiBlock( data, "BSSkin::BoneData" )
		|| !bones.isValid() || !boneList.isValid() || !vertexData.isValid() ) {
		error = Spell::tr( "The receiver does not have complete FO4 BSSkin data." );
		return -1;
	}
	const int oldCount = nif->rowCount( bones );
	if ( oldCount != nif->rowCount( boneList )
		|| nif->get<quint32>( skin, "Num Bones" ) != quint32( oldCount )
		|| nif->get<quint32>( data, "Num Bones" ) != quint32( oldCount ) ) {
		error = Spell::tr( "The receiver bone counts are inconsistent." );
		return -1;
	}
	int skinOwners = 0, dataOwners = 0;
	const int skinBlock = nif->getBlockNumber( skin );
	const int dataBlock = nif->getBlockNumber( data );
	for ( int block = 0; block < nif->getBlockCount(); block++ ) {
		QModelIndex candidate = nif->getBlockIndex( block );
		if ( riggingIsTriShape( nif, candidate ) && nif->getLink( candidate, "Skin" ) == skinBlock )
			skinOwners++;
		if ( nif->isNiBlock( candidate, "BSSkin::Instance" )
			&& nif->getLink( candidate, "Data" ) == dataBlock ) dataOwners++;
	}
	if ( skinOwners != 1 || dataOwners != 1 ) {
		error = Spell::tr( "The receiver Skin or BoneData is shared; removing a binding would also restructure another mesh." );
		return -1;
	}
	QSet<int> remove;
	for ( int slot : requestedSlots )
		if ( slot >= 0 && slot < oldCount ) remove.insert( slot );
	if ( remove.isEmpty() )
		return 0;
	if ( remove.size() >= oldCount ) {
		error = Spell::tr( "A skinned receiver must retain at least one bone binding." );
		return -1;
	}
	QSet<int> removedNodeSet;
	for ( int slot : remove )
		removedNodeSet.insert( nif->getLink( nif->getIndex( bones, slot ) ) );
	QVector<int> removedNodes( removedNodeSet.begin(), removedNodeSet.end() );

	QVector<int> oldToNew( oldCount, -1 );
	QVector<RiggingContextBoneCopy> remaining;
	remaining.reserve( oldCount - remove.size() );
	QStringList names = riggingBoneNames( nif, shape );
	for ( int oldSlot = 0; oldSlot < oldCount; oldSlot++ ) {
		if ( remove.contains( oldSlot ) )
			continue;
		QModelIndex entry = nif->getIndex( boneList, oldSlot );
		QModelIndex sphere = nif->getIndex( entry, "Bounding Sphere" );
		if ( !entry.isValid() || !sphere.isValid() || !Transform::canConstruct( nif, entry ) ) {
			error = Spell::tr( "Receiver BoneData slot %1 is incomplete." ).arg( oldSlot );
			return -1;
		}
		oldToNew[oldSlot] = remaining.size();
		RiggingContextBoneCopy copy;
		copy.name = names.value( oldSlot );
		copy.node = nif->getLink( nif->getIndex( bones, oldSlot ) );
		copy.sphereCenter = nif->get<Vector3>( sphere, "Center" );
		copy.sphereRadius = nif->get<float>( sphere, "Radius" );
		copy.transform = Transform( nif, entry );
		remaining.append( copy );
	}

	QVector<RigPaintVertex> rewritten;
	rewritten.reserve( nif->rowCount( vertexData ) );
	for ( int vertex = 0; vertex < nif->rowCount( vertexData ); vertex++ ) {
		QModelIndex row = nif->getIndex( vertexData, vertex );
		QModelIndex weights = nif->getIndex( row, "Bone Weights" );
		QModelIndex indices = nif->getIndex( row, "Bone Indices" );
		if ( !weights.isValid() || !indices.isValid()
			|| nif->rowCount( weights ) != 4 || nif->rowCount( indices ) != 4 ) {
			error = Spell::tr( "Receiver vertex %1 does not have four skin slots." ).arg( vertex );
			return -1;
		}
		QMap<int, float> keep;
		for ( int influence = 0; influence < 4; influence++ ) {
			float weight = qBound( 0.0f, nif->get<float>( nif->getIndex( weights, influence ) ), 1.0f );
			int oldBone = nif->get<quint8>( nif->getIndex( indices, influence ) );
			if ( weight <= 1.0e-8f )
				continue;
			if ( oldBone < 0 || oldBone >= oldCount ) {
				error = Spell::tr( "Receiver vertex %1 references invalid bone slot %2." ).arg( vertex ).arg( oldBone );
				return -1;
			}
			if ( oldToNew.at( oldBone ) >= 0 )
				keep[oldToNew.at( oldBone )] += weight;
		}
		if ( keep.isEmpty() ) {
			error = Spell::tr( "Removing these bindings would leave vertex %1 with no bone influence. Transfer or reassign that weight first." ).arg( vertex );
			return -1;
		}
		QVector<QPair<int, float>> ordered;
		for ( auto it = keep.constBegin(); it != keep.constEnd(); ++it )
			ordered.append( qMakePair( it.key(), it.value() ) );
		std::sort( ordered.begin(), ordered.end(), []( const auto & a, const auto & b ) {
			return a.second != b.second ? a.second > b.second : a.first < b.first;
		} );
		if ( ordered.size() > 4 ) ordered.resize( 4 );
		float sum = 0.0f;
		for ( const auto & influence : ordered ) sum += influence.second;
		RigPaintVertex out;
		out.vertex = vertex;
		for ( int influence = 0; influence < ordered.size(); influence++ ) {
			out.bones[influence] = quint8( ordered.at( influence ).first );
			out.weights[influence] = ordered.at( influence ).second / sum;
		}
		rewritten.append( out );
	}

	QModelIndex remap;
	for ( int link : nif->getChildLinks( targetBlock ) ) {
		QModelIndex extra = nif->getBlockIndex( link, "NiBinaryExtraData" );
		if ( extra.isValid() && nif->get<QString>( extra, "Name" ) == QStringLiteral( "CustomizationRemapData" ) ) {
			remap = extra;
			break;
		}
	}
	bool sculptSkin = false;
	for ( const QString & name : names )
		if ( name.startsWith( QStringLiteral( "skin_bone_" ), Qt::CaseInsensitive ) ) sculptSkin = true;
	QPersistentModelIndex persistentRemap = remap;
	bool writesOk = true;
	const int newCount = remaining.size();
	bool snapshot = nifSnapshotOp( nif, Spell::tr( "Remove %1 receiver bone binding(s)" ).arg( remove.size() ), [&]() {
		QPersistentModelIndex pSkin = skin, pData = data, pShape = shape;
		bool previousHold = nif->holdUpdates( true );
		writesOk = nif->set<quint32>( pSkin, "Num Bones", quint32( newCount ) ) && writesOk;
		bones = nif->getIndex( pSkin, "Bones" );
		nif->updateArraySize( bones );
		writesOk = nif->set<quint32>( pData, "Num Bones", quint32( newCount ) ) && writesOk;
		boneList = nif->getIndex( pData, "Bone List" );
		nif->updateArraySize( boneList );
		for ( int slot = 0; slot < remaining.size(); slot++ ) {
			const RiggingContextBoneCopy & copy = remaining.at( slot );
			writesOk = nif->setLink( nif->getIndex( bones, slot ), copy.node ) && writesOk;
			QModelIndex entry = nif->getIndex( boneList, slot );
			QModelIndex sphere = nif->getIndex( entry, "Bounding Sphere" );
			writesOk = nif->set<Vector3>( sphere, "Center", copy.sphereCenter ) && writesOk;
			writesOk = nif->set<float>( sphere, "Radius", copy.sphereRadius ) && writesOk;
			copy.transform.writeBack( nif, entry );
		}
		vertexData = nif->getIndex( pShape, "Vertex Data" );
		for ( const RigPaintVertex & out : rewritten ) {
			QModelIndex row = nif->getIndex( vertexData, out.vertex );
			QModelIndex weights = nif->getIndex( row, "Bone Weights" );
			QModelIndex indices = nif->getIndex( row, "Bone Indices" );
			for ( int influence = 0; influence < 4; influence++ ) {
				writesOk = nif->set<float>( nif->getIndex( weights, influence ), out.weights[influence] ) && writesOk;
				writesOk = nif->set<quint8>( nif->getIndex( indices, influence ), out.bones[influence] ) && writesOk;
			}
		}
		// A receiver-local leaf bone can be removed completely. Shared donor
		// nodes, parents with children, controller targets, etc. retain additional
		// parent links and are deliberately kept.
		std::sort( removedNodes.begin(), removedNodes.end(), std::greater<int>() );
		for ( int removedNode : removedNodes ) {
			if ( removedNode < 0 || removedNode >= nif->getBlockCount() ) continue;
			int sceneParent = riggingSceneParent( nif, removedNode );
			QList<int> parents = nif->getParentLinks( removedNode );
			if ( sceneParent >= 0 && parents.size() == 1 && parents.contains( sceneParent )
				&& nif->getLinkArray( nif->getBlockIndex( removedNode ), "Children" ).isEmpty() ) {
				delLink( nif, nif->getBlockIndex( sceneParent ), QStringLiteral( "Children" ), removedNode );
				nif->removeNiBlock( removedNode );
			}
		}
		boundsUpdated = spUpdateBounds::calculateBoneBounds( nif, QPersistentModelIndex( pShape ) );
		if ( persistentRemap.isValid() && !sculptSkin ) {
			QString remapError;
			QByteArray blob = riggingCustomizationRemapBlob( nif, pShape, &remapError );
			writesOk = !blob.isEmpty() && nif->set<QByteArray>( persistentRemap, "Binary Data", blob ) && writesOk;
			if ( blob.isEmpty() && error.isEmpty() ) error = remapError;
		}
		nif->holdUpdates( previousHold );
	} );
	if ( !snapshot || !writesOk ) {
		if ( error.isEmpty() ) error = Spell::tr( "The selected bindings could not be removed as an Undo step." );
		return -1;
	}
	return remove.size();
}

//! Project only the selected donor bone channels onto the receiver. When the
//! same bone is selected on several donor meshes, the closest donor surface is
//! chosen independently for each receiver vertex. Unselected receiver channels
//! are preserved, then the four strongest influences are normalized.
static int riggingTransferContextWeights( NifModel * nif, int targetBlock,
	const QVector<RiggingContextBoneRef> & requested, bool & boundsUpdated, QString & error )
{
	error.clear();
	boundsUpdated = false;
	QModelIndex targetIndex = nif->getBlockIndex( targetBlock );
	RigShape target = riggingReadShape( nif, targetIndex );
	if ( !target.valid || target.pos.isEmpty() || target.bones.isEmpty() ) {
		error = Spell::tr( "The primary receiver is not a readable skinned FO4 shape." );
		return -1;
	}
	QMap<QString, int> targetSlots;
	for ( int slot = 0; slot < target.bones.size(); slot++ ) {
		const QString & name = target.bones.at( slot );
		if ( name.isEmpty() || targetSlots.contains( name ) ) {
			error = name.isEmpty() ? Spell::tr( "Receiver bone slot %1 is unnamed." ).arg( slot )
				: Spell::tr( "Receiver contains duplicate bone name '%1'." ).arg( name );
			return -1;
		}
		targetSlots.insert( name, slot );
	}

	QMap<int, QSet<int>> slotsByDonor;
	for ( const RiggingContextBoneRef & ref : requested )
		if ( ref.donorBlock >= 0 && ref.donorBlock != targetBlock && ref.donorSlot >= 0 )
			slotsByDonor[ref.donorBlock].insert( ref.donorSlot );
	if ( slotsByDonor.isEmpty() )
		return 0;

	const int vertices = target.pos.size();
	QVector<QMap<QString, float>> replacement( vertices );
	QVector<QMap<QString, float>> bestDistance( vertices );
	QSet<QString> selectedNames;
	QModelIndex targetSkin = riggingSkinInstance( nif, targetIndex );
	int targetRoot = nif->getLink( targetSkin, "Skeleton Root" );
	for ( auto donorIt = slotsByDonor.constBegin(); donorIt != slotsByDonor.constEnd(); ++donorIt ) {
		QModelIndex donorIndex = nif->getBlockIndex( donorIt.key() );
		RigShape donor = riggingReadShape( nif, donorIndex );
		if ( !donor.valid ) {
			error = Spell::tr( "Secondary mesh at block %1 is not a readable skinned FO4 shape." ).arg( donorIt.key() );
			return -1;
		}
		QModelIndex donorSkin = riggingSkinInstance( nif, donorIndex );
		Transform targetSpace, donorSpace;
		int donorRoot = nif->getLink( donorSkin, "Skeleton Root" );
		if ( donorRoot != targetRoot
			|| !riggingNodeWorld( nif, targetBlock, targetRoot, targetSpace )
			|| !riggingNodeWorld( nif, donorIt.key(), donorRoot, donorSpace )
			|| riggingTransformDelta( targetSpace, donorSpace ) > 0.001f ) {
			error = Spell::tr( "Secondary mesh at block %1 is not in the receiver's skeleton-root-relative space." ).arg( donorIt.key() );
			return -1;
		}
		QSet<QString> donorNames;
		for ( int slot : donorIt.value() ) {
			if ( slot < 0 || slot >= donor.bones.size() || donor.bones.at( slot ).isEmpty() ) {
				error = Spell::tr( "Secondary block %1 no longer has the selected bone slot %2." ).arg( donorIt.key() ).arg( slot );
				return -1;
			}
			const QString & name = donor.bones.at( slot );
			if ( !targetSlots.contains( name ) ) {
				error = Spell::tr( "Receiver is not bound to '%1'. Add the selected binding before transferring its weights." ).arg( name );
				return -1;
			}
			donorNames.insert( name );
			selectedNames.insert( name );
		}

		QVector<float> snap;
		bool cancelled = false;
		auto mapped = riggingTransferWithProgress( donor, target.pos,
			Spell::tr( "Transfer Selected Weights - block %1" ).arg( donorIt.key() ), &snap, &cancelled );
		if ( cancelled ) {
			error = Spell::tr( "Selected-bone weight transfer was cancelled. Nothing was changed." );
			return -1;
		}
		if ( mapped.size() != vertices || snap.size() != vertices ) {
			error = Spell::tr( "The selected donor surface could not be mapped completely." );
			return -1;
		}
		for ( int vertex = 0; vertex < vertices; vertex++ ) {
			QMap<QString, float> mappedWeights;
			for ( const auto & influence : mapped.at( vertex ) )
				mappedWeights[influence.first] += influence.second;
			for ( const QString & name : donorNames ) {
				auto prior = bestDistance[vertex].constFind( name );
				if ( prior == bestDistance[vertex].constEnd() || snap.at( vertex ) < prior.value() ) {
					bestDistance[vertex][name] = snap.at( vertex );
					replacement[vertex][name] = mappedWeights.value( name, 0.0f );
				}
			}
		}
	}
	if ( selectedNames.isEmpty() )
		return 0;

	QModelIndex vertexData = nif->getIndex( targetIndex, "Vertex Data" );
	QVector<RigPaintVertex> changes;
	for ( int vertex = 0; vertex < nif->rowCount( vertexData ); vertex++ ) {
		QModelIndex row = nif->getIndex( vertexData, vertex );
		QModelIndex iWeights = nif->getIndex( row, "Bone Weights" );
		QModelIndex iIndices = nif->getIndex( row, "Bone Indices" );
		if ( !iWeights.isValid() || !iIndices.isValid()
			|| nif->rowCount( iWeights ) != 4 || nif->rowCount( iIndices ) != 4 ) {
			error = Spell::tr( "Receiver vertex %1 does not have four skin slots." ).arg( vertex );
			return -1;
		}
		QMap<int, float> weights;
		std::array<float, 4> oldWeights = {};
		std::array<quint8, 4> oldBones = {};
		for ( int influence = 0; influence < 4; influence++ ) {
			float weight = qBound( 0.0f, nif->get<float>( nif->getIndex( iWeights, influence ) ), 1.0f );
			int bone = nif->get<quint8>( nif->getIndex( iIndices, influence ) );
			oldWeights[influence] = weight;
			oldBones[influence] = quint8( bone );
			if ( weight > 1.0e-8f && bone >= 0 && bone < target.bones.size() )
				weights[bone] += weight;
		}
		for ( const QString & name : selectedNames ) {
			int slot = targetSlots.value( name, -1 );
			float value = replacement.value( vertex ).value( name, 0.0f );
			if ( value > 1.0e-8f ) weights[slot] = value;
			else weights.remove( slot );
		}
		QVector<QPair<int, float>> ordered;
		for ( auto it = weights.constBegin(); it != weights.constEnd(); ++it )
			if ( it.value() > 1.0e-8f ) ordered.append( qMakePair( it.key(), it.value() ) );
		std::sort( ordered.begin(), ordered.end(), []( const auto & a, const auto & b ) {
			return a.second != b.second ? a.second > b.second : a.first < b.first;
		} );
		if ( ordered.size() > 4 ) ordered.resize( 4 );
		float sum = 0.0f;
		for ( const auto & influence : ordered ) sum += influence.second;
		if ( sum <= 1.0e-8f ) {
			error = Spell::tr( "The selected transfer would leave receiver vertex %1 without an influence." ).arg( vertex );
			return -1;
		}
		RigPaintVertex out;
		out.vertex = vertex;
		for ( int influence = 0; influence < ordered.size(); influence++ ) {
			out.bones[influence] = quint8( ordered.at( influence ).first );
			out.weights[influence] = ordered.at( influence ).second / sum;
		}
		bool changed = false;
		for ( int influence = 0; influence < 4; influence++ )
			changed = changed || out.bones[influence] != oldBones[influence]
				|| std::fabs( out.weights[influence] - oldWeights[influence] ) > 1.0e-7f;
		if ( changed ) changes.append( out );
	}
	if ( changes.isEmpty() )
		return 0;

	QModelIndex remap;
	for ( int link : nif->getChildLinks( targetBlock ) ) {
		QModelIndex extra = nif->getBlockIndex( link, "NiBinaryExtraData" );
		if ( extra.isValid() && nif->get<QString>( extra, "Name" ) == QStringLiteral( "CustomizationRemapData" ) ) {
			remap = extra;
			break;
		}
	}
	bool sculptSkin = false;
	for ( const QString & name : target.bones )
		if ( name.startsWith( QStringLiteral( "skin_bone_" ), Qt::CaseInsensitive ) ) sculptSkin = true;
	bool writesOk = true;
	bool snapshot = nifSnapshotOp( nif,
		Spell::tr( "Transfer %1 selected bone channel(s) onto %2 vertices" )
			.arg( selectedNames.size() ).arg( changes.size() ), [&]() {
		bool previousHold = nif->holdUpdates( true );
		for ( const RigPaintVertex & out : changes ) {
			QModelIndex row = nif->getIndex( vertexData, out.vertex );
			QModelIndex iWeights = nif->getIndex( row, "Bone Weights" );
			QModelIndex iIndices = nif->getIndex( row, "Bone Indices" );
			for ( int influence = 0; influence < 4; influence++ ) {
				writesOk = nif->set<float>( nif->getIndex( iWeights, influence ), out.weights[influence] ) && writesOk;
				writesOk = nif->set<quint8>( nif->getIndex( iIndices, influence ), out.bones[influence] ) && writesOk;
			}
		}
		boundsUpdated = spUpdateBounds::calculateBoneBounds( nif, QPersistentModelIndex( targetIndex ) );
		if ( remap.isValid() && !sculptSkin ) {
			QString remapError;
			QByteArray blob = riggingCustomizationRemapBlob( nif, targetIndex, &remapError );
			writesOk = !blob.isEmpty() && nif->set<QByteArray>( remap, "Binary Data", blob ) && writesOk;
			if ( blob.isEmpty() && error.isEmpty() ) error = remapError;
		}
		nif->holdUpdates( previousHold );
	} );
	if ( !snapshot || !writesOk ) {
		if ( error.isEmpty() ) error = Spell::tr( "The selected donor weights could not be stored as an Undo step." );
		return -1;
	}
	return changes.size();
}

//! Build the Rigging workspace manager. It deliberately invokes registered
//! spells through NifSkope::castSpell so the menu, context menu, and workspace
//! share one implementation and one undo/selection path.
QDockWidget * tlCreateRiggingManagerDock( NifModel * nif, QMainWindow * mw, GLView * ogl )
{
	auto * dock = new QDockWidget( QObject::tr( "Rigging Manager" ), mw );
	dock->setObjectName( QStringLiteral( "RiggingManagerDock" ) );
	dock->setAllowedAreas( Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea );

	auto * panel = new QWidget( dock );
	auto * layout = new QVBoxLayout( panel );
	layout->setContentsMargins( 6, 6, 6, 6 );
	layout->setSpacing( 5 );

	auto * targetHeading = new QLabel( QObject::tr( "Primary receiver mesh" ), panel );
	targetHeading->setStyleSheet( QStringLiteral( "QLabel { font-weight: 600; }" ) );
	layout->addWidget( targetHeading );
	auto * targetStatus = new QLabel( QObject::tr( "Select a skinned FO4 BSTriShape." ), panel );
	targetStatus->setWordWrap( true );
	layout->addWidget( targetStatus );

	auto * contextGroup = new QGroupBox( QObject::tr( "Selected-mesh bone transfer" ), panel );
	contextGroup->setObjectName( QStringLiteral( "RiggingContextBoneTransferGroup" ) );
	contextGroup->setSizePolicy( QSizePolicy::Preferred, QSizePolicy::Maximum );
	auto * contextLayout = new QVBoxLayout( contextGroup );
	contextLayout->setContentsMargins( 6, 8, 6, 6 );
	contextLayout->setSpacing( 5 );
	auto * boneFilterRow = new QHBoxLayout;
	boneFilterRow->setSpacing( 5 );
	auto * boneSearch = new QLineEdit( contextGroup );
	boneSearch->setObjectName( QStringLiteral( "RiggingBoneSearch" ) );
	boneSearch->setClearButtonEnabled( true );
	boneSearch->setPlaceholderText( QObject::tr( "Search receiver and donor bones..." ) );
	boneSearch->setToolTip( QObject::tr( "Filter both bone lists by bone or donor mesh name" ) );
	boneFilterRow->addWidget( boneSearch, 1 );
	auto * boneKindFilter = new QComboBox( contextGroup );
	boneKindFilter->setObjectName( QStringLiteral( "RiggingBoneKindFilter" ) );
	boneKindFilter->addItems( { QObject::tr( "All" ), QObject::tr( "Bones" ), QObject::tr( "Segments" ) } );
	boneKindFilter->setToolTip( QObject::tr( "Show all rows, only skin bones, or only segments and subsegments" ) );
	boneKindFilter->setSizeAdjustPolicy( QComboBox::AdjustToContents );
	boneFilterRow->addWidget( boneKindFilter );
	contextLayout->addLayout( boneFilterRow );

	auto * contextTrees = new QHBoxLayout;
	contextTrees->setSpacing( 5 );
	auto * donorTree = new QTreeWidget( contextGroup );
	donorTree->setObjectName( QStringLiteral( "RiggingContextDonorBoneTree" ) );
	donorTree->setHeaderLabels( { QObject::tr( "Donor bones & segments" ), QObject::tr( "State" ) } );
	donorTree->setRootIsDecorated( true );
	donorTree->setUniformRowHeights( true );
	donorTree->setAlternatingRowColors( true );
	donorTree->setSelectionMode( QAbstractItemView::ExtendedSelection );
	donorTree->setSelectionBehavior( QAbstractItemView::SelectRows );
	donorTree->setContextMenuPolicy( Qt::CustomContextMenu );
	donorTree->setDragEnabled( true );
	donorTree->setDragDropMode( QAbstractItemView::DragOnly );
	donorTree->setDefaultDropAction( Qt::CopyAction );
	donorTree->header()->setSectionResizeMode( 0, QHeaderView::Stretch );
	donorTree->header()->setSectionResizeMode( 1, QHeaderView::ResizeToContents );
	donorTree->setMinimumHeight( 135 );
	donorTree->setMaximumHeight( 185 );
	donorTree->setVerticalScrollBarPolicy( Qt::ScrollBarAsNeeded );
	contextTrees->addWidget( donorTree, 1 );

	auto * contextBindButton = new QPushButton( QObject::tr( "Add selected  →" ), contextGroup );
	contextBindButton->setObjectName( QStringLiteral( "RiggingContextBindBonesButton" ) );
	contextBindButton->setEnabled( false );
	contextBindButton->setToolTip( QObject::tr(
		"Add the selected donor bindings to the receiver as one Undo step; donors are not changed" ) );
	auto * boneTree = new RiggingBoneDropTree( contextGroup );
	boneTree->setObjectName( QStringLiteral( "RiggingBoneTree" ) );
	boneTree->setHeaderLabels( { QObject::tr( "# / ID" ), QObject::tr( "Receiver bones & segments" ) } );
	boneTree->setRootIsDecorated( true );
	boneTree->setUniformRowHeights( true );
	boneTree->setAlternatingRowColors( true );
	boneTree->setSelectionMode( QAbstractItemView::ExtendedSelection );
	boneTree->setSelectionBehavior( QAbstractItemView::SelectRows );
	boneTree->setEditTriggers( QAbstractItemView::NoEditTriggers );
	boneTree->setContextMenuPolicy( Qt::CustomContextMenu );
	boneTree->setAcceptDrops( true );
	boneTree->setDragDropMode( QAbstractItemView::DropOnly );
	boneTree->header()->setSectionResizeMode( 0, QHeaderView::ResizeToContents );
	boneTree->header()->setSectionResizeMode( 1, QHeaderView::Stretch );
	boneTree->setMinimumHeight( 135 );
	boneTree->setMaximumHeight( 185 );
	boneTree->setVerticalScrollBarPolicy( Qt::ScrollBarAsNeeded );
	contextTrees->addWidget( boneTree, 1 );
	donorTree->setItemDelegate( new RiggingBoneSelectionDelegate( donorTree ) );
	boneTree->setItemDelegate( new RiggingBoneSelectionDelegate( boneTree ) );
	contextLayout->addLayout( contextTrees );
	auto * contextBoneStatus = new QLabel( QObject::tr(
		"Select a primary mesh and one or more secondary donor meshes in the viewport." ), contextGroup );
	contextBoneStatus->setObjectName( QStringLiteral( "RiggingContextBoneStatus" ) );
	contextBoneStatus->setWordWrap( true );
	contextBoneStatus->setStyleSheet( QStringLiteral( "QLabel { color: palette(mid); }" ) );
	auto * contextActionRow = new QHBoxLayout;
	contextActionRow->addWidget( contextBoneStatus, 1 );
	contextActionRow->addWidget( contextBindButton );
	contextLayout->addLayout( contextActionRow );
	layout->addWidget( contextGroup );
	auto * weightMap = new QCheckBox( QObject::tr( "Show selected weights / segment faces" ), panel );
	weightMap->setObjectName( QStringLiteral( "RiggingWeightHeatmapToggle" ) );
	weightMap->setToolTip( QObject::tr(
		"Bones use a blue-to-red weight heatmap. Segments and subsegments use a binary grey/orange face overlay." ) );
	layout->addWidget( weightMap );
	auto * weightMapStatus = new QLabel( QObject::tr( "Select a skin bone or segment to inspect it." ), panel );
	weightMapStatus->setObjectName( QStringLiteral( "RiggingWeightHeatmapStatus" ) );
	weightMapStatus->setWordWrap( true );
	weightMapStatus->setStyleSheet( QStringLiteral( "QLabel { color: palette(mid); }" ) );
	layout->addWidget( weightMapStatus );

	auto * paintGroup = new QGroupBox( QObject::tr( "Manual weight painting" ), panel );
	paintGroup->setObjectName( QStringLiteral( "RiggingWeightPaintGroup" ) );
	auto * paintLayout = new QVBoxLayout( paintGroup );
	paintLayout->setContentsMargins( 6, 8, 6, 6 );
	paintLayout->setSpacing( 4 );
	auto * paintModeRow = new QHBoxLayout;
	paintModeRow->addWidget( new QLabel( QObject::tr( "Brush" ), paintGroup ) );
	auto * paintMode = new QComboBox( paintGroup );
	paintMode->setObjectName( QStringLiteral( "RiggingWeightPaintMode" ) );
	paintMode->addItems( { QObject::tr( "Add" ), QObject::tr( "Subtract" ),
		QObject::tr( "Replace" ), QObject::tr( "Smooth" ) } );
	paintModeRow->addWidget( paintMode, 1 );
	auto * paintButton = new QPushButton( QObject::tr( "Start Painting" ), paintGroup );
	paintButton->setObjectName( QStringLiteral( "RiggingWeightPaintButton" ) );
	paintButton->setCheckable( true );
	paintButton->setEnabled( false );
	paintModeRow->addWidget( paintButton );
	paintLayout->addLayout( paintModeRow );
	auto addPaintSlider = [=]( const QString & objectName, const QString & caption,
		int minimum, int maximum, int value, QSlider *& slider, QLabel *& valueLabel ) {
		auto * row = new QHBoxLayout;
		row->addWidget( new QLabel( caption, paintGroup ) );
		slider = new QSlider( Qt::Horizontal, paintGroup );
		slider->setObjectName( objectName );
		slider->setRange( minimum, maximum );
		slider->setValue( value );
		valueLabel = new QLabel( QString::number( value ), paintGroup );
		valueLabel->setMinimumWidth( 30 );
		valueLabel->setAlignment( Qt::AlignRight | Qt::AlignVCenter );
		row->addWidget( slider, 1 );
		row->addWidget( valueLabel );
		paintLayout->addLayout( row );
	};
	QSlider * paintWeight = nullptr, * paintStrength = nullptr, * paintRadius = nullptr;
	QLabel * paintWeightValue = nullptr, * paintStrengthValue = nullptr, * paintRadiusValue = nullptr;
	addPaintSlider( QStringLiteral( "RiggingWeightPaintWeight" ), QObject::tr( "Weight" ),
		0, 100, 100, paintWeight, paintWeightValue );
	addPaintSlider( QStringLiteral( "RiggingWeightPaintStrength" ), QObject::tr( "Strength" ),
		1, 100, 25, paintStrength, paintStrengthValue );
	addPaintSlider( QStringLiteral( "RiggingWeightPaintRadius" ), QObject::tr( "Radius" ),
		4, 200, 32, paintRadius, paintRadiusValue );
	auto * paintAccumulate = new QCheckBox( QObject::tr( "Accumulate" ), paintGroup );
	paintAccumulate->setObjectName( QStringLiteral( "RiggingWeightPaintAccumulate" ) );
	paintAccumulate->setToolTip( QObject::tr(
		"Build weight repeatedly while a stroke passes over the same vertex. When off, each vertex is capped to one brush application per LMB drag." ) );
	paintLayout->addWidget( paintAccumulate );
	auto * paintStatus = new QLabel( QObject::tr( "Select a skin bone, then start painting in the viewport." ), paintGroup );
	paintStatus->setObjectName( QStringLiteral( "RiggingWeightPaintStatus" ) );
	paintStatus->setWordWrap( true );
	paintStatus->setStyleSheet( QStringLiteral( "QLabel { color: palette(mid); }" ) );
	paintLayout->addWidget( paintStatus );
	layout->addWidget( paintGroup );

	auto * segmentPaintGroup = new QGroupBox( QObject::tr( "Segment painting" ), panel );
	segmentPaintGroup->setObjectName( QStringLiteral( "RiggingSegmentPaintGroup" ) );
	auto * segmentPaintLayout = new QVBoxLayout( segmentPaintGroup );
	segmentPaintLayout->setContentsMargins( 6, 8, 6, 6 );
	segmentPaintLayout->setSpacing( 4 );
	auto * segmentPaintRow = new QHBoxLayout;
	segmentPaintRow->addWidget( new QLabel( QObject::tr( "Brush" ), segmentPaintGroup ) );
	auto * segmentPaintOperation = new QComboBox( segmentPaintGroup );
	segmentPaintOperation->setObjectName( QStringLiteral( "RiggingSegmentPaintOperation" ) );
	segmentPaintOperation->addItems( { QObject::tr( "Assign" ), QObject::tr( "Remove to Segment 0" ) } );
	segmentPaintRow->addWidget( segmentPaintOperation, 1 );
	auto * segmentPaintButton = new QPushButton( QObject::tr( "Start Painting" ), segmentPaintGroup );
	segmentPaintButton->setObjectName( QStringLiteral( "RiggingSegmentPaintButton" ) );
	segmentPaintButton->setCheckable( true );
	segmentPaintButton->setEnabled( false );
	segmentPaintRow->addWidget( segmentPaintButton );
	segmentPaintLayout->addLayout( segmentPaintRow );
	auto * segmentRadiusRow = new QHBoxLayout;
	segmentRadiusRow->addWidget( new QLabel( QObject::tr( "Radius" ), segmentPaintGroup ) );
	auto * segmentPaintRadius = new QSlider( Qt::Horizontal, segmentPaintGroup );
	segmentPaintRadius->setObjectName( QStringLiteral( "RiggingSegmentPaintRadius" ) );
	segmentPaintRadius->setRange( 4, 200 );
	segmentPaintRadius->setValue( 32 );
	auto * segmentPaintRadiusValue = new QLabel( QStringLiteral( "32 px" ), segmentPaintGroup );
	segmentRadiusRow->addWidget( segmentPaintRadius, 1 );
	segmentRadiusRow->addWidget( segmentPaintRadiusValue );
	segmentPaintLayout->addLayout( segmentRadiusRow );
	auto * segmentPaintStatus = new QLabel( QObject::tr(
		"Select a receiver segment or subsegment, then paint its binary face membership." ), segmentPaintGroup );
	segmentPaintStatus->setObjectName( QStringLiteral( "RiggingSegmentPaintStatus" ) );
	segmentPaintStatus->setWordWrap( true );
	segmentPaintStatus->setStyleSheet( QStringLiteral( "QLabel { color: palette(mid); }" ) );
	segmentPaintLayout->addWidget( segmentPaintStatus );
	layout->addWidget( segmentPaintGroup );

	auto * overlayGroup = new QGroupBox( QObject::tr( "Donor geometry overlay" ), panel );
	auto * overlayLayout = new QVBoxLayout( overlayGroup );
	overlayLayout->setContentsMargins( 6, 8, 6, 6 );
	overlayLayout->setSpacing( 4 );
	auto * overlayStatus = new QLabel( QObject::tr( "No donor geometry loaded." ), overlayGroup );
	overlayStatus->setObjectName( QStringLiteral( "RiggingDonorOverlayStatus" ) );
	overlayStatus->setWordWrap( true );
	overlayLayout->addWidget( overlayStatus );
	auto * overlayButtons = new QHBoxLayout;
	auto * loadOverlay = new QPushButton( QObject::tr( "Load Donor..." ), overlayGroup );
	loadOverlay->setObjectName( QStringLiteral( "RiggingLoadDonorOverlayButton" ) );
	auto * clearOverlayButton = new QPushButton( QObject::tr( "Clear" ), overlayGroup );
	clearOverlayButton->setObjectName( QStringLiteral( "RiggingClearDonorOverlayButton" ) );
	clearOverlayButton->setEnabled( false );
	overlayButtons->addWidget( loadOverlay );
	overlayButtons->addWidget( clearOverlayButton );
	overlayButtons->addStretch();
	overlayLayout->addLayout( overlayButtons );
	auto * overlayStyle = new QHBoxLayout;
	auto * overlayFilled = new QCheckBox( QObject::tr( "Filled" ), overlayGroup );
	overlayFilled->setObjectName( QStringLiteral( "RiggingDonorOverlayFilled" ) );
	overlayFilled->setChecked( true );
	auto * overlayWire = new QCheckBox( QObject::tr( "Wireframe" ), overlayGroup );
	overlayWire->setObjectName( QStringLiteral( "RiggingDonorOverlayWireframe" ) );
	overlayWire->setChecked( true );
	auto * opacityLabel = new QLabel( QObject::tr( "Opacity 24%" ), overlayGroup );
	auto * overlayOpacity = new QSlider( Qt::Horizontal, overlayGroup );
	overlayOpacity->setObjectName( QStringLiteral( "RiggingDonorOverlayOpacity" ) );
	overlayOpacity->setRange( 5, 80 );
	overlayOpacity->setValue( 24 );
	overlayOpacity->setToolTip( QObject::tr( "Transparency of the cyan donor surface" ) );
	overlayStyle->addWidget( overlayFilled );
	overlayStyle->addWidget( overlayWire );
	overlayStyle->addWidget( opacityLabel );
	overlayStyle->addWidget( overlayOpacity, 1 );
	overlayLayout->addLayout( overlayStyle );
	layout->addWidget( overlayGroup );

	auto * transferHeading = new QLabel( QObject::tr( "Donor transfer" ), panel );
	transferHeading->setStyleSheet( QStringLiteral( "QLabel { font-weight: 600; }" ) );
	layout->addWidget( transferHeading );
	auto * primary = new QPushButton( QObject::tr( "Transfer Bones and Weights..." ), panel );
	primary->setObjectName( QStringLiteral( "RiggingTransferButton" ) );
	primary->setToolTip( QObject::tr( "Choose one donor and run remap generation, node import, binding, and weight transfer as one Undo step" ) );
	auto * primaryRow = new QHBoxLayout;
	primaryRow->addWidget( primary );
	primaryRow->addStretch();
	layout->addLayout( primaryRow );

	auto * quickRow = new QHBoxLayout;
	auto * compare = new QPushButton( QObject::tr( "Compare Bones..." ), panel );
	auto * preview = new QPushButton( QObject::tr( "Preview Transfer..." ), panel );
	auto * validate = new QPushButton( QObject::tr( "Validate FO4 Skin" ), panel );
	quickRow->addWidget( compare );
	quickRow->addWidget( preview );
	quickRow->addWidget( validate );
	quickRow->addStretch();
	layout->addLayout( quickRow );

	auto * advanced = new QGroupBox( QObject::tr( "Advanced steps" ), panel );
	advanced->setCheckable( true );
	advanced->setChecked( false );
	auto * advancedLayout = new QVBoxLayout( advanced );
	advancedLayout->setContentsMargins( 6, 8, 6, 6 );
	advancedLayout->setSpacing( 4 );
	auto * rebind = new QPushButton( QObject::tr( "Rebind to Reference Skeleton..." ), advanced );
	rebind->setToolTip( QObject::tr(
		"Compare this skin with an explicit game skeleton, then optionally rebuild BoneData while preserving raw geometry" ) );
	auto * generate = new QPushButton( QObject::tr( "1. Generate CustomizationRemapData" ), advanced );
	auto * importNodes = new QPushButton( QObject::tr( "2. Import Donor Bone Nodes..." ), advanced );
	auto * bindBones = new QPushButton( QObject::tr( "3. Bind Donor Bones..." ), advanced );
	auto * transferWeights = new QPushButton( QObject::tr( "4. Transfer Weights (existing bones)..." ), advanced );
	auto * hint = new QLabel( QObject::tr(
		"For inspection, repair, and partial files." ), advanced );
	hint->setWordWrap( true );
	hint->setStyleSheet( QStringLiteral( "QLabel { color: palette(mid); }" ) );
	advancedLayout->addWidget( hint );
	advancedLayout->addWidget( rebind );
	advancedLayout->addWidget( generate );
	advancedLayout->addWidget( importNodes );
	advancedLayout->addWidget( bindBones );
	advancedLayout->addWidget( transferWeights );
	const QList<QWidget *> advancedWidgets = { hint, rebind, generate, importNodes, bindBones, transferWeights };
	for ( QWidget * widget : advancedWidgets )
		widget->setVisible( false );
	QObject::connect( advanced, &QGroupBox::toggled, panel, [=]( bool expanded ) {
		for ( QWidget * widget : advancedWidgets )
			widget->setVisible( expanded );
	} );
	layout->addWidget( advanced );

	NifSkope * skope = qobject_cast<NifSkope *>( mw );
	auto targetBlock = std::make_shared<int>( -1 );
	auto overlayTargetBlock = std::make_shared<int>( -1 );
	auto heatmapTargetBlock = std::make_shared<int>( -1 );
	auto overlaySoup = std::make_shared<QVector<Vector3>>();
	auto heatmapSoup = std::make_shared<QVector<Vector3>>();
	auto heatmapColors = std::make_shared<QVector<FloatVector4>>();
	auto heatmapWeights = std::make_shared<QVector<float>>();
	auto heatmapCorners = std::make_shared<QVector<QVector<int>>>();
	auto heatmapNeighbours = std::make_shared<QVector<QSet<int>>>();
	auto paintStroke = std::make_shared<QMap<int, float>>();
	auto paintPreviewBase = std::make_shared<QVector<float>>();
	auto paintPreviewWeights = std::make_shared<QVector<float>>();
	auto paintStrokeTarget = std::make_shared<int>( -1 );
	auto paintStrokeBone = std::make_shared<int>( -1 );
	auto paintStrokeMode = std::make_shared<int>( 0 );
	auto paintStrokeWeight = std::make_shared<float>( 1.0f );
	auto paintStrokeStrength = std::make_shared<float>( 0.25f );
	auto paintStrokeAccumulate = std::make_shared<bool>( false );
	auto paintCommitting = std::make_shared<bool>( false );
	auto segmentStrokeFaces = std::make_shared<QSet<int>>();
	auto segmentBaseMembership = std::make_shared<QVector<int>>();
	auto segmentBaseSubmembership = std::make_shared<QVector<int>>();
	auto segmentStrokeTarget = std::make_shared<int>( -1 );
	auto segmentStrokeSegment = std::make_shared<int>( -1 );
	auto segmentStrokeSubsegment = std::make_shared<int>( -1 );
	auto segmentCommitting = std::make_shared<bool>( false );
	auto refreshAfterSegmentEdit = std::make_shared<std::function<void()>>();
	auto applyOverlay = [=]() {
		if ( !ogl )
			return;
		ogl->setRiggingDonorPreviewStyle( overlayFilled->isChecked(), overlayWire->isChecked(),
			float( overlayOpacity->value() ) / 100.0f );
		if ( dock->isVisible() && !overlaySoup->isEmpty() )
			ogl->setRiggingDonorPreview( *overlaySoup );
		else
			ogl->clearRiggingDonorPreview();
	};
	auto resetOverlay = [=]() {
		overlaySoup->clear();
		*overlayTargetBlock = -1;
		overlayStatus->setText( QObject::tr( "No donor geometry loaded." ) );
		clearOverlayButton->setEnabled( false );
		if ( ogl )
			ogl->clearRiggingDonorPreview();
	};
	auto applyHeatmap = [=]() {
		if ( !ogl )
			return;
		if ( dock->isVisible() && weightMap->isChecked() && !heatmapSoup->isEmpty() )
			ogl->setRiggingWeightPreview( *heatmapSoup, *heatmapColors );
		else
			ogl->clearRiggingWeightPreview();
	};
	auto resetHeatmap = [=]() {
		heatmapSoup->clear();
		heatmapColors->clear();
		heatmapWeights->clear();
		heatmapCorners->clear();
		heatmapNeighbours->clear();
		*heatmapTargetBlock = -1;
		weightMap->setChecked( false );
		weightMapStatus->setText( QObject::tr( "Select a skin bone or segment to inspect it." ) );
		if ( ogl )
			ogl->clearRiggingWeightPreview();
	};
	auto stopPainting = [=]() {
		paintStroke->clear();
		*paintStrokeTarget = *paintStrokeBone = -1;
		if ( paintButton->isChecked() ) {
			QSignalBlocker blocker( paintButton );
			paintButton->setChecked( false );
		}
		paintButton->setText( QObject::tr( "Start Painting" ) );
		if ( ogl && ogl->riggingWeightPaintModeActive() )
			ogl->setRiggingWeightPaintMode( false );
	};
	auto stopSegmentPainting = [=]() {
		segmentStrokeFaces->clear();
		*segmentStrokeTarget = *segmentStrokeSegment = *segmentStrokeSubsegment = -1;
		if ( segmentPaintButton->isChecked() ) {
			QSignalBlocker blocker( segmentPaintButton );
			segmentPaintButton->setChecked( false );
		}
		segmentPaintButton->setText( QObject::tr( "Start Painting" ) );
		if ( ogl && ogl->segmentPaintModeActive() ) ogl->setSegmentPaintMode( false );
	};
	std::function<void()> rebuildHeatmap = [=]() {
		heatmapSoup->clear();
		heatmapColors->clear();
		heatmapWeights->clear();
		heatmapCorners->clear();
		heatmapNeighbours->clear();
		QTreeWidgetItem * item = boneTree->currentItem();
		QModelIndex targetShape = nif->getBlockIndex( *targetBlock );
		if ( !weightMap->isChecked() || !item || !riggingIsSkinnedShape( nif, targetShape ) ) {
			applyHeatmap();
			return;
		}
		const int rowType = item->data( 0, RiggingReceiverRowTypeRole ).toInt();
		RigShape targetShapeData = riggingReadShape( nif, targetShape );
		Transform targetWorld;
		if ( !targetShapeData.valid || !riggingNodeAbsoluteWorld( nif, *targetBlock, targetWorld ) ) {
			weightMapStatus->setText( QObject::tr( "The selected target skin could not be visualized." ) );
			applyHeatmap();
			return;
		}
		auto targetVertexWorld = [&]( int vertex ) {
			Vector3 world;
			if ( ogl && ogl->evaluatedVertexWorld( *targetBlock, vertex, world ) )
				return world;
			return targetWorld * targetShapeData.pos.at( vertex );
		};

		if ( rowType == RiggingReceiverSegmentRow || rowType == RiggingReceiverSubsegmentRow ) {
			const int selectedSegment = item->data( 0, RiggingReceiverSegmentRole ).toInt();
			const int selectedSubsegment = item->data( 0, RiggingReceiverSubsegmentRole ).toInt();
			QVector<bool> selectedTriangles( targetShapeData.tris.size(), false );
			QVector<RigSegmentRange> ranges = riggingReadSegmentRanges( nif, targetShape );
			bool parentHasChildren = false;
			for ( const RigSegmentRange & range : ranges )
				if ( range.segment == selectedSegment && range.subsegment >= 0 ) parentHasChildren = true;
			for ( const RigSegmentRange & range : ranges ) {
				if ( range.segment != selectedSegment ) continue;
				bool include = rowType == RiggingReceiverSubsegmentRow
					? range.subsegment == selectedSubsegment
					: parentHasChildren ? range.subsegment >= 0 : range.subsegment < 0;
				if ( !include ) continue;
				int first = qBound( 0, range.startTriangle, targetShapeData.tris.size() );
				int last = qBound( first, first + qMax( 0, range.triangleCount ), targetShapeData.tris.size() );
				for ( int triangle = first; triangle < last; triangle++ ) selectedTriangles[triangle] = true;
			}

			int selectedCount = 0;
			heatmapSoup->reserve( targetShapeData.tris.size() * 3 );
			heatmapColors->reserve( targetShapeData.tris.size() * 3 );
			for ( int triangleIndex = 0; triangleIndex < targetShapeData.tris.size(); triangleIndex++ ) {
				const Triangle & triangle = targetShapeData.tris.at( triangleIndex );
				quint16 corners[3] = { triangle.v1(), triangle.v2(), triangle.v3() };
				const bool member = selectedTriangles.at( triangleIndex );
				if ( member ) selectedCount++;
				const FloatVector4 colour = member
					? FloatVector4( 1.0f, 0.32f, 0.0f, 0.90f )
					: FloatVector4( 0.22f, 0.22f, 0.22f, 0.30f );
				for ( quint16 vertex : corners ) {
					heatmapSoup->append( targetVertexWorld( vertex ) );
					heatmapColors->append( colour );
				}
			}
			weightMapStatus->setText( rowType == RiggingReceiverSubsegmentRow
				? QObject::tr( "Subsegment %1.%2 · %3 member faces · binary membership\nOrange = member   Grey = not a member" )
					.arg( selectedSegment ).arg( selectedSubsegment ).arg( selectedCount )
				: QObject::tr( "Segment %1 · %2 member faces including its subsegments · binary membership\nOrange = member   Grey = not a member" )
					.arg( selectedSegment ).arg( selectedCount ) );
			*heatmapTargetBlock = *targetBlock;
			applyHeatmap();
			return;
		}

		bool indexOk = false;
		int selectedBone = item->data( 0, Qt::UserRole + 1 ).toInt( &indexOk );
		if ( rowType != RiggingReceiverBoneRow || !indexOk || selectedBone < 0
			|| selectedBone >= targetShapeData.bones.size() ) {
			weightMapStatus->setText( QObject::tr( "The selected target item could not be visualized." ) );
			applyHeatmap();
			return;
		}
		QString bone = targetShapeData.bones.at( selectedBone );
		QVector<float> weights( targetShapeData.pos.size(), 0.0f );
		int weightedVertices = 0;
		float maximum = 0.0f;
		for ( int vertex = 0; vertex < targetShapeData.skin.size(); vertex++ ) {
			for ( const auto & influence : targetShapeData.skin.at( vertex ) )
				if ( influence.first == bone ) weights[vertex] += influence.second;
			weights[vertex] = qBound( 0.0f, weights.at( vertex ), 1.0f );
			if ( weights.at( vertex ) > 0.0f ) weightedVertices++;
			maximum = qMax( maximum, weights.at( vertex ) );
		}
		*heatmapWeights = weights;
		heatmapCorners->resize( weights.size() );
		heatmapNeighbours->resize( weights.size() );
		heatmapSoup->reserve( targetShapeData.tris.size() * 3 );
		heatmapColors->reserve( targetShapeData.tris.size() * 3 );
		for ( const Triangle & triangle : targetShapeData.tris ) {
			quint16 corners[3] = { triangle.v1(), triangle.v2(), triangle.v3() };
			for ( int edge = 0; edge < 3; edge++ ) {
				quint16 vertex = corners[edge];
				quint16 next = corners[( edge + 1 ) % 3];
				( *heatmapNeighbours )[vertex].insert( next );
				( *heatmapNeighbours )[next].insert( vertex );
				heatmapSoup->append( targetVertexWorld( vertex ) );
				( *heatmapCorners )[vertex].append( heatmapColors->size() );
				heatmapColors->append( riggingWeightHeatColor( weights.at( vertex ) ) );
			}
		}
		weightMapStatus->setText( QObject::tr( "%1 · %2 weighted vertices · maximum %3\n"
			"Blue 0   Cyan 0.25   Yellow 0.75   Red 1" )
			.arg( bone.isEmpty() ? QObject::tr( "<unnamed bone>" ) : bone )
			.arg( weightedVertices ).arg( maximum, 0, 'g', 4 ) );
		*heatmapTargetBlock = *targetBlock;
		applyHeatmap();
	};
	QObject::connect( weightMap, &QCheckBox::toggled, panel, [=]( bool enabled ) {
		if ( enabled )
			rebuildHeatmap();
		else if ( ogl )
			ogl->clearRiggingWeightPreview();
	} );
	QObject::connect( boneTree, &QTreeWidget::currentItemChanged, panel,
		[=]() {
			const bool boneSelected = riggingIsReceiverBoneRow( boneTree->currentItem() );
			const int rowType = boneTree->currentItem()
				? boneTree->currentItem()->data( 0, RiggingReceiverRowTypeRole ).toInt() : -1;
			const bool segmentSelected = rowType == RiggingReceiverSegmentRow
				|| rowType == RiggingReceiverSubsegmentRow;
			if ( !boneSelected && paintButton->isChecked() ) stopPainting();
			if ( !segmentSelected && segmentPaintButton->isChecked() ) stopSegmentPainting();
			paintButton->setEnabled( *targetBlock >= 0 && boneSelected && ogl );
			segmentPaintButton->setEnabled( *targetBlock >= 0 && segmentSelected && ogl );
			if ( weightMap->isChecked() ) rebuildHeatmap();
			if ( paintButton->isChecked() && boneSelected )
				ogl->setRiggingWeightPaintMode( true, *targetBlock, paintMode->currentIndex(),
					float( paintRadius->value() ), float( paintWeight->value() ) / 100.0f,
					float( paintStrength->value() ) / 100.0f );
			if ( segmentPaintButton->isChecked() && segmentSelected )
				ogl->setSegmentPaintMode( true, *targetBlock, float( segmentPaintRadius->value() ) );
		} );
	auto syncingBonePrimary = std::make_shared<bool>( false );
	QObject::connect( boneTree, &QTreeWidget::itemSelectionChanged, panel, [=]() {
		if ( *syncingBonePrimary ) return;
		*syncingBonePrimary = true;
		QList<QTreeWidgetItem *> selected = boneTree->selectedItems();
		QTreeWidgetItem * current = boneTree->currentItem();
		if ( !current || !current->isSelected() )
			boneTree->setCurrentItem( selected.isEmpty() ? nullptr : selected.last() );
		boneTree->viewport()->update();
		*syncingBonePrimary = false;
	} );
	auto refreshPaintControls = [=]() {
		paintWeightValue->setText( QString::number( paintWeight->value() ) + QStringLiteral( "%" ) );
		paintStrengthValue->setText( QString::number( paintStrength->value() ) + QStringLiteral( "%" ) );
		paintRadiusValue->setText( QString::number( paintRadius->value() ) + QStringLiteral( " px" ) );
		if ( paintButton->isChecked() && ogl )
			ogl->setRiggingWeightPaintMode( true, *targetBlock, paintMode->currentIndex(),
				float( paintRadius->value() ), float( paintWeight->value() ) / 100.0f,
				float( paintStrength->value() ) / 100.0f );
	};
	QObject::connect( paintMode, &QComboBox::currentIndexChanged, panel, [=]() { refreshPaintControls(); } );
	QObject::connect( paintWeight, &QSlider::valueChanged, panel, [=]() { refreshPaintControls(); } );
	QObject::connect( paintStrength, &QSlider::valueChanged, panel, [=]() { refreshPaintControls(); } );
	QObject::connect( paintRadius, &QSlider::valueChanged, panel, [=]() { refreshPaintControls(); } );
	refreshPaintControls();
	QObject::connect( paintButton, &QPushButton::toggled, panel, [=]( bool enabled ) {
		QTreeWidgetItem * item = boneTree->currentItem();
		QModelIndex shape = nif->getBlockIndex( *targetBlock );
		if ( enabled && ( !ogl || !riggingIsReceiverBoneRow( item ) || !riggingIsSkinnedShape( nif, shape ) ) ) {
			stopPainting();
			paintStatus->setText( QObject::tr( "Select a valid target mesh and skin bone first." ) );
			return;
		}
		paintButton->setText( enabled ? QObject::tr( "Stop Painting" ) : QObject::tr( "Start Painting" ) );
		if ( enabled ) {
			weightMap->setChecked( true );
			paintStatus->setText( QObject::tr( "Brush: LMB paints one undoable stroke; wheel zooms. A geometry selection masks painting; an empty selection paints everywhere. Tab returns to Object Mode." ) );
			ogl->setRiggingWeightPaintMode( true, *targetBlock, paintMode->currentIndex(),
				float( paintRadius->value() ), float( paintWeight->value() ) / 100.0f,
				float( paintStrength->value() ) / 100.0f );
		} else if ( ogl ) {
			ogl->setRiggingWeightPaintMode( false );
		}
	} );
	if ( ogl ) {
		QObject::connect( ogl, &GLView::riggingWeightPaintModeChanged, panel, [=]( bool enabled ) {
			if ( !enabled && paintButton->isChecked() ) {
				QSignalBlocker blocker( paintButton );
				paintButton->setChecked( false );
				paintButton->setText( QObject::tr( "Start Painting" ) );
			}
		} );
		QObject::connect( ogl, &GLView::riggingWeightPaintBrushChanged, panel, [=]( bool brushEnabled ) {
			if ( !ogl->riggingWeightPaintModeActive() )
				return;
			paintStatus->setText( brushEnabled
				? QObject::tr( "Brush: LMB paints; wheel zooms. Selected geometry masks painting; an empty selection paints everywhere. Tab returns to Object Mode." )
				: QObject::tr( "Selection: 1/2/3 chooses vertices, edges, or faces. A/B/C, Ctrl+I, linked, grow/shrink, and hide work; Ctrl+X fills. Use the Brush toolbar toggle to paint." ) );
		} );
		QObject::connect( ogl, &GLView::riggingWeightStrokeBegan, panel, [=]() {
			paintStroke->clear();
			*paintStrokeTarget = *targetBlock;
			QTreeWidgetItem * item = boneTree->currentItem();
			*paintStrokeBone = riggingIsReceiverBoneRow( item )
				? item->data( 0, Qt::UserRole + 1 ).toInt() : -1;
			*paintStrokeAccumulate = paintAccumulate->isChecked();
			*paintPreviewBase = *heatmapWeights;
			*paintPreviewWeights = *heatmapWeights;
		} );
		QObject::connect( ogl, &GLView::riggingWeightBrushSample, panel,
			[=]( int target, const QVector<int> & vertices, const QVector<float> & falloff,
				int mode, float weight, float strength ) {
				if ( target != *paintStrokeTarget || vertices.size() != falloff.size() )
					return;
				*paintStrokeMode = mode;
				*paintStrokeWeight = weight;
				*paintStrokeStrength = strength;
				for ( int i = 0; i < vertices.size(); i++ ) {
					float & amount = ( *paintStroke )[vertices.at( i )];
					amount = *paintStrokeAccumulate
						? qMin( 1024.0f, amount + qMax( 0.0f, falloff.at( i ) ) )
						: qMax( amount, falloff.at( i ) );
				}

				// The NIF remains untouched until release, but update the selected-bone
				// heatmap immediately from the accumulated stroke. This gives Blender-like
				// live feedback while retaining one atomic Undo snapshot per drag.
				if ( paintPreviewBase->size() == heatmapCorners->size()
					&& paintPreviewWeights->size() == paintPreviewBase->size()
					&& heatmapColors->size() == heatmapSoup->size() ) {
					for ( int vertex : vertices ) {
						if ( vertex < 0 || vertex >= paintPreviewBase->size() )
							continue;
						float current = paintPreviewBase->at( vertex );
						float alpha = riggingPaintStrokeAlpha( paintStroke->value( vertex ),
							strength, mode, *paintStrokeAccumulate );
						float targetWeight = current;
						if ( mode == 0 )
							targetWeight = qMin( 1.0f, current + weight * alpha );
						else if ( mode == 1 )
							targetWeight = qMax( 0.0f, current - weight * alpha );
						else if ( mode == 2 )
							targetWeight = current + ( weight - current ) * alpha;
						else if ( vertex < heatmapNeighbours->size() ) {
							const QSet<int> & neighbours = heatmapNeighbours->at( vertex );
							float average = current;
							if ( !neighbours.isEmpty() ) {
								average = 0.0f;
								for ( int neighbour : neighbours )
									average += paintPreviewBase->value( neighbour );
								average /= float( neighbours.size() );
							}
							targetWeight = current + ( average - current ) * alpha;
						}
						// A sole normalized influence cannot be reduced until another bone
						// exists; mirror the write path so preview and release agree here.
						if ( current >= 1.0f - 1.0e-6f && targetWeight < current )
							targetWeight = 1.0f;
						targetWeight = qBound( 0.0f, targetWeight, 1.0f );
						( *paintPreviewWeights )[vertex] = targetWeight;
						FloatVector4 color = riggingWeightHeatColor( targetWeight );
						for ( int corner : heatmapCorners->at( vertex ) )
							if ( corner >= 0 && corner < heatmapColors->size() )
								( *heatmapColors )[corner] = color;
					}
					ogl->setRiggingWeightPreviewColors( *heatmapColors );
				}
			} );
		QObject::connect( ogl, &GLView::riggingWeightStrokeEnded, panel, [=]( bool commit ) {
			if ( !commit || paintStroke->isEmpty() || *paintStrokeTarget != *targetBlock ) {
				paintStroke->clear();
				rebuildHeatmap();
				return;
			}
			QString error;
			bool boundsUpdated = false;
			*paintCommitting = true;
			int changed = riggingApplyPaintStroke( nif, nif->getBlockIndex( *targetBlock ),
				*paintStrokeBone, *paintStroke, *paintStrokeMode, *paintStrokeWeight,
				*paintStrokeStrength, *paintStrokeAccumulate, boundsUpdated, error );
			*paintCommitting = false;
			paintStroke->clear();
			if ( changed < 0 ) {
				paintStatus->setText( error );
				QMessageBox::warning( mw, QObject::tr( "Manual Weight Painting" ), error );
			} else if ( changed == 0 ) {
				paintStatus->setText( QObject::tr( "Stroke made no weight changes." ) );
			} else {
				paintStatus->setText( boundsUpdated
					? QObject::tr( "Painted %1 vertices as one Undo step." ).arg( changed )
					: QObject::tr( "Painted %1 vertices. Bone bounds could not be recalculated." ).arg( changed ) );
			}
			rebuildHeatmap();
		} );
	}
	QObject::connect( segmentPaintRadius, &QSlider::valueChanged, panel, [=]( int value ) {
		segmentPaintRadiusValue->setText( QObject::tr( "%1 px" ).arg( value ) );
		if ( segmentPaintButton->isChecked() && ogl )
			ogl->setSegmentPaintMode( true, *targetBlock, float( value ) );
	} );
	QObject::connect( segmentPaintButton, &QPushButton::toggled, panel, [=]( bool enabled ) {
		QTreeWidgetItem * item = boneTree->currentItem();
		const int rowType = item ? item->data( 0, RiggingReceiverRowTypeRole ).toInt() : -1;
		QModelIndex shape = nif->getBlockIndex( *targetBlock );
		const bool segmentRow = rowType == RiggingReceiverSegmentRow
			|| rowType == RiggingReceiverSubsegmentRow;
		if ( enabled && ( !ogl || !segmentRow || !nif->blockInherits( shape, "BSSubIndexTriShape" ) ) ) {
			stopSegmentPainting();
			segmentPaintStatus->setText( QObject::tr( "Select a segment or subsegment on a BSSubIndexTriShape first." ) );
			return;
		}
		segmentPaintButton->setText( enabled ? QObject::tr( "Stop Painting" ) : QObject::tr( "Start Painting" ) );
		if ( enabled ) {
			stopPainting();
			weightMap->setChecked( true );
		segmentPaintStatus->setText( QObject::tr(
			"LMB paints continuously. Selected geometry masks painting; an empty selection paints everywhere. Disable Brush for selection; Ctrl+X fills selected faces." ) );
			ogl->setSegmentPaintMode( true, *targetBlock, float( segmentPaintRadius->value() ) );
		} else if ( ogl ) {
			ogl->setSegmentPaintMode( false );
		}
	} );
	if ( ogl ) {
		QObject::connect( ogl, &GLView::segmentPaintModeChanged, panel, [=]( bool enabled ) {
			if ( !enabled && segmentPaintButton->isChecked() ) {
				QSignalBlocker blocker( segmentPaintButton );
				segmentPaintButton->setChecked( false );
				segmentPaintButton->setText( QObject::tr( "Start Painting" ) );
			}
		} );
		QObject::connect( ogl, &GLView::segmentPaintBrushChanged, panel, [=]( bool enabled ) {
			if ( !ogl->segmentPaintModeActive() ) return;
			segmentPaintStatus->setText( enabled
				? QObject::tr( "Brush: LMB paints binary membership; wheel zooms. Selection masks painting." )
				: QObject::tr( "Selection: use face/edge/vertex selection, linked, grow/shrink, invert, and hide; enable Brush to paint." ) );
		} );
		QObject::connect( ogl, &GLView::segmentPaintStrokeBegan, panel, [=]() {
			segmentStrokeFaces->clear();
			*segmentStrokeTarget = *targetBlock;
			QTreeWidgetItem * item = boneTree->currentItem();
			*segmentStrokeSegment = item ? item->data( 0, RiggingReceiverSegmentRole ).toInt() : -1;
			*segmentStrokeSubsegment = item ? item->data( 0, RiggingReceiverSubsegmentRole ).toInt() : -1;
			riggingReadTriangleMembership( nif, nif->getBlockIndex( *targetBlock ),
				*segmentBaseMembership, *segmentBaseSubmembership );
		} );
		QObject::connect( ogl, &GLView::segmentPaintBrushSample, panel,
			[=]( int target, const QVector<int> & triangles ) {
				if ( target != *segmentStrokeTarget ) return;
				for ( int triangle : triangles )
					if ( triangle >= 0 && triangle < segmentBaseMembership->size() )
						segmentStrokeFaces->insert( triangle );
				if ( heatmapColors->size() == segmentBaseMembership->size() * 3 ) {
					for ( int triangle : triangles ) {
						if ( triangle < 0 || triangle >= segmentBaseMembership->size() ) continue;
						bool member = segmentPaintOperation->currentIndex() == 0;
						FloatVector4 colour = member
							? FloatVector4( 1.0f, 0.32f, 0.0f, 0.90f )
							: FloatVector4( 0.22f, 0.22f, 0.22f, 0.30f );
						for ( int corner = 0; corner < 3; corner++ )
							( *heatmapColors )[triangle * 3 + corner] = colour;
					}
					ogl->setRiggingWeightPreviewColors( *heatmapColors );
				}
			} );
		QObject::connect( ogl, &GLView::segmentPaintStrokeEnded, panel, [=]( bool commit ) {
			if ( !commit || segmentStrokeFaces->isEmpty() || *segmentStrokeTarget != *targetBlock ) {
				segmentStrokeFaces->clear();
				rebuildHeatmap();
				return;
			}
			QVector<int> segments = *segmentBaseMembership;
			QVector<int> subsegments = *segmentBaseSubmembership;
			int changed = 0;
			for ( int triangle : *segmentStrokeFaces ) {
				if ( triangle < 0 || triangle >= segments.size() ) continue;
				if ( segmentPaintOperation->currentIndex() == 0 ) {
					if ( segments[triangle] != *segmentStrokeSegment
						|| subsegments[triangle] != *segmentStrokeSubsegment ) changed++;
					segments[triangle] = *segmentStrokeSegment;
					subsegments[triangle] = *segmentStrokeSubsegment;
				} else {
					const bool belongs = segments[triangle] == *segmentStrokeSegment
						&& ( *segmentStrokeSubsegment < 0 || subsegments[triangle] == *segmentStrokeSubsegment );
					if ( belongs && ( segments[triangle] != 0 || subsegments[triangle] != -1 ) ) {
						segments[triangle] = 0;
						subsegments[triangle] = -1;
						changed++;
					}
				}
			}
			QString error;
			if ( changed > 0 ) {
				QVector<RigSegmentDefinition> definitions = riggingReadSegmentDefinitions( nif,
					nif->getBlockIndex( *targetBlock ) );
				*segmentCommitting = true;
				bool ok = nifSnapshotOp( nif, QObject::tr( "Paint %1 segment face(s)" ).arg( changed ), [&]() {
					return riggingWriteSegmentLayout( nif, nif->getBlockIndex( *targetBlock ),
						definitions, segments, subsegments, error );
				} );
				*segmentCommitting = false;
				if ( !ok ) {
					segmentPaintStatus->setText( error );
					QMessageBox::warning( mw, QObject::tr( "Segment Painting" ), error );
				} else {
					segmentPaintStatus->setText( QObject::tr( "Changed %1 faces as one Undo step." ).arg( changed ) );
				}
			} else {
				segmentPaintStatus->setText( QObject::tr( "Stroke made no membership changes." ) );
			}
			segmentStrokeFaces->clear();
			QTimer::singleShot( 0, panel, [=]() {
				if ( *refreshAfterSegmentEdit ) ( *refreshAfterSegmentEdit )();
			} );
		} );
	}
	QObject::connect( overlayFilled, &QCheckBox::toggled, panel, [=]() { applyOverlay(); } );
	QObject::connect( overlayWire, &QCheckBox::toggled, panel, [=]() { applyOverlay(); } );
	QObject::connect( overlayOpacity, &QSlider::valueChanged, panel, [=]( int value ) {
		opacityLabel->setText( QObject::tr( "Opacity %1%" ).arg( value ) );
		applyOverlay();
	} );
	QObject::connect( clearOverlayButton, &QPushButton::clicked, panel, [=]() { resetOverlay(); } );
	QObject::connect( loadOverlay, &QPushButton::clicked, panel, [=]() {
		QModelIndex targetShape = nif->getBlockIndex( *targetBlock );
		if ( !riggingIsSkinnedShape( nif, targetShape ) )
			return;
		QString fileName = QFileDialog::getOpenFileName( mw, QObject::tr( "Choose Donor NIF for Overlay" ),
			nif->getFolder(), QObject::tr( "NIF files (*.nif)" ) );
		if ( fileName.isEmpty() )
			return;
		NifModel donor;
		if ( !donor.loadFromFile( fileName ) || donor.getBSVersion() != nif->getBSVersion() ) {
			QMessageBox::warning( mw, QObject::tr( "Donor Geometry Overlay" ),
				QObject::tr( "The donor could not be loaded or uses a different BS version." ) );
			return;
		}
		QStringList labels;
		QList<int> shapes = riggingSkinnedShapes( &donor, labels );
		int donorBlock = riggingChooseDonorShape( shapes, labels, QObject::tr( "Donor Geometry Overlay" ) );
		if ( donorBlock < 0 )
			return;
		RigShape donorShape = riggingReadShape( &donor, donor.getBlockIndex( donorBlock ) );
		if ( !donorShape.valid ) {
			QMessageBox::warning( mw, QObject::tr( "Donor Geometry Overlay" ),
				QObject::tr( "The selected donor shape has no readable FO4 geometry." ) );
			return;
		}
		Transform targetSpace, donorSpace, targetWorld;
		int targetRoot = nif->getLink( riggingSkinInstance( nif, targetShape ), "Skeleton Root" );
		int donorRoot = donor.getLink( riggingSkinInstance( &donor, donor.getBlockIndex( donorBlock ) ), "Skeleton Root" );
		if ( !riggingNodeWorld( nif, *targetBlock, targetRoot, targetSpace )
			|| !riggingNodeWorld( &donor, donorBlock, donorRoot, donorSpace )
			|| riggingTransformDelta( targetSpace, donorSpace ) > 0.001f
			|| !riggingNodeAbsoluteWorld( nif, *targetBlock, targetWorld ) ) {
			QMessageBox::warning( mw, QObject::tr( "Donor Geometry Overlay" ), QObject::tr(
				"Donor and target shapes are not in the same skeleton-root-relative space. "
				"The overlay was not changed." ) );
			return;
		}

		QVector<Vector3> soup;
		soup.reserve( donorShape.tris.size() * 3 );
		for ( const Triangle & triangle : donorShape.tris )
			soup << targetWorld * donorShape.pos.at( triangle.v1() )
				<< targetWorld * donorShape.pos.at( triangle.v2() )
				<< targetWorld * donorShape.pos.at( triangle.v3() );
		*overlaySoup = soup;
		*overlayTargetBlock = *targetBlock;
		QString donorName = donor.get<QString>( donor.getBlockIndex( donorBlock ), "Name" );
		overlayStatus->setText( QObject::tr( "%1\n%2 vertices · %3 triangles · cyan preview only" )
			.arg( donorName.isEmpty() ? QFileInfo( fileName ).fileName() : donorName )
			.arg( donorShape.pos.size() ).arg( donorShape.tris.size() ) );
		clearOverlayButton->setEnabled( true );
		applyOverlay();
	} );
	const QString riggingPage = Spell::tr( "Rigging" ) + QStringLiteral( "/" );
	const QList<QPair<QPushButton *, QString>> actions = {
		{ primary, riggingPage + Spell::tr( "Transfer Bones and Weights..." ) },
		{ compare, riggingPage + Spell::tr( "Compare Bones with Donor..." ) },
		{ preview, riggingPage + Spell::tr( "Preview Transfer from Donor..." ) },
		{ validate, riggingPage + Spell::tr( "Validate FO4 Skin" ) },
		{ rebind, riggingPage + Spell::tr( "Rebind to Reference Skeleton..." ) },
		{ generate, riggingPage + Spell::tr( "Generate CustomizationRemapData" ) },
		{ importNodes, riggingPage + Spell::tr( "Import Donor Bone Nodes..." ) },
		{ bindBones, riggingPage + Spell::tr( "Bind Donor Bones (existing nodes)..." ) },
		{ transferWeights, riggingPage + Spell::tr( "Transfer Weights (existing bones)..." ) }
	};
	auto updateContextButton = [=]() {
		bool hasTransferableSelection = false;
		for ( QTreeWidgetItem * item : donorTree->selectedItems() ) {
			QVariant donorData = item->data( 0, Qt::UserRole );
			QVariant slotData = item->data( 0, Qt::UserRole + 1 );
			if ( donorData.isValid() && slotData.isValid()
				&& donorData.toInt() >= 0 && slotData.toInt() >= 0
				&& !item->data( 0, Qt::UserRole + 3 ).toBool()
				&& ( item->flags() & Qt::ItemIsEnabled ) ) {
				hasTransferableSelection = true;
				break;
			}
		}
		contextBindButton->setEnabled( hasTransferableSelection && *targetBlock >= 0 );
	};
	QObject::connect( donorTree, &QTreeWidget::itemSelectionChanged, panel, updateContextButton );
	auto applyBoneFilter = [=]() {
		const QString needle = boneSearch->text().trimmed();
		const int kind = boneKindFilter->currentIndex(); // 0 all, 1 bones, 2 segments
		for ( int row = 0; row < boneTree->topLevelItemCount(); row++ ) {
			QTreeWidgetItem * item = boneTree->topLevelItem( row );
			const int rowType = item->data( 0, RiggingReceiverRowTypeRole ).toInt();
			const bool typeMatch = kind == 0
				|| ( kind == 1 && rowType == RiggingReceiverBoneRow )
				|| ( kind == 2 && rowType != RiggingReceiverBoneRow );
			bool parentMatch = needle.isEmpty() || item->text( 0 ).contains( needle, Qt::CaseInsensitive )
				|| item->text( 1 ).contains( needle, Qt::CaseInsensitive );
			bool childMatch = false;
			for ( int child = 0; child < item->childCount(); child++ ) {
				QTreeWidgetItem * subitem = item->child( child );
				bool match = typeMatch && ( parentMatch
					|| subitem->text( 0 ).contains( needle, Qt::CaseInsensitive )
					|| subitem->text( 1 ).contains( needle, Qt::CaseInsensitive ) );
				subitem->setHidden( !match );
				childMatch = childMatch || match;
			}
			item->setHidden( !typeMatch || ( !parentMatch && !childMatch ) );
			if ( !needle.isEmpty() && childMatch ) item->setExpanded( true );
		}
		std::function<bool( QTreeWidgetItem *, bool )> filterDonorItem =
			[&]( QTreeWidgetItem * item, bool ancestorMatch ) {
				QVariant typeData = item->data( 0, RiggingReceiverRowTypeRole );
				const bool container = !typeData.isValid();
				const int rowType = typeData.toInt();
				const bool typeMatch = container || kind == 0
					|| ( kind == 1 && rowType == RiggingReceiverBoneRow )
					|| ( kind == 2 && rowType != RiggingReceiverBoneRow );
				bool ownMatch = typeMatch && ( ancestorMatch || needle.isEmpty()
					|| item->text( 0 ).contains( needle, Qt::CaseInsensitive )
					|| item->text( 1 ).contains( needle, Qt::CaseInsensitive ) );
				bool childMatch = false;
				for ( int child = 0; child < item->childCount(); child++ )
					childMatch = filterDonorItem( item->child( child ), ownMatch ) || childMatch;
				const bool anyMatch = container ? childMatch : ( ownMatch || childMatch );
				item->setHidden( !anyMatch );
				if ( !needle.isEmpty() && anyMatch && item->childCount() > 0 ) item->setExpanded( true );
				return anyMatch;
			};
		for ( int row = 0; row < donorTree->topLevelItemCount(); row++ )
			filterDonorItem( donorTree->topLevelItem( row ), false );
	};
	QObject::connect( boneSearch, &QLineEdit::textChanged, panel, [=]() { applyBoneFilter(); } );
	QObject::connect( boneKindFilter, &QComboBox::currentIndexChanged, panel, [=]() { applyBoneFilter(); } );
	auto refreshingBoneTrees = std::make_shared<bool>( false );

	auto refresh = [=]( const QModelIndex & current ) {
		*refreshingBoneTrees = true;
		int selectedBlock = nif->getBlockNumber( current );
		if ( ogl ) {
			int activeBlock = ogl->activeObjectBlock();
			if ( ogl->selectedObjectBlocks().contains( activeBlock )
				&& riggingIsSkinnedShape( nif, nif->getBlockIndex( activeBlock ) ) )
				selectedBlock = activeBlock;
		}
		QModelIndex selected = nif->getBlockIndex( selectedBlock );
		if ( riggingIsSkinnedShape( nif, selected ) ) {
			if ( *targetBlock >= 0 && *targetBlock != selectedBlock ) {
				stopPainting();
				stopSegmentPainting();
			}
			if ( *overlayTargetBlock >= 0 && *overlayTargetBlock != selectedBlock )
				resetOverlay();
			if ( *heatmapTargetBlock >= 0 && *heatmapTargetBlock != selectedBlock )
				resetHeatmap();
			*targetBlock = selectedBlock;
		}

		QModelIndex shape = nif->getBlockIndex( *targetBlock );
		bool valid = riggingIsSkinnedShape( nif, shape );
		// Keep the block number through the transient empty phase of a whole-model
		// snapshot restore. The same shape can then be reacquired after Undo/Redo;
		// completeLoading explicitly clears it for a genuinely new document.
		boneTree->clear();
		donorTree->clear();
		if ( valid ) {
			QString shapeName = nif->get<QString>( shape, "Name" );
			QStringList bones = riggingBoneNames( nif, shape );
			QVector<RigSegmentRange> segmentRanges = riggingReadSegmentRanges( nif, shape );
			int segmentCount = 0, subsegmentCount = 0;
			for ( const RigSegmentRange & range : segmentRanges ) {
				if ( range.subsegment < 0 ) segmentCount++;
				else subsegmentCount++;
			}
			int vertices = nif->rowCount( nif->getIndex( shape, "Vertex Data" ) );
			int remapBytes = -1;
			for ( int link : nif->getChildLinks( *targetBlock ) ) {
				QModelIndex extra = nif->getBlockIndex( link, "NiBinaryExtraData" );
				if ( extra.isValid()
					&& nif->get<QString>( extra, "Name" ) == QStringLiteral( "CustomizationRemapData" ) ) {
					remapBytes = nif->get<QByteArray>( extra, "Binary Data" ).size();
					break;
				}
			}
			targetStatus->setText( QObject::tr( "%1  [block %2]\n%3 vertices · %4 bones · %5 segments / %6 subsegments · RemapData: %7" )
				.arg( shapeName.isEmpty() ? QObject::tr( "<unnamed shape>" ) : shapeName )
				.arg( *targetBlock ).arg( vertices ).arg( bones.size() ).arg( segmentCount ).arg( subsegmentCount )
				.arg( remapBytes < 0 ? QObject::tr( "not attached" )
					: QObject::tr( "%1 bytes" ).arg( remapBytes ) ) );
			for ( int i = 0; i < bones.size(); i++ ) {
				auto * item = new QTreeWidgetItem( boneTree );
				item->setText( 0, QString::number( i ) );
				item->setText( 1, bones.at( i ).isEmpty() ? QObject::tr( "<unnamed>" ) : bones.at( i ) );
				item->setData( 0, Qt::UserRole, *targetBlock );
				item->setData( 0, Qt::UserRole + 1, i );
				item->setData( 0, Qt::UserRole + 2, bones.at( i ) );
				QModelIndex skin = riggingSkinInstance( nif, shape );
				QModelIndex bindings = nif->getIndex( skin, "Bones" );
				item->setData( 0, Qt::UserRole + 3,
					nif->getLink( nif->getIndex( bindings, i ) ) );
				item->setData( 0, RiggingReceiverRowTypeRole, RiggingReceiverBoneRow );
				item->setFlags( item->flags() | Qt::ItemIsEditable );
			}

			const QColor segmentColour( 92, 190, 200 );
			for ( const RigSegmentRange & parentRange : segmentRanges ) {
				if ( parentRange.subsegment >= 0 ) continue;
				const int displayedFaces = parentRange.triangleCount;
				auto * segmentItem = new QTreeWidgetItem( boneTree );
				segmentItem->setText( 0, QObject::tr( "SEG %1" ).arg( parentRange.segment ) );
				segmentItem->setText( 1, QObject::tr( "Segment %1 · %2 faces" )
					.arg( parentRange.segment ).arg( displayedFaces ) );
				segmentItem->setData( 0, RiggingReceiverRowTypeRole, RiggingReceiverSegmentRow );
				segmentItem->setData( 0, RiggingReceiverSegmentRole, parentRange.segment );
				segmentItem->setData( 0, RiggingReceiverSubsegmentRole, -1 );
				if ( parentRange.hasSharedData )
					segmentItem->setText( 1, segmentItem->text( 1 )
						+ QObject::tr( " / user %1" ).arg( parentRange.userIndex ) );
				segmentItem->setForeground( 0, segmentColour );
				segmentItem->setForeground( 1, segmentColour );
				QFont segmentFont = segmentItem->font( 1 );
				segmentFont.setBold( true );
				segmentItem->setFont( 1, segmentFont );

				for ( const RigSegmentRange & childRange : segmentRanges ) {
					if ( childRange.segment != parentRange.segment || childRange.subsegment < 0 ) continue;
					auto * subitem = new QTreeWidgetItem( segmentItem );
					subitem->setText( 0, QObject::tr( "SUB %1.%2" )
						.arg( childRange.segment ).arg( childRange.subsegment ) );
					QString details = QObject::tr( "Subsegment %1.%2 · %3 faces" )
						.arg( childRange.segment ).arg( childRange.subsegment ).arg( childRange.triangleCount );
					if ( childRange.hasSharedData ) {
						details += QObject::tr( " · user %1" ).arg( childRange.userIndex );
						if ( childRange.boneId != std::numeric_limits<quint32>::max() )
							details += QObject::tr( " · bone 0x%1" )
								.arg( childRange.boneId, 8, 16, QLatin1Char( '0' ) ).toUpper();
					}
					subitem->setText( 1, details );
					subitem->setData( 0, RiggingReceiverRowTypeRole, RiggingReceiverSubsegmentRow );
					subitem->setData( 0, RiggingReceiverSegmentRole, childRange.segment );
					subitem->setData( 0, RiggingReceiverSubsegmentRole, childRange.subsegment );
					subitem->setForeground( 0, segmentColour );
					subitem->setForeground( 1, segmentColour );
				}
				segmentItem->setExpanded( true );
			}
		} else {
			targetStatus->setText( QObject::tr( "Select a skinned FO4 BSTriShape in the block list or viewport." ) );
		}

		int secondaryCount = 0, missingCount = 0, incompatibleCount = 0;
		if ( valid && ogl ) {
			QList<int> secondaryBlocks = ogl->selectedObjectBlocks().values();
			std::sort( secondaryBlocks.begin(), secondaryBlocks.end() );
			QSet<QString> receiverBones;
			for ( const QString & name : riggingBoneNames( nif, shape ) )
				if ( !name.isEmpty() ) receiverBones << name;
			QModelIndex iReceiverSkin = riggingSkinInstance( nif, shape );
			int receiverRoot = nif->getLink( iReceiverSkin, "Skeleton Root" );

			for ( int donorBlock : secondaryBlocks ) {
				if ( donorBlock == *targetBlock )
					continue;
				QModelIndex donorShape = nif->getBlockIndex( donorBlock );
				if ( !riggingIsSkinnedShape( nif, donorShape ) )
					continue;
				secondaryCount++;
				QString donorName = nif->get<QString>( donorShape, "Name" );
				auto * donorItem = new QTreeWidgetItem( donorTree );
				donorItem->setText( 0, QStringLiteral( "%1  [block %2]" )
					.arg( donorName.isEmpty() ? QObject::tr( "<unnamed mesh>" ) : donorName ).arg( donorBlock ) );
				donorItem->setFlags( donorItem->flags() & ~Qt::ItemIsSelectable );

				QModelIndex iDonorSkin = riggingSkinInstance( nif, donorShape );
				QModelIndex iDonorData = nif->getBlockIndex( nif->getLink( iDonorSkin, "Data" ) );
				QModelIndex iDonorBones = nif->getIndex( iDonorSkin, "Bones" );
				QModelIndex iDonorList = nif->getIndex( iDonorData, "Bone List" );
				bool compatible = nif->isNiBlock( iReceiverSkin, "BSSkin::Instance" )
					&& nif->isNiBlock( iDonorSkin, "BSSkin::Instance" )
					&& nif->isNiBlock( iDonorData, "BSSkin::BoneData" )
					&& iDonorBones.isValid() && iDonorList.isValid()
					&& nif->getLink( iDonorSkin, "Skeleton Root" ) == receiverRoot
					&& nif->rowCount( iDonorBones ) == nif->rowCount( iDonorList )
					&& nif->get<quint32>( iDonorSkin, "Num Bones" ) == quint32( nif->rowCount( iDonorBones ) )
					&& nif->get<quint32>( iDonorData, "Num Bones" ) == quint32( nif->rowCount( iDonorList ) )
					&& nif->get<quint32>( iDonorSkin, "Num Scales" ) == 0;
				if ( !compatible ) {
					donorItem->setText( 1, QObject::tr( "Incompatible" ) );
					incompatibleCount++;
				} else {
					donorItem->setText( 1, QObject::tr( "Donor" ) );
				}

				QStringList donorBones = riggingBoneNames( nif, donorShape );
				for ( int slot = 0; slot < donorBones.size(); slot++ ) {
					const QString & boneName = donorBones.at( slot );
					auto * boneItem = new QTreeWidgetItem( donorItem );
					boneItem->setText( 0, QStringLiteral( "%1  %2" ).arg( slot )
						.arg( boneName.isEmpty() ? QObject::tr( "<unnamed>" ) : boneName ) );
					bool alreadyBound = !boneName.isEmpty() && receiverBones.contains( boneName );
					boneItem->setData( 0, RiggingReceiverRowTypeRole, RiggingReceiverBoneRow );
					boneItem->setText( 1, alreadyBound ? QObject::tr( "Bound" )
						: compatible ? QObject::tr( "Missing" ) : QObject::tr( "Unavailable" ) );
					if ( compatible && !boneName.isEmpty() ) {
						boneItem->setData( 0, Qt::UserRole, donorBlock );
						boneItem->setData( 0, Qt::UserRole + 1, slot );
						boneItem->setData( 0, Qt::UserRole + 2, boneName );
						boneItem->setData( 0, Qt::UserRole + 3, alreadyBound );
						if ( !alreadyBound ) missingCount++;
					} else {
						boneItem->setFlags( boneItem->flags() & ~Qt::ItemIsSelectable & ~Qt::ItemIsEnabled );
					}
				}
				const QColor donorSegmentColour( 92, 190, 200 );
				QVector<RigSegmentRange> donorSegments = riggingReadSegmentRanges( nif, donorShape );
				for ( const RigSegmentRange & parentRange : donorSegments ) {
					if ( parentRange.subsegment >= 0 ) continue;
					auto * segmentItem = new QTreeWidgetItem( donorItem );
					segmentItem->setText( 0, QObject::tr( "SEG %1  Segment %1" ).arg( parentRange.segment ) );
					segmentItem->setText( 1, QObject::tr( "Binary faces" ) );
					segmentItem->setData( 0, RiggingReceiverRowTypeRole, RiggingReceiverSegmentRow );
					segmentItem->setData( 0, RiggingReceiverSegmentRole, parentRange.segment );
					segmentItem->setData( 0, RiggingReceiverSubsegmentRole, -1 );
					segmentItem->setData( 0, Qt::UserRole, donorBlock );
					segmentItem->setForeground( 0, donorSegmentColour );
					segmentItem->setForeground( 1, donorSegmentColour );
					QFont segmentFont = segmentItem->font( 0 );
					segmentFont.setBold( true );
					segmentItem->setFont( 0, segmentFont );
					for ( const RigSegmentRange & childRange : donorSegments ) {
						if ( childRange.segment != parentRange.segment || childRange.subsegment < 0 ) continue;
						auto * subitem = new QTreeWidgetItem( segmentItem );
						subitem->setText( 0, QObject::tr( "SUB %1.%2  Subsegment %1.%2" )
							.arg( childRange.segment ).arg( childRange.subsegment ) );
						subitem->setText( 1, QObject::tr( "%1 faces" ).arg( childRange.triangleCount ) );
						subitem->setData( 0, RiggingReceiverRowTypeRole, RiggingReceiverSubsegmentRow );
						subitem->setData( 0, RiggingReceiverSegmentRole, childRange.segment );
						subitem->setData( 0, RiggingReceiverSubsegmentRole, childRange.subsegment );
						subitem->setData( 0, Qt::UserRole, donorBlock );
						subitem->setForeground( 0, donorSegmentColour );
						subitem->setForeground( 1, donorSegmentColour );
					}
					segmentItem->setExpanded( true );
				}
				donorItem->setExpanded( true );
			}
		}
		if ( !valid ) {
			contextBoneStatus->setText( QObject::tr( "Select a valid primary receiver mesh." ) );
		} else if ( !ogl || secondaryCount == 0 ) {
			contextBoneStatus->setText( QObject::tr(
				"Shift-select one or more secondary skinned meshes in the viewport; the active mesh remains the receiver." ) );
		} else if ( missingCount == 0 ) {
			contextBoneStatus->setText( incompatibleCount > 0
				? QObject::tr( "No transferable bones. Secondary meshes must share the receiver's in-file Skeleton Root and FO4 BSSkin layout." )
				: QObject::tr( "All bones from the selected secondary meshes are already bound to the receiver." ) );
		} else {
			contextBoneStatus->setText( QObject::tr(
				"%1 secondary mesh(es) · %2 missing binding(s). Select donor bones, then add them to the receiver." )
				.arg( secondaryCount ).arg( missingCount ) );
		}
		updateContextButton();
		const bool showDonors = secondaryCount > 0;
		donorTree->setVisible( showDonors );
		contextBindButton->setVisible( showDonors );
		contextBoneStatus->setVisible( showDonors );
		contextGroup->setTitle( showDonors
			? QObject::tr( "Selected-mesh bone & segment transfer" )
			: QObject::tr( "Receiver bones & segments" ) );
		boneSearch->setPlaceholderText( showDonors
			? QObject::tr( "Search receiver and donor bones..." )
			: QObject::tr( "Search receiver bones and segments..." ) );
		applyBoneFilter();
		loadOverlay->setEnabled( valid );
		paintButton->setEnabled( valid && riggingIsReceiverBoneRow( boneTree->currentItem() ) && ogl );
		const int currentType = boneTree->currentItem()
			? boneTree->currentItem()->data( 0, RiggingReceiverRowTypeRole ).toInt() : -1;
		segmentPaintButton->setEnabled( valid && ogl && ( currentType == RiggingReceiverSegmentRow
			|| currentType == RiggingReceiverSubsegmentRow ) );

		for ( const auto & action : actions ) {
			SpellPtr spell = SpellBook::lookup( action.second );
			action.first->setEnabled( valid && spell && spell->isApplicable( nif, shape ) );
		}
		*refreshingBoneTrees = false;
	};
	*refreshAfterSegmentEdit = [=]() { refresh( nif->getBlockIndex( *targetBlock ) ); };

	auto applySegmentMutation = [=]( const QString & label,
		const std::function<bool( QVector<RigSegmentDefinition> &, QVector<int> &, QVector<int> &, QString & )> & mutate,
		bool notify = true ) {
		QModelIndex shape = nif->getBlockIndex( *targetBlock );
		if ( !nif->blockInherits( shape, "BSSubIndexTriShape" ) ) return false;
		QVector<RigSegmentDefinition> definitions = riggingReadSegmentDefinitions( nif, shape );
		QVector<int> segments, subsegments;
		riggingReadTriangleMembership( nif, shape, segments, subsegments );
		QString error;
		if ( !mutate( definitions, segments, subsegments, error ) || definitions.isEmpty() ) {
			if ( notify && !error.isEmpty() ) QMessageBox::warning( mw, QObject::tr( "Edit Segments" ), error );
			return false;
		}
		stopSegmentPainting();
		resetHeatmap();
		*segmentCommitting = true;
		bool ok = nifSnapshotOp( nif, label, [&]() {
			return riggingWriteSegmentLayout( nif, shape, definitions, segments, subsegments, error );
		} );
		*segmentCommitting = false;
		refresh( nif->getBlockIndex( *targetBlock ) );
		if ( !ok && notify ) QMessageBox::warning( mw, QObject::tr( "Edit Segments" ),
			error.isEmpty() ? QObject::tr( "The segment edit could not be stored as one Undo step." ) : error );
		return ok;
	};

	auto nextSegmentUserIndex = []( const QVector<RigSegmentDefinition> & definitions ) {
		quint32 next = 0;
		auto include = [&]( const RigSegmentSharedEntry & entry ) {
			if ( entry.userIndex != std::numeric_limits<quint32>::max() )
				next = qMax( next, entry.userIndex + 1U );
		};
		for ( const RigSegmentDefinition & definition : definitions ) {
			include( definition.parent );
			for ( const RigSegmentSharedEntry & child : definition.children ) include( child );
		}
		return next;
	};

	std::function<void( QTreeWidgetItem * )> editSegmentIds;
	editSegmentIds = [=]( QTreeWidgetItem * item ) {
		if ( !item ) return;
		const int type = item->data( 0, RiggingReceiverRowTypeRole ).toInt();
		if ( type != RiggingReceiverSegmentRow && type != RiggingReceiverSubsegmentRow ) return;
		const int segment = item->data( 0, RiggingReceiverSegmentRole ).toInt();
		const int subsegment = item->data( 0, RiggingReceiverSubsegmentRole ).toInt();
		if ( subsegment < 0 ) {
			QMessageBox::information( mw, QObject::tr( "Segment Identity" ), QObject::tr(
				"Top-level segments have no stored name. Their identity is their fixed array index (SEG %1). "
				"Subsegments expose editable User Index and Bone ID fields." ).arg( segment ) );
			return;
		}
		QVector<RigSegmentDefinition> current = riggingReadSegmentDefinitions( nif,
			nif->getBlockIndex( *targetBlock ) );
		if ( segment < 0 || segment >= current.size()
			|| ( subsegment >= 0 && subsegment >= current.at( segment ).children.size() ) ) return;
		const RigSegmentSharedEntry entry = subsegment >= 0
			? current.at( segment ).children.at( subsegment ) : current.at( segment ).parent;
		bool accepted = false;
		int oldUser = entry.userIndex == std::numeric_limits<quint32>::max() ? segment : int( qMin<quint32>( entry.userIndex, INT_MAX ) );
		int userIndex = QInputDialog::getInt( mw, QObject::tr( "Edit Segment ID" ),
			QObject::tr( "User Index (the numeric segment identifier stored in the NIF):" ),
			oldUser, 0, INT_MAX, 1, &accepted );
		if ( !accepted ) return;
		QString boneText;
		if ( subsegment >= 0 ) {
			boneText = entry.boneId == std::numeric_limits<quint32>::max()
				? QStringLiteral( "FFFFFFFF" ) : QString::number( entry.boneId, 16 ).toUpper();
			boneText = QInputDialog::getText( mw, QObject::tr( "Edit Subsegment Bone ID" ),
				QObject::tr( "Bone ID (8 hexadecimal digits; FFFFFFFF means none):" ),
				QLineEdit::Normal, boneText, &accepted ).trimmed();
			if ( !accepted ) return;
			bool boneOk = false;
			quint32 parsed = boneText.toUInt( &boneOk, 16 );
			if ( !boneOk ) {
				QMessageBox::warning( mw, QObject::tr( "Edit Segment ID" ),
					QObject::tr( "Bone ID must be an unsigned hexadecimal value." ) );
				return;
			}
			applySegmentMutation( QObject::tr( "Edit subsegment %1.%2 IDs" ).arg( segment ).arg( subsegment ),
				[=]( auto & definitions, auto &, auto &, QString & ) {
					definitions[segment].children[subsegment].userIndex = quint32( userIndex );
					definitions[segment].children[subsegment].boneId = parsed;
					return true;
				} );
		}
	};

	for ( const auto & action : actions ) {
		QObject::connect( action.first, &QPushButton::clicked, panel, [=]() {
			QModelIndex shape = nif->getBlockIndex( *targetBlock );
			if ( skope && riggingIsSkinnedShape( nif, shape ) )
				skope->castSpell( action.second, shape );
			refresh( nif->getBlockIndex( *targetBlock ) );
		} );
	}
	auto selectedDonorRefs = [=]() {
		QVector<RiggingContextBoneRef> requested;
		for ( QTreeWidgetItem * item : donorTree->selectedItems() ) {
			QVariant donorData = item->data( 0, Qt::UserRole );
			QVariant slotData = item->data( 0, Qt::UserRole + 1 );
			if ( !donorData.isValid() || !slotData.isValid() )
				continue;
			int donorBlock = donorData.toInt();
			int donorSlot = slotData.toInt();
			if ( donorBlock >= 0 && donorSlot >= 0 )
				requested.append( { donorBlock, donorSlot } );
		}
		return requested;
	};
	auto selectedDonorSegmentRefs = [=]() {
		QVector<RiggingContextSegmentRef> requested;
		for ( QTreeWidgetItem * item : donorTree->selectedItems() ) {
			const int type = item->data( 0, RiggingReceiverRowTypeRole ).toInt();
			if ( type != RiggingReceiverSegmentRow && type != RiggingReceiverSubsegmentRow ) continue;
			bool blockOk = false, segmentOk = false;
			int donorBlock = item->data( 0, Qt::UserRole ).toInt( &blockOk );
			int segment = item->data( 0, RiggingReceiverSegmentRole ).toInt( &segmentOk );
			int subsegment = item->data( 0, RiggingReceiverSubsegmentRole ).toInt();
			if ( blockOk && segmentOk ) requested.append( { donorBlock, segment, subsegment } );
		}
		return requested;
	};
	auto performContextAdd = [=]( const QVector<RiggingContextBoneRef> & requested, bool notify ) {
		if ( requested.isEmpty() ) return 0;
		stopPainting();
		resetHeatmap();
		QString error;
		int added = riggingBindContextBones( nif, *targetBlock, requested, error );
		refresh( nif->getBlockIndex( *targetBlock ) );
		if ( added < 0 ) {
			contextBoneStatus->setText( error );
			if ( notify ) QMessageBox::warning( mw, QObject::tr( "Selected-mesh Bone Transfer" ), error );
		} else if ( added == 0 ) {
			contextBoneStatus->setText( QObject::tr( "No new receiver bindings were needed." ) );
		} else {
			contextBoneStatus->setText( QObject::tr(
				"Added %1 bone binding(s) to the primary receiver as one Undo step. Donor meshes were unchanged." ).arg( added ) );
		}
		return added;
	};
	auto performContextTransfer = [=]( const QVector<RiggingContextBoneRef> & requested, bool notify ) {
		if ( requested.isEmpty() ) return 0;
		stopPainting();
		resetHeatmap();
		QString error;
		bool boundsUpdated = false;
		int changed = riggingTransferContextWeights( nif, *targetBlock, requested, boundsUpdated, error );
		refresh( nif->getBlockIndex( *targetBlock ) );
		if ( changed < 0 ) {
			contextBoneStatus->setText( error );
			if ( notify ) QMessageBox::warning( mw, QObject::tr( "Selected Bone Weight Transfer" ), error );
		} else if ( changed == 0 ) {
			contextBoneStatus->setText( QObject::tr( "The selected donor channels already match the receiver." ) );
		} else {
			contextBoneStatus->setText( boundsUpdated
				? QObject::tr( "Transferred selected bone channels onto %1 receiver vertices as one Undo step." ).arg( changed )
				: QObject::tr( "Transferred selected bone channels onto %1 vertices; bone bounds could not be recalculated." ).arg( changed ) );
		}
		return changed;
	};
	auto performContextSegmentTransfer = [=]( const QVector<RiggingContextSegmentRef> & requested,
		QTreeWidgetItem * receiver, bool notify ) {
		if ( requested.isEmpty() || !receiver ) return 0;
		const int type = receiver->data( 0, RiggingReceiverRowTypeRole ).toInt();
		if ( type != RiggingReceiverSegmentRow && type != RiggingReceiverSubsegmentRow ) return 0;
		const int segment = receiver->data( 0, RiggingReceiverSegmentRole ).toInt();
		const int subsegment = receiver->data( 0, RiggingReceiverSubsegmentRole ).toInt();
		stopSegmentPainting();
		resetHeatmap();
		QString error;
		*segmentCommitting = true;
		int changed = riggingTransferContextSegments( nif, *targetBlock, segment, subsegment, requested, error );
		*segmentCommitting = false;
		refresh( nif->getBlockIndex( *targetBlock ) );
		if ( changed < 0 ) {
			if ( notify ) QMessageBox::warning( mw, QObject::tr( "Segment Face Transfer" ), error );
		} else if ( changed == 0 ) {
			contextBoneStatus->setText( QObject::tr( "The selected donor binary membership already matches this receiver channel." ) );
		} else {
			contextBoneStatus->setText( QObject::tr(
				"Transferred nearest-surface binary membership onto %1 receiver faces as one Undo step." ).arg( changed ) );
		}
		return changed;
	};
	boneTree->donorDrop = [=]( QTreeWidget * source ) {
		if ( source != donorTree ) return;
		QVector<RiggingContextBoneRef> requested = selectedDonorRefs();
		QVector<RiggingContextSegmentRef> requestedSegments = selectedDonorSegmentRefs();
		if ( !requestedSegments.isEmpty() ) {
			performContextSegmentTransfer( requestedSegments, boneTree->currentItem(), true );
			return;
		}
		int added = performContextAdd( requested, true );
		if ( added == 0 && !requested.isEmpty() )
			contextBoneStatus->setText( QObject::tr(
				"Those donor bones are already bound. Use right-click Transfer Selected Weights to copy their channels." ) );
	};
	QObject::connect( contextBindButton, &QPushButton::clicked, panel, [=]() {
		performContextAdd( selectedDonorRefs(), true );
	} );

	QObject::connect( boneTree, &QTreeWidget::itemDoubleClicked, panel,
		[=]( QTreeWidgetItem * item, int column ) {
			if ( column == 1 && riggingIsReceiverBoneRow( item ) )
				boneTree->editItem( item, 1 );
			else if ( column == 1 )
				editSegmentIds( item );
		} );
	auto * renameShortcut = new QShortcut( QKeySequence( Qt::Key_F2 ), boneTree );
	renameShortcut->setContext( Qt::WidgetWithChildrenShortcut );
	QObject::connect( renameShortcut, &QShortcut::activated, panel, [=]() {
		if ( riggingIsReceiverBoneRow( boneTree->currentItem() ) )
			boneTree->editItem( boneTree->currentItem(), 1 );
		else
			editSegmentIds( boneTree->currentItem() );
	} );
	QObject::connect( boneTree, &QTreeWidget::itemChanged, panel,
		[=]( QTreeWidgetItem * item, int column ) {
			if ( *refreshingBoneTrees || !item || column != 1 )
				return;
			bool slotOk = false;
			int slot = item->data( 0, Qt::UserRole + 1 ).toInt( &slotOk );
			int node = item->data( 0, Qt::UserRole + 3 ).toInt();
			QString oldName = item->data( 0, Qt::UserRole + 2 ).toString();
			QString newName = item->text( 1 ).trimmed();
			auto restoreName = [=]() {
				QSignalBlocker blocker( boneTree );
				item->setText( 1, oldName.isEmpty() ? QObject::tr( "<unnamed>" ) : oldName );
			};
			if ( !slotOk || slot < 0 || node < 0 || newName == oldName ) {
				restoreName();
				return;
			}
			if ( newName.isEmpty() || newName == QObject::tr( "<unnamed>" ) ) {
				restoreName();
				QMessageBox::warning( mw, QObject::tr( "Rename Skin Bone" ),
					QObject::tr( "A skin-bone name cannot be empty." ) );
				return;
			}
			QStringList names = riggingBoneNames( nif, nif->getBlockIndex( *targetBlock ) );
			for ( int i = 0; i < names.size(); i++ ) {
				if ( i != slot && names.at( i ).compare( newName, Qt::CaseInsensitive ) == 0 ) {
					restoreName();
					QMessageBox::warning( mw, QObject::tr( "Rename Skin Bone" ),
						QObject::tr( "The receiver already contains a bone named '%1'." ).arg( newName ) );
					return;
				}
			}
			bool sharedOutsideReceiver = false;
			for ( int block = 0; block < nif->getBlockCount(); block++ ) {
				QModelIndex candidate = nif->getBlockIndex( block );
				if ( block == *targetBlock || !riggingIsSkinnedShape( nif, candidate ) ) continue;
				QModelIndex candidateBones = nif->getIndex( riggingSkinInstance( nif, candidate ), "Bones" );
				for ( int i = 0; i < nif->rowCount( candidateBones ); i++ ) {
					if ( nif->getLink( nif->getIndex( candidateBones, i ) ) == node ) {
						sharedOutsideReceiver = true;
						break;
					}
				}
				if ( sharedOutsideReceiver ) break;
			}
			QModelIndex sourceNode = nif->getBlockIndex( node );
			int parent = sharedOutsideReceiver ? riggingSceneParent( nif, node ) : -1;
			if ( sharedOutsideReceiver ) {
				QModelIndex receiverSkin = riggingSkinInstance( nif, nif->getBlockIndex( *targetBlock ) );
				int receiverSkinBlock = nif->getBlockNumber( receiverSkin );
				int receiverSkinOwners = 0;
				for ( int block = 0; block < nif->getBlockCount(); block++ )
					if ( riggingIsTriShape( nif, nif->getBlockIndex( block ) )
						&& nif->getLink( nif->getBlockIndex( block ), "Skin" ) == receiverSkinBlock ) receiverSkinOwners++;
				if ( receiverSkinOwners != 1 ) {
					restoreName();
					QMessageBox::warning( mw, QObject::tr( "Rename Skin Bone" ), QObject::tr(
						"The receiver's BSSkin::Instance is shared by another shape, so its binding cannot be redirected independently." ) );
					return;
				}
			}
			if ( sharedOutsideReceiver && ( parent < 0 || !Transform::canConstruct( nif, sourceNode ) ) ) {
				restoreName();
				QMessageBox::warning( mw, QObject::tr( "Rename Skin Bone" ), QObject::tr(
					"This bone is shared, but its single parent/transform cannot be read. A receiver-local clone was not created." ) );
				return;
			}
			Transform sourceTransform;
			quint32 sourceFlags = 0;
			if ( sharedOutsideReceiver ) {
				sourceTransform = Transform( nif, sourceNode );
				sourceFlags = nif->get<quint32>( sourceNode, "Flags" );
			}
			stopPainting();
			resetHeatmap();
			bool wrote = false;
			bool snapshot = nifSnapshotOp( nif,
				QObject::tr( "Rename skin bone '%1' to '%2'" ).arg( oldName, newName ), [&]() {
					if ( !sharedOutsideReceiver ) {
						wrote = nif->set<QString>( nif->getBlockIndex( node ), "Name", newName );
						return;
					}
					QPersistentModelIndex receiverSkin = riggingSkinInstance( nif, nif->getBlockIndex( *targetBlock ) );
					QModelIndex created = nif->insertNiBlock( "NiNode", -1 );
					if ( !created.isValid() ) return;
					nif->assignString( created, "Name", newName, false );
					wrote = nif->set<quint32>( created, "Flags", sourceFlags );
					sourceTransform.writeBack( nif, created );
					int clone = nif->getBlockNumber( created );
					wrote = addLink( nif, nif->getBlockIndex( parent ), "Children", clone ) && wrote;
					QModelIndex receiverBones = nif->getIndex( receiverSkin, "Bones" );
					wrote = nif->setLink( nif->getIndex( receiverBones, slot ), clone ) && wrote;
				} );
			if ( !snapshot || !wrote ) {
				restoreName();
				QMessageBox::warning( mw, QObject::tr( "Rename Skin Bone" ),
					QObject::tr( "The skeleton node could not be renamed as an Undo step." ) );
				return;
			}
			refresh( nif->getBlockIndex( *targetBlock ) );
			contextBoneStatus->setText( sharedOutsideReceiver
				? QObject::tr( "Created and rebound a receiver-local NiNode named '%1' at the original bone position. Donors keep '%2'." )
					.arg( newName, oldName )
				: QObject::tr( "Renamed receiver bone '%1' to '%2'." ).arg( oldName, newName ) );
		} );

	QObject::connect( donorTree, &QTreeWidget::customContextMenuRequested, panel,
		[=]( const QPoint & pos ) {
			QTreeWidgetItem * clicked = donorTree->itemAt( pos );
			if ( clicked && ( clicked->flags() & Qt::ItemIsSelectable ) && !clicked->isSelected() ) {
				donorTree->clearSelection();
				clicked->setSelected( true );
				donorTree->setCurrentItem( clicked );
			}
			QVector<RiggingContextBoneRef> requested = selectedDonorRefs();
			QVector<RiggingContextSegmentRef> requestedSegments = selectedDonorSegmentRefs();
			bool anyMissing = false, allBound = !requested.isEmpty();
			for ( QTreeWidgetItem * item : donorTree->selectedItems() ) {
				if ( item->data( 0, RiggingReceiverRowTypeRole ).toInt() != RiggingReceiverBoneRow
					|| !item->data( 0, Qt::UserRole ).isValid() ) continue;
				bool bound = item->data( 0, Qt::UserRole + 3 ).toBool();
				anyMissing = anyMissing || !bound;
				allBound = allBound && bound;
			}
			QMenu menu( donorTree );
			QAction * add = menu.addAction( QObject::tr( "Add Selected Bindings to Receiver" ) );
			add->setEnabled( anyMissing );
			QAction * transfer = menu.addAction( QObject::tr( "Transfer Selected Weights to Receiver" ) );
			transfer->setEnabled( allBound );
			QAction * addTransfer = menu.addAction( QObject::tr( "Add Bindings, then Transfer Selected Weights" ) );
			addTransfer->setEnabled( !requested.isEmpty() && anyMissing );
			QAction * transferSegments = menu.addAction( QObject::tr( "Transfer Selected Binary Faces to Active Receiver Segment" ) );
			const int receiverType = boneTree->currentItem()
				? boneTree->currentItem()->data( 0, RiggingReceiverRowTypeRole ).toInt() : -1;
			transferSegments->setEnabled( !requestedSegments.isEmpty()
				&& ( receiverType == RiggingReceiverSegmentRow || receiverType == RiggingReceiverSubsegmentRow ) );
			menu.addSeparator();
			QAction * selectVisible = menu.addAction( QObject::tr( "Select All Visible Donor Bones" ) );
			QAction * selectVisibleSegments = menu.addAction( QObject::tr( "Select All Visible Donor Segments" ) );
			QAction * chosen = menu.exec( donorTree->viewport()->mapToGlobal( pos ) );
			if ( chosen == add ) {
				performContextAdd( requested, true );
			} else if ( chosen == transfer ) {
				performContextTransfer( requested, true );
			} else if ( chosen == addTransfer ) {
				int added = performContextAdd( requested, true );
				if ( added >= 0 ) performContextTransfer( requested, true );
			} else if ( chosen == transferSegments ) {
				performContextSegmentTransfer( requestedSegments, boneTree->currentItem(), true );
			} else if ( chosen == selectVisible ) {
				donorTree->clearSelection();
				for ( int row = 0; row < donorTree->topLevelItemCount(); row++ ) {
					QTreeWidgetItem * donor = donorTree->topLevelItem( row );
					for ( int child = 0; child < donor->childCount(); child++ ) {
						QTreeWidgetItem * bone = donor->child( child );
						if ( !donor->isHidden() && !bone->isHidden()
							&& bone->data( 0, Qt::UserRole ).isValid()
							&& bone->data( 0, Qt::UserRole + 1 ).isValid()
							&& ( bone->flags() & Qt::ItemIsSelectable ) ) bone->setSelected( true );
					}
				}
			} else if ( chosen == selectVisibleSegments ) {
				donorTree->clearSelection();
				std::function<void( QTreeWidgetItem * )> selectSegments = [&]( QTreeWidgetItem * item ) {
					if ( !item || item->isHidden() ) return;
					const int type = item->data( 0, RiggingReceiverRowTypeRole ).toInt();
					if ( ( type == RiggingReceiverSegmentRow || type == RiggingReceiverSubsegmentRow )
						&& ( item->flags() & Qt::ItemIsSelectable ) ) item->setSelected( true );
					for ( int child = 0; child < item->childCount(); child++ ) selectSegments( item->child( child ) );
				};
				for ( int row = 0; row < donorTree->topLevelItemCount(); row++ )
					selectSegments( donorTree->topLevelItem( row ) );
			}
		} );

	QObject::connect( boneTree, &QTreeWidget::customContextMenuRequested, panel,
		[=]( const QPoint & pos ) {
			QTreeWidgetItem * clicked = boneTree->itemAt( pos );
			if ( clicked && !clicked->isSelected() ) {
				boneTree->clearSelection();
				clicked->setSelected( true );
				boneTree->setCurrentItem( clicked );
			}
			QSet<int> receiverSlots;
			QSet<QString> receiverNames;
			for ( QTreeWidgetItem * item : boneTree->selectedItems() ) {
				if ( !riggingIsReceiverBoneRow( item ) ) continue;
				bool ok = false;
				int slot = item->data( 0, Qt::UserRole + 1 ).toInt( &ok );
				if ( ok ) receiverSlots.insert( slot );
				receiverNames.insert( item->data( 0, Qt::UserRole + 2 ).toString() );
			}
			QVector<RiggingContextBoneRef> donorRefs = selectedDonorRefs();
			QVector<RiggingContextSegmentRef> donorSegmentRefs = selectedDonorSegmentRefs();
			QTreeWidgetItem * activeItem = boneTree->currentItem();
			const int activeType = activeItem
				? activeItem->data( 0, RiggingReceiverRowTypeRole ).toInt() : -1;
			const bool activeSegment = activeType == RiggingReceiverSegmentRow
				|| activeType == RiggingReceiverSubsegmentRow;
			const int activeSegmentIndex = activeSegment
				? activeItem->data( 0, RiggingReceiverSegmentRole ).toInt() : -1;
			const int activeSubsegmentIndex = activeSegment
				? activeItem->data( 0, RiggingReceiverSubsegmentRole ).toInt() : -1;
			bool donorAllBound = !donorRefs.isEmpty();
			for ( QTreeWidgetItem * item : donorTree->selectedItems() )
				if ( item->data( 0, Qt::UserRole ).isValid() )
					donorAllBound = donorAllBound && item->data( 0, Qt::UserRole + 3 ).toBool();
			QMenu menu( boneTree );
			QAction * addNew = menu.addAction( QObject::tr( "Add Brand-New Bone..." ) );
			addNew->setEnabled( riggingIsReceiverBoneRow( boneTree->currentItem() ) );
			QAction * rename = menu.addAction( QObject::tr( "Rename Active Bone (F2)" ) );
			rename->setEnabled( riggingIsReceiverBoneRow( boneTree->currentItem() ) );
			QAction * selectDonors = menu.addAction( QObject::tr( "Select Matching Donor Bones" ) );
			selectDonors->setEnabled( !receiverNames.isEmpty() );
			QAction * transfer = menu.addAction( QObject::tr( "Transfer Weights from Selected Donor Bones" ) );
			transfer->setEnabled( donorAllBound );
			menu.addSeparator();
			QAction * remove = menu.addAction( QObject::tr( "Delete Selected Bone(s) from Receiver..." ) );
			remove->setEnabled( !receiverSlots.isEmpty() );
			menu.addSeparator();
			QAction * addSegment = menu.addAction( QObject::tr( "Add New Segment" ) );
			addSegment->setEnabled( nif->blockInherits( nif->getBlockIndex( *targetBlock ), "BSSubIndexTriShape" ) );
			QAction * addSubsegment = menu.addAction( QObject::tr( "Add New Subsegment" ) );
			addSubsegment->setEnabled( activeSegment );
			QAction * editSegment = menu.addAction( activeSubsegmentIndex >= 0
				? QObject::tr( "Edit Active Subsegment IDs (F2)..." )
				: QObject::tr( "Explain Active Segment Identity (F2)..." ) );
			editSegment->setEnabled( activeSegment );
			QAction * transferSegment = menu.addAction( QObject::tr( "Transfer Selected Donor Binary Faces Here" ) );
			transferSegment->setEnabled( activeSegment && !donorSegmentRefs.isEmpty() );
			QAction * deleteSegment = menu.addAction( activeSubsegmentIndex >= 0
				? QObject::tr( "Delete Active Subsegment (Faces to Segment 0)..." )
				: QObject::tr( "Delete Active Segment (Faces to Segment 0)..." ) );
			deleteSegment->setEnabled( activeSegment && ( activeSubsegmentIndex >= 0 || activeSegmentIndex > 0 ) );
			QAction * chosen = menu.exec( boneTree->viewport()->mapToGlobal( pos ) );
			if ( chosen == addSegment ) {
				applySegmentMutation( QObject::tr( "Add segment" ),
					[=]( auto & definitions, auto &, auto &, QString & ) {
						RigSegmentDefinition added;
						added.parent.userIndex = quint32( definitions.size() );
						definitions.append( added );
						return true;
					} );
			} else if ( chosen == addSubsegment && activeSegment ) {
				applySegmentMutation( QObject::tr( "Add subsegment to segment %1" ).arg( activeSegmentIndex ),
					[=]( auto & definitions, auto &, auto &, QString & error ) {
						if ( activeSegmentIndex < 0 || activeSegmentIndex >= definitions.size() ) {
							error = QObject::tr( "The active segment no longer exists." );
							return false;
						}
						RigSegmentSharedEntry added;
						added.userIndex = nextSegmentUserIndex( definitions );
						definitions[activeSegmentIndex].children.append( added );
						return true;
					} );
			} else if ( chosen == editSegment ) {
				editSegmentIds( activeItem );
			} else if ( chosen == transferSegment ) {
				performContextSegmentTransfer( donorSegmentRefs, activeItem, true );
			} else if ( chosen == deleteSegment && activeSegment ) {
				const QString kind = activeSubsegmentIndex >= 0 ? QObject::tr( "subsegment" ) : QObject::tr( "segment" );
				if ( QMessageBox::question( mw, QObject::tr( "Delete Segment" ), QObject::tr(
					"Delete this %1? Every face assigned to it will be reassigned to Segment 0. This is one Undo step." ).arg( kind ) )
					!= QMessageBox::Yes ) return;
				applySegmentMutation( QObject::tr( "Delete %1" ).arg( kind ),
					[=]( auto & definitions, auto & segments, auto & subsegments, QString & error ) {
						if ( activeSegmentIndex < 0 || activeSegmentIndex >= definitions.size() ) {
							error = QObject::tr( "The active segment no longer exists." );
							return false;
						}
						if ( activeSubsegmentIndex >= 0 ) {
							if ( activeSubsegmentIndex >= definitions.at( activeSegmentIndex ).children.size() ) return false;
							for ( int face = 0; face < segments.size(); face++ ) {
								if ( segments[face] != activeSegmentIndex ) continue;
								if ( subsegments[face] == activeSubsegmentIndex ) {
									segments[face] = 0;
									subsegments[face] = -1;
								} else if ( subsegments[face] > activeSubsegmentIndex ) {
									subsegments[face]--;
								}
							}
							definitions[activeSegmentIndex].children.removeAt( activeSubsegmentIndex );
						} else {
							if ( activeSegmentIndex == 0 ) {
								error = QObject::tr( "Segment 0 is the required fallback and cannot be deleted." );
								return false;
							}
							for ( int face = 0; face < segments.size(); face++ ) {
								if ( segments[face] == activeSegmentIndex ) {
									segments[face] = 0;
									subsegments[face] = -1;
								} else if ( segments[face] > activeSegmentIndex ) {
									segments[face]--;
								}
							}
							definitions.removeAt( activeSegmentIndex );
						}
						return true;
					} );
			} else if ( chosen == addNew && riggingIsReceiverBoneRow( boneTree->currentItem() ) ) {
				bool slotOk = false;
				int templateSlot = boneTree->currentItem()->data( 0, Qt::UserRole + 1 ).toInt( &slotOk );
				QStringList existing = riggingBoneNames( nif, nif->getBlockIndex( *targetBlock ) );
				QString suggestion = QStringLiteral( "NewBone" );
				int suffix = 1;
				auto containsName = [&]( const QString & candidate ) {
					for ( const QString & name : existing )
						if ( name.compare( candidate, Qt::CaseInsensitive ) == 0 ) return true;
					return false;
				};
				while ( containsName( suggestion ) ) suggestion = QStringLiteral( "NewBone_%1" ).arg( suffix++ );
				bool accepted = false;
				QString name = QInputDialog::getText( mw, QObject::tr( "Add Brand-New Receiver Bone" ),
					QObject::tr( "Bone name:" ), QLineEdit::Normal, suggestion, &accepted ).trimmed();
				if ( !accepted || !slotOk ) return;
				stopPainting();
				resetHeatmap();
				QString error;
				int newSlot = riggingAddNewReceiverBone( nif, *targetBlock, templateSlot, name, error );
				refresh( nif->getBlockIndex( *targetBlock ) );
				if ( newSlot < 0 ) {
					contextBoneStatus->setText( error );
					QMessageBox::warning( mw, QObject::tr( "Add Brand-New Receiver Bone" ), error );
				} else {
					QTreeWidgetItem * created = boneTree->topLevelItem( newSlot );
					if ( created ) {
						created->setSelected( true );
						boneTree->setCurrentItem( created );
					}
					contextBoneStatus->setText( QObject::tr(
						"Created a brand-new NiNode '%1' beside the active bone and appended a zero-influence receiver binding." ).arg( name ) );
				}
			} else if ( chosen == rename && riggingIsReceiverBoneRow( boneTree->currentItem() ) ) {
				boneTree->editItem( boneTree->currentItem(), 1 );
			} else if ( chosen == selectDonors ) {
				donorTree->clearSelection();
				for ( int row = 0; row < donorTree->topLevelItemCount(); row++ ) {
					QTreeWidgetItem * donor = donorTree->topLevelItem( row );
					for ( int child = 0; child < donor->childCount(); child++ ) {
						QTreeWidgetItem * bone = donor->child( child );
						if ( receiverNames.contains( bone->data( 0, Qt::UserRole + 2 ).toString() )
							&& ( bone->flags() & Qt::ItemIsSelectable ) ) {
							bone->setSelected( true );
							donor->setExpanded( true );
						}
					}
				}
			} else if ( chosen == transfer ) {
				performContextTransfer( donorRefs, true );
			} else if ( chosen == remove ) {
				QStringList labels;
				for ( QTreeWidgetItem * item : boneTree->selectedItems() )
					if ( riggingIsReceiverBoneRow( item ) ) labels << item->text( 1 );
				if ( QMessageBox::question( mw, QObject::tr( "Delete Receiver Bones" ),
					QObject::tr( "Delete %1 selected receiver bone binding(s) and their vertex influences?\n\n%2\n\n"
						"Exclusive unused leaf NiNodes are deleted; shared donor/skeleton nodes are kept. This is one Undo step." )
						.arg( receiverSlots.size() ).arg( labels.join( QStringLiteral( "\n" ) ) ) ) != QMessageBox::Yes )
					return;
				stopPainting();
				resetHeatmap();
				QString error;
				bool boundsUpdated = false;
				int removed = riggingRemoveContextBones( nif, *targetBlock, receiverSlots, boundsUpdated, error );
				refresh( nif->getBlockIndex( *targetBlock ) );
				if ( removed < 0 ) {
					contextBoneStatus->setText( error );
					QMessageBox::warning( mw, QObject::tr( "Delete Receiver Bones" ), error );
				} else {
					contextBoneStatus->setText( boundsUpdated
						? QObject::tr( "Removed %1 receiver binding(s) and remapped their weights as one Undo step." ).arg( removed )
						: QObject::tr( "Removed %1 receiver binding(s); bone bounds could not be recalculated." ).arg( removed ) );
				}
			}
	} );
	if ( ogl )
		QObject::connect( ogl, &GLView::objectSelectionChanged, panel, [=]() {
			QElapsedTimer perfT;	// TEMP DIAGNOSTIC (WW_PERF_TEST)
			perfT.start();
			refresh( skope ? skope->currentNifIndex() : QModelIndex() );
			if ( qEnvironmentVariableIsSet( "WW_PERF_TEST" ) ) {
				QFile f( QApplication::applicationDirPath() + "/ww_perf_test.log" );
				if ( f.open( QIODevice::Append | QIODevice::Text ) )
					QTextStream( &f ) << "    [rigging refresh: " << perfT.elapsed()
						<< " ms, visible=" << panel->isVisible() << "]\n";
			}
		} );
	if ( skope ) {
		QObject::connect( skope, &NifSkope::currentNifIndexChanged, panel, refresh );
		QObject::connect( skope, &NifSkope::completeLoading, panel,
			[=]( bool, QString & ) {
				stopPainting();
				stopSegmentPainting();
				resetOverlay();
				resetHeatmap();
				*targetBlock = -1;
				QTimer::singleShot( 0, panel, [=]() { refresh( skope->currentNifIndex() ); } );
			} );
	}
	QObject::connect( nif, &QAbstractItemModel::modelReset, panel,
		[=]() {
			stopPainting();
			stopSegmentPainting();
			resetOverlay();
			resetHeatmap();
			QTimer::singleShot( 0, panel, [=]() { refresh( QModelIndex() ); } );
		} );
	QObject::connect( nif, &QAbstractItemModel::rowsInserted, panel,
		[=]() {
			if ( !overlaySoup->isEmpty() ) resetOverlay();
			if ( !heatmapSoup->isEmpty() ) resetHeatmap();
		} );
	QObject::connect( nif, &QAbstractItemModel::rowsRemoved, panel,
		[=]() {
			if ( !overlaySoup->isEmpty() ) resetOverlay();
			if ( !heatmapSoup->isEmpty() ) resetHeatmap();
		} );
	if ( nif->undoStack )
		QObject::connect( nif->undoStack, &QUndoStack::indexChanged, panel,
			[=]() {
				if ( *paintCommitting || *segmentCommitting )
					return;
				stopPainting();
				stopSegmentPainting();
				if ( !overlaySoup->isEmpty() ) resetOverlay();
				if ( !heatmapSoup->isEmpty() ) resetHeatmap();
			} );
	QObject::connect( dock, &QDockWidget::visibilityChanged, panel, [=]( bool visible ) {
		if ( !ogl )
			return;
		if ( visible )
			applyOverlay();
		else
			ogl->clearRiggingDonorPreview();
		if ( visible )
			applyHeatmap();
		else {
			stopPainting();
			stopSegmentPainting();
			ogl->clearRiggingWeightPreview();
		}
	} );
	if ( ogl )
		QObject::connect( panel, &QObject::destroyed, ogl, [=]() {
			ogl->setRiggingWeightPaintMode( false );
			ogl->setSegmentPaintMode( false );
			ogl->clearRiggingDonorPreview();
			ogl->clearRiggingWeightPreview();
		} );

	layout->addStretch( 1 );
	auto * scroll = new QScrollArea( dock );
	scroll->setObjectName( QStringLiteral( "RiggingToolsScrollArea" ) );
	scroll->setWidgetResizable( true );
	scroll->setFrameShape( QFrame::NoFrame );
	scroll->setHorizontalScrollBarPolicy( Qt::ScrollBarAlwaysOff );
	scroll->setVerticalScrollBarPolicy( Qt::ScrollBarAsNeeded );
	scroll->setWidget( panel );
	dock->setWidget( scroll );
	mw->addDockWidget( Qt::RightDockWidgetArea, dock );
	dock->hide();
	QTimer::singleShot( 0, panel,
		[=]() { refresh( skope ? skope->currentNifIndex() : QModelIndex() ); } );
	return dock;
}

QDockWidget * tlCreateVertexPaintManagerDock( NifModel * nif, QMainWindow * mw, GLView * ogl )
{
	auto * dock = new QDockWidget( QObject::tr( "Vertex Paint Manager" ), mw );
	dock->setObjectName( QStringLiteral( "VertexPaintManagerDock" ) );
	dock->setAllowedAreas( Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea );
	auto * panel = new QWidget( dock );
	auto * layout = new QVBoxLayout( panel );
	layout->setContentsMargins( 6, 6, 6, 6 );
	layout->setSpacing( 5 );

	auto * targetHeading = new QLabel( QObject::tr( "Active paint mesh" ), panel );
	targetHeading->setStyleSheet( QStringLiteral( "QLabel { font-weight: 600; }" ) );
	layout->addWidget( targetHeading );
	auto * targetStatus = new QLabel( QObject::tr( "Select a BSTriShape." ), panel );
	targetStatus->setObjectName( QStringLiteral( "VertexPaintTargetStatus" ) );
	targetStatus->setWordWrap( true );
	layout->addWidget( targetStatus );

	auto * paintGroup = new QGroupBox( QObject::tr( "Vertex painting" ), panel );
	paintGroup->setObjectName( QStringLiteral( "VertexPaintControls" ) );
	auto * paintLayout = new QVBoxLayout( paintGroup );
	paintLayout->setContentsMargins( 6, 8, 6, 6 );
	paintLayout->setSpacing( 4 );
	auto * topRow = new QHBoxLayout;
	topRow->addWidget( new QLabel( QObject::tr( "Channel" ), paintGroup ) );
	auto * channel = new QComboBox( paintGroup );
	channel->setObjectName( QStringLiteral( "VertexPaintChannel" ) );
	channel->addItems( { QObject::tr( "Color (RGB)" ), QObject::tr( "Alpha" ) } );
	channel->setToolTip( QObject::tr(
		"RGB painting preserves vertex alpha. Alpha painting preserves the vertex RGB colour." ) );
	topRow->addWidget( channel, 1 );
	auto * paintButton = new QPushButton( QObject::tr( "Start Painting" ), paintGroup );
	paintButton->setObjectName( QStringLiteral( "VertexPaintButton" ) );
	paintButton->setCheckable( true );
	paintButton->setEnabled( false );
	topRow->addWidget( paintButton );
	paintLayout->addLayout( topRow );

	auto makeSlider = [=]( const QString & name, const QString & caption, int minimum,
		int maximum, int value, QSlider *& slider, QLabel *& valueLabel ) {
		auto * row = new QHBoxLayout;
		row->addWidget( new QLabel( caption, paintGroup ) );
		slider = new QSlider( Qt::Horizontal, paintGroup );
		slider->setObjectName( name );
		slider->setRange( minimum, maximum );
		slider->setValue( value );
		valueLabel = new QLabel( paintGroup );
		valueLabel->setMinimumWidth( 34 );
		valueLabel->setAlignment( Qt::AlignRight | Qt::AlignVCenter );
		row->addWidget( slider, 1 );
		row->addWidget( valueLabel );
		paintLayout->addLayout( row );
	};
	QSlider * alpha = nullptr, * strength = nullptr, * radius = nullptr;
	QLabel * alphaValue = nullptr, * strengthValue = nullptr, * radiusValue = nullptr;
	makeSlider( QStringLiteral( "VertexPaintAlpha" ), QObject::tr( "Alpha" ),
		0, 100, 100, alpha, alphaValue );
	makeSlider( QStringLiteral( "VertexPaintStrength" ), QObject::tr( "Strength" ),
		1, 100, 25, strength, strengthValue );
	makeSlider( QStringLiteral( "VertexPaintRadius" ), QObject::tr( "Radius" ),
		4, 200, 32, radius, radiusValue );
	auto * paintStatus = new QLabel( QObject::tr(
		"LMB paints. Disable the Brush toolbar toggle to select vertices, edges, or faces." ), paintGroup );
	paintStatus->setObjectName( QStringLiteral( "VertexPaintStatus" ) );
	paintStatus->setWordWrap( true );
	paintStatus->setStyleSheet( QStringLiteral( "QLabel { color: palette(mid); }" ) );
	paintLayout->addWidget( paintStatus );
	layout->addWidget( paintGroup );

	// This is the complete NifSkope color chooser, docked in live-edit mode.
	auto * colorPicker = new ColorPickerPanel( QColor( 255, 255, 255 ), false, panel );
	colorPicker->setObjectName( QStringLiteral( "VertexPaintColorPicker" ) );
	layout->addWidget( colorPicker );
	layout->addStretch( 1 );

	NifSkope * skope = qobject_cast<NifSkope *>( mw );
	auto targetBlock = std::make_shared<int>( -1 );
	auto brushColor = std::make_shared<QColor>( QColor( 255, 255, 255 ) );
	auto stroke = std::make_shared<QMap<int, float>>();
	auto base = std::make_shared<QVector<Color4>>();
	auto preview = std::make_shared<QVector<Color4>>();
	auto changed = std::make_shared<QSet<int>>();
	auto strokeTarget = std::make_shared<int>( -1 );
	auto committing = std::make_shared<bool>( false );

	auto stopPainting = [=]() {
		stroke->clear();
		changed->clear();
		*strokeTarget = -1;
		if ( paintButton->isChecked() ) {
			QSignalBlocker blocker( paintButton );
			paintButton->setChecked( false );
		}
		paintButton->setText( QObject::tr( "Start Painting" ) );
		if ( ogl && ogl->vertexPaintModeActive() )
			ogl->setVertexPaintMode( false );
	};

	std::function<void( const QModelIndex & )> refreshTarget = [=]( const QModelIndex & current ) {
		int selectedBlock = nif->getBlockNumber( current );
		if ( ogl ) {
			const int activeBlock = ogl->activeObjectBlock();
			if ( ogl->selectedObjectBlocks().contains( activeBlock )
				&& nif->blockInherits( nif->getBlockIndex( activeBlock ), "BSTriShape" ) )
				selectedBlock = activeBlock;
		}
		QModelIndex selected = nif->getBlockIndex( selectedBlock );
		const bool valid = nif->blockInherits( selected, "BSTriShape" );
		if ( valid ) {
			if ( *targetBlock >= 0 && *targetBlock != selectedBlock && !*committing )
				stopPainting();
			*targetBlock = selectedBlock;
			const QString name = nif->get<QString>( selected, "Name" );
			const int vertices = nif->rowCount( nif->getIndex( selected, "Vertex Data" ) );
			const bool hasColors = nif->get<BSVertexDesc>( selected, "Vertex Desc" )
				.HasFlag( VertexAttribute::VA_COLOR );
			targetStatus->setText( QObject::tr( "%1  [block %2]\n%3 vertices · RGBA channel: %4" )
				.arg( name.isEmpty() ? QObject::tr( "<unnamed shape>" ) : name )
				.arg( selectedBlock ).arg( vertices )
				.arg( hasColors ? QObject::tr( "ready" ) : QObject::tr( "will initialize to opaque white" ) ) );
		} else if ( !ogl || !ogl->vertexPaintModeActive() ) {
			*targetBlock = -1;
			targetStatus->setText( QObject::tr( "Select a BSTriShape in the block list or viewport." ) );
		}
		paintButton->setEnabled( *targetBlock >= 0 && ogl );
	};

	colorPicker->setColorChangedCallback( [=]( const QColor & selected ) {
		if ( !selected.isValid() ) return;
		*brushColor = selected;
		brushColor->setAlpha( 255 );
	} );
	auto refreshControls = [=]() {
		alphaValue->setText( QString::number( alpha->value() ) + QStringLiteral( "%" ) );
		strengthValue->setText( QString::number( strength->value() ) + QStringLiteral( "%" ) );
		radiusValue->setText( QString::number( radius->value() ) + QStringLiteral( " px" ) );
		const bool rgb = channel->currentIndex() == 0;
		colorPicker->setEnabled( rgb );
		alpha->setEnabled( !rgb );
		if ( paintButton->isChecked() && ogl )
			ogl->setVertexPaintMode( true, *targetBlock, float( radius->value() ) );
	};
	QObject::connect( channel, &QComboBox::currentIndexChanged, panel, [=]() { refreshControls(); } );
	QObject::connect( alpha, &QSlider::valueChanged, panel, [=]() { refreshControls(); } );
	QObject::connect( strength, &QSlider::valueChanged, panel, [=]() { refreshControls(); } );
	QObject::connect( radius, &QSlider::valueChanged, panel, [=]() { refreshControls(); } );
	refreshControls();

	QObject::connect( paintButton, &QPushButton::toggled, panel, [=]( bool enabled ) {
		QModelIndex shape = nif->getBlockIndex( *targetBlock );
		if ( enabled && ( !ogl || !nif->blockInherits( shape, "BSTriShape" ) ) ) {
			stopPainting();
			paintStatus->setText( QObject::tr( "Select a valid BSTriShape first." ) );
			return;
		}
		paintButton->setText( enabled ? QObject::tr( "Stop Painting" ) : QObject::tr( "Start Painting" ) );
		if ( enabled ) {
			QString error;
			*committing = true;
			const bool ready = riggingEnsureVertexColors( nif, shape, error );
			*committing = false;
			if ( !ready || !riggingReadVertexColors( nif, shape, *base, error ) ) {
				stopPainting();
				paintStatus->setText( error );
				QMessageBox::warning( mw, QObject::tr( "Vertex Paint" ), error );
				return;
			}
			*preview = *base;
			paintStatus->setText( QObject::tr(
				"LMB paints continuously; wheel zooms. Selected geometry masks painting; an empty selection paints everywhere. Tab returns to Object Mode." ) );
			ogl->setVertexPaintMode( true, *targetBlock, float( radius->value() ) );
			refreshTarget( shape );
		} else if ( ogl ) {
			ogl->setVertexPaintMode( false );
		}
	} );

	if ( ogl ) {
		QObject::connect( ogl, &GLView::vertexPaintModeChanged, panel, [=]( bool enabled ) {
			if ( !enabled && paintButton->isChecked() ) {
				QSignalBlocker blocker( paintButton );
				paintButton->setChecked( false );
				paintButton->setText( QObject::tr( "Start Painting" ) );
			}
		} );
		QObject::connect( ogl, &GLView::vertexPaintBrushChanged, panel, [=]( bool brushEnabled ) {
			if ( !ogl->vertexPaintModeActive() ) return;
			paintStatus->setText( brushEnabled
				? QObject::tr( "Brush: LMB paints; wheel zooms. Selected geometry masks painting; an empty selection paints everywhere." )
				: QObject::tr( "Selection: 1/2/3 chooses vertices, edges, or faces. Selection, invert, linked, grow/shrink, and hide remain available." ) );
		} );
		QObject::connect( ogl, &GLView::vertexPaintStrokeBegan, panel, [=]() {
			stroke->clear();
			changed->clear();
			*strokeTarget = *targetBlock;
			QString error;
			if ( !riggingReadVertexColors( nif, nif->getBlockIndex( *targetBlock ), *base, error ) ) {
				paintStatus->setText( error );
				return;
			}
			*preview = *base;
		} );
		QObject::connect( ogl, &GLView::vertexPaintBrushSample, panel,
			[=]( int target, const QVector<int> & vertices, const QVector<float> & falloff ) {
				if ( target != *strokeTarget || vertices.size() != falloff.size()
					|| base->size() != preview->size() ) return;
				const float brushStrength = float( strength->value() ) / 100.0f;
				const bool rgb = channel->currentIndex() == 0;
				const FloatVector4 brush( float( brushColor->redF() ), float( brushColor->greenF() ),
					float( brushColor->blueF() ), float( alpha->value() ) / 100.0f );
				for ( int i = 0; i < vertices.size(); i++ ) {
					const int vertex = vertices.at( i );
					if ( vertex < 0 || vertex >= base->size() ) continue;
					float & amount = ( *stroke )[vertex];
					amount = qMin( 1024.0f, amount + qMax( 0.0f, falloff.at( i ) ) );
					const float blend = 1.0f - std::pow(
						qBound( 0.0f, 1.0f - brushStrength, 1.0f ), amount );
					Color4 result = base->at( vertex );
					if ( rgb ) {
						for ( int component = 0; component < 3; component++ )
							result[component] += ( brush[component] - result[component] ) * blend;
					} else {
						result[3] += ( brush[3] - result[3] ) * blend;
					}
					( *preview )[vertex] = result;
					changed->insert( vertex );
				}
				ogl->setVertexPaintPreviewColors( target, *preview );
			} );
		QObject::connect( ogl, &GLView::vertexPaintStrokeEnded, panel, [=]( bool commit ) {
			if ( !commit || changed->isEmpty() || *strokeTarget != *targetBlock ) {
				if ( *strokeTarget >= 0 && !base->isEmpty() )
					ogl->setVertexPaintPreviewColors( *strokeTarget, *base );
				stroke->clear();
				changed->clear();
				return;
			}
			QString error;
			*committing = true;
			const int count = riggingCommitVertexColors( nif, nif->getBlockIndex( *targetBlock ),
				*preview, *changed, error );
			*committing = false;
			stroke->clear();
			changed->clear();
			if ( count < 0 ) {
				paintStatus->setText( error );
				QMessageBox::warning( mw, QObject::tr( "Vertex Paint" ), error );
				ogl->setVertexPaintPreviewColors( *targetBlock, *base );
			} else {
				paintStatus->setText( QObject::tr( "Painted %1 vertices as one Undo step." ).arg( count ) );
				*base = *preview;
				ogl->setVertexPaintPreviewColors( *targetBlock, *preview );
			}
		} );
		QObject::connect( ogl, &GLView::objectSelectionChanged, panel, [=]() {
			QElapsedTimer perfT;	// TEMP DIAGNOSTIC (WW_PERF_TEST)
			perfT.start();
			refreshTarget( skope ? skope->currentNifIndex() : QModelIndex() );
			if ( qEnvironmentVariableIsSet( "WW_PERF_TEST" ) ) {
				QFile f( QApplication::applicationDirPath() + "/ww_perf_test.log" );
				if ( f.open( QIODevice::Append | QIODevice::Text ) )
					QTextStream( &f ) << "    [vertex-paint refreshTarget: " << perfT.elapsed()
						<< " ms, visible=" << panel->isVisible() << "]\n";
			}
		} );
	}
	if ( skope ) {
		QObject::connect( skope, &NifSkope::currentNifIndexChanged, panel, refreshTarget );
		QObject::connect( skope, &NifSkope::completeLoading, panel, [=]( bool, QString & ) {
			stopPainting();
			*targetBlock = -1;
			QTimer::singleShot( 0, panel, [=]() { refreshTarget( skope->currentNifIndex() ); } );
		} );
	}
	QObject::connect( nif, &QAbstractItemModel::modelReset, panel, [=]() {
		stopPainting();
		*targetBlock = -1;
		QTimer::singleShot( 0, panel, [=]() {
			refreshTarget( skope ? skope->currentNifIndex() : QModelIndex() );
		} );
	} );
	if ( nif->undoStack )
		QObject::connect( nif->undoStack, &QUndoStack::indexChanged, panel, [=]() {
			if ( *committing ) return;
			stopPainting();
			QTimer::singleShot( 0, panel, [=]() {
				refreshTarget( skope ? skope->currentNifIndex() : QModelIndex() );
			} );
		} );
	QObject::connect( dock, &QDockWidget::visibilityChanged, panel, [=]( bool visible ) {
		if ( !visible ) stopPainting();
	} );
	if ( ogl )
		QObject::connect( panel, &QObject::destroyed, ogl, [=]() {
			ogl->setVertexPaintMode( false );
		} );

	auto * scroll = new QScrollArea( dock );
	scroll->setObjectName( QStringLiteral( "VertexPaintScrollArea" ) );
	scroll->setWidgetResizable( true );
	scroll->setFrameShape( QFrame::NoFrame );
	scroll->setHorizontalScrollBarPolicy( Qt::ScrollBarAlwaysOff );
	scroll->setVerticalScrollBarPolicy( Qt::ScrollBarAsNeeded );
	scroll->setWidget( panel );
	dock->setWidget( scroll );
	mw->addDockWidget( Qt::RightDockWidgetArea, dock );
	dock->hide();
	QTimer::singleShot( 0, panel, [=]() {
		refreshTarget( skope ? skope->currentNifIndex() : QModelIndex() );
	} );
	return dock;
}
