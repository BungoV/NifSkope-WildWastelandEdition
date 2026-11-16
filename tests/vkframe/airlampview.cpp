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
// Fixture: 21 x 12 x 64, identity reprojection (the history tap is the same froxel). Arms: def frame 1 (volLampHistOk 0: no
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
// Last line `AIRLAMPVIEW GATE OK` or `AIRLAMPVIEW GATE FAILED`.
#define FLD_NO_MAIN
#include "farloddraw.cpp"
#undef FLD_NO_MAIN
#include "features/air/cellvolairshare.h"

namespace alv
{
using namespace fv;
using namespace fld;

const int kLvCols = 21, kLvRows = 12, kLvSlices = 64, kLvLive = 50, kLvMask = 128, kLvMaskD = 64, kLvLights = 2, kLvTpl = 8;
const float kLvSentinel = 7.0f;
const std::uint16_t kLvSentinelH = 0x4700;	// 7.0 in half
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
};
At at;

bool loadAt()
{
	return load( at.vertexAttribPointer, "glVertexAttribPointer" ) && load( at.enableVertexAttribArray, "glEnableVertexAttribArray" )
		&& load( at.genVertexArrays, "glGenVertexArrays" ) && load( at.bindVertexArray, "glBindVertexArray" )
		&& load( at.clearTexImage, "glClearTexImage" ) && load( at.clearTexSubImage, "glClearTexSubImage" )
		&& load( at.framebufferTextureLayer, "glFramebufferTextureLayer" );
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
		{ "volReproj", { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0 }, true }, { "volPrevGrid", { 1.2, 2.1, 0.5, lnr }, true },
		{ "volPrevScale", { 1.0 }, true }, { "fogDistScale", { 1.0 }, true }, { "cellRow", { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0 }, false } };
	for ( const U & u : us )
		if ( !setU( p, u.n, u.v ) ) {
			if ( u.need )
				return u.n;
			if ( absent )
				*absent += std::string( " " ) + u.n;
		}
	return "";
}

struct Units
{
	int now = -1, hist = -1, stbn = -1, lights = -1, ddgi = -1;
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
			: n == "cellDdgiAtlas" ? &u.ddgi : nullptr;
		if ( slot )
			*slot = us[0];
		f.uniform1iv( f.getUniformLocation( p, nm ), GLsizei( us.size() ), us.data() );
		samplers += int( us.size() );
	}
	std::printf( "AIRLAMPVIEW %s: %d blocks zero, %d loose uniforms seeded, %d samplers on units 1..%d (volLampNow %d volLampHist %d volStbn %d "
				 "cellLights %d cellDdgiAtlas %d)\n",
		tag, nb, seeded, samplers, unit - 1, u.now, u.hist, u.stbn, u.lights, u.ddgi );
	if ( u.now < 0 || u.hist < 0 || u.stbn < 0 || u.lights < 0 )
		return "no volLampNow / volLampHist / volStbn / cellLights sampler";
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
	};
	auto bindInputs = [&]( const Units & u, const Bound & b ) {
		bindUnit( u.now, kLv3d, b.now );
		bindUnit( u.hist, kLv3d, b.hist );
		bindUnit( u.stbn, kLv2da, b.stbn );
		bindUnit( u.lights, kLvTb, lightTex );
	};
	auto unbindInputs = [&]( const Units & u ) {
		bindUnit( u.now, kLv3d, 0 );
		bindUnit( u.hist, kLv3d, 0 );
		bindUnit( u.stbn, kLv2da, 0 );
		bindUnit( u.lights, kLvTb, 0 );
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

	struct Arm
	{
		int key, histOk;
	};
	const Arm arms[3] = { { 0, 0 }, { 0, 1 }, { 1, 1 } };
	int fails = 0, probesDead = 0, redsRun = 0, redsFired = 0;
	for ( const Arm & a : arms ) {
		const int key = a.key, hk = a.histOk;
		const std::string arm = std::string( keyName[key] ) + " frame " + ( hk ? "2" : "1" );
		const char * an = arm.c_str();
		const Bound b0 { in.now[0], in.hist[0], in.stbn };
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
		}
		for ( int red : { 1, 2, 4, 1024, 8, 16, 32, 64, 128, 256, 512 } ) {
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
			bool fired = false;
			VolV jv;
			const char * name = red == 32 ? "key" : red == 64 ? "feedback" : red == 128 ? "volStbn" : red == 256 ? "volLampHist"
				: red == 512 ? "cellDdgiAtlas" : nullptr;
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
