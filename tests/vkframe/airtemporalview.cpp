// Lane VKAIR12 (2026-11-16): the air inject's stage 1, the |3 temporal reader (cellvolair.cpp vaPassInner, stage 1 with a
// history: 64 draws, draw k writes the inject's layer k at attachment 0 and stratum p's layer p x 64 + k of the other history
// at attachment 1 + p, MRT 1 + strata) on the present device: vkairtemporal.inc airTemporalRun draws the 64 layers in one
// crossing into the SHARED inject (RGBA32F 3D, cols x rows x 64) and the SHARED out history (RGBA32F 3D, cols x rows x
// strata x 64) with the airinject twins of cell_volair, reading every input from GL's units (shared ones in place, others
// one copy a run). GL's side is GL's own cell_volair program (airinjectview_prep.py, both pixel-stage keys) into GL-owned
// twins of both targets through glFramebufferTextureLayer, the app's state (no blend, depth, cull; the quad at location 0;
// viewport cols x rows), every loose uniform seeded by farloddraw's seedUnis (profile fogOn), every block bound zero, then the
// stage's own (stage1() below).
// Inputs, each made by the app's helper (cellvolairshare.h vaShareEnsure, the app's specs) as a shared target and filled as
// the app fills it: the history in (RGBA32F 3D, strata x 64 layers, linear clamp: a GL clear per layer, alpha 1 except the
// region x < 5, y < 4 at alpha 0.5, an invalid tap), a second history (other values: the swap red), volLampNow (RGBA32F 3D,
// 64 layers, nearest), the drift noise (R8 8^3, linear repeat, glTexSubImage3D as the app's refill), airTerr (R32F 2D array
// 33 x 17 x 2, linear clamp, glTexSubImage3D both layers), volTile (shared RG32F 2D, a clear).
// Fixture: 21 x 12 x 64. Arms: def strata 6 volTemporal 1 (reproject, Sample LINEAR), rt strata 6 volTemporal 1, def strata 4
// volTemporal 2 (Load). The strata 4 arm re-asks the helper with a new spec (the history's remake path).
// Bar (set before the first run, never widened):
//   GL writes every texel of both targets (no sentinel 7 left), all finite;
//   Vulkan vs GL, every texel of the inject and the out history: written, finite, |vk - gl| <= 1e-4 max(1, |gl|) on every
//   value (EXACT = every value equal, else BOUND);
//   the route's stat: 1 run, 64 draws, image copies 0 (every input shared);
//   read twice: the first read of both targets equals a second read (EXACT);
//   a rerun after the reds is EXACT against the first Vulkan run;
//   re-ask cycle: the noise released and made again GL-owned (want false), refilled: Vulkan EXACT against the first run with
//   exactly 1 image copy; then every input released, made shared again and refilled: EXACT with 0 copies.
// GL probes (moved = texels of both targets past the bar's clause against GL's own run; LIVE >= 50, else TESTS NOTHING and the
// gate fails): volLayer 0 on every draw, the out strata permuted (GL's own output, p -> p + 1 mod strata: the strata are
// distinct), the second history bound, volLampNow rewritten, the noise rewritten (glTexSubImage3D), the noise's MAG filter
// NEAREST (a magnified sample: the noise tile spans many froxels), airTerr rewritten.
// Reds, every arm, each must FAIL (never pass): route 1 volLayer stuck at 0; route 2 the strata attachments one late
// ((p + 1) mod strata); gate 4 Vulkan's run binds the second history; gate 8 volLampNow rewritten after GL's draw; gate 16
// the noise rewritten (glTexSubImage3D) after GL's draw; gate 128 the noise's MAG filter NEAREST after GL's draw (the run
// reads GL's sampler state); gate 32 the other key's wwDefs: must REFUSE, named "key"; gate 64 the out history's key = the
// history in's: must REFUSE, named "feedback"; gate 256 a GL-owned copy of the history bound on volHist: must REFUSE, named
// "volHist"; gate 512 (the RT GI arm only) a GL-owned RGBA32F texture buffer bound on cellDdgiAtlas, a big all-on input
// that is not shared: must REFUSE, named "cellDdgiAtlas".
// Coverage limit: the app's own s1Share branches (cellvolair.cpp) run only in the app; this gate drives the helper they call.
// Lane VKAIR17 (2026-11-16), arm "rt strata 6 temporal 1 atlas" (appended; the three arms above keep their lines byte-equal):
// the RT GI key (only WW_AIR_LEAK = WW_CELL_FOGLEAK && WW_CELL_RTDDGI samples the atlas: setupProg FAILS if the def key
// samples volLampAtlas / volLampTable or the rt key does not) with the lamps' depth atlas made by the app's helper
// (airlampatlasshare.h airAtlasEnsure, wished shared, 8192 D16, filled by a depth clear through its FBO) and the table (a GL
// texture buffer) bound, volLampVis 1, volLampN 2, volLampAtlasRed 0. Stage 1 with volLampVol 1 (the app's state when the lamp
// volume ran) takes the lamps from volLampNow and never samples the atlas: this arm proves the route RUNS with them bound (the
// atlas content read is gated by airlampview's atlas arm, stage 4; stage 1 with volLampVol 0 is not gated here). Bar: make
// check (shared, FBO complete, farSharedOf == the key); Vulkan vs GL and the stat as the other arms (0 image copies); every red
// and probe of the arms above; red gate 2048 a GL-owned atlas (the helper's red 1: the share skipped) bound: must REFUSE, named
// "volLampAtlas".
// Lane VKAIR18 (2026-11-16), arm "rt strata 6 temporal 1 atlas all-on" (appended last; the arms above stay byte-equal): the
// all-on route (main.cpp kAllOn: RTDDGI 1, RT ultra, so RESTIR on) binds cellDdgiAtlas (RGBA32F) and cellRestir (RG32F), the
// RT GI's texel buffers shared by name (vkrtbuf.h rtBufShare + rtBufOwner, as rtddgicell.cpp shares them), with the atlas.
// Bar (set before the first run, never widened): the rt key's program samples cellRestir (a unit); Vulkan vs GL and the stat
// as the atlas arm; one run asks rtBufOf once a name (rtBufStat lookups +1 each). Reds, each must FAIL: gate 16384
// WW_VKVIEW_RTBUF_RED=1 (named "WW_VKVIEW_RTBUF_RED"); gate 32768 the owner's fresh() false (its why named); gate 65536 a GL-owned
// RG32F buffer with the same bytes on cellRestir while the slot lives (named "cellRestir"); plus every red above (512 included).
// Content: stage 1 reads cellRestir only under volLeakRed & 4: the shipped uniforms gate the binding and the refusals; the
// "RT GI inputs rewritten" line is informative, not a bar. airtemporalview.sh greps both routes for the rtBufOf call.
// Lane VKAIR19 (2026-11-16), the far grid's stage 1 (census row |6, cellvolair.cpp vaPassInner's far loop, FARGRID1): appended
// after every arm above (their lines stay byte-equal). GL draws stage 1 of the far grid as the app does: volFarMode 3,
// volLayerBase 64, volTemporal 0, volHistOk 0, volTerms 9, volDepthSkip 0, volDrift (0, 2000, 1, 0.5), volClusterDims (0, 0, 8,
// 8), volLampVol 0, volDims / volSlices of the far grid (13 x 9 x 24 from 200, ln 10), nothing on volHist; one attachment, layer
// 64 + k of an inject 21 x 12 x 88, k in [k0, k1). Vulkan: airTemporalRun with `far` set (base 64, k0, k1) into the SHARED
// inject. Arms "far def" (key 0) and "far all-on" (key 1, the atlas and both RT GI buffers bound as the all-on arm), each a
// whole run (0..24) and a spread run (5..17, airfar::Share's run of layers).
// Bar (set before the first run, never widened): GL writes every texel of the run's layers inside 13 x 9, all finite, and
// leaves every other texel at the sentinel; at least half of those texels lit (rgb > 0: a black frame passes any identity);
// Vulkan vs GL on every texel: the run's ones |vk - gl| <= 1e-4 max(1, |gl|), every other one the sentinel on both sides;
// stat (takeAirFarStat) 1 run, k1 - k0 draws, 0 image copies; read twice EXACT; all-on: rtBufOf lookups 1 and 1 a run; a
// rerun after the reds EXACT against the first run.
// GL probes (LIVE >= 50): volLayer 0 on every draw, airTerr rewritten, volSlices' start doubled. volDrift.x is 0 in the far
// grid, so the noise is not read: no noise probe or red. Informative: volFarMode 0 vs 3 (airFarVis).
// Reds, each must FAIL: route 1 volLayer stuck at 0; route 4 the base dropped (layers k, not 64 + k); route 8 the run's
// bounds ignored (spread run: 0..24 drawn); gate 16 airTerr rewritten after GL's draw; gate 32 the other key: REFUSE "key";
// gate 64 the inject bound on volHist: REFUSE "feedback"; all-on 16384 WW_VKVIEW_RTBUF_RED=1 and 65536 a GL-owned cellRestir
// (REFUSE by name).
// Coverage limit: airFarVis (the cascades, the terrain shadow-height map, the clouds) reads blocks this gate binds zero (ussOn,
// volCsm), so the far grid's own sun visibility is not reached here; the informative volFarMode line shows how much moves.
// Without the far route (no WW_VK_AIRFAR) the far arms print "no route" and the gate FAILS (the red-first run).
// Lane VKAIR21 (2026-11-16), the near grid's stage 1 with temporal off (census row |5, cellvolair.cpp vaPassInner's last else:
// s.temporal off, the MRT refused, or red 262144): appended after the far arms (every line above stays byte-equal). GL draws it
// as the app does: stage1()'s near set with volTemporal 0 and volHistOk 0, nothing on volHist, one attachment, layer k of the
// inject (21 x 12 x 64), k in [0, 64). Vulkan: airTemporalRun with `far` and `plain` set (base 0, 0..64) into the SHARED inject,
// the route's third instance. Arms "plain def" (key 0) and "plain all-on" (key 1, the atlas and both RT GI buffers bound).
// Bar (set before the first run, never widened): GL writes every texel of the 64 layers, all finite, at least half lit; Vulkan
// vs GL |vk - gl| <= 1e-4 max(1, |gl|) on every value; stat (takeAirPlainStat) 1 run, 64 draws, 0 image copies, and the
// reader's and the far grid's stats 0 runs (the plain instance is its own); read twice EXACT; all-on: rtBufOf lookups 1 and 1 a
// run; a rerun after the reds EXACT against the first run.
// GL probes (LIVE >= 50): volLayer 0 on every draw, airTerr rewritten, volLampNow rewritten, the noise rewritten.
// Reds, each must FAIL: route 1 volLayer stuck at 0; gate 2 the route dropped (the run skipped: the inject keeps the sentinel);
// gate 4 a wrong input bound (the history on volLampNow: FAIL, or REFUSE named "volLampNow"); gate 8 volLampNow rewritten after
// GL's draw; gate 16 airTerr rewritten after GL's draw; gate 128 the noise rewritten after GL's draw; gate 32 the other key:
// REFUSE "key"; gate 64 the inject bound on volHist: REFUSE "feedback"; all-on 16384 and 65536 as the far arms.
// airtemporalview.sh greps cellvolair.cpp for airPlainOnGl (the app's last else has a route).
// Coverage limit: stage 2 in that else (the integrate when oneSum is off and fogRun did not take it) stays GL, not gated here.
// Without the plain route (no WW_VK_AIRPLAIN) the plain arms print "no route" and the gate FAILS (the red-first run).
// Last line `AIRTEMPORALVIEW GATE OK` or `AIRTEMPORALVIEW GATE FAILED`.
#define FLD_NO_MAIN
#include "farloddraw.cpp"
#undef FLD_NO_MAIN
#include "features/air/cellvolairshare.h"
#include "features/air/airlampatlasshare.h"	// lane VKAIR17

namespace atv
{
using namespace fv;
using namespace fld;

const int kAtCols = 21, kAtRows = 12, kAtSlices = 64, kAtLive = 50, kAtNoiseN = 8, kAtTerrW = 33, kAtTerrH = 17;
const float kAtSentinel = 7.0f;
const GLenum kAtRgba = 0x1908, kAt3d = 0x806F, kAt2da = 0x8C1A, kAtUbyte = 0x1401;

struct At
{
	void( WINAPI * vertexAttribPointer )( GLuint, GLint, GLenum, GLboolean, GLsizei, const void * ) = nullptr;
	void( WINAPI * enableVertexAttribArray )( GLuint ) = nullptr;
	void( WINAPI * genVertexArrays )( GLsizei, GLuint * ) = nullptr;
	void( WINAPI * bindVertexArray )( GLuint ) = nullptr;
	void( WINAPI * getActiveUniformBlockName )( GLuint, GLuint, GLsizei, GLsizei *, char * ) = nullptr;
	void( WINAPI * clearTexImage )( GLuint, GLint, GLenum, GLenum, const void * ) = nullptr;
	void( WINAPI * clearTexSubImage )( GLuint, GLint, GLint, GLint, GLint, GLsizei, GLsizei, GLsizei, GLenum, GLenum, const void * ) = nullptr;
	void( WINAPI * framebufferTextureLayer )( GLenum, GLenum, GLuint, GLint, GLint ) = nullptr;
	void( WINAPI * framebufferTexture2D )( GLenum, GLenum, GLenum, GLuint, GLint ) = nullptr;
	void( WINAPI * deleteFramebuffers )( GLsizei, const GLuint * ) = nullptr;
};
At at;

bool loadAt()
{
	return load( at.vertexAttribPointer, "glVertexAttribPointer" ) && load( at.enableVertexAttribArray, "glEnableVertexAttribArray" )
		&& load( at.genVertexArrays, "glGenVertexArrays" ) && load( at.bindVertexArray, "glBindVertexArray" )
		&& load( at.getActiveUniformBlockName, "glGetActiveUniformBlockName" ) && load( at.clearTexImage, "glClearTexImage" )
		&& load( at.clearTexSubImage, "glClearTexSubImage" ) && load( at.framebufferTextureLayer, "glFramebufferTextureLayer" )
		&& load( at.framebufferTexture2D, "glFramebufferTexture2D" ) && load( at.deleteFramebuffers, "glDeleteFramebuffers" );
}

// the helper's GL (cellvolairshare.h): the gate's entry points
struct GateFn
{
	void glGenTextures( GLsizei n, GLuint * t ) { ::glGenTextures( n, t ); }
	void glDeleteTextures( GLsizei n, const GLuint * t ) { ::glDeleteTextures( n, t ); }
	void glBindTexture( GLenum a, GLuint t ) { ::glBindTexture( a, t ); }
	void glTexParameteri( GLenum a, GLenum p, GLint v ) { ::glTexParameteri( a, p, v ); }
	void glTexImage3D( GLenum a, GLint l, GLint fmt, GLsizei w, GLsizei h, GLsizei d, GLint b, GLenum pf, GLenum ty, const void * px )
	{
		f.texImage3D( a, l, fmt, w, h, d, b, pf, ty, px );
	}
	// lane VKAIR17: the atlas helper's (airlampatlasshare.h)
	void glTexImage2D( GLenum a, GLint l, GLint fmt, GLsizei w, GLsizei h, GLint b, GLenum pf, GLenum ty, const void * px )
	{
		::glTexImage2D( a, l, fmt, w, h, b, pf, ty, px );
	}
	void glGetIntegerv( GLenum p, GLint * v ) { ::glGetIntegerv( p, v ); }
	void glGenFramebuffers( GLsizei n, GLuint * t ) { f.genFramebuffers( n, t ); }
	void glDeleteFramebuffers( GLsizei n, const GLuint * t ) { at.deleteFramebuffers( n, t ); }
	void glBindFramebuffer( GLenum a, GLuint t ) { f.bindFramebuffer( a, t ); }
	void glFramebufferTexture2D( GLenum a, GLenum p, GLenum tg, GLuint t, GLint l ) { at.framebufferTexture2D( a, p, tg, t, l ); }
	void glDrawBuffer( GLenum b ) { ::glDrawBuffer( b ); }
	GLenum glCheckFramebufferStatus( GLenum a ) { return f.checkFramebufferStatus( a ); }
};
GateFn gfn;
bool gRtFresh = true;	// lane VKAIR18: the gate owner's fresh() answer (red 32768: false)
int gAtlasOn = 0;	// lane VKAIR17: 1 = stage1 sets the atlas uniforms (the atlas arm)
char gAtlasKey, gAtlasPlainKey;

// key slots (a static array: never an address inside a container)
enum Slot { kInject, kOut6, kOut4, kHistA, kHistB, kLampNow, kNoise, kTerr, kTile, kInjectFar, kSlots };
char gKeys[kSlots];
std::uintptr_t keyOf( int s )
{
	return reinterpret_cast<std::uintptr_t>( &gKeys[s] );
}

struct Inputs
{
	GLuint histA = 0, histB = 0, lampNow = 0, noise = 0, terr = 0;
	VaShareState sA, sB, sL, sN, sT;
	int strata = 0;
};

VaShareSpec histSpec( int ns )
{
	return { 1, fRgba32f, kAtRgba, GL_FLOAT, kAtCols, kAtRows, ns * kAtSlices, true, false };
}
const VaShareSpec kLampSpec { 1, fRgba32f, kAtRgba, GL_FLOAT, kAtCols, kAtRows, kAtSlices, false, false };
const VaShareSpec kNoiseSpec { 1, fR8, fRed, kAtUbyte, kAtNoiseN, kAtNoiseN, kAtNoiseN, true, true };
const VaShareSpec kTerrSpec { 2, fR32f, fRed, GL_FLOAT, kAtTerrW, kAtTerrH, 2, true, false };

// a GL clear per layer (never glTexSubImage into a shared colour volume: ww-gl-vk-interop "fill with a GL clear")
void fillHist( GLuint t, int layers, int salt )
{
	for ( int z = 0; z < layers; z++ ) {
		const float v[4] = { 0.05f + 0.011f * float( ( z * 7 + salt ) % 37 ), 0.04f + 0.013f * float( ( z * 5 + 3 * salt ) % 29 ),
			0.03f + 0.017f * float( ( z * 3 + 5 * salt ) % 23 ), 1.0f };
		at.clearTexSubImage( t, 0, 0, 0, z, kAtCols, kAtRows, 1, kAtRgba, GL_FLOAT, v );
		const float h[4] = { v[0] * 0.5f, v[1] * 2.0f, v[2], 0.5f };
		at.clearTexSubImage( t, 0, 0, 0, z, 5, 4, 1, kAtRgba, GL_FLOAT, h );
	}
}

void fillLamp( GLuint t, int salt )
{
	for ( int z = 0; z < kAtSlices; z++ ) {
		const float v[4] = { 0.2f + 0.007f * float( ( z + salt ) % 41 ), 0.15f + 0.009f * float( ( 2 * z + salt ) % 31 ),
			0.1f + 0.012f * float( ( 3 * z + salt ) % 19 ), 0.0f };
		at.clearTexSubImage( t, 0, 0, 0, z, kAtCols, kAtRows, 1, kAtRgba, GL_FLOAT, v );
	}
}

void fillNoise( GLuint t, unsigned seed )
{
	std::mt19937 rng( seed * 977u + 5u );
	std::vector<std::uint8_t> d( size_t( kAtNoiseN ) * size_t( kAtNoiseN ) * size_t( kAtNoiseN ) );
	for ( std::uint8_t & x : d )
		x = std::uint8_t( rng() & 255u );
	glBindTexture( kAt3d, t );
	f.texSubImage3D( kAt3d, 0, 0, 0, 0, kAtNoiseN, kAtNoiseN, kAtNoiseN, fRed, kAtUbyte, d.data() );
	glBindTexture( kAt3d, 0 );
}

void fillTerr( GLuint t, unsigned seed )
{
	std::mt19937 rng( seed * 613u + 11u );
	std::uniform_real_distribution<float> u( 8.0f, 12.0f );	// around airTerrRef 10: the ground layer's anchor near the samples' height
	std::vector<float> d( size_t( kAtTerrW ) * size_t( kAtTerrH ) * 2 );
	for ( float & x : d )
		x = u( rng );
	glBindTexture( kAt2da, t );
	f.texSubImage3D( kAt2da, 0, 0, 0, 0, kAtTerrW, kAtTerrH, 2, fRed, GL_FLOAT, d.data() );
	glBindTexture( kAt2da, 0 );
}

void setNoiseMag( GLuint t, bool nearest )
{
	glBindTexture( kAt3d, t );
	glTexParameteri( kAt3d, GL_TEXTURE_MAG_FILTER, nearest ? GL_NEAREST : GL_LINEAR );
	glBindTexture( kAt3d, 0 );
}

//! every input as the app asks for it (want: shared); made ones filled as the app fills them
void ensureInputs( Inputs & in, int ns, bool want, bool noiseWant )
{
	const VaShareSpec hs = histSpec( ns );
	if ( vaShareEnsure( &gfn, keyOf( kHistA ), hs, want, in.histA, in.sA ) || in.strata != ns )
		fillHist( in.histA, ns * kAtSlices, 0 );
	if ( vaShareEnsure( &gfn, keyOf( kHistB ), hs, want, in.histB, in.sB ) || in.strata != ns )
		fillHist( in.histB, ns * kAtSlices, 9 );
	if ( vaShareEnsure( &gfn, keyOf( kLampNow ), kLampSpec, want, in.lampNow, in.sL ) )
		fillLamp( in.lampNow, 0 );
	if ( vaShareEnsure( &gfn, keyOf( kNoise ), kNoiseSpec, want && noiseWant, in.noise, in.sN ) )
		fillNoise( in.noise, 1 );
	if ( vaShareEnsure( &gfn, keyOf( kTerr ), kTerrSpec, want, in.terr, in.sT ) )
		fillTerr( in.terr, 1 );
	in.strata = ns;
}

void releaseInputs( Inputs & in )
{
	vaShareRelease( keyOf( kHistA ), in.histA, in.sA );
	vaShareRelease( keyOf( kHistB ), in.histB, in.sB );
	vaShareRelease( keyOf( kLampNow ), in.lampNow, in.sL );
	vaShareRelease( keyOf( kNoise ), in.noise, in.sN );
	vaShareRelease( keyOf( kTerr ), in.terr, in.sT );
	in.strata = 0;
}

void sentinel( GLuint t )
{
	const float s[4] = { kAtSentinel, kAtSentinel, kAtSentinel, kAtSentinel };
	at.clearTexImage( t, 0, kAtRgba, GL_FLOAT, s );
}

std::vector<float> readVol( GLuint t, int layers )
{
	std::vector<float> v( size_t( kAtCols ) * size_t( kAtRows ) * size_t( layers ) * 4 );
	glBindTexture( kAt3d, t );
	glGetTexImage( kAt3d, 0, kAtRgba, GL_FLOAT, v.data() );
	glBindTexture( kAt3d, 0 );
	return v;
}

struct Pair
{
	std::vector<float> inj, out;
};

struct VolV
{
	int texels = 0, unwritten = 0, nonFinite = 0, diff = 0, bad = 0, moved = 0;
	double maxRel = 0.0;
	bool ok() const { return texels > 0 && unwritten == 0 && nonFinite == 0 && bad == 0; }
	const char * word() const { return !ok() ? "FAIL" : diff == 0 ? "EXACT" : "BOUND"; }
};

void judgeInto( VolV & v, const std::vector<float> & gl, const std::vector<float> & vk )
{
	if ( gl.size() != vk.size() ) {
		v.bad++;
		return;
	}
	for ( size_t t = 0; t < gl.size() / 4; t++ ) {
		v.texels++;
		bool m = false;
		bool sg = true, sk = true;
		for ( int c = 0; c < 4; c++ ) {
			sg = sg && gl[4 * t + size_t( c )] == kAtSentinel;
			sk = sk && vk[4 * t + size_t( c )] == kAtSentinel;
		}
		v.unwritten += sg || sk;
		m = sg || sk;
		for ( int c = 0; c < 4; c++ ) {
			const double g = gl[4 * t + size_t( c )], k = vk[4 * t + size_t( c )];
			if ( !std::isfinite( g ) || !std::isfinite( k ) ) {
				v.nonFinite++;
				m = true;
				continue;
			}
			const double rel = std::fabs( k - g ) / std::max( 1.0, std::fabs( g ) );
			v.maxRel = std::max( v.maxRel, rel );
			v.diff += k != g;
			v.bad += rel > 1e-4;
			m = m || rel > 1e-4;
		}
		v.moved += m;
	}
}

VolV judge( const Pair & gl, const Pair & vk )
{
	VolV v;
	judgeInto( v, gl.inj, vk.inj );
	judgeInto( v, gl.out, vk.out );
	return v;
}

// lane VKAIR19: the far grid's run = layers [lo, hi) of the inject, cols x rows of the texture; every other texel the sentinel on both
struct FarV
{
	int run = 0, unwritten = 0, nonFinite = 0, diff = 0, bad = 0, stray = 0, moved = 0, lit = 0;
	double maxRel = 0.0;
	bool ok() const { return run > 0 && unwritten == 0 && nonFinite == 0 && bad == 0 && stray == 0; }
	const char * word() const { return !ok() ? "FAIL" : diff == 0 ? "EXACT" : "BOUND"; }
};

FarV judgeFar( const std::vector<float> & gl, const std::vector<float> & vk, int cols, int rows, int lo, int hi )
{
	FarV v;
	const size_t layerFloats = static_cast<size_t>( kAtCols ) * static_cast<size_t>( kAtRows ) * 4;
	if ( gl.size() != vk.size() || gl.size() % layerFloats != 0 ) {
		v.bad++;
		return v;
	}
	const size_t layers = gl.size() / layerFloats;
	for ( size_t z = 0; z < layers; z++ )
		for ( int y = 0; y < kAtRows; y++ )
			for ( int x = 0; x < kAtCols; x++ ) {
				const size_t t = ( ( z * size_t( kAtRows ) + size_t( y ) ) * size_t( kAtCols ) + size_t( x ) ) * 4;
				bool sg = true, sk = true;
				for ( int c = 0; c < 4; c++ ) {
					sg = sg && gl[t + size_t( c )] == kAtSentinel;
					sk = sk && vk[t + size_t( c )] == kAtSentinel;
				}
				const bool inRun = int( z ) >= lo && int( z ) < hi && x < cols && y < rows;
				if ( !inRun ) {
					const bool st = !sg || !sk;
					v.stray += st;
					v.moved += st;
					continue;
				}
				v.run++;
				bool m = sg || sk;
				v.unwritten += m;
				v.lit += !sg && ( gl[t] > 0.0f || gl[t + 1] > 0.0f || gl[t + 2] > 0.0f );
				for ( int c = 0; c < 4; c++ ) {
					const double g = gl[t + size_t( c )], k = vk[t + size_t( c )];
					if ( !std::isfinite( g ) || !std::isfinite( k ) ) {
						v.nonFinite++;
						m = true;
						continue;
					}
					const double rel = std::fabs( k - g ) / std::max( 1.0, std::fabs( g ) );
					v.maxRel = std::max( v.maxRel, rel );
					v.diff += k != g;
					v.bad += rel > 1e-4;
					m = m || rel > 1e-4;
				}
				v.moved += m;
			}
	return v;
}

//! a uniform by name in its own type (ints for int / bool types), the values padded with zeros; false when GL has none
bool setU( GLuint p, const char * name, std::vector<double> vals )
{
	GLint nu = 0;
	f.getProgramiv( p, fActiveUnis, &nu );
	for ( GLint i = 0; i < nu; i++ ) {
		char nm[256] = {};
		GLint sz = 0;
		GLenum ty = 0;
		f.getActiveUniform( p, GLuint( i ), 256, nullptr, &sz, &ty, nm );
		std::string base( nm );
		if ( base.size() > 3 && base.compare( base.size() - 3, 3, "[0]" ) == 0 )
			base.resize( base.size() - 3 );
		if ( base != name )
			continue;
		int comp = 0;
		bool isInt = false;
		switch ( ty ) {
		case 0x1406: comp = 1; break;
		case 0x8B50: comp = 2; break;
		case 0x8B51: comp = 3; break;
		case 0x8B52: comp = 4; break;
		case 0x1404: case 0x8B56: comp = 1; isInt = true; break;
		case 0x8B53: case 0x8B57: comp = 2; isInt = true; break;
		case 0x8B54: case 0x8B58: comp = 3; isInt = true; break;
		case 0x8B55: case 0x8B59: comp = 4; isInt = true; break;
		default: return false;
		}
		const GLsizei cnt = GLsizei( std::max( sz, 1 ) );
		vals.resize( size_t( cnt ) * size_t( comp ), 0.0 );
		const GLint l = f.getUniformLocation( p, nm );
		f.useProgram( p );
		if ( isInt ) {
			std::vector<GLint> iv( vals.size() );
			for ( size_t k = 0; k < vals.size(); k++ )
				iv[k] = GLint( std::lround( vals[k] ) );
			( comp == 1 ? f.uniform1iv : comp == 2 ? f.uniform2iv : comp == 3 ? f.uniform3iv : f.uniform4iv )( l, cnt, iv.data() );
		} else {
			std::vector<float> fv( vals.size() );
			for ( size_t k = 0; k < vals.size(); k++ )
				fv[k] = float( vals[k] );
			( comp == 1 ? f.uniform1fv : comp == 2 ? f.uniform2fv : comp == 3 ? f.uniform3fv : f.uniform4fv )( l, cnt, fv.data() );
		}
		return true;
	}
	return false;
}

//! the stage's own uniforms (cellvolair.cpp vaPassInner, stage 1 temporal); "" or the first required one GL's program lacks
std::string stage1( GLuint p, int temporal, int ns, int layer, std::string * absent = nullptr )
{
	struct U
	{
		const char * n;
		std::vector<double> v;
		bool need;
	};
	const double lnr = std::log( 400.0 );
	const std::vector<U> us = {
		{ "volStage", { 1 }, true }, { "volLayer", { double( layer ) }, true }, { "volDims", { kAtCols, kAtRows, kAtSlices }, true },
		{ "volTemporal", { double( temporal ) }, true }, { "volHistOk", { 1 }, true }, { "volHistStrata", { double( ns ) }, true },
		{ "volEnvSpread", { ns == 6 ? 1.0 : 0.0 }, false }, { "volQ", { 1 }, false }, { "volSunRate", { 1, 0 }, false },
		{ "cellOn", { 1 }, true }, { "volTerms", { 15 }, true }, { "volLampVol", { 1 }, true }, { "volDrift", { 0.6, 1000.0, 0.0, 0.5 }, true },
		{ "volDriftOfs", { 3.0, 7.0, 1.0, 0.002, 11.0, 5.0, 2.0, 0.0035 }, true }, { "airTerrOn", { 1 }, true },
		{ "airTerrXf", { 0.004, 0.5, 0.004, 0.5 }, false }, { "airTerrFineXf", { 0.02, 0.5, 0.02, 0.5 }, false },
		{ "airTerrRef", { 10.0 }, false }, { "airMed", { 0.002, 0.3, 0.5, 2.0 }, true },	// a thin medium: the seeded sigma saturated every slice
 { "volProj", { 1.2, 2.1 }, false }, { "volSlices", { 0.5, lnr, kAtSlices, 1.0 }, false },
		{ "volReproj", { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0 }, false }, { "volPrevGrid", { 1.2, 2.1, 0.5, lnr }, false },
		{ "volPrevScale", { 70.0 }, false }, { "volDepthSkip", { 0 }, false }, { "volJitter", { 0, 0, 0, 0 }, false },
		{ "fogDistScale", { 70.0 }, false }, { "cellRow", { 1.2, 0, 1.6, 100, 0, 1, 0, -50, 0, 0, 1, 30 }, false } };
	for ( const U & u : us )
		if ( !setU( p, u.n, u.v ) ) {
			if ( u.need )
				return u.n;
			if ( absent )
				*absent += std::string( " " ) + u.n;
		}
	if ( gAtlasOn ) {	// lane VKAIR17: the atlas arm
		for ( const U & u : std::vector<U> { { "volLampVis", { 1.0 }, true }, { "volLampN", { 2.0 }, true }, { "volLampAtlasRed", { 0.0 }, true } } )
			if ( !setU( p, u.n, u.v ) )
				return u.n;
	}
	return "";
}

struct Units
{
	int hist = -1, lampNow = -1, noise = -1, terr = -1, tile = -1, ddgi = -1, atlas = -1, table = -1, restir = -1;
};

//! blocks bound zero, loose uniforms seeded, samplers on their own units (nothing bound); the stage's units back
std::string setupProg( GLuint p, const char * tag, Units & u )
{
	GLint nb = 0;
	f.getProgramiv( p, 0x8A36 /* GL_ACTIVE_UNIFORM_BLOCKS */, &nb );
	std::vector<GLuint> ubo( size_t( std::max( nb, 1 ) ) );
	f.genBuffers( GLsizei( ubo.size() ), ubo.data() );
	for ( GLint b = 0; b < nb; b++ ) {
		GLint bytes = 16;
		f.getActiveUniformBlockiv( p, GLuint( b ), fBlockSize, &bytes );
		f.uniformBlockBinding( p, GLuint( b ), GLuint( b ) );
		const std::vector<std::uint8_t> z( size_t( std::max( bytes, 16 ) ), 0 );
		f.bindBuffer( fUbo, ubo[size_t( b )] );
		f.bufferData( fUbo, std::ptrdiff_t( z.size() ), z.data(), fStatic );
		f.bindBufferBase( fUbo, GLuint( b ), ubo[size_t( b )] );
	}
	_putenv( "FLD_ON=fogOn" );
	std::mt19937 rng( 23 );
	const int seeded = seedUnis( p, rng );
	f.useProgram( p );
	GLint nu = 0;
	f.getProgramiv( p, fActiveUnis, &nu );
	int unit = 1, samplers = 0;
	for ( GLint i = 0; i < nu; i++ ) {
		char nm[256] = {};
		GLint sz = 0;
		GLenum ty = 0;
		f.getActiveUniform( p, GLuint( i ), 256, nullptr, &sz, &ty, nm );
		const bool image = ty >= 0x904D && ty <= 0x906C;
		const bool sampler = !image && ( ( ty >= 0x8B5D && ty <= 0x8B64 ) || ( ty >= 0x8DC0 && ty <= 0x8DD8 ) || ( ty >= 0x9008 && ty <= 0x9110 ) );
		if ( !sampler )
			continue;
		std::vector<GLint> us( size_t( std::max( sz, 1 ) ) );
		for ( GLint & x : us )
			x = unit++;
		const std::string n( nm );
		int * slot = n == "volHist" ? &u.hist : n == "volLampNow" ? &u.lampNow : n == "volNoise" ? &u.noise : n == "airTerr" ? &u.terr
			: n == "volTile" ? &u.tile : n == "cellDdgiAtlas" ? &u.ddgi : n == "volLampAtlas" ? &u.atlas : n == "volLampTable" ? &u.table
			: n == "cellRestir" ? &u.restir : nullptr;
		if ( slot )
			*slot = us[0];
		f.uniform1iv( f.getUniformLocation( p, nm ), GLsizei( us.size() ), us.data() );
		samplers += int( us.size() );
	}
	std::printf( "AIRTEMPORALVIEW %s: %d blocks zero, %d loose uniforms seeded, %d samplers on units 1..%d (volHist %d volLampNow %d "
				 "volNoise %d airTerr %d volTile %d cellDdgiAtlas %d)\n",
		tag, nb, seeded, samplers, unit - 1, u.hist, u.lampNow, u.noise, u.terr, u.tile, u.ddgi );
	if ( u.hist < 0 || u.lampNow < 0 || u.noise < 0 || u.terr < 0 )
		return "no volHist / volLampNow / volNoise / airTerr sampler";
	// lane VKAIR17: WW_AIR_LEAK = WW_CELL_FOGLEAK && WW_CELL_RTDDGI: only the RT GI key samples the atlas; the default key must not
	std::printf( "AIRTEMPORALVIEW %s: volLampAtlas %d volLampTable %d\n", tag, u.atlas, u.table );
	std::printf( "AIRTEMPORALVIEW %s: cellRestir %d\n", tag, u.restir );
	const bool rtKey = std::string( tag ) == "rt";
	if ( rtKey && ( u.atlas < 0 || u.table < 0 ) )
		return "no volLampAtlas / volLampTable sampler";
	if ( !rtKey && ( u.atlas >= 0 || u.table >= 0 ) )
		return "the default key samples volLampAtlas / volLampTable (the census's default column assumes it does not)";
	if ( unit > 160 )
		return "more sampler units than the gate binds";
	return "";
}
}  // namespace atv

int main( int argc, char ** argv )
{
	using namespace fv;
	using namespace fld;
	using namespace atv;
	gTag = "AIRTEMPORALVIEW";
	if ( argc < 2 ) {
		std::printf( "usage: airtemporalview <prep dir>\nAIRTEMPORALVIEW GATE FAILED\n" );
		return 2;
	}
	const std::string dir = argv[1];
	auto fatal = [&]( const std::string & why ) {
		std::printf( "AIRTEMPORALVIEW GATE FAILED (%s)\n", why.c_str() );
		return 2;
	};
	const int S = 4, W = 256, H = 160;
	gOpenGl = LoadLibraryA( "opengl32.dll" );
	WNDCLASSA wc {};
	wc.style = CS_OWNDC;
	wc.lpfnWndProc = wndProc;
	wc.hInstance = GetModuleHandleA( nullptr );
	wc.lpszClassName = "wwairtemporalview";
	RegisterClassA( &wc );
	HWND wnd = CreateWindowExA( WS_EX_NOACTIVATE, "wwairtemporalview", "air temporal harness", WS_OVERLAPPEDWINDOW, 40, 40, 320, 200, nullptr,
		nullptr, wc.hInstance, nullptr );
	HDC dc = GetDC( wnd );
	PIXELFORMATDESCRIPTOR pfd { sizeof( pfd ), 1, PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER, PFD_TYPE_RGBA, 32 };
	SetPixelFormat( dc, ChoosePixelFormat( dc, &pfd ), &pfd );
	HGLRC glrc = wglCreateContext( dc );
	wglMakeCurrent( dc, glrc );
	ShowWindow( wnd, SW_SHOWNOACTIVATE );
	SetWindowPos( wnd, HWND_BOTTOM, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE );
	pump();
	if ( !loadAll() || !loadMore() || !loadAt() )
		return fatal( "GL entry points" );
	std::printf( "AIRTEMPORALVIEW GL on %s\n", reinterpret_cast<const char *>( glGetString( GL_RENDERER ) ) );
	glPixelStorei( GL_PACK_ALIGNMENT, 1 );
	glPixelStorei( GL_UNPACK_ALIGNMENT, 1 );
	std::string why;
	if ( !WwVkPresent::viewBegin( W, H, S, glProc, why ) )
		return fatal( "the view: " + why );

	// ---- the targets: shared (Vulkan's) and GL-owned twins; the tile (shared RG32F, a clear)
	unsigned vkInj = 0, vkOut[2] = {}, tile = 0;
	if ( !WwVkPresent::sharedTarget( keyOf( kInject ), 1, kAtCols, kAtRows, kAtSlices, fRgba32f, vkInj, why, 1 ) )
		return fatal( "the shared inject: " + why );
	for ( int i = 0; i < 2; i++ )
		if ( !WwVkPresent::sharedTarget( keyOf( i ? kOut4 : kOut6 ), 1, kAtCols, kAtRows, ( i ? 4 : 6 ) * kAtSlices, fRgba32f, vkOut[i], why, 1 ) )
			return fatal( "the shared out history: " + why );
	if ( !WwVkPresent::sharedTarget( keyOf( kTile ), 0, kAtCols, kAtRows, 1, fRg32f, tile, why ) )
		return fatal( "the shared tile: " + why );
	{
		const float tv[2] = { 60.0f, 0.75f };
		at.clearTexImage( tile, 0, fRg, GL_FLOAT, tv );
		glBindTexture( GL_TEXTURE_2D, tile );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
		glBindTexture( GL_TEXTURE_2D, 0 );
	}
	GLuint glInj = 0, glOut[2] = {};
	auto vol = [&]( GLuint & t, int layers ) {
		glGenTextures( 1, &t );
		glBindTexture( kAt3d, t );
		f.texImage3D( kAt3d, 0, GLint( fRgba32f ), kAtCols, kAtRows, layers, 0, kAtRgba, GL_FLOAT, nullptr );
		glTexParameteri( kAt3d, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
		glTexParameteri( kAt3d, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
		glBindTexture( kAt3d, 0 );
	};
	vol( glInj, kAtSlices );
	vol( glOut[0], 6 * kAtSlices );
	vol( glOut[1], 4 * kAtSlices );
	// lane VKAIR17: the atlas by the app's helper (wished: shared), a depth clear through its FBO; the table a GL texture buffer
	GLuint atlasTex = 0, atlasFbo = 0, tableBuf = 0, tableTex = 0;
	bool atlasFboOk = false;
	AirAtlasShare atlasSh;
	const std::uintptr_t atlasKeyV = reinterpret_cast<std::uintptr_t>( &gAtlasKey );
	auto clearAtlas = [&]( GLuint fbo, double d ) {
		f.bindFramebuffer( fFb, fbo );
		glDepthMask( GL_TRUE );
		glDisable( GL_SCISSOR_TEST );
		glClearDepth( d );
		glClear( GL_DEPTH_BUFFER_BIT );
		glClearDepth( 1.0 );
		f.bindFramebuffer( fFb, 0 );
		glFinish();
	};
	{
		const int made = airAtlasEnsure( &gfn, atlasKeyV, 8192, true, atlasTex, atlasFbo, atlasFboOk, atlasSh );
		clearAtlas( atlasFbo, 0.6 );
		const bool found = farSharedOf( atlasTex ) == atlasKeyV;
		const bool ok = made == 1 && atlasSh.shared && atlasFboOk && found && atlasSh.size == 8192;
		std::printf( "AIRTEMPORALVIEW atlas make: made %d shared %d fbo %s farSharedOf %s size %d: %s\n", made, int( atlasSh.shared ),
			atlasFboOk ? "complete" : "INCOMPLETE", found ? "== key" : "NOT the key", atlasSh.size, ok ? "OK" : "FAIL" );
		std::printf( "AIRTEMPORALVIEW %s\n", airAtlasSay( 8192, true, atlasFboOk, atlasSh ).c_str() );
		if ( !ok )
			return fatal( "the atlas make check" );
		std::vector<float> t( 2 * 16, 0.0f );
		for ( int i = 0; i < 2; i++ ) {
			t[static_cast<size_t>( i ) * 16 + 12] = 64.0f;
			t[static_cast<size_t>( i ) * 16 + 13] = 1.0f;
		}
		f.genBuffers( 1, &tableBuf );
		f.bindBuffer( 0x8C2A, tableBuf );
		f.bufferData( 0x8C2A, std::ptrdiff_t( t.size() * 4 ), t.data(), fStatic );
		f.bindBuffer( 0x8C2A, 0 );
		glGenTextures( 1, &tableTex );
		glBindTexture( 0x8C2A, tableTex );
		ex.texBuffer( 0x8C2A, GLenum( fRgba32f ), tableBuf );
		glBindTexture( 0x8C2A, 0 );
	}
	GLuint boundAtlas = 0;	// the atlas arm: bound with the table on the program's units
	GLuint boundDdgi = 0, boundRestir = 0;	// lane VKAIR18: the all-on arm's RT GI inputs (the shared buffers' GL views)
	GLuint fb = 0;
	f.genFramebuffers( 1, &fb );
	GLuint vao = 0, vb = 0, ib = 0;
	at.genVertexArrays( 1, &vao );
	at.bindVertexArray( vao );
	static const float quad[12] = { -1, -1, 0, 1, -1, 0, -1, 1, 0, 1, 1, 0 };
	static const std::uint16_t idx[6] = { 0, 1, 2, 2, 1, 3 };
	f.genBuffers( 1, &vb );
	f.genBuffers( 1, &ib );
	f.bindBuffer( 0x8892, vb );
	f.bufferData( 0x8892, sizeof( quad ), quad, fStatic );
	f.bindBuffer( 0x8893, ib );
	f.bufferData( 0x8893, sizeof( idx ), idx, fStatic );
	at.vertexAttribPointer( 0, 3, GL_FLOAT, GL_FALSE, 12, nullptr );
	at.enableVertexAttribArray( 0 );

	const char * keyName[2] = { "def", "rt" };
	GLuint progs[2] = {};
	Units units[2];
	for ( int key = 0; key < 2; key++ ) {
		progs[key] = program( readFile( dir + "/" + keyName[key] + ".vert.glsl" ), readFile( dir + "/" + keyName[key] + ".frag.glsl" ), keyName[key] );
		if ( !progs[key] )
			return fatal( std::string( "GL program " ) + keyName[key] );
		const std::string e = setupProg( progs[key], keyName[key], units[key] );
		if ( !e.empty() )
			return fatal( std::string( keyName[key] ) + ": " + e );
		std::string absent;
		const std::string m = stage1( progs[key], 1, 6, 0, &absent );
		if ( !m.empty() )
			return fatal( std::string( keyName[key] ) + ": GL's program has no uniform " + m );
		std::printf( "AIRTEMPORALVIEW %s: optional stage uniforms GL's program lacks:%s\n", keyName[key], absent.empty() ? " none" : absent.c_str() );
	}

	Inputs in;
	auto bindInputs = [&]( const Units & u, GLuint hist ) {
		bindUnit( u.hist, kAt3d, hist );
		bindUnit( u.lampNow, kAt3d, in.lampNow );
		bindUnit( u.noise, kAt3d, in.noise );
		bindUnit( u.terr, kAt2da, in.terr );
		if ( u.tile >= 0 )
			bindUnit( u.tile, GL_TEXTURE_2D, tile );
		if ( boundAtlas && u.atlas >= 0 ) {
			bindUnit( u.atlas, GL_TEXTURE_2D, boundAtlas );
			bindUnit( u.table, 0x8C2A, tableTex );
		}
		if ( boundDdgi )
			bindUnit( u.ddgi, 0x8C2A, boundDdgi );
		if ( boundRestir )
			bindUnit( u.restir, 0x8C2A, boundRestir );
	};
	auto unbindInputs = [&]( const Units & u ) {
		bindUnit( u.hist, kAt3d, 0 );
		bindUnit( u.lampNow, kAt3d, 0 );
		bindUnit( u.noise, kAt3d, 0 );
		bindUnit( u.terr, kAt2da, 0 );
		if ( u.tile >= 0 )
			bindUnit( u.tile, GL_TEXTURE_2D, 0 );
		if ( boundAtlas && u.atlas >= 0 ) {
			bindUnit( u.atlas, GL_TEXTURE_2D, 0 );
			bindUnit( u.table, 0x8C2A, 0 );
		}
		if ( ( boundDdgi || boundRestir ) && u.ddgi >= 0 )	// a def-key draw has no RT units
			bindUnit( u.ddgi, 0x8C2A, 0 );
		if ( ( boundDdgi || boundRestir ) && u.restir >= 0 )
			bindUnit( u.restir, 0x8C2A, 0 );
	};

	// GL's stage 1 as vaPassInner draws it (64 layers, MRT 1 + strata) into GL's own targets (the sentinel first)
	auto glRun = [&]( int key, int temporal, int ns, GLuint hist, std::string & err, bool layerStuck = false ) {
		const GLuint out = glOut[ns == 4 ? 1 : 0];
		sentinel( glInj );
		sentinel( out );
		stage1( progs[key], temporal, ns, 0 );
		bindInputs( units[key], hist );
		f.bindFramebuffer( fFb, fb );
		const GLenum bufs[7] = { 0x8CE0, 0x8CE1, 0x8CE2, 0x8CE3, 0x8CE4, 0x8CE5, 0x8CE6 };
		f.drawBuffers( 1 + ns, bufs );
		drawState0();
		glViewport( 0, 0, kAtCols, kAtRows );
		at.bindVertexArray( vao );
		for ( int k = 0; k < kAtSlices; k++ ) {
			at.framebufferTextureLayer( fFb, 0x8CE0, glInj, 0, k );
			for ( int p = 0; p < ns; p++ )
				at.framebufferTextureLayer( fFb, GLenum( 0x8CE1 + p ), out, 0, p * kAtSlices + k );
			if ( k == 0 && f.checkFramebufferStatus( fFb ) != 0x8CD5 )
				err = "GL's stage-1 framebuffer is not complete";
			setU( progs[key], "volLayer", { layerStuck ? 0.0 : double( k ) } );
			glDrawElements( GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, nullptr );
		}
		for ( int p = 0; p <= ns; p++ )
			at.framebufferTextureLayer( fFb, GLenum( 0x8CE0 + p ), 0, 0, 0 );
		f.bindFramebuffer( fFb, 0 );
		unbindInputs( units[key] );
		glFinish();
		if ( const GLenum e = glGetError() )
			err = "GL error " + std::to_string( e ) + " in GL's stage 1";
		return Pair { readVol( glInj, kAtSlices ), readVol( out, ns * kAtSlices ) };
	};

	// lane VKAIR18: the all-on arm's RT GI inputs, shared by name as rtddgicell.cpp shares them (slot, format, owner)
	constexpr int kRtTexels = 4096;	// cellDdgiAtlas 4096 RGBA32F texels, cellRestir 64 x 64 RG32F
	GLuint rtBuf[2] = {}, rtTex[2] = {}, rtSrc[2] = {};	// rtSrc: GL's own buffers the owner copies from (rtBufCopy, as the app)
	auto rtData = [&]( int s2, int salt ) {
		std::vector<float> d( static_cast<size_t>( kRtTexels * ( s2 ? 2 : 4 ) ) );
		for ( size_t i = 0; i < d.size(); i++ )
			d[i] = 0.05f + 0.9f * float( ( i * 37 + static_cast<size_t>( salt ) * 11 ) % 97 ) / 97.0f;
		return d;
	};
	auto fillRt = [&]( int salt ) {
		for ( int s2 = 0; s2 < 2; s2++ ) {
			const std::vector<float> d = rtData( s2, salt );
			if ( !rtSrc[s2] )
				f.genBuffers( 1, &rtSrc[s2] );
			f.bindBuffer( 0x8C2A, rtSrc[s2] );
			f.bufferData( 0x8C2A, std::ptrdiff_t( d.size() * 4 ), d.data(), fStatic );
			f.bindBuffer( 0x8C2A, 0 );
			std::string w;
			if ( !WwVkPresent::rtBufCopy( s2, rtSrc[s2], d.size() * 4, w ) )
				std::printf( "AIRTEMPORALVIEW rtBufCopy %s: %s\n", WwVkPresent::rtBufName( s2 ), w.c_str() );
		}
	};
#ifdef WW_VK_AIRTEMPORAL
	{
		std::string why;
		const unsigned fmt[2] = { unsigned( fRgba32f ), unsigned( fRg32f ) };
		for ( int s2 = 0; s2 < 2; s2++ )
			if ( !WwVkPresent::rtBufShare( s2, fmt[s2], static_cast<std::size_t>( kRtTexels ) * ( s2 ? 8u : 16u ), rtBuf[s2], rtTex[s2], why ) )
				return fatal( std::string( "rtBufShare " ) + WwVkPresent::rtBufName( s2 ) + ": " + why );
		for ( int s2 = 0; s2 < 2; s2++ )
			WwVkPresent::rtBufOwner( s2, []( std::string & w ) -> bool {
				if ( gRtFresh )
					return true;
				w = "gate: the owner has nothing new";
				return false;
			} );
		fillRt( 0 );
		const GLenum fe = glGetError();
		std::printf( "AIRTEMPORALVIEW all-on inputs shared: cellDdgiAtlas buf %u tex %u, cellRestir buf %u tex %u, fill GL error 0x%04X\n",
			rtBuf[0], rtTex[0], rtBuf[1], rtTex[1], unsigned( fe ) );
		if ( fe )
			return fatal( "the all-on inputs' fill" );
	}
#endif
	struct Arm
	{
		int key, ns, temporal;
		bool atlas = false;	// lane VKAIR17: the atlas and its table bound (the RT GI key)
		bool allOn = false;	// lane VKAIR18: the all-on route's RT GI inputs bound too (cellDdgiAtlas, cellRestir: shared by name)
	};
	const Arm arms[5] = { { 0, 6, 1 }, { 1, 6, 1 }, { 0, 4, 2 }, { 1, 6, 1, true }, { 1, 6, 1, true, true } };
	int fails = 0, probesDead = 0, redsRun = 0, redsFired = 0;
	for ( const Arm & a : arms ) {
		const int key = a.key, ns = a.ns, tm = a.temporal;
		const std::string arm = std::string( keyName[key] ) + " strata " + std::to_string( ns ) + " temporal " + std::to_string( tm ) + ( a.atlas ? " atlas" : "" )
			+ ( a.allOn ? " all-on" : "" );
		const char * an = arm.c_str();
		gAtlasOn = a.atlas ? 1 : 0;
		boundAtlas = a.atlas ? atlasTex : 0;
		boundDdgi = a.allOn ? rtTex[0] : 0;
		boundRestir = a.allOn ? rtTex[1] : 0;
		if ( a.allOn ) {
			const bool unitsOk = units[key].ddgi >= 0 && units[key].restir >= 0 && rtTex[0] && rtTex[1];
			std::printf( "AIRTEMPORALVIEW %s units: cellDdgiAtlas %d cellRestir %d, shared views %u %u %s\n", an, units[key].ddgi,
				units[key].restir, rtTex[0], rtTex[1], unitsOk ? "OK" : "FAIL" );
			fails += !unitsOk;
		}
		ensureInputs( in, ns, true, true );
		std::printf( "AIRTEMPORALVIEW %s inputs shared: histA %d histB %d lampNow %d noise %d terr %d\n", an, int( in.sA.shared ), int( in.sB.shared ),
			int( in.sL.shared ), int( in.sN.shared ), int( in.sT.shared ) );
		if ( !in.sA.shared || !in.sB.shared || !in.sL.shared || !in.sN.shared || !in.sT.shared )
			return fatal( arm + ": an input was not made shared" );
		std::string err;
		const Pair g = glRun( key, tm, ns, in.histA, err );
		if ( !err.empty() )
			return fatal( arm + ": " + err );
		{
			const VolV self = judge( g, g );
			std::printf( "AIRTEMPORALVIEW %s GL written: unwritten %d non-finite %d of %d texels\n", an, self.unwritten, self.nonFinite, self.texels );
			fails += self.unwritten != 0 || self.nonFinite != 0 || self.texels == 0;
		}
		// ---- GL probes: each red's change reaches the stage
		auto probe = [&]( const char * what, const Pair & other ) {
			const VolV p = judge( g, other );
			const bool live = p.moved >= kAtLive;
			std::printf( "AIRTEMPORALVIEW %s probe %s: moved %d of %d %s\n", an, what, p.moved, p.texels, live ? "LIVE" : "TESTS NOTHING" );
			probesDead += !live;
		};
		probe( "volLayer 0 on every draw", glRun( key, tm, ns, in.histA, err, true ) );
		{
			Pair perm = g;
			const size_t layer = size_t( kAtCols ) * size_t( kAtRows ) * 4 * size_t( kAtSlices );
			for ( int p = 0; p < ns; p++ )
				std::copy( g.out.begin() + std::ptrdiff_t( size_t( ( p + 1 ) % ns ) * layer ),
					g.out.begin() + std::ptrdiff_t( size_t( ( p + 1 ) % ns + 1 ) * layer ), perm.out.begin() + std::ptrdiff_t( size_t( p ) * layer ) );
			probe( "out strata permuted", perm );
		}
		probe( "second history bound", glRun( key, tm, ns, in.histB, err ) );
		fillLamp( in.lampNow, 13 );
		probe( "volLampNow rewritten", glRun( key, tm, ns, in.histA, err ) );
		fillLamp( in.lampNow, 0 );
		fillNoise( in.noise, 2 );
		probe( "noise rewritten", glRun( key, tm, ns, in.histA, err ) );
		fillNoise( in.noise, 1 );
		setNoiseMag( in.noise, true );
		probe( "noise MAG NEAREST", glRun( key, tm, ns, in.histA, err ) );
		setNoiseMag( in.noise, false );
		fillTerr( in.terr, 2 );
		probe( "airTerr rewritten", glRun( key, tm, ns, in.histA, err ) );
		fillTerr( in.terr, 1 );
		if ( a.allOn ) {	// informative, not a bar: stage 1 reads cellRestir only under volLeakRed & 4
			fillRt( 1 );
			const VolV p = judge( g, glRun( key, tm, ns, in.histA, err ) );
			std::printf( "AIRTEMPORALVIEW %s RT GI inputs rewritten: moved %d of %d (informative)\n", an, p.moved, p.texels );
			fillRt( 0 );
		}
		if ( !err.empty() )
			return fatal( arm + " (probes): " + err );

#ifdef WW_VK_AIRTEMPORAL
		const std::uintptr_t outKey = keyOf( ns == 4 ? kOut4 : kOut6 );
		const GLuint vkO = vkOut[ns == 4 ? 1 : 0];
		auto vkRun = [&]( int red, GLuint hist, std::string & w, WwVkPresent::AirTemporalStat & st, Pair * res ) {
			sentinel( vkInj );
			sentinel( vkO );
			stage1( progs[key], tm, ns, 0 );
			bindInputs( units[key], hist );
			( void )WwVkPresent::takeAirTemporalStat();
			WwVkPresent::AirTemporalRun r;
			r.prog = progs[key];
			r.rt = key == 1;
			r.wwDefs = WwVkPresent::airTemporalKey( ( key == 1 ) != ( ( red & 32 ) != 0 ) );
			r.cols = kAtCols;
			r.rows = kAtRows;
			r.slices = kAtSlices;
			r.strata = ns;
			r.injectKey = keyOf( kInject );
			r.histOutKey = ( red & 64 ) ? keyOf( kHistA ) : outKey;
			r.red = red & 3;
			const bool ok = WwVkPresent::airTemporalRun( r, gw, w );
			st = WwVkPresent::takeAirTemporalStat();
			unbindInputs( units[key] );
			if ( ok && res )
				*res = Pair { readVol( vkInj, kAtSlices ), readVol( vkO, ns * kAtSlices ) };
			return ok;
		};
		std::string w;
		WwVkPresent::AirTemporalStat st;
		Pair v0;
		if ( !vkRun( 0, in.histA, w, st, &v0 ) ) {
			std::printf( "AIRTEMPORALVIEW %s: Vulkan refused: %s\n", an, w.c_str() );
			fails++;
			continue;
		}
		{
			const Pair again { readVol( vkInj, kAtSlices ), readVol( vkO, ns * kAtSlices ) };
			const VolV twice = judge( v0, again );
			const bool same = twice.ok() && twice.diff == 0;
			std::printf( "AIRTEMPORALVIEW %s read twice: %s (differ %d, unwritten %d)\n", an, same ? "EXACT" : "FAIL", twice.diff, twice.unwritten );
			fails += !same;
		}
		{
			const VolV v = judge( g, v0 );
			std::printf( "AIRTEMPORALVIEW %s Vulkan vs GL: %s (texels %d, unwritten %d, non-finite %d, differ %d, past 1e-4 rel %d, max rel %.3g)\n", an,
				v.word(), v.texels, v.unwritten, v.nonFinite, v.diff, v.bad, v.maxRel );
			fails += !v.ok();
			const bool cp = st.runs == 1 && st.draws == kAtSlices && st.copies == 0;
			std::printf( "AIRTEMPORALVIEW %s stat: runs %d draws %d copies %d buffer copies %d (want 1 run, %d draws, 0 copies) %s\n", an, st.runs,
				st.draws, st.copies, st.bufCopies, kAtSlices, cp ? "OK" : "FAIL" );
			fails += !cp;
		}
		if ( a.allOn ) {	// one run asks the shared buffers by name once a name
			const WwVkPresent::RtBufStat s0 = WwVkPresent::rtBufStat();
			std::string lw;
			WwVkPresent::AirTemporalStat ls;
			Pair lv;
			const bool ran = vkRun( 0, in.histA, lw, ls, &lv );
			const WwVkPresent::RtBufStat s1 = WwVkPresent::rtBufStat();
			const unsigned long long da = s1.lookups[0] - s0.lookups[0], dr = s1.lookups[1] - s0.lookups[1];
			const bool ok = ran && da == 1 && dr == 1;
			std::printf( "AIRTEMPORALVIEW %s rtBufOf lookups a run: cellDdgiAtlas %llu cellRestir %llu (want 1 and 1) %s%s%s\n", an, da, dr,
				ok ? "OK" : "FAIL", ran ? "" : " refused: ", ran ? "" : lw.c_str() );
			fails += !ok;
		}
		std::vector<int> redList { 1, 2, 4, 8, 16, 128, 32, 64, 256, 512 };
		if ( a.atlas )
			redList.push_back( 2048 );
		if ( a.allOn )
			redList.insert( redList.end(), { 16384, 32768, 65536 } );
		for ( int red : redList ) {
			if ( red == 512 && ( key != 1 || units[key].ddgi < 0 ) )
				continue;  // the RT GI arm only (the default program has no cellDdgiAtlas)
			redsRun++;
			// lane VKAIR17 red 2048: the app helper's GL-owned branch (the share skipped) bound on volLampAtlas
			GLuint plainAt = 0, plainFbo = 0;
			bool plainOk = false;
			AirAtlasShare plainSh;
			const std::uintptr_t plainKey = reinterpret_cast<std::uintptr_t>( &gAtlasPlainKey );
			if ( red == 2048 ) {
				airAtlasRed() = 1;
				( void )airAtlasEnsure( &gfn, plainKey, 8192, true, plainAt, plainFbo, plainOk, plainSh );
				airAtlasRed() = 0;
				clearAtlas( plainFbo, 0.6 );
				boundAtlas = plainAt;
				std::printf( "AIRTEMPORALVIEW %s red 2048 atlas: shared %d fbo %s\n", an, int( plainSh.shared ), plainOk ? "complete" : "INCOMPLETE" );
			}
			if ( red == 8 )
				fillLamp( in.lampNow, 13 );
			if ( red == 16 )
				fillNoise( in.noise, 2 );
			if ( red == 128 )
				setNoiseMag( in.noise, true );
			GLuint plain = 0;
			if ( red == 256 ) {  // a GL-owned copy of the history in
				glGenTextures( 1, &plain );
				glBindTexture( kAt3d, plain );
				f.texImage3D( kAt3d, 0, GLint( fRgba32f ), kAtCols, kAtRows, ns * kAtSlices, 0, kAtRgba, GL_FLOAT, nullptr );
				glBindTexture( kAt3d, 0 );
				fillHist( plain, ns * kAtSlices, 0 );
			}
			GLuint atlas = 0, atlasBuf = 0;
			if ( red == 512 ) {  // a GL-owned RGBA32F texture buffer bound on cellDdgiAtlas (Buffer<float4>)
				f.genBuffers( 1, &atlasBuf );
				f.bindBuffer( 0x8C2A, atlasBuf );
				f.bufferData( 0x8C2A, 4096 * 16, nullptr, fStatic );
				f.bindBuffer( 0x8C2A, 0 );
				glGenTextures( 1, &atlas );
				glBindTexture( 0x8C2A, atlas );
				ex.texBuffer( 0x8C2A, GLenum( fRgba32f ), atlasBuf );
				glBindTexture( 0x8C2A, 0 );
			}
			const GLuint hist = red == 4 ? in.histB : red == 256 ? plain : in.histA;
			// lane VKAIR18 reds: 16384 the rt buffer red, 32768 the owner stale, 65536 a GL-owned cellRestir with the same bytes
			const GLuint keepDdgi = boundDdgi, keepRestir = boundRestir;
			if ( red == 512 && boundDdgi )
				boundDdgi = 0;	// the red's GL-owned buffer stays on the unit
			if ( red == 16384 )
				_putenv( "WW_VKVIEW_RTBUF_RED=1" );
			if ( red == 32768 )
				gRtFresh = false;
			GLuint plainRs = 0, plainRsBuf = 0;
			if ( red == 65536 ) {
				const std::vector<float> d = rtData( 1, 0 );
				f.genBuffers( 1, &plainRsBuf );
				f.bindBuffer( 0x8C2A, plainRsBuf );
				f.bufferData( 0x8C2A, std::ptrdiff_t( d.size() * 4 ), d.data(), fStatic );
				f.bindBuffer( 0x8C2A, 0 );
				glGenTextures( 1, &plainRs );
				glBindTexture( 0x8C2A, plainRs );
				ex.texBuffer( 0x8C2A, GLenum( fRg32f ), plainRsBuf );
				glBindTexture( 0x8C2A, 0 );
				boundRestir = plainRs;
			}
			std::string rw;
			WwVkPresent::AirTemporalStat rs;
			Pair rv;
			if ( atlas )
				bindUnit( units[key].ddgi, 0x8C2A, atlas );
			const bool ran = vkRun( red == 1 || red == 2 || red == 32 || red == 64 ? red : 0, hist, rw, rs, &rv );
			if ( atlas ) {  // the 64 KB buffer stays (no glDeleteBuffers loaded): one a gate run
				bindUnit( units[key].ddgi, 0x8C2A, 0 );
				glDeleteTextures( 1, &atlas );
			}
			if ( red == 8 )
				fillLamp( in.lampNow, 0 );
			if ( red == 16 )
				fillNoise( in.noise, 1 );
			if ( red == 128 )
				setNoiseMag( in.noise, false );
			if ( plain )
				glDeleteTextures( 1, &plain );
			if ( plainAt ) {
				boundAtlas = atlasTex;
				airAtlasRelease( &gfn, plainKey, plainAt, plainFbo, plainSh );
			}
			boundDdgi = keepDdgi;
			boundRestir = keepRestir;
			if ( red == 16384 )
				_putenv( "WW_VKVIEW_RTBUF_RED=0" );
			if ( red == 32768 )
				gRtFresh = true;
			if ( plainRs )
				glDeleteTextures( 1, &plainRs );	// its buffer lives on (no glDeleteBuffers loaded)
			bool fired = false;
			VolV jv;
			if ( red == 2048 )
				fired = !ran && rw.find( "volLampAtlas" ) != std::string::npos;
			else if ( red == 32 )
				fired = !ran && rw.find( "key" ) != std::string::npos;
			else if ( red == 64 )
				fired = !ran && rw.find( "feedback" ) != std::string::npos;
			else if ( red == 256 )
				fired = !ran && rw.find( "volHist" ) != std::string::npos;
			else if ( red == 512 )
				fired = !ran && rw.find( "cellDdgiAtlas" ) != std::string::npos;
			else if ( red == 16384 )
				fired = !ran && rw.find( "WW_VKVIEW_RTBUF_RED" ) != std::string::npos;
			else if ( red == 32768 )
				fired = !ran && rw.find( "gate: the owner has nothing new" ) != std::string::npos;
			else if ( red == 65536 )
				fired = !ran && rw.find( "cellRestir" ) != std::string::npos;
			else if ( ran ) {
				jv = judge( g, rv );
				fired = !jv.ok();
			}
			std::printf( "AIRTEMPORALVIEW %s red %d: %s (unwritten %d, past 1e-4 rel %d, moved %d)%s%s\n", an, red, fired ? "FAIL (fired)" : "PASS (dead)",
				jv.unwritten, jv.bad, jv.moved, ran ? "" : " refused: ", ran ? "" : rw.c_str() );
			redsFired += fired;
		}
		auto rerun = [&]( const char * what, int wantCopies ) {
			std::string aw;
			WwVkPresent::AirTemporalStat as;
			Pair rv;
			const bool ran = vkRun( 0, in.histA, aw, as, &rv );
			const VolV v = ran ? judge( v0, rv ) : VolV();
			const bool ok = ran && v.ok() && v.diff == 0 && as.copies == wantCopies;
			std::printf( "AIRTEMPORALVIEW %s %s: %s (differ %d, copies %d want %d)%s%s\n", an, what, ok ? "EXACT" : "FAIL", v.diff, as.copies, wantCopies,
				ran ? "" : " refused: ", ran ? "" : aw.c_str() );
			fails += !ok;
		};
		rerun( "rerun after the reds", 0 );
		// the re-ask cycle: the noise GL-owned (one copy a run), then every input released and made shared again
		ensureInputs( in, ns, true, false );
		std::printf( "AIRTEMPORALVIEW %s re-ask: noise shared %d\n", an, int( in.sN.shared ) );
		fails += in.sN.shared;
		rerun( "noise GL-owned", 1 );
		releaseInputs( in );
		ensureInputs( in, ns, true, true );
		fails += !in.sA.shared || !in.sB.shared || !in.sL.shared || !in.sN.shared || !in.sT.shared;
		rerun( "every input made again", 0 );
#else
		( void )vkInj;
		( void )vkOut;
#endif
	}
	// ---- lane VKAIR19: the far grid's stage 1 (cellvolair.cpp vaPassInner's far loop, census row |6), GL's draws vs the far route
	{
		const int fCols = 13, fRows = 9, fSl = 24, fBase = kAtSlices, fDepth = kAtSlices + 24;
		unsigned vkInjF = 0;
		if ( !WwVkPresent::sharedTarget( keyOf( kInjectFar ), 1, kAtCols, kAtRows, fDepth, fRgba32f, vkInjF, why, 1 ) )
			return fatal( "the shared far inject: " + why );
		GLuint glInjF = 0;
		vol( glInjF, fDepth );
		struct FarOpt
		{
			bool stuck = false;
			double start = 200.0;
			int mode = 3;
		};
		// the app's far block (FARGRID1) over stage1's near set: the far loop runs after the near passes
		auto farStage = [&]( int key, const FarOpt & o, std::string * absent ) {
			stage1( progs[key], 0, 6, 0 );
			struct U
			{
				const char * n;
				std::vector<double> v;
				bool need;
			};
			const std::vector<U> us = { { "volFarMode", { double( o.mode ) }, true }, { "volLayerBase", { double( fBase ) }, false },
				{ "volTemporal", { 0 }, true }, { "volHistOk", { 0 }, true }, { "volTerms", { 9 }, true }, { "volLampVol", { 0 }, true },
				{ "volDepthSkip", { 0 }, false }, { "volClusterDims", { 0, 0, 8, 8 }, false }, { "volDrift", { 0.0, 2000.0, 1.0, 0.5 }, true },
				{ "volJitter", { 0, 0, 0, 0 }, false }, { "volDims", { double( fCols ), double( fRows ), double( fSl ) }, true },
				{ "volSlices", { o.start, std::log( 10.0 ), double( fSl ), 1.0 }, true } };
			std::string need;
			for ( const U & u : us )
				if ( !setU( progs[key], u.n, u.v ) ) {
					if ( u.need )
						need += std::string( " " ) + u.n;
					else if ( absent )
						*absent += std::string( " " ) + u.n;
				}
			return need;
		};
		// GL's far stage 1 as the app draws it: one attachment, layer base + k, k in [k0, k1), nothing on volHist
		auto glRunF = [&]( int key, int k0, int k1, std::string & err, const FarOpt & o ) {
			sentinel( glInjF );
			farStage( key, o, nullptr );
			bindInputs( units[key], 0 );
			f.bindFramebuffer( fFb, fb );
			const GLenum b0[1] = { 0x8CE0 };
			f.drawBuffers( 1, b0 );
			drawState0();
			glViewport( 0, 0, fCols, fRows );
			at.bindVertexArray( vao );
			for ( int k = k0; k < k1; k++ ) {
				at.framebufferTextureLayer( fFb, 0x8CE0, glInjF, 0, fBase + k );
				if ( k == k0 && f.checkFramebufferStatus( fFb ) != 0x8CD5 )
					err = "GL's far framebuffer is not complete";
				setU( progs[key], "volLayer", { o.stuck ? 0.0 : double( k ) } );
				glDrawElements( GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, nullptr );
			}
			at.framebufferTextureLayer( fFb, 0x8CE0, 0, 0, 0 );
			f.bindFramebuffer( fFb, 0 );
			unbindInputs( units[key] );
			glFinish();
			if ( const GLenum e = glGetError() )
				err = "GL error " + std::to_string( e ) + " in GL's far stage 1";
			return readVol( glInjF, fDepth );
		};
		for ( int key = 0; key < 2; key++ ) {
			std::string absent;
			const std::string need = farStage( key, FarOpt(), &absent );
			std::printf( "AIRTEMPORALVIEW far %s: optional far uniforms GL's program lacks:%s\n", keyName[key], absent.empty() ? " none" : absent.c_str() );
			if ( !need.empty() )
				return fatal( std::string( "far " ) + keyName[key] + ": GL's program has no uniform" + need );
		}
		ensureInputs( in, 6, true, true );
		struct FarArm
		{
			int key;
			bool allOn;
			int k0, k1;
		};
		const FarArm farArms[4] = { { 0, false, 0, fSl }, { 0, false, 5, 17 }, { 1, true, 0, fSl }, { 1, true, 5, 17 } };
		for ( const FarArm & a : farArms ) {
			const int key = a.key, k0 = a.k0, k1 = a.k1;
			const bool whole = k0 == 0 && k1 == fSl;
			const std::string arm = std::string( a.allOn ? "far all-on" : "far def" ) + " layers " + std::to_string( k0 ) + ".." + std::to_string( k1 );
			const char * an = arm.c_str();
			gAtlasOn = a.allOn ? 1 : 0;
			boundAtlas = a.allOn ? atlasTex : 0;
			boundDdgi = a.allOn ? rtTex[0] : 0;
			boundRestir = a.allOn ? rtTex[1] : 0;
			std::string err;
			const std::vector<float> g = glRunF( key, k0, k1, err, FarOpt() );
			if ( !err.empty() )
				return fatal( arm + ": " + err );
			{
				const FarV self = judgeFar( g, g, fCols, fRows, fBase + k0, fBase + k1 );
				const bool ok = self.ok() && self.lit * 2 >= self.run;
				std::printf( "AIRTEMPORALVIEW %s GL written: run %d texels, unwritten %d, non-finite %d, outside the run not the sentinel %d, lit %d "
							 "(want >= half) %s\n",
					an, self.run, self.unwritten, self.nonFinite, self.stray, self.lit, ok ? "OK" : "FAIL" );
				fails += !ok;
			}
			auto probe = [&]( const char * what, const std::vector<float> & other, bool bar ) {
				const FarV p = judgeFar( g, other, fCols, fRows, fBase + k0, fBase + k1 );
				const bool live = p.moved >= kAtLive;
				std::printf( "AIRTEMPORALVIEW %s probe %s: moved %d of %d %s\n", an, what, p.moved, p.run,
					bar ? ( live ? "LIVE" : "TESTS NOTHING" ) : "(informative)" );
				probesDead += bar && !live;
			};
			if ( whole ) {
				FarOpt o;
				o.stuck = true;
				probe( "volLayer 0 on every draw", glRunF( key, k0, k1, err, o ), true );
				fillTerr( in.terr, 2 );
				probe( "airTerr rewritten", glRunF( key, k0, k1, err, FarOpt() ), true );
				fillTerr( in.terr, 1 );
				o = FarOpt();
				o.start = 400.0;
				probe( "volSlices start doubled", glRunF( key, k0, k1, err, o ), true );
				o = FarOpt();
				o.mode = 0;
				probe( "volFarMode 0 (airFarVis off)", glRunF( key, k0, k1, err, o ), false );
				if ( !err.empty() )
					return fatal( arm + " (probes): " + err );
			}
#ifdef WW_VK_AIRFAR
			auto vkRunF = [&]( int red, std::string & w, WwVkPresent::AirTemporalStat & st, std::vector<float> * res ) {
				sentinel( vkInjF );
				farStage( key, FarOpt(), nullptr );
				bindInputs( units[key], ( red & 64 ) ? vkInjF : 0 );
				( void )WwVkPresent::takeAirFarStat();
				WwVkPresent::AirTemporalRun r;
				r.prog = progs[key];
				r.rt = key == 1;
				r.wwDefs = WwVkPresent::airTemporalKey( ( key == 1 ) != ( ( red & 32 ) != 0 ) );
				r.cols = fCols;
				r.rows = fRows;
				r.slices = fSl;
				r.strata = 0;
				r.injectKey = keyOf( kInjectFar );
				r.histOutKey = 0;
				r.far = true;
				r.base = fBase;
				r.k0 = k0;
				r.k1 = k1;
				r.red = red & 13;
				const bool ok = WwVkPresent::airTemporalRun( r, gw, w );
				st = WwVkPresent::takeAirFarStat();
				unbindInputs( units[key] );
				if ( ok && res )
					*res = readVol( vkInjF, fDepth );
				return ok;
			};
			std::string w;
			WwVkPresent::AirTemporalStat st;
			std::vector<float> v0;
			if ( !vkRunF( 0, w, st, &v0 ) ) {
				std::printf( "AIRTEMPORALVIEW %s: Vulkan refused: %s\n", an, w.c_str() );
				fails++;
				continue;
			}
			{
				const std::vector<float> again = readVol( vkInjF, fDepth );
				const FarV twice = judgeFar( v0, again, fCols, fRows, fBase + k0, fBase + k1 );
				const bool same = twice.ok() && twice.diff == 0;
				std::printf( "AIRTEMPORALVIEW %s read twice: %s (differ %d, unwritten %d)\n", an, same ? "EXACT" : "FAIL", twice.diff, twice.unwritten );
				fails += !same;
			}
			{
				const FarV v = judgeFar( g, v0, fCols, fRows, fBase + k0, fBase + k1 );
				std::printf( "AIRTEMPORALVIEW %s Vulkan vs GL: %s (run %d, unwritten %d, non-finite %d, outside the run not the sentinel %d, differ %d, "
							 "past 1e-4 rel %d, max rel %.3g)\n",
					an, v.word(), v.run, v.unwritten, v.nonFinite, v.stray, v.diff, v.bad, v.maxRel );
				fails += !v.ok();
				const bool cp = st.runs == 1 && st.draws == k1 - k0 && st.copies == 0;
				std::printf( "AIRTEMPORALVIEW %s stat: runs %d draws %d copies %d buffer copies %d (want 1 run, %d draws, 0 copies) %s\n", an, st.runs,
					st.draws, st.copies, st.bufCopies, k1 - k0, cp ? "OK" : "FAIL" );
				fails += !cp;
			}
			if ( a.allOn ) {
				const WwVkPresent::RtBufStat s0 = WwVkPresent::rtBufStat();
				std::string lw;
				WwVkPresent::AirTemporalStat ls;
				const bool ran = vkRunF( 0, lw, ls, nullptr );
				const WwVkPresent::RtBufStat s1 = WwVkPresent::rtBufStat();
				const unsigned long long da = s1.lookups[0] - s0.lookups[0], dr = s1.lookups[1] - s0.lookups[1];
				const bool ok = ran && da == 1 && dr == 1;
				std::printf( "AIRTEMPORALVIEW %s rtBufOf lookups a run: cellDdgiAtlas %llu cellRestir %llu (want 1 and 1) %s%s%s\n", an, da, dr,
					ok ? "OK" : "FAIL", ran ? "" : " refused: ", ran ? "" : lw.c_str() );
				fails += !ok;
			}
			std::vector<int> redList { 1, 4, 16, 32, 64 };
			if ( !whole )
				redList.push_back( 8 );	// the run's bounds ignored: only a spread run can show it
			if ( a.allOn )
				redList.insert( redList.end(), { 16384, 65536 } );
			for ( int red : redList ) {
				redsRun++;
				if ( red == 16 )
					fillTerr( in.terr, 2 );
				if ( red == 16384 )
					_putenv( "WW_VKVIEW_RTBUF_RED=1" );
				const GLuint keepRestir = boundRestir;
				GLuint plainRs = 0, plainRsBuf = 0;
				if ( red == 65536 ) {
					const std::vector<float> d = rtData( 1, 0 );
					f.genBuffers( 1, &plainRsBuf );
					f.bindBuffer( 0x8C2A, plainRsBuf );
					f.bufferData( 0x8C2A, std::ptrdiff_t( d.size() * 4 ), d.data(), fStatic );
					f.bindBuffer( 0x8C2A, 0 );
					glGenTextures( 1, &plainRs );
					glBindTexture( 0x8C2A, plainRs );
					ex.texBuffer( 0x8C2A, GLenum( fRg32f ), plainRsBuf );
					glBindTexture( 0x8C2A, 0 );
					boundRestir = plainRs;
				}
				std::string rw;
				WwVkPresent::AirTemporalStat rs;
				std::vector<float> rv;
				const bool ran = vkRunF( red == 1 || red == 4 || red == 8 || red == 32 || red == 64 ? red : 0, rw, rs, &rv );
				if ( red == 16 )
					fillTerr( in.terr, 1 );
				if ( red == 16384 )
					_putenv( "WW_VKVIEW_RTBUF_RED=0" );
				boundRestir = keepRestir;
				if ( plainRs )
					glDeleteTextures( 1, &plainRs );	// its buffer lives on (no glDeleteBuffers loaded)
				bool fired = false;
				FarV jv;
				if ( red == 32 )
					fired = !ran && rw.find( "key" ) != std::string::npos;
				else if ( red == 64 )
					fired = !ran && rw.find( "feedback" ) != std::string::npos;
				else if ( red == 16384 )
					fired = !ran && rw.find( "WW_VKVIEW_RTBUF_RED" ) != std::string::npos;
				else if ( red == 65536 )
					fired = !ran && rw.find( "cellRestir" ) != std::string::npos;
				else if ( ran ) {
					jv = judgeFar( g, rv, fCols, fRows, fBase + k0, fBase + k1 );
					fired = !jv.ok();
				}
				std::printf( "AIRTEMPORALVIEW %s red %d: %s (unwritten %d, outside the run %d, past 1e-4 rel %d, moved %d)%s%s\n", an, red,
					fired ? "FAIL (fired)" : "PASS (dead)", jv.unwritten, jv.stray, jv.bad, jv.moved, ran ? "" : " refused: ", ran ? "" : rw.c_str() );
				redsFired += fired;
			}
			{
				std::string aw;
				WwVkPresent::AirTemporalStat as;
				std::vector<float> rv;
				const bool ran = vkRunF( 0, aw, as, &rv );
				const FarV v = ran ? judgeFar( v0, rv, fCols, fRows, fBase + k0, fBase + k1 ) : FarV();
				const bool ok = ran && v.ok() && v.diff == 0 && as.copies == 0;
				std::printf( "AIRTEMPORALVIEW %s rerun after the reds: %s (differ %d, copies %d want 0)%s%s\n", an, ok ? "EXACT" : "FAIL", v.diff, as.copies,
					ran ? "" : " refused: ", ran ? "" : aw.c_str() );
				fails += !ok;
			}
#else
			( void )vkInjF;
			std::printf( "AIRTEMPORALVIEW %s: no route (WW_VK_AIRFAR not defined)\n", an );
			fails++;
#endif
		}
		gAtlasOn = 0;
		boundAtlas = boundDdgi = boundRestir = 0;
	}
	// ---- lane VKAIR21: the near grid's stage 1 with temporal off (cellvolair.cpp vaPassInner's last else, census row |5)
	{
		auto plainStage = [&]( int key ) {
			stage1( progs[key], 0, 6, 0 );
			setU( progs[key], "volHistOk", { 0 } );	// the app: temporal && g.histOk
		};
		// GL's stage 1 as the app's last else draws it: one attachment, layer k of the inject, k in [0, 64), nothing on volHist
		auto glRunP = [&]( int key, std::string & err, bool stuck ) {
			sentinel( glInj );
			plainStage( key );
			bindInputs( units[key], 0 );
			f.bindFramebuffer( fFb, fb );
			const GLenum b0[1] = { 0x8CE0 };
			f.drawBuffers( 1, b0 );
			drawState0();
			glViewport( 0, 0, kAtCols, kAtRows );
			at.bindVertexArray( vao );
			for ( int k = 0; k < kAtSlices; k++ ) {
				at.framebufferTextureLayer( fFb, 0x8CE0, glInj, 0, k );
				if ( k == 0 && f.checkFramebufferStatus( fFb ) != 0x8CD5 )
					err = "GL's plain framebuffer is not complete";
				setU( progs[key], "volLayer", { stuck ? 0.0 : double( k ) } );
				glDrawElements( GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, nullptr );
			}
			at.framebufferTextureLayer( fFb, 0x8CE0, 0, 0, 0 );
			f.bindFramebuffer( fFb, 0 );
			unbindInputs( units[key] );
			glFinish();
			if ( const GLenum e = glGetError() )
				err = "GL error " + std::to_string( e ) + " in GL's plain stage 1";
			return readVol( glInj, kAtSlices );
		};
		ensureInputs( in, 6, true, true );
		for ( int key = 0; key < 2; key++ ) {
			const bool allOn = key == 1;
			const std::string arm = allOn ? "plain all-on" : "plain def";
			const char * an = arm.c_str();
			gAtlasOn = allOn ? 1 : 0;
			boundAtlas = allOn ? atlasTex : 0;
			boundDdgi = allOn ? rtTex[0] : 0;
			boundRestir = allOn ? rtTex[1] : 0;
			std::string err;
			const std::vector<float> g = glRunP( key, err, false );
			if ( !err.empty() )
				return fatal( arm + ": " + err );
			{
				const FarV self = judgeFar( g, g, kAtCols, kAtRows, 0, kAtSlices );
				const bool ok = self.ok() && self.lit * 2 >= self.run;
				std::printf( "AIRTEMPORALVIEW %s GL written: run %d texels, unwritten %d, non-finite %d, lit %d (want >= half) %s\n", an, self.run,
					self.unwritten, self.nonFinite, self.lit, ok ? "OK" : "FAIL" );
				fails += !ok;
			}
			auto probe = [&]( const char * what, const std::vector<float> & other ) {
				const FarV p = judgeFar( g, other, kAtCols, kAtRows, 0, kAtSlices );
				const bool live = p.moved >= kAtLive;
				std::printf( "AIRTEMPORALVIEW %s probe %s: moved %d of %d %s\n", an, what, p.moved, p.run, live ? "LIVE" : "TESTS NOTHING" );
				probesDead += !live;
			};
			probe( "volLayer 0 on every draw", glRunP( key, err, true ) );
			fillTerr( in.terr, 2 );
			probe( "airTerr rewritten", glRunP( key, err, false ) );
			fillTerr( in.terr, 1 );
			fillLamp( in.lampNow, 13 );
			probe( "volLampNow rewritten", glRunP( key, err, false ) );
			fillLamp( in.lampNow, 0 );
			fillNoise( in.noise, 2 );
			probe( "the noise rewritten", glRunP( key, err, false ) );
			fillNoise( in.noise, 1 );
			if ( !err.empty() )
				return fatal( arm + " (probes): " + err );
#ifdef WW_VK_AIRPLAIN
			auto vkRunP = [&]( int red, std::string & w, WwVkPresent::AirTemporalStat & st, std::vector<float> * res, int * others ) {
				sentinel( vkInj );
				plainStage( key );
				bindInputs( units[key], ( red & 64 ) ? vkInj : 0 );
				if ( red & 4 )
					bindUnit( units[key].lampNow, kAt3d, in.histA );	// a wrong input: the history on volLampNow
				( void )WwVkPresent::takeAirPlainStat();
				( void )WwVkPresent::takeAirTemporalStat();
				( void )WwVkPresent::takeAirFarStat();
				WwVkPresent::AirTemporalRun r;
				r.prog = progs[key];
				r.rt = key == 1;
				r.wwDefs = WwVkPresent::airTemporalKey( ( key == 1 ) != ( ( red & 32 ) != 0 ) );
				r.cols = kAtCols;
				r.rows = kAtRows;
				r.slices = kAtSlices;
				r.strata = 0;
				r.injectKey = keyOf( kInject );
				r.histOutKey = 0;
				r.far = true;
				r.plain = true;
				r.base = 0;
				r.k0 = 0;
				r.k1 = kAtSlices;
				r.red = red & 1;
				const bool ok = ( red & 2 ) ? true : WwVkPresent::airTemporalRun( r, gw, w );	// red 2: the route dropped
				st = WwVkPresent::takeAirPlainStat();
				if ( others )
					*others = WwVkPresent::takeAirTemporalStat().runs + WwVkPresent::takeAirFarStat().runs;
				unbindInputs( units[key] );
				if ( ok && res )
					*res = readVol( vkInj, kAtSlices );
				return ok;
			};
			std::string w;
			WwVkPresent::AirTemporalStat st;
			std::vector<float> v0;
			int others = -1;
			if ( !vkRunP( 0, w, st, &v0, &others ) ) {
				std::printf( "AIRTEMPORALVIEW %s: Vulkan refused: %s\n", an, w.c_str() );
				fails++;
				continue;
			}
			{
				const std::vector<float> again = readVol( vkInj, kAtSlices );
				const FarV twice = judgeFar( v0, again, kAtCols, kAtRows, 0, kAtSlices );
				const bool same = twice.ok() && twice.diff == 0;
				std::printf( "AIRTEMPORALVIEW %s read twice: %s (differ %d, unwritten %d)\n", an, same ? "EXACT" : "FAIL", twice.diff, twice.unwritten );
				fails += !same;
			}
			{
				const FarV v = judgeFar( g, v0, kAtCols, kAtRows, 0, kAtSlices );
				std::printf( "AIRTEMPORALVIEW %s Vulkan vs GL: %s (run %d, unwritten %d, non-finite %d, differ %d, past 1e-4 rel %d, max rel %.3g)\n", an,
					v.word(), v.run, v.unwritten, v.nonFinite, v.diff, v.bad, v.maxRel );
				fails += !v.ok();
				const bool cp = st.runs == 1 && st.draws == kAtSlices && st.copies == 0 && others == 0;
				std::printf( "AIRTEMPORALVIEW %s stat: runs %d draws %d copies %d buffer copies %d, reader + far runs %d (want 1 run, %d draws, 0 copies, "
							 "0 other runs) %s\n",
					an, st.runs, st.draws, st.copies, st.bufCopies, others, kAtSlices, cp ? "OK" : "FAIL" );
				fails += !cp;
			}
			if ( allOn ) {
				const WwVkPresent::RtBufStat s0 = WwVkPresent::rtBufStat();
				std::string lw;
				WwVkPresent::AirTemporalStat ls;
				const bool ran = vkRunP( 0, lw, ls, nullptr, nullptr );
				const WwVkPresent::RtBufStat s1 = WwVkPresent::rtBufStat();
				const unsigned long long da = s1.lookups[0] - s0.lookups[0], dr = s1.lookups[1] - s0.lookups[1];
				const bool ok = ran && da == 1 && dr == 1;
				std::printf( "AIRTEMPORALVIEW %s rtBufOf lookups a run: cellDdgiAtlas %llu cellRestir %llu (want 1 and 1) %s%s%s\n", an, da, dr,
					ok ? "OK" : "FAIL", ran ? "" : " refused: ", ran ? "" : lw.c_str() );
				fails += !ok;
			}
			std::vector<int> redList { 1, 2, 4, 8, 16, 128, 32, 64 };
			if ( allOn )
				redList.insert( redList.end(), { 16384, 65536 } );
			for ( int red : redList ) {
				redsRun++;
				if ( red == 8 )
					fillLamp( in.lampNow, 13 );
				if ( red == 16 )
					fillTerr( in.terr, 2 );
				if ( red == 128 )
					fillNoise( in.noise, 2 );
				if ( red == 16384 )
					_putenv( "WW_VKVIEW_RTBUF_RED=1" );
				const GLuint keepRestir = boundRestir;
				GLuint plainRs = 0, plainRsBuf = 0;
				if ( red == 65536 ) {
					const std::vector<float> d = rtData( 1, 0 );
					f.genBuffers( 1, &plainRsBuf );
					f.bindBuffer( 0x8C2A, plainRsBuf );
					f.bufferData( 0x8C2A, std::ptrdiff_t( d.size() * 4 ), d.data(), fStatic );
					f.bindBuffer( 0x8C2A, 0 );
					glGenTextures( 1, &plainRs );
					glBindTexture( 0x8C2A, plainRs );
					ex.texBuffer( 0x8C2A, GLenum( fRg32f ), plainRsBuf );
					glBindTexture( 0x8C2A, 0 );
					boundRestir = plainRs;
				}
				std::string rw;
				WwVkPresent::AirTemporalStat rs;
				std::vector<float> rv;
				const bool ran = vkRunP( red <= 4 || red == 32 || red == 64 ? red : 0, rw, rs, &rv, nullptr );
				if ( red == 8 )
					fillLamp( in.lampNow, 0 );
				if ( red == 16 )
					fillTerr( in.terr, 1 );
				if ( red == 128 )
					fillNoise( in.noise, 1 );
				if ( red == 16384 )
					_putenv( "WW_VKVIEW_RTBUF_RED=0" );
				boundRestir = keepRestir;
				if ( plainRs )
					glDeleteTextures( 1, &plainRs );	// its buffer lives on (no glDeleteBuffers loaded)
				bool fired = false;
				FarV jv;
				if ( red == 32 )
					fired = !ran && rw.find( "key" ) != std::string::npos;
				else if ( red == 64 )
					fired = !ran && rw.find( "feedback" ) != std::string::npos;
				else if ( red == 16384 )
					fired = !ran && rw.find( "WW_VKVIEW_RTBUF_RED" ) != std::string::npos;
				else if ( red == 65536 )
					fired = !ran && rw.find( "cellRestir" ) != std::string::npos;
				else if ( red == 4 && !ran )
					fired = rw.find( "volLampNow" ) != std::string::npos;
				else if ( ran ) {
					jv = judgeFar( g, rv, kAtCols, kAtRows, 0, kAtSlices );
					fired = !jv.ok();
				}
				std::printf( "AIRTEMPORALVIEW %s red %d: %s (unwritten %d, past 1e-4 rel %d, moved %d)%s%s\n", an, red, fired ? "FAIL (fired)" : "PASS (dead)",
					jv.unwritten, jv.bad, jv.moved, ran ? "" : " refused: ", ran ? "" : rw.c_str() );
				redsFired += fired;
			}
			{
				std::string aw;
				WwVkPresent::AirTemporalStat as;
				std::vector<float> rv;
				const bool ran = vkRunP( 0, aw, as, &rv, nullptr );
				const FarV v = ran ? judgeFar( v0, rv, kAtCols, kAtRows, 0, kAtSlices ) : FarV();
				const bool ok = ran && v.ok() && v.diff == 0 && as.copies == 0;
				std::printf( "AIRTEMPORALVIEW %s rerun after the reds: %s (differ %d, copies %d want 0)%s%s\n", an, ok ? "EXACT" : "FAIL", v.diff, as.copies,
					ran ? "" : " refused: ", ran ? "" : aw.c_str() );
				fails += !ok;
			}
#else
			std::printf( "AIRTEMPORALVIEW %s: no route (WW_VK_AIRPLAIN not defined)\n", an );
			fails++;
#endif
		}
		gAtlasOn = 0;
		boundAtlas = boundDdgi = boundRestir = 0;
	}
#ifdef WW_VK_AIRTEMPORAL
	for ( int s2 = 0; s2 < 2; s2++ ) {	// lane VKAIR18: the gate's owner gone
		WwVkPresent::rtBufOwner( s2, nullptr );
		WwVkPresent::rtBufDrop( s2 );
	}
#endif
	std::printf( "AIRTEMPORALVIEW GL: %d fails, %d probes TESTS NOTHING\n", fails, probesDead );
#ifdef WW_VK_AIRTEMPORAL
	const bool ok = fails == 0 && probesDead == 0 && redsRun > 0 && redsFired == redsRun;
	std::printf( "AIRTEMPORALVIEW %d fails, %d dead probes, reds %d of %d fired\n", fails, probesDead, redsFired, redsRun );
	std::printf( "AIRTEMPORALVIEW GATE %s\n", ok ? "OK" : "FAILED" );
	return ok ? 0 : 1;
#else
	( void )redsRun;
	( void )redsFired;
	std::printf( "AIRTEMPORALVIEW GATE FAILED (no route: WW_VK_AIRTEMPORAL not defined; GL coverage and probes above)\n" );
	return 1;
#endif
}
