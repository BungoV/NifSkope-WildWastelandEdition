// Lane VKFRAME1 (2026-11-12, item 3): the bloom chain on Vulkan from the Fusion Shaders HLSL (res/shaders/hlsl/post:
// bloom_down / bloom_hold / bloom_up over BloomCommon.hlsli) against the GL chain's own output (tests/vkframe/bloom_gl_ref.py
// writes the job folders), every level's half-float bits compared. No NifSkope: a headless Vulkan device of its own (the
// compute-only family when there is one, as bakert's), RGBA16F images in GENERAL layout, the LinearClamp sampler, one
// descriptor set a dispatch and a compute -> compute barrier between dispatches (vkbloom.cpp records the same chain).
// Usage: bloommatch <job dir> <down.spv> <hold.spv> <up.spv>
// Prints one line a level, the chain's GPU ms (timestamps, median of 32 after 2 warm-ups), then
// `BLOOMMATCH <job> EXACT`, `BLOOMMATCH <job> BOUND (max 1 fp16 ulp: fp16 rounding of a float-order difference)` or
// `BLOOMMATCH <job> FAIL ...`. Exit 0 for EXACT or BOUND.
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
	v.resize( size_t( n ) / 4 );
	if ( std::fread( v.data(), 4, v.size(), f ) != v.size() )
		v.clear();
	std::fclose( f );
	return v;
}

// int32 W, H (and a layer count for the ring), then uint16 RGBA
struct Half { int w = 0, h = 0, layers = 1; std::vector<uint16_t> px; };

static bool readHalf( const std::string & path, Half & out, bool ring = false )
{
	FILE * f = std::fopen( path.c_str(), "rb" );
	if ( !f )
		return false;
	int32_t hdr[3] = { 0, 0, 1 };
	bool ok = std::fread( hdr, 4, ring ? 3 : 2, f ) == size_t( ring ? 3 : 2 );
	out.w = hdr[0];
	out.h = hdr[1];
	out.layers = ring ? hdr[2] : 1;
	out.px.resize( size_t( out.w ) * out.h * out.layers * 4 );
	ok = ok && std::fread( out.px.data(), 2, out.px.size(), f ) == out.px.size();
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

// one RGBA16F image (vkbloom.cpp's pyramid: sampled + storage, GENERAL), its whole view and, for the ring, one view a layer
struct Img { VkImage i = VK_NULL_HANDLE; VkDeviceMemory m = VK_NULL_HANDLE; VkImageView all = VK_NULL_HANDLE; VkImageView layer[8] = {};
	int w = 0, h = 0, layers = 1; };

static Img makeImg( int w, int h, int layers )
{
	Img I;
	I.w = w;
	I.h = h;
	I.layers = layers;
	VkImageCreateInfo ci { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
	ci.imageType = VK_IMAGE_TYPE_2D;
	ci.format = VK_FORMAT_R16G16B16A16_SFLOAT;
	ci.extent = { uint32_t( w ), uint32_t( h ), 1 };
	ci.mipLevels = 1;
	ci.arrayLayers = uint32_t( layers );
	ci.samples = VK_SAMPLE_COUNT_1_BIT;
	ci.tiling = VK_IMAGE_TILING_OPTIMAL;
	ci.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
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
	vi.viewType = layers > 1 ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D;
	vi.format = ci.format;
	vi.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, uint32_t( layers ) };
	vkCreateImageView( dev, &vi, nullptr, &I.all );
	if ( layers > 1 )
		for ( int l = 0; l < layers && l < 8; l++ ) {
			vi.viewType = VK_IMAGE_VIEW_TYPE_2D;
			vi.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, uint32_t( l ), 1 };
			vkCreateImageView( dev, &vi, nullptr, &I.layer[l] );
		}
	return I;
}

static VkDescriptorSetLayout setLayout;
static VkPipelineLayout pipeLayout;
static VkDescriptorPool pool;
static VkCommandPool cpool;
static VkSampler linearClamp;

// BloomBindingsVk.hlsli: 0 BloomSource, 1 BloomMip, 2 BloomRing (sampled), 3 BloomLinearClamp, 4 BloomTarget (storage)
static bool makeLayout()
{
	VkDescriptorSetLayoutBinding b[5];
	const VkDescriptorType t[5] = { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
		VK_DESCRIPTOR_TYPE_SAMPLER, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE };
	for ( uint32_t i = 0; i < 5; i++ )
		b[i] = VkDescriptorSetLayoutBinding { i, t[i], 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr };
	VkDescriptorSetLayoutCreateInfo li { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
	li.bindingCount = 5;
	li.pBindings = b;
	if ( vkCreateDescriptorSetLayout( dev, &li, nullptr, &setLayout ) != VK_SUCCESS )
		return false;
	VkPushConstantRange pr { VK_SHADER_STAGE_COMPUTE_BIT, 0, 48 };
	VkPipelineLayoutCreateInfo pi { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
	pi.setLayoutCount = 1;
	pi.pSetLayouts = &setLayout;
	pi.pushConstantRangeCount = 1;
	pi.pPushConstantRanges = &pr;
	if ( vkCreatePipelineLayout( dev, &pi, nullptr, &pipeLayout ) != VK_SUCCESS )
		return false;
	VkDescriptorPoolSize ps[3] = { { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 3 * 32 }, { VK_DESCRIPTOR_TYPE_SAMPLER, 32 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 32 } };
	VkDescriptorPoolCreateInfo dp { VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
	dp.maxSets = 32;
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

static void barrier( VkCommandBuffer cb, VkPipelineStageFlags src, VkAccessFlags sa, VkPipelineStageFlags dst, VkAccessFlags da )
{
	VkMemoryBarrier mb { VK_STRUCTURE_TYPE_MEMORY_BARRIER };
	mb.srcAccessMask = sa;
	mb.dstAccessMask = da;
	vkCmdPipelineBarrier( cb, src, dst, 0, 1, &mb, 0, nullptr, 0, nullptr );
}

// BloomBindingsVk.hlsli BloomJob
struct Job { float params[4]; float spare[4]; int32_t ints[4]; };

struct Arm { int hold = 0, k = 0, n = 1, red = 0, mips = 0; float thr = 1, knee = 0.5f, radius = 1, disp = 0; float sw[8] = {}, aw[8] = {}; };

static bool readJob( const std::string & path, Arm & a )
{
	FILE * f = std::fopen( path.c_str(), "r" );
	if ( !f )
		return false;
	char key[32];
	while ( std::fscanf( f, "%31s", key ) == 1 ) {
		const std::string k = key;
		if ( k == "hold" ) std::fscanf( f, "%d", &a.hold );
		else if ( k == "k" ) std::fscanf( f, "%d", &a.k );
		else if ( k == "n" ) std::fscanf( f, "%d", &a.n );
		else if ( k == "red" ) std::fscanf( f, "%d", &a.red );
		else if ( k == "thr" ) std::fscanf( f, "%f", &a.thr );
		else if ( k == "knee" ) std::fscanf( f, "%f", &a.knee );
		else if ( k == "radius" ) std::fscanf( f, "%f", &a.radius );
		else if ( k == "disp" ) std::fscanf( f, "%f", &a.disp );
		else if ( k == "mips" ) std::fscanf( f, "%d", &a.mips );
		else if ( k.size() > 1 && k[0] == 'w' ) {
			const int l = std::atoi( k.c_str() + 1 );
			if ( l >= 0 && l < 8 )
				std::fscanf( f, "%f %f", &a.sw[l], &a.aw[l] );
		}
	}
	std::fclose( f );
	return a.mips > 0 && a.mips <= 8;
}

// the dispatch: one descriptor set (source, mip, ring, sampler, target), the push constants, ceil(size / 8) groups
static void dispatch( VkCommandBuffer cb, VkPipeline p, VkImageView src, VkImageView mip, VkImageView ring, VkImageView dst, int w, int h,
	const Job & j )
{
	VkDescriptorSetAllocateInfo ai { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
	ai.descriptorPool = pool;
	ai.descriptorSetCount = 1;
	ai.pSetLayouts = &setLayout;
	VkDescriptorSet set;
	vkAllocateDescriptorSets( dev, &ai, &set );
	VkDescriptorImageInfo ii[5] = { { VK_NULL_HANDLE, src, VK_IMAGE_LAYOUT_GENERAL }, { VK_NULL_HANDLE, mip, VK_IMAGE_LAYOUT_GENERAL },
		{ VK_NULL_HANDLE, ring, VK_IMAGE_LAYOUT_GENERAL }, { linearClamp, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_UNDEFINED },
		{ VK_NULL_HANDLE, dst, VK_IMAGE_LAYOUT_GENERAL } };
	const VkDescriptorType t[5] = { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
		VK_DESCRIPTOR_TYPE_SAMPLER, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE };
	VkWriteDescriptorSet wr[5];
	for ( uint32_t i = 0; i < 5; i++ ) {
		wr[i] = VkWriteDescriptorSet { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
		wr[i].dstSet = set;
		wr[i].dstBinding = i;
		wr[i].descriptorCount = 1;
		wr[i].descriptorType = t[i];
		wr[i].pImageInfo = &ii[i];
	}
	vkUpdateDescriptorSets( dev, 5, wr, 0, nullptr );
	vkCmdBindPipeline( cb, VK_PIPELINE_BIND_POINT_COMPUTE, p );
	vkCmdBindDescriptorSets( cb, VK_PIPELINE_BIND_POINT_COMPUTE, pipeLayout, 0, 1, &set, 0, nullptr );
	vkCmdPushConstants( cb, pipeLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof( Job ), &j );
	vkCmdDispatch( cb, uint32_t( ( w + 7 ) / 8 ), uint32_t( ( h + 7 ) / 8 ), 1 );
	barrier( cb, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
		VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT );
}

// the distance of two halves in representable steps (sign-magnitude to an ordered integer); NaN against anything = big
static int ulps( uint16_t a, uint16_t b )
{
	auto ord = []( uint16_t x ) { return ( x & 0x8000 ) ? -int( x & 0x7FFF ) : int( x ); };
	auto nan = []( uint16_t x ) { return ( x & 0x7C00 ) == 0x7C00 && ( x & 0x03FF ); };
	if ( nan( a ) || nan( b ) )
		return a == b ? 0 : 1 << 20;
	return std::abs( ord( a ) - ord( b ) );
}

int main( int argc, char ** argv )
{
	std::setvbuf( stdout, nullptr, _IONBF, 0 );
	if ( argc < 5 ) {
		std::printf( "usage: bloommatch <job dir> <down.spv> <hold.spv> <up.spv>\n" );
		return 2;
	}
	const std::string dir = argv[1];
	std::string name = dir;
	while ( !name.empty() && ( name.back() == '/' || name.back() == '\\' ) )
		name.pop_back();
	name = name.substr( name.find_last_of( "/\\" ) + 1 );
	Arm a;
	Half src, ringIn;
	if ( !readJob( dir + "/job.txt", a ) || !readHalf( dir + "/src.bin", src ) || ( a.hold && !readHalf( dir + "/ring.bin", ringIn, true ) ) ) {
		std::printf( "BLOOMMATCH %s FAIL (job files)\n", name.c_str() );
		return 2;
	}
	const std::vector<uint32_t> spvDown = readSpv( argv[2] ), spvHold = readSpv( argv[3] ), spvUp = readSpv( argv[4] );
	if ( spvDown.empty() || spvHold.empty() || spvUp.empty() ) {
		std::printf( "BLOOMMATCH %s FAIL (spv)\n", name.c_str() );
		return 2;
	}
	HMODULE lib = LoadLibraryA( "vulkan-1.dll" );
	if ( !lib ) {
		std::printf( "BLOOMMATCH %s FAIL (no vulkan-1.dll)\n", name.c_str() );
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
		std::printf( "BLOOMMATCH %s FAIL (instance)\n", name.c_str() );
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
		std::printf( "BLOOMMATCH %s FAIL (no device)\n", name.c_str() );
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
	for ( uint32_t i = 0; i < nq; i++ )	// the compute-only family with timestamps (vkasyncrule.h pickFamily's choice)
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
		std::printf( "BLOOMMATCH %s FAIL (device)\n", name.c_str() );
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
		std::printf( "BLOOMMATCH %s FAIL (layout)\n", name.c_str() );
		return 2;
	}
	const VkPipeline pDown = makePipe( spvDown ), pHold = makePipe( spvHold ), pUp = makePipe( spvUp );
	if ( !pDown || !pHold || !pUp ) {
		std::printf( "BLOOMMATCH %s FAIL (pipelines)\n", name.c_str() );
		return 2;
	}
	std::printf( "device %s, queue family %u (%s)\n", props.deviceName, family,
		( qf[family].queueFlags & VK_QUEUE_GRAPHICS_BIT ) ? "graphics" : "compute-only" );

	// the pyramid: wwBloomPlanMips' ceil halving, as the GL reference
	int dims[8][2];
	int w = src.w, h = src.h;
	for ( int i = 0; i < a.mips; i++ ) {
		w = std::max( ( w + 1 ) / 2, 1 );
		h = std::max( ( h + 1 ) / 2, 1 );
		dims[i][0] = w;
		dims[i][1] = h;
	}
	Img S = makeImg( src.w, src.h, 1 );
	Img R = makeImg( dims[0][0], dims[0][1], a.hold ? 8 : 2 );	// 2 layers when unused: an array view for binding 2
	std::vector<Img> M, Up;
	for ( int i = 0; i < a.mips; i++ ) {
		M.push_back( makeImg( dims[i][0], dims[i][1], 1 ) );
		if ( i + 1 < a.mips )
			Up.push_back( makeImg( dims[i][0], dims[i][1], 1 ) );
	}
	std::vector<Img *> all = { &S, &R };
	for ( Img & x : M )
		all.push_back( &x );
	for ( Img & x : Up )
		all.push_back( &x );

	// upload: every image to GENERAL, the source and the ring from staging
	Buf stage = makeBuf( std::max( src.px.size(), ringIn.px.size() ) * 2 + 256 );
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
			ib.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, uint32_t( x->layers ) };
			vkCmdPipelineBarrier( cb, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &ib );
		}
		vkEndCommandBuffer( cb );
		submitWait( cb );
	}
	auto upload = [&]( const Half & d, Img & x ) {
		std::memcpy( stage.p, d.px.data(), d.px.size() * 2 );
		VkCommandBuffer cb = newCb();
		VkCommandBufferBeginInfo bi { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
		vkBeginCommandBuffer( cb, &bi );
		VkBufferImageCopy c {};
		c.imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, uint32_t( d.layers ) };
		c.imageExtent = { uint32_t( d.w ), uint32_t( d.h ), 1 };
		vkCmdCopyBufferToImage( cb, stage.b, x.i, VK_IMAGE_LAYOUT_GENERAL, 1, &c );
		vkEndCommandBuffer( cb );
		submitWait( cb );
	};
	upload( src, S );
	if ( a.hold )
		upload( ringIn, R );

	// the chain, recorded once (timestamps at both ends), as vkbloom.cpp records it
	VkQueryPoolCreateInfo qpi { VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO };
	qpi.queryType = VK_QUERY_TYPE_TIMESTAMP;
	qpi.queryCount = 2;
	VkQueryPool qp;
	vkCreateQueryPool( dev, &qpi, nullptr, &qp );
	VkCommandBuffer chain = newCb();
	{
		VkCommandBufferBeginInfo bi { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
		vkBeginCommandBuffer( chain, &bi );
		vkCmdResetQueryPool( chain, qp, 0, 2 );
		vkCmdWriteTimestamp( chain, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, qp, 0 );
		for ( int i = 0; i < a.mips; i++ ) {
			Job j {};
			j.params[0] = a.thr;
			j.params[1] = a.knee;
			j.params[2] = i == 0 ? 1.0f : 0.0f;
			j.params[3] = 1.0f;
			j.ints[0] = a.red;
			const VkImageView in = i == 0 ? S.all : M[i - 1].all;
			const VkImageView out = ( i == 0 && a.hold ) ? R.layer[a.k & 7] : M[i].all;
			dispatch( chain, pDown, in, in, R.all, out, dims[i][0], dims[i][1], j );
			if ( i == 0 && a.hold ) {
				Job hj {};
				hj.ints[1] = a.k;
				hj.ints[2] = a.n;
				dispatch( chain, pHold, S.all, S.all, R.all, M[0].all, dims[0][0], dims[0][1], hj );
			}
		}
		VkImageView acc = M[a.mips - 1].all;
		for ( int off = a.mips - 1; off > 0; off-- ) {
			const int lv = off - 1;
			Job j {};
			j.params[0] = a.radius;
			j.params[1] = a.sw[lv];
			j.params[2] = a.aw[lv];
			j.params[3] = a.disp;
			dispatch( chain, pUp, M[lv].all, acc, R.all, Up[lv].all, dims[lv][0], dims[lv][1], j );
			acc = Up[lv].all;
		}
		vkCmdWriteTimestamp( chain, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, qp, 1 );
		vkEndCommandBuffer( chain );
	}
	submitWait( chain );

	// read back and compare with the GL chain's levels
	Buf rb = makeBuf( size_t( dims[0][0] ) * dims[0][1] * 8 + 256 );
	auto readImg = [&]( Img & x, std::vector<uint16_t> & out ) {
		VkCommandBuffer cb = newCb();
		VkCommandBufferBeginInfo bi { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
		vkBeginCommandBuffer( cb, &bi );
		VkBufferImageCopy c {};
		c.imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
		c.imageExtent = { uint32_t( x.w ), uint32_t( x.h ), 1 };
		vkCmdCopyImageToBuffer( cb, x.i, VK_IMAGE_LAYOUT_GENERAL, rb.b, 1, &c );
		vkEndCommandBuffer( cb );
		submitWait( cb );
		out.resize( size_t( x.w ) * x.h * 4 );
		std::memcpy( out.data(), rb.p, out.size() * 2 );
	};
	size_t diffAll = 0, valuesAll = 0;
	int maxUlp = 0;
	bool filesOk = true;
	auto compare = [&]( Img & x, const std::string & file ) {
		Half g;
		if ( !readHalf( dir + "/" + file, g ) || g.w != x.w || g.h != x.h ) {
			std::printf( "%s: missing or wrong size\n", file.c_str() );
			filesOk = false;
			return;
		}
		std::vector<uint16_t> v;
		readImg( x, v );
		size_t d = 0, nz = 0;
		int mu = 0;
		for ( size_t k = 0; k < v.size(); k++ ) {
			if ( ( k & 3 ) == 3 )
				continue;	// alpha: both write 1
			const int u = ulps( v[k], g.px[k] );
			d += u != 0;
			mu = std::max( mu, u );
			nz += ( g.px[k] & 0x7FFF ) != 0;
		}
		diffAll += d;
		valuesAll += v.size() / 4 * 3;
		maxUlp = std::max( maxUlp, mu );
		std::printf( "%s %dx%d: %zu of %zu rgb halves differ, max %d ulp (GL non-zero %zu)\n", file.c_str(), x.w, x.h, d, v.size() / 4 * 3, mu, nz );
	};
	for ( int i = 0; i < a.mips; i++ ) {
		compare( M[i], "gl_mip" + std::to_string( i ) + ".bin" );
		if ( i + 1 < a.mips )
			compare( Up[i], "gl_up" + std::to_string( i ) + ".bin" );
	}

	// time the chain: median of 32 submits after 2 warm-ups
	std::vector<double> ms;
	for ( int r = 0; r < 34; r++ ) {
		submitWait( chain );
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
		std::printf( "time %s: Vulkan chain %.4f ms, GL chain %.4f ms: %+.4f ms (%+.1f %%) [bar: slower by <= 0.1 ms or <= 5 %%]\n",
			name.c_str(), vkMs, glMs, vkMs - glMs, ( vkMs - glMs ) / glMs * 100.0 );
	else
		std::printf( "time %s: Vulkan chain %.4f ms\n", name.c_str(), vkMs );

	if ( !filesOk ) {
		std::printf( "BLOOMMATCH %s FAIL (gl files)\n", name.c_str() );
		return 1;
	}
	if ( diffAll == 0 ) {
		std::printf( "BLOOMMATCH %s EXACT (%zu rgb halves)\n", name.c_str(), valuesAll );
		return 0;
	}
	if ( maxUlp <= 1 ) {
		std::printf( "BLOOMMATCH %s BOUND (%zu of %zu halves at 1 fp16 ulp: fp16 rounding of a float-order difference)\n", name.c_str(),
			diffAll, valuesAll );
		return 0;
	}
	std::printf( "BLOOMMATCH %s FAIL (%zu of %zu halves differ, max %d ulp)\n", name.c_str(), diffAll, valuesAll, maxUlp );
	return 1;
}
