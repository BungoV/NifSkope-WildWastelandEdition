#include "features/far-lod/cellfarinst.h"

#include "cellfarhide.h"
#include "cellmesh.h"
#include "features/far-lod/lodpreview.h"	// lane PERFLOAD1 P8: the LOD preview's tint (attribute 9, y)
#include "features/far-lod/lodinative.h"	// MODULES1 10b-2: the far AO row and the channel view
#include "wwswitch.h"
#include "features/shared/cellpasstime.h"
#include "gl/cellunits.h"	// lane FIX1101A lodch: the far stream's unit
#include "gl/glscene.h"
#include "gl/gpustage.h"	// MODULES1: static buffers filled by a GPU copy (no driver RAM copy)
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

quint64 wwGlPaintCount();	// glview.cpp

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
	// lane PERFLOAD1 P7 round 2: per row the ladder dither (attribute 9, x), the ladder rows of each REFR, the rows set
	GLuint fade = 0;
	std::vector<float> fadeCpu;
	QHash<quint32, QVector<int>> refRows;
	std::vector<int> fadeSet;
	quint64 fadeFor = ~quint64( 0 ), fadePaint = ~quint64( 0 );
	quint64 tintFor = 0;	// lane PERFLOAD1 P8: LodPreview::gen() the rows' tint codes (attribute 9, y) were written for
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
				if ( it->fade )
					fn->glDeleteBuffers( 1, &it->fade );
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
	wwStageBufferData( GL_ARRAY_BUFFER, GLsizeiptr( d.vbo.size() ), d.vbo.constData(), GL_STATIC_DRAW );
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
	wwStageBufferData( GL_ARRAY_BUFFER, GLsizeiptr( d.inst.size() ) * 4, d.inst.constData(), GL_STATIC_DRAW );
	for ( int a = 0; a < 4; a++ ) {
		fn->glEnableVertexAttribArray( GLuint( 10 + a ) );
		fn->glVertexAttribPointer( GLuint( 10 + a ), 4, GL_FLOAT, GL_FALSE, 64, (const void *) std::size_t( 16 * a ) );
		fn->glVertexAttribDivisor( GLuint( 10 + a ), 1 );
	}
	fn->glGenBuffers( 1, &k.ebo );
	fn->glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, k.ebo );
	wwStageBufferData( GL_ELEMENT_ARRAY_BUFFER, GLsizeiptr( d.ebo.size() ) * 2, d.ebo.constData(), GL_STATIC_DRAW );
	fn->glBindVertexArray( 0 );
	fn->glBindBuffer( GL_ARRAY_BUFFER, 0 );
	fn->glGenBuffers( 1, &k.ind );
	if ( !d.vstream.isEmpty() ) {	// lane FIX1101A lodch: read by fo4_default.vert at the row's texel + gl_VertexID
		fn->glGenBuffers( 1, &k.stream );
		fn->glBindBuffer( GL_TEXTURE_BUFFER, k.stream );
		wwStageBufferData( GL_TEXTURE_BUFFER, GLsizeiptr( d.vstream.size() ), d.vstream.constData(), GL_STATIC_DRAW );
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

/* lane PERFLOAD1 P7 round 2: the tree ladder's per-row dither (attribute 9, read by fo4_default.vert as farLadder).
 * Set once a paint, so the depth pre-pass and the main pass (drawn EQUAL) discard the same pixels.
 * Made on the first draw of a bucket with ladder rows (zeros: every row whole); each frame the rows of the trees mid-fade
 * (CellFarHide::ladderFades) take TreeLadder::rowFade and last frame's go back to 0, one sub-upload of the span. */
void ladderFadeRows( GLFunctions * fn, Ctx & c, Copy & k, const CellMeshInst & d )
{
	// lane PERFLOAD1 P8: y = the LOD preview's tint code (CellMeshInst::rowLevel, 0 = untinted), written on each change
	const bool tint = LodPreview::tint() && !d.rowLevel.isEmpty();
	if ( d.rowStep.isEmpty() && !tint && !k.fade )
		return;
	const qsizetype nRows = d.inst.size() / 16;
	if ( !k.fade ) {
		k.fadeCpu.assign( std::size_t( nRows ) * 2, 0.0f );
		for ( qsizetype i = 0; i < nRows && i < d.rowStep.size() && i < d.rowRef.size(); i++ )
			if ( d.rowStep[i] && d.rowRef[i] )
				k.refRows[d.rowRef[i]].append( int( i ) );
		fn->glBindVertexArray( k.vao );
		fn->glGenBuffers( 1, &k.fade );
		fn->glBindBuffer( GL_ARRAY_BUFFER, k.fade );
		fn->glBufferData( GL_ARRAY_BUFFER, GLsizeiptr( k.fadeCpu.size() * sizeof( float ) ), k.fadeCpu.data(), GL_DYNAMIC_DRAW );
		fn->glEnableVertexAttribArray( 9 );
		fn->glVertexAttribPointer( 9, 2, GL_FLOAT, GL_FALSE, 8, (const void *) 0 );
		fn->glVertexAttribDivisor( 9, 1 );
		fn->glBindVertexArray( 0 );
		fn->glBindBuffer( GL_ARRAY_BUFFER, 0 );
		k.indBytes += k.fadeCpu.size() * sizeof( float );
		c.gpuBytes += qint64( k.fadeCpu.size() * sizeof( float ) );
	}
	if ( k.tintFor != LodPreview::gen() ) {
		k.tintFor = LodPreview::gen();
		int perCode[8] = {};	// lane LODLIVE1: the coloration's row counts (code = band + 1)
		for ( qsizetype i = 0; i < nRows; i++ ) {
			k.fadeCpu[std::size_t( i ) * 2 + 1] = tint && i < d.rowLevel.size() ? float( d.rowLevel[i] ) : 0.0f;
			if ( tint && i < d.rowLevel.size() && d.rowLevel[i] > 0 && d.rowLevel[i] < 8 )
				perCode[d.rowLevel[i]]++;
		}
		for ( int code = 1; code < 8; code++ )
			if ( perCode[code] )
				LodPreview::noteRows( code - 1, perCode[code] );
		fn->glBindBuffer( GL_ARRAY_BUFFER, k.fade );
		fn->glBufferSubData( GL_ARRAY_BUFFER, 0, GLsizeiptr( k.fadeCpu.size() * sizeof( float ) ), k.fadeCpu.data() );
		fn->glBindBuffer( GL_ARRAY_BUFFER, 0 );
	}
	if ( d.rowStep.isEmpty() )
		return;
	const quint64 g = CellFarHide::ladderFadeGen(), paint = wwGlPaintCount();
	if ( k.fadeFor == g || k.fadePaint == paint )
		return;
	k.fadeFor = g;
	k.fadePaint = paint;
	int lo = int( nRows ), hi = -1;
	for ( int i : k.fadeSet ) {
		k.fadeCpu[std::size_t( i ) * 2] = 0.0f;
		lo = std::min( lo, i );
		hi = std::max( hi, i );
	}
	k.fadeSet.clear();
	const QHash<quint32, CellFarHide::LadderFade> & fades = CellFarHide::ladderFades();
	for ( auto it = fades.cbegin(); it != fades.cend(); ++it ) {
		const auto rr = k.refRows.constFind( it.key() );
		if ( rr == k.refRows.cend() )
			continue;
		for ( int i : *rr ) {
			const float f = TreeLadder::rowFade( it->st, it->draw, quint8( d.rowStep[i] & 0x7Fu ) );
			if ( f == 0.0f )
				continue;
			k.fadeCpu[std::size_t( i ) * 2] = f;
			k.fadeSet.push_back( i );
			lo = std::min( lo, i );
			hi = std::max( hi, i );
		}
	}
	CellFarHide::ladderDitherRows() += qint64( k.fadeSet.size() );
	if ( hi < lo )
		return;
	fn->glBindBuffer( GL_ARRAY_BUFFER, k.fade );
	fn->glBufferSubData( GL_ARRAY_BUFFER, GLintptr( std::size_t( lo ) * 8 ), GLsizeiptr( std::size_t( hi - lo + 1 ) * 8 ),
		&k.fadeCpu[std::size_t( lo ) * 2] );
	fn->glBindBuffer( GL_ARRAY_BUFFER, 0 );
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

int vcAoType( const QString & shapeName )
{
	if ( !shapeName.startsWith( QLatin1String( "FarLOD " ) ) )
		return 0;
	const int t = shapeName.startsWith( QLatin1String( "FarLOD terrain r" ) ) ? 2
		: shapeName.startsWith( QLatin1String( "FarLOD r" ) ) ? 1 : 0;
	if ( !t )
		return 0;
	WW_SWITCH_STATIC( bool, view, lodlChannelFromEnv() != LodlChannel::None );
	// the far ground's grey is its mask sheet's B only with the Far LOD AO row on (btdterrain.cpp ringMaskCh)
	return view || ( t == 2 && !lodlAoOn() ) ? 0 : t;
}

void vcAoUniform( NifSkopeOpenGLContext::Program * prog, int type )
{
	if ( !prog || prog->uniLocation( "farVcAo" ) < 0 )
		return;
	auto key01 = []( const char * name ) {
		bool ok = false;
		const float v = wwEnvStr( name ).trimmed().toFloat( &ok );
		return ok ? std::clamp( v, 0.0f, 1.0f ) : 1.0f;
	};
	WW_SWITCH_STATIC( float, aoMax, key01( "WW_LODL_AO_MAX" ) );
	WW_SWITCH_STATIC( float, skyMax, key01( "WW_LODL_SKY_MAX" ) );
	WW_SWITCH_STATIC( bool, albedo, wwEnv( "WW_LODL_AO_RED" ).trimmed() == "albedo" );
	// MODULES1 10c: the PBR far draws (pbrm_* programs: fo4_default.vert + pbrm_default.frag) take the same path;
	// red WW_LODL_AO_RED=nopbr: type 0 there (the grey stays in the vertex colour, as before 10c)
	WW_SWITCH_STATIC( bool, noPbr, wwEnv( "WW_LODL_AO_RED" ).trimmed() == "nopbr" );
	const bool pbr = std::string_view( prog->name ).rfind( "pbrm_", 0 ) == 0;
	if ( pbr && noPbr )
		type = 0;
	prog->uni4f( "farVcAo", FloatVector4( aoMax, skyMax, float( type ), albedo ? 1.0f : 0.0f ) );
	static int pbrSaid = -1;
	if ( pbr && pbrSaid != ( noPbr ? 0 : 1 ) ) {
		pbrSaid = noPbr ? 0 : 1;
		std::fprintf( stderr, "far baked ao vc: PBR far draws (%s): %s\n", std::string( prog->name ).c_str(),
			noPbr ? "RED WW_LODL_AO_RED=nopbr (the grey stays in the vertex colour)"
			: "the same farVcAo path and the same two debug maxima as the legacy far draws" );
	}
	if ( type <= 0 )
		return;
	static int seen = 0;
	static float said[3] = { -1.0f, -1.0f, -1.0f };
	const int bit = 1 << ( type - 1 );
	if ( ( seen & bit ) && said[0] == aoMax && said[1] == skyMax && said[2] == ( albedo ? 1.0f : 0.0f ) )
		return;
	seen |= bit;
	said[0] = aoMax;
	said[1] = skyMax;
	said[2] = albedo ? 1.0f : 0.0f;
	std::fprintf( stderr, "far baked ao vc: rule %s; drawn so far: flat far copy %s (its grey -> the AO rule x max %.3f, "
		"WW_LODL_AO_MAX), far ground %s (far ground carries sky occlusion only, no baked contact AO: its mask sheet's B "
		"-> the sky occlusion x max %.3f, WW_LODL_SKY_MAX), "
		"instanced rows without the stream %s (mesh grey x row grey -> the AO rule x max %.3f)\n",
		albedo ? "RED albedo (the grey stays in the vertex colour)" : "shared (cell_ao.glsl cellAoShared / the far sky occlusion)",
		( seen & 1 ) ? "yes" : "not yet", double( aoMax ), ( seen & 2 ) ? "yes" : "not yet", double( skyMax ),
		( seen & 4 ) ? "yes" : "not yet", double( aoMax ) );
}

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
	if ( !d.rowStep.isEmpty() || !d.rowLevel.isEmpty() ) {	// lane PERFLOAD1 P7 round 2: the ladder rows' dither, the cards' noise frame; P8 the tint
		ladderFadeRows( r->fn, *cp, *k, d );
		prog->uni1f( "farFadeFrame", float( wwGlPaintCount() & 1023u ) );	// the cards' frame (impostorchunk.cpp)
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
		{	/* MODULES1 round 10: the far rows' baked AO enters the one AO rule (cell_ao.glsl cellAoShared), scaled by a
			 * live maximum; the baked sky occlusion ( 1 - visibility ) takes its own. Debug keys only (no menu row, no
			 * INI): WW_LODL_AO_MAX, WW_LODL_SKY_MAX, 0..1, unset = 1. Red WW_LODL_AO_RED=albedo: the AO in the vertex
			 * colour (the diffuse light only), as before. */
			auto key01 = []( const char * name ) {
				bool ok = false;
				const float v = wwEnvStr( name ).trimmed().toFloat( &ok );
				return ok ? std::clamp( v, 0.0f, 1.0f ) : 1.0f;
			};
			WW_SWITCH_STATIC( float, aoMax, key01( "WW_LODL_AO_MAX" ) );
			WW_SWITCH_STATIC( float, skyMax, key01( "WW_LODL_SKY_MAX" ) );
			WW_SWITCH_STATIC( bool, albedo, wwEnv( "WW_LODL_AO_RED" ).trimmed() == "albedo" );
			prog->uni3f( "farBakeK", aoMax, skyMax, albedo ? 1.0f : 0.0f );
			static float said[3] = { -1.0f, -1.0f, -1.0f };
			if ( said[0] != aoMax || said[1] != skyMax || said[2] != ( albedo ? 1.0f : 0.0f ) ) {
				said[0] = aoMax;
				said[1] = skyMax;
				said[2] = albedo ? 1.0f : 0.0f;
				std::fprintf( stderr, "far baked ao: max %.3f (WW_LODL_AO_MAX), sky occlusion max %.3f (WW_LODL_SKY_MAX), rule %s; "
					"the far rows with the AO + sky stream (the flat copy, the far ground and the rows without the stream: line `far baked ao vc:`)\n",
					double( aoMax ), double( skyMax ), albedo ? "RED albedo (the vertex colour)" : "shared (cell_ao.glsl cellAoShared: the whole lit colour)" );
			}
		}
	}
	// MODULES1 10b-2: rows without the stream carry the AO as mesh grey x row grey (lodinative.cpp), unless a channel view
	bool rowsAo = false;
	if ( !k->streamTex ) {
		WW_SWITCH_STATIC( bool, view, lodlChannelFromEnv() != LodlChannel::None );
		rowsAo = !view && lodlAoOn();
	}
	vcAoUniform( prog, rowsAo ? 3 : 0 );
	submit( r, *cp, *k, cmds );
	if ( rowsAo )
		vcAoUniform( prog, 0 );
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
	const CellFarHide::View vw = CellFarHide::view( d, true );	// lane PERFLOAD1 P7 round 2: the casters
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
	const CellFarHide::View vw = CellFarHide::view( d, true );	// lane PERFLOAD1 P7 round 2: the casters
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
	const CellFarHide::View vw = CellFarHide::view( d, true );   // lane FARHOLE2; PERFLOAD1 P7 round 2: the ladder's cast step
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
