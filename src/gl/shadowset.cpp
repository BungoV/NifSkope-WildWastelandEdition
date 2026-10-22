/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#include "shadowset.h"
#include "wwswitch.h"	// lane TESTDRIVER1: the WW_* switch store

#include "gl/cellunits.h"	// lane HARNESSMIN1 (testint): the terrain/contact maps registered on 31/32
#include "gl/cellssr.h"		// the cell view's shared opaque pass (contact shadows read its depth)
#include "gl/glnode.h"
#include "gl/glscene.h"
#include "gl/glshape.h"
#include "gl/lookdevstage.h"
#include "gl/renderer.h"
#include "harnesswindow.h"

#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QOpenGLContext>
#include <QSettings>
#include <QStringList>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <future>
#include <limits>
#include <vector>

/* See shadowset.h. Design sources (ported, written fresh for OpenGL): FO4CS docs/plans/unified-shadows-plan.md
 * (the composition), far-field-shadows-plan.md (the sweep, its cadence and failure contract),
 * DirectionalShadowCascadePolicy.h (the filter law), SunSync.h (one sun). */

namespace
{

constexpr double kPi = 3.14159265358979323846;
constexpr double kDeg = kPi / 180.0;
constexpr float kNoHeight = -1.0e30f;
constexpr double kSunRadius = 0.265;	// degrees: the penumbra's half-angle
constexpr double kRelatch = 0.25;	// degrees: the sun moved this far -> a new sweep
constexpr int kUploadBytes = 512 * 1024;	// the finished map goes up this much a frame
constexpr double kLatitude = 42.36;	// Boston

const char * const kCascadeKey = "Settings/Render/Scene/Shadow Cascade Quality";
const char * const kContactKey = "Settings/Render/Scene/Contact Shadows";
const char * const kTerrainKey = "Settings/Render/Scene/Terrain Shadows";
const char * const kSunPathKey = "Settings/Render/Scene/Sun Path";

struct GpuTimer
{
	unsigned int q[4] = { 0, 0, 0, 0 };	// lane TERRSHADOW1: two slots of begin/end GL_TIMESTAMP counters
	bool pending[2] = { false, false };
	int cur = 0;
	double ms = -1.0;
};

//! a terrain shape as the worker sees it: implicitly shared copies (the render thread may replace its own)
struct TerrSrc
{
	Transform xf;
	QVector<Vector3> verts;
	QVector<Triangle> tris;
};

struct Terrain
{
	// the grid
	int N = 0;
	double T = 160.0, x0 = 0, y0 = 0, cx = 1e300, cy = 1e300;
	std::vector<float> H;
	float zmin = 0, zmax = 0;
	// the encoded range: a pad below the lowest terrain so a shadow height decays past it onto the receivers
	// below (lane TERRSHADOW1: S under zmin clamped to zmin made the shadow never end over lower ground)
	float encLo = 0, encSpan = 1;
	quint64 sig = 0;
	int shapes = 0, tris = 0;
	double rasterMs = 0;
	int checkIn = 0;

	// the sweep in flight
	bool running = false;
	int line = 0;
	double dir[2] = { 1, 0 }, tanLo = 0, tanHi = 0, elev = 0, az = 0, latched[3] = { 0, 0, 1 };
	bool majorX = true;
	int sMaj = 1;
	double vMin = 0, stepLen = 1;
	std::vector<float> gLo, gHi, nLo;	// the rays' horizons (lo, hi limb); the line's heights
	std::vector<std::uint16_t> out;
	double sweepMs = 0, frameMs = 0;
	int linesPerFrame = 0;
	quint64 sweepSig = 0;
	float swLo = 0, swSpan = 1;	// the encoded range, latched at the sweep's start

	// published (the front texture)
	bool valid = false;
	int pubN = 0;
	double pubX0 = 0, pubY0 = 0, pubT = 1, pubElev = 0, pubAz = 0, pubTanLo = 0, pubTanHi = 0, pubDir[2] = { 1, 0 };
	float pubZmin = 0, pubSpan = 1;
	std::vector<std::uint16_t> pubOut;
	int sweeps = 0;

	// the render thread's side of the worker: the set to raster next, the upload in flight, the frame costs
	std::vector<TerrSrc> src;
	bool needRaster = false;
	int setShapes = 0;
	bool uploading = false;
	int upRow = 0;
	double frameMax = 0, upMs = 0;
	int frames = 0;

	// GL
	const void * ctx = nullptr;
	unsigned int tex[2] = { 0, 0 };
	int texN = 0;
	int front = 0;
	QString last = QStringLiteral( "off" );
};

//! the raster and the sweep run here, off the render thread (lane TERRSHADOW1: 600 ms of CPU at Ultra stalled a
//! frame); the render thread hands it a grid, a set and a sun, and uploads the finished map a band a frame
struct TerrJob
{
	Terrain w;	// the CPU half only: grid, heights, sweep, out
	std::vector<TerrSrc> src;
	bool raster = false, flip = false, nodecay = false;
	double toSun[3] = { 0, 0, 1 };
	std::future<void> fut;
	bool busy = false;
	std::atomic<bool> stop { false };
	double workMs = 0;
};

struct Contact
{
	const void * ctx = nullptr;
	unsigned int tex = 0, fbo = 0;
	int w = 0, h = 0, fullW = 0, fullH = 0;
	bool valid = false;
	const void * doc = nullptr;
	QString last = QStringLiteral( "off" );
};

struct SetState
{
	bool init = false;
	quint64 liveGen = 0;	// lane TESTDRIVER1: the switch generation last read
	int cascade = 0, contact = 0, terrain = 0, sunPath = 0;
	bool pinCascade = false, pinContact = false, pinTerrain = false, pinSunPath = false;
	bool sunPin = false;
	double pinElev = 0, pinAz = 0;
	int view = 0;
	bool sync = false;
	bool terrainAll = false;	// WW_SHADOW_TERRAIN_ALL=1: every visible shape is terrain (the known-answer fixtures)
	QString dump, echo;
	QStringList reds;
	bool unitsOk = true;
	int unitsChecked = 0;

	Terrain t;
	TerrJob job;
	Contact c;
	GpuTimer tmTerrain, tmContact;
	double lastElevReal = 0, lastElevShaded = 0, lastElevEngine = 0, lastDiscEngine = 0;
	QString sunLast = QStringLiteral( "engine" );
};

int tierFrom( const QString & e )
{
	bool ok = false;
	const int v = e.trimmed().toInt( &ok );
	if ( ok )
		return std::clamp( v, 0, 4 );
	const QString l = e.trimmed().toLower();
	return l == QLatin1StringView( "low" ) ? 1 : l == QLatin1StringView( "medium" ) ? 2 : l == QLatin1StringView( "high" ) ? 3
		: l == QLatin1StringView( "ultra" ) ? 4 : 0;
}

SetState & ss()
{
	static SetState s;
	// lane TESTDRIVER1: a live set of these switches re-reads them (pins and or-ed reds back to their defaults first)
	if ( !s.init || wwSwitchMovedFor( s.liveGen, { "WW_SHADOW_", "WW_SUNPATH" } ) ) {
		s.init = true;
		s.liveGen = wwSwitchGen();
		{
			const SetState d {};
			s.cascade = d.cascade;
			s.contact = d.contact;
			s.terrain = d.terrain;
			s.sunPath = d.sunPath;
			s.pinCascade = d.pinCascade;
			s.pinContact = d.pinContact;
			s.pinTerrain = d.pinTerrain;
			s.pinSunPath = d.pinSunPath;
			s.sunPin = d.sunPin;
			s.pinElev = d.pinElev;
			s.pinAz = d.pinAz;
		}
		QSettings q;
		auto tier = [&q]( const char * env, const char * key, int & v, bool & pin ) {
			const QString e = wwEnvStr( env );
			if ( !e.trimmed().isEmpty() ) {
				v = tierFrom( e );
				pin = true;
			} else if ( wwHarnessRun() ) {
				v = 0;	// a harness measures what it pins
				pin = true;
			} else {
				v = std::clamp( q.value( QLatin1StringView( key ), 0 ).toInt(), 0, 4 );
			}
		};
		tier( "WW_SHADOW_CASCADE", kCascadeKey, s.cascade, s.pinCascade );
		tier( "WW_SHADOW_CONTACT", kContactKey, s.contact, s.pinContact );
		tier( "WW_SHADOW_TERRAIN", kTerrainKey, s.terrain, s.pinTerrain );
		tier( "WW_SUNPATH", kSunPathKey, s.sunPath, s.pinSunPath );
		s.sunPath = std::clamp( s.sunPath, 0, 1 );
		const QStringList pin = wwEnvStr( "WW_SUNPATH_PIN" ).split( QChar( ',' ), Qt::SkipEmptyParts );
		if ( pin.size() == 2 ) {
			bool a = false, b = false;
			s.pinElev = pin.at( 0 ).toDouble( &a );
			s.pinAz = pin.at( 1 ).toDouble( &b );
			s.sunPin = a && b;
		}
		s.view = wwEnvStr( "WW_SHADOW_VIEW" ).toInt();
		s.sync = wwEnvStr( "WW_SHADOW_TERRAIN_SYNC" ) == QLatin1StringView( "1" );
		s.terrainAll = wwEnvStr( "WW_SHADOW_TERRAIN_ALL" ) == QLatin1StringView( "1" );
		s.dump = wwEnvStr( "WW_SHADOW_DUMP" ).trimmed();
		if ( !QDir::isAbsolutePath( s.dump ) )
			s.dump.clear();
		s.echo = wwEnvStr( "WW_SHADOW_ECHO" ).trimmed();
		if ( !QDir::isAbsolutePath( s.echo ) )
			s.echo.clear();
		s.reds = wwEnvStr( "WW_SHADOW_RED" ).split( QChar( ',' ), Qt::SkipEmptyParts );
		for ( QString & r : s.reds )
			r = r.trimmed().toLower();
	}
	return s;
}

void setTier( int & v, int tier, bool pin, const char * key )
{
	v = std::clamp( tier, 0, 4 );
	if ( !pin )
		QSettings().setValue( QLatin1StringView( key ), v );
}

double dot3( const double * a, const double * b )
{
	return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

bool norm3( double * v )
{
	const double l = std::sqrt( dot3( v, v ) );
	if ( l < 1e-12 )
		return false;
	for ( int i = 0; i < 3; i++ )
		v[i] /= l;
	return true;
}

// ---- GPU timers (read a frame late, never stall) ----
// Timestamp pairs, not GL_TIME_ELAPSED: elapsed queries cannot nest, and a producer's End closed the frame
// timer (WW_RENDER_GPUMS, glview.cpp) early, so the frame read ~13 ms with contact shadows on.
// Red WW_SHADOW_RED=elapsedtimer brings the elapsed query back.
namespace {
constexpr GLenum kTimeElapsed = 0x88BF;
constexpr GLenum kTimestamp = 0x8E28;
bool elapsedRed()
{
	WW_SWITCH_STATIC( bool, r, wwEnvStr( "WW_SHADOW_RED" ).split( QChar( ',' ) ).contains( QStringLiteral( "elapsedtimer" ) ) );
	return r;
}
void timerRead( NifSkopeOpenGLContext::GLFunctions * fn, GpuTimer & t, int slot, bool wait )
{
	if ( !t.pending[slot] )
		return;
	const GLuint last = elapsedRed() ? t.q[slot * 2] : t.q[slot * 2 + 1];
	if ( !wait ) {
		GLuint avail = 0;
		fn->glGetQueryObjectuiv( last, GL_QUERY_RESULT_AVAILABLE, &avail );
		if ( !avail )
			return;
	}
	if ( elapsedRed() ) {
		GLuint ns = 0;
		fn->glGetQueryObjectuiv( t.q[slot * 2], GL_QUERY_RESULT, &ns );
		t.ms = double( ns ) * 1e-6;
	} else {
		GLuint64 a = 0, b = 0;
		fn->glGetQueryObjectui64v( t.q[slot * 2], GL_QUERY_RESULT, &a );
		fn->glGetQueryObjectui64v( t.q[slot * 2 + 1], GL_QUERY_RESULT, &b );
		t.ms = b > a ? double( b - a ) * 1e-6 : 0.0;
	}
	t.pending[slot] = false;
}
}

void timerBegin( NifSkopeOpenGLContext::GLFunctions * fn, GpuTimer & t )
{
	if ( !t.q[0] )
		fn->glGenQueries( 4, t.q );
	timerRead( fn, t, 1 - t.cur, false );
	timerRead( fn, t, t.cur, true );	// both in flight: take the older one (a wait of at most a frame)
	if ( elapsedRed() )
		fn->glBeginQuery( kTimeElapsed, t.q[t.cur * 2] );
	else
		fn->glQueryCounter( t.q[t.cur * 2], kTimestamp );
}

void timerEnd( NifSkopeOpenGLContext::GLFunctions * fn, GpuTimer & t )
{
	if ( elapsedRed() )
		fn->glEndQuery( kTimeElapsed );
	else
		fn->glQueryCounter( t.q[t.cur * 2 + 1], kTimestamp );
	t.pending[t.cur] = true;
	t.cur = 1 - t.cur;
}

bool unitsOk( NifSkopeOpenGLContext::GLFunctions * fn )
{
	SetState & s = ss();
	if ( !s.unitsChecked ) {
		GLint units = 0;
		fn->glGetIntegerv( GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, &units );	// lane HARNESSMIN1: a unit index is bindable up to combined - 1
		s.unitsOk = units > WwCellUnit::ContactShadow;
		s.unitsChecked = 1;
	}
	return s.unitsOk;
}

// ---- (C) terrain ----

struct TerrPreset
{
	int N;
	double T;
};

TerrPreset terrPreset( int tier )
{
	switch ( tier ) {
	case 1: return { 512, 160.0 };	// 82 k units across
	case 2: return { 1024, 160.0 };	// 164 k
	case 3: return { 2048, 160.0 };	// 328 k
	default: return { 4096, 128.0 };	// 524 k
	}
}

//! a terrain shape: the cells' LAND (under a node named "ground") or the far LOD's terrain
bool isTerrainShape( const Shape * sh )
{
	if ( ss().terrainAll )
		return true;
	if ( sh->getName().startsWith( QLatin1StringView( "FarLOD terrain" ) ) )
		return true;
	for ( const Node * p = sh->parentNode(); p; p = p->parentNode() ) {
		const QString & n = p->getName();
		if ( n.compare( QLatin1StringView( "ground" ), Qt::CaseInsensitive ) == 0 || n.startsWith( QLatin1StringView( "FarLOD terrain" ) ) )
			return true;
	}
	return false;
}

void collectTerrain( Scene * scene, std::vector<const Shape *> & out, quint64 & sig )
{
	out.clear();
	sig = 1469598103934665603ull;
	auto mix = [&sig]( quint64 v ) {
		sig ^= v;
		sig *= 1099511628211ull;
	};
	for ( Node * node : scene->nodes.list() ) {
		const Shape * sh = dynamic_cast<const Shape *>( node );
		if ( !sh || !sh->isVisible() || sh->verts.isEmpty() || sh->triangles.isEmpty() || !isTerrainShape( sh ) )
			continue;
		out.push_back( sh );
		const Transform & w = sh->worldTrans();
		mix( quint64( reinterpret_cast<quintptr>( sh ) ) );
		mix( quint64( sh->verts.size() ) );
		mix( quint64( sh->triangles.size() ) );
		quint32 b[4];
		const float f[4] = { w.translation[0], w.translation[1], w.translation[2], w.scale };
		std::memcpy( b, f, sizeof( b ) );
		for ( quint32 v : b )
			mix( v );
	}
	mix( quint64( out.size() ) );
}

//! the heights at the texel centres (point samples of the surface; the max where surfaces overlap)
void rasterTerrain( Terrain & t, const std::vector<TerrSrc> & shapes, const std::atomic<bool> & stop )
{
	QElapsedTimer tm;
	tm.start();
	const int N = t.N;
	t.H.assign( size_t( N ) * size_t( N ), kNoHeight );
	t.tris = 0;
	std::vector<Vector3> w;
	for ( const TerrSrc & sh : shapes ) {
		if ( stop.load( std::memory_order_relaxed ) )
			return;
		const Transform & xf = sh.xf;
		w.resize( size_t( sh.verts.size() ) );
		for ( qsizetype v = 0; v < sh.verts.size(); v++ )
			w[size_t( v )] = xf * sh.verts.at( v );
		for ( const Triangle & tri : sh.tris ) {
			if ( tri[0] >= w.size() || tri[1] >= w.size() || tri[2] >= w.size() )
				continue;
			const Vector3 & a = w[tri[0]], & b = w[tri[1]], & c = w[tri[2]];
			// texel space: centre of texel i at x0 + (i + 0.5) T
			const double ax = ( a[0] - t.x0 ) / t.T - 0.5, ay = ( a[1] - t.y0 ) / t.T - 0.5;
			const double bx = ( b[0] - t.x0 ) / t.T - 0.5, by = ( b[1] - t.y0 ) / t.T - 0.5;
			const double cx = ( c[0] - t.x0 ) / t.T - 0.5, cy = ( c[1] - t.y0 ) / t.T - 0.5;
			const double area = ( bx - ax ) * ( cy - ay ) - ( by - ay ) * ( cx - ax );
			if ( std::fabs( area ) < 1e-12 )
				continue;
			const int i0 = std::max( 0, int( std::ceil( std::min( { ax, bx, cx } ) ) ) );
			const int i1 = std::min( N - 1, int( std::floor( std::max( { ax, bx, cx } ) ) ) );
			const int j0 = std::max( 0, int( std::ceil( std::min( { ay, by, cy } ) ) ) );
			const int j1 = std::min( N - 1, int( std::floor( std::max( { ay, by, cy } ) ) ) );
			if ( i0 > i1 || j0 > j1 )
				continue;
			t.tris++;
			const double inv = 1.0 / area, eps = -1e-7;
			for ( int j = j0; j <= j1; j++ ) {
				for ( int i = i0; i <= i1; i++ ) {
					const double px = i, py = j;
					const double wa = ( ( bx - px ) * ( cy - py ) - ( by - py ) * ( cx - px ) ) * inv;
					const double wb = ( ( cx - px ) * ( ay - py ) - ( cy - py ) * ( ax - px ) ) * inv;
					const double wc = 1.0 - wa - wb;
					if ( wa < eps || wb < eps || wc < eps )
						continue;
					const float z = float( wa * a[2] + wb * b[2] + wc * c[2] );
					float & h = t.H[size_t( j ) * size_t( N ) + size_t( i )];
					h = std::max( h, z );
				}
			}
		}
	}
	t.zmin = std::numeric_limits<float>::max();
	t.zmax = -std::numeric_limits<float>::max();
	for ( float h : t.H ) {
		if ( h <= kNoHeight )
			continue;
		t.zmin = std::min( t.zmin, h );
		t.zmax = std::max( t.zmax, h );
	}
	if ( t.zmin > t.zmax ) {
		t.zmin = 0;
		t.zmax = 1;
	}
	t.encLo = t.zmin - std::max( 4096.0f, t.zmax - t.zmin );
	t.encSpan = std::max( t.zmax - t.encLo, 1.0f );
	t.shapes = int( shapes.size() );
	t.rasterMs = double( tm.nsecsElapsed() ) * 1e-6;
}

float heightAt( const Terrain & t, int i, int j )
{
	return t.H[size_t( j ) * size_t( t.N ) + size_t( i )];
}

//! a line of heights at a fractional minor index; -inf off the map or where no terrain was
float sampleLine( const std::vector<float> & g, double m )
{
	const int n = int( g.size() );
	if ( m < 0.0 || m > double( n - 1 ) )
		return kNoHeight;
	const int m0 = int( std::floor( m ) );
	const int m1 = std::min( m0 + 1, n - 1 );
	const float f = float( m - m0 );
	const float a = g[size_t( m0 )], b = g[size_t( m1 )];
	if ( a > kNoHeight && b > kNoHeight )
		return a + ( b - a ) * f;
	if ( a > kNoHeight )
		return f < 0.5f ? a : kNoHeight;
	if ( b > kNoHeight )
		return f >= 0.5f ? b : kNoHeight;
	return kNoHeight;
}

std::uint16_t encodeS( float S, float zmin, float span )
{
	// 0 = no shadow height (nothing toward the sun, or decayed below the encoded range): lit
	if ( S <= kNoHeight * 0.5f || S < zmin )
		return 0;
	const double v = ( double( S ) - double( zmin ) ) / double( span );
	return std::uint16_t( std::max( 1L, std::lround( std::min( v, 1.0 ) * 65535.0 ) ) );
}

void startSweep( Terrain & t, const double toSun[3], bool flip, bool nodecay )
{
	const double hz = std::sqrt( toSun[0] * toSun[0] + toSun[1] * toSun[1] );
	t.elev = std::atan2( toSun[2], hz ) / kDeg;
	t.az = std::atan2( toSun[0], toSun[1] ) / kDeg;
	t.dir[0] = toSun[0] / hz;
	t.dir[1] = toSun[1] / hz;
	if ( flip ) {
		t.dir[0] = -t.dir[0];
		t.dir[1] = -t.dir[1];
	}
	const double lo = std::max( t.elev - kSunRadius, 0.05 ), hi = t.elev + kSunRadius;
	t.tanLo = nodecay ? 0.0 : std::tan( lo * kDeg );
	t.tanHi = nodecay ? 0.0 : std::tan( std::min( hi, 89.0 ) * kDeg );
	t.majorX = std::fabs( t.dir[0] ) >= std::fabs( t.dir[1] );
	const double dMaj = t.majorX ? t.dir[0] : t.dir[1], dMin = t.majorX ? t.dir[1] : t.dir[0];
	t.sMaj = dMaj >= 0 ? 1 : -1;
	t.vMin = dMin / std::fabs( dMaj );
	t.stepLen = t.T / std::fabs( dMaj );	// world length of one step (one texel along the major axis)
	for ( int k = 0; k < 3; k++ )
		t.latched[k] = toSun[k];
	// one ray per minor texel, enough of them that the sheared fan covers every texel of every line
	const int rays = t.N + int( std::ceil( double( t.N ) * std::fabs( t.vMin ) ) ) + 2;
	t.gLo.assign( size_t( rays ), kNoHeight );
	t.gHi.assign( size_t( rays ), kNoHeight );
	t.nLo.assign( size_t( t.N ), kNoHeight );	// the current line's heights
	t.out.assign( size_t( t.N ) * size_t( t.N ) * 2, 0 );
	t.line = 0;
	t.running = true;
	t.sweepMs = 0;
	t.sweepSig = t.sig;
	t.swLo = t.encLo;
	t.swSpan = t.encSpan;
}

//! lines [line, line + count) of the sweep; returns the band of major indices written.
//! Rays run exactly along the sun's direction (one per minor texel, sheared by vMin a line) and carry their
//! own horizon; each texel reads the two rays either side of it. Resampling the horizon onto the texel grid
//! every line instead drifts sideways off-axis (nearest rounding compounds: a 30-degree shadow ran at 45).
void sweepLines( Terrain & t, int count, int & aLo, int & aHi )
{
	const int N = t.N;
	const int R = int( t.gLo.size() );
	const float zmin = t.swLo, span = t.swSpan;
	const float dLo = float( t.stepLen * t.tanLo ), dHi = float( t.stepLen * t.tanHi );
	// ray r sits at minor r + base - line * vMin: base keeps the fan over [0, N - 1] on every line
	const double base = t.vMin < 0.0 ? std::floor( double( N ) * t.vMin ) : 0.0;
	aLo = N;
	aHi = -1;
	for ( int k = 0; k < count && t.line < N; k++, t.line++ ) {
		const int a = t.sMaj > 0 ? N - 1 - t.line : t.line;
		aLo = std::min( aLo, a );
		aHi = std::max( aHi, a );
		const double c = base - double( t.line ) * t.vMin;
		for ( int m = 0; m < N; m++ )
			t.nLo[size_t( m )] = heightAt( t, t.majorX ? a : m, t.majorX ? m : a );
		// one step down the sun line: the carried horizon decays
		if ( t.line > 0 ) {
			for ( int r = 0; r < R; r++ ) {
				if ( t.gLo[size_t( r )] > kNoHeight )
					t.gLo[size_t( r )] -= dLo;
				if ( t.gHi[size_t( r )] > kNoHeight )
					t.gHi[size_t( r )] -= dHi;
			}
		}
		// each texel: the rays either side (the horizon toward the sun, not counting its own height)
		for ( int m = 0; m < N; m++ ) {
			const double q = double( m ) - c;
			const int r0 = int( std::floor( q ) );
			const float w = float( q - r0 );
			auto pick = [&]( const std::vector<float> & g ) {
				const float A = ( r0 >= 0 && r0 < R ) ? g[size_t( r0 )] : kNoHeight;
				const float B = ( r0 + 1 >= 0 && r0 + 1 < R ) ? g[size_t( r0 + 1 )] : kNoHeight;
				if ( A > kNoHeight && B > kNoHeight )
					return A + ( B - A ) * w;
				if ( A > kNoHeight )
					return w < 0.5f ? A : kNoHeight;
				if ( B > kNoHeight )
					return w >= 0.5f ? B : kNoHeight;
				return kNoHeight;
			};
			const float sLo = pick( t.gLo ), sHi = pick( t.gHi );
			const int i = t.majorX ? a : m, j = t.majorX ? m : a;
			const size_t o = ( size_t( j ) * size_t( N ) + size_t( i ) ) * 2;
			t.out[o + 0] = encodeS( sLo, zmin, span );	// .x lit above (the lower limb clears)
			t.out[o + 1] = encodeS( sHi, zmin, span );	// .y shadowed below (the upper limb is hidden)
		}
		// then every ray meets the terrain under it on this line
		for ( int r = 0; r < R; r++ ) {
			const float h = sampleLine( t.nLo, double( r ) + c );
			if ( h > kNoHeight ) {
				t.gLo[size_t( r )] = std::max( t.gLo[size_t( r )], h );
				t.gHi[size_t( r )] = std::max( t.gHi[size_t( r )], h );
			}
		}
	}
}

bool ensureTerrTex( NifSkopeOpenGLContext::GLFunctions * fn, Terrain & t )
{
	const void * ctx = QOpenGLContext::currentContext();
	if ( ctx != t.ctx ) {
		t.ctx = ctx;
		t.tex[0] = t.tex[1] = 0;
		t.texN = 0;
		t.valid = false;
	}
	if ( t.tex[0] && t.texN == t.N )
		return true;
	if ( !t.tex[0] )
		fn->glGenTextures( 2, t.tex );
	GLint prev = 0;
	fn->glGetIntegerv( GL_TEXTURE_BINDING_2D, &prev );
	for ( int k = 0; k < 2; k++ ) {
		fn->glBindTexture( GL_TEXTURE_2D, t.tex[k] );
		fn->glTexImage2D( GL_TEXTURE_2D, 0, GL_RG16, t.N, t.N, 0, GL_RG, GL_UNSIGNED_SHORT, nullptr );
		fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
		fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
		fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
		fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
	}
	fn->glBindTexture( GL_TEXTURE_2D, GLuint( prev ) );
	t.texN = t.N;
	t.valid = false;
	return t.tex[0] != 0;
}

//! rows [r0, r1) of the finished map into the back texture
void uploadRows( NifSkopeOpenGLContext::GLFunctions * fn, Terrain & t, const std::vector<std::uint16_t> & out, int r0, int r1 )
{
	if ( r0 >= r1 )
		return;
	GLint prev = 0, rl = 0, sp = 0, sr = 0, al = 4;
	fn->glGetIntegerv( GL_TEXTURE_BINDING_2D, &prev );
	fn->glGetIntegerv( GL_UNPACK_ROW_LENGTH, &rl );
	fn->glGetIntegerv( GL_UNPACK_SKIP_PIXELS, &sp );
	fn->glGetIntegerv( GL_UNPACK_SKIP_ROWS, &sr );
	fn->glGetIntegerv( GL_UNPACK_ALIGNMENT, &al );
	fn->glBindTexture( GL_TEXTURE_2D, t.tex[1 - t.front] );
	fn->glPixelStorei( GL_UNPACK_ALIGNMENT, 4 );
	fn->glPixelStorei( GL_UNPACK_ROW_LENGTH, 0 );
	fn->glPixelStorei( GL_UNPACK_SKIP_PIXELS, 0 );
	fn->glPixelStorei( GL_UNPACK_SKIP_ROWS, 0 );
	fn->glTexSubImage2D( GL_TEXTURE_2D, 0, 0, r0, t.N, r1 - r0, GL_RG, GL_UNSIGNED_SHORT, out.data() + size_t( r0 ) * size_t( t.N ) * 2 );
	fn->glPixelStorei( GL_UNPACK_ROW_LENGTH, rl );
	fn->glPixelStorei( GL_UNPACK_SKIP_PIXELS, sp );
	fn->glPixelStorei( GL_UNPACK_SKIP_ROWS, sr );
	fn->glPixelStorei( GL_UNPACK_ALIGNMENT, al );
	fn->glBindTexture( GL_TEXTURE_2D, GLuint( prev ) );
}

//! WW_SHADOW_DUMP: the published map and the heights it was swept from (tests/spells/terrshadow1_march.py)
void dumpTerrain( const Terrain & t, const std::vector<float> & H, const QString & path )
{
	QFile f( path );
	if ( !f.open( QIODevice::WriteOnly | QIODevice::Truncate ) )
		return;
	const qint32 n = t.pubN;
	f.write( reinterpret_cast<const char *>( &n ), 4 );
	const double hd[12] = { t.pubX0, t.pubY0, t.pubT, double( t.pubZmin ), double( t.pubSpan ), t.pubDir[0], t.pubDir[1],
		t.pubTanLo, t.pubTanHi, t.pubElev, t.pubAz, double( kNoHeight ) };
	f.write( reinterpret_cast<const char *>( hd ), sizeof( hd ) );
	f.write( reinterpret_cast<const char *>( H.data() ), qint64( H.size() * sizeof( float ) ) );
	f.write( reinterpret_cast<const char *>( t.pubOut.data() ), qint64( t.pubOut.size() * sizeof( std::uint16_t ) ) );
}

//! the worker: the raster when the set changed, then the whole sweep
void runTerrJob( TerrJob * j )
{
	QElapsedTimer tm;
	tm.start();
	Terrain & w = j->w;
	if ( j->raster ) {
		rasterTerrain( w, j->src, j->stop );
		j->src.clear();
	}
	startSweep( w, j->toSun, j->flip, j->nodecay );
	QElapsedTimer sw;
	sw.start();
	while ( w.line < w.N && !j->stop.load( std::memory_order_relaxed ) ) {
		int aLo = 0, aHi = -1;
		sweepLines( w, 64, aLo, aHi );
	}
	w.sweepMs = double( sw.nsecsElapsed() ) * 1e-6;
	w.running = false;
	j->workMs = double( tm.nsecsElapsed() ) * 1e-6;
}

void stopTerrJob( TerrJob & j )
{
	if ( !j.busy )
		return;
	j.stop = true;
	j.fut.wait();
	j.fut = std::future<void>();
	j.busy = false;
	j.stop = false;
}

void terrainFrame( Scene * scene, const double toSun[3] )
{
	SetState & s = ss();
	Terrain & t = s.t;
	TerrJob & j = s.job;
	Renderer * r = scene->renderer;
	auto fn = r->fn;
	if ( !s.terrain ) {
		stopTerrJob( j );
		j.w.H.clear();
		j.w.H.shrink_to_fit();
		j.w.out.clear();
		j.w.out.shrink_to_fit();
		j.src.clear();
		t.src.clear();
		t.valid = false;
		t.uploading = false;
		t.pubOut.clear();
		t.pubOut.shrink_to_fit();
		t.sig = 0;
		t.setShapes = 0;
		t.cx = t.cy = 1e300;
		t.frameMax = 0;
		t.frames = 0;
		t.last = QStringLiteral( "off" );
		return;
	}
	if ( !unitsOk( fn ) ) {
		t.valid = false;
		t.last = QStringLiteral( "refused(fewer than 26 texture units)" );
		return;
	}
	QElapsedTimer tm;
	tm.start();
	const TerrPreset p = terrPreset( s.terrain );
	bool regrid = p.N != t.N || p.T != t.T;
	t.N = p.N;
	t.T = p.T;
	// world-locked, re-centred on the camera in eighths of the map
	const Transform & vt = scene->view;
	const double sc = vt.scale != 0.0f ? double( vt.scale ) : 1.0;
	double cam[3];
	for ( int a = 0; a < 3; a++ ) {
		double c = 0;
		for ( int k = 0; k < 3; k++ )
			c -= vt.rotation( k, a ) * vt.translation[k];
		cam[a] = c / sc;
	}
	const double snap = t.N * t.T / 8.0;
	if ( std::fabs( cam[0] - t.cx ) > t.N * t.T / 4.0 || std::fabs( cam[1] - t.cy ) > t.N * t.T / 4.0 || regrid ) {
		t.cx = std::round( cam[0] / snap ) * snap;
		t.cy = std::round( cam[1] / snap ) * snap;
		regrid = true;
	}
	t.x0 = t.cx - t.N * t.T * 0.5;
	t.y0 = t.cy - t.N * t.T * 0.5;
	if ( !ensureTerrTex( fn, t ) ) {
		t.last = QStringLiteral( "refused(no GL texture)" );
		return;
	}
	if ( t.uploading && j.w.N != t.N )
		t.uploading = false;	// a map for the old grid size (the textures were just re-made): drop it
	// the terrain set: checked every 16 frames (and on a re-grid); a change hands the worker a snapshot to raster
	if ( regrid || t.sig == 0 || --t.checkIn <= 0 ) {
		t.checkIn = 16;
		std::vector<const Shape *> shapes;
		quint64 sig = 0;
		collectTerrain( scene, shapes, sig );
		sig ^= quint64( std::llround( t.x0 ) ) * 31ull ^ quint64( std::llround( t.y0 ) ) * 17ull ^ quint64( t.N );
		if ( sig != t.sig ) {
			t.sig = sig;
			t.src.clear();
			t.src.reserve( shapes.size() );
			for ( const Shape * sh : shapes )
				t.src.push_back( { sh->worldTrans(), sh->verts, sh->triangles } );
			t.setShapes = int( shapes.size() );
			t.needRaster = true;
		}
	}
	if ( t.setShapes == 0 ) {
		t.valid = false;
		t.last = QStringLiteral( "on(no terrain shapes)" );
		return;
	}
	// the sun: below 0.05 degrees or overhead, no terrain shadows (fail open)
	const double hz = std::sqrt( toSun[0] * toSun[0] + toSun[1] * toSun[1] );
	if ( toSun[2] <= std::sin( 0.05 * kDeg ) || hz < 1e-4 ) {
		t.valid = false;
		t.last = QStringLiteral( "on(sun below the horizon or overhead: lit)" );
		return;
	}
	// steps 1 and 2 run again right after a synchronous job, so a pinned harness publishes in one frame
	auto reap = [&]() {
		// 1. a finished job becomes an upload
		if ( j.busy && ( s.sync || j.fut.wait_for( std::chrono::seconds( 0 ) ) == std::future_status::ready ) ) {
			j.fut.get();
			j.busy = false;
			if ( j.w.line >= j.w.N && j.w.N == t.N ) {
				t.uploading = true;
				t.upRow = 0;
				t.upMs = 0;
			}
			t.shapes = j.w.shapes;
			t.tris = j.w.tris;
			t.zmin = j.w.zmin;
			t.zmax = j.w.zmax;
			t.rasterMs = j.w.rasterMs;
			t.sweepMs = j.w.sweepMs;
			t.elev = j.w.elev;
			t.az = j.w.az;
		}
		// 2. the upload, a band a frame (the whole map in one frame when synchronous)
		if ( t.uploading ) {
			const int rows = s.sync ? t.N : std::max( 1, kUploadBytes / ( t.N * 4 ) );
			const int r1 = std::min( t.N, t.upRow + rows );
			QElapsedTimer ut;
			ut.start();
			timerBegin( fn, s.tmTerrain );
			uploadRows( fn, t, j.w.out, t.upRow, r1 );
			timerEnd( fn, s.tmTerrain );
			t.upMs += double( ut.nsecsElapsed() ) * 1e-6;
			t.upRow = r1;
			if ( t.upRow >= t.N ) {
				// publish: the back texture is complete
				const Terrain & w = j.w;
				t.uploading = false;
				t.front = 1 - t.front;
				t.valid = true;
				t.sweeps++;
				t.pubN = w.N;
				t.pubX0 = w.x0;
				t.pubY0 = w.y0;
				t.pubT = w.T;
				t.pubZmin = w.swLo;
				t.pubSpan = w.swSpan;
				t.pubElev = w.elev;
				t.pubAz = w.az;
				t.pubTanLo = w.tanLo;
				t.pubTanHi = w.tanHi;
				t.pubDir[0] = w.dir[0];
				t.pubDir[1] = w.dir[1];
				if ( !s.dump.isEmpty() ) {
					t.pubOut = w.out;
					dumpTerrain( t, w.H, s.dump );
				}
			}
		}
	};
	reap();
	// 3. a new job: the sun moved past the threshold, or the set or the grid changed; one at a time, never mid-upload
	const double moved = std::acos( std::clamp( dot3( toSun, t.latched ), -1.0, 1.0 ) ) / kDeg;
	static quint64 sweptGen = 0;	// lane TESTDRIVER1: a live set (the terrain reds) sweeps again
	const bool stale = sweptGen != wwSwitchGen() || !t.valid || t.pubN != t.N || t.pubX0 != t.x0 || t.pubY0 != t.y0 || t.needRaster
		|| moved > kRelatch;
	if ( !j.busy && !t.uploading && stale ) {
		sweptGen = wwSwitchGen();
		Terrain & w = j.w;
		w.N = t.N;
		w.T = t.T;
		w.x0 = t.x0;
		w.y0 = t.y0;
		j.raster = t.needRaster || w.H.size() != size_t( t.N ) * size_t( t.N );
		if ( j.raster ) {
			j.src = std::move( t.src );
			t.src.clear();
			t.needRaster = false;
		}
		for ( int k = 0; k < 3; k++ ) {
			j.toSun[k] = toSun[k];
			t.latched[k] = toSun[k];
		}
		j.flip = wwShadowRed( "terrflip" );
		j.nodecay = wwShadowRed( "terrnodecay" );
		j.stop = false;
		j.busy = true;
		if ( s.sync ) {
			std::promise<void> done;
			runTerrJob( &j );
			done.set_value();
			j.fut = done.get_future();
			reap();
		} else {
			j.fut = std::async( std::launch::async, runTerrJob, &j );
		}
	}
	t.frameMs = double( tm.nsecsElapsed() ) * 1e-6;
	if ( !s.sync && t.frames++ > 2 )
		t.frameMax = std::max( t.frameMax, t.frameMs );
	t.last = QStringLiteral( "on N=%1 texel=%2 x0=%3 y0=%4 z=%5..%6 shapes=%7 tris=%8 raster_ms=%9 elev=%10 az=%11 "
		"worker=%12 upload_row=%13/%14 sweep_cpu_ms=%15 frame_cpu_ms=%16 frame_cpu_max_ms=%17 upload_cpu_ms=%18 "
		"upload_gpu_ms=%19 sweeps=%20 valid=%21 mem_mb=%22 cpu_mb=%23" )
		.arg( t.N ).arg( t.T ).arg( t.x0, 0, 'f', 1 ).arg( t.y0, 0, 'f', 1 ).arg( double( t.zmin ), 0, 'f', 1 )
		.arg( double( t.zmax ), 0, 'f', 1 ).arg( t.shapes ).arg( t.tris ).arg( t.rasterMs, 0, 'f', 2 )
		.arg( t.elev, 0, 'f', 3 ).arg( t.az, 0, 'f', 3 ).arg( j.busy ? QStringLiteral( "busy" ) : QStringLiteral( "idle" ) )
		.arg( t.uploading ? t.upRow : t.N ).arg( t.N ).arg( t.sweepMs, 0, 'f', 2 ).arg( t.frameMs, 0, 'f', 3 )
		.arg( t.frameMax, 0, 'f', 3 ).arg( t.upMs, 0, 'f', 3 ).arg( s.tmTerrain.ms, 0, 'f', 4 ).arg( t.sweeps )
		.arg( t.valid ? 1 : 0 ).arg( double( t.N ) * t.N * 4.0 * 2.0 / ( 1024.0 * 1024.0 ), 0, 'f', 1 )
		.arg( double( j.w.H.size() * 4 + j.w.out.size() * 2 + t.pubOut.size() * 2 ) / ( 1024.0 * 1024.0 ), 0, 'f', 1 );
}

// ---- (B) contact ----

struct ContactPreset
{
	int samples;
	int scale;	// 1 full, 2 half
};

ContactPreset contactPreset( int tier )
{
	switch ( tier ) {
	case 1: return { 32, 2 };
	case 2: return { 60, 2 };
	case 3: return { 60, 1 };
	default: return { 96, 1 };
	}
}

void echoSummary()
{
	SetState & s = ss();
	if ( s.echo.isEmpty() )
		return;
	static QString lastWritten;
	const QString line = wwShadowSetSummary() + QStringLiteral( "\n" );
	if ( line == lastWritten )
		return;
	QFile f( s.echo );
	if ( f.open( QIODevice::WriteOnly | QIODevice::Truncate ) ) {
		f.write( line.toUtf8() );
		lastWritten = line;
	}
}

} // namespace

bool wwShadowRed( const char * name )
{
	return ss().reds.contains( QString::fromLatin1( name ) );
}

int wwShadowCascadeTier() { return ss().cascade; }
int wwShadowContactTier() { return ss().contact; }
int wwShadowTerrainTier() { return ss().terrain; }
int wwSunPathMode() { return ss().sunPath; }

void wwShadowSetCascadeTier( int tier )
{
	SetState & s = ss();
	setTier( s.cascade, tier, s.pinCascade, kCascadeKey );
}

void wwShadowSetContactTier( int tier )
{
	SetState & s = ss();
	setTier( s.contact, tier, s.pinContact, kContactKey );
}

void wwShadowSetTerrainTier( int tier )
{
	SetState & s = ss();
	setTier( s.terrain, tier, s.pinTerrain, kTerrainKey );
}

void wwSunPathSetMode( int mode )
{
	SetState & s = ss();
	setTier( s.sunPath, std::clamp( mode, 0, 1 ), s.pinSunPath, kSunPathKey );
}

// ---- (D) the sun ----

double wwSunPathReal( double hour, double gameDay, double out[3] )
{
	const SetState & s = ss();
	if ( s.sunPin ) {
		const double el = s.pinElev * kDeg, az = s.pinAz * kDeg;
		out[0] = std::sin( az ) * std::cos( el );
		out[1] = std::cos( az ) * std::cos( el );
		out[2] = std::sin( el );
		return s.pinElev;
	}
	// the game starts on 23 October (day 296 of the year); solar time = the game hour
	const double n = std::fmod( 296.0 + std::floor( std::max( gameDay, 0.0 ) ), 365.0 ) + 1.0;
	const double decl = 23.44 * kDeg * std::sin( 2.0 * kPi * ( 284.0 + n ) / 365.0 );
	const double H = ( hour - 12.0 ) * 15.0 * kDeg;
	const double phi = kLatitude * kDeg;
	out[0] = -std::cos( decl ) * std::sin( H );
	out[1] = std::cos( phi ) * std::sin( decl ) - std::sin( phi ) * std::cos( decl ) * std::cos( H );
	out[2] = std::sin( phi ) * std::sin( decl ) + std::cos( phi ) * std::cos( decl ) * std::cos( H );
	norm3( out );
	return std::asin( std::clamp( out[2], -1.0, 1.0 ) ) / kDeg;
}

bool wwSunPathApply( double hour, double gameDay, float sunDir[3], float disc[3] )
{
	SetState & s = ss();
	{
		const double l = std::sqrt( double( sunDir[0] ) * sunDir[0] + double( sunDir[1] ) * sunDir[1] + double( sunDir[2] ) * sunDir[2] );
		s.lastElevEngine = l > 1e-9 ? std::asin( std::clamp( sunDir[2] / l, -1.0, 1.0 ) ) / kDeg : 0.0;
		const double d = std::sqrt( double( disc[0] ) * disc[0] + double( disc[1] ) * disc[1] + double( disc[2] ) * disc[2] );
		s.lastDiscEngine = d > 1e-9 ? std::asin( std::clamp( disc[2] / d, -1.0, 1.0 ) ) / kDeg : 0.0;
	}
	if ( s.sunPath != 1 ) {
		double r[3];
		const double e = wwSunPathReal( hour, gameDay, r );
		s.sunLast = QStringLiteral( "engine elev=%1 disc_elev=%2 (real would be %3)" ).arg( s.lastElevEngine, 0, 'f', 3 )
			.arg( s.lastDiscEngine, 0, 'f', 3 ).arg( e, 0, 'f', 3 );
		return false;
	}
	double r[3];
	const double e = wwSunPathReal( hour, gameDay, r );
	s.lastElevReal = e;
	double night[3] = { sunDir[0], sunDir[1], sunDir[2] };
	norm3( night );
	double o[3];
	if ( e >= 0.0 ) {
		std::copy( r, r + 3, o );
	} else {
		// SunSync's rule: one light; under the horizon it crossfades to the night light by -6 degrees
		double rh[3] = { r[0], r[1], 0.0 };
		if ( !norm3( rh ) )
			rh[2] = 1.0;
		const double w0 = std::clamp( -e / 6.0, 0.0, 1.0 ), w = w0 * w0 * ( 3.0 - 2.0 * w0 );
		const double c = std::clamp( dot3( rh, night ), -1.0, 1.0 ), om = std::acos( c );
		if ( om < 1e-4 || om > kPi - 1e-3 ) {
			for ( int k = 0; k < 3; k++ )
				o[k] = rh[k] * ( 1.0 - w ) + night[k] * w;
			if ( !norm3( o ) )
				std::copy( night, night + 3, o );
		} else {
			const double a = std::sin( ( 1.0 - w ) * om ) / std::sin( om ), b = std::sin( w * om ) / std::sin( om );
			for ( int k = 0; k < 3; k++ )
				o[k] = rh[k] * a + night[k] * b;
			norm3( o );
		}
	}
	const double dl = std::sqrt( double( disc[0] ) * disc[0] + double( disc[1] ) * disc[1] + double( disc[2] ) * disc[2] );
	if ( e >= 0.0 )
		for ( int k = 0; k < 3; k++ )
			disc[k] = float( r[k] * ( dl > 1e-6 ? dl : 400.0 ) );
	for ( int k = 0; k < 3; k++ )
		sunDir[k] = float( o[k] );
	s.lastElevShaded = std::asin( std::clamp( o[2], -1.0, 1.0 ) ) / kDeg;
	s.sunLast = QStringLiteral( "real elev=%1 shaded_elev=%2 az=%3 engine_elev=%4%5" ).arg( e, 0, 'f', 3 )
		.arg( s.lastElevShaded, 0, 'f', 3 ).arg( std::atan2( r[0], r[1] ) / kDeg, 0, 'f', 3 ).arg( s.lastElevEngine, 0, 'f', 3 )
		.arg( s.sunPin ? QStringLiteral( " (pinned)" ) : QString() );
	return true;
}

// ---- (A) ----

bool wwShadowCascadePreset( WwCascadePreset & p )
{
	switch ( ss().cascade ) {
	case 1: p.D = 4000; p.split0 = 900; p.split1 = 2600; p.blend = 150; p.map = 2048; p.radius = 2.0f; break;
	case 2: p.D = 6000; p.split0 = 1100; p.split1 = 3400; p.blend = 200; p.map = 2048; p.radius = 2.5f; break;
	case 3: p.D = 10000; p.split0 = 1500; p.split1 = 4800; p.blend = 300; p.map = 4096; p.radius = 3.0f; break;
	case 4: p.D = 16000; p.split0 = 2000; p.split1 = 6500; p.blend = 400; p.map = 4096; p.radius = 3.5f; break;
	default: return false;
	}
	// a low sun reaches far: the light's origin sits behind the camera by twice the reach (never under CSM1's)
	p.lightBack = std::max( 15000.0, 2.0 * p.D );
	p.taps = std::clamp( int( std::lround( 32.0 * ( p.radius / 3.0 ) * ( p.radius / 3.0 ) ) ), 8, 48 );
	p.biasTexels = 1.5f;
	p.slopeBias = 6.0f;
	p.unitBias = 12.0f;
	return true;
}

// ---- the frame ----

void wwShadowSetFrame( Scene * scene )
{
	if ( !scene || !scene->renderer || scene->selecting )
		return;
	SetState & s = ss();
	float sunDir[3], disc[3];
	wwLookdevShadowLight( sunDir, disc );
	double to[3] = { sunDir[0], sunDir[1], sunDir[2] };
	norm3( to );
	terrainFrame( scene, to );
	echoSummary();
}

bool wwContactShadowWantsTargets( Scene * scene )
{
	return scene && ss().contact > 0 && wwLookdevActive() && !scene->selecting;
}

void wwContactShadowPass( Scene * scene )
{
	SetState & s = ss();
	Contact & c = s.c;
	c.valid = false;
	if ( !scene || !scene->renderer || scene->selecting )
		return;
	if ( !s.contact || !wwLookdevActive() ) {
		c.last = QStringLiteral( "off" );
		return;
	}
	Renderer * r = scene->renderer;
	auto fn = r->fn;
	if ( !unitsOk( fn ) ) {
		c.last = QStringLiteral( "refused(fewer than 26 texture units)" );
		return;
	}
	WwCellAoTargets g;
	if ( !wwCellAoTargets( scene, g ) || !g.gbuf ) {
		c.last = QStringLiteral( "on(no opaque pass: the cell view only)" );
		return;
	}
	const auto & pm = r->globalUniforms->projectionMatrix;
	if ( pm[3][3] == 1.0f ) {
		c.last = QStringLiteral( "refused(orthographic view)" );
		return;
	}
	const ContactPreset p = contactPreset( s.contact );
	const void * ctx = QOpenGLContext::currentContext();
	if ( ctx != c.ctx ) {
		c.ctx = ctx;
		c.tex = c.fbo = 0;
		c.w = c.h = 0;
	}
	const int W = g.w, H = g.h, mw = ( W + p.scale - 1 ) / p.scale, mh = ( H + p.scale - 1 ) / p.scale;
	if ( !c.tex ) {
		fn->glGenTextures( 1, &c.tex );
		fn->glGenFramebuffers( 1, &c.fbo );
	}
	GLint prevTex = 0, prevFbo = 0, prevRead = 0, vp[4], prevActive = 0;
	fn->glGetIntegerv( GL_TEXTURE_BINDING_2D, &prevTex );
	fn->glGetIntegerv( GL_DRAW_FRAMEBUFFER_BINDING, &prevFbo );
	fn->glGetIntegerv( GL_READ_FRAMEBUFFER_BINDING, &prevRead );
	fn->glGetIntegerv( GL_VIEWPORT, vp );
	fn->glGetIntegerv( GL_ACTIVE_TEXTURE, &prevActive );
	if ( c.w != mw || c.h != mh ) {
		fn->glBindTexture( GL_TEXTURE_2D, c.tex );
		fn->glTexImage2D( GL_TEXTURE_2D, 0, GL_R8, mw, mh, 0, GL_RED, GL_UNSIGNED_BYTE, nullptr );
		fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
		fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
		fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
		fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
		fn->glBindTexture( GL_TEXTURE_2D, GLuint( prevTex ) );
		fn->glBindFramebuffer( GL_FRAMEBUFFER, c.fbo );
		fn->glFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, c.tex, 0 );
		c.w = mw;
		c.h = mh;
	}
	fn->glBindFramebuffer( GL_FRAMEBUFFER, c.fbo );
	const bool complete = fn->glCheckFramebufferStatus( GL_FRAMEBUFFER ) == GL_FRAMEBUFFER_COMPLETE;
	NifSkopeOpenGLContext::Program * prog = complete ? r->useProgram( "ww_contact.prog" ) : nullptr;
	if ( !prog ) {
		fn->glBindFramebuffer( GL_DRAW_FRAMEBUFFER, GLuint( prevFbo ) );
		fn->glBindFramebuffer( GL_READ_FRAMEBUFFER, GLuint( prevRead ) );
		c.last = complete ? QStringLiteral( "refused(ww_contact.prog did not link)" ) : QStringLiteral( "refused(target incomplete)" );
		return;
	}
	timerBegin( fn, s.tmContact );
	const bool wasDepth = fn->glIsEnabled( GL_DEPTH_TEST ), wasBlend = fn->glIsEnabled( GL_BLEND );
	const bool wasCull = fn->glIsEnabled( GL_CULL_FACE ), wasScissor = fn->glIsEnabled( GL_SCISSOR_TEST );
	GLboolean depthMask = GL_TRUE, colorMask[4];
	fn->glGetBooleanv( GL_DEPTH_WRITEMASK, &depthMask );
	fn->glGetBooleanv( GL_COLOR_WRITEMASK, colorMask );
	fn->glDisable( GL_DEPTH_TEST );
	fn->glDisable( GL_BLEND );
	fn->glDisable( GL_CULL_FACE );
	fn->glDisable( GL_SCISSOR_TEST );
	fn->glDepthMask( GL_FALSE );
	fn->glColorMask( GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE );
	fn->glViewport( 0, 0, mw, mh );

	// the light in view space, then where it projects (Bend's light coordinate)
	float sunDir[3], disc[3];
	wwLookdevShadowLight( sunDir, disc );
	const Transform & vt = scene->view;
	double lv[3];
	for ( int i = 0; i < 3; i++ )
		lv[i] = vt.rotation( i, 0 ) * sunDir[0] + vt.rotation( i, 1 ) * sunDir[1] + vt.rotation( i, 2 ) * sunDir[2];
	norm3( lv );
	if ( wwShadowRed( "contactflip" ) )
		for ( double & v : lv )
			v = -v;
	const double cx = pm[0][0] * lv[0] + pm[1][0] * lv[1] + pm[2][0] * lv[2];
	const double cy = pm[0][1] * lv[0] + pm[1][1] * lv[1] + pm[2][1] * lv[2];
	const double cz = pm[0][2] * lv[0] + pm[1][2] * lv[1] + pm[2][2] * lv[2];
	double cw = pm[0][3] * lv[0] + pm[1][3] * lv[1] + pm[2][3] * lv[2];
	if ( std::fabs( cw ) < 1e-6 )
		cw = cw < 0 ? -1e-6 : 1e-6;
	// mask pixels, bottom-left origin (the g-buffer's own row order)
	const double lx = ( cx / cw * 0.5 + 0.5 ) * mw, ly = ( cy / cw * 0.5 + 0.5 ) * mh, lz = cz / cw * 0.5 + 0.5;

	fn->glActiveTexture( GL_TEXTURE0 );
	fn->glBindTexture( GL_TEXTURE_2D, g.gbuf );
	prog->uni1i( "gbuf", 0 );
	fn->glUniform2i( prog->uniLocation( "fullSize" ), W, H );
	prog->uni1f( "maskScale", float( p.scale ) );
	prog->uni4f( "lightCoord", FloatVector4( float( lx ), float( ly ), float( lz ), cw > 0 ? 1.0f : -1.0f ) );
	// the thickness window (2 % of the depth, 1.5 units at least), Bend's bilinear threshold 0.02 and contrast 4
	prog->uni4f( "sssParams", FloatVector4( 0.02f, 0.02f, 4.0f, float( p.samples ) ) );
	prog->uni1f( "sssFloor", 1.5f );
	static const float quad[12] = { -1, -1, 0, 1, -1, 0, -1, 1, 0, 1, 1, 0 };
	static const std::uint16_t idx[6] = { 0, 1, 2, 2, 1, 3 };
	const float * attrs = quad;
	r->drawShape( 4, 3, 6, GL_TRIANGLES, GL_UNSIGNED_SHORT, &attrs, idx );
	r->stopProgram();
	fn->glBindTexture( GL_TEXTURE_2D, 0 );
	timerEnd( fn, s.tmContact );

	fn->glActiveTexture( GLenum( prevActive ) );
	fn->glBindFramebuffer( GL_DRAW_FRAMEBUFFER, GLuint( prevFbo ) );
	fn->glBindFramebuffer( GL_READ_FRAMEBUFFER, GLuint( prevRead ) );
	fn->glViewport( vp[0], vp[1], vp[2], vp[3] );
	fn->glDepthMask( depthMask );
	fn->glColorMask( colorMask[0], colorMask[1], colorMask[2], colorMask[3] );
	wasDepth ? fn->glEnable( GL_DEPTH_TEST ) : fn->glDisable( GL_DEPTH_TEST );
	wasBlend ? fn->glEnable( GL_BLEND ) : fn->glDisable( GL_BLEND );
	wasCull ? fn->glEnable( GL_CULL_FACE ) : fn->glDisable( GL_CULL_FACE );
	wasScissor ? fn->glEnable( GL_SCISSOR_TEST ) : fn->glDisable( GL_SCISSOR_TEST );

	c.valid = true;
	c.doc = scene->nifModel;
	c.fullW = W;
	c.fullH = H;
	c.last = QStringLiteral( "on mask=%1x%2 full=%3x%4 samples=%5 light=%6,%7,%8,%9 gpu_ms=%10 mem_mb=%11" ).arg( mw ).arg( mh )
		.arg( W ).arg( H ).arg( p.samples ).arg( lx, 0, 'f', 1 ).arg( ly, 0, 'f', 1 ).arg( lz, 0, 'f', 6 ).arg( cw > 0 ? 1 : -1 )
		.arg( s.tmContact.ms, 0, 'f', 4 ).arg( double( mw ) * mh / ( 1024.0 * 1024.0 ), 0, 'f', 2 );
}

bool wwShadowSetActive( Scene * scene )
{
	const SetState & s = ss();
	if ( !scene )
		return false;
	return ( s.t.valid && s.terrain > 0 ) || ( s.c.valid && s.c.doc == scene->nifModel );
}

void wwShadowSetUniforms( Scene * scene, NifSkopeOpenGLContext::Program * prog, bool csmValid, const float csmTexel[3] )
{
	if ( !scene || !scene->renderer || !prog || prog->uniLocation( "ussOn" ) < 0 )
		return;
	SetState & s = ss();
	Renderer * r = scene->renderer;
	auto fn = r->fn;
	// the volumetric fog's froxels and the effect shaders' (blended) surfaces: no screen-space term
	const bool volume = prog->uniLocation( "volStage" ) >= 0;
	const bool noScreen = volume || prog->uniLocation( "GreyscaleMap" ) >= 0 || prog->uniLocation( "hasGreyscaleMap" ) >= 0;
	const bool terr = s.terrain > 0 && s.t.valid && s.t.tex[s.t.front];
	const bool contact = !noScreen && s.c.valid && s.c.doc == scene->nifModel && s.c.tex;
	const bool upgraded = s.cascade > 0 && csmValid;
	// a frame with the cascades invalid (the Shadows row off) runs the composition so CSM1's path never reads them
	const bool on = upgraded || terr || contact || !csmValid;
	prog->uni1i( "ussOn", on ? 1 : 0 );
	const bool units = unitsOk( fn );
	if ( units ) {
		GLint prevActive = 0;
		fn->glGetIntegerv( GL_ACTIVE_TEXTURE, &prevActive );
		fn->glActiveTexture( GLenum( GL_TEXTURE0 + WwCellUnit::TerrShadow ) );
		fn->glBindTexture( GL_TEXTURE_2D, terr ? s.t.tex[s.t.front] : 0 );
		fn->glActiveTexture( GLenum( GL_TEXTURE0 + WwCellUnit::ContactShadow ) );
		fn->glBindTexture( GL_TEXTURE_2D, contact ? s.c.tex : 0 );
		fn->glActiveTexture( GLenum( prevActive ) );
		prog->uni1i( "ussTerrMap", WwCellUnit::TerrShadow );
		prog->uni1i( "ussContactMap", WwCellUnit::ContactShadow );
	}
	if ( !on )
		return;
	prog->uni1i( "ussCsmOn", csmValid ? 1 : 0 );
	WwCascadePreset cp;
	const bool vogel = upgraded && wwShadowCascadePreset( cp ) && !wwShadowRed( "novogel" );
	prog->uni4f( "ussCsmFilter", FloatVector4( vogel ? 1.0f : 0.0f, cp.radius, float( cp.taps ), ( vogel && !volume ) ? 1.0f : 0.0f ) );
	prog->uni3f( "ussCsmBias", cp.biasTexels * csmTexel[0], cp.biasTexels * csmTexel[1], cp.biasTexels * csmTexel[2] );
	prog->uni1i( "ussTerrOn", terr ? 1 : 0 );
	if ( terr ) {
		const Terrain & t = s.t;
		const double span = t.pubN * t.pubT;
		prog->uni4f( "ussTerrXform", FloatVector4( float( 1.0 / span ), float( -t.pubX0 / span ), float( 1.0 / span ), float( -t.pubY0 / span ) ) );
		// zmin, span, the receiver's bias (world units up), the edge fade (uv)
		prog->uni4f( "ussTerrRange", FloatVector4( t.pubZmin, t.pubSpan, float( 0.05 * t.pubT + 4.0 ), 0.02f ) );
		// world from view: Q = R^T (p - t) / sc
		const Transform & vt = scene->view;
		const float sc = vt.scale != 0.0f ? vt.scale : 1.0f;
		float m[16];
		for ( int i = 0; i < 3; i++ ) {
			float k = 0.0f;
			for ( int j = 0; j < 3; j++ ) {
				m[j * 4 + i] = vt.rotation( j, i ) / sc;	// column j, row i: R^T(i, j) = R(j, i)
				k -= vt.rotation( j, i ) * vt.translation[j];
			}
			m[12 + i] = k / sc;
		}
		m[3] = m[7] = m[11] = 0.0f;
		m[15] = 1.0f;
		const int l = prog->uniLocation( "ussWorldFromView" );
		if ( l >= 0 )
			fn->glUniformMatrix4fv( l, 1, GL_FALSE, m );
	}
	prog->uni1i( "ussContactOn", contact ? 1 : 0 );
	if ( contact )
		prog->uni2f( "ussContactInvSize", 1.0f / float( std::max( s.c.fullW, 1 ) ), 1.0f / float( std::max( s.c.fullH, 1 ) ) );
	prog->uni1i( "ussView", s.view );
	prog->uni1i( "ussRed", ( wwShadowRed( "terrseam" ) ? 1 : 0 ) | ( wwShadowRed( "mulcompose" ) ? 2 : 0 ) );
}

bool wwTerrainShadowMap( WwTerrainShadowMap & out )
{
	const SetState & s = ss();
	const Terrain & t = s.t;
	if ( s.terrain <= 0 || !t.valid || !t.tex[t.front] )
		return false;
	out.tex = t.tex[t.front];
	out.n = t.pubN;
	out.x0 = t.pubX0;
	out.y0 = t.pubY0;
	out.texel = t.pubT;
	out.elevDeg = t.pubElev;
	out.azDeg = t.pubAz;
	out.zLo = t.pubZmin;
	out.zSpan = t.pubSpan;
	return true;
}

QString wwShadowSetSummary()
{
	const SetState & s = ss();
	WwCascadePreset cp;
	const bool up = wwShadowCascadePreset( cp );
	return QStringLiteral( "shadowset cascade=%1%2 contact=%3 [%4] terrain=%5 [%6] sunpath=%7 [%8] view=%9 red=%10" )
		.arg( s.cascade )
		.arg( up ? QStringLiteral( " (D=%1 split=%2,%3 blend=%4 map=%5 radius=%6 taps=%7 lightback=%8)" ).arg( cp.D ).arg( cp.split0 )
			.arg( cp.split1 ).arg( cp.blend ).arg( cp.map ).arg( double( cp.radius ) ).arg( cp.taps ).arg( cp.lightBack ) : QString() )
		.arg( s.contact ).arg( s.c.last ).arg( s.terrain ).arg( s.t.last ).arg( s.sunPath ).arg( s.sunLast ).arg( s.view )
		.arg( s.reds.isEmpty() ? QStringLiteral( "none" ) : s.reds.join( QChar( '+' ) ) );
}
