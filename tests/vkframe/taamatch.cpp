// Lane VKFRAME1 (2026-11-12, P5): the Modern temporal AA resolve on Vulkan (res/shaders/hlsl/post/taa_resolve.hlsl over
// TaaCommon.hlsli) against GL's taa2.frag on the same inputs (tests/vkframe/taa_gl_ref.py writes them), the three outputs'
// half-float bits compared. No NifSkope: a headless Vulkan device of its own (the compute-only family when there is one, as
// bakert's); cur / hist / lock / mask RGBA16F, mv / vel RG32F, depth R32F (the GL reference's formats), outputs RGBA16F
// storage, all in GENERAL; the LinearClamp sampler.
// Usage: taamatch <job dir> <taa_resolve.spv>
// Prints one line an output, the resolve's GPU ms (timestamps, median of 32 after 2 warm-ups), then `TAAMATCH <job> EXACT`,
// `TAAMATCH <job> BOUND (...)` (max 1 ulp, or the counted step rule at the verdict) or `TAAMATCH <job> FAIL ...`. Exit 0 for
// EXACT or BOUND. TAAMATCH_GROUP=<x>x<y> dispatches for a scratch kernel of another group shape.
#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include <windows.h>
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#define VKF( name ) static PFN_##name name
VKF( vkCreateInstance ); VKF( vkEnumeratePhysicalDevices ); VKF( vkGetPhysicalDeviceQueueFamilyProperties );
VKF( vkGetPhysicalDeviceProperties ); VKF( vkGetPhysicalDeviceMemoryProperties ); VKF( vkCreateDevice ); VKF( vkGetDeviceQueue );
VKF( vkCreateBuffer ); VKF( vkGetBufferMemoryRequirements ); VKF( vkAllocateMemory ); VKF( vkBindBufferMemory ); VKF( vkMapMemory );
VKF( vkCreateImage ); VKF( vkGetImageMemoryRequirements ); VKF( vkBindImageMemory ); VKF( vkCreateImageView ); VKF( vkCreateSampler );
VKF( vkCreateDescriptorSetLayout ); VKF( vkCreatePipelineLayout ); VKF( vkCreateShaderModule ); VKF( vkCreateComputePipelines );
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

static std::vector<uint32_t> readSpv( const std::string & path )
{
	std::vector<uint32_t> v;
	FILE * f = std::fopen( path.c_str(), "rb" );
	if ( !f )
		return v;
	std::fseek( f, 0, SEEK_END );
	const long n = std::ftell( f );
	std::fseek( f, 0, SEEK_SET );
	v.resize( static_cast<size_t>( n ) / 4 );
	if ( std::fread( v.data(), 4, v.size(), f ) != v.size() )
		v.clear();
	std::fclose( f );
	return v;
}

// int32 W, H, then the texels (bytes as stored)
struct Raw { int w = 0, h = 0; std::vector<uint8_t> b; };

static bool readRaw( const std::string & path, Raw & out, size_t texelBytes )
{
	FILE * f = std::fopen( path.c_str(), "rb" );
	if ( !f )
		return false;
	int32_t hdr[2] = { 0, 0 };
	bool ok = std::fread( hdr, 4, 2, f ) == 2;
	out.w = hdr[0];
	out.h = hdr[1];
	out.b.resize( static_cast<size_t>( out.w ) * out.h * texelBytes );
	ok = ok && std::fread( out.b.data(), 1, out.b.size(), f ) == out.b.size();
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

struct Img { VkImage i = VK_NULL_HANDLE; VkDeviceMemory m = VK_NULL_HANDLE; VkImageView v = VK_NULL_HANDLE; int w = 0, h = 0; };

static Img makeImg( int w, int h, VkFormat fmt, bool storage )
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
	ci.samples = VK_SAMPLE_COUNT_1_BIT;
	ci.tiling = VK_IMAGE_TILING_OPTIMAL;
	ci.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT
		| ( storage ? VK_IMAGE_USAGE_STORAGE_BIT : 0 );
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
	vi.format = fmt;
	vi.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
	vkCreateImageView( dev, &vi, nullptr, &I.v );
	return I;
}

static VkDescriptorSetLayout setLayout;
static VkPipelineLayout pipeLayout;
static VkDescriptorPool pool;
static VkCommandPool cpool;
static VkSampler linearClamp;
static const uint32_t kBindings = 11;

// TaaBindingsVk.hlsli: 0-6 sampled, 7 sampler, 8-10 storage
static VkDescriptorType bindingType( uint32_t i )
{
	return i < 7 ? VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE : ( i == 7 ? VK_DESCRIPTOR_TYPE_SAMPLER : VK_DESCRIPTOR_TYPE_STORAGE_IMAGE );
}

static bool makeLayout()
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
	VkDescriptorPoolSize ps[3] = { { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 7 * 4 }, { VK_DESCRIPTOR_TYPE_SAMPLER, 4 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 3 * 4 } };
	VkDescriptorPoolCreateInfo dp { VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
	dp.maxSets = 4;
	dp.poolSizeCount = 3;
	dp.pPoolSizes = ps;
	if ( vkCreateDescriptorPool( dev, &dp, nullptr, &pool ) != VK_SUCCESS )
		return false;
	VkSamplerCreateInfo si { VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
	si.magFilter = si.minFilter = VK_FILTER_LINEAR;
	si.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
	si.addressModeU = si.addressModeV = si.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	si.maxLod = 0.0f;
	if ( vkCreateSampler( dev, &si, nullptr, &linearClamp ) != VK_SUCCESS )
		return false;
	VkCommandPoolCreateInfo ci { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
	ci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	ci.queueFamilyIndex = family;
	return vkCreateCommandPool( dev, &ci, nullptr, &cpool ) == VK_SUCCESS;
}

static VkPipeline makePipe( const std::vector<uint32_t> & spv )
{
	VkShaderModuleCreateInfo mi { VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
	mi.codeSize = spv.size() * 4;
	mi.pCode = spv.data();
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

// TaaBindingsVk.hlsli TaaJob
struct Job { float sizeJitter[4]; int32_t ints[4]; int32_t ints2[4]; float f[4]; float reproj[16]; };

static bool readJob( const std::string & path, Job & j, std::string & input )
{
	FILE * f = std::fopen( path.c_str(), "r" );
	if ( !f )
		return false;
	char key[32], val[256];
	int seed = -1;
	bool haveTier = false;
	while ( std::fscanf( f, "%31s", key ) == 1 ) {
		const std::string k = key;
		if ( k == "input" ) { if ( std::fscanf( f, "%255s", val ) == 1 ) input = val; }
		else if ( k == "tier" ) haveTier = std::fscanf( f, "%d", &j.ints[0] ) == 1;
		else if ( k == "red" ) std::fscanf( f, "%d", &j.ints[1] );
		else if ( k == "nohist" ) std::fscanf( f, "%d", &j.ints[2] );
		else if ( k == "inputs" ) std::fscanf( f, "%d", &j.ints[3] );
		else if ( k == "linear" ) std::fscanf( f, "%d", &j.ints2[0] );
		else if ( k == "seed" ) std::fscanf( f, "%d", &seed );
		else if ( k == "expose" ) std::fscanf( f, "%f", &j.f[0] );
		else if ( k == "jx" ) std::fscanf( f, "%f", &j.sizeJitter[2] );
		else if ( k == "jy" ) std::fscanf( f, "%f", &j.sizeJitter[3] );
		else if ( k == "reproj" )
			for ( int i = 0; i < 16; i++ )
				std::fscanf( f, "%f", &j.reproj[i] );
	}
	std::fclose( f );
	j.ints2[1] = seed >= 0 ? 1 : 0;
	j.ints2[2] = std::max( seed, 0 );
	return haveTier && !input.empty();
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

int main( int argc, char ** argv )
{
	std::setvbuf( stdout, nullptr, _IONBF, 0 );
	if ( argc < 3 ) {
		std::printf( "usage: taamatch <job dir> <taa_resolve.spv>\n" );
		return 2;
	}
	const std::string dir = argv[1];
	std::string name = dir;
	while ( !name.empty() && ( name.back() == '/' || name.back() == '\\' ) )
		name.pop_back();
	const std::string parent = name.substr( 0, name.find_last_of( "/\\" ) + 1 );
	name = name.substr( name.find_last_of( "/\\" ) + 1 );
	Job job {};
	std::string input;
	if ( !readJob( dir + "/job.txt", job, input ) ) {
		std::printf( "TAAMATCH %s FAIL (job.txt)\n", name.c_str() );
		return 2;
	}
	const bool disp = job.ints2[0] == 0;
	const std::string in = parent + input + "/";
	Raw cur, hist, lock, mask, mv, vel, depth;
	if ( !readRaw( in + ( disp ? "curd.bin" : "cur.bin" ), cur, 8 ) || !readRaw( in + ( disp ? "histd.bin" : "hist.bin" ), hist, 8 )
		|| !readRaw( in + "lock.bin", lock, 8 ) || !readRaw( in + "mask.bin", mask, 8 ) || !readRaw( in + "mv.bin", mv, 8 )
		|| !readRaw( in + "vel.bin", vel, 8 ) || !readRaw( in + "depth.bin", depth, 4 ) ) {
		std::printf( "TAAMATCH %s FAIL (input files in %s)\n", name.c_str(), in.c_str() );
		return 2;
	}
	const int W = cur.w, H = cur.h;
	job.sizeJitter[0] = 1.0f / float( W );
	job.sizeJitter[1] = 1.0f / float( H );
	const std::vector<uint32_t> spv = readSpv( argv[2] );
	if ( spv.empty() ) {
		std::printf( "TAAMATCH %s FAIL (spv)\n", name.c_str() );
		return 2;
	}
	HMODULE lib = LoadLibraryA( "vulkan-1.dll" );
	if ( !lib ) {
		std::printf( "TAAMATCH %s FAIL (no vulkan-1.dll)\n", name.c_str() );
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
		std::printf( "TAAMATCH %s FAIL (instance)\n", name.c_str() );
		return 2;
	}
#define LOADI( fn ) fn = reinterpret_cast<PFN_##fn>( gipa( inst, #fn ) )
	LOADI( vkEnumeratePhysicalDevices ); LOADI( vkGetPhysicalDeviceQueueFamilyProperties ); LOADI( vkGetPhysicalDeviceProperties );
	LOADI( vkGetPhysicalDeviceMemoryProperties ); LOADI( vkCreateDevice ); LOADI( vkGetDeviceQueue );
	uint32_t nd = 0;
	vkEnumeratePhysicalDevices( inst, &nd, nullptr );
	std::vector<VkPhysicalDevice> pds( nd );
	vkEnumeratePhysicalDevices( inst, &nd, pds.data() );
	if ( !nd ) {
		std::printf( "TAAMATCH %s FAIL (no device)\n", name.c_str() );
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
	const char * fam = std::getenv( "TAAMATCH_GRAPHICS_QUEUE" );	// 1: the graphics family (timing comparison)
	for ( uint32_t i = 0; i < nq && fam && fam[0] == '1'; i++ )
		if ( qf[i].queueFlags & VK_QUEUE_GRAPHICS_BIT ) {
			family = i;
			break;
		}
	for ( uint32_t i = 0; family == UINT32_MAX && i < nq; i++ )
		if ( ( qf[i].queueFlags & VK_QUEUE_COMPUTE_BIT ) && !( qf[i].queueFlags & VK_QUEUE_GRAPHICS_BIT ) && qf[i].timestampValidBits ) {
			family = i;
			break;
		}
	for ( uint32_t i = 0; family == UINT32_MAX && i < nq; i++ )
		if ( qf[i].queueFlags & VK_QUEUE_COMPUTE_BIT )
			family = i;
	const float prio = 1.0f;
	VkDeviceQueueCreateInfo qi { VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO };
	qi.queueFamilyIndex = family;
	qi.queueCount = 1;
	qi.pQueuePriorities = &prio;
	VkDeviceCreateInfo di { VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
	di.queueCreateInfoCount = 1;
	di.pQueueCreateInfos = &qi;
	if ( vkCreateDevice( phys, &di, nullptr, &dev ) != VK_SUCCESS ) {
		std::printf( "TAAMATCH %s FAIL (device)\n", name.c_str() );
		return 2;
	}
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
	if ( !makeLayout() ) {
		std::printf( "TAAMATCH %s FAIL (layout)\n", name.c_str() );
		return 2;
	}
	const VkPipeline pipe = makePipe( spv );
	if ( !pipe ) {
		std::printf( "TAAMATCH %s FAIL (pipeline)\n", name.c_str() );
		return 2;
	}
	std::printf( "device %s, queue family %u (%s)\n", props.deviceName, family,
		( qf[family].queueFlags & VK_QUEUE_GRAPHICS_BIT ) ? "graphics" : "compute-only" );

	const VkFormat F16 = VK_FORMAT_R16G16B16A16_SFLOAT;
	Img iCur = makeImg( W, H, F16, false ), iHist = makeImg( W, H, F16, false ), iLock = makeImg( W, H, F16, false );
	Img iMask = makeImg( W, H, F16, false ), iMv = makeImg( W, H, VK_FORMAT_R32G32_SFLOAT, false );
	Img iVel = makeImg( W, H, VK_FORMAT_R32G32_SFLOAT, false ), iDepth = makeImg( W, H, VK_FORMAT_R32_SFLOAT, false );
	Img out[3] = { makeImg( W, H, F16, true ), makeImg( W, H, F16, true ), makeImg( W, H, F16, true ) };
	Img * all[10] = { &iCur, &iHist, &iLock, &iMask, &iMv, &iVel, &iDepth, &out[0], &out[1], &out[2] };
	{
		VkCommandBuffer cb = newCb();
		VkCommandBufferBeginInfo bi { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
		vkBeginCommandBuffer( cb, &bi );
		for ( Img * x : all ) {
			VkImageMemoryBarrier ib { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
			ib.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
			ib.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			ib.newLayout = VK_IMAGE_LAYOUT_GENERAL;
			ib.srcQueueFamilyIndex = ib.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			ib.image = x->i;
			ib.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
			vkCmdPipelineBarrier( cb, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &ib );
		}
		vkEndCommandBuffer( cb );
		submitWait( cb );
	}
	Buf stage = makeBuf( static_cast<size_t>( W ) * H * 8 + 256 );
	auto upload = [&]( const Raw & d, Img & x ) {
		std::memcpy( stage.p, d.b.data(), d.b.size() );
		VkCommandBuffer cb = newCb();
		VkCommandBufferBeginInfo bi { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
		vkBeginCommandBuffer( cb, &bi );
		VkBufferImageCopy c {};
		c.imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
		c.imageExtent = { uint32_t( d.w ), uint32_t( d.h ), 1 };
		vkCmdCopyBufferToImage( cb, stage.b, x.i, VK_IMAGE_LAYOUT_GENERAL, 1, &c );
		vkEndCommandBuffer( cb );
		submitWait( cb );
	};
	upload( cur, iCur );
	upload( hist, iHist );
	upload( lock, iLock );
	upload( mask, iMask );
	upload( mv, iMv );
	upload( vel, iVel );
	upload( depth, iDepth );

	VkDescriptorSetAllocateInfo ai { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
	ai.descriptorPool = pool;
	ai.descriptorSetCount = 1;
	ai.pSetLayouts = &setLayout;
	VkDescriptorSet set;
	vkAllocateDescriptorSets( dev, &ai, &set );
	const VkImageView views[kBindings] = { iCur.v, iHist.v, iMv.v, iDepth.v, iLock.v, iVel.v, iMask.v, VK_NULL_HANDLE, out[0].v, out[1].v,
		out[2].v };
	VkDescriptorImageInfo ii[kBindings];
	VkWriteDescriptorSet wr[kBindings];
	for ( uint32_t i = 0; i < kBindings; i++ ) {
		ii[i] = i == 7 ? VkDescriptorImageInfo { linearClamp, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_UNDEFINED }
			: VkDescriptorImageInfo { VK_NULL_HANDLE, views[i], VK_IMAGE_LAYOUT_GENERAL };
		wr[i] = VkWriteDescriptorSet { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
		wr[i].dstSet = set;
		wr[i].dstBinding = i;
		wr[i].descriptorCount = 1;
		wr[i].descriptorType = bindingType( i );
		wr[i].pImageInfo = &ii[i];
	}
	vkUpdateDescriptorSets( dev, kBindings, wr, 0, nullptr );

	VkQueryPoolCreateInfo qpi { VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO };
	qpi.queryType = VK_QUERY_TYPE_TIMESTAMP;
	qpi.queryCount = 2;
	VkQueryPool qp;
	vkCreateQueryPool( dev, &qpi, nullptr, &qp );
	VkCommandBuffer run = newCb();
	{
		VkCommandBufferBeginInfo bi { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
		vkBeginCommandBuffer( run, &bi );
		vkCmdResetQueryPool( run, qp, 0, 2 );
		vkCmdWriteTimestamp( run, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, qp, 0 );
		vkCmdBindPipeline( run, VK_PIPELINE_BIND_POINT_COMPUTE, pipe );
		vkCmdBindDescriptorSets( run, VK_PIPELINE_BIND_POINT_COMPUTE, pipeLayout, 0, 1, &set, 0, nullptr );
		vkCmdPushConstants( run, pipeLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof( Job ), &job );
		int gx = 8, gy = 8;	// the kernel's numthreads; TAAMATCH_GROUP=<x>x<y> for a scratch variant
		if ( const char * g = std::getenv( "TAAMATCH_GROUP" ) )
			std::sscanf( g, "%dx%d", &gx, &gy );
		vkCmdDispatch( run, uint32_t( ( W + gx - 1 ) / gx ), uint32_t( ( H + gy - 1 ) / gy ), 1 );
		vkCmdWriteTimestamp( run, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, qp, 1 );
		vkEndCommandBuffer( run );
	}
	submitWait( run );

	Buf rb = makeBuf( static_cast<size_t>( W ) * H * 8 + 256 );
	size_t diffAll = 0, valuesAll = 0, bigAll = 0;
	int maxUlp = 0;
	bool filesOk = true;
	for ( int o = 0; o < 3; o++ ) {
		Raw g;
		const std::string file = "gl_o" + std::to_string( o ) + ".bin";
		if ( !readRaw( dir + "/" + file, g, 8 ) || g.w != W || g.h != H ) {
			std::printf( "%s: missing or wrong size\n", file.c_str() );
			filesOk = false;
			continue;
		}
		VkCommandBuffer cb = newCb();
		VkCommandBufferBeginInfo bi { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
		vkBeginCommandBuffer( cb, &bi );
		VkBufferImageCopy c {};
		c.imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
		c.imageExtent = { uint32_t( W ), uint32_t( H ), 1 };
		vkCmdCopyImageToBuffer( cb, out[o].i, VK_IMAGE_LAYOUT_GENERAL, rb.b, 1, &c );
		vkEndCommandBuffer( cb );
		submitWait( cb );
		const uint16_t * v = static_cast<const uint16_t *>( rb.p );
		const uint16_t * gp = reinterpret_cast<const uint16_t *>( g.b.data() );
		const size_t n = static_cast<size_t>( W ) * H * 4;
		size_t d = 0, big = 0, nz = 0;
		int mu = 0, firstBad = -1;
		for ( size_t k = 0; k < n; k++ ) {
			const int u = ulps( v[k], gp[k] );
			d += u != 0;
			big += u > 1;
			if ( u > 1 && firstBad < 0 )
				firstBad = int( k );
			mu = std::max( mu, u );
			nz += ( gp[k] & 0x7FFF ) != 0;
		}
		diffAll += d;
		bigAll += big;
		valuesAll += n;
		maxUlp = std::max( maxUlp, mu );
		std::printf( "o%d %dx%d: %zu of %zu halves differ, %zu by > 1 ulp, max %d ulp (GL non-zero %zu)", o, W, H, d, n, big, mu, nz );
		if ( firstBad >= 0 )
			std::printf( "; first > 1 ulp at x %d y %d ch %d: vk 0x%04x gl 0x%04x", ( firstBad / 4 ) % W, ( firstBad / 4 ) / W, firstBad % 4,
				v[firstBad], gp[firstBad] );
		std::printf( "\n" );
	}

	// 0.5 s of resolves first: after the CPU compare the GPU had clocked down and read 0.31 ms for a 0.16 ms resolve
	for ( const DWORD t0 = GetTickCount(); GetTickCount() - t0 < 500; )
		submitWait( run );
	std::vector<double> ms;
	for ( int r = 0; r < 34; r++ ) {
		submitWait( run );
		uint64_t ts[2] = {};
		vkGetQueryPoolResults( dev, qp, 0, 2, sizeof( ts ), ts, 8, VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT );
		if ( r >= 2 )
			ms.push_back( double( ts[1] - ts[0] ) * props.limits.timestampPeriod / 1e6 );
	}
	std::sort( ms.begin(), ms.end() );
	const double vkMs = ( ms[15] + ms[16] ) * 0.5;
	double glMs = -1.0;
	if ( FILE * f = std::fopen( ( dir + "/gl_ms.txt" ).c_str(), "r" ) ) {
		if ( std::fscanf( f, "%lf", &glMs ) != 1 )
			glMs = -1.0;
		std::fclose( f );
	}
	if ( glMs > 0.0 )
		std::printf( "time %s: Vulkan resolve %.4f ms, GL resolve %.4f ms: %+.4f ms (%+.1f %%) [bar: slower by <= 0.1 ms or <= 5 %%]\n",
			name.c_str(), vkMs, glMs, vkMs - glMs, ( vkMs - glMs ) / glMs * 100.0 );
	else
		std::printf( "time %s: Vulkan resolve %.4f ms\n", name.c_str(), vkMs );

	if ( !filesOk ) {
		std::printf( "TAAMATCH %s FAIL (gl files)\n", name.c_str() );
		return 1;
	}
	if ( diffAll == 0 ) {
		std::printf( "TAAMATCH %s EXACT (%zu halves)\n", name.c_str(), valuesAll );
		return 0;
	}
	if ( maxUlp <= 1 ) {
		std::printf( "TAAMATCH %s BOUND (%zu of %zu halves at 1 fp16 ulp)\n", name.c_str(), diffAll, valuesAll );
		return 0;
	}
	/* Counted rule: the history taps sample through the bilinear unit (8-bit sub-texel weights) at a computed uv, and the
	 * resolve has hard steps (lock sign at |dY| 0.02, keep smoothsteps, the disocclusion test): a last-bit uv or float-order
	 * difference flips a weight step or a branch on a few pixels. Bound: <= 0.5 % of halves differ at all and <= 0.1 % by
	 * more than 1 ulp (the red TAA_RESOLVE_RED: 1.4 % by more than 1 ulp). */
	if ( diffAll * 200 <= valuesAll && bigAll * 1000 <= valuesAll ) {
		std::printf( "TAAMATCH %s BOUND (step: %zu of %zu halves differ, %zu by > 1 ulp at sampler-weight / branch steps, max %d ulp)\n",
			name.c_str(), diffAll, valuesAll, bigAll, maxUlp );
		return 0;
	}
	std::printf( "TAAMATCH %s FAIL (%zu of %zu halves differ, %zu by > 1 ulp, max %d ulp)\n", name.c_str(), diffAll, valuesAll, bigAll,
		maxUlp );
	return 1;
}
