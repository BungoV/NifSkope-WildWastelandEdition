/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

/* WW_SCENE_TEST=1 -- lane PBRR2A's in-app gates (2026-09-24), run INSIDE the
   running application. tests/spells/pbr_r2a_gates.sh is the driver.

   LEGS (each one switched on by its variable)

     WW_SCENE_TEST_CUBE=1
         (cube) a uniform cube of one sRGB byte (188 -> linear L = 0.50289)
         built with FO4's own header quirk (cube bits in dwCaps, dwCaps2 = 0,
         BGRA8 masks), sent through the Studio loader (header normalise ->
         SFCubeMapCache prefilter -> upload). EVERY level of EVERY face of the
         prefiltered cube AND of the irradiance cube is read back with
         glGetTexImage: max |texel - L| <= 1/255.
         Floors: both cubes exist, the prefiltered one holds >= 7 levels, both
         are GL_RGB9_E5 (the filter's output format, so the input was not merely
         passed through), the irradiance cube is 32 px.
         Red: WW_R2A_RED=cubedecode (the header keeps UNORM, no sRGB decode):
         the readback is 0.737, the gate FAILS.
     WW_SCENE_TEST_VANILLA=<abs path of a vanilla cube .dds>
         the vanilla FO4 cube through the same loader: loads, is not black, is
         not uniform (its picture survived). Also REPORTS (no verdict) what the
         legacy loader binds for the same file on a cube unit -- the L1 finding.
     WW_SCENE_TEST_WINDOW=1
         (window) View > Scene and the toolbar button exist; the old Render-menu
         "PBR Route View" entry is gone; the action opens a Qt::Tool window owned
         by the main window, non-modal; Lighting -> Studio, then Exposure +2 EV
         changes the viewport (a NIF with a PBR shape is loaded by the driver, PBR
         display pinned); View Transform changes it again; the window is put at
         WW_SCENE_TEST_SETGEOM=x,y,w,h, closed from the menu, reopened from the
         toolbar button and is where it was.
         Red: WW_R2A_RED=nolive (the rows never reach the state): FAILS.
     WW_SCENE_TEST_EXPECT=x,y,w,h  (+ WW_SCENE_TEST_EXPECT_EV, _VIEW)
         (restart) the previous launch's geometry, exposure and view transform
         came back; the lighting mode did NOT (always Legacy, ruling Q7).
         Red: WW_R2A_RED=nosave in the SAVING launch: FAILS here.
     WW_SCENE_TEST_LOOKDEV=1
         (lookdev, lane PBRR2B) Lighting -> Lookdev through the Scene window's
         Mode row; then, through the widgets: Hour 12 -> 0 changes the viewport,
         the Ground row off changes it, and a different Weather (activated, as a
         user pick) changes the Status line. Every row reaches the state.
         Red: WW_R2A_RED=nolive (the rows never reach the state): FAILS.
     WW_SCENE_TEST_WEATHER=1
         (weather preview, lane PBRWX1) after the lookdev leg: the Sky, Clouds,
         Sun and Moon rows start OFF (masters ship off), are enabled in Lookdev,
         and each one reaches the state AND the picture (Sky / Sun / Clouds by
         pixels, Moon by the drawn echo, the Game Day row by the Status line).
         Each row also writes its setting (the load half is the driver's: a
         pre-seeded scope, then a shot, pbr_wx1_gates.sh "persist").
         Red: WW_R2A_RED=nolive (the rows never reach the state): FAILS.
         Red: WW_R2A_RED=nosave (nothing is written): the (save) checks FAIL.
     WW_SCENE_TEST_LOG=<path>

   REFUSAL: the harness writes settings, so it runs ONLY inside a scratch
   WW_SETTINGS_SCOPE; without one it FAILS by name and touches nothing. */

#include "nifskope.h"
#include "glview.h"
#include "harnesswindow.h"
#include "gl/glscene.h"
#include "gl/gltex.h"
#include "gl/scenelighting.h"
#include "gl/lookdevstage.h"
#include "model/nifmodel.h"
#include "ui/scenewindow.h"

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDoubleSpinBox>
#include <QFile>
#include <QLabel>
#include <QMenu>
#include <QSettings>
#include <QTextStream>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>
#include <QUndoStack>

#include <cmath>
#include <vector>

namespace
{

struct WwScState
{
	int checks = 0;
	int fails = 0;
	QString logPath;
	bool done = false;
};

void say( WwScState & st, const QString & line )
{
	QFile f( st.logPath );
	if ( f.open( QIODevice::Append | QIODevice::Text ) ) {
		QTextStream out( &f );
		out << line << "\n";
		out.flush();
		f.close();
	}
}

void check( WwScState & st, const QString & what, bool pass )
{
	st.checks++;
	if ( !pass )
		st.fails++;
	say( st, ( pass ? QStringLiteral( "  ok   " ) : QStringLiteral( "  FAIL " ) ) + what );
}

bool quad( const char * var, QRect & r )
{
	const QStringList l = qEnvironmentVariable( var ).split( QLatin1Char( ',' ), Qt::SkipEmptyParts );
	if ( l.size() != 4 )
		return false;
	int v[4];
	for ( int i = 0; i < 4; i++ ) {
		bool ok = false;
		v[i] = l[i].trimmed().toInt( &ok );
		if ( !ok )
			return false;
	}
	r = QRect( v[0], v[1], v[2], v[3] );
	return true;
}

QString rectStr( const QRect & r )
{
	return QStringLiteral( "%1,%2,%3,%4" ).arg( r.x() ).arg( r.y() ).arg( r.width() ).arg( r.height() );
}

//! a uniform BGRA8 cube with FO4's header quirk: cube bits in dwCaps, dwCaps2 = 0
QByteArray uniformCubeFo4Style( int width, unsigned char v )
{
	int mips = 0;
	qsizetype faceBytes = 0;
	for ( int w = width; w >= 1; w >>= 1 ) {
		mips++;
		faceBytes += qsizetype( w ) * w * 4;
	}
	QByteArray d( 128 + faceBytes * 6, '\0' );
	auto put = [&d]( int off, quint32 x ) {
		for ( int i = 0; i < 4; i++ )
			d[off + i] = char( ( x >> ( 8 * i ) ) & 0xFF );
	};
	put( 0, 0x20534444 );		// "DDS "
	put( 4, 124 );
	put( 8, 0x0002100F );		// caps | height | width | pitch | pixelformat | mipcount
	put( 12, quint32( width ) );
	put( 16, quint32( width ) );
	put( 20, quint32( width * 4 ) );
	put( 28, quint32( mips ) );
	put( 76, 32 );
	put( 80, 0x41 );			// DDPF_RGB | DDPF_ALPHAPIXELS
	put( 88, 32 );
	put( 92, 0x00FF0000 );
	put( 96, 0x0000FF00 );
	put( 100, 0x000000FF );
	put( 104, 0xFF000000 );
	put( 108, 0x0040FE08 );		// exactly mipblur_DefaultOutside1.dds's dwCaps
	put( 112, 0 );				// and its dwCaps2
	for ( qsizetype i = 128; i < d.size(); i += 4 ) {
		d[i] = char( v );
		d[i + 1] = char( v );
		d[i + 2] = char( v );
		d[i + 3] = char( 0xFF );
	}
	return d;
}

struct CubeRead
{
	int levels = 0;
	int width0 = 0;
	GLint format = 0;
	double maxErr = 0.0;
	double mean = 0.0;
	double minV = 1e30, maxV = -1e30;
	qint64 texels = 0;
};

CubeRead readCube( GLuint id, double L )
{
	CubeRead r;
	glBindTexture( GL_TEXTURE_CUBE_MAP, id );
	GLint maxLevel = 0;
	glGetTexParameteriv( GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAX_LEVEL, &maxLevel );
	glGetTexLevelParameteriv( GL_TEXTURE_CUBE_MAP_POSITIVE_X, 0, GL_TEXTURE_INTERNAL_FORMAT, &r.format );
	glPixelStorei( GL_PACK_ALIGNMENT, 1 );
	double sum = 0.0;
	for ( int lvl = 0; lvl <= maxLevel && lvl < 16; lvl++ ) {
		GLint w = 0, h = 0;
		glGetTexLevelParameteriv( GL_TEXTURE_CUBE_MAP_POSITIVE_X, lvl, GL_TEXTURE_WIDTH, &w );
		glGetTexLevelParameteriv( GL_TEXTURE_CUBE_MAP_POSITIVE_X, lvl, GL_TEXTURE_HEIGHT, &h );
		if ( w <= 0 || h <= 0 )
			break;
		if ( lvl == 0 )
			r.width0 = w;
		r.levels++;
		std::vector<float> px( size_t( w ) * size_t( h ) * 3 );
		for ( int face = 0; face < 6; face++ ) {
			glGetTexImage( GLenum( GL_TEXTURE_CUBE_MAP_POSITIVE_X + face ), lvl, GL_RGB, GL_FLOAT, px.data() );
			for ( float v : px ) {
				r.maxErr = std::max( r.maxErr, std::fabs( double( v ) - L ) );
				r.minV = std::min( r.minV, double( v ) );
				r.maxV = std::max( r.maxV, double( v ) );
				sum += v;
			}
			r.texels += qint64( px.size() );
		}
	}
	glPixelStorei( GL_PACK_ALIGNMENT, 4 );
	r.mean = r.texels ? sum / double( r.texels ) : 0.0;
	return r;
}

constexpr GLint kRGB9E5 = 0x8C3D;

void cubeLeg( NifSkope * skope, WwScState & st )
{
	GLView * ogl = skope->getGLView();
	if ( !ogl || !ogl->getScene() || !ogl->getScene()->textures ) {
		check( st, QStringLiteral( "(floor) the viewport and its texture cache exist" ), false );
		return;
	}
	ogl->makeCurrent();
	TexCache * tc = ogl->getScene()->textures;
	const NifModel * nif = skope->getNifModel();

	if ( qEnvironmentVariable( "WW_SCENE_TEST_CUBE" ) == QLatin1StringView( "1" ) ) {
		const unsigned char byte = 188;
		const double c = byte / 255.0;
		const double L = c <= 0.04045 ? c / 12.92 : std::pow( ( c + 0.055 ) / 1.055, 2.4 );
		say( st, QStringLiteral( "--- cube: uniform sRGB byte %1 -> L = %2, tolerance 1/255 = %3 ---" )
			.arg( byte ).arg( L, 0, 'f', 5 ).arg( 1.0 / 255.0, 0, 'f', 5 ) );
		const QByteArray dds = uniformCubeFo4Style( 32, byte );
		GLuint id[2] = { 0, 0 };
		QString why;
		const bool ok = wwStudioCubeLoad( tc, nif, QStringLiteral( "ww_scene_test_uniform188.dds" ), dds, id, &why );
		check( st, QStringLiteral( "(floor) the Studio loader accepts the FO4-style header (%1)" )
			.arg( ok ? QStringLiteral( "loaded" ) : why ), ok );
		if ( !ok )
			return;
		const CubeRead s = readCube( id[0], L );
		const CubeRead d = readCube( id[1], L );
		say( st, QStringLiteral( "  prefiltered: %1 levels, %2 px, format 0x%3, %4 values, min %5 max %6 mean %7, max|v-L| %8" )
			.arg( s.levels ).arg( s.width0 ).arg( s.format, 0, 16 ).arg( s.texels )
			.arg( s.minV, 0, 'f', 5 ).arg( s.maxV, 0, 'f', 5 ).arg( s.mean, 0, 'f', 5 ).arg( s.maxErr, 0, 'f', 5 ) );
		say( st, QStringLiteral( "  irradiance:  %1 levels, %2 px, format 0x%3, %4 values, min %5 max %6 mean %7, max|v-L| %8" )
			.arg( d.levels ).arg( d.width0 ).arg( d.format, 0, 16 ).arg( d.texels )
			.arg( d.minV, 0, 'f', 5 ).arg( d.maxV, 0, 'f', 5 ).arg( d.mean, 0, 'f', 5 ).arg( d.maxErr, 0, 'f', 5 ) );
		check( st, QStringLiteral( "(floor) the prefiltered cube holds >= 7 levels (the roughness table)" ), s.levels >= 7 );
		check( st, QStringLiteral( "(floor) both cubes are GL_RGB9_E5, the prefilter's output" ),
			s.format == kRGB9E5 && d.format == kRGB9E5 );
		check( st, QStringLiteral( "(floor) the irradiance cube is 32 px" ), d.width0 == 32 );
		check( st, QStringLiteral( "(cube) every prefiltered texel of every level = L within 1/255 (max %1)" )
			.arg( s.maxErr, 0, 'f', 5 ), s.texels > 0 && s.maxErr <= 1.0 / 255.0 );
		check( st, QStringLiteral( "(cube) every irradiance texel = L within 1/255 (max %1)" )
			.arg( d.maxErr, 0, 'f', 5 ), d.texels > 0 && d.maxErr <= 1.0 / 255.0 );
		glDeleteTextures( 2, id );
	}

	const QString vanilla = qEnvironmentVariable( "WW_SCENE_TEST_VANILLA" );
	if ( !vanilla.isEmpty() ) {
		say( st, QStringLiteral( "--- vanilla cube: %1 ---" ).arg( vanilla ) );
		QFile f( vanilla );
		QByteArray data;
		if ( f.open( QIODevice::ReadOnly ) )
			data = f.readAll();
		check( st, QStringLiteral( "(floor) the vanilla cube file was read (%1 bytes)" ).arg( data.size() ), data.size() > 128 );
		if ( data.size() > 128 ) {
			GLuint id[2] = { 0, 0 };
			QString why;
			const bool ok = wwStudioCubeLoad( tc, nif, QStringLiteral( "ww_scene_test_vanilla.dds" ), data, id, &why );
			check( st, QStringLiteral( "(vanilla) the Studio loader makes a cube of it (%1)" )
				.arg( ok ? QStringLiteral( "loaded" ) : why ), ok );
			if ( ok ) {
				const CubeRead s = readCube( id[0], 0.0 );
				say( st, QStringLiteral( "  prefiltered: %1 levels, %2 px, min %3 max %4 mean %5" )
					.arg( s.levels ).arg( s.width0 ).arg( s.minV, 0, 'f', 4 ).arg( s.maxV, 0, 'f', 4 ).arg( s.mean, 0, 'f', 4 ) );
				check( st, QStringLiteral( "(vanilla) it is not black (mean %1 > 0.01)" ).arg( s.mean, 0, 'f', 4 ), s.mean > 0.01 );
				check( st, QStringLiteral( "(vanilla) it is not uniform (its picture survived: max-min %1 > 0.05)" )
					.arg( s.maxV - s.minV, 0, 'f', 4 ), s.maxV - s.minV > 0.05 );
				glDeleteTextures( 2, id );
			}
		}
		// the L1 finding: what the LEGACY loader binds for the same file on a cube unit
		if ( nif ) {
			glBindTexture( GL_TEXTURE_CUBE_MAP, 0 );
			glBindTexture( GL_TEXTURE_2D, 0 );
			const bool bound = tc->bindCube( QStringLiteral( "textures/shared/cubemaps/mipblur_defaultoutside1.dds" ), nif, false );
			GLint cube = 0, tex2d = 0;
			glGetIntegerv( GL_TEXTURE_BINDING_CUBE_MAP, &cube );
			glGetIntegerv( GL_TEXTURE_BINDING_2D, &tex2d );
			say( st, QStringLiteral( "  REPORT (L1, no verdict): legacy bindCube(mipblur_defaultoutside1) returned %1; "
									 "cube binding %2, 2D binding %3 -> the legacy path binds it as %4" )
				.arg( bound ? QStringLiteral( "true" ) : QStringLiteral( "false" ) ).arg( cube ).arg( tex2d )
				.arg( cube ? QStringLiteral( "a CUBE" ) : ( tex2d ? QStringLiteral( "a 2D texture (the cube unit stays empty)" )
															 : QStringLiteral( "nothing" ) ) ) );
		}
	}
	ogl->doneCurrent();
}

int diffCount( const QImage & a, const QImage & b )
{
	if ( a.isNull() || b.isNull() || a.size() != b.size() )
		return -1;
	int d = 0;
	for ( int y = 0; y < a.height(); y += 2 )
		for ( int x = 0; x < a.width(); x += 2 )
			if ( a.pixel( x, y ) != b.pixel( x, y ) )
				d++;
	return d;
}

double meanLuma( const QImage & a )
{
	if ( a.isNull() )
		return 0.0;
	double s = 0.0;
	qint64 n = 0;
	for ( int y = 0; y < a.height(); y += 2 )
		for ( int x = 0; x < a.width(); x += 2 ) {
			const QRgb p = a.pixel( x, y );
			s += 0.2126 * qRed( p ) + 0.7152 * qGreen( p ) + 0.0722 * qBlue( p );
			n++;
		}
	return n ? s / double( n ) : 0.0;
}

QImage freshGrab( NifSkope * skope )
{
	skope->getGLView()->update();
	QCoreApplication::processEvents( QEventLoop::AllEvents, 250 );
	QCoreApplication::processEvents( QEventLoop::AllEvents, 250 );
	QCoreApplication::processEvents( QEventLoop::AllEvents, 250 );
	return skope->getGLView()->grabFramebuffer();
}

void pump( int rounds = 4 )
{
	for ( int i = 0; i < rounds; i++ )
		QCoreApplication::processEvents( QEventLoop::AllEvents, 100 );
}

void windowLeg( NifSkope * skope, WwScState & st )
{
	QAction * a = skope->findChild<QAction *>( QStringLiteral( "aSceneWindow" ) );
	QWidget * w = skope->findChild<QWidget *>( QStringLiteral( "SceneWindow" ) );
	check( st, QStringLiteral( "(floor) the Scene action exists" ), a != nullptr );
	check( st, QStringLiteral( "(floor) the Scene window exists" ), w != nullptr );
	if ( !a || !w )
		return;

	QMenu * mView = skope->findChild<QMenu *>( QStringLiteral( "mView" ) );
	QMenu * mRender = skope->findChild<QMenu *>( QStringLiteral( "mRender" ) );
	QToolBar * tRender = skope->findChild<QToolBar *>( QStringLiteral( "tRender" ) );
	check( st, QStringLiteral( "(entry) View > Scene is in the View menu" ), mView && mView->actions().contains( a ) );
	QToolButton * button = nullptr;
	if ( tRender )
		for ( QToolButton * b : tRender->findChildren<QToolButton *>() )
			if ( b->defaultAction() == a )
				button = b;
	check( st, QStringLiteral( "(entry) the viewport toolbar has the Scene button" ), button != nullptr );
	bool oldRoute = false;
	if ( mRender )
		for ( QAction * x : mRender->actions() )
			if ( x->text() == QLatin1StringView( "PBR Route View" ) )
				oldRoute = true;
	check( st, QStringLiteral( "(moved) the Render menu no longer holds PBR Route View" ), mRender && !oldRoute );
	check( st, QStringLiteral( "(moved) the Scene window holds the PBR Route View row" ),
		w->findChild<QWidget *>( QStringLiteral( "sceneRouteView" ) ) != nullptr );

	a->trigger();
	pump();
	check( st, QStringLiteral( "(open) the menu action opens it" ), w->isVisible() && a->isChecked() );
	check( st, QStringLiteral( "(window) a separate top-level Qt::Tool window owned by the main window, non-modal" ),
		w->isWindow() && ( w->windowFlags() & Qt::WindowType_Mask ) == Qt::Tool && w->parentWidget() == skope
		&& !w->isModal() && w->windowModality() == Qt::NonModal );

	auto * mode = w->findChild<QComboBox *>( QStringLiteral( "sceneMode" ) );
	auto * ev = w->findChild<QDoubleSpinBox *>( QStringLiteral( "sceneExposure" ) );
	auto * view = w->findChild<QComboBox *>( QStringLiteral( "sceneViewTransform" ) );
	check( st, QStringLiteral( "(floor) the Lighting, Exposure and View Transform rows exist" ), mode && ev && view );
	if ( mode && ev && view ) {
		const QImage legacy = freshGrab( skope );
		mode->setCurrentIndex( 1 );
		const QImage studio = freshGrab( skope );
		const double ev0 = ev->value();
		ev->setValue( ev0 + 2.0 );
		const QImage brighter = freshGrab( skope );
		const int viewBefore = view->currentIndex();
		view->setCurrentIndex( viewBefore == 0 ? 2 : 0 );
		const QImage other = freshGrab( skope );
		const int dMode = diffCount( legacy, studio ), dEv = diffCount( studio, brighter ), dView = diffCount( brighter, other );
		say( st, QStringLiteral( "  grabs %1x%2: Legacy->Studio %3 px differ, EV %4->%5 %6 px (mean luma %7 -> %8), view %9->%10 %11 px" )
			.arg( studio.width() ).arg( studio.height() ).arg( dMode )
			.arg( ev0 ).arg( ev0 + 2.0 ).arg( dEv ).arg( meanLuma( studio ), 0, 'f', 2 ).arg( meanLuma( brighter ), 0, 'f', 2 )
			.arg( viewBefore ).arg( view->currentIndex() ).arg( dView ) );
		check( st, QStringLiteral( "(live) the Lighting row reached the state (Studio)" ), wwSceneMode() == WwSceneStudio );
		check( st, QStringLiteral( "(live) Legacy -> Studio changes the viewport (%1 px >= 500)" ).arg( dMode ), dMode >= 500 );
		check( st, QStringLiteral( "(live) Exposure +2 EV changes the viewport (%1 px >= 500) and brightens it" ).arg( dEv ),
			dEv >= 500 && meanLuma( brighter ) > meanLuma( studio ) );
		check( st, QStringLiteral( "(live) the View Transform row changes the viewport (%1 px >= 500)" ).arg( dView ), dView >= 500 );
		// leave the saved values the restart leg expects: Standard
		view->setCurrentIndex( 0 );
		pump();
		say( st, QStringLiteral( "  left for the restart: exposure %1 EV, view %2, stored: %3" )
			.arg( ev->value() ).arg( view->currentIndex() ).arg( wwSceneEcho() ) );
	}

	QRect want;
	if ( quad( "WW_SCENE_TEST_SETGEOM", want ) ) {
		w->setGeometry( want );
		pump();
		const QRect g1 = w->geometry();
		say( st, QStringLiteral( "  put at %1, the window reports %2" ).arg( rectStr( want ), rectStr( g1 ) ) );
		a->trigger();	// close from the menu
		pump();
		check( st, QStringLiteral( "(close) the menu action closes it and unchecks" ), !w->isVisible() && !a->isChecked() );
		if ( button )
			button->click();	// reopen from the toolbar button
		pump();
		const QRect g2 = w->geometry();
		check( st, QStringLiteral( "(reopen) the toolbar button reopens it" ), w->isVisible() && a->isChecked() );
		check( st, QStringLiteral( "(geometry) reopened where it was: %1 == %2" ).arg( rectStr( g2 ), rectStr( g1 ) ), g2 == g1 );
		a->trigger();	// close: saves
		pump();
		QSettings s;
		s.sync();
		say( st, QStringLiteral( "  stored geometry: %1 bytes" )
			.arg( s.value( QLatin1StringView( SceneWindow::geometryKey() ) ).toByteArray().size() ) );
	}
}

void lookdevLeg( NifSkope * skope, WwScState & st )
{
	QWidget * w = skope->findChild<QWidget *>( QStringLiteral( "SceneWindow" ) );
	check( st, QStringLiteral( "(floor) the Scene window exists (lookdev leg)" ), w != nullptr );
	if ( !w )
		return;
	auto * mode = w->findChild<QComboBox *>( QStringLiteral( "sceneMode" ) );
	auto * plugin = w->findChild<QComboBox *>( QStringLiteral( "lookdevPlugin" ) );
	auto * weather = w->findChild<QComboBox *>( QStringLiteral( "lookdevWeather" ) );
	auto * hour = w->findChild<QDoubleSpinBox *>( QStringLiteral( "lookdevHour" ) );
	auto * ground = w->findChild<QCheckBox *>( QStringLiteral( "lookdevGround" ) );
	auto * status = w->findChild<QLabel *>( QStringLiteral( "lookdevStatus" ) );
	check( st, QStringLiteral( "(floor) the Mode, Plugin, Weather, Hour, Ground and Status rows exist" ),
		mode && plugin && weather && hour && ground && status );
	if ( !( mode && plugin && weather && hour && ground && status ) )
		return;
	check( st, QStringLiteral( "(floor) the Mode row offers Lookdev" ), mode->count() >= 3
		&& mode->itemText( 2 ) == QLatin1StringView( "Lookdev" ) );
	if ( !w->isVisible() ) {
		w->show();
		pump();
	}
	mode->setCurrentIndex( 2 );
	pump();
	check( st, QStringLiteral( "(live) the Mode row reached the state (Lookdev)" ), wwSceneMode() == WwSceneLookdev );
	check( st, QStringLiteral( "(rows) the weather rows are enabled in Lookdev" ),
		weather->isEnabled() && hour->isEnabled() && ground->isEnabled() && plugin->isEnabled() );
	say( st, QStringLiteral( "  plugin row: %1 (%2 items); weather row: %3 items; status: %4" )
		.arg( plugin->currentText() ).arg( plugin->count() ).arg( weather->count() ).arg( status->text() ) );
	check( st, QStringLiteral( "(rows) the Weather row lists the loaded WTHRs (%1 >= 2)" ).arg( weather->count() ),
		weather->count() >= 2 );

	hour->setValue( 12.0 );
	if ( !ground->isChecked() )
		ground->setChecked( true );
	const QImage noon = freshGrab( skope );
	const QString sNoon = status->text();
	hour->setValue( 0.0 );
	const QImage midnight = freshGrab( skope );
	const QString sMid = status->text();
	const int dHour = diffCount( noon, midnight );
	say( st, QStringLiteral( "  hour 12 -> 0: %1 px differ, mean luma %2 -> %3" )
		.arg( dHour ).arg( meanLuma( noon ), 0, 'f', 2 ).arg( meanLuma( midnight ), 0, 'f', 2 ) );
	check( st, QStringLiteral( "(live) the Hour row reached the state (%1)" ).arg( wwLookdevHour() ),
		std::fabs( wwLookdevHour() ) < 1e-6 );
	check( st, QStringLiteral( "(live) Hour 12 -> 0 changes the viewport (%1 px >= 500)" ).arg( dHour ), dHour >= 500 );
	check( st, QStringLiteral( "(live) Hour 12 -> 0 changes the Status line" ), sNoon != sMid );

	hour->setValue( 12.0 );
	const QImage withGround = freshGrab( skope );
	ground->setChecked( false );
	const QImage noGround = freshGrab( skope );
	const int dGround = diffCount( withGround, noGround );
	say( st, QStringLiteral( "  ground on -> off: %1 px differ" ).arg( dGround ) );
	check( st, QStringLiteral( "(live) the Ground row reached the state (off)" ), !wwLookdevGround() );
	check( st, QStringLiteral( "(live) Ground off changes the viewport (%1 px >= 500)" ).arg( dGround ), dGround >= 500 );
	ground->setChecked( true );
	pump();

	const QString keyBefore = wwLookdevWeatherKey();
	const QString sBefore = status->text();
	int pick = -1;
	for ( int i = 0; i < weather->count() && pick < 0; i++ )
		if ( i != weather->currentIndex() )
			pick = i;
	if ( pick >= 0 ) {
		weather->setCurrentIndex( pick );
		emit weather->activated( pick );
		pump();
	}
	const QString sAfter = status->text();
	say( st, QStringLiteral( "  weather %1 -> %2; status now: %3" ).arg( keyBefore, wwLookdevWeatherKey(), sAfter ) );
	check( st, QStringLiteral( "(live) the Weather row reached the state" ), pick >= 0 && wwLookdevWeatherKey() != keyBefore );
	check( st, QStringLiteral( "(live) the Weather row changes the Status line" ), pick >= 0 && sAfter != sBefore );
	say( st, QStringLiteral( "  echo: %1" ).arg( wwLookdevEcho() ) );
}

void weatherLeg( NifSkope * skope, WwScState & st )
{
	QWidget * w = skope->findChild<QWidget *>( QStringLiteral( "SceneWindow" ) );
	check( st, QStringLiteral( "(floor) the Scene window exists (weather leg)" ), w != nullptr );
	if ( !w )
		return;
	auto * mode = w->findChild<QComboBox *>( QStringLiteral( "sceneMode" ) );
	auto * hour = w->findChild<QDoubleSpinBox *>( QStringLiteral( "lookdevHour" ) );
	auto * status = w->findChild<QLabel *>( QStringLiteral( "lookdevStatus" ) );
	auto * sky = w->findChild<QCheckBox *>( QStringLiteral( "lookdevSky" ) );
	auto * clouds = w->findChild<QCheckBox *>( QStringLiteral( "lookdevClouds" ) );
	auto * sun = w->findChild<QCheckBox *>( QStringLiteral( "lookdevSun" ) );
	auto * moon = w->findChild<QCheckBox *>( QStringLiteral( "lookdevMoon" ) );
	auto * day = w->findChild<QDoubleSpinBox *>( QStringLiteral( "lookdevGameDay" ) );
	const bool all = mode && hour && status && sky && clouds && sun && moon && day;
	check( st, QStringLiteral( "(floor) the Sky, Clouds, Sun, Moon and Game Day rows exist" ), all );
	if ( !all )
		return;
	check( st, QStringLiteral( "(ship) every preview row starts OFF in a fresh scope" ),
		!sky->isChecked() && !clouds->isChecked() && !sun->isChecked() && !moon->isChecked()
		&& !wwLookdevSky() && !wwLookdevClouds() && !wwLookdevSun() && !wwLookdevMoon() );
	if ( !w->isVisible() ) {
		w->show();
		pump();
	}
	mode->setCurrentIndex( 0 );
	pump();
	check( st, QStringLiteral( "(rows) the preview rows are greyed outside Lookdev" ),
		!sky->isEnabled() && !clouds->isEnabled() && !sun->isEnabled() && !moon->isEnabled() && !day->isEnabled() );
	mode->setCurrentIndex( 2 );
	pump();
	check( st, QStringLiteral( "(rows) the preview rows are enabled in Lookdev" ),
		sky->isEnabled() && clouds->isEnabled() && sun->isEnabled() && moon->isEnabled() && day->isEnabled() );

	hour->setValue( 12.0 );
	pump();
	struct Step { QCheckBox * box; const char * name; bool ( *get )(); const char * echo; const char * key; };
	const Step steps[] = {
		{ sky, "Sky", &wwLookdevSky, "sky:dome", "Settings/Render/Scene/Lookdev Sky" },
		{ sun, "Sun", &wwLookdevSun, "sun:", "Settings/Render/Scene/Lookdev Sun" },
		{ clouds, "Clouds", &wwLookdevClouds, "clouds:drawn", "Settings/Render/Scene/Lookdev Clouds" },
	};
	for ( const Step & k : steps ) {
		const QImage before = freshGrab( skope );
		k.box->setChecked( true );
		const QImage after = freshGrab( skope );
		const int d = diffCount( before, after );
		const QString echo = wwLookdevSummary();
		say( st, QStringLiteral( "  %1 on: %2 px differ; status: %3" ).arg( QLatin1StringView( k.name ) ).arg( d ).arg( echo ) );
		check( st, QStringLiteral( "(live) the %1 row reached the state" ).arg( QLatin1StringView( k.name ) ), k.get() );
		check( st, QStringLiteral( "(live) %1 on changes the viewport (%2 px >= 500)" ).arg( QLatin1StringView( k.name ) ).arg( d ),
			d >= 500 );
		check( st, QStringLiteral( "(live) the drawn echo names the %1 pass" ).arg( QLatin1StringView( k.name ) ),
			echo.contains( QLatin1StringView( k.echo ) ) );
		check( st, QStringLiteral( "(save) the %1 row wrote ON to its setting" ).arg( QLatin1StringView( k.name ) ),
			QSettings().value( QLatin1StringView( k.key ) ).toBool() );
	}

	hour->setValue( 22.0 );
	day->setValue( 4.0 );
	pump();
	freshGrab( skope );
	moon->setChecked( true );
	const QImage m4 = freshGrab( skope );
	const QString e4 = status->text();
	check( st, QStringLiteral( "(live) the Moon row reached the state" ), wwLookdevMoon() );
	const QString moonEcho = wwLookdevSummary();
	say( st, QStringLiteral( "  moon on at 22:00, day 4: %1" ).arg( moonEcho ) );
	check( st, QStringLiteral( "(live) the drawn echo names the moon pass" ), moonEcho.contains( QLatin1StringView( "moon:" ) )
		&& !moonEcho.contains( QLatin1StringView( "moon:refused" ) ) );
	day->setValue( 32.0 );
	const QImage m32 = freshGrab( skope );
	const QString e32 = status->text();
	say( st, QStringLiteral( "  day 4 -> 32 at 22:00: %1 px differ; status: %2" ).arg( diffCount( m4, m32 ) ).arg( e32 ) );
	check( st, QStringLiteral( "(live) the Game Day row reached the state (%1)" ).arg( wwLookdevGameDay() ),
		std::fabs( wwLookdevGameDay() - 32.0 ) < 1e-6 );
	check( st, QStringLiteral( "(live) the Game Day row changes the Status line" ), e4 != e32 );
	check( st, QStringLiteral( "(save) the Moon row and Game Day 32 wrote their settings" ),
		QSettings().value( QLatin1StringView( "Settings/Render/Scene/Lookdev Moon" ) ).toBool()
		&& std::fabs( QSettings().value( QLatin1StringView( "Settings/Render/Scene/Lookdev Game Day" ) ).toDouble() - 32.0 ) < 1e-6 );

	for ( QCheckBox * b : { sky, sun, clouds, moon } )
		b->setChecked( false );
	day->setValue( 0.0 );
	hour->setValue( 12.0 );
	pump();
	check( st, QStringLiteral( "(live) every preview row back OFF reached the state" ),
		!wwLookdevSky() && !wwLookdevClouds() && !wwLookdevSun() && !wwLookdevMoon() );
}

void restartLeg( NifSkope * skope, WwScState & st )
{
	QRect want;
	if ( !quad( "WW_SCENE_TEST_EXPECT", want ) )
		return;
	QAction * a = skope->findChild<QAction *>( QStringLiteral( "aSceneWindow" ) );
	QWidget * w = skope->findChild<QWidget *>( QStringLiteral( "SceneWindow" ) );
	if ( !a || !w ) {
		check( st, QStringLiteral( "(floor) the Scene action and window exist" ), false );
		return;
	}
	QSettings s;
	check( st, QStringLiteral( "(floor) the previous launch's geometry is in the store" ),
		!s.value( QLatin1StringView( SceneWindow::geometryKey() ) ).toByteArray().isEmpty() );
	a->trigger();
	pump();
	const QRect g = w->geometry();
	check( st, QStringLiteral( "(restart) reopened after the restart at %1 == %2" ).arg( rectStr( g ), rectStr( want ) ), g == want );
	auto * mode = w->findChild<QComboBox *>( QStringLiteral( "sceneMode" ) );
	auto * ev = w->findChild<QDoubleSpinBox *>( QStringLiteral( "sceneExposure" ) );
	auto * view = w->findChild<QComboBox *>( QStringLiteral( "sceneViewTransform" ) );
	bool okEv = false;
	const double wantEv = qEnvironmentVariable( "WW_SCENE_TEST_EXPECT_EV" ).toDouble( &okEv );
	if ( okEv && ev )
		check( st, QStringLiteral( "(restart) the exposure came back: %1 == %2 EV" ).arg( ev->value() ).arg( wantEv ),
			std::fabs( ev->value() - wantEv ) < 1e-6 && std::fabs( double( wwSceneExposureEV() ) - wantEv ) < 1e-6 );
	bool okView = false;
	const int wantView = qEnvironmentVariable( "WW_SCENE_TEST_EXPECT_VIEW" ).toInt( &okView );
	if ( okView && view )
		check( st, QStringLiteral( "(restart) the view transform came back: %1 == %2" ).arg( view->currentIndex() ).arg( wantView ),
			view->currentIndex() == wantView && wwSceneViewTransform() == wantView );
	if ( mode )
		check( st, QStringLiteral( "(Q7) the lighting mode starts Legacy, never restored" ),
			mode->currentIndex() == 0 && wwSceneMode() == WwSceneLegacy );
	a->trigger();
	pump();
}

void finish( NifSkope * skope, WwScState * st )
{
	if ( st->done )
		return;
	st->done = true;
	say( *st, QStringLiteral( "%1 checks, %2 failures" ).arg( st->checks ).arg( st->fails ) );
	say( *st, st->fails == 0 && st->checks > 0 ? QStringLiteral( "PASS" ) : QStringLiteral( "FAIL" ) );
	say( *st, QStringLiteral( "done" ) );
	if ( skope ) {
		if ( NifModel * n = skope->getNifModel(); n && n->undoStack )
			n->undoStack->setClean();
		skope->setWindowModified( false );
	}
	delete st;
	QTimer::singleShot( 0, qApp, [skope]() {
		if ( skope )
			skope->close();
		QTimer::singleShot( 200, qApp, []() { QCoreApplication::exit( 0 ); } );
	} );
}

void run( NifSkope * skope, WwScState * st )
{
	say( *st, QStringLiteral( "--- WW_SCENE_TEST: Studio cube, Scene window (lane PBRR2A) ---" ) );
	const QString suffix = wwHarnessSettingsSuffix();
	say( *st, QStringLiteral( "  settings scope: '%1'; %2" ).arg( suffix.trimmed(), wwSceneEcho() ) );
	const bool scoped = !suffix.isEmpty();
	check( *st, QStringLiteral( "(floor) the run is inside a scratch WW_SETTINGS_SCOPE" ), scoped );
	if ( !scoped ) {
		say( *st, QStringLiteral( "  REFUSED: no scratch scope, nothing read or written" ) );
		return;
	}
	cubeLeg( skope, *st );
	if ( qEnvironmentVariable( "WW_SCENE_TEST_WINDOW" ) == QLatin1StringView( "1" ) )
		windowLeg( skope, *st );
	if ( qEnvironmentVariable( "WW_SCENE_TEST_LOOKDEV" ) == QLatin1StringView( "1" ) )
		lookdevLeg( skope, *st );
	if ( qEnvironmentVariable( "WW_SCENE_TEST_WEATHER" ) == QLatin1StringView( "1" ) )
		weatherLeg( skope, *st );
	restartLeg( skope, *st );
}

} // namespace

void wwSceneHarness( NifSkope * skope )
{
	if ( !skope || !qEnvironmentVariableIsSet( "WW_SCENE_TEST" ) )
		return;
	auto * st = new WwScState;
	st->logPath = qEnvironmentVariable( "WW_SCENE_TEST_LOG" );
	if ( st->logPath.isEmpty() )
		st->logPath = QCoreApplication::applicationDirPath() + QStringLiteral( "/ww_scene_test.log" );
	QFile::remove( st->logPath );
	// 3 s: after the NIF the driver passes has loaded and painted once
	QTimer::singleShot( 3000, skope, [skope, st]() {
		run( skope, st );
		finish( skope, st );
	} );
}
