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
#include "gl/cellpasstime.h"	// lane PERF2: the draws' CPU setup time
#include "gl/celldrawstate.h"	// lane PERF3
#include "gl/celldrawpacket.h"	// lane GAPPERF-BINDLESS
#include "gl/cellbindless.h"	// lane BINDLESS2
#include "gl/cellqueue.h"	// lane GAPPERF-BINDLESS: the sampler census reads every bind
#include "gl/cellvrs.h"	// lane VRS1
#include "gl/cellfxres.h"	// lane FXRES1
#include "wwswitch.h"	// lane TESTDRIVER1: the WW_* switch store

#include "message.h"
#include "nifskope.h"
#include "gl/glshape.h"
#include "gl/glproperty.h"
#include "gl/glscene.h"
#include "gl/gltex.h"
#include "gl/scenelighting.h"
#include "gl/lookdevstage.h"
#include "gl/atmosphere.h"	// lane SKYA
#include "gl/sunshadow.h"
#include "gl/celllights.h"
#include "gl/cellfxlit.h"
#include "gl/cellwater.h"	// lane WATER1
#include "cellsplat.h"	// lane TERRBLEND1: wwCellLandIs
#include "gl/cellgrass.h"	// lane GRASSMB1
#include "gl/cellcull.h"	// lane DECALS1: the placed decals' parallax
#include "gl/celllooks.h"
#include "gl/cellssr.h"
#include "gl/cellreflprobe.h"	// lane REFLPROBE1
#include "gl/cellssgi.h"
#include "esmweather.h"
#include "io/material.h"
#include "model/nifmodel.h"
#include "ui/settingsdialog.h"
#include "gl/BSMesh.h"
#include "gl/cellhdr.h"
#include "gl/cellfogbloom.h"
#include "gl/cellunits.h"	// lane PERF1: the first dedicated unit
#include "gl/cellvolair.h"	// lane VOLPART1: volumetric lit effect cards
#include "gl/cellfarground.h"	// lane FARDRAW1: the far ring 0's live ground
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
#include <unordered_map>

/* lane PERF1: the scene-global setters (fog + air, sun shadow, cell lights) skipped on a shape whose program already
 * holds this pass's values. Their output depends on the scene, the pass and the program only, and each one reads
 * GL state back (glGetIntegerv), a driver sync paid once per shape. WW_SETUP_MEMO=0 runs them on every shape, 2 keeps
 * a stamp per program instead of the last one; WW_SETUP_MEMO_CHECK=1 runs them anyway and counts any GL state they
 * would have changed (the identity gate). */
namespace
{
struct SetupMemo
{
	int mode = 1;
	bool check = false;
	bool armed = false;
	quint64 epoch = 1;
	std::unordered_map<const void *, std::pair<const void *, quint64>> stamps;	// program -> scene, epoch
	qint64 hits = 0, runs = 0, checked = 0, bad = 0, arms = 0;
	SetupMemo()
	{
		const QByteArray v = wwEnv( "WW_SETUP_MEMO" );
		mode = v.isEmpty() ? 1 : v.toInt();
		check = wwEnvInt( "WW_SETUP_MEMO_CHECK" ) != 0;
	}
};

SetupMemo & setupMemo()
{
	static SetupMemo m;
	static quint64 liveGen = 0;
	if ( wwSwitchMoved( liveGen ) ) {	// lane TESTDRIVER1: any live set may move a uniform the memo would skip; old stamps are void
		const QByteArray v = wwEnv( "WW_SETUP_MEMO" );
		m.mode = v.isEmpty() ? 1 : v.toInt();
		m.check = wwEnvInt( "WW_SETUP_MEMO_CHECK" ) != 0;
		m.epoch++;
		m.stamps.clear();
	}
	return m;
}

struct GlSnap
{
	GLint active = 0;
	std::vector<GLint> tex;
	std::vector<std::pair<std::string, std::array<GLint, 16>>> uni;
};

void glSnap( Renderer * r, GLuint prog, GlSnap & o )
{
	auto fn = r->fn;
	static const GLenum binds[] = { GL_TEXTURE_BINDING_2D, GL_TEXTURE_BINDING_2D_ARRAY, GL_TEXTURE_BINDING_3D,
		GL_TEXTURE_BINDING_CUBE_MAP, GL_TEXTURE_BINDING_CUBE_MAP_ARRAY, GL_TEXTURE_BINDING_BUFFER, GL_TEXTURE_BINDING_2D_MULTISAMPLE };
	fn->glGetIntegerv( GL_ACTIVE_TEXTURE, &o.active );
	GLint units = 0;
	fn->glGetIntegerv( GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, &units );
	units = std::min( units, 32 );
	o.tex.clear();
	for ( int u = 0; u < units; u++ ) {
		fn->glActiveTexture( GLenum( GL_TEXTURE0 + u ) );
		for ( GLenum b : binds ) {
			GLint t = 0;
			fn->glGetIntegerv( b, &t );
			o.tex.push_back( t );
		}
	}
	fn->glActiveTexture( GLenum( o.active ) );
	o.uni.clear();
	GLint n = 0;
	fn->glGetProgramiv( prog, GL_ACTIVE_UNIFORMS, &n );
	for ( GLint i = 0; i < n; i++ ) {
		char name[256];
		GLsizei len = 0;
		GLint size = 0;
		GLenum type = 0;
		fn->glGetActiveUniform( prog, GLuint( i ), sizeof( name ), &len, &size, &type, name );
		std::string base( name, size_t( len ) );
		const bool isArray = base.size() > 3 && base.compare( base.size() - 3, 3, "[0]" ) == 0;
		if ( isArray )
			base.resize( base.size() - 3 );
		const bool isFloat = type == GL_FLOAT || type == GL_FLOAT_VEC2 || type == GL_FLOAT_VEC3 || type == GL_FLOAT_VEC4
			|| type == GL_FLOAT_MAT2 || type == GL_FLOAT_MAT3 || type == GL_FLOAT_MAT4;
		for ( GLint k = 0; k < ( isArray ? size : 1 ); k++ ) {
			const std::string nm = isArray ? base + "[" + std::to_string( k ) + "]" : base;
			const GLint l = fn->glGetUniformLocation( prog, nm.c_str() );
			if ( l < 0 )
				continue;	// a block member: the setters never write a block
			std::array<GLint, 16> v{};
			if ( isFloat )
				fn->glGetUniformfv( prog, l, reinterpret_cast<GLfloat *>( v.data() ) );
			else
				fn->glGetUniformiv( prog, l, v.data() );
			o.uni.emplace_back( nm, v );
		}
	}
}

int glSnapDiff( const GlSnap & a, const GlSnap & b, bool print, const char * tag = "setup memo" )
{
	int d = ( a.active != b.active ) ? 1 : 0;
	if ( d && print )
		std::fprintf( stderr, "%s: MISMATCH active texture %d -> %d\n", tag, a.active, b.active );
	for ( size_t i = 0; i < a.tex.size() && i < b.tex.size(); i++ )
		if ( a.tex[i] != b.tex[i] ) {
			d++;
			if ( print )
				std::fprintf( stderr, "%s: MISMATCH unit %d target %d texture %d -> %d\n", tag, int( i / 7 ), int( i % 7 ), a.tex[i], b.tex[i] );
		}
	for ( size_t i = 0; i < a.uni.size() && i < b.uni.size(); i++ )
		if ( a.uni[i].second != b.uni[i].second ) {
			d++;
			if ( print )
				std::fprintf( stderr, "%s: MISMATCH uniform %s\n", tag, a.uni[i].first.c_str() );
		}
	if ( a.uni.size() != b.uni.size() || a.tex.size() != b.tex.size() )
		d++;
	return d;
}
}

namespace {
struct OpaqueCut
{
	unsigned serial = 1;
	bool armed = false;
	const char * pass = "";
	long long drawn = 0, skips = 0, wrong = 0;	// check mode, this armed run
	int saw = -1;
	QSet<QString> writers;
};
OpaqueCut & opaqueCut()
{
	static OpaqueCut c;
	return c;
}
}

void wwOpaqueCutNewFrame()
{
	opaqueCut().serial++;
}

bool wwOpaqueCutChecking()
{
	WW_SWITCH_STATIC( bool, on, qEnvironmentVariableIntValue( "WW_CELL_OPAQUE_CUT_CHECK" ) == 1 );
	return on;
}

void wwOpaqueCutArm( bool on, const char * pass )
{
	OpaqueCut & c = opaqueCut();
	if ( !on && c.armed && wwOpaqueCutChecking() )
		std::fprintf( stderr, "opaque cut check (%s): sorted draws=%lld predicted skips=%lld wrong=%lld (must be 0)\n",
			c.pass, c.drawn, c.skips, c.wrong );
	c.armed = on;
	c.pass = pass;
	c.drawn = c.skips = c.wrong = 0;
}

bool wwOpaqueCutArmed()
{
	return opaqueCut().armed;
}

bool wwOpaqueCutSkips( const Shape * s )
{
	// a record from this or the last two passes' frames (an edit shows at most that late); none: draw
	const OpaqueCut & c = opaqueCut();
	return c.armed && s->wwCutStamp != 0 && c.serial - s->wwCutStamp <= 2 && !s->wwCutWrote;
}

void wwOpaqueCutSaw( bool opaque )
{
	opaqueCut().saw = opaque ? 1 : 0;
}

void wwOpaqueCutCheckNote( bool predictedSkip, const QString & name )
{
	OpaqueCut & c = opaqueCut();
	const bool wrote = c.saw != 0;	// -1 (the pass never judged it): counted as written, never skippable
	c.saw = -1;
	c.drawn++;
	c.skips += predictedSkip ? 1 : 0;
	c.wrong += ( predictedSkip && wrote ) ? 1 : 0;
	// the sorted shapes that write in the opaque pass (unblended, cell-lit): what the old full cut dropped
	if ( wrote && c.writers.size() < 64 && !c.writers.contains( name ) ) {
		c.writers.insert( name );
		std::fprintf( stderr, "opaque cut writer (%s): '%s'\n", c.pass, qPrintable( name ) );
	}
}

void wwSetupMemoArm( bool on )
{
	SetupMemo & m = setupMemo();
	m.epoch++;
	m.stamps.clear();
	m.armed = on && m.mode > 0;
	if ( on && ++m.arms % 300 == 0 )
		std::fprintf( stderr, "setup memo: mode=%d check=%d passes=%lld kept=%lld ran=%lld checked=%lld mismatches=%lld\n", m.mode,
			m.check ? 1 : 0, (long long)m.arms, (long long)m.hits, (long long)m.runs, (long long)m.checked, (long long)m.bad );
}

//! lane PERF1: runs the scene-global setters (all of them, in one call) unless this program already holds this pass's values
template <typename F> static void wwSceneGlobals( Renderer * r, Scene * scene, int texunit, F run )
{
	SetupMemo & m = setupMemo();
	NifSkopeOpenGLContext::Program * prog = r->getCurrentProgram();
	// a shape that bound a texture on a dedicated unit (10 and up) must have the setters bind theirs back
	const bool can = m.armed && prog && texunit < WwCellUnit::Shadow;
	if ( can ) {
		auto it = m.stamps.find( prog );
		if ( it != m.stamps.end() && it->second.first == scene && it->second.second == m.epoch ) {
			m.hits++;
			if ( !m.check )
				return;
			GlSnap a, b;
			glSnap( r, prog->id, a );
			run();
			glSnap( r, prog->id, b );
			m.checked++;
			m.bad += glSnapDiff( a, b, m.bad < 20 );
			return;
		}
	}
	m.runs++;
	run();
	if ( can ) {
		if ( m.mode == 1 )
			m.stamps.clear();
		m.stamps[prog] = { scene, m.epoch };
	}
}


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

/* lane FXCHECK1: the game scales an effect's cube by the env mask's R (specular), not G (gloss, often 1.0).
 * WW/FxEnvMaskR or WW_FX_ENVMASK (off until seen); red WW_FX_ENVMASK_RED=gloss (old) | noenv (no cube). */
bool wwFxEnvMaskR()
{
	WW_SWITCH_STATIC( int, v, [] {	// lane LIVESET1: follows a live set (the uniform is set per draw)
		const QByteArray red = qgetenv( "WW_FX_ENVMASK_RED" ).trimmed();
		const QByteArray pin = qgetenv( "WW_FX_ENVMASK" ).trimmed();
		const bool on = !pin.isEmpty() ? pin != "0" : QSettings().value( QStringLiteral( "WW/FxEnvMaskR" ), false ).toBool();
		const bool r = on && red != "gloss";
		fprintf( stderr, "fx env mask: %s\n", r ? "R (the game's)" : on ? "G (red WW_FX_ENVMASK_RED=gloss)" : "G (as before)" );
		return r ? 1 : 0;
	}() );
	return v != 0;
}

static bool wwFxEnvNone()
{
	WW_SWITCH_STATIC( bool, v, qgetenv( "WW_FX_ENVMASK_RED" ).trimmed() == "noenv" );
	return v;
}


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

static Scene * wwAoScene = nullptr;	// lane AO1: the drawing scene, set by setupProgram (Shape::scene is protected)

/* lane ALLON2: the blended-ground layer-edge guard (fo4_default.frag / pbrm_default.frag landSplatDbg).
 * 0 = guard on; red WW_TERRBLEND_RED=edgepass = 1 (the old output); probe WW_TERRBLEND_PROBE=edge = 2 (a pass
 * whose colour or share came out non-finite draws green, everything else black). */
static int wwLandSplatDbg()
{
	WW_SWITCH_STATIC( int, v, wwEnv( "WW_TERRBLEND_PROBE" ) == "edge" ? 2 : wwEnv( "WW_TERRBLEND_RED" ) == "edgepass" ? 1 : 0 );
	return v;
}

// lane PERF3: the per-draw GL query cache (celldrawstate.h)
namespace {
struct DrawGlCache
{
	int depth = 0;
	int blend = -1;
	GLint active = -1;
	GLint vp[4] = { 0, 0, 1, 1 };
	bool vpOk = false;
};
DrawGlCache & drawGl() { static DrawGlCache c; return c; }
bool envOn( const char * sw, const char * red )
{
	return qgetenv( sw ).trimmed() != "0" && qgetenv( red ).trimmed() != "1" && qgetenv( "WW_PERF3_RED" ).trimmed() != "1";
}
bool drawGlOn() { static const bool on = envOn( "WW_DRAW_GLCACHE", "WW_DRAW_GLCACHE_RED" ); return on; }
}

WwDrawGlScope::WwDrawGlScope()
{
	DrawGlCache & c = drawGl();
	if ( drawGlOn() && c.depth++ == 0 ) {
		c.blend = -1;
		c.active = -1;
		c.vpOk = false;
	}
}

WwDrawGlScope::~WwDrawGlScope()
{
	if ( drawGlOn() )
		drawGl().depth--;
}

bool wwDrawBlend()
{
	DrawGlCache & c = drawGl();
	if ( !c.depth )
		return glIsEnabled( GL_BLEND );
	if ( c.blend < 0 )
		c.blend = glIsEnabled( GL_BLEND ) ? 1 : 0;
	return c.blend != 0;
}

int wwDrawActiveTexture()
{
	DrawGlCache & c = drawGl();
	if ( !c.depth || c.active < 0 ) {
		GLint a = 0;
		glGetIntegerv( GL_ACTIVE_TEXTURE, &a );
		if ( !c.depth )
			return int( a );
		c.active = a;
	}
	return int( c.active );
}

void wwDrawViewport( int vp[4] )
{
	DrawGlCache & c = drawGl();
	if ( !c.depth ) {
		GLint v[4] = { 0, 0, 1, 1 };
		glGetIntegerv( GL_VIEWPORT, v );
		for ( int i = 0; i < 4; i++ )
			vp[i] = int( v[i] );
		return;
	}
	if ( !c.vpOk ) {
		glGetIntegerv( GL_VIEWPORT, c.vp );
		c.vpOk = true;
	}
	for ( int i = 0; i < 4; i++ )
		vp[i] = int( c.vp[i] );
}

void wwDrawGlCacheDirty()
{
	drawGl().blend = -1;
}

bool wwCellAoLean()
{
	static const bool on = envOn( "WW_CELL_AO_LEAN", "WW_CELL_AO_LEAN_RED" );
	return on;
}

static NifSkopeOpenGLContext::Program * wwProgramCensus( const NifModel * nif, Shape * mesh,
	const BSShaderLightingProperty * wwSp, const char * wwKind,
	int msn, int lodLand, NifSkopeOpenGLContext::Program * program,
	const FloatVector4 & lightViewDir )
{
	const WwDrawGlScope drawGlScope;	// lane PERF3
	/* lane IMGS1: the imagespace's measure pass holds the cell-lit fragments' linear light. Any
	 * other program (sky, debug) writes neither colour nor depth there; glview.cpp
	 * restores both masks after the pass. The stencil: bit 0 = a cell-lit fragment landed here (the
	 * pixels the mean counts; the void around a model is not in the game's frame), bit 1 = the last
	 * fragment was blended or not cell-lit (the gate compares the picture where the value is 1 only).
	 * Lane EXPO1: the FO4 effect shader writes its colour too, blended as drawn: fo4_effectcell (lane EFX2)
	 * writes the game's linear effect value (texture x vertex colour x base colour, decoded, faded, fogged),
	 * as the game draws its effects into the HDR target its adaptation and bloom read (INFERRED from the
	 * glows blooming in game). Measured on the walkway, see docs/PRTP_PLAN.md 2p. */
	if ( wwCellImageSpaceIsMeasuring() ) {
		// lane DECALLIT1: an effect decal is part of the surface it lies on (the game's deferred decal groups)
		const bool cell = program && ( wwIsCellProgramName( program->name ) || wwIsCellDecalProgramName( program->name ) );
		const bool fx = program && ( program->name == std::string_view( "fo4_effectshader.prog" )
			|| program->name == std::string_view( "fo4_effectcell.prog" ) ) && wwCellImageSpaceMeasuresEffects();
		glColorMask( cell || fx, cell || fx, cell || fx, cell || fx );
		if ( !cell )
			glDepthMask( GL_FALSE );
		glEnable( GL_STENCIL_TEST );
		glStencilMask( cell ? 0x03 : 0x02 );
		glStencilFunc( GL_ALWAYS, !cell ? 2 : wwDrawBlend() ? 3 : 1, 0xFF );
		glStencilOp( GL_KEEP, GL_KEEP, GL_REPLACE );
	} else if ( wwCellHdrActive() ) {
		// lane HDR1: the linear frame (cellhdr.h). The stencil keeps who wrote a pixel last: 1 a cell program or a
		// cell effect (linear light, tone-mapped once at the end), 2 any other program (its value as written)
		// WATERHDR1: and the water, which writes linear light too (it was written back raw: the creek ~6/255)
		const bool lin = program && ( wwIsCellProgramName( program->name ) || program->name == std::string_view( "fo4_effectcell.prog" )
			|| wwIsCellDecalProgramName( program->name )	// lane DECALLIT1
			|| program->name == std::string_view( "fo4_effectcellfx.prog" )	// lane FXRES1: the layer's (composited as 1)
			|| ( program->name == std::string_view( "fo4_water.prog" ) && wwCellWaterWritesLinear() ) );
		// lane FOGBLOOM1 (cellfogbloom.h): a blended linear draw (an effect, glass) also sets bit 2, the bloom it takes in fog
		const bool fogMark = lin && wwCellFogBloomMarks() && glIsEnabled( GL_BLEND )
			&& program->name != std::string_view( "fo4_water.prog" ) && !wwIsCellDecalProgramName( program->name );
		glEnable( GL_STENCIL_TEST );
		glStencilMask( wwCellFogBloomMarks() ? 0x07 : 0x03 );
		glStencilFunc( GL_ALWAYS, ( lin ? 1 : 2 ) | ( fogMark ? 4 : 0 ), 0xFF );
		glStencilOp( GL_KEEP, GL_KEEP, GL_REPLACE );
	}
	// lane FXRES1: in the soft-effect layer, a draw it cannot hold exactly is masked and drawn in place after it
	if ( mesh && wwCellFxResDrawing() )
		wwCellFxResDrawHook( mesh, program ? program->name : std::string_view() );
	// lane PERF2FIX1: what this draw writes in the obscurance/reflection opaque passes (their own test, below)
	if ( mesh && wwKind ) {
		mesh->wwCutWrote = program && wwIsCellProgramName( program->name ) && !glIsEnabled( GL_BLEND );
		mesh->wwCutStamp = opaqueCut().serial;
	}
	// lane AO1: the obscurance's opaque pass (normal + depth from the opaque cell-lit draws only), and every
	// other cell-lit draw reads this frame's obscurance (celllights.h)
	if ( mesh && wwAoScene )
		wwCellAoDraw( wwAoScene, program && ( wwIsCellProgramName( program->name ) ) );
	// lane PRTP6: a water shape drawn through a cell program reads no probe GI (water is dynamic)
	if ( mesh && wwAoScene )
		wwCellGiWaterDraw( wwAoScene, mesh->id(), program && wwIsCellProgramName( program->name ) );
	// lane SSR1: the reflections' scene pass, and the reflection a flagged opaque draw reads (cellssr.h)
	// lane PERF3: not in the AO opaque pass (the main pass sets them again per draw)
	const bool aoLean = wwCellAoInPass() && wwCellAoLean();
	if ( mesh && wwAoScene && !aoLean )
		wwCellSsrDraw( wwAoScene, program && ( wwIsCellProgramName( program->name ) ) );
	// lane REFLPROBE1: the placed probes a cell-lit env-mapped draw reads (cellreflprobe.h)
	if ( mesh && wwAoScene )
		wwCellReflProbeDraw( wwAoScene, program && ( wwIsCellProgramName( program->name ) ) );
	// lane SSGI1: the screen-space bounce a cell-lit opaque draw reads (cellssgi.h)
	if ( mesh && wwAoScene && !aoLean )
		wwCellSsgiDraw( wwAoScene, program && ( wwIsCellProgramName( program->name ) ) );
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
		if ( program && ( served == QLatin1StringView( "pbrm_default.prog" ) || served == QLatin1StringView( "pbrm_csm.prog" )
			|| served == QLatin1StringView( "pbrm_cell.prog" ) || served == QLatin1StringView( "pbrm_cellcsm.prog" ) ) ) {
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
	static quint64	liveGen = 0;	// lane LIVESET1: a live set arms (or disarms) it
	if ( armed < 0 || wwSwitchMovedFor( liveGen, { "WW_PROGRAM_CENSUS" } ) ) {
		QString	p = QString::fromLocal8Bit( wwEnv( "WW_PROGRAM_CENSUS" ) ).trimmed();
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
 * texture object is shared with the other programs, so every change is undone
 * at the start of the next shape's program setup -- back to the state the
 * texture had (lane SRGBTAG1: a Fallout 4 file texture already loads SKIPPED,
 * TexCache::wwFo4SrgbTagAsUnorm, and must stay so; other textures decode). */
bool wwLegacyRouteRed();	// lane LEGACYROUTE1, defined beside setupProgramPBRM

namespace {
struct WwDecodeChange { const void * ctx; GLuint tex; GLint prevMode; };
QVector<WwDecodeChange> & wwSkippedDecode()
{
	static QVector<WwDecodeChange> v;
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
		if ( v[i].ctx != ctx )
			continue;
		if ( glIsTexture( v[i].tex ) ) {
			glBindTexture( GL_TEXTURE_2D, v[i].tex );
			glTexParameteri( GL_TEXTURE_2D, 0x8A48, v[i].prevMode );	// TEXTURE_SRGB_DECODE_EXT, as it was
		}
		v.remove( i );
	}
	glBindTexture( GL_TEXTURE_2D, GLuint( prev ) );
}

/* lane CUBE1: the game keeps its env cubes in an sRGB array, so its sampler decodes each texel BEFORE the filter
 * and its shader decodes nothing. The loader tags a DX10 B8G8R8A8 cube as sRGB: the same order, and the cell term
 * must not decode it again (measured: decoded twice, the gate's first run sat at 0.35 of the expected). Called
 * with the cube bound on the active unit; returns the shader's cube mode: 3 = the sampler decodes, 1 = an
 * untagged cube, decoded in the shader (after the filter: the nearest an untagged texture allows) */
float wwCellCubeDecodeMode()
{
	GLint tex = 0;
	glGetIntegerv( GL_TEXTURE_BINDING_CUBE_MAP, &tex );
	if ( !tex )
		return 1.0f;
	GLint f = 0;
	glGetTexLevelParameteriv( GL_TEXTURE_CUBE_MAP_POSITIVE_X, 0, GL_TEXTURE_INTERNAL_FORMAT, &f );
	if ( f != 0x8C40 && f != 0x8C41 && f != 0x8C42 && f != 0x8C43 && ( f < 0x8C48 || f > 0x8C4F ) && f != 0x8E8D )
		return 1.0f;	// not sRGB-tagged: the sampler returns the stored bytes
	return 3.0f;
}
} // namespace

NifSkopeOpenGLContext::Program * Renderer::setupProgram( Shape * mesh, Program * hint )
{
	PassTime::DrawCpuScope wwSetupT( PassTime::CpuSetup );	// lane PERF2
	wwRestoreSrgbDecode();
	const NifModel *	nif = mesh->scene->nifModel;
	wwAoScene = mesh->scene;	// lane AO1
	if ( wwCellVrsActive() ) {	// lane VRS1: a draw that can discard (alpha test, effect, PBR composition) shades 1x1
		const Material * vm = mesh->bslsp ? mesh->bslsp->getMaterial() : nullptr;
		wwCellVrsDraw( ( mesh->alphaProperty && mesh->alphaProperty->hasAlphaTest() ) || ( vm && vm->hasAlphaTest() ) || mesh->bsesp
			|| ( mesh->bslsp && mesh->bslsp->pbrmValid ) );
	}
	{	// lane SSR1: the draw's reflection flag (an environment-mapped material file with its reflections switch on)
		const Material * sm = mesh->bslsp ? mesh->bslsp->getMaterial() : nullptr;
		wwCellSsrNote( sm && sm->isShaderMaterial() && sm->bEnvironmentMapping && sm->bScreenSpaceReflections );
		wwCellReflProbeNote( mesh->bslsp && mesh->bslsp->greyscaleColor );	// lane REFLPROBE1: the palette owns GreyscaleMap
	}

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
	//   PBR            : only where a real PBR material resolved (pbrmValid)
	//   Legacy and PBR : the same
	/* lane LEGACYROUTE1 (bungo's rule: legacy materials draw with legacy math, PBR materials with PBR math).
	 * The game has no PBR materials, so a vanilla material must look the same in every renderer mode: it now
	 * draws exactly the program the Legacy mode picks, with the same uniforms. The PBR renderer mode used to
	 * drive it through pbrm_default with a material derived from its spec/gloss slots (the "legacy fallback"),
	 * which misread what the legacy shaders read (the far terrain's _msn read near black, FARLOD3).
	 * WW_LEGACYROUTE_RED=oldroute: the PBR renderer mode sends every lit shape to the PBR program again --
	 * the planted red of the parity gate (notes/legacyroute1/gates.sh). */
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

	/* lane WATER1: a cell water surface (src/gl/cellwater.h) takes fo4_water.prog by name while the draw is
	 * cell-lit: the matrices, the cell's uniforms and the fog through the lighting setup, then the record's */
	if ( mesh->bslsp && wwLodChannelView == 0 && wwCellWaterWanted( mesh->scene, mesh->id() ) ) {
		if ( Program * program = useProgram( "fo4_water.prog" ) ) {
			if ( setupProgramCE1( nif, program, mesh ) && wwCellWaterUniforms( mesh->scene, mesh->id(), 0 ) )
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
	const bool wantPbrm = mesh->bslsp && wwLodChannelView == 0 && lightingMode != PbrmModeLegacy
		&& ( mesh->bslsp->pbrmValid || ( lightingMode == PbrmModePBR && wwLegacyRouteRed() ) );
	if ( wantPbrm ) {
		/* Cascaded sun shadows (lane CSM1) are their own program, pbrm_csm.prog: the
		 * same pbrm_default.frag with WW_SUNSHADOW defined, chosen only while the
		 * shadow map was built this frame -- Shadows off runs the pre-CSM shader. */
		/* lane PRTPGI: a cell-lit draw takes pbrm_cell.prog (the cell's lights + the bounce) */
		/* lane SUNCELL1: and pbrm_cellcsm.prog while that draw also receives the cascades (the Shadows row on) */
		const char * pbrmWant = wwCellLightsWanted( mesh->scene )
			? ( wwSunShadowWanted( mesh->scene ) ? "pbrm_cellcsm.prog" : "pbrm_cell.prog" )
			: wwSunShadowWanted( mesh->scene ) ? "pbrm_csm.prog" : "pbrm_default.prog";
		if ( Program * program = useProgram( pbrmWant ) ) {
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
		&& ( ( pbrmProgramSeen && hint == pbrmProgramSeen ) || ( routeProgramSeen && hint == routeProgramSeen )
			|| hint->name == std::string_view( "pbrm_csm.prog" )	// lane CSM1: the shadow variant is never a hint
			|| hint->name == std::string_view( "pbrm_cell.prog" )	// lane PRTPGI: nor the cell-lit one
			|| hint->name == std::string_view( "pbrm_cellcsm.prog" )	// lane SUNCELL1: nor its cascade variant
			|| hint->name == std::string_view( "fo4_water.prog" ) );	// lane WATER1: nor the water's
	/* Weather fog (lane FOG1) has its own program, fo4_fog.prog: the same
	 * fo4_default.frag with WW_FOG defined. With the fog code merely present and
	 * fogOn false, the driver compiled fo4_default differently -- 783 px of the
	 * legacy zero set moved (GRailCurveR01, alpha-test edges, up to 56 levels).
	 * So fo4_default.prog stays the pre-fog shader, and the fog program is swapped
	 * in by name only while this draw fogs (a hint is swapped either way). */
	/* lane BINDLESS2: a cell-lit FO4 lighting draw takes the bindless twin of fo4_cell / fo4_cellcsm while the Bindless
	 * draws row is on: the shapes the draw packets record (no selection, refraction or census pass) */
	auto wwBlWants = [mesh]() -> bool {
		if ( !mesh->bslsp || !WwBl::on() )
			return false;
		const Scene * scene = mesh->scene;
		const auto v = scene->nifModel->getBSVersion();
		return v >= 130 && v < 151 && !scene->selecting && !( mesh->bslsp->hasRefraction && scene->showRefraction )
			&& !GpuDriven::census();
	};
	auto fogVariant = [&]( Program * p ) -> Program * {
		/* lane EFX2: a cell-lit effect draw takes fo4_effectcell.prog (the effect shader the game's way: unlit,
		 * linear, soft and near fades, fog, the cell's imagespace); red legacy keeps the viewer's effect shader */
		/* lane DECALLIT1: an effect decal takes fo4_effectdecal.prog (the game draws it into the surface under it, lit as that
		 * surface; + the cascades while the map is built); red WW_CELL_DECALLIT_RED=unlit keeps it a self-lit effect */
		if ( p->name == std::string_view( "fo4_effectshader.prog" ) || p->name == std::string_view( "fo4_effectcell.prog" )
			|| p->name == std::string_view( "fo4_effectcellfx.prog" )	// lane FXRES1
			|| wwIsCellDecalProgramName( p->name ) ) {
			const bool cellFx = wwCellLightsWanted( mesh->scene ) && !( wwCellFxRed() & 1 );
			/* lane GIDARK1: an exterior's lit effect takes the same program, lit by the spot it stands in (fxRoomLit) */
			const bool roomLit = cellFx && !mesh->effectDecal && wwCellFxRoomLit( mesh->scene, mesh->id() );
			const char * want = !cellFx ? "fo4_effectshader.prog"
				: ( ( mesh->effectDecal && !wwCellDecalLitRed() ) || roomLit )
					? ( wwSunShadowWanted( mesh->scene ) ? "fo4_effectdecalcsm.prog" : "fo4_effectdecal.prog" )
				: wwCellFxResDrawing() ? "fo4_effectcellfx.prog" : "fo4_effectcell.prog";	// lane FXRES1: the layer's outputs
			if ( p->name == std::string_view( want ) )
				return p;
			Program * q = useProgram( want );
			return q ? q : p;
		}
		if ( p->name != std::string_view( "fo4_fog.prog" ) && p->name != std::string_view( "fo4_default.prog" )
			&& p->name != std::string_view( "fo4_cell.prog" ) && p->name != std::string_view( "fo4_cellcsm.prog" )
			&& p->name != std::string_view( "fo4_csm.prog" ) && p->name != std::string_view( "fo4_fogcsm.prog" )
			&& p->name != std::string_view( "fo4_cellbl.prog" ) && p->name != std::string_view( "fo4_cellcsmbl.prog" )
			&& p->name != std::string_view( "fo4_cellfgl.prog" ) && p->name != std::string_view( "fo4_cellfgc.prog" )	// lane FARDRAW1
			&& p->name != std::string_view( "fo4_cellcsmfgl.prog" ) && p->name != std::string_view( "fo4_cellcsmfgc.prog" )
			&& p->name != std::string_view( "fo4_cellfgp.prog" ) && p->name != std::string_view( "fo4_cellcsmfgp.prog" ) )
			return p;
		// red fognoswap: never swap, so the legacy path cannot fog
		// lane SKYA: the sky's aerial perspective reads the fog program too (with the Fog row off)
		const bool fog = ( wwLookdevFogWanted( mesh->scene ) || wwAtmoApActive() ) && !wwLookdevRed( "fognoswap" );
		/* lane PRTP3: a cell-lit draw takes fo4_cell.prog (the same shader with the cell's lights,
		 * and the fog, compiled in); the row off never reaches it */
		const char * want = wwCellLightsWanted( mesh->scene )
			? ( wwSunShadowWanted( mesh->scene ) ? "fo4_cellcsm.prog" : "fo4_cell.prog" )	// lane SUNCELL1: + the cascades
			: wwSunShadowWanted( mesh->scene ) ? ( fog ? "fo4_fogcsm.prog" : "fo4_csm.prog" )	// lane SUNCELL1: Lookdev, not cell-lit
			: fog ? "fo4_fog.prog" : "fo4_default.prog";
		// lane FARDRAW1: ring 0's terrain takes the live-ground twin while the Far ground row is live
		const bool fgCsm = std::string_view( want ) == "fo4_cellcsm.prog";
		bool fgTwin = false;	// MERGE1027: a far-ground twin draws as itself, never as a bindless twin
		if ( fgCsm || std::string_view( want ) == "fo4_cell.prog" ) {
			static const char * const kTwin[2][3] = { { "fo4_cellfgl.prog", "fo4_cellfgc.prog", "fo4_cellfgp.prog" },
				{ "fo4_cellcsmfgl.prog", "fo4_cellcsmfgc.prog", "fo4_cellcsmfgp.prog" } };	// FARPAGES1: 3 = the BC1 pages
			const int t = CellFarGround::twinFor( mesh->scene->nifModel, mesh->id() );
			if ( t > 0 ) {
				want = kTwin[fgCsm ? 1 : 0][t - 1];
				fgTwin = true;
			}
		}
		if ( !fgTwin && ( want[4] == 'c' && want[5] == 'e' ) && wwBlWants() ) {	// lane BINDLESS2: fo4_cell / fo4_cellcsm
			const char * bl = want[8] == 'c' ? "fo4_cellcsmbl.prog" : "fo4_cellbl.prog";
			Program * base = useProgram( want );
			Program * q = base ? useProgram( bl ) : nullptr;
			if ( q ) {
				q->wwTwin = base;
				return q;
			}
		}
		if ( p->name == std::string_view( want ) )
			return p;
		Program * q = useProgram( want );
		return q ? q : p;
	};
	if ( hint && hint->status && !stalePbrmHint ) [[likely]] {
		Program * program = fogVariant( hint );
		fn->glUseProgram( program->id );
		currentProgram = program;
		bool	setupStatus;
		if ( nif->getBSVersion() >= 170 )
			setupStatus = setupProgramCE2( nif, program, mesh );
		else if ( nif->getBSVersion() >= 83 )
			setupStatus = setupProgramCE1( nif, program, mesh );
		else
			setupStatus = setupProgramFO3( nif, program, mesh );
		if ( setupStatus ) {
			if ( program->wwBindless )	// lane BINDLESS2: the draw's row
				WwBl::commit( fn, program );
			return wwProgramCensus( nif, mesh, wwSp, wwKind, wwMsn, wwLodLand, program,
				globalUniforms->lightSourcePosition[0] );
		}
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

	for ( Program * scanned = programsLinked; scanned; scanned = scanned->nextProgram ) {
		if ( !scanned->conditions.isEmpty() && scanned->conditions.eval( nif, iBlocks ) ) {
			Program * program = fogVariant( scanned );
			fn->glUseProgram( program->id );
			currentProgram = program;
			bool	setupStatus;
			if ( nif->getBSVersion() >= 170 )
				setupStatus = setupProgramCE2( nif, program, mesh );
			else if ( nif->getBSVersion() >= 83 )
				setupStatus = setupProgramCE1( nif, program, mesh );
			else
				setupStatus = setupProgramFO3( nif, program, mesh );
			if ( setupStatus ) {
				if ( program->wwBindless )	// lane BINDLESS2
					WwBl::commit( fn, program );
				return wwProgramCensus( nif, mesh, wwSp, wwKind, wwMsn, wwLodLand, program,
				globalUniforms->lightSourcePosition[0] );
			}
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
/*! lane LEGACYROUTE1: the planted red of the vanilla parity gate. WW_LEGACYROUTE_RED=oldroute restores the
 * pre-LEGACYROUTE1 ROUTE (the PBR renderer mode sends every lit shape to the PBR program); any other value is
 * refused by name on stderr and changes nothing. Echoed once, so a picture under it can be told apart. */
bool wwLegacyRouteRed()
{
	WW_SWITCH_STATIC( bool, v, []() {
		const QString s = wwEnvStr( "WW_LEGACYROUTE_RED" ).trimmed().toLower();
		if ( s.isEmpty() )
			return false;
		const bool on = ( s == QLatin1StringView( "oldroute" ) );
		fprintf( stderr, "WW_LEGACYROUTE_RED=%s: %s\n", qPrintable( s ),
			on ? "RED: the PBR renderer mode sends every lit shape to the PBR program (the old route)"
			   : "REFUSED: unknown value (known: oldroute); legacy materials draw their legacy program" );
		fflush( stderr );
		return on;
	}() );
	return v;
}

bool Renderer::setupProgramPBRM( const NifModel * nif, Program * prog, Shape * mesh )
{
	auto scene = mesh->scene;
	auto lsp = mesh->bslsp;
	if ( !( lsp && scene && scene->textures ) )
		return false;

	int texunit = 0;
	// the units the base and emissive textures landed on, for the sRGB-tag query
	int baseUnit = -1, emissiveUnit = -1, specUnit = -1;

	/* lane LEGACYROUTE1: only a real PBR material reaches this program -- setupProgram sends a legacy
	 * material to its legacy program in every renderer mode. The derived "legacy fallback" that drove a
	 * vanilla material from its spec/gloss slots here is retired. Under the planted red
	 * (WW_LEGACYROUTE_RED=oldroute) such a shape binds an empty material's neutral defaults: a vanilla
	 * shape drawn by the PBR program, which the parity gate must catch. */
	if ( !lsp->pbrmValid && !wwLegacyRouteRed() )
		return false;
	const PbrmMaterial & m = lsp->pbrm;

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

	{
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
	}
	/* lane FARLOD3's pbrModelSpaceNormals (a legacy _msn read in model axes by the fallback) went with the
	 * fallback (lane LEGACYROUTE1): the far terrain's _msn is now read by fo4_default.frag, as Legacy reads it. */
	// lane CUBE1: the cell's cube term reads a legacy material's specular map as the game does (the legacy slot;
	// white = the scalars alone). Only the cell program declares it; the PBR shading keeps its own derivation
	prog->uniSampler( lsp, "CellSpecMap", 7, texunit, white, lsp->clampMode );

	/* sRGB tag (lane PBRR2A, docs s5.2 "decoded as sRGB regardless of the DXGI
	 * tag"): a texture whose DXGI format is _SRGB was created with a GL sRGB
	 * internal format, so the sampler already returns linear; any other is decoded
	 * in the shader. Read off the bound texture itself (the GL truth), not guessed
	 * from the file name. Red "srgbtag": always decode, so the sRGB twin is decoded
	 * twice (lane SRGBTAG1: a Fallout 4 texture now loads with the decode
	 * skipped, so the red forces it back on for the draw). */
	const bool srgbRed = wwR2aRed( "srgbtag" );
	auto srgbAt = [&]( int unit ) -> bool {
		if ( unit < 0 )
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
			// SKIP_DECODE_EXT, or DECODE_EXT under the red; noted only when it changes
			const GLint want = srgbRed ? 0x8A49 : 0x8A4A;
			GLint cur = 0x8A49;
			glGetTexParameteriv( GL_TEXTURE_2D, 0x8A48, &cur );
			if ( cur != want ) {
				glTexParameteri( GL_TEXTURE_2D, 0x8A48, want );	// TEXTURE_SRGB_DECODE_EXT
				wwSkippedDecode().append( { c, GLuint( tex ), cur } );
			}
			return false;
		}
		return !srgbRed;
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
	const bool specV6 = m.specularV6 && wwPbrmF0Law() == PbrmF0Law::Auto;
	const float specWeight = specV6 ? m.specularWeight : 1.0f;
	const float specF0 = specV6 ? std::min( pbrmIorF0( m.specularIor ), 1.0f ) : pbrmDielectricF0( m, wwPbrmF0Law() );
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
	// -1 = the NIF alpha property: a blended-ground pass, and the red "nocomp"
	const int r4Red = wwR4RedBits();
	/* lane TERRBLEND1: a blended-ground pass keeps its OWN blend, whatever the .pbrm
	 * says: the base opaque (scaled by its share in the shader), every layer ADDED at
	 * its share. A landscape .pbrm's composition (opaque) would draw each layer over
	 * the base and bring the hard squares back the moment PBR lighting is on. */
	const bool wwLandSplat = scene && wwCellLandIs( scene->nifModel, mesh->id() );
	const int composition = ( !( r4Red & 16 ) && !wwLandSplat ) ? std::clamp( m.composition, 0, 7 ) : -1;
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
	/* lane CUBE1: a .pbrm shape is no legacy material, so mode 2 keeps its own PBR cube law there (the legacy
	 * material's game term, cell_lights.glsl cellCubeGame, is the legacy program's since LEGACYROUTE1) */
	if ( prog->uniLocation( "cellCubeMat" ) >= 0 ) {
		Material * cm = lsp->getMaterial();
		const bool specOn = ( cm && cm->isShaderMaterial() ) ? static_cast<ShaderMaterial *>( cm )->specularEnabled()
			: lsp->hasSF1( ShaderFlags::SLSF1_Specular );
		const bool specView = scene->hasOption( Scene::DoSpecular ) && scene->hasOption( Scene::DoLighting );
		prog->uni4f( "cellCubeMat", FloatVector4( specOn && specView ? lsp->specularStrength : 0.0f, lsp->specularGloss,
			hasCubeMap ? 2.0f : 0.0f, 0.0f ) );
	}

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
	wwSceneGlobals( this, scene, texunit, [scene]() {	// lane PERF1: once per program and pass
		wwLookdevFogUniforms( scene );	// lane FOG1: fogOn is false outside Lookdev
		wwSunShadowUniforms( scene );	// lane CSM1: a no-op unless this is pbrm_csm.prog
		wwCellLightsUniforms( scene );	// lane PRTPGI: a no-op unless this is pbrm_cell.prog
	} );
	if ( prog->uniLocation( "landSplat" ) >= 0 )	// lane TERRBLEND1: a blended-ground pass's alpha is its share
		prog->uni1b( "landSplat", wwCellLandIs( scene->nifModel, mesh->id() ) );
	if ( prog->uniLocation( "landSplatDbg" ) >= 0 )	// lane ALLON2: the layer-edge guard, its red and its probe
		prog->uni1i( "landSplatDbg", wwLandSplatDbg() );
	wwCellGrassUniforms( scene, mesh->id() );	// lane GRASSMB1: the grass fade, zero on every other shape

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
	} else if ( wwLandSplat ) {
		/* lane TERRBLEND1: the NiAlphaProperty the cell build wrote (4109 = SRC_ALPHA, ONE
		 * for a layer; none for a base), as the CE1 path applies it. The PBR route used
		 * to force alpha-over on every translucent shape. */
		AlphaProperty::glProperty( mesh->alphaProperty, prog );
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
	// lane PBRR4: a .pbrm composition's Transparency/Depth depthWrite (default: composition <= 1)
	const bool pbrmDepthWrite = composition >= 0 ? m.depthWrite : !mesh->translucent;
	glEnable( GL_DEPTH_TEST );
	glDepthFunc( GL_LEQUAL );
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

	const auto ce1Tail = [this, scene, prog, mesh]( int texunit ) -> bool {
		wwSceneGlobals( this, scene, texunit, [scene]() {	// lane PERF1: once per program and pass
			wwLookdevFogUniforms( scene );	// lane FOG1: fo4_default reads it; fogOn is false outside Lookdev
			wwCellLightsUniforms( scene );	// lane PRTP3: a no-op unless this is fo4_cell.prog
			wwSunShadowUniforms( scene );	// lane SUNCELL1: a no-op unless this is a cascade program (fo4_cellcsm, fo4_csm, fo4_fogcsm)
		} );
		if ( prog->uniLocation( "landSplat" ) >= 0 )	// lane TERRBLEND1: a blended-ground pass's alpha is its share
			prog->uni1b( "landSplat", wwCellLandIs( scene->nifModel, mesh->id() ) );
		if ( prog->uniLocation( "landSplatDbg" ) >= 0 )	// lane ALLON2: the layer-edge guard, its red and its probe
			prog->uni1i( "landSplatDbg", wwLandSplatDbg() );
		if ( prog->uniLocation( "editorBrdf" ) >= 0 )	// lane VANILLADEF1: Shading > Editor BRDF (off = vanilla)
			prog->uni1b( "editorBrdf", scene->editorBrdf );
		if ( prog->uniLocation( "wwDecalPom" ) >= 0 ) {	// lane DECALS1: (scale x 0.1, step cap); x = 0 is the old draw
			float sc = 0.0f, steps = 0.0f;
			const bool on = wwLookOn( WwLook::DecalParallax ) && wwCellDecalPomFor( scene->nifModel, mesh->id(), sc, steps );
			WW_SWITCH_STATIC( bool, deep, wwEnv( "WW_CELL_DECAL_POM_RED" ).trimmed() == "deep" );
			prog->uni2f( "wwDecalPom", on ? sc * ( deep ? 4.0f : 1.0f ) : 0.0f, steps );
		}
		if ( ( wwVolPartFx() & 1 ) && ( wwVolPartRed() & 64 ) && prog->name == std::string_view( "fo4_effectcell.prog" ) ) {
			prog->uni1b( "cellShadowOn", false );	// lane VOLPART1 red noshadow: the placed lights' cubes too
			setupMemo().stamps.erase( prog );	// per shape: the next shape's scene globals run again (PERF1's memo)
		}
		wwCellGrassUniforms( scene, mesh->id() );	// lane GRASSMB1: the grass fade, zero on every other shape
		if ( prog->uniLocation( "fgMode" ) >= 0 )	// lane FARDRAW1: a live-ground twin's grid, layers and decals
			CellFarGround::uniforms( scene, prog );
		if ( prog->uniLocation( "fxAdditive" ) >= 0 ) {
			// lane EFX2: an additive effect is dimmed by the fog, a blended one fogged toward its colour (the game's two PS)
			GLint dst = 0;
			glGetIntegerv( GL_BLEND_DST_RGB, &dst );
			prog->uni1b( "fxAdditive", glIsEnabled( GL_BLEND ) && dst == GL_ONE );
			if ( prog->uniLocation( "fxDecalProbe" ) >= 0 )	// lane DECALLIT1: the gate's light probe
				prog->uni1b( "fxDecalProbe", wwCellDecalLitProbe() && mesh->effectDecal && wwCellProbePass( scene ) );
			if ( prog->uniLocation( "fxRoomLit" ) >= 0 )	// lane GIDARK1: an exterior's lit effect, lit by its spot
				prog->uni1b( "fxRoomLit", !mesh->effectDecal && wwCellFxRoomLit( scene, mesh->id() ) );
			if ( wwCellFxLitProbeShape( scene, mesh->id() ) ) {
				// lane FXLIT1: its probes write the lit effects opaque, the nearest card winning: layered cards drawn
				// without a depth write leave the last one drawn, which differed between the five probe runs
				glDisable( GL_BLEND );
				glEnable( GL_DEPTH_TEST );
				glDepthFunc( GL_LEQUAL );
				glDepthMask( GL_TRUE );
			}
		}
		return true;
	};
	/* lane GAPPERF-BINDLESS (PERF5-DRAWPACKETS): a cell-lit shape's setup up to the scene globals, recorded by the
	 * frame's first pass and replayed by its later ones (celldrawpacket.h); the tail above runs every time */
	WwDp::Packet * dpSlot = nullptr;
	if ( lsp && nifVersion >= 130 && nifVersion < 151 && !scene->selecting && !( lsp->hasRefraction && scene->showRefraction )
		&& ( prog->name == std::string_view( "fo4_cell.prog" ) || prog->name == std::string_view( "fo4_cellcsm.prog" )
			|| prog->wwBindless )	// lane BINDLESS2
		&& !GpuDriven::census() )
		dpSlot = WwDp::slot( mesh, prog );
	int dpMismatch = -1;
	GlSnap dpA;
	WwDp::FixedSnap dpFixA;
	WwDp::Packet dpOld;
	WwBl::Row blA;	// lane BINDLESS2: the row the replay staged
	if ( dpSlot && WwDp::valid( *dpSlot, prog ) ) {
		std::pair<Shape *, Program *> dpLive( mesh, prog );
		WwDp::replay( *dpSlot, fn, scene->textures, []( void * c ) {
			auto * l = static_cast<std::pair<Shape *, Program *> *>( c );
			l->first->setUniforms( l->second );
		}, &dpLive );
		if ( WwDp::mode() != 2 ) {
			WwDp::counted( true, -1 );
			return ce1Tail( dpSlot->texunit );
		}
		glSnap( this, prog->id, dpA );
		WwDp::snapFixed( fn, dpFixA );
		dpOld = *dpSlot;
		blA = WwBl::staged();
		dpMismatch = 0;
	}
	struct DpGuard
	{
		bool on;
		~DpGuard()
		{
			if ( on )
				WwDp::end( false, 0 );	// a failed setup keeps no packet
		}
	} dpGuard{ dpSlot != nullptr };
	if ( dpSlot )
		WwDp::begin( *dpSlot, prog );

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
		/* lane FIXPACK1 (REDFIX1): WW_R2A_RED=nomsn now breaks the _msn read where it lives since LEGACYROUTE1
		 * (this legacy program, every renderer mode): the far ground's model-space sheet is read as tangent
		 * space again. FARLOD3's red sat in pbrm_default.frag, which no vanilla material reaches any more. */
		WW_SWITCH_STATIC( bool, redNoMsn, wwEnv( "WW_R2A_RED" ) == "nomsn" );
		prog->uni1i( "hasModelSpaceNormals",
			int( !redNoMsn && forced == &emptyString
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
		float xemi[3];	// lane LIGHTS2 (XEMI1): a placement's emittance colour replaces the emissive colour, as the game's does
		if ( wwCellXemiFor( scene->nifModel, mesh->id(), xemi ) )
			prog->uni3f( "glowColor", xemi[0], xemi[1], xemi[2] );
		else
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
		bool	ownCube = false;	// lane CUBE1: the material's own env map bound (not the default cube)
		GLint uniCubeMap = prog->uniLocation( "CubeMap" );
		if ( uniCubeMap < 0 ) {
			hasCubeMap = false;
		} else {
			fn->glActiveTexture( GL_TEXTURE0 + texunit );
			WWDP( WwDp::ActiveTex, GL_TEXTURE0 + texunit );
			QString	fname = bsprop->fileName( 4 );
			const QString *	cube = &fname;
			if ( hasCubeMap && ( fname.isEmpty() || !scene->bindCube( fname ) ) ) {
				cube = ( nifVersion < 151 ? ( nifVersion < 128 ? &cube_sk : &cube_fo4 ) : &cfg.cubeMapPathFO76 );
				hasCubeMap = scene->bindCube( *cube );
			}
			ownCube = hasCubeMap && cube == &fname;
			if ( !hasCubeMap ) [[unlikely]]
				scene->bindCube( grayCube, 1 );
			WWDP( WwDp::U1i, uniCubeMap, 0, &texunit, 1 );
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
		/* lane CUBE1: what the game's interior reflection reads (res/shaders/cell_lights.glsl cellCubeGame): the
		 * specular scale (0 with the material's specular switch off), the smoothness, 1 when the material's own
		 * env map is bound (indoors the game has no default cube), and the gate's material number */
		if ( prog->uniLocation( "cellCubeMat" ) >= 0 ) {
			const bool specOn = ( mat && mat->isShaderMaterial() ) ? static_cast<ShaderMaterial *>( mat )->specularEnabled()
				: lsp->hasSF1( ShaderFlags::SLSF1_Specular );
			const bool specView = scene->hasOption( Scene::DoSpecular ) && scene->hasOption( Scene::DoLighting );
			prog->uni4f( "cellCubeMat", FloatVector4( specOn && specView ? lsp->specularStrength : 0.0f, lsp->specularGloss,
				ownCube ? wwCellCubeDecodeMode() : 0.0f, float( ownCube ? wwCellCubeTag( lsp->getName() ) : 0 ) ) );
		}

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
		if ( prog->uniLocation( "fxEmit" ) >= 0 ) {	// lane LIGHTS2 (XEMI1): the placement's emittance colour ^2.2; w 0 = none
			float xe[3];
			const bool on = wwCellXemiFor( scene->nifModel, mesh->id(), xe );
			prog->uni4f( "fxEmit", on ? FloatVector4( std::pow( xe[0], 2.2f ), std::pow( xe[1], 2.2f ), std::pow( xe[2], 2.2f ), 1.0f )
				: FloatVector4( 0.0f, 0.0f, 0.0f, 0.0f ) );
		}
		prog->uni1f( "glowMult", scene->hasOption(Scene::DoGlow) ? esp->emissiveMult : 0.0f );

		// Falloff params

		prog->uni4f( "falloffParams", FloatVector4( esp->falloff.startAngle, esp->falloff.stopAngle,
													esp->falloff.startOpacity, esp->falloff.stopOpacity ) );

		prog->uni1f( "falloffDepth", esp->falloff.softDepth );

		/* lane EFX2 (fo4_effectcell only): a Soft effect fades where it nears the opaque surface behind it and
		 * near the eye, from the depth of the opaque pass, grabbed once per second pass */
		if ( prog->uniLocation( "fxSoft" ) >= 0 ) {
			/* lane VOLPART1: a LIT effect card while Volumetric Particles is on (cellvolair.h) is lit as a puff of the
			 * air (cell_volpart.glsl) and always soft: a slab of the card's soft depth (else kVpCardDepth units)
			 * against the opaque pass's depth, drawn with no depth test (BSShape reads wwVolPartFx) */
			constexpr float kVpCardDepth = 64.0f;
			const bool fxLit = wwCellFxLitFor( scene->nifModel, mesh->id() ) != nullptr;
			const bool vp = wwVolPartUniforms( scene ) && fxLit;
			if ( wwEnv( "WW_CELL_PARTICLES_AIR_RED" ).trimmed() == "1" && prog->uniLocation( "cellGiAirOn" ) >= 0 )
				prog->uni1b( "cellGiAirOn", false );	// SKY3 red: a lit card's puff reads the grid's shell, as before
			if ( !vp && prog->uniLocation( "volPart" ) >= 0 )
				prog->uni1i( "volPart", 0 );
			const bool espSoft = esp->soft && esp->falloff.softDepth > 0.0f;
			const bool soft = ( espSoft || vp ) && !( wwCellFxRed() & 2 ) && scene->grabEffectDepth();
			prog->uni1b( "fxSoft", soft );
			prog->uni1f( "fxSoftDepth", espSoft ? std::max( esp->falloff.softDepth, 1.0f ) : vp ? kVpCardDepth : 1.0f );
			wwVolPartFxSet( vp ? 1 | ( soft && !( wwVolPartRed() & 4 ) ? 2 : 0 ) : 0 );
			if ( vp ) {
				static QString said;
				const QString now = wwVolPartSummary() + QStringLiteral( " soft=%1" ).arg( soft ? 1 : 0 );
				if ( now != said ) {
					said = now;
					fprintf( stderr, "cell effects volumetric: %s\n", qPrintable( now ) );
				}
			}
			fn->glActiveTexture( GL_TEXTURE0 + texunit );
			fn->glBindTexture( GL_TEXTURE_2D, soft ? scene->fxDepthTexId : 0 );
			prog->uni1i_l( prog->uniLocation( "fxDepth" ), texunit++ );
			prog->uni1i( "fxRed", wwCellFxRed() );
			wwCellFxLitUniforms( scene, mesh->id() );	// lane FXLIT1: a lit effect's four placed lights
		}

		// BSEffectShader textures (FIXME: should implement using error color?)

		prog->uniSampler( bsprop, "BaseMap", 0, texunit, white, clamp,
						  scene->hasOption(Scene::DoDiffuse) ? emptyString : white );
		prog->uniSampler( bsprop, "GreyscaleMap", 1, texunit, "", TexClampMode::CLAMP_S_CLAMP_T );

		if ( nifVersion >= 130 ) {

			prog->uni1f( "lightingInfluence", esp->lightingInfluence );
			if ( wwCellFxLitProbeShape( scene, mesh->id() ) && wwEnvIsSet( "WW_CELL_FXLIT_DUMP" ) ) {
				// lane FXLIT1: what each lit effect's probe drew with, once per shape (the gate's dump)
				static QSet<int> said;
				if ( !said.contains( mesh->id() ) ) {
					said.insert( mesh->id() );
					int serial = -1;
					wwCellFxLitFor( scene->nifModel, mesh->id(), &serial );
					qInfo().noquote() << "cell fxlit: shape" << mesh->id() << "model" << serial << "influence"
						<< esp->lightingInfluence << "material" << ( esp->getMaterial() ? "read" : "none" )
						<< "shape name" << mesh->getName();
					if ( Material * fm = esp->getMaterial(); fm && fm->isEffectMaterial() )
						qInfo().noquote() << "cell fxlit:   its material: influence"
							<< static_cast<EffectMaterial *>( fm )->lightingInfluence() << "flags2"
							<< static_cast<EffectMaterial *>( fm )->effectShaderFlags2() << "textures" << fm->textures().join( "," );
				}
			}

			prog->uni1i( "hasNormalMap", esp->hasNormalMap && scene->hasOption(Scene::DoLighting)
						&& scene->hasOption(Scene::DoNormalMap) );

			prog->uniSampler( bsprop, "NormalMap", 3, texunit, default_n, clamp );

			prog->uni1i( "hasCubeMap", esp->hasEnvironmentMap );
			prog->uni1i( "hasEnvMask", esp->hasEnvironmentMask );
			float refl = 0.0;
			if ( esp->hasEnvironmentMap && scene->hasOption(Scene::DoCubeMapping) && scene->hasOption(Scene::DoLighting) )
				refl = esp->environmentReflection;

			if ( wwFxEnvNone() )
				refl = 0.0f;
			prog->uni1f( "envReflection", refl );
			prog->uni1b( "envMaskR", wwFxEnvMaskR() );

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

	{
		WwDp::Packet * dpRec = WwDp::rec;	// lane GAPPERF-BINDLESS: the transforms are never recorded
		WwDp::rec = nullptr;
		mesh->setUniforms( prog );
		WwDp::rec = dpRec;
		WWDP( WwDp::Live, 0 );
	}
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
		WWDP( WwDp::Disable, GL_CULL_FACE );
	} else {
		glEnable( GL_CULL_FACE );
		glCullFace( GL_BACK );
		WWDP( WwDp::Enable, GL_CULL_FACE );
		WWDP( WwDp::CullFace, GL_BACK );
	}

	// setup blending

	/* lane ALLON2: A BLENDED-GROUND PASS TAKES THE CELL BUILD'S ALPHA PROPERTY, NEVER ITS MATERIAL'S. A landscape
	 * set whose TXST carries no TX00 hands its .bgsm through as the bucket's name (esmdata ltexTextureSet), and the
	 * BGSM's blend then won here: no blend, so every LAYER pass was drawn OPAQUE at its share over the base -- black
	 * where the layer's share is small, hard-edged where its quads stop (bungo's circled ground at -19,23, on his
	 * stack: Boston Natural Surroundings' material-backed sets). The game's landscape shader never reads the BGSM's
	 * alpha. WW_TERRBLEND_RED=matalpha puts the material back in charge, so the gate is shown to fail on it. */
	WW_SWITCH_STATIC( bool, landMatAlphaRed, wwEnv( "WW_TERRBLEND_RED" ) == "matalpha" );
	const bool landSplatPass = !landMatAlphaRed && scene && wwCellLandIs( scene->nifModel, mesh->id() );
	if ( landSplatPass ) {
		AlphaProperty::glProperty( mesh->alphaProperty, prog );
	} else if ( mat ) {
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
			const GLint bf[4] = { GLint( blendMap[mat->iAlphaSrc] ), GLint( blendMap[mat->iAlphaDst] ), GL_ONE,
				GLint( blendMap[mat->iAlphaDst] ) };
			WWDP( WwDp::Enable, GL_BLEND );
			WWDP( WwDp::BlendSep, 0, 0, bf, 4 );
		} else {
			glDisable( GL_BLEND );
			WWDP( WwDp::Disable, GL_BLEND );
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
			WW_SWITCH_STATIC( int, treeDial, wwEnvInt( "WW_LODL_TREE_ALPHA" ) );
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
			if ( WwDp::rec ) {
				const float o[2] = { -1.0f, -1.0f };
				WWDP( WwDp::Enable, GL_POLYGON_OFFSET_FILL );
				WWDP( WwDp::PolyOffset, 0, 0, o, 2 );
			}
		}

	} else {
		// BSESP/BSLSP do not always need an NiAlphaProperty, and appear to override it at times
		if ( mesh->translucent && scene->hasOption(Scene::DoBlending) ) {
			glEnable( GL_BLEND );
			fn->glBlendFuncSeparate( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA );
			if ( WwDp::rec ) {
				const GLint bf[4] = { GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA };
				WWDP( WwDp::Enable, GL_BLEND );
				WWDP( WwDp::BlendSep, 0, 0, bf, 4 );
			}
			// If mesh is alpha tested, override threshold
			prog->uni1i( "alphaFlags", ( mesh->alphaProperty && mesh->alphaProperty->hasAlphaTest() ? 12 : 8 ) );
			prog->uni1f( "alphaThreshold", 0.1f );
		} else {
			AlphaProperty::glProperty( mesh->alphaProperty, prog );
		}

		if ( bsprop->hasSF1( ShaderFlags::SF1( ShaderFlags::SLSF1_Decal | ShaderFlags::SLSF1_Dynamic_Decal ) ) ) {
			glEnable( GL_POLYGON_OFFSET_FILL );
			glPolygonOffset( -1.0f, -1.0f );
			if ( WwDp::rec ) {
				const float o[2] = { -1.0f, -1.0f };
				WWDP( WwDp::Enable, GL_POLYGON_OFFSET_FILL );
				WWDP( WwDp::PolyOffset, 0, 0, o, 2 );
			}
		}
	}

	if ( !mesh->depthTest ) {
		glDisable( GL_DEPTH_TEST );
		WWDP( WwDp::Disable, GL_DEPTH_TEST );
	} else {
		glEnable( GL_DEPTH_TEST );
		glDepthFunc( GL_LEQUAL );
		WWDP( WwDp::Enable, GL_DEPTH_TEST );
		WWDP( WwDp::DepthFunc, GL_LEQUAL );
	}
	glDepthMask( !mesh->depthWrite || mesh->translucent ? GL_FALSE : GL_TRUE );
	glPolygonMode( GL_FRONT_AND_BACK, GL_FILL );
	WWDP( WwDp::DepthMask, !mesh->depthWrite || mesh->translucent ? GL_FALSE : GL_TRUE );
	WWDP( WwDp::PolyMode, GL_FRONT_AND_BACK, GL_FILL );

	if ( dpSlot ) {
		dpGuard.on = false;
		WwDp::end( true, texunit );
		if ( dpMismatch >= 0 ) {	// the check: the replay left the state this full setup leaves
			GlSnap b;
			WwDp::FixedSnap fb;
			glSnap( this, prog->id, b );
			WwDp::snapFixed( fn, fb );
			static int said = 0;
			const int blDiff = prog->wwBindless && std::memcmp( &blA, &WwBl::staged(), sizeof( WwBl::Row ) ) ? 1 : 0;
			if ( blDiff && said < 20 )
				std::fprintf( stderr, "draw packets: bindless row differs, shape %d\n", mesh->id() );
			dpMismatch += glSnapDiff( dpA, b, said < 20, "draw packets" ) + ( dpFixA.v != fb.v ? 1 : 0 )
				+ ( WwDp::sameOps( dpOld, *dpSlot ) ? 0 : 1 ) + blDiff;
			if ( dpMismatch && said++ < 20 )
				std::fprintf( stderr, "draw packets: MISMATCH shape %d fixed=%d ops=%d\n", mesh->id(), dpFixA.v != fb.v ? 1 : 0,
					WwDp::sameOps( dpOld, *dpSlot ) ? 0 : 1 );
			WwDp::counted( true, dpMismatch );
		}
	}
	return ce1Tail( texunit );
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
