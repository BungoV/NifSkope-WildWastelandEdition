/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#include "impostorpreviewtest.h"
#include "wwswitch.h"	// the WW_* switch store

#include "gl/glscene.h"
#include "features/far-lod/impostordraw.h"
#include "glview.h"
#include "impostorcard.h"
#include "impostoroct.h"
#include "nifskope.h"

#include <QApplication>
#include <QDockWidget>
#include <QFile>
#include <QImage>
#include <QTimer>

#include <cmath>
#include <cstdio>	// fflush, for the redirected streams a caller reads
#include <cstdlib>	// std::_Exit -- see endRun()

//! nifskope_ui.cpp: the card bake's .pbrm retarget, applied to the preview's mesh (harness only)
QStringList wwPbrmRetargetScene( Scene * sc, const QString & looseRoot );

// WW_IMPOSTOR_PREVIEW -- what "actually works" is allowed to mean.

namespace {

enum class Mode { None, Map, Iou, IouShuffled, Azimuth, Channels, Show, Pair, Orbit, Distance };

//! The eight named azimuths of the gate's row (a), and the two elevations.
//! Named, not generated, so a failure names a direction a person can picture.
struct NamedView { const char * name; float azimDeg; float elevDeg; };
const NamedView kViews[16] = {
	{ "E-low",   0.0f, 15.0f }, { "NE-low",  45.0f, 15.0f },
	{ "N-low",  90.0f, 15.0f }, { "NW-low", 135.0f, 15.0f },
	{ "W-low", 180.0f, 15.0f }, { "SW-low", 225.0f, 15.0f },
	{ "S-low", 270.0f, 15.0f }, { "SE-low", 315.0f, 15.0f },
	{ "E-high",  0.0f, 55.0f }, { "NE-high",  45.0f, 55.0f },
	{ "N-high", 90.0f, 55.0f }, { "NW-high", 135.0f, 55.0f },
	{ "W-high",180.0f, 55.0f }, { "SW-high", 225.0f, 55.0f },
	{ "S-high",270.0f, 55.0f }, { "SE-high", 315.0f, 55.0f }
};

struct State
{
	Mode mode = Mode::None;
	bool armed = false;			//!< the card may be drawn
	bool suppressScene = false;	//!< the card ALONE is being photographed
	bool registered = false;	//!< the bake's tree has reached the resource list
	ImpostorCardSet set;
	ImpostorDraw::Options opt;
	Vector3 offset;
	QStringList log;

	// THE SECOND SET, for `pair`.
	ImpostorCardSet set2;
	Vector3 offset2;
	bool haveSecond = false;
	bool registered2 = false;
	//! THE RED CONTROL for the pair row.
	int forceN = 0;
};

State & state()
{
	static State s;
	return s;
}

Mode parseMode( const QString & s )
{
	if ( s.compare( QLatin1String( "map" ), Qt::CaseInsensitive ) == 0 )
		return Mode::Map;
	if ( s.compare( QLatin1String( "iou" ), Qt::CaseInsensitive ) == 0 )
		return Mode::Iou;
	if ( s.compare( QLatin1String( "iou-shuffled" ), Qt::CaseInsensitive ) == 0 )
		return Mode::IouShuffled;
	if ( s.compare( QLatin1String( "azimuth" ), Qt::CaseInsensitive ) == 0 )
		return Mode::Azimuth;
	if ( s.compare( QLatin1String( "channels" ), Qt::CaseInsensitive ) == 0 )
		return Mode::Channels;
	if ( s.compare( QLatin1String( "show" ), Qt::CaseInsensitive ) == 0 || s == QLatin1String( "1" ) )
		return Mode::Show;
	if ( s.compare( QLatin1String( "pair" ), Qt::CaseInsensitive ) == 0 )
		return Mode::Pair;
	if ( s.compare( QLatin1String( "orbit" ), Qt::CaseInsensitive ) == 0 )
		return Mode::Orbit;
	if ( s.compare( QLatin1String( "distance" ), Qt::CaseInsensitive ) == 0 )
		return Mode::Distance;
	return Mode::None;
}

//! Put the bake's own folder on the resource list, once, and say what happened.
void registerNow( GLView * ogl )
{
	State & s = state();
	if ( s.registered || !ogl )
		return;
	s.registered = true;
	const QString root = ImpostorDraw::registerLooseSheets( ogl->getScene(), s.set );
	if ( !root.isEmpty() )
		s.log << QStringLiteral( "registered loose data folder: %1" ).arg( root );
	else if ( s.set.colour.fromLocal )
		s.log << QStringLiteral( "WARNING: the sheets sit beside the .lodm but no ancestor of"
				" %1 holds a textures\\ subtree -- the texture cache has no absolute-path"
				" fallback, so the card will draw blank. Bake under a data folder: the sheet"
				" the .lodm names is \"%2\", which the resource stack reads as"
				" textures\\<that>." )
				.arg( QFileInfo( s.set.lodmPath ).absolutePath(), s.set.colour.gamePath );

	// And say, in the log, whether the resource stack can actually answer for the colour sheet under the name
	// the `.lodm` gives it.
	const Scene * scene = ogl->getScene();
	if ( scene && scene->nifModel && !s.set.colour.gamePath.isEmpty() ) {
		const QString found = scene->nifModel->findResourceFile(
				s.set.colour.gamePath, "textures", ".dds" );
		s.log << QStringLiteral( "resource lookup \"%1\" -> %2" )
				.arg( s.set.colour.gamePath,
					found.isEmpty() ? QStringLiteral( "NOT FOUND" ) : found );
	}

	// THE SECOND SET'S FOLDER TOO, and separately.
	if ( s.haveSecond && !s.registered2 ) {
		s.registered2 = true;
		const QString root2 = ImpostorDraw::registerLooseSheets( ogl->getScene(), s.set2 );
		s.log << QStringLiteral( "set B loose data folder: %1" )
				.arg( root2.isEmpty() ? QStringLiteral( "NONE -- its card will draw blank" ) : root2 );
	}
}

void writeLog()
{
	const QString out = wwEnvStr( "WW_IMPOSTOR_LOG" );
	if ( out.isEmpty() )
		return;
	QFile f( out );
	if ( f.open( QIODevice::WriteOnly | QIODevice::Text ) )
		f.write( state().log.join( QStringLiteral( "\n" ) ).toUtf8() + "\n" );
}

//! END THE PROCESS, here, with the log already on disk. Three routes were MEASURED on 2026-09-19 against this
//! exe, and this is the one that ends.
void endRun( int code )
{
	writeLog();
	fflush( nullptr );      // the redirected stdout/stderr a caller reads
	std::_Exit( code );
}

//! Force the window and the viewport, then REPORT what was obtained.
QSize forceWindow( NifSkope * skope, GLView * ogl, QWidget * viewportHeader )
{
	int rw = 1024, rh = 1024;
	const QString szEnv = wwEnvStr( "WW_RENDER_SIZE" );
	if ( szEnv.contains( QLatin1Char( 'x' ) ) ) {
		const QStringList parts = szEnv.split( QLatin1Char( 'x' ) );
		if ( parts.size() == 2 ) {
			rw = qMax( parts.at( 0 ).toInt(), 320 );
			rh = qMax( parts.at( 1 ).toInt(), 240 );
		}
	}
	// `showNormal()` ALONE DOES NOT CLEAR A MAXIMIZED STATE that arrived with the persisted geometry, and a
	// resize applied to a maximized window is discarded without a word.
	skope->setWindowState( skope->windowState()
			& ~( Qt::WindowMaximized | Qt::WindowFullScreen | Qt::WindowMinimized ) );
	skope->showNormal();
	for ( QDockWidget * dw : skope->findChildren<QDockWidget *>() )
		dw->hide();
	if ( viewportHeader )
		viewportHeader->hide();
	skope->resize( rw, rh );
	qApp->processEvents();

	// And the WINDOW is not the FRAMEBUFFER: the GL surface gets whatever the menu bar, the status bar and the
	// frame leave over, so a window of WxH yields a smaller grab. Add the difference back and settle.
	QImage probe = ogl->grabFramebuffer();
	for ( int pass = 0; pass < 3 && probe.size() != QSize( rw, rh ); pass++ ) {
		const QSize win = skope->size();
		const int dw = rw - probe.width();
		const int dh = rh - probe.height();
		if ( !dw && !dh )
			break;
		skope->resize( qMax( 320, win.width() + dw ), qMax( 240, win.height() + dh ) );
		qApp->processEvents();
		probe = ogl->grabFramebuffer();
	}

	// The size that MATTERS is the one the grab returns, not the one asked
	// for. Read it back off a real frame.
	return probe.size();
}

//! The camera angles for a direction, in the bake's own terms (src/nifskope_ui.cpp:22755..22757).
void bakeAngles( float azimDeg, float elevDeg, float & rx, float & rz, bool legacy = false )
{
	rx = -90.0f + elevDeg;
	rz = ( legacy ? 90.0f : 270.0f ) - azimDeg;
}

//! Point the camera at the card, orthographically, framed on the card's own extents so the mesh and the card
//! are photographed at the same scale.
void aimCamera( GLView * ogl, const ImpostorCardSet & set, const Vector3 & offset,
				float azimDeg, float elevDeg, float widen = 1.0f,
				const Vector3 * centerOverride = nullptr )
{
	GLView::WwCameraPin pin;
	pin.active = true;
	pin.haveCenter = true;
	pin.center = centerOverride ? *centerOverride
			: offset + Vector3( set.center[0], set.center[1], set.center[2] );
	pin.haveDist = true;
	pin.dist = 4.0f * widen * qMax( set.halfW, set.halfH );
	pin.haveOrtho = true;
	// Orthographic, at the card's own half-width. The BAKE was orthographic (spec 225..239), so a perspective
	// comparison would charge the card for a projection it was never given.
	pin.orthoHalfWidth = 1.02f * widen * qMax( set.halfW, set.halfH );

	ogl->wwApplyCameraPin( pin );

	// AND the rotation, after the pin. `WwCameraPin` re-asserts what it was given on every later paint and it
	// was given no view, so the rotation set here should survive.
	float rx = 0.0f, rz = 0.0f;
	bakeAngles( azimDeg, elevDeg, rx, rz );
	ogl->setRotation( rx, 0.0f, rz );
	ogl->update();
	qApp->processEvents();
}

//! The silhouette of what was drawn: every pixel that is not the clear colour.
QVector<uchar> silhouette( const QImage & img, const QColor & clear, int & covered )
{
	const QImage rgb = img.convertToFormat( QImage::Format_RGB888 );
	QVector<uchar> mask( rgb.width() * rgb.height(), 0 );
	covered = 0;
	const int cr = clear.red(), cg = clear.green(), cb = clear.blue();
	for ( int y = 0; y < rgb.height(); y++ ) {
		const uchar * line = rgb.constScanLine( y );
		for ( int x = 0; x < rgb.width(); x++ ) {
			const int r = line[x * 3 + 0], g = line[x * 3 + 1], b = line[x * 3 + 2];
			// 12 per channel: JPEG-free grabs are exact, but the clear colour
			// goes through the same sRGB path the scene does and comes back a
			// value or two off on some drivers.
			const bool bg = ( std::abs( r - cr ) <= 12 && std::abs( g - cg ) <= 12 && std::abs( b - cb ) <= 12 );
			if ( !bg ) {
				mask[y * rgb.width() + x] = 1;
				covered++;
			}
		}
	}
	return mask;
}

//! MEAN COLOUR ERROR, card against mesh, over the pixels BOTH cover.
double colourError( const QImage & a, const QImage & b,
		const QVector<uchar> & ma, const QVector<uchar> & mb, int & overlap )
{
	overlap = 0;
	const QImage x = a.convertToFormat( QImage::Format_RGB888 );
	const QImage y = b.convertToFormat( QImage::Format_RGB888 );
	if ( x.size() != y.size() || ma.size() != mb.size()
			|| ma.size() != x.width() * x.height() )
		return -1.0;
	double sum = 0.0;
	for ( int row = 0; row < x.height(); row++ ) {
		const uchar * la = x.constScanLine( row );
		const uchar * lb = y.constScanLine( row );
		for ( int col = 0; col < x.width(); col++ ) {
			const int i = row * x.width() + col;
			if ( !ma.at( i ) || !mb.at( i ) )
				continue;
			sum += ( std::abs( int( la[col * 3 + 0] ) - int( lb[col * 3 + 0] ) )
					+ std::abs( int( la[col * 3 + 1] ) - int( lb[col * 3 + 1] ) )
					+ std::abs( int( la[col * 3 + 2] ) - int( lb[col * 3 + 2] ) ) ) / 3.0;
			overlap++;
		}
	}
	return ( overlap > 0 ) ? ( sum / double( overlap ) ) / 255.0 : -1.0;
}

double iouOf( const QVector<uchar> & a, const QVector<uchar> & b )
{
	if ( a.size() != b.size() || a.isEmpty() )
		return -1.0;
	qint64 inter = 0, uni = 0;
	for ( int i = 0; i < a.size(); i++ ) {
		const bool x = a.at( i ) != 0, y = b.at( i ) != 0;
		if ( x && y )
			inter++;
		if ( x || y )
			uni++;
	}
	return ( uni > 0 ) ? double( inter ) / double( uni ) : -1.0;
}

} // namespace

bool wwImpostorPreviewActive()
{
	return state().armed;
}

bool wwImpostorPreviewSuppressScene()
{
	return state().armed && state().suppressScene;
}

void wwImpostorPreviewDraw( Scene * scene )
{
	State & s = state();
	if ( !s.armed || !scene || !s.set.ok )
		return;
	// NOT BEFORE THE SHEETS ARE REACHABLE.
	if ( !s.registered )
		return;
	// BOTH CARDS, each with its OWN set, in the one pass.
	if ( s.haveSecond && s.set2.ok && s.registered2 ) {
		QString why2;
		if ( !ImpostorDraw::drawCard( scene, s.set2, s.offset2, s.opt, &why2 ) ) {
			static QString said2;
			if ( said2 != why2 ) {
				said2 = why2;
				s.log << QStringLiteral( "set B draw REFUSED: %1" ).arg( why2 );
				writeLog();
			}
		}
	}

	QString why;
	if ( !ImpostorDraw::drawCard( scene, s.set, s.offset, s.opt, &why ) ) {
		// Once, not once a frame: a card that cannot draw says so and stops
		// saying it, because a message printed at 60 Hz is a message nobody
		// reads and a log nobody can diff.
		static QString said;
		if ( said != why ) {
			said = why;
			s.log << QStringLiteral( "draw REFUSED: %1" ).arg( why );
			writeLog();
		}
	}
}

bool wwImpostorPreviewStart( NifSkope * skope, GLView * ogl, QWidget * viewportHeader )
{
	const QString modeEnv = wwEnvStr( "WW_IMPOSTOR_PREVIEW" );
	if ( modeEnv.isEmpty() || !skope || !ogl )
		return false;

	State & s = state();
	s.mode = parseMode( modeEnv );
	if ( s.mode == Mode::None ) {
		s.log << QStringLiteral( "REFUSED: WW_IMPOSTOR_PREVIEW=%1 is not one of"
				" map, iou, iou-shuffled, azimuth, channels, show, pair" ).arg( modeEnv );
		// A REFUSAL ENDS THE PROCESS TOO.
		writeLog();
		endRun( 2 );
		return false;
	}

	const QString lodm = wwEnvStr( "WW_IMPOSTOR_LODM" );
	if ( lodm.isEmpty() ) {
		s.log << QStringLiteral( "REFUSED: WW_IMPOSTOR_PREVIEW=%1 with no WW_IMPOSTOR_LODM."
				" This harness does not bake; point it at a baked <id>_oct.lodm." ).arg( modeEnv );
		writeLog();
		endRun( 2 );
		return false;
	}

	const int layer = wwEnvIsSet( "WW_IMPOSTOR_LAYER" )
			? wwEnvInt( "WW_IMPOSTOR_LAYER" ) : -1;
	s.set = impostorCardLoad( lodm, layer );
	s.log << s.set.notes();
	if ( !s.set.ok ) {
		writeLog();
		endRun( 2 );
		return false;
	}

	// The options, all of them nameable from the environment so a picture can
	// be reproduced from its own log line and nothing is a hidden default.
	s.opt.depthOffset  = wwEnvInt( "WW_IMPOSTOR_DEPTH" ) != 0
			|| !wwEnvIsSet( "WW_IMPOSTOR_DEPTH" );
	s.opt.heightBlend  = wwEnvInt( "WW_IMPOSTOR_BLEND" ) != 0
			|| !wwEnvIsSet( "WW_IMPOSTOR_BLEND" );
	s.opt.bakedAo      = wwEnvInt( "WW_IMPOSTOR_AO" ) != 0
			|| !wwEnvIsSet( "WW_IMPOSTOR_AO" );
	s.opt.debugChannel = wwEnvInt( "WW_IMPOSTOR_CHANNEL" );
	{
		const QString cut = wwEnvStr( "WW_IMPOSTOR_CUT" );
		s.opt.cutRule = cut.compare( QStringLiteral( "mean" ), Qt::CaseInsensitive ) == 0 ? 1
				: cut.compare( QStringLiteral( "strong" ), Qt::CaseInsensitive ) == 0 ? 2 : 0;
	}
	// the rule the DRAWER resolves, so the crisp end's own
	// cut (the strongest frame, by name) is what the log says.
	const int cutDrawn = s.opt.cutRule != 0 ? s.opt.cutRule : ImpostorDraw::resolve( s.opt ).cutRule;
	s.log << ( cutDrawn == 1
			? QStringLiteral( "cut rule: mean -- the 3-frame MEAN coverage (WW_IMPOSTOR_CUT=mean, the way back)" )
			: cutDrawn == 2 && s.opt.cutRule == 0
			? QStringLiteral( "cut rule: strong -- the STRONGEST frame alone: the crisp end's own cut (R5, the default)" )
			: cutDrawn == 2
			? QStringLiteral( "cut rule: strong -- the STRONGEST frame alone (WW_IMPOSTOR_CUT=strong, row 18's red control)" )
			: QStringLiteral( "cut rule: stipple -- each frame's own silhouette at density min(1, 2w), mean as the floor (IMPOSTORTEAR1)" ) );
	// the blend, the snap and the depth search, each said out loud (a BLEND=0 run used to leave no trace in
	// the log).
	const ImpostorDraw::Resolved rs = ImpostorDraw::resolve( s.opt );
	s.log << QStringLiteral( "slider: %1 -- %2%3" ).arg( double( rs.slider ), 0, 'f', 2 )
			.arg( rs.slider <= 0.0f ? QStringLiteral( "the CRISP end, flat snap" )
				: rs.slider >= 1.0f ? QStringLiteral( "the SMOOTH end, stipple + search" )
				: QStringLiteral( "between the ends, weights sharpened to the power %1" ).arg( double( rs.sharpen ), 0, 'f', 3 ) )
			.arg( rs.sliderForced ? QStringLiteral( " (WW_IMPOSTOR_SLIDER)" ) : QStringLiteral( " (the default)" ) );
	s.log << ( !s.opt.heightBlend
			? QStringLiteral( "frames: ONE, flat on the card plane (WW_IMPOSTOR_BLEND=0)" )
			: rs.snap && !rs.parallax
			? QStringLiteral( "frames: ONE, the nearest, flat on the card plane (the slider's crisp end)" )
			: rs.snap
			? QStringLiteral( "frames: ONE, the nearest, moved by its depth (%1, a harness override)" ).arg( rs.snapForced
				? QStringLiteral( "WW_IMPOSTOR_SNAP=1" ) : QStringLiteral( "Options::snap" ) )
			: QStringLiteral( "frames: THREE, height-blended" ) );
	s.log << QStringLiteral( "depth search: %1" ).arg( !rs.parallax
			? QStringLiteral( "none -- the frame is drawn flat, not moved by its depth" )
			: rs.searchSteps > 0
			? QStringLiteral( "%1 steps + 1 refinement (%2)" ).arg( rs.searchSteps )
				.arg( rs.searchForced ? QStringLiteral( "WW_IMPOSTOR_SEARCH" ) : QStringLiteral( "the slider's" ) )
			: QStringLiteral( "off -- the one-step parallax (%1)" )
				.arg( rs.searchForced ? QStringLiteral( "WW_IMPOSTOR_SEARCH" ) : QStringLiteral( "the slider's" ) ) );
	s.log << QStringLiteral( "coverage filter: DECODED per texel, then bilinear -- always (WW_IMPOSTOR_COVFILTER retired)" );
	if ( wwEnvIsSet( "WW_IMPOSTOR_ALPHA" ) )
		s.opt.alphaThreshold = float( wwEnvStr( "WW_IMPOSTOR_ALPHA" ).toDouble() );
	// The cut is in the DECODED fraction domain; a negative one means the set's
	// own `coverage.floor` decides. Said out loud so a picture names its cut.
	s.log << ( s.opt.alphaThreshold >= 0.0f
			? QStringLiteral( "coverage cut: %1 (forced by WW_IMPOSTOR_ALPHA)" )
					.arg( double( s.opt.alphaThreshold ), 0, 'f', 4 )
			: QStringLiteral( "coverage cut: vanilla's LOD alpha test 128/255 = %1 (the set's floor would be %2)" )
					.arg( double( kImpostorVanillaCut ), 0, 'f', 4 )
					.arg( s.set.covOk()
						? QStringLiteral( "%1/255 = %2" ).arg( s.set.covFloor )
								.arg( double( s.set.covFloor ) / 255.0, 0, 'f', 4 )
						: QStringLiteral( "undeclared" ) ) );
	// THE CONVENTION IS THE SET'S TO DECLARE, not the harness's to choose: the `.lodm`'s `conv` token says
	// which bake made it and `drawCard` follows it.
	if ( wwEnvIsSet( "WW_IMPOSTOR_CONVENTION" ) ) {
		const QString c = wwEnvStr( "WW_IMPOSTOR_CONVENTION" ).trimmed().toLower();
		if ( c == QLatin1String( "spec" ) || c == QLatin1String( "spec1" ) ) {
			s.opt.forceConvention = true;
			s.opt.convention = ImpostorOct::Convention::SpecLiteral;
		} else if ( c == QLatin1String( "asbaked" ) || c == QLatin1String( "legacy" ) ) {
			s.opt.forceConvention = true;
			s.opt.convention = ImpostorOct::Convention::AsBaked;
		} else {
			s.log << QStringLiteral( "REFUSED: WW_IMPOSTOR_CONVENTION=%1 is not spec or asbaked."
					" Not guessing -- the set's own token is being used instead." ).arg( c );
		}
	}
	s.log << QStringLiteral( "set convention token: %1%2" )
			.arg( s.set.conv.isEmpty() ? QStringLiteral( "NONE -- legacy set, baked before the"
					" 2026-09-19 azimuth repair, RE-BAKE IT" ) : s.set.conv,
				  s.opt.forceConvention
					? QStringLiteral( " (OVERRIDDEN by WW_IMPOSTOR_CONVENTION)" ) : QString() );
	// THE RED CONTROL, reachable from EVERY mode.
	s.opt.shuffleFrames = ( s.mode == Mode::IouShuffled )
			|| wwEnvInt( "WW_IMPOSTOR_SHUFFLE" ) != 0;
	if ( s.opt.shuffleFrames )
		s.log << QStringLiteral( "SHUFFLED: the frame choice is deliberately wrong (red control)" );

	/* THE SWAY, on request: the drawer's wind shear at a fixed
	 * amplitude and phase, so a run of phases makes a moving picture. 0 = still. */
	s.opt.swayAmplitude = float( wwEnvStr( "WW_IMPOSTOR_SWAY_AMP" ).toDouble() );
	s.opt.swayPhase = float( wwEnvStr( "WW_IMPOSTOR_SWAY_PHASE" ).toDouble() );
	if ( s.opt.swayAmplitude != 0.0f )
		s.log << QStringLiteral( "sway: amplitude %1, phase %2 rad" ).arg( s.opt.swayAmplitude ).arg( s.opt.swayPhase );

	/* THE SECOND SET. Loaded for every mode, not just `pair`, so a `show` run
	 * can put two grids on the screen for a picture; only `pair` measures it. */
	const QString lodm2 = wwEnvStr( "WW_IMPOSTOR_LODM2" );
	if ( !lodm2.isEmpty() ) {
		s.set2 = impostorCardLoad( lodm2, layer );
		for ( const QString & line : s.set2.notes() )
			s.log << QStringLiteral( "B| %1" ).arg( line );
		if ( !s.set2.ok ) {
			s.log << QStringLiteral( "REFUSED: WW_IMPOSTOR_LODM2 did not load: %1" ).arg( s.set2.error );
			writeLog();
			endRun( 2 );
			return false;
		}
		s.haveSecond = true;
		s.forceN = wwEnvInt( "WW_IMPOSTOR_FORCE_N" );
		if ( s.forceN >= 2 && s.forceN <= 16 && s.forceN != s.set2.oct ) {
			s.log << QStringLiteral( "FORCED N: set B's grid is %1 but is being read as %2"
					" (red control -- a GLOBAL N is this bug)" ).arg( s.set2.oct ).arg( s.forceN );
			s.set2.oct = s.forceN;
		} else {
			s.forceN = 0;
		}
		// Side by side, along +X, with a gap.
		s.offset2 = Vector3( 2.5f * ( s.set.halfW + s.set2.halfW ), 0.0f, 0.0f );
		s.log << QStringLiteral( "pair: A oct %1 frame %2x%3 half %4x%5 | B oct %6 frame %7x%8 half %9x%10" )
				.arg( s.set.oct ).arg( s.set.frameW ).arg( s.set.frameH )
				.arg( double( s.set.halfW ), 0, 'f', 2 ).arg( double( s.set.halfH ), 0, 'f', 2 )
				.arg( s.set2.oct ).arg( s.set2.frameW ).arg( s.set2.frameH )
				.arg( double( s.set2.halfW ), 0, 'f', 2 ).arg( double( s.set2.halfH ), 0, 'f', 2 );
	} else if ( s.mode == Mode::Pair ) {
		s.log << QStringLiteral( "REFUSED: WW_IMPOSTOR_PREVIEW=pair needs a SECOND set in"
				" WW_IMPOSTOR_LODM2. One card cannot show that N is per-card." );
		writeLog();
		endRun( 2 );
		return false;
	}

	s.armed = true;

	// REGISTERING THE BAKE'S TREE HAPPENS AFTER THE NIF IS OPEN, not here.

	if ( s.mode == Mode::Show ) {
		s.log << QStringLiteral( "mode show: armed, staying open" );
		QTimer::singleShot( 1500, skope, [ogl]() {
			registerNow( ogl );
			writeLog();
			ogl->update();
		} );
		writeLog();
		return true;
	}

	// Everything below MEASURES, so it runs once the first frame exists and
	// then quits. A timer rather than a direct call: the GL widget has not
	// been shown yet at the point this is called from startup.
	QTimer::singleShot( 1500, skope, [skope, ogl, viewportHeader]() {
		State & st = state();
		registerNow( ogl );
		const QSize got = forceWindow( skope, ogl, viewportHeader );
		st.log << QStringLiteral( "viewport %1x%2" ).arg( got.width() ).arg( got.height() );
		st.log << QStringLiteral( "requested %1" ).arg(
				wwEnvStr( "WW_RENDER_SIZE", QStringLiteral( "1024x1024 (default)" ) ) );

		const QColor clear = ogl->clearColor().toQColor();
		Scene * scene = ogl->getScene();

		// VIEWER CHROME IS NOT PART OF THE SUBJECT, and leaving it on was a measurement fault, not a cosmetic
		// one.
		if ( scene )
			scene->options &= ~Scene::SceneOptions( Scene::ShowAxes | Scene::ShowGrid );
		ogl->showCursor = false;
		st.log << QStringLiteral( "chrome off: nav gizmo, 3D cursor and GRID suppressed" );
		// THE GRID IS THE WORSE HALF OF THE SAME FAULT, and it biases the other way -- downwards, at four
		// directions only, which is how it survived: it looks like a defect in the card.

		if ( st.mode == Mode::Map ) {
			for ( const NamedView & v : kViews ) {
				aimCamera( ogl, st.set, st.offset, v.azimDeg, v.elevDeg );
				st.log << QStringLiteral( "view %1 azim %2 elev %3" )
						.arg( QLatin1String( v.name ) ).arg( double( v.azimDeg ) ).arg( double( v.elevDeg ) );
				for ( const QString & line : ImpostorDraw::describeSelection( scene, st.set, st.offset, st.opt ) )
					st.log << QStringLiteral( "  %1" ).arg( line );
			}
		} else if ( st.mode == Mode::Azimuth ) {
			// THE REPAIR'S GATE ROW.
			const float elev = wwEnvIsSet( "WW_IMPOSTOR_ELEV" )
					? float( wwEnvStr( "WW_IMPOSTOR_ELEV" ).toDouble() ) : 15.0f;
			const QString stem = wwEnvStr( "WW_IMPOSTOR_SHOT" );
			double sumSame = 0.0, sumOpp = 0.0, worst = 1e9;
			int counted = 0, positive = 0;
			for ( int k = 0; k < 8; k++ ) {
				const NamedView & v = kViews[k];
				const float azim = v.azimDeg;

				// (1) the card alone, from the spec direction of its frames
				aimCamera( ogl, st.set, st.offset, azim, elev );
				st.suppressScene = true;
				ogl->update();
				qApp->processEvents();
				const QImage cardImg = ogl->grabFramebuffer();
				st.suppressScene = false;

				// (2) the mesh alone, same camera
				st.armed = false;
				ogl->update();
				qApp->processEvents();
				const QImage meshSame = ogl->grabFramebuffer();

				// (3) the mesh alone, the opposite azimuth
				aimCamera( ogl, st.set, st.offset, azim + 180.0f, elev );
				ogl->update();
				qApp->processEvents();
				const QImage meshOpp = ogl->grabFramebuffer();
				st.armed = true;

				int cardCov = 0, sameCov = 0, oppCov = 0;
				const QVector<uchar> c = silhouette( cardImg, clear, cardCov );
				const QVector<uchar> ms = silhouette( meshSame, clear, sameCov );
				const QVector<uchar> mo = silhouette( meshOpp, clear, oppCov );
				const double fracSame = double( sameCov ) / double( qMax( 1, ms.size() ) );
				const double iouSame = iouOf( c, ms );
				const double iouOpp  = iouOf( c, mo );

				st.log << QStringLiteral( "azimuth %1 deg (%2) elev %3  card %4  same %5  opposite %6  margin %7" )
						.arg( double( azim ) ).arg( QLatin1String( v.name ) ).arg( double( elev ) )
						.arg( double( cardCov ) / double( qMax( 1, c.size() ) ), 0, 'f', 4 )
						.arg( iouSame, 0, 'f', 4 ).arg( iouOpp, 0, 'f', 4 )
						.arg( iouSame - iouOpp, 0, 'f', 4 );

				if ( !stem.isEmpty() ) {
					const QString tag = QStringLiteral( "%1_az%2" ).arg( stem ).arg( int( azim ), 3, 10, QLatin1Char( '0' ) );
					cardImg.save( tag + QStringLiteral( "_card.png" ) );
					meshSame.save( tag + QStringLiteral( "_mesh.png" ) );
					meshOpp.save( tag + QStringLiteral( "_mesh_opposite.png" ) );
				}

				if ( sameCov == 0 || oppCov == 0 || fracSame > 0.95 ) {
					st.log << QStringLiteral( "  EXCLUDED: mesh coverage %1 is degenerate" )
							.arg( fracSame, 0, 'f', 4 );
					continue;
				}
				if ( iouSame < 0.0 || iouOpp < 0.0 )
					continue;
				sumSame += iouSame;
				sumOpp  += iouOpp;
				worst = qMin( worst, iouSame - iouOpp );
				if ( iouSame > iouOpp )
					positive++;
				counted++;
			}
			if ( counted > 0 ) {
				st.log << QStringLiteral( "azimuth same mean %1" ).arg( sumSame / double( counted ), 0, 'f', 4 );
				st.log << QStringLiteral( "azimuth opposite mean %1" ).arg( sumOpp / double( counted ), 0, 'f', 4 );
				st.log << QStringLiteral( "azimuth worst margin %1" ).arg( worst, 0, 'f', 4 );
				// THE MEAN AND THE COUNT, because the worst alone is the wrong question to put to a real
				// subject.
				st.log << QStringLiteral( "azimuth mean margin %1" )
						.arg( ( sumSame - sumOpp ) / double( counted ), 0, 'f', 4 );
				st.log << QStringLiteral( "azimuth positive %1 of %2" ).arg( positive ).arg( counted );
			} else {
				st.log << QStringLiteral( "azimuth same mean REFUSED: every view was degenerate --"
						" is the baked object's own NIF open?" );
			}
			st.log << QStringLiteral( "azimuths counted %1 of 8" ).arg( counted );
		} else if ( st.mode == Mode::Pair ) {
			// THE VARIABLE-GRID ROW. Two sets, different N, one scene, one camera.
			const Vector3 mid = ( st.offset + Vector3( st.set.center[0], st.set.center[1], st.set.center[2] )
					+ st.offset2 + Vector3( st.set2.center[0], st.set2.center[1], st.set2.center[2] ) ) / 2.0f;
			const float widen = ( st.offset2[0] + qMax( st.set2.halfW, st.set2.halfH )
					+ qMax( st.set.halfW, st.set.halfH ) )
					/ qMax( 1.0f, qMax( st.set.halfW, st.set.halfH ) );
			for ( const NamedView & v : kViews ) {
				aimCamera( ogl, st.set, st.offset, v.azimDeg, v.elevDeg, widen, &mid );
				st.log << QStringLiteral( "pair view %1 azim %2 elev %3" )
						.arg( QLatin1String( v.name ) ).arg( double( v.azimDeg ) ).arg( double( v.elevDeg ) );
				for ( const QString & line : ImpostorDraw::describeSelection( scene, st.set, st.offset, st.opt ) )
					st.log << QStringLiteral( "  A %1" ).arg( line );
				for ( const QString & line : ImpostorDraw::describeSelection( scene, st.set2, st.offset2, st.opt ) )
					st.log << QStringLiteral( "  B %1" ).arg( line );
			}
			// And one picture, so the row has a thing a person can look at.
			const QString stem = wwEnvStr( "WW_IMPOSTOR_SHOT" );
			if ( !stem.isEmpty() ) {
				aimCamera( ogl, st.set, st.offset, 45.0f, 20.0f, widen, &mid );
				st.suppressScene = true;
				ogl->update();
				qApp->processEvents();
				const QImage img = ogl->grabFramebuffer();
				st.suppressScene = false;
				const QString out = QStringLiteral( "%1_pair.png" ).arg( stem );
				st.log << QStringLiteral( "pair picture -> %1 (%2)" ).arg( out )
						.arg( img.save( out ) ? QStringLiteral( "written" ) : QStringLiteral( "SAVE FAILED" ) );
			}
			st.log << QStringLiteral( "pair forced-n %1" ).arg( st.forceN );
		} else if ( st.mode == Mode::Orbit ) {
			/* THE ORBIT. Step 3 of the brief asks for a strip of 12 azimuths at
			 * two elevations, mesh on top and card underneath, and an animated
			 * version of the same thing. Both are built OUTSIDE this file from
			 * the PNGs written here, because composing a strip is a job for a
			 * picture library and not for a GL widget.
			 *
			 * The azimuth count is read, not fixed at 12. Nothing in this
			 * preview may assume a grid size, and a show that samples the orbit
			 * at exactly the grid's own rate would hide the blend by never
			 * landing between frames -- with N=4 and 12 azimuths, two views in
			 * three are mid-cell, which is the point.
			 *
			 * MESH FIRST, THEN CARD, at one camera per view: the pair is the
			 * comparison, and a camera that moved between the two halves would
			 * make every number here a measure of the camera. */
			const QString stem = wwEnvStr( "WW_IMPOSTOR_SHOT",
					QApplication::applicationDirPath() + QStringLiteral( "/ww_impostor_orbit" ) );
			const int nAz = qBound( 1, wwEnvIsSet( "WW_IMPOSTOR_ORBIT_AZ" )
					? wwEnvInt( "WW_IMPOSTOR_ORBIT_AZ" ) : 12, 64 );
			QList<float> elevs;
			for ( const QString & piece : wwEnvStr( "WW_IMPOSTOR_ORBIT_ELEV",
					QStringLiteral( "15,45" ) ).split( QLatin1Char( ',' ), Qt::SkipEmptyParts ) ) {
				bool okNum = false;
				const float e = float( piece.trimmed().toDouble( &okNum ) );
				if ( okNum )
					elevs << e;
			}
			if ( elevs.isEmpty() )
				elevs << 15.0f;

			// THE KNOWN-ANSWER CONTROL'S OWN VIEWS.
			QList<QPair<float, float>> views;
			for ( const QString & piece : wwEnvStr( "WW_IMPOSTOR_ORBIT_VIEWS" )
					.split( QLatin1Char( ',' ), Qt::SkipEmptyParts ) ) {
				const QStringList ab = piece.trimmed().split( QLatin1Char( ':' ) );
				if ( ab.size() != 2 )
					continue;
				bool okA = false, okE = false;
				const float a = float( ab[0].toDouble( &okA ) );
				const float e = float( ab[1].toDouble( &okE ) );
				if ( okA && okE )
					views << qMakePair( a, e );
			}
			const bool explicitViews = !views.isEmpty();
			if ( !explicitViews ) {
				for ( float elev : elevs )
					for ( int k = 0; k < nAz; k++ )
						views << qMakePair( 360.0f * float( k ) / float( nAz ), elev );
			}

			st.log << ( explicitViews
					? QStringLiteral( "orbit %1 EXPLICIT views (WW_IMPOSTOR_ORBIT_VIEWS), stem %2" )
							.arg( views.size() ).arg( stem )
					: QStringLiteral( "orbit %1 azimuths x %2 elevations, stem %3" )
							.arg( nAz ).arg( elevs.size() ).arg( stem ) );

			// THE LIGHTING GATE'S TWO SWITCHES:
			// WW_IMPOSTOR_MESH_CHANNEL=n photographs the MESH half of every pair through the LOD preview
			// channel n (8 = the view-space normal, the same picture the bake stores), and restores the
			// channel before the card half, so the card is untouched by it.
			const int meshChannel = wwEnvInt( "WW_IMPOSTOR_MESH_CHANNEL" );
			// WW_IMPOSTOR_MESH_PBRM=1 (IMPOSTORPBRM1's pictures): the mesh takes the bake's .pbrm retarget, so
			// channel 10 on the mesh half is the source's own roughness / metallic.
			if ( wwEnvInt( "WW_IMPOSTOR_MESH_PBRM" ) == 1 ) {
				for ( const QString & l : wwPbrmRetargetScene( ogl->getScene(),
						wwEnvStr( "WW_LODGEN_DATA_ROOT" ) ) )
					st.log << l;
			}

			if ( meshChannel != 0 )
				st.log << QStringLiteral( "orbit mesh half through LOD channel %1 (WW_IMPOSTOR_MESH_CHANNEL)" )
						.arg( meshChannel );
			{
				const QStringList lp = wwEnvStr( "WW_IMPOSTOR_LIGHT" )
						.split( QLatin1Char( ',' ), Qt::SkipEmptyParts );
				bool okD = false, okP = false;
				if ( lp.size() == 2 ) {
					const float d = float( lp[0].trimmed().toDouble( &okD ) );
					const float p = float( lp[1].trimmed().toDouble( &okP ) );
					if ( okD && okP ) {
						ogl->declination = d;
						ogl->planarAngle = p;
						ogl->setFrontalLight( false );
					}
				}
				if ( !lp.isEmpty() )
					st.log << ( ( okD && okP )
							? QStringLiteral( "orbit light: world-fixed, declination %1 planar %2 (WW_IMPOSTOR_LIGHT)" )
									.arg( double( ogl->declination ), 0, 'f', 1 )
									.arg( double( ogl->planarAngle ), 0, 'f', 1 )
							: QStringLiteral( "orbit light: WW_IMPOSTOR_LIGHT '%1' REFUSED, want declination,planar;"
									" the headlight stays" ).arg( wwEnvStr( "WW_IMPOSTOR_LIGHT" ) ) );
			}

			double sumIou = 0.0, sumCol = 0.0;
			int counted = 0;
			{
				for ( const QPair<float, float> & view : views ) {
					const float azim = view.first;
					const float elev = view.second;
					aimCamera( ogl, st.set, st.offset, azim, elev );

					st.armed = false;
					const int keepChannel = wwLodChannelView;
					if ( meshChannel != 0 )
						wwLodChannelView = meshChannel;
					ogl->update();
					qApp->processEvents();
					const QImage meshImg = ogl->grabFramebuffer();
					wwLodChannelView = keepChannel;
					st.armed = true;

					st.suppressScene = true;
					ogl->update();
					qApp->processEvents();
					const QImage cardImg = ogl->grabFramebuffer();
					st.suppressScene = false;

					int meshCov = 0, cardCov = 0, overlap = 0;
					const QVector<uchar> m = silhouette( meshImg, clear, meshCov );
					const QVector<uchar> c = silhouette( cardImg, clear, cardCov );
					const double frac = double( meshCov ) / double( qMax( 1, m.size() ) );
					const double iou = iouOf( m, c );
					const double col = colourError( cardImg, meshImg, c, m, overlap );

					const QString tag = QStringLiteral( "%1_az%2_el%3" ).arg( stem )
							.arg( int( azim ), 3, 10, QLatin1Char( '0' ) )
							.arg( int( elev ), 2, 10, QLatin1Char( '0' ) );
					const bool wroteMesh = meshImg.save( tag + QStringLiteral( "_mesh.png" ) );
					const bool wroteCard = cardImg.save( tag + QStringLiteral( "_card.png" ) );

					st.log << QStringLiteral( "orbit azim %1 elev %2 mesh %3 card %4"
							" iou %5 colour %6 overlap %7 files %8" )
							.arg( double( azim ), 0, 'f', 1 ).arg( double( elev ), 0, 'f', 1 )
							.arg( frac, 0, 'f', 4 )
							.arg( double( cardCov ) / double( qMax( 1, c.size() ) ), 0, 'f', 4 )
							.arg( iou, 0, 'f', 4 ).arg( col, 0, 'f', 4 ).arg( overlap )
							.arg( ( wroteMesh && wroteCard ) ? QStringLiteral( "written" )
									: QStringLiteral( "SAVE FAILED" ) );

					// THE SELECTION, per view, on request: the frames the
					// drawer chose here and their weights, so a gate can check the rule.
					if ( wwEnvInt( "WW_IMPOSTOR_ORBIT_SELECT" ) != 0 )
						for ( const QString & line : ImpostorDraw::describeSelection( scene, st.set, st.offset, st.opt ) )
							st.log << QStringLiteral( "  select %1" ).arg( line );

					if ( meshCov == 0 || frac > 0.95 ) {
						st.log << QStringLiteral( "  EXCLUDED: mesh coverage %1 is degenerate" )
								.arg( frac, 0, 'f', 4 );
						continue;
					}
					// An overlap of a few hundred pixels is not a colour measurement.
					if ( iou >= 0.0 && col >= 0.0
							&& overlap >= qMax( 64, m.size() / 1000 ) ) {
						sumIou += iou;
						sumCol += col;
						counted++;
					} else if ( iou >= 0.0 ) {
						st.log << QStringLiteral( "  colour EXCLUDED: overlap %1 of %2 px" )
								.arg( overlap ).arg( m.size() );
					}
				}
			}
			if ( counted > 0 ) {
				st.log << QStringLiteral( "orbit iou mean %1" ).arg( sumIou / double( counted ), 0, 'f', 4 );
				st.log << QStringLiteral( "orbit colour mean %1" ).arg( sumCol / double( counted ), 0, 'f', 4 );
			} else {
				st.log << QStringLiteral( "orbit iou mean REFUSED: every view was degenerate"
						" or had too little overlap -- is the baked object's own NIF open?" );
			}
			st.log << QStringLiteral( "orbit counted %1 of %2" )
					.arg( counted ).arg( views.size() );
		} else if ( st.mode == Mode::Distance ) {
			// THE DISTANCE STRIP, sized in PIXELS rather than in metres:
			// The brief asks for the LOD4 / LOD8 / LOD16 / LOD32 switch heights.
			const QString stem = wwEnvStr( "WW_IMPOSTOR_SHOT",
					QApplication::applicationDirPath() + QStringLiteral( "/ww_impostor_dist" ) );
			const float azim = wwEnvIsSet( "WW_IMPOSTOR_AZIM" )
					? float( wwEnvStr( "WW_IMPOSTOR_AZIM" ).toDouble() ) : 45.0f;
			const float elev = wwEnvIsSet( "WW_IMPOSTOR_ELEV" )
					? float( wwEnvStr( "WW_IMPOSTOR_ELEV" ).toDouble() ) : 15.0f;
			QList<int> targets;
			for ( const QString & piece : wwEnvStr( "WW_IMPOSTOR_DIST_PX",
					QStringLiteral( "256,128,64,32,16" ) ).split( QLatin1Char( ',' ), Qt::SkipEmptyParts ) ) {
				bool okNum = false;
				const int p = piece.trimmed().toInt( &okNum );
				if ( okNum && p > 0 )
					targets << p;
			}
			if ( targets.isEmpty() )
				targets << 64;

			const int vpH = ogl->grabFramebuffer().height();
			const float big = qMax( st.set.halfW, st.set.halfH );
			st.log << QStringLiteral( "distance strip: azim %1 elev %2, viewport height %3 px,"
					" set half %4 x %5" ).arg( double( azim ) ).arg( double( elev ) ).arg( vpH )
					.arg( double( st.set.halfW ) ).arg( double( st.set.halfH ) );
			for ( int px : targets ) {
				/* orthoHalfWidth = halfH * viewportH / target, and aimCamera
				 * multiplies 1.02 * widen * max(halfW,halfH) -- so this is the
				 * widen that lands the SET's own height on `px` pixels. */
				const float widen = st.set.halfH * float( vpH )
						/ ( float( px ) * 1.02f * qMax( 1.0f, big ) );
				aimCamera( ogl, st.set, st.offset, azim, elev, widen );

				st.armed = false;
				ogl->update();
				qApp->processEvents();
				const QImage sceneImg = ogl->grabFramebuffer();
				st.armed = true;

				st.suppressScene = true;
				ogl->update();
				qApp->processEvents();
				const QImage cardImg = ogl->grabFramebuffer();
				st.suppressScene = false;

				int sceneCov = 0, cardCov = 0;
				const QVector<uchar> s = silhouette( sceneImg, clear, sceneCov );
				const QVector<uchar> c = silhouette( cardImg, clear, cardCov );
				const QString tag = QStringLiteral( "%1_px%2" ).arg( stem )
						.arg( px, 4, 10, QLatin1Char( '0' ) );
				const bool w1 = sceneImg.save( tag + QStringLiteral( "_scene.png" ) );
				const bool w2 = cardImg.save( tag + QStringLiteral( "_card.png" ) );
				st.log << QStringLiteral( "distance px %1 widen %2 scene %3 card %4 iou %5 files %6" )
						.arg( px ).arg( double( widen ), 0, 'f', 3 )
						.arg( double( sceneCov ) / double( qMax( 1, s.size() ) ), 0, 'f', 5 )
						.arg( double( cardCov ) / double( qMax( 1, c.size() ) ), 0, 'f', 5 )
						.arg( iouOf( s, c ), 0, 'f', 4 )
						.arg( ( w1 && w2 ) ? QStringLiteral( "written" ) : QStringLiteral( "SAVE FAILED" ) );
			}
		} else if ( st.mode == Mode::Channels ) {
			const QString stem = wwEnvStr( "WW_IMPOSTOR_SHOT",
					QApplication::applicationDirPath() + QStringLiteral( "/ww_impostor" ) );
			const int saved = st.opt.debugChannel;
			for ( int ch = 0; ch <= 12; ch++ ) {
				st.opt.debugChannel = ch;
				aimCamera( ogl, st.set, st.offset, 45.0f, 20.0f );
				st.suppressScene = true;
				ogl->update();
				qApp->processEvents();
				const QImage img = ogl->grabFramebuffer();
				st.suppressScene = false;
				const QString out = QStringLiteral( "%1_ch%2.png" ).arg( stem ).arg( ch, 2, 10, QLatin1Char( '0' ) );
				st.log << QStringLiteral( "channel %1 -> %2 (%3)" ).arg( ch ).arg( out )
						.arg( img.save( out ) ? QStringLiteral( "written" ) : QStringLiteral( "SAVE FAILED" ) );
			}
			st.opt.debugChannel = saved;
		} else {
			// iou / iou-shuffled
			double sum = 0.0;
			int counted = 0;
			for ( const NamedView & v : kViews ) {
				aimCamera( ogl, st.set, st.offset, v.azimDeg, v.elevDeg );

				// (1) the real mesh alone
				st.armed = false;
				ogl->update();
				qApp->processEvents();
				const QImage meshImg = ogl->grabFramebuffer();
				st.armed = true;

				// (2) the card alone, same camera, same frame size
				st.suppressScene = true;
				ogl->update();
				qApp->processEvents();
				const QImage cardImg = ogl->grabFramebuffer();
				st.suppressScene = false;

				int meshCov = 0, cardCov = 0;
				const QVector<uchar> m = silhouette( meshImg, clear, meshCov );
				const QVector<uchar> c = silhouette( cardImg, clear, cardCov );
				const double frac = double( meshCov ) / double( qMax( 1, m.size() ) );
				const double cfrac = double( cardCov ) / double( qMax( 1, c.size() ) );
				const double iou = iouOf( m, c );

				st.log << QStringLiteral( "view %1 mesh %2 card %3 iou %4" )
						.arg( QLatin1String( v.name ) ).arg( frac, 0, 'f', 4 )
						.arg( cfrac, 0, 'f', 4 ).arg( iou, 0, 'f', 4 );

				if ( meshCov == 0 || frac > 0.95 ) {
					// Excluded and SAID, never averaged in: an empty frame or a
					// frame the mesh fills leaves the number meaningless in the
					// direction that flatters the card.
					st.log << QStringLiteral( "  EXCLUDED: mesh coverage %1 is degenerate" )
							.arg( frac, 0, 'f', 4 );
					continue;
				}
				if ( iou >= 0.0 ) {
					sum += iou;
					counted++;
				}
			}
			if ( counted > 0 )
				st.log << QStringLiteral( "iou mean %1" ).arg( sum / double( counted ), 0, 'f', 4 );
			else
				st.log << QStringLiteral( "iou mean REFUSED: every view was degenerate --"
						" is the baked object's own NIF open?" );
			st.log << QStringLiteral( "views counted %1 of %2" ).arg( counted ).arg( int( std::size( kViews ) ) );
		}

		writeLog();
		endRun( 0 );
	} );

	return true;
}
