/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#include "cellaodecalgl.h"

#include "cellaodecal.h"
#include "gl/glscene.h"
#include "gl/renderer.h"

#include <QElapsedTimer>
#include <QFile>
#include <QHash>
#include <QTextStream>

#include <algorithm>
#include <cmath>
#include <vector>

namespace
{

constexpr std::uint32_t kSelfBit = 0x80u;

struct DecalState
{
	bool loaded = false;
	bool add = false;		// WW_CELL_AODECAL_RED=add
	QString dump;			// WW_CELL_AODECAL_DUMP
	QString last = QStringLiteral( "none yet" );
};

DecalState & st()
{
	static DecalState s;
	if ( !s.loaded ) {
		s.loaded = true;
		s.add = qgetenv( "WW_CELL_AODECAL_RED" ).trimmed() == "add";
		s.dump = QString::fromLocal8Bit( qgetenv( "WW_CELL_AODECAL_DUMP" ) );
	}
	return s;
}

QHash<const void *, std::shared_ptr<const AoDecalSet>> & sets()
{
	static QHash<const void *, std::shared_ptr<const AoDecalSet>> h;
	return h;
}

struct ModelGpu
{
	GLuint vol = 0;			// RGBA32F 3D: dims.x x dims.y x 3 dims.z
	GLuint vao = 0, vbo = 0;	// the mask triangles, then the footprint box's 36 vertices
	GLsizei maskVerts = 0;
};

struct DecalGpu
{
	const AoDecalSet * set = nullptr;
	std::vector<ModelGpu> models;
	GLuint target = 0, selfId = 0, fbo = 0;
	int w = 0, h = 0;
};

QHash<const void *, DecalGpu> & gpus()
{
	static QHash<const void *, DecalGpu> g;
	return g;
}

void freeModels( NifSkopeOpenGLContext::GLFunctions * fn, DecalGpu & g )
{
	for ( ModelGpu & m : g.models ) {
		if ( m.vol )
			fn->glDeleteTextures( 1, &m.vol );
		if ( m.vbo )
			fn->glDeleteBuffers( 1, &m.vbo );
		if ( m.vao )
			fn->glDeleteVertexArrays( 1, &m.vao );
	}
	g.models.clear();
	g.set = nullptr;
}

void uploadModels( NifSkopeOpenGLContext::GLFunctions * fn, DecalGpu & g, const AoDecalSet & S )
{
	freeModels( fn, g );
	g.set = &S;
	g.models.resize( S.models.size() );
	for ( size_t i = 0; i < S.models.size(); i++ ) {
		const aovol::Volume & v = S.models[i].vol;
		ModelGpu & m = g.models[i];
		if ( v.voxels() == 0 || v.k.size() != v.voxels() * 9 )
			continue;
		const int dx = v.dims[0], dy = v.dims[1], dz = v.dims[2];
		std::vector<float> tex( size_t( dx ) * size_t( dy ) * size_t( dz ) * 3 * 4, 0.0f );
		for ( size_t vi = 0; vi < v.voxels(); vi++ ) {
			const size_t x = vi % size_t( dx ), y = ( vi / size_t( dx ) ) % size_t( dy ), z = vi / ( size_t( dx ) * size_t( dy ) );
			for ( int slab = 0; slab < 3; slab++ ) {
				const size_t t = ( ( ( size_t( slab ) * size_t( dz ) + z ) * size_t( dy ) + y ) * size_t( dx ) + x ) * 4;
				for ( int c = 0; c < 4 && slab * 4 + c < 9; c++ )
					tex[t + size_t( c )] = v.k[vi * 9 + size_t( slab * 4 + c )];
			}
		}
		fn->glGenTextures( 1, &m.vol );
		fn->glBindTexture( GL_TEXTURE_3D, m.vol );
		fn->glTexImage3D( GL_TEXTURE_3D, 0, GL_RGBA32F, dx, dy, dz * 3, 0, GL_RGBA, GL_FLOAT, tex.data() );
		fn->glTexParameteri( GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
		fn->glTexParameteri( GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
		fn->glTexParameteri( GL_TEXTURE_3D, GL_TEXTURE_MAX_LEVEL, 0 );
		fn->glBindTexture( GL_TEXTURE_3D, 0 );
		// the mask triangles, then the footprint box (outward counter-clockwise; drawn with the front faces culled)
		std::vector<float> vb = S.models[i].maskTris;
		m.maskVerts = GLsizei( vb.size() / 3 );
		float lo[3], hi[3];
		v.footprint( lo, hi );
		const float c[8][3] = { { lo[0], lo[1], lo[2] }, { hi[0], lo[1], lo[2] }, { lo[0], hi[1], lo[2] }, { hi[0], hi[1], lo[2] },
			{ lo[0], lo[1], hi[2] }, { hi[0], lo[1], hi[2] }, { lo[0], hi[1], hi[2] }, { hi[0], hi[1], hi[2] } };
		static const int q[6][4] = { { 0, 2, 3, 1 }, { 4, 5, 7, 6 }, { 0, 1, 5, 4 }, { 2, 6, 7, 3 }, { 0, 4, 6, 2 }, { 1, 3, 7, 5 } };
		for ( const auto & f : q )
			for ( int k : { f[0], f[1], f[2], f[0], f[2], f[3] } )
				vb.insert( vb.end(), c[k], c[k] + 3 );
		fn->glGenVertexArrays( 1, &m.vao );
		fn->glGenBuffers( 1, &m.vbo );
		fn->glBindVertexArray( m.vao );
		fn->glBindBuffer( GL_ARRAY_BUFFER, m.vbo );
		fn->glBufferData( GL_ARRAY_BUFFER, GLsizeiptr( vb.size() * sizeof( float ) ), vb.data(), GL_STATIC_DRAW );
		fn->glEnableVertexAttribArray( 0 );
		fn->glVertexAttribPointer( 0, 3, GL_FLOAT, GL_FALSE, 0, nullptr );
		fn->glBindVertexArray( 0 );
		fn->glBindBuffer( GL_ARRAY_BUFFER, 0 );
	}
}

void alloc( NifSkopeOpenGLContext::GLFunctions * fn, DecalGpu & g, int W, int H, GLuint depthRb )
{
	if ( !g.target ) {
		fn->glGenTextures( 1, &g.target );
		fn->glGenTextures( 1, &g.selfId );
		fn->glGenFramebuffers( 1, &g.fbo );
	}
	if ( g.w != W || g.h != H ) {
		for ( GLuint t : { g.target, g.selfId } ) {
			fn->glBindTexture( GL_TEXTURE_2D, t );
			fn->glTexImage2D( GL_TEXTURE_2D, 0, GL_R32F, W, H, 0, GL_RED, GL_FLOAT, nullptr );
			fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
			fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
			fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
			fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
			fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0 );
		}
		fn->glBindTexture( GL_TEXTURE_2D, 0 );
		g.w = W;
		g.h = H;
	}
	fn->glBindFramebuffer( GL_FRAMEBUFFER, g.fbo );
	fn->glFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, g.target, 0 );
	fn->glFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, g.selfId, 0 );
	fn->glFramebufferRenderbuffer( GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, depthRb );
}

}	// namespace

void wwCellAoDecalPublish( const void * nif, std::shared_ptr<const AoDecalSet> set )
{
	if ( set && !set->copies.empty() )
		sets()[nif] = std::move( set );
	else
		sets().remove( nif );
}

bool wwCellAoDecalHas( const void * nif )
{
	return sets().contains( nif );
}

QString wwCellAoDecalEcho()
{
	return st().last;
}

unsigned wwCellAoDecalRun( Scene * scene, unsigned gbufTex, unsigned depthRb, int W, int H )
{
	if ( !scene || !scene->renderer )
		return 0;
	DecalState & s = st();
	auto it = sets().constFind( scene->nifModel );
	if ( it == sets().constEnd() || !aoDecalOn() ) {
		s.last = QStringLiteral( "off" );
		return 0;
	}
	const AoDecalSet & S = *it.value();
	Renderer * r = scene->renderer;
	auto fn = r->fn;
	QElapsedTimer timer;
	timer.start();
	DecalGpu & g = gpus()[r];
	if ( g.set != &S )
		uploadModels( fn, g, S );
	alloc( fn, g, W, H, GLuint( depthRb ) );
	if ( fn->glCheckFramebufferStatus( GL_FRAMEBUFFER ) != GL_FRAMEBUFFER_COMPLETE ) {
		s.last = QStringLiteral( "refused(target incomplete)" );
		return 0;
	}
	NifSkopeOpenGLContext::Program * prog = r->useProgram( "cell_aodecal.prog" );
	if ( !prog ) {
		s.last = QStringLiteral( "refused(no cell_aodecal.prog)" );
		return 0;
	}
	GLint prevBlend[4] = { GL_ONE, GL_ZERO, GL_ONE, GL_ZERO }, prevCull = GL_BACK, prevFront = GL_CCW, prevDepthFunc = GL_LEQUAL;
	GLfloat prevOffset[2] = { 0.0f, 0.0f };
	fn->glGetIntegerv( GL_BLEND_SRC_RGB, &prevBlend[0] );
	fn->glGetIntegerv( GL_BLEND_DST_RGB, &prevBlend[1] );
	fn->glGetIntegerv( GL_BLEND_SRC_ALPHA, &prevBlend[2] );
	fn->glGetIntegerv( GL_BLEND_DST_ALPHA, &prevBlend[3] );
	fn->glGetIntegerv( GL_CULL_FACE_MODE, &prevCull );
	fn->glGetIntegerv( GL_FRONT_FACE, &prevFront );
	fn->glGetIntegerv( GL_DEPTH_FUNC, &prevDepthFunc );
	fn->glGetFloatv( GL_POLYGON_OFFSET_FACTOR, &prevOffset[0] );
	fn->glGetFloatv( GL_POLYGON_OFFSET_UNITS, &prevOffset[1] );
	fn->glViewport( 0, 0, W, H );
	const GLenum both[2] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
	fn->glDrawBuffers( 2, both );
	fn->glColorMask( GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE );
	// cleared to 1 in both modes: the red "add" is 1 + sum(AO - 1), apart from the product only where copies overlap
	const GLfloat one[4] = { 1.0f, 0, 0, 0 }, zero[4] = { 0, 0, 0, 0 };
	fn->glClearBufferfv( GL_COLOR, 0, one );
	fn->glClearBufferfv( GL_COLOR, 1, zero );
	fn->glStencilMask( kSelfBit );
	fn->glClearStencil( 0 );
	fn->glClear( GL_STENCIL_BUFFER_BIT );
	fn->glEnable( GL_STENCIL_TEST );
	fn->glDepthMask( GL_FALSE );
	fn->glEnable( GL_CULL_FACE );
	fn->glFrontFace( GL_CCW );
	fn->glPolygonMode( GL_FRONT_AND_BACK, GL_FILL );

	fn->glActiveTexture( GL_TEXTURE0 );
	fn->glBindTexture( GL_TEXTURE_2D, GLuint( gbufTex ) );
	prog->uni1i( "aoGbuf", 0 );
	prog->uni1i( "aoVol", 1 );
	prog->uni1b( "aoAdd", s.add );
	const auto & pm = r->globalUniforms->projectionMatrix;
	prog->uni4f_l( prog->uniLocation( "aoProj" ), FloatVector4( pm[0][0], pm[1][1], pm[2][0], pm[2][1] ) );
	prog->uni2f_l( prog->uniLocation( "aoSize" ), float( W ), float( H ) );
	const Transform & vt = scene->view;
	const double sc = vt.scale != 0.0f ? double( vt.scale ) : 1.0;
	prog->uni1f( "aoSc", float( sc ) );
	double Rv[3][3], tv[3], cam[3];
	for ( int i = 0; i < 3; i++ ) {
		tv[i] = double( vt.translation[i] );
		for ( int j = 0; j < 3; j++ )
			Rv[i][j] = double( vt.rotation( i, j ) );
	}
	for ( int k = 0; k < 3; k++ ) {	// the camera, world: -Rv^T tv / sc
		cam[k] = 0.0;
		for ( int j = 0; j < 3; j++ )
			cam[k] -= Rv[j][k] * tv[j] / sc;
	}
	int drawn = 0, culled = 0;
	for ( size_t ci = 0; ci < S.copies.size(); ci++ ) {
		const AoDecalSet::Copy & c = S.copies[ci];
		if ( c.model < 0 || size_t( c.model ) >= g.models.size() || !g.models[size_t( c.model )].vao ) {
			culled++;
			continue;
		}
		const ModelGpu & mg = g.models[size_t( c.model )];
		const aovol::Volume & v = S.models[size_t( c.model )].vol;
		// model -> view: A = sc s Rv Rc, b = sc Rv t + tv; then the projection (pm[col][row])
		double A[3][3], b[3];
		for ( int i = 0; i < 3; i++ ) {
			b[i] = tv[i];
			for ( int j = 0; j < 3; j++ ) {
				double a = 0.0;
				for ( int k = 0; k < 3; k++ )
					a += Rv[i][k] * c.R[k * 3 + j];
				A[i][j] = sc * c.s * a;
				b[i] += sc * Rv[i][j] * c.t[j];
			}
		}
		for ( int row = 0; row < 4; row++ ) {
			double e[4] = { 0, 0, 0, double( pm[3][row] ) };
			for ( int i = 0; i < 3; i++ ) {
				for ( int j = 0; j < 3; j++ )
					e[j] += double( pm[i][row] ) * A[i][j];
				e[3] += double( pm[i][row] ) * b[i];
			}
			prog->uni4f_l( prog->uniLocation( "aoClip[%d]", row ), FloatVector4( float( e[0] ), float( e[1] ), float( e[2] ), float( e[3] ) ) );
		}
		// view -> model: M = Rc^T Rv^T / (s sc), b' = Rc^T (cam - t) / s; normals Rc^T Rv^T
		for ( int i = 0; i < 3; i++ ) {
			double M[3] = { 0, 0, 0 }, N[3] = { 0, 0, 0 }, bb = 0.0;
			for ( int j = 0; j < 3; j++ ) {
				for ( int k = 0; k < 3; k++ )
					N[j] += c.R[k * 3 + i] * Rv[j][k];
				M[j] = N[j] / ( c.s * sc );
				bb += c.R[j * 3 + i] * ( cam[j] - c.t[j] ) / c.s;
			}
			prog->uni4f_l( prog->uniLocation( "aoM[%d]", i ), FloatVector4( float( M[0] ), float( M[1] ), float( M[2] ), float( bb ) ) );
			prog->uni3f_l( prog->uniLocation( "aoN[%d]", i ), float( N[0] ), float( N[1] ), float( N[2] ) );
		}
		prog->uni3f_l( prog->uniLocation( "aoLo" ), v.lo[0], v.lo[1], v.lo[2] );
		prog->uni3f_l( prog->uniLocation( "aoHi" ), v.hi[0], v.hi[1], v.hi[2] );
		fn->glUniform3i( prog->uniLocation( "aoDims" ), v.dims[0], v.dims[1], v.dims[2] );
		prog->uni1f( "aoFade", v.fade );
		prog->uni3f_l( prog->uniLocation( "aoC" ), v.c[0], v.c[1], v.c[2] );
		for ( int j = 0; j < 9; j++ )
			prog->uni1f_l( prog->uniLocation( "aoCoef[%d]", j ), v.coef[j] );
		prog->uni1f( "aoRcut", v.rcut );
		prog->uni1f( "aoSelf", float( ci + 1 ) );
		fn->glActiveTexture( GL_TEXTURE1 );
		fn->glBindTexture( GL_TEXTURE_3D, mg.vol );
		fn->glBindVertexArray( mg.vao );
		// 1. the copy's own visible pixels: the stencil bit (and its id), nothing in the target
		prog->uni1i( "aoMode", 0 );
		const GLenum idOnly[2] = { GL_NONE, GL_COLOR_ATTACHMENT1 };
		fn->glDrawBuffers( 2, idOnly );
		fn->glDisable( GL_BLEND );
		fn->glDisable( GL_CULL_FACE );
		fn->glEnable( GL_DEPTH_TEST );
		fn->glDepthFunc( GL_LEQUAL );
		fn->glEnable( GL_POLYGON_OFFSET_FILL );
		fn->glPolygonOffset( -1.0f, -4.0f );
		fn->glStencilFunc( GL_ALWAYS, GLint( kSelfBit ), kSelfBit );
		fn->glStencilOp( GL_KEEP, GL_KEEP, GL_REPLACE );
		fn->glStencilMask( kSelfBit );
		if ( mg.maskVerts > 0 )
			fn->glDrawArrays( GL_TRIANGLES, 0, mg.maskVerts );
		fn->glDisable( GL_POLYGON_OFFSET_FILL );
		// 2. the footprint box's back faces: AO multiplied in wherever the bit is clear
		prog->uni1i( "aoMode", 1 );
		const GLenum aoOnly[2] = { GL_COLOR_ATTACHMENT0, GL_NONE };
		fn->glDrawBuffers( 2, aoOnly );
		fn->glDisable( GL_DEPTH_TEST );
		fn->glEnable( GL_CULL_FACE );
		fn->glCullFace( GL_FRONT );
		fn->glEnable( GL_BLEND );
		if ( s.add )
			fn->glBlendFunc( GL_ONE, GL_ONE );
		else
			fn->glBlendFunc( GL_DST_COLOR, GL_ZERO );
		fn->glStencilFunc( GL_NOTEQUAL, GLint( kSelfBit ), kSelfBit );
		fn->glStencilOp( GL_KEEP, GL_KEEP, GL_KEEP );
		fn->glStencilMask( 0 );
		fn->glEnable( GL_DEPTH_CLAMP );	// a back face past the far plane still covers its pixels
		fn->glDrawArrays( GL_TRIANGLES, mg.maskVerts, 36 );
		fn->glDisable( GL_DEPTH_CLAMP );
		// 3. the bit cleared for the next copy
		fn->glStencilMask( kSelfBit );
		fn->glClear( GL_STENCIL_BUFFER_BIT );
		drawn++;
	}
	fn->glBindVertexArray( 0 );
	fn->glDisable( GL_BLEND );
	fn->glBlendFuncSeparate( GLenum( prevBlend[0] ), GLenum( prevBlend[1] ), GLenum( prevBlend[2] ), GLenum( prevBlend[3] ) );
	fn->glCullFace( GLenum( prevCull ) );
	fn->glFrontFace( GLenum( prevFront ) );
	fn->glPolygonOffset( prevOffset[0], prevOffset[1] );
	fn->glDisable( GL_STENCIL_TEST );
	fn->glStencilMask( 0xFFu );
	fn->glEnable( GL_DEPTH_TEST );
	fn->glDepthFunc( GLenum( prevDepthFunc ) );
	fn->glBindTexture( GL_TEXTURE_3D, 0 );
	fn->glActiveTexture( GL_TEXTURE0 );
	fn->glBindTexture( GL_TEXTURE_2D, 0 );
	const GLenum first[1] = { GL_COLOR_ATTACHMENT0 };
	fn->glDrawBuffers( 1, first );
	r->stopProgram();
	s.last = QStringLiteral( "on %1x%2 copies %3 drawn %4 skipped %5 red %6 ms %7" ).arg( W ).arg( H )
		.arg( S.copies.size() ).arg( drawn ).arg( culled ).arg( s.add ? QStringLiteral( "add" ) : QStringLiteral( "none" ) )
		.arg( timer.elapsed() );

	/* WW_CELL_AODECAL_DUMP=<file>: int32 W H, the target (W x H float, bottom row first), the copies' ids (index + 1,
	 * 0 none), the opaque pass (W x H RGBA float); <file>.txt the view (sc, Rv rows, tv, P00 P11 P20 P21), then one
	 * line per copy: index, its .ao file, R (row-major), t, s -- as PLACED (Copy::placed*, not what was drawn) */
	if ( !s.dump.isEmpty() ) {
		std::vector<float> tg( size_t( W ) * size_t( H ) ), id( tg.size() ), gb( tg.size() * 4 );
		fn->glPixelStorei( GL_PACK_ALIGNMENT, 4 );
		fn->glBindFramebuffer( GL_READ_FRAMEBUFFER, g.fbo );
		fn->glReadBuffer( GL_COLOR_ATTACHMENT0 );
		fn->glReadPixels( 0, 0, W, H, GL_RED, GL_FLOAT, tg.data() );
		fn->glReadBuffer( GL_COLOR_ATTACHMENT1 );
		fn->glReadPixels( 0, 0, W, H, GL_RED, GL_FLOAT, id.data() );
		fn->glReadBuffer( GL_COLOR_ATTACHMENT0 );
		fn->glBindTexture( GL_TEXTURE_2D, GLuint( gbufTex ) );
		fn->glGetTexImage( GL_TEXTURE_2D, 0, GL_RGBA, GL_FLOAT, gb.data() );
		fn->glBindTexture( GL_TEXTURE_2D, 0 );
		double mean = 0.0, mn = 1.0;
		for ( float v : tg ) {
			mean += double( v );
			mn = std::min( mn, double( v ) );
		}
		mean /= double( std::max<size_t>( tg.size(), 1 ) );
		s.last += QStringLiteral( " mean %1 min %2" ).arg( mean, 0, 'f', 5 ).arg( mn, 0, 'f', 4 );
		QFile f( s.dump );
		if ( f.open( QIODevice::WriteOnly ) ) {
			const qint32 hd[2] = { W, H };
			f.write( reinterpret_cast<const char *>( hd ), sizeof( hd ) );
			f.write( reinterpret_cast<const char *>( tg.data() ), qint64( tg.size() * sizeof( float ) ) );
			f.write( reinterpret_cast<const char *>( id.data() ), qint64( id.size() * sizeof( float ) ) );
			f.write( reinterpret_cast<const char *>( gb.data() ), qint64( gb.size() * sizeof( float ) ) );
		}
		QFile t( s.dump + QStringLiteral( ".txt" ) );
		if ( t.open( QIODevice::WriteOnly | QIODevice::Text ) ) {
			QTextStream ts( &t );
			ts.setRealNumberPrecision( 17 );
			ts << "aodecal: " << s.last << "\n";
			ts << "dir " << S.dir << "\n";
			ts << "view " << sc;
			for ( int i = 0; i < 3; i++ )
				for ( int j = 0; j < 3; j++ )
					ts << " " << Rv[i][j];
			ts << " " << tv[0] << " " << tv[1] << " " << tv[2];
			ts << " " << pm[0][0] << " " << pm[1][1] << " " << pm[2][0] << " " << pm[2][1] << "\n";
			for ( size_t ci = 0; ci < S.copies.size(); ci++ ) {
				const AoDecalSet::Copy & c = S.copies[ci];
				if ( c.model < 0 )
					continue;
				ts << "copy " << ci << " " << aovol::aoFileName( S.models[size_t( c.model )].norm );
				for ( double e : c.placedR )	// as placed: the red "frozen" draws elsewhere, the gate must see it
					ts << " " << e;
				ts << " " << c.placedT[0] << " " << c.placedT[1] << " " << c.placedT[2] << " " << c.placedS << "\n";
			}
		}
	}
	return g.target;
}
