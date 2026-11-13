/* lane VKFAR1 round 2 (row G3, 2026-11-13): what the far cards (impostor_cellinstez) need on the present device before
 * they can draw there. Included once by vkpresent.cpp after vkfardots.inc (it reads that file's anonymous-namespace
 * parts: Shared, sharedFree, memType, oneShot, imageView, vkFormatOf).
 * 1. twins sharedTarget does not make: the card sheets' BC formats (GL's compressed names) and cube-map arrays (the
 *    placed lights' shadow cubes, GL_DEPTH_COMPONENT16). Same contract as sharedTarget (one image both APIs see, keyed,
 *    GENERAL on both sides, GL copies into it), its own key map so sharedTarget's twins and stats stay as they were.
 * 2. texel buffers both APIs see (farTexelBuffer). 3. the cards' descriptor set, reflected from their module (farCardsSet). */
#include "features/vkframe/farcards_view_ps.spv.h"
#include "features/vkframe/farcards_view_vs.spv.h"  // lane VKFAR2: impostor_instez_vs, its bindings 0 / 32 / 96 / 97 in the same set
namespace
{
constexpr GLenum kFcCubeArray = 0x9009, kFcBindingCubeArray = 0x900A, kFcDepth16 = 0x81A5;
constexpr std::uint32_t kFcSets = 256;  // lane VKFAR2: descriptor sets, one a card set drawn in a pass (more: GL draws the pass)

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

// 2. a texel buffer both APIs see (cellCluster int4, cellLights / cellFarSlots / cellFarRecs float4): the present device
//    makes the buffer on exportable memory, GL imports it (glBufferStorageMemEXT) and views it as GL_TEXTURE_BUFFER.
struct FcBuf
{
	VkBuffer buf = VK_NULL_HANDLE;
	VkDeviceMemory mem = VK_NULL_HANDLE;
	VkBufferView view = VK_NULL_HANDLE;
	VkDeviceSize bytes = 0;
	GLuint glBuf = 0, glTex = 0, obj = 0;
	GLenum glFmt = 0;
	void * glCtx = nullptr;
	double mb = 0.0;
};

// entry points nothing else on the present device or in vkpresent.cpp's Gl loads (read on first use, GL per context)
struct FcDev
{
	PFN_vkCreateBufferView vkCreateBufferView = nullptr;
	PFN_vkDestroyBufferView vkDestroyBufferView = nullptr;
	PFN_vkCmdCopyBuffer vkCmdCopyBuffer = nullptr;
	VkDevice dev = VK_NULL_HANDLE;
	std::uint32_t maxTexels = 0;
	void * glCtx = nullptr;
	void( WINAPI * genBuffers )( GLsizei, GLuint * ) = nullptr;
	void( WINAPI * deleteBuffers )( GLsizei, const GLuint * ) = nullptr;
	void( WINAPI * bindBuffer )( GLenum, GLuint ) = nullptr;
	void( WINAPI * bufferStorageMem )( GLenum, std::intptr_t, GLuint, GLuint64 ) = nullptr;
	void( WINAPI * texBuffer )( GLenum, GLenum, GLuint ) = nullptr;
};

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
	std::map<std::uintptr_t, FcBuf> bufs;
	FcDev d;
	// 3. the far cards' set (farCardsSet)
	VkDescriptorSetLayout setLayout = VK_NULL_HANDLE;
	VkDescriptorPool pool = VK_NULL_HANDLE;
	VkDescriptorSet set = VK_NULL_HANDLE;  // sets[0]
	std::vector<VkDescriptorSet> sets;     // lane VKFAR2: one a drawn card set (kFcSets)
	VkPipelineLayout pipeLayout = VK_NULL_HANDLE;
	VkShaderModule ps = VK_NULL_HANDLE, vs = VK_NULL_HANDLE;
	WwVkPresent::FarSetCounts counts;  // the pixel stage's (the vertex stage's 0 / 32 / 96 / 97 join the layout only)
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

constexpr GLenum kFcTexBuffer = 0x8C2A, kFcBindingTexBuffer = 0x8C2C, kFcRgba32f = 0x8814, kFcRgba32i = 0x8D82, kFcRgba32ui = 0x8D70;

VkFormat fcTexelFormat( GLenum f )
{
	return f == kFcRgba32f ? VK_FORMAT_R32G32B32A32_SFLOAT : f == kFcRgba32i ? VK_FORMAT_R32G32B32A32_SINT
		: f == kFcRgba32ui ? VK_FORMAT_R32G32B32A32_UINT : VK_FORMAT_UNDEFINED;
}

bool fcDev( std::string & why )
{
	Vk & v = vk();
	FcDev & d = farCards().d;
	if ( d.dev != v.dev ) {
		d = FcDev();
		d.vkCreateBufferView = reinterpret_cast<PFN_vkCreateBufferView>( v.vkGetDeviceProcAddr( v.dev, "vkCreateBufferView" ) );
		d.vkDestroyBufferView = reinterpret_cast<PFN_vkDestroyBufferView>( v.vkGetDeviceProcAddr( v.dev, "vkDestroyBufferView" ) );
		d.vkCmdCopyBuffer = reinterpret_cast<PFN_vkCmdCopyBuffer>( v.vkGetDeviceProcAddr( v.dev, "vkCmdCopyBuffer" ) );
		VkPhysicalDeviceProperties2 p { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2 };
		v.vkGetPhysicalDeviceProperties2( v.phys, &p );
		d.maxTexels = p.properties.limits.maxTexelBufferElements;
		d.dev = v.dev;
	}
	void * ctx = reinterpret_cast<void *>( wglGetCurrentContext() );
	if ( ctx && d.glCtx != ctx ) {
		auto ld = []( auto & p, const char * n ) { p = reinterpret_cast<std::remove_reference_t<decltype( p )>>( wglGetProcAddress( n ) ); };
		ld( d.genBuffers, "glGenBuffers" );
		ld( d.deleteBuffers, "glDeleteBuffers" );
		ld( d.bindBuffer, "glBindBuffer" );
		ld( d.bufferStorageMem, "glBufferStorageMemEXT" );
		ld( d.texBuffer, "glTexBuffer" );
		d.glCtx = ctx;
	}
	if ( !d.vkCreateBufferView || !d.vkDestroyBufferView || !d.vkCmdCopyBuffer ) {
		why = "the present device has no buffer views / buffer copies";
		return false;
	}
	if ( !d.genBuffers || !d.deleteBuffers || !d.bindBuffer || !d.bufferStorageMem || !d.texBuffer ) {
		why = "GL has no glBufferStorageMemEXT / glTexBuffer in this context";
		return false;
	}
	return true;
}

// the caller drained the worker and waited the device idle
void fcBufFree( FcBuf & b, bool glAlive )
{
	Vk & v = vk();
	FcDev & d = farCards().d;
	if ( glAlive && b.glCtx == gl().ctx ) {
		if ( b.glTex )
			gl().deleteTextures( 1, &b.glTex );
		if ( b.glBuf && d.deleteBuffers )
			d.deleteBuffers( 1, &b.glBuf );
		if ( b.obj )
			gl().deleteMemoryObjects( 1, &b.obj );
	}
	if ( b.view && d.vkDestroyBufferView )
		d.vkDestroyBufferView( v.dev, b.view, nullptr );
	if ( b.buf )
		v.vkDestroyBuffer( v.dev, b.buf, nullptr );
	if ( b.mem )
		v.vkFreeMemory( v.dev, b.mem, nullptr );
	b = FcBuf();
}

bool fcBufMake( FcBuf & b, GLenum glFmt, VkFormat fmt, VkDeviceSize bytes, std::string & why )
{
	Vk & v = vk();
	Gl & g = gl();
	FcDev & d = farCards().d;
	VkFormatProperties fp {};
	v.vkGetPhysicalDeviceFormatProperties( v.phys, fmt, &fp );
	if ( !( fp.bufferFeatures & VK_FORMAT_FEATURE_UNIFORM_TEXEL_BUFFER_BIT ) ) {
		why = "format " + std::to_string( int( fmt ) ) + " is no uniform texel buffer on the present device";
		return false;
	}
	if ( bytes / 16 > d.maxTexels ) {
		why = std::to_string( bytes / 16 ) + " texels, the device's texel buffers hold " + std::to_string( d.maxTexels );
		return false;
	}
	VkExternalMemoryBufferCreateInfo ext { VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_BUFFER_CREATE_INFO };
	ext.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT;
	VkBufferCreateInfo ci { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
	ci.pNext = &ext;
	ci.size = bytes;
	ci.usage = VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	if ( v.vkCreateBuffer( v.dev, &ci, nullptr, &b.buf ) != VK_SUCCESS ) {
		why = "vkCreateBuffer (a far-card texel buffer) failed";
		return false;
	}
	VkMemoryRequirements r;
	v.vkGetBufferMemoryRequirements( v.dev, b.buf, &r );
	VkMemoryDedicatedAllocateInfo ded { VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO };
	ded.buffer = b.buf;
	VkExportMemoryAllocateInfo exp { VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO };
	exp.pNext = &ded;
	exp.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT;
	VkMemoryAllocateInfo ai { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
	ai.pNext = &exp;
	ai.allocationSize = r.size;
	ai.memoryTypeIndex = memType( r.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );
	if ( ai.memoryTypeIndex == UINT32_MAX || v.vkAllocateMemory( v.dev, &ai, nullptr, &b.mem ) != VK_SUCCESS
		|| v.vkBindBufferMemory( v.dev, b.buf, b.mem, 0 ) != VK_SUCCESS ) {
		why = "no exportable device memory for a far-card texel buffer";
		return false;
	}
	b.bytes = bytes;
	b.mb = double( r.size ) / 1048576.0;
	VkBufferViewCreateInfo vi { VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO };
	vi.buffer = b.buf;
	vi.format = fmt;
	vi.range = VK_WHOLE_SIZE;
	if ( d.vkCreateBufferView( v.dev, &vi, nullptr, &b.view ) != VK_SUCCESS ) {
		why = "vkCreateBufferView (a far-card texel buffer) failed";
		return false;
	}
	VkMemoryGetWin32HandleInfoKHR mi { VK_STRUCTURE_TYPE_MEMORY_GET_WIN32_HANDLE_INFO_KHR };
	mi.memory = b.mem;
	mi.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT;
	HANDLE mh = nullptr;
	if ( v.vkGetMemoryWin32HandleKHR( v.dev, &mi, &mh ) != VK_SUCCESS || !mh ) {
		why = "vkGetMemoryWin32HandleKHR (a far-card texel buffer) failed";
		return false;
	}
	while ( g.getError() != 0 ) {}
	GLint wasBuf = 0, wasTex = 0;
	g.getIntegerv( kFcTexBuffer, &wasBuf );  // GL_TEXTURE_BUFFER_BINDING has the target's value
	g.getIntegerv( kFcBindingTexBuffer, &wasTex );
	g.createMemoryObjects( 1, &b.obj );
	const GLint one = 1;
	g.memoryObjectParameteriv( b.obj, kDedicated, &one );
	g.importMemoryWin32Handle( b.obj, GLuint64( r.size ), kHandleOpaqueWin32, mh );
	const GLenum errImport = g.getError();
	CloseHandle( mh );
	d.genBuffers( 1, &b.glBuf );
	d.bindBuffer( kFcTexBuffer, b.glBuf );
	d.bufferStorageMem( kFcTexBuffer, std::intptr_t( bytes ), b.obj, 0 );
	g.genTextures( 1, &b.glTex );
	g.bindTexture( kFcTexBuffer, b.glTex );
	d.texBuffer( kFcTexBuffer, glFmt, b.glBuf );
	const GLenum errStorage = g.getError();
	g.bindTexture( kFcTexBuffer, GLuint( wasTex ) );
	d.bindBuffer( kFcTexBuffer, GLuint( wasBuf ) );
	if ( errImport || errStorage ) {
		why = "the GL import of a far-card texel buffer failed (GL error at the import " + std::to_string( errImport ) + ", the storage "
			+ std::to_string( errStorage ) + ")";
		return false;
	}
	b.glFmt = glFmt;
	b.glCtx = g.ctx;
	return true;
}

// the caller drained the worker and waited the device idle
void fcSetFree()
{
	Vk & v = vk();
	FarCards & C = farCards();
	if ( C.ps )
		v.vkDestroyShaderModule( v.dev, C.ps, nullptr );
	if ( C.vs )
		v.vkDestroyShaderModule( v.dev, C.vs, nullptr );
	C.vs = VK_NULL_HANDLE;
	C.sets.clear();
	if ( C.pipeLayout )
		v.vkDestroyPipelineLayout( v.dev, C.pipeLayout, nullptr );
	if ( C.pool )
		v.vkDestroyDescriptorPool( v.dev, C.pool, nullptr );  // frees the set
	if ( C.setLayout )
		v.vkDestroyDescriptorSetLayout( v.dev, C.setLayout, nullptr );
	C.ps = VK_NULL_HANDLE;
	C.pipeLayout = VK_NULL_HANDLE;
	C.pool = VK_NULL_HANDLE;
	C.set = VK_NULL_HANDLE;
	C.setLayout = VK_NULL_HANDLE;
	C.counts = WwVkPresent::FarSetCounts();
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
	if ( C.twins.empty() && C.bufs.empty() && !C.setLayout && !C.ps )
		return;
	drain();
	vk().vkDeviceWaitIdle( vk().dev );
	for ( auto & kv : C.twins )
		sharedFree( kv.second, glAlive && kv.second.glCtx == gl().ctx );
	for ( auto & kv : C.bufs )
		fcBufFree( kv.second, glAlive );
	fcSetFree();
	C.twins.clear();
	C.bufs.clear();
	C.d = FcDev();
	C.st.mb = 0.0;
}

bool farTexelBuffer( std::uintptr_t key, unsigned glFmt, std::size_t bytes, unsigned & glBuf, unsigned & glTex, std::string & why )
{
	Gl & g = gl();
	Vk & v = vk();
	FarCards & C = farCards();
	glBuf = glTex = 0;
	auto fail = [&]( const std::string & r ) {
		why = r;
		C.st.fails++;
		C.st.failWhy = r;
		return false;
	};
	if ( !v.ok || !g.ok || g.ctx != reinterpret_cast<void *>( wglGetCurrentContext() ) )
		return fail( "the present device or GL's entry points are not ready in this context" );
	const VkFormat fmt = fcTexelFormat( GLenum( glFmt ) );
	if ( fmt == VK_FORMAT_UNDEFINED )
		return fail( "GL format " + std::to_string( glFmt ) + " has no far-card texel buffer (RGBA32F / RGBA32I / RGBA32UI)" );
	if ( bytes < 16 || bytes % 16 )
		return fail( "a far-card texel buffer of " + std::to_string( bytes ) + " bytes (whole 16-byte texels)" );
	if ( !fcDev( why ) )
		return fail( why );
	auto it = C.bufs.find( key );
	if ( it != C.bufs.end() && it->second.glCtx == g.ctx && it->second.glFmt == glFmt && it->second.bytes == bytes ) {
		glBuf = it->second.glBuf;
		glTex = it->second.glTex;
		return true;
	}
	drain();
	v.vkDeviceWaitIdle( v.dev );
	if ( it != C.bufs.end() ) {
		C.st.mb -= it->second.mb;
		fcBufFree( it->second, true );
		C.bufs.erase( it );
		C.st.dropped++;
	}
	FcBuf b;
	if ( !fcBufMake( b, GLenum( glFmt ), fmt, VkDeviceSize( bytes ), why ) ) {
		fcBufFree( b, true );
		return fail( why );
	}
	C.st.made++;
	C.st.mb += b.mb;
	glBuf = b.glBuf;
	glTex = b.glTex;
	C.bufs[key] = b;
	return true;
}

void farTexelBufferDrop( std::uintptr_t key )
{
	FarCards & C = farCards();
	auto it = C.bufs.find( key );
	if ( it == C.bufs.end() )
		return;
	drain();
	vk().vkDeviceWaitIdle( vk().dev );
	C.st.mb -= it->second.mb;
	C.st.dropped++;
	fcBufFree( it->second, gl().ok );
	C.bufs.erase( it );
}

// the bytes as the present device sees them (the gates compare them with what GL wrote; GL finished first)
bool farTexelBufferRead( std::uintptr_t key, std::vector<std::uint8_t> & out, std::string & why )
{
	Vk & v = vk();
	FarCards & C = farCards();
	auto it = C.bufs.find( key );
	if ( it == C.bufs.end() || !it->second.buf ) {
		why = "no far-card texel buffer of that key";
		return false;
	}
	const FcBuf & s = it->second;
	drain();
	v.vkDeviceWaitIdle( v.dev );
	VkBuffer buf = VK_NULL_HANDLE;
	VkDeviceMemory mem = VK_NULL_HANDLE;
	VkBufferCreateInfo bci { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
	bci.size = s.bytes;
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
		VkBufferCopy c { 0, 0, s.bytes };
		C.d.vkCmdCopyBuffer( cmd, s.buf, buf, 1, &c );
		v.vkEndCommandBuffer( cmd );
		ok = oneShot( cmd, fence );
	}
	if ( ok ) {
		void * p = nullptr;
		ok = v.vkMapMemory( v.dev, mem, 0, s.bytes, 0, &p ) == VK_SUCCESS;
		if ( ok ) {
			out.assign( static_cast<const std::uint8_t *>( p ), static_cast<const std::uint8_t *>( p ) + s.bytes );
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
		why = "the far-card texel buffer's read back failed";
	return ok;
}

std::uint32_t farTexelBufferMax()
{
	std::string why;
	fcDev( why );  // the Vulkan half reads the limit even where GL's entry points are missing
	return farCards().d.maxTexels;
}

// 3. the far cards' set, read from the module itself (the HLSL's registers after fx()'s shifts), so the layout follows
//    the shader and a resource the HLSL gains or drops cannot leave the two apart
bool farSetReflect( const std::uint32_t * w, std::size_t n, FarSetCounts & out, std::string & why )
{
	out = FarSetCounts();
	if ( !w || n < 5 || w[0] != 0x07230203u ) {
		why = "not SPIR-V";
		return false;
	}
	struct Img
	{
		std::uint32_t dim = 0, arrayed = 0, sampled = 0;
	};
	std::map<std::uint32_t, std::uint32_t> binding, setOf, ptrTo, varPtr, varStorage, loadOf;
	std::map<std::uint32_t, Img> images;
	std::map<std::uint32_t, int> samplerTypes, structs;
	std::vector<std::uint32_t> drefSampled;  // OpSampledImage results a depth-compare sample reads
	std::map<std::uint32_t, std::uint32_t> sampledSampler;  // OpSampledImage result -> the sampler value
	for ( std::size_t i = 5; i < n; ) {
		const std::uint32_t op = w[i] & 0xFFFFu, wc = w[i] >> 16;
		if ( !wc || i + wc > n ) {
			why = "a broken instruction at word " + std::to_string( i );
			return false;
		}
		const std::uint32_t * a = w + i + 1;
		switch ( op ) {
		case 71:  // OpDecorate
			if ( wc >= 4 && a[1] == 33 )
				binding[a[0]] = a[2];
			else if ( wc >= 4 && a[1] == 34 )
				setOf[a[0]] = a[2];
			break;
		case 25:  // OpTypeImage
			if ( wc >= 9 )
				images[a[0]] = { a[2], a[4], a[6] };
			break;
		case 26: samplerTypes[a[0]] = 1; break;  // OpTypeSampler
		case 30: structs[a[0]] = 1; break;       // OpTypeStruct
		case 32:                                  // OpTypePointer
			if ( wc >= 4 )
				ptrTo[a[0]] = a[2];
			break;
		case 59:  // OpVariable
			if ( wc >= 4 ) {
				varPtr[a[1]] = a[0];
				varStorage[a[1]] = a[2];
			}
			break;
		case 61:  // OpLoad
		case 83:  // OpCopyObject
			if ( wc >= 4 )
				loadOf[a[1]] = loadOf.count( a[2] ) ? loadOf[a[2]] : a[2];
			break;
		case 86:  // OpSampledImage
			if ( wc >= 5 )
				sampledSampler[a[1]] = loadOf.count( a[3] ) ? loadOf[a[3]] : a[3];
			break;
		case 89: case 90: case 91: case 92: case 97:  // the Dref samples and gathers
			if ( wc >= 4 )
				drefSampled.push_back( loadOf.count( a[2] ) ? loadOf[a[2]] : a[2] );
			break;
		default: break;
		}
		i += wc;
	}
	std::map<std::uint32_t, int> compareVars;
	for ( std::uint32_t s : drefSampled ) {
		auto it = sampledSampler.find( s );
		if ( it != sampledSampler.end() )
			compareVars[it->second] = 1;
	}
	std::map<std::uint32_t, FarSetSlot> binds;
	for ( const auto & kv : binding ) {
		const std::uint32_t var = kv.first;
		if ( !varPtr.count( var ) )
			continue;  // a binding on something other than a variable
		if ( setOf.count( var ) && setOf[var] != 0 ) {
			why = "binding " + std::to_string( kv.second ) + " is in set " + std::to_string( setOf[var] ) + ", the far cards use set 0";
			return false;
		}
		const std::uint32_t t = ptrTo.count( varPtr[var] ) ? ptrTo[varPtr[var]] : 0;
		FarSetSlot s;
		s.binding = kv.second;
		if ( images.count( t ) ) {
			const Img & im = images[t];
			if ( im.sampled != 1 ) {
				why = "binding " + std::to_string( s.binding ) + " is a storage image / texel buffer: the far cards read only";
				return false;
			}
			s.kind = im.dim == 5 ? 2 : 1;
			s.dim = int( im.dim );
			s.arrayed = im.arrayed != 0;
		} else if ( samplerTypes.count( t ) ) {
			s.kind = 3;
			s.compare = compareVars.count( var ) != 0;
		} else if ( structs.count( t ) && varStorage[var] == 2 ) {
			s.kind = 0;
		} else {
			why = "binding " + std::to_string( s.binding ) + " has a type the far cards' set does not take (an array, a combined sampler, a storage buffer)";
			return false;
		}
		if ( binds.count( s.binding ) ) {
			why = "binding " + std::to_string( s.binding ) + " is declared twice";
			return false;
		}
		binds[s.binding] = s;
	}
	for ( const auto & kv : binds ) {
		const FarSetSlot & s = kv.second;
		out.binds.push_back( s );
		out.ubos += s.kind == 0;
		out.images += s.kind == 1;
		out.texelBuffers += s.kind == 2;
		out.samplers += s.kind == 3;
		out.compareSamplers += s.kind == 3 && s.compare;
		out.cubeArrays += s.kind == 1 && s.dim == 3 && s.arrayed;
	}
	return true;
}

bool farCardsSet( FarSetCounts & out, std::string & why )
{
	Vk & v = vk();
	FarCards & C = farCards();
	if ( !v.ok ) {
		why = "no present device";
		return false;
	}
	if ( C.set ) {
		out = C.counts;
		return true;
	}
	if ( !farSetReflect( vkframe_farcards_view_ps_spv, sizeof( vkframe_farcards_view_ps_spv ) / 4, C.counts, why ) )
		return false;
	if ( C.counts.cubeArrays && !fcFeatures( false, true, why ) )
		return false;
	// lane VKFAR2: the vertex stage (impostor_instez_vs) shares the set: b0 with the pixel stage, its own b31 at 32 and its
	// texel buffers at 96 / 97, as vkframe_spirv.sh's fxc() places them (read from its module; anything else refuses)
	FarSetCounts vsc;
	if ( !farSetReflect( vkframe_farcards_view_vs_spv, sizeof( vkframe_farcards_view_vs_spv ) / 4, vsc, why ) ) {
		why = "the far cards' vertex stage: " + why;
		return false;
	}
	std::vector<VkDescriptorSetLayoutBinding> lb;
	std::map<VkDescriptorType, std::uint32_t> sizes;
	auto typeOf = []( int kind ) {
		return kind == 0 ? VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER : kind == 1 ? VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE
			: kind == 2 ? VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER : VK_DESCRIPTOR_TYPE_SAMPLER;
	};
	for ( const FarSetSlot & s : C.counts.binds ) {
		const VkDescriptorType t = typeOf( s.kind );
		lb.push_back( { s.binding, t, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr } );
		sizes[t] += kFcSets;
	}
	for ( const FarSetSlot & s : vsc.binds ) {
		const bool want = ( s.binding == 0 || s.binding == 32 ) ? s.kind == 0 : ( s.binding == 96 || s.binding == 97 ) && s.kind == 2;
		if ( !want ) {
			why = "the far cards' vertex stage has binding " + std::to_string( s.binding ) + " of kind " + std::to_string( s.kind )
				+ " (it takes b0 at 0, b31 at 32, two texel buffers at 96 / 97)";
			return false;
		}
		bool shared = false;
		for ( VkDescriptorSetLayoutBinding & b : lb )
			if ( b.binding == s.binding ) {
				if ( b.descriptorType != typeOf( s.kind ) ) {
					why = "binding " + std::to_string( s.binding ) + " has another type in the vertex stage";
					return false;
				}
				b.stageFlags |= VK_SHADER_STAGE_VERTEX_BIT;
				shared = true;
			}
		if ( !shared ) {
			lb.push_back( { s.binding, typeOf( s.kind ), 1, VK_SHADER_STAGE_VERTEX_BIT, nullptr } );
			sizes[typeOf( s.kind )] += kFcSets;
		}
	}
	if ( vsc.binds.size() != 4 ) {
		why = "the far cards' vertex stage declares " + std::to_string( vsc.binds.size() ) + " bindings, not 4";
		return false;
	}
	std::vector<VkDescriptorPoolSize> ps;
	for ( const auto & kv : sizes )
		ps.push_back( { kv.first, kv.second } );
	VkDescriptorSetLayoutCreateInfo li { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
	li.bindingCount = std::uint32_t( lb.size() );
	li.pBindings = lb.data();
	VkDescriptorPoolCreateInfo pi { VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
	pi.maxSets = kFcSets;
	pi.poolSizeCount = std::uint32_t( ps.size() );
	pi.pPoolSizes = ps.data();
	bool ok = v.vkCreateDescriptorSetLayout( v.dev, &li, nullptr, &C.setLayout ) == VK_SUCCESS
		&& v.vkCreateDescriptorPool( v.dev, &pi, nullptr, &C.pool ) == VK_SUCCESS;
	if ( ok ) {
		std::vector<VkDescriptorSetLayout> lays( kFcSets, C.setLayout );
		C.sets.assign( kFcSets, VK_NULL_HANDLE );
		VkDescriptorSetAllocateInfo ai { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
		ai.descriptorPool = C.pool;
		ai.descriptorSetCount = kFcSets;
		ai.pSetLayouts = lays.data();
		VkPipelineLayoutCreateInfo pl { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
		pl.setLayoutCount = 1;
		pl.pSetLayouts = &C.setLayout;
		ok = v.vkAllocateDescriptorSets( v.dev, &ai, C.sets.data() ) == VK_SUCCESS
			&& v.vkCreatePipelineLayout( v.dev, &pl, nullptr, &C.pipeLayout ) == VK_SUCCESS
			&& fxModule( vkframe_farcards_view_ps_spv, sizeof( vkframe_farcards_view_ps_spv ), C.ps )
			&& fxModule( vkframe_farcards_view_vs_spv, sizeof( vkframe_farcards_view_vs_spv ), C.vs );
		C.set = ok ? C.sets[0] : VK_NULL_HANDLE;
	}
	if ( !ok ) {
		why = "the far cards' set layout / pool / set / pipeline layout / module failed on the present device";
		drain();
		v.vkDeviceWaitIdle( v.dev );
		fcSetFree();
		return false;
	}
	out = C.counts;
	return true;
}
}  // namespace WwVkPresent
