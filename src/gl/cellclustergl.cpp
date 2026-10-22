/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#include "cellclustergl.h"
#include "wwswitch.h"	// lane TESTDRIVER1: the WW_* switch store

#include "gl/cellcluster.h"
#include "gl/cellunits.h"
#include "gl/glscene.h"
#include "gl/renderer.h"

#include <QElapsedTimer>
#include <QHash>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>

quint64 wwGlPaintCount();	// glview.cpp: frames painted

namespace
{

constexpr int kClusterUnit = WwCellUnit::LightCluster;	// isamplerBuffer (MERGEALL1: was 24, VOLAIR1's AirVol)
constexpr int kTexelsPerLight = 8;  // celllights.cpp's light layout, 4 floats a texel
constexpr int kRebuildsPerFrame = 4;
/* LIGHTCLUSTER1: grids kept per renderer (passes that switch camera or viewport each keep theirs), and how far a
 * TAA jitter may move the projection, in pixels either axis, before the lists are rebuilt. The lists are built
 * padded by the same amount (cellcluster.h jitterPadPx), so a pixel drawn under any jitter within it is covered. */
constexpr int kSlots = 4;
constexpr double kJitterReusePx = 1.0;

struct Key
{
	float proj[16];
	int vp[4];
	float view[13];
	std::uint64_t stamp;
	int count;
	bool operator==( const Key & o ) const { return std::memcmp( this, &o, sizeof( Key ) ) == 0; }
};

struct Settings
{
	bool on = true;
	bool keep = true;   // LIGHTCLUSTER1: grids kept across jitter, camera switches and draws (off/red: one grid, exact key)
	int stats = 0;      // WW_CELL_CLUSTER_STATS=n: a stats line every n frames (0 none)
	WwClusterParams params;
};

const Settings & settings()
{
	WW_SWITCH_STATIC( Settings, s, []() {
		Settings t;
		t.on = wwEnvStr( "WW_CELL_CLUSTER", QStringLiteral( "1" ) ) != QLatin1String( "0" );
		if ( const int v = wwEnvInt( "WW_CELL_CLUSTER_TILE" ); v > 0 )
			t.params.tilePx = std::clamp( v, 8, 512 );
		if ( const int v = wwEnvInt( "WW_CELL_CLUSTER_SLICES" ); v > 0 )
			t.params.slices = std::clamp( v, 1, 64 );
		if ( const int v = wwEnvInt( "WW_CELL_CLUSTER_MAX" ); v > 0 )
			t.params.maxPerCluster = v;
		// the refuter: slices culled at their middle depth only, so lights near a slice edge go missing
		const QString red = wwEnvStr( "WW_CELL_CLUSTER_RED" );
		t.params.redMidSlice = red.contains( QLatin1String( "midslice" ) );
		// LIGHTCLUSTER1: WW_CELL_CLUSTER_KEEP=0 the switch, WW_CELL_CLUSTER_RED=rebuild the red (both: the old
		// single grid on the exact projection, a rebuild per TAA frame, the buffer re-attached per draw)
		t.keep = wwEnvStr( "WW_CELL_CLUSTER_KEEP", QStringLiteral( "1" ) ) != QLatin1String( "0" )
			&& !red.contains( QLatin1String( "rebuild" ) );
		if ( t.keep )
			t.params.jitterPadPx = kJitterReusePx;
		if ( wwEnvIsSet( "WW_CELL_CLUSTER_STATS" ) )
			t.stats = std::max( wwEnvInt( "WW_CELL_CLUSTER_STATS" ), 1 );
		return t;
	}() );
	return s;
}

struct Slot
{
	GLuint buf = 0, tex = 0;
	bool valid = false;
	bool attached = false;  // the buffer is attached to the texture (once per upload, not per draw)
	Key key = {};           // key.proj is the projection the lists were built with (the jitter anchor)
	WwClusterGrid grid;
	WwClusterScratch scratch;
	quint64 used = 0;
};

struct Stats
{
	quint64 builds = 0, boxesKept = 0, hits = 0, jitterHits = 0, evictions = 0, capped = 0;
	double buildMs = 0.0;
	quint64 lastFrame = ~quint64( 0 );
};

struct ClusterGpu
{
	int units = -1;
	std::array<Slot, kSlots> grids;	// not "slots": a Qt keyword macro
	quint64 tick = 0;
	QElapsedTimer window;   // rebuilds counted per frame-length window
	int rebuilds = 0;
	Stats stats;
};

QHash<const void *, ClusterGpu> & gpus()
{
	static QHash<const void *, ClusterGpu> h;
	return h;
}

QString & lastEcho()
{
	static QString s;
	return s;
}

Key keyFor( Renderer * r, const Transform & vt, const int vp[4], int count, std::uint64_t stamp )
{
	Key k;
	std::memset( &k, 0, sizeof( k ) );
	std::memcpy( k.proj, &r->globalUniforms->projectionMatrix[0][0], sizeof( k.proj ) );
	std::copy( vp, vp + 4, k.vp );
	for ( int i = 0; i < 3; i++ )
		for ( int j = 0; j < 3; j++ )
			k.view[i * 3 + j] = vt.rotation( i, j );
	for ( int i = 0; i < 3; i++ )
		k.view[9 + i] = vt.translation[i];
	k.view[12] = vt.scale;
	k.stamp = stamp;
	k.count = count;
	return k;
}

// The uploaded lights in view space (posView = R p s + t, as Transform applies it).
std::vector<WwClusterLight> viewLights( const std::vector<float> & texels, int count, const Transform & vt )
{
	std::vector<WwClusterLight> out( size_t( std::max( count, 0 ) ) );
	for ( int i = 0; i < count && size_t( i + 1 ) * kTexelsPerLight * 4 <= texels.size(); i++ ) {
		const float * t = texels.data() + size_t( i ) * kTexelsPerLight * 4;
		WwClusterLight & l = out[size_t( i )];
		const Vector3 p = vt * Vector3( t[0], t[1], t[2] );
		const Vector3 d = vt.rotation * Vector3( t[8], t[9], t[10] );
		for ( int k = 0; k < 3; k++ ) {
			l.pos[k] = p[k];
			l.dir[k] = d[k];
		}
		l.radius = double( t[3] ) * double( vt.scale );
		const float shape = t[7];   // cos of the half angle for a spot; -2 omni, -3 hemisphere, -4 box
		l.shape = shape > -1.5f ? 1 : ( shape > -3.5f && shape < -2.5f ) ? 2 : 0;
		l.cosOuter = shape > -1.5f ? shape : -1.0;
		const double len = std::sqrt( l.dir[0] * l.dir[0] + l.dir[1] * l.dir[1] + l.dir[2] * l.dir[2] );
		if ( l.shape != 0 && len > 0.0 )
			for ( double & c : l.dir )
				c /= len;
		else if ( l.shape != 0 )
			l.shape = 0;    // no axis: cull as a sphere
	}
	return out;
}

void echo( const WwClusterGrid & g, int count )
{
	double mean = 0.0;
	int listed = 0;
	for ( int c = 0; c < g.clusterCount(); c++ )
		if ( g.cells[size_t( c ) * 2 + 1] >= 0 )
			listed++;
	if ( listed )
		mean = double( g.refs ) / double( listed );
	const QString line = QStringLiteral( "cell cluster: %1x%2 tiles of %3 px x %4 slices, %5 lights, refs %6, mean list %7, "
		"max list %8, clusters over the cap %9 (flat loop), lights off screen %10, build %11 ms%12" )
		.arg( g.tilesX ).arg( g.tilesY ).arg( g.tilePx ).arg( g.slices ).arg( count ).arg( g.refs )
		.arg( mean, 0, 'f', 1 ).arg( g.maxCount ).arg( g.overflowed ).arg( g.culledOffscreen )
		.arg( g.buildMs, 0, 'f', 3 ).arg( settings().params.redMidSlice ? QStringLiteral( " (RED midslice)" ) : QString() );
	// once per change of what the lists hold, not per frame
	const QString shape = line.section( QStringLiteral( ", build" ), 0, 0 );
	if ( shape != lastEcho().section( QStringLiteral( ", build" ), 0, 0 ) || wwEnvIsSet( "WW_CELL_CLUSTER_ECHO" ) )
		std::fprintf( stderr, "%s\n", qPrintable( line ) );
	lastEcho() = line;
}

bool rebuild( Renderer * r, ClusterGpu & c, Slot & sl, const Key & k, const std::vector<float> & texels, int count, const Transform & vt )
{
	if ( !c.window.isValid() || c.window.elapsed() > 16 ) {
		c.window.start();
		c.rebuilds = 0;
	}
	// a pass that switches cameras draw by draw keeps the flat loop instead of rebuilding per draw
	if ( ++c.rebuilds > kRebuildsPerFrame ) {
		c.stats.capped++;
		return false;
	}
	const quint64 kept = sl.scratch.hits;
	sl.valid = wwClusterBuild( k.proj, k.vp, viewLights( texels, count, vt ), settings().params, sl.grid,
		settings().keep ? &sl.scratch : nullptr );
	sl.key = k;
	sl.used = ++c.tick;
	if ( !sl.valid )
		return false;
	c.stats.builds++;
	c.stats.boxesKept += sl.scratch.hits - kept;
	c.stats.buildMs += sl.grid.buildMs;
	std::vector<std::int32_t> data;
	data.reserve( sl.grid.cells.size() + sl.grid.index.size() + 1 );
	data.insert( data.end(), sl.grid.cells.begin(), sl.grid.cells.end() );
	data.insert( data.end(), sl.grid.index.begin(), sl.grid.index.end() );
	data.push_back( 0 );    // a buffer texture must have a store
	auto fn = r->fn;
	if ( !sl.buf ) {
		fn->glGenBuffers( 1, &sl.buf );
		fn->glGenTextures( 1, &sl.tex );
	}
	fn->glBindBuffer( GL_TEXTURE_BUFFER, sl.buf );
	fn->glBufferData( GL_TEXTURE_BUFFER, GLsizeiptr( data.size() * sizeof( std::int32_t ) ), data.data(), GL_STREAM_DRAW );
	fn->glBindBuffer( GL_TEXTURE_BUFFER, 0 );
	sl.attached = false;
	echo( sl.grid, count );
	return true;
}

// LIGHTCLUSTER1: the same projection but for a TAA jitter (perspective: m[8], m[9]) within kJitterReusePx of the anchor
bool jitterNear( const Key & anchor, const Key & k )
{
	if ( !std::equal( k.vp, k.vp + 4, anchor.vp ) )
		return false;
	for ( int i = 0; i < 16; i++ )
		if ( i != 8 && i != 9 && k.proj[i] != anchor.proj[i] )
			return false;
	if ( k.proj[11] != -1.0f || k.proj[15] != 0.0f )
		return k.proj[8] == anchor.proj[8] && k.proj[9] == anchor.proj[9];   // not a perspective: exact
	const double dx = std::fabs( double( k.proj[8] ) - double( anchor.proj[8] ) ) * 0.5 * double( k.vp[2] );
	const double dy = std::fabs( double( k.proj[9] ) - double( anchor.proj[9] ) ) * 0.5 * double( k.vp[3] );
	return dx <= kJitterReusePx && dy <= kJitterReusePx;
}

bool sameScene( const Key & a, const Key & b )
{
	return std::memcmp( a.view, b.view, sizeof( a.view ) ) == 0 && a.stamp == b.stamp && a.count == b.count;
}

// The slot whose lists serve this draw, rebuilt when they cannot; null keeps the flat loop.
Slot * slotFor( Renderer * r, ClusterGpu & c, const Key & k, const std::vector<float> & texels, int count, const Transform & vt )
{
	Stats & st = c.stats;
	if ( !settings().keep ) {
		// the old path: one grid, the exact key
		Slot & sl = c.grids[0];
		if ( sl.valid && sl.key == k ) {
			st.hits++;
			return &sl;
		}
		return rebuild( r, c, sl, k, texels, count, vt ) ? &sl : nullptr;
	}
	Slot * nearSlot = nullptr;
	for ( Slot & sl : c.grids ) {
		if ( !sl.valid || !jitterNear( sl.key, k ) )
			continue;
		if ( sameScene( sl.key, k ) ) {
			sl.used = ++c.tick;
			st.hits++;
			st.jitterHits += sl.key.proj[8] != k.proj[8] || sl.key.proj[9] != k.proj[9];
			return &sl;
		}
		nearSlot = &sl;
	}
	Key b = k;
	if ( nearSlot ) {
		// the camera or lights moved: new lists under the slot's own projection, so its boxes stay
		std::copy( nearSlot->key.proj, nearSlot->key.proj + 16, b.proj );
		return rebuild( r, c, *nearSlot, b, texels, count, vt ) ? nearSlot : nullptr;
	}
	Slot * lru = &c.grids[0];
	for ( Slot & sl : c.grids )
		if ( !sl.valid || sl.used < lru->used ) {
			lru = &sl;
			if ( !sl.valid )
				break;
		}
	st.evictions += lru->valid;
	return rebuild( r, c, *lru, b, texels, count, vt ) ? lru : nullptr;
}

void statsLine( ClusterGpu & c )
{
	const quint64 f = wwGlPaintCount();
	if ( !settings().stats || f == c.stats.lastFrame || f % quint64( settings().stats ) != 0 )
		return;
	c.stats.lastFrame = f;
	const Stats & s = c.stats;
	std::fprintf( stderr, "cell cluster stats: frame %llu, %s, builds %llu (boxes kept %llu), list reuses %llu (across jitter %llu), "
		"evictions %llu, over the rebuild cap %llu, build ms total %.3f\n", (unsigned long long)f,
		settings().keep ? "keep" : "keep OFF (one grid, exact key)", (unsigned long long)s.builds,
		(unsigned long long)s.boxesKept, (unsigned long long)s.hits, (unsigned long long)s.jitterHits,
		(unsigned long long)s.evictions, (unsigned long long)s.capped, s.buildMs );
}

} // namespace

void wwCellClusterUniforms( Scene * scene, const std::vector<float> * texels, int count, std::uint64_t stamp )
{
	if ( !scene || !scene->renderer )
		return;
	Renderer * r = scene->renderer;
	NifSkopeOpenGLContext::Program * prog = r->getCurrentProgram();
	if ( !prog || prog->uniLocation( "cellClusterOn" ) < 0 )
		return;
	auto fn = r->fn;
	ClusterGpu & c = gpus()[r];
	statsLine( c );
	if ( c.units < 0 ) {
		GLint units = 0;
		fn->glGetIntegerv( GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, &units );	// a unit index is bounded by the combined limit
		c.units = units;
	}
	Slot * sl = nullptr;
	if ( texels && count > 0 && settings().on && c.units > kClusterUnit ) {
		GLint vp[4] = { 0, 0, 0, 0 };
		fn->glGetIntegerv( GL_VIEWPORT, vp );
		const Key k = keyFor( r, scene->view, vp, count, stamp );
		sl = slotFor( r, c, k, *texels, count, scene->view );
	}
	const bool on = sl != nullptr;
	{
		// the last unit stands in on a GPU without kClusterUnit: the sampler must not share unit 0 with BaseMap
		const int unit = std::min( kClusterUnit, c.units - 1 );
		GLint prevActive = 0;
		fn->glGetIntegerv( GL_ACTIVE_TEXTURE, &prevActive );
		fn->glActiveTexture( GLenum( GL_TEXTURE0 + unit ) );
		fn->glBindTexture( GL_TEXTURE_BUFFER, on ? sl->tex : 0 );
		if ( on && ( !sl->attached || !settings().keep ) ) {
			fn->glTexBuffer( GL_TEXTURE_BUFFER, GL_R32I, sl->buf );
			sl->attached = true;
		}
		fn->glActiveTexture( GLenum( prevActive ) );
		prog->uni1i( "cellCluster", unit );
	}
	prog->uni1b( "cellClusterOn", on );
	if ( !on )
		return;
	const WwClusterGrid & g = sl->grid;
	const Transform & vt = scene->view;
	prog->uni4f( "cellClusterDims", FloatVector4( float( g.tilesX ), float( g.tilesY ), float( g.slices ), float( g.tilePx ) ) );
	prog->uni2f( "cellClusterOrigin", float( g.vp[0] ), float( g.vp[1] ) );
	// view depth = -(posView.z) = -(s R[2] . P + t.z)
	prog->uni4f( "cellClusterDepth", FloatVector4( -vt.scale * vt.rotation( 2, 0 ), -vt.scale * vt.rotation( 2, 1 ),
		-vt.scale * vt.rotation( 2, 2 ), -vt.translation[2] ) );
	prog->uni2f( "cellClusterLog", g.logNear, g.sliceScale );
	prog->uni1i( "cellClusterBase", int( g.cells.size() ) );
}

QString wwCellClusterEcho()
{
	return lastEcho();
}
