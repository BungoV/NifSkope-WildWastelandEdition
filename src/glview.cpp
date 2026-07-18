/***** BEGIN LICENSE BLOCK *****

BSD License

Copyright (c) 2005-2015, NIF File Format Library and Tools
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:
1. Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.
3. The name of the NIF File Format Library and Tools project may not be
   used to endorse or promote products derived from this software
   without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

***** END LICENCE BLOCK *****/

#include "glview.h"

#include "message.h"
#include "nifskope.h"
#include "gl/renderer.h"
#include "gl/glshape.h"
#include "gl/gltex.h"
#include "model/nifmodel.h"
#include "model/undocommands.h"
#include "data/nifitem.h"
#include "nifsnapshot.h"
#include "shortcutregistry.h"
#include "ui/settingsdialog.h"
#include "ui/widgets/fileselect.h"
#include "fp32vec4.hpp"
#include "ui/widgets/filebrowser.h"
#include "qt5compat.hpp"
#include "spells/blocks.h"	// blockLink for Separate / Duplicate

#include <memory>

#include <QApplication>
#include <QActionGroup>
#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QDebug>
#include <QHash>
#include <QSet>
#include <QDialog>
#include <QDir>
#include <QGroupBox>
#include <QImageWriter>
#include <QKeyEvent>
#include <QLabel>
#include <QMenu>
#include <QInputDialog>
#include <QMouseEvent>
#include <QScreen>
#include <QTimer>

namespace {
//! A QMenu that cancels itself once the pointer moves well clear of it, so the
//! small operator pop-ups (Delete / Merge / Separate / Snap / Set Origin) close
//! on hover-out, Blender-style. A grabbed menu does not reliably get mouseMove
//! events off itself, so it polls the global cursor against its screen geometry.
//! The main right-click context menu keeps normal click-to-dismiss (unaffected).
class AutoCloseMenu final : public QMenu
{
public:
	explicit AutoCloseMenu( QWidget * parent = nullptr ) : QMenu( parent )
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
}
#include <QMimeData>
#include <QMouseEvent>
#include <QOpenGLContext>
#include <QOpenGLFunctions>
#include <QPainter>
#include <QPushButton>
#include <QRadioButton>
#include <QSettings>
#include <QSpinBox>
#include <QSurface>
#include <QTimer>
#include <QToolBar>
#include <QWindow>

#include <QBuffer>
#include <QOpenGLContext>
#include <QOpenGLFunctions>
#include <QOpenGLFramebufferObject>

// NOTE: The FPS define is a frame limiter,
//	NOT the guaranteed FPS in the viewport.
//	Also the QTimer is integer milliseconds
//	so 60 will give you 1000/60 = 16, not 16.666
//	therefore it's really 62.5FPS
#define FPS 144

#define ZOOM_MIN 1.0
#define ZOOM_MAX 1000.0

#define DEBUG_FRAME_TIME 0

//! @file glview.cpp GLView implementation


const Vector3 GLView::viewRotations[6] = {
	{ 0.0f, 0.0f, 0.0f },		// Top
	{ 180.0f, 0.0f, 0.0f },		// Bottom
	{ -90.0f, 0.0f, -90.0f },	// Left
	{ -90.0f, 0.0f, 90.0f },	// Right
	{ -90.0f, 0.0f, 180.0f },	// Front
	{ -90.0f, 0.0f, 0.0f }		// Back
};

//! Register the 3D viewport's rebindable shortcuts (once per process).
//! Every id here has at least one matches() call in GLView::keyPressEvent or
//! NifSkope::eventFilter; the settings Shortcuts page edits them. The modal
//! gesture grammar (X/Y/Z axis locks, numeric entry, Esc/Enter) and the
//! Blender numpad view block stay fixed.
static void tlRegisterViewportShortcuts()
{
	static bool done = false;
	if ( done )
		return;
	done = true;
	auto & r = ShortcutRegistry::get();
	const QString cat = QObject::tr( "3D Viewport" );
	const QString catSel = QObject::tr( "3D Viewport - Selection" );
	const QString catEdit = QObject::tr( "3D Viewport - Edit Mode" );
	const QString catObj = QObject::tr( "3D Viewport - Object Mode" );

	r.reg( "viewport.toggle_edit_mode", QObject::tr( "Toggle Object / Edit Mode" ), cat, QKeySequence( Qt::Key_Tab ) );
	r.reg( "viewport.quick_menu", QObject::tr( "Specials Quick Menu" ), cat, QKeySequence( Qt::Key_W ) );
	r.reg( "viewport.transform.move", QObject::tr( "Move (modal transform)" ), cat, QKeySequence( Qt::Key_G ) );
	r.reg( "viewport.transform.rotate", QObject::tr( "Rotate (modal transform)" ), cat, QKeySequence( Qt::Key_R ) );
	r.reg( "viewport.transform.scale", QObject::tr( "Scale (modal transform)" ), cat, QKeySequence( Qt::Key_S ) );
	r.reg( "viewport.hide", QObject::tr( "Hide Selection" ), cat, QKeySequence( Qt::Key_H ) );
	r.reg( "viewport.unhide_all", QObject::tr( "Unhide All" ), cat, QKeySequence( Qt::ALT | Qt::Key_H ) );
	r.reg( "viewport.duplicate", QObject::tr( "Duplicate" ), cat, QKeySequence( Qt::SHIFT | Qt::Key_D ) );
	r.reg( "viewport.snap", QObject::tr( "Snap Menu" ), cat, QKeySequence( Qt::SHIFT | Qt::Key_S ) );
	r.reg( "viewport.frame_selection", QObject::tr( "Frame Selection" ), cat, QKeySequence( Qt::Key_Period ) );
	r.reg( "viewport.free_camera", QObject::tr( "Free Camera (Fly / Walk)" ), cat, QKeySequence( Qt::SHIFT | Qt::Key_F ) );
	r.reg( "viewport.snap_cursor_median", QObject::tr( "Snap 3D Cursor to Selection" ), cat, QKeySequence( Qt::SHIFT | Qt::Key_C ) );

	r.reg( "viewport.select.all", QObject::tr( "Select All (toggle)" ), catSel, QKeySequence( Qt::Key_A ) );
	r.reg( "viewport.select.none", QObject::tr( "Deselect All" ), catSel, QKeySequence( Qt::ALT | Qt::Key_A ) );
	r.reg( "viewport.select.invert", QObject::tr( "Invert Selection" ), catSel, QKeySequence( Qt::CTRL | Qt::Key_I ) );
	r.reg( "viewport.select.box", QObject::tr( "Box Select" ), catSel, QKeySequence( Qt::Key_B ) );
	r.reg( "viewport.select.circle", QObject::tr( "Circle Select" ), catSel, QKeySequence( Qt::Key_C ) );
	r.reg( "viewport.select.more", QObject::tr( "Select More" ), catSel, QKeySequence( Qt::CTRL | Qt::Key_Equal ) );
	r.reg( "viewport.select.less", QObject::tr( "Select Less" ), catSel, QKeySequence( Qt::CTRL | Qt::Key_Minus ) );
	r.reg( "viewport.select.linked", QObject::tr( "Select Linked" ), catSel, QKeySequence( Qt::CTRL | Qt::Key_L ) );
	r.reg( "viewport.select.linked_angle", QObject::tr( "Select Linked by Angle" ), catSel,
		QKeySequence( Qt::CTRL | Qt::ALT | Qt::SHIFT | Qt::Key_F ) );
	r.reg( "viewport.select.checker", QObject::tr( "Checker Deselect" ), catSel, QKeySequence() );

	r.reg( "viewport.pick_vertex", QObject::tr( "Pick Mode: Vertex (Shift extends)" ), catEdit, QKeySequence( Qt::Key_1 ) );
	r.reg( "viewport.pick_edge", QObject::tr( "Pick Mode: Edge (Shift extends)" ), catEdit, QKeySequence( Qt::Key_2 ) );
	r.reg( "viewport.pick_face", QObject::tr( "Pick Mode: Face (Shift extends)" ), catEdit, QKeySequence( Qt::Key_3 ) );
	r.reg( "viewport.extrude", QObject::tr( "Extrude Region" ), catEdit, QKeySequence( Qt::Key_E ) );
	r.reg( "viewport.inset", QObject::tr( "Inset Faces" ), catEdit, QKeySequence( Qt::Key_I ) );
	r.reg( "viewport.fill", QObject::tr( "Make Face (quad) / Fill / Bridge" ), catEdit, QKeySequence( Qt::Key_F ) );
	r.reg( "viewport.tris_to_quads", QObject::tr( "Tris to Quads" ), catEdit, QKeySequence( Qt::ALT | Qt::Key_J ) );
	r.reg( "viewport.triangulate", QObject::tr( "Triangulate Faces" ), catEdit, QKeySequence( Qt::CTRL | Qt::Key_T ) );
	r.reg( "viewport.loop_cut", QObject::tr( "Loop Cut" ), catEdit, QKeySequence( Qt::CTRL | Qt::Key_R ) );
	r.reg( "viewport.knife", QObject::tr( "Knife" ), catEdit, QKeySequence( Qt::Key_K ) );
	r.reg( "viewport.edge_slide", QObject::tr( "Edge Slide" ), catEdit, QKeySequence( Qt::SHIFT | Qt::Key_V ) );
	r.reg( "viewport.dissolve", QObject::tr( "Dissolve Vertices" ), catEdit, QKeySequence( Qt::CTRL | Qt::Key_X ) );
	r.reg( "viewport.split", QObject::tr( "Split" ), catEdit, QKeySequence( Qt::Key_Y ) );
	r.reg( "viewport.rip", QObject::tr( "Rip" ), catEdit, QKeySequence( Qt::Key_V ) );
	r.reg( "viewport.bevel", QObject::tr( "Bevel" ), catEdit, QKeySequence( Qt::CTRL | Qt::Key_B ) );
	r.reg( "viewport.merge", QObject::tr( "Merge Menu" ), catEdit, QKeySequence( Qt::Key_M ) );
	r.reg( "viewport.separate", QObject::tr( "Separate Menu" ), catEdit, QKeySequence( Qt::Key_P ) );
	r.reg( "viewport.delete", QObject::tr( "Delete Menu" ), catEdit, QKeySequence( Qt::Key_X ) );
	r.reg( "viewport.repeat_last", QObject::tr( "Repeat Last Operator" ), catEdit,
		QKeySequence( Qt::SHIFT | Qt::Key_R ) );
	r.reg( "viewport.panel_to_cursor", QObject::tr( "Adjust Panel at Cursor" ), catEdit,
		QKeySequence( Qt::Key_F9 ) );

	r.reg( "viewport.add_primitive", QObject::tr( "Add Primitive Menu" ), catObj, QKeySequence( Qt::SHIFT | Qt::Key_A ) );
	r.reg( "viewport.join", QObject::tr( "Join Selected Objects" ), catObj, QKeySequence( Qt::CTRL | Qt::Key_J ) );
	r.reg( "viewport.parent_set", QObject::tr( "Set Parent Menu" ), catObj, QKeySequence( Qt::CTRL | Qt::Key_P ) );
	r.reg( "viewport.parent_clear", QObject::tr( "Clear Parent Menu" ), catObj, QKeySequence( Qt::ALT | Qt::Key_P ) );

	r.reg( "viewport.paint_fill", QObject::tr( "Paint: Fill Selection (weight / segment)" ), cat,
		QKeySequence( Qt::CTRL | Qt::Key_X ) );
	// unbound by default; the Shortcuts page also offers this as a combo box
	r.reg( "viewport.swap_mouse_select", QObject::tr( "Swap Select / Place-Gizmo Mouse Buttons" ), cat,
		QKeySequence() );
}

GLView::GLView( QWindow * p )
	: QOpenGLWindow( QOpenGLWindow::NoPartialUpdate, p )
{
	tlRegisterViewportShortcuts();
	QSettings settings;
	int	aa = settings.value( "Settings/Render/General/Msaa Samples", 2 ).toInt();
	editDeformedCage = settings.value( "GLView/Edit/DeformedCage", true ).toBool();
	selectWithRightMouse = settings.value( "Shortcuts/MouseSelect" ).toString() == QLatin1String( "right" );
	mirrorEditing = settings.value( "GLView/Edit/MirrorX", false ).toBool();
	aa = std::clamp< int >( aa, 0, 4 );

	QSurfaceFormat	fmt;

	// OpenGL version (4.1 or 4.2, core profile)
	fmt.setRenderableType( QSurfaceFormat::OpenGL );
	fmt.setMajorVersion( 4 );
#ifdef Q_OS_MACOS
	fmt.setMinorVersion( 1 );
#else
	fmt.setMinorVersion( 2 );
#endif
	fmt.setProfile( QSurfaceFormat::CoreProfile );
	fmt.setOption( QSurfaceFormat::DeprecatedFunctions, false );

	// V-Sync
	fmt.setSwapInterval( DEBUG_FRAME_TIME ? 0 : 1 );
	fmt.setSwapBehavior( QSurfaceFormat::DoubleBuffer );

	fmt.setDepthBufferSize( 24 );
	fmt.setStencilBufferSize( 8 );
	fmt.setSamples( 1 << aa );

	setFormat( fmt );

	view = ViewDefault;
	debugMode = DbgNone;
	perspectiveMode = true;
	animState = AnimEnabled;

	Zoom = 1.0;

	doCenter  = false;
	doCompile = 0;

	model = nullptr;

	time = 0.0f;
	Dist = 128.0f;
	lastTime = std::chrono::steady_clock::now();

	textures = new TexCache( this );

	updateSettings();
	view = cfg.startupDirection;
	if ( int i = int( view ) - int( ViewTop ); i >= 0 && i <= 5 )
		Rot = viewRotations[i];

	scene = new Scene( textures );
	connect( textures, &TexCache::sigRefresh, this, static_cast<void (GLView::*)()>(&GLView::update) );
	connect( scene, &Scene::sceneUpdated, this, static_cast<void (GLView::*)()>(&GLView::update) );

	timer = new QTimer( this );
	timer->setInterval( 1000 / FPS );
	timer->start();
	connect( timer, &QTimer::timeout, this, &GLView::advanceGears );

	lightVisTimeout = 1500;
	lightVisTimer = new QTimer( this );
	lightVisTimer->setSingleShot( true );
	connect( lightVisTimer, &QTimer::timeout, [this]() { setVisMode( Scene::VisLightPos, false ); update(); } );

	connect( NifSkope::getOptions(), &SettingsDialog::flush3D, textures, &TexCache::flush );
	connect( NifSkope::getOptions(), &SettingsDialog::update3D, this, &GLView::update3D );

	setMinimumSize( QSize( 50, 50 ) );
}

GLView::~GLView()
{
	auto	prvContext = pushGLContext();

	flush();
	delete textures;
	delete scene;

	popGLContext( prvContext );
}

QWidget * GLView::createWindowContainer( QWidget * parent )
{
	graphicsView = QWidget::createWindowContainer( this, parent );
	graphicsView->setContextMenuPolicy( Qt::PreventContextMenu );
	graphicsView->setFocusPolicy( Qt::ClickFocus );
	graphicsView->setAcceptDrops( true );
	graphicsView->setMinimumSize( QSize( 50, 50 ) );

	graphicsView->installEventFilter( parent );
	installEventFilter( graphicsView );

	return graphicsView;
}

void GLView::setCollisionPreview( const QVector<Vector3> & triangleSoup )
{
	collisionPreviewSoup = triangleSoup;
	update();
}

void GLView::clearCollisionPreview()
{
	if ( collisionPreviewSoup.isEmpty() )
		return;
	collisionPreviewSoup.clear();
	update();
}

void GLView::setRiggingDonorPreview( const QVector<Vector3> & triangleSoup )
{
	riggingDonorPreviewSoup = triangleSoup;
	update();
}

void GLView::setRiggingDonorPreviewStyle( bool filled, bool wireframe, float opacity )
{
	riggingDonorPreviewFilled = filled;
	riggingDonorPreviewWireframe = wireframe;
	riggingDonorPreviewOpacity = qBound( 0.02f, opacity, 0.95f );
	update();
}

void GLView::clearRiggingDonorPreview()
{
	if ( riggingDonorPreviewSoup.isEmpty() )
		return;
	riggingDonorPreviewSoup.clear();
	update();
}

void GLView::setSessionDocumentPreview( const QVector<Vector3> & triangleSoup )
{
	sessionDocumentPreviewSoup = triangleSoup;
	// The preview is drawn opaque, so a single flat color would collapse into
	// an unreadable silhouette. Bake simple per-face lambert shading from a
	// fixed world-space light once per rebuild; the draw itself stays a plain
	// colored triangle soup.
	sessionDocumentPreviewColors.clear();
	sessionDocumentPreviewColors.reserve( sessionDocumentPreviewSoup.size() );
	static const Vector3 lightDir = Vector3( 0.35f, -0.45f, 0.82f ).normalize();
	const FloatVector4 base( 0.46f, 0.54f, 0.62f, 1.0f );
	for ( qsizetype i = 0; i + 2 < sessionDocumentPreviewSoup.size(); i += 3 ) {
		Vector3 normal = Vector3::crossproduct(
			sessionDocumentPreviewSoup.at( i + 1 ) - sessionDocumentPreviewSoup.at( i ),
			sessionDocumentPreviewSoup.at( i + 2 ) - sessionDocumentPreviewSoup.at( i ) );
		if ( normal.squaredLength() > 0.0f )
			normal.normalize();
		const float shade = 0.45f
			+ 0.55f * std::fabs( Vector3::dotproduct( normal, lightDir ) );
		FloatVector4 color = base * shade;
		color[3] = 1.0f;
		sessionDocumentPreviewColors << color << color << color;
	}
	update();
}

void GLView::clearSessionDocumentPreview()
{
	if ( sessionDocumentPreviewSoup.isEmpty() )
		return;
	sessionDocumentPreviewSoup.clear();
	sessionDocumentPreviewColors.clear();
	update();
}

void GLView::setRiggingWeightPreview( const QVector<Vector3> & triangleSoup,
	const QVector<FloatVector4> & colors )
{
	if ( triangleSoup.size() != colors.size() ) {
		clearRiggingWeightPreview();
		return;
	}
	riggingWeightPreviewSoup = triangleSoup;
	riggingWeightPreviewColors = colors;
	update();
}

void GLView::setRiggingWeightPreviewColors( const QVector<FloatVector4> & colors )
{
	if ( colors.size() != riggingWeightPreviewSoup.size() )
		return;
	riggingWeightPreviewColors = colors;
	update();
}

void GLView::clearRiggingWeightPreview()
{
	if ( riggingWeightPreviewSoup.isEmpty() && riggingWeightPreviewColors.isEmpty() )
		return;
	riggingWeightPreviewSoup.clear();
	riggingWeightPreviewColors.clear();
	update();
}

void GLView::setRiggingWeightPaintMode( bool enabled, int targetBlock, int brushMode,
	float radius, float paintWeight, float strength )
{
	if ( enabled && vertexPaintMode )
		setVertexPaintMode( false );
	if ( enabled && segmentPaintMode )
		setSegmentPaintMode( false );
	if ( enabled && ( !model || !scene || targetBlock < 0 || !shapeForBlock( targetBlock ) ) )
		enabled = false;
	const bool wasEnabled = riggingWeightPaintMode;

	riggingWeightPaintTarget = enabled ? targetBlock : -1;
	riggingWeightPaintBrushMode = qBound( 0, brushMode, 3 );
	riggingWeightPaintRadius = qBound( 4.0f, radius, 400.0f );
	riggingWeightPaintWeight = qBound( 0.0f, paintWeight, 1.0f );
	riggingWeightPaintStrength = qBound( 0.0f, strength, 1.0f );
	riggingWeightPaintProjectionValid = false;
	riggingWeightPaintScreen.clear();
	riggingWeightPaintCandidates.clear();

	if ( enabled && !wasEnabled ) {
		// Weight Paint shares Edit Mode's complete element-selection engine.
		// Enter it on the paint target, then keep Weight Paint as the public mode
		// while Tab/the toolbar brush button switches the active tool.
		if ( editMode )
			setEditMode( false );
		QModelIndex target = model->getBlockIndex( targetBlock );
		scene->currentBlock = target;
		scene->currentIndex = target.sibling( target.row(), 0 );
		setEditMode( true );
		if ( !editMode ) {
			riggingWeightPaintTarget = -1;
			return;
		}
		editShapeBlocks.clear();
		editShapeBlocks.insert( targetBlock );
		editShapeBlock = targetBlock;
		scene->restPoseBlock = targetBlock;
		for ( int i = pickedElems.size() - 1; i >= 0; i-- )
			if ( pickedElems.at( i ).shapeBlock != targetBlock )
				pickedElems.remove( i );

		riggingWeightPaintMode = true;
		// Weight Paint always operates on the evaluated surface, even when the
		// user's normal Edit Mode preference is Raw Bind Position.
		scene->options.setFlag( Scene::DoSkinning, true );
		if ( Shape * shape = shapeForBlock( targetBlock ) )
			shape->updateBoneTransforms();
		refreshPickedElementPositions();
		riggingWeightPaintBrushEnabled = true;
		boxSelecting = false;
		boxSelectDrag = false;
		circleSelecting = false;
		circlePainting = circleErasing = false;
		riggingWeightPaintPos = QPointF( mapFromGlobal( QCursor::pos() ) );
		setCursor( Qt::BlankCursor );
	} else if ( !enabled && wasEnabled ) {
		if ( riggingWeightPaintStroke )
			emit riggingWeightStrokeEnded( false );
		riggingWeightPaintStroke = false;
		riggingWeightPaintMode = false;
		riggingWeightPaintBrushEnabled = false;
		unsetCursor();
		if ( editMode )
			setEditMode( false );
	}

	if ( wasEnabled != riggingWeightPaintMode ) {
		emit riggingWeightPaintBrushChanged( riggingWeightPaintBrushActive() );
		emit riggingWeightPaintModeChanged( enabled );
	}
	if ( riggingWeightPaintMode )
		emit gizmoStatus( riggingWeightPaintBrushEnabled
			? tr( "Weight Paint - Brush: LMB paints, wheel = zoom, RMB = menu, Tab = Object Mode, Esc = done" )
			: tr( "Weight Paint - Select: 1/2/3 vertex/edge/face, Ctrl+X fills, Tab = brush" ) );
	update();
}

void GLView::setVertexPaintPreviewColors( int targetBlock, const QVector<Color4> & colors )
{
	Shape * shape = shapeForBlock( targetBlock );
	if ( !shape || colors.size() != shape->verts.size() )
		return;
	shape->colors = colors;
	shape->clearHash();
	update();
}

void GLView::setVertexPaintMode( bool enabled, int targetBlock, float radius )
{
	if ( enabled && riggingWeightPaintMode )
		setRiggingWeightPaintMode( false );
	if ( enabled && segmentPaintMode )
		setSegmentPaintMode( false );
	if ( enabled && ( !model || !scene || targetBlock < 0 || !shapeForBlock( targetBlock ) ) )
		enabled = false;
	const bool wasEnabled = vertexPaintMode;

	vertexPaintTarget = enabled ? targetBlock : -1;
	vertexPaintRadius = qBound( 4.0f, radius, 400.0f );
	vertexPaintProjectionValid = false;
	vertexPaintScreen.clear();
	vertexPaintCandidates.clear();

	if ( enabled && !wasEnabled ) {
		if ( editMode )
			setEditMode( false );
		QModelIndex target = model->getBlockIndex( targetBlock );
		scene->currentBlock = target;
		scene->currentIndex = target.sibling( target.row(), 0 );
		setEditMode( true );
		if ( !editMode ) {
			vertexPaintTarget = -1;
			return;
		}
		editShapeBlocks.clear();
		editShapeBlocks.insert( targetBlock );
		editShapeBlock = targetBlock;
		scene->restPoseBlock = targetBlock;
		for ( int i = pickedElems.size() - 1; i >= 0; i-- )
			if ( pickedElems.at( i ).shapeBlock != targetBlock )
				pickedElems.remove( i );
		vertexPaintMode = true;
		// Paint on the same evaluated surface the user sees in Object Mode.
		scene->options.setFlag( Scene::DoSkinning, true );
		if ( Shape * shape = shapeForBlock( targetBlock ) )
			shape->updateBoneTransforms();
		refreshPickedElementPositions();
		vertexPaintBrushEnabled = true;
		boxSelecting = boxSelectDrag = false;
		circleSelecting = circlePainting = circleErasing = false;
		vertexPaintPos = QPointF( mapFromGlobal( QCursor::pos() ) );
		setCursor( Qt::BlankCursor );
	} else if ( !enabled && wasEnabled ) {
		if ( vertexPaintStroke )
			emit vertexPaintStrokeEnded( false );
		vertexPaintStroke = false;
		vertexPaintMode = false;
		vertexPaintBrushEnabled = false;
		unsetCursor();
		if ( editMode )
			setEditMode( false );
	}

	if ( wasEnabled != vertexPaintMode ) {
		emit vertexPaintBrushChanged( vertexPaintBrushActive() );
		emit vertexPaintModeChanged( vertexPaintMode );
	}
	if ( vertexPaintMode )
		emit gizmoStatus( vertexPaintBrushEnabled
			? tr( "Vertex Paint - Brush: LMB paints, wheel = zoom, RMB = menu, Tab = Object Mode, Esc = done" )
			: tr( "Vertex Paint - Select: 1/2/3 vertex/edge/face, Tab = brush" ) );
	update();
}

void GLView::setVertexPaintBrushEnabled( bool enabled )
{
	if ( !vertexPaintMode )
		enabled = false;
	if ( vertexPaintBrushEnabled == enabled )
		return;
	if ( vertexPaintStroke )
		emit vertexPaintStrokeEnded( false );
	vertexPaintStroke = false;
	vertexPaintBrushEnabled = enabled;
	vertexPaintProjectionValid = false;
	vertexPaintScreen.clear();
	vertexPaintCandidates.clear();
	if ( enabled ) {
		boxSelecting = boxSelectDrag = false;
		circleSelecting = circlePainting = circleErasing = false;
		vertexPaintPos = QPointF( mapFromGlobal( QCursor::pos() ) );
		setCursor( Qt::BlankCursor );
		emit gizmoStatus( tr( "Vertex Paint - Brush: LMB paints, wheel = zoom, RMB = menu, Tab = Object Mode" ) );
	} else {
		unsetCursor();
		emit gizmoStatus( tr( "Vertex Paint - Select: 1/2/3 vertex/edge/face, Tab = brush" ) );
	}
	emit vertexPaintBrushChanged( enabled );
	update();
}

void GLView::setSegmentPaintMode( bool enabled, int targetBlock, float radius )
{
	if ( enabled && riggingWeightPaintMode ) setRiggingWeightPaintMode( false );
	if ( enabled && vertexPaintMode ) setVertexPaintMode( false );
	if ( enabled && ( !model || !scene || targetBlock < 0 || !shapeForBlock( targetBlock ) ) )
		enabled = false;
	const bool wasEnabled = segmentPaintMode;
	segmentPaintTarget = enabled ? targetBlock : -1;
	segmentPaintRadius = qBound( 4.0f, radius, 400.0f );
	segmentPaintProjectionValid = false;
	segmentPaintScreen.clear();
	segmentPaintCandidates.clear();
	if ( enabled && !wasEnabled ) {
		if ( editMode ) setEditMode( false );
		QModelIndex target = model->getBlockIndex( targetBlock );
		scene->currentBlock = target;
		scene->currentIndex = target.sibling( target.row(), 0 );
		setEditMode( true );
		if ( !editMode ) { segmentPaintTarget = -1; return; }
		editShapeBlocks.clear();
		editShapeBlocks.insert( targetBlock );
		editShapeBlock = targetBlock;
		scene->restPoseBlock = targetBlock;
		for ( int i = pickedElems.size() - 1; i >= 0; i-- )
			if ( pickedElems.at( i ).shapeBlock != targetBlock ) pickedElems.remove( i );
		segmentPaintMode = true;
		scene->options.setFlag( Scene::DoSkinning, true );
		if ( Shape * shape = shapeForBlock( targetBlock ) ) shape->updateBoneTransforms();
		refreshPickedElementPositions();
		setPickMode( 3 );
		segmentPaintBrushEnabled = true;
		boxSelecting = boxSelectDrag = false;
		circleSelecting = circlePainting = circleErasing = false;
		segmentPaintPos = QPointF( mapFromGlobal( QCursor::pos() ) );
		setCursor( Qt::BlankCursor );
	} else if ( !enabled && wasEnabled ) {
		if ( segmentPaintStroke ) emit segmentPaintStrokeEnded( false );
		segmentPaintStroke = false;
		segmentPaintMode = false;
		segmentPaintBrushEnabled = false;
		unsetCursor();
		if ( editMode ) setEditMode( false );
	}
	if ( wasEnabled != segmentPaintMode ) {
		emit segmentPaintBrushChanged( segmentPaintBrushActive() );
		emit segmentPaintModeChanged( segmentPaintMode );
	}
	if ( segmentPaintMode )
		emit gizmoStatus( segmentPaintBrushEnabled
			? tr( "Segment Paint - Brush: LMB assigns/removes faces, wheel = zoom, Tab = Object Mode" )
			: tr( "Segment Paint - Face Select: selection masks painting, Tab = brush" ) );
	update();
}

void GLView::setSegmentPaintBrushEnabled( bool enabled )
{
	if ( !segmentPaintMode ) enabled = false;
	if ( segmentPaintBrushEnabled == enabled ) return;
	if ( segmentPaintStroke ) emit segmentPaintStrokeEnded( false );
	segmentPaintStroke = false;
	segmentPaintBrushEnabled = enabled;
	segmentPaintProjectionValid = false;
	segmentPaintScreen.clear();
	segmentPaintCandidates.clear();
	if ( enabled ) {
		boxSelecting = boxSelectDrag = false;
		circleSelecting = circlePainting = circleErasing = false;
		segmentPaintPos = QPointF( mapFromGlobal( QCursor::pos() ) );
		setCursor( Qt::BlankCursor );
	} else {
		unsetCursor();
		setPickMode( 3 );
	}
	emit segmentPaintBrushChanged( enabled );
	emit gizmoStatus( enabled
		? tr( "Segment Paint - Brush: LMB assigns/removes faces, wheel = zoom, Tab = Object Mode" )
		: tr( "Segment Paint - Face Select: selection masks painting, Tab = brush" ) );
	update();
}

void GLView::setRiggingWeightPaintBrushEnabled( bool enabled )
{
	if ( !riggingWeightPaintMode )
		enabled = false;
	if ( riggingWeightPaintBrushEnabled == enabled )
		return;
	if ( riggingWeightPaintStroke )
		emit riggingWeightStrokeEnded( false );
	riggingWeightPaintStroke = false;
	riggingWeightPaintBrushEnabled = enabled;
	riggingWeightPaintProjectionValid = false;
	riggingWeightPaintScreen.clear();
	riggingWeightPaintCandidates.clear();
	if ( enabled ) {
		boxSelecting = boxSelectDrag = false;
		circleSelecting = circlePainting = circleErasing = false;
		riggingWeightPaintPos = QPointF( mapFromGlobal( QCursor::pos() ) );
		setCursor( Qt::BlankCursor );
		emit gizmoStatus( tr( "Weight Paint - Brush: LMB paints, wheel = zoom, Tab = Object Mode, RMB/Esc = done" ) );
	} else {
		unsetCursor();
		emit gizmoStatus( tr( "Weight Paint - Select: 1/2/3 vertex/edge/face, Ctrl+X fills, Tab = brush" ) );
	}
	emit riggingWeightPaintBrushChanged( enabled );
	update();
}

void GLView::fillRiggingWeightSelection()
{
	if ( !riggingWeightPaintMode || riggingWeightPaintTarget < 0 )
		return;
	// One pending fill at a time. The fill emits a stroke whose commit slot
	// serializes the whole model into an Undo snapshot; without this guard,
	// keyboard autorepeat (or a double delivery) stacks dozens of snapshots
	// synchronously and freezes the UI.
	if ( paintFillPending )
		return;
	QSet<int> selected = pickedVertexRefs().value( riggingWeightPaintTarget );
	if ( selected.isEmpty() ) {
		emit gizmoStatus( tr( "Weight Paint: select vertices, edges, or faces before Ctrl+X" ) );
		return;
	}
	paintFillPending = true;
	// Run outside the key-event handler so the snapshot/undo push and any
	// resulting modal cannot re-enter the event that triggered us.
	QTimer::singleShot( 0, this, [this]() {
		paintFillPending = false;
		if ( !riggingWeightPaintMode || riggingWeightPaintTarget < 0 )
			return;
		QSet<int> sel = pickedVertexRefs().value( riggingWeightPaintTarget );
		if ( sel.isEmpty() )
			return;
		QVector<int> vertices( sel.constBegin(), sel.constEnd() );
		std::sort( vertices.begin(), vertices.end() );
		QVector<float> fullStrength( vertices.size(), 1.0f );
		emit riggingWeightStrokeBegan();
		emit riggingWeightBrushSample( riggingWeightPaintTarget, vertices, fullStrength,
			2, riggingWeightPaintWeight, 1.0f ); // Replace at the current Weight value
		emit riggingWeightStrokeEnded( true );
	} );
}

void GLView::fillSegmentPaintSelection()
{
	if ( !segmentPaintMode || segmentPaintTarget < 0 )
		return;
	if ( paintFillPending )
		return;
	Shape * shape = shapeForBlock( segmentPaintTarget );
	if ( !shape ) return;
	paintFillPending = true;
	QTimer::singleShot( 0, this, [this]() {
		paintFillPending = false;
		if ( !segmentPaintMode || segmentPaintTarget < 0 )
			return;
		Shape * shape = shapeForBlock( segmentPaintTarget );
		if ( !shape ) return;
		QSet<int> exactFaces;
		QSet<int> selectedVertices = pickedVertexRefs().value( segmentPaintTarget );
		for ( const auto & pe : pickedElems )
			if ( pe.shapeBlock == segmentPaintTarget && pe.type == 3 && pe.e0 >= 0 )
				exactFaces.insert( pe.e0 );
		QSet<int> selectedFaces = exactFaces;
		if ( exactFaces.isEmpty() && !selectedVertices.isEmpty() ) {
			for ( int triangle = 0; triangle < shape->triangles.size(); triangle++ ) {
				const Triangle & tri = shape->triangles.at( triangle );
				if ( selectedVertices.contains( tri.v1() ) || selectedVertices.contains( tri.v2() )
					|| selectedVertices.contains( tri.v3() ) ) selectedFaces.insert( triangle );
			}
		}
		if ( selectedFaces.isEmpty() ) {
			emit gizmoStatus( tr( "Segment Paint: select vertices, edges, or faces before Ctrl+X" ) );
			return;
		}
		QVector<int> triangles = selectedFaces.values();
		std::sort( triangles.begin(), triangles.end() );
		emit segmentPaintStrokeBegan();
		emit segmentPaintBrushSample( segmentPaintTarget, triangles );
		emit segmentPaintStrokeEnded( true );
	} );
}

float	GLView::Settings::vertexPointSize = 5.0f;
float	GLView::Settings::tbnPointSize = 7.0f;
float	GLView::Settings::vertexSelectPointSize = 8.5f;
float	GLView::Settings::vertexPointSizeSelected = 10.0f;
float	GLView::Settings::lineWidthAxes = 2.0f;
float	GLView::Settings::lineWidthWireframe = 1.6f;
float	GLView::Settings::lineWidthHighlight = 2.5f;
float	GLView::Settings::lineWidthGrid = 1.4f;
float	GLView::Settings::lineWidthSelect = 5.0f;
float	GLView::Settings::zoomInScale = 0.95f;
float	GLView::Settings::zoomOutScale = 1.0f / 0.95f;

void GLView::updateSettings()
{
	QSettings settings;
	settings.beginGroup( "Settings/Render" );

	cfg.background = Color4( settings.value( "Colors/Background", QColor( 46, 46, 46 ) ).value<QColor>() );
	// Preserve the Settings defaults on a clean profile. QVariant::toFloat()
	// returns zero for a missing key, which previously reduced both keyboard
	// camera transforms and Shift+F fly movement to a no-op until the Render
	// settings page had been saved at least once.
	cfg.fov = std::clamp( settings.value( "General/Camera/Field Of View", 60.0f ).toFloat(), 45.0f, 120.0f );
	cfg.moveSpd = std::clamp( settings.value( "General/Camera/Movement Speed", 350.0f ).toFloat(), 0.0f, 7000.0f );
	cfg.rotSpd = std::clamp( settings.value( "General/Camera/Rotation Speed", 45.0f ).toFloat(), 15.0f, 1500.0f );
	cfg.upAxis = UpAxis(settings.value( "General/Up Axis", ZAxis ).toInt());
	int	z = settings.value( "General/Camera/Startup Direction", 1 ).toInt();
	static const ViewState	startupDirections[6] = {
		ViewLeft, ViewFront, ViewTop, ViewRight, ViewBack, ViewBottom
	};
	cfg.startupDirection = startupDirections[std::clamp< int >( z, 0, 5 )];
	z = settings.value( "General/Camera/Mwheel Zoom Speed", 8 ).toInt();
	z = std::clamp< int >( z, 0, 16 );

	// viewport display sizes (Options > Settings > Render > Viewport Display)
	gizmoSizeMul = settings.value( "General/Gizmo Cursor Size", 1.75 ).toFloat();
	wireWidthMul = settings.value( "General/Wireframe Thickness", 1.0 ).toFloat();
	vertexPointSize = settings.value( "General/Edit Vertex Size", 5.0 ).toFloat();
	selLineWidth = settings.value( "General/Selection Line Width", 2.0 ).toFloat();

	settings.endGroup();

	if ( scene )
		scene->updateSettings( settings );

	// TODO: make these configurable via the UI
	double	p = devicePixelRatioF();
	Settings::vertexPointSize = float( p * 5.0 );
	Settings::tbnPointSize = float( p * 7.0 );
	Settings::vertexSelectPointSize = float( p * 8.5 );
	Settings::vertexPointSizeSelected = float( p * 10.0 );
	Settings::lineWidthAxes = float( p * 2.0 );
	Settings::lineWidthWireframe = float( p * 1.6 );
	Settings::lineWidthHighlight = float( p * 2.5 );
	Settings::lineWidthGrid = float( p * 1.4 );
	Settings::lineWidthSelect = float( p * 5.0 );

	double	tmp = std::pow( 0.95, std::sqrt( double(1 << z) * (1.0 / 256.0) ) );
	Settings::zoomInScale = float( tmp );
	Settings::zoomOutScale = float( 1.0 / tmp );
}

void GLView::update3D()
{
	updateSettings();
	auto	prvContext = pushGLContext();
	glClearColor( cfg.background.red(), cfg.background.green(), cfg.background.blue(), cfg.background.alpha() );
	popGLContext( prvContext );
	update();
}

static bool envMapFileListFilterFunction( void * p, const std::string_view & s )
{
	(void) p;
	if ( !s.starts_with("textures/") )
		return false;
	if ( !(s.ends_with(".dds") || s.ends_with(".hdr")) )
		return false;
	return ( s.find("/cubemaps/") != std::string_view::npos );
}

bool GLView::selectPBRCubeMapForGame( quint32 bsVersion )
{
	if ( bsVersion < 151 )
		return false;
	bool	isStarfield = ( bsVersion >= 170 );
	Game::GameMode	game = ( !isStarfield ? Game::FALLOUT_76 : Game::STARFIELD );
	QString	cfgPath( !isStarfield ? "Settings/Render/General/Cube Map Path FO 76" : "Settings/Render/General/Cube Map Path STF" );

	std::set< std::string_view >	fileSet;
	Game::GameManager::list_files( fileSet, game, &envMapFileListFilterFunction );
	QSettings	settings;
	std::string	prvPath( settings.value( cfgPath ).toString().toStdString() );
	if ( !prvPath.empty() && fileSet.find( prvPath ) == fileSet.end() )
		prvPath.clear();

	FileBrowserWidget	fileBrowser( 640, 480, "Select Default Environment Map", fileSet, prvPath,
										&( Game::GameManager::getGameResources( game ) ) );
	const std::string_view *	newPath = nullptr;
	if ( fileBrowser.exec() == QDialog::Accepted )
		newPath = fileBrowser.getItemSelected();
	if ( !newPath || newPath->empty() )
		return false;

	if ( NifSkope::getOptions() )
		NifSkope::getOptions()->apply();
	settings.setValue( cfgPath, QString::fromLatin1( newPath->data(), qsizetype(newPath->length()) ) );
	if ( NifSkope::getOptions() )
		emit NifSkope::getOptions()->loadSettings();

	return true;
}

void GLView::selectPBRCubeMap()
{
	if ( model && selectPBRCubeMapForGame( model->getBSVersion() ) ) {
		if ( scene && scene->renderer ) {
			scene->renderer->updateSettings();
			updateScene();
		}
	}
}

Color4 GLView::clearColor() const
{
	return cfg.background;
}


/*
 * Scene
 */

Scene * GLView::getScene()
{
	return scene;
}

void GLView::updateScene()
{
	scene->update( model, QModelIndex() );
	update();
}

void GLView::updateAnimationState( bool checked )
{
	QAction * action = qobject_cast<QAction *>(sender());
	if ( action ) {
		auto opt = AnimationState( action->data().toInt() );

		if ( checked )
			animState |= opt;
		else
			animState &= ~opt;

		scene->animate = (animState & AnimEnabled);
		lastTime = std::chrono::steady_clock::now();

		update();
	}
}


/*
 *  OpenGL
 */

void GLView::initializeGL()
{
	auto	cx = context();
	// Obtain a functions object and resolve all entry points
	auto	glFuncs = cx->functions();
	if ( !glFuncs ) {
		QMessageBox::critical( nullptr, "NifSkope error", tr( "Could not obtain OpenGL functions" ) );
		std::exit( 1 );
	}
	glFuncs->initializeOpenGLFunctions();
	scene->setOpenGLContext( cx );
	glContext = scene->renderer;
	textures->setOpenGLContext( glContext );
	updateShaders();		// should be called after TexCache is initialized
	glClearColor( cfg.background.red(), cfg.background.green(), cfg.background.blue(), cfg.background.alpha() );

	// Initial viewport values
	//	Made viewport and aspect member variables.
	//	They were being updated every single frame instead of only when resizing.
	//glGetIntegerv( GL_VIEWPORT, viewport );
	aspect = (GLdouble)width() / (GLdouble)height();

	GLenum err;

	// Check for errors
	while ( ( err = glGetError() ) != GL_NO_ERROR )
		qDebug() << tr( "glview.cpp - GL ERROR (init) : " ) << getGLErrorString( int(err) );
}

void GLView::updateShaders()
{
	if ( !isValid() )
		return;
	auto	prvContext = pushGLContext();
	scene->updateShaders();
	popGLContext( prvContext );
	update();
}

void GLView::glProjection( [[maybe_unused]] int x, [[maybe_unused]] int y )
{
	if ( !scene->haveRenderer() )
		return;

	BoundSphere bs = scene->view * scene->bounds();

	if ( scene->hasOption(Scene::ShowAxes) ) {
		bs |= BoundSphere( scene->view * Vector3(), axis );
	}

	float bounds = std::max< float >( bs.radius, 1024.0f * scale() );


	GLdouble nr = std::fabs( bs.center[2] ) - bounds * 1.5;
	GLdouble fr = std::fabs( bs.center[2] ) + bounds * 1.5;

	if ( perspectiveMode || (view == ViewWalk) ) {
		// Perspective View
		if ( nr > fr ) {
			// add: swap them when needed
			std::swap( nr, fr );
		}
		nr = std::max< GLdouble >( nr, scale() );
		// ensure distance
		fr = std::max< GLdouble >( fr, nr + scale() );

		GLdouble h2 = std::tan( ( cfg.fov / Zoom ) / 360 * M_PI ) * nr;
		GLdouble w2 = h2 * aspect;
		scene->renderer->setProjectionMatrix( Matrix4::fromFrustum( -w2, +w2, -h2, +h2, nr, fr ) );
	} else {
		// Orthographic View
		GLdouble h2 = Dist / Zoom;
		GLdouble w2 = h2 * aspect;
		scene->renderer->setProjectionMatrix( Matrix4::fromOrtho( -w2, +w2, -h2, +h2, nr, fr ) );
	}
}


void GLView::paintGL()
{
#if DEBUG_FRAME_TIME
	auto	prvTime = std::chrono::steady_clock::now();
#endif

	updatePending = 0;

	glDisable( GL_FRAMEBUFFER_SRGB );
	glDepthMask( GL_TRUE );

	if ( isDisabled || !scene->haveRenderer() ) [[unlikely]] {
		glClearColor( cfg.background.red(), cfg.background.green(), cfg.background.blue(), cfg.background.alpha() );
		glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT );
		return;
	}

	// Clear Viewport
	if ( scene->hasVisMode(Scene::VisSilhouette) ) {
		glClearColor( 1.0f, 1.0f, 1.0f, 1.0f );
	}

	bool	clearNeeded = true;
	if ( !perspectiveMode || doCompile ) {
		glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT );
		clearNeeded = false;
	}

	// Compile the model
	if ( doCompile ) [[unlikely]] {
		if ( doCompile > 1 ) [[unlikely]] {
			doCompile--;
			update();
			return;
		}
		// avoid potential infinite recursion in case a message box is opened while initializing the scene
		isDisabled = true;
		textures->setNifFolder( model->getFolder() );
		scene->make( model );
		scene->transform( Transform(), scene->timeMin() );

		axis = (scene->bounds().radius <= 0) ? 1024.0 * scale() : scene->bounds().radius;

		if ( scene->timeMin() != scene->timeMax() ) {
			if ( time < scene->timeMin() || time > scene->timeMax() )
				time = scene->timeMin();

			emit sequencesUpdated();

		} else if ( scene->timeMax() == 0 ) {
			// No Animations in this NIF
			emit sequencesDisabled( true );
		}
		emit sceneTimeChanged( time, scene->timeMin(), scene->timeMax() );
		isDisabled = false;
		doCompile = 0;
		// two corrective repaints after the compile; the startup grid/axes
		// defect (see WW_CHANGES 2026-07-17) is still open - a synthetic
		// current block was tried here and reverted: it did not heal the
		// grid in live use and framing the root broke setCenter()
		postCompileRepaints = 2;
	}

	// Center the model
	if ( doCenter ) {
		setCenter();
		doCenter = false;
	}

	NifSkopeOpenGLContext *	cx = scene->renderer;

	// Transform the scene (viewTransform() must stay identical to this)
	Transform	viewTrans = viewTransform();

	scene->transform( viewTrans, time );

	// Setup projection mode
	glProjection();

	cx->setViewTransform( scene->view, int( cfg.upAxis ), envMapRotation );
	auto &	globalUniforms = *( cx->globalUniforms );
	globalUniforms.toneMapScale = toneMapping;
	globalUniforms.brightnessScale = brightnessScale;
	globalUniforms.glowScale = ( scene->hasOption(Scene::DoGlow) ? glowScale : 0.0f );
	FloatVector4	mat_amb( 0.0f );
	FloatVector4	mat_diff( 0.0f );
	FloatVector4	lightDir( 0.0f, 0.0f, 1.0f, 0.0f );
	bool	drawLightPos = false;

	if ( scene->hasVisMode(Scene::VisSilhouette) ) {
		globalUniforms.brightnessScale = 0.0f;

	} else if ( scene->hasOption(Scene::DoLighting) ) {
		// Setup light

		if ( !frontalLight ) {
			Matrix m;
			m.fromEuler( deg2rad( declination ), 0.0f, deg2rad( planarAngle ) );
			lightDir = FloatVector4::convertVector3( m.data() + 6 );
			if ( cfg.upAxis == XAxis )
				lightDir.shuffleValues( 0xD2 );
			else if ( cfg.upAxis == YAxis )
				lightDir.shuffleValues( 0xC9 );
			globalUniforms.lightSourcePosition[0] = globalUniforms.viewMatrix[0] * lightDir[0];
			globalUniforms.lightSourcePosition[0] += globalUniforms.viewMatrix[1] * lightDir[1];
			globalUniforms.lightSourcePosition[0] += globalUniforms.viewMatrix[2] * lightDir[2];

			drawLightPos = scene->hasVisMode( Scene::VisLightPos );
		} else {
			globalUniforms.lightSourcePosition[0] = FloatVector4( 0.0f, 0.0f, 1.0f, 0.0f );
		}

		mat_amb = FloatVector4( ambient );

		//                       red 0 to 1   green 0 to 1  blue -1 to 0  green -1 to 0
		const FloatVector4	a6(  2.22062011f,  0.74144780f,  1.54254896f,  5.04086054f );
		const FloatVector4	a5( -8.61531450f, -2.99683819f,  1.07328175f, 15.77713878f );
		const FloatVector4	a4( 14.04554747f,  5.24041808f, -4.19602456f, 17.63027420f );
		const FloatVector4	a3(-12.84139010f, -5.38996620f, -4.89534064f,  7.70809183f );
		const FloatVector4	a2(  7.50629512f,  3.76745881f,  0.83151672f,  1.09740388f );
		const FloatVector4	a1( -2.98006874f, -1.86500227f,  3.00010002f,  1.26401749f );
		const FloatVector4	a0( 1.0f );
		FloatVector4	c( lightColor );
		c = ( ( ( ( (c * a6 + a5) * c + a4 ) * c + a3 ) * c + a2 ) * c + a1 ) * c + a0;
		c = ( lightColor < 0.0f ? c.shuffleValues( 0x2C ) : c.shuffleValues( 0xF4 ) );
		mat_diff = c.maxValues( FloatVector4(0.0f) ).minValues( FloatVector4(1.0f) ) * brightnessL;

	} else {
		mat_amb = FloatVector4( 7.0f );
		mat_diff = FloatVector4( 0.0f );
	}

	globalUniforms.lightSourceAmbient = mat_amb;
	globalUniforms.lightSourceDiffuse[0] = mat_diff;
	globalUniforms.glowScaleSRGB = float( std::sqrt( globalUniforms.glowScale ) );
	globalUniforms.doSkinning = std::int32_t( scene->hasOption(Scene::DoSkinning) );
	globalUniforms.sceneOptions = std::int32_t( scene->options );
	cx->setGlobalUniforms();

	cx->setDefaultVertexAttribs( Scene::defaultAttrMask, Scene::defaultVertexAttrs );

	if ( scene->hasOption(Scene::DoMultisampling) )
		glEnable( GL_MULTISAMPLE );

	if ( perspectiveMode ) {
		bool	colorBufCleared = scene->renderer->drawSkyBox( scene );
		if ( clearNeeded ) {
			glClear( colorBufCleared ? GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT
										: GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT );
		}
	}

	if ( drawLightPos ) {
		glEnable( GL_DEPTH_TEST );
		glDepthMask( GL_TRUE );
		glDepthFunc( GL_LESS );

		// Scale the distance a bit
		float	s = scale() * 64.0f;
		float	l = axis + s;
		l = (l < s * 2.0f) ? axis * 1.5f : l;
		l = (l > s * 32.0f) ? axis * 0.66f : l;
		l = (l > s * 16.0f) ? axis * 0.75f : l;
		lightDir = lightDir * l;

		scene->setGLColor( FloatVector4( 1.0f ) );
		scene->setGLLineWidth( Settings::lineWidthAxes * 0.5f );
		scene->loadModelViewMatrix( viewTrans );
		scene->drawDashLine( Vector3(), Vector3( lightDir ), 30 );
		scene->drawSphereSimple( Vector3( lightDir ), axis / 10.0f, 72, 6 );
	}

#ifndef QT_NO_DEBUG
	if ( debugMode == DbgBounds ) {
		// Debug scene bounds
		glEnable( GL_DEPTH_TEST );
		glDepthMask( GL_TRUE );
		glDepthFunc( GL_LESS );
		BoundSphere bs = scene->bounds();
		bs |= BoundSphere( Vector3(), axis );
		scene->loadModelViewMatrix( viewTrans );
		scene->setGLColor( 1.0f, 1.0f, 1.0f, 0.25f );
		scene->setGLLineWidth( Settings::lineWidthAxes );
		scene->drawSphereSimple( bs.center, bs.radius, 72, 6 );
	}

	// Color Key debug
	if ( debugMode == DbgColorPicker ) {
		glDisable( GL_MULTISAMPLE );
		glDisable( GL_LINE_SMOOTH );
		glDisable( GL_DITHER );
		glEnable( GL_DEPTH_TEST );
		glDepthFunc( GL_LEQUAL );
		glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
		scene->selecting = ( scene->isSelModeVertex() ? 3 : 5 );
	} else {
		scene->selecting = 0;
	}
#endif

	// object mode shows the Blender-style silhouette outline for the selection,
	// so the legacy green selection wireframe is suppressed throughout object
	// mode (previously it depended on the tree selection matching, which made
	// it flicker back on after A-select-all / deselect-all)
	scene->objSelActive = !editMode;

	// Draw the model
	glDisable( GL_BLEND );
	scene->draw();

	// Selected-bone weight heatmap. Per-corner colours are supplied by the
	// Rigging Manager, while the viewport owns only an ephemeral triangle soup.
	if ( !riggingWeightPreviewSoup.isEmpty()
		&& riggingWeightPreviewSoup.size() == riggingWeightPreviewColors.size()
		&& !scene->selecting ) {
		glEnable( GL_DEPTH_TEST );
		glDepthFunc( GL_LEQUAL );
		glDepthMask( GL_FALSE );
		glEnable( GL_BLEND );
		glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
		scene->loadModelViewMatrix( viewTrans );
		glEnable( GL_POLYGON_OFFSET_FILL );
		glPolygonOffset( -1.0f, -1.0f );
		scene->drawTriangles( riggingWeightPreviewSoup.constData(),
			size_t( riggingWeightPreviewSoup.size() ), riggingWeightPreviewColors.constData(), true );
		glDisable( GL_POLYGON_OFFSET_FILL );
		glDisable( GL_BLEND );
		glDepthMask( GL_TRUE );
		glDepthFunc( GL_LESS );
	}

	// Blender-style Weight Paint selection mask. Unlike Edit Mode, Weight Paint
	// must not cover the useful heat colours with orange selection fill. The
	// paintable selection stays at full heatmap colour; protected geometry gets
	// Blender's neutral-grey veil. Face-mask boundaries and selected vertex/edge
	// cues are white, while unselected cage elements remain black.
	const bool elementPaintMode = riggingWeightPaintMode || vertexPaintMode || segmentPaintMode;
	const int elementPaintTarget = riggingWeightPaintMode ? riggingWeightPaintTarget
		: vertexPaintMode ? vertexPaintTarget : segmentPaintTarget;
	if ( model && elementPaintMode && elementPaintTarget >= 0
		&& !pickedElems.isEmpty() && !scene->selecting ) {
		Shape * s = shapeForBlock( elementPaintTarget );
		QSet<int> selectedVerts = pickedVertexRefs().value( elementPaintTarget );
		if ( s && !s->isHidden() && !selectedVerts.isEmpty()
			&& !s->verts.isEmpty() && !s->triangles.isEmpty() ) {
			const float dpr = float( devicePixelRatioF() );
			const int nv = s->verts.size();
			const bool faceMask = bool( pickMode & 4 );
			const bool vertexMask = bool( pickMode & 1 );
			const bool edgeMask = bool( pickMode & 2 );
			const QSet<int> hiddenT = editHiddenTris.value( elementPaintTarget );

			QSet<int> selectedFaces;
			QSet<quint64> selectedEdges;
			auto edgeKey = []( int a, int b ) {
				return ( quint64( std::min( a, b ) ) << 32 ) | quint64( std::max( a, b ) );
			};
			for ( const PickedElement & pe : pickedElems ) {
				if ( pe.shapeBlock != elementPaintTarget )
					continue;
				if ( pe.type == 3 )
					selectedFaces.insert( pe.e0 );
				else if ( pe.type == 2 )
					selectedEdges.insert( edgeKey( pe.e0, pe.e1 ) );
			}

			Transform wt = shapeRenderTrans( s );
			QVector<Vector3> worldVerts( nv );
			for ( int i = 0; i < nv; i++ )
				worldVerts[i] = wt * editVertexLocal( s, i );

			// In face mode a triangle is selected explicitly. Vertex and edge modes
			// use the actual paintable vertex set, allowing Blender's characteristic
			// soft grey transition across a triangle with a partly selected corner set.
			QVector<Vector3> maskSoup;
			QVector<FloatVector4> maskColors;
			maskSoup.reserve( s->triangles.size() * 3 );
			maskColors.reserve( s->triangles.size() * 3 );
			const FloatVector4 clearMask( 0.55f, 0.55f, 0.55f, 0.0f );
			const FloatVector4 greyMask( 0.55f, 0.55f, 0.55f, 0.68f );
			for ( int ti = 0; ti < s->triangles.size(); ti++ ) {
				if ( hiddenT.contains( ti ) )
					continue;
				const Triangle & t = s->triangles.at( ti );
				if ( t[0] >= nv || t[1] >= nv || t[2] >= nv )
					continue;
				const bool faceSelected = faceMask && selectedFaces.contains( ti );
				for ( int corner = 0; corner < 3; corner++ ) {
					const int vi = t[corner];
					maskSoup.append( worldVerts.at( vi ) );
					maskColors.append( faceMask
						? ( faceSelected ? clearMask : greyMask )
						: ( selectedVerts.contains( vi ) ? clearMask : greyMask ) );
				}
			}

			if ( !maskSoup.isEmpty() ) {
				glEnable( GL_DEPTH_TEST );
				glDepthFunc( GL_LEQUAL );
				glDepthMask( GL_FALSE );
				glEnable( GL_BLEND );
				glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
				scene->loadModelViewMatrix( viewTrans );
				glEnable( GL_POLYGON_OFFSET_FILL );
				glPolygonOffset( -2.0f, -2.0f );
				scene->drawTriangles( maskSoup.constData(), size_t( maskSoup.size() ),
					maskColors.constData(), true );
				glDisable( GL_POLYGON_OFFSET_FILL );
				glDisable( GL_BLEND );
				glDepthMask( GL_TRUE );
				glDepthFunc( GL_LESS );
			}

			// Build the visible cage once. Face mode uses only the boundary between
			// selected and protected faces; vertex/edge modes retain Blender's
			// black cage with selected endpoints/edges changed to white.
			struct MaskEdge {
				int a = -1;
				int b = -1;
				int adjacent = 0;
				bool selectedSide = false;
				bool protectedSide = false;
			};
			QHash<quint64, MaskEdge> maskEdges;
			for ( int ti = 0; ti < s->triangles.size(); ti++ ) {
				if ( hiddenT.contains( ti ) )
					continue;
				const Triangle & t = s->triangles.at( ti );
				if ( t[0] >= nv || t[1] >= nv || t[2] >= nv )
					continue;
				const bool selected = selectedFaces.contains( ti );
				for ( int e = 0; e < 3; e++ ) {
					const int a = t[e], b = t[( e + 1 ) % 3];
					const quint64 key = edgeKey( a, b );
					MaskEdge & edge = maskEdges[key];
					edge.a = std::min( a, b );
					edge.b = std::max( a, b );
					edge.adjacent++;
					edge.selectedSide = edge.selectedSide || selected;
					edge.protectedSide = edge.protectedSide || !selected;
				}
			}

			glEnable( GL_DEPTH_TEST );
			glDepthFunc( GL_LEQUAL );
			glDepthMask( GL_FALSE );
			scene->loadModelViewMatrix( viewTrans );
			if ( faceMask ) {
				QVector<Vector3> boundary;
				for ( auto it = maskEdges.constBegin(); it != maskEdges.constEnd(); ++it ) {
					const MaskEdge & edge = it.value();
					if ( edge.selectedSide && ( edge.protectedSide || edge.adjacent == 1 ) )
						boundary << worldVerts.at( edge.a ) << worldVerts.at( edge.b );
				}
				if ( !boundary.isEmpty() ) {
					glDisable( GL_DEPTH_TEST );
					scene->setGLLineWidth( 1.35f * dpr * wireWidthMul );
					scene->setGLColor( 1.0f, 1.0f, 1.0f, 0.95f );
					scene->drawLines( boundary.constData(), size_t( boundary.size() ), nullptr );
					glEnable( GL_DEPTH_TEST );
				}
			} else if ( vertexMask || edgeMask ) {
				const FloatVector4 black( 0.0f, 0.0f, 0.0f, 1.0f );
				const FloatVector4 white( 1.0f, 1.0f, 1.0f, 1.0f );
				QVector<Vector3> lines;
				QVector<FloatVector4> lineColors;
				lines.reserve( maskEdges.size() * 2 );
				lineColors.reserve( maskEdges.size() * 2 );
				for ( auto it = maskEdges.constBegin(); it != maskEdges.constEnd(); ++it ) {
					const MaskEdge & edge = it.value();
					lines << worldVerts.at( edge.a ) << worldVerts.at( edge.b );
					if ( edgeMask && selectedEdges.contains( it.key() ) )
						lineColors << white << white;
					else
						lineColors << ( selectedVerts.contains( edge.a ) ? white : black )
							<< ( selectedVerts.contains( edge.b ) ? white : black );
				}
				if ( !lines.isEmpty() ) {
					scene->setGLLineWidth( 1.0f * dpr * wireWidthMul );
					scene->drawLines( lines.constData(), size_t( lines.size() ), lineColors.constData() );
				}
				if ( vertexMask ) {
					QVector<Vector3> unselectedPoints, selectedPoints;
					for ( int vi = 0; vi < nv; vi++ )
						( selectedVerts.contains( vi ) ? selectedPoints : unselectedPoints )
							.append( worldVerts.at( vi ) );
					scene->setGLPointSize( vertexPointSize * dpr );
					if ( !unselectedPoints.isEmpty() ) {
						scene->setGLColor( black );
						scene->drawPoints( unselectedPoints.constData(), size_t( unselectedPoints.size() ) );
					}
					if ( !selectedPoints.isEmpty() ) {
						glDisable( GL_DEPTH_TEST );
						scene->setGLColor( white );
						scene->drawPoints( selectedPoints.constData(), size_t( selectedPoints.size() ) );
						glEnable( GL_DEPTH_TEST );
					}
				}
			}
			glDepthMask( GL_TRUE );
			glDepthFunc( GL_LESS );
		}
	}

	// Other open NIF documents form a neutral, read-only scene assembly around
	// the active document. They do not participate in picking or saving. The
	// preview is fully opaque (alpha comes from the baked per-face colors) and
	// writes depth so it occludes, and is occluded by, the primary correctly.
	if ( !sessionDocumentPreviewSoup.isEmpty() && !scene->selecting ) {
		glEnable( GL_DEPTH_TEST );
		glDepthFunc( GL_LEQUAL );
		glDepthMask( GL_TRUE );
		scene->loadModelViewMatrix( viewTrans );
		scene->setGLColor( 0.46f, 0.54f, 0.62f, 1.0f );
		glEnable( GL_POLYGON_OFFSET_FILL );
		glPolygonOffset( 1.0f, 1.0f );
		const bool shaded =
			sessionDocumentPreviewColors.size() == sessionDocumentPreviewSoup.size();
		scene->drawTriangles( sessionDocumentPreviewSoup.constData(),
			size_t( sessionDocumentPreviewSoup.size() ),
			shaded ? sessionDocumentPreviewColors.constData() : nullptr, true );
		glDisable( GL_POLYGON_OFFSET_FILL );
		glDepthFunc( GL_LESS );
	}

	// Rigging donor overlay: copied, read-only donor geometry in target model
	// space. It is deliberately outside Scene, so it cannot be picked, saved,
	// skinned, or confused with target blocks. Cyan distinguishes it from the
	// amber Collision Manager preview and orange target selection.
	if ( !riggingDonorPreviewSoup.isEmpty() && !scene->selecting
		&& ( riggingDonorPreviewFilled || riggingDonorPreviewWireframe ) ) {
		glEnable( GL_DEPTH_TEST );
		glDepthFunc( GL_LEQUAL );
		glDepthMask( GL_FALSE );
		scene->loadModelViewMatrix( viewTrans );
		if ( riggingDonorPreviewFilled ) {
			glEnable( GL_BLEND );
			glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
			scene->setGLColor( 0.08f, 0.66f, 1.0f, riggingDonorPreviewOpacity );
			glEnable( GL_POLYGON_OFFSET_FILL );
			glPolygonOffset( -2.0f, -2.0f );
			scene->drawTriangles( riggingDonorPreviewSoup.constData(),
				size_t( riggingDonorPreviewSoup.size() ), nullptr, true );
			glDisable( GL_POLYGON_OFFSET_FILL );
			glDisable( GL_BLEND );
		}
		if ( riggingDonorPreviewWireframe ) {
			scene->setGLColor( 0.18f, 0.82f, 1.0f, 1.0f );
			scene->setGLLineWidth( Settings::lineWidthHighlight * 0.8f );
			scene->drawTriangles( riggingDonorPreviewSoup.constData(),
				size_t( riggingDonorPreviewSoup.size() ), nullptr, false );
		}
		glDepthMask( GL_TRUE );
		glDepthFunc( GL_LESS );
	}

	// Collision Manager live preview: translucent amber fill plus a bright
	// wire overlay. This is a world-space triangle soup owned by the viewport,
	// not a model block, so cancelling the operator is a zero-cost clear.
	if ( !collisionPreviewSoup.isEmpty() && !scene->selecting ) {
		glEnable( GL_DEPTH_TEST );
		glDepthFunc( GL_LEQUAL );
		glEnable( GL_BLEND );
		glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
		glDepthMask( GL_FALSE );
		scene->loadModelViewMatrix( viewTrans );
		scene->setGLColor( 1.0f, 0.58f, 0.08f, 0.28f );
		glEnable( GL_POLYGON_OFFSET_FILL );
		glPolygonOffset( -1.0f, -1.0f );
		scene->drawTriangles( collisionPreviewSoup.constData(), size_t( collisionPreviewSoup.size() ), nullptr, true );
		glDisable( GL_POLYGON_OFFSET_FILL );
		glDepthMask( GL_TRUE );
		scene->setGLColor( 1.0f, 0.72f, 0.16f, 1.0f );
		scene->setGLLineWidth( Settings::lineWidthHighlight );
		scene->drawTriangles( collisionPreviewSoup.constData(), size_t( collisionPreviewSoup.size() ), nullptr, false );
		glDisable( GL_BLEND );
	}

	// Flat shading: the textured shapes were skipped, so fill every visible
	// mesh with a uniform dark grey (Blender wireframe-mode faces). X-ray
	// makes the fill half-transparent so geometry behind shows through.
	if ( model && scene->flatGrey && !scene->selecting ) {
		glEnable( GL_DEPTH_TEST );
		glDepthFunc( GL_LESS );
		glDepthMask( scene->xRay ? GL_FALSE : GL_TRUE );
		for ( Shape * s : scene->shapes ) {
			if ( !s || s->isHidden() || s->verts.isEmpty() || s->triangles.isEmpty() )
				continue;
			const QSet<int> hidT = editMode ? editHiddenTris.value( s->id() ) : QSet<int>();
			int nv = s->verts.size();
			QVector<Vector3> soup;
			soup.reserve( s->triangles.size() * 3 );
			auto displayLocal = [this, s]( int vi ) {
				return scene->hasOption( Scene::DoSkinning )
					? s->skinVertex( vi, s->verts[vi] ) : s->verts[vi];
			};
			for ( int ti = 0; ti < s->triangles.size(); ti++ ) {
				if ( hidT.contains( ti ) )
					continue;
				const Triangle & t = s->triangles.at( ti );
				if ( t[0] >= nv || t[1] >= nv || t[2] >= nv )
					continue;
				soup << displayLocal( t[0] ) << displayLocal( t[1] ) << displayLocal( t[2] );
			}
			if ( soup.isEmpty() )
				continue;
			scene->loadModelViewMatrix( s->viewTrans() );
			scene->setGLColor( 0.20f, 0.20f, 0.21f, scene->xRay ? 0.45f : 1.0f );
			scene->drawTriangles( soup.constData(), size_t( soup.size() ), nullptr, true );
		}
		glDepthMask( GL_TRUE );
		glDisable( GL_BLEND );
	}

	// Blender-style selection outlines around the object-mode selection
	if ( model && !editMode && !objSelection.isEmpty() && !scene->selecting )
		drawObjectOutlines();

	// Pivot gizmo: RGB axes at the selected node's origin (oriented to the node),
	// plus the active constraint axis while a modal G/R/S transform is running.
	// In edit mode it follows the picked elements rather than the tree selection.
	bool showGizmo = editMode ? ( !pickedElems.isEmpty() || elemTransform )
	                          : ( objActive >= 0 || scene->currentBlock.isValid() );
	if ( model && showGizmo ) {
		int gb;
		if ( editMode && editShapeBlock >= 0 ) {
			gb = editShapeBlock;
		} else if ( !editMode && objActive >= 0 ) {
			gb = objActive;	// follow the active object/node (incl. block-list picks)
		} else {
			gb = model->getBlockNumber( QModelIndex( scene->currentBlock ) );
			while ( gb >= 0 && !model->blockInherits( model->getBlockIndex( gb ), "NiAVObject" ) )
				gb = model->getParent( gb );
		}
		Node * gizmoNode = ( gb >= 0 ) ? scene->getNode( model, model->getBlockIndex( gb ) ) : nullptr;

		if ( gizmoNode ) {
			glDisable( GL_DEPTH_TEST );
			glDepthMask( GL_FALSE );

			QModelIndex iGb = model->getBlockIndex( gb );
			// during a modal gesture keep the frozen frame; otherwise follow the settings
			Matrix basis = gizmoMode ? gizmoBasisM : gizmoBasis( iGb );
			Vector3 pivot = gizmoMode ? gizmoPivotWorld : gizmoPivotPoint( iGb );
			if ( elemTransform )
				pivot = elemPivot;	// element transforms orbit the element median / cursor
			else if ( editMode && !pickedElems.isEmpty() )
				pivot = ( gizmoPivot == 3 ) ? cursorPos : pickedMedian();	// edit-mode gizmo sits on the selection

			Transform nt;
			nt.rotation = basis;
			nt.translation = pivot;
			nt.scale = 1.0f;
			float gs = gizmoScale( pivot );

			scene->setGLLineWidth( Settings::lineWidthAxes * ( gizmoMode ? 1.6f : 1.0f ) );
			scene->loadModelViewMatrix( viewTrans * nt );

			// object origin dot (Blender): always shown for the selected object /
			// node so geometry-less nodes (NiNode, lights) are visible and can be
			// grabbed, even with the gizmo handles toggled off
			if ( !editMode ) {
				scene->setGLPointSize( 8.0f * float( devicePixelRatioF() ) );
				if ( gb == objActive )
					scene->setGLColor( 1.0f, 0.616f, 0.0f, 1.0f );	// active #FF9D00
				else
					scene->setGLColor( 1.0f, 0.447f, 0.0f, 1.0f );	// secondary #FF7200
				Vector3 o;
				scene->drawPoints( &o, 1 );
			}

			if ( gizmoHandlesOn ) {
				// Blender-style handles: arrows with solid cone tips (move),
				// rings (rotate), solid boxes (scale), center circle (view-
				// plane move); modelview is already the gizmo basis at the
				// pivot. Colours match Blender: X #FF3352, Y #8BDC00, Z #2890FF.
				// During a modal G/R/S only the relevant sub-gizmo is drawn,
				// and only the constrained axis if one is locked in. When the
				// Show Gizmo toggle is off nothing is drawn here (not even during
				// a modal gesture) - only the axis constraint guide lines below
				// still show, so a G+X/Y/Z move stays readable.
				const float L = gs;
				const float axR[3][4] = {
					{ 1.000f, 0.200f, 0.322f, 1.0f },	// X
					{ 0.545f, 0.863f, 0.000f, 1.0f },	// Y
					{ 0.157f, 0.565f, 1.000f, 1.0f }	// Z
				};
				const float hw = Settings::lineWidthAxes * ( gizmoMode ? 1.5f : 1.1f );

				// solid cone: apex at base+dir*len, 12-segment fan + base disc
				auto solidCone = [this]( const Vector3 & base, const Vector3 & dir, float radius, float len ) {
					Vector3 n( dir );
					n.normalize();
					Vector3 u = Vector3::crossproduct( n, ( std::fabs( n[2] ) < 0.9f )
						? Vector3( 0.0f, 0.0f, 1.0f ) : Vector3( 1.0f, 0.0f, 0.0f ) );
					u.normalize();
					Vector3 v = Vector3::crossproduct( n, u );
					Vector3 apex = base + n * len;
					const int sd = 12;
					QVector<Vector3> tris;
					tris.reserve( sd * 6 );
					for ( int i = 0; i < sd; i++ ) {
						float a0 = float( i ) * float( M_PI * 2.0 / sd );
						float a1 = float( i + 1 ) * float( M_PI * 2.0 / sd );
						Vector3 p0 = base + ( u * std::cos( a0 ) + v * std::sin( a0 ) ) * radius;
						Vector3 p1 = base + ( u * std::cos( a1 ) + v * std::sin( a1 ) ) * radius;
						tris << apex << p0 << p1;		// side
						tris << base << p1 << p0;		// base disc
					}
					scene->drawTriangles( tris.constData(), size_t( tris.size() ), nullptr, true );
				};
				// solid axis-aligned cube (gizmo-local space)
				auto solidBox = [this]( const Vector3 & c, float h ) {
					Vector3 p[8];
					for ( int i = 0; i < 8; i++ )
						p[i] = c + Vector3( ( i & 1 ) ? h : -h, ( i & 2 ) ? h : -h, ( i & 4 ) ? h : -h );
					static const int F[12][3] = {
						{ 0, 2, 3 }, { 0, 3, 1 }, { 4, 5, 7 }, { 4, 7, 6 },
						{ 0, 1, 5 }, { 0, 5, 4 }, { 2, 6, 7 }, { 2, 7, 3 },
						{ 0, 4, 6 }, { 0, 6, 2 }, { 1, 3, 7 }, { 1, 7, 5 }
					};
					QVector<Vector3> tris;
					tris.reserve( 36 );
					for ( auto & f : F )
						tris << p[f[0]] << p[f[1]] << p[f[2]];
					scene->drawTriangles( tris.constData(), size_t( tris.size() ), nullptr, true );
				};
				auto axisColor = [this, &axR]( int i, bool hov ) {
					if ( hov )
						scene->setGLColor( 1.0f, 1.0f, 1.0f, 1.0f );
					else
						scene->setGLColor( axR[i][0], axR[i][1], axR[i][2], axR[i][3] );
				};
				auto drawArrow = [&]( int i, bool hov ) {
					Vector3 u;
					u[i] = 1.0f;
					axisColor( i, hov );
					scene->setGLLineWidth( hw );
					scene->drawLine( u * ( L * 0.2f ), u * ( L * 0.92f ) );
					solidCone( u * ( L * 0.92f ), u, L * 0.045f, L * 0.18f );
				};
				auto drawRing = [&]( int i, bool hov ) {
					Vector3 u;
					u[i] = 1.0f;
					axisColor( i, hov );
					scene->setGLLineWidth( hw );
					scene->drawCircle( Vector3(), u, L * 0.85f, 64 );
				};
				auto drawScaleHandle = [&]( int i, bool hov, bool withShaft ) {
					Vector3 u;
					u[i] = 1.0f;
					axisColor( i, hov );
					if ( withShaft ) {
						// modal scale: Blender look, shaft ending in a box
						scene->setGLLineWidth( hw );
						scene->drawLine( u * ( L * 0.2f ), u * ( L * 0.85f ) );
						solidBox( u * ( L * 0.9f ), L * 0.05f );
					} else {
						solidBox( u * ( L * 0.62f ), L * 0.045f );
					}
				};

				if ( gizmoMode ) {
					// modal G/R/S: only the relevant sub-gizmo, only the locked
					// axis once constrained, and for plane moves the two
					// in-plane arrows (the excluded axis is hidden)
					for ( int i = 0; i < 3; i++ ) {
						if ( gizmoAxis > 0 && gizmoAxis != 1 + i )
							continue;
						if ( gizmoMode == 1 && gizmoPlane == 1 + i )
							continue;
						if ( gizmoMode == 1 )
							drawArrow( i, false );
						else if ( gizmoMode == 2 )
							drawRing( i, false );
						else
							drawScaleHandle( i, false, true );
					}
				} else {
					for ( int i = 0; i < 3; i++ ) {
						drawArrow( i, gizmoHover == 1 + i );
						drawScaleHandle( i, gizmoHover == 8 + i, false );
						drawRing( i, gizmoHover == 5 + i );
					}
					// plane-move handles: small quads between axis pairs,
					// coloured by the plane's normal axis (Blender)
					for ( int i = 0; i < 3; i++ ) {
						int j = ( i + 1 ) % 3, k = ( i + 2 ) % 3;
						Vector3 a, b;
						a[j] = 1.0f;
						b[k] = 1.0f;
						bool hov = ( gizmoHover == 12 + i );
						scene->setGLColor( axR[i][0], axR[i][1], axR[i][2], hov ? 0.95f : 0.5f );
						Vector3 q[6] = {
							a * ( L * 0.3f ) + b * ( L * 0.3f ), a * ( L * 0.5f ) + b * ( L * 0.3f ),
							a * ( L * 0.5f ) + b * ( L * 0.5f ),
							a * ( L * 0.3f ) + b * ( L * 0.3f ), a * ( L * 0.5f ) + b * ( L * 0.5f ),
							a * ( L * 0.3f ) + b * ( L * 0.5f )
						};
						scene->drawTriangles( q, 6, nullptr, true );
					}
					// center circle (view-plane move) + white view-rotate ring,
					// both billboarded to the camera
					Matrix vtInv = viewTrans.rotation.inverted();
					Matrix bInv = basis.inverted();
					Vector3 camN = bInv * ( vtInv * Vector3( 0.0f, 0.0f, 1.0f ) );
					scene->setGLColor( 1.0f, 1.0f, 1.0f, gizmoHover == 4 ? 1.0f : 0.6f );
					scene->setGLLineWidth( Settings::lineWidthAxes * 1.2f );
					scene->drawCircle( Vector3(), camN, L * 0.12f, 32 );
					scene->setGLColor( 1.0f, 1.0f, 1.0f, gizmoHover == 11 ? 0.95f : 0.4f );
					scene->setGLLineWidth( Settings::lineWidthAxes * 1.1f );
					scene->drawCircle( Vector3(), camN, L * 1.02f, 64 );
				}
			}

			if ( gizmoMode && ( gizmoAxis > 0 || gizmoPlane > 0 ) ) {
				// constraint guide lines through the pivot, in the same Blender
				// axis colours as the gizmo (plane constraints show both axes)
				static const float gc[3][3] = {
					{ 1.000f, 0.200f, 0.322f },	// X #FF3352
					{ 0.545f, 0.863f, 0.000f },	// Y #8BDC00
					{ 0.157f, 0.565f, 1.000f }	// Z #2890FF
				};
				scene->loadModelViewMatrix( viewTrans );
				scene->setGLLineWidth( Settings::lineWidthAxes * 0.8f );
				for ( int i = 0; i < 3; i++ ) {
					bool show = ( gizmoAxis == 1 + i )
					            || ( gizmoMode == 1 && gizmoPlane > 0 && gizmoPlane != 1 + i );
					if ( !show )
						continue;
					Vector3 unit;
					unit[i] = 1.0f;
					Vector3 a = basis * unit;
					scene->setGLColor( gc[i][0], gc[i][1], gc[i][2], 0.9f );
					scene->drawLine( pivot - a * ( gs * 40.0f ), pivot + a * ( gs * 40.0f ) );
				}
			}

			glDepthMask( GL_TRUE );
			glEnable( GL_DEPTH_TEST );
		}
	}

	// Blender-style wireframe overlay: a black wireframe drawn on top of the
	// solid/shaded render, so the texture shows through the faces. Opaque
	// (depth-tested against the textured geometry) unless X-ray is on, which
	// makes both the geometry (half-transparent) and every edge show through.
	if ( model && wireframeOverlay && !scene->selecting ) {
		auto edgeKey = []( int a, int b ) {
			return ( quint64( std::min( a, b ) ) << 32 ) | quint64( std::max( a, b ) );
		};

		glEnable( GL_DEPTH_TEST );
		glDepthFunc( GL_LEQUAL );
		glDepthMask( GL_FALSE );
		if ( scene->xRay )
			glDisable( GL_DEPTH_TEST );	// X-ray: every edge shows through
		scene->setGLColor( 0.0f, 0.0f, 0.0f, 1.0f );	// Blender wireframe black
		scene->setGLLineWidth( Settings::lineWidthWireframe * wireWidthMul );

		for ( Shape * s : scene->shapes ) {
			if ( !s || s->isHidden() || s->verts.isEmpty() || s->triangles.isEmpty() )
				continue;
			// edited meshes are wired by the edit overlay (keeps selection colours)
			if ( editMode && editShapeBlocks.contains( s->id() ) )
				continue;
			const QSet<int> hidT = editMode ? editHiddenTris.value( s->id() ) : QSet<int>();
			int nv = s->verts.size();

			// pull the wire toward the camera (local space) so it sits just in
			// front of the textured faces instead of z-fighting them
			Transform mv = s->viewTrans();
			Vector3 eyeL = mv.inverted() * Vector3( 0.0f, 0.0f, 0.0f );

			QSet<quint64> eset;
			QVector<Vector3> lines;
			for ( int ti = 0; ti < s->triangles.size(); ti++ ) {
				if ( hidT.contains( ti ) )
					continue;
				const Triangle & t = s->triangles.at( ti );
				if ( t[0] >= nv || t[1] >= nv || t[2] >= nv )
					continue;
				for ( int e = 0; e < 3; e++ ) {
					int a = t[e], b = t[( e + 1 ) % 3];
					quint64 k = edgeKey( a, b );
					if ( !eset.contains( k ) ) {
						eset.insert( k );
						lines << ( eyeL + ( s->verts[a] - eyeL ) * 0.998f )
						      << ( eyeL + ( s->verts[b] - eyeL ) * 0.998f );
					}
				}
			}
			if ( lines.isEmpty() )
				continue;
			scene->loadModelViewMatrix( mv );
			scene->drawLines( lines.constData(), size_t( lines.size() ), nullptr );
		}
		glDepthMask( GL_TRUE );
		glEnable( GL_DEPTH_TEST );
		glDepthFunc( GL_LESS );
	}

	// object-mode selection is indicated by the Blender-style silhouette
	// outline (drawObjectOutlines, right after the scene) - the old coloured
	// wireframe overlay is gone

	// Blender-style edit-mode overlay: black wireframe + black vertex dots,
	// selected elements orange (#FF8500), active element white, translucent
	// orange fill on selected faces, and an orange->black gradient on edges
	// running into selected vertices (vertex mode only). Depth-tested like
	// Blender; positions are pulled slightly toward the eye to avoid z-fights.
	if ( model && editMode && !riggingWeightPaintMode && !vertexPaintMode && !segmentPaintMode && !editShapeBlocks.isEmpty() ) {
		const float dpr = float( devicePixelRatioF() );
		const FloatVector4 colWire( 0.0f, 0.0f, 0.0f, 1.0f );
		const FloatVector4 colSel( 1.0f, 0.522f, 0.0f, 1.0f );		// Blender select #FF8500
		const FloatVector4 colActive( 1.0f, 1.0f, 1.0f, 1.0f );	// active element

		// gather the selection per shape
		QHash<int, QSet<int>> selVerts;
		QHash<int, QSet<quint64>> selEdges;
		QHash<int, QSet<int>> selFaces;
		auto edgeKey = []( int a, int b ) {
			return ( quint64( std::min( a, b ) ) << 32 ) | quint64( std::max( a, b ) );
		};
		for ( const auto & pe : pickedElems ) {
			if ( pe.type == 1 )
				selVerts[pe.shapeBlock].insert( pe.e0 );
			else if ( pe.type == 2 )
				selEdges[pe.shapeBlock].insert( edgeKey( pe.e0, pe.e1 ) );
			else if ( pe.type == 3 )
				selFaces[pe.shapeBlock].insert( pe.e0 );
		}
		const PickedElement * activeElem = pickedElems.isEmpty() ? nullptr : &pickedElems.constLast();

		glEnable( GL_DEPTH_TEST );
		glDepthFunc( GL_LEQUAL );
		glDepthMask( GL_FALSE );
		scene->loadModelViewMatrix( viewTrans );
		Vector3 eye = viewTrans.inverted() * Vector3( 0.0f, 0.0f, 0.0f );

		for ( int wb : editShapeBlocks ) {
			Shape * s = shapeForBlock( wb );
			if ( !s || s->verts.isEmpty() )
				continue;
			Transform wt = shapeRenderTrans( s );
			int nv = s->verts.size();

			// world-space vertices, pulled a hair toward the camera
			QVector<Vector3> wv( nv );
			for ( int i = 0; i < nv; i++ ) {
				Vector3 w = wt * editVertexLocal( s, i );
				wv[i] = eye + ( w - eye ) * 0.997f;
			}

			// unique edge list (hidden triangles excluded, Blender H). Marked
			// quad diagonals are skipped so a tri pair reads as one quad —
			// but only while both halves are visible and the edge is still
			// manifold (otherwise the mark is stale and the edge shows).
			const QSet<int> hiddenT = editHiddenTris.value( wb );
			const QSet<quint64> qmarks = quadMarksFor( wb );
			QHash<quint64, int> markAdj;
			if ( !qmarks.isEmpty() ) {
				for ( int ti = 0; ti < s->triangles.size(); ti++ ) {
					if ( hiddenT.contains( ti ) )
						continue;
					const Triangle & t = s->triangles.at( ti );
					if ( t[0] >= nv || t[1] >= nv || t[2] >= nv
						|| t[0] == t[1] || t[1] == t[2] || t[0] == t[2] )
						continue;
					for ( int e = 0; e < 3; e++ ) {
						quint64 k = edgeKey( t[e], t[( e + 1 ) % 3] );
						if ( qmarks.contains( k ) )
							markAdj[k]++;
					}
				}
			}
			QSet<int> visVerts;
			QSet<quint64> eset;
			QVector<QPair<int, int>> edges;
			for ( int ti = 0; ti < s->triangles.size(); ti++ ) {
				if ( hiddenT.contains( ti ) )
					continue;
				const Triangle & t = s->triangles.at( ti );
				if ( t[0] >= nv || t[1] >= nv || t[2] >= nv )
					continue;
				for ( int e = 0; e < 3; e++ ) {
					int a = t[e], b = t[( e + 1 ) % 3];
					visVerts.insert( a );
					quint64 k = edgeKey( a, b );
					if ( !eset.contains( k ) ) {
						eset.insert( k );
						if ( qmarks.contains( k ) && markAdj.value( k ) == 2 )
							continue;	// interior quad diagonal: not drawn
						edges.append( qMakePair( a, b ) );
					}
				}
			}

			const bool vertMode = bool( pickMode & 1 );
			const QSet<int> & sv = selVerts[wb];
			const QSet<quint64> & se = selEdges[wb];

			// Faces to fill: a face is filled when it is face-selected, OR (only in
			// the matching select mode) all of its verts / all of its edges are
			// selected. Gating the implicit fill on the mode bit keeps face mode
			// from lighting up a de-selected face just because its verts are still
			// covered by selected neighbours (the invert-in-face-mode surprise).
			const bool fillByVerts = bool( pickMode & 1 ) && !sv.isEmpty();
			const bool fillByEdges = bool( pickMode & 2 ) && !se.isEmpty();
			QSet<int> filledTris;
			if ( selFaces.contains( wb ) )
				filledTris = selFaces.value( wb );
			if ( fillByVerts || fillByEdges ) {
				for ( int ti = 0; ti < s->triangles.size(); ti++ ) {
					if ( hiddenT.contains( ti ) )
						continue;
					const Triangle & t = s->triangles.at( ti );
					if ( t[0] >= nv || t[1] >= nv || t[2] >= nv )
						continue;
					bool allV = fillByVerts && sv.contains( t[0] ) && sv.contains( t[1] ) && sv.contains( t[2] );
					bool allE = fillByEdges && se.contains( edgeKey( t[0], t[1] ) )
						&& se.contains( edgeKey( t[1], t[2] ) ) && se.contains( edgeKey( t[2], t[0] ) );
					if ( allV || allE )
						filledTris.insert( ti );
				}
			}

			// Selection (fills, selected edges/verts, outlines) is drawn with
			// the depth test OFF so nearby unconnected geometry can never
			// occlude it - the edit cage stays on top, Blender-style. The plain
			// black wireframe + unselected dots keep the depth test.
			QVector<Vector3> foutline;	// non-active filled faces (orange outline)
			QVector<Vector3> aoutline;	// the active/primary face (white outline)
			if ( !filledTris.isEmpty() ) {
				glDisable( GL_DEPTH_TEST );
				QVector<Vector3> ftris, atris;
				for ( int fi : filledTris ) {
					if ( fi < 0 || fi >= s->triangles.size() )
						continue;
					const Triangle & t = s->triangles.at( fi );
					if ( t[0] >= nv || t[1] >= nv || t[2] >= nv )
						continue;
					bool act = ( activeElem && activeElem->type == 3
					             && activeElem->shapeBlock == wb && activeElem->e0 == fi );
					QVector<Vector3> & dst = act ? atris : ftris;
					dst << wv[t[0]] << wv[t[1]] << wv[t[2]];
					// only the active face gets the white outline (Blender); the
					// rest of the selection stays orange. A marked quad diagonal
					// is never outlined - otherwise selecting a quad would draw
					// its hidden diagonal right back on top of the fill
					QVector<Vector3> & odst = act ? aoutline : foutline;
					for ( int e = 0; e < 3; e++ ) {
						const int a = t[e], b = t[( e + 1 ) % 3];
						const quint64 k = edgeKey( a, b );
						if ( !qmarks.isEmpty() && qmarks.contains( k ) && markAdj.value( k ) == 2 )
							continue;
						odst << wv[a] << wv[b];
					}
				}
				if ( !ftris.isEmpty() ) {
					scene->setGLColor( 1.0f, 0.522f, 0.0f, 0.30f );
					scene->drawTriangles( ftris.constData(), size_t( ftris.size() ), nullptr, true );
				}
				if ( !atris.isEmpty() ) {
					scene->setGLColor( 1.0f, 0.72f, 0.35f, 0.36f );	// active face: lighter
					scene->drawTriangles( atris.constData(), size_t( atris.size() ), nullptr, true );
				}
				glEnable( GL_DEPTH_TEST );
			}

			// base wireframe with per-endpoint colours (gradient in vertex mode)
			QVector<Vector3> lineVerts;
			QVector<FloatVector4> lineCols;
			lineVerts.reserve( edges.size() * 2 );
			lineCols.reserve( edges.size() * 2 );
			for ( const auto & e : edges ) {
				lineVerts << wv[e.first] << wv[e.second];
				FloatVector4 ca = colWire, cb = colWire;
				if ( vertMode ) {
					if ( sv.contains( e.first ) )
						ca = colSel;
					if ( sv.contains( e.second ) )
						cb = colSel;
				}
				lineCols << ca << cb;
			}
			if ( !lineVerts.isEmpty() ) {
				scene->setGLLineWidth( 1.0f * dpr * wireWidthMul );
				scene->drawLines( lineVerts.constData(), size_t( lineVerts.size() ), lineCols.constData() );
			}

			// unselected vertex dots (depth-tested)
			if ( vertMode ) {
				scene->setGLPointSize( vertexPointSize * dpr );
				scene->setGLColor( colWire );
				if ( hiddenT.isEmpty() ) {
					scene->drawPoints( wv.constData(), size_t( nv ) );
				} else {
					QVector<Vector3> visPts;
					visPts.reserve( visVerts.size() );
					for ( int vi : visVerts )
						visPts.append( wv[vi] );
					if ( !visPts.isEmpty() )
						scene->drawPoints( visPts.constData(), size_t( visPts.size() ) );
				}
			}

			// --- selected elements, always on top ---
			glDisable( GL_DEPTH_TEST );

			// outline around every filled face: white in edge/face select mode
			// (Blender), orange in pure vertex mode so the selection still reads
			// as orange instead of white
			if ( !foutline.isEmpty() ) {
				scene->setGLLineWidth( 1.7f * dpr * wireWidthMul );
				scene->setGLColor( colSel );	// non-primary selected faces: orange
				scene->drawLines( foutline.constData(), size_t( foutline.size() ), nullptr );
			}
			if ( !aoutline.isEmpty() ) {
				scene->setGLLineWidth( 1.7f * dpr * wireWidthMul );
				scene->setGLColor( 1.0f, 1.0f, 1.0f, 0.95f );	// active/primary face: white (Blender)
				scene->drawLines( aoutline.constData(), size_t( aoutline.size() ), nullptr );
			}

			// selected edges (any mode), slightly wider like Blender
			if ( selEdges.contains( wb ) ) {
				QVector<Vector3> selLines, actLines;
				for ( const auto & e : edges ) {
					quint64 k = edgeKey( e.first, e.second );
					if ( !se.contains( k ) )
						continue;
					bool act = ( activeElem && activeElem->type == 2 && activeElem->shapeBlock == wb
					             && edgeKey( activeElem->e0, activeElem->e1 ) == k );
					QVector<Vector3> & dst = act ? actLines : selLines;
					dst << wv[e.first] << wv[e.second];
				}
				scene->setGLLineWidth( selLineWidth * dpr );
				if ( !selLines.isEmpty() ) {
					scene->setGLColor( colSel );
					scene->drawLines( selLines.constData(), size_t( selLines.size() ), nullptr );
				}
				if ( !actLines.isEmpty() ) {
					scene->setGLColor( colActive );
					scene->drawLines( actLines.constData(), size_t( actLines.size() ), nullptr );
				}
			}

			// selected / active vertex dots
			if ( vertMode && !sv.isEmpty() ) {
				scene->setGLPointSize( vertexPointSize * dpr );
				QVector<Vector3> selPts, actPts;
				for ( int vi : sv ) {
					if ( vi < 0 || vi >= nv )
						continue;
					bool act = ( activeElem && activeElem->type == 1
					             && activeElem->shapeBlock == wb && activeElem->e0 == vi );
					( act ? actPts : selPts ).append( wv[vi] );
				}
				if ( !selPts.isEmpty() ) {
					scene->setGLColor( colSel );
					scene->drawPoints( selPts.constData(), size_t( selPts.size() ) );
				}
				if ( !actPts.isEmpty() ) {
					scene->setGLColor( colActive );
					scene->drawPoints( actPts.constData(), size_t( actPts.size() ) );
				}
			}

			glEnable( GL_DEPTH_TEST );
		}

		glDepthMask( GL_TRUE );
		glDepthFunc( GL_LESS );
	}

	// picked reference elements outside edit mode (snap targets etc.)
	if ( model && !editMode && !pickedElems.isEmpty() ) {
		glDisable( GL_DEPTH_TEST );
		glDepthMask( GL_FALSE );
		scene->loadModelViewMatrix( viewTrans );
		float ms = std::max( float( Dist ) / 100.0f, 0.005f ) * gizmoSizeMul;

		for ( const auto & pe : pickedElems ) {
			Shape * s = shapeForBlock( pe.shapeBlock );
			if ( !s )
				continue;
			Transform wt = shapeRenderTrans( s );
			int nv = s->verts.size();

			if ( pe.type == 1 ) {
				if ( pe.e0 >= nv )
					continue;
				Vector3 v = wt * editVertexLocal( s, pe.e0 );
				scene->setGLColor( 0.0f, 0.0f, 0.0f, 1.0f );	// contrast halo
				scene->drawSphereSimple( v, ms * 0.62f, 16, 2 );
				scene->setGLColor( 1.0f, 0.5f, 0.0f, 1.0f );	// orange verts
				scene->drawSphereSimple( v, ms * 0.45f, 16, 2 );
			} else if ( pe.type == 2 ) {
				if ( pe.e0 >= nv || pe.e1 >= nv )
					continue;
				Vector3 a = wt * editVertexLocal( s, pe.e0 ), b = wt * editVertexLocal( s, pe.e1 );
				scene->setGLColor( 0.0f, 0.0f, 0.0f, 1.0f );
				scene->setGLLineWidth( Settings::lineWidthAxes * 3.4f );
				scene->drawLine( a, b );
				scene->setGLColor( 1.0f, 0.9f, 0.2f, 1.0f );	// yellow edges
				scene->setGLLineWidth( Settings::lineWidthAxes * 1.6f );
				scene->drawLine( a, b );
			} else if ( pe.type == 3 && pe.e0 >= 0 && pe.e0 < s->triangles.size() ) {
				const Triangle & t = s->triangles.at( pe.e0 );
				if ( t[0] >= nv || t[1] >= nv || t[2] >= nv )
					continue;
				Vector3 tri[3] = { wt * editVertexLocal( s, t[0] ),
					wt * editVertexLocal( s, t[1] ), wt * editVertexLocal( s, t[2] ) };
				// filled translucent face + solid outline
				scene->setGLColor( 1.0f, 0.4f, 0.85f, 0.45f );	// magenta fill
				scene->drawTriangles( tri, 3, nullptr, true );
				scene->setGLColor( 0.0f, 0.0f, 0.0f, 1.0f );
				scene->setGLLineWidth( Settings::lineWidthAxes * 3.2f );
				scene->drawLine( tri[0], tri[1] );
				scene->drawLine( tri[1], tri[2] );
				scene->drawLine( tri[2], tri[0] );
				scene->setGLColor( 1.0f, 0.55f, 0.95f, 1.0f );
				scene->setGLLineWidth( Settings::lineWidthAxes * 1.5f );
				scene->drawLine( tri[0], tri[1] );
				scene->drawLine( tri[1], tri[2] );
				scene->drawLine( tri[2], tri[0] );
			}
		}

		if ( pickedElems.size() > 1 ) {
			// median marker
			Vector3 m = pickedMedian();
			scene->setGLColor( 1.0f, 1.0f, 1.0f, 1.0f );
			scene->setGLLineWidth( Settings::lineWidthAxes );
			for ( int c = 0; c < 3; c++ ) {
				Vector3 d;
				d[c] = ms * 1.5f;
				scene->drawLine( m - d, m + d );
			}
		}

		glDepthMask( GL_TRUE );
		glEnable( GL_DEPTH_TEST );
	}

	// Blender-style origin dots + parent relationship lines for the selection
	if ( model && showOrigins && ( !objSelection.isEmpty() || ( editMode && !editShapeBlocks.isEmpty() ) ) ) {
		const QSet<int> & selN = editMode ? editShapeBlocks : objSelection;
		glDisable( GL_DEPTH_TEST );
		glDepthMask( GL_FALSE );
		scene->loadModelViewMatrix( viewTrans );
		float dpr = float( devicePixelRatioF() );
		for ( int ob : selN ) {
			Node * n = scene->getNode( model, model->getBlockIndex( ob ) );
			if ( !n )
				continue;
			Vector3 o = n->worldTrans().translation;
			scene->setGLPointSize( 9.0f * dpr );
			scene->setGLColor( 0.05f, 0.05f, 0.05f, 1.0f );
			scene->drawPoints( &o, 1 );
			scene->setGLPointSize( 6.0f * dpr );
			// selection colours matching the block list / wireframe:
			// active (last-selected) #FF9D00, other selected #FF7200
			bool isActive = editMode ? ( ob == editShapeBlock ) : ( ob == objActive );
			if ( isActive )
				scene->setGLColor( 1.0f, 0.616f, 0.0f, 1.0f );
			else
				scene->setGLColor( 1.0f, 0.447f, 0.0f, 1.0f );
			scene->drawPoints( &o, 1 );
			// dashed relationship line to the parent's origin
			int pb = model->getParent( ob );
			if ( pb >= 0 && model->blockInherits( model->getBlockIndex( pb ), "NiAVObject" ) ) {
				Node * pn = scene->getNode( model, model->getBlockIndex( pb ) );
				if ( pn ) {
					scene->setGLLineWidth( 1.0f * dpr );
					scene->setGLColor( 0.75f, 0.75f, 0.75f, 0.7f );
					scene->drawDashLine( o, pn->worldTrans().translation, 24 );
				}
			}
		}
		glDepthMask( GL_TRUE );
		glEnable( GL_DEPTH_TEST );
	}

	// The old GL corner axes indicator is replaced by the Blender-style
	// navigation gizmo drawn with QPainter at the end of paintGL().

	cx->stopProgram();
	cx->shrinkCache();

	// Check for errors
	GLenum err;
	while ( ( err = glGetError() ) != GL_NO_ERROR )
		qDebug() << tr( "glview.cpp - GL ERROR (paint): " ) << getGLErrorString( int(err) );

	// 2D overlays drawn over the GL scene with QPainter: the Blender-style
	// navigation gizmo and the 3D cursor (constant screen size, like Blender)
	bool drawSnapMarker = snapIndicator && ( gizmoMode != 0 || elemTransform );
	if ( scene->hasOption( Scene::ShowAxes ) || ( model && showCursor ) || freeCamera || drawSnapMarker || boxSelectDrag || circleSelecting || knifeActive || riggingWeightPaintMode || vertexPaintMode || segmentPaintMode ) {
		// The scene passes (esp. collision wireframe) leave GL state that
		// QPainter inherits: depth test on (so filled overlay shapes get
		// depth-rejected against the geometry, while text glyphs survive - the
		// "gizmo vanishes but XYZ letters stay" bug), a custom blend func, a
		// bound program/VAO, and a non-fill polygon mode. Reset all of it so
		// the QPainter overlay renders predictably.
		if ( QOpenGLContext * glCtx = QOpenGLContext::currentContext() ) {
			QOpenGLFunctions * f = glCtx->functions();
			f->glDisable( GL_DEPTH_TEST );
			f->glDepthMask( GL_TRUE );
			f->glDisable( GL_BLEND );
			f->glDisable( GL_CULL_FACE );
			f->glDisable( GL_SCISSOR_TEST );
			f->glUseProgram( 0 );
			// NOTE: do NOT glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0) here - the
			// element-buffer binding is part of the currently bound VAO's state,
			// and the renderer caches and reuses that VAO. Clearing it corrupts
			// the cached shape's index buffer and crashes the next glDrawElements
			// (seen as a crash when interacting with the collision preview).
		}
		glPolygonMode( GL_FRONT_AND_BACK, GL_FILL );
		QPainter painter( this );
		if ( model && showCursor )
			drawCursorOverlay( painter );
		if ( scene->hasOption( Scene::ShowAxes ) )
			drawNavGizmo( painter );
		if ( drawSnapMarker ) {
			// Blender snap marker: orange square outline + white center dot
			QPointF sp;
			if ( worldToScreen( snapIndicatorPos, sp ) ) {
				painter.setRenderHint( QPainter::Antialiasing, true );
				float r = 7.0f;
				painter.setPen( QPen( QColor( 0xFF, 0x9D, 0x00, 235 ), 2.0 ) );
				painter.setBrush( Qt::NoBrush );
				painter.drawRect( QRectF( sp.x() - r, sp.y() - r, r * 2.0, r * 2.0 ) );
				painter.setPen( Qt::NoPen );
				painter.setBrush( QColor( 255, 255, 255, 235 ) );
				painter.drawEllipse( sp, 1.8, 1.8 );
			}
		}
		if ( freeCamera ) {
			// fly-mode crosshair at the exact screen centre (always visible)
			painter.setRenderHint( QPainter::Antialiasing, true );
			painter.setPen( QPen( QColor( 255, 255, 255, 220 ), 1.6 ) );
			QPointF c( width() * 0.5, height() * 0.5 );
			painter.drawLine( c - QPointF( 9, 0 ), c + QPointF( 9, 0 ) );
			painter.drawLine( c - QPointF( 0, 9 ), c + QPointF( 0, 9 ) );
		}
		if ( boxSelectDrag ) {
			// Blender box-select rectangle: faint fill + dashed white outline
			painter.setRenderHint( QPainter::Antialiasing, false );
			QRect r = QRect( boxSelectStart, boxSelectCur ).normalized();
			painter.setPen( QPen( QColor( 255, 255, 255, 235 ), 1.0, Qt::DashLine ) );
			painter.setBrush( QColor( 255, 255, 255, 26 ) );
			painter.drawRect( r );
		}
		if ( circleSelecting ) {
			// Blender circle-select brush: dashed white circle following the cursor
			painter.setRenderHint( QPainter::Antialiasing, true );
			painter.setPen( QPen( QColor( 255, 255, 255, 235 ), 1.0, Qt::DashLine ) );
			painter.setBrush( ( circlePainting || circleErasing )
				? QBrush( QColor( 255, 255, 255, 18 ) ) : QBrush( Qt::NoBrush ) );
			painter.drawEllipse( circleSelectPos, qreal( circleSelectRadius ), qreal( circleSelectRadius ) );
		}
		if ( knifeActive ) {
			// Blender knife: white cut line, green squares on committed points
			// (snapped ones filled), dashed rubber band to the hover point.
			// Points are re-projected every frame so the line stays glued to
			// the surface while orbiting with MMB.
			painter.setRenderHint( QPainter::Antialiasing, true );
			// a point can fail to project after an MMB orbit puts it behind
			// the camera: segments touching such a point are simply not drawn
			QVector<QPointF> kscr( knifePoints.size() );
			QVector<bool> kok( knifePoints.size() );
			for ( int i = 0; i < knifePoints.size(); i++ )
				kok[i] = worldToScreen( knifePoints.at( i ).world, kscr[i] );
			painter.setPen( QPen( QColor( 255, 255, 255, 240 ), 1.6 ) );
			for ( int i = 0; i + 1 < kscr.size(); i++ )
				if ( kok.at( i ) && kok.at( i + 1 ) )
					painter.drawLine( kscr.at( i ), kscr.at( i + 1 ) );
			if ( knifeHoverValid && !kscr.isEmpty() && kok.constLast() ) {
				QPointF hp;
				if ( worldToScreen( knifeHoverPt.world, hp ) ) {
					painter.setPen( QPen( QColor( 255, 255, 255, 200 ), 1.2, Qt::DashLine ) );
					painter.drawLine( kscr.constLast(), hp );
				}
			}
			const QColor snapGreen( 0x39, 0xE0, 0x63, 245 );
			for ( int i = 0; i < kscr.size(); i++ ) {
				if ( !kok.at( i ) )
					continue;
				const KnifePoint & kp = knifePoints.at( i );
				const bool snapped = ( kp.snapVert >= 0 || kp.edgeA >= 0 );
				painter.setPen( QPen( snapGreen, 1.4 ) );
				painter.setBrush( snapped ? QBrush( snapGreen ) : QBrush( Qt::NoBrush ) );
				const float r = 3.0f;
				painter.drawRect( QRectF( kscr.at( i ).x() - r, kscr.at( i ).y() - r, r * 2.0, r * 2.0 ) );
			}
			if ( knifeHoverValid ) {
				QPointF hp;
				if ( worldToScreen( knifeHoverPt.world, hp ) ) {
					const bool snapped = ( knifeHoverPt.snapVert >= 0 || knifeHoverPt.edgeA >= 0 );
					painter.setPen( QPen( snapped ? snapGreen : QColor( 255, 255, 255, 235 ), 1.4 ) );
					painter.setBrush( Qt::NoBrush );
					painter.drawEllipse( hp, 4.0, 4.0 );
				}
			}
		}
		if ( riggingWeightPaintMode && riggingWeightPaintBrushEnabled && !freeCamera ) {
			// Cyan distinguishes weight painting from the white selection brush.
			painter.setRenderHint( QPainter::Antialiasing, true );
			painter.setPen( QPen( QColor( 0x39, 0xD9, 0xFF, 245 ), 1.5, Qt::DashLine ) );
			painter.setBrush( riggingWeightPaintStroke
				? QBrush( QColor( 0x39, 0xD9, 0xFF, 28 ) ) : QBrush( Qt::NoBrush ) );
			painter.drawEllipse( riggingWeightPaintPos, qreal( riggingWeightPaintRadius ),
				qreal( riggingWeightPaintRadius ) );
		}
		if ( vertexPaintMode && vertexPaintBrushEnabled && !freeCamera ) {
			painter.setRenderHint( QPainter::Antialiasing, true );
			painter.setPen( QPen( QColor( 0xFF, 0x9D, 0x3D, 245 ), 1.5, Qt::DashLine ) );
			painter.setBrush( vertexPaintStroke
				? QBrush( QColor( 0xFF, 0x9D, 0x3D, 28 ) ) : QBrush( Qt::NoBrush ) );
			painter.drawEllipse( vertexPaintPos, qreal( vertexPaintRadius ), qreal( vertexPaintRadius ) );
		}
		if ( segmentPaintMode && segmentPaintBrushEnabled && !freeCamera ) {
			painter.setRenderHint( QPainter::Antialiasing, true );
			painter.setPen( QPen( QColor( 0x57, 0xE3, 0x68, 245 ), 1.5, Qt::DashLine ) );
			painter.setBrush( segmentPaintStroke
				? QBrush( QColor( 0x57, 0xE3, 0x68, 28 ) ) : QBrush( Qt::NoBrush ) );
			painter.drawEllipse( segmentPaintPos, qreal( segmentPaintRadius ), qreal( segmentPaintRadius ) );
		}
		painter.end();

		// QPainter changes GL state behind the renderer's back (bound program,
		// blending, scissor); reset everything that would corrupt the next
		// selection/picking render, which reuses this context.
		// IMPORTANT: unbind through the renderer, not with a raw
		// glUseProgram(0) — the raw call desynced the renderer's cached
		// current program, so the next frame's first draw with the SAME
		// cached program (typically lines.prog, i.e. the ground grid and
		// origin axes) skipped the rebind and rendered with program 0:
		// this was the "no grid until something is clicked" startup bug.
		if ( scene->haveRenderer() )
			scene->renderer->stopProgram();
		if ( QOpenGLContext * glCtx = QOpenGLContext::currentContext() ) {
			QOpenGLFunctions * f = glCtx->functions();
			f->glActiveTexture( GL_TEXTURE0 );
		}
		glDisable( GL_BLEND );
		glDisable( GL_SCISSOR_TEST );
		glDisable( GL_STENCIL_TEST );
		glEnable( GL_DEPTH_TEST );
		glDepthMask( GL_TRUE );
	}

#if DEBUG_FRAME_TIME
	glFlush();
	glFinish();

	static float	frameTimes[8] = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
	static unsigned int	frameTimeIndex = 0;
	auto	t = std::chrono::steady_clock::now();
	double	dt = double( std::chrono::duration_cast< std::chrono::microseconds >( t - prvTime ).count() ) / 8000.0;
	frameTimes[frameTimeIndex & 7] = float( dt );
	frameTimeIndex++;
	float	avgTime = 0.0f;
	for ( int i = 0; i < 8; i++ )
		avgTime += frameTimes[i];
	std::fprintf( stderr, "Average frame time = %.2f ms\n", avgTime );
#endif

	// drain the post-compile corrective repaints (see glview.h)
	if ( postCompileRepaints > 0 ) {
		postCompileRepaints--;
		QTimer::singleShot( 16, this, [this]() { update(); } );
	}

	emit paintUpdate();
}

void GLView::update()
{
	if ( !isExposed() ) {
		QOpenGLWindow::update();
	} else {
		// work around 5 ms delay to update()
		if ( !updatePending )
			QCoreApplication::postEvent( this, new QEvent( QEvent::UpdateRequest ), Qt::HighEventPriority );
		updatePending = 10;
	}
}


void GLView::resizeGL( int width, int height )
{
	pixelWidth = width;
	pixelHeight = height;

	if ( !isValid() )
		return;
	auto	prvContext = pushGLContext();

	aspect = GLdouble(width) / GLdouble(height);
	if ( !scene->renderer ) [[unlikely]]
		glViewport( 0, 0, width, height );
	else
		scene->renderer->setViewport( 0, 0, width, height );

	glDisable( GL_FRAMEBUFFER_SRGB );
	glClearColor( cfg.background.red(), cfg.background.green(), cfg.background.blue(), cfg.background.alpha() );

	popGLContext( prvContext );
}

void GLView::resizeEvent( QResizeEvent * e )
{
	double	p = devicePixelRatioF();
	resizeGL( int( p * e->size().width() + 0.5 ), int( p * e->size().height() + 0.5 ) );
}

void GLView::setFrontalLight( bool frontal )
{
	frontalLight = frontal;
	update();
}

static float convertBrightnessValue( int value )
{
	if ( value < 720 ) {
		// lower half of the slider range: sRGB curve from 0.0 to 1.0
		if ( value < 1 )
			return 0.0f;
		if ( value <= 29 )
			return float(value) / (720.0f * 12.92f);
		return float(std::pow((float(value) + 39.6f) / 759.6f, 2.4f));
	}
	// upper half of the slider range: exponential from 1.0 to 16.0
	if ( value == 720 )
		return 1.0f;
	if ( value >= 1440 )
		return 16.0f;
	return float(std::exp2(float(value - 720) / 180.0f));
}

void GLView::setBrightness( int value )
{
	brightnessScale = convertBrightnessValue( value );
	update();
}

void GLView::setLightLevel( int value )
{
	brightnessL = convertBrightnessValue( value );
	update();
}

void GLView::setLightColor( int value )
{
	lightColor = float( value ) / 720.0f - 1.0f;
	lightColor = lightColor * float( std::sqrt(std::fabs(lightColor)) );
	// color temperature = 6548.04 * exp(lightColor * 2.0401036)
	update();
}

void GLView::setToneMapping( int value )
{
	toneMapping = float( std::pow( 4.22978723f, float( value - 1440 ) / 720.0f ) );
	update();
}

void GLView::setAmbient( int value )
{
	ambient = convertBrightnessValue( value );
	update();
}

void GLView::setEnvMapRotation( int angle )
{
	envMapRotation = float( angle ) * 0.25f;	// Divide by 4 because sliders are -720 <-> 720
	update();
}

void GLView::setGlowScale( int value )
{
	glowScale = convertBrightnessValue( value );
	update();
}

void GLView::setDebugMode( DebugMode mode )
{
	debugMode = mode;
}

void GLView::setVisMode( Scene::VisMode mode, bool checked )
{
	if ( checked ) {
		scene->visMode |= mode;
	} else {
		if ( mode & scene->visMode & Scene::VisSilhouette ) {
			auto	prvContext = pushGLContext();
			glClearColor( cfg.background.red(), cfg.background.green(), cfg.background.blue(), cfg.background.alpha() );
			popGLContext( prvContext );
		}
		scene->visMode &= ~mode;
	}

	update();
}

typedef void (Scene::* DrawFunc)( void );

static int indexAt(
	NifModel * model, Scene * scene, QList<DrawFunc> drawFunc, const QPointF & pos, int & furn, bool shiftModifier )
{
	// Color Key O(1) selection
	//	Open GL 3.0 says glRenderMode is deprecated
	//	ATI OpenGL API implementation of GL_SELECT corrupts NifSkope memory
	//
	// Create FBO for sharp edges and no shading.
	// Texturing, blending, dithering, lighting and smooth shading should be disabled.
	// The FBO can be used for the drawing operations to keep the drawing operations invisible to the user.

	auto	context = scene->renderer;
	std::int32_t	viewport[4];
	context->getViewport().convertToInt32( viewport );

	// Create new FBO with multisampling disabled
	QOpenGLFramebufferObjectFormat fboFmt;
	fboFmt.setTextureTarget( GL_TEXTURE_2D );
	fboFmt.setInternalTextureFormat( GL_RGBA8 );
	fboFmt.setAttachment( QOpenGLFramebufferObject::Attachment::CombinedDepthStencil );

	QOpenGLFramebufferObject fbo( viewport[2], viewport[3], fboFmt );
	fbo.bind();

	float	savedClearColor[4];
	glGetFloatv( GL_COLOR_CLEAR_VALUE, savedClearColor );

	glDisable( GL_MULTISAMPLE );
	glDisable( GL_LINE_SMOOTH );
	glDisable( GL_POLYGON_SMOOTH );
	glDisable( GL_BLEND );
	glDisable( GL_DITHER );
	glEnable( GL_DEPTH_TEST );
	glDepthMask( GL_TRUE );
	glDepthFunc( GL_LEQUAL );
	glPolygonMode( GL_FRONT_AND_BACK, GL_FILL );
	glClearColor( 0.0f, 0.0f, 0.0f, 0.0f );
	glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );

	context->setGlobalUniforms();
	context->setDefaultVertexAttribs( Scene::defaultAttrMask, Scene::defaultVertexAttrs );

	// Rasterize the scene
	int	selectionFlags = int( Scene::SelObject );
	if ( scene->isSelModeVertex() )
		selectionFlags |= int( Scene::SelVertex );
	else if ( shiftModifier )
		selectionFlags |= int( Scene::SelTriangle );
	scene->selecting = (unsigned char) selectionFlags;
	for ( DrawFunc df : drawFunc ) {
		(scene->*df)();
	}
	scene->selecting = 0;

	context->stopProgram();
	context->shrinkCache();

	glClearColor( savedClearColor[0], savedClearColor[1], savedClearColor[2], savedClearColor[3] );
	glEnable( GL_DITHER );
	glEnable( GL_MULTISAMPLE );

	fbo.release();

	QImage img( fbo.toImage() );
	// disable premultiplied alpha
	img.reinterpretAsFormat( QImage::Format_ARGB32 );
	std::uint32_t pixel = std::uint32_t( img.pixel( pos.toPoint() ) );

#ifndef QT_NO_DEBUG
	img.save( "fbo.png" );
#endif

	// Convert BGRA to RGBA
	pixel = ( pixel & 0xFF00FF00U ) | ( ( pixel & 0xFFU ) << 16 ) | ( ( pixel >> 16 ) & 0xFFU );

	// Decode:
	// R = (id & 0x000000FF) >> 0
	// G = (id & 0x0000FF00) >> 8
	// B = (id & 0x00FF0000) >> 16
	// A = (id & 0xFF000000) >> 24

	int choose = getIDFromColorKey( pixel );

	// Pick BSFurnitureMarker
	if ( choose > 0 && selectionFlags == int( Scene::SelObject ) ) {
		int b = choose & 0x0ffff;
		int p = ( choose >> 16 ) & 0x0ffff;
		auto furnBlock = model->getBlockIndex( b, "BSFurnitureMarker" );

		if ( furnBlock.isValid() && model->getIndex( model->getIndex( furnBlock, "Positions" ), p ).isValid() ) {
			furn = p;
			choose = b;
		}
	}

	//qDebug() << "Key:" << a << " R" << pixel.red() << " G" << pixel.green() << " B" << pixel.blue();
	return choose;
}

QModelIndex GLView::indexAt( const QPointF & pos, bool shiftModifier )
{
	if ( !(model && isValid() && isVisible() && height() && scene->renderer) )
		return QModelIndex();

	QList<DrawFunc> df;

	if ( scene->hasOption(Scene::ShowCollision) )
		df << &Scene::drawHavok;

	if ( scene->hasOption(Scene::ShowNodes) )
		df << &Scene::drawNodes;

	if ( scene->hasOption(Scene::ShowMarkers) )
		df << &Scene::drawFurn;

	df << &Scene::drawShapes;

	auto	prvContext = pushGLContext();

	double	p = devicePixelRatioF();
	int	wp = pixelWidth;
	int	hp = pixelHeight;
	QPointF	posScaled( pos );
	posScaled *= p;
	scene->renderer->setViewport( 0, 0, wp, hp );
	glProjection( int( posScaled.x() + 0.5 ), int( posScaled.y() + 0.5 ) );

	int choose = -1, furn = -1;
	choose = ::indexAt( model, scene, df, posScaled, /*out*/ furn, shiftModifier );

	popGLContext( prvContext );

	QModelIndex chooseIndex;

	if ( scene->isSelModeVertex() ) {
		// Vertex
		int block = ( choose >> 16 ) & 0xFFFF;
		int vert = choose & 0xFFFF;

		auto shape = scene->shapes.value( block );
		if ( shape )
			chooseIndex = shape->vertexAt( vert );
	} else if ( choose >= 0 ) {
		// Block Index
		chooseIndex = model->getBlockIndex( !shiftModifier ? choose : ( choose & 0x7FFF ) );
		if ( shiftModifier ) {
			// Triangle
			if ( auto node = scene->getNode( scene->nifModel, chooseIndex ); node ) {
				if ( auto shape = dynamic_cast< Shape * >( node ); shape ) {
					auto	triangleIndex = shape->triangleAt( int( (unsigned int) choose >> 15 ) );
					if ( triangleIndex.isValid() )
						chooseIndex = triangleIndex;
				}
			}
		} else if ( furn != -1 ) {
			// Furniture Row @ Block Index
			chooseIndex = model->getIndex( model->getIndex( chooseIndex, "Positions" ), furn );
		}
	}

	return chooseIndex;
}

void GLView::center()
{
	doCenter = true;
	update();
}

void GLView::move( float x, float y, float z )
{
	Pos += Matrix::euler( deg2rad(Rot[0]), deg2rad(Rot[1]), deg2rad(Rot[2]) ).inverted() * Vector3( x, y, z );
	updateViewpoint();
	update();
}

void GLView::rotate( float x, float y, float z )
{
	// Blender's default Auto Perspective behaviour: orbiting away from an
	// axis-aligned orthographic view returns to perspective. This keeps the
	// horizontal ground grid visible after MMB or numpad orbit, while a static
	// arbitrary User Orthographic view (entered with Numpad 5) remains gridless.
	if ( !perspectiveMode && view != ViewWalk )
		setProjection( true );

	// orbit around the selection (Blender preference): keep the selection
	// center fixed in view space while the rotation changes
	Matrix R1;
	bool orbitFix = false;
	Vector3 orbitC;
	if ( orbitSelection && model && scene && view != ViewWalk && !freeCamera ) {
		int n = 0;
		const QSet<int> & sel = editMode ? editShapeBlocks : objSelection;
		for ( int b : sel ) {
			Node * nd = scene->getNode( model, model->getBlockIndex( b ) );
			if ( nd ) {
				orbitC += nd->worldTrans().translation;
				n++;
			}
		}
		if ( n ) {
			orbitC = orbitC / float( n );
			R1.fromEuler( deg2rad( Rot[0] ), deg2rad( Rot[1] ), deg2rad( Rot[2] ) );
			orbitFix = true;
		}
	}

	FloatVector4	tmp( x, y, z, 0.0f );
	tmp += FloatVector4::convertVector3( &(Rot[0]) );
	( tmp - ( tmp / 360.0f ).roundValues() * 360.0f ).convertToVector3( &(Rot[0]) );	// wrap to -180.0 to 180.0

	if ( orbitFix ) {
		// view(w) = R * ( w + Pos ); solve Pos' so the center stays put
		Matrix R2;
		R2.fromEuler( deg2rad( Rot[0] ), deg2rad( Rot[1] ), deg2rad( Rot[2] ) );
		Pos = R2.inverted() * ( R1 * ( orbitC + Pos ) ) - orbitC;
	}

	updateViewpoint();
	update();
}

void GLView::rotateLight( float x, float z )
{
	declination += x;
	planarAngle -= z;
	declination -= float( roundFloat( declination / 360.0f ) ) * 360.0f;		// wrap to -180.0 to 180.0
	planarAngle -= float( roundFloat( planarAngle / 360.0f ) ) * 360.0f;
	lightVisTimer->start( lightVisTimeout );
	setVisMode( Scene::VisLightPos, true );
}

void GLView::setCenter()
{
	Node * node = scene->getNode( model, scene->currentBlock );

	if ( node ) {
		// Center on selected node
		BoundSphere bs = node->bounds();

		if ( bs.radius > 0 ) {
			Dist = bs.radius * 1.2;
		}

		this->setPosition( -bs.center );
	} else {
		// Center on entire mesh
		BoundSphere bs = scene->bounds();

		if ( bs.radius < scale() )
			bs.radius = 1024.0 * scale();

		Dist = bs.radius * 1.2;
		Zoom = 1.0;

		Pos = -bs.center;
	}
}

void GLView::frameSelected()
{
	// Blender Numpad-.: center + zoom the camera on the selection; with no
	// selection, frame the whole model like Center does
	if ( !model || !scene ) {
		center();
		return;
	}
	Vector3 lo, hi;
	bool have = false;
	auto grow = [&]( const Vector3 & p ) {
		if ( !have ) {
			lo = hi = p;
			have = true;
			return;
		}
		for ( int i = 0; i < 3; i++ ) {
			lo[i] = std::min( lo[i], p[i] );
			hi[i] = std::max( hi[i], p[i] );
		}
	};
	if ( editMode && !pickedElems.isEmpty() ) {
		for ( const PickedElement & pe : pickedElems ) {
			grow( pe.wA );
			if ( pe.type >= 2 )
				grow( pe.wB );
			if ( pe.type == 3 )
				grow( pe.wC );
		}
	} else if ( !editMode && !objSelection.isEmpty() ) {
		for ( Shape * s : scene->shapes ) {
			if ( !s || !objSelection.contains( s->id() ) )
				continue;
			Transform wt = shapeRenderTrans( s );
			if ( s->verts.isEmpty() ) {
				grow( wt * Vector3() );
			} else {
				for ( const Vector3 & v : s->verts )
					grow( wt * v );
			}
		}
	}
	if ( !have ) {
		center();
		return;
	}
	Vector3 c = ( lo + hi ) * 0.5f;
	float radius = ( hi - c ).length();
	Pos = -c;
	Dist = std::max( radius * 1.2f, 0.5f );
	Zoom = 1.0;
	update();
}

void GLView::frameAll()
{
	if ( !scene )
		return;
	BoundSphere bs = scene->bounds();
	if ( bs.radius < scale() )
		bs.radius = 1024.0f * scale();
	Pos = -bs.center;
	Dist = bs.radius * 1.2f;
	Zoom = 1.0;
	update();
}

bool GLView::handleBlenderNumpad( int key, Qt::KeyboardModifiers modifiers, bool trigger )
{
	const Qt::KeyboardModifiers commandMods = modifiers
		& ( Qt::ControlModifier | Qt::ShiftModifier | Qt::AltModifier );
	const bool keypad = modifiers.testFlag( Qt::KeypadModifier );

	// Home is part of Blender's view-navigation set even though it is not on
	// every compact numpad.
	if ( !keypad ) {
		if ( key != Qt::Key_Home || commandMods != Qt::NoModifier )
			return false;
		if ( trigger )
			frameAll();
		return true;
	}

	const bool ctrl = commandMods.testFlag( Qt::ControlModifier );
	const bool shift = commandMods.testFlag( Qt::ShiftModifier );
	const bool alt = commandMods.testFlag( Qt::AltModifier );
	if ( alt )
		return false; // Ctrl+Alt+Num0 remains reserved for future active-camera support.

	auto setAxisView = [this, trigger]( ViewState state ) {
		if ( trigger ) {
			setProjection( false );
			setOrientation( state, false );
		}
	};
	auto orbit = [this, trigger]( float pitch, float yaw ) {
		if ( trigger ) {
			view = ViewUser;
			rotate( pitch, 0.0f, yaw );
		}
	};
	auto pan = [this, trigger]( float x, float y ) {
		if ( trigger ) {
			const float step = std::max( float( Dist / Zoom ) * 0.10f, scale() );
			move( x * step, y * step, 0.0f );
		}
	};

	switch ( key ) {
	case Qt::Key_1:
		if ( shift ) return false;
		setAxisView( ctrl ? ViewBack : ViewFront );
		return true;
	case Qt::Key_3:
		if ( shift ) return false;
		setAxisView( ctrl ? ViewLeft : ViewRight );
		return true;
	case Qt::Key_7:
		if ( shift ) return false;
		setAxisView( ctrl ? ViewBottom : ViewTop );
		return true;
	case Qt::Key_2:
		if ( ctrl ) return false;
		if ( shift ) pan( 0.0f, 1.0f ); else orbit( 15.0f, 0.0f );
		return true;
	case Qt::Key_4:
		if ( ctrl ) return false;
		if ( shift ) pan( 1.0f, 0.0f ); else orbit( 0.0f, -15.0f );
		return true;
	case Qt::Key_6:
		if ( ctrl ) return false;
		if ( shift ) pan( -1.0f, 0.0f ); else orbit( 0.0f, 15.0f );
		return true;
	case Qt::Key_8:
		if ( ctrl ) return false;
		if ( shift ) pan( 0.0f, -1.0f ); else orbit( -15.0f, 0.0f );
		return true;
	case Qt::Key_5:
		if ( commandMods != Qt::NoModifier ) return false;
		if ( trigger ) setProjection( !perspectiveMode );
		return true;
	case Qt::Key_9:
		if ( commandMods != Qt::NoModifier ) return false;
		if ( trigger ) flipOrientation();
		return true;
	case Qt::Key_Plus:
		if ( commandMods != Qt::NoModifier ) return false;
		if ( trigger ) setZoom( float( Zoom ) * 1.2f );
		return true;
	case Qt::Key_Minus:
		if ( commandMods != Qt::NoModifier ) return false;
		if ( trigger ) setZoom( float( Zoom ) / 1.2f );
		return true;
	case Qt::Key_Period:
	case Qt::Key_Comma:
		if ( commandMods != Qt::NoModifier ) return false;
		if ( trigger ) frameSelected();
		return true;
	case Qt::Key_Slash:
		if ( commandMods != Qt::NoModifier ) return false;
		if ( trigger ) {
			const bool isolated = editMode ? !editHiddenTris.isEmpty()
				: ( scene && ( !scene->hiddenNodes.isEmpty() || scene->soloNode >= 0 ) );
			if ( isolated ) restoreAllVisibility(); else isolateSelected();
		}
		return true;
	default:
		break;
	}
	return false;
}

void GLView::setDistance( float x )
{
	Dist = x;
	update();
}

void GLView::setPosition( float x, float y, float z )
{
	Pos = { x, y, z };
	update();
}

void GLView::setPosition( const Vector3 & v )
{
	Pos = v;
	update();
}

void GLView::setProjection( bool isPersp )
{
	if ( perspectiveMode == isPersp )
		return;
	perspectiveMode = isPersp;
	update();
	emit projectionChanged( perspectiveMode );
}

void GLView::setRotation( float x, float y, float z )
{
	Rot = { x, y, z };
	update();
}

void GLView::setZoom( float z )
{
	Zoom = std::min< float >( std::max< float >( z, ZOOM_MIN ), ZOOM_MAX );

	update();
}


void GLView::flipOrientation()
{
	ViewState tmp = ViewDefault;

	switch ( view ) {
	case ViewTop:
		tmp = ViewBottom;
		break;
	case ViewBottom:
		tmp = ViewTop;
		break;
	case ViewLeft:
		tmp = ViewRight;
		break;
	case ViewRight:
		tmp = ViewLeft;
		break;
	case ViewFront:
		tmp = ViewBack;
		break;
	case ViewBack:
		tmp = ViewFront;
		break;
	case ViewUser:
	default:
		view = tmp;
		if ( Node * node = scene->getNode( model, scene->currentBlock ); node )
			Pos = node->bounds().center * -2.0f - Pos;
		else
			Pos = scene->bounds().center * -2.0f - Pos;
		Rot[0] = ( Rot[0] < 0.0f ? -180.0f : 180.0f ) - Rot[0];
		Rot[1] *= -1.0f;
		Rot[2] = ( Rot[2] < 0.0f ? 180.0f : -180.0f ) + Rot[2];
		update();
		return;
	}

	setOrientation( tmp, false );
}

void GLView::setOrientation( GLView::ViewState state, bool recenter )
{
	if ( state == view )
		return;

	if ( int i = int( state ) - int( ViewTop ); i >= 0 && i <= 5 ) {
		Rot = viewRotations[i];
		update();
	}

	view = state;

	// Recenter
	if ( recenter )
		center();
}

GLView::ViewState GLView::axisAlignedViewState() const
{
	Matrix current;
	current.fromEuler( deg2rad( Rot[0] ), deg2rad( Rot[1] ), deg2rad( Rot[2] ) );
	for ( int i = 0; i < 6; i++ ) {
		Matrix candidate;
		candidate.fromEuler( deg2rad( viewRotations[i][0] ), deg2rad( viewRotations[i][1] ),
			deg2rad( viewRotations[i][2] ) );
		float maxDifference = 0.0f;
		for ( int row = 0; row < 3; row++ )
			for ( int column = 0; column < 3; column++ )
				maxDifference = std::max( maxDifference,
					std::fabs( current( row, column ) - candidate( row, column ) ) );
		if ( maxDifference <= 1.0e-4f )
			return ViewState( int( ViewTop ) + i );
	}
	return ViewDefault;
}

void GLView::updateViewpoint()
{
	switch ( view ) {
	case ViewTop:
	case ViewBottom:
	case ViewLeft:
	case ViewRight:
	case ViewFront:
	case ViewBack:
	case ViewUser:
		emit viewpointChanged();
		break;
	default:
		break;
	}
}

void GLView::flush()
{
	if ( textures )
		textures->flush();
}


/*
 *  NifModel
 */

void GLView::setNif( NifModel * nif )
{
	if ( model ) {
		// disconnect( model ) may not work with new Qt5 syntax...
		// it says the calls need to remain symmetric to the connect() ones.
		// Otherwise, use QMetaObject::Connection
		disconnect( model );
	}

	model = nif;

	if ( model ) {
		connect( model, &NifModel::dataChanged, this, &GLView::dataChanged );
		connect( model, &NifModel::linksChanged, this, &GLView::modelLinked );
		connect( model, &NifModel::modelReset, this, &GLView::modelChanged );
		connect( model, &NifModel::destroyed, this, &GLView::modelDestroyed );
		Dist = ( model->getBSVersion() < 170 ? 1228.8f : 19.2f );
	}

	doCompile = 2;
}

void GLView::setCurrentIndex( const QModelIndex & index )
{
	if ( !( model && index.model() == model ) )
		return;

	scene->currentBlock = model->getBlockIndex( index );
	scene->currentIndex = index.sibling( index.row(), 0 );

	if ( soloMode )
		updateSoloNode();

	update();
}

void GLView::updateSoloNode()
{
	int solo = -1;

	if ( soloMode && model && scene->currentBlock.isValid() ) {
		// walk up to the nearest NiAVObject so properties/shaders resolve to their owner
		int b = model->getBlockNumber( QModelIndex( scene->currentBlock ) );
		while ( b >= 0 && !model->blockInherits( model->getBlockIndex( b ), "NiAVObject" ) )
			b = model->getParent( b );
		solo = b;
	}

	if ( scene->soloNode != solo ) {
		scene->soloNode = solo;
		update();
	}
}

void GLView::setSoloMode( bool enable )
{
	soloMode = enable;
	updateSoloNode();

	if ( !enable && scene->soloNode >= 0 ) {
		scene->soloNode = -1;
		update();
	}
}

void GLView::setSoloBlock( int blockNumber )
{
	scene->soloNode = blockNumber;
	update();
}

/*
 *  Modal transform gizmo (Blender style: G/R/S, X/Y/Z, LMB commit, Esc cancel)
 */

float GLView::gizmoSnapStep = 1.0f;
float GLView::gizmoRotSnapDeg = 5.0f;
float GLView::gizmoSizeMul = 1.75f;
float GLView::wireWidthMul = 1.0f;
float GLView::vertexPointSize = 5.0f;
float GLView::selLineWidth = 2.0f;

void GLView::setFreeCamera( bool on )
{
	if ( freeCamera == on )
		return;
	if ( on && riggingWeightPaintStroke ) {
		emit riggingWeightStrokeEnded( false );
		riggingWeightPaintStroke = false;
		mouseButtonState &= ~std::uint32_t( Qt::LeftButton );
	}
	if ( on && segmentPaintStroke ) {
		emit segmentPaintStrokeEnded( false );
		segmentPaintStroke = false;
		mouseButtonState &= ~std::uint32_t( Qt::LeftButton );
	}
	freeCamera = on;
	kbdState = 0;
	if ( on ) {
		requestActivate();
		// grab the keyboard for the whole flight: transient focus steals
		// (tooltips, dock updates) must not stop WASD delivery
		setKeyboardGrabEnabled( true );
		setCursor( Qt::BlankCursor );	// FPS-style: hide cursor, show a crosshair
		QCursor::setPos( mapToGlobal( QPoint( width() / 2, height() / 2 ) ) );
		lastPos = QPointF( width() * 0.5, height() * 0.5 );
		emit gizmoStatus( tr( "Free camera: move the mouse to look, WASD to fly, Q/E down/up, hold Shift to speed up (Shift+F or Esc to exit)" ) );
	} else {
		setKeyboardGrabEnabled( false );
		if ( riggingWeightPaintBrushActive() ) {
			riggingWeightPaintPos = QPointF( mapFromGlobal( QCursor::pos() ) );
			setCursor( Qt::BlankCursor );
			emit gizmoStatus( tr( "Weight Paint - Brush: LMB paints, wheel = zoom, Tab = Object Mode, RMB/Esc = done" ) );
		} else if ( vertexPaintBrushActive() ) {
			vertexPaintPos = QPointF( mapFromGlobal( QCursor::pos() ) );
			setCursor( Qt::BlankCursor );
			emit gizmoStatus( tr( "Vertex Paint - Brush: LMB paints, wheel = zoom, Tab = Object Mode, RMB/Esc = done" ) );
		} else if ( segmentPaintBrushActive() ) {
			segmentPaintPos = QPointF( mapFromGlobal( QCursor::pos() ) );
			setCursor( Qt::BlankCursor );
			emit gizmoStatus( tr( "Segment Paint - Brush: LMB assigns/removes faces, wheel = zoom, Tab = Object Mode" ) );
		} else {
			unsetCursor();
			emit gizmoStatus( riggingWeightPaintMode
				? tr( "Weight Paint - Select: 1/2/3 vertex/edge/face, Ctrl+X fills, Tab = brush" )
				: vertexPaintMode ? tr( "Vertex Paint - Select: 1/2/3 vertex/edge/face, Tab = brush" )
				: segmentPaintMode ? tr( "Segment Paint - Face Select: selection masks painting, Tab = brush" )
				: QString() );
		}
	}
	update();
}

void GLView::freeCameraLook( float dPitch, float dYaw )
{
	// eye_world = -Pos + R^-1 * (0,0,2*Dist); keep it fixed while rotating so
	// the camera looks around its own position instead of orbiting the scene
	Matrix R;
	R.fromEuler( deg2rad( Rot[0] ), deg2rad( Rot[1] ), deg2rad( Rot[2] ) );
	Rot[0] += dPitch;
	Rot[2] += dYaw;
	Rot[0] = std::min( std::max( Rot[0], -179.9f ), 179.9f );
	Matrix R2;
	R2.fromEuler( deg2rad( Rot[0] ), deg2rad( Rot[1] ), deg2rad( Rot[2] ) );
	Vector3 d( 0.0f, 0.0f, 2.0f * float( Dist ) );
	Pos += R2.inverted() * d - R.inverted() * d;
	update();
}

/*
 *  Blender-style navigation gizmo (top-right axis-ball widget)
 */

void GLView::navGizmoLayout( QPointF & center, float & radius, float & ballRadius ) const
{
	radius = 38.0f;
	ballRadius = 10.0f;
	center = QPointF( width() - radius - 18.0f, radius + 18.0f );
}

void GLView::navGizmoBalls( QPointF pos[6], float depth[6] ) const
{
	QPointF center;
	float R, ballR;
	navGizmoLayout( center, R, ballR );

	Matrix vtr;
	vtr.fromEuler( deg2rad( Rot[0] ), deg2rad( Rot[1] ), deg2rad( Rot[2] ) );
	float d = R - ballR;
	// world axis k appears in view space at column k of the rotation matrix;
	// screen y is flipped, view-space z is depth (positive = toward the camera)
	for ( int k = 0; k < 3; k++ ) {
		float sx = vtr( 0, k ), sy = vtr( 1, k ), dz = vtr( 2, k );
		pos[k * 2]     = center + QPointF(  sx * d, -sy * d );  depth[k * 2]     =  dz;  // +axis
		pos[k * 2 + 1] = center + QPointF( -sx * d,  sy * d );  depth[k * 2 + 1] = -dz;  // -axis
	}
}

int GLView::navGizmoHitTest( const QPointF & p ) const
{
	QPointF center;
	float R, ballR;
	navGizmoLayout( center, R, ballR );

	QPointF pos[6];
	float depth[6];
	navGizmoBalls( pos, depth );

	int best = -1;
	float bestDepth = -1.0e9f;
	float hitR = ballR + 3.0f;
	for ( int i = 0; i < 6; i++ ) {
		QPointF d = p - pos[i];
		if ( ( d.x() * d.x() + d.y() * d.y() ) <= hitR * hitR && depth[i] > bestDepth ) {
			best = i;
			bestDepth = depth[i];
		}
	}
	if ( best >= 0 )
		return best;

	// inside the surrounding circle -> orbit ring
	QPointF dc = p - center;
	if ( ( dc.x() * dc.x() + dc.y() * dc.y() ) <= R * R )
		return 6;
	return -1;
}

void GLView::snapToAxis( int axis )
{
	int k = axis / 2;              // 0 = X, 1 = Y, 2 = Z
	bool neg = ( axis & 1 );

	Vector3 back( 0.0f, 0.0f, 0.0f );
	back[k] = neg ? -1.0f : 1.0f;  // view "back" axis = the clicked world axis
	// choose an up vector that is not parallel to the view direction (Z-up world)
	Vector3 up = ( k == 2 ) ? Vector3( 0.0f, 1.0f, 0.0f ) : Vector3( 0.0f, 0.0f, 1.0f );
	Vector3 right = Vector3::crossproduct( up, back ); right.normalize();
	up = Vector3::crossproduct( back, right ); up.normalize();

	Matrix R;
	R( 0, 0 ) = right[0]; R( 0, 1 ) = right[1]; R( 0, 2 ) = right[2];
	R( 1, 0 ) = up[0];    R( 1, 1 ) = up[1];    R( 1, 2 ) = up[2];
	R( 2, 0 ) = back[0];  R( 2, 1 ) = back[1];  R( 2, 2 ) = back[2];

	float x, y, z;
	R.toEuler( x, y, z );
	view = ViewUser;
	setRotation( rad2deg( x ), rad2deg( y ), rad2deg( z ) );
	updateViewpoint();
}

void GLView::drawNavGizmo( QPainter & painter )
{
	QPointF center;
	float R, ballR;
	navGizmoLayout( center, R, ballR );

	QPointF pos[6];
	float depth[6];
	navGizmoBalls( pos, depth );

	painter.setRenderHint( QPainter::Antialiasing, true );
	painter.setRenderHint( QPainter::TextAntialiasing, true );

	// subtle round background when hovered or dragging
	if ( navGizmoHover >= 0 || navGizmoDrag ) {
		painter.setPen( Qt::NoPen );
		painter.setBrush( QColor( 255, 255, 255, 26 ) );
		painter.drawEllipse( center, R + 3.0f, R + 3.0f );
	}

	const QColor axisCol[3] = {
		QColor( 226,  87,  87 ),   // X red
		QColor( 140, 200,  75 ),   // Y green
		QColor(  70, 138, 235 )    // Z blue
	};
	const char * axisLabel[3] = { "X", "Y", "Z" };

	// draw far balls first so nearer ones overlap them
	int order[6] = { 0, 1, 2, 3, 4, 5 };
	std::sort( order, order + 6, [&]( int a, int b ) { return depth[a] < depth[b]; } );

	QFont font = painter.font();
	font.setPixelSize( int( ballR * 1.35f ) );
	font.setBold( true );
	painter.setFont( font );

	for ( int idx = 0; idx < 6; idx++ ) {
		int i = order[idx];
		int k = i / 2;
		bool positive = !( i & 1 );
		bool hover = ( i == navGizmoHover );
		// dim balls that point away from the camera
		float t = ( depth[i] + 1.0f ) * 0.5f;         // 0 (far) .. 1 (near)
		float alpha = 0.35f + 0.65f * t;
		QColor col = axisCol[k];

		// line from centre to the positive axis balls (Blender style)
		if ( positive ) {
			QColor lc = col; lc.setAlphaF( alpha );
			QPen pen( lc, 2.2f );
			pen.setCapStyle( Qt::RoundCap );
			painter.setPen( pen );
			painter.drawLine( center, pos[i] );
		}

		if ( positive || hover ) {
			QColor fill = col; fill.setAlphaF( positive ? alpha : ( 0.25f + 0.5f * t ) );
			painter.setBrush( fill );
			painter.setPen( hover ? QPen( QColor( 255, 255, 255, 230 ), 1.6f ) : Qt::NoPen );
			painter.drawEllipse( pos[i], ballR, ballR );
			// axis letter on positive balls
			if ( positive ) {
				painter.setPen( QColor( 20, 20, 20, int( 255 * alpha ) ) );
				painter.drawText( QRectF( pos[i].x() - ballR, pos[i].y() - ballR, ballR * 2, ballR * 2 ),
				                  Qt::AlignCenter, axisLabel[k] );
			}
		} else {
			// negative axis: hollow ring
			QColor ring = col; ring.setAlphaF( alpha );
			painter.setBrush( QColor( 0, 0, 0, int( 90 * alpha ) ) );
			painter.setPen( QPen( ring, 1.8f ) );
			painter.drawEllipse( pos[i], ballR, ballR );
		}
	}
}

void GLView::drawCursorOverlay( QPainter & painter )
{
	// Blender-style 3D cursor: red/white dashed circle with four crosshair
	// ticks, drawn at constant screen size at the projected cursor position
	QPointF sp;
	if ( !worldToScreen( cursorPos, sp ) )
		return;

	painter.setRenderHint( QPainter::Antialiasing, true );
	const qreal r = 6.5;

	// red base circle, then white dashes over it -> alternating red/white
	painter.setBrush( Qt::NoBrush );
	painter.setPen( QPen( QColor( 214, 56, 56 ), 1.6 ) );
	painter.drawEllipse( sp, r, r );
	QPen dashPen( QColor( 255, 255, 255 ), 1.6 );
	dashPen.setDashPattern( { 2.2, 2.2 } );
	painter.setPen( dashPen );
	painter.drawEllipse( sp, r, r );

	// dark crosshair ticks just outside the circle
	painter.setPen( QPen( QColor( 10, 10, 10, 235 ), 1.4 ) );
	const qreal t0 = r + 1.5, t1 = r + 6.0;
	painter.drawLine( sp + QPointF( t0, 0 ), sp + QPointF( t1, 0 ) );
	painter.drawLine( sp - QPointF( t0, 0 ), sp - QPointF( t1, 0 ) );
	painter.drawLine( sp + QPointF( 0, t0 ), sp + QPointF( 0, t1 ) );
	painter.drawLine( sp - QPointF( 0, t0 ), sp - QPointF( 0, t1 ) );
}

Transform GLView::viewTransform() const
{
	Transform vt;
	vt.rotation.fromEuler( deg2rad( Rot[0] ), deg2rad( Rot[1] ), deg2rad( Rot[2] ) );
	vt.translation = vt.rotation * Pos;
	if ( cfg.upAxis != ZAxis ) {
		float * r = &( vt.rotation( 0, 0 ) );
		if ( cfg.upAxis == XAxis ) {			// YZX -> XYZ
			FloatVector4::convertVector3( r ).shuffleValues( 0xD2 ).convertToVector3( r );
			FloatVector4::convertVector3( r + 3 ).shuffleValues( 0xD2 ).convertToVector3( r + 3 );
			FloatVector4::convertVector3( r + 6 ).shuffleValues( 0xD2 ).convertToVector3( r + 6 );
		} else if ( cfg.upAxis == YAxis ) {		// ZXY -> XYZ
			FloatVector4::convertVector3( r ).shuffleValues( 0xC9 ).convertToVector3( r );
			FloatVector4::convertVector3( r + 3 ).shuffleValues( 0xC9 ).convertToVector3( r + 3 );
			FloatVector4::convertVector3( r + 6 ).shuffleValues( 0xC9 ).convertToVector3( r + 6 );
		}
	}
	if ( view != ViewWalk )
		vt.translation[2] -= Dist * 2;
	return vt;
}

bool GLView::worldToScreen( const Vector3 & w, QPointF & out ) const
{
	float ww = (float)width(), hh = (float)height();
	if ( ww < 1.0f || hh < 1.0f )
		return false;

	Vector3 c = viewTransform() * w;

	if ( perspectiveMode || view == ViewWalk ) {
		if ( c[2] >= -1.0e-4f )
			return false;	// behind the camera
		float tanF = float( std::tan( ( cfg.fov / Zoom ) / 360.0 * M_PI ) );
		out = QPointF( ww * 0.5 * ( 1.0 + c[0] / ( -c[2] * tanF * aspect ) ),
		               hh * 0.5 * ( 1.0 - c[1] / ( -c[2] * tanF ) ) );
	} else {
		float h2 = float( Dist / Zoom );
		float w2 = h2 * float( aspect );
		out = QPointF( ww * 0.5 * ( 1.0 + c[0] / w2 ), hh * 0.5 * ( 1.0 - c[1] / h2 ) );
	}
	return true;
}

//! Distance from a point to a screen-space segment
static float tlPtSegDist( const QPointF & p, const QPointF & a, const QPointF & b )
{
	QPointF d = b - a;
	float len2 = float( d.x() * d.x() + d.y() * d.y() );
	float t = 0.0f;
	if ( len2 > 1.0e-6f ) {
		t = float( ( ( p.x() - a.x() ) * d.x() + ( p.y() - a.y() ) * d.y() ) / len2 );
		t = std::min( std::max( t, 0.0f ), 1.0f );
	}
	QPointF q = a + d * t;
	return float( std::hypot( p.x() - q.x(), p.y() - q.y() ) );
}

int GLView::gizmoHandleHitTest( const QPointF & pos ) const
{
	if ( !model || !scene || view == ViewWalk )
		return 0;

	int gb;
	if ( editMode && editShapeBlock >= 0 && !pickedElems.isEmpty() ) {
		gb = editShapeBlock;
	} else {
		if ( !scene->currentBlock.isValid() )
			return 0;
		gb = model->getBlockNumber( QModelIndex( scene->currentBlock ) );
		while ( gb >= 0 && !model->blockInherits( model->getBlockIndex( gb ), "NiAVObject" ) )
			gb = model->getParent( gb );
		if ( gb < 0 )
			return 0;
		if ( !model->getIndex( model->getBlockIndex( gb ), "Translation" ).isValid() )
			return 0;
	}
	QModelIndex iGb = model->getBlockIndex( gb );

	Matrix basis = gizmoBasis( iGb );
	Vector3 P = gizmoPivotPoint( iGb );
	float gs = gizmoScale( P );

	QPointF sp;
	if ( !worldToScreen( P, sp ) )
		return 0;

	// center: view-plane move
	if ( std::hypot( pos.x() - sp.x(), pos.y() - sp.y() ) < 10.0 )
		return 4;

	Vector3 ax[3];
	for ( int i = 0; i < 3; i++ ) {
		Vector3 u;
		u[i] = 1.0f;
		ax[i] = basis * u;
	}

	// scale boxes first: they sit on the arrow shafts
	for ( int i = 0; i < 3; i++ ) {
		QPointF bp;
		if ( worldToScreen( P + ax[i] * ( gs * 0.62f ), bp )
			&& std::hypot( pos.x() - bp.x(), pos.y() - bp.y() ) < 8.0 )
			return 8 + i;
	}

	// plane-move handles: quads between axis pairs (normal = axis i)
	for ( int i = 0; i < 3; i++ ) {
		int j = ( i + 1 ) % 3, k = ( i + 2 ) % 3;
		QPointF pp;
		if ( worldToScreen( P + ( ax[j] + ax[k] ) * ( gs * 0.4f ), pp )
			&& std::hypot( pos.x() - pp.x(), pos.y() - pp.y() ) < 8.0 )
			return 12 + i;
	}

	// move arrows
	for ( int i = 0; i < 3; i++ ) {
		QPointF a, b;
		if ( worldToScreen( P + ax[i] * ( gs * 0.2f ), a )
			&& worldToScreen( P + ax[i] * ( gs * 1.12f ), b )
			&& tlPtSegDist( pos, a, b ) < 7.0f )
			return 1 + i;
	}

	// rotation rings
	for ( int i = 0; i < 3; i++ ) {
		Vector3 u = Vector3::crossproduct( ax[i], ax[( i + 1 ) % 3] );
		u.normalize();
		Vector3 v = Vector3::crossproduct( ax[i], u );
		float r = gs * 0.85f;
		QPointF prev;
		bool prevOk = false;
		float best = 1.0e9f;
		for ( int s = 0; s <= 48; s++ ) {
			float ang = float( s ) * float( M_PI * 2.0 / 48.0 );
			QPointF q;
			if ( !worldToScreen( P + ( u * std::cos( ang ) + v * std::sin( ang ) ) * r, q ) ) {
				prevOk = false;
				continue;
			}
			if ( prevOk )
				best = std::min( best, tlPtSegDist( pos, prev, q ) );
			prev = q;
			prevOk = true;
		}
		if ( best < 6.0f )
			return 5 + i;
	}

	// white view-rotate ring: billboarded circle around the pivot
	{
		Matrix vm;
		vm.fromEuler( deg2rad( Rot[0] ), deg2rad( Rot[1] ), deg2rad( Rot[2] ) );
		Vector3 camRightW( vm( 0, 0 ), vm( 0, 1 ), vm( 0, 2 ) );
		QPointF rp;
		if ( worldToScreen( P + camRightW * ( gs * 1.02f ), rp ) ) {
			float rr = float( std::hypot( rp.x() - sp.x(), rp.y() - sp.y() ) );
			float dd = float( std::hypot( pos.x() - sp.x(), pos.y() - sp.y() ) );
			if ( std::fabs( dd - rr ) < 6.0f )
				return 11;
		}
	}

	return 0;
}

Matrix GLView::gizmoBasis( const QModelIndex & iBlock ) const
{
	return gizmoBasisFor( iBlock, gizmoOrient );
}

Matrix GLView::gizmoBasisFor( const QModelIndex & iBlock, int orient ) const
{
	if ( orient == 1 || orient == 2 ) {
		int b = model->getBlockNumber( iBlock );
		if ( orient == 2 )
			b = model->getParent( b );
		Node * n = ( b >= 0 ) ? scene->getNode( model, model->getBlockIndex( b ) ) : nullptr;
		if ( n )
			return n->worldTrans().rotation;
	} else if ( orient == 3 ) {
		Matrix vm;
		vm.fromEuler( deg2rad( Rot[0] ), deg2rad( Rot[1] ), deg2rad( Rot[2] ) );
		// camera rows become world basis columns (transpose)
		Matrix b;
		for ( int r = 0; r < 3; r++ ) {
			for ( int c = 0; c < 3; c++ )
				b( r, c ) = vm( c, r );
		}
		return b;
	}
	return Matrix();
}

float GLView::gizmoScale( const Vector3 & pivot ) const
{
	// world size for ~90 logical pixels on screen, regardless of camera
	// distance, zoom or projection - the same behaviour as the 2D cursor.
	// In perspective the apparent size depends on the pivot's view-space
	// depth, not the orbit distance (the pivot can sit well off-center).
	float hh = std::max( height(), 1 );
	float wpp;
	if ( perspectiveMode || view == ViewWalk ) {
		float depth = -( viewTransform() * pivot )[2];
		float tanF = float( std::tan( ( cfg.fov / Zoom ) / 360.0 * M_PI ) );
		wpp = 2.0f * std::max( depth, 0.01f ) * tanF / hh;
	} else {
		wpp = 2.0f * float( Dist / Zoom ) / hh;
	}
	return std::max( wpp, 1.0e-6f ) * 90.0f * ( gizmoSizeMul / 1.75f );
}

Vector3 GLView::gizmoPivotPoint( const QModelIndex & iBlock ) const
{
	// in edit mode the gizmo sits on the picked elements
	if ( editMode && !pickedElems.isEmpty() && gizmoPivot != 3 )
		return pickedMedian();
	if ( gizmoPivot == 2 && !pickedElems.isEmpty() )
		return pickedMedian();
	if ( gizmoPivot == 3 )
		return cursorPos;
	if ( gizmoPivot == 4 && objActive >= 0 ) {
		// active (last selected) object's origin: multi-selections orbit it
		Node * an = scene->getNode( model, model->getBlockIndex( objActive ) );
		if ( an )
			return an->worldTrans().translation;
	}

	Node * n = scene->getNode( model, iBlock );
	if ( !n )
		return Vector3();
	if ( gizmoPivot == 1 ) {
		BoundSphere bs = n->bounds();
		return bs.center;
	}
	return n->worldTrans().translation;
}

bool GLView::startModalTransform( int mode )
{
	if ( gizmoMode )
		return false;	// a gesture is running; keyPressEvent handles mode switch
	if ( editMode && !pickedElems.isEmpty() && gizmoBeginElement( mode ) )
		return true;
	return gizmoBegin( mode );
}

bool GLView::gizmoBegin( int mode )
{
	if ( !model )
		return false;

	// in object mode transform the ACTIVE selection (which a block-list pick
	// updates via objActive); scene->currentBlock is not always kept in sync
	// with a block-list selection, so relying on it dropped non-shape nodes
	QModelIndex iBlock;
	if ( !editMode && objActive >= 0 )
		iBlock = model->getBlockIndex( objActive );
	if ( !iBlock.isValid() ) {
		if ( !scene->currentBlock.isValid() )
			return false;
		iBlock = model->getBlockIndex( QModelIndex( scene->currentBlock ) );
	}
	// walk up to the nearest transformable object
	int b = model->getBlockNumber( iBlock );
	while ( b >= 0 && !model->blockInherits( model->getBlockIndex( b ), "NiAVObject" ) )
		b = model->getParent( b );
	if ( b < 0 )
		return false;

	iBlock = model->getBlockIndex( b );
	if ( !model->getIndex( iBlock, "Translation" ).isValid() )
		return false;

	gizmoBlock = iBlock;
	gizmoMode = mode;
	gizmoAxis = 0;
	gizmoPlane = 0;
	gizmoAxisLocal = false;
	gizmoTrackball = false;
	gizmoNum = QStringList() << QString() << QString() << QString();
	gizmoNumCur = 0;
	gizmoStartPos = mapFromGlobal( QCursor::pos() );
	gizmoOrigTrans = model->get<Vector3>( iBlock, "Translation" );
	gizmoOrigRot = model->get<Matrix>( iBlock, "Rotation" );
	gizmoOrigScale = model->get<float>( iBlock, "Scale" );

	// freeze the frame of reference for the whole gesture
	gizmoBasisM = gizmoBasis( iBlock );
	gizmoBasisOrig = gizmoBasisM;
	gizmoPivotWorld = gizmoPivotPoint( iBlock );
	int parentNum = model->getParent( b );
	Node * pn = ( parentNum >= 0 ) ? scene->getNode( model, model->getBlockIndex( parentNum ) ) : nullptr;
	if ( pn ) {
		Transform pt = pn->worldTrans();
		gizmoParentRot = pt.rotation;
		gizmoParentPos = pt.translation;
		gizmoParentScale = ( pt.scale != 0.0f ) ? pt.scale : 1.0f;
	} else {
		gizmoParentRot = Matrix();
		gizmoParentPos = Vector3();
		gizmoParentScale = 1.0f;
	}
	gizmoOrigWorldPos = gizmoParentPos + gizmoParentRot * gizmoOrigTrans * gizmoParentScale;

	// multi-selection: transform every selected node together (Blender). Nodes
	// whose ancestor is also selected are skipped so they are not moved twice.
	gizmoNodes.clear();
	auto addNode = [this]( int nb ) {
		QModelIndex ib = model->getBlockIndex( nb );
		if ( !model->getIndex( ib, "Translation" ).isValid() )
			return;
		for ( const auto & gn : gizmoNodes ) {
			if ( QModelIndex( gn.iBlock ) == ib )
				return;
		}
		GizmoNodeState st;
		st.iBlock = ib;
		st.origTrans = model->get<Vector3>( ib, "Translation" );
		st.origRot = model->get<Matrix>( ib, "Rotation" );
		st.origScale = model->get<float>( ib, "Scale" );
		int pn = model->getParent( nb );
		Node * p = ( pn >= 0 ) ? scene->getNode( model, model->getBlockIndex( pn ) ) : nullptr;
		if ( p ) {
			Transform pt = p->worldTrans();
			st.parentRot = pt.rotation;
			st.parentPos = pt.translation;
			st.parentScale = ( pt.scale != 0.0f ) ? pt.scale : 1.0f;
		}
		st.origWorldPos = st.parentPos + st.parentRot * st.origTrans * st.parentScale;
		gizmoNodes.append( st );
	};
	addNode( b );	// primary first
	{
		QSet<int> cand;
		cand.insert( b );
		for ( int sb : objSelection ) {
			int nb = sb;
			while ( nb >= 0 && !model->blockInherits( model->getBlockIndex( nb ), "NiAVObject" ) )
				nb = model->getParent( nb );
			if ( nb >= 0 )
				cand.insert( nb );
		}
		for ( int nb : cand ) {
			if ( nb == b )
				continue;
			bool ancestorSelected = false;
			for ( int pp = model->getParent( nb ); pp >= 0; pp = model->getParent( pp ) ) {
				if ( cand.contains( pp ) ) {
					ancestorSelected = true;
					break;
				}
			}
			if ( !ancestorSelected )
				addNode( nb );
		}
	}

	static const char * modeNames[4] = { "", "Move", "Rotate", "Scale" };
	emit gizmoStatus( tr( "%1 [%2]:  X/Y/Z axis (twice = local), Shift+X/Y/Z plane, MMB smart axis, R,R trackball, Ctrl snap (%3), Shift precise, LMB/Enter commit, Esc cancel" )
		.arg( QLatin1String( modeNames[mode] ), model->resolveString( iBlock, "Name" ) ).arg( gizmoSnapStep ) );

	// Blender: the modal gesture owns the mouse and keyboard for its whole
	// life, so the drag keeps working outside the viewport (over docks, other
	// windows) and X/Y/Z / typed values / Esc reach the modal even when the
	// block list has key focus
	gizmoWrapOffset = QPoint();
	setMouseGrabEnabled( true );
	setKeyboardGrabEnabled( true );

	return true;
}

//! Value of one typed part ("-" or "." alone parse as 0)
static float gizmoPartVal( const QStringList & parts, int i )
{
	if ( i < 0 || i >= parts.size() )
		return 0.0f;
	bool ok = false;
	float v = parts.at( i ).toFloat( &ok );
	return ok ? v : 0.0f;
}

bool GLView::gizmoNumActive() const
{
	for ( const QString & s : gizmoNum ) {
		if ( !s.isEmpty() )
			return true;
	}
	return false;
}

void GLView::gizmoUpdate( const QPoint & pos, Qt::KeyboardModifiers mods )
{
	if ( elemTransform ) {
		gizmoUpdateElement( pos, mods );
		return;
	}

	if ( !gizmoMode || !model || !gizmoBlock.isValid() )
		return;

	QModelIndex iBlock( gizmoBlock );
	float dx = pos.x() - gizmoStartPos.x();
	float dy = pos.y() - gizmoStartPos.y();
	const bool numeric = gizmoNumActive();
	// magnet toggle makes snapping the default; Ctrl inverts it (Blender)
	bool snap = ( ( ( mods & Qt::ControlModifier ) != 0 ) != snapDefaultOn ) && !numeric;
	// snapping only applies to the transform types enabled in the snap panel
	if ( snap && !( snapAffect & ( 1 << ( gizmoMode - 1 ) ) ) )
		snap = false;
	snapIndicator = false;	// re-set below when an element snap engages
	float precision = ( mods & Qt::ShiftModifier ) ? 0.2f : 1.0f;
	// Shift+Ctrl = fine snap increments (Blender)
	float snapFine = ( snap && ( mods & Qt::ShiftModifier ) ) ? 0.2f : 1.0f;

	// camera orientation in world space
	Matrix vm;
	vm.fromEuler( deg2rad( Rot[0] ), deg2rad( Rot[1] ), deg2rad( Rot[2] ) );
	Vector3 camRight( vm( 0, 0 ), vm( 0, 1 ), vm( 0, 2 ) );
	Vector3 camUp( vm( 1, 0 ), vm( 1, 1 ), vm( 1, 2 ) );
	Vector3 camFwd( vm( 2, 0 ), vm( 2, 1 ), vm( 2, 2 ) );

	QString status;

	// world-space position -> local translation relative to the (frozen) parent frame
	auto worldToLocalTrans = [this]( const Vector3 & w ) {
		return gizmoParentRot.inverted() * ( ( w - gizmoParentPos ) * ( 1.0f / gizmoParentScale ) );
	};

	if ( gizmoMode == 1 ) {
		float wpp = std::max( (float)Dist, 0.01f ) * 2.0f / std::max( height(), 1 ) * precision;
		Vector3 deltaWorld;

		if ( numeric ) {
			// typed offset: along the constraint axis, or X/Y/Z parts of the
			// active orientation (Tab cycles)
			if ( gizmoAxis == 0 ) {
				deltaWorld = gizmoBasisM * Vector3( gizmoPartVal( gizmoNum, 0 ),
					gizmoPartVal( gizmoNum, 1 ), gizmoPartVal( gizmoNum, 2 ) );
			} else {
				Vector3 unit;
				unit[gizmoAxis - 1] = 1.0f;
				deltaWorld = ( gizmoBasisM * unit ) * gizmoPartVal( gizmoNum, 0 );
			}
		} else if ( gizmoPlane > 0 ) {
			// plane constraint (Shift+X/Y/Z): view-plane delta with the excluded
			// axis component removed
			Vector3 ex;
			ex[gizmoPlane - 1] = 1.0f;
			Vector3 n = gizmoBasisM * ex;
			n.normalize();
			Vector3 d = camRight * ( dx * wpp ) + camUp * ( -dy * wpp );
			deltaWorld = d - n * Vector3::dotproduct( d, n );
		} else if ( gizmoAxis == 0 ) {
			deltaWorld = camRight * ( dx * wpp ) + camUp * ( -dy * wpp );
		} else {
			Vector3 unit;
			unit[gizmoAxis - 1] = 1.0f;
			Vector3 axis = gizmoBasisM * unit;
			float amount = ( dx * wpp ) * ( Vector3::dotproduct( camRight, axis ) >= 0 ? 1.0f : -1.0f )
			             + ( -dy * wpp ) * ( Vector3::dotproduct( camUp, axis ) >= 0 ? 1.0f : -1.0f );
			deltaWorld = axis * amount;
		}

		// element snapping: Ctrl + a vertex/edge/face snap target drops the node
		// onto the geometry under the mouse (the moved subtree is excluded)
		bool elemSnapped = false;
		if ( snap && snapTargetMode > 0 ) {
			SceneRayHit sh = raycastScene( QPointF( pos ), model->getBlockNumber( iBlock ) );
			if ( sh.shape ) {
				Transform swt = shapeRenderTrans( sh.shape );
				Vector3 target = swt * sh.hitLocal;
				const Triangle & tri = sh.shape->triangles.at( sh.tri );
				Vector3 va = sh.shape->verts.at( tri[0] );
				Vector3 vb = sh.shape->verts.at( tri[1] );
				Vector3 vc = sh.shape->verts.at( tri[2] );

				if ( snapTargetMode == 1 ) {
					float d0 = ( va - sh.hitLocal ).squaredLength();
					float d1 = ( vb - sh.hitLocal ).squaredLength();
					float d2 = ( vc - sh.hitLocal ).squaredLength();
					target = swt * ( d0 <= d1 && d0 <= d2 ? va : ( d1 <= d2 ? vb : vc ) );
				} else if ( snapTargetMode == 2 ) {
					auto closest = [&sh]( const Vector3 & a, const Vector3 & b ) {
						Vector3 d = b - a;
						float len2 = d.squaredLength();
						float t = ( len2 > 1.0e-12f )
						          ? std::min( std::max( Vector3::dotproduct( sh.hitLocal - a, d ) / len2, 0.0f ), 1.0f ) : 0.0f;
						return a + d * t;
					};
					Vector3 p01 = closest( va, vb ), p12 = closest( vb, vc ), p20 = closest( vc, va );
					float d01 = ( p01 - sh.hitLocal ).squaredLength();
					float d12 = ( p12 - sh.hitLocal ).squaredLength();
					float d20 = ( p20 - sh.hitLocal ).squaredLength();
					target = swt * ( d01 <= d12 && d01 <= d20 ? p01 : ( d12 <= d20 ? p12 : p20 ) );
				}

				// Snap Base: which part of the selection lands on the target
				Vector3 base = gizmoOrigWorldPos;	// active (primary node)
				if ( snapBase == 1 || snapBase == 2 ) {
					// center (bounds middle) / median of the moved node origins
					Vector3 mn = gizmoNodes.first().origWorldPos, mx = mn, sum;
					for ( const auto & st : gizmoNodes ) {
						sum += st.origWorldPos;
						for ( int c = 0; c < 3; c++ ) {
							mn[c] = std::min( mn[c], st.origWorldPos[c] );
							mx[c] = std::max( mx[c], st.origWorldPos[c] );
						}
					}
					base = ( snapBase == 1 ) ? ( mn + mx ) / 2.0f : sum / float( gizmoNodes.size() );
				} else if ( snapBase == 0 ) {
					float bd = 1.0e30f;
					for ( const auto & st : gizmoNodes ) {
						float d = ( st.origWorldPos - target ).squaredLength();
						if ( d < bd ) {
							bd = d;
							base = st.origWorldPos;
						}
					}
				}

				deltaWorld = target - base;
				// constrain the snap to the active axis / plane so an axis+snap
				// move only shifts along it (the snapped coordinate matches the
				// target, Blender), instead of dropping fully onto the target
				if ( gizmoAxis > 0 ) {
					Vector3 unit;
					unit[gizmoAxis - 1] = 1.0f;
					Vector3 saxis = gizmoBasisM * unit;
					saxis.normalize();
					deltaWorld = saxis * Vector3::dotproduct( deltaWorld, saxis );
				} else if ( gizmoPlane > 0 ) {
					Vector3 ex;
					ex[gizmoPlane - 1] = 1.0f;
					Vector3 snrm = gizmoBasisM * ex;
					snrm.normalize();
					deltaWorld = deltaWorld - snrm * Vector3::dotproduct( deltaWorld, snrm );
				}
				elemSnapped = true;
				snapIndicator = true;
				snapIndicatorPos = target;	// Blender: the marker sits on the snap target

				if ( snapAlignRot ) {
					// orient the node's +Z to the target face normal
					Vector3 n = Vector3::crossproduct( swt.rotation * ( vb - va ), swt.rotation * ( vc - va ) );
					n.normalize();
					Vector3 z( 0, 0, 1 );
					Vector3 axc = Vector3::crossproduct( z, n );
					float dz = std::min( std::max( Vector3::dotproduct( z, n ), -1.0f ), 1.0f );
					Matrix ar;
					if ( axc.length() > 1.0e-5f ) {
						axc.normalize();
						Quat qa;
						qa.fromAxisAngle( axc, std::acos( dz ) );
						ar.fromQuat( qa );
					} else if ( dz < 0.0f ) {
						Quat qa;
						qa.fromAxisAngle( Vector3( 1, 0, 0 ), float( M_PI ) );
						ar.fromQuat( qa );
					}
					model->set<Matrix>( iBlock, "Rotation", gizmoParentRot.inverted() * ar );
				}
			}
		}

		// grid stepping applies whenever snapping is on and no vertex/edge/face
		// snap engaged (an unhit element-snap move still grid-steps rather than
		// doing nothing). Snap the MOVEMENT in the active orientation basis so
		// an axis-constrained move only steps along that axis (the other delta
		// components are ~0 and round to 0, leaving those axes untouched).
		if ( snap && snapTargetMode == 0 && !elemSnapped && gizmoSnapStep > 0 ) {
			float step = gizmoSnapStep * snapFine;
			Vector3 deltaBasis = gizmoBasisM.inverted() * deltaWorld;
			for ( int c = 0; c < 3; c++ )
				deltaBasis[c] = std::round( deltaBasis[c] / step ) * step;
			deltaWorld = gizmoBasisM * deltaBasis;
		}

		Vector3 nt = worldToLocalTrans( gizmoOrigWorldPos + deltaWorld );

		// effective world delta after snapping, derived from the primary node,
		// applied identically to every node of the multi-selection
		Vector3 effDelta = ( gizmoParentPos + gizmoParentRot * ( nt * gizmoParentScale ) ) - gizmoOrigWorldPos;
		gizmoLastParam = gizmoBasisM.inverted() * effDelta;
		for ( const auto & st : gizmoNodes ) {
			Vector3 w = st.origWorldPos + effDelta;
			model->set<Vector3>( QModelIndex( st.iBlock ), "Translation",
				st.parentRot.inverted() * ( ( w - st.parentPos ) * ( 1.0f / st.parentScale ) ) );
		}
		status = tr( "Move: %1, %2, %3" ).arg( nt[0], 0, 'f', 3 ).arg( nt[1], 0, 'f', 3 ).arg( nt[2], 0, 'f', 3 );
		if ( gizmoNodes.size() > 1 )
			status += tr( "  (%1 objects)" ).arg( gizmoNodes.size() );
	} else if ( gizmoMode == 2 ) {
		float rotStep = gizmoRotSnapDeg * snapFine;
		Matrix dr;

		if ( gizmoTrackball && !numeric ) {
			// R,R: trackball rotation around the camera right/up axes
			float ax = dy * 0.5f * precision;
			float ay = dx * 0.5f * precision;
			if ( snap && rotStep > 0.0f ) {
				ax = std::round( ax / rotStep ) * rotStep;
				ay = std::round( ay / rotStep ) * rotStep;
			}
			Quat q1, q2;
			q1.fromAxisAngle( camRight, deg2rad( ax ) );
			q2.fromAxisAngle( camUp, deg2rad( ay ) );
			Matrix m1, m2;
			m1.fromQuat( q1 );
			m2.fromQuat( q2 );
			dr = m2 * m1;
			gizmoLastParam = Vector3( ay, ax, 0 );
			gizmoLastRotAxis = camFwd;
			status = tr( "Trackball: %1°, %2°" ).arg( ay, 0, 'f', 1 ).arg( ax, 0, 'f', 1 );
		} else {
			float angle = numeric ? gizmoPartVal( gizmoNum, 0 ) : dx * 0.5f * precision;
			if ( snap && rotStep > 0.0f )
				angle = std::round( angle / rotStep ) * rotStep;

			Vector3 axis;
			if ( gizmoAxis == 0 ) {
				axis = camFwd;
			} else {
				Vector3 unit;
				unit[gizmoAxis - 1] = 1.0f;
				axis = gizmoBasisM * unit;
			}

			Quat q;
			q.fromAxisAngle( axis, deg2rad( angle ) );
			dr.fromQuat( q );

			gizmoLastParam = Vector3( angle, 0, 0 );
			gizmoLastRotAxis = axis;
			status = tr( "Rotate: %1°" ).arg( angle, 0, 'f', 1 );
		}

		// world delta rotation expressed in each node's parent frame, so it is
		// correct under rotated parents too; with a non-origin pivot the nodes
		// also orbit the shared pivot point
		for ( const auto & st : gizmoNodes ) {
			QModelIndex ib( st.iBlock );
			model->set<Matrix>( ib, "Rotation", st.parentRot.inverted() * dr * st.parentRot * st.origRot );
			if ( gizmoPivot != 0 ) {
				Vector3 newWorld = gizmoPivotWorld + dr * ( st.origWorldPos - gizmoPivotWorld );
				model->set<Vector3>( ib, "Translation",
					st.parentRot.inverted() * ( ( newWorld - st.parentPos ) * ( 1.0f / st.parentScale ) ) );
			}
		}
	} else if ( gizmoMode == 3 ) {
		float factor = numeric ? gizmoPartVal( gizmoNum, 0 ) : 1.0f + dx * 0.01f * precision;
		factor = std::max( factor, 0.001f );
		if ( snap ) {
			// Shift+Ctrl = fine snap (0.01 steps instead of 0.1)
			float ss = ( snapFine < 1.0f ) ? 100.0f : 10.0f;
			factor = std::max( std::round( factor * ss ) / ss, 0.01f );
		}

		gizmoLastParam = Vector3( factor, 0, 0 );
		for ( const auto & st : gizmoNodes ) {
			QModelIndex ib( st.iBlock );
			model->set<float>( ib, "Scale", st.origScale * factor );
			if ( gizmoPivot != 0 ) {
				Vector3 newWorld = gizmoPivotWorld + ( st.origWorldPos - gizmoPivotWorld ) * factor;
				model->set<Vector3>( ib, "Translation",
					st.parentRot.inverted() * ( ( newWorld - st.parentPos ) * ( 1.0f / st.parentScale ) ) );
			}
		}
		status = tr( "Scale: ×%1 (uniform; NIF scale is a single value)" ).arg( factor, 0, 'f', 2 );
	}

	static const char * axisNames[4] = { "view", "X", "Y", "Z" };
	if ( gizmoPlane > 0 )
		status += tr( "   [plane: exclude %1]" ).arg( QLatin1String( axisNames[gizmoPlane] ) );
	else
		status += tr( "   [axis: %1%2]" ).arg( QLatin1String( axisNames[gizmoAxis] ),
			gizmoAxisLocal ? tr( " (local)" ) : QString() );

	// show the snap state so it is obvious whether snapping is engaging
	static const char * snapNames[4] = { "grid", "vertex", "edge", "face" };
	if ( snap )
		status += tr( "   [snap: %1]" ).arg( QLatin1String( snapNames[std::min( std::max( snapTargetMode, 0 ), 3 )] ) );
	else
		status += tr( "   [snap: off - hold Ctrl or enable the magnet]" );

	if ( numeric ) {
		if ( gizmoMode == 1 && gizmoAxis == 0 ) {
			QStringList shown;
			for ( int i = 0; i < 3; i++ ) {
				QString s = gizmoNum.value( i );
				if ( s.isEmpty() )
					s = QStringLiteral( "0" );
				if ( i == gizmoNumCur )
					s = QStringLiteral( "[" ) + s + QStringLiteral( "]" );
				shown << s;
			}
			status += tr( "   typed: %1  (Tab = next component)" ).arg( shown.join( QLatin1String( ", " ) ) );
		} else {
			status += tr( "   typed: %1" ).arg( gizmoNum.value( 0 ) );
		}
	}

	emit gizmoStatus( status );

	update();
}

void GLView::gizmoEnd( bool commit )
{
	gizmoHandleDrag = false;

	if ( elemTransform ) {
		gizmoEndElement( commit );
		return;
	}

	if ( !gizmoMode )
		return;

	QModelIndex iBlock( gizmoBlock );
	int mode = gizmoMode;
	gizmoMode = 0;
	setMouseGrabEnabled( false );	// the gesture owned the mouse + keyboard
	setKeyboardGrabEnabled( false );
	emit gizmoStatus( QString() );

	if ( !model || !iBlock.isValid() ) {
		return;
	}

	// capture the dragged values, restore originals, then re-apply through the
	// undo stack - for every node of the multi-selection
	if ( gizmoNodes.isEmpty() ) {
		GizmoNodeState st;
		st.iBlock = iBlock;
		st.origTrans = gizmoOrigTrans;
		st.origRot = gizmoOrigRot;
		st.origScale = gizmoOrigScale;
		gizmoNodes.append( st );
	}

	QVector<Vector3> newTransV( gizmoNodes.size() );
	QVector<Matrix> newRotV( gizmoNodes.size() );
	QVector<float> newScaleV( gizmoNodes.size() );
	for ( int k = 0; k < gizmoNodes.size(); k++ ) {
		QModelIndex ib( gizmoNodes.at( k ).iBlock );
		newTransV[k] = model->get<Vector3>( ib, "Translation" );
		newRotV[k] = model->get<Matrix>( ib, "Rotation" );
		newScaleV[k] = model->get<float>( ib, "Scale" );
		model->set<Vector3>( ib, "Translation", gizmoNodes.at( k ).origTrans );
		model->set<Matrix>( ib, "Rotation", gizmoNodes.at( k ).origRot );
		model->set<float>( ib, "Scale", gizmoNodes.at( k ).origScale );
	}
	Vector3 newTrans = newTransV.value( 0, gizmoOrigTrans );
	Matrix newRot = newRotV.value( 0, gizmoOrigRot );
	float newScale = newScaleV.value( 0, gizmoOrigScale );

	if ( commit ) {
		ChangeValueCommand::createTransaction();
		auto pushTyped = [this]( const QModelIndex & ib, const char * fieldName, auto newVal ) {
			QModelIndex iField = model->getIndex( ib, fieldName );
			if ( !iField.isValid() )
				return;
			QModelIndex vIdx = iField.sibling( iField.row(), NifModel::ValueCol );
			const NifItem * item = static_cast<const NifItem *>( vIdx.internalPointer() );
			if ( !item )
				return;
			NifValue oldVal = item->value();
			NifValue nv = oldVal;
			if ( nv.set( newVal, model, item ) )
				model->undoStack->push( new ChangeValueCommand( vIdx, oldVal, nv, model->itemName( iField ), model ) );
		};

		// pivot-relative rotate/scale moves Translation too, so commit whatever changed
		for ( int k = 0; k < gizmoNodes.size(); k++ ) {
			QModelIndex ib( gizmoNodes.at( k ).iBlock );
			if ( !ib.isValid() )
				continue;
			if ( !( newTransV.at( k ) == gizmoNodes.at( k ).origTrans ) )
				pushTyped( ib, "Translation", newTransV.at( k ) );
			if ( !( newRotV.at( k ) == gizmoNodes.at( k ).origRot ) )
				pushTyped( ib, "Rotation", newRotV.at( k ) );
			if ( newScaleV.at( k ) != gizmoNodes.at( k ).origScale )
				pushTyped( ib, "Scale", newScaleV.at( k ) );
		}

		if ( gizmoAutoKey )
			emit transformCommitted( model->getBlockNumber( iBlock ) );

		// freeze the gesture frame for the redo panel
		lastGestureElement = false;
		lastGizmoMode = mode;
		lastGizmoAxis = gizmoAxis;
		lastGizmoOrient = gizmoOrient;
		lastGizmoBlock = iBlock;
		lastBasis = gizmoBasisM;
		lastPivot = gizmoPivotWorld;
		lastParentRot = gizmoParentRot;
		lastParentPos = gizmoParentPos;
		lastParentScale = gizmoParentScale;
		lastOrigWorldPos = gizmoOrigWorldPos;
		lastOrigTrans = gizmoOrigTrans;
		lastOrigRot = gizmoOrigRot;
		lastOrigScale = gizmoOrigScale;
		lastUndoIndex = model->undoStack ? model->undoStack->index() : -1;
		emit transformGesture( mode, gizmoAxis, gizmoLastParam );
	}

	update();
}

static QModelIndex tlVertexValueIndex( NifModel * model, const QModelIndex & iShape, int vi );

bool GLView::gizmoReapplyElement( const Vector3 & param, int axisOverride )
{
	if ( !model || !model->undoStack || !lastElemMode || lastElemVerts.isEmpty() )
		return false;
	if ( model->undoStack->index() != lastUndoIndex )
		return false;	// stale: something else touched the undo stack
	int axis = ( axisOverride >= 0 ) ? axisOverride : lastElemAxis;

	// undo the committed edit, then re-apply the same gesture with new params
	model->undoStack->undo();

	QHash<int, Transform> xf;
	auto toLocal = [&]( int shape, const Vector3 & w ) {
		if ( !xf.contains( shape ) ) {
			Node * n = scene->getNode( model, model->getBlockIndex( shape ) );
			xf.insert( shape, n ? shapeRenderTrans( n ) : Transform() );
		}
		Transform wt = xf.value( shape );
		float sc = ( wt.scale != 0.0f ) ? wt.scale : 1.0f;
		return wt.rotation.inverted() * ( ( w - wt.translation ) * ( 1.0f / sc ) );
	};

	Matrix dr;
	if ( lastElemMode == 2 ) {
		Vector3 rax;
		if ( axis == 0 ) {
			Matrix vm;
			vm.fromEuler( deg2rad( Rot[0] ), deg2rad( Rot[1] ), deg2rad( Rot[2] ) );
			rax = Vector3( vm( 2, 0 ), vm( 2, 1 ), vm( 2, 2 ) );
		} else {
			rax[axis - 1] = 1.0f;
		}
		Quat q;
		q.fromAxisAngle( rax, deg2rad( param[0] ) );
		dr.fromQuat( q );
	}

	ChangeValueCommand::createTransaction();
	for ( const ElemVert & ev : lastElemVerts ) {
		Vector3 w;
		if ( lastElemMode == 1 ) {
			Vector3 d = param;
			if ( axis > 0 ) { float a = d[axis - 1]; d = Vector3(); d[axis - 1] = a; }
			w = ev.origWorld + d;
		} else if ( lastElemMode == 2 ) {
			w = lastElemPivot + dr * ( ev.origWorld - lastElemPivot );
		} else {
			Vector3 rel = ev.origWorld - lastElemPivot;
			if ( axis > 0 ) rel[axis - 1] *= param[0]; else rel = rel * param[0];
			w = lastElemPivot + rel;
		}
		QModelIndex iShape = model->getBlockIndex( ev.shape );
		QModelIndex vIdx = tlVertexValueIndex( model, iShape, ev.idx );
		const NifItem * item = vIdx.isValid() ? static_cast<const NifItem *>( vIdx.internalPointer() ) : nullptr;
		if ( !item )
			continue;
		Vector3 cageLocal = toLocal( ev.shape, w );
		Vector3 local;
		Shape * shape = shapeForBlock( ev.shape );
		if ( !shape || !editVertexRawLocal( shape, ev.idx, cageLocal, local ) )
			continue;
		NifValue oldVal = item->value();
		NifValue newVal = oldVal;
		if ( item->hasValueType( NifValue::tHalfVector3 ) )
			newVal.set<HalfVector3>( HalfVector3( local ), model, item );
		else
			newVal.set<Vector3>( local, model, item );
		if ( !( oldVal == newVal ) )
			model->undoStack->push( new ChangeValueCommand( vIdx, oldVal, newVal, tr( "Vertex" ), model ) );
	}
	lastElemAxis = axis;
	lastUndoIndex = model->undoStack->index();
	modelChanged();
	update();
	return true;
}

bool GLView::gizmoReapply( const Vector3 & param, int axisOverride, int orientOverride )
{
	if ( lastGestureElement )
		return gizmoReapplyElement( param, axisOverride );
	if ( !model || !model->undoStack || !lastGizmoMode || !lastGizmoBlock.isValid() )
		return false;
	// the gesture is stale once anything else touches the undo stack
	if ( model->undoStack->index() != lastUndoIndex )
		return false;

	QModelIndex iBlock( lastGizmoBlock );

	// the operator panel can re-express the gesture on another axis or in
	// another orientation; recompute the frozen basis / rotation axis then
	bool reframed = false;
	if ( orientOverride >= 0 && orientOverride != lastGizmoOrient ) {
		lastBasis = gizmoBasisFor( iBlock, orientOverride );
		lastGizmoOrient = orientOverride;
		reframed = true;
	}
	if ( axisOverride >= 0 && axisOverride != lastGizmoAxis ) {
		lastGizmoAxis = axisOverride;
		reframed = true;
	}
	if ( reframed && lastGizmoMode == 2 ) {
		if ( lastGizmoAxis == 0 ) {
			// view axis: the camera forward direction
			Matrix vm;
			vm.fromEuler( deg2rad( Rot[0] ), deg2rad( Rot[1] ), deg2rad( Rot[2] ) );
			gizmoLastRotAxis = Vector3( vm( 2, 0 ), vm( 2, 1 ), vm( 2, 2 ) );
		} else {
			Vector3 unit;
			unit[lastGizmoAxis - 1] = 1.0f;
			gizmoLastRotAxis = lastBasis * unit;
		}
	}

	// revert the previous commit (one merged transaction), then re-apply
	model->undoStack->undo();

	auto worldToLocal = [this]( const Vector3 & w ) {
		return lastParentRot.inverted() * ( ( w - lastParentPos ) * ( 1.0f / lastParentScale ) );
	};

	Vector3 newTrans = lastOrigTrans;
	Matrix newRot = lastOrigRot;
	float newScale = lastOrigScale;

	if ( lastGizmoMode == 1 ) {
		newTrans = worldToLocal( lastOrigWorldPos + lastBasis * param );
		gizmoLastParam = param;
	} else if ( lastGizmoMode == 2 ) {
		Quat q;
		q.fromAxisAngle( gizmoLastRotAxis, deg2rad( param[0] ) );
		Matrix dr;
		dr.fromQuat( q );
		newRot = lastParentRot.inverted() * dr * lastParentRot * lastOrigRot;
		Vector3 newWorld = lastPivot + dr * ( lastOrigWorldPos - lastPivot );
		newTrans = worldToLocal( newWorld );
		gizmoLastParam = Vector3( param[0], 0, 0 );
	} else if ( lastGizmoMode == 3 ) {
		float factor = std::max( param[0], 0.001f );
		newScale = lastOrigScale * factor;
		Vector3 newWorld = lastPivot + ( lastOrigWorldPos - lastPivot ) * factor;
		newTrans = worldToLocal( newWorld );
		gizmoLastParam = Vector3( factor, 0, 0 );
	}

	ChangeValueCommand::createTransaction();
	auto pushTyped = [this, &iBlock]( const char * fieldName, auto newVal ) {
		QModelIndex iField = model->getIndex( iBlock, fieldName );
		if ( !iField.isValid() )
			return;
		QModelIndex vIdx = iField.sibling( iField.row(), NifModel::ValueCol );
		const NifItem * item = static_cast<const NifItem *>( vIdx.internalPointer() );
		if ( !item )
			return;
		NifValue oldVal = item->value();
		NifValue nv = oldVal;
		if ( nv.set( newVal, model, item ) )
			model->undoStack->push( new ChangeValueCommand( vIdx, oldVal, nv, model->itemName( iField ), model ) );
	};

	if ( !( newTrans == lastOrigTrans ) )
		pushTyped( "Translation", newTrans );
	if ( !( newRot == lastOrigRot ) )
		pushTyped( "Rotation", newRot );
	if ( newScale != lastOrigScale )
		pushTyped( "Scale", newScale );

	lastUndoIndex = model->undoStack->index();

	if ( gizmoAutoKey )
		emit transformCommitted( model->getBlockNumber( iBlock ) );

	update();
	return true;
}

/*
 *  Element reference picking, 3D cursor, snapping
 */

//! Möller-Trumbore ray/triangle intersection
static bool tlRayTri( const Vector3 & ro, const Vector3 & rd,
	const Vector3 & a, const Vector3 & b, const Vector3 & c, float & tOut )
{
	Vector3 e1 = b - a;
	Vector3 e2 = c - a;
	Vector3 p = Vector3::crossproduct( rd, e2 );
	float det = Vector3::dotproduct( e1, p );
	if ( std::fabs( det ) < 1.0e-9f )
		return false;
	float inv = 1.0f / det;
	Vector3 s = ro - a;
	float u = Vector3::dotproduct( s, p ) * inv;
	if ( u < -1.0e-4f || u > 1.0001f )
		return false;
	Vector3 q = Vector3::crossproduct( s, e1 );
	float v = Vector3::dotproduct( rd, q ) * inv;
	if ( v < -1.0e-4f || u + v > 1.0001f )
		return false;
	float t = Vector3::dotproduct( e2, q ) * inv;
	if ( t <= 1.0e-5f )
		return false;
	tOut = t;
	return true;
}

void GLView::mouseRayWorld( const QPointF & pos, Vector3 & origin, Vector3 & dir ) const
{
	Transform vt = viewTransform();
	Matrix ri = vt.rotation.inverted();
	float ww = std::max( (float)width(), 1.0f ), hh = std::max( (float)height(), 1.0f );

	if ( perspectiveMode || view == ViewWalk ) {
		float tanF = float( std::tan( ( cfg.fov / Zoom ) / 360.0 * M_PI ) );
		Vector3 dc( ( 2.0f * float( pos.x() ) / ww - 1.0f ) * tanF * float( aspect ),
		            ( 1.0f - 2.0f * float( pos.y() ) / hh ) * tanF, -1.0f );
		origin = ri * ( Vector3() - vt.translation );
		dir = ri * dc;
	} else {
		float h2 = float( Dist / Zoom );
		float w2 = h2 * float( aspect );
		Vector3 pc( ( 2.0f * float( pos.x() ) / ww - 1.0f ) * w2,
		            ( 1.0f - 2.0f * float( pos.y() ) / hh ) * h2, 0.0f );
		origin = ri * ( pc - vt.translation );
		dir = ri * Vector3( 0, 0, -1 );
	}
	dir.normalize();
}

Transform GLView::shapeRenderTrans( Node * n ) const
{
	// viewTrans() = scene->view * (billboard-adjusted world); undo the camera
	// to recover the world transform actually used to draw the mesh
	return scene->view.inverted() * n->viewTrans();
}

Vector3 GLView::editVertexLocal( Shape * shape, int vertexIndex ) const
{
	if ( !shape || vertexIndex < 0 || vertexIndex >= shape->verts.size() )
		return Vector3();
	const Vector3 raw = shape->verts.at( vertexIndex );
	return editDeformedCageActive() ? shape->skinVertex( vertexIndex, raw ) : raw;
}

bool GLView::evaluatedVertexWorld( int shapeBlock, int vertexIndex, Vector3 & world ) const
{
	Shape * shape = shapeForBlock( shapeBlock );
	if ( !shape || vertexIndex < 0 || vertexIndex >= shape->verts.size() )
		return false;
	world = shapeRenderTrans( shape ) * shape->skinVertex( vertexIndex, shape->verts.at( vertexIndex ) );
	return true;
}

bool GLView::editVertexRawLocal( Shape * shape, int vertexIndex,
	const Vector3 & cageLocal, Vector3 & rawLocal ) const
{
	if ( !shape || vertexIndex < 0 || vertexIndex >= shape->verts.size() )
		return false;
	if ( !editDeformedCageActive() ) {
		rawLocal = cageLocal;
		return true;
	}
	return shape->unskinVertex( vertexIndex, cageLocal, rawLocal );
}

void GLView::refreshPickedElementPositions()
{
	// drop picks whose elements no longer exist (an in-place topology undo
	// shrinks the arrays under a live selection)
	for ( int i = pickedElems.size() - 1; i >= 0; i-- ) {
		const PickedElement & pe = pickedElems.at( i );
		Shape * shape = shapeForBlock( pe.shapeBlock );
		if ( !shape )
			continue;	// scene not rebuilt yet: judged on the next refresh
		const int nv = shape->verts.size();
		bool ok = true;
		if ( pe.type == 1 )
			ok = ( pe.e0 >= 0 && pe.e0 < nv );
		else if ( pe.type == 2 )
			ok = ( pe.e0 >= 0 && pe.e0 < nv && pe.e1 >= 0 && pe.e1 < nv );
		else if ( pe.type == 3 )
			ok = ( pe.e0 >= 0 && pe.e0 < shape->triangles.size() );
		if ( !ok )
			pickedElems.removeAt( i );
	}
	for ( PickedElement & pe : pickedElems ) {
		Shape * shape = shapeForBlock( pe.shapeBlock );
		if ( !shape )
			continue;
		Transform wt = shapeRenderTrans( shape );
		const int nv = shape->verts.size();
		if ( pe.type == 1 && pe.e0 >= 0 && pe.e0 < nv ) {
			pe.worldPos = wt * editVertexLocal( shape, pe.e0 );
			pe.wA = pe.wB = pe.wC = pe.worldPos;
		} else if ( pe.type == 2 && pe.e0 >= 0 && pe.e0 < nv && pe.e1 >= 0 && pe.e1 < nv ) {
			pe.wA = wt * editVertexLocal( shape, pe.e0 );
			pe.wB = wt * editVertexLocal( shape, pe.e1 );
			pe.wC = pe.wA;
			pe.worldPos = ( pe.wA + pe.wB ) * 0.5f;
		} else if ( pe.type == 3 && pe.e0 >= 0 && pe.e0 < shape->triangles.size() ) {
			const Triangle & tri = shape->triangles.at( pe.e0 );
			if ( tri[0] >= nv || tri[1] >= nv || tri[2] >= nv )
				continue;
			pe.wA = wt * editVertexLocal( shape, tri[0] );
			pe.wB = wt * editVertexLocal( shape, tri[1] );
			pe.wC = wt * editVertexLocal( shape, tri[2] );
			pe.worldPos = ( pe.wA + pe.wB + pe.wC ) * ( 1.0f / 3.0f );
		}
	}
}

void GLView::setEditDeformedCage( bool enabled )
{
	if ( editDeformedCage == enabled )
		return;
	if ( elemTransform )
		gizmoEndElement( false );
	editDeformedCage = enabled;
	QSettings().setValue( "GLView/Edit/DeformedCage", enabled );
	if ( editMode && scene ) {
		const bool evaluated = riggingWeightPaintMode || vertexPaintMode || segmentPaintMode || editDeformedCage;
		scene->options.setFlag( Scene::DoSkinning, evaluated );
		for ( int block : editShapeBlocks ) {
			if ( Shape * shape = shapeForBlock( block ) )
				shape->updateBoneTransforms();
		}
		refreshPickedElementPositions();
		doCompile = 1;
	}
	emit editDeformedCageChanged( enabled );
	if ( editMode && !riggingWeightPaintMode && !vertexPaintMode && !segmentPaintMode )
		emit gizmoStatus( enabled
			? tr( "Edit Mode - Deformed Cage (evaluated game/skinned position)" )
			: tr( "Edit Mode - Raw Bind Position" ) );
	update();
}

GLView::SceneRayHit GLView::raycastScene( const QPointF & pos, int excludeBlock, const QSet<int> * onlyShapes ) const
{
	SceneRayHit hit;
	if ( !model || !scene )
		return hit;

	Vector3 ro, rd;
	mouseRayWorld( pos, ro, rd );

	for ( Shape * s : scene->shapes ) {
		if ( !s || s->isHidden() || s->verts.isEmpty() || s->triangles.isEmpty() )
			continue;

		if ( onlyShapes && !onlyShapes->contains( s->id() ) )
			continue;

		if ( excludeBlock >= 0 ) {
			// skip the transformed node and its subtree
			int b = s->id();
			while ( b >= 0 && b != excludeBlock )
				b = model->getParent( b );
			if ( b == excludeBlock )
				continue;
		}

		Transform wt = shapeRenderTrans( s );
		float sc = ( wt.scale != 0.0f ) ? wt.scale : 1.0f;
		Matrix ri = wt.rotation.inverted();
		Vector3 lo = ri * ( ( ro - wt.translation ) * ( 1.0f / sc ) );
		Vector3 ld = ri * rd;
		ld.normalize();

		// hidden edit-mode triangles are not pickable (Blender H)
		const QSet<int> * hidT = nullptr;
		if ( editMode ) {
			auto ith = editHiddenTris.constFind( s->id() );
			if ( ith != editHiddenTris.constEnd() && !ith->isEmpty() )
				hidT = &( *ith );
		}

		for ( int i = 0; i < s->triangles.size(); i++ ) {
			if ( hidT && hidT->contains( i ) )
				continue;
			const Triangle & tri = s->triangles.at( i );
			if ( tri[0] >= s->verts.size() || tri[1] >= s->verts.size() || tri[2] >= s->verts.size() )
				continue;
			float t = 0;
			if ( tlRayTri( lo, ld, editVertexLocal( s, tri[0] ), editVertexLocal( s, tri[1] ),
				 editVertexLocal( s, tri[2] ), t ) ) {
				float worldT = t * sc;
				if ( worldT < hit.dist ) {
					hit.dist = worldT;
					hit.shape = s;
					hit.tri = i;
					hit.hitLocal = lo + ld * t;
				}
			}
		}
	}
	return hit;
}

void GLView::drawObjectOutlines()
{
	float dpr = float( devicePixelRatioF() );

	glEnable( GL_STENCIL_TEST );
	glStencilMask( 0xFF );
	glDisable( GL_DEPTH_TEST );	// the outline shows around the whole object
	glDepthMask( GL_FALSE );

	for ( int b : objSelection ) {
		// All shapes in the selected object's subtree form one silhouette. Use
		// each Shape's own cached vertex/index buffer: its outline shaders apply
		// the same GPU bone transforms as the normal skinned mesh draw.
		QVector<Shape *> shs;
		for ( Shape * s : scene->shapes ) {
			if ( !s || s->isHidden() || s->verts.isEmpty() || s->triangles.isEmpty() )
				continue;
			int p = s->id();
			while ( p >= 0 && p != b )
				p = model->getParent( p );
			if ( p != b )
				continue;
			shs.append( s );
		}
		if ( shs.isEmpty() )
			continue;

		// white while a transform gesture is running on this object (Blender)
		bool transforming = false;
		if ( gizmoMode ) {
			for ( const auto & st : gizmoNodes ) {
				if ( model->getBlockNumber( QModelIndex( st.iBlock ) ) == b ) {
					transforming = true;
					break;
				}
			}
		}

		// pass 1: mark the object's screen area in the stencil buffer
		glClear( GL_STENCIL_BUFFER_BIT );
		glStencilFunc( GL_ALWAYS, 1, 0xFF );
		glStencilOp( GL_KEEP, GL_KEEP, GL_REPLACE );
		glColorMask( GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE );
		glPolygonMode( GL_FRONT_AND_BACK, GL_FILL );
		for ( Shape * shape : shs ) {
			if ( shape->bindShape() )
				shape->drawTriangles( 0, shape->triangles.size(), FloatVector4( 0.0f, 0.0f, 0.0f, 1.0f ) );
		}
		glColorMask( GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE );

		// pass 2: thick wireframe clipped to OUTSIDE the stencil = silhouette
		glStencilFunc( GL_EQUAL, 0, 0xFF );
		glStencilOp( GL_KEEP, GL_KEEP, GL_KEEP );
		FloatVector4 outlineColor = transforming
			? FloatVector4( 1.0f, 1.0f, 1.0f, 1.0f )
			: b == objActive ? FloatVector4( 1.0f, 0.616f, 0.0f, 1.0f )	// #FF9D00
			: FloatVector4( 1.0f, 0.447f, 0.0f, 1.0f );				// #FF7200
		for ( Shape * shape : shs ) {
			if ( shape->bindShape() )
				shape->drawWireframe( outlineColor, 3.2f * dpr );
		}
	}

	glDisable( GL_STENCIL_TEST );
	glEnable( GL_DEPTH_TEST );
	glDepthMask( GL_TRUE );
}

void GLView::hideSelectedElements()
{
	if ( !editMode || pickedElems.isEmpty() )
		return;

	for ( const auto & pe : pickedElems ) {
		Shape * s = shapeForBlock( pe.shapeBlock );
		if ( !s )
			continue;
		QSet<int> & hid = editHiddenTris[pe.shapeBlock];
		for ( int ti = 0; ti < s->triangles.size(); ti++ ) {
			const Triangle & t = s->triangles.at( ti );
			bool h = false;
			if ( pe.type == 1 ) {
				h = ( t[0] == pe.e0 || t[1] == pe.e0 || t[2] == pe.e0 );
			} else if ( pe.type == 2 ) {
				bool a = ( t[0] == pe.e0 || t[1] == pe.e0 || t[2] == pe.e0 );
				bool b = ( t[0] == pe.e1 || t[1] == pe.e1 || t[2] == pe.e1 );
				h = a && b;
			} else {
				h = ( ti == pe.e0 );
			}
			if ( h )
				hid.insert( ti );
		}
	}

	pickedElems.clear();	// hidden geometry is deselected, like Blender
	scene->hiddenTris = editHiddenTris;
	emit gizmoStatus( tr( "Hidden selected elements  (Alt+H to unhide)" ) );
	update();
}

void GLView::unhideAllElements()
{
	editHiddenTris.clear();
	scene->hiddenTris.clear();
	update();
}

void GLView::isolateSelected()
{
	if ( !model || !scene )
		return;

	if ( editMode ) {
		if ( pickedElems.isEmpty() ) {
			emit gizmoStatus( tr( "Select geometry before isolating it" ) );
			return;
		}
		const QHash<int, QSet<int>> selectedVertices = pickedVertexRefs();
		for ( int block : editShapeBlocks ) {
			Shape * shape = shapeForBlock( block );
			if ( !shape )
				continue;
			const QSet<int> keep = selectedVertices.value( block );
			QSet<int> & hidden = editHiddenTris[block];
			for ( int triangle = 0; triangle < shape->triangles.size(); triangle++ ) {
				const Triangle & t = shape->triangles.at( triangle );
				// Hiding unselected vertices in Blender also hides any face that
				// needs one of those vertices. The same rule naturally covers
				// edge and face selections through pickedVertexRefs().
				if ( !keep.contains( t[0] ) || !keep.contains( t[1] ) || !keep.contains( t[2] ) )
					hidden.insert( triangle );
			}
		}
		scene->hiddenTris = editHiddenTris;
		emit gizmoStatus( tr( "Isolated selected geometry  (Restore All to reveal everything)" ) );
		update();
		return;
	}

	if ( objSelection.isEmpty() ) {
		emit gizmoStatus( tr( "Select one or more objects before isolating them" ) );
		return;
	}

	// Preserve selected objects, their descendants, and the ancestor chain
	// needed to reach them. Every unrelated NiAVObject can be hidden safely.
	QSet<int> relevant;
	for ( int selected : objSelection ) {
		for ( int p = selected; p >= 0; p = model->getParent( p ) )
			relevant.insert( p );
		for ( int b = 0; b < model->getBlockCount(); b++ ) {
			for ( int p = b; p >= 0; p = model->getParent( p ) ) {
				if ( p == selected ) {
					relevant.insert( b );
					break;
				}
			}
		}
	}
	soloMode = false;
	scene->soloNode = -1;
	scene->hiddenNodes.clear();
	for ( int b = 0; b < model->getBlockCount(); b++ ) {
		QModelIndex block = model->getBlockIndex( b );
		if ( model->blockInherits( block, "NiAVObject" ) && !relevant.contains( b ) )
			scene->hiddenNodes.insert( b );
	}
	updateDimmedBlocks();
	emit gizmoStatus( tr( "Isolated %1 selected object(s)" ).arg( objSelection.size() ) );
	update();
}

void GLView::isolatePrimary()
{
	if ( !model || !scene )
		return;
	int primary = editMode ? editShapeBlock : objActive;
	if ( primary < 0 && scene->currentBlock.isValid() ) {
		primary = model->getBlockNumber( QModelIndex( scene->currentBlock ) );
		while ( primary >= 0 && !model->blockInherits( model->getBlockIndex( primary ), "NiAVObject" ) )
			primary = model->getParent( primary );
	}
	if ( primary < 0 ) {
		emit gizmoStatus( tr( "No primary object is selected" ) );
		return;
	}

	// A primary is one subtree, so the scene's explicit isolation slot is both
	// cheaper and more exact than building a hidden-node set for every sibling.
	soloMode = false;
	scene->hiddenNodes.clear();
	updateDimmedBlocks();
	scene->soloNode = primary;
	emit gizmoStatus( tr( "Isolated primary object %1" ).arg( primary ) );
	update();
}

void GLView::hideSecondarySelection()
{
	if ( !model || !scene )
		return;
	int primary = editMode ? editShapeBlock : objActive;
	int hidden = 0;
	for ( int block : objSelection ) {
		if ( block != primary && model->blockInherits( model->getBlockIndex( block ), "NiAVObject" ) ) {
			scene->hiddenNodes.insert( block );
			hidden++;
		}
	}
	if ( hidden <= 0 ) {
		emit gizmoStatus( tr( "No secondary selected objects to hide" ) );
		return;
	}
	updateDimmedBlocks();
	emit gizmoStatus( tr( "Hid %1 secondary selected object(s)" ).arg( hidden ) );
	update();
}

void GLView::restoreAllVisibility()
{
	if ( !scene )
		return;
	soloMode = false;
	scene->soloNode = -1;
	editHiddenTris.clear();
	scene->hiddenTris.clear();
	scene->hiddenNodes.clear();
	updateDimmedBlocks();
	emit gizmoStatus( tr( "Restored all hidden geometry and objects" ) );
	update();
}

Vector3 GLView::pickedMedian() const
{
	Vector3 m;
	if ( pickedElems.isEmpty() )
		return m;
	int n = 0;
	for ( const auto & pe : pickedElems ) {
		// recompute from live vertex data so the pivot tracks billboards /
		// animation as the camera moves, instead of the cached pick position
		Shape * s = shapeForBlock( pe.shapeBlock );
		if ( s ) {
			Transform wt = shapeRenderTrans( s );
			int nv = s->verts.size();
			if ( pe.type == 1 && pe.e0 < nv ) {
				m += wt * editVertexLocal( s, pe.e0 );
				n++;
				continue;
			} else if ( pe.type == 2 && pe.e0 < nv && pe.e1 < nv ) {
				m += ( wt * editVertexLocal( s, pe.e0 ) + wt * editVertexLocal( s, pe.e1 ) ) * 0.5f;
				n++;
				continue;
			} else if ( pe.type == 3 && pe.e0 >= 0 && pe.e0 < s->triangles.size() ) {
				const Triangle & t = s->triangles.at( pe.e0 );
				if ( t[0] < nv && t[1] < nv && t[2] < nv ) {
					m += ( wt * editVertexLocal( s, t[0] ) + wt * editVertexLocal( s, t[1] )
						+ wt * editVertexLocal( s, t[2] ) ) / 3.0f;
					n++;
					continue;
				}
			}
		}
		m += pe.worldPos;	// fallback to cached
		n++;
	}
	return ( n > 0 ) ? ( m / float( n ) ) : m;
}

float GLView::nearestScreenVertex( const QPointF & pos, float radius,
	const QSet<int> * only, PickedElement & out ) const
{
	out = PickedElement();
	float best = radius;
	// occlusion test: a vertex hidden behind opaque geometry is not pickable
	// (Blender picks front-most; X-ray mode deliberately picks through). One
	// raycast at the vertex's own screen position, compared along the view
	// ray — a vertex ON the hit surface (its own triangles) passes the
	// tolerance. Only candidates that would become the winner are tested.
	auto visible = [this, only]( const Vector3 & wv, const QPointF & sp ) {
		if ( !scene || scene->xRay )
			return true;
		Vector3 rayO, rayD;
		mouseRayWorld( sp, rayO, rayD );
		SceneRayHit hit = raycastScene( sp, -1, only );
		if ( !hit.shape )
			return true;
		const float tVert = Vector3::dotproduct( wv - rayO, rayD );
		return !( hit.dist < tVert * 0.999f - 0.01f );
	};
	for ( Shape * s : scene->shapes ) {
		if ( !s || s->isHidden() )
			continue;
		if ( only && !only->contains( s->id() ) )
			continue;
		Transform wt = shapeRenderTrans( s );
		for ( int i = 0; i < s->verts.size(); i++ ) {
			QPointF sp;
			Vector3 wv = wt * editVertexLocal( s, i );
			if ( !worldToScreen( wv, sp ) )
				continue;
			float d = float( std::hypot( sp.x() - pos.x(), sp.y() - pos.y() ) );
			if ( d < best && visible( wv, sp ) ) {
				best = d;
				out.shapeBlock = s->id();
				out.type = 1;
				out.e0 = i;
				out.e1 = -1;
				out.worldPos = wv;
				out.wA = out.wB = out.wC = wv;
			}
		}
	}
	return best;
}

bool GLView::pickElementUnder( const QPointF & pos, PickedElement & pe ) const
{
	// in edit mode restrict picks to the mesh(es) being edited
	const QSet<int> * only = ( editMode && !editShapeBlocks.isEmpty() ) ? &editShapeBlocks : nullptr;
	SceneRayHit hit = raycastScene( pos, -1, only );
	pe = PickedElement();

	if ( hit.shape ) {
		Shape * s = hit.shape;
		Transform wt = shapeRenderTrans( s );
		const Triangle & tri = s->triangles.at( hit.tri );
		Vector3 va = editVertexLocal( s, tri[0] ), vb = editVertexLocal( s, tri[1] );
		Vector3 vc = editVertexLocal( s, tri[2] );

		pe.shapeBlock = s->id();
		pe.wA = wt * va;
		pe.wB = wt * vb;
		pe.wC = wt * vc;

		// choose the element type among the enabled modes (bitmask) by cursor
		// proximity, Blender-style: vertex, then edge, then face
		int mode = 0;
		{
			QPointF sa, sb, sc;
			bool oka = worldToScreen( pe.wA, sa ), okb = worldToScreen( pe.wB, sb ), okc = worldToScreen( pe.wC, sc );
			float dv = 1.0e9f;
			if ( oka ) dv = std::min( dv, float( std::hypot( sa.x() - pos.x(), sa.y() - pos.y() ) ) );
			if ( okb ) dv = std::min( dv, float( std::hypot( sb.x() - pos.x(), sb.y() - pos.y() ) ) );
			if ( okc ) dv = std::min( dv, float( std::hypot( sc.x() - pos.x(), sc.y() - pos.y() ) ) );
			float de = 1.0e9f;
			if ( oka && okb ) de = std::min( de, tlPtSegDist( pos, sa, sb ) );
			if ( okb && okc ) de = std::min( de, tlPtSegDist( pos, sb, sc ) );
			if ( okc && oka ) de = std::min( de, tlPtSegDist( pos, sc, sa ) );

			if ( ( pickMode & 1 ) && dv < 11.0f )
				mode = 1;
			else if ( ( pickMode & 2 ) && de < 8.0f )
				mode = 2;
			else if ( pickMode & 4 )
				mode = 3;
			else if ( pickMode & 1 )
				mode = 1;
			else if ( pickMode & 2 )
				mode = 2;
			else
				mode = 3;
		}
		pe.type = mode;

		if ( mode == 1 ) {
			// nearest corner of the hit triangle
			float d0 = ( va - hit.hitLocal ).squaredLength();
			float d1 = ( vb - hit.hitLocal ).squaredLength();
			float d2 = ( vc - hit.hitLocal ).squaredLength();
			int corner = ( d0 <= d1 && d0 <= d2 ) ? 0 : ( d1 <= d2 ? 1 : 2 );
			pe.e0 = tri[corner];
			pe.worldPos = wt * editVertexLocal( s, pe.e0 );
			pe.wA = pe.wB = pe.wC = pe.worldPos;
			// a floating vertex (extruded spur) in front of this surface never
			// raycasts — prefer it when it is closer to the cursor on screen
			QPointF cs;
			const float cornerDist = worldToScreen( pe.worldPos, cs )
				? float( std::hypot( cs.x() - pos.x(), cs.y() - pos.y() ) ) : 1.0e9f;
			PickedElement freeVert;
			const float freeDist = nearestScreenVertex( pos, std::min( cornerDist, 11.0f ), only, freeVert );
			if ( freeVert.shapeBlock >= 0 && freeDist < cornerDist )
				pe = freeVert;
		} else if ( mode == 2 ) {
			// nearest edge of the hit triangle
			auto edgeDist = [&hit]( const Vector3 & a, const Vector3 & b ) {
				Vector3 d = b - a;
				float len2 = d.squaredLength();
				float t = ( len2 > 1.0e-12f )
				          ? std::min( std::max( Vector3::dotproduct( hit.hitLocal - a, d ) / len2, 0.0f ), 1.0f ) : 0.0f;
				return ( a + d * t - hit.hitLocal ).squaredLength();
			};
			float d01 = edgeDist( va, vb ), d12 = edgeDist( vb, vc ), d20 = edgeDist( vc, va );
			int ea, eb;
			if ( d01 <= d12 && d01 <= d20 ) {
				ea = tri[0]; eb = tri[1];
			} else if ( d12 <= d20 ) {
				ea = tri[1]; eb = tri[2];
			} else {
				ea = tri[2]; eb = tri[0];
			}
			pe.e0 = std::min( ea, eb );
			pe.e1 = std::max( ea, eb );
			pe.wA = wt * editVertexLocal( s, pe.e0 );
			pe.wB = wt * editVertexLocal( s, pe.e1 );
			pe.wC = pe.wA;
			pe.worldPos = ( pe.wA + pe.wB ) / 2.0f;
		} else {
			pe.e0 = hit.tri;
			pe.worldPos = ( pe.wA + pe.wB + pe.wC ) / 3.0f;
			Vector3 n = Vector3::crossproduct( pe.wB - pe.wA, pe.wC - pe.wA );
			n.normalize();
			pe.worldNormal = n;
		}
	} else if ( pickMode & 1 ) {
		// off-surface: nearest vertex by screen distance (restricted in edit mode)
		nearestScreenVertex( pos, 12.0f, only, pe );
		if ( pe.shapeBlock < 0 )
			return false;
	} else {
		return false;
	}

	return true;
}

bool GLView::pickElementAt( const QPointF & pos, bool additive )
{
	PickedElement pe;
	if ( !pickElementUnder( pos, pe ) )
		return false;
	recordSelection();

	// picking one half of a marked quad picks the whole quad (both tris)
	PickedElement partner;
	if ( pe.type == 3 ) {
		const int tj = quadPartnerTri( pe.shapeBlock, pe.e0 );
		Shape * s = ( tj >= 0 ) ? shapeForBlock( pe.shapeBlock ) : nullptr;
		if ( s && tj < s->triangles.size() ) {
			const Triangle & t = s->triangles.at( tj );
			Transform wt = shapeRenderTrans( s );
			partner.shapeBlock = pe.shapeBlock;
			partner.type = 3;
			partner.e0 = tj;
			partner.wA = wt * editVertexLocal( s, t[0] );
			partner.wB = wt * editVertexLocal( s, t[1] );
			partner.wC = wt * editVertexLocal( s, t[2] );
			partner.worldPos = ( partner.wA + partner.wB + partner.wC ) / 3.0f;
			Vector3 n = Vector3::crossproduct( partner.wB - partner.wA, partner.wC - partner.wA );
			n.normalize();
			partner.worldNormal = n;
		}
	}
	const bool havePartner = ( partner.shapeBlock >= 0 );

	if ( additive ) {
		int at = pickedElems.indexOf( pe );
		if ( at >= 0 ) {
			pickedElems.remove( at );
			if ( havePartner ) {
				int pat = pickedElems.indexOf( partner );
				if ( pat >= 0 )
					pickedElems.remove( pat );
			}
		} else {
			if ( havePartner && pickedElems.indexOf( partner ) < 0 )
				pickedElems.append( partner );
			pickedElems.append( pe );
		}
	} else {
		pickedElems.clear();
		if ( havePartner )
			pickedElems.append( partner );
		pickedElems.append( pe );
	}

	update();
	return true;
}

bool GLView::pickPathSelect( const QPointF & pos )
{
	// nothing to path from: behave like a plain extend
	if ( pickedElems.isEmpty() )
		return pickElementAt( pos, true );

	PickedElement target;
	if ( !pickElementUnder( pos, target ) )
		return false;
	recordSelection();

	const PickedElement active = pickedElems.constLast();
	Shape * s = shapeForBlock( target.shapeBlock );
	if ( active.shapeBlock != target.shapeBlock || active.type != target.type
		|| !s || s->triangles.isEmpty() ) {
		// no path across shapes or element types: just extend
		if ( pickedElems.indexOf( target ) < 0 )
			pickedElems.append( target );
		update();
		return true;
	}

	int nv = s->verts.size();
	Transform wt = shapeRenderTrans( s );
	auto appendElem = [this]( const PickedElement & pe ) {
		int at = pickedElems.indexOf( pe );
		if ( at >= 0 )
			pickedElems.remove( at );	// re-append so the path end becomes active
		pickedElems.append( pe );
	};
	auto vertexElem = [&]( int vi ) {
		PickedElement pe;
		pe.shapeBlock = target.shapeBlock;
		pe.type = 1;
		pe.e0 = vi;
		pe.worldPos = wt * editVertexLocal( s, vi );
		pe.wA = pe.wB = pe.wC = pe.worldPos;
		return pe;
	};
	auto edgeElem = [&]( int a, int b ) {
		PickedElement pe;
		pe.shapeBlock = target.shapeBlock;
		pe.type = 2;
		pe.e0 = std::min( a, b );
		pe.e1 = std::max( a, b );
		pe.wA = wt * editVertexLocal( s, pe.e0 );
		pe.wB = wt * editVertexLocal( s, pe.e1 );
		pe.wC = pe.wA;
		pe.worldPos = ( pe.wA + pe.wB ) / 2.0f;
		return pe;
	};

	if ( target.type == 3 ) {
		// face path: BFS over triangles connected by shared edges
		QHash<QPair<int, int>, QVector<int>> etris;
		for ( int ti = 0; ti < s->triangles.size(); ti++ ) {
			const Triangle & t = s->triangles.at( ti );
			if ( t[0] >= nv || t[1] >= nv || t[2] >= nv )
				continue;
			for ( int e = 0; e < 3; e++ ) {
				int a = t[e], b = t[( e + 1 ) % 3];
				etris[qMakePair( std::min( a, b ), std::max( a, b ) )].append( ti );
			}
		}
		QHash<int, int> prev;
		prev.insert( active.e0, active.e0 );
		QList<int> queue{ active.e0 };
		while ( !queue.isEmpty() && !prev.contains( target.e0 ) ) {
			int ti = queue.takeFirst();
			const Triangle & t = s->triangles.at( ti );
			for ( int e = 0; e < 3; e++ ) {
				int a = t[e], b = t[( e + 1 ) % 3];
				for ( int nb : etris.value( qMakePair( std::min( a, b ), std::max( a, b ) ) ) ) {
					if ( !prev.contains( nb ) ) {
						prev.insert( nb, ti );
						queue.append( nb );
					}
				}
			}
		}
		if ( prev.contains( target.e0 ) ) {
			QVector<int> path;
			for ( int ti = target.e0; ti != active.e0; ti = prev.value( ti ) )
				path.prepend( ti );
			for ( int ti : path ) {
				const Triangle & t = s->triangles.at( ti );
				PickedElement pe;
				pe.shapeBlock = target.shapeBlock;
				pe.type = 3;
				pe.e0 = ti;
				pe.wA = wt * editVertexLocal( s, t[0] );
				pe.wB = wt * editVertexLocal( s, t[1] );
				pe.wC = wt * editVertexLocal( s, t[2] );
				pe.worldPos = ( pe.wA + pe.wB + pe.wC ) / 3.0f;
				Vector3 n = Vector3::crossproduct( pe.wB - pe.wA, pe.wC - pe.wA );
				n.normalize();
				pe.worldNormal = n;
				appendElem( pe );
			}
		} else {
			appendElem( target );
		}
		update();
		return true;
	}

	// vertex / edge path: BFS over the vertex-edge graph
	QHash<int, QVector<int>> adj;
	for ( const Triangle & t : s->triangles ) {
		if ( t[0] >= nv || t[1] >= nv || t[2] >= nv )
			continue;
		for ( int e = 0; e < 3; e++ ) {
			int a = t[e], b = t[( e + 1 ) % 3];
			adj[a].append( b );
			adj[b].append( a );
		}
	}

	QHash<int, int> prev;
	QList<int> queue;
	QSet<int> goals;
	if ( target.type == 1 ) {
		prev.insert( active.e0, active.e0 );
		queue.append( active.e0 );
		goals.insert( target.e0 );
	} else {
		// both endpoints of the active edge seed the search; stop at either
		// endpoint of the target edge
		prev.insert( active.e0, active.e0 );
		prev.insert( active.e1, active.e1 );
		queue << active.e0 << active.e1;
		goals << target.e0 << target.e1;
	}

	int reached = -1;
	while ( !queue.isEmpty() && reached < 0 ) {
		int v = queue.takeFirst();
		if ( goals.contains( v ) ) {
			reached = v;
			break;
		}
		for ( int nb : adj.value( v ) ) {
			if ( !prev.contains( nb ) ) {
				prev.insert( nb, v );
				queue.append( nb );
			}
		}
	}

	if ( reached < 0 ) {
		appendElem( target );	// disconnected: just extend
		update();
		return true;
	}

	// walk the predecessor chain back from the reached goal to the seed
	QVector<int> path;
	for ( int v = reached; ; v = prev.value( v ) ) {
		path.prepend( v );
		if ( prev.value( v ) == v )
			break;
	}

	if ( target.type == 1 ) {
		for ( int vi : path )
			appendElem( vertexElem( vi ) );
		appendElem( vertexElem( target.e0 ) );
	} else {
		for ( int i = 0; i + 1 < path.size(); i++ )
			appendElem( edgeElem( path.at( i ), path.at( i + 1 ) ) );
		appendElem( edgeElem( target.e0, target.e1 ) );
	}

	update();
	return true;
}

void GLView::placeCursor( const QPointF & pos )
{
	SceneRayHit hit = raycastScene( pos );
	if ( hit.shape ) {
		Transform wt = shapeRenderTrans( hit.shape );
		cursorPos = wt * hit.hitLocal;
	} else {
		Vector3 ro, rd;
		mouseRayWorld( pos, ro, rd );
		cursorPos = ro + rd * float( Dist * 2 );
	}
	update();
}

bool GLView::snapBlockWorldPos( int b, const Vector3 & worldPos )
{
	if ( !model || b < 0 )
		return false;
	QModelIndex iBlock = model->getBlockIndex( b );
	QModelIndex iField = model->getIndex( iBlock, "Translation" );
	if ( !iField.isValid() )
		return false;

	// world position -> local translation under the parent
	Matrix pr;
	Vector3 pp;
	float ps = 1.0f;
	int parentNum = model->getParent( b );
	Node * pn = ( parentNum >= 0 ) ? scene->getNode( model, model->getBlockIndex( parentNum ) ) : nullptr;
	if ( pn ) {
		Transform pt = pn->worldTrans();
		pr = pt.rotation;
		pp = pt.translation;
		ps = ( pt.scale != 0.0f ) ? pt.scale : 1.0f;
	}
	Vector3 nt = pr.inverted() * ( ( worldPos - pp ) * ( 1.0f / ps ) );

	QModelIndex vIdx = iField.sibling( iField.row(), NifModel::ValueCol );
	const NifItem * item = static_cast<const NifItem *>( vIdx.internalPointer() );
	if ( !item )
		return false;
	NifValue oldVal = item->value();
	NifValue nv = oldVal;
	if ( nv.set( nt, model, item ) )
		model->undoStack->push( new ChangeValueCommand( vIdx, oldVal, nv, model->itemName( iField ), model ) );
	update();
	return true;
}

void GLView::snapNodeToCursor()
{
	if ( !model || !scene->currentBlock.isValid() )
		return;

	int b = model->getBlockNumber( QModelIndex( scene->currentBlock ) );
	while ( b >= 0 && !model->blockInherits( model->getBlockIndex( b ), "NiAVObject" ) )
		b = model->getParent( b );
	if ( b < 0 )
		return;

	ChangeValueCommand::createTransaction();
	snapBlockWorldPos( b, cursorPos );
}

QHash<int, QSet<int>> GLView::pickedVertexRefs() const
{
	// collect target vertices per shape (faces/edges contribute their corners)
	QHash<int, QSet<int>> byShape;
	for ( const auto & pe : pickedElems ) {
		if ( pe.shapeBlock < 0 )
			continue;
		if ( pe.type == 1 ) {
			byShape[pe.shapeBlock].insert( pe.e0 );
		} else if ( pe.type == 2 ) {
			byShape[pe.shapeBlock].insert( pe.e0 );
			byShape[pe.shapeBlock].insert( pe.e1 );
		} else if ( pe.type == 3 ) {
			Node * n = scene->getNode( model, model->getBlockIndex( pe.shapeBlock ) );
			Shape * s = static_cast<Shape *>( n );
			if ( s && pe.e0 >= 0 && pe.e0 < s->triangles.size() ) {
				const Triangle & tri = s->triangles.at( pe.e0 );
				byShape[pe.shapeBlock].insert( tri[0] );
				byShape[pe.shapeBlock].insert( tri[1] );
				byShape[pe.shapeBlock].insert( tri[2] );
			}
		}
	}
	return byShape;
}

void GLView::movePickedVertsToCursor()
{
	if ( !model || pickedElems.isEmpty() )
		return;

	QHash<int, QSet<int>> byShape = pickedVertexRefs();
	if ( byShape.isEmpty() )
		return;

	nifSnapshotOp( model, tr( "Move vertices to 3D cursor" ), [&]() {
		for ( auto it = byShape.constBegin(); it != byShape.constEnd(); it++ ) {
			QModelIndex iShape = model->getBlockIndex( it.key() );
			Node * n = scene->getNode( model, iShape );
			Shape * shape = shapeForBlock( it.key() );
			if ( !n || !shape )
				continue;
			Transform wt = shapeRenderTrans( n );
			float sc = ( wt.scale != 0.0f ) ? wt.scale : 1.0f;
			Vector3 local = wt.rotation.inverted() * ( ( cursorPos - wt.translation ) * ( 1.0f / sc ) );

			QModelIndex iVData = model->getIndex( iShape, "Vertex Data" );
			QModelIndex iVerts = model->getIndex( model->getBlockIndex( model->getLink( iShape, "Data" ) ), "Vertices" );
			if ( !iVData.isValid() && !iVerts.isValid() )
				iVerts = model->getIndex( iShape, "Vertices" );

			for ( int vi : it.value() ) {
				Vector3 rawLocal;
				if ( !editVertexRawLocal( shape, vi, local, rawLocal ) )
					continue;
				if ( iVData.isValid() && vi < model->rowCount( iVData ) ) {
					QModelIndex iv = model->getIndex( model->getIndex( iVData, vi ), "Vertex" );
					if ( iv.isValid() ) {
						const NifItem * item = static_cast<const NifItem *>( iv.internalPointer() );
						if ( item && item->hasValueType( NifValue::tHalfVector3 ) )
							model->set<HalfVector3>( iv, HalfVector3( rawLocal ) );
						else
							model->set<Vector3>( iv, rawLocal );
					}
				} else if ( iVerts.isValid() && vi < model->rowCount( iVerts ) ) {
					model->set<Vector3>( model->getIndex( iVerts, vi ), rawLocal );
				}
			}
		}
	} );

	pickedElems.clear();
	modelChanged();	// rebuild the shape display lists
}

//! Read one vertex's stored (local) position from a shape block
static bool tlGetVertexLocal( NifModel * model, const QModelIndex & iShape, int vi, Vector3 & out )
{
	QModelIndex iVData = model->getIndex( iShape, "Vertex Data" );
	if ( iVData.isValid() && vi >= 0 && vi < model->rowCount( iVData ) ) {
		QModelIndex iv = model->getIndex( model->getIndex( iVData, vi ), "Vertex" );
		if ( iv.isValid() ) {
			out = model->get<Vector3>( iv );
			return true;
		}
	}
	QModelIndex iVerts = model->getIndex( model->getBlockIndex( model->getLink( iShape, "Data" ) ), "Vertices" );
	if ( !iVerts.isValid() )
		iVerts = model->getIndex( iShape, "Vertices" );
	if ( iVerts.isValid() && vi >= 0 && vi < model->rowCount( iVerts ) ) {
		out = model->get<Vector3>( model->getIndex( iVerts, vi ) );
		return true;
	}
	return false;
}

//! Write one vertex's stored (local) position to a shape block
static void tlSetVertexLocal( NifModel * model, const QModelIndex & iShape, int vi, const Vector3 & local )
{
	QModelIndex iVData = model->getIndex( iShape, "Vertex Data" );
	if ( iVData.isValid() && vi >= 0 && vi < model->rowCount( iVData ) ) {
		QModelIndex iv = model->getIndex( model->getIndex( iVData, vi ), "Vertex" );
		if ( iv.isValid() ) {
			// FO4 stores vertices as half-precision; set<Vector3> would be rejected
			const NifItem * item = static_cast<const NifItem *>( iv.internalPointer() );
			if ( item && item->hasValueType( NifValue::tHalfVector3 ) )
				model->set<HalfVector3>( iv, HalfVector3( local ) );
			else
				model->set<Vector3>( iv, local );
		}
		return;
	}
	QModelIndex iVerts = model->getIndex( model->getBlockIndex( model->getLink( iShape, "Data" ) ), "Vertices" );
	if ( !iVerts.isValid() )
		iVerts = model->getIndex( iShape, "Vertices" );
	if ( iVerts.isValid() && vi >= 0 && vi < model->rowCount( iVerts ) )
		model->set<Vector3>( model->getIndex( iVerts, vi ), local );
}

int GLView::mirrorPartnerOf( int shapeBlock, int vi ) const
{
	Shape * s = shapeForBlock( shapeBlock );
	if ( !s || vi < 0 || vi >= s->verts.size() )
		return -1;
	auto it = mirrorPairCache.constFind( shapeBlock );
	if ( it != mirrorPairCache.constEnd() && it->first == s->verts.size() )
		return it->second.value( vi, -1 );

	// build: spatial hash of the positions, then pair each vertex with the
	// nearest X-negated match within tolerance (27-cell probe so grid
	// boundaries cannot hide a partner); center-line verts pair with nobody
	const float tol = 1.0e-3f;
	auto cellKey = []( int x, int y, int z ) {
		return ( quint64( quint32( x ) ) * 73856093ULL )
			^ ( quint64( quint32( y ) ) * 19349663ULL )
			^ ( quint64( quint32( z ) ) * 83492791ULL );
	};
	auto cellOf = [tol]( float v ) { return int( std::floor( v / ( tol * 2.0f ) ) ); };
	QHash<quint64, QVector<int>> buckets;
	const int nv = s->verts.size();
	for ( int i = 0; i < nv; i++ ) {
		const Vector3 & p = s->verts.at( i );
		buckets[cellKey( cellOf( p[0] ), cellOf( p[1] ), cellOf( p[2] ) )].append( i );
	}
	QHash<int, int> pairs;
	for ( int i = 0; i < nv; i++ ) {
		const Vector3 & p = s->verts.at( i );
		if ( std::fabs( p[0] ) <= tol )
			continue;
		const Vector3 m( -p[0], p[1], p[2] );
		int best = -1;
		float bestD = tol;
		const int cx = cellOf( m[0] ), cy = cellOf( m[1] ), cz = cellOf( m[2] );
		for ( int ox = -1; ox <= 1; ox++ ) {
			for ( int oy = -1; oy <= 1; oy++ ) {
				for ( int oz = -1; oz <= 1; oz++ ) {
					auto bIt = buckets.constFind( cellKey( cx + ox, cy + oy, cz + oz ) );
					if ( bIt == buckets.constEnd() )
						continue;
					for ( int j : *bIt ) {
						if ( j == i )
							continue;
						const float d = ( s->verts.at( j ) - m ).length();
						if ( d < bestD ) {
							bestD = d;
							best = j;
						}
					}
				}
			}
		}
		if ( best >= 0 )
			pairs.insert( i, best );
	}
	QPair<int, QHash<int, int>> & slot = mirrorPairCache[shapeBlock];
	slot.first = nv;
	slot.second = pairs;
	return slot.second.value( vi, -1 );
}

bool GLView::gizmoBeginElement( int mode )
{
	if ( !model || pickedElems.isEmpty() )
		return false;

	QHash<int, QSet<int>> byShape = pickedVertexRefs();
	if ( byShape.isEmpty() )
		return false;

	elemVerts.clear();
	for ( auto it = byShape.constBegin(); it != byShape.constEnd(); it++ ) {
		QModelIndex iShape = model->getBlockIndex( it.key() );
		Node * n = scene->getNode( model, iShape );
		Shape * shape = shapeForBlock( it.key() );
		if ( !n || !shape )
			continue;
		Transform wt = shapeRenderTrans( n );
		for ( int vi : it.value() ) {
			Vector3 local;
			if ( !tlGetVertexLocal( model, iShape, vi, local ) )
				continue;
			ElemVert ev;
			ev.shape = it.key();
			ev.idx = vi;
			ev.origLocal = local;
			ev.origWorld = wt * ( editDeformedCageActive() ? shape->skinVertex( vi, local ) : local );
			ev.currentLocal = local;
			elemVerts.append( ev );
		}
	}
	if ( elemVerts.isEmpty() )
		return false;

	// pivot: 3D cursor if selected, otherwise the median of the picked verts
	if ( gizmoPivot == 3 ) {
		elemPivot = cursorPos;
	} else {
		Vector3 m;
		for ( const auto & ev : elemVerts )
			m += ev.origWorld;
		elemPivot = m / float( elemVerts.size() );
	}

	// X-mirror: unselected mirror partners join the gesture as FOLLOWERS —
	// they take the X-negated local of their source each update, and are
	// excluded from the pivot and from the direct transform
	if ( mirrorEditing ) {
		QSet<quint64> inGesture;
		for ( const auto & ev : elemVerts )
			inGesture.insert( ( quint64( quint32( ev.shape ) ) << 32 ) | quint32( ev.idx ) );
		const int nSrc = elemVerts.size();
		for ( int i = 0; i < nSrc; i++ ) {
			const ElemVert src = elemVerts.at( i );
			const int mi = mirrorPartnerOf( src.shape, src.idx );
			if ( mi < 0 )
				continue;
			const quint64 key = ( quint64( quint32( src.shape ) ) << 32 ) | quint32( mi );
			if ( inGesture.contains( key ) )
				continue;	// both sides selected: both transform directly
			QModelIndex iShape = model->getBlockIndex( src.shape );
			Node * pn = scene->getNode( model, iShape );
			Shape * shape = shapeForBlock( src.shape );
			if ( !pn || !shape )
				continue;
			Vector3 local;
			if ( !tlGetVertexLocal( model, iShape, mi, local ) )
				continue;
			ElemVert ev;
			ev.shape = src.shape;
			ev.idx = mi;
			ev.origLocal = local;
			ev.origWorld = shapeRenderTrans( pn )
				* ( editDeformedCageActive() ? shape->skinVertex( mi, local ) : local );
			ev.currentLocal = local;
			ev.mirrorOf = i;
			elemVerts.append( ev );
			inGesture.insert( key );
		}
	}

	gizmoMode = mode;
	gizmoAxis = 0;
	gizmoPlane = 0;
	gizmoAxisLocal = false;
	gizmoTrackball = false;
	gizmoNum = QStringList() << QString() << QString() << QString();
	gizmoNumCur = 0;
	gizmoStartPos = mapFromGlobal( QCursor::pos() );
	gizmoBasisM = Matrix();	// element transforms use the global frame
	gizmoBasisOrig = gizmoBasisM;
	elemTransform = true;

	static const char * modeNames[4] = { "", "Move", "Rotate", "Scale" };
	emit gizmoStatus( tr( "%1 %2 element(s):  move mouse or type a value, X/Y/Z = axis, Ctrl = snap, LMB/Enter = commit, Esc = cancel" )
		.arg( QLatin1String( modeNames[mode] ) ).arg( elemVerts.size() ) );

	// Blender: the modal gesture owns the mouse and keyboard (see gizmoBegin)
	gizmoWrapOffset = QPoint();
	setMouseGrabEnabled( true );
	setKeyboardGrabEnabled( true );

	update();
	return true;
}

void GLView::gizmoUpdateElement( const QPoint & pos, Qt::KeyboardModifiers mods )
{
	if ( !elemTransform || !model )
		return;

	float dx = pos.x() - gizmoStartPos.x();
	float dy = pos.y() - gizmoStartPos.y();
	const bool numeric = gizmoNumActive();
	bool snap = ( ( ( mods & Qt::ControlModifier ) != 0 ) != snapDefaultOn ) && !numeric;
	if ( snap && !( snapAffect & ( 1 << ( gizmoMode - 1 ) ) ) )
		snap = false;
	snapIndicator = false;
	float precision = ( mods & Qt::ShiftModifier ) ? 0.2f : 1.0f;

	Matrix vm;
	vm.fromEuler( deg2rad( Rot[0] ), deg2rad( Rot[1] ), deg2rad( Rot[2] ) );
	Vector3 camRight( vm( 0, 0 ), vm( 0, 1 ), vm( 0, 2 ) );
	Vector3 camUp( vm( 1, 0 ), vm( 1, 1 ), vm( 1, 2 ) );
	Vector3 camFwd( vm( 2, 0 ), vm( 2, 1 ), vm( 2, 2 ) );

	// cache each shape's world transform to convert new world positions back to local
	QHash<int, Transform> xf;
	auto toLocal = [&]( int shape, const Vector3 & w ) {
		if ( !xf.contains( shape ) ) {
			Node * n = scene->getNode( model, model->getBlockIndex( shape ) );
			xf.insert( shape, n ? shapeRenderTrans( n ) : Transform() );
		}
		Transform wt = xf.value( shape );
		float sc = ( wt.scale != 0.0f ) ? wt.scale : 1.0f;
		return wt.rotation.inverted() * ( ( w - wt.translation ) * ( 1.0f / sc ) );
	};

	// Edit-mode dragging is a renderer preview. Writing every selected packed
	// vertex into NifModel on every mouse event emitted thousands of model
	// notifications and made BSSubIndexTriShape transforms progressively slower.
	// Keep the authored model untouched until release and invalidate each affected
	// shape's GPU cache once per sample.
	QSet<Shape *> previewShapes;
	auto previewVertex = [&]( ElemVert & ev, const Vector3 & cageLocal ) {
		Shape * shape = shapeForBlock( ev.shape );
		if ( shape && ev.idx >= 0 && ev.idx < shape->verts.size() ) {
			Vector3 rawLocal;
			if ( !editVertexRawLocal( shape, ev.idx, cageLocal, rawLocal ) )
				return;
			ev.currentLocal = rawLocal;
			shape->verts[ev.idx] = rawLocal;
			previewShapes.insert( shape );
		}
	};

	QString status;

	if ( gizmoMode == 1 ) {
		float wpp = std::max( (float)Dist, 0.01f ) * 2.0f / std::max( height(), 1 ) * precision;
		Vector3 deltaWorld;
		if ( numeric ) {
			if ( gizmoAxis == 0 )
				deltaWorld = Vector3( gizmoPartVal( gizmoNum, 0 ), gizmoPartVal( gizmoNum, 1 ), gizmoPartVal( gizmoNum, 2 ) );
			else {
				Vector3 u;
				u[gizmoAxis - 1] = 1.0f;
				deltaWorld = u * gizmoPartVal( gizmoNum, 0 );
			}
		} else if ( gizmoAxis == 0 ) {
			deltaWorld = camRight * ( dx * wpp ) + camUp * ( -dy * wpp );
		} else {
			Vector3 u;
			u[gizmoAxis - 1] = 1.0f;
			float amount = ( dx * wpp ) * ( Vector3::dotproduct( camRight, u ) >= 0 ? 1.0f : -1.0f )
			             + ( -dy * wpp ) * ( Vector3::dotproduct( camUp, u ) >= 0 ? 1.0f : -1.0f );
			deltaWorld = u * amount;
		}

		// element snapping (vertex/edge/face target): the selection median
		// lands on the geometry under the mouse. The edited mesh itself is a
		// valid target (Blender snaps to unselected geometry of the same
		// mesh); only triangles touching the actively dragged vertices are
		// rejected, so the selection can't chase itself.
		bool elemSnapped = false;
		if ( snap && snapTargetMode > 0 ) {
			Vector3 target;
			bool haveTarget = false;

			if ( snapTargetMode == 1 ) {
				// Blender vertex snap: nearest vertex to the cursor in SCREEN space.
				// A surface raycast only fires when the cursor is exactly over a
				// triangle, so it missed whenever the pointer sat just beside the
				// target vertex (over empty space) - that was the "can't snap on an
				// axis" report. Dragged vertices are skipped so it can't chase itself.
				float bestD = 24.0f;
				for ( Shape * s : scene->shapes ) {
					if ( !s || s->isHidden() || s->verts.isEmpty() )
						continue;
					int sid = s->id();
					Transform wt = shapeRenderTrans( s );
					for ( int i = 0; i < s->verts.size(); i++ ) {
						bool dragged = false;
						for ( const auto & ev : elemVerts )
							if ( ev.shape == sid && ev.idx == i ) { dragged = true; break; }
						if ( dragged )
							continue;
						Vector3 wv = wt * editVertexLocal( s, i );
						QPointF sp;
						if ( !worldToScreen( wv, sp ) )
							continue;
						float d = float( std::hypot( sp.x() - pos.x(), sp.y() - pos.y() ) );
						if ( d < bestD ) {
							bestD = d;
							target = wv;
							haveTarget = true;
						}
					}
				}
			} else {
				// edge / face: surface raycast, then nearest point on the hit edge
				SceneRayHit sh = raycastScene( QPointF( pos ), -1 );
				if ( sh.shape ) {
					const Triangle & htri = sh.shape->triangles.at( sh.tri );
					int hitBlock = sh.shape->id();
					for ( const auto & ev : elemVerts ) {
						if ( ev.shape == hitBlock
							&& ( ev.idx == htri[0] || ev.idx == htri[1] || ev.idx == htri[2] ) ) {
							sh.shape = nullptr;
							break;
						}
					}
				}
				if ( sh.shape ) {
					Transform swt = shapeRenderTrans( sh.shape );
					target = swt * sh.hitLocal;
					const Triangle & tri = sh.shape->triangles.at( sh.tri );
					Vector3 va = editVertexLocal( sh.shape, tri[0] );
					Vector3 vb = editVertexLocal( sh.shape, tri[1] );
					Vector3 vc = editVertexLocal( sh.shape, tri[2] );
					if ( snapTargetMode == 2 ) {
						auto closest = [&sh]( const Vector3 & a, const Vector3 & b ) {
							Vector3 d = b - a;
							float len2 = d.squaredLength();
							float t = ( len2 > 1.0e-12f )
							          ? std::min( std::max( Vector3::dotproduct( sh.hitLocal - a, d ) / len2, 0.0f ), 1.0f ) : 0.0f;
							return a + d * t;
						};
						Vector3 p01 = closest( va, vb ), p12 = closest( vb, vc ), p20 = closest( vc, va );
						float d01 = ( p01 - sh.hitLocal ).squaredLength();
						float d12 = ( p12 - sh.hitLocal ).squaredLength();
						float d20 = ( p20 - sh.hitLocal ).squaredLength();
						target = swt * ( d01 <= d12 && d01 <= d20 ? p01 : ( d12 <= d20 ? p12 : p20 ) );
					}
					haveTarget = true;
				}
			}

			if ( haveTarget ) {
				// Snap Base over the dragged vertices' original positions
				Vector3 base = elemPivot;	// median (the element pivot)
				if ( !elemVerts.isEmpty() ) {
					if ( snapBase == 0 ) {
						float bd = 1.0e30f;
						for ( const auto & ev : elemVerts ) {
							float d = ( ev.origWorld - target ).squaredLength();
							if ( d < bd ) {
								bd = d;
								base = ev.origWorld;
							}
						}
					} else if ( snapBase == 1 ) {
						Vector3 mn = elemVerts.first().origWorld, mx = mn;
						for ( const auto & ev : elemVerts ) {
							for ( int c = 0; c < 3; c++ ) {
								mn[c] = std::min( mn[c], ev.origWorld[c] );
								mx[c] = std::max( mx[c], ev.origWorld[c] );
							}
						}
						base = ( mn + mx ) / 2.0f;
					} else if ( snapBase == 3 && !pickedElems.isEmpty() ) {
						base = pickedElems.constLast().worldPos;	// active element
					}
				}

				deltaWorld = target - base;
				// constrain the snap to the locked axis: move only along it so the
				// vertex's axis coordinate matches the target vertex (Blender G,X +
				// vertex snap), instead of jumping the whole way onto the target
				if ( gizmoAxis > 0 ) {
					float a = deltaWorld[gizmoAxis - 1];
					deltaWorld = Vector3();
					deltaWorld[gizmoAxis - 1] = a;
				}
				elemSnapped = true;
				snapIndicator = true;
				snapIndicatorPos = target;	// Blender: the marker sits on the snap target
			}
		}

		// grid stepping ONLY when the Grid Step target is selected. With a
		// vertex/edge/face target, an un-hit move is free (Blender), not grid-
		// snapped - otherwise snapping to nothing quietly jumps to the grid.
		if ( snap && snapTargetMode == 0 && !elemSnapped && gizmoSnapStep > 0 ) {
			for ( int c = 0; c < 3; c++ )
				deltaWorld[c] = std::round( deltaWorld[c] / gizmoSnapStep ) * gizmoSnapStep;
		}
		gizmoLastParam = deltaWorld;
		for ( auto & ev : elemVerts ) {
			if ( ev.mirrorOf >= 0 )
				continue;	// X-mirror follower: set from its source below
			previewVertex( ev, toLocal( ev.shape, ev.origWorld + deltaWorld ) );
		}
		status = tr( "Move: %1, %2, %3" ).arg( deltaWorld[0], 0, 'f', 3 ).arg( deltaWorld[1], 0, 'f', 3 ).arg( deltaWorld[2], 0, 'f', 3 );
	} else if ( gizmoMode == 2 ) {
		float angle = numeric ? gizmoPartVal( gizmoNum, 0 ) : dx * 0.5f * precision;
		if ( snap && gizmoRotSnapDeg > 0.0f )
			angle = std::round( angle / gizmoRotSnapDeg ) * gizmoRotSnapDeg;
		Vector3 axis;
		if ( gizmoAxis == 0 )
			axis = camFwd;
		else
			axis[gizmoAxis - 1] = 1.0f;
		Quat q;
		q.fromAxisAngle( axis, deg2rad( angle ) );
		Matrix dr;
		dr.fromQuat( q );
		gizmoLastParam = Vector3( angle, 0.0f, 0.0f );
		for ( auto & ev : elemVerts ) {
			if ( ev.mirrorOf >= 0 )
				continue;	// X-mirror follower
			Vector3 w = elemPivot + dr * ( ev.origWorld - elemPivot );
			previewVertex( ev, toLocal( ev.shape, w ) );
		}
		status = tr( "Rotate: %1°" ).arg( angle, 0, 'f', 1 );
	} else if ( gizmoMode == 3 ) {
		float factor = numeric ? gizmoPartVal( gizmoNum, 0 ) : 1.0f + dx * 0.01f * precision;
		if ( snap )
			factor = std::round( factor * 10.0f ) / 10.0f;
		gizmoLastParam = Vector3( factor, 0.0f, 0.0f );
		for ( auto & ev : elemVerts ) {
			if ( ev.mirrorOf >= 0 )
				continue;	// X-mirror follower
			Vector3 rel = ev.origWorld - elemPivot; if ( gizmoAxis > 0 ) rel[gizmoAxis - 1] *= factor; else rel = rel * factor; Vector3 w = elemPivot + rel;	/* axis-constrained scale (Blender S,X/Y/Z) */
			previewVertex( ev, toLocal( ev.shape, w ) );
		}
		status = tr( "Scale: ×%1" ).arg( factor, 0, 'f', 3 );
	}
	// X-mirror followers take their source's new position with local X
	// negated (raw/authored space, so deformed cages mirror sanely)
	for ( auto & ev : elemVerts ) {
		if ( ev.mirrorOf < 0 || ev.mirrorOf >= elemVerts.size() )
			continue;
		const Vector3 & srcL = elemVerts.at( ev.mirrorOf ).currentLocal;
		const Vector3 mLocal( -srcL[0], srcL[1], srcL[2] );
		Shape * shape = shapeForBlock( ev.shape );
		if ( shape && ev.idx >= 0 && ev.idx < shape->verts.size() ) {
			ev.currentLocal = mLocal;
			shape->verts[ev.idx] = mLocal;
			previewShapes.insert( shape );
		}
	}
	for ( Shape * shape : previewShapes )
		shape->clearHash();

	static const char * axisNames[4] = { "view", "X", "Y", "Z" };
	emit gizmoStatus( status + tr( "   [axis: %1]" ).arg( QLatin1String( axisNames[gizmoAxis] ) ) );
	update();
}

// value-column index of a vertex position field (BSTriShape or legacy)
static QModelIndex tlVertexValueIndex( NifModel * model, const QModelIndex & iShape, int vi )
{
	QModelIndex iv;
	QModelIndex iVData = model->getIndex( iShape, "Vertex Data" );
	if ( iVData.isValid() && vi >= 0 && vi < model->rowCount( iVData ) )
		iv = model->getIndex( model->getIndex( iVData, vi ), "Vertex" );
	if ( !iv.isValid() ) {
		QModelIndex iVerts = model->getIndex( model->getBlockIndex( model->getLink( iShape, "Data" ) ), "Vertices" );
		if ( !iVerts.isValid() )
			iVerts = model->getIndex( iShape, "Vertices" );
		if ( iVerts.isValid() && vi >= 0 && vi < model->rowCount( iVerts ) )
			iv = model->getIndex( iVerts, vi );
	}
	if ( iv.isValid() )
		return iv.sibling( iv.row(), NifModel::ValueCol );
	return QModelIndex();
}

//! Area-weighted normal accumulation for a subset of verts (current model
//! positions); shared by the direct write (tlRecalcNormalsSubset) and the
//! undo-tracked write (tlPushNormalCommands).
static QHash<int, Vector3> tlAccumulateAreaNormals( NifModel * model, const QModelIndex & iShape,
	const QSet<int> & verts )
{
	QHash<int, Vector3> acc;
	QModelIndex iVD = model->getIndex( iShape, "Vertex Data" );
	QModelIndex iTris = model->getIndex( iShape, "Triangles" );
	if ( !iVD.isValid() || !iTris.isValid() || verts.isEmpty() )
		return acc;
	// non-reporting probe: a layout without normals is normal, not a warning
	if ( !model->getItem( model->getIndex( iVD, 0 ), "Normal" ) )
		return acc;
	const int numVerts = model->get<int>( iShape, "Num Vertices" );
	const int numTris = model->get<int>( iShape, "Num Triangles" );
	QVector<Vector3> pos( numVerts );
	for ( int i = 0; i < numVerts; i++ )
		pos[i] = model->get<Vector3>( model->getIndex( iVD, i ), "Vertex" );
	for ( int v : verts )
		if ( v >= 0 && v < numVerts )
			acc.insert( v, Vector3() );
	for ( int t = 0; t < numTris; t++ ) {
		Triangle tri = model->get<Triangle>( model->getIndex( iTris, t ) );
		if ( !( acc.contains( tri[0] ) || acc.contains( tri[1] ) || acc.contains( tri[2] ) ) )
			continue;
		// unnormalized cross = area weighting (degenerate scaffolds contribute 0)
		Vector3 n = Vector3::crossproduct( pos.at( tri[1] ) - pos.at( tri[0] ),
			pos.at( tri[2] ) - pos.at( tri[0] ) );
		for ( int c = 0; c < 3; c++ ) {
			auto it = acc.find( tri[c] );
			if ( it != acc.end() )
				it.value() += n;
		}
	}
	return acc;
}

//! Recompute the given verts' normals via ChangeValueCommands, so the writes
//! join the currently open transaction (the extrude-chained move commit) and
//! undo with it — no whole-model snapshot, no reload.
static void tlPushNormalCommands( NifModel * model, const QModelIndex & iShape, const QSet<int> & verts )
{
	if ( !model || !model->undoStack )
		return;
	const QHash<int, Vector3> acc = tlAccumulateAreaNormals( model, iShape, verts );
	QModelIndex iVD = model->getIndex( iShape, "Vertex Data" );
	for ( auto it = acc.constBegin(); it != acc.constEnd(); ++it ) {
		Vector3 n = it.value();
		if ( n.squaredLength() < 1.0e-16f )
			continue;
		n.normalize();
		QModelIndex row = model->getIndex( iVD, it.key() );
		if ( !model->getItem( row, "Normal" ) )
			continue;	// tolerate an incomplete row rather than warn per vertex
		QModelIndex nIdx = model->getIndex( row, "Normal" );
		const NifItem * item = nIdx.isValid() ? static_cast<const NifItem *>( nIdx.internalPointer() ) : nullptr;
		if ( !item )
			continue;
		NifValue oldVal = item->value();
		NifValue newVal = oldVal;
		newVal.set<ByteVector3>( n, model, item );
		if ( !( oldVal == newVal ) )
			model->undoStack->push( new ChangeValueCommand( nIdx, oldVal, newVal, GLView::tr( "Normal" ), model ) );
	}
}

void GLView::gizmoEndElement( bool commit )
{
	if ( !elemTransform )
		return;
	int mode = gizmoMode;		// captured before clearing, for the redo panel
	int axis = gizmoAxis;
	elemTransform = false;
	gizmoMode = 0;
	setMouseGrabEnabled( false );	// the gesture owned the mouse + keyboard
	setKeyboardGrabEnabled( false );
	emit gizmoStatus( QString() );

	if ( model ) {
		if ( commit ) {
			// Undo via per-vertex value commands rather than a whole-model
			// snapshot: serialising the entire FO4 model on every edit is slow
			// and was corrupting some files. Each vertex already holds its new
			// value from the renderer preview; the model still contains the
			// original, so push one ChangeValueCommand per changed vertex now.
			ChangeValueCommand::createTransaction();
			for ( const ElemVert & ev : elemVerts ) {
				QModelIndex iShape = model->getBlockIndex( ev.shape );
				QModelIndex vIdx = tlVertexValueIndex( model, iShape, ev.idx );
				const NifItem * item = vIdx.isValid() ? static_cast<const NifItem *>( vIdx.internalPointer() ) : nullptr;
				if ( !item )
					continue;
				NifValue oldVal = item->value();
				NifValue newVal = oldVal;
				bool half = item->hasValueType( NifValue::tHalfVector3 );
				if ( half )
					newVal.set<HalfVector3>( HalfVector3( ev.currentLocal ), model, item );
				else
					newVal.set<Vector3>( ev.currentLocal, model, item );
				if ( !( oldVal == newVal ) )
					model->undoStack->push( new ChangeValueCommand( vIdx, oldVal, newVal, tr( "Vertex" ), model ) );
			}
			// freeze the gesture for the edit-mode operator redo panel
			lastGestureElement = true;
			lastElemMode = mode;
			lastElemAxis = axis;
			lastElemVerts = elemVerts;
			lastElemPivot = elemPivot;
			lastUndoIndex = model->undoStack ? model->undoStack->index() : -1;
			if ( extrudeChainArmed ) {
				// the move belongs to an extrude: refresh the new walls' and
				// cap's normals for the final positions inside this same
				// transaction, then arm the extrude redo panel instead of the
				// plain transform panel
				extrudeChainArmed = false;
				if ( extrudeTouchedShape >= 0 && !extrudeTouchedVerts.isEmpty() )
					tlPushNormalCommands( model, model->getBlockIndex( extrudeTouchedShape ),
						extrudeTouchedVerts );
				armExtrudeRedoPanel( ( mode == 1 ) ? gizmoLastParam : Vector3() );
			} else {
				emit transformGesture( mode, axis, gizmoLastParam );
			}
		} else {
			// cancel: the model was never touched; restore only the renderer preview
			QSet<Shape *> restored;
			for ( const ElemVert & ev : elemVerts ) {
				Shape * shape = shapeForBlock( ev.shape );
				if ( shape && ev.idx >= 0 && ev.idx < shape->verts.size() ) {
					shape->verts[ev.idx] = ev.origLocal;
					restored.insert( shape );
				}
			}
			for ( Shape * shape : restored ) shape->clearHash();
			if ( extrudeChainArmed ) {
				// Esc after E: the extrusion stays in place at zero offset
				// (Blender); the panel still lets the offset be dialed in
				extrudeChainArmed = false;
				armExtrudeRedoPanel( Vector3() );
			}
		}
	}

	elemVerts.clear();
	refreshPickedElementPositions();
	update();
}

//! Blender-style popup placement: move a modal dialog so `onCursor` (usually
//! its primary action button) opens directly under the pointer for an instant
//! click. Call after the buttons are added, before exec() — adjustSize() lays
//! the dialog out so the child geometry is valid. Clamped to the cursor's
//! screen so it never opens partly off-screen. Pass onCursor = nullptr to
//! centre the whole dialog on the cursor instead.
static void tlPlacePopupAtCursor( QWidget * box, QWidget * onCursor )
{
	if ( !box )
		return;
	// show() realizes and lays the dialog out (so the button's real screen
	// position exists) but does not paint until the event loop starts inside
	// exec() — so repositioning now is flicker-free. QMessageBox child geometry
	// is unreliable BEFORE show, hence measuring the button after it.
	box->adjustSize();
	box->show();
	const QPoint cursor = QCursor::pos();
	const QPoint anchor = ( onCursor && box->isAncestorOf( onCursor ) )
		? onCursor->mapToGlobal( onCursor->rect().center() )
		: box->frameGeometry().center();
	// shift the whole window so the anchor lands on the cursor (delta is
	// coordinate-system agnostic, sidestepping frame-margin confusion)
	box->move( box->pos() + ( cursor - anchor ) );
	// clamp back onto the cursor's screen if an edge pushed it off
	if ( QScreen * scr = QGuiApplication::screenAt( cursor ) ) {
		const QRect avail = scr->availableGeometry();
		const QRect fg = box->frameGeometry();
		QPoint adj;
		if ( fg.left() < avail.left() ) adj.setX( avail.left() - fg.left() );
		else if ( fg.right() > avail.right() ) adj.setX( avail.right() - fg.right() );
		if ( fg.top() < avail.top() ) adj.setY( avail.top() - fg.top() );
		else if ( fg.bottom() > avail.bottom() ) adj.setY( avail.bottom() - fg.bottom() );
		if ( !adj.isNull() )
			box->move( box->pos() + adj );
	}
}

void GLView::showDeleteMenu()
{
	if ( !editMode || pickedElems.isEmpty() ) {
		emit gizmoStatus( tr( "Delete needs a selection in edit mode" ) );
		return;
	}
	AutoCloseMenu m;
	m.addSection( tr( "Delete" ) );
	QAction * aVerts = m.addAction( tr( "Vertices" ) );
	QAction * aEdges = m.addAction( tr( "Edges" ) );
	QAction * aFaces = m.addAction( tr( "Faces" ) );
	m.addSeparator();
	QAction * aOnlyFaces = m.addAction( tr( "Only Faces" ) );
	QAction * r = m.exec( QCursor::pos() );
	if ( r == aVerts )
		deleteGeometry( 0 );
	else if ( r == aEdges )
		deleteGeometry( 1 );
	else if ( r == aFaces )
		deleteGeometry( 2 );
	else if ( r == aOnlyFaces )
		deleteGeometry( 3 );
}

/*
 *  Separate / Join / Duplicate (Blender P / Ctrl+J / Shift+D)
 */

//! Clone a block verbatim (all fields, incl. packed vertex data so normals are
//! preserved byte-for-byte) and append it; returns the new block's index.
static QModelIndex tlCloneBlock( NifModel * nif, const QModelIndex & iBlock )
{
	if ( !iBlock.isValid() )
		return QModelIndex();
	QByteArray data;
	QBuffer buffer( &data );
	if ( buffer.open( QIODevice::WriteOnly ) && nif->saveIndex( buffer, iBlock ) ) {
		buffer.close();
		if ( buffer.open( QIODevice::ReadOnly ) ) {
			QModelIndex nb = nif->insertNiBlock( nif->itemName( iBlock ), nif->getBlockCount() );
			// loadIndex populates every item without setting Loading state (the
			// full-file loader does), so on a high-poly block each of tens of
			// thousands of value/array writes emits a change signal and the live
			// scene reacts per write — quadratic, a multi-second freeze. Mirror
			// the real loader: suppress signals during the block build.
			nif->setState( BaseModel::Loading );
			nif->loadIndex( buffer, nb );
			nif->restoreState();
			nif->dataChanged( nb, nb );
			return nb;
		}
	}
	return QModelIndex();
}

//! Keep only the triangles of a BSTriShape for which keep(triIndex) is true;
//! rewrites Num Triangles / Triangles / Data Size (verts are left in place).
static int tlKeepTriangles( NifModel * nif, const QModelIndex & iShape, const std::function<bool( int )> & keep )
{
	QModelIndex iTris = nif->getIndex( iShape, "Triangles" );
	if ( !iTris.isValid() || !nif->getIndex( iShape, "Num Triangles" ).isValid() )
		return 0;
	int numTris = nif->get<int>( iShape, "Num Triangles" );
	int numVerts = nif->get<int>( iShape, "Num Vertices" );
	int dataSize = nif->get<int>( iShape, "Data Size" );
	int stride = ( numVerts > 0 ) ? ( dataSize - numTris * 6 ) / numVerts : 0;
	QVector<Triangle> kept;
	for ( int t = 0; t < numTris && t < nif->rowCount( iTris ); t++ ) {
		Triangle tri = nif->get<Triangle>( nif->getIndex( iTris, t ) );
		if ( keep( t ) )
			kept.append( tri );
	}
	// suppress the per-write dataChanged storm: a large kept-triangle rewrite
	// otherwise makes every live view (edit overlay, block list) react per
	// write, which is quadratic and freezes on high-poly shapes
	nif->setState( BaseModel::Processing );
	nif->set<int>( iShape, "Num Triangles", kept.size() );
	nif->updateArraySize( iTris );
	for ( int t = 0; t < kept.size(); t++ )
		nif->set<Triangle>( nif->getIndex( iTris, t ), kept[t] );
	if ( stride > 0 )
		nif->set<int>( iShape, "Data Size", numVerts * stride + kept.size() * 6 );
	nif->restoreState();
	nif->dataChanged( iShape, iShape );
	return kept.size();
}

//! Clone the block plus its shader / alpha property so a split-off mesh does
//! not share a property block with the original; returns the new block number.
static int tlCloneShapeWithProps( NifModel * nif, int srcBlock )
{
	QModelIndex iNew = tlCloneBlock( nif, nif->getBlockIndex( srcBlock ) );
	if ( !iNew.isValid() )
		return -1;
	int nNew = nif->getBlockNumber( iNew );
	for ( const char * prop : { "Shader Property", "Alpha Property" } ) {
		int ref = nif->getLink( nif->getBlockIndex( srcBlock ), prop );
		if ( ref < 0 )
			continue;
		QModelIndex ic = tlCloneBlock( nif, nif->getBlockIndex( ref ) );
		if ( ic.isValid() )
			nif->setLink( nif->getBlockIndex( nNew ), prop, nif->getBlockNumber( ic ) );
	}
	return nNew;
}

//! Blender-style unique name: strip any trailing ".NNN" to a stem, then return
//! stem.001 / .002 ... using the lowest number not already a node name.
static QString tlUniqueNodeName( NifModel * nif, const QString & name )
{
	QString stem = name;
	int dot = name.lastIndexOf( QLatin1Char( '.' ) );
	if ( dot > 0 && name.size() - dot - 1 >= 3 ) {
		bool allDigits = true;
		for ( int i = dot + 1; i < name.size(); i++ ) {
			if ( !name.at( i ).isDigit() ) {
				allDigits = false;
				break;
			}
		}
		if ( allDigits )
			stem = name.left( dot );
	}

	QSet<QString> used;
	for ( int b = 0; b < nif->getBlockCount(); b++ ) {
		QModelIndex ib = nif->getBlockIndex( b );
		if ( nif->getIndex( ib, "Name" ).isValid() )
			used.insert( nif->get<QString>( ib, "Name" ) );
	}
	for ( int n = 1; n < 100000; n++ ) {
		QString cand = stem + QString::asprintf( ".%03d", n );
		if ( !used.contains( cand ) )
			return cand;
	}
	return stem + QStringLiteral( ".001" );
}

void GLView::showSeparateMenu()
{
	if ( !editMode || pickedElems.isEmpty() ) {
		emit gizmoStatus( tr( "Separate needs a selection in edit mode" ) );
		return;
	}
	AutoCloseMenu m;
	m.addSection( tr( "Separate" ) );
	QAction * aSel = m.addAction( tr( "Selection" ) );
	QAction * aMat = m.addAction( tr( "By Material" ) );
	QAction * aLoose = m.addAction( tr( "By Loose Parts" ) );
	aMat->setEnabled( false );		// not implemented yet (Blender parity later)
	aLoose->setEnabled( false );
	QAction * r = m.exec( QCursor::pos() );
	if ( r == aSel )
		separateSelection();
}

//! Recursively snapshot every leaf value of an item subtree (order matches
//! tlRestoreValues' traversal). Generic — captures any field layout exactly.
static void tlCaptureValues( NifModel * nif, const QModelIndex & idx, QVector<NifValue> & out )
{
	const int rc = nif->rowCount( idx );
	if ( rc > 0 ) {
		for ( int r = 0; r < rc; r++ )
			tlCaptureValues( nif, nif->getIndex( idx, r ), out );
	} else {
		out.append( nif->getValue( idx ) );
	}
}

static void tlRestoreValues( NifModel * nif, const QModelIndex & idx, const QVector<NifValue> & in, int & pos )
{
	const int rc = nif->rowCount( idx );
	if ( rc > 0 ) {
		for ( int r = 0; r < rc; r++ )
			tlRestoreValues( nif, nif->getIndex( idx, r ), in, pos );
	} else if ( pos < in.size() ) {
		nif->setIndexValue( idx, in.at( pos++ ) );
	}
}

//! In-place undo for arbitrary single-shape topology rewrites (Loop Cut,
//! Subdivide, Dissolve, Symmetrize — anything that rewrites triangles or
//! removes verts): the constructor snapshots the shape's counts + the whole
//! Vertex Data and Triangles subtrees as typed values; undo restores them
//! exactly. Heavier than the targeted commands (a few MB / ~0.5 s on big
//! meshes) but still no model reload, no flash.
class TlShapeStateCommand final : public QUndoCommand
{
public:
	TlShapeStateCommand( NifModel * model, const QModelIndex & iShape,
		const QString & text, std::function<void()> applyFn )
		: nif( model ), block( iShape ), apply( std::move( applyFn ) )
	{
		setText( text );
		oldNV = nif->get<int>( iShape, "Num Vertices" );
		oldNT = nif->get<int>( iShape, "Num Triangles" );
		oldDataSize = nif->get<int>( iShape, "Data Size" );
		QModelIndex iBound = nif->getIndex( iShape, "Bounding Sphere" );
		if ( iBound.isValid() ) {
			boundCenter = nif->get<Vector3>( iBound, "Center" );
			boundRadius = nif->get<float>( iBound, "Radius" );
		}
		tlCaptureValues( nif, nif->getIndex( iShape, "Vertex Data" ), vertexState );
		tlCaptureValues( nif, nif->getIndex( iShape, "Triangles" ), triState );
	}

	void redo() override
	{
		if ( QModelIndex( block ).isValid() && apply )
			apply();
	}

	void undo() override
	{
		QModelIndex iShape( block );
		if ( !iShape.isValid() )
			return;
		nif->setState( BaseModel::Processing );
		QModelIndex iVD = nif->getIndex( iShape, "Vertex Data" );
		QModelIndex iTris = nif->getIndex( iShape, "Triangles" );
		nif->set<int>( iShape, "Num Vertices", oldNV );
		nif->updateArraySize( iVD );
		nif->set<int>( iShape, "Num Triangles", oldNT );
		nif->updateArraySize( iTris );
		int pos = 0;
		tlRestoreValues( nif, iVD, vertexState, pos );
		pos = 0;
		tlRestoreValues( nif, iTris, triState, pos );
		nif->set<int>( iShape, "Data Size", oldDataSize );
		QModelIndex iBound = nif->getIndex( iShape, "Bounding Sphere" );
		if ( iBound.isValid() ) {
			nif->set<Vector3>( iBound, "Center", boundCenter );
			nif->set<float>( iBound, "Radius", boundRadius );
		}
		nif->restoreState();
		nif->dataChanged( iShape, iShape );
	}

private:
	NifModel * nif;
	QPersistentModelIndex block;
	std::function<void()> apply;
	int oldNV = 0, oldNT = 0, oldDataSize = 0;
	Vector3 boundCenter;
	float boundRadius = 0.0f;
	QVector<NifValue> vertexState, triState;
};

//! Remove ONE null (-1) entry from a node's Children array, scanning from the
//! end — removeNiBlock turns our appended child link into a null ref, and
//! addLink would append another slot on redo instead of reusing it. Returns
//! true if a null entry was removed (callers deleting several children of one
//! parent loop until it returns false).
static bool tlRemoveNullChildLink( NifModel * nif, int parentBlock )
{
	QModelIndex iParent = nif->getBlockIndex( parentBlock );
	QModelIndex iSize = nif->getIndex( iParent, "Num Children" );
	QModelIndex iArray = nif->getIndex( iParent, "Children" );
	if ( !iSize.isValid() || !iArray.isValid() )
		return false;
	const int n = nif->get<int>( iSize );
	for ( int c = n - 1; c >= 0; c-- ) {
		if ( nif->getLink( nif->getIndex( iArray, c ) ) != -1 )
			continue;
		for ( int j = c; j < n - 1; j++ )
			nif->setLink( nif->getIndex( iArray, j ),
				nif->getLink( nif->getIndex( iArray, j + 1 ) ) );
		nif->set<int>( iSize, n - 1 );
		nif->updateArraySize( iArray );
		return true;
	}
	return false;
}

//! In-place undo for operators that only APPEND new blocks (object-mode
//! Duplicate, Separate's clone, Add Primitive): redo runs the creation
//! closure and records the appended block range; undo removes those blocks
//! (highest first) and prunes the null child links the removal leaves in the
//! recorded parents. NifModel appends blocks deterministically, so a later
//! redo recreates the same numbers.
class TlBlockAppendCommand final : public QUndoCommand
{
public:
	TlBlockAppendCommand( NifModel * model, const QString & text,
		std::function<void()> createFn,
		std::shared_ptr<QVector<int>> linkedParentsOut = nullptr )
		: nif( model ), create( std::move( createFn ) ),
		linkedParents( std::move( linkedParentsOut ) )
	{
		setText( text );
	}
	void redo() override
	{
		base = nif->getBlockCount();
		if ( linkedParents )
			linkedParents->clear();
		if ( create )
			create();
		made = nif->getBlockCount() - base;
	}
	void undo() override
	{
		for ( int b = base + made - 1; b >= base; b-- )
			if ( b < nif->getBlockCount() )
				nif->removeNiBlock( b );
		if ( linkedParents )
			for ( int p : std::as_const( *linkedParents ) )
				tlRemoveNullChildLink( nif, p );
	}
private:
	NifModel * nif;
	std::function<void()> create;
	std::shared_ptr<QVector<int>> linkedParents;
	int base = 0, made = 0;
};

void GLView::separateSelection()
{
	if ( !model || !editMode || pickedElems.isEmpty() )
		return;

	auto edgeKey = []( int a, int b ) {
		return ( quint64( std::min( a, b ) ) << 32 ) | quint64( std::max( a, b ) );
	};

	QHash<int, QSet<int>> selVerts = pickedVertexRefs();
	QHash<int, QSet<int>> selFaces;
	for ( const auto & pe : pickedElems )
		if ( pe.type == 3 && pe.shapeBlock >= 0 )
			selFaces[pe.shapeBlock].insert( pe.e0 );

	QSet<int> shapeSet;
	for ( int k : selVerts.keys() )
		shapeSet.insert( k );
	for ( int k : selFaces.keys() )
		shapeSet.insert( k );
	Q_UNUSED( edgeKey );

	// in-place undo per shape: a pure state-capture command (empty apply)
	// snapshots the source arrays, then the append command runs the actual
	// separate — undo removes the clone (+ its null child link) and restores
	// the source. Everything precomputed so redo closures are deterministic.
	const bool inPlaceU = ( model->undoStack != nullptr );
	auto movedCount = std::make_shared<int>( 0 );
	auto lastNew = std::make_shared<int>( -1 );
	if ( inPlaceU )
		model->undoStack->beginMacro( tr( "Separate selection" ) );
	for ( int sb : shapeSet ) {
		QModelIndex iShape = model->getBlockIndex( sb );
		if ( !model->blockInherits( iShape, "BSTriShape" ) )
			continue;
		QModelIndex iTris = model->getIndex( iShape, "Triangles" );
		if ( !iTris.isValid() )
			continue;
		int numTris = model->get<int>( iShape, "Num Triangles" );
		const QSet<int> & sv = selVerts.value( sb );
		const QSet<int> & sf = selFaces.value( sb );

		QVector<bool> sep( numTris, false );
		int sepCount = 0;
		for ( int t = 0; t < numTris && t < model->rowCount( iTris ); t++ ) {
			Triangle tri = model->get<Triangle>( model->getIndex( iTris, t ) );
			bool s = sf.contains( t )
				|| ( !sv.isEmpty() && sv.contains( tri[0] ) && sv.contains( tri[1] ) && sv.contains( tri[2] ) );
			sep[t] = s;
			if ( s )
				sepCount++;
		}
		if ( sepCount == 0 || sepCount == numTris )
			continue;	// nothing to split, or everything (Blender no-ops)

		auto parents = std::make_shared<QVector<int>>();
		auto applySep = [this, sb, sep, sepCount, parents, movedCount, lastNew]() {
			int nNew = tlCloneShapeWithProps( model, sb );
			if ( nNew < 0 )
				return;
			int parentNum = model->getParent( sb );
			if ( parentNum >= 0 ) {
				blockLink( model, model->getBlockIndex( parentNum ), model->getBlockIndex( nNew ) );
				parents->append( parentNum );
			}
			QString nm = model->get<QString>( model->getBlockIndex( sb ), "Name" );
			model->set<QString>( model->getBlockIndex( nNew ), "Name", tlUniqueNodeName( model, nm ) );
			// new keeps the separated triangles; original keeps the rest
			tlKeepTriangles( model, model->getBlockIndex( nNew ), [&]( int t ) { return sep[t]; } );
			tlKeepTriangles( model, model->getBlockIndex( sb ), [&]( int t ) { return !sep[t]; } );
			*movedCount += sepCount;
			*lastNew = nNew;
		};
		if ( inPlaceU ) {
			model->undoStack->push( new TlShapeStateCommand( model, iShape,
				tr( "Separate selection" ), std::function<void()>() ) );
			model->undoStack->push( new TlBlockAppendCommand( model,
				tr( "Separate selection" ), applySep, parents ) );
		} else {
			applySep();
		}
	}
	if ( inPlaceU )
		model->undoStack->endMacro();
	int totalMoved = *movedCount;
	int selectBlock = *lastNew;

	if ( totalMoved == 0 ) {
		emit gizmoStatus( tr( "Nothing to separate (select part of the mesh)" ) );
		return;
	}

	setEditMode( false );
	pickedElems.clear();
	if ( selectBlock >= 0 ) {
		syncObjectSelection( selectBlock );
		emit clicked( model->getBlockIndex( selectBlock ) );
	}
	emit gizmoStatus( tr( "Separated %1 triangle(s) into a new object" ).arg( totalMoved ) );
	modelChanged();
}

void GLView::duplicateSelection()
{
	if ( editMode ) {
		duplicateElements();
		return;
	}
	if ( !model || objSelection.isEmpty() )
		return;

	// in-place undo: the duplicate only APPENDS blocks (shape + property
	// clones); undo removes them and prunes the null child links the removal
	// leaves behind — no model reload, no flash
	const QSet<int> sel = objSelection;	// the closure's own copy: same
										// iteration order on every redo
	auto parents = std::make_shared<QVector<int>>();
	auto madeShapes = std::make_shared<QVector<int>>();
	auto createDup = [this, sel, parents, madeShapes]() {
		madeShapes->clear();
		for ( int sb : sel ) {
			QModelIndex iS = model->getBlockIndex( sb );
			if ( !model->blockInherits( iS, "BSTriShape" ) )
				continue;	// v1: geometry duplicate (branch duplicate is future)
			int nNew = tlCloneShapeWithProps( model, sb );
			if ( nNew < 0 )
				continue;
			int parentNum = model->getParent( sb );
			if ( parentNum >= 0 ) {
				blockLink( model, model->getBlockIndex( parentNum ), model->getBlockIndex( nNew ) );
				parents->append( parentNum );
			}
			QString nm = model->get<QString>( model->getBlockIndex( sb ), "Name" );
			model->set<QString>( model->getBlockIndex( nNew ), "Name", tlUniqueNodeName( model, nm ) );
			madeShapes->append( nNew );
		}
	};
	if ( model->undoStack )
		model->undoStack->push( new TlBlockAppendCommand( model, tr( "Duplicate" ), createDup, parents ) );
	else
		createDup();
	QVector<int> newBlocks = *madeShapes;

	if ( newBlocks.isEmpty() ) {
		emit gizmoStatus( tr( "Nothing to duplicate (select a mesh)" ) );
		return;
	}

	// select the duplicates and immediately start a move gesture (Blender):
	// cancelling the move leaves the copies at the original position, selected
	objSelection.clear();
	for ( int b : newBlocks )
		objSelection.insert( b );
	objActive = newBlocks.last();
	scene->currentBlock = model->getBlockIndex( objActive );
	scene->currentIndex = QModelIndex( scene->currentBlock );
	emit objectSelectionChanged();
	modelChanged();
	gizmoBegin( 1 );
	emit gizmoStatus( tr( "Duplicated %1 mesh(es) - move, or Esc to leave in place" ).arg( newBlocks.size() ) );
}

//! Recursively copy every leaf value from one item subtree to another with an
//! identical structure (used to append a vertex-data element verbatim).
static void tlCopyItemValues( NifModel * nif, const QModelIndex & src, const QModelIndex & dst )
{
	int rc = nif->rowCount( src );
	if ( rc > 0 && rc == nif->rowCount( dst ) ) {
		for ( int r = 0; r < rc; r++ )
			tlCopyItemValues( nif, nif->getIndex( src, r ), nif->getIndex( dst, r ) );
	} else {
		nif->setIndexValue( dst, nif->getValue( src ) );
	}
}

//! Is this transform close enough to identity that appending verts verbatim
//! (no transform) is exact? (the seamless separate->join round-trip case)
static bool tlNearIdentity( const Transform & t )
{
	if ( t.translation.length() > 1.0e-4f || std::fabs( t.scale - 1.0f ) > 1.0e-4f )
		return false;
	for ( int i = 0; i < 3; i++ )
		for ( int j = 0; j < 3; j++ )
			if ( std::fabs( t.rotation( i, j ) - ( i == j ? 1.0f : 0.0f ) ) > 1.0e-4f )
				return false;
	return true;
}

//! Recompute a BSTriShape's bounding sphere from its vertex positions.
static void tlUpdateBounds( NifModel * nif, const QModelIndex & iShape )
{
	QModelIndex iBound = nif->getIndex( iShape, "Bounding Sphere" );
	QModelIndex iVD = nif->getIndex( iShape, "Vertex Data" );
	if ( !iBound.isValid() || !iVD.isValid() )
		return;
	int nv = nif->rowCount( iVD );
	if ( nv <= 0 )
		return;
	Vector3 mn, mx;
	for ( int i = 0; i < nv; i++ ) {
		Vector3 v = nif->get<Vector3>( nif->getIndex( iVD, i ), "Vertex" );
		if ( i == 0 ) {
			mn = mx = v;
		} else {
			for ( int c = 0; c < 3; c++ ) {
				mn[c] = std::min( mn[c], v[c] );
				mx[c] = std::max( mx[c], v[c] );
			}
		}
	}
	Vector3 c = ( mn + mx ) * 0.5f;
	float r = 0.0f;
	for ( int i = 0; i < nv; i++ )
		r = std::max( r, ( nif->get<Vector3>( nif->getIndex( iVD, i ), "Vertex" ) - c ).length() );
	nif->set<Vector3>( iBound, "Center", c );
	nif->set<float>( iBound, "Radius", r );
}

//! Blender-style delete on one shape. V = selected vertices (edge/face picks
//! contribute their corners), F = explicitly face-picked triangle indices.
//! mode: 0 Vertices, 1 Edges, 2 Faces, 3 Only Faces. For a BSTriShape the packed
//! vertex array is compacted and the triangles reindexed (Faces removes verts
//! left orphaned, like Blender); legacy NiTriShapeData only drops triangles.
static void tlDeleteGeometry( NifModel * nif, const QModelIndex & iShape,
                              const QSet<int> & V, const QSet<int> & F, int mode,
                              int & removedTris, int & removedVerts,
                              QVector<int> & outRemap, bool & outVertsRemoved )
{
	// triangle source: BSTriShape stores them inline, legacy in the Data block
	QModelIndex iTris = nif->getIndex( iShape, "Triangles" );
	bool bsShape = iTris.isValid() && nif->getIndex( iShape, "Num Triangles" ).isValid();
	QModelIndex iTrisArr = iTris;
	QModelIndex iCountBlock = iShape;
	if ( !bsShape ) {
		QModelIndex iData = nif->getBlockIndex( nif->getLink( iShape, "Data" ) );
		iTrisArr = nif->getIndex( iData, "Triangles" );
		iCountBlock = iData;
		if ( !iTrisArr.isValid() )
			return;
	}

	int numTris = std::min( nif->get<int>( iCountBlock, "Num Triangles" ), nif->rowCount( iTrisArr ) );
	QVector<Triangle> tris;
	tris.reserve( numTris );
	for ( int t = 0; t < numTris; t++ )
		tris.append( nif->get<Triangle>( nif->getIndex( iTrisArr, t ) ) );

	auto cornersIn = [&]( const Triangle & tri ) {
		return int( V.contains( tri[0] ) ) + int( V.contains( tri[1] ) ) + int( V.contains( tri[2] ) );
	};

	QVector<Triangle> keptTris;
	for ( int t = 0; t < numTris; t++ ) {
		const Triangle & tri = tris.at( t );
		bool remove;
		switch ( mode ) {
		case 0:	remove = cornersIn( tri ) > 0;  break;	// Vertices: any corner
		case 1:	remove = cornersIn( tri ) >= 2; break;	// Edges: a selected edge
		default:	remove = F.contains( t ) || cornersIn( tri ) == 3; break;	// Faces
		}
		if ( !remove )
			keptTris.append( tri );
	}
	removedTris += numTris - keptTris.size();

	bool removeLoose = ( mode == 2 );	// Faces removes orphaned verts (Blender)
	const QSet<int> & explicitDrop = ( mode == 0 ) ? V : QSet<int>();

	QModelIndex iVD = nif->getIndex( iShape, "Vertex Data" );
	if ( bsShape && iVD.isValid() ) {
		int numVerts = nif->get<int>( iShape, "Num Vertices" );
		int dataSize = nif->get<int>( iShape, "Data Size" );
		int stride = ( numVerts > 0 ) ? ( dataSize - numTris * 6 ) / numVerts : 0;

		QSet<int> used;
		for ( const Triangle & tri : keptTris ) {
			used.insert( tri[0] ); used.insert( tri[1] ); used.insert( tri[2] );
		}
		QVector<int> remap( numVerts, -1 );
		int newN = 0;
		for ( int v = 0; v < numVerts; v++ ) {
			if ( explicitDrop.contains( v ) )
				continue;
			if ( removeLoose && !used.contains( v ) )
				continue;
			remap[v] = newN++;
		}
		removedVerts += numVerts - newN;
		outRemap = remap;
		outVertsRemoved = ( newN != numVerts );

		if ( newN != numVerts ) {
			// forward compaction: the new index is always <= the old one, so
			// copying in increasing order never clobbers a slot still to be read
			for ( int v = 0; v < numVerts; v++ ) {
				int j = remap[v];
				if ( j >= 0 && j != v )
					tlCopyItemValues( nif, nif->getIndex( iVD, v ), nif->getIndex( iVD, j ) );
			}
			nif->set<int>( iShape, "Num Vertices", newN );
			nif->updateArraySize( iVD );
		}

		nif->set<int>( iShape, "Num Triangles", keptTris.size() );
		nif->updateArraySize( iTrisArr );
		for ( int t = 0; t < keptTris.size(); t++ ) {
			const Triangle & tri = keptTris.at( t );
			int a = ( remap[tri[0]] < 0 ) ? 0 : remap[tri[0]];
			int b = ( remap[tri[1]] < 0 ) ? 0 : remap[tri[1]];
			int c = ( remap[tri[2]] < 0 ) ? 0 : remap[tri[2]];
			nif->set<Triangle>( nif->getIndex( iTrisArr, t ), Triangle( a, b, c ) );
		}
		if ( stride > 0 )
			nif->set<int>( iShape, "Data Size", newN * stride + keptTris.size() * 6 );

		tlUpdateBounds( nif, iShape );
	} else {
		// legacy NiTriShapeData: drop triangles only, leave verts in place
		nif->set<int>( iCountBlock, "Num Triangles", keptTris.size() );
		if ( nif->getIndex( iCountBlock, "Num Triangle Points" ).isValid() )
			nif->set<int>( iCountBlock, "Num Triangle Points", keptTris.size() * 3 );
		nif->updateArraySize( iTrisArr );
		for ( int t = 0; t < keptTris.size(); t++ )
			nif->set<Triangle>( nif->getIndex( iTrisArr, t ), keptTris.at( t ) );
	}
}

//! Keep a shape's skin data consistent after a delete (mirrors NifSkope's own
//! spRemoveWasteVertices): remap NiSkinData bone vertex-weight indices through
//! the vertex remap, and return the block number of a now-stale NiSkinPartition
//! for the caller to remove (or -1). FO4 BSSkin::Instance carries neither a
//! NiSkinData nor a NiSkinPartition (skin weights are inline in the vertex data,
//! already compacted), so this no-ops there.
static int tlSkinResync( NifModel * nif, const QModelIndex & iShape,
                         const QVector<int> & remap, bool vertsRemoved )
{
	QModelIndex iSkinInst = nif->getBlockIndex( nif->getLink( iShape, "Skin Instance" ) );
	if ( !iSkinInst.isValid() )
		iSkinInst = nif->getBlockIndex( nif->getLink( iShape, "Skin" ) );
	QModelIndex iSkinData = nif->getBlockIndex( nif->getLink( iSkinInst, "Data" ), "NiSkinData" );

	// NiSkinData: per-bone vertex-weight lists index shape vertices; drop weights
	// for deleted verts and reindex the survivors through the remap
	if ( vertsRemoved && iSkinData.isValid() ) {
		QModelIndex iBones = nif->getIndex( iSkinData, "Bone List" );
		for ( int b = 0; b < nif->rowCount( iBones ); b++ ) {
			QModelIndex iBone = nif->getIndex( iBones, b );
			QModelIndex iWeights = nif->getIndex( iBone, "Vertex Weights" );
			QVector<QPair<int, float>> kept;
			for ( int w = 0; w < nif->rowCount( iWeights ); w++ ) {
				QModelIndex iw = nif->getIndex( iWeights, w );
				int idx = nif->get<int>( iw, "Index" );
				if ( idx >= 0 && idx < remap.size() && remap[idx] >= 0 )
					kept.append( qMakePair( remap[idx], nif->get<float>( iw, "Weight" ) ) );
			}
			nif->set<int>( iBone, "Num Vertices", kept.size() );
			nif->updateArraySize( iWeights );
			for ( int w = 0; w < kept.size(); w++ ) {
				nif->set<int>( nif->getIndex( iWeights, w ), "Index", kept[w].first );
				nif->set<float>( nif->getIndex( iWeights, w ), "Weight", kept[w].second );
			}
		}
	}

	// NiSkinPartition: its vertex maps + triangles are now stale. Rebuilding one
	// correctly is non-trivial, so (like spRemoveWasteVertices) drop it and let
	// the user regenerate with the "Make Skin Partition" spell.
	QModelIndex iSkinPart = nif->getBlockIndex( nif->getLink( iSkinInst, "Skin Partition" ), "NiSkinPartition" );
	if ( !iSkinPart.isValid() )
		iSkinPart = nif->getBlockIndex( nif->getLink( iSkinData, "Skin Partition" ), "NiSkinPartition" );
	return iSkinPart.isValid() ? nif->getBlockNumber( iSkinPart ) : -1;
}

void GLView::deleteGeometry( int mode )
{
	if ( !model || pickedElems.isEmpty() )
		return;

	QHash<int, QSet<int>> vertsByShape = pickedVertexRefs();
	QHash<int, QSet<int>> faceTris;
	for ( const auto & pe : pickedElems )
		if ( pe.type == 3 && pe.shapeBlock >= 0 )
			faceTris[pe.shapeBlock].insert( pe.e0 );

	QSet<int> shapes;
	for ( int k : vertsByShape.keys() )
		shapes.insert( k );
	for ( int k : faceTris.keys() )
		shapes.insert( k );
	if ( shapes.isEmpty() )
		return;

	int removedTris = 0, removedVerts = 0;
	QVector<int> partitions;

	// in-place undo whenever no shape carries legacy skin blocks: for pure
	// FO4 BSTriShapes tlSkinResync provably no-ops (skin is inline in Vertex
	// Data) and no block is removed, so a per-shape TlShapeStateCommand
	// restores everything — no model reload, no flash on Ctrl+Z
	bool inPlace = ( model->undoStack != nullptr );
	for ( int sb : shapes ) {
		QModelIndex iShape = model->getBlockIndex( sb );
		if ( !model->blockInherits( iShape, "BSTriShape" ) ) {
			inPlace = false;
			break;
		}
		QModelIndex iSI = model->getBlockIndex( model->getLink( iShape, "Skin Instance" ) );
		if ( !iSI.isValid() )
			iSI = model->getBlockIndex( model->getLink( iShape, "Skin" ) );
		if ( iSI.isValid()
			&& ( model->getBlockIndex( model->getLink( iSI, "Data" ), "NiSkinData" ).isValid()
				|| model->getBlockIndex( model->getLink( iSI, "Skin Partition" ), "NiSkinPartition" ).isValid() ) ) {
			inPlace = false;
			break;
		}
	}

	if ( inPlace ) {
		auto remT = std::make_shared<int>( 0 );
		auto remV = std::make_shared<int>( 0 );
		model->undoStack->beginMacro( tr( "Delete" ) );
		for ( int sb : shapes ) {
			QModelIndex iShape = model->getBlockIndex( sb );
			const QSet<int> sv = vertsByShape.value( sb );
			const QSet<int> sf = faceTris.value( sb );
			const QPersistentModelIndex pShape( iShape );
			auto applyDel = [this, pShape, sv, sf, mode, remT, remV]() {
				QModelIndex iS( pShape );
				if ( !iS.isValid() )
					return;
				// Processing suppresses the per-leaf dataChanged storm (the
				// compaction is thousands of value writes)
				model->setState( BaseModel::Processing );
				QVector<int> remap;
				bool vertsRemoved = false;
				tlDeleteGeometry( model, iS, sv, sf, mode, *remT, *remV, remap, vertsRemoved );
				model->restoreState();
				model->dataChanged( QModelIndex( iS ), QModelIndex( iS ) );
			};
			model->undoStack->push(
				new TlShapeStateCommand( model, iShape, tr( "Delete" ), applyDel ) );
		}
		model->undoStack->endMacro();
		removedTris = *remT;
		removedVerts = *remV;
	} else
	nifSnapshotOp( model, tr( "Delete" ), [&]() {
		// Processing suppresses the per-leaf dataChanged storm: compacting the
		// packed vertex array is thousands of value writes, and live views
		// (UV editor reload, Block Details) reacting to EVERY one froze the
		// app for ~10 s per deleted vertex on a 3k-vert mesh. One dataChanged
		// per shape at the end tells them everything they need.
		model->setState( BaseModel::Processing );
		for ( int sb : shapes ) {
			QModelIndex iShape = model->getBlockIndex( sb );
			QVector<int> remap;
			bool vertsRemoved = false;
			tlDeleteGeometry( model, iShape, vertsByShape.value( sb ), faceTris.value( sb ),
			                  mode, removedTris, removedVerts, remap, vertsRemoved );
			int part = tlSkinResync( model, iShape, remap, vertsRemoved );
			if ( part >= 0 && !partitions.contains( part ) )
				partitions.append( part );
		}
		model->restoreState();
		for ( int sb : shapes ) {
			QModelIndex iS = model->getBlockIndex( sb );
			if ( iS.isValid() )
				model->dataChanged( iS, iS );
		}
		// remove now-stale skin partitions last, highest block number first so
		// the block numbers still in flight above don't shift under us
		std::sort( partitions.begin(), partitions.end(), std::greater<int>() );
		for ( int p : partitions )
			model->removeNiBlock( p );
	} );

	QString msg = ( removedVerts > 0 )
		? tr( "Deleted %1 vertices, %2 triangles" ).arg( removedVerts ).arg( removedTris )
		: tr( "Deleted %1 triangles" ).arg( removedTris );
	if ( !partitions.isEmpty() )
		msg += tr( " — stale skin partition removed (regenerate with Make Skin Partition)" );
	emit gizmoStatus( msg );
	pickedElems.clear();
	modelChanged();
}

//! union-find root with path compression
static int tlFindRoot( QVector<int> & p, int x )
{
	while ( p[x] != x ) {
		p[x] = p[p[x]];
		x = p[x];
	}
	return x;
}

void GLView::showMergeMenu()
{
	if ( !editMode || pickedElems.isEmpty() ) {
		emit gizmoStatus( tr( "Merge needs a vertex selection in edit mode" ) );
		return;
	}
	AutoCloseMenu m;
	m.addSection( tr( "Merge" ) );
	QAction * aCenter = m.addAction( tr( "At Center" ) );
	QAction * aCursor = m.addAction( tr( "At Cursor" ) );
	m.addSeparator();
	QAction * aDist = m.addAction( tr( "By Distance…" ) );
	QAction * r = m.exec( QCursor::pos() );
	if ( r == aCenter )
		mergeVertices( 0 );
	else if ( r == aCursor )
		mergeVertices( 1 );
	else if ( r == aDist )
		// merge tiny by default; the redo panel is where you dial the distance up
		mergeVertices( 2, lastMergeDistance );
}

//! Per-shape merge body shared by the in-place and snapshot undo paths.
//! haveTarget/targetLocal is the precomputed local-space cursor position for
//! At Cursor mode (resolved at arm time so a later redo stays deterministic).
static void tlMergeShapeVerts( NifModel * model, const QModelIndex & iShape,
	const QVector<int> & selList, int mode, float threshold,
	bool haveTarget, const Vector3 & targetLocal, int & mergedVerts, int & removedTris )
{
	QModelIndex iVD = model->getIndex( iShape, "Vertex Data" );
	QModelIndex iTris = model->getIndex( iShape, "Triangles" );
	if ( !iVD.isValid() || !iTris.isValid() || selList.size() < 1 )
		return;	// BSTriShape only (legacy meshes not welded here)
	int numVerts = model->get<int>( iShape, "Num Vertices" );
	int numTris = model->get<int>( iShape, "Num Triangles" );
	int dataSize = model->get<int>( iShape, "Data Size" );
	int stride = ( numVerts > 0 ) ? ( dataSize - numTris * 6 ) / numVerts : 0;
	auto localPos = [&]( int i ) { return model->get<Vector3>( model->getIndex( iVD, i ), "Vertex" ); };

	// union-find over the selected verts: which ones weld together
	QVector<int> parent( numVerts );
	for ( int i = 0; i < numVerts; i++ )
		parent[i] = i;

	if ( mode == 0 || mode == 1 ) {
		// At Center / At Cursor: all selected verts collapse to one
		int rep = selList[0];
		for ( int i : selList )
			rep = std::min( rep, i );
		for ( int i : selList )
			parent[i] = rep;
		Vector3 np;
		if ( haveTarget ) {
			np = targetLocal;
		} else {
			for ( int i : selList )
				np += localPos( i );
			np = np / float( selList.size() );
		}
		tlSetVertexLocal( model, iShape, rep, np );
	} else {
		// By Distance: union verts within the threshold, weld each group
		// to its average position
		for ( int a = 0; a < selList.size(); a++ )
			for ( int b = a + 1; b < selList.size(); b++ ) {
				if ( ( localPos( selList[a] ) - localPos( selList[b] ) ).length() <= threshold ) {
					int ra = tlFindRoot( parent, selList[a] ), rb = tlFindRoot( parent, selList[b] );
					if ( ra != rb )
						parent[std::max( ra, rb )] = std::min( ra, rb );
				}
			}
		QHash<int, QVector<int>> groups;
		for ( int i : selList )
			groups[tlFindRoot( parent, i )].append( i );
		for ( auto g = groups.constBegin(); g != groups.constEnd(); ++g ) {
			Vector3 avg;
			for ( int i : g.value() )
				avg += localPos( i );
			avg = avg / float( g.value().size() );
			tlSetVertexLocal( model, iShape, g.key(), avg );
		}
	}

	// weld[i] = the surviving vertex i collapses into
	QVector<int> remap( numVerts, -1 );
	int newN = 0;
	for ( int i = 0; i < numVerts; i++ )
		if ( tlFindRoot( parent, i ) == i )
			remap[i] = newN++;			// this vertex survives
	for ( int i = 0; i < numVerts; i++ )
		remap[i] = remap[tlFindRoot( parent, i )];
	mergedVerts += numVerts - newN;

	// compact the vertex array (forward copy: survivor new index <= old)
	if ( newN != numVerts ) {
		for ( int i = 0; i < numVerts; i++ ) {
			int j = remap[i];
			if ( tlFindRoot( parent, i ) == i && j != i )
				tlCopyItemValues( model, model->getIndex( iVD, i ), model->getIndex( iVD, j ) );
		}
		model->set<int>( iShape, "Num Vertices", newN );
		model->updateArraySize( iVD );
	}

	// rewrite triangles, dropping ones gone degenerate after the weld
	QVector<Triangle> kept;
	for ( int t = 0; t < numTris && t < model->rowCount( iTris ); t++ ) {
		Triangle tri = model->get<Triangle>( model->getIndex( iTris, t ) );
		int a = remap[tri[0]], b = remap[tri[1]], c = remap[tri[2]];
		if ( a == b || b == c || a == c )
			continue;
		kept.append( Triangle( a, b, c ) );
	}
	removedTris += numTris - kept.size();
	model->set<int>( iShape, "Num Triangles", kept.size() );
	model->updateArraySize( iTris );
	for ( int t = 0; t < kept.size(); t++ )
		model->set<Triangle>( model->getIndex( iTris, t ), kept[t] );
	if ( stride > 0 )
		model->set<int>( iShape, "Data Size", newN * stride + kept.size() * 6 );

	tlUpdateBounds( model, iShape );
	tlSkinResync( model, iShape, remap, newN != numVerts );
}

void GLView::mergeVertices( int mode, float threshold )
{
	if ( !model || !editMode || pickedElems.isEmpty() )
		return;
	QVector<PickedElement> seed = pickedElems;	// restored by the redo panel
	QHash<int, QSet<int>> byShape = pickedVertexRefs();
	if ( byShape.isEmpty() )
		return;

	int mergedVerts = 0, removedTris = 0;

	// per-shape inputs precomputed so redo closures stay deterministic (the
	// cursor target is resolved NOW; a later cursor move must not change redo)
	struct MergeJob { int sb; QVector<int> selList; bool haveTarget = false; Vector3 targetLocal; };
	QVector<MergeJob> jobs;
	bool inPlace = ( model->undoStack != nullptr );
	for ( auto it = byShape.constBegin(); it != byShape.constEnd(); ++it ) {
		MergeJob j;
		j.sb = it.key();
		j.selList = QVector<int>( it.value().constBegin(), it.value().constEnd() );
		std::sort( j.selList.begin(), j.selList.end() );
		j.haveTarget = ( mode == 1 );
		if ( j.haveTarget ) {
			Node * n = shapeForBlock( j.sb );
			Transform wt = n ? shapeRenderTrans( n ) : Transform();
			float sc = ( wt.scale != 0.0f ) ? wt.scale : 1.0f;
			j.targetLocal = wt.rotation.inverted() * ( ( cursorPos - wt.translation ) * ( 1.0f / sc ) );
		}
		QModelIndex iShape = model->getBlockIndex( j.sb );
		// legacy NiSkinData / partitions: the resync writes outside the shape
		// and would not be captured in place
		QModelIndex iSI = model->getBlockIndex( model->getLink( iShape, "Skin Instance" ) );
		if ( !iSI.isValid() )
			iSI = model->getBlockIndex( model->getLink( iShape, "Skin" ) );
		if ( iSI.isValid()
			&& ( model->getBlockIndex( model->getLink( iSI, "Data" ), "NiSkinData" ).isValid()
				|| model->getBlockIndex( model->getLink( iSI, "Skin Partition" ), "NiSkinPartition" ).isValid() ) )
			inPlace = false;
		jobs.append( j );
	}

	if ( inPlace ) {
		auto mv = std::make_shared<int>( 0 );
		auto rt = std::make_shared<int>( 0 );
		model->undoStack->beginMacro( tr( "Merge vertices" ) );
		for ( const MergeJob & j : std::as_const( jobs ) ) {
			QModelIndex iShape = model->getBlockIndex( j.sb );
			if ( !model->getIndex( iShape, "Vertex Data" ).isValid() )
				continue;
			const QPersistentModelIndex pShape( iShape );
			const QVector<int> selList = j.selList;
			const bool haveT = j.haveTarget;
			const Vector3 tgt = j.targetLocal;
			auto applyMerge = [this, pShape, selList, mode, threshold, haveT, tgt, mv, rt]() {
				QModelIndex iS( pShape );
				if ( !iS.isValid() )
					return;
				// suppress the per-leaf dataChanged storm (see deleteGeometry)
				model->setState( BaseModel::Processing );
				tlMergeShapeVerts( model, iS, selList, mode, threshold, haveT, tgt, *mv, *rt );
				model->restoreState();
				model->dataChanged( QModelIndex( iS ), QModelIndex( iS ) );
			};
			model->undoStack->push(
				new TlShapeStateCommand( model, iShape, tr( "Merge vertices" ), applyMerge ) );
		}
		model->undoStack->endMacro();
		mergedVerts = *mv;
		removedTris = *rt;
	} else {
		nifSnapshotOp( model, tr( "Merge vertices" ), [&]() {
			// suppress the per-leaf dataChanged storm (see deleteGeometry)
			model->setState( BaseModel::Processing );
			for ( const MergeJob & j : std::as_const( jobs ) )
				tlMergeShapeVerts( model, model->getBlockIndex( j.sb ), j.selList, mode,
					threshold, j.haveTarget, j.targetLocal, mergedVerts, removedTris );
			model->restoreState();
			for ( const MergeJob & j : std::as_const( jobs ) ) {
				QModelIndex iS = model->getBlockIndex( j.sb );
				if ( iS.isValid() )
					model->dataChanged( iS, iS );
			}
		} );
	}

	emit gizmoStatus( tr( "Merged %1 vertices, removed %2 triangles" ).arg( mergedVerts ).arg( removedTris ) );
	pickedElems.clear();
	modelChanged();
	// Redo Panel v2: scrub the merge distance afterwards (the whole merge is
	// one undo macro, so the reapply undoes exactly one entry)
	if ( mode == 2 ) {
		lastMergeDistance = threshold;
		lastOpExRerun = [this]( const QVector<TlOpParam> & ps ) {
			mergeVertices( 2, float( ps.value( 0 ).value ) );
		};
		QVector<TlOpParam> ps( 1 );
		ps[0].label = tr( "Merge Distance" );
		ps[0].type = TlOpParam::Float;
		ps[0].value = threshold;
		ps[0].mn = 0.0;
		ps[0].mx = 1000.0;
		ps[0].step = 0.01;
		ps[0].decimals = 4;
		armOperatorPanelEx( tr( "Merge by Distance" ), ps,
			model->undoStack ? 1 : 0, seed );
	}
}

bool GLView::reapplyOperator( float param )
{
	if ( opReapplying || lastOpKind == 0 )
		return false;
	opReapplying = true;	// suppress re-arming the panel while we re-run
	if ( lastOpKind == 1 ) {
		// merge by distance: undo the previous merge, restore the seed, re-merge
		if ( !model || !model->undoStack || model->undoStack->index() != lastOpUndoIndex ) {
			opReapplying = false;
			return false;	// something else touched the undo stack — stale
		}
		model->undoStack->undo();
		pickedElems = lastOpSeed;
		mergeVertices( 2, param );
		lastOpUndoIndex = model->undoStack ? model->undoStack->index() : -1;
	} else if ( lastOpKind == 2 ) {
		// select linked by angle: restore the seed selection, re-grow
		pickedElems = lastOpSeed;
		selectLinked( true, param );
	} else if ( lastOpKind == 3 ) {
		// Floating decal preview is deliberately in-place: rebuilding cloned
		// blocks and serializing the whole NIF for every scrub tick was very slow.
		if ( !model || !model->undoStack || model->undoStack->index() != lastOpUndoIndex ) {
			opReapplying = false;
			return false;
		}
		QSet<int> touched;
		for ( const DecalPreviewVert & dv : lastDecalVerts ) {
			QModelIndex iShape = model->getBlockIndex( dv.shape );
			if ( !iShape.isValid() ) {
				opReapplying = false;
				return false;
			}
			tlSetVertexLocal( model, iShape, dv.vertex, dv.base + dv.normal * param );
			touched.insert( dv.shape );
		}
		for ( int sb : touched )
			tlUpdateBounds( model, model->getBlockIndex( sb ) );
		modelChanged();
	}
	lastOpParam = param;
	opReapplying = false;
	return true;
}

void GLView::commitOperatorPreview()
{
	if ( lastOpKind != 3 || !model || !model->undoStack
		|| model->undoStack->index() != lastOpUndoIndex || lastOpUndoIndex <= 0 )
		return;
	QByteArray after;
	QBuffer buf( &after );
	if ( !buf.open( QIODevice::WriteOnly ) || !model->save( buf ) )
		return;
	const QUndoCommand * command = model->undoStack->command( lastOpUndoIndex - 1 );
	auto snapshot = dynamic_cast<NifSnapshotCommand *>( const_cast<QUndoCommand *>( command ) );
	if ( snapshot )
		snapshot->setAfterSnapshot( after );
}

// ---------------------------------------------------------------------------
// generalized operator redo panel (Redo Panel v2, MODELING_TOOLS_PLAN F0.a)

void GLView::armOperatorPanelEx( const QString & title, const QVector<TlOpParam> & params,
	int undoSteps, const QVector<PickedElement> & seed )
{
	if ( opExReapplying )
		return;
	lastOpExParams = params;
	lastOpExUndoSteps = std::max( undoSteps, 0 );
	lastOpExSeed = seed;
	lastOpExUndoIndex = ( model && model->undoStack ) ? model->undoStack->index() : -1;
	emit operatorPanelEx( title, params );
}

bool GLView::reapplyOperatorEx( const QVector<TlOpParam> & params )
{
	if ( opExReapplying || !lastOpExRerun || !model || !model->undoStack )
		return false;
	if ( model->undoStack->index() != lastOpExUndoIndex )
		return false;	// something else touched the undo stack — stale
	opExReapplying = true;
	for ( int i = 0; i < lastOpExUndoSteps; i++ )
		model->undoStack->undo();
	pickedElems = lastOpExSeed;	// re-run on the operator's original selection
	const int base = model->undoStack->index();
	lastOpExRerun( params );
	lastOpExUndoSteps = model->undoStack->index() - base;
	lastOpExUndoIndex = model->undoStack->index();
	lastOpExParams = params;
	opExReapplying = false;
	update();
	return true;
}

void GLView::joinSelectedObjects()
{
	if ( !model || editMode || objActive < 0 || objSelection.size() < 2 )
		return;
	QModelIndex iActive = model->getBlockIndex( objActive );
	if ( !model->blockInherits( iActive, "BSTriShape" ) ) {
		emit gizmoStatus( tr( "Join: the active object must be a BSTriShape" ) );
		return;
	}
	quint64 activeDesc = model->get<BSVertexDesc>( iActive, "Vertex Desc" ).Value();

	// compatible sources: same block type + identical vertex format
	QVector<int> sources;
	for ( int sb : objSelection ) {
		if ( sb == objActive )
			continue;
		QModelIndex iS = model->getBlockIndex( sb );
		if ( !model->blockInherits( iS, "BSTriShape" ) )
			continue;
		if ( model->get<BSVertexDesc>( iS, "Vertex Desc" ).Value() != activeDesc )
			continue;
		sources.append( sb );
	}
	if ( sources.isEmpty() ) {
		emit gizmoStatus( tr( "Join: no compatible meshes selected (need a matching vertex format)" ) );
		return;
	}

	Transform activeWorld;
	if ( Node * an = scene->getNode( model, iActive ) )
		activeWorld = an->worldTrans();

	int joined = 0;
	bool capSkipped = false;
	QVector<int> merged;
	QPersistentModelIndex pActive( iActive );
	// DELIBERATELY snapshot undo (the one remaining topology op with a reload
	// flash on Ctrl+Z): Join REMOVES the source blocks, which renumbers every
	// block above them and rewrites links model-wide — restoring that in
	// place would need exact block re-insertion at original indices with full
	// link mending. The snapshot is the safe undo here.
	nifSnapshotOp( model, tr( "Join geometry" ), [&]() {
		Transform activeInv = activeWorld.inverted();
		for ( int sb : sources ) {
			QModelIndex iA( pActive );
			QModelIndex iS = model->getBlockIndex( sb );
			int oldNV = model->get<int>( iA, "Num Vertices" );
			int oldNT = model->get<int>( iA, "Num Triangles" );
			int addNV = model->get<int>( iS, "Num Vertices" );
			int addNT = model->get<int>( iS, "Num Triangles" );
			if ( addNV <= 0 || addNT <= 0 )
				continue;
			// vertex indices and Num Vertices are uint16 — merging past the
			// cap wraps the reindexed triangles onto unrelated vertices
			if ( oldNV + addNV > 0xFFFF ) {
				capSkipped = true;
				continue;
			}

			Transform srcWorld;
			if ( Node * sn = scene->getNode( model, iS ) )
				srcWorld = sn->worldTrans();
			Transform relT = activeInv * srcWorld;
			bool ident = tlNearIdentity( relT );

			int dataSize = model->get<int>( iA, "Data Size" );
			int stride = ( oldNV > 0 ) ? ( dataSize - oldNT * 6 ) / oldNV : 0;

			// append vertex data (verbatim, then transform into active space)
			model->set<int>( iA, "Num Vertices", oldNV + addNV );
			QModelIndex iAVD = model->getIndex( iA, "Vertex Data" );
			QModelIndex iSVD = model->getIndex( iS, "Vertex Data" );
			model->updateArraySize( iAVD );
			for ( int i = 0; i < addNV; i++ ) {
				QModelIndex sVert = model->getIndex( iSVD, i );
				QModelIndex dVert = model->getIndex( iAVD, oldNV + i );
				tlCopyItemValues( model, sVert, dVert );
				if ( !ident ) {
					Vector3 v = model->get<Vector3>( dVert, "Vertex" );
					tlSetVertexLocal( model, iA, oldNV + i, relT * v );
					if ( model->getIndex( dVert, "Normal" ).isValid() ) {
						Vector3 n = relT.rotation * model->get<Vector3>( dVert, "Normal" );
						n.normalize();
						model->set<ByteVector3>( dVert, "Normal", n );
					}
					if ( model->getIndex( dVert, "Tangent" ).isValid() ) {
						Vector3 tg = relT.rotation * model->get<Vector3>( dVert, "Tangent" );
						tg.normalize();
						model->set<ByteVector3>( dVert, "Tangent", tg );
					}
				}
			}

			// append triangles, reindexed by the vertex offset
			QModelIndex iAT = model->getIndex( iA, "Triangles" );
			QModelIndex iST = model->getIndex( iS, "Triangles" );
			model->set<int>( iA, "Num Triangles", oldNT + addNT );
			model->updateArraySize( iAT );
			for ( int t = 0; t < addNT; t++ ) {
				Triangle tri = model->get<Triangle>( model->getIndex( iST, t ) );
				tri[0] = quint16( tri[0] + oldNV );
				tri[1] = quint16( tri[1] + oldNV );
				tri[2] = quint16( tri[2] + oldNV );
				model->set<Triangle>( model->getIndex( iAT, oldNT + t ), tri );
			}
			if ( stride > 0 )
				model->set<int>( iA, "Data Size", ( oldNV + addNV ) * stride + ( oldNT + addNT ) * 6 );

			joined++;
			merged.append( sb );
		}

		tlUpdateBounds( model, QModelIndex( pActive ) );

		// remove ONLY the merged source blocks (high -> low so numbers stay
		// valid); skipped sources must survive untouched
		QVector<int> rm = merged;
		std::sort( rm.begin(), rm.end(), std::greater<int>() );
		for ( int sb : rm )
			model->removeNiBlock( sb );
	} );

	if ( joined == 0 ) {
		emit gizmoStatus( capSkipped
			? tr( "Join: would exceed the 65,535-vertex limit of BSTriShape" )
			: tr( "Join: nothing merged" ) );
		return;
	}

	int newActive = model->getBlockNumber( QModelIndex( pActive ) );
	objSelection.clear();
	objActive = newActive;
	if ( newActive >= 0 ) {
		objSelection.insert( newActive );
		scene->currentBlock = model->getBlockIndex( newActive );
		scene->currentIndex = QModelIndex( scene->currentBlock );
	}
	emit objectSelectionChanged();
	emit gizmoStatus( tr( "Joined %1 mesh(es) into the active object" ).arg( joined )
		+ ( capSkipped ? tr( " (some skipped at the 65,535-vertex limit)" ) : QString() ) );
	modelChanged();
}

void GLView::deleteSelectedObjects()
{
	if ( !model || editMode || objSelection.isEmpty() )
		return;
	deleteBlocksWithConfirm( QVector<int>( objSelection.constBegin(), objSelection.constEnd() ) );
}

int GLView::deleteBlocksWithConfirm( const QVector<int> & blocks )
{
	if ( !model )
		return 0;
	// keep only valid, distinct blocks
	QSet<int> roots;
	for ( int b : blocks )
		if ( b >= 0 && b < model->getBlockCount() )
			roots.insert( b );
	if ( roots.isEmpty() )
		return 0;

	QMessageBox box( QMessageBox::NoIcon, tr( "Delete" ),
		tr( "Delete selected objects?" ), QMessageBox::NoButton );
	QPushButton * del = box.addButton( tr( "Delete" ), QMessageBox::AcceptRole );
	box.addButton( tr( "Cancel" ), QMessageBox::RejectRole );
	box.setDefaultButton( del );
	// Blender-style: open with the Delete button under the pointer
	tlPlacePopupAtCursor( &box, del );
	box.exec();
	if ( box.clickedButton() != del )
		return 0;

	// branch closure: each selected block plus every descendant it parents
	// (matches Remove Branch; shared refs / property blocks are left alone)
	QSet<int> closure;
	std::function<void( int )> collect = [&]( int b ) {
		if ( b < 0 || closure.contains( b ) )
			return;
		closure.insert( b );
		for ( int link : model->getChildLinks( b ) )
			if ( link >= 0 && model->getParent( link ) == b )
				collect( link );
	};
	for ( int b : std::as_const( roots ) )
		collect( b );

	// parents that survive the deletion get a dangling -1 child link to prune
	QSet<int> survivingParents;
	for ( int b : std::as_const( roots ) ) {
		int p = model->getParent( b );
		if ( p >= 0 && !closure.contains( p ) )
			survivingParents.insert( p );
	}

	// block numbers shift as rows are removed; track everything by persistent
	// index so the whole thing is order-independent and one undo step
	QVector<QPersistentModelIndex> pDelete;
	for ( int b : std::as_const( closure ) )
		pDelete.append( model->getBlockIndex( b ) );
	QVector<QPersistentModelIndex> pParents;
	for ( int p : std::as_const( survivingParents ) )
		pParents.append( model->getBlockIndex( p ) );

	// block removal renumbers every later block and rewrites links model-wide;
	// a whole-model snapshot is the safe single-step undo (as Join uses)
	nifSnapshotOp( model, tr( "Delete objects" ), [&]() {
		for ( const QPersistentModelIndex & p : std::as_const( pDelete ) )
			if ( p.isValid() )
				model->removeNiBlock( model->getBlockNumber( p ) );
		for ( const QPersistentModelIndex & p : std::as_const( pParents ) )
			if ( p.isValid() )
				while ( tlRemoveNullChildLink( model, model->getBlockNumber( p ) ) )
					;
	} );

	// the selection is gone; drop any stale references and refresh
	objSelection.clear();
	objActive = -1;
	pickedElems.clear();
	scene->currentBlock = QModelIndex();
	scene->currentIndex = QModelIndex();
	emit objectSelectionChanged();
	emit gizmoStatus( tr( "Deleted %1 object(s)" ).arg( roots.size() ) );
	modelChanged();
	return roots.size();
}

void GLView::duplicateElements()
{
	if ( !model || pickedElems.isEmpty() )
		return;

	auto edgeKey = []( int a, int b ) {
		return ( quint64( std::min( a, b ) ) << 32 ) | quint64( std::max( a, b ) );
	};
	Q_UNUSED( edgeKey );

	QHash<int, QSet<int>> selVerts = pickedVertexRefs();
	QHash<int, QSet<int>> selFacesX;
	for ( const auto & pe : pickedElems )
		if ( pe.type == 3 && pe.shapeBlock >= 0 )
			selFacesX[pe.shapeBlock].insert( pe.e0 );

	QSet<int> shapeSet;
	for ( int k : selVerts.keys() )
		shapeSet.insert( k );
	for ( int k : selFacesX.keys() )
		shapeSet.insert( k );
	if ( shapeSet.isEmpty() )
		return;

	QVector<PickedElement> newSel;
	int totalV = 0;
	bool capSkipped = false;
	auto lastNewShape = std::make_shared<int>( -1 );
	int newShapeCount = 0;

	// per-shape inputs precomputed from pre-op state so the redo closures are
	// deterministic; the append-only mutation runs under an in-place command
	const bool macroU = ( model->undoStack != nullptr );
	if ( macroU )
		model->undoStack->beginMacro( tr( "Duplicate" ) );
	for ( int sb : shapeSet ) {
		QModelIndex iShape = model->getBlockIndex( sb );
		if ( !model->blockInherits( iShape, "BSTriShape" ) )
			continue;
		QModelIndex iVD = model->getIndex( iShape, "Vertex Data" );
		QModelIndex iTris = model->getIndex( iShape, "Triangles" );
		if ( !iVD.isValid() || !iTris.isValid() )
			continue;
		const int oldNV = model->get<int>( iShape, "Num Vertices" );
		const int oldNT = model->get<int>( iShape, "Num Triangles" );

		const QSet<int> & sv = selVerts.value( sb );
		const QSet<int> & sfx = selFacesX.value( sb );

		// faces to duplicate: explicit face selection or all-verts-selected
		QVector<int> dupFaces;
		for ( int t = 0; t < oldNT; t++ ) {
			Triangle tri = model->get<Triangle>( model->getIndex( iTris, t ) );
			bool dup = sfx.contains( t )
				|| ( !sv.isEmpty() && sv.contains( tri[0] ) && sv.contains( tri[1] ) && sv.contains( tri[2] ) );
			if ( dup )
				dupFaces.append( t );
		}

		// verts to duplicate = selected verts + verts of the duplicated faces
		QSet<int> dupVertsSet = sv;
		for ( int t : dupFaces ) {
			Triangle tri = model->get<Triangle>( model->getIndex( iTris, t ) );
			dupVertsSet.insert( tri[0] );
			dupVertsSet.insert( tri[1] );
			dupVertsSet.insert( tri[2] );
		}
		if ( dupVertsSet.isEmpty() )
			continue;
		// vertex indices and Num Vertices are uint16 — a duplicate past the
		// cap wraps every new Triangle write onto unrelated early vertices.
		// Offer to duplicate into a NEW shape instead (the Separate recipe
		// minus the source edit: clone with props, keep only the selected
		// faces, leave the original untouched — the clone keeps the source's
		// vertex array, so it can never itself exceed the cap).
		if ( oldNV + dupVertsSet.size() > 0xFFFF ) {
			if ( dupFaces.isEmpty() ) {
				capSkipped = true;	// loose verts only: no faces to carry over
				continue;
			}
			const QString srcName = model->get<QString>( iShape, "Name" );
			QMessageBox capBox( QMessageBox::NoIcon, tr( "Duplicate" ),
				tr( "Duplicating %1 vertices would push \"%2\" past the "
					"65,535-vertex limit of one BSTriShape.\n\n"
					"Duplicate the selection into a NEW shape instead?" )
					.arg( dupVertsSet.size() ).arg( srcName ), QMessageBox::NoButton );
			QPushButton * newShapeBtn = capBox.addButton( tr( "New Shape" ), QMessageBox::AcceptRole );
			capBox.addButton( tr( "Cancel" ), QMessageBox::RejectRole );
			capBox.setDefaultButton( newShapeBtn );
			tlPlacePopupAtCursor( &capBox, newShapeBtn );
			capBox.exec();
			if ( capBox.clickedButton() != newShapeBtn ) {
				capSkipped = true;
				continue;
			}
			QVector<bool> dupMask( oldNT, false );
			for ( int t : std::as_const( dupFaces ) )
				dupMask[t] = true;
			auto parents = std::make_shared<QVector<int>>();
			auto applyDupNew = [this, sb, dupMask, parents, lastNewShape]() {
				int nNew = tlCloneShapeWithProps( model, sb );
				if ( nNew < 0 )
					return;
				int parentNum = model->getParent( sb );
				if ( parentNum >= 0 ) {
					blockLink( model, model->getBlockIndex( parentNum ), model->getBlockIndex( nNew ) );
					parents->append( parentNum );
				}
				QString nm = model->get<QString>( model->getBlockIndex( sb ), "Name" );
				model->set<QString>( model->getBlockIndex( nNew ), "Name", tlUniqueNodeName( model, nm ) );
				tlKeepTriangles( model, model->getBlockIndex( nNew ),
					[dupMask]( int t ) { return t >= 0 && t < dupMask.size() && dupMask.at( t ); } );
				*lastNewShape = nNew;
			};
			if ( model->undoStack )
				model->undoStack->push( new TlBlockAppendCommand( model,
					tr( "Duplicate" ), applyDupNew, parents ) );
			else
				applyDupNew();
			newShapeCount++;
			continue;
		}

		QVector<int> dupVerts( dupVertsSet.constBegin(), dupVertsSet.constEnd() );
		std::sort( dupVerts.begin(), dupVerts.end() );
		QHash<int, int> vremap;
		for ( int i = 0; i < dupVerts.size(); i++ )
			vremap.insert( dupVerts[i], oldNV + i );
		QHash<int, int> fremap;
		for ( int i = 0; i < dupFaces.size(); i++ )
			fremap.insert( dupFaces[i], oldNT + i );

		const QPersistentModelIndex pShape( iShape );
		auto applyDup = [this, pShape, dupVerts, dupFaces, vremap, oldNV, oldNT]() {
			QModelIndex iS( pShape );
			if ( !iS.isValid() )
				return;
			// suppress the per-leaf dataChanged storm (see deleteGeometry)
			model->setState( BaseModel::Processing );
			QModelIndex iVD2 = model->getIndex( iS, "Vertex Data" );
			QModelIndex iT2 = model->getIndex( iS, "Triangles" );
			const int ds = model->get<int>( iS, "Data Size" );
			const int stride = ( oldNV > 0 ) ? ( ds - oldNT * 6 ) / oldNV : 0;

			// append the duplicated verts (verbatim - same positions/normals)
			const int addNV = dupVerts.size();
			model->set<int>( iS, "Num Vertices", oldNV + addNV );
			model->updateArraySize( iVD2 );
			for ( int i = 0; i < addNV; i++ )
				tlCopyItemValues( model, model->getIndex( iVD2, dupVerts[i] ), model->getIndex( iVD2, oldNV + i ) );

			// append the duplicated faces (reindexed to the new verts)
			const int addNT = dupFaces.size();
			if ( addNT > 0 ) {
				model->set<int>( iS, "Num Triangles", oldNT + addNT );
				model->updateArraySize( iT2 );
				for ( int i = 0; i < addNT; i++ ) {
					Triangle tri = model->get<Triangle>( model->getIndex( iT2, dupFaces[i] ) );
					tri[0] = quint16( vremap.value( tri[0] ) );
					tri[1] = quint16( vremap.value( tri[1] ) );
					tri[2] = quint16( vremap.value( tri[2] ) );
					model->set<Triangle>( model->getIndex( iT2, oldNT + i ), tri );
				}
			}
			if ( stride > 0 )
				model->set<int>( iS, "Data Size", ( oldNV + addNV ) * stride + ( oldNT + addNT ) * 6 );
			model->restoreState();
			model->dataChanged( QModelIndex( iS ), QModelIndex( iS ) );
		};
		if ( model->undoStack )
			model->undoStack->push( new TlShapeStateCommand( model, iShape, tr( "Duplicate" ), applyDup ) );
		else
			applyDup();

		// mirror the selection onto the duplicate (coincident, so the world
		// positions of the original picked elements still apply)
		for ( const auto & pe : pickedElems ) {
			if ( pe.shapeBlock != sb )
				continue;
			PickedElement np = pe;
			if ( pe.type == 1 ) {
				if ( !vremap.contains( pe.e0 ) )
					continue;
				np.e0 = vremap.value( pe.e0 );
			} else if ( pe.type == 2 ) {
				if ( !vremap.contains( pe.e0 ) || !vremap.contains( pe.e1 ) )
					continue;
				np.e0 = vremap.value( pe.e0 );
				np.e1 = vremap.value( pe.e1 );
			} else if ( pe.type == 3 ) {
				if ( !fremap.contains( pe.e0 ) )
					continue;
				np.e0 = fremap.value( pe.e0 );
			}
			newSel.append( np );
		}
		totalV += dupVerts.size();
	}
	if ( macroU )
		model->undoStack->endMacro();

	if ( newSel.isEmpty() ) {
		if ( *lastNewShape >= 0 ) {
			// everything went into new shape(s): hand over like Separate does
			setEditMode( false );
			pickedElems.clear();
			syncObjectSelection( *lastNewShape );
			emit clicked( model->getBlockIndex( *lastNewShape ) );
			emit gizmoStatus( tr( "Duplicated the selection into %1 new shape(s) - press G to move" )
				.arg( newShapeCount ) );
			modelChanged();
			return;
		}
		emit gizmoStatus( capSkipped
			? tr( "Duplicate: would exceed the 65,535-vertex limit of BSTriShape" )
			: tr( "Nothing to duplicate (select verts / faces)" ) );
		return;
	}

	// select the duplicates and start a move; Esc leaves them coincident with
	// the original (still the only selected geometry), Blender-style
	pickedElems = newSel;
	modelChanged();
	gizmoBeginElement( 1 );
	QString note;
	if ( newShapeCount > 0 )
		note = tr( " (+%1 over-limit shape(s) duplicated as new objects)" ).arg( newShapeCount );
	else if ( capSkipped )
		note = tr( " (a shape at the 65,535-vertex limit was skipped)" );
	emit gizmoStatus( tr( "Duplicated %1 vert(s) - move, or Esc to leave in place" ).arg( totalV ) + note );
}

// ---------------------------------------------------------------------------
// Extrude (E) — MODELING_TOOLS_PLAN Phase 1

//! Recompute area-weighted vertex normals for the given verts only (the
//! packed FO4 layout may omit the Normal field entirely — then this no-ops).
//! Tangents are left alone; run Update Tangent Space for a full refresh.
static void tlRecalcNormalsSubset( NifModel * model, const QModelIndex & iShape, const QSet<int> & verts )
{
	const QHash<int, Vector3> acc = tlAccumulateAreaNormals( model, iShape, verts );
	QModelIndex iVD = model->getIndex( iShape, "Vertex Data" );
	for ( auto it = acc.constBegin(); it != acc.constEnd(); ++it ) {
		Vector3 n = it.value();
		if ( n.squaredLength() < 1.0e-16f )
			continue;
		n.normalize();
		QModelIndex row = model->getIndex( iVD, it.key() );
		if ( !model->getItem( row, "Normal" ) )
			continue;	// tolerate an incomplete row rather than warn per vertex
		model->set<ByteVector3>( row, "Normal", n );
	}
}

//! World-space delta -> shape-local delta via the scene node transform
//! (approximation for skinned meshes with a deformed cage — same convention
//! as the redo panels elsewhere).
static Vector3 tlWorldToLocalDelta( Scene * scene, NifModel * model, int block, const Vector3 & d )
{
	Node * nd = scene ? scene->getNode( model, model->getBlockIndex( block ) ) : nullptr;
	if ( !nd )
		return d;
	Transform t = nd->worldTrans();
	Vector3 local = t.rotation.inverted() * d;
	if ( t.scale > 1.0e-6f && t.scale != 1.0f )
		local /= t.scale;
	return local;
}

//! Extrude plan, computed READ-ONLY before the snapshot mutates anything
//! (nifSnapshotOp always pushes, so a failed op must never reach it).
struct TlExtrudePlan
{
	QVector<int> dupVerts;              //!< originals to duplicate, sorted
	QHash<int, int> vremap;             //!< original -> duplicate index
	QVector<int> repointFaces;          //!< region faces re-pointed onto the duplicates
	struct Wall { int a, b; };          //!< boundary edge a->b in cap winding
	QVector<Wall> walls;
	//! Blender vertex extrude: verts that extrude to a bare edge. NIF has no
	//! loose edges, so each becomes a zero-area scaffold triangle (v, v', v')
	//! — it draws as the edge line in wireframe, a later edge extrude turns it
	//! into real faces, and a weld drops it as degenerate.
	QVector<int> spurVerts;
	QSet<int> capVerts;                 //!< final indices that move with the extrusion
	int oldNV = 0, oldNT = 0, stride = 0;
};

static bool tlExtrudePlanBuild( NifModel * model, const QModelIndex & iShape,
	const QSet<int> & selVerts, const QSet<int> & selFacesX,
	const QVector<QPair<int, int>> & selEdges, TlExtrudePlan & plan, QString & err )
{
	plan = TlExtrudePlan();
	plan.oldNV = model->get<int>( iShape, "Num Vertices" );
	plan.oldNT = model->get<int>( iShape, "Num Triangles" );
	const int dataSize = model->get<int>( iShape, "Data Size" );
	plan.stride = ( plan.oldNV > 0 ) ? ( dataSize - plan.oldNT * 6 ) / plan.oldNV : 0;
	QModelIndex iTris = model->getIndex( iShape, "Triangles" );
	if ( !iTris.isValid() || plan.oldNT < 1 ) {
		err = GLView::tr( "Extrude: unexpected mesh layout" );
		return false;
	}
	QVector<Triangle> tris( plan.oldNT );
	for ( int t = 0; t < plan.oldNT; t++ )
		tris[t] = model->get<Triangle>( model->getIndex( iTris, t ) );

	auto ekey = []( int a, int b ) {
		if ( a > b ) std::swap( a, b );
		return ( quint64( quint32( a ) ) << 32 ) | quint32( b );
	};

	// degenerate scaffold triangles (vertex-extrude spurs, welded leftovers)
	// are edge stand-ins, never region faces
	auto isDegenerate = [&tris]( int t ) {
		const Triangle & tr = tris.at( t );
		return tr[0] == tr[1] || tr[1] == tr[2] || tr[0] == tr[2];
	};

	// region faces: explicit face picks, or faces with every corner selected
	QSet<int> fsel;
	for ( int f : selFacesX )
		if ( f >= 0 && f < plan.oldNT && !isDegenerate( f ) )
			fsel << f;
	if ( !selVerts.isEmpty() )
		for ( int t = 0; t < plan.oldNT; t++ )
			if ( !isDegenerate( t )
				&& selVerts.contains( tris[t][0] ) && selVerts.contains( tris[t][1] )
				&& selVerts.contains( tris[t][2] ) )
				fsel << t;

	QSet<int> dupSet;
	if ( !fsel.isEmpty() ) {
		// region mode: boundary = edges used by exactly one region face; those
		// verts split, interior verts ride along with the cap
		QHash<quint64, int> edgeUse;
		for ( int f : std::as_const( fsel ) ) {
			const Triangle & t = tris.at( f );
			edgeUse[ekey( t[0], t[1] )]++;
			edgeUse[ekey( t[1], t[2] )]++;
			edgeUse[ekey( t[2], t[0] )]++;
		}
		for ( int f : std::as_const( fsel ) ) {
			const Triangle & t = tris.at( f );
			for ( int e = 0; e < 3; e++ ) {
				const int a = t[e], b = t[( e + 1 ) % 3];
				if ( edgeUse.value( ekey( a, b ) ) == 1 ) {
					plan.walls.append( { a, b } );	// a->b in cap winding
					dupSet << a << b;
				}
			}
		}
		plan.repointFaces = QVector<int>( fsel.constBegin(), fsel.constEnd() );
		std::sort( plan.repointFaces.begin(), plan.repointFaces.end() );
	} else {
		// edge-run mode: explicit edge picks, else mesh edges induced by the
		// selected verts; the run extrudes to a ribbon of wall quads
		QVector<QPair<int, int>> edges = selEdges;
		if ( edges.isEmpty() && !selVerts.isEmpty() ) {
			QSet<quint64> seen;
			for ( int t = 0; t < plan.oldNT; t++ ) {
				for ( int e = 0; e < 3; e++ ) {
					const int a = tris[t][e], b = tris[t][( e + 1 ) % 3];
					if ( a != b && selVerts.contains( a ) && selVerts.contains( b )
						&& !seen.contains( ekey( a, b ) ) ) {
						seen << ekey( a, b );
						edges.append( { a, b } );
					}
				}
			}
		}
		// orient each edge by the winding of one adjacent face so the ribbon
		// faces the same way as the surrounding surface
		QHash<quint64, QPair<int, int>> winding;
		for ( int t = 0; t < plan.oldNT; t++ )
			for ( int e = 0; e < 3; e++ ) {
				const int a = tris[t][e], b = tris[t][( e + 1 ) % 3];
				if ( a != b && !winding.contains( ekey( a, b ) ) )
					winding.insert( ekey( a, b ), { a, b } );
			}
		QSet<quint64> seenE;
		QSet<int> covered;
		for ( const auto & ed : std::as_const( edges ) ) {
			if ( ed.first == ed.second )
				continue;	// a degenerate self-edge can't extrude
			const quint64 k = ekey( ed.first, ed.second );
			if ( seenE.contains( k ) )
				continue;
			seenE << k;
			const auto w = winding.value( k, ed );
			plan.walls.append( { w.first, w.second } );
			dupSet << ed.first << ed.second;
			covered << ed.first << ed.second;
		}
		// verts not on any extruded edge are Blender vertex extrudes: they pull
		// out a bare edge, realised as a zero-area scaffold triangle
		for ( int v : selVerts ) {
			if ( !covered.contains( v ) ) {
				plan.spurVerts.append( v );
				dupSet << v;
			}
		}
		std::sort( plan.spurVerts.begin(), plan.spurVerts.end() );
		if ( dupSet.isEmpty() ) {
			err = GLView::tr( "Extrude: nothing to extrude" );
			return false;
		}
	}
	if ( dupSet.isEmpty() && fsel.isEmpty() ) {
		err = GLView::tr( "Extrude: nothing to extrude" );
		return false;
	}
	if ( plan.oldNV + dupSet.size() > 0xFFFF ) {
		err = GLView::tr( "Extrude: would exceed the 65,535-vertex limit of BSTriShape" );
		return false;
	}
	plan.dupVerts = QVector<int>( dupSet.constBegin(), dupSet.constEnd() );
	std::sort( plan.dupVerts.begin(), plan.dupVerts.end() );
	for ( int i = 0; i < plan.dupVerts.size(); i++ )
		plan.vremap.insert( plan.dupVerts.at( i ), plan.oldNV + i );
	// the cap = duplicated boundary verts + (region mode) the interior verts
	for ( int i = 0; i < plan.dupVerts.size(); i++ )
		plan.capVerts << plan.oldNV + i;
	for ( int f : std::as_const( plan.repointFaces ) ) {
		const Triangle & t = tris.at( f );
		for ( int c = 0; c < 3; c++ )
			if ( !plan.vremap.contains( t[c] ) )
				plan.capVerts << t[c];
	}
	return true;
}

//! Mutate the mesh per the plan (run inside nifSnapshotOp): duplicate the
//! boundary verts, re-point the region faces, stitch the side walls, offset
//! the cap and refresh normals/bounds.
static void tlExtrudeApplyPlan( NifModel * model, const QModelIndex & iShape,
	const TlExtrudePlan & plan, const Vector3 & localOffset, bool flipNormals )
{
	QModelIndex iVD = model->getIndex( iShape, "Vertex Data" );
	QModelIndex iTris = model->getIndex( iShape, "Triangles" );
	if ( !iVD.isValid() || !iTris.isValid() )
		return;
	// suppress the per-leaf dataChanged storm and any mid-mutation view
	// reactions; one dataChanged for the shape goes out at the end
	model->setState( BaseModel::Processing );

	// append the duplicated verts (verbatim rows: UVs, weights, colors copy)
	const int addNV = plan.dupVerts.size();
	model->set<int>( iShape, "Num Vertices", plan.oldNV + addNV );
	model->updateArraySize( iVD );
	for ( int i = 0; i < addNV; i++ )
		tlCopyItemValues( model, model->getIndex( iVD, plan.dupVerts.at( i ) ),
			model->getIndex( iVD, plan.oldNV + i ) );

	// re-point the region faces onto the duplicates: the surface detaches and
	// becomes the moving cap while the surrounding mesh keeps the originals
	for ( int f : std::as_const( plan.repointFaces ) ) {
		Triangle tri = model->get<Triangle>( model->getIndex( iTris, f ) );
		bool changed = false;
		for ( int c = 0; c < 3; c++ ) {
			auto it = plan.vremap.constFind( tri[c] );
			if ( it != plan.vremap.constEnd() ) {
				tri[c] = quint16( it.value() );
				changed = true;
			}
		}
		if ( changed )
			model->set<Triangle>( model->getIndex( iTris, f ), tri );
	}

	// side walls: boundary edge a->b (cap winding) gets the quad a,b,b',a' as
	// two outward-facing triangles (a,b,b') + (a,b',a'); vertex spurs get one
	// zero-area scaffold triangle (v, v', v') that draws as the new edge
	const int addNT = plan.walls.size() * 2 + plan.spurVerts.size();
	if ( addNT > 0 ) {
		model->set<int>( iShape, "Num Triangles", plan.oldNT + addNT );
		model->updateArraySize( iTris );
		int t = plan.oldNT;
		for ( const TlExtrudePlan::Wall & w : std::as_const( plan.walls ) ) {
			const quint16 a = quint16( w.a ), b = quint16( w.b );
			const quint16 a2 = quint16( plan.vremap.value( w.a ) );
			const quint16 b2 = quint16( plan.vremap.value( w.b ) );
			Triangle t1( a, b, b2 ), t2( a, b2, a2 );
			if ( flipNormals ) {
				t1 = Triangle( a, b2, b );
				t2 = Triangle( a, a2, b2 );
			}
			model->set<Triangle>( model->getIndex( iTris, t++ ), t1 );
			model->set<Triangle>( model->getIndex( iTris, t++ ), t2 );
		}
		for ( int v : std::as_const( plan.spurVerts ) ) {
			const quint16 d = quint16( plan.vremap.value( v ) );
			model->set<Triangle>( model->getIndex( iTris, t++ ), Triangle( quint16( v ), d, d ) );
		}
	}
	if ( plan.stride > 0 )
		model->set<int>( iShape, "Data Size",
			( plan.oldNV + addNV ) * plan.stride + ( plan.oldNT + addNT ) * 6 );

	// offset the cap
	if ( !( localOffset == Vector3() ) ) {
		for ( int v : plan.capVerts ) {
			Vector3 p = model->get<Vector3>( model->getIndex( iVD, v ), "Vertex" );
			tlSetVertexLocal( model, iShape, v, p + localOffset );
		}
	}

	// refresh normals of everything the extrusion touched (cap + the original
	// boundary ring that now borders the walls)
	QSet<int> touched = plan.capVerts;
	for ( int v : std::as_const( plan.dupVerts ) )
		touched << v;
	tlRecalcNormalsSubset( model, iShape, touched );
	tlUpdateBounds( model, iShape );
	model->restoreState();
	model->dataChanged( QModelIndex( iShape ), QModelIndex( iShape ) );
}

// ---------------------------------------------------------------------------
// Fill (F) / Bridge Edge Loops — MODELING_TOOLS_PLAN Phase 2

//! An ordered run of rim vertices (closed = ring). The order follows the
//! HOLE direction (reverse of the adjacent surface winding), so a cap wound
//! in loop order faces the same way as the surrounding surface.
struct TlLoop
{
	QVector<int> verts;
	bool closed = false;
};

//! Extract ordered rim loops from the selection: explicit edge picks if any,
//! else the mesh boundary edges (exactly one adjacent non-degenerate face)
//! whose both endpoints are selected. Returns false with err on non-manifold
//! chains or when nothing usable is selected.
static bool tlExtractLoops( NifModel * model, const QModelIndex & iShape,
	const QSet<int> & selVerts, const QVector<QPair<int, int>> & selEdges,
	QVector<TlLoop> & loops, QString & err )
{
	loops.clear();
	QModelIndex iTris = model->getIndex( iShape, "Triangles" );
	const int numTris = model->get<int>( iShape, "Num Triangles" );
	if ( !iTris.isValid() || numTris < 1 ) {
		err = GLView::tr( "Fill/Bridge: unexpected mesh layout" );
		return false;
	}
	QVector<Triangle> tris( numTris );
	for ( int t = 0; t < numTris; t++ )
		tris[t] = model->get<Triangle>( model->getIndex( iTris, t ) );

	auto ekey = []( int a, int b ) {
		if ( a > b ) std::swap( a, b );
		return ( quint64( quint32( a ) ) << 32 ) | quint32( b );
	};
	auto degenerate = []( const Triangle & t ) {
		return t[0] == t[1] || t[1] == t[2] || t[0] == t[2];
	};

	// surface winding of each undirected edge (first non-degenerate face wins)
	QHash<quint64, QPair<int, int>> winding;
	QHash<quint64, int> useCount;
	for ( int t = 0; t < numTris; t++ ) {
		if ( degenerate( tris.at( t ) ) )
			continue;
		for ( int e = 0; e < 3; e++ ) {
			const int a = tris[t][e], b = tris[t][( e + 1 ) % 3];
			useCount[ekey( a, b )]++;
			if ( !winding.contains( ekey( a, b ) ) )
				winding.insert( ekey( a, b ), { a, b } );
		}
	}

	// candidate rim edges, directed along the HOLE (reverse surface winding)
	QHash<int, int> next;           // from -> to
	QSet<int> hasIncoming;
	auto addDirected = [&]( int from, int to, QString & e2 ) {
		if ( next.contains( from ) && next.value( from ) != to ) {
			e2 = GLView::tr( "Fill/Bridge: the selected rim is non-manifold (a vertex joins 3+ rim edges)" );
			return false;
		}
		next.insert( from, to );
		hasIncoming << to;
		return true;
	};
	if ( !selEdges.isEmpty() ) {
		QSet<quint64> seen;
		for ( const auto & ed : selEdges ) {
			if ( ed.first == ed.second || seen.contains( ekey( ed.first, ed.second ) ) )
				continue;
			seen << ekey( ed.first, ed.second );
			const auto w = winding.value( ekey( ed.first, ed.second ), ed );
			if ( !addDirected( w.second, w.first, err ) )
				return false;
		}
	} else {
		for ( auto it = useCount.constBegin(); it != useCount.constEnd(); ++it ) {
			if ( it.value() != 1 )
				continue;	// interior edge
			const auto w = winding.value( it.key() );
			if ( !selVerts.contains( w.first ) || !selVerts.contains( w.second ) )
				continue;
			if ( !addDirected( w.second, w.first, err ) )
				return false;
		}
	}
	if ( next.isEmpty() ) {
		err = GLView::tr( "Fill/Bridge: select the rim of a hole (boundary vertices or edges)" );
		return false;
	}

	// chain: open runs start at verts with no incoming edge, the rest are rings
	QSet<int> visited;
	auto walk = [&]( int start, bool closed ) {
		TlLoop loop;
		loop.closed = closed;
		int v = start;
		while ( true ) {
			loop.verts.append( v );
			visited << v;
			auto it = next.constFind( v );
			if ( it == next.constEnd() )
				break;
			v = it.value();
			if ( v == start ) {
				loop.closed = true;
				break;
			}
			if ( visited.contains( v ) )
				break;	// merged into an earlier walk (shouldn't happen when manifold)
		}
		if ( loop.verts.size() >= 2 )
			loops.append( loop );
	};
	for ( auto it = next.constBegin(); it != next.constEnd(); ++it )
		if ( !hasIncoming.contains( it.key() ) )
			walk( it.key(), false );
	for ( auto it = next.constBegin(); it != next.constEnd(); ++it )
		if ( !visited.contains( it.key() ) )
			walk( it.key(), true );
	if ( loops.isEmpty() ) {
		err = GLView::tr( "Fill/Bridge: no usable rim loop in the selection" );
		return false;
	}
	return true;
}

//! Newell plane normal of an ordered polygon
static Vector3 tlNewellNormal( const QVector<Vector3> & p )
{
	Vector3 n;
	for ( int i = 0; i < p.size(); i++ ) {
		const Vector3 & a = p.at( i );
		const Vector3 & b = p.at( ( i + 1 ) % p.size() );
		n[0] += ( a[1] - b[1] ) * ( a[2] + b[2] );
		n[1] += ( a[2] - b[2] ) * ( a[0] + b[0] );
		n[2] += ( a[0] - b[0] ) * ( a[1] + b[1] );
	}
	return n;
}

//! Ear-clip an ordered (closed) polygon; returns triangles as loop-order
//! index triples, wound in loop order. Falls back to a fan when stuck
//! (degenerate/self-intersecting rims still produce something usable).
static QVector<Triangle> tlEarClip( const QVector<Vector3> & poly )
{
	QVector<Triangle> out;
	const int n = poly.size();
	if ( n < 3 )
		return out;
	// project onto the dominant plane of the Newell normal
	Vector3 nrm = tlNewellNormal( poly );
	int drop = 2;
	if ( std::fabs( nrm[0] ) >= std::fabs( nrm[1] ) && std::fabs( nrm[0] ) >= std::fabs( nrm[2] ) )
		drop = 0;
	else if ( std::fabs( nrm[1] ) >= std::fabs( nrm[2] ) )
		drop = 1;
	const int ax = ( drop + 1 ) % 3, ay = ( drop + 2 ) % 3;
	const float sign = ( nrm[drop] >= 0.0f ) ? 1.0f : -1.0f;
	QVector<QPointF> p2( n );
	for ( int i = 0; i < n; i++ )
		p2[i] = QPointF( poly.at( i )[ax], sign * poly.at( i )[ay] );
	auto cross2 = []( const QPointF & o, const QPointF & a, const QPointF & b ) {
		return ( a.x() - o.x() ) * ( b.y() - o.y() ) - ( a.y() - o.y() ) * ( b.x() - o.x() );
	};
	QVector<int> idx( n );
	for ( int i = 0; i < n; i++ )
		idx[i] = i;
	int guard = 0;
	while ( idx.size() > 3 && guard < n * n ) {
		bool clipped = false;
		for ( int i = 0; i < idx.size(); i++ ) {
			const int i0 = idx.at( ( i + idx.size() - 1 ) % idx.size() );
			const int i1 = idx.at( i );
			const int i2 = idx.at( ( i + 1 ) % idx.size() );
			if ( cross2( p2[i0], p2[i1], p2[i2] ) <= 0.0 )
				continue;	// reflex
			bool ear = true;
			for ( int j : std::as_const( idx ) ) {
				if ( j == i0 || j == i1 || j == i2 )
					continue;
				if ( cross2( p2[i0], p2[i1], p2[j] ) > 0.0
					&& cross2( p2[i1], p2[i2], p2[j] ) > 0.0
					&& cross2( p2[i2], p2[i0], p2[j] ) > 0.0 ) {
					ear = false;
					break;
				}
			}
			if ( ear ) {
				out.append( Triangle( quint16( i0 ), quint16( i1 ), quint16( i2 ) ) );
				idx.removeAt( i );
				clipped = true;
				break;
			}
			guard++;
		}
		if ( !clipped )
			break;	// stuck: fan the remainder below
	}
	if ( idx.size() == 3 ) {
		out.append( Triangle( quint16( idx[0] ), quint16( idx[1] ), quint16( idx[2] ) ) );
	} else {
		for ( int i = 1; i + 1 < idx.size(); i++ )
			out.append( Triangle( quint16( idx[0] ), quint16( idx[i] ), quint16( idx[i + 1] ) ) );
	}
	return out;
}

//! Fill dst's row with an interpolation of rows va and vb (t = 0..1):
//! position, UV, normal/tangent (renormalized) and bone weights (merged,
//! top 4, renormalized); everything else copies from va.
static void tlWriteLerpVertex( NifModel * model, const QModelIndex & iShape,
	int dst, int va, int vb, float t )
{
	QModelIndex iVD = model->getIndex( iShape, "Vertex Data" );
	QModelIndex rowA = model->getIndex( iVD, va );
	QModelIndex rowB = model->getIndex( iVD, vb );
	QModelIndex rowD = model->getIndex( iVD, dst );
	if ( !rowA.isValid() || !rowB.isValid() || !rowD.isValid() )
		return;
	tlCopyItemValues( model, rowA, rowD );
	const Vector3 pa = model->get<Vector3>( rowA, "Vertex" );
	const Vector3 pb = model->get<Vector3>( rowB, "Vertex" );
	tlSetVertexLocal( model, iShape, dst, pa + ( pb - pa ) * t );
	if ( model->getItem( rowA, "UV" ) ) {
		Vector2 uv = model->get<Vector2>( rowA, "UV" ) * ( 1.0f - t )
			+ model->get<Vector2>( rowB, "UV" ) * t;
		model->set<HalfVector2>( rowD, "UV", HalfVector2( uv ) );
	}
	for ( const char * attr : { "Normal", "Tangent" } ) {
		if ( !model->getItem( rowA, attr ) )
			continue;
		Vector3 v = model->get<Vector3>( rowA, attr ) * ( 1.0f - t )
			+ model->get<Vector3>( rowB, attr ) * t;
		if ( v.squaredLength() > 1.0e-12f ) {
			v.normalize();
			model->set<ByteVector3>( rowD, attr, v );
		}
	}
	if ( model->getItem( rowA, "Bone Weights" ) ) {
		QHash<int, float> acc;
		for ( int side = 0; side < 2; side++ ) {
			const QModelIndex & row = side ? rowB : rowA;
			const float scale = side ? t : ( 1.0f - t );
			QModelIndex iW = model->getIndex( row, "Bone Weights" );
			QModelIndex iI = model->getIndex( row, "Bone Indices" );
			for ( int j = 0; j < 4; j++ ) {
				const float w = model->get<float>( model->getIndex( iW, j ) ) * scale;
				if ( w > 0.0f )
					acc[int( model->get<quint8>( model->getIndex( iI, j ) ) )] += w;
			}
		}
		QVector<QPair<int, float>> weights;
		for ( auto it = acc.constBegin(); it != acc.constEnd(); ++it )
			weights.append( { it.key(), it.value() } );
		std::sort( weights.begin(), weights.end(),
			[]( const auto & a, const auto & b ) { return a.second > b.second; } );
		while ( weights.size() > 4 )
			weights.removeLast();
		float sum = 0.0f;
		for ( const auto & w : std::as_const( weights ) )
			sum += w.second;
		QModelIndex iW = model->getIndex( rowD, "Bone Weights" );
		QModelIndex iI = model->getIndex( rowD, "Bone Indices" );
		for ( int j = 0; j < 4; j++ ) {
			const bool on = ( j < weights.size() && sum > 0.0f );
			model->set<float>( model->getIndex( iW, j ), on ? weights.at( j ).second / sum : 0.0f );
			model->set<quint8>( model->getIndex( iI, j ), quint8( on ? weights.at( j ).first : 0 ) );
		}
	}
}

//! Three-source generalization of tlWriteLerpVertex: dst = u*va + v*vb + w*vc
//! (barycentric). Attributes interpolate the same way (normals/tangents
//! renormalized, bone weights merged / top-4 / renormalized).
static void tlWriteBaryVertex( NifModel * model, const QModelIndex & iShape,
	int dst, int va, int vb, int vc, const Vector3 & bary )
{
	QModelIndex iVD = model->getIndex( iShape, "Vertex Data" );
	QModelIndex rows[3] = { model->getIndex( iVD, va ), model->getIndex( iVD, vb ),
		model->getIndex( iVD, vc ) };
	QModelIndex rowD = model->getIndex( iVD, dst );
	if ( !rows[0].isValid() || !rows[1].isValid() || !rows[2].isValid() || !rowD.isValid() )
		return;
	tlCopyItemValues( model, rows[0], rowD );
	Vector3 p;
	for ( int k = 0; k < 3; k++ )
		p += model->get<Vector3>( rows[k], "Vertex" ) * bary[k];
	tlSetVertexLocal( model, iShape, dst, p );
	if ( model->getItem( rows[0], "UV" ) ) {
		Vector2 uv;
		for ( int k = 0; k < 3; k++ )
			uv += model->get<Vector2>( rows[k], "UV" ) * bary[k];
		model->set<HalfVector2>( rowD, "UV", HalfVector2( uv ) );
	}
	for ( const char * attr : { "Normal", "Tangent" } ) {
		if ( !model->getItem( rows[0], attr ) )
			continue;
		Vector3 v;
		for ( int k = 0; k < 3; k++ )
			v += model->get<Vector3>( rows[k], attr ) * bary[k];
		if ( v.squaredLength() > 1.0e-12f ) {
			v.normalize();
			model->set<ByteVector3>( rowD, attr, v );
		}
	}
	if ( model->getItem( rows[0], "Bone Weights" ) ) {
		QHash<int, float> acc;
		for ( int k = 0; k < 3; k++ ) {
			QModelIndex iW = model->getIndex( rows[k], "Bone Weights" );
			QModelIndex iI = model->getIndex( rows[k], "Bone Indices" );
			for ( int j = 0; j < 4; j++ ) {
				const float w = model->get<float>( model->getIndex( iW, j ) ) * bary[k];
				if ( w > 0.0f )
					acc[int( model->get<quint8>( model->getIndex( iI, j ) ) )] += w;
			}
		}
		QVector<QPair<int, float>> weights;
		for ( auto it = acc.constBegin(); it != acc.constEnd(); ++it )
			weights.append( { it.key(), it.value() } );
		std::sort( weights.begin(), weights.end(),
			[]( const auto & a, const auto & b ) { return a.second > b.second; } );
		while ( weights.size() > 4 )
			weights.removeLast();
		float sum = 0.0f;
		for ( const auto & w : std::as_const( weights ) )
			sum += w.second;
		QModelIndex iW = model->getIndex( rowD, "Bone Weights" );
		QModelIndex iI = model->getIndex( rowD, "Bone Indices" );
		for ( int j = 0; j < 4; j++ ) {
			const bool on = ( j < weights.size() && sum > 0.0f );
			model->set<float>( model->getIndex( iW, j ), on ? weights.at( j ).second / sum : 0.0f );
			model->set<quint8>( model->getIndex( iI, j ), quint8( on ? weights.at( j ).first : 0 ) );
		}
	}
}

//! In-place undo for operators that only APPEND verts/triangles and refresh
//! normals of existing verts (Fill, Bridge): redo runs the op's apply
//! closure, undo shrinks the arrays back and restores the saved normals and
//! Data Size — instant, no snapshot reload.
class TlMeshGrowCommand final : public QUndoCommand
{
public:
	TlMeshGrowCommand( NifModel * model, const QModelIndex & iShape,
		const QSet<int> & touchedVerts, const QString & text,
		std::function<void()> applyFn )
		: nif( model ), block( iShape ), apply( std::move( applyFn ) )
	{
		setText( text );
		oldNV = nif->get<int>( iShape, "Num Vertices" );
		oldNT = nif->get<int>( iShape, "Num Triangles" );
		oldDataSize = nif->get<int>( iShape, "Data Size" );
		QModelIndex iVD = nif->getIndex( iShape, "Vertex Data" );
		const bool hasNormals = iVD.isValid()
			&& nif->getItem( nif->getIndex( iVD, 0 ), "Normal" );
		if ( hasNormals )
			for ( int v : touchedVerts )
				if ( v >= 0 && v < oldNV )
					savedNrm.append( { v, nif->get<Vector3>( nif->getIndex( iVD, v ), "Normal" ) } );
	}

	void redo() override
	{
		if ( QModelIndex( block ).isValid() && apply )
			apply();
	}

	void undo() override
	{
		QModelIndex iShape( block );
		if ( !iShape.isValid() )
			return;
		nif->setState( BaseModel::Processing );
		QModelIndex iVD = nif->getIndex( iShape, "Vertex Data" );
		QModelIndex iTris = nif->getIndex( iShape, "Triangles" );
		nif->set<int>( iShape, "Num Triangles", oldNT );
		nif->updateArraySize( iTris );
		nif->set<int>( iShape, "Num Vertices", oldNV );
		nif->updateArraySize( iVD );
		for ( const auto & sn : std::as_const( savedNrm ) ) {
			QModelIndex row = nif->getIndex( iVD, sn.first );
			if ( nif->getItem( row, "Normal" ) )
				nif->set<ByteVector3>( row, "Normal", sn.second );
		}
		nif->set<int>( iShape, "Data Size", oldDataSize );
		nif->restoreState();
		nif->dataChanged( iShape, iShape );
	}

private:
	NifModel * nif;
	QPersistentModelIndex block;
	std::function<void()> apply;
	int oldNV = 0, oldNT = 0, oldDataSize = 0;
	QVector<QPair<int, Vector3>> savedNrm;
};

//! In-place undo for Extrude (no whole-model snapshot, so no reload flash on
//! Ctrl+Z or redo-panel scrubbing): redo applies the plan, undo shrinks the
//! arrays back and restores the re-pointed triangles, moved positions,
//! refreshed normals, Data Size and bounds.
class TlExtrudeCommand final : public QUndoCommand
{
public:
	//! applyFn: the actual mutation (extrude / inset — anything the plan's
	//! capture covers: appends + region re-point + moves of the touched verts)
	TlExtrudeCommand( NifModel * model, const QModelIndex & iShape,
		const TlExtrudePlan & extrudePlan, const QString & text,
		std::function<void()> applyFn )
		: nif( model ), block( iShape ), plan( extrudePlan ), apply( std::move( applyFn ) )
	{
		setText( text );
		// capture the before-state an in-place undo needs
		QModelIndex iVD = nif->getIndex( iShape, "Vertex Data" );
		QModelIndex iTris = nif->getIndex( iShape, "Triangles" );
		oldDataSize = nif->get<int>( iShape, "Data Size" );
		QModelIndex iBound = nif->getIndex( iShape, "Bounding Sphere" );
		if ( iBound.isValid() ) {
			boundCenter = nif->get<Vector3>( iBound, "Center" );
			boundRadius = nif->get<float>( iBound, "Radius" );
		}
		for ( int f : plan.repointFaces )
			savedTris.append( { f, nif->get<Triangle>( nif->getIndex( iTris, f ) ) } );
		QSet<int> touched;	// original verts the op moves / re-lights
		for ( int v : plan.capVerts )
			if ( v < plan.oldNV )
				touched << v;
		for ( int v : plan.dupVerts )
			touched << v;
		const bool hasNormals = iVD.isValid()
			&& nif->getItem( nif->getIndex( iVD, 0 ), "Normal" );
		for ( int v : touched ) {
			QModelIndex row = nif->getIndex( iVD, v );
			savedPos.append( { v, nif->get<Vector3>( row, "Vertex" ) } );
			if ( hasNormals )
				savedNrm.append( { v, nif->get<Vector3>( row, "Normal" ) } );
		}
	}

	void redo() override
	{
		if ( QModelIndex( block ).isValid() && apply )
			apply();
	}

	void undo() override
	{
		QModelIndex iShape( block );
		if ( !iShape.isValid() )
			return;
		nif->setState( BaseModel::Processing );
		QModelIndex iVD = nif->getIndex( iShape, "Vertex Data" );
		QModelIndex iTris = nif->getIndex( iShape, "Triangles" );
		// drop the appended geometry (walls/scaffolds first, then the dup verts)
		nif->set<int>( iShape, "Num Triangles", plan.oldNT );
		nif->updateArraySize( iTris );
		nif->set<int>( iShape, "Num Vertices", plan.oldNV );
		nif->updateArraySize( iVD );
		// restore the re-pointed region faces
		for ( const auto & st : std::as_const( savedTris ) )
			nif->set<Triangle>( nif->getIndex( iTris, st.first ), st.second );
		// restore moved positions and refreshed normals of the original verts
		for ( const auto & sp : std::as_const( savedPos ) )
			tlSetVertexLocal( nif, iShape, sp.first, sp.second );
		for ( const auto & sn : std::as_const( savedNrm ) ) {
			QModelIndex row = nif->getIndex( iVD, sn.first );
			if ( nif->getItem( row, "Normal" ) )
				nif->set<ByteVector3>( row, "Normal", sn.second );
		}
		nif->set<int>( iShape, "Data Size", oldDataSize );
		QModelIndex iBound = nif->getIndex( iShape, "Bounding Sphere" );
		if ( iBound.isValid() ) {
			nif->set<Vector3>( iBound, "Center", boundCenter );
			nif->set<float>( iBound, "Radius", boundRadius );
		}
		nif->restoreState();
		nif->dataChanged( iShape, iShape );
	}

private:
	NifModel * nif;
	QPersistentModelIndex block;
	TlExtrudePlan plan;
	std::function<void()> apply;
	int oldDataSize = 0;
	Vector3 boundCenter;
	float boundRadius = 0.0f;
	QVector<QPair<int, Triangle>> savedTris;
	QVector<QPair<int, Vector3>> savedPos;
	QVector<QPair<int, Vector3>> savedNrm;
};


void GLView::extrudeRegion()
{
	if ( !model || !editMode || pickedElems.isEmpty() ) {
		emit gizmoStatus( tr( "Extrude needs a selection in edit mode" ) );
		return;
	}
	if ( gizmoMode != 0 )
		return;
	// v1: one shape at a time
	const int sb = pickedElems.first().shapeBlock;
	for ( const PickedElement & pe : std::as_const( pickedElems ) ) {
		if ( pe.shapeBlock != sb ) {
			emit gizmoStatus( tr( "Extrude works on one mesh at a time" ) );
			return;
		}
	}
	QModelIndex iShape = model->getBlockIndex( sb );
	if ( !model->blockInherits( iShape, "BSTriShape" ) ) {
		emit gizmoStatus( tr( "Extrude is supported on FO4 (BSTriShape) meshes only" ) );
		return;
	}
	const QSet<int> sv = pickedVertexRefs().value( sb );
	QSet<int> sfx;
	QVector<QPair<int, int>> sedges;
	for ( const PickedElement & pe : std::as_const( pickedElems ) ) {
		if ( pe.type == 3 )
			sfx << pe.e0;
		else if ( pe.type == 2 )
			sedges.append( { pe.e0, pe.e1 } );
	}

	TlExtrudePlan plan;
	QString err;
	if ( !tlExtrudePlanBuild( model, iShape, sv, sfx, sedges, plan, err ) ) {
		emit gizmoStatus( err );
		return;
	}

	extrudeSeed = pickedElems;
	extrudeUndoIndexBase = ( model->undoStack ) ? model->undoStack->index() : -1;
	extrudeTouchedShape = sb;
	extrudeTouchedVerts = plan.capVerts;
	for ( int v : std::as_const( plan.dupVerts ) )
		extrudeTouchedVerts << v;
	// in-place undo command (push applies via redo): Ctrl+Z and redo-panel
	// scrubbing stay instant, with no whole-model snapshot reload
	const QPersistentModelIndex pShapeEx( iShape );
	if ( model->undoStack )
		model->undoStack->push( new TlExtrudeCommand( model, iShape, plan, tr( "Extrude" ),
			[this, pShapeEx, plan]() {
				tlExtrudeApplyPlan( model, QModelIndex( pShapeEx ), plan, Vector3(), false );
			} ) );
	else
		tlExtrudeApplyPlan( model, iShape, plan, Vector3(), false );

	// select the cap: verts/edges remap onto the duplicates, region faces were
	// re-pointed in place so face picks stay valid
	auto capSelection = [this]( const TlExtrudePlan & p ) {
		QVector<PickedElement> sel;
		sel.reserve( lastOpExSeed.size() + extrudeSeed.size() );
		const QVector<PickedElement> & src = extrudeSeed;
		for ( const PickedElement & pe : src ) {
			PickedElement np = pe;
			if ( pe.type == 1 ) {
				np.e0 = p.vremap.value( pe.e0, pe.e0 );
			} else if ( pe.type == 2 ) {
				np.e0 = p.vremap.value( pe.e0, pe.e0 );
				np.e1 = p.vremap.value( pe.e1, pe.e1 );
			}
			sel.append( np );
		}
		return sel;
	};
	pickedElems = capSelection( plan );
	modelChanged();

	// the re-run callback rebuilds the plan from the seed selection and applies
	// the offset inside one snapshot (proper normals, single undo entry)
	lastOpExRerun = [this, sb, sv, sfx, sedges, capSelection]( const QVector<TlOpParam> & ps ) {
		QModelIndex iS = model->getBlockIndex( sb );
		TlExtrudePlan p2;
		QString e2;
		if ( !iS.isValid() || !tlExtrudePlanBuild( model, iS, sv, sfx, sedges, p2, e2 ) )
			return;
		const Vector3 world( float( ps.value( 0 ).value ), float( ps.value( 1 ).value ),
			float( ps.value( 2 ).value ) );
		const bool flip = ( ps.value( 3 ).value != 0.0 );
		const Vector3 local = tlWorldToLocalDelta( scene, model, sb, world );
		const QPersistentModelIndex pS( iS );
		if ( model->undoStack )
			model->undoStack->push( new TlExtrudeCommand( model, iS, p2, tr( "Extrude" ),
				[this, pS, p2, local, flip]() {
					tlExtrudeApplyPlan( model, QModelIndex( pS ), p2, local, flip );
				} ) );
		else
			tlExtrudeApplyPlan( model, iS, p2, local, flip );
		pickedElems = capSelection( p2 );
		modelChanged();
	};

	// chain a modal move on the cap; its commit/cancel arms the redo panel
	extrudeChainArmed = true;
	if ( !gizmoBeginElement( 1 ) ) {
		extrudeChainArmed = false;
		armExtrudeRedoPanel( Vector3() );
	}
	emit gizmoStatus( tr( "Extruded %1 vert(s), %2 wall tri(s) - move, Esc leaves in place" )
		.arg( plan.dupVerts.size() ).arg( plan.walls.size() * 2 ) );
}

void GLView::armExtrudeRedoPanel( const Vector3 & worldDelta )
{
	QVector<TlOpParam> ps( 4 );
	static const char * axisNames[3] = { QT_TR_NOOP( "Move X" ), "Y", "Z" };
	for ( int i = 0; i < 3; i++ ) {
		ps[i].label = tr( axisNames[i] );
		ps[i].type = TlOpParam::Float;
		ps[i].value = double( worldDelta[i] );
		ps[i].step = 0.01;
		ps[i].decimals = 4;
	}
	ps[3].label = tr( "Flip Normals" );
	ps[3].type = TlOpParam::Bool;
	ps[3].value = 0.0;
	const int steps = ( model && model->undoStack && extrudeUndoIndexBase >= 0 )
		? model->undoStack->index() - extrudeUndoIndexBase : 1;
	armOperatorPanelEx( tr( "Extrude Region and Move" ), ps, std::max( steps, 1 ), extrudeSeed );
	// NOTE: no eager consolidation — the chained move already refreshed the
	// normals inside its own transaction, and with TlExtrudeCommand both undo
	// paths are in-place anyway (no snapshot, no reload flash).
}

//! Cap one closed rim loop (ear-clip in the loop's best-fit plane). The loop
//! order follows the hole direction, so loop-order winding faces like the
//! surrounding surface; flip reverses it. Runs under Processing and emits one
//! dataChanged (call from TlMeshGrowCommand::redo).
static void tlFillApply( NifModel * model, const QModelIndex & iShape,
	const QVector<int> & loop, bool flipNormals )
{
	QModelIndex iVD = model->getIndex( iShape, "Vertex Data" );
	QModelIndex iTris = model->getIndex( iShape, "Triangles" );
	if ( !iVD.isValid() || !iTris.isValid() || loop.size() < 3 )
		return;
	QVector<Vector3> pos( loop.size() );
	for ( int i = 0; i < loop.size(); i++ )
		pos[i] = model->get<Vector3>( model->getIndex( iVD, loop.at( i ) ), "Vertex" );
	const QVector<Triangle> cap = tlEarClip( pos );	// loop-order indices
	if ( cap.isEmpty() )
		return;
	model->setState( BaseModel::Processing );
	const int numVerts = model->get<int>( iShape, "Num Vertices" );
	const int oldNT = model->get<int>( iShape, "Num Triangles" );
	const int dataSize = model->get<int>( iShape, "Data Size" );
	const int stride = ( numVerts > 0 ) ? ( dataSize - oldNT * 6 ) / numVerts : 0;
	model->set<int>( iShape, "Num Triangles", oldNT + cap.size() );
	model->updateArraySize( iTris );
	for ( int t = 0; t < cap.size(); t++ ) {
		Triangle m( quint16( loop.at( cap[t][0] ) ), quint16( loop.at( cap[t][1] ) ),
			quint16( loop.at( cap[t][2] ) ) );
		if ( flipNormals )
			std::swap( m[1], m[2] );
		model->set<Triangle>( model->getIndex( iTris, oldNT + t ), m );
	}
	if ( stride > 0 )
		model->set<int>( iShape, "Data Size",
			numVerts * stride + ( oldNT + cap.size() ) * 6 );
	tlRecalcNormalsSubset( model, iShape,
		QSet<int>( loop.constBegin(), loop.constEnd() ) );
	model->restoreState();
	model->dataChanged( QModelIndex( iShape ), QModelIndex( iShape ) );
}

//! Connect two rim loops with a band of triangles (Blender's Bridge Edge
//! Loops). Handles unequal vertex counts via an arc-length zip; cuts insert
//! interpolated rings (equal-count loops only — weights/UVs lerp per pair).
//! Returns a short status note. Runs under Processing, one dataChanged.
static QString tlBridgeApply( NifModel * model, const QModelIndex & iShape,
	const TlLoop & loopA, const TlLoop & loopB, int cuts, int twist, bool flipNormals )
{
	QModelIndex iVD = model->getIndex( iShape, "Vertex Data" );
	QModelIndex iTris = model->getIndex( iShape, "Triangles" );
	if ( !iVD.isValid() || !iTris.isValid() )
		return QString();
	auto readPos = [&]( int v ) {
		return model->get<Vector3>( model->getIndex( iVD, v ), "Vertex" );
	};

	// A in SURFACE order (the band is a wall like extrude's: edge a->b of the
	// surface pairs with the far ring); B aligned to follow A spatially.
	QVector<int> A = loopA.verts;
	std::reverse( A.begin(), A.end() );
	QVector<int> B = loopB.verts;
	const bool closed = loopA.closed;	// caller guarantees both match

	if ( closed && !B.isEmpty() ) {
		// start B at its vertex nearest to A's start (+ twist)
		int best = 0;
		float bestD = 1.0e30f;
		const Vector3 a0 = readPos( A.first() );
		for ( int j = 0; j < B.size(); j++ ) {
			const float d = ( readPos( B.at( j ) ) - a0 ).squaredLength();
			if ( d < bestD ) {
				bestD = d;
				best = j;
			}
		}
		const int off = ( ( best + twist ) % B.size() + B.size() ) % B.size();
		QVector<int> rot;
		rot.reserve( B.size() );
		for ( int j = 0; j < B.size(); j++ )
			rot.append( B.at( ( off + j ) % B.size() ) );
		B = rot;
	}
	// direction: sample a few fractional positions and keep the orientation
	// with the smaller total distance
	auto sampleDist = [&]( const QVector<int> & bb ) {
		float sum = 0.0f;
		for ( float s : { 0.0f, 0.25f, 0.5f, 0.75f } ) {
			const int ia = int( s * ( A.size() - 1 ) + 0.5f );
			const int ib = int( s * ( bb.size() - 1 ) + 0.5f );
			sum += ( readPos( A.at( ia ) ) - readPos( bb.at( ib ) ) ).squaredLength();
		}
		return sum;
	};
	{
		QVector<int> rev = B;
		std::reverse( rev.begin(), rev.end() );
		if ( closed && rev.size() > 1 ) {
			// keep the same start vertex after reversing a ring
			rev.prepend( rev.takeLast() );
		}
		if ( sampleDist( rev ) < sampleDist( B ) )
			B = rev;
	}

	const int nA = A.size(), nB = B.size();
	QString note;
	if ( cuts > 0 && nA != nB ) {
		note = GLView::tr( " (cuts need equal loop lengths: %1 vs %2)" ).arg( nA ).arg( nB );
		cuts = 0;
	}
	const int oldNV = model->get<int>( iShape, "Num Vertices" );
	if ( cuts > 0 && oldNV + cuts * nA > 0xFFFF ) {
		cuts = std::max( 0, ( 0xFFFF - oldNV ) / std::max( nA, 1 ) );
		note = GLView::tr( " (cuts clamped by the 65,535-vertex limit)" );
	}

	model->setState( BaseModel::Processing );
	const int oldNT = model->get<int>( iShape, "Num Triangles" );
	const int dataSize = model->get<int>( iShape, "Data Size" );
	const int stride = ( oldNV > 0 ) ? ( dataSize - oldNT * 6 ) / oldNV : 0;

	// interpolated rings for the cuts (equal counts guaranteed here)
	QVector<QVector<int>> rings;
	rings.append( A );
	if ( cuts > 0 ) {
		model->set<int>( iShape, "Num Vertices", oldNV + cuts * nA );
		model->updateArraySize( iVD );
		int nv = oldNV;
		for ( int r = 1; r <= cuts; r++ ) {
			const float t = float( r ) / float( cuts + 1 );
			QVector<int> ring( nA );
			for ( int i = 0; i < nA; i++ ) {
				tlWriteLerpVertex( model, iShape, nv, A.at( i ), B.at( i ), t );
				ring[i] = nv++;
			}
			rings.append( ring );
		}
	}
	rings.append( B );

	// band triangles between consecutive rings
	QVector<Triangle> band;
	for ( int r = 0; r + 1 < rings.size(); r++ ) {
		const QVector<int> & bot = rings.at( r );
		const QVector<int> & top = rings.at( r + 1 );
		if ( bot.size() == top.size() ) {
			// equal counts: one quad (a,b,b') + (a,b',a') per edge
			const int n = bot.size();
			const int segs = closed ? n : n - 1;
			for ( int i = 0; i < segs; i++ ) {
				const quint16 a = quint16( bot.at( i ) );
				const quint16 b = quint16( bot.at( ( i + 1 ) % n ) );
				const quint16 a2 = quint16( top.at( i ) );
				const quint16 b2 = quint16( top.at( ( i + 1 ) % n ) );
				band.append( Triangle( a, b, b2 ) );
				band.append( Triangle( a, b2, a2 ) );
			}
		} else {
			// unequal: zip by normalized arc length
			auto params = [&]( const QVector<int> & loop ) {
				QVector<float> p( loop.size() + ( closed ? 1 : 0 ), 0.0f );
				float total = 0.0f;
				for ( int i = 1; i < p.size(); i++ ) {
					total += ( readPos( loop.at( i % loop.size() ) )
						- readPos( loop.at( i - 1 ) ) ).length();
					p[i] = total;
				}
				if ( total > 1.0e-9f )
					for ( float & v : p )
						v /= total;
				return p;
			};
			const QVector<float> pa = params( bot );
			const QVector<float> pb = params( top );
			const int endA = closed ? bot.size() : bot.size() - 1;
			const int endB = closed ? top.size() : top.size() - 1;
			int i = 0, j = 0;
			while ( i < endA || j < endB ) {
				bool advanceA;
				if ( i >= endA )
					advanceA = false;
				else if ( j >= endB )
					advanceA = true;
				else
					advanceA = ( pa.at( i + 1 ) <= pb.at( j + 1 ) );
				const quint16 ai = quint16( bot.at( i % bot.size() ) );
				const quint16 bj = quint16( top.at( j % top.size() ) );
				if ( advanceA ) {
					band.append( Triangle( ai, quint16( bot.at( ( i + 1 ) % bot.size() ) ), bj ) );
					i++;
				} else {
					band.append( Triangle( ai, quint16( top.at( ( j + 1 ) % top.size() ) ), bj ) );
					j++;
				}
			}
		}
	}
	if ( flipNormals )
		for ( Triangle & t : band )
			std::swap( t[1], t[2] );

	const int newNV = model->get<int>( iShape, "Num Vertices" );
	model->set<int>( iShape, "Num Triangles", oldNT + band.size() );
	model->updateArraySize( iTris );
	for ( int t = 0; t < band.size(); t++ )
		model->set<Triangle>( model->getIndex( iTris, oldNT + t ), band.at( t ) );
	if ( stride > 0 )
		model->set<int>( iShape, "Data Size",
			newNV * stride + ( oldNT + band.size() ) * 6 );

	QSet<int> touched;
	for ( const QVector<int> & ring : std::as_const( rings ) )
		for ( int v : ring )
			touched << v;
	tlRecalcNormalsSubset( model, iShape, touched );
	model->restoreState();
	model->dataChanged( QModelIndex( iShape ), QModelIndex( iShape ) );
	return note;
}

void GLView::smartConnect()
{
	if ( !model || !editMode || pickedElems.isEmpty() ) {
		emit gizmoStatus( tr( "Fill/Bridge needs a rim selection in edit mode" ) );
		return;
	}
	if ( gizmoMode != 0 )
		return;
	const int sb = pickedElems.first().shapeBlock;
	for ( const PickedElement & pe : std::as_const( pickedElems ) ) {
		if ( pe.shapeBlock != sb ) {
			emit gizmoStatus( tr( "Fill/Bridge works on one mesh at a time (Join first)" ) );
			return;
		}
	}
	QModelIndex iShape = model->getBlockIndex( sb );
	if ( !model->blockInherits( iShape, "BSTriShape" ) ) {
		emit gizmoStatus( tr( "Fill/Bridge is supported on FO4 (BSTriShape) meshes only" ) );
		return;
	}
	const QSet<int> sv = pickedVertexRefs().value( sb );
	QVector<QPair<int, int>> sedges;
	for ( const PickedElement & pe : std::as_const( pickedElems ) )
		if ( pe.type == 2 && pe.e0 != pe.e1 )
			sedges.append( { pe.e0, pe.e1 } );

	QVector<TlLoop> loops;
	QString err;
	if ( !tlExtractLoops( model, iShape, sv, sedges, loops, err ) ) {
		emit gizmoStatus( err );
		return;
	}
	const QVector<PickedElement> seed = pickedElems;
	const QPersistentModelIndex pShape( iShape );

	if ( loops.size() == 1 && loops.first().closed && loops.first().verts.size() >= 3 ) {
		// ---- Fill ----
		const QVector<int> loop = loops.first().verts;
		const QSet<int> touched( loop.constBegin(), loop.constEnd() );
		if ( model->undoStack ) {
			const int base = model->undoStack->index();
			model->undoStack->push( new TlMeshGrowCommand( model, iShape, touched, tr( "Fill" ),
				[this, pShape, loop]() { tlFillApply( model, QModelIndex( pShape ), loop, false ); } ) );
			modelChanged();
			emit gizmoStatus( tr( "Filled the hole with %1 triangle(s)" ).arg( loop.size() - 2 ) );
			lastOpExRerun = [this, sb, sv, sedges, pShape]( const QVector<TlOpParam> & ps ) {
				QModelIndex iS( pShape );
				QVector<TlLoop> l2;
				QString e2;
				if ( !iS.isValid() || !tlExtractLoops( model, iS, sv, sedges, l2, e2 )
					|| l2.size() != 1 || !l2.first().closed )
					return;
				const QVector<int> lv = l2.first().verts;
				const QSet<int> t2( lv.constBegin(), lv.constEnd() );
				const bool flip = ( ps.value( 0 ).value != 0.0 );
				model->undoStack->push( new TlMeshGrowCommand( model, iS, t2, tr( "Fill" ),
					[this, pShape, lv, flip]() { tlFillApply( model, QModelIndex( pShape ), lv, flip ); } ) );
				modelChanged();
			};
			QVector<TlOpParam> ps( 1 );
			ps[0].label = tr( "Flip Normals" );
			ps[0].type = TlOpParam::Bool;
			ps[0].value = 0.0;
			armOperatorPanelEx( tr( "Fill" ), ps, model->undoStack->index() - base, seed );
		}
	} else if ( loops.size() == 2 ) {
		// ---- Bridge Edge Loops ----
		if ( loops.at( 0 ).closed != loops.at( 1 ).closed ) {
			emit gizmoStatus( tr( "Bridge: the two rims must both be closed rings or both open runs" ) );
			return;
		}
		const TlLoop la = loops.at( 0 ), lb = loops.at( 1 );
		QSet<int> touched;
		for ( int v : la.verts ) touched << v;
		for ( int v : lb.verts ) touched << v;
		if ( model->undoStack ) {
			const int base = model->undoStack->index();
			model->undoStack->push( new TlMeshGrowCommand( model, iShape, touched, tr( "Bridge Edge Loops" ),
				[this, pShape, la, lb]() { tlBridgeApply( model, QModelIndex( pShape ), la, lb, 0, 0, false ); } ) );
			modelChanged();
			emit gizmoStatus( tr( "Bridged the loops (%1 + %2 rim verts)" )
				.arg( la.verts.size() ).arg( lb.verts.size() ) );
			lastOpExRerun = [this, sb, sv, sedges, pShape]( const QVector<TlOpParam> & ps ) {
				QModelIndex iS( pShape );
				QVector<TlLoop> l2;
				QString e2;
				if ( !iS.isValid() || !tlExtractLoops( model, iS, sv, sedges, l2, e2 )
					|| l2.size() != 2 || l2.at( 0 ).closed != l2.at( 1 ).closed )
					return;
				const int cuts = std::clamp( int( ps.value( 0 ).value + 0.5 ), 0, 64 );
				const int twist = int( ps.value( 1 ).value );
				const bool flip = ( ps.value( 2 ).value != 0.0 );
				QSet<int> t2;
				for ( int v : l2.at( 0 ).verts ) t2 << v;
				for ( int v : l2.at( 1 ).verts ) t2 << v;
				const TlLoop a2 = l2.at( 0 ), b2 = l2.at( 1 );
				model->undoStack->push( new TlMeshGrowCommand( model, iS, t2, tr( "Bridge Edge Loops" ),
					[this, pShape, a2, b2, cuts, twist, flip]() {
						tlBridgeApply( model, QModelIndex( pShape ), a2, b2, cuts, twist, flip );
					} ) );
				modelChanged();
			};
			QVector<TlOpParam> ps( 3 );
			ps[0].label = tr( "Number of Cuts" );
			ps[0].type = TlOpParam::Int;
			ps[0].value = 0.0;
			ps[0].mn = 0.0;
			ps[0].mx = 64.0;
			ps[0].step = 1.0;
			ps[1].label = tr( "Twist" );
			ps[1].type = TlOpParam::Int;
			ps[1].value = 0.0;
			ps[1].mn = -64.0;
			ps[1].mx = 64.0;
			ps[1].step = 1.0;
			ps[2].label = tr( "Flip Normals" );
			ps[2].type = TlOpParam::Bool;
			ps[2].value = 0.0;
			armOperatorPanelEx( tr( "Bridge Edge Loops" ), ps, model->undoStack->index() - base, seed );
		}
	} else {
		emit gizmoStatus( tr( "Fill/Bridge: select ONE closed rim (fill) or TWO rims (bridge) — found %1 loop(s)" )
			.arg( loops.size() ) );
	}
}

// ---------------------------------------------------------------------------
// Edge Slide (Shift+V) / Smooth / Flip & Recalc Normals — value-level ops

//! Push position writes as ChangeValueCommands merged into one transaction
//! (the gizmo-commit pattern): instant in-place undo, no snapshot.
static void tlPushPositionCommands( NifModel * model, const QModelIndex & iShape,
	const QVector<QPair<int, Vector3>> & targets )
{
	if ( !model || !model->undoStack )
		return;
	for ( const auto & tg : targets ) {
		QModelIndex vIdx = tlVertexValueIndex( model, iShape, tg.first );
		const NifItem * item = vIdx.isValid()
			? static_cast<const NifItem *>( vIdx.internalPointer() ) : nullptr;
		if ( !item )
			continue;
		NifValue oldVal = item->value();
		NifValue newVal = oldVal;
		if ( item->hasValueType( NifValue::tHalfVector3 ) )
			newVal.set<HalfVector3>( HalfVector3( tg.second ), model, item );
		else
			newVal.set<Vector3>( tg.second, model, item );
		if ( !( oldVal == newVal ) )
			model->undoStack->push( new ChangeValueCommand( vIdx, oldVal, newVal,
				GLView::tr( "Vertex" ), model ) );
	}
}

//! Shared front matter of the single-shape vertex operators: validates the
//! selection and returns the shape block + its selected verts.
bool GLView::vertexOpTarget( int & sb, QSet<int> & sv, const char * opName )
{
	if ( !model || !editMode || pickedElems.isEmpty() ) {
		emit gizmoStatus( tr( "%1 needs a selection in edit mode" ).arg( QLatin1String( opName ) ) );
		return false;
	}
	if ( gizmoMode != 0 )
		return false;
	sb = pickedElems.first().shapeBlock;
	for ( const PickedElement & pe : std::as_const( pickedElems ) ) {
		if ( pe.shapeBlock != sb ) {
			emit gizmoStatus( tr( "%1 works on one mesh at a time" ).arg( QLatin1String( opName ) ) );
			return false;
		}
	}
	QModelIndex iShape = model->getBlockIndex( sb );
	if ( !model->blockInherits( iShape, "BSTriShape" ) ) {
		emit gizmoStatus( tr( "%1 is supported on FO4 (BSTriShape) meshes only" ).arg( QLatin1String( opName ) ) );
		return false;
	}
	sv = pickedVertexRefs().value( sb );
	if ( sv.isEmpty() ) {
		emit gizmoStatus( tr( "%1: no vertices in the selection" ).arg( QLatin1String( opName ) ) );
		return false;
	}
	return true;
}

void GLView::edgeSlide()
{
	int sb = -1;
	QSet<int> sv;
	if ( !vertexOpTarget( sb, sv, "Edge Slide" ) )
		return;
	QModelIndex iShape = model->getBlockIndex( sb );
	QModelIndex iVD = model->getIndex( iShape, "Vertex Data" );
	QModelIndex iTris = model->getIndex( iShape, "Triangles" );
	const int numTris = model->get<int>( iShape, "Num Triangles" );

	// unselected neighbors per selected vert
	QHash<int, QSet<int>> nbr;
	for ( int t = 0; t < numTris; t++ ) {
		Triangle tri = model->get<Triangle>( model->getIndex( iTris, t ) );
		if ( tri[0] == tri[1] || tri[1] == tri[2] || tri[0] == tri[2] )
			continue;
		for ( int e = 0; e < 3; e++ ) {
			const int a = tri[e], b = tri[( e + 1 ) % 3];
			if ( sv.contains( a ) && !sv.contains( b ) )
				nbr[a] << b;
			if ( sv.contains( b ) && !sv.contains( a ) )
				nbr[b] << a;
		}
	}
	auto readPos = [&]( int v ) {
		return model->get<Vector3>( model->getIndex( iVD, v ), "Vertex" );
	};
	// per vert: the two most-opposite unselected neighbors; a global reference
	// direction keeps the positive side consistent across the loop
	struct Slide { int v; Vector3 orig, plus, minus; };
	QVector<Slide> slides;
	Vector3 ref;
	for ( int v : std::as_const( sv ) ) {
		const QSet<int> & cands = nbr.value( v );
		if ( cands.isEmpty() )
			continue;
		const Vector3 p = readPos( v );
		int u1 = -1, u2 = -1;
		float worst = 2.0f;
		if ( cands.size() == 1 ) {
			u1 = *cands.constBegin();
		} else {
			for ( int a : cands )
				for ( int b : cands ) {
					if ( a >= b )
						continue;
					Vector3 da = readPos( a ) - p, db = readPos( b ) - p;
					da.normalize();
					db.normalize();
					const float d = Vector3::dotproduct( da, db );
					if ( d < worst ) {
						worst = d;
						u1 = a;
						u2 = b;
					}
				}
		}
		if ( u1 < 0 )
			continue;
		Vector3 d1 = readPos( u1 ) - p;
		if ( ref.squaredLength() < 1.0e-12f )
			ref = d1;
		if ( u2 >= 0 && Vector3::dotproduct( readPos( u2 ) - p, ref )
			> Vector3::dotproduct( d1, ref ) )
			std::swap( u1, u2 );
		Slide s;
		s.v = v;
		s.orig = p;
		s.plus = readPos( u1 );
		s.minus = ( u2 >= 0 ) ? readPos( u2 ) : p;
		slides.append( s );
	}
	if ( slides.isEmpty() ) {
		emit gizmoStatus( tr( "Edge Slide: the selection has no unselected neighbors to slide along" ) );
		return;
	}

	const QPersistentModelIndex pShape( iShape );
	QSet<int> touched = sv;
	for ( auto it = nbr.constBegin(); it != nbr.constEnd(); ++it )
		for ( int n : it.value() )
			touched << n;
	auto applySlide = [this, pShape, slides, touched]( float f ) {
		QModelIndex iS( pShape );
		if ( !iS.isValid() )
			return;
		ChangeValueCommand::createTransaction();
		QVector<QPair<int, Vector3>> targets;
		targets.reserve( slides.size() );
		for ( const Slide & s : slides ) {
			const Vector3 to = ( f >= 0.0f ) ? s.plus : s.minus;
			targets.append( { s.v, s.orig + ( to - s.orig ) * std::min( std::fabs( f ), 1.0f ) } );
		}
		tlPushPositionCommands( model, iS, targets );
		tlPushNormalCommands( model, iS, touched );
		modelChanged();
	};
	// factor 0 = armed at rest; scrub the panel to slide (the redo panel is
	// the modal here — same philosophy as the other operator panels)
	lastOpExRerun = [applySlide]( const QVector<TlOpParam> & ps ) {
		applySlide( float( ps.value( 0 ).value ) );
	};
	QVector<TlOpParam> ps( 1 );
	ps[0].label = tr( "Factor" );
	ps[0].type = TlOpParam::Float;
	ps[0].value = 0.0;
	ps[0].mn = -1.0;
	ps[0].mx = 1.0;
	ps[0].step = 0.02;
	ps[0].decimals = 3;
	armOperatorPanelEx( tr( "Edge Slide" ), ps, 0, pickedElems );
	emit gizmoStatus( tr( "Edge Slide armed: scrub Factor in the panel (%1 vert(s))" ).arg( slides.size() ) );
}

void GLView::smoothVertices()
{
	int sb = -1;
	QSet<int> sv;
	if ( !vertexOpTarget( sb, sv, "Smooth" ) )
		return;
	QModelIndex iShape = model->getBlockIndex( sb );
	QModelIndex iVD = model->getIndex( iShape, "Vertex Data" );
	QModelIndex iTris = model->getIndex( iShape, "Triangles" );
	const int numTris = model->get<int>( iShape, "Num Triangles" );
	const int numVerts = model->get<int>( iShape, "Num Vertices" );

	// adjacency + mesh boundary verts (kept fixed, Blender-style)
	QHash<int, QSet<int>> nbr;
	QHash<quint64, int> edgeUse;
	auto ekey = []( int a, int b ) {
		if ( a > b ) std::swap( a, b );
		return ( quint64( quint32( a ) ) << 32 ) | quint32( b );
	};
	for ( int t = 0; t < numTris; t++ ) {
		Triangle tri = model->get<Triangle>( model->getIndex( iTris, t ) );
		if ( tri[0] == tri[1] || tri[1] == tri[2] || tri[0] == tri[2] )
			continue;
		for ( int e = 0; e < 3; e++ ) {
			const int a = tri[e], b = tri[( e + 1 ) % 3];
			nbr[a] << b;
			nbr[b] << a;
			edgeUse[ekey( a, b )]++;
		}
	}
	QSet<int> boundary;
	for ( auto it = edgeUse.constBegin(); it != edgeUse.constEnd(); ++it )
		if ( it.value() == 1 ) {
			boundary << int( it.key() >> 32 );
			boundary << int( it.key() & 0xFFFFFFFFu );
		}
	QVector<Vector3> pos( numVerts );
	for ( int i = 0; i < numVerts; i++ )
		pos[i] = model->get<Vector3>( model->getIndex( iVD, i ), "Vertex" );

	const QPersistentModelIndex pShape( iShape );
	auto applySmooth = [this, pShape, sv, nbr, boundary, pos]( float factor, int iterations ) {
		QModelIndex iS( pShape );
		if ( !iS.isValid() )
			return;
		QVector<Vector3> work = pos;
		iterations = std::clamp( iterations, 1, 50 );
		factor = std::clamp( factor, 0.0f, 1.0f );
		for ( int pass = 0; pass < iterations; pass++ ) {
			QVector<Vector3> next = work;
			for ( int v : sv ) {
				if ( boundary.contains( v ) )
					continue;
				const QSet<int> & ns = nbr.value( v );
				if ( ns.isEmpty() )
					continue;
				Vector3 avg;
				for ( int n : ns )
					avg += work.at( n );
				avg /= float( ns.size() );
				next[v] = work.at( v ) + ( avg - work.at( v ) ) * factor;
			}
			work.swap( next );
		}
		ChangeValueCommand::createTransaction();
		QVector<QPair<int, Vector3>> targets;
		for ( int v : sv )
			if ( !boundary.contains( v ) )
				targets.append( { v, work.at( v ) } );
		tlPushPositionCommands( model, iS, targets );
		QSet<int> touched = sv;
		for ( int v : sv )
			for ( int n : nbr.value( v ) )
				touched << n;
		tlPushNormalCommands( model, iS, touched );
		modelChanged();
	};
	applySmooth( 0.5f, 1 );
	lastOpExRerun = [applySmooth]( const QVector<TlOpParam> & ps ) {
		applySmooth( float( ps.value( 0 ).value ), int( ps.value( 1 ).value + 0.5 ) );
	};
	QVector<TlOpParam> ps( 2 );
	ps[0].label = tr( "Factor" );
	ps[0].type = TlOpParam::Float;
	ps[0].value = 0.5;
	ps[0].mn = 0.0;
	ps[0].mx = 1.0;
	ps[0].step = 0.05;
	ps[0].decimals = 3;
	ps[1].label = tr( "Iterations" );
	ps[1].type = TlOpParam::Int;
	ps[1].value = 1.0;
	ps[1].mn = 1.0;
	ps[1].mx = 50.0;
	ps[1].step = 1.0;
	armOperatorPanelEx( tr( "Smooth Vertices" ), ps, 1, pickedElems );
	emit gizmoStatus( tr( "Smoothed %1 vert(s)" ).arg( sv.size() ) );
}

void GLView::flipSelectedFaces()
{
	int sb = -1;
	QSet<int> sv;
	if ( !vertexOpTarget( sb, sv, "Flip Normals" ) )
		return;
	QModelIndex iShape = model->getBlockIndex( sb );
	QModelIndex iTris = model->getIndex( iShape, "Triangles" );
	const int numTris = model->get<int>( iShape, "Num Triangles" );
	QSet<int> faces;
	for ( const PickedElement & pe : std::as_const( pickedElems ) )
		if ( pe.type == 3 && pe.e0 >= 0 && pe.e0 < numTris )
			faces << pe.e0;
	for ( int t = 0; t < numTris; t++ ) {
		Triangle tri = model->get<Triangle>( model->getIndex( iTris, t ) );
		if ( tri[0] == tri[1] || tri[1] == tri[2] || tri[0] == tri[2] )
			continue;
		if ( sv.contains( tri[0] ) && sv.contains( tri[1] ) && sv.contains( tri[2] ) )
			faces << t;
	}
	if ( faces.isEmpty() ) {
		emit gizmoStatus( tr( "Flip Normals: select whole faces" ) );
		return;
	}
	ChangeValueCommand::createTransaction();
	QSet<int> touched;
	for ( int t : std::as_const( faces ) ) {
		QModelIndex tIdx = model->getIndex( iTris, t );
		const NifItem * item = tIdx.isValid()
			? static_cast<const NifItem *>( tIdx.internalPointer() ) : nullptr;
		if ( !item )
			continue;
		Triangle tri = model->get<Triangle>( tIdx );
		touched << tri[0] << tri[1] << tri[2];
		std::swap( tri[1], tri[2] );
		NifValue oldVal = item->value();
		NifValue newVal = oldVal;
		newVal.set<Triangle>( tri, model, item );
		if ( !( oldVal == newVal ) )
			model->undoStack->push( new ChangeValueCommand( tIdx, oldVal, newVal, tr( "Triangle" ), model ) );
	}
	tlPushNormalCommands( model, iShape, touched );
	modelChanged();
	emit gizmoStatus( tr( "Flipped %1 face(s)" ).arg( faces.size() ) );
}

void GLView::recalcSelectedNormals()
{
	int sb = -1;
	QSet<int> sv;
	if ( !vertexOpTarget( sb, sv, "Recalculate Normals" ) )
		return;
	QModelIndex iShape = model->getBlockIndex( sb );
	ChangeValueCommand::createTransaction();
	tlPushNormalCommands( model, iShape, sv );
	modelChanged();
	emit gizmoStatus( tr( "Recalculated normals of %1 vert(s)" ).arg( sv.size() ) );
}

// ---------------------------------------------------------------------------
// Subdivide / Inset (I)

void GLView::subdivideSelection()
{
	int sb = -1;
	QSet<int> sv;
	if ( !vertexOpTarget( sb, sv, "Subdivide" ) )
		return;
	QModelIndex iShape = model->getBlockIndex( sb );
	QModelIndex iTris = model->getIndex( iShape, "Triangles" );
	const int numTris = model->get<int>( iShape, "Num Triangles" );
	auto ekey = []( int a, int b ) {
		if ( a > b ) std::swap( a, b );
		return ( quint64( quint32( a ) ) << 32 ) | quint32( b );
	};

	// marked edges: explicit edge picks; face picks contribute all 3 edges;
	// otherwise mesh edges with both endpoints selected
	QSet<quint64> marked;
	QSet<int> pickedFaces;
	for ( const PickedElement & pe : std::as_const( pickedElems ) ) {
		if ( pe.type == 2 && pe.e0 != pe.e1 )
			marked << ekey( pe.e0, pe.e1 );
		else if ( pe.type == 3 )
			pickedFaces << pe.e0;
	}
	QVector<Triangle> tris( numTris );
	for ( int t = 0; t < numTris; t++ )
		tris[t] = model->get<Triangle>( model->getIndex( iTris, t ) );
	auto degenerate = []( const Triangle & t ) {
		return t[0] == t[1] || t[1] == t[2] || t[0] == t[2];
	};
	for ( int t : std::as_const( pickedFaces ) ) {
		if ( t < 0 || t >= numTris || degenerate( tris.at( t ) ) )
			continue;
		for ( int e = 0; e < 3; e++ )
			marked << ekey( tris[t][e], tris[t][( e + 1 ) % 3] );
	}
	if ( marked.isEmpty() ) {
		for ( int t = 0; t < numTris; t++ ) {
			if ( degenerate( tris.at( t ) ) )
				continue;
			for ( int e = 0; e < 3; e++ ) {
				const int a = tris[t][e], b = tris[t][( e + 1 ) % 3];
				if ( sv.contains( a ) && sv.contains( b ) )
					marked << ekey( a, b );
			}
		}
	}
	if ( marked.isEmpty() ) {
		emit gizmoStatus( tr( "Subdivide: select edges or faces" ) );
		return;
	}

	// quad-aware (Blender): a marked-diagonal tri pair whose diagonal and all
	// four outer edges are marked subdivides as a QUAD — four sub-quads around
	// a center vertex on the diagonal midpoint; the diagonal itself is not cut
	struct QuadSub { int tA, tB; quint64 diag; int loop[4]; quint64 outer[4]; };
	QVector<QuadSub> quads;
	QSet<int> quadTris;
	{
		const QHash<int, int> & qmap = quadPartnerMap( sb );
		for ( auto qi = qmap.constBegin(); qi != qmap.constEnd(); ++qi ) {
			const int tA = qi.key(), tB = qi.value();
			if ( tA > tB || tA < 0 || tB < 0 || tA >= numTris || tB >= numTris )
				continue;	// each pair once
			const Triangle & ta = tris.at( tA );
			const Triangle & tb = tris.at( tB );
			if ( degenerate( ta ) || degenerate( tb ) )
				continue;
			// diagonal = the shared edge; c / d = the off-diagonal corners
			int a = -1, b = -1, c = -1, d = -1;
			for ( int i = 0; i < 3; i++ ) {
				const bool shared = ( ta[i] == tb[0] || ta[i] == tb[1] || ta[i] == tb[2] );
				if ( !shared )
					c = ta[i];
				else if ( a < 0 )
					a = ta[i];
				else
					b = ta[i];
			}
			for ( int i = 0; i < 3; i++ )
				if ( tb[i] != a && tb[i] != b )
					d = tb[i];
			if ( a < 0 || b < 0 || c < 0 || d < 0 || c == d )
				continue;
			QuadSub q;
			q.tA = tA;
			q.tB = tB;
			q.diag = ekey( a, b );
			if ( !marked.contains( q.diag ) )
				continue;
			// corner loop in tA's winding: c -> x -> d -> y keeps orientation
			const int ci = ( ta[0] == c ) ? 0 : ( ta[1] == c ) ? 1 : 2;
			q.loop[0] = c;
			q.loop[1] = ta[( ci + 1 ) % 3];
			q.loop[2] = d;
			q.loop[3] = ta[( ci + 2 ) % 3];
			bool all = true;
			for ( int i = 0; i < 4; i++ ) {
				q.outer[i] = ekey( q.loop[i], q.loop[( i + 1 ) % 4] );
				all = all && marked.contains( q.outer[i] );
			}
			if ( !all )
				continue;
			quads.append( q );
			quadTris << tA << tB;
		}
		for ( const QuadSub & q : std::as_const( quads ) )
			marked.remove( q.diag );
	}

	const int oldNV = model->get<int>( iShape, "Num Vertices" );
	if ( oldNV + marked.size() + quads.size() > 0xFFFF ) {
		emit gizmoStatus( tr( "Subdivide: would exceed the 65,535-vertex limit" ) );
		return;
	}

	const QPersistentModelIndex pShape( iShape );
	const QVector<quint64> markedList( marked.constBegin(), marked.constEnd() );
	auto applySubdivide = [this, pShape, markedList, ekey, quads, quadTris]() {
		QModelIndex iS( pShape );
		if ( !iS.isValid() )
			return;
		QModelIndex iVD = model->getIndex( iS, "Vertex Data" );
		QModelIndex iT = model->getIndex( iS, "Triangles" );
		const int nv = model->get<int>( iS, "Num Vertices" );
		const int nt = model->get<int>( iS, "Num Triangles" );
		const int ds = model->get<int>( iS, "Data Size" );
		const int stride = ( nv > 0 ) ? ( ds - nt * 6 ) / nv : 0;
		QVector<Triangle> tv( nt );
		for ( int t = 0; t < nt; t++ )
			tv[t] = model->get<Triangle>( model->getIndex( iT, t ) );

		model->setState( BaseModel::Processing );
		// midpoint vert per marked edge, plus a center vert per quad
		model->set<int>( iS, "Num Vertices", nv + markedList.size() + quads.size() );
		model->updateArraySize( iVD );
		QHash<quint64, int> mid;
		int nvi = nv;
		for ( quint64 k : markedList ) {
			const int a = int( k >> 32 ), b = int( k & 0xFFFFFFFFu );
			tlWriteLerpVertex( model, iS, nvi, a, b, 0.5f );
			mid.insert( k, nvi++ );
		}
		const int centerBase = nvi;
		for ( const QuadSub & q : quads ) {
			tlWriteLerpVertex( model, iS, nvi,
				int( q.diag >> 32 ), int( q.diag & 0xFFFFFFFFu ), 0.5f );
			nvi++;
		}
		// re-split each affected triangle by its marked-edge count
		QVector<Triangle> out;
		out.reserve( nt * 2 );
		QSet<int> touched;
		for ( int t = 0; t < nt; t++ ) {
			if ( quadTris.contains( t ) )
				continue;	// re-emitted below as a quad grid
			const Triangle & tr = tv.at( t );
			if ( tr[0] == tr[1] || tr[1] == tr[2] || tr[0] == tr[2] ) {
				out.append( tr );
				continue;
			}
			int m[3];
			int count = 0;
			for ( int e = 0; e < 3; e++ ) {
				m[e] = mid.value( ekey( tr[e], tr[( e + 1 ) % 3] ), -1 );
				if ( m[e] >= 0 )
					count++;
			}
			if ( count == 0 ) {
				out.append( tr );
				continue;
			}
			touched << tr[0] << tr[1] << tr[2];
			for ( int e = 0; e < 3; e++ )
				if ( m[e] >= 0 )
					touched << m[e];
			const quint16 a = tr[0], b = tr[1], c = tr[2];
			const int mab = m[0], mbc = m[1], mca = m[2];
			if ( count == 3 ) {
				out.append( Triangle( a, quint16( mab ), quint16( mca ) ) );
				out.append( Triangle( quint16( mab ), b, quint16( mbc ) ) );
				out.append( Triangle( quint16( mca ), quint16( mbc ), c ) );
				out.append( Triangle( quint16( mab ), quint16( mbc ), quint16( mca ) ) );
			} else if ( count == 2 ) {
				// rotate so the two marked edges are (a,b) and (b,c)
				quint16 p = a, q = b, r = c;
				int m1 = mab, m2 = mbc;
				if ( mab >= 0 && mca >= 0 ) {
					p = c; q = a; r = b;
					m1 = mca; m2 = mab;
				} else if ( mbc >= 0 && mca >= 0 ) {
					p = b; q = c; r = a;
					m1 = mbc; m2 = mca;
				}
				out.append( Triangle( quint16( m1 ), q, quint16( m2 ) ) );
				out.append( Triangle( p, quint16( m1 ), quint16( m2 ) ) );
				out.append( Triangle( p, quint16( m2 ), r ) );
			} else {
				// rotate so the marked edge is (a,b)
				quint16 p = a, q = b, r = c;
				int m1 = mab;
				if ( mbc >= 0 ) {
					p = b; q = c; r = a;
					m1 = mbc;
				} else if ( mca >= 0 ) {
					p = c; q = a; r = b;
					m1 = mca;
				}
				out.append( Triangle( p, quint16( m1 ), r ) );
				out.append( Triangle( quint16( m1 ), q, r ) );
			}
		}
		// quad grids: 4 sub-quads (8 tris) around the center vertex, wound
		// like the original pair
		for ( int qi = 0; qi < quads.size(); qi++ ) {
			const QuadSub & q = quads.at( qi );
			const int X = centerBase + qi;
			int M[4];
			bool okAll = true;
			for ( int i = 0; i < 4; i++ ) {
				M[i] = mid.value( q.outer[i], -1 );
				okAll = okAll && M[i] >= 0;
			}
			if ( !okAll ) {	// cannot happen by construction; keep the originals
				out.append( tv.at( q.tA ) );
				out.append( tv.at( q.tB ) );
				continue;
			}
			for ( int i = 0; i < 4; i++ ) {
				const quint16 p0 = quint16( q.loop[i] ), p1 = quint16( M[i] );
				const quint16 p2 = quint16( X ), p3 = quint16( M[( i + 3 ) % 4] );
				out.append( Triangle( p0, p1, p3 ) );
				out.append( Triangle( p1, p2, p3 ) );
			}
			for ( int i = 0; i < 4; i++ )
				touched << q.loop[i] << M[i];
			touched << X;
		}
		model->set<int>( iS, "Num Triangles", out.size() );
		model->updateArraySize( iT );
		for ( int t = 0; t < out.size(); t++ )
			model->set<Triangle>( model->getIndex( iT, t ), out.at( t ) );
		if ( stride > 0 )
			model->set<int>( iS, "Data Size",
				( nv + int( markedList.size() ) + int( quads.size() ) ) * stride + out.size() * 6 );
		tlRecalcNormalsSubset( model, iS, touched );
		tlUpdateBounds( model, iS );
		model->restoreState();
		model->dataChanged( QModelIndex( iS ), QModelIndex( iS ) );
	};

	// re-record the quad marks against the new vertex count: surviving old
	// diagonals keep their indices (vertices are only appended), a cut
	// diagonal's mark dies with its edge, and each subdivided quad trades its
	// diagonal for the four sub-quad diagonals. Computed BEFORE the apply so
	// every input is pre-mutation state.
	QSet<quint64> newMarks = quadMarksFor( sb );
	const bool marksUpdate = !newMarks.isEmpty() || !quads.isEmpty();
	if ( marksUpdate ) {
		for ( auto it = newMarks.begin(); it != newMarks.end(); ) {
			if ( marked.contains( *it ) )
				it = newMarks.erase( it );
			else
				++it;
		}
		QHash<quint64, int> midIdx;
		for ( int i = 0; i < markedList.size(); i++ )
			midIdx.insert( markedList.at( i ), oldNV + i );
		for ( const QuadSub & q : std::as_const( quads ) ) {
			newMarks.remove( q.diag );
			int M[4];
			bool okAll = true;
			for ( int i = 0; i < 4; i++ ) {
				M[i] = midIdx.value( q.outer[i], -1 );
				okAll = okAll && M[i] >= 0;
			}
			if ( !okAll )
				continue;
			for ( int i = 0; i < 4; i++ )
				newMarks.insert( ekey( M[i], M[( i + 3 ) % 4] ) );
		}
	}

	if ( model->undoStack ) {
		if ( marksUpdate )
			model->undoStack->beginMacro( tr( "Subdivide" ) );
		model->undoStack->push( new TlShapeStateCommand( model, iShape, tr( "Subdivide" ), applySubdivide ) );
		if ( marksUpdate ) {
			setQuadMarks( sb, newMarks, tr( "Subdivide" ),
				oldNV + markedList.size() + quads.size() );
			model->undoStack->endMacro();
		}
	} else {
		applySubdivide();
	}
	modelChanged();
	emit gizmoStatus( quads.isEmpty()
		? tr( "Subdivided %1 edge(s)" ).arg( marked.size() )
		: tr( "Subdivided %1 edge(s) and %2 quad(s)" ).arg( marked.size() ).arg( quads.size() ) );
}

//! Inset the region described by an extrude plan: create it like a zero-offset
//! extrude, then pull the duplicated boundary inward (and the whole cap along
//! the region normal by depth).
static void tlInsetApply( NifModel * model, const QModelIndex & iShape,
	const TlExtrudePlan & plan, float thickness, float depth )
{
	tlExtrudeApplyPlan( model, iShape, plan, Vector3(), false );
	QModelIndex iVD = model->getIndex( iShape, "Vertex Data" );
	if ( !iVD.isValid() )
		return;
	auto readPos = [&]( int v ) {
		return model->get<Vector3>( model->getIndex( iVD, v ), "Vertex" );
	};
	// region centroid + average normal from the (re-pointed) cap faces
	QModelIndex iTris = model->getIndex( iShape, "Triangles" );
	Vector3 nrm;
	for ( int f : plan.repointFaces ) {
		Triangle tr = model->get<Triangle>( model->getIndex( iTris, f ) );
		nrm += Vector3::crossproduct( readPos( tr[1] ) - readPos( tr[0] ),
			readPos( tr[2] ) - readPos( tr[0] ) );
	}
	if ( nrm.squaredLength() > 1.0e-12f )
		nrm.normalize();
	Vector3 centroid;
	int cn = 0;
	for ( int v : plan.capVerts ) {
		centroid += readPos( v );
		cn++;
	}
	if ( cn > 0 )
		centroid /= float( cn );

	model->setState( BaseModel::Processing );
	QSet<int> dupSet;
	for ( auto it = plan.vremap.constBegin(); it != plan.vremap.constEnd(); ++it )
		dupSet << it.value();
	for ( int v : plan.capVerts ) {
		Vector3 p = readPos( v );
		Vector3 target = p + nrm * depth;
		if ( dupSet.contains( v ) ) {
			// duplicated boundary vert: pull inward, perpendicular to the normal
			Vector3 in = centroid - p;
			in -= nrm * Vector3::dotproduct( in, nrm );
			const float len = in.length();
			if ( len > 1.0e-6f )
				target += in * std::min( thickness / len, 1.0f );
		}
		tlSetVertexLocal( model, iShape, v, target );
	}
	QSet<int> touched = plan.capVerts;
	for ( int v : std::as_const( plan.dupVerts ) )
		touched << v;
	tlRecalcNormalsSubset( model, iShape, touched );
	tlUpdateBounds( model, iShape );
	model->restoreState();
	model->dataChanged( QModelIndex( iShape ), QModelIndex( iShape ) );
}

void GLView::insetRegion()
{
	int sb = -1;
	QSet<int> sv;
	if ( !vertexOpTarget( sb, sv, "Inset" ) )
		return;
	QModelIndex iShape = model->getBlockIndex( sb );
	QSet<int> sfx;
	for ( const PickedElement & pe : std::as_const( pickedElems ) )
		if ( pe.type == 3 )
			sfx << pe.e0;
	TlExtrudePlan plan;
	QString err;
	if ( !tlExtrudePlanBuild( model, iShape, sv, sfx, {}, plan, err ) ) {
		emit gizmoStatus( err );
		return;
	}
	if ( plan.repointFaces.isEmpty() ) {
		emit gizmoStatus( tr( "Inset needs a face region (select whole faces)" ) );
		return;
	}
	const QVector<PickedElement> seed = pickedElems;
	const QPersistentModelIndex pShape( iShape );
	if ( !model->undoStack )
		return;
	const int base = model->undoStack->index();
	model->undoStack->push( new TlExtrudeCommand( model, iShape, plan, tr( "Inset" ),
		[this, pShape, plan]() { tlInsetApply( model, QModelIndex( pShape ), plan, 0.0f, 0.0f ); } ) );
	modelChanged();
	lastOpExRerun = [this, sb, sv, sfx, pShape]( const QVector<TlOpParam> & ps ) {
		QModelIndex iS( pShape );
		TlExtrudePlan p2;
		QString e2;
		if ( !iS.isValid() || !tlExtrudePlanBuild( model, iS, sv, sfx, {}, p2, e2 )
			|| p2.repointFaces.isEmpty() )
			return;
		const float thickness = float( ps.value( 0 ).value );
		const float depth = float( ps.value( 1 ).value );
		model->undoStack->push( new TlExtrudeCommand( model, iS, p2, tr( "Inset" ),
			[this, pShape, p2, thickness, depth]() {
				tlInsetApply( model, QModelIndex( pShape ), p2, thickness, depth );
			} ) );
		modelChanged();
	};
	QVector<TlOpParam> ps( 2 );
	ps[0].label = tr( "Thickness" );
	ps[0].type = TlOpParam::Float;
	ps[0].value = 0.0;
	ps[0].mn = 0.0;
	ps[0].step = 0.01;
	ps[0].decimals = 4;
	ps[1].label = tr( "Depth" );
	ps[1].type = TlOpParam::Float;
	ps[1].value = 0.0;
	ps[1].step = 0.01;
	ps[1].decimals = 4;
	armOperatorPanelEx( tr( "Inset Faces" ), ps, model->undoStack->index() - base, seed );
	emit gizmoStatus( tr( "Inset armed: scrub Thickness / Depth in the panel (%1 face(s))" )
		.arg( plan.repointFaces.size() ) );
}

// ---------------------------------------------------------------------------
// Quad layer: Blender-style quads over the triangle-only NIF. A quad is a
// tri pair whose shared edge is marked as a diagonal; the NIF data stays
// triangles at all times, so saving needs no triangulation step.

//! Undo/redo for a shape's diagonal-mark set (view-layer state, but users
//! expect Ctrl+Z for Make Face / Tris to Quads / Triangulate)
class TlQuadMarksCommand final : public QUndoCommand
{
public:
	TlQuadMarksCommand( GLView * view, int shapeBlock,
		const QSet<quint64> & newMarks, int newVertCount, const QString & text )
		: gv( view ), sb( shapeBlock ), after( newMarks ), nvAfter( newVertCount )
	{
		before = gv->quadDiagonals.value( sb );
		nvBefore = gv->quadMarkVerts.value( sb, -1 );
		setText( text );
	}
	void undo() override
	{
		gv->quadDiagonals[sb] = before;
		gv->quadMarkVerts[sb] = nvBefore;
		gv->quadPartnerCache.remove( sb );
		gv->update();
	}
	void redo() override
	{
		gv->quadDiagonals[sb] = after;
		gv->quadMarkVerts[sb] = nvAfter;
		gv->quadPartnerCache.remove( sb );
		gv->update();
	}
private:
	GLView *	gv;
	int	sb;
	QSet<quint64>	before, after;
	int	nvBefore = -1, nvAfter = -1;
};

const QSet<quint64> GLView::quadMarksFor( int shapeBlock ) const
{
	// a changed vertex count means the indices the marks were recorded
	// against no longer line up: treat the whole set as stale
	auto it = quadDiagonals.constFind( shapeBlock );
	if ( it == quadDiagonals.constEnd() || it->isEmpty() )
		return QSet<quint64>();
	Shape * s = shapeForBlock( shapeBlock );
	if ( !s || quadMarkVerts.value( shapeBlock, -1 ) != s->verts.size() )
		return QSet<quint64>();
	return *it;
}

bool GLView::isQuadDiagonal( int shapeBlock, int a, int b ) const
{
	auto it = quadDiagonals.constFind( shapeBlock );
	if ( it == quadDiagonals.constEnd() )
		return false;
	Shape * s = shapeForBlock( shapeBlock );
	if ( !s || quadMarkVerts.value( shapeBlock, -1 ) != s->verts.size() )
		return false;
	return it->contains( quadEdgeKey( a, b ) );
}

int GLView::quadPartnerTri( int shapeBlock, int tri ) const
{
	const QSet<quint64> marks = quadMarksFor( shapeBlock );
	if ( marks.isEmpty() )
		return -1;
	Shape * s = shapeForBlock( shapeBlock );
	if ( !s || tri < 0 || tri >= s->triangles.size() )
		return -1;
	const Triangle & t = s->triangles.at( tri );
	if ( t[0] == t[1] || t[1] == t[2] || t[0] == t[2] )
		return -1;
	for ( int e = 0; e < 3; e++ ) {
		const int a = t[e], b = t[( e + 1 ) % 3];
		if ( !marks.contains( quadEdgeKey( a, b ) ) )
			continue;
		// the partner is the one other non-degenerate triangle on this edge
		int partner = -1;
		for ( int ti = 0; ti < s->triangles.size(); ti++ ) {
			if ( ti == tri )
				continue;
			const Triangle & o = s->triangles.at( ti );
			if ( o[0] == o[1] || o[1] == o[2] || o[0] == o[2] )
				continue;
			int hits = 0;
			for ( int c = 0; c < 3; c++ )
				hits += int( o[c] == a || o[c] == b );
			if ( hits == 2 ) {
				if ( partner >= 0 ) {	// non-manifold edge: no unique partner
					partner = -1;
					break;
				}
				partner = ti;
			}
		}
		if ( partner >= 0 )
			return partner;
	}
	return -1;
}

void GLView::setQuadMarks( int shapeBlock, const QSet<quint64> & marks, const QString & opName,
	int vertCountOverride )
{
	Shape * s = shapeForBlock( shapeBlock );
	if ( !s || !model || !model->undoStack )
		return;
	model->undoStack->push( new TlQuadMarksCommand( this, shapeBlock, marks,
		vertCountOverride >= 0 ? vertCountOverride : s->verts.size(), opName ) );
}

void GLView::makeFace()
{
	// Blender F: two adjacent face-picked triangles (or the four corners of
	// a tri pair) form a quad by marking their shared edge; anything else
	// falls through to the fill / bridge operator
	do {
		if ( !model || !editMode || pickedElems.isEmpty() )
			break;
		int sb = -1;
		QVector<int> faces;
		QSet<int> verts;
		bool other = false;
		for ( const PickedElement & pe : std::as_const( pickedElems ) ) {
			if ( sb >= 0 && pe.shapeBlock != sb ) { other = true; break; }
			sb = pe.shapeBlock;
			if ( pe.type == 3 )
				faces << pe.e0;
			else if ( pe.type == 1 )
				verts << pe.e0;
			else
				other = true;
		}
		Shape * s = ( !other && sb >= 0 ) ? shapeForBlock( sb ) : nullptr;
		if ( !s )
			break;
		auto degenerate = []( const Triangle & t ) {
			return t[0] == t[1] || t[1] == t[2] || t[0] == t[2];
		};
		int tA = -1, tB = -1;
		if ( faces.size() == 2 && verts.isEmpty() ) {
			tA = faces.at( 0 );
			tB = faces.at( 1 );
		} else if ( faces.isEmpty() && verts.size() == 4 ) {
			// exactly the two triangles covered by the four picked corners
			for ( int ti = 0; ti < s->triangles.size(); ti++ ) {
				const Triangle & t = s->triangles.at( ti );
				if ( degenerate( t ) )
					continue;
				if ( verts.contains( t[0] ) && verts.contains( t[1] ) && verts.contains( t[2] ) ) {
					if ( tA < 0 )
						tA = ti;
					else if ( tB < 0 )
						tB = ti;
					else { tA = tB = -1; break; }	// more than two: ambiguous
				}
			}
		}
		if ( tA < 0 || tB < 0 || tA == tB
			|| tA >= s->triangles.size() || tB >= s->triangles.size() )
			break;
		const Triangle & ta = s->triangles.at( tA );
		const Triangle & tb = s->triangles.at( tB );
		if ( degenerate( ta ) || degenerate( tb ) )
			break;
		// the shared edge
		int shared[2] = { -1, -1 };
		int n = 0;
		for ( int i = 0; i < 3 && n < 2; i++ )
			for ( int j = 0; j < 3; j++ )
				if ( ta[i] == tb[j] ) { shared[n++] = ta[i]; break; }
		if ( n != 2 )
			break;
		QSet<quint64> marks = quadMarksFor( sb );
		const quint64 k = quadEdgeKey( shared[0], shared[1] );
		if ( marks.contains( k ) )
			break;	// already a quad: fall through (F is also fill)
		// a triangle can only be half of ONE quad: a second diagonal on the
		// same tri would hide two of its edges and make the partner lookup
		// edge-order dependent
		auto inQuad = [&]( const Triangle & t ) {
			for ( int e = 0; e < 3; e++ )
				if ( marks.contains( quadEdgeKey( t[e], t[( e + 1 ) % 3] ) ) )
					return true;
			return false;
		};
		if ( inQuad( ta ) || inQuad( tb ) ) {
			emit gizmoStatus( tr( "Make Face: a triangle already belongs to a quad (Triangulate it first)" ) );
			update();
			return;
		}
		marks.insert( k );
		setQuadMarks( sb, marks, tr( "Make Quad" ) );
		emit gizmoStatus( tr( "Quad formed (triangulated automatically on save)" ) );
		update();
		return;
	} while ( false );

	smartConnect();
}

//! Edge -> the (up to two) non-degenerate triangles on it; n saturates at 3
//! so a non-manifold edge is detectable without storing every triangle.
//! Built once per shape (O(T)) so the batch quad operators can look partners
//! up in O(1) — quadPartnerTri() scans all triangles per call, which turned
//! select-all Tris to Quads / Triangulate into an O(T²) freeze.
struct TlEdgeTris { int t0 = -1, t1 = -1; int n = 0; };

static QHash<quint64, TlEdgeTris> tlBuildEdgeTris( Shape * s )
{
	QHash<quint64, TlEdgeTris> adj;
	adj.reserve( s->triangles.size() * 3 / 2 );
	for ( int ti = 0; ti < s->triangles.size(); ti++ ) {
		const Triangle & t = s->triangles.at( ti );
		if ( t[0] == t[1] || t[1] == t[2] || t[0] == t[2] )
			continue;
		for ( int e = 0; e < 3; e++ ) {
			TlEdgeTris & et = adj[GLView::quadEdgeKey( t[e], t[( e + 1 ) % 3] )];
			if ( et.n == 0 )
				et.t0 = ti;
			else if ( et.n == 1 )
				et.t1 = ti;
			if ( et.n < 3 )
				et.n++;
		}
	}
	return adj;
}

//! quadPartnerTri() via the prebuilt adjacency: the one other triangle on a
//! marked edge of tri, or -1 (non-manifold edges have no unique partner)
static int tlQuadPartnerVia( const QSet<quint64> & marks,
	const QHash<quint64, TlEdgeTris> & adj, Shape * s, int tri )
{
	if ( marks.isEmpty() || !s || tri < 0 || tri >= s->triangles.size() )
		return -1;
	const Triangle & t = s->triangles.at( tri );
	if ( t[0] == t[1] || t[1] == t[2] || t[0] == t[2] )
		return -1;
	for ( int e = 0; e < 3; e++ ) {
		const quint64 k = GLView::quadEdgeKey( t[e], t[( e + 1 ) % 3] );
		if ( !marks.contains( k ) )
			continue;
		const TlEdgeTris et = adj.value( k );
		if ( et.n != 2 )
			continue;
		const int other = ( et.t0 == tri ) ? et.t1 : et.t0;
		if ( other >= 0 && other != tri )
			return other;
	}
	return -1;
}

const QHash<int, int> & GLView::quadPartnerMap( int shapeBlock ) const
{
	static const QHash<int, int> empty;
	Shape * s = shapeForBlock( shapeBlock );
	const QSet<quint64> marks = quadMarksFor( shapeBlock );
	if ( !s || marks.isEmpty() )
		return empty;
	auto it = quadPartnerCache.constFind( shapeBlock );
	if ( it != quadPartnerCache.constEnd() && it->first == s->verts.size() )
		return it->second;
	QHash<int, int> map;
	const QHash<quint64, TlEdgeTris> adj = tlBuildEdgeTris( s );
	for ( auto et = adj.constBegin(); et != adj.constEnd(); ++et ) {
		if ( et.value().n != 2 || !marks.contains( et.key() ) )
			continue;
		map.insert( et.value().t0, et.value().t1 );
		map.insert( et.value().t1, et.value().t0 );
	}
	QPair<int, QHash<int, int>> & slot = quadPartnerCache[shapeBlock];
	slot.first = s->verts.size();
	slot.second = map;
	return slot.second;
}

bool GLView::buildFacePick( int shapeBlock, int tri, PickedElement & pe ) const
{
	Shape * s = shapeForBlock( shapeBlock );
	if ( !s || tri < 0 || tri >= s->triangles.size() )
		return false;
	const Triangle & t = s->triangles.at( tri );
	if ( t[0] >= s->verts.size() || t[1] >= s->verts.size() || t[2] >= s->verts.size() )
		return false;
	Transform wt = shapeRenderTrans( s );
	pe = PickedElement();
	pe.shapeBlock = shapeBlock;
	pe.type = 3;
	pe.e0 = tri;
	pe.wA = wt * editVertexLocal( s, t[0] );
	pe.wB = wt * editVertexLocal( s, t[1] );
	pe.wC = wt * editVertexLocal( s, t[2] );
	pe.worldPos = ( pe.wA + pe.wB + pe.wC ) / 3.0f;
	Vector3 n = Vector3::crossproduct( pe.wB - pe.wA, pe.wC - pe.wA );
	n.normalize();
	pe.worldNormal = n;
	return true;
}

void GLView::expandQuadPartners( QVector<PickedElement> & elems ) const
{
	auto key = []( int sb, int t ) {
		return ( quint64( quint32( sb ) ) << 32 ) | quint32( t );
	};
	QSet<quint64> have;
	for ( const PickedElement & pe : std::as_const( elems ) )
		if ( pe.type == 3 )
			have.insert( key( pe.shapeBlock, pe.e0 ) );
	if ( have.isEmpty() )
		return;
	const int n = elems.size();
	for ( int i = 0; i < n; i++ ) {
		const PickedElement & pe = elems.at( i );
		if ( pe.type != 3 )
			continue;
		const int tj = quadPartnerMap( pe.shapeBlock ).value( pe.e0, -1 );
		if ( tj < 0 || have.contains( key( pe.shapeBlock, tj ) ) )
			continue;
		PickedElement partner;
		if ( buildFacePick( pe.shapeBlock, tj, partner ) ) {
			have.insert( key( pe.shapeBlock, tj ) );
			elems.append( partner );
		}
	}
}

void GLView::trisToQuads( float maxFaceAngleDeg, float maxShapeAngleDeg, bool armPanel )
{
	if ( !model || !editMode ) {
		emit gizmoStatus( tr( "Tris to Quads needs edit mode" ) );
		return;
	}
	const QVector<PickedElement> seed = pickedElems;
	const int undoBase = model->undoStack ? model->undoStack->index() : 0;
	// group the face selection per shape
	QHash<int, QSet<int>> selFaces;
	for ( const PickedElement & pe : std::as_const( pickedElems ) )
		if ( pe.type == 3 )
			selFaces[pe.shapeBlock].insert( pe.e0 );
	if ( selFaces.isEmpty() ) {
		emit gizmoStatus( tr( "Tris to Quads: select the faces to join (A selects all)" ) );
		return;
	}
	const float cosFace = std::cos( deg2rad( maxFaceAngleDeg ) );
	int joined = 0;
	for ( auto it = selFaces.constBegin(); it != selFaces.constEnd(); ++it ) {
		const int sb = it.key();
		Shape * s = shapeForBlock( sb );
		if ( !s )
			continue;
		const QSet<int> & sel = it.value();
		auto degenerate = []( const Triangle & t ) {
			return t[0] == t[1] || t[1] == t[2] || t[0] == t[2];
		};
		auto triNormal = [&]( int ti ) {
			const Triangle & t = s->triangles.at( ti );
			Vector3 n = Vector3::crossproduct(
				editVertexLocal( s, t[1] ) - editVertexLocal( s, t[0] ),
				editVertexLocal( s, t[2] ) - editVertexLocal( s, t[0] ) );
			n.normalize();
			return n;
		};
		auto thirdVert = [&]( int ti, int a, int b ) {
			const Triangle & t = s->triangles.at( ti );
			for ( int c = 0; c < 3; c++ )
				if ( t[c] != a && t[c] != b )
					return int( t[c] );
			return -1;
		};
		QSet<quint64> marks = quadMarksFor( sb );
		// triangles already in quads are not up for pairing (O(1) partner
		// lookups via the prebuilt adjacency; only needed when marks exist)
		QSet<int> taken;
		if ( !marks.isEmpty() ) {
			const QHash<quint64, TlEdgeTris> adj = tlBuildEdgeTris( s );
			for ( int ti : sel )
				if ( tlQuadPartnerVia( marks, adj, s, ti ) >= 0 )
					taken << ti;
		}
		// candidate shared edges between two free selected triangles
		QHash<quint64, QVector<int>> edgeTris;
		for ( int ti : sel ) {
			if ( taken.contains( ti ) || ti < 0 || ti >= s->triangles.size() )
				continue;
			const Triangle & t = s->triangles.at( ti );
			if ( degenerate( t ) )
				continue;
			for ( int e = 0; e < 3; e++ )
				edgeTris[quadEdgeKey( t[e], t[( e + 1 ) % 3] )] << ti;
		}
		struct Cand { quint64 key; int tA, tB; float cost; };
		QVector<Cand> cands;
		for ( auto et = edgeTris.constBegin(); et != edgeTris.constEnd(); ++et ) {
			if ( et.value().size() != 2 )
				continue;
			const int tA = et.value().at( 0 ), tB = et.value().at( 1 );
			// face angle: reject folds
			const Vector3 nA = triNormal( tA ), nB = triNormal( tB );
			const float d = Vector3::dotproduct( nA, nB );
			if ( d < cosFace )
				continue;
			// quad shape: corner angles' worst deviation from 90 degrees
			const int a = int( et.key() >> 32 ), b = int( et.key() & 0xFFFFFFFF );
			const int c = thirdVert( tA, a, b ), e = thirdVert( tB, a, b );
			if ( c < 0 || e < 0 || c == e )
				continue;
			const Vector3 P[4] = { editVertexLocal( s, c ), editVertexLocal( s, a ),
				editVertexLocal( s, e ), editVertexLocal( s, b ) };
			float worst = 0.0f;
			for ( int i = 0; i < 4; i++ ) {
				Vector3 u = P[( i + 3 ) % 4] - P[i];
				Vector3 v = P[( i + 1 ) % 4] - P[i];
				const float len = u.length() * v.length();
				if ( len < 1.0e-12f ) { worst = 1.0e9f; break; }
				const float ang = rad2deg(
					std::acos( std::clamp( Vector3::dotproduct( u, v ) / len, -1.0f, 1.0f ) ) );
				worst = std::max( worst, std::fabs( ang - 90.0f ) );
			}
			if ( worst > maxShapeAngleDeg )
				continue;
			Cand cd;
			cd.key = et.key();
			cd.tA = tA;
			cd.tB = tB;
			cd.cost = worst + rad2deg( std::acos( std::clamp( d, -1.0f, 1.0f ) ) );
			cands.append( cd );
		}
		std::sort( cands.begin(), cands.end(),
			[]( const Cand & x, const Cand & y ) { return x.cost < y.cost; } );
		QSet<int> used;
		int shapeJoined = 0;
		for ( const Cand & cd : std::as_const( cands ) ) {
			if ( used.contains( cd.tA ) || used.contains( cd.tB ) )
				continue;
			used << cd.tA << cd.tB;
			marks.insert( cd.key );
			shapeJoined++;
		}
		if ( shapeJoined > 0 ) {
			setQuadMarks( sb, marks, tr( "Tris to Quads" ) );
			joined += shapeJoined;
		}
	}
	emit gizmoStatus( joined > 0
		? tr( "Tris to Quads: %1 quad(s) formed" ).arg( joined )
		: tr( "Tris to Quads: no pair within the angle limits" ) );
	if ( joined > 0 && armPanel && model->undoStack ) {
		// Blender-style adjust-last-operation panel: scrub the angle limits
		lastOpExRerun = [this]( const QVector<TlOpParam> & ps ) {
			trisToQuads( float( ps.value( 0 ).value ), float( ps.value( 1 ).value ), false );
		};
		QVector<TlOpParam> ps( 2 );
		ps[0].label = tr( "Max Face Angle°" );
		ps[0].type = TlOpParam::Float;
		ps[0].value = maxFaceAngleDeg;
		ps[0].mn = 0.0;
		ps[0].mx = 180.0;
		ps[0].step = 1.0;
		ps[0].decimals = 1;
		ps[1].label = tr( "Max Shape Angle°" );
		ps[1].type = TlOpParam::Float;
		ps[1].value = maxShapeAngleDeg;
		ps[1].mn = 0.0;
		ps[1].mx = 180.0;
		ps[1].step = 1.0;
		ps[1].decimals = 1;
		armOperatorPanelEx( tr( "Tris to Quads" ), ps,
			model->undoStack->index() - undoBase, seed );
	}
	update();
}

void GLView::triangulateSelection( int diagonalMode, bool armPanel )
{
	if ( !model || !editMode ) {
		emit gizmoStatus( tr( "Triangulate needs edit mode" ) );
		return;
	}
	const QVector<PickedElement> seed = pickedElems;
	const int undoBase = model->undoStack ? model->undoStack->index() : 0;
	QHash<int, QSet<int>> selFaces;
	for ( const PickedElement & pe : std::as_const( pickedElems ) )
		if ( pe.type == 3 )
			selFaces[pe.shapeBlock].insert( pe.e0 );
	if ( selFaces.isEmpty() ) {
		emit gizmoStatus( tr( "Triangulate: select the quads to split" ) );
		return;
	}
	int split = 0;
	for ( auto it = selFaces.constBegin(); it != selFaces.constEnd(); ++it ) {
		const int sb = it.key();
		Shape * s = shapeForBlock( sb );
		if ( !s )
			continue;
		QSet<quint64> marks = quadMarksFor( sb );
		if ( marks.isEmpty() )
			continue;
		QModelIndex iShape = model->getBlockIndex( sb );
		QModelIndex iTris = model->getIndex( iShape, "Triangles" );
		if ( !iTris.isValid() )
			continue;
		// collect the quads whose both halves are in the selection (O(1)
		// partner lookups via the prebuilt adjacency)
		const QHash<quint64, TlEdgeTris> adj = tlBuildEdgeTris( s );
		struct Quad { quint64 key; int tA, tB; };
		QVector<Quad> quads;
		QSet<int> seen;
		for ( int ti : it.value() ) {
			if ( seen.contains( ti ) )
				continue;
			const int tj = tlQuadPartnerVia( marks, adj, s, ti );
			if ( tj < 0 || !it.value().contains( tj ) )
				continue;
			seen << ti << tj;
			// the marked shared edge
			const Triangle & t = s->triangles.at( ti );
			for ( int e = 0; e < 3; e++ ) {
				const quint64 k = quadEdgeKey( t[e], t[( e + 1 ) % 3] );
				if ( marks.contains( k ) ) {
					Quad q;
					q.key = k;
					q.tA = ti;
					q.tB = tj;
					quads.append( q );
					break;
				}
			}
		}
		if ( quads.isEmpty() )
			continue;
		// diagonal flips (everything except "keep") rewrite the two triangles
		QVector<QPair<int, Triangle>> rewrites;
		for ( const Quad & q : std::as_const( quads ) ) {
			marks.remove( q.key );
			if ( diagonalMode == 0 )
				continue;
			const int a = int( q.key >> 32 ), b = int( q.key & 0xFFFFFFFF );
			auto thirdVert = [&]( int ti ) {
				const Triangle & t = s->triangles.at( ti );
				for ( int c = 0; c < 3; c++ )
					if ( t[c] != a && t[c] != b )
						return int( t[c] );
				return -1;
			};
			const int c = thirdVert( q.tA ), d = thirdVert( q.tB );
			if ( c < 0 || d < 0 || c == d )
				continue;
			const Vector3 pa = editVertexLocal( s, a ), pb = editVertexLocal( s, b );
			const Vector3 pc = editVertexLocal( s, c ), pd = editVertexLocal( s, d );
			bool flip = false;
			if ( diagonalMode == 2 )
				flip = ( pc - pd ).length() < ( pa - pb ).length();
			else if ( diagonalMode == 3 )
				flip = ( pc - pd ).length() > ( pa - pb ).length();
			else {
				// beauty: Delaunay max-min-angle criterion on the quad c,a,d,b
				auto minAngle = []( const Vector3 & x, const Vector3 & y, const Vector3 & z ) {
					auto ang = []( const Vector3 & u, const Vector3 & v ) {
						const float len = u.length() * v.length();
						return ( len < 1.0e-12f ) ? 0.0f
							: std::acos( std::clamp( Vector3::dotproduct( u, v ) / len, -1.0f, 1.0f ) );
					};
					return std::min( { ang( y - x, z - x ), ang( x - y, z - y ), ang( x - z, y - z ) } );
				};
				const float keepMin = std::min( minAngle( pa, pb, pc ), minAngle( pa, pb, pd ) );
				const float flipMin = std::min( minAngle( pc, pd, pa ), minAngle( pc, pd, pb ) );
				flip = flipMin > keepMin + 1.0e-6f;
			}
			if ( !flip )
				continue;
			// quad perimeter loop c -> a -> d -> b, wound like the original pair
			Vector3 loopN = Vector3::crossproduct( pa - pc, pd - pc )
				+ Vector3::crossproduct( pd - pc, pb - pc );
			const Triangle & tOld = s->triangles.at( q.tA );
			Vector3 oldN = Vector3::crossproduct(
				editVertexLocal( s, tOld[1] ) - editVertexLocal( s, tOld[0] ),
				editVertexLocal( s, tOld[2] ) - editVertexLocal( s, tOld[0] ) );
			const bool reverse = Vector3::dotproduct( loopN, oldN ) < 0.0f;
			Triangle n1, n2;
			if ( !reverse ) {
				n1 = Triangle( quint16( c ), quint16( a ), quint16( d ) );
				n2 = Triangle( quint16( c ), quint16( d ), quint16( b ) );
			} else {
				n1 = Triangle( quint16( d ), quint16( a ), quint16( c ) );
				n2 = Triangle( quint16( b ), quint16( d ), quint16( c ) );
			}
			rewrites.append( qMakePair( q.tA, n1 ) );
			rewrites.append( qMakePair( q.tB, n2 ) );
		}
		// one undo step per shape: without the macro, Ctrl+Z restored the
		// marks but left the flipped triangles until a second Ctrl+Z
		const bool macro = !rewrites.isEmpty() && model->undoStack;
		if ( macro )
			model->undoStack->beginMacro( tr( "Triangulate" ) );
		if ( !rewrites.isEmpty() ) {
			nifSnapshotOp( model, tr( "Triangulate" ), [&]() {
				for ( const auto & rw : std::as_const( rewrites ) )
					model->set<Triangle>( model->getIndex( iTris, rw.first ), rw.second );
				model->dataChanged( QModelIndex( iShape ), QModelIndex( iShape ) );
			} );
		}
		setQuadMarks( sb, marks, tr( "Triangulate" ) );
		if ( macro )
			model->undoStack->endMacro();
		split += quads.size();
	}
	emit gizmoStatus( split > 0
		? tr( "Triangulate: %1 quad(s) split back to triangles" ).arg( split )
		: tr( "Triangulate: no quads in the selection" ) );
	if ( split > 0 && armPanel && model->undoStack ) {
		// Blender-style adjust-last-operation panel: switch the quad method live
		lastOpExRerun = [this]( const QVector<TlOpParam> & ps ) {
			triangulateSelection( int( ps.value( 0 ).value + 0.5 ), false );
		};
		QVector<TlOpParam> ps( 1 );
		ps[0].label = tr( "Quad Method" );
		ps[0].type = TlOpParam::Enum;
		ps[0].value = diagonalMode;
		ps[0].enumNames = QStringList()
			<< tr( "Keep Diagonals" ) << tr( "Beauty (max-min angle)" )
			<< tr( "Shortest Diagonal" ) << tr( "Longest Diagonal" );
		armOperatorPanelEx( tr( "Triangulate Faces" ), ps,
			model->undoStack->index() - undoBase, seed );
	}
	update();
}

// ---------------------------------------------------------------------------
// Split (Y) / Rip (V) — Blender-style in-place detachment

void GLView::splitSelection()
{
	if ( !model || !editMode || pickedElems.isEmpty() ) {
		emit gizmoStatus( tr( "Split needs a selection in edit mode" ) );
		return;
	}
	QHash<int, QSet<int>> selVertsH = pickedVertexRefs();
	QHash<int, QSet<int>> selFacesH;
	for ( const auto & pe : pickedElems )
		if ( pe.type == 3 && pe.shapeBlock >= 0 )
			selFacesH[pe.shapeBlock].insert( pe.e0 );
	QSet<int> shapeSet;
	for ( int k : selVertsH.keys() )
		shapeSet.insert( k );
	for ( int k : selFacesH.keys() )
		shapeSet.insert( k );

	int totalDup = 0;
	QVector<QPair<int, QHash<int, int>>> remapsByShape;
	const bool macroU = ( model->undoStack != nullptr );
	if ( macroU )
		model->undoStack->beginMacro( tr( "Split" ) );
	for ( int sb : shapeSet ) {
		QModelIndex iShape = model->getBlockIndex( sb );
		if ( !model->blockInherits( iShape, "BSTriShape" ) )
			continue;
		QModelIndex iVD = model->getIndex( iShape, "Vertex Data" );
		QModelIndex iTris = model->getIndex( iShape, "Triangles" );
		if ( !iVD.isValid() || !iTris.isValid() )
			continue;
		const int nT = model->get<int>( iShape, "Num Triangles" );
		const int nV = model->get<int>( iShape, "Num Vertices" );
		const QSet<int> & sv = selVertsH.value( sb );
		const QSet<int> & sf = selFacesH.value( sb );

		// region faces: explicit face picks, or faces fully covered by verts
		QSet<int> region = sf;
		QVector<Triangle> tris( nT );
		for ( int t = 0; t < nT; t++ ) {
			tris[t] = model->get<Triangle>( model->getIndex( iTris, t ) );
			if ( !region.contains( t ) && !sv.isEmpty()
				&& sv.contains( tris[t][0] ) && sv.contains( tris[t][1] ) && sv.contains( tris[t][2] ) )
				region.insert( t );
		}
		if ( region.isEmpty() || region.size() == nT )
			continue;	// nothing selected here, or everything (no boundary)

		// boundary verts: used by region AND non-region faces
		QSet<int> regionVerts, outsideVerts;
		for ( int t = 0; t < nT; t++ ) {
			const Triangle & tr = tris.at( t );
			if ( tr[0] == tr[1] || tr[1] == tr[2] || tr[0] == tr[2] )
				continue;
			for ( int c = 0; c < 3; c++ )
				( region.contains( t ) ? regionVerts : outsideVerts ).insert( tr[c] );
		}
		QVector<int> dupList;
		for ( int v : regionVerts )
			if ( outsideVerts.contains( v ) )
				dupList.append( v );
		if ( dupList.isEmpty() )
			continue;	// already detached
		std::sort( dupList.begin(), dupList.end() );
		if ( nV + dupList.size() > 0xFFFF ) {
			emit gizmoStatus( tr( "Split: would exceed the 65,535-vertex limit" ) );
			continue;
		}
		QHash<int, int> vremap;
		for ( int i = 0; i < dupList.size(); i++ )
			vremap.insert( dupList.at( i ), nV + i );
		QVector<int> regionList( region.constBegin(), region.constEnd() );
		std::sort( regionList.begin(), regionList.end() );

		const QPersistentModelIndex pShape( iShape );
		auto applySplit = [this, pShape, dupList, vremap, regionList, nV]() {
			QModelIndex iS( pShape );
			if ( !iS.isValid() )
				return;
			model->setState( BaseModel::Processing );
			QModelIndex iVD2 = model->getIndex( iS, "Vertex Data" );
			QModelIndex iT2 = model->getIndex( iS, "Triangles" );
			const int nT2 = model->get<int>( iS, "Num Triangles" );
			const int ds = model->get<int>( iS, "Data Size" );
			const int stride = ( nV > 0 ) ? ( ds - nT2 * 6 ) / nV : 0;
			model->set<int>( iS, "Num Vertices", nV + dupList.size() );
			model->updateArraySize( iVD2 );
			for ( int i = 0; i < dupList.size(); i++ )
				tlCopyItemValues( model, model->getIndex( iVD2, dupList.at( i ) ),
					model->getIndex( iVD2, nV + i ) );
			for ( int t : regionList ) {
				Triangle tr = model->get<Triangle>( model->getIndex( iT2, t ) );
				for ( int c = 0; c < 3; c++ )
					if ( vremap.contains( tr[c] ) )
						tr[c] = quint16( vremap.value( tr[c] ) );
				model->set<Triangle>( model->getIndex( iT2, t ), tr );
			}
			if ( stride > 0 )
				model->set<int>( iS, "Data Size",
					int( nV + dupList.size() ) * stride + nT2 * 6 );
			model->restoreState();
			model->dataChanged( QModelIndex( iS ), QModelIndex( iS ) );
		};
		if ( model->undoStack )
			model->undoStack->push( new TlShapeStateCommand( model, iShape, tr( "Split" ), applySplit ) );
		else
			applySplit();
		remapsByShape.append( qMakePair( sb, vremap ) );
		totalDup += dupList.size();
	}
	if ( macroU )
		model->undoStack->endMacro();

	if ( totalDup == 0 ) {
		emit gizmoStatus( tr( "Split: the selection shares no boundary with the rest of the mesh" ) );
		return;
	}
	// the selection follows the split-off side: boundary vertex/edge picks
	// move onto the duplicates (face indices are unchanged)
	for ( PickedElement & pe : pickedElems ) {
		for ( const auto & pr : std::as_const( remapsByShape ) ) {
			if ( pe.shapeBlock != pr.first )
				continue;
			if ( pe.type == 1 && pr.second.contains( pe.e0 ) ) {
				pe.e0 = pr.second.value( pe.e0 );
			} else if ( pe.type == 2 ) {
				if ( pr.second.contains( pe.e0 ) )
					pe.e0 = pr.second.value( pe.e0 );
				if ( pr.second.contains( pe.e1 ) )
					pe.e1 = pr.second.value( pe.e1 );
			}
		}
	}
	modelChanged();
	emit gizmoStatus( tr( "Split: %1 boundary vert(s) duplicated — the selection is the detached part" )
		.arg( totalDup ) );
	update();
}

void GLView::ripSelection()
{
	if ( !model || !editMode ) {
		emit gizmoStatus( tr( "Rip needs edit mode" ) );
		return;
	}
	auto ekey = []( int a, int b ) {
		if ( a > b ) std::swap( a, b );
		return ( quint64( quint32( a ) ) << 32 ) | quint32( b );
	};
	// the edge path (one mesh per rip in v1)
	int sb = -1;
	QSet<quint64> pathEdges;
	for ( const PickedElement & pe : std::as_const( pickedElems ) ) {
		if ( pe.type != 2 )
			continue;
		if ( sb >= 0 && pe.shapeBlock != sb ) {
			emit gizmoStatus( tr( "Rip: one mesh per rip" ) );
			return;
		}
		sb = pe.shapeBlock;
		pathEdges.insert( ekey( pe.e0, pe.e1 ) );
	}
	if ( sb < 0 || pathEdges.isEmpty() ) {
		emit gizmoStatus( tr( "Rip: select an edge path in edge mode" ) );
		return;
	}
	Shape * s = shapeForBlock( sb );
	QModelIndex iShape = model->getBlockIndex( sb );
	if ( !s || !model->blockInherits( iShape, "BSTriShape" ) ) {
		emit gizmoStatus( tr( "Rip is supported on FO4 (BSTriShape) meshes only" ) );
		return;
	}
	const int nV = s->verts.size();
	const int nT = s->triangles.size();
	auto degenerate = []( const Triangle & t ) {
		return t[0] == t[1] || t[1] == t[2] || t[0] == t[2];
	};

	// validation: interior manifold path, no branches
	const QHash<quint64, TlEdgeTris> adj = tlBuildEdgeTris( s );
	for ( quint64 k : std::as_const( pathEdges ) ) {
		if ( adj.value( k ).n != 2 ) {
			emit gizmoStatus( tr( "Rip: the path must run through interior manifold edges" ) );
			return;
		}
	}
	QHash<int, int> degree;
	for ( quint64 k : std::as_const( pathEdges ) ) {
		degree[int( k >> 32 )]++;
		degree[int( k & 0xFFFFFFFFu )]++;
	}
	for ( auto it = degree.constBegin(); it != degree.constEnd(); ++it ) {
		if ( it.value() > 2 ) {
			emit gizmoStatus( tr( "Rip: branching paths are not supported" ) );
			return;
		}
	}

	// seed: the path-adjacent face whose center is screen-closest to the
	// cursor — that side follows the mouse (Blender)
	const QPointF cur( mapFromGlobal( QCursor::pos() ) );
	Transform wt = shapeRenderTrans( s );
	float bestD = 1.0e30f;
	int seedFace = -1;
	for ( quint64 k : std::as_const( pathEdges ) ) {
		const TlEdgeTris et = adj.value( k );
		for ( int f : { et.t0, et.t1 } ) {
			if ( f < 0 || f >= nT )
				continue;
			const Triangle & tr = s->triangles.at( f );
			Vector3 c = ( wt * editVertexLocal( s, tr[0] ) + wt * editVertexLocal( s, tr[1] )
				+ wt * editVertexLocal( s, tr[2] ) ) / 3.0f;
			QPointF sp;
			if ( !worldToScreen( c, sp ) )
				continue;
			const float d = float( std::hypot( sp.x() - cur.x(), sp.y() - cur.y() ) );
			if ( d < bestD ) {
				bestD = d;
				seedFace = f;
			}
		}
	}
	if ( seedFace < 0 ) {
		emit gizmoStatus( tr( "Rip: could not resolve the rip side" ) );
		return;
	}

	// per interior path vertex, split the incident-face fan into the arcs on
	// either side of the path (components not crossing path edges)
	QHash<int, QVector<int>> vertFaces;
	for ( int t = 0; t < nT; t++ ) {
		const Triangle & tr = s->triangles.at( t );
		if ( degenerate( tr ) )
			continue;
		for ( int c = 0; c < 3; c++ )
			if ( degree.value( tr[c], 0 ) == 2 )	// interior path verts only
				vertFaces[tr[c]].append( t );
	}
	struct VertArcs { int v; QVector<QSet<int>> comps; };
	QVector<VertArcs> arcs;
	for ( auto it = vertFaces.constBegin(); it != vertFaces.constEnd(); ++it ) {
		const int v = it.key();
		const QVector<int> & fl = it.value();
		QVector<int> par( fl.size() );
		for ( int i = 0; i < fl.size(); i++ )
			par[i] = i;
		// faces at v sharing a NON-path edge (v, o) union into one arc
		QHash<int, QVector<int>> byOther;
		for ( int i = 0; i < fl.size(); i++ ) {
			const Triangle & tr = s->triangles.at( fl.at( i ) );
			for ( int c = 0; c < 3; c++ )
				if ( tr[c] != v )
					byOther[tr[c]].append( i );
		}
		for ( auto ot = byOther.constBegin(); ot != byOther.constEnd(); ++ot ) {
			if ( pathEdges.contains( ekey( v, ot.key() ) ) )
				continue;
			for ( int i = 1; i < ot.value().size(); i++ ) {
				int ra = tlFindRoot( par, ot.value().at( 0 ) );
				int rb = tlFindRoot( par, ot.value().at( i ) );
				if ( ra != rb )
					par[std::max( ra, rb )] = std::min( ra, rb );
			}
		}
		QHash<int, QSet<int>> comps;
		for ( int i = 0; i < fl.size(); i++ )
			comps[tlFindRoot( par, i )].insert( fl.at( i ) );
		if ( comps.size() < 2 )
			continue;	// fan not split here (shouldn't happen mid-path)
		VertArcs va;
		va.v = v;
		for ( auto ct = comps.constBegin(); ct != comps.constEnd(); ++ct )
			va.comps.append( ct.value() );
		arcs.append( va );
	}
	if ( arcs.isEmpty() ) {
		emit gizmoStatus( tr( "Rip: select a path of at least two edges (interior verts rip)" ) );
		return;
	}

	// flood the moving side from the seed through overlapping arcs
	QSet<int> moveFaces;
	moveFaces.insert( seedFace );
	bool changed = true;
	while ( changed ) {
		changed = false;
		for ( const VertArcs & va : std::as_const( arcs ) ) {
			for ( const QSet<int> & comp : va.comps ) {
				bool touch = false;
				for ( int f : comp )
					if ( moveFaces.contains( f ) ) {
						touch = true;
						break;
					}
				if ( !touch )
					continue;
				for ( int f : comp )
					if ( !moveFaces.contains( f ) ) {
						moveFaces.insert( f );
						changed = true;
					}
			}
		}
	}

	// rip verts: interior path verts with arcs on BOTH sides
	QVector<int> dupList;
	for ( const VertArcs & va : std::as_const( arcs ) ) {
		bool hasMove = false, hasStay = false;
		for ( const QSet<int> & comp : va.comps ) {
			bool m = false;
			for ( int f : comp )
				if ( moveFaces.contains( f ) ) {
					m = true;
					break;
				}
			( m ? hasMove : hasStay ) = true;
		}
		if ( hasMove && hasStay )
			dupList.append( va.v );
	}
	if ( dupList.isEmpty() ) {
		emit gizmoStatus( tr( "Rip: nothing to rip here" ) );
		return;
	}
	std::sort( dupList.begin(), dupList.end() );
	if ( nV + dupList.size() > 0xFFFF ) {
		emit gizmoStatus( tr( "Rip: would exceed the 65,535-vertex limit" ) );
		return;
	}
	QHash<int, int> vremap;
	for ( int i = 0; i < dupList.size(); i++ )
		vremap.insert( dupList.at( i ), nV + i );

	// move-side triangles that touch a ripped vert re-point onto the copies
	QVector<QPair<int, Triangle>> repoint;
	for ( int f : std::as_const( moveFaces ) ) {
		if ( f < 0 || f >= nT )
			continue;
		Triangle tr = s->triangles.at( f );
		bool ch = false;
		for ( int c = 0; c < 3; c++ ) {
			if ( vremap.contains( tr[c] ) ) {
				tr[c] = quint16( vremap.value( tr[c] ) );
				ch = true;
			}
		}
		if ( ch )
			repoint.append( qMakePair( f, tr ) );
	}

	const QPersistentModelIndex pShape( iShape );
	auto applyRip = [this, pShape, dupList, repoint, nV]() {
		QModelIndex iS( pShape );
		if ( !iS.isValid() )
			return;
		model->setState( BaseModel::Processing );
		QModelIndex iVD2 = model->getIndex( iS, "Vertex Data" );
		QModelIndex iT2 = model->getIndex( iS, "Triangles" );
		const int nT2 = model->get<int>( iS, "Num Triangles" );
		const int ds = model->get<int>( iS, "Data Size" );
		const int stride = ( nV > 0 ) ? ( ds - nT2 * 6 ) / nV : 0;
		model->set<int>( iS, "Num Vertices", nV + dupList.size() );
		model->updateArraySize( iVD2 );
		for ( int i = 0; i < dupList.size(); i++ )
			tlCopyItemValues( model, model->getIndex( iVD2, dupList.at( i ) ),
				model->getIndex( iVD2, nV + i ) );
		for ( const auto & rp : repoint )
			model->set<Triangle>( model->getIndex( iT2, rp.first ), rp.second );
		if ( stride > 0 )
			model->set<int>( iS, "Data Size",
				int( nV + dupList.size() ) * stride + nT2 * 6 );
		model->restoreState();
		model->dataChanged( QModelIndex( iS ), QModelIndex( iS ) );
	};
	if ( model->undoStack )
		model->undoStack->push( new TlShapeStateCommand( model, iShape, tr( "Rip" ), applyRip ) );
	else
		applyRip();

	// select the ripped copies and start the chained move (Blender: the
	// ripped side follows the mouse; Esc keeps it coincident)
	pickedElems.clear();
	for ( int v : std::as_const( dupList ) ) {
		PickedElement pe;
		pe.shapeBlock = sb;
		pe.type = 1;
		pe.e0 = vremap.value( v );
		pe.worldPos = wt * editVertexLocal( s, v );
		pe.wA = pe.wB = pe.wC = pe.worldPos;
		pickedElems.append( pe );
	}
	pickMode |= 1;
	modelChanged();
	gizmoBeginElement( 1 );
	emit gizmoStatus( tr( "Rip: %1 vert(s) ripped — move the ripped side, Esc keeps it in place" )
		.arg( dupList.size() ) );
}

void GLView::bevelSelection( float width, bool armPanel )
{
	// Bevel via rip + offset + bridge: rip the edge path, push both rows
	// half the width into their own side's surface plane, and bridge the
	// slit with a marked-quad strip. The path ENDPOINTS stay welded and the
	// strip tapers closed into them with one triangle each — so the corner
	// terminations that made bevel a mesh-corrupter risk never arise.
	// Same input contract as Rip: one mesh, interior manifold path, no
	// branches, at least two edges (or a closed loop).
	if ( !model || !editMode ) {
		emit gizmoStatus( tr( "Bevel needs edit mode" ) );
		return;
	}
	auto ekey = []( int a, int b ) {
		if ( a > b ) std::swap( a, b );
		return ( quint64( quint32( a ) ) << 32 ) | quint32( b );
	};
	const QVector<PickedElement> seed = pickedElems;
	int sb = -1;
	QSet<quint64> pathEdges;
	for ( const PickedElement & pe : std::as_const( pickedElems ) ) {
		if ( pe.type != 2 )
			continue;
		if ( sb >= 0 && pe.shapeBlock != sb ) {
			emit gizmoStatus( tr( "Bevel: one mesh per bevel" ) );
			return;
		}
		sb = pe.shapeBlock;
		pathEdges.insert( ekey( pe.e0, pe.e1 ) );
	}
	if ( sb < 0 || pathEdges.isEmpty() ) {
		emit gizmoStatus( tr( "Bevel: select an edge path in edge mode" ) );
		return;
	}
	Shape * s = shapeForBlock( sb );
	QModelIndex iShape = model->getBlockIndex( sb );
	if ( !s || !model->blockInherits( iShape, "BSTriShape" ) ) {
		emit gizmoStatus( tr( "Bevel is supported on FO4 (BSTriShape) meshes only" ) );
		return;
	}
	const int nV = s->verts.size();
	const int nT = s->triangles.size();
	auto degenerate = []( const Triangle & t ) {
		return t[0] == t[1] || t[1] == t[2] || t[0] == t[2];
	};

	const QHash<quint64, TlEdgeTris> adj = tlBuildEdgeTris( s );
	for ( quint64 k : std::as_const( pathEdges ) ) {
		if ( adj.value( k ).n != 2 ) {
			emit gizmoStatus( tr( "Bevel: the path must run through interior manifold edges" ) );
			return;
		}
	}
	QHash<int, int> degree;
	for ( quint64 k : std::as_const( pathEdges ) ) {
		degree[int( k >> 32 )]++;
		degree[int( k & 0xFFFFFFFFu )]++;
	}
	QHash<int, QVector<int>> nbr;
	for ( quint64 k : std::as_const( pathEdges ) ) {
		const int a = int( k >> 32 ), b = int( k & 0xFFFFFFFFu );
		nbr[a].append( b );
		nbr[b].append( a );
	}
	for ( auto it = degree.constBegin(); it != degree.constEnd(); ++it ) {
		if ( it.value() > 2 ) {
			emit gizmoStatus( tr( "Bevel: branching paths are not supported" ) );
			return;
		}
	}

	// ordered chain walk (open: endpoint to endpoint; closed: full ring)
	bool closed = true;
	int start = -1;
	for ( auto it = degree.constBegin(); it != degree.constEnd(); ++it ) {
		if ( it.value() == 1 ) {
			start = it.key();
			closed = false;
			break;
		}
	}
	if ( start < 0 )
		start = int( *pathEdges.constBegin() >> 32 );
	QVector<int> chain;
	{
		int prev = -1, cur = start;
		chain.append( cur );
		while ( chain.size() <= pathEdges.size() + 1 ) {
			int nxt = -1;
			for ( int cand : nbr.value( cur ) )
				if ( cand != prev ) {
					nxt = cand;
					break;
				}
			if ( nxt < 0 || ( closed && nxt == start ) )
				break;
			chain.append( nxt );
			prev = cur;
			cur = nxt;
		}
	}
	const int expected = closed ? pathEdges.size() : pathEdges.size() + 1;
	if ( chain.size() != expected ) {
		emit gizmoStatus( tr( "Bevel: the selected edges must form one connected path" ) );
		return;
	}
	// interior verts, in chain order (closed loop: every vert)
	QVector<int> interior;
	QVector<int> chainPos;	// interior[i]'s index in chain
	for ( int ci = 0; ci < chain.size(); ci++ ) {
		if ( degree.value( chain.at( ci ), 0 ) == 2 ) {
			interior.append( chain.at( ci ) );
			chainPos.append( ci );
		}
	}
	if ( interior.isEmpty() ) {
		emit gizmoStatus( tr( "Bevel: select a path of at least two edges (interior verts bevel)" ) );
		return;
	}
	if ( nV + interior.size() > 0xFFFF ) {
		emit gizmoStatus( tr( "Bevel: would exceed the 65,535-vertex limit" ) );
		return;
	}

	// per interior vertex, the incident-face fan splits into side arcs
	QHash<int, QVector<int>> vertFaces;
	for ( int t = 0; t < nT; t++ ) {
		const Triangle & tr = s->triangles.at( t );
		if ( degenerate( tr ) )
			continue;
		for ( int c = 0; c < 3; c++ )
			if ( degree.value( tr[c], 0 ) == 2 )
				vertFaces[tr[c]].append( t );
	}
	struct VertArcs { int v; QVector<QSet<int>> comps; };
	QVector<VertArcs> arcs;
	for ( auto it = vertFaces.constBegin(); it != vertFaces.constEnd(); ++it ) {
		const int v = it.key();
		const QVector<int> & fl = it.value();
		QVector<int> par( fl.size() );
		for ( int i = 0; i < fl.size(); i++ )
			par[i] = i;
		QHash<int, QVector<int>> byOther;
		for ( int i = 0; i < fl.size(); i++ ) {
			const Triangle & tr = s->triangles.at( fl.at( i ) );
			for ( int c = 0; c < 3; c++ )
				if ( tr[c] != v )
					byOther[tr[c]].append( i );
		}
		for ( auto ot = byOther.constBegin(); ot != byOther.constEnd(); ++ot ) {
			if ( pathEdges.contains( ekey( v, ot.key() ) ) )
				continue;
			for ( int i = 1; i < ot.value().size(); i++ ) {
				int ra = tlFindRoot( par, ot.value().at( 0 ) );
				int rb = tlFindRoot( par, ot.value().at( i ) );
				if ( ra != rb )
					par[std::max( ra, rb )] = std::min( ra, rb );
			}
		}
		QHash<int, QSet<int>> comps;
		for ( int i = 0; i < fl.size(); i++ )
			comps[tlFindRoot( par, i )].insert( fl.at( i ) );
		if ( comps.size() < 2 )
			continue;
		VertArcs va;
		va.v = v;
		for ( auto ct = comps.constBegin(); ct != comps.constEnd(); ++ct )
			va.comps.append( ct.value() );
		arcs.append( va );
	}

	// flood both sides from the two faces of the first path edge
	auto flood = [&arcs]( int seedFace ) {
		QSet<int> fs;
		fs.insert( seedFace );
		bool changed = true;
		while ( changed ) {
			changed = false;
			for ( const VertArcs & va : std::as_const( arcs ) ) {
				for ( const QSet<int> & comp : va.comps ) {
					bool touch = false;
					for ( int f : comp )
						if ( fs.contains( f ) ) {
							touch = true;
							break;
						}
					if ( !touch )
						continue;
					for ( int f : comp )
						if ( !fs.contains( f ) ) {
							fs.insert( f );
							changed = true;
						}
				}
			}
		}
		return fs;
	};
	const TlEdgeTris eSeed = adj.value( ekey( chain.at( 0 ), chain.at( 1 ) ) );
	QSet<int> sideA = flood( eSeed.t0 );
	QSet<int> sideB = flood( eSeed.t1 );
	if ( sideA.intersects( sideB ) ) {
		emit gizmoStatus( tr( "Bevel: the two sides of the path connect — cannot bevel here" ) );
		return;
	}

	// default width: a quarter of the average path edge length
	if ( width < 0.0f ) {
		float len = 0.0f;
		for ( quint64 k : std::as_const( pathEdges ) )
			len += ( s->verts.at( int( k >> 32 ) ) - s->verts.at( int( k & 0xFFFFFFFFu ) ) ).length();
		width = 0.25f * len / float( pathEdges.size() );
	}

	// offsets: each row moves half the width into its own side's surface
	// plane, perpendicular to the local path direction (centroid-based side
	// direction — a v1 approximation of Blender's edge-slide directions)
	QHash<int, int> vremap;
	for ( int i = 0; i < interior.size(); i++ )
		vremap.insert( interior.at( i ), nV + i );
	QVector<Vector3> offA( interior.size() ), offB( interior.size() );
	for ( int i = 0; i < interior.size(); i++ ) {
		const int v = interior.at( i );
		const int ci = chainPos.at( i );
		const int vPrev = chain.at( ( ci - 1 + chain.size() ) % chain.size() );
		const int vNext = chain.at( ( ci + 1 ) % chain.size() );
		const Vector3 p = s->verts.at( v );
		Vector3 pathDir = s->verts.at( vNext ) - s->verts.at( vPrev );
		if ( pathDir.squaredLength() > 1.0e-12f )
			pathDir.normalize();
		auto sideDir = [&]( const QSet<int> & side ) {
			Vector3 acc;
			int cnt = 0;
			for ( int f : vertFaces.value( v ) ) {
				if ( !side.contains( f ) )
					continue;
				const Triangle & tr = s->triangles.at( f );
				const Vector3 c = ( s->verts.at( tr[0] ) + s->verts.at( tr[1] )
					+ s->verts.at( tr[2] ) ) / 3.0f;
				acc += c - p;
				cnt++;
			}
			if ( cnt == 0 )
				return Vector3();
			acc = acc / float( cnt );
			acc -= pathDir * Vector3::dotproduct( acc, pathDir );
			if ( acc.squaredLength() > 1.0e-12f )
				acc.normalize();
			return acc;
		};
		offA[i] = p + sideDir( sideA ) * ( width * 0.5f );
		offB[i] = p + sideDir( sideB ) * ( width * 0.5f );
	}

	// side B faces touching an interior vert re-point onto the copies
	QVector<QPair<int, Triangle>> repoint;
	for ( int f : std::as_const( sideB ) ) {
		if ( f < 0 || f >= nT )
			continue;
		Triangle tr = s->triangles.at( f );
		bool ch = false;
		for ( int c = 0; c < 3; c++ ) {
			if ( vremap.contains( tr[c] ) ) {
				tr[c] = quint16( vremap.value( tr[c] ) );
				ch = true;
			}
		}
		if ( ch )
			repoint.append( qMakePair( f, tr ) );
	}

	// bridge the slit with a quad strip (+ end triangles on an open chain)
	QVector<Triangle> fill;
	QSet<quint64> stripDiags;
	auto addQuad = [&]( int a0, int a1, int b1, int b0 ) {
		// quad loop a0 -> a1 -> b1 -> b0, diagonal a0-b1
		fill.append( Triangle( quint16( a0 ), quint16( a1 ), quint16( b1 ) ) );
		fill.append( Triangle( quint16( a0 ), quint16( b1 ), quint16( b0 ) ) );
		stripDiags.insert( ekey( a0, b1 ) );
	};
	for ( int i = 0; i + 1 < interior.size(); i++ ) {
		if ( chainPos.at( i + 1 ) != chainPos.at( i ) + 1 )
			continue;	// not chain-adjacent (cannot happen on a valid path)
		addQuad( interior.at( i ), interior.at( i + 1 ),
			vremap.value( interior.at( i + 1 ) ), vremap.value( interior.at( i ) ) );
	}
	if ( closed ) {
		addQuad( interior.constLast(), interior.constFirst(),
			vremap.value( interior.constFirst() ), vremap.value( interior.constLast() ) );
	} else {
		// taper triangles into the welded endpoints
		fill.append( Triangle( quint16( chain.constFirst() ),
			quint16( interior.constFirst() ), quint16( vremap.value( interior.constFirst() ) ) ) );
		fill.append( Triangle( quint16( chain.constLast() ),
			quint16( vremap.value( interior.constLast() ) ), quint16( interior.constLast() ) ) );
	}

	// winding: one decision for the whole strip, from the surface normal of
	// side A around the path vs the first quad's normal at offset positions
	{
		Vector3 surfN;
		for ( int f : std::as_const( sideA ) ) {
			if ( f < 0 || f >= nT )
				continue;
			const Triangle & tr = s->triangles.at( f );
			surfN += Vector3::crossproduct( s->verts.at( tr[1] ) - s->verts.at( tr[0] ),
				s->verts.at( tr[2] ) - s->verts.at( tr[0] ) );
		}
		auto posOf = [&]( int v ) {
			for ( int i = 0; i < interior.size(); i++ ) {
				if ( interior.at( i ) == v )
					return offA.at( i );
				if ( vremap.value( interior.at( i ), -1 ) == v )
					return offB.at( i );
			}
			return s->verts.at( v );
		};
		if ( !fill.isEmpty() ) {
			const Triangle & f0 = fill.constFirst();
			const Vector3 n0 = Vector3::crossproduct( posOf( f0[1] ) - posOf( f0[0] ),
				posOf( f0[2] ) - posOf( f0[0] ) );
			if ( Vector3::dotproduct( n0, surfN ) < 0.0f ) {
				for ( Triangle & tr : fill )
					std::swap( tr[1], tr[2] );
			}
		}
	}

	// apply in place; strip diagonals recorded as quad marks in the macro
	const QPersistentModelIndex pShape( iShape );
	const QVector<int> interiorCopy = interior;
	auto applyBevel = [this, pShape, interiorCopy, offA, offB, repoint, fill, nV]() {
		QModelIndex iS( pShape );
		if ( !iS.isValid() )
			return;
		model->setState( BaseModel::Processing );
		QModelIndex iVD2 = model->getIndex( iS, "Vertex Data" );
		QModelIndex iT2 = model->getIndex( iS, "Triangles" );
		const int nT2 = model->get<int>( iS, "Num Triangles" );
		const int ds = model->get<int>( iS, "Data Size" );
		const int stride = ( nV > 0 ) ? ( ds - nT2 * 6 ) / nV : 0;
		model->set<int>( iS, "Num Vertices", nV + interiorCopy.size() );
		model->updateArraySize( iVD2 );
		for ( int i = 0; i < interiorCopy.size(); i++ ) {
			tlCopyItemValues( model, model->getIndex( iVD2, interiorCopy.at( i ) ),
				model->getIndex( iVD2, nV + i ) );
			tlSetVertexLocal( model, iS, nV + i, offB.at( i ) );
			tlSetVertexLocal( model, iS, interiorCopy.at( i ), offA.at( i ) );
		}
		for ( const auto & rp : repoint )
			model->set<Triangle>( model->getIndex( iT2, rp.first ), rp.second );
		model->set<int>( iS, "Num Triangles", nT2 + fill.size() );
		model->updateArraySize( iT2 );
		for ( int i = 0; i < fill.size(); i++ )
			model->set<Triangle>( model->getIndex( iT2, nT2 + i ), fill.at( i ) );
		if ( stride > 0 )
			model->set<int>( iS, "Data Size",
				int( nV + interiorCopy.size() ) * stride + ( nT2 + fill.size() ) * 6 );
		QSet<int> touched;
		for ( int i = 0; i < interiorCopy.size(); i++ )
			touched << interiorCopy.at( i ) << ( nV + i );
		for ( const Triangle & tr : fill )
			touched << tr[0] << tr[1] << tr[2];
		tlRecalcNormalsSubset( model, iS, touched );
		tlUpdateBounds( model, iS );
		model->restoreState();
		model->dataChanged( QModelIndex( iS ), QModelIndex( iS ) );
	};

	// marks: keep valid old marks that don't touch the beveled path (side B
	// edges were re-pointed), add the strip's diagonals
	QSet<quint64> newMarks;
	{
		const QSet<quint64> oldMarks = quadMarksFor( sb );
		QSet<int> pathVerts( degree.keyBegin(), degree.keyEnd() );
		for ( quint64 k : oldMarks ) {
			if ( !pathVerts.contains( int( k >> 32 ) )
				&& !pathVerts.contains( int( k & 0xFFFFFFFFu ) ) )
				newMarks.insert( k );
		}
		newMarks |= stripDiags;
	}
	if ( model->undoStack ) {
		model->undoStack->beginMacro( tr( "Bevel" ) );
		model->undoStack->push( new TlShapeStateCommand( model, iShape, tr( "Bevel" ), applyBevel ) );
		setQuadMarks( sb, newMarks, tr( "Bevel" ), nV + interior.size() );
		model->undoStack->endMacro();
	} else {
		applyBevel();
	}
	pickedElems.clear();
	modelChanged();

	if ( armPanel && model->undoStack ) {
		lastOpExRerun = [this]( const QVector<TlOpParam> & ps ) {
			bevelSelection( float( ps.value( 0 ).value ), false );
		};
		QVector<TlOpParam> ps( 1 );
		ps[0].label = tr( "Width" );
		ps[0].type = TlOpParam::Float;
		ps[0].value = width;
		ps[0].mn = 0.0;
		ps[0].mx = 10000.0;
		ps[0].step = 0.05;
		ps[0].decimals = 3;
		armOperatorPanelEx( tr( "Bevel" ), ps, 1, seed );
	}
	emit gizmoStatus( tr( "Bevel: %1 vert(s) beveled into a %2-quad strip" )
		.arg( interior.size() ).arg( fill.size() / 2 ) );
	update();
}

// ---------------------------------------------------------------------------
// Knife (K): Blender-style cut tool. Cut points snap to vertices and edges;
// Enter splits every edge the polyline crosses (arbitrary split position,
// attributes lerped), Esc cancels. Face clicks are waypoints in v1.

void GLView::beginKnife()
{
	if ( !model || !editMode || gizmoMode != 0
		|| riggingWeightPaintMode || vertexPaintMode || segmentPaintMode ) {
		emit gizmoStatus( tr( "Knife needs edit mode" ) );
		return;
	}
	// the knife replaces any armed select gadget
	boxSelecting = false;
	boxSelectDrag = false;
	circleSelecting = false;
	circlePainting = false;
	circleErasing = false;
	knifeActive = true;
	knifePoints.clear();
	knifeHoverValid = knifeProbe( QPointF( mapFromGlobal( QCursor::pos() ) ), knifeHoverPt );
	setCursor( Qt::CrossCursor );
	// the cut points reference vertex indices: any undo-stack activity while
	// the knife is armed (Edit menu Undo, Ctrl+Z with the pointer off the
	// viewport, a spell) can invalidate them, so it cancels the knife
	if ( model->undoStack ) {
		knifeUndoConn = connect( model->undoStack, &QUndoStack::indexChanged,
			this, [this]( int ) {
				if ( knifeActive )
					cancelKnife();
			} );
	}
	emit gizmoStatus( tr( "Knife: LMB place cut points (snaps to verts/edges), MMB orbit, Z cut-through (%1), Enter cut, Esc cancel" )
		.arg( knifeCutThrough ? tr( "on" ) : tr( "off" ) ) );
	update();
}

void GLView::cancelKnife()
{
	disconnect( knifeUndoConn );
	knifeActive = false;
	knifePoints.clear();
	knifeHoverValid = false;
	unsetCursor();
	emit gizmoStatus( tr( "Knife cancelled" ) );
	update();
}

bool GLView::knifeProbe( const QPointF & pos, KnifePoint & kp ) const
{
	kp = KnifePoint();
	if ( !model )
		return false;
	const QSet<int> * only = ( !editShapeBlocks.isEmpty() ) ? &editShapeBlocks : nullptr;
	SceneRayHit hit = raycastScene( pos, -1, only );
	if ( !hit.shape || hit.tri < 0 || hit.tri >= hit.shape->triangles.size() )
		return false;
	Shape * s = hit.shape;
	if ( editHiddenTris.value( s->id() ).contains( hit.tri ) )
		return false;
	const Triangle & t = s->triangles.at( hit.tri );
	if ( t[0] == t[1] || t[1] == t[2] || t[0] == t[2] )
		return false;
	Transform wt = shapeRenderTrans( s );
	Vector3 wv[3];
	QPointF sc[3];
	bool ok[3];
	for ( int i = 0; i < 3; i++ ) {
		wv[i] = wt * editVertexLocal( s, t[i] );
		ok[i] = worldToScreen( wv[i], sc[i] );
	}
	kp.shapeBlock = s->id();

	// vertex snap (Blender: strongest)
	float bestV = 11.0f;
	int vi = -1;
	for ( int i = 0; i < 3; i++ ) {
		if ( !ok[i] )
			continue;
		const float d = float( std::hypot( sc[i].x() - pos.x(), sc[i].y() - pos.y() ) );
		if ( d < bestV ) {
			bestV = d;
			vi = i;
		}
	}
	if ( vi >= 0 ) {
		kp.snapVert = t[vi];
		kp.world = wv[vi];
		kp.screen = sc[vi];
		return true;
	}

	// edge snap
	float bestE = 8.0f;
	int ei = -1;
	float eu = 0.0f;
	for ( int i = 0; i < 3; i++ ) {
		const int j = ( i + 1 ) % 3;
		if ( !ok[i] || !ok[j] )
			continue;
		const QPointF d = sc[j] - sc[i];
		const float len2 = float( d.x() * d.x() + d.y() * d.y() );
		if ( len2 < 1.0e-6f )
			continue;
		float u = float( ( pos.x() - sc[i].x() ) * d.x() + ( pos.y() - sc[i].y() ) * d.y() ) / len2;
		u = std::clamp( u, 0.0f, 1.0f );
		const QPointF c = sc[i] + d * u;
		const float dist = float( std::hypot( c.x() - pos.x(), c.y() - pos.y() ) );
		if ( dist < bestE ) {
			bestE = dist;
			ei = i;
			eu = u;
		}
	}
	if ( ei >= 0 ) {
		const int j = ( ei + 1 ) % 3;
		int a = t[ei], b = t[j];
		float u = std::clamp( eu, 0.02f, 0.98f );
		if ( a > b ) {
			std::swap( a, b );
			u = 1.0f - u;
		}
		kp.edgeA = a;
		kp.edgeB = b;
		kp.edgeT = u;
		kp.world = wt * ( editVertexLocal( s, kp.edgeA ) * ( 1.0f - u )
			+ editVertexLocal( s, kp.edgeB ) * u );
		worldToScreen( kp.world, kp.screen );
		return true;
	}

	// free point on the face: becomes a poked vertex on apply (v2). The
	// barycentrics are clamped slightly inward so the fan cannot degenerate.
	kp.faceTri = hit.tri;
	{
		const Vector3 pa = editVertexLocal( s, t[0] );
		const Vector3 pb = editVertexLocal( s, t[1] );
		const Vector3 pc = editVertexLocal( s, t[2] );
		const Vector3 n = Vector3::crossproduct( pb - pa, pc - pa );
		const float a2 = n.squaredLength();
		if ( a2 > 1.0e-16f ) {
			const Vector3 & p = hit.hitLocal;
			float u = Vector3::dotproduct( Vector3::crossproduct( pc - pb, p - pb ), n ) / a2;
			float v = Vector3::dotproduct( Vector3::crossproduct( pa - pc, p - pc ), n ) / a2;
			float w = 1.0f - u - v;
			u = std::max( u, 0.02f );
			v = std::max( v, 0.02f );
			w = std::max( w, 0.02f );
			const float sum = u + v + w;
			kp.bary = Vector3( u / sum, v / sum, w / sum );
		} else {
			kp.faceTri = -1;
		}
	}
	kp.world = wt * hit.hitLocal;
	worldToScreen( kp.world, kp.screen );
	return true;
}

void GLView::knifeAddPoint( const QPointF & pos )
{
	KnifePoint kp;
	if ( !knifeProbe( pos, kp ) ) {
		emit gizmoStatus( tr( "Knife: click on the edited mesh" ) );
		return;
	}
	// v2: the polyline may run across several edit-session meshes; the cut
	// applies per shape on Enter
	knifePoints.append( kp );
	update();
}

void GLView::knifeToggleCutThrough()
{
	knifeCutThrough = !knifeCutThrough;
	emit gizmoStatus( knifeCutThrough
		? tr( "Knife: cut-through ON (occluded front-facing edges are cut)" )
		: tr( "Knife: cut-through OFF (only visible edges are cut)" ) );
	update();
}

void GLView::knifeApply()
{
	// disarm the undo-stack watcher FIRST: the apply itself pushes commands
	disconnect( knifeUndoConn );
	const QVector<KnifePoint> pts = knifePoints;
	const bool cutThrough = knifeCutThrough;
	knifeActive = false;
	knifePoints.clear();
	knifeHoverValid = false;
	unsetCursor();
	if ( pts.size() < 2 || !model ) {
		emit gizmoStatus( tr( "Knife: place at least two points, then Enter" ) );
		update();
		return;
	}

	// fresh screen positions (the view may have orbited since the clicks)
	QVector<QPointF> scr( pts.size() );
	for ( int i = 0; i < pts.size(); i++ ) {
		if ( !worldToScreen( pts.at( i ).world, scr[i] ) ) {
			emit gizmoStatus( tr( "Knife: a cut point is behind the camera" ) );
			update();
			return;
		}
	}

	// v2: the polyline may span several edit-session meshes; each applies
	// its own cuts, all inside one undo macro
	QSet<int> shapes;
	for ( const KnifePoint & kp : pts )
		shapes.insert( kp.shapeBlock );

	const Vector3 eye = viewTransform().inverted() * Vector3( 0.0f, 0.0f, 0.0f );
	int totalCuts = 0, totalPokes = 0;
	const bool macroU = ( model->undoStack != nullptr );
	if ( macroU )
		model->undoStack->beginMacro( tr( "Knife" ) );
	for ( int sb : std::as_const( shapes ) ) {
		Shape * s = shapeForBlock( sb );
		QModelIndex iShape = model->getBlockIndex( sb );
		if ( !s || !model->blockInherits( iShape, "BSTriShape" ) )
			continue;

		// interior points on this shape become poked vertices (one per tri)
		struct Poke { int tri; Vector3 bary; };
		QVector<Poke> pokes;
		QSet<int> pokedTris;
		for ( const KnifePoint & kp : pts ) {
			if ( kp.shapeBlock != sb || kp.snapVert >= 0 || kp.edgeA >= 0
				|| kp.faceTri < 0 || kp.faceTri >= s->triangles.size() )
				continue;
			if ( pokedTris.contains( kp.faceTri ) )
				continue;	// one poke per triangle (v2)
			pokedTris.insert( kp.faceTri );
			Poke pk;
			pk.tri = kp.faceTri;
			pk.bary = kp.bary;
			pokes.append( pk );
		}

		// cut set: normalized edge key -> split position
		QHash<quint64, float> cuts;
		auto addCut = [&cuts]( int a, int b, float t ) {
			if ( a == b )
				return;
			if ( a > b ) {
				std::swap( a, b );
				t = 1.0f - t;
			}
			const quint64 k = quadEdgeKey( a, b );
			if ( !cuts.contains( k ) )
				cuts.insert( k, std::clamp( t, 0.02f, 0.98f ) );
		};
		for ( const KnifePoint & kp : pts )
			if ( kp.shapeBlock == sb && kp.edgeA >= 0 )
				addCut( kp.edgeA, kp.edgeB, kp.edgeT );

		// candidate edges: unique edges of visible, front-facing triangles
		const QSet<int> hiddenT = editHiddenTris.value( sb );
		Transform wt = shapeRenderTrans( s );
		struct ScrEdge { int a, b; QPointF sa, sb; };
		QVector<ScrEdge> edges;
		{
			QSet<quint64> seen;
			for ( int ti = 0; ti < s->triangles.size(); ti++ ) {
				if ( hiddenT.contains( ti ) )
					continue;
				const Triangle & t = s->triangles.at( ti );
				if ( t[0] == t[1] || t[1] == t[2] || t[0] == t[2] )
					continue;
				const Vector3 w0 = wt * editVertexLocal( s, t[0] );
				const Vector3 w1 = wt * editVertexLocal( s, t[1] );
				const Vector3 w2 = wt * editVertexLocal( s, t[2] );
				Vector3 n = Vector3::crossproduct( w1 - w0, w2 - w0 );
				if ( Vector3::dotproduct( n, eye - ( w0 + w1 + w2 ) / 3.0f ) <= 0.0f )
					continue;	// back-facing: the knife cuts what you see
				for ( int e = 0; e < 3; e++ ) {
					const int a = t[e], b = t[( e + 1 ) % 3];
					const quint64 k = quadEdgeKey( a, b );
					if ( seen.contains( k ) )
						continue;
					seen.insert( k );
					ScrEdge se;
					se.a = std::min( a, b );
					se.b = std::max( a, b );
					QPointF pa, pb;
					if ( !worldToScreen( wt * editVertexLocal( s, se.a ), pa )
						|| !worldToScreen( wt * editVertexLocal( s, se.b ), pb ) )
						continue;
					se.sa = pa;
					se.sb = pb;
					edges.append( se );
				}
			}
		}

		// 2D crossings between the polyline and this shape's edges. With
		// cut-through OFF the crossing itself must be visible: the first
		// thing the ray hits there has to be one of the edge's own faces.
		for ( int i = 0; i + 1 < pts.size(); i++ ) {
			const QPointF p0 = scr.at( i ), p1 = scr.at( i + 1 );
			const QPointF r = p1 - p0;
			for ( const ScrEdge & se : std::as_const( edges ) ) {
				const QPointF q0 = se.sa, q1 = se.sb;
				const QPointF q = q1 - q0;
				const double denom = r.x() * q.y() - r.y() * q.x();
				if ( std::fabs( denom ) < 1.0e-9 )
					continue;
				const QPointF d = q0 - p0;
				const double tSeg = ( d.x() * q.y() - d.y() * q.x() ) / denom;
				const double uEdge = ( d.x() * r.y() - d.y() * r.x() ) / denom;
				if ( tSeg < 0.0 || tSeg > 1.0 || uEdge < 0.02 || uEdge > 0.98 )
					continue;
				if ( !cutThrough ) {
					const QPointF cp = p0 + r * tSeg;
					SceneRayHit oh = raycastScene( cp, -1 );
					if ( !oh.shape || oh.shape->id() != sb
						|| oh.tri < 0 || oh.tri >= oh.shape->triangles.size() )
						continue;
					const Triangle & ot = oh.shape->triangles.at( oh.tri );
					int hitsAB = 0;
					for ( int c = 0; c < 3; c++ )
						hitsAB += int( ot[c] == se.a || ot[c] == se.b );
					if ( hitsAB < 2 )
						continue;	// something else is in front here
				}
				addCut( se.a, se.b, float( uEdge ) );
			}
		}

		if ( cuts.isEmpty() && pokes.isEmpty() )
			continue;
		const int oldNV = model->get<int>( iShape, "Num Vertices" );
		if ( oldNV + cuts.size() + pokes.size() > 0xFFFF ) {
			emit gizmoStatus( tr( "Knife: result would exceed the 65,535-vertex limit" ) );
			continue;
		}

		// apply: poke the interior points (fanning their host triangles),
		// split each cut edge, then re-split the affected triangles exactly
		// like Subdivide does (its splitting is t-agnostic)
		const QPersistentModelIndex pShape( iShape );
		QVector<QPair<quint64, float>> cutList;
		cutList.reserve( cuts.size() );
		for ( auto it = cuts.constBegin(); it != cuts.constEnd(); ++it )
			cutList.append( qMakePair( it.key(), it.value() ) );
		auto applyKnife = [this, pShape, cutList, pokes]() {
			QModelIndex iS( pShape );
			if ( !iS.isValid() )
				return;
			auto ekey = []( int a, int b ) {
				if ( a > b ) std::swap( a, b );
				return ( quint64( quint32( a ) ) << 32 ) | quint32( b );
			};
			QModelIndex iVD = model->getIndex( iS, "Vertex Data" );
			QModelIndex iT = model->getIndex( iS, "Triangles" );
			const int nv = model->get<int>( iS, "Num Vertices" );
			const int nt = model->get<int>( iS, "Num Triangles" );
			const int ds = model->get<int>( iS, "Data Size" );
			const int stride = ( nv > 0 ) ? ( ds - nt * 6 ) / nv : 0;
			// safety net: everything captured at click time must still
			// reference live rows (undo elsewhere could have changed them)
			QVector<QPair<quint64, float>> validCuts;
			validCuts.reserve( cutList.size() );
			for ( const auto & c : cutList ) {
				const int a = int( c.first >> 32 ), b = int( c.first & 0xFFFFFFFFu );
				if ( a >= 0 && b >= 0 && a < nv && b < nv && a != b )
					validCuts.append( c );
			}
			QVector<Poke> validPokes;
			validPokes.reserve( pokes.size() );
			for ( const Poke & pk : pokes )
				if ( pk.tri >= 0 && pk.tri < nt )
					validPokes.append( pk );
			if ( validCuts.isEmpty() && validPokes.isEmpty() )
				return;
			QVector<Triangle> tv( nt );
			for ( int t = 0; t < nt; t++ )
				tv[t] = model->get<Triangle>( model->getIndex( iT, t ) );

			model->setState( BaseModel::Processing );
			model->set<int>( iS, "Num Vertices",
				nv + validPokes.size() + validCuts.size() );
			model->updateArraySize( iVD );
			int nvi = nv;
			QSet<int> touched;
			// pokes first: a new interior vertex fans its host triangle (the
			// fans keep every original edge, so the cut pass runs unchanged)
			QVector<Triangle> tv2;
			tv2.reserve( nt + validPokes.size() * 2 );
			QHash<int, int> pokeVert;
			for ( const Poke & pk : validPokes ) {
				const Triangle & ht = tv.at( pk.tri );
				if ( ht[0] == ht[1] || ht[1] == ht[2] || ht[0] == ht[2] )
					continue;
				tlWriteBaryVertex( model, iS, nvi, ht[0], ht[1], ht[2], pk.bary );
				pokeVert.insert( pk.tri, nvi );
				touched << ht[0] << ht[1] << ht[2] << nvi;
				nvi++;
			}
			for ( int t = 0; t < nt; t++ ) {
				const Triangle & tr = tv.at( t );
				auto pv = pokeVert.constFind( t );
				if ( pv == pokeVert.constEnd() ) {
					tv2.append( tr );
					continue;
				}
				const quint16 p = quint16( pv.value() );
				tv2.append( Triangle( tr[0], tr[1], p ) );
				tv2.append( Triangle( tr[1], tr[2], p ) );
				tv2.append( Triangle( tr[2], tr[0], p ) );
			}
			// midpoint vert per cut edge
			QHash<quint64, int> mid;
			for ( const auto & c : validCuts ) {
				const int a = int( c.first >> 32 ), b = int( c.first & 0xFFFFFFFFu );
				tlWriteLerpVertex( model, iS, nvi, a, b, c.second );
				mid.insert( c.first, nvi++ );
			}
			QVector<Triangle> out;
			out.reserve( tv2.size() * 2 );
			for ( int t = 0; t < tv2.size(); t++ ) {
				const Triangle & tr = tv2.at( t );
				if ( tr[0] == tr[1] || tr[1] == tr[2] || tr[0] == tr[2] ) {
					out.append( tr );
					continue;
				}
				int m[3];
				int count = 0;
				for ( int e = 0; e < 3; e++ ) {
					m[e] = mid.value( ekey( tr[e], tr[( e + 1 ) % 3] ), -1 );
					if ( m[e] >= 0 )
						count++;
				}
				if ( count == 0 ) {
					out.append( tr );
					continue;
				}
				touched << tr[0] << tr[1] << tr[2];
				for ( int e = 0; e < 3; e++ )
					if ( m[e] >= 0 )
						touched << m[e];
				const quint16 a = tr[0], b = tr[1], c = tr[2];
				const int mab = m[0], mbc = m[1], mca = m[2];
				if ( count == 3 ) {
					out.append( Triangle( a, quint16( mab ), quint16( mca ) ) );
					out.append( Triangle( quint16( mab ), b, quint16( mbc ) ) );
					out.append( Triangle( quint16( mca ), quint16( mbc ), c ) );
					out.append( Triangle( quint16( mab ), quint16( mbc ), quint16( mca ) ) );
				} else if ( count == 2 ) {
					quint16 p = a, q = b, r = c;
					int m1 = mab, m2 = mbc;
					if ( mab >= 0 && mca >= 0 ) {
						p = c; q = a; r = b;
						m1 = mca; m2 = mab;
					} else if ( mbc >= 0 && mca >= 0 ) {
						p = b; q = c; r = a;
						m1 = mbc; m2 = mca;
					}
					out.append( Triangle( quint16( m1 ), q, quint16( m2 ) ) );
					out.append( Triangle( p, quint16( m1 ), quint16( m2 ) ) );
					out.append( Triangle( p, quint16( m2 ), r ) );
				} else {
					quint16 p = a, q = b, r = c;
					int m1 = mab;
					if ( mbc >= 0 ) {
						p = b; q = c; r = a;
						m1 = mbc;
					} else if ( mca >= 0 ) {
						p = c; q = a; r = b;
						m1 = mca;
					}
					out.append( Triangle( p, quint16( m1 ), r ) );
					out.append( Triangle( quint16( m1 ), q, r ) );
				}
			}
			model->set<int>( iS, "Num Triangles", out.size() );
			model->updateArraySize( iT );
			for ( int t = 0; t < out.size(); t++ )
				model->set<Triangle>( model->getIndex( iT, t ), out.at( t ) );
			if ( stride > 0 )
				model->set<int>( iS, "Data Size",
					( nv + int( validPokes.size() ) + int( validCuts.size() ) ) * stride
						+ out.size() * 6 );
			tlRecalcNormalsSubset( model, iS, touched );
			tlUpdateBounds( model, iS );
			model->restoreState();
			model->dataChanged( QModelIndex( iS ), QModelIndex( iS ) );
		};
		if ( model->undoStack )
			model->undoStack->push( new TlShapeStateCommand( model, iShape, tr( "Knife" ), applyKnife ) );
		else
			applyKnife();
		// the cut rebuilt the triangle list: edge/face picks on this shape
		// would silently point at the wrong elements (vertex picks survive)
		for ( int i = pickedElems.size() - 1; i >= 0; i-- )
			if ( pickedElems.at( i ).shapeBlock == sb && pickedElems.at( i ).type != 1 )
				pickedElems.remove( i );
		totalCuts += cuts.size();
		totalPokes += pokes.size();
	}
	if ( macroU )
		model->undoStack->endMacro();

	modelChanged();
	if ( totalCuts == 0 && totalPokes == 0 )
		emit gizmoStatus( tr( "Knife: the line crossed no edges" ) );
	else
		emit gizmoStatus( tr( "Knife: cut %1 edge(s), poked %2 interior point(s)" )
			.arg( totalCuts ).arg( totalPokes ) );
	update();
}

// ---------------------------------------------------------------------------
// Loop Cut (Ctrl+R)

void GLView::loopCut()
{
	if ( !model || !editMode ) {
		emit gizmoStatus( tr( "Loop Cut needs edit mode" ) );
		return;
	}
	if ( gizmoMode != 0 )
		return;
	// the edge under the cursor seeds the ring (same trick as edge-loop select)
	const int savedPickMode = pickMode;
	pickMode = 2;
	PickedElement pe;
	const bool picked = pickElementUnder( QPointF( mapFromGlobal( QCursor::pos() ) ), pe );
	pickMode = savedPickMode;
	if ( !picked || pe.type != 2 || pe.shapeBlock < 0 ) {
		emit gizmoStatus( tr( "Loop Cut: hover an edge of the edited mesh" ) );
		return;
	}
	const int sb = pe.shapeBlock;
	QModelIndex iShape = model->getBlockIndex( sb );
	if ( !model->blockInherits( iShape, "BSTriShape" ) ) {
		emit gizmoStatus( tr( "Loop Cut is supported on FO4 (BSTriShape) meshes only" ) );
		return;
	}
	QModelIndex iVD = model->getIndex( iShape, "Vertex Data" );
	QModelIndex iTris = model->getIndex( iShape, "Triangles" );
	const int numTris = model->get<int>( iShape, "Num Triangles" );
	QVector<Triangle> tris( numTris );
	for ( int t = 0; t < numTris; t++ )
		tris[t] = model->get<Triangle>( model->getIndex( iTris, t ) );
	auto degenerate = []( const Triangle & t ) {
		return t[0] == t[1] || t[1] == t[2] || t[0] == t[2];
	};
	auto ekey = []( int a, int b ) {
		if ( a > b ) std::swap( a, b );
		return ( quint64( quint32( a ) ) << 32 ) | quint32( b );
	};
	QHash<quint64, QVector<int>> edgeTris;
	for ( int t = 0; t < numTris; t++ ) {
		if ( degenerate( tris.at( t ) ) )
			continue;
		for ( int e = 0; e < 3; e++ )
			edgeTris[ekey( tris[t][e], tris[t][( e + 1 ) % 3] )] << t;
	}
	auto readPos = [&]( int v ) {
		return model->get<Vector3>( model->getIndex( iVD, v ), "Vertex" );
	};
	auto thirdVert = [&]( int t, int a, int b ) {
		for ( int c = 0; c < 3; c++ )
			if ( tris[t][c] != a && tris[t][c] != b )
				return int( tris[t][c] );
		return -1;
	};

	// ring walk: from oriented edge (a,b) + the walk-side triangle, hop the
	// tri-pair "quad" to the parallel edge; orientation keeps a on the a-chain.
	// An explicitly marked quad diagonal (Make Face / Tris to Quads) always
	// wins over the parallel-direction guess.
	struct RingStep { int a2 = -1, b2 = -1, triA = -1, triB = -1; };
	auto step = [&]( int a, int b, int triA ) -> RingStep {
		RingStep r;
		if ( triA < 0 )
			return r;
		const int c = thirdVert( triA, a, b );
		if ( c < 0 )
			return r;
		const Vector3 eDir = ( readPos( b ) - readPos( a ) );
		float bestDot = -1.0f;
		for ( int x : { a, b } ) {
			const QVector<int> adj = edgeTris.value( ekey( x, c ) );
			int triB = -1;
			for ( int t : adj )
				if ( t != triA )
					triB = t;
			if ( triB < 0 )
				continue;
			const int y = thirdVert( triB, x, c );
			if ( y < 0 || y == a || y == b )
				continue;
			const bool marked = isQuadDiagonal( sb, x, c );
			Vector3 f = readPos( y ) - readPos( c );
			float d = std::fabs( Vector3::dotproduct( f, eDir ) )
				/ std::max( f.length() * eDir.length(), 1.0e-9f );
			if ( marked )
				d = 2.0f;	// beats any geometric score
			if ( d > bestDot ) {
				bestDot = d;
				r.triA = triA;
				r.triB = triB;
				if ( x == b ) {		// diag leaves b: c sits on the a-chain
					r.a2 = c;
					r.b2 = y;
				} else {			// diag leaves a: c sits on the b-chain
					r.a2 = y;
					r.b2 = c;
				}
			}
		}
		return r;
	};

	struct RingEdge { int a, b; };
	QVector<RingEdge> ring;
	QVector<QPair<int, int>> quads;	// (triA, triB) between ring[i] and ring[i+1]
	bool closed = false;
	{
		const int a0 = pe.e0, b0 = pe.e1;
		const QVector<int> seedTris = edgeTris.value( ekey( a0, b0 ) );
		QSet<int> usedTris;
		// direction 1
		QVector<RingEdge> fwd;
		QVector<QPair<int, int>> fq;
		int a = a0, b = b0;
		int walkTri = seedTris.value( 0, -1 );
		fwd.append( { a, b } );
		while ( true ) {
			RingStep r = step( a, b, walkTri );
			if ( r.a2 < 0 || usedTris.contains( r.triA ) || usedTris.contains( r.triB ) )
				break;
			usedTris << r.triA << r.triB;
			fq.append( { r.triA, r.triB } );
			if ( ( r.a2 == a0 && r.b2 == b0 ) || ( r.a2 == b0 && r.b2 == a0 ) ) {
				closed = true;
				break;
			}
			fwd.append( { r.a2, r.b2 } );
			a = r.a2;
			b = r.b2;
			const QVector<int> adj = edgeTris.value( ekey( a, b ) );
			walkTri = -1;
			for ( int t : adj )
				if ( !usedTris.contains( t ) )
					walkTri = t;
			if ( walkTri < 0 )
				break;
		}
		ring = fwd;
		quads = fq;
		if ( !closed && seedTris.size() > 1 ) {
			// direction 2: walk the other side, then splice reversed in front
			QVector<RingEdge> bwd;
			QVector<QPair<int, int>> bq;
			a = a0;
			b = b0;
			walkTri = seedTris.value( 1, -1 );
			while ( true ) {
				RingStep r = step( a, b, walkTri );
				if ( r.a2 < 0 || usedTris.contains( r.triA ) || usedTris.contains( r.triB ) )
					break;
				usedTris << r.triA << r.triB;
				bq.append( { r.triA, r.triB } );
				bwd.append( { r.a2, r.b2 } );
				a = r.a2;
				b = r.b2;
				const QVector<int> adj = edgeTris.value( ekey( a, b ) );
				walkTri = -1;
				for ( int t : adj )
					if ( !usedTris.contains( t ) )
						walkTri = t;
				if ( walkTri < 0 )
					break;
			}
			// prepend the backward walk (reversed order, same chain sides)
			QVector<RingEdge> all;
			QVector<QPair<int, int>> allQ;
			for ( int i = bwd.size() - 1; i >= 0; i-- )
				all.append( bwd.at( i ) );
			all += ring;
			for ( int i = bq.size() - 1; i >= 0; i-- )
				allQ.append( bq.at( i ) );
			allQ += quads;
			ring = all;
			quads = allQ;
		}
	}
	if ( quads.isEmpty() ) {
		emit gizmoStatus( tr( "Loop Cut: no ring found from this edge (boundary or pole)" ) );
		return;
	}

	const QPersistentModelIndex pShape( iShape );
	QVector<QPair<int, int>> ringEdges;
	for ( const RingEdge & e : std::as_const( ring ) )
		ringEdges.append( { e.a, e.b } );
	const QVector<QPair<int, int>> quadTris = quads;
	const bool ringClosed = closed;

	auto applyLoopCut = [this, pShape, ringEdges, quadTris, ringClosed]( int cuts ) {
		QModelIndex iS( pShape );
		if ( !iS.isValid() )
			return;
		QModelIndex iVD2 = model->getIndex( iS, "Vertex Data" );
		QModelIndex iT = model->getIndex( iS, "Triangles" );
		const int nv = model->get<int>( iS, "Num Vertices" );
		const int nt = model->get<int>( iS, "Num Triangles" );
		const int ds = model->get<int>( iS, "Data Size" );
		const int stride = ( nv > 0 ) ? ( ds - nt * 6 ) / nv : 0;
		cuts = std::clamp( cuts, 1, 64 );
		if ( nv + cuts * ringEdges.size() > 0xFFFF )
			cuts = std::max( 1, int( ( 0xFFFF - nv ) / std::max( ringEdges.size(), qsizetype( 1 ) ) ) );
		auto readP = [&]( int v ) {
			return model->get<Vector3>( model->getIndex( iVD2, v ), "Vertex" );
		};

		model->setState( BaseModel::Processing );
		// cut verts per ring edge, ordered a -> b
		model->set<int>( iS, "Num Vertices", nv + cuts * ringEdges.size() );
		model->updateArraySize( iVD2 );
		QVector<QVector<int>> cutsOf( ringEdges.size() );
		int nvi = nv;
		for ( int i = 0; i < ringEdges.size(); i++ ) {
			cutsOf[i].resize( cuts );
			for ( int k = 0; k < cuts; k++ ) {
				tlWriteLerpVertex( model, iS, nvi, ringEdges.at( i ).first,
					ringEdges.at( i ).second, float( k + 1 ) / float( cuts + 1 ) );
				cutsOf[i][k] = nvi++;
			}
		}
		// ladder re-triangulation per quad
		QVector<Triangle> tv( nt );
		for ( int t = 0; t < nt; t++ )
			tv[t] = model->get<Triangle>( model->getIndex( iT, t ) );
		QVector<Triangle> extra;
		QSet<int> touched;
		for ( int q = 0; q < quadTris.size(); q++ ) {
			const int i = q;
			const int j = ( q + 1 ) % ringEdges.size();
			// rows of the ladder: a-chain, the cut rungs, then the b-chain
			QVector<QPair<int, int>> rows;
			rows.append( { ringEdges.at( i ).first, ringEdges.at( j ).first } );
			for ( int k = 0; k < cuts; k++ )
				rows.append( { cutsOf.at( i ).at( k ), cutsOf.at( j ).at( k ) } );
			rows.append( { ringEdges.at( i ).second, ringEdges.at( j ).second } );
			// reference winding from the quad's first original triangle
			const Triangle ot = tv.at( quadTris.at( q ).first );
			Vector3 refN = Vector3::crossproduct( readP( ot[1] ) - readP( ot[0] ),
				readP( ot[2] ) - readP( ot[0] ) );
			QVector<Triangle> cell;
			for ( int k = 0; k + 1 < rows.size(); k++ ) {
				const quint16 u0 = quint16( rows.at( k ).first );
				const quint16 v0 = quint16( rows.at( k ).second );
				const quint16 u1 = quint16( rows.at( k + 1 ).first );
				const quint16 v1 = quint16( rows.at( k + 1 ).second );
				cell.append( Triangle( u0, v0, v1 ) );
				cell.append( Triangle( u0, v1, u1 ) );
			}
			for ( Triangle & t : cell ) {
				Vector3 n = Vector3::crossproduct( readP( t[1] ) - readP( t[0] ),
					readP( t[2] ) - readP( t[0] ) );
				if ( Vector3::dotproduct( n, refN ) < 0.0f )
					std::swap( t[1], t[2] );
				touched << t[0] << t[1] << t[2];
			}
			// first two cells replace the quad's original triangles in place
			tv[quadTris.at( q ).first] = cell.at( 0 );
			tv[quadTris.at( q ).second] = cell.at( 1 );
			for ( int k = 2; k < cell.size(); k++ )
				extra.append( cell.at( k ) );
		}
		model->set<int>( iS, "Num Triangles", nt + extra.size() );
		model->updateArraySize( iT );
		for ( int t = 0; t < nt; t++ )
			model->set<Triangle>( model->getIndex( iT, t ), tv.at( t ) );
		for ( int t = 0; t < extra.size(); t++ )
			model->set<Triangle>( model->getIndex( iT, nt + t ), extra.at( t ) );
		if ( stride > 0 )
			model->set<int>( iS, "Data Size",
				( nv + cuts * int( ringEdges.size() ) ) * stride + ( nt + extra.size() ) * 6 );
		tlRecalcNormalsSubset( model, iS, touched );
		model->restoreState();
		model->dataChanged( QModelIndex( iS ), QModelIndex( iS ) );
	};

	if ( !model->undoStack )
		return;
	const QVector<PickedElement> seed = pickedElems;
	const int base = model->undoStack->index();
	model->undoStack->push( new TlShapeStateCommand( model, iShape, tr( "Loop Cut" ),
		[applyLoopCut]() { applyLoopCut( 1 ); } ) );
	modelChanged();
	lastOpExRerun = [this, pShape, applyLoopCut]( const QVector<TlOpParam> & ps ) {
		QModelIndex iS( pShape );
		if ( !iS.isValid() )
			return;
		const int cuts = std::clamp( int( ps.value( 0 ).value + 0.5 ), 1, 64 );
		model->undoStack->push( new TlShapeStateCommand( model, iS, tr( "Loop Cut" ),
			[applyLoopCut, cuts]() { applyLoopCut( cuts ); } ) );
		modelChanged();
	};
	QVector<TlOpParam> ps( 1 );
	ps[0].label = tr( "Number of Cuts" );
	ps[0].type = TlOpParam::Int;
	ps[0].value = 1.0;
	ps[0].mn = 1.0;
	ps[0].mx = 64.0;
	ps[0].step = 1.0;
	armOperatorPanelEx( tr( "Loop Cut" ), ps, model->undoStack->index() - base, seed );
	emit gizmoStatus( tr( "Loop Cut: %1 quad(s) %2 — scrub Number of Cuts in the panel" )
		.arg( quads.size() ).arg( closed ? tr( "(closed ring)" ) : tr( "(open ring)" ) ) );
}

// ---------------------------------------------------------------------------
// Dissolve Vertices (Ctrl+X) / Symmetrize

void GLView::dissolveVerts()
{
	int sb = -1;
	QSet<int> sv;
	if ( !vertexOpTarget( sb, sv, "Dissolve" ) )
		return;
	QModelIndex iShape = model->getBlockIndex( sb );
	const QPersistentModelIndex pShape( iShape );
	const QVector<int> victims( sv.constBegin(), sv.constEnd() );

	auto applyDissolve = [this, pShape, victims]() {
		QModelIndex iS( pShape );
		if ( !iS.isValid() )
			return;
		QModelIndex iVD = model->getIndex( iS, "Vertex Data" );
		QModelIndex iT = model->getIndex( iS, "Triangles" );
		const int nv = model->get<int>( iS, "Num Vertices" );
		const int nt = model->get<int>( iS, "Num Triangles" );
		const int ds = model->get<int>( iS, "Data Size" );
		const int stride = ( nv > 0 ) ? ( ds - nt * 6 ) / nv : 0;
		QVector<Triangle> tv( nt );
		for ( int t = 0; t < nt; t++ )
			tv[t] = model->get<Triangle>( model->getIndex( iT, t ) );
		QVector<Vector3> pos( nv );
		for ( int i = 0; i < nv; i++ )
			pos[i] = model->get<Vector3>( model->getIndex( iVD, i ), "Vertex" );

		QSet<int> removed;
		int skipped = 0;
		for ( int v : victims ) {
			if ( v < 0 || v >= nv || removed.contains( v ) )
				continue;
			// the vert's 1-ring: incident tris and the boundary loop around it
			QVector<int> incident;
			QHash<int, int> next;	// directed rim edges (tri winding order)
			bool bad = false;
			for ( int t = 0; t < tv.size() && !bad; t++ ) {
				const Triangle & tr = tv.at( t );
				if ( tr[0] != v && tr[1] != v && tr[2] != v )
					continue;
				if ( tr[0] == tr[1] || tr[1] == tr[2] || tr[0] == tr[2] ) {
					incident.append( t );	// scaffold: drop with the vert
					continue;
				}
				incident.append( t );
				for ( int e = 0; e < 3; e++ ) {
					const int a = tr[e], b = tr[( e + 1 ) % 3];
					if ( a == v || b == v )
						continue;
					if ( next.contains( a ) ) {
						bad = true;	// non-manifold fan
						break;
					}
					next.insert( a, b );
				}
			}
			if ( bad || next.isEmpty() ) {
				skipped++;
				continue;
			}
			// chain the rim into ONE closed loop (interior verts only, v1)
			QVector<int> loop;
			const int start = next.constBegin().key();
			int cur = start;
			bool closedLoop = false;
			for ( int guard = 0; guard <= next.size(); guard++ ) {
				loop.append( cur );
				auto it = next.constFind( cur );
				if ( it == next.constEnd() )
					break;
				cur = it.value();
				if ( cur == start ) {
					closedLoop = true;
					break;
				}
			}
			if ( !closedLoop || loop.size() != next.size() || loop.size() < 3 ) {
				skipped++;	// boundary vert or split fan: leave it
				continue;
			}
			// remove the incident tris, cap the loop (tri-winding rim order =
			// cap order: the shared-edge rule keeps the cap facing outward)
			std::sort( incident.begin(), incident.end(), std::greater<int>() );
			for ( int t : std::as_const( incident ) )
				tv.removeAt( t );
			QVector<Vector3> lpos( loop.size() );
			for ( int i = 0; i < loop.size(); i++ )
				lpos[i] = pos.at( loop.at( i ) );
			const QVector<Triangle> cap = tlEarClip( lpos );
			for ( const Triangle & c : cap )
				tv.append( Triangle( quint16( loop.at( c[0] ) ), quint16( loop.at( c[1] ) ),
					quint16( loop.at( c[2] ) ) ) );
			removed << v;
		}
		if ( removed.isEmpty() ) {
			emit gizmoStatus( tr( "Dissolve: nothing dissolvable (boundary or non-manifold verts)" ) );
			return;
		}

		model->setState( BaseModel::Processing );
		// compact the removed verts (forward row copy) and remap the triangles
		QVector<int> remap( nv, -1 );
		int j = 0;
		for ( int i = 0; i < nv; i++ ) {
			if ( removed.contains( i ) )
				continue;
			if ( j != i )
				tlCopyItemValues( model, model->getIndex( iVD, i ), model->getIndex( iVD, j ) );
			remap[i] = j++;
		}
		QVector<Triangle> kept;
		kept.reserve( tv.size() );
		for ( const Triangle & tr : std::as_const( tv ) ) {
			const int a = remap[tr[0]], b = remap[tr[1]], c = remap[tr[2]];
			if ( a < 0 || b < 0 || c < 0 )
				continue;
			kept.append( Triangle( quint16( a ), quint16( b ), quint16( c ) ) );
		}
		model->set<int>( iS, "Num Vertices", j );
		model->updateArraySize( iVD );
		model->set<int>( iS, "Num Triangles", kept.size() );
		model->updateArraySize( iT );
		for ( int t = 0; t < kept.size(); t++ )
			model->set<Triangle>( model->getIndex( iT, t ), kept.at( t ) );
		if ( stride > 0 )
			model->set<int>( iS, "Data Size", j * stride + kept.size() * 6 );
		// the rim set is hard to track through the remap: recompute the whole
		// shape's normals (cheap enough, and always correct)
		QSet<int> all;
		for ( int i = 0; i < j; i++ )
			all << i;
		tlRecalcNormalsSubset( model, iS, all );
		tlUpdateBounds( model, iS );
		model->restoreState();
		model->dataChanged( QModelIndex( iS ), QModelIndex( iS ) );
		emit gizmoStatus( tr( "Dissolved %1 vert(s)%2" ).arg( removed.size() )
			.arg( victims.size() - removed.size() > 0
				? tr( " (%1 skipped: boundary/non-manifold)" ).arg( victims.size() - removed.size() )
				: QString() ) );
	};
	if ( model->undoStack )
		model->undoStack->push( new TlShapeStateCommand( model, iShape, tr( "Dissolve" ), applyDissolve ) );
	else
		applyDissolve();
	pickedElems.clear();
	modelChanged();
}

void GLView::symmetrizeShape()
{
	// operates on the whole active edited mesh (Blender's Symmetrize)
	if ( !model || !editMode || editShapeBlock < 0 ) {
		emit gizmoStatus( tr( "Symmetrize needs edit mode" ) );
		return;
	}
	const int sb = editShapeBlock;
	QModelIndex iShape = model->getBlockIndex( sb );
	if ( !model->blockInherits( iShape, "BSTriShape" ) ) {
		emit gizmoStatus( tr( "Symmetrize is supported on FO4 (BSTriShape) meshes only" ) );
		return;
	}
	const QPersistentModelIndex pShape( iShape );

	// axis: 0..5 = +X→−X, −X→+X, +Y→−Y, −Y→+Y, +Z→−Z, −Z→+Z
	auto applySymmetrize = [this, pShape]( int axisMode, float mergeDist ) {
		QModelIndex iS( pShape );
		if ( !iS.isValid() )
			return;
		const int axis = axisMode / 2;
		const float keepSign = ( axisMode % 2 == 0 ) ? 1.0f : -1.0f;
		mergeDist = std::max( mergeDist, 1.0e-6f );
		QModelIndex iVD = model->getIndex( iS, "Vertex Data" );
		QModelIndex iT = model->getIndex( iS, "Triangles" );
		const int nv = model->get<int>( iS, "Num Vertices" );
		const int nt = model->get<int>( iS, "Num Triangles" );
		const int ds = model->get<int>( iS, "Data Size" );
		const int stride = ( nv > 0 ) ? ( ds - nt * 6 ) / nv : 0;
		QVector<Vector3> pos( nv );
		for ( int i = 0; i < nv; i++ )
			pos[i] = model->get<Vector3>( model->getIndex( iVD, i ), "Vertex" );
		QVector<Triangle> tv( nt );
		for ( int t = 0; t < nt; t++ )
			tv[t] = model->get<Triangle>( model->getIndex( iT, t ) );

		// classes: keep (kept side), plane (welds), discard (mirrored side)
		enum { Keep, Plane, Discard };
		QVector<int> cls( nv );
		int keepCount = 0;
		for ( int i = 0; i < nv; i++ ) {
			const float c = pos.at( i )[axis] * keepSign;
			cls[i] = ( c > mergeDist ) ? Keep : ( c >= -mergeDist ? Plane : Discard );
			if ( cls.at( i ) != Discard )
				keepCount++;
		}
		int mirrorCount = 0;
		for ( int i = 0; i < nv; i++ )
			if ( cls.at( i ) == Keep )
				mirrorCount++;
		if ( keepCount + mirrorCount > 0xFFFF ) {
			emit gizmoStatus( tr( "Symmetrize: result would exceed the 65,535-vertex limit" ) );
			return;
		}

		model->setState( BaseModel::Processing );
		// 1) compact away the discarded side; snap plane verts onto the plane
		QVector<int> remap( nv, -1 );
		int j = 0;
		for ( int i = 0; i < nv; i++ ) {
			if ( cls.at( i ) == Discard )
				continue;
			if ( j != i )
				tlCopyItemValues( model, model->getIndex( iVD, i ), model->getIndex( iVD, j ) );
			if ( cls.at( i ) == Plane ) {
				Vector3 p = pos.at( i );
				p[axis] = 0.0f;
				tlSetVertexLocal( model, iS, j, p );
			}
			remap[i] = j++;
		}
		// 2) mirrored copies of the kept-side verts
		QVector<int> mirrorOf( nv, -1 );
		model->set<int>( iS, "Num Vertices", j + mirrorCount );
		model->updateArraySize( iVD );
		int mi = j;
		for ( int i = 0; i < nv; i++ ) {
			if ( cls.at( i ) != Keep )
				continue;
			tlCopyItemValues( model, model->getIndex( iVD, remap.at( i ) ), model->getIndex( iVD, mi ) );
			Vector3 p = pos.at( i );
			p[axis] = -p[axis];
			tlSetVertexLocal( model, iS, mi, p );
			for ( const char * attr : { "Normal", "Tangent" } ) {
				QModelIndex row = model->getIndex( iVD, mi );
				if ( !model->getItem( row, attr ) )
					continue;
				Vector3 n = model->get<Vector3>( row, attr );
				n[axis] = -n[axis];
				model->set<ByteVector3>( row, attr, n );
			}
			mirrorOf[i] = mi++;
		}
		// 3) triangles: kept-side tris survive; crossing tris are dropped (v1,
		//    no bisection); mirrored copies with flipped winding
		QVector<Triangle> out;
		out.reserve( tv.size() * 2 );
		for ( const Triangle & tr : std::as_const( tv ) ) {
			if ( cls.at( tr[0] ) == Discard || cls.at( tr[1] ) == Discard
				|| cls.at( tr[2] ) == Discard )
				continue;
			out.append( Triangle( quint16( remap[tr[0]] ), quint16( remap[tr[1]] ),
				quint16( remap[tr[2]] ) ) );
			// mirror (plane verts map to themselves); skip tris fully on the plane
			const int m0 = ( cls.at( tr[0] ) == Keep ) ? mirrorOf[tr[0]] : remap[tr[0]];
			const int m1 = ( cls.at( tr[1] ) == Keep ) ? mirrorOf[tr[1]] : remap[tr[1]];
			const int m2 = ( cls.at( tr[2] ) == Keep ) ? mirrorOf[tr[2]] : remap[tr[2]];
			if ( cls.at( tr[0] ) == Keep || cls.at( tr[1] ) == Keep || cls.at( tr[2] ) == Keep )
				out.append( Triangle( quint16( m0 ), quint16( m2 ), quint16( m1 ) ) );
		}
		model->set<int>( iS, "Num Triangles", out.size() );
		model->updateArraySize( iT );
		for ( int t = 0; t < out.size(); t++ )
			model->set<Triangle>( model->getIndex( iT, t ), out.at( t ) );
		if ( stride > 0 )
			model->set<int>( iS, "Data Size", ( j + mirrorCount ) * stride + out.size() * 6 );
		QSet<int> planeVerts;
		for ( int i = 0; i < nv; i++ )
			if ( cls.at( i ) == Plane && remap.at( i ) >= 0 )
				planeVerts << remap.at( i );
		tlRecalcNormalsSubset( model, iS, planeVerts );
		tlUpdateBounds( model, iS );
		model->restoreState();
		model->dataChanged( QModelIndex( iS ), QModelIndex( iS ) );
	};

	if ( !model->undoStack )
		return;
	const QVector<PickedElement> seed = pickedElems;
	const int base = model->undoStack->index();
	model->undoStack->push( new TlShapeStateCommand( model, iShape, tr( "Symmetrize" ),
		[applySymmetrize]() { applySymmetrize( 0, 0.001f ); } ) );
	pickedElems.clear();
	modelChanged();
	lastOpExRerun = [this, pShape, applySymmetrize]( const QVector<TlOpParam> & ps ) {
		QModelIndex iS( pShape );
		if ( !iS.isValid() )
			return;
		const int axisMode = std::clamp( int( ps.value( 0 ).value + 0.5 ), 0, 5 );
		const float mergeDist = float( ps.value( 1 ).value );
		model->undoStack->push( new TlShapeStateCommand( model, iS, tr( "Symmetrize" ),
			[applySymmetrize, axisMode, mergeDist]() { applySymmetrize( axisMode, mergeDist ); } ) );
		pickedElems.clear();
		modelChanged();
	};
	QVector<TlOpParam> ps( 2 );
	ps[0].label = tr( "Direction" );
	ps[0].type = TlOpParam::Enum;
	ps[0].value = 0.0;
	ps[0].enumNames = QStringList()
		<< QStringLiteral( "+X → −X" ) << QStringLiteral( "−X → +X" )
		<< QStringLiteral( "+Y → −Y" ) << QStringLiteral( "−Y → +Y" )
		<< QStringLiteral( "+Z → −Z" ) << QStringLiteral( "−Z → +Z" );
	ps[1].label = tr( "Merge Distance" );
	ps[1].type = TlOpParam::Float;
	ps[1].value = 0.001;
	ps[1].mn = 0.0;
	ps[1].mx = 10.0;
	ps[1].step = 0.001;
	ps[1].decimals = 4;
	armOperatorPanelEx( tr( "Symmetrize" ), ps, model->undoStack->index() - base, seed );
	emit gizmoStatus( tr( "Symmetrized (crossing triangles are dropped, not bisected — v1)" ) );
}

// ---------------------------------------------------------------------------
// Add Primitive (Shift+A, object mode)

struct TlPrimVert
{
	Vector3 pos, nrm;
	Vector2 uv;
};

static void tlMakePrimitive( int kind, float size, int segs,
	QVector<TlPrimVert> & verts, QVector<Triangle> & tris )
{
	verts.clear();
	tris.clear();
	const float h = size * 0.5f;
	auto quad = [&]( int a, int b, int c, int d ) {
		tris.append( Triangle( quint16( a ), quint16( b ), quint16( c ) ) );
		tris.append( Triangle( quint16( a ), quint16( c ), quint16( d ) ) );
	};
	if ( kind == 0 ) {
		// plane on XY, +Z normal
		const Vector3 n( 0, 0, 1 );
		verts.append( { Vector3( -h, -h, 0 ), n, Vector2( 0, 1 ) } );
		verts.append( { Vector3( h, -h, 0 ), n, Vector2( 1, 1 ) } );
		verts.append( { Vector3( h, h, 0 ), n, Vector2( 1, 0 ) } );
		verts.append( { Vector3( -h, h, 0 ), n, Vector2( 0, 0 ) } );
		quad( 0, 1, 2, 3 );
	} else if ( kind == 1 ) {
		// cube: per-face verts for hard normals
		static const int axes[6][2] = { { 0, 1 }, { 0, 1 }, { 0, 2 }, { 0, 2 }, { 1, 2 }, { 1, 2 } };
		static const int naxis[6] = { 2, 2, 1, 1, 0, 0 };
		static const float nsign[6] = { 1, -1, 1, -1, 1, -1 };
		for ( int f = 0; f < 6; f++ ) {
			Vector3 n;
			n[naxis[f]] = nsign[f];
			const int u = axes[f][0], v = axes[f][1];
			const int base = verts.size();
			for ( int k = 0; k < 4; k++ ) {
				static const float cu[4] = { -1, 1, 1, -1 };
				static const float cv[4] = { -1, -1, 1, 1 };
				Vector3 p;
				p[naxis[f]] = nsign[f] * h;
				p[u] = cu[k] * h;
				p[v] = cv[k] * h;
				verts.append( { p, n, Vector2( ( cu[k] + 1 ) * 0.5f, ( 1 - cv[k] ) * 0.5f ) } );
			}
			// consistent outward winding: flip for the negative faces
			if ( nsign[f] > 0 )
				quad( base, base + 1, base + 2, base + 3 );
			else
				quad( base, base + 3, base + 2, base + 1 );
		}
	} else if ( kind == 2 ) {
		// cylinder along Z: split side verts (radial normals) + two caps
		const int n = std::clamp( segs, 3, 64 );
		for ( int i = 0; i <= n; i++ ) {
			const float ang = float( i % n ) / float( n ) * 6.2831853f;
			const Vector3 dir( std::cos( ang ), std::sin( ang ), 0.0f );
			const float u = float( i ) / float( n );
			verts.append( { dir * h + Vector3( 0, 0, -h ), dir, Vector2( u, 1 ) } );
			verts.append( { dir * h + Vector3( 0, 0, h ), dir, Vector2( u, 0 ) } );
		}
		for ( int i = 0; i < n; i++ )
			quad( i * 2, ( i + 1 ) * 2, ( i + 1 ) * 2 + 1, i * 2 + 1 );
		for ( int cap = 0; cap < 2; cap++ ) {
			const float z = cap ? h : -h;
			const Vector3 nn( 0, 0, cap ? 1.0f : -1.0f );
			const int center = verts.size();
			verts.append( { Vector3( 0, 0, z ), nn, Vector2( 0.5f, 0.5f ) } );
			const int rim = verts.size();
			for ( int i = 0; i < n; i++ ) {
				const float ang = float( i ) / float( n ) * 6.2831853f;
				const Vector3 dir( std::cos( ang ), std::sin( ang ), 0.0f );
				verts.append( { dir * h + Vector3( 0, 0, z ), nn,
					Vector2( 0.5f + dir[0] * 0.5f, 0.5f - dir[1] * 0.5f ) } );
			}
			for ( int i = 0; i < n; i++ ) {
				if ( cap )
					tris.append( Triangle( quint16( center ), quint16( rim + i ),
						quint16( rim + ( i + 1 ) % n ) ) );
				else
					tris.append( Triangle( quint16( center ), quint16( rim + ( i + 1 ) % n ),
						quint16( rim + i ) ) );
			}
		}
	} else {
		// UV sphere: (u+1) x (v+1) grid, radial normals
		const int su = std::clamp( segs, 3, 64 );
		const int sV = std::clamp( segs / 2, 2, 32 );
		for ( int r = 0; r <= sV; r++ ) {
			const float theta = float( r ) / float( sV ) * 3.14159265f;
			for ( int i = 0; i <= su; i++ ) {
				const float phi = float( i % su ) / float( su ) * 6.2831853f;
				const Vector3 dir( std::sin( theta ) * std::cos( phi ),
					std::sin( theta ) * std::sin( phi ), std::cos( theta ) );
				verts.append( { dir * h, dir,
					Vector2( float( i ) / float( su ), float( r ) / float( sV ) ) } );
			}
		}
		const int stride = su + 1;
		for ( int r = 0; r < sV; r++ )
			for ( int i = 0; i < su; i++ ) {
				const int a = r * stride + i, b = a + 1;
				const int c = a + stride, d = b + stride;
				if ( r > 0 )
					tris.append( Triangle( quint16( a ), quint16( b ), quint16( d ) ) );
				if ( r < sV - 1 )
					tris.append( Triangle( quint16( a ), quint16( d ), quint16( c ) ) );
			}
	}
}

void GLView::addPrimitive( int kind )
{
	if ( !model || editMode ) {
		emit gizmoStatus( tr( "Add Primitive works in object mode" ) );
		return;
	}
	// template: the active BSTriShape provides the vertex layout + material
	int tmpl = -1;
	if ( objActive >= 0
		&& model->blockInherits( model->getBlockIndex( objActive ), "BSTriShape" ) )
		tmpl = objActive;
	else
		for ( int b = 0; b < model->getBlockCount() && tmpl < 0; b++ )
			if ( model->blockInherits( model->getBlockIndex( b ), "BSTriShape" ) )
				tmpl = b;
	if ( tmpl < 0 ) {
		emit gizmoStatus( tr( "Add Primitive needs an existing BSTriShape as a layout/material template" ) );
		return;
	}
	if ( model->getLink( model->getBlockIndex( tmpl ), "Skin" ) >= 0 ) {
		emit gizmoStatus( tr( "Add Primitive: pick an unskinned template shape (this one is skinned)" ) );
		return;
	}
	static const char * primNames[4] = {
		QT_TR_NOOP( "Plane" ), QT_TR_NOOP( "Cube" ),
		QT_TR_NOOP( "Cylinder" ), QT_TR_NOOP( "Sphere" )
	};
	const Vector3 at = cursorPos;

	auto applyAdd = [this, tmpl, kind, at]( float size, int segs ) {
		// in-place undo: the primitive only APPENDS blocks (clone + props);
		// undo removes them and prunes the null child link
		auto parents = std::make_shared<QVector<int>>();
		auto madeShape = std::make_shared<int>( -1 );
		auto createPrim = [this, tmpl, kind, at, size, segs, parents, madeShape]() {
			*madeShape = -1;
			int newBlock = tlCloneShapeWithProps( model, tmpl );
			if ( newBlock < 0 )
				return;
			QModelIndex iNew = model->getBlockIndex( newBlock );
			const int parentNum = model->getParent( tmpl );
			if ( parentNum >= 0 ) {
				blockLink( model, model->getBlockIndex( parentNum ), iNew );
				parents->append( parentNum );
			}
			model->set<QString>( iNew, "Name",
				tlUniqueNodeName( model, tr( primNames[std::clamp( kind, 0, 3 )] ) ) );
			model->set<Vector3>( iNew, "Translation", at );

			QVector<TlPrimVert> pv;
			QVector<Triangle> pt;
			tlMakePrimitive( kind, size, segs, pv, pt );
			QModelIndex iVD = model->getIndex( iNew, "Vertex Data" );
			QModelIndex iT = model->getIndex( iNew, "Triangles" );
			const int oldNV = model->get<int>( iNew, "Num Vertices" );
			const int oldNT = model->get<int>( iNew, "Num Triangles" );
			const int ds = model->get<int>( iNew, "Data Size" );
			const int stride = ( oldNV > 0 ) ? ( ds - oldNT * 6 ) / oldNV : 0;
			model->setState( BaseModel::Processing );
			model->set<int>( iNew, "Num Vertices", pv.size() );
			model->updateArraySize( iVD );
			for ( int i = 0; i < pv.size(); i++ ) {
				QModelIndex row = model->getIndex( iVD, i );
				tlSetVertexLocal( model, iNew, i, pv.at( i ).pos );
				if ( model->getItem( row, "Normal" ) )
					model->set<ByteVector3>( row, "Normal", pv.at( i ).nrm );
				if ( model->getItem( row, "Tangent" ) ) {
					// any stable perpendicular will do until a real tangent pass
					Vector3 t = Vector3::crossproduct( pv.at( i ).nrm, Vector3( 0, 0, 1 ) );
					if ( t.squaredLength() < 1.0e-6f )
						t = Vector3( 1, 0, 0 );
					t.normalize();
					model->set<ByteVector3>( row, "Tangent", t );
				}
				if ( model->getItem( row, "UV" ) )
					model->set<HalfVector2>( row, "UV", HalfVector2( pv.at( i ).uv ) );
			}
			model->set<int>( iNew, "Num Triangles", pt.size() );
			model->updateArraySize( iT );
			for ( int t = 0; t < pt.size(); t++ )
				model->set<Triangle>( model->getIndex( iT, t ), pt.at( t ) );
			if ( stride > 0 )
				model->set<int>( iNew, "Data Size", pv.size() * stride + pt.size() * 6 );
			tlUpdateBounds( model, iNew );
			model->restoreState();
			*madeShape = newBlock;
		};
		if ( model->undoStack )
			model->undoStack->push( new TlBlockAppendCommand( model,
				tr( "Add %1" ).arg( tr( primNames[std::clamp( kind, 0, 3 )] ) ), createPrim, parents ) );
		else
			createPrim();
		if ( *madeShape >= 0 ) {
			modelChanged();
			syncObjectSelection( *madeShape );
			emit clicked( model->getBlockIndex( *madeShape ) );
		}
	};
	if ( !model->undoStack )
		return;
	const int base = model->undoStack->index();
	applyAdd( 1.0f, 16 );
	lastOpExRerun = [applyAdd]( const QVector<TlOpParam> & ps ) {
		applyAdd( float( ps.value( 0 ).value ), int( ps.value( 1 ).value + 0.5 ) );
	};
	QVector<TlOpParam> ps( 2 );
	ps[0].label = tr( "Size" );
	ps[0].type = TlOpParam::Float;
	ps[0].value = 1.0;
	ps[0].mn = 0.001;
	ps[0].mx = 10000.0;
	ps[0].step = 0.1;
	ps[0].decimals = 3;
	ps[1].label = tr( "Segments" );
	ps[1].type = TlOpParam::Int;
	ps[1].value = 16.0;
	ps[1].mn = 3.0;
	ps[1].mx = 64.0;
	ps[1].step = 1.0;
	armOperatorPanelEx( tr( "Add %1" ).arg( tr( primNames[std::clamp( kind, 0, 3 )] ) ), ps,
		model->undoStack->index() - base, pickedElems );
	emit gizmoStatus( tr( "Added %1 at the 3D cursor (Size / Segments in the panel)" )
		.arg( tr( primNames[std::clamp( kind, 0, 3 )] ) ) );
}

void GLView::showSpecialsMenu()
{
	if ( !model )
		return;
	const bool anyPaint = riggingWeightPaintModeActive() || vertexPaintModeActive()
		|| segmentPaintModeActive();
	AutoCloseMenu m;
	// the modal transforms head the quick menu in both modes (not while painting)
	if ( !anyPaint ) {
		m.addSection( tr( "Transform" ) );
		populateTransformMenu( &m );
	}
	if ( editMode ) {
		const bool hasSel = !pickedElems.isEmpty();
		m.addSection( tr( "Specials" ) );
		QAction * aSubd = m.addAction( tr( "Subdivide" ) );
		QAction * aSmooth = m.addAction( tr( "Smooth Vertices…" ) );
		QAction * aMerge = m.addAction( tr( "Merge…" ) );
		QAction * aDoubles = m.addAction( tr( "Remove Doubles…" ) );
		QAction * aDissolve = m.addAction( tr( "Dissolve Vertices" ) );
		m.addSeparator();
		QAction * aExtrude = m.addAction( tr( "Extrude Region…" ) );
		QAction * aFill = m.addAction( tr( "Make Face / Fill / Bridge…" ) );
		QAction * aInset = m.addAction( tr( "Inset Faces…" ) );
		QAction * aSlide = m.addAction( tr( "Edge Slide…" ) );
		QAction * aT2Q = m.addAction( tr( "Tris to Quads" ) );
		QAction * aTriang = m.addAction( tr( "Triangulate Faces" ) );
		m.addSeparator();
		QAction * aFlip = m.addAction( tr( "Flip Normals" ) );
		QAction * aRecalc = m.addAction( tr( "Recalculate Normals" ) );
		QAction * aSym = m.addAction( tr( "Symmetrize…" ) );
		m.addSeparator();
		QAction * aHide = m.addAction( tr( "Hide Selection" ) );
		QAction * aReveal = m.addAction( tr( "Reveal All" ) );
		QAction * aInvert = m.addAction( tr( "Invert Selection" ) );
		for ( QAction * a : { aSubd, aSmooth, aMerge, aDoubles, aDissolve, aExtrude,
			aFill, aInset, aSlide, aT2Q, aTriang, aFlip, aRecalc, aHide } )
			a->setEnabled( hasSel );
		QAction * r = m.exec( QCursor::pos() );
		if ( r == aSubd )
			subdivideSelection();
		else if ( r == aSmooth )
			smoothVertices();
		else if ( r == aMerge )
			showMergeMenu();
		else if ( r == aDoubles )
			mergeVertices( 2, lastMergeDistance );
		else if ( r == aDissolve )
			dissolveVerts();
		else if ( r == aExtrude )
			extrudeRegion();
		else if ( r == aFill )
			makeFace();
		else if ( r == aInset )
			insetRegion();
		else if ( r == aSlide )
			edgeSlide();
		else if ( r == aT2Q )
			trisToQuads();
		else if ( r == aTriang )
			triangulateSelection( 0 );
		else if ( r == aFlip )
			flipSelectedFaces();
		else if ( r == aRecalc )
			recalcSelectedNormals();
		else if ( r == aSym )
			symmetrizeShape();
		else if ( r == aHide )
			hideSelectedElements();
		else if ( r == aReveal )
			unhideAllElements();
		else if ( r == aInvert )
			invertSelection();
	} else {
		const bool hasSel = !objSelection.isEmpty();
		m.addSection( tr( "Specials" ) );
		QAction * aAdd = m.addAction( tr( "Add Primitive…" ) );
		QAction * aDup = m.addAction( tr( "Duplicate" ) );
		QAction * aJoin = m.addAction( tr( "Join" ) );
		m.addSeparator();
		QAction * aSnap = m.addAction( tr( "Snap…" ) );
		QAction * aOrigin = m.addAction( tr( "Set Origin…" ) );
		aDup->setEnabled( hasSel );
		aJoin->setEnabled( objSelection.size() > 1 );
		QAction * r = m.exec( QCursor::pos() );
		if ( r == aAdd )
			showAddPrimitiveMenu();
		else if ( r == aDup )
			duplicateSelection();
		else if ( r == aJoin )
			joinSelectedObjects();
		else if ( r == aSnap )
			showSnapMenu();
		else if ( r == aOrigin )
			showSetOriginMenu();
	}
}

void GLView::showAddPrimitiveMenu()
{
	if ( !model || editMode )
		return;
	AutoCloseMenu m;
	m.addSection( tr( "Add Primitive" ) );
	QAction * aPlane = m.addAction( tr( "Plane" ) );
	QAction * aCube = m.addAction( tr( "Cube" ) );
	QAction * aCyl = m.addAction( tr( "Cylinder" ) );
	QAction * aSph = m.addAction( tr( "UV Sphere" ) );
	QAction * r = m.exec( QCursor::pos() );
	if ( r == aPlane )
		addPrimitive( 0 );
	else if ( r == aCube )
		addPrimitive( 1 );
	else if ( r == aCyl )
		addPrimitive( 2 );
	else if ( r == aSph )
		addPrimitive( 3 );
}

void GLView::populateTransformMenu( QMenu * m )
{
	m->addAction( tr( "Move\tG" ), this, [this]() { startModalTransform( 1 ); } );
	m->addAction( tr( "Rotate\tR" ), this, [this]() { startModalTransform( 2 ); } );
	m->addAction( tr( "Scale\tS" ), this, [this]() { startModalTransform( 3 ); } );
}

void GLView::populateSelectMenu( QMenu * m )
{
	if ( !model )
		return;
	m->addAction( tr( "All\tA" ), this, [this]() { selectAll( 1 ); } );
	m->addAction( tr( "None" ), this, [this]() { selectAll( 2 ); } );
	m->addAction( tr( "Invert\tCtrl+I" ), this, [this]() { invertSelection(); } );
	if ( editMode )
		m->addAction( tr( "Checker Deselect…" ), this,
			[this]() { checkerDeselect(); } )->setEnabled( pickedElems.size() >= 2 );
	m->addSeparator();
	m->addAction( tr( "Box Select\tB" ), this, [this]() { beginBoxSelect(); } );
	m->addAction( tr( "Circle Select\tC" ), this, [this]() { beginCircleSelect(); } );
	if ( editMode ) {
		const bool hasSel = !pickedElems.isEmpty();
		m->addSeparator();
		m->addAction( tr( "Select More\tCtrl+=" ), this,
			[this]() { selectMoreLess( true ); } )->setEnabled( hasSel );
		m->addAction( tr( "Select Less\tCtrl+-" ), this,
			[this]() { selectMoreLess( false ); } )->setEnabled( hasSel );
		m->addSeparator();
		m->addAction( tr( "Select Linked\tCtrl+L" ), this,
			[this]() { selectLinked( false ); } )->setEnabled( hasSel );
		// grow across faces within the sharpness angle; the redo panel that
		// pops up afterwards lets you readjust the angle live
		m->addAction( tr( "Select Linked by Angle…\tCtrl+Alt+Shift+F" ), this, [this]() {
			selectLinked( true, ( lastOpKind == 2 ) ? lastOpParam : 30.0f );
		} )->setEnabled( hasSel );
	}
}

void GLView::populateAddMenu( QMenu * m )
{
	if ( !model )
		return;
	m->addAction( tr( "Plane" ), this, [this]() { addPrimitive( 0 ); } );
	m->addAction( tr( "Cube" ), this, [this]() { addPrimitive( 1 ); } );
	m->addAction( tr( "Cylinder" ), this, [this]() { addPrimitive( 2 ); } );
	m->addAction( tr( "UV Sphere" ), this, [this]() { addPrimitive( 3 ); } );
}

void GLView::populateObjectMenu( QMenu * m )
{
	if ( !model )
		return;
	const bool hasSel = !objSelection.isEmpty();
	populateTransformMenu( m->addMenu( tr( "Transform" ) ) );
	m->addSeparator();
	m->addAction( tr( "Snap…\tShift+S" ), this, [this]() { showSnapMenu(); } );
	m->addAction( tr( "Set Origin…\tShift+Ctrl+Alt+C" ), this, [this]() { showSetOriginMenu(); } );
	m->addSeparator();
	m->addAction( tr( "Duplicate\tShift+D" ), this,
		[this]() { duplicateSelection(); } )->setEnabled( hasSel );
	m->addAction( tr( "Join\tCtrl+J" ), this,
		[this]() { joinSelectedObjects(); } )->setEnabled( objSelection.size() >= 2 );
	m->addAction( tr( "Delete\tX" ), this,
		[this]() { deleteSelectedObjects(); } )->setEnabled( hasSel );
	QMenu * mPar = m->addMenu( tr( "Parent" ) );
	mPar->addAction( tr( "Set Parent…\tCtrl+P" ), this,
		[this]() { showParentMenu(); } )->setEnabled( hasSel );
	mPar->addAction( tr( "Clear Parent…\tAlt+P" ), this,
		[this]() { showClearParentMenu(); } )->setEnabled( hasSel );
	m->addSeparator();
	QMenu * mVis = m->addMenu( tr( "Show/Hide" ) );
	mVis->addAction( tr( "Hide\tH" ), this, [this]() { hideSelected(); } )->setEnabled( hasSel );
	mVis->addAction( tr( "Unhide All\tAlt+H" ), this, [this]() { unhideAll(); } );
}

void GLView::populateMeshMenu( QMenu * m )
{
	if ( !model )
		return;
	const bool hasSel = !pickedElems.isEmpty();
	populateTransformMenu( m->addMenu( tr( "Transform" ) ) );
	m->addSeparator();
	m->addAction( tr( "Snap…\tShift+S" ), this, [this]() { showSnapMenu(); } );
	m->addAction( tr( "Set Origin…\tShift+Ctrl+Alt+C" ), this, [this]() { showSetOriginMenu(); } );
	m->addSeparator();
	m->addAction( tr( "Extrude Region…\tE" ), this,
		[this]() { extrudeRegion(); } )->setEnabled( hasSel );
	m->addAction( tr( "Duplicate\tShift+D" ), this,
		[this]() { duplicateElements(); } )->setEnabled( hasSel );
	m->addAction( tr( "Separate…\tP" ), this,
		[this]() { showSeparateMenu(); } )->setEnabled( hasSel );
	m->addAction( tr( "Symmetrize…" ), this, [this]() { symmetrizeShape(); } );
	QMenu * mNorm = m->addMenu( tr( "Normals" ) );
	mNorm->addAction( tr( "Flip" ), this,
		[this]() { flipSelectedFaces(); } )->setEnabled( hasSel );
	mNorm->addAction( tr( "Recalculate" ), this,
		[this]() { recalcSelectedNormals(); } )->setEnabled( hasSel );
	m->addSeparator();
	QAction * aDecal = m->addAction( tr( "Create Floating Decal…" ), this,
		[this]() { createFloatingDecal(); } );
	aDecal->setEnabled( hasSel );
	aDecal->setToolTip( tr( "Copy selected faces to a separate shape and offset them along their normals" ) );
	m->addSeparator();
	QMenu * mVis = m->addMenu( tr( "Show/Hide" ) );
	mVis->addAction( tr( "Hide Selection\tH" ), this,
		[this]() { hideSelectedElements(); } )->setEnabled( hasSel );
	mVis->addAction( tr( "Unhide All\tAlt+H" ), this, [this]() { unhideAllElements(); } );
	m->addSeparator();
	m->addAction( tr( "Split\tY" ), this,
		[this]() { splitSelection(); } )->setEnabled( hasSel );
	m->addAction( tr( "Delete…\tX" ), this,
		[this]() { showDeleteMenu(); } )->setEnabled( hasSel );
	m->addSeparator();
	// Blender's Mirror X: modal transforms also move the unselected mirror
	// partner of each vertex (paired by position, X-negated)
	QAction * aMirror = m->addAction( tr( "Mirror Editing (X)" ) );
	aMirror->setCheckable( true );
	aMirror->setChecked( mirrorEditing );
	connect( aMirror, &QAction::toggled, this, [this]( bool on ) {
		mirrorEditing = on;
		QSettings settings;
		settings.setValue( "GLView/Edit/MirrorX", on );
		emit gizmoStatus( on ? tr( "Mirror editing ON (X axis, position-paired)" )
			: tr( "Mirror editing off" ) );
	} );
}

void GLView::populateVertexMenu( QMenu * m )
{
	if ( !model )
		return;
	const bool hasSel = !pickedElems.isEmpty();
	m->addAction( tr( "Merge…\tM" ), this,
		[this]() { showMergeMenu(); } )->setEnabled( hasSel );
	m->addAction( tr( "Remove Doubles…" ), this, [this]() {
		mergeVertices( 2, lastMergeDistance );
	} )->setEnabled( hasSel );
	m->addSeparator();
	m->addAction( tr( "Smooth Vertices…" ), this,
		[this]() { smoothVertices(); } )->setEnabled( hasSel );
	m->addAction( tr( "Dissolve Vertices\tCtrl+X" ), this,
		[this]() { dissolveVerts(); } )->setEnabled( hasSel );
	m->addSeparator();
	m->addAction( tr( "Rip Vertices\tV" ), this,
		[this]() { ripSelection(); } )->setEnabled( hasSel );
}

void GLView::populateEdgeMenu( QMenu * m )
{
	if ( !model )
		return;
	const bool hasSel = !pickedElems.isEmpty();
	m->addAction( tr( "Loop Cut…\tCtrl+R" ), this, [this]() { loopCut(); } );
	m->addAction( tr( "Knife…\tK" ), this, [this]() { beginKnife(); } );
	m->addAction( tr( "Bevel…\tCtrl+B" ), this,
		[this]() { bevelSelection(); } )->setEnabled( hasSel );
	m->addAction( tr( "Subdivide" ), this,
		[this]() { subdivideSelection(); } )->setEnabled( hasSel );
	m->addAction( tr( "Edge Slide…\tShift+V" ), this,
		[this]() { edgeSlide(); } )->setEnabled( hasSel );
}

void GLView::populateFaceMenu( QMenu * m )
{
	if ( !model )
		return;
	const bool hasSel = !pickedElems.isEmpty();
	m->addAction( tr( "Extrude Region…\tE" ), this,
		[this]() { extrudeRegion(); } )->setEnabled( hasSel );
	m->addAction( tr( "Inset Faces…\tI" ), this,
		[this]() { insetRegion(); } )->setEnabled( hasSel );
	m->addAction( tr( "Make Face / Fill / Bridge…\tF" ), this,
		[this]() { makeFace(); } )->setEnabled( hasSel );
	m->addSeparator();
	m->addAction( tr( "Tris to Quads\tAlt+J" ), this,
		[this]() { trisToQuads(); } )->setEnabled( hasSel );
	QMenu * mTri = m->addMenu( tr( "Triangulate Faces" ) );
	mTri->setEnabled( hasSel );
	mTri->addAction( tr( "Keep Diagonals\tCtrl+T" ), this,
		[this]() { triangulateSelection( 0 ); } );
	mTri->addAction( tr( "Beauty (max-min angle)" ), this,
		[this]() { triangulateSelection( 1 ); } );
	mTri->addAction( tr( "Shortest Diagonal" ), this,
		[this]() { triangulateSelection( 2 ); } );
	mTri->addAction( tr( "Longest Diagonal" ), this,
		[this]() { triangulateSelection( 3 ); } );
}

void GLView::populatePaintMenu( QMenu * m )
{
	if ( !model )
		return;
	const bool weightPaint = riggingWeightPaintModeActive();
	const bool segmentPaint = segmentPaintModeActive();
	const bool hasSel = !pickedElems.isEmpty();
	if ( weightPaint ) {
		m->addAction( tr( "Fill Selected Weight\tCtrl+X" ), this,
			[this]() { fillRiggingWeightSelection(); } )->setEnabled( hasSel );
		m->addSeparator();
	} else if ( segmentPaint ) {
		m->addAction( tr( "Apply Segment Brush to Selection\tCtrl+X" ), this,
			[this]() { fillSegmentPaintSelection(); } )->setEnabled( hasSel );
		m->addSeparator();
	}
	m->addAction( tr( "Hide Selection\tH" ), this,
		[this]() { hideSelectedElements(); } )->setEnabled( hasSel );
	m->addAction( tr( "Unhide All\tAlt+H" ), this, [this]() { unhideAllElements(); } );
}

static QVector<int> tlNiNodeParents( NifModel * nif, int child )
{
	QVector<int> parents;
	if ( !nif || child < 0 )
		return parents;
	for ( int b = 0; b < nif->getBlockCount(); b++ ) {
		QModelIndex iNode = nif->getBlockIndex( b );
		if ( !nif->blockInherits( iNode, "NiNode" ) )
			continue;
		if ( nif->getLinkArray( nif->getIndex( iNode, "Children" ) ).contains( child ) )
			parents.append( b );
	}
	return parents;
}

static bool tlSceneDescendantContains( NifModel * nif, int root, int wanted, QSet<int> & visited )
{
	if ( root == wanted )
		return true;
	if ( !nif || root < 0 || visited.contains( root ) )
		return false;
	visited.insert( root );
	QModelIndex iRoot = nif->getBlockIndex( root );
	if ( !nif->blockInherits( iRoot, "NiNode" ) )
		return false;
	for ( int child : nif->getLinkArray( nif->getIndex( iRoot, "Children" ) ) )
		if ( tlSceneDescendantContains( nif, child, wanted, visited ) )
			return true;
	return false;
}

void GLView::showParentMenu()
{
	if ( editMode )
		return;
	AutoCloseMenu menu;
	menu.addSection( tr( "Set Parent" ) );
	QAction * keepWorld = menu.addAction( tr( "Object (Keep Transform)" ) );
	QAction * keepLocal = menu.addAction( tr( "Object (Keep Local Transform)" ) );
	menu.addSeparator();
	QAction * link = menu.addAction( tr( "Link to Additional Parent" ) );
	link->setToolTip( tr( "Keep existing parents and add another NiNode link; the local transform is shared" ) );
	QAction * chosen = menu.exec( QCursor::pos() );
	if ( chosen == keepWorld ) parentSelection( 0 );
	else if ( chosen == keepLocal ) parentSelection( 1 );
	else if ( chosen == link ) parentSelection( 2 );
}

void GLView::parentSelection( int mode )
{
	if ( !model || editMode || objSelection.isEmpty() ) {
		emit gizmoStatus( tr( "Set Parent needs one or more selected scene objects" ) );
		return;
	}

	int target = -1;
	if ( objSelection.size() > 1 && objActive >= 0
		&& model->blockInherits( model->getBlockIndex( objActive ), "NiNode" ) )
		target = objActive;

	if ( target < 0 ) {
		QStringList labels;
		QVector<int> blocks;
		for ( int b = 0; b < model->getBlockCount(); b++ ) {
			QModelIndex iBlock = model->getBlockIndex( b );
			if ( !model->blockInherits( iBlock, "NiNode" ) )
				continue;
			QString name = model->resolveString( iBlock, "Name" );
			if ( name.isEmpty() ) name = tr( "Unnamed NiNode" );
			labels.append( tr( "%1  [block %2]" ).arg( name ).arg( b ) );
			blocks.append( b );
		}
		if ( blocks.isEmpty() ) {
			emit gizmoStatus( tr( "This NIF contains no NiNode that can be used as a parent" ) );
			return;
		}
		bool ok = false;
		QString selected = QInputDialog::getItem( nullptr, tr( "Set Parent" ),
			tr( "Parent NiNode:" ), labels, 0, false, &ok );
		if ( !ok ) return;
		target = blocks.value( labels.indexOf( selected ), -1 );
	}

	QSet<int> children = objSelection;
	children.remove( target );
	QVector<int> validChildren;
	int incompatible = 0, cycles = 0;
	for ( int child : children ) {
		QModelIndex iChild = model->getBlockIndex( child );
		if ( !model->blockInherits( iChild, "NiAVObject" ) ) {
			incompatible++;
			continue;
		}
		QSet<int> visited;
		if ( tlSceneDescendantContains( model, child, target, visited ) ) {
			cycles++;
			continue;
		}
		validChildren.append( child );
	}
	if ( validChildren.isEmpty() ) {
		emit gizmoStatus( cycles ? tr( "Set Parent refused: the requested hierarchy would create a cycle" )
			: tr( "Select a compatible child object as well as the parent NiNode" ) );
		return;
	}

	auto worldTransform = [this]( int block ) {
		QModelIndex iBlock = model->getBlockIndex( block );
		if ( Node * node = scene->getNode( model, iBlock ) )
			return node->worldTrans();
		Transform local( model, iBlock );
		int parent = model->getParent( block );
		if ( parent >= 0 )
			if ( Node * parentNode = scene->getNode( model, model->getBlockIndex( parent ) ) )
				return parentNode->worldTrans() * local;
		return local;
	};
	QHash<int, Transform> oldWorld;
	for ( int child : validChildren )
		oldWorld.insert( child, worldTransform( child ) );
	Transform parentWorld = worldTransform( target );

	nifSnapshotOp( model, mode == 2 ? tr( "Link to additional parent" ) : tr( "Set parent" ), [&]() {
		for ( int child : validChildren ) {
			QModelIndex iChild = model->getBlockIndex( child );
			if ( mode != 2 ) {
				for ( int oldParent : tlNiNodeParents( model, child ) ) {
					delLink( model, model->getBlockIndex( oldParent ), QStringLiteral( "Children" ), child );
					if ( model->blockInherits( iChild, "NiDynamicEffect" ) )
						delLink( model, model->getBlockIndex( oldParent ), QStringLiteral( "Effects" ), child );
				}
			}
			blockLink( model, model->getBlockIndex( target ), iChild );
			if ( mode == 0 && Transform::canConstruct( model, iChild ) )
				( parentWorld.inverted() * oldWorld.value( child ) ).writeBack( model, iChild );
		}
	} );

	objSelection.insert( target );
	objActive = target;
	scene->currentBlock = model->getBlockIndex( target );
	scene->currentIndex = QModelIndex( scene->currentBlock );
	emit objectSelectionChanged();
	modelChanged();
	QString message = mode == 2
		? tr( "Linked %1 object(s) to an additional parent" ).arg( validChildren.size() )
		: tr( "Parented %1 object(s) to %2" ).arg( validChildren.size() ).arg( model->resolveString( model->getBlockIndex( target ), "Name" ) );
	if ( incompatible ) message += tr( "; skipped %1 incompatible block(s)" ).arg( incompatible );
	if ( cycles ) message += tr( "; skipped %1 cycle(s)" ).arg( cycles );
	emit gizmoStatus( message );
}

void GLView::showClearParentMenu()
{
	if ( editMode )
		return;
	AutoCloseMenu menu;
	menu.addSection( tr( "Clear Parent" ) );
	QAction * clear = menu.addAction( tr( "Clear Parent" ) );
	QAction * keep = menu.addAction( tr( "Clear and Keep Transform" ) );
	QAction * inverse = menu.addAction( tr( "Clear Parent Inverse" ) );
	inverse->setEnabled( false );
	inverse->setToolTip( tr( "NIF scene objects do not store Blender-style parent-inverse matrices" ) );
	QAction * chosen = menu.exec( QCursor::pos() );
	if ( chosen == clear ) clearParentSelection( false );
	else if ( chosen == keep ) clearParentSelection( true );
}

void GLView::clearParentSelection( bool keepWorld )
{
	if ( !model || editMode || objSelection.isEmpty() ) {
		emit gizmoStatus( tr( "Clear Parent needs selected scene objects" ) );
		return;
	}
	QVector<int> children;
	QHash<int, Transform> oldWorld;
	for ( int child : objSelection ) {
		QModelIndex iChild = model->getBlockIndex( child );
		if ( !model->blockInherits( iChild, "NiAVObject" ) || tlNiNodeParents( model, child ).isEmpty() )
			continue;
		children.append( child );
		if ( keepWorld ) {
			if ( Node * node = scene->getNode( model, iChild ) ) oldWorld.insert( child, node->worldTrans() );
			else oldWorld.insert( child, Transform( model, iChild ) );
		}
	}
	if ( children.isEmpty() ) {
		emit gizmoStatus( tr( "The selected objects have no NiNode parent links" ) );
		return;
	}

	nifSnapshotOp( model, keepWorld ? tr( "Clear parent and keep transform" ) : tr( "Clear parent" ), [&]() {
		for ( int child : children ) {
			QModelIndex iChild = model->getBlockIndex( child );
			for ( int parent : tlNiNodeParents( model, child ) ) {
				delLink( model, model->getBlockIndex( parent ), QStringLiteral( "Children" ), child );
				if ( model->blockInherits( iChild, "NiDynamicEffect" ) )
					delLink( model, model->getBlockIndex( parent ), QStringLiteral( "Effects" ), child );
			}
			if ( keepWorld && oldWorld.contains( child ) && Transform::canConstruct( model, iChild ) )
				oldWorld.value( child ).writeBack( model, iChild );
		}
	} );
	modelChanged();
	emit gizmoStatus( keepWorld
		? tr( "Cleared %1 parent link(s) and preserved world transforms" ).arg( children.size() )
		: tr( "Cleared %1 parent link(s)" ).arg( children.size() ) );
}

void GLView::createFloatingDecal( float offset )
{
	if ( !model || pickedElems.isEmpty() || ( !editMode && !opReapplying ) ) {
		emit gizmoStatus( tr( "Floating Decal needs selected faces in edit mode" ) );
		return;
	}

	const QVector<PickedElement> seed = pickedElems;
	QHash<int, QSet<int>> selectedVerts = pickedVertexRefs();
	QHash<int, QSet<int>> explicitFaces;
	for ( const PickedElement & pe : pickedElems )
		if ( pe.type == 3 && pe.shapeBlock >= 0 )
			explicitFaces[pe.shapeBlock].insert( pe.e0 );

	// Resolve vertex/edge selections to complete triangles.  This matches the
	// duplicate tool, while face mode remains the natural decal workflow.
	QHash<int, QSet<int>> facesByShape;
	QSet<int> shapes;
	for ( int sb : selectedVerts.keys() )
		shapes.insert( sb );
	for ( int sb : explicitFaces.keys() )
		shapes.insert( sb );
	for ( int sb : shapes ) {
		QModelIndex iShape = model->getBlockIndex( sb );
		if ( !model->blockInherits( iShape, "BSTriShape" ) )
			continue;
		QModelIndex iTris = model->getIndex( iShape, "Triangles" );
		int nt = std::min( model->get<int>( iShape, "Num Triangles" ), model->rowCount( iTris ) );
		const QSet<int> & sv = selectedVerts.value( sb );
		for ( int t = 0; t < nt; t++ ) {
			Triangle tri = model->get<Triangle>( model->getIndex( iTris, t ) );
			if ( explicitFaces.value( sb ).contains( t )
				|| ( !sv.isEmpty() && sv.contains( tri[0] ) && sv.contains( tri[1] ) && sv.contains( tri[2] ) ) )
				facesByShape[sb].insert( t );
		}
	}
	if ( facesByShape.isEmpty() ) {
		emit gizmoStatus( tr( "Floating Decal needs at least one complete selected face" ) );
		return;
	}

	QVector<int> newBlocks;
	QVector<DecalPreviewVert> previewVerts;
	int totalFaces = 0;
	nifSnapshotOp( model, tr( "Create Floating Decal" ), [&]() {
		for ( auto it = facesByShape.constBegin(); it != facesByShape.constEnd(); ++it ) {
			const int sb = it.key();
			const QSet<int> chosen = it.value();
			if ( chosen.isEmpty() )
				continue;
			int nNew = tlCloneShapeWithProps( model, sb );
			if ( nNew < 0 )
				continue;
			QModelIndex iNew = model->getBlockIndex( nNew );
			int parentNum = model->getParent( sb );
			if ( parentNum >= 0 )
				blockLink( model, model->getBlockIndex( parentNum ), iNew );

			QString srcName = model->get<QString>( model->getBlockIndex( sb ), "Name" );
			model->set<QString>( iNew, "Name", tlUniqueNodeName( model, srcName + QStringLiteral( "_Decal" ) ) );
			int kept = tlKeepTriangles( model, iNew, [&chosen]( int t ) { return chosen.contains( t ); } );
			if ( kept <= 0 )
				continue;

			QModelIndex iVD = model->getIndex( iNew, "Vertex Data" );
			QModelIndex iTris = model->getIndex( iNew, "Triangles" );
			int nv = model->rowCount( iVD );
			QVector<Vector3> geometricNormals( nv );
			QSet<int> usedVerts;
			for ( int t = 0; t < kept && t < model->rowCount( iTris ); t++ ) {
				Triangle tri = model->get<Triangle>( model->getIndex( iTris, t ) );
				Vector3 a, b, c;
				if ( !tlGetVertexLocal( model, iNew, tri[0], a )
					|| !tlGetVertexLocal( model, iNew, tri[1], b )
					|| !tlGetVertexLocal( model, iNew, tri[2], c ) )
					continue;
				Vector3 fn = Vector3::crossproduct( b - a, c - a );
				if ( fn.length() > 1.0e-8f )
					fn.normalize();
				for ( int k = 0; k < 3; k++ ) {
					int vi = tri[k];
					if ( vi >= 0 && vi < nv ) {
						usedVerts.insert( vi );
						geometricNormals[vi] += fn;
					}
				}
			}

			for ( int vi : usedVerts ) {
				Vector3 normal;
				QModelIndex iVert = model->getIndex( iVD, vi );
				if ( model->getIndex( iVert, "Normal" ).isValid() )
					normal = model->get<Vector3>( iVert, "Normal" );
				if ( normal.length() <= 1.0e-8f )
					normal = geometricNormals.value( vi );
				if ( normal.length() <= 1.0e-8f )
					continue;
				normal.normalize();
				Vector3 pos;
				if ( tlGetVertexLocal( model, iNew, vi, pos ) ) {
					previewVerts.append( DecalPreviewVert{ nNew, vi, pos, normal } );
					tlSetVertexLocal( model, iNew, vi, pos + normal * offset );
				}
			}
			tlUpdateBounds( model, iNew );
			newBlocks.append( nNew );
			totalFaces += kept;
		}
	} );

	if ( newBlocks.isEmpty() ) {
		emit gizmoStatus( tr( "No floating decal geometry was created" ) );
		return;
	}

	setEditMode( false );
	pickedElems.clear();
	objSelection.clear();
	for ( int b : newBlocks )
		objSelection.insert( b );
	objActive = newBlocks.last();
	scene->currentBlock = model->getBlockIndex( objActive );
	scene->currentIndex = QModelIndex( scene->currentBlock );
	emit objectSelectionChanged();
	emit clicked( scene->currentBlock );
	modelChanged();

	if ( !opReapplying ) {
		lastOpKind = 3;
		lastOpParam = offset;
		lastOpSeed = seed;
		lastDecalVerts = previewVerts;
		lastOpUndoIndex = model->undoStack ? model->undoStack->index() : -1;
		emit operatorPanel( 3, offset );
	}
	emit gizmoStatus( tr( "Created %1 floating decal face(s) in %2 separate shape(s) - assign the new shape its decal material" )
		.arg( totalFaces ).arg( newBlocks.size() ) );
}

void GLView::showSetOriginMenu()
{
	if ( !model || ( editMode ? editShapeBlocks.isEmpty() : objSelection.isEmpty() ) ) {
		emit gizmoStatus( tr( "Set Origin needs a mesh selected" ) );
		return;
	}
	AutoCloseMenu m;
	m.addSection( tr( "Set Origin" ) );
	QAction * aGTO = m.addAction( tr( "Geometry to Origin" ) );
	QAction * aOTG = m.addAction( tr( "Origin to Geometry" ) );
	QAction * aOTC = m.addAction( tr( "Origin to 3D Cursor" ) );
	QAction * r = m.exec( QCursor::pos() );
	if ( r == aGTO )
		setOrigin( 0 );
	else if ( r == aOTG )
		setOrigin( 1 );
	else if ( r == aOTC )
		setOrigin( 2 );
}

void GLView::setOrigin( int mode )
{
	// operate on the edited meshes in edit mode, the object selection otherwise
	const QSet<int> targets = editMode ? editShapeBlocks : objSelection;
	if ( !model || targets.isEmpty() )
		return;

	int done = 0;
	nifSnapshotOp( model, tr( "Set origin" ), [&]() {
		for ( int sb : targets ) {
			QModelIndex iBlock = model->getBlockIndex( sb );
			if ( !model->blockInherits( iBlock, "NiAVObject" )
				|| !model->getIndex( iBlock, "Translation" ).isValid() )
				continue;

			bool isShape = model->blockInherits( iBlock, "BSTriShape" );
			Node * node = scene->getNode( model, iBlock );
			Transform nw = node ? node->worldTrans() : Transform();
			float nws = ( nw.scale != 0.0f ) ? nw.scale : 1.0f;

			// geometry points in THIS node's local space (a plain NiNode
			// aggregates the vertices of its descendant shapes)
			QVector<Vector3> pts;
			if ( isShape ) {
				Shape * s = shapeForBlock( sb );
				if ( s )
					pts = s->verts;
			} else {
				Matrix nwrInv = nw.rotation.inverted();
				for ( Shape * s : scene->shapes ) {
					if ( !s || s->verts.isEmpty() )
						continue;
					int p = s->id();
					while ( p >= 0 && p != sb )
						p = model->getParent( p );
					if ( p != sb )
						continue;
					Transform swt = shapeRenderTrans( s );
					for ( const Vector3 & v : s->verts )
						pts.append( nwrInv * ( ( swt * v - nw.translation ) * ( 1.0f / nws ) ) );
				}
			}

			// centre C (node-local) that becomes the new origin
			Vector3 C;
			if ( mode == 2 ) {
				C = nw.rotation.inverted() * ( ( cursorPos - nw.translation ) * ( 1.0f / nws ) );
			} else {
				if ( pts.isEmpty() )
					continue;	// nothing to centre on
				Vector3 mn = pts.first(), mx = pts.first();
				for ( const Vector3 & v : pts )
					for ( int k = 0; k < 3; k++ ) {
						mn[k] = std::min( mn[k], v[k] );
						mx[k] = std::max( mx[k], v[k] );
					}
				C = ( mn + mx ) * 0.5f;
			}

			// shift the contents so C sits at the local origin: vertices for a
			// shape, the direct child translations for a plain NiNode
			if ( isShape ) {
				Shape * s = shapeForBlock( sb );
				int nv = s ? s->verts.size() : 0;
				for ( int i = 0; i < nv; i++ )
					tlSetVertexLocal( model, iBlock, i, s->verts[i] - C );
				tlUpdateBounds( model, iBlock );
			} else {
				QModelIndex iChildren = model->getIndex( iBlock, "Children" );
				for ( int cr = 0; cr < model->rowCount( iChildren ); cr++ ) {
					int cb = model->getLink( model->getIndex( iChildren, cr ) );
					if ( cb < 0 )
						continue;
					QModelIndex ic = model->getBlockIndex( cb );
					if ( !model->getIndex( ic, "Translation" ).isValid() )
						continue;
					model->set<Vector3>( ic, "Translation", model->get<Vector3>( ic, "Translation" ) - C );
				}
			}

			// Origin-to-X also moves the node so its contents stay put in world
			if ( mode != 0 ) {
				Vector3 Tl = model->get<Vector3>( iBlock, "Translation" );
				Matrix Rl = model->get<Matrix>( iBlock, "Rotation" );
				float Sl = model->get<float>( iBlock, "Scale" );
				model->set<Vector3>( iBlock, "Translation", Tl + Rl * ( C * Sl ) );
			}

			done++;
		}
	} );

	if ( done == 0 )
		emit gizmoStatus( tr( "Set Origin: nothing applicable selected" ) );
	else
		emit gizmoStatus( tr( "Set origin (%1 node%2)" ).arg( done ).arg( done == 1 ? "" : "s" ) );
	modelChanged();
}

bool GLView::isEditableMesh( const QModelIndex & iBlock ) const
{
	if ( !model || !iBlock.isValid() )
		return false;
	if ( model->blockInherits( iBlock, "NiParticleSystem" ) )
		return false;	// particle geometry is generated, not hand-editable
	return model->blockInherits( iBlock, "BSTriShape" )
	       || model->blockInherits( iBlock, "NiTriBasedGeom" )
	       || model->blockInherits( iBlock, "BSGeometry" );
}

void GLView::setEditMode( bool on )
{
	if ( on == editMode )
		return;

	if ( knifeActive )
		cancelKnife();

	if ( on ) {
		// walk up to the nearest editable mesh of the current selection
		int b = model ? model->getBlockNumber( QModelIndex( scene->currentBlock ) ) : -1;
		while ( b >= 0 && !isEditableMesh( model->getBlockIndex( b ) ) )
			b = model->getParent( b );
		if ( b < 0 ) {
			emit gizmoStatus( tr( "Edit Mode needs a mesh selected (BSTriShape / NiTriShape)" ) );
			return;
		}
		editMode = true;
		// Remember the global render preference, then let Edit Mode's explicit
		// cage preference decide whether it exposes evaluated or raw positions.
		// Weight Paint forces evaluated skinning immediately after entering.
		editSkinningWasEnabled = scene->hasOption( Scene::DoSkinning );
		scene->options.setFlag( Scene::DoSkinning, editDeformedCage );
		// edit every selected mesh (object-mode multi-selection), plus the one
		// the current block resolves to
		editShapeBlocks.clear();
		for ( int sb : objSelection ) {
			if ( isEditableMesh( model->getBlockIndex( sb ) ) )
				editShapeBlocks.insert( sb );
		}
		editShapeBlocks.insert( b );
		editShapeBlock = b;
		for ( int wb : editShapeBlocks )
			if ( Shape * shape = shapeForBlock( wb ) )
				shape->updateBoneTransforms();
		pickMode = 1;	// start in vertex select, like Blender
		// restore this session's remembered selection for these meshes,
		// re-deriving world positions from the current geometry (the mesh
		// or its transform may have changed since the selection was saved)
		pickedElems.clear();
		int typeBits = 0;
		for ( int wb : editShapeBlocks ) {
			auto it = savedElemSelections.constFind( wb );
			if ( it == savedElemSelections.constEnd() )
				continue;
			Shape * sp = shapeForBlock( wb );
			if ( !sp )
				continue;
			Transform wt = shapeRenderTrans( sp );
			int nv = sp->verts.size();
			for ( PickedElement pe : *it ) {
				if ( pe.type == 1 ) {
					if ( pe.e0 < 0 || pe.e0 >= nv )
						continue;
					pe.worldPos = wt * editVertexLocal( sp, pe.e0 );
					pe.wA = pe.worldPos;
				} else if ( pe.type == 2 ) {
					if ( pe.e0 < 0 || pe.e0 >= nv || pe.e1 < 0 || pe.e1 >= nv )
						continue;
					pe.wA = wt * editVertexLocal( sp, pe.e0 );
					pe.wB = wt * editVertexLocal( sp, pe.e1 );
					pe.wC = pe.wA;
					pe.worldPos = ( pe.wA + pe.wB ) / 2.0f;
				} else if ( pe.type == 3 ) {
					if ( pe.e0 < 0 || pe.e0 >= sp->triangles.size() )
						continue;
					const Triangle & tri = sp->triangles.at( pe.e0 );
					if ( tri[0] >= nv || tri[1] >= nv || tri[2] >= nv )
						continue;
					pe.wA = wt * editVertexLocal( sp, tri[0] );
					pe.wB = wt * editVertexLocal( sp, tri[1] );
					pe.wC = wt * editVertexLocal( sp, tri[2] );
					pe.worldPos = ( pe.wA + pe.wB + pe.wC ) * ( 1.0f / 3.0f );
				} else {
					continue;
				}
				typeBits |= ( pe.type == 3 ) ? 4 : pe.type;
				pickedElems.append( pe );
			}
		}
		if ( typeBits )
			pickMode = typeBits;	// re-enable the modes the restored elements need
		scene->editMode = true;
		scene->restPoseBlock = b;
		scene->hiddenTris = editHiddenTris;	// hidden elements apply in edit mode only
		emit gizmoStatus( tr( "Edit Mode (%1 mesh%2): 1/2/3 = vertex/edge/face, G/R/S, X delete, Shift+S snap, Tab exits" )
			.arg( editShapeBlocks.size() ).arg( editShapeBlocks.size() == 1 ? "" : "es" ) );
	} else {
		// remember the selection per mesh so re-entering edit mode on the
		// same object restores it (an empty save means "user deselected all")
		for ( int wb : editShapeBlocks ) {
			QVector<PickedElement> kept;
			for ( const PickedElement & pe : pickedElems )
				if ( pe.shapeBlock == wb )
					kept.append( pe );
			savedElemSelections.insert( wb, kept );
		}
		editMode = false;
		scene->options.setFlag( Scene::DoSkinning, editSkinningWasEnabled );
		editShapeBlock = -1;
		editShapeBlocks.clear();
		pickMode = 0;
		pickedElems.clear();
		if ( elemTransform )
			gizmoEndElement( false );
		scene->editMode = false;
		scene->restPoseBlock = -1;
		scene->hiddenTris.clear();	// object mode always shows the full mesh
		emit gizmoStatus( QString() );
	}

	emit editModeChanged( editMode );
	emit pickModeChanged( pickMode );
	doCompile = 1;	// rebuild so the rest-pose toggle takes effect
	update();
}

void GLView::objectSelectClick( int avBlock, bool shift )
{
	if ( editMode )
		return;
	recordSelection();
	if ( avBlock < 0 ) {
		if ( !shift ) {
			objSelection.clear();
			objActive = -1;
			emit objectSelectionChanged();
			update();
		}
		return;
	}

	if ( shift ) {
		// toggle; the toggled block becomes active if now selected
		if ( objSelection.contains( avBlock ) ) {
			objSelection.remove( avBlock );
			if ( objActive == avBlock )
				objActive = objSelection.isEmpty() ? -1 : *objSelection.constBegin();
		} else {
			objSelection.insert( avBlock );
			objActive = avBlock;
		}
	} else {
		objSelection.clear();
		objSelection.insert( avBlock );
		objActive = avBlock;
	}
	emit objectSelectionChanged();
	update();
}

void GLView::syncObjectSelection( int avBlock )
{
	// a plain single selection from the tree/list replaces the multi-selection.
	// If the block is already the active one (e.g. this is the echo of a viewport
	// multi-select click), keep the existing multi-selection intact.
	if ( editMode )
		return;
	// already part of the multi-selection: just make it active, keep the set
	if ( avBlock >= 0 && objSelection.contains( avBlock ) ) {
		if ( objActive != avBlock ) {
			objActive = avBlock;
			emit objectSelectionChanged();
			update();
		}
		return;
	}
	objSelection.clear();
	if ( avBlock >= 0 ) {
		objSelection.insert( avBlock );
		objActive = avBlock;
	} else {
		objActive = -1;
	}
	emit objectSelectionChanged();
	update();
}

void GLView::setObjectSelection( const QSet<int> & sel, int active )
{
	if ( editMode )
		return;
	objSelection = sel;
	objActive = ( active >= 0 && sel.contains( active ) ) ? active
	            : ( sel.isEmpty() ? -1 : *sel.constBegin() );
	emit objectSelectionChanged();
	update();
}

// ---- 2D triangle / rectangle overlap, for geometry-accurate box select ----
static bool tlSegSegHit( const QPointF & a, const QPointF & b, const QPointF & c, const QPointF & d )
{
	auto cross = []( const QPointF & p, const QPointF & q ) { return p.x() * q.y() - p.y() * q.x(); };
	QPointF r = b - a, s = d - c;
	double rxs = cross( r, s );
	if ( std::fabs( rxs ) < 1.0e-9 )
		return false;
	double t = cross( c - a, s ) / rxs;
	double u = cross( c - a, r ) / rxs;
	return t >= 0.0 && t <= 1.0 && u >= 0.0 && u <= 1.0;
}
static bool tlPointInTri2D( const QPointF & p, const QPointF & a, const QPointF & b, const QPointF & c )
{
	auto sign = []( const QPointF & p1, const QPointF & p2, const QPointF & p3 ) {
		return ( p1.x() - p3.x() ) * ( p2.y() - p3.y() ) - ( p2.x() - p3.x() ) * ( p1.y() - p3.y() );
	};
	double d1 = sign( p, a, b ), d2 = sign( p, b, c ), d3 = sign( p, c, a );
	bool neg = ( d1 < 0.0 ) || ( d2 < 0.0 ) || ( d3 < 0.0 );
	bool pos = ( d1 > 0.0 ) || ( d2 > 0.0 ) || ( d3 > 0.0 );
	return !( neg && pos );
}
//! Does the screen-space triangle (a,b,c) overlap the rectangle at all? Covers a
//! triangle vertex inside the box, the box sitting inside the triangle, and edge
//! crossings - so box select catches faces even when no vertex is in the box.
static bool tlBoxTriOverlap( const QRectF & r, const QPointF & a, const QPointF & b, const QPointF & c )
{
	if ( r.contains( a ) || r.contains( b ) || r.contains( c ) )
		return true;
	QPointF rc[4] = { r.topLeft(), r.topRight(), r.bottomRight(), r.bottomLeft() };
	if ( tlPointInTri2D( rc[0], a, b, c ) )
		return true;
	QPointF tv[3] = { a, b, c };
	for ( int i = 0; i < 3; i++ )
		for ( int j = 0; j < 4; j++ )
			if ( tlSegSegHit( tv[i], tv[( i + 1 ) % 3], rc[j], rc[( j + 1 ) % 4] ) )
				return true;
	return false;
}

void GLView::beginBoxSelect()
{
	if ( !model )
		return;
	if ( riggingWeightPaintMode )
		setRiggingWeightPaintBrushEnabled( false );
	if ( vertexPaintMode )
		setVertexPaintBrushEnabled( false );
	if ( segmentPaintMode )
		setSegmentPaintBrushEnabled( false );
	boxSelecting = true;
	boxSelectDrag = false;
	boxSelectPrevActive = objActive;	// restored later if it survives the box
	setCursor( Qt::CrossCursor );
	emit gizmoStatus( tr( "Box select: drag a rectangle (Shift adds, Ctrl removes)" ) );
	update();
}

void GLView::applyBoxSelect( const QRect & rect, Qt::KeyboardModifiers mods )
{
	if ( !model || !scene )
		return;
	recordSelection();
	// plain drag adds, Shift- or Ctrl-drag deselects what's inside the box
	bool sub = mods & ( Qt::ShiftModifier | Qt::ControlModifier );
	bool xray = scene->xRay;
	QRect r = rect.normalized();

	// view direction into the scene, for back-face rejection when not x-raying
	Vector3 rayO, rayD;
	mouseRayWorld( QPointF( r.center() ), rayO, rayD );

	if ( !editMode ) {
		// object mode: a shape is picked if any of its geometry projects into the
		// box. Testing the node origin (Blender's rule) is wrong for NIF shapes:
		// skinned/attached meshes routinely sit at the skeleton root (0,0,0) far
		// from their visible verts, so an origin test could never catch them.
		QSet<int> hit;
		QRectF rF( r );
		for ( Shape * s : scene->shapes ) {
			if ( !s || s->isHidden() )
				continue;
			Transform wt = shapeRenderTrans( s );
			int nv = s->verts.size();
			QVector<QPointF> sp( nv );
			QVector<bool> ok( nv, false );
			for ( int i = 0; i < nv; i++ ) {
				Vector3 local = scene->hasOption( Scene::DoSkinning )
					? s->skinVertex( i, s->verts.at( i ) ) : s->verts.at( i );
				ok[i] = worldToScreen( wt * local, sp[i] );
			}
			bool inside = false;
			// a vertex inside the box (fast path)
			for ( int i = 0; i < nv && !inside; i++ )
				if ( ok[i] && rF.contains( sp[i] ) )
					inside = true;
			// else the box may straddle an edge or sit inside a face: test the
			// actual triangles so any geometry under the box is caught
			if ( !inside ) {
				for ( const Triangle & t : s->triangles ) {
					if ( t[0] >= nv || t[1] >= nv || t[2] >= nv || !ok[t[0]] || !ok[t[1]] || !ok[t[2]] )
						continue;
					if ( tlBoxTriOverlap( rF, sp[t[0]], sp[t[1]], sp[t[2]] ) ) {
						inside = true;
						break;
					}
				}
			}
			// vertexless shapes: fall back to the node origin
			if ( !inside && nv == 0 ) {
				QPointF o;
				inside = worldToScreen( wt * Vector3(), o ) && rF.contains( o );
			}
			if ( inside )
				hit.insert( s->id() );
		}
		// additive by default: the box only ever grows the selection,
		// Shift/Ctrl-drag deselects; A / click-empty clears
		QSet<int> sel = sub ? ( objSelection - hit ) : ( objSelection | hit );
		// box select sets no new primary; keep the pre-box primary if it survived
		objSelection = sel;
		objActive = ( boxSelectPrevActive >= 0 && sel.contains( boxSelectPrevActive ) )
		            ? boxSelectPrevActive : -1;
		emit objectSelectionChanged();
		emit gizmoStatus( tr( "Box selected %1 object(s)" ).arg( sel.size() ) );
		if ( !boxReapplying ) {
			lastBoxRect = r;
			lastGestureKind = 1;
			emit boxSelectApplied();
		}
		update();
		return;
	}

	// edit mode: collect verts / edges / faces inside the box per pick mode
	QVector<PickedElement> box;
	for ( Shape * s : scene->shapes ) {
		if ( !s || s->isHidden() || !editShapeBlocks.contains( s->id() ) )
			continue;
		Transform wt = shapeRenderTrans( s );
		int nv = s->verts.size();
		QVector<Vector3> wv( nv );
		QVector<bool> inBox( nv, false );
		for ( int i = 0; i < nv; i++ ) {
			wv[i] = wt * editVertexLocal( s, i );
			QPointF p;
			if ( worldToScreen( wv[i], p ) )
				inBox[i] = r.contains( p.toPoint() );
		}
		// front-facing test (x-ray off = only what you can see): a triangle is
		// visible if its geometric normal faces the camera; a vertex/edge is
		// visible if it belongs to such a triangle
		QVector<bool> tFront( s->triangles.size(), true );
		QVector<bool> vFront;
		if ( !xray ) {
			vFront.fill( false, nv );
			for ( int t = 0; t < s->triangles.size(); t++ ) {
				const Triangle & tri = s->triangles.at( t );
				if ( tri[0] >= nv || tri[1] >= nv || tri[2] >= nv )
					continue;
				Vector3 n = Vector3::crossproduct( wv[tri[1]] - wv[tri[0]], wv[tri[2]] - wv[tri[0]] );
				bool front = Vector3::dotproduct( n, rayD ) < 0.0f;
				tFront[t] = front;
				if ( front ) {
					vFront[tri[0]] = true; vFront[tri[1]] = true; vFront[tri[2]] = true;
				}
			}
		}
		auto vVis = [&]( int i ) { return xray || ( i < vFront.size() && vFront[i] ); };

		if ( pickMode & 1 ) {
			for ( int i = 0; i < nv; i++ ) {
				if ( inBox[i] && vVis( i ) ) {
					PickedElement pe;
					pe.shapeBlock = s->id(); pe.type = 1; pe.e0 = i; pe.e1 = -1;
					pe.worldPos = wv[i]; pe.wA = pe.wB = pe.wC = wv[i];
					box.append( pe );
				}
			}
		}
		if ( pickMode & 2 ) {
			QSet<qint64> seen;
			for ( int t = 0; t < s->triangles.size(); t++ ) {
				const Triangle & tri = s->triangles.at( t );
				for ( int e = 0; e < 3; e++ ) {
					int a = tri[e], b = tri[( e + 1 ) % 3];
					if ( a >= nv || b >= nv )
						continue;
					int lo = std::min( a, b ), hi = std::max( a, b );
					qint64 key = ( qint64( lo ) << 32 ) | quint32( hi );
					if ( seen.contains( key ) )
						continue;
					seen.insert( key );
					if ( inBox[lo] && inBox[hi] && vVis( lo ) && vVis( hi ) ) {
						PickedElement pe;
						pe.shapeBlock = s->id(); pe.type = 2; pe.e0 = lo; pe.e1 = hi;
						pe.wA = wv[lo]; pe.wB = wv[hi]; pe.wC = pe.wA;
						pe.worldPos = ( wv[lo] + wv[hi] ) * 0.5f;
						box.append( pe );
					}
				}
			}
		}
		if ( pickMode & 4 ) {
			for ( int t = 0; t < s->triangles.size(); t++ ) {
				const Triangle & tri = s->triangles.at( t );
				if ( tri[0] >= nv || tri[1] >= nv || tri[2] >= nv )
					continue;
				if ( !xray && !tFront[t] )
					continue;
				Vector3 ctr = ( wv[tri[0]] + wv[tri[1]] + wv[tri[2]] ) / 3.0f;
				QPointF cp;
				if ( worldToScreen( ctr, cp ) && r.contains( cp.toPoint() ) ) {
					PickedElement pe;
					pe.shapeBlock = s->id(); pe.type = 3; pe.e0 = t; pe.e1 = -1;
					pe.wA = wv[tri[0]]; pe.wB = wv[tri[1]]; pe.wC = wv[tri[2]];
					pe.worldPos = ctr;
					Vector3 n = Vector3::crossproduct( pe.wB - pe.wA, pe.wC - pe.wA );
					n.normalize();
					pe.worldNormal = n;
					box.append( pe );
				}
			}
		}
	}

	// a marked quad selects / deselects as one face
	expandQuadPartners( box );

	// additive by default: plain drag adds, Shift/Ctrl-drag deselects
	for ( const PickedElement & pe : box ) {
		int at = pickedElems.indexOf( pe );
		if ( sub ) {
			if ( at >= 0 )
				pickedElems.remove( at );
		} else if ( at < 0 ) {
			pickedElems.append( pe );
		}
	}
	emit gizmoStatus( tr( "Box select: %1 element(s) selected" ).arg( pickedElems.size() ) );
	if ( !boxReapplying ) {
		lastBoxRect = r;
		lastGestureKind = 1;
		emit boxSelectApplied();
	}
	update();
}

void GLView::deselectLastGesture()
{
	// re-run the last box / circle stroke subtractively; only meaningful right
	// after the gesture (its coordinates are in screen space, so a camera move
	// retargets it)
	if ( lastGestureKind == 1 ) {
		if ( !lastBoxRect.isValid() || lastBoxRect.width() < 2 || lastBoxRect.height() < 2 )
			return;
		boxReapplying = true;
		applyBoxSelect( lastBoxRect, Qt::ControlModifier );
		boxReapplying = false;
	} else if ( lastGestureKind == 2 && !lastCircleStroke.isEmpty() ) {
		recordSelection();
		float saved = circleSelectRadius;
		circleSelectRadius = lastCircleStrokeRad;
		for ( const QPointF & p : lastCircleStroke )
			applyCircleSelect( p, true );
		circleSelectRadius = saved;
	}
}

void GLView::selectAll( int action )
{
	// action: 0 = toggle like Blender's A key (all if nothing picked, else
	// deselect), 1 = select all, 2 = deselect all
	if ( !model )
		return;
	if ( riggingWeightPaintMode )
		setRiggingWeightPaintBrushEnabled( false );
	if ( vertexPaintMode )
		setVertexPaintBrushEnabled( false );
	if ( segmentPaintMode )
		setSegmentPaintBrushEnabled( false );
	recordSelection();
	if ( editMode ) {
		bool clearOnly = ( action == 2 ) || ( action == 0 && !pickedElems.isEmpty() );
		pickedElems.clear();
		if ( !clearOnly ) {
			for ( int wb : editShapeBlocks ) {
				Shape * sp = shapeForBlock( wb );
				if ( !sp )
					continue;
				Transform wt = shapeRenderTrans( sp );
				int nv = sp->verts.size();
				if ( pickMode & 4 ) {
					for ( int t = 0; t < sp->triangles.size(); t++ ) {
						const Triangle & tri = sp->triangles.at( t );
						if ( tri[0] >= nv || tri[1] >= nv || tri[2] >= nv )
							continue;
						PickedElement pe;
						pe.shapeBlock = wb;
						pe.type = 3;
						pe.e0 = t;
						pe.wA = wt * editVertexLocal( sp, tri[0] );
						pe.wB = wt * editVertexLocal( sp, tri[1] );
						pe.wC = wt * editVertexLocal( sp, tri[2] );
						pe.worldPos = ( pe.wA + pe.wB + pe.wC ) * ( 1.0f / 3.0f );
						pickedElems.append( pe );
					}
				} else {
					for ( int vi = 0; vi < nv; vi++ ) {
						PickedElement pe;
						pe.shapeBlock = wb;
						pe.type = 1;
						pe.e0 = vi;
						pe.worldPos = wt * editVertexLocal( sp, vi );
						pe.wA = pe.worldPos;
						pickedElems.append( pe );
					}
				}
			}
		}
		update();
	} else {
		bool clearOnly = ( action == 2 ) || ( action == 0 && !objSelection.isEmpty() );
		objSelection.clear();
		objActive = -1;
		if ( !clearOnly ) {
			for ( Shape * sp : scene->shapes ) {
				if ( sp && !sp->isHidden() ) {
					objSelection.insert( sp->id() );
					objActive = sp->id();
				}
			}
		}
		emit objectSelectionChanged();
		update();
	}
}

void GLView::selectMoreLess( bool more )
{
	// Blender Ctrl+= / Ctrl+-: grow the selection one adjacency ring, or drop
	// its boundary elements
	if ( !editMode || !model || pickedElems.isEmpty() )
		return;
	if ( riggingWeightPaintMode )
		setRiggingWeightPaintBrushEnabled( false );
	if ( vertexPaintMode )
		setVertexPaintBrushEnabled( false );
	if ( segmentPaintMode )
		setSegmentPaintBrushEnabled( false );
	recordSelection();

	auto ekey = []( int a, int b ) {
		return ( qint64( std::min( a, b ) ) << 32 ) | quint32( std::max( a, b ) );
	};

	QVector<PickedElement> result;
	for ( int wb : editShapeBlocks ) {
		Shape * sp = shapeForBlock( wb );
		if ( !sp )
			continue;
		Transform wt = shapeRenderTrans( sp );
		int nv = sp->verts.size();

		QSet<int> selV, selF;
		QSet<qint64> selE;
		for ( const PickedElement & pe : pickedElems ) {
			if ( pe.shapeBlock != wb )
				continue;
			if ( pe.type == 1 )
				selV.insert( pe.e0 );
			else if ( pe.type == 2 )
				selE.insert( ekey( pe.e0, pe.e1 ) );
			else if ( pe.type == 3 )
				selF.insert( pe.e0 );
		}
		if ( selV.isEmpty() && selE.isEmpty() && selF.isEmpty() )
			continue;

		// adjacency from the triangle list
		QHash<int, QSet<int>> vAdj;
		QHash<qint64, QVector<int>> eTris;
		for ( int t = 0; t < sp->triangles.size(); t++ ) {
			const Triangle & tri = sp->triangles.at( t );
			if ( tri[0] >= nv || tri[1] >= nv || tri[2] >= nv )
				continue;
			for ( int e = 0; e < 3; e++ ) {
				int a = tri[e], b = tri[( e + 1 ) % 3];
				vAdj[a].insert( b );
				vAdj[b].insert( a );
				eTris[ ekey( a, b ) ].append( t );
			}
		}

		QSet<int> newV = selV, newF = selF;
		QSet<qint64> newE = selE;

		if ( more ) {
			for ( int v : selV )
				for ( int n : vAdj.value( v ) )
					newV.insert( n );
			// edges grow across shared endpoints
			QSet<int> evs;
			for ( qint64 k : selE ) {
				evs.insert( int( k >> 32 ) );
				evs.insert( int( k & 0xffffffff ) );
			}
			for ( auto it = eTris.constBegin(); it != eTris.constEnd(); ++it ) {
				int a = int( it.key() >> 32 ), b = int( it.key() & 0xffffffff );
				if ( evs.contains( a ) || evs.contains( b ) )
					newE.insert( it.key() );
			}
			// faces grow across shared edges
			for ( int t : selF ) {
				const Triangle & tri = sp->triangles.at( t );
				for ( int e = 0; e < 3; e++ )
					for ( int n : eTris.value( ekey( tri[e], tri[( e + 1 ) % 3] ) ) )
						newF.insert( n );
			}
		} else {
			// keep only elements whose whole neighborhood is selected
			newV.clear();
			newE.clear();
			newF.clear();
			for ( int v : selV ) {
				bool inner = true;
				for ( int n : vAdj.value( v ) ) {
					if ( !selV.contains( n ) ) {
						inner = false;
						break;
					}
				}
				if ( inner )
					newV.insert( v );
			}
			for ( qint64 k : selE ) {
				int ends[2] = { int( k >> 32 ), int( k & 0xffffffff ) };
				bool inner = true;
				for ( int end : ends ) {
					for ( int n : vAdj.value( end ) ) {
						if ( !selE.contains( ekey( end, n ) ) ) {
							inner = false;
							break;
						}
					}
					if ( !inner )
						break;
				}
				if ( inner )
					newE.insert( k );
			}
			for ( int t : selF ) {
				const Triangle & tri = sp->triangles.at( t );
				bool inner = true;
				for ( int e = 0; e < 3 && inner; e++ ) {
					for ( int n : eTris.value( ekey( tri[e], tri[( e + 1 ) % 3] ) ) ) {
						if ( n != t && !selF.contains( n ) ) {
							inner = false;
							break;
						}
					}
				}
				if ( inner )
					newF.insert( t );
			}
		}

		// rebuild the picks with fresh world coordinates
		for ( int v : newV ) {
			if ( v < 0 || v >= nv )
				continue;
			PickedElement pe;
			pe.shapeBlock = wb;
			pe.type = 1;
			pe.e0 = v;
			pe.e1 = -1;
			pe.worldPos = wt * editVertexLocal( sp, v );
			pe.wA = pe.wB = pe.wC = pe.worldPos;
			result.append( pe );
		}
		for ( qint64 k : newE ) {
			int a = int( k >> 32 ), b = int( k & 0xffffffff );
			if ( a < 0 || a >= nv || b < 0 || b >= nv )
				continue;
			PickedElement pe;
			pe.shapeBlock = wb;
			pe.type = 2;
			pe.e0 = a;
			pe.e1 = b;
			pe.wA = wt * editVertexLocal( sp, a );
			pe.wB = wt * editVertexLocal( sp, b );
			pe.wC = pe.wA;
			pe.worldPos = ( pe.wA + pe.wB ) / 2.0f;
			result.append( pe );
		}
		for ( int t : newF ) {
			if ( t < 0 || t >= sp->triangles.size() )
				continue;
			const Triangle & tri = sp->triangles.at( t );
			if ( tri[0] >= nv || tri[1] >= nv || tri[2] >= nv )
				continue;
			PickedElement pe;
			pe.shapeBlock = wb;
			pe.type = 3;
			pe.e0 = t;
			pe.e1 = -1;
			pe.wA = wt * editVertexLocal( sp, tri[0] );
			pe.wB = wt * editVertexLocal( sp, tri[1] );
			pe.wC = wt * editVertexLocal( sp, tri[2] );
			pe.worldPos = ( pe.wA + pe.wB + pe.wC ) * ( 1.0f / 3.0f );
			Vector3 n = Vector3::crossproduct( pe.wB - pe.wA, pe.wC - pe.wA );
			n.normalize();
			pe.worldNormal = n;
			result.append( pe );
		}
	}

	pickedElems = result;
	emit gizmoStatus( ( more ? tr( "Select More: %1 element(s)" ) : tr( "Select Less: %1 element(s)" ) )
		.arg( pickedElems.size() ) );
	update();
}

bool GLView::selectEdgeLoop( const QPointF & pos, bool extend )
{
	if ( !editMode || !model )
		return false;

	// need an edge under the cursor whatever the current pick mode is
	int savedMode = pickMode;
	pickMode = 2;
	PickedElement hit;
	bool ok = pickElementUnder( pos, hit );
	pickMode = savedMode;
	if ( !ok || hit.type != 2 )
		return false;

	Shape * sp = shapeForBlock( hit.shapeBlock );
	if ( !sp )
		return false;
	int nv = sp->verts.size();

	auto ekey = []( int a, int b ) {
		return ( qint64( std::min( a, b ) ) << 32 ) | quint32( std::max( a, b ) );
	};

	QHash<qint64, QVector<int>> eTris;
	QHash<int, QVector<QPair<int, qint64>>> vEdges;	// vert -> (other end, edge key)
	for ( int t = 0; t < sp->triangles.size(); t++ ) {
		const Triangle & tri = sp->triangles.at( t );
		if ( tri[0] >= nv || tri[1] >= nv || tri[2] >= nv )
			continue;
		for ( int e = 0; e < 3; e++ ) {
			int a = tri[e], b = tri[( e + 1 ) % 3];
			qint64 k = ekey( a, b );
			if ( !eTris.contains( k ) ) {
				vEdges[a].append( qMakePair( b, k ) );
				vEdges[b].append( qMakePair( a, k ) );
			}
			eTris[k].append( t );
		}
	}

	auto edgeDir = [this, sp]( int from, int to ) {
		Vector3 d = editVertexLocal( sp, to ) - editVertexLocal( sp, from );
		float l = d.length();
		if ( l > 1.0e-8f )
			d /= l;
		return d;
	};

	// walk from both ends of the clicked edge. A triangulated mesh has no true
	// quad loops, so continue with the most collinear edge that shares no
	// triangle with the current one (boundary edges walk the boundary instead)
	QSet<qint64> loop;
	qint64 startKey = ekey( hit.e0, hit.e1 );
	loop.insert( startKey );
	for ( int dir = 0; dir < 2; dir++ ) {
		int prev = ( dir == 0 ) ? hit.e0 : hit.e1;
		int cur = ( dir == 0 ) ? hit.e1 : hit.e0;
		qint64 curKey = startKey;
		for ( int guard = 0; guard < 100000; guard++ ) {
			bool curBoundary = eTris.value( curKey ).size() < 2;
			Vector3 inDir = edgeDir( prev, cur );
			int bestNext = -1;
			qint64 bestKey = 0;
			float bestDot = 0.5f;	// require the turn to stay under ~60 degrees
			for ( const auto & cand : vEdges.value( cur ) ) {
				if ( cand.second == curKey || loop.contains( cand.second ) )
					continue;
				bool candBoundary = eTris.value( cand.second ).size() < 2;
				if ( curBoundary ) {
					if ( !candBoundary )
						continue;	// a boundary loop stays on the boundary
				} else {
					bool shares = false;
					for ( int t : eTris.value( curKey ) ) {
						if ( eTris.value( cand.second ).contains( t ) ) {
							shares = true;
							break;
						}
					}
					if ( shares )
						continue;
				}
				float dot = Vector3::dotproduct( inDir, edgeDir( cur, cand.first ) );
				if ( dot > bestDot ) {
					bestDot = dot;
					bestNext = cand.first;
					bestKey = cand.second;
				}
			}
			if ( bestNext < 0 )
				break;
			loop.insert( bestKey );
			prev = cur;
			cur = bestNext;
			curKey = bestKey;
		}
	}

	recordSelection();
	if ( !extend )
		pickedElems.clear();
	Transform wt = shapeRenderTrans( sp );
	for ( qint64 k : loop ) {
		int a = int( k >> 32 ), b = int( k & 0xffffffff );
		if ( a < 0 || a >= nv || b < 0 || b >= nv )
			continue;
		PickedElement pe;
		pe.shapeBlock = hit.shapeBlock;
		pe.type = 2;
		pe.e0 = a;
		pe.e1 = b;
		pe.wA = wt * editVertexLocal( sp, a );
		pe.wB = wt * editVertexLocal( sp, b );
		pe.wC = pe.wA;
		pe.worldPos = ( pe.wA + pe.wB ) / 2.0f;
		if ( pickedElems.indexOf( pe ) < 0 )
			pickedElems.append( pe );
	}
	if ( !( pickMode & 2 ) )
		setPickMode( pickMode | 2 );	// make the loop visible and editable
	emit gizmoStatus( tr( "Edge loop: %1 edge(s)" ).arg( loop.size() ) );
	update();
	return true;
}

float GLView::circleSelectRadius = 26.0f;

void GLView::beginCircleSelect()
{
	if ( !model )
		return;
	if ( riggingWeightPaintMode )
		setRiggingWeightPaintBrushEnabled( false );
	if ( vertexPaintMode )
		setVertexPaintBrushEnabled( false );
	if ( segmentPaintMode )
		setSegmentPaintBrushEnabled( false );
	boxSelecting = false;	// the two gadgets are exclusive
	boxSelectDrag = false;
	circleSelecting = true;
	circlePainting = false;
	circleErasing = false;
	circleSelectPos = QPointF( mapFromGlobal( QCursor::pos() ) );
	setCursor( Qt::BlankCursor );
	emit gizmoStatus( tr( "Circle Select:  LMB paint select, MMB deselect, wheel = size, RMB/Esc = done" ) );
	update();
}

void GLView::applyRiggingWeightPaintBrush( const QPointF & from, const QPointF & to )
{
	if ( !riggingWeightPaintMode || !riggingWeightPaintBrushEnabled || !model || !scene )
		return;
	Shape * shape = shapeForBlock( riggingWeightPaintTarget );
	if ( !shape || shape->isHidden() || shape->verts.isEmpty() )
		return;

	int nv = shape->verts.size();
	if ( !riggingWeightPaintProjectionValid ) {
		Transform wt = shapeRenderTrans( shape );
		QVector<Vector3> world( nv );
		for ( int vertex = 0; vertex < nv; vertex++ )
			world[vertex] = wt * editVertexLocal( shape, vertex );

		QVector<bool> front;
		if ( !scene->xRay ) {
			front.fill( false, nv );
			Vector3 rayOrigin, rayDirection;
			mouseRayWorld( to, rayOrigin, rayDirection );
			for ( const Triangle & triangle : shape->triangles ) {
				if ( triangle[0] >= nv || triangle[1] >= nv || triangle[2] >= nv )
					continue;
				Vector3 normal = Vector3::crossproduct( world[triangle[1]] - world[triangle[0]],
					world[triangle[2]] - world[triangle[0]] );
				if ( Vector3::dotproduct( normal, rayDirection ) < 0.0f ) {
					front[triangle[0]] = true;
					front[triangle[1]] = true;
					front[triangle[2]] = true;
				}
			}
		}

		riggingWeightPaintScreen.resize( nv );
		riggingWeightPaintCandidates.clear();
		QSet<int> selected = pickedVertexRefs().value( riggingWeightPaintTarget );
		auto cacheVertex = [&]( int vertex ) {
			if ( vertex < 0 || vertex >= nv || ( !scene->xRay && !front.value( vertex ) ) )
				return;
			QPointF screen;
			if ( worldToScreen( world.at( vertex ), screen ) ) {
				riggingWeightPaintScreen[vertex] = screen;
				riggingWeightPaintCandidates.append( vertex );
			}
		};
		if ( selected.isEmpty() ) {
			riggingWeightPaintCandidates.reserve( nv );
			for ( int vertex = 0; vertex < nv; vertex++ )
				cacheVertex( vertex );
		} else {
			riggingWeightPaintCandidates.reserve( selected.size() );
			for ( int vertex : selected )
				cacheVertex( vertex );
		}
		riggingWeightPaintProjectionValid = true;
	}

	QVector<int> vertices;
	QVector<float> falloff;
	float radius2 = riggingWeightPaintRadius * riggingWeightPaintRadius;
	QPointF segment = to - from;
	qreal segmentLength2 = segment.x() * segment.x() + segment.y() * segment.y();
	vertices.reserve( riggingWeightPaintCandidates.size() );
	falloff.reserve( riggingWeightPaintCandidates.size() );
	auto testVertex = [&]( int vertex ) {
		const QPointF & screen = riggingWeightPaintScreen.at( vertex );
		qreal t = 0.0;
		if ( segmentLength2 > 1.0e-6 ) {
			QPointF fromVertex = screen - from;
			t = qBound( qreal( 0.0 ),
				( fromVertex.x() * segment.x() + fromVertex.y() * segment.y() ) / segmentLength2,
				qreal( 1.0 ) );
		}
		QPointF delta = screen - ( from + segment * t );
		float distance2 = float( delta.x() * delta.x() + delta.y() * delta.y() );
		if ( distance2 > radius2 )
			return;
		float linear = 1.0f - std::sqrt( distance2 ) / riggingWeightPaintRadius;
		float smooth = linear * linear * ( 3.0f - 2.0f * linear );
		vertices.append( vertex );
		falloff.append( smooth );
	};
	for ( int vertex : riggingWeightPaintCandidates )
		testVertex( vertex );
	if ( vertices.isEmpty() )
		return;
	emit riggingWeightBrushSample( riggingWeightPaintTarget, vertices, falloff,
		riggingWeightPaintBrushMode, riggingWeightPaintWeight, riggingWeightPaintStrength );

	// X-mirror: the position-paired partners of the brushed verts form the
	// mirrored half of the stroke (partners already under the brush are
	// skipped — they were painted directly)
	if ( riggingPaintMirrorX ) {
		QSet<int> brushed( vertices.constBegin(), vertices.constEnd() );
		QVector<int> mVerts;
		QVector<float> mFall;
		mVerts.reserve( vertices.size() );
		mFall.reserve( vertices.size() );
		for ( int i = 0; i < vertices.size(); i++ ) {
			const int mi = mirrorPartnerOf( riggingWeightPaintTarget, vertices.at( i ) );
			if ( mi >= 0 && !brushed.contains( mi ) ) {
				mVerts.append( mi );
				mFall.append( falloff.at( i ) );
			}
		}
		if ( !mVerts.isEmpty() )
			emit riggingWeightBrushSampleMirrored( riggingWeightPaintTarget, mVerts, mFall,
				riggingWeightPaintBrushMode, riggingWeightPaintWeight, riggingWeightPaintStrength );
	}
}

void GLView::applyVertexPaintBrush( const QPointF & from, const QPointF & to )
{
	if ( !vertexPaintMode || !vertexPaintBrushEnabled || !model || !scene )
		return;
	Shape * shape = shapeForBlock( vertexPaintTarget );
	if ( !shape || shape->isHidden() || shape->verts.isEmpty() )
		return;

	const int nv = shape->verts.size();
	if ( !vertexPaintProjectionValid ) {
		Transform wt = shapeRenderTrans( shape );
		QVector<Vector3> world( nv );
		for ( int vertex = 0; vertex < nv; vertex++ )
			world[vertex] = wt * editVertexLocal( shape, vertex );

		QVector<bool> front;
		if ( !scene->xRay ) {
			front.fill( false, nv );
			Vector3 rayOrigin, rayDirection;
			mouseRayWorld( to, rayOrigin, rayDirection );
			for ( const Triangle & triangle : shape->triangles ) {
				if ( triangle[0] >= nv || triangle[1] >= nv || triangle[2] >= nv )
					continue;
				Vector3 normal = Vector3::crossproduct( world[triangle[1]] - world[triangle[0]],
					world[triangle[2]] - world[triangle[0]] );
				if ( Vector3::dotproduct( normal, rayDirection ) < 0.0f ) {
					front[triangle[0]] = true;
					front[triangle[1]] = true;
					front[triangle[2]] = true;
				}
			}
		}

		vertexPaintScreen.resize( nv );
		vertexPaintCandidates.clear();
		QSet<int> selected = pickedVertexRefs().value( vertexPaintTarget );
		auto cacheVertex = [&]( int vertex ) {
			if ( vertex < 0 || vertex >= nv || ( !scene->xRay && !front.value( vertex ) ) )
				return;
			QPointF screen;
			if ( worldToScreen( world.at( vertex ), screen ) ) {
				vertexPaintScreen[vertex] = screen;
				vertexPaintCandidates.append( vertex );
			}
		};
		if ( selected.isEmpty() ) {
			vertexPaintCandidates.reserve( nv );
			for ( int vertex = 0; vertex < nv; vertex++ )
				cacheVertex( vertex );
		} else {
			vertexPaintCandidates.reserve( selected.size() );
			for ( int vertex : selected )
				cacheVertex( vertex );
		}
		vertexPaintProjectionValid = true;
	}

	QVector<int> vertices;
	QVector<float> falloff;
	const float radius2 = vertexPaintRadius * vertexPaintRadius;
	QPointF segment = to - from;
	qreal segmentLength2 = segment.x() * segment.x() + segment.y() * segment.y();
	vertices.reserve( vertexPaintCandidates.size() );
	falloff.reserve( vertexPaintCandidates.size() );
	for ( int vertex : vertexPaintCandidates ) {
		const QPointF & screen = vertexPaintScreen.at( vertex );
		qreal t = 0.0;
		if ( segmentLength2 > 1.0e-6 ) {
			QPointF fromVertex = screen - from;
			t = qBound( qreal( 0.0 ),
				( fromVertex.x() * segment.x() + fromVertex.y() * segment.y() ) / segmentLength2,
				qreal( 1.0 ) );
		}
		QPointF delta = screen - ( from + segment * t );
		float distance2 = float( delta.x() * delta.x() + delta.y() * delta.y() );
		if ( distance2 > radius2 )
			continue;
		float linear = 1.0f - std::sqrt( distance2 ) / vertexPaintRadius;
		float smooth = linear * linear * ( 3.0f - 2.0f * linear );
		vertices.append( vertex );
		falloff.append( smooth );
	}
	if ( !vertices.isEmpty() )
		emit vertexPaintBrushSample( vertexPaintTarget, vertices, falloff );
}

void GLView::applySegmentPaintBrush( const QPointF & from, const QPointF & to )
{
	if ( !segmentPaintMode || !segmentPaintBrushEnabled || !model || !scene ) return;
	Shape * shape = shapeForBlock( segmentPaintTarget );
	if ( !shape || shape->isHidden() || shape->triangles.isEmpty() ) return;
	const int nt = shape->triangles.size();
	const int nv = shape->verts.size();
	if ( !segmentPaintProjectionValid ) {
		Transform wt = shapeRenderTrans( shape );
		QVector<Vector3> world( nv );
		for ( int vertex = 0; vertex < nv; vertex++ )
			world[vertex] = wt * editVertexLocal( shape, vertex );
		QSet<int> selectedFaces;
		for ( const PickedElement & pe : pickedElems )
			if ( pe.shapeBlock == segmentPaintTarget && pe.type == 3 && pe.e0 >= 0 )
				selectedFaces.insert( pe.e0 );
		const bool masked = !pickedElems.isEmpty();
		QSet<int> selectedVertices;
		if ( masked && selectedFaces.isEmpty() )
			selectedVertices = pickedVertexRefs().value( segmentPaintTarget );
		segmentPaintScreen.resize( nt );
		segmentPaintCandidates.clear();
		Vector3 rayOrigin, rayDirection;
		mouseRayWorld( to, rayOrigin, rayDirection );
		for ( int triangleIndex = 0; triangleIndex < nt; triangleIndex++ ) {
			const Triangle & triangle = shape->triangles.at( triangleIndex );
			if ( triangle[0] >= nv || triangle[1] >= nv || triangle[2] >= nv ) continue;
			if ( masked ) {
				if ( !selectedFaces.isEmpty() && !selectedFaces.contains( triangleIndex ) ) continue;
				if ( selectedFaces.isEmpty() && !selectedVertices.contains( triangle[0] )
					&& !selectedVertices.contains( triangle[1] )
					&& !selectedVertices.contains( triangle[2] ) ) continue;
			}
			Vector3 normal = Vector3::crossproduct( world[triangle[1]] - world[triangle[0]],
				world[triangle[2]] - world[triangle[0]] );
			if ( !scene->xRay && Vector3::dotproduct( normal, rayDirection ) >= 0.0f ) continue;
			QPointF screen;
			if ( worldToScreen( ( world[triangle[0]] + world[triangle[1]] + world[triangle[2]] ) / 3.0f, screen ) ) {
				segmentPaintScreen[triangleIndex] = screen;
				segmentPaintCandidates.append( triangleIndex );
			}
		}
		segmentPaintProjectionValid = true;
	}
	QVector<int> triangles;
	const float radius2 = segmentPaintRadius * segmentPaintRadius;
	QPointF segment = to - from;
	qreal length2 = segment.x() * segment.x() + segment.y() * segment.y();
	for ( int triangleIndex : segmentPaintCandidates ) {
		const QPointF & screen = segmentPaintScreen.at( triangleIndex );
		qreal t = 0.0;
		if ( length2 > 1.0e-6 ) {
			QPointF d = screen - from;
			t = qBound( qreal( 0.0 ),
				( d.x() * segment.x() + d.y() * segment.y() ) / length2, qreal( 1.0 ) );
		}
		QPointF delta = screen - ( from + segment * t );
		if ( float( delta.x() * delta.x() + delta.y() * delta.y() ) <= radius2 )
			triangles.append( triangleIndex );
	}
	if ( !triangles.isEmpty() )
		emit segmentPaintBrushSample( segmentPaintTarget, triangles );
}

void GLView::applyCircleSelect( const QPointF & pos, bool erase )
{
	if ( !model || !scene )
		return;
	float rad = circleSelectRadius;
	float rad2 = rad * rad;
	auto inCircleF = [&]( const QPointF & p ) {
		QPointF d = p - pos;
		return float( d.x() * d.x() + d.y() * d.y() ) <= rad2;
	};

	// view direction for back-face rejection when not x-raying
	Vector3 rayO, rayD;
	mouseRayWorld( pos, rayO, rayD );
	bool xray = scene->xRay;

	if ( !editMode ) {
		// object mode: any vertex under the brush picks the shape
		for ( Shape * s : scene->shapes ) {
			if ( !s || s->isHidden() )
				continue;
			Transform wt = shapeRenderTrans( s );
			bool inside = false;
			QPointF sp2;
			for ( const Vector3 & v : s->verts ) {
				if ( worldToScreen( wt * v, sp2 ) && inCircleF( sp2 ) ) {
					inside = true;
					break;
				}
			}
			if ( !inside && s->verts.isEmpty() )
				inside = worldToScreen( wt * Vector3(), sp2 ) && inCircleF( sp2 );
			if ( !inside )
				continue;
			if ( erase ) {
				objSelection.remove( s->id() );
				if ( objActive == s->id() )
					objActive = objSelection.isEmpty() ? -1 : *objSelection.constBegin();
			} else {
				objSelection.insert( s->id() );
			}
		}
		emit objectSelectionChanged();
		update();
		return;
	}

	// edit mode: verts / edges / faces under the brush per pick mode
	for ( Shape * s : scene->shapes ) {
		if ( !s || s->isHidden() || !editShapeBlocks.contains( s->id() ) )
			continue;
		Transform wt = shapeRenderTrans( s );
		int nv = s->verts.size();
		QVector<Vector3> wv( nv );
		QVector<QPointF> sp( nv );
		QVector<bool> in( nv, false ), ok( nv, false );
		for ( int i = 0; i < nv; i++ ) {
			wv[i] = wt * editVertexLocal( s, i );
			ok[i] = worldToScreen( wv[i], sp[i] );
			in[i] = ok[i] && inCircleF( sp[i] );
		}
		// front-facing test, same rule as box select
		QVector<bool> tFront( s->triangles.size(), true );
		QVector<bool> vFront;
		if ( !xray ) {
			vFront.fill( false, nv );
			for ( int t = 0; t < s->triangles.size(); t++ ) {
				const Triangle & tri = s->triangles.at( t );
				if ( tri[0] >= nv || tri[1] >= nv || tri[2] >= nv )
					continue;
				Vector3 n = Vector3::crossproduct( wv[tri[1]] - wv[tri[0]], wv[tri[2]] - wv[tri[0]] );
				bool front = Vector3::dotproduct( n, rayD ) < 0.0f;
				tFront[t] = front;
				if ( front ) {
					vFront[tri[0]] = true;
					vFront[tri[1]] = true;
					vFront[tri[2]] = true;
				}
			}
		}
		auto vVis = [&]( int i ) { return xray || ( i < vFront.size() && vFront[i] ); };
		auto applyPick = [&]( PickedElement & pe ) {
			int at = pickedElems.indexOf( pe );
			if ( erase ) {
				if ( at >= 0 )
					pickedElems.remove( at );
			} else if ( at < 0 ) {
				pickedElems.append( pe );
			}
		};

		if ( pickMode & 1 ) {
			for ( int i = 0; i < nv; i++ ) {
				if ( in[i] && vVis( i ) ) {
					PickedElement pe;
					pe.shapeBlock = s->id();
					pe.type = 1;
					pe.e0 = i;
					pe.e1 = -1;
					pe.worldPos = wv[i];
					pe.wA = pe.wB = pe.wC = wv[i];
					applyPick( pe );
				}
			}
		}
		if ( pickMode & 2 ) {
			QSet<qint64> seen;
			for ( int t = 0; t < s->triangles.size(); t++ ) {
				const Triangle & tri = s->triangles.at( t );
				for ( int e = 0; e < 3; e++ ) {
					int a = tri[e], b = tri[( e + 1 ) % 3];
					if ( a >= nv || b >= nv )
						continue;
					int lo = std::min( a, b ), hi = std::max( a, b );
					qint64 key = ( qint64( lo ) << 32 ) | quint32( hi );
					if ( seen.contains( key ) )
						continue;
					seen.insert( key );
					// the brush touches an edge if either endpoint or the
					// midpoint falls inside the circle
					bool touch = ( in[lo] && vVis( lo ) ) || ( in[hi] && vVis( hi ) );
					if ( !touch && ok[lo] && ok[hi] && vVis( lo ) && vVis( hi ) )
						touch = inCircleF( ( sp[lo] + sp[hi] ) / 2.0 );
					if ( touch ) {
						PickedElement pe;
						pe.shapeBlock = s->id();
						pe.type = 2;
						pe.e0 = lo;
						pe.e1 = hi;
						pe.wA = wv[lo];
						pe.wB = wv[hi];
						pe.wC = pe.wA;
						pe.worldPos = ( wv[lo] + wv[hi] ) * 0.5f;
						applyPick( pe );
					}
				}
			}
		}
		if ( pickMode & 4 ) {
			for ( int t = 0; t < s->triangles.size(); t++ ) {
				const Triangle & tri = s->triangles.at( t );
				if ( tri[0] >= nv || tri[1] >= nv || tri[2] >= nv )
					continue;
				if ( !xray && !tFront[t] )
					continue;
				Vector3 ctr = ( wv[tri[0]] + wv[tri[1]] + wv[tri[2]] ) / 3.0f;
				QPointF cp;
				bool touch = worldToScreen( ctr, cp ) && inCircleF( cp );
				for ( int e = 0; e < 3 && !touch; e++ )
					touch = in[tri[e]];
				if ( touch ) {
					PickedElement pe;
					pe.shapeBlock = s->id();
					pe.type = 3;
					pe.e0 = t;
					pe.e1 = -1;
					pe.wA = wv[tri[0]];
					pe.wB = wv[tri[1]];
					pe.wC = wv[tri[2]];
					pe.worldPos = ctr;
					Vector3 n = Vector3::crossproduct( pe.wB - pe.wA, pe.wC - pe.wA );
					n.normalize();
					pe.worldNormal = n;
					applyPick( pe );
					// a marked quad selects / deselects as one face
					const int tj = quadPartnerMap( s->id() ).value( t, -1 );
					PickedElement partner;
					if ( tj >= 0 && buildFacePick( s->id(), tj, partner ) )
						applyPick( partner );
				}
			}
		}
	}
	update();
}

void GLView::invertSelection()
{
	if ( !model || !scene )
		return;
	if ( riggingWeightPaintMode )
		setRiggingWeightPaintBrushEnabled( false );
	if ( vertexPaintMode )
		setVertexPaintBrushEnabled( false );
	if ( segmentPaintMode )
		setSegmentPaintBrushEnabled( false );
	recordSelection();

	if ( !editMode ) {
		// object mode: every selectable shape not currently selected
		QSet<int> universe;
		for ( Shape * s : scene->shapes )
			if ( s && !s->isHidden() )
				universe.insert( s->id() );
		QSet<int> sel = universe - objSelection;
		objSelection = sel;
		objActive = ( objActive >= 0 && sel.contains( objActive ) )
		            ? objActive : ( sel.isEmpty() ? -1 : *sel.constBegin() );
		emit objectSelectionChanged();
		emit gizmoStatus( tr( "Inverted selection (%1 object(s))" ).arg( sel.size() ) );
		update();
		return;
	}

	// edit mode: invert per enabled pick mode over the edited shapes
	QHash<int, QSet<int>> selV, selF;
	QHash<int, QSet<qint64>> selE;
	for ( const PickedElement & pe : pickedElems ) {
		if ( pe.type == 1 )
			selV[pe.shapeBlock].insert( pe.e0 );
		else if ( pe.type == 3 )
			selF[pe.shapeBlock].insert( pe.e0 );
		else if ( pe.type == 2 ) {
			int lo = std::min( pe.e0, pe.e1 ), hi = std::max( pe.e0, pe.e1 );
			selE[pe.shapeBlock].insert( ( qint64( lo ) << 32 ) | quint32( hi ) );
		}
	}

	QVector<PickedElement> inv;
	for ( Shape * s : scene->shapes ) {
		if ( !s || s->isHidden() || !editShapeBlocks.contains( s->id() ) )
			continue;
		Transform wt = shapeRenderTrans( s );
		int nv = s->verts.size();
		if ( pickMode & 1 ) {
			const QSet<int> & have = selV.value( s->id() );
			for ( int i = 0; i < nv; i++ ) {
				if ( have.contains( i ) )
					continue;
				PickedElement pe;
				pe.shapeBlock = s->id(); pe.type = 1; pe.e0 = i; pe.e1 = -1;
				pe.worldPos = wt * editVertexLocal( s, i ); pe.wA = pe.wB = pe.wC = pe.worldPos;
				inv.append( pe );
			}
		}
		if ( pickMode & 4 ) {
			const QSet<int> & have = selF.value( s->id() );
			for ( int t = 0; t < s->triangles.size(); t++ ) {
				const Triangle & tri = s->triangles.at( t );
				if ( have.contains( t ) || tri[0] >= nv || tri[1] >= nv || tri[2] >= nv )
					continue;
				PickedElement pe;
				pe.shapeBlock = s->id(); pe.type = 3; pe.e0 = t; pe.e1 = -1;
				pe.wA = wt * editVertexLocal( s, tri[0] );
				pe.wB = wt * editVertexLocal( s, tri[1] );
				pe.wC = wt * editVertexLocal( s, tri[2] );
				pe.worldPos = ( pe.wA + pe.wB + pe.wC ) / 3.0f;
				inv.append( pe );
			}
		}
		if ( pickMode & 2 ) {
			const QSet<qint64> & have = selE.value( s->id() );
			QSet<qint64> seen;
			for ( int t = 0; t < s->triangles.size(); t++ ) {
				const Triangle & tri = s->triangles.at( t );
				for ( int e = 0; e < 3; e++ ) {
					int a = tri[e], b = tri[( e + 1 ) % 3];
					if ( a >= nv || b >= nv )
						continue;
					int lo = std::min( a, b ), hi = std::max( a, b );
					qint64 key = ( qint64( lo ) << 32 ) | quint32( hi );
					if ( seen.contains( key ) )
						continue;
					seen.insert( key );
					if ( have.contains( key ) )
						continue;
					PickedElement pe;
					pe.shapeBlock = s->id(); pe.type = 2; pe.e0 = lo; pe.e1 = hi;
					pe.wA = wt * editVertexLocal( s, lo ); pe.wB = wt * editVertexLocal( s, hi ); pe.wC = pe.wA;
					pe.worldPos = ( pe.wA + pe.wB ) * 0.5f;
					inv.append( pe );
				}
			}
		}
	}
	pickedElems = inv;
	emit gizmoStatus( tr( "Inverted selection (%1 element(s))" ).arg( pickedElems.size() ) );
	update();
}

void GLView::checkerDeselect( int nth, int offset, bool armPanel )
{
	// Blender Checker Deselect: drop every Nth element of the selection.
	// Order = the selection's own order (click / box-scan order), not
	// Blender's connectivity walk — good enough for the alternating pattern.
	if ( !editMode || pickedElems.size() < 2 ) {
		emit gizmoStatus( tr( "Checker Deselect needs at least two selected elements" ) );
		return;
	}
	const QVector<PickedElement> seed = pickedElems;
	nth = std::max( nth, 2 );
	QVector<PickedElement> kept;
	kept.reserve( pickedElems.size() );
	for ( int i = 0; i < pickedElems.size(); i++ )
		if ( ( ( i + offset ) % nth ) != 0 )
			kept.append( pickedElems.at( i ) );
	const int dropped = pickedElems.size() - kept.size();
	pickedElems = kept;
	if ( armPanel && model && model->undoStack ) {
		lastOpExRerun = [this]( const QVector<TlOpParam> & ps ) {
			checkerDeselect( int( ps.value( 0 ).value + 0.5 ), int( ps.value( 1 ).value + 0.5 ), false );
		};
		QVector<TlOpParam> ps( 2 );
		ps[0].label = tr( "Deselect every Nth" );
		ps[0].type = TlOpParam::Int;
		ps[0].value = nth;
		ps[0].mn = 2.0;
		ps[0].mx = 100.0;
		ps[0].step = 1.0;
		ps[1].label = tr( "Offset" );
		ps[1].type = TlOpParam::Int;
		ps[1].value = offset;
		ps[1].mn = 0.0;
		ps[1].mx = 100.0;
		ps[1].step = 1.0;
		armOperatorPanelEx( tr( "Checker Deselect" ), ps, 0, seed );
	}
	emit gizmoStatus( tr( "Checker Deselect: %1 deselected, %2 kept" )
		.arg( dropped ).arg( pickedElems.size() ) );
	update();
}

void GLView::repeatLastOperator()
{
	// Blender Repeat Last (Shift+R): run the last panel operator again with
	// its current parameter values, on the CURRENT selection — a fresh
	// application, not the panel's undo-and-adjust
	if ( !lastOpExRerun ) {
		emit gizmoStatus( tr( "Repeat Last: no repeatable operator yet (adjust-panel operators only)" ) );
		return;
	}
	lastOpExRerun( lastOpExParams );
}

// Selection undo: a small dedicated history so any selection (click, box, invert,
// select-all) can be reverted with Ctrl+Z. It resets whenever the model itself
// changes (detected via the undo-stack index) so Ctrl+Z after an edit undoes the
// edit, not a stale selection - and it never dirties the document.
static void tlSyncSelUndo( NifModel * model, QVector<GLView::SelState> & u,
                           QVector<GLView::SelState> & r, int & syncedIndex )
{
	int mi = ( model && model->undoStack ) ? model->undoStack->index() : -1;
	if ( mi != syncedIndex ) {
		u.clear();
		r.clear();
		syncedIndex = mi;
	}
}

void GLView::recordSelection()
{
	tlSyncSelUndo( model, selUndo, selRedo, selUndoModelIndex );
	selRedo.clear();
	selUndo.append( SelState{ pickedElems, objSelection, objActive } );
	if ( selUndo.size() > 128 )
		selUndo.remove( 0, selUndo.size() - 128 );
	// Every element-selection mutator snapshots first, so this is the one
	// choke point where external mirrors (the UV editor) can be notified.
	// Deferred: the mutation itself happens after this call returns.
	scheduleElementSelectionNotify();
}

void GLView::scheduleElementSelectionNotify()
{
	if ( elemSelNotifyPending )
		return;
	elemSelNotifyPending = true;
	QTimer::singleShot( 0, this, [this]() {
		elemSelNotifyPending = false;
		emit elementSelectionChanged();
	} );
}

void GLView::setElementSelectionExternal( int shapeBlock, const QVector<PickedElement> & elems, int mode )
{
	if ( !editMode )
		return;
	recordSelection();
	QVector<PickedElement> kept;
	kept.reserve( pickedElems.size() + elems.size() );
	for ( const PickedElement & pe : std::as_const( pickedElems ) )
		if ( pe.shapeBlock != shapeBlock )
			kept << pe;
	kept += elems;
	pickedElems = kept;
	if ( mode >= 1 && mode <= 3 && pickMode != mode ) {
		pickMode = mode;
		emit pickModeChanged( mode );
	}
	refreshPickedElementPositions();
	update();
}

void GLView::setElementPickMode( int mode )
{
	if ( mode < 0 || mode > 3 || mode == pickMode )
		return;
	pickMode = mode;
	emit pickModeChanged( mode );
	update();
}

bool GLView::hasSelectionUndo()
{
	tlSyncSelUndo( model, selUndo, selRedo, selUndoModelIndex );
	return !selUndo.isEmpty();
}

bool GLView::hasSelectionRedo()
{
	tlSyncSelUndo( model, selUndo, selRedo, selUndoModelIndex );
	return !selRedo.isEmpty();
}

bool GLView::selectionUndo()
{
	if ( !hasSelectionUndo() )
		return false;
	selRedo.append( SelState{ pickedElems, objSelection, objActive } );
	SelState s = selUndo.takeLast();
	pickedElems = s.picked;
	objSelection = s.objSel;
	objActive = s.objActive;
	if ( !editMode )
		emit objectSelectionChanged();
	scheduleElementSelectionNotify();
	emit gizmoStatus( tr( "Undo selection" ) );
	update();
	return true;
}

bool GLView::selectionRedo()
{
	if ( !hasSelectionRedo() )
		return false;
	selUndo.append( SelState{ pickedElems, objSelection, objActive } );
	SelState s = selRedo.takeLast();
	pickedElems = s.picked;
	objSelection = s.objSel;
	objActive = s.objActive;
	if ( !editMode )
		emit objectSelectionChanged();
	scheduleElementSelectionNotify();
	emit gizmoStatus( tr( "Redo selection" ) );
	update();
	return true;
}

void GLView::hideSelected()
{
	if ( !model || !scene->currentBlock.isValid() )
		return;
	// hide the nearest NiAVObject of the current selection
	int b = model->getBlockNumber( QModelIndex( scene->currentBlock ) );
	while ( b >= 0 && !model->blockInherits( model->getBlockIndex( b ), "NiAVObject" ) )
		b = model->getParent( b );
	if ( b < 0 )
		return;
	scene->hiddenNodes.insert( b );
	updateDimmedBlocks();
	emit gizmoStatus( tr( "Hid node %1 (Alt+H to reveal all)" ).arg( b ) );
	update();
}

void GLView::unhideAll()
{
	if ( scene->hiddenNodes.isEmpty() )
		return;
	scene->hiddenNodes.clear();
	updateDimmedBlocks();
	emit gizmoStatus( tr( "Revealed all hidden nodes" ) );
	update();
}

void GLView::updateDimmedBlocks()
{
	if ( !model )
		return;
	QSet<qint32> dim;
	if ( !scene->hiddenNodes.isEmpty() ) {
		// a block is dimmed if it, or any ancestor, is a hidden node
		for ( int b = 0; b < model->getBlockCount(); b++ ) {
			for ( int p = b; p >= 0; p = model->getParent( p ) ) {
				if ( scene->hiddenNodes.contains( p ) ) {
					dim.insert( b );
					break;
				}
			}
		}
	}
	model->dimmedBlocks = dim;
	emit hiddenNodesChanged();
}

Shape * GLView::shapeForBlock( int b ) const
{
	for ( Shape * s : scene->shapes ) {
		if ( s && s->id() == b )
			return s;
	}
	return nullptr;
}

void GLView::selectLinked( bool flatOnly, float maxAngleDeg )
{
	const float cosThresh = std::cos( deg2rad( std::max( maxAngleDeg, 0.0f ) ) );
	if ( !editMode || pickedElems.isEmpty() )
		return;
	if ( riggingWeightPaintMode )
		setRiggingWeightPaintBrushEnabled( false );
	if ( vertexPaintMode )
		setVertexPaintBrushEnabled( false );
	if ( segmentPaintMode )
		setSegmentPaintBrushEnabled( false );
	QVector<PickedElement> preGrowSel = pickedElems;	// restored by the redo panel

	// seed triangles and vertices per shape from the current selection
	QHash<int, QSet<int>> seedTris, seedVerts;
	for ( const auto & pe : pickedElems ) {
		if ( pe.type == 3 )
			seedTris[pe.shapeBlock].insert( pe.e0 );
		else if ( pe.type == 1 )
			seedVerts[pe.shapeBlock].insert( pe.e0 );
		else if ( pe.type == 2 ) {
			seedVerts[pe.shapeBlock].insert( pe.e0 );
			seedVerts[pe.shapeBlock].insert( pe.e1 );
		}
	}

	QSet<int> shapes;
	for ( int k : seedTris.keys() )
		shapes.insert( k );
	for ( int k : seedVerts.keys() )
		shapes.insert( k );

	// pickMode is a bitmask (1 vertex, 2 edge, 4 face), while PickedElement
	// stores element types as 1/2/3. Passing face bit 4 through as an element
	// type previously fell into the edge branch, making Select Linked appear to
	// do nothing in face mode.
	int outType = flatOnly ? 3 : ( ( pickMode & 4 ) ? 3 : ( ( pickMode & 2 ) ? 2 : 1 ) );

	for ( int sb : shapes ) {
		Shape * s = shapeForBlock( sb );
		if ( !s || s->triangles.isEmpty() )
			continue;
		int nv = s->verts.size();

		// vertex -> incident triangles
		QHash<int, QVector<int>> vtris;
		for ( int ti = 0; ti < s->triangles.size(); ti++ ) {
			const Triangle & t = s->triangles.at( ti );
			if ( t[0] >= nv || t[1] >= nv || t[2] >= nv )
				continue;
			vtris[t[0]].append( ti );
			vtris[t[1]].append( ti );
			vtris[t[2]].append( ti );
		}

		auto triNormal = [&]( int ti ) {
			const Triangle & t = s->triangles.at( ti );
			Vector3 n = Vector3::crossproduct( editVertexLocal( s, t[1] ) - editVertexLocal( s, t[0] ),
				editVertexLocal( s, t[2] ) - editVertexLocal( s, t[0] ) );
			n.normalize();
			return n;
		};

		// seed triangle set (from selected faces and the triangles around seed verts)
		QSet<int> seed = seedTris.value( sb );
		for ( int v : seedVerts.value( sb ) )
			for ( int ti : vtris.value( v ) )
				seed.insert( ti );
		if ( seed.isEmpty() )
			continue;

		// flood fill by shared vertices (optionally only across coplanar faces)
		QSet<int> comp = seed;
		QList<int> stack = seed.values();
		while ( !stack.isEmpty() ) {
			int ti = stack.takeLast();
			Vector3 nt = triNormal( ti );
			const Triangle & t = s->triangles.at( ti );
			for ( int c = 0; c < 3; c++ ) {
				for ( int nb : vtris.value( t[c] ) ) {
					if ( comp.contains( nb ) )
						continue;
					if ( flatOnly && Vector3::dotproduct( triNormal( nb ), nt ) < cosThresh )
						continue;	// only grow across faces within maxAngleDeg
					comp.insert( nb );
					stack.append( nb );
				}
			}
		}

		Transform wt = shapeRenderTrans( s );
		auto already = [&]( const PickedElement & pe ) { return pickedElems.indexOf( pe ) >= 0; };

		if ( outType == 3 ) {
			for ( int ti : comp ) {
				const Triangle & t = s->triangles.at( ti );
				PickedElement pe;
				pe.shapeBlock = sb;
				pe.type = 3;
				pe.e0 = ti;
				pe.wA = wt * editVertexLocal( s, t[0] );
				pe.wB = wt * editVertexLocal( s, t[1] );
				pe.wC = wt * editVertexLocal( s, t[2] );
				pe.worldPos = ( pe.wA + pe.wB + pe.wC ) / 3.0f;
				Vector3 n = Vector3::crossproduct( pe.wB - pe.wA, pe.wC - pe.wA );
				n.normalize();
				pe.worldNormal = n;
				if ( !already( pe ) )
					pickedElems.append( pe );
			}
		} else if ( outType == 1 ) {
			QSet<int> vs;
			for ( int ti : comp ) {
				const Triangle & t = s->triangles.at( ti );
				vs.insert( t[0] );
				vs.insert( t[1] );
				vs.insert( t[2] );
			}
			for ( int vi : vs ) {
				PickedElement pe;
				pe.shapeBlock = sb;
				pe.type = 1;
				pe.e0 = vi;
				pe.worldPos = wt * editVertexLocal( s, vi );
				pe.wA = pe.wB = pe.wC = pe.worldPos;
				if ( !already( pe ) )
					pickedElems.append( pe );
			}
		} else {
			QSet<QPair<int, int>> edges;
			for ( int ti : comp ) {
				const Triangle & t = s->triangles.at( ti );
				for ( int e = 0; e < 3; e++ ) {
					int a = t[e], b = t[( e + 1 ) % 3];
					edges.insert( qMakePair( std::min( a, b ), std::max( a, b ) ) );
				}
			}
			for ( const auto & ed : edges ) {
				PickedElement pe;
				pe.shapeBlock = sb;
				pe.type = 2;
				pe.e0 = ed.first;
				pe.e1 = ed.second;
				pe.wA = wt * editVertexLocal( s, ed.first );
				pe.wB = wt * editVertexLocal( s, ed.second );
				pe.wC = pe.wA;
				pe.worldPos = ( pe.wA + pe.wB ) / 2.0f;
				if ( !already( pe ) )
					pickedElems.append( pe );
			}
		}
	}

	if ( flatOnly && pickMode != 4 ) {
		pickMode = 4;	// switch to face mode without clearing the new selection
		emit pickModeChanged( pickMode );
	}
	emit gizmoStatus( tr( "Selected linked: %1 element(s)" ).arg( pickedElems.size() ) );
	// Redo Panel v2 (only the by-angle variant): pure selection op, nothing
	// on the undo stack — the reapply just restores the seed and re-grows
	if ( flatOnly ) {
		lastOpExRerun = [this]( const QVector<TlOpParam> & ps ) {
			selectLinked( true, float( ps.value( 0 ).value ) );
		};
		QVector<TlOpParam> ps( 1 );
		ps[0].label = tr( "Sharpness°" );
		ps[0].type = TlOpParam::Float;
		ps[0].value = maxAngleDeg;
		ps[0].mn = 0.0;
		ps[0].mx = 180.0;
		ps[0].step = 1.0;
		ps[0].decimals = 1;
		armOperatorPanelEx( tr( "Select Linked by Angle" ), ps, 0, preGrowSel );
	}
	update();
}

void GLView::setPickMode( int m )
{
	if ( m == pickMode )
		return;
	pickMode = m;	// bitmask: 1 vertex, 2 edge, 4 face (may be combined)
	emit pickModeChanged( pickMode );
	update();
}

void GLView::snapSelectionToGrid()
{
	if ( !model || pickedElems.isEmpty() )
		return;
	QHash<int, QSet<int>> byShape = pickedVertexRefs();
	if ( byShape.isEmpty() )
		return;
	float step = std::max( gizmoSnapStep, 0.0001f );

	nifSnapshotOp( model, tr( "Snap selection to grid" ), [&]() {
		for ( auto it = byShape.constBegin(); it != byShape.constEnd(); it++ ) {
			QModelIndex iShape = model->getBlockIndex( it.key() );
			Node * n = scene->getNode( model, iShape );
			Shape * shape = shapeForBlock( it.key() );
			if ( !n || !shape )
				continue;
			Transform wt = shapeRenderTrans( n );
			float sc = ( wt.scale != 0.0f ) ? wt.scale : 1.0f;
			for ( int vi : it.value() ) {
				Vector3 local;
				if ( !tlGetVertexLocal( model, iShape, vi, local ) )
					continue;
				Vector3 world = wt * ( editDeformedCageActive() ? shape->skinVertex( vi, local ) : local );
				for ( int c = 0; c < 3; c++ )
					world[c] = std::round( world[c] / step ) * step;
				Vector3 cageLocal = wt.rotation.inverted() * ( ( world - wt.translation ) * ( 1.0f / sc ) );
				Vector3 rawLocal;
				if ( editVertexRawLocal( shape, vi, cageLocal, rawLocal ) )
					tlSetVertexLocal( model, iShape, vi, rawLocal );
			}
		}
	} );
	modelChanged();
}

void GLView::showSnapMenu()
{
	AutoCloseMenu m;
	m.addSection( tr( "Snap" ) );
	QAction * aSelGrid = m.addAction( tr( "Selection to Grid" ) );
	QAction * aSelCur  = m.addAction( tr( "Selection to Cursor" ) );
	QAction * aSelOrig = m.addAction( editMode ? tr( "Selection to Node Origin" ) : tr( "Selection to Active" ) );
	// edit mode: the active element (last picked) is a separate snap target
	QAction * aSelAct = editMode ? m.addAction( tr( "Selection to Active" ) ) : nullptr;
	m.addSeparator();
	QAction * aCurSel  = m.addAction( tr( "Cursor to Selected" ) );
	QAction * aCurOrig = m.addAction( tr( "Cursor to World Origin" ) );
	QAction * aCurNode = m.addAction( editMode ? tr( "Cursor to Node Origin" ) : tr( "Cursor to Active" ) );
	QAction * aCurAct = editMode ? m.addAction( tr( "Cursor to Active" ) ) : nullptr;
	QAction * aCurGrid = m.addAction( tr( "Cursor to Grid" ) );

	// object mode works on the selected objects' origins instead of vertices
	bool hasSel = editMode ? !pickedElems.isEmpty() : !objSelection.isEmpty();
	aSelGrid->setEnabled( hasSel );
	aSelCur->setEnabled( hasSel );
	aSelOrig->setEnabled( editMode ? hasSel : ( objActive >= 0 && objSelection.size() > 1 ) );
	if ( aSelAct )
		aSelAct->setEnabled( hasSel && pickedElems.size() > 1 );
	if ( aCurAct )
		aCurAct->setEnabled( hasSel );
	if ( !editMode ) {
		aCurSel->setEnabled( hasSel );
		aCurNode->setEnabled( objActive >= 0 );
	}

	auto objOrigin = [this]( int b ) {
		Node * nd = scene->getNode( model, model->getBlockIndex( b ) );
		return nd ? nd->worldTrans().translation : Vector3();
	};
	auto objSelectionAvg = [this, &objOrigin]() {
		Vector3 sum;
		int n = 0;
		for ( int b : objSelection ) {
			sum += objOrigin( b );
			n++;
		}
		return ( n > 0 ) ? ( sum / float( n ) ) : Vector3();
	};

	// average origin of the node(s) owning the current selection
	auto nodeOriginAvg = [this]() {
		QSet<int> shapes;
		for ( const auto & pe : pickedElems )
			if ( pe.shapeBlock >= 0 )
				shapes.insert( pe.shapeBlock );
		if ( shapes.isEmpty() && editShapeBlock >= 0 )
			shapes.insert( editShapeBlock );
		Vector3 sum;
		int n = 0;
		for ( int sb : shapes ) {
			Node * nd = scene->getNode( model, model->getBlockIndex( sb ) );
			if ( nd ) {
				sum += nd->worldTrans().translation;
				n++;
			}
		}
		return ( n > 0 ) ? ( sum / float( n ) ) : Vector3();
	};

	QAction * r = m.exec( QCursor::pos() );
	if ( !r )
		return;
	if ( r == aSelGrid ) {
		if ( editMode ) {
			snapSelectionToGrid();
		} else {
			float step = std::max( gizmoSnapStep, 0.0001f );
			ChangeValueCommand::createTransaction();
			for ( int b : objSelection ) {
				Vector3 wp = objOrigin( b );
				for ( int c = 0; c < 3; c++ )
					wp[c] = std::round( wp[c] / step ) * step;
				snapBlockWorldPos( b, wp );
			}
		}
	} else if ( r == aSelCur ) {
		if ( editMode ) {
			movePickedVertsToCursor();
		} else {
			ChangeValueCommand::createTransaction();
			for ( int b : objSelection )
				snapBlockWorldPos( b, cursorPos );
		}
	} else if ( r == aSelOrig ) {
		if ( editMode ) {
			Vector3 saved = cursorPos;
			cursorPos = nodeOriginAvg();	// reuse the move-to-cursor path
			movePickedVertsToCursor();
			cursorPos = saved;
		} else {
			Vector3 tgt = objOrigin( objActive );
			ChangeValueCommand::createTransaction();
			for ( int b : objSelection ) {
				if ( b != objActive )
					snapBlockWorldPos( b, tgt );
			}
		}
	} else if ( aSelAct && r == aSelAct ) {
		// collapse the selection onto the active (last picked) element
		Vector3 saved = cursorPos;
		cursorPos = pickedElems.constLast().worldPos;
		movePickedVertsToCursor();	// reuse the move-to-cursor path
		cursorPos = saved;
	} else if ( aCurAct && r == aCurAct ) {
		cursorPos = pickedElems.constLast().worldPos;
		update();
	} else if ( r == aCurSel ) {
		cursorPos = editMode ? pickedMedian() : objSelectionAvg();
		update();
	} else if ( r == aCurOrig ) {
		cursorPos = Vector3();
		update();
	} else if ( r == aCurNode ) {
		cursorPos = editMode ? nodeOriginAvg() : objOrigin( objActive );
		update();
	} else if ( r == aCurGrid ) {
		float step = std::max( gizmoSnapStep, 0.0001f );
		for ( int c = 0; c < 3; c++ )
			cursorPos[c] = std::round( cursorPos[c] / step ) * step;
		update();
	}
}

QModelIndex parent( QModelIndex ix, QModelIndex xi )
{
	ix = ix.sibling( ix.row(), 0 );
	xi = xi.sibling( xi.row(), 0 );

	while ( ix.isValid() ) {
		QModelIndex x = xi;

		while ( x.isValid() ) {
			if ( ix == x )
				return ix;

			x = x.parent();
		}

		ix = ix.parent();
	}

	return QModelIndex();
}

void GLView::dataChanged( const QModelIndex & idx, const QModelIndex & xdi )
{
	if ( doCompile )
		return;

	if ( model && idx == model->getRootIndex() && xdi == idx ) {
		modelChanged();
		return;
	}

	QModelIndex ix = idx;

	if ( idx == xdi ) {
		if ( idx.column() != 0 )
			ix = idx.sibling( idx.row(), 0 );
	} else {
		ix = ::parent( idx, xdi );
	}

	if ( ix.isValid() ) {
		scene->update( model, idx );
		update();
	} else {
		modelChanged();
	}
}

void GLView::modelChanged()
{
	if ( doCompile )
		return;

	doCompile = 1;
	//doCenter  = true;
	update();
}

void GLView::modelLinked()
{
	if ( doCompile )
		return;

	doCompile = 1; //scene->update( model, QModelIndex() );
	update();
}

void GLView::modelDestroyed()
{
	setNif( nullptr );
}


/*
 * UI
 */

void GLView::setSceneTime( float t )
{
	time = t;
	update();
	emit sceneTimeChanged( time, scene->timeMin(), scene->timeMax() );
}

void GLView::setSceneSequence( const QString & seqname )
{
	// Update UI
	QAction * action = qobject_cast<QAction *>(sender());
	if ( !action ) {
		// Called from self and not UI
		emit sequenceChanged( seqname );
	}

	scene->setSequence( seqname );
	time = scene->timeMin();
	emit sceneTimeChanged( time, scene->timeMin(), scene->timeMax() );
	update();
}

// TODO: Multiple user views, ala Recent Files
void GLView::saveUserView()
{
	userViewRot = Rot;
	userViewPos = Pos;
	userViewDist = Dist;
	userViewSaved = true;
}

void GLView::loadUserView()
{
	if ( !userViewSaved )
		return;
	setRotation( userViewRot[0], userViewRot[1], userViewRot[2] );
	setPosition( userViewPos );
	setDistance( userViewDist );
}

inline bool GLView::kbd( int n ) const
{
	return bool( kbdState & ( 1ULL << n ) );
}

void GLView::advanceGears()
{
	updatePending -= (unsigned char) bool( updatePending );

	std::chrono::steady_clock::time_point t = std::chrono::steady_clock::now();
	float dT = float( std::chrono::duration_cast< std::chrono::microseconds >( t - lastTime ).count() ) * 0.000001f;
	lastTime = t;

	if ( !isVisible() )
		return;

	dT = std::clamp< float >( dT, 0.0f, 1.0f );
	if ( ( animState & AnimEnabled ) && ( animState & AnimPlay )
		&& scene->timeMin() != scene->timeMax() )
	{
		time += dT;

		if ( time > scene->timeMax() ) {
			if ( ( animState & AnimSwitch ) && !scene->animGroups.isEmpty() ) {
				int ix = scene->animGroups.indexOf( scene->animGroup );

				if ( ++ix >= scene->animGroups.count() )
					ix -= scene->animGroups.count();

				setSceneSequence( scene->animGroups.value( ix ) );
			} else if ( animState & AnimLoop ) {
				time = scene->timeMin();
			} else {
				// Animation has completed and is not looping
				//	or cycling through animations.
				// Reset time and state and then inform UI it has stopped.
				time = scene->timeMin();
				animState &= ~AnimPlay;
				emit sequenceStopped();
			}
		} else {
			// Animation is not done yet
		}

		emit sceneTimeChanged( time, scene->timeMin(), scene->timeMax() );
		update();
	}

	float	rotateStep = cfg.rotSpd * dT;
	// Fix movement speed for Starfield scale
	dT *= scale();
	float	moveStep = cfg.moveSpd * dT;
	// Blender fly mode: scroll wheel sets the base speed, Shift boosts it
	if ( freeCamera ) {
		moveStep *= freeCamSpeed;
		if ( kbd( Key_Shift ) )
			moveStep *= 4.0f;
	}

	// TODO: Some kind of input class for choosing the appropriate
	// keys based on user preferences of what app they would like to
	// emulate for the control scheme
	// Rotation
	if ( kbd( Key_Shift ) && !frontalLight ) {
		if ( kbd( Key_RotateUp ) )    rotateLight( -rotateStep, 0.0f );
		if ( kbd( Key_RotateDown ) )  rotateLight( rotateStep, 0.0f );
		if ( kbd( Key_RotateLeft ) )  rotateLight( 0.0f, -rotateStep );
		if ( kbd( Key_RotateRight ) ) rotateLight( 0.0f, rotateStep );
	} else {
		if ( kbd( Key_RotateUp ) )    rotate( -rotateStep, 0, 0 );
		if ( kbd( Key_RotateDown ) )  rotate( rotateStep, 0, 0 );
		if ( kbd( Key_RotateLeft ) )  rotate( 0, 0, -rotateStep );
		if ( kbd( Key_RotateRight ) ) rotate( 0, 0, rotateStep );
	}

	// Movement
	if ( kbd( Key_MoveLeft ) ) move( moveStep, 0, 0 );
	if ( kbd( Key_MoveRight ) ) move( -moveStep, 0, 0 );
	if ( kbd( Key_MoveForward ) ) move( 0, 0, moveStep );
	if ( kbd( Key_MoveBack ) ) move( 0, 0, -moveStep );
	if ( kbd( Key_MoveDown ) ) move( 0, moveStep, 0 );
	if ( kbd( Key_MoveUp ) ) move( 0, -moveStep, 0 );

	// Focal Length
	if ( kbd( Key_ZoomIn ) )   setZoom( Zoom * std::sqrt( Settings::zoomOutScale ) );
	if ( kbd( Key_ZoomOut ) )  setZoom( Zoom * std::sqrt( Settings::zoomInScale ) );

	if ( mouseMov[0] != 0 || mouseMov[1] != 0 || mouseMov[2] != 0 ) {
		move( mouseMov[0], mouseMov[1], mouseMov[2] );
		mouseMov = Vector3();
	}

	if ( mouseRot[0] != 0 || mouseRot[1] != 0 || mouseRot[2] != 0 ) {
		rotate( mouseRot[0], mouseRot[1], mouseRot[2] );
		mouseRot = Vector3();
	}

	// update display without movement
	if ( kbd( Key_Update ) ) update();
}


// TODO: Separate widget
void GLView::saveImage()
{
	auto dlg = new QDialog( qApp->activeWindow() );
	QGridLayout * lay = new QGridLayout( dlg );
	dlg->setWindowTitle( tr( "Save View" ) );
	dlg->setLayout( lay );
	dlg->setMinimumWidth( 400 );

	// Save file format, quality and default screenshot path
	int imgFormat, jpegQuality, ss;
	QString imgPath;
	{
		QSettings settings;
		jpegQuality = settings.value( "JPEG/Quality", 90 ).toInt();
		imgFormat = settings.value( "Screenshot/Format", 0 ).toInt();
		imgFormat = std::clamp< int >( imgFormat, 0, 4 );
		imgPath = settings.value( "Screenshot/Folder", "screenshots" ).toString();
		ss = settings.value( "Screenshot/Size", 0 ).toInt();
		ss = std::clamp< int >( ss, 0, 3 );
	}

	QString date = QDateTime::currentDateTime().toString( "yyyyMMdd_HH-mm-ss" );
	QString name = model->getFilename();

	QString nifFolder = model->getFolder();
	static const char *	screenshotImgFormats[5] = {
		".jpg", ".png", ".webp", ".bmp", ".dds"
	};
	QString filename = name + (!name.isEmpty() ? "_" : "") + date + screenshotImgFormats[imgFormat];

	// Default: NifSkope directory
	QString nifskopePath = "screenshots/" + filename;
	// Absolute: NIF directory
	QString nifPath = nifFolder + (!nifFolder.isEmpty() ? "/" : "") + filename;

	FileSelector * file = new FileSelector( FileSelector::SaveFile, tr( "File" ), QBoxLayout::LeftToRight );
	file->setParent( dlg );
	file->setFilter( { "Images (*.jpg *.png *.webp *.bmp *.dds)", "JPEG (*.jpg)", "PNG (*.png)", "WebP (*.webp)", "BMP (*.bmp)", "DDS (*.dds)" } );
	file->setFile( imgPath + "/" + filename  );
	lay->addWidget( file, 0, 0, 1, -1 );

	QPushButton * nifskopeDir = new QPushButton( tr( "NifSkope Directory" ), dlg );
	nifskopeDir->setToolTip( tr( "Save to NifSkope screenshots directory" ) );

	QPushButton * niffileDir = new QPushButton( tr( "NIF Directory" ), dlg );
	niffileDir->setDisabled( nifFolder.isEmpty() );
	niffileDir->setToolTip( tr( "Save to NIF file directory" ) );

	lay->addWidget( nifskopeDir, 1, 0, 1, 1 );
	lay->addWidget( niffileDir, 1, 1, 1, 1 );

	QHBoxLayout * pixBox = new QHBoxLayout;
	pixBox->setAlignment( Qt::AlignRight );
	QSpinBox * pixQuality = new QSpinBox( dlg );
	pixQuality->setRange( -1, 100 );
	pixQuality->setSingleStep( 10 );
	pixQuality->setValue( jpegQuality );
	pixQuality->setSpecialValueText( tr( "Auto" ) );
	pixQuality->setMaximumWidth( pixQuality->minimumSizeHint().width() );
	pixBox->addWidget( new QLabel( tr( "JPEG Quality" ), dlg ) );
	pixBox->addWidget( pixQuality );
	lay->addLayout( pixBox, 1, 2, Qt::AlignRight );


	// Get max viewport size for platform
	GLint	dims[2];
	glGetIntegerv( GL_MAX_VIEWPORT_DIMS, dims );

	// Disable any of these that would exceed the max viewport size of the platform
	int	w = width();
	int	h = height();
	double	p = devicePixelRatioF();
	QRadioButton *	btnSS[4];
	for ( int i = 0; i < 4; i++ ) {
		QRadioButton* &	b = btnSS[i];
		b = new QRadioButton( ( i < 2 ? ( i == 0 ? "1x" : "2x" ) : ( i == 2 ? "4x" : "8x" ) ), dlg );
		b->setCheckable( true );
		if ( i > 0 ) {
			int	wp = int( p * ( w << i ) + 0.5 );
			int	hp = int( p * ( h << i ) + 0.5 );
			bool isDisabled = ( wp > dims[0] || hp > dims[1] );
			b->setDisabled( isDisabled );
			if ( isDisabled )
				ss = std::min< int >( ss, i - 1 );
		}
	}
	btnSS[ss]->setChecked( true );


	auto grpBox = new QGroupBox( tr( "Image Size" ), dlg );
	auto grpBoxLayout = new QHBoxLayout;
	grpBoxLayout->addWidget( btnSS[0] );
	grpBoxLayout->addWidget( btnSS[1] );
	grpBoxLayout->addWidget( btnSS[2] );
	grpBoxLayout->addWidget( btnSS[3] );
	grpBoxLayout->addWidget( new QLabel( "<b>Caution:</b><br/> 4x and 8x may be memory intensive.", dlg ) );
	grpBoxLayout->addStretch( 1 );
	grpBox->setLayout( grpBoxLayout );

	auto grpSize = new QButtonGroup( dlg );
	grpSize->addButton( btnSS[0], 0 );
	grpSize->addButton( btnSS[1], 1 );
	grpSize->addButton( btnSS[2], 2 );
	grpSize->addButton( btnSS[3], 3 );

	grpSize->setExclusive( true );

	lay->addWidget( grpBox, 2, 0, 1, -1 );


	QHBoxLayout * hBox = new QHBoxLayout;
	QPushButton * btnOk = new QPushButton( tr( "Save" ), dlg );
	QPushButton * btnCancel = new QPushButton( tr( "Cancel" ), dlg );
	hBox->addWidget( btnOk );
	hBox->addWidget( btnCancel );
	lay->addLayout( hBox, 3, 0, 1, -1 );

	// Set FileSelector to NifSkope dir (relative)
	connect( nifskopeDir, &QPushButton::clicked, [=]()
		{
			file->setText( nifskopePath );
			file->setFile( nifskopePath );
		}
	);
	// Set FileSelector to NIF File dir (absolute)
	connect( niffileDir, &QPushButton::clicked, [=]()
		{
			file->setText( nifPath );
			file->setFile( nifPath );
		}
	);

	// Validate on OK
	connect( btnOk, &QPushButton::clicked, [&]()
		{
			imgPath = file->file();
			for ( imgFormat = int( sizeof( screenshotImgFormats ) / sizeof( char * ) ); --imgFormat > 0; ) {
				if ( imgPath.endsWith( screenshotImgFormats[imgFormat], Qt::CaseInsensitive ) )
					break;
			}
#ifdef Q_OS_WIN32
			imgPath.replace( QChar('\\'), QChar('/') );
#endif
			imgPath.truncate( imgPath.lastIndexOf( QChar('/') ) );

			// Supersampling
			ss = grpSize->checkedId();

			// Save JPEG Quality and other settings
			QSettings settings;
			settings.setValue( "JPEG/Quality", pixQuality->value() );
			settings.setValue( "Screenshot/Format", imgFormat );
			if ( !imgPath.isEmpty() )
				settings.setValue( "Screenshot/Folder", imgPath );
			settings.setValue( "Screenshot/Size", ss );

			auto	prvContext = pushGLContext();

			// Resize viewport for supersampling
			if ( ss > 0 )
				resizeGL( int( p * ( w << ss ) + 0.5 ), int( p * ( h << ss ) + 0.5 ) );

			QSize	fboSize( getSizeInPixels() );
			auto	savedSceneOptions = scene->options;
			bool	haveAlpha = ( imgFormat == 1 || imgFormat == 4 );	// PNG or DDS
			std::string	err;

			QImage	rgbImg;
			const Color4 & c = cfg.background;
			try {
				QOpenGLFramebufferObjectFormat fboFmt;
				fboFmt.setTextureTarget( GL_TEXTURE_2D );
				fboFmt.setInternalTextureFormat( GL_SRGB8_ALPHA8 );
				fboFmt.setMipmap( false );
				fboFmt.setAttachment( QOpenGLFramebufferObject::Attachment::Depth );
				fboFmt.setSamples( 16 >> ss );

				QOpenGLFramebufferObject fbo( fboSize.width(), fboSize.height(), fboFmt );
				fbo.bind();

				if ( haveAlpha ) {
					glClearColor( c.red(), c.green(), c.blue(), 0.0f );
					scene->options = savedSceneOptions & ~( Scene::ShowAxes | Scene::ShowGrid );
				}
				paintGL();

				fbo.release();

				rgbImg = fbo.toImage();
			} catch ( std::exception & e ) {
				err = e.what();
			}

			// Restore settings and return viewport to original size
			scene->options = savedSceneOptions;
			glClearColor( c.red(), c.green(), c.blue(), c.alpha() );
			if ( ss > 0 )
				resizeGL( int( p * w + 0.5 ), int( p * h + 0.5 ) );

			popGLContext( prvContext );

			if ( !err.empty() ) {
				QMessageBox::critical( nullptr, "NifSkope error", QString::fromStdString( err ) );
				return;
			}

			rgbImg.reinterpretAsFormat( !haveAlpha ? QImage::Format_RGB32 : QImage::Format_ARGB32 );
			int	imgWidth = rgbImg.bytesPerLine() >> 2;
			int	imgHeight = rgbImg.height();

			try {
				if ( imgFormat != 4 ) {
					QImageWriter writer( file->file() );

					// Set Compression for formats that can use it
					writer.setCompression( 1 );

					// Handle JPEG/WebP Quality
					writer.setFormat( screenshotImgFormats[imgFormat] + 1 );
					int	q = pixQuality->value();
					if ( q < 0 )
						q = 75;
					switch ( imgFormat ) {
					case 0:	// JPEG
						writer.setQuality( 50 + q / 2 );
						writer.setOptimizedWrite( true );
						writer.setProgressiveScanWrite( true );
						break;
					case 1:	// PNG
						writer.setCompression( q );
						break;
					case 2:	// WebP
						writer.setQuality( 50 + q / 2 );
						break;
					}

					if ( !writer.write( rgbImg ) )
						throw NifSkopeError( "%s", writer.errorString().toStdString().c_str() );

				} else {	// DDS
					DDSOutputFile	writer( file->file().toStdString().c_str(), imgWidth, imgHeight,
											DDSInputFile::pixelFormatRGBA32 );
					// TODO: portable handling of byte order
					writer.writeData( rgbImg.constBits(), size_t( rgbImg.sizeInBytes() ) );
				}

				dlg->accept();

			} catch ( std::exception & e ) {
				QMessageBox::critical( nullptr, "NifSkope error", tr( "Could not save %1: %2" ).arg( file->file() ).arg( e.what() ) );
			}
		}
	);
	connect( btnCancel, &QPushButton::clicked, dlg, &QDialog::reject );

	if ( dlg->exec() != QDialog::Accepted ) {
		return;
	}
}


/*
 * QWidget Event Handlers
 */

void GLView::contextMenuEvent( QContextMenuEvent * e )
{
	// The viewport has no context menu. Mouse clicks are interpreted in
	// mouseReleaseEvent (select vs place-gizmo, swappable buttons); the
	// keyboard menu key opens the W quick menu.
	if ( e->reason() == QContextMenuEvent::Keyboard ) {
		mouseButtonState = 0;
		showSpecialsMenu();
	}
	e->accept();
}

void GLView::dragEnterEvent( QDragEnterEvent * e )
{
	// Intercept NIF files
	if ( e->mimeData()->hasUrls() ) {
		QList<QUrl> urls = e->mimeData()->urls();
		for ( auto url : urls ) {
			if ( url.scheme() == "file" ) {
				QString fn = url.toLocalFile();
				QFileInfo finfo( fn );
				if ( finfo.exists() && NifSkope::fileExtensions().contains( finfo.suffix(), Qt::CaseInsensitive ) ) {
					draggedNifs << finfo.absoluteFilePath();
				}
			}
		}

		if ( !draggedNifs.isEmpty() ) {
			e->accept();
			return;
		}
	}

	auto md = e->mimeData();
	if ( md && md->hasUrls() && md->urls().count() == 1 ) {
		QUrl url = md->urls().first();

		if ( url.scheme() == "file" ) {
			QString fn = url.toLocalFile();

			if ( textures->canLoad( fn ) ) {
				fnDragTex = textures->stripPath( fn, model->getFolder() );
				e->accept();
				return;
			}
		}
	}

	e->ignore();
}

void GLView::dragLeaveEvent( QDragLeaveEvent * e )
{
	if ( !draggedNifs.isEmpty() ) {
		draggedNifs.clear();
		e->ignore();
		return;
	}

	if ( iDragTarget.isValid() ) {
		model->set<QString>( iDragTarget, fnDragTexOrg );
		iDragTarget = QModelIndex();
		fnDragTex = fnDragTexOrg = QString();
	}
}

void GLView::dragMoveEvent( QDragMoveEvent * e )
{
	if ( !draggedNifs.isEmpty() ) {
		e->accept();
		return;
	}

	if ( iDragTarget.isValid() ) {
		model->set<QString>( iDragTarget, fnDragTexOrg );
		iDragTarget  = QModelIndex();
		fnDragTexOrg = QString();
	}

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
	QModelIndex iObj = model->getBlockIndex( indexAt( e->posF() ), "NiAVObject" );
#else
	QModelIndex iObj = model->getBlockIndex( indexAt( e->position() ), "NiAVObject" );
#endif

	if ( iObj.isValid() ) {
		for ( const auto l : model->getChildLinks( model->getBlockNumber( iObj ) ) ) {
			QModelIndex iTxt = model->getBlockIndex( l, "NiTexturingProperty" );

			if ( iTxt.isValid() ) {
				QModelIndex iSrc = model->getBlockIndex( model->getLink( iTxt, "Base Texture/Source" ), "NiSourceTexture" );

				if ( iSrc.isValid() ) {
					iDragTarget = model->getIndex( iSrc, "File Name" );

					if ( iDragTarget.isValid() ) {
						fnDragTexOrg = model->get<QString>( iDragTarget );
						model->set<QString>( iDragTarget, fnDragTex );
						e->accept();
						return;
					}
				}
			}
		}
	}

	e->ignore();
}

void GLView::dropEvent( QDropEvent * e )
{
	if ( !draggedNifs.isEmpty() ) {
		auto ns = qobject_cast<NifSkope *>( graphicsView->parent() );
		if ( ns )
			ns->openFiles( draggedNifs );

		draggedNifs.clear();
		e->accept();
		return;
	}

	iDragTarget = QModelIndex();
	fnDragTex = fnDragTexOrg = QString();
	e->accept();
}

void GLView::focusOutEvent( QFocusEvent * )
{
	// the free camera holds a keyboard grab, so key events keep arriving even
	// without focus; zeroing the held WASD keys here would stop flight dead
	// whenever a tooltip or dock update steals focus for a moment
	if ( freeCamera )
		return;
	kbdState = 0;
	mouseButtonState = 0;
}

int GLView::convertKeyCode( int n ) const
{
	switch ( n ) {
	case Qt::Key_Up:
		return Key_RotateUp;
	case Qt::Key_Down:
		return Key_RotateDown;
	case Qt::Key_Left:
		return Key_RotateLeft;
	case Qt::Key_Right:
		return Key_RotateRight;
	case Qt::Key_PageUp:
		return Key_ZoomIn;
	case Qt::Key_PageDown:
		return Key_ZoomOut;
	case Qt::Key_A:
	case Qt::Key_D:
	case Qt::Key_W:
	case Qt::Key_S:
	case Qt::Key_Q:
	case Qt::Key_E:
		// keyboard camera movement only in free camera / walk mode, so the
		// letters stay free for the Blender-style transform shortcuts
		if ( !( freeCamera || view == ViewWalk ) )
			return -1;
		switch ( n ) {
		case Qt::Key_A:
			return Key_MoveLeft;
		case Qt::Key_D:
			return Key_MoveRight;
		case Qt::Key_W:
			return Key_MoveForward;
		case Qt::Key_S:
			return Key_MoveBack;
		case Qt::Key_Q:
			return Key_MoveDown;
		default:
			return Key_MoveUp;
		}
	case Qt::Key_M:
		return Key_Update;
	case Qt::Key_Space:
		return Key_MoveCam;
	case Qt::Key_Shift:
		return Key_Shift;
	case Qt::Key_J:
		return Key_RotateXY;
	case Qt::Key_K:
		return Key_RotateZ;
	case Qt::Key_I:
		return Key_Scale;
	case Qt::Key_O:
		return Key_TranslateXY;
	}
	return -1;
}

void GLView::keyPressEvent( QKeyEvent * event )
{
	const Qt::KeyboardModifiers mods = event->modifiers();
	// rebindable shortcuts: exact key+modifier matching against the registry
	const auto & shortcuts = ShortcutRegistry::get();
	if ( segmentPaintMode && shortcuts.matches( "viewport.toggle_edit_mode", event->key(), mods ) ) {
		setSegmentPaintMode( false );
		return;
	}
	if ( segmentPaintMode && event->key() == Qt::Key_Escape
		&& !boxSelecting && !circleSelecting ) {
		setSegmentPaintMode( false );
		return;
	}
	if ( vertexPaintMode && shortcuts.matches( "viewport.toggle_edit_mode", event->key(), mods ) ) {
		setVertexPaintMode( false );
		return;
	}
	if ( vertexPaintMode && event->key() == Qt::Key_Escape
		&& !boxSelecting && !circleSelecting ) {
		setVertexPaintMode( false );
		return;
	}
	if ( riggingWeightPaintMode && shortcuts.matches( "viewport.toggle_edit_mode", event->key(), mods ) ) {
		setRiggingWeightPaintMode( false );
		return;
	}
	if ( riggingWeightPaintMode && shortcuts.matches( "viewport.paint_fill", event->key(), mods ) ) {
		if ( !event->isAutoRepeat() )
			fillRiggingWeightSelection();
		return;
	}
	if ( segmentPaintMode && shortcuts.matches( "viewport.paint_fill", event->key(), mods ) ) {
		if ( !event->isAutoRepeat() )
			fillSegmentPaintSelection();
		return;
	}
	if ( riggingWeightPaintMode && event->key() == Qt::Key_Escape
		&& !boxSelecting && !circleSelecting ) {
		setRiggingWeightPaintMode( false );
		return;
	}
	// the knife is modal: Enter applies, Esc cancels, Z toggles cut-through,
	// other keys stay inert
	if ( knifeActive ) {
		if ( event->key() == Qt::Key_Escape )
			cancelKnife();
		else if ( event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter )
			knifeApply();
		else if ( event->key() == Qt::Key_Z )
			knifeToggleCutThrough();
		return;
	}

	// Escape disarms an armed / in-progress box select (Blender)
	if ( boxSelecting && event->key() == Qt::Key_Escape ) {
		boxSelecting = false;
		boxSelectDrag = false;
		unsetCursor();
		emit gizmoStatus( tr( "Box select cancelled" ) );
		update();
		return;
	}

	// Escape exits the circle-select brush
	if ( circleSelecting && event->key() == Qt::Key_Escape ) {
		circleSelecting = false;
		circlePainting = false;
		circleErasing = false;
		unsetCursor();
		emit gizmoStatus( QString() );
		update();
		return;
	}

	// modal transform gizmo
	if ( gizmoMode ) {
		// Blender: G/R/S during a gesture switches the transform mode,
		// resetting to the original values first; R while already rotating
		// toggles trackball rotation (Blender R,R)
		int nm = 0;
		if ( shortcuts.matches( "viewport.transform.move", event->key(), mods ) )
			nm = 1;
		else if ( shortcuts.matches( "viewport.transform.rotate", event->key(), mods ) )
			nm = 2;
		else if ( shortcuts.matches( "viewport.transform.scale", event->key(), mods ) )
			nm = 3;
		if ( nm ) {
			if ( nm != gizmoMode ) {
				bool wasElem = elemTransform;
				gizmoEnd( false );
				if ( wasElem )
					gizmoBeginElement( nm );
				else
					gizmoBegin( nm );
			} else if ( nm == 2 && !elemTransform ) {
				gizmoTrackball = !gizmoTrackball;
				gizmoAxis = 0;
				gizmoUpdate( mapFromGlobal( QCursor::pos() ), event->modifiers() );
			}
			return;
		}
		switch ( event->key() ) {
		case Qt::Key_X:
		case Qt::Key_Y:
		case Qt::Key_Z: {
			int ax = ( event->key() == Qt::Key_X ) ? 1 : ( event->key() == Qt::Key_Y ? 2 : 3 );
			if ( elemTransform ) {
				// element transforms: plain axis toggle in the global frame
				gizmoAxis = ( gizmoAxis == ax ) ? 0 : ax;
			} else if ( ( event->modifiers() & Qt::ShiftModifier ) && gizmoMode == 1 ) {
				// Shift+X/Y/Z: move in the plane excluding this axis (toggle)
				gizmoPlane = ( gizmoPlane == ax ) ? 0 : ax;
				gizmoAxis = 0;
				gizmoAxisLocal = false;
				gizmoBasisM = gizmoBasisOrig;
			} else if ( gizmoAxis == ax && !gizmoAxisLocal ) {
				// second tap: constrain to the node's LOCAL axis (Blender X,X)
				gizmoAxisLocal = true;
				gizmoBasisM = gizmoParentRot * gizmoOrigRot;
				gizmoPlane = 0;
			} else if ( gizmoAxis == ax && gizmoAxisLocal ) {
				// third tap: clear the constraint
				gizmoAxis = 0;
				gizmoAxisLocal = false;
				gizmoBasisM = gizmoBasisOrig;
			} else {
				gizmoAxis = ax;
				gizmoAxisLocal = false;
				gizmoPlane = 0;
				gizmoTrackball = false;
				gizmoBasisM = gizmoBasisOrig;
			}
			gizmoUpdate( mapFromGlobal( QCursor::pos() ), event->modifiers() );
			return;
		}
		case Qt::Key_Escape:
			gizmoEnd( false );
			return;
		case Qt::Key_Return:
		case Qt::Key_Enter:
			gizmoEnd( true );
			return;
		case Qt::Key_Tab:
			// next component for unconstrained moves (Blender: G 10 Tab 5 Tab 0)
			if ( gizmoMode == 1 && gizmoAxis == 0 ) {
				gizmoNumCur = ( gizmoNumCur + 1 ) % 3;
				gizmoUpdate( mapFromGlobal( QCursor::pos() ), event->modifiers() );
			}
			return;
		case Qt::Key_Backspace:
			if ( gizmoNumCur < gizmoNum.size() && !gizmoNum[gizmoNumCur].isEmpty() )
				gizmoNum[gizmoNumCur].chop( 1 );
			gizmoUpdate( mapFromGlobal( QCursor::pos() ), event->modifiers() );
			return;
		case Qt::Key_Minus:
			// Blender behavior: minus toggles the sign of the current entry
			if ( gizmoNumCur < gizmoNum.size() ) {
				QString & s = gizmoNum[gizmoNumCur];
				if ( s.startsWith( QLatin1Char( '-' ) ) )
					s.remove( 0, 1 );
				else
					s.prepend( QLatin1Char( '-' ) );
				gizmoUpdate( mapFromGlobal( QCursor::pos() ), event->modifiers() );
			}
			return;
		default:
			// Use the produced text as well as Qt's key code.  In particular,
			// keypad decimal keys and non-English keyboard layouts do not always
			// arrive as Key_Period/Key_Comma even though they type '.' or ','.
			QChar c;
			if ( event->key() >= Qt::Key_0 && event->key() <= Qt::Key_9 )
				c = QChar( ushort( '0' + ( event->key() - Qt::Key_0 ) ) );
			else if ( event->key() == Qt::Key_Period || event->key() == Qt::Key_Comma )
				c = QLatin1Char( '.' );
			else if ( !event->text().isEmpty() ) {
				const QChar typed = event->text().at( 0 );
				if ( typed.isDigit() )
					c = typed;
				else if ( typed == QLatin1Char( '.' ) || typed == QLatin1Char( ',' ) )
					c = QLatin1Char( '.' );
			}

			if ( !c.isNull() && gizmoNumCur < gizmoNum.size() ) {
				QString & s = gizmoNum[gizmoNumCur];
				if ( c != QLatin1Char( '.' ) || !s.contains( QLatin1Char( '.' ) ) )
					s.append( c );
				gizmoUpdate( mapFromGlobal( QCursor::pos() ), event->modifiers() );
			}
			return;
		}
	}

	// Free camera (Blender fly): only WASD/Q/E + Shift move; everything else is
	// locked out until you exit with Shift+F / Esc
	if ( freeCamera ) {
		if ( shortcuts.matches( "viewport.free_camera", event->key(), mods )
			|| event->key() == Qt::Key_Escape ) {
			setFreeCamera( false );
			return;
		}
		int fk = convertKeyCode( event->key() );
		if ( fk >= 0 )
			kbdState = kbdState | ( 1ULL << fk );
		return;
	}

	// swap which mouse button selects vs places the gizmo (unbound by default;
	// also settable from the Shortcuts settings page)
	if ( shortcuts.matches( "viewport.swap_mouse_select", event->key(), mods ) ) {
		setSelectWithRightMouse( !selectWithRightMouse );
		QSettings().setValue( "Shortcuts/MouseSelect",
			selectWithRightMouse ? QLatin1String( "right" ) : QLatin1String( "left" ) );
		emit gizmoStatus( selectWithRightMouse
			? tr( "Select with RIGHT mouse - LEFT places the gizmo" )
			: tr( "Select with LEFT mouse - RIGHT places the gizmo" ) );
		return;
	}

	// Tab toggles Blender-style Object / Edit mode (needs a mesh selected)
	if ( shortcuts.matches( "viewport.toggle_edit_mode", event->key(), mods ) ) {
		setEditMode( !editMode );
		return;
	}

	// Blender hierarchy shortcuts in object mode.
	if ( !editMode && shortcuts.matches( "viewport.parent_set", event->key(), mods ) ) {
		showParentMenu();
		return;
	}
	if ( !editMode && shortcuts.matches( "viewport.parent_clear", event->key(), mods ) ) {
		showClearParentMenu();
		return;
	}

	// edit-mode select-linked and linked flat faces
	if ( editMode ) {
		if ( shortcuts.matches( "viewport.select.linked", event->key(), mods ) ) {
			selectLinked( false );
			return;
		}
		if ( shortcuts.matches( "viewport.select.linked_angle", event->key(), mods ) ) {
			// Blender "Select Linked Flat Faces": grow across faces within the
			// sharpness angle; the redo panel lets you tweak the angle afterwards
			selectLinked( true, ( lastOpKind == 2 ) ? lastOpParam : 30.0f );
			return;
		}
		// Separate menu (Blender)
		if ( !riggingWeightPaintMode && !vertexPaintMode && !segmentPaintMode
			&& shortcuts.matches( "viewport.separate", event->key(), mods ) ) {
			showSeparateMenu();
			return;
		}
	}

	// duplicate the selection and start a move (object + edit mode)
	if ( !riggingWeightPaintMode && !vertexPaintMode && !segmentPaintMode
		&& shortcuts.matches( "viewport.duplicate", event->key(), mods ) && model ) {
		duplicateSelection();
		return;
	}

	// join the selected compatible meshes into the active one (object)
	if ( shortcuts.matches( "viewport.join", event->key(), mods ) && !editMode && model ) {
		joinSelectedObjects();
		return;
	}

	// Shift+Ctrl+Alt+C (Set Origin) is a window-level QAction in nifskope_ui.cpp

	// hide the selection / reveal everything: nodes in object mode,
	// picked vertices/edges/faces in edit mode (Blender H / Alt+H)
	if ( shortcuts.matches( "viewport.unhide_all", event->key(), mods ) ) {
		if ( editMode )
			unhideAllElements();
		else
			unhideAll();
		return;
	}
	if ( shortcuts.matches( "viewport.hide", event->key(), mods ) ) {
		if ( editMode )
			hideSelectedElements();
		else
			hideSelected();
		return;
	}

	if ( view != ViewWalk ) {
		// snap pie (object and edit mode, Blender Shift+S) - must beat the
		// plain S scale shortcut
		if ( !riggingWeightPaintMode && !vertexPaintMode && !segmentPaintMode
			&& shortcuts.matches( "viewport.snap", event->key(), mods ) && model ) {
			showSnapMenu();
			return;
		}

		// select all (toggle) / deselect all (Blender A / Alt+A)
		if ( shortcuts.matches( "viewport.select.all", event->key(), mods ) && !freeCamera && model ) {
			selectAll( 0 );
			return;
		}
		if ( shortcuts.matches( "viewport.select.none", event->key(), mods ) && !freeCamera && model ) {
			selectAll( 2 );
			return;
		}

		int m = 0;
		if ( shortcuts.matches( "viewport.transform.move", event->key(), mods ) )
			m = 1;
		else if ( shortcuts.matches( "viewport.transform.rotate", event->key(), mods ) )
			m = 2;
		else if ( shortcuts.matches( "viewport.transform.scale", event->key(), mods ) )
			m = 3;

		if ( m && !riggingWeightPaintMode && !vertexPaintMode && !segmentPaintMode ) {
			// with picked elements, G/R/S transforms them; otherwise the node
			if ( editMode && !pickedElems.isEmpty() && gizmoBeginElement( m ) )
				return;
			if ( gizmoBegin( m ) )
				return;
		}

		// delete (Blender X / Delete): in edit mode the verts/edges/faces menu;
		// in object mode the selected objects (with a confirmation). Delete key
		// is a fixed alternate for both.
		if ( !riggingWeightPaintMode && !vertexPaintMode && !segmentPaintMode
			&& ( shortcuts.matches( "viewport.delete", event->key(), mods )
				|| ( event->key() == Qt::Key_Delete
					&& !( mods & ( Qt::ControlModifier | Qt::AltModifier | Qt::ShiftModifier ) ) ) ) ) {
			if ( editMode && !pickedElems.isEmpty() ) {
				showDeleteMenu();
				return;
			}
			if ( !editMode && !objSelection.isEmpty() ) {
				deleteSelectedObjects();
				return;
			}
		}

		// element pick modes (Blender: 1/2/3); Shift extends the enabled set,
		// so it is stripped before matching the binding
		const Qt::KeyboardModifiers pmMods = mods & ~Qt::ShiftModifier;
		int pm = 0;
		if ( shortcuts.matches( "viewport.pick_vertex", event->key(), pmMods ) )
			pm = 1;	// vertex bit
		else if ( shortcuts.matches( "viewport.pick_edge", event->key(), pmMods ) )
			pm = 2;	// edge bit
		else if ( shortcuts.matches( "viewport.pick_face", event->key(), pmMods ) )
			pm = 4;	// face bit
		if ( pm && editMode ) {
			if ( riggingWeightPaintMode )
				setRiggingWeightPaintBrushEnabled( false );
			if ( vertexPaintMode )
				setVertexPaintBrushEnabled( false );
			if ( segmentPaintMode )
				setSegmentPaintBrushEnabled( false );
			// Shift extends the enabled modes (multi-mode), plain sets a single one
			if ( event->modifiers() & Qt::ShiftModifier )
				setPickMode( pickMode ^ pm );
			else
				setPickMode( pm );
			emit gizmoStatus( tr( "Edit Mode - select  (click = pick, Ctrl+click = add, Shift+click = path select, G/R/S move, X delete, Shift+S snap)" ) );
			return;
		}
		if ( shortcuts.matches( "viewport.snap_cursor_median", event->key(), mods ) ) {
			cursorPos = pickedElems.isEmpty() ? Vector3() : pickedMedian();
			update();
			return;
		}
		if ( shortcuts.matches( "viewport.knife", event->key(), mods ) && editMode ) {
			beginKnife();
			return;
		}
		if ( shortcuts.matches( "viewport.repeat_last", event->key(), mods ) ) {
			repeatLastOperator();
			return;
		}
		if ( shortcuts.matches( "viewport.panel_to_cursor", event->key(), mods ) ) {
			emit redoPanelToCursor();
			return;
		}
		if ( shortcuts.matches( "viewport.select.checker", event->key(), mods ) && editMode ) {
			checkerDeselect();
			return;
		}
		if ( shortcuts.matches( "viewport.split", event->key(), mods ) && editMode ) {
			splitSelection();
			return;
		}
		if ( shortcuts.matches( "viewport.rip", event->key(), mods ) && editMode ) {
			ripSelection();
			return;
		}
		if ( shortcuts.matches( "viewport.bevel", event->key(), mods ) && editMode ) {
			bevelSelection();
			return;
		}
		if ( shortcuts.matches( "viewport.select.circle", event->key(), mods ) ) {
			beginCircleSelect();	// cursor placement moved to plain RMB (Blender)
			update();
			return;
		}
		// grow / shrink the selection (Blender Select More/Less); the keyboard
		// produces either Plus or Equal for the same physical key
		const int mlKey = ( event->key() == Qt::Key_Plus ) ? Qt::Key_Equal : event->key();
		if ( shortcuts.matches( "viewport.select.more", mlKey, mods ) ) {
			selectMoreLess( true );
			return;
		}
		if ( shortcuts.matches( "viewport.select.less", mlKey, mods ) ) {
			selectMoreLess( false );
			return;
		}
		if ( event->key() == Qt::Key_Escape && !pickedElems.isEmpty() ) {
			pickedElems.clear();
			update();
			return;
		}
	}

	// circle select arms in object mode too (edit mode handles it above)
	if ( shortcuts.matches( "viewport.select.circle", event->key(), mods ) && !editMode && model ) {
		beginCircleSelect();
		return;
	}

	// frame the current selection (Blender Numpad-.)
	if ( shortcuts.matches( "viewport.frame_selection", event->key(), mods ) && model ) {
		frameSelected();
		return;
	}

	// Blender-like free camera toggle (only reached when entering; the lockout
	// block above handles exiting). Frontal light is now Ctrl+Shift+F.
	if ( shortcuts.matches( "viewport.free_camera", event->key(), mods ) ) {
		setFreeCamera( true );
		return;
	}

	int	k = convertKeyCode( event->key() );
	if ( k >= 0 ) {
		kbdState = kbdState | ( 1ULL << k );
		if ( k != Key_Shift )
			return;
	} else {
		switch ( event->key() ) {
		case Qt::Key_Escape:
			doCompile = 1;

			if ( view == ViewWalk )
				doCenter = true;

			update();
			break;
		case Qt::Key_F:
		case Qt::Key_L:
		case Qt::Key_T:
			if ( event->modifiers() & Qt::ShiftModifier ) {
				if ( event->key() == Qt::Key_F ) {
					if ( !frontalLight ) {
						frontalLight = true;
						emit frontalLightChanged( true );
						update();
					}
				} else {
					float	d = ( event->key() == Qt::Key_T ? 0.0f : 90.0f );
					declination = d;
					planarAngle = d;
					if ( frontalLight ) {
						frontalLight = false;
						emit frontalLightChanged( false );
					}
					update();
				}
				return;
			}
			break;
		default:
			break;
		}
	}
	event->ignore();
}

void GLView::keyReleaseEvent( QKeyEvent * event )
{
	int	k = convertKeyCode( event->key() );
	if ( k >= 0 ) {
		kbdState = kbdState & ~( 1ULL << k );
		if ( k != Key_Shift )
			return;
	}
	event->ignore();
}

void GLView::mouseDoubleClickEvent( QMouseEvent * )
{
	/*
	doCompile = 1;
	if ( ! aViewWalk->isChecked() )
	doCenter = true;
	update();
	*/
}

void GLView::mouseMoveEvent( QMouseEvent * event )
{
	// knife rubber band follows the cursor (also while MMB-orbiting)
	if ( knifeActive ) {
		knifeHoverValid = knifeProbe( getQMouseEventPosition( event ), knifeHoverPt );
		update();
	}

	if ( gizmoMode ) {
		// Blender: the drag is unbounded — the mouse is grabbed for the whole
		// gesture, and at a screen edge the cursor wraps to the opposite side.
		// The teleport jump is accumulated in gizmoWrapOffset so the position
		// fed to the gesture stays continuous.
		if ( QScreen * scr = screen() ) {
			QRect sg = scr->geometry();
			QPoint gl( int( event->globalPosition().x() ), int( event->globalPosition().y() ) );
			QPoint wrapped = gl;
			if ( gl.x() <= sg.left() )
				wrapped.setX( sg.right() - 1 );
			else if ( gl.x() >= sg.right() )
				wrapped.setX( sg.left() + 1 );
			if ( gl.y() <= sg.top() )
				wrapped.setY( sg.bottom() - 1 );
			else if ( gl.y() >= sg.bottom() )
				wrapped.setY( sg.top() + 1 );
			if ( wrapped != gl ) {
				gizmoWrapOffset += gl - wrapped;
				QCursor::setPos( scr, wrapped );
			}
		}
		auto gp = getQMouseEventPosition( event );
		gizmoUpdate( QPoint( (int)gp.x(), (int)gp.y() ) + gizmoWrapOffset, event->modifiers() );
		return;
	}

	// Fly look has priority over every modal viewport tool. Weight Paint stays
	// active underneath and resumes when fly mode exits.
	if ( freeCamera ) {
		if ( !isActive() )
			requestActivate();	// regain key focus so WASD keeps working
		QPointF c( width() * 0.5, height() * 0.5 );
		auto p = getQMouseEventPosition( event );
		float ldx = float( p.x() - c.x() );
		float ldy = float( p.y() - c.y() );
		if ( ldx != 0.0f || ldy != 0.0f ) {
			freeCameraLook( ldy * 0.2f, ldx * 0.2f );
			QCursor::setPos( mapToGlobal( QPoint( int( c.x() ), int( c.y() ) ) ) );
			update();
		}
		return;
	}

	// Rigging weight paint: one swept pass covers the full segment between mouse
	// events. This stays continuous without projecting every vertex once per
	// synthetic dab. MMB is allowed through to the shared orbit/pan path below.
	if ( riggingWeightPaintMode && riggingWeightPaintBrushEnabled ) {
		riggingWeightPaintPos = getQMouseEventPosition( event );
		if ( riggingWeightPaintStroke ) {
			auto now = std::chrono::steady_clock::now();
			if ( std::chrono::duration_cast<std::chrono::milliseconds>(
				now - riggingWeightPaintSampleTime ).count() < 8 ) {
				update();
				return;
			}
			applyRiggingWeightPaintBrush( riggingWeightPaintLastSample, riggingWeightPaintPos );
			riggingWeightPaintLastSample = riggingWeightPaintPos;
			riggingWeightPaintSampleTime = now;
			update();
			return;
		}
		update();
		if ( !( event->buttons() & Qt::MiddleButton ) )
			return;
	}

	if ( vertexPaintMode && vertexPaintBrushEnabled ) {
		vertexPaintPos = getQMouseEventPosition( event );
		if ( vertexPaintStroke ) {
			auto now = std::chrono::steady_clock::now();
			if ( std::chrono::duration_cast<std::chrono::milliseconds>(
				now - vertexPaintSampleTime ).count() < 8 ) {
				update();
				return;
			}
			applyVertexPaintBrush( vertexPaintLastSample, vertexPaintPos );
			vertexPaintLastSample = vertexPaintPos;
			vertexPaintSampleTime = now;
			update();
			return;
		}
		update();
		if ( !( event->buttons() & Qt::MiddleButton ) )
			return;
	}

	if ( segmentPaintMode && segmentPaintBrushEnabled ) {
		segmentPaintPos = getQMouseEventPosition( event );
		if ( segmentPaintStroke ) {
			auto now = std::chrono::steady_clock::now();
			if ( std::chrono::duration_cast<std::chrono::milliseconds>(
				now - segmentPaintSampleTime ).count() < 8 ) { update(); return; }
			applySegmentPaintBrush( segmentPaintLastSample, segmentPaintPos );
			segmentPaintLastSample = segmentPaintPos;
			segmentPaintSampleTime = now;
			update();
			return;
		}
		update();
		if ( !( event->buttons() & Qt::MiddleButton ) ) return;
	}

	// circle select: the brush follows the cursor and keeps painting while held
	if ( circleSelecting ) {
		circleSelectPos = getQMouseEventPosition( event );
		if ( circlePainting ) {
			lastCircleStroke.append( circleSelectPos );
			applyCircleSelect( circleSelectPos, false );
		} else if ( circleErasing ) {
			applyCircleSelect( circleSelectPos, true );
		}
		update();
		return;
	}

	// box select: grow the rubber-band rectangle
	if ( boxSelectDrag ) {
		boxSelectCur = getQMouseEventPosition( event ).toPoint();
		update();
		return;
	}

	// dragging the navigation gizmo ring orbits the view
	if ( navGizmoDrag ) {
		auto newPos = getQMouseEventPosition( event );
		mouseRot += Vector3( float( newPos.y() - lastPos.y() ) * 0.5f, 0.0f,
		                     float( newPos.x() - lastPos.x() ) * 0.5f );
		lastPos = newPos;
		view = ViewUser;
		return;
	}

	// hover highlighting of the navigation gizmo balls
	if ( !mouseButtonState && scene->hasOption( Scene::ShowAxes ) ) {
		int h = navGizmoHitTest( getQMouseEventPosition( event ) );
		int hoverBall = ( h >= 0 && h < 6 ) ? h : -1;
		if ( hoverBall != navGizmoHover ) {
			navGizmoHover = hoverBall;
			update();
		}
	}

	// hover highlighting of the gizmo handles
	if ( gizmoHandlesOn && !mouseButtonState && model ) {
		int h = gizmoHandleHitTest( getQMouseEventPosition( event ) );
		if ( h != gizmoHover ) {
			gizmoHover = h;
			update();
		}
	}

	auto	newPos = getQMouseEventPosition( event );
	float	dx = newPos.x() - lastPos.x();
	float	dy = newPos.y() - lastPos.y();
	Qt::MouseButtons	buttonMask = Qt::MouseButtons( mouseButtonState );

	if ( ( buttonMask | event->buttons() ) != buttonMask ) [[unlikely]] {
		// work around button events being lost after activating the context menu
		buttonMask = buttonMask | event->buttons();
		mouseButtonState = std::uint32_t( buttonMask );
		dx = 0.0f;
		dy = 0.0f;
	}

	// Blender-style viewport navigation: plain MMB orbits and Shift+MMB
	// pans. LMB is reserved for selection and tools; it never transforms the
	// camera. Explicit legacy item/light transforms keep their own LMB chords.
	if ( buttonMask & Qt::MiddleButton ) {
		if ( event->modifiers() & Qt::ShiftModifier ) {
			// Convert logical screen pixels to view-plane world units at the
			// model's current camera depth. The old axis/viewport-size scale was
			// tied only to mesh bounds and felt increasingly sluggish as the
			// camera moved away. This matches the projection math, so pan speed
			// tracks what is visible on screen like Blender's Shift+MMB pan.
			float hh = float( std::max( height(), 1 ) );
			float worldPerPixel;
			if ( perspectiveMode || view == ViewWalk ) {
				float depth = std::max( float( Dist * 2.0 ), 0.01f );
				if ( scene ) {
					float sceneDepth = -( viewTransform() * scene->bounds().center )[2];
					if ( sceneDepth > 0.01f )
						depth = sceneDepth;
				}
				float tanHalfFov = float( std::tan( ( cfg.fov / Zoom ) / 360.0 * M_PI ) );
				worldPerPixel = 2.0f * depth * tanHalfFov / hh;
			} else {
				worldPerPixel = 2.0f * float( Dist / Zoom ) / hh;
			}
			mouseMov += Vector3( dx * worldPerPixel, -dy * worldPerPixel, 0.0f );
		} else {
			mouseRot += Vector3( dy * 0.5f, 0.0f, dx * 0.5f );
		}
	} else if ( buttonMask & Qt::LeftButton ) {
		if ( kbd( Key_RotateXY ) || kbd( Key_RotateZ ) || kbd( Key_Scale ) || kbd( Key_TranslateXY ) )
			transformItem( dx, dy );
		else if ( !frontalLight && ( event->modifiers() & Qt::ShiftModifier ) )
			rotateLight( dy * 0.5f, dx * 0.5f );
	} else if ( buttonMask & Qt::RightButton ) {
		setDistance( Dist - (dx + dy) * (axis / (qMax( width(), height() ) + 1)) );
	}

	lastPos = newPos;
}

void GLView::mousePressEvent( QMouseEvent * event )
{
	if ( gizmoMode ) {
		// MMB during a modal transform: smart axis lock - constrain to the
		// axis whose screen direction best matches the drag so far (Blender)
		if ( event->button() == Qt::MiddleButton && !elemTransform ) {
			QPointF p = getQMouseEventPosition( event );
			QPointF d = p - QPointF( gizmoStartPos );
			QPointF sp;
			if ( std::hypot( d.x(), d.y() ) > 3.0 && worldToScreen( gizmoPivotWorld, sp ) ) {
				int best = 0;
				float bestDot = -1.0f;
				for ( int i = 0; i < 3; i++ ) {
					Vector3 u;
					u[i] = 1.0f;
					QPointF ap;
					if ( !worldToScreen( gizmoPivotWorld + ( gizmoBasisM * u ) * float( Dist * 0.25 ), ap ) )
						continue;
					QPointF av = ap - sp;
					float al = float( std::hypot( av.x(), av.y() ) );
					float dl = float( std::hypot( d.x(), d.y() ) );
					if ( al < 1.0e-3f || dl < 1.0e-3f )
						continue;
					float dot = std::fabs( float( av.x() * d.x() + av.y() * d.y() ) ) / ( al * dl );
					if ( dot > bestDot ) {
						bestDot = dot;
						best = i + 1;
					}
				}
				if ( best ) {
					gizmoAxis = best;
					gizmoPlane = 0;
					gizmoAxisLocal = false;
					gizmoBasisM = gizmoBasisOrig;
					gizmoUpdate( QPoint( int( p.x() ), int( p.y() ) ), event->modifiers() );
				}
			}
			gizmoSwallowClick = true;
			return;
		}
		gizmoEnd( event->button() == Qt::LeftButton );
		gizmoSwallowClick = true;
		return;
	}

	// Fly mode owns the mouse even when Weight Paint or a selection gadget is
	// active underneath. A click confirms/exits fly mode, matching Blender.
	if ( freeCamera ) {
		setFreeCamera( false );
		lastPos = getQMouseEventPosition( event );
		return;
	}

	if ( riggingWeightPaintMode && riggingWeightPaintBrushEnabled ) {
		riggingWeightPaintPos = getQMouseEventPosition( event );
		if ( event->button() == Qt::LeftButton ) {
			riggingWeightPaintStroke = true;
			riggingWeightPaintLastSample = riggingWeightPaintPos;
			riggingWeightPaintProjectionValid = false;
			riggingWeightPaintSampleTime = std::chrono::steady_clock::now();
			emit riggingWeightStrokeBegan();
			applyRiggingWeightPaintBrush( riggingWeightPaintPos, riggingWeightPaintPos );
			mouseButtonState |= std::uint32_t( event->button() );
			update();
			return;
		}
		// MMB and Shift+MMB continue into the normal orbit/pan handler. RMB
		// likewise continues to the normal click path so release drops the
		// gizmo / 3D cursor without tearing down the mode.
	}

	if ( vertexPaintMode && vertexPaintBrushEnabled ) {
		vertexPaintPos = getQMouseEventPosition( event );
		if ( event->button() == Qt::LeftButton ) {
			vertexPaintStroke = true;
			vertexPaintLastSample = vertexPaintPos;
			vertexPaintProjectionValid = false;
			vertexPaintSampleTime = std::chrono::steady_clock::now();
			emit vertexPaintStrokeBegan();
			applyVertexPaintBrush( vertexPaintPos, vertexPaintPos );
			mouseButtonState |= std::uint32_t( event->button() );
			update();
			return;
		}
	}

	if ( segmentPaintMode && segmentPaintBrushEnabled ) {
		segmentPaintPos = getQMouseEventPosition( event );
		if ( event->button() == Qt::LeftButton ) {
			segmentPaintStroke = true;
			segmentPaintLastSample = segmentPaintPos;
			segmentPaintProjectionValid = false;
			segmentPaintSampleTime = std::chrono::steady_clock::now();
			emit segmentPaintStrokeBegan();
			applySegmentPaintBrush( segmentPaintPos, segmentPaintPos );
			mouseButtonState |= std::uint32_t( event->button() );
			update();
			return;
		}
	}

	// knife armed (K): LMB places cut points, RMB cancels, MMB orbits
	if ( knifeActive ) {
		if ( event->button() == Qt::LeftButton ) {
			knifeAddPoint( getQMouseEventPosition( event ) );
			gizmoSwallowClick = true;
			return;
		}
		if ( event->button() == Qt::RightButton ) {
			cancelKnife();
			// keep this click from dropping the gizmo on release
			pressPos = QPointF( -10000.0, -10000.0 );
			return;
		}
		// MMB continues into the normal orbit handler
	}

	// circle select armed (C): LMB paints select, MMB paints deselect,
	// RMB / Esc exits the gadget
	if ( circleSelecting ) {
		auto cp = getQMouseEventPosition( event );
		circleSelectPos = cp;
		if ( event->button() == Qt::LeftButton ) {
			recordSelection();	// one undo step per paint stroke
			circlePainting = true;
			lastCircleStroke.clear();	// record the stroke for the redo panel
			lastCircleStroke.append( cp );
			lastCircleStrokeRad = circleSelectRadius;
			applyCircleSelect( cp, false );
		} else if ( event->button() == Qt::MiddleButton ) {
			recordSelection();
			circleErasing = true;
			applyCircleSelect( cp, true );
		} else if ( event->button() == Qt::RightButton ) {
			circleSelecting = false;
			circlePainting = circleErasing = false;
			unsetCursor();
			emit gizmoStatus( QString() );
			// keep this click from dropping the gizmo on release
			lastPos = cp;
			pressPos = QPointF( -10000.0, -10000.0 );
			update();
			return;
		}
		mouseButtonState |= std::uint32_t( event->button() );
		update();
		return;
	}

	// box select armed (B): left-drag rubber-bands a rectangle, right-click cancels
	if ( boxSelecting ) {
		if ( event->button() == Qt::LeftButton ) {
			boxSelectDrag = true;
			boxSelectStart = boxSelectCur = getQMouseEventPosition( event ).toPoint();
			mouseButtonState |= std::uint32_t( event->button() );
		} else if ( event->button() == Qt::RightButton ) {
			boxSelecting = false;
			unsetCursor();
			emit gizmoStatus( tr( "Box select cancelled" ) );
			// keep this click from dropping the gizmo on release
			pressPos = QPointF( -10000.0, -10000.0 );
		}
		update();
		return;
	}

	// Blender-style navigation gizmo: click an axis ball to snap the view,
	// or click/drag the surrounding ring to orbit. Only plain left-clicks are
	// intercepted so Shift/Ctrl multi-select clicks always reach the viewport.
	if ( event->button() == Qt::LeftButton && scene->hasOption( Scene::ShowAxes )
		&& !( event->modifiers() & ( Qt::ShiftModifier | Qt::ControlModifier | Qt::AltModifier ) ) ) {
		int h = navGizmoHitTest( getQMouseEventPosition( event ) );
		if ( h >= 0 && h < 6 ) {
			snapToAxis( h );
			gizmoSwallowClick = true;
			mouseButtonState |= std::uint32_t( event->button() );
			return;
		} else if ( h == 6 ) {
			navGizmoDrag = true;
			mouseButtonState |= std::uint32_t( event->button() );
			lastPos = getQMouseEventPosition( event );
			pressPos = lastPos;
			return;
		}
	}

	// grabbing a gizmo handle starts a constrained drag; releasing commits.
	// checked before element picking so the handles stay usable in edit mode.
	// Ctrl is allowed: holding it from the start means "drag with snapping"
	// (Blender), and it previously blocked the drag from ever starting.
	if ( gizmoHandlesOn && event->button() == Qt::LeftButton && view != ViewWalk
		&& !kbdState && !( event->modifiers() & ( Qt::AltModifier | Qt::ShiftModifier ) ) ) {
		auto p = getQMouseEventPosition( event );
		int h = gizmoHandleHitTest( p );
		if ( h ) {
			// 1-3 arrows, 4 center, 5-7 rings, 8-10 boxes, 11 view ring, 12-14 planes
			int mode = ( h == 11 ) ? 2 : ( h >= 12 ? 1 : ( h >= 8 ? 3 : ( h >= 5 ? 2 : 1 ) ) );
			bool started = ( editMode && !pickedElems.isEmpty() ) ? gizmoBeginElement( mode ) : gizmoBegin( mode );
			if ( started ) {
				gizmoAxis = ( h == 4 || h == 11 || h >= 12 ) ? 0 : ( h >= 8 ? h - 7 : ( h >= 5 ? h - 4 : h ) );
				if ( h >= 12 )
					gizmoPlane = h - 11;
				gizmoStartPos = QPoint( int( p.x() ), int( p.y() ) );
				gizmoHandleDrag = true;
				mouseButtonState |= std::uint32_t( event->button() );
				gizmoUpdate( gizmoStartPos, event->modifiers() );
				return;
			}
		}
	}

	// NOTE: edit-mode element picking is deferred to mouseReleaseEvent and only
	// runs when the click barely moved, so orbiting the camera with LMB (incl.
	// Ctrl/Shift held) no longer selects or deselects geometry.

	mouseButtonState |= std::uint32_t( event->button() );
	if ( event->button() == Qt::ForwardButton || event->button() == Qt::BackButton ) {
		event->ignore();
		return;
	}

	lastPos = getQMouseEventPosition( event );

	pressPos = lastPos;
}

void GLView::mouseReleaseEvent( QMouseEvent * event )
{
	if ( riggingWeightPaintMode && riggingWeightPaintBrushEnabled
		&& riggingWeightPaintStroke && event->button() == Qt::LeftButton ) {
		QPointF releasePos = getQMouseEventPosition( event );
		// Most releases arrive at the same position as the last move event. Avoid
		// projecting/sampling the full brush a second time there; it was pure work
		// in normal mode and an unintended extra dab with Accumulate enabled.
		QPointF releaseDelta = releasePos - riggingWeightPaintLastSample;
		if ( std::hypot( releaseDelta.x(), releaseDelta.y() ) >= 0.5 )
			applyRiggingWeightPaintBrush( riggingWeightPaintLastSample, releasePos );
		riggingWeightPaintLastSample = releasePos;
		riggingWeightPaintStroke = false;
		riggingWeightPaintProjectionValid = false;
		riggingWeightPaintScreen.clear();
		riggingWeightPaintCandidates.clear();
		mouseButtonState &= ~( std::uint32_t( event->button() ) );
		emit riggingWeightStrokeEnded( true );
		update();
		return;
	}
	if ( vertexPaintMode && vertexPaintBrushEnabled
		&& vertexPaintStroke && event->button() == Qt::LeftButton ) {
		QPointF releasePos = getQMouseEventPosition( event );
		QPointF releaseDelta = releasePos - vertexPaintLastSample;
		if ( std::hypot( releaseDelta.x(), releaseDelta.y() ) >= 0.5 )
			applyVertexPaintBrush( vertexPaintLastSample, releasePos );
		vertexPaintLastSample = releasePos;
		vertexPaintStroke = false;
		vertexPaintProjectionValid = false;
		vertexPaintScreen.clear();
		vertexPaintCandidates.clear();
		mouseButtonState &= ~( std::uint32_t( event->button() ) );
		emit vertexPaintStrokeEnded( true );
		update();
		return;
	}
	if ( segmentPaintMode && segmentPaintBrushEnabled
		&& segmentPaintStroke && event->button() == Qt::LeftButton ) {
		QPointF releasePos = getQMouseEventPosition( event );
		QPointF delta = releasePos - segmentPaintLastSample;
		if ( std::hypot( delta.x(), delta.y() ) >= 0.5 )
			applySegmentPaintBrush( segmentPaintLastSample, releasePos );
		segmentPaintLastSample = releasePos;
		segmentPaintStroke = false;
		segmentPaintProjectionValid = false;
		segmentPaintScreen.clear();
		segmentPaintCandidates.clear();
		mouseButtonState &= ~( std::uint32_t( event->button() ) );
		emit segmentPaintStrokeEnded( true );
		update();
		return;
	}
	// circle select: end the paint stroke but stay armed
	if ( circleSelecting && ( circlePainting || circleErasing ) ) {
		bool wasSelect = circlePainting;
		circlePainting = false;
		circleErasing = false;
		mouseButtonState &= ~( std::uint32_t( event->button() ) );
		if ( wasSelect && !lastCircleStroke.isEmpty() ) {
			lastGestureKind = 2;	// arm the gesture redo panel's Deselect
			emit circleSelectApplied();
		}
		update();
		return;
	}

	// finish a box select: apply the rectangle, then disarm
	if ( boxSelectDrag ) {
		boxSelectDrag = false;
		boxSelecting = false;
		mouseButtonState &= ~( std::uint32_t( event->button() ) );
		unsetCursor();
		QRect r = QRect( boxSelectStart, getQMouseEventPosition( event ).toPoint() ).normalized();
		if ( r.width() >= 2 && r.height() >= 2 )
			applyBoxSelect( r, event->modifiers() );
		else
			emit gizmoStatus( tr( "Box select cancelled" ) );
		update();
		return;
	}

	if ( navGizmoDrag ) {
		navGizmoDrag = false;
		mouseButtonState &= ~( std::uint32_t( event->button() ) );
		return;
	}

	if ( gizmoHandleDrag ) {
		gizmoHandleDrag = false;
		mouseButtonState &= ~( std::uint32_t( event->button() ) );
		gizmoEnd( true );
		return;
	}

	if ( gizmoSwallowClick ) {
		gizmoSwallowClick = false;
		mouseButtonState &= ~( std::uint32_t( event->button() ) );
		return;
	}

	mouseButtonState &= ~( std::uint32_t( event->button() ) );

	auto	evtPos = getQMouseEventPosition( event );
#ifdef Q_OS_LINUX
	bool	isColorPicker = bool( event->modifiers() & ( Qt::AltModifier | Qt::ControlModifier ) );
#else
	bool	isColorPicker = bool( event->modifiers() & Qt::AltModifier );
#endif
	// in edit mode Alt+click / Ctrl+click on the select button are selection
	// clicks (edge loop / extend, Blender), never the background color picker
	if ( editMode && pickMode && event->button() == selectMouseButton() )
		isColorPicker = false;
	if ( model && ( pressPos - evtPos ).manhattanLength() <= 3 ) {
		if ( event->button() == Qt::ForwardButton || event->button() == Qt::BackButton
			|| event->button() == Qt::MiddleButton ) {
			event->ignore();
			return;
		}

		// edit-mode element picking happens here (on a click, not a drag) so
		// camera orbiting never changes the selection: click = pick,
		// Ctrl+click = extend/toggle, Shift+click = shortest-path,
		// Alt+click = edge loop (Shift+Alt extends it) - all Blender
		if ( editMode && pickMode && event->button() == selectMouseButton() && !isColorPicker ) {
			auto p = evtPos;
			if ( event->modifiers() & Qt::AltModifier ) {
				if ( !selectEdgeLoop( p, bool( event->modifiers() & Qt::ShiftModifier ) ) )
					emit gizmoStatus( tr( "Edge loop: no edge under the cursor" ) );
			} else if ( event->modifiers() & Qt::ShiftModifier ) {
				pickPathSelect( p );
			} else {
				pickElementAt( p, bool( event->modifiers() & Qt::ControlModifier ) );
			}
			update();
			return;
		}

		if ( !isColorPicker && event->button() == selectMouseButton() ) {
			bool shift = bool( event->modifiers() & Qt::ShiftModifier );
			// in object mode Shift OR Ctrl extends the multi-selection (Blender
			// accepts both); Shift alone also drives indexAt()'s vertex cycling
			bool extend = shift || bool( event->modifiers() & Qt::ControlModifier );
			QModelIndex idx = indexAt( evtPos, editMode ? shift : false );
			scene->currentBlock = model->getBlockIndex( idx );
			scene->currentIndex = idx.sibling( idx.row(), 0 );

			if ( !editMode ) {
				// resolve the nearest NiAVObject and update the object selection
				int av = idx.isValid() ? model->getBlockNumber( scene->currentBlock ) : -1;
				while ( av >= 0 && !model->blockInherits( model->getBlockIndex( av ), "NiAVObject" ) )
					av = model->getParent( av );
				objectSelectClick( av, extend );
			}

			if ( idx.isValid() ) {
#if 0
				// this makes vertex selection slow, and may no longer be needed with newer Qt versions
				emit clicked( QModelIndex() ); // HACK: To get Block Details to update
#endif
				emit clicked( idx );
			}

		} else if ( isColorPicker ) {
			// Color Picker / Eyedrop tool
			auto	prvContext = pushGLContext();
			{
				QOpenGLFramebufferObjectFormat fboFmt;
				fboFmt.setTextureTarget( GL_TEXTURE_2D );
				fboFmt.setInternalTextureFormat( GL_SRGB8 );
				fboFmt.setMipmap( false );
				fboFmt.setAttachment( QOpenGLFramebufferObject::Attachment::Depth );

				QOpenGLFramebufferObject fbo( pixelWidth, pixelHeight, fboFmt );
				fbo.bind();

				paintGL();

				fbo.release();

				QImage img( fbo.toImage() );

				QColor what = QColor( img.pixel( ( evtPos * devicePixelRatioF() ).toPoint() ) );

				glClearColor( what.redF(), what.greenF(), what.blueF(), what.alphaF() );
				// qDebug() << what;
			}
			popGLContext( prvContext );
		}

		update();
	}

	// a plain click (not a drag) on the cursor-place button drops the gizmo /
	// 3D cursor on the surface under the mouse (select/place buttons swappable)
	if ( event->button() == cursorPlaceButton() && !isColorPicker && model
		&& ( pressPos - evtPos ).manhattanLength() <= 10 ) {
		placeCursor( evtPos );
		update();
	}
}

void GLView::wheelEvent( QWheelEvent * event )
{
	// Fly mode owns the wheel even if a paint/selection tool remains active.
	if ( freeCamera ) {
		freeCamSpeed *= ( event->angleDelta().y() > 0 ) ? 1.25f : 0.8f;
		freeCamSpeed = std::min( std::max( freeCamSpeed, 0.05f ), 50.0f );
		emit gizmoStatus( tr( "Fly speed: %1x (scroll to adjust)" ).arg( double( freeCamSpeed ), 0, 'f', 2 ) );
		return;
	}

	// circle select: the scroll wheel resizes the brush (Blender)
	if ( circleSelecting ) {
		float d = ( event->angleDelta().y() > 0 ) ? 1.15f : ( 1.0f / 1.15f );
		circleSelectRadius = std::min( std::max( circleSelectRadius * d, 4.0f ), 400.0f );
		update();
		return;
	}

	if ( view == ViewWalk ) {
		mouseMov += Vector3( 0, 0, double( event->angleDelta().y() ) / 4.0 ) * scale();
	} else {
		if (event->angleDelta().y() < 0)
			setDistance( Dist * Settings::zoomOutScale );
		else
			setDistance( Dist * Settings::zoomInScale );
	}
}

void GLView::transformItem( float dx, float dy )
{
	if ( !( std::max( std::fabs( dx ), std::fabs( dy ) ) > 0.01f ) )
		return;
	if ( !( scene->nifModel && scene->renderer && scene->currentBlock.isValid() ) )
		return;
	NifModel *	nif = const_cast< NifModel * >( scene->nifModel );
	QModelIndex	iBlock = scene->currentBlock;
	if ( !nif->blockInherits( iBlock, { "BSGeometry", "BSTriShape", "NiNode", "NiTriBasedGeom" } ) )
		return;
	Node *	node = scene->getNode( nif, iBlock );
	if ( !node )
		return;
	Shape *	shape = dynamic_cast< Shape * >( node );
	if ( shape && shape->iSkin.isValid() )
		return;
	dx = dx * 2.0f / float( width() );
	dy = dy * -2.0f / float( height() );
	if ( kbd( Key_TranslateXY ) ) {
		glProjection( 0, 0 );
		Matrix4	m( &( scene->renderer->globalUniforms->projectionMatrix[0][0] ) );
		m = m * scene->view;
		if ( auto p = node->parentNode(); p )
			m = m * p->worldTrans();
		FloatVector4	v0( 0.0f );
		if ( shape && !shape->verts.isEmpty() ) {
			const Vector3 *	vp = shape->verts.constData();
			int	n = int( shape->verts.size() );
			for ( int i = 0; i < n; i++, vp++ )
				v0 += FloatVector4::convertVector3( vp->data() );
			v0 = v0 / float( n );
		}
		v0[3] = 1.0f;
		v0 = node->localTrans().toMatrix4() * v0;
		FloatVector4	v( v0 );
		v = m * v;
		float	w = v[3];
		if ( w > 0.000001f ) {
			v = v / w;
			if ( v[2] >= -1.0f && v[2] <= 1.0f ) {
				v += FloatVector4( dx, dy, 0.0f, 0.0f );
				v = m.inverted() * ( v * w );
				if ( auto i = nif->getIndex( iBlock, "Translation" ); i.isValid() )
					nif->set<Vector3>( i, nif->get<Vector3>( i ) + Vector3( v - v0 ) );
			}
		}
	}
	if ( kbd( Key_Scale ) ) {
		if ( auto i = nif->getIndex( iBlock, "Scale" ); i.isValid() )
			nif->set<float>( i, nif->get<float>( i ) * float( std::exp2( dx + dy ) ) );
	}
	if ( kbd( Key_RotateXY ) || kbd( Key_RotateZ ) ) {
		if ( auto i = nif->getIndex( iBlock, "Rotation" ); i.isValid() ) {
			Matrix	m0 = scene->view.rotation;
			if ( auto p = node->parentNode(); p )
				m0 = m0 * p->worldTrans().rotation;
			Matrix	m = m0 * nif->get<Matrix>( i );
			Matrix	r;
			float	x = ( kbd( Key_RotateXY ) ? dy * -3.14159265f : 0.0f );
			float	y = ( kbd( Key_RotateXY ) ? dx * 3.14159265f : 0.0f );
			float	z = ( kbd( Key_RotateZ ) ? ( dx + dy ) * -3.14159265f : 0.0f );
			r.fromEuler( x, y, z );
			m = r * m;
			m = m0.inverted() * m;
			m.toEuler( x, y, z );
			m.fromEuler( x, y, z );
			nif->set<Matrix>( i, m );
		}
	}
}

const char * GLView::getGLErrorString( int err )
{
	switch ( err ) {
	case GL_NO_ERROR:
		return "No Error";
	case GL_INVALID_ENUM:
		return "GL_INVALID_ENUM";
	case GL_INVALID_VALUE:
		return "GL_INVALID_VALUE";
	case GL_INVALID_OPERATION:
		return "GL_INVALID_OPERATION";
	case GL_INVALID_FRAMEBUFFER_OPERATION:
		return "GL_INVALID_FRAMEBUFFER_OPERATION";
	case GL_OUT_OF_MEMORY:
		return "GL_OUT_OF_MEMORY";
	case GL_STACK_UNDERFLOW:
		return "GL_STACK_UNDERFLOW";
	case GL_STACK_OVERFLOW:
		return "GL_STACK_OVERFLOW";
	}
	return "Unknown OpenGL Error";
}
