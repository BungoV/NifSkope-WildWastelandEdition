// Lane VKFRAME1 (2026-11-12, P7): one chain of the depth of field / motion blur / object blur kernels on Vulkan
// (res/shaders/hlsl/post/dof_*, mblur, doflens_*, objblur* over PostFxBindingsVk.hlsli) against GL's .frag chain on the same
// inputs (tests/vkframe/postfx_gl_ref.py writes the chain file, the push blocks and GL's results); every compared stage's
// half-float bits. No NifSkope: a headless Vulkan device of its own (the compute-only family when there is one).
// Usage: postfxmatch <chain.txt> <spv dir> [time]
// chain.txt:  img <name> <rgba16f|rg16f|r16f|r32f|rg32f> <w> <h> <file relative to the job's parent | ->
//             pass <label> <spv> <pc file> <out0> <out1|-> <t0> <t1> <t2> <t3> <t4>   (- = unbound)
//             cmp <image> <gl file> <ulp1|step|gather>
// Prints a line per compare, with `time` the per-pass GPU ms (timestamps, median of 32 after 0.5 s of warm-up chains) beside
// gl_ms.txt, then `POSTFXMATCH <job> EXACT|BOUND (...)|FAIL (...)`. Exit 0 for EXACT or BOUND.
#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include <windows.h>
#include <algorithm>
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

struct Img { VkImage i = VK_NULL_HANDLE; VkDeviceMemory m = VK_NULL_HANDLE; VkImageView v = VK_NULL_HANDLE; int w = 0, h = 0, tb = 8; bool half = true; std::string file; };

struct Fmt { const char * name; VkFormat f; int tb; bool half; };
static const Fmt kFmts[] = { { "rgba16f", VK_FORMAT_R16G16B16A16_SFLOAT, 8, true }, { "rg16f", VK_FORMAT_R16G16_SFLOAT, 4, true },
	{ "r16f", VK_FORMAT_R16_SFLOAT, 2, true }, { "r32f", VK_FORMAT_R32_SFLOAT, 4, false }, { "rg32f", VK_FORMAT_R32G32_SFLOAT, 8, false } };

static Img makeImg( int w, int h, const Fmt & fm )
{
	Img I;
	I.w = w;
	I.h = h;
	I.tb = fm.tb;
	I.half = fm.half;
	VkImageCreateInfo ci { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
	ci.imageType = VK_IMAGE_TYPE_2D;
	ci.format = fm.f;
	ci.extent = { uint32_t( w ), uint32_t( h ), 1 };
	ci.mipLevels = 1;
	ci.arrayLayers = 1;
	ci.samples = VK_SAMPLE_COUNT_1_BIT;
	ci.tiling = VK_IMAGE_TILING_OPTIMAL;
	ci.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_STORAGE_BIT;
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
	vi.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
	vkCreateImageView( dev, &vi, nullptr, &I.v );
	return I;
}

static VkDescriptorSetLayout setLayout;
static VkPipelineLayout pipeLayout;
static VkDescriptorPool pool;
static VkCommandPool cpool;
static VkSampler samplers[2];	// linear, point
static const uint32_t kBindings = 9;

// PostFxBindingsVk.hlsli: 0-4 sampled, 5 linear, 6 point, 7-8 storage
static VkDescriptorType bindingType( uint32_t i )
{
	return i < 5 ? VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE : ( i < 7 ? VK_DESCRIPTOR_TYPE_SAMPLER : VK_DESCRIPTOR_TYPE_STORAGE_IMAGE );
}

static bool makeLayout( uint32_t sets )
{
	VkDescriptorSetLayoutBinding b[kBindings];
	for ( uint32_t i = 0; i < kBindings; i++ )
		b[i] = VkDescriptorSetLayoutBinding { i, bindingType( i ), 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr };
	VkDescriptorSetLayoutCreateInfo li { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
	li.bindingCount = kBindings;
	li.pBindings = b;
	if ( vkCreateDescriptorSetLayout( dev, &li, nullptr, &setLayout ) != VK_SUCCESS )
		return false;
	VkPushConstantRange pr { VK_SHADER_STAGE_COMPUTE_BIT, 0, 128 };
	VkPipelineLayoutCreateInfo pi { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
	pi.setLayoutCount = 1;
	pi.pSetLayouts = &setLayout;
	pi.pushConstantRangeCount = 1;
	pi.pPushConstantRanges = &pr;
	if ( vkCreatePipelineLayout( dev, &pi, nullptr, &pipeLayout ) != VK_SUCCESS )
		return false;
	VkDescriptorPoolSize ps[3] = { { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 5 * sets }, { VK_DESCRIPTOR_TYPE_SAMPLER, 2 * sets },
		{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 2 * sets } };
	VkDescriptorPoolCreateInfo dp { VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
	dp.maxSets = sets;
	dp.poolSizeCount = 3;
	dp.pPoolSizes = ps;
	if ( vkCreateDescriptorPool( dev, &dp, nullptr, &pool ) != VK_SUCCESS )
		return false;
	for ( int s = 0; s < 2; s++ ) {
		VkSamplerCreateInfo si { VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
		si.magFilter = si.minFilter = s == 0 ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;
		si.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
		si.addressModeU = si.addressModeV = si.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		si.maxLod = 0.0f;
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

// the distance of two halves in representable steps; two NaNs are equal, NaN against a number = big
static int ulps( uint16_t a, uint16_t b )
{
	auto ord = []( uint16_t x ) { return ( x & 0x8000 ) ? -int( x & 0x7FFF ) : int( x ); };
	auto nan = []( uint16_t x ) { return ( x & 0x7C00 ) == 0x7C00 && ( x & 0x03FF ); };
	if ( nan( a ) || nan( b ) )
		return nan( a ) == nan( b ) ? 0 : 1 << 20;
	return std::abs( ord( a ) - ord( b ) );
}

struct Pass { std::string label, spv, pc; std::string out[2], t[5]; };
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
	const std::string dir = chainPath.substr( 0, chainPath.find_last_of( "/\\" ) + 1 );
	std::string name = dir.substr( 0, dir.size() - 1 );
	const std::string parent = name.substr( 0, name.find_last_of( "/\\" ) + 1 );
	name = name.substr( name.find_last_of( "/\\" ) + 1 ) + "/" + chainPath.substr( chainPath.find_last_of( "/\\" ) + 1 );
	auto fail = [&]( const char * why ) {
		std::printf( "POSTFXMATCH %s FAIL (%s)\n", name.c_str(), why );
		return 2;
	};

	std::vector<std::string> imgOrder;
	std::map<std::string, std::pair<const Fmt *, std::pair<int, int>>> imgDecl;
	std::map<std::string, std::string> imgFile;
	std::vector<Pass> passes;
	std::vector<Cmp> cmps;
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
			} else if ( k == "pass" ) {
				Pass p;
				l >> p.label >> p.spv >> p.pc >> p.out[0] >> p.out[1];
				for ( auto & t : p.t )
					l >> t;
				passes.push_back( p );
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
		if ( ( qf[i].queueFlags & VK_QUEUE_COMPUTE_BIT ) && !( qf[i].queueFlags & VK_QUEUE_GRAPHICS_BIT ) && qf[i].timestampValidBits )
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
	vkGetDeviceQueue( dev, family, 0, &queue );
	if ( !makeLayout( uint32_t( passes.size() ) ) )
		return fail( "layout" );

	// images, all to GENERAL; a 1x1 dummy for unbound slots
	std::map<std::string, Img> imgs;
	size_t maxBytes = 64;
	for ( const std::string & n : imgOrder ) {
		const auto & d = imgDecl[n];
		imgs[n] = makeImg( d.second.first, d.second.second, *d.first );
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
			ib.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
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
		const size_t want = size_t( x.w ) * x.h * x.tb;
		if ( f.size() != want + 8 || reinterpret_cast<const int32_t *>( f.data() )[0] != x.w
			|| reinterpret_cast<const int32_t *>( f.data() )[1] != x.h ) {
			std::printf( "input %s (%s): wrong size\n", n.c_str(), x.file.c_str() );
			return fail( "input file" );
		}
		std::memcpy( stage.p, f.data() + 8, want );
		VkCommandBuffer cb = newCb();
		VkBufferImageCopy c {};
		c.imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
		c.imageExtent = { uint32_t( x.w ), uint32_t( x.h ), 1 };
		vkCmdCopyBufferToImage( cb, stage.b, x.i, VK_IMAGE_LAYOUT_GENERAL, 1, &c );
		vkEndCommandBuffer( cb );
		submitWait( cb );
	}

	// one set, one pipeline, one push block a pass
	std::map<std::string, VkPipeline> pipes;
	std::vector<VkDescriptorSet> sets( passes.size() );
	std::vector<std::vector<uint8_t>> pcs( passes.size() );
	for ( size_t k = 0; k < passes.size(); k++ ) {
		const Pass & p = passes[k];
		if ( !pipes.count( p.spv ) ) {
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
			if ( i < 5 || i >= 7 ) {
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
		vkCmdBindPipeline( run, VK_PIPELINE_BIND_POINT_COMPUTE, pipes[passes[k].spv] );
		vkCmdBindDescriptorSets( run, VK_PIPELINE_BIND_POINT_COMPUTE, pipeLayout, 0, 1, &sets[k], 0, nullptr );
		vkCmdPushConstants( run, pipeLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, 128, pcs[k].data() );
		vkCmdDispatch( run, uint32_t( ( o.w + 7 ) / 8 ), uint32_t( ( o.h + 7 ) / 8 ), 1 );
		VkMemoryBarrier mb { VK_STRUCTURE_TYPE_MEMORY_BARRIER };
		mb.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
		mb.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_READ_BIT;
		vkCmdPipelineBarrier( run, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT,
			0, 1, &mb, 0, nullptr, 0, nullptr );
		vkCmdWriteTimestamp( run, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, qp, uint32_t( k + 1 ) );
	}
	vkEndCommandBuffer( run );
	submitWait( run );

	Buf rb = makeBuf( maxBytes + 256 );
	size_t diffAll = 0, valuesAll = 0;
	bool exact = true, ok = true;
	for ( const Cmp & c : cmps ) {
		if ( !imgs.count( c.img ) || !imgs[c.img].half )
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
		const uint16_t * v = static_cast<const uint16_t *>( rb.p );
		const uint16_t * gp = reinterpret_cast<const uint16_t *>( g.data() + 8 );
		if ( std::getenv( "POSTFXMATCH_DUMP" ) )	// vk_<image>.bin beside gl_<image>.bin, for a difference map
			if ( FILE * f = std::fopen( ( dir + "vk_" + c.img + ".bin" ).c_str(), "wb" ) ) {
				const int32_t wh[2] = { x.w, x.h };
				std::fwrite( wh, 4, 2, f );
				std::fwrite( v, 1, bytes, f );
				std::fclose( f );
			}
		const size_t n = bytes / 2;
		const int ch = x.tb / 2;
		size_t d = 0, big = 0, nz = 0;
		int mu = 0;
		long firstBad = -1;
		for ( size_t k = 0; k < n; k++ ) {
			const int u = ulps( v[k], gp[k] );
			d += u != 0;
			big += u > 1;
			if ( u > 1 && firstBad < 0 )
				firstBad = long( k );
			mu = std::max( mu, u );
			nz += ( gp[k] & 0x7FFF ) != 0;
		}
		diffAll += d;
		valuesAll += n;
		/* step: a gather at a computed uv through the bilinear unit (8-bit sub-texel weights) or a hard branch (a depth test,
		 * a circle edge) flips on a last-bit difference; bound: <= 0.5 % of halves differ and <= 0.1 % by more than 1 ulp */
		/* gather (the object blur): any count at 1 ulp (the alpha is wsum / wsum, which either side's divide leaves 1 or
		 * 1 - 1 ulp, the truncating store makes it a half ulp; measured 77410 of the 77919), <= 0.01 % by more than 1 ulp
		 * (a sample position flipped by a last-bit jitter) */
		const bool pass = mu <= 1 || ( c.rule == "step" && d * 200 <= n && big * 1000 <= n ) || ( c.rule == "gather" && big * 10000 <= n );
		exact = exact && d == 0;
		ok = ok && pass;
		std::printf( "%s %dx%d [%s]: %zu of %zu halves differ, %zu by > 1 ulp, max %d ulp (GL non-zero %zu)%s", c.img.c_str(), x.w, x.h,
			c.rule.c_str(), d, n, big, mu, nz, pass ? "" : " OUT");
		if ( firstBad >= 0 )
			std::printf( "; first > 1 ulp at x %ld y %ld ch %ld: vk 0x%04x gl 0x%04x", ( firstBad / ch ) % x.w, ( firstBad / ch ) / x.w,
				firstBad % ch, v[firstBad], gp[firstBad] );
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
		std::printf( "POSTFXMATCH %s FAIL (%zu of %zu halves differ)\n", name.c_str(), diffAll, valuesAll );
		return 1;
	}
	if ( exact ) {
		std::printf( "POSTFXMATCH %s EXACT (%zu halves)\n", name.c_str(), valuesAll );
		return 0;
	}
	std::printf( "POSTFXMATCH %s BOUND (%zu of %zu halves differ, every stage within its rule)\n", name.c_str(), diffAll, valuesAll );
	return 0;
}
