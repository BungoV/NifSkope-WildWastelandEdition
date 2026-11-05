// lane MODULES1: the screen AO module (the depth+normal pass, its targets, the AO methods), moved from
// src/gl/celllights.cpp unchanged. The screen GI half pass and the light spheres ride along until their module moves.
#include "gl/celllights.h"
#include "wwswitch.h"	// lane TESTDRIVER1: the WW_* switch store
#include "gl/cellclustergl.h"
#include "gl/alphabgsm.h"	// the side passes' alpha-test census
#include "gl/cellssr.h"
#include "gl/cellvolair.h"
#include "gl/shadowset.h"
#include "gl/glnode.h"
#include "gl/glscene.h"
#include "gl/glshape.h"
#include "gl/glproperty.h"	// lane GAPFIX1: alpha-tested cube casters read the base map
#include "gl/renderer.h"
#include "gl/cellhdr.h"
#include "gl/cellbloom.h"	// lane BLOOM1b
#include "gl/cellaodecalgl.h"	// lane AODECAL1
#include "gl/cellaomethod.h"
#include "gl/cellcull.h"	// wwCellDecalMappedHas
#include "gl/cellpasstime.h"	// sub-stage timers
#include "gl/celldrawstate.h"
#include "gl/lookdevstage.h"
#include "gl/cellwater.h"	// water casts no cube shadow
#include "gl/cellunderwater.h"	// the WATR INAM imagespace under water
#include "esmwater.h"
#include "gl/cellwatergi.h"	// water reads no probe GI
#include "gl/celllampshadow.h"	// lane LAMPSHADOW1
#include "gl/cubefacecull.h"	// lane PERFFIX2 b: a caster draws only into the cube faces it can reach
#include "gl/lampslice.h"	// lane PERFFIX2 b: the lamp store's first stores spread over frames
#include "gl/celllooks.h"	// lane GAPLOOK1
#include "gl/celllensflare.h"	// lane LIGHTS3 (LENSFLARE1): its echo
#include "gl/cellfxlit.h"	// lane LIGHTS4: a placement's emittance colour
#include "gamemanager.h"	// lane SUNCELL1: the weather imagespace's LUT
#include "esmweather.h"	// lane WEATHERBLEND2: wwLutBlendN
#include "probealbedo.h"	// lane LIGHTS5 (GOBO1): a spot's pattern
#include "recx.h"	// lane LIGHTS5 (FOGSCALE1): fog.scale
#include "gl/cellnifxlights.h"	// lane NIFXLIGHT1: a fixture mesh's .nifx lights
#include "rtddgicell.h"	// lane RTXGI1 M2: the ray-traced GI volume's read (rtddgiCellUniforms)
#include "gl/celluniprof.h"	// lane PERFFIX1: WW_CELL_UNI_PROF
#include "gl/cellunirec.h"	// lane PERFFIX1: the cell-uniform class record
#include "gl/cloudvol.h"	// lane LIGHTNING2: a storm's flash on the exterior ambient (wwLightningGround)
#include "cellworldanim.h"	// lane NIFXLIGHT3: the world clock a .nifx light's dimmer follows
#include "gl/atmosphere.h"	// lane FIX1101A (atmo): wwAtmoWbGain, the test white balance
#include "gl/cellsunlit.h"	// lane INTSKY1: the sunlit interior rule
#include "gl/cellflickclock.h"	// lane PERFFIX2 a: the flicker's phase on the world clock
#include <QDateTime>
#include <QDir>	// lane INTSKY1: the Cells file in the census
#include <QJsonValue>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QSettings>
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include "gl/cellunits.h"
#include "gl/celluniform.h"	// lane CELLUNI1
#include <QOpenGLContext>
#include <cstring>
#include <map>
#include <vector>
#include <unordered_map>	// lane PERFFIX1: the cell-uniform class records

quint64 wwGlPaintCount();	// glview.cpp
bool wwCellLitProbePicture();	// celllights.cpp: a probe picture (WW_CELL_LIT_PROBE) is drawn

namespace
{

constexpr int kAoUnit = WwCellUnit::Ao;			// the obscurance (sampler2D, half the view); 9 on a GPU with only 16 units
constexpr int kAoAngles = 8;		// a still view's mean over the game's per-frame angle in [0, pi)
constexpr float kAoRadius = 108.2f, kAoBias = 0.6f, kAoIntensity = 7.1f;	// the game's INI defaults, game units

struct AoState
{
	bool loaded = false;
	quint64 liveGen = 0;	// lane TESTDRIVER1: the switch generation last read
	bool on = true;			// WW_CELL_AO=0: none computed
	bool pinned = false;	// the AO row (WW/CellAo); the pin wins
	int red = 0;			// 1 off (computed, not applied), 2 radius (halved), 4 noblur, 8 noreset
	QString dump;			// WW_CELL_AO_DUMP
	int method = WwAoVanilla;	// lane SSAO2: WW_CELL_AO_METHOD (pin) or WW/CellAoMethod
	bool methodPinned = false;
	int methodRed = 0;		// WW_CELL_AO_METHOD_RED: 1 vanilla (the method runs, vanilla's term bound), 2 raw (no denoise)
	bool ssrRedOld = false;	// WW_CELL_AO_SSR_RED=old: AO off leaves the reflections no opaque pass (the old coupling)
	int quality = WwAoTierUltra;	// WW_CELL_AO_QUALITY (pin) or WW/CellAoQuality, the presets' tier
	bool qualityPinned = false;
	bool qualityRedOld = false;	// WW_CELL_AO_QUALITY_RED=old: every tier draws as before the tiers (8 angles, 9 x 3)
	bool pass = false;		// the opaque pass is drawing
	QString last = QStringLiteral( "none yet" );
};

AoState & ao()
{
	static AoState a;
	// lane TESTDRIVER1: a live set of these switches re-reads them (pins and or-ed reds back to their defaults first)
	if ( !a.loaded || wwSwitchMovedFor( a.liveGen, { "WW_CELL_AO" } ) ) {
		a.loaded = true;
		a.liveGen = wwSwitchGen();
		const QByteArray aoPin = wwEnv( "WW_CELL_AO" ).trimmed();
		a.pinned = !aoPin.isEmpty();
		a.on = a.pinned ? aoPin != "0" : QSettings().value( QStringLiteral( "WW/CellAo" ), true ).toBool();
		const QByteArray red = wwEnv( "WW_CELL_AO_RED" ).trimmed();
		a.red = red == "off" ? 1 : red == "radius" ? 2 : red == "noblur" ? 4 : red == "noreset" ? 8 : 0;
		a.dump = QString::fromLocal8Bit( wwEnv( "WW_CELL_AO_DUMP" ) );
		const QByteArray mPin = wwEnv( "WW_CELL_AO_METHOD" ).trimmed().toLower();
		const int pinned = wwAoMethodParse( mPin.constData() );
		a.methodPinned = pinned >= 0;
		a.method = pinned >= 0 ? pinned : std::max( 0, wwAoMethodParse( QSettings().value( QStringLiteral( "WW/CellAoMethod" ),
			QStringLiteral( "vanilla" ) ).toString().toLatin1().constData() ) );
		const QByteArray mRed = wwEnv( "WW_CELL_AO_METHOD_RED" ).trimmed();
		a.methodRed = mRed == "vanilla" ? 1 : mRed == "raw" ? 2 : 0;
		a.ssrRedOld = wwEnv( "WW_CELL_AO_SSR_RED" ).trimmed() == "old";
		const QByteArray qPin = wwEnv( "WW_CELL_AO_QUALITY" ).trimmed().toLower();
		const int qp = wwAoTierParse( qPin.constData() );
		a.qualityPinned = qp >= 0;
		a.quality = qp >= 0 ? qp : std::clamp( QSettings().value( QStringLiteral( "WW/CellAoQuality" ), int( WwAoTierUltra ) ).toInt(),
			0, WwAoTierCount - 1 );
		a.qualityRedOld = wwEnv( "WW_CELL_AO_QUALITY_RED" ).trimmed() == "old";
	}
	return a;
}

struct AoTarget
{
	GLuint tex = 0, fbo = 0;
	int w = 0, h = 0;
};

struct AoGpu
{
	AoTarget gbuf;			// full size: view normal + linear depth (RGBA32F), with a depth-stencil buffer
	GLuint gbufDepth = 0;
	AoTarget mip[5];		// 1..4: the min-of-2x2 depth (R32F)
	AoTarget raw, across, fin;	// half size: (A, key) RG32F, then the blurs; fin R32F, bilinear
	AoTarget xz[5], xa, xb, xe;	// lane SSAO2, full size: the working depth mips (R32F), the term ping-pong (R8), XeGTAO's (term, edges)
	GLuint aoTex = 0;		// lane SSAO2: the term the cell programs read this frame (fin, xa or xb)
	const void * doc = nullptr;
	bool ready = false;		// the opaque pass + depth pyramid of this frame (the shared g-buffer: SSR, the AO decal)
	bool aoReady = false;	// and the obscurance computed from it (false while the AO row is off)
	int unit = -1;
	GLuint decal = 0;		// this frame's AO decal target (0: none)
	int decalUnit = -1;		// WwCellUnit::AoDecal, or -1 on a GPU without it (lane VOLAIR1: was 20, under the far slots)
	AoTarget giHalf;		// lane PERF1: the screen GI (RGBA32F), its key on the same FBO's attachment 1
	GLuint giKey = 0;
	int giKeyW = 0, giKeyH = 0;
	int giStep = 2;			// 2 half resolution, 1 full
	bool giReady = false;	// this frame's screen GI
	bool volOn = false;		// this frame's volumes' mask was drawn
	AoTarget volMask;		// lane PERF1: full size, the pixel's interior volume's room + 1 (R32F; 0 none)
	GLuint volBuf = 0, volTex = 0, volVao = 0;	// the shapes, 3 RGBA32F texels each (a buffer texture)
	std::vector<float> volData;	// what volBuf holds
	bool giUnits = false;	// the GPU has WwCellUnit::GiHalfKey
};

QHash<const void *, AoGpu> & aoGpus()
{
	static QHash<const void *, AoGpu> g;
	return g;
}

void aoAlloc( NifSkopeOpenGLContext::GLFunctions * fn, AoTarget & t, int w, int h, GLenum ifmt, GLenum fmt, bool linear )
{
	if ( t.tex && t.w == w && t.h == h )
		return;
	if ( !t.tex ) {
		fn->glGenTextures( 1, &t.tex );
		fn->glGenFramebuffers( 1, &t.fbo );
	}
	fn->glBindTexture( GL_TEXTURE_2D, t.tex );
	fn->glTexImage2D( GL_TEXTURE_2D, 0, GLint( ifmt ), w, h, 0, fmt, GL_FLOAT, nullptr );
	fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, linear ? GL_LINEAR : GL_NEAREST );
	fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, linear ? GL_LINEAR : GL_NEAREST );
	fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
	fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
	fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0 );
	fn->glBindTexture( GL_TEXTURE_2D, 0 );
	fn->glBindFramebuffer( GL_FRAMEBUFFER, t.fbo );
	fn->glFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, t.tex, 0 );
	t.w = w;
	t.h = h;
}

bool aoIsCellProgram( const NifSkopeOpenGLContext::Program * p )
{
	return p && ( wwIsCellProgramName( p->name ) || wwIsCellDecalProgramName( p->name ) );	// a decal reads it too
}

/* Resolve the units whether or not the pass runs: unresolved, a cell-lit draw would bind texture 0 on unit 0
 * (BaseMap) and lose its albedo. */
void aoUnits( NifSkopeOpenGLContext::GLFunctions * fn, AoGpu & g )
{
	if ( g.unit >= 0 )
		return;
	GLint units = 0;
	fn->glGetIntegerv( GL_MAX_TEXTURE_IMAGE_UNITS, &units );
	g.unit = units > kAoUnit ? kAoUnit : 9;
	g.decalUnit = units > WwCellUnit::AoDecal ? WwCellUnit::AoDecal : -1;
	g.giUnits = units > WwCellUnit::GiHalfKey;
}

//! lane PERF1: the half-res GI's telemetry line, printed when it changes
void giEcho( const QString & line )
{
	static QString last;
	if ( line == last )
		return;
	last = line;
	std::fprintf( stderr, "cell gi rate: %s\n", qPrintable( line ) );
}

/* lane PERF1: the GI volume mask (the Division deck's D4: the volumes drawn into the stencil). Each interior shape's
 * hull, grown by the bias, back faces only, depth-tested against the shared opaque pass (a hull wholly in front of the
 * surface draws nothing); cell_givol.frag tests the surface's point a bias in front against the shape and writes its
 * room + 1, blended by max. The outdoors draws nothing (0). False: no mask this frame. */
bool giVolMaskPass( Scene * scene, AoGpu & g, const WwCellGi * G, int W, int H, float p00, float p11 )
{
	Renderer * r = scene->renderer;
	auto fn = r->fn;
	const int n = G ? int( G->volShapes.size() / 12 ) : 0;
	if ( n <= 0 || !g.gbufDepth )
		return false;
	if ( !g.volBuf ) {
		fn->glGenBuffers( 1, &g.volBuf );
		fn->glGenTextures( 1, &g.volTex );
		fn->glGenVertexArrays( 1, &g.volVao );
		g.volData.clear();
	}
	if ( g.volData != G->volShapes ) {
		fn->glBindBuffer( GL_TEXTURE_BUFFER, g.volBuf );
		fn->glBufferData( GL_TEXTURE_BUFFER, GLsizeiptr( G->volShapes.size() * sizeof( float ) ), G->volShapes.data(), GL_STATIC_DRAW );
		fn->glBindBuffer( GL_TEXTURE_BUFFER, 0 );
		g.volData = G->volShapes;
	}
	aoAlloc( fn, g.volMask, W, H, GL_R32F, GL_RED, false );
	fn->glBindFramebuffer( GL_FRAMEBUFFER, g.volMask.fbo );
	fn->glFramebufferRenderbuffer( GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, g.gbufDepth );
	const GLenum one = GL_COLOR_ATTACHMENT0;
	fn->glDrawBuffers( 1, &one );
	if ( fn->glCheckFramebufferStatus( GL_FRAMEBUFFER ) != GL_FRAMEBUFFER_COMPLETE )
		return false;
	NifSkopeOpenGLContext::Program * prog = r->useProgram( "cell_givol.prog" );
	if ( !prog )
		return false;
	fn->glViewport( 0, 0, W, H );
	const GLfloat zero[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
	fn->glClearBufferfv( GL_COLOR, 0, zero );
	// world -> clip (the projection after posView = sc R world + t), and view -> world (cell_lights.glsl cellRow)
	const Transform & vt = scene->view;
	const float sc = vt.scale != 0.0f ? vt.scale : 1.0f;
	const auto & pm = r->globalUniforms->projectionMatrix;
	for ( int row = 0; row < 4; row++ ) {
		double e[4] = { 0, 0, 0, double( pm[3][row] ) };
		for ( int i = 0; i < 3; i++ ) {
			for ( int j = 0; j < 3; j++ )
				e[j] += double( pm[i][row] ) * double( vt.rotation( i, j ) ) * double( sc );
			e[3] += double( pm[i][row] ) * double( vt.translation[i] );
		}
		prog->uni4f_l( prog->uniLocation( "volClip[%d]", row ), FloatVector4( float( e[0] ), float( e[1] ), float( e[2] ), float( e[3] ) ) );
	}
	for ( int k = 0; k < 3; k++ ) {
		float w = 0.0f;
		for ( int j = 0; j < 3; j++ )
			w -= vt.rotation( j, k ) * vt.translation[j];
		prog->uni4f_l( prog->uniLocation( "volRow[%d]", k ), FloatVector4( vt.rotation( 0, k ) / sc, vt.rotation( 1, k ) / sc,
			vt.rotation( 2, k ) / sc, w / sc ) );
	}
	const float bias = 0.75f * G->roomsCell;	// the room grid's read (cell_lights.glsl cellGiVolAt)
	prog->uni1f( "volBias", bias );
	prog->uni1f( "volGrow", bias + 1.0f );
	prog->uni2f( "volProj", p00, p11 );
	fn->glUniform2i( prog->uniLocation( "volFull" ), W, H );
	GLint prevActive = 0, prevCull = GL_BACK, prevFront = GL_CCW, prevDepthFunc = GL_LESS, prevEq = GL_FUNC_ADD;
	GLint prevBlend[4] = { GL_ONE, GL_ZERO, GL_ONE, GL_ZERO };
	fn->glGetIntegerv( GL_ACTIVE_TEXTURE, &prevActive );
	fn->glGetIntegerv( GL_CULL_FACE_MODE, &prevCull );
	fn->glGetIntegerv( GL_FRONT_FACE, &prevFront );
	fn->glGetIntegerv( GL_DEPTH_FUNC, &prevDepthFunc );
	fn->glGetIntegerv( GL_BLEND_EQUATION_RGB, &prevEq );
	fn->glGetIntegerv( GL_BLEND_SRC_RGB, &prevBlend[0] );
	fn->glGetIntegerv( GL_BLEND_DST_RGB, &prevBlend[1] );
	fn->glGetIntegerv( GL_BLEND_SRC_ALPHA, &prevBlend[2] );
	fn->glGetIntegerv( GL_BLEND_DST_ALPHA, &prevBlend[3] );
	fn->glActiveTexture( GLenum( GL_TEXTURE0 + WwCellUnit::GiHalf ) );
	fn->glBindTexture( GL_TEXTURE_2D, g.gbuf.tex );
	fn->glActiveTexture( GLenum( GL_TEXTURE0 + WwCellUnit::GiHalfKey ) );
	fn->glBindTexture( GL_TEXTURE_BUFFER, g.volTex );
	fn->glTexBuffer( GL_TEXTURE_BUFFER, GL_RGBA32F, g.volBuf );
	prog->uni1i( "volGbuf", WwCellUnit::GiHalf );
	prog->uni1i( "volShapes", WwCellUnit::GiHalfKey );
	fn->glEnable( GL_DEPTH_TEST );
	fn->glDepthFunc( GL_GEQUAL );	// the hull's back face behind the surface: the surface may be inside
	fn->glDepthMask( GL_FALSE );
	fn->glDisable( GL_STENCIL_TEST );
	fn->glEnable( GL_CULL_FACE );
	fn->glFrontFace( GL_CCW );
	fn->glCullFace( GL_FRONT );
	fn->glEnable( GL_DEPTH_CLAMP );	// a back face past the far plane still covers its pixels
	fn->glEnable( GL_BLEND );
	fn->glBlendEquation( GL_MAX );
	fn->glBlendFunc( GL_ONE, GL_ONE );
	fn->glBindVertexArray( g.volVao );
	fn->glDrawArraysInstanced( GL_TRIANGLES, 0, 36, n );
	fn->glBindVertexArray( 0 );
	fn->glDisable( GL_DEPTH_CLAMP );
	fn->glBlendEquation( GLenum( prevEq ) );
	fn->glBlendFuncSeparate( GLenum( prevBlend[0] ), GLenum( prevBlend[1] ), GLenum( prevBlend[2] ), GLenum( prevBlend[3] ) );
	fn->glDisable( GL_BLEND );
	fn->glCullFace( GLenum( prevCull ) );
	fn->glFrontFace( GLenum( prevFront ) );
	fn->glDisable( GL_CULL_FACE );
	fn->glDepthFunc( GLenum( prevDepthFunc ) );
	fn->glDisable( GL_DEPTH_TEST );
	r->stopProgram();
	fn->glBindTexture( GL_TEXTURE_BUFFER, 0 );
	fn->glActiveTexture( GLenum( GL_TEXTURE0 + WwCellUnit::GiHalf ) );
	fn->glBindTexture( GL_TEXTURE_2D, 0 );
	fn->glActiveTexture( GLenum( prevActive ) );
	return true;
}

/* lane PERF1: the screen GI. The volume read (cellGiVolume) once per giStep x giStep pixels over the shared opaque
 * g-buffer, in each pixel's volume (the mask above, else the room grid): the sample and its key (world position,
 * packed normal), which the opaque cell-lit draws upsample (cell_lights.glsl cellGiSample). Runs inside wwCellAoPass,
 * after the depth pyramid; the caller restores the state. */
void giHalfPass( Scene * scene, AoGpu & g, int W, int H, float p00, float p11 )
{
	Renderer * r = scene->renderer;
	auto fn = r->fn;
	const WwCellGi * G = wwCellGiFor( scene->nifModel );
	const bool volOn = giVolMaskPass( scene, g, G, W, H, p00, p11 );
	g.volOn = volOn;
	/* PERFFIX2: the ray-traced volume's read is FULL rate unless WW_CELL_GI_RATE=half (4): full is the inline picture
	 * (tests/ddgiscreen: rel err <= 1e-3), half bilinears across texels with other normals (p99 0.11 there) */
	const int rate = wwCellGiRate();
	g.giStep = rate == 1 || ( rate == 0 && !wwCellPrtOn() ) ? 1 : 2;
	const int hw = ( W + g.giStep - 1 ) / g.giStep, hh = ( H + g.giStep - 1 ) / g.giStep;
	aoAlloc( fn, g.giHalf, hw, hh, GL_RGBA32F, GL_RGBA, false );
	if ( !g.giKey )
		fn->glGenTextures( 1, &g.giKey );
	if ( g.giKeyW != hw || g.giKeyH != hh ) {
		fn->glBindTexture( GL_TEXTURE_2D, g.giKey );
		fn->glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA32F, hw, hh, 0, GL_RGBA, GL_FLOAT, nullptr );
		fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
		fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
		fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
		fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
		fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0 );
		fn->glBindTexture( GL_TEXTURE_2D, 0 );
		g.giKeyW = hw;
		g.giKeyH = hh;
	}
	fn->glBindFramebuffer( GL_FRAMEBUFFER, g.giHalf.fbo );
	fn->glFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, g.giKey, 0 );
	const GLenum bufs[2] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
	fn->glDrawBuffers( 2, bufs );
	if ( fn->glCheckFramebufferStatus( GL_FRAMEBUFFER ) != GL_FRAMEBUFFER_COMPLETE ) {
		giEcho( QStringLiteral( "refused(target incomplete): the full chain" ) );
		return;
	}
	NifSkopeOpenGLContext::Program * prog = r->useProgram( "cell_gihalf.prog" );
	if ( !prog ) {
		giEcho( QStringLiteral( "refused(cell_gihalf.prog did not link): the full chain" ) );
		return;
	}
	wwCellLightsUniforms( scene );	// the surfaces' GI uniforms and grids, on this program
	prog->uni1b( "cellGiHalfOn", false );
	GLint prevActive = 0;
	fn->glGetIntegerv( GL_ACTIVE_TEXTURE, &prevActive );
	fn->glActiveTexture( GLenum( GL_TEXTURE0 + WwCellUnit::GiHalf ) );
	fn->glBindTexture( GL_TEXTURE_2D, g.gbuf.tex );
	prog->uni1i( "giGbuf", WwCellUnit::GiHalf );
	prog->uni1i( "cellGiHalf", WwCellUnit::GiHalf );	// the same sampler type on the unit (never read here)
	prog->uni1i( "cellGiHalfKey", WwCellUnit::GiHalf );
	fn->glActiveTexture( GLenum( GL_TEXTURE0 + WwCellUnit::GiHalfKey ) );
	fn->glBindTexture( GL_TEXTURE_2D, volOn ? g.volMask.tex : 0 );
	prog->uni1i( "giVolMask", WwCellUnit::GiHalfKey );
	prog->uni1b( "giVolOn", volOn );
	prog->uni1i( "giStep", g.giStep );
	fn->glUniform2i( prog->uniLocation( "giFull" ), W, H );
	prog->uni2f( "giProj", p00, p11 );
	fn->glViewport( 0, 0, hw, hh );
	fn->glDisable( GL_DEPTH_TEST );
	fn->glDisable( GL_STENCIL_TEST );
	fn->glDisable( GL_BLEND );
	fn->glDisable( GL_CULL_FACE );
	static const float quad[12] = { -1, -1, 0, 1, -1, 0, -1, 1, 0, 1, 1, 0 };
	static const std::uint16_t idx[6] = { 0, 1, 2, 2, 1, 3 };
	const float * attrs = quad;
	r->drawShape( 4, 3, 6, GL_TRIANGLES, GL_UNSIGNED_SHORT, &attrs, idx );
	r->stopProgram();
	fn->glBindTexture( GL_TEXTURE_2D, 0 );
	fn->glActiveTexture( GLenum( GL_TEXTURE0 + WwCellUnit::GiHalf ) );
	fn->glBindTexture( GL_TEXTURE_2D, 0 );
	fn->glActiveTexture( GLenum( prevActive ) );
	const GLenum one = GL_COLOR_ATTACHMENT0;
	fn->glDrawBuffers( 1, &one );
	g.giReady = true;
	giEcho( QStringLiteral( "%1 %2x%3 screen %4x%5; volumes: mask %6 (%7 shapes)%8" ).arg( wwCellGiRate() == 3
		? QStringLiteral( "red (the screen pass runs, the surfaces read inline)" ) : g.giStep == 1 ? QStringLiteral( "full" )
		: QStringLiteral( "half" ) ).arg( W ).arg( H ).arg( hw ).arg( hh ).arg( volOn ? QStringLiteral( "on" ) : QStringLiteral( "off" ) )
		.arg( G ? qint64( G->volShapes.size() / 12 ) : 0 )
		.arg( wwCellPrtOn() ? QString() : QStringLiteral( "; the ray-traced volume's read (PERFFIX2)" ) ) );
}

//! the telemetry line, printed when it changes
void aoEcho( const QString & line )
{
	static QString last;
	if ( line == last )
		return;
	last = line;
	std::fprintf( stderr, "cell ao: %s\n", qPrintable( line ) );
}

// lane SSAO2: FO4 Community Shaders' SSAO defaults (radius 1.2 x 32 units, falloff, intensity, power, quality, denoise)
constexpr float kAoxRadius = 38.4f, kAoxFalloff = 0.615f, kAoxIntensity = 1.0f, kAoxPower = 1.5f;
constexpr int kAoxSlices = 9, kAoxSteps = 3;
constexpr float kAoxDenoiseRadius = 2.0f, kAoxDenoiseSigma = 0.08f, kAoxBlurBeta = 1.2f;

//! lane SSAO2: a ported method's full-size passes (res/shaders/cell_aox.frag) over the opaque pass; sets g.aoTex
const char * aoxPasses( Renderer * r, AoGpu & g, int method, int red, int W, int H, const int ms[5][2], float p00, float p11 )
{
	auto fn = r->fn;
	NifSkopeOpenGLContext::Program * prog = r->useProgram( "cell_aox.prog" );
	if ( !prog ) {
		if ( red != 1 )
			g.aoTex = 0;
		return "refused(no cell_aox.prog)";
	}
	const AoTarget * bound[6] = { &g.gbuf, &g.xz[0], &g.xz[1], &g.xz[2], &g.xz[3], &g.xz[4] };
	for ( int u = 0; u < 6; u++ ) {
		fn->glActiveTexture( GLenum( GL_TEXTURE0 + u ) );
		fn->glBindTexture( GL_TEXTURE_2D, bound[u]->tex );
	}
	prog->uni1i( "gbuf", 0 );
	prog->uni1i( "xz0", 1 );
	prog->uni1i( "xz1", 2 );
	prog->uni1i( "xz2", 3 );
	prog->uni1i( "xz3", 4 );
	prog->uni1i( "xz4", 5 );
	prog->uni1i( "src", 6 );
	fn->glUniform2i( prog->uniLocation( "fullSize" ), W, H );
	for ( int m = 0; m < 5; m++ )
		fn->glUniform2i( prog->uniLocation( "mipSize[%d]", m ), ms[m][0], ms[m][1] );
	// view space x right, y up, z forward; uv top row first (FO4CS's NDCToViewMul / NDCToViewAdd)
	prog->uni4f( "ndcToView", FloatVector4( 2.0f / p00, -2.0f / p11, -1.0f / p00, 1.0f / p11 ) );
	prog->uni4f( "aoxParams", FloatVector4( kAoxRadius, kAoxFalloff, kAoxIntensity, kAoxPower ) );
	int slices = kAoxSlices, steps = kAoxSteps;
	wwAoTierCounts( ao().quality, ao().qualityRedOld, slices, steps );	// lane PERFFIX2 (item 9): the tier's taps
	prog->uni2f( "aoxCounts", float( slices ), float( steps ) );
	prog->uni3f( "aoxDenoise", kAoxDenoiseRadius, kAoxDenoiseSigma, kAoxBlurBeta );
	static const float quad[12] = { -1, -1, 0, 1, -1, 0, -1, 1, 0, 1, 1, 0 };
	static const std::uint16_t idx[6] = { 0, 1, 2, 2, 1, 3 };
	const float * attrs = quad;
	auto pass = [&]( int stage, const AoTarget & out, const AoTarget * in ) {
		fn->glActiveTexture( GL_TEXTURE6 );
		fn->glBindTexture( GL_TEXTURE_2D, in ? in->tex : 0 );
		prog->uni1i( "aoxStage", stage );
		fn->glBindFramebuffer( GL_FRAMEBUFFER, out.fbo );
		fn->glViewport( 0, 0, out.w, out.h );
		r->drawShape( 4, 3, 6, GL_TRIANGLES, GL_UNSIGNED_SHORT, &attrs, idx );
	};
	pass( 1, g.xz[0], nullptr );
	for ( int m = 1; m < 5; m++ ) {
		prog->uni1i( "mipLevel", m );
		pass( 2, g.xz[m], &g.xz[m - 1] );
	}
	const int * st = wwAoMethodStages( method );
	const AoTarget * in = nullptr;
	for ( int i = 0; st[i]; i++ ) {
		if ( red == 2 && st[i] == 5 )
			break;	// WW_CELL_AO_METHOD_RED=raw: the term before the bilateral denoise
		const AoTarget & out = st[i] == 6 ? g.xe : in == &g.xa ? g.xb : g.xa;
		pass( st[i], out, in );
		in = &out;
	}
	r->stopProgram();
	fn->glActiveTexture( GL_TEXTURE6 );
	fn->glBindTexture( GL_TEXTURE_2D, 0 );
	if ( red != 1 && in )
		g.aoTex = in->tex;
	return nullptr;
}

}	// namespace

bool wwCellAoOn()
{
	return ao().on;
}

void wwCellAoSetOn( bool on )
{
	AoState & a = ao();
	if ( a.pinned )
		return;
	a.on = on;
	QSettings().setValue( QStringLiteral( "WW/CellAo" ), on );
}

bool wwCellAoPinned()
{
	return ao().pinned;
}

int wwCellAoMethod()
{
	return ao().method;
}

void wwCellAoSetMethod( int method )
{
	AoState & a = ao();
	if ( a.methodPinned || method < 0 || method >= WwAoMethodCount )
		return;
	a.method = method;
	QSettings().setValue( QStringLiteral( "WW/CellAoMethod" ), QString::fromLatin1( wwAoMethodName( method ) ) );
}

bool wwCellAoMethodPinned()
{
	return ao().methodPinned;
}

int wwCellAoQuality()
{
	return ao().quality;
}

void wwCellAoSetQuality( int tier )
{
	AoState & a = ao();
	if ( a.qualityPinned || tier < 0 || tier >= WwAoTierCount )
		return;
	a.quality = tier;
	QSettings().setValue( QStringLiteral( "WW/CellAoQuality" ), tier );
}

bool wwCellAoQualityPinned()
{
	return ao().qualityPinned;
}

namespace {
// lane PERFHUNT2: the obscurance walk's sorted list for the SSR water rays (raw pointers: read only within the paint)
struct AoSortedList
{
	Scene * scene = nullptr;
	QVector<Node *> nodes;
};
AoSortedList & aoSortedList()
{
	static AoSortedList l;
	return l;
}
}

bool wwCellSortedMemo()
{
	WW_SWITCH_STATIC( bool, on, wwSwitchEcho( "sorted memo", wwEnvInt( "WW_CELL_SORTMEMO_RED" ) != 1 ) );
	return on;
}

bool wwCellAoSortedTake( Scene * scene, NodeList & out )
{
	AoSortedList & l = aoSortedList();
	if ( !scene || l.scene != scene )
		return false;
	for ( Node * n : std::as_const( l.nodes ) )
		out.add( n );
	l.scene = nullptr;
	l.nodes.clear();
	return true;
}

void wwCellAoPass( Scene * scene, bool run )
{
	aoSortedList().scene = nullptr;	// lane PERFHUNT2: a list from an earlier paint is never taken
	aoSortedList().nodes.clear();
	if ( !scene || !scene->renderer )
		return;
	AoState & a = ao();
	Renderer * r = scene->renderer;
	AoGpu & g = aoGpus()[r];
	g.ready = false;
	g.aoReady = false;
	g.giReady = false;
	g.aoTex = 0;
	if ( !run || !wwCellLightsWanted( scene ) )
		return;
	auto fn = r->fn;
	aoUnits( fn, g );
	g.decal = 0;
	/* The opaque pass below is the frame's SHARED g-buffer (SSR and the AO decal read it); the AO row switches
	 * off only the obscurance computed from it, never the pass. */
	const bool forSsr = wwCellSsrWantsTargets( scene );
	const bool forDecal = g.decalUnit >= 0 && wwCellAoDecalWanted( scene->nifModel );
	const bool forContact = wwContactShadowWantsTargets( scene );	// lane TERRSHADOW1: contact shadows march its depth
	const bool forDecalAngle = wwCellDecalMappedHas( scene->nifModel );	// lane DECALANGLE2: placed decals read its normal
	// lane PERF1: the half-res GI reads it too (the GI row on, no probe pass, a GPU with the units)
	const int giRate = wwCellGiRate();
	/* PERFFIX2 (PERFRES1101 A gap 2): without the PRT (PRTOFF1) the pass runs for the ray-traced volume's read instead,
	 * once a screen texel (cell_gihalf.frag), when a volume is up; the stored GI row means nothing there */
	const bool forGi = giRate != 2 && wwCellGiVis() == 0 && g.giUnits
		&& ( wwCellPrtOn() ? wwCellGiOn() : rtddgiCellReadUp( scene ) ) && wwCellPassFor( scene->nifModel ) == 0;
	if ( wwCellGiVis() == 1 )
		giEcho( QStringLiteral( "march (red WW_CELL_GI_VIS or a march-only WW_CELL_PV_RED: the per-pixel chain every call, no screen pass)" ) );
	else if ( giRate == 2 )
		giEcho( QStringLiteral( "inline (no screen pass: every surface reads the volumes)" ) );
	else if ( !g.giUnits )
		giEcho( QStringLiteral( "full (the GPU has no unit %1)" ).arg( WwCellUnit::GiHalfKey ) );
	if ( !a.on ) {
		aoEcho( ( forSsr || forDecal || forContact || forGi || forDecalAngle ) ? QStringLiteral( "off (row); the shared opaque pass kept for%1%2%3%4%5" )
			.arg( forSsr ? QStringLiteral( " ssr" ) : QString() ).arg( forDecal ? QStringLiteral( " aodecal" ) : QString() )
			.arg( forContact ? QStringLiteral( " contact" ) : QString() ).arg( forGi ? QStringLiteral( " gihalf" ) : QString() )
			.arg( forDecalAngle ? QStringLiteral( " decalangle" ) : QString() )
			: QStringLiteral( "off (row); no opaque pass (nothing reads it)" ) );
		// lane SSAO2: the keep rule lives in cellaomethod.h (tests/aomethod); its red drops SSR's claim
		// lane RESTIRDI1: the RT quality preset's ReSTIR pass reads it too (rtddgiRestirPass)
		if ( !wwAoSharedPassRuns( false, forSsr, forDecal || forContact || forGi || forDecalAngle || rtddgiRestirShaderDefine(), a.ssrRedOld ) ) {
			a.last = QStringLiteral( "off" );
			return;
		}
	} else {
		aoEcho( ( ( a.red & 1 ) ? QStringLiteral( "computed, not applied (RED=off)" ) : QStringLiteral( "on" ) )
			+ ( a.method != WwAoVanilla ? QStringLiteral( " method %1" ).arg( QString::fromLatin1( wwAoMethodName( a.method ) ) ) : QString() ) );
	}
	QElapsedTimer timer;
	timer.start();
	GLint prevFbo = 0, prevRead = 0, vp[4] = { 0, 0, 1, 1 }, prevActive = 0;
	GLboolean depthMask = GL_TRUE, colorMask[4] = { GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE };
	fn->glGetIntegerv( GL_DRAW_FRAMEBUFFER_BINDING, &prevFbo );
	fn->glGetIntegerv( GL_READ_FRAMEBUFFER_BINDING, &prevRead );
	fn->glGetIntegerv( GL_VIEWPORT, vp );
	if ( !wwCellUni6Red( "hdrbegin" ) )	// lane CELLUNI6: the HDR frame's Begin takes them (every pass after puts them back)
		WwCellUni::frameTarget().put( wwGlPaintCount(), prevFbo, prevRead, vp );
	fn->glGetIntegerv( GL_ACTIVE_TEXTURE, &prevActive );
	fn->glGetBooleanv( GL_DEPTH_WRITEMASK, &depthMask );
	fn->glGetBooleanv( GL_COLOR_WRITEMASK, colorMask );
	const bool wasDepth = fn->glIsEnabled( GL_DEPTH_TEST ), wasCull = fn->glIsEnabled( GL_CULL_FACE );
	const bool wasBlend = fn->glIsEnabled( GL_BLEND ), wasScissor = fn->glIsEnabled( GL_SCISSOR_TEST );
	const bool wasStencil = fn->glIsEnabled( GL_STENCIL_TEST );
	{	// lane PERFFIX3: the air pass after SSR / contact / SSGI takes this state instead of asking (celluniform.h EntryNote)
		WwCellUni::EntryNote & en = WwCellUni::entryNote();
		en.frame = wwGlPaintCount();
		en.draw = prevFbo;
		en.read = prevRead;
		std::copy( vp, vp + 4, en.vp );
		en.active = prevActive;
		en.depthMask = depthMask;
		std::copy( colorMask, colorMask + 4, en.colorMask );
		en.depth = wasDepth;
		en.cull = wasCull;
		en.blend = wasBlend;
		en.scissor = wasScissor;
		en.stencil = wasStencil;
	}
	const int W = std::max( vp[2], 1 ), H = std::max( vp[3], 1 );
	const int hw = ( W + 1 ) / 2, hh = ( H + 1 ) / 2;
	int ms[5][2] = { { W, H } };
	for ( int m = 1; m < 5; m++ ) {
		ms[m][0] = std::max( ms[m - 1][0] / 2, 1 );
		ms[m][1] = std::max( ms[m - 1][1] / 2, 1 );
	}

	// the targets
	aoAlloc( fn, g.gbuf, W, H, GL_RGBA32F, GL_RGBA, false );
	if ( !g.gbufDepth )
		fn->glGenRenderbuffers( 1, &g.gbufDepth );
	fn->glBindRenderbuffer( GL_RENDERBUFFER, g.gbufDepth );
	GLint rbw = 0, rbh = 0;
	fn->glGetRenderbufferParameteriv( GL_RENDERBUFFER, GL_RENDERBUFFER_WIDTH, &rbw );
	fn->glGetRenderbufferParameteriv( GL_RENDERBUFFER, GL_RENDERBUFFER_HEIGHT, &rbh );
	if ( rbw != W || rbh != H )
		fn->glRenderbufferStorage( GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, W, H );
	fn->glBindRenderbuffer( GL_RENDERBUFFER, 0 );
	fn->glBindFramebuffer( GL_FRAMEBUFFER, g.gbuf.fbo );
	fn->glFramebufferRenderbuffer( GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, g.gbufDepth );
	for ( int m = 1; m < 5; m++ )
		aoAlloc( fn, g.mip[m], ms[m][0], ms[m][1], GL_R32F, GL_RED, false );
	aoAlloc( fn, g.raw, hw, hh, GL_RG32F, GL_RG, false );
	aoAlloc( fn, g.across, hw, hh, GL_RG32F, GL_RG, false );
	aoAlloc( fn, g.fin, hw, hh, GL_R32F, GL_RED, true );
	const int method = a.on ? a.method : WwAoVanilla;	// lane SSAO2
	if ( method != WwAoVanilla ) {
		for ( int m = 0; m < 5; m++ )
			aoAlloc( fn, g.xz[m], ms[m][0], ms[m][1], GL_R32F, GL_RED, false );
		aoAlloc( fn, g.xa, W, H, GL_R8, GL_RED, false );
		aoAlloc( fn, g.xb, W, H, GL_R8, GL_RED, false );
		if ( method == WwAoXeGtao )
			aoAlloc( fn, g.xe, W, H, GL_RG32F, GL_RG, false );
	}

	// 1. the opaque cell-lit fragments: the view normal and the linear depth (probe 20); the background far away
	fn->glBindFramebuffer( GL_FRAMEBUFFER, g.gbuf.fbo );
	const bool complete = fn->glCheckFramebufferStatus( GL_FRAMEBUFFER ) == GL_FRAMEBUFFER_COMPLETE;
	NifSkopeOpenGLContext::Program * prog = complete ? r->useProgram( "cell_ao.prog" ) : nullptr;
	if ( prog )
		r->stopProgram();
	if ( !prog ) {
		fn->glBindFramebuffer( GL_DRAW_FRAMEBUFFER, GLuint( prevFbo ) );
		fn->glBindFramebuffer( GL_READ_FRAMEBUFFER, GLuint( prevRead ) );
		a.last = complete ? QStringLiteral( "refused(no cell_ao.prog)" ) : QStringLiteral( "refused(target incomplete)" );
		return;
	}
	fn->glViewport( 0, 0, W, H );
	fn->glDisable( GL_SCISSOR_TEST );
	fn->glColorMask( GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE );
	fn->glDepthMask( GL_TRUE );
	const GLfloat far4[4] = { 0.0f, 0.0f, 0.0f, 1.0e6f };
	fn->glClearBufferfv( GL_COLOR, 0, far4 );
	if ( wwCellSsrPrevStencil() )
		fn->glStencilMask( 0xFF );	// lane PERF3: the clear zeroes the flag bit too
	fn->glClear( GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT );
	fn->glDisable( GL_BLEND );
	fn->glColorMask( GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE );	// wwCellAoDraw opens it for the opaque cell-lit draws
	a.pass = true;
	PassTime::subBegin( PassTime::AoGeometry );	// lane PERF2: the obscurance pass's three parts
	{
		/* lane PERF2: this pass is CPU-bound per draw. memo: the scene-global setters once per program (as the main
		 * pass); opaque: the sorted draws that write nothing here are skipped (PERF2FIX1). =0 or *_RED=1: old */
		WW_SWITCH_STATIC( bool, memo, qgetenv( "WW_CELL_AO_MEMO" ) != "0" && !qEnvironmentVariableIntValue( "WW_CELL_AO_MEMO_RED" ) );
		WW_SWITCH_STATIC( bool, opaqueOnly, qgetenv( "WW_CELL_AO_OPAQUE" ) != "0" && !qEnvironmentVariableIntValue( "WW_CELL_AO_OPAQUE_RED" ) );
		NodeList second;
		wwOpaqueCutNewFrame();
		if ( memo )
			wwSetupMemoArm( true, "cellProbe" );	// wwCellAoDraw sets it (20) after every setup
		scene->collectShapes( second );
		if ( memo )
			wwSetupMemoArm( false );
		aoSortedList().scene = scene;	// lane PERFHUNT2: before the sort below reorders it
		aoSortedList().nodes = second.list();
		// lane PERF2FIX1: the sorted draws keep their order; the cut skips only those that wrote nothing here last frame
		if ( opaqueOnly )
			wwOpaqueCutArm( true, "ao" );
		const bool sortedMemo = memo && wwCellSortedMemo();
		if ( sortedMemo )
			wwSetupMemoArm( true, "cellProbe" );
		Scene::drawDeferredShapes( second );
		scene->drawShapeEffects();
		if ( sortedMemo )
			wwSetupMemoArm( false );
		if ( opaqueOnly )
			wwOpaqueCutArm( false, "ao" );
	}
	PassTime::subEnd( PassTime::AoGeometry );
	a.pass = false;
	fn->glColorMask( GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE );
	fn->glDepthMask( GL_TRUE );
	fn->glDisable( GL_STENCIL_TEST );
	fn->glDisable( GL_POLYGON_OFFSET_FILL );
	if ( wwCellSsrPrevStencil() ) {	// lane PERF3: the flag write's stencil state back to the defaults
		fn->glStencilOp( GL_KEEP, GL_KEEP, GL_KEEP );
		fn->glStencilMask( 0xFF );
		fn->glStencilFunc( GL_ALWAYS, 0, 0xFF );
	}
	// the AO decal (src/gl/cellaodecalgl.h) copies over this opaque pass, into its own target
	if ( g.decalUnit >= 0 && wwCellAoDecalHas( scene->nifModel ) )
		g.decal = GLuint( wwCellAoDecalRun( scene, g.gbuf.tex, g.gbufDepth, W, H ) );

	// 2. the full-screen passes (cell_ao.frag)
	PassTime::subBegin( PassTime::AoStages );
	prog = r->useProgram( "cell_ao.prog" );
	fn->glDisable( GL_DEPTH_TEST );
	fn->glDisable( GL_CULL_FACE );
	fn->glDisable( GL_BLEND );
	fn->glDepthMask( GL_FALSE );
	fn->glPolygonMode( GL_FRONT_AND_BACK, GL_FILL );
	const AoTarget * bound[6] = { &g.gbuf, &g.mip[1], &g.mip[2], &g.mip[3], &g.mip[4], nullptr };
	for ( int u = 0; u < 5; u++ ) {
		fn->glActiveTexture( GLenum( GL_TEXTURE0 + u ) );
		fn->glBindTexture( GL_TEXTURE_2D, bound[u]->tex );
	}
	prog->uni1i( "gbuf", 0 );
	prog->uni1i( "zMip1", 1 );
	prog->uni1i( "zMip2", 2 );
	prog->uni1i( "zMip3", 3 );
	prog->uni1i( "zMip4", 4 );
	prog->uni1i( "src", 5 );
	fn->glUniform2i( prog->uniLocation( "fullSize" ), W, H );
	fn->glUniform2i( prog->uniLocation( "halfSize" ), hw, hh );
	for ( int m = 0; m < 5; m++ )
		fn->glUniform2i( prog->uniLocation( "mipSize[%d]", m ), ms[m][0], ms[m][1] );
	const auto & pm = r->globalUniforms->projectionMatrix;
	const float p00 = pm[0][0], p11 = pm[1][1];
	const float radius = kAoRadius * ( ( a.red & 2 ) ? 0.5f : 1.0f );
	prog->uni2f( "proj", p00, p11 );
	prog->uni3f( "aoParams", radius, kAoBias, kAoIntensity );
	const int angles = wwAoTierAngles( a.quality, a.qualityRedOld );	// lane PERFFIX2 (item 9): the tier's angles
	prog->uni1i( "aoAngles", angles );
	prog->uni1b( "aoNoBlur", ( a.red & 4 ) != 0 );
	prog->uni1b( "aoNoReset", ( a.red & 8 ) != 0 );
	static const float quad[12] = { -1, -1, 0, 1, -1, 0, -1, 1, 0, 1, 1, 0 };
	static const std::uint16_t idx[6] = { 0, 1, 2, 2, 1, 3 };
	const float * attrs = quad;
	auto pass = [&]( int stage, const AoTarget & out, const AoTarget * in ) {
		fn->glActiveTexture( GL_TEXTURE5 );
		fn->glBindTexture( GL_TEXTURE_2D, in ? in->tex : 0 );
		prog->uni1i( "aoStage", stage );
		fn->glBindFramebuffer( GL_FRAMEBUFFER, out.fbo );
		fn->glViewport( 0, 0, out.w, out.h );
		r->drawShape( 4, 3, 6, GL_TRIANGLES, GL_UNSIGNED_SHORT, &attrs, idx );
	};
	for ( int m = 1; m < 5; m++ ) {
		prog->uni1i( "mipLevel", m );
		pass( 1, g.mip[m], m > 1 ? &g.mip[m - 1] : nullptr );
	}
	// the obscurance itself; the pyramid above is shared. A ported method skips it
	if ( a.on && ( method == WwAoVanilla || a.methodRed == 1 ) ) {
		pass( 2, g.raw, nullptr );
		pass( 3, g.across, &g.raw );
		pass( 4, g.fin, &g.across );
	}
	r->stopProgram();
	g.aoTex = a.on ? g.fin.tex : 0;
	const char * aoxRefused = nullptr;
	if ( method != WwAoVanilla )
		aoxRefused = aoxPasses( r, g, method, a.methodRed, W, H, ms, p00, p11 );
	for ( int u = 0; u < 6; u++ ) {
		fn->glActiveTexture( GLenum( GL_TEXTURE0 + u ) );
		fn->glBindTexture( GL_TEXTURE_2D, 0 );
	}
	fn->glActiveTexture( GLenum( prevActive ) );
	PassTime::subEnd( PassTime::AoStages );
	PassTime::subBegin( PassTime::AoGi );
	if ( forGi )	// lane PERF1
		giHalfPass( scene, g, W, H, p00, p11 );
	PassTime::subEnd( PassTime::AoGi );

	g.ready = true;
	g.aoReady = a.on && g.aoTex;
	g.doc = scene->nifModel;
	int aoxSl = 0, aoxSt = 0;
	wwAoTierCounts( a.quality, a.qualityRedOld, aoxSl, aoxSt );
	if ( !a.on ) {
		a.last = QStringLiteral( "off (opaque pass %1x%2 kept, ms %3)" ).arg( W ).arg( H ).arg( timer.elapsed() );
	} else if ( method != WwAoVanilla ) {
		a.last = QStringLiteral( "on method %1 full %2x%3 radius %4 falloff %5 intensity %6 power %7 slices %8 steps %9 "
			"bound %10 red %11 ms %12 quality %13" ).arg( QString::fromLatin1( wwAoMethodName( method ) ) ).arg( W ).arg( H )
			.arg( double( kAoxRadius ), 0, 'f', 2 ).arg( double( kAoxFalloff ), 0, 'f', 3 ).arg( double( kAoxIntensity ), 0, 'f', 2 )
			.arg( double( kAoxPower ), 0, 'f', 2 ).arg( aoxSl ).arg( aoxSt )
			.arg( aoxRefused ? QString::fromLatin1( aoxRefused ) : g.aoTex == g.fin.tex ? QStringLiteral( "vanilla(red)" )
				: QStringLiteral( "aox" ) ).arg( a.methodRed ).arg( timer.elapsed() ).arg( a.quality );
	} else {
		a.last = QStringLiteral( "on %1x%2 half %3x%4 radius %5 bias %6 intensity %7 angles %8 p00 %9 p11 %10 p20 %11 p21 %12 "
			"unit %13 red %14 ms %15 quality %16" ).arg( W ).arg( H ).arg( hw ).arg( hh ).arg( double( radius ), 0, 'f', 2 )
			.arg( double( kAoBias ), 0, 'f', 2 ).arg( double( kAoIntensity ), 0, 'f', 2 ).arg( angles )
			.arg( double( p00 ), 0, 'f', 6 ).arg( double( p11 ), 0, 'f', 6 ).arg( double( pm[2][0] ), 0, 'f', 6 )
			.arg( double( pm[2][1] ), 0, 'f', 6 ).arg( g.unit ).arg( a.red ).arg( timer.elapsed() ).arg( a.quality );
	}

	// WW_CELL_AO_DUMP=<file>: int32 W H hw hh, the opaque pass (W x H RGBA float, bottom row first), the raw
	// (A, key) and the final obscurance (hw x hh, top row first); <file>.txt the numbers
	// lane SSAO2: a ported method writes <file>.aox instead: int32 W H, the bound term (W x H float, top row first)
	if ( !a.dump.isEmpty() && g.aoReady && method != WwAoVanilla && g.aoTex != g.fin.tex ) {
		std::vector<std::uint8_t> t8( size_t( W ) * size_t( H ) );
		std::vector<float> tf( t8.size() );
		fn->glPixelStorei( GL_PACK_ALIGNMENT, 1 );
		fn->glBindFramebuffer( GL_READ_FRAMEBUFFER, g.aoTex == g.xa.tex ? g.xa.fbo : g.xb.fbo );
		fn->glReadPixels( 0, 0, W, H, GL_RED, GL_UNSIGNED_BYTE, t8.data() );
		fn->glPixelStorei( GL_PACK_ALIGNMENT, 4 );
		double mean = 0.0;
		for ( size_t i = 0; i < t8.size(); i++ ) {
			tf[i] = float( t8[i] ) / 255.0f;
			mean += double( tf[i] );
		}
		mean /= double( std::max<size_t>( tf.size(), 1 ) );
		a.last += QStringLiteral( " mean %1" ).arg( mean, 0, 'f', 4 );
		QFile f( a.dump + QStringLiteral( ".aox" ) );
		if ( f.open( QIODevice::WriteOnly ) ) {
			const qint32 hd[2] = { W, H };
			f.write( reinterpret_cast<const char *>( hd ), sizeof( hd ) );
			f.write( reinterpret_cast<const char *>( tf.data() ), qint64( tf.size() * sizeof( float ) ) );
		}
		QFile t( a.dump + QStringLiteral( ".txt" ) );
		if ( t.open( QIODevice::WriteOnly | QIODevice::Text ) )
			t.write( ( QStringLiteral( "ao: " ) + a.last + QStringLiteral( "\n" ) ).toUtf8() );
	} else if ( !a.dump.isEmpty() && g.aoReady ) {
		std::vector<float> gb( size_t( W ) * size_t( H ) * 4 ), raw( size_t( hw ) * size_t( hh ) * 2 ),
			fin( size_t( hw ) * size_t( hh ) );
		fn->glPixelStorei( GL_PACK_ALIGNMENT, 4 );
		fn->glBindFramebuffer( GL_READ_FRAMEBUFFER, g.gbuf.fbo );
		fn->glReadPixels( 0, 0, W, H, GL_RGBA, GL_FLOAT, gb.data() );
		fn->glBindFramebuffer( GL_READ_FRAMEBUFFER, g.raw.fbo );
		fn->glReadPixels( 0, 0, hw, hh, GL_RG, GL_FLOAT, raw.data() );
		fn->glBindFramebuffer( GL_READ_FRAMEBUFFER, g.fin.fbo );
		fn->glReadPixels( 0, 0, hw, hh, GL_RED, GL_FLOAT, fin.data() );
		double mean = 0.0;
		for ( float v : fin )
			mean += double( v );
		mean /= double( std::max<size_t>( fin.size(), 1 ) );
		a.last += QStringLiteral( " mean %1" ).arg( mean, 0, 'f', 4 );
		QFile f( a.dump );
		if ( f.open( QIODevice::WriteOnly ) ) {
			const qint32 hd[4] = { W, H, hw, hh };
			f.write( reinterpret_cast<const char *>( hd ), sizeof( hd ) );
			f.write( reinterpret_cast<const char *>( gb.data() ), qint64( gb.size() * sizeof( float ) ) );
			f.write( reinterpret_cast<const char *>( raw.data() ), qint64( raw.size() * sizeof( float ) ) );
			f.write( reinterpret_cast<const char *>( fin.data() ), qint64( fin.size() * sizeof( float ) ) );
		}
		QFile t( a.dump + QStringLiteral( ".txt" ) );
		if ( t.open( QIODevice::WriteOnly | QIODevice::Text ) )
			t.write( ( QStringLiteral( "ao: " ) + a.last + QStringLiteral( "\n" ) ).toUtf8() );
	}

	fn->glBindFramebuffer( GL_DRAW_FRAMEBUFFER, GLuint( prevFbo ) );
	fn->glBindFramebuffer( GL_READ_FRAMEBUFFER, GLuint( prevRead ) );
	fn->glViewport( vp[0], vp[1], vp[2], vp[3] );
	fn->glColorMask( colorMask[0], colorMask[1], colorMask[2], colorMask[3] );
	fn->glDepthMask( depthMask );
	if ( wasDepth ) fn->glEnable( GL_DEPTH_TEST ); else fn->glDisable( GL_DEPTH_TEST );
	if ( wasCull ) fn->glEnable( GL_CULL_FACE ); else fn->glDisable( GL_CULL_FACE );
	if ( wasBlend ) fn->glEnable( GL_BLEND ); else fn->glDisable( GL_BLEND );
	if ( wasScissor ) fn->glEnable( GL_SCISSOR_TEST ); else fn->glDisable( GL_SCISSOR_TEST );
	if ( wasStencil ) fn->glEnable( GL_STENCIL_TEST ); else fn->glDisable( GL_STENCIL_TEST );
}

// this frame's opaque pass and depth pyramid, for the reflections' march (src/gl/cellssr.h)
bool wwCellAoTargets( Scene * scene, WwCellAoTargets & out )
{
	if ( !scene || !scene->renderer || !aoGpus().contains( scene->renderer ) )
		return false;
	const AoGpu & g = aoGpus()[scene->renderer];
	if ( !g.ready || g.doc != scene->nifModel )
		return false;
	out.gbuf = g.gbuf.tex;
	out.depthRb = g.gbufDepth;
	out.w = g.gbuf.w;
	out.h = g.gbuf.h;
	for ( int m = 1; m < 5; m++ ) {
		out.mip[m] = g.mip[m].tex;
		out.mipW[m] = g.mip[m].w;
		out.mipH[m] = g.mip[m].h;
	}
	return true;
}

void wwCellAoDraw( Scene * scene, bool cellProgram )
{
	PassTime::DrawCpuScope cpuT( PassTime::CpuAoDraw );	// lane PERF2
	if ( !scene || !scene->renderer )
		return;
	AoState & a = ao();
	Renderer * r = scene->renderer;
	NifSkopeOpenGLContext::Program * prog = r->getCurrentProgram();
	cellProgram = cellProgram && aoIsCellProgram( prog );
	if ( a.pass ) {
		// the game's opaque (deferred) pass: no blended draw, no effect, nothing that is not cell-lit
		const bool opaque = cellProgram && !wwDrawBlend();
		glColorMask( opaque, opaque, opaque, opaque );
		if ( !opaque )
			glDepthMask( GL_FALSE );
		if ( wwCellSsrPrevStencil() ) {
			// lane PERF3 (cellssr.h): stencil bit 0x01 = the nearest opaque surface's reflection flag (AODECAL1 uses 0x80)
			glEnable( GL_STENCIL_TEST );
			glStencilOp( GL_KEEP, GL_KEEP, GL_REPLACE );
			glStencilMask( opaque ? 0x01u : 0x00u );
			glStencilFunc( GL_ALWAYS, opaque && wwCellSsrFlagged() ? 1 : 0, 0xFF );
		}
		wwOpaqueCutSaw( opaque );	// lane PERF2FIX1 (the cut's check)
		if ( cellProgram ) {
			prog->uni1i( "cellProbe", 20 );
			// lane PERF1: the fragment returns once it has the normal (WW_AO_EARLY=0 shades it all, =red marks the early ones)
			WW_SWITCH_STATIC( int, early, [] { const QByteArray v = wwEnv( "WW_AO_EARLY" ).trimmed(); return v == "0" ? 0 : v == "red" ? 2 : 1; }() );
			prog->uni1i( "cellAoEarly", early );
			prog->uni1b( "cellAoOn", false );
			prog->uni1b( "cellAoDecalOn", false );
			if ( prog->uniLocation( "cellCtOn" ) >= 0 )
				prog->uni1b( "cellCtOn", false );	// lane GAPLOOK1: this pass writes the depth C8 reads
			if ( prog->uniLocation( "wwDecalGbufOn" ) >= 0 )
				prog->uni1b( "wwDecalGbufOn", false );	// lane DECALANGLE2: nor may a decal read it while it is written
		}
		return;
	}
	if ( !cellProgram || prog->uniLocation( "cellAoOn" ) < 0 )
		return;
	AoGpu & g = aoGpus()[r];
	aoUnits( r->fn, g );	// never unit 0 (BaseMap), even when the pass has not run
	// an effect decal is blended, but the game draws it into the opaque surface it lies on
	const bool decalProg = wwIsCellDecalProgramName( prog->name );
	const bool on = g.aoReady && g.doc == scene->nifModel && !( a.red & 1 ) && ( !wwDrawBlend() || decalProg )
		&& wwCellLightsWanted( scene );
	// bound whether or not this draw reads it (a sampler left on unit 0 would sit beside BaseMap)
	WwCellUni::Units<std::remove_pointer_t<decltype( r->fn )>> u( r->fn, wwCellUniDsa() );	// lane CELLUNI2: no query
	if ( !u.dsa )
		u.seed( wwDrawActiveTexture() );	// lane PERF3
	u.bind( g.unit, GL_TEXTURE_2D, on ? g.aoTex : 0 );	// lane SSAO2: the method's term (fin for vanilla)
	u.done();
	prog->uni1i( "cellAo", g.unit );
	prog->uni1b( "cellAoOn", on );
	/* The AO decal target multiplies the probe term (cellGiE). Opaque draws only, like the obscurance:
	 * a blended draw is not in the opaque pass the target was made over. */
	const bool decalOn = g.ready && g.doc == scene->nifModel && g.decal && g.decalUnit >= 0 && ( !wwDrawBlend() || decalProg );
	{
		const int du = g.decalUnit >= 0 ? g.decalUnit : g.unit;
		if ( g.decalUnit >= 0 )
			u.bind( du, GL_TEXTURE_2D, decalOn ? g.decal : 0 );
		else if ( !u.dsa )
			u.raw( du );	// the old sequence moved the unit there and back
		u.done();
		prog->uni1i( "cellAoDecal", du );
		prog->uni1b( "cellAoDecalOn", decalOn );
		if ( decalOn ) {
			int dvp[4];
			wwDrawViewport( dvp );
			prog->uni4f_l( prog->uniLocation( "cellAoDecalRect" ), FloatVector4( float( dvp[0] ), float( dvp[1] ), 0.0f, 0.0f ) );
		}
	}
	// lane GAPLOOK1 (C8): the lamps' contact march reads the shared opaque pass (opaque draws, perspective views only)
	if ( prog->uniLocation( "cellCtOn" ) >= 0 ) {
		const auto & pm = r->globalUniforms->projectionMatrix;
		const bool ctOn = wwLookOn( WwLook::ContactLocal ) && g.ready && g.doc == scene->nifModel && g.gbuf.tex
			&& ( !wwDrawBlend() || decalProg ) && pm[3][3] != 1.0f;
		// lane DECALANGLE2: a placed decal reads the drawn normal under it from the same target
		const bool dgOn = decalProg && g.ready && g.doc == scene->nifModel && g.gbuf.tex && prog->uniLocation( "wwDecalGbufOn" ) >= 0;
		u.bind( WwCellUnit::ContactGbuf, GL_TEXTURE_2D, ( ctOn || dgOn ) ? g.gbuf.tex : 0 );
		u.done();
		prog->uni1i( "cellCtGbuf", WwCellUnit::ContactGbuf );
		prog->uni1b( "cellCtOn", ctOn );
		if ( prog->uniLocation( "wwDecalGbufOn" ) >= 0 ) {
			prog->uni1b( "wwDecalGbufOn", dgOn );
			if ( dgOn ) {
				int dvp[4];
				wwDrawViewport( dvp );
				prog->uni4f( "wwDecalGbufRect", FloatVector4( float( dvp[0] ), float( dvp[1] ), float( g.gbuf.w ), float( g.gbuf.h ) ) );
			}
		}
		if ( ctOn ) {
			prog->uni4f( "cellCtProj", FloatVector4( pm[0][0], pm[1][1], pm[2][0], pm[2][1] ) );
			prog->uni4f( "cellCtRect", FloatVector4( 0.0f, 0.0f, float( g.gbuf.w ), float( g.gbuf.h ) ) );
			prog->uni1i( "cellCtRed", wwEnvStr( "WW_CELL_CONTACT_LOCAL_RED" ) == QLatin1String( "away" ) ? 1 : 0 );	// red: march away from the light
		}
	}
	/* lane PERF1: the half-res GI, read by the opaque draws (and the decals on them), like the obscurance; the
	 * upsample's keys send any pixel that is not the g-buffer's surface to the full chain */
	if ( prog->uniLocation( "cellGiHalfOn" ) >= 0 ) {
		// a probe picture (WW_CELL_LIT_PROBE) reads the volumes' mask at its own pixel instead: the screen pass's read
		const bool probePic = wwCellLitProbePicture();
		const bool giOn = g.giReady && g.doc == scene->nifModel && ( wwCellGiRate() == 0 || wwCellGiRate() == 1 || wwCellGiRate() == 4 ) && wwCellGiVis() == 0
			&& ( !wwDrawBlend() || decalProg ) && ( !probePic || g.volOn );
		if ( g.giUnits ) {
			u.bind( WwCellUnit::GiHalf, GL_TEXTURE_2D, giOn ? ( probePic ? g.volMask.tex : g.giHalf.tex ) : 0 );
			u.bind( WwCellUnit::GiHalfKey, GL_TEXTURE_2D, giOn ? g.giKey : 0 );
			u.done();
		}
		prog->uni1b( "cellGiHalfOn", giOn );
		if ( giOn ) {
			int vp[4];
			wwDrawViewport( vp );
			prog->uni4f_l( prog->uniLocation( "cellGiHalfRect" ), FloatVector4( float( vp[0] ), float( vp[1] ),
				float( g.giHalf.w ), float( g.giHalf.h ) ) );
			prog->uni1f( "cellGiHalfScale", 1.0f / float( g.giStep ) );
		}
	}
	if ( on ) {
		int vp[4];
		wwDrawViewport( vp );
		prog->uni4f_l( prog->uniLocation( "cellAoRect" ), FloatVector4( float( vp[0] ), float( vp[1] ),
			1.0f / float( std::max( vp[2], 1 ) ), 1.0f / float( std::max( vp[3], 1 ) ) ) );
	}
}

bool wwCellAoInPass()
{
	return ao().pass;
}
