/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#include "cellclustergl.h"

#include "gl/cellcluster.h"
#include "gl/glscene.h"
#include "gl/renderer.h"

#include <QElapsedTimer>
#include <QHash>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace
{

constexpr int kClusterUnit = 24;    // isamplerBuffer; units 8..23 belong to the other cell passes
constexpr int kTexelsPerLight = 8;  // celllights.cpp's light layout, 4 floats a texel
constexpr int kRebuildsPerFrame = 4;

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
	WwClusterParams params;
};

const Settings & settings()
{
	static const Settings s = []() {
		Settings t;
		t.on = qEnvironmentVariable( "WW_CELL_CLUSTER", QStringLiteral( "1" ) ) != QLatin1String( "0" );
		if ( const int v = qEnvironmentVariableIntValue( "WW_CELL_CLUSTER_TILE" ); v > 0 )
			t.params.tilePx = std::clamp( v, 8, 512 );
		if ( const int v = qEnvironmentVariableIntValue( "WW_CELL_CLUSTER_SLICES" ); v > 0 )
			t.params.slices = std::clamp( v, 1, 64 );
		if ( const int v = qEnvironmentVariableIntValue( "WW_CELL_CLUSTER_MAX" ); v > 0 )
			t.params.maxPerCluster = v;
		// the refuter: slices culled at their middle depth only, so lights near a slice edge go missing
		t.params.redMidSlice = qEnvironmentVariable( "WW_CELL_CLUSTER_RED" ) == QLatin1String( "midslice" );
		return t;
	}();
	return s;
}

struct ClusterGpu
{
	GLuint buf = 0, tex = 0;
	int units = -1;
	bool valid = false;
	Key key = {};
	WwClusterGrid grid;
	QElapsedTimer window;   // rebuilds counted per frame-length window
	int rebuilds = 0;
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
	if ( shape != lastEcho().section( QStringLiteral( ", build" ), 0, 0 ) || qEnvironmentVariableIsSet( "WW_CELL_CLUSTER_ECHO" ) )
		std::fprintf( stderr, "%s\n", qPrintable( line ) );
	lastEcho() = line;
}

bool rebuild( Renderer * r, ClusterGpu & c, const Key & k, const std::vector<float> & texels, int count, const Transform & vt )
{
	if ( !c.window.isValid() || c.window.elapsed() > 16 ) {
		c.window.start();
		c.rebuilds = 0;
	}
	// a pass that switches cameras draw by draw keeps the flat loop instead of rebuilding per draw
	if ( ++c.rebuilds > kRebuildsPerFrame )
		return false;
	c.valid = wwClusterBuild( k.proj, k.vp, viewLights( texels, count, vt ), settings().params, c.grid );
	c.key = k;
	if ( !c.valid )
		return false;
	std::vector<std::int32_t> data;
	data.reserve( c.grid.cells.size() + c.grid.index.size() + 1 );
	data.insert( data.end(), c.grid.cells.begin(), c.grid.cells.end() );
	data.insert( data.end(), c.grid.index.begin(), c.grid.index.end() );
	data.push_back( 0 );    // a buffer texture must have a store
	auto fn = r->fn;
	if ( !c.buf ) {
		fn->glGenBuffers( 1, &c.buf );
		fn->glGenTextures( 1, &c.tex );
	}
	fn->glBindBuffer( GL_TEXTURE_BUFFER, c.buf );
	fn->glBufferData( GL_TEXTURE_BUFFER, GLsizeiptr( data.size() * sizeof( std::int32_t ) ), data.data(), GL_STREAM_DRAW );
	fn->glBindBuffer( GL_TEXTURE_BUFFER, 0 );
	echo( c.grid, count );
	return true;
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
	if ( c.units < 0 ) {
		GLint units = 0;
		fn->glGetIntegerv( GL_MAX_TEXTURE_IMAGE_UNITS, &units );
		c.units = units;
	}
	bool on = false;
	if ( texels && count > 0 && settings().on && c.units > kClusterUnit ) {
		GLint vp[4] = { 0, 0, 0, 0 };
		fn->glGetIntegerv( GL_VIEWPORT, vp );
		const Key k = keyFor( r, scene->view, vp, count, stamp );
		on = ( c.valid && c.key == k ) || rebuild( r, c, k, *texels, count, scene->view );
	}
	{
		// the last unit stands in on a GPU without unit 24: the sampler must not share unit 0 with BaseMap
		const int unit = std::min( kClusterUnit, c.units - 1 );
		GLint prevActive = 0;
		fn->glGetIntegerv( GL_ACTIVE_TEXTURE, &prevActive );
		fn->glActiveTexture( GLenum( GL_TEXTURE0 + unit ) );
		fn->glBindTexture( GL_TEXTURE_BUFFER, on ? c.tex : 0 );
		if ( on )
			fn->glTexBuffer( GL_TEXTURE_BUFFER, GL_R32I, c.buf );
		fn->glActiveTexture( GLenum( prevActive ) );
		prog->uni1i( "cellCluster", unit );
	}
	prog->uni1b( "cellClusterOn", on );
	if ( !on )
		return;
	const WwClusterGrid & g = c.grid;
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
