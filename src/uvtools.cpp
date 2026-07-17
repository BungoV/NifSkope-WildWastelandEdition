/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

//! @file uvtools.cpp Blender-style UV editing workspace (dockable 2D editor).
//!
//! NIF stores one UV per vertex (not per face corner like Blender), so every
//! UV point here corresponds 1:1 to a mesh vertex and selection mirrors the
//! 3D viewport's edit-mode element selection both ways. All UV writes go
//! through the model undo stack as one merged transaction per gesture.

#include "glview.h"
#include "nifskope.h"
#include "nifsnapshot.h"
#include "gl/glcontext.hpp"
#include "gl/gltex.h"
#include "gl/glscene.h"
#include "model/nifmodel.h"
#include "model/undocommands.h"
#include "data/niftypes.h"

#include <QBuffer>

#include "libfo76utils/src/fp32vec4.hpp"

#include <QAbstractSpinBox>
#include <QAction>
#include <QApplication>
#include <QElapsedTimer>
#include <QFile>
#include <QCheckBox>
#include <QComboBox>
#include <QCursor>
#include <QDialog>
#include <QDockWidget>
#include <QDoubleSpinBox>
#include <QEnterEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMainWindow>
#include <QMenu>
#include <QImage>
#include <QMouseEvent>
#include <QOpenGLContext>
#include <QOpenGLWidget>
#include <QPainter>
#include <QSettings>
#include <QSurfaceFormat>
#include <QTimer>
#include <QToolButton>
#include <QUndoStack>
#include <QVBoxLayout>
#include <QWidgetAction>
#include <QWheelEvent>

#include <cmath>
#include <functional>

namespace
{

//! A QMenu that cancels itself once the pointer moves well clear of it — the
//! same behaviour (and hover-out distance) as the 3D viewport's operator
//! pop-ups (AutoCloseMenu in glview.cpp), so the UV editor's small tool menus
//! (Merge / Snap / Unwrap / Mirror / Layout) feel identical, Blender-style.
class UVAutoCloseMenu final : public QMenu
{
public:
	explicit UVAutoCloseMenu( QWidget * parent = nullptr ) : QMenu( parent )
	{
		m_timer.setInterval( 60 );
		connect( &m_timer, &QTimer::timeout, this, [this]() {
			if ( !geometry().adjusted( -46, -46, 46, 46 ).contains( QCursor::pos() ) )
				close();
		} );
	}
protected:
	void showEvent( QShowEvent * e ) override { QMenu::showEvent( e ); m_timer.start(); }
	void hideEvent( QHideEvent * e ) override { m_timer.stop(); QMenu::hideEvent( e ); }
private:
	QTimer m_timer;
};

//! Blender-style scrubbable number field — an exact clone of the 3D viewport
//! redo panels' DragSpinBox (nifskope_ui.cpp): hold LMB on the value and drag
//! left/right to change it (Shift = fine), click to type, hover shows ‹ ›
//! step arrows, clicks in the side margins step the value.
class UVDragSpinBox final : public QDoubleSpinBox
{
public:
	explicit UVDragSpinBox( QWidget * parent = nullptr ) : QDoubleSpinBox( parent )
	{
		setButtonSymbols( QAbstractSpinBox::NoButtons );	// we draw ‹ › ourselves
		setAlignment( Qt::AlignCenter );
		setStyleSheet( QStringLiteral(
			"QDoubleSpinBox { background: #545454; border: none; border-radius: 3px; color: #e6e6e6; }"
			"QLineEdit { background: transparent; border: none; color: #e6e6e6;"
			" selection-background-color: #4772b3; selection-color: #ffffff; }" ) );
		if ( QLineEdit * le = lineEdit() ) {
			// the spin box's internal line edit gets the mouse events, so drive
			// the drag from an event filter on it (a subclass override never fires)
			le->installEventFilter( this );
			le->setMouseTracking( true );
			le->setCursor( Qt::SizeHorCursor );
		}
	}
protected:
	static constexpr int arrowW = 16;
	void resizeEvent( QResizeEvent * e ) override
	{
		QDoubleSpinBox::resizeEvent( e );
		if ( QLineEdit * le = lineEdit() )	// inset so the ‹ › arrows have room
			le->setGeometry( arrowW, 0, std::max( width() - 2 * arrowW, 0 ), height() );
	}
	void enterEvent( QEnterEvent * e ) override
	{
		QDoubleSpinBox::enterEvent( e );
		m_hover = true;		// hover reveals the ‹ › arrows (Blender)
		update();
	}
	void leaveEvent( QEvent * e ) override
	{
		QDoubleSpinBox::leaveEvent( e );
		m_hover = false;
		update();
	}
	void mousePressEvent( QMouseEvent * e ) override
	{
		// clicks in the side margins (outside the inset line edit) step the value
		if ( e->button() == Qt::LeftButton ) {
			int x = int( e->position().x() );
			if ( x < arrowW ) { stepBy( -1 ); Q_EMIT editingFinished(); return; }
			if ( x > width() - arrowW ) { stepBy( 1 ); Q_EMIT editingFinished(); return; }
		}
		QDoubleSpinBox::mousePressEvent( e );
	}
	bool eventFilter( QObject * o, QEvent * ev ) override
	{
		QLineEdit * le = lineEdit();
		if ( o == le ) {
			if ( ev->type() == QEvent::MouseButtonPress ) {
				auto me = static_cast<QMouseEvent *>( ev );
				if ( me->button() == Qt::LeftButton ) {
					m_dragging = true;
					m_moved = false;
					m_pressX = int( me->globalPosition().x() );
					m_startVal = value();
					return true;	// swallow: don't let the line edit start selecting
				}
			} else if ( ev->type() == QEvent::MouseMove && m_dragging ) {
				auto me = static_cast<QMouseEvent *>( ev );
				int dx = int( me->globalPosition().x() ) - m_pressX;
				if ( !m_moved && std::abs( dx ) > 2 ) {
					m_moved = true;
					update();	// light up the field while scrubbing (Blender)
				}
				if ( m_moved ) {
					double scale = ( me->modifiers() & Qt::ShiftModifier ) ? 0.01 : 0.1;
					setValue( m_startVal + double( dx ) * singleStep() * scale );
				}
				return true;
			} else if ( ev->type() == QEvent::MouseButtonRelease && m_dragging ) {
				m_dragging = false;
				const bool finishedScrub = m_moved;
				if ( !m_moved ) {		// a plain click: edit the value
					le->selectAll();
					le->setFocus();
				}
				m_moved = false;
				update();
				if ( finishedScrub )
					Q_EMIT editingFinished();
				return true;
			}
		}
		return QDoubleSpinBox::eventFilter( o, ev );
	}
	void paintEvent( QPaintEvent * ev ) override
	{
		QDoubleSpinBox::paintEvent( ev );
		QPainter p( this );
		p.setRenderHint( QPainter::Antialiasing );
		if ( m_dragging && m_moved ) {
			// scrub highlight: brighten the well while the value is being dragged
			p.setPen( Qt::NoPen );
			p.setBrush( QColor( 255, 255, 255, 45 ) );
			p.drawRoundedRect( rect().adjusted( 0, 0, -1, -1 ), 3.0, 3.0 );
		}
		// ‹ › step arrows only appear under the pointer, and not while typing
		if ( m_hover && !( lineEdit() && lineEdit()->hasFocus() ) ) {
			p.setPen( QColor( 230, 230, 230 ) );
			p.drawText( QRect( 0, 0, arrowW, height() ), Qt::AlignCenter, QStringLiteral( "‹" ) );
			p.drawText( QRect( width() - arrowW, 0, arrowW, height() ), Qt::AlignCenter, QStringLiteral( "›" ) );
		}
	}
private:
	bool m_dragging = false, m_moved = false, m_hover = false;
	int m_pressX = 0;
	double m_startVal = 0.0;
};

//! Set a redo panel's title, keeping the ˅ / ˃ collapse marker in sync
//! (clone of nifskope_ui.cpp's tlSetPanelTitle)
static void uvSetPanelTitle( QFrame * panel, QToolButton * title, const QString & text )
{
	panel->setProperty( "titleText", text );
	bool col = panel->property( "collapsed" ).toBool();
	title->setText( ( col ? QStringLiteral( "˃  " ) : QStringLiteral( "˅  " ) ) + text );
}

//! Blender: clicking the panel header collapses it to just the title bar
static void uvTogglePanelCollapse( QFrame * panel, QToolButton * title, QWidget * body )
{
	bool col = !panel->property( "collapsed" ).toBool();
	panel->setProperty( "collapsed", col );
	body->setVisible( !col );
	uvSetPanelTitle( panel, title, panel->property( "titleText" ).toString() );
	panel->adjustSize();
	panel->resize( panel->sizeHint() );
}

// Shared edit palette (matches the Block List / 3D edit overlay)
const FloatVector4 uvColActive( 1.0f, 0.616f, 0.0f, 1.0f );      // #FF9D00
const FloatVector4 uvColSelected( 1.0f, 0.447f, 0.0f, 1.0f );    // #FF7200
// Blender: unselected UV edges are a light, clearly readable grey that shows
// over both the dark checker and a bright texture; unselected vertex dots stay
// darker so they read as distinct points.
const FloatVector4 uvColWireUnsel( 0.82f, 0.85f, 0.90f, 0.95f );
const FloatVector4 uvColVertUnsel( 0.12f, 0.12f, 0.14f, 1.0f );
const FloatVector4 uvColDimShape( 0.45f, 0.50f, 0.55f, 0.35f );
const FloatVector4 uvColFaceFill( 1.0f, 0.447f, 0.0f, 0.34f );
// Blender fills every UV face with a faint translucent white so islands read
// as solid shapes rather than bare wireframe; selected faces get the brighter
// orange fill above.
const FloatVector4 uvColIslandFill( 1.0f, 1.0f, 1.0f, 0.10f );
// Object-mode read-only display: the active/primary mesh is white, each
// secondary-selected mesh gets a distinct color so overlapping layouts read apart
const FloatVector4 uvColPrimaryRO( 0.94f, 0.94f, 0.94f, 0.95f );
static FloatVector4 uvSecondaryColor( int i )
{
	static const FloatVector4 palette[6] = {
		FloatVector4( 0.30f, 0.80f, 1.00f, 0.9f ),   // cyan
		FloatVector4( 0.55f, 1.00f, 0.40f, 0.9f ),   // green
		FloatVector4( 1.00f, 0.55f, 0.85f, 0.9f ),   // pink
		FloatVector4( 1.00f, 0.80f, 0.30f, 0.9f ),   // amber
		FloatVector4( 0.70f, 0.55f, 1.00f, 0.9f ),   // violet
		FloatVector4( 1.00f, 0.45f, 0.35f, 0.9f )    // coral
	};
	return palette[( ( i % 6 ) + 6 ) % 6];
}

inline quint64 uvEdgeKey( int a, int b )
{
	if ( a > b ) std::swap( a, b );
	return ( quint64( quint32( a ) ) << 32 ) | quint64( quint32( b ) );
}

//! UV + topology of one shape participating in the edit session
struct UVShapeData
{
	int block = -1;
	QPersistentModelIndex iShape;
	//! BSTriShape "Vertex Data" (rows hold the "UV" field), or the legacy
	//! "UV Sets"/0 Vector2 array when legacyData is true
	QPersistentModelIndex iUVData;
	//! Legacy "UV Sets" array root (multiple coordinate sets); invalid for
	//! BSTriShape, whose FO4 vertex format stores exactly one UV channel
	QPersistentModelIndex iUVSets;
	int uvSetCount = 1;
	bool legacyData = false;
	bool valid = false;
	QVector<Vector2> uvs;
	QVector<Triangle> tris;
	QVector<int> islandOfVert;
	int islandCount = 0;
	//! For each vertex sharing its 3D position with others (a split seam), the
	//! coincident partners. Static per topology; the basis for Blender-style
	//! sticky selection (Shared Location) and for welding islands whose seam
	//! UVs have been merged back together.
	QHash<int, QVector<int>> coPosVerts;

	// ---- per-shape selection state (multi-mesh editing) ----
	// For the ACTIVE shape the editor's members are the live working copy and
	// these are stale until stashActiveSelection(); for every other shape in
	// the edit session these are authoritative.
	QSet<int> selVerts;
	QSet<quint64> selEdges;
	QSet<int> selFaces;
	QSet<int> viewport3DVerts;
	QSet<int> hiddenFaces;
	QSet<int> hiddenVerts;
	QSet<int> pinnedVerts;
};

//! Sticky match tolerance: UVs this close (per axis) count as the same point.
//! Comfortably above half-float UV quantization noise, far below visible.
static constexpr float uvStickyEps = 1.0e-5f;

//! Model index of the UV value for one vertex
static QModelIndex uvValueIndex( NifModel * nif, const UVShapeData & sd, int vertex )
{
	if ( !sd.iUVData.isValid() )
		return QModelIndex();
	if ( sd.legacyData )
		return nif->getIndex( QModelIndex( sd.iUVData ), vertex );
	QModelIndex row = nif->index( vertex, 0, QModelIndex( sd.iUVData ) );
	return nif->getIndex( row, "UV" );
}

//! Lightweight Undo command for UV edits: stores only the changed vertices'
//! old/new UVs and patches them directly (like the live-drag write). Unlike a
//! whole-model snapshot it does NOT reload the model, so Undo/Redo is instant
//! and the editor never blanks; unlike the old per-vertex ChangeValueCommand it
//! re-resolves each vertex's field by number and sets the Vector2 directly, so
//! it can't collapse the layout via stale indices or QVariant round-trips.
class UVEditCommand final : public QUndoCommand
{
public:
	UVEditCommand( NifModel * model, int blockNum, bool legacy, const QPersistentModelIndex & uvData,
		const QVector<int> & vertices, const QVector<Vector2> & before, const QVector<Vector2> & after,
		bool alreadyApplied, const QString & text )
		: nif( model ), block( blockNum ), legacyData( legacy ), iUVData( uvData ),
		verts( vertices ), oldUVs( before ), newUVs( after ), skipFirstRedo( alreadyApplied )
	{
		setText( text );
	}

	void redo() override
	{
		if ( skipFirstRedo ) {           // model already holds the new UVs (live write)
			skipFirstRedo = false;
			return;
		}
		apply( newUVs );
	}
	void undo() override { apply( oldUVs ); }

private:
	void apply( const QVector<Vector2> & uvs )
	{
		if ( !nif || !iUVData.isValid() || uvs.size() != verts.size() )
			return;
		QModelIndex data( iUVData );
		nif->setState( BaseModel::Processing );
		for ( int i = 0; i < verts.size(); i++ ) {
			QModelIndex idx = legacyData
				? nif->getIndex( data, verts.at( i ) )
				: nif->getIndex( nif->index( verts.at( i ), 0, data ), "UV" );
			if ( !idx.isValid() )
				continue;
			if ( legacyData )
				nif->set<Vector2>( idx, uvs.at( i ) );
			else
				nif->set<HalfVector2>( idx, HalfVector2( uvs.at( i ) ) );
		}
		nif->restoreState();
		QModelIndex iShape = nif->getBlockIndex( block );
		if ( iShape.isValid() )
			nif->dataChanged( iShape, iShape );
	}

	NifModel * nif;
	int block;
	bool legacyData;
	QPersistentModelIndex iUVData;
	QVector<int> verts;
	QVector<Vector2> oldUVs, newUVs;
	bool skipFirstRedo;
};

} // namespace

static bool readShapePositions( NifModel * nif, const UVShapeData & sd, QVector<Vector3> & pos );

//! (Re)compute UV islands: connected components over shared vertex indices,
//! additionally welding co-located seam vertices whose UVs coincide (Blender
//! treats those as one chart — a merged seam becomes a single island).
static void uvRebuildIslands( UVShapeData & sd )
{
	const int nv = sd.uvs.size();
	QVector<int> parent( nv );
	for ( int i = 0; i < nv; i++ ) parent[i] = i;
	std::function<int(int)> findRoot = [&]( int a ) {
		while ( parent[a] != a ) { parent[a] = parent[parent[a]]; a = parent[a]; }
		return a;
	};
	for ( const Triangle & t : std::as_const( sd.tris ) ) {
		int r0 = findRoot( t[0] );
		parent[findRoot( t[1] )] = r0;
		parent[findRoot( t[2] )] = findRoot( r0 );
	}
	for ( auto it = sd.coPosVerts.constBegin(); it != sd.coPosVerts.constEnd(); ++it ) {
		for ( int b : it.value() ) {
			if ( b <= it.key() )
				continue;	// each pair once
			const Vector2 d = sd.uvs.at( it.key() ) - sd.uvs.at( b );
			if ( std::fabs( d[0] ) <= uvStickyEps && std::fabs( d[1] ) <= uvStickyEps )
				parent[findRoot( b )] = findRoot( it.key() );
		}
	}
	sd.islandOfVert.resize( nv );
	QHash<int, int> islandIds;
	for ( int i = 0; i < nv; i++ ) {
		int root = findRoot( i );
		auto it = islandIds.constFind( root );
		if ( it == islandIds.constEnd() )
			it = islandIds.insert( root, islandIds.size() );
		sd.islandOfVert[i] = it.value();
	}
	sd.islandCount = islandIds.size();
}

static bool loadShapeUVs( NifModel * nif, int block, UVShapeData & sd, int uvSet = 0 )
{
	sd = UVShapeData();
	sd.block = block;
	QModelIndex iShape = nif->getBlockIndex( block );
	if ( !iShape.isValid() )
		return false;
	sd.iShape = iShape;

	if ( nif->blockInherits( iShape, "BSTriShape" ) ) {
		QModelIndex iData = nif->getIndex( iShape, "Vertex Data" );
		if ( !iData.isValid() )
			return false;
		int numVerts = nif->get<int>( iShape, "Num Vertices" );
		if ( numVerts < 1 || nif->rowCount( iData ) < numVerts )
			return false;
		// meshes without VF_UV have no "UV" field in the vertex rows
		if ( !nif->getIndex( nif->index( 0, 0, iData ), "UV" ).isValid() )
			return false;
		sd.iUVData = iData;
		sd.uvs.reserve( numVerts );
		for ( int i = 0; i < numVerts; i++ )
			sd.uvs << nif->get<Vector2>( nif->index( i, 0, iData ), "UV" );
		sd.tris = nif->getArray<Triangle>( iShape, "Triangles" );
	} else if ( nif->blockInherits( iShape, "NiTriBasedGeom" ) ) {
		QModelIndex iData = nif->getBlockIndex( nif->getLink( iShape, "Data" ) );
		if ( !iData.isValid() )
			return false;
		QModelIndex iUVSets = nif->getIndex( iData, "UV Sets" );
		const int setCount = iUVSets.isValid() ? nif->rowCount( iUVSets ) : 0;
		if ( setCount < 1 )
			return false;
		uvSet = std::clamp( uvSet, 0, setCount - 1 );
		QModelIndex iCoords = nif->getIndex( iUVSets, uvSet );
		if ( !iCoords.isValid() || nif->rowCount( iCoords ) < 1 )
			return false;
		sd.legacyData = true;
		sd.iUVSets = iUVSets;
		sd.uvSetCount = setCount;
		sd.iUVData = iCoords;
		sd.uvs = nif->getArray<Vector2>( iCoords );
		if ( nif->isNiBlock( iData, "NiTriShapeData" ) ) {
			sd.tris = nif->getArray<Triangle>( iData, "Triangles" );
		} else if ( nif->isNiBlock( iData, "NiTriStripsData" ) ) {
			QModelIndex iPoints = nif->getIndex( iData, "Points" );
			for ( int r = 0; iPoints.isValid() && r < nif->rowCount( iPoints ); r++ ) {
				QVector<quint16> strip = nif->getArray<quint16>( nif->getIndex( iPoints, r ) );
				for ( int i = 2; i < strip.size(); i++ ) {
					quint16 a = strip[i - 2], b = strip[i - 1], c = strip[i];
					if ( a == b || b == c || a == c )
						continue;
					sd.tris << ( ( i & 1 ) ? Triangle( b, a, c ) : Triangle( a, b, c ) );
				}
			}
		}
	} else {
		return false;
	}

	if ( sd.uvs.isEmpty() || sd.tris.isEmpty() )
		return false;

	// drop out-of-range triangle indices defensively (matches the legacy editor)
	const int nv = sd.uvs.size();
	for ( Triangle & t : sd.tris )
		for ( int i = 0; i < 3; i++ )
			if ( t[i] >= nv ) t[i] = 0;

	// co-located vertices (split seams): bucket by quantized 3D position, then
	// confirm with exact component equality (same approach as Stitch)
	{
		QVector<Vector3> pos;
		if ( readShapePositions( nif, sd, pos ) && pos.size() >= nv ) {
			auto poskey = []( const Vector3 & p ) -> quint64 {
				auto q = []( float f ) { return quint64( qRound64( double( f ) * 256.0 ) ) & 0x1FFFFFull; };
				return ( q( p[0] ) << 42 ) ^ ( q( p[1] ) << 21 ) ^ q( p[2] );
			};
			QHash<quint64, QVector<int>> buckets;
			for ( int i = 0; i < nv; i++ )
				buckets[poskey( pos.at( i ) )] << i;
			for ( const QVector<int> & grp : std::as_const( buckets ) ) {
				if ( grp.size() < 2 )
					continue;
				for ( int a : grp ) {
					QVector<int> partners;
					const Vector3 & pa = pos.at( a );
					for ( int b : grp ) {
						const Vector3 & pb = pos.at( b );
						if ( b != a && pa[0] == pb[0] && pa[1] == pb[1] && pa[2] == pb[2] )
							partners << b;
					}
					if ( !partners.isEmpty() )
						sd.coPosVerts.insert( a, partners );
				}
			}
		}
	}

	uvRebuildIslands( sd );
	sd.valid = true;
	return true;
}

//! Texture slots offered for the underlay
struct UVTexSlot
{
	QString label;
	QString path;
	//! Shader colour-conversion mode (matches the legacy UV editor / uvedit.frag):
	//! 0 raw, 1 sRGB compress, 2 BC5 UNORM normal, 3 BC5 SNORM normal.
	int colorMode = 0;
};

static QVector<UVTexSlot> findTextureSlots( NifModel * nif, const QModelIndex & iShape )
{
	QVector<UVTexSlot> found;
	if ( !iShape.isValid() )
		return found;
	const int bsver = nif->getBSVersion();
	static const char * fo4SlotNames[10] = {
		"Diffuse", "Normal", "Glow / Skin / Hair", "Greyscale", "Environment",
		"Env Mask", "Wrinkle", "Specular", "Slot 8", "Slot 9"
	};
	QVector<qint32> props = nif->getLinkArray( iShape, "Properties" );
	props << nif->getLink( iShape, "Shader Property" );
	for ( qint32 l : std::as_const( props ) ) {
		QModelIndex iProp = nif->getBlockIndex( l, "BSLightingShaderProperty" );
		if ( !iProp.isValid() )
			iProp = nif->getBlockIndex( l, "BSShaderPPLightingProperty" );
		if ( iProp.isValid() ) {
			QModelIndex iTexSet = nif->getBlockIndex( nif->getLink( iProp, "Texture Set" ) );
			QModelIndex iTextures = iTexSet.isValid() ? nif->getIndex( iTexSet, "Textures" ) : QModelIndex();
			if ( iTextures.isValid() ) {
				const int n = nif->rowCount( iTextures );
				for ( int i = 0; i < n; i++ ) {
					// slot 4 is the environment cubemap (renders as garbage in a
					// 2D view) and slot 5 its mask; neither helps UV work
					if ( i == 4 || i == 5 )
						continue;
					QString path = nif->get<QString>( nif->getIndex( iTextures, i ) );
					if ( path.isEmpty() )
						continue;
					UVTexSlot slot;
					slot.label = QObject::tr( "%1 — %2" )
						.arg( i < 10 ? QString::fromLatin1( fo4SlotNames[i] ) : QString::number( i ),
							QFileInfo( path ).fileName() );
					slot.path = path;
					// Match the legacy editor exactly: FO4 (bsver 130) shows the
					// diffuse raw (mode 0); only SSE (>=151) sRGB-compresses it.
					if ( ( i == 0 || i == 9 ) && bsver >= 151 )
						slot.colorMode = 1;
					else if ( i == 1 && bsver >= 130 )
						slot.colorMode = ( bsver < 151 ? 2 : 3 );
					found << slot;
				}
			}
			continue;
		}
		iProp = nif->getBlockIndex( l, "BSEffectShaderProperty" );
		if ( iProp.isValid() ) {
			for ( const char * field : { "Source Texture", "Greyscale Texture", "Normal Texture" } ) {
				QString path = nif->get<QString>( iProp, field );
				if ( path.isEmpty() )
					continue;
				UVTexSlot slot;
				slot.label = QObject::tr( "%1 — %2" )
					.arg( QString::fromLatin1( field ), QFileInfo( path ).fileName() );
				slot.path = path;
				if ( qstrcmp( field, "Source Texture" ) == 0 )
					slot.colorMode = ( bsver >= 151 ? 1 : 0 );
				else if ( qstrcmp( field, "Normal Texture" ) == 0 && bsver >= 130 )
					slot.colorMode = ( bsver < 151 ? 2 : 3 );
				found << slot;
			}
		}
	}
	return found;
}

// unwrap helpers (defined after the class implementation)
static bool readShapePositions( NifModel * nif, const UVShapeData & sd, QVector<Vector3> & pos );
static bool lscmSolveComponent( const QVector<Vector3> & pos, const QVector<Triangle> & tris,
	QHash<int, Vector2> & outUV, const QHash<int, Vector2> * pins = nullptr );


//! Blender-style 2D UV editor canvas. No Q_OBJECT: outward communication
//! goes through std::function callbacks wired by tlCreateUVManagerDock().
class UVEditorView final : public QOpenGLWidget
{
public:
	explicit UVEditorView( QWidget * parent );
	~UVEditorView() override;

	NifModel * nif = nullptr;
	GLView * ogl = nullptr;

	// ---- settings (persisted by the dock glue) ----
	int selectMode = 1;                 // 1 vertex, 2 edge, 3 face, 4 island
	int pivotMode = 0;                  // 0 bbox center, 1 median, 2 2D cursor
	bool snapOn = false;                // magnet toggle; Ctrl inverts
	int snapTarget = 0;                 // 0 increment, 1 grid, 2 vertex
	int snapBase = 0;                   // 0 closest, 1 center, 2 median, 3 active
	bool snapMove = true, snapRotate = true, snapScale = true;
	float rotIncrement = 5.0f;          // degrees
	float gridStep = 0.0625f;           // 1/16
	bool textureAlpha = true;
	int underlaySlot = -1;              // index into texSlots, -1 = checker
	QString customUnderlay;             // non-empty overrides underlaySlot
	Vector2 cursor2D = Vector2( 0.5f, 0.5f );
	//! Blender's UV Sync Selection. On: selection mirrors the 3D viewport both
	//! ways (per-vertex UVs make this exact). Off: UV selection is local, and
	//! only faces selected in the 3D viewport are shown/pickable here.
	bool syncSelection = true;
	//! Blender's Sticky Selection (Shared Location): picking a UV also grabs
	//! the UVs of co-located mesh vertices at the same UV spot, so merged
	//! seam points select and move as one (vertex/edge modes).
	bool stickySelection = true;
	int uvSetIndex = 0;                 // legacy multi-UV-set meshes only
	//! false = show only the 0-1 tile, dark outside (Blender default);
	//! true = repeat the image and grid across every tile
	bool repeatImage = false;
	//! subtle grid at texture-pixel boundaries (needs the texture resolution)
	bool showPixelGrid = false;
	//! true while displaying object-mode selection read-only (no editing)
	bool objectModeView = false;
	//! clamp UV edits into the 0-1 tile (Blender's Constrain to Image Bounds)
	bool constrainBounds = false;

	// ---- callbacks to the dock ----
	std::function<void( const QString & )> statusCb;
	std::function<void()> selectionInfoCb;
	std::function<void()> cursorMovedCb;
	std::function<void( int )> modeChangedCb;
	//! show (kind, params) / hide (kind 0) the adjust-last-operation panel
	std::function<void( int, const QVector<float> & )> operatorPanelCb;
	//! canvas resized (the dock glue repositions its overlay panel)
	std::function<void()> resizedCb;

	// ---- data / sync ----
	bool applyingEdit = false;          //!< suppress our own dataChanged echoes
	//! a selection/mode change arrived while the editor was hidden; the
	//! rebuild is deferred to the next showEvent (rebuilding a big mesh
	//! costs real time, so a hidden editor must not chase every click)
	bool viewportRebuildPending = false;
	//! the full rebuild wiring (rebuildFromViewport + dock combos), set by
	//! tlCreateUVManagerDock so the deferred rebuild refreshes the combos too
	std::function<void()> deferredRebuildCb;
	void rebuildFromViewport();
	void clearData();
	void reloadShapeUVs( int block );
	bool shapeRowMatches( const QModelIndex & idx ) const;
	void syncSelectionFromViewport();
	QVector<UVTexSlot> texSlots;
	void refreshTexSlots();

	// ---- selection ----
	void selectAllUV( int action );     // 0 toggle, 1 all, 2 none
	void invertSelection();
	void setSelectMode( int mode, bool fromViewport );
	void selectLinkedUnderCursor( bool add );
	void growSelectionToLinked();

	// ---- cursor / view ----
	void frameAll();
	void frameSelected();
	void placeCursor( const QPointF & widgetPos );
	void setCursorUV( const Vector2 & uv );
	void showSnapMenu( const QPoint & globalPos );
	void snapSelectedToCursor( bool keepOffset );
	void snapSelectedToGrid();
	void snapCursor( int mode );        // 0 selected, 1 active, 2 grid, 3 origin, 4 tile center

	// ---- unwrap / project ----
	void showUnwrapMenu( const QPoint & globalPos );
	//! angle-based (LSCM) on the selected faces; margin = island border/spacing
	void unwrapSelection( float margin = 0.02f );
	void projectFromView();
	void projectShape( int kind );      // 0 cube, 1 cylinder, 2 sphere

	// ---- Phase 2 operators ----
	void showMergeMenu( const QPoint & globalPos );
	//! mode: 0 at center, 1 at cursor, 2 by distance (dist < 0 = default threshold)
	void mergeSelection( int mode, float dist = -1.0f );
	void showTransformMenu( const QPoint & globalPos );  // mirror / align
	void mirrorSelection( int axis );   // 1 = mirror U (flip X), 2 = mirror V (flip Y)
	void alignSelection( int mode );    // 0 align U, 1 align V, 2 straighten X, 3 straighten Y
	void roundSelectionToPixels();
	void hideSelectedFaces();
	void unhideAllFaces();

	// ---- Phase 3 layout tools ----
	//! target islands = islands touched by the selection, or all if none selected
	QSet<int> targetIslands() const;
	void packIslands( float margin = 0.01f );	// repack selected/all islands into the 0-1 tile
	void averageIslandsScale();         // equalise per-island texel density
	void smartProject( float angleDeg );// normal-cluster charts -> LSCM -> pack
	void minimizeStretch( int iters );  // Laplacian relax of the selected UVs
	void selectOverlappingUVs();        // select faces whose UV triangles overlap
	void stitchSelection();             // weld selected UVs across seams by 3D position
	void copySelectedUVs();
	void pasteCopiedUVs();
	void showLayoutMenu( const QPoint & globalPos );

	// ---- Phase 4 topology / unwrap ----
	void pinSelected( bool pin );       // P pin / Alt+P unpin selected verts
	void invertPins();
	void unwrapWithPins();              // LSCM over selection honouring pinnedVerts
	void ripSelectedFaces();            // duplicate boundary verts -> free island (Rip/Split)
	void exportUVLayout();              // render the UV wireframe to a PNG

	// ---- adjust last operation (Blender's operator redo panel) ----
	//! Undo the armed operator and re-run it with new parameters. Returns
	//! false when the gesture went stale (something else touched the undo
	//! stack); the dock then freezes the panel's inputs, like the 3D panels.
	bool reapplyUVOperator( const QVector<float> & params );
	void cancelOperatorPanel();         // disarm + hide (no-op mid-reapply)

	// ---- sync / UV set control (dock buttons) ----
	void setSyncSelection( bool on );
	void setUVSet( int set );
	void snapSelectedToPixels();
	void snapCursorToPixels();
	//! Snap a UV to the nearest texture-pixel corner (identity if no resolution)
	Vector2 snapToPixel( const Vector2 & uv ) const;

	// ---- info for the dock ----
	int activeBlock() const { return activeShape >= 0 ? shapes.at( activeShape ).block : -1; }
	int selectedCount() const { return selVerts.size(); }
	bool hasData() const { return activeShape >= 0; }
	int activeUVSetCount() const { return activeShape >= 0 ? shapes.at( activeShape ).uvSetCount : 1; }

protected:
	void initializeGL() override;
	void resizeGL( int w, int h ) override;
	void paintGL() override;
	void mousePressEvent( QMouseEvent * e ) override;
	void mouseReleaseEvent( QMouseEvent * e ) override;
	void mouseMoveEvent( QMouseEvent * e ) override;
	void wheelEvent( QWheelEvent * e ) override;
	void keyPressEvent( QKeyEvent * e ) override;
	void contextMenuEvent( QContextMenuEvent * e ) override;
	//! Focus-follows-mouse (Blender): entering the canvas grabs keyboard focus
	//! so G/R/S/A and the other single-key shortcuts fire without a prior click.
	void enterEvent( QEnterEvent * e ) override;
	//! service a rebuild that was deferred while the editor was hidden
	void showEvent( QShowEvent * e ) override
	{
		QOpenGLWidget::showEvent( e );
		if ( viewportRebuildPending ) {
			viewportRebuildPending = false;
			if ( deferredRebuildCb )
				deferredRebuildCb();
			else
				rebuildFromViewport();
		}
	}

private:
	NifSkopeOpenGLContext * cx = nullptr;
	TexCache * textures = nullptr;
	int pixelWidth = 640, pixelHeight = 480;
	double viewPos[2] = { 0.0, 0.0 };   // UV-space center offset from (0.5, 0.5)
	double zoom = 1.2;
	FloatVector4 viewScaleAndOffset = FloatVector4( 1.0f, 1.0f, 0.0f, 0.0f );
	//! resolution (texels) of the last bound underlay, 0 = none/checker
	Vector2 textureRes = Vector2( 0.0f, 0.0f );

	QVector<UVShapeData> shapes;
	int activeShape = -1;               // index into shapes

	// active-shape selection; vertices are the ground truth, edge/face sets
	// carry the mode-specific membership
	QSet<int> selVerts;
	QSet<quint64> selEdges;
	QSet<int> selFaces;
	int activeVert = -1;
	int activeFace = -1;
	//! Mirror of the 3D viewport's selected vertices for the active shape;
	//! with sync off this is the Blender-style visibility filter
	QSet<int> viewport3DVerts;
	//! UV-local hidden faces (Blender H / Alt+H); reset on rebuild
	QSet<int> hiddenFaces;
	//! verts whose every incident face is hidden (derived from hiddenFaces)
	QSet<int> hiddenVerts;
	//! Copy/Paste UVs buffer, keyed by vertex index (identical-topology meshes)
	QHash<int, Vector2> copiedUVs;
	//! LSCM pins: vertices whose UVs are held fixed by Unwrap (Phase 4, P/Alt+P)
	QSet<int> pinnedVerts;
	//! distortion (stretch) heatmap overlay toggle
	bool showStretch = false;

	// adjust-last-operation state (see reapplyUVOperator). Kinds: 0 none,
	// 1 merge by distance, 2 minimize stretch, 3 pack islands, 4 smart project,
	// 5 unwrap, 6 move, 7 rotate, 8 scale
	int lastOpKind = 0;
	QVector<float> lastOpParams;
	QSet<int> lastOpSeedVerts;          //!< selection to restore before a re-run
	int lastOpUndoIndex = -1;           //!< undo-stack index right after the op (stale guard)
	bool lastOpPushed = true;           //!< the last (re)run actually pushed an undo command
	bool opReapplying = false;          //!< suppress re-arming / cancelling while re-running
	void armOperatorPanel( int kind, const QVector<float> & params );

	// modal transform
	int xformMode = 0;                  // 0 none, 1 move, 2 rotate, 3 scale
	int xformAxis = 0;                  // 0 free, 1 U, 2 V
	struct XVert { int shape; int idx; Vector2 orig; Vector2 current; };
	QVector<XVert> xverts;
	Vector2 xformPivot;
	// last-applied gesture parameters (for the adjust-last-operation panel)
	Vector2 xformLastDelta;
	float xformLastAngle = 0.0f;        // radians
	Vector2 xformLastScale = Vector2( 1.0f, 1.0f );
	// the committed gesture's vertices/pivot, kept for panel re-runs
	QVector<XVert> lastOpXVerts;
	Vector2 lastOpPivot;
	QPointF xformStartPx;               // device px
	QPointF xformVirtualPx;             // precision-scaled accumulated position
	QPointF xformLastPx;
	QString numericBuf;
	bool snapIndicatorOn = false;
	Vector2 snapIndicatorUV;

	// box select
	bool boxDrag = false;
	QPointF boxStartPx, boxCurPx;       // device px
	bool boxPending = false;
	Qt::KeyboardModifiers boxMods;

	// pan
	bool panning = false;
	QPointF panLastPx;

	// GL scratch buffers
	QVector<Vector3> posBuf;
	QVector<FloatVector4> colorBuf;

	UVShapeData * active() { return activeShape >= 0 ? &shapes[activeShape] : nullptr; }
	const UVShapeData * active() const { return activeShape >= 0 ? &shapes.at( activeShape ) : nullptr; }

	void updateViewRect();
	void updateViewMapping();
	Vector2 mapToUV( const QPointF & devicePx ) const;
	QPointF mapFromUV( const Vector2 & uv ) const;
	QPointF devicePos( const QMouseEvent * e ) const;

	void setStatus( const QString & s ) { if ( statusCb ) statusCb( s ); }
	void notifySelectionInfo() { if ( selectionInfoCb ) selectionInfoCb(); }

	// selection internals
	//! Sync-off visibility (Blender: only 3D-selected faces are shown), also
	//! honouring UV-local face hiding (H / Alt+H).
	bool vertVisibleUV( int v ) const
	{
		if ( hiddenVerts.contains( v ) )
			return false;
		return syncSelection || viewport3DVerts.contains( v );
	}
	bool faceVisibleUV( int f, const Triangle & t ) const
	{
		if ( hiddenFaces.contains( f ) )
			return false;
		return syncSelection || ( viewport3DVerts.contains( t[0] )
			&& viewport3DVerts.contains( t[1] ) && viewport3DVerts.contains( t[2] ) );
	}
	//! Recompute hiddenVerts from hiddenFaces (a vert is hidden only when every
	//! face using it is hidden).
	void recomputeHiddenVerts();
	// ---- multi-mesh selection plumbing ----
	//! Copy the editor's live selection members into shapes[activeShape]
	//! (call before switching the active shape or reading all shapes uniformly)
	void stashActiveSelection();
	//! Make shape s active and load its stored selection into the live members
	void adoptActiveSelection( int s );
	//! Per-shape visibility (the members-vs-stash split makes this explicit):
	//! authoritative sets for shape s — the live members when s is active
	bool vertVisibleUVIn( int s, int v ) const
	{
		if ( s == activeShape )
			return vertVisibleUV( v );
		const UVShapeData & sd = shapes.at( s );
		if ( sd.hiddenVerts.contains( v ) )
			return false;
		return syncSelection || sd.viewport3DVerts.contains( v );
	}
	bool faceVisibleUVIn( int s, int f, const Triangle & t ) const
	{
		if ( s == activeShape )
			return faceVisibleUV( f, t );
		const UVShapeData & sd = shapes.at( s );
		if ( sd.hiddenFaces.contains( f ) )
			return false;
		return syncSelection || ( sd.viewport3DVerts.contains( t[0] )
			&& sd.viewport3DVerts.contains( t[1] ) && sd.viewport3DVerts.contains( t[2] ) );
	}
	//! The edit-session shape whose element is closest to the cursor (vertex,
	//! else edge, else face body, per the current select mode); -1 = none
	int pickShapeAt( const QPointF & devicePx, double radiusPx ) const;
	//! Sticky selection (Shared Location): grow the set with co-located mesh
	//! verts sitting at the same UV spot, transitively. No-op with sticky off.
	void expandStickyIn( int s, QSet<int> & verts ) const
	{
		if ( s < 0 || s >= shapes.size() || !stickySelection )
			return;
		const UVShapeData & sd = shapes.at( s );
		if ( sd.coPosVerts.isEmpty() )
			return;
		QVector<int> queue = verts.values();
		while ( !queue.isEmpty() ) {
			const int v = queue.takeLast();
			auto it = sd.coPosVerts.constFind( v );
			if ( it == sd.coPosVerts.constEnd() )
				continue;
			for ( int w : it.value() ) {
				if ( verts.contains( w ) )
					continue;
				const Vector2 d = sd.uvs.at( v ) - sd.uvs.at( w );
				if ( std::fabs( d[0] ) <= uvStickyEps && std::fabs( d[1] ) <= uvStickyEps ) {
					verts << w;
					queue << w;
				}
			}
		}
	}
	void expandSticky( QSet<int> & verts ) const { expandStickyIn( activeShape, verts ); }
	void rebuildDerivedSelection();
	//! Push one shape's authoritative selection (live members when active) to
	//! the 3D viewport; pushSelectionToViewport() = active shape + notify
	void pushShapeSelectionToViewport( int s );
	void pickAt( const QPointF & devicePx, bool extend );
	void applyBoxSelect( const QRectF & deviceRect, Qt::KeyboardModifiers mods );
	int nearestVertex( const QPointF & devicePx, double radiusPx, const QSet<int> * exclude = nullptr ) const;
	int faceUnder( const Vector2 & uv ) const;
	bool edgeUnder( const QPointF & devicePx, double radiusPx, int & outA, int & outB ) const;
	void pushSelectionToViewport();

	// transforms
	void beginTransform( int mode );
	void updateTransform( const QPointF & devicePx, Qt::KeyboardModifiers mods );
	void endTransform( bool commit );
	Vector2 transformBasePoint() const;
	void writeLiveUVs();
	//! returns true when a command was actually pushed (something changed)
	bool commitTransformUndo( const QString & name );
	//! returns true when a command was actually pushed (something changed)
	bool applyUVEditUndoable( const QHash<int, Vector2> & newUVs, const QString & name )
	{
		return applyUVEditUndoableShape( activeShape, newUVs, name );
	}
	//! per-shape form used by multi-mesh re-applies
	bool applyUVEditUndoableShape( int s, const QHash<int, Vector2> & newUVs, const QString & name );

	void drawShapeLayers();
	void bindUnderlayTexture( int & colorModeOut );
};


UVEditorView::UVEditorView( QWidget * parent )
	: QOpenGLWidget( parent )
{
	QSettings settings;
	int aa = settings.value( "Settings/Render/General/Msaa Samples", 2 ).toInt();
	aa = std::min< int >( std::max< int >( aa, 0 ), 4 );

	QSurfaceFormat fmt = format();
	fmt.setRenderableType( QSurfaceFormat::OpenGL );
	fmt.setMajorVersion( 4 );
#ifdef Q_OS_MACOS
	fmt.setMinorVersion( 1 );
#else
	fmt.setMinorVersion( 2 );
#endif
	fmt.setProfile( QSurfaceFormat::CoreProfile );
	fmt.setOption( QSurfaceFormat::DeprecatedFunctions, false );
	fmt.setSwapInterval( 1 );
	fmt.setSwapBehavior( QSurfaceFormat::DoubleBuffer );
	fmt.setDepthBufferSize( 24 );
	fmt.setStencilBufferSize( 8 );
	fmt.setSamples( 1 << aa );
	setFormat( fmt );
	setTextureFormat( GL_SRGB8 );

	setFocusPolicy( Qt::StrongFocus );
	setMouseTracking( true );
	setCursor( QCursor( Qt::CrossCursor ) );
	setMinimumSize( 260, 260 );

	textures = new TexCache( this );
}

UVEditorView::~UVEditorView()
{
	QOpenGLContext * prvContext = QOpenGLContext::currentContext();
	if ( context() != prvContext )
		makeCurrent();
	delete textures;
	delete cx;
	if ( !prvContext )
		doneCurrent();
	else if ( prvContext != context() )
		prvContext->makeCurrent( prvContext->surface() );
}

void UVEditorView::initializeGL()
{
	cx = new NifSkopeOpenGLContext( context() );
	textures->setOpenGLContext( cx );
	cx->updateShaders();

	glEnable( GL_MULTISAMPLE );
	glEnable( GL_BLEND );
	cx->fn->glBlendFuncSeparate( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA );
	glDepthFunc( GL_LEQUAL );
	glDisable( GL_DEPTH_TEST );
	glDisable( GL_CULL_FACE );
	glDisable( GL_FRAMEBUFFER_SRGB );
	glPolygonMode( GL_FRONT_AND_BACK, GL_FILL );
}

void UVEditorView::resizeGL( int w, int h )
{
	double p = devicePixelRatioF();
	pixelWidth = int( p * w + 0.5 );
	pixelHeight = int( p * h + 0.5 );
	updateViewMapping();
	if ( resizedCb )
		resizedCb();
}

//! Recompute viewScaleAndOffset only (no GL calls; safe outside paintGL)
void UVEditorView::updateViewMapping()
{
	double scaleX, scaleY;
	if ( pixelWidth < pixelHeight ) {
		scaleX = zoom;
		scaleY = zoom * double( pixelHeight ) / double( std::max( pixelWidth, 1 ) );
	} else {
		scaleX = zoom * double( pixelWidth ) / double( std::max( pixelHeight, 1 ) );
		scaleY = zoom;
	}
	double offsX = viewPos[0] + 0.5 - scaleX * 0.5;
	double offsY = viewPos[1] + 0.5 - scaleY * 0.5;
	viewScaleAndOffset = FloatVector4( float( scaleX ), float( scaleY ), float( offsX ), float( offsY ) );
}

void UVEditorView::updateViewRect()
{
	updateViewMapping();

	cx->setProjectionMatrix( Matrix4() );
	cx->setGlobalUniforms();
	cx->setDefaultVertexAttribs( Scene::defaultAttrMask, Scene::defaultVertexAttrs );

	Matrix4 modelViewMatrix;
	double invScaleX = 2.0 / double( viewScaleAndOffset[0] );
	double invScaleY = 2.0 / double( viewScaleAndOffset[1] );
	modelViewMatrix( 0, 0 ) = float( invScaleX );
	modelViewMatrix( 1, 1 ) = float( -invScaleY );
	modelViewMatrix( 2, 2 ) = -0.25f;
	modelViewMatrix( 3, 0 ) = float( ( viewPos[0] + 0.5 ) * -invScaleX );
	modelViewMatrix( 3, 1 ) = float( ( viewPos[1] + 0.5 ) * invScaleY );
	modelViewMatrix( 3, 2 ) = 0.375f;

	if ( auto prog = cx->useProgram( "lines.prog" ); prog )
		prog->uni4m( "modelViewMatrix", modelViewMatrix );
	if ( auto prog = cx->useProgram( "selection.prog" ); prog )
		prog->uni4m( "modelViewMatrix", modelViewMatrix );
	if ( auto prog = cx->useProgram( "wireframe.prog" ); prog ) {
		prog->uni3m( "normalMatrix", Matrix() );
		prog->uni4m( "modelViewMatrix", modelViewMatrix );
	}
}

Vector2 UVEditorView::mapToUV( const QPointF & devicePx ) const
{
	double x = devicePx.x() / double( std::max( pixelWidth, 1 ) );
	double y = devicePx.y() / double( std::max( pixelHeight, 1 ) );
	x = x * double( viewScaleAndOffset[0] ) + double( viewScaleAndOffset[2] );
	y = y * double( viewScaleAndOffset[1] ) + double( viewScaleAndOffset[3] );
	return Vector2( float( x ), float( y ) );
}

QPointF UVEditorView::mapFromUV( const Vector2 & uv ) const
{
	double x = ( double( uv[0] ) - double( viewScaleAndOffset[2] ) ) / double( viewScaleAndOffset[0] );
	double y = ( double( uv[1] ) - double( viewScaleAndOffset[3] ) ) / double( viewScaleAndOffset[1] );
	return QPointF( x * pixelWidth, y * pixelHeight );
}

QPointF UVEditorView::devicePos( const QMouseEvent * e ) const
{
	return e->position() * devicePixelRatioF();
}

void UVEditorView::bindUnderlayTexture( int & colorModeOut )
{
	colorModeOut = -1;
	textureRes = Vector2( 0.0f, 0.0f );
	textures->activateTextureUnit( 0 );
	const GLenum wrap = repeatImage ? GL_REPEAT : GL_CLAMP_TO_EDGE;
	QString path = customUnderlay;
	int colorMode = 0;   // custom images: display raw (matches FO4 diffuse)
	if ( path.isEmpty() && underlaySlot >= 0 && underlaySlot < texSlots.size() ) {
		path = texSlots.at( underlaySlot ).path;
		colorMode = texSlots.at( underlaySlot ).colorMode;
	}
	if ( !path.isEmpty() && nif && textures->bind( path, nif ) ) {
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrap );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrap );
		// real resolution for the pixel grid and pixel snapping
		GLint tw = 0, th = 0;
		cx->fn->glGetTexLevelParameteriv( GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &tw );
		cx->fn->glGetTexLevelParameteriv( GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &th );
		textureRes = Vector2( float( tw ), float( th ) );
		colorModeOut = colorMode & 3;
		return;
	}
	// Blender look: the untextured 0-1 tile is a mid gray, not white, so the
	// whitish grid lines carry the contrast (same idea as the 3D viewport)
	static const QString defaultTexture = "#FF393939";
	if ( nif )
		textures->bind( defaultTexture, nif );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrap );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrap );
	colorModeOut = 0;
}

void UVEditorView::paintGL()
{
	if ( !cx )
		return;
	cx->setCacheSize( 16777216 );
	cx->setViewport( 0, 0, pixelWidth, pixelHeight );

	// Blender's UV editor background (#2B2B2B, neutral dark gray)
	FloatVector4 bgColor( 0.168f, 0.168f, 0.168f, 1.0f );

	glDepthMask( GL_TRUE );
	auto prog = cx->useProgram( "uvedit.prog" );
	if ( !( textures && prog && nif ) ) {
		glClearColor( bgColor[0], bgColor[1], bgColor[2], bgColor[3] );
		glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
		return;
	}

	updateViewRect();
	glClear( GL_DEPTH_BUFFER_BIT );

	static const float positions[8] = { 0.0f, 0.0f,  0.0f, 1.0f,  1.0f, 0.0f,  1.0f, 1.0f };
	static const float * attrData[8] = { nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, positions };
	cx->bindShape( 4, 0x20000000, 0, attrData, nullptr );

	int textureColorMode = -1;
	bindUnderlayTexture( textureColorMode );
	// A real image underlay is present when the bound texture reported a size;
	// the checker fallback (#FF393939) leaves textureRes at 0.
	const bool hasImageUnderlay = textureRes[0] > 0.5f && textureRes[1] > 0.5f;

	prog = cx->useProgram( "uvedit.prog" );
	prog->uni4f( "viewScaleAndOffset", viewScaleAndOffset );
	prog->uni1i( "BaseMap", 0 );
	prog->uni1i( "textureColorMode", textureColorMode | ( textureAlpha ? 0 : 4 ) );
	prog->uni2f( "uvCenter", 0.0f, 0.0f );
	prog->uni4f( "uvScaleAndOffset", FloatVector4( 1.0f, 1.0f, 0.0f, 0.0f ) );
	prog->uni1f( "uvRotation", 0.0f );
	prog->uni2f( "pixelScale",
		float( pixelWidth ) / viewScaleAndOffset[0], float( pixelHeight ) / viewScaleAndOffset[1] );
	// Blender look: uniform, subtle, slightly cool-grey lines. The grid is
	// zoom-adaptive (Blender-style): the coarsest level's spacing stays in a
	// readable screen range at ANY zoom (coarsening beyond one line per tile
	// when far out), and the ×8 finer level crossfades in as you zoom in.
	FloatVector4 gridColors[3] = {
		FloatVector4( 0.90f, 0.92f, 0.95f, 0.17f ),
		FloatVector4( 0.90f, 0.92f, 0.95f, 0.13f ),
		FloatVector4( 0.90f, 0.92f, 0.95f, 0.09f )
	};
	float gridLineWidths[3] = {
		GLView::Settings::lineWidthGrid,
		GLView::Settings::lineWidthGrid * ( 6.0f / 7.0f ),
		GLView::Settings::lineWidthGrid * ( 4.0f / 7.0f )
	};
	const float sPx = std::min( float( pixelWidth ) / viewScaleAndOffset[0],
		float( pixelHeight ) / viewScaleAndOffset[1] );
	const double octave = std::log( std::max( double( sPx ), 1.0e-3 ) / 24.0 ) / std::log( 8.0 );
	const double octaveFloor = std::floor( octave );
	const float gridBaseDiv = float( std::pow( 8.0, octaveFloor ) );
	const float octaveFrac = float( octave - octaveFloor );
	gridColors[1][3] *= octaveFrac;
	for ( int i = 0; i < 3; i++ ) {
		if ( gridLineWidths[i] < 1.0f ) {
			gridColors[i][3] *= gridLineWidths[i];
			gridLineWidths[i] = 1.0f;
		}
	}
	prog->uni4fv( "gridColors", gridColors, 3 );
	prog->uni3f( "gridLineWidths", gridLineWidths[0], gridLineWidths[1], gridLineWidths[2] );
	prog->uni1f( "gridBaseDiv", gridBaseDiv );
	// Hide the subdivision grid entirely when an image is loaded (Blender shows
	// the image, not the grid). The pixel grid (below) is independent.
	bool gridEnabled[3] = { !hasImageUnderlay, ( !hasImageUnderlay && octaveFrac > 0.004f ),
		false };
	prog->uni1bv( "gridEnabled", gridEnabled, 3 );
	prog->uni4f( "backgroundColor", bgColor );
	// Show a loaded image close to full brightness; keep the checker dimmer so
	// the grid lines read against it.
	if ( hasImageUnderlay )
		prog->uni2f( "textureColorScale", 0.96f, 0.6f );
	else
		prog->uni2f( "textureColorScale", 0.75f, 0.5f );
	prog->uni1i( "tileMode", repeatImage ? 0 : 1 );
	const bool pixelGridActive = showPixelGrid && textureRes[0] > 0.5f && textureRes[1] > 0.5f;
	prog->uni4f( "pixelGrid", FloatVector4( pixelGridActive ? textureRes[0] : 0.0f,
		pixelGridActive ? textureRes[1] : 0.0f, 0.5f, 0.0f ) );

	glDisable( GL_DEPTH_TEST );
	glDepthMask( GL_FALSE );
	glDisable( GL_CULL_FACE );
	glDisable( GL_BLEND );
	glDisable( GL_FRAMEBUFFER_SRGB );

	cx->fn->glDrawArrays( GL_TRIANGLE_STRIP, 0, 4 );

	drawShapeLayers();

	cx->shrinkCache();

	// Blender-style 2D cursor: red/white dashed ring + crosshair ticks, drawn
	// with QPainter over the GL content (constant screen size, crisp).
	QPointF sp = mapFromUV( cursor2D ) / devicePixelRatioF();
	if ( sp.x() > -40 && sp.y() > -40 && sp.x() < width() + 40 && sp.y() < height() + 40 ) {
		QPainter painter( this );
		painter.setRenderHint( QPainter::Antialiasing, true );
		const qreal r = 7.0;
		painter.setBrush( Qt::NoBrush );
		painter.setPen( QPen( QColor( 214, 56, 56 ), 1.6 ) );
		painter.drawEllipse( sp, r, r );
		QPen dashPen( QColor( 255, 255, 255 ), 1.6 );
		dashPen.setDashPattern( { 2.2, 2.2 } );
		painter.setPen( dashPen );
		painter.drawEllipse( sp, r, r );
		painter.setPen( QPen( QColor( 12, 12, 12, 235 ), 1.3 ) );
		const qreal t0 = r + 1.5, t1 = r + 6.0;
		painter.drawLine( sp + QPointF( t0, 0 ), sp + QPointF( t1, 0 ) );
		painter.drawLine( sp - QPointF( t0, 0 ), sp - QPointF( t1, 0 ) );
		painter.drawLine( sp + QPointF( 0, t0 ), sp + QPointF( 0, t1 ) );
		painter.drawLine( sp - QPointF( 0, t0 ), sp - QPointF( 0, t1 ) );
		painter.end();
	}
}

void UVEditorView::drawShapeLayers()
{
	glEnable( GL_BLEND );
	cx->fn->glBlendFuncSeparate( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA );
	glDisable( GL_DEPTH_TEST );
	glDepthMask( GL_FALSE );

	// Blender-style soft white outline of the 0-1 tile, drawn under the UV
	// wireframes so the working space reads even when an image fills it
	// edge-to-edge or the view is panned into empty space.
	{
		static const Vector3 tileRect[4] = {
			Vector3( 0.0f, 0.0f, 0.0f ), Vector3( 1.0f, 0.0f, 0.0f ),
			Vector3( 1.0f, 1.0f, 0.0f ), Vector3( 0.0f, 1.0f, 0.0f )
		};
		const float * p = &( tileRect[0][0] );
		cx->bindShape( 4, 0x03, 0, &p, nullptr );
		if ( auto prog = cx->useProgram( "lines.prog" ); prog ) {
			prog->uni4f( "vertexColorOverride", FloatVector4( 0.90f, 0.92f, 0.95f, 0.50f ) );
			prog->uni1i( "selectionParam", -1 );
			prog->uni1f( "lineWidth", std::max( GLView::Settings::lineWidthGrid, 1.0f ) );
			cx->fn->glDrawArrays( GL_LINE_LOOP, 0, 4 );
		}
	}

	// --- Object Mode: read-only wireframes. Primary (active) mesh white,
	// each secondary a distinct color; no selection, points, or fills. ---
	if ( objectModeView ) {
		int secondaryIndex = 0;
		for ( int s = 0; s < shapes.size(); s++ ) {
			if ( !shapes.at( s ).valid )
				continue;
			const UVShapeData & sd = shapes.at( s );
			const FloatVector4 color = ( s == activeShape )
				? uvColPrimaryRO : uvSecondaryColor( secondaryIndex++ );
			const qsizetype nv = sd.uvs.size();
			posBuf.resize( nv );
			colorBuf.resize( nv );
			for ( qsizetype i = 0; i < nv; i++ ) {
				posBuf[i] = Vector3( sd.uvs.at( i )[0], sd.uvs.at( i )[1], 0.0f );
				colorBuf[i] = color;
			}
			const float * attrData[2] = { &( posBuf[0][0] ), &( colorBuf[0][0] ) };
			// faint fill so islands read as solid shapes (brighter for the primary)
			cx->bindShape( (unsigned int) nv, 0x43, size_t( sd.tris.size() ) * 6, attrData, sd.tris.constData() );
			if ( auto prog = cx->useProgram( "selection.prog" ); prog ) {
				FloatVector4 fill = ( s == activeShape ) ? uvColIslandFill
					: FloatVector4( color[0], color[1], color[2], 0.05f );
				prog->uni4f( "vertexColorOverride", FloatVector4( 1.0e-15f ).maxValues( fill ) );
				prog->uni1i( "selectionFlags", 0 );
				prog->uni1i( "selectionParam", -1 );
				cx->fn->glDrawElements( GL_TRIANGLES, GLsizei( sd.tris.size() * 3 ), GL_UNSIGNED_SHORT, (void *) 0 );
			}
			if ( auto prog = cx->useProgram( "wireframe.prog" ); prog ) {
				prog->uni4f( "vertexColorOverride", FloatVector4( 0.0f ) );
				prog->uni1i( "selectionParam", -1 );
				prog->uni1f( "lineWidth", GLView::Settings::lineWidthWireframe
					* ( s == activeShape ? 0.625f : 0.5f ) );
				cx->fn->glDrawElements( GL_TRIANGLES, GLsizei( sd.tris.size() * 3 ), GL_UNSIGNED_SHORT, (void *) 0 );
			}
		}
		glDisable( GL_BLEND );
		return;
	}

	// --- edit mode: every shape in the session renders its full selection
	// state (multi-mesh editing); the active shape additionally shows the
	// active vertex, pins, and the stretch overlay ---
	for ( int s = 0; s < shapes.size(); s++ ) {
		const UVShapeData & osd = shapes.at( s );
		if ( !osd.valid )
			continue;
		const bool isActive = ( s == activeShape );
		const QSet<int> & sVerts = isActive ? selVerts : osd.selVerts;
		const QSet<quint64> & sEdges = isActive ? selEdges : osd.selEdges;
		const QSet<int> & sPins = isActive ? pinnedVerts : osd.pinnedVerts;
		const QSet<int> & sVp3d = isActive ? viewport3DVerts : osd.viewport3DVerts;
		const QSet<int> & sHiddenFaces = isActive ? hiddenFaces : osd.hiddenFaces;
		const int actV = isActive ? activeVert : -1;
		const UVShapeData * sd = &osd;
		const qsizetype nv = sd->uvs.size();
		posBuf.resize( nv );
		colorBuf.resize( nv );
		const bool islandMode = ( selectMode == 4 );
		for ( qsizetype i = 0; i < nv; i++ ) {
			posBuf[i] = Vector3( sd->uvs.at( i )[0], sd->uvs.at( i )[1], 0.0f );
			if ( int( i ) == actV && selectMode == 1 )
				colorBuf[i] = uvColActive;
			else if ( sVerts.contains( int( i ) ) )
				colorBuf[i] = islandMode ? uvColActive : uvColSelected;
			else
				colorBuf[i] = uvColWireUnsel;
		}
		const float * attrData[2] = { &( posBuf[0][0] ), &( colorBuf[0][0] ) };

		// with sync off, only faces selected in the 3D viewport are shown; and
		// UV-hidden faces (H) are excluded in every mode
		const QVector<Triangle> * wireTris = &sd->tris;
		QVector<Triangle> visibleTris;
		if ( !syncSelection || !sHiddenFaces.isEmpty() ) {
			visibleTris.reserve( sd->tris.size() );
			for ( int f = 0; f < sd->tris.size(); f++ )
				if ( faceVisibleUVIn( s, f, sd->tris.at( f ) ) )
					visibleTris << sd->tris.at( f );
			wireTris = &visibleTris;
		}

		// distortion heatmap overlay (Show Stretch): fill each visible face by its
		// UV-area / 3D-area density relative to the mesh mean — blue = compressed,
		// green = even, red = stretched. Replaces the flat island fill while on.
		// (active shape only — it needs a per-frame vertex position read)
		if ( showStretch && isActive && !wireTris->isEmpty() ) {
			QVector<Vector3> hpos;
			if ( readShapePositions( nif, *sd, hpos ) && hpos.size() >= nv ) {
				QVector<float> dens( wireTris->size(), -1.0f );
				double refNum = 0.0, refDen = 0.0;
				for ( int i = 0; i < wireTris->size(); i++ ) {
					const Triangle & t = wireTris->at( i );
					Vector3 n3 = Vector3::crossproduct( hpos.at( t[1] ) - hpos.at( t[0] ),
						hpos.at( t[2] ) - hpos.at( t[0] ) );
					const double a3 = 0.5 * std::sqrt( double( Vector3::dotproduct( n3, n3 ) ) );
					const Vector2 & a = sd->uvs.at( t[0] );
					const Vector2 & b = sd->uvs.at( t[1] );
					const Vector2 & c = sd->uvs.at( t[2] );
					const double au = 0.5 * std::fabs( double( ( b[0] - a[0] ) * ( c[1] - a[1] )
						- ( c[0] - a[0] ) * ( b[1] - a[1] ) ) );
					if ( a3 > 1.0e-14 && au > 1.0e-16 ) {
						dens[i] = float( au / a3 );
						refNum += au;
						refDen += a3;
					}
				}
				const double refD = ( refDen > 1.0e-14 ) ? refNum / refDen : 1.0;
				const int NB = 14;
				QVector<QVector<Triangle>> bins( NB );
				for ( int i = 0; i < wireTris->size(); i++ ) {
					if ( dens[i] < 0.0f )
						continue;
					const double ratio = double( dens[i] ) / std::max( refD, 1.0e-20 );
					const double tt = std::clamp( std::log2( ratio ) / 2.0, -1.0, 1.0 );
					const int bin = std::clamp( int( ( tt + 1.0 ) * 0.5 * ( NB - 1 ) + 0.5 ), 0, NB - 1 );
					bins[bin] << wireTris->at( i );
				}
				const FloatVector4 evenCol( 0.20f, 0.80f, 0.30f, 0.55f );
				const FloatVector4 lowCol( 0.20f, 0.40f, 0.95f, 0.55f );
				const FloatVector4 highCol( 0.95f, 0.25f, 0.20f, 0.55f );
				for ( int bi = 0; bi < NB; bi++ ) {
					if ( bins[bi].isEmpty() )
						continue;
					const double tt = double( bi ) / ( NB - 1 ) * 2.0 - 1.0;
					FloatVector4 col = ( tt < 0.0 )
						? evenCol * ( 1.0f + float( tt ) ) + lowCol * float( -tt )
						: evenCol * ( 1.0f - float( tt ) ) + highCol * float( tt );
					col[3] = 0.55f;
					cx->bindShape( (unsigned int) nv, 0x43, size_t( bins[bi].size() ) * 6, attrData, bins[bi].constData() );
					if ( auto prog = cx->useProgram( "selection.prog" ); prog ) {
						prog->uni4f( "vertexColorOverride", FloatVector4( 1.0e-15f ).maxValues( col ) );
						prog->uni1i( "selectionFlags", 0 );
						prog->uni1i( "selectionParam", -1 );
						cx->fn->glDrawElements( GL_TRIANGLES, GLsizei( bins[bi].size() * 3 ), GL_UNSIGNED_SHORT, (void *) 0 );
					}
				}
			}
		} else if ( !wireTris->isEmpty() ) {
			// faint white fill on every visible face, so islands read as solid
			// shapes (Blender's face theme colour), under the selection fills/wires
			cx->bindShape( (unsigned int) nv, 0x43, size_t( wireTris->size() ) * 6, attrData, wireTris->constData() );
			if ( auto prog = cx->useProgram( "selection.prog" ); prog ) {
				prog->uni4f( "vertexColorOverride", FloatVector4( 1.0e-15f ).maxValues( uvColIslandFill ) );
				prog->uni1i( "selectionFlags", 0 );
				prog->uni1i( "selectionParam", -1 );
				cx->fn->glDrawElements( GL_TRIANGLES, GLsizei( wireTris->size() * 3 ), GL_UNSIGNED_SHORT, (void *) 0 );
			}
		}

		// Selected face fills, in EVERY mode (Blender): any face whose three
		// corners are all selected gets the brighter orange fill, derived from
		// the vertex ground truth so vertex/edge/face/island all fill correctly.
		{
			QVector<Triangle> fill;
			for ( const Triangle & t : *wireTris )
				if ( sVerts.contains( t[0] ) && sVerts.contains( t[1] )
					&& sVerts.contains( t[2] ) )
					fill << t;
			if ( !fill.isEmpty() ) {
				cx->bindShape( (unsigned int) nv, 0x43, size_t( fill.size() ) * 6, attrData, fill.constData() );
				if ( auto prog = cx->useProgram( "selection.prog" ); prog ) {
					prog->uni4f( "vertexColorOverride",
						FloatVector4( 1.0e-15f ).maxValues( uvColFaceFill ) );
					prog->uni1i( "selectionFlags", 0 );
					prog->uni1i( "selectionParam", -1 );
					cx->fn->glDrawElements( GL_TRIANGLES, GLsizei( fill.size() * 3 ), GL_UNSIGNED_SHORT, (void *) 0 );
				}
			}
		}

		// all (visible) wires, per-vertex colored (selection gradient)
		if ( !wireTris->isEmpty() ) {
			cx->bindShape( (unsigned int) nv, 0x43, size_t( wireTris->size() ) * 6, attrData, wireTris->constData() );
			if ( auto prog = cx->useProgram( "wireframe.prog" ); prog ) {
				prog->uni4f( "vertexColorOverride", FloatVector4( 0.0f ) );
				prog->uni1i( "selectionParam", -1 );
				prog->uni1f( "lineWidth", GLView::Settings::lineWidthWireframe * 0.625f );
				cx->fn->glDrawElements( GL_TRIANGLES, GLsizei( wireTris->size() * 3 ), GL_UNSIGNED_SHORT, (void *) 0 );
			}
		}

		// explicit selected edges (edge mode)
		if ( selectMode == 2 && !sEdges.isEmpty() ) {
			QVector<quint16> lineIdx;
			lineIdx.reserve( sEdges.size() * 2 );
			for ( quint64 key : sEdges )
				lineIdx << quint16( key >> 32 ) << quint16( key & 0xFFFFFFFFu );
			cx->bindShape( (unsigned int) nv, 0x43, size_t( lineIdx.size() ) * 2, attrData, lineIdx.constData() );
			if ( auto prog = cx->useProgram( "lines.prog" ); prog ) {
				prog->uni4f( "vertexColorOverride",
					FloatVector4( 1.0e-15f ).maxValues( uvColSelected ) );
				prog->uni1i( "selectionParam", -1 );
				prog->uni1f( "lineWidth", GLView::Settings::lineWidthHighlight );
				cx->fn->glDrawElements( GL_LINES, GLsizei( lineIdx.size() ), GL_UNSIGNED_SHORT, (void *) 0 );
			}
		}

		// vertex dots — Blender shows them in every select mode. Unselected are
		// dark dots, selected orange, active bright orange; edges keep the gray
		// wireframe color above (this is a second, point-specific color buffer).
		{
			QVector<FloatVector4> pointColors( nv );
			for ( qsizetype i = 0; i < nv; i++ ) {
				if ( !sPins.isEmpty() && sPins.contains( int( i ) ) )
					pointColors[i] = FloatVector4( 0.95f, 0.15f, 0.15f, 1.0f );  // Blender pins are red
				else if ( int( i ) == actV )
					pointColors[i] = uvColActive;
				else if ( sVerts.contains( int( i ) ) )
					pointColors[i] = uvColSelected;
				else
					pointColors[i] = uvColVertUnsel;
			}
			const float * ptAttr[2] = { &( posBuf[0][0] ), &( pointColors[0][0] ) };
			QVector<quint16> visible;
			if ( !syncSelection ) {
				visible.reserve( sVp3d.size() );
				for ( int v : sVp3d )
					if ( v >= 0 && v < int( nv ) )
						visible << quint16( v );
			}
			const bool drawAll = syncSelection;
			if ( drawAll || !visible.isEmpty() ) {
				cx->bindShape( (unsigned int) nv, 0x43,
					drawAll ? 0 : size_t( visible.size() ) * 2, ptAttr,
					drawAll ? nullptr : visible.constData() );
				if ( auto prog = cx->useProgram( "selection.prog" ); prog ) {
					prog->uni4f( "vertexColorOverride", FloatVector4( 0.0f ) );
					float pointSize = GLView::Settings::vertexPointSize * 0.7f + 0.5f;
					prog->uni1i( "selectionFlags", ( roundFloat( std::min( pointSize * 8.0f, 255.0f ) ) << 8 ) | 0x0002 );
					prog->uni1i( "selectionParam", -1 );
					glPointSize( pointSize );
					if ( drawAll )
						cx->fn->glDrawArrays( GL_POINTS, 0, GLsizei( nv ) );
					else
						cx->fn->glDrawElements( GL_POINTS, GLsizei( visible.size() ), GL_UNSIGNED_SHORT, (void *) 0 );
				}
			}
		}
	}

	// --- overlays: cursor, snap indicator, box select ---
	auto drawLines = [this]( const QVector<Vector3> & pts, const FloatVector4 & color, float width, unsigned int mode ) {
		if ( pts.isEmpty() )
			return;
		const float * p = &( pts.constData()[0][0] );
		cx->bindShape( (unsigned int) pts.size(), 0x03, 0, &p, nullptr );
		if ( auto prog = cx->useProgram( "lines.prog" ); prog ) {
			prog->uni4f( "vertexColorOverride", FloatVector4( 1.0e-15f ).maxValues( color ) );
			prog->uni1i( "selectionParam", -1 );
			prog->uni1f( "lineWidth", width );
			cx->fn->glDrawArrays( mode, 0, GLsizei( pts.size() ) );
		}
	};
	auto drawPoint = [this]( const Vector2 & uv, const FloatVector4 & color, float pointSize ) {
		Vector3 pt( uv[0], uv[1], 0.6f );
		const float * p = &( pt[0] );
		cx->bindShape( 1, 0x03, 0, &p, nullptr );
		if ( auto prog = cx->useProgram( "selection.prog" ); prog ) {
			prog->uni4f( "vertexColorOverride", FloatVector4( 1.0e-15f ).maxValues( color ) );
			prog->uni1i( "selectionFlags", ( roundFloat( std::min( pointSize * 8.0f, 255.0f ) ) << 8 ) | 0x0002 );
			prog->uni1i( "selectionParam", -1 );
			glPointSize( pointSize );
			cx->fn->glDrawArrays( GL_POINTS, 0, 1 );
		}
	};

	// (the Blender-style 2D cursor is drawn with QPainter after GL, in paintGL)

	if ( snapIndicatorOn )
		drawPoint( snapIndicatorUV, uvColActive, GLView::Settings::vertexPointSize + 2.0f );

	if ( boxDrag ) {
		Vector2 a = mapToUV( boxStartPx );
		Vector2 b = mapToUV( boxCurPx );
		QVector<Vector3> rect;
		rect << Vector3( a[0], a[1], 0.7f ) << Vector3( b[0], a[1], 0.7f )
			<< Vector3( b[0], b[1], 0.7f ) << Vector3( a[0], b[1], 0.7f );
		drawLines( rect, FloatVector4( 1.0f, 1.0f, 1.0f, 0.8f ),
			GLView::Settings::lineWidthWireframe * 0.5f, GL_LINE_LOOP );
	}

	glDisable( GL_BLEND );
}

// ---------------------------------------------------------------------------
// data / sync

void UVEditorView::clearData()
{
	cancelOperatorPanel();
	shapes.clear();
	activeShape = -1;
	selVerts.clear();
	selEdges.clear();
	selFaces.clear();
	pinnedVerts.clear();
	activeVert = -1;
	activeFace = -1;
	xformMode = 0;
	update();
	notifySelectionInfo();
}

void UVEditorView::rebuildFromViewport()
{
	// the shape set / edit session changed: the armed operator's seed selection
	// no longer matches the mesh (guarded internally while a re-run rebuilds us)
	cancelOperatorPanel();
	shapes.clear();
	activeShape = -1;
	selVerts.clear();
	selEdges.clear();
	selFaces.clear();
	pinnedVerts.clear();
	viewport3DVerts.clear();
	hiddenFaces.clear();
	hiddenVerts.clear();
	activeVert = -1;
	activeFace = -1;
	xformMode = 0;
	if ( !nif || !ogl ) {
		objectModeView = false;
		update();
		notifySelectionInfo();
		setStatus( tr( "Open a mesh to see its UVs." ) );
		return;
	}

	objectModeView = !ogl->editMode;

	// Edit Mode edits the meshes in the edit session; Object Mode shows the
	// object-mode selection read-only (primary white, secondaries colored).
	QList<int> blocks;
	int activeBlockNum = -1;
	if ( ogl->editMode ) {
		blocks = ogl->editShapeBlocks.values();
		activeBlockNum = ogl->editShapeBlock;
	} else {
		QSet<int> sel = ogl->objSelection;
		if ( ogl->objActive >= 0 )
			sel.insert( ogl->objActive );
		blocks = sel.values();
		activeBlockNum = ogl->objActive;
	}
	std::sort( blocks.begin(), blocks.end() );
	for ( int block : std::as_const( blocks ) ) {
		UVShapeData sd;
		if ( loadShapeUVs( nif, block, sd, uvSetIndex ) )
			shapes << sd;
	}
	for ( int s = 0; s < shapes.size(); s++ )
		if ( shapes.at( s ).block == activeBlockNum )
			activeShape = s;
	if ( activeShape < 0 && !shapes.isEmpty() )
		activeShape = 0;
	refreshTexSlots();
	if ( activeShape < 0 )
		setStatus( objectModeView
			? tr( "Select a UV-mapped mesh to view its UVs." )
			: tr( "No UV-mapped mesh in Edit Mode." ) );
	else if ( objectModeView )
		setStatus( tr( "%1 — %2 UVs (Object Mode: read-only%3)" )
			.arg( nif->get<QString>( QModelIndex( shapes.at( activeShape ).iShape ), "Name" ) )
			.arg( shapes.at( activeShape ).uvs.size() )
			.arg( shapes.size() > 1 ? tr( ", %1 meshes" ).arg( shapes.size() ) : QString() ) );
	else
		setStatus( tr( "%1 — %2 UVs, %3 islands" )
			.arg( nif->get<QString>( QModelIndex( shapes.at( activeShape ).iShape ), "Name" ) )
			.arg( shapes.at( activeShape ).uvs.size() )
			.arg( shapes.at( activeShape ).islandCount ) );
	syncSelectionFromViewport();
	update();
}

void UVEditorView::refreshTexSlots()
{
	texSlots.clear();
	if ( const UVShapeData * sd = active(); sd && nif )
		texSlots = findTextureSlots( nif, QModelIndex( sd->iShape ) );
	if ( underlaySlot >= texSlots.size() )
		underlaySlot = texSlots.isEmpty() ? -1 : 0;
	if ( underlaySlot < 0 && !texSlots.isEmpty() )
		underlaySlot = 0;
}

void UVEditorView::reloadShapeUVs( int block )
{
	for ( UVShapeData & sd : shapes ) {
		if ( sd.block != block || !sd.valid )
			continue;
		const int nv = sd.uvs.size();
		if ( sd.legacyData ) {
			QVector<Vector2> fresh = nif->getArray<Vector2>( QModelIndex( sd.iUVData ) );
			if ( fresh.size() == nv )
				sd.uvs = fresh;
		} else {
			for ( int i = 0; i < nv; i++ )
				sd.uvs[i] = nif->get<Vector2>( nif->index( i, 0, QModelIndex( sd.iUVData ) ), "UV" );
		}
		// merged/separated seams change which islands are welded together
		uvRebuildIslands( sd );
		update();
		return;
	}
}

bool UVEditorView::shapeRowMatches( const QModelIndex & idx ) const
{
	if ( !nif || !idx.isValid() )
		return false;
	const int block = nif->getBlockNumber( idx );
	for ( const UVShapeData & sd : shapes )
		if ( sd.block == block )
			return true;
	return false;
}

//! Derive the edge/face sets implied by a vertex set (shared by the active
//! shape's rebuildDerivedSelection and the multi-mesh per-shape sync)
static void uvDeriveEdgesFaces( const UVShapeData & sd, const QSet<int> & verts,
	QSet<quint64> & edges, QSet<int> & faces )
{
	edges.clear();
	faces.clear();
	for ( int f = 0; f < sd.tris.size(); f++ ) {
		const Triangle & t = sd.tris.at( f );
		const bool s0 = verts.contains( t[0] );
		const bool s1 = verts.contains( t[1] );
		const bool s2 = verts.contains( t[2] );
		if ( s0 && s1 ) edges << uvEdgeKey( t[0], t[1] );
		if ( s1 && s2 ) edges << uvEdgeKey( t[1], t[2] );
		if ( s0 && s2 ) edges << uvEdgeKey( t[0], t[2] );
		if ( s0 && s1 && s2 ) faces << f;
	}
}

void UVEditorView::syncSelectionFromViewport()
{
	if ( !ogl )
		return;
	// Object Mode is a read-only display: there is no element selection to
	// mirror, and the whole mesh is always shown.
	if ( objectModeView || shapes.isEmpty() )
		return;

	bool anyChange = false;
	for ( int s = 0; s < shapes.size(); s++ ) {
		UVShapeData & osd = shapes[s];
		if ( !osd.valid )
			continue;
		const bool isActive = ( s == activeShape );
		QSet<int> verts;
		QSet<quint64> edges;
		QSet<int> faces;
		int lastVert = -1, lastFace = -1;
		for ( const GLView::PickedElement & pe : std::as_const( ogl->pickedElems ) ) {
			if ( pe.shapeBlock != osd.block )
				continue;
			if ( pe.type == 1 && pe.e0 >= 0 && pe.e0 < osd.uvs.size() ) {
				verts << pe.e0;
				lastVert = pe.e0;
			} else if ( pe.type == 2 && pe.e0 >= 0 && pe.e1 >= 0
						&& pe.e0 < osd.uvs.size() && pe.e1 < osd.uvs.size() ) {
				edges << uvEdgeKey( pe.e0, pe.e1 );
				verts << pe.e0 << pe.e1;
			} else if ( pe.type == 3 && pe.e0 >= 0 && pe.e0 < osd.tris.size() ) {
				faces << pe.e0;
				const Triangle & t = osd.tris.at( pe.e0 );
				verts << t[0] << t[1] << t[2];
				lastFace = pe.e0;
			}
		}
		// always track the 3D selection: it is the sync-off visibility filter
		QSet<int> & vp3d = isActive ? viewport3DVerts : osd.viewport3DVerts;
		if ( vp3d != verts ) {
			vp3d = verts;
			anyChange = true;
		}
		if ( !syncSelection )
			continue;

		QSet<int> & sVerts = isActive ? selVerts : osd.selVerts;
		if ( verts == sVerts ) {
			// selection unchanged (usually the echo of our own push). Do NOT
			// adopt the pick lists wholesale here: vertex-typed echoes would
			// wipe the derived edge/face/island sets and make face and island
			// selections appear to "not work".
			continue;
		}
		sVerts = verts;
		QSet<quint64> & sEdges = isActive ? selEdges : osd.selEdges;
		QSet<int> & sFaces = isActive ? selFaces : osd.selFaces;
		if ( selectMode == 4 || ( faces.isEmpty() && edges.isEmpty() && selectMode != 1 ) ) {
			// island mode (or a vertex-typed echo while in edge/face mode):
			// re-derive edges/faces from the vertex ground truth
			uvDeriveEdgesFaces( osd, sVerts, sEdges, sFaces );
		} else {
			sEdges = edges;
			sFaces = faces;
		}
		if ( isActive ) {
			if ( lastVert >= 0 )
				activeVert = lastVert;
			if ( lastFace >= 0 )
				activeFace = lastFace;
		}
		anyChange = true;
	}
	if ( !anyChange )
		return;
	// mirror the 3D pick mode when it differs (island stays UV-side)
	if ( syncSelection && selectMode != 4
		&& ogl->pickMode >= 1 && ogl->pickMode <= 3 && ogl->pickMode != selectMode )
		setSelectMode( ogl->pickMode, true );
	notifySelectionInfo();
	update();
}

void UVEditorView::pushShapeSelectionToViewport( int s )
{
	// with sync off the UV selection stays local (Blender behavior)
	if ( !ogl || !ogl->editMode || !syncSelection )
		return;
	if ( s < 0 || s >= shapes.size() || !shapes.at( s ).valid )
		return;
	const UVShapeData & sd = shapes.at( s );
	const bool isActive = ( s == activeShape );
	const QSet<int> & verts = isActive ? selVerts : sd.selVerts;
	const QSet<quint64> & edges = isActive ? selEdges : sd.selEdges;
	const QSet<int> & faces = isActive ? selFaces : sd.selFaces;
	QVector<GLView::PickedElement> elems;
	int mode = ( selectMode >= 1 && selectMode <= 3 ) ? selectMode : 1;
	if ( selectMode == 3 ) {
		elems.reserve( faces.size() );
		for ( int f : faces ) {
			GLView::PickedElement pe;
			pe.shapeBlock = sd.block;
			pe.type = 3;
			pe.e0 = f;
			elems << pe;
		}
	} else if ( selectMode == 2 ) {
		elems.reserve( edges.size() );
		for ( quint64 key : edges ) {
			GLView::PickedElement pe;
			pe.shapeBlock = sd.block;
			pe.type = 2;
			pe.e0 = int( key >> 32 );
			pe.e1 = int( key & 0xFFFFFFFFu );
			elems << pe;
		}
	} else {
		// vertex and island selections surface in 3D as vertices
		elems.reserve( verts.size() );
		for ( int v : verts ) {
			GLView::PickedElement pe;
			pe.shapeBlock = sd.block;
			pe.type = 1;
			pe.e0 = v;
			elems << pe;
		}
		mode = 1;
	}
	ogl->setElementSelectionExternal( sd.block, elems, mode );
}

void UVEditorView::pushSelectionToViewport()
{
	pushShapeSelectionToViewport( activeShape );
	notifySelectionInfo();
}

// ---------------------------------------------------------------------------
// selection

void UVEditorView::rebuildDerivedSelection()
{
	const UVShapeData * sd = active();
	if ( !sd )
		return;
	uvDeriveEdgesFaces( *sd, selVerts, selEdges, selFaces );
}

void UVEditorView::stashActiveSelection()
{
	if ( activeShape < 0 || activeShape >= shapes.size() )
		return;
	UVShapeData & sd = shapes[activeShape];
	sd.selVerts = selVerts;
	sd.selEdges = selEdges;
	sd.selFaces = selFaces;
	sd.viewport3DVerts = viewport3DVerts;
	sd.hiddenFaces = hiddenFaces;
	sd.hiddenVerts = hiddenVerts;
	sd.pinnedVerts = pinnedVerts;
}

void UVEditorView::adoptActiveSelection( int s )
{
	if ( s < 0 || s >= shapes.size() || s == activeShape )
		return;
	stashActiveSelection();
	activeShape = s;
	const UVShapeData & sd = shapes.at( s );
	selVerts = sd.selVerts;
	selEdges = sd.selEdges;
	selFaces = sd.selFaces;
	viewport3DVerts = sd.viewport3DVerts;
	hiddenFaces = sd.hiddenFaces;
	hiddenVerts = sd.hiddenVerts;
	pinnedVerts = sd.pinnedVerts;
	activeVert = -1;
	activeFace = -1;
	// the operator panel's seed selection belongs to the previous active shape
	cancelOperatorPanel();
}

int UVEditorView::pickShapeAt( const QPointF & devicePx, double radiusPx ) const
{
	// nearest visible vertex across every edit-session shape…
	int bestShape = -1;
	double bestDist = radiusPx * radiusPx;
	for ( int s = 0; s < shapes.size(); s++ ) {
		const UVShapeData & sd = shapes.at( s );
		if ( !sd.valid )
			continue;
		for ( int i = 0; i < sd.uvs.size(); i++ ) {
			if ( !vertVisibleUVIn( s, i ) )
				continue;
			QPointF d = mapFromUV( sd.uvs.at( i ) ) - devicePx;
			double dd = d.x() * d.x() + d.y() * d.y();
			if ( dd < bestDist ) {
				bestDist = dd;
				bestShape = s;
			}
		}
	}
	if ( bestShape >= 0 )
		return bestShape;

	// …else the nearest visible edge (edge mode picks edge bodies)
	if ( selectMode == 2 ) {
		auto segDist2 = []( const QPointF & p, const QPointF & a, const QPointF & b ) {
			const QPointF ab = b - a, ap = p - a;
			const double len2 = ab.x() * ab.x() + ab.y() * ab.y();
			const double t = ( len2 > 0.0 )
				? std::clamp( ( ap.x() * ab.x() + ap.y() * ab.y() ) / len2, 0.0, 1.0 ) : 0.0;
			const QPointF q = a + t * ab - p;
			return q.x() * q.x() + q.y() * q.y();
		};
		const double edgeR2 = ( radiusPx + 4.0 ) * ( radiusPx + 4.0 );
		double best = edgeR2;
		for ( int s = 0; s < shapes.size(); s++ ) {
			const UVShapeData & sd = shapes.at( s );
			if ( !sd.valid )
				continue;
			for ( int f = 0; f < sd.tris.size(); f++ ) {
				const Triangle & t = sd.tris.at( f );
				if ( !faceVisibleUVIn( s, f, t ) )
					continue;
				for ( int e = 0; e < 3; e++ ) {
					const double dd = segDist2( devicePx,
						mapFromUV( sd.uvs.at( t[e] ) ), mapFromUV( sd.uvs.at( t[( e + 1 ) % 3] ) ) );
					if ( dd < best ) {
						best = dd;
						bestShape = s;
					}
				}
			}
		}
		return bestShape;
	}

	// …else the face body under the cursor (face/island modes; topmost first)
	if ( selectMode == 3 || selectMode == 4 ) {
		const Vector2 uv = mapToUV( devicePx );
		auto cross2 = []( const Vector2 & u, const Vector2 & v ) { return u[0] * v[1] - u[1] * v[0]; };
		for ( int s = shapes.size() - 1; s >= 0; s-- ) {
			const UVShapeData & sd = shapes.at( s );
			if ( !sd.valid )
				continue;
			for ( int f = 0; f < sd.tris.size(); f++ ) {
				const Triangle & t = sd.tris.at( f );
				if ( !faceVisibleUVIn( s, f, t ) )
					continue;
				const Vector2 & a = sd.uvs.at( t[0] );
				const Vector2 & b = sd.uvs.at( t[1] );
				const Vector2 & c = sd.uvs.at( t[2] );
				const float c1 = cross2( b - a, uv - a );
				const float c2 = cross2( c - b, uv - b );
				const float c3 = cross2( a - c, uv - c );
				if ( ( c1 >= 0.0f && c2 >= 0.0f && c3 >= 0.0f )
					|| ( c1 <= 0.0f && c2 <= 0.0f && c3 <= 0.0f ) )
					return s;
			}
		}
	}
	return -1;
}

int UVEditorView::nearestVertex( const QPointF & devicePx, double radiusPx, const QSet<int> * exclude ) const
{
	const UVShapeData * sd = active();
	if ( !sd )
		return -1;
	int best = -1;
	double bestDist = radiusPx * radiusPx;
	for ( int i = 0; i < sd->uvs.size(); i++ ) {
		if ( exclude && exclude->contains( i ) )
			continue;
		if ( !vertVisibleUV( i ) )
			continue;
		QPointF d = mapFromUV( sd->uvs.at( i ) ) - devicePx;
		double dd = d.x() * d.x() + d.y() * d.y();
		if ( dd < bestDist ) {
			bestDist = dd;
			best = i;
		}
	}
	return best;
}

int UVEditorView::faceUnder( const Vector2 & uv ) const
{
	const UVShapeData * sd = active();
	if ( !sd )
		return -1;
	for ( int f = 0; f < sd->tris.size(); f++ ) {
		const Triangle & t = sd->tris.at( f );
		if ( !faceVisibleUV( f, t ) )
			continue;
		const Vector2 & a = sd->uvs.at( t[0] );
		const Vector2 & b = sd->uvs.at( t[1] );
		const Vector2 & c = sd->uvs.at( t[2] );
		const float d = ( b[1] - c[1] ) * ( a[0] - c[0] ) + ( c[0] - b[0] ) * ( a[1] - c[1] );
		if ( std::fabs( d ) < 1.0e-12f )
			continue;
		const float w0 = ( ( b[1] - c[1] ) * ( uv[0] - c[0] ) + ( c[0] - b[0] ) * ( uv[1] - c[1] ) ) / d;
		const float w1 = ( ( c[1] - a[1] ) * ( uv[0] - c[0] ) + ( a[0] - c[0] ) * ( uv[1] - c[1] ) ) / d;
		const float w2 = 1.0f - w0 - w1;
		if ( w0 >= 0.0f && w1 >= 0.0f && w2 >= 0.0f )
			return f;
	}
	return -1;
}

bool UVEditorView::edgeUnder( const QPointF & devicePx, double radiusPx, int & outA, int & outB ) const
{
	const UVShapeData * sd = active();
	if ( !sd )
		return false;
	double bestDist = radiusPx * radiusPx;
	bool found = false;
	QSet<quint64> seen;
	for ( int f = 0; f < sd->tris.size(); f++ ) {
		const Triangle & t = sd->tris.at( f );
		if ( !faceVisibleUV( f, t ) )
			continue;
		for ( int e = 0; e < 3; e++ ) {
			int a = t[e], b = t[( e + 1 ) % 3];
			quint64 key = uvEdgeKey( a, b );
			if ( seen.contains( key ) )
				continue;
			seen << key;
			QPointF pa = mapFromUV( sd->uvs.at( a ) );
			QPointF pb = mapFromUV( sd->uvs.at( b ) );
			QPointF ab = pb - pa;
			double len2 = ab.x() * ab.x() + ab.y() * ab.y();
			double tPar = len2 > 0.0
				? std::clamp( ( ( devicePx.x() - pa.x() ) * ab.x() + ( devicePx.y() - pa.y() ) * ab.y() ) / len2, 0.0, 1.0 )
				: 0.0;
			QPointF proj = pa + ab * tPar;
			QPointF d = devicePx - proj;
			double dd = d.x() * d.x() + d.y() * d.y();
			if ( dd < bestDist ) {
				bestDist = dd;
				outA = a;
				outB = b;
				found = true;
			}
		}
	}
	return found;
}

void UVEditorView::pickAt( const QPointF & devicePx, bool extend )
{
	if ( objectModeView )
		return;
	if ( !active() )
		return;
	const double radius = 9.0 * devicePixelRatioF();

	// multi-mesh: the pick may land on another edit-session shape — it becomes
	// the active one (Blender multi-object editing). A plain (non-extend) pick
	// is exclusive across every shape, so clear the others' stored selections.
	const int hitShape = pickShapeAt( devicePx, radius );
	if ( hitShape >= 0 && hitShape != activeShape )
		adoptActiveSelection( hitShape );	// selection content unchanged: no push needed
	if ( !extend ) {
		for ( int s = 0; s < shapes.size(); s++ ) {
			if ( s == activeShape || !shapes.at( s ).valid )
				continue;
			UVShapeData & osd = shapes[s];
			if ( osd.selVerts.isEmpty() && osd.selEdges.isEmpty() && osd.selFaces.isEmpty() )
				continue;
			osd.selVerts.clear();
			osd.selEdges.clear();
			osd.selFaces.clear();
			pushShapeSelectionToViewport( s );
		}
	}
	const UVShapeData * sd = active();

	if ( selectMode == 1 ) {
		int v = nearestVertex( devicePx, radius );
		if ( v < 0 ) {
			if ( !extend ) { selVerts.clear(); rebuildDerivedSelection(); pushSelectionToViewport(); update(); }
			return;
		}
		// sticky (Shared Location): the pick includes co-located seam partners
		QSet<int> group;
		group << v;
		expandSticky( group );
		if ( extend && selVerts.contains( v ) && activeVert == v )
			selVerts -= group;
		else if ( extend )
			selVerts += group;
		else
			selVerts = group;
		activeVert = v;
	} else if ( selectMode == 2 ) {
		int a = -1, b = -1;
		if ( !edgeUnder( devicePx, radius + 4.0, a, b ) ) {
			if ( !extend ) { selVerts.clear(); selEdges.clear(); pushSelectionToViewport(); update(); }
			return;
		}
		quint64 key = uvEdgeKey( a, b );
		if ( extend && selEdges.contains( key ) )
			selEdges.remove( key );
		else if ( extend )
			selEdges << key;
		else {
			selEdges.clear();
			selEdges << key;
		}
		selVerts.clear();
		for ( quint64 k : std::as_const( selEdges ) )
			selVerts << int( k >> 32 ) << int( k & 0xFFFFFFFFu );
		expandSticky( selVerts );
	} else if ( selectMode == 3 ) {
		int f = faceUnder( mapToUV( devicePx ) );
		if ( f < 0 ) {
			if ( !extend ) { selVerts.clear(); selFaces.clear(); pushSelectionToViewport(); update(); }
			return;
		}
		if ( extend && selFaces.contains( f ) && activeFace == f )
			selFaces.remove( f );
		else if ( extend )
			selFaces << f;
		else {
			selFaces.clear();
			selFaces << f;
		}
		activeFace = f;
		selVerts.clear();
		for ( int fi : std::as_const( selFaces ) ) {
			const Triangle & t = sd->tris.at( fi );
			selVerts << t[0] << t[1] << t[2];
		}
	} else {
		// island mode: pick by vertex or face body
		int v = nearestVertex( devicePx, radius );
		if ( v < 0 ) {
			int f = faceUnder( mapToUV( devicePx ) );
			if ( f >= 0 )
				v = sd->tris.at( f )[0];
		}
		if ( v < 0 ) {
			if ( !extend ) { selVerts.clear(); selFaces.clear(); selEdges.clear(); pushSelectionToViewport(); update(); }
			return;
		}
		const int island = sd->islandOfVert.at( v );
		QSet<int> islandVerts;
		for ( int i = 0; i < sd->uvs.size(); i++ )
			if ( sd->islandOfVert.at( i ) == island )
				islandVerts << i;
		const bool wasSelected = selVerts.contains( v );
		if ( extend && wasSelected )
			selVerts -= islandVerts;
		else if ( extend )
			selVerts += islandVerts;
		else
			selVerts = islandVerts;
		rebuildDerivedSelection();
	}
	pushSelectionToViewport();
	notifySelectionInfo();
	update();
}

void UVEditorView::applyBoxSelect( const QRectF & deviceRect, Qt::KeyboardModifiers mods )
{
	if ( objectModeView || shapes.isEmpty() )
		return;
	// same convention as the 3D box select: plain adds, Shift/Ctrl deselects
	const bool subtract = mods.testFlag( Qt::ShiftModifier ) || mods.testFlag( Qt::ControlModifier );

	// multi-mesh: the box applies to every shape in the edit session
	for ( int s = 0; s < shapes.size(); s++ ) {
		UVShapeData & osd = shapes[s];
		if ( !osd.valid )
			continue;
		const bool isActive = ( s == activeShape );
		QSet<int> & sVerts = isActive ? selVerts : osd.selVerts;
		QSet<quint64> & sEdges = isActive ? selEdges : osd.selEdges;
		QSet<int> & sFaces = isActive ? selFaces : osd.selFaces;

		QSet<int> boxVerts;
		for ( int i = 0; i < osd.uvs.size(); i++ )
			if ( vertVisibleUVIn( s, i ) && deviceRect.contains( mapFromUV( osd.uvs.at( i ) ) ) )
				boxVerts << i;
		if ( boxVerts.isEmpty() )
			continue;

		if ( selectMode == 4 ) {
			QSet<int> islands;
			for ( int v : std::as_const( boxVerts ) )
				islands << osd.islandOfVert.at( v );
			boxVerts.clear();
			for ( int i = 0; i < osd.uvs.size(); i++ )
				if ( islands.contains( osd.islandOfVert.at( i ) ) )
					boxVerts << i;
		}

		if ( selectMode == 2 ) {
			QSet<quint64> seen;
			for ( const Triangle & t : std::as_const( osd.tris ) ) {
				for ( int e = 0; e < 3; e++ ) {
					int a = t[e], b = t[( e + 1 ) % 3];
					quint64 key = uvEdgeKey( a, b );
					if ( seen.contains( key ) )
						continue;
					seen << key;
					if ( boxVerts.contains( a ) && boxVerts.contains( b ) ) {
						if ( subtract ) sEdges.remove( key );
						else sEdges << key;
					}
				}
			}
			sVerts.clear();
			for ( quint64 k : std::as_const( sEdges ) )
				sVerts << int( k >> 32 ) << int( k & 0xFFFFFFFFu );
			expandStickyIn( s, sVerts );
		} else if ( selectMode == 3 ) {
			for ( int f = 0; f < osd.tris.size(); f++ ) {
				const Triangle & t = osd.tris.at( f );
				if ( boxVerts.contains( t[0] ) && boxVerts.contains( t[1] ) && boxVerts.contains( t[2] ) ) {
					if ( subtract ) sFaces.remove( f );
					else sFaces << f;
				}
			}
			sVerts.clear();
			for ( int fi : std::as_const( sFaces ) ) {
				const Triangle & t = osd.tris.at( fi );
				sVerts << t[0] << t[1] << t[2];
			}
		} else {
			expandStickyIn( s, boxVerts );	// sticky: seam partners join/leave together
			if ( subtract ) sVerts -= boxVerts;
			else sVerts += boxVerts;
			uvDeriveEdgesFaces( osd, sVerts, sEdges, sFaces );
		}
		pushShapeSelectionToViewport( s );
	}
	notifySelectionInfo();
	update();
}

void UVEditorView::selectAllUV( int action )
{
	if ( !active() )
		return;
	// multi-mesh: A toggles over the whole edit session — anything short of
	// everything selected means "select all", everywhere
	bool all = false;
	if ( action == 0 ) {
		for ( int s = 0; s < shapes.size() && !all; s++ ) {
			const UVShapeData & osd = shapes.at( s );
			if ( !osd.valid )
				continue;
			const QSet<int> & sVerts = ( s == activeShape ) ? selVerts : osd.selVerts;
			all = ( sVerts.size() < osd.uvs.size() );
		}
	} else {
		all = ( action == 1 );
	}
	for ( int s = 0; s < shapes.size(); s++ ) {
		UVShapeData & osd = shapes[s];
		if ( !osd.valid )
			continue;
		const bool isActive = ( s == activeShape );
		QSet<int> & sVerts = isActive ? selVerts : osd.selVerts;
		QSet<quint64> & sEdges = isActive ? selEdges : osd.selEdges;
		QSet<int> & sFaces = isActive ? selFaces : osd.selFaces;
		sVerts.clear();
		if ( all )
			for ( int i = 0; i < osd.uvs.size(); i++ )
				if ( vertVisibleUVIn( s, i ) )
					sVerts << i;
		uvDeriveEdgesFaces( osd, sVerts, sEdges, sFaces );
		pushShapeSelectionToViewport( s );
	}
	notifySelectionInfo();
	update();
}

void UVEditorView::invertSelection()
{
	if ( !active() )
		return;
	for ( int s = 0; s < shapes.size(); s++ ) {
		UVShapeData & osd = shapes[s];
		if ( !osd.valid )
			continue;
		const bool isActive = ( s == activeShape );
		QSet<int> & sVerts = isActive ? selVerts : osd.selVerts;
		QSet<quint64> & sEdges = isActive ? selEdges : osd.selEdges;
		QSet<int> & sFaces = isActive ? selFaces : osd.selFaces;
		QSet<int> inverted;
		for ( int i = 0; i < osd.uvs.size(); i++ )
			if ( vertVisibleUVIn( s, i ) && !sVerts.contains( i ) )
				inverted << i;
		sVerts = inverted;
		uvDeriveEdgesFaces( osd, sVerts, sEdges, sFaces );
		pushShapeSelectionToViewport( s );
	}
	notifySelectionInfo();
	update();
}

void UVEditorView::setSelectMode( int mode, bool fromViewport )
{
	if ( mode < 1 || mode > 4 || mode == selectMode )
		return;
	selectMode = mode;
	// derive edge/face membership from the vertex ground truth (every shape:
	// the mode reinterprets each stored selection)
	rebuildDerivedSelection();
	for ( int s = 0; s < shapes.size(); s++ ) {
		if ( s == activeShape || !shapes.at( s ).valid )
			continue;
		UVShapeData & osd = shapes[s];
		uvDeriveEdgesFaces( osd, osd.selVerts, osd.selEdges, osd.selFaces );
	}
	if ( !fromViewport ) {
		if ( ogl && mode >= 1 && mode <= 3 )
			ogl->setElementPickMode( mode );
		for ( int s = 0; s < shapes.size(); s++ )
			pushShapeSelectionToViewport( s );
		notifySelectionInfo();
	}
	if ( modeChangedCb )
		modeChangedCb( selectMode );
	update();
}

void UVEditorView::selectLinkedUnderCursor( bool add )
{
	if ( !active() )
		return;
	QPointF devicePx = QPointF( mapFromGlobal( QCursor::pos() ) ) * devicePixelRatioF();
	// multi-mesh: L works on whichever shape is under the cursor
	const int hitShape = pickShapeAt( devicePx, 24.0 * devicePixelRatioF() );
	if ( hitShape >= 0 && hitShape != activeShape )
		adoptActiveSelection( hitShape );
	const UVShapeData * sd = active();
	int v = nearestVertex( devicePx, 24.0 * devicePixelRatioF() );
	if ( v < 0 ) {
		int f = faceUnder( mapToUV( devicePx ) );
		if ( f >= 0 )
			v = sd->tris.at( f )[0];
	}
	if ( v < 0 )
		return;
	const int island = sd->islandOfVert.at( v );
	if ( !add )
		selVerts.clear();
	for ( int i = 0; i < sd->uvs.size(); i++ )
		if ( sd->islandOfVert.at( i ) == island )
			selVerts << i;
	rebuildDerivedSelection();
	pushSelectionToViewport();
	notifySelectionInfo();
	update();
}

void UVEditorView::setSyncSelection( bool on )
{
	if ( syncSelection == on )
		return;
	syncSelection = on;
	QSettings().setValue( "UVEditor/SyncSelection", on );
	if ( on ) {
		// re-adopt the viewport selection as ours
		selVerts.clear();
		syncSelectionFromViewport();
		setStatus( tr( "UV Sync Selection on — selection mirrors the 3D viewport." ) );
	} else {
		setStatus( tr( "UV Sync Selection off — only faces selected in 3D are shown; UV selection is local." ) );
	}
	notifySelectionInfo();
	update();
}

void UVEditorView::setUVSet( int set )
{
	if ( set < 0 || set == uvSetIndex )
		return;
	uvSetIndex = set;
	// same topology, different coordinates: reload UVs in place and keep the
	// selection (vertex indices are shared between UV sets)
	for ( UVShapeData & sd : shapes ) {
		if ( !sd.valid || !sd.legacyData || !sd.iUVSets.isValid() )
			continue;
		const int clamped = std::clamp( set, 0, sd.uvSetCount - 1 );
		QModelIndex iCoords = nif->getIndex( QModelIndex( sd.iUVSets ), clamped );
		if ( !iCoords.isValid() )
			continue;
		QVector<Vector2> fresh = nif->getArray<Vector2>( iCoords );
		if ( fresh.size() != sd.uvs.size() )
			continue;
		sd.iUVData = iCoords;
		sd.uvs = fresh;
	}
	update();
}

void UVEditorView::growSelectionToLinked()
{
	const UVShapeData * sd = active();
	if ( !sd || selVerts.isEmpty() )
		return;
	QSet<int> islands;
	for ( int v : std::as_const( selVerts ) )
		islands << sd->islandOfVert.at( v );
	for ( int i = 0; i < sd->uvs.size(); i++ )
		if ( islands.contains( sd->islandOfVert.at( i ) ) )
			selVerts << i;
	rebuildDerivedSelection();
	pushSelectionToViewport();
	notifySelectionInfo();
	update();
}

// ---------------------------------------------------------------------------
// view / cursor

void UVEditorView::frameAll()
{
	const UVShapeData * sd = active();
	if ( !sd || sd->uvs.isEmpty() ) {
		viewPos[0] = 0.0;
		viewPos[1] = 0.0;
		zoom = 1.2;
		updateViewMapping();
		update();
		return;
	}
	Vector2 mn = sd->uvs.first(), mx = sd->uvs.first();
	for ( const Vector2 & v : sd->uvs ) {
		mn[0] = std::min( mn[0], v[0] ); mn[1] = std::min( mn[1], v[1] );
		mx[0] = std::max( mx[0], v[0] ); mx[1] = std::max( mx[1], v[1] );
	}
	viewPos[0] = double( mn[0] + mx[0] ) * 0.5 - 0.5;
	viewPos[1] = double( mn[1] + mx[1] ) * 0.5 - 0.5;
	zoom = std::clamp( double( std::max( mx[0] - mn[0], mx[1] - mn[1] ) ) * 1.15 + 0.05, 0.02, 20.0 );
	updateViewMapping();
	update();
}

void UVEditorView::frameSelected()
{
	// frame the combined selection across every edit-session shape
	bool first = true;
	Vector2 mn, mx;
	for ( int s = 0; s < shapes.size(); s++ ) {
		const UVShapeData & osd = shapes.at( s );
		if ( !osd.valid )
			continue;
		const QSet<int> & sVerts = ( s == activeShape ) ? selVerts : osd.selVerts;
		for ( int v : sVerts ) {
			if ( v < 0 || v >= osd.uvs.size() )
				continue;
			const Vector2 & uv = osd.uvs.at( v );
			if ( first ) { mn = mx = uv; first = false; continue; }
			mn[0] = std::min( mn[0], uv[0] ); mn[1] = std::min( mn[1], uv[1] );
			mx[0] = std::max( mx[0], uv[0] ); mx[1] = std::max( mx[1], uv[1] );
		}
	}
	if ( first ) {
		frameAll();
		return;
	}
	viewPos[0] = double( mn[0] + mx[0] ) * 0.5 - 0.5;
	viewPos[1] = double( mn[1] + mx[1] ) * 0.5 - 0.5;
	zoom = std::clamp( double( std::max( mx[0] - mn[0], mx[1] - mn[1] ) ) * 1.3 + 0.02, 0.02, 20.0 );
	updateViewMapping();
	update();
}

void UVEditorView::placeCursor( const QPointF & widgetPos )
{
	cursor2D = mapToUV( widgetPos * devicePixelRatioF() );
	if ( cursorMovedCb )
		cursorMovedCb();
	update();
}

void UVEditorView::setCursorUV( const Vector2 & uv )
{
	cursor2D = uv;
	if ( cursorMovedCb )
		cursorMovedCb();
	update();
}

void UVEditorView::snapCursor( int mode )
{
	const UVShapeData * sd = active();
	Vector2 target = cursor2D;
	if ( mode == 0 && sd && !selVerts.isEmpty() ) {
		Vector2 sum;
		for ( int v : std::as_const( selVerts ) )
			sum += sd->uvs.at( v );
		target = sum / float( selVerts.size() );
	} else if ( mode == 1 && sd && activeVert >= 0 && activeVert < sd->uvs.size() ) {
		target = sd->uvs.at( activeVert );
	} else if ( mode == 2 && gridStep > 0.0f ) {
		target = Vector2( std::round( cursor2D[0] / gridStep ) * gridStep,
			std::round( cursor2D[1] / gridStep ) * gridStep );
	} else if ( mode == 3 ) {
		target = Vector2( 0.0f, 0.0f );
	} else if ( mode == 4 ) {
		target = Vector2( 0.5f, 0.5f );
	}
	setCursorUV( target );
}

void UVEditorView::snapSelectedToCursor( bool keepOffset )
{
	const UVShapeData * sd = active();
	if ( !sd || selVerts.isEmpty() )
		return;
	QHash<int, Vector2> edits;
	if ( keepOffset ) {
		Vector2 sum;
		for ( int v : std::as_const( selVerts ) )
			sum += sd->uvs.at( v );
		Vector2 delta = cursor2D - sum / float( selVerts.size() );
		for ( int v : std::as_const( selVerts ) )
			edits.insert( v, sd->uvs.at( v ) + delta );
	} else {
		for ( int v : std::as_const( selVerts ) )
			edits.insert( v, cursor2D );
	}
	applyUVEditUndoable( edits, tr( "Snap UVs to Cursor" ) );
}

void UVEditorView::snapSelectedToGrid()
{
	const UVShapeData * sd = active();
	if ( !sd || selVerts.isEmpty() || gridStep <= 0.0f )
		return;
	QHash<int, Vector2> edits;
	for ( int v : std::as_const( selVerts ) ) {
		const Vector2 & uv = sd->uvs.at( v );
		edits.insert( v, Vector2( std::round( uv[0] / gridStep ) * gridStep,
			std::round( uv[1] / gridStep ) * gridStep ) );
	}
	applyUVEditUndoable( edits, tr( "Snap UVs to Grid" ) );
}

Vector2 UVEditorView::snapToPixel( const Vector2 & uv ) const
{
	if ( textureRes[0] < 0.5f || textureRes[1] < 0.5f )
		return uv;
	// snap to the nearest texel corner (uv * res is integer at corners)
	return Vector2( std::round( uv[0] * textureRes[0] ) / textureRes[0],
		std::round( uv[1] * textureRes[1] ) / textureRes[1] );
}

void UVEditorView::snapSelectedToPixels()
{
	const UVShapeData * sd = active();
	if ( !sd || selVerts.isEmpty() )
		return;
	if ( textureRes[0] < 0.5f ) {
		setStatus( tr( "Pixel snap needs a texture underlay with a known resolution." ) );
		return;
	}
	QHash<int, Vector2> edits;
	for ( int v : std::as_const( selVerts ) )
		edits.insert( v, snapToPixel( sd->uvs.at( v ) ) );
	applyUVEditUndoable( edits, tr( "Snap UVs to Pixels" ) );
}

void UVEditorView::snapCursorToPixels()
{
	if ( textureRes[0] < 0.5f ) {
		setStatus( tr( "Pixel snap needs a texture underlay with a known resolution." ) );
		return;
	}
	setCursorUV( snapToPixel( cursor2D ) );
}

void UVEditorView::showSnapMenu( const QPoint & globalPos )
{
	const bool havePixels = ( textureRes[0] > 0.5f && textureRes[1] > 0.5f );
	const bool haveSel = !selVerts.isEmpty() && !objectModeView;
	UVAutoCloseMenu menu;
	menu.addSection( tr( "Snap" ) );
	QAction * selToCursor = menu.addAction( tr( "Selection to Cursor" ) );
	QAction * selToCursorOffset = menu.addAction( tr( "Selection to Cursor (Keep Offset)" ) );
	QAction * selToGrid = menu.addAction( tr( "Selection to Grid" ) );
	QAction * selToPixel = menu.addAction( havePixels
		? tr( "Selection to Pixel (%1×%2)" ).arg( int( textureRes[0] ) ).arg( int( textureRes[1] ) )
		: tr( "Selection to Pixel" ) );
	for ( QAction * a : { selToCursor, selToCursorOffset, selToGrid } )
		a->setEnabled( haveSel );
	selToPixel->setEnabled( havePixels && haveSel );
	menu.addSeparator();
	QAction * curToSel = menu.addAction( tr( "Cursor to Selected" ) );
	QAction * curToActive = menu.addAction( tr( "Cursor to Active" ) );
	QAction * curToGrid = menu.addAction( tr( "Cursor to Grid" ) );
	QAction * curToPixel = menu.addAction( tr( "Cursor to Pixel" ) );
	curToSel->setEnabled( !selVerts.isEmpty() );
	curToActive->setEnabled( activeVert >= 0 );
	curToPixel->setEnabled( havePixels );
	QAction * curToOrigin = menu.addAction( tr( "Cursor to Origin (0, 0)" ) );
	QAction * curToTile = menu.addAction( tr( "Cursor to Tile Center (0.5, 0.5)" ) );
	QAction * chosen = menu.exec( globalPos );
	if ( chosen == selToCursor ) snapSelectedToCursor( false );
	else if ( chosen == selToCursorOffset ) snapSelectedToCursor( true );
	else if ( chosen == selToGrid ) snapSelectedToGrid();
	else if ( chosen == selToPixel ) snapSelectedToPixels();
	else if ( chosen == curToSel ) snapCursor( 0 );
	else if ( chosen == curToActive ) snapCursor( 1 );
	else if ( chosen == curToGrid ) snapCursor( 2 );
	else if ( chosen == curToPixel ) snapCursorToPixels();
	else if ( chosen == curToOrigin ) snapCursor( 3 );
	else if ( chosen == curToTile ) snapCursor( 4 );
}

void UVEditorView::showUnwrapMenu( const QPoint & globalPos )
{
	UVAutoCloseMenu menu;
	menu.addSection( tr( "Unwrap" ) );
	QAction * unwrap = menu.addAction( tr( "Unwrap (Angle Based)…" ) );
	menu.addSeparator();
	// "…" marks operators that pop the adjust-last-operation panel afterwards
	// (same convention as the 3D viewport's "By Distance…")
	QAction * smart = menu.addAction( tr( "Smart UV Project…" ) );
	menu.addSeparator();
	QAction * project = menu.addAction( tr( "Project From View" ) );
	QAction * cube = menu.addAction( tr( "Cube Projection" ) );
	QAction * cyl = menu.addAction( tr( "Cylinder Projection" ) );
	QAction * sphere = menu.addAction( tr( "Sphere Projection" ) );
	QAction * chosen = menu.exec( globalPos );
	if ( chosen == unwrap ) unwrapSelection();
	else if ( chosen == smart ) smartProject( 66.0f );
	else if ( chosen == project ) projectFromView();
	else if ( chosen == cube ) projectShape( 0 );
	else if ( chosen == cyl ) projectShape( 1 );
	else if ( chosen == sphere ) projectShape( 2 );
}

void UVEditorView::unwrapSelection( float margin )
{
	if ( objectModeView ) {
		setStatus( tr( "Object Mode is read-only — switch to Edit Mode to unwrap." ) );
		return;
	}
	const UVShapeData * sd = active();
	if ( !sd || !nif )
		return;
	margin = std::clamp( margin, 0.0f, 0.25f );
	// target faces: face-mode selection, else every face whose corners are all
	// selected (this is what Blender's Unwrap operates on)
	QVector<Triangle> target;
	QList<int> targetIds;
	for ( int f = 0; f < sd->tris.size(); f++ ) {
		const Triangle & t = sd->tris.at( f );
		const bool inSel = ( selectMode == 3 )
			? selFaces.contains( f )
			: ( selVerts.contains( t[0] ) && selVerts.contains( t[1] ) && selVerts.contains( t[2] ) );
		if ( inSel && faceVisibleUV( f, t ) ) {
			target << t;
			targetIds << f;
		}
	}
	if ( target.isEmpty() ) {
		setStatus( tr( "Unwrap: select the faces to unwrap first." ) );
		return;
	}
	QVector<Vector3> pos;
	if ( !readShapePositions( nif, *sd, pos ) ) {
		setStatus( tr( "Unwrap: could not read vertex positions." ) );
		return;
	}

	// split the target faces into connected components (existing vertex splits
	// act as the seams — the per-vertex NIF UV model)
	QHash<int, int> compRoot;
	std::function<int(int)> findRoot = [&]( int a ) {
		int r = a;
		while ( compRoot.value( r, r ) != r )
			r = compRoot.value( r, r );
		return r;
	};
	for ( const Triangle & t : std::as_const( target ) ) {
		if ( !compRoot.contains( t[0] ) ) compRoot[t[0]] = t[0];
		if ( !compRoot.contains( t[1] ) ) compRoot[t[1]] = t[1];
		if ( !compRoot.contains( t[2] ) ) compRoot[t[2]] = t[2];
		int r0 = findRoot( t[0] );
		compRoot[findRoot( t[1] )] = r0;
		compRoot[findRoot( t[2] )] = findRoot( r0 );
	}
	QHash<int, QVector<Triangle>> componentTris;
	for ( const Triangle & t : std::as_const( target ) )
		componentTris[findRoot( t[0] )] << t;

	// solve each component, then scale it so texel density is uniform
	struct CompResult
	{
		QHash<int, Vector2> uv;
		Vector2 mn, mx;
	};
	QVector<CompResult> results;
	for ( auto it = componentTris.constBegin(); it != componentTris.constEnd(); ++it ) {
		CompResult res;
		if ( !lscmSolveComponent( pos, it.value(), res.uv ) || res.uv.isEmpty() )
			continue;
		double area3D = 0.0, areaUV = 0.0;
		for ( const Triangle & t : it.value() ) {
			Vector3 n3 = Vector3::crossproduct( pos.at( t[1] ) - pos.at( t[0] ),
				pos.at( t[2] ) - pos.at( t[0] ) );
			area3D += 0.5 * std::sqrt( double( Vector3::dotproduct( n3, n3 ) ) );
			const Vector2 & a = res.uv.value( t[0] );
			const Vector2 & b = res.uv.value( t[1] );
			const Vector2 & c = res.uv.value( t[2] );
			areaUV += 0.5 * std::fabs( double( ( b[0] - a[0] ) * ( c[1] - a[1] )
				- ( c[0] - a[0] ) * ( b[1] - a[1] ) ) );
		}
		const float densityScale = ( areaUV > 1.0e-12 )
			? float( std::sqrt( area3D / areaUV ) ) : 1.0f;
		bool first = true;
		for ( auto uvIt = res.uv.begin(); uvIt != res.uv.end(); ++uvIt ) {
			*uvIt = *uvIt * densityScale;
			if ( first ) { res.mn = res.mx = *uvIt; first = false; continue; }
			res.mn[0] = std::min( res.mn[0], (*uvIt)[0] ); res.mn[1] = std::min( res.mn[1], (*uvIt)[1] );
			res.mx[0] = std::max( res.mx[0], (*uvIt)[0] ); res.mx[1] = std::max( res.mx[1], (*uvIt)[1] );
		}
		results << res;
	}
	if ( results.isEmpty() ) {
		setStatus( tr( "Unwrap failed (degenerate geometry?)." ) );
		return;
	}

	// simple shelf pack of the component bboxes, then normalize into 0-1
	std::sort( results.begin(), results.end(), []( const CompResult & a, const CompResult & b ) {
		return ( a.mx[1] - a.mn[1] ) > ( b.mx[1] - b.mn[1] );
	} );
	float totalW = 0.0f, maxW = 0.0f;
	for ( const CompResult & res : std::as_const( results ) ) {
		totalW += ( res.mx[0] - res.mn[0] );
		maxW = std::max( maxW, res.mx[0] - res.mn[0] );
	}
	const float shelfWidth = std::max( maxW, std::sqrt( std::max( totalW, 1.0e-6f ) * totalW ) / 4.0f + maxW * 0.5f );
	const float gap = shelfWidth * std::max( margin, 0.005f );
	float penX = 0.0f, penY = 0.0f, shelfH = 0.0f;
	QHash<int, Vector2> placed;
	for ( CompResult & res : results ) {
		const float w = res.mx[0] - res.mn[0];
		const float h = res.mx[1] - res.mn[1];
		if ( penX > 0.0f && penX + w > shelfWidth ) {
			penX = 0.0f;
			penY += shelfH + gap;
			shelfH = 0.0f;
		}
		for ( auto uvIt = res.uv.constBegin(); uvIt != res.uv.constEnd(); ++uvIt )
			placed.insert( uvIt.key(),
				Vector2( uvIt.value()[0] - res.mn[0] + penX, uvIt.value()[1] - res.mn[1] + penY ) );
		penX += w + gap;
		shelfH = std::max( shelfH, h );
	}
	// normalize the packed layout into the 0-1 tile with a small margin
	Vector2 mn, mx;
	bool first = true;
	for ( auto it2 = placed.constBegin(); it2 != placed.constEnd(); ++it2 ) {
		if ( first ) { mn = mx = it2.value(); first = false; continue; }
		mn[0] = std::min( mn[0], it2.value()[0] ); mn[1] = std::min( mn[1], it2.value()[1] );
		mx[0] = std::max( mx[0], it2.value()[0] ); mx[1] = std::max( mx[1], it2.value()[1] );
	}
	const float extent = std::max( std::max( mx[0] - mn[0], mx[1] - mn[1] ), 1.0e-6f );
	const float scale = ( 1.0f - 2.0f * margin ) / extent;
	QHash<int, Vector2> edits;
	for ( auto it2 = placed.constBegin(); it2 != placed.constEnd(); ++it2 )
		edits.insert( it2.key(), Vector2(
			( it2.value()[0] - mn[0] ) * scale + margin,
			( it2.value()[1] - mn[1] ) * scale + margin ) );

	const bool pushed = applyUVEditUndoable( edits, tr( "Unwrap UVs" ) );
	setStatus( tr( "Unwrapped %1 faces in %2 island(s)." )
		.arg( target.size() ).arg( results.size() ) );
	pushSelectionToViewport();
	if ( pushed )
		armOperatorPanel( 5, { margin } );
}

void UVEditorView::projectFromView()
{
	if ( objectModeView ) {
		setStatus( tr( "Object Mode is read-only — switch to Edit Mode to project." ) );
		return;
	}
	const UVShapeData * sd = active();
	if ( !sd || !nif || !ogl )
		return;
	QSet<int> verts = selVerts;
	if ( verts.isEmpty() ) {
		setStatus( tr( "Project From View: select geometry first." ) );
		return;
	}
	QVector<Vector3> pos;
	if ( !readShapePositions( nif, *sd, pos ) ) {
		setStatus( tr( "Project From View: could not read vertex positions." ) );
		return;
	}
	// Project along the current 3D camera. Positions are object-space; for
	// shapes with a non-identity world transform the projection direction is
	// still the camera's, only the in-plane offset differs.
	Transform viewT = ogl->viewTransform();
	QHash<int, Vector2> projected;
	Vector2 mn, mx;
	bool first = true;
	for ( int v : std::as_const( verts ) ) {
		if ( v < 0 || v >= pos.size() )
			continue;
		Vector3 p = viewT * pos.at( v );
		Vector2 uv( p[0], -p[1] );   // view Y up -> UV v down
		projected.insert( v, uv );
		if ( first ) { mn = mx = uv; first = false; continue; }
		mn[0] = std::min( mn[0], uv[0] ); mn[1] = std::min( mn[1], uv[1] );
		mx[0] = std::max( mx[0], uv[0] ); mx[1] = std::max( mx[1], uv[1] );
	}
	if ( projected.isEmpty() )
		return;
	const float extent = std::max( std::max( mx[0] - mn[0], mx[1] - mn[1] ), 1.0e-6f );
	const float scale = 0.96f / extent;
	QHash<int, Vector2> edits;
	for ( auto it = projected.constBegin(); it != projected.constEnd(); ++it )
		edits.insert( it.key(), Vector2(
			( it.value()[0] - mn[0] ) * scale + 0.02f,
			( it.value()[1] - mn[1] ) * scale + 0.02f ) );
	applyUVEditUndoable( edits, tr( "Project UVs From View" ) );
	setStatus( tr( "Projected %1 UVs from the current view." ).arg( edits.size() ) );
}

void UVEditorView::projectShape( int kind )
{
	if ( objectModeView ) {
		setStatus( tr( "Object Mode is read-only — switch to Edit Mode to project." ) );
		return;
	}
	const UVShapeData * sd = active();
	if ( !sd || !nif )
		return;
	QSet<int> verts = selVerts;
	if ( verts.isEmpty() ) {
		setStatus( tr( "Projection: select geometry first." ) );
		return;
	}
	QVector<Vector3> pos;
	if ( !readShapePositions( nif, *sd, pos ) ) {
		setStatus( tr( "Projection: could not read vertex positions." ) );
		return;
	}
	// object-space bounds/centre of the selected verts
	Vector3 mn, mx;
	bool first = true;
	for ( int v : std::as_const( verts ) ) {
		if ( v < 0 || v >= pos.size() )
			continue;
		const Vector3 & p = pos.at( v );
		if ( first ) { mn = mx = p; first = false; continue; }
		for ( int a = 0; a < 3; a++ ) {
			mn[a] = std::min( mn[a], p[a] );
			mx[a] = std::max( mx[a], p[a] );
		}
	}
	if ( first )
		return;
	const Vector3 centre = ( mn + mx ) * 0.5f;
	const Vector3 ext = mx - mn;
	const float invW = ext[0] > 1.0e-6f ? 1.0f / ext[0] : 1.0f;
	const float invD = ext[2] > 1.0e-6f ? 1.0f / ext[2] : 1.0f;
	const float invH = ext[1] > 1.0e-6f ? 1.0f / ext[1] : 1.0f;
	const float twoPi = 6.28318530718f;

	QHash<int, Vector2> edits;
	for ( int v : std::as_const( verts ) ) {
		if ( v < 0 || v >= pos.size() )
			continue;
		Vector3 r = pos.at( v ) - centre;
		Vector2 uv;
		if ( kind == 2 ) {
			// sphere: longitude / latitude
			float lon = std::atan2( r[0], r[2] ) / twoPi + 0.5f;
			float len = std::sqrt( r[0] * r[0] + r[1] * r[1] + r[2] * r[2] );
			float lat = len > 1.0e-6f ? ( std::asin( std::clamp( r[1] / len, -1.0f, 1.0f ) ) / 3.14159265f + 0.5f ) : 0.5f;
			uv = Vector2( lon, lat );
		} else if ( kind == 1 ) {
			// cylinder: angle around Z-up axis / height
			float lon = std::atan2( r[0], r[2] ) / twoPi + 0.5f;
			uv = Vector2( lon, ( pos.at( v )[1] - mn[1] ) * invH );
		} else {
			// cube: project onto the plane of the dominant position-relative axis
			float ax = std::fabs( r[0] ), ay = std::fabs( r[1] ), az = std::fabs( r[2] );
			if ( ax >= ay && ax >= az )
				uv = Vector2( ( pos.at( v )[2] - mn[2] ) * invD, ( pos.at( v )[1] - mn[1] ) * invH );
			else if ( az >= ax && az >= ay )
				uv = Vector2( ( pos.at( v )[0] - mn[0] ) * invW, ( pos.at( v )[1] - mn[1] ) * invH );
			else
				uv = Vector2( ( pos.at( v )[0] - mn[0] ) * invW, ( pos.at( v )[2] - mn[2] ) * invD );
		}
		edits.insert( v, uv );
	}
	const QString name = kind == 2 ? tr( "Sphere Project UVs" )
		: kind == 1 ? tr( "Cylinder Project UVs" ) : tr( "Cube Project UVs" );
	applyUVEditUndoable( edits, name );
	setStatus( tr( "Projected %1 UVs." ).arg( edits.size() ) );
}

// ---------------------------------------------------------------------------
// Phase 2 operators: merge, mirror/align, round-to-pixels, hide/show faces

void UVEditorView::recomputeHiddenVerts()
{
	hiddenVerts.clear();
	const UVShapeData * sd = active();
	if ( !sd || hiddenFaces.isEmpty() )
		return;
	QHash<int, int> total, hidden;
	for ( int f = 0; f < sd->tris.size(); f++ ) {
		const Triangle & t = sd->tris.at( f );
		const bool h = hiddenFaces.contains( f );
		for ( int c = 0; c < 3; c++ ) {
			total[t[c]]++;
			if ( h ) hidden[t[c]]++;
		}
	}
	for ( auto it = total.constBegin(); it != total.constEnd(); ++it )
		if ( hidden.value( it.key(), 0 ) == it.value() )
			hiddenVerts << it.key();
}

void UVEditorView::mergeSelection( int mode, float dist )
{
	const UVShapeData * sd = active();
	if ( !sd || selVerts.isEmpty() )
		return;
	float threshold = 0.0f;
	QHash<int, Vector2> edits;
	if ( mode == 1 ) {
		// at cursor
		for ( int v : std::as_const( selVerts ) )
			edits.insert( v, cursor2D );
	} else if ( mode == 2 ) {
		// by distance: weld verts within a threshold to their group average
		threshold = ( dist >= 0.0f ) ? dist : ( gridStep > 0.0f ? gridStep * 0.5f : 0.01f );
		QList<int> verts = selVerts.values();
		QVector<int> parent( verts.size() );
		for ( int i = 0; i < verts.size(); i++ ) parent[i] = i;
		std::function<int(int)> root = [&]( int a ) {
			while ( parent[a] != a ) { parent[a] = parent[parent[a]]; a = parent[a]; }
			return a;
		};
		for ( int i = 0; i < verts.size(); i++ )
			for ( int j = i + 1; j < verts.size(); j++ ) {
				Vector2 d = sd->uvs.at( verts[i] ) - sd->uvs.at( verts[j] );
				if ( std::fabs( d[0] ) <= threshold && std::fabs( d[1] ) <= threshold )
					parent[root( j )] = root( i );
			}
		QHash<int, Vector2> sum;
		QHash<int, int> count;
		for ( int i = 0; i < verts.size(); i++ ) {
			int r = root( i );
			sum[r] += sd->uvs.at( verts[i] );
			count[r]++;
		}
		for ( int i = 0; i < verts.size(); i++ ) {
			int r = root( i );
			edits.insert( verts[i], sum[r] / float( count[r] ) );
		}
	} else {
		// at center: single average
		Vector2 avg;
		for ( int v : std::as_const( selVerts ) )
			avg += sd->uvs.at( v );
		avg = avg / float( selVerts.size() );
		for ( int v : std::as_const( selVerts ) )
			edits.insert( v, avg );
	}
	const bool pushed = applyUVEditUndoable( edits, tr( "Merge UVs" ) );
	setStatus( tr( "Merged %1 UVs." ).arg( edits.size() ) );
	if ( mode == 2 && pushed )
		armOperatorPanel( 1, { threshold } );
}

void UVEditorView::showMergeMenu( const QPoint & globalPos )
{
	if ( objectModeView || selVerts.isEmpty() ) {
		setStatus( tr( "Merge: select UVs first (Edit Mode)." ) );
		return;
	}
	UVAutoCloseMenu menu;
	menu.addSection( tr( "Merge" ) );
	QAction * atCenter = menu.addAction( tr( "At Center" ) );
	QAction * atCursor = menu.addAction( tr( "At Cursor" ) );
	menu.addSeparator();
	QAction * byDist = menu.addAction( tr( "By Distance…" ) );
	QAction * chosen = menu.exec( globalPos );
	if ( chosen == atCenter ) mergeSelection( 0 );
	else if ( chosen == atCursor ) mergeSelection( 1 );
	else if ( chosen == byDist ) mergeSelection( 2 );
}

void UVEditorView::mirrorSelection( int axis )
{
	const UVShapeData * sd = active();
	if ( !sd || selVerts.isEmpty() )
		return;
	// pivot from the pivot selector
	Vector2 pivot;
	if ( pivotMode == 2 ) {
		pivot = cursor2D;
	} else {
		Vector2 mn = sd->uvs.at( *selVerts.constBegin() ), mx = mn, sum;
		for ( int v : std::as_const( selVerts ) ) {
			const Vector2 & uv = sd->uvs.at( v );
			mn[0] = std::min( mn[0], uv[0] ); mn[1] = std::min( mn[1], uv[1] );
			mx[0] = std::max( mx[0], uv[0] ); mx[1] = std::max( mx[1], uv[1] );
			sum += uv;
		}
		pivot = ( pivotMode == 1 ) ? sum / float( selVerts.size() ) : ( mn + mx ) * 0.5f;
	}
	QHash<int, Vector2> edits;
	for ( int v : std::as_const( selVerts ) ) {
		Vector2 uv = sd->uvs.at( v );
		if ( axis == 1 ) uv[0] = 2.0f * pivot[0] - uv[0];
		else uv[1] = 2.0f * pivot[1] - uv[1];
		edits.insert( v, uv );
	}
	applyUVEditUndoable( edits, axis == 1 ? tr( "Mirror UVs U" ) : tr( "Mirror UVs V" ) );
	setStatus( axis == 1 ? tr( "Mirrored along U." ) : tr( "Mirrored along V." ) );
}

void UVEditorView::alignSelection( int mode )
{
	const UVShapeData * sd = active();
	if ( !sd || selVerts.isEmpty() )
		return;
	Vector2 mn = sd->uvs.at( *selVerts.constBegin() ), mx = mn, sum;
	for ( int v : std::as_const( selVerts ) ) {
		const Vector2 & uv = sd->uvs.at( v );
		mn[0] = std::min( mn[0], uv[0] ); mn[1] = std::min( mn[1], uv[1] );
		mx[0] = std::max( mx[0], uv[0] ); mx[1] = std::max( mx[1], uv[1] );
		sum += uv;
	}
	Vector2 avg = sum / float( selVerts.size() );
	QHash<int, Vector2> edits;
	for ( int v : std::as_const( selVerts ) ) {
		Vector2 uv = sd->uvs.at( v );
		if ( mode == 0 ) uv[0] = avg[0];        // align U (collapse to mean U)
		else if ( mode == 1 ) uv[1] = avg[1];   // align V
		else if ( mode == 2 ) uv[0] = mn[0];    // straighten to min U (left)
		else uv[1] = mn[1];                      // straighten to min V (bottom)
		edits.insert( v, uv );
	}
	static const char * names[4] = { "Align UVs U", "Align UVs V", "Straighten UVs Left", "Straighten UVs Bottom" };
	applyUVEditUndoable( edits, tr( names[std::clamp( mode, 0, 3 )] ) );
}

void UVEditorView::showTransformMenu( const QPoint & globalPos )
{
	if ( objectModeView || selVerts.isEmpty() ) {
		setStatus( tr( "Select UVs first (Edit Mode)." ) );
		return;
	}
	UVAutoCloseMenu menu;
	menu.addSection( tr( "Mirror / Align" ) );
	QAction * mirU = menu.addAction( tr( "Mirror U (horizontal)" ) );
	QAction * mirV = menu.addAction( tr( "Mirror V (vertical)" ) );
	menu.addSeparator();
	QAction * alignU = menu.addAction( tr( "Align U (same column)" ) );
	QAction * alignV = menu.addAction( tr( "Align V (same row)" ) );
	QAction * straightU = menu.addAction( tr( "Straighten to Left" ) );
	QAction * straightV = menu.addAction( tr( "Straighten to Bottom" ) );
	QAction * chosen = menu.exec( globalPos );
	if ( chosen == mirU ) mirrorSelection( 1 );
	else if ( chosen == mirV ) mirrorSelection( 2 );
	else if ( chosen == alignU ) alignSelection( 0 );
	else if ( chosen == alignV ) alignSelection( 1 );
	else if ( chosen == straightU ) alignSelection( 2 );
	else if ( chosen == straightV ) alignSelection( 3 );
}

void UVEditorView::roundSelectionToPixels()
{
	snapSelectedToPixels();
}

void UVEditorView::hideSelectedFaces()
{
	const UVShapeData * sd = active();
	if ( !sd || objectModeView )
		return;
	int before = hiddenFaces.size();
	for ( int f = 0; f < sd->tris.size(); f++ ) {
		const Triangle & t = sd->tris.at( f );
		if ( selVerts.contains( t[0] ) && selVerts.contains( t[1] ) && selVerts.contains( t[2] ) )
			hiddenFaces << f;
	}
	if ( hiddenFaces.size() == before ) {
		setStatus( tr( "Hide Faces: select faces (all three corners) first." ) );
		return;
	}
	recomputeHiddenVerts();
	// drop now-hidden verts from the selection
	QSet<int> keep;
	for ( int v : std::as_const( selVerts ) )
		if ( !hiddenVerts.contains( v ) )
			keep << v;
	selVerts = keep;
	rebuildDerivedSelection();
	pushSelectionToViewport();
	notifySelectionInfo();
	setStatus( tr( "Hid %1 faces (Alt+H to reveal)." ).arg( hiddenFaces.size() - before ) );
	update();
}

void UVEditorView::unhideAllFaces()
{
	if ( hiddenFaces.isEmpty() )
		return;
	hiddenFaces.clear();
	hiddenVerts.clear();
	setStatus( tr( "Revealed all UV faces." ) );
	update();
}

// ---------------------------------------------------------------------------
// Phase 3 layout tools

//! Signed area of a 2D polygon (CCW positive).
static double uvPolyArea( const QVector<Vector2> & p )
{
	double a = 0.0;
	const int n = p.size();
	for ( int i = 0; i < n; i++ ) {
		const Vector2 & u = p.at( i );
		const Vector2 & v = p.at( ( i + 1 ) % n );
		a += double( u[0] ) * v[1] - double( v[0] ) * u[1];
	}
	return 0.5 * a;
}

//! Area of the intersection of two triangles (Sutherland-Hodgman clip). Used to
//! flag genuinely overlapping UV faces without tripping on edge-adjacent ones.
static double uvTriIntersectArea( const Vector2 A[3], const Vector2 B[3] )
{
	QVector<Vector2> clip = { B[0], B[1], B[2] };
	if ( uvPolyArea( clip ) < 0.0 )
		std::reverse( clip.begin(), clip.end() );          // clipper must be CCW
	QVector<Vector2> out = { A[0], A[1], A[2] };
	const int cn = clip.size();
	for ( int e = 0; e < cn && !out.isEmpty(); e++ ) {
		const Vector2 c0 = clip.at( e );
		const Vector2 c1 = clip.at( ( e + 1 ) % cn );
		const Vector2 edge = c1 - c0;
		auto inside = [&]( const Vector2 & pt ) {
			return ( edge[0] * ( pt[1] - c0[1] ) - edge[1] * ( pt[0] - c0[0] ) ) >= 0.0f;
		};
		auto isect = [&]( const Vector2 & p0, const Vector2 & p1 ) {
			const Vector2 d = p1 - p0;
			const double denom = double( edge[0] ) * d[1] - double( edge[1] ) * d[0];
			if ( std::fabs( denom ) < 1.0e-20 )
				return p1;
			const double s = ( double( edge[0] ) * ( p0[1] - c0[1] )
				- double( edge[1] ) * ( p0[0] - c0[0] ) ) / -denom;
			return Vector2( p0[0] + float( s ) * d[0], p0[1] + float( s ) * d[1] );
		};
		QVector<Vector2> input = out;
		out.clear();
		const int m = input.size();
		for ( int i = 0; i < m; i++ ) {
			const Vector2 cur = input.at( i );
			const Vector2 prev = input.at( ( i + m - 1 ) % m );
			const bool curIn = inside( cur ), prevIn = inside( prev );
			if ( curIn ) {
				if ( !prevIn )
					out << isect( prev, cur );
				out << cur;
			} else if ( prevIn ) {
				out << isect( prev, cur );
			}
		}
	}
	return std::fabs( uvPolyArea( out ) );
}

QSet<int> UVEditorView::targetIslands() const
{
	QSet<int> isl;
	const UVShapeData * sd = active();
	if ( !sd )
		return isl;
	if ( selVerts.isEmpty() ) {
		for ( int i = 0; i < sd->islandCount; i++ )
			isl << i;
	} else {
		for ( int v : std::as_const( selVerts ) )
			if ( v >= 0 && v < sd->islandOfVert.size() )
				isl << sd->islandOfVert.at( v );
	}
	return isl;
}

void UVEditorView::averageIslandsScale()
{
	UVShapeData * sd = active();
	if ( !sd || objectModeView ) {
		setStatus( tr( "Average Islands Scale: switch to Edit Mode." ) );
		return;
	}
	QVector<Vector3> pos;
	if ( !readShapePositions( nif, *sd, pos ) ) {
		setStatus( tr( "Average Islands Scale: could not read vertex positions." ) );
		return;
	}
	const QSet<int> isl = targetIslands();
	if ( isl.size() < 2 ) {
		setStatus( tr( "Average Islands Scale: needs at least two islands." ) );
		return;
	}
	QHash<int, double> area3D, areaUV;
	QHash<int, Vector2> centSum;
	QHash<int, int> centCount;
	for ( int f = 0; f < sd->tris.size(); f++ ) {
		const Triangle & t = sd->tris.at( f );
		const int island = sd->islandOfVert.at( t[0] );
		if ( !isl.contains( island ) )
			continue;
		Vector3 n3 = Vector3::crossproduct( pos.at( t[1] ) - pos.at( t[0] ),
			pos.at( t[2] ) - pos.at( t[0] ) );
		area3D[island] += 0.5 * std::sqrt( double( Vector3::dotproduct( n3, n3 ) ) );
		const Vector2 & a = sd->uvs.at( t[0] );
		const Vector2 & b = sd->uvs.at( t[1] );
		const Vector2 & c = sd->uvs.at( t[2] );
		areaUV[island] += 0.5 * std::fabs( double( ( b[0] - a[0] ) * ( c[1] - a[1] )
			- ( c[0] - a[0] ) * ( b[1] - a[1] ) ) );
	}
	for ( int v = 0; v < sd->uvs.size(); v++ ) {
		const int island = sd->islandOfVert.at( v );
		if ( !isl.contains( island ) )
			continue;
		centSum[island] += sd->uvs.at( v );
		centCount[island]++;
	}
	QHash<int, double> dens;
	double sumD = 0.0;
	int nD = 0;
	for ( int island : std::as_const( isl ) ) {
		const double a3 = area3D.value( island, 0.0 );
		const double au = areaUV.value( island, 0.0 );
		if ( a3 < 1.0e-12 || au < 1.0e-14 )
			continue;
		const double d = std::sqrt( au / a3 );        // uv units per world unit
		dens[island] = d;
		sumD += d;
		nD++;
	}
	if ( nD < 2 ) {
		setStatus( tr( "Average Islands Scale: degenerate islands." ) );
		return;
	}
	const double meanD = sumD / nD;
	QHash<int, Vector2> edits;
	for ( int v = 0; v < sd->uvs.size(); v++ ) {
		const int island = sd->islandOfVert.at( v );
		if ( !dens.contains( island ) )
			continue;
		const float s = float( meanD / dens.value( island ) );
		const Vector2 cent = centSum.value( island ) / float( std::max( 1, centCount.value( island ) ) );
		edits.insert( v, cent + ( sd->uvs.at( v ) - cent ) * s );
	}
	applyUVEditUndoable( edits, tr( "Average Islands Scale" ) );
	setStatus( tr( "Equalised texel density across %1 islands." ).arg( nD ) );
}

void UVEditorView::packIslands( float margin )
{
	UVShapeData * sd = active();
	if ( !sd || objectModeView ) {
		setStatus( tr( "Pack Islands: switch to Edit Mode." ) );
		return;
	}
	margin = std::clamp( margin, 0.0f, 0.25f );
	const QSet<int> isl = targetIslands();
	if ( isl.isEmpty() )
		return;
	QHash<int, Vector2> islMn, islMx;
	for ( int v = 0; v < sd->uvs.size(); v++ ) {
		const int island = sd->islandOfVert.at( v );
		if ( !isl.contains( island ) )
			continue;
		const Vector2 & uv = sd->uvs.at( v );
		auto itMn = islMn.find( island );
		if ( itMn == islMn.end() ) {
			islMn.insert( island, uv );
			islMx.insert( island, uv );
		} else {
			Vector2 & mn = itMn.value();
			Vector2 & mx = islMx[island];
			mn[0] = std::min( mn[0], uv[0] );
			mn[1] = std::min( mn[1], uv[1] );
			mx[0] = std::max( mx[0], uv[0] );
			mx[1] = std::max( mx[1], uv[1] );
		}
	}
	QVector<int> ids = islMn.keys();
	if ( ids.isEmpty() )
		return;
	std::sort( ids.begin(), ids.end(), [&]( int a, int b ) {
		return ( islMx[a][1] - islMn[a][1] ) > ( islMx[b][1] - islMn[b][1] );
	} );
	double totalArea = 0.0;
	float widest = 0.0f;
	for ( int id : std::as_const( ids ) ) {
		const float w = islMx[id][0] - islMn[id][0] + margin;
		const float h = islMx[id][1] - islMn[id][1] + margin;
		totalArea += double( w ) * h;
		widest = std::max( widest, w );
	}
	float shelfW = std::max( widest, float( std::sqrt( std::max( totalArea, 1.0e-9 ) ) ) );
	QHash<int, Vector2> offsetOf;
	float penX = 0.0f, penY = 0.0f, shelfH = 0.0f, usedW = 0.0f, usedH = 0.0f;
	for ( int id : std::as_const( ids ) ) {
		const float w = islMx[id][0] - islMn[id][0] + margin;
		const float h = islMx[id][1] - islMn[id][1] + margin;
		if ( penX > 0.0f && penX + w > shelfW ) {
			penX = 0.0f;
			penY += shelfH;
			shelfH = 0.0f;
		}
		offsetOf.insert( id, Vector2( penX + margin * 0.5f, penY + margin * 0.5f ) - islMn[id] );
		penX += w;
		shelfH = std::max( shelfH, h );
		usedW = std::max( usedW, penX );
		usedH = std::max( usedH, penY + shelfH );
	}
	const float scale = 1.0f / std::max( { usedW, usedH, 1.0e-3f } );
	QHash<int, Vector2> edits;
	for ( int v = 0; v < sd->uvs.size(); v++ ) {
		const int island = sd->islandOfVert.at( v );
		auto it = offsetOf.constFind( island );
		if ( it == offsetOf.constEnd() )
			continue;
		edits.insert( v, ( sd->uvs.at( v ) + it.value() ) * scale );
	}
	const bool pushed = applyUVEditUndoable( edits, tr( "Pack Islands" ) );
	setStatus( tr( "Packed %1 islands into the 0-1 tile." ).arg( ids.size() ) );
	if ( pushed )
		armOperatorPanel( 3, { margin } );
}

void UVEditorView::minimizeStretch( int iters )
{
	UVShapeData * sd = active();
	if ( !sd || objectModeView || selVerts.isEmpty() ) {
		setStatus( tr( "Minimize Stretch: select a face region first." ) );
		return;
	}
	QVector<int> faceList;
	for ( int f = 0; f < sd->tris.size(); f++ ) {
		const Triangle & t = sd->tris.at( f );
		if ( selVerts.contains( t[0] ) && selVerts.contains( t[1] ) && selVerts.contains( t[2] )
			&& faceVisibleUV( f, t ) )
			faceList << f;
	}
	if ( faceList.isEmpty() ) {
		setStatus( tr( "Minimize Stretch: select whole faces (all three corners)." ) );
		return;
	}
	auto ekey = []( int a, int b ) {
		if ( a > b ) std::swap( a, b );
		return ( quint64( uint( a ) ) << 32 ) | uint( b );
	};
	QHash<int, QSet<int>> nbr;
	QHash<quint64, int> edgeCount;
	for ( int f : std::as_const( faceList ) ) {
		const Triangle & t = sd->tris.at( f );
		nbr[t[0]] << t[1] << t[2];
		nbr[t[1]] << t[0] << t[2];
		nbr[t[2]] << t[0] << t[1];
		edgeCount[ekey( t[0], t[1] )]++;
		edgeCount[ekey( t[1], t[2] )]++;
		edgeCount[ekey( t[2], t[0] )]++;
	}
	QSet<int> boundary;
	for ( auto it = edgeCount.constBegin(); it != edgeCount.constEnd(); ++it )
		if ( it.value() == 1 )
			boundary << int( it.key() >> 32 ) << int( it.key() & 0xFFFFFFFFu );
	QHash<int, Vector2> work;
	for ( auto it = nbr.constBegin(); it != nbr.constEnd(); ++it )
		work.insert( it.key(), sd->uvs.at( it.key() ) );
	iters = std::clamp( iters, 1, 200 );
	for ( int pass = 0; pass < iters; pass++ ) {
		QHash<int, Vector2> next = work;
		for ( auto it = nbr.constBegin(); it != nbr.constEnd(); ++it ) {
			const int v = it.key();
			if ( boundary.contains( v ) || !selVerts.contains( v ) || it.value().isEmpty() )
				continue;
			Vector2 sum;
			for ( int m : it.value() )
				sum += work.value( m );
			next[v] = sum / float( it.value().size() );
		}
		work.swap( next );
	}
	QHash<int, Vector2> edits;
	for ( auto it = work.constBegin(); it != work.constEnd(); ++it )
		if ( selVerts.contains( it.key() ) && !boundary.contains( it.key() ) )
			edits.insert( it.key(), it.value() );
	if ( edits.isEmpty() ) {
		setStatus( tr( "Minimize Stretch: nothing interior to relax (all on the border)." ) );
		return;
	}
	const bool pushed = applyUVEditUndoable( edits, tr( "Minimize Stretch" ) );
	setStatus( tr( "Relaxed %1 UVs over %2 iterations." ).arg( edits.size() ).arg( iters ) );
	if ( pushed )
		armOperatorPanel( 2, { float( iters ) } );
}

void UVEditorView::selectOverlappingUVs()
{
	UVShapeData * sd = active();
	if ( !sd || objectModeView )
		return;
	struct FB { int f; Vector2 mn, mx; };
	QVector<FB> faces;
	for ( int f = 0; f < sd->tris.size(); f++ ) {
		const Triangle & t = sd->tris.at( f );
		if ( !faceVisibleUV( f, t ) )
			continue;
		const Vector2 a = sd->uvs.at( t[0] ), b = sd->uvs.at( t[1] ), c = sd->uvs.at( t[2] );
		faces << FB{ f,
			Vector2( std::min( { a[0], b[0], c[0] } ), std::min( { a[1], b[1], c[1] } ) ),
			Vector2( std::max( { a[0], b[0], c[0] } ), std::max( { a[1], b[1], c[1] } ) ) };
	}
	std::sort( faces.begin(), faces.end(), []( const FB & a, const FB & b ) { return a.mn[0] < b.mn[0]; } );
	QSet<int> hitFaces;
	for ( int i = 0; i < faces.size(); i++ ) {
		const FB & A = faces.at( i );
		const Triangle & ta = sd->tris.at( A.f );
		const Vector2 A3[3] = { sd->uvs.at( ta[0] ), sd->uvs.at( ta[1] ), sd->uvs.at( ta[2] ) };
		const double areaA = std::fabs( uvPolyArea( { A3[0], A3[1], A3[2] } ) );
		for ( int j = i + 1; j < faces.size(); j++ ) {
			const FB & B = faces.at( j );
			if ( B.mn[0] > A.mx[0] )
				break;                                    // sweep prune on X
			if ( A.mx[1] < B.mn[1] || B.mx[1] < A.mn[1] )
				continue;
			const Triangle & tb = sd->tris.at( B.f );
			int shared = 0;
			for ( int x = 0; x < 3; x++ )
				for ( int y = 0; y < 3; y++ )
					if ( ta[x] == tb[y] ) shared++;
			if ( shared >= 2 )
				continue;                                 // edge-adjacent: not an overlap
			const Vector2 B3[3] = { sd->uvs.at( tb[0] ), sd->uvs.at( tb[1] ), sd->uvs.at( tb[2] ) };
			const double areaB = std::fabs( uvPolyArea( { B3[0], B3[1], B3[2] } ) );
			const double ia = uvTriIntersectArea( A3, B3 );
			if ( ia > 1.0e-7 * std::max( areaA, areaB ) && ia > 1.0e-12 ) {
				hitFaces << A.f << B.f;
			}
		}
	}
	if ( hitFaces.isEmpty() ) {
		setStatus( tr( "No overlapping UV faces found." ) );
		return;
	}
	selVerts.clear();
	for ( int f : std::as_const( hitFaces ) ) {
		const Triangle & t = sd->tris.at( f );
		selVerts << t[0] << t[1] << t[2];
	}
	activeVert = -1;
	rebuildDerivedSelection();
	pushSelectionToViewport();
	notifySelectionInfo();
	setStatus( tr( "Selected %1 overlapping faces." ).arg( hitFaces.size() ) );
	update();
}

void UVEditorView::stitchSelection()
{
	UVShapeData * sd = active();
	if ( !sd || objectModeView || selVerts.isEmpty() ) {
		setStatus( tr( "Stitch: select the seam UVs first." ) );
		return;
	}
	QVector<Vector3> pos;
	if ( !readShapePositions( nif, *sd, pos ) )
		return;
	auto poskey = []( const Vector3 & p ) -> quint64 {
		auto q = []( float f ) { return quint64( qRound64( double( f ) * 256.0 ) ) & 0x1FFFFFull; };
		return ( q( p[0] ) << 42 ) ^ ( q( p[1] ) << 21 ) ^ q( p[2] );
	};
	QHash<quint64, QVector<int>> buckets;
	for ( int v : std::as_const( selVerts ) )
		if ( v >= 0 && v < pos.size() )
			buckets[poskey( pos.at( v ) )] << v;
	QHash<int, Vector2> edits;
	for ( auto it = buckets.constBegin(); it != buckets.constEnd(); ++it ) {
		const QVector<int> & grp = it.value();
		QVector<bool> used( grp.size(), false );
		for ( int a = 0; a < grp.size(); a++ ) {
			if ( used[a] )
				continue;
			QVector<int> same;
			same << grp[a];
			used[a] = true;
			const Vector3 & pa = pos.at( grp[a] );
			for ( int b = a + 1; b < grp.size(); b++ ) {
				const Vector3 & pb = pos.at( grp[b] );
				if ( !used[b] && pa[0] == pb[0] && pa[1] == pb[1] && pa[2] == pb[2] ) {
					same << grp[b];
					used[b] = true;
				}
			}
			if ( same.size() < 2 )
				continue;
			Vector2 avg;
			for ( int v : std::as_const( same ) )
				avg += sd->uvs.at( v );
			avg = avg / float( same.size() );
			for ( int v : std::as_const( same ) )
				edits.insert( v, avg );
		}
	}
	if ( edits.isEmpty() ) {
		setStatus( tr( "Stitch: no coincident UV seams in the selection." ) );
		return;
	}
	applyUVEditUndoable( edits, tr( "Stitch UVs" ) );
	setStatus( tr( "Stitched %1 UVs across seams." ).arg( edits.size() ) );
}

void UVEditorView::copySelectedUVs()
{
	const UVShapeData * sd = active();
	if ( !sd || selVerts.isEmpty() ) {
		setStatus( tr( "Copy UVs: select UVs first." ) );
		return;
	}
	copiedUVs.clear();
	for ( int v : std::as_const( selVerts ) )
		if ( v >= 0 && v < sd->uvs.size() )
			copiedUVs.insert( v, sd->uvs.at( v ) );
	setStatus( tr( "Copied %1 UVs (paste onto identical-topology meshes)." ).arg( copiedUVs.size() ) );
}

void UVEditorView::pasteCopiedUVs()
{
	UVShapeData * sd = active();
	if ( !sd || objectModeView )
		return;
	if ( copiedUVs.isEmpty() ) {
		setStatus( tr( "Paste UVs: nothing copied yet." ) );
		return;
	}
	QHash<int, Vector2> edits;
	for ( auto it = copiedUVs.constBegin(); it != copiedUVs.constEnd(); ++it )
		if ( it.key() >= 0 && it.key() < sd->uvs.size() )
			edits.insert( it.key(), it.value() );
	if ( edits.isEmpty() ) {
		setStatus( tr( "Paste UVs: copied indices are out of range for this mesh." ) );
		return;
	}
	applyUVEditUndoable( edits, tr( "Paste UVs" ) );
	setStatus( tr( "Pasted %1 UVs." ).arg( edits.size() ) );
}

void UVEditorView::showLayoutMenu( const QPoint & globalPos )
{
	if ( objectModeView ) {
		setStatus( tr( "Layout tools need Edit Mode." ) );
		return;
	}
	UVAutoCloseMenu menu;
	menu.addSection( tr( "Layout Tools" ) );
	// "…" marks operators that pop the adjust-last-operation panel afterwards
	QAction * pack = menu.addAction( tr( "Pack Islands…" ) );
	QAction * avg = menu.addAction( tr( "Average Islands Scale" ) );
	menu.addSeparator();
	QAction * relax = menu.addAction( tr( "Minimize Stretch (Relax)…" ) );
	QAction * stitch = menu.addAction( tr( "Stitch (weld seams)" ) );
	relax->setEnabled( !selVerts.isEmpty() );
	stitch->setEnabled( !selVerts.isEmpty() );
	menu.addSeparator();
	QAction * overlap = menu.addAction( tr( "Select Overlapping UVs" ) );
	QAction * stretch = menu.addAction( showStretch
		? tr( "Hide Stretch Overlay" ) : tr( "Show Stretch Overlay" ) );
	menu.addSeparator();
	QAction * copy = menu.addAction( tr( "Copy UVs" ) );
	QAction * paste = menu.addAction( tr( "Paste UVs" ) );
	copy->setEnabled( !selVerts.isEmpty() );
	paste->setEnabled( !copiedUVs.isEmpty() );
	QAction * chosen = menu.exec( globalPos );
	if ( chosen == pack ) packIslands();
	else if ( chosen == avg ) averageIslandsScale();
	else if ( chosen == relax ) minimizeStretch( 20 );
	else if ( chosen == stitch ) stitchSelection();
	else if ( chosen == overlap ) selectOverlappingUVs();
	else if ( chosen == stretch ) { showStretch = !showStretch; update(); }
	else if ( chosen == copy ) copySelectedUVs();
	else if ( chosen == paste ) pasteCopiedUVs();
}

// ---------------------------------------------------------------------------
// Phase 4 unwrap-with-pins / export

void UVEditorView::pinSelected( bool pin )
{
	if ( objectModeView || selVerts.isEmpty() ) {
		setStatus( tr( "Pin: select UVs first." ) );
		return;
	}
	if ( pin )
		pinnedVerts.unite( selVerts );
	else
		pinnedVerts.subtract( selVerts );
	setStatus( pin ? tr( "Pinned %1 UVs (held fixed by Unwrap)." ).arg( selVerts.size() )
		: tr( "Unpinned selection (%1 pins remain)." ).arg( pinnedVerts.size() ) );
	update();
}

void UVEditorView::invertPins()
{
	const UVShapeData * sd = active();
	if ( !sd )
		return;
	QSet<int> inv;
	for ( int v = 0; v < sd->uvs.size(); v++ )
		if ( !pinnedVerts.contains( v ) )
			inv << v;
	pinnedVerts = inv;
	setStatus( tr( "Inverted pins (%1 pinned)." ).arg( pinnedVerts.size() ) );
	update();
}

void UVEditorView::unwrapWithPins()
{
	if ( pinnedVerts.isEmpty() ) {
		unwrapSelection();                                // no pins: auto-pin + pack
		return;
	}
	if ( objectModeView ) {
		setStatus( tr( "Object Mode is read-only — switch to Edit Mode to unwrap." ) );
		return;
	}
	const UVShapeData * sd = active();
	if ( !sd || !nif )
		return;
	QVector<Triangle> target;
	for ( int f = 0; f < sd->tris.size(); f++ ) {
		const Triangle & t = sd->tris.at( f );
		const bool inSel = ( selectMode == 3 )
			? selFaces.contains( f )
			: ( selVerts.contains( t[0] ) && selVerts.contains( t[1] ) && selVerts.contains( t[2] ) );
		if ( inSel && faceVisibleUV( f, t ) )
			target << t;
	}
	if ( target.isEmpty() ) {
		setStatus( tr( "Unwrap: select the faces to unwrap first." ) );
		return;
	}
	QVector<Vector3> pos;
	if ( !readShapePositions( nif, *sd, pos ) ) {
		setStatus( tr( "Unwrap: could not read vertex positions." ) );
		return;
	}
	// pins carry their current UV positions (absolute placement anchors)
	QHash<int, Vector2> pins;
	for ( int v : std::as_const( pinnedVerts ) )
		if ( v >= 0 && v < sd->uvs.size() )
			pins.insert( v, sd->uvs.at( v ) );

	// split target into connected components; solve each honouring its pins
	QHash<int, int> compRoot;
	std::function<int(int)> findRoot = [&]( int a ) {
		int r = a;
		while ( compRoot.value( r, r ) != r )
			r = compRoot.value( r, r );
		return r;
	};
	for ( const Triangle & t : std::as_const( target ) ) {
		if ( !compRoot.contains( t[0] ) ) compRoot[t[0]] = t[0];
		if ( !compRoot.contains( t[1] ) ) compRoot[t[1]] = t[1];
		if ( !compRoot.contains( t[2] ) ) compRoot[t[2]] = t[2];
		int r0 = findRoot( t[0] );
		compRoot[findRoot( t[1] )] = r0;
		compRoot[findRoot( t[2] )] = findRoot( r0 );
	}
	QHash<int, QVector<Triangle>> componentTris;
	for ( const Triangle & t : std::as_const( target ) )
		componentTris[findRoot( t[0] )] << t;

	QHash<int, Vector2> edits;
	for ( auto it = componentTris.constBegin(); it != componentTris.constEnd(); ++it ) {
		QHash<int, Vector2> uv;
		if ( !lscmSolveComponent( pos, it.value(), uv, &pins ) )
			continue;
		for ( auto uvIt = uv.constBegin(); uvIt != uv.constEnd(); ++uvIt )
			edits.insert( uvIt.key(), uvIt.value() );
	}
	if ( edits.isEmpty() ) {
		setStatus( tr( "Unwrap failed (degenerate geometry?)." ) );
		return;
	}
	applyUVEditUndoable( edits, tr( "Unwrap (Pinned)" ) );
	setStatus( tr( "Unwrapped %1 UVs around %2 pins." ).arg( edits.size() ).arg( pins.size() ) );
}

void UVEditorView::exportUVLayout()
{
	const UVShapeData * sd = active();
	if ( !sd ) {
		setStatus( tr( "Export UV: no mesh loaded." ) );
		return;
	}
	int size = 1024;
	if ( textureRes[0] > 0.0f )
		size = std::clamp( int( textureRes[0] ), 256, 4096 );
	const QString fn = QFileDialog::getSaveFileName( this, tr( "Export UV Layout" ),
		QString(), tr( "PNG Image (*.png)" ) );
	if ( fn.isEmpty() )
		return;
	QImage img( size, size, QImage::Format_ARGB32 );
	img.fill( Qt::transparent );
	QPainter pnt( &img );
	pnt.setRenderHint( QPainter::Antialiasing, true );
	auto toPx = [&]( const Vector2 & uv ) {
		return QPointF( double( uv[0] ) * size, ( 1.0 - double( uv[1] ) ) * size );
	};
	for ( int s = 0; s < shapes.size(); s++ ) {
		const UVShapeData & d = shapes.at( s );
		if ( !d.valid )
			continue;
		const QColor col = ( s == activeShape ) ? QColor( 235, 235, 240, 235 )
			: QColor( 150, 160, 180, 150 );
		pnt.setPen( QPen( col, ( s == activeShape ) ? 1.2 : 0.8 ) );
		for ( const Triangle & t : d.tris ) {
			const QPointF a = toPx( d.uvs.at( t[0] ) );
			const QPointF b = toPx( d.uvs.at( t[1] ) );
			const QPointF c = toPx( d.uvs.at( t[2] ) );
			pnt.drawLine( a, b );
			pnt.drawLine( b, c );
			pnt.drawLine( c, a );
		}
	}
	pnt.end();
	if ( img.save( fn, "PNG" ) )
		setStatus( tr( "Exported UV layout to %1" ).arg( QFileInfo( fn ).fileName() ) );
	else
		setStatus( tr( "Export failed: could not write %1" ).arg( fn ) );
}

// ---------------------------------------------------------------------------
// Phase 4 vertex splitting (Rip / Split, Smart UV Project) — BSTriShape only.
// FO4 packs skin weights inline in the vertex data, so duplicating a vertex row
// copies its weights automatically and no skin-partition resync is needed.

//! Deep-copy every field of one packed vertex row into another (== tlCopyItemValues).
static void uvCopyVertexRow( NifModel * nif, const QModelIndex & src, const QModelIndex & dst )
{
	const int rc = nif->rowCount( src );
	if ( rc > 0 && rc == nif->rowCount( dst ) ) {
		for ( int r = 0; r < rc; r++ )
			uvCopyVertexRow( nif, nif->getIndex( src, r ), nif->getIndex( dst, r ) );
	} else {
		nif->setIndexValue( dst, nif->getValue( src ) );
	}
}

//! Recompute a BSTriShape's bounding sphere from its vertex positions.
static void uvUpdateShapeBounds( NifModel * nif, const QModelIndex & iShape )
{
	QModelIndex iBound = nif->getIndex( iShape, "Bounding Sphere" );
	QModelIndex iVD = nif->getIndex( iShape, "Vertex Data" );
	if ( !iBound.isValid() || !iVD.isValid() )
		return;
	const int nv = nif->rowCount( iVD );
	if ( nv <= 0 )
		return;
	Vector3 mn, mx;
	for ( int i = 0; i < nv; i++ ) {
		Vector3 v = nif->get<Vector3>( nif->getIndex( iVD, i ), "Vertex" );
		if ( i == 0 ) { mn = mx = v; }
		else for ( int c = 0; c < 3; c++ ) { mn[c] = std::min( mn[c], v[c] ); mx[c] = std::max( mx[c], v[c] ); }
	}
	Vector3 c = ( mn + mx ) * 0.5f;
	float r = 0.0f;
	for ( int i = 0; i < nv; i++ )
		r = std::max( r, ( nif->get<Vector3>( nif->getIndex( iVD, i ), "Vertex" ) - c ).length() );
	nif->set<Vector3>( iBound, "Center", c );
	nif->set<float>( iBound, "Radius", r );
}

void UVEditorView::ripSelectedFaces()
{
	UVShapeData * sd = active();
	if ( !sd || objectModeView ) {
		setStatus( tr( "Rip: switch to Edit Mode first." ) );
		return;
	}
	if ( sd->legacyData ) {
		setStatus( tr( "Rip/Split is supported on FO4 (BSTriShape) meshes only." ) );
		return;
	}
	QSet<int> S;
	for ( int f = 0; f < sd->tris.size(); f++ ) {
		const Triangle & t = sd->tris.at( f );
		const bool inSel = ( selectMode == 3 )
			? selFaces.contains( f )
			: ( selVerts.contains( t[0] ) && selVerts.contains( t[1] ) && selVerts.contains( t[2] ) );
		if ( inSel && faceVisibleUV( f, t ) )
			S << f;
	}
	if ( S.isEmpty() ) {
		setStatus( tr( "Rip: select whole faces (all three corners) first." ) );
		return;
	}
	QSet<int> inS, inRest;
	for ( int f = 0; f < sd->tris.size(); f++ ) {
		const Triangle & t = sd->tris.at( f );
		QSet<int> & dst = S.contains( f ) ? inS : inRest;
		dst << t[0] << t[1] << t[2];
	}
	QVector<int> boundary;
	for ( int v : std::as_const( inS ) )
		if ( inRest.contains( v ) )
			boundary << v;
	if ( boundary.isEmpty() ) {
		setStatus( tr( "Rip: the selected faces are already a separate island." ) );
		return;
	}
	std::sort( boundary.begin(), boundary.end() );

	const QModelIndex iShape = nif->getBlockIndex( sd->block );
	const QModelIndex iVD = nif->getIndex( iShape, "Vertex Data" );
	const QModelIndex iTris = nif->getIndex( iShape, "Triangles" );
	if ( !iVD.isValid() || !iTris.isValid() ) {
		setStatus( tr( "Rip: unexpected mesh layout." ) );
		return;
	}
	const int numVerts = nif->get<int>( iShape, "Num Vertices" );
	const int numTris = nif->get<int>( iShape, "Num Triangles" );
	const int dataSize = nif->get<int>( iShape, "Data Size" );
	const int stride = ( numVerts > 0 ) ? ( dataSize - numTris * 6 ) / numVerts : 0;

	QHash<int, int> dup;
	for ( int i = 0; i < boundary.size(); i++ )
		dup.insert( boundary[i], numVerts + i );
	const int newN = numVerts + boundary.size();

	applyingEdit = true;
	const bool ok = nifSnapshotOp( nif, tr( "Rip UV Faces" ), [&]() {
		// suppress the per-leaf dataChanged storm; one dataChanged at the end
		nif->setState( BaseModel::Processing );
		nif->set<int>( iShape, "Num Vertices", newN );
		nif->updateArraySize( iVD );
		for ( int i = 0; i < boundary.size(); i++ )
			uvCopyVertexRow( nif, nif->getIndex( iVD, boundary[i] ), nif->getIndex( iVD, numVerts + i ) );
		for ( int f : std::as_const( S ) ) {
			if ( f >= numTris )
				continue;
			Triangle t = nif->get<Triangle>( nif->getIndex( iTris, f ) );
			for ( int c = 0; c < 3; c++ ) {
				auto it = dup.constFind( t[c] );
				if ( it != dup.constEnd() )
					t[c] = it.value();
			}
			nif->set<Triangle>( nif->getIndex( iTris, f ), t );
		}
		if ( stride > 0 )
			nif->set<int>( iShape, "Data Size", newN * stride + numTris * 6 );
		uvUpdateShapeBounds( nif, iShape );
		nif->restoreState();
		nif->dataChanged( iShape, iShape );
	} );
	applyingEdit = false;
	if ( ok ) {
		if ( ogl )
			ogl->updateScene();
		rebuildFromViewport();
		setStatus( tr( "Ripped %1 faces free (%2 seam verts duplicated)." )
			.arg( S.size() ).arg( boundary.size() ) );
	}
}

void UVEditorView::smartProject( float angleDeg )
{
	UVShapeData * sd = active();
	if ( !sd || objectModeView ) {
		setStatus( tr( "Smart UV Project: switch to Edit Mode first." ) );
		return;
	}
	if ( sd->legacyData ) {
		setStatus( tr( "Smart UV Project is supported on FO4 (BSTriShape) meshes only." ) );
		return;
	}
	QVector<Vector3> pos;
	if ( !readShapePositions( nif, *sd, pos ) ) {
		setStatus( tr( "Smart UV Project: could not read vertex positions." ) );
		return;
	}
	const int numTris = sd->tris.size();
	const int numVerts = sd->uvs.size();
	if ( numTris < 1 )
		return;

	// per-face normals
	QVector<Vector3> fnrm( numTris );
	for ( int f = 0; f < numTris; f++ ) {
		const Triangle & t = sd->tris.at( f );
		Vector3 n = Vector3::crossproduct( pos.at( t[1] ) - pos.at( t[0] ), pos.at( t[2] ) - pos.at( t[0] ) );
		const float len = n.length();
		fnrm[f] = ( len > 1.0e-12f ) ? n / len : Vector3( 0.0f, 0.0f, 1.0f );
	}
	auto ekey = []( int a, int b ) {
		if ( a > b ) std::swap( a, b );
		return ( quint64( uint( a ) ) << 32 ) | uint( b );
	};
	QHash<quint64, QVector<int>> edgeFaces;
	for ( int f = 0; f < numTris; f++ ) {
		const Triangle & t = sd->tris.at( f );
		edgeFaces[ekey( t[0], t[1] )] << f;
		edgeFaces[ekey( t[1], t[2] )] << f;
		edgeFaces[ekey( t[2], t[0] )] << f;
	}
	// region-grow charts by inter-face normal angle
	const float cosLimit = std::cos( std::clamp( angleDeg, 1.0f, 89.0f ) * 0.017453292519943295f );
	QVector<int> chartOf( numTris, -1 );
	int chartCount = 0;
	for ( int seed = 0; seed < numTris; seed++ ) {
		if ( chartOf[seed] >= 0 )
			continue;
		const int c = chartCount++;
		QVector<int> stack;
		stack << seed;
		chartOf[seed] = c;
		while ( !stack.isEmpty() ) {
			const int f = stack.takeLast();
			const Triangle & t = sd->tris.at( f );
			const int es[3][2] = { { t[0], t[1] }, { t[1], t[2] }, { t[2], t[0] } };
			for ( int k = 0; k < 3; k++ ) {
				for ( int g : edgeFaces.value( ekey( es[k][0], es[k][1] ) ) ) {
					if ( g == f || chartOf[g] >= 0 )
						continue;
					if ( Vector3::dotproduct( fnrm[f], fnrm[g] ) >= cosLimit ) {
						chartOf[g] = c;
						stack << g;
					}
				}
			}
		}
	}
	// final index per (chart, vertex): first chart keeps the original, others dup
	auto keyCV = [&]( int c, int v ) -> qint64 { return qint64( c ) * numVerts + v; };
	QHash<qint64, int> finalIndex;
	QHash<int, int> firstChartOfVert;
	QVector<int> dupSrc;
	for ( int f = 0; f < numTris; f++ ) {
		const int c = chartOf[f];
		const Triangle & t = sd->tris.at( f );
		for ( int k = 0; k < 3; k++ ) {
			const int v = t[k];
			const qint64 key = keyCV( c, v );
			if ( finalIndex.contains( key ) )
				continue;
			auto it = firstChartOfVert.constFind( v );
			if ( it == firstChartOfVert.constEnd() ) {
				firstChartOfVert.insert( v, c );
				finalIndex.insert( key, v );
			} else {
				finalIndex.insert( key, numVerts + dupSrc.size() );
				dupSrc << v;
			}
		}
	}
	const int newN = numVerts + dupSrc.size();

	// LSCM each chart on the original positions, normalise texel density
	struct CR { QHash<int, Vector2> uvByFinal; Vector2 mn, mx; };
	QVector<QVector<Triangle>> chartTris( chartCount );
	for ( int f = 0; f < numTris; f++ )
		chartTris[chartOf[f]] << sd->tris.at( f );
	QVector<CR> charts;
	for ( int c = 0; c < chartCount; c++ ) {
		if ( chartTris[c].isEmpty() )
			continue;
		QHash<int, Vector2> uv;
		if ( !lscmSolveComponent( pos, chartTris[c], uv ) || uv.isEmpty() )
			continue;
		double a3 = 0.0, au = 0.0;
		for ( const Triangle & t : chartTris[c] ) {
			Vector3 n3 = Vector3::crossproduct( pos.at( t[1] ) - pos.at( t[0] ), pos.at( t[2] ) - pos.at( t[0] ) );
			a3 += 0.5 * std::sqrt( double( Vector3::dotproduct( n3, n3 ) ) );
			const Vector2 & A = uv.value( t[0] );
			const Vector2 & B = uv.value( t[1] );
			const Vector2 & C = uv.value( t[2] );
			au += 0.5 * std::fabs( double( ( B[0] - A[0] ) * ( C[1] - A[1] ) - ( C[0] - A[0] ) * ( B[1] - A[1] ) ) );
		}
		const float ds = ( au > 1.0e-12 ) ? float( std::sqrt( a3 / au ) ) : 1.0f;
		CR cr;
		bool first = true;
		for ( auto it = uv.begin(); it != uv.end(); ++it ) {
			const Vector2 p = *it * ds;
			const int fin = finalIndex.value( keyCV( c, it.key() ), it.key() );
			cr.uvByFinal.insert( fin, p );
			if ( first ) { cr.mn = cr.mx = p; first = false; }
			else {
				cr.mn[0] = std::min( cr.mn[0], p[0] ); cr.mn[1] = std::min( cr.mn[1], p[1] );
				cr.mx[0] = std::max( cr.mx[0], p[0] ); cr.mx[1] = std::max( cr.mx[1], p[1] );
			}
		}
		charts << cr;
	}
	if ( charts.isEmpty() ) {
		setStatus( tr( "Smart UV Project failed (degenerate geometry?)." ) );
		return;
	}
	// shelf pack, then normalise into the 0-1 tile
	std::sort( charts.begin(), charts.end(), []( const CR & a, const CR & b ) {
		return ( a.mx[1] - a.mn[1] ) > ( b.mx[1] - b.mn[1] );
	} );
	float totalW = 0.0f, maxW = 0.0f;
	for ( const CR & r : std::as_const( charts ) ) {
		totalW += r.mx[0] - r.mn[0];
		maxW = std::max( maxW, r.mx[0] - r.mn[0] );
	}
	const float shelfWidth = std::max( maxW, std::sqrt( std::max( totalW, 1.0e-6f ) * totalW ) / 4.0f + maxW * 0.5f );
	const float gap = shelfWidth * 0.01f;
	float penX = 0.0f, penY = 0.0f, shelfH = 0.0f;
	QHash<int, Vector2> finalUV;
	for ( const CR & r : std::as_const( charts ) ) {
		const float w = r.mx[0] - r.mn[0], h = r.mx[1] - r.mn[1];
		if ( penX > 0.0f && penX + w > shelfWidth ) {
			penX = 0.0f;
			penY += shelfH + gap;
			shelfH = 0.0f;
		}
		const Vector2 off( penX - r.mn[0], penY - r.mn[1] );
		for ( auto it = r.uvByFinal.constBegin(); it != r.uvByFinal.constEnd(); ++it )
			finalUV.insert( it.key(), it.value() + off );
		penX += w + gap;
		shelfH = std::max( shelfH, h );
	}
	Vector2 mn, mx;
	bool f0 = true;
	for ( auto it = finalUV.constBegin(); it != finalUV.constEnd(); ++it ) {
		if ( f0 ) { mn = mx = it.value(); f0 = false; }
		else {
			mn[0] = std::min( mn[0], it.value()[0] ); mn[1] = std::min( mn[1], it.value()[1] );
			mx[0] = std::max( mx[0], it.value()[0] ); mx[1] = std::max( mx[1], it.value()[1] );
		}
	}
	const float span = std::max( { mx[0] - mn[0], mx[1] - mn[1], 1.0e-6f } );
	const float invSpan = 0.98f / span;
	for ( auto it = finalUV.begin(); it != finalUV.end(); ++it )
		*it = ( *it - mn ) * invSpan + Vector2( 0.01f, 0.01f );

	const QModelIndex iShape = nif->getBlockIndex( sd->block );
	const QModelIndex iVD = nif->getIndex( iShape, "Vertex Data" );
	const QModelIndex iTris = nif->getIndex( iShape, "Triangles" );
	if ( !iVD.isValid() || !iTris.isValid() ) {
		setStatus( tr( "Smart UV Project: unexpected mesh layout." ) );
		return;
	}
	const int dataSize = nif->get<int>( iShape, "Data Size" );
	const int stride = ( numVerts > 0 ) ? ( dataSize - numTris * 6 ) / numVerts : 0;

	applyingEdit = true;
	const bool ok = nifSnapshotOp( nif, tr( "Smart UV Project" ), [&]() {
		// suppress the per-leaf dataChanged storm (Block Details reacting to
		// thousands of UV writes); one dataChanged goes out at the end
		nif->setState( BaseModel::Processing );
		if ( newN != numVerts ) {
			nif->set<int>( iShape, "Num Vertices", newN );
			nif->updateArraySize( iVD );
			for ( int k = 0; k < dupSrc.size(); k++ )
				uvCopyVertexRow( nif, nif->getIndex( iVD, dupSrc[k] ), nif->getIndex( iVD, numVerts + k ) );
		}
		for ( auto it = finalUV.constBegin(); it != finalUV.constEnd(); ++it ) {
			if ( it.key() < 0 || it.key() >= newN )
				continue;
			QModelIndex uvi = nif->getIndex( nif->getIndex( iVD, it.key() ), "UV" );
			if ( uvi.isValid() )
				nif->set<HalfVector2>( uvi, HalfVector2( it.value() ) );
		}
		for ( int f = 0; f < numTris && f < nif->rowCount( iTris ); f++ ) {
			const int c = chartOf[f];
			const Triangle & ot = sd->tris.at( f );
			Triangle nt( finalIndex.value( keyCV( c, ot[0] ), ot[0] ),
				finalIndex.value( keyCV( c, ot[1] ), ot[1] ),
				finalIndex.value( keyCV( c, ot[2] ), ot[2] ) );
			nif->set<Triangle>( nif->getIndex( iTris, f ), nt );
		}
		if ( stride > 0 )
			nif->set<int>( iShape, "Data Size", newN * stride + numTris * 6 );
		uvUpdateShapeBounds( nif, iShape );
		nif->restoreState();
		nif->dataChanged( iShape, iShape );
	} );
	applyingEdit = false;
	if ( ok ) {
		if ( ogl )
			ogl->updateScene();
		rebuildFromViewport();
		setStatus( tr( "Smart UV Project: %1 charts, %2 verts added." )
			.arg( chartCount ).arg( dupSrc.size() ) );
		armOperatorPanel( 4, { angleDeg } );
	}
}

// ---------------------------------------------------------------------------
// unwrap (angle-based LSCM + projections)

//! Object-space vertex positions of a shape
static bool readShapePositions( NifModel * nif, const UVShapeData & sd, QVector<Vector3> & pos )
{
	pos.clear();
	QModelIndex iShape( sd.iShape );
	if ( !iShape.isValid() )
		return false;
	if ( !sd.legacyData ) {
		QModelIndex iData = nif->getIndex( iShape, "Vertex Data" );
		if ( !iData.isValid() )
			return false;
		const int nv = sd.uvs.size();
		pos.reserve( nv );
		for ( int i = 0; i < nv; i++ )
			pos << nif->get<Vector3>( nif->index( i, 0, iData ), "Vertex" );
	} else {
		QModelIndex iData = nif->getBlockIndex( nif->getLink( iShape, "Data" ) );
		if ( !iData.isValid() )
			return false;
		pos = nif->getArray<Vector3>( iData, "Vertices" );
	}
	return pos.size() >= sd.uvs.size();
}

//! Least Squares Conformal Map for one connected patch of triangles.
//! Unknowns are (u,v) per vertex; two pinned vertices remove the similarity
//! degrees of freedom; the normal equations are solved with Jacobi-
//! preconditioned conjugate gradient (A applied row-wise, never assembled).
static bool lscmSolveComponent( const QVector<Vector3> & pos, const QVector<Triangle> & tris,
	QHash<int, Vector2> & outUV, const QHash<int, Vector2> * pins )
{
	// collect the patch vertices
	QHash<int, int> localOf;
	QVector<int> vertIds;
	for ( const Triangle & t : tris ) {
		for ( int c = 0; c < 3; c++ ) {
			if ( !localOf.contains( t[c] ) ) {
				localOf.insert( t[c], vertIds.size() );
				vertIds << t[c];
			}
		}
	}
	const int n = vertIds.size();
	if ( n < 3 || tris.isEmpty() )
		return false;

	// Pins: caller-supplied fixed UVs (Phase 4 pinning) take priority, provided at
	// least two of them fall in this component. Otherwise pin the extreme pair
	// along the dominant object-space axis, which removes the similarity DOF.
	QHash<int, Vector2> pinMap;
	if ( pins ) {
		for ( int vid : std::as_const( vertIds ) ) {
			auto it = pins->constFind( vid );
			if ( it != pins->constEnd() )
				pinMap.insert( vid, it.value() );
		}
	}
	if ( pinMap.size() < 2 ) {
		pinMap.clear();
		Vector3 mn = pos.at( vertIds.first() ), mx = mn;
		for ( int vid : std::as_const( vertIds ) ) {
			const Vector3 & p = pos.at( vid );
			for ( int a = 0; a < 3; a++ ) {
				mn[a] = std::min( mn[a], p[a] );
				mx[a] = std::max( mx[a], p[a] );
			}
		}
		int axis = 0;
		for ( int a = 1; a < 3; a++ )
			if ( mx[a] - mn[a] > mx[axis] - mn[axis] )
				axis = a;
		int pinA = vertIds.first(), pinB = vertIds.first();
		for ( int vid : std::as_const( vertIds ) ) {
			if ( pos.at( vid )[axis] < pos.at( pinA )[axis] ) pinA = vid;
			if ( pos.at( vid )[axis] > pos.at( pinB )[axis] ) pinB = vid;
		}
		if ( pinA == pinB )
			return false;
		pinMap.insert( pinA, Vector2( 0.0f, 0.0f ) );
		pinMap.insert( pinB, Vector2( 1.0f, 0.0f ) );
	}

	// sparse rows: 2 per triangle, 6 coefficients each, over 2n unknowns
	// (u at 2*local, v at 2*local+1)
	struct Row { int col[6]; double a[6]; double b; };
	QVector<Row> rows;
	rows.reserve( tris.size() * 2 );
	auto unknownOf = [&]( int vid, bool vComp ) { return localOf.value( vid ) * 2 + ( vComp ? 1 : 0 ); };
	const int freeMark = -1;
	QVector<double> pinned( n * 2, 0.0 );
	QVector<int> freeIndex( n * 2, freeMark );
	{
		int next = 0;
		for ( int i = 0; i < n * 2; i++ ) {
			const int vid = vertIds.at( i / 2 );
			auto it = pinMap.constFind( vid );
			if ( it != pinMap.constEnd() ) {
				pinned[i] = it.value()[i & 1];
			} else {
				freeIndex[i] = next++;
			}
		}
	}
	int nFree = 0;
	for ( int i = 0; i < n * 2; i++ )
		if ( freeIndex[i] >= 0 ) nFree = std::max( nFree, freeIndex[i] + 1 );
	if ( nFree < 1 )
		return false;

	for ( const Triangle & t : tris ) {
		const Vector3 & p0 = pos.at( t[0] );
		const Vector3 & p1 = pos.at( t[1] );
		const Vector3 & p2 = pos.at( t[2] );
		Vector3 e1 = p1 - p0;
		Vector3 e2 = p2 - p0;
		const double x1 = std::sqrt( double( Vector3::dotproduct( e1, e1 ) ) );
		if ( x1 < 1.0e-9 )
			continue;
		const double x2 = double( Vector3::dotproduct( e2, e1 ) ) / x1;
		Vector3 nrm = Vector3::crossproduct( e1, e2 );
		const double y2 = std::sqrt( double( Vector3::dotproduct( nrm, nrm ) ) ) / x1;
		if ( y2 < 1.0e-9 )
			continue;
		// conformality (P1-P0)*(x2+iy2) = (P2-P0)*x1, expanded to 2 real rows
		const int cols[6] = {
			unknownOf( t[0], false ), unknownOf( t[0], true ),
			unknownOf( t[1], false ), unknownOf( t[1], true ),
			unknownOf( t[2], false ), unknownOf( t[2], true )
		};
		const double row1[6] = { x1 - x2,  y2,      x2, -y2, -x1, 0.0 };
		const double row2[6] = { -y2,      x1 - x2, y2,  x2, 0.0, -x1 };
		for ( const double * r : { row1, row2 } ) {
			Row row;
			row.b = 0.0;
			for ( int c = 0; c < 6; c++ ) {
				row.col[c] = cols[c];
				row.a[c] = r[c];
				if ( freeIndex[cols[c]] < 0 )
					row.b -= r[c] * pinned[cols[c]];
			}
			rows << row;
		}
	}
	if ( rows.isEmpty() )
		return false;

	// conjugate gradient on the normal equations
	QVector<double> x( nFree, 0.0 ), r( nFree, 0.0 ), z( nFree, 0.0 ),
		p( nFree, 0.0 ), Ap( nFree, 0.0 ), diag( nFree, 0.0 );
	auto applyNormal = [&]( const QVector<double> & in, QVector<double> & out ) {
		out.fill( 0.0 );
		for ( const Row & row : std::as_const( rows ) ) {
			double dot = 0.0;
			for ( int c = 0; c < 6; c++ ) {
				const int fi = freeIndex[row.col[c]];
				if ( fi >= 0 )
					dot += row.a[c] * in.at( fi );
			}
			for ( int c = 0; c < 6; c++ ) {
				const int fi = freeIndex[row.col[c]];
				if ( fi >= 0 )
					out[fi] += row.a[c] * dot;
			}
		}
	};
	for ( const Row & row : std::as_const( rows ) )
		for ( int c = 0; c < 6; c++ ) {
			const int fi = freeIndex[row.col[c]];
			if ( fi >= 0 )
				diag[fi] += row.a[c] * row.a[c];
		}
	for ( double & d : diag )
		if ( d < 1.0e-12 ) d = 1.0;
	// r = Aᵀb (x starts at zero)
	for ( const Row & row : std::as_const( rows ) )
		for ( int c = 0; c < 6; c++ ) {
			const int fi = freeIndex[row.col[c]];
			if ( fi >= 0 )
				r[fi] += row.a[c] * row.b;
		}
	double rz = 0.0;
	for ( int i = 0; i < nFree; i++ ) {
		z[i] = r[i] / diag[i];
		p[i] = z[i];
		rz += r[i] * z[i];
	}
	const double tol = 1.0e-12 * std::max( rz, 1.0e-30 );
	const int maxIter = std::min( 2000, nFree * 4 + 50 );
	for ( int it = 0; it < maxIter && rz > tol; it++ ) {
		applyNormal( p, Ap );
		double pAp = 0.0;
		for ( int i = 0; i < nFree; i++ )
			pAp += p[i] * Ap[i];
		if ( pAp <= 0.0 )
			break;
		const double alpha = rz / pAp;
		double rzNew = 0.0;
		for ( int i = 0; i < nFree; i++ ) {
			x[i] += alpha * p[i];
			r[i] -= alpha * Ap[i];
			z[i] = r[i] / diag[i];
			rzNew += r[i] * z[i];
		}
		const double beta = rzNew / rz;
		rz = rzNew;
		for ( int i = 0; i < nFree; i++ )
			p[i] = z[i] + beta * p[i];
	}

	for ( int i = 0; i < n; i++ ) {
		const int vid = vertIds.at( i );
		double u = ( freeIndex[i * 2] >= 0 ) ? x.at( freeIndex[i * 2] ) : pinned[i * 2];
		double v = ( freeIndex[i * 2 + 1] >= 0 ) ? x.at( freeIndex[i * 2 + 1] ) : pinned[i * 2 + 1];
		outUV.insert( vid, Vector2( float( u ), float( v ) ) );
	}
	return true;
}

// ---------------------------------------------------------------------------
// modal transforms

Vector2 UVEditorView::transformBasePoint() const
{
	if ( xverts.isEmpty() )
		return Vector2();
	if ( snapBase == 3 && activeVert >= 0 ) {
		for ( const XVert & xv : xverts )
			if ( xv.shape == activeShape && xv.idx == activeVert )
				return xv.current;
	}
	if ( snapBase == 1 || snapBase == 3 ) {
		Vector2 mn = xverts.first().current, mx = xverts.first().current;
		for ( const XVert & xv : xverts ) {
			mn[0] = std::min( mn[0], xv.current[0] ); mn[1] = std::min( mn[1], xv.current[1] );
			mx[0] = std::max( mx[0], xv.current[0] ); mx[1] = std::max( mx[1], xv.current[1] );
		}
		return ( mn + mx ) * 0.5f;
	}
	if ( snapBase == 2 ) {
		Vector2 sum;
		for ( const XVert & xv : xverts )
			sum += xv.current;
		return sum / float( xverts.size() );
	}
	// closest: nearest dragged vert to the mouse
	QPointF mousePx = QPointF( mapFromGlobal( QCursor::pos() ) ) * devicePixelRatioF();
	Vector2 mouseUV = mapToUV( mousePx );
	Vector2 best = xverts.first().current;
	float bestD = 1.0e30f;
	for ( const XVert & xv : xverts ) {
		Vector2 d = xv.current - mouseUV;
		float dd = d[0] * d[0] + d[1] * d[1];
		if ( dd < bestD ) {
			bestD = dd;
			best = xv.current;
		}
	}
	return best;
}

void UVEditorView::beginTransform( int mode )
{
	if ( objectModeView ) {
		setStatus( tr( "Object Mode is read-only — switch to Edit Mode to move UVs." ) );
		return;
	}
	const UVShapeData * sd = active();
	if ( !sd || xformMode != 0 )
		return;
	// multi-mesh: the gesture moves every shape's selected UVs together
	xverts.clear();
	for ( int s = 0; s < shapes.size(); s++ ) {
		const UVShapeData & osd = shapes.at( s );
		if ( !osd.valid )
			continue;
		const QSet<int> & sVerts = ( s == activeShape ) ? selVerts : osd.selVerts;
		for ( int v : sVerts ) {
			if ( v < 0 || v >= osd.uvs.size() )
				continue;
			XVert xv;
			xv.shape = s;
			xv.idx = v;
			xv.orig = osd.uvs.at( v );
			xv.current = xv.orig;
			xverts << xv;
		}
	}
	if ( xverts.isEmpty() ) {
		setStatus( tr( "Nothing selected to transform." ) );
		return;
	}
	xformMode = mode;
	xformAxis = 0;
	numericBuf.clear();
	xformLastDelta = Vector2();
	xformLastAngle = 0.0f;
	xformLastScale = Vector2( 1.0f, 1.0f );

	if ( pivotMode == 2 ) {
		xformPivot = cursor2D;
	} else if ( pivotMode == 1 ) {
		Vector2 sum;
		for ( const XVert & xv : xverts )
			sum += xv.orig;
		xformPivot = sum / float( xverts.size() );
	} else {
		Vector2 mn = xverts.first().orig, mx = xverts.first().orig;
		for ( const XVert & xv : xverts ) {
			mn[0] = std::min( mn[0], xv.orig[0] ); mn[1] = std::min( mn[1], xv.orig[1] );
			mx[0] = std::max( mx[0], xv.orig[0] ); mx[1] = std::max( mx[1], xv.orig[1] );
		}
		xformPivot = ( mn + mx ) * 0.5f;
	}

	QPointF px = QPointF( mapFromGlobal( QCursor::pos() ) ) * devicePixelRatioF();
	xformStartPx = px;
	xformVirtualPx = px;
	xformLastPx = px;
	grabMouse();
	setStatus( mode == 1
		? tr( "Move: X/Y constrains, type a value, Ctrl snaps, LMB/Enter commits, Esc/RMB cancels" )
		: mode == 2 ? tr( "Rotate: type degrees, Ctrl snaps to the increment" )
		: tr( "Scale: X/Y constrains, type a factor, Ctrl snaps" ) );
	update();
}

void UVEditorView::updateTransform( const QPointF & devicePx, Qt::KeyboardModifiers mods )
{
	UVShapeData * sd = active();
	if ( !sd || xformMode == 0 )
		return;

	// Shift precision: accumulate scaled deltas onto a virtual position
	const double precision = mods.testFlag( Qt::ShiftModifier ) ? 0.1 : 1.0;
	xformVirtualPx += ( devicePx - xformLastPx ) * precision;
	xformLastPx = devicePx;

	const bool ctrl = mods.testFlag( Qt::ControlModifier );
	const bool snapping = ( snapOn != ctrl );
	snapIndicatorOn = false;

	const Vector2 startUV = mapToUV( xformStartPx );
	const Vector2 curUV = mapToUV( xformVirtualPx );

	const bool haveNumeric = !numericBuf.isEmpty() && numericBuf != "-" && numericBuf != "."
		&& numericBuf != "-.";
	const float numeric = haveNumeric ? numericBuf.toFloat() : 0.0f;

	if ( xformMode == 1 ) {
		Vector2 delta = curUV - startUV;
		if ( haveNumeric ) {
			int axis = xformAxis ? xformAxis : 1;
			delta = Vector2();
			delta[axis - 1] = numeric;
		} else if ( xformAxis == 1 ) {
			delta[1] = 0.0f;
		} else if ( xformAxis == 2 ) {
			delta[0] = 0.0f;
		}
		if ( snapping && snapMove && !haveNumeric ) {
			if ( snapTarget == 2 ) {
				// vertex: land the base point on the nearest untouched UV vertex
				// (snap targets stay on the active shape)
				QSet<int> dragged;
				for ( const XVert & xv : xverts )
					if ( xv.shape == activeShape )
						dragged << xv.idx;
				int target = nearestVertex( devicePx, 24.0 * devicePixelRatioF(), &dragged );
				if ( target >= 0 ) {
					for ( XVert & xv : xverts )
						xv.current = xv.orig + delta;
					Vector2 base = transformBasePoint();
					Vector2 corr = sd->uvs.at( target ) - base;
					if ( xformAxis == 1 ) corr[1] = 0.0f;
					if ( xformAxis == 2 ) corr[0] = 0.0f;
					delta += corr;
					snapIndicatorOn = true;
					snapIndicatorUV = sd->uvs.at( target );
				}
			} else if ( snapTarget == 3 && textureRes[0] > 0.5f ) {
				// pixel: land the base point on the nearest texel corner
				for ( XVert & xv : xverts )
					xv.current = xv.orig + delta;
				Vector2 base = transformBasePoint();
				Vector2 snapped = snapToPixel( base );
				Vector2 corr = snapped - base;
				if ( xformAxis == 1 ) corr[1] = 0.0f;
				if ( xformAxis == 2 ) corr[0] = 0.0f;
				delta += corr;
				snapIndicatorOn = true;
				snapIndicatorUV = snapped;
			} else if ( snapTarget != 2 && snapTarget != 3 && gridStep > 0.0f ) {
				if ( snapTarget == 1 ) {
					// grid: snap the base point to absolute grid lines
					for ( XVert & xv : xverts )
						xv.current = xv.orig + delta;
					Vector2 base = transformBasePoint();
					Vector2 snapped( std::round( base[0] / gridStep ) * gridStep,
						std::round( base[1] / gridStep ) * gridStep );
					Vector2 corr = snapped - base;
					if ( xformAxis == 1 ) corr[1] = 0.0f;
					if ( xformAxis == 2 ) corr[0] = 0.0f;
					delta += corr;
					snapIndicatorOn = true;
					snapIndicatorUV = snapped;
				} else {
					// increment: quantize the delta itself
					delta = Vector2( std::round( delta[0] / gridStep ) * gridStep,
						std::round( delta[1] / gridStep ) * gridStep );
				}
			}
		}
		for ( XVert & xv : xverts )
			xv.current = xv.orig + delta;
		xformLastDelta = delta;
		setStatus( tr( "Move  dU %1  dV %2%3%4" )
			.arg( delta[0], 0, 'f', 4 ).arg( delta[1], 0, 'f', 4 )
			.arg( xformAxis == 1 ? QStringLiteral( "  [U]" ) : xformAxis == 2 ? QStringLiteral( "  [V]" ) : QString(),
				numericBuf.isEmpty() ? QString() : QStringLiteral( "  (%1)" ).arg( numericBuf ) ) );
	} else if ( xformMode == 2 ) {
		float angle;
		if ( haveNumeric ) {
			angle = deg2rad( numeric );
		} else {
			QPointF pivotPx = mapFromUV( xformPivot );
			double a0 = std::atan2( xformStartPx.y() - pivotPx.y(), xformStartPx.x() - pivotPx.x() );
			double a1 = std::atan2( xformVirtualPx.y() - pivotPx.y(), xformVirtualPx.x() - pivotPx.x() );
			angle = float( a1 - a0 );
		}
		if ( snapping && snapRotate && !haveNumeric && rotIncrement > 0.0f ) {
			const float inc = deg2rad( rotIncrement );
			angle = std::round( angle / inc ) * inc;
		}
		const float c = std::cos( angle ), s = std::sin( angle );
		for ( XVert & xv : xverts ) {
			Vector2 rel = xv.orig - xformPivot;
			xv.current = xformPivot + Vector2( rel[0] * c - rel[1] * s, rel[0] * s + rel[1] * c );
		}
		xformLastAngle = angle;
		setStatus( tr( "Rotate  %1°%2" ).arg( rad2deg( angle ), 0, 'f', 2 )
			.arg( numericBuf.isEmpty() ? QString() : QStringLiteral( "  (%1)" ).arg( numericBuf ) ) );
	} else if ( xformMode == 3 ) {
		float factor;
		if ( haveNumeric ) {
			factor = numeric;
		} else {
			QPointF pivotPx = mapFromUV( xformPivot );
			double d0 = std::hypot( xformStartPx.x() - pivotPx.x(), xformStartPx.y() - pivotPx.y() );
			double d1 = std::hypot( xformVirtualPx.x() - pivotPx.x(), xformVirtualPx.y() - pivotPx.y() );
			factor = ( d0 > 1.0 ) ? float( d1 / d0 ) : 1.0f;
		}
		if ( snapping && snapScale && !haveNumeric )
			factor = std::round( factor * 10.0f ) / 10.0f;
		for ( XVert & xv : xverts ) {
			Vector2 rel = xv.orig - xformPivot;
			if ( xformAxis == 1 ) rel[0] *= factor;
			else if ( xformAxis == 2 ) rel[1] *= factor;
			else rel = rel * factor;
			xv.current = xformPivot + rel;
		}
		xformLastScale = ( xformAxis == 1 ) ? Vector2( factor, 1.0f )
			: ( xformAxis == 2 ) ? Vector2( 1.0f, factor ) : Vector2( factor, factor );
		setStatus( tr( "Scale  %1%2%3" ).arg( factor, 0, 'f', 3 )
			.arg( xformAxis == 1 ? QStringLiteral( "  [U]" ) : xformAxis == 2 ? QStringLiteral( "  [V]" ) : QString(),
				numericBuf.isEmpty() ? QString() : QStringLiteral( "  (%1)" ).arg( numericBuf ) ) );
	}

	writeLiveUVs();
	update();
}

void UVEditorView::writeLiveUVs()
{
	if ( !nif || shapes.isEmpty() )
		return;
	applyingEdit = true;
	nif->setState( BaseModel::Processing );
	QSet<int> touched;
	for ( const XVert & xv : std::as_const( xverts ) ) {
		if ( xv.shape < 0 || xv.shape >= shapes.size() )
			continue;
		UVShapeData & osd = shapes[xv.shape];
		Vector2 uv = xv.current;
		if ( constrainBounds )
			uv = Vector2( std::clamp( uv[0], 0.0f, 1.0f ), std::clamp( uv[1], 0.0f, 1.0f ) );
		osd.uvs[xv.idx] = uv;
		QModelIndex idx = uvValueIndex( nif, osd, xv.idx );
		if ( !idx.isValid() )
			continue;
		if ( osd.legacyData )
			nif->set<Vector2>( idx, uv );
		else
			nif->set<HalfVector2>( idx, HalfVector2( uv ) );
		touched << xv.shape;
	}
	nif->restoreState();
	// one notification per shape per drag step so the 3D preview follows live
	for ( int s : std::as_const( touched ) )
		nif->dataChanged( QModelIndex( shapes.at( s ).iShape ), QModelIndex( shapes.at( s ).iShape ) );
	applyingEdit = false;
}

bool UVEditorView::commitTransformUndo( const QString & name )
{
	if ( !nif || !nif->undoStack || shapes.isEmpty() )
		return false;
	// The model already holds the live-written final UVs. Record only the
	// changed vertices so Undo/Redo patches them in place (instant, no reload).
	// Multi-mesh gestures push one command per shape inside a single macro so
	// Ctrl+Z reverts the whole gesture at once.
	QHash<int, QVector<int>> verts;
	QHash<int, QVector<Vector2>> before, after;
	for ( const XVert & xv : std::as_const( xverts ) ) {
		if ( xv.shape < 0 || xv.shape >= shapes.size() )
			continue;
		Vector2 cur = xv.current;
		if ( constrainBounds )
			cur = Vector2( std::clamp( cur[0], 0.0f, 1.0f ), std::clamp( cur[1], 0.0f, 1.0f ) );
		if ( xv.orig == cur )
			continue;
		verts[xv.shape] << xv.idx;
		before[xv.shape] << xv.orig;
		after[xv.shape] << cur;
	}
	if ( verts.isEmpty() )
		return false;
	const bool macro = ( verts.size() > 1 );
	if ( macro )
		nif->undoStack->beginMacro( name );
	for ( auto it = verts.constBegin(); it != verts.constEnd(); ++it ) {
		const UVShapeData & osd = shapes.at( it.key() );
		nif->undoStack->push( new UVEditCommand( nif, osd.block, osd.legacyData,
			osd.iUVData, it.value(), before[it.key()], after[it.key()],
			/*alreadyApplied*/ true, name ) );
	}
	if ( macro )
		nif->undoStack->endMacro();
	return true;
}

void UVEditorView::endTransform( bool commit )
{
	if ( xformMode == 0 )
		return;
	const int mode = xformMode;
	xformMode = 0;
	snapIndicatorOn = false;
	releaseMouse();
	if ( commit ) {
		const bool pushed = commitTransformUndo(
			mode == 1 ? tr( "Move UVs" ) : mode == 2 ? tr( "Rotate UVs" ) : tr( "Scale UVs" ) );
		setStatus( tr( "Committed." ) );
		// arm the adjust-last-operation panel with the gesture's parameters,
		// like the 3D viewport's transform redo panel
		if ( pushed ) {
			// the commit bypasses reloadShapeUVs (already applied live), so
			// re-derive island welds here: seams moved apart must split
			QSet<int> touchedShapes;
			for ( const XVert & xv : std::as_const( xverts ) )
				touchedShapes << xv.shape;
			for ( int s : std::as_const( touchedShapes ) )
				if ( s >= 0 && s < shapes.size() )
					uvRebuildIslands( shapes[s] );
			lastOpXVerts = xverts;
			lastOpPivot = xformPivot;
			if ( mode == 1 )
				armOperatorPanel( 6, { xformLastDelta[0], xformLastDelta[1] } );
			else if ( mode == 2 )
				armOperatorPanel( 7, { rad2deg( xformLastAngle ) } );
			else
				armOperatorPanel( 8, { xformLastScale[0], xformLastScale[1] } );
		}
	} else {
		for ( XVert & xv : xverts )
			xv.current = xv.orig;
		writeLiveUVs();
		setStatus( tr( "Cancelled." ) );
	}
	xverts.clear();
	numericBuf.clear();
	update();
}

bool UVEditorView::applyUVEditUndoableShape( int s, const QHash<int, Vector2> & newUVs, const QString & name )
{
	if ( s < 0 || s >= shapes.size() || !nif || !nif->undoStack || newUVs.isEmpty() )
		return false;
	const UVShapeData & sd = shapes.at( s );
	if ( !sd.valid )
		return false;
	// Record old/new UVs and let one lightweight command apply them (its redo
	// writes here since the model is not pre-modified). Instant Undo, no reload.
	QVector<int> verts;
	QVector<Vector2> before, after;
	for ( auto it = newUVs.constBegin(); it != newUVs.constEnd(); ++it ) {
		const int v = it.key();
		if ( v < 0 || v >= sd.uvs.size() )
			continue;
		Vector2 uv = it.value();
		if ( constrainBounds )
			uv = Vector2( std::clamp( uv[0], 0.0f, 1.0f ), std::clamp( uv[1], 0.0f, 1.0f ) );
		if ( sd.uvs.at( v ) == uv )
			continue;
		verts << v;
		before << sd.uvs.at( v );
		after << uv;
	}
	if ( verts.isEmpty() )
		return false;
	// The command's redo() (run on push) writes the new UVs and emits
	// dataChanged, which refreshes our cached sd.uvs via reloadShapeUVs.
	nif->undoStack->push( new UVEditCommand( nif, sd.block, sd.legacyData,
		sd.iUVData, verts, before, after, /*alreadyApplied*/ false, name ) );
	update();
	return true;
}

// ---------------------------------------------------------------------------
// adjust last operation (Blender's operator redo panel)

void UVEditorView::armOperatorPanel( int kind, const QVector<float> & params )
{
	if ( opReapplying )
		return;
	lastOpKind = kind;
	lastOpParams = params;
	lastOpSeedVerts = selVerts;
	lastOpPushed = true;	// only armed right after a successful push
	lastOpUndoIndex = ( nif && nif->undoStack ) ? nif->undoStack->index() : -1;
	if ( operatorPanelCb )
		operatorPanelCb( kind, params );
}

void UVEditorView::cancelOperatorPanel()
{
	if ( opReapplying || lastOpKind == 0 )
		return;
	lastOpKind = 0;
	lastOpSeedVerts.clear();
	lastOpXVerts.clear();
	if ( operatorPanelCb )
		operatorPanelCb( 0, {} );
}

bool UVEditorView::reapplyUVOperator( const QVector<float> & params )
{
	if ( opReapplying || lastOpKind == 0 || !nif || !nif->undoStack || params.isEmpty() )
		return false;
	if ( nif->undoStack->index() != lastOpUndoIndex )
		return false;	// something else touched the undo stack — stale
	const int kind = lastOpKind;
	const float p0 = params.value( 0 );
	const float p1 = params.value( 1 );
	opReapplying = true;
	if ( lastOpPushed )
		nif->undoStack->undo();
	if ( kind == 4 ) {
		// the snapshot undo reloaded the whole model (modelReset -> clearData);
		// repopulate synchronously so the re-run sees the restored mesh
		rebuildFromViewport();
	} else if ( kind <= 5 ) {
		// re-run on the operator's original selection (Blender semantics);
		// transform re-runs (6-8) work from lastOpXVerts instead
		selVerts = lastOpSeedVerts;
		rebuildDerivedSelection();
		pushSelectionToViewport();
	}
	const int base = nif->undoStack->index();
	switch ( kind ) {
	case 1: mergeSelection( 2, std::max( p0, 0.0f ) ); break;
	case 2: minimizeStretch( int( p0 + 0.5f ) ); break;
	case 3: packIslands( p0 ); break;
	case 4: smartProject( p0 ); break;
	case 5: unwrapSelection( p0 ); break;
	case 6: case 7: case 8: {
		// transforms: recompute absolute targets from the gesture's original
		// UVs and pivot (after the undo above, the model holds those originals);
		// multi-mesh gestures re-apply per shape inside one macro
		QHash<int, QHash<int, Vector2>> editsByShape;
		const float c = std::cos( deg2rad( p0 ) ), s = std::sin( deg2rad( p0 ) );
		for ( const XVert & xv : std::as_const( lastOpXVerts ) ) {
			Vector2 t;
			if ( kind == 6 ) {
				t = xv.orig + Vector2( p0, p1 );
			} else if ( kind == 7 ) {
				Vector2 rel = xv.orig - lastOpPivot;
				t = lastOpPivot + Vector2( rel[0] * c - rel[1] * s, rel[0] * s + rel[1] * c );
			} else {
				Vector2 rel = xv.orig - lastOpPivot;
				t = lastOpPivot + Vector2( rel[0] * p0, rel[1] * p1 );
			}
			editsByShape[xv.shape].insert( xv.idx, t );
		}
		const QString name = kind == 6 ? tr( "Move UVs" )
			: kind == 7 ? tr( "Rotate UVs" ) : tr( "Scale UVs" );
		const bool macro = ( editsByShape.size() > 1 );
		if ( macro )
			nif->undoStack->beginMacro( name );
		for ( auto it = editsByShape.constBegin(); it != editsByShape.constEnd(); ++it )
			applyUVEditUndoableShape( it.key(), it.value(), name );
		if ( macro )
			nif->undoStack->endMacro();
	} break;
	}
	// a re-run may legitimately push nothing (e.g. merge distance too small);
	// remember that so the next adjustment doesn't undo an unrelated command
	lastOpKind = kind;	// clearData/rebuild during the re-run can't disarm (guarded), but be explicit
	lastOpPushed = ( nif->undoStack->index() != base );
	lastOpParams = params;
	lastOpUndoIndex = nif->undoStack->index();
	opReapplying = false;
	update();
	return true;
}

// ---------------------------------------------------------------------------
// input

void UVEditorView::enterEvent( QEnterEvent * e )
{
	// Only take focus if a text field isn't mid-edit, so hovering the canvas
	// never interrupts typing elsewhere.
	QWidget * fw = QApplication::focusWidget();
	const bool editingText = qobject_cast<QLineEdit *>( fw ) || qobject_cast<QAbstractSpinBox *>( fw );
	if ( !editingText )
		setFocus( Qt::MouseFocusReason );
	QOpenGLWidget::enterEvent( e );
}

void UVEditorView::mousePressEvent( QMouseEvent * e )
{
	setFocus( Qt::MouseFocusReason );
	const QPointF dp = devicePos( e );

	if ( xformMode != 0 ) {
		if ( e->button() == Qt::LeftButton )
			endTransform( true );
		else if ( e->button() == Qt::RightButton )
			endTransform( false );
		e->accept();
		return;
	}

	if ( e->button() == Qt::MiddleButton ) {
		panning = true;
		panLastPx = dp;
		setCursor( Qt::ClosedHandCursor );
		e->accept();
		return;
	}

	if ( e->button() == Qt::RightButton && e->modifiers().testFlag( Qt::ShiftModifier ) ) {
		placeCursor( e->position() );
		e->accept();
		return;
	}

	if ( e->button() == Qt::LeftButton ) {
		boxPending = true;
		boxDrag = false;
		boxStartPx = dp;
		boxCurPx = dp;
		boxMods = e->modifiers();
		e->accept();
		return;
	}

	QOpenGLWidget::mousePressEvent( e );
}

void UVEditorView::mouseMoveEvent( QMouseEvent * e )
{
	const QPointF dp = devicePos( e );

	if ( xformMode != 0 ) {
		updateTransform( dp, e->modifiers() );
		e->accept();
		return;
	}

	if ( panning ) {
		QPointF d = dp - panLastPx;
		panLastPx = dp;
		viewPos[0] -= d.x() / double( std::max( pixelWidth, 1 ) ) * double( viewScaleAndOffset[0] );
		viewPos[1] -= d.y() / double( std::max( pixelHeight, 1 ) ) * double( viewScaleAndOffset[1] );
		updateViewMapping();
		update();
		e->accept();
		return;
	}

	if ( boxPending ) {
		boxCurPx = dp;
		if ( !boxDrag ) {
			QPointF d = dp - boxStartPx;
			if ( std::hypot( d.x(), d.y() ) > 4.0 * devicePixelRatioF() )
				boxDrag = true;
		}
		if ( boxDrag )
			update();
		e->accept();
		return;
	}

	QOpenGLWidget::mouseMoveEvent( e );
}

void UVEditorView::mouseReleaseEvent( QMouseEvent * e )
{
	const QPointF dp = devicePos( e );

	if ( panning && e->button() == Qt::MiddleButton ) {
		panning = false;
		setCursor( Qt::CrossCursor );
		e->accept();
		return;
	}

	if ( boxPending && e->button() == Qt::LeftButton ) {
		boxPending = false;
		if ( boxDrag ) {
			boxDrag = false;
			QRectF rect( QPointF( std::min( boxStartPx.x(), dp.x() ), std::min( boxStartPx.y(), dp.y() ) ),
				QPointF( std::max( boxStartPx.x(), dp.x() ), std::max( boxStartPx.y(), dp.y() ) ) );
			applyBoxSelect( rect, boxMods );
		} else {
			pickAt( dp, boxMods.testFlag( Qt::ShiftModifier ) );
		}
		e->accept();
		return;
	}

	QOpenGLWidget::mouseReleaseEvent( e );
}

void UVEditorView::wheelEvent( QWheelEvent * e )
{
	const double steps = e->angleDelta().y() / 120.0;
	if ( steps == 0.0 ) {
		e->accept();
		return;
	}
	// zoom about the cursor position so the point under the mouse stays put
	QPointF dp = e->position() * devicePixelRatioF();
	Vector2 before = mapToUV( dp );
	zoom = std::clamp( zoom * std::pow( 0.8, steps ), 0.02, 20.0 );
	updateViewMapping();
	Vector2 after = mapToUV( dp );
	viewPos[0] += double( before[0] ) - double( after[0] );
	viewPos[1] += double( before[1] ) - double( after[1] );
	updateViewMapping();
	update();
	e->accept();
}

void UVEditorView::keyPressEvent( QKeyEvent * e )
{
	const int key = e->key();
	const Qt::KeyboardModifiers mods = e->modifiers();

	if ( xformMode != 0 ) {
		if ( key == Qt::Key_Escape ) {
			endTransform( false );
		} else if ( key == Qt::Key_Return || key == Qt::Key_Enter ) {
			endTransform( true );
		} else if ( key == Qt::Key_X ) {
			xformAxis = ( xformAxis == 1 ) ? 0 : 1;
			updateTransform( xformLastPx, mods );
		} else if ( key == Qt::Key_Y ) {
			xformAxis = ( xformAxis == 2 ) ? 0 : 2;
			updateTransform( xformLastPx, mods );
		} else if ( key == Qt::Key_Backspace ) {
			numericBuf.chop( 1 );
			updateTransform( xformLastPx, mods );
		} else if ( key == Qt::Key_Minus ) {
			if ( numericBuf.startsWith( '-' ) ) numericBuf.remove( 0, 1 );
			else numericBuf.prepend( '-' );
			updateTransform( xformLastPx, mods );
		} else if ( ( key >= Qt::Key_0 && key <= Qt::Key_9 ) || key == Qt::Key_Period ) {
			numericBuf += ( key == Qt::Key_Period ) ? QChar( '.' ) : QChar( '0' + ( key - Qt::Key_0 ) );
			updateTransform( xformLastPx, mods );
		}
		e->accept();
		return;
	}

	switch ( key ) {
	case Qt::Key_G:
		beginTransform( 1 );
		break;
	case Qt::Key_R:
		beginTransform( 2 );
		break;
	case Qt::Key_S:
		if ( mods.testFlag( Qt::ShiftModifier ) )
			showSnapMenu( QCursor::pos() );
		else
			beginTransform( 3 );
		break;
	case Qt::Key_A:
		// Alt+A always deselects; plain A toggles select-all/deselect-all so a
		// second press clears the selection (matches the 3D viewport's A).
		if ( mods.testFlag( Qt::AltModifier ) )
			selectAllUV( 2 );
		else if ( mods == Qt::NoModifier )
			selectAllUV( 0 );
		break;
	case Qt::Key_I:
		if ( mods.testFlag( Qt::ControlModifier ) )
			invertSelection();
		break;
	case Qt::Key_L:
		if ( mods.testFlag( Qt::ControlModifier ) )
			growSelectionToLinked();
		else
			selectLinkedUnderCursor( true );
		break;
	case Qt::Key_1:
		setSelectMode( 1, false );
		break;
	case Qt::Key_2:
		setSelectMode( 2, false );
		break;
	case Qt::Key_3:
		setSelectMode( 3, false );
		break;
	case Qt::Key_4:
		setSelectMode( 4, false );
		break;
	case Qt::Key_U:
		showUnwrapMenu( QCursor::pos() );
		break;
	case Qt::Key_P:
		// Blender's UV pin: P pins the selection, Alt+P unpins it
		if ( mods.testFlag( Qt::AltModifier ) )
			pinSelected( false );
		else if ( mods == Qt::NoModifier )
			pinSelected( true );
		break;
	case Qt::Key_V:
		if ( mods == Qt::NoModifier )
			stitchSelection();                            // Blender UV Stitch
		break;
	case Qt::Key_Y:
		if ( mods == Qt::NoModifier )
			ripSelectedFaces();                           // Rip / Split (topology)
		break;
	case Qt::Key_M:
		if ( mods.testFlag( Qt::ControlModifier ) )
			showTransformMenu( QCursor::pos() );      // Ctrl+M mirror/align
		else
			showMergeMenu( QCursor::pos() );          // M merge
		break;
	case Qt::Key_H:
		if ( mods.testFlag( Qt::AltModifier ) )
			unhideAllFaces();
		else if ( mods == Qt::NoModifier )
			hideSelectedFaces();
		break;
	case Qt::Key_Period:
		frameSelected();
		break;
	case Qt::Key_Home:
		frameAll();
		break;
	case Qt::Key_C:
		if ( mods.testFlag( Qt::ShiftModifier ) ) {
			setCursorUV( Vector2( 0.0f, 0.0f ) );
			frameAll();
		}
		break;
	default:
		QOpenGLWidget::keyPressEvent( e );
		return;
	}
	e->accept();
}

void UVEditorView::contextMenuEvent( QContextMenuEvent * e )
{
	if ( xformMode != 0 || ( e->modifiers() & Qt::ShiftModifier ) ) {
		e->accept();
		return;
	}
	// remember where the click landed so "Place 2D Cursor Here" is exact
	const QPointF clickWidgetPos = e->pos();
	QMenu menu( this );
	// Blender's 2D cursor is placed from the header/right-click; make it the
	// first action so it is the default primary command
	QAction * placeCursorAction = menu.addAction( tr( "Place 2D Cursor Here" ) );
	menu.addSeparator();
	QAction * frameSel = menu.addAction( tr( "Frame Selected\t." ) );
	QAction * frameAllA = menu.addAction( tr( "Frame All\tHome" ) );
	menu.addSeparator();
	QAction * snapMenuA = menu.addAction( tr( "Snap…\tShift+S" ) );
	menu.addSeparator();
	QMenu * ops = menu.addMenu( tr( "UVs" ) );
	QAction * mergeA = ops->addAction( tr( "Merge…\tM" ) );
	QAction * mirrorA = ops->addAction( tr( "Mirror / Align…\tCtrl+M" ) );
	QAction * unwrapA = ops->addAction( tr( "Unwrap / Project…\tU" ) );
	QAction * layoutA = ops->addAction( tr( "Layout Tools…" ) );
	QAction * roundA = ops->addAction( tr( "Round to Pixels" ) );
	ops->addSeparator();
	QAction * pinA = ops->addAction( tr( "Pin Selected\tP" ) );
	QAction * unpinA = ops->addAction( tr( "Unpin Selected\tAlt+P" ) );
	QAction * invPinA = ops->addAction( tr( "Invert Pins" ) );
	QAction * unwrapPinA = ops->addAction( tr( "Unwrap (Live, Pinned)" ) );
	unwrapPinA->setEnabled( !pinnedVerts.isEmpty() );
	ops->addSeparator();
	QAction * ripA = ops->addAction( tr( "Rip / Split Selection\tY" ) );
	QAction * stitchA = ops->addAction( tr( "Stitch (weld seams)\tV" ) );
	ops->addSeparator();
	QAction * hideA = ops->addAction( tr( "Hide Selected Faces\tH" ) );
	QAction * unhideA = ops->addAction( tr( "Reveal All Faces\tAlt+H" ) );
	ops->setEnabled( !objectModeView );
	menu.addSeparator();
	QAction * exportA = menu.addAction( tr( "Export UV Layout to PNG…" ) );
	menu.addSeparator();
	QAction * selAll = menu.addAction( tr( "Select All\tA" ) );
	QAction * selNone = menu.addAction( tr( "Deselect All\tAlt+A" ) );
	QAction * selInv = menu.addAction( tr( "Invert Selection\tCtrl+I" ) );
	for ( QAction * a : { selAll, selNone, selInv } )
		a->setEnabled( !objectModeView );
	QAction * chosen = menu.exec( e->globalPos() );
	if ( chosen == placeCursorAction ) placeCursor( clickWidgetPos );
	else if ( chosen == frameSel ) frameSelected();
	else if ( chosen == frameAllA ) frameAll();
	else if ( chosen == snapMenuA ) showSnapMenu( e->globalPos() );
	else if ( chosen == mergeA ) showMergeMenu( e->globalPos() );
	else if ( chosen == mirrorA ) showTransformMenu( e->globalPos() );
	else if ( chosen == unwrapA ) showUnwrapMenu( e->globalPos() );
	else if ( chosen == layoutA ) showLayoutMenu( e->globalPos() );
	else if ( chosen == roundA ) roundSelectionToPixels();
	else if ( chosen == pinA ) pinSelected( true );
	else if ( chosen == unpinA ) pinSelected( false );
	else if ( chosen == invPinA ) invertPins();
	else if ( chosen == unwrapPinA ) unwrapWithPins();
	else if ( chosen == ripA ) ripSelectedFaces();
	else if ( chosen == stitchA ) stitchSelection();
	else if ( chosen == hideA ) hideSelectedFaces();
	else if ( chosen == unhideA ) unhideAllFaces();
	else if ( chosen == exportA ) exportUVLayout();
	else if ( chosen == selAll ) selectAllUV( 1 );
	else if ( chosen == selNone ) selectAllUV( 2 );
	else if ( chosen == selInv ) invertSelection();
	e->accept();
}

// ---------------------------------------------------------------------------
// dock factory

QDockWidget * tlCreateUVManagerDock( NifModel * nif, QMainWindow * mw, GLView * ogl )
{
	auto * dock = new QDockWidget( QObject::tr( "UV Editor" ), mw );
	dock->setObjectName( QStringLiteral( "UVManagerDock" ) );
	dock->setAllowedAreas( Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea );

	auto * panel = new QWidget( dock );
	auto * layout = new QVBoxLayout( panel );
	layout->setContentsMargins( 4, 4, 4, 4 );
	layout->setSpacing( 3 );

	auto * view = new UVEditorView( panel );
	view->nif = nif;
	view->ogl = ogl;

	QSettings settings;
	view->snapOn = settings.value( "UVEditor/Snap/Enabled", false ).toBool();
	view->snapTarget = settings.value( "UVEditor/Snap/Target", 0 ).toInt();
	view->snapBase = settings.value( "UVEditor/Snap/Base", 0 ).toInt();
	view->snapMove = settings.value( "UVEditor/Snap/AffectMove", true ).toBool();
	view->snapRotate = settings.value( "UVEditor/Snap/AffectRotate", true ).toBool();
	view->snapScale = settings.value( "UVEditor/Snap/AffectScale", true ).toBool();
	view->rotIncrement = settings.value( "UVEditor/Snap/RotationIncrement", 5.0 ).toFloat();
	view->gridStep = settings.value( "UVEditor/Snap/GridStep", 0.0625 ).toFloat();
	view->pivotMode = settings.value( "UVEditor/Pivot", 0 ).toInt();
	view->syncSelection = settings.value( "UVEditor/SyncSelection", true ).toBool();
	view->stickySelection = settings.value( "UVEditor/StickySelection", true ).toBool();
	view->snapTarget = std::clamp( view->snapTarget, 0, 3 );
	view->repeatImage = settings.value( "UVEditor/RepeatImage", false ).toBool();
	view->showPixelGrid = settings.value( "UVEditor/PixelGrid", false ).toBool();
	view->constrainBounds = settings.value( "UVEditor/ConstrainBounds", false ).toBool();

	// ---- settings bar, row 1: modes / pivot / snap ----
	auto * bar1 = new QHBoxLayout;
	bar1->setSpacing( 2 );

	// Blender's UV Sync Selection toggle (leftmost, like Blender's header)
	auto * sync = new QToolButton( panel );
	sync->setText( QStringLiteral( "⇄" ) );
	sync->setCheckable( true );
	sync->setChecked( view->syncSelection );
	sync->setToolTip( QObject::tr(
		"UV Sync Selection. On: selection mirrors the 3D viewport both ways.\n"
		"Off: UV selection is independent, and only faces selected in the 3D viewport are shown." ) );
	QObject::connect( sync, &QToolButton::toggled, view,
		[view]( bool on ) { view->setSyncSelection( on ); } );
	bar1->addWidget( sync );

	// Blender's Sticky Selection (Shared Location)
	auto * sticky = new QToolButton( panel );
	sticky->setText( QObject::tr( "Sticky" ) );
	sticky->setCheckable( true );
	sticky->setChecked( view->stickySelection );
	sticky->setToolTip( QObject::tr(
		"Sticky selection (Shared Location). UVs of the same mesh position that sit\n"
		"at the same UV spot select and move together, so merged seams behave as one\n"
		"point. Turn off to pull coincident UVs apart individually." ) );
	QObject::connect( sticky, &QToolButton::toggled, view, [view]( bool on ) {
		view->stickySelection = on;
		QSettings().setValue( "UVEditor/StickySelection", on );
	} );
	bar1->addWidget( sticky );
	bar1->addSpacing( 6 );

	const QStringList modeNames = {
		QObject::tr( "Vertex" ), QObject::tr( "Edge" ), QObject::tr( "Face" ), QObject::tr( "Island" )
	};
	QVector<QToolButton *> modeButtons;
	for ( int m = 0; m < 4; m++ ) {
		auto * b = new QToolButton( panel );
		b->setText( modeNames.at( m ) );
		b->setCheckable( true );
		b->setChecked( m == 0 );
		b->setToolTip( QObject::tr( "Selection mode (%1)" ).arg( m + 1 ) );
		b->setAutoRaise( true );
		bar1->addWidget( b );
		modeButtons << b;
		QObject::connect( b, &QToolButton::clicked, view,
			[view, m]() { view->setSelectMode( m + 1, false ); } );
	}
	view->modeChangedCb = [modeButtons]( int mode ) {
		for ( int m = 0; m < modeButtons.size(); m++ ) {
			QSignalBlocker blocker( modeButtons.at( m ) );
			modeButtons.at( m )->setChecked( mode == m + 1 );
		}
	};

	bar1->addSpacing( 8 );
	bar1->addWidget( new QLabel( QObject::tr( "Pivot" ), panel ) );
	auto * pivot = new QComboBox( panel );
	pivot->addItems( { QObject::tr( "Bounding Box Center" ), QObject::tr( "Median" ), QObject::tr( "2D Cursor" ) } );
	pivot->setCurrentIndex( std::clamp( view->pivotMode, 0, 2 ) );
	QObject::connect( pivot, &QComboBox::currentIndexChanged, view, [view]( int index ) {
		view->pivotMode = index;
		QSettings().setValue( "UVEditor/Pivot", index );
	} );
	bar1->addWidget( pivot );

	bar1->addSpacing( 8 );
	auto * magnet = new QToolButton( panel );
	magnet->setText( QObject::tr( "Snap" ) );
	magnet->setCheckable( true );
	magnet->setChecked( view->snapOn );
	magnet->setToolTip( QObject::tr( "Snapping on/off (Ctrl inverts during a transform)" ) );
	QObject::connect( magnet, &QToolButton::toggled, view, [view]( bool on ) {
		view->snapOn = on;
		QSettings().setValue( "UVEditor/Snap/Enabled", on );
	} );
	bar1->addWidget( magnet );

	// snap settings popover (Blender's magnet dropdown)
	auto * snapMenuButton = new QToolButton( panel );
	snapMenuButton->setText( QStringLiteral( "▾" ) );
	snapMenuButton->setPopupMode( QToolButton::InstantPopup );
	snapMenuButton->setToolTip( QObject::tr( "Snap settings" ) );
	auto * snapMenu = new QMenu( snapMenuButton );
	{
		auto * host = new QWidget( snapMenu );
		auto * grid = new QVBoxLayout( host );
		grid->setContentsMargins( 10, 8, 10, 8 );
		grid->setSpacing( 5 );

		grid->addWidget( new QLabel( QObject::tr( "Snap Target" ), host ) );
		auto * target = new QComboBox( host );
		target->addItems( { QObject::tr( "Increment" ), QObject::tr( "Grid" ),
			QObject::tr( "Vertex" ), QObject::tr( "Pixel" ) } );
		target->setCurrentIndex( std::clamp( view->snapTarget, 0, 3 ) );
		QObject::connect( target, &QComboBox::currentIndexChanged, view, [view]( int index ) {
			view->snapTarget = index;
			QSettings().setValue( "UVEditor/Snap/Target", index );
		} );
		grid->addWidget( target );

		grid->addWidget( new QLabel( QObject::tr( "Snap Base" ), host ) );
		auto * base = new QComboBox( host );
		base->addItems( { QObject::tr( "Closest" ), QObject::tr( "Center" ), QObject::tr( "Median" ), QObject::tr( "Active" ) } );
		base->setCurrentIndex( std::clamp( view->snapBase, 0, 3 ) );
		QObject::connect( base, &QComboBox::currentIndexChanged, view, [view]( int index ) {
			view->snapBase = index;
			QSettings().setValue( "UVEditor/Snap/Base", index );
		} );
		grid->addWidget( base );

		grid->addWidget( new QLabel( QObject::tr( "Affect" ), host ) );
		auto * affectRow = new QHBoxLayout;
		const char * affectKeys[3] = { "UVEditor/Snap/AffectMove", "UVEditor/Snap/AffectRotate", "UVEditor/Snap/AffectScale" };
		const QString affectNames[3] = { QObject::tr( "Move" ), QObject::tr( "Rotate" ), QObject::tr( "Scale" ) };
		bool * affectFlags[3] = { &view->snapMove, &view->snapRotate, &view->snapScale };
		for ( int i = 0; i < 3; i++ ) {
			auto * cb = new QCheckBox( affectNames[i], host );
			cb->setChecked( *affectFlags[i] );
			bool * flag = affectFlags[i];
			const char * settingsKey = affectKeys[i];
			QObject::connect( cb, &QCheckBox::toggled, view, [flag, settingsKey]( bool on ) {
				*flag = on;
				QSettings().setValue( QLatin1String( settingsKey ), on );
			} );
			affectRow->addWidget( cb );
		}
		grid->addLayout( affectRow );

		auto * rotRow = new QHBoxLayout;
		rotRow->addWidget( new QLabel( QObject::tr( "Rotation Increment" ), host ) );
		auto * rotSpin = new QDoubleSpinBox( host );
		rotSpin->setRange( 0.1, 90.0 );
		rotSpin->setDecimals( 1 );
		rotSpin->setSuffix( QStringLiteral( "°" ) );
		rotSpin->setValue( view->rotIncrement );
		QObject::connect( rotSpin, &QDoubleSpinBox::valueChanged, view, [view]( double value ) {
			view->rotIncrement = float( value );
			QSettings().setValue( "UVEditor/Snap/RotationIncrement", value );
		} );
		rotRow->addWidget( rotSpin );
		grid->addLayout( rotRow );

		auto * stepRow = new QHBoxLayout;
		stepRow->addWidget( new QLabel( QObject::tr( "Grid Step" ), host ) );
		auto * stepSpin = new QDoubleSpinBox( host );
		stepSpin->setRange( 0.0009765625, 1.0 );
		stepSpin->setDecimals( 6 );
		stepSpin->setSingleStep( 0.03125 );
		stepSpin->setValue( view->gridStep );
		QObject::connect( stepSpin, &QDoubleSpinBox::valueChanged, view, [view]( double value ) {
			view->gridStep = float( value );
			QSettings().setValue( "UVEditor/Snap/GridStep", value );
		} );
		stepRow->addWidget( stepSpin );
		grid->addLayout( stepRow );

		auto * hostAction = new QWidgetAction( snapMenu );
		hostAction->setDefaultWidget( host );
		snapMenu->addAction( hostAction );
	}
	snapMenuButton->setMenu( snapMenu );
	bar1->addWidget( snapMenuButton );

	bar1->addSpacing( 8 );
	auto * unwrapButton = new QToolButton( panel );
	unwrapButton->setText( QObject::tr( "Unwrap ▾" ) );
	unwrapButton->setToolTip( QObject::tr( "Unwrap the selected geometry (U)" ) );
	QObject::connect( unwrapButton, &QToolButton::clicked, view, [view, unwrapButton]() {
		view->showUnwrapMenu( unwrapButton->mapToGlobal( QPoint( 0, unwrapButton->height() ) ) );
	} );
	bar1->addWidget( unwrapButton );

	bar1->addStretch( 1 );
	auto * frameButton = new QToolButton( panel );
	frameButton->setText( QObject::tr( "Frame" ) );
	frameButton->setToolTip( QObject::tr( "Frame all UVs (Home); '.' frames the selection" ) );
	QObject::connect( frameButton, &QToolButton::clicked, view, [view]() { view->frameAll(); } );
	bar1->addWidget( frameButton );
	layout->addLayout( bar1 );

	// ---- settings bar, row 2: underlay / cursor ----
	auto * bar2 = new QHBoxLayout;
	bar2->setSpacing( 4 );
	bar2->addWidget( new QLabel( QObject::tr( "Image" ), panel ) );
	auto * underlay = new QComboBox( panel );
	underlay->setSizeAdjustPolicy( QComboBox::AdjustToMinimumContentsLengthWithIcon );
	underlay->setMinimumContentsLength( 18 );
	bar2->addWidget( underlay, 1 );
	auto * browse = new QToolButton( panel );
	browse->setText( QObject::tr( "..." ) );
	browse->setToolTip( QObject::tr( "Open any image as the underlay (for retargeting to another atlas)" ) );
	bar2->addWidget( browse );
	auto * alpha = new QToolButton( panel );
	alpha->setText( QObject::tr( "α" ) );
	alpha->setCheckable( true );
	alpha->setChecked( view->textureAlpha );
	alpha->setToolTip( QObject::tr( "Texture alpha blending" ) );
	QObject::connect( alpha, &QToolButton::toggled, view, [view]( bool on ) {
		view->textureAlpha = on;
		view->update();
	} );
	bar2->addWidget( alpha );

	auto * repeat = new QToolButton( panel );
	repeat->setText( QObject::tr( "Repeat" ) );
	repeat->setCheckable( true );
	repeat->setChecked( view->repeatImage );
	repeat->setToolTip( QObject::tr(
		"Repeat the image and grid across every UV tile.\n"
		"Off (default): show only the 0-1 tile, dark outside, like Blender." ) );
	QObject::connect( repeat, &QToolButton::toggled, view, [view]( bool on ) {
		view->repeatImage = on;
		QSettings().setValue( "UVEditor/RepeatImage", on );
		view->update();
	} );
	bar2->addWidget( repeat );

	auto * pixels = new QToolButton( panel );
	pixels->setText( QObject::tr( "Pixels" ) );
	pixels->setCheckable( true );
	pixels->setChecked( view->showPixelGrid );
	pixels->setToolTip( QObject::tr(
		"Show a subtle grid at the underlay texture's pixel boundaries\n"
		"(appears when zoomed in enough). Snap Target > Pixel snaps to it." ) );
	QObject::connect( pixels, &QToolButton::toggled, view, [view]( bool on ) {
		view->showPixelGrid = on;
		QSettings().setValue( "UVEditor/PixelGrid", on );
		view->update();
	} );
	bar2->addWidget( pixels );

	auto * bounds = new QToolButton( panel );
	bounds->setText( QObject::tr( "Bounds" ) );
	bounds->setCheckable( true );
	bounds->setChecked( view->constrainBounds );
	bounds->setToolTip( QObject::tr(
		"Constrain to Image Bounds: clamp UV edits into the 0-1 tile." ) );
	QObject::connect( bounds, &QToolButton::toggled, view, [view]( bool on ) {
		view->constrainBounds = on;
		QSettings().setValue( "UVEditor/ConstrainBounds", on );
	} );
	bar2->addWidget( bounds );

	bar2->addSpacing( 8 );
	bar2->addWidget( new QLabel( QObject::tr( "UV Map" ), panel ) );
	auto * uvSetCombo = new QComboBox( panel );
	uvSetCombo->setToolTip( QObject::tr(
		"UV coordinate set. FO4 BSTriShape stores exactly one UV channel;\n"
		"multiple sets exist only on legacy (NiTriShape-era) meshes." ) );
	uvSetCombo->addItem( QObject::tr( "UV 1" ) );
	uvSetCombo->setEnabled( false );
	QObject::connect( uvSetCombo, &QComboBox::activated, view,
		[view]( int index ) { view->setUVSet( index ); } );
	bar2->addWidget( uvSetCombo );

	bar2->addSpacing( 8 );
	bar2->addWidget( new QLabel( QObject::tr( "Cursor" ), panel ) );
	auto * cursorU = new QDoubleSpinBox( panel );
	auto * cursorV = new QDoubleSpinBox( panel );
	for ( QDoubleSpinBox * spin : { cursorU, cursorV } ) {
		spin->setRange( -64.0, 64.0 );
		spin->setDecimals( 4 );
		spin->setSingleStep( 0.01 );
		bar2->addWidget( spin );
	}
	cursorU->setValue( view->cursor2D[0] );
	cursorV->setValue( view->cursor2D[1] );
	auto cursorEdited = [view, cursorU, cursorV]() {
		view->setCursorUV( Vector2( float( cursorU->value() ), float( cursorV->value() ) ) );
	};
	QObject::connect( cursorU, &QDoubleSpinBox::valueChanged, view, cursorEdited );
	QObject::connect( cursorV, &QDoubleSpinBox::valueChanged, view, cursorEdited );
	view->cursorMovedCb = [view, cursorU, cursorV]() {
		QSignalBlocker bu( cursorU );
		QSignalBlocker bv( cursorV );
		cursorU->setValue( view->cursor2D[0] );
		cursorV->setValue( view->cursor2D[1] );
	};
	layout->addLayout( bar2 );

	// ---- canvas + status ----
	layout->addWidget( view, 1 );
	auto * status = new QLabel( QObject::tr( "Enter Edit Mode on a mesh to edit its UVs." ), panel );
	status->setObjectName( QStringLiteral( "UVEditorStatus" ) );
	status->setWordWrap( true );
	layout->addWidget( status );
	view->statusCb = [status]( const QString & s ) { status->setText( s ); };
	view->selectionInfoCb = [view, status]() {
		if ( view->hasData() && view->selectedCount() > 0 )
			status->setText( QObject::tr( "%1 UVs selected" ).arg( view->selectedCount() ) );
	};

	// ---- adjust-last-operation panel (Blender's operator redo panel) ----
	// Overlay in the canvas' bottom-left corner; appears after a parameterized
	// operator (Merge by Distance, Unwrap, Pack, …) or a transform gesture
	// (G/R/S) and re-runs it live as the values are edited. Same design, colors
	// and DragSpinBox scrubbing as the 3D viewport's redo panels.
	auto * opPanel = new QFrame( view );
	opPanel->setObjectName( QStringLiteral( "UVOperatorPanel" ) );
	opPanel->setFrameShape( QFrame::StyledPanel );
	opPanel->setAutoFillBackground( true );
	opPanel->setStyleSheet( QStringLiteral(
		"QFrame#UVOperatorPanel {"
		" background: #2f2f2f; border: 1px solid #202020; }"
		"QLabel { color: #cccccc; background: transparent; }"
		"QToolButton { color: #cccccc; background: transparent; border: none; }"
		"QToolButton:hover { color: #ffffff; }" ) );
	opPanel->hide();

	auto * opOuter = new QVBoxLayout( opPanel );
	opOuter->setContentsMargins( 10, 8, 10, 8 );
	opOuter->setSpacing( 4 );
	auto * opHdr = new QHBoxLayout();
	auto * opTitle = new QToolButton( opPanel );
	opTitle->setAutoRaise( true );
	QFont opFont = opTitle->font();
	opFont.setBold( true );
	opTitle->setFont( opFont );
	auto * opClose = new QToolButton( opPanel );
	opClose->setText( QStringLiteral( "✕" ) );
	opClose->setAutoRaise( true );
	opClose->setToolTip( QObject::tr( "Keep the result and close" ) );
	opHdr->addWidget( opTitle );
	opHdr->addStretch( 1 );
	opHdr->addWidget( opClose );
	auto * opBody = new QWidget( opPanel );
	auto * opGrid = new QGridLayout( opBody );
	opGrid->setContentsMargins( 0, 0, 0, 0 );
	opGrid->setHorizontalSpacing( 8 );
	opGrid->setVerticalSpacing( 3 );
	opOuter->addLayout( opHdr );
	opOuter->addWidget( opBody );

	QLabel * opLbls[2] = { new QLabel( opBody ), new QLabel( opBody ) };
	UVDragSpinBox * opVals[2] = { new UVDragSpinBox( opBody ), new UVDragSpinBox( opBody ) };
	for ( int i = 0; i < 2; i++ ) {
		opLbls[i]->setAlignment( Qt::AlignRight | Qt::AlignVCenter );
		opVals[i]->setRange( -1.0e6, 1.0e6 );
		opVals[i]->setDecimals( 4 );
		opVals[i]->setKeyboardTracking( false );
		opVals[i]->setMinimumWidth( 150 );
		opGrid->addWidget( opLbls[i], i, 0 );
		opGrid->addWidget( opVals[i], i, 1, 1, 2 );
	}

	auto positionOpPanel = [view, opPanel]() {
		opPanel->adjustSize();
		opPanel->resize( opPanel->sizeHint() );
		opPanel->move( 10, view->height() - opPanel->height() - 10 );
	};
	view->resizedCb = positionOpPanel;
	QObject::connect( opClose, &QToolButton::clicked, opPanel, &QWidget::hide );
	QObject::connect( opTitle, &QToolButton::clicked, opPanel,
		[opPanel, opTitle, opBody, positionOpPanel]() {
			uvTogglePanelCollapse( opPanel, opTitle, opBody );
			positionOpPanel();
		} );

	// per-kind panel spec: title + one or two value rows
	struct UVOpRow { const char * label; int decimals; double mn, mx, step; const char * suffix; };
	struct UVOpSpec { const char * title; UVOpRow rows[2]; int rowCount; };
	static const QHash<int, UVOpSpec> opSpecs = {
		{ 1, { QT_TR_NOOP( "Merge by Distance" ), { { QT_TR_NOOP( "Distance" ), 4, 0.0, 1.0, 0.01, "" } }, 1 } },
		{ 2, { QT_TR_NOOP( "Minimize Stretch" ), { { QT_TR_NOOP( "Iterations" ), 0, 1.0, 200.0, 5.0, "" } }, 1 } },
		{ 3, { QT_TR_NOOP( "Pack Islands" ), { { QT_TR_NOOP( "Margin" ), 3, 0.0, 0.25, 0.01, "" } }, 1 } },
		{ 4, { QT_TR_NOOP( "Smart UV Project" ), { { QT_TR_NOOP( "Angle Limit" ), 1, 1.0, 89.0, 1.0, "°" } }, 1 } },
		{ 5, { QT_TR_NOOP( "Unwrap" ), { { QT_TR_NOOP( "Margin" ), 3, 0.0, 0.25, 0.01, "" } }, 1 } },
		{ 6, { QT_TR_NOOP( "Move" ), { { QT_TR_NOOP( "Move U" ), 4, -1.0e6, 1.0e6, 0.01, "" },
			{ QT_TR_NOOP( "V" ), 4, -1.0e6, 1.0e6, 0.01, "" } }, 2 } },
		{ 7, { QT_TR_NOOP( "Rotate" ), { { QT_TR_NOOP( "Angle°" ), 2, -1.0e6, 1.0e6, 1.0, "" } }, 1 } },
		{ 8, { QT_TR_NOOP( "Scale" ), { { QT_TR_NOOP( "Scale U" ), 4, -1.0e6, 1.0e6, 0.01, "" },
			{ QT_TR_NOOP( "V" ), 4, -1.0e6, 1.0e6, 0.01, "" } }, 2 } },
	};

	view->operatorPanelCb = [opPanel, opTitle, opLbl0 = opLbls[0], opLbl1 = opLbls[1],
			opVal0 = opVals[0], opVal1 = opVals[1], positionOpPanel]( int kind, const QVector<float> & params ) {
		if ( kind == 0 || !opSpecs.contains( kind ) ) {
			opPanel->hide();
			return;
		}
		const UVOpSpec & spec = opSpecs[kind];
		QLabel * lbls[2] = { opLbl0, opLbl1 };
		UVDragSpinBox * vals[2] = { opVal0, opVal1 };
		for ( QWidget * w : opPanel->findChildren<QWidget *>() )
			w->setEnabled( true );	// a stale gesture froze them
		for ( int i = 0; i < 2; i++ ) {
			QSignalBlocker blocker( vals[i] );
			const bool on = ( i < spec.rowCount );
			lbls[i]->setVisible( on );
			vals[i]->setVisible( on );
			if ( !on )
				continue;
			const UVOpRow & row = spec.rows[i];
			lbls[i]->setText( QObject::tr( row.label ) );
			vals[i]->setSuffix( QString::fromUtf8( row.suffix ) );
			vals[i]->setDecimals( row.decimals );
			vals[i]->setRange( row.mn, row.mx );
			vals[i]->setSingleStep( row.step );
			vals[i]->setValue( double( params.value( i ) ) );
		}
		uvSetPanelTitle( opPanel, opTitle, QObject::tr( spec.title ) );
		positionOpPanel();
		opPanel->show();
		opPanel->raise();
	};
	auto applyOpEdit = [view, opPanel, opVal0 = opVals[0], opVal1 = opVals[1]]() {
		if ( !opPanel->isVisible() )
			return;
		if ( !view->reapplyUVOperator( { float( opVal0->value() ), float( opVal1->value() ) } ) ) {
			// gesture went stale (something else touched the undo stack): keep
			// the panel visible but freeze its inputs, like the 3D redo panels
			// (the title and close buttons stay usable)
			for ( QWidget * w : opPanel->findChildren<QWidget *>() )
				if ( !w->inherits( "QToolButton" ) )
					w->setEnabled( false );
		}
	};
	for ( UVDragSpinBox * sb : opVals )
		QObject::connect( sb, qOverload<double>( &QDoubleSpinBox::valueChanged ), view, applyOpEdit );

	auto refreshUnderlayCombo = [view, underlay]() {
		QSignalBlocker blocker( underlay );
		underlay->clear();
		underlay->addItem( QObject::tr( "Checker / None" ) );
		for ( const UVTexSlot & slot : std::as_const( view->texSlots ) )
			underlay->addItem( slot.label );
		if ( !view->customUnderlay.isEmpty() ) {
			underlay->addItem( QObject::tr( "Custom — %1" ).arg( QFileInfo( view->customUnderlay ).fileName() ) );
			underlay->setCurrentIndex( underlay->count() - 1 );
		} else {
			underlay->setCurrentIndex( view->underlaySlot + 1 );
		}
	};
	QObject::connect( underlay, &QComboBox::activated, view, [view]( int index ) {
		view->customUnderlay.clear();
		view->underlaySlot = index - 1;
		view->update();
	} );
	QObject::connect( browse, &QToolButton::clicked, view, [view, refreshUnderlayCombo]() {
		QString file = QFileDialog::getOpenFileName( view, QObject::tr( "Open Underlay Image" ),
			QString(), QObject::tr( "Images (*.dds *.png *.tga *.bmp *.jpg)" ) );
		if ( file.isEmpty() )
			return;
		view->customUnderlay = file;
		refreshUnderlayCombo();
		view->update();
	} );

	auto refreshUVSetCombo = [view, uvSetCombo]() {
		QSignalBlocker blocker( uvSetCombo );
		uvSetCombo->clear();
		const int count = std::max( view->activeUVSetCount(), 1 );
		for ( int i = 0; i < count; i++ )
			uvSetCombo->addItem( QObject::tr( "UV %1" ).arg( i + 1 ) );
		uvSetCombo->setCurrentIndex( std::clamp( view->uvSetIndex, 0, count - 1 ) );
		uvSetCombo->setEnabled( count > 1 );
	};

	// ---- viewport wiring ----
	auto rebuild = [view, refreshUnderlayCombo, refreshUVSetCombo]() {
		view->rebuildFromViewport();
		refreshUnderlayCombo();
		refreshUVSetCombo();
	};
	// a hidden editor defers to its next showEvent instead of rebuilding a
	// (possibly huge) mesh on every viewport selection change
	view->deferredRebuildCb = rebuild;
	auto rebuildOrDefer = [view, rebuild]() {
		if ( view->isVisible() )
			rebuild();
		else
			view->viewportRebuildPending = true;
	};
	QObject::connect( ogl, &GLView::editModeChanged, view, [rebuildOrDefer]( bool ) { rebuildOrDefer(); } );
	QObject::connect( ogl, &GLView::elementSelectionChanged, view,
		[view]() { view->syncSelectionFromViewport(); } );
	// Object Mode is a read-only view; follow the object selection so the shown
	// mesh (and its secondaries) track what is selected in the 3D viewport.
	QObject::connect( ogl, &GLView::objectSelectionChanged, view, [view, rebuildOrDefer]() {
		if ( !view->ogl->editMode )
			rebuildOrDefer();
	} );
	QObject::connect( ogl, &GLView::pickModeChanged, view, [view]( int mode ) {
		if ( mode >= 1 && mode <= 3 )
			view->setSelectMode( mode, true );
	} );
	QObject::connect( nif, &NifModel::dataChanged, view,
		[view]( const QModelIndex & topLeft, const QModelIndex & ) {
			if ( view->applyingEdit )
				return;
			if ( view->shapeRowMatches( topLeft ) )
				view->reloadShapeUVs( view->nif->getBlockNumber( topLeft ) );
		} );
	QObject::connect( nif, &NifModel::modelReset, view, [view]() { view->clearData(); } );
	// A UV edit's Undo/redo (UVEditCommand) patches values in place, so the
	// editor stays populated and refreshes instantly via dataChanged — no blank
	// flash. Only a *structural* Undo (whole-model snapshot from a spell, etc.)
	// reloads the model and clears us via modelReset; rebuild only in that case.
	if ( nif->undoStack ) {
		QObject::connect( nif->undoStack, &QUndoStack::indexChanged, view, [view, rebuild]() {
			if ( view->applyingEdit || !view->isVisible() || view->hasData() )
				return;
			QTimer::singleShot( 0, view, [view, rebuild]() {
				if ( !view->applyingEdit && !view->hasData() )
					rebuild();
			} );
		} );
	}
	// Opening the workspace no longer forces Edit Mode: in Object Mode the
	// editor shows the selected mesh's UVs read-only (primary white, secondary
	// meshes colored). Switch to Edit Mode (Tab / mode selector) to edit.
	QObject::connect( dock, &QDockWidget::visibilityChanged, view, [rebuild]( bool visible ) {
		if ( visible )
			rebuild();
	} );

	dock->setWidget( panel );
	dock->setMinimumWidth( 340 );
	mw->addDockWidget( Qt::RightDockWidgetArea, dock );
	dock->hide();
	return dock;
}
