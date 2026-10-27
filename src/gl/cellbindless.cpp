#include "gl/cellbindless.h"

#include "gl/cellbindlesspool.h"
#include "gl/celllooks.h"
#include "gl/glshape.h"
#include "wwswitch.h"

#include <QOpenGLContext>
#include <QOpenGLFunctions>
#include <QString>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <unordered_map>
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
};

std::unordered_map<const QOpenGLContext *, Ctx> & ctxs()
{
	static std::unordered_map<const QOpenGLContext *, Ctx> m;
	return m;
}

Ctx & cx()
{
	// lane BINDLESSPERF1: the last context's state without a map lookup (several calls a draw; map nodes never move)
	static const QOpenGLContext * lastQ = nullptr;
	static Ctx * lastC = nullptr;
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
		return v == "sampler" ? 1 : v == "pooloff1" ? 2 : v == "nopool" ? 3 : v == "cellname" ? 4 : v == "slow" ? 5 : 0;
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
	return want && red() == 0 && on() && cx().mdi;
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

void forgetAll()
{
	if ( !anyFrozen )
		return;
	Ctx & c = cx();
	if ( QOpenGLContext * q = QOpenGLContext::currentContext() )
		q->functions()->glFinish();	// the ring's rows in flight name these handles
	for ( auto * m : { &c.handles, &c.redHandles } ) {
		for ( auto & e : *m )
			c.nonResident( e.second );
		m->clear();
	}
	c.frozen.clear();
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

void release( Entry & e )
{
	if ( e.cls )
		e.cls->a.release( e.span );
	e.cls = nullptr;
}
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
		release( e );
		if ( !k.attrMask ) {
			k.attrMask = h->attrMask;
			k.floatsPerVert = floatsOf( h->attrMask );
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

void drawElements( NifSkopeOpenGLContext::GLFunctions * f, GLsizei count, std::uintptr_t off )
{
	if ( geo.on )
		f->glDrawElementsBaseVertex( GL_TRIANGLES, count, GL_UNSIGNED_SHORT, (const void *) ( off + geo.idxOff ), geo.baseVertex );
	else
		f->glDrawElements( GL_TRIANGLES, count, GL_UNSIGNED_SHORT, (const void *) off );
}

void multiDrawElements( NifSkopeOpenGLContext::GLFunctions * f, const GLsizei * counts, const void * const * offs, GLsizei n )
{
	if ( !geo.on ) {
		f->glMultiDrawElements( GL_TRIANGLES, counts, GL_UNSIGNED_SHORT, offs, n );
		return;
	}
	static std::vector<const void *> o;
	static std::vector<GLint> b;
	o.resize( size_t( n ) );
	b.assign( size_t( n ), geo.baseVertex );
	for ( GLsizei i = 0; i < n; i++ )
		o[size_t( i )] = (const void *) ( std::uintptr_t( offs[i] ) + geo.idxOff );
	f->glMultiDrawElementsBaseVertex( GL_TRIANGLES, const_cast<GLsizei *>( counts ), GL_UNSIGNED_SHORT,
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
	c.frameNo++;
	static quint64 lineGen = 0;	// lane BINDLESS2FIX: a live set says the line again (the check row repeated the on row's)
	if ( wwSwitchMoved( lineGen ) )
		c.line.clear();
	if ( ( c.frameNo % 64 ) == 0 ) {	// shapes not drawn for 600 frames give their span back
		for ( auto it = c.entries.begin(); it != c.entries.end(); ) {
			if ( c.frameNo - it->second.lastFrame > 600 ) {
				release( it->second );
				if ( c.lastAlloc == it->first )
					c.lastAlloc = nullptr;
				it = c.entries.erase( it );
			} else {
				++it;
			}
		}
	}
	if ( c.draws || c.conflicts || c.fallbacks ) {
		double poolBytes = 0.0;
		for ( auto & k : c.classes )
			poolBytes += double( k.second.a.vertCap ) * k.second.floatsPerVert * 4.0 + double( k.second.a.idxCap ) * 2.0;
		const QString line = QStringLiteral( "bindless: draws=%1 pooled=%2 handles=%3 uploads=%4 classes=%5 pool=%6MB ring=%7MB "
			"waits=%8 conflicts=%9 fallbacks=%10 rebuilds=%11 (conflicts all frames %12)%13" )
			.arg( c.draws ).arg( c.pooled ).arg( c.handles.size() + c.redHandles.size() ).arg( c.uploads ).arg( c.classes.size() )
			.arg( poolBytes / 1048576.0, 0, 'f', 1 ).arg( double( kRows ) * sizeof( Row ) / 1048576.0, 0, 'f', 1 )
			.arg( c.waits ).arg( c.conflicts ).arg( c.fallbacks ).arg( c.rebuilds ).arg( c.conflictsAll )
			.arg( red() == 1 ? QStringLiteral( " RED sampler" ) : red() == 2 ? QStringLiteral( " RED pooloff1" )
				: red() == 3 ? QStringLiteral( " RED nopool" ) : red() == 4 ? QStringLiteral( " RED cellname" )
				: red() == 5 ? QStringLiteral( " RED slow" ) : QString() )
			// lane BINDLESSMDI1: runs = multi-draws of more than one shape, merged = the shapes that joined a leader
			+ ( mdiOn() ? QStringLiteral( " mdi: runs=%1 merged=%2 calls=%3 longest=%4 cmds=%5MB%6" ).arg( c.runs ).arg( c.merged )
				.arg( c.mdiCalls ).arg( c.longest ).arg( double( kCmds ) * kCmdBytes / 1048576.0, 0, 'f', 2 )
				.arg( mdiRed() == 1 ? QStringLiteral( " RED rowoff" ) : QString() ) : QStringLiteral( " mdi: off" ) );
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
