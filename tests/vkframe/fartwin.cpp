// Lane VKFAR1 round 2 (2026-11-13, row G3): the far cards' BC sheets and the cube-array shadow (vkfarcards.inc farTwin)
// on the present device. GL writes known bytes into each twin as into its own texture (glCompressedTexSubImage2D/3D for
// BC1 / BC3 / BC5 / BC7 / BC7 sRGB, glTexSubImage3D for a GL_DEPTH_COMPONENT16 cube-map array); after glFinish the present
// device copies every level and layer out (farTwinRead) and the bytes must be the same, byte for byte.
// Bar (set before the first run): every compared level / layer EXACT; GL raised no error; a GL format with no twin refused.
// Reds (each must FAIL): the layer one off (a cube face order / layer mapping slip), the level one off, the twin made as
// another format (BC1 for a BC3 sheet).
// Texel buffers (farTexelBuffer: cellLights RGBA32F, cellCluster RGBA32I): GL writes with glBufferSubData into the imported
// buffer, its GL_TEXTURE_BUFFER view must name that buffer and format, the present device's bytes must be the same.
// Reds: a texel one off, the buffer remade at half the size. Last line `FARTWIN GATE OK` or `FARTWIN GATE FAILED`.
#define main fxview_main
#include "fxview.cpp"
#undef main

#include <functional>
#include <random>

namespace fartw
{
using namespace fv;
using CTex2D = void( WINAPI * )( GLenum, GLint, GLint, GLint, GLsizei, GLsizei, GLenum, GLsizei, const void * );
using CTex3D = void( WINAPI * )( GLenum, GLint, GLint, GLint, GLint, GLsizei, GLsizei, GLsizei, GLenum, GLsizei, const void * );
CTex2D cSub2D = nullptr;
CTex3D cSub3D = nullptr;

struct Case
{
	const char * name;
	int type;  // 0 2D, 2 2D array, 3 cube-map array
	int w, h, layers, levels;
	unsigned fmt;
	int block;  // bytes a 4x4 block; 0 = D16
	std::vector<std::vector<std::vector<std::uint8_t>>> data;  // [level][layer]
	char key = 0;
	unsigned tex = 0;
};

size_t levelBytes( const Case & c, int level )
{
	const int w = std::max( c.w >> level, 1 ), h = std::max( c.h >> level, 1 );
	return c.block ? size_t( ( w + 3 ) / 4 ) * size_t( ( h + 3 ) / 4 ) * size_t( c.block ) : size_t( w ) * size_t( h ) * 2;
}

GLenum target( const Case & c )
{
	return c.type == 3 ? 0x9009 : c.type == 2 ? fT2DA : GL_TEXTURE_2D;
}

void upload( Case & c, std::mt19937 & rng )
{
	glBindTexture( target( c ), c.tex );
	c.data.assign( size_t( c.levels ), {} );
	for ( int l = 0; l < c.levels; l++ ) {
		const int w = std::max( c.w >> l, 1 ), h = std::max( c.h >> l, 1 );
		for ( int z = 0; z < c.layers; z++ ) {
			std::vector<std::uint8_t> b( levelBytes( c, l ) );
			for ( auto & x : b )
				x = std::uint8_t( rng() );
			if ( c.type == 0 )
				cSub2D( GL_TEXTURE_2D, l, 0, 0, w, h, c.fmt, GLsizei( b.size() ), b.data() );
			else if ( c.block )
				cSub3D( target( c ), l, 0, 0, z, w, h, 1, c.fmt, GLsizei( b.size() ), b.data() );
			else
				f.texSubImage3D( target( c ), l, 0, 0, z, w, h, 1, GL_DEPTH_COMPONENT, GL_UNSIGNED_SHORT, b.data() );
			c.data[size_t( l )].push_back( std::move( b ) );
		}
	}
	glBindTexture( target( c ), 0 );
}

// bytes that differ (a size mismatch counts every byte of the longer side)
size_t differ( const std::vector<std::uint8_t> & a, const std::vector<std::uint8_t> & b )
{
	if ( a.size() != b.size() )
		return std::max( a.size(), b.size() );
	size_t n = 0;
	for ( size_t i = 0; i < a.size(); i++ )
		n += a[i] != b[i];
	return n;
}
}  // namespace fartw

namespace fartw
{
// the HLSL's own declarations (register lines) as the set should hold them after fx()'s shifts
std::map<std::uint32_t, WwVkPresent::FarSetSlot> hlslSlots( const std::string & path )
{
	std::map<std::uint32_t, WwVkPresent::FarSetSlot> m;
	std::istringstream in( readFile( path ) );
	std::string line;
	while ( std::getline( in, line ) ) {
		const size_t r = line.find( "register(" );
		if ( r == std::string::npos )
			continue;
		const char c = line[r + 9];
		const std::uint32_t n = std::uint32_t( std::atoi( line.c_str() + r + 10 ) );
		WwVkPresent::FarSetSlot s;
		auto has = [&]( const char * k ) { return line.compare( 0, std::strlen( k ), k ) == 0; };
		if ( c == 'b' && has( "cbuffer" ) ) {
			s.binding = n;
			s.kind = 0;
		} else if ( c == 's' ) {
			s.binding = 128 + n;
			s.kind = 3;
			s.compare = has( "SamplerComparisonState" );
		} else if ( c == 't' ) {
			s.binding = 64 + n;
			s.kind = has( "Buffer<" ) ? 2 : 1;
			s.dim = has( "Buffer<" ) ? 5 : has( "TextureCube" ) ? 3 : has( "Texture3D" ) ? 2 : has( "Texture1D" ) ? 0 : 1;
			s.arrayed = line.find( "Array<" ) != std::string::npos && line.find( "Array<" ) < r;
		} else
			continue;
		m[s.binding] = s;
	}
	return m;
}

// reflected against declared: the bindings that differ (missing, extra, or another kind / dim / arrayed / compare)
size_t setDiff( const WwVkPresent::FarSetCounts & got, const std::map<std::uint32_t, WwVkPresent::FarSetSlot> & want, bool print )
{
	size_t bad = 0;
	std::map<std::uint32_t, WwVkPresent::FarSetSlot> g;
	for ( const auto & s : got.slots )
		g[s.binding] = s;
	for ( const auto & kv : want ) {
		auto it = g.find( kv.first );
		const auto & w = kv.second;
		if ( it == g.end() || it->second.kind != w.kind || it->second.dim != w.dim || it->second.arrayed != w.arrayed
			|| it->second.compare != w.compare ) {
			bad++;
			if ( print )
				std::printf( "FARTWIN set binding %u: declared kind %d dim %d arrayed %d compare %d, module %s\n", kv.first, w.kind, w.dim,
					int( w.arrayed ), int( w.compare ), it == g.end() ? "has none" : "differs" );
		}
	}
	for ( const auto & kv : g )
		if ( !want.count( kv.first ) ) {
			bad++;
			if ( print )
				std::printf( "FARTWIN set binding %u: in the module, not declared\n", kv.first );
		}
	return bad;
}
}  // namespace fartw

int main( int argc, char ** argv )
{
	using namespace fartw;
	setvbuf( stdout, nullptr, _IONBF, 0 );
	auto fatal = []( const std::string & m ) {
		std::printf( "FARTWIN %s\nFARTWIN GATE FAILED\n", m.c_str() );
		return 2;
	};
	gOpenGl = LoadLibraryA( "opengl32.dll" );
	WNDCLASSA wc {};
	wc.style = CS_OWNDC;
	wc.lpfnWndProc = wndProc;
	wc.hInstance = GetModuleHandleA( nullptr );
	wc.lpszClassName = "wwfartwin";
	RegisterClassA( &wc );
	HWND wnd = CreateWindowExA( WS_EX_NOACTIVATE, "wwfartwin", "far twin harness", WS_OVERLAPPEDWINDOW, 40, 40, 160, 90, nullptr,
		nullptr, wc.hInstance, nullptr );
	HDC dc = GetDC( wnd );
	PIXELFORMATDESCRIPTOR pfd { sizeof( pfd ), 1, PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER, PFD_TYPE_RGBA, 32 };
	SetPixelFormat( dc, ChoosePixelFormat( dc, &pfd ), &pfd );
	HGLRC glrc = wglCreateContext( dc );
	wglMakeCurrent( dc, glrc );
	pump();
	if ( !loadAll() || !load( cSub2D, "glCompressedTexSubImage2D" ) || !load( cSub3D, "glCompressedTexSubImage3D" ) )
		return fatal( "GL entry points" );
	std::printf( "FARTWIN GL on %s\n", reinterpret_cast<const char *>( glGetString( GL_RENDERER ) ) );
	glPixelStorei( GL_PACK_ALIGNMENT, 1 );
	glPixelStorei( GL_UNPACK_ALIGNMENT, 1 );
	std::string why;
	if ( !WwVkPresent::viewBegin( 160, 90, 1, glProc, why ) )
		return fatal( "the view: " + why );
	std::vector<Case> cs;
	cs.push_back( { "BC1 2D 64x32, 3 levels", 0, 64, 32, 1, 3, 0x83F1, 8 } );
	cs.push_back( { "BC3 2D array 32x32x5", 2, 32, 32, 5, 1, 0x83F3, 16 } );
	cs.push_back( { "BC5 2D array 16x48x2, 2 levels", 2, 16, 48, 2, 2, 0x8DBD, 16 } );
	cs.push_back( { "BC7 2D 128x64, 4 levels", 0, 128, 64, 1, 4, 0x8E8C, 16 } );
	cs.push_back( { "BC7 sRGB 2D array 64x64x3", 2, 64, 64, 3, 1, 0x8E8D, 16 } );
	cs.push_back( { "D16 cube-map array 32x32, 2 cubes", 3, 32, 32, 12, 1, 0x81A5, 0 } );
	std::mt19937 rng( 1113u );
	bool ok = true;
	while ( glGetError() != GL_NO_ERROR ) {
	}
	for ( auto & c : cs ) {
		if ( !WwVkPresent::farTwin( reinterpret_cast<std::uintptr_t>( &c.key ), c.type, c.w, c.h, c.layers, c.fmt, c.tex, why, c.levels ) )
			return fatal( std::string( "twin " ) + c.name + ": " + why );
		upload( c, rng );
		const GLenum e = glGetError();
		if ( e != GL_NO_ERROR ) {
			std::printf( "FARTWIN %s: GL error 0x%04X on the upload\n", c.name, unsigned( e ) );
			ok = false;
		}
	}
	glFinish();
	for ( auto & c : cs ) {
		size_t bad = 0, bytes = 0, parts = 0;
		for ( int l = 0; l < c.levels; l++ )
			for ( int z = 0; z < c.layers; z++ ) {
				std::vector<std::uint8_t> got;
				if ( !WwVkPresent::farTwinRead( reinterpret_cast<std::uintptr_t>( &c.key ), l, z, got, why ) ) {
					std::printf( "FARTWIN %s: read level %d layer %d failed: %s\n", c.name, l, z, why.c_str() );
					ok = false;
					continue;
				}
				bad += differ( got, c.data[size_t( l )][size_t( z )] );
				bytes += got.size();
				parts++;
			}
		std::printf( "FARTWIN %s: %zu levels x layers, %zu bytes, %zu differ: %s\n", c.name, parts, bytes, bad, bad ? "FAIL" : "EXACT" );
		ok = ok && bad == 0 && parts == size_t( c.levels * c.layers );
	}
	{
		unsigned t = 0;
		std::string w2;
		char k = 0;
		const bool made = WwVkPresent::farTwin( reinterpret_cast<std::uintptr_t>( &k ), 0, 16, 16, 1, 0x8051 /* GL_RGB8 */, t, w2 );
		std::printf( "FARTWIN a format with no twin (GL_RGB8): %s (%s)\n", made ? "MADE (wrong)" : "refused", w2.c_str() );
		if ( made ) {
			ok = false;
			WwVkPresent::farTwinDrop( reinterpret_cast<std::uintptr_t>( &k ) );
		}
	}
	// reds: each must FAIL
	auto red = [&]( const char * name, size_t bad ) {
		std::printf( "FARTWIN red %s: %s (%zu bytes differ)\n", name, bad ? "FAIL" : "EXACT", bad );
		if ( !bad ) {
			std::printf( "FARTWIN red %s PASSED: the gate cannot see it\n", name );
			ok = false;
		}
	};
	{
		Case & c = cs[5];
		std::vector<std::uint8_t> got;
		WwVkPresent::farTwinRead( reinterpret_cast<std::uintptr_t>( &c.key ), 0, 7, got, why );
		red( "layer one off (cube 1 face 1 read as face 2)", differ( got, c.data[0][8] ) );
	}
	{
		Case & c = cs[3];
		std::vector<std::uint8_t> got;
		WwVkPresent::farTwinRead( reinterpret_cast<std::uintptr_t>( &c.key ), 1, 0, got, why );
		red( "level one off (level 1 read as level 2)", differ( got, c.data[2][0] ) );
	}
	{
		Case & c = cs[1];
		unsigned t = 0;
		WwVkPresent::farTwin( reinterpret_cast<std::uintptr_t>( &c.key ), c.type, c.w, c.h, c.layers, 0x83F1, t, why );
		std::vector<std::uint8_t> got;
		WwVkPresent::farTwinRead( reinterpret_cast<std::uintptr_t>( &c.key ), 0, 0, got, why );
		red( "the twin made as BC1 for a BC3 sheet", differ( got, c.data[0][0] ) );
	}
	// texel buffers (cellLights float4, cellCluster int4): GL writes with glBufferSubData, the present device reads back
	{
		using BufSub = void( WINAPI * )( GLenum, std::intptr_t, std::intptr_t, const void * );
		BufSub bufSub = nullptr;
		void( WINAPI * bindBuf )( GLenum, GLuint ) = nullptr;
		if ( !load( bufSub, "glBufferSubData" ) || !load( bindBuf, "glBindBuffer" ) )
			return fatal( "glBufferSubData" );
		std::printf( "FARTWIN texel buffers: the device holds %u texels a buffer\n", WwVkPresent::farTexelBufferMax() );
		struct TB
		{
			const char * name;
			unsigned fmt;
			size_t bytes;
			char key = 0;
			unsigned buf = 0, tex = 0;
			std::vector<std::uint8_t> data;
		};
		TB tb[2] = { { "cellLights RGBA32F, 4096 texels", 0x8814, 4096 * 16 }, { "cellCluster RGBA32I, 24576 texels", 0x8D82, 24576 * 16 } };
		for ( auto & b : tb ) {
			if ( !WwVkPresent::farTexelBuffer( reinterpret_cast<std::uintptr_t>( &b.key ), b.fmt, b.bytes, b.buf, b.tex, why ) )
				return fatal( std::string( "texel buffer " ) + b.name + ": " + why );
			b.data.resize( b.bytes );
			for ( auto & x : b.data )
				x = std::uint8_t( rng() );
			bindBuf( 0x8C2A, b.buf );
			bufSub( 0x8C2A, 0, std::intptr_t( b.bytes / 2 ), b.data.data() );  // two writes: the second at an offset
			bufSub( 0x8C2A, std::intptr_t( b.bytes / 2 ), std::intptr_t( b.bytes - b.bytes / 2 ), b.data.data() + b.bytes / 2 );
			bindBuf( 0x8C2A, 0 );
			glBindTexture( 0x8C2A, b.tex );
			GLint store = 0, ifmt = 0;
			glGetTexLevelParameteriv( 0x8C2A, 0, 0x8C2D, &store );  // GL_TEXTURE_BUFFER_DATA_STORE_BINDING
			glGetTexLevelParameteriv( 0x8C2A, 0, GL_TEXTURE_INTERNAL_FORMAT, &ifmt );
			glBindTexture( 0x8C2A, 0 );
			const GLenum e = glGetError();
			std::printf( "FARTWIN %s: GL view on buffer %d (want %u), format 0x%04X, GL error 0x%04X\n", b.name, store, b.buf, unsigned( ifmt ),
				unsigned( e ) );
			ok = ok && e == GL_NO_ERROR && GLuint( store ) == b.buf && unsigned( ifmt ) == b.fmt;
		}
		glFinish();
		for ( auto & b : tb ) {
			std::vector<std::uint8_t> got;
			const bool read = WwVkPresent::farTexelBufferRead( reinterpret_cast<std::uintptr_t>( &b.key ), got, why );
			const size_t bad = read ? differ( got, b.data ) : b.bytes;
			std::printf( "FARTWIN %s: %zu bytes, %zu differ: %s\n", b.name, got.size(), bad, bad ? "FAIL" : "EXACT" );
			ok = ok && bad == 0;
		}
		{
			unsigned gb = 0, gt = 0;
			char k = 0;
			const bool made = WwVkPresent::farTexelBuffer( reinterpret_cast<std::uintptr_t>( &k ), 0x8058 /* GL_RGBA8 */, 64, gb, gt, why );
			std::printf( "FARTWIN a texel buffer format with no twin (GL_RGBA8): %s\n", made ? "MADE (wrong)" : "refused" );
			ok = ok && !made;
		}
		{
			std::vector<std::uint8_t> got, want( tb[0].data.begin() + 16, tb[0].data.end() );
			WwVkPresent::farTexelBufferRead( reinterpret_cast<std::uintptr_t>( &tb[0].key ), got, why );
			got.resize( got.size() - 16 );
			const size_t bad = differ( got, want );
			std::printf( "FARTWIN red texel one off (cellLights read a texel late): %s (%zu bytes differ)\n", bad ? "FAIL" : "EXACT", bad );
			if ( !bad )
				ok = false;
		}
		{
			TB & b = tb[1];
			WwVkPresent::farTexelBuffer( reinterpret_cast<std::uintptr_t>( &b.key ), b.fmt, b.bytes / 2, b.buf, b.tex, why );
			std::vector<std::uint8_t> got;
			WwVkPresent::farTexelBufferRead( reinterpret_cast<std::uintptr_t>( &b.key ), got, why );
			const size_t bad = differ( got, b.data );
			std::printf( "FARTWIN red the buffer remade at half the size: %s (%zu bytes differ)\n", bad ? "FAIL" : "EXACT", bad );
			if ( !bad )
				ok = false;
		}
		for ( auto & b : tb )
			WwVkPresent::farTexelBufferDrop( reinterpret_cast<std::uintptr_t>( &b.key ) );
	}
	// the far cards' set: reflected from the shipped module, against the HLSL's declarations and the counts the pass needs
	{
		if ( argc < 2 )
			return fatal( "usage: fartwin.exe <tree root>" );
		const auto want = hlslSlots( std::string( argv[1] ) + "/res/shaders/hlsl/far-lod/impostor_cellinstez_ps__WW_CELL_FOGLEAK.hlsl" );
		WwVkPresent::FarSetCounts sc;
		const bool made = WwVkPresent::farCardsSet( sc, why );
		int tSlots = 0;
		bool b0 = false, b31 = false, cubeAt74 = false, compareAt138 = false;
		for ( const auto & s : sc.slots ) {
			tSlots += s.binding >= 64 && s.binding < 128;
			b0 = b0 || ( s.binding == 0 && s.kind == 0 );
			b31 = b31 || ( s.binding == 31 && s.kind == 0 );
			cubeAt74 = cubeAt74 || ( s.binding == 74 && s.kind == 1 && s.dim == 3 && s.arrayed );
			compareAt138 = compareAt138 || ( s.binding == 138 && s.compare );
		}
		const size_t diff = made ? setDiff( sc, want, true ) : want.size();
		std::printf( "FARTWIN set: %s (%s); %zu bindings (HLSL declares %zu), %d uniform buffers, %d images, %d texel buffers, %d samplers "
			"(%d compare), %d cube arrays, %d t-registers; b0 %d b31 %d cube array at t10 %d compare sampler at s10 %d; %zu differ\n",
			made ? "made" : "FAILED", made ? "layout, pool, set, pipeline layout, module" : why.c_str(), sc.slots.size(), want.size(), sc.ubos,
			sc.images, sc.texelBuffers, sc.samplers, sc.compareSamplers, sc.cubeArrays, tSlots, int( b0 ), int( b31 ), int( cubeAt74 ),
			int( compareAt138 ), diff );
		ok = ok && made && diff == 0 && tSlots == 26 && sc.texelBuffers == 4 && sc.cubeArrays == 1 && sc.compareSamplers == 1 && b0 && b31
			&& cubeAt74 && compareAt138;
		// reds: altered modules must be refused or differ from the declarations
		const std::vector<std::uint32_t> base( vkframe_farcards_view_ps_spv,
			vkframe_farcards_view_ps_spv + sizeof( vkframe_farcards_view_ps_spv ) / 4 );
		auto redRun = [&]( const char * name, const std::function<int( std::vector<std::uint32_t> & )> & alter ) {
			std::vector<std::uint32_t> w = base;
			const int hits = alter( w );
			WwVkPresent::FarSetCounts rc;
			std::string rw;
			const bool refl = WwVkPresent::farSetReflect( w.data(), w.size(), rc, rw );
			const size_t d = refl ? setDiff( rc, want, false ) : want.size();
			std::printf( "FARTWIN red %s (%d words altered): %s\n", name, hits, !refl ? ( "refused: " + rw ).c_str() : d ? "differs" : "SAME" );
			if ( !hits || ( refl && !d ) ) {
				std::printf( "FARTWIN red %s PASSED: the gate cannot see it\n", name );
				ok = false;
			}
		};
		auto walk = [&]( std::vector<std::uint32_t> & w, const std::function<int( std::uint32_t, std::uint32_t *, std::uint32_t )> & f ) {
			int hits = 0;
			for ( size_t i = 5; i < w.size(); ) {
				const std::uint32_t wc = w[i] >> 16;
				if ( !wc )
					break;
				hits += f( w[i] & 0xFFFFu, &w[i], wc );
				i += wc;
			}
			return hits;
		};
		redRun( "the shadow's cube array typed 2D array", [&]( std::vector<std::uint32_t> & w ) {
			return walk( w, []( std::uint32_t op, std::uint32_t * p, std::uint32_t wc ) {
				if ( op == 25 && wc >= 9 && p[3] == 3 && p[5] == 1 ) {
					p[3] = 1;
					return 1;
				}
				return 0;
			} );
		} );
		redRun( "cellCluster's binding moved onto cellLights'", [&]( std::vector<std::uint32_t> & w ) {
			return walk( w, []( std::uint32_t op, std::uint32_t * p, std::uint32_t wc ) {
				if ( op == 71 && wc >= 4 && p[2] == 33 && p[3] == 70 ) {
					p[3] = 73;
					return 1;
				}
				return 0;
			} );
		} );
		redRun( "the depth-compare samples made plain", [&]( std::vector<std::uint32_t> & w ) {
			return walk( w, []( std::uint32_t op, std::uint32_t * p, std::uint32_t ) {
				if ( op == 89 || op == 90 ) {
					p[0] = ( p[0] & 0xFFFF0000u ) | ( op - 2 );  // OpImageSampleImplicitLod / ExplicitLod
					return 1;
				}
				return 0;
			} );
		} );
		redRun( "cellFarRecs typed Texture2D", [&]( std::vector<std::uint32_t> & w ) {
			// the image type of binding 77's variable: follow decoration -> variable -> pointer -> image
			std::uint32_t var = 0, ptr = 0, img = 0;
			walk( w, [&]( std::uint32_t op, std::uint32_t * p, std::uint32_t wc ) {
				if ( op == 71 && wc >= 4 && p[2] == 33 && p[3] == 77 )
					var = p[1];
				return 0;
			} );
			walk( w, [&]( std::uint32_t op, std::uint32_t * p, std::uint32_t wc ) {
				if ( op == 59 && wc >= 4 && p[2] == var )
					ptr = p[1];
				return 0;
			} );
			walk( w, [&]( std::uint32_t op, std::uint32_t * p, std::uint32_t wc ) {
				if ( op == 32 && wc >= 4 && p[1] == ptr )
					img = p[3];
				return 0;
			} );
			return walk( w, [&]( std::uint32_t op, std::uint32_t * p, std::uint32_t wc ) {
				if ( op == 25 && wc >= 9 && p[1] == img && p[3] == 5 ) {
					p[3] = 1;  // shared with the other float4 buffers: they all change, the set must still differ
					return 1;
				}
				return 0;
			} );
		} );
	}
	const WwVkPresent::FarTwinStat st = WwVkPresent::farTwinStat();
	std::printf( "FARTWIN stat: %d made, %d dropped, %d fails (%s), %.2f MB\n", st.made, st.dropped, st.fails, st.failWhy.c_str(), st.mb );
	for ( auto & c : cs )
		WwVkPresent::farTwinDrop( reinterpret_cast<std::uintptr_t>( &c.key ) );
	WwVkPresent::shutdown();
	wglMakeCurrent( nullptr, nullptr );
	wglDeleteContext( glrc );
	DestroyWindow( wnd );
	std::printf( "%s\n", ok ? "FARTWIN GATE OK" : "FARTWIN GATE FAILED" );
	return ok ? 0 : 1;
}
