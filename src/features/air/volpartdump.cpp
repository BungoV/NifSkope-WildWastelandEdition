/* Volumetric Air module: the volumetric-particle probe dump (lane VOLPART1, gates only; was in
 * src/gl/glparticles.cpp until MODULES1). The particle draw calls wwVolPartDumpSystem after a volumetric system. */
#include "features/air/volpartdump.h"
#include "gl/glparticles.h"
#include "wwswitch.h"	// lane TESTDRIVER1: the WW_* switch store
#include <cmath>
#include <QRegularExpression>
#include <QSet>
#include <cstdio>
#include "gl/cellhdr.h"	// lane SUNCELL1: the HDR frame's stencil
#include "features/near/cellqueue.h"	// lane GPUFIX1
#include "gl/cellunits.h"	// lane WATERFIX7: the soft-depth unit
#include "gl/celllights.h"	// lane SUNCELL1: the cell effect law
#include "gl/cellfxlit.h"	// lane SUNCELL1: a lit particle's placed lights
#include "features/air/cellvolair.h"	// lane VOLPART1: volumetric particles
#include "gl/fxparity.h"	// lane FXPARITY1: Particle Game Rules
#include "gl/controllers.h"
#include "gl/lookdevstage.h"	// lane SUNCELL1: the weather fog
#include "gl/glproperty.h"
#include "gl/glscene.h"
#include "gl/renderer.h"
#include "features/shadows/sunshadow.h"	// lane VOLPART1: the sun's cascades on a puff
#include "glview.h"
#include "model/nifmodel.h"
#include <QFile>
#include <functional>
#include <algorithm>
#include <vector>

/* lane VOLPART1, gates only: WW_CELL_VOLPART_DUMP=<file>. After a volumetric system draws, it draws again into a
 * one-row float target, 8 pixels a particle at its centre (particles.geom / particles.frag vpDump), and appends one
 * block to <file>: the uniforms an independent rebuild needs, then a line a particle (its 8 RGBA probes). The placed
 * lights' records (8 texels a light, as the shader reads them) go to <file>.lights. The last frame's alone. */
namespace
{
constexpr int kVpDumpMax = 2048;	// particles a system's probe row holds

void vpDumpWrite( Scene * scene, NifSkopeOpenGLContext::Program * prog, const QString & path, const QString & name,
	int block, int count, int n, const GLint * vp, const std::vector<float> & px, const std::vector<float> * extra = nullptr );

void vpDumpSystem( Scene * scene, NifSkopeOpenGLContext::Program * prog, const QString & path, const QString & name,
	int block, int count )
{
	auto fn = scene->renderer->fn;
	static GLuint fbo = 0, tex = 0;
	const int n = std::min( count, kVpDumpMax ), W = n * 8;
	if ( !fbo ) {
		fn->glGenTextures( 1, &tex );
		fn->glBindTexture( GL_TEXTURE_2D, tex );
		fn->glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA32F, kVpDumpMax * 8, 1, 0, GL_RGBA, GL_FLOAT, nullptr );
		fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
		fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
		fn->glBindTexture( GL_TEXTURE_2D, 0 );
		fn->glGenFramebuffers( 1, &fbo );
		fn->glBindFramebuffer( GL_FRAMEBUFFER, fbo );
		fn->glFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0 );
	}
	GLint prevDraw = 0, prevRead = 0, vp[4] = { 0, 0, 1, 1 };
	fn->glGetIntegerv( GL_DRAW_FRAMEBUFFER_BINDING, &prevDraw );
	fn->glGetIntegerv( GL_READ_FRAMEBUFFER_BINDING, &prevRead );
	fn->glGetIntegerv( GL_VIEWPORT, vp );
	GLboolean cm[4];
	fn->glGetBooleanv( GL_COLOR_WRITEMASK, cm );
	const bool wasBlend = fn->glIsEnabled( GL_BLEND ), wasDepth = fn->glIsEnabled( GL_DEPTH_TEST );
	const bool wasStencil = fn->glIsEnabled( GL_STENCIL_TEST ), wasScissor = fn->glIsEnabled( GL_SCISSOR_TEST );
	fn->glBindFramebuffer( GL_FRAMEBUFFER, fbo );
	fn->glViewport( 0, 0, W, 1 );
	fn->glDisable( GL_BLEND );
	fn->glDisable( GL_DEPTH_TEST );
	fn->glDisable( GL_STENCIL_TEST );
	fn->glDisable( GL_SCISSOR_TEST );
	fn->glColorMask( GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE );
	const float clear[4] = { 0, 0, 0, 0 };
	fn->glClearBufferfv( GL_COLOR, 0, clear );
	prog->uni1i( "volPartDump", W );
	fn->glDrawArrays( GL_POINTS, 0, GLsizei( n ) );
	prog->uni1i( "volPartDump", 0 );
	std::vector<float> px( size_t( W ) * 4 );
	fn->glPixelStorei( GL_PACK_ALIGNMENT, 4 );
	fn->glReadPixels( 0, 0, W, 1, GL_RGBA, GL_FLOAT, px.data() );
	fn->glBindFramebuffer( GL_DRAW_FRAMEBUFFER, GLuint( prevDraw ) );
	fn->glBindFramebuffer( GL_READ_FRAMEBUFFER, GLuint( prevRead ) );
	fn->glViewport( vp[0], vp[1], vp[2], vp[3] );
	fn->glColorMask( cm[0], cm[1], cm[2], cm[3] );
	if ( wasBlend ) fn->glEnable( GL_BLEND );
	if ( wasDepth ) fn->glEnable( GL_DEPTH_TEST );
	if ( wasStencil ) fn->glEnable( GL_STENCIL_TEST );
	if ( wasScissor ) fn->glEnable( GL_SCISSOR_TEST );

	vpDumpWrite( scene, prog, path, name, block, count, n, vp, px );
}

/* the block a dumped draw appends (a particle system or, lane VOLPART1, an effect card's sampled pixels): the
 * uniforms an independent rebuild needs, then a line a probe (8 RGBA: px holds n x 32 floats); the file holds the
 * last frame alone, the placed lights' records go to <path>.lights once a frame */
void vpDumpWrite( Scene * scene, NifSkopeOpenGLContext::Program * prog, const QString & path, const QString & name,
	int block, int count, int n, const GLint * vp, const std::vector<float> & px, const std::vector<float> * extra )
{
	auto fn = scene->renderer->fn;
	auto uni = [&]( const QString & u, int cnt ) {
		float v[4] = { 0, 0, 0, 0 };
		// the GL's own lookup: the program's cache keys on the name's pointer, a temporary here
		const int l = fn->glGetUniformLocation( prog->id, u.toLatin1().constData() );
		if ( l >= 0 )
			fn->glGetUniformfv( prog->id, l, v );
		QStringList o;
		for ( int i = 0; i < cnt; i++ )
			o << QString::number( double( v[i] ), 'g', 9 );
		return u + QStringLiteral( " " ) + o.join( ' ' ) + QStringLiteral( "\n" );
	};
	auto uniI = [&]( const char * u ) {
		GLint v = 0;
		const int l = fn->glGetUniformLocation( prog->id, u );
		if ( l >= 0 )
			fn->glGetUniformiv( prog->id, l, &v );
		return QStringLiteral( "%1 %2\n" ).arg( QString::fromLatin1( u ) ).arg( v );
	};
	static int seq = 0;
	QString o = QStringLiteral( "system %1 block %2 seq %3 count %4 dumped %5\n" ).arg( name.isEmpty() ? QStringLiteral( "-" ) : name )
		.arg( block ).arg( seq++ ).arg( count ).arg( n );
	const auto & pm = scene->renderer->globalUniforms->projectionMatrix;
	o += QStringLiteral( "proj %1 %2 %3 %4\nviewport %5 %6 %7 %8\n" ).arg( double( pm[0][0] ), 0, 'g', 9 ).arg( double( pm[1][1] ), 0, 'g', 9 )
		.arg( double( pm[2][2] ), 0, 'g', 9 ).arg( double( pm[3][2] ), 0, 'g', 9 ).arg( vp[0] ).arg( vp[1] ).arg( vp[2] ).arg( vp[3] );
	for ( const char * u : { "fogView", "airMed", "volPhase[0]", "volPhase[1]", "volPhase[2]", "volGi", "cellRow[0]", "cellRow[1]",
			"cellRow[2]", "airLayer", "airGround", "fogK[0]", "fogK[1]", "fogK[2]", "fogK[3]", "fogK[4]", "fogK[5]", "fogSun",
			"fogSunColour" } )	// VOLPART1R: the landed air's layered law and the records' colour, for the check's rebuild
		o += uni( QString::fromLatin1( u ), 4 );
	for ( const char * u : { "cellDirColor", "cellDirTo" } )
		o += uni( QString::fromLatin1( u ), 3 );
	for ( const char * u : { "fogDistScale", "fxDistScale", "lightingInfluence" } )
		o += uni( QString::fromLatin1( u ), 1 );
	for ( const char * u : { "cellLightCount", "volTerms", "volPartRed", "volCsm", "cellShadowOn", "cellHasDir", "cellInterior",
			"cellGiOn", "fxLitMode", "fxAdditive", "fxSoft", "volOn", "fogOn", "cellRed", "volRed", "volVeil" } )
		o += uniI( u );
	for ( int i = 0; i < n; i++ ) {
		QStringList l;
		for ( int k = 0; k < 32; k++ )
			l << QString::number( double( px[size_t( i ) * 32 + size_t( k )] ), 'g', 9 );
		o += QStringLiteral( "p " ) + l.join( ' ' ) + QStringLiteral( "\n" );
		if ( extra && n > 0 ) {	// an effect card's slots 8.. (the card volume, the phase's two lights, the soft slab)
			const size_t st = extra->size() / size_t( n );
			QStringList e;
			for ( size_t k = 0; k < st; k++ )
				e << QString::number( double( ( *extra )[size_t( i ) * st + k] ), 'g', 9 );
			o += QStringLiteral( "q " ) + e.join( ' ' ) + QStringLiteral( "\n" );
		}
	}
	// the file holds the LAST frame alone (as the air's dump): a new frame starts it again, the lights with it
	static int lastFrame = -1;
	const bool fresh = wwVolAirFrameSerial() != lastFrame;
	lastFrame = wwVolAirFrameSerial();
	QFile f( path );
	if ( f.open( ( fresh ? QIODevice::WriteOnly | QIODevice::Truncate : QIODevice::Append ) | QIODevice::Text ) )
		f.write( QStringLiteral( "frame %1\n" ).arg( lastFrame ).toUtf8() + o.toUtf8() );

	// the placed lights' records, once a frame: the texture buffer the shader reads, through its own unit
	if ( !fresh )
		return;
	GLint unit = -1, prevActive = 0, tb = 0, buf = 0, prevBuf = 0, size = 0;
	const int lu = prog->uniLocation( "cellLights" );
	if ( lu < 0 )
		return;
	fn->glGetUniformiv( prog->id, lu, &unit );
	fn->glGetIntegerv( GL_ACTIVE_TEXTURE, &prevActive );
	fn->glActiveTexture( GLenum( GL_TEXTURE0 + unit ) );
	fn->glGetIntegerv( GL_TEXTURE_BINDING_BUFFER, &tb );
	if ( tb )
		fn->glGetTexLevelParameteriv( GL_TEXTURE_BUFFER, 0, GL_TEXTURE_BUFFER_DATA_STORE_BINDING, &buf );
	fn->glActiveTexture( GLenum( prevActive ) );
	std::vector<float> rec;
	if ( buf ) {
		fn->glGetIntegerv( GL_TEXTURE_BUFFER, &prevBuf );
		fn->glBindBuffer( GL_TEXTURE_BUFFER, GLuint( buf ) );
		fn->glGetBufferParameteriv( GL_TEXTURE_BUFFER, GL_BUFFER_SIZE, &size );
		rec.resize( size_t( size ) / sizeof( float ) );
		if ( !rec.empty() )
			fn->glGetBufferSubData( GL_TEXTURE_BUFFER, 0, GLsizeiptr( rec.size() * sizeof( float ) ), rec.data() );
		fn->glBindBuffer( GL_TEXTURE_BUFFER, GLuint( prevBuf ) );
	}
	QFile lf( path + QStringLiteral( ".lights" ) );
	if ( lf.open( QIODevice::WriteOnly | QIODevice::Text ) ) {
		QString t = QStringLiteral( "floats %1\n" ).arg( rec.size() );
		for ( size_t i = 0; i + 32 <= rec.size(); i += 32 ) {
			QStringList l;
			for ( size_t k = 0; k < 32; k++ )
				l << QString::number( double( rec[i + k] ), 'g', 9 );
			t += QStringLiteral( "light " ) + l.join( ' ' ) + QStringLiteral( "\n" );
		}
		lf.write( t.toUtf8() );
	}
}
}	// namespace

/* lane VOLPART1, gates only (WW_CELL_VOLPART_DUMP; cellvolair.h): a volumetric effect card drawn again 8 times into a
 * quarter-size float target with no depth test, probe slot k (cell_volpart.glsl vpProbe) at every pixel, and up to
 * kVpFxSamples of its covered pixels appended to the dump as "particles" (the same 8 RGBA a probe) */
void wwVolPartFxDump( Scene * scene, const std::function<void()> & draw, int block, const QString & name )
{
	WW_SWITCH_STATIC( QString, path, QString::fromLocal8Bit( qgetenv( "WW_CELL_VOLPART_DUMP" ) ) );
	if ( path.isEmpty() || !scene || !scene->renderer || !wwVolPartOn() )
		return;
	NifSkopeOpenGLContext::Program * prog = scene->renderer->getCurrentProgram();
	if ( !prog )
		return;
	constexpr int kVpFxSamples = 64;
	auto fn = scene->renderer->fn;
	GLint vp[4] = { 0, 0, 1, 1 };
	fn->glGetIntegerv( GL_VIEWPORT, vp );
	const int w = std::max( 1, vp[2] / 4 ), h = std::max( 1, vp[3] / 4 );
	static GLuint fbo = 0, tex = 0;
	static int tw = 0, th = 0;
	GLint prevDraw = 0, prevRead = 0, prevTex = 0;
	fn->glGetIntegerv( GL_DRAW_FRAMEBUFFER_BINDING, &prevDraw );
	fn->glGetIntegerv( GL_READ_FRAMEBUFFER_BINDING, &prevRead );
	if ( !fbo ) {
		fn->glGenTextures( 1, &tex );
		fn->glGenFramebuffers( 1, &fbo );
	}
	if ( tw != w || th != h ) {
		fn->glGetIntegerv( GL_TEXTURE_BINDING_2D, &prevTex );
		fn->glBindTexture( GL_TEXTURE_2D, tex );
		fn->glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA32F, w, h, 0, GL_RGBA, GL_FLOAT, nullptr );
		fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
		fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
		fn->glBindTexture( GL_TEXTURE_2D, GLuint( prevTex ) );
		fn->glBindFramebuffer( GL_FRAMEBUFFER, fbo );
		fn->glFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0 );
		tw = w;
		th = h;
	}
	GLboolean cm[4];
	fn->glGetBooleanv( GL_COLOR_WRITEMASK, cm );
	const bool wasBlend = fn->glIsEnabled( GL_BLEND ), wasDepth = fn->glIsEnabled( GL_DEPTH_TEST );
	const bool wasStencil = fn->glIsEnabled( GL_STENCIL_TEST ), wasScissor = fn->glIsEnabled( GL_SCISSOR_TEST );
	const bool wasCull = fn->glIsEnabled( GL_CULL_FACE ), wasSrgb = fn->glIsEnabled( GL_FRAMEBUFFER_SRGB );
	const bool wasMs = fn->glIsEnabled( GL_MULTISAMPLE );
	fn->glBindFramebuffer( GL_FRAMEBUFFER, fbo );
	fn->glViewport( 0, 0, w, h );
	fn->glDisable( GL_BLEND );
	fn->glDisable( GL_DEPTH_TEST );
	fn->glDisable( GL_STENCIL_TEST );
	fn->glDisable( GL_SCISSOR_TEST );
	fn->glDisable( GL_CULL_FACE );
	fn->glDisable( GL_FRAMEBUFFER_SRGB );
	fn->glDisable( GL_MULTISAMPLE );
	fn->glColorMask( GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE );
	constexpr int kSlots = 13;	// vpProbe's 8, then the card's 8..12 (fo4_effectshader.frag)
	constexpr int kEx = ( kSlots - 8 ) * 4;
	// the soft slab reads the opaque pass's depth at the full-size pixel this quarter-size one stands for
	const GLint scaleLoc = fn->glGetUniformLocation( prog->id, "vpDumpScale" );
	if ( scaleLoc >= 0 )
		fn->glUniform2f( scaleLoc, float( vp[2] ) / float( w ), float( vp[3] ) / float( h ) );
	std::vector<float> slot[kSlots];
	const float clear[4] = { 0, 0, 0, -1 };	// slot 7's alpha -1: no pixel of the card
	fn->glPixelStorei( GL_PACK_ALIGNMENT, 4 );
	for ( int k = 0; k < kSlots; k++ ) {
		fn->glClearBufferfv( GL_COLOR, 0, clear );
		prog->uni1i( "volPartDump", 100 + k );
		draw();
		slot[k].resize( size_t( w ) * size_t( h ) * 4 );
		fn->glReadPixels( 0, 0, w, h, GL_RGBA, GL_FLOAT, slot[k].data() );
	}
	prog->uni1i( "volPartDump", 0 );
	if ( scaleLoc >= 0 )
		fn->glUniform2f( scaleLoc, 0.0f, 0.0f );
	fn->glBindFramebuffer( GL_DRAW_FRAMEBUFFER, GLuint( prevDraw ) );
	fn->glBindFramebuffer( GL_READ_FRAMEBUFFER, GLuint( prevRead ) );
	fn->glViewport( vp[0], vp[1], vp[2], vp[3] );
	fn->glColorMask( cm[0], cm[1], cm[2], cm[3] );
	if ( wasBlend ) fn->glEnable( GL_BLEND );
	if ( wasDepth ) fn->glEnable( GL_DEPTH_TEST );
	if ( wasStencil ) fn->glEnable( GL_STENCIL_TEST );
	if ( wasScissor ) fn->glEnable( GL_SCISSOR_TEST );
	if ( wasCull ) fn->glEnable( GL_CULL_FACE );
	if ( wasSrgb ) fn->glEnable( GL_FRAMEBUFFER_SRGB );
	if ( wasMs ) fn->glEnable( GL_MULTISAMPLE );

	std::vector<int> cov;
	for ( int i = 0; i < w * h; i++ )
		if ( slot[7][size_t( i ) * 4 + 3] >= 0.0f )
			cov.push_back( i );
	if ( cov.empty() )
		return;
	const int n = std::min( int( cov.size() ), kVpFxSamples );
	std::vector<float> px( size_t( n ) * 32 ), ex( size_t( n ) * kEx );
	for ( int j = 0; j < n; j++ ) {
		const size_t i = size_t( cov[size_t( j ) * cov.size() / size_t( n )] );
		for ( int k = 0; k < 8; k++ )
			std::copy( &slot[k][i * 4], &slot[k][i * 4] + 4, &px[size_t( j ) * 32 + size_t( k ) * 4] );
		for ( int k = 8; k < kSlots; k++ )
			std::copy( &slot[k][i * 4], &slot[k][i * 4] + 4, &ex[size_t( j ) * kEx + size_t( k - 8 ) * 4] );
	}
	const GLint svp[4] = { 0, 0, w, h };
	vpDumpWrite( scene, prog, path, QStringLiteral( "fx:" ) + name, block, int( cov.size() ), n, svp, px, &ex );
}

void wwVolPartDumpSystem( Scene * scene, NifSkopeOpenGLContext::Program * prog, const QString & path,
	const QString & name, int block, int count )
{
	vpDumpSystem( scene, prog, path, name, block, count );
}
