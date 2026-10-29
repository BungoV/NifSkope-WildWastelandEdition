/* Lane RTXGI1 M2 (2026-10-29): the ray-traced GI volume in the cell view (src/rtddgicell.h). */

#include "rtddgicell.h"

#include "harnesswindow.h"   // wwHarnessSettingsSuffix
#include "probeplace.h"      // ProbeSoup
#include "probesky.h"
#include "rtddgi.h"
#include "rtddgimath.h"
#include "version.h"
#include "wwswitch.h"
#include "gl/celllights.h"
#include "gl/cellunits.h"
#include "gl/glscene.h"
#include "gl/renderer.h"

#include <QDir>
#include <QElapsedTimer>
#include <QSettings>
#include <QStringList>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

namespace {

constexpr int kAtlasTexels = 193;           // a probe's texels in the texture buffer: 64 irradiance + 128 distance + 1 probe
constexpr double kViewReach = 3000.0;       // the debug view draws the probes within this of the camera (the PRTP view's)
constexpr size_t kViewMaxProbes = 20000;    // and at most this many spheres

//! WW_CELL_RTDDGI_SPEC (harness): the volume's knobs, "k=v" comma list; the text is part of the build key
struct CellSpec
{
	float spacing = 140.0f;   // GUIDE 4: a probe every 2 m to start; grown to fit `probes`
	float zscale = 1.0f;
	float reach = 2048.0f;    // the box: the probe GI's grid within this of the camera in x, y and z (one cell across)
	int probes = 4096;        // the cap (the readback is ~ probes x 3 KB a frame)
	int rays = 128;
	int updates = 4;          // a frame (budget: M3)
	int max = 0;              // stop after this many updates (0: never; pictures that must repeat set it)
	QString text;
};

CellSpec cellSpecNow()
{
	CellSpec s;
	s.text = wwEnvStr( "WW_CELL_RTDDGI_SPEC" ).trimmed();
	for ( const QString & kv : s.text.split( QLatin1Char( ',' ), Qt::SkipEmptyParts ) ) {
		const QStringList p = kv.split( QLatin1Char( '=' ) );
		if ( p.size() != 2 )
			continue;
		const QString k = p[0].trimmed();
		bool ok = false;
		const double v = p[1].trimmed().toDouble( &ok );
		if ( !ok )
			continue;
		if ( k == QLatin1String( "spacing" ) && v > 1.0 ) s.spacing = float( v );
		else if ( k == QLatin1String( "zscale" ) && v > 0.05 ) s.zscale = float( v );
		else if ( k == QLatin1String( "reach" ) && v > 100.0 ) s.reach = float( v );
		else if ( k == QLatin1String( "probes" ) && v >= 8.0 ) s.probes = int( std::min( v, 65536.0 ) );
		else if ( k == QLatin1String( "rays" ) && v >= 33.0 ) s.rays = int( std::min( v, 256.0 ) );
		else if ( k == QLatin1String( "updates" ) && v >= 1.0 ) s.updates = int( std::min( v, 16.0 ) );
		else if ( k == QLatin1String( "max" ) && v >= 0.0 ) s.max = int( v );
	}
	return s;
}

//! WW_CELL_RTDDGI_RED: the read's red bits (rtddgimath.h kSample*), live
int cellRedBits()
{
	WW_SWITCH_STATIC( int, bits, []() {
		int b = 0;
		for ( const QString & r : wwEnvStr( "WW_CELL_RTDDGI_RED" ).split( QLatin1Char( ',' ), Qt::SkipEmptyParts ) ) {
			const QString t = r.trimmed();
			if ( t == QLatin1String( "trilinear" ) ) b |= int( rtddgi::kSampleTrilinear );
			if ( t == QLatin1String( "nocheb" ) ) b |= int( rtddgi::kSampleNoCheb );
			if ( t == QLatin1String( "nobias" ) ) b |= int( rtddgi::kSampleNoBias );
		}
		return b;
	}() );
	return bits;
}

//! one volume being opened on a worker (the soup copy, the BLAS, relocation and classification take seconds)
struct Build
{
	std::mutex m;
	bool done = false, cancel = false, ok = false;
	RtDdgiLive live;
	QString why;
	double ms = 0, msCopy = 0;
	CellSpec spec;
	QString box;              // the census's words for the box
	int lights = 0;
	QString skyLabel;
};

struct Cell
{
	bool loaded = false, guiOn = false;
	int guiView = 0;
	const void * nif = nullptr;
	quint64 key = 0;
	std::shared_ptr<Build> pending, live;
	QString refused;
	int sets = 0, busy = 0;
	bool stopped = false, dumped = false, censusBuilt = false;
	// GL: one texture buffer (the context of the renderer that made it)
	const void * glOwner = nullptr;
	GLuint buf = 0, tex = 0;
	size_t bytes = 0;
	bool uploaded = false, attach = false, probesPacked = false;
	double uploadMs = 0;
	// the debug view's geometry, rebuilt each frame it draws
	std::vector<Vector3> vp;
	std::vector<FloatVector4> vc;
};

Cell & cell()
{
	static Cell * c = new Cell;   // never destroyed: a live volume must not close after the bake device's statics
	return *c;
}

//! the GUI's settings file (bakert.cpp's; a harness WW_SETTINGS_SCOPE starts empty, so the rows read off there)
QString rowSettingsApp()
{
	return QStringLiteral( "NifSkope " ) + NifSkopeVersion::rawToMajMin( NIFSKOPE_VERSION ) + wwHarnessSettingsSuffix();
}

void rowWrite( const QString & key, const QVariant & value )
{
	QSettings s( QStringLiteral( "NifTools" ), rowSettingsApp() );
	s.setValue( key, value );
}

void loadRows( Cell & c )
{
	if ( c.loaded )
		return;
	c.loaded = true;
	QSettings s( QStringLiteral( "NifTools" ), rowSettingsApp() );
	c.guiOn = s.value( QStringLiteral( "WW/CellRtDdgi" ), false ).toBool();
	c.guiView = std::clamp( s.value( QStringLiteral( "WW/CellRtDdgiView" ), 0 ).toInt(), 0, 2 );
}

void say( const QString & line )
{
	std::fprintf( stderr, "rtddgi cell: %s\n", qPrintable( line ) );
	std::fflush( stderr );
}

void refuse( Cell & c, const QString & why )
{
	if ( why == c.refused )
		return;
	c.refused = why;
	say( QStringLiteral( "refused(%1)" ).arg( why ) );
}

void dropLive( Cell & c )
{
	if ( c.pending ) {
		std::lock_guard<std::mutex> lk( c.pending->m );
		c.pending->cancel = true;
		if ( c.pending->done )
			c.pending->live.close();
	}
	c.pending.reset();
	c.live.reset();   // its RtDdgiLive closes the device's session
	c.sets = c.busy = 0;
	c.stopped = c.dumped = c.censusBuilt = false;
	c.uploaded = c.probesPacked = false;
}

void freeGl( Cell & c, Renderer * r )
{
	if ( r && c.glOwner == r && ( c.buf || c.tex ) ) {
		r->fn->glDeleteTextures( 1, &c.tex );
		r->fn->glDeleteBuffers( 1, &c.buf );
	}
	c.buf = c.tex = 0;   // made in another context: left to it (one view draws the cell)
	c.bytes = 0;
	c.glOwner = nullptr;
	c.uploaded = c.attach = c.probesPacked = false;
}

quint64 fnv( quint64 h, const void * p, size_t n )
{
	const unsigned char * b = static_cast<const unsigned char *>( p );
	for ( size_t i = 0; i < n; i++ )
		h = ( h ^ b[i] ) * 0x100000001B3ULL;
	return h;
}

//! the camera's world position (celllights.cpp's cellRow w: R^T (0 - t) / scale)
void eyeOf( const Transform & vt, float eye[3] )
{
	const float sc = vt.scale != 0.0f ? vt.scale : 1.0f;
	for ( int k = 0; k < 3; k++ ) {
		float w = 0.0f;
		for ( int j = 0; j < 3; j++ )
			w -= vt.rotation( j, k ) * vt.translation[j];
		eye[k] = w / sc;
	}
}

//! the volume's box: the probe GI's grid (its probes' part: the x/y pad voxels left out) within reach of the camera
bool boxFor( const WwCellGi * G, const ProbeSoup & soup, const float eye[3], const CellSpec & sp, float box[6], QString * words )
{
	float lo[3], hi[3];
	if ( G && G->dims[0] > 0 && G->dims[1] > 0 && G->dims[2] > 0 ) {
		for ( int k = 0; k < 3; k++ ) {
			const float pad = k < 2 ? float( G->pad ) * G->voxel : 0.0f;
			lo[k] = G->origin[k] + pad;
			hi[k] = G->origin[k] + float( G->dims[k] ) * G->voxel - pad;
		}
		*words = QStringLiteral( "the probe GI's grid" );
	} else {
		for ( int k = 0; k < 3; k++ ) {
			lo[k] = 3.4e38f;
			hi[k] = -3.4e38f;
		}
		for ( size_t i = 0; i < soup.tris.size(); i += 3 )
			for ( int k = 0; k < 3; k++ ) {
				lo[k] = std::min( lo[k], soup.tris[i + size_t( k )] );
				hi[k] = std::max( hi[k], soup.tris[i + size_t( k )] );
			}
		*words = QStringLiteral( "the soup (no probe GI grid)" );
	}
	for ( int k = 0; k < 3; k++ ) {
		if ( hi[k] <= lo[k] )
			return false;
		const float c = std::clamp( eye[k], lo[k], hi[k] );   // a camera outside the box: the part nearest it
		const float a = std::max( lo[k], c - sp.reach ), b = std::min( hi[k], c + sp.reach );
		const float minSpan = 2.0f * sp.spacing * ( k == 2 ? sp.zscale : 1.0f );   // three probe planes at least
		const float mid = 0.5f * ( a + b ), half = 0.5f * std::max( b - a, minSpan );
		box[k] = mid - half;
		box[3 + k] = mid + half;
	}
	*words += QStringLiteral( " within %1 of the camera (%2 %3 %4)" ).arg( double( sp.reach ), 0, 'f', 0 )
		.arg( double( eye[0] ), 0, 'f', 0 ).arg( double( eye[1] ), 0, 'f', 0 ).arg( double( eye[2] ), 0, 'f', 0 );
	return true;
}

QString statesOf( const RtDdgiVolume & v )
{
	return QStringLiteral( "probes %1: off %2, asleep %3, vigilant %4, moved %5" ).arg( v.probes ).arg( v.off ).arg( v.asleep )
		.arg( v.vigilant ).arg( v.moved );
}

qint64 atlasBytes( int nP )
{
	return qint64( nP ) * kAtlasTexels * 16;
}

QString msText( double ms )
{
	return ms < 0.0 ? QStringLiteral( "-" ) : QString::number( ms, 'f', 3 );
}

//! the tiles of the last step into the texture buffer (and the probes' texels once, after the first step)
bool upload( Cell & c, Renderer * r, const RtDdgiLive & L )
{
	auto fn = r->fn;
	const RtDdgiVolume & v = L.volume();
	const int nP = v.probeCount();
	const float * irr = L.irr(), * dist = L.dist();
	const quint32 * age = L.age();
	if ( !irr || !dist || !age || nP <= 0 )
		return false;
	if ( c.glOwner != r ) {
		freeGl( c, nullptr );
		c.glOwner = r;
	}
	if ( !c.buf ) {
		fn->glGenBuffers( 1, &c.buf );
		fn->glGenTextures( 1, &c.tex );
	}
	QElapsedTimer t;
	t.start();
	const size_t bytes = size_t( atlasBytes( nP ) );
	fn->glBindBuffer( GL_TEXTURE_BUFFER, c.buf );
	if ( c.bytes != bytes ) {
		fn->glBufferData( GL_TEXTURE_BUFFER, GLsizeiptr( bytes ), nullptr, GL_DYNAMIC_DRAW );
		c.bytes = bytes;
		c.attach = true;
		c.probesPacked = false;
	}
	fn->glBufferSubData( GL_TEXTURE_BUFFER, 0, GLsizeiptr( size_t( nP ) * 64 * 16 ), irr );
	fn->glBufferSubData( GL_TEXTURE_BUFFER, GLintptr( size_t( nP ) * 64 * 16 ), GLsizeiptr( size_t( nP ) * 128 * 16 ), dist );
	if ( !c.probesPacked ) {   // the states do not change after classification; usable needs one blend (age > 0)
		std::vector<float> pt( size_t( nP ) * 4 );
		for ( int p = 0; p < nP; p++ ) {
			for ( int k = 0; k < 3; k++ )
				pt[size_t( p ) * 4 + size_t( k )] = v.offset[size_t( p ) * 3 + size_t( k )];
			const int st = v.state[size_t( p )];
			pt[size_t( p ) * 4 + 3] = float( rtddgi::probeUsable( st, age[p] ) ? st : int( rtddgi::kOff ) );
		}
		fn->glBufferSubData( GL_TEXTURE_BUFFER, GLintptr( size_t( nP ) * 192 * 16 ), GLsizeiptr( pt.size() * 4 ), pt.data() );
		c.probesPacked = true;
	}
	fn->glBindBuffer( GL_TEXTURE_BUFFER, 0 );
	c.uploadMs = double( t.nsecsElapsed() ) / 1e6;
	c.uploaded = true;
	return true;
}

void startBuild( Cell & c, const ProbeSoup & soup, const WwCellLighting & lights, const ProbeSkyLight & sky, const WwCellGi * G,
	const float eye[3], const CellSpec & sp )
{
	auto b = std::make_shared<Build>();
	b->spec = sp;
	b->lights = int( lights.lights.size() );
	b->skyLabel = sky.on ? sky.label : QStringLiteral( "no sky (interior)" );
	RtDdgiSpec spec;
	spec.spacing = sp.spacing;
	spec.zScale = sp.zscale;
	spec.rays = sp.rays;
	spec.maxProbes = sp.probes;
	spec.fitProbes = true;
	spec.place = false;
	spec.gpu = 1;
	if ( !boxFor( G, soup, eye, sp, spec.box, &b->box ) ) {
		std::lock_guard<std::mutex> lk( b->m );
		b->done = true;
		b->why = QStringLiteral( "no box: the grid or the soup is empty" );
		c.pending = b;
		return;
	}
	QElapsedTimer t;
	t.start();
	auto soupCopy = std::make_shared<ProbeSoup>( soup );   // the kept soup may be replaced while the worker reads
	b->msCopy = double( t.nsecsElapsed() ) / 1e6;
	c.pending = b;
	std::thread( [b, soupCopy, lights, sky, spec]() {
		QElapsedTimer tm;
		tm.start();
		const bool ok = b->live.open( *soupCopy, lights, sky, spec );
		std::lock_guard<std::mutex> lk( b->m );
		b->ok = ok;
		b->why = b->live.why();
		b->ms = double( tm.nsecsElapsed() ) / 1e6;
		b->done = true;
		if ( b->cancel )
			b->live.close();
	} ).detach();
}

void censusBuilt( const Build & b )
{
	const RtDdgiVolume & v = b.live.volume();
	const RtDdgiSpec & s = b.live.spec();
	const int nP = v.probeCount();
	const bool grown = std::fabs( v.step[0] - s.spacing ) > 1e-3f;
	say( QStringLiteral( "built %1 in %2 ms (soup copy %3 ms); grid %4 x %5 x %6, spacing %7 x %8 x %9%10; box %11; %12; "
		"relocate+classify %13 ms; BLAS grown %14 u; rays %15 a probe, %16 updates a frame%17; atlas bytes %18 (irr %19, dist %20, "
		"probes %21); lights %22; sky %23" )
		.arg( v.path ).arg( b.ms, 0, 'f', 1 ).arg( b.msCopy, 0, 'f', 1 ).arg( v.dims[0] ).arg( v.dims[1] ).arg( v.dims[2] )
		.arg( double( v.step[0] ), 0, 'f', 1 ).arg( double( v.step[1] ), 0, 'f', 1 ).arg( double( v.step[2] ), 0, 'f', 1 )
		.arg( grown ? QStringLiteral( " (asked %1, grown to fit the cap %2)" ).arg( double( s.spacing ), 0, 'f', 1 ).arg( s.maxProbes ) : QString() )
		.arg( b.box ).arg( statesOf( v ) ).arg( v.msReloc, 0, 'f', 1 ).arg( v.blasGrow, 0, 'f', 4 ).arg( s.rays ).arg( b.spec.updates )
		.arg( b.spec.max > 0 ? QStringLiteral( " (stop at %1)" ).arg( b.spec.max ) : QString() )
		.arg( atlasBytes( nP ) ).arg( qint64( nP ) * 64 * 16 ).arg( qint64( nP ) * 128 * 16 ).arg( qint64( nP ) * 16 )
		.arg( b.lights ).arg( b.skyLabel ) );
}

void censusSet( const Cell & c, const RtDdgiLive & L, int k, const QString & tail )
{
	const double * ms = L.lastMs();
	const double rb = ms[3] < 0.0 ? -1.0 : ms[3] + c.uploadMs;
	say( QStringLiteral( "set %1, updates %2 (+%3); gpu ms trace %4 blend %5 border %6 (%3 updates); readback ms %7 (device copy %8 + "
		"GL upload %9; interop owed when over 0.2); wall %10 ms; rays %11; %12; atlas bytes %13; busy frames %14%15" )
		.arg( c.sets ).arg( L.updates() ).arg( k ).arg( msText( ms[0] ), msText( ms[1] ), msText( ms[2] ) ).arg( msText( rb ) )
		.arg( msText( ms[3] ) ).arg( c.uploadMs, 0, 'f', 3 ).arg( L.lastWallMs(), 0, 'f', 3 ).arg( L.lastRays() )
		.arg( statesOf( L.volume() ) ).arg( atlasBytes( L.volume().probeCount() ) ).arg( c.busy ).arg( tail ) );
}

void dumpLive( Cell & c, const RtDdgiLive & L )
{
	const QString dir = QString::fromLocal8Bit( wwEnv( "WW_CELL_RTDDGI_DUMP" ) ).trimmed();
	if ( dir.isEmpty() || c.dumped )
		return;
	c.dumped = true;
	RtDdgiVolume v;
	L.snapshot( &v );
	QString err;
	if ( rtddgiDump( v, L.spec(), dir, &err ) )
		say( QStringLiteral( "dumped %1 updates to %2" ).arg( L.updates() ).arg( QDir::toNativeSeparators( dir ) ) );
	else
		say( QStringLiteral( "dump refused(%1)" ).arg( err ) );
}

// a unit sphere for the debug view: an octahedron split once (32 triangles), vertices on the sphere
const std::vector<Vector3> & sphereTris()
{
	static std::vector<Vector3> t;
	if ( !t.empty() )
		return t;
	const Vector3 ax[6] = { Vector3( 1, 0, 0 ), Vector3( -1, 0, 0 ), Vector3( 0, 1, 0 ), Vector3( 0, -1, 0 ), Vector3( 0, 0, 1 ), Vector3( 0, 0, -1 ) };
	auto unit = []( const Vector3 & v ) {
		const float l = std::sqrt( v[0] * v[0] + v[1] * v[1] + v[2] * v[2] );
		return Vector3( v[0] / l, v[1] / l, v[2] / l );
	};
	for ( int sx = 0; sx < 2; sx++ )
		for ( int sy = 2; sy < 4; sy++ )
			for ( int sz = 4; sz < 6; sz++ ) {
				const Vector3 a = ax[sx], b = ax[sy], cz = ax[sz];
				const Vector3 ab = unit( a + b ), bc = unit( b + cz ), ca = unit( cz + a );
				for ( const Vector3 & v : { a, ab, ca, ab, b, bc, ca, bc, cz, ab, bc, ca } )
					t.push_back( v );
			}
	return t;
}

} // namespace

bool rtddgiCellPinned()
{
	return wwEnvIsSet( "WW_CELL_RTDDGI" );
}

bool rtddgiCellOn()
{
	if ( wwEnvIsSet( "WW_CELL_RTDDGI" ) ) {
		const QByteArray v = wwEnv( "WW_CELL_RTDDGI" ).trimmed();
		return !v.isEmpty() && v != "0";
	}
	Cell & c = cell();
	loadRows( c );
	return c.guiOn;
}

void rtddgiCellSetOn( bool on )
{
	Cell & c = cell();
	loadRows( c );
	c.guiOn = on;
	rowWrite( QStringLiteral( "WW/CellRtDdgi" ), on );
}

bool rtddgiCellShaderDefine()
{
	return rtddgiCellOn() && wwCellGiVis() == 0;
}

bool rtddgiCellViewPinned()
{
	return wwEnvIsSet( "WW_CELL_RTDDGI_VIEW" );
}

int rtddgiCellView()
{
	if ( wwEnvIsSet( "WW_CELL_RTDDGI_VIEW" ) ) {
		const QByteArray v = wwEnv( "WW_CELL_RTDDGI_VIEW" ).trimmed().toLower();
		return v == "states" || v == "1" ? 1 : v == "irradiance" || v == "2" ? 2 : 0;
	}
	Cell & c = cell();
	loadRows( c );
	return c.guiView;
}

void rtddgiCellSetView( int mode )
{
	Cell & c = cell();
	loadRows( c );
	c.guiView = std::clamp( mode, 0, 2 );
	rowWrite( QStringLiteral( "WW/CellRtDdgiView" ), c.guiView );
}

void rtddgiCellFrame( Scene * scene )
{
	if ( !scene || !scene->nifModel || !scene->renderer )
		return;
	Cell & c = cell();
	const void * nif = scene->nifModel;
	Renderer * r = scene->renderer;
	if ( !rtddgiCellShaderDefine() ) {   // off (or the march red): nothing kept, nothing bound
		if ( c.pending || c.live || c.buf ) {
			dropLive( c );
			freeGl( c, r );
			c.key = 0;
			c.nif = nullptr;
			say( QStringLiteral( "freed (the switch is off)" ) );
		}
		c.refused.clear();
		return;
	}
	ProbeSkyLight sky;
	WwCellLighting lights;
	const ProbeSoup * soup = probeSkyKeptSoup( nif, &sky, &lights );
	if ( !soup ) {
		if ( c.nif == nif && ( c.pending || c.live ) ) {
			dropLive( c );
			c.key = 0;
		}
		refuse( c, QStringLiteral( "no relit cell in this view: the volume traces the soup the Precomputed Radiance Transfer relight keeps" ) );
		return;
	}
	const WwCellGi * G = wwCellGiFor( nif );
	const CellSpec sp = cellSpecNow();
	// what the volume is built from (the camera only at the build: the box does not follow it, M3 scrolls)
	quint64 key = 0xCBF29CE484222325ULL;
	key = fnv( key, &nif, sizeof nif );
	key = fnv( key, &soup, sizeof soup );
	const size_t nTris = soup->tris.size();
	key = fnv( key, &nTris, sizeof nTris );
	if ( nTris >= 9 ) {
		key = fnv( key, soup->tris.data(), 36 );
		key = fnv( key, soup->tris.data() + nTris - 9, 36 );
	}
	const int skyOn = sky.on ? 1 : 0;
	key = fnv( key, &skyOn, sizeof skyOn );
	key = fnv( key, sky.amb, sizeof sky.amb );
	key = fnv( key, sky.sun, sizeof sky.sun );
	key = fnv( key, sky.sunTo, sizeof sky.sunTo );
	const quint64 lk = rtddgiLightsKey( lights );
	key = fnv( key, &lk, sizeof lk );
	const QByteArray st = sp.text.toUtf8();
	key = fnv( key, st.constData(), size_t( st.size() ) );
	if ( G ) {
		key = fnv( key, G->origin, sizeof G->origin );
		key = fnv( key, G->dims, sizeof G->dims );
		key = fnv( key, &G->voxel, sizeof G->voxel );
		key = fnv( key, &G->pad, sizeof G->pad );
	}
	if ( key != c.key || c.nif != nif ) {
		const bool had = c.pending || c.live;
		dropLive( c );
		c.key = key;
		c.nif = nif;
		c.refused.clear();
		float eye[3];
		eyeOf( scene->view, eye );
		startBuild( c, *soup, lights, sky, G, eye, sp );
		say( QStringLiteral( "%1 (%2 triangles)" ).arg( had ? QStringLiteral( "rebuilding: the soup, lights, sky or spec moved" )
			: QStringLiteral( "building" ) ).arg( qint64( nTris / 9 ) ) );
	}
	if ( c.pending ) {
		std::shared_ptr<Build> b = c.pending;
		std::lock_guard<std::mutex> lk2( b->m );
		if ( b->done ) {
			c.pending.reset();
			if ( b->ok ) {
				c.live = b;
				censusBuilt( *b );
			} else {
				refuse( c, b->why );
			}
		}
	}
	if ( !c.live || c.stopped )
		return;
	RtDdgiLive & L = c.live->live;
	int k = c.live->spec.updates;
	if ( c.live->spec.max > 0 )
		k = std::min( k, c.live->spec.max - L.updates() );
	if ( k <= 0 ) {
		c.stopped = true;
		return;
	}
	if ( !L.step( k ) ) {
		if ( L.busy() ) {
			c.busy++;
			return;
		}
		refuse( c, QStringLiteral( "an update failed: %1" ).arg( L.why() ) );
		c.live.reset();
		c.uploaded = false;
		return;
	}
	c.sets++;
	if ( !upload( c, r, L ) ) {
		refuse( c, QStringLiteral( "no tiles came back" ) );
		return;
	}
	const bool atMax = c.live->spec.max > 0 && L.updates() >= c.live->spec.max;
	const bool pow2 = ( c.sets & ( c.sets - 1 ) ) == 0;
	if ( pow2 || atMax || c.sets % 256 == 0 )
		censusSet( c, L, k, atMax ? QStringLiteral( "; stopped at the spec's max" ) : QString() );
	if ( atMax ) {
		c.stopped = true;
		dumpLive( c, L );
	} else if ( c.live->spec.max <= 0 && c.sets == 64 ) {
		dumpLive( c, L );
	}
}

bool rtddgiCellPending()
{
	if ( !rtddgiCellShaderDefine() )
		return false;
	const Cell & c = cell();
	if ( c.pending )
		return true;   // the worker's build (a failed one is harvested by the next frame, then nothing is pending)
	return c.live && c.live->spec.max > 0 && !c.stopped;
}

void rtddgiCellUniforms( Scene * scene )
{
	Renderer * r = scene ? scene->renderer : nullptr;
	NifSkopeOpenGLContext::Program * prog = r ? r->getCurrentProgram() : nullptr;
	if ( !prog )
		return;
	const int locOn = prog->uniLocation( "cellDdgiOn" );
	if ( locOn < 0 )
		return;   // a program without the read: with the switch off, every program
	Cell & c = cell();
	const bool ready = c.live && c.uploaded && c.tex && c.glOwner == r && c.nif == scene->nifModel;
	auto fn = r->fn;
	GLint prevActive = 0;
	fn->glGetIntegerv( GL_ACTIVE_TEXTURE, &prevActive );
	fn->glActiveTexture( GLenum( GL_TEXTURE0 + WwCellUnit::DdgiAtlas ) );
	fn->glBindTexture( GL_TEXTURE_BUFFER, ready ? c.tex : 0 );
	if ( ready && c.attach ) {
		fn->glTexBuffer( GL_TEXTURE_BUFFER, GL_RGBA32F, c.buf );
		c.attach = false;
	}
	fn->glActiveTexture( GLenum( prevActive ) );
	prog->uni1i( "cellDdgiAtlas", WwCellUnit::DdgiAtlas );
	prog->uni1b_l( locOn, ready );
	if ( !ready )
		return;
	const RtDdgiVolume & v = c.live->live.volume();
	const float D = std::min( v.step[0], std::min( v.step[1], v.step[2] ) );
	prog->uni4f( "cellDdgiOrigin", FloatVector4( v.origin[0], v.origin[1], v.origin[2], D ) );
	prog->uni4f( "cellDdgiStep", FloatVector4( v.step[0], v.step[1], v.step[2], c.live->live.spec().bias ) );
	const int locDims = prog->uniLocation( "cellDdgiDims" );
	if ( locDims >= 0 )
		fn->glUniform4i( locDims, v.dims[0], v.dims[1], v.dims[2], v.probeCount() );
	prog->uni1i( "cellDdgiRed", cellRedBits() );
}

void rtddgiCellViewDraw( Scene * scene, const Transform & viewTrans )
{
	if ( !scene || !scene->renderer || scene->selecting || !rtddgiCellShaderDefine() )
		return;
	const int mode = rtddgiCellView();
	Cell & c = cell();
	if ( !mode || !c.live || c.nif != scene->nifModel )
		return;
	const RtDdgiLive & L = c.live->live;
	const RtDdgiVolume & v = L.volume();
	const float * irr = L.irr();
	const quint32 * age = L.age();
	if ( mode == 2 && ( !irr || !age ) )
		return;
	const std::vector<Vector3> & sph = sphereTris();
	const float rad = 0.12f * std::min( v.step[0], std::min( v.step[1], v.step[2] ) );
	c.vp.clear();
	c.vc.clear();
	size_t drawn = 0;
	const int nP = v.probeCount();
	for ( int p = 0; p < nP && drawn < kViewMaxProbes; p++ ) {
		float pos[3];
		v.position( p, pos );
		const Vector3 P( pos[0], pos[1], pos[2] );
		if ( double( ( viewTrans * P ).length() ) > kViewReach )   // view units, as the PRTP view's rings (cellvolumeview.cpp)
			continue;
		drawn++;
		const int st = v.state[size_t( p )];
		FloatVector4 flat( 0.2f, 0.9f, 0.3f, 1.0f );   // Vigilant green; Asleep blue; Off red
		if ( st == rtddgi::kAsleep )
			flat = FloatVector4( 0.35f, 0.45f, 0.95f, 1.0f );
		else if ( st == rtddgi::kOff )
			flat = FloatVector4( 0.95f, 0.15f, 0.15f, 1.0f );
		const bool usable = age && rtddgi::probeUsable( st, age[p] );
		for ( const Vector3 & n : sph ) {
			c.vp.push_back( Vector3( pos[0] + n[0] * rad, pos[1] + n[1] * rad, pos[2] + n[2] * rad ) );
			if ( mode == 1 ) {
				c.vc.push_back( flat );
				continue;
			}
			if ( !usable ) {   // the read never takes it
				c.vc.push_back( FloatVector4( 0.12f, 0.12f, 0.12f, 1.0f ) );
				continue;
			}
			// this probe alone at the sphere's normal, the read's decode: bilinear e, (e^2.5)^2 x 2 pi; shown E / pi as
			// the probe passes show it, pow(clamp, 1 / 2.2)
			float e[4];
			rtddgi::sampleTile( &irr[size_t( p ) * 256], rtddgi::kIrrTile, 4, rtddgi::V3( n[0], n[1], n[2] ), e );
			float rgb[3];
			for ( int k = 0; k < 3; k++ ) {
				const float q = std::pow( std::max( e[k], 0.0f ), 2.5f );
				rgb[k] = std::pow( std::clamp( q * q * 2.0f, 0.0f, 1.0f ), 1.0f / 2.2f );   // q^2 2 pi / pi
			}
			c.vc.push_back( FloatVector4( rgb[0], rgb[1], rgb[2], 1.0f ) );
		}
	}
	if ( c.vp.empty() )
		return;
	auto fn = scene->renderer->fn;
	scene->loadModelViewMatrix( viewTrans );
	fn->glEnable( GL_DEPTH_TEST );
	fn->glDepthFunc( GL_LEQUAL );
	fn->glDisable( GL_CULL_FACE );
	constexpr size_t kChunk = 65532;   // a multiple of 3
	for ( size_t i = 0; i < c.vp.size(); i += kChunk )
		scene->drawTriangles( c.vp.data() + i, std::min( kChunk, c.vp.size() - i ), c.vc.data() + i, true );
	fn->glDepthFunc( GL_LESS );
}
