// Lane VKFAR17 (2026-11-15, route (a) steps 1 and 2): the probe relight's shared inputs on the present device.
// Step 1: GL_R11F_G11F_B10F (the reflection probe atlas, cellreflprobe.cpp: 512 x 320, kLods + 1 = 5 levels) as a
//   sharedTarget twin (vkFormatOf 0x8C3A -> VK_FORMAT_B10G11R11_UFLOAT_PACK32).
// Step 2: COLOUR cube arrays as sharedTarget type 3 (rpAlb RGBA8, rpNd RGBA16F: 64 x 64 x 6 * 33 layers, GL's capture fills
//   them a layer at a time through glFramebufferTextureLayer).
// GL writes known bytes into each twin as into its own texture (glTexSubImage2D / 3D; no NaN / inf patterns), then after
// glFinish the present device copies every level and layer out and the bytes must be the same.
// Bar (set before the first run, never widened): every compared level / layer EXACT; GL raised no error; the atlas level 0
// and a cube layer attach as a complete framebuffer; a GL clear of the atlas to (1, 2, 0.5) reads back on the present
// device as B10G11R11 0x702003C0 (R bits 0-10, G 11-21, B 22-31; the header said 0x381C03C0 by a hand slip, the code
// always computed the word from the exponents) at level 0 AND at level 4 after glGenerateMipmap; a GL
// clear of cube layer 7 reads back as the clear colour on layer 7 while layers 6 and 8 keep their uploaded bytes;
// a non-square cube array and a cube array of 7 layers are refused.
// Reds (each must FAIL): the layer one off (layer 8 read as the cleared layer), the level one off (atlas level 1 read as
// level 2), the channel order swapped (the clear packed with R and B exchanged).
// Last line `SHAREDFMT GATE OK` or `SHAREDFMT GATE FAILED`.
#define main fxview_main
#include "fxview.cpp"
#undef main

#include <random>

namespace sfmt
{
using namespace fv;

struct Case
{
	const char * name;
	int type;  // 0 2D, 3 cube-map array
	int w, h, layers, levels;
	unsigned fmt;
	int bpt;  // bytes a texel
	std::vector<std::vector<std::vector<std::uint8_t>>> data;  // [level][layer]
	char key = 0;
	unsigned tex = 0;
};

GLenum target( const Case & c )
{
	return c.type == 3 ? 0x9009 : GL_TEXTURE_2D;
}

// random texels with no inf / NaN pattern (an exponent of all ones is moved one down)
void fill( std::vector<std::uint8_t> & b, unsigned fmt, std::mt19937 & rng )
{
	for ( auto & x : b )
		x = std::uint8_t( rng() );
	if ( fmt == 0x8C3A ) {
		for ( size_t i = 0; i + 4 <= b.size(); i += 4 ) {
			std::uint32_t p;
			std::memcpy( &p, &b[i], 4 );
			const int sh[3] = { 6, 17, 27 };  // the exponents of R (11 bit), G (11 bit), B (10 bit)
			for ( int s : sh )
				if ( ( ( p >> s ) & 31u ) == 31u )
					p &= ~( 1u << s );
			std::memcpy( &b[i], &p, 4 );
		}
	} else if ( fmt == 0x881A ) {
		for ( size_t i = 0; i + 2 <= b.size(); i += 2 ) {
			std::uint16_t h;
			std::memcpy( &h, &b[i], 2 );
			if ( ( ( h >> 10 ) & 31u ) == 31u )
				h = std::uint16_t( h & ~( 1u << 10 ) );
			std::memcpy( &b[i], &h, 2 );
		}
	}
}

void upload( Case & c, std::mt19937 & rng )
{
	const GLenum pf = c.fmt == 0x8C3A ? GL_RGB : GL_RGBA;
	const GLenum ty = c.fmt == 0x8C3A ? 0x8C3B /* GL_UNSIGNED_INT_10F_11F_11F_REV */ : c.fmt == 0x881A ? 0x140B /* GL_HALF_FLOAT */
		: GL_UNSIGNED_BYTE;
	glBindTexture( target( c ), c.tex );
	c.data.assign( size_t( c.levels ), {} );
	for ( int l = 0; l < c.levels; l++ ) {
		const int w = std::max( c.w >> l, 1 ), h = std::max( c.h >> l, 1 );
		for ( int z = 0; z < c.layers; z++ ) {
			std::vector<std::uint8_t> b( size_t( w ) * size_t( h ) * size_t( c.bpt ) );
			fill( b, c.fmt, rng );
			if ( c.type == 0 )
				glTexSubImage2D( GL_TEXTURE_2D, l, 0, 0, w, h, pf, ty, b.data() );
			else
				f.texSubImage3D( target( c ), l, 0, 0, z, w, h, 1, pf, ty, b.data() );
			c.data[size_t( l )].push_back( std::move( b ) );
		}
	}
	glBindTexture( target( c ), 0 );
}

// one level of one layer of a shared target as the present device sees it (GL finished first)
bool readShared( std::uintptr_t key, int level, int layer, int bpt, std::vector<std::uint8_t> & out, std::string & why )
{
	Vk & v = vk();
	const Shared * s = sharedFind( key );
	if ( !s || !s->img ) {
		why = "no shared target of that key";
		return false;
	}
	const int lw = std::max( s->w >> level, 1 ), lh = std::max( s->h >> level, 1 );
	const VkDeviceSize bytes = VkDeviceSize( lw ) * VkDeviceSize( lh ) * VkDeviceSize( bpt );
	drain();
	v.vkDeviceWaitIdle( v.dev );
	VkBuffer buf = VK_NULL_HANDLE;
	VkDeviceMemory mem = VK_NULL_HANDLE;
	VkBufferCreateInfo bci { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
	bci.size = bytes;
	bci.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	bool ok = v.vkCreateBuffer( v.dev, &bci, nullptr, &buf ) == VK_SUCCESS;
	if ( ok ) {
		VkMemoryRequirements br;
		v.vkGetBufferMemoryRequirements( v.dev, buf, &br );
		VkMemoryAllocateInfo ai { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
		ai.allocationSize = br.size;
		ai.memoryTypeIndex = memType( br.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT );
		ok = ai.memoryTypeIndex != UINT32_MAX && v.vkAllocateMemory( v.dev, &ai, nullptr, &mem ) == VK_SUCCESS
			&& v.vkBindBufferMemory( v.dev, buf, mem, 0 ) == VK_SUCCESS;
	}
	VkCommandBufferAllocateInfo cai { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
	cai.commandPool = v.pool;
	cai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	cai.commandBufferCount = 1;
	VkCommandBuffer cmd = VK_NULL_HANDLE;
	VkFence fence = VK_NULL_HANDLE;
	VkFenceCreateInfo fi { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
	ok = ok && v.vkAllocateCommandBuffers( v.dev, &cai, &cmd ) == VK_SUCCESS && v.vkCreateFence( v.dev, &fi, nullptr, &fence ) == VK_SUCCESS;
	if ( ok ) {
		VkCommandBufferBeginInfo bi { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
		bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		v.vkBeginCommandBuffer( cmd, &bi );
		VkBufferImageCopy c {};
		c.imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, std::uint32_t( level ), std::uint32_t( layer ), 1 };
		c.imageExtent = { std::uint32_t( lw ), std::uint32_t( lh ), 1 };
		v.vkCmdCopyImageToBuffer( cmd, s->img, VK_IMAGE_LAYOUT_GENERAL, buf, 1, &c );
		v.vkEndCommandBuffer( cmd );
		ok = oneShot( cmd, fence );
	}
	if ( ok ) {
		void * p = nullptr;
		ok = v.vkMapMemory( v.dev, mem, 0, bytes, 0, &p ) == VK_SUCCESS;
		if ( ok ) {
			out.assign( static_cast<const std::uint8_t *>( p ), static_cast<const std::uint8_t *>( p ) + bytes );
			v.vkUnmapMemory( v.dev, mem );
		}
	}
	if ( cmd )
		v.vkFreeCommandBuffers( v.dev, v.pool, 1, &cmd );
	if ( fence )
		v.vkDestroyFence( v.dev, fence, nullptr );
	if ( buf )
		v.vkDestroyBuffer( v.dev, buf, nullptr );
	if ( mem )
		v.vkFreeMemory( v.dev, mem, nullptr );
	if ( !ok )
		why = "the read back failed";
	return ok;
}

size_t differ( const std::vector<std::uint8_t> & a, const std::vector<std::uint8_t> & b )
{
	if ( a.size() != b.size() )
		return std::max( a.size(), b.size() );
	size_t n = 0;
	for ( size_t i = 0; i < a.size(); i++ )
		n += a[i] != b[i];
	return n;
}

// texels of a level that are not the packed word p (4-byte texels)
size_t notWord( const std::vector<std::uint8_t> & a, std::uint32_t p )
{
	if ( a.empty() || a.size() % 4 )
		return a.size() + 1;
	size_t n = 0;
	for ( size_t i = 0; i < a.size(); i += 4 ) {
		std::uint32_t q;
		std::memcpy( &q, &a[i], 4 );
		n += q != p;
	}
	return n;
}

// texels of a level that are not the bytes t (one texel)
size_t notTexel( const std::vector<std::uint8_t> & a, const std::vector<std::uint8_t> & t )
{
	if ( a.empty() || a.size() % t.size() )
		return a.size() + 1;
	size_t n = 0;
	for ( size_t i = 0; i < a.size(); i += t.size() )
		n += std::memcmp( &a[i], t.data(), t.size() ) != 0;
	return n;
}
}  // namespace sfmt

int main( int argc, char ** argv )
{
	using namespace sfmt;
	(void)argc;
	(void)argv;
	setvbuf( stdout, nullptr, _IONBF, 0 );
	auto fatal = []( const std::string & m ) {
		std::printf( "SHAREDFMT %s\nSHAREDFMT GATE FAILED\n", m.c_str() );
		return 2;
	};
	using FbLayer = void( WINAPI * )( GLenum, GLenum, GLuint, GLint, GLint );
	using ClearBufFv = void( WINAPI * )( GLenum, GLint, const GLfloat * );
	FbLayer fbLayer = nullptr;
	ClearBufFv clearBuf = nullptr;
	gOpenGl = LoadLibraryA( "opengl32.dll" );
	WNDCLASSA wc {};
	wc.style = CS_OWNDC;
	wc.lpfnWndProc = wndProc;
	wc.hInstance = GetModuleHandleA( nullptr );
	wc.lpszClassName = "wwsharedfmt";
	RegisterClassA( &wc );
	HWND wnd = CreateWindowExA( WS_EX_NOACTIVATE, "wwsharedfmt", "shared format harness", WS_OVERLAPPEDWINDOW, 40, 40, 160, 90, nullptr,
		nullptr, wc.hInstance, nullptr );
	HDC dc = GetDC( wnd );
	PIXELFORMATDESCRIPTOR pfd { sizeof( pfd ), 1, PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER, PFD_TYPE_RGBA, 32 };
	SetPixelFormat( dc, ChoosePixelFormat( dc, &pfd ), &pfd );
	HGLRC glrc = wglCreateContext( dc );
	wglMakeCurrent( dc, glrc );
	pump();
	if ( !loadAll() || !load( fbLayer, "glFramebufferTextureLayer" ) || !load( clearBuf, "glClearBufferfv" ) )
		return fatal( "GL entry points" );
	std::printf( "SHAREDFMT GL on %s\n", reinterpret_cast<const char *>( glGetString( GL_RENDERER ) ) );
	glPixelStorei( GL_PACK_ALIGNMENT, 1 );
	glPixelStorei( GL_UNPACK_ALIGNMENT, 1 );
	std::string why;
	if ( !WwVkPresent::viewBegin( 160, 90, 1, glProc, why ) )
		return fatal( "the view: " + why );
	std::vector<Case> cs;
	cs.push_back( { "R11G11B10F atlas 2D 512x320, 5 levels", 0, 512, 320, 1, 5, 0x8C3A, 4 } );
	cs.push_back( { "RGBA8 cube-map array 64x64, 33 cubes", 3, 64, 64, 6 * 33, 1, 0x8058, 4 } );
	cs.push_back( { "RGBA16F cube-map array 64x64, 33 cubes", 3, 64, 64, 6 * 33, 1, 0x881A, 8 } );
	std::mt19937 rng( 1115u );
	bool ok = true;
	while ( glGetError() != GL_NO_ERROR ) {
	}
	std::vector<bool> made( cs.size(), false );
	for ( size_t i = 0; i < cs.size(); i++ ) {
		Case & c = cs[i];
		if ( !WwVkPresent::sharedTarget( reinterpret_cast<std::uintptr_t>( &c.key ), c.type, c.w, c.h, c.layers, c.fmt, c.tex, why, c.levels ) ) {
			std::printf( "SHAREDFMT %s: not made: %s\n", c.name, why.c_str() );
			ok = false;
			continue;
		}
		made[i] = true;
		upload( c, rng );
		const GLenum e = glGetError();
		if ( e != GL_NO_ERROR ) {
			std::printf( "SHAREDFMT %s: GL error 0x%04X on the upload\n", c.name, unsigned( e ) );
			ok = false;
		}
	}
	glFinish();
	for ( size_t i = 0; i < cs.size(); i++ ) {
		if ( !made[i] )
			continue;
		Case & c = cs[i];
		size_t bad = 0, bytes = 0, parts = 0;
		for ( int l = 0; l < c.levels; l++ )
			for ( int z = 0; z < c.layers; z++ ) {
				std::vector<std::uint8_t> got;
				if ( !readShared( reinterpret_cast<std::uintptr_t>( &c.key ), l, z, c.bpt, got, why ) ) {
					std::printf( "SHAREDFMT %s: read level %d layer %d failed: %s\n", c.name, l, z, why.c_str() );
					ok = false;
					continue;
				}
				bad += differ( got, c.data[size_t( l )][size_t( z )] );
				bytes += got.size();
				parts++;
			}
		std::printf( "SHAREDFMT %s: %zu levels x layers, %zu bytes, %zu differ: %s\n", c.name, parts, bytes, bad,
			bad || !bytes ? "FAIL" : "EXACT" );
		ok = ok && bad == 0 && bytes > 0 && parts == size_t( c.levels * c.layers );
	}
	auto red = [&]( const char * name, size_t bad ) {
		std::printf( "SHAREDFMT red %s: %s (%zu differ)\n", name, bad ? "FAIL" : "EXACT", bad );
		if ( !bad ) {
			std::printf( "SHAREDFMT red %s PASSED: the gate cannot see it\n", name );
			ok = false;
		}
	};
	GLuint fb = 0;
	f.genFramebuffers( 1, &fb );
	// the atlas: level 0 as a render target (the relight's), a clear, then GL's mips
	if ( made[0] ) {
		Case & c = cs[0];
		const std::uintptr_t key = reinterpret_cast<std::uintptr_t>( &c.key );
		{
			std::vector<std::uint8_t> got;
			readShared( key, 1, 0, c.bpt, got, why );
			red( "level one off (atlas level 1 read as level 2)", differ( got, c.data[2][0] ) );
		}
		f.bindFramebuffer( fFb, fb );
		f.framebufferTexture2D( fFb, fCol0, GL_TEXTURE_2D, c.tex, 0 );
		const GLenum st = f.checkFramebufferStatus( fFb );
		std::printf( "SHAREDFMT atlas level 0 as a framebuffer: 0x%04X %s\n", unsigned( st ), st == fComplete ? "complete" : "INCOMPLETE" );
		ok = ok && st == fComplete;
		const GLfloat col[4] = { 1.0f, 2.0f, 0.5f, 1.0f };
		glViewport( 0, 0, c.w, c.h );
		clearBuf( GL_COLOR, 0, col );
		f.bindFramebuffer( fFb, 0 );
		glBindTexture( GL_TEXTURE_2D, c.tex );
		f.generateMipmap( GL_TEXTURE_2D );
		glBindTexture( GL_TEXTURE_2D, 0 );
		const GLenum e = glGetError();
		if ( e != GL_NO_ERROR ) {
			std::printf( "SHAREDFMT atlas clear / mips: GL error 0x%04X\n", unsigned( e ) );
			ok = false;
		}
		glFinish();
		// R 1.0 (11 bit: exponent 15) | G 2.0 (11 bit: exponent 16) << 11 | B 0.5 (10 bit: exponent 14) << 22
		const std::uint32_t want = ( 15u << 6 ) | ( ( 16u << 6 ) << 11 ) | ( ( 14u << 5 ) << 22 );
		const std::uint32_t swapped = ( 14u << 6 ) | ( ( 16u << 6 ) << 11 ) | ( ( 15u << 5 ) << 22 );  // R and B exchanged
		for ( int l : { 0, 4 } ) {
			std::vector<std::uint8_t> got;
			const bool r = readShared( key, l, 0, c.bpt, got, why );
			const size_t n = r ? notWord( got, want ) : size_t( 1 );
			std::printf( "SHAREDFMT atlas clear (1, 2, 0.5) level %d: 0x%08X wanted, %zu of %zu texels differ: %s\n", l, unsigned( want ), n,
				got.size() / 4, n ? "FAIL" : "EXACT" );
			ok = ok && r && n == 0;
			if ( l == 0 )
				red( "channel order swapped (R and B exchanged)", r ? notWord( got, swapped ) : 0 );
		}
	}
	// the cube arrays: one layer as a render target (GL's capture), a clear, neighbours untouched
	for ( size_t i = 1; i < cs.size(); i++ ) {
		if ( !made[i] )
			continue;
		Case & c = cs[i];
		const std::uintptr_t key = reinterpret_cast<std::uintptr_t>( &c.key );
		f.bindFramebuffer( fFb, fb );
		f.framebufferTexture2D( fFb, fCol0, GL_TEXTURE_2D, 0, 0 );
		fbLayer( fFb, fCol0, c.tex, 0, 7 );
		const GLenum st = f.checkFramebufferStatus( fFb );
		std::printf( "SHAREDFMT %s layer 7 as a framebuffer: 0x%04X %s\n", c.name, unsigned( st ), st == fComplete ? "complete" : "INCOMPLETE" );
		ok = ok && st == fComplete;
		const GLfloat col[4] = { 0.25f, 0.5f, 0.75f, 1.0f };
		glViewport( 0, 0, c.w, c.h );
		clearBuf( GL_COLOR, 0, col );
		f.bindFramebuffer( fFb, 0 );
		const GLenum e = glGetError();
		if ( e != GL_NO_ERROR ) {
			std::printf( "SHAREDFMT %s layer clear: GL error 0x%04X\n", c.name, unsigned( e ) );
			ok = false;
		}
		glFinish();
		std::vector<std::uint8_t> texel;
		if ( c.fmt == 0x8058 )
			texel = { 64, 128, 191, 255 };  // 0.25 * 255 = 63.75, 0.5 * 255 = 127.5, 0.75 * 255 = 191.25: round to nearest
		else
			texel = { 0x00, 0x34, 0x00, 0x38, 0x00, 0x3A, 0x00, 0x3C };  // halves 0.25, 0.5, 0.75, 1.0
		std::vector<std::uint8_t> got;
		const bool r = readShared( key, 0, 7, c.bpt, got, why );
		const size_t n = r ? notTexel( got, texel ) : size_t( 1 );
		std::printf( "SHAREDFMT %s cleared layer 7: %zu of %zu texels differ: %s\n", c.name, n, got.size() / size_t( c.bpt ), n ? "FAIL" : "EXACT" );
		ok = ok && r && n == 0;
		for ( int z : { 6, 8 } ) {
			std::vector<std::uint8_t> g2;
			const bool r2 = readShared( key, 0, z, c.bpt, g2, why );
			const size_t n2 = r2 ? differ( g2, c.data[0][size_t( z )] ) : size_t( 1 );
			std::printf( "SHAREDFMT %s layer %d kept its bytes: %zu differ: %s\n", c.name, z, n2, n2 ? "FAIL" : "EXACT" );
			ok = ok && n2 == 0;
		}
		if ( i == 1 ) {
			std::vector<std::uint8_t> g3;
			readShared( key, 0, 8, c.bpt, g3, why );
			red( "layer one off (layer 8 read as the cleared layer)", notTexel( g3, texel ) );
		}
	}
	// refusals
	{
		char k1 = 0, k2 = 0;
		unsigned t = 0;
		std::string w1, w2;
		const bool a = WwVkPresent::sharedTarget( reinterpret_cast<std::uintptr_t>( &k1 ), 3, 64, 32, 6, 0x8058, t, w1 );
		const bool b = WwVkPresent::sharedTarget( reinterpret_cast<std::uintptr_t>( &k2 ), 3, 64, 64, 7, 0x8058, t, w2 );
		std::printf( "SHAREDFMT a 64x32 cube array: %s (%s)\n", a ? "MADE (wrong)" : "refused", w1.c_str() );
		std::printf( "SHAREDFMT a cube array of 7 layers: %s (%s)\n", b ? "MADE (wrong)" : "refused", w2.c_str() );
		if ( a )
			WwVkPresent::sharedTargetDrop( reinterpret_cast<std::uintptr_t>( &k1 ) );
		if ( b )
			WwVkPresent::sharedTargetDrop( reinterpret_cast<std::uintptr_t>( &k2 ) );
		ok = ok && !a && !b;
	}
	for ( auto & c : cs )
		WwVkPresent::sharedTargetDrop( reinterpret_cast<std::uintptr_t>( &c.key ) );
	std::printf( "SHAREDFMT GATE %s\n", ok ? "OK" : "FAILED" );
	return ok ? 0 : 1;
}
