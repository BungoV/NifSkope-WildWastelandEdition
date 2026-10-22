/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#include "gl/cellgpucull.h"

#include "gl/cellcull.h"
#include "gl/cellgrass.h"
#include "gl/cellhdr.h"
#include "gl/celllights.h"
#include "gl/cellpasstime.h"
#include "gl/cellqueue.h"
#include "gl/cellssr.h"
#include "gl/glcontext.hpp"
#include "gl/glscene.h"
#include "gl/glshape.h"
#include "gl/renderer.h"

#include <QElapsedTimer>
#include <QHash>
#include <QOpenGLContext>
#include <QOpenGLFunctions_4_3_Core>
#include <QOpenGLVersionFunctionsFactory>
#include <QSet>
#include <QStringList>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace GpuDrivenB
{

namespace
{

struct Switches
{
	bool on = true, cone = true, occ = true, frustum = true, hard = false, nocull = false;
	Switches()
	{
		on = qEnvironmentVariable( "WW_GPUDRIVEN_B", QStringLiteral( "1" ) ).trimmed() != QLatin1String( "0" );
		cone = qEnvironmentVariable( "WW_GPUDRIVEN_B_CONE", QStringLiteral( "1" ) ).trimmed() != QLatin1String( "0" );
		occ = qEnvironmentVariable( "WW_GPUDRIVEN_B_OCC", QStringLiteral( "1" ) ).trimmed() != QLatin1String( "0" );
		frustum = qEnvironmentVariable( "WW_GPUDRIVEN_B_FRUSTUM", QStringLiteral( "1" ) ).trimmed() != QLatin1String( "0" );
		const QStringList reds = qEnvironmentVariable( "WW_GPUDRIVEN_B_RED" ).split( QLatin1Char( ',' ), Qt::SkipEmptyParts );
		for ( const QString & r : reds ) {
			hard = hard || r.trimmed() == QLatin1String( "hard" );
			nocull = nocull || r.trimmed() == QLatin1String( "nocull" );
		}
	}
};

const Switches & sw()
{
	static const Switches s;
	return s;
}

constexpr std::uint32_t kClusterTris = 128;

// the programs that transform with fo4_default.vert (no displacement on a shape that is not skinned, grass or swaying)
bool vertexProgram( const std::string_view & n, bool & occluder )
{
	static const char * const solid[] = { "fo4_cell", "fo4_cellcsm", "fo4_csm", "fo4_default", "fo4_fog", "fo4_fogcsm" };
	// the PBR path discards on its own composition, which the queue does not see: a candidate, never an occluder
	static const char * const pbr[] = { "pbr_route", "pbrm_cell", "pbrm_cellcsm", "pbrm_csm", "pbrm_default" };
	std::string_view b = n;
	if ( b.size() > 5 && b.substr( b.size() - 5 ) == ".prog" )
		b = b.substr( 0, b.size() - 5 );
	for ( const char * s : solid ) {
		if ( b == s ) {
			occluder = true;
			return true;
		}
	}
	for ( const char * s : pbr ) {
		if ( b == s ) {
			occluder = false;
			return true;
		}
	}
	occluder = false;
	return false;
}

// what the last main frame drew the shape with (read back after setupProgram)
struct Learned
{
	const void * program = nullptr;
	std::uint32_t frame = 0, seen = 0;
	bool vertexOk = false, occluderProgram = false, opaque = false, cullOn = false, cullBack = false, ccw = true;
	// a culled fragment would have failed the depth test: it may leave neither colour, depth nor stencil behind
	bool candidate = false;
};

struct Rec
{
	std::uint32_t numTris = 0;
	bool whole = true;		// the CPU cull kept the whole shape
	bool indirect = false;	// drawn from its cluster commands
	std::uint32_t cmdFirst = 0, cmdCount = 0;
	std::vector<std::uint32_t> ranges;
};

struct Frame
{
	bool walk = false, flushing = false;
	std::uint32_t frame = 0;
	std::vector<std::pair<Shape *, Note>> notes;
	QSet<const Shape *> noted;
	QHash<const Shape *, Rec> recs;
	QHash<const Shape *, Learned> learned;
	// census
	qint64 shapes = 0, candidates = 0, occluders = 0, items = 0, fallback = 0, mdi = 0, mdiCmds = 0, rangeDraws = 0;
	qint64 trisKept = 0;
	std::uint32_t counts[8] = {};
	double cpuMs = 0.0, gpuMs[3] = { -1.0, -1.0, -1.0 };
	QString why;
};

Frame & fr()
{
	static Frame f;
	return f;
}

struct MeshletGpu
{
	float sphere[4];	// centre, radius (the shape's own space)
	float cone[4];	// axis, cutoff (> 1: no cone)
	std::uint32_t tri[4];	// first triangle, count
};
static_assert( sizeof( MeshletGpu ) == 48, "std430 layout" );

struct Cluster
{
	std::uint64_t h0 = 0, h1 = 0;
	std::uint32_t numTris = 0;
	const void * runs = nullptr;
	std::size_t numRuns = 0;
	std::uint32_t base = 0, lastFrame = 0;
	bool ok = false;
	std::vector<std::uint32_t> firsts;
	std::vector<MeshletGpu> data;
};

struct JobGpu
{
	float mv[16];
	float p[4];	// radius scale, cone sign (0 = no cone test), 0, 0
};
static_assert( sizeof( JobGpu ) == 80, "std430 layout" );

using G43 = QOpenGLFunctions_4_3_Core;

struct Gpu
{
	G43 * g = nullptr;
	bool tried = false, ok = false;
	GLuint progHiz0 = 0, progHiz0Ms = 0, progHizN = 0, progCull = 0;
	GLuint fbo = 0, depthTex = 0;
	int w = 0, h = 0, samples = -1;
	GLenum depthFmt = 0;
	GLuint hiz = 0, meshlets = 0, jobs = 0, items = 0, cmds = 0, census = 0;
	std::size_t hizBytes = 0, jobsBytes = 0, itemsBytes = 0, cmdsBytes = 0;
	std::size_t mCap = 0, mUsed = 0, mGarbage = 0;
	std::uint32_t frame = 0;
	QHash<const Shape *, Cluster> clusters;
	GLuint q[4] = {};
	bool qPending = false;
	qint64 builds = 0, compactions = 0;
};

QHash<const void *, Gpu> & gpus()
{
	static QHash<const void *, Gpu> g;
	return g;
}

const char * const kHiz0 = R"(
layout( local_size_x = 8, local_size_y = 8 ) in;
layout( std430, binding = 0 ) writeonly buffer HizBuf { float hiz[]; };
#ifdef MS
uniform sampler2DMS depthTex;
#else
uniform sampler2D depthTex;
#endif
uniform int samples;
uniform ivec2 dim;
void main()
{
	ivec2 p = ivec2( gl_GlobalInvocationID.xy );
	if ( p.x >= dim.x || p.y >= dim.y )
		return;
#ifdef MS
	float m = 0.0;
	for ( int s = 0; s < samples; s++ )
		m = max( m, texelFetch( depthTex, p, s ).r );
#else
	float m = texelFetch( depthTex, p, 0 ).r;
#endif
	hiz[p.y * dim.x + p.x] = m;
}
)";

const char * const kHizN = R"(
layout( local_size_x = 8, local_size_y = 8 ) in;
layout( std430, binding = 0 ) buffer HizBuf { float hiz[]; };
uniform int srcOff;
uniform int dstOff;
uniform ivec2 srcDim;
uniform ivec2 dstDim;
void main()
{
	ivec2 p = ivec2( gl_GlobalInvocationID.xy );
	if ( p.x >= dstDim.x || p.y >= dstDim.y )
		return;
	ivec2 a = p * 2;
	ivec2 b = min( a + 1, srcDim - 1 );
	float m = max( max( hiz[srcOff + a.y * srcDim.x + a.x], hiz[srcOff + a.y * srcDim.x + b.x] ),
		max( hiz[srcOff + b.y * srcDim.x + a.x], hiz[srcOff + b.y * srcDim.x + b.x] ) );
	hiz[dstOff + p.y * dstDim.x + p.x] = m;
}
)";

/* One thread per cluster. Culled: wholly outside a frustum plane (1), every triangle facing away (2, the
 * meshoptimizer sphere-cone test), or nearer than nothing it covers (3): its nearest depth is farther than the
 * farthest prepass depth under its screen box, one pixel wider on every side. */
const char * const kCull = R"(
layout( local_size_x = 64 ) in;
struct Meshlet { vec4 sphere; vec4 cone; uvec4 tri; };
struct Job { mat4 mv; vec4 p; };
layout( std430, binding = 0 ) readonly buffer HizBuf { float hiz[]; };
layout( std430, binding = 1 ) readonly buffer MeshletBuf { Meshlet ml[]; };
layout( std430, binding = 2 ) readonly buffer JobBuf { Job jobs[]; };
layout( std430, binding = 3 ) readonly buffer ItemBuf { uvec2 items[]; };
layout( std430, binding = 4 ) writeonly buffer CmdBuf { uint cmds[]; };
layout( std430, binding = 5 ) buffer CensusBuf { uint census[8]; };
uniform mat4 proj;
uniform vec4 planes[5];
uniform uint numItems;
uniform ivec2 vpSize;
uniform int levels;
uniform int lvlOff[16];
uniform ivec2 lvlDim[16];
uniform int mode;	// 1 occlusion, 2 cone, 4 frustum, 8 red hard, 16 red nocull, 32 census
uniform float depthEps;
void main()
{
	uint i = gl_GlobalInvocationID.x;
	if ( i >= numItems )
		return;
	uvec2 it = items[i];
	Meshlet m = ml[it.y];
	Job j = jobs[it.x];
	vec3 c = ( j.mv * vec4( m.sphere.xyz, 1.0 ) ).xyz;
	float r = m.sphere.w * j.p.x;
	int why = 0;
	if ( ( mode & 4 ) != 0 ) {
		float margin = r * 1.001 + 1e-5 * length( c );
		for ( int k = 0; k < 5; k++ ) {
			if ( dot( planes[k].xyz, c ) + planes[k].w < -margin ) {
				why = 1;
				break;
			}
		}
	}
	if ( why == 0 && ( mode & 2 ) != 0 && j.p.y != 0.0 && m.cone.w <= 1.0 ) {
		vec3 a = normalize( mat3( j.mv ) * m.cone.xyz ) * j.p.y;
		if ( dot( c, a ) >= m.cone.w * length( c ) + r )
			why = 2;
	}
	if ( why == 0 && ( mode & 1 ) != 0 ) {
		bool front = true;
		vec2 lo = vec2( 1e30 ), hi = vec2( -1e30 );
		for ( int k = 0; k < 8; k++ ) {
			vec3 o = vec3( ( k & 1 ) != 0 ? r : -r, ( k & 2 ) != 0 ? r : -r, ( k & 4 ) != 0 ? r : -r );
			vec4 q = proj * vec4( c + o, 1.0 );
			if ( q.w <= 0.0 || q.z < -q.w ) {
				front = false;
				break;
			}
			lo = min( lo, q.xy / q.w );
			hi = max( hi, q.xy / q.w );
		}
		if ( front ) {
			vec2 p0 = ( lo * 0.5 + 0.5 ) * vec2( vpSize ) - 1.0;
			vec2 p1 = ( hi * 0.5 + 0.5 ) * vec2( vpSize ) + 1.0;
			if ( p1.x >= 0.0 && p1.y >= 0.0 && p0.x < float( vpSize.x ) && p0.y < float( vpSize.y ) ) {
				ivec2 a = clamp( ivec2( floor( p0 ) ), ivec2( 0 ), vpSize - 1 );
				ivec2 b = clamp( ivec2( floor( p1 ) ), ivec2( 0 ), vpSize - 1 );
				int span = max( b.x - a.x, b.y - a.y ) + 1;
				int L = 0;
				while ( L < levels - 1 && ( 1 << L ) < span )
					L++;
				ivec2 ta = a >> L, tb = b >> L;
				float h = 0.0;
				for ( int y = ta.y; y <= tb.y; y++ )
					for ( int x = ta.x; x <= tb.x; x++ )
						h = max( h, hiz[lvlOff[L] + y * lvlDim[L].x + x] );
				float z = ( mode & 8 ) != 0 ? c.z : c.z + r;
				vec4 q = proj * vec4( 0.0, 0.0, z, 1.0 );
				if ( ( q.z / q.w ) * 0.5 + 0.5 > h + depthEps )
					why = 3;
			}
		}
	}
	if ( ( mode & 16 ) != 0 )
		why = 0;
	uint o = i * 5u;
	cmds[o] = m.tri.y * 3u;
	cmds[o + 1u] = why == 0 ? 1u : 0u;
	cmds[o + 2u] = m.tri.x * 3u;
	cmds[o + 3u] = 0u;
	cmds[o + 4u] = 0u;
	if ( ( mode & 32 ) != 0 ) {
		atomicAdd( census[why], 1u );
		if ( why == 0 )
			atomicAdd( census[4], m.tri.y );
	}
}
)";

GLuint compile( G43 * g, const char * body, const char * defines )
{
	const std::string src = std::string( "#version 430 core\n" ) + defines + body;
	const char * s = src.c_str();
	const GLuint sh = g->glCreateShader( GL_COMPUTE_SHADER );
	g->glShaderSource( sh, 1, &s, nullptr );
	g->glCompileShader( sh );
	GLint ok = 0;
	g->glGetShaderiv( sh, GL_COMPILE_STATUS, &ok );
	if ( !ok ) {
		char log[2048] = {};
		g->glGetShaderInfoLog( sh, sizeof( log ) - 1, nullptr, log );
		std::fprintf( stderr, "gpudriven b: compute shader failed: %s\n", log );
		g->glDeleteShader( sh );
		return 0;
	}
	const GLuint p = g->glCreateProgram();
	g->glAttachShader( p, sh );
	g->glLinkProgram( p );
	g->glDeleteShader( sh );
	g->glGetProgramiv( p, GL_LINK_STATUS, &ok );
	if ( !ok ) {
		char log[2048] = {};
		g->glGetProgramInfoLog( p, sizeof( log ) - 1, nullptr, log );
		std::fprintf( stderr, "gpudriven b: compute link failed: %s\n", log );
		g->glDeleteProgram( p );
		return 0;
	}
	return p;
}

Gpu * gpuOf()
{
	QOpenGLContext * ctx = QOpenGLContext::currentContext();
	if ( !ctx )
		return nullptr;
	Gpu & G = gpus()[ctx];
	if ( !G.tried ) {
		G.tried = true;
		const QSurfaceFormat f = ctx->format();
		if ( f.majorVersion() * 10 + f.minorVersion() >= 43 ) {
			G.g = QOpenGLVersionFunctionsFactory::get<G43>( ctx );
			if ( G.g && G.g->initializeOpenGLFunctions() ) {
				G.progHiz0 = compile( G.g, kHiz0, "" );
				G.progHiz0Ms = compile( G.g, kHiz0, "#define MS 1\n" );
				G.progHizN = compile( G.g, kHizN, "" );
				G.progCull = compile( G.g, kCull, "" );
				G.ok = G.progHiz0 && G.progHiz0Ms && G.progHizN && G.progCull;
				if ( G.ok )
					G.g->glGenQueries( 4, G.q );
			}
		}
		if ( GpuDriven::census() )
			std::fprintf( stderr, "gpudriven b: GL %d.%d, compute %s\n", f.majorVersion(), f.minorVersion(),
				G.ok ? "ready" : "unavailable (step A path)" );
	}
	return G.ok ? &G : nullptr;
}

// a bigger buffer of `need` bytes, nothing kept (orphaned every frame: no wait on the last frame's reads)
void stream( G43 * g, GLuint & b, std::size_t & cap, std::size_t need, const void * data )
{
	if ( !b )
		g->glGenBuffers( 1, &b );
	g->glBindBuffer( GL_SHADER_STORAGE_BUFFER, b );
	if ( need > cap )
		cap = std::max< std::size_t >( need + need / 2, 4096 );
	g->glBufferData( GL_SHADER_STORAGE_BUFFER, GLsizeiptr( cap ), nullptr, GL_STREAM_DRAW );
	if ( data && need )
		g->glBufferSubData( GL_SHADER_STORAGE_BUFFER, 0, GLsizeiptr( need ), data );
	g->glBindBuffer( GL_SHADER_STORAGE_BUFFER, 0 );
}

// ---- clusters

inline float len3( const float * v )
{
	return std::sqrt( v[0] * v[0] + v[1] * v[1] + v[2] * v[2] );
}

// one cluster's sphere and normal cone (meshoptimizer's meshopt_computeClusterBounds, sphere form)
bool bounds( const Shape * sh, std::uint32_t first, std::uint32_t count, MeshletGpu & m )
{
	const qsizetype nv = sh->verts.size();
	float lo[3] = { 1e30f, 1e30f, 1e30f }, hi[3] = { -1e30f, -1e30f, -1e30f };
	for ( std::uint32_t t = first; t < first + count; t++ ) {
		const Triangle & tr = sh->triangles.at( qsizetype( t ) );
		for ( int k = 0; k < 3; k++ ) {
			if ( qsizetype( tr[k] ) >= nv )
				return false;
			const Vector3 & v = sh->verts.at( tr[k] );
			for ( int a = 0; a < 3; a++ ) {
				lo[a] = std::min( lo[a], v[a] );
				hi[a] = std::max( hi[a], v[a] );
			}
		}
	}
	float c[3], r2 = 0.0f;
	for ( int a = 0; a < 3; a++ )
		c[a] = ( lo[a] + hi[a] ) * 0.5f;
	float axis[3] = { 0, 0, 0 };
	std::vector<float> normals;
	normals.reserve( std::size_t( count ) * 3 );
	for ( std::uint32_t t = first; t < first + count; t++ ) {
		const Triangle & tr = sh->triangles.at( qsizetype( t ) );
		const Vector3 & p0 = sh->verts.at( tr[0] ), & p1 = sh->verts.at( tr[1] ), & p2 = sh->verts.at( tr[2] );
		for ( const Vector3 * p : { &p0, &p1, &p2 } ) {
			const float d[3] = { ( *p )[0] - c[0], ( *p )[1] - c[1], ( *p )[2] - c[2] };
			r2 = std::max( r2, d[0] * d[0] + d[1] * d[1] + d[2] * d[2] );
		}
		const float e1[3] = { p1[0] - p0[0], p1[1] - p0[1], p1[2] - p0[2] };
		const float e2[3] = { p2[0] - p0[0], p2[1] - p0[1], p2[2] - p0[2] };
		float n[3] = { e1[1] * e2[2] - e1[2] * e2[1], e1[2] * e2[0] - e1[0] * e2[2], e1[0] * e2[1] - e1[1] * e2[0] };
		const float l = len3( n );
		if ( !( l > 0.0f ) || !std::isfinite( l ) )
			continue;	// no area: never rasterized
		for ( int a = 0; a < 3; a++ ) {
			n[a] /= l;
			axis[a] += n[a];
			normals.push_back( n[a] );
		}
	}
	const float cl = len3( c );
	const float r = std::sqrt( r2 );
	m.sphere[0] = c[0];
	m.sphere[1] = c[1];
	m.sphere[2] = c[2];
	m.sphere[3] = r * 1.0005f + 1e-5f * std::max( 1.0f, cl );
	m.cone[0] = m.cone[1] = m.cone[2] = 0.0f;
	m.cone[3] = 2.0f;
	const float al = len3( axis );
	if ( al > 0.0f && std::isfinite( al ) && !normals.empty() ) {
		for ( int a = 0; a < 3; a++ )
			axis[a] /= al;
		float mindp = 1.0f;
		for ( std::size_t k = 0; k + 2 < normals.size(); k += 3 )
			mindp = std::min( mindp, normals[k] * axis[0] + normals[k + 1] * axis[1] + normals[k + 2] * axis[2] );
		if ( mindp > 0.1f ) {
			// sin of the spread, a little wider than measured (the test only gets harder to pass)
			const float cutoff = std::sqrt( std::max( 0.0f, 1.0f - mindp * mindp ) ) + 0.01f;
			if ( cutoff < 1.0f ) {
				m.cone[0] = axis[0];
				m.cone[1] = axis[1];
				m.cone[2] = axis[2];
				m.cone[3] = cutoff;
			}
		}
	}
	m.tri[0] = first;
	m.tri[1] = count;
	m.tri[2] = m.tri[3] = 0;
	return std::isfinite( m.sphere[3] ) && std::isfinite( cl );
}

void build( const Shape * sh, const std::vector<WwCullRun> * runs, std::uint32_t numTris, Cluster & c )
{
	c.firsts.clear();
	c.data.clear();
	c.ok = false;
	// the placements' runs when they cover the shape in order, so the CPU cull's ranges fall on cluster bounds
	std::vector<std::pair<std::uint32_t, std::uint32_t>> seg;
	if ( runs && !runs->empty() ) {
		std::uint32_t at = 0;
		for ( const WwCullRun & r : *runs ) {
			if ( r.first != at || r.first + r.count > numTris )
				break;
			if ( r.count )
				seg.push_back( { r.first, r.count } );
			at = r.first + r.count;
		}
		if ( at != numTris )
			seg.clear();
	}
	if ( seg.empty() )
		seg.push_back( { 0, numTris } );
	for ( const auto & s : seg ) {
		for ( std::uint32_t t = s.first; t < s.first + s.second; t += kClusterTris ) {
			MeshletGpu m;
			if ( !bounds( sh, t, std::min( kClusterTris, s.first + s.second - t ), m ) ) {
				c.firsts.clear();
				c.data.clear();
				return;
			}
			c.firsts.push_back( t );
			c.data.push_back( m );
		}
	}
	c.ok = !c.data.empty();
}

void arenaUpload( G43 * g, Gpu & G, std::size_t first, const std::vector<MeshletGpu> & d )
{
	g->glBindBuffer( GL_SHADER_STORAGE_BUFFER, G.meshlets );
	g->glBufferSubData( GL_SHADER_STORAGE_BUFFER, GLintptr( first * sizeof( MeshletGpu ) ),
		GLsizeiptr( d.size() * sizeof( MeshletGpu ) ), d.data() );
	g->glBindBuffer( GL_SHADER_STORAGE_BUFFER, 0 );
}

// every live cluster set uploaded again into a store sized for them (the garbage left behind)
void arenaRebuild( G43 * g, Gpu & G, std::size_t extra )
{
	std::size_t live = 0;
	for ( const Cluster & c : std::as_const( G.clusters ) )
		live += c.ok ? c.data.size() : 0;
	G.mCap = std::max< std::size_t >( ( live + extra ) + ( live + extra ) / 2, std::size_t( 1 ) << 14 );
	if ( !G.meshlets )
		g->glGenBuffers( 1, &G.meshlets );
	g->glBindBuffer( GL_SHADER_STORAGE_BUFFER, G.meshlets );
	g->glBufferData( GL_SHADER_STORAGE_BUFFER, GLsizeiptr( G.mCap * sizeof( MeshletGpu ) ), nullptr, GL_STATIC_DRAW );
	g->glBindBuffer( GL_SHADER_STORAGE_BUFFER, 0 );
	G.mUsed = 0;
	for ( Cluster & c : G.clusters ) {
		if ( !c.ok )
			continue;
		c.base = std::uint32_t( G.mUsed );
		arenaUpload( g, G, G.mUsed, c.data );
		G.mUsed += c.data.size();
	}
	G.mGarbage = 0;
	G.compactions++;
}

const Cluster * clusters( Gpu & G, Scene * scene, const Shape * sh, std::uint32_t numTris )
{
	const NifSkopeOpenGLContext::ShapeDataHash * h = sh->wwDataHash();
	if ( !h )
		return nullptr;
	const std::vector<WwCullRun> * runs = wwCellCullRuns( scene->nifModel, sh->id() );
	const std::size_t nRuns = runs ? runs->size() : 0;
	auto it = G.clusters.find( sh );
	if ( it != G.clusters.end() ) {
		Cluster & c = *it;
		if ( c.h0 == h->h[0] && c.h1 == h->h[1] && c.numTris == numTris && c.runs == runs && c.numRuns == nRuns ) {
			c.lastFrame = G.frame;
			return c.ok ? &c : nullptr;
		}
		G.mGarbage += c.ok ? c.data.size() : 0;
		G.clusters.erase( it );
	}
	Cluster c;
	c.h0 = h->h[0];
	c.h1 = h->h[1];
	c.numTris = numTris;
	c.runs = runs;
	c.numRuns = nRuns;
	c.lastFrame = G.frame;
	build( sh, runs, numTris, c );
	G.builds++;
	if ( c.ok ) {
		if ( G.mUsed + c.data.size() > G.mCap ) {
			G.clusters.insert( sh, c );
			arenaRebuild( G.g, G, 0 );
			return &G.clusters[sh];
		}
		c.base = std::uint32_t( G.mUsed );
		arenaUpload( G.g, G, G.mUsed, c.data );
		G.mUsed += c.data.size();
	}
	auto ins = G.clusters.insert( sh, c );
	return ins->ok ? &( *ins ) : nullptr;
}

// ---- the frame

bool cameraPass( Scene * scene )
{
	return sw().on && GpuDriven::mainPass() && scene && scene->renderer && !scene->selecting && wwCellHdrActive()
		&& !wwCellAoInPass() && !wwCellSsrInPass() && !wwCellImageSpaceIsMeasuring() && !wwCellProbePass( scene )
		&& !wwCellAlbedoProbePass( scene );
}

// the projection reads view depth into w alone, and x, y never into z (the Hi-Z and the cone assume it)
bool standardProjection( const FloatVector4 * pm )
{
	return pm[0][3] == 0.0f && pm[1][3] == 0.0f && pm[2][3] == -1.0f && pm[3][3] == 0.0f && pm[0][2] == 0.0f
		&& pm[1][2] == 0.0f;
}

struct GlSaved
{
	GLint drawFbo = 0, readFbo = 0, vp[4] = {}, depthFunc = 0, cullMode = 0, frontFace = 0, active = 0, tex2d = 0,
		texMs = 0;
	GLboolean depthMask = GL_TRUE;
	GLfloat clearDepth = 1.0f, offFactor = 0.0f, offUnits = 0.0f;
	bool depthTest = false, cull = false, offFill = false, stencil = false, blend = false, ms = false;
};

void save( G43 * g, GlSaved & s )
{
	g->glGetIntegerv( GL_DRAW_FRAMEBUFFER_BINDING, &s.drawFbo );
	g->glGetIntegerv( GL_READ_FRAMEBUFFER_BINDING, &s.readFbo );
	g->glGetIntegerv( GL_VIEWPORT, s.vp );
	g->glGetIntegerv( GL_DEPTH_FUNC, &s.depthFunc );
	g->glGetBooleanv( GL_DEPTH_WRITEMASK, &s.depthMask );
	g->glGetFloatv( GL_DEPTH_CLEAR_VALUE, &s.clearDepth );
	g->glGetIntegerv( GL_CULL_FACE_MODE, &s.cullMode );
	g->glGetIntegerv( GL_FRONT_FACE, &s.frontFace );
	g->glGetFloatv( GL_POLYGON_OFFSET_FACTOR, &s.offFactor );
	g->glGetFloatv( GL_POLYGON_OFFSET_UNITS, &s.offUnits );
	g->glGetIntegerv( GL_ACTIVE_TEXTURE, &s.active );
	s.depthTest = g->glIsEnabled( GL_DEPTH_TEST );
	s.cull = g->glIsEnabled( GL_CULL_FACE );
	s.offFill = g->glIsEnabled( GL_POLYGON_OFFSET_FILL );
	s.stencil = g->glIsEnabled( GL_STENCIL_TEST );
	s.blend = g->glIsEnabled( GL_BLEND );
	s.ms = g->glIsEnabled( GL_MULTISAMPLE );
	g->glActiveTexture( GL_TEXTURE0 );
	g->glGetIntegerv( GL_TEXTURE_BINDING_2D, &s.tex2d );
	g->glGetIntegerv( GL_TEXTURE_BINDING_2D_MULTISAMPLE, &s.texMs );
}

void enable( G43 * g, GLenum cap, bool on )
{
	on ? g->glEnable( cap ) : g->glDisable( cap );
}

void restore( G43 * g, const GlSaved & s )
{
	g->glBindFramebuffer( GL_DRAW_FRAMEBUFFER, GLuint( s.drawFbo ) );
	g->glBindFramebuffer( GL_READ_FRAMEBUFFER, GLuint( s.readFbo ) );
	g->glViewport( s.vp[0], s.vp[1], s.vp[2], s.vp[3] );
	g->glDepthFunc( GLenum( s.depthFunc ) );
	g->glDepthMask( s.depthMask );
	g->glClearDepth( s.clearDepth );
	g->glCullFace( GLenum( s.cullMode ) );
	g->glFrontFace( GLenum( s.frontFace ) );
	g->glPolygonOffset( s.offFactor, s.offUnits );
	enable( g, GL_DEPTH_TEST, s.depthTest );
	enable( g, GL_CULL_FACE, s.cull );
	enable( g, GL_POLYGON_OFFSET_FILL, s.offFill );
	enable( g, GL_STENCIL_TEST, s.stencil );
	enable( g, GL_BLEND, s.blend );
	enable( g, GL_MULTISAMPLE, s.ms );
	g->glActiveTexture( GL_TEXTURE0 );
	g->glBindTexture( GL_TEXTURE_2D, GLuint( s.tex2d ) );
	g->glBindTexture( GL_TEXTURE_2D_MULTISAMPLE, GLuint( s.texMs ) );
	g->glActiveTexture( GLenum( s.active ) );
}

// the prepass target: the main frame's size, samples and depth format (the same rasterization, the same depths)
bool target( G43 * g, Gpu & G, int w, int h, int samples, GLenum fmt )
{
	if ( G.fbo && G.w == w && G.h == h && G.samples == samples && G.depthFmt == fmt )
		return true;
	if ( G.depthTex )
		g->glDeleteTextures( 1, &G.depthTex );
	if ( !G.fbo )
		g->glGenFramebuffers( 1, &G.fbo );
	g->glGenTextures( 1, &G.depthTex );
	g->glBindFramebuffer( GL_DRAW_FRAMEBUFFER, G.fbo );
	if ( samples > 1 ) {
		g->glBindTexture( GL_TEXTURE_2D_MULTISAMPLE, G.depthTex );
		g->glTexImage2DMultisample( GL_TEXTURE_2D_MULTISAMPLE, samples, fmt, w, h, GL_TRUE );
		g->glFramebufferTexture2D( GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D_MULTISAMPLE, G.depthTex, 0 );
	} else {
		g->glBindTexture( GL_TEXTURE_2D, G.depthTex );
		g->glTexImage2D( GL_TEXTURE_2D, 0, GLint( fmt ), w, h, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr );
		g->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );	// complete without mips: texelFetch reads it
		g->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
		g->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_NONE );
		g->glFramebufferTexture2D( GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, G.depthTex, 0 );
	}
	g->glDrawBuffer( GL_NONE );
	g->glReadBuffer( GL_NONE );
	const GLenum st = g->glCheckFramebufferStatus( GL_DRAW_FRAMEBUFFER );
	G.w = w;
	G.h = h;
	G.samples = samples;
	G.depthFmt = fmt;
	if ( st != GL_FRAMEBUFFER_COMPLETE ) {
		G.w = 0;
		return false;
	}
	return true;
}

struct Occluder
{
	const Shape * sh;
	const Rec * rec;
	bool cull;
};

} // namespace

bool on()
{
	return sw().on;
}

void cameraWalk( bool on )
{
	fr().walk = on && sw().on;
}

void note( Shape * sh, const Note & n )
{
	Frame & f = fr();
	if ( !f.walk || f.flushing )
		return;
	f.notes.push_back( { sh, n } );
}

void prepare( Scene * scene )
{
	Frame & f = fr();
	f.recs.clear();
	f.noted.clear();
	f.flushing = false;
	if ( f.notes.empty() || !cameraPass( scene ) ) {
		f.notes.clear();
		return;
	}
	Gpu * gp = gpuOf();
	NifSkopeOpenGLContext * r = scene->renderer;
	const FloatVector4 * pm = r->globalUniforms->projectionMatrix;
	if ( !gp || !standardProjection( pm ) ) {
		f.why = gp ? QStringLiteral( "off: projection" ) : QStringLiteral( "off: no compute" );
		f.notes.clear();
		return;
	}
	Gpu & G = *gp;
	G43 * g = G.g;
	QElapsedTimer cpu;
	if ( GpuDriven::census() )
		cpu.start();
	f.frame++;
	G.frame++;
	f.flushing = true;
	for ( const auto & n : f.notes )
		f.noted.insert( n.first );

	GlSaved S;
	save( g, S );
	GLint samples = 0;
	g->glGetIntegerv( GL_SAMPLES, &samples );
	GLint compType = 0, depthBits = 0;
	if ( S.drawFbo ) {
		g->glGetFramebufferAttachmentParameteriv( GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
			GL_FRAMEBUFFER_ATTACHMENT_COMPONENT_TYPE, &compType );
		g->glGetFramebufferAttachmentParameteriv( GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
			GL_FRAMEBUFFER_ATTACHMENT_DEPTH_SIZE, &depthBits );
	}
	const GLenum depthFmt = compType == GL_FLOAT ? GL_DEPTH_COMPONENT32F
		: ( depthBits == 24 ? GL_DEPTH_COMPONENT24 : ( depthBits == 16 ? GL_DEPTH_COMPONENT16 : GLenum( 0 ) ) );
	const int vw = S.vp[2], vh = S.vp[3];
	// the prepass must rasterize as the main frame does: anything unusual and occlusion sits out this frame
	bool occ = sw().occ && S.drawFbo && depthFmt && vw > 0 && vh > 0 && !g->glIsEnabled( GL_SCISSOR_TEST )
		&& !g->glIsEnabled( GL_DEPTH_CLAMP );
	if ( G.qPending ) {
		GLint avail = 0;
		g->glGetQueryObjectiv( G.q[3], GL_QUERY_RESULT_AVAILABLE, &avail );
		if ( avail ) {
			GLuint64 t[4] = {};
			for ( int k = 0; k < 4; k++ )
				g->glGetQueryObjectui64v( G.q[k], GL_QUERY_RESULT, &t[k] );
			for ( int k = 0; k < 3; k++ )
				f.gpuMs[k] = double( t[k + 1] - t[k] ) * 1e-6;
			G.qPending = false;
		}
	}
	const bool timed = GpuDriven::census() && !G.qPending;
	if ( timed )
		g->glQueryCounter( G.q[0], GL_TIMESTAMP );

	// the candidates: what the CPU cull keeps of them, their clusters, their job
	std::vector<JobGpu> jobs;
	std::vector<std::uint32_t> items;	// (job, cluster) pairs
	std::vector<Occluder> occluders;
	f.shapes = qint64( f.notes.size() );
	f.candidates = f.occluders = f.fallback = f.trisKept = 0;
	f.mdi = f.mdiCmds = f.rangeDraws = 0;
	static std::vector<std::uint32_t> ranges;
	for ( const auto & nn : f.notes ) {
		Shape * sh = nn.first;
		const Note & n = nn.second;
		auto lt = f.learned.constFind( sh );
		if ( lt == f.learned.constEnd() || !n.fullLod || n.swaying || n.queue < 0 || n.queue > GpuDriven::AlphaTest )
			continue;
		const Learned & L = *lt;
		if ( !L.vertexOk || !L.candidate || L.program != n.program || f.frame - L.seen > 2 || f.recs.contains( sh ) )
			continue;
		const qsizetype nt = sh->triangles.size();
		if ( nt <= 0 || nt > qsizetype( 0x3fffffff ) || sh->verts.isEmpty() || sh->wwGpuSkinned()
			|| wwCellGrassIsShape( scene->nifModel, sh->id() ) )
			continue;
		Rec & rec = f.recs[sh];
		rec.numTris = std::uint32_t( nt );
		rec.whole = !wwCellCullCamera( scene, sh, sh->id(), std::int64_t( nt ), ranges );	// once per shape per walk
		if ( rec.whole )
			rec.ranges = { 0u, rec.numTris };
		else
			rec.ranges = ranges;
		f.candidates++;
		const Cluster * c = clusters( G, scene, sh, rec.numTris );
		if ( c ) {
			// the kept ranges as cluster spans; a range off the cluster bounds keeps the CPU draw
			const std::size_t i0 = items.size();
			bool aligned = true;
			for ( std::size_t k = 0; k + 1 < rec.ranges.size() && aligned; k += 2 ) {
				const std::uint32_t a = rec.ranges[k], e = a + rec.ranges[k + 1];
				if ( !rec.ranges[k + 1] )
					continue;
				auto lo = std::lower_bound( c->firsts.begin(), c->firsts.end(), a );
				auto hi = std::lower_bound( c->firsts.begin(), c->firsts.end(), e );
				if ( lo == c->firsts.end() || *lo != a || ( hi == c->firsts.end() ? e != rec.numTris : *hi != e ) ) {
					aligned = false;
					break;
				}
				for ( auto k2 = lo; k2 != hi; ++k2 ) {
					items.push_back( std::uint32_t( jobs.size() ) );
					items.push_back( c->base + std::uint32_t( k2 - c->firsts.begin() ) );
				}
			}
			if ( aligned && items.size() > i0 ) {
				JobGpu j;
				const Matrix4 mv = sh->viewTrans().toMatrix4();
				std::memcpy( j.mv, mv.data(), sizeof( j.mv ) );
				const float * m = j.mv;
				const float c0 = len3( m ), c1 = len3( m + 4 ), c2 = len3( m + 8 );
				const float d01 = m[0] * m[4] + m[1] * m[5] + m[2] * m[6], d02 = m[0] * m[8] + m[1] * m[9] + m[2] * m[10],
					d12 = m[4] * m[8] + m[5] * m[9] + m[6] * m[10];
				const float tol = 1e-4f * c0;
				const bool similar = c0 > 0.0f && std::fabs( c1 - c0 ) <= tol && std::fabs( c2 - c0 ) <= tol
					&& std::fabs( d01 ) <= tol * c0 && std::fabs( d02 ) <= tol * c0 && std::fabs( d12 ) <= tol * c0;
				const float det = m[0] * ( m[5] * m[10] - m[6] * m[9] ) - m[4] * ( m[1] * m[10] - m[2] * m[9] )
					+ m[8] * ( m[1] * m[6] - m[2] * m[5] );
				// the radius: the column length of a similarity, else the Frobenius norm (never smaller than the stretch)
				j.p[0] = similar ? std::max( { c0, c1, c2 } ) * 1.0002f : std::sqrt( c0 * c0 + c1 * c1 + c2 * c2 );
				// the cone only where GL culls back faces and the transform keeps normals' directions
				j.p[1] = ( similar && L.cullOn && L.cullBack && det != 0.0f ) ? ( ( det > 0.0f ) == L.ccw ? 1.0f : -1.0f ) : 0.0f;
				j.p[2] = j.p[3] = 0.0f;
				rec.indirect = true;
				rec.cmdFirst = std::uint32_t( i0 / 2 );
				rec.cmdCount = std::uint32_t( ( items.size() - i0 ) / 2 );
				jobs.push_back( j );
			} else {
				items.resize( i0 );
			}
		}
		if ( !rec.indirect )
			f.fallback++;
		for ( std::size_t k = 0; k + 1 < rec.ranges.size(); k += 2 )
			f.trisKept += rec.ranges[k + 1];
		// an occluder: drawn solid and opaque last frame, through a program whose depth the prepass reproduces
		if ( occ && n.queue != GpuDriven::AlphaTest && L.occluderProgram && L.opaque && ( !L.cullOn || ( L.cullBack && L.ccw ) ) )
			occluders.push_back( { sh, &rec, L.cullOn } );
	}
	f.items = qint64( items.size() / 2 );
	f.occluders = qint64( occluders.size() );
	occ = occ && !occluders.empty();

	// 1. the prepass: the occluders' kept ranges from the step A pool, one indirect multi-draw per cull state
	if ( occ && target( g, G, vw, vh, samples, depthFmt ) && GpuDriven::poolBegin( r ) ) {
		std::vector<float> mats;
		std::vector<GpuDriven::DrawCmd> cmds;
		std::size_t split = 0;
		for ( int pass = 0; pass < 2; pass++ ) {
			if ( pass == 1 )
				split = cmds.size();
			for ( const Occluder & o : occluders ) {
				GpuDriven::PoolEntry e;
				if ( o.cull != ( pass == 0 ) || !GpuDriven::poolEnsure( r, o.sh, e ) )
					continue;
				const std::uint32_t mi = std::uint32_t( mats.size() / 16 );
				const Matrix4 mv = o.sh->viewTrans().toMatrix4();
				mats.insert( mats.end(), mv.data(), mv.data() + 16 );
				for ( std::size_t k = 0; k + 1 < o.rec->ranges.size(); k += 2 ) {
					if ( o.rec->ranges[k + 1] )
						cmds.push_back( { o.rec->ranges[k + 1] * 3, 1, e.firstIndex + o.rec->ranges[k] * 3, e.baseVertex, mi } );
				}
			}
		}
		g->glBindFramebuffer( GL_DRAW_FRAMEBUFFER, G.fbo );
		g->glViewport( 0, 0, vw, vh );
		g->glDepthMask( GL_TRUE );
		g->glClearDepth( 1.0 );
		g->glClear( GL_DEPTH_BUFFER_BIT );
		auto prog = r->useProgram( "lookdev_csmdepth_q.prog" );
		if ( prog && !cmds.empty() ) {
			if ( const int l = prog->uniLocation( "csmClipFromView" ); l >= 0 )
				g->glUniformMatrix4fv( l, 1, GL_FALSE, &pm[0][0] );
			g->glEnable( GL_DEPTH_TEST );
			g->glDepthFunc( GL_LEQUAL );
			g->glEnable( GL_POLYGON_OFFSET_FILL );
			g->glPolygonOffset( 1.0f, 2.0f );	// BSShape::drawShapes' first pass
			g->glDisable( GL_STENCIL_TEST );
			g->glDisable( GL_BLEND );
			g->glEnable( GL_MULTISAMPLE );
			g->glCullFace( GL_BACK );
			g->glFrontFace( GL_CCW );
			GpuDriven::poolBind( r, mats, cmds );
			if ( split ) {
				g->glEnable( GL_CULL_FACE );
				GpuDriven::poolDraw( r, 0, split );
			}
			if ( cmds.size() > split ) {
				g->glDisable( GL_CULL_FACE );
				GpuDriven::poolDraw( r, split, cmds.size() - split );
			}
			GpuDriven::poolUnbind( r );
		} else {
			occ = false;
		}
		r->stopProgram();
	} else {
		occ = false;
	}
	if ( timed )
		g->glQueryCounter( G.q[1], GL_TIMESTAMP );

	// 2. the Hi-Z: level 0 the farthest sample of each pixel, each level above the farthest of its 2x2
	int levels = 0;
	GLint lvlOff[16] = {}, lvlDim[32] = {};
	if ( occ ) {
		int w = vw, h = vh;
		std::size_t total = 0;
		while ( levels < 16 ) {
			lvlOff[levels] = GLint( total );
			lvlDim[levels * 2] = w;
			lvlDim[levels * 2 + 1] = h;
			total += std::size_t( w ) * std::size_t( h );
			levels++;
			if ( w == 1 && h == 1 )
				break;
			w = ( w + 1 ) / 2;
			h = ( h + 1 ) / 2;
		}
		if ( !G.hiz )
			g->glGenBuffers( 1, &G.hiz );
		if ( total * 4 > G.hizBytes ) {
			G.hizBytes = total * 4;
			g->glBindBuffer( GL_SHADER_STORAGE_BUFFER, G.hiz );
			g->glBufferData( GL_SHADER_STORAGE_BUFFER, GLsizeiptr( G.hizBytes ), nullptr, GL_DYNAMIC_COPY );
			g->glBindBuffer( GL_SHADER_STORAGE_BUFFER, 0 );
		}
		g->glBindBufferBase( GL_SHADER_STORAGE_BUFFER, 0, G.hiz );
		const GLuint p0 = samples > 1 ? G.progHiz0Ms : G.progHiz0;
		g->glUseProgram( p0 );
		g->glActiveTexture( GL_TEXTURE0 );
		g->glBindTexture( samples > 1 ? GL_TEXTURE_2D_MULTISAMPLE : GL_TEXTURE_2D, G.depthTex );
		g->glUniform1i( g->glGetUniformLocation( p0, "depthTex" ), 0 );
		g->glUniform1i( g->glGetUniformLocation( p0, "samples" ), std::max( 1, samples ) );
		g->glUniform2i( g->glGetUniformLocation( p0, "dim" ), vw, vh );
		g->glDispatchCompute( GLuint( ( vw + 7 ) / 8 ), GLuint( ( vh + 7 ) / 8 ), 1 );
		g->glUseProgram( G.progHizN );
		const GLint lSrcOff = g->glGetUniformLocation( G.progHizN, "srcOff" ), lDstOff = g->glGetUniformLocation( G.progHizN, "dstOff" ),
			lSrcDim = g->glGetUniformLocation( G.progHizN, "srcDim" ), lDstDim = g->glGetUniformLocation( G.progHizN, "dstDim" );
		for ( int l = 1; l < levels; l++ ) {
			g->glMemoryBarrier( GL_SHADER_STORAGE_BARRIER_BIT );
			g->glUniform1i( lSrcOff, lvlOff[l - 1] );
			g->glUniform1i( lDstOff, lvlOff[l] );
			g->glUniform2i( lSrcDim, lvlDim[( l - 1 ) * 2], lvlDim[( l - 1 ) * 2 + 1] );
			g->glUniform2i( lDstDim, lvlDim[l * 2], lvlDim[l * 2 + 1] );
			g->glDispatchCompute( GLuint( ( lvlDim[l * 2] + 7 ) / 8 ), GLuint( ( lvlDim[l * 2 + 1] + 7 ) / 8 ), 1 );
		}
		g->glMemoryBarrier( GL_SHADER_STORAGE_BARRIER_BIT );
	}
	if ( timed )
		g->glQueryCounter( G.q[2], GL_TIMESTAMP );

	// 3. the cull: one command per cluster, in the shapes' order
	if ( !items.empty() ) {
		stream( g, G.jobs, G.jobsBytes, jobs.size() * sizeof( JobGpu ), jobs.data() );
		stream( g, G.items, G.itemsBytes, items.size() * sizeof( std::uint32_t ), items.data() );
		stream( g, G.cmds, G.cmdsBytes, ( items.size() / 2 ) * sizeof( GpuDriven::DrawCmd ), nullptr );
		const std::uint32_t zero[8] = {};
		std::size_t censusBytes = G.census ? 32 : 0;
		stream( g, G.census, censusBytes, 32, zero );
		if ( !G.hiz ) {	// bound even when occlusion sits out (never read then)
			g->glGenBuffers( 1, &G.hiz );
			g->glBindBuffer( GL_SHADER_STORAGE_BUFFER, G.hiz );
			g->glBufferData( GL_SHADER_STORAGE_BUFFER, 16, nullptr, GL_DYNAMIC_COPY );
			g->glBindBuffer( GL_SHADER_STORAGE_BUFFER, 0 );
			G.hizBytes = 16;
		}
		g->glBindBufferBase( GL_SHADER_STORAGE_BUFFER, 0, G.hiz );
		g->glBindBufferBase( GL_SHADER_STORAGE_BUFFER, 1, G.meshlets );
		g->glBindBufferBase( GL_SHADER_STORAGE_BUFFER, 2, G.jobs );
		g->glBindBufferBase( GL_SHADER_STORAGE_BUFFER, 3, G.items );
		g->glBindBufferBase( GL_SHADER_STORAGE_BUFFER, 4, G.cmds );
		g->glBindBufferBase( GL_SHADER_STORAGE_BUFFER, 5, G.census );
		const GLuint p = G.progCull;
		g->glUseProgram( p );
		g->glUniformMatrix4fv( g->glGetUniformLocation( p, "proj" ), 1, GL_FALSE, &pm[0][0] );
		float pl[5][4];
		for ( int k = 0; k < 5; k++ ) {
			// left, right, bottom, top, near: row 3 +- row 0, 1, 2 (pm[column][row])
			const int axis = k >> 1;
			const float sg = ( k & 1 ) ? -1.0f : 1.0f;
			float l = 0.0f;
			for ( int c = 0; c < 4; c++ ) {
				pl[k][c] = pm[c][3] + sg * pm[c][axis];
				if ( c < 3 )
					l += pl[k][c] * pl[k][c];
			}
			l = std::sqrt( l );
			for ( int c = 0; c < 4; c++ )
				pl[k][c] = l > 0.0f ? pl[k][c] / l : 0.0f;
			if ( !( l > 0.0f ) )
				pl[k][3] = 1.0f;	// no plane: everything inside
		}
		g->glUniform4fv( g->glGetUniformLocation( p, "planes" ), 5, &pl[0][0] );
		g->glUniform1ui( g->glGetUniformLocation( p, "numItems" ), GLuint( items.size() / 2 ) );
		g->glUniform2i( g->glGetUniformLocation( p, "vpSize" ), vw, vh );
		g->glUniform1i( g->glGetUniformLocation( p, "levels" ), levels );
		g->glUniform1iv( g->glGetUniformLocation( p, "lvlOff" ), 16, lvlOff );
		g->glUniform2iv( g->glGetUniformLocation( p, "lvlDim" ), 16, lvlDim );
		const int mode = ( occ ? 1 : 0 ) | ( sw().cone ? 2 : 0 ) | ( sw().frustum ? 4 : 0 ) | ( sw().hard ? 8 : 0 )
			| ( sw().nocull ? 16 : 0 ) | ( GpuDriven::census() ? 32 : 0 );
		g->glUniform1i( g->glGetUniformLocation( p, "mode" ), mode );
		// a few float steps at depth 1 plus the 24-bit step: the prepass and the frame may round apart
		g->glUniform1f( g->glGetUniformLocation( p, "depthEps" ), 4e-6f );
		g->glDispatchCompute( GLuint( ( items.size() / 2 + 63 ) / 64 ), 1, 1 );
		g->glMemoryBarrier( GL_COMMAND_BARRIER_BIT | GL_SHADER_STORAGE_BARRIER_BIT | GL_BUFFER_UPDATE_BARRIER_BIT );
		for ( GLuint b = 0; b < 6; b++ )
			g->glBindBufferBase( GL_SHADER_STORAGE_BUFFER, b, 0 );
		g->glUseProgram( 0 );
		if ( GpuDriven::census() ) {
			g->glBindBuffer( GL_SHADER_STORAGE_BUFFER, G.census );
			g->glGetBufferSubData( GL_SHADER_STORAGE_BUFFER, 0, 32, f.counts );
			g->glBindBuffer( GL_SHADER_STORAGE_BUFFER, 0 );
		}
	} else {
		std::fill( std::begin( f.counts ), std::end( f.counts ), 0u );
	}
	if ( timed ) {
		g->glQueryCounter( G.q[3], GL_TIMESTAMP );
		G.qPending = true;
	}
	r->stopProgram();
	restore( g, S );

	// clusters of shapes gone for 600 frames leave the arena; it is rebuilt once they are most of it
	if ( ( G.frame % 600 ) == 0 ) {
		for ( auto it = G.clusters.begin(); it != G.clusters.end(); ) {
			if ( G.frame - it->lastFrame > 600 ) {
				G.mGarbage += it->ok ? it->data.size() : 0;
				it = G.clusters.erase( it );
			} else {
				++it;
			}
		}
		if ( G.mGarbage > ( std::size_t( 1 ) << 14 ) && G.mGarbage * 2 > G.mUsed )
			arenaRebuild( g, G, 0 );
	}
	f.why = occ ? QStringLiteral( "on" ) : QStringLiteral( "on, no occlusion this frame" );
	if ( GpuDriven::census() )
		f.cpuMs = double( cpu.nsecsElapsed() ) * 1e-6;
}

void learn( Shape * sh, const void * program )
{
	Frame & f = fr();
	if ( !f.flushing || !program || wwCellAoInPass() || wwCellSsrInPass() || !f.noted.contains( sh ) )
		return;
	Learned & L = f.learned[sh];
	const std::uint32_t last = L.seen;
	L.seen = f.frame;
	if ( L.program == program && f.frame - L.frame < 16 && f.frame - last <= 2 )
		return;	// the state is read again every 16 frames, and at once when the program changes
	Gpu * gp = gpuOf();
	if ( !gp )
		return;
	G43 * g = gp->g;
	L.program = program;
	L.frame = f.frame;
	L.vertexOk = vertexProgram( static_cast<const NifSkopeOpenGLContext::Program *>( program )->name, L.occluderProgram );
	GLint depthFunc = 0, cullMode = 0, frontFace = 0, sf = 0, sbf = 0, ops[4] = {};
	GLboolean depthMask = GL_FALSE;
	GLfloat of = 0.0f, ou = 0.0f;
	g->glGetIntegerv( GL_DEPTH_FUNC, &depthFunc );
	g->glGetBooleanv( GL_DEPTH_WRITEMASK, &depthMask );
	g->glGetIntegerv( GL_CULL_FACE_MODE, &cullMode );
	g->glGetIntegerv( GL_FRONT_FACE, &frontFace );
	g->glGetFloatv( GL_POLYGON_OFFSET_FACTOR, &of );
	g->glGetFloatv( GL_POLYGON_OFFSET_UNITS, &ou );
	const bool stencil = g->glIsEnabled( GL_STENCIL_TEST );
	if ( stencil ) {
		g->glGetIntegerv( GL_STENCIL_FUNC, &sf );
		g->glGetIntegerv( GL_STENCIL_BACK_FUNC, &sbf );
		g->glGetIntegerv( GL_STENCIL_FAIL, &ops[0] );
		g->glGetIntegerv( GL_STENCIL_PASS_DEPTH_FAIL, &ops[1] );
		g->glGetIntegerv( GL_STENCIL_BACK_FAIL, &ops[2] );
		g->glGetIntegerv( GL_STENCIL_BACK_PASS_DEPTH_FAIL, &ops[3] );
	}
	const bool keepOnFail = !stencil || ( ops[0] == GL_KEEP && ops[1] == GL_KEEP && ops[2] == GL_KEEP && ops[3] == GL_KEEP );
	const bool blend = g->glIsEnabled( GL_BLEND ), depthTest = g->glIsEnabled( GL_DEPTH_TEST );
	const bool offFill = g->glIsEnabled( GL_POLYGON_OFFSET_FILL );
	// the offset only pushes away (never in front of the sphere's nearest depth)
	L.candidate = !blend && depthTest && ( depthFunc == GL_LEQUAL || depthFunc == GL_LESS ) && keepOnFail
		&& ( !offFill || ( of >= 0.0f && ou >= 0.0f ) ) && !g->glIsEnabled( GL_DEPTH_CLAMP );
	L.cullOn = g->glIsEnabled( GL_CULL_FACE );
	L.cullBack = cullMode == GL_BACK;
	L.ccw = frontFace == GL_CCW;
	L.opaque = L.candidate && depthMask && depthFunc == GL_LEQUAL && offFill && of == 1.0f && ou == 2.0f && g->glIsEnabled( GL_MULTISAMPLE )
		&& ( !stencil || ( sf == GL_ALWAYS && sbf == GL_ALWAYS ) ) && !g->glIsEnabled( GL_SAMPLE_ALPHA_TO_COVERAGE )
		&& !g->glIsEnabled( GL_SAMPLE_COVERAGE ) && !g->glIsEnabled( GL_SAMPLE_MASK ) && !g->glIsEnabled( GL_RASTERIZER_DISCARD );
}

bool draw( Scene * scene, Shape * sh, qsizetype numTriangles )
{
	Frame & f = fr();
	if ( !f.flushing || wwCellAoInPass() || wwCellSsrInPass() )
		return false;
	auto it = f.recs.find( sh );
	if ( it == f.recs.end() )
		return false;
	const Rec & rec = *it;
	auto fn = scene->renderer->fn;
	Gpu * gp = rec.indirect ? gpuOf() : nullptr;
	if ( gp && qsizetype( rec.numTris ) == numTriangles ) {
		gp->g->glBindBuffer( GL_DRAW_INDIRECT_BUFFER, gp->cmds );
		gp->g->glMultiDrawElementsIndirect( GL_TRIANGLES, GL_UNSIGNED_SHORT,
			(const void *) ( std::size_t( rec.cmdFirst ) * sizeof( GpuDriven::DrawCmd ) ), GLsizei( rec.cmdCount ),
			GLsizei( sizeof( GpuDriven::DrawCmd ) ) );
		gp->g->glBindBuffer( GL_DRAW_INDIRECT_BUFFER, 0 );
		PassTime::countDraw( 1 );
		f.mdi++;
		f.mdiCmds += rec.cmdCount;
		return true;
	}
	// the CPU cull's ranges, as BSShape::drawShapes draws them (the cull was asked once, in prepare)
	if ( rec.whole ) {
		fn->glDrawElements( GL_TRIANGLES, GLsizei( std::min< qsizetype >( numTriangles, rec.numTris ) * 3 ), GL_UNSIGNED_SHORT,
			(void *) 0 );
		PassTime::countDraw( 1 );
	} else {
		static std::vector<GLsizei> counts;
		static std::vector<const void *> offsets;
		counts.clear();
		offsets.clear();
		for ( std::size_t k = 0; k + 1 < rec.ranges.size(); k += 2 ) {
			counts.push_back( GLsizei( rec.ranges[k + 1] * 3 ) );
			offsets.push_back( (const void *) ( std::size_t( rec.ranges[k] ) * 6 ) );
		}
		if ( !counts.empty() ) {
			fn->glMultiDrawElements( GL_TRIANGLES, counts.data(), GL_UNSIGNED_SHORT, offsets.data(), GLsizei( counts.size() ) );
			PassTime::countDraw( qint64( counts.size() ) );
		}
	}
	f.rangeDraws++;
	return true;
}

void finish()
{
	Frame & f = fr();
	if ( f.flushing && GpuDriven::census() ) {
		const Gpu * gp = gpus().contains( QOpenGLContext::currentContext() ) ? &gpus()[QOpenGLContext::currentContext()] : nullptr;
		GpuDriven::censusLine( "b", QStringLiteral( "%1 shapes %2 candidates %3 occluders %4 fallback %5 clusters %6 visible %7 "
			"frustum %8 cone %9 occluded %10 tris %11/%12 multidraws %13 commands %14 rangedraws %15" )
			.arg( f.why ).arg( f.shapes ).arg( f.candidates ).arg( f.occluders ).arg( f.fallback ).arg( f.items )
			.arg( f.counts[0] ).arg( f.counts[1] ).arg( f.counts[2] ).arg( f.counts[3] ).arg( f.counts[4] ).arg( f.trisKept )
			.arg( f.mdi ).arg( f.mdiCmds ).arg( f.rangeDraws ) );
		if ( gp )
			GpuDriven::censusLine( "b arena", QStringLiteral( "shapes %1 clusters %2/%3 garbage %4 builds %5 rebuilds %6%7" )
				.arg( gp->clusters.size() ).arg( gp->mUsed ).arg( gp->mCap ).arg( gp->mGarbage ).arg( gp->builds )
				.arg( gp->compactions ).arg( sw().hard ? QStringLiteral( " RED hard" ) : sw().nocull ? QStringLiteral( " RED nocull" ) : QString() ) );
		std::fprintf( stderr, "gpudriven b time: cpu %.3f ms gpu prepass %.3f hiz %.3f cull %.3f ms\n", f.cpuMs, f.gpuMs[0],
			f.gpuMs[1], f.gpuMs[2] );
	}
	f.flushing = false;
	f.notes.clear();
	f.noted.clear();
	f.recs.clear();
}

}
