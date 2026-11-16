// Lane VKGROUND6 (2026-11-16): the far ground page paint (cellfarground.cpp paintJobs: drawShape 1 = cell_fgpage into the
// RGBA8 scratch, drawShape 2 = cell_fgbc1 a level into the RG32UI blocks, uploaded into the pool's BC1 layer) on the present
// device (vkfgpaint.h). GL's side is GL's own two programs (fgpaintview_prep.py) with paintJobs' state: the quad
// { -1 -1 0, 1 -1 0, -1 1 0, 1 1 0 } / { 0 1 2 2 1 3 } at location 0, viewport side x side, glGenerateMipmap, then per level L
// fgSrcLevel L at viewport (side >> L) / 4, the blocks read back and uploaded into level L of the layer
// (glCompressedTexSubImage3D, GL_COMPRESSED_RGB_S3TC_DXT1).
// Fixture (worked out before the first run): the ring-0 cell (1, 1) (world 4096..8192), a 40 x 40 grid at spacing 128 from
// (3584, 3584) (the cell + 512 a side), fgGrid RGBA32UI (nearest, clamped), fgLtex 64 x 64 x 4 RGBA8 (7 levels, repeat,
// aniso 8 on the texture; GL's paint samples it through its own sampler object at anisotropy 1, as paintJobs does since
// VKGROUND8), fgAtlas 64 x 64 RGBA8 (7 levels): all three SHARED (sharedTarget, as cellfarground.cpp makes them with the view
// on); fgHeight R32F 40 x 40 (GL-owned, linear), heights 250..350; fgDecal a texel buffer of 8 records (boxes about 600 x 500
// world) over 10 x 10 bins of 512, every bin listing every record. Arms N (side 256, 7 levels) and F (side 128, 6 levels),
// pools of 3 layers, the page into layer 1. Every layer and level of both pools starts with the same random BC1 blocks
// (the prior), uploaded into GL's own pool and the Vulkan far twin pool alike.
// Bar (set before the first run, never widened):
//   GL: two GL paints EXACT (the pool layer, every level); GL's own encode inside the BC1 bound below at every level (the
//   decoder and the reference encoder of this gate checked on GL's encoder); probes, each must move: fgBc1Red 1 breaks the
//   bound at level 0; the prior breaks it at every level (so a wrong or a stale page fails); all-zero blocks break it at
//   every level (so a skipped encode fails); fgPageDecals 0 moves >= 2000 texels of level 0 on N, >= 500 on F; level 0
//   holds >= 1000 distinct colours.
//   BC1 bound (FARPAGES1's): per level, rmse( decode( blocks ), GL's page level ) x 255 <= 1.15 x ref + 1, ref = the same
//   rmse of a principal-axis encoder (mean, covariance, 8 power steps from (1,1,1)/sqrt 3, endpoints at the extreme
//   projections, 565 by round, the 4-colour palette, nearest by squared distance). Decode: c0 = lo16, c1 = hi16, 4 colours
//   when c0 > c1 else (p0 + p1) / 2 and black, index i at bits 2i, texel i at (i & 3, i >> 2), channels c / 31, c / 63.
//   Vulkan (WW_VK_FGPAINT): S0 the route's scratch level 0 vs GL's within 2 / 255 a channel (FARPAGES1's paint-vs-live bar);
//   P every level of the layer inside the BC1 bound against GL's page level (the blocks equal to GL's counted as info: the
//   mips are a blit, not glGenerateMipmap, so EXACT is not claimed); O every other layer EXACT to the prior; E (encode only:
//   GL's scratch copied into the route's, every level) the layer's blocks EXACT to GL's pool; every judged read equal to a
//   second read; copies a run 1 (fgHeight; fgGrid / fgLtex / fgAtlas read in place); a run after the reds EXACT to the
//   first run's pool.
// Reds, both arms, each must FAIL: route red 1 the blocks into layer 2 (wrong page), 2 the copy into the pool dropped (a
// stale page), 4 the encode skipped (zero blocks copied). Before each red the Vulkan pool gets the prior again.
// Last line `FGPAINTVIEW GATE OK` or `FGPAINTVIEW GATE FAILED`.
#define FLD_NO_MAIN
#include "farloddraw.cpp"
#undef FLD_NO_MAIN
#include "features/vkframe/vkfgpaint.h"

#include <cmath>

namespace fpv
{
using namespace fv;
using namespace fld;

const int kFpGn = 40, kFpBins = 10, kFpRec = 8, kFpBinOff = kFpRec * 5, kFpIdOff = kFpBinOff + kFpBins * kFpBins;
const int kFpTexels = kFpIdOff + ( kFpBins * kFpBins * kFpRec + 3 ) / 4;
const int kFpLayers = 3, kFpLayer = 1, kFpLtex = 64, kFpLtexN = 4, kFpAtlas = 64, kFpMaps = 7;
const float kFpOx = 3584.0f, kFpOy = 3584.0f, kFpSpacing = 128.0f, kFpBin = 512.0f, kFpTiling = 400.0f, kFpCell = 4096.0f;
const float kFpCellX = 1.0f, kFpCellY = 1.0f;
const GLenum kFpBc1 = 0x83F0, kFpRg32ui = 0x823C, kFpRgba32ui = 0x8D70, kFpRgInt = 0x8228, kFpRgbaInt = 0x8D99, kFpTexBuf = 0x8C2A;
const GLenum kFpRgba8 = 0x8058, kFpClamp = 0x812F, kFpDrawFb = 0x8CA9, kFpReadFb = 0x8CA8;
enum FpUnit { uGrid = 1, uLtex, uDecal, uAtlas, uHeight };

struct Fp
{
	void( WINAPI * compressedTexSubImage3D )( GLenum, GLint, GLint, GLint, GLint, GLsizei, GLsizei, GLsizei, GLenum, GLsizei,
		const void * ) = nullptr;
	void( WINAPI * getCompressedTexImage )( GLenum, GLint, void * ) = nullptr;
	void( WINAPI * genSamplers )( GLsizei, GLuint * ) = nullptr;
	void( WINAPI * bindSampler )( GLuint, GLuint ) = nullptr;
	void( WINAPI * samplerParameteri )( GLuint, GLenum, GLint ) = nullptr;
	void( WINAPI * samplerParameterf )( GLuint, GLenum, GLfloat ) = nullptr;
};
Fp fp;

bool loadFp()
{
	return load( fp.compressedTexSubImage3D, "glCompressedTexSubImage3D" ) && load( fp.getCompressedTexImage, "glGetCompressedTexImage" )
		&& load( fp.genSamplers, "glGenSamplers" ) && load( fp.bindSampler, "glBindSampler" )
		&& load( fp.samplerParameteri, "glSamplerParameteri" ) && load( fp.samplerParameterf, "glSamplerParameterf" );
}

std::uint32_t pack4x8( float a, float b, float c, float d )
{
	auto q = []( float v ) { return std::uint32_t( std::lround( std::clamp( v, 0.0f, 1.0f ) * 255.0f ) ); };
	return q( a ) | ( q( b ) << 8 ) | ( q( c ) << 16 ) | ( q( d ) << 24 );
}

// per sample ids 0|1, ids 2|3 (65535 = the engine default), weights unorm8 x 4, colour RGB8 + page 255 (fargrounddraw's)
std::vector<std::uint32_t> gridData( std::mt19937 & rng )
{
	std::uniform_real_distribution<float> u01( 0.0f, 1.0f );
	std::vector<std::uint32_t> g( static_cast<size_t>( kFpGn * kFpGn * 4 ) );
	for ( int k = 0; k < kFpGn * kFpGn; k++ ) {
		std::uint32_t id[4];
		for ( auto & i : id )
			i = ( rng() & 7u ) == 7u ? 65535u : rng() & 3u;
		float w[4];
		for ( auto & x : w )
			x = ( rng() & 3u ) == 0u ? 0.0f : u01( rng );
		g[static_cast<size_t>( k * 4 )] = id[0] | ( id[1] << 16 );
		g[static_cast<size_t>( k * 4 + 1 )] = id[2] | ( id[3] << 16 );
		g[static_cast<size_t>( k * 4 + 2 )] = pack4x8( w[0], w[1], w[2], w[3] );
		g[static_cast<size_t>( k * 4 + 3 )] = pack4x8( 0.5f + 0.5f * u01( rng ), 0.5f + 0.5f * u01( rng ), 0.5f + 0.5f * u01( rng ), 1.0f );
	}
	return g;
}

// kFpRec records of 5 texels (position + scale, rotation, box xy, box z + texel size, atlas rect), then (start, count) a bin,
// then the record ids 4 a texel
std::vector<float> decalData( std::mt19937 & rng )
{
	std::uniform_real_distribution<float> u01( 0.0f, 1.0f );
	std::vector<float> d( static_cast<size_t>( kFpTexels * 4 ), 0.0f );
	for ( int r = 0; r < kFpRec; r++ ) {
		float * p = &d[static_cast<size_t>( r * 5 * 4 )];
		const float a = r % 3 == 0 ? 0.6f * float( r + 1 ) : 0.0f, sz = 0.8f + 0.6f * u01( rng );
		const float px = 4500.0f + 3200.0f * u01( rng ), py = 4500.0f + 3200.0f * u01( rng );
		const float rec[20] = { px, py, 300.0f, sz, 0.0f, 0.0f, std::sin( 0.5f * a ), std::cos( 0.5f * a ),
			-300.0f - 120.0f * u01( rng ), -220.0f, 300.0f, 220.0f + 100.0f * u01( rng ), -600.0f, 600.0f, 1.0f, 0.0f,
			0.05f + 0.2f * u01( rng ), 0.05f + 0.2f * u01( rng ), 0.6f + 0.3f * u01( rng ), 0.6f + 0.3f * u01( rng ) };
		std::copy( rec, rec + 20, p );
	}
	for ( int b = 0; b < kFpBins * kFpBins; b++ ) {
		d[static_cast<size_t>( ( kFpBinOff + b ) * 4 )] = float( b * kFpRec );
		d[static_cast<size_t>( ( kFpBinOff + b ) * 4 + 1 )] = float( kFpRec );
	}
	for ( int k = 0; k < kFpBins * kFpBins * kFpRec; k++ )
		d[static_cast<size_t>( kFpIdOff * 4 + k )] = float( ( k + k / kFpRec ) % kFpRec );
	return d;
}

// ---- BC1 on the CPU: decode, the reference encoder, the bound
void unpack565( std::uint32_t c, float o[3] )
{
	o[0] = float( ( c >> 11 ) & 31u ) / 31.0f;
	o[1] = float( ( c >> 5 ) & 63u ) / 63.0f;
	o[2] = float( c & 31u ) / 31.0f;
}

//! one block (lo = c0 | c1 << 16, hi = the indices) into 16 RGB texels
void decodeBlock( std::uint32_t lo, std::uint32_t hi, float o[16][3] )
{
	const std::uint32_t c0 = lo & 0xFFFFu, c1 = lo >> 16;
	float p[4][3];
	unpack565( c0, p[0] );
	unpack565( c1, p[1] );
	for ( int c = 0; c < 3; c++ ) {
		if ( c0 > c1 ) {
			p[2][c] = ( 2.0f * p[0][c] + p[1][c] ) / 3.0f;
			p[3][c] = ( p[0][c] + 2.0f * p[1][c] ) / 3.0f;
		} else {
			p[2][c] = 0.5f * ( p[0][c] + p[1][c] );
			p[3][c] = 0.0f;
		}
	}
	for ( int i = 0; i < 16; i++ )
		for ( int c = 0; c < 3; c++ )
			o[i][c] = p[( hi >> ( 2 * i ) ) & 3u][c];
}

//! a page level's RGB at (x, y), 0..1
inline float px( const std::vector<std::uint8_t> & pg, int s, int x, int y, int c )
{
	return float( pg[( static_cast<size_t>( y ) * static_cast<size_t>( s ) + static_cast<size_t>( x ) ) * 4 + static_cast<size_t>( c )] ) / 255.0f;
}

//! rmse x 255 of the blocks of one level (bw x bw, 8 bytes each) against the page level (s x s RGBA8)
double blocksRmse( const std::uint32_t * blk, const std::vector<std::uint8_t> & pg, int s )
{
	const int bw = s / 4;
	double e = 0.0;
	float o[16][3];
	for ( int by = 0; by < bw; by++ )
		for ( int bx = 0; bx < bw; bx++ ) {
			const size_t b = ( static_cast<size_t>( by ) * static_cast<size_t>( bw ) + static_cast<size_t>( bx ) ) * 2;
			decodeBlock( blk[b], blk[b + 1], o );
			for ( int i = 0; i < 16; i++ )
				for ( int c = 0; c < 3; c++ ) {
					const double d = double( o[i][c] ) - double( px( pg, s, bx * 4 + ( i & 3 ), by * 4 + ( i >> 2 ), c ) );
					e += d * d;
				}
		}
	return 255.0 * std::sqrt( e / ( double( s ) * double( s ) * 3.0 ) );
}

//! the reference encoder's rmse x 255 on one page level
double refRmse( const std::vector<std::uint8_t> & pg, int s )
{
	const int bw = s / 4;
	const float q[3] = { 31.0f, 63.0f, 31.0f };
	double e = 0.0;
	for ( int by = 0; by < bw; by++ )
		for ( int bx = 0; bx < bw; bx++ ) {
			float t[16][3], mu[3] = {}, cv[3][3] = {};
			for ( int i = 0; i < 16; i++ )
				for ( int c = 0; c < 3; c++ ) {
					t[i][c] = px( pg, s, bx * 4 + ( i & 3 ), by * 4 + ( i >> 2 ), c );
					mu[c] += t[i][c] / 16.0f;
				}
			for ( int i = 0; i < 16; i++ )
				for ( int a = 0; a < 3; a++ )
					for ( int b = 0; b < 3; b++ )
						cv[a][b] += ( t[i][a] - mu[a] ) * ( t[i][b] - mu[b] );
			float v[3] = { 0.57735027f, 0.57735027f, 0.57735027f };
			for ( int it = 0; it < 8; it++ ) {
				float n[3];
				for ( int a = 0; a < 3; a++ )
					n[a] = cv[a][0] * v[0] + cv[a][1] * v[1] + cv[a][2] * v[2];
				const float l = std::sqrt( n[0] * n[0] + n[1] * n[1] + n[2] * n[2] );
				if ( l < 1e-12f )
					break;
				for ( int a = 0; a < 3; a++ )
					v[a] = n[a] / l;
			}
			float tmin = 1e30f, tmax = -1e30f;
			for ( int i = 0; i < 16; i++ ) {
				const float d = ( t[i][0] - mu[0] ) * v[0] + ( t[i][1] - mu[1] ) * v[1] + ( t[i][2] - mu[2] ) * v[2];
				tmin = std::min( tmin, d );
				tmax = std::max( tmax, d );
			}
			float p[4][3];
			for ( int c = 0; c < 3; c++ ) {
				p[0][c] = std::round( std::clamp( mu[c] + v[c] * tmax, 0.0f, 1.0f ) * q[c] ) / q[c];
				p[1][c] = std::round( std::clamp( mu[c] + v[c] * tmin, 0.0f, 1.0f ) * q[c] ) / q[c];
				p[2][c] = ( 2.0f * p[0][c] + p[1][c] ) / 3.0f;
				p[3][c] = ( p[0][c] + 2.0f * p[1][c] ) / 3.0f;
			}
			for ( int i = 0; i < 16; i++ ) {
				double best = 1e30;
				for ( int k = 0; k < 4; k++ ) {
					double d = 0.0;
					for ( int c = 0; c < 3; c++ )
						d += double( t[i][c] - p[k][c] ) * double( t[i][c] - p[k][c] );
					best = std::min( best, d );
				}
				e += best;
			}
		}
	return 255.0 * std::sqrt( e / ( double( s ) * double( s ) * 3.0 ) );
}

struct Arm
{
	const char * name = "";
	int side = 0, levels = 0;
	GLuint scr = 0, glPool = 0, vkPool = 0;
	std::uintptr_t key = 0;
	std::vector<std::vector<std::uint32_t>> prior;	// a level: every layer's blocks
};

int levelsOf( int side )
{
	int n = 0;
	for ( int s = side; s >= 4; s >>= 1 )
		n++;
	return n;
}

std::vector<std::uint32_t> readPool( GLuint t, int level )
{
	GLint w = 0, hgt = 0, d = 0;
	glBindTexture( fT2DA, t );
	glGetTexLevelParameteriv( fT2DA, level, GL_TEXTURE_WIDTH, &w );
	glGetTexLevelParameteriv( fT2DA, level, GL_TEXTURE_HEIGHT, &hgt );
	glGetTexLevelParameteriv( fT2DA, level, 0x8071 /* GL_TEXTURE_DEPTH */, &d );
	std::vector<std::uint32_t> v( static_cast<size_t>( std::max( w / 4, 1 ) ) * static_cast<size_t>( std::max( hgt / 4, 1 ) )
		* static_cast<size_t>( std::max( d, 1 ) ) * 2, 0xDEADBEEFu );
	fp.getCompressedTexImage( fT2DA, level, v.data() );
	glBindTexture( fT2DA, 0 );
	return v;
}

void putPool( GLuint t, int level, int s, int layer0, int layers, const std::uint32_t * blocks )
{
	const int bw = s / 4;
	glBindTexture( fT2DA, t );
	fp.compressedTexSubImage3D( fT2DA, level, 0, 0, layer0, s, s, layers, kFpBc1, bw * bw * 8 * layers, blocks );
	glBindTexture( fT2DA, 0 );
}

std::vector<std::uint8_t> readLevel( GLuint t, int level, int s )
{
	std::vector<std::uint8_t> v( static_cast<size_t>( s ) * static_cast<size_t>( s ) * 4, 0 );
	glBindTexture( GL_TEXTURE_2D, t );
	glGetTexImage( GL_TEXTURE_2D, level, GL_RGBA, GL_UNSIGNED_BYTE, v.data() );
	glBindTexture( GL_TEXTURE_2D, 0 );
	return v;
}

//! one layer's blocks of a level read whole
const std::uint32_t * layerOf( const std::vector<std::uint32_t> & lv, int s, int layer )
{
	return lv.data() + static_cast<size_t>( s / 4 ) * static_cast<size_t>( s / 4 ) * 2 * static_cast<size_t>( layer );
}

GLint need( GLuint p, const char * n, std::string & miss )
{
	const GLint l = f.getUniformLocation( p, n );
	if ( l < 0 )
		miss += std::string( miss.empty() ? "" : " " ) + n;
	return l;
}
}  // namespace fpv

int main( int argc, char ** argv )
{
	using namespace fv;
	using namespace fld;
	using namespace fpv;
	gTag = "FGPAINTVIEW";
	if ( argc < 2 ) {
		std::printf( "usage: fgpaintview <prep dir>\nFGPAINTVIEW GATE FAILED\n" );
		return 2;
	}
	const std::string dir = argv[1];
	auto fatal = [&]( const std::string & why ) {
		std::printf( "FGPAINTVIEW GATE FAILED (%s)\n", why.c_str() );
		return 2;
	};
	const int S = 1, W = 256, H = 160;
	gOpenGl = LoadLibraryA( "opengl32.dll" );
	WNDCLASSA wc {};
	wc.style = CS_OWNDC;
	wc.lpfnWndProc = wndProc;
	wc.hInstance = GetModuleHandleA( nullptr );
	wc.lpszClassName = "wwfgpaintview";
	RegisterClassA( &wc );
	HWND wnd = CreateWindowExA( WS_EX_NOACTIVATE, "wwfgpaintview", "far ground paint harness", WS_OVERLAPPEDWINDOW, 40, 40, 320, 200,
		nullptr, nullptr, wc.hInstance, nullptr );
	HDC dc = GetDC( wnd );
	PIXELFORMATDESCRIPTOR pfd { sizeof( pfd ), 1, PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER, PFD_TYPE_RGBA, 32 };
	SetPixelFormat( dc, ChoosePixelFormat( dc, &pfd ), &pfd );
	HGLRC glrc = wglCreateContext( dc );
	wglMakeCurrent( dc, glrc );
	ShowWindow( wnd, SW_SHOWNOACTIVATE );
	SetWindowPos( wnd, HWND_BOTTOM, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE );
	pump();
	if ( !loadAll() || !loadMore() || !loadFp() )
		return fatal( "GL entry points" );
	std::printf( "FGPAINTVIEW GL on %s\n", reinterpret_cast<const char *>( glGetString( GL_RENDERER ) ) );
	glPixelStorei( GL_PACK_ALIGNMENT, 1 );
	glPixelStorei( GL_UNPACK_ALIGNMENT, 1 );
	std::string why;
	if ( !WwVkPresent::viewBegin( W, H, S, glProc, why ) )
		return fatal( "the view: " + why );

	// ---- the inputs: grid, layers and atlas shared as the owner makes them on the view; heights GL-owned; the decal buffer
	std::mt19937 rng( 20261116u );
	std::uniform_int_distribution<int> byte( 0, 255 );
	static char keys[8];
	auto kOf = [&]( int i ) { return reinterpret_cast<std::uintptr_t>( &keys[i] ); };
	unsigned grid = 0, ltex = 0, atlas = 0;
	if ( !WwVkPresent::sharedTarget( kOf( 0 ), 0, kFpGn, kFpGn, 1, kFpRgba32ui, grid, why ) )
		return fatal( "sharedTarget fgGrid: " + why );
	if ( !WwVkPresent::sharedTarget( kOf( 1 ), 2, kFpLtex, kFpLtex, kFpLtexN, kFpRgba8, ltex, why, kFpMaps ) )
		return fatal( "sharedTarget fgLtex: " + why );
	if ( !WwVkPresent::sharedTarget( kOf( 2 ), 0, kFpAtlas, kFpAtlas, 1, kFpRgba8, atlas, why, kFpMaps ) )
		return fatal( "sharedTarget fgAtlas: " + why );
	const std::vector<std::uint32_t> gridV = gridData( rng );
	glBindTexture( GL_TEXTURE_2D, grid );
	glTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, kFpGn, kFpGn, kFpRgbaInt, GL_UNSIGNED_INT, gridV.data() );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, kFpClamp );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, kFpClamp );
	std::vector<std::uint8_t> lt( static_cast<size_t>( kFpLtex * kFpLtex * kFpLtexN * 4 ) );
	for ( size_t i = 0; i < lt.size(); i++ )
		lt[i] = i % 4 == 3 ? 255 : std::uint8_t( byte( rng ) );
	glBindTexture( fT2DA, ltex );
	f.texSubImage3D( fT2DA, 0, 0, 0, 0, kFpLtex, kFpLtex, kFpLtexN, GL_RGBA, GL_UNSIGNED_BYTE, lt.data() );
	f.generateMipmap( fT2DA );
	glTexParameteri( fT2DA, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR );
	glTexParameteri( fT2DA, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
	glTexParameteri( fT2DA, GL_TEXTURE_WRAP_S, GL_REPEAT );
	glTexParameteri( fT2DA, GL_TEXTURE_WRAP_T, GL_REPEAT );
	glTexParameterf( fT2DA, 0x84FE /* GL_TEXTURE_MAX_ANISOTROPY */, 8.0f );
	glBindTexture( fT2DA, 0 );
	// paintJobs' LTEX sampler (VKGROUND8): the texture's state at anisotropy 1, bound for the paint draw only
	GLuint ltexSmp = 0;
	fp.genSamplers( 1, &ltexSmp );
	fp.samplerParameteri( ltexSmp, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR );
	fp.samplerParameteri( ltexSmp, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
	fp.samplerParameteri( ltexSmp, GL_TEXTURE_WRAP_S, GL_REPEAT );
	fp.samplerParameteri( ltexSmp, GL_TEXTURE_WRAP_T, GL_REPEAT );
	fp.samplerParameterf( ltexSmp, 0x84FE /* GL_TEXTURE_MAX_ANISOTROPY */, 1.0f );
	std::vector<std::uint8_t> at( static_cast<size_t>( kFpAtlas * kFpAtlas * 4 ) );
	for ( auto & x : at )
		x = std::uint8_t( byte( rng ) );
	glBindTexture( GL_TEXTURE_2D, atlas );
	glTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, kFpAtlas, kFpAtlas, GL_RGBA, GL_UNSIGNED_BYTE, at.data() );
	f.generateMipmap( GL_TEXTURE_2D );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, kFpClamp );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, kFpClamp );
	std::vector<float> hv( static_cast<size_t>( kFpGn * kFpGn ) );
	std::uniform_real_distribution<float> u01( 0.0f, 1.0f );
	for ( auto & x : hv )
		x = 250.0f + 100.0f * u01( rng );
	const GLuint height = tex2d( kFpGn, kFpGn, fR32f, GL_RED, GL_FLOAT, hv.data(), true );
	glBindTexture( GL_TEXTURE_2D, height );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, kFpClamp );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, kFpClamp );
	glBindTexture( GL_TEXTURE_2D, 0 );
	const std::vector<float> dv = decalData( rng );
	GLuint dbuf = 0, dtex = 0;
	f.genBuffers( 1, &dbuf );
	f.bindBuffer( kFpTexBuf, dbuf );
	f.bufferData( kFpTexBuf, std::ptrdiff_t( dv.size() * 4 ), dv.data(), fStatic );
	glGenTextures( 1, &dtex );
	glBindTexture( kFpTexBuf, dtex );
	ex.texBuffer( kFpTexBuf, fRgba32f, dbuf );
	glBindTexture( kFpTexBuf, 0 );
	f.bindBuffer( kFpTexBuf, 0 );
	if ( glGetError() != GL_NO_ERROR )
		return fatal( "the inputs (GL error)" );

	// ---- the programs, the quad, the block target
	const GLuint progPage = program( readFile( dir + "/page.vert.glsl" ), readFile( dir + "/page.frag.glsl" ), "page" );
	const GLuint progBc = program( readFile( dir + "/bc1.vert.glsl" ), readFile( dir + "/bc1.frag.glsl" ), "bc1" );
	if ( !progPage || !progBc )
		return fatal( "GL programs" );
	std::string miss;
	const GLint lGrid = need( progPage, "fgGrid", miss ), lLtex = need( progPage, "fgLtex", miss ), lDecal = need( progPage, "fgDecal", miss );
	const GLint lAtlas = need( progPage, "fgAtlas", miss ), lHeight = need( progPage, "fgHeight", miss );
	const GLint lGridMap = need( progPage, "fgGridMap", miss ), lGridN = need( progPage, "fgGridN", miss );
	const GLint lBinMap = need( progPage, "fgBinMap", miss ), lBinN = need( progPage, "fgBinN", miss );
	const GLint lSide = need( progPage, "fgStampSide", miss ), lDecals = need( progPage, "fgPageDecals", miss );
	const GLint lCell = need( progPage, "fgStampCell", miss );
	const GLint lSrc = need( progBc, "fgSrc", miss ), lLevel = need( progBc, "fgSrcLevel", miss ), lRed = need( progBc, "fgBc1Red", miss );
	if ( !miss.empty() )
		return fatal( "uniforms not active: " + miss );
	GLuint vao = 0, vb = 0, ib = 0;
	ex.genVertexArrays( 1, &vao );
	ex.bindVertexArray( vao );
	static const float quad[12] = { -1, -1, 0, 1, -1, 0, -1, 1, 0, 1, 1, 0 };
	static const std::uint16_t idx[6] = { 0, 1, 2, 2, 1, 3 };
	f.genBuffers( 1, &vb );
	f.genBuffers( 1, &ib );
	f.bindBuffer( 0x8892, vb );
	f.bufferData( 0x8892, sizeof( quad ), quad, fStatic );
	f.bindBuffer( 0x8893, ib );
	f.bufferData( 0x8893, sizeof( idx ), idx, fStatic );
	ex.vertexAttribPointer( 0, 3, GL_FLOAT, GL_FALSE, 12, nullptr );
	ex.enableVertexAttribArray( 0 );
	glDisable( GL_BLEND );
	glDisable( GL_DEPTH_TEST );
	glDisable( GL_SCISSOR_TEST );
	glDisable( GL_CULL_FACE );
	const int bw0 = 256 / 4;
	GLuint blocks = 0;
	glGenTextures( 1, &blocks );
	glBindTexture( GL_TEXTURE_2D, blocks );
	ex.texStorage2D( GL_TEXTURE_2D, 1, kFpRg32ui, bw0, bw0 );
	glBindTexture( GL_TEXTURE_2D, 0 );
	GLuint fb = 0, fbBc = 0;
	f.genFramebuffers( 1, &fb );
	f.genFramebuffers( 1, &fbBc );
	f.bindFramebuffer( kFpDrawFb, fbBc );
	f.framebufferTexture2D( kFpDrawFb, fCol0, GL_TEXTURE_2D, blocks, 0 );
	if ( f.checkFramebufferStatus( kFpDrawFb ) != fComplete )
		return fatal( "the block target is incomplete" );

	// ---- paintJobs' GL side
	auto paintGl = [&]( Arm & a, int decals ) {
		f.useProgram( progPage );
		bindUnit( uGrid, GL_TEXTURE_2D, grid );
		bindUnit( uLtex, fT2DA, ltex );
		bindUnit( uDecal, kFpTexBuf, dtex );
		bindUnit( uAtlas, GL_TEXTURE_2D, atlas );
		bindUnit( uHeight, GL_TEXTURE_2D, height );
		f.uniform1i( lGrid, uGrid );
		f.uniform1i( lLtex, uLtex );
		f.uniform1i( lDecal, uDecal );
		f.uniform1i( lAtlas, uAtlas );
		f.uniform1i( lHeight, uHeight );
		const float gm[4] = { kFpOx, kFpOy, 1.0f / kFpSpacing, kFpTiling }, gn[2] = { float( kFpGn ), float( kFpGn ) };
		const float bm[4] = { kFpOx, kFpOy, 1.0f / kFpBin, float( kFpMaps - 1 ) };
		const float bn[4] = { float( kFpBins ), float( kFpBins ), float( kFpBinOff ), float( kFpIdOff ) };
		const float sd = float( a.side ), cell[2] = { kFpCellX * kFpCell, kFpCellY * kFpCell };
		f.uniform4fv( lGridMap, 1, gm );
		f.uniform2fv( lGridN, 1, gn );
		f.uniform4fv( lBinMap, 1, bm );
		f.uniform4fv( lBinN, 1, bn );
		f.uniform1fv( lSide, 1, &sd );
		f.uniform1i( lDecals, decals );
		f.uniform2fv( lCell, 1, cell );
		f.bindFramebuffer( kFpDrawFb, fb );
		f.framebufferTexture2D( kFpDrawFb, fCol0, GL_TEXTURE_2D, a.scr, 0 );
		const GLenum one = fCol0;
		f.drawBuffers( 1, &one );
		glViewport( 0, 0, a.side, a.side );
		fp.bindSampler( uLtex, ltexSmp );
		glDrawElements( GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, nullptr );
		fp.bindSampler( uLtex, 0 );
		f.framebufferTexture2D( kFpDrawFb, fCol0, GL_TEXTURE_2D, 0, 0 );
		glBindTexture( GL_TEXTURE_2D, a.scr );
		f.generateMipmap( GL_TEXTURE_2D );
		glBindTexture( GL_TEXTURE_2D, 0 );
	};
	auto encodeGl = [&]( Arm & a, GLuint pool, int layer, int red ) {
		f.useProgram( progBc );
		bindUnit( 0, GL_TEXTURE_2D, a.scr );
		f.uniform1i( lSrc, 0 );
		f.uniform1i( lRed, red );
		f.bindFramebuffer( kFpDrawFb, fbBc );
		const GLenum one = fCol0;
		f.drawBuffers( 1, &one );
		f.bindFramebuffer( kFpReadFb, fbBc );
		glReadBuffer( fCol0 );
		std::vector<std::uint32_t> bl( static_cast<size_t>( bw0 ) * static_cast<size_t>( bw0 ) * 2 );
		for ( int L = 0; L < a.levels; L++ ) {
			const int s = a.side >> L, bw = s / 4;
			f.uniform1i( lLevel, L );
			glViewport( 0, 0, bw, bw );
			glDrawElements( GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, nullptr );
			glReadPixels( 0, 0, bw, bw, kFpRgInt, GL_UNSIGNED_INT, bl.data() );
			putPool( pool, L, s, layer, 1, bl.data() );
		}
		bindUnit( 0, GL_TEXTURE_2D, 0 );
		f.bindFramebuffer( kFpReadFb, 0 );
	};
	auto putPrior = [&]( Arm & a, GLuint pool ) {
		for ( int L = 0; L < a.levels; L++ )
			putPool( pool, L, a.side >> L, 0, kFpLayers, a.prior[static_cast<size_t>( L )].data() );
	};

	Arm arms[2];
	arms[0].name = "N";
	arms[0].side = 256;
	arms[1].name = "F";
	arms[1].side = 128;
	int fails = 0, probesDead = 0, redsRun = 0, redsFired = 0;
	for ( Arm & a : arms ) {
		a.levels = levelsOf( a.side );
		a.key = kOf( 4 + int( &a - arms ) );
		glGenTextures( 1, &a.scr );
		glBindTexture( GL_TEXTURE_2D, a.scr );
		ex.texStorage2D( GL_TEXTURE_2D, a.levels, kFpRgba8, a.side, a.side );
		glTexParameteri( GL_TEXTURE_2D, fMaxLevel, a.levels - 1 );
		glBindTexture( GL_TEXTURE_2D, 0 );
		glGenTextures( 1, &a.glPool );
		glBindTexture( fT2DA, a.glPool );
		ex.texStorage3D( fT2DA, a.levels, kFpBc1, a.side, a.side, kFpLayers );
		glBindTexture( fT2DA, 0 );
		unsigned vp = 0;
		if ( !WwVkPresent::farTwin( a.key, 2, a.side, a.side, kFpLayers, kFpBc1, vp, why, a.levels ) )
			return fatal( std::string( "farTwin pool " ) + a.name + ": " + why );
		a.vkPool = vp;
		std::mt19937 pr( 77u + unsigned( a.side ) );
		a.prior.resize( static_cast<size_t>( a.levels ) );
		for ( int L = 0; L < a.levels; L++ ) {
			const int bw = ( a.side >> L ) / 4;
			auto & v = a.prior[static_cast<size_t>( L )];
			v.resize( static_cast<size_t>( bw ) * static_cast<size_t>( bw ) * 2 * kFpLayers );
			for ( auto & x : v )
				x = std::uint32_t( pr() );
		}
		putPrior( a, a.glPool );
		putPrior( a, a.vkPool );
		if ( glGetError() != GL_NO_ERROR )
			return fatal( std::string( "the pools " ) + a.name + " (GL error)" );

		// ---- GL: paint + encode, twice EXACT; the bound on GL's own encoder; probes
		paintGl( a, 1 );
		encodeGl( a, a.glPool, kFpLayer, 0 );
		std::vector<std::vector<std::uint32_t>> glP( static_cast<size_t>( a.levels ) );
		std::vector<std::vector<std::uint8_t>> page( static_cast<size_t>( a.levels ) );
		std::vector<double> ref( static_cast<size_t>( a.levels ) );
		for ( int L = 0; L < a.levels; L++ ) {
			glP[static_cast<size_t>( L )] = readPool( a.glPool, L );
			page[static_cast<size_t>( L )] = readLevel( a.scr, L, a.side >> L );
			ref[static_cast<size_t>( L )] = refRmse( page[static_cast<size_t>( L )], a.side >> L );
		}
		paintGl( a, 1 );
		encodeGl( a, a.glPool, kFpLayer, 0 );
		int same = 0;
		for ( int L = 0; L < a.levels; L++ )
			same += readPool( a.glPool, L ) == glP[static_cast<size_t>( L )];
		std::printf( "FGPAINTVIEW %s GL twice: %s (%d of %d levels equal)\n", a.name, same == a.levels ? "EXACT" : "FAIL", same, a.levels );
		fails += same != a.levels;
		auto bound = [&]( int L ) { return 1.15 * ref[static_cast<size_t>( L )] + 1.0; };
		int inGl = 0;
		for ( int L = 0; L < a.levels; L++ ) {
			const int s = a.side >> L;
			const double e = blocksRmse( layerOf( glP[static_cast<size_t>( L )], s, kFpLayer ), page[static_cast<size_t>( L )], s );
			const double pe = blocksRmse( layerOf( a.prior[static_cast<size_t>( L )], s, kFpLayer ), page[static_cast<size_t>( L )], s );
			const std::vector<std::uint32_t> zero( static_cast<size_t>( s / 4 ) * static_cast<size_t>( s / 4 ) * 2, 0u );
			const double ze = blocksRmse( zero.data(), page[static_cast<size_t>( L )], s );
			const bool ok = e <= bound( L ), priorLive = pe > bound( L ), zeroLive = ze > bound( L );
			std::printf( "FGPAINTVIEW %s level %d (%d px): GL encode rmse %.3f ref %.3f bound %.3f %s; probe prior %.2f %s, zero blocks %.2f %s\n",
				a.name, L, s, e, ref[static_cast<size_t>( L )], bound( L ), ok ? "IN" : "OVER", pe, priorLive ? "LIVE" : "TESTS NOTHING", ze,
				zeroLive ? "LIVE" : "TESTS NOTHING" );
			inGl += ok;
			probesDead += !priorLive + !zeroLive;
		}
		std::printf( "FGPAINTVIEW %s GL's encoder inside the bound: %d of %d levels %s\n", a.name, inGl, a.levels, inGl == a.levels ? "OK" : "FAIL" );
		fails += inGl != a.levels;
		{	// fgBc1Red 1 breaks the bound at level 0 (into layer 0 of GL's pool, the layer the gate never judges against GL)
			encodeGl( a, a.glPool, 0, 1 );
			const double e = blocksRmse( layerOf( readPool( a.glPool, 0 ), a.side, 0 ), page[0], a.side );
			const bool live = e > bound( 0 );
			std::printf( "FGPAINTVIEW %s probe fgBc1Red 1: level 0 rmse %.3f vs bound %.3f %s\n", a.name, e, bound( 0 ), live ? "LIVE" : "TESTS NOTHING" );
			probesDead += !live;
		}
		{	// decals reach the page; colours
			paintGl( a, 0 );
			const std::vector<std::uint8_t> nd = readLevel( a.scr, 0, a.side );
			int moved = 0;
			std::set<std::uint32_t> cols;
			for ( size_t i = 0; i < nd.size(); i += 4 ) {
				moved += nd[i] != page[0][i] || nd[i + 1] != page[0][i + 1] || nd[i + 2] != page[0][i + 2];
				cols.insert( std::uint32_t( page[0][i] ) | ( std::uint32_t( page[0][i + 1] ) << 8 ) | ( std::uint32_t( page[0][i + 2] ) << 16 ) );
			}
			const int want = a.side >= 256 ? 2000 : 500;
			const bool live = moved >= want, rich = cols.size() >= 1000;
			std::printf( "FGPAINTVIEW %s probe fgPageDecals 0: moved %d of %d texels (>= %d) %s; level 0 colours %zu %s\n", a.name, moved,
				a.side * a.side, want, live ? "LIVE" : "TESTS NOTHING", cols.size(), rich ? "OK" : "FLAT" );
			probesDead += !live;
			fails += !rich;
			paintGl( a, 1 );	// the scratch back to the judged page
		}
		putPrior( a, a.glPool );
		encodeGl( a, a.glPool, kFpLayer, 0 );
		if ( glGetError() != GL_NO_ERROR )
			return fatal( std::string( a.name ) + " GL (GL error)" );

#ifdef WW_VK_FGPAINT
		auto vkRun = [&]( int red, bool encodeOnly, std::string & w, WwVkPresent::FgPaintStat & st ) {
			f.useProgram( progPage );
			( void )WwVkPresent::takeFgPaintStat();
			WwVkPresent::FgPaintRun r;
			r.prog = progPage;
			r.side = a.side;
			r.layer = kFpLayer;
			r.poolKey = a.key;
			r.encodeOnly = encodeOnly;
			r.red = red;
			const bool ok = WwVkPresent::fgPaintRun( r, gw, w );
			st = WwVkPresent::takeFgPaintStat();
			return ok;
		};
		//! every level of the Vulkan pool, read twice; the verdict of P / O against GL's page and the prior
		struct Judged
		{
			std::vector<std::vector<std::uint32_t>> pool;
			int inBound = 0, exactGl = 0, otherExact = 0, twiceBad = 0;
			double worst = 0.0;
			bool ok( int levels ) const { return inBound == levels && otherExact == levels && twiceBad == 0; }
		};
		auto judge = [&]() {
			Judged j;
			for ( int L = 0; L < a.levels; L++ ) {
				const int s = a.side >> L;
				const size_t one = static_cast<size_t>( s / 4 ) * static_cast<size_t>( s / 4 ) * 2;
				std::vector<std::uint32_t> v = readPool( a.vkPool, L );
				j.twiceBad += readPool( a.vkPool, L ) != v;
				const double e = blocksRmse( layerOf( v, s, kFpLayer ), page[static_cast<size_t>( L )], s );
				j.worst = std::max( j.worst, e - bound( L ) );
				j.inBound += e <= bound( L );
				j.exactGl += std::equal( layerOf( v, s, kFpLayer ), layerOf( v, s, kFpLayer ) + one, layerOf( glP[static_cast<size_t>( L )], s, kFpLayer ) );
				bool other = true;
				for ( int l = 0; l < kFpLayers; l++ )
					if ( l != kFpLayer )
						other = other && std::equal( layerOf( v, s, l ), layerOf( v, s, l ) + one, layerOf( a.prior[static_cast<size_t>( L )], s, l ) );
				j.otherExact += other;
				j.pool.push_back( std::move( v ) );
			}
			return j;
		};
		std::string w;
		WwVkPresent::FgPaintStat st;
		if ( !vkRun( 0, false, w, st ) ) {
			std::printf( "FGPAINTVIEW %s: Vulkan refused: %s\n", a.name, w.c_str() );
			fails++;
			continue;
		}
		const Judged first = judge();
		const GLuint vkScr = WwVkPresent::fgPaintPageTex( a.side );
		{	// S0: the page itself
			const std::vector<std::uint8_t> v0 = readLevel( vkScr, 0, a.side ), v1 = readLevel( vkScr, 0, a.side );
			int worst = 0, over = 0;
			for ( size_t i = 0; i < v0.size(); i++ )
				if ( i % 4 != 3 ) {
					const int d = std::abs( int( v0[i] ) - int( page[0][i] ) );
					worst = std::max( worst, d );
					over += d > 2;
				}
			const bool ok = vkScr && over == 0 && v0 == v1;
			std::printf( "FGPAINTVIEW %s S0 page level 0 vs GL: max %d / 255, %d values over 2, read twice %s: %s\n", a.name, worst, over,
				v0 == v1 ? "equal" : "DIFFER", ok ? "OK" : "FAIL" );
			fails += !ok;
		}
		const bool p0 = first.ok( a.levels ) && st.copies == 1;
		std::printf( "FGPAINTVIEW %s P: %d of %d levels inside the bound (worst past it %.3f), %d of %d blocks-EXACT to GL (info); O: %d of %d "
					 "levels' other layers EXACT; read twice differ %d; copies %d (bar 1): %s\n",
			a.name, first.inBound, a.levels, first.worst, first.exactGl, a.levels, first.otherExact, a.levels, first.twiceBad, st.copies,
			p0 ? "OK" : "FAIL" );
		fails += !p0;
		{	// E: GL's scratch into the route's, encode only: EXACT to GL's blocks
			for ( int L = 0; L < a.levels; L++ )
				f.copyImageSubData( a.scr, GL_TEXTURE_2D, L, 0, 0, 0, vkScr, GL_TEXTURE_2D, L, 0, 0, 0, a.side >> L, a.side >> L, 1 );
			const bool ran = vkRun( 0, true, w, st );
			int exact = 0;
			for ( int L = 0; L < a.levels && ran; L++ ) {
				const int s = a.side >> L;
				const size_t one = static_cast<size_t>( s / 4 ) * static_cast<size_t>( s / 4 ) * 2;
				const std::vector<std::uint32_t> v = readPool( a.vkPool, L );
				exact += readPool( a.vkPool, L ) == v
					&& std::equal( layerOf( v, s, kFpLayer ), layerOf( v, s, kFpLayer ) + one, layerOf( glP[static_cast<size_t>( L )], s, kFpLayer ) );
			}
			const bool ok = ran && exact == a.levels;
			std::printf( "FGPAINTVIEW %s E encode only: %d of %d levels EXACT to GL's blocks %s%s\n", a.name, exact, a.levels, ok ? "OK" : "FAIL",
				ran ? "" : ( " (refused: " + w + ")" ).c_str() );
			fails += !ok;
		}
		const struct
		{
			int bit;
			const char * what;
		} reds[3] = { { 1, "wrong page (layer 2)" }, { 2, "stale page (copy dropped)" }, { 4, "encode skipped (zero blocks)" } };
		for ( const auto & rd : reds ) {
			putPrior( a, a.vkPool );
			const bool ran = vkRun( rd.bit, false, w, st );
			const Judged j = judge();
			const bool fired = !ran || !j.ok( a.levels );
			std::printf( "FGPAINTVIEW %s red %d %s: inside the bound %d of %d, other layers EXACT %d of %d -> %s\n", a.name, rd.bit, rd.what,
				j.inBound, a.levels, j.otherExact, a.levels, fired ? "FAILS as it must" : "PASSED (red tests nothing)" );
			redsRun++;
			redsFired += fired;
		}
		putPrior( a, a.vkPool );
		{
			const bool ran = vkRun( 0, false, w, st );
			const Judged j = judge();
			const bool ok = ran && j.pool == first.pool;
			std::printf( "FGPAINTVIEW %s rerun after the reds: %s\n", a.name, ok ? "EXACT" : "FAIL" );
			fails += !ok;
		}
#endif
	}
	std::printf( "FGPAINTVIEW GL: %d fails, %d probes TESTS NOTHING\n", fails, probesDead );
#ifdef WW_VK_FGPAINT
	const bool ok = fails == 0 && probesDead == 0 && redsRun > 0 && redsFired == redsRun;
	std::printf( "FGPAINTVIEW %d fails, %d dead probes, reds %d of %d fired\n", fails, probesDead, redsFired, redsRun );
	std::printf( "FGPAINTVIEW GATE %s\n", ok ? "OK" : "FAILED" );
	return ok ? 0 : 1;
#else
	( void )redsRun;
	( void )redsFired;
	std::printf( "FGPAINTVIEW GATE FAILED (no route: WW_VK_FGPAINT not defined; GL clauses and probes above)\n" );
	return 1;
#endif
}
