// Lane VKFAR21 (2026-11-16): the reflection probe G-buffer CAPTURE (cellreflprobe.cpp, cell_reflcap.prog, vkstatus row 113)
// on the present device. BAR FIRST: this file holds the bar and the reds, set before any route code or run, never widened.
// The harness body is VKFAR23's (2026-11-16): bash tests/vkframe/reflcapdraw.sh <out dir>.
//
// Route under test (to be written: vkreflcapture.inc, WwVkPresent::reflCaptureRun): per job (slot, face), a render pass
// with two colour attachments = level 0, layer 6 * slot + face of the SHARED G-buffer cube arrays (sharedTarget type 3,
// 64 x 64 x 6 * 33: rpAlb RGBA8, rpNd RGBA16F; the views made per layer) and one depth attachment (a shared 64 x 64 D24
// 2D target, the twin's own, so GL can read it in the gate), all three cleared like GL's (colour 0, depth 1.0), depth test
// LEQUAL with writes, no cull, no blend, fill; one draw per caster with the reflcap_view_vs / reflcap_view_ps twins
// (vkframe_spirv.sh fxt / fxg lines), its client arrays copied into a host-visible vertex + index ring (location 0 position,
// 1 colour, 2 normal, 7 uv; a missing array reads GL's current value (0, 0, 0, 1) from a constant stream), its b31 slices
// (VS at b287: rpRelFromView, rpClipFromRel, modelViewMatrix, uvScale, uvOffset; PS at b31: rpHasBase, rpHasVc, rpAlphaTest,
// alpha, alphaThreshold) and BaseMap from the GL texture the caster binds (a twin by waterTwin / farTwin, as the relight's
// inputs), viewport 0 0 64 64 (no y flip: GL's face rows from the bottom are the image's rows from the top).
// GL's side is GL's own program (cell_reflcap.prog) with the app's state into GL-owned cube arrays of the same formats and a
// DEPTH_COMPONENT24 renderbuffer, attached per layer with glFramebufferTextureLayer, as the app does.
//
// Fixture: 4 casters, 3 jobs (slot 0 face 0, slot 0 face 3, slot 2 face 1; every other layer is cleared to a marker on both
// sides first and must keep it): A a textured quad with an alpha checker (RGBA8 BaseMap, alpha 0 / 1 in 8-texel squares,
// alphaThreshold 0.5, rpAlphaTest on), uvScale (2, 2), uvOffset (0.25, 0.125); B a vertex-coloured box (no base map, rpHasVc
// on, colours distinct per corner) partly IN FRONT of A (the depth test decides); C a mesh seen from its back (normals point
// away from the probe: the shader turns them); D a caster with normals but no uv (rpHasBase off: grey 0.5).
// Bar (per job; codes as stored):
//   covered = texels whose GL alb.a is 1; covered >= 500 of 4096 (an empty face FAILS).
//   depth: Vulkan's D24 code == GL's on EVERY texel (EXACT; read as 24-bit integers).
//   hit mask: alb.a equal on every texel (EXACT).
//   alb RGB (UNORM8): |vk - gl| <= 1 code on every covered texel; EXACT expected (printed).
//   nd (RGBA16F, normal xyz + distance w): |vk - gl| <= 2 half-float steps per channel on >= 99.9 % of covered texels and
//   relative <= 1e-3 on all of them (the 0.1 % colour bar); texels GL did not cover equal the clear on both sides.
//   untouched layers: both sides hold the marker (EXACT: proves the per-layer view).
// Reds (each must FAIL, never pass; a red that moves nothing prints TESTS NOTHING and fails the gate):
//   1 layer: Vulkan draws job 0 into layer + 1; 2 clip: rpClipFromRel's B term (row 3 z) zero on Vulkan's b287 slice;
//   4 uv: uvOffset zero on Vulkan's slice (the fixture's is not); 8 alpha: rpAlphaTest false on Vulkan's PS slice;
//   16 vc: location 1 fed (1, 1, 1, 1) on Vulkan; 32 depth: Vulkan's depth compare ALWAYS.
// Bar addendum (VKFAR23, 2026-11-16, set before the route change; the coordinator's in-place BaseMap ruling): C's BaseMap is
// a BC4 texture of the texture cache (objTexMake 0x8DBB, 16 x 16, 5 levels of seeded blocks, trilinear, repeat) with GL's
// swizzle R, R, R, 1 (as gli loads BC4), so Vulkan must read it through the twin's own swizzled view. The main run's stat:
//   copies == 1 (A's RGBA8 checker, not a cache texture) and inPlace == 1 (C's cache twin read in place, never copied).
//   Red 64 swz: Vulkan reads C's BaseMap without the swizzle (R, 0, 0, 1); must FAIL.
// Last line `REFLCAPDRAW GATE OK` or `REFLCAPDRAW GATE FAILED`.
//
// Harness (VKFAR23; the bar above unchanged). The route keeps ONE depth target for a run, so a run's depth holds its last
// job only: the main run (all 3 jobs) is judged on every term with the depth of its last job, and one single-job run per job
// (the same draws) gives each job's depth for the depth term. Reds run the 3-job run and are judged the same way. Each job
// sees the casters through its own rpRelFromView: a rotation taking face 0's axes (M, U, V of cellreflprobe.cpp) to the job
// face's, and the probe 22 / 20 / 24 units behind the casters (30 / 26 / 34 covered 424 / 527 / 349: under the bar's 500;
// shifted 0 / 3 / -2 along U), so every face draws all four in the order B, A, C, D (B in front of A but drawn first: red 32 lets A cover it). "relative" in the nd term is
// |vk - gl| / max(1, |gl|) per channel (the colour bar's form: normal components near 0 are judged absolutely).
#define FLD_NO_MAIN
#include "farloddraw.cpp"
#undef FLD_NO_MAIN

namespace rcd
{
using namespace fv;
using namespace fld;

const int kFace = 64, kLayers = 6 * 33, kTexel = kFace * kFace;
const int kJobFace[3] = { 0, 3, 1 }, kJobSlot[3] = { 0, 0, 2 };
const float kJobDist[3] = { 22.0f, 20.0f, 24.0f }, kJobShift[3] = { 0.0f, 3.0f, -2.0f };
// cellreflprobe.cpp's face axes
const float kM[6][3] = { { 1, 0, 0 }, { -1, 0, 0 }, { 0, 1, 0 }, { 0, -1, 0 }, { 0, 0, 1 }, { 0, 0, -1 } };
const float kU[6][3] = { { 0, 0, -1 }, { 0, 0, 1 }, { 1, 0, 0 }, { 1, 0, 0 }, { 1, 0, 0 }, { -1, 0, 0 } };
const float kV[6][3] = { { 0, -1, 0 }, { 0, -1, 0 }, { 0, 0, 1 }, { 0, 0, -1 }, { 0, -1, 0 }, { 0, -1, 0 } };
const std::uint8_t kAlbMark[4] = { 7, 77, 177, 99 };
const float kNdMark[4] = { 0.125f, -0.25f, 0.5f, 321.0f };

struct Xf
{
	void( WINAPI * vertexAttribPointer )( GLuint, GLint, GLenum, GLboolean, GLsizei, const void * ) = nullptr;
	void( WINAPI * enableVertexAttribArray )( GLuint ) = nullptr;
	void( WINAPI * disableVertexAttribArray )( GLuint ) = nullptr;
	void( WINAPI * texStorage2D )( GLenum, GLsizei, GLenum, GLsizei, GLsizei ) = nullptr;
	void( WINAPI * texStorage3D )( GLenum, GLsizei, GLenum, GLsizei, GLsizei, GLsizei ) = nullptr;
	void( WINAPI * clearTexImage )( GLuint, GLint, GLenum, GLenum, const void * ) = nullptr;
	void( WINAPI * genRenderbuffers )( GLsizei, GLuint * ) = nullptr;
	void( WINAPI * bindRenderbuffer )( GLenum, GLuint ) = nullptr;
	void( WINAPI * renderbufferStorage )( GLenum, GLenum, GLsizei, GLsizei ) = nullptr;
	void( WINAPI * framebufferRenderbuffer )( GLenum, GLenum, GLenum, GLuint ) = nullptr;
	void( WINAPI * framebufferTextureLayer )( GLenum, GLenum, GLuint, GLint, GLint ) = nullptr;
	void( WINAPI * clearBufferfv )( GLenum, GLint, const GLfloat * ) = nullptr;
	void( WINAPI * compressedTextureSubImage2D )( GLuint, GLint, GLint, GLint, GLsizei, GLsizei, GLenum, GLsizei, const void * ) = nullptr;
	void( WINAPI * textureParameteri )( GLuint, GLenum, GLint ) = nullptr;
};
Xf xf;

bool loadXf()
{
	return load( xf.vertexAttribPointer, "glVertexAttribPointer" ) && load( xf.enableVertexAttribArray, "glEnableVertexAttribArray" )
		&& load( xf.disableVertexAttribArray, "glDisableVertexAttribArray" ) && load( xf.texStorage2D, "glTexStorage2D" )
		&& load( xf.texStorage3D, "glTexStorage3D" ) && load( xf.clearTexImage, "glClearTexImage" )
		&& load( xf.genRenderbuffers, "glGenRenderbuffers" ) && load( xf.bindRenderbuffer, "glBindRenderbuffer" )
		&& load( xf.renderbufferStorage, "glRenderbufferStorage" ) && load( xf.framebufferRenderbuffer, "glFramebufferRenderbuffer" )
		&& load( xf.framebufferTextureLayer, "glFramebufferTextureLayer" ) && load( xf.clearBufferfv, "glClearBufferfv" )
		&& load( xf.compressedTextureSubImage2D, "glCompressedTextureSubImage2D" ) && load( xf.textureParameteri, "glTextureParameteri" );
}

float h2f( std::uint16_t h )
{
	const int e = ( h >> 10 ) & 31, m = h & 0x3FF;
	const float s = ( h & 0x8000 ) ? -1.0f : 1.0f;
	if ( e == 0 )
		return s * std::ldexp( float( m ), -24 );
	if ( e == 31 )
		return s * 65504.0f;
	return s * std::ldexp( float( m | 0x400 ), e - 25 );
}

int hOrd( std::uint16_t h )
{
	return ( h & 0x8000 ) ? -int( h & 0x7FFF ) : int( h );
}

//! a caster: model-space arrays, its model-view matrix, its material switches
struct Caster
{
	const char * name = "";
	std::vector<float> pos, col, nrm, uv;
	std::vector<std::uint16_t> idx;
	float mv[16] = {};
	bool base = false, vc = false, at = false;
	bool bc4 = false;  //!< VKFAR23: BaseMap is the BC4 cache twin (R, R, R, 1), else the RGBA8 checker
	float uvScale[2] = { 1, 1 }, uvOffset[2] = { 0, 0 };
};

void translate( float * m, float x, float y, float z, float rotX )
{
	const float c = std::cos( rotX ), s = std::sin( rotX );
	const float r[16] = { 1, 0, 0, 0, 0, c, s, 0, 0, -s, c, 0, x, y, z, 1 };
	std::memcpy( m, r, sizeof( r ) );
}

//! a quad in the model's x = 0 plane, y in [-hy, hy], z in [-hz, hz], one normal, uv over [0, 1]
void quad( Caster & c, float hy, float hz, const float n[3], bool uv )
{
	const float ys[4] = { -hy, hy, -hy, hy }, zs[4] = { -hz, -hz, hz, hz };
	for ( int i = 0; i < 4; i++ ) {
		c.pos.insert( c.pos.end(), { 0.0f, ys[i], zs[i] } );
		// a slight per-corner tilt: the interpolated normals vary over the quad
		c.nrm.insert( c.nrm.end(), { n[0], n[1] + 0.15f * float( i & 1 ), n[2] + 0.1f * float( i >> 1 ) } );
		if ( uv )
			c.uv.insert( c.uv.end(), { float( i & 1 ), float( i >> 1 ) } );
	}
	c.idx = { 0, 1, 2, 2, 1, 3 };
}

std::vector<Caster> casters()
{
	std::vector<Caster> v( 4 );
	{	// B: the vertex-coloured box, in front of A, drawn first
		Caster & c = v[0];
		c.name = "B";
		c.vc = true;
		for ( int i = 0; i < 8; i++ ) {
			const float x = ( i & 1 ) ? 4.0f : -4.0f, y = ( i & 2 ) ? 4.0f : -4.0f, z = ( i & 4 ) ? 4.0f : -4.0f;
			c.pos.insert( c.pos.end(), { x, y, z } );
			c.nrm.insert( c.nrm.end(), { x, y, z } );
			c.col.insert( c.col.end(), { 0.1f + 0.8f * float( i & 1 ), 0.15f + 0.7f * float( ( i >> 1 ) & 1 ), 0.2f + 0.6f * float( i >> 2 ),
										   0.25f + 0.1f * float( i ) } );
		}
		c.idx = { 0, 2, 1, 1, 2, 3, 4, 5, 6, 6, 5, 7, 0, 1, 4, 4, 1, 5, 2, 6, 3, 3, 6, 7, 0, 4, 2, 2, 4, 6, 1, 3, 5, 5, 3, 7 };
		translate( c.mv, -4.0f, 3.0f, 5.0f, 0.35f );
	}
	{	// A: the alpha-tested textured quad
		Caster & c = v[1];
		c.name = "A";
		const float n[3] = { -1, 0, 0 };
		quad( c, 14.0f, 14.0f, n, true );
		c.base = c.at = true;
		c.uvScale[0] = c.uvScale[1] = 2.0f;
		c.uvOffset[0] = 0.25f;
		c.uvOffset[1] = 0.125f;
		translate( c.mv, 8.0f, 0.0f, 0.0f, 0.0f );
	}
	{	// C: seen from its back (normals away from the probe), textured, no alpha test
		Caster & c = v[2];
		c.name = "C";
		const float n[3] = { 1, 0, 0 };
		quad( c, 4.0f, 6.0f, n, true );
		c.base = c.bc4 = true;
		translate( c.mv, 5.0f, -9.0f, -7.0f, 0.0f );
	}
	{	// D: normals, no uv, no base map (grey 0.5), no colour
		Caster & c = v[3];
		c.name = "D";
		const float n[3] = { -1, 0, 0 };
		quad( c, 4.0f, 5.0f, n, false );
		translate( c.mv, 10.0f, 10.0f, -9.0f, 0.0f );
	}
	return v;
}

//! job j's rpRelFromView (face 0's axes to the face's, the probe behind the casters) and rpClipFromRel (cellreflprobe.cpp)
void jobMats( int j, float rel[16], float clip[16] )
{
	const int fc = kJobFace[j];
	for ( int r = 0; r < 3; r++ ) {
		rel[0 + r] = kM[fc][r];
		rel[4 + r] = -kV[fc][r];
		rel[8 + r] = -kU[fc][r];
		rel[12 + r] = kJobDist[j] * kM[fc][r] + kJobShift[j] * kU[fc][r];
	}
	rel[3] = rel[7] = rel[11] = 0.0f;
	rel[15] = 1.0f;
	const float n = 4.0f, f = 4096.0f;
	const float A = ( f + n ) / ( f - n ), B = -2.0f * f * n / ( f - n );
	for ( int c = 0; c < 3; c++ ) {
		clip[c * 4 + 0] = kU[fc][c];
		clip[c * 4 + 1] = kV[fc][c];
		clip[c * 4 + 2] = A * kM[fc][c];
		clip[c * 4 + 3] = kM[fc][c];
	}
	clip[12] = clip[13] = clip[15] = 0.0f;
	clip[14] = B;
}

struct Cube
{
	std::vector<std::uint8_t> alb;
	std::vector<std::uint16_t> nd;
};

Cube readCube( GLuint alb, GLuint nd )
{
	Cube c;
	c.alb.resize( size_t( kTexel ) * kLayers * 4 );
	c.nd.resize( size_t( kTexel ) * kLayers * 4 );
	glBindTexture( 0x9009, alb );
	glGetTexImage( 0x9009, 0, GL_RGBA, GL_UNSIGNED_BYTE, c.alb.data() );
	glBindTexture( 0x9009, nd );
	glGetTexImage( 0x9009, 0, GL_RGBA, 0x140B, c.nd.data() );
	glBindTexture( 0x9009, 0 );
	return c;
}

struct JobV
{
	int covered = 0, hitDiff = 0, albDiff = 0, albPast = 0, ndDiff = 0, ndPast = 0, ndRel = 0, uncov = 0, depthDiff = -1;
	int maxAlb = 0, maxNd = 0;
	double maxRel = 0.0;
	bool ok() const
	{
		return covered >= 500 && hitDiff == 0 && albPast == 0 && ndPast * 1000 <= covered && ndRel == 0 && uncov == 0 && depthDiff <= 0;
	}
	const char * word() const
	{
		return !ok() ? "FAIL" : hitDiff + albDiff + ndDiff + std::max( depthDiff, 0 ) == 0 ? "EXACT" : "BOUND";
	}
};

JobV judgeJob( const Cube & gl, const Cube & vk, int layer )
{
	JobV v;
	const size_t o = size_t( layer ) * kTexel * 4;
	for ( size_t t = 0; t < size_t( kTexel ); t++ ) {
		const size_t i = o + t * 4;
		const bool cov = gl.alb[i + 3] == 255;
		v.covered += cov;
		v.hitDiff += gl.alb[i + 3] != vk.alb[i + 3];
		if ( !cov ) {
			bool bad = false;
			for ( int c = 0; c < 4; c++ )
				bad = bad || gl.alb[i + size_t( c )] || vk.alb[i + size_t( c )] || gl.nd[i + size_t( c )] || vk.nd[i + size_t( c )];
			v.uncov += bad;
			continue;
		}
		int ma = 0, mn = 0;
		bool rel = false;
		for ( int c = 0; c < 3; c++ )
			ma = std::max( ma, std::abs( int( gl.alb[i + size_t( c )] ) - int( vk.alb[i + size_t( c )] ) ) );
		for ( int c = 0; c < 4; c++ ) {
			const std::uint16_t a = gl.nd[i + size_t( c )], b = vk.nd[i + size_t( c )];
			mn = std::max( mn, std::abs( hOrd( a ) - hOrd( b ) ) );
			const double ga = h2f( a ), d = std::fabs( double( h2f( b ) ) - ga ) / std::max( 1.0, std::fabs( ga ) );
			v.maxRel = std::max( v.maxRel, d );
			rel = rel || d > 1e-3;
		}
		v.albDiff += ma != 0;
		v.albPast += ma > 1;
		v.maxAlb = std::max( v.maxAlb, ma );
		v.ndDiff += mn != 0;
		v.ndPast += mn > 2;
		v.maxNd = std::max( v.maxNd, mn );
		v.ndRel += rel;
	}
	return v;
}

//! layers no job draws: both sides hold the marker
int markerBad( const Cube & gl, const Cube & vk, const std::vector<int> & jobLayers, std::uint16_t ndMark[4] )
{
	int bad = 0;
	for ( int z = 0; z < kLayers; z++ ) {
		if ( std::find( jobLayers.begin(), jobLayers.end(), z ) != jobLayers.end() )
			continue;
		const size_t o = size_t( z ) * kTexel * 4;
		bool lb = false;
		for ( size_t t = 0; t < size_t( kTexel ) && !lb; t++ )
			for ( int c = 0; c < 4; c++ ) {
				const size_t i = o + t * 4 + size_t( c );
				lb = lb || gl.alb[i] != kAlbMark[c] || vk.alb[i] != kAlbMark[c] || gl.nd[i] != ndMark[c] || vk.nd[i] != ndMark[c];
			}
		bad += lb;
	}
	return bad;
}

std::vector<std::uint32_t> readDepth( GLuint tex )
{
	std::vector<std::uint32_t> d( static_cast<size_t>( kTexel ) );
	glBindTexture( GL_TEXTURE_2D, tex );
	glGetTexImage( GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, d.data() );
	glBindTexture( GL_TEXTURE_2D, 0 );
	return d;
}

int depthDiff( const std::vector<std::uint32_t> & a, const std::vector<std::uint32_t> & b )
{
	int n = 0;
	for ( size_t i = 0; i < a.size(); i++ )
		n += ( a[i] >> 8 ) != ( b[i] >> 8 );
	return n;
}
}  // namespace rcd

int main( int argc, char ** argv )
{
	using namespace fv;
	using namespace fld;
	using namespace rcd;
	gTag = "REFLCAPDRAW";
	if ( argc < 2 ) {
		std::printf( "usage: reflcapdraw <shader dir>\nREFLCAPDRAW GATE FAILED\n" );
		return 2;
	}
	const std::string dir = argv[1];
	auto fatal = [&]( const std::string & why ) {
		std::printf( "REFLCAPDRAW GATE FAILED (%s)\n", why.c_str() );
		return 2;
	};
	const int S = 4, W = 256, H = 160;
	gOpenGl = LoadLibraryA( "opengl32.dll" );
	WNDCLASSA wc {};
	wc.style = CS_OWNDC;
	wc.lpfnWndProc = wndProc;
	wc.hInstance = GetModuleHandleA( nullptr );
	wc.lpszClassName = "wwreflcapdraw";
	RegisterClassA( &wc );
	HWND wnd = CreateWindowExA( WS_EX_NOACTIVATE, "wwreflcapdraw", "reflection capture harness", WS_OVERLAPPEDWINDOW, 40, 40, 320, 200, nullptr,
		nullptr, wc.hInstance, nullptr );
	HDC dc = GetDC( wnd );
	PIXELFORMATDESCRIPTOR pfd { sizeof( pfd ), 1, PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER, PFD_TYPE_RGBA, 32 };
	SetPixelFormat( dc, ChoosePixelFormat( dc, &pfd ), &pfd );
	HGLRC glrc = wglCreateContext( dc );
	wglMakeCurrent( dc, glrc );
	ShowWindow( wnd, SW_SHOWNOACTIVATE );
	SetWindowPos( wnd, HWND_BOTTOM, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE );
	pump();
	if ( !loadAll() || !loadMore() || !loadXf() )
		return fatal( "GL entry points" );
	std::printf( "REFLCAPDRAW GL on %s\n", reinterpret_cast<const char *>( glGetString( GL_RENDERER ) ) );
	glPixelStorei( GL_PACK_ALIGNMENT, 1 );
	glPixelStorei( GL_UNPACK_ALIGNMENT, 1 );
	std::string why;
	if ( !WwVkPresent::viewBegin( W, H, S, glProc, why ) )
		return fatal( "the view: " + why );

	// ---- the cubes: Vulkan's shared (as the app site makes them with the view on), GL's own of the same formats
	static char keys[2];
	GLuint vkAlb = 0, vkNd = 0, glAlb = 0, glNd = 0;
	if ( !WwVkPresent::sharedTarget( reinterpret_cast<std::uintptr_t>( &keys[0] ), 3, kFace, kFace, kLayers, 0x8058, vkAlb, why )
		|| !WwVkPresent::sharedTarget( reinterpret_cast<std::uintptr_t>( &keys[1] ), 3, kFace, kFace, kLayers, 0x881A, vkNd, why ) )
		return fatal( "the shared G-buffer cubes: " + why );
	for ( GLuint * t : { &glAlb, &glNd } ) {
		glGenTextures( 1, t );
		glBindTexture( 0x9009, *t );
		xf.texStorage3D( 0x9009, 1, t == &glAlb ? 0x8058 : 0x881A, kFace, kFace, kLayers );
	}
	glBindTexture( 0x9009, 0 );
	auto markAll = [&]( GLuint alb, GLuint nd ) {
		xf.clearTexImage( alb, 0, GL_RGBA, GL_UNSIGNED_BYTE, kAlbMark );
		xf.clearTexImage( nd, 0, GL_RGBA, GL_FLOAT, kNdMark );
	};
	markAll( glAlb, glNd );
	markAll( vkAlb, vkNd );
	std::uint16_t ndMark[4];
	{
		const Cube m = readCube( glAlb, glNd );
		std::memcpy( ndMark, m.nd.data(), sizeof( ndMark ) );
	}

	// ---- the BaseMap: 16 x 16 RGBA8, seeded colours, alpha 0 / 1 in 8-texel squares, mipmapped, repeat, trilinear
	GLuint baseTex = 0;
	{
		std::mt19937 rng( 5 );
		std::vector<std::uint8_t> px( 16 * 16 * 4 );
		for ( int y = 0; y < 16; y++ )
			for ( int x = 0; x < 16; x++ ) {
				std::uint8_t * p = &px[size_t( y * 16 + x ) * 4];
				for ( int c = 0; c < 3; c++ )
					p[c] = std::uint8_t( 30 + rng() % 200 );
				p[3] = ( ( x >> 3 ) ^ ( y >> 3 ) ) & 1 ? 255 : 0;
			}
		glGenTextures( 1, &baseTex );
		glBindTexture( GL_TEXTURE_2D, baseTex );
		xf.texStorage2D( GL_TEXTURE_2D, 5, 0x8058, 16, 16 );
		glTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, 16, 16, GL_RGBA, GL_UNSIGNED_BYTE, px.data() );
		f.generateMipmap( GL_TEXTURE_2D );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT );
		glBindTexture( GL_TEXTURE_2D, 0 );
	}
	// ---- C's BaseMap (bar addendum): a BC4 texture cache twin, seeded blocks per level, gli's swizzle R, R, R, 1
	unsigned bcTex = 0;
	{
		if ( !WwVkPresent::objTexMake( 0x8DBB, 16, 16, 5, bcTex, why ) )
			return fatal( "objTexMake BC4: " + why );
		std::mt19937 rng( 9 );
		for ( int l = 0; l < 5; l++ ) {
			const int lw = std::max( 16 >> l, 1 ), lh = std::max( 16 >> l, 1 );
			std::vector<std::uint8_t> blocks( size_t( ( lw + 3 ) / 4 ) * size_t( ( lh + 3 ) / 4 ) * 8 );
			for ( std::uint8_t & b : blocks )
				b = std::uint8_t( rng() & 0xFF );
			xf.compressedTextureSubImage2D( bcTex, l, 0, 0, lw, lh, 0x8DBB, GLsizei( blocks.size() ), blocks.data() );
		}
		xf.textureParameteri( bcTex, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR );
		xf.textureParameteri( bcTex, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
		xf.textureParameteri( bcTex, GL_TEXTURE_WRAP_S, GL_REPEAT );
		xf.textureParameteri( bcTex, GL_TEXTURE_WRAP_T, GL_REPEAT );
		xf.textureParameteri( bcTex, 0x8E42, GL_RED );	// GL_TEXTURE_SWIZZLE_R .. A
		xf.textureParameteri( bcTex, 0x8E43, GL_RED );
		xf.textureParameteri( bcTex, 0x8E44, GL_RED );
		xf.textureParameteri( bcTex, 0x8E45, GL_ONE );
		if ( const GLenum e = glGetError() )
			return fatal( "GL error " + std::to_string( e ) + " making the BC4 BaseMap" );
	}

	// ---- GL's program (cell_reflcap.vert / .frag as the app links them: no include, no loader define)
	const GLuint prog = program( readFile( dir + "/cell_reflcap.vert" ), readFile( dir + "/cell_reflcap.frag" ), "cell_reflcap" );
	if ( !prog )
		return fatal( "GL program cell_reflcap" );
	f.useProgram( prog );
	auto loc = [&]( const char * n ) { return f.getUniformLocation( prog, n ); };
	for ( const char * n : { "rpRelFromView", "rpClipFromRel", "modelViewMatrix", "uvScale", "uvOffset", "BaseMap", "rpHasBase", "rpHasVc",
			  "rpAlphaTest", "alpha", "alphaThreshold" } )
		if ( loc( n ) < 0 )
			return fatal( std::string( "GL's program has no uniform " ) + n );
	f.uniform1i( loc( "BaseMap" ), 0 );
	const std::vector<Caster> cs = casters();
	// one caster's GL state as the app sets it before drawShape (cellreflprobe.cpp): uniforms, BaseMap, the client arrays
	auto setCaster = [&]( const Caster & c ) {
		f.uniform1i( loc( "rpHasBase" ), c.base );
		f.uniform1i( loc( "rpAlphaTest" ), c.at && c.base );
		f.uniform2fv( loc( "uvScale" ), 1, c.uvScale );
		f.uniform2fv( loc( "uvOffset" ), 1, c.uvOffset );
		const float one = 1.0f, half = 0.5f;
		f.uniform1fv( loc( "alpha" ), 1, &one );
		f.uniform1fv( loc( "alphaThreshold" ), 1, &half );
		f.uniform1i( loc( "rpHasVc" ), c.vc );
		f.uniformMatrix4fv( loc( "modelViewMatrix" ), 1, GL_FALSE, c.mv );
		bindUnit( 0, GL_TEXTURE_2D, !c.base ? 0 : c.bc4 ? GLuint( bcTex ) : baseTex );
		f.bindBuffer( 0x8892, 0 );
		f.bindBuffer( 0x8893, 0 );
		const std::vector<float> * arr[4] = { &c.pos, &c.col, &c.nrm, &c.uv };
		const GLuint at[4] = { 0, 1, 2, 7 };
		const GLint comps[4] = { 3, 4, 3, 2 };
		for ( int s = 0; s < 4; s++ )
			if ( !arr[s]->empty() ) {
				xf.vertexAttribPointer( at[s], comps[s], GL_FLOAT, GL_FALSE, 0, arr[s]->data() );
				xf.enableVertexAttribArray( at[s] );
			} else
				xf.disableVertexAttribArray( at[s] );
	};

	// ---- GL's capture: per job capFbo + the D24 renderbuffer, the layer attached, cleared, the casters drawn
	GLuint capFbo = 0, rb = 0;
	f.genFramebuffers( 1, &capFbo );
	xf.genRenderbuffers( 1, &rb );
	xf.bindRenderbuffer( 0x8D41, rb );
	xf.renderbufferStorage( 0x8D41, 0x81A6, kFace, kFace );
	std::vector<int> jobLayers;
	std::vector<std::uint32_t> glDepth[3];
	drawState0();
	f.bindFramebuffer( fFb, capFbo );
	xf.framebufferRenderbuffer( fFb, 0x8D00 /* GL_DEPTH_ATTACHMENT */, 0x8D41, rb );
	{
		const GLenum bufs[2] = { fCol0, fCol0 + 1 };
		f.drawBuffers( 2, bufs );
	}
	glEnable( GL_DEPTH_TEST );
	glDepthFunc( GL_LEQUAL );
	glDepthMask( GL_TRUE );
	glViewport( 0, 0, kFace, kFace );
	for ( int j = 0; j < 3; j++ ) {
		const int layer = 6 * kJobSlot[j] + kJobFace[j];
		jobLayers.push_back( layer );
		xf.framebufferTextureLayer( fFb, fCol0, glAlb, 0, layer );
		xf.framebufferTextureLayer( fFb, fCol0 + 1, glNd, 0, layer );
		if ( f.checkFramebufferStatus( fFb ) != fComplete )
			return fatal( "GL's capture framebuffer is incomplete" );
		const float zero[4] = { 0, 0, 0, 0 };
		xf.clearBufferfv( GL_COLOR, 0, zero );
		xf.clearBufferfv( GL_COLOR, 1, zero );
		glClearDepth( 1.0 );
		glClear( GL_DEPTH_BUFFER_BIT );
		float rel[16], clip[16];
		jobMats( j, rel, clip );
		f.uniformMatrix4fv( loc( "rpRelFromView" ), 1, GL_FALSE, rel );
		f.uniformMatrix4fv( loc( "rpClipFromRel" ), 1, GL_FALSE, clip );
		for ( const Caster & c : cs ) {
			setCaster( c );
			glDrawElements( GL_TRIANGLES, GLsizei( c.idx.size() ), GL_UNSIGNED_SHORT, c.idx.data() );
		}
		glDepth[j].resize( size_t( kTexel ) );
		f.bindFramebuffer( 0x8CA8, capFbo );	// GL_READ_FRAMEBUFFER
		glReadPixels( 0, 0, kFace, kFace, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, glDepth[j].data() );
	}
	f.bindFramebuffer( fFb, 0 );
	drawState0();
	glFinish();
	if ( const GLenum e = glGetError() )
		return fatal( "GL error " + std::to_string( e ) + " in GL's capture" );
	const Cube gl = readCube( glAlb, glNd );
	for ( int j = 0; j < 3; j++ ) {
		const JobV v = judgeJob( gl, gl, jobLayers[size_t( j )] );
		int deep = 0;
		for ( std::uint32_t d : glDepth[j] )
			deep += ( d >> 8 ) != 0xFFFFFFu;
		std::printf( "REFLCAPDRAW GL job %d (layer %d): covered %d of %d, depth written %d\n", j, jobLayers[size_t( j )], v.covered, kTexel, deep );
	}

	// ---- the present device: the casters collected as the app site does (uniforms set, reflCaptureDrawOf), then the run
	std::vector<WwVkPresent::ReflCaptureDraw> draws;
	std::vector<WwVkPresent::ReflCaptureJob> jobs;
	for ( int j = 0; j < 3; j++ ) {
		float rel[16], clip[16];
		jobMats( j, rel, clip );
		f.uniformMatrix4fv( loc( "rpRelFromView" ), 1, GL_FALSE, rel );
		f.uniformMatrix4fv( loc( "rpClipFromRel" ), 1, GL_FALSE, clip );
		WwVkPresent::ReflCaptureJob vj;
		vj.layer = jobLayers[size_t( j )];
		vj.first = int( draws.size() );
		for ( const Caster & c : cs ) {
			setCaster( c );
			WwVkPresent::ReflCaptureDraw d;
			d.pos = c.pos.data();
			d.col = c.col.empty() ? nullptr : c.col.data();
			d.nrm = c.nrm.empty() ? nullptr : c.nrm.data();
			d.uv = c.uv.empty() ? nullptr : c.uv.data();
			d.verts = int( c.pos.size() / 3 );
			d.idx = c.idx.data();
			d.indices = int( c.idx.size() );
			WwVkPresent::reflCaptureDrawOf( gw, prog, d );
			if ( !c.base )
				d.base = WwVkPresent::WaterSlot();
			draws.push_back( d );
		}
		vj.count = int( draws.size() ) - vj.first;
		jobs.push_back( vj );
	}
	for ( GLuint s : { 0u, 1u, 2u, 7u } )
		xf.disableVertexAttribArray( s );
	auto vkRun = [&]( int red, const std::vector<WwVkPresent::ReflCaptureJob> & js, std::string & w ) -> bool {
#ifdef WW_VK_REFLCAPTURE
		markAll( vkAlb, vkNd );
		WwVkPresent::ReflCaptureRun r;
		r.prog = prog;
		r.wwDefs = "";
		r.albKey = reinterpret_cast<std::uintptr_t>( &keys[0] );
		r.ndKey = reinterpret_cast<std::uintptr_t>( &keys[1] );
		r.jobs = js;
		r.draws = draws;
		r.red = red;
		return WwVkPresent::reflCaptureRun( r, w );
#else
		( void )red;
		( void )js;
		w = "no route (vkreflcapture.inc is not built: WW_VK_REFLCAPTURE undefined)";
		return false;
#endif
	};
	// every term of a 3-job run (depth: the last job's); returns the failing job / marker count
	auto judgeRun = [&]( const char * tag, bool print ) -> int {
		const Cube vk = readCube( vkAlb, vkNd );
		const std::vector<std::uint32_t> vd = readDepth( WwVkPresent::reflCaptureDepthTex() );
		int bad = 0;
		for ( int j = 0; j < 3; j++ ) {
			JobV v = judgeJob( gl, vk, jobLayers[size_t( j )] );
			if ( j == 2 )
				v.depthDiff = depthDiff( glDepth[2], vd );
			bad += !v.ok();
			if ( print )
				std::printf( "REFLCAPDRAW %s job %d (layer %d): %s (covered %d, hit differ %d, alb differ %d past 1 %d max %d, nd differ %d past 2 %d "
							 "max %d rel past %d max %.2e, uncovered off clear %d, depth differ %d)\n",
					tag, j, jobLayers[size_t( j )], v.word(), v.covered, v.hitDiff, v.albDiff, v.albPast, v.maxAlb, v.ndDiff, v.ndPast, v.maxNd,
					v.ndRel, v.maxRel, v.uncov, v.depthDiff );
		}
		const int mb = markerBad( gl, vk, jobLayers, ndMark );
		if ( print )
			std::printf( "REFLCAPDRAW %s untouched layers: %s (%d of %d off the marker)\n", tag, mb ? "FAIL" : "EXACT", mb, kLayers - 3 );
		return bad + mb;
	};
	int fails = 0;
	std::string w;
	if ( !vkRun( 0, jobs, w ) ) {
		std::printf( "REFLCAPDRAW Vulkan refused: %s\nREFLCAPDRAW GATE FAILED\n", w.c_str() );
		return 1;
	}
	fails += judgeRun( "main", true );
	{
		const WwVkPresent::ReflCaptureStat st = WwVkPresent::takeReflCaptureStat();
		std::printf( "REFLCAPDRAW main stat: runs %d faces %d draws %d crossings %d copies %d inPlace %d pipelines %d fails %d\n", st.runs, st.faces,
			st.draws, st.crossings, st.copies, st.inPlace, st.pipelines, st.fails );
		// bar addendum: the checker copied, the BC4 cache twin read in place
		const bool place = st.copies == 1 && st.inPlace == 1;
		std::printf( "REFLCAPDRAW main BaseMap route: %s (copies %d want 1, inPlace %d want 1)\n", place ? "OK" : "FAIL", st.copies, st.inPlace );
		fails += !place;
	}
	for ( int j = 0; j < 3; j++ ) {	// each job's depth: a single-job run
		std::string sw;
		if ( !vkRun( 0, { jobs[size_t( j )] }, sw ) ) {
			std::printf( "REFLCAPDRAW depth job %d: Vulkan refused: %s\n", j, sw.c_str() );
			fails++;
			continue;
		}
		const int dd = depthDiff( glDepth[j], readDepth( WwVkPresent::reflCaptureDepthTex() ) );
		std::printf( "REFLCAPDRAW depth job %d (single run): %s (%d of %d D24 codes differ)\n", j, dd ? "FAIL" : "EXACT", dd, kTexel );
		fails += dd != 0;
	}
	// ---- reds
	int redsRun = 0, redsFired = 0;
	for ( int red : { 1, 2, 4, 8, 16, 32, 64 } ) {
		redsRun++;
		std::string rw;
		const bool ran = vkRun( red, jobs, rw );
		const int bad = ran ? judgeRun( "red", false ) : 0;
		const bool fired = ran && bad > 0;
		std::printf( "REFLCAPDRAW red %d: %s%s%s\n", red, fired ? "FAIL (fired, " : ran ? "PASS (dead) TESTS NOTHING" : "REFUSED: ",
			fired ? ( std::to_string( bad ) + " jobs / layers off)" ).c_str() : "", ran ? "" : rw.c_str() );
		redsFired += fired;
	}
	const bool ok = fails == 0 && redsFired == redsRun;
	std::printf( "REFLCAPDRAW %d fails, reds %d of %d fired\n", fails, redsFired, redsRun );
	std::printf( "REFLCAPDRAW GATE %s\n", ok ? "OK" : "FAILED" );
	return ok ? 0 : 1;
}
