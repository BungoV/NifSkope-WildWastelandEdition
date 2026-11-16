// Lane VKAIR15 (2026-11-16): the air's stage 4, the lamp volume's walk (cellvolair.cpp vaPassInner, census row |2: 64 draws,
// draw k writes lvHist[out] layer k (RGBA16F) at attachment 0 and lvNow[out] layer k (RGBA32F) at attachment 1, MRT 2) on the
// present device: vkairlamp.inc airLampRun draws the 64 layers in one crossing into the SHARED lvHist / lvNow outs with the
// airinject twins of cell_volair (volStage 4), reading every input from GL's units (shared ones in place, the light list
// copied a run). GL's side is GL's own cell_volair program (airinjectview_prep.py, both pixel-stage keys) into GL-owned twins
// of both outs through glFramebufferTextureLayer, the app's state, every loose uniform seeded by farloddraw's seedUnis
// (profile fogOn), every block zero, then the stage's own (stage4() below).
// Inputs, each made by the app's helper (cellvolairshare.h vaShareEnsure, the app's specs) as a shared target: lvNow / lvHist
// in (the history GL wrote as frame 1 of the same world, shared slot 0), the blue-noise mask (R8 2D array 128 x 128 x 64,
// nearest repeat, glTexSubImage3D as the app's fill), the light list (a GL RGBA32F texture buffer, 2 omni lamps in the grid).
// Fixture: 21 x 12 x 64, identity reprojection with last frame's grid 0.99 narrower in x and y (VKAIR16: with the same grid every
// tap sat on a texel centre, so the edge columns' bounds reject rode on the drivers' rounding of a ~1e-6 weight: 46 texels a
// frame-2 arm kept on one side, rejected on the other, N 4 vs 5; now the edge taps sit 0.05..0.1 of a texel inside; in depth the
// same 0.99 about the middle slice: last frame's near 0.5 exp(-0.32 lnr / 0.99 / 64), its log span lnr / 0.99, so layer 0's tap
// sits 0.315 of a slice inside, layer 63's 0.315 below the top; with x and y alone layer 0 still flipped 4 texels an arm). Arms: def frame 1 (volLampHistOk 0: no
// history, four samples), def frame 2 (volLampHistOk 1: history kept where the hash matches), rt frame 2.
// Bar (set before the first run, never widened):
//   GL writes every texel of both outs (no sentinel 7 left), all finite; frame 1 lit (lvNow rgb > 0) >= 50 texels; frame 2
//   kept (lvHist N > 4.5: frame 1 writes N 4, a kept texel N 4 + 1) >= 50 texels (the history really taken);
//   Vulkan vs GL, every texel: written, finite; lvHist (16F) within 1 half step a value; lvNow rgb |vk - gl| <= 1e-4
//   max(1, |gl|); lvNow alpha (the light-set hash) EXACT;
//   the route's stat: 1 run, 64 draws, image copies 0 (every image input shared);
//   read twice EXACT; a rerun after the reds EXACT against the first Vulkan run.
// GL probes (moved = texels past the bar against GL's own run; LIVE >= 50, else TESTS NOTHING and the gate fails): volLayer 0 on
// every draw, the mask rewritten, the lamps moved, volFrame moved; frame 2 also: lvHist in rewritten, lvNow in rewritten.
// Reds, each must FAIL: route 1 volLayer stuck at 0; route 2 the out slices one late; gate 4 the mask rewritten after GL's draw;
// gate 1024 the lamps moved after GL's draw (the list is read this run); frame 2: gate 8 lvHist in rewritten after GL's draw,
// gate 16 lvNow in rewritten after GL's draw; refusals: gate 32 the other key's wwDefs, named "key"; gate 64 lvHist out key =
// lvHist in's, named "feedback"; gate 128 a GL-owned copy of the mask on volStbn, named "volStbn"; gate 256 (frame 2) lvHist in
// unbound while volLampHistOk (GL's unbound sampler reads (0,0,0,1), Vulkan's zero view 0: the N > 0.5 tap would part), named
// "volLampHist"; gate 512 (rt only) a GL-owned RGBA32F texture buffer on cellDdgiAtlas, named "cellDdgiAtlas".
// Coverage limit: the app's own s4Share branches (cellvolair.cpp) run only in the app; this gate drives the helper they call.
// Lane VKAIR17 (2026-11-16), arm "rt frame 2 atlas" (appended after the three arms above; their lines stay byte-equal; the RT
// GI key, because only WW_AIR_LEAK = WW_CELL_FOGLEAK && WW_CELL_RTDDGI samples the atlas: run 1 found the def program without
// it, so setupProg now FAILS if the def key samples volLampAtlas / volLampTable or the rt key does not): the
// lamps' depth atlas (airlampatlas.cpp) made by the app's helper (airlampatlasshare.h airAtlasEnsure, wished: a shared D16 2D
// target) at the app's size (8192), its tiles filled by scissored depth clears through the helper's FBO (GL's own writes, as the
// app's tile clears), the table (4 RGBA32F texels a lamp: face origins, tile size, ready) a GL texture buffer; volLampVis 1,
// volLampN 2, volLampAtlasRed 0 (the fixture lamps carry the shadow flag and no cube slot: every lamp reads the atlas).
// Bar (set before the first run, never widened): make check: shared, FBO complete, farSharedOf(atlas) == the helper's key, size
// 8192, and the fill read back through the FBO within 1 D16 unit of the cleared depth (0 misses over a tile); probes (LIVE >= 50
// moved): "atlas off" (volLampVis 0), "atlas rewritten", "table rewritten"; Vulkan vs GL the same bar as the other arms; stat: 1 run, 64 draws,
// image copies 0 (the atlas read in place), buffer copies 2 (the light list and the table). Reds, each must FAIL: gate 2048 a
// GL-owned atlas (the helper's red 1: the share skipped) bound, named "volLampAtlas"; gate 4096 the atlas rewritten after GL's
// draw; gate 8192 the table rewritten after GL's draw (tile size 32: read each run). Plus every red of the arms above.
// Lane VKAIR18 (2026-11-16), arm "rt frame 2 atlas all-on" (appended last; the arms above stay byte-equal): the all-on route
// (main.cpp kAllOn: RTDDGI 1, RT ultra, so RESTIR on) binds the RT GI's texel buffers, cellDdgiAtlas (RGBA32F) and cellRestir
// (RG32F), shared by name (vkrtbuf.h rtBufShare + rtBufOwner, as rtddgicell.cpp shares them), plus the atlas of the arm above.
// Bar (set before the first run, never widened): the rt key's program samples cellRestir (a unit), Vulkan vs GL and the stat
// as the atlas arm; one run asks rtBufOf once a name (rtBufStat lookups: cellDdgiAtlas +1, cellRestir +1). Reds, each must
// FAIL: gate 16384 WW_VKVIEW_RTBUF_RED=1 (refusal names "WW_VKVIEW_RTBUF_RED"); gate 32768 the owner's fresh() says false
// (refusal names its why); gate 65536 a GL-owned RG32F buffer with the same bytes on cellRestir while the slot lives (named
// "cellRestir"); plus every red of the atlas arm (512: a GL-owned buffer on cellDdgiAtlas). Content: cellRestir is read only
// under volLeakRed & 4 (cell_lights.hlsli), so the shipped uniforms gate the binding and the refusals, not its values; the
// "RT GI inputs rewritten" probe line is informative, not a bar. airlampview.sh greps both routes for the rtBufOf call.
// Last line `AIRLAMPVIEW GATE OK` or `AIRLAMPVIEW GATE FAILED`.
#define FLD_NO_MAIN
#include "farloddraw.cpp"
#undef FLD_NO_MAIN
#include "features/air/cellvolairshare.h"
#include "features/air/airlampatlasshare.h"

namespace alv
{
using namespace fv;
using namespace fld;

const int kLvCols = 21, kLvRows = 12, kLvSlices = 64, kLvLive = 50, kLvMask = 128, kLvMaskD = 64, kLvLights = 2, kLvTpl = 8;
const float kLvSentinel = 7.0f;
const std::uint16_t kLvSentinelH = 0x4700;	// 7.0 in half
bool gRtOn = false;	// lane VKAIR18: the all-on arm (its RT GI inputs bound; unbound after each draw)
bool gRtFresh = true;	// lane VKAIR18: the gate owner's fresh() answer (red 32768: false)
int gAtlasOn = 0;	// lane VKAIR17: 0: stage4 leaves the atlas uniforms alone (the old arms); 1: volLampVis 1; -1: volLampVis 0
const GLenum kLvRgba = 0x1908, kLv3d = 0x806F, kLv2da = 0x8C1A, kLvUbyte = 0x1401, kLvHalf = 0x140B, kLvTb = 0x8C2A;

struct At
{
	void( WINAPI * vertexAttribPointer )( GLuint, GLint, GLenum, GLboolean, GLsizei, const void * ) = nullptr;
	void( WINAPI * enableVertexAttribArray )( GLuint ) = nullptr;
	void( WINAPI * genVertexArrays )( GLsizei, GLuint * ) = nullptr;
	void( WINAPI * bindVertexArray )( GLuint ) = nullptr;
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
		&& load( at.clearTexImage, "glClearTexImage" ) && load( at.clearTexSubImage, "glClearTexSubImage" )
		&& load( at.framebufferTextureLayer, "glFramebufferTextureLayer" ) && load( at.framebufferTexture2D, "glFramebufferTexture2D" )
		&& load( at.deleteFramebuffers, "glDeleteFramebuffers" );
}

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
	// lane VKAIR17: the atlas helper's calls (airlampatlasshare.h)
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

enum Slot { kHist0, kHist1, kNow0, kNow1, kStbn, kSlots };
char gKeys[kSlots];
std::uintptr_t keyOf( int s )
{
	return reinterpret_cast<std::uintptr_t>( &gKeys[s] );
}

// the app's specs (cellvolair.cpp: hs / ns of the lamp volume, ss of the mask)
const VaShareSpec kHistSpec { 1, fRgba16f, kLvRgba, GL_FLOAT, kLvCols, kLvRows, kLvSlices, false, false };
const VaShareSpec kNowSpec { 1, fRgba32f, kLvRgba, GL_FLOAT, kLvCols, kLvRows, kLvSlices, false, false };
const VaShareSpec kStbnSpec { 2, fR8, fRed, kLvUbyte, kLvMask, kLvMask, kLvMaskD, false, true };

struct Inputs
{
	GLuint hist[2] = {}, now[2] = {}, stbn = 0;
	VaShareState sH[2], sN[2], sS;
};

std::vector<std::uint8_t> maskBytes( unsigned seed )
{
	std::mt19937 rng( seed * 977u + 5u );
	std::vector<std::uint8_t> d( static_cast<size_t>( kLvMask ) * kLvMask * kLvMaskD );
	for ( std::uint8_t & x : d )
		x = std::uint8_t( rng() & 255u );
	return d;
}

void fillMask( GLuint t, unsigned seed )
{
	const std::vector<std::uint8_t> d = maskBytes( seed );
	glBindTexture( kLv2da, t );
	f.texSubImage3D( kLv2da, 0, 0, 0, 0, kLvMask, kLvMask, kLvMaskD, fRed, kLvUbyte, d.data() );
	glBindTexture( kLv2da, 0 );
}

bool ensureInputs( Inputs & in )
{
	for ( int i = 0; i < 2; i++ ) {
		( void )vaShareEnsure( &gfn, keyOf( kHist0 + i ), kHistSpec, true, in.hist[i], in.sH[i] );
		( void )vaShareEnsure( &gfn, keyOf( kNow0 + i ), kNowSpec, true, in.now[i], in.sN[i] );
	}
	if ( vaShareEnsure( &gfn, keyOf( kStbn ), kStbnSpec, true, in.stbn, in.sS ) )
		fillMask( in.stbn, 1 );
	return in.sH[0].shared && in.sH[1].shared && in.sN[0].shared && in.sN[1].shared && in.sS.shared;
}

void clearVol( GLuint t, float a )
{
	for ( int z = 0; z < kLvSlices; z++ ) {
		const float v[4] = { 0.3f + 0.01f * float( z % 17 ), 0.2f + 0.013f * float( z % 11 ), 0.1f + 0.02f * float( z % 7 ), a };
		at.clearTexSubImage( t, 0, 0, 0, z, kLvCols, kLvRows, 1, kLvRgba, GL_FLOAT, v );
	}
}

void sentinel( GLuint t )
{
	const float s[4] = { kLvSentinel, kLvSentinel, kLvSentinel, kLvSentinel };
	at.clearTexImage( t, 0, kLvRgba, GL_FLOAT, s );
}

std::vector<float> readF( GLuint t )
{
	std::vector<float> v( static_cast<size_t>( kLvCols ) * kLvRows * kLvSlices * 4 );
	glBindTexture( kLv3d, t );
	glGetTexImage( kLv3d, 0, kLvRgba, GL_FLOAT, v.data() );
	glBindTexture( kLv3d, 0 );
	return v;
}

std::vector<std::uint16_t> readH( GLuint t )
{
	std::vector<std::uint16_t> v( static_cast<size_t>( kLvCols ) * kLvRows * kLvSlices * 4 );
	glBindTexture( kLv3d, t );
	glGetTexImage( kLv3d, 0, kLvRgba, kLvHalf, v.data() );
	glBindTexture( kLv3d, 0 );
	return v;
}

struct Pair
{
	std::vector<std::uint16_t> hist;
	std::vector<float> now;
};

float lvHalf( std::uint16_t h )
{
	const int e = ( h >> 10 ) & 31, m = h & 1023;
	const float s = ( h & 0x8000 ) ? -1.0f : 1.0f;
	if ( e == 31 )
		return m ? std::nanf( "" ) : s * INFINITY;
	return e == 0 ? s * std::ldexp( float( m ), -24 ) : s * std::ldexp( float( m | 1024 ), e - 25 );
}

int hOrd( std::uint16_t h )
{
	return ( h & 0x8000 ) ? -int( h & 0x7FFF ) : int( h );
}

struct VolV
{
	int texels = 0, unwritten = 0, nonFinite = 0, diff = 0, bad = 0, moved = 0, hashBad = 0, maxSteps = 0;
	double maxRel = 0.0;
	bool ok() const { return texels > 0 && unwritten == 0 && nonFinite == 0 && bad == 0 && hashBad == 0; }
	const char * word() const { return !ok() ? "FAIL" : diff == 0 ? "EXACT" : "BOUND"; }
};

VolV judge( const Pair & gl, const Pair & vk )
{
	VolV v;
	if ( gl.hist.size() != vk.hist.size() || gl.now.size() != vk.now.size() || gl.hist.size() != gl.now.size() ) {
		v.bad++;
		return v;
	}
	for ( size_t t = 0; t < gl.now.size() / 4; t++ ) {
		v.texels++;
		bool sg = true, sk = true, sgn = true, skn = true;
		for ( int c = 0; c < 4; c++ ) {
			sg = sg && gl.hist[4 * t + size_t( c )] == kLvSentinelH;
			sk = sk && vk.hist[4 * t + size_t( c )] == kLvSentinelH;
			sgn = sgn && gl.now[4 * t + size_t( c )] == kLvSentinel;
			skn = skn && vk.now[4 * t + size_t( c )] == kLvSentinel;
		}
		bool m = sg || sk || sgn || skn;
		v.unwritten += m;
		for ( int c = 0; c < 4; c++ ) {	// lvHist: half steps
			const std::uint16_t g = gl.hist[4 * t + size_t( c )], k = vk.hist[4 * t + size_t( c )];
			if ( !std::isfinite( lvHalf( g ) ) || !std::isfinite( lvHalf( k ) ) ) {
				v.nonFinite++;
				m = true;
				continue;
			}
			const int st = std::abs( hOrd( g ) - hOrd( k ) );
			v.maxSteps = std::max( v.maxSteps, st );
			v.diff += g != k;
			v.bad += st > 1;
			m = m || st > 1;
		}
		for ( int c = 0; c < 4; c++ ) {	// lvNow: rgb 1e-4 rel, alpha (the hash) EXACT
			const double g = gl.now[4 * t + size_t( c )], k = vk.now[4 * t + size_t( c )];
			if ( !std::isfinite( g ) || !std::isfinite( k ) ) {
				v.nonFinite++;
				m = true;
				continue;
			}
			v.diff += k != g;
			if ( c == 3 ) {
				v.hashBad += k != g;
				m = m || k != g;
				continue;
			}
			const double rel = std::fabs( k - g ) / std::max( 1.0, std::fabs( g ) );
			v.maxRel = std::max( v.maxRel, rel );
			v.bad += rel > 1e-4;
			m = m || rel > 1e-4;
		}
		v.moved += m;
	}
	return v;
}

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
		vals.resize( static_cast<size_t>( cnt ) * static_cast<size_t>( comp ), 0.0 );
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

//! the stage's own uniforms (cellvolair.cpp vaPassInner, stage 4); "" or the first required one GL's program lacks
std::string stage4( GLuint p, int histOk, int layer, int frame, std::string * absent = nullptr )
{
	struct U
	{
		const char * n;
		std::vector<double> v;
		bool need;
	};
	const double lnr = std::log( 400.0 );
	const std::vector<U> us = {
		{ "volStage", { 4 }, true }, { "volLayer", { double( layer ) }, true }, { "volDims", { kLvCols, kLvRows, kLvSlices }, true },
		{ "volLampHistOk", { double( histOk ) }, true }, { "volLampA", { 0.1 }, true }, { "volFrame", { double( frame ) }, true },
		{ "cellOn", { 1 }, true }, { "volTerms", { 15 }, true }, { "volRed", { 0 }, false }, { "volLampRed", { 0 }, false },
		{ "volDepthSkip", { 0 }, false }, { "cellLightCount", { kLvLights }, true }, { "cellBulbCount", { 0 }, false },
		{ "cellBulbBase", { 0 }, false }, { "volClusterDims", { 0, 0, 1, 1 }, false }, { "volSurfClusters", { 0 }, false },
		{ "volLightShift", { 0, 0, 0 }, false }, { "volMsLamp", { 0, 0, 0, 0 }, false }, { "volGndLamp", { 0, 0, 0, 0 }, false },
		{ "volPhase", { 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0 }, false }, { "airMed", { 0.002, 0.3, 0.5, 2.0 }, false },
		{ "fogView", { 0, 0, 0, 0 }, false }, { "volProj", { 1.2, 2.1 }, true }, { "volSlices", { 0.5, lnr, kLvSlices, 1.0 }, true },
		{ "volReproj", { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0 }, true }, { "volPrevGrid", { 1.188, 2.079, 0.5 * std::exp( -0.32 * lnr / 0.99 / 64.0 ), lnr / 0.99 }, true },
		{ "volPrevScale", { 1.0 }, true }, { "fogDistScale", { 1.0 }, true }, { "cellRow", { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0 }, false } };
	for ( const U & u : us )
		if ( !setU( p, u.n, u.v ) ) {
			if ( u.need )
				return u.n;
			if ( absent )
				*absent += std::string( " " ) + u.n;
		}
	if ( gAtlasOn != 0 ) {	// lane VKAIR17: the atlas arm
		for ( const U & u : std::vector<U> { { "volLampVis", { gAtlasOn > 0 ? 1.0 : 0.0 }, true }, { "volLampN", { double( kLvLights ) }, true },
				  { "volLampAtlasRed", { 0 }, true } } )
			if ( !setU( p, u.n, u.v ) )
				return u.n;
	}
	return "";
}

struct Units
{
	int now = -1, hist = -1, stbn = -1, lights = -1, ddgi = -1, atlas = -1, table = -1, restir = -1;
};

std::string setupProg( GLuint p, const char * tag, Units & u )
{
	GLint nb = 0;
	f.getProgramiv( p, 0x8A36 /* GL_ACTIVE_UNIFORM_BLOCKS */, &nb );
	std::vector<GLuint> ubo( static_cast<size_t>( std::max( nb, 1 ) ) );
	f.genBuffers( GLsizei( ubo.size() ), ubo.data() );
	for ( GLint b = 0; b < nb; b++ ) {
		GLint bytes = 16;
		f.getActiveUniformBlockiv( p, GLuint( b ), fBlockSize, &bytes );
		f.uniformBlockBinding( p, GLuint( b ), GLuint( b ) );
		const std::vector<std::uint8_t> z( static_cast<size_t>( std::max( bytes, 16 ) ), 0 );
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
		std::vector<GLint> us( static_cast<size_t>( std::max( sz, 1 ) ) );
		for ( GLint & x : us )
			x = unit++;
		const std::string n( nm );
		int * slot = n == "volLampNow" ? &u.now : n == "volLampHist" ? &u.hist : n == "volStbn" ? &u.stbn : n == "cellLights" ? &u.lights
			: n == "cellDdgiAtlas" ? &u.ddgi : n == "volLampAtlas" ? &u.atlas : n == "volLampTable" ? &u.table
			: n == "cellRestir" ? &u.restir : nullptr;
		if ( slot )
			*slot = us[0];
		f.uniform1iv( f.getUniformLocation( p, nm ), GLsizei( us.size() ), us.data() );
		samplers += int( us.size() );
	}
	std::printf( "AIRLAMPVIEW %s: %d blocks zero, %d loose uniforms seeded, %d samplers on units 1..%d (volLampNow %d volLampHist %d volStbn %d "
				 "cellLights %d cellDdgiAtlas %d)\n",
		tag, nb, seeded, samplers, unit - 1, u.now, u.hist, u.stbn, u.lights, u.ddgi );
	std::printf( "AIRLAMPVIEW %s: volLampAtlas %d volLampTable %d\n", tag, u.atlas, u.table );
	std::printf( "AIRLAMPVIEW %s: cellRestir %d\n", tag, u.restir );
	if ( u.now < 0 || u.hist < 0 || u.stbn < 0 || u.lights < 0 )
		return "no volLampNow / volLampHist / volStbn / cellLights sampler";
	// WW_AIR_LEAK = WW_CELL_FOGLEAK && WW_CELL_RTDDGI: only the RT GI key samples the atlas; the default key must not
	const bool rtKey = std::string( tag ) == "rt";
	if ( rtKey && ( u.atlas < 0 || u.table < 0 ) )
		return "no volLampAtlas / volLampTable sampler";
	if ( !rtKey && ( u.atlas >= 0 || u.table >= 0 ) )
		return "the default key samples volLampAtlas / volLampTable (the census's default column assumes it does not)";
	if ( unit > 160 )
		return "more sampler units than the gate binds";
	return "";
}

//! the lamps: 2 omni (t4 = (-1, 1): no cube slot), world = view here (cellRow identity, fogDistScale 1); salt moves them
std::vector<float> lampTexels( int salt )
{
	std::vector<float> L( static_cast<size_t>( kLvLights ) * kLvTpl * 4, 0.0f );
	const float pos[2][4] = { { 2.0f + 3.0f * salt, -1.0f, -40.0f - 5.0f * salt, 60.0f }, { 15.0f, 8.0f - 2.0f * salt, -150.0f, 120.0f } };
	const float col[2][3] = { { 1.0f, 0.8f, 0.6f }, { 0.4f, 0.6f, 1.0f } };
	for ( int i = 0; i < kLvLights; i++ ) {
		float * t = &L[static_cast<size_t>( i ) * kLvTpl * 4];
		for ( int c = 0; c < 4; c++ )
			t[c] = pos[i][c];
		t[4] = col[i][0];
		t[5] = col[i][1];
		t[6] = col[i][2];
		t[7] = -2.0f;
		t[10] = -1.0f;
		t[11] = 1.0f;
		t[16] = -1.0f;
		t[17] = 1.0f;
	}
	return L;
}

// lane VKAIR17: the atlas arm. The app's size; tiles of kAtTile px, lamp i face f at ((6 i + f + 1) kAtTile, kAtTile)
const int kAtSize = 8192, kAtTile = 64, kAtBlock = 8;
char gAtlasKey, gAtlasPlainKey;

float atDepth( int i, int fc, int bx, int by, int pattern )
{
	static const float lv[4] = { 0.12f, 0.35f, 0.6f, 1.0f };
	const unsigned h = unsigned( i * 131 + fc * 31 + bx * 7 + by * 13 + pattern * 5 ) * 2654435761u;
	return lv[( h >> 13 ) & 3u];
}

//! the tiles by scissored depth clears through the atlas's FBO (GL's own writes, as the app's tile clears)
void fillAtlas( GLuint fbo, int pattern )
{
	f.bindFramebuffer( fFb, fbo );
	glDepthMask( GL_TRUE );
	glDisable( GL_SCISSOR_TEST );
	glClearDepth( 1.0 );
	glClear( GL_DEPTH_BUFFER_BIT );
	glEnable( GL_SCISSOR_TEST );
	for ( int i = 0; i < kLvLights; i++ )
		for ( int fc = 0; fc < 6; fc++ )
			for ( int by = 0; by < kAtTile / kAtBlock; by++ )
				for ( int bx = 0; bx < kAtTile / kAtBlock; bx++ ) {
					glScissor( ( 6 * i + fc + 1 ) * kAtTile + bx * kAtBlock, kAtTile + by * kAtBlock, kAtBlock, kAtBlock );
					glClearDepth( atDepth( i, fc, bx, by, pattern ) );
					glClear( GL_DEPTH_BUFFER_BIT );
				}
	glDisable( GL_SCISSOR_TEST );
	glClearDepth( 1.0 );
	f.bindFramebuffer( fFb, 0 );
	glFinish();
}

//! lamp 0 face 0's tile read back through the FBO: texels more than 1 D16 unit off the cleared depth
int atlasMisses( GLuint fbo, int pattern )
{
	std::vector<std::uint16_t> px( static_cast<size_t>( kAtTile ) * kAtTile );
	f.bindFramebuffer( 0x8CA8 /* GL_READ_FRAMEBUFFER */, fbo );
	glReadPixels( kAtTile, kAtTile, kAtTile, kAtTile, GL_DEPTH_COMPONENT, GL_UNSIGNED_SHORT, px.data() );
	f.bindFramebuffer( 0x8CA8, 0 );
	int miss = 0;
	for ( int y = 0; y < kAtTile; y++ )
		for ( int x = 0; x < kAtTile; x++ ) {
			const int want = int( std::lround( atDepth( 0, 0, x / kAtBlock, y / kAtBlock, pattern ) * 65535.0 ) );
			miss += std::abs( int( px[static_cast<size_t>( y ) * kAtTile + x] ) - want ) > 1;
		}
	return miss;
}

//! the table (airlampatlas.cpp's layout: texels 0..2 the face origins xy / zw, texel 3 (tile size, ready))
std::vector<float> atlasTable( int tile )
{
	std::vector<float> t( static_cast<size_t>( kLvLights ) * 16, 0.0f );
	for ( int i = 0; i < kLvLights; i++ ) {
		float * q = &t[static_cast<size_t>( i ) * 16];
		for ( int fc = 0; fc < 6; fc++ ) {
			q[( fc >> 1 ) * 4 + ( fc & 1 ) * 2] = float( ( 6 * i + fc + 1 ) * kAtTile );
			q[( fc >> 1 ) * 4 + ( fc & 1 ) * 2 + 1] = float( kAtTile );
		}
		q[12] = float( tile );
		q[13] = 1.0f;
	}
	return t;
}
}  // namespace alv

int main( int argc, char ** argv )
{
	using namespace fv;
	using namespace fld;
	using namespace alv;
	gTag = "AIRLAMPVIEW";
	if ( argc < 2 ) {
		std::printf( "usage: airlampview <prep dir>\nAIRLAMPVIEW GATE FAILED\n" );
		return 2;
	}
	const std::string dir = argv[1];
	auto fatal = [&]( const std::string & why ) {
		std::printf( "AIRLAMPVIEW GATE FAILED (%s)\n", why.c_str() );
		return 2;
	};
	const int S = 4, W = 256, H = 160;
	gOpenGl = LoadLibraryA( "opengl32.dll" );
	WNDCLASSA wc {};
	wc.style = CS_OWNDC;
	wc.lpfnWndProc = wndProc;
	wc.hInstance = GetModuleHandleA( nullptr );
	wc.lpszClassName = "wwairlampview";
	RegisterClassA( &wc );
	HWND wnd = CreateWindowExA( WS_EX_NOACTIVATE, "wwairlampview", "air lamp harness", WS_OVERLAPPEDWINDOW, 40, 40, 320, 200, nullptr, nullptr,
		wc.hInstance, nullptr );
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
	std::printf( "AIRLAMPVIEW GL on %s\n", reinterpret_cast<const char *>( glGetString( GL_RENDERER ) ) );
	glPixelStorei( GL_PACK_ALIGNMENT, 1 );
	glPixelStorei( GL_UNPACK_ALIGNMENT, 1 );
	std::string why;
	if ( !WwVkPresent::viewBegin( W, H, S, glProc, why ) )
		return fatal( "the view: " + why );

	Inputs in;
	if ( !ensureInputs( in ) )
		return fatal( "an input was not made shared" );
	GLuint glHist = 0, glNow = 0;
	auto vol = [&]( GLuint & t, GLenum fmt ) {
		glGenTextures( 1, &t );
		glBindTexture( kLv3d, t );
		f.texImage3D( kLv3d, 0, GLint( fmt ), kLvCols, kLvRows, kLvSlices, 0, kLvRgba, GL_FLOAT, nullptr );
		glTexParameteri( kLv3d, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
		glTexParameteri( kLv3d, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
		glBindTexture( kLv3d, 0 );
	};
	vol( glHist, fRgba16f );
	vol( glNow, fRgba32f );
	// the light list: a GL RGBA32F texture buffer (the app's cellLights: GL's own)
	GLuint lightBuf = 0, lightTex = 0;
	f.genBuffers( 1, &lightBuf );
	auto setLamps = [&]( int salt ) {
		const std::vector<float> L = lampTexels( salt );
		f.bindBuffer( kLvTb, lightBuf );
		f.bufferData( kLvTb, std::ptrdiff_t( L.size() * 4 ), L.data(), fStatic );
		f.bindBuffer( kLvTb, 0 );
	};
	setLamps( 0 );
	glGenTextures( 1, &lightTex );
	glBindTexture( kLvTb, lightTex );
	ex.texBuffer( kLvTb, GLenum( fRgba32f ), lightBuf );
	glBindTexture( kLvTb, 0 );
	// lane VKAIR17: the atlas by the app's helper (wished: shared) and its table (a GL texture buffer)
	GLuint atlasTex = 0, atlasFbo = 0, tableBuf = 0, tableTex = 0;
	bool atlasFboOk = false;
	AirAtlasShare atlasSh;
	const std::uintptr_t atlasKeyV = reinterpret_cast<std::uintptr_t>( &gAtlasKey );
	{
		const int made = airAtlasEnsure( &gfn, atlasKeyV, kAtSize, true, atlasTex, atlasFbo, atlasFboOk, atlasSh );
		const int again = airAtlasEnsure( &gfn, atlasKeyV, kAtSize, true, atlasTex, atlasFbo, atlasFboOk, atlasSh );
		fillAtlas( atlasFbo, 1 );
		const int miss = atlasMisses( atlasFbo, 1 );
		const bool found = farSharedOf( atlasTex ) == atlasKeyV;
		const bool ok = made == 1 && again == 0 && atlasSh.shared && atlasFboOk && found && atlasSh.size == kAtSize && miss == 0;
		std::printf( "AIRLAMPVIEW atlas make: made %d again %d shared %d fbo %s farSharedOf %s size %d fill misses %d: %s\n", made, again,
			int( atlasSh.shared ), atlasFboOk ? "complete" : "INCOMPLETE", found ? "== key" : "NOT the key", atlasSh.size, miss, ok ? "OK" : "FAIL" );
		std::printf( "AIRLAMPVIEW %s\n", airAtlasSay( kAtSize, true, atlasFboOk, atlasSh ).c_str() );
		if ( !ok )
			return fatal( "the atlas make check" );
	}
	f.genBuffers( 1, &tableBuf );
	auto setTable = [&]( int tile ) {
		const std::vector<float> t = atlasTable( tile );
		f.bindBuffer( kLvTb, tableBuf );
		f.bufferData( kLvTb, std::ptrdiff_t( t.size() * 4 ), t.data(), fStatic );
		f.bindBuffer( kLvTb, 0 );
	};
	setTable( kAtTile );
	glGenTextures( 1, &tableTex );
	glBindTexture( kLvTb, tableTex );
	ex.texBuffer( kLvTb, GLenum( fRgba32f ), tableBuf );
	glBindTexture( kLvTb, 0 );
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
		const std::string m = stage4( progs[key], 1, 0, 5, &absent );
		if ( !m.empty() )
			return fatal( std::string( keyName[key] ) + ": GL's program has no uniform " + m );
		std::printf( "AIRLAMPVIEW %s: optional stage uniforms GL's program lacks:%s\n", keyName[key], absent.empty() ? " none" : absent.c_str() );
	}

	struct Bound
	{
		GLuint now, hist, stbn;
		GLuint atlas = 0;	// lane VKAIR17: the atlas arm only (then the table too)
		GLuint ddgi = 0, restir = 0;	// lane VKAIR18: the all-on arm's RT GI inputs (the shared buffers' GL views)
	};
	auto bindInputs = [&]( const Units & u, const Bound & b ) {
		bindUnit( u.now, kLv3d, b.now );
		bindUnit( u.hist, kLv3d, b.hist );
		bindUnit( u.stbn, kLv2da, b.stbn );
		bindUnit( u.lights, kLvTb, lightTex );
		if ( b.atlas ) {
			bindUnit( u.atlas, GL_TEXTURE_2D, b.atlas );
			bindUnit( u.table, kLvTb, tableTex );
		}
		if ( b.ddgi )
			bindUnit( u.ddgi, kLvTb, b.ddgi );
		if ( b.restir )
			bindUnit( u.restir, kLvTb, b.restir );
	};
	auto unbindInputs = [&]( const Units & u ) {
		bindUnit( u.now, kLv3d, 0 );
		bindUnit( u.hist, kLv3d, 0 );
		bindUnit( u.stbn, kLv2da, 0 );
		bindUnit( u.lights, kLvTb, 0 );
		if ( gAtlasOn != 0 && u.atlas >= 0 ) {
			bindUnit( u.atlas, GL_TEXTURE_2D, 0 );
			bindUnit( u.table, kLvTb, 0 );
		}
		if ( gRtOn ) {
			bindUnit( u.ddgi, kLvTb, 0 );
			bindUnit( u.restir, kLvTb, 0 );
		}
	};
	// GL's stage 4 as vaPassInner draws it (64 layers, MRT 2) into outHist / outNow (the sentinel first)
	auto glDraw = [&]( int key, int histOk, int frame, GLuint outHist, GLuint outNow, const Bound & b, std::string & err, bool layerStuck = false ) {
		sentinel( outHist );
		sentinel( outNow );
		stage4( progs[key], histOk, 0, frame );
		bindInputs( units[key], b );
		f.bindFramebuffer( fFb, fb );
		const GLenum bufs[2] = { 0x8CE0, 0x8CE1 };
		f.drawBuffers( 2, bufs );
		drawState0();
		glViewport( 0, 0, kLvCols, kLvRows );
		at.bindVertexArray( vao );
		for ( int k = 0; k < kLvSlices; k++ ) {
			at.framebufferTextureLayer( fFb, 0x8CE0, outHist, 0, k );
			at.framebufferTextureLayer( fFb, 0x8CE1, outNow, 0, k );
			if ( k == 0 && f.checkFramebufferStatus( fFb ) != 0x8CD5 )
				err = "GL's stage-4 framebuffer is not complete";
			setU( progs[key], "volLayer", { layerStuck ? 0.0 : double( k ) } );
			glDrawElements( GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, nullptr );
		}
		for ( int p = 0; p < 2; p++ )
			at.framebufferTextureLayer( fFb, GLenum( 0x8CE0 + p ), 0, 0, 0 );
		f.bindFramebuffer( fFb, 0 );
		unbindInputs( units[key] );
		glFinish();
		if ( const GLenum e = glGetError() )
			err = "GL error " + std::to_string( e ) + " in GL's stage 4";
		return Pair { readH( outHist ), readF( outNow ) };
	};
	// frame 1 of the same world into the shared slot 0 (the history every frame-2 arm reads); slot 1's inputs cleared
	auto makeHistory = [&]( std::string & err ) {
		clearVol( in.hist[1], 0.0f );
		clearVol( in.now[1], 0.0f );
		( void )glDraw( 0, 0, 4, in.hist[0], in.now[0], Bound { in.now[1], in.hist[1], in.stbn }, err );
	};
	std::string err;
	makeHistory( err );
	if ( !err.empty() )
		return fatal( "the history: " + err );

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
			f.bindBuffer( kLvTb, rtSrc[s2] );
			f.bufferData( kLvTb, std::ptrdiff_t( d.size() * 4 ), d.data(), fStatic );
			f.bindBuffer( kLvTb, 0 );
			std::string w;
			if ( !WwVkPresent::rtBufCopy( s2, rtSrc[s2], d.size() * 4, w ) )
				std::printf( "AIRLAMPVIEW rtBufCopy %s: %s\n", WwVkPresent::rtBufName( s2 ), w.c_str() );
		}
	};
#ifdef WW_VK_AIRLAMP
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
		std::printf( "AIRLAMPVIEW all-on inputs shared: cellDdgiAtlas buf %u tex %u, cellRestir buf %u tex %u, fill GL error 0x%04X\n", rtBuf[0],
			rtTex[0], rtBuf[1], rtTex[1], unsigned( fe ) );
		if ( fe )
			return fatal( "the all-on inputs' fill" );
	}
#endif
	struct Arm
	{
		int key, histOk;
		bool atlas = false;	// lane VKAIR17: lamps' shadows from the shared atlas (volLampVis 1), its own history
		bool allOn = false;	// lane VKAIR18: the all-on route's RT GI inputs bound too (cellDdgiAtlas, cellRestir: shared by name)
	};
	const Arm arms[5] = { { 0, 0 }, { 0, 1 }, { 1, 1 }, { 1, 1, true }, { 1, 1, true, true } };	// the atlas arm on the RT GI key (the only one that samples it)
	int fails = 0, probesDead = 0, redsRun = 0, redsFired = 0;
	for ( const Arm & a : arms ) {
		const int key = a.key, hk = a.histOk;
		const std::string arm = std::string( keyName[key] ) + " frame " + ( hk ? "2" : "1" ) + ( a.atlas ? " atlas" : "" ) + ( a.allOn ? " all-on" : "" );
		const char * an = arm.c_str();
		if ( a.atlas )
			gAtlasOn = 1;
		gRtOn = a.allOn;
		const Bound b0 = [&] {
			Bound x { in.now[0], in.hist[0], in.stbn, a.atlas ? atlasTex : 0u };
			if ( a.allOn ) {
				x.ddgi = rtTex[0];
				x.restir = rtTex[1];
			}
			return x;
		}();
		if ( a.allOn ) {
			const bool unitsOk = units[key].ddgi >= 0 && units[key].restir >= 0 && rtTex[0] && rtTex[1];
			std::printf( "AIRLAMPVIEW %s units: cellDdgiAtlas %d cellRestir %d, shared views %u %u %s\n", an, units[key].ddgi, units[key].restir,
				rtTex[0], rtTex[1], unitsOk ? "OK" : "FAIL" );
			fails += !unitsOk;
		}
		const Pair g = glDraw( key, hk, 5, glHist, glNow, b0, err );
		if ( !err.empty() )
			return fatal( arm + ": " + err );
		{
			const VolV self = judge( g, g );
			int lit = 0, kept = 0;
			for ( size_t t = 0; t < g.now.size() / 4; t++ ) {
				lit += g.now[4 * t] > 0.0f || g.now[4 * t + 1] > 0.0f || g.now[4 * t + 2] > 0.0f;
				kept += lvHalf( g.hist[4 * t + 3] ) > 4.5f;
			}
			const bool live = lit >= kLvLive && ( !hk || kept >= kLvLive );
			std::printf( "AIRLAMPVIEW %s GL written: unwritten %d non-finite %d of %d texels; lit %d, history kept %d %s\n", an, self.unwritten,
				self.nonFinite, self.texels, lit, kept, live ? "LIVE" : "TESTS NOTHING" );
			fails += self.unwritten != 0 || self.nonFinite != 0 || self.texels == 0;
			probesDead += !live;
		}
		auto probe = [&]( const char * what, const Pair & other ) {
			const VolV p = judge( g, other );
			const bool live = p.moved >= kLvLive;
			std::printf( "AIRLAMPVIEW %s probe %s: moved %d of %d %s\n", an, what, p.moved, p.texels, live ? "LIVE" : "TESTS NOTHING" );
			probesDead += !live;
		};
		probe( "volLayer 0 on every draw", glDraw( key, hk, 5, glHist, glNow, b0, err, true ) );
		fillMask( in.stbn, 2 );
		probe( "mask rewritten", glDraw( key, hk, 5, glHist, glNow, b0, err ) );
		fillMask( in.stbn, 1 );
		setLamps( 1 );
		probe( "lamps moved", glDraw( key, hk, 5, glHist, glNow, b0, err ) );
		setLamps( 0 );
		probe( "volFrame moved", glDraw( key, hk, 6, glHist, glNow, b0, err ) );
		if ( hk ) {
			clearVol( in.hist[0], 3.0f );
			probe( "lvHist in rewritten", glDraw( key, hk, 5, glHist, glNow, b0, err ) );
			makeHistory( err );
			clearVol( in.now[0], 0.0f );
			probe( "lvNow in rewritten", glDraw( key, hk, 5, glHist, glNow, b0, err ) );
			makeHistory( err );
		}
		if ( a.atlas ) {
			gAtlasOn = -1;
			probe( "atlas off", glDraw( key, hk, 5, glHist, glNow, b0, err ) );
			gAtlasOn = 1;
			fillAtlas( atlasFbo, 2 );
			probe( "atlas rewritten", glDraw( key, hk, 5, glHist, glNow, b0, err ) );
			fillAtlas( atlasFbo, 1 );
			setTable( kAtTile / 2 );
			probe( "table rewritten", glDraw( key, hk, 5, glHist, glNow, b0, err ) );
			setTable( kAtTile );
		}
		if ( a.allOn ) {	// informative, not a bar: stage 4 reads cellRestir only under volLeakRed & 4
			fillRt( 1 );
			const VolV p = judge( g, glDraw( key, hk, 5, glHist, glNow, b0, err ) );
			std::printf( "AIRLAMPVIEW %s RT GI inputs rewritten: moved %d of %d (informative)\n", an, p.moved, p.texels );
			fillRt( 0 );
		}
		if ( !err.empty() )
			return fatal( arm + " (probes): " + err );

#ifdef WW_VK_AIRLAMP
		auto vkRun = [&]( int red, const Bound & b, std::string & w, WwVkPresent::AirLampStat & st, Pair * res ) {
			sentinel( in.hist[1] );
			sentinel( in.now[1] );
			stage4( progs[key], hk, 0, 5 );
			bindInputs( units[key], b );
			( void )WwVkPresent::takeAirLampStat();
			WwVkPresent::AirLampRun r;
			r.prog = progs[key];
			r.rt = key == 1;
			r.wwDefs = WwVkPresent::airLampKey( ( key == 1 ) != ( ( red & 32 ) != 0 ) );
			r.cols = kLvCols;
			r.rows = kLvRows;
			r.slices = kLvSlices;
			r.histOutKey = ( red & 64 ) ? keyOf( kHist0 ) : keyOf( kHist1 );
			r.nowOutKey = keyOf( kNow1 );
			r.red = red & 3;
			const bool ok = WwVkPresent::airLampRun( r, gw, w );
			st = WwVkPresent::takeAirLampStat();
			unbindInputs( units[key] );
			if ( ok && res )
				*res = Pair { readH( in.hist[1] ), readF( in.now[1] ) };
			return ok;
		};
		std::string w;
		WwVkPresent::AirLampStat st;
		Pair v0;
		if ( !vkRun( 0, b0, w, st, &v0 ) ) {
			std::printf( "AIRLAMPVIEW %s: Vulkan refused: %s\n", an, w.c_str() );
			fails++;
			continue;
		}
		{
			const Pair again { readH( in.hist[1] ), readF( in.now[1] ) };
			const VolV twice = judge( v0, again );
			const bool same = twice.ok() && twice.diff == 0;
			std::printf( "AIRLAMPVIEW %s read twice: %s (differ %d, unwritten %d)\n", an, same ? "EXACT" : "FAIL", twice.diff, twice.unwritten );
			fails += !same;
		}
		{
			const VolV v = judge( g, v0 );
			std::printf( "AIRLAMPVIEW %s Vulkan vs GL: %s (texels %d, unwritten %d, non-finite %d, differ %d, past the bar %d, hash differ %d, max rel "
						 "%.3g, max half steps %d)\n",
				an, v.word(), v.texels, v.unwritten, v.nonFinite, v.diff, v.bad, v.hashBad, v.maxRel, v.maxSteps );
			fails += !v.ok();
			const bool cp = st.runs == 1 && st.draws == kLvSlices && st.copies == 0;
			std::printf( "AIRLAMPVIEW %s stat: runs %d draws %d copies %d buffer copies %d (want 1 run, %d draws, 0 copies) %s\n", an, st.runs, st.draws,
				st.copies, st.bufCopies, kLvSlices, cp ? "OK" : "FAIL" );
			fails += !cp;
			if ( a.atlas ) {
				const bool bc = st.bufCopies == 2;
				std::printf( "AIRLAMPVIEW %s atlas stat: buffer copies %d (want 2: the lights and the table; the atlas read in place) %s\n", an,
					st.bufCopies, bc ? "OK" : "FAIL" );
				fails += !bc;
			}
		}
		if ( a.allOn ) {	// one run asks the shared buffers by name once a name
			const WwVkPresent::RtBufStat s0 = WwVkPresent::rtBufStat();
			std::string lw;
			WwVkPresent::AirLampStat ls;
			Pair lv;
			const bool ran = vkRun( 0, b0, lw, ls, &lv );
			const WwVkPresent::RtBufStat s1 = WwVkPresent::rtBufStat();
			const unsigned long long da = s1.lookups[0] - s0.lookups[0], dr = s1.lookups[1] - s0.lookups[1];
			const bool ok = ran && da == 1 && dr == 1;
			std::printf( "AIRLAMPVIEW %s rtBufOf lookups a run: cellDdgiAtlas %llu cellRestir %llu (want 1 and 1) %s%s%s\n", an, da, dr,
				ok ? "OK" : "FAIL", ran ? "" : " refused: ", ran ? "" : lw.c_str() );
			fails += !ok;
		}
		std::vector<int> redList { 1, 2, 4, 1024, 8, 16, 32, 64, 128, 256, 512 };
		if ( a.atlas )
			redList.insert( redList.end(), { 2048, 4096, 8192 } );
		if ( a.allOn )
			redList.insert( redList.end(), { 16384, 32768, 65536 } );
		for ( int red : redList ) {
			if ( ( red == 8 || red == 16 || red == 256 ) && !hk )
				continue;	// frame 2 only: frame 1 reads no history
			if ( red == 512 && ( key != 1 || units[key].ddgi < 0 ) )
				continue;	// the RT GI arm only
			redsRun++;
			Bound b = b0;
			if ( red == 4 )
				fillMask( in.stbn, 2 );
			if ( red == 1024 )
				setLamps( 1 );
			if ( red == 8 )
				clearVol( in.hist[0], 3.0f );
			if ( red == 16 )
				clearVol( in.now[0], 0.0f );
			GLuint plain = 0;
			if ( red == 128 ) {	// a GL-owned copy of the mask
				const std::vector<std::uint8_t> d = maskBytes( 1 );
				glGenTextures( 1, &plain );
				glBindTexture( kLv2da, plain );
				f.texImage3D( kLv2da, 0, GLint( fR8 ), kLvMask, kLvMask, kLvMaskD, 0, fRed, kLvUbyte, d.data() );
				glTexParameteri( kLv2da, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
				glTexParameteri( kLv2da, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
				glBindTexture( kLv2da, 0 );
				b.stbn = plain;
			}
			if ( red == 256 )
				b.hist = 0;
			GLuint atlas = 0, atlasBuf = 0;
			if ( red == 512 ) {
				f.genBuffers( 1, &atlasBuf );
				f.bindBuffer( kLvTb, atlasBuf );
				f.bufferData( kLvTb, 4096 * 16, nullptr, fStatic );
				f.bindBuffer( kLvTb, 0 );
				glGenTextures( 1, &atlas );
				glBindTexture( kLvTb, atlas );
				ex.texBuffer( kLvTb, GLenum( fRgba32f ), atlasBuf );
				glBindTexture( kLvTb, 0 );
				bindUnit( units[key].ddgi, kLvTb, atlas );
			}
			// lane VKAIR17 reds: 2048 the app helper's GL-owned branch (the share skipped), 4096 the atlas and 8192 the table
			// rewritten after GL's draw
			GLuint plainAt = 0, plainFbo = 0;
			bool plainOk = false;
			AirAtlasShare plainSh;
			const std::uintptr_t plainKey = reinterpret_cast<std::uintptr_t>( &gAtlasPlainKey );
			if ( red == 2048 ) {
				airAtlasRed() = 1;
				( void )airAtlasEnsure( &gfn, plainKey, kAtSize, true, plainAt, plainFbo, plainOk, plainSh );
				airAtlasRed() = 0;
				fillAtlas( plainFbo, 1 );
				b.atlas = plainAt;
				std::printf( "AIRLAMPVIEW %s red 2048 atlas: shared %d fbo %s\n", an, int( plainSh.shared ), plainOk ? "complete" : "INCOMPLETE" );
			}
			if ( red == 4096 )
				fillAtlas( atlasFbo, 2 );
			if ( red == 8192 )
				setTable( kAtTile / 2 );
			// lane VKAIR18 reds: 16384 the rt buffer red, 32768 the owner stale, 65536 a GL-owned cellRestir with the same bytes
			if ( red == 512 )
				b.ddgi = 0;	// the red's GL-owned buffer stays on the unit
			if ( red == 16384 )
				_putenv( "WW_VKVIEW_RTBUF_RED=1" );
			if ( red == 32768 )
				gRtFresh = false;
			GLuint plainRs = 0, plainRsBuf = 0;
			if ( red == 65536 ) {
				const std::vector<float> d = rtData( 1, 0 );
				f.genBuffers( 1, &plainRsBuf );
				f.bindBuffer( kLvTb, plainRsBuf );
				f.bufferData( kLvTb, std::ptrdiff_t( d.size() * 4 ), d.data(), fStatic );
				f.bindBuffer( kLvTb, 0 );
				glGenTextures( 1, &plainRs );
				glBindTexture( kLvTb, plainRs );
				ex.texBuffer( kLvTb, GLenum( fRg32f ), plainRsBuf );
				glBindTexture( kLvTb, 0 );
				b.restir = plainRs;
			}
			std::string rw;
			WwVkPresent::AirLampStat rs;
			Pair rv;
			const bool ran = vkRun( red == 1 || red == 2 || red == 32 || red == 64 ? red : 0, b, rw, rs, &rv );
			if ( atlas ) {
				bindUnit( units[key].ddgi, kLvTb, 0 );
				glDeleteTextures( 1, &atlas );
			}
			if ( red == 4 )
				fillMask( in.stbn, 1 );
			if ( red == 1024 )
				setLamps( 0 );
			if ( red == 8 || red == 16 )
				makeHistory( err );
			if ( plain )
				glDeleteTextures( 1, &plain );
			if ( plainAt )
				airAtlasRelease( &gfn, plainKey, plainAt, plainFbo, plainSh );
			if ( red == 4096 )
				fillAtlas( atlasFbo, 1 );
			if ( red == 8192 )
				setTable( kAtTile );
			if ( red == 16384 )
				_putenv( "WW_VKVIEW_RTBUF_RED=0" );
			if ( red == 32768 )
				gRtFresh = true;
			if ( plainRs )
				glDeleteTextures( 1, &plainRs );	// its buffer lives on (the gate's GL table has no glDeleteBuffers)
			bool fired = false;
			VolV jv;
			const char * name = red == 32 ? "key" : red == 64 ? "feedback" : red == 128 ? "volStbn" : red == 256 ? "volLampHist"
				: red == 512 ? "cellDdgiAtlas" : red == 2048 ? "volLampAtlas" : red == 16384 ? "WW_VKVIEW_RTBUF_RED"
				: red == 32768 ? "gate: the owner has nothing new" : red == 65536 ? "cellRestir" : nullptr;
			if ( name )
				fired = !ran && rw.find( name ) != std::string::npos;
			else if ( ran ) {
				jv = judge( g, rv );
				fired = !jv.ok();
			}
			std::printf( "AIRLAMPVIEW %s red %d: %s (unwritten %d, past the bar %d, hash differ %d, moved %d)%s%s\n", an, red,
				fired ? "FAIL (fired)" : "PASS (dead)", jv.unwritten, jv.bad, jv.hashBad, jv.moved, ran ? "" : " refused: ", ran ? "" : rw.c_str() );
			redsFired += fired;
		}
		{
			std::string aw;
			WwVkPresent::AirLampStat as;
			Pair rv;
			const bool ran = vkRun( 0, b0, aw, as, &rv );
			const VolV v = ran ? judge( v0, rv ) : VolV();
			const bool ok = ran && v.ok() && v.diff == 0 && as.copies == 0;
			std::printf( "AIRLAMPVIEW %s rerun after the reds: %s (differ %d, copies %d)%s%s\n", an, ok ? "EXACT" : "FAIL", v.diff, as.copies,
				ran ? "" : " refused: ", ran ? "" : aw.c_str() );
			fails += !ok;
		}
#endif
		if ( !err.empty() )
			return fatal( arm + " (reds): " + err );
	}
#ifdef WW_VK_AIRLAMP
	for ( int s2 = 0; s2 < 2; s2++ ) {	// lane VKAIR18: the gate's owner gone
		WwVkPresent::rtBufOwner( s2, nullptr );
		WwVkPresent::rtBufDrop( s2 );
	}
#endif
	std::printf( "AIRLAMPVIEW GL: %d fails, %d probes TESTS NOTHING\n", fails, probesDead );
#ifdef WW_VK_AIRLAMP
	const bool ok = fails == 0 && probesDead == 0 && redsRun > 0 && redsFired == redsRun;
	std::printf( "AIRLAMPVIEW %d fails, %d dead probes, reds %d of %d fired\n", fails, probesDead, redsFired, redsRun );
	std::printf( "AIRLAMPVIEW GATE %s\n", ok ? "OK" : "FAILED" );
	return ok ? 0 : 1;
#else
	( void )redsRun;
	( void )redsFired;
	std::printf( "AIRLAMPVIEW GATE FAILED (no route: WW_VK_AIRLAMP not defined; GL coverage and probes above)\n" );
	return 1;
#endif
}
