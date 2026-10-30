#include "gl/cellfarinst.h"

#include "cellfarhide.h"
#include "cellmesh.h"
#include "wwswitch.h"
#include "gl/cellpasstime.h"
#include "gl/glscene.h"
#include "gl/renderer.h"

#include <QElapsedTimer>
#include <QHash>
#include <QOpenGLContext>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

namespace
{

using GLFunctions = NifSkopeOpenGLContext::GLFunctions;
using MultiDrawIndirect = void ( QOPENGLF_APIENTRYP )( GLenum mode, GLenum type, const void * indirect, GLsizei drawcount,
	GLsizei stride );

struct Copy
{
	GLuint vao = 0, vbo = 0, ebo = 0, rows = 0, ind = 0;
	qint64 lastUse = 0;
	std::size_t indBytes = 0;
};

struct Ctx
{
	QHash<quint64, Copy> copies;
	MultiDrawIndirect mdi = nullptr;
	bool mdiLooked = false;
	qint64 lastSweep = 0;
	qint64 gpuBytes = 0;
};

QHash<const void *, Ctx> & ctxs()
{
	static QHash<const void *, Ctx> c;
	return c;
}

qint64 nowMs()
{
	static QElapsedTimer t;
	if ( !t.isValid() )
		t.start();
	return t.elapsed();
}

bool redNoCull()
{
	WW_SWITCH_STATIC( bool, on, wwEnv( "WW_CELL_FARINST_RED" ) == "nocull" );
	return on;
}

bool census()
{
	WW_SWITCH_STATIC( bool, on, wwEnvInt( "WW_CELL_FARINST_CENSUS" ) != 0 );
	return on;
}

// the frame's totals; a bucket drawn a second time starts the next frame
struct Tally
{
	QHash<quint64, int> seen;
	qint64 shapes = 0, tiles = 0, tilesAll = 0, cmds = 0, rows = 0, rowsAll = 0, tris = 0, trisAll = 0;
};

void tallyFlush( Tally & t, const Ctx & c )
{
	if ( t.shapes ) {
		static QString last;
		const QString line = QStringLiteral( "farinst: shapes %1, tiles %2 of %3, indirect commands %4, rows drawn %5 of %6, "
			"tris drawn %7 of %8, GPU copy %9 MB" )
			.arg( t.shapes ).arg( t.tiles ).arg( t.tilesAll ).arg( t.cmds ).arg( t.rows ).arg( t.rowsAll ).arg( t.tris )
			.arg( t.trisAll ).arg( double( c.gpuBytes ) / ( 1024.0 * 1024.0 ), 0, 'f', 1 );
		if ( line != last ) {
			last = line;
			std::fprintf( stderr, "%s\n", qPrintable( line ) );
			std::fflush( stderr );
		}
	}
	t = Tally();
}

// one GPU copy a bucket, made on its first draw in this context; buckets unused for 10 s leave
Copy * copyOf( GLFunctions * fn, Ctx & c, const CellMeshInst & d )
{
	const qint64 now = nowMs();
	if ( now - c.lastSweep > 10000 ) {
		c.lastSweep = now;
		for ( auto it = c.copies.begin(); it != c.copies.end(); ) {
			if ( now - it->lastUse > 10000 ) {
				fn->glDeleteVertexArrays( 1, &it->vao );
				GLuint b[4] = { it->vbo, it->ebo, it->rows, it->ind };
				fn->glDeleteBuffers( 4, b );
				c.gpuBytes -= qint64( it->indBytes );
				it = c.copies.erase( it );
			} else {
				++it;
			}
		}
	}
	auto it = c.copies.find( d.id );
	if ( it != c.copies.end() ) {
		it->lastUse = now;
		return &*it;
	}
	if ( d.vbo.isEmpty() || d.ebo.isEmpty() || d.inst.isEmpty() || d.cmds.isEmpty() )
		return nullptr;
	Copy k;
	k.lastUse = now;
	fn->glGenVertexArrays( 1, &k.vao );
	fn->glBindVertexArray( k.vao );
	fn->glGenBuffers( 1, &k.vbo );
	fn->glBindBuffer( GL_ARRAY_BUFFER, k.vbo );
	fn->glBufferData( GL_ARRAY_BUFFER, GLsizeiptr( d.vbo.size() ), d.vbo.constData(), GL_STATIC_DRAW );
	const GLsizei vs = 36;
	fn->glEnableVertexAttribArray( 0 );
	fn->glVertexAttribPointer( 0, 3, GL_FLOAT, GL_FALSE, vs, (const void *) 0 );
	fn->glEnableVertexAttribArray( 1 );
	fn->glVertexAttribPointer( 1, 4, GL_UNSIGNED_BYTE, GL_TRUE, vs, (const void *) 12 );
	for ( int a = 0; a < 3; a++ ) {
		fn->glEnableVertexAttribArray( GLuint( 2 + a ) );
		fn->glVertexAttribPointer( GLuint( 2 + a ), 4, GL_INT_2_10_10_10_REV, GL_TRUE, vs, (const void *) std::size_t( 16 + 4 * a ) );
	}
	fn->glEnableVertexAttribArray( 7 );
	fn->glVertexAttribPointer( 7, 2, GL_FLOAT, GL_FALSE, vs, (const void *) 28 );
	fn->glGenBuffers( 1, &k.rows );
	fn->glBindBuffer( GL_ARRAY_BUFFER, k.rows );
	fn->glBufferData( GL_ARRAY_BUFFER, GLsizeiptr( d.inst.size() ) * 4, d.inst.constData(), GL_STATIC_DRAW );
	for ( int a = 0; a < 4; a++ ) {
		fn->glEnableVertexAttribArray( GLuint( 10 + a ) );
		fn->glVertexAttribPointer( GLuint( 10 + a ), 4, GL_FLOAT, GL_FALSE, 64, (const void *) std::size_t( 16 * a ) );
		fn->glVertexAttribDivisor( GLuint( 10 + a ), 1 );
	}
	fn->glGenBuffers( 1, &k.ebo );
	fn->glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, k.ebo );
	fn->glBufferData( GL_ELEMENT_ARRAY_BUFFER, GLsizeiptr( d.ebo.size() ) * 2, d.ebo.constData(), GL_STATIC_DRAW );
	fn->glBindVertexArray( 0 );
	fn->glBindBuffer( GL_ARRAY_BUFFER, 0 );
	fn->glGenBuffers( 1, &k.ind );
	k.indBytes = std::size_t( d.vbo.size() ) + std::size_t( d.ebo.size() ) * 2 + std::size_t( d.inst.size() ) * 4;
	c.gpuBytes += qint64( k.indBytes );
	return &*c.copies.insert( d.id, k );
}

// the camera's four side planes in view space (gl/cellcull.cpp's Gribb-Hartmann, copied: it is file-local there)
void cameraPlanes( Scene * scene, float pl[4][4] )
{
	const auto & pm = scene->renderer->globalUniforms->projectionMatrix;	// pm[column][row]
	for ( int i = 0; i < 4; i++ ) {
		const int axis = i >> 1;
		const float sg = ( i & 1 ) ? -1.0f : 1.0f;
		float l = 0.0f;
		for ( int c = 0; c < 4; c++ ) {
			pl[i][c] = pm[c][3] + sg * pm[c][axis];
			if ( c < 3 )
				l += pl[i][c] * pl[i][c];
		}
		l = std::sqrt( l );
		if ( l > 0.0f )
			for ( int c = 0; c < 4; c++ )
				pl[i][c] /= l;
	}
}

// the surviving tiles' commands, uploaded and drawn in one call (one call a command without the MDI entry point)
void submit( NifSkopeOpenGLContext * r, Ctx & c, Copy & k, const std::vector<CellMeshInst::Cmd> & cmds )
{
	GLFunctions * fn = r->fn;
	fn->glBindVertexArray( k.vao );
	fn->glBindBuffer( GL_DRAW_INDIRECT_BUFFER, k.ind );
	fn->glBufferData( GL_DRAW_INDIRECT_BUFFER, GLsizeiptr( cmds.size() * sizeof( CellMeshInst::Cmd ) ), cmds.data(),
		GL_STREAM_DRAW );
	if ( c.mdi ) {
		c.mdi( GL_TRIANGLES, GL_UNSIGNED_SHORT, nullptr, GLsizei( cmds.size() ), 0 );
		PassTime::countDraw( 1 );
	} else {
		for ( const CellMeshInst::Cmd & m : cmds )
			fn->glDrawElementsInstancedBaseVertexBaseInstance( GL_TRIANGLES, GLsizei( m.count ), GL_UNSIGNED_SHORT,
				(const void *) ( std::size_t( m.firstIndex ) * 2 ), GLsizei( m.instanceCount ), m.baseVertex, m.baseInstance );
		PassTime::countDraw( qint64( cmds.size() ) );
	}
	fn->glBindBuffer( GL_DRAW_INDIRECT_BUFFER, 0 );
	fn->glBindVertexArray( 0 );
}

Ctx * ctxOf()
{
	QOpenGLContext * q = QOpenGLContext::currentContext();
	if ( !q )
		return nullptr;
	Ctx & c = ctxs()[q];
	if ( !c.mdiLooked ) {
		c.mdiLooked = true;
		c.mdi = reinterpret_cast<MultiDrawIndirect>( q->getProcAddress( "glMultiDrawElementsIndirect" ) );
	}
	return &c;
}

} // namespace

namespace CellFarInst
{

bool draw( NifSkopeOpenGLContext * r, Scene * scene, const CellMeshInst & d, NifSkopeOpenGLContext::Program * prog,
	const Transform & viewTrans )
{
	Ctx * cp = ctxOf();
	if ( !r || !prog || !cp )
		return false;
	Copy * k = copyOf( r->fn, *cp, d );
	if ( !k )
		return false;
	static Tally tally;
	if ( census() && tally.seen.contains( d.id ) )
		tallyFlush( tally, *cp );
	float pl[4][4];
	cameraPlanes( scene, pl );
	const CellFarHide::View vw = CellFarHide::view( d );   // lane FARHOLE2: the rows a loaded near cell left out
	std::vector<CellMeshInst::Cmd> cmds;
	cmds.reserve( std::size_t( vw.cmds->size() ) );
	qint64 tiles = 0, rows = 0, tris = 0;
	for ( const CellMeshInst::Tile & t : *vw.tiles ) {
		if ( !redNoCull() ) {
			const Vector3 v = viewTrans * t.centre;
			const float rv = t.radius * std::fabs( viewTrans.scale );
			bool in = true;
			for ( int i = 0; i < 4 && in; i++ )
				in = pl[i][0] * v[0] + pl[i][1] * v[1] + pl[i][2] * v[2] + pl[i][3] >= -rv;
			if ( !in )
				continue;
		}
		tiles++;
		for ( int j = 0; j < t.cmdCount; j++ ) {
			const CellMeshInst::Cmd & m = ( *vw.cmds )[t.firstCmd + j];
			cmds.push_back( m );
			rows += m.instanceCount;
			tris += qint64( m.instanceCount ) * ( m.count / 3 );
		}
	}
	if ( census() ) {
		tally.seen.insert( d.id, 1 );
		tally.shapes++;
		tally.tiles += tiles;
		tally.tilesAll += d.tiles.size();
		tally.cmds += qint64( cmds.size() );
		tally.rows += rows;
		tally.rowsAll += qint64( d.inst.size() / 16 );
		tally.tris += tris;
		tally.trisAll += d.drawnTris;
	}
	if ( cmds.empty() )
		return true;
	{	// lane SKYSHARDS3: the program the far rings draw with, once a change (a live row reads it)
		static std::string lastProg;
		if ( std::string( prog->name ) != lastProg ) {
			lastProg = std::string( prog->name );
			std::fprintf( stderr, "farinst: far rings drawn with %s%s\n", lastProg.c_str(),
				prog->wwBindless ? " (bindless twin: rows read at the base instance, red WW_CELL_FARINST_RED=blrow)" : "" );
			std::fflush( stderr );
		}
	}
	prog->uni1i( "farInst", 1 );
	submit( r, *cp, *k, cmds );
	prog->uni1i( "farInst", 0 );
	return true;
}

bool bindlessTwinAllowed()
{
	WW_SWITCH_STATIC( bool, red, wwEnv( "WW_CELL_FARINST_RED" ) == "blrow" );
	return red;
}

RowAudit rowAudit( const CellMeshInst & d, bool bindlessProgram, quint32 ringRows )
{
	RowAudit a;
	for ( const CellMeshInst::Cmd & m : d.cmds ) {
		a.cmds++;
		const quint32 off = bindlessProgram ? m.baseInstance : 0u;	// wwDrawIdx - wwDrawBase
		if ( off == 0 )
			continue;
		a.foreign++;
		if ( off >= ringRows )
			a.pastRing++;
		a.maxOffset = std::max( a.maxOffset, off );
	}
	return a;
}

bool drawCascade( NifSkopeOpenGLContext * r, const CellMeshInst & d, NifSkopeOpenGLContext::Program * prog,
	const float m[16] )
{
	Ctx * cp = ctxOf();
	if ( !r || !prog || !cp )
		return false;
	Copy * k = copyOf( r->fn, *cp, d );
	if ( !k )
		return false;
	const CellFarHide::View vw = CellFarHide::view( d );   // lane FARHOLE2
	std::vector<CellMeshInst::Cmd> cmds;
	cmds.reserve( std::size_t( vw.cmds->size() ) );
	for ( const CellMeshInst::Tile & t : *vw.tiles ) {
		if ( !redNoCull() ) {
			// the cascade is orthographic: the tile's centre in clip space, its radius scaled per axis
			const float x = m[0] * t.centre[0] + m[4] * t.centre[1] + m[8] * t.centre[2] + m[12];
			const float y = m[1] * t.centre[0] + m[5] * t.centre[1] + m[9] * t.centre[2] + m[13];
			const float rx = t.radius * std::sqrt( m[0] * m[0] + m[4] * m[4] + m[8] * m[8] );
			const float ry = t.radius * std::sqrt( m[1] * m[1] + m[5] * m[5] + m[9] * m[9] );
			if ( std::fabs( x ) > 1.0f + rx || std::fabs( y ) > 1.0f + ry )
				continue;
		}
		for ( int j = 0; j < t.cmdCount; j++ )
			cmds.push_back( ( *vw.cmds )[t.firstCmd + j] );
	}
	if ( cmds.empty() )
		return true;
	prog->uni1i( "farInst", 1 );
	submit( r, *cp, *k, cmds );
	prog->uni1i( "farInst", 0 );
	return true;
}

}
