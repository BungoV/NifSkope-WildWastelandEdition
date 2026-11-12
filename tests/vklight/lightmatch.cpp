// Lane VKLIGHT1 (2026-11-12): one chain of the lighting kernels (res/shaders/hlsl/vklight/*.hlsl over
// LightBindingsVk.hlsli) on Vulkan against GL's .frag chain on the same inputs (tests/vklight/light_gl_ref.py writes the
// chain file, the push blocks and GL's results). Derived from tests/vkframe/postfxmatch.cpp (VKFRAME1 P7) by
// notes/vklight1/tools/make_lightmatch.py: same chain.txt and set layout, plus float32 compares and rgba32f images.
// Usage: lightmatch <chain.txt> <spv dir> [time]
// chain.txt:  img <name> <rgba16f|rg16f|r16f|r32f|rg32f|rgba32f> <w> <h> <file relative to the job's parent | ->
//             pass <label> <spv> <pc file> <out0> <out1|-> <t0> <t1> <t2> <t3> <t4>   (- = unbound)
//             cmp <image> <gl file> <ulpN|step|gather|absX>   (ulpN: max N ulp of the image's own type, half or float;
//             absX: float32, every value within X absolute, <= 2 % of values differ, no NaN mismatch)
// Last line `LIGHTMATCH <job> EXACT|BOUND (...)|FAIL (...)`. Exit 0 for EXACT or BOUND. LIGHTMATCH_DUMP=1 writes vk_<image>.bin.
#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include <windows.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#define VKF( name ) static PFN_##name name
VKF( vkCreateInstance ); VKF( vkEnumeratePhysicalDevices ); VKF( vkGetPhysicalDeviceQueueFamilyProperties );
VKF( vkGetPhysicalDeviceProperties ); VKF( vkGetPhysicalDeviceMemoryProperties ); VKF( vkCreateDevice ); VKF( vkGetDeviceQueue );
VKF( vkGetPhysicalDeviceFeatures );
VKF( vkCreateBuffer ); VKF( vkGetBufferMemoryRequirements ); VKF( vkAllocateMemory ); VKF( vkBindBufferMemory ); VKF( vkMapMemory );
VKF( vkCreateImage ); VKF( vkGetImageMemoryRequirements ); VKF( vkBindImageMemory ); VKF( vkCreateImageView ); VKF( vkCreateSampler );
VKF( vkCreateDescriptorSetLayout ); VKF( vkCreatePipelineLayout ); VKF( vkCreateShaderModule ); VKF( vkCreateComputePipelines );
VKF( vkCreateDescriptorPool ); VKF( vkAllocateDescriptorSets ); VKF( vkUpdateDescriptorSets ); VKF( vkCreateCommandPool );
VKF( vkAllocateCommandBuffers ); VKF( vkBeginCommandBuffer ); VKF( vkEndCommandBuffer ); VKF( vkCmdBindPipeline );
VKF( vkCmdBindDescriptorSets ); VKF( vkCmdPushConstants ); VKF( vkCmdDispatch ); VKF( vkQueueSubmit ); VKF( vkQueueWaitIdle );
VKF( vkCmdPipelineBarrier ); VKF( vkCmdCopyBufferToImage ); VKF( vkCmdCopyImageToBuffer ); VKF( vkCreateQueryPool );
VKF( vkCmdResetQueryPool ); VKF( vkCmdWriteTimestamp ); VKF( vkGetQueryPoolResults );
VKF( vkCreateGraphicsPipelines ); VKF( vkCreateRenderPass ); VKF( vkCreateFramebuffer ); VKF( vkCmdBeginRenderPass );
VKF( vkCmdEndRenderPass ); VKF( vkCmdDraw );

static VkDevice dev;
static VkQueue queue;
static uint32_t family;
static VkPhysicalDeviceMemoryProperties memProps;

static std::vector<uint8_t> readFile( const std::string & path )
{
	std::vector<uint8_t> v;
	FILE * f = std::fopen( path.c_str(), "rb" );
	if ( !f )
		return v;
	std::fseek( f, 0, SEEK_END );
	const long n = std::ftell( f );
	std::fseek( f, 0, SEEK_SET );
	v.resize( static_cast<size_t>( n ) );
	if ( std::fread( v.data(), 1, v.size(), f ) != v.size() )
		v.clear();
	std::fclose( f );
	return v;
}

static uint32_t memType( uint32_t bits, VkMemoryPropertyFlags want )
{
	for ( uint32_t t = 0; t < memProps.memoryTypeCount; t++ )
		if ( ( bits & ( 1u << t ) ) && ( memProps.memoryTypes[t].propertyFlags & want ) == want )
			return t;
	return 0;
}

struct Buf { VkBuffer b = VK_NULL_HANDLE; VkDeviceMemory m = VK_NULL_HANDLE; void * p = nullptr; };

static Buf makeBuf( size_t n )
{
	Buf B;
	VkBufferCreateInfo bi { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
	bi.size = n;
	bi.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
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

struct Img { VkImage i = VK_NULL_HANDLE; VkDeviceMemory m = VK_NULL_HANDLE; VkImageView v = VK_NULL_HANDLE; int w = 0, h = 0, tb = 8, lv = 1; bool half = true; VkFormat f = VK_FORMAT_UNDEFINED; std::string file; };

struct Fmt { const char * name; VkFormat f; int tb; bool half; };
static const Fmt kFmts[] = { { "rgba16f", VK_FORMAT_R16G16B16A16_SFLOAT, 8, true }, { "rg16f", VK_FORMAT_R16G16_SFLOAT, 4, true },
	{ "r16f", VK_FORMAT_R16_SFLOAT, 2, true }, { "r32f", VK_FORMAT_R32_SFLOAT, 4, false }, { "rg32f", VK_FORMAT_R32G32_SFLOAT, 8, false },
	{ "rgba32f", VK_FORMAT_R32G32B32A32_SFLOAT, 16, false }, { "r8", VK_FORMAT_R8_UNORM, 1, false } };
static const Fmt kRgba8 = { "rgba8", VK_FORMAT_R8G8B8A8_UNORM, 4, false };
static int rasterMode = 0;	// LIGHTMATCH_RASTER

static Img makeImg( int w, int h, const Fmt & fm, int lv = 1 )
{
	Img I;
	I.lv = lv;
	I.w = w;
	I.h = h;
	I.tb = fm.tb;
	I.half = fm.half;
	I.f = fm.f;
	VkImageCreateInfo ci { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
	ci.imageType = VK_IMAGE_TYPE_2D;
	ci.format = fm.f;
	ci.extent = { uint32_t( w ), uint32_t( h ), 1 };
	ci.mipLevels = uint32_t( lv );	// an input with mips (img line's 7th field): sampled only, all levels in the view
	ci.arrayLayers = 1;
	ci.samples = VK_SAMPLE_COUNT_1_BIT;
	ci.tiling = VK_IMAGE_TILING_OPTIMAL;
	ci.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_STORAGE_BIT
		| ( rasterMode ? VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT : 0 );
	ci.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	vkCreateImage( dev, &ci, nullptr, &I.i );
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
	vi.format = fm.f;
	vi.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, uint32_t( lv ), 0, 1 };
	vkCreateImageView( dev, &vi, nullptr, &I.v );
	return I;
}

static VkDescriptorSetLayout setLayout;
static VkPipelineLayout pipeLayout;
static VkDescriptorPool pool;
static VkCommandPool cpool;
static VkSampler samplers[2];	// linear, point
static const uint32_t kBindings = 10;
static const VkShaderStageFlags kStages = VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

// LightBindingsVk.hlsli: 0-4 sampled, 5 linear, 6 point, 7-8 storage images, 9 one storage buffer (the chain's `buf`)
static VkDescriptorType bindingType( uint32_t i )
{
	return i < 5 ? VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE : ( i < 7 ? VK_DESCRIPTOR_TYPE_SAMPLER
		: i < 9 ? VK_DESCRIPTOR_TYPE_STORAGE_IMAGE : VK_DESCRIPTOR_TYPE_STORAGE_BUFFER );
}

static bool makeLayout( uint32_t sets )
{
	VkDescriptorSetLayoutBinding b[kBindings];
	for ( uint32_t i = 0; i < kBindings; i++ )
		b[i] = VkDescriptorSetLayoutBinding { i, bindingType( i ), 1, kStages, nullptr };
	VkDescriptorSetLayoutCreateInfo li { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
	li.bindingCount = kBindings;
	li.pBindings = b;
	if ( vkCreateDescriptorSetLayout( dev, &li, nullptr, &setLayout ) != VK_SUCCESS )
		return false;
	VkPushConstantRange pr { kStages, 0, 128 };
	VkPipelineLayoutCreateInfo pi { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
	pi.setLayoutCount = 1;
	pi.pSetLayouts = &setLayout;
	pi.pushConstantRangeCount = 1;
	pi.pPushConstantRanges = &pr;
	if ( vkCreatePipelineLayout( dev, &pi, nullptr, &pipeLayout ) != VK_SUCCESS )
		return false;
	VkDescriptorPoolSize ps[4] = { { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 5 * sets }, { VK_DESCRIPTOR_TYPE_SAMPLER, 2 * sets },
		{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 2 * sets }, { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, sets } };
	VkDescriptorPoolCreateInfo dp { VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
	dp.maxSets = sets;
	dp.poolSizeCount = 4;
	dp.pPoolSizes = ps;
	if ( vkCreateDescriptorPool( dev, &dp, nullptr, &pool ) != VK_SUCCESS )
		return false;
	for ( int s = 0; s < 2; s++ ) {
		VkSamplerCreateInfo si { VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
		si.magFilter = si.minFilter = s == 0 ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;
		si.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
		si.addressModeU = si.addressModeV = si.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		si.maxLod = VK_LOD_CLAMP_NONE;	// GL's mip rule on mip inputs; single-level images clamp to 0 anyway
		if ( vkCreateSampler( dev, &si, nullptr, &samplers[s] ) != VK_SUCCESS )
			return false;
	}
	VkCommandPoolCreateInfo ci { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
	ci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	ci.queueFamilyIndex = family;
	return vkCreateCommandPool( dev, &ci, nullptr, &cpool ) == VK_SUCCESS;
}

static VkPipeline makePipe( const std::vector<uint8_t> & spv )
{
	VkShaderModuleCreateInfo mi { VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
	mi.codeSize = spv.size();
	mi.pCode = reinterpret_cast<const uint32_t *>( spv.data() );
	VkShaderModule m;
	if ( vkCreateShaderModule( dev, &mi, nullptr, &m ) != VK_SUCCESS )
		return VK_NULL_HANDLE;
	VkComputePipelineCreateInfo pc { VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO };
	pc.stage = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_COMPUTE_BIT, m, "main", nullptr };
	pc.layout = pipeLayout;
	VkPipeline p = VK_NULL_HANDLE;
	vkCreateComputePipelines( dev, VK_NULL_HANDLE, 1, &pc, nullptr, &p );
	return p;
}

// a pass as a fragment pass: a full-screen triangle into out0 (and out1), LOAD + STORE, GENERAL throughout
struct Raster { VkRenderPass rp = VK_NULL_HANDLE; VkFramebuffer fb = VK_NULL_HANDLE; VkPipeline pipe = VK_NULL_HANDLE; };

static Raster makeRaster( const std::vector<const Img *> & outs, VkShaderModule vs, VkShaderModule ps )
{
	Raster g;
	const uint32_t n = uint32_t( outs.size() );
	VkAttachmentDescription at[2] {};
	VkAttachmentReference cr[2];
	VkImageView views[2];
	for ( uint32_t k = 0; k < n; k++ ) {
		at[k].format = outs[k]->f;
		at[k].samples = VK_SAMPLE_COUNT_1_BIT;
		at[k].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
		at[k].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		at[k].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		at[k].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		at[k].initialLayout = at[k].finalLayout = VK_IMAGE_LAYOUT_GENERAL;
		cr[k] = { k, VK_IMAGE_LAYOUT_GENERAL };
		views[k] = outs[k]->v;
	}
	VkSubpassDescription sp {};
	sp.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	sp.colorAttachmentCount = n;
	sp.pColorAttachments = cr;
	VkRenderPassCreateInfo rpi { VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO };
	rpi.attachmentCount = n;
	rpi.pAttachments = at;
	rpi.subpassCount = 1;
	rpi.pSubpasses = &sp;
	if ( vkCreateRenderPass( dev, &rpi, nullptr, &g.rp ) != VK_SUCCESS )
		return Raster();
	const int W = outs[0]->w, H = outs[0]->h;
	VkFramebufferCreateInfo fbi { VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
	fbi.renderPass = g.rp;
	fbi.attachmentCount = n;
	fbi.pAttachments = views;
	fbi.width = uint32_t( W );
	fbi.height = uint32_t( H );
	fbi.layers = 1;
	if ( vkCreateFramebuffer( dev, &fbi, nullptr, &g.fb ) != VK_SUCCESS )
		return Raster();
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
	ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	VkPipelineColorBlendAttachmentState cba[2] {};
	cba[0].colorWriteMask = cba[1].colorWriteMask = 0xF;
	VkPipelineColorBlendStateCreateInfo cb { VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
	cb.attachmentCount = n;
	cb.pAttachments = cba;
	VkGraphicsPipelineCreateInfo gpc { VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
	gpc.stageCount = 2;
	gpc.pStages = st;
	gpc.pVertexInputState = &vin;
	gpc.pInputAssemblyState = &ia;
	gpc.pViewportState = &vps;
	gpc.pRasterizationState = &rs;
	gpc.pMultisampleState = &ms;
	gpc.pColorBlendState = &cb;
	gpc.layout = pipeLayout;
	gpc.renderPass = g.rp;
	vkCreateGraphicsPipelines( dev, VK_NULL_HANDLE, 1, &gpc, nullptr, &g.pipe );
	return g;
}

static VkShaderModule loadModule( const std::string & path )
{
	const std::vector<uint8_t> spv = readFile( path );
	if ( spv.empty() )
		return VK_NULL_HANDLE;
	VkShaderModuleCreateInfo mi { VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
	mi.codeSize = spv.size();
	mi.pCode = reinterpret_cast<const uint32_t *>( spv.data() );
	VkShaderModule m = VK_NULL_HANDLE;
	vkCreateShaderModule( dev, &mi, nullptr, &m );
	return m;
}

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
	VkSubmitInfo si { VK_STRUCTURE_TYPE_SUBMIT_INFO };
	si.commandBufferCount = 1;
	si.pCommandBuffers = &cb;
	vkQueueSubmit( queue, 1, &si, VK_NULL_HANDLE );
	vkQueueWaitIdle( queue );
}

// the distance of two values in representable steps; two NaNs are equal, NaN against a number = big
static int ulps( uint16_t a, uint16_t b )
{
	auto ord = []( uint16_t x ) { return ( x & 0x8000 ) ? -int( x & 0x7FFF ) : int( x ); };
	auto nan = []( uint16_t x ) { return ( x & 0x7C00 ) == 0x7C00 && ( x & 0x03FF ); };
	if ( nan( a ) || nan( b ) )
		return nan( a ) == nan( b ) ? 0 : 1 << 20;
	return std::abs( ord( a ) - ord( b ) );
}

// the distance of two floats in representable steps; two NaNs are equal, NaN against a number = big
static int ulps32( uint32_t a, uint32_t b )
{
	auto ord = []( uint32_t x ) { return ( x & 0x80000000u ) ? -int64_t( x & 0x7FFFFFFFu ) : int64_t( x ); };
	auto nan = []( uint32_t x ) { return ( x & 0x7F800000u ) == 0x7F800000u && ( x & 0x007FFFFFu ); };
	if ( nan( a ) || nan( b ) )
		return nan( a ) == nan( b ) ? 0 : 1 << 30;
	const int64_t d = ord( a ) - ord( b );
	return int( std::min<int64_t>( d < 0 ? -d : d, 1 << 30 ) );
}

struct Pass { std::string label, spv, pc; std::string out[2], t[5]; int gx = 0, gy = 0; };	// gx > 0: explicit groups
struct Cmp { std::string img, file, rule; };

int main( int argc, char ** argv )
{
	std::setvbuf( stdout, nullptr, _IONBF, 0 );
	if ( argc < 3 ) {
		std::printf( "usage: postfxmatch <chain.txt> <spv dir> [time]\n" );
		return 2;
	}
	const std::string chainPath = argv[1], spvDir = std::string( argv[2] ) + "/";
	const bool timeIt = argc > 3 && std::string( argv[3] ) == "time";
	if ( const char * r = std::getenv( "LIGHTMATCH_RASTER" ) )
		rasterMode = std::atoi( r );
	const std::string dir = chainPath.substr( 0, chainPath.find_last_of( "/\\" ) + 1 );
	std::string name = dir.substr( 0, dir.size() - 1 );
	const std::string parent = name.substr( 0, name.find_last_of( "/\\" ) + 1 );
	name = name.substr( name.find_last_of( "/\\" ) + 1 ) + "/" + chainPath.substr( chainPath.find_last_of( "/\\" ) + 1 );
	if ( rasterMode )
		name += rasterMode == 8 ? " raster8" : " raster";
	auto fail = [&]( const char * why ) {
		std::printf( "LIGHTMATCH %s FAIL (%s)\n", name.c_str(), why );
		return 2;
	};

	std::vector<std::string> imgOrder;
	std::map<std::string, std::pair<const Fmt *, std::pair<int, int>>> imgDecl;
	std::map<std::string, std::string> imgFile;
	std::map<std::string, int> imgLv;
	std::vector<Pass> passes;
	std::vector<Cmp> cmps;
	std::string bufFile;
	{
		const std::vector<uint8_t> txt = readFile( chainPath );
		if ( txt.empty() )
			return fail( "chain file" );
		std::istringstream in( std::string( txt.begin(), txt.end() ) );
		std::string line;
		while ( std::getline( in, line ) ) {
			std::istringstream l( line );
			std::string k;
			l >> k;
			if ( k == "img" ) {
				std::string n, f, file;
				int w = 0, h = 0;
				l >> n >> f >> w >> h >> file;
				const Fmt * fm = nullptr;
				for ( const Fmt & x : kFmts )
					if ( f == x.name )
						fm = &x;
				if ( !fm || w <= 0 || h <= 0 )
					return fail( "img line" );
				imgOrder.push_back( n );
				imgDecl[n] = { fm, { w, h } };
				imgFile[n] = file;
				int lv = 1;
				if ( !( l >> lv ) || lv < 1 )
					lv = 1;
				imgLv[n] = lv;	// the file holds every level, each with its own W, H (floor halving)
			} else if ( k == "pass" ) {
				Pass p;
				l >> p.label >> p.spv >> p.pc >> p.out[0] >> p.out[1];
				for ( auto & t : p.t )
					l >> t;
				l >> p.gx >> p.gy;	// optional: the group counts (default 8x8 groups over out0)
				passes.push_back( p );
			} else if ( k == "buf" ) {
				l >> bufFile;	// the storage buffer at binding 9, relative to the job folder
			} else if ( k == "cmp" ) {
				Cmp c;
				l >> c.img >> c.file >> c.rule;
				cmps.push_back( c );
			}
		}
	}
	if ( passes.empty() || cmps.empty() )
		return fail( "chain has no pass or no compare" );

	HMODULE lib = LoadLibraryA( "vulkan-1.dll" );
	if ( !lib )
		return fail( "no vulkan-1.dll" );
	auto gipa = reinterpret_cast<PFN_vkGetInstanceProcAddr>( GetProcAddress( lib, "vkGetInstanceProcAddr" ) );
	vkCreateInstance = reinterpret_cast<PFN_vkCreateInstance>( gipa( nullptr, "vkCreateInstance" ) );
	VkApplicationInfo app { VK_STRUCTURE_TYPE_APPLICATION_INFO };
	app.apiVersion = VK_API_VERSION_1_2;
	VkInstanceCreateInfo ici { VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };
	ici.pApplicationInfo = &app;
	VkInstance inst;
	if ( vkCreateInstance( &ici, nullptr, &inst ) != VK_SUCCESS )
		return fail( "instance" );
#define LOADI( fn ) fn = reinterpret_cast<PFN_##fn>( gipa( inst, #fn ) )
	LOADI( vkEnumeratePhysicalDevices ); LOADI( vkGetPhysicalDeviceQueueFamilyProperties ); LOADI( vkGetPhysicalDeviceProperties );
	LOADI( vkGetPhysicalDeviceMemoryProperties ); LOADI( vkCreateDevice ); LOADI( vkGetDeviceQueue ); LOADI( vkGetPhysicalDeviceFeatures );
	uint32_t nd = 0;
	vkEnumeratePhysicalDevices( inst, &nd, nullptr );
	std::vector<VkPhysicalDevice> pds( nd );
	vkEnumeratePhysicalDevices( inst, &nd, pds.data() );
	if ( !nd )
		return fail( "no device" );
	VkPhysicalDevice phys = pds[0];
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
	VkPhysicalDeviceFeatures have {};
	vkGetPhysicalDeviceFeatures( phys, &have );
	if ( !have.shaderStorageImageExtendedFormats )
		return fail( "no shaderStorageImageExtendedFormats (rg16f / r16f targets)" );
	uint32_t nq = 0;
	vkGetPhysicalDeviceQueueFamilyProperties( phys, &nq, nullptr );
	std::vector<VkQueueFamilyProperties> qf( nq );
	vkGetPhysicalDeviceQueueFamilyProperties( phys, &nq, qf.data() );
	family = UINT32_MAX;
	for ( uint32_t i = 0; family == UINT32_MAX && i < nq; i++ )
		if ( rasterMode ? ( qf[i].queueFlags & VK_QUEUE_GRAPHICS_BIT ) && qf[i].timestampValidBits
						: ( qf[i].queueFlags & VK_QUEUE_COMPUTE_BIT ) && !( qf[i].queueFlags & VK_QUEUE_GRAPHICS_BIT ) && qf[i].timestampValidBits )
			family = i;
	for ( uint32_t i = 0; family == UINT32_MAX && i < nq; i++ )
		if ( qf[i].queueFlags & VK_QUEUE_COMPUTE_BIT )
			family = i;
	const float prio = 1.0f;
	VkDeviceQueueCreateInfo qi { VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO };
	qi.queueFamilyIndex = family;
	qi.queueCount = 1;
	qi.pQueuePriorities = &prio;
	VkPhysicalDeviceFeatures want {};
	want.shaderStorageImageExtendedFormats = VK_TRUE;
	want.shaderFloat64 = have.shaderFloat64;	// AO angle reduction (cell_ao.frag doubles)
	VkDeviceCreateInfo di { VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
	di.queueCreateInfoCount = 1;
	di.pQueueCreateInfos = &qi;
	di.pEnabledFeatures = &want;
	if ( vkCreateDevice( phys, &di, nullptr, &dev ) != VK_SUCCESS )
		return fail( "device" );
	auto gdpa = reinterpret_cast<PFN_vkGetDeviceProcAddr>( gipa( inst, "vkGetDeviceProcAddr" ) );
#define LOADD( fn ) fn = reinterpret_cast<PFN_##fn>( gdpa( dev, #fn ) )
	LOADD( vkCreateBuffer ); LOADD( vkGetBufferMemoryRequirements ); LOADD( vkAllocateMemory ); LOADD( vkBindBufferMemory );
	LOADD( vkMapMemory ); LOADD( vkCreateImage ); LOADD( vkGetImageMemoryRequirements ); LOADD( vkBindImageMemory );
	LOADD( vkCreateImageView ); LOADD( vkCreateSampler ); LOADD( vkCreateDescriptorSetLayout ); LOADD( vkCreatePipelineLayout );
	LOADD( vkCreateShaderModule ); LOADD( vkCreateComputePipelines ); LOADD( vkCreateDescriptorPool ); LOADD( vkAllocateDescriptorSets );
	LOADD( vkUpdateDescriptorSets ); LOADD( vkCreateCommandPool ); LOADD( vkAllocateCommandBuffers ); LOADD( vkBeginCommandBuffer );
	LOADD( vkEndCommandBuffer ); LOADD( vkCmdBindPipeline ); LOADD( vkCmdBindDescriptorSets ); LOADD( vkCmdPushConstants );
	LOADD( vkCmdDispatch ); LOADD( vkQueueSubmit ); LOADD( vkQueueWaitIdle ); LOADD( vkCmdPipelineBarrier );
	LOADD( vkCmdCopyBufferToImage ); LOADD( vkCmdCopyImageToBuffer ); LOADD( vkCreateQueryPool ); LOADD( vkCmdResetQueryPool );
	LOADD( vkCmdWriteTimestamp ); LOADD( vkGetQueryPoolResults );
	LOADD( vkCreateGraphicsPipelines ); LOADD( vkCreateRenderPass ); LOADD( vkCreateFramebuffer ); LOADD( vkCmdBeginRenderPass );
	LOADD( vkCmdEndRenderPass ); LOADD( vkCmdDraw );
	vkGetDeviceQueue( dev, family, 0, &queue );
	if ( !makeLayout( uint32_t( passes.size() ) ) )
		return fail( "layout" );

	// images, all to GENERAL; a 1x1 dummy for unbound slots
	std::map<std::string, Img> imgs;
	size_t maxBytes = 64;
	const std::string ldr = rasterMode == 8 ? passes.back().out[0] : std::string();	// the LDR target in RGBA8
	for ( const std::string & n : imgOrder ) {
		const auto & d = imgDecl[n];
		imgs[n] = makeImg( d.second.first, d.second.second, n == ldr ? kRgba8 : *d.first, imgLv[n] );
		imgs[n].file = imgFile[n];
		maxBytes = std::max( maxBytes, size_t( d.second.first ) * d.second.second * d.first->tb );
	}
	const Img dummy = makeImg( 1, 1, kFmts[0] );
	{
		VkCommandBuffer cb = newCb();
		std::vector<VkImage> all { dummy.i };
		for ( auto & kv : imgs )
			all.push_back( kv.second.i );
		for ( VkImage x : all ) {
			VkImageMemoryBarrier ib { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
			ib.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
			ib.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			ib.newLayout = VK_IMAGE_LAYOUT_GENERAL;
			ib.srcQueueFamilyIndex = ib.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			ib.image = x;
			ib.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, VK_REMAINING_MIP_LEVELS, 0, 1 };
			vkCmdPipelineBarrier( cb, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &ib );
		}
		vkEndCommandBuffer( cb );
		submitWait( cb );
	}
	Buf stage = makeBuf( maxBytes + 256 );
	for ( const std::string & n : imgOrder ) {
		Img & x = imgs[n];
		if ( x.file == "-" )
			continue;
		const std::vector<uint8_t> f = readFile( parent + x.file );
		size_t at = 0;
		for ( int m = 0; m < x.lv; m++ ) {
			const int mw = std::max( x.w >> m, 1 ), mh = std::max( x.h >> m, 1 );
			const size_t want = size_t( mw ) * mh * x.tb;
			if ( f.size() < at + want + 8 || reinterpret_cast<const int32_t *>( f.data() + at )[0] != mw
				|| reinterpret_cast<const int32_t *>( f.data() + at )[1] != mh ) {
				std::printf( "input %s (%s) level %d: wrong size\n", n.c_str(), x.file.c_str(), m );
				return fail( "input file" );
			}
			std::memcpy( stage.p, f.data() + at + 8, want );
			at += want + 8;
			VkCommandBuffer cb = newCb();
			VkBufferImageCopy c {};
			c.imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, uint32_t( m ), 0, 1 };
			c.imageExtent = { uint32_t( mw ), uint32_t( mh ), 1 };
			vkCmdCopyBufferToImage( cb, stage.b, x.i, VK_IMAGE_LAYOUT_GENERAL, 1, &c );
			vkEndCommandBuffer( cb );
			submitWait( cb );
		}
		if ( at != f.size() ) {
			std::printf( "input %s (%s): %zu bytes left over\n", n.c_str(), x.file.c_str(), f.size() - at );
			return fail( "input file" );
		}
	}

	// the storage buffer (binding 9): the chain's `buf` file, else 256 zero bytes
	std::vector<uint8_t> bufData = bufFile.empty() ? std::vector<uint8_t>( 256, 0 ) : readFile( dir + bufFile );
	if ( bufData.empty() )
		return fail( "buf file" );
	Buf sbuf = makeBuf( bufData.size() );
	std::memcpy( sbuf.p, bufData.data(), bufData.size() );
	VkDescriptorBufferInfo sbi { sbuf.b, 0, VK_WHOLE_SIZE };

	// one set, one pipeline, one push block a pass
	std::map<std::string, VkPipeline> pipes;
	std::vector<VkDescriptorSet> sets( passes.size() );
	std::vector<std::vector<uint8_t>> pcs( passes.size() );
	std::vector<Raster> rasters( passes.size() );
	for ( size_t k = 0; k < passes.size(); k++ ) {
		const Pass & p = passes[k];
		if ( rasterMode ) {
			static const VkShaderModule vs = loadModule( spvDir + "pfx_vs.spv" );
			const VkShaderModule ps = loadModule( spvDir + p.spv.substr( 0, p.spv.size() - 4 ) + "_ps.spv" );
			if ( !imgs.count( p.out[0] ) || ( p.out[1] != "-" && !imgs.count( p.out[1] ) ) )
				return fail( "pass target" );
			std::vector<const Img *> outs { &imgs[p.out[0]] };
			if ( p.out[1] != "-" )
				outs.push_back( &imgs[p.out[1]] );
			if ( vs == VK_NULL_HANDLE || ps == VK_NULL_HANDLE
				|| ( rasters[k] = makeRaster( outs, vs, ps ) ).pipe == VK_NULL_HANDLE ) {
				std::printf( "pass %s: raster spv for %s missing or no pipeline\n", p.label.c_str(), p.spv.c_str() );
				return fail( "raster pipeline" );
			}
		} else if ( !pipes.count( p.spv ) ) {
			const std::vector<uint8_t> spv = readFile( spvDir + p.spv );
			if ( spv.empty() || ( pipes[p.spv] = makePipe( spv ) ) == VK_NULL_HANDLE ) {
				std::printf( "pass %s: spv %s missing or no pipeline\n", p.label.c_str(), p.spv.c_str() );
				return fail( "pipeline" );
			}
		}
		pcs[k] = readFile( dir + p.pc );
		if ( pcs[k].size() != 128 )
			return fail( "push block file" );
		if ( !imgs.count( p.out[0] ) )
			return fail( "pass target" );
		VkDescriptorSetAllocateInfo ai { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
		ai.descriptorPool = pool;
		ai.descriptorSetCount = 1;
		ai.pSetLayouts = &setLayout;
		vkAllocateDescriptorSets( dev, &ai, &sets[k] );
		VkDescriptorImageInfo ii[kBindings];
		VkWriteDescriptorSet wr[kBindings];
		for ( uint32_t i = 0; i < kBindings; i++ ) {
			const std::string & n = i < 5 ? p.t[i] : i >= 7 ? p.out[i - 7] : std::string();
			if ( i == 9 )
				ii[i] = VkDescriptorImageInfo {};
			else if ( i < 5 || i >= 7 ) {
				if ( n != "-" && !imgs.count( n ) ) {
					std::printf( "pass %s: no image %s\n", p.label.c_str(), n.c_str() );
					return fail( "pass image" );
				}
				ii[i] = VkDescriptorImageInfo { VK_NULL_HANDLE, n == "-" ? dummy.v : imgs[n].v, VK_IMAGE_LAYOUT_GENERAL };
			} else
				ii[i] = VkDescriptorImageInfo { samplers[i - 5], VK_NULL_HANDLE, VK_IMAGE_LAYOUT_UNDEFINED };
			wr[i] = VkWriteDescriptorSet { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
			wr[i].dstSet = sets[k];
			wr[i].dstBinding = i;
			wr[i].descriptorCount = 1;
			wr[i].descriptorType = bindingType( i );
			if ( i == 9 )
				wr[i].pBufferInfo = &sbi;
			else
				wr[i].pImageInfo = &ii[i];
		}
		vkUpdateDescriptorSets( dev, kBindings, wr, 0, nullptr );
	}

	const uint32_t nts = uint32_t( passes.size() ) + 1;
	VkQueryPoolCreateInfo qpi { VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO };
	qpi.queryType = VK_QUERY_TYPE_TIMESTAMP;
	qpi.queryCount = nts;
	VkQueryPool qp;
	vkCreateQueryPool( dev, &qpi, nullptr, &qp );
	VkCommandBuffer run = newCb();
	vkCmdResetQueryPool( run, qp, 0, nts );
	vkCmdWriteTimestamp( run, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, qp, 0 );
	for ( size_t k = 0; k < passes.size(); k++ ) {
		const Img & o = imgs[passes[k].out[0]];
		VkMemoryBarrier mb { VK_STRUCTURE_TYPE_MEMORY_BARRIER };
		if ( rasterMode ) {
			VkRenderPassBeginInfo rb { VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
			rb.renderPass = rasters[k].rp;
			rb.framebuffer = rasters[k].fb;
			rb.renderArea = { { 0, 0 }, { uint32_t( o.w ), uint32_t( o.h ) } };
			vkCmdBeginRenderPass( run, &rb, VK_SUBPASS_CONTENTS_INLINE );
			vkCmdBindPipeline( run, VK_PIPELINE_BIND_POINT_GRAPHICS, rasters[k].pipe );
			vkCmdBindDescriptorSets( run, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeLayout, 0, 1, &sets[k], 0, nullptr );
			vkCmdPushConstants( run, pipeLayout, kStages, 0, 128, pcs[k].data() );
			vkCmdDraw( run, 3, 1, 0, 0 );
			vkCmdEndRenderPass( run );
			mb.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			mb.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
				| VK_ACCESS_TRANSFER_READ_BIT;
			vkCmdPipelineBarrier( run, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
				| VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 1, &mb, 0, nullptr, 0, nullptr );
		} else {
			vkCmdBindPipeline( run, VK_PIPELINE_BIND_POINT_COMPUTE, pipes[passes[k].spv] );
			vkCmdBindDescriptorSets( run, VK_PIPELINE_BIND_POINT_COMPUTE, pipeLayout, 0, 1, &sets[k], 0, nullptr );
			vkCmdPushConstants( run, pipeLayout, kStages, 0, 128, pcs[k].data() );
			if ( passes[k].gx > 0 )
				vkCmdDispatch( run, uint32_t( passes[k].gx ), uint32_t( std::max( passes[k].gy, 1 ) ), 1 );
			else
				vkCmdDispatch( run, uint32_t( ( o.w + 7 ) / 8 ), uint32_t( ( o.h + 7 ) / 8 ), 1 );
			mb.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
			mb.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_READ_BIT;
			vkCmdPipelineBarrier( run, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT,
				0, 1, &mb, 0, nullptr, 0, nullptr );
		}
		vkCmdWriteTimestamp( run, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, qp, uint32_t( k + 1 ) );
	}
	vkEndCommandBuffer( run );
	submitWait( run );

	Buf rb = makeBuf( maxBytes + 256 );
	size_t diffAll = 0, valuesAll = 0;
	bool exact = true, ok = true;
	for ( const Cmp & c : cmps ) {
		if ( c.img == ldr ) {
			std::printf( "%s: RGBA8 (LIGHTMATCH_RASTER=8), not compared\n", c.img.c_str() );
			continue;
		}
		if ( !imgs.count( c.img ) )
			return fail( "compare image" );
		const Img & x = imgs[c.img];
		const std::vector<uint8_t> g = readFile( dir + c.file );
		const size_t bytes = size_t( x.w ) * x.h * x.tb;
		if ( g.size() != bytes + 8 ) {
			std::printf( "%s: GL file missing or wrong size\n", c.file.c_str() );
			ok = false;
			continue;
		}
		VkCommandBuffer cb = newCb();
		VkBufferImageCopy bc {};
		bc.imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
		bc.imageExtent = { uint32_t( x.w ), uint32_t( x.h ), 1 };
		vkCmdCopyImageToBuffer( cb, x.i, VK_IMAGE_LAYOUT_GENERAL, rb.b, 1, &bc );
		vkEndCommandBuffer( cb );
		submitWait( cb );
		const bool h16 = x.half, b8 = x.tb == 1;	// b8: R8 unorm, compared by byte steps
		const size_t es = b8 ? 1 : h16 ? 2 : 4;
		const uint8_t * v8 = static_cast<const uint8_t *>( rb.p );
		const uint8_t * g8 = g.data() + 8;
		const uint16_t * v = static_cast<const uint16_t *>( rb.p );
		const uint16_t * gp = reinterpret_cast<const uint16_t *>( g.data() + 8 );
		const uint32_t * v32 = static_cast<const uint32_t *>( rb.p );
		const uint32_t * g32 = reinterpret_cast<const uint32_t *>( g.data() + 8 );
		if ( std::getenv( "LIGHTMATCH_DUMP" ) )	// vk_<image>.bin beside gl_<image>.bin, for a difference map
			if ( FILE * f = std::fopen( ( dir + "vk_" + c.img + ".bin" ).c_str(), "wb" ) ) {
				const int32_t wh[2] = { x.w, x.h };
				std::fwrite( wh, 4, 2, f );
				std::fwrite( rb.p, 1, bytes, f );
				std::fclose( f );
			}
		const size_t n = bytes / es;
		const int ch = x.tb / int( es );
		size_t d = 0, big = 0, nz = 0;
		int mu = 0;
		long firstBad = -1;
		const int tol = c.rule.compare( 0, 3, "ulp" ) == 0 ? std::max( std::atoi( c.rule.c_str() + 3 ), 0 ) : 1;
		// absX (float32 images): every value within X of GL's absolute, at most 2 % of values differ, no NaN mismatch
		const bool absRule = !h16 && c.rule.compare( 0, 3, "abs" ) == 0;
		const double absTol = absRule ? std::atof( c.rule.c_str() + 3 ) : 0.0;
		double maxAbs = 0.0;
		bool nanBad = false;
		for ( size_t k = 0; k < n; k++ ) {
			if ( absRule ) {
				float fa, fb;
				std::memcpy( &fa, &v32[k], 4 );
				std::memcpy( &fb, &g32[k], 4 );
				if ( ( fa != fa ) != ( fb != fb ) )
					nanBad = true;
				else if ( fa == fa )
					maxAbs = std::max( maxAbs, std::fabs( double( fa ) - double( fb ) ) );
			}
			const int u = b8 ? std::abs( int( v8[k] ) - int( g8[k] ) ) : h16 ? ulps( v[k], gp[k] ) : ulps32( v32[k], g32[k] );
			d += u != 0;
			big += u > tol;
			if ( u > tol && firstBad < 0 )
				firstBad = long( k );
			mu = std::max( mu, u );
			nz += b8 ? g8[k] != 0 : h16 ? ( gp[k] & 0x7FFF ) != 0 : ( g32[k] & 0x7FFFFFFFu ) != 0;
		}
		diffAll += d;
		valuesAll += n;
		/* step: a gather at a computed uv through the bilinear unit (8-bit sub-texel weights) or a hard branch (a depth test,
		 * a circle edge) flips on a last-bit difference; bound: <= 0.5 % of values differ and <= 0.1 % by more than 1 ulp */
		/* gather (the object blur): any count at 1 ulp (the alpha is wsum / wsum, which either side's divide leaves 1 or
		 * 1 - 1 ulp, the truncating store makes it a half ulp; measured 77410 of the 77919), <= 0.01 % by more than 1 ulp
		 * (a sample position flipped by a last-bit jitter) */
		/* sector (SSGI): the slice angle's divide by the slice count is lowered differently by NV's GL and Vulkan compilers
		 * (1-2 ulp on ~40 % of texels, no single reciprocal fits either side); a sector bit or march sample then flips:
		 * bound <= 1 % of values differ and <= 0.2 % by more than 1 ulp (the red: 12 %) */
		const bool pass = mu <= tol || ( c.rule == "step" && d * 200 <= n && big * 1000 <= n ) || ( c.rule == "gather" && big * 10000 <= n )
			|| ( c.rule == "sector" && d * 100 <= n && big * 500 <= n )
			|| ( absRule && !nanBad && maxAbs <= absTol && d * 50 <= n );
		exact = exact && d == 0;
		ok = ok && pass;
		std::printf( "%s %dx%d [%s]: %zu of %zu values differ, %zu by > tol, max %d ulp, max abs %.3g (GL non-zero %zu)%s", c.img.c_str(),
			x.w, x.h, c.rule.c_str(), d, n, big, mu, maxAbs, nz, pass ? "" : " OUT");
		if ( firstBad >= 0 )
			std::printf( "; first > 1 ulp at x %ld y %ld ch %ld: vk 0x%x gl 0x%x", ( firstBad / ch ) % x.w, ( firstBad / ch ) / x.w,
				firstBad % ch, b8 ? unsigned( v8[firstBad] ) : h16 ? unsigned( v[firstBad] ) : unsigned( v32[firstBad] ),
				b8 ? unsigned( g8[firstBad] ) : h16 ? unsigned( gp[firstBad] ) : unsigned( g32[firstBad] ) );
		std::printf( "\n" );
	}

	if ( timeIt ) {
		for ( const DWORD t0 = GetTickCount(); GetTickCount() - t0 < 500; )
			submitWait( run );
		std::vector<std::vector<double>> ms( passes.size() + 1 );
		for ( int r = 0; r < 34; r++ ) {
			submitWait( run );
			std::vector<uint64_t> ts( nts );
			vkGetQueryPoolResults( dev, qp, 0, nts, nts * 8, ts.data(), 8, VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT );
			if ( r < 2 )
				continue;
			for ( size_t k = 0; k < passes.size(); k++ )
				ms[k].push_back( double( ts[k + 1] - ts[k] ) * props.limits.timestampPeriod / 1e6 );
			ms[passes.size()].push_back( double( ts[nts - 1] - ts[0] ) * props.limits.timestampPeriod / 1e6 );
		}
		std::map<std::string, double> gl;
		if ( FILE * f = std::fopen( ( dir + "gl_ms.txt" ).c_str(), "r" ) ) {
			char lab[64];
			double m;
			while ( std::fscanf( f, "%63s %lf", lab, &m ) == 2 )
				gl[lab] = m;
			std::fclose( f );
		}
		double glTot = 0.0;
		for ( size_t k = 0; k <= passes.size(); k++ ) {
			std::sort( ms[k].begin(), ms[k].end() );
			const double vk = ( ms[k][15] + ms[k][16] ) * 0.5;
			const std::string lab = k < passes.size() ? passes[k].label : "total";
			double g = -1.0;
			if ( k < passes.size() && gl.count( lab ) ) {
				g = gl[lab];
				glTot += g;
			} else if ( k == passes.size() && !gl.empty() )
				g = glTot;
			if ( g > 0.0 )
				std::printf( "time %s %s: Vulkan %.4f ms, GL %.4f ms: %+.4f ms (%+.1f %%)\n", name.c_str(), lab.c_str(), vk, g, vk - g,
					( vk - g ) / g * 100.0 );
			else
				std::printf( "time %s %s: Vulkan %.4f ms\n", name.c_str(), lab.c_str(), vk );
		}
	}

	if ( !ok ) {
		std::printf( "LIGHTMATCH %s FAIL (%zu of %zu values differ)\n", name.c_str(), diffAll, valuesAll );
		return 1;
	}
	if ( exact ) {
		std::printf( "LIGHTMATCH %s EXACT (%zu values)\n", name.c_str(), valuesAll );
		return 0;
	}
	std::printf( "LIGHTMATCH %s BOUND (%zu of %zu values differ, every stage within its rule)\n", name.c_str(), diffAll, valuesAll );
	return 0;
}
