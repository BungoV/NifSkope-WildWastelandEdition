#include "cellpost.h"
#include "wwswitch.h"	// lane TESTDRIVER1: the WW_* switch store
#include "gl/celllooks.h"	// lane OPENCELLFIX2: the Camera master
#include "features/shared/cellpasstime.h"	// lane POSTFX1: the post zone's sub-timers
#include "gl/taainputs.h"	// lane POSTFX1: the object vectors

#include "esmdata.h"
#include "gl/gametaa.h"
#include "gl/glscene.h"
#include "gl/lookdevstage.h"
#include "gl/renderer.h"
#include "harnesswindow.h"

#include <QDebug>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QHash>
#include <QSettings>
#include <QTextStream>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

#include "esmfile.hpp"

// the game's depth of field and motion blur on the cell view (cellpost.h)

namespace {

const char * const kDofKey = "WW/CellDof2";	// lane DEFAULTS1: ships on; new key so an old OFF does not stick
const char * const kDofOldKey = "WW/CellDof";	// WW_DEFAULTS1_RED=1: the old row, default off
const char * const kMblurKey = "WW/CellMotionBlur";
const char * const kObKey = "WW/CellObjectBlur";
const char * const kLinKey = "WW/CellPostLinear";	// lane POSTFX2
const char * const kLensKey = "WW/CellDofLens";	// lane DOFLENS1

// lane DOFLENS1: the thin lens's rulings (notes/doflens1/RESULTS.md): a 24 mm high sensor (f = 12 mm / tan(fov / 2)),
// 1 unit = 14.2875 mm, circles at most 64 px; the menu state's lens is f/2.8 focused on the frame's centre
constexpr float kLensSensorMm = 24.0f;
constexpr float kLensMmPerUnit = 14.2875f;
constexpr float kLensMaxPx = 64.0f;
constexpr float kLensMenuFNumber = 2.8f;
constexpr int kLensTile = 8;	// the near reach's tiles, in quarter texels (32 px: the largest near radius)

// lane DOFLENS1-CENSUS: the gameplay states whose depth of field the plugins hold (notes/doflens1/census): the IMAD
// each state's record names in Fallout4.esm (ZOOM GNAM, DOBJ, DFOB, CAMS MNAM). Dialogue's IMAD (DialogueImod) holds no
// depth of field, so dialogue stays the menu state's lens.
struct DofState
{
	const char * name;
	quint32 imad;
	const char * edid;
};
const DofState kDofStates[] = {
	{ "pipboy", 0x0012BC08U, "PipboyMenuImod" },
	{ "pipboypa", 0x000BACD9U, "PipboyPowerArmorImod" },	// DOBJ IMPP
	{ "scope", 0x002041B6U, "zd_ScopeTargetingRecon" },	// ZOOM ZM_Recon_Scope_x2 / x4 / x10
	{ "vats", 0x00024590U, "VATSScanImod" },	// DFOB VATSMenuImageSpaceModifier
	{ "inventory", 0x000E61E5U, "VATSPlaybackDesatBlurImod" },	// DOBJ IMID
	{ "inspect", 0x001B2CBAU, "InspectImod" },	// DFOB InspectImageSpace_DO
	{ "killcam", 0x00049B5EU, "VATSPlaybackDOFImod" },	// CAMS (the VATS playback cameras)
	{ "vatsdialogue", 0x001A517AU, "VATSDialogueImodClose" },	// CAMS (the VATS dialogue cameras)
};
constexpr int kDofStateCount = int( sizeof( kDofStates ) / sizeof( kDofStates[0] ) );

// lane POSTFX1: the object blur's rulings (notes/postfx1/RESULTS.md): a 180 degree shutter (half the frame's travel),
// at most 32 px, 16 px tiles (the neighbour max reaches 3 tiles), 15 taps
constexpr float kObShutter = 0.5f;
constexpr float kObMaxPx = 32.0f;
constexpr int kObTile = 16;
constexpr int kObSamples = 15;

// the game's motion blur: scale (50, times the literal 0.001 and
// 1 / the frame time), its max blur (0.01) and fThreshold (70)
constexpr float kMbScale = 50.0f;
constexpr float kMbMax = 0.01f;
constexpr float kMbThreshold = 70.0f;

// the game's blur weights (a Gaussian of sigma r / 2 over its
// sum), row r - 1 for radius r, tap k at offset k - 7; the bits as the game holds them (a float recompute misses
// by up to 3e-7, so the table is the data, not the formula)
constexpr int kBlurMaxRadius = 7;
const float kBlurWeights[kBlurMaxRadius][15] = {
	{ 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.106506713f, 0.786985755f, 0.106506713f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f },
	{ 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0544885658f, 0.244201481f, 0.402619928f, 0.244201481f, 0.0544885658f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f },
	{ 0.0f, 0.0f, 0.0f, 0.0f, 0.0366327688f, 0.111280687f, 0.216745228f, 0.270682156f, 0.216745228f, 0.111280687f, 0.0366327688f, 0.0f, 0.0f, 0.0f, 0.0f },
	{ 0.0f, 0.0f, 0.0f, 0.0276304893f, 0.0662821382f, 0.1238316f, 0.180173829f, 0.204163671f, 0.180173829f, 0.1238316f, 0.0662821382f, 0.0276304893f, 0.0f, 0.0f, 0.0f },
	{ 0.0f, 0.0f, 0.0221905038f, 0.0455889553f, 0.0798113719f, 0.119064637f, 0.151360765f, 0.163967222f, 0.151360765f, 0.119064637f, 0.0798113719f, 0.0455889553f, 0.0221905038f, 0.0f, 0.0f },
	{ 0.0f, 0.018543981f, 0.0341669098f, 0.0563317239f, 0.0831085816f, 0.109719239f, 0.129617959f, 0.137022808f, 0.129617959f, 0.109719239f, 0.0831085816f, 0.0563317239f, 0.0341669098f, 0.018543981f, 0.0f },
	{ 0.0159283616f, 0.0270778015f, 0.0424232148f, 0.0612547807f, 0.0815124661f, 0.0999668092f, 0.112988554f, 0.117695801f, 0.112988554f, 0.0999668092f, 0.0815124661f, 0.0612547807f, 0.0424232148f, 0.0270778015f, 0.0159283616f },
};

constexpr int kUnitCur = 12, kUnitBlur = 13, kUnitDepth = 14, kUnitMv = 15;
constexpr int kUnitNear = 11;	// lane DOFLENS1: the lens's near layer

struct Dmat
{
	double m[4][4];	// [row][col]
	static Dmat identity()
	{
		Dmat d;
		for ( int r = 0; r < 4; r++ )
			for ( int c = 0; c < 4; c++ )
				d.m[r][c] = r == c ? 1.0 : 0.0;
		return d;
	}
	Dmat operator*( const Dmat & b ) const
	{
		Dmat o;
		for ( int r = 0; r < 4; r++ )
			for ( int c = 0; c < 4; c++ ) {
				double s = 0.0;
				for ( int k = 0; k < 4; k++ )
					s += m[r][k] * b.m[k][c];
				o.m[r][c] = s;
			}
		return o;
	}
};

Dmat fromColumnMajor( const float * f )	// Matrix4 is column-major: data()[c * 4 + r]
{
	Dmat d;
	for ( int r = 0; r < 4; r++ )
		for ( int c = 0; c < 4; c++ )
			d.m[r][c] = double( f[c * 4 + r] );
	return d;
}

Dmat fromTransform( const Transform & t )
{
	Dmat d = Dmat::identity();
	for ( int r = 0; r < 3; r++ ) {
		for ( int c = 0; c < 3; c++ )
			d.m[r][c] = double( t.rotation( r, c ) ) * double( t.scale );
		d.m[r][3] = double( t.translation[r] );
	}
	return d;
}

bool invert( const Dmat & a, Dmat & out )
{
	double t[4][8];
	for ( int r = 0; r < 4; r++ )
		for ( int c = 0; c < 8; c++ )
			t[r][c] = c < 4 ? a.m[r][c] : ( c - 4 == r ? 1.0 : 0.0 );
	for ( int c = 0; c < 4; c++ ) {
		int p = c;
		for ( int r = c + 1; r < 4; r++ )
			if ( std::fabs( t[r][c] ) > std::fabs( t[p][c] ) )
				p = r;
		if ( std::fabs( t[p][c] ) < 1e-300 )
			return false;
		if ( p != c )
			for ( int k = 0; k < 8; k++ )
				std::swap( t[p][k], t[c][k] );
		const double inv = 1.0 / t[c][c];
		for ( int k = 0; k < 8; k++ )
			t[c][k] *= inv;
		for ( int r = 0; r < 4; r++ ) {
			if ( r == c || t[r][c] == 0.0 )
				continue;
			const double f = t[r][c];
			for ( int k = 0; k < 8; k++ )
				t[r][k] -= f * t[c][k];
		}
	}
	for ( int r = 0; r < 4; r++ )
		for ( int c = 0; c < 4; c++ )
			out.m[r][c] = t[r][c + 4];
	return true;
}

Matrix4 toMatrix4( const Dmat & d )
{
	float f[16];
	for ( int r = 0; r < 4; r++ )
		for ( int c = 0; c < 4; c++ )
			f[c * 4 + r] = float( d.m[r][c] );
	return Matrix4( f );
}

struct PostGpu
{
	GLuint inFbo = 0, fbo = 0, mvFbo = 0;
	GLuint cur = 0, depth = 0, blur[2] = { 0, 0 }, mv = 0, out = 0;	// blur[0] = the game's tex2, blur[1] its tex3
	int w = 0, h = 0;
	int qw[2] = { 0, 0 }, qh[2] = { 0, 0 };	// tex2 = (ceil(W / 4), ceil(H / 4)), tex3 = (W >> 2, H >> 2)
	GLenum dsFormat = 0;
	GLbitfield depthBits = 0;
	GLenum mvFormat = 0;	// lane POSTFX1: RG16F (red WW_CELL_MBLUR_RED=rg32: RG32F)
	GLenum colFormat = 0;	// lane POSTFX2: cur / out, RGBA32F over a 32-bit linear frame, else RGBA16F
	GLuint obVec = 0, obTile[2] = { 0, 0 };	// lane POSTFX1: the object blur's pixel vectors, tile max, neighbour max
	int obTw = 0, obTh = 0;
	GLuint lensFar = 0, lensNear = 0;	// lane DOFLENS1: the lens's far and near layers (tex2's size)
	GLuint lensTile[2] = { 0, 0 };	// lane DOFLENS1-NEARTILE: the near reach's tile max and its 3 x 3 dilation
	int lensTw = 0, lensTh = 0;
};

struct PostState
{
	bool init = false;
	quint64 liveGen = 0;	// lane TESTDRIVER1: the switch generation last read
	bool dof = false, dofPinned = false, mb = false, mbPinned = false;
	bool redNoCoc = false, redNoVelocity = false, redNoJitter = false, redBlurRow = false;
	bool ob = false, obPinned = false, redObCamera = false, redRg32 = false;	// lane POSTFX1
	bool redFuse = false;	// lane POSTFX2: WW_CELL_POSTFUSE_RED=1 keeps the depth of field and motion blur two passes
	bool lin = false, linPinned = false;	// lane POSTFX2: the depth of field and motion blur in the linear frame
	bool linRan = false;	// the linear stage ran them this frame (the display stage then skips them)
	/* lane POSTFX3: WW_CELL_POST_MSAALIN_RED=1 writes the linear stage's picture into every multisample alike (POSTFX2's
	 * copy: the tone map then maps one value per pixel and a still edge loses its anti-aliasing) */
	bool redMsLin = false;
	// lane DOFLENS1: the thin-lens depth of field (row WW_CELL_DOF_LENS), its red (WW_CELL_DOF_LENS_RED=1: the game's
	// passes) and the gameplay state (WW_CELL_DOF_STATE=menu: the Pip-Boy / dialogue lens, else play)
	bool lens = false, lensPinned = false, redLens = false, lensMenu = false;
	// lane DOFLENS1-CENSUS: WW_CELL_DOF_STATE=<kDofStates name>: that state's IMAD instead of the cell's record (-1: play;
	// -2: a name not in the table); its values read at the cell's load (strength, distance, range = the curves' largest
	// keys, flags; target = the IMAD's use-target, the lens autofocuses)
	int lensState = -1;
	QString lensStateName;
	struct StateRec
	{
		bool have = false;
		float rec[4] = { 0, 0, 0, 0 };
		bool target = false;
	} stateRec[kDofStateCount];
	bool redNearTile = false;	// lane DOFLENS1-NEARTILE: WW_CELL_DOF_LENS_NEARTILE_RED=1: the near reach is the fixed 32 px
	int curFrame = 0;	// this frame's index and time, taken by the frame's first stage
	double curDt = 0.0;
	QString dumpDir;
	int dumpFrame = 2;

	// the cell
	bool interior = false, haveCellDnam = false;
	float cellDnam[4] = { 0, 0, 0, 0 };

	// the path
	int pathFrame = -1;
	float pathFps = 0.0f;
	int autoFrame = 0;
	QElapsedTimer clock;

	// the motion vectors' previous frame
	bool haveLast = false;
	int lastFrame = 0, lastW = 0, lastH = 0;
	Dmat lastVP = Dmat::identity(), lastPrevVP = Dmat::identity();

	QString lastDof = QStringLiteral( "off" ), lastMb = QStringLiteral( "off" ), lastOb = QStringLiteral( "off" );
	QString saidDof, saidMb, saidOb;
};

bool readPin( const char * env, const char * key, bool & pinned, bool def = false )
{
	const QString pin = wwEnvStr( env ).trimmed();
	if ( !pin.isEmpty() ) {
		pinned = true;
		return pin != QLatin1StringView( "0" ) && pin.compare( QLatin1StringView( "off" ), Qt::CaseInsensitive ) != 0;
	}
	if ( wwHarnessRun() ) {
		pinned = true;	// a harness measures what it pins: no pin, no pass, whatever was saved
		return false;
	}
	pinned = false;
	return QSettings().value( QLatin1StringView( key ), def ).toBool();	// new rows ship OFF unless def
}

PostState & ps()
{
	static PostState s;
	// lane TESTDRIVER1: a live set of these switches re-reads them (pins and or-ed reds back to their defaults first)
	if ( !s.init || wwSwitchMovedFor( s.liveGen, { "WW_CELL_DOF", "WW_CELL_MBLUR", "WW_CELL_POST", "WW_CELL_OBJBLUR" } ) ) {
		s.init = true;
		s.liveGen = wwSwitchGen();
		{
			const PostState d {};
			s.dumpFrame = d.dumpFrame;
		}
		const bool dRed = wwEnvStr( "WW_DEFAULTS1_RED" ).trimmed() == QLatin1StringView( "1" );	// lane DEFAULTS1
		s.dof = readPin( "WW_CELL_DOF", dRed ? kDofOldKey : kDofKey, s.dofPinned, !dRed );
		s.mb = readPin( "WW_CELL_MBLUR", kMblurKey, s.mbPinned );
		s.redNoCoc = wwEnvStr( "WW_CELL_DOF_RED" ).contains( QLatin1StringView( "nococ" ) );
		s.redBlurRow = wwEnvStr( "WW_CELL_DOF_RED" ).contains( QLatin1StringView( "blurrow" ) );
		s.redNoVelocity = wwEnvStr( "WW_CELL_MBLUR_RED" ).contains( QLatin1StringView( "novelocity" ) );
		s.redNoJitter = wwEnvStr( "WW_CELL_MBLUR_RED" ).contains( QLatin1StringView( "nojitter" ) );
		s.redRg32 = wwEnvStr( "WW_CELL_MBLUR_RED" ).contains( QLatin1StringView( "rg32" ) );
		s.redFuse = wwEnvStr( "WW_CELL_POSTFUSE_RED" ).trimmed() == QLatin1StringView( "1" );
		s.ob = readPin( "WW_CELL_OBJBLUR", kObKey, s.obPinned );
		s.lin = readPin( "WW_CELL_POST_LINEAR", kLinKey, s.linPinned );
		s.redMsLin = wwEnvStr( "WW_CELL_POST_MSAALIN_RED" ).trimmed() == QLatin1StringView( "1" );
		s.lens = readPin( "WW_CELL_DOF_LENS", kLensKey, s.lensPinned );
		s.redLens = wwEnvStr( "WW_CELL_DOF_LENS_RED" ).trimmed() == QLatin1StringView( "1" );
		s.lensStateName = wwEnvStr( "WW_CELL_DOF_STATE" ).trimmed().toLower();
		s.lensMenu = s.lensStateName == QLatin1StringView( "menu" );
		s.lensState = -1;
		if ( !s.lensStateName.isEmpty() && !s.lensMenu && s.lensStateName != QLatin1StringView( "play" ) ) {
			s.lensState = -2;
			for ( int i = 0; i < kDofStateCount; i++ )
				if ( s.lensStateName == QLatin1StringView( kDofStates[i].name ) )
					s.lensState = i;
		}
		s.redNearTile = wwEnvStr( "WW_CELL_DOF_LENS_NEARTILE_RED" ).trimmed() == QLatin1StringView( "1" );
		s.redObCamera = wwEnvStr( "WW_CELL_OBJBLUR_RED" ).contains( QLatin1StringView( "camera" ) );
		s.dumpDir = wwEnvStr( "WW_CELL_POST_DUMP" ).trimmed();
		bool ok = false;
		const int df = wwEnvStr( "WW_CELL_POST_DUMP_FRAME" ).toInt( &ok );
		if ( ok )
			s.dumpFrame = df;
	}
	return s;
}

QHash<Renderer *, PostGpu> & postGpus( bool linear )
{
	static QHash<Renderer *, PostGpu> g, gl;	// lane POSTFX2: the linear stage keeps its own (float) targets
	return linear ? gl : g;
}

// telemetry: one line per change of verdict, never per frame (the frame number is left out of the comparison)
void noteDof( PostState & s, const QString & line )
{
	s.lastDof = line;
	if ( s.saidDof != line ) {
		s.saidDof = line;
		qInfo().noquote() << "cell dof:" << line;
	}
}

void noteMb( PostState & s, const QString & line, const QString & key )
{
	s.lastMb = line;
	if ( s.saidMb != key ) {
		s.saidMb = key;
		qInfo().noquote() << "cell mblur:" << line;
	}
}

void noteOb( PostState & s, const QString & line, const QString & key )
{
	s.lastOb = line;
	if ( s.saidOb != key ) {
		s.saidOb = key;
		qInfo().noquote() << "cell objblur:" << line;
	}
}

GLuint makeTex( Renderer * r, GLenum internal, GLenum format, GLenum type, int w, int h, GLenum filter )
{
	auto fn = r->fn;
	GLuint t = 0;
	fn->glGenTextures( 1, &t );
	fn->glBindTexture( GL_TEXTURE_2D, t );
	fn->glTexImage2D( GL_TEXTURE_2D, 0, GLint( internal ), w, h, 0, format, type, nullptr );
	fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GLint( filter ) );
	fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GLint( filter ) );
	fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
	fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
	fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0 );
	if ( format == GL_DEPTH_COMPONENT || format == GL_DEPTH_STENCIL )
		fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_NONE );
	fn->glBindTexture( GL_TEXTURE_2D, 0 );
	return t;
}

void freeGpu( Renderer * r, PostGpu & g )
{
	auto fn = r->fn;
	GLuint texs[13] = { g.cur, g.depth, g.blur[0], g.blur[1], g.mv, g.out, g.obVec, g.obTile[0], g.obTile[1], g.lensFar, g.lensNear,
		g.lensTile[0], g.lensTile[1] };
	for ( GLuint t : texs )
		if ( t )
			fn->glDeleteTextures( 1, &t );
	g.cur = g.depth = g.blur[0] = g.blur[1] = g.mv = g.out = 0;
	g.obVec = g.obTile[0] = g.obTile[1] = 0;
	g.lensFar = g.lensNear = 0;
	g.lensTile[0] = g.lensTile[1] = 0;
}

// a float32 dump: int32 w h channels, then rows bottom-up (GL order), channels interleaved (the TAA dump's format)
bool dumpFloat( const QString & path, int w, int h, int ch, const std::vector<float> & v )
{
	QFile f( path );
	if ( !f.open( QIODevice::WriteOnly ) )
		return false;
	const std::int32_t hdr[3] = { w, h, ch };
	f.write( reinterpret_cast<const char *>( hdr ), sizeof( hdr ) );
	f.write( reinterpret_cast<const char *>( v.data() ), qint64( v.size() * sizeof( float ) ) );
	return true;
}

std::vector<float> readTex( Renderer * r, GLuint fbo, GLuint tex, bool depth, GLenum format, int ch, int w, int h )
{
	auto fn = r->fn;
	std::vector<float> v( size_t( w ) * size_t( h ) * size_t( ch ) );
	fn->glBindFramebuffer( GL_READ_FRAMEBUFFER, fbo );
	if ( !depth ) {
		fn->glFramebufferTexture2D( GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0 );
		fn->glReadBuffer( GL_COLOR_ATTACHMENT0 );
	}
	fn->glPixelStorei( GL_PACK_ALIGNMENT, 1 );
	fn->glReadPixels( 0, 0, w, h, format, GL_FLOAT, v.data() );
	return v;
}

QString f9( double v )
{
	return QString::number( v, 'g', 9 );
}

QString v4( const FloatVector4 & v )
{
	return f9( v[0] ) + QLatin1Char( ' ' ) + f9( v[1] ) + QLatin1Char( ' ' ) + f9( v[2] ) + QLatin1Char( ' ' ) + f9( v[3] );
}

//! the depth of field's record this frame: the cell's XCIM indoors, the weather's keys outdoors
bool dofRecord( const PostState & s, float out[4], QString & src )
{
	if ( s.interior ) {
		if ( !s.haveCellDnam ) {
			src = QStringLiteral( "interior without an imagespace DNAM" );
			return false;
		}
		std::memcpy( out, s.cellDnam, sizeof( s.cellDnam ) );
		src = QStringLiteral( "interior XCIM" );
		return true;
	}
	float a[4], b[4], t = 0.0f;
	bool haveA = false, haveB = false;
	if ( !wwLookdevImageSpaceDof( a, b, t, haveA, haveB ) ) {
		src = QStringLiteral( "no weather loaded" );
		return false;
	}
	if ( !haveA && !haveB ) {
		src = QStringLiteral( "the weather's imagespaces have no DNAM" );
		return false;
	}
	if ( !haveA )
		std::memcpy( a, b, sizeof( a ) );
	if ( !haveB )
		std::memcpy( b, a, sizeof( b ) );
	for ( int i = 0; i < 3; i++ )
		out[i] = a[i] + ( b[i] - a[i] ) * t;
	out[3] = t < 0.5f ? a[3] : b[3];	// the flags are a bit field: the nearer key's, never a blend
	src = QStringLiteral( "weather keys t=%1" ).arg( f9( t ) );
	return true;
}

}	// namespace

bool wwCellDofOn()
{
	return ps().dof && ( ps().dofPinned || wwGroupLets( WwGroup::CameraVanilla ) );	// lane OPENCELLFIX2: the Camera master
}

void wwCellDofSetOn( bool on )
{
	PostState & s = ps();
	s.dof = on;
	if ( !s.dofPinned )
		QSettings().setValue( QLatin1StringView( kDofKey ), on );
}

bool wwCellMotionBlurOn()
{
	return ps().mb && ( ps().mbPinned || wwGroupLets( WwGroup::CameraVanilla ) );
}

void wwCellMotionBlurSetOn( bool on )
{
	PostState & s = ps();
	s.mb = on;
	s.haveLast = false;
	if ( !s.mbPinned )
		QSettings().setValue( QLatin1StringView( kMblurKey ), on );
}

bool wwCellObjectBlurOn()
{
	return ps().ob;	// not the game's: no Camera (vanilla) master
}

void wwCellObjectBlurSetOn( bool on )
{
	PostState & s = ps();
	s.ob = on;
	if ( !s.obPinned )
		QSettings().setValue( QLatin1StringView( kObKey ), on );
}

bool wwCellPostLinearOn()
{
	return ps().lin;
}

void wwCellPostLinearSetOn( bool on )
{
	PostState & s = ps();
	s.lin = on;
	if ( !s.linPinned )
		QSettings().setValue( QLatin1StringView( kLinKey ), on );
}

bool wwCellDofLensOn()
{
	return ps().lens;
}

void wwCellDofLensSetOn( bool on )
{
	PostState & s = ps();
	s.lens = on;
	if ( !s.lensPinned )
		QSettings().setValue( QLatin1StringView( kLensKey ), on );
}

void wwCellPostSetCell( const EsmWorld & world, bool interior )
{
	PostState & s = ps();
	s.interior = interior;
	s.haveCellDnam = false;
	for ( int i = 0; i < 4; i++ )
		s.cellDnam[i] = 0.0f;
	s.haveLast = false;
	const quint32 xcim = interior ? world.interior().imageSpace : 0U;
	ESMFile * esm = world.plugin();
	// lane DOFLENS1-CENSUS: the gameplay states' IMADs (DNAM: use-target u8 at 224, flags u8 at 225, the game's IMGS
	// flags packing; WNAM / XNAM / YNAM: (time, value) keys of strength, distance, range; the state = the largest key)
	for ( int i = 0; i < kDofStateCount; i++ ) {
		auto & sr = s.stateRec[i];
		sr = PostState::StateRec {};
		if ( !esm )
			continue;
		try {
			const ESMFile::ESMRecord * r = esm->findRecord( kDofStates[i].imad );
			if ( !r || !( *r == "IMAD" ) )
				continue;
			ESMFile::ESMField f( *esm, *r );
			bool dnam = false;
			while ( f.next() ) {
				const int k = f == "WNAM" ? 0 : f == "XNAM" ? 1 : f == "YNAM" ? 2 : -1;
				if ( k >= 0 ) {
					for ( size_t o = 0; o + 8 <= f.size(); o += 8 ) {
						float v = 0.0f;
						std::memcpy( &v, f.data() + o + 4, 4 );
						if ( std::isfinite( v ) )
							sr.rec[k] = std::max( sr.rec[k], v );
					}
				} else if ( f == "DNAM" && f.size() >= 228 ) {
					sr.target = f.data()[224] != 0;
					sr.rec[3] = float( f.data()[225] );
					dnam = true;
				}
			}
			sr.have = dnam;
		} catch ( std::exception & ) {
			sr = PostState::StateRec {};
		}
	}
	if ( !xcim || !esm )
		return;
	try {
		const ESMFile::ESMRecord * r = esm->findRecord( xcim );
		if ( !r || !( *r == "IMGS" ) )
			return;
		ESMFile::ESMField f( *esm, *r );
		while ( f.next() ) {
			if ( f == "DNAM" && f.size() >= 16 ) {
				std::memcpy( s.cellDnam, f.data(), 16 );
				s.haveCellDnam = true;
				break;
			}
		}
	} catch ( std::exception & ) {
		s.haveCellDnam = false;
	}
}

int wwCellPostPathFrame()
{
	return ps().pathFrame;
}

float wwCellPostPathFps()
{
	return ps().pathFrame >= 0 ? ps().pathFps : 0.0f;
}

void wwCellPostSetPath( int frame, float fps )
{
	PostState & s = ps();
	s.pathFrame = frame;
	s.pathFps = fps;
	if ( frame < 0 )
		s.haveLast = false;
}

QString wwCellPostEcho()
{
	const PostState & s = ps();
	return QStringLiteral( "cell dof: %1 | cell mblur: %2 | cell objblur: %3" ).arg( s.lastDof, s.lastMb, s.lastOb );
}

namespace {

/*! lane POSTFX2: one stage of the chain. linear: the linear frame before the tone map, the depth of field and motion
 *  blur only (the row WW_CELL_POST_LINEAR); else the tone-mapped frame: what the linear stage did not run this frame. */
void postApply( Scene * scene, bool linear )
{
	PostState & s = ps();
	const bool linRan = !linear && s.linRan;	// the linear stage already took the depth of field and motion blur
	if ( !linear )
		s.linRan = false;
	if ( linear && ( !s.lin || ( !s.dof && !s.mb ) || !scene || !scene->renderer || scene->selecting ) )
		return;
	const bool wantDof = s.dof && !linRan, wantMb = s.mb && !linRan, wantOb = s.ob && !linear;
	if ( !linear ) {
		if ( !s.dof )
			s.lastDof = QStringLiteral( "off" );
		if ( !s.mb )
			s.lastMb = QStringLiteral( "off" );
		if ( !s.ob )
			s.lastOb = QStringLiteral( "off" );
	}
	if ( ( !wantDof && !wantMb && !wantOb ) || !scene || !scene->renderer || scene->selecting )
		return;
	if ( wantOb )
		wwTaaInputsRequest();	// lane POSTFX1: the object vectors of the NEXT frame (the inputs ran before this pass)
	Renderer * r = scene->renderer;
	auto fn = r->fn;
	while ( fn->glGetError() != GL_NO_ERROR ) {
	}

	// this frame's index and time, taken once a frame (by the linear stage when it ran)
	int frame = 0;
	double dt = 0.0;
	if ( linRan ) {
		frame = s.curFrame;
		dt = s.curDt;
	} else if ( s.pathFrame >= 0 ) {
		frame = s.pathFrame;
		dt = s.pathFps > 0.0f ? 1.0 / double( s.pathFps ) : 0.0;
	} else {
		frame = ++s.autoFrame;
		if ( s.clock.isValid() ) {
			dt = double( s.clock.nsecsElapsed() ) * 1e-9;
			s.clock.restart();
		} else {
			s.clock.start();
		}
	}
	s.curFrame = frame;
	s.curDt = dt;
	if ( linear )
		s.linRan = true;
	const QString where = linear ? QStringLiteral( " in the linear frame" ) : QString();

	GLint prevDraw = 0, prevRead = 0, vp[4] = { 0, 0, 1, 1 };
	fn->glGetIntegerv( GL_DRAW_FRAMEBUFFER_BINDING, &prevDraw );
	fn->glGetIntegerv( GL_READ_FRAMEBUFFER_BINDING, &prevRead );
	fn->glGetIntegerv( GL_VIEWPORT, vp );
	const int W = vp[2], H = vp[3];
	if ( W < 2 || H < 2 )
		return;

	// the camera: the projection as uploaded (column-major), near and far from its depth row
	const float * P = &( r->globalUniforms->projectionMatrix[0][0] );
	const bool perspective = P[11] < -0.5f && std::fabs( P[15] ) < 1e-6f;
	float zNear = 0.0f, zFar = 0.0f;
	if ( perspective ) {
		zNear = P[14] / ( P[10] - 1.0f );
		zFar = P[14] / ( P[10] + 1.0f );
	}

	// the depth of field's verdict and constants, as the game sets them
	bool runDof = false;
	bool farOnly = false;
	int radius = 3;
	FloatVector4 dc0( 0.0f ), dc1( 0.0f ), dc2( 0.0f ), dc3( 1.0f );
	// lane DOFLENS1: the thin lens (row WW_CELL_DOF_LENS; red WW_CELL_DOF_LENS_RED=1 runs the game's passes). The focal
	// length from the view's field of view; play: the record's verdict, focus and range, the aperture whose circle at
	// distance + range is the game's blur (strength x 8 x radius px); menu (WW_CELL_DOF_STATE=menu): f/2.8, autofocus
	// on the frame's centre, far only, whatever the record says
	bool runLens = false;
	FloatVector4 lz( 0.0f ), lensP( 0.0f ), lensQ( 0.0f );
	const bool lensOn = s.lens && !s.redLens;
	const float lensFmm = perspective ? 0.5f * kLensSensorMm * P[5] : 0.0f;
	const float lensPxMm = float( H ) / kLensSensorMm;
	const float lensFu = lensFmm / kLensMmPerUnit;
	auto lensMenu = [&]() {
		lz = FloatVector4( zNear, zFar - zNear, zFar * zNear, 0.0f );
		lensP = FloatVector4( 0.0f, lensFmm / kLensMenuFNumber * lensPxMm, lensFu, 0.0f );
		lensQ = FloatVector4( kLensMaxPx, 0.0f, 1.0f, 0.0f );
		runLens = true;
		return QStringLiteral( "on LENS menu f %1 mm f/%2 autofocus far-only max %3 px%4" )
			.arg( f9( lensFmm ), f9( kLensMenuFNumber ), f9( kLensMaxPx ), where );
	};
	// lane DOFLENS1-CENSUS: a gameplay state's IMAD (WW_CELL_DOF_STATE=<name>) instead of the cell's record, for the
	// game's passes and the lens alike
	const PostState::StateRec * stateRec = s.lensState >= 0 ? &s.stateRec[s.lensState] : nullptr;
	auto record = [&]( float rec[4], QString & src ) {
		if ( s.lensState == -1 )
			return dofRecord( s, rec, src );
		if ( s.lensState == -2 ) {
			QStringList names { QStringLiteral( "play" ), QStringLiteral( "menu" ) };
			for ( const DofState & d : kDofStates )
				names << QLatin1StringView( d.name );
			src = QStringLiteral( "WW_CELL_DOF_STATE %1 is none of %2" ).arg( s.lensStateName, names.join( QLatin1Char( ' ' ) ) );
			return false;
		}
		const DofState & d = kDofStates[s.lensState];
		const QString id = QStringLiteral( "%1 %2 %3" ).arg( QLatin1StringView( d.name ), QLatin1StringView( d.edid ) )
			.arg( d.imad, 8, 16, QLatin1Char( '0' ) );
		if ( !stateRec->have ) {
			src = QStringLiteral( "state %1: the IMAD is not in the loaded plugins" ).arg( id );
			return false;
		}
		std::memcpy( rec, stateRec->rec, sizeof( stateRec->rec ) );
		src = QStringLiteral( "state %1%2" ).arg( id, stateRec->target ? QStringLiteral( " target" ) : QString() );
		return true;
	};
	if ( wantDof ) {
		float rec[4] = { 0, 0, 0, 0 };
		QString src;
		if ( !perspective ) {
			noteDof( s, QStringLiteral( "refused (an orthographic view has no depth of field)" ) );
		} else if ( lensOn && s.lensMenu && !( perspective && lensFu > 0.0f ) ) {
			noteDof( s, QStringLiteral( "refused (the lens has no focal length)" ) );
		} else if ( lensOn && s.lensMenu ) {
			runDof = true;
			noteDof( s, lensMenu() );
		} else if ( !record( rec, src ) ) {
			noteDof( s, QStringLiteral( "refused (%1)" ).arg( src ) );
		} else {
			const float strength = rec[0], d = rec[1], rg = rec[2];
			const float fl = rec[3];
			const quint32 flags = ( std::isfinite( fl ) && fl > 0.0f && fl < 4294967296.0f ) ? quint32( fl ) : 0U;
			const quint32 mode = flags & 3U;
			radius = int( flags >> 3 ) ? int( flags >> 3 ) : 3;
			radius = std::min( radius, kBlurMaxRadius );	// Blur3..Blur15: the longest kernel is 15 taps
			farOnly = mode == 2U;
			dc0 = FloatVector4( d - ( d - rg ), ( rg + d ) - d, d, 10000.0f );	// .w: the game's, unread by the composite
			dc1 = FloatVector4( strength, mode > 1U ? 0.0f : 1.0f, ( flags & 1U ) ? 0.0f : 1.0f, ( flags & 4U ) ? 1.0f : 0.0f );
			dc2 = FloatVector4( -1e8f, zNear, zFar - zNear, zFar * zNear );
			const QString what = QStringLiteral( "strength %1 distance %2 range %3 flags %4 radius %5 %6 from %7%8" )
				.arg( f9( strength ), f9( d ), f9( rg ) ).arg( flags ).arg( radius )
				.arg( farOnly ? QStringLiteral( "far-only" ) : QStringLiteral( "near+far" ), src,
					s.redNoCoc ? QStringLiteral( " RED nococ" ) : s.redBlurRow ? QStringLiteral( " RED blurrow" ) : QString() );
			if ( !( strength > 0.0f ) ) {
				noteDof( s, QStringLiteral( "refused (strength 0: %1)" ).arg( what ) );
			} else if ( lensOn && lensFu > 0.0f ) {
				runDof = true;
				runLens = true;
				const float s1 = std::max( d, lensFu * 1.01f + 0.001f );
				const float rgv = rg > 0.0f ? rg : 1.0f;
				const float want = strength * 8.0f * float( radius );	// the circle (px) at distance + range
				// a state's range 0: the full circle at infinity (the play record keeps its range 1)
				const float aPx = ( stateRec && !( rg > 0.0f ) ) ? want * ( s1 - lensFu ) / lensFu
					: want * ( s1 + rgv ) * ( s1 - lensFu ) / ( lensFu * rgv );
				const bool autofocus = stateRec && stateRec->target;	// the IMAD's use-target: the frame's centre
				lz = FloatVector4( zNear, zFar - zNear, zFar * zNear, dc1[3] );
				lensP = FloatVector4( autofocus ? 0.0f : s1, aPx, lensFu, 0.0f );
				lensQ = FloatVector4( kLensMaxPx, ( farOnly || dc1[1] == 0.0f ) ? 0.0f : 1.0f, dc1[2], s.redNearTile ? 0.0f : 1.0f );
				noteDof( s, QStringLiteral( "on LENS %1 f %2 mm f/%3 focus %4 max %5 px near %6 far %7 reach %8 (%9)%10" )
					.arg( stateRec ? QLatin1StringView( kDofStates[s.lensState].name ) : QLatin1StringView( "play" ), f9( lensFmm ),
						f9( aPx > 0.0f ? lensFmm * lensPxMm / aPx : 0.0f ),
						autofocus ? QStringLiteral( "autofocus" ) : f9( s1 ), f9( kLensMaxPx ) )
					.arg( int( lensQ[1] ) ).arg( int( lensQ[2] ) )
					.arg( lensQ[1] == 0.0f ? QStringLiteral( "-" ) : s.redNearTile ? QStringLiteral( "fixed RED neartile" ) : QStringLiteral( "tile" ),
						what, where ) );
			} else {
				runDof = true;
				noteDof( s, QStringLiteral( "on %1%2%3" ).arg( what, s.lens && s.redLens ? QStringLiteral( " RED lens" ) : QString(), where ) );
			}
		}
	}

	// the motion vectors' frames: the next index continues the path, the same index repeats it
	// the camera without the temporal AA's jitter, as the game draws its vector target; the vector pass removes the
	// jitter from the pixel's NDC. WW_CELL_MBLUR_RED=nojitter keeps it (debug control)
	Matrix4 unjittered;
	float jitX = 0.0f, jitY = 0.0f;
	const bool jittered = wwGameTaaUnjittered( unjittered ) && wwGameTaaJitter( jitX, jitY );
	if ( !jittered )
		jitX = jitY = 0.0f;
	const Dmat curVP = ( jittered && !s.redNoJitter ? fromColumnMajor( unjittered.data() ) : fromColumnMajor( P ) )
		* fromTransform( scene->view );
	bool runMb = false;
	Dmat reproj = Dmat::identity();
	FloatVector4 mc0( 0.0f ), mc1( 0.0f );
	if ( wantMb ) {
		Dmat prevVP = curVP;
		bool have = false;
		if ( s.haveLast && s.lastW == W && s.lastH == H ) {
			if ( frame == s.lastFrame + 1 || s.pathFrame < 0 ) {
				prevVP = s.lastVP;
				have = true;
			} else if ( frame == s.lastFrame ) {
				prevVP = s.lastPrevVP;
				have = true;
			}
		}
		s.lastPrevVP = prevVP;
		s.lastVP = curVP;
		s.lastFrame = frame;
		s.lastW = W;
		s.lastH = H;
		s.haveLast = true;
		Dmat invCur;
		if ( !have ) {
			noteMb( s, QStringLiteral( "waiting (no previous frame)" ), QStringLiteral( "waiting" ) );
		} else if ( !( dt > 0.0 ) ) {
			noteMb( s, QStringLiteral( "refused (no frame time)" ), QStringLiteral( "nodt" ) );
		} else if ( !invert( curVP, invCur ) ) {
			noteMb( s, QStringLiteral( "refused (the camera matrix does not invert)" ), QStringLiteral( "noinv" ) );
		} else {
			reproj = prevVP * invCur;
			runMb = true;
			// c0 = (scale x 0.001 x (1 / dt), max blur, fThreshold, 0); c1 = (1, 1, (W - 1) / W, (H - 1) / H)
			mc0 = FloatVector4( kMbScale * 0.001f * ( 1.0f / float( dt ) ), kMbMax, kMbThreshold, 0.0f );
			mc1 = FloatVector4( 1.0f, 1.0f, float( W - 1 ) / float( W ), float( H - 1 ) / float( H ) );
			// lane POSTFX2: say whether the pass also composites the depth of field
			const QString fused = ( !runDof ? QString() : s.redFuse ? QStringLiteral( " two-pass RED postfuse" )
				: QStringLiteral( " fused with the depth of field" ) ) + where;
			const QString line = QStringLiteral( "on frame %1 dt %2 c0 %3 jitter %4 %5%6%7" ).arg( frame ).arg( f9( dt ), v4( mc0 ),
				f9( jitX ), f9( jitY ), s.redNoVelocity ? QStringLiteral( " RED novelocity" )
				: s.redNoJitter ? QStringLiteral( " RED nojitter" ) : QString(), fused );
			// the key: a path's fixed frame time, never an interactive window's measured one (no line per paint)
			noteMb( s, line, QStringLiteral( "on %1 %2%3" ).arg( s.pathFrame >= 0 ? f9( dt ) : QStringLiteral( "measured" ) )
				.arg( s.redNoVelocity ? 1 : 0 ).arg( fused ) );
		}
	}
	// lane POSTFX1: the object blur's verdict: this frame's object vectors and the camera they were drawn with
	bool runOb = false;
	GLuint obVel = 0, obMask = 0;
	float obReproj[16] = {};
	float obJx = 0.0f, obJy = 0.0f;
	if ( wantOb ) {
		int vw = 0, vh = 0, mw = 0, mh = 0;
		obVel = wwTaaInputsVelocityTex( scene, vw, vh );
		obMask = wwTaaInputsMaskTex( scene, mw, mh );
		if ( !perspective ) {
			noteOb( s, QStringLiteral( "refused (an orthographic view)" ), QStringLiteral( "ortho" ) );
		} else if ( !obVel || !obMask || !wwTaaInputsCamera( scene, obReproj, obJx, obJy ) ) {
			noteOb( s, QStringLiteral( "waiting (no object vectors this frame)" ), QStringLiteral( "waiting" ) );
		} else if ( vw != W || vh != H || mw != W || mh != H ) {
			noteOb( s, QStringLiteral( "refused (vectors %1x%2, frame %3x%4)" ).arg( vw ).arg( vh ).arg( W ).arg( H ),
				QStringLiteral( "size" ) );
		} else {
			runOb = true;
			noteOb( s, QStringLiteral( "on shutter %1 max %2 px tile %3 taps %4%5" ).arg( f9( kObShutter ), f9( kObMaxPx ) )
				.arg( kObTile ).arg( kObSamples ).arg( s.redObCamera ? QStringLiteral( " RED camera" ) : QString() ),
				QStringLiteral( "on %1" ).arg( s.redObCamera ? 1 : 0 ) );
		}
	}
	if ( !runDof && !runMb && !runOb )
		return;

	// the frame's depth format: the depth goes over by a blit, which needs the same format both sides
	GLint depthBits = 0, stencilBits = 0, compType = 0;
	const GLenum att = prevDraw ? GL_DEPTH_ATTACHMENT : GL_DEPTH;
	fn->glGetFramebufferAttachmentParameteriv( GL_DRAW_FRAMEBUFFER, att, GL_FRAMEBUFFER_ATTACHMENT_DEPTH_SIZE, &depthBits );
	fn->glGetFramebufferAttachmentParameteriv( GL_DRAW_FRAMEBUFFER, prevDraw ? GL_STENCIL_ATTACHMENT : GL_STENCIL,
		GL_FRAMEBUFFER_ATTACHMENT_STENCIL_SIZE, &stencilBits );
	fn->glGetFramebufferAttachmentParameteriv( GL_DRAW_FRAMEBUFFER, att, GL_FRAMEBUFFER_ATTACHMENT_COMPONENT_TYPE, &compType );
	GLenum dsFormat = 0, dsFmt = 0, dsType = 0;
	GLbitfield blitBits = GL_DEPTH_BUFFER_BIT;
	if ( stencilBits == 8 && depthBits == 24 && compType != GL_FLOAT ) {
		dsFormat = GL_DEPTH24_STENCIL8; dsFmt = GL_DEPTH_STENCIL; dsType = GL_UNSIGNED_INT_24_8;
		blitBits |= GL_STENCIL_BUFFER_BIT;
	} else if ( stencilBits == 8 && depthBits == 32 && compType == GL_FLOAT ) {
		dsFormat = GL_DEPTH32F_STENCIL8; dsFmt = GL_DEPTH_STENCIL; dsType = GL_FLOAT_32_UNSIGNED_INT_24_8_REV;
		blitBits |= GL_STENCIL_BUFFER_BIT;
	} else if ( stencilBits == 0 && depthBits == 24 ) {
		dsFormat = GL_DEPTH_COMPONENT24; dsFmt = GL_DEPTH_COMPONENT; dsType = GL_UNSIGNED_INT;
	} else if ( stencilBits == 0 && depthBits == 32 && compType == GL_FLOAT ) {
		dsFormat = GL_DEPTH_COMPONENT32F; dsFmt = GL_DEPTH_COMPONENT; dsType = GL_FLOAT;
	}
	if ( !dsFormat ) {
		const QString why = QStringLiteral( "refused (the frame's depth is %1 bits, %2 stencil)" ).arg( depthBits ).arg( stencilBits );
		if ( runDof )
			noteDof( s, why );
		if ( runMb )
			noteMb( s, why, why );
		if ( runOb )
			noteOb( s, why, why );
		return;
	}

	PostGpu & g = postGpus( linear )[r];
	// lane POSTFX2: the linear frame's own precision (RGBA32F by default): its values pass through unclamped
	GLenum colFormat = GL_RGBA16F;
	GLint frameSamples = 0;	// lane POSTFX3: the linear frame's multisamples (the bound draw framebuffer)
	if ( linear && prevDraw ) {
		GLint redBits = 0;
		fn->glGetFramebufferAttachmentParameteriv( GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_FRAMEBUFFER_ATTACHMENT_RED_SIZE, &redBits );
		if ( redBits >= 32 )
			colFormat = GL_RGBA32F;
		fn->glGetIntegerv( GL_SAMPLES, &frameSamples );
	}
	// lane POSTFX1: the camera vectors in RG16F (the inputs' format; the game's motion blur reads them within 0.5 of an
	// 8-bit step on 99.99% of pixels, tests/postfx/postfx_gates.py); red WW_CELL_MBLUR_RED=rg32 keeps RG32F
	const GLenum mvFormat = s.redRg32 ? GL_RG32F : GL_RG16F;
	if ( !g.inFbo || g.w != W || g.h != H || g.dsFormat != dsFormat || g.mvFormat != mvFormat || g.colFormat != colFormat ) {
		freeGpu( r, g );
		if ( !g.inFbo ) {
			fn->glGenFramebuffers( 1, &g.inFbo );
			fn->glGenFramebuffers( 1, &g.fbo );
			fn->glGenFramebuffers( 1, &g.mvFbo );
		}
		g.qw[0] = ( W + 3 ) / 4;
		g.qh[0] = ( H + 3 ) / 4;
		g.qw[1] = std::max( W >> 2, 1 );
		g.qh[1] = std::max( H >> 2, 1 );
		// half floats between the passes, as the game's targets: the frame's values pass through unclamped
		g.cur = makeTex( r, colFormat, GL_RGBA, GL_FLOAT, W, H, GL_LINEAR );				// t0
		g.depth = makeTex( r, dsFormat, dsFmt, dsType, W, H, GL_NEAREST );					// the DoF's t2
		g.blur[0] = makeTex( r, GL_RGBA16F, GL_RGBA, GL_FLOAT, g.qw[0], g.qh[0], GL_NEAREST );	// tex2, the DoF's t1
		g.blur[1] = makeTex( r, GL_RGBA16F, GL_RGBA, GL_FLOAT, g.qw[1], g.qh[1], GL_NEAREST );	// tex3
		g.mv = makeTex( r, mvFormat, GL_RG, GL_FLOAT, W, H, GL_LINEAR );					// the blur's t1
		g.out = makeTex( r, colFormat, GL_RGBA, GL_FLOAT, W, H, GL_LINEAR );
		g.mvFormat = mvFormat;
		g.colFormat = colFormat;
		fn->glBindFramebuffer( GL_FRAMEBUFFER, g.inFbo );
		fn->glFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, g.cur, 0 );
		fn->glFramebufferTexture2D( GL_FRAMEBUFFER, dsFmt == GL_DEPTH_STENCIL ? GL_DEPTH_STENCIL_ATTACHMENT : GL_DEPTH_ATTACHMENT,
			GL_TEXTURE_2D, g.depth, 0 );
		fn->glBindFramebuffer( GL_FRAMEBUFFER, g.mvFbo );
		fn->glFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, g.mv, 0 );
		g.w = W;
		g.h = H;
		g.dsFormat = dsFormat;
		g.depthBits = blitBits;
	}
	if ( runOb && !g.obVec ) {	// lane POSTFX1: made on the first frame that blurs (freed with the rest on a resize)
		g.obTw = ( W + kObTile - 1 ) / kObTile;
		g.obTh = ( H + kObTile - 1 ) / kObTile;
		g.obVec = makeTex( r, GL_RG16F, GL_RG, GL_FLOAT, W, H, GL_NEAREST );
		g.obTile[0] = makeTex( r, GL_RG16F, GL_RG, GL_FLOAT, g.obTw, g.obTh, GL_NEAREST );
		g.obTile[1] = makeTex( r, GL_RG16F, GL_RG, GL_FLOAT, g.obTw, g.obTh, GL_NEAREST );
	}

	// state kept for the restore
	GLint prevActive = 0;
	fn->glGetIntegerv( GL_ACTIVE_TEXTURE, &prevActive );
	GLboolean colorMask[4], depthMask = GL_TRUE;
	fn->glGetBooleanv( GL_COLOR_WRITEMASK, colorMask );
	fn->glGetBooleanv( GL_DEPTH_WRITEMASK, &depthMask );
	const bool wasDepth = fn->glIsEnabled( GL_DEPTH_TEST ), wasBlend = fn->glIsEnabled( GL_BLEND );
	const bool wasCull = fn->glIsEnabled( GL_CULL_FACE ), wasStencil = fn->glIsEnabled( GL_STENCIL_TEST );
	const bool wasScissor = fn->glIsEnabled( GL_SCISSOR_TEST );

	// 1. the frame (resolved when multisampled) into t0 and the depth
	fn->glBindFramebuffer( GL_READ_FRAMEBUFFER, GLuint( prevDraw ) );
	fn->glBindFramebuffer( GL_DRAW_FRAMEBUFFER, g.inFbo );
	fn->glDisable( GL_SCISSOR_TEST );
	fn->glBlitFramebuffer( 0, 0, W, H, 0, 0, W, H, GL_COLOR_BUFFER_BIT, GL_NEAREST );
	fn->glBlitFramebuffer( 0, 0, W, H, 0, 0, W, H, g.depthBits, GL_NEAREST );
	const GLenum blitErr = fn->glGetError();
	bool ok = blitErr == GL_NO_ERROR;

	fn->glDisable( GL_DEPTH_TEST );
	fn->glDepthMask( GL_FALSE );
	fn->glDisable( GL_BLEND );
	fn->glDisable( GL_CULL_FACE );
	fn->glDisable( GL_STENCIL_TEST );
	fn->glColorMask( GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE );

	static const float quad[12] = { -1, -1, 0, 1, -1, 0, -1, 1, 0, 1, 1, 0 };
	static const std::uint16_t idx[6] = { 0, 1, 2, 2, 1, 3 };
	const float * attrs = quad;
	const bool dump = !s.dumpDir.isEmpty() && frame == s.dumpFrame;
	const QString dd = s.dumpDir + QLatin1Char( '/' );
	if ( dump )
		QDir().mkpath( s.dumpDir );
	const GLenum one = GL_COLOR_ATTACHMENT0;
	auto target = [&]( GLuint tex ) {
		fn->glBindFramebuffer( GL_FRAMEBUFFER, g.fbo );
		fn->glFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0 );
		fn->glDrawBuffers( 1, &one );
	};
	auto bind = [&]( int unit, GLuint tex ) {
		fn->glActiveTexture( GLenum( GL_TEXTURE0 + unit ) );
		fn->glBindTexture( GL_TEXTURE_2D, tex );
	};
	if ( ok && dump ) {
		dumpFloat( dd + "cur.bin", W, H, 4, readTex( r, g.fbo, g.cur, false, GL_RGBA, 4, W, H ) );
		dumpFloat( dd + "depth.bin", W, H, 1, readTex( r, g.inFbo, 0, true, GL_DEPTH_COMPONENT, 1, W, H ) );
	}

	// 2. the depth of field, the game's passes: the downsample (03706) of the frame into the quarter-size tex2, the
	// blur (Blur3..Blur15) down the columns tex2 -> tex3 then along the rows tex3 -> tex2, both reading by point,
	// then the composite (03664 / 03676) into out reading tex2 linearly; out back to cur
	auto filter = [&]( GLuint tex, GLenum f ) {
		fn->glBindTexture( GL_TEXTURE_2D, tex );
		fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GLint( f ) );
		fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GLint( f ) );
	};
	// lane POSTFX2: with both on, the composite runs inside the motion blur's pass (game_dofmblur.frag): no composite
	// target, no copy back (within 0.25 of an 8-bit step of the two passes, less GPU time, tests/postfx/postfx_fuse.py)
	const bool fuse = runDof && runMb && !s.redFuse;
	/* lane POSTFX3: in a multisampled linear frame the copy back keeps every sample's own value where the blur left the
	 * pixel alone (cell_post_mscopy.frag), so the tone map still maps each sample. It needs the frame's own resolve (cur)
	 * beside the picture: the depth of field alone composites into out and skips its copy to cur; the two-pass red
	 * (WW_CELL_POSTFUSE_RED) overwrites cur and takes the plain copy. */
	const bool msKeep = linear && frameSamples > 1 && !s.redMsLin && !( runDof && runMb && !fuse );
	// lane DOFLENS1: the thin lens instead of the game's passes: the prep (colour and circle, game_doflens_prep) into
	// tex2, the near / far gather (game_doflens_gather) into the lens's two layers, then the composite into out
	// (game_doflens; inside the motion blur's pass when fused); out back to cur
	auto lensUnis = [&]( auto * p ) {
		p->uni4f( "lz", lz );
		p->uni4f( "lensP", lensP );
		p->uni4f( "lensQ", lensQ );
	};
	if ( ok && runLens ) {
		PassTime::subBegin( PassTime::PostDof );
		if ( !g.lensFar ) {	// made on the first frame the lens runs (freed with the rest on a resize)
			g.lensFar = makeTex( r, GL_RGBA16F, GL_RGBA, GL_FLOAT, g.qw[0], g.qh[0], GL_LINEAR );
			g.lensNear = makeTex( r, GL_RGBA16F, GL_RGBA, GL_FLOAT, g.qw[0], g.qh[0], GL_LINEAR );
		}
		target( g.blur[0] );
		fn->glViewport( 0, 0, g.qw[0], g.qh[0] );
		if ( auto prog = r->useProgram( "game_doflens_prep.prog" ) ) {
			bind( kUnitCur, g.cur );
			bind( kUnitDepth, g.depth );
			prog->uni1i( "t0", kUnitCur );
			prog->uni1i( "t1", kUnitDepth );
			lensUnis( prog );
			r->drawShape( 4, 3, 6, GL_TRIANGLES, GL_UNSIGNED_SHORT, &attrs, idx );
			r->stopProgram();
		} else {
			ok = false;
		}
		// lane DOFLENS1-NEARTILE: the near layer's reach (game_doflens_tile): the largest near radius per tile of 8 x 8
		// quarter texels, then over the 3 x 3 tiles around; the gather's near kernel takes it (lensQ.w = 1)
		const bool nearTile = lensQ[1] != 0.0f && lensQ[3] > 0.5f;
		if ( ok && nearTile ) {
			if ( !g.lensTile[0] ) {
				g.lensTw = ( g.qw[0] + kLensTile - 1 ) / kLensTile;
				g.lensTh = ( g.qh[0] + kLensTile - 1 ) / kLensTile;
				for ( GLuint & t : g.lensTile )
					t = makeTex( r, GL_R16F, GL_RED, GL_FLOAT, g.lensTw, g.lensTh, GL_NEAREST );
			}
			fn->glViewport( 0, 0, g.lensTw, g.lensTh );
			for ( int pass = 0; pass < 2 && ok; pass++ ) {
				target( g.lensTile[pass] );
				if ( auto prog = r->useProgram( "game_doflens_tile.prog" ) ) {
					bind( kUnitNear, pass ? g.lensTile[0] : g.blur[0] );
					prog->uni1i( "t0", kUnitNear );
					prog->uni1i( "dilate", pass );
					r->drawShape( 4, 3, 6, GL_TRIANGLES, GL_UNSIGNED_SHORT, &attrs, idx );
					r->stopProgram();
				} else {
					ok = false;
				}
			}
			fn->glViewport( 0, 0, g.qw[0], g.qh[0] );
		}
		if ( ok ) {
			fn->glBindFramebuffer( GL_FRAMEBUFFER, g.fbo );
			fn->glFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, g.lensFar, 0 );
			fn->glFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, g.lensNear, 0 );
			const GLenum two[2] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
			fn->glDrawBuffers( 2, two );
			bind( kUnitBlur, g.blur[0] );
			filter( g.blur[0], GL_LINEAR );
			if ( nearTile )
				bind( kUnitNear, g.lensTile[1] );
			if ( auto prog = r->useProgram( "game_doflens_gather.prog" ) ) {
				prog->uni1i( "t0", kUnitBlur );
				prog->uni1i( "t1", kUnitNear );
				prog->uni4f( "lensQ", lensQ );
				r->drawShape( 4, 3, 6, GL_TRIANGLES, GL_UNSIGNED_SHORT, &attrs, idx );
				r->stopProgram();
			} else {
				ok = false;
			}
			fn->glFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, 0, 0 );
		}
		fn->glViewport( vp[0], vp[1], vp[2], vp[3] );
		if ( ok && !fuse ) {
			target( g.out );
			if ( auto prog = r->useProgram( "game_doflens.prog" ) ) {
				bind( kUnitCur, g.cur );
				bind( kUnitBlur, g.lensFar );
				bind( kUnitDepth, g.depth );
				bind( kUnitNear, g.lensNear );
				prog->uni1i( "t0", kUnitCur );
				prog->uni1i( "t1", kUnitBlur );
				prog->uni1i( "t2", kUnitDepth );
				prog->uni1i( "lensNear", kUnitNear );
				lensUnis( prog );
				r->drawShape( 4, 3, 6, GL_TRIANGLES, GL_UNSIGNED_SHORT, &attrs, idx );
				r->stopProgram();
			} else {
				ok = false;
			}
		}
		if ( ok && dump ) {
			dumpFloat( dd + "lens_prep.bin", g.qw[0], g.qh[0], 4, readTex( r, g.fbo, g.blur[0], false, GL_RGBA, 4, g.qw[0], g.qh[0] ) );
			dumpFloat( dd + "lens_far.bin", g.qw[0], g.qh[0], 4, readTex( r, g.fbo, g.lensFar, false, GL_RGBA, 4, g.qw[0], g.qh[0] ) );
			dumpFloat( dd + "lens_near.bin", g.qw[0], g.qh[0], 4, readTex( r, g.fbo, g.lensNear, false, GL_RGBA, 4, g.qw[0], g.qh[0] ) );
			if ( nearTile )
				dumpFloat( dd + "lens_tile.bin", g.lensTw, g.lensTh, 1, readTex( r, g.fbo, g.lensTile[1], false, GL_RED, 1, g.lensTw, g.lensTh ) );
			if ( !fuse )
				dumpFloat( dd + "dof_out.bin", W, H, 4, readTex( r, g.fbo, g.out, false, GL_RGBA, 4, W, H ) );
		}
		if ( ok && !fuse && !( msKeep && !runMb ) ) {	// the motion blur reads the depth of field's picture
			fn->glBindFramebuffer( GL_READ_FRAMEBUFFER, g.fbo );
			fn->glFramebufferTexture2D( GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, g.out, 0 );
			fn->glReadBuffer( GL_COLOR_ATTACHMENT0 );
			fn->glBindFramebuffer( GL_DRAW_FRAMEBUFFER, g.inFbo );
			fn->glDrawBuffers( 1, &one );
			fn->glBlitFramebuffer( 0, 0, W, H, 0, 0, W, H, GL_COLOR_BUFFER_BIT, GL_NEAREST );
		}
		PassTime::subEnd( PassTime::PostDof );
	}
	if ( ok && runDof && !runLens ) {
		PassTime::subBegin( PassTime::PostDof );
		target( g.blur[0] );
		fn->glViewport( 0, 0, g.qw[0], g.qh[0] );
		auto prog = r->useProgram( "game_dof_down.prog" );
		if ( !prog ) {
			ok = false;
		} else {
			bind( kUnitCur, g.cur );
			prog->uni1i( "t0", kUnitCur );
			prog->uni4f( "c4", FloatVector4( 1.0f, 1.0f, 2.0f / float( W ), 2.0f / float( H ) ) );
			prog->uni4f( "c7", FloatVector4( 1.0f / float( W ), 1.0f / float( H ), 0.0f, 0.0f ) );
			const FloatVector4 taps[4] = { FloatVector4( -1.0f, -1.0f, 0.25f, 0.0f ), FloatVector4( 1.0f, -1.0f, 0.25f, 0.0f ),
				FloatVector4( -1.0f, 1.0f, 0.25f, 0.0f ), FloatVector4( 1.0f, 1.0f, 0.25f, 0.0f ) };
			prog->uni4fv( "taps", taps, 4 );
			prog->uni2f( "dstSize", float( g.qw[0] ), float( g.qh[0] ) );
			r->drawShape( 4, 3, 6, GL_TRIANGLES, GL_UNSIGNED_SHORT, &attrs, idx );
			r->stopProgram();
		}
		if ( ok && dump )
			dumpFloat( dd + "dof_down.bin", g.qw[0], g.qh[0], 4, readTex( r, g.fbo, g.blur[0], false, GL_RGBA, 4, g.qw[0], g.qh[0] ) );
		const int taps = 2 * radius + 1;
		// the weight row: radius - 1; the red (WW_CELL_DOF_RED=blurrow) reads the next radius's row (Blur15 reads Blur13's)
		const int row = s.redBlurRow ? ( radius < kBlurMaxRadius ? radius : radius - 2 ) : radius - 1;
		for ( int pass = 0; pass < 2 && ok; pass++ ) {	// pass 0: the columns (y), pass 1: the rows (x)
			const GLuint src = g.blur[pass], dst = g.blur[1 - pass];
			const int sw = g.qw[pass], sh = g.qh[pass], dw = g.qw[1 - pass], dh = g.qh[1 - pass];
			target( dst );
			fn->glViewport( 0, 0, dw, dh );
			auto bprog = r->useProgram( "game_dof_blur.prog" );
			if ( !bprog ) {
				ok = false;
				break;
			}
			bind( kUnitBlur, src );
			filter( src, GL_NEAREST );
			FloatVector4 o[15];
			for ( int i = 0; i < taps; i++ ) {
				const int k = 7 - radius + i;
				o[i] = FloatVector4( pass == 1 ? float( k - 7 ) / float( sw ) : 0.0f, pass == 0 ? float( k - 7 ) / float( sh ) : 0.0f,
					kBlurWeights[row][k], 0.0f );
			}
			bprog->uni1i( "t0", kUnitBlur );
			bprog->uni4fv( "o", o, size_t( taps ) );
			bprog->uni1i( "taps", taps );
			bprog->uni2f( "dstSize", float( dw ), float( dh ) );
			r->drawShape( 4, 3, 6, GL_TRIANGLES, GL_UNSIGNED_SHORT, &attrs, idx );
			r->stopProgram();
		}
		fn->glViewport( vp[0], vp[1], vp[2], vp[3] );
		if ( ok )
			filter( g.blur[0], GL_LINEAR );
		if ( ok && !fuse ) {
			target( g.out );
			auto prog = r->useProgram( "game_dof.prog" );
			if ( !prog ) {
				ok = false;
			} else {
				bind( kUnitCur, g.cur );
				bind( kUnitBlur, g.blur[0] );
				bind( kUnitDepth, g.depth );
				prog->uni1i( "t0", kUnitCur );
				prog->uni1i( "t1", kUnitBlur );
				prog->uni1i( "t2", kUnitDepth );
				prog->uni4f( "c0", dc0 );
				prog->uni4f( "c1", dc1 );
				prog->uni4f( "c2", dc2 );
				prog->uni4f( "c3", dc3 );
				prog->uni1i( "farOnly", farOnly ? 1 : 0 );
				prog->uni1i( "dofRed", s.redNoCoc ? 1 : 0 );
				r->drawShape( 4, 3, 6, GL_TRIANGLES, GL_UNSIGNED_SHORT, &attrs, idx );
				r->stopProgram();
			}
		}
		if ( ok && dump ) {
			dumpFloat( dd + "dof_blur.bin", g.qw[0], g.qh[0], 4, readTex( r, g.fbo, g.blur[0], false, GL_RGBA, 4, g.qw[0], g.qh[0] ) );
			if ( !fuse )
				dumpFloat( dd + "dof_out.bin", W, H, 4, readTex( r, g.fbo, g.out, false, GL_RGBA, 4, W, H ) );
		}
		if ( ok && !fuse && !( msKeep && !runMb ) ) {	// the motion blur reads the depth of field's picture
			fn->glBindFramebuffer( GL_READ_FRAMEBUFFER, g.fbo );
			fn->glFramebufferTexture2D( GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, g.out, 0 );
			fn->glReadBuffer( GL_COLOR_ATTACHMENT0 );
			fn->glBindFramebuffer( GL_DRAW_FRAMEBUFFER, g.inFbo );
			fn->glDrawBuffers( 1, &one );
			fn->glBlitFramebuffer( 0, 0, W, H, 0, 0, W, H, GL_COLOR_BUFFER_BIT, GL_NEAREST );
		}
		PassTime::subEnd( PassTime::PostDof );
	}

	// 3. the motion blur: the camera's vectors (the temporal AA's vector pass), then the game's blur into out
	if ( ok && runMb ) {
		PassTime::subBegin( PassTime::PostMblur );
		fn->glBindFramebuffer( GL_FRAMEBUFFER, g.mvFbo );
		fn->glDrawBuffers( 1, &one );
		if ( s.redNoVelocity ) {
			const GLfloat zero[4] = { 0, 0, 0, 0 };
			fn->glClearBufferfv( GL_COLOR, 0, zero );
		} else {
			auto prog = r->useProgram( "game_taa_mv.prog" );
			if ( !prog ) {
				ok = false;
			} else {
				bind( kUnitDepth, g.depth );
				prog->uni1i( "depthTex", kUnitDepth );
				prog->uni4m( "reproj", toMatrix4( reproj ) );
				// 0 when the temporal AA is off; 0 in the red (its frames keep the jitter; post.txt still records it)
				prog->uni2f( "jitterNdc", s.redNoJitter ? 0.0f : jitX, s.redNoJitter ? 0.0f : jitY );
				prog->uni2f( "invSize", 1.0f / float( W ), 1.0f / float( H ) );
				r->drawShape( 4, 3, 6, GL_TRIANGLES, GL_UNSIGNED_SHORT, &attrs, idx );
				r->stopProgram();
			}
		}
		if ( ok && dump ) {
			dumpFloat( dd + "mb_in.bin", W, H, 4, readTex( r, g.fbo, g.cur, false, GL_RGBA, 4, W, H ) );
			dumpFloat( dd + "mv.bin", W, H, 2, readTex( r, g.mvFbo, g.mv, false, GL_RG, 2, W, H ) );
		}
		if ( ok && fuse ) {	// lane POSTFX2: the composite and the blur in one pass
			target( g.out );
			auto prog = r->useProgram( runLens ? "game_doflensmblur.prog" : "game_dofmblur.prog" );	// lane DOFLENS1
			if ( !prog ) {
				ok = false;
			} else {
				bind( kUnitCur, g.cur );
				bind( kUnitMv, g.mv );
				bind( kUnitBlur, runLens ? g.lensFar : g.blur[0] );
				bind( kUnitDepth, g.depth );
				if ( runLens ) {
					bind( kUnitNear, g.lensNear );
					prog->uni1i( "lensNear", kUnitNear );
					lensUnis( prog );
				}
				prog->uni1i( "t0", kUnitCur );
				prog->uni1i( "t1", kUnitMv );
				prog->uni1i( "t2", kUnitBlur );
				prog->uni1i( "t3", kUnitDepth );
				prog->uni4f( "dc0", dc0 );
				prog->uni4f( "dc1", dc1 );
				prog->uni4f( "dc2", dc2 );
				prog->uni4f( "dc3", dc3 );
				prog->uni4f( "mc0", mc0 );
				prog->uni4f( "mc1", mc1 );
				prog->uni1i( "farOnly", farOnly ? 1 : 0 );
				prog->uni1i( "dofRed", s.redNoCoc ? 1 : 0 );
				r->drawShape( 4, 3, 6, GL_TRIANGLES, GL_UNSIGNED_SHORT, &attrs, idx );
				r->stopProgram();
			}
		} else if ( ok ) {
			target( g.out );
			auto prog = r->useProgram( "game_mblur.prog" );
			if ( !prog ) {
				ok = false;
			} else {
				bind( kUnitCur, g.cur );
				bind( kUnitMv, g.mv );
				prog->uni1i( "t0", kUnitCur );
				prog->uni1i( "t1", kUnitMv );
				prog->uni4f( "c0", mc0 );
				prog->uni4f( "c1", mc1 );
				r->drawShape( 4, 3, 6, GL_TRIANGLES, GL_UNSIGNED_SHORT, &attrs, idx );
				r->stopProgram();
			}
		}
		if ( ok && dump )
			dumpFloat( dd + "mb_out.bin", W, H, 4, readTex( r, g.fbo, g.out, false, GL_RGBA, 4, W, H ) );
		PassTime::subEnd( PassTime::PostMblur );
	}

	// 3b. lane POSTFX1: the object blur on the picture so far (out after the motion blur, else cur): the pixel
	// vectors without the camera, the tile max and the neighbour max, then the gather into the other texture
	GLuint picture = runMb || ( msKeep && runDof ) ? g.out : g.cur;
	if ( ok && runOb ) {
		PassTime::subBegin( PassTime::PostObjBlur );
		const GLuint dst = runMb ? g.cur : g.out;
		auto pass = [&]( const char * name, GLuint into, int w, int h, auto && set ) {
			if ( !ok )
				return;
			target( into );
			fn->glViewport( 0, 0, w, h );
			auto prog = r->useProgram( name );
			if ( !prog ) {
				ok = false;
				return;
			}
			set( prog );
			r->drawShape( 4, 3, 6, GL_TRIANGLES, GL_UNSIGNED_SHORT, &attrs, idx );
			r->stopProgram();
		};
		pass( "cell_objblur_vec.prog", g.obVec, W, H, [&]( auto * p ) {
			bind( kUnitMv, obVel );
			bind( kUnitBlur, obMask );
			bind( kUnitDepth, g.depth );
			p->uni1i( "velTex", kUnitMv );
			p->uni1i( "maskTex", kUnitBlur );
			p->uni1i( "depthTex", kUnitDepth );
			p->uni4m( "reproj", Matrix4( obReproj ) );
			p->uni2f( "jitterNdc", obJx, obJy );
			p->uni2f( "invSize", 1.0f / float( W ), 1.0f / float( H ) );
			p->uni1f( "shutter", kObShutter );
			p->uni1f( "maxPx", kObMaxPx );
			p->uni1i( "objRed", s.redObCamera ? 1 : 0 );
		} );
		for ( int mode = 0; mode < 2; mode++ )
			pass( "cell_objblur_tile.prog", g.obTile[mode], g.obTw, g.obTh, [&]( auto * p ) {
				bind( kUnitMv, mode ? g.obTile[0] : g.obVec );
				p->uni1i( "srcTex", kUnitMv );
				p->uni1i( "tile", kObTile );
				p->uni1i( "tileMode", mode );
			} );
		pass( "cell_objblur.prog", dst, W, H, [&]( auto * p ) {
			bind( kUnitCur, picture );
			bind( kUnitBlur, g.obVec );
			bind( kUnitMv, g.obTile[1] );
			bind( kUnitDepth, g.depth );
			p->uni1i( "colTex", kUnitCur );
			p->uni1i( "blurTex", kUnitBlur );
			p->uni1i( "neighTex", kUnitMv );
			p->uni1i( "depthTex", kUnitDepth );
			p->uni1i( "tile", kObTile );
			p->uni1i( "samples", kObSamples );
			p->uni1f( "zNear", zNear );
			p->uni1f( "zFar", zFar );
			p->uni1f( "noiseSeed", float( frame % 8 ) * 0.125f );
		} );
		fn->glViewport( vp[0], vp[1], vp[2], vp[3] );
		if ( ok && dump ) {
			dumpFloat( dd + "ob_vec.bin", W, H, 2, readTex( r, g.fbo, g.obVec, false, GL_RG, 2, W, H ) );
			dumpFloat( dd + "ob_out.bin", W, H, 4, readTex( r, g.fbo, dst, false, GL_RGBA, 4, W, H ) );
		}
		if ( ok )
			picture = dst;
		PassTime::subEnd( PassTime::PostObjBlur );
	}

	if ( ok && dump ) {
		QFile f( dd + "post.txt" );
		if ( f.open( QIODevice::WriteOnly | QIODevice::Text ) ) {
			QTextStream t( &f );
			t << "frame " << frame << "\nW " << W << "\nH " << H << "\ndt " << f9( dt )
			  << "\njitter " << ( jittered ? 1 : 0 ) << ' ' << f9( jitX ) << ' ' << f9( jitY )
			  << "\nq2 " << g.qw[0] << ' ' << g.qh[0] << "\nq3 " << g.qw[1] << ' ' << g.qh[1]
			  << "\ndof " << ( runDof ? 1 : 0 ) << "\nfarOnly " << ( farOnly ? 1 : 0 ) << "\nradius " << radius
			  << "\ndc0 " << v4( dc0 ) << "\ndc1 " << v4( dc1 ) << "\ndc2 " << v4( dc2 ) << "\ndc3 " << v4( dc3 )
			  << "\nmb " << ( runMb ? 1 : 0 ) << "\nmc0 " << v4( mc0 ) << "\nmc1 " << v4( mc1 )
			  << "\nreproj";
			const Matrix4 rp = toMatrix4( reproj );
			for ( int i = 0; i < 16; i++ )
				t << ' ' << f9( rp.data()[i] );
			t << "\nred " << ( s.redNoCoc ? "nococ" : s.redNoVelocity ? "novelocity" : s.redNoJitter ? "nojitter" : s.redBlurRow ? "blurrow" : "none" ) << '\n';
		}
		qInfo().noquote() << "cell post: dumped frame" << frame << "to" << s.dumpDir;
	}

	// 4. the picture back into the frame (every sample of a multisampled frame the same value)
	if ( ok && !msKeep ) {
		fn->glBindFramebuffer( GL_DRAW_FRAMEBUFFER, GLuint( prevDraw ) );
		auto prog = r->useProgram( "game_taa_copy.prog" );
		if ( !prog ) {
			ok = false;
		} else {
			bind( kUnitCur, picture );	// after the DoF alone its picture is already in cur
			prog->uni1i( "srcTex", kUnitCur );
			r->drawShape( 4, 3, 6, GL_TRIANGLES, GL_UNSIGNED_SHORT, &attrs, idx );
			r->stopProgram();
		}
	}
	/* 4b. lane POSTFX3: each sample s becomes s * w + (B - w * R) by a dual-source blend (R = cur, the frame's resolve,
	 * B = the picture, w = min(1, B / R)): the samples' mean is B, none goes negative, a pixel the blur left alone keeps
	 * its samples bit for bit, and the alpha (dst) stays the sample's own */
	if ( ok && msKeep ) {
		GLint bEq = 0, bEqA = 0, bSrc = 0, bDst = 0, bSrcA = 0, bDstA = 0;
		fn->glGetIntegerv( GL_BLEND_EQUATION_RGB, &bEq );
		fn->glGetIntegerv( GL_BLEND_EQUATION_ALPHA, &bEqA );
		fn->glGetIntegerv( GL_BLEND_SRC_RGB, &bSrc );
		fn->glGetIntegerv( GL_BLEND_DST_RGB, &bDst );
		fn->glGetIntegerv( GL_BLEND_SRC_ALPHA, &bSrcA );
		fn->glGetIntegerv( GL_BLEND_DST_ALPHA, &bDstA );
		fn->glBindFramebuffer( GL_DRAW_FRAMEBUFFER, GLuint( prevDraw ) );
		auto prog = r->useProgram( "cell_post_mscopy.prog" );
		if ( !prog ) {
			ok = false;
		} else {
			bind( kUnitCur, picture );
			bind( kUnitBlur, g.cur );
			prog->uni1i( "srcTex", kUnitCur );
			prog->uni1i( "refTex", kUnitBlur );
			fn->glEnable( GL_BLEND );
			fn->glBlendEquationSeparate( GL_FUNC_ADD, GL_FUNC_ADD );
			fn->glBlendFuncSeparate( GL_ONE, GL_SRC1_COLOR, GL_ZERO, GL_ONE );
			r->drawShape( 4, 3, 6, GL_TRIANGLES, GL_UNSIGNED_SHORT, &attrs, idx );
			r->stopProgram();
			fn->glBlendEquationSeparate( GLenum( bEq ), GLenum( bEqA ) );
			fn->glBlendFuncSeparate( GLenum( bSrc ), GLenum( bDst ), GLenum( bSrcA ), GLenum( bDstA ) );
		}
	}

	for ( int u : { kUnitCur, kUnitBlur, kUnitDepth, kUnitMv } ) {
		fn->glActiveTexture( GLenum( GL_TEXTURE0 + u ) );
		fn->glBindTexture( GL_TEXTURE_2D, 0 );
	}
	fn->glActiveTexture( GLenum( prevActive ) );
	if ( wasStencil ) fn->glEnable( GL_STENCIL_TEST ); else fn->glDisable( GL_STENCIL_TEST );
	if ( wasDepth ) fn->glEnable( GL_DEPTH_TEST ); else fn->glDisable( GL_DEPTH_TEST );
	if ( wasBlend ) fn->glEnable( GL_BLEND ); else fn->glDisable( GL_BLEND );
	if ( wasCull ) fn->glEnable( GL_CULL_FACE ); else fn->glDisable( GL_CULL_FACE );
	if ( wasScissor ) fn->glEnable( GL_SCISSOR_TEST );
	fn->glColorMask( colorMask[0], colorMask[1], colorMask[2], colorMask[3] );
	fn->glDepthMask( depthMask );
	fn->glBindFramebuffer( GL_DRAW_FRAMEBUFFER, GLuint( prevDraw ) );
	fn->glBindFramebuffer( GL_READ_FRAMEBUFFER, GLuint( prevRead ) );
	if ( !ok ) {
		s.haveLast = false;
		const QString why = blitErr != GL_NO_ERROR ? QStringLiteral( "refused (the frame's blit failed, GL error %1)" ).arg( blitErr )
			: QStringLiteral( "refused (a game_dof / game_mblur / cell_objblur program is missing)" );
		if ( runDof )
			noteDof( s, why );
		if ( runMb )
			noteMb( s, why, why );
		if ( runOb )
			noteOb( s, why, why );
	}
}

}	// namespace

void wwCellPostApply( Scene * scene )
{
	postApply( scene, false );
}

void wwCellPostApplyLinear( Scene * scene )
{
	postApply( scene, true );
}
