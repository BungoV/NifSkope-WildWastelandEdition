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

#include "renderer.h"

#include "message.h"
#include "nifskope.h"
#include "gl/glshape.h"
#include "gl/glproperty.h"
#include "gl/glscene.h"
#include "gl/gltex.h"
#include "gl/scenelighting.h"
#include "gl/lookdevstage.h"
#include "esmweather.h"
#include "io/material.h"
#include "model/nifmodel.h"
#include "ui/settingsdialog.h"
#include "gl/BSMesh.h"
#include "libfo76utils/src/ddstxt16.hpp"
#include "glview.h"

#include <limits>
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QOpenGLContext>
#include <QSet>
#include <QSettings>
#include <QTextStream>
#include <chrono>


//! @file renderer.cpp Renderer and child classes implementation

static const QString white = "#FFFFFFFF";
static const QString black = "#FF000000";
static const QString lighting = "#FF00F040";
static const QString reflectivity = "#FF0A0A0A";
static const QString gray = "#FF808080s";
static const QString magenta = "#FFFF00FF";
static const QString default_n = "#FFFF8080";
static const QString default_ns = "#FFFF8080n";
static const QString cube_sk = "textures/cubemaps/bleakfallscube_e.dds";
static const QString cube_fo4 = "textures/shared/cubemaps/mipblur_defaultoutside1.dds";
static const QString grayCube = "#FF555555c";
static const QString pbr_lut_sf = "#sfpbr.dds";


Renderer::Renderer( QOpenGLContext * c )
	: NifSkopeOpenGLContext( c )
{
	updateSettings();

	connect( NifSkope::getOptions(), &SettingsDialog::saveSettings, this, &Renderer::updateSettings );
}

Renderer::~Renderer()
{
}


void Renderer::updateSettings()
{
	QSettings settings;

	int	tmp = settings.value( "Settings/Render/General/Mesh Cache Size", 128 ).toInt();
	cfg.meshCacheSize = std::uint8_t( std::clamp< int >( ( tmp + 4 ) >> 3, 1, 128 ) );
	tmp = settings.value( "Settings/Render/General/Cube Map Bgnd", 1 ).toInt();
	globalUniforms->cubeBgndMipLevel = std::clamp< int >( tmp, -1, 6 );
	tmp = settings.value( "Settings/Render/General/Sf Parallax Steps", 200 ).toInt();
	globalUniforms->sfParallaxMaxSteps = std::clamp< int >( tmp, 16, 512 );
	globalUniforms->sfParallaxScale = settings.value( "Settings/Render/General/Sf Parallax Scale", 0.0f).toFloat();
	globalUniforms->sfParallaxOffset = settings.value( "Settings/Render/General/Sf Parallax Offset", 0.5f).toFloat();
	cfg.cubeMapPathFO76 = settings.value( "Settings/Render/General/Cube Map Path FO 76", "textures/shared/cubemaps/mipblur_defaultoutside1.dds" ).toString();
	cfg.cubeMapPathSTF = settings.value( "Settings/Render/General/Cube Map Path STF", "textures/cubemaps/cell_cityplazacube.dds" ).toString();
	setCacheSize( std::uint32_t( cfg.meshCacheSize ) << 23 );
	TexCache::loadSettings( settings );
}

/* WW_PROGRAM_CENSUS -- which program actually lights which shape.
 *
 * `fo4_default.prog` excludes Shader Type 18 and `sk_msn.prog`'s conditions
 * look like they match an FO4 .BTR, so which of the two lights the legacy
 * terrain cannot be settled by reading the condition files: it is decided at
 * runtime by the scan order in setupProgram below.  This writes the answer
 * down instead of inferring it -- one line per first-sighted (shape, program)
 * pair, plus the view-space light direction the lighting arithmetic uses.
 *
 * Armed only by an ABSOLUTE path in WW_PROGRAM_CENSUS; it renders nothing and
 * touches no uniform, so a frame is byte-identical whether it is armed or not.
 */
void wwPbrmCensusNoteUploadF0( float f0 );	// src/gl/glproperty.cpp

static NifSkopeOpenGLContext::Program * wwProgramCensus( const NifModel * nif, Shape * mesh,
	const BSShaderLightingProperty * wwSp, const char * wwKind,
	int msn, int lodLand, NifSkopeOpenGLContext::Program * program,
	const FloatVector4 & lightViewDir )
{
	/* WW_PBRM_CENSUS (lane PBRR0) rides the same exits: every return of
	 * setupProgram passes through here with the program it actually bound,
	 * so the route it prints is the served one. Pick renders (wwKind null) are skipped. */
	if ( wwPbrmCensusArmed() && mesh && wwKind ) {
		QString	served( "(none)" );
		if ( program )
			served = QString::fromLatin1( program->name.data(), qsizetype( program->name.length() ) );
		// Read the uploaded pbrF0 back from the program (lane PBRR1): the row's
		// f0= is what the GPU holds, not what the law says it should be.
		float	f0 = std::numeric_limits<float>::quiet_NaN();
		if ( program && served == QLatin1StringView( "pbrm_default.prog" ) ) {
			const int	l = program->uniLocation( "pbrF0" );
			if ( l >= 0 ) {
				GLfloat	v = -1.0f;
				program->f->glGetUniformfv( program->id, l, &v );
				f0 = v;
			}
		}
		wwPbrmCensusNoteUploadF0( f0 );
		wwPbrmCensus( mesh->getName(), wwKind, wwSp, served );
		wwPbrmCensusNoteUploadF0( std::numeric_limits<float>::quiet_NaN() );
	}

	static int	armed = -1;
	static QString	censusPath;
	if ( armed < 0 ) {
		QString	p = QString::fromLocal8Bit( qgetenv( "WW_PROGRAM_CENSUS" ) ).trimmed();
		armed = ( !p.isEmpty() && QDir::isAbsolutePath( p ) ) ? 1 : 0;
		censusPath = p;
	}
	if ( !armed )
		return program;

	QString	progName( "(none)" );
	if ( program )
		progName = QString::fromLatin1( program->name.data(), qsizetype( program->name.length() ) );

	QString	row = QString( "shape=\"%1\" bsver=%2 msn=%3 lodland=%4 prog=%5" )
		.arg( mesh ? mesh->getName() : QString( "(null)" ) )
		.arg( nif ? int( nif->getBSVersion() ) : -1 )
		.arg( msn ).arg( lodLand ).arg( progName );

	static QSet<QString>	seen;
	if ( seen.contains( row ) )
		return program;
	bool	first = seen.isEmpty();
	seen.insert( row );

	QFile	f( censusPath );
	if ( f.open( QIODevice::Append | QIODevice::Text ) ) {
		QTextStream	s( &f );
		if ( first ) {
			s << "# WW_PROGRAM_CENSUS  light(view) = "
			  << lightViewDir[0] << " " << lightViewDir[1] << " " << lightViewDir[2] << "\n";
		}
		s << row << "\n";
	}
	return program;
}

/* sRGB tag (lane PBRR2A, 2026-09-24). The PBR program samples an sRGB-tagged
 * base or emissive texture with the hardware decode SKIPPED
 * (GL_EXT_texture_sRGB_decode) and decodes it in the shader exactly as it does
 * a UNORM one, so both tags take one path -- filter, then decode -- and render
 * byte-identically. Letting the hardware decode the tagged one (decode, then
 * filter) left up to 3/255 on minified texels: measured, gate srgbtag. The
 * texture object is shared with the legacy programs, which expect the decode,
 * so every skip is undone at the start of the next shape's program setup. */
namespace {
QVector<QPair<const void *, GLuint>> & wwSkippedDecode()
{
	static QVector<QPair<const void *, GLuint>> v;
	return v;
}

void wwRestoreSrgbDecode()
{
	auto & v = wwSkippedDecode();
	if ( v.isEmpty() )
		return;
	const void * ctx = QOpenGLContext::currentContext();
	GLint prev = 0;
	glGetIntegerv( GL_TEXTURE_BINDING_2D, &prev );
	for ( int i = int( v.size() ) - 1; i >= 0; i-- ) {
		if ( v[i].first != ctx )
			continue;
		if ( glIsTexture( v[i].second ) ) {
			glBindTexture( GL_TEXTURE_2D, v[i].second );
			glTexParameteri( GL_TEXTURE_2D, 0x8A48, 0x8A49 );	// TEXTURE_SRGB_DECODE_EXT = DECODE_EXT
		}
		v.remove( i );
	}
	glBindTexture( GL_TEXTURE_2D, GLuint( prev ) );
}
} // namespace

NifSkopeOpenGLContext::Program * Renderer::setupProgram( Shape * mesh, Program * hint )
{
	wwRestoreSrgbDecode();
	const NifModel *	nif = mesh->scene->nifModel;

	/* Read here, not inside wwProgramCensus: `Shape::bslsp` is protected and
	 * only a Shape's friends -- Renderer's own members -- may read it. */
	const int	wwMsn = ( mesh->bslsp
		? int( mesh->bslsp->hasSF1( ShaderFlags::SLSF1_Model_Space_Normals ) ) : 0 );
	const int	wwLodLand = ( mesh->bslsp
		? int( mesh->bslsp->isST( ShaderFlags::ST_WorldMap4 ) ) : 0 );
	const BSShaderLightingProperty *	wwSp = mesh->bssp;
	// nullptr during a pick render: the PBRM census skips those.
	const char *	wwKind = mesh->scene->selecting ? nullptr
		: ( mesh->bslsp ? "lit" : ( mesh->bsesp ? "effect" : "other" ) );

	if ( nif == nullptr || nif->getBSVersion() == 0 ) {
		useProgram( "default.prog" );
		setupFixedFunction( mesh );
		return wwProgramCensus( nif, mesh, wwSp, wwKind, wwMsn, wwLodLand, currentProgram,
		globalUniforms->lightSourcePosition[0] );
	}

	// PBRM route. Whether a shape uses it depends on a runtime material
	// resolution and a UI mode, neither of which a .prog condition can express
	// (those evaluate NIF data), so it is selected by name here rather than by
	// the condition scan below. Checked before the hint so switching modes cannot
	// leave a shape on a cached program.
	//   Legacy         : never
	//   PBR            : always — shapes without a PBRM are driven from their
	//                    legacy material so the whole scene sits under one BRDF
	//   Legacy and PBR : only where a PBRM resolved; PBR overrides legacy there
	/* PBR route view (lane PBRR1): a DATA view, chosen by name ahead of every
	 * shading route, for FO4 lighting and effect shapes only. */
	if ( pbrmRouteView() && wwSp && wwLodChannelView == 0
		&& nif->getBSVersion() >= 130 && nif->getBSVersion() < 140 ) {
		if ( Program * program = useProgram( "pbr_route.prog" ) ) {
			routeProgramSeen = program;
			if ( setupProgramRoute( nif, program, mesh ) )
				return wwProgramCensus( nif, mesh, wwSp, wwKind, wwMsn, wwLodLand, program,
				globalUniforms->lightSourcePosition[0] );
			stopProgram();
		}
	}

	const int lightingMode = pbrmMode();
	/* The LOD channel preview is a DATA view, not a shading mode: it draws one
	 * generated vertex channel flat. Routing it through the PBRM program shows
	 * PBR shading instead, because that program has no preview branch -- which
	 * is why every terrain channel rendered byte-identical while the object
	 * ones differed: terrain LOD resolves a baked PBRM and objects do not. */
	const bool wantPbrm = mesh->bslsp && wwLodChannelView == 0
		&& ( lightingMode == PbrmModePBR
			|| ( lightingMode == PbrmModeLegacyAndPBR && mesh->bslsp->pbrmValid ) );
	if ( wantPbrm ) {
		if ( Program * program = useProgram( "pbrm_default.prog" ) ) {
			pbrmProgramSeen = program;
			if ( setupProgramPBRM( nif, program, mesh ) )
				return wwProgramCensus( nif, mesh, wwSp, wwKind, wwMsn, wwLodLand, program,
				globalUniforms->lightSourcePosition[0] );
			stopProgram();
		}
	}

	/* The LOD channel preview lives in fo4_default.frag and nowhere else, and
	 * fo4_default.prog's conditions exclude Shader Type 18 - LOD landscape.
	 * So a TERRAIN chunk never reached a program that had the uniform: not
	 * through the hint, not through the condition scan, in any lighting mode.
	 * Measured before this: one chunk rendered top-down under channels 0, 3
	 * and 6, flat and lit, six images byte-identical in a fresh process where
	 * no hint could have been cached. Objects are a different shader type and
	 * always did vary, which is what made the cache look like the cause.
	 *
	 * The preview is a flat data view - texture layout, the reason type 18
	 * has its own path, does not matter to it - so while it is on, every
	 * FO4 lighting-shader shape goes to fo4_default.prog by name, the way the
	 * PBRM route above is chosen by name. */
	if ( wwLodChannelView != 0 && mesh->bslsp
		&& nif->getBSVersion() >= 130 && nif->getBSVersion() <= 139 ) {
		if ( Program * program = useProgram( "fo4_default.prog" ) ) {
			if ( setupProgramCE1( nif, program, mesh ) )
				return wwProgramCensus( nif, mesh, wwSp, wwKind, wwMsn, wwLodLand, program,
				globalUniforms->lightSourcePosition[0] );
			stopProgram();
		}
	}

	/* The hint is the program the shape drew with LAST frame, and the only
	 * thing that ever cleared it was a NIF-block edit (Shape::updateImpl).
	 * So a terrain shape that resolved a PBRM on its first, ordinary paint
	 * handed pbrm_default back as its hint on every later frame - including
	 * the frames after the LOD channel preview was switched on, when the
	 * verdict above had already said "not PBRM". That program has no
	 * lodChannelView uniform, the uniform write is a silent no-op, and every
	 * terrain channel rendered byte-identical while objects - never PBRM -
	 * varied. Gating the NEW selection on the preview (above) did not touch
	 * the cached one; this does. */
	/* Reaching this line means neither the PBR route nor the route view served
	 * the shape THIS frame -- including a PBR binding aborted on a texture
	 * failure (lane PBRR1), where wantPbrm is still true -- so a hint naming
	 * either of those programs is stale. */
	const bool stalePbrmHint = hint
		&& ( ( pbrmProgramSeen && hint == pbrmProgramSeen ) || ( routeProgramSeen && hint == routeProgramSeen ) );
	if ( hint && hint->status && !stalePbrmHint ) [[likely]] {
		Program * program = hint;
		fn->glUseProgram( program->id );
		currentProgram = program;
		bool	setupStatus;
		if ( nif->getBSVersion() >= 170 )
			setupStatus = setupProgramCE2( nif, program, mesh );
		else if ( nif->getBSVersion() >= 83 )
			setupStatus = setupProgramCE1( nif, program, mesh );
		else
			setupStatus = setupProgramFO3( nif, program, mesh );
		if ( setupStatus )
			return wwProgramCensus( nif, mesh, wwSp, wwKind, wwMsn, wwLodLand, program,
				globalUniforms->lightSourcePosition[0] );
		stopProgram();
	}

	QVector<QModelIndex> iBlocks;
	iBlocks << mesh->index();
	iBlocks << mesh->iData;
	{
		PropertyList props;
		mesh->activeProperties( props );

		for ( Property * p : props ) {
			iBlocks.append( p->index() );
		}
	}

	for ( Program * program = programsLinked; program; program = program->nextProgram ) {
		if ( !program->conditions.isEmpty() && program->conditions.eval( nif, iBlocks ) ) {
			fn->glUseProgram( program->id );
			currentProgram = program;
			bool	setupStatus;
			if ( nif->getBSVersion() >= 170 )
				setupStatus = setupProgramCE2( nif, program, mesh );
			else if ( nif->getBSVersion() >= 83 )
				setupStatus = setupProgramCE1( nif, program, mesh );
			else
				setupStatus = setupProgramFO3( nif, program, mesh );
			if ( setupStatus )
				return wwProgramCensus( nif, mesh, wwSp, wwKind, wwMsn, wwLodLand, program,
				globalUniforms->lightSourcePosition[0] );
			stopProgram();
		}
	}

	useProgram( "default.prog" );
	setupFixedFunction( mesh );
	return wwProgramCensus( nif, mesh, wwSp, wwKind, wwMsn, wwLodLand, currentProgram,
		globalUniforms->lightSourcePosition[0] );
}

static int setFlipbookParameters( const CE2Material::Material & m, FloatVector4 & uvScaleAndOffset )
{
	int	flipbookColumns = std::min< int >( m.flipbookColumns, 127 );
	int	flipbookRows = std::min< int >( m.flipbookRows, 127 );
	int	flipbookFrames = flipbookColumns * flipbookRows;
	if ( flipbookFrames < 2 )
		return 0;
	float	flipbookFPMS = std::min( std::max( m.flipbookFPS, 1.0f ), 100.0f ) * 0.001f;
	double	flipbookFrame = double( std::chrono::duration_cast< std::chrono::milliseconds >( std::chrono::steady_clock::now().time_since_epoch() ).count() );
	flipbookFrame = flipbookFrame * flipbookFPMS / double( flipbookFrames );
	flipbookFrame = flipbookFrame - std::floor( flipbookFrame );
	int	n = std::min< int >( int( flipbookFrame * double( flipbookFrames ) ), flipbookFrames - 1 );
	uvScaleAndOffset += FloatVector4( 0.0f, 0.0f, float(n % flipbookColumns), float(n / flipbookColumns) );
	float	w = float( flipbookColumns );
	float	h = float( flipbookRows );
	uvScaleAndOffset /= FloatVector4( w, h, w, h );
	return 4;
}

static inline void setupGLBlendModeSF( int blendMode, NifSkopeOpenGLContext::GLFunctions * fn )
{
	// source RGB, destination RGB, source alpha, destination alpha
	static const GLenum blendModeMap[32] = {
		GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA,	// AlphaBlend
		GL_SRC_ALPHA, GL_ONE, GL_ONE, GL_ONE_MINUS_SRC_ALPHA,	// Additive
		GL_SRC_ALPHA, GL_ONE, GL_ONE, GL_ONE_MINUS_SRC_ALPHA,	// SourceSoftAdditive (alpha is squared in the shader)
		GL_DST_COLOR, GL_ZERO, GL_DST_ALPHA, GL_ZERO,	// Multiply
		GL_SRC_ALPHA, GL_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA,	// DestinationSoftAdditive
		GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA,	// TODO: DestinationInvertedSoftAdditive
		GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA,	// TODO: TakeSmaller
		GL_ZERO, GL_ONE, GL_ZERO, GL_ONE	// None
	};
	const GLenum *	p = &( blendModeMap[blendMode << 2] );
	fn->glEnable( GL_BLEND );
	fn->glBlendFuncSeparate( p[0], p[1], p[2], p[3] );
}

bool Renderer::setupProgramCE2( const NifModel * nif, Program * prog, Shape * mesh )
{
	auto scene = mesh->scene;
	auto lsp = mesh->bssp;
	if ( !lsp )
		return false;

	const CE2Material *	mat = nullptr;
	bool	useErrorColor = false;
	if ( !lsp->getSFMaterial( mat, nif ) )
		useErrorColor = scene->hasOption(Scene::DoErrorColor);
	if ( !mat )
		return false;

	mesh->depthWrite = true;
	mesh->depthTest = true;
	bool	isEffect = ( (mat->flags & CE2Material::Flag_IsEffect) && mat->shaderRoute != 0 );
	if ( isEffect ) {
		mesh->depthWrite = bool(mat->effectSettings->flags & CE2Material::EffectFlag_ZWrite);
		mesh->depthTest = bool(mat->effectSettings->flags & CE2Material::EffectFlag_ZTest);
	}

	// texturing

	int texunit = 0;

	// Always bind cube to texture units 0 (specular) and 1 (diffuse),
	// regardless of shader settings
	bool hasCubeMap = scene->hasOption(Scene::DoCubeMapping) && scene->hasOption(Scene::DoLighting);
	GLint uniCubeMap = prog->uniLocation( "CubeMap" );
	if ( uniCubeMap < 0 )
		return false;
	fn->glActiveTexture( GL_TEXTURE0 + texunit );
	hasCubeMap = hasCubeMap && scene->bindCube( cfg.cubeMapPathSTF );
	if ( !hasCubeMap ) [[unlikely]]
		scene->bindCube( grayCube, 1 );
	fn->glUniform1i( uniCubeMap, texunit++ );

	uniCubeMap = prog->uniLocation( "CubeMap2" );
	if ( uniCubeMap < 0 )
		return false;
	fn->glActiveTexture( GL_TEXTURE0 + texunit );
	hasCubeMap = hasCubeMap && scene->bindCube( cfg.cubeMapPathSTF, 2 );
	if ( !hasCubeMap ) [[unlikely]]
		scene->bindCube( grayCube, 1 );
	fn->glUniform1i( uniCubeMap, texunit++ );

	prog->uni1i( "hasCubeMap", hasCubeMap );

	// texture unit 2 is reserved for the environment BRDF LUT texture
	fn->glActiveTexture( GL_TEXTURE0 + texunit );
	if ( !lsp->bind( pbr_lut_sf, true, TexClampMode::CLAMP_S_CLAMP_T ) )
		return false;
	texunit++;

	static const std::string_view	emptyTexturePath = "";

	prog->uni1i( "hasSpecular", int(scene->hasOption(Scene::DoSpecular)) );
	prog->uni1i( "lm.shaderModel", mat->shaderModel );

	// emissive settings
	if ( mat->flags & CE2Material::Flag_LayeredEmissivity && scene->hasOption(Scene::DoGlow) ) {
		const CE2Material::LayeredEmissiveSettings *	sp = mat->layeredEmissiveSettings;
		prog->uni1b( "lm.layeredEmissivity.isEnabled", sp->isEnabled );
		prog->uni1i( "lm.layeredEmissivity.firstLayerIndex", sp->layer1Index );
		prog->uni4c( "lm.layeredEmissivity.firstLayerTint", sp->layer1Tint, true );
		prog->uni1i( "lm.layeredEmissivity.firstLayerMaskIndex", sp->layer1MaskIndex );
		prog->uni1i( "lm.layeredEmissivity.secondLayerIndex", ( sp->layer2Active ? int(sp->layer2Index) : -1 ) );
		prog->uni4c( "lm.layeredEmissivity.secondLayerTint", sp->layer2Tint, true );
		prog->uni1i( "lm.layeredEmissivity.secondLayerMaskIndex", sp->layer2MaskIndex );
		prog->uni1i( "lm.layeredEmissivity.firstBlenderIndex", sp->blender1Index );
		prog->uni1i( "lm.layeredEmissivity.firstBlenderMode", sp->blender1Mode );
		prog->uni1i( "lm.layeredEmissivity.thirdLayerIndex", ( sp->layer3Active ? int(sp->layer3Index) : -1 ) );
		prog->uni4c( "lm.layeredEmissivity.thirdLayerTint", sp->layer3Tint, true );
		prog->uni1i( "lm.layeredEmissivity.thirdLayerMaskIndex", sp->layer3MaskIndex );
		prog->uni1i( "lm.layeredEmissivity.secondBlenderIndex", sp->blender2Index );
		prog->uni1i( "lm.layeredEmissivity.secondBlenderMode", sp->blender2Mode );
		prog->uni1f( "lm.layeredEmissivity.emissiveClipThreshold", sp->clipThreshold );
		prog->uni1b( "lm.layeredEmissivity.adaptiveEmittance", sp->adaptiveEmittance );
		prog->uni1f( "lm.layeredEmissivity.luminousEmittance", sp->luminousEmittance );
		prog->uni1f( "lm.layeredEmissivity.exposureOffset", sp->exposureOffset );
		prog->uni1b( "lm.layeredEmissivity.enableAdaptiveLimits", sp->enableAdaptiveLimits );
		prog->uni1f( "lm.layeredEmissivity.maxOffsetEmittance", sp->maxOffset );
		prog->uni1f( "lm.layeredEmissivity.minOffsetEmittance", sp->minOffset );
	}	else {
		prog->uni1b( "lm.layeredEmissivity.isEnabled", false );
	}
	if ( mat->flags & CE2Material::Flag_Emissive && scene->hasOption(Scene::DoGlow) ) {
		const CE2Material::EmissiveSettings *	sp = mat->emissiveSettings;
		prog->uni1b( "lm.emissiveSettings.isEnabled", sp->isEnabled );
		prog->uni1i( "lm.emissiveSettings.emissiveSourceLayer", sp->sourceLayer );
		prog->uni4srgb( "lm.emissiveSettings.emissiveTint", sp->emissiveTint );
		prog->uni1i( "lm.emissiveSettings.emissiveMaskSourceBlender", sp->maskSourceBlender );
		prog->uni1f( "lm.emissiveSettings.emissiveClipThreshold", sp->clipThreshold );
		prog->uni1b( "lm.emissiveSettings.adaptiveEmittance", sp->adaptiveEmittance );
		prog->uni1f( "lm.emissiveSettings.luminousEmittance", sp->luminousEmittance );
		prog->uni1f( "lm.emissiveSettings.exposureOffset", sp->exposureOffset );
		prog->uni1b( "lm.emissiveSettings.enableAdaptiveLimits", sp->enableAdaptiveLimits );
		prog->uni1f( "lm.emissiveSettings.maxOffsetEmittance", sp->maxOffset );
		prog->uni1f( "lm.emissiveSettings.minOffsetEmittance", sp->minOffset );
	}	else {
		prog->uni1b( "lm.emissiveSettings.isEnabled", false );
	}

	// translucency settings
	if ( mat->flags & CE2Material::Flag_Translucency ) {
		const CE2Material::TranslucencySettings *	sp = mat->translucencySettings;
		prog->uni1b( "lm.translucencySettings.isEnabled", sp->isEnabled );
		prog->uni1b( "lm.translucencySettings.isThin", sp->isThin );
		prog->uni1b( "lm.translucencySettings.flipBackFaceNormalsInViewSpace", sp->flipBackFaceNormalsInVS );
		prog->uni1b( "lm.translucencySettings.useSSS", sp->useSSS );
		prog->uni1f( "lm.translucencySettings.sssWidth", sp->sssWidth );
		prog->uni1f( "lm.translucencySettings.sssStrength", sp->sssStrength );
		prog->uni1f( "lm.translucencySettings.transmissiveScale", sp->transmissiveScale );
		prog->uni1f( "lm.translucencySettings.transmittanceWidth", sp->transmittanceWidth );
		prog->uni1f( "lm.translucencySettings.specLobe0RoughnessScale", sp->specLobe0RoughnessScale );
		prog->uni1f( "lm.translucencySettings.specLobe1RoughnessScale", sp->specLobe1RoughnessScale );
		prog->uni1i( "lm.translucencySettings.transmittanceSourceLayer", sp->sourceLayer );
	} else {
		prog->uni1b( "lm.translucencySettings.isEnabled", false );
	}

	// decal settings
	if ( mat->flags & CE2Material::Flag_IsDecal ) {
		const CE2Material::DecalSettings *	sp = mat->decalSettings;
		prog->uni1b( "lm.decalSettings.isDecal", sp->isDecal );
		prog->uni1f( "lm.decalSettings.materialOverallAlpha", sp->decalAlpha );
		prog->uni1i( "lm.decalSettings.writeMask", int(sp->writeMask) );
		prog->uni1b( "lm.decalSettings.isPlanet", sp->isPlanet );
		prog->uni1b( "lm.decalSettings.isProjected", sp->isProjected );
		prog->uni1b( "lm.decalSettings.useParallaxOcclusionMapping", sp->useParallaxMapping );
		FloatVector4	replUniform( 0.0f );
		int	texUniform = lsp->getSFTexture( texunit, replUniform, *(sp->surfaceHeightMap), 0, 0, nullptr );
		prog->uni1i( "lm.decalSettings.surfaceHeightMap", texUniform );
		prog->uni1f( "lm.decalSettings.parallaxOcclusionScale", sp->parallaxOcclusionScale );
		prog->uni1b( "lm.decalSettings.parallaxOcclusionShadows", sp->parallaxOcclusionShadows );
		prog->uni1i( "lm.decalSettings.maxParralaxOcclusionSteps", sp->maxParallaxSteps );
		prog->uni1i( "lm.decalSettings.renderLayer", sp->renderLayer );
		prog->uni1b( "lm.decalSettings.useGBufferNormals", sp->useGBufferNormals );
		prog->uni1i( "lm.decalSettings.blendMode", sp->blendMode );
		prog->uni1b( "lm.decalSettings.animatedDecalIgnoresTAA", sp->animatedDecalIgnoresTAA );
	} else {
		prog->uni1b( "lm.decalSettings.isDecal", false );
	}

	// effect settings
	prog->uni1b( "lm.isEffect", isEffect );
	prog->uni1b( "lm.hasOpacityComponent", ( isEffect && (mat->flags & CE2Material::Flag_HasOpacityComponent) ) );
	int	layeredEdgeFalloffFlags = 0;
	if ( isEffect ) {
		const CE2Material::EffectSettings *	sp = mat->effectSettings;
		if ( mat->flags & CE2Material::Flag_LayeredEdgeFalloff )
			layeredEdgeFalloffFlags = mat->layeredEdgeFalloff->activeLayersMask & 0x07;
		prog->uni1b( "lm.effectSettings.vertexColorBlend", bool(sp->flags & CE2Material::EffectFlag_VertexColorBlend) );
		// these settings appear to be unused, effects are always alpha tested with a threshold of 1/128
#if 0
		prog->uni1b( "lm.effectSettings.isAlphaTested", bool(sp->flags & CE2Material::EffectFlag_IsAlphaTested) );
		prog->uni1f( "lm.effectSettings.alphaTestThreshold", sp->alphaThreshold );
#endif
		prog->uni1b( "lm.effectSettings.noHalfResOptimization", bool(sp->flags & CE2Material::EffectFlag_NoHalfResOpt) );
		prog->uni1b( "lm.effectSettings.softEffect", bool(sp->flags & CE2Material::EffectFlag_SoftEffect) );
		prog->uni1f( "lm.effectSettings.softFalloffDepth", sp->softFalloffDepth );
		prog->uni1b( "lm.effectSettings.emissiveOnlyEffect", bool(sp->flags & CE2Material::EffectFlag_EmissiveOnly) );
		prog->uni1b( "lm.effectSettings.emissiveOnlyAutomaticallyApplied", bool(sp->flags & CE2Material::EffectFlag_EmissiveOnlyAuto) );
		prog->uni1b( "lm.effectSettings.receiveDirectionalShadows", bool(sp->flags & CE2Material::EffectFlag_DirShadows) );
		prog->uni1b( "lm.effectSettings.receiveNonDirectionalShadows", bool(sp->flags & CE2Material::EffectFlag_NonDirShadows) );
		prog->uni1b( "lm.effectSettings.isGlass", bool(sp->flags & CE2Material::EffectFlag_IsGlass) );
		prog->uni1b( "lm.effectSettings.frosting", bool(sp->flags & CE2Material::EffectFlag_Frosting) );
		prog->uni1f( "lm.effectSettings.frostingUnblurredBackgroundAlphaBlend", sp->frostingBgndBlend );
		prog->uni1f( "lm.effectSettings.frostingBlurBias", sp->frostingBlurBias );
		prog->uni1f( "lm.effectSettings.materialOverallAlpha", sp->materialAlpha );
		prog->uni1b( "lm.effectSettings.zTest", bool(sp->flags & CE2Material::EffectFlag_ZTest) );
		prog->uni1b( "lm.effectSettings.zWrite", bool(sp->flags & CE2Material::EffectFlag_ZWrite) );
		prog->uni1i( "lm.effectSettings.blendingMode", sp->blendMode );
		prog->uni1b( "lm.effectSettings.backLightingEnable", bool(sp->flags & CE2Material::EffectFlag_BacklightEnable) );
		prog->uni1f( "lm.effectSettings.backlightingScale", sp->backlightScale );
		prog->uni1f( "lm.effectSettings.backlightingSharpness", sp->backlightSharpness );
		prog->uni1f( "lm.effectSettings.backlightingTransparencyFactor", sp->backlightTransparency );
		prog->uni4f( "lm.effectSettings.backLightingTintColor", sp->backlightTintColor );
		prog->uni1b( "lm.effectSettings.depthMVFixup", bool(sp->flags & CE2Material::EffectFlag_MVFixup) );
		prog->uni1b( "lm.effectSettings.depthMVFixupEdgesOnly", bool(sp->flags & CE2Material::EffectFlag_MVFixupEdgesOnly) );
		prog->uni1b( "lm.effectSettings.forceRenderBeforeOIT", bool(sp->flags & CE2Material::EffectFlag_RenderBeforeOIT) );
		prog->uni1i( "lm.effectSettings.depthBiasInUlp", sp->depthBias );
		// opacity component
		if ( mat->flags & CE2Material::Flag_HasOpacityComponent ) {
			prog->uni1i( "lm.opacity.firstLayerIndex", mat->opacityLayer1 );
			prog->uni1b( "lm.opacity.secondLayerActive", bool(mat->flags & CE2Material::Flag_OpacityLayer2Active) );
			if ( mat->flags & CE2Material::Flag_OpacityLayer2Active ) {
				prog->uni1i( "lm.opacity.secondLayerIndex", mat->opacityLayer2 );
				prog->uni1i( "lm.opacity.firstBlenderIndex", mat->opacityBlender1 );
				prog->uni1i( "lm.opacity.firstBlenderMode", mat->opacityBlender1Mode );
			}
			prog->uni1b( "lm.opacity.thirdLayerActive", bool(mat->flags & CE2Material::Flag_OpacityLayer3Active) );
			if ( mat->flags & CE2Material::Flag_OpacityLayer3Active ) {
				prog->uni1i( "lm.opacity.thirdLayerIndex", mat->opacityLayer3 );
				prog->uni1i( "lm.opacity.secondBlenderIndex", mat->opacityBlender2 );
				prog->uni1i( "lm.opacity.secondBlenderMode", mat->opacityBlender2Mode );
			}
			prog->uni1f( "lm.opacity.specularOpacityOverride", mat->specularOpacityOverride );
		}
	}
	if ( layeredEdgeFalloffFlags ) {
		const CE2Material::LayeredEdgeFalloff *	sp = mat->layeredEdgeFalloff;
		prog->uni1fv( "lm.layeredEdgeFalloff.falloffStartAngles", sp->falloffStartAngles, 3 );
		prog->uni1fv( "lm.layeredEdgeFalloff.falloffStopAngles", sp->falloffStopAngles, 3 );
		prog->uni1fv( "lm.layeredEdgeFalloff.falloffStartOpacities", sp->falloffStartOpacities, 3 );
		prog->uni1fv( "lm.layeredEdgeFalloff.falloffStopOpacities", sp->falloffStopOpacities, 3 );
		if ( sp->useRGBFalloff )
			layeredEdgeFalloffFlags = layeredEdgeFalloffFlags | 0x80;
	}
	prog->uni1i( "lm.layeredEdgeFalloff.flags", layeredEdgeFalloffFlags );

	// alpha settings
	if ( mat->flags & CE2Material::Flag_HasOpacity ) {
		prog->uni1b( "lm.alphaSettings.hasOpacity", true );
		prog->uni1f( "lm.alphaSettings.alphaTestThreshold", mat->alphaThreshold );
		prog->uni1i( "lm.alphaSettings.opacitySourceLayer", mat->alphaSourceLayer );
		prog->uni1i( "lm.alphaSettings.alphaBlenderMode", mat->alphaBlendMode );
		prog->uni1b( "lm.alphaSettings.useDetailBlendMask", bool(mat->flags & CE2Material::Flag_AlphaDetailBlendMask) );
		prog->uni1b( "lm.alphaSettings.useVertexColor", bool(mat->flags & CE2Material::Flag_AlphaVertexColor) );
		prog->uni1i( "lm.alphaSettings.vertexColorChannel", mat->alphaVertexColorChannel );
		const CE2Material::UVStream *	uvStream = mat->alphaUVStream;
		if ( !uvStream )
			uvStream = &CE2Material::defaultUVStream;
		prog->uni4f( "lm.alphaSettings.opacityUVstream.scaleAndOffset", uvStream->scaleAndOffset );
		prog->uni1b( "lm.alphaSettings.opacityUVstream.useChannelTwo", (uvStream->channel > 1) );
		prog->uni1f( "lm.alphaSettings.heightBlendThreshold", mat->alphaHeightBlendThreshold );
		prog->uni1f( "lm.alphaSettings.heightBlendFactor", mat->alphaHeightBlendFactor );
		prog->uni1f( "lm.alphaSettings.position", mat->alphaPosition );
		prog->uni1f( "lm.alphaSettings.contrast", mat->alphaContrast );
		prog->uni1b( "lm.alphaSettings.useDitheredTransparency", bool(mat->flags & CE2Material::Flag_DitheredTransparency) );
	} else {
		prog->uni1b( "lm.alphaSettings.hasOpacity", false );
	}

	// detail blender settings
	if ( ( mat->flags & CE2Material::Flag_UseDetailBlender ) && mat->detailBlenderSettings->isEnabled ) {
		const CE2Material::DetailBlenderSettings *	sp = mat->detailBlenderSettings;
		prog->uni1b( "lm.detailBlender.detailBlendMaskSupported", true );
		const CE2Material::UVStream *	uvStream = sp->uvStream;
		if ( !uvStream )
			uvStream = &CE2Material::defaultUVStream;
		FloatVector4	replUniform( 0.0f );
		int	texUniform = lsp->getSFTexture( texunit, replUniform, *(sp->texturePath), sp->textureReplacement, int(sp->textureReplacementEnabled), uvStream );
		prog->uni1i( "lm.detailBlender.maskTexture", texUniform );
		if ( texUniform < 0 )
			prog->uni4f( "lm.detailBlender.maskTextureReplacement", replUniform );
		prog->uni4f( "lm.detailBlender.uvStream.scaleAndOffset", uvStream->scaleAndOffset );
		prog->uni1b( "lm.detailBlender.uvStream.useChannelTwo", (uvStream->channel > 1) );
	} else {
		prog->uni1b( "lm.detailBlender.detailBlendMaskSupported", false );
	}

	// material layers
	int	texUniforms[9];
	FloatVector4	replUniforms[9];
	// limit the number of layers to 6, or 2 if the shader model is Eye1Layer, or 5 for Skin5Layer
	int	numLayers = std::countr_one( mat->layerMask & ( mat->shaderModel != 41 ?
														( mat->shaderModel != 48 ? 0x3FU : 0x1FU ) : 0x03U ) );
	prog->uni1i( "lm.numLayers", numLayers );
	for ( int i = 0; i < numLayers; i++ ) {
		const CE2Material::Layer *	layer = mat->layers[i];
		std::uint32_t	textureSlotMap = 0;
		std::uint32_t	textureReplModes = 0x0055955E;	// 2, 3, 1, 1, 1, 1, 1, 2, 1, 1, 1, 1
		const CE2Material::Blender *	blender = nullptr;
		unsigned char	blendMode = 3;	// "None"
		if ( i ) {
			blender = mat->blenders[i - 1];
			if ( !blender ) [[unlikely]]
				blender = &CE2Material::defaultBlender;
			else
				blendMode = blender->blendMode;
			if ( blendMode == 4 ) {
				// CharacterCombine: remap color, roughness and metalness to overlay texture slots (0,3,4 -> 14,15,16)
				textureSlotMap = 0x000CC00E;
			}
		}
		const CE2Material::Material *	material = layer->material;
		if ( !material ) [[unlikely]]
			material = &CE2Material::defaultMaterial;
		const CE2Material::TextureSet *	textureSet = material->textureSet;
		if ( !textureSet ) [[unlikely]]
			textureSet = &CE2Material::defaultTextureSet;
		prog->uni1f_l( prog->uniLocation("lm.layers[%d].material.textureSet.floatParam", i), textureSet->floatParam );
		for ( int j = 0; j < 9; j++ ) {
			int	k = j + int( textureSlotMap & 15U );
			const std::string_view *	texturePath = textureSet->texturePaths[k];
			std::uint32_t	textureReplacement = textureSet->textureReplacements[k];
			int	textureReplacementMode =
				( !( textureSet->textureReplacementMask & (1 << k) ) ? 0 : int( textureReplModes & 3U ) );
			textureSlotMap = textureSlotMap >> 4;
			textureReplModes = textureReplModes >> 2;
			const CE2Material::UVStream *	uvStream = layer->uvStream;
			if ( j == 0 ) {
				if ( !scene->hasOption(Scene::DoDiffuse)
					|| (scene->hasVisMode(Scene::VisNormalsOnly) && scene->hasOption(Scene::DoLighting)) || useErrorColor ) {
					texturePath = &emptyTexturePath;
					textureReplacement = ( useErrorColor ? 0xFFFF00FFU : 0xFFFFFFFFU );
					textureReplacementMode = 1;
				} else if ( !texturePath->empty() && !textureReplacementMode
							&& ( scene->options & (Scene::DoTexturing | Scene::DoErrorColor) ) != Scene::DoTexturing ) {
					textureReplacement = ( ( scene->options & Scene::DoTexturing ) ? 0xFFFF00FFU : 0xFFFFFFFFU );
					textureReplacementMode = 1;
				}
			} else if ( j == 1 && ( !scene->hasOption(Scene::DoLighting) || !scene->hasOption(Scene::DoNormalMap) ) ) {
				texturePath = &emptyTexturePath;
				textureReplacement = 0xFFFF8080U;
				textureReplacementMode = 3;
			} else if ( j == 2 && ( mat->flags & CE2Material::Flag_HasOpacity ) && i == mat->alphaSourceLayer ) {
				uvStream = mat->alphaUVStream;
			}
			replUniforms[j] = FloatVector4( 0.0f );
			texUniforms[j] = lsp->getSFTexture( texunit, replUniforms[j], *texturePath, textureReplacement, textureReplacementMode, uvStream );
		}
		if ( blendMode == 4 ) [[unlikely]] {
			// set default color (0.5) for overlay textures in CharacterCombine blend mode
			if ( !texUniforms[0] ) {
				texUniforms[0] = -1;
				replUniforms[0] = FloatVector4( 0.5f );
			}
			if ( !texUniforms[3] ) {
				texUniforms[3] = -1;
				replUniforms[3] = FloatVector4( 0.5f );
			}
			if ( !texUniforms[4] ) {
				texUniforms[4] = -1;
				replUniforms[4] = FloatVector4( 0.5f );
			}
		}
		if ( mat->shaderModel == 44 ) [[unlikely]] {	// Hair1Layer
			if ( !texUniforms[3] && ( mat->flags & CE2Material::Flag_IsHair ) && mat->hairSettings ) {
				float	hairRoughness = mat->hairSettings->roughness;
				texUniforms[3] = -1;
				replUniforms[3] = FloatVector4( ( ( hairRoughness - 2.0f ) * hairRoughness + 2.0f ) * hairRoughness );
			}
		}
		prog->uni1iv_l( prog->uniLocation("lm.layers[%d].material.textureSet.textures", i), texUniforms, 9 );
		prog->uni4fv_l( prog->uniLocation("lm.layers[%d].material.textureSet.textureReplacements", i), replUniforms, 9 );

		const CE2Material::UVStream *	uvStream = layer->uvStream;
		if ( !uvStream )
			uvStream = &CE2Material::defaultUVStream;
		FloatVector4	uvScaleAndOffset( uvStream->scaleAndOffset );
		prog->uni4srgb_l( prog->uniLocation("lm.layers[%d].material.color", i), layer->material->color );
		// disable vertex color tint for 1LayerMouth
		int	materialFlags = layer->material->colorModeFlags & ( mat->shaderModel != 9 ? 3 : 1 );
		if ( layer->material->flipbookFlags & 1 ) [[unlikely]]
			materialFlags = materialFlags | setFlipbookParameters( *(layer->material), uvScaleAndOffset );
		prog->uni1i_l( prog->uniLocation("lm.layers[%d].material.flags", i), materialFlags );
		prog->uni4f_l( prog->uniLocation("lm.layers[%d].uvStream.scaleAndOffset", i), uvScaleAndOffset );
		prog->uni1b_l( prog->uniLocation("lm.layers[%d].uvStream.useChannelTwo", i), (uvStream->channel > 1) );

		if ( !blender )
			continue;
		uvStream = blender->uvStream;
		if ( !uvStream )
			uvStream = &CE2Material::defaultUVStream;
		prog->uni4f_l( prog->uniLocation("lm.blenders[%d].uvStream.scaleAndOffset", i - 1), uvStream->scaleAndOffset );
		prog->uni1b_l( prog->uniLocation("lm.blenders[%d].uvStream.useChannelTwo", i - 1), (uvStream->channel > 1) );
		FloatVector4	replUniform( 0.0f );
		int	texUniform = lsp->getSFTexture( texunit, replUniform, *(blender->texturePath), blender->textureReplacement, int(blender->textureReplacementEnabled), uvStream );
		prog->uni1i_l( prog->uniLocation("lm.blenders[%d].maskTexture", i - 1), texUniform );
		if ( texUniform < 0 )
			prog->uni4f_l( prog->uniLocation("lm.blenders[%d].maskTextureReplacement", i - 1), replUniform );
		prog->uni1i_l( prog->uniLocation("lm.blenders[%d].blendMode", i - 1), int(blendMode) );
		prog->uni1i_l( prog->uniLocation("lm.blenders[%d].colorChannel", i - 1), int(blender->colorChannel) );
		prog->uni1fv_l( prog->uniLocation("lm.blenders[%d].floatParams", i - 1), blender->floatParams, CE2Material::Blender::maxFloatParams );
		prog->uni1bv_l( prog->uniLocation("lm.blenders[%d].boolParams", i - 1), blender->boolParams, CE2Material::Blender::maxBoolParams );
	}

	prog->uniSampler_l( prog->uniLocation("textureUnits"), 2, texunit - 2, TexCache::num_texture_units - 2 );

	mesh->setUniforms( prog );
	FloatVector4 vertexColorOverride( scene->hasOption(Scene::DoVertexColors) ? 0.0f : 1.0f );
	if ( !scene->hasOption(Scene::DoVertexAlpha) )
		vertexColorOverride[3] = 1.0f;
	prog->uni4f( "vertexColorOverride", vertexColorOverride );

	// setup alpha blending and testing

	int	alphaFlags = 0;
	if ( mat && scene->hasOption(Scene::DoBlending) ) {
		if ( isEffect || !( ~(mat->flags) & ( CE2Material::Flag_IsDecal | CE2Material::Flag_AlphaBlending ) ) ) {
			int	blendMode;
			if ( !isEffect ) {
				blendMode = mat->decalSettings->blendMode;
			} else if ( !( mat->effectSettings->flags & (CE2Material::EffectFlag_EmissiveOnly | CE2Material::EffectFlag_EmissiveOnlyAuto) ) ) {
				blendMode = mat->effectSettings->blendMode;
			} else {
				blendMode = 1;	// emissive only: additive blending
			}
			setupGLBlendModeSF( blendMode, prog->f );
			alphaFlags = 2;
		}

		if ( isEffect )
			alphaFlags |= int( bool(mat->effectSettings->flags & CE2Material::EffectFlag_IsAlphaTested) );
		else
			alphaFlags |= int( bool(mat->flags & CE2Material::Flag_HasOpacity) && mat->alphaThreshold > 0.0f );

		if ( mat->flags & CE2Material::Flag_IsDecal ) {
			fn->glEnable( GL_POLYGON_OFFSET_FILL );
			fn->glPolygonOffset( -1.0f, -1.0f );
		}
	}
	prog->uni1i( "alphaFlags", alphaFlags );
	if ( !( alphaFlags & 2 ) )
		fn->glDisable( GL_BLEND );

	if ( !mesh->depthTest ) [[unlikely]]
		fn->glDisable( GL_DEPTH_TEST );
	else
		fn->glEnable( GL_DEPTH_TEST );
	fn->glDepthMask( !mesh->depthWrite || mesh->translucent ? GL_FALSE : GL_TRUE );
	fn->glDepthFunc( GL_LEQUAL );
	if ( mat->flags & CE2Material::Flag_TwoSided ) {
		fn->glDisable( GL_CULL_FACE );
	} else {
		fn->glEnable( GL_CULL_FACE );
		fn->glCullFace( GL_BACK );
	}
	fn->glPolygonMode( GL_FRONT_AND_BACK, GL_FILL );

	return true;
}

/*! Material setup for the PBRM metallic/roughness program.
 *
 * Textures are bound by explicit path rather than through `uniSampler`, which
 * resolves slots out of the shader property's own texture list — a PBRM's paths
 * live in the material document, not in the NIF.
 *
 * Feature bits are handed to the shader verbatim so it never guesses whether a
 * channel is real: a clear bit means "use the constant", which is exactly the
 * override semantics the reader already resolved.
 */
bool Renderer::setupProgramPBRM( const NifModel * nif, Program * prog, Shape * mesh )
{
	auto scene = mesh->scene;
	auto lsp = mesh->bslsp;
	if ( !( lsp && scene && scene->textures ) )
		return false;

	int texunit = 0;
	// the units the base and emissive textures landed on, for the sRGB-tag query
	int baseUnit = -1, emissiveUnit = -1, specUnit = -1;

	// PBR mode renders EVERYTHING through this program, so a shape with no PBRM
	// has to be driven from its legacy material instead. Spec/gloss maps onto
	// metallic/roughness the same way the conversion pipeline does it: gloss is
	// inverted to roughness, FO4 carries no metallicity so metal is 0, and F0 is
	// the dielectric constant. Not a substitute for an authored PBRM — it is what
	// "show me the whole scene under one BRDF" can honestly mean.
	PbrmMaterial derived;
	if ( !lsp->pbrmValid ) {
		derived.roughness = std::clamp( 1.0f - lsp->specularGloss, 0.02f, 1.0f );
		derived.metallic = 0.0f;
		derived.ao = 1.0f;
		derived.f0 = 0.04f;
		derived.normalStrength = 1.0f;
		derived.opacity = 1.0f;
		// Diffuse and normal exist in the legacy slots, so tell the shader to
		// sample them; there is no legacy RMAOS or emissive-intensity map, so
		// those bits stay clear and the constants above apply.
		derived.features = PbrmMaterial::BaseColorTexture | PbrmMaterial::NormalTexture;
	}
	const PbrmMaterial & m = lsp->pbrmValid ? lsp->pbrm : derived;

	lsp->pbrmBindRefusal.clear();

	// `required` = the material SAMPLES this texture (its feature bit is set):
	// then a failed bind returns false and the caller aborts the whole binding.
	auto bindPath = [&]( const char * uniform, const QString & path, const QString & fallback, bool required ) -> bool {
		int uni = prog->uniLocation( uniform );
		if ( uni < 0 )
			return true;
		fn->glActiveTexture( GL_TEXTURE0 + GLenum( texunit ) );
		bool bound = false;
		if ( required && !path.isEmpty() )
			bound = scene->textures->bind( QStringView( path ), nif );
		if ( !bound ) {
			if ( required )
				return false;
			scene->textures->bind( QStringView( fallback ), nif );
		}
		prog->uni1i_l( uni, texunit );
		texunit++;
		return true;
	};

	if ( lsp->pbrmValid ) {
		/* THE WHOLE-BINDING ABORT (lane PBRR1, docs s2.1 "texture failure"): a
		 * texture the material samples that fails to load aborts the PBR
		 * binding -- the shape falls back to legacy and the census names the
		 * slot and path -- as the game does, rather than rendering the PBRM
		 * with a neutral stand-in. A slot the material does NOT sample (off, or
		 * every channel overridden) still binds its neutral: that is a
		 * legitimate authoring state, not a failure. */
		struct SlotBind
		{
			const char * uniform;
			const char * slot;
			const PbrmMaterial::Slot * s;
			quint32 bit;
			QString fallback;
			bool sampled;	//!< the shader reads the texture (for the feature-bit slots: the bit)
		};
		// the tint mask (lane PBRR4) has no feature bit: it is sampled when any mask channel reads it
		const bool tintSampled = m.tintEnabled
			&& ( m.tintUseTexture[0] || m.tintUseTexture[1] || m.tintUseTexture[2] || m.tintUseTexture[3] );
		const SlotBind binds[] = {
			{ "BaseMap", "baseColor", &m.baseColor, PbrmMaterial::BaseColorTexture, white, false },
			{ "NormalMap", "normal", &m.normal, PbrmMaterial::NormalTexture, ::default_n, false },
			{ "RmaosMap", "rmaos", &m.rmaos, PbrmMaterial::RmaosTexture, white, false },
			{ "EmissiveMap", "emissive", &m.emissive, PbrmMaterial::EmissiveTexture, black, false },
			// v6 (lane PBRR3): the specular colour map -- RGB the sRGB tint (bit 25), A the IOR over
			// [0, iorMax] (bit 30). Sampled under either bit, so either makes it required.
			{ "SpecColorMap", "specularColor", &m.specularColor,
				quint32( PbrmMaterial::SpecularColorTexture | PbrmMaterial::SpecularIorTexture ), white, false },
			{ "TintMaskMap", "tintMask", &m.tintMask, 0u, black, tintSampled },
		};
		for ( const SlotBind & sb : binds ) {
			const bool required = ( sb.bit ? ( m.features & sb.bit ) != 0 : sb.sampled )
				&& sb.s->enabled && sb.s->pathValid;
			const int unitBefore = texunit;
			const bool bindOk = bindPath( sb.uniform, sb.s->lookupPath, sb.fallback, required );
			if ( bindOk && texunit > unitBefore ) {
				if ( sb.bit == PbrmMaterial::BaseColorTexture )
					baseUnit = unitBefore;
				else if ( sb.bit == PbrmMaterial::EmissiveTexture )
					emissiveUnit = unitBefore;
				else if ( sb.s == &m.specularColor )
					specUnit = unitBefore;
			}
			if ( !bindOk ) {
				lsp->pbrmBindRefusal = QStringLiteral( "texture load failed: %1 %2; PBR binding aborted" )
					.arg( QLatin1StringView( sb.slot ), sb.s->lookupPath );
				return false;
			}
		}
	} else {
		// Legacy fallback: pull diffuse and normal from the property's own slots
		// via uniSampler, which is what resolves them for the spec/gloss path.
		TexClampMode clamp = lsp->clampMode;
		QString emptyString;
		baseUnit = texunit;
		prog->uniSampler( lsp, "BaseMap", 0, texunit, white, clamp, emptyString );
		if ( texunit == baseUnit )
			baseUnit = -1;
		prog->uniSampler( lsp, "NormalMap", 1, texunit, ::default_n, clamp, emptyString );
		bindPath( "RmaosMap", QString(), white, false );
		emissiveUnit = texunit;
		bindPath( "EmissiveMap", QString(), black, false );
		if ( texunit == emissiveUnit )
			emissiveUnit = -1;
		bindPath( "SpecColorMap", QString(), white, false );
		bindPath( "TintMaskMap", QString(), black, false );
	}

	/* sRGB tag (lane PBRR2A, docs s5.2 "decoded as sRGB regardless of the DXGI
	 * tag"): a texture whose DXGI format is _SRGB was created with a GL sRGB
	 * internal format, so the sampler already returns linear; any other is decoded
	 * in the shader. Read off the bound texture itself (the GL truth), not guessed
	 * from the file name. Red "srgbtag": always decode, so the sRGB twin is decoded
	 * twice. */
	auto srgbAt = [&]( int unit ) -> bool {
		if ( unit < 0 || wwR2aRed( "srgbtag" ) )
			return false;
		fn->glActiveTexture( GL_TEXTURE0 + GLenum( unit ) );
		GLint tex = 0;
		glGetIntegerv( GL_TEXTURE_BINDING_2D, &tex );
		if ( !tex )
			return false;
		GLint f = 0;
		glGetTexLevelParameteriv( GL_TEXTURE_2D, 0, GL_TEXTURE_INTERNAL_FORMAT, &f );
		switch ( f ) {
		case 0x8C40:	// GL_SRGB
		case 0x8C41:	// GL_SRGB8
		case 0x8C42:	// GL_SRGB_ALPHA
		case 0x8C43:	// GL_SRGB8_ALPHA8
		case 0x8C48:	// GL_COMPRESSED_SRGB
		case 0x8C49:	// GL_COMPRESSED_SRGB_ALPHA
		case 0x8C4C:	// GL_COMPRESSED_SRGB_S3TC_DXT1_EXT
		case 0x8C4D:	// GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT
		case 0x8C4E:	// GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT
		case 0x8C4F:	// GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT
		case 0x8E8D:	// GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM
			break;
		default:
			return false;
		}
		// sRGB-tagged: skip the hardware decode so the shader's decode is the only
		// one (see wwRestoreSrgbDecode); without the extension the sampler decodes
		if ( QOpenGLContext * c = QOpenGLContext::currentContext();
			c && c->hasExtension( QByteArrayLiteral( "GL_EXT_texture_sRGB_decode" ) ) ) {
			glTexParameteri( GL_TEXTURE_2D, 0x8A48, 0x8A4A );	// TEXTURE_SRGB_DECODE_EXT = SKIP_DECODE_EXT
			wwSkippedDecode().append( { c, GLuint( tex ) } );
			return false;
		}
		return true;
	};
	prog->uni1i( "baseIsSrgbTex", srgbAt( baseUnit ) );
	prog->uni1i( "emissiveIsSrgbTex", srgbAt( emissiveUnit ) );
	prog->uni1i( "specIsSrgbTex", srgbAt( specUnit ) );

	/* v6 specular (lane PBRR3, PBRM-v6-Specular.md; FO4CS wave 89 SPECV6 is the runtime twin).
	 * The shader takes the law apart, because the weight scales the whole dielectric Fresnel
	 * (OpenPBR: weight 0 = no specular lobe), not only F0:
	 *   pbrSpecV6     bit 7 is the WEIGHT map (v6), not an F0 map (v4/v5)
	 *   pbrSpecWeight the weight constant (1 for v4/v5)
	 *   pbrF0         the untinted F0 of a weight-1 surface: v6 min(IorF0(ior), 1); v4/v5 min(f0, 0.16)
	 *   pbrSpecTint   the constant tint, sRGB-decoded here (white when the slot is off)
	 *   pbrIorMax     the IOR map's ceiling (bit 30: ior = A x iorMax)
	 * so pbrSpecWeight x pbrF0 is pbrmDielectricF0 (the census level) in every case. The red
	 * law V5 (WW_PBRM_F0_LAW=v5) is uploaded as a v5 reading: weight 1, the scalar as F0. */
	const bool specV6 = lsp->pbrmValid && m.specularV6 && wwPbrmF0Law() == PbrmF0Law::Auto;
	const float specWeight = specV6 ? m.specularWeight : 1.0f;
	const float specF0 = !lsp->pbrmValid ? m.f0
		: specV6 ? std::min( pbrmIorF0( m.specularIor ), 1.0f ) : pbrmDielectricF0( m, wwPbrmF0Law() );
	auto srgbDecode = []( float c ) {
		return c <= 0.04045f ? c / 12.92f : std::pow( ( c + 0.055f ) / 1.055f, 2.4f );
	};
	prog->uni1i( "pbrFeatures", int( m.features ) );
	prog->uni1i( "pbrSpecV6", specV6 );
	prog->uni1f( "pbrSpecWeight", specWeight );
	prog->uni3f( "pbrSpecTint", srgbDecode( m.specularTint[0] ), srgbDecode( m.specularTint[1] ),
		srgbDecode( m.specularTint[2] ) );
	prog->uni1f( "pbrIorMax", m.specularIorMax );
	prog->uni1f( "pbrDiffuseRoughness", m.diffuseRoughness );

	prog->uni3f( "pbrBaseColor", m.baseColorRGB[0], m.baseColorRGB[1], m.baseColorRGB[2] );
	prog->uni1f( "pbrOpacity", m.opacity );
	prog->uni1f( "pbrRoughness", m.roughness );
	prog->uni1f( "pbrMetallic", m.metallic );
	prog->uni1f( "pbrAo", m.ao );
	prog->uni1f( "pbrF0", specF0 );
	prog->uni1f( "pbrNormalStrength", m.normalStrength );
	prog->uni3f( "pbrEmissiveColor", m.emissiveRGB[0], m.emissiveRGB[1], m.emissiveRGB[2] );
	prog->uni1f( "pbrEmissiveIntensity", m.emissiveIntensity );
	// lane PBRR4 (docs s3.2 items 4, 7, 9): emission replace semantics, tint masks, composition
	prog->uni1f( "pbrEmissiveMask", m.emissiveMask );
	prog->uni1b( "pbrEmissiveMapColor", !m.overrideEmissiveColor );
	prog->uni1b( "pbrEmissiveMapMask", !m.overrideEmissiveMask );
	{
		int useTex = 0;
		for ( int c = 0; c < 4; c++ )
			useTex |= ( m.tintUseTexture[c] ? 1 << c : 0 );
		prog->uni1b( "pbrTintEnabled", m.tintEnabled );
		prog->uni1i( "pbrTintUseTex", m.tintEnabled ? useTex : 0 );
		prog->uni4f( "pbrTintMasks", FloatVector4( m.tintMaskConst[0], m.tintMaskConst[1], m.tintMaskConst[2],
			m.tintMaskConst[3] ) );
		for ( int c = 0; c < 4; c++ )
			prog->uni3f_l( prog->uniLocation( "pbrTintColors[%d]", c ), m.tintColor[c][0], m.tintColor[c][1],
				m.tintColor[c][2] );
		prog->uni1i( "pbrTintMode", m.tintOverlap );
	}
	// -1 = the NIF alpha property: derived (non-.pbrm) shapes, and the red "nocomp"
	const int r4Red = wwR4RedBits();
	const int composition = ( lsp->pbrmValid && !( r4Red & 16 ) ) ? std::clamp( m.composition, 0, 7 ) : -1;
	prog->uni1i( "pbrComposition", composition );
	prog->uni1f( "pbrGlobalOpacity", m.globalOpacity );
	prog->uni1f( "pbrAlphaThreshold", m.alphaThreshold );
	prog->uni1b( "pbrAlphaConst", m.alphaSourceConstant );
	prog->uni1i( "r4Red", r4Red );

	// Alpha and the UV transform still come from the NIF/BGSM side: how a shape
	// blends is a property of the geometry's use, not of the PBRM document.
	prog->uni1f( "alpha", lsp->alpha );
	prog->uni2f( "uvScale", lsp->uvScale.x, lsp->uvScale.y );
	prog->uni2f( "uvOffset", lsp->uvOffset.x, lsp->uvOffset.y );

	// Environment: reuse whatever cube map the property already resolved. It is
	// all NifSkope has — the editor's prefiltered IBL has no counterpart here.
	bool hasCubeMap = ( scene->hasOption( Scene::DoCubeMapping ) && scene->hasOption( Scene::DoLighting )
		&& lsp->hasEnvironmentMap );
	int uniCubeMap = prog->uniLocation( "CubeMap" );
	if ( uniCubeMap >= 0 ) {
		const QString & fname = lsp->fileName( 4 );
		fn->glActiveTexture( GL_TEXTURE0 + GLenum( texunit ) );
		if ( hasCubeMap && ( fname.isEmpty() || !scene->bindCube( fname ) ) )
			hasCubeMap = false;
		if ( !hasCubeMap ) {
			// Must still bind SOMETHING complete: a sampler the shader references
			// with an incomplete texture attached can invalidate the whole draw,
			// not just the sample. cube_sk is Skyrim's and is absent from FO4
			// archives, so it left exactly that hole — pick by version the way
			// the spec/gloss path does.
			const std::uint32_t bsv = nif->getBSVersion();
			const QString & fallback = ( bsv < 128 ? ::cube_sk : ::cube_fo4 );
			if ( !scene->bindCube( fallback ) )
				scene->bindCube( ::cube_sk );
		}
		prog->uni1i_l( uniCubeMap, texunit );
		texunit++;
	}
	prog->uni1i( "hasCubeMap", hasCubeMap );
	prog->uni1f( "envReflection", lsp->environmentReflection );

	/* Scene lighting (lane PBRR2A, docs s5.2). Studio binds the SFCubeMapCache
	 * pair of the ONE Studio cube (the FO4 default outdoor cube until R2b's
	 * lookdev picks another); Legacy binds the complete grey colour cube on both
	 * units, because a samplerCube left on unit 0 would share it with BaseMap's
	 * sampler2D and invalidate the draw. */
	const int sceneMode = wwSceneMode();
	bool hasStudioCube = ( sceneMode == WwSceneStudio || sceneMode == WwSceneLookdev );
	/* Lookdev (lane PBRR2B, docs s6.3 stage-1 cube order): the material's own
	 * EnvmapTexture first, else the lookdev cube (mipblur_DefaultOutside1 unless
	 * picked). The irradiance half follows whichever cube the specular half took. */
	QString ldCube;
	if ( sceneMode == WwSceneLookdev ) {
		ldCube = wwLookdevCubePath();
		const QString & own = lsp->fileName( 4 );
		if ( lsp->hasEnvironmentMap && !own.isEmpty() ) {
			fn->glActiveTexture( GL_TEXTURE0 + GLenum( texunit ) );
			if ( wwBindStudioCube( scene->textures, nif, own, false ) )
				ldCube = own;
		}
	}
	for ( int k = 0; k < 2; k++ ) {
		const int uni = prog->uniLocation( k ? "IrradianceMap" : "StudioCube" );
		if ( uni < 0 ) {
			hasStudioCube = false;
			continue;
		}
		fn->glActiveTexture( GL_TEXTURE0 + GLenum( texunit ) );
		const bool ok = ( sceneMode == WwSceneStudio
			&& wwBindStudioCube( scene->textures, nif, wwStudioCubePath(), k == 1 ) )
			|| ( sceneMode == WwSceneLookdev && wwBindStudioCube( scene->textures, nif, ldCube, k == 1 ) );
		if ( !ok ) {
			hasStudioCube = false;
			scene->bindCube( ::grayCube );
		}
		prog->uni1i_l( uni, texunit );
		texunit++;
	}
	prog->uni1i( "sceneMode", sceneMode );
	prog->uni1i( "hasStudioCube", hasStudioCube );
	prog->uni1f( "sceneExposure", wwSceneExposureScale() );
	prog->uni1i( "viewTransform", wwSceneViewTransform() );
	prog->uni1f( "studioProbe", wwStudioProbe() );
	prog->uni1f( "studioSun", wwStudioSunScale() );
	prog->uni1i( "r3Term", wwR3Term() );
	prog->uni1i( "r3Red", wwR3RedBits() );
	if ( sceneMode == WwSceneLookdev ) {
		float dalc[6][3];
		wwLookdevDalc( dalc );
		for ( int a = 0; a < 6; a++ )
			prog->uni3f_l( prog->uniLocation( "lookdevDalc[%d]", a ), dalc[a][0], dalc[a][1], dalc[a][2] );
		prog->uni1b( "lookdevDalcFlip", wwLookdevRed( "dalcflip" ) );
	}
	wwLookdevFogUniforms( scene );	// lane FOG1: fogOn is false outside Lookdev

	// Per-draw GL state, same as the spec/gloss path ends with. Omitting it made
	// the shape inherit whatever blend/depth state the previous program left
	// behind, and it drew nothing at all — the geometry was there, the state was
	// not. AlphaProperty::glProperty also supplies alphaFlags/alphaThreshold,
	// which the fragment shader reads.
	if ( composition >= 0 ) {
		/* The .pbrm composition replaces the NIF alpha property (lane PBRR4; FO4CS
		 * PBRM.cpp:679-705 rewrites the NiAlphaProperty flags from it; the blend
		 * functions are the editor's, ED:5447-5450). 0/1 draw opaque (the test is the
		 * shader's discard), the rest blend. Draw order is not re-sorted (owed). */
		prog->uni1i( "alphaFlags", 0 );
		prog->uni1f( "alphaThreshold", m.alphaThreshold );
		if ( composition >= 2 && scene->hasOption( Scene::DoBlending ) ) {
			glEnable( GL_BLEND );
			switch ( composition ) {
			case 3:	// Premultiplied
				fn->glBlendFuncSeparate( GL_ONE, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA );
				break;
			case 4:	// Additive
				fn->glBlendFuncSeparate( GL_SRC_ALPHA, GL_ONE, GL_ZERO, GL_ONE );
				break;
			case 5:	// Multiply
				fn->glBlendFuncSeparate( GL_DST_COLOR, GL_ZERO, GL_ZERO, GL_ONE );
				break;
			default:	// Alpha Blend, and Transmission / Water until the merge
				fn->glBlendFuncSeparate( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA );
				break;
			}
		} else {
			glDisable( GL_BLEND );
		}
	} else if ( mesh->translucent && scene->hasOption( Scene::DoBlending ) ) {
		glEnable( GL_BLEND );
		fn->glBlendFuncSeparate( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA );
		prog->uni1i( "alphaFlags", ( mesh->alphaProperty && mesh->alphaProperty->hasAlphaTest() ? 12 : 8 ) );
		prog->uni1f( "alphaThreshold", 0.1f );
	} else {
		AlphaProperty::glProperty( mesh->alphaProperty, prog );
	}

	if ( lsp->hasSF1( ShaderFlags::SF1( ShaderFlags::SLSF1_Decal | ShaderFlags::SLSF1_Dynamic_Decal ) ) ) {
		glEnable( GL_POLYGON_OFFSET_FILL );
		glPolygonOffset( -1.0f, -1.0f );
	}

	/* The runtime's bind-time flag writes (docs s2.1, TruePBRShimRuntime.cpp
	 * 3807-3828), the depth subset (lane PBRR1): a shape a .pbrm serves gets
	 * ZBufferTest, and ZBufferWrite unless it alpha-blends. TwoSided and
	 * AlphaTest from the .pbrm are not read by this reader yet (owed). */
	const bool pbrmDepthTest = lsp->pbrmValid || mesh->depthTest;
	// lane PBRR4: a .pbrm composition's Transparency/Depth depthWrite (default: composition <= 1)
	const bool pbrmDepthWrite = composition >= 0 ? m.depthWrite
		: lsp->pbrmValid ? !mesh->translucent : ( mesh->depthWrite && !mesh->translucent );
	if ( !pbrmDepthTest ) {
		glDisable( GL_DEPTH_TEST );
	} else {
		glEnable( GL_DEPTH_TEST );
		glDepthFunc( GL_LEQUAL );
	}
	glDepthMask( pbrmDepthWrite ? GL_TRUE : GL_FALSE );
	glPolygonMode( GL_FRONT_AND_BACK, GL_FILL );

	/* Two uniforms this path never uploaded.
	 *
	 * vertexColorOverride: pbrm_default.frag multiplies by the vertex colour
	 * like the others, and GL initialises the uniform to 0 - so without this
	 * a mesh with no real vertex colours is multiplied by the black padding
	 * array forever. Every sibling path sets it; this one was simply missed.
	 *
	 * setUniforms: normalMatrix and modelViewMatrix are PER-PROGRAM uniforms,
	 * not part of the shared globalUniforms block, so PBR mode was never
	 * uploading them at all. Latent, and independent of the colour bug.
	 */
	mesh->setUniforms( prog );
	/* Q14 (lane PBRR3): vertex colour reaches the PBR base colour only when the shader
	 * property's SLSF2_Vertex_Colors flag is set, the flag the game applies them under. */
	prog->uni4f( "vertexColorOverride",
		( mesh->hasVertexColors && mesh->colors.size() >= mesh->verts.size()
			&& scene->hasOption( Scene::DoVertexColors ) && lsp->hasSF2( ShaderFlags::SLSF2_Vertex_Colors ) )
			? FloatVector4( 0.0f ) : FloatVector4( 1.0f ) );

	return true;
}

/*! The PBR route debug view (lane PBRR1): the shape flat in its route's
 * colour (pbrmRouteColor), lit by N.L. A shape whose PBR binding was aborted by
 * a texture failure shows legacy grey, because legacy is what it draws.
 */
bool Renderer::setupProgramRoute( const NifModel * nif, Program * prog, Shape * mesh )
{
	Q_UNUSED( nif );
	const BSShaderLightingProperty * sp = mesh->bssp;
	if ( !sp || !mesh->scene )
		return false;
	float c[3];
	pbrmRouteColor( ( sp->pbrmValid && sp->pbrmBindRefusal.isEmpty() ) ? sp->pbrmRoute : PbrmRoute::Legacy, c );
	prog->uni3f( "routeColor", c[0], c[1], c[2] );

	glDisable( GL_BLEND );
	glDisable( GL_POLYGON_OFFSET_FILL );
	glEnable( GL_DEPTH_TEST );
	glDepthFunc( GL_LEQUAL );
	glDepthMask( GL_TRUE );
	glPolygonMode( GL_FRONT_AND_BACK, GL_FILL );

	mesh->setUniforms( prog );
	prog->uni4f( "vertexColorOverride", FloatVector4( 1.0f ) );
	return true;
}

bool Renderer::setupProgramCE1( const NifModel * nif, Program * prog, Shape * mesh )
{
	auto nifVersion = nif->getBSVersion();
	auto scene = mesh->scene;
	auto lsp = mesh->bslsp;
	auto esp = mesh->bsesp;

	BSShaderLightingProperty * bsprop;
	if ( lsp )
		bsprop = lsp;
	else if ( esp )
		bsprop = esp;
	else
		return false;
	Material * mat = bsprop->getMaterial();

	const QString & default_n = (nifVersion >= 151) ? ::default_ns : ::default_n;

	// texturing

	TexClampMode clamp = bsprop->clampMode;

	QString	emptyString;
	int texunit = 0;
	if ( lsp ) {
		// BSLightingShaderProperty

		const QString *	forced = &emptyString;
		if ( !scene->hasOption(Scene::DoDiffuse) || ( scene->hasOption(Scene::DoLighting)
			&& ( scene->hasVisMode(Scene::VisNormalsOnly) || scene->hasVisMode(Scene::VisVertexColors) ) )
			)
			forced = &white;	// diffuse -> white so the vertex colour becomes the albedo
		const QString &	alt = ( !scene->hasOption(Scene::DoErrorColor) ? white : magenta );
		prog->uniSampler( bsprop, "BaseMap", 0, texunit, alt, clamp, *forced );

		forced = &emptyString;
		if ( !scene->hasOption(Scene::DoLighting) || !scene->hasOption(Scene::DoNormalMap) )
			forced = &default_n;
		prog->uniSampler( lsp, "NormalMap", 1, texunit, emptyString, clamp, *forced );

		/* Shader Flags 1 bit 12 says the map just bound is a MODEL-space
		 * normal map (an `_msn`).  fo4_default.frag then transforms it by the
		 * model matrix alone instead of the tangent frame.
		 *
		 * Gated on `forced == &emptyString`, the SAME test that decided whether
		 * a real normal map was bound just above: with lighting or normal maps
		 * switched off the substitute is default_n, a flat TANGENT-space
		 * texel, and reading that as model-space would read it as "north" and
		 * tilt the whole surface over. */
		prog->uni1i( "hasModelSpaceNormals",
			int( forced == &emptyString
				&& lsp->hasSF1( ShaderFlags::SLSF1_Model_Space_Normals ) ) );

		prog->uniSampler( lsp, "GlowMap", 2, texunit, black, clamp );

		prog->uni1f( "lightingEffect1", lsp->lightingEffect1 );
		prog->uni1f( "lightingEffect2", lsp->lightingEffect2 );

		prog->uni1f( "alpha", lsp->alpha );

		prog->uni2f( "uvScale", lsp->uvScale.x, lsp->uvScale.y );
		prog->uni2f( "uvOffset", lsp->uvOffset.x, lsp->uvOffset.y );

		const bool useMaterialTint = scene->hasOption( Scene::DoMaterialTint );
		const bool useDetailTextures = scene->hasOption( Scene::DoDetailTextures );
		prog->uni1i( "greyscaleColor", lsp->greyscaleColor && useMaterialTint );
		prog->uniSampler( bsprop, "GreyscaleMap", 3, texunit, "", TexClampMode::CLAMP_S_CLAMP_T );

		prog->uni1i( "hasTintColor", lsp->hasTintColor && useMaterialTint );
		if ( lsp->hasTintColor && useMaterialTint ) {
			prog->uni3f( "tintColor", lsp->tintColor.red(), lsp->tintColor.green(), lsp->tintColor.blue() );
		}

		prog->uni1i( "hasDetailMask", lsp->hasDetailMask && useDetailTextures );
		prog->uniSampler( bsprop, "DetailMask", 3, texunit, "#FF404040", clamp );

		prog->uni1i( "hasTintMask", lsp->hasTintMask && useMaterialTint );
		prog->uniSampler( bsprop, "TintMask", 6, texunit, gray, clamp );

		// Rim & Soft params

		prog->uni1i( "hasSoftlight", lsp->hasSoftlight );
		prog->uni1i( "hasRimlight", lsp->hasRimlight );

		prog->uniSampler( bsprop, "LightMask", 2, texunit, default_n, clamp );

		// Backlight params

		prog->uni1i( "hasBacklight", lsp->hasBacklight );

		prog->uniSampler( bsprop, "BacklightMap", 7, texunit, default_n, clamp );

		// Glow params

		if ( scene->hasOption(Scene::DoGlow) && scene->hasOption(Scene::DoLighting) && (lsp->hasEmittance || nifVersion >= 151) )
			prog->uni1f( "glowMult", lsp->emissiveMult );
		else
			prog->uni1f( "glowMult", 0 );

		bool hasEmit = lsp->hasEmittance;
		if ( hasEmit && nifVersion >= 151 ) {
			// disable Fallout 76 emittance if the lighting map has no alpha channel
			hasEmit = false;
			if ( auto txtInfo = scene->getTextureInfo( bsprop->fileName( 9 ) ); txtInfo ) {
				hasEmit = bool( txtInfo->format.imageEncoding
								& ( TexCache::TexFmt::TEXFMT_DXT3 | TexCache::TexFmt::TEXFMT_DXT5
									| TexCache::TexFmt::TEXFMT_RGBA8 ) );
			}
		}
		prog->uni1i( "hasEmit", hasEmit );
		prog->uni1i( "hasGlowMap", lsp->hasGlowMap );
		prog->uni3f( "glowColor", lsp->emissiveColor.red(), lsp->emissiveColor.green(), lsp->emissiveColor.blue() );

		// Specular params
		float s = ( scene->hasOption(Scene::DoSpecular) && scene->hasOption(Scene::DoLighting) ) ? lsp->specularStrength : 0.0;
		prog->uni1f( "specStrength", s );
		prog->uni1b( "useGloss", scene->hasOption(Scene::DoGloss) );
		prog->uni3f( "specColor", lsp->specularColor.red(), lsp->specularColor.green(), lsp->specularColor.blue() );
		prog->uni1i( "hasSpecularMap", lsp->hasSpecularMap );

		if ( nifVersion >= 151 ) {
			prog->uni1i( "hasSpecular", int(scene->hasOption(Scene::DoSpecular)) );
		} else {
			// Assure specular power does not break the shaders
			prog->uni1f( "specGlossiness", lsp->specularGloss);

			if ( nifVersion >= 130 || (lsp->hasSpecularMap && !lsp->hasBacklight) )
				prog->uniSampler( bsprop, "SpecularMap", 7, texunit, white, clamp );
			else
				prog->uniSampler( bsprop, "SpecularMap", 7, texunit, black, clamp );
		}

		if ( nifVersion >= 130 ) {
			prog->uni1f( "paletteScale", lsp->paletteScale );
			prog->uni1f( "fresnelPower", lsp->fresnelPower );
			if ( nifVersion < 151 ) {
				prog->uni1f( "subsurfaceRolloff", lsp->lightingEffect1 );
				prog->uni1f( "rimPower", lsp->rimPower );
				prog->uni1f( "backlightPower", lsp->backlightPower );
			} else {
				FloatVector4	translucencyColorAndScale( 1.0f, 1.0f, 1.0f, -1.0f );
				if ( mat && mat->isShaderMaterial() ) {
					const ShaderMaterial *	bgsm = static_cast< ShaderMaterial * >( mat );
					if ( bgsm->bTranslucency ) {
						if ( bgsm->bTranslucencyMixAlbedoWithSubsurfaceCol )
							translucencyColorAndScale = FloatVector4( Color4( bgsm->cTranslucencySubsurfaceColor ) );
						translucencyColorAndScale[3] = bgsm->fTranslucencyTransmissiveScale;
					}
				}
				prog->uni4f( "translucencyColorAndScale", translucencyColorAndScale );
			}
		}

		// Multi-Layer

		prog->uniSampler( bsprop, "InnerMap", 6, texunit, default_n, clamp );
		if ( lsp->hasMultiLayerParallax && useDetailTextures ) {
			prog->uni2f( "innerScale", lsp->innerTextureScale.x, lsp->innerTextureScale.y );
			prog->uni1f( "innerThickness", lsp->innerThickness );

			prog->uni1f( "outerRefraction", lsp->outerRefractionStrength );
			prog->uni1f( "outerReflection", lsp->outerReflectionStrength );
		}

		// Environment Mapping

		bool	hasCubeMap = ( scene->hasOption(Scene::DoCubeMapping) && scene->hasOption(Scene::DoLighting) && (lsp->hasEnvironmentMap || nifVersion >= 151) );
		prog->uni1i( "hasEnvMask", lsp->useEnvironmentMask );
		float refl = ( nifVersion < 151 ? lsp->environmentReflection : 1.0f );
		prog->uni1f( "envReflection", refl );

		// Always bind cube regardless of shader settings
		GLint uniCubeMap = prog->uniLocation( "CubeMap" );
		if ( uniCubeMap < 0 ) {
			hasCubeMap = false;
		} else {
			fn->glActiveTexture( GL_TEXTURE0 + texunit );
			QString	fname = bsprop->fileName( 4 );
			const QString *	cube = &fname;
			if ( hasCubeMap && ( fname.isEmpty() || !scene->bindCube( fname ) ) ) {
				cube = ( nifVersion < 151 ? ( nifVersion < 128 ? &cube_sk : &cube_fo4 ) : &cfg.cubeMapPathFO76 );
				hasCubeMap = scene->bindCube( *cube );
			}
			if ( !hasCubeMap ) [[unlikely]]
				scene->bindCube( grayCube, 1 );
			fn->glUniform1i( uniCubeMap, texunit++ );
			if ( nifVersion >= 151 && ( uniCubeMap = prog->uniLocation( "CubeMap2" ) ) >= 0 ) {
				// Fallout 76: load second cube map for diffuse lighting
				fn->glActiveTexture( GL_TEXTURE0 + texunit );
				hasCubeMap = hasCubeMap && scene->bindCube( *cube, 2 );
				if ( !hasCubeMap ) [[unlikely]]
					scene->bindCube( grayCube, 1 );
				fn->glUniform1i( uniCubeMap, texunit++ );
			}
		}
		prog->uni1i( "hasCubeMap", hasCubeMap );

		// Screen-space refraction preview (SLSF1_Refraction): the shape draws
		// in the second pass, so the framebuffer already holds the scene
		// behind it; distort that with the fragment normal instead of showing
		// the bare normal map / missing-texture magenta
		bool	doRefr = false;
		if ( lsp->hasRefraction && scene->showRefraction && !scene->selecting ) {
			doRefr = scene->grabRefractionSource();
			if ( doRefr ) {
				fn->glActiveTexture( GL_TEXTURE0 + texunit );
				fn->glBindTexture( GL_TEXTURE_2D, scene->refractionTexId );
				prog->uni1i_l( prog->uniLocation( "RefractionSrc" ), texunit );
				texunit++;
				prog->uni1f( "refractionStrength", lsp->refractionStrength );
			}
		}
		prog->uni1i( "doRefraction", doRefr );

		if ( nifVersion < 151 ) {
			// Always bind mask regardless of shader settings
			prog->uniSampler( bsprop, "EnvironmentMap", 5, texunit, white, clamp );
		} else {
			if ( prog->uniLocation( "EnvironmentMap" ) >= 0 ) {
				fn->glActiveTexture( GL_TEXTURE0 + texunit );
				if ( !bsprop->bind( pbr_lut_sf, true, TexClampMode::CLAMP_S_CLAMP_T ) )
					return false;
				fn->glUniform1i( prog->uniLocation( "EnvironmentMap" ), texunit++ );
			}
			prog->uniSampler( bsprop, "ReflMap", 8, texunit, reflectivity, clamp );
			prog->uniSampler( bsprop, "LightingMap", 9, texunit, lighting, clamp );
		}

		// Parallax
		prog->uni1i( "hasHeightMap", lsp->hasHeightMap && scene->hasOption(Scene::DoParallax) );
		prog->uniSampler( bsprop, "HeightMap", 3, texunit, gray, clamp );

	} else {
		// BSEffectShaderProperty

		prog->uni2f( "uvScale", esp->uvScale.x, esp->uvScale.y );
		prog->uni2f( "uvOffset", esp->uvOffset.x, esp->uvOffset.y );

		prog->uni1i( "hasSourceTexture", esp->hasSourceTexture );
		prog->uni1i( "hasGreyscaleMap", esp->hasGreyscaleMap );

		prog->uni1i( "greyscaleAlpha", esp->greyscaleAlpha );
		prog->uni1i( "greyscaleColor", esp->greyscaleColor && scene->hasOption(Scene::DoMaterialTint) );


		prog->uni1i( "useFalloff", esp->useFalloff );
		prog->uni1i( "hasRGBFalloff", esp->hasRGBFalloff );
		prog->uni1i( "hasWeaponBlood", esp->hasWeaponBlood );

		// Glow params

		prog->uni4f( "glowColor", FloatVector4( esp->emissiveColor ) );
		prog->uni1f( "glowMult", scene->hasOption(Scene::DoGlow) ? esp->emissiveMult : 0.0f );

		// Falloff params

		prog->uni4f( "falloffParams", FloatVector4( esp->falloff.startAngle, esp->falloff.stopAngle,
													esp->falloff.startOpacity, esp->falloff.stopOpacity ) );

		prog->uni1f( "falloffDepth", esp->falloff.softDepth );

		// BSEffectShader textures (FIXME: should implement using error color?)

		prog->uniSampler( bsprop, "BaseMap", 0, texunit, white, clamp,
						  scene->hasOption(Scene::DoDiffuse) ? emptyString : white );
		prog->uniSampler( bsprop, "GreyscaleMap", 1, texunit, "", TexClampMode::CLAMP_S_CLAMP_T );

		if ( nifVersion >= 130 ) {

			prog->uni1f( "lightingInfluence", esp->lightingInfluence );

			prog->uni1i( "hasNormalMap", esp->hasNormalMap && scene->hasOption(Scene::DoLighting)
						&& scene->hasOption(Scene::DoNormalMap) );

			prog->uniSampler( bsprop, "NormalMap", 3, texunit, default_n, clamp );

			prog->uni1i( "hasCubeMap", esp->hasEnvironmentMap );
			prog->uni1i( "hasEnvMask", esp->hasEnvironmentMask );
			float refl = 0.0;
			if ( esp->hasEnvironmentMap && scene->hasOption(Scene::DoCubeMapping) && scene->hasOption(Scene::DoLighting) )
				refl = esp->environmentReflection;

			prog->uni1f( "envReflection", refl );

			GLint uniCubeMap = prog->uniLocation( "CubeMap" );
			if ( uniCubeMap >= 0 ) {
				QString fname = bsprop->fileName( 2 );
				const QString&	cube = (nifVersion < 151 ? (nifVersion < 128 ? cube_sk : cube_fo4) : cfg.cubeMapPathFO76);
				if ( fname.isEmpty() )
					fname = cube;

				fn->glActiveTexture( GL_TEXTURE0 + texunit );
				if ( !scene->bindCube( fname ) && !scene->bindCube( cube ) && !scene->bindCube( grayCube, 1 ) )
					return false;

				fn->glUniform1i( uniCubeMap, texunit++ );
			}
			if ( nifVersion < 151 ) {
				prog->uniSampler( bsprop, "SpecularMap", 4, texunit, white, clamp );
			} else {
				prog->uniSampler( bsprop, "EnvironmentMap", 4, texunit, white, clamp );
				prog->uniSampler( bsprop, "ReflMap", 6, texunit, reflectivity, clamp );
				prog->uniSampler( bsprop, "LightingMap", 7, texunit, lighting, clamp );
				prog->uni1i( "hasSpecularMap", int(!bsprop->fileName( 7 ).isEmpty()) );
				bool	glassEnabled = false;
				if ( mat && mat->isEffectMaterial() )
					glassEnabled = static_cast< EffectMaterial * >( mat )->bGlassEnabled;
				prog->uni1b( "isGlass", glassEnabled );
			}

			prog->uni1f( "fLumEmittance", esp->lumEmittance );
		}
	}

	mesh->setUniforms( prog );
	{
		FloatVector4	c( 0.0f );

		bool	doVCs = ( mesh->hasVertexColors && scene->hasOption(Scene::DoVertexColors) && !mesh->colors.isEmpty() );
		// Always do vertex colors for FO4 if colors present
		if ( nifVersion < 130 && !bsprop->hasSF2(ShaderFlags::SLSF2_Vertex_Colors) )
			doVCs = false;

		if ( !doVCs ) {
			c = FloatVector4( 1.0f );
			if ( nifVersion < 130 && !mesh->hasVertexColors && lsp && lsp->hasVertexColors ) {
				// Correctly blacken the mesh if SLSF2_Vertex_Colors is still on
				//	yet "Has Vertex Colors" is not.
				c.blendValues( FloatVector4( 1.0e-15f ), 0x07 );
			}
		} else if ( !scene->hasOption(Scene::DoVertexAlpha) || mesh->isVertexAlphaAnimation
					|| ( nifVersion < 130 && lsp && !lsp->hasSF1(ShaderFlags::SLSF1_Vertex_Alpha) ) ) {
			// TODO (Gavrant): suspicious code. Should the check be replaced with !bsprop->hasVertexAlpha ?
			c[3] = 1.0f;
		}

		// Vertex-colour shading: the diffuse is forced white above, so the albedo
		// becomes the per-vertex colour. Show the real vertex colour (including its
		// alpha) regardless of the vertex-colour display toggle or shader flags.
		if ( scene->hasVisMode(Scene::VisVertexColors) )
			c = ( mesh->hasVertexColors && !mesh->colors.isEmpty() ) ? FloatVector4( 0.0f ) : FloatVector4( 1.0f );

		prog->uni4f( "vertexColorOverride", c );
	}

	/* LOD channel preview. Set HERE and not only in
	 * VertexColorProperty::glProperty, which is where the sibling
	 * vertexColorOverride uniform lives: this path sets that one directly and
	 * never calls it, so a value written there reaches every renderer except
	 * the FO4 one -- the only one that matters for generated LOD.
	 */
	prog->uni1i( "lodChannelView", wwLodChannelView );
	/* The impostor bake's material channel: a shape whose slot 7 was
	 * retargeted to a source .lodm's third texture is read raw; every other
	 * shape bakes the legacy pair, which needs the specular strength whether
	 * or not lighting is on (specStrength above is zeroed when it is off). */
	prog->uni1i( "lodMaskRaw", bsprop->wwTextureOverride.contains( 7 ) ? 1 : 0 );
	/* The impostor bake's emissive channel: a shape whose GLOW slot (2) a
	 * source .lodm retargeted reads that texture raw - GlowMap is bound from
	 * fileName( 2 ) above, which consults the retarget, and falls back to the
	 * black texture when the override is empty. Every other shape bakes the
	 * vanilla LOD glow rule from its own diffuse. */
	prog->uni1i( "lodEmissiveRaw", bsprop->wwTextureOverride.contains( 2 ) ? 1 : 0 );
	/* ...and the source's EMISSIVE COLOUR, which the vanilla glow rule folds
	 * into the sheet. Set UNCONDITIONALLY, unlike the sibling glowColor above,
	 * which is only written while DoGlow and DoLighting are on: the card bake
	 * photographs with lighting OFF, so a value written under those options
	 * would never reach channel 13. Black without a lighting property, which
	 * is a shape that emits nothing. */
	prog->uni3f( "lodEmissiveColor", lsp ? lsp->emissiveColor.red() : 0.0f,
		lsp ? lsp->emissiveColor.green() : 0.0f, lsp ? lsp->emissiveColor.blue() : 0.0f );
	prog->uni1i( "lodTreeAnim", bsprop->isVertexAlphaAnimation ? 1 : 0 );
	prog->uni1i( "lodMaskByTree", wwLodMaskByTree );
	prog->uni1f( "lodSpecStrength", lsp ? lsp->specularStrength : 1.0f );

	if ( mesh->isDoubleSided ) {
		glDisable( GL_CULL_FACE );
	} else {
		glEnable( GL_CULL_FACE );
		glCullFace( GL_BACK );
	}

	// setup blending

	if ( mat ) {
		static const GLenum blendMap[11] = {
			GL_ONE, GL_ZERO, GL_SRC_COLOR, GL_ONE_MINUS_SRC_COLOR,
			GL_DST_COLOR, GL_ONE_MINUS_DST_COLOR, GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA,
			GL_DST_ALPHA, GL_ONE_MINUS_DST_ALPHA, GL_SRC_ALPHA_SATURATE
		};

		int	alphaFlags = 0;
		if ( mat->hasAlphaBlend() && scene->hasOption(Scene::DoBlending) ) {
			glEnable( GL_BLEND );
			fn->glBlendFuncSeparate( blendMap[mat->iAlphaSrc], blendMap[mat->iAlphaDst],
										GL_ONE, blendMap[mat->iAlphaDst] );
			alphaFlags = 8;
		} else {
			glDisable( GL_BLEND );
		}

		float	alphaThreshold = 0.0f;
		if ( mat->hasAlphaTest() && scene->hasOption(Scene::DoBlending) ) {
			alphaFlags |= 4;	// greater
			alphaThreshold = float( mat->iAlphaTestRef ) / 255.0f;
			/* PICTURE-ONLY dial for the tree-card cutoff question (bungo
			 * 2026-09-18, "what is going with those trees? They seem broken"):
			 * WW_LODL_TREE_ALPHA=1..255 tests every BGSM whose first texture
			 * lives under a trees/ folder at that threshold instead of the
			 * BGSM's own (80/82 on the tree LOD sets). The BGSM wins over the
			 * NiAlphaProperty here, so this is the only place the dial bites. */
			static const int treeDial = qEnvironmentVariableIntValue( "WW_LODL_TREE_ALPHA" );
			if ( treeDial >= 1 && treeDial <= 255 && !mat->textureList.isEmpty()
				&& QString( mat->textureList.at( 0 ) ).replace( QChar( '\\' ), QChar( '/' ) )
					.contains( QLatin1String( "/trees/" ), Qt::CaseInsensitive ) )
				alphaThreshold = float( treeDial ) / 255.0f;
		}
		prog->uni1i( "alphaFlags", alphaFlags );
		prog->uni1f( "alphaThreshold", alphaThreshold );

		if ( mat->bDecal ) {
			glEnable( GL_POLYGON_OFFSET_FILL );
			glPolygonOffset( -1.0f, -1.0f );
		}

	} else {
		// BSESP/BSLSP do not always need an NiAlphaProperty, and appear to override it at times
		if ( mesh->translucent && scene->hasOption(Scene::DoBlending) ) {
			glEnable( GL_BLEND );
			fn->glBlendFuncSeparate( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA );
			// If mesh is alpha tested, override threshold
			prog->uni1i( "alphaFlags", ( mesh->alphaProperty && mesh->alphaProperty->hasAlphaTest() ? 12 : 8 ) );
			prog->uni1f( "alphaThreshold", 0.1f );
		} else {
			AlphaProperty::glProperty( mesh->alphaProperty, prog );
		}

		if ( bsprop->hasSF1( ShaderFlags::SF1( ShaderFlags::SLSF1_Decal | ShaderFlags::SLSF1_Dynamic_Decal ) ) ) {
			glEnable( GL_POLYGON_OFFSET_FILL );
			glPolygonOffset( -1.0f, -1.0f );
		}
	}

	if ( !mesh->depthTest ) {
		glDisable( GL_DEPTH_TEST );
	} else {
		glEnable( GL_DEPTH_TEST );
		glDepthFunc( GL_LEQUAL );
	}
	glDepthMask( !mesh->depthWrite || mesh->translucent ? GL_FALSE : GL_TRUE );
	glPolygonMode( GL_FRONT_AND_BACK, GL_FILL );

	wwLookdevFogUniforms( scene );	// lane FOG1: fo4_default reads it; fogOn is false outside Lookdev
	return true;
}

bool Renderer::setupProgramFO3( const NifModel * nif, Program * prog, Shape * mesh )
{
	auto scene = mesh->scene;
	auto esp = mesh->bsesp;

	// defaults for uniforms
	bool	isDecal = false;
	bool	hasSpecular = ( scene->hasOption( Scene::DoSpecular ) && scene->hasOption( Scene::DoLighting ) );
	bool	hasEmit = false;
	bool	hasGlowMap = false;
	bool	hasCubeMap = ( scene->hasOption( Scene::DoCubeMapping ) && scene->hasOption( Scene::DoLighting ) );
	bool	hasCubeMask = false;
	float	cubeMapScale = 1.0f;
	int	parallaxMaxSteps = 0;
	float	parallaxScale = 0.04f;
	float	glowMult = ( scene->hasOption( Scene::DoGlow ) && scene->hasOption( Scene::DoLighting ) ? 1.0f : 0.0f );
	FloatVector4	uvScaleAndOffset( 1.0f, 1.0f, 0.0f, 0.0f );
	FloatVector4	uvCenterAndRotation( 0.5f, 0.5f, 0.0f, 0.0f );
	FloatVector4	falloffParams( 1.0f, 0.0f, 1.0f, 1.0f );

	// texturing

	TexturingProperty * texprop = mesh->findProperty< TexturingProperty >();
	BSShaderLightingProperty * bsprop = mesh->bssp;
	if ( !bsprop && !texprop )
		return false;

	TexClampMode clamp = TexClampMode::WRAP_S_WRAP_T;

	QString	emptyString;
	int texunit = 0;
	if ( bsprop ) {
		clamp = bsprop->clampMode;
		mesh->depthTest = bsprop->depthTest;
		mesh->depthWrite = bsprop->depthWrite;
		const QString *	forced = &emptyString;
		if ( !scene->hasOption(Scene::DoDiffuse) || ( scene->hasOption(Scene::DoLighting)
			&& ( scene->hasVisMode(Scene::VisNormalsOnly) || scene->hasVisMode(Scene::VisVertexColors) ) )
			)
			forced = &white;	// diffuse -> white so the vertex colour becomes the albedo

		const QString &	alt = ( esp || !scene->hasOption(Scene::DoErrorColor) ? white : magenta );

		prog->uniSampler( bsprop, "BaseMap", 0, texunit, alt, clamp, *forced );
	} else {
		hasCubeMap = false;
		GLint uniBaseMap = prog->uniLocation( "BaseMap" );
		if ( uniBaseMap >= 0 ) [[likely]] {
			fn->glActiveTexture( GL_TEXTURE0 + texunit );
			if ( !scene->hasOption(Scene::DoDiffuse) )
				texprop->bind( 0, white );
			else if ( !texprop->bind( 0 ) )
				texprop->bind( 0, ( !scene->hasOption(Scene::DoErrorColor) ? white : magenta ) );
			fn->glUniform1i( uniBaseMap, texunit++ );
		}
	}

	GLint	uniCubeMap = prog->uniLocation( "CubeMap" );
	// always bind a cube map to the cube sampler on texture unit 1 to avoid invalid operation error
	if ( uniCubeMap >= 0 ) [[likely]] {
		fn->glActiveTexture( GL_TEXTURE0 + texunit );
		if ( !( hasCubeMap && bsprop && !esp && scene->bindCube( bsprop->fileName( 4 ) ) ) ) {
			scene->bindCube( grayCube, 1 );
			hasCubeMap = false;
		}
		fn->glUniform1i( uniCubeMap, texunit++ );
	} else {
		hasCubeMap = false;
	}

	const TexCache::Tex::ImageInfo *	txtInfo = nullptr;
	if ( bsprop ) {
		const QString *	forced = &emptyString;
		if ( esp || !scene->hasOption(Scene::DoLighting) || !scene->hasOption(Scene::DoNormalMap) )
			forced = &default_n;
		prog->uniSampler( bsprop, "NormalMap", 1, texunit, emptyString, clamp, *forced );
		if ( hasSpecular )
			txtInfo = scene->getTextureInfo( bsprop->fileName( 1 ) );
	} else {
		GLint uniNormalMap = prog->uniLocation( "NormalMap" );
		if ( uniNormalMap >= 0 ) {
			fn->glActiveTexture( GL_TEXTURE0 + texunit );
			QString fname = texprop->fileName( 0 );
			if ( !fname.isEmpty() ) {
				int pos = fname.lastIndexOf( "_" );
				if ( pos >= 0 )
					fname = fname.left( pos ) + "_n.dds";
				else if ( (pos = fname.lastIndexOf( "." )) >= 0 )
					fname = fname.insert( pos, "_n" );
			}

			if ( !scene->hasOption(Scene::DoNormalMap) || fname.isEmpty() || !texprop->bind( 0, fname ) )
				texprop->bind( 0, default_n );
			else
				txtInfo = scene->getTextureInfo( fname );
			fn->glUniform1i( uniNormalMap, texunit++ );
		}
	}
	if ( !( txtInfo && ( txtInfo->format.imageEncoding
							& ( TexCache::TexFmt::TEXFMT_DXT3 | TexCache::TexFmt::TEXFMT_DXT5
								| TexCache::TexFmt::TEXFMT_RGBA8 ) ) != 0 ) ) {
		// disable specular if the normal map has no alpha channel
		hasSpecular = false;
	}

	if ( bsprop && !esp ) {
		hasGlowMap = !bsprop->fileName( 2 ).isEmpty();
		prog->uniSampler( bsprop, "GlowMap", 2, texunit, black, clamp );

		// Parallax
		prog->uniSampler( bsprop, "HeightMap", 3, texunit, gray, clamp );

		// Environment Mapping
		hasCubeMask = !bsprop->fileName( 5 ).isEmpty();
		prog->uniSampler( bsprop, "EnvironmentMap", 5, texunit, white, clamp );

	} else if ( !bsprop ) {
		GLint uniGlowMap = prog->uniLocation( "GlowMap" );
		if ( uniGlowMap >= 0 ) {
			fn->glActiveTexture( GL_TEXTURE0 + texunit );
			bool	result = false;
			QString fname = texprop->fileName( 0 );
			if ( !fname.isEmpty() ) {
				int pos = fname.lastIndexOf( "_" );
				if ( pos >= 0 )
					fname = fname.left( pos ) + "_g.dds";
				else if ( (pos = fname.lastIndexOf( "." )) >= 0 )
					fname = fname.insert( pos, "_g" );
			}

			if ( !fname.isEmpty() && texprop->bind( 0, fname ) )
				result = true;

			hasGlowMap = result;
			if ( !result )
				texprop->bind( 0, black );
			fn->glUniform1i( uniGlowMap, texunit++ );
		}
	}

	if ( texprop ) {
		auto	t = texprop->getTexture( 0 );
		if ( t && t->hasTransform ) {
			uvScaleAndOffset = FloatVector4( t->tiling[0], t->tiling[1], t->translation[0], t->translation[1] );
			uvCenterAndRotation = FloatVector4( t->center[0], t->center[1], t->rotation, 0.0f );
		}
		const NifItem *	i = texprop->getItem( nif, "Apply Mode" );
		if ( i ) {
			quint32	applyMode = nif->get<quint32>( i );
			isDecal = ( applyMode == 1 );
			if ( applyMode == 4 && scene->hasOption(Scene::DoParallax) )
				parallaxMaxSteps = 1;
		}
	}
	if ( bsprop ) {
		isDecal = bsprop->hasSF1( ShaderFlags::SF1( ShaderFlags::SLSF1_Decal | ShaderFlags::SLSF1_Dynamic_Decal ) );
		hasCubeMap = hasCubeMap && bsprop->hasSF1( ShaderFlags::SLSF1_Environment_Mapping );
		cubeMapScale = bsprop->environmentReflection;
		if ( scene->hasOption(Scene::DoParallax) && bsprop->hasSF1( ShaderFlags::SLSF1_Parallax_Occlusion ) ) {
			const NifItem *	i = bsprop->getItem( nif, "Parallax Max Passes" );
			if ( i )
				parallaxMaxSteps = std::max< int >( roundFloat( nif->get<float>(i) ), 4 );
			i = bsprop->getItem( nif, "Parallax Scale" );
			if ( i )
				parallaxScale *= nif->get<float>( i );
		} else if ( scene->hasOption(Scene::DoParallax) && bsprop->hasSF1( ShaderFlags::SLSF1_Parallax ) ) {
			parallaxMaxSteps = 2;
		}
		if ( esp ) {
			glowMult = ( scene->hasOption(Scene::DoGlow) && scene->hasOption(Scene::DoLighting) ? 1.0f : 0.0f );
			falloffParams = FloatVector4( esp->falloff.startAngle, esp->falloff.stopAngle,
											esp->falloff.startOpacity, esp->falloff.stopOpacity );
		}
	} else {
		hasEmit = true;
	}

	{
		FloatVector4	vcOverride( 0.0f );
		// Do VCs if legacy or if either bslsp or bsesp is set
		bool	doVCs = ( !bsprop || bsprop->hasSF2(ShaderFlags::SLSF2_Vertex_Colors) || bsprop->bsVersion < 83 );

		// same padded-array trap as the fixed-function fallback below
		if ( !( mesh->hasVertexColors && mesh->colors.size() >= mesh->verts.size()
			&& scene->hasOption(Scene::DoVertexColors) && doVCs ) ) {
			vcOverride = FloatVector4( 1.0f );
			if ( !mesh->hasVertexColors && ( mesh->bslsp && mesh->bslsp->hasVertexColors )
				&& scene->hasOption(Scene::DoVertexColors) ) {
				// Correctly blacken the mesh if SLSF2_Vertex_Colors is still on
				//	yet "Has Vertex Colors" is not.
				vcOverride.blendValues( FloatVector4( 1.0e-15f ), 0x07 );
			}
		}
		// TODO (Gavrant): suspicious code. Should the check be replaced with !bsprop->hasVertexAlpha ?
		if ( !scene->hasOption(Scene::DoVertexAlpha) || mesh->isVertexAlphaAnimation
			|| ( mesh->bslsp && !mesh->bslsp->hasSF1(ShaderFlags::SLSF1_Vertex_Alpha) ) )
			vcOverride[3] = 1.0f;

		VertexColorProperty::glProperty( mesh->findProperty< VertexColorProperty >(), vcOverride, prog );
	}
	prog->uni1b( "isEffect", bool(esp) );
	prog->uni1b( "hasSpecular", hasSpecular );
	prog->uni1b( "useGloss", scene->hasOption(Scene::DoGloss) );
	prog->uni1b( "hasEmit", hasEmit );
	prog->uni1b( "hasGlowMap", hasGlowMap );
	prog->uni1b( "hasCubeMap", hasCubeMap );
	prog->uni1b( "hasCubeMask", hasCubeMask );
	prog->uni1f( "cubeMapScale", cubeMapScale );
	prog->uni1i( "parallaxMaxSteps", parallaxMaxSteps );
	prog->uni1f( "parallaxScale", parallaxScale );
	prog->uni1f( "glowMult", glowMult );
	prog->uni2f( "uvCenter", uvCenterAndRotation[0], uvCenterAndRotation[1] );
	prog->uni2f( "uvScale", uvScaleAndOffset[0], uvScaleAndOffset[1] );
	prog->uni2f( "uvOffset", uvScaleAndOffset[2], uvScaleAndOffset[3] );
	prog->uni1f( "uvRotation", uvCenterAndRotation[2] );
	prog->uni4f( "falloffParams", falloffParams );

	mesh->setUniforms( prog );

	if ( mesh->isDoubleSided ) {
		glDisable( GL_CULL_FACE );
	} else {
		glEnable( GL_CULL_FACE );
		glCullFace( GL_BACK );
	}

	if ( !mesh->depthTest ) {
		glDisable( GL_DEPTH_TEST );
	} else {
		glEnable( GL_DEPTH_TEST );
		glDepthFunc( GL_LEQUAL );
	}
	glDepthMask( !mesh->depthWrite || mesh->translucent ? GL_FALSE : GL_TRUE );

	// setup blending

	AlphaProperty::glProperty( mesh->alphaProperty, prog );

	if ( isDecal ) {
		glEnable( GL_POLYGON_OFFSET_FILL );
		glPolygonOffset( -1.0f, -1.0f );
	}

	// setup material

	MaterialProperty::glProperty( mesh->findProperty< MaterialProperty >(), mesh->findProperty< SpecularProperty >(),
									prog );
	prog->uni4f( "frontMaterialSpecular", FloatVector4( 1.0f ) );		// ignore specular color

	// setup Z buffer

	if ( auto p = mesh->findProperty< ZBufferProperty >(); p )
		ZBufferProperty::glProperty( p );

	// setup stencil

	StencilProperty::glProperty( mesh->findProperty< StencilProperty >() );

	glPolygonMode( GL_FRONT_AND_BACK, GL_FILL );

	return true;
}

void Renderer::setupFixedFunction( Shape * mesh )
{
	auto	prog = currentProgram;
	if ( !( prog && ( prog->name == "default.prog" || prog->name == "particles.prog" ) ) ) {
		if ( ( prog = useProgram( "default.prog" ) ) == nullptr )
			return;
	}

	PropertyList props;
	mesh->activeProperties( props );

	// Disable specular because it washes out vertex colors
	//	at perpendicular viewing angles
	prog->uni4f( "frontMaterialSpecular", FloatVector4( 0.0f ) );

	// setup blending

	AlphaProperty::glProperty( mesh->alphaProperty, prog );

	// setup vertex colors

	Scene *	scene = mesh->scene;

	FloatVector4	vcOverride( 0.0f );
	/* hasVertexColors, not colors.size().
	 *
	 * BSShape::updateData pads `colors` with OPAQUE BLACK for every vertex
	 * whether or not the layout has a Vertex Colors field (bsshape.cpp:90),
	 * and bindShape then binds it unconditionally - so colors.size() is
	 * ALWAYS >= verts.size() for a BSShape and this test never fires. The
	 * fragment shader multiplies albedo by that attribute, so a shape which
	 * reaches this fallback renders BLACK rather than untextured grey.
	 *
	 * Measured: SecurityCamera.nif block 3 'SecurityCamera_LensGlass' has
	 * Shader Property -1, so no FO4 .prog can match it (they all require a
	 * BSLightingShaderProperty or BSEffectShaderProperty) and it lands here.
	 */
	if ( !mesh->hasVertexColors || mesh->colors.size() < mesh->verts.size()
		|| !scene->hasOption(Scene::DoVertexColors) )
		vcOverride = FloatVector4( 1.0f );

	VertexColorProperty::glProperty( props.get<VertexColorProperty>(), vcOverride, prog );

	// setup material

	MaterialProperty::glProperty( props.get<MaterialProperty>(), props.get<SpecularProperty>(), prog );

	// setup texturing

	//TexturingProperty::glProperty( props.get< TexturingProperty >() );

	// setup z buffer

	ZBufferProperty::glProperty( props.get<ZBufferProperty>() );

	if ( !mesh->depthTest ) {
		glDisable( GL_DEPTH_TEST );
	}

	if ( !mesh->depthWrite ) {
		glDepthMask( GL_FALSE );
	}

	// setup stencil

	StencilProperty::glProperty( props.get<StencilProperty>() );

	// wireframe ?

	bool	isWireframe = WireframeProperty::glProperty( props.get<WireframeProperty>() );

	mesh->setUniforms( currentProgram );

	if ( isWireframe )
		return;

	// setup texturing

	for ( int i = 0; i < TexturingProperty::numTextures; i++ ) {
		prog->uni1i_l( prog->uniLocation( "textureUnits[%d]", i ), 0 );
		prog->uni1i_l( prog->uniLocation( "textures[%d].textureUnit", i ), 0 );
	}

	int	stage = 0;

	if ( TexturingProperty * texprop = props.get<TexturingProperty>() ) {
		// standard multi texturing property

		// base
		stage += int( texprop->bind( 0, stage, prog ) );

		// dark
		stage += int( texprop->bind( 1, stage, prog ) );

		// detail
		stage += int( texprop->bind( 2, stage, prog ) );

		// glow
		stage += int( texprop->bind( 4, stage, prog ) );

		// decal 0
		stage += int( texprop->bind( 6, stage, prog ) );

		// decal 1
		stage += int( texprop->bind( 7, stage, prog ) );

		// decal 2
		stage += int( texprop->bind( 8, stage, prog ) );

		// decal 3
		stage += int( texprop->bind( 9, stage, prog ) );

	} else if ( TextureProperty * texprop = props.get<TextureProperty>() ) {
		// old single texture property
		stage += int( texprop->bind( prog ) );
	}

	if ( !stage ) {
		scene->textures->activateTextureUnit( 0 );
		scene->textures->bind( white, scene->nifModel );
	}
}

bool Renderer::drawSkyBox( Scene * scene )
{
	static const std::uint16_t	skyBoxTriangles[36] = {
		1, 5, 3,  3, 5, 7,  0, 2, 4,  4, 2, 6,	// +X, -X
		2, 3, 6,  6, 3, 7,  0, 4, 1,  1, 4, 5,	// +Y, -Y
		4, 6, 5,  5, 6, 7,  0, 1, 2,  2, 1, 3	// +Z, -Z
	};
	static const float	skyBoxVertices[24] = {
		-1.125f, -1.125f, -1.125f,   1.125f, -1.125f, -1.125f,  -1.125f,  1.125f, -1.125f,   1.125f,  1.125f, -1.125f,
		-1.125f, -1.125f,  1.125f,   1.125f, -1.125f,  1.125f,  -1.125f,  1.125f,  1.125f,   1.125f,  1.125f,  1.125f
	};

	if ( globalUniforms->cubeBgndMipLevel < 0 || !scene->nifModel || scene->nifModel->getBSVersion() < 151
		|| scene->selecting || scene->hasVisMode( Scene::VisSilhouette ) ) {
		return false;
	}

	const NifModel *	nif = scene->nifModel;
	quint32	bsVersion = nif->getBSVersion();
	Program *	prog = useProgram( "skybox.prog" );
	if ( !prog )
		return false;

	glDisable( GL_POLYGON_OFFSET_FILL );
	glEnable( GL_FRAMEBUFFER_SRGB );

	// texturing

	int	texunit = 0;

	// Always bind cube to texture unit 0, regardless of shader settings
	bool	hasCubeMap = scene->hasOption(Scene::DoCubeMapping) && scene->hasOption(Scene::DoLighting);
	GLint	uniCubeMap = prog->uniLocation( "CubeMap" );
	if ( uniCubeMap < 0 ) {
		stopProgram();
		return false;
	}
	fn->glActiveTexture( GL_TEXTURE0 + texunit );
	if ( hasCubeMap )
		hasCubeMap = scene->bindCube( bsVersion < 170 ? cfg.cubeMapPathFO76 : cfg.cubeMapPathSTF );
	if ( !hasCubeMap )
		scene->bindCube( grayCube, 1 );
	fn->glUniform1i( uniCubeMap, texunit++ );

	prog->uni1i( "hasCubeMap", hasCubeMap );
	prog->uni1b( "invertZAxis", ( bsVersion < 170 ) );

	glDisable( GL_BLEND );
	glDisable( GL_DEPTH_TEST );
	glDepthMask( GL_FALSE );
	glDepthFunc( GL_ALWAYS );
	glEnable( GL_CULL_FACE );
	glCullFace( GL_BACK );
	glPolygonMode( GL_FRONT_AND_BACK, GL_FILL );

	const float *	vertexPositions = skyBoxVertices;

	drawShape( 8, 3, 36, GL_TRIANGLES, GL_UNSIGNED_SHORT, &vertexPositions, skyBoxTriangles );

	stopProgram();
	glDepthMask( GL_TRUE );

	return true;
}
