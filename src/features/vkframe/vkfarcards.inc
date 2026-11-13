/* lane VKFAR1 round 2 (row G3, 2026-11-13): what the far cards (impostor_cellinstez) need on the present device before
 * they can draw there. Included once by vkpresent.cpp after vkfardots.inc (it reads that file's anonymous-namespace
 * parts: Shared, sharedFree, memType, oneShot, imageView, vkFormatOf).
 * 1. twins sharedTarget does not make: the card sheets' BC formats (GL's compressed names) and cube-map arrays (the
 *    placed lights' shadow cubes, GL_DEPTH_COMPONENT16). Same contract as sharedTarget (one image both APIs see, keyed,
 *    GENERAL on both sides, GL copies into it), its own key map so sharedTarget's twins and stats stay as they were. */
namespace
{
constexpr GLenum kFcCubeArray = 0x9009, kFcBindingCubeArray = 0x900A, kFcDepth16 = 0x81A5;

struct FcFormat
{
	VkFormat fmt;
	int block;  // bytes per 4x4 block (BC), 0: per texel below
	int texel;
	bool depth;
};

// GL's internal format -> the twin's (BC: S3TC, RGTC, BPTC names; the depth cube; else sharedTarget's colour formats)
FcFormat fcFormat( GLenum f )
{
	switch ( f ) {
	case 0x83F0: return { VK_FORMAT_BC1_RGB_UNORM_BLOCK, 8, 0, false };
	case 0x83F1: return { VK_FORMAT_BC1_RGBA_UNORM_BLOCK, 8, 0, false };
	case 0x8C4C: return { VK_FORMAT_BC1_RGB_SRGB_BLOCK, 8, 0, false };
	case 0x8C4D: return { VK_FORMAT_BC1_RGBA_SRGB_BLOCK, 8, 0, false };
	case 0x83F2: return { VK_FORMAT_BC2_UNORM_BLOCK, 16, 0, false };
	case 0x8C4E: return { VK_FORMAT_BC2_SRGB_BLOCK, 16, 0, false };
	case 0x83F3: return { VK_FORMAT_BC3_UNORM_BLOCK, 16, 0, false };
	case 0x8C4F: return { VK_FORMAT_BC3_SRGB_BLOCK, 16, 0, false };
	case 0x8DBB: return { VK_FORMAT_BC4_UNORM_BLOCK, 8, 0, false };
	case 0x8DBC: return { VK_FORMAT_BC4_SNORM_BLOCK, 8, 0, false };
	case 0x8DBD: return { VK_FORMAT_BC5_UNORM_BLOCK, 16, 0, false };
	case 0x8DBE: return { VK_FORMAT_BC5_SNORM_BLOCK, 16, 0, false };
	case 0x8E8F: return { VK_FORMAT_BC6H_UFLOAT_BLOCK, 16, 0, false };
	case 0x8E8E: return { VK_FORMAT_BC6H_SFLOAT_BLOCK, 16, 0, false };
	case 0x8E8C: return { VK_FORMAT_BC7_UNORM_BLOCK, 16, 0, false };
	case 0x8E8D: return { VK_FORMAT_BC7_SRGB_BLOCK, 16, 0, false };
	case kFcDepth16: return { VK_FORMAT_D16_UNORM, 0, 2, true };
	default: break;
	}
	bool depth = false;
	const VkFormat c = vkFormatOf( f, depth );
	if ( c == VK_FORMAT_UNDEFINED || depth )
		return { VK_FORMAT_UNDEFINED, 0, 0, false };
	const int texel = c == VK_FORMAT_R32G32B32A32_SFLOAT ? 16 : c == VK_FORMAT_R16G16B16A16_SFLOAT || c == VK_FORMAT_R32G32_SFLOAT ? 8
		: c == VK_FORMAT_R8_UNORM ? 1 : c == VK_FORMAT_R8G8_UNORM ? 2 : 4;
	return { c, 0, texel, false };
}

// bytes of one layer of one level as vkCmdCopyImageToBuffer packs it
VkDeviceSize fcLayerBytes( const FcFormat & f, int w, int h )
{
	if ( f.block )
		return VkDeviceSize( ( w + 3 ) / 4 ) * VkDeviceSize( ( h + 3 ) / 4 ) * VkDeviceSize( f.block );
	return VkDeviceSize( w ) * VkDeviceSize( h ) * VkDeviceSize( f.texel );
}

struct FarCards
{
	std::map<std::uintptr_t, Shared> twins;
	WwVkPresent::FarTwinStat st;
	int features = -1;  // -1 unread; bit 0 textureCompressionBC, bit 1 imageCubeArray (the device enables what is offered)
};

FarCards & farCards()
{
	static FarCards c;
	return c;
}

bool fcFeatures( bool bc, bool cubeArray, std::string & why )
{
	Vk & v = vk();
	FarCards & C = farCards();
	if ( C.features < 0 ) {
		VkPhysicalDeviceFeatures f {};
		if ( v.vkGetPhysicalDeviceFeatures )
			v.vkGetPhysicalDeviceFeatures( v.phys, &f );
		C.features = ( f.textureCompressionBC ? 1 : 0 ) | ( f.imageCubeArray ? 2 : 0 );
	}
	if ( bc && !( C.features & 1 ) ) {
		why = "the present device has no textureCompressionBC (the card sheets' BC formats)";
		return false;
	}
	if ( cubeArray && !( C.features & 2 ) ) {
		why = "the present device has no imageCubeArray (the shadow cubes)";
		return false;
	}
	return true;
}

// one image the present device makes and GL imports, sampled only (BC has no attachment use); a cube array is
// cube-compatible with layers = 6 x cubes. GENERAL from the start, as makeShared's.
bool fcMake( Shared & s, const FcFormat & f, GLenum glFmt, int type, int w, int h, int layers, int levels, std::string & why )
{
	Vk & v = vk();
	Gl & g = gl();
	const bool cube = type == 3;
	VkImageUsageFlags usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	if ( f.depth )
		usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;  // GL's shadow cubes are drawn into: the import wants every use
	else if ( !f.block )
		usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	const VkImageCreateFlags flags = cube ? VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT : 0;
	VkImageFormatProperties ip {};
	if ( v.vkGetPhysicalDeviceImageFormatProperties( v.phys, f.fmt, VK_IMAGE_TYPE_2D, VK_IMAGE_TILING_OPTIMAL, usage, flags, &ip ) != VK_SUCCESS
		|| ip.maxExtent.width < std::uint32_t( w ) || ip.maxExtent.height < std::uint32_t( h ) || ip.maxArrayLayers < std::uint32_t( layers )
		|| ip.maxMipLevels < std::uint32_t( levels ) ) {
		why = "format " + std::to_string( int( f.fmt ) ) + ( cube ? " (cube array, " : " (" ) + std::to_string( w ) + "x" + std::to_string( h ) + "x"
			+ std::to_string( layers ) + ", " + std::to_string( levels ) + " levels) is not offered by the device";
		return false;
	}
	VkExternalMemoryImageCreateInfo ext { VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO };
	ext.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT;
	VkImageCreateInfo ci { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
	ci.pNext = &ext;
	ci.flags = flags;
	ci.imageType = VK_IMAGE_TYPE_2D;
	ci.format = f.fmt;
	ci.extent = { std::uint32_t( w ), std::uint32_t( h ), 1 };
	ci.mipLevels = std::uint32_t( levels );
	ci.arrayLayers = std::uint32_t( layers );
	ci.samples = VK_SAMPLE_COUNT_1_BIT;
	ci.tiling = VK_IMAGE_TILING_OPTIMAL;
	ci.usage = usage;
	if ( v.vkCreateImage( v.dev, &ci, nullptr, &s.img ) != VK_SUCCESS ) {
		why = "vkCreateImage (a far-card twin) failed";
		return false;
	}
	VkMemoryRequirements r;
	v.vkGetImageMemoryRequirements( v.dev, s.img, &r );
	VkMemoryDedicatedAllocateInfo ded { VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO };
	ded.image = s.img;
	VkExportMemoryAllocateInfo exp { VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO };
	exp.pNext = &ded;
	exp.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT;
	VkMemoryAllocateInfo ai { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
	ai.pNext = &exp;
	ai.allocationSize = r.size;
	ai.memoryTypeIndex = memType( r.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );
	if ( ai.memoryTypeIndex == UINT32_MAX || v.vkAllocateMemory( v.dev, &ai, nullptr, &s.mem ) != VK_SUCCESS
		|| v.vkBindImageMemory( v.dev, s.img, s.mem, 0 ) != VK_SUCCESS ) {
		why = "no exportable device memory for a far-card twin";
		return false;
	}
	s.mb = double( r.size ) / 1048576.0;
	const VkImageAspectFlags aspect = f.depth ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
	VkCommandBufferAllocateInfo cai { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
	cai.commandPool = v.pool;
	cai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	cai.commandBufferCount = 1;
	VkCommandBuffer cmd = VK_NULL_HANDLE;
	VkFence fence = VK_NULL_HANDLE;
	VkFenceCreateInfo fi { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
	bool laid = false;
	if ( v.vkAllocateCommandBuffers( v.dev, &cai, &cmd ) == VK_SUCCESS && v.vkCreateFence( v.dev, &fi, nullptr, &fence ) == VK_SUCCESS ) {
		VkCommandBufferBeginInfo bi { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
		bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		v.vkBeginCommandBuffer( cmd, &bi );
		VkImageMemoryBarrier b { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
		b.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		b.newLayout = VK_IMAGE_LAYOUT_GENERAL;
		b.srcQueueFamilyIndex = b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		b.image = s.img;
		b.subresourceRange = { aspect, 0, std::uint32_t( levels ), 0, std::uint32_t( layers ) };
		v.vkCmdPipelineBarrier( cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 1, &b );
		v.vkEndCommandBuffer( cmd );
		laid = oneShot( cmd, fence );
	}
	if ( cmd )
		v.vkFreeCommandBuffers( v.dev, v.pool, 1, &cmd );
	if ( fence )
		v.vkDestroyFence( v.dev, fence, nullptr );
	if ( !laid ) {
		why = "a far-card twin's first layout change failed";
		return false;
	}
	VkMemoryGetWin32HandleInfoKHR mi { VK_STRUCTURE_TYPE_MEMORY_GET_WIN32_HANDLE_INFO_KHR };
	mi.memory = s.mem;
	mi.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT;
	HANDLE mh = nullptr;
	if ( v.vkGetMemoryWin32HandleKHR( v.dev, &mi, &mh ) != VK_SUCCESS || !mh ) {
		why = "vkGetMemoryWin32HandleKHR (a far-card twin) failed";
		return false;
	}
	const GLenum target = cube ? kFcCubeArray : layers > 1 || type == 2 ? kTexture2DArray : kTexture2D;
	while ( g.getError() != 0 ) {}
	GLint was = 0;
	g.getIntegerv( cube ? kFcBindingCubeArray : target == kTexture2DArray ? kTexBinding2DArray : kTexBinding2D, &was );
	g.createMemoryObjects( 1, &s.obj );
	const GLint one = 1;
	g.memoryObjectParameteriv( s.obj, kDedicated, &one );
	g.importMemoryWin32Handle( s.obj, GLuint64( r.size ), kHandleOpaqueWin32, mh );
	const GLenum errImport = g.getError();
	CloseHandle( mh );
	g.genTextures( 1, &s.tex );
	g.bindTexture( target, s.tex );
	if ( target == kTexture2D )
		g.texStorageMem2D( target, levels, glFmt, w, h, s.obj, 0 );
	else
		g.texStorageMem3D( target, levels, glFmt, w, h, layers, s.obj, 0 );
	g.texParameteri( target, kTexMinFilter, GLint( kNearest ) );
	g.texParameteri( target, kTexMagFilter, GLint( kNearest ) );
	const GLenum errStorage = g.getError();
	g.bindTexture( target, GLuint( was ) );
	if ( errImport || errStorage ) {
		why = "the GL import of a far-card twin failed (GL error at the import " + std::to_string( errImport ) + ", the storage "
			+ std::to_string( errStorage ) + ")";
		return false;
	}
	s.view = imageView( s.img, cube ? VK_IMAGE_VIEW_TYPE_CUBE_ARRAY : target == kTexture2DArray ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D,
		f.fmt, aspect, std::uint32_t( levels ), std::uint32_t( layers ) );
	if ( !s.view ) {
		why = "a far-card twin's image view failed";
		return false;
	}
	return true;
}

const Shared * farTwinFind( std::uintptr_t key )
{
	auto & m = farCards().twins;
	auto it = m.find( key );
	return it == m.end() ? nullptr : &it->second;
}
}  // namespace

namespace WwVkPresent
{
bool farTwin( std::uintptr_t key, int type, int w, int h, int layers, unsigned glFmt, unsigned & tex, std::string & why, int levels )
{
	Gl & g = gl();
	Vk & v = vk();
	FarCards & C = farCards();
	tex = 0;
	auto fail = [&]( const std::string & r ) {
		why = r;
		C.st.fails++;
		C.st.failWhy = r;
		return false;
	};
	if ( !v.ok || !g.ok || g.ctx != reinterpret_cast<void *>( wglGetCurrentContext() ) )
		return fail( "the present device or GL's entry points are not ready in this context" );
	if ( !g.texStorageMem3D && type != 0 )
		return fail( "no glTexStorageMem3DEXT (a 2D array / cube array twin)" );
	if ( w < 1 || h < 1 || ( type != 0 && type != 2 && type != 3 ) || ( type == 3 && ( layers < 6 || layers % 6 ) ) )
		return fail( "a far-card twin of size " + std::to_string( w ) + "x" + std::to_string( h ) + "x" + std::to_string( layers ) + " type "
			+ std::to_string( type ) );
	const FcFormat f = fcFormat( GLenum( glFmt ) );
	if ( f.fmt == VK_FORMAT_UNDEFINED )
		return fail( "GL format " + std::to_string( glFmt ) + " has no far-card twin" );
	if ( !fcFeatures( f.block != 0, type == 3, why ) )
		return fail( why );
	layers = type == 0 ? 1 : std::max( layers, 1 );
	levels = f.depth ? 1 : std::max( levels, 1 );
	auto it = C.twins.find( key );
	if ( it != C.twins.end() ) {
		const Shared & s = it->second;
		if ( s.glCtx == g.ctx && s.type == type && s.w == w && s.h == h && s.d == layers && s.glFmt == glFmt && s.levels == levels ) {
			tex = s.tex;
			return true;
		}
	}
	drain();
	v.vkDeviceWaitIdle( v.dev );
	if ( it != C.twins.end() ) {
		C.st.mb -= it->second.mb;
		sharedFree( it->second, it->second.glCtx == g.ctx );
		C.twins.erase( it );
		C.st.dropped++;
	}
	Shared s;
	if ( !fcMake( s, f, GLenum( glFmt ), type, w, h, layers, levels, why ) ) {
		sharedFree( s, true );
		return fail( why );
	}
	s.glCtx = g.ctx;
	s.type = type;
	s.w = w;
	s.h = h;
	s.d = layers;
	s.levels = levels;
	s.glFmt = GLenum( glFmt );
	s.fmt = f.fmt;
	C.st.made++;
	C.st.mb += s.mb;
	tex = s.tex;
	C.twins[key] = s;
	return true;
}

void farTwinDrop( std::uintptr_t key )
{
	FarCards & C = farCards();
	auto it = C.twins.find( key );
	if ( it == C.twins.end() )
		return;
	drain();
	vk().vkDeviceWaitIdle( vk().dev );
	C.st.mb -= it->second.mb;
	C.st.dropped++;
	sharedFree( it->second, gl().ok && it->second.glCtx == gl().ctx );
	C.twins.erase( it );
}

// one level of one layer as the present device sees it (the gates compare it with what GL wrote; GL finished first)
bool farTwinRead( std::uintptr_t key, int level, int layer, std::vector<std::uint8_t> & out, std::string & why )
{
	Vk & v = vk();
	const Shared * s = farTwinFind( key );
	if ( !s || !s->img ) {
		why = "no far-card twin of that key";
		return false;
	}
	const FcFormat f = fcFormat( s->glFmt );
	const int lw = std::max( s->w >> level, 1 ), lh = std::max( s->h >> level, 1 );
	const VkDeviceSize bytes = fcLayerBytes( f, lw, lh );
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
		c.imageSubresource = { VkImageAspectFlags( f.depth ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT ), std::uint32_t( level ),
			std::uint32_t( layer ), 1 };
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
		why = "the far-card twin's read back failed";
	return ok;
}

FarTwinStat farTwinStat()
{
	return farCards().st;
}

// shutdown(): the twins go with the view (GL alive: its names and memory objects too)
void farCardsFree( bool glAlive )
{
	FarCards & C = farCards();
	if ( C.twins.empty() )
		return;
	drain();
	vk().vkDeviceWaitIdle( vk().dev );
	for ( auto & kv : C.twins )
		sharedFree( kv.second, glAlive && kv.second.glCtx == gl().ctx );
	C.twins.clear();
	C.st.mb = 0.0;
}
}  // namespace WwVkPresent
