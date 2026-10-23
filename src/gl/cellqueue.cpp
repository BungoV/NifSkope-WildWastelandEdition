/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#include "gl/cellqueue.h"
#include "gl/cellgpucull.h"	// lane GPUDRIVEN1 step B
#include "wwswitch.h"	// lane LIVESET1: the switches follow a live set

#include "gl/glcontext.hpp"
#include "gl/glnode.h"
#include "gl/glscene.h"
#include "gl/glshape.h"

#include <QHash>
#include <QOffscreenSurface>
#include <QSurface>
#include <QOpenGLContext>
#include <QSet>
#include <QStringList>
#include <QSurfaceFormat>

#include <algorithm>
#include <cstdio>
#include <random>
#include <utility>

namespace GpuDriven
{

namespace
{

struct Switches
{
	bool on = true, csm = true, main = true, perdraw = false, order = false, census = false;
	Switches()
	{
		on = qEnvironmentVariable( "WW_GPUDRIVEN", QStringLiteral( "1" ) ).trimmed() != QLatin1String( "0" );
		const QStringList reds = qEnvironmentVariable( "WW_GPUDRIVEN_RED" ).split( QLatin1Char( ',' ), Qt::SkipEmptyParts );
		for ( const QString & r : reds ) {
			perdraw = perdraw || r.trimmed() == QLatin1String( "perdraw" );
			order = order || r.trimmed() == QLatin1String( "order" );
		}
		csm = on && !perdraw && qEnvironmentVariable( "WW_GPUDRIVEN_CSM", QStringLiteral( "1" ) ).trimmed() != QLatin1String( "0" );
		main = on && !perdraw && qEnvironmentVariable( "WW_GPUDRIVEN_MAIN", QStringLiteral( "1" ) ).trimmed() != QLatin1String( "0" );
		census = qEnvironmentVariableIntValue( "WW_GPUDRIVEN_CENSUS" ) == 1;
	}
};

const Switches & sw()
{
	// lane LIVESET1: read again after a live set; the pools are built at first use either way
	WW_SWITCH_STATIC( Switches, s, Switches() );
	return s;
}

// a fixed seed per shuffle: the same exe draws the same wrong order every frame
template <typename T> void redShuffle( std::vector<T> & v, unsigned int salt )
{
	std::mt19937 g( 0x6d1u + salt );
	std::shuffle( v.begin(), v.end(), g );
}

} // namespace

bool on() { return sw().on; }
bool csm() { return sw().csm; }
bool mainPass() { return sw().main; }
bool redOrder() { return sw().order; }
bool census() { return sw().census; }

void requestContext( QSurfaceFormat & fmt )
{
#ifndef Q_OS_MACOS
	if ( !on() )
		return;
	// probe once: a window whose context cannot be made never draws, so ask only for what the driver gives
	static int got = -1;
	if ( got < 0 ) {
		QOpenGLContext * prev = QOpenGLContext::currentContext();
		QSurface * prevSurface = prev ? prev->surface() : nullptr;
		QSurfaceFormat want = fmt;
		want.setMajorVersion( 4 );
		want.setMinorVersion( 6 );
		QOffscreenSurface surf;
		surf.setFormat( want );
		surf.create();
		QOpenGLContext c;
		c.setFormat( want );
		const bool ok = surf.isValid() && c.create() && c.makeCurrent( &surf );
		got = ok ? c.format().majorVersion() * 10 + c.format().minorVersion() : 0;
		if ( ok )
			c.doneCurrent();
		if ( prev && prevSurface )
			prev->makeCurrent( prevSurface );
		if ( census() )
			std::fprintf( stderr, "gpudriven context: asked 4.6, probe %d.%d, %s\n", got / 10, got % 10,
				got >= 46 ? "using 4.6" : "kept 4.2" );
	}
	if ( got >= 46 ) {
		fmt.setMajorVersion( 4 );
		fmt.setMinorVersion( 6 );
	}
#else
	Q_UNUSED( fmt );
#endif
}

// ---- the main walks

namespace
{

struct MainQueues
{
	bool armed = false, flushing = false;
	std::vector<Node *> q[QueueCount];
	qint64 sizes[QueueCount] = {};
};

MainQueues & mq()
{
	static MainQueues m;
	return m;
}

} // namespace

bool mainBegin( Scene * scene )
{
	MainQueues & m = mq();
	if ( !mainPass() || !scene || scene->selecting || m.armed || m.flushing )
		return false;
	m.armed = true;
	for ( auto & v : m.q )
		v.clear();
	return true;
}

bool mainArmed()
{
	const MainQueues & m = mq();
	return m.armed && !m.flushing;
}

bool mainTake( Node * shape, int queue )
{
	MainQueues & m = mq();
	if ( !m.armed || m.flushing || queue < 0 || queue >= QueueCount )
		return false;
	m.q[queue].push_back( shape );
	return true;
}

void mainFlush( Scene * scene, NodeList * secondPass )
{
	MainQueues & m = mq();
	if ( !m.armed )
		return;
	m.flushing = true;
	GpuDrivenB::prepare( scene );	// step B: the prepass, the Hi-Z and the cluster cull, before the queues draw
	for ( int k = 0; k < QueueCount; k++ ) {
		if ( redOrder() )
			redShuffle( m.q[k], unsigned( k ) );
		for ( Node * n : m.q[k] )
			n->drawShapes( secondPass );
	}
	GpuDrivenB::finish();
	m.flushing = false;
	m.armed = false;
	if ( census() ) {
		censusLine( "main", QStringLiteral( "opaque %1 two-sided %2 alpha-test %3 rest %4" )
			.arg( m.q[Opaque].size() ).arg( m.q[TwoSided].size() ).arg( m.q[AlphaTest].size() ).arg( m.q[Rest].size() ) );
	}
	Q_UNUSED( scene );
}

// ---- the cascades' pool

namespace
{

using GLFunctions = NifSkopeOpenGLContext::GLFunctions;
using MultiDrawIndirect = void ( QOPENGLF_APIENTRYP )( GLenum mode, GLenum type, const void * indirect, GLsizei drawcount,
	GLsizei stride );

struct Entry
{
	std::uint64_t h0 = 0, h1 = 0;
	std::uint32_t numVerts = 0;
	PoolEntry e;
	std::uint32_t lastFrame = 0;
};

struct Pool
{
	GLuint vao = 0, pos = 0, uv = 0, ebo = 0, mat = 0, ind = 0;
	std::size_t vCap = 0, vUsed = 0, iCap = 0, iUsed = 0;	// vertices / 16-bit indices
	std::size_t vGarbage = 0, iGarbage = 0;
	std::size_t matBytes = 0, indBytes = 0;
	bool vaoDirty = true;
	MultiDrawIndirect mdi = nullptr;
	std::uint32_t frame = 0;
	QHash<const Shape *, Entry> entries;
	qint64 uploads = 0, grows = 0, compactions = 0;
};

QHash<const void *, Pool> & pools()
{
	static QHash<const void *, Pool> p;
	return p;
}

Pool * poolOf()
{
	const void * ctx = QOpenGLContext::currentContext();
	if ( !ctx )
		return nullptr;
	return &pools()[ctx];
}

// a new buffer of newBytes holding the first keepBytes of the old one (copied on the GPU)
void regrow( GLFunctions * fn, GLuint & buf, std::size_t keepBytes, std::size_t newBytes )
{
	GLuint nb = 0;
	fn->glGenBuffers( 1, &nb );
	fn->glBindBuffer( GL_COPY_WRITE_BUFFER, nb );
	fn->glBufferData( GL_COPY_WRITE_BUFFER, GLsizeiptr( newBytes ), nullptr, GL_STATIC_DRAW );
	if ( buf && keepBytes ) {
		fn->glBindBuffer( GL_COPY_READ_BUFFER, buf );
		fn->glCopyBufferSubData( GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER, 0, 0, GLsizeiptr( keepBytes ) );
		fn->glBindBuffer( GL_COPY_READ_BUFFER, 0 );
	}
	fn->glBindBuffer( GL_COPY_WRITE_BUFFER, 0 );
	if ( buf )
		fn->glDeleteBuffers( 1, &buf );
	buf = nb;
}

void upload( GLFunctions * fn, GLuint buf, std::size_t offset, std::size_t bytes, const void * data )
{
	fn->glBindBuffer( GL_COPY_WRITE_BUFFER, buf );
	fn->glBufferSubData( GL_COPY_WRITE_BUFFER, GLintptr( offset ), GLsizeiptr( bytes ), data );
	fn->glBindBuffer( GL_COPY_WRITE_BUFFER, 0 );
}

void ensureCapacity( GLFunctions * fn, Pool & p, std::size_t verts, std::size_t idx )
{
	if ( p.vUsed + verts > p.vCap ) {
		const std::size_t cap = std::max< std::size_t >( { p.vCap * 2, p.vUsed + verts, std::size_t( 1 ) << 18 } );
		regrow( fn, p.pos, p.vUsed * 12, cap * 12 );
		regrow( fn, p.uv, p.vUsed * 8, cap * 8 );
		p.vCap = cap;
		p.vaoDirty = true;
		p.grows++;
	}
	if ( p.iUsed + idx > p.iCap ) {
		const std::size_t cap = std::max< std::size_t >( { p.iCap * 2, p.iUsed + idx, std::size_t( 1 ) << 20 } );
		regrow( fn, p.ebo, p.iUsed * 2, cap * 2 );
		p.iCap = cap;
		p.vaoDirty = true;
		p.grows++;
	}
}

void uploadEntry( GLFunctions * fn, Pool & p, const Shape * sh, const PoolEntry & e )
{
	const std::size_t nv = std::size_t( sh->verts.size() );
	upload( fn, p.pos, std::size_t( e.baseVertex ) * 12, nv * 12, &( sh->verts.constFirst()[0] ) );
	if ( e.uv )
		upload( fn, p.uv, std::size_t( e.baseVertex ) * 8, nv * 8, &( sh->coords.constFirst().constFirst()[0] ) );
	upload( fn, p.ebo, std::size_t( e.firstIndex ) * 2, std::size_t( e.numIndices ) * 2, sh->triangles.constData() );
	p.uploads++;
}

// the live entries moved into fresh buffers, the garbage left behind
void compact( GLFunctions * fn, Pool & p )
{
	std::size_t nv = 0, ni = 0;
	for ( const Entry & x : std::as_const( p.entries ) ) {
		nv += x.numVerts;
		ni += x.e.numIndices;
	}
	const std::size_t vCap = std::max< std::size_t >( nv + nv / 4, std::size_t( 1 ) << 18 );
	const std::size_t iCap = std::max< std::size_t >( ni + ni / 4, std::size_t( 1 ) << 20 );
	GLuint np = 0, nu = 0, ne = 0;
	fn->glGenBuffers( 1, &np );
	fn->glGenBuffers( 1, &nu );
	fn->glGenBuffers( 1, &ne );
	const GLuint src[3] = { p.pos, p.uv, p.ebo }, dst[3] = { np, nu, ne };
	const std::size_t caps[3] = { vCap * 12, vCap * 8, iCap * 2 };
	for ( int b = 0; b < 3; b++ ) {
		fn->glBindBuffer( GL_COPY_WRITE_BUFFER, dst[b] );
		fn->glBufferData( GL_COPY_WRITE_BUFFER, GLsizeiptr( caps[b] ), nullptr, GL_STATIC_DRAW );
	}
	std::size_t v = 0, i = 0;
	for ( Entry & x : p.entries ) {
		const std::size_t sz[3] = { std::size_t( x.numVerts ) * 12, std::size_t( x.numVerts ) * 8, std::size_t( x.e.numIndices ) * 2 };
		const std::size_t from[3] = { std::size_t( x.e.baseVertex ) * 12, std::size_t( x.e.baseVertex ) * 8, std::size_t( x.e.firstIndex ) * 2 };
		const std::size_t to[3] = { v * 12, v * 8, i * 2 };
		for ( int b = 0; b < 3; b++ ) {
			if ( b == 1 && !x.e.uv )
				continue;
			fn->glBindBuffer( GL_COPY_READ_BUFFER, src[b] );
			fn->glBindBuffer( GL_COPY_WRITE_BUFFER, dst[b] );
			fn->glCopyBufferSubData( GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER, GLintptr( from[b] ), GLintptr( to[b] ),
				GLsizeiptr( sz[b] ) );
		}
		x.e.baseVertex = std::int32_t( v );
		x.e.firstIndex = std::uint32_t( i );
		v += x.numVerts;
		i += x.e.numIndices;
	}
	fn->glBindBuffer( GL_COPY_READ_BUFFER, 0 );
	fn->glBindBuffer( GL_COPY_WRITE_BUFFER, 0 );
	GLuint old[3] = { p.pos, p.uv, p.ebo };
	fn->glDeleteBuffers( 3, old );
	p.pos = np;
	p.uv = nu;
	p.ebo = ne;
	p.vCap = vCap;
	p.iCap = iCap;
	p.vUsed = v;
	p.iUsed = i;
	p.vGarbage = p.iGarbage = 0;
	p.vaoDirty = true;
	p.compactions++;
}

void setupVao( GLFunctions * fn, Pool & p )
{
	if ( !p.vao )
		fn->glGenVertexArrays( 1, &p.vao );
	if ( !p.mat )
		fn->glGenBuffers( 1, &p.mat );
	fn->glBindVertexArray( p.vao );
	fn->glBindBuffer( GL_ARRAY_BUFFER, p.pos );
	fn->glEnableVertexAttribArray( 0 );
	fn->glVertexAttribPointer( 0, 3, GL_FLOAT, GL_FALSE, 12, (const void *) 0 );
	fn->glBindBuffer( GL_ARRAY_BUFFER, p.uv );
	fn->glEnableVertexAttribArray( 7 );
	fn->glVertexAttribPointer( 7, 2, GL_FLOAT, GL_FALSE, 8, (const void *) 0 );
	fn->glBindBuffer( GL_ARRAY_BUFFER, p.mat );
	for ( int c = 0; c < 4; c++ ) {
		fn->glEnableVertexAttribArray( GLuint( 8 + c ) );
		fn->glVertexAttribPointer( GLuint( 8 + c ), 4, GL_FLOAT, GL_FALSE, 64, (const void *) ( std::size_t( c ) * 16 ) );
		fn->glVertexAttribDivisor( GLuint( 8 + c ), 1 );
	}
	fn->glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, p.ebo );
	fn->glBindBuffer( GL_ARRAY_BUFFER, 0 );
	p.vaoDirty = false;
}

} // namespace

bool poolBegin( NifSkopeOpenGLContext * r )
{
	Pool * pp = poolOf();
	if ( !pp || !r )
		return false;
	Pool & p = *pp;
	if ( !p.mdi ) {
		auto * c = QOpenGLContext::currentContext();
		p.mdi = reinterpret_cast<MultiDrawIndirect>( c->getProcAddress( "glMultiDrawElementsIndirect" ) );
		if ( census() )
			std::fprintf( stderr, "gpudriven context: GL %d.%d, glMultiDrawElementsIndirect %s\n", c->format().majorVersion(),
				c->format().minorVersion(), p.mdi ? "found" : "missing (one call per command)" );
	}
	p.frame++;
	// shapes not cast for 600 frames leave the pool; their space is reclaimed once it is most of it
	if ( ( p.frame % 600 ) == 0 ) {
		for ( auto it = p.entries.begin(); it != p.entries.end(); ) {
			if ( p.frame - it->lastFrame > 600 ) {
				p.vGarbage += it->numVerts;
				p.iGarbage += it->e.numIndices;
				it = p.entries.erase( it );
			} else {
				++it;
			}
		}
	}
	if ( p.vGarbage > ( std::size_t( 1 ) << 16 ) && p.vGarbage * 2 > p.vUsed )
		compact( r->fn, p );
	return true;
}

bool poolEnsure( NifSkopeOpenGLContext * r, const Shape * sh, PoolEntry & out )
{
	Pool * pp = poolOf();
	if ( !pp || !sh )
		return false;
	Pool & p = *pp;
	const NifSkopeOpenGLContext::ShapeDataHash * h = sh->wwDataHash();
	if ( !h )
		return false;
	const std::uint32_t nv = std::uint32_t( sh->verts.size() );
	const std::uint32_t ni = std::uint32_t( sh->triangles.size() ) * 3;
	const bool uv = !sh->coords.isEmpty() && sh->coords.constFirst().size() == sh->verts.size();
	auto it = p.entries.find( sh );
	if ( it != p.entries.end() ) {
		Entry & x = *it;
		x.lastFrame = p.frame;
		if ( x.h0 == h->h[0] && x.h1 == h->h[1] && x.numVerts == nv && x.e.numIndices == ni && x.e.uv == uv ) {
			out = x.e;
			return true;
		}
		if ( x.numVerts == nv && x.e.numIndices == ni ) {
			// the same sizes: rewritten in place
			x.h0 = h->h[0];
			x.h1 = h->h[1];
			x.e.uv = uv;
			uploadEntry( r->fn, p, sh, x.e );
			out = x.e;
			return true;
		}
		p.vGarbage += x.numVerts;
		p.iGarbage += x.e.numIndices;
		p.entries.erase( it );
	}
	ensureCapacity( r->fn, p, nv, ni );
	Entry x;
	x.h0 = h->h[0];
	x.h1 = h->h[1];
	x.numVerts = nv;
	x.lastFrame = p.frame;
	x.e.baseVertex = std::int32_t( p.vUsed );
	x.e.firstIndex = std::uint32_t( p.iUsed );
	x.e.numIndices = ni;
	x.e.uv = uv;
	p.vUsed += nv;
	p.iUsed += ni;
	uploadEntry( r->fn, p, sh, x.e );
	p.entries.insert( sh, x );
	out = x.e;
	return true;
}

void poolBind( NifSkopeOpenGLContext * r, const std::vector<float> & matrices, const std::vector<DrawCmd> & cmds )
{
	Pool * pp = poolOf();
	if ( !pp )
		return;
	Pool & p = *pp;
	auto fn = r->fn;
	if ( !p.mat )
		fn->glGenBuffers( 1, &p.mat );
	if ( !p.ind )
		fn->glGenBuffers( 1, &p.ind );
	// one upload each per frame; a bigger set re-allocates, a smaller one reuses the store
	const std::size_t mb = std::max< std::size_t >( matrices.size() * sizeof( float ), 64 );
	fn->glBindBuffer( GL_ARRAY_BUFFER, p.mat );
	if ( mb > p.matBytes ) {
		p.matBytes = mb + mb / 2;
		fn->glBufferData( GL_ARRAY_BUFFER, GLsizeiptr( p.matBytes ), nullptr, GL_STREAM_DRAW );
	}
	if ( !matrices.empty() )
		fn->glBufferSubData( GL_ARRAY_BUFFER, 0, GLsizeiptr( matrices.size() * sizeof( float ) ), matrices.data() );
	fn->glBindBuffer( GL_ARRAY_BUFFER, 0 );
	const std::size_t cb = std::max< std::size_t >( cmds.size() * sizeof( DrawCmd ), 64 );
	fn->glBindBuffer( GL_DRAW_INDIRECT_BUFFER, p.ind );
	if ( cb > p.indBytes ) {
		p.indBytes = cb + cb / 2;
		fn->glBufferData( GL_DRAW_INDIRECT_BUFFER, GLsizeiptr( p.indBytes ), nullptr, GL_STREAM_DRAW );
	}
	if ( !cmds.empty() )
		fn->glBufferSubData( GL_DRAW_INDIRECT_BUFFER, 0, GLsizeiptr( cmds.size() * sizeof( DrawCmd ) ), cmds.data() );
	if ( p.vaoDirty )
		setupVao( fn, p );
	else
		fn->glBindVertexArray( p.vao );
}

void poolRebind( NifSkopeOpenGLContext * r )
{
	Pool * pp = poolOf();
	if ( !pp || !pp->vao )
		return;
	r->fn->glBindVertexArray( pp->vao );
	r->fn->glBindBuffer( GL_DRAW_INDIRECT_BUFFER, pp->ind );
}

void poolDraw( NifSkopeOpenGLContext * r, std::size_t first, std::size_t n )
{
	Pool * pp = poolOf();
	if ( !pp || !n )
		return;
	if ( pp->mdi ) {
		pp->mdi( GL_TRIANGLES, GL_UNSIGNED_SHORT, (const void *) ( first * sizeof( DrawCmd ) ), GLsizei( n ),
			GLsizei( sizeof( DrawCmd ) ) );
		return;
	}
	// without the 4.3 call: the same commands one by one (they still read the pool and the matrix buffer)
	static std::vector<DrawCmd> mirror;
	mirror.resize( n );
	r->fn->glGetBufferSubData( GL_DRAW_INDIRECT_BUFFER, GLintptr( first * sizeof( DrawCmd ) ),
		GLsizeiptr( n * sizeof( DrawCmd ) ), mirror.data() );
	for ( const DrawCmd & c : mirror )
		r->fn->glDrawElementsInstancedBaseVertexBaseInstance( GL_TRIANGLES, GLsizei( c.count ), GL_UNSIGNED_SHORT,
			(const void *) ( std::size_t( c.firstIndex ) * 2 ), GLsizei( c.instanceCount ), c.baseVertex, c.baseInstance );
}

void poolUnbind( NifSkopeOpenGLContext * r )
{
	r->fn->glBindVertexArray( 0 );
	r->fn->glBindBuffer( GL_DRAW_INDIRECT_BUFFER, 0 );
}

QString poolCensus( NifSkopeOpenGLContext * r )
{
	Q_UNUSED( r );
	const Pool * p = poolOf();
	if ( !p )
		return QStringLiteral( "pool none" );
	return QStringLiteral( "pool shapes %1 verts %2/%3 idx %4/%5 garbage %6 uploads %7 grows %8 compactions %9" )
		.arg( p->entries.size() ).arg( p->vUsed ).arg( p->vCap ).arg( p->iUsed ).arg( p->iCap ).arg( p->vGarbage )
		.arg( p->uploads ).arg( p->grows ).arg( p->compactions );
}

// ---- census

void samplerCensus( unsigned int texture, int wrapMode )
{
	if ( !census() )
		return;
	static QHash<unsigned int, QSet<int>> seen;
	static qsizetype multi = 0;
	QSet<int> & s = seen[texture];
	const qsizetype before = s.size();
	s.insert( wrapMode );
	if ( s.size() == 2 && before == 1 )
		multi++;
	if ( s.size() != before && ( ( s.size() == 2 && before == 1 ) || ( seen.size() % 256 ) == 0 ) )
		censusLine( "sampler", QStringLiteral( "textures %1 with more than one sampler state %2" ).arg( seen.size() ).arg( multi ) );
}

void censusLine( const char * tag, const QString & line )
{
	static QHash<QByteArray, QString> last;
	QString & l = last[QByteArray( tag )];
	if ( l == line )
		return;
	l = line;
	std::fprintf( stderr, "gpudriven %s: %s\n", tag, qPrintable( line ) );
	std::fflush( stderr );
}

}
