// Lane VKFRAME1 (2026-11-12, P6): the cell view's tone map on Vulkan from the Fusion Shaders-ready HLSL
// (res/shaders/hlsl/post/tonemap.hlsl over ToneCommon.hlsli) against GL's own cell_hdr.frag output
// (tests/vkframe/tonemap_gl_ref.py writes the job folders). No NifSkope: a headless Vulkan device of its own.
// The multisample frame (RGBA16F + D24S8, 4 samples) and the fog's gain (R8, 4 samples) are drawn from the job's data the way
// the GL reference draws them (srccomb_gen.hlsl per sample, stencil REPLACE, one draw a value); the kernel then reads them,
// the combine, the measure's bloom, the LUT (3D) and the backdrop, and writes each sample's value at column x * S + s.
// Usage: tonemapmatch <job dir> <tonemap.spv> <gen_vs.spv> <gen_ps.spv>
// Prints `TONEMATCH <job> EXACT|BOUND|FAIL ...` and the GPU ms (timestamps, median of 32 after 0.5 s of warm-up dispatches).
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
static long long ulps32( uint32_t a, uint32_t b )
{
	auto ord = []( uint32_t x ) { return ( x & 0x80000000u ) ? -(long long)( x & 0x7FFFFFFFu ) : (long long)( x ); };
	auto nan = []( uint32_t x ) { return ( x & 0x7F800000u ) == 0x7F800000u && ( x & 0x007FFFFFu ); };
	if ( nan( a ) || nan( b ) )
		return nan( a ) && nan( b ) ? 0 : 1ll << 40;
	return std::llabs( ord( a ) - ord( b ) );
}

VKF( vkCmdClearColorImage );

// ToneBindingsVk.hlsli ToneJob (96 bytes)
struct ToneJob { float cineE[4], tint[4], wbExposure[4], bloomRect[4], fo[4]; int32_t ints[4]; };

static Img makeImg3( int n, VkFormat fmt, VkImageUsageFlags usage )
{
	Img I;
	I.w = n;
	I.h = n * n;
	VkImageCreateInfo ci { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
	ci.imageType = VK_IMAGE_TYPE_3D;
	ci.format = fmt;
	ci.extent = { uint32_t( n ), uint32_t( n ), uint32_t( n ) };
	ci.mipLevels = 1;
	ci.arrayLayers = 1;
	ci.samples = VK_SAMPLE_COUNT_1_BIT;
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
	vi.viewType = VK_IMAGE_VIEW_TYPE_3D;
	vi.format = fmt;
	vi.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
	vkCreateImageView( dev, &vi, nullptr, &I.v );
	return I;
}

static void upload3( const Img & I, int n, const void * px, size_t bytes )
{
	Buf b = makeBuf( bytes );
	std::memcpy( b.p, px, bytes );
	VkCommandBuffer cb = newCb();
	toGeneral( cb, I.i, VK_IMAGE_ASPECT_COLOR_BIT );
	VkBufferImageCopy r {};
	r.imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
	r.imageExtent = { uint32_t( n ), uint32_t( n ), uint32_t( n ) };
	vkCmdCopyBufferToImage( cb, b.b, I.i, VK_IMAGE_LAYOUT_GENERAL, 1, &r );
	submitWait( cb );
}

// a render target the generator draws per sample: colour (+ depth / stencil), clear, store, GENERAL after
struct Gen { VkRenderPass rp = VK_NULL_HANDLE; VkFramebuffer fb = VK_NULL_HANDLE; VkPipeline pipe = VK_NULL_HANDLE; };

static Gen makeGen( VkFormat fmt, const Img & color, const Img * ds, int W, int H, int S, VkPipelineLayout layout,
	VkShaderModule vs, VkShaderModule ps )
{
	Gen g;
	VkAttachmentDescription at[2] = {};
	at[0].format = fmt;
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
	sp.pDepthStencilAttachment = ds ? &dr : nullptr;
	VkRenderPassCreateInfo rpi { VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO };
	rpi.attachmentCount = ds ? 2 : 1;
	rpi.pAttachments = at;
	rpi.subpassCount = 1;
	rpi.pSubpasses = &sp;
	vkCreateRenderPass( dev, &rpi, nullptr, &g.rp );
	VkImageView fv[2] = { color.v, ds ? ds->v : VK_NULL_HANDLE };
	VkFramebufferCreateInfo fbi { VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
	fbi.renderPass = g.rp;
	fbi.attachmentCount = ds ? 2 : 1;
	fbi.pAttachments = fv;
	fbi.width = uint32_t( W );
	fbi.height = uint32_t( H );
	fbi.layers = 1;
	vkCreateFramebuffer( dev, &fbi, nullptr, &g.fb );
	VkPipelineShaderStageCreateInfo st[2] = {
		{ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_VERTEX_BIT, vs, "vs", nullptr },
		{ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_FRAGMENT_BIT, ps, "ps", nullptr } };
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
	VkPipelineDepthStencilStateCreateInfo dss { VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };
	dss.stencilTestEnable = ds ? VK_TRUE : VK_FALSE;
	dss.front = { VK_STENCIL_OP_KEEP, VK_STENCIL_OP_REPLACE, VK_STENCIL_OP_KEEP, VK_COMPARE_OP_ALWAYS, 0xFF, 0xFF, 0 };
	dss.back = dss.front;
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
	gpc.pDepthStencilState = &dss;
	gpc.pColorBlendState = &cb;
	gpc.pDynamicState = &dy;
	gpc.layout = layout;
	gpc.renderPass = g.rp;
	vkCreateGraphicsPipelines( dev, VK_NULL_HANDLE, 1, &gpc, nullptr, &g.pipe );
	return g;
}

int main( int argc, char ** argv )
{
	std::setvbuf( stdout, nullptr, _IONBF, 0 );
	if ( argc < 5 ) {
		std::printf( "usage: tonemapmatch <job dir> <tonemap.spv> <gen_vs.spv> <gen_ps.spv>\n" );
		return 2;
	}
	const std::string dir = argv[1];
	std::string name = dir;
	while ( !name.empty() && ( name.back() == '/' || name.back() == '\\' ) )
		name.pop_back();
	name = name.substr( name.find_last_of( "/\\" ) + 1 );
	// params.txt (tonemap_gl_ref.py): W H S mode test ref cellon van lut measure fo fog backdrop dither seed red pos x 8,
	// then E expo adapted cine x 3 tint x 4 wb x 3 foW ec cap
	int iv[24] = {};
	float fv[16] = {};
	{
		FILE * f = std::fopen( ( dir + "/params.txt" ).c_str(), "r" );
		bool ok = f != nullptr;
		for ( int i = 0; ok && i < 24; i++ )
			ok = std::fscanf( f, "%d", &iv[i] ) == 1;
		for ( int i = 0; ok && i < 16; i++ )
			ok = std::fscanf( f, "%f", &fv[i] ) == 1;
		if ( f )
			std::fclose( f );
		if ( !ok ) {
			std::printf( "TONEMATCH %s FAIL (params)\n", name.c_str() );
			return 2;
		}
	}
	const int W = iv[0], H = iv[1], S = iv[2];
	Raw data, sten, fog, comb, mb, lut, back, glOut;
	if ( S != kSamples || !readRaw( dir + "/data.bin", data, 8 ) || !readRaw( dir + "/sten.bin", sten, 1 ) || !readRaw( dir + "/fog.bin", fog, 8 )
		|| !readRaw( dir + "/comb.bin", comb, 16 ) || !readRaw( dir + "/mb.bin", mb, 8 ) || !readRaw( dir + "/lut.bin", lut, 4 )
		|| !readRaw( dir + "/backdrop.bin", back, 8 ) || !readRaw( dir + "/gl_out.bin", glOut, 16 ) || data.w != W * S || data.h != H ) {
		std::printf( "TONEMATCH %s FAIL (job files)\n", name.c_str() );
		return 2;
	}
	const std::vector<uint32_t> spvTone = readSpv( argv[2] ), spvVs = readSpv( argv[3] ), spvPs = readSpv( argv[4] );
	if ( spvTone.empty() || spvVs.empty() || spvPs.empty() ) {
		std::printf( "TONEMATCH %s FAIL (spv)\n", name.c_str() );
		return 2;
	}
	HMODULE lib = LoadLibraryA( "vulkan-1.dll" );
	if ( !lib ) {
		std::printf( "TONEMATCH %s FAIL (no vulkan-1.dll)\n", name.c_str() );
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
		std::printf( "TONEMATCH %s FAIL (instance)\n", name.c_str() );
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
		std::printf( "TONEMATCH %s FAIL (no device)\n", name.c_str() );
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
	family = UINT32_MAX;
	for ( uint32_t i = 0; i < nq && family == UINT32_MAX; i++ )
		if ( ( qf[i].queueFlags & VK_QUEUE_GRAPHICS_BIT ) && ( qf[i].queueFlags & VK_QUEUE_COMPUTE_BIT ) && qf[i].timestampValidBits )
			family = i;
	if ( family == UINT32_MAX ) {
		std::printf( "TONEMATCH %s FAIL (no graphics family)\n", name.c_str() );
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
		std::printf( "TONEMATCH %s FAIL (device)\n", name.c_str() );
		return 2;
	}
	auto gdpa = reinterpret_cast<PFN_vkGetDeviceProcAddr>( gipa( inst, "vkGetDeviceProcAddr" ) );
#define LOADD( fn ) fn = reinterpret_cast<PFN_##fn>( gdpa( dev, #fn ) )
	LOADD( vkCreateBuffer ); LOADD( vkGetBufferMemoryRequirements ); LOADD( vkAllocateMemory ); LOADD( vkBindBufferMemory );
	LOADD( vkMapMemory ); LOADD( vkCreateImage ); LOADD( vkGetImageMemoryRequirements ); LOADD( vkBindImageMemory );
	LOADD( vkCreateImageView ); LOADD( vkCreateSampler ); LOADD( vkCreateDescriptorSetLayout ); LOADD( vkCreatePipelineLayout );
	LOADD( vkCreateShaderModule ); LOADD( vkCreateComputePipelines ); LOADD( vkCreateGraphicsPipelines ); LOADD( vkCreateRenderPass );
	LOADD( vkCreateFramebuffer ); LOADD( vkCmdBeginRenderPass ); LOADD( vkCmdEndRenderPass ); LOADD( vkCmdDraw );
	LOADD( vkCmdSetStencilReference ); LOADD( vkCmdClearColorImage );
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

	const VkImageUsageFlags up = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	const VkImageAspectFlags DS = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT, C = VK_IMAGE_ASPECT_COLOR_BIT;
	const VkFormat F16 = VK_FORMAT_R16G16B16A16_SFLOAT, F32 = VK_FORMAT_R32G32B32A32_SFLOAT;
	Img iData = makeImg( W * S, H, F16, 1, up, C ), iSten = makeImg( W * S, H, VK_FORMAT_R8_UINT, 1, up, C );
	Img iFogData = makeImg( W * S, H, F16, 1, up, C ), iZero = makeImg( W * S, H, VK_FORMAT_R8_UINT, 1, up, C );
	Img iColor = makeImg( W, H, F16, S, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, C );
	Img iDs = makeImg( W, H, VK_FORMAT_D24_UNORM_S8_UINT, S, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, DS );
	Img iFog = makeImg( W, H, VK_FORMAT_R8_UNORM, S, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, C );
	Img iComb = makeImg( W, H, F32, 1, up, C ), iMb = makeImg( mb.w, mb.h, F16, 1, up, C ), iBack = makeImg( W, H, F16, 1, up, C );
	Img iLut = makeImg3( 16, VK_FORMAT_R8G8B8A8_UNORM, up );
	Img iOut = makeImg( W * S, H, F32, 1, VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, C );
	if ( !iData.v || !iSten.v || !iFogData.v || !iZero.v || !iColor.v || !iDs.sv || !iFog.v || !iComb.v || !iMb.v || !iBack.v || !iLut.v
		|| !iOut.v || lut.w != 16 || lut.h != 256 ) {
		std::printf( "TONEMATCH %s FAIL (images)\n", name.c_str() );
		return 2;
	}
	upload( iData, data.px.data(), data.px.size(), C );
	upload( iSten, sten.px.data(), sten.px.size(), C );
	upload( iFogData, fog.px.data(), fog.px.size(), C );
	{
		std::vector<unsigned char> z( size_t( W ) * S * H, 0 );
		upload( iZero, z.data(), z.size(), C );
	}
	upload( iComb, comb.px.data(), comb.px.size(), C );
	upload( iMb, mb.px.data(), mb.px.size(), C );
	upload( iBack, back.px.data(), back.px.size(), C );
	upload3( iLut, 16, lut.px.data(), lut.px.size() );
	{
		VkCommandBuffer cb = newCb();
		toGeneral( cb, iOut.i, C );
		submitWait( cb );
	}

	// the generator: the frame (colour + stencil, one draw per value) and the fog's gain (R8, every sample)
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
	const VkShaderModule mVs = module( spvVs ), mPs = module( spvPs );
	const Gen gFrame = makeGen( F16, iColor, &iDs, W, H, S, gPipeLayout, mVs, mPs );
	const Gen gFog = makeGen( VK_FORMAT_R8_UNORM, iFog, nullptr, W, H, S, gPipeLayout, mVs, mPs );
	if ( !gFrame.pipe || !gFog.pipe ) {
		std::printf( "TONEMATCH %s FAIL (generator pipelines)\n", name.c_str() );
		return 2;
	}

	// the kernel's layout: ToneBindingsVk.hlsli 0..8
	VkDescriptorSetLayoutBinding kb[9];
	const VkDescriptorType SI = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
	const VkDescriptorType kt[9] = { SI, SI, SI, SI, SI, SI, SI, VK_DESCRIPTOR_TYPE_SAMPLER, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE };
	for ( uint32_t i = 0; i < 9; i++ )
		kb[i] = VkDescriptorSetLayoutBinding { i, kt[i], 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr };
	VkDescriptorSetLayoutCreateInfo kli { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
	kli.bindingCount = 9;
	kli.pBindings = kb;
	VkDescriptorSetLayout kLayout;
	vkCreateDescriptorSetLayout( dev, &kli, nullptr, &kLayout );
	VkPushConstantRange kpr { VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof( ToneJob ) };
	VkPipelineLayoutCreateInfo kpi { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
	kpi.setLayoutCount = 1;
	kpi.pSetLayouts = &kLayout;
	kpi.pushConstantRangeCount = 1;
	kpi.pPushConstantRanges = &kpr;
	VkPipelineLayout kPipeLayout;
	vkCreatePipelineLayout( dev, &kpi, nullptr, &kPipeLayout );
	VkPipeline pTone = VK_NULL_HANDLE;
	{
		VkComputePipelineCreateInfo pc { VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO };
		pc.stage = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_COMPUTE_BIT, module( spvTone ), "main", nullptr };
		pc.layout = kPipeLayout;
		vkCreateComputePipelines( dev, VK_NULL_HANDLE, 1, &pc, nullptr, &pTone );
	}
	if ( !pTone ) {
		std::printf( "TONEMATCH %s FAIL (kernel pipeline)\n", name.c_str() );
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
	VkDescriptorPoolSize ps[3] = { { SI, 32 }, { VK_DESCRIPTOR_TYPE_SAMPLER, 8 }, { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 8 } };
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
	const VkDescriptorSet gFrameSet = alloc( gLayout ), gFogSet = alloc( gLayout ), kSet = alloc( kLayout );
	write( gFrameSet, 0, SI, iData.v, VK_NULL_HANDLE );
	write( gFrameSet, 1, SI, iSten.v, VK_NULL_HANDLE );
	write( gFogSet, 0, SI, iFogData.v, VK_NULL_HANDLE );
	write( gFogSet, 1, SI, iZero.v, VK_NULL_HANDLE );
	write( kSet, 0, SI, iColor.v, VK_NULL_HANDLE );
	write( kSet, 1, SI, iDs.sv, VK_NULL_HANDLE );
	write( kSet, 2, SI, iComb.v, VK_NULL_HANDLE );
	write( kSet, 3, SI, iMb.v, VK_NULL_HANDLE );
	write( kSet, 4, SI, iLut.v, VK_NULL_HANDLE );
	write( kSet, 5, SI, iFog.v, VK_NULL_HANDLE );
	write( kSet, 6, SI, iBack.v, VK_NULL_HANDLE );
	write( kSet, 7, VK_DESCRIPTOR_TYPE_SAMPLER, VK_NULL_HANDLE, linearClamp );
	write( kSet, 8, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, iOut.v, VK_NULL_HANDLE );

	auto draw = [&]( const Gen & g, VkDescriptorSet set, std::initializer_list<int> values, bool stencil ) {
		VkCommandBuffer c = newCb();
		VkClearValue cv[2] = {};
		VkRenderPassBeginInfo rb { VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
		rb.renderPass = g.rp;
		rb.framebuffer = g.fb;
		rb.renderArea = { { 0, 0 }, { uint32_t( W ), uint32_t( H ) } };
		rb.clearValueCount = stencil ? 2 : 1;
		rb.pClearValues = cv;
		vkCmdBeginRenderPass( c, &rb, VK_SUBPASS_CONTENTS_INLINE );
		vkCmdBindPipeline( c, VK_PIPELINE_BIND_POINT_GRAPHICS, g.pipe );
		vkCmdBindDescriptorSets( c, VK_PIPELINE_BIND_POINT_GRAPHICS, gPipeLayout, 0, 1, &set, 0, nullptr );
		for ( int v : values ) {
			const int32_t pc[2] = { S, v };
			vkCmdPushConstants( c, gPipeLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, 8, pc );
			vkCmdSetStencilReference( c, VK_STENCIL_FACE_FRONT_AND_BACK, uint32_t( v ) );
			vkCmdDraw( c, 3, 1, 0, 0 );
		}
		vkCmdEndRenderPass( c );
		submitWait( c );
	};
	draw( gFrame, gFrameSet, { 0, 1, 2, 3, 5, 7 }, true );
	draw( gFog, gFogSet, { 0 }, false );

	// the job: cell_hdr.frag's uniforms as ToneBindingsVk.hlsli packs them
	ToneJob j {};
	const int mode = iv[3], test = iv[4], ref = iv[5], cellon = iv[6], van = iv[7], lutOn = iv[8], measure = iv[9], fo = iv[10], fogOn = iv[11],
		backOn = iv[12], dither = iv[13], seed = iv[14], red = iv[15];
	j.cineE[0] = fv[3]; j.cineE[1] = fv[4]; j.cineE[2] = fv[5]; j.cineE[3] = fv[0];
	for ( int i = 0; i < 4; i++ )
		j.tint[i] = fv[6 + i];
	j.wbExposure[0] = fv[10]; j.wbExposure[1] = fv[11]; j.wbExposure[2] = fv[12]; j.wbExposure[3] = fv[1];
	j.bloomRect[0] = 0.0f; j.bloomRect[1] = 0.0f; j.bloomRect[2] = 1.0f / float( W ); j.bloomRect[3] = 1.0f / float( H );
	j.fo[0] = fv[13]; j.fo[1] = fv[14]; j.fo[2] = fv[15]; j.fo[3] = fv[2];
	j.ints[0] = ( red & 15 ) | ( van ? 1 << 4 : 0 ) | ( lutOn ? 1 << 5 : 0 ) | ( measure ? 1 << 6 : 0 ) | ( fo ? 1 << 7 : 0 )
		| ( fogOn ? 1 << 9 : 0 ) | ( cellon ? 1 << 10 : 0 ) | ( backOn ? 1 << 11 : 0 ) | ( mode << 16 ) | ( test << 20 ) | ( ref << 24 );
	j.ints[1] = dither;
	j.ints[2] = seed;
	j.ints[3] = 0;

	VkQueryPool qp;
	{
		VkQueryPoolCreateInfo qpi { VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO };
		qpi.queryType = VK_QUERY_TYPE_TIMESTAMP;
		qpi.queryCount = 2;
		vkCreateQueryPool( dev, &qpi, nullptr, &qp );
	}
	auto run = [&]( bool clearFirst, bool timeIt ) {
		VkCommandBuffer c = newCb();
		if ( clearFirst ) {	// GL's target starts at the sentinel: a sample the stencil test skips keeps it
			VkClearColorValue cv;
			cv.float32[0] = cv.float32[1] = cv.float32[2] = cv.float32[3] = -7.0f;
			VkImageSubresourceRange rr { C, 0, 1, 0, 1 };
			barrier( c, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_MEMORY_WRITE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT );
			vkCmdClearColorImage( c, iOut.i, VK_IMAGE_LAYOUT_GENERAL, &cv, 1, &rr );
		}
		if ( timeIt ) {
			vkCmdResetQueryPool( c, qp, 0, 2 );
			vkCmdWriteTimestamp( c, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, qp, 0 );
		}
		barrier( c, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_MEMORY_WRITE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
			VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT );
		vkCmdBindPipeline( c, VK_PIPELINE_BIND_POINT_COMPUTE, pTone );
		vkCmdBindDescriptorSets( c, VK_PIPELINE_BIND_POINT_COMPUTE, kPipeLayout, 0, 1, &kSet, 0, nullptr );
		vkCmdPushConstants( c, kPipeLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof( ToneJob ), &j );
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
	run( true, false );
	std::vector<uint32_t> vk( size_t( W ) * S * H * 4 );
	download( iOut, vk.data(), vk.size() * 4 );
	// 0.5 s of dispatches first: a GPU that clocked down during the download reads up to 2x slow (the P5 lesson)
	for ( const DWORD t0 = GetTickCount(); GetTickCount() - t0 < 500; )
		run( false, false );
	std::vector<double> t;
	for ( int r = 0; r < 34; r++ ) {
		const double ms = run( false, true );
		if ( r >= 2 )
			t.push_back( ms );
	}
	std::sort( t.begin(), t.end() );
	const double msTone = t[t.size() / 2];

	// compare: every float's bits; then as the 8-bit view would hold it (the value clamped, x 255, rounded)
	const uint32_t * gp = reinterpret_cast<const uint32_t *>( glOut.px.data() );
	size_t diff = 0, diff8 = 0, kept = 0, sentinelMiss = 0;
	long long maxU = 0;
	int max8 = 0;
	auto lvl = []( uint32_t b ) {
		float f;
		std::memcpy( &f, &b, 4 );
		if ( std::isnan( f ) )
			return -1;
		return int( std::lround( std::min( std::max( f, 0.0f ), 1.0f ) * 255.0f ) );
	};
	const uint32_t sentinel = 0xC0E00000u;	// -7.0f
	for ( size_t i = 0; i < vk.size(); i++ ) {
		kept += gp[i] != sentinel;
		sentinelMiss += ( gp[i] == sentinel ) != ( vk[i] == sentinel );
		const long long u = ulps32( vk[i], gp[i] );
		if ( !u )
			continue;
		if ( diff < 6 ) {
			float a, b;
			std::memcpy( &a, &vk[i], 4 );
			std::memcpy( &b, &gp[i], 4 );
			const size_t px = i / 4;
			std::printf( "diff x %zu s %zu y %zu c %zu: vk %.9g gl %.9g (%lld ulp)\n", ( px % ( size_t( W ) * S ) ) / S, px % S,
				px / ( size_t( W ) * S ), i % 4, a, b, u );
		}
		diff++;
		maxU = std::max( maxU, u );
		const int d8 = std::abs( lvl( vk[i] ) - lvl( gp[i] ) );
		diff8 += d8 != 0;
		max8 = std::max( max8, d8 );
	}
	std::printf( "device %s\n", props.deviceName );
	std::printf( "time %s: Vulkan tone map %.4f ms\n", name.c_str(), msTone );
	std::printf( "tone %s: %zu of %zu floats differ, max %lld ulp; 8-bit levels: %zu differ, max %d; %zu GL floats written, "
		"%zu skip mismatches\n", name.c_str(), diff, vk.size(), maxU, diff8, max8, kept, sentinelMiss );
	// EXACT: every float. BOUND (named cause: the drivers' pow / exp2 / log2 / divide differ by an ulp between the GLSL and
	// SPIR-V paths, and the Physical curve's PQ exponent 78.84 grows that): no skip mismatch, none past 1 level in the
	// 8-bit view, at most 1 in 1,000 floats a level off there
	const bool bound = sentinelMiss == 0 && max8 <= 1 && diff8 * 1000 <= vk.size();
	if ( diff == 0 )
		std::printf( "TONEMATCH %s EXACT\n", name.c_str() );
	else
		std::printf( "TONEMATCH %s %s (%zu floats, max %lld ulp, %zu 8-bit levels off by at most %d)\n", name.c_str(), bound ? "BOUND" : "FAIL",
			diff, maxU, diff8, max8 );
	return diff == 0 || bound ? 0 : 1;
}
