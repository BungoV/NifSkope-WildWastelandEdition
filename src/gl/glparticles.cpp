/***** BEGIN LICENSE BLOCK *****

BSD License

Copyright (c) 2005-2015, NIF File Format Library and Tools
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:
1. Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.
3. The name of the NIF File Format Library and Tools project may not be
   used to endorse or promote products derived from this software
   without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

***** END LICENCE BLOCK *****/

#include "glparticles.h"
#include "wwswitch.h"	// lane TESTDRIVER1: the WW_* switch store

#include <cmath>

#include <QRegularExpression>
#include <QSet>

#include <cstdio>

#include "gl/cellhdr.h"	// lane SUNCELL1: the HDR frame's stencil
#include "gl/cellqueue.h"	// lane GPUFIX1
#include "gl/celllights.h"	// lane SUNCELL1: the cell effect law
#include "gl/cellfxlit.h"	// lane SUNCELL1: a lit particle's placed lights
#include "gl/cellvolair.h"	// lane VOLPART1: volumetric particles
#include "gl/fxparity.h"	// lane FXPARITY1: Particle Game Rules
#include "gl/controllers.h"
#include "gl/lookdevstage.h"	// lane SUNCELL1: the weather fog
#include "gl/glproperty.h"
#include "gl/glscene.h"
#include "gl/renderer.h"
#include "gl/sunshadow.h"	// lane VOLPART1: the sun's cascades on a puff
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
constexpr int kVpFxDepthUnit = 9;	// the opaque pass's depth (sampler2D); the cell's units are 10..22
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
			"cellGiOn", "fxLitMode", "fxAdditive", "fxSoft", "volOn", "fogOn", "cellRed", "volRed" } )
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


/*
 *  Particle
 */

void Particles::clear()
{
	Node::clear();

	verts.clear();
	colors.clear();
	sizes.clear();
	uvOffsets.clear();
	uvCell = Vector2( 1.0f, 1.0f );
	angles.clear();
}

void Particles::updateImpl( const NifModel * nif, const QModelIndex & index )
{
	Node::updateImpl( nif, index );

	if ( index == iBlock ) {
		for (const auto link : nif->getChildLinks(id())) {
			QModelIndex iChild = nif->getBlockIndex(link);

			if (!iChild.isValid())
				continue;

			if (nif->blockInherits(iChild, "NiParticlesData")) {
				iData = iChild;
				updateData = true;
			}
		}

		// FO4 particle systems link the shader/alpha directly on the block
		// rather than through the old NiProperty list, so pick them up here
		iShaderProp = nif->getBlockIndex( nif->getLink( iBlock, "Shader Property" ) );
		iAlphaProp = nif->getBlockIndex( nif->getLink( iBlock, "Alpha Property" ) );
	}

	if ( index == iData )
		updateData = true;
}

void Particles::setController( const NifModel * nif, const QModelIndex & index )
{
	auto contrName = nif->itemName(index);
	if ( contrName == "NiParticleSystemController" || contrName == "NiBSPArrayController" ) {
		Controller * ctrl = new ParticleController( this, index );
		registerController(nif, ctrl);
	} else if ( contrName == "NiPSysUpdateCtlr" ) {
		// modern (FO4 era) particle systems store no vertices in the file;
		// the update controller drives a preview-grade CPU simulation instead
		Controller * ctrl = new PSysSimController( this, index );
		registerController(nif, ctrl);
	} else if ( contrName.startsWith( QLatin1String( "NiPSys" ) ) || contrName.startsWith( QLatin1String( "BSPSys" ) ) ) {
		// emitter/modifier controllers are read by the simulator directly
	} else {
		Node::setController( nif, index );
	}
}

void Particles::transform()
{
	auto nif = NifModel::fromValidIndex(iBlock);
	if ( !nif ) {
		clear();
		return;
	}

	if ( updateData ) {
		updateData = false;

		verts  = nif->getArray<Vector3>( nif->getIndex( iData, "Vertices" ) );
		colors = nif->getArray<Color4>( nif->getIndex( iData, "Vertex Colors" ) );
		sizes  = nif->getArray<float>( nif->getIndex( iData, "Sizes" ) );

		active = nif->get<int>( iData, "Num Valid" );
		size = nif->get<float>( iData, "Active Radius" );
	}

	Node::transform();
}

int Particles::wwPreroll( float from, float to, float step )
{
	int steps = 0;
	for ( Controller * c : controllers ) {
		auto * sim = dynamic_cast<PSysSimController *>( c );
		if ( !sim || !( step > 0.0f ) || !( to > from ) )
			continue;
		sim->updateTime( from + 1.0e6f );	// then a backward jump to `from`: the simulation restarts empty
		sim->updateTime( from );
		for ( float t = from + step; t < to; t += step, steps++ )
			sim->updateTime( t );
		sim->updateTime( to );
		steps++;
	}
	return steps;
}

void Particles::transformShapes()
{
	Node::transformShapes();
}

BoundSphere Particles::bounds() const
{
	BoundSphere sphere( verts );
	sphere.radius += size;
	return worldTrans() * sphere | Node::bounds();
}

Vector2 Particles::spriteUVCell()
{
	// Per-particle offsets mean the simulator already chose the cells; then only
	// the cell SIZE is global.
	if ( uvOffsets.size() >= verts.size() && !uvOffsets.isEmpty() )
		return uvCell;

	const NifModel * nif = scene ? scene->nifModel : nullptr;
	int grid = 0;
	if ( nif && iData.isValid() && nif->getIndex( iData, "Num Subtexture Offsets" ).isValid() ) {
		int nSub = nif->get<int>( iData, "Num Subtexture Offsets" );
		if ( nSub >= 4 )
			grid = int( std::lround( std::sqrt( double( nSub ) ) ) );
	}
	if ( grid < 2 ) {
		// fall back to an NxN hint in the texture file name (e.g. _4x4)
		QString srcTex;
		if ( BSShaderLightingProperty * sp = findProperty<BSShaderLightingProperty>() )
			srcTex = sp->fileName( 0 );
		if ( srcTex.isEmpty() && nif && iShaderProp.isValid()
			&& nif->blockInherits( iShaderProp, "BSEffectShaderProperty" ) ) {
			srcTex = nif->get<QString>( iShaderProp, "Source Texture" );
		}
		QRegularExpressionMatch mm = QRegularExpression( "(\\d+)x(\\d+)" ).match( srcTex );
		if ( mm.hasMatch() )
			grid = mm.captured( 1 ).toInt();
	}
	if ( grid >= 2 ) {
		const float sc = 1.0f / float( grid );
		return Vector2( sc, sc );
	}
	return Vector2( 1.0f, 1.0f );
}

/* The sprite quads as static world-space triangles.
 *
 * This mirrors particles.geom corner for corner. The GPU is handed ONE POINT per
 * particle and expands it in eye space, which is why the sprites always face the
 * camera and why nothing in the NIF describes their geometry: it does not exist
 * until the vertex reaches the GPU. A bake has to build it, and pick a facing,
 * because a still cannot turn.
 */
bool Particles::bakeSprites( const Vector3 & viewAxis, QVector<Vector3> & tris,
                             QVector<Vector2> & uvs, QVector<Color4> & cols )
{
	tris.clear();
	uvs.clear();
	cols.clear();

	if ( isHidden() || verts.isEmpty() || active < 1 )
		return false;

	// Only the first `active` are alive — the tail of the array is dead storage
	// the draw call never reaches, and baking it would scatter stale sprites.
	const qsizetype n = std::min<qsizetype>( active, verts.size() );
	if ( n < 1 )
		return false;

	// particles.geom adds the corner offsets in EYE space, so a bake needs two
	// world axes standing in for screen right and screen up.
	Vector3 fwd = viewAxis;
	if ( fwd.length() < 1.0e-4f )
		fwd = Vector3( 0.0f, -1.0f, 0.0f );
	fwd.normalize();
	Vector3 upRef = ( std::fabs( fwd[2] ) < 0.9f ) ? Vector3( 0.0f, 0.0f, 1.0f )
	                                               : Vector3( 1.0f, 0.0f, 0.0f );
	Vector3 right = Vector3::crossproduct( upRef, fwd );
	right.normalize();
	Vector3 up = Vector3::crossproduct( fwd, right );
	up.normalize();

	const Transform & wt = worldTrans();
	const float baseSize = size * wt.scale;
	const Vector2 cell = spriteUVCell();

	tris.reserve( n * 6 );
	uvs.reserve( n * 6 );
	cols.reserve( n * 6 );

	for ( qsizetype i = 0; i < n; i++ ) {
		// particleSize defaults to 1 on the GPU when the attribute is absent
		const float ps = ( sizes.size() > i ) ? sizes.at( i ) : 1.0f;
		const float s = ps * baseSize;
		if ( !( s > 0.0f ) )
			continue;

		const Vector3 c = wt * verts.at( i );
		const float a = ( angles.size() > i ) ? angles.at( i ) : 0.0f;
		const float ca = std::cos( a );
		const float sa = std::sin( a );

		const Vector3 c11 = c + right * ( (  ca - sa ) * s ) + up * ( (  sa + ca ) * s );
		const Vector3 c01 = c + right * ( ( -ca - sa ) * s ) + up * ( ( -sa + ca ) * s );
		const Vector3 c10 = c + right * ( (  ca + sa ) * s ) + up * ( (  sa - ca ) * s );
		const Vector3 c00 = c + right * ( ( -ca + sa ) * s ) + up * ( ( -sa - ca ) * s );

		const Vector2 o = ( uvOffsets.size() > i ) ? uvOffsets.at( i ) : Vector2( 0.0f, 0.0f );
		const Vector2 u11( o[0] + cell[0], o[1] + cell[1] );
		const Vector2 u01( o[0],           o[1] + cell[1] );
		const Vector2 u10( o[0] + cell[0], o[1] );
		const Vector2 u00( o[0],           o[1] );

		const Color4 col = ( colors.size() > i ) ? colors.at( i )
		                                        : Color4( 1.0f, 1.0f, 1.0f, 1.0f );

		// the strip 11,01,10,00 as two triangles, keeping GL's winding
		tris << c11 << c01 << c10;
		uvs  << u11 << u01 << u10;
		tris << c10 << c01 << c00;
		uvs  << u10 << u01 << u00;
		for ( int k = 0; k < 6; k++ )
			cols << col;
	}
	return !tris.isEmpty();
}

void Particles::drawShapes( NodeList * secondPass )
{
	if ( isHidden() || scene->selecting > (unsigned char) Scene::SelObject || !scene->renderer || !scene->nifModel
		|| verts.isEmpty() || active < 1 ) {
		return;
	}

	// display option: hide particle systems entirely (still pickable so the
	// block can be selected in the viewport)
	if ( !scene->showParticles && !scene->selecting )
		return;

	const NifModel * nif = scene->nifModel;

	// FO4 particles are transparent VFX (they carry a shader / alpha property
	// linked directly): draw them in the second (alpha-blended) pass
	if ( ( iAlphaProp.isValid() || iShaderProp.isValid() ) && secondPass ) {
		secondPass->add( this );
		return;
	}

	AlphaProperty * aprop = findProperty<AlphaProperty>();

	if ( aprop && aprop->hasAlphaBlend() && secondPass ) {
		secondPass->add( this );
		return;
	}
	// lane GPUFIX1: while the walk fills the render queues every shape waits for the flush, so tree order holds
	if ( secondPass && GpuDriven::mainTake( this, GpuDriven::Rest ) )
		return;

	/* lane SUNCELL1: in the cell view a particle draws with the cell's effect law (particles_cell.prog: linear, the
	 * fog, the HDR frame or the imagespace), as the game draws its particles with the effect shader. Red
	 * WW_CELL_PARTICLES_RED=viewer keeps the viewer's program (sRGB as written, no fog, no stencil). */
	WW_SWITCH_STATIC( bool, cellPfxRed, wwEnv( "WW_CELL_PARTICLES_RED" ).trimmed() == "viewer" );
	const bool cellPfx = !scene->selecting && !cellPfxRed && wwCellLightsWanted( scene );
	// lane VOLPART1, gates only: WW_CELL_VOLPART_HIDE=1 draws no cell particle (the plate the hard-edge gate subtracts)
	WW_SWITCH_STATIC( bool, vpHide, qEnvironmentVariableIntValue( "WW_CELL_VOLPART_HIDE" ) == 1 );	// lane LIVESET1: follows a live set
	if ( cellPfx && vpHide )
		return;
	bool volPart = false, vpAlways = false;
	auto	prog = scene->renderer->useProgram( scene->selecting ? "selection.prog"
		: cellPfx ? "particles_cell.prog" : "particles.prog" );
	if ( !prog )
		return;

	prog->uni4m( "modelViewMatrix", viewTrans().toMatrix4() );

	if ( scene->selecting ) {
		prog->uni1i( "selectionFlags", 0x0001 );
		prog->uni1i( "selectionParam", scene->nifModel->getBlockNumber( iBlock ) );
		glPointSize( GLView::Settings::vertexSelectPointSize );
		glDisable( GL_BLEND );

	} else {
		float	s2 = size * worldTrans().scale;
		prog->uni2f( "particleScale", s2, s2 );

		// the shader property (added to this node by Node::updateImpl for FO4)
		// resolves and binds the source texture through the normal path
		BSShaderLightingProperty * shaderProp = findProperty<BSShaderLightingProperty>();
		QString	srcTex = shaderProp ? shaderProp->fileName( 0 ) : QString();
		if ( srcTex.isEmpty() && iShaderProp.isValid() ) {
			if ( nif->blockInherits( iShaderProp, "BSEffectShaderProperty" ) )
				srcTex = nif->get<QString>( iShaderProp, "Source Texture" );
		}

		// atlas sheets (e.g. T_..._4x4.dds) pack many frames: show a single cell
		// instead of squashing the whole sheet onto every sprite. When the
		// simulator supplies per-particle cell offsets, only the cell size goes
		// through the uniform and the offsets ride a vertex attribute.
		// through spriteUVCell so the bake cannot pick a different cell than the
		// preview shows
		const Vector2	cell = spriteUVCell();
		prog->uni4f( "particleUV", FloatVector4( 0.0f, 0.0f, cell[0], cell[1] ) );

		// lane FXPARITY1: Particle Game Rules (fxparity.h), the shader's part: 1 the effect law, 2 the soft fade
		const int fxRules = wwFxParityRules();
		prog->uni1i( "pfxGame", ( ( fxRules & WwFxLaw ) ? 1 : 0 ) | ( ( fxRules & WwFxSoft ) ? 2 : 0 ) );

		// setup blending: honour the linked NiAlphaProperty (FO4 electricity is
		// additive: SRC_ALPHA, ONE), else fall back to additive
		AlphaProperty::glProperty( aprop, prog );
		if ( !( aprop && aprop->hasAlphaBlend() ) ) {
			static const GLenum blendMap[16] = {
				GL_ONE, GL_ZERO, GL_SRC_COLOR, GL_ONE_MINUS_SRC_COLOR, GL_DST_COLOR,
				GL_ONE_MINUS_DST_COLOR, GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_DST_ALPHA,
				GL_ONE_MINUS_DST_ALPHA, GL_SRC_ALPHA_SATURATE, GL_ONE, GL_ONE, GL_ONE, GL_ONE, GL_ONE };
			glEnable( GL_BLEND );
			glDisable( GL_ALPHA_TEST );
			int flags = iAlphaProp.isValid() ? nif->get<int>( iAlphaProp, "Flags" ) : 0;
			if ( iAlphaProp.isValid() && ( flags & 1 ) )
				glBlendFunc( blendMap[( flags >> 1 ) & 0xf], blendMap[( flags >> 5 ) & 0xf] );
			else
				glBlendFunc( GL_SRC_ALPHA, GL_ONE );
		}

		if ( cellPfx ) {
			// lane SUNCELL1: the cell's uniforms (fog first: the cell's own fog indoors overrides the weather's),
			// additive from the blend just set, and the frame's stencil as Renderer::setupProgram marks an effect
			GLint dst = 0;
			glGetIntegerv( GL_BLEND_DST_RGB, &dst );
			const bool additive = glIsEnabled( GL_BLEND ) && dst == GL_ONE;
			wwLookdevFogUniforms( scene );
			wwCellLightsUniforms( scene );
			prog->uni1b( "fxAdditive", additive );
			/* lane SUNCELL1 (last round): A LIT PARTICLE. The game's technique sets Lit from the property's
			 * lighting flag beside Ptcl, so a system the cell registered as a lit effect (src/cellview.cpp, the
			 * particles) takes its model's four placed lights, as a lit effect card does (FXLIT1). fxLitMode stays 0
			 * for any other system. Red WW_CELL_PARTICLES_RED=unlit draws every particle unlit, as before. */
			WW_SWITCH_STATIC( bool, litRed, wwEnv( "WW_CELL_PARTICLES_RED" ).trimmed() == "unlit" );
			const int litBlock = scene->nifModel->getBlockNumber( iBlock );
			const auto * espLit = dynamic_cast<const BSEffectShaderProperty *>( shaderProp );
			wwCellFxLitUniforms( scene, litBlock );
			prog->uni1f( "lightingInfluence", espLit ? espLit->lightingInfluence : 0.0f );
			if ( litRed )
				prog->uni1i( "fxLitMode", 0 );
			const bool litNow = !litRed && wwCellFxLitFor( scene->nifModel, litBlock ) != nullptr;
			/* lane VOLPART1: volumetric particles (cellvolair.h). The sun's cascades and the air's medium on this
			 * program, the opaque pass's depth for the soft ball; off: volPart 0 and nothing else read */
			wwSunShadowUniforms( scene );
			volPart = wwVolPartUniforms( scene );
			{
				/* lane FXPARITY1: under the game's soft rule a particle whose effect shader is Soft fades near the
				 * opaque surface behind it, as an effect card does (fo4_effectshader.frag cellFxFade) */
				const auto * espSoft = dynamic_cast<const BSEffectShaderProperty *>( shaderProp );
				const bool gameSoft = !volPart && ( fxRules & WwFxSoft ) && espSoft && espSoft->soft
					&& espSoft->falloff.softDepth > 0.0f;
				const bool soft = ( volPart || gameSoft ) && scene->grabEffectDepth();
				vpAlways = volPart && soft && !( wwVolPartRed() & 4 );	// the ball's fade is the occlusion
				prog->uni1b( "fxSoft", soft );
				prog->uni1f( "fxSoftDepth", gameSoft ? std::max( espSoft->falloff.softDepth, 1.0f ) : 1.0f );
				GLint prevActive = 0;
				glGetIntegerv( GL_ACTIVE_TEXTURE, &prevActive );
				scene->renderer->fn->glActiveTexture( GLenum( GL_TEXTURE0 + kVpFxDepthUnit ) );
				glBindTexture( GL_TEXTURE_2D, soft ? scene->fxDepthTexId : 0 );
				scene->renderer->fn->glActiveTexture( GLenum( prevActive ) );
				prog->uni1i( "fxDepth", kVpFxDepthUnit );
				if ( volPart && ( wwVolPartRed() & 64 ) )
					prog->uni1b( "cellShadowOn", false );	// red noshadow: the placed lights' cubes too
				static QString vpSaid;
				const QString vpNow = wwVolPartSummary() + QStringLiteral( " soft=%1" ).arg( soft ? 1 : 0 );
				if ( vpNow != vpSaid ) {
					vpSaid = vpNow;
					fprintf( stderr, "cell particles volumetric: %s\n", qPrintable( vpNow ) );
				}
			}
			const char * frame = "plain";
			if ( wwCellImageSpaceIsMeasuring() ) {
				const bool fx = wwCellImageSpaceMeasuresEffects();
				glColorMask( fx, fx, fx, fx );
				glDepthMask( GL_FALSE );
				glEnable( GL_STENCIL_TEST );
				glStencilMask( 0x02 );
				glStencilFunc( GL_ALWAYS, 2, 0xFF );
				glStencilOp( GL_KEEP, GL_KEEP, GL_REPLACE );
				frame = "measure";
			} else if ( wwCellHdrActive() ) {
				glEnable( GL_STENCIL_TEST );
				glStencilMask( 0x03 );
				glStencilFunc( GL_ALWAYS, 1, 0xFF );
				glStencilOp( GL_KEEP, GL_KEEP, GL_REPLACE );
				frame = "hdr linear";
			}
			// telemetry: the systems drawn this way, said at 1, 2, 4, 8, ... distinct systems
			static QSet<const void *> drawn;
			if ( !drawn.contains( this ) ) {
				drawn.insert( this );
				const int n = int( drawn.size() );
				if ( ( n & ( n - 1 ) ) == 0 )
					fprintf( stderr, "cell particles drawn: %d systems so far with particles_cell.prog (latest %s: %d live, %s, %s frame)\n",
						n, qPrintable( getName() ), active, additive ? "additive" : "blended", frame );
			}
			// telemetry: the lit systems drawn, said at 1, 2, 4, 8, ... (or the red, once)
			static QSet<const void *> litDrawn;
			if ( litNow && !litDrawn.contains( this ) ) {
				litDrawn.insert( this );
				const int n = int( litDrawn.size() );
				if ( ( n & ( n - 1 ) ) == 0 )
					fprintf( stderr, "cell particles lit: %d systems so far take their model's placed lights (latest %s, influence %.3f)\n",
						n, qPrintable( getName() ), espLit ? double( espLit->lightingInfluence ) : 0.0 );
			}
			static bool litRedSaid = false;
			if ( litRed && !litRedSaid ) {
				litRedSaid = true;
				fprintf( stderr, "cell particles lit: off (red WW_CELL_PARTICLES_RED=unlit)\n" );
			}
			/* lane SUNCELL1 final: the vertex ALPHA takes the 2.2 too. Every particle pixel shader of the game (lit
			 * Ptcl|Lit|Pma asm 00613, unlit 00604 / 00903) opens with log/mul 2.2/exp on v2.xyzw, so the colour AND
			 * the alpha of the vertex colour are raised to 2.2 before the texture multiply. Red
			 * WW_CELL_PARTICLES_RED=linalpha keeps the vertex alpha as written, as before. */
			WW_SWITCH_STATIC( bool, alphaRed, wwEnv( "WW_CELL_PARTICLES_RED" ).trimmed() == "linalpha" );
			prog->uni1b( "pfxAlphaRed", alphaRed );
			static bool alphaSaid = false;
			if ( !alphaSaid ) {
				alphaSaid = true;
				fprintf( stderr, "cell particles alpha: %s\n", alphaRed
					? "vertex alpha as written (red WW_CELL_PARTICLES_RED=linalpha)"
					: "texture alpha x vertex alpha^2.2, as the game's particle shaders" );
			}
		}

		// setup vertex colors

		VertexColorProperty::glProperty( nullptr, FloatVector4( colors.size() < verts.size() ? 1.0f : 0.0f ), prog );

		// setup material

		MaterialProperty::glProperty( findProperty<MaterialProperty>(), findProperty<SpecularProperty>(), prog );
		prog->uni4f( "frontMaterialEmission", FloatVector4( 0.0f ) );

		// setup texturing: the sprite texture always goes on texture unit 0
		// (the previous code bound through the shader property WITHOUT
		// activating a unit first, so the texture landed on whatever unit the
		// last drawn shape left active and the sampler read a stale texture)

		for ( int i = 0; i < TexturingProperty::numTextures; i++ ) {
			prog->uni1i_l( prog->uniLocation( "textureUnits[%d]", i ), 0 );
			prog->uni1i_l( prog->uniLocation( "textures[%d].textureUnit", i ), 0 );
		}

		bool	bound = false;

		if ( auto p = findProperty<TexturingProperty>(); p )
			bound = p->bind( 0, 0, prog );	// legacy path sets its own uniforms

		if ( !bound ) {
			scene->textures->activateTextureUnit( 0 );
			if ( shaderProp )
				bound = shaderProp->bind( 0 );
			if ( !bound && !srcTex.isEmpty() )
				bound = scene->textures->bind( srcTex, nif );
			if ( !bound ) {
				static const QString	defaultTexture = "#FFFFFFFF";
				scene->textures->bind( defaultTexture, scene->nifModel );
			}
			prog->uni1i_l( prog->uniLocation( "textureUnits[%d]", 0 ), 0 );
			prog->uni2f_l( prog->uniLocation( "textures[%d].uvCenter", 0 ), 0.5f, 0.5f );
			prog->uni2f_l( prog->uniLocation( "textures[%d].uvScale", 0 ), 1.0f, 1.0f );
			prog->uni2f_l( prog->uniLocation( "textures[%d].uvOffset", 0 ), 0.0f, 0.0f );
			prog->uni1f_l( prog->uniLocation( "textures[%d].uvRotation", 0 ), 0.0f );
			prog->uni1i_l( prog->uniLocation( "textures[%d].coordSet", 0 ), 0 );
			// the fragment shader samples textureUnits[textureUnit - 1]
			prog->uni1i_l( prog->uniLocation( "textures[%d].textureUnit", 0 ), 1 );
		}

		// effect-shader features from the linked BSEffectShaderProperty (.bgem):
		// normal-map driven falloff, greyscale-to-palette gradients and cube-map
		// reflection - flat sprites otherwise miss all of these in preview
		prog->uni1i( "hasNormalMap", 0 );
		prog->uni1i( "hasGreyscaleMap", 0 );
		prog->uni1i( "hasCubeMap", 0 );
		prog->uni1i( "greyscaleColor", 0 );
		prog->uni1i( "greyscaleAlpha", 0 );
		prog->uni1i( "useFalloff", 0 );
		prog->uni1i( "hasRGBFalloff", 0 );
		prog->uni4f( "glowColor", FloatVector4( 1.0f, 1.0f, 1.0f, 1.0f ) );
		prog->uni1f( "glowMult", 1.0f );
		prog->uni4f( "falloffParams", FloatVector4( 1.0f, 0.0f, 1.0f, 1.0f ) );
		prog->uni1f( "envReflection", 0.0f );

		// the CubeMap sampler must always point at its own complete texture
		// unit: a samplerCube sharing a unit with a sampler2D is a GL error at
		// draw time, so bind a neutral gray cube on unit 3 up front (the real
		// environment map, if any, overwrites this binding below)
		scene->textures->activateTextureUnit( 3 );
		scene->bindCube( "#FF555555c", 1 );
		prog->uni1i( "CubeMap", 3 );
		scene->textures->activateTextureUnit( 0 );

		if ( auto esp = dynamic_cast<BSEffectShaderProperty *>( shaderProp ) ) {
			// emissive tint: an unset / near-black emissive would zero out the
			// sprite, so fall back to white (plain base texture) in that case
			FloatVector4	gc( esp->emissiveColor );
			float	gm = esp->emissiveMult;
			// lane FXPARITY1: the game takes the emissive as written (no white fallback, no clamp), as for a card
			if ( !( fxRules & WwFxLaw ) && gc[0] + gc[1] + gc[2] < 0.02f ) {
				gc = FloatVector4( 1.0f, 1.0f, 1.0f, gc[3] );
				gm = 1.0f;
			}
			prog->uni4f( "glowColor", gc );
			prog->uni1f( "glowMult", ( fxRules & WwFxLaw ) ? gm : std::min( std::max( gm, 0.0f ), 4.0f ) );

			prog->uni1i( "greyscaleColor", esp->greyscaleColor );
			prog->uni1i( "greyscaleAlpha", esp->greyscaleAlpha );
			prog->uni1i( "useFalloff", esp->useFalloff );
			prog->uni1i( "hasRGBFalloff", esp->hasRGBFalloff );
			prog->uni4f( "falloffParams", FloatVector4( esp->falloff.startAngle, esp->falloff.stopAngle,
														esp->falloff.startOpacity, esp->falloff.stopOpacity ) );

			// normal map -> texture unit 1
			if ( esp->hasNormalMap && scene->hasOption( Scene::DoLighting ) ) {
				QString	nmName = esp->fileName( 3 );
				if ( !nmName.isEmpty() ) {
					scene->textures->activateTextureUnit( 1 );
					if ( scene->textures->bind( nmName, nif ) ) {
						prog->uni1i( "NormalMap", 1 );
						prog->uni1i( "hasNormalMap", 1 );
					}
				}
			}

			// greyscale palette -> texture unit 2
			if ( esp->hasGreyscaleMap && ( esp->greyscaleColor || esp->greyscaleAlpha ) ) {
				QString	gsName = esp->fileName( 1 );
				if ( !gsName.isEmpty() ) {
					scene->textures->activateTextureUnit( 2 );
					if ( scene->textures->bind( gsName, nif ) ) {
						prog->uni1i( "GreyscaleMap", 2 );
						prog->uni1i( "hasGreyscaleMap", 1 );
					}
				}
			}

			// environment cube map -> texture unit 3
			if ( esp->hasEnvironmentMap && scene->hasOption( Scene::DoCubeMapping )
				 && scene->hasOption( Scene::DoLighting ) ) {
				QString	cubeName = esp->fileName( 2 );
				scene->textures->activateTextureUnit( 3 );
				if ( !cubeName.isEmpty() && scene->bindCube( cubeName ) ) {
					prog->uni1i( "CubeMap", 3 );
					prog->uni1i( "hasCubeMap", 1 );
					prog->uni1f( "envReflection", esp->environmentReflection );
				}
			}

			// leave unit 0 (the sprite base map) active for the draw
			scene->textures->activateTextureUnit( 0 );
		}
	}

	// setup z buffer

	ZBufferProperty::glProperty( findProperty<ZBufferProperty>() );

	// setup stencil

	StencilProperty::glProperty( findProperty<StencilProperty>() );

	// wireframe ?

#if 0
	WireframeProperty::glProperty( findProperty<WireframeProperty>() );
#endif

	// render the particles

	// the emissive tint and greyscale palette are now applied in the fragment
	// shader (glowColor / GreyscaleMap uniforms), so the raw per-particle
	// vertex colours are handed to the shader unchanged

	qsizetype	numVerts = verts.size();
	const float *	attrData[5] = { &( verts.constFirst()[0] ), nullptr, nullptr, nullptr, nullptr };
	unsigned int	attrMask = 0x03;
	if ( colors.size() >= numVerts ) {
		attrData[1] = &( colors.constFirst()[0] );
		attrMask = attrMask | 0x40;
	}
	if ( uvOffsets.size() >= numVerts ) {
		// per-particle flipbook cell offset (vec2 at location 2)
		attrData[2] = &( uvOffsets.constFirst()[0] );
		attrMask = attrMask | 0x200;
	}
	if ( angles.size() >= numVerts ) {
		// per-particle sprite rotation angle (float at location 3)
		attrData[3] = angles.constData();
		attrMask = attrMask | 0x1000;
	}
	if ( sizes.size() >= numVerts ) {
		attrData[4] = sizes.constData();
		attrMask = attrMask | 0x00010000;
	}
	scene->renderer->bindShape( (unsigned int) numVerts, attrMask, 0, attrData, nullptr );

	if ( active < numVerts )
		numVerts = active;

	// transparent sprites must not write depth: they would punch square
	// holes into transparent geometry drawn after them
	if ( !scene->selecting )
		glDepthMask( GL_FALSE );
	/* lane VOLPART1: a soft puff draws with no depth test: its fade (the share of its ball in front of the opaque
	 * depth) is the occlusion, so it thins to nothing where it meets a surface instead of being cut by it */
	GLint prevDepthFunc = GL_LESS;
	if ( vpAlways ) {
		glGetIntegerv( GL_DEPTH_FUNC, &prevDepthFunc );
		glDepthFunc( GL_ALWAYS );
	}
	scene->renderer->fn->glDrawArrays( GL_POINTS, 0, GLsizei( numVerts ) );
	if ( vpAlways )
		glDepthFunc( GLenum( prevDepthFunc ) );
	if ( !scene->selecting )
		glDepthMask( GL_TRUE );
	// lane VOLPART1, gates only: the per-particle probe (WW_CELL_VOLPART_DUMP)
	WW_SWITCH_STATIC( QString, vpDump, QString::fromLocal8Bit( qgetenv( "WW_CELL_VOLPART_DUMP" ) ) );
	if ( volPart && !vpDump.isEmpty() && numVerts > 0 )
		vpDumpSystem( scene, prog, vpDump, getName(), scene->nifModel->getBlockNumber( iBlock ), int( numVerts ) );
}
