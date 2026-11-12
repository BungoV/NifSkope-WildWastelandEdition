// Lane VKFRAME1 (2026-11-12, P2 + P3): the bloom's source and combine on Vulkan from the Fusion Shaders-ready HLSL
// (res/shaders/hlsl/post/bloom_src.hlsl, bloom_comb.hlsl over BloomCommon.hlsli) against GL's own output
// (tests/vkframe/srccomb_gl_ref.py writes the job folders). No NifSkope: a headless Vulkan device of its own.
// The multisample frame (RGBA16F colour + D24S8 depth / stencil, 4 samples) is drawn from the job's data the way the GL
// reference draws it: a per-sample pass (srccomb_gen_ps.hlsl) keeps a sample only where its stencil value is the draw's,
// stencil REPLACE, one draw a value. Then the source kernel reads the colour and a stencil-aspect view of the depth /
// stencil (as vkframe.cpp does on the GL-shared frame), and the combine kernel reads the job's top.
// Usage: srccombmatch <job dir> <src.spv> <comb.spv> <gen_vs.spv> <gen_ps.spv>
// Prints `SRCMATCH <job> EXACT|BOUND|FAIL ...`, `COMBMATCH <job> EXACT|BOUND|FAIL ...` and the GPU ms (timestamps, median
// of 32 after 2 warm-ups). Exit 0 when both are EXACT or BOUND (source: 1 fp16 ulp from the resolve's sum order; combine:
// EXACT only).
#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include <windows.h>
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <string>
#include <vector>

#define VKF( name ) static PFN_##name name
VKF( vkCreateInstance ); VKF( vkEnumeratePhysicalDevices ); VKF( vkGetPhysicalDeviceQueueFamilyProperties );
VKF( vkGetPhysicalDeviceProperties ); VKF( vkGetPhysicalDeviceMemoryProperties ); VKF( vkCreateDevice ); VKF( vkGetDeviceQueue );
VKF( vkGetPhysicalDeviceFormatProperties );
VKF( vkCreateBuffer ); VKF( vkGetBufferMemoryRequirements ); VKF( vkAllocateMemory ); VKF( vkBindBufferMemory ); VKF( vkMapMemory );
VKF( vkCreateImage ); VKF( vkGetImageMemoryRequirements ); VKF( vkBindImageMemory ); VKF( vkCreateImageView ); VKF( vkCreateSampler );
VKF( vkCreateDescriptorSetLayout ); VKF( vkCreatePipelineLayout ); VKF( vkCreateShaderModule ); VKF( vkCreateComputePipelines );
VKF( vkCreateGraphicsPipelines ); VKF( vkCreateRenderPass ); VKF( vkCreateFramebuffer ); VKF( vkCmdBeginRenderPass );
VKF( vkCmdEndRenderPass ); VKF( vkCmdDraw ); VKF( vkCmdSetStencilReference );
VKF( vkCreateDescriptorPool ); VKF( vkAllocateDescriptorSets ); VKF( vkUpdateDescriptorSets ); VKF( vkCreateCommandPool );
VKF( vkAllocateCommandBuffers ); VKF( vkBeginCommandBuffer ); VKF( vkEndCommandBuffer ); VKF( vkCmdBindPipeline );
VKF( vkCmdBindDescriptorSets ); VKF( vkCmdPushConstants ); VKF( vkCmdDispatch ); VKF( vkQueueSubmit ); VKF( vkQueueWaitIdle );
VKF( vkCmdPipelineBarrier ); VKF( vkCmdCopyBufferToImage ); VKF( vkCmdCopyImageToBuffer ); VKF( vkCreateQueryPool );
VKF( vkCmdResetQueryPool ); VKF( vkCmdWriteTimestamp ); VKF( vkGetQueryPoolResults );

static VkDevice dev;
static VkPhysicalDevice phys;
static VkQueue queue;
static uint32_t family;
static VkPhysicalDeviceMemoryProperties memProps;
static const int kSamples = 4;

static std::vector<uint32_t> readSpv( const std::string & path )
{
	std::vector<uint32_t> v;
	FILE * f = std::fopen( path.c_str(), "rb" );
	if ( !f )
		return v;
	std::fseek( f, 0, SEEK_END );
	const long n = std::ftell( f );
	std::fseek( f, 0, SEEK_SET );
	v.resize( size_t( n ) / 4 );
	if ( std::fread( v.data(), 4, v.size(), f ) != v.size() )
		v.clear();
	std::fclose( f );
	return v;
}

// int32 W, H, then W * H * comps elements of `bytes` each
struct Raw { int w = 0, h = 0; std::vector<unsigned char> px; };

static bool readRaw( const std::string & path, Raw & out, size_t elem )
{
	FILE * f = std::fopen( path.c_str(), "rb" );
	if ( !f )
		return false;
	int32_t hdr[2] = {};
	bool ok = std::fread( hdr, 4, 2, f ) == 2;
	out.w = hdr[0];
	out.h = hdr[1];
	out.px.resize( size_t( out.w ) * out.h * elem );
	ok = ok && std::fread( out.px.data(), 1, out.px.size(), f ) == out.px.size();
	std::fclose( f );
	return ok;
}

static uint32_t memType( uint32_t bits, VkMemoryPropertyFlags want )
{
	for ( uint32_t t = 0; t < memProps.memoryTypeCount; t++ )
		if ( ( bits & ( 1u << t ) ) && ( memProps.memoryTypes[t].propertyFlags & want ) == want )
			return t;
	return 0;
}

struct Buf { VkBuffer b = VK_NULL_HANDLE; VkDeviceMemory m = VK_NULL_HANDLE; void * p = nullptr; size_t n = 0; };

static Buf makeBuf( size_t n )
{
	Buf B;
	B.n = n;
	VkBufferCreateInfo bi { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
	bi.size = n;
	bi.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	vkCreateBuffer( dev, &bi, nullptr, &B.b );
	VkMemoryRequirements r;
	vkGetBufferMemoryRequirements( dev, B.b, &r );
	VkMemoryAllocateInfo ai { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
	ai.allocationSize = r.size;
	ai.memoryTypeIndex = memType( r.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT );
	vkAllocateMemory( dev, &ai, nullptr, &B.m );
	vkBindBufferMemory( dev, B.b, B.m, 0 );
	vkMapMemory( dev, B.m, 0, VK_WHOLE_SIZE, 0, &B.p );
	return B;
}

struct Img { VkImage i = VK_NULL_HANDLE; VkDeviceMemory m = VK_NULL_HANDLE; VkImageView v = VK_NULL_HANDLE, sv = VK_NULL_HANDLE; int w = 0, h = 0; };

static Img makeImg( int w, int h, VkFormat fmt, int samples, VkImageUsageFlags usage, VkImageAspectFlags aspect )
{
	Img I;
	I.w = w;
	I.h = h;
	VkImageCreateInfo ci { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
	ci.imageType = VK_IMAGE_TYPE_2D;
	ci.format = fmt;
	ci.extent = { uint32_t( w ), uint32_t( h ), 1 };
	ci.mipLevels = 1;
	ci.arrayLayers = 1;
	ci.samples = VkSampleCountFlagBits( samples );
	ci.tiling = VK_IMAGE_TILING_OPTIMAL;
	ci.usage = usage;
	ci.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	if ( vkCreateImage( dev, &ci, nullptr, &I.i ) != VK_SUCCESS )
		return Img();
	VkMemoryRequirements r;
	vkGetImageMemoryRequirements( dev, I.i, &r );
	VkMemoryAllocateInfo ai { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
	ai.allocationSize = r.size;
	ai.memoryTypeIndex = memType( r.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );
	vkAllocateMemory( dev, &ai, nullptr, &I.m );
	vkBindImageMemory( dev, I.i, I.m, 0 );
	VkImageViewCreateInfo vi { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
	vi.image = I.i;
	vi.viewType = VK_IMAGE_VIEW_TYPE_2D;
	vi.format = fmt;
	vi.subresourceRange = { aspect, 0, 1, 0, 1 };
	vkCreateImageView( dev, &vi, nullptr, &I.v );
	if ( aspect == ( VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT ) ) {	// the stencil-aspect view the kernel reads
		vi.subresourceRange.aspectMask = VK_IMAGE_ASPECT_STENCIL_BIT;
		vkCreateImageView( dev, &vi, nullptr, &I.sv );
	}
	return I;
}

static VkCommandPool cpool;
static VkCommandBuffer newCb()
{
	VkCommandBufferAllocateInfo ca { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
	ca.commandPool = cpool;
	ca.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	ca.commandBufferCount = 1;
	VkCommandBuffer cb = VK_NULL_HANDLE;
	vkAllocateCommandBuffers( dev, &ca, &cb );
	VkCommandBufferBeginInfo bi { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
	vkBeginCommandBuffer( cb, &bi );
	return cb;
}

static void submitWait( VkCommandBuffer cb )
{
	vkEndCommandBuffer( cb );
	VkSubmitInfo si { VK_STRUCTURE_TYPE_SUBMIT_INFO };
	si.commandBufferCount = 1;
	si.pCommandBuffers = &cb;
	vkQueueSubmit( queue, 1, &si, VK_NULL_HANDLE );
	vkQueueWaitIdle( queue );
}

static void barrier( VkCommandBuffer cb, VkPipelineStageFlags src, VkAccessFlags sa, VkPipelineStageFlags dst, VkAccessFlags da )
{
	VkMemoryBarrier mb { VK_STRUCTURE_TYPE_MEMORY_BARRIER };
	mb.srcAccessMask = sa;
	mb.dstAccessMask = da;
	vkCmdPipelineBarrier( cb, src, dst, 0, 1, &mb, 0, nullptr, 0, nullptr );
}

static void toGeneral( VkCommandBuffer cb, VkImage i, VkImageAspectFlags aspect )
{
	VkImageMemoryBarrier b { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
	b.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	b.newLayout = VK_IMAGE_LAYOUT_GENERAL;
	b.srcQueueFamilyIndex = b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	b.image = i;
	b.subresourceRange = { aspect, 0, 1, 0, 1 };
	b.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
	vkCmdPipelineBarrier( cb, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, 1, &b );
}

static void upload( const Img & I, const void * px, size_t n, VkImageAspectFlags aspect )
{
	Buf b = makeBuf( n );
	std::memcpy( b.p, px, n );
	VkCommandBuffer cb = newCb();
	toGeneral( cb, I.i, aspect );
	VkBufferImageCopy r {};
	r.imageSubresource = { aspect, 0, 0, 1 };
	r.imageExtent = { uint32_t( I.w ), uint32_t( I.h ), 1 };
	vkCmdCopyBufferToImage( cb, b.b, I.i, VK_IMAGE_LAYOUT_GENERAL, 1, &r );
	submitWait( cb );
}

static void download( const Img & I, void * px, size_t n )
{
	Buf b = makeBuf( n );
	VkCommandBuffer cb = newCb();
	barrier( cb, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_MEMORY_WRITE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_READ_BIT );
	VkBufferImageCopy r {};
	r.imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
	r.imageExtent = { uint32_t( I.w ), uint32_t( I.h ), 1 };
	vkCmdCopyImageToBuffer( cb, I.i, VK_IMAGE_LAYOUT_GENERAL, b.b, 1, &r );
	submitWait( cb );
	std::memcpy( px, b.p, n );
}

static VkShaderModule module( const std::vector<uint32_t> & spv )
{
	VkShaderModuleCreateInfo mi { VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
	mi.codeSize = spv.size() * 4;
	mi.pCode = spv.data();
	VkShaderModule m = VK_NULL_HANDLE;
	vkCreateShaderModule( dev, &mi, nullptr, &m );
	return m;
}

// BloomBindingsVk.hlsli BloomJob
struct Job { float params[4]; float spare[4]; int32_t ints[4]; };

static int ulps16( uint16_t a, uint16_t b )
{
	auto ord = []( uint16_t x ) { return ( x & 0x8000 ) ? -int( x & 0x7FFF ) : int( x ); };
	auto nan = []( uint16_t x ) { return ( x & 0x7C00 ) == 0x7C00 && ( x & 0x03FF ); };
	if ( nan( a ) || nan( b ) )
		return nan( a ) && nan( b ) ? 0 : 1 << 20;
	return std::abs( ord( a ) - ord( b ) );
}

static long long ulps32( uint32_t a, uint32_t b )
{
	auto ord = []( uint32_t x ) { return ( x & 0x80000000u ) ? -(long long)( x & 0x7FFFFFFFu ) : (long long)( x ); };
	auto nan = []( uint32_t x ) { return ( x & 0x7F800000u ) == 0x7F800000u && ( x & 0x007FFFFFu ); };
	if ( nan( a ) || nan( b ) )
		return nan( a ) && nan( b ) ? 0 : 1ll << 40;
	return std::llabs( ord( a ) - ord( b ) );
}

int main( int argc, char ** argv )
{
	std::setvbuf( stdout, nullptr, _IONBF, 0 );
	if ( argc < 6 ) {
		std::printf( "usage: srccombmatch <job dir> <src.spv> <comb.spv> <gen_vs.spv> <gen_ps.spv>\n" );
		return 2;
	}
	const std::string dir = argv[1];
	std::string name = dir;
	while ( !name.empty() && ( name.back() == '/' || name.back() == '\\' ) )
		name.pop_back();
	name = name.substr( name.find_last_of( "/\\" ) + 1 );
	int W = 0, H = 0, S = 0, on = 1;
	float expo = 1, impulse = 0, radius = 1, disp = 0;
	{
		FILE * f = std::fopen( ( dir + "/job.txt" ).c_str(), "r" );
		char key[32];
		while ( f && std::fscanf( f, "%31s", key ) == 1 ) {
			const std::string k = key;
			if ( k == "w" ) std::fscanf( f, "%d", &W );
			else if ( k == "h" ) std::fscanf( f, "%d", &H );
			else if ( k == "samples" ) std::fscanf( f, "%d", &S );
			else if ( k == "on" ) std::fscanf( f, "%d", &on );
			else if ( k == "expo" ) std::fscanf( f, "%f", &expo );
			else if ( k == "impulse" ) std::fscanf( f, "%f", &impulse );
			else if ( k == "radius" ) std::fscanf( f, "%f", &radius );
			else if ( k == "disp" ) std::fscanf( f, "%f", &disp );
		}
		if ( f )
			std::fclose( f );
	}
	Raw data, sten, top, glSrc, glComb;
	if ( W <= 0 || H <= 0 || S != kSamples || !readRaw( dir + "/data.bin", data, 8 ) || !readRaw( dir + "/sten.bin", sten, 1 )
		|| !readRaw( dir + "/top.bin", top, 8 ) || !readRaw( dir + "/gl_src.bin", glSrc, 8 ) || !readRaw( dir + "/gl_comb.bin", glComb, 16 ) ) {
		std::printf( "SRCMATCH %s FAIL (job files)\n", name.c_str() );
		return 2;
	}
	const std::vector<uint32_t> spvSrc = readSpv( argv[2] ), spvComb = readSpv( argv[3] ), spvVs = readSpv( argv[4] ), spvPs = readSpv( argv[5] );
	if ( spvSrc.empty() || spvComb.empty() || spvVs.empty() || spvPs.empty() ) {
		std::printf( "SRCMATCH %s FAIL (spv)\n", name.c_str() );
		return 2;
	}
	HMODULE lib = LoadLibraryA( "vulkan-1.dll" );
	if ( !lib ) {
		std::printf( "SRCMATCH %s FAIL (no vulkan-1.dll)\n", name.c_str() );
		return 2;
	}
	auto gipa = reinterpret_cast<PFN_vkGetInstanceProcAddr>( GetProcAddress( lib, "vkGetInstanceProcAddr" ) );
	vkCreateInstance = reinterpret_cast<PFN_vkCreateInstance>( gipa( nullptr, "vkCreateInstance" ) );
	VkApplicationInfo app { VK_STRUCTURE_TYPE_APPLICATION_INFO };
	app.apiVersion = VK_API_VERSION_1_2;
	VkInstanceCreateInfo ici { VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };
	ici.pApplicationInfo = &app;
	VkInstance inst;
	if ( vkCreateInstance( &ici, nullptr, &inst ) != VK_SUCCESS ) {
		std::printf( "SRCMATCH %s FAIL (instance)\n", name.c_str() );
		return 2;
	}
#define LOADI( fn ) fn = reinterpret_cast<PFN_##fn>( gipa( inst, #fn ) )
	LOADI( vkEnumeratePhysicalDevices ); LOADI( vkGetPhysicalDeviceQueueFamilyProperties ); LOADI( vkGetPhysicalDeviceProperties );
	LOADI( vkGetPhysicalDeviceMemoryProperties ); LOADI( vkCreateDevice ); LOADI( vkGetDeviceQueue );
	LOADI( vkGetPhysicalDeviceFormatProperties );
	uint32_t nd = 0;
	vkEnumeratePhysicalDevices( inst, &nd, nullptr );
	std::vector<VkPhysicalDevice> pds( nd );
	vkEnumeratePhysicalDevices( inst, &nd, pds.data() );
	if ( !nd ) {
		std::printf( "SRCMATCH %s FAIL (no device)\n", name.c_str() );
		return 2;
	}
	phys = pds[0];
	for ( VkPhysicalDevice p : pds ) {
		VkPhysicalDeviceProperties pp;
		vkGetPhysicalDeviceProperties( p, &pp );
		if ( pp.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU ) {
			phys = p;
			break;
		}
	}
	VkPhysicalDeviceProperties props;
	vkGetPhysicalDeviceProperties( phys, &props );
	vkGetPhysicalDeviceMemoryProperties( phys, &memProps );
	uint32_t nq = 0;
	vkGetPhysicalDeviceQueueFamilyProperties( phys, &nq, nullptr );
	std::vector<VkQueueFamilyProperties> qf( nq );
	vkGetPhysicalDeviceQueueFamilyProperties( phys, &nq, qf.data() );
	family = UINT32_MAX;	// graphics: the frame is drawn here (in the app GL draws it; the kernels run on the compute family)
	for ( uint32_t i = 0; i < nq && family == UINT32_MAX; i++ )
		if ( ( qf[i].queueFlags & VK_QUEUE_GRAPHICS_BIT ) && ( qf[i].queueFlags & VK_QUEUE_COMPUTE_BIT ) && qf[i].timestampValidBits )
			family = i;
	if ( family == UINT32_MAX ) {
		std::printf( "SRCMATCH %s FAIL (no graphics family)\n", name.c_str() );
		return 2;
	}
	const float prio = 1.0f;
	VkDeviceQueueCreateInfo qi { VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO };
	qi.queueFamilyIndex = family;
	qi.queueCount = 1;
	qi.pQueuePriorities = &prio;
	VkPhysicalDeviceFeatures feat {};
	feat.sampleRateShading = VK_TRUE;
	VkDeviceCreateInfo di { VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
	di.queueCreateInfoCount = 1;
	di.pQueueCreateInfos = &qi;
	di.pEnabledFeatures = &feat;
	if ( vkCreateDevice( phys, &di, nullptr, &dev ) != VK_SUCCESS ) {
		std::printf( "SRCMATCH %s FAIL (device)\n", name.c_str() );
		return 2;
	}
	auto gdpa = reinterpret_cast<PFN_vkGetDeviceProcAddr>( gipa( inst, "vkGetDeviceProcAddr" ) );
#define LOADD( fn ) fn = reinterpret_cast<PFN_##fn>( gdpa( dev, #fn ) )
	LOADD( vkCreateBuffer ); LOADD( vkGetBufferMemoryRequirements ); LOADD( vkAllocateMemory ); LOADD( vkBindBufferMemory );
	LOADD( vkMapMemory ); LOADD( vkCreateImage ); LOADD( vkGetImageMemoryRequirements ); LOADD( vkBindImageMemory );
	LOADD( vkCreateImageView ); LOADD( vkCreateSampler ); LOADD( vkCreateDescriptorSetLayout ); LOADD( vkCreatePipelineLayout );
	LOADD( vkCreateShaderModule ); LOADD( vkCreateComputePipelines ); LOADD( vkCreateGraphicsPipelines ); LOADD( vkCreateRenderPass );
	LOADD( vkCreateFramebuffer ); LOADD( vkCmdBeginRenderPass ); LOADD( vkCmdEndRenderPass ); LOADD( vkCmdDraw );
	LOADD( vkCmdSetStencilReference );
	LOADD( vkCreateDescriptorPool ); LOADD( vkAllocateDescriptorSets ); LOADD( vkUpdateDescriptorSets ); LOADD( vkCreateCommandPool );
	LOADD( vkAllocateCommandBuffers ); LOADD( vkBeginCommandBuffer ); LOADD( vkEndCommandBuffer ); LOADD( vkCmdBindPipeline );
	LOADD( vkCmdBindDescriptorSets ); LOADD( vkCmdPushConstants ); LOADD( vkCmdDispatch ); LOADD( vkQueueSubmit );
	LOADD( vkQueueWaitIdle ); LOADD( vkCmdPipelineBarrier ); LOADD( vkCmdCopyBufferToImage ); LOADD( vkCmdCopyImageToBuffer );
	LOADD( vkCreateQueryPool ); LOADD( vkCmdResetQueryPool ); LOADD( vkCmdWriteTimestamp ); LOADD( vkGetQueryPoolResults );
	vkGetDeviceQueue( dev, family, 0, &queue );
	VkCommandPoolCreateInfo cpi { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
	cpi.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	cpi.queueFamilyIndex = family;
	vkCreateCommandPool( dev, &cpi, nullptr, &cpool );

	// images: the generator's data, the multisample frame, the source, the top, the combine
	const VkImageUsageFlags up = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	const VkImageAspectFlags DS = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
	Img iData = makeImg( W * S, H, VK_FORMAT_R16G16B16A16_SFLOAT, 1, up, VK_IMAGE_ASPECT_COLOR_BIT );
	Img iSten = makeImg( W * S, H, VK_FORMAT_R8_UINT, 1, up, VK_IMAGE_ASPECT_COLOR_BIT );
	Img iColor = makeImg( W, H, VK_FORMAT_R16G16B16A16_SFLOAT, S, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
		VK_IMAGE_ASPECT_COLOR_BIT );
	Img iDs = makeImg( W, H, VK_FORMAT_D24_UNORM_S8_UINT, S, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, DS );
	const VkImageUsageFlags rw = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT
		| VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	Img iSrc = makeImg( W, H, VK_FORMAT_R16G16B16A16_SFLOAT, 1, rw, VK_IMAGE_ASPECT_COLOR_BIT );
	Img iTop = makeImg( top.w, top.h, VK_FORMAT_R16G16B16A16_SFLOAT, 1, rw, VK_IMAGE_ASPECT_COLOR_BIT );
	Img iComb = makeImg( W, H, VK_FORMAT_R32G32B32A32_SFLOAT, 1, rw, VK_IMAGE_ASPECT_COLOR_BIT );
	if ( !iData.v || !iSten.v || !iColor.v || !iDs.sv || !iSrc.v || !iTop.v || !iComb.v ) {
		std::printf( "SRCMATCH %s FAIL (images: D24S8 x%d sampled refused?)\n", name.c_str(), S );
		return 2;
	}
	upload( iData, data.px.data(), data.px.size(), VK_IMAGE_ASPECT_COLOR_BIT );
	upload( iSten, sten.px.data(), sten.px.size(), VK_IMAGE_ASPECT_COLOR_BIT );
	upload( iTop, top.px.data(), top.px.size(), VK_IMAGE_ASPECT_COLOR_BIT );
	{
		VkCommandBuffer cb = newCb();
		toGeneral( cb, iSrc.i, VK_IMAGE_ASPECT_COLOR_BIT );
		toGeneral( cb, iComb.i, VK_IMAGE_ASPECT_COLOR_BIT );
		submitWait( cb );
	}

	// the frame's draw: render pass (clear, store, GENERAL after), per-sample pipeline, stencil REPLACE with a dynamic reference
	VkAttachmentDescription at[2] = {};
	at[0].format = VK_FORMAT_R16G16B16A16_SFLOAT;
	at[0].samples = VkSampleCountFlagBits( S );
	at[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	at[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	at[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	at[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	at[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	at[0].finalLayout = VK_IMAGE_LAYOUT_GENERAL;
	at[1] = at[0];
	at[1].format = VK_FORMAT_D24_UNORM_S8_UINT;
	at[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	at[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_STORE;
	VkAttachmentReference cr { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL }, dr { 1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL };
	VkSubpassDescription sp {};
	sp.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	sp.colorAttachmentCount = 1;
	sp.pColorAttachments = &cr;
	sp.pDepthStencilAttachment = &dr;
	VkRenderPassCreateInfo rpi { VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO };
	rpi.attachmentCount = 2;
	rpi.pAttachments = at;
	rpi.subpassCount = 1;
	rpi.pSubpasses = &sp;
	VkRenderPass rp;
	vkCreateRenderPass( dev, &rpi, nullptr, &rp );
	VkImageView fv[2] = { iColor.v, iDs.v };
	VkFramebufferCreateInfo fbi { VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
	fbi.renderPass = rp;
	fbi.attachmentCount = 2;
	fbi.pAttachments = fv;
	fbi.width = uint32_t( W );
	fbi.height = uint32_t( H );
	fbi.layers = 1;
	VkFramebuffer fb;
	vkCreateFramebuffer( dev, &fbi, nullptr, &fb );
	VkDescriptorSetLayoutBinding gb[2] = { { 0, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr },
		{ 1, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr } };
	VkDescriptorSetLayoutCreateInfo gli { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
	gli.bindingCount = 2;
	gli.pBindings = gb;
	VkDescriptorSetLayout gLayout;
	vkCreateDescriptorSetLayout( dev, &gli, nullptr, &gLayout );
	VkPushConstantRange gpr { VK_SHADER_STAGE_FRAGMENT_BIT, 0, 8 };
	VkPipelineLayoutCreateInfo gpi { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
	gpi.setLayoutCount = 1;
	gpi.pSetLayouts = &gLayout;
	gpi.pushConstantRangeCount = 1;
	gpi.pPushConstantRanges = &gpr;
	VkPipelineLayout gPipeLayout;
	vkCreatePipelineLayout( dev, &gpi, nullptr, &gPipeLayout );
	VkPipelineShaderStageCreateInfo st[2] = {
		{ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_VERTEX_BIT, module( spvVs ), "vs", nullptr },
		{ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_FRAGMENT_BIT, module( spvPs ), "ps", nullptr } };
	VkPipelineVertexInputStateCreateInfo vin { VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
	VkPipelineInputAssemblyStateCreateInfo ia { VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
	ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	VkViewport vp { 0, 0, float( W ), float( H ), 0, 1 };
	VkRect2D sc { { 0, 0 }, { uint32_t( W ), uint32_t( H ) } };
	VkPipelineViewportStateCreateInfo vps { VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO };
	vps.viewportCount = 1;
	vps.pViewports = &vp;
	vps.scissorCount = 1;
	vps.pScissors = &sc;
	VkPipelineRasterizationStateCreateInfo rs { VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
	rs.polygonMode = VK_POLYGON_MODE_FILL;
	rs.cullMode = VK_CULL_MODE_NONE;
	rs.lineWidth = 1.0f;
	VkPipelineMultisampleStateCreateInfo ms { VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
	ms.rasterizationSamples = VkSampleCountFlagBits( S );
	ms.sampleShadingEnable = VK_TRUE;
	ms.minSampleShading = 1.0f;
	VkPipelineDepthStencilStateCreateInfo ds { VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };
	ds.stencilTestEnable = VK_TRUE;
	ds.front = { VK_STENCIL_OP_KEEP, VK_STENCIL_OP_REPLACE, VK_STENCIL_OP_KEEP, VK_COMPARE_OP_ALWAYS, 0xFF, 0xFF, 0 };
	ds.back = ds.front;
	VkPipelineColorBlendAttachmentState cba {};
	cba.colorWriteMask = 0xF;
	VkPipelineColorBlendStateCreateInfo cb { VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
	cb.attachmentCount = 1;
	cb.pAttachments = &cba;
	const VkDynamicState dyn = VK_DYNAMIC_STATE_STENCIL_REFERENCE;
	VkPipelineDynamicStateCreateInfo dy { VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
	dy.dynamicStateCount = 1;
	dy.pDynamicStates = &dyn;
	VkGraphicsPipelineCreateInfo gpc { VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
	gpc.stageCount = 2;
	gpc.pStages = st;
	gpc.pVertexInputState = &vin;
	gpc.pInputAssemblyState = &ia;
	gpc.pViewportState = &vps;
	gpc.pRasterizationState = &rs;
	gpc.pMultisampleState = &ms;
	gpc.pDepthStencilState = &ds;
	gpc.pColorBlendState = &cb;
	gpc.pDynamicState = &dy;
	gpc.layout = gPipeLayout;
	gpc.renderPass = rp;
	VkPipeline gPipe = VK_NULL_HANDLE;
	if ( vkCreateGraphicsPipelines( dev, VK_NULL_HANDLE, 1, &gpc, nullptr, &gPipe ) != VK_SUCCESS ) {
		std::printf( "SRCMATCH %s FAIL (generator pipeline)\n", name.c_str() );
		return 2;
	}

	// the kernels' layout: BloomBindingsVk.hlsli 0..7 (bloom_src: 4 target, 5 colour, 6 stencil; bloom_comb: 0 top, 3 sampler, 7 comb)
	VkDescriptorSetLayoutBinding kb[8];
	const VkDescriptorType kt[8] = { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
		VK_DESCRIPTOR_TYPE_SAMPLER, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
		VK_DESCRIPTOR_TYPE_STORAGE_IMAGE };
	for ( uint32_t i = 0; i < 8; i++ )
		kb[i] = VkDescriptorSetLayoutBinding { i, kt[i], 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr };
	VkDescriptorSetLayoutCreateInfo kli { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
	kli.bindingCount = 8;
	kli.pBindings = kb;
	VkDescriptorSetLayout kLayout;
	vkCreateDescriptorSetLayout( dev, &kli, nullptr, &kLayout );
	VkPushConstantRange kpr { VK_SHADER_STAGE_COMPUTE_BIT, 0, 48 };
	VkPipelineLayoutCreateInfo kpi { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
	kpi.setLayoutCount = 1;
	kpi.pSetLayouts = &kLayout;
	kpi.pushConstantRangeCount = 1;
	kpi.pPushConstantRanges = &kpr;
	VkPipelineLayout kPipeLayout;
	vkCreatePipelineLayout( dev, &kpi, nullptr, &kPipeLayout );
	auto compute = [&]( const std::vector<uint32_t> & spv ) {
		VkComputePipelineCreateInfo pc { VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO };
		pc.stage = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_COMPUTE_BIT, module( spv ), "main", nullptr };
		pc.layout = kPipeLayout;
		VkPipeline p = VK_NULL_HANDLE;
		vkCreateComputePipelines( dev, VK_NULL_HANDLE, 1, &pc, nullptr, &p );
		return p;
	};
	const VkPipeline pSrc = compute( spvSrc ), pComb = compute( spvComb );
	if ( !pSrc || !pComb ) {
		std::printf( "SRCMATCH %s FAIL (kernel pipelines)\n", name.c_str() );
		return 2;
	}
	VkSampler linearClamp;
	{
		VkSamplerCreateInfo si { VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
		si.magFilter = si.minFilter = VK_FILTER_LINEAR;
		si.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
		si.addressModeU = si.addressModeV = si.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		vkCreateSampler( dev, &si, nullptr, &linearClamp );
	}
	VkDescriptorPoolSize ps[3] = { { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 32 }, { VK_DESCRIPTOR_TYPE_SAMPLER, 8 }, { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 16 } };
	VkDescriptorPoolCreateInfo dp { VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
	dp.maxSets = 8;
	dp.poolSizeCount = 3;
	dp.pPoolSizes = ps;
	VkDescriptorPool pool;
	vkCreateDescriptorPool( dev, &dp, nullptr, &pool );
	auto alloc = [&]( VkDescriptorSetLayout l ) {
		VkDescriptorSetAllocateInfo ai { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
		ai.descriptorPool = pool;
		ai.descriptorSetCount = 1;
		ai.pSetLayouts = &l;
		VkDescriptorSet s = VK_NULL_HANDLE;
		vkAllocateDescriptorSets( dev, &ai, &s );
		return s;
	};
	auto write = [&]( VkDescriptorSet s, uint32_t b, VkDescriptorType t, VkImageView v, VkSampler smp ) {
		VkDescriptorImageInfo ii { smp, v, VK_IMAGE_LAYOUT_GENERAL };
		VkWriteDescriptorSet w { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
		w.dstSet = s;
		w.dstBinding = b;
		w.descriptorCount = 1;
		w.descriptorType = t;
		w.pImageInfo = &ii;
		vkUpdateDescriptorSets( dev, 1, &w, 0, nullptr );
	};
	const VkDescriptorSet gSet = alloc( gLayout ), srcSet = alloc( kLayout ), combSet = alloc( kLayout );
	write( gSet, 0, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, iData.v, VK_NULL_HANDLE );
	write( gSet, 1, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, iSten.v, VK_NULL_HANDLE );
	write( srcSet, 4, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, iSrc.v, VK_NULL_HANDLE );
	write( srcSet, 5, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, iColor.v, VK_NULL_HANDLE );
	write( srcSet, 6, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, iDs.sv, VK_NULL_HANDLE );
	write( combSet, 0, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, iTop.v, VK_NULL_HANDLE );
	write( combSet, 3, VK_DESCRIPTOR_TYPE_SAMPLER, VK_NULL_HANDLE, linearClamp );
	write( combSet, 7, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, iComb.v, VK_NULL_HANDLE );

	// draw the frame
	{
		VkCommandBuffer c = newCb();
		VkClearValue cv[2] = {};
		VkRenderPassBeginInfo rb { VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
		rb.renderPass = rp;
		rb.framebuffer = fb;
		rb.renderArea = sc;
		rb.clearValueCount = 2;
		rb.pClearValues = cv;
		vkCmdBeginRenderPass( c, &rb, VK_SUBPASS_CONTENTS_INLINE );
		vkCmdBindPipeline( c, VK_PIPELINE_BIND_POINT_GRAPHICS, gPipe );
		vkCmdBindDescriptorSets( c, VK_PIPELINE_BIND_POINT_GRAPHICS, gPipeLayout, 0, 1, &gSet, 0, nullptr );
		for ( int v : { 0, 1, 2, 3, 5, 7 } ) {
			const int32_t pc[2] = { S, v };
			vkCmdPushConstants( c, gPipeLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, 8, pc );
			vkCmdSetStencilReference( c, VK_STENCIL_FACE_FRONT_AND_BACK, uint32_t( v ) );
			vkCmdDraw( c, 3, 1, 0, 0 );
		}
		vkCmdEndRenderPass( c );
		submitWait( c );
	}

	VkQueryPool qp;
	{
		VkQueryPoolCreateInfo qpi { VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO };
		qpi.queryType = VK_QUERY_TYPE_TIMESTAMP;
		qpi.queryCount = 2;
		vkCreateQueryPool( dev, &qpi, nullptr, &qp );
	}
	auto run = [&]( VkPipeline p, VkDescriptorSet s, const Job & j, bool timeIt ) {
		VkCommandBuffer c = newCb();
		if ( timeIt ) {
			vkCmdResetQueryPool( c, qp, 0, 2 );
			vkCmdWriteTimestamp( c, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, qp, 0 );
		}
		barrier( c, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_MEMORY_WRITE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
			VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT );
		vkCmdBindPipeline( c, VK_PIPELINE_BIND_POINT_COMPUTE, p );
		vkCmdBindDescriptorSets( c, VK_PIPELINE_BIND_POINT_COMPUTE, kPipeLayout, 0, 1, &s, 0, nullptr );
		vkCmdPushConstants( c, kPipeLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof( Job ), &j );
		vkCmdDispatch( c, uint32_t( ( W + 7 ) / 8 ), uint32_t( ( H + 7 ) / 8 ), 1 );
		if ( timeIt )
			vkCmdWriteTimestamp( c, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, qp, 1 );
		submitWait( c );
		if ( !timeIt )
			return 0.0;
		uint64_t ts[2] = {};
		vkGetQueryPoolResults( dev, qp, 0, 2, sizeof( ts ), ts, 8, VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT );
		return double( ts[1] - ts[0] ) * double( props.limits.timestampPeriod ) * 1.0e-6;
	};
	auto median = [&]( VkPipeline p, VkDescriptorSet s, const Job & j ) {
		std::vector<double> t;
		for ( int r = 0; r < 34; r++ ) {
			const double ms = run( p, s, j, true );
			if ( r >= 2 )
				t.push_back( ms );
		}
		std::sort( t.begin(), t.end() );
		return t[t.size() / 2];
	};
	Job js {};
	js.params[0] = expo;
	js.params[1] = float( W / 2 );
	js.params[2] = float( H / 2 );
	js.params[3] = impulse;
	js.ints[0] = on;
	Job jc {};
	jc.params[0] = radius;
	jc.params[1] = 0.8f;
	jc.params[2] = disp;
	run( pSrc, srcSet, js, false );
	run( pComb, combSet, jc, false );
	const double msSrc = median( pSrc, srcSet, js ), msComb = median( pComb, combSet, jc );
	std::vector<uint16_t> vSrc( size_t( W ) * H * 4 );
	std::vector<uint32_t> vComb( size_t( W ) * H * 4 );
	download( iSrc, vSrc.data(), vSrc.size() * 2 );
	download( iComb, vComb.data(), vComb.size() * 4 );
	const uint16_t * gs = reinterpret_cast<const uint16_t *>( glSrc.px.data() );
	const uint32_t * gc = reinterpret_cast<const uint32_t *>( glComb.px.data() );
	// source: the definition on the CPU (bloom_src.hlsl's words): each lit sample max( v, 0 ) * exposure rounded to half, the
	// float mean of the samples, rounded to half (nearest even). GL's resolve blit averages in the hardware's own way (no
	// fp16 / fp32 sum order reproduces it: tests/vkframe notes), so Vulkan is held to the definition and GL to 2 ulp of it.
	auto toF = []( uint16_t h ) {
		const uint32_t s = ( h & 0x8000u ) << 16, e = ( h >> 10 ) & 31, m = h & 1023;
		uint32_t b;
		if ( e == 31 )
			b = s | 0x7F800000u | ( m << 13 );
		else if ( e )
			b = s | ( ( e + 112 ) << 23 ) | ( m << 13 );
		else {
			const float v = std::ldexp( float( m ), -24 );
			std::memcpy( &b, &v, 4 );
			b |= s;
		}
		float f;
		std::memcpy( &f, &b, 4 );
		return f;
	};
	auto toH = []( float f ) {	// round to nearest even, overflow to inf, NaN kept
		uint32_t b;
		std::memcpy( &b, &f, 4 );
		const uint16_t s = uint16_t( ( b >> 16 ) & 0x8000u );
		const uint32_t a = b & 0x7FFFFFFFu;
		if ( a > 0x7F800000u )
			return uint16_t( s | 0x7E00u );
		if ( a >= 0x477FF000u )	// 65520 and up round to inf
			return uint16_t( s | 0x7C00u );
		if ( a < 0x38800000u ) {	// half denormal: v * 2^24 rounded to an integer (nearest even)
			const float v = std::fabs( f ) * 16777216.0f;
			return uint16_t( s | uint16_t( std::nearbyint( v ) ) );
		}
		const uint32_t r = a + 0x0FFFu + ( ( a >> 13 ) & 1u );
		return uint16_t( s | ( ( r - 0x38000000u ) >> 13 ) );
	};
	const uint16_t * dpx = reinterpret_cast<const uint16_t *>( data.px.data() );
	std::vector<uint16_t> model( vSrc.size() );
	for ( int y = 0; y < H; y++ )
		for ( int x = 0; x < W; x++ )
			for ( int c = 0; c < 4; c++ ) {
				float r = 0.0f;
				if ( impulse > 0.0f )
					r = c == 3 ? 1.0f : ( x / 2 == ( W / 2 ) / 2 && y / 2 == ( H / 2 ) / 2 ? impulse : 0.0f );
				else if ( on ) {
					float sum = 0.0f;
					for ( int s = 0; s < S; s++ ) {
						const size_t q = size_t( y ) * W * S + size_t( x ) * S + s;
						if ( ( sten.px[q] & 3 ) != 1 )
							continue;
						const float v = toF( dpx[q * 4 + c] );
						sum += c == 3 ? 1.0f : toF( toH( ( v > 0.0f ? v : 0.0f ) * expo ) );	// HLSL max( NaN, 0 ) = 0
					}
					r = sum / float( S );
				}
				model[( size_t( y ) * W + x ) * 4 + c] = toH( r );
			}
	size_t dS = 0, dC = 0, nz = 0, dVm = 0;
	int mS = 0, mVm = 0, mGm = 0;
	long long mC = 0;
	for ( size_t i = 0; i < vSrc.size(); i++ ) {
		const int u = ulps16( vSrc[i], gs[i] ), um = ulps16( vSrc[i], model[i] ), ug = ulps16( gs[i], model[i] );
		static int shown = 0;
		if ( um > 0 && std::getenv( "SRCMATCH_SHOW" ) && shown++ < 12 )
			std::printf( "src diff x %zu y %zu c %zu: vk %04x gl %04x model %04x\n", ( i / 4 ) % W, ( i / 4 ) / W, i % 4, vSrc[i], gs[i],
				model[i] );
		dS += u != 0;
		mS = std::max( mS, u );
		dVm += um != 0;
		mVm = std::max( mVm, um );
		mGm = std::max( mGm, ug );
		nz += ( gs[i] & 0x7FFF ) != 0;
	}
	// combine: EXACT, or BOUND when the few differing floats sit in the dispersed channels (R, B) only, number at most 1 in
	// 10,000, and each is within one bilinear weight step (1/256, doubled) of the largest top texel its taps reach (the
	// dispersion's uv offset divides by the radial length; GL's and SPIR-V's divides may round 1 ulp apart and tip a texel's
	// 8-bit filter weight). A window holding a NaN texel may flip a whole tap (NaN -> 0): 4/16 of it.
	const uint16_t * tp = reinterpret_cast<const uint16_t *>( top.px.data() );
	auto half = []( uint16_t h ) {
		const uint32_t e = ( h >> 10 ) & 31, m = h & 1023;
		if ( e == 31 )
			return m ? std::nanf( "" ) : 65504.0f;	// inf: F4FX_SanitizeBloomSample's bound
		const float v = e ? std::ldexp( float( m | 1024 ), int( e ) - 25 ) : std::ldexp( float( m ), -24 );
		return v;
	};
	const int reach = int( std::ceil( radius + disp ) ) + 2;
	size_t outB = 0, offChan = 0;
	double worst = 0.0;
	for ( size_t i = 0; i < vComb.size(); i++ ) {
		const long long u = ulps32( vComb[i], gc[i] );
		if ( !u )
			continue;
		const size_t c = i % 4, px = i / 4, x = px % W, y = px / W;
		float a, b;
		std::memcpy( &a, &vComb[i], 4 );
		std::memcpy( &b, &gc[i], 4 );
		if ( dC < 4 )
			std::printf( "comb diff x %zu y %zu c %zu: vk %.9g gl %.9g\n", x, y, c, a, b );
		dC++;
		mC = std::max( mC, u );
		offChan += c != 0 && c != 2;
		const int tx = int( ( x + 0.5 ) * top.w / W ), ty = int( ( y + 0.5 ) * top.h / H );
		float big = 0.0f;
		bool nanIn = false;
		for ( int yy = std::max( 0, ty - reach ); yy <= std::min( top.h - 1, ty + reach ); yy++ )
			for ( int xx = std::max( 0, tx - reach ); xx <= std::min( top.w - 1, tx + reach ); xx++ ) {
				const float v = half( tp[( size_t( yy ) * top.w + xx ) * 4 + c] );
				if ( std::isnan( v ) )
					nanIn = true;
				else
					big = std::max( big, std::min( std::fabs( v ), 65504.0f ) );
			}
		const double d = std::fabs( double( a ) - double( b ) ), bound = nanIn ? big * 0.25 : big / 128.0;
		worst = std::max( worst, big > 0 ? d / big : ( d > 0 ? 1e9 : 0.0 ) );
		outB += !( d <= bound );
	}
	const bool combBound = dC > 0 && offChan == 0 && outB == 0 && dC * 10000 <= vComb.size();
	std::printf( "device %s, queue family %u (graphics + compute: the harness draws the frame)\n", props.deviceName, family );
	std::printf( "time %s: Vulkan source %.4f ms, combine %.4f ms\n", name.c_str(), msSrc, msComb );
	std::printf( "source %s: %zu of %zu halves differ, max %d ulp (%zu non-zero in GL's)\n", name.c_str(), dS, vSrc.size(), mS, nz );
	std::printf( "combine %s: %zu of %zu floats differ, max %lld ulp\n", name.c_str(), dC, vComb.size(), mC );
	std::printf( "source %s vs the definition: Vulkan %zu halves off, max %d ulp; GL's resolve max %d ulp\n", name.c_str(), dVm, mVm, mGm );
	const bool sOk = mS == 0 || ( mVm == 0 && mGm <= 2 ), cOk = mC == 0 || combBound;
	if ( mS == 0 )
		std::printf( "SRCMATCH %s EXACT\n", name.c_str() );
	else
		std::printf( "SRCMATCH %s %s (Vulkan = the definition %s, GL's resolve within %d ulp of it, Vulkan vs GL max %d ulp)\n", name.c_str(),
			sOk ? "BOUND" : "FAIL", mVm == 0 ? "bit for bit" : "NOT", mGm, mS );
	if ( mC == 0 )
		std::printf( "COMBMATCH %s EXACT\n", name.c_str() );
	else
		std::printf( "COMBMATCH %s %s (%zu floats differ, %zu off R/B, %zu past the filter-step bound, worst |diff| / top %.2e)\n",
			name.c_str(), combBound ? "BOUND" : "FAIL", dC, offChan, outB, worst );
	return sOk && cOk ? 0 : 1;
}
