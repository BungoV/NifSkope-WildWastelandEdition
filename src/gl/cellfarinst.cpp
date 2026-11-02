#include "gl/cellfarinst.h"

#include "cellfarhide.h"
#include "cellmesh.h"
#include "wwswitch.h"
#include "gl/cellpasstime.h"
#include "gl/cellunits.h"	// lane FIX1101A lodch: the far stream's unit
#include "gl/glscene.h"
#include "gl/renderer.h"

#include <QElapsedTimer>
#include <QHash>
#include <QOpenGLContext>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
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
	GLuint stream = 0, streamTex = 0;	// lane FIX1101A lodch: the rows' per-vertex bake channels (RG8 buffer texture)
	GLuint box = 0;	// lane FARLOOK1101: per row its mesh box (centre, half extent), made on the first splat draw
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
				GLuint b[5] = { it->vbo, it->ebo, it->rows, it->ind, it->stream };
				fn->glDeleteBuffers( it->stream ? 5 : 4, b );
				if ( it->streamTex )
					fn->glDeleteTextures( 1, &it->streamTex );
				if ( it->box )
					fn->glDeleteBuffers( 1, &it->box );
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
	if ( !d.vstream.isEmpty() ) {	// lane FIX1101A lodch: read by fo4_default.vert at the row's texel + gl_VertexID
		fn->glGenBuffers( 1, &k.stream );
		fn->glBindBuffer( GL_TEXTURE_BUFFER, k.stream );
		fn->glBufferData( GL_TEXTURE_BUFFER, GLsizeiptr( d.vstream.size() ), d.vstream.constData(), GL_STATIC_DRAW );
		fn->glBindBuffer( GL_TEXTURE_BUFFER, 0 );
		GLint was = 0;
		fn->glGetIntegerv( GL_ACTIVE_TEXTURE, &was );
		fn->glActiveTexture( GLenum( GL_TEXTURE0 + WwCellUnit::FarStream ) );
		fn->glGenTextures( 1, &k.streamTex );
		fn->glBindTexture( GL_TEXTURE_BUFFER, k.streamTex );
		fn->glTexBuffer( GL_TEXTURE_BUFFER, GL_RG8, k.stream );
		fn->glActiveTexture( GLenum( was ) );
	}
	k.indBytes = std::size_t( d.vbo.size() ) + std::size_t( d.ebo.size() ) * 2 + std::size_t( d.inst.size() ) * 4
		+ std::size_t( d.vstream.size() );
	c.gpuBytes += qint64( k.indBytes );
	return &*c.copies.insert( d.id, k );
}

/* lane FARLOOK1101: the rows' mesh boxes (attributes 14/15, one a row): each command's vertices (its indices +
 * base vertex) bound the box of every row it draws. Made once a copy, only when a splat pass first needs it, so the
 * splat off makes no GL call more. */
void ensureBox( GLFunctions * fn, Ctx & c, Copy & k, const CellMeshInst & d )
{
	if ( k.box )
		return;
	const qsizetype nRows = d.inst.size() / 16;
	std::vector<float> box( std::size_t( nRows ) * 8, 0.0f );
	const char * vb = d.vbo.constData();
	const qsizetype nVerts = d.vbo.size() / 36;
	for ( const CellMeshInst::Cmd & m : d.cmds ) {
		float lo[3] = { 1e30f, 1e30f, 1e30f }, hi[3] = { -1e30f, -1e30f, -1e30f };
		for ( quint32 i = 0; i < m.count; i++ ) {
			const qsizetype ii = qsizetype( m.firstIndex ) + qsizetype( i );
			if ( ii >= d.ebo.size() )
				break;
			const qsizetype v = qsizetype( d.ebo[ii] ) + m.baseVertex;
			if ( v < 0 || v >= nVerts )
				continue;
			float p[3];
			std::memcpy( p, vb + v * 36, 12 );
			for ( int a = 0; a < 3; a++ ) {
				lo[a] = std::min( lo[a], p[a] );
				hi[a] = std::max( hi[a], p[a] );
			}
		}
		if ( lo[0] > hi[0] )
			continue;
		for ( quint32 j = 0; j < m.instanceCount; j++ ) {
			const qsizetype row = qsizetype( m.baseInstance ) + qsizetype( j );
			if ( row >= nRows )
				break;
			float * b = &box[std::size_t( row ) * 8];
			for ( int a = 0; a < 3; a++ ) {
				b[a] = 0.5f * ( lo[a] + hi[a] );
				b[4 + a] = std::max( 0.5f * ( hi[a] - lo[a] ), 1e-3f );
			}
		}
	}
	fn->glBindVertexArray( k.vao );
	fn->glGenBuffers( 1, &k.box );
	fn->glBindBuffer( GL_ARRAY_BUFFER, k.box );
	fn->glBufferData( GL_ARRAY_BUFFER, GLsizeiptr( box.size() * sizeof( float ) ), box.data(), GL_STATIC_DRAW );
	for ( int a = 0; a < 2; a++ ) {
		fn->glEnableVertexAttribArray( GLuint( 14 + a ) );
		fn->glVertexAttribPointer( GLuint( 14 + a ), 4, GL_FLOAT, GL_FALSE, 32, (const void *) std::size_t( 16 * a ) );
		fn->glVertexAttribDivisor( GLuint( 14 + a ), 1 );
	}
	fn->glBindVertexArray( 0 );
	fn->glBindBuffer( GL_ARRAY_BUFFER, 0 );
	k.indBytes += box.size() * sizeof( float );
	c.gpuBytes += qint64( box.size() * sizeof( float ) );
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
	const Transform & viewTrans, int splatPass )
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
				!prog->wwBindless ? "" : bindlessUniformRow() ? " (bindless twin: the row in the uniform alone, WW_CELL_FARINST_BL)"
				: " (bindless twin: rows read at the base instance, red WW_CELL_FARINST_RED=blrow)" );
			std::fflush( stderr );
		}
	}
	if ( splatPass > 0 ) {	// lane FARLOOK1101: a splat twin (fo4_cellfs / fo4_cellcsmfs)
		ensureBox( r->fn, *cp, *k, d );
		prog->uni2f( "farSplat", splatPx(), float( splatPass ) );
		if ( splatPass == 2 ) {
			if ( !splatRedNoAlpha() ) {
				r->fn->glEnable( GL_BLEND );
				r->fn->glBlendFuncSeparate( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ZERO, GL_ONE );
			} else {
				r->fn->glDisable( GL_BLEND );
			}
			r->fn->glDepthMask( GL_FALSE );
		}
	} else if ( prog->uniLocation( "farSplat" ) >= 0 ) {
		prog->uni2f( "farSplat", 0.0f, 0.0f );	// a twin drawn without a second pass (no blending): every row, as before
	}
	const bool uniRow = prog->wwBindless && bindlessUniformRow();	// FARINSTBL1: the shape's own row for every command
	if ( uniRow )
		prog->uni1i( "wwBlUniformRow", 1 );
	prog->uni1i( "farInst", 1 );
	GLint was = 0;
	if ( k->streamTex ) {	// lane FIX1101A lodch: the rows' AO + sky (the sampler's unit is set at link, gl/glcontext.cpp)
		r->fn->glGetIntegerv( GL_ACTIVE_TEXTURE, &was );
		r->fn->glActiveTexture( GLenum( GL_TEXTURE0 + WwCellUnit::FarStream ) );
		r->fn->glBindTexture( GL_TEXTURE_BUFFER, k->streamTex );
		r->fn->glActiveTexture( GLenum( was ) );
		prog->uni1i( "farStreamOn", 1 );
	}
	submit( r, *cp, *k, cmds );
	if ( k->streamTex )
		prog->uni1i( "farStreamOn", 0 );
	prog->uni1i( "farInst", 0 );
	if ( uniRow )
		prog->uni1i( "wwBlUniformRow", 0 );
	if ( splatPass == 2 ) {	// the far rings' own state (opaque, depth written) for whatever draws next
		r->fn->glDisable( GL_BLEND );
		r->fn->glDepthMask( GL_TRUE );
	}
	return true;
}

float splatPx()
{
	WW_SWITCH_STATIC( float, s, [] {
		const QString v = wwEnvStr( "WW_CELL_FARSPLAT" ).trimmed().toLower();
		if ( v == QLatin1String( "1" ) || v == QLatin1String( "on" ) )
			return 2.0f;
		bool ok = false;
		const float f = v.toFloat( &ok );
		return ok && f > 0.0f ? std::min( f, 16.0f ) : 0.0f;
	}() );
	return s;
}

bool splatRedNoAlpha()
{
	WW_SWITCH_STATIC( bool, red, wwEnv( "WW_CELL_FARSPLAT_RED" ) == "noalpha" );
	return red;
}

static bool redBlRow()
{
	WW_SWITCH_STATIC( bool, red, wwEnv( "WW_CELL_FARINST_RED" ) == "blrow" );
	return red;
}

bool bindlessUniformRow()
{
	WW_SWITCH_STATIC( bool, on, [] {
		const QString v = wwEnvStr( "WW_CELL_FARINST_BL" ).trimmed().toLower();
		return v == QLatin1String( "1" ) || v == QLatin1String( "on" );
	}() );
	return on && !redBlRow();
}

bool bindlessTwinAllowed()
{
	return redBlRow() || bindlessUniformRow();
}

RowAudit rowAudit( const CellMeshInst & d, bool bindlessProgram, quint32 ringRows, bool uniformRow )
{
	RowAudit a;
	for ( const CellMeshInst::Cmd & m : d.cmds ) {
		a.cmds++;
		const quint32 off = bindlessProgram && !uniformRow ? m.baseInstance : 0u;	// wwDrawIdx - wwDrawBase
		if ( off == 0 )
			continue;
		a.foreign++;
		if ( off >= ringRows )
			a.pastRing++;
		a.maxOffset = std::max( a.maxOffset, off );
	}
	return a;
}

// lane SHADOWSHIMMER4: a tile meets the cascade's view slice (gl/farswayslice.h); mv column-major, uniform scale
static bool tileInSlice( const CellMeshInst::Tile & t, const float * mv, const WwFarSwaySlice::Slice & s )
{
	const double x = double( mv[0] ) * t.centre[0] + double( mv[4] ) * t.centre[1] + double( mv[8] ) * t.centre[2] + mv[12];
	const double y = double( mv[1] ) * t.centre[0] + double( mv[5] ) * t.centre[1] + double( mv[9] ) * t.centre[2] + mv[13];
	const double z = double( mv[2] ) * t.centre[0] + double( mv[6] ) * t.centre[1] + double( mv[10] ) * t.centre[2] + mv[14];
	const double sc = std::sqrt( double( mv[0] ) * mv[0] + double( mv[1] ) * mv[1] + double( mv[2] ) * mv[2] );
	return WwFarSwaySlice::tileMeets( s, x, y, z, double( t.radius ) * sc );
}

int sliceTiles( const CellMeshInst & d, const float mv[16], const WwFarSwaySlice::Slice & s, quint64 & hash )
{
	const CellFarHide::View vw = CellFarHide::view( d );
	int n = 0;
	for ( qsizetype i = 0; i < vw.tiles->size(); i++ ) {
		if ( !tileInSlice( ( *vw.tiles )[i], mv, s ) )
			continue;
		n++;
		hash ^= quint64( i ) + 0x9e3779b97f4a7c15ULL + ( hash << 6 ) + ( hash >> 2 );
	}
	return n;
}

bool tileBounds( const CellMeshInst & d, const float m[16], const float * mv, const WwFarSwaySlice::Slice * slice,
	float grow, float out[4] )
{
	const CellFarHide::View vw = CellFarHide::view( d );
	const float sx = std::sqrt( m[0] * m[0] + m[4] * m[4] + m[8] * m[8] );
	const float sy = std::sqrt( m[1] * m[1] + m[5] * m[5] + m[9] * m[9] );
	bool any = false;
	for ( const CellMeshInst::Tile & t : *vw.tiles ) {
		if ( slice && mv && !tileInSlice( t, mv, *slice ) )
			continue;
		const float x = m[0] * t.centre[0] + m[4] * t.centre[1] + m[8] * t.centre[2] + m[12];
		const float y = m[1] * t.centre[0] + m[5] * t.centre[1] + m[9] * t.centre[2] + m[13];
		if ( !redNoCull() && ( std::fabs( x ) > 1.0f + t.radius * sx || std::fabs( y ) > 1.0f + t.radius * sy ) )
			continue;	// drawCascade skips it: it draws nothing
		const float rx = ( t.radius + grow ) * sx, ry = ( t.radius + grow ) * sy;
		if ( !any ) {
			out[0] = x - rx;
			out[1] = y - ry;
			out[2] = x + rx;
			out[3] = y + ry;
			any = true;
		} else {
			out[0] = std::min( out[0], x - rx );
			out[1] = std::min( out[1], y - ry );
			out[2] = std::max( out[2], x + rx );
			out[3] = std::max( out[3], y + ry );
		}
	}
	return any;
}

bool drawCascade( NifSkopeOpenGLContext * r, const CellMeshInst & d, NifSkopeOpenGLContext::Program * prog,
	const float m[16], const float * mv, const WwFarSwaySlice::Slice * slice, int part )
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
	const bool split = slice && mv && part != WwFarSwaySlice::Both;
	for ( const CellMeshInst::Tile & t : *vw.tiles ) {
		if ( split && tileInSlice( t, mv, *slice ) != ( part == WwFarSwaySlice::Sway ) )
			continue;	// lane SHADOWSHIMMER4: the other part of the bucket
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
