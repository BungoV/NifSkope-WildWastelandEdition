// lane HLSLSRC3 (2026-11-13): headless Vulkan byte compare of the GPU cull kernels (cellgpucull_hiz0 / _hizn / _cull) against
// their HLSL twins. Run by tests/hlslmatch/cullmatch.sh:
//   cullmatch <hiz0|hiz0ms|hizn|cull|bc7> <glsl.spv> <hlsl.spv>   (bc7: lane FO4MATCH1, the far-LOD BC7 encoder)
// Both modules get the same random inputs, the same prefilled outputs and the same std140 uniform bytes (binding 31), case
// after case; every watched buffer must come back byte-identical. The GLSL run must also change its output (a kernel that
// writes nothing would match a red). Exit 0 match, 1 mismatch, 2 the GLSL wrote nothing, 3 setup error.
// The depth image (hiz0) is filled by fill.spv / fillms.spv (tests/hlslmatch/cullfill.comp) next to the exe's cwd.
#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include <windows.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#define VKF( name ) static PFN_##name name
VKF( vkCreateInstance ); VKF( vkEnumeratePhysicalDevices ); VKF( vkGetPhysicalDeviceProperties );
VKF( vkGetPhysicalDeviceQueueFamilyProperties ); VKF( vkGetPhysicalDeviceMemoryProperties ); VKF( vkGetPhysicalDeviceFeatures );
VKF( vkGetPhysicalDeviceImageFormatProperties ); VKF( vkCreateDevice ); VKF( vkGetDeviceQueue ); VKF( vkCreateBuffer );
VKF( vkGetBufferMemoryRequirements ); VKF( vkAllocateMemory ); VKF( vkBindBufferMemory ); VKF( vkMapMemory );
VKF( vkCreateImage ); VKF( vkGetImageMemoryRequirements ); VKF( vkBindImageMemory ); VKF( vkCreateImageView );
VKF( vkCreateSampler ); VKF( vkCreateShaderModule ); VKF( vkDestroyShaderModule ); VKF( vkCreateDescriptorSetLayout );
VKF( vkDestroyDescriptorSetLayout ); VKF( vkCreatePipelineLayout ); VKF( vkDestroyPipelineLayout );
VKF( vkCreateComputePipelines ); VKF( vkDestroyPipeline ); VKF( vkCreateDescriptorPool ); VKF( vkDestroyDescriptorPool );
VKF( vkAllocateDescriptorSets ); VKF( vkUpdateDescriptorSets ); VKF( vkCreateCommandPool ); VKF( vkAllocateCommandBuffers );
VKF( vkFreeCommandBuffers ); VKF( vkBeginCommandBuffer ); VKF( vkEndCommandBuffer ); VKF( vkCmdBindPipeline );
VKF( vkCmdBindDescriptorSets ); VKF( vkCmdPushConstants ); VKF( vkCmdDispatch ); VKF( vkCmdPipelineBarrier );
VKF( vkQueueSubmit ); VKF( vkCreateFence ); VKF( vkDestroyFence ); VKF( vkWaitForFences );

static VkPhysicalDevice gpu;
static VkDevice dev;
static VkQueue queue;
static VkCommandPool cpool;
static VkPhysicalDeviceMemoryProperties memProps;
static bool msStorage = false;

static void die( const char * what )
{
	printf( "setup error: %s\n", what );
	exit( 3 );
}
#define CK( call ) do { if ( ( call ) != VK_SUCCESS ) die( #call ); } while ( 0 )

static uint32_t memType( uint32_t bits, VkMemoryPropertyFlags want )
{
	for ( uint32_t i = 0; i < memProps.memoryTypeCount; i++ )
		if ( ( bits & ( 1u << i ) ) && ( memProps.memoryTypes[i].propertyFlags & want ) == want )
			return i;
	die( "no memory type" );
	return 0;
}

struct Buf { VkBuffer b; void * p; size_t n; };
static Buf makeBuf( size_t n, VkBufferUsageFlags use )
{
	Buf r{};
	r.n = n;
	VkBufferCreateInfo bi{ VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
	bi.size = n;
	bi.usage = use;
	CK( vkCreateBuffer( dev, &bi, nullptr, &r.b ) );
	VkMemoryRequirements mr;
	vkGetBufferMemoryRequirements( dev, r.b, &mr );
	VkMemoryAllocateInfo ai{ VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
	ai.allocationSize = mr.size;
	ai.memoryTypeIndex = memType( mr.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT );
	VkDeviceMemory m;
	CK( vkAllocateMemory( dev, &ai, nullptr, &m ) );
	CK( vkBindBufferMemory( dev, r.b, m, 0 ) );
	CK( vkMapMemory( dev, m, 0, VK_WHOLE_SIZE, 0, &r.p ) );
	return r;
}
static Buf ssbo( size_t n ) { return makeBuf( n, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT ); }

struct Img { VkImage i; VkImageView v; };
static Img makeImg( int w, int h, int samples )
{
	Img r{};
	VkImageCreateInfo ii{ VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
	ii.imageType = VK_IMAGE_TYPE_2D;
	ii.format = VK_FORMAT_R32_SFLOAT;
	ii.extent = { uint32_t( w ), uint32_t( h ), 1 };
	ii.mipLevels = 1;
	ii.arrayLayers = 1;
	ii.samples = VkSampleCountFlagBits( samples );
	ii.tiling = VK_IMAGE_TILING_OPTIMAL;
	ii.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	CK( vkCreateImage( dev, &ii, nullptr, &r.i ) );
	VkMemoryRequirements mr;
	vkGetImageMemoryRequirements( dev, r.i, &mr );
	VkMemoryAllocateInfo ai{ VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
	ai.allocationSize = mr.size;
	ai.memoryTypeIndex = memType( mr.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );
	VkDeviceMemory m;
	CK( vkAllocateMemory( dev, &ai, nullptr, &m ) );
	CK( vkBindImageMemory( dev, r.i, m, 0 ) );
	VkImageViewCreateInfo vi{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
	vi.image = r.i;
	vi.viewType = VK_IMAGE_VIEW_TYPE_2D;
	vi.format = VK_FORMAT_R32_SFLOAT;
	vi.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
	CK( vkCreateImageView( dev, &vi, nullptr, &r.v ) );
	return r;
}

// one descriptor: a buffer, or the depth image with the descriptor type this module wants
struct Bind { uint32_t binding; VkDescriptorType type; VkBuffer buf; VkImageView view; };
static VkSampler sampler;

// one dispatch of one module; an image binding in a fresh layout goes UNDEFINED -> GENERAL first when asked
static void run( const std::vector<uint32_t> & spv, const std::vector<Bind> & binds, const void * push, uint32_t pushN,
	uint32_t gx, uint32_t gy, VkImage initImg = VK_NULL_HANDLE )
{
	VkShaderModuleCreateInfo si{ VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
	si.codeSize = spv.size() * 4;
	si.pCode = spv.data();
	VkShaderModule mod;
	CK( vkCreateShaderModule( dev, &si, nullptr, &mod ) );
	std::vector<VkDescriptorSetLayoutBinding> lb;
	std::vector<VkDescriptorPoolSize> ps;
	for ( const Bind & b : binds ) {
		VkDescriptorSetLayoutBinding x{};
		x.binding = b.binding;
		x.descriptorType = b.type;
		x.descriptorCount = 1;
		x.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
		lb.push_back( x );
		ps.push_back( { b.type, 1 } );
	}
	VkDescriptorSetLayoutCreateInfo li{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
	li.bindingCount = uint32_t( lb.size() );
	li.pBindings = lb.data();
	VkDescriptorSetLayout dsl;
	CK( vkCreateDescriptorSetLayout( dev, &li, nullptr, &dsl ) );
	VkPushConstantRange pr{ VK_SHADER_STAGE_COMPUTE_BIT, 0, pushN };
	VkPipelineLayoutCreateInfo pli{ VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
	pli.setLayoutCount = 1;
	pli.pSetLayouts = &dsl;
	pli.pushConstantRangeCount = pushN ? 1 : 0;
	pli.pPushConstantRanges = &pr;
	VkPipelineLayout pl;
	CK( vkCreatePipelineLayout( dev, &pli, nullptr, &pl ) );
	VkComputePipelineCreateInfo ci{ VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO };
	ci.stage = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
	ci.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	ci.stage.module = mod;
	ci.stage.pName = "main";
	ci.layout = pl;
	VkPipeline pipe;
	CK( vkCreateComputePipelines( dev, VK_NULL_HANDLE, 1, &ci, nullptr, &pipe ) );
	VkDescriptorPoolCreateInfo pi{ VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
	pi.maxSets = 1;
	pi.poolSizeCount = uint32_t( ps.size() );
	pi.pPoolSizes = ps.data();
	VkDescriptorPool pool;
	CK( vkCreateDescriptorPool( dev, &pi, nullptr, &pool ) );
	VkDescriptorSetAllocateInfo ai{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
	ai.descriptorPool = pool;
	ai.descriptorSetCount = 1;
	ai.pSetLayouts = &dsl;
	VkDescriptorSet set;
	CK( vkAllocateDescriptorSets( dev, &ai, &set ) );
	std::vector<VkDescriptorBufferInfo> bis( binds.size() );
	std::vector<VkDescriptorImageInfo> iis( binds.size() );
	std::vector<VkWriteDescriptorSet> ws;
	for ( size_t i = 0; i < binds.size(); i++ ) {
		VkWriteDescriptorSet w{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
		w.dstSet = set;
		w.dstBinding = binds[i].binding;
		w.descriptorCount = 1;
		w.descriptorType = binds[i].type;
		if ( binds[i].view ) {
			iis[i] = { sampler, binds[i].view, VK_IMAGE_LAYOUT_GENERAL };
			w.pImageInfo = &iis[i];
		} else {
			bis[i] = { binds[i].buf, 0, VK_WHOLE_SIZE };
			w.pBufferInfo = &bis[i];
		}
		ws.push_back( w );
	}
	vkUpdateDescriptorSets( dev, uint32_t( ws.size() ), ws.data(), 0, nullptr );
	VkCommandBufferAllocateInfo cai{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
	cai.commandPool = cpool;
	cai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	cai.commandBufferCount = 1;
	VkCommandBuffer cb;
	CK( vkAllocateCommandBuffers( dev, &cai, &cb ) );
	VkCommandBufferBeginInfo bi{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
	bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	CK( vkBeginCommandBuffer( cb, &bi ) );
	VkMemoryBarrier all{ VK_STRUCTURE_TYPE_MEMORY_BARRIER };
	all.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;
	all.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
	if ( initImg ) {
		VkImageMemoryBarrier ib{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
		ib.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
		ib.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		ib.newLayout = VK_IMAGE_LAYOUT_GENERAL;
		ib.srcQueueFamilyIndex = ib.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		ib.image = initImg;
		ib.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
		vkCmdPipelineBarrier( cb, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 1, &all, 0, nullptr, 1, &ib );
	} else
		vkCmdPipelineBarrier( cb, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 1, &all, 0, nullptr, 0, nullptr );
	vkCmdBindPipeline( cb, VK_PIPELINE_BIND_POINT_COMPUTE, pipe );
	vkCmdBindDescriptorSets( cb, VK_PIPELINE_BIND_POINT_COMPUTE, pl, 0, 1, &set, 0, nullptr );
	if ( pushN )
		vkCmdPushConstants( cb, pl, VK_SHADER_STAGE_COMPUTE_BIT, 0, pushN, push );
	vkCmdDispatch( cb, gx, gy, 1 );
	VkMemoryBarrier host{ VK_STRUCTURE_TYPE_MEMORY_BARRIER };
	host.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
	host.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
	vkCmdPipelineBarrier( cb, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0, 1, &host, 0, nullptr, 0, nullptr );
	CK( vkEndCommandBuffer( cb ) );
	VkFenceCreateInfo fi{ VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
	VkFence fence;
	CK( vkCreateFence( dev, &fi, nullptr, &fence ) );
	VkSubmitInfo sub{ VK_STRUCTURE_TYPE_SUBMIT_INFO };
	sub.commandBufferCount = 1;
	sub.pCommandBuffers = &cb;
	CK( vkQueueSubmit( queue, 1, &sub, fence ) );
	if ( vkWaitForFences( dev, 1, &fence, VK_TRUE, 20000000000ull ) != VK_SUCCESS )
		die( "dispatch did not finish in 20 s" );
	vkDestroyFence( dev, fence, nullptr );
	vkFreeCommandBuffers( dev, cpool, 1, &cb );
	vkDestroyDescriptorPool( dev, pool, nullptr );
	vkDestroyPipeline( dev, pipe, nullptr );
	vkDestroyPipelineLayout( dev, pl, nullptr );
	vkDestroyDescriptorSetLayout( dev, dsl, nullptr );
	vkDestroyShaderModule( dev, mod, nullptr );
}

static std::vector<uint32_t> loadSpv( const char * path )
{
	FILE * f = fopen( path, "rb" );
	if ( !f )
		die( path );
	std::vector<uint32_t> v;
	uint32_t w;
	while ( fread( &w, 4, 1, f ) == 1 )
		v.push_back( w );
	fclose( f );
	return v;
}

static void initVk()
{
	HMODULE lib = LoadLibraryA( "vulkan-1.dll" );
	if ( !lib )
		die( "vulkan-1.dll" );
	auto gipa = reinterpret_cast<PFN_vkGetInstanceProcAddr>( reinterpret_cast<void *>( GetProcAddress( lib, "vkGetInstanceProcAddr" ) ) );
	vkCreateInstance = reinterpret_cast<PFN_vkCreateInstance>( gipa( VK_NULL_HANDLE, "vkCreateInstance" ) );
	VkApplicationInfo app{ VK_STRUCTURE_TYPE_APPLICATION_INFO };
	app.apiVersion = VK_API_VERSION_1_2;
	VkInstanceCreateInfo ic{ VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };
	ic.pApplicationInfo = &app;
	VkInstance inst;
	CK( vkCreateInstance( &ic, nullptr, &inst ) );
#define LOADI( name ) name = reinterpret_cast<PFN_##name>( gipa( inst, #name ) )
	LOADI( vkEnumeratePhysicalDevices ); LOADI( vkGetPhysicalDeviceProperties ); LOADI( vkGetPhysicalDeviceQueueFamilyProperties );
	LOADI( vkGetPhysicalDeviceMemoryProperties ); LOADI( vkGetPhysicalDeviceFeatures ); LOADI( vkGetPhysicalDeviceImageFormatProperties );
	LOADI( vkCreateDevice );
	uint32_t n = 0;
	vkEnumeratePhysicalDevices( inst, &n, nullptr );
	if ( !n )
		die( "no GPU" );
	std::vector<VkPhysicalDevice> pds( n );
	vkEnumeratePhysicalDevices( inst, &n, pds.data() );
	gpu = pds[0];
	for ( VkPhysicalDevice p : pds ) {
		VkPhysicalDeviceProperties pp;
		vkGetPhysicalDeviceProperties( p, &pp );
		if ( pp.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU ) {
			gpu = p;
			break;
		}
	}
	VkPhysicalDeviceProperties pp;
	vkGetPhysicalDeviceProperties( gpu, &pp );
	vkGetPhysicalDeviceMemoryProperties( gpu, &memProps );
	uint32_t qn = 0;
	vkGetPhysicalDeviceQueueFamilyProperties( gpu, &qn, nullptr );
	std::vector<VkQueueFamilyProperties> qf( qn );
	vkGetPhysicalDeviceQueueFamilyProperties( gpu, &qn, qf.data() );
	uint32_t fam = UINT32_MAX;
	for ( uint32_t i = 0; i < qn && fam == UINT32_MAX; i++ )
		if ( ( qf[i].queueFlags & VK_QUEUE_COMPUTE_BIT ) && !( qf[i].queueFlags & VK_QUEUE_GRAPHICS_BIT ) )
			fam = i;
	for ( uint32_t i = 0; i < qn && fam == UINT32_MAX; i++ )
		if ( qf[i].queueFlags & VK_QUEUE_COMPUTE_BIT )
			fam = i;
	if ( fam == UINT32_MAX )
		die( "no compute queue" );
	VkPhysicalDeviceFeatures have, want{};
	vkGetPhysicalDeviceFeatures( gpu, &have );
	VkImageFormatProperties ifp;
	msStorage = have.shaderStorageImageMultisample && vkGetPhysicalDeviceImageFormatProperties( gpu, VK_FORMAT_R32_SFLOAT,
		VK_IMAGE_TYPE_2D, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, 0, &ifp ) == VK_SUCCESS
		&& ( ifp.sampleCounts & VK_SAMPLE_COUNT_4_BIT );
	want.shaderStorageImageMultisample = msStorage;
	float prio = 1.0f;
	VkDeviceQueueCreateInfo qi{ VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO };
	qi.queueFamilyIndex = fam;
	qi.queueCount = 1;
	qi.pQueuePriorities = &prio;
	VkDeviceCreateInfo di{ VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
	di.queueCreateInfoCount = 1;
	di.pQueueCreateInfos = &qi;
	di.pEnabledFeatures = &want;
	CK( vkCreateDevice( gpu, &di, nullptr, &dev ) );
	auto gdpa = reinterpret_cast<PFN_vkGetDeviceProcAddr>( gipa( inst, "vkGetDeviceProcAddr" ) );
#define LOADD( name ) name = reinterpret_cast<PFN_##name>( gdpa( dev, #name ) )
	LOADD( vkGetDeviceQueue ); LOADD( vkCreateBuffer ); LOADD( vkGetBufferMemoryRequirements ); LOADD( vkAllocateMemory );
	LOADD( vkBindBufferMemory ); LOADD( vkMapMemory ); LOADD( vkCreateImage ); LOADD( vkGetImageMemoryRequirements );
	LOADD( vkBindImageMemory ); LOADD( vkCreateImageView ); LOADD( vkCreateSampler ); LOADD( vkCreateShaderModule );
	LOADD( vkDestroyShaderModule ); LOADD( vkCreateDescriptorSetLayout ); LOADD( vkDestroyDescriptorSetLayout );
	LOADD( vkCreatePipelineLayout ); LOADD( vkDestroyPipelineLayout ); LOADD( vkCreateComputePipelines ); LOADD( vkDestroyPipeline );
	LOADD( vkCreateDescriptorPool ); LOADD( vkDestroyDescriptorPool ); LOADD( vkAllocateDescriptorSets );
	LOADD( vkUpdateDescriptorSets ); LOADD( vkCreateCommandPool ); LOADD( vkAllocateCommandBuffers ); LOADD( vkFreeCommandBuffers );
	LOADD( vkBeginCommandBuffer ); LOADD( vkEndCommandBuffer ); LOADD( vkCmdBindPipeline ); LOADD( vkCmdBindDescriptorSets );
	LOADD( vkCmdPushConstants ); LOADD( vkCmdDispatch ); LOADD( vkCmdPipelineBarrier ); LOADD( vkQueueSubmit );
	LOADD( vkCreateFence ); LOADD( vkDestroyFence ); LOADD( vkWaitForFences );
	vkGetDeviceQueue( dev, fam, 0, &queue );
	VkCommandPoolCreateInfo cpi{ VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
	cpi.queueFamilyIndex = fam;
	CK( vkCreateCommandPool( dev, &cpi, nullptr, &cpool ) );
	VkSamplerCreateInfo smi{ VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
	smi.magFilter = smi.minFilter = VK_FILTER_NEAREST;
	smi.addressModeU = smi.addressModeV = smi.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	CK( vkCreateSampler( dev, &smi, nullptr, &sampler ) );
	printf( "gpu %s, multisample storage %s\n", pp.deviceName, msStorage ? "yes" : "no" );
}

static uint32_t rng = 0x9e3779b9u;
static uint32_t rnd() { rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5; return rng; }
static float frand( float a, float b ) { return a + ( b - a ) * float( rnd() >> 8 ) / float( 1 << 24 ); }
static void randomBytes( void * p, size_t n ) { for ( size_t i = 0; i < n; i++ ) static_cast<uint8_t *>( p )[i] = uint8_t( rnd() >> 24 ); }

// std140 uniform bytes, written at the offsets glslang gives the default block (checked against the SPIR-V by cullmatch.sh's
// dumps; the HLSL cbuffer pads to the same)
struct Ubo { std::vector<uint8_t> b = std::vector<uint8_t>( 1024, 0 ); template<class T> void put( size_t o, T v ) { memcpy( &b[o], &v, sizeof v ); } };

struct Side { std::vector<uint32_t> spv; VkDescriptorType imgType; };
static int cases = 0, bad = 0, wrote = 0;

// run both sides on identical prefilled outputs; watched = the output buffers, compared byte for byte
// depths: a hiz the kernel reads back is prefilled with depths in [0, 1], never random bits. A NaN takes GLSL max (FMax,
// undefined for NaN) and HLSL max (NMax, drops the NaN) apart, and a depth buffer holds none.
// given (lane FO4MATCH1, bc7): the prefill itself (a later pass starts from the GLSL's earlier pass); returns the GLSL's output
typedef std::vector<std::vector<uint8_t>> Bytes;
static Bytes both( const char * name, Side * s, std::vector<Bind> binds, const std::vector<Buf> & watched, uint32_t gx, uint32_t gy,
	bool depths = false, const Bytes * given = nullptr )
{
	std::vector<std::vector<uint8_t>> pre, out[2];
	for ( const Buf & w : watched ) {
		pre.emplace_back( w.n );
		if ( given )
			pre.back() = ( *given )[pre.size() - 1];
		else if ( depths )
			for ( size_t i = 0; i + 4 <= w.n; i += 4 ) {
				float v = frand( 0.0f, 1.0f );
				memcpy( &pre.back()[i], &v, 4 );
			}
		else
			randomBytes( pre.back().data(), w.n );
	}
	for ( int k = 0; k < 2; k++ ) {
		for ( size_t i = 0; i < watched.size(); i++ )
			memcpy( watched[i].p, pre[i].data(), watched[i].n );
		for ( Bind & b : binds )
			if ( b.view )
				b.type = s[k].imgType;
		run( s[k].spv, binds, nullptr, 0, gx, gy );
		for ( const Buf & w : watched )
			out[k].emplace_back( static_cast<uint8_t *>( w.p ), static_cast<uint8_t *>( w.p ) + w.n );
	}
	cases++;
	bool same = out[0] == out[1], changed = out[0] != pre;
	wrote += changed;
	size_t diff = 0;
	for ( size_t i = 0; i < watched.size(); i++ )
		for ( size_t j = 0; j < watched[i].n; j++ )
			diff += out[0][i][j] != out[1][i][j];
	if ( !same )
		bad++;
	printf( "%-5s %s (%zu bytes differ%s)\n", same ? "same" : "DIFF", name, diff, changed ? "" : ", GLSL wrote nothing" );
	return out[0];
}

static void caseHiz0( Side * s, bool ms, int w, int h, int samples, int imgSamples )
{
	Img img = makeImg( w, h, imgSamples );
	Buf src = ssbo( size_t( w ) * h * imgSamples * 4 );
	for ( size_t i = 0; i < size_t( w ) * h * imgSamples; i++ )
		static_cast<float *>( src.p )[i] = frand( 0.0f, 1.0f );
	static std::vector<uint32_t> fill[2] = { loadSpv( "fill.spv" ), loadSpv( "fillms.spv" ) };
	int push[3] = { w, h, imgSamples };
	run( fill[ms], { { 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, src.b, VK_NULL_HANDLE },
		{ 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_NULL_HANDLE, img.v } }, push, sizeof push, ( w + 7 ) / 8, ( h + 7 ) / 8, img.i );
	Buf hiz = ssbo( size_t( w ) * h * 4 + 64 );
	Ubo u;
	u.put( 0, samples );
	u.put( 8, w );
	u.put( 12, h );
	Buf ub = makeBuf( u.b.size(), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT );
	memcpy( ub.p, u.b.data(), u.b.size() );
	char name[96];
	snprintf( name, sizeof name, "hiz0%s %dx%d samples %d/%d", ms ? "ms" : "", w, h, samples, imgSamples );
	both( name, s, { { 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, hiz.b, VK_NULL_HANDLE }, { 1, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, VK_NULL_HANDLE, img.v },
		{ 31, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, ub.b, VK_NULL_HANDLE } }, { hiz }, ( w + 8 ) / 8, ( h + 8 ) / 8 );
}

static void caseHizn( Side * s, int sw, int sh, int srcOff )
{
	int dw = ( sw + 1 ) / 2, dh = ( sh + 1 ) / 2, dstOff = srcOff + sw * sh + 3;
	Buf hiz = ssbo( size_t( dstOff + dw * dh + 16 ) * 4 );
	Ubo u;
	u.put( 0, srcOff );
	u.put( 4, dstOff );
	u.put( 8, sw );
	u.put( 12, sh );
	u.put( 16, dw );
	u.put( 20, dh );
	Buf ub = makeBuf( u.b.size(), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT );
	memcpy( ub.p, u.b.data(), u.b.size() );
	char name[96];
	snprintf( name, sizeof name, "hizn %dx%d -> %dx%d", sw, sh, dw, dh );
	// the source level is the same random depths on both sides too (both() prefills the whole buffer)
	both( name, s, { { 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, hiz.b, VK_NULL_HANDLE }, { 31, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, ub.b, VK_NULL_HANDLE } },
		{ hiz }, ( dw + 8 ) / 8, ( dh + 8 ) / 8, true );
}

// the cull scene: GL column-major matrices, a perspective proj, its five planes, a hiz ladder over vpSize
static void caseCull( Side * s, int mode, uint32_t seed )
{
	rng = seed;
	const int vw = 96, vh = 64, nJobs = 8, nMl = 120;
	const uint32_t nItems = 333;
	float n = 0.5f, f = 200.0f, t = 1.0f / std::tan( 0.6f ), asp = float( vw ) / vh;
	float P[16] = { t / asp, 0, 0, 0, 0, t, 0, 0, 0, 0, ( f + n ) / ( n - f ), -1, 0, 0, 2 * f * n / ( n - f ), 0 };
	auto row = [ & ]( int r, float * o ) { for ( int c = 0; c < 4; c++ ) o[c] = P[c * 4 + r]; };
	float r0[4], r1[4], r2[4], r3[4];
	row( 0, r0 ); row( 1, r1 ); row( 2, r2 ); row( 3, r3 );
	float planes[5][4];
	for ( int c = 0; c < 4; c++ ) {
		planes[0][c] = r3[c] + r0[c];
		planes[1][c] = r3[c] - r0[c];
		planes[2][c] = r3[c] + r1[c];
		planes[3][c] = r3[c] - r1[c];
		planes[4][c] = r3[c] + r2[c];
	}
	for ( auto & p : planes ) {
		float l = std::sqrt( p[0] * p[0] + p[1] * p[1] + p[2] * p[2] );
		for ( float & v : p )
			v /= l;
	}
	int levels = 0, lvlOff[16] = {}, lvlDim[16][2] = {}, total = 0;
	for ( int w = vw, h = vh; levels < 16; levels++ ) {
		lvlOff[levels] = total;
		lvlDim[levels][0] = w;
		lvlDim[levels][1] = h;
		total += w * h;
		if ( w == 1 && h == 1 ) {
			levels++;
			break;
		}
		w = std::max( 1, ( w + 1 ) / 2 );
		h = std::max( 1, ( h + 1 ) / 2 );
	}
	Buf hiz = ssbo( size_t( total ) * 4 );
	for ( int i = 0; i < total; i++ )
		static_cast<float *>( hiz.p )[i] = frand( 0.0f, 1.0f ) < 0.15f ? 1.0f : frand( 0.9f, 1.0f );
	Buf ml = ssbo( nMl * 48 );
	for ( int i = 0; i < nMl; i++ ) {
		float * m = static_cast<float *>( ml.p ) + i * 12;
		m[0] = frand( -3, 3 ); m[1] = frand( -3, 3 ); m[2] = frand( -3, 3 ); m[3] = frand( 0.05f, 2.0f );
		float a[3] = { frand( -1, 1 ), frand( -1, 1 ), frand( -1, 1 ) }, l = std::sqrt( a[0] * a[0] + a[1] * a[1] + a[2] * a[2] ) + 1e-6f;
		m[4] = a[0] / l; m[5] = a[1] / l; m[6] = a[2] / l; m[7] = frand( -1.0f, 1.3f );
		uint32_t * u = reinterpret_cast<uint32_t *>( m + 8 );
		u[0] = rnd() % 5000; u[1] = 1 + rnd() % 124; u[2] = rnd(); u[3] = rnd();
	}
	Buf jobs = ssbo( nJobs * 80 );
	for ( int j = 0; j < nJobs; j++ ) {
		float * M = static_cast<float *>( jobs.p ) + j * 20;
		float yaw = frand( -3.1f, 3.1f ), sc = frand( 0.5f, 2.0f ), cy = std::cos( yaw ), sy = std::sin( yaw );
		float T[16] = { cy * sc, 0, -sy * sc, 0, 0, sc, 0, 0, sy * sc, 0, cy * sc, 0, frand( -30, 30 ), frand( -20, 20 ), frand( -150, 5 ), 1 };
		memcpy( M, T, sizeof T );
		M[16] = sc;
		M[17] = j % 3 == 0 ? 0.0f : j % 3 == 1 ? 1.0f : -1.0f;
		M[18] = 0;
		M[19] = 0;
	}
	Buf items = ssbo( nItems * 8 );
	for ( uint32_t i = 0; i < nItems; i++ ) {
		static_cast<uint32_t *>( items.p )[i * 2] = rnd() % nJobs;
		static_cast<uint32_t *>( items.p )[i * 2 + 1] = rnd() % nMl;
	}
	Buf cmds = ssbo( size_t( nItems ) * 5 * 4 + 64 );
	Buf census = ssbo( 8 * 4 );
	Ubo u;
	for ( int i = 0; i < 16; i++ )
		u.put( i * 4, P[i] );
	for ( int p = 0; p < 5; p++ )
		for ( int c = 0; c < 4; c++ )
			u.put( 64 + p * 16 + c * 4, planes[p][c] );
	u.put( 144, nItems );
	u.put( 152, vw );
	u.put( 156, vh );
	u.put( 160, levels );
	for ( int i = 0; i < 16; i++ ) {
		u.put( 176 + i * 16, lvlOff[i] );
		u.put( 432 + i * 16, lvlDim[i][0] );
		u.put( 432 + i * 16 + 4, lvlDim[i][1] );
	}
	u.put( 688, mode );
	u.put( 692, 1e-4f );
	Buf ub = makeBuf( u.b.size(), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT );
	memcpy( ub.p, u.b.data(), u.b.size() );
	char name[96];
	snprintf( name, sizeof name, "cull mode %d seed %u", mode, seed );
	both( name, s, { { 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, hiz.b, VK_NULL_HANDLE }, { 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, ml.b, VK_NULL_HANDLE },
		{ 2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, jobs.b, VK_NULL_HANDLE }, { 3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, items.b, VK_NULL_HANDLE },
		{ 4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, cmds.b, VK_NULL_HANDLE }, { 5, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, census.b, VK_NULL_HANDLE },
		{ 31, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, ub.b, VK_NULL_HANDLE } }, { cmds, census }, ( nItems + 63 ) / 64, 1 );
	// census: a fresh zeroed count per run shows which reasons the scene reached (the watched copy above starts random)
	if ( mode & 32 ) {
		memset( census.p, 0, 32 );
		run( s[0].spv, { { 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, hiz.b, VK_NULL_HANDLE }, { 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, ml.b, VK_NULL_HANDLE },
			{ 2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, jobs.b, VK_NULL_HANDLE }, { 3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, items.b, VK_NULL_HANDLE },
			{ 4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, cmds.b, VK_NULL_HANDLE }, { 5, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, census.b, VK_NULL_HANDLE },
			{ 31, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, ub.b, VK_NULL_HANDLE } }, nullptr, 0, ( nItems + 63 ) / 64, 1 );
		const uint32_t * c = static_cast<uint32_t *>( census.p );
		printf( "      census (GLSL, zeroed): drawn %u frustum %u cone %u occluded %u tris %u\n", c[0], c[1], c[2], c[3], c[4] );
	}
}

// lane FO4MATCH1 (11-14): the far-LOD BC7 encoder (res/shaders/features/far-lod/lodgen_bc7.glsl, tables filled as
// src/features/far-lod/lodgengpu.cpp does) against lodgen_bc7_cs.hlsl: its three passes as the host runs them (pass 0 on
// every block, then passes 1 and 2 on the blocks the pass before left with error, each from the GLSL's earlier output).
// Pixels 0xAARRGGBB: a gradient plus noise, opaque or random alpha. Uniforms (std140): uPass 0, uW 4, uH 8, uBw 12,
// uFirst 16, uCount 20, uWt 32.
static void caseBc7( Side * s, int w, int h, bool opaque, int wt0, int wt1, int wt2, int wt3, uint32_t seed )
{
	rng = seed;
	int bw = ( w + 3 ) / 4, bh = ( h + 3 ) / 4, nb = bw * bh;
	Buf src = ssbo( size_t( w ) * h * 4 );
	for ( int y = 0; y < h; y++ )
		for ( int x = 0; x < w; x++ ) {
			auto ch = [ & ]( float base, float gx, float gy ) {
				return uint32_t( std::min( 255.0f, std::max( 0.0f, base + gx * x + gy * y + frand( -24.0f, 24.0f ) ) ) );
			};
			uint32_t a = opaque ? 255u : ( rnd() >> 24 );
			static_cast<uint32_t *>( src.p )[y * w + x] = ( a << 24 ) | ( ch( 40, 3.1f, 0.7f ) << 16 ) | ( ch( 200, -1.3f, 2.2f ) << 8 ) | ch( 90, 0.4f, -1.9f );
		}
	Buf dst = ssbo( size_t( nb ) * 16 ), err = ssbo( size_t( nb ) * 4 ), list = ssbo( size_t( nb ) * 4 + 4 );
	Bytes prev;
	int count = nb;
	for ( int pass = 0; pass < 3 && count > 0; pass++ ) {
		Ubo u;
		u.put( 0, pass );
		u.put( 4, w );
		u.put( 8, h );
		u.put( 12, bw );
		u.put( 16, 0 );
		u.put( 20, count );
		int wt[4] = { wt0, wt1, wt2, wt3 };
		for ( int i = 0; i < 4; i++ )
			u.put( 32 + i * 4, wt[i] );
		Buf ub = makeBuf( u.b.size(), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT );
		memcpy( ub.p, u.b.data(), u.b.size() );
		char name[128];
		snprintf( name, sizeof name, "bc7 %dx%d %s wt %d,%d,%d,%d pass %d blocks %d", w, h, opaque ? "opaque" : "alpha", wt0, wt1, wt2, wt3,
			pass, count );
		prev = both( name, s, { { 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, src.b, VK_NULL_HANDLE }, { 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, dst.b, VK_NULL_HANDLE },
			{ 2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, err.b, VK_NULL_HANDLE }, { 3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, list.b, VK_NULL_HANDLE },
			{ 31, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, ub.b, VK_NULL_HANDLE } }, { dst, err }, uint32_t( ( count + 63 ) / 64 ), 1, false,
			pass ? &prev : nullptr );
		// the next pass's list: the blocks the GLSL left with error (the host's blkList)
		count = 0;
		const int32_t * e = reinterpret_cast<const int32_t *>( prev[1].data() );
		for ( int b = 0; b < nb; b++ )
			if ( e[b] > 0 )
				static_cast<int32_t *>( list.p )[count++] = b;
	}
}

int main( int argc, char ** argv )
{
	if ( argc != 4 ) {
		printf( "usage: cullmatch <hiz0|hiz0ms|hizn|cull|bc7> <glsl.spv> <hlsl.spv>\n" );
		return 3;
	}
	std::string k = argv[1];
	initVk();
	Side s[2] = { { loadSpv( argv[2] ), VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER }, { loadSpv( argv[3] ), VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE } };
	if ( k == "hiz0" ) {
		int dims[][2] = { { 37, 23 }, { 64, 64 }, { 1, 1 }, { 13, 50 }, { 96, 64 } };
		for ( auto & d : dims )
			caseHiz0( s, false, d[0], d[1], 1, 1 );
	} else if ( k == "hiz0ms" ) {
		if ( !msStorage )
			die( "no multisample storage images on this GPU (the fill needs them)" );
		int dims[][2] = { { 37, 23 }, { 64, 64 }, { 1, 1 }, { 13, 50 } };
		for ( auto & d : dims ) {
			caseHiz0( s, true, d[0], d[1], 4, 4 );
			caseHiz0( s, true, d[0], d[1], 3, 4 );
			caseHiz0( s, true, d[0], d[1], 1, 4 );
		}
	} else if ( k == "hizn" ) {
		int dims[][3] = { { 37, 23, 7 }, { 64, 64, 0 }, { 5, 1, 2 }, { 1, 9, 11 }, { 96, 64, 5 }, { 2, 2, 0 } };
		for ( auto & d : dims )
			caseHizn( s, d[0], d[1], d[2] );
	} else if ( k == "cull" ) {
		int modes[] = { 0, 1, 2, 4, 7, 1 | 32, 2 | 32, 4 | 32, 7 | 32, 7 | 8 | 32, 7 | 16 | 32, 63 };
		uint32_t seed = 1;
		for ( int m : modes )
			for ( int r = 0; r < 3; r++ )
				caseCull( s, m, 0x1234567u * seed++ );
	} else if ( k == "bc7" ) {
		caseBc7( s, 16, 16, true, 1, 1, 1, 1, 11 );
		caseBc7( s, 37, 23, true, 2, 3, 1, 4, 12 );
		caseBc7( s, 64, 32, false, 1, 1, 1, 1, 13 );
		caseBc7( s, 13, 9, false, 3, 1, 2, 5, 14 );
	} else {
		printf( "unknown kernel %s\n", k.c_str() );
		return 3;
	}
	if ( wrote < cases ) {
		printf( "%s: %d of %d cases the GLSL wrote nothing\n", k.c_str(), cases - wrote, cases );
		if ( !bad )
			return 2;
	}
	printf( "%s: %d of %d cases byte-identical -> %s\n", k.c_str(), cases - bad, cases, bad ? "MISMATCH" : "MATCH" );
	return bad ? 1 : 0;
}
