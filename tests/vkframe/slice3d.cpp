// Lane VKAIR9 (2026-11-16, ruling (a) of 11-16): a 3D shared target carries VK_IMAGE_CREATE_2D_ARRAY_COMPATIBLE_BIT, so the
// present device can render into ONE slice of it (a 2D colour view at baseArrayLayer z: GL's glFramebufferTextureLayer on a
// 3D texture), the base of the air inject's |3 route (one framebuffer per layer).
// Cases: RGBA32F 16x8x6 (the inject), RGBA16F 16x8x6 (the integrated volume), R8 16x8x6 (a single-channel volume).
// Per case:
//   1. Shared.slices is true, and a slice view of z = d and of a 2D target are refused.
//   2. Layout kept: GL uploads random bytes (glTexSubImage3D), the present device copies every slice out: EXACT.
//   3. Slices render: GL clears the whole target to a sentinel (glClearTexImage), the present device runs one render pass
//      a slice (loadOp CLEAR, a distinct value per slice, one framebuffer per slice through sliceView), GL reads the
//      target back with glGetTexImage TWICE; both reads equal, every slice holds its own value.
// Bar (set before the first run, never widened): every check above EXACT (0 texels differ), no GL error, the read-twice
// clause holds, and the device offers each format as a 3D image with the flag (printed: max extent, sample counts).
// Reds (each must FAIL, printed with moved / unwritten counts):
//   noflag   the target made exactly as before this lane (sharedSliceRed() = 1: no 2D_ARRAY_COMPATIBLE): sliceView refuses,
//            the slices keep the sentinel;
//   stuck    every slice's pass drawn into slice 0 (layer stuck at 0);
//   offby1   slice k's value drawn into slice k + 1 (strata off by one).
// Last line `SLICE3D GATE OK` or `SLICE3D GATE FAILED`.
#define main fxview_main
#include "fxview.cpp"
#undef main

#include <random>

namespace sl3
{
using namespace fv;

struct Case
{
	const char * name;
	unsigned glFmt;
	int bpt;  // bytes a texel
	int comps;  // GL read: 4 = RGBA float, 1 = RED unsigned byte
	char key = 0;
	unsigned tex = 0;
};

const int sw = 16, sh = 8, sd = 6;

// slice k's clear value
VkClearColorValue sliceValue( const Case & c, int k )
{
	VkClearColorValue v {};
	if ( c.comps == 1 ) {
		v.float32[0] = float( 32 * ( k + 1 ) ) / 255.0f;
		v.float32[3] = 1.0f;
	} else {
		v.float32[0] = 0.25f * float( k + 1 );
		v.float32[1] = 0.5f;
		v.float32[2] = 0.125f * float( k );
		v.float32[3] = 1.0f;
	}
	return v;
}

// mode 0 green, 1 stuck (every pass into slice 0), 2 off by one (slice k's value into slice k + 1); returns passes drawn
int renderSlices( const Shared & s, const Case & c, int mode, std::string & why )
{
	Vk & v = vk();
	drain();
	v.vkDeviceWaitIdle( v.dev );
	VkAttachmentDescription ad {};
	ad.format = s.fmt;
	ad.samples = VK_SAMPLE_COUNT_1_BIT;
	ad.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	ad.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	ad.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	ad.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	ad.initialLayout = VK_IMAGE_LAYOUT_GENERAL;
	ad.finalLayout = VK_IMAGE_LAYOUT_GENERAL;
	VkAttachmentReference ar { 0, VK_IMAGE_LAYOUT_GENERAL };
	VkSubpassDescription sp {};
	sp.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	sp.colorAttachmentCount = 1;
	sp.pColorAttachments = &ar;
	VkRenderPassCreateInfo rci { VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO };
	rci.attachmentCount = 1;
	rci.pAttachments = &ad;
	rci.subpassCount = 1;
	rci.pSubpasses = &sp;
	VkRenderPass rp = VK_NULL_HANDLE;
	if ( v.vkCreateRenderPass( v.dev, &rci, nullptr, &rp ) != VK_SUCCESS ) {
		why = "vkCreateRenderPass failed";
		return -1;
	}
	std::vector<VkImageView> views( static_cast<size_t>( s.d ), VK_NULL_HANDLE );
	std::vector<VkFramebuffer> fbs( static_cast<size_t>( s.d ), VK_NULL_HANDLE );
	for ( int z = 0; z < s.d; z++ ) {
		std::string w;
		views[size_t( z )] = sliceView( s, z, w );
		if ( !views[size_t( z )] ) {
			if ( why.empty() )
				why = w;
			continue;
		}
		VkFramebufferCreateInfo fci { VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
		fci.renderPass = rp;
		fci.attachmentCount = 1;
		fci.pAttachments = &views[size_t( z )];
		fci.width = std::uint32_t( s.w );
		fci.height = std::uint32_t( s.h );
		fci.layers = 1;
		if ( v.vkCreateFramebuffer( v.dev, &fci, nullptr, &fbs[size_t( z )] ) != VK_SUCCESS && why.empty() )
			why = "vkCreateFramebuffer (a slice) failed";
	}
	VkCommandBufferAllocateInfo cai { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
	cai.commandPool = v.pool;
	cai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	cai.commandBufferCount = 1;
	VkCommandBuffer cmd = VK_NULL_HANDLE;
	VkFence fence = VK_NULL_HANDLE;
	VkFenceCreateInfo fi { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
	int drawn = 0;
	bool ok = v.vkAllocateCommandBuffers( v.dev, &cai, &cmd ) == VK_SUCCESS && v.vkCreateFence( v.dev, &fi, nullptr, &fence ) == VK_SUCCESS;
	if ( ok ) {
		VkCommandBufferBeginInfo bi { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
		bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		v.vkBeginCommandBuffer( cmd, &bi );
		for ( int k = 0; k < s.d; k++ ) {
			const int z = mode == 1 ? 0 : mode == 2 ? k + 1 : k;
			if ( z >= s.d || !fbs[size_t( z )] )
				continue;
			VkClearValue cv {};
			cv.color = sliceValue( c, k );
			VkRenderPassBeginInfo rb { VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
			rb.renderPass = rp;
			rb.framebuffer = fbs[size_t( z )];
			rb.renderArea = { { 0, 0 }, { std::uint32_t( s.w ), std::uint32_t( s.h ) } };
			rb.clearValueCount = 1;
			rb.pClearValues = &cv;
			v.vkCmdBeginRenderPass( cmd, &rb, VK_SUBPASS_CONTENTS_INLINE );
			v.vkCmdEndRenderPass( cmd );
			drawn++;
		}
		VkMemoryBarrier mb { VK_STRUCTURE_TYPE_MEMORY_BARRIER };
		mb.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		mb.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
		v.vkCmdPipelineBarrier( cmd, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 1, &mb, 0, nullptr, 0,
			nullptr );
		v.vkEndCommandBuffer( cmd );
		ok = oneShot( cmd, fence );
		if ( !ok )
			why = "the slice passes' submit failed";
	}
	if ( cmd )
		v.vkFreeCommandBuffers( v.dev, v.pool, 1, &cmd );
	if ( fence )
		v.vkDestroyFence( v.dev, fence, nullptr );
	for ( auto fb : fbs )
		if ( fb )
			v.vkDestroyFramebuffer( v.dev, fb, nullptr );
	for ( auto iv : views )
		if ( iv )
			v.vkDestroyImageView( v.dev, iv, nullptr );
	v.vkDestroyRenderPass( v.dev, rp, nullptr );
	return ok ? drawn : -1;
}

// one slice of a 3D shared target as the present device sees it (GL finished first)
bool readSlice( const Shared & s, int z, int bpt, std::vector<std::uint8_t> & out, std::string & why )
{
	Vk & v = vk();
	const VkDeviceSize bytes = VkDeviceSize( s.w ) * VkDeviceSize( s.h ) * VkDeviceSize( bpt );
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
		c.imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
		c.imageOffset = { 0, 0, z };
		c.imageExtent = { std::uint32_t( s.w ), std::uint32_t( s.h ), 1 };
		v.vkCmdCopyImageToBuffer( cmd, s.img, VK_IMAGE_LAYOUT_GENERAL, buf, 1, &c );
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
		why = "the slice read back failed";
	return ok;
}

// random texels with no inf / NaN pattern (an exponent of all ones moved one down)
void fill( std::vector<std::uint8_t> & b, const Case & c, std::mt19937 & rng )
{
	for ( auto & x : b )
		x = std::uint8_t( rng() );
	if ( c.glFmt == 0x881A ) {
		for ( size_t i = 0; i + 2 <= b.size(); i += 2 ) {
			std::uint16_t h;
			std::memcpy( &h, &b[i], 2 );
			if ( ( ( h >> 10 ) & 31u ) == 31u )
				h = std::uint16_t( h & ~( 1u << 10 ) );
			std::memcpy( &b[i], &h, 2 );
		}
	} else if ( c.glFmt == 0x8814 ) {
		for ( size_t i = 0; i + 4 <= b.size(); i += 4 ) {
			std::uint32_t p;
			std::memcpy( &p, &b[i], 4 );
			if ( ( ( p >> 23 ) & 255u ) == 255u )
				p &= ~( 1u << 23 );
			std::memcpy( &b[i], &p, 4 );
		}
	}
}
}  // namespace sl3

int main( int argc, char ** argv )
{
	using namespace sl3;
	(void)argc;
	(void)argv;
	setvbuf( stdout, nullptr, _IONBF, 0 );
	auto fatal = []( const std::string & m ) {
		std::printf( "SLICE3D %s\nSLICE3D GATE FAILED\n", m.c_str() );
		return 2;
	};
	using ClearTex = void( WINAPI * )( GLuint, GLint, GLenum, GLenum, const void * );
	ClearTex clearTex = nullptr;
	gOpenGl = LoadLibraryA( "opengl32.dll" );
	WNDCLASSA wc {};
	wc.style = CS_OWNDC;
	wc.lpfnWndProc = wndProc;
	wc.hInstance = GetModuleHandleA( nullptr );
	wc.lpszClassName = "wwslice3d";
	RegisterClassA( &wc );
	HWND wnd = CreateWindowExA( WS_EX_NOACTIVATE, "wwslice3d", "3D slice harness", WS_OVERLAPPEDWINDOW, 40, 40, 160, 90, nullptr, nullptr,
		wc.hInstance, nullptr );
	HDC dc = GetDC( wnd );
	PIXELFORMATDESCRIPTOR pfd { sizeof( pfd ), 1, PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER, PFD_TYPE_RGBA, 32 };
	SetPixelFormat( dc, ChoosePixelFormat( dc, &pfd ), &pfd );
	HGLRC glrc = wglCreateContext( dc );
	wglMakeCurrent( dc, glrc );
	pump();
	if ( !loadAll() || !load( clearTex, "glClearTexImage" ) )
		return fatal( "GL entry points" );
	std::printf( "SLICE3D GL on %s\n", reinterpret_cast<const char *>( glGetString( GL_RENDERER ) ) );
	glPixelStorei( GL_PACK_ALIGNMENT, 1 );
	glPixelStorei( GL_UNPACK_ALIGNMENT, 1 );
	std::string why;
	if ( !WwVkPresent::viewBegin( 160, 90, 1, glProc, why ) )
		return fatal( "the view: " + why );
	std::vector<Case> cs;
	cs.push_back( { "RGBA32F 3D 16x8x6", 0x8814, 16, 4 } );
	cs.push_back( { "RGBA16F 3D 16x8x6", 0x881A, 8, 4 } );
	cs.push_back( { "R8 3D 16x8x6", 0x8229, 1, 1 } );
	std::mt19937 rng( 1116u );
	bool ok = true;
	while ( glGetError() != GL_NO_ERROR ) {
	}
	{
		Vk & v = vk();
		for ( auto & c : cs ) {
			VkImageFormatProperties ip {};
			bool dep = false;
			const VkResult r = v.vkGetPhysicalDeviceImageFormatProperties( v.phys, vkFormatOf( c.glFmt, dep ), VK_IMAGE_TYPE_3D, VK_IMAGE_TILING_OPTIMAL,
				VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
				VK_IMAGE_CREATE_2D_ARRAY_COMPATIBLE_BIT, &ip );
			std::printf( "SLICE3D %s with 2D_ARRAY_COMPATIBLE: %s (max %ux%ux%u, samples 0x%X)\n", c.name, r == VK_SUCCESS ? "offered" : "REFUSED",
				ip.maxExtent.width, ip.maxExtent.height, ip.maxExtent.depth, unsigned( ip.sampleCounts ) );
			ok = ok && r == VK_SUCCESS;
		}
	}
	// GL's view of the whole target after a run; returns texels off their slice's value, counts sentinel texels and a read mismatch
	auto glJudge = [&]( Case & c, int mode, size_t & unwritten, bool & twice ) {
		const size_t texels = size_t( sw ) * size_t( sh ) * size_t( sd );
		const size_t bytes = texels * size_t( c.comps == 1 ? 1 : 16 );
		std::vector<std::uint8_t> a( bytes ), b( bytes );
		glBindTexture( 0x806F, c.tex );
		const GLenum pf = c.comps == 1 ? GL_RED : GL_RGBA, ty = c.comps == 1 ? GL_UNSIGNED_BYTE : GL_FLOAT;
		glGetTexImage( 0x806F, 0, pf, ty, a.data() );
		glGetTexImage( 0x806F, 0, pf, ty, b.data() );
		glBindTexture( 0x806F, 0 );
		twice = a == b;
		size_t bad = 0;
		unwritten = 0;
		for ( int z = 0; z < sd; z++ ) {
			const VkClearColorValue cv = sliceValue( c, z );
			for ( int i = 0; i < sw * sh; i++ ) {
				const size_t t = size_t( z ) * size_t( sw * sh ) + size_t( i );
				if ( c.comps == 1 ) {
					const std::uint8_t want = std::uint8_t( 32 * ( z + 1 ) ), got = a[t];
					bad += got != want;
					unwritten += got == 5;
				} else {
					float got[4];
					std::memcpy( got, &a[t * 16], 16 );
					bool off = false;
					for ( int q = 0; q < 4; q++ )
						off = off || got[q] != cv.float32[q];
					bad += off;
					unwritten += got[0] == -7.0f;
				}
			}
		}
		(void)mode;
		return bad;
	};
	auto sentinel = [&]( Case & c ) {
		if ( c.comps == 1 ) {
			const std::uint8_t s = 5;
			clearTex( c.tex, 0, GL_RED, GL_UNSIGNED_BYTE, &s );
		} else {
			const float s[4] = { -7.0f, -7.0f, -7.0f, -7.0f };
			clearTex( c.tex, 0, GL_RGBA, GL_FLOAT, s );
		}
		glFinish();
	};
	auto red = [&]( const char * name, size_t bad, size_t unwritten ) {
		std::printf( "SLICE3D red %s: %s (%zu texels moved, %zu unwritten)\n", name, bad ? "FAIL" : "EXACT", bad, unwritten );
		if ( !bad ) {
			std::printf( "SLICE3D red %s PASSED: the gate cannot see it\n", name );
			ok = false;
		}
	};
	for ( auto & c : cs ) {
		const std::uintptr_t key = reinterpret_cast<std::uintptr_t>( &c.key );
		if ( !WwVkPresent::sharedTarget( key, 1, sw, sh, sd, c.glFmt, c.tex, why, 1 ) ) {
			std::printf( "SLICE3D %s: not made: %s\n", c.name, why.c_str() );
			ok = false;
			continue;
		}
		const Shared * s = sharedFind( key );
		if ( !s ) {
			std::printf( "SLICE3D %s: no Shared of its key\n", c.name );
			ok = false;
			continue;
		}
		std::printf( "SLICE3D %s: slices %s\n", c.name, s->slices ? "on" : "OFF" );
		ok = ok && s->slices;
		{
			std::string w;
			VkImageView iv = sliceView( *s, sd, w );
			std::printf( "SLICE3D %s: slice %d of %d: %s (%s)\n", c.name, sd, sd, iv ? "MADE (wrong)" : "refused", w.c_str() );
			if ( iv )
				vk().vkDestroyImageView( vk().dev, iv, nullptr );
			ok = ok && !iv;
		}
		// 2: GL's upload, the present device's copy of every slice
		{
			std::vector<std::vector<std::uint8_t>> up( static_cast<size_t>( sd ) );
			glBindTexture( 0x806F, c.tex );
			for ( int z = 0; z < sd; z++ ) {
				up[size_t( z )].resize( size_t( sw ) * size_t( sh ) * size_t( c.bpt ) );
				fill( up[size_t( z )], c, rng );
				f.texSubImage3D( 0x806F, 0, 0, 0, z, sw, sh, 1, c.comps == 1 ? GL_RED : GL_RGBA,
					c.glFmt == 0x881A ? 0x140B : c.glFmt == 0x8814 ? GL_FLOAT : GL_UNSIGNED_BYTE, up[size_t( z )].data() );
			}
			glBindTexture( 0x806F, 0 );
			glFinish();
			size_t bad = 0, bytes = 0;
			for ( int z = 0; z < sd; z++ ) {
				std::vector<std::uint8_t> got;
				if ( !readSlice( *s, z, c.bpt, got, why ) ) {
					std::printf( "SLICE3D %s: slice %d read failed: %s\n", c.name, z, why.c_str() );
					ok = false;
					continue;
				}
				for ( size_t i = 0; i < got.size() && i < up[size_t( z )].size(); i++ )
					bad += got[i] != up[size_t( z )][i];
				bad += got.size() != up[size_t( z )].size();
				bytes += got.size();
			}
			std::printf( "SLICE3D %s: GL upload vs the present device's slice copies: %zu bytes, %zu differ: %s\n", c.name, bytes, bad,
				bad || !bytes ? "FAIL" : "EXACT" );
			ok = ok && bad == 0 && bytes > 0;
		}
		// 3: the slices as render targets
		for ( int mode = 0; mode < 3; mode++ ) {
			sentinel( c );
			std::string w;
			const int drawn = renderSlices( *s, c, mode, w );
			size_t unwritten = 0;
			bool twice = false;
			const size_t bad = glJudge( c, mode, unwritten, twice );
			const GLenum e = glGetError();
			if ( mode == 0 ) {
				std::printf( "SLICE3D %s: %d slice passes, %zu of %d texels off their slice's value, %zu unwritten, read twice %s, GL error 0x%04X: %s%s%s\n",
					c.name, drawn, bad, sw * sh * sd, unwritten, twice ? "equal" : "DIFFERS", unsigned( e ),
					bad || !twice || e || drawn != sd ? "FAIL" : "EXACT", w.empty() ? "" : " - ", w.c_str() );
				ok = ok && bad == 0 && twice && e == GL_NO_ERROR && drawn == sd;
			} else {
				ok = ok && twice && e == GL_NO_ERROR;
				red( mode == 1 ? "stuck (every slice's pass into slice 0)" : "offby1 (slice k's value into slice k + 1)", bad, unwritten );
			}
		}
	}
	// red noflag: a target made as before this lane; its slices cannot be render targets
	{
		Case & c = cs[0];
		char rk = 0;
		unsigned rt = 0;
		const std::uintptr_t key = reinterpret_cast<std::uintptr_t>( &rk );
		sharedSliceRed() = 1;
		const bool made = WwVkPresent::sharedTarget( key, 1, sw, sh, sd, c.glFmt, rt, why, 1 );
		sharedSliceRed() = 0;
		const Shared * s = made ? sharedFind( key ) : nullptr;
		if ( !s ) {
			std::printf( "SLICE3D red noflag: the target was not made (%s)\n", why.c_str() );
			ok = false;
		} else {
			Case rc = c;
			rc.tex = rt;
			sentinel( rc );
			std::string w;
			const int drawn = renderSlices( *s, rc, 0, w );
			size_t unwritten = 0;
			bool twice = false;
			const size_t bad = glJudge( rc, 0, unwritten, twice );
			std::printf( "SLICE3D red noflag: slices %s, %d slice passes (%s)\n", s->slices ? "on" : "off", drawn, w.c_str() );
			red( "noflag (made without 2D_ARRAY_COMPATIBLE)", bad, unwritten );
			WwVkPresent::sharedTargetDrop( key );
		}
	}
	// a 2D target has no slices
	{
		char k2 = 0;
		unsigned t2 = 0;
		const std::uintptr_t key = reinterpret_cast<std::uintptr_t>( &k2 );
		if ( WwVkPresent::sharedTarget( key, 0, sw, sh, 1, 0x8814, t2, why, 1 ) ) {
			const Shared * s = sharedFind( key );
			std::string w;
			VkImageView iv = s ? sliceView( *s, 0, w ) : VK_NULL_HANDLE;
			std::printf( "SLICE3D a slice of a 2D target: %s (%s)\n", iv ? "MADE (wrong)" : "refused", w.c_str() );
			if ( iv )
				vk().vkDestroyImageView( vk().dev, iv, nullptr );
			ok = ok && !iv && s;
			WwVkPresent::sharedTargetDrop( key );
		} else {
			std::printf( "SLICE3D the 2D target was not made: %s\n", why.c_str() );
			ok = false;
		}
	}
	for ( auto & c : cs )
		WwVkPresent::sharedTargetDrop( reinterpret_cast<std::uintptr_t>( &c.key ) );
	std::printf( "SLICE3D GATE %s\n", ok ? "OK" : "FAILED" );
	return ok ? 0 : 1;
}
