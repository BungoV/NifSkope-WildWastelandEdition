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
#include "ui/settingsdialog.h"
#include "ui/widgets/fileselect.h"
#include "fp32vec4.hpp"
#include "ui/widgets/filebrowser.h"
#include "qt5compat.hpp"
#include "spells/blocks.h"	// blockLink for Separate / Duplicate

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

GLView::GLView( QWindow * p )
	: QOpenGLWindow( QOpenGLWindow::NoPartialUpdate, p )
{
	QSettings settings;
	int	aa = settings.value( "Settings/Render/General/Msaa Samples", 2 ).toInt();
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
	contextMenuShiftModifier = false;
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
	cfg.fov = settings.value( "General/Camera/Field Of View" ).toFloat();
	cfg.moveSpd = settings.value( "General/Camera/Movement Speed" ).toFloat();
	cfg.rotSpd = settings.value( "General/Camera/Rotation Speed" ).toFloat();
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
			for ( int ti = 0; ti < s->triangles.size(); ti++ ) {
				if ( hidT.contains( ti ) )
					continue;
				const Triangle & t = s->triangles.at( ti );
				if ( t[0] >= nv || t[1] >= nv || t[2] >= nv )
					continue;
				soup << s->verts[t[0]] << s->verts[t[1]] << s->verts[t[2]];
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
	bool showGizmo = editMode ? ( !pickedElems.isEmpty() || elemTransform ) : scene->currentBlock.isValid();
	if ( model && showGizmo ) {
		int gb;
		if ( editMode && editShapeBlock >= 0 ) {
			gb = editShapeBlock;
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

			if ( gizmoMode || gizmoHandlesOn ) {
				// Blender-style handles: arrows with solid cone tips (move),
				// rings (rotate), solid boxes (scale), center circle (view-
				// plane move); modelview is already the gizmo basis at the
				// pivot. Colours match Blender: X #FF3352, Y #8BDC00, Z #2890FF.
				// During a modal G/R/S only the relevant sub-gizmo is drawn,
				// and only the constrained axis if one is locked in. The top-
				// bar toggle only hides the combined (idle) gizmo.
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
	if ( model && editMode && !editShapeBlocks.isEmpty() ) {
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
				Vector3 w = wt * s->verts.at( i );
				wv[i] = eye + ( w - eye ) * 0.997f;
			}

			// unique edge list (hidden triangles excluded, Blender H)
			const QSet<int> hiddenT = editHiddenTris.value( wb );
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
						edges.append( qMakePair( a, b ) );
					}
				}
			}

			const bool vertMode = bool( pickMode & 1 );
			const QSet<int> & sv = selVerts[wb];
			const QSet<quint64> & se = selEdges[wb];

			// Faces to fill (Blender fills a face when it is face-selected OR
			// all of its verts / all of its edges are selected in vert/edge mode)
			QSet<int> filledTris;
			if ( selFaces.contains( wb ) )
				filledTris = selFaces.value( wb );
			if ( !sv.isEmpty() || !se.isEmpty() ) {
				for ( int ti = 0; ti < s->triangles.size(); ti++ ) {
					if ( hiddenT.contains( ti ) )
						continue;
					const Triangle & t = s->triangles.at( ti );
					if ( t[0] >= nv || t[1] >= nv || t[2] >= nv )
						continue;
					bool allV = !sv.isEmpty() && sv.contains( t[0] ) && sv.contains( t[1] ) && sv.contains( t[2] );
					bool allE = !se.isEmpty() && se.contains( edgeKey( t[0], t[1] ) )
						&& se.contains( edgeKey( t[1], t[2] ) ) && se.contains( edgeKey( t[2], t[0] ) );
					if ( allV || allE )
						filledTris.insert( ti );
				}
			}

			// Selection (fills, selected edges/verts, outlines) is drawn with
			// the depth test OFF so nearby unconnected geometry can never
			// occlude it - the edit cage stays on top, Blender-style. The plain
			// black wireframe + unselected dots keep the depth test.
			QVector<Vector3> foutline;
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
					foutline << wv[t[0]] << wv[t[1]] << wv[t[1]] << wv[t[2]] << wv[t[2]] << wv[t[0]];
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
				if ( pickMode & ( 2 | 4 ) )
					scene->setGLColor( 1.0f, 1.0f, 1.0f, 0.95f );
				else
					scene->setGLColor( colSel );
				scene->drawLines( foutline.constData(), size_t( foutline.size() ), nullptr );
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
				Vector3 v = wt * s->verts[pe.e0];
				scene->setGLColor( 0.0f, 0.0f, 0.0f, 1.0f );	// contrast halo
				scene->drawSphereSimple( v, ms * 0.62f, 16, 2 );
				scene->setGLColor( 1.0f, 0.5f, 0.0f, 1.0f );	// orange verts
				scene->drawSphereSimple( v, ms * 0.45f, 16, 2 );
			} else if ( pe.type == 2 ) {
				if ( pe.e0 >= nv || pe.e1 >= nv )
					continue;
				Vector3 a = wt * s->verts[pe.e0], b = wt * s->verts[pe.e1];
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
				Vector3 tri[3] = { wt * s->verts[t[0]], wt * s->verts[t[1]], wt * s->verts[t[2]] };
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
	if ( scene->hasOption( Scene::ShowAxes ) || ( model && showCursor ) || freeCamera || drawSnapMarker ) {
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
		painter.end();

		// QPainter changes GL state behind the renderer's back (bound program,
		// blending, scissor); reset everything that would corrupt the next
		// selection/picking render, which reuses this context.
		if ( QOpenGLContext * glCtx = QOpenGLContext::currentContext() ) {
			QOpenGLFunctions * f = glCtx->functions();
			f->glUseProgram( 0 );
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
	perspectiveMode = isPersp;
	update();
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
		unsetCursor();
		emit gizmoStatus( QString() );
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

bool GLView::gizmoBegin( int mode )
{
	if ( !model || !scene->currentBlock.isValid() )
		return false;

	QModelIndex iBlock = model->getBlockIndex( QModelIndex( scene->currentBlock ) );
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
				elemSnapped = true;
				snapIndicator = true;
				snapIndicatorPos = target;

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
		if ( snap && !elemSnapped && gizmoSnapStep > 0 ) {
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
	emit gizmoStatus( QString() );

	if ( !model || !iBlock.isValid() )
		return;

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

bool GLView::gizmoReapply( const Vector3 & param, int axisOverride, int orientOverride )
{
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
			if ( tlRayTri( lo, ld, s->verts.at( tri[0] ), s->verts.at( tri[1] ), s->verts.at( tri[2] ), t ) ) {
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
		// all shapes in the selected object's subtree form one silhouette;
		// expanded triangle soup per shape (local space, drawn with the
		// shape's own modelview)
		QVector<QPair<Shape *, QVector<Vector3>>> shs;
		for ( Shape * s : scene->shapes ) {
			if ( !s || s->isHidden() || s->verts.isEmpty() || s->triangles.isEmpty() )
				continue;
			int p = s->id();
			while ( p >= 0 && p != b )
				p = model->getParent( p );
			if ( p != b )
				continue;
			QVector<Vector3> soup;
			soup.reserve( s->triangles.size() * 3 );
			int nv = s->verts.size();
			for ( const Triangle & t : s->triangles ) {
				if ( t[0] >= nv || t[1] >= nv || t[2] >= nv )
					continue;
				soup << s->verts.at( t[0] ) << s->verts.at( t[1] ) << s->verts.at( t[2] );
			}
			if ( !soup.isEmpty() )
				shs.append( qMakePair( s, soup ) );
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
		for ( const auto & sp : shs ) {
			scene->loadModelViewMatrix( sp.first->viewTrans() );
			scene->setGLColor( 0.0f, 0.0f, 0.0f, 1.0f );
			scene->drawTriangles( sp.second.constData(), size_t( sp.second.size() ), nullptr, true );
		}
		glColorMask( GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE );

		// pass 2: thick wireframe clipped to OUTSIDE the stencil = silhouette
		glStencilFunc( GL_EQUAL, 0, 0xFF );
		glStencilOp( GL_KEEP, GL_KEEP, GL_KEEP );
		if ( transforming )
			scene->setGLColor( 1.0f, 1.0f, 1.0f, 1.0f );
		else if ( b == objActive )
			scene->setGLColor( 1.0f, 0.616f, 0.0f, 1.0f );	// #FF9D00
		else
			scene->setGLColor( 1.0f, 0.447f, 0.0f, 1.0f );	// #FF7200
		scene->setGLLineWidth( 3.2f * dpr );
		for ( const auto & sp : shs ) {
			scene->loadModelViewMatrix( sp.first->viewTrans() );
			scene->drawTriangles( sp.second.constData(), size_t( sp.second.size() ), nullptr, false );
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
				m += wt * s->verts[pe.e0];
				n++;
				continue;
			} else if ( pe.type == 2 && pe.e0 < nv && pe.e1 < nv ) {
				m += wt * ( ( s->verts[pe.e0] + s->verts[pe.e1] ) * 0.5f );
				n++;
				continue;
			} else if ( pe.type == 3 && pe.e0 >= 0 && pe.e0 < s->triangles.size() ) {
				const Triangle & t = s->triangles.at( pe.e0 );
				if ( t[0] < nv && t[1] < nv && t[2] < nv ) {
					m += wt * ( ( s->verts[t[0]] + s->verts[t[1]] + s->verts[t[2]] ) / 3.0f );
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
		Vector3 va = s->verts.at( tri[0] ), vb = s->verts.at( tri[1] ), vc = s->verts.at( tri[2] );

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
			pe.worldPos = wt * s->verts.at( pe.e0 );
			pe.wA = pe.wB = pe.wC = pe.worldPos;
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
			pe.wA = wt * s->verts.at( pe.e0 );
			pe.wB = wt * s->verts.at( pe.e1 );
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
		float best = 12.0f;
		for ( Shape * s : scene->shapes ) {
			if ( !s || s->isHidden() )
				continue;
			if ( only && !only->contains( s->id() ) )
				continue;
			Transform wt = shapeRenderTrans( s );
			for ( int i = 0; i < s->verts.size(); i++ ) {
				QPointF sp;
				Vector3 wv = wt * s->verts.at( i );
				if ( !worldToScreen( wv, sp ) )
					continue;
				float d = float( std::hypot( sp.x() - pos.x(), sp.y() - pos.y() ) );
				if ( d < best ) {
					best = d;
					pe.shapeBlock = s->id();
					pe.type = 1;
					pe.e0 = i;
					pe.e1 = -1;
					pe.worldPos = wv;
					pe.wA = pe.wB = pe.wC = wv;
				}
			}
		}
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

	if ( additive ) {
		int at = pickedElems.indexOf( pe );
		if ( at >= 0 )
			pickedElems.remove( at );
		else
			pickedElems.append( pe );
	} else {
		pickedElems.clear();
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
		pe.worldPos = wt * s->verts.at( vi );
		pe.wA = pe.wB = pe.wC = pe.worldPos;
		return pe;
	};
	auto edgeElem = [&]( int a, int b ) {
		PickedElement pe;
		pe.shapeBlock = target.shapeBlock;
		pe.type = 2;
		pe.e0 = std::min( a, b );
		pe.e1 = std::max( a, b );
		pe.wA = wt * s->verts.at( pe.e0 );
		pe.wB = wt * s->verts.at( pe.e1 );
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
				pe.wA = wt * s->verts.at( t[0] );
				pe.wB = wt * s->verts.at( t[1] );
				pe.wC = wt * s->verts.at( t[2] );
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
			if ( !n )
				continue;
			Transform wt = shapeRenderTrans( n );
			float sc = ( wt.scale != 0.0f ) ? wt.scale : 1.0f;
			Vector3 local = wt.rotation.inverted() * ( ( cursorPos - wt.translation ) * ( 1.0f / sc ) );

			QModelIndex iVData = model->getIndex( iShape, "Vertex Data" );
			QModelIndex iVerts = model->getIndex( model->getBlockIndex( model->getLink( iShape, "Data" ) ), "Vertices" );
			if ( !iVData.isValid() && !iVerts.isValid() )
				iVerts = model->getIndex( iShape, "Vertices" );

			for ( int vi : it.value() ) {
				if ( iVData.isValid() && vi < model->rowCount( iVData ) ) {
					QModelIndex iv = model->getIndex( model->getIndex( iVData, vi ), "Vertex" );
					if ( iv.isValid() ) {
						const NifItem * item = static_cast<const NifItem *>( iv.internalPointer() );
						if ( item && item->hasValueType( NifValue::tHalfVector3 ) )
							model->set<HalfVector3>( iv, HalfVector3( local ) );
						else
							model->set<Vector3>( iv, local );
					}
				} else if ( iVerts.isValid() && vi < model->rowCount( iVerts ) ) {
					model->set<Vector3>( model->getIndex( iVerts, vi ), local );
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
		if ( !n )
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
			ev.origWorld = wt * local;
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

	// snapshot the whole model for a single undo step
	elemBefore.clear();
	{
		QBuffer buf( &elemBefore );
		buf.open( QIODevice::WriteOnly );
		model->save( buf );
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
			SceneRayHit sh = raycastScene( QPointF( pos ), -1 );
			if ( sh.shape ) {
				const Triangle & htri = sh.shape->triangles.at( sh.tri );
				int hitBlock = sh.shape->id();
				for ( const auto & ev : elemVerts ) {
					if ( ev.shape == hitBlock
						&& ( ev.idx == htri[0] || ev.idx == htri[1] || ev.idx == htri[2] ) ) {
						sh.shape = nullptr;	// hit the dragged geometry itself
						break;
					}
				}
			}
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
				elemSnapped = true;
				snapIndicator = true;
				snapIndicatorPos = target;
			}
		}

		// grid stepping whenever no element snap engaged (see gizmoUpdate)
		if ( snap && !elemSnapped && gizmoSnapStep > 0 ) {
			for ( int c = 0; c < 3; c++ )
				deltaWorld[c] = std::round( deltaWorld[c] / gizmoSnapStep ) * gizmoSnapStep;
		}
		for ( const auto & ev : elemVerts )
			tlSetVertexLocal( model, model->getBlockIndex( ev.shape ), ev.idx, toLocal( ev.shape, ev.origWorld + deltaWorld ) );
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
		for ( const auto & ev : elemVerts ) {
			Vector3 w = elemPivot + dr * ( ev.origWorld - elemPivot );
			tlSetVertexLocal( model, model->getBlockIndex( ev.shape ), ev.idx, toLocal( ev.shape, w ) );
		}
		status = tr( "Rotate: %1°" ).arg( angle, 0, 'f', 1 );
	} else if ( gizmoMode == 3 ) {
		float factor = numeric ? gizmoPartVal( gizmoNum, 0 ) : 1.0f + dx * 0.01f * precision;
		if ( snap )
			factor = std::round( factor * 10.0f ) / 10.0f;
		for ( const auto & ev : elemVerts ) {
			Vector3 w = elemPivot + ( ev.origWorld - elemPivot ) * factor;
			tlSetVertexLocal( model, model->getBlockIndex( ev.shape ), ev.idx, toLocal( ev.shape, w ) );
		}
		status = tr( "Scale: ×%1" ).arg( factor, 0, 'f', 3 );
	}

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

void GLView::gizmoEndElement( bool commit )
{
	if ( !elemTransform )
		return;
	elemTransform = false;
	gizmoMode = 0;
	emit gizmoStatus( QString() );

	if ( model ) {
		if ( commit ) {
			// Undo via per-vertex value commands rather than a whole-model
			// snapshot: serialising the entire FO4 model on every edit is slow
			// and was corrupting some files. Each vertex already holds its new
			// value from the live drag; restore the original, then push a
			// ChangeValueCommand that re-applies the new one.
			ChangeValueCommand::createTransaction();
			for ( const ElemVert & ev : elemVerts ) {
				QModelIndex iShape = model->getBlockIndex( ev.shape );
				QModelIndex vIdx = tlVertexValueIndex( model, iShape, ev.idx );
				const NifItem * item = vIdx.isValid() ? static_cast<const NifItem *>( vIdx.internalPointer() ) : nullptr;
				if ( !item )
					continue;
				NifValue newVal = item->value();		// current (dragged) value
				NifValue oldVal = newVal;
				bool half = item->hasValueType( NifValue::tHalfVector3 );
				if ( half )
					oldVal.set<HalfVector3>( HalfVector3( ev.origLocal ), model, item );
				else
					oldVal.set<Vector3>( ev.origLocal, model, item );
				if ( !( oldVal == newVal ) )
					model->undoStack->push( new ChangeValueCommand( vIdx, oldVal, newVal, tr( "Vertex" ), model ) );
			}
		} else {
			// cancel: restore original vertex positions in place
			for ( const ElemVert & ev : elemVerts )
				tlSetVertexLocal( model, model->getBlockIndex( ev.shape ), ev.idx, ev.origLocal );
		}
	}

	elemVerts.clear();
	elemBefore.clear();
	modelChanged();
	update();
}

void GLView::deletePickedElements()
{
	if ( !model || pickedElems.isEmpty() )
		return;

	// vertices to delete (verts, edge endpoints, face corners)
	QHash<int, QSet<int>> vertsByShape = pickedVertexRefs();
	// explicitly face-selected triangles
	QHash<int, QSet<int>> faceTris;
	for ( const auto & pe : pickedElems ) {
		if ( pe.type == 3 && pe.shapeBlock >= 0 )
			faceTris[pe.shapeBlock].insert( pe.e0 );
	}

	QSet<int> shapes;
	for ( int k : vertsByShape.keys() )
		shapes.insert( k );
	for ( int k : faceTris.keys() )
		shapes.insert( k );
	if ( shapes.isEmpty() )
		return;

	int removed = 0;
	nifSnapshotOp( model, tr( "Delete mesh elements" ), [&]() {
		for ( int sb : shapes ) {
			QModelIndex iShape = model->getBlockIndex( sb );
			const QSet<int> & delVerts = vertsByShape.value( sb );
			const QSet<int> & delFaces = faceTris.value( sb );

			auto keepTri = [&]( const Triangle & tri, int t ) {
				if ( delFaces.contains( t ) )
					return false;
				if ( !delVerts.isEmpty() && ( delVerts.contains( tri[0] ) || delVerts.contains( tri[1] ) || delVerts.contains( tri[2] ) ) )
					return false;
				return true;
			};

			// BSTriShape: triangles + Num Triangles + Data Size live on the block
			QModelIndex iTris = model->getIndex( iShape, "Triangles" );
			if ( iTris.isValid() && model->getIndex( iShape, "Num Triangles" ).isValid() ) {
				int numTris = model->get<int>( iShape, "Num Triangles" );
				int numVerts = model->get<int>( iShape, "Num Vertices" );
				int dataSize = model->get<int>( iShape, "Data Size" );
				int stride = ( numVerts > 0 ) ? ( dataSize - numTris * 6 ) / numVerts : 0;
				QVector<Triangle> keep;
				for ( int t = 0; t < numTris && t < model->rowCount( iTris ); t++ ) {
					Triangle tri = model->get<Triangle>( model->getIndex( iTris, t ) );
					if ( keepTri( tri, t ) )
						keep.append( tri );
				}
				removed += numTris - keep.size();
				model->set<int>( iShape, "Num Triangles", keep.size() );
				model->updateArraySize( iTris );
				for ( int t = 0; t < keep.size(); t++ )
					model->set<Triangle>( model->getIndex( iTris, t ), keep[t] );
				if ( stride > 0 )
					model->set<int>( iShape, "Data Size", numVerts * stride + keep.size() * 6 );
				continue;
			}

			// legacy NiTriShapeData
			QModelIndex iData = model->getBlockIndex( model->getLink( iShape, "Data" ) );
			QModelIndex iTris2 = model->getIndex( iData, "Triangles" );
			if ( iTris2.isValid() ) {
				int numTris = model->get<int>( iData, "Num Triangles" );
				QVector<Triangle> keep;
				for ( int t = 0; t < numTris && t < model->rowCount( iTris2 ); t++ ) {
					Triangle tri = model->get<Triangle>( model->getIndex( iTris2, t ) );
					if ( keepTri( tri, t ) )
						keep.append( tri );
				}
				removed += numTris - keep.size();
				model->set<int>( iData, "Num Triangles", keep.size() );
				if ( model->getIndex( iData, "Num Triangle Points" ).isValid() )
					model->set<int>( iData, "Num Triangle Points", keep.size() * 3 );
				model->updateArraySize( iTris2 );
				for ( int t = 0; t < keep.size(); t++ )
					model->set<Triangle>( model->getIndex( iTris2, t ), keep[t] );
			}
		}
	} );

	emit gizmoStatus( tr( "Deleted %1 triangle(s)" ).arg( removed ) );
	pickedElems.clear();
	modelChanged();
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
			nif->loadIndex( buffer, nb );
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
	nif->set<int>( iShape, "Num Triangles", kept.size() );
	nif->updateArraySize( iTris );
	for ( int t = 0; t < kept.size(); t++ )
		nif->set<Triangle>( nif->getIndex( iTris, t ), kept[t] );
	if ( stride > 0 )
		nif->set<int>( iShape, "Data Size", numVerts * stride + kept.size() * 6 );
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
	QMenu m;
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

	int totalMoved = 0;
	int selectBlock = -1;
	nifSnapshotOp( model, tr( "Separate selection" ), [&]() {
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

			int nNew = tlCloneShapeWithProps( model, sb );
			if ( nNew < 0 )
				continue;

			int parentNum = model->getParent( sb );
			if ( parentNum >= 0 )
				blockLink( model, model->getBlockIndex( parentNum ), model->getBlockIndex( nNew ) );

			QString nm = model->get<QString>( model->getBlockIndex( sb ), "Name" );
			model->set<QString>( model->getBlockIndex( nNew ), "Name", tlUniqueNodeName( model, nm ) );

			// new keeps the separated triangles; original keeps the rest
			tlKeepTriangles( model, model->getBlockIndex( nNew ), [&]( int t ) { return sep[t]; } );
			tlKeepTriangles( model, model->getBlockIndex( sb ), [&]( int t ) { return !sep[t]; } );

			totalMoved += sepCount;
			selectBlock = nNew;
		}
	} );

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

	QVector<int> newBlocks;
	nifSnapshotOp( model, tr( "Duplicate" ), [&]() {
		for ( int sb : objSelection ) {
			QModelIndex iS = model->getBlockIndex( sb );
			if ( !model->blockInherits( iS, "BSTriShape" ) )
				continue;	// v1: geometry duplicate (branch duplicate is future)
			int nNew = tlCloneShapeWithProps( model, sb );
			if ( nNew < 0 )
				continue;
			int parentNum = model->getParent( sb );
			if ( parentNum >= 0 )
				blockLink( model, model->getBlockIndex( parentNum ), model->getBlockIndex( nNew ) );
			QString nm = model->get<QString>( model->getBlockIndex( sb ), "Name" );
			model->set<QString>( model->getBlockIndex( nNew ), "Name", tlUniqueNodeName( model, nm ) );
			newBlocks.append( nNew );
		}
	} );

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
	QPersistentModelIndex pActive( iActive );
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
		}

		tlUpdateBounds( model, QModelIndex( pActive ) );

		// remove the merged source blocks (high -> low so numbers stay valid)
		QVector<int> rm = sources;
		std::sort( rm.begin(), rm.end(), std::greater<int>() );
		for ( int sb : rm )
			model->removeNiBlock( sb );
	} );

	if ( joined == 0 ) {
		emit gizmoStatus( tr( "Join: nothing merged" ) );
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
	emit gizmoStatus( tr( "Joined %1 mesh(es) into the active object" ).arg( joined ) );
	modelChanged();
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

	nifSnapshotOp( model, tr( "Duplicate" ), [&]() {
		for ( int sb : shapeSet ) {
			QModelIndex iShape = model->getBlockIndex( sb );
			if ( !model->blockInherits( iShape, "BSTriShape" ) )
				continue;
			QModelIndex iVD = model->getIndex( iShape, "Vertex Data" );
			QModelIndex iTris = model->getIndex( iShape, "Triangles" );
			if ( !iVD.isValid() || !iTris.isValid() )
				continue;
			int oldNV = model->get<int>( iShape, "Num Vertices" );
			int oldNT = model->get<int>( iShape, "Num Triangles" );
			int dataSize = model->get<int>( iShape, "Data Size" );
			int stride = ( oldNV > 0 ) ? ( dataSize - oldNT * 6 ) / oldNV : 0;

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

			QVector<int> dupVerts( dupVertsSet.constBegin(), dupVertsSet.constEnd() );
			std::sort( dupVerts.begin(), dupVerts.end() );
			QHash<int, int> vremap;
			for ( int i = 0; i < dupVerts.size(); i++ )
				vremap.insert( dupVerts[i], oldNV + i );
			QHash<int, int> fremap;
			for ( int i = 0; i < dupFaces.size(); i++ )
				fremap.insert( dupFaces[i], oldNT + i );

			// append the duplicated verts (verbatim - same positions/normals)
			int addNV = dupVerts.size();
			model->set<int>( iShape, "Num Vertices", oldNV + addNV );
			model->updateArraySize( iVD );
			for ( int i = 0; i < addNV; i++ )
				tlCopyItemValues( model, model->getIndex( iVD, dupVerts[i] ), model->getIndex( iVD, oldNV + i ) );

			// append the duplicated faces (reindexed to the new verts)
			int addNT = dupFaces.size();
			if ( addNT > 0 ) {
				model->set<int>( iShape, "Num Triangles", oldNT + addNT );
				model->updateArraySize( iTris );
				for ( int i = 0; i < addNT; i++ ) {
					Triangle tri = model->get<Triangle>( model->getIndex( iTris, dupFaces[i] ) );
					tri[0] = quint16( vremap.value( tri[0] ) );
					tri[1] = quint16( vremap.value( tri[1] ) );
					tri[2] = quint16( vremap.value( tri[2] ) );
					model->set<Triangle>( model->getIndex( iTris, oldNT + i ), tri );
				}
			}
			if ( stride > 0 )
				model->set<int>( iShape, "Data Size", ( oldNV + addNV ) * stride + ( oldNT + addNT ) * 6 );

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
			totalV += addNV;
		}
	} );

	if ( newSel.isEmpty() ) {
		emit gizmoStatus( tr( "Nothing to duplicate (select verts / faces)" ) );
		return;
	}

	// select the duplicates and start a move; Esc leaves them coincident with
	// the original (still the only selected geometry), Blender-style
	pickedElems = newSel;
	modelChanged();
	gizmoBeginElement( 1 );
	emit gizmoStatus( tr( "Duplicated %1 vert(s) - move, or Esc to leave in place" ).arg( totalV ) );
}

void GLView::showSetOriginMenu()
{
	if ( !model || ( editMode ? editShapeBlocks.isEmpty() : objSelection.isEmpty() ) ) {
		emit gizmoStatus( tr( "Set Origin needs a mesh selected" ) );
		return;
	}
	QMenu m;
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
		// edit every selected mesh (object-mode multi-selection), plus the one
		// the current block resolves to
		editShapeBlocks.clear();
		for ( int sb : objSelection ) {
			if ( isEditableMesh( model->getBlockIndex( sb ) ) )
				editShapeBlocks.insert( sb );
		}
		editShapeBlocks.insert( b );
		editShapeBlock = b;
		pickMode = 1;	// start in vertex select, like Blender
		scene->editMode = true;
		scene->restPoseBlock = b;
		scene->hiddenTris = editHiddenTris;	// hidden elements apply in edit mode only
		emit gizmoStatus( tr( "Edit Mode (%1 mesh%2): 1/2/3 = vertex/edge/face, G/R/S, X delete, Shift+S snap, Tab exits" )
			.arg( editShapeBlocks.size() ).arg( editShapeBlocks.size() == 1 ? "" : "es" ) );
	} else {
		editMode = false;
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

	int outType = flatOnly ? 3 : ( pickMode ? pickMode : 1 );

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
			Vector3 n = Vector3::crossproduct( s->verts[t[1]] - s->verts[t[0]], s->verts[t[2]] - s->verts[t[0]] );
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
				pe.wA = wt * s->verts[t[0]];
				pe.wB = wt * s->verts[t[1]];
				pe.wC = wt * s->verts[t[2]];
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
				pe.worldPos = wt * s->verts[vi];
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
				pe.wA = wt * s->verts[ed.first];
				pe.wB = wt * s->verts[ed.second];
				pe.wC = pe.wA;
				pe.worldPos = ( pe.wA + pe.wB ) / 2.0f;
				if ( !already( pe ) )
					pickedElems.append( pe );
			}
		}
	}

	if ( flatOnly && pickMode != 3 ) {
		pickMode = 3;	// switch to face mode without clearing the new selection
		emit pickModeChanged( pickMode );
	}
	emit gizmoStatus( tr( "Selected linked: %1 element(s)" ).arg( pickedElems.size() ) );
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
			if ( !n )
				continue;
			Transform wt = shapeRenderTrans( n );
			float sc = ( wt.scale != 0.0f ) ? wt.scale : 1.0f;
			for ( int vi : it.value() ) {
				Vector3 local;
				if ( !tlGetVertexLocal( model, iShape, vi, local ) )
					continue;
				Vector3 world = wt * local;
				for ( int c = 0; c < 3; c++ )
					world[c] = std::round( world[c] / step ) * step;
				Vector3 nl = wt.rotation.inverted() * ( ( world - wt.translation ) * ( 1.0f / sc ) );
				tlSetVertexLocal( model, iShape, vi, nl );
			}
		}
	} );
	modelChanged();
}

void GLView::showSnapMenu()
{
	QMenu m;
	m.addSection( tr( "Snap" ) );
	QAction * aSelGrid = m.addAction( tr( "Selection to Grid" ) );
	QAction * aSelCur  = m.addAction( tr( "Selection to Cursor" ) );
	QAction * aSelOrig = m.addAction( editMode ? tr( "Selection to Node Origin" ) : tr( "Selection to Active" ) );
	m.addSeparator();
	QAction * aCurSel  = m.addAction( tr( "Cursor to Selected" ) );
	QAction * aCurOrig = m.addAction( tr( "Cursor to World Origin" ) );
	QAction * aCurNode = m.addAction( editMode ? tr( "Cursor to Node Origin" ) : tr( "Cursor to Active" ) );
	QAction * aCurGrid = m.addAction( tr( "Cursor to Grid" ) );

	// object mode works on the selected objects' origins instead of vertices
	bool hasSel = editMode ? !pickedElems.isEmpty() : !objSelection.isEmpty();
	aSelGrid->setEnabled( hasSel );
	aSelCur->setEnabled( hasSel );
	aSelOrig->setEnabled( editMode ? hasSel : ( objActive >= 0 && objSelection.size() > 1 ) );
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
	QSettings settings;
	settings.beginGroup( "GLView" );
	settings.beginGroup( "User View" );
	settings.setValue( "RotX", Rot[0] );
	settings.setValue( "RotY", Rot[1] );
	settings.setValue( "RotZ", Rot[2] );
	settings.setValue( "PosX", Pos[0] );
	settings.setValue( "PosY", Pos[1] );
	settings.setValue( "PosZ", Pos[2] );
	settings.setValue( "Dist", Dist );
	settings.endGroup();
	settings.endGroup();
}

void GLView::loadUserView()
{
	QSettings settings;
	settings.beginGroup( "GLView" );
	settings.beginGroup( "User View" );
	setRotation( settings.value( "RotX" ).toDouble(), settings.value( "RotY" ).toDouble(), settings.value( "RotZ" ).toDouble() );
	setPosition( settings.value( "PosX" ).toDouble(), settings.value( "PosY" ).toDouble(), settings.value( "PosZ" ).toDouble() );
	setDistance( settings.value( "Dist" ).toDouble() );
	settings.endGroup();
	settings.endGroup();
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
	if ( e->reason() == QContextMenuEvent::Keyboard || ( pressPos - lastPos ).manhattanLength() <= 10 ) {
		mouseButtonState = 0;
		contextMenuShiftModifier = bool( e->modifiers() & Qt::ShiftModifier );
		emit graphicsView->customContextMenuRequested( e->pos() );
		e->accept();
	}
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
	// modal transform gizmo
	if ( gizmoMode ) {
		// Blender: G/R/S during a gesture switches the transform mode,
		// resetting to the original values first; R while already rotating
		// toggles trackball rotation (Blender R,R)
		if ( event->key() == Qt::Key_G || event->key() == Qt::Key_R || event->key() == Qt::Key_S ) {
			int nm = ( event->key() == Qt::Key_G ) ? 1 : ( event->key() == Qt::Key_R ? 2 : 3 );
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
			if ( ( event->key() >= Qt::Key_0 && event->key() <= Qt::Key_9 )
				|| event->key() == Qt::Key_Period || event->key() == Qt::Key_Comma ) {
				if ( gizmoNumCur < gizmoNum.size() ) {
					QChar c = ( event->key() == Qt::Key_Comma ) ? QLatin1Char( '.' )
					          : QChar( (ushort)( event->key() == Qt::Key_Period ? '.' : '0' + ( event->key() - Qt::Key_0 ) ) );
					if ( c != QLatin1Char( '.' ) || !gizmoNum[gizmoNumCur].contains( QLatin1Char( '.' ) ) )
						gizmoNum[gizmoNumCur].append( c );
					gizmoUpdate( mapFromGlobal( QCursor::pos() ), event->modifiers() );
				}
			}
			return;
		}
	}

	// Free camera (Blender fly): only WASD/Q/E + Shift move; everything else is
	// locked out until you exit with Shift+F / Esc
	if ( freeCamera ) {
		if ( ( event->key() == Qt::Key_F && ( event->modifiers() & Qt::ShiftModifier ) )
			|| event->key() == Qt::Key_Escape ) {
			setFreeCamera( false );
			return;
		}
		int fk = convertKeyCode( event->key() );
		if ( fk >= 0 )
			kbdState = kbdState | ( 1ULL << fk );
		return;
	}

	// Tab toggles Blender-style Object / Edit mode (needs a mesh selected)
	if ( event->key() == Qt::Key_Tab && !( event->modifiers() & ( Qt::ControlModifier | Qt::AltModifier ) ) ) {
		setEditMode( !editMode );
		return;
	}

	// edit-mode select-linked (Ctrl+L) and linked flat faces (Shift+Ctrl+Alt+F)
	if ( editMode ) {
		Qt::KeyboardModifiers mods = event->modifiers();
		if ( event->key() == Qt::Key_L && ( mods & Qt::ControlModifier )
			&& !( mods & ( Qt::AltModifier | Qt::ShiftModifier ) ) ) {
			selectLinked( false );
			return;
		}
		if ( event->key() == Qt::Key_F && ( mods & Qt::ControlModifier )
			&& ( mods & Qt::AltModifier ) && ( mods & Qt::ShiftModifier ) ) {
			selectLinked( true );
			return;
		}
		// P: Separate menu (Blender)
		if ( event->key() == Qt::Key_P && !( mods & ( Qt::ControlModifier | Qt::AltModifier | Qt::ShiftModifier ) ) ) {
			showSeparateMenu();
			return;
		}
	}

	// Shift+D: duplicate the selection and start a move (object + edit mode)
	if ( event->key() == Qt::Key_D && ( event->modifiers() & Qt::ShiftModifier )
		&& !( event->modifiers() & ( Qt::ControlModifier | Qt::AltModifier ) ) && model ) {
		duplicateSelection();
		return;
	}

	// Ctrl+J: join the selected compatible meshes into the active one (object)
	if ( event->key() == Qt::Key_J && ( event->modifiers() & Qt::ControlModifier )
		&& !( event->modifiers() & ( Qt::AltModifier | Qt::ShiftModifier ) ) && !editMode && model ) {
		joinSelectedObjects();
		return;
	}

	// Shift+Ctrl+Alt+C (Set Origin) is a window-level QAction in nifskope_ui.cpp

	// H hides the selection, Alt+H reveals everything: nodes in object mode,
	// picked vertices/edges/faces in edit mode (Blender)
	if ( event->key() == Qt::Key_H ) {
		if ( event->modifiers() & Qt::AltModifier ) {
			if ( editMode )
				unhideAllElements();
			else
				unhideAll();
			return;
		}
		if ( !( event->modifiers() & ( Qt::ControlModifier | Qt::ShiftModifier ) ) ) {
			if ( editMode )
				hideSelectedElements();
			else
				hideSelected();
			return;
		}
	}

	if ( view != ViewWalk && !( event->modifiers() & ( Qt::ControlModifier | Qt::AltModifier ) ) ) {
		// Shift+S: snap pie (object and edit mode, Blender) - must beat the
		// plain S scale shortcut
		if ( event->key() == Qt::Key_S && ( event->modifiers() & Qt::ShiftModifier ) && model ) {
			showSnapMenu();
			return;
		}

		// A: select all / deselect all (Blender), in object and edit mode
		if ( event->key() == Qt::Key_A && !( event->modifiers() & Qt::ShiftModifier ) && !freeCamera && model ) {
			if ( editMode ) {
				if ( !pickedElems.isEmpty() ) {
					pickedElems.clear();
				} else {
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
								pe.wA = wt * sp->verts[tri[0]];
								pe.wB = wt * sp->verts[tri[1]];
								pe.wC = wt * sp->verts[tri[2]];
								pe.worldPos = ( pe.wA + pe.wB + pe.wC ) * ( 1.0f / 3.0f );
								pickedElems.append( pe );
							}
						} else {
							for ( int vi = 0; vi < nv; vi++ ) {
								PickedElement pe;
								pe.shapeBlock = wb;
								pe.type = 1;
								pe.e0 = vi;
								pe.worldPos = wt * sp->verts[vi];
								pe.wA = pe.worldPos;
								pickedElems.append( pe );
							}
						}
					}
				}
				update();
			} else {
				if ( !objSelection.isEmpty() ) {
					objSelection.clear();
					objActive = -1;
				} else {
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
			return;
		}

		int m = 0;
		if ( event->key() == Qt::Key_G )
			m = 1;
		else if ( event->key() == Qt::Key_R )
			m = 2;
		else if ( event->key() == Qt::Key_S )
			m = 3;

		if ( m ) {
			// with picked elements, G/R/S transforms them; otherwise the node
			if ( editMode && !pickedElems.isEmpty() && gizmoBeginElement( m ) )
				return;
			if ( gizmoBegin( m ) )
				return;
		}

		// delete picked vertices / edges / faces
		if ( ( event->key() == Qt::Key_Delete || event->key() == Qt::Key_X ) && !pickedElems.isEmpty() ) {
			deletePickedElements();
			return;
		}

		// element pick modes (Blender: 1/2/3) - only in edit mode
		int pm = 0;
		if ( event->key() == Qt::Key_1 )
			pm = 1;	// vertex bit
		else if ( event->key() == Qt::Key_2 )
			pm = 2;	// edge bit
		else if ( event->key() == Qt::Key_3 )
			pm = 4;	// face bit
		if ( pm && editMode ) {
			// Shift extends the enabled modes (multi-mode), plain sets a single one
			if ( event->modifiers() & Qt::ShiftModifier )
				setPickMode( pickMode ^ pm );
			else
				setPickMode( pm );
			emit gizmoStatus( tr( "Edit Mode - select  (click = pick, Ctrl+click = add, Shift+click = path select, G/R/S move, X delete, Shift+S snap)" ) );
			return;
		}
		if ( event->key() == Qt::Key_C ) {
			if ( event->modifiers() & Qt::ShiftModifier )
				cursorPos = pickedElems.isEmpty() ? Vector3() : pickedMedian();
			else
				placeCursor( mapFromGlobal( QCursor::pos() ) );
			update();
			return;
		}
		if ( event->key() == Qt::Key_Escape && !pickedElems.isEmpty() ) {
			pickedElems.clear();
			update();
			return;
		}
	}

	// Blender-like free camera toggle (only reached when entering; the lockout
	// block above handles exiting). Frontal light is now Ctrl+Shift+F.
	if ( event->key() == Qt::Key_F && ( event->modifiers() & Qt::ShiftModifier )
		&& !( event->modifiers() & ( Qt::ControlModifier | Qt::AltModifier ) ) ) {
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
	if ( gizmoMode ) {
		auto gp = getQMouseEventPosition( event );
		gizmoUpdate( QPoint( (int)gp.x(), (int)gp.y() ), event->modifiers() );
		return;
	}

	// free camera: mouse looks around the eye (first person), cursor recentres
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

	if ( ( buttonMask & Qt::LeftButton ) && !kbd( Key_MoveCam ) ) {
		if ( kbd( Key_RotateXY ) || kbd( Key_RotateZ ) || kbd( Key_Scale ) || kbd( Key_TranslateXY ) )
			transformItem( dx, dy );
		else if ( !frontalLight && ( event->modifiers() & Qt::ShiftModifier ) )
			rotateLight( dy * 0.5f, dx * 0.5f );
		else
			mouseRot += Vector3( dy * 0.5f, 0.0f, dx * 0.5f );
	} else if ( ( buttonMask & Qt::MiddleButton ) || ( ( buttonMask & Qt::LeftButton ) && kbd( Key_MoveCam ) ) ) {
		float d = axis / (qMax( width(), height() ) + 1);
		mouseMov += Vector3( dx * d, -dy * d, 0.0f );
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

	// clicking exits free camera (Blender walk-mode confirm/cancel)
	if ( freeCamera ) {
		setFreeCamera( false );
		lastPos = getQMouseEventPosition( event );
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

	// Shift+right-click drops the 3D cursor on the surface in ANY mode
	// (Blender); plain right-click keeps its usual behaviour in edit mode too
	if ( event->button() == Qt::RightButton && ( event->modifiers() & Qt::ShiftModifier )
		&& !( event->modifiers() & ( Qt::ControlModifier | Qt::AltModifier ) ) ) {
		placeCursor( getQMouseEventPosition( event ) );
		gizmoSwallowClick = true;
		return;
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
	if ( model && ( pressPos - evtPos ).manhattanLength() <= 3 ) {
		if ( event->button() == Qt::ForwardButton || event->button() == Qt::BackButton
			|| event->button() == Qt::MiddleButton ) {
			event->ignore();
			return;
		}

		// edit-mode element picking happens here (on a click, not a drag) so
		// camera orbiting never changes the selection: click = pick,
		// Ctrl+click = extend/toggle, Shift+click = shortest-path (Blender)
		if ( editMode && pickMode && event->button() == Qt::LeftButton && !isColorPicker ) {
			auto p = evtPos;
			if ( event->modifiers() & Qt::ShiftModifier )
				pickPathSelect( p );
			else
				pickElementAt( p, bool( event->modifiers() & Qt::ControlModifier ) );
			update();
			return;
		}

		if ( !isColorPicker ) {
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

		} else {
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

	if ( event->button() == Qt::RightButton && !isColorPicker ) {
		QContextMenuEvent	e( QContextMenuEvent::Mouse,
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
								event->pos(), event->globalPos(),
#else
								evtPos.toPoint(), event->globalPosition().toPoint(),
#endif
								event->modifiers() );
		contextMenuEvent( &e );
	}
}

void GLView::wheelEvent( QWheelEvent * event )
{
	// free camera: the scroll wheel adjusts fly speed (Blender walk/fly mode)
	if ( freeCamera ) {
		freeCamSpeed *= ( event->angleDelta().y() > 0 ) ? 1.25f : 0.8f;
		freeCamSpeed = std::min( std::max( freeCamSpeed, 0.05f ), 50.0f );
		emit gizmoStatus( tr( "Fly speed: %1x (scroll to adjust)" ).arg( double( freeCamSpeed ), 0, 'f', 2 ) );
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
