#include "gl/cellbindless.h"

#include "gl/cellbindlesspool.h"
#include "gl/cellbindlessreopen.h"
#include "gl/celllooks.h"
#include "gl/glshape.h"
#include "wwswitch.h"

#include <QObject>
#include <QOpenGLContext>
#include <QOpenGLFunctions>
#include <QString>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <deque>
#include <unordered_map>
#include <utility>
#include <vector>

bool wwFrameLog();	// gl/cellcull.cpp

#ifndef GL_SHADER_STORAGE_BUFFER
#define GL_SHADER_STORAGE_BUFFER 0x90D2
#endif
#ifndef GL_MAP_PERSISTENT_BIT
#define GL_MAP_PERSISTENT_BIT 0x0040
#endif
#ifndef GL_MAP_COHERENT_BIT
#define GL_MAP_COHERENT_BIT 0x0080
#endif
#ifndef GL_DRAW_INDIRECT_BUFFER
#define GL_DRAW_INDIRECT_BUFFER 0x8F3F
#endif

namespace WwBl
{
bool anyFrozen = false;
bool resolving = false;
GLuint resolvedTex = 0;
Geo geo;

namespace
{
typedef std::uint64_t ( APIENTRY * PfnTexHandle )( GLuint );
typedef std::uint64_t ( APIENTRY * PfnTexSamplerHandle )( GLuint, GLuint );
typedef void ( APIENTRY * PfnHandleResident )( std::uint64_t );
typedef void ( APIENTRY * PfnBufferStorage )( GLenum, GLsizeiptr, const void *, GLbitfield );
typedef void ( APIENTRY * PfnMdi )( GLenum, GLenum, const void *, GLsizei, GLsizei );

constexpr std::uint32_t kSegRows = 8192, kSegs = 8, kRows = kSegRows * kSegs;
// lane BINDLESSMDI1: a segment's indirect commands, reused under the same fence as its rows (2 a row: cull ranges)
constexpr std::uint32_t kCmdSeg = kSegRows * 2, kCmds = kCmdSeg * kSegs, kCmdBytes = 20;
constexpr bool kMdiDefault = true;

struct Key
{
	int wrapS = 0, wrapT = 0, minFilter = 0;
	float aniso = 0.0f;
	bool operator==( const Key & o ) const
	{
		return wrapS == o.wrapS && wrapT == o.wrapT && minFilter == o.minFilter && aniso == o.aniso;
	}
};

struct Class
{
	std::uint64_t attrMask = 0;
	GLuint vao = 0, ebo = 0, vbo[16] = {};
	WwBlPool::Arena a;
	std::uint32_t floatsPerVert = 0;
};

struct Entry
{
	Class * cls = nullptr;
	WwBlPool::Span span;
	std::uint64_t h0 = 0, h1 = 0, attrMask = 0;
	std::uint32_t numVerts = 0, elementBytes = 0;
	std::uint32_t lastFrame = 0;
	const Shape * prevOwner = nullptr;	// the red's "one draw off"
};

struct Ctx
{
	bool tried = false, ok = false;
	PfnTexHandle texHandle = nullptr;
	PfnTexSamplerHandle texSamplerHandle = nullptr;
	PfnHandleResident resident = nullptr, nonResident = nullptr;
	PfnBufferStorage bufferStorage = nullptr;
	Class * boundCls = nullptr;	// the layout poolBind bound last
	PfnMdi mdi = nullptr;	// lane BINDLESSMDI1: + GL_ARB_shader_draw_parameters (the row's base instance)
	GLuint cmdBuf = 0;
	unsigned char * cmdMap = nullptr;
	std::uint32_t cmdCursor = 0;
	std::unordered_map<GLuint, std::uint64_t> handles, redHandles;
	std::unordered_map<GLuint, Key> frozen;
	GLuint redSampler = 0, black = 0;
	std::uint64_t blackHandle = 0;
	GLuint ring = 0;
	unsigned char * map = nullptr;
	std::uint32_t cursor = 0;
	GLsync fences[kSegs] = {};
	bool bound = false;
	std::unordered_map<std::uint64_t, Class> classes;
	std::unordered_map<const Shape *, Entry> entries;
	const Shape * lastAlloc = nullptr;
	std::uint32_t frameNo = 1;
	// counters
	long long draws = 0, pooled = 0, uploads = 0, waits = 0, conflicts = 0, fallbacks = 0, rebuilds = 0;
	long long conflictsAll = 0;
	long long runs = 0, merged = 0, mdiCalls = 0, longest = 0;	// lane BINDLESSMDI1
	QString line;
	/* lane BINDREOPEN1: the functions of the last draw (the frame sweep's fences and shrinks have no caller's), one
	 * fence per frame that released pool spans (a grave's range is reused once its frame's fence passed), counters */
	NifSkopeOpenGLContext::GLFunctions * fn = nullptr;
	std::deque<std::pair<std::uint64_t, GLsync>> frameFences;
	std::uint64_t doneEpoch = 0;
	long long shrinks = 0, reopens = 0;
};

std::unordered_map<const QOpenGLContext *, Ctx> & ctxs()
{
	static std::unordered_map<const QOpenGLContext *, Ctx> m;
	return m;
}

// lane BINDREOPEN1: at namespace scope, so a destroyed context's Ctx can be forgotten (ctxGone)
const QOpenGLContext * lastQ = nullptr;
Ctx * lastC = nullptr;
void ctxGone( const QOpenGLContext * q );

Ctx & cx()
{
	// lane BINDLESSPERF1: the last context's state without a map lookup (several calls a draw; map nodes never move)
	const QOpenGLContext * q = QOpenGLContext::currentContext();
	if ( !lastC || q != lastQ ) {
		lastC = &ctxs()[q];
		lastQ = q;
	}
	return *lastC;
}

Row row;
Key lastKey;
bool lastKeyValid = false;

bool init( Ctx & c )
{
	if ( c.tried )
		return c.ok;
	c.tried = true;
	QOpenGLContext * q = QOpenGLContext::currentContext();
	if ( !q || !q->hasExtension( "GL_ARB_bindless_texture" ) || !q->hasExtension( "GL_ARB_shader_storage_buffer_object" )
		|| !q->hasExtension( "GL_ARB_buffer_storage" ) ) {
		std::fprintf( stderr, "bindless: unsupported (GL_ARB_bindless_texture / shader_storage_buffer_object / buffer_storage)\n" );
		return false;
	}
	c.texHandle = reinterpret_cast<PfnTexHandle>( q->getProcAddress( "glGetTextureHandleARB" ) );
	c.texSamplerHandle = reinterpret_cast<PfnTexSamplerHandle>( q->getProcAddress( "glGetTextureSamplerHandleARB" ) );
	c.resident = reinterpret_cast<PfnHandleResident>( q->getProcAddress( "glMakeTextureHandleResidentARB" ) );
	c.nonResident = reinterpret_cast<PfnHandleResident>( q->getProcAddress( "glMakeTextureHandleNonResidentARB" ) );
	c.bufferStorage = reinterpret_cast<PfnBufferStorage>( q->getProcAddress( "glBufferStorage" ) );
	if ( q->hasExtension( "GL_ARB_shader_draw_parameters" ) && q->hasExtension( "GL_ARB_multi_draw_indirect" ) )
		c.mdi = reinterpret_cast<PfnMdi>( q->getProcAddress( "glMultiDrawElementsIndirect" ) );
	c.ok = c.texHandle && c.texSamplerHandle && c.resident && c.nonResident && c.bufferStorage;
	if ( !c.ok )
		std::fprintf( stderr, "bindless: unsupported (entry points missing)\n" );
	// lane BINDREOPEN1: a context's state (its mapped rings, handles, pool) goes with the context, so a new context
	// made at the same address never writes through the dead one's mapped pointers
	if ( c.ok )
		QObject::connect( q, &QOpenGLContext::aboutToBeDestroyed, [q]() { ctxGone( q ); } );
	return c.ok;
}

}

int slotOf( const char * var )
{
	static const char * const names[] = { "BaseMap", "NormalMap", "GlowMap", "BacklightMap", "SpecularMap", "GreyscaleMap",
		"EnvironmentMap" };
	for ( int i = 0; i < 7; i++ )
		if ( std::strcmp( var, names[i] ) == 0 )
			return i;
	return -1;
}

int red()
{
	WW_SWITCH_STATIC( int, r, [] {
		const QByteArray v = wwEnv( "WW_BINDLESS_RED" ).trimmed();
		return v == "sampler" ? 1 : v == "pooloff1" ? 2 : v == "nopool" ? 3 : v == "cellname" ? 4 : v == "slow" ? 5
			: v == "poolgrow" ? 6 : v == "reopenraw" ? 7 : 0;	// lane BINDREOPEN1: 6, 7
	}() );
	return r;
}

bool on()
{
	if ( !wwLookOn( WwLook::BindlessDraws ) )
		return false;
	Ctx & c = cx();
	return init( c );
}

bool fastOn()
{
	const int r = red();
	return r != 3 && r != 5 && on();
}

int mdiRed()
{
	WW_SWITCH_STATIC( int, r, wwEnv( "WW_BINDLESS_MDI_RED" ).trimmed() == "rowoff" ? 1 : 0 );
	return r;
}

bool mdiOn()
{
	// WW_BINDLESS_MDI=0 is the per-draw path (one draw call a shape); no reds of the bindless path batch
	WW_SWITCH_STATIC( bool, want, [] {
		const QByteArray v = wwEnv( "WW_BINDLESS_MDI" ).trimmed();
		return v.isEmpty() ? kMdiDefault : v != "0";
	}() );
	const int r = red();	// lane BINDREOPEN1: the pool and reopen reds keep the multi-draw
	return want && ( r == 0 || r == 6 || r == 7 ) && on() && cx().mdi;
}

bool frozenSkip( GLuint tex, int wrapS, int wrapT, int minFilter, float aniso )
{
	if ( !anyFrozen || !tex )
		return false;
	Ctx & c = cx();
	auto it = c.frozen.find( tex );
	if ( it == c.frozen.end() )
		return false;
	Key k;
	k.wrapS = wrapS;
	k.wrapT = wrapT;
	k.minFilter = minFilter;
	k.aniso = aniso;
	if ( !( it->second == k ) ) {
		c.conflicts++;
		c.conflictsAll++;
	}
	return true;
}

void noteKey( int wrapS, int wrapT, int minFilter, float aniso )
{
	lastKey.wrapS = wrapS;
	lastKey.wrapT = wrapT;
	lastKey.minFilter = minFilter;
	lastKey.aniso = aniso;
	lastKeyValid = true;
}

std::uint64_t handleOfBound( NifSkopeOpenGLContext::GLFunctions * f )
{
	Ctx & c = cx();
	GLint tex = GLint( resolvedTex );	// lane BINDLESSPERF1: the bind just made, no query (the slow red asks)
	if ( !tex || red() == 5 )
		f->glGetIntegerv( GL_TEXTURE_BINDING_2D, &tex );
	const bool r = red() == 1;
	auto & map = r ? c.redHandles : c.handles;
	if ( tex ) {
		auto it = map.find( GLuint( tex ) );
		if ( it != map.end() ) {
			lastKeyValid = false;
			return it->second;
		}
	}
	std::uint64_t h = 0;
	if ( tex ) {
		if ( r ) {
			if ( !c.redSampler ) {	// the red: mirrored, nearest -- the wrong sampler for every map
				f->glGenSamplers( 1, &c.redSampler );
				f->glSamplerParameteri( c.redSampler, GL_TEXTURE_WRAP_S, GL_MIRRORED_REPEAT );
				f->glSamplerParameteri( c.redSampler, GL_TEXTURE_WRAP_T, GL_MIRRORED_REPEAT );
				f->glSamplerParameteri( c.redSampler, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST );
				f->glSamplerParameteri( c.redSampler, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
			}
			h = c.texSamplerHandle( GLuint( tex ), c.redSampler );
		} else {
			h = c.texHandle( GLuint( tex ) );	// the texture's own sampler state: what the bound draw samples with
		}
	}
	if ( !h ) {
		// an incomplete or missing texture samples (0, 0, 0, 1) bound; the same from a 1x1 texel
		c.fallbacks++;
		if ( !c.blackHandle ) {
			const unsigned char px[4] = { 0, 0, 0, 255 };
			GLint prev = 0;
			f->glGetIntegerv( GL_TEXTURE_BINDING_2D, &prev );
			f->glGenTextures( 1, &c.black );
			f->glBindTexture( GL_TEXTURE_2D, c.black );
			f->glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, px );
			f->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
			f->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
			f->glBindTexture( GL_TEXTURE_2D, GLuint( prev ) );
			c.blackHandle = c.texHandle( c.black );
			if ( c.blackHandle )
				c.resident( c.blackHandle );
		}
		lastKeyValid = false;
		return c.blackHandle;
	}
	c.resident( h );
	map[GLuint( tex )] = h;
	if ( !c.frozen.count( GLuint( tex ) ) )
		c.frozen[GLuint( tex )] = lastKeyValid ? lastKey : Key{};
	lastKeyValid = false;
	anyFrozen = true;
	return h;
}

void setSlot( int slot, std::uint64_t handle )
{
	if ( slot >= 0 && slot < SlotCount )
		row.tex[slot] = handle;
}

void setMatrices( const float * n, const float * mv )
{
	std::memcpy( row.mv, mv, 64 );
	for ( int c = 0; c < 3; c++ ) {
		row.nm[c * 4 + 0] = n[c * 3 + 0];
		row.nm[c * 4 + 1] = n[c * 3 + 1];
		row.nm[c * 4 + 2] = n[c * 3 + 2];
		row.nm[c * 4 + 3] = 0.0f;
	}
}

const Row & staged()
{
	return row;
}

void commit( NifSkopeOpenGLContext::GLFunctions * f, NifSkopeOpenGLContext::Program * prog )
{
	Ctx & c = cx();
	if ( !c.ok )
		return;
	c.fn = f;	// lane BINDREOPEN1
	if ( !c.ring ) {
		f->glGenBuffers( 1, &c.ring );
		f->glBindBuffer( GL_SHADER_STORAGE_BUFFER, c.ring );
		const GLbitfield fl = GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT;
		c.bufferStorage( GL_SHADER_STORAGE_BUFFER, GLsizeiptr( kRows ) * GLsizeiptr( sizeof( Row ) ), nullptr, fl );
		c.map = static_cast<unsigned char *>( f->glMapBufferRange( GL_SHADER_STORAGE_BUFFER, 0,
			GLsizeiptr( kRows ) * GLsizeiptr( sizeof( Row ) ), fl ) );
		f->glBindBuffer( GL_SHADER_STORAGE_BUFFER, 0 );
		if ( !c.map ) {
			std::fprintf( stderr, "bindless: the per-draw ring did not map\n" );
			c.ok = false;
			return;
		}
	}
	if ( !c.bound ) {
		f->glBindBufferBase( GL_SHADER_STORAGE_BUFFER, 12, c.ring );
		c.bound = true;
	}
	const std::uint32_t idx = c.cursor;
	if ( idx % kSegRows == 0 ) {
		c.cmdCursor = ( idx / kSegRows ) * kCmdSeg;	// lane BINDLESSMDI1: the segment's commands, freed by the same fence
		// entering a segment: the one before it is all issued (fence it), this one must be done on the GPU
		const std::uint32_t seg = idx / kSegRows, prev = ( seg + kSegs - 1 ) % kSegs;
		if ( c.fences[prev] )
			f->glDeleteSync( c.fences[prev] );
		c.fences[prev] = f->glFenceSync( GL_SYNC_GPU_COMMANDS_COMPLETE, 0 );
		if ( c.fences[seg] ) {
			GLenum r = f->glClientWaitSync( c.fences[seg], GL_SYNC_FLUSH_COMMANDS_BIT, 0 );
			if ( r == GL_TIMEOUT_EXPIRED ) {
				c.waits++;
				f->glClientWaitSync( c.fences[seg], GL_SYNC_FLUSH_COMMANDS_BIT, 1000000000ULL );
			}
			f->glDeleteSync( c.fences[seg] );
			c.fences[seg] = nullptr;
		}
	}
	std::memcpy( c.map + size_t( idx ) * sizeof( Row ), &row, sizeof( Row ) );
	f->glUniform1ui( prog->wwDrawIdLoc, GLuint( idx ) );
	c.cursor = ( idx + 1 ) % kRows;
	c.draws++;
}

namespace
{
struct Run
{
	bool open = false;
	std::uint32_t base = 0, cmd0 = 0, cmdEnd = 0, rows = 0;
} run;
}

bool runOpen( NifSkopeOpenGLContext::GLFunctions * f, std::uint32_t leaderCmds )
{
	run.open = false;
	Ctx & c = cx();
	if ( !geo.on || !c.ok || !c.ring || !c.mdi )
		return false;
	if ( !c.cmdBuf ) {
		f->glGenBuffers( 1, &c.cmdBuf );
		f->glBindBuffer( GL_DRAW_INDIRECT_BUFFER, c.cmdBuf );
		const GLbitfield fl = GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT;
		c.bufferStorage( GL_DRAW_INDIRECT_BUFFER, GLsizeiptr( kCmds ) * kCmdBytes, nullptr, fl );
		c.cmdMap = static_cast<unsigned char *>( f->glMapBufferRange( GL_DRAW_INDIRECT_BUFFER, 0,
			GLsizeiptr( kCmds ) * kCmdBytes, fl ) );
		f->glBindBuffer( GL_DRAW_INDIRECT_BUFFER, 0 );
		if ( !c.cmdMap ) {
			std::fprintf( stderr, "bindless: the indirect command ring did not map\n" );
			c.mdi = nullptr;
			return false;
		}
	}
	run.base = ( c.cursor + kRows - 1 ) % kRows;	// the leader's row (commit wrote it)
	run.cmdEnd = ( run.base / kSegRows + 1 ) * kCmdSeg;
	run.cmd0 = c.cmdCursor;
	run.rows = 1;
	if ( run.cmd0 < run.cmdEnd - kCmdSeg || run.cmd0 + leaderCmds > run.cmdEnd )
		return false;
	run.open = true;
	return true;
}

bool runRoom( std::uint32_t cmds )
{
	return run.open && cx().cmdCursor + cmds <= run.cmdEnd && cx().cursor % kSegRows != 0;
}

void runCmd( GLsizei count, std::uintptr_t offBytes, std::uintptr_t idxOff, std::int32_t baseVertex, std::uint32_t rel )
{
	Ctx & c = cx();
	const std::uint32_t w[5] = { std::uint32_t( count ), 1u, std::uint32_t( ( offBytes + idxOff ) / 2 ),
		std::uint32_t( baseVertex ), rel };
	std::memcpy( c.cmdMap + std::size_t( c.cmdCursor ) * kCmdBytes, w, kCmdBytes );
	c.cmdCursor++;
}

std::uint32_t runRow()
{
	Ctx & c = cx();
	const std::uint32_t idx = c.cursor;
	std::memcpy( c.map + size_t( idx ) * sizeof( Row ), &row, sizeof( Row ) );
	c.cursor = ( idx + 1 ) % kRows;
	c.draws++;
	c.pooled++;
	run.rows++;
	const std::uint32_t rel = idx - run.base;
	return mdiRed() == 1 ? rel - 1 : rel;	// red rowoff: a follower reads the row before its own
}

bool runPoolPeek( const Shape * sh, std::uintptr_t & idxOff, std::int32_t & baseVertex )
{
	Ctx & c = cx();
	const auto it = c.entries.find( sh );
	if ( it == c.entries.end() || !it->second.cls || !geo.on )
		return false;
	Entry & e = it->second;
	const NifSkopeOpenGLContext::ShapeDataHash * h = sh->wwDataHash( nullptr );
	if ( !h || e.h0 != h->h[0] || e.h1 != h->h[1] || e.attrMask != h->attrMask || e.numVerts != h->numVerts
		|| e.elementBytes != h->elementBytes || e.cls != c.boundCls )
		return false;
	e.lastFrame = c.frameNo;
	WwBlPool::drawArgs( e.span, nullptr, idxOff, baseVertex );
	return true;
}

bool runIssue( NifSkopeOpenGLContext::GLFunctions * f )
{
	Ctx & c = cx();
	if ( !run.open )
		return false;
	run.open = false;
	if ( run.rows < 2 ) {
		c.cmdCursor = run.cmd0;	// the leader alone draws as before
		return false;
	}
	const GLsizei n = GLsizei( c.cmdCursor - run.cmd0 );
	if ( n > 0 ) {
		f->glBindBuffer( GL_DRAW_INDIRECT_BUFFER, c.cmdBuf );
		c.mdi( GL_TRIANGLES, GL_UNSIGNED_SHORT, (const void *) ( std::size_t( run.cmd0 ) * kCmdBytes ), n, GLsizei( kCmdBytes ) );
		f->glBindBuffer( GL_DRAW_INDIRECT_BUFFER, 0 );
		c.mdiCalls++;
	}
	c.runs++;
	c.merged += run.rows - 1;
	c.longest = std::max<long long>( c.longest, run.rows );
	return true;
}

void forget( GLuint tex )
{
	if ( !anyFrozen )
		return;
	Ctx & c = cx();
	for ( auto * m : { &c.handles, &c.redHandles } ) {
		auto it = m->find( tex );
		if ( it != m->end() ) {
			c.nonResident( it->second );
			m->erase( it );
		}
	}
	c.frozen.erase( tex );
}

// ---- the geometry pool (step 3) ----

namespace
{
std::uint32_t floatsOf( std::uint64_t m )
{
	std::uint32_t n = 0;
	for ( ; m; m >>= 4 )
		n += std::uint32_t( m & 15 );
	return n;
}

void setupVao( NifSkopeOpenGLContext::GLFunctions * f, Class & k )
{
	f->glBindVertexArray( k.vao );
	std::uint64_t m = k.attrMask;
	for ( GLuint i = 0; m; i++, m >>= 4 ) {
		const GLint n = GLint( m & 15 );
		if ( !n )
			continue;
		f->glBindBuffer( GL_ARRAY_BUFFER, k.vbo[i] );
		f->glVertexAttribPointer( i, n, GL_FLOAT, GL_FALSE, 0, nullptr );
		f->glEnableVertexAttribArray( i );
	}
	f->glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, k.ebo );
}

//! a bigger (packed) arena for `k`, its live spans copied over on the GPU
void rebuild( Ctx & c, NifSkopeOpenGLContext::GLFunctions * f, Class & k, std::uint32_t verts, std::uint32_t idx )
{
	std::uint32_t nv = 0, ni = 0;
	k.a.plan( verts, idx, nv, ni );
	std::vector<Entry *> live;
	for ( auto & e : c.entries )
		if ( e.second.cls == &k )
			live.push_back( &e.second );
	std::sort( live.begin(), live.end(), []( const Entry * x, const Entry * y ) { return x->span.firstVert < y->span.firstVert; } );
	std::vector<WwBlPool::Span> spans, old;
	for ( Entry * e : live )
		spans.push_back( e->span );
	old = spans;
	WwBlPool::pack( spans, k.a, nv, ni );
	GLuint nvbo[16] = {}, nebo = 0;
	std::uint64_t m = k.attrMask;
	for ( int i = 0; m; i++, m >>= 4 ) {
		const std::uint32_t n = std::uint32_t( m & 15 );
		if ( !n )
			continue;
		f->glGenBuffers( 1, &nvbo[i] );
		f->glBindBuffer( GL_COPY_WRITE_BUFFER, nvbo[i] );
		f->glBufferData( GL_COPY_WRITE_BUFFER, GLsizeiptr( nv ) * n * 4, nullptr, GL_STATIC_DRAW );
		if ( k.vbo[i] ) {
			f->glBindBuffer( GL_COPY_READ_BUFFER, k.vbo[i] );
			for ( size_t j = 0; j < live.size(); j++ )
				f->glCopyBufferSubData( GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER, GLintptr( old[j].firstVert ) * n * 4,
					GLintptr( spans[j].firstVert ) * n * 4, GLsizeiptr( old[j].numVerts ) * n * 4 );
			f->glDeleteBuffers( 1, &k.vbo[i] );
		}
		k.vbo[i] = nvbo[i];
	}
	f->glGenBuffers( 1, &nebo );
	f->glBindBuffer( GL_COPY_WRITE_BUFFER, nebo );
	f->glBufferData( GL_COPY_WRITE_BUFFER, GLsizeiptr( ni ) * 2, nullptr, GL_STATIC_DRAW );
	if ( k.ebo ) {
		f->glBindBuffer( GL_COPY_READ_BUFFER, k.ebo );
		for ( size_t j = 0; j < live.size(); j++ )
			f->glCopyBufferSubData( GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER, GLintptr( old[j].firstIndex ) * 2,
				GLintptr( spans[j].firstIndex ) * 2, GLsizeiptr( old[j].numIndices ) * 2 );
		f->glDeleteBuffers( 1, &k.ebo );
	}
	k.ebo = nebo;
	f->glBindBuffer( GL_COPY_READ_BUFFER, 0 );
	f->glBindBuffer( GL_COPY_WRITE_BUFFER, 0 );
	for ( size_t j = 0; j < live.size(); j++ )
		live[j]->span = spans[j];
	if ( !k.vao )
		f->glGenVertexArrays( 1, &k.vao );
	setupVao( f, k );
	c.rebuilds++;
}

//! lane BINDREOPEN1: `epoch` = the last frame whose draws may read the span (its fence frees the range for reuse)
void release( Entry & e, std::uint64_t epoch )
{
	if ( e.cls )
		e.cls->a.release( e.span, epoch );
	e.cls = nullptr;
}

//! lane BINDREOPEN1: every arena's buffers and every entry go (the next draws upload again, from the floor)
void poolDrop( Ctx & c, bool gl )
{
	if ( gl && c.fn ) {
		c.fn->glBindVertexArray( 0 );
		for ( auto & kv : c.classes ) {
			Class & k = kv.second;
			for ( GLuint & b : k.vbo )
				if ( b )
					c.fn->glDeleteBuffers( 1, &b );
			if ( k.ebo )
				c.fn->glDeleteBuffers( 1, &k.ebo );
			if ( k.vao )
				c.fn->glDeleteVertexArrays( 1, &k.vao );
		}
	}
	c.classes.clear();
	c.entries.clear();
	c.lastAlloc = nullptr;
	c.boundCls = nullptr;
	geo.on = false;
}

double poolMB( const Ctx & c, double * liveMB = nullptr )
{
	double cap = 0.0, live = 0.0;
	for ( auto & k : c.classes ) {
		const double fpv = double( k.second.floatsPerVert ) * 4.0;
		cap += double( k.second.a.vertCap ) * fpv + double( k.second.a.idxCap ) * 2.0;
		live += double( k.second.a.liveVerts() ) * fpv + double( k.second.a.liveIdx() ) * 2.0;
	}
	if ( liveMB )
		*liveMB = live / 1048576.0;
	return cap / 1048576.0;
}

//! lane BINDREOPEN1: the teardown steps of src/gl/cellbindlessreopen.h on the real GL (`gl` false: no GL call is made)
struct GlSteps
{
	Ctx & c;
	bool gl;
	QOpenGLFunctions * qf;

	void finish()
	{
		if ( gl && qf )
			qf->glFinish();
	}
	void fenceWait()
	{
		if ( !gl )
			return;
		if ( !c.fn ) {	// nothing of ours was ever issued in this context
			finish();
			return;
		}
		GLsync s = c.fn->glFenceSync( GL_SYNC_GPU_COMMANDS_COMPLETE, 0 );
		for ( int k = 0; k < 30 && c.fn->glClientWaitSync( s, GL_SYNC_FLUSH_COMMANDS_BIT, 1000000000ULL ) == GL_TIMEOUT_EXPIRED; k++ ) {}
		c.fn->glDeleteSync( s );
	}
	void ringReset()
	{
		for ( GLsync & s : c.fences ) {
			if ( s && gl && c.fn )
				c.fn->glDeleteSync( s );
			s = nullptr;
		}
		for ( auto & ff : c.frameFences )
			if ( gl && c.fn )
				c.fn->glDeleteSync( ff.second );
		c.frameFences.clear();
		c.doneEpoch = c.frameNo;
		c.cursor = 0;
		c.cmdCursor = 0;
		c.bound = false;
		run.open = false;
	}
	void rowZero()
	{
		std::memset( &row, 0, sizeof( row ) );
		lastKeyValid = false;
		resolvedTex = 0;
	}
	void handlesNonResident()
	{
		for ( auto * m : { &c.handles, &c.redHandles } ) {
			if ( gl )
				for ( auto & e : *m )
					c.nonResident( e.second );
			m->clear();
		}
	}
	void frozenClear()
	{
		c.frozen.clear();
	}
	void poolDrop()
	{
		WwBl::poolDrop( c, gl );
	}
	void unmapRings()
	{
		if ( gl && c.fn && c.map ) {
			c.fn->glBindBuffer( GL_SHADER_STORAGE_BUFFER, c.ring );
			c.fn->glUnmapBuffer( GL_SHADER_STORAGE_BUFFER );
			c.fn->glBindBuffer( GL_SHADER_STORAGE_BUFFER, 0 );
		}
		if ( gl && c.fn && c.cmdMap ) {
			c.fn->glBindBuffer( GL_DRAW_INDIRECT_BUFFER, c.cmdBuf );
			c.fn->glUnmapBuffer( GL_DRAW_INDIRECT_BUFFER );
			c.fn->glBindBuffer( GL_DRAW_INDIRECT_BUFFER, 0 );
		}
		c.map = nullptr;
		c.cmdMap = nullptr;
	}
	void deleteBuffers()
	{
		if ( gl && c.fn ) {
			if ( c.ring )
				c.fn->glDeleteBuffers( 1, &c.ring );
			if ( c.cmdBuf )
				c.fn->glDeleteBuffers( 1, &c.cmdBuf );
			if ( c.blackHandle )
				c.nonResident( c.blackHandle );
			if ( c.black )
				c.fn->glDeleteTextures( 1, &c.black );
			if ( c.redSampler )
				c.fn->glDeleteSamplers( 1, &c.redSampler );
		}
		ringReset();
		WwBl::poolDrop( c, gl );
	}
	void forgetCtx()
	{
		for ( auto it = ctxs().begin(); it != ctxs().end(); ++it ) {
			if ( &it->second == &c ) {
				ctxs().erase( it );
				break;
			}
		}
		lastQ = nullptr;
		lastC = nullptr;
	}
};

void ctxGone( const QOpenGLContext * q )
{
	auto it = ctxs().find( q );
	if ( it == ctxs().end() )
		return;
	// GL calls only while the dying context is current; otherwise the driver frees its objects with it
	const bool gl = QOpenGLContext::currentContext() == q;
	GlSteps g{ it->second, gl, gl ? QOpenGLContext::currentContext()->functions() : nullptr };
	if ( red() == 7 ) {	// the red keeps the dead context's state (and its mapped pointers)
		WwBlReopen::contextGone( g, true );
		return;
	}
	WwBlReopen::contextGone( g, false );
	std::fprintf( stderr, "bindless: context gone, its rings unmapped and its state forgotten\n" );
}
}

void forgetAll()
{
	Ctx & c = cx();
	const bool raw = red() == 7;
	if ( raw && !anyFrozen )
		return;	// the red: as before the lane
	if ( !c.ok )
		return;
	/* lane BINDREOPEN1: the reopen census, printed before the teardown (and so before the first frame's re-specify that
	 * D1 crashed in): handles still resident here, the pool, the mapped rings, handles held by OTHER contexts (a flush
	 * made while another context is current releases none of them) */
	std::size_t otherHandles = 0, otherCtx = 0;
	for ( auto & kv : ctxs() )
		if ( &kv.second != &c && ( kv.second.handles.size() + kv.second.redHandles.size() ) ) {
			otherCtx++;
			otherHandles += kv.second.handles.size() + kv.second.redHandles.size();
		}
	double liveMB = 0.0;
	const double capMB = poolMB( c, &liveMB );
	std::fprintf( stderr, "bindless reopen: handles resident %zu, pool MB %.1f (live %.1f, entries %zu), mapped %d, "
		"other contexts %zu holding %zu handles, order %s\n",
		c.handles.size() + c.redHandles.size(), capMB, liveMB, c.entries.size(), int( c.map != nullptr ) + int( c.cmdMap != nullptr ),
		otherCtx, otherHandles, raw ? "RED reopenraw" : "fenced" );
	std::fflush( stderr );
	QOpenGLContext * q = QOpenGLContext::currentContext();
	GlSteps g{ c, q != nullptr, q ? q->functions() : nullptr };
	WwBlReopen::teardown( g, raw );
	if ( !raw )
		c.reopens++;
}

bool poolBind( const Shape * sh, NifSkopeOpenGLContext::GLFunctions * f, unsigned int ownVao )
{
	geo.on = false;
	geo.ownVao = ownVao;
	if ( red() == 3 )
		return false;
	Ctx & c = cx();
	if ( !c.ok )
		return false;
	c.fn = f;	// lane BINDREOPEN1
	const float * attrs[16] = {};
	const NifSkopeOpenGLContext::ShapeDataHash * h = sh->wwDataHash( attrs );
	if ( !h || ( h->attrMask >> 60 ) != 0 || h->numVerts > 65536 )
		return false;
	Entry & e = c.entries[sh];
	const bool same = e.cls && e.h0 == h->h[0] && e.h1 == h->h[1] && e.attrMask == h->attrMask && e.numVerts == h->numVerts
		&& e.elementBytes == h->elementBytes;
	Class & k = c.classes[h->attrMask];
	if ( !same ) {
		// new or changed: (re)placed and uploaded from the shape's arrays, as ShapeData uploads them (fp32)
		std::uint32_t verts = h->numVerts, idx = h->elementBytes / 2;
		release( e, c.frameNo );
		if ( !k.attrMask ) {
			k.attrMask = h->attrMask;
			k.floatsPerVert = floatsOf( h->attrMask );
			k.a.grow = red() == 6;	// lane BINDREOPEN1: red poolgrow, the append-only arena
		}
		WwBlPool::Span sp;
		if ( !k.a.alloc( verts, idx, sp ) ) {
			rebuild( c, f, k, verts, idx );
			if ( !k.a.alloc( verts, idx, sp ) )
				return false;
		}
		e.cls = &k;
		e.span = sp;
		e.h0 = h->h[0];
		e.h1 = h->h[1];
		e.attrMask = h->attrMask;
		e.numVerts = h->numVerts;
		e.elementBytes = h->elementBytes;
		e.prevOwner = c.lastAlloc;
		c.lastAlloc = sh;
		f->glBindVertexArray( k.vao );
		std::uint64_t m = k.attrMask;
		for ( int i = 0; m; i++, m >>= 4 ) {
			const std::uint32_t n = std::uint32_t( m & 15 );
			if ( !n || !attrs[i] )
				continue;
			f->glBindBuffer( GL_ARRAY_BUFFER, k.vbo[i] );
			f->glBufferSubData( GL_ARRAY_BUFFER, GLintptr( sp.firstVert ) * n * 4, GLsizeiptr( verts ) * n * 4, attrs[i] );
		}
		f->glBufferSubData( GL_ELEMENT_ARRAY_BUFFER, GLintptr( sp.firstIndex ) * 2, GLsizeiptr( idx ) * 2,
			sh->triangles.constData() );
		c.uploads++;
	} else if ( red() != 5 ) {
		f->glBindVertexArray( k.vao );	// lane BINDLESSPERF1: no query (it waits for the driver's thread)
	} else {
		GLint cur = 0;
		f->glGetIntegerv( GL_VERTEX_ARRAY_BINDING, &cur );
		if ( GLuint( cur ) != k.vao )
			f->glBindVertexArray( k.vao );
	}
	e.lastFrame = c.frameNo;
	const WwBlPool::Span * prev = nullptr;
	if ( red() == 2 && e.prevOwner ) {
		auto it = c.entries.find( e.prevOwner );
		if ( it != c.entries.end() && it->second.cls == e.cls )
			prev = &it->second.span;
	}
	WwBlPool::drawArgs( e.span, prev, geo.idxOff, geo.baseVertex );
	c.boundCls = &k;
	geo.on = true;
	c.pooled++;
	return true;
}

void poolEnd( NifSkopeOpenGLContext::GLFunctions * f )
{
	// lane BINDLESSPERF1: the layout's array stays bound; every later draw binds its own before drawing
	if ( geo.on && red() == 5 )
		f->glBindVertexArray( geo.ownVao );
	geo.on = false;
}

void drawElements( NifSkopeOpenGLContext::GLFunctions * f, GLsizei count, std::uintptr_t off, GLenum mode )
{
	if ( geo.on )
		f->glDrawElementsBaseVertex( mode, count, GL_UNSIGNED_SHORT, (const void *) ( off + geo.idxOff ), geo.baseVertex );
	else
		f->glDrawElements( mode, count, GL_UNSIGNED_SHORT, (const void *) off );
}

void multiDrawElements( NifSkopeOpenGLContext::GLFunctions * f, const GLsizei * counts, const void * const * offs, GLsizei n,
	GLenum mode )
{
	if ( !geo.on ) {
		f->glMultiDrawElements( mode, counts, GL_UNSIGNED_SHORT, offs, n );
		return;
	}
	static std::vector<const void *> o;
	static std::vector<GLint> b;
	o.resize( size_t( n ) );
	b.assign( size_t( n ), geo.baseVertex );
	for ( GLsizei i = 0; i < n; i++ )
		o[size_t( i )] = (const void *) ( std::uintptr_t( offs[i] ) + geo.idxOff );
	f->glMultiDrawElementsBaseVertex( mode, const_cast<GLsizei *>( counts ), GL_UNSIGNED_SHORT,
		const_cast<const void * const *>( o.data() ), n, b.data() );
}

void unpoolForIndirect( NifSkopeOpenGLContext::GLFunctions * f )
{
	if ( !geo.on )
		return;
	f->glBindVertexArray( geo.ownVao );
	geo.on = false;
}

void frame()
{
	if ( ctxs().empty() )
		return;
	Ctx & c = cx();
	if ( !c.tried )
		return;
	c.bound = false;
	/* lane BINDREOPEN1: the frame just issued gets a fence when pool spans wait in graves; a passed fence frees the
	 * graves of its frame and before (their ranges join the free lists). A live set of the poolgrow red re-makes the
	 * pool in the new rule. */
	const bool growNow = red() == 6;
	bool modeMoved = false, pending = false;
	for ( auto & k : c.classes ) {
		modeMoved |= k.second.a.grow != growNow;
		pending |= !k.second.a.graves.empty();
	}
	if ( modeMoved )
		poolDrop( c, true );
	if ( c.fn && pending )
		c.frameFences.emplace_back( c.frameNo, c.fn->glFenceSync( GL_SYNC_GPU_COMMANDS_COMPLETE, 0 ) );
	while ( c.fn && !c.frameFences.empty() ) {
		auto & ff = c.frameFences.front();
		GLenum r = c.fn->glClientWaitSync( ff.second, 0, 0 );
		if ( r == GL_TIMEOUT_EXPIRED && c.frameFences.size() > 8 )	// the GPU 8 frames behind: wait for the oldest
			r = c.fn->glClientWaitSync( ff.second, GL_SYNC_FLUSH_COMMANDS_BIT, 1000000000ULL );
		if ( r != GL_ALREADY_SIGNALED && r != GL_CONDITION_SATISFIED )
			break;
		c.doneEpoch = std::max( c.doneEpoch, ff.first );
		c.fn->glDeleteSync( ff.second );
		c.frameFences.pop_front();
	}
	for ( auto & k : c.classes )
		k.second.a.retire( c.doneEpoch );
	c.frameNo++;
	static quint64 lineGen = 0;	// lane BINDLESS2FIX: a live set says the line again (the check row repeated the on row's)
	if ( wwSwitchMoved( lineGen ) )
		c.line.clear();
	if ( ( c.frameNo % 64 ) == 0 ) {	// shapes not drawn for 600 frames give their span back
		for ( auto it = c.entries.begin(); it != c.entries.end(); ) {
			if ( c.frameNo - it->second.lastFrame > 600 ) {
				release( it->second, c.frameNo );
				if ( c.lastAlloc == it->first )
					c.lastAlloc = nullptr;
				it = c.entries.erase( it );
			} else {
				++it;
			}
		}
		// lane BINDREOPEN1: an arena whose live part fell under half is re-made at 1.5x its live part (never the red)
		if ( c.fn ) {
			GLint vao = -1, ab = 0;
			for ( auto & k : c.classes ) {
				if ( !k.second.a.shrinkWanted() )
					continue;
				if ( vao < 0 ) {	// at most once per 64 frames, and only when an arena shrinks
					c.fn->glGetIntegerv( GL_VERTEX_ARRAY_BINDING, &vao );
					c.fn->glGetIntegerv( GL_ARRAY_BUFFER_BINDING, &ab );
				}
				rebuild( c, c.fn, k.second, 0, 0 );
				c.shrinks++;
			}
			if ( vao >= 0 ) {
				c.fn->glBindVertexArray( GLuint( vao ) );
				c.fn->glBindBuffer( GL_ARRAY_BUFFER, GLuint( ab ) );
			}
		}
	}
	if ( c.draws || c.conflicts || c.fallbacks ) {
		double liveMB = 0.0;
		const double poolBytes = poolMB( c, &liveMB ) * 1048576.0;
		const QString line = QStringLiteral( "bindless: draws=%1 pooled=%2 handles=%3 uploads=%4 classes=%5 pool=%6MB ring=%7MB "
			"waits=%8 conflicts=%9 fallbacks=%10 rebuilds=%11 (conflicts all frames %12)%13" )
			.arg( c.draws ).arg( c.pooled ).arg( c.handles.size() + c.redHandles.size() ).arg( c.uploads ).arg( c.classes.size() )
			.arg( poolBytes / 1048576.0, 0, 'f', 1 ).arg( double( kRows ) * sizeof( Row ) / 1048576.0, 0, 'f', 1 )
			.arg( c.waits ).arg( c.conflicts ).arg( c.fallbacks ).arg( c.rebuilds ).arg( c.conflictsAll )
			.arg( red() == 1 ? QStringLiteral( " RED sampler" ) : red() == 2 ? QStringLiteral( " RED pooloff1" )
				: red() == 3 ? QStringLiteral( " RED nopool" ) : red() == 4 ? QStringLiteral( " RED cellname" )
				: red() == 5 ? QStringLiteral( " RED slow" ) : red() == 6 ? QStringLiteral( " RED poolgrow" )
				: red() == 7 ? QStringLiteral( " RED reopenraw" ) : QString() )
			// lane BINDLESSMDI1: runs = multi-draws of more than one shape, merged = the shapes that joined a leader
			+ ( mdiOn() ? QStringLiteral( " mdi: runs=%1 merged=%2 calls=%3 longest=%4 cmds=%5MB%6" ).arg( c.runs ).arg( c.merged )
				.arg( c.mdiCalls ).arg( c.longest ).arg( double( kCmds ) * kCmdBytes / 1048576.0, 0, 'f', 2 )
				.arg( mdiRed() == 1 ? QStringLiteral( " RED rowoff" ) : QString() ) : QStringLiteral( " mdi: off" ) )
			// lane BINDREOPEN1: the pool's live part, its reused spans and shrinks (all frames), graves waiting on a fence
			+ QStringLiteral( " poolLive=%1MB reused=%2 shrinks=%3 graves=%4" ).arg( liveMB, 0, 'f', 1 )
				.arg( [&c] { unsigned long long n = 0; for ( auto & k : c.classes ) n += k.second.a.reused; return n; }() )
				.arg( c.shrinks )
				.arg( [&c] { unsigned long long n = 0; for ( auto & k : c.classes ) n += k.second.a.graves.size(); return n; }() );
		if ( line != c.line && wwFrameLog() ) {
			c.line = line;
			std::fprintf( stderr, "%s\n", line.toLocal8Bit().constData() );
		}
	}
	c.draws = c.pooled = c.uploads = c.waits = c.conflicts = c.fallbacks = c.rebuilds = 0;
	c.runs = c.merged = c.mdiCalls = c.longest = 0;
}
}

bool wwBlCellNameRed()
{
	return WwBl::red() == 4;
}
