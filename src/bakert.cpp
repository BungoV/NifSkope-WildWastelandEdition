/* Lane RTBAKE1 (2026-10-21): the bakes' shared ray back end (see bakert.h). A headless Vulkan compute device: no
 * window, no GL interop. vulkan-1.dll is loaded at run time, so the exe starts (and bakes on the CPU) without it. */

#include "bakert.h"
#include "probebvh.h"

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <dlfcn.h>
#endif

#include <chrono>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <sstream>

namespace {
#include "bakert/rq.spv.h"
#include "bakert/bvh.spv.h"
}

namespace bakert {

namespace {

// ------------------------------------------------------------------ the loader
#define BAKERT_VK_GLOBAL( X ) X( vkCreateInstance ) X( vkEnumerateInstanceVersion )
#define BAKERT_VK_INSTANCE( X ) X( vkDestroyInstance ) X( vkEnumeratePhysicalDevices ) X( vkGetPhysicalDeviceProperties ) \
	X( vkGetPhysicalDeviceFeatures2 ) X( vkGetPhysicalDeviceQueueFamilyProperties ) X( vkGetPhysicalDeviceMemoryProperties ) \
	X( vkEnumerateDeviceExtensionProperties ) X( vkCreateDevice ) X( vkGetDeviceProcAddr ) X( vkGetPhysicalDeviceProperties2 )
#define BAKERT_VK_DEVICE( X ) X( vkDestroyDevice ) X( vkGetDeviceQueue ) X( vkCreateBuffer ) X( vkDestroyBuffer ) \
	X( vkGetBufferMemoryRequirements ) X( vkAllocateMemory ) X( vkFreeMemory ) X( vkBindBufferMemory ) X( vkMapMemory ) \
	X( vkCreateCommandPool ) X( vkAllocateCommandBuffers ) X( vkBeginCommandBuffer ) X( vkEndCommandBuffer ) \
	X( vkResetCommandBuffer ) X( vkCmdCopyBuffer ) X( vkCmdBindPipeline ) X( vkCmdBindDescriptorSets ) X( vkCmdPushConstants ) \
	X( vkCmdDispatch ) X( vkCmdPipelineBarrier ) X( vkQueueSubmit ) X( vkCreateFence ) X( vkWaitForFences ) X( vkResetFences ) \
	X( vkCreateShaderModule ) X( vkDestroyShaderModule ) X( vkCreateDescriptorSetLayout ) X( vkCreatePipelineLayout ) \
	X( vkCreateComputePipelines ) X( vkCreateDescriptorPool ) X( vkAllocateDescriptorSets ) X( vkFreeDescriptorSets ) \
	X( vkUpdateDescriptorSets ) X( vkGetBufferDeviceAddress )
#define BAKERT_VK_RT( X ) X( vkCreateAccelerationStructureKHR ) X( vkDestroyAccelerationStructureKHR ) \
	X( vkGetAccelerationStructureBuildSizesKHR ) X( vkCmdBuildAccelerationStructuresKHR ) \
	X( vkGetAccelerationStructureDeviceAddressKHR )
#define BAKERT_DECL( f ) PFN_##f f = nullptr;
struct Fn
{
	PFN_vkGetInstanceProcAddr gipa = nullptr;
	BAKERT_VK_GLOBAL( BAKERT_DECL )
	BAKERT_VK_INSTANCE( BAKERT_DECL )
	BAKERT_VK_DEVICE( BAKERT_DECL )
	BAKERT_VK_RT( BAKERT_DECL )
};
Fn vk;

struct Buf
{
	VkBuffer b = VK_NULL_HANDLE;
	VkDeviceMemory m = VK_NULL_HANDLE;
	void * map = nullptr;
	VkDeviceSize size = 0;
	VkDeviceAddress addr = 0;
};

struct Dev
{
	bool ok = false, rt = false;
	std::string name, why;
	VkInstance inst = VK_NULL_HANDLE;
	VkPhysicalDevice pd = VK_NULL_HANDLE;
	VkDevice dev = VK_NULL_HANDLE;
	VkQueue q = VK_NULL_HANDLE;
	uint32_t qf = 0;
	VkPhysicalDeviceMemoryProperties mem {};
	VkCommandPool pool = VK_NULL_HANDLE;
	VkCommandBuffer cb = VK_NULL_HANDLE;
	VkFence fence = VK_NULL_HANDLE;
	VkDescriptorSetLayout dsl[2] = { VK_NULL_HANDLE, VK_NULL_HANDLE };   // [0] compute, [1] rt
	VkPipelineLayout pl[2] = { VK_NULL_HANDLE, VK_NULL_HANDLE };
	VkPipeline pipe[2] = { VK_NULL_HANDLE, VK_NULL_HANDLE };
	VkDescriptorPool dpool = VK_NULL_HANDLE;
	VkDeviceSize scratchAlign = 256;
	Buf rays, hits;            // one batch's rays and hits, shared by every Scene (under m)
	size_t cap = 0;
	std::mutex m;
};

const char * kRed = "WW_BAKE_RT_RED";

uint32_t memType( const Dev & D, uint32_t bits, VkMemoryPropertyFlags want )
{
	for ( uint32_t i = 0; i < D.mem.memoryTypeCount; i++ )
		if ( ( bits & ( 1u << i ) ) && ( D.mem.memoryTypes[i].propertyFlags & want ) == want )
			return i;
	return ~0u;
}

void freeBuf( Dev & D, Buf & b )
{
	if ( b.b )
		vk.vkDestroyBuffer( D.dev, b.b, nullptr );
	if ( b.m )
		vk.vkFreeMemory( D.dev, b.m, nullptr );
	b = Buf();
}

// a buffer in the first memory type that has every flag of one of the `wants`, in order; host-visible ones are mapped
bool makeBuf( Dev & D, VkDeviceSize size, VkBufferUsageFlags usage, std::initializer_list<VkMemoryPropertyFlags> wants, Buf & out )
{
	out = Buf();
	VkBufferCreateInfo bi { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
	bi.size = std::max<VkDeviceSize>( size, 16 );
	bi.usage = usage;
	bi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	if ( vk.vkCreateBuffer( D.dev, &bi, nullptr, &out.b ) != VK_SUCCESS )
		return false;
	VkMemoryRequirements mr;
	vk.vkGetBufferMemoryRequirements( D.dev, out.b, &mr );
	uint32_t ti = ~0u;
	VkMemoryPropertyFlags got = 0;
	for ( VkMemoryPropertyFlags w : wants )
		if ( ( ti = memType( D, mr.memoryTypeBits, w ) ) != ~0u ) {
			got = D.mem.memoryTypes[ti].propertyFlags;
			break;
		}
	if ( ti == ~0u ) {
		freeBuf( D, out );
		return false;
	}
	VkMemoryAllocateFlagsInfo fi { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO };
	fi.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;
	VkMemoryAllocateInfo ai { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
	ai.allocationSize = mr.size;
	ai.memoryTypeIndex = ti;
	if ( usage & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT )
		ai.pNext = &fi;
	if ( vk.vkAllocateMemory( D.dev, &ai, nullptr, &out.m ) != VK_SUCCESS || vk.vkBindBufferMemory( D.dev, out.b, out.m, 0 ) != VK_SUCCESS ) {
		freeBuf( D, out );
		return false;
	}
	out.size = bi.size;
	if ( ( got & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT ) && vk.vkMapMemory( D.dev, out.m, 0, VK_WHOLE_SIZE, 0, &out.map ) != VK_SUCCESS ) {
		freeBuf( D, out );
		return false;
	}
	if ( usage & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT ) {
		VkBufferDeviceAddressInfo ad { VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO };
		ad.buffer = out.b;
		out.addr = vk.vkGetBufferDeviceAddress( D.dev, &ad );
	}
	return true;
}

const VkMemoryPropertyFlags kDevLocal = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
const VkMemoryPropertyFlags kHost = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

bool begin( Dev & D )
{
	vk.vkResetCommandBuffer( D.cb, 0 );
	VkCommandBufferBeginInfo bi { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
	bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	return vk.vkBeginCommandBuffer( D.cb, &bi ) == VK_SUCCESS;
}

bool submitWait( Dev & D )
{
	if ( vk.vkEndCommandBuffer( D.cb ) != VK_SUCCESS )
		return false;
	VkSubmitInfo si { VK_STRUCTURE_TYPE_SUBMIT_INFO };
	si.commandBufferCount = 1;
	si.pCommandBuffers = &D.cb;
	vk.vkResetFences( D.dev, 1, &D.fence );
	if ( vk.vkQueueSubmit( D.q, 1, &si, D.fence ) != VK_SUCCESS )
		return false;
	return vk.vkWaitForFences( D.dev, 1, &D.fence, VK_TRUE, UINT64_MAX ) == VK_SUCCESS;
}

// a device-local buffer holding `bytes` (through a staging copy)
bool upload( Dev & D, const void * data, size_t bytes, VkBufferUsageFlags usage, Buf & out )
{
	if ( !makeBuf( D, bytes, usage | VK_BUFFER_USAGE_TRANSFER_DST_BIT, { kDevLocal, kHost }, out ) )
		return false;
	if ( !bytes )
		return true;
	if ( out.map ) {   // landed in host memory (no device-local type took it): write it straight
		std::memcpy( out.map, data, bytes );
		return true;
	}
	Buf st;
	if ( !makeBuf( D, bytes, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, { kHost }, st ) )
		return false;
	std::memcpy( st.map, data, bytes );
	bool ok = begin( D );
	if ( ok ) {
		VkBufferCopy c { 0, 0, bytes };
		vk.vkCmdCopyBuffer( D.cb, st.b, out.b, 1, &c );
		ok = submitWait( D );
	}
	freeBuf( D, st );
	return ok;
}

VkPipeline pipeline( Dev & D, const uint32_t * code, size_t bytes, VkPipelineLayout pl )
{
	VkShaderModuleCreateInfo mi { VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
	mi.codeSize = bytes;
	mi.pCode = code;
	VkShaderModule sm = VK_NULL_HANDLE;
	if ( vk.vkCreateShaderModule( D.dev, &mi, nullptr, &sm ) != VK_SUCCESS )
		return VK_NULL_HANDLE;
	VkComputePipelineCreateInfo ci { VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO };
	ci.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	ci.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	ci.stage.module = sm;
	ci.stage.pName = "main";
	ci.layout = pl;
	VkPipeline p = VK_NULL_HANDLE;
	if ( vk.vkCreateComputePipelines( D.dev, VK_NULL_HANDLE, 1, &ci, nullptr, &p ) != VK_SUCCESS )
		p = VK_NULL_HANDLE;
	vk.vkDestroyShaderModule( D.dev, sm, nullptr );
	return p;
}

bool layout( Dev & D, int which )
{
	std::vector<VkDescriptorSetLayoutBinding> b;
	const int n = which ? 8 : 9;
	for ( int i = 0; i < n; i++ ) {
		VkDescriptorSetLayoutBinding x {};
		x.binding = uint32_t( i );
		x.descriptorType = ( which && i == 6 ) ? VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR : VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		x.descriptorCount = 1;
		x.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
		b.push_back( x );
	}
	VkDescriptorSetLayoutCreateInfo li { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
	li.bindingCount = uint32_t( b.size() );
	li.pBindings = b.data();
	if ( vk.vkCreateDescriptorSetLayout( D.dev, &li, nullptr, &D.dsl[which] ) != VK_SUCCESS )
		return false;
	VkPushConstantRange pr { VK_SHADER_STAGE_COMPUTE_BIT, 0, 24 };
	VkPipelineLayoutCreateInfo pi { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
	pi.setLayoutCount = 1;
	pi.pSetLayouts = &D.dsl[which];
	pi.pushConstantRangeCount = 1;
	pi.pPushConstantRanges = &pr;
	if ( vk.vkCreatePipelineLayout( D.dev, &pi, nullptr, &D.pl[which] ) != VK_SUCCESS )
		return false;
	D.pipe[which] = which ? pipeline( D, bakert_rq_spv, sizeof bakert_rq_spv, D.pl[1] )
		: pipeline( D, bakert_bvh_spv, sizeof bakert_bvh_spv, D.pl[0] );
	return D.pipe[which] != VK_NULL_HANDLE;
}

bool hasExt( const std::vector<VkExtensionProperties> & e, const char * n )
{
	for ( const VkExtensionProperties & x : e )
		if ( !std::strcmp( x.extensionName, n ) )
			return true;
	return false;
}

void init( Dev & D )
{
	const char * red = std::getenv( kRed );
	if ( red && !std::strcmp( red, "cpu" ) ) {
		D.why = "WW_BAKE_RT_RED=cpu";
		return;
	}
#ifdef _WIN32
	HMODULE lib = LoadLibraryA( "vulkan-1.dll" );
	if ( lib )
		vk.gipa = reinterpret_cast<PFN_vkGetInstanceProcAddr>( reinterpret_cast<void *>( GetProcAddress( lib, "vkGetInstanceProcAddr" ) ) );
#else
	void * lib = dlopen( "libvulkan.so.1", RTLD_NOW );
	if ( lib )
		vk.gipa = reinterpret_cast<PFN_vkGetInstanceProcAddr>( dlsym( lib, "vkGetInstanceProcAddr" ) );
#endif
	if ( !vk.gipa ) {
		D.why = "no Vulkan loader (vulkan-1.dll)";
		return;
	}
#define BAKERT_G( f ) vk.f = reinterpret_cast<PFN_##f>( vk.gipa( VK_NULL_HANDLE, #f ) );
	BAKERT_VK_GLOBAL( BAKERT_G )
	uint32_t api = VK_API_VERSION_1_0;
	if ( !vk.vkCreateInstance || !vk.vkEnumerateInstanceVersion || vk.vkEnumerateInstanceVersion( &api ) != VK_SUCCESS
		|| api < VK_API_VERSION_1_2 ) {
		D.why = "the Vulkan loader is older than 1.2";
		return;
	}
	VkApplicationInfo app { VK_STRUCTURE_TYPE_APPLICATION_INFO };
	app.pApplicationName = "NifSkope bake";
	app.apiVersion = VK_API_VERSION_1_2;
	VkInstanceCreateInfo ii { VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };
	ii.pApplicationInfo = &app;
	if ( vk.vkCreateInstance( &ii, nullptr, &D.inst ) != VK_SUCCESS ) {
		D.why = "vkCreateInstance failed";
		return;
	}
#define BAKERT_I( f ) vk.f = reinterpret_cast<PFN_##f>( vk.gipa( D.inst, #f ) );
	BAKERT_VK_INSTANCE( BAKERT_I )
	uint32_t n = 0;
	vk.vkEnumeratePhysicalDevices( D.inst, &n, nullptr );
	std::vector<VkPhysicalDevice> pds( n );
	if ( n )
		vk.vkEnumeratePhysicalDevices( D.inst, &n, pds.data() );
	// the device: RT first, then a discrete GPU, then anything with a compute queue on Vulkan 1.2
	int best = -1, bestScore = -1;
	bool bestRt = false;
	uint32_t bestQf = 0;
	for ( uint32_t i = 0; i < n; i++ ) {
		VkPhysicalDeviceProperties pp;
		vk.vkGetPhysicalDeviceProperties( pds[i], &pp );
		if ( pp.apiVersion < VK_API_VERSION_1_2 )
			continue;
		uint32_t nq = 0;
		vk.vkGetPhysicalDeviceQueueFamilyProperties( pds[i], &nq, nullptr );
		std::vector<VkQueueFamilyProperties> qs( nq );
		vk.vkGetPhysicalDeviceQueueFamilyProperties( pds[i], &nq, qs.data() );
		int qf = -1;
		for ( uint32_t k = 0; k < nq && qf < 0; k++ )
			if ( qs[k].queueFlags & VK_QUEUE_COMPUTE_BIT )
				qf = int( k );
		if ( qf < 0 )
			continue;
		uint32_t ne = 0;
		vk.vkEnumerateDeviceExtensionProperties( pds[i], nullptr, &ne, nullptr );
		std::vector<VkExtensionProperties> ex( ne );
		vk.vkEnumerateDeviceExtensionProperties( pds[i], nullptr, &ne, ex.data() );
		VkPhysicalDeviceRayQueryFeaturesKHR rq { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR };
		VkPhysicalDeviceAccelerationStructureFeaturesKHR as { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR };
		VkPhysicalDeviceVulkan12Features v12 { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES };
		const bool exts = hasExt( ex, VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME ) && hasExt( ex, VK_KHR_RAY_QUERY_EXTENSION_NAME )
			&& hasExt( ex, VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME );
		v12.pNext = exts ? &as : nullptr;
		as.pNext = &rq;
		VkPhysicalDeviceFeatures2 f2 { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2 };
		f2.pNext = &v12;
		vk.vkGetPhysicalDeviceFeatures2( pds[i], &f2 );
		const bool rt = exts && rq.rayQuery && as.accelerationStructure && v12.bufferDeviceAddress;
		const int score = ( rt ? 4 : 0 ) + ( pp.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU ? 2 : 0 )
			+ ( pp.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU ? 1 : 0 );
		if ( score > bestScore ) {
			best = int( i );
			bestScore = score;
			bestRt = rt;
			bestQf = uint32_t( qf );
		}
	}
	if ( best < 0 ) {
		D.why = "no Vulkan 1.2 device with a compute queue";
		return;
	}
	D.pd = pds[size_t( best )];
	D.qf = bestQf;
	VkPhysicalDeviceProperties pp;
	vk.vkGetPhysicalDeviceProperties( D.pd, &pp );
	D.name = pp.deviceName;
	vk.vkGetPhysicalDeviceMemoryProperties( D.pd, &D.mem );
	// a red asking for compute never turns RT on
	const bool wantRt = bestRt && !( red && !std::strcmp( red, "compute" ) );
	const float prio = 1.0f;
	VkDeviceQueueCreateInfo qi { VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO };
	qi.queueFamilyIndex = D.qf;
	qi.queueCount = 1;
	qi.pQueuePriorities = &prio;
	VkPhysicalDeviceRayQueryFeaturesKHR rq { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR };
	rq.rayQuery = VK_TRUE;
	VkPhysicalDeviceAccelerationStructureFeaturesKHR as { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR };
	as.accelerationStructure = VK_TRUE;
	as.pNext = &rq;
	VkPhysicalDeviceVulkan12Features v12 { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES };
	v12.bufferDeviceAddress = wantRt ? VK_TRUE : VK_FALSE;
	v12.pNext = wantRt ? &as : nullptr;
	const char * exts[] = { VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME, VK_KHR_RAY_QUERY_EXTENSION_NAME,
		VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME };
	VkDeviceCreateInfo di { VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
	di.pNext = &v12;
	di.queueCreateInfoCount = 1;
	di.pQueueCreateInfos = &qi;
	di.enabledExtensionCount = wantRt ? 3 : 0;
	di.ppEnabledExtensionNames = exts;
	if ( vk.vkCreateDevice( D.pd, &di, nullptr, &D.dev ) != VK_SUCCESS ) {
		D.why = "vkCreateDevice failed on " + D.name;
		return;
	}
#define BAKERT_D( f ) vk.f = reinterpret_cast<PFN_##f>( vk.vkGetDeviceProcAddr( D.dev, #f ) );
	BAKERT_VK_DEVICE( BAKERT_D )
	if ( wantRt ) {
		BAKERT_VK_RT( BAKERT_D )
		VkPhysicalDeviceAccelerationStructurePropertiesKHR ap { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_PROPERTIES_KHR };
		VkPhysicalDeviceProperties2 p2 { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2 };
		p2.pNext = &ap;
		vk.vkGetPhysicalDeviceProperties2( D.pd, &p2 );
		D.scratchAlign = std::max<VkDeviceSize>( ap.minAccelerationStructureScratchOffsetAlignment, 1 );
	}
	vk.vkGetDeviceQueue( D.dev, D.qf, 0, &D.q );
	VkCommandPoolCreateInfo ci { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
	ci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	ci.queueFamilyIndex = D.qf;
	VkCommandBufferAllocateInfo cai { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
	VkFenceCreateInfo fi { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
	if ( vk.vkCreateCommandPool( D.dev, &ci, nullptr, &D.pool ) != VK_SUCCESS ) {
		D.why = "no command pool";
		return;
	}
	cai.commandPool = D.pool;
	cai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	cai.commandBufferCount = 1;
	if ( vk.vkAllocateCommandBuffers( D.dev, &cai, &D.cb ) != VK_SUCCESS || vk.vkCreateFence( D.dev, &fi, nullptr, &D.fence ) != VK_SUCCESS ) {
		D.why = "no command buffer";
		return;
	}
	VkDescriptorPoolSize ps[2] = { { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 64 * 9 }, { VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 64 } };
	VkDescriptorPoolCreateInfo dpi { VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
	dpi.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
	dpi.maxSets = 64;
	dpi.poolSizeCount = wantRt ? 2 : 1;
	dpi.pPoolSizes = ps;
	if ( vk.vkCreateDescriptorPool( D.dev, &dpi, nullptr, &D.dpool ) != VK_SUCCESS || !layout( D, 0 ) ) {
		D.why = "the compute tracer did not build";
		return;
	}
	if ( wantRt && !layout( D, 1 ) ) {
		D.why = "the ray-query tracer did not build";
		return;
	}
	// one batch: 1M rays (32 MB) and their hits; the hits read back from cached host memory when there is one
	D.cap = size_t( 1 ) << 20;
	if ( !makeBuf( D, D.cap * sizeof( Ray ), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, { kHost | kDevLocal, kHost }, D.rays )
		|| !makeBuf( D, D.cap * sizeof( Hit ), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
			{ kHost | VK_MEMORY_PROPERTY_HOST_CACHED_BIT, kHost }, D.hits ) ) {
		D.why = "no memory for the ray batch";
		return;
	}
	D.ok = true;
	D.rt = wantRt;
}

Dev & dev()
{
	static Dev D;
	static std::once_flag once;
	std::call_once( once, []() { init( D ); } );
	return D;
}

} // namespace

const char * pathName( Path p )
{
	return p == Path::Rt ? "rt" : p == Path::Compute ? "compute" : "cpu";
}

Path pickPath( std::string * why )
{
	Dev & D = dev();
	const char * red = std::getenv( kRed );
	const std::string r = red ? red : "";
	Path p = Path::Cpu;
	if ( r == "cpu" )
		p = Path::Cpu;
	else if ( D.ok && D.rt && r != "compute" )
		p = Path::Rt;
	else if ( D.ok )
		p = Path::Compute;
	if ( why ) {
		*why = r.empty() ? std::string( "picked" ) : "red " + r;
		if ( !D.why.empty() )
			*why += "; " + D.why;
		if ( r == "rt" && p != Path::Rt )
			*why += "; asked for rt, the device has none";
	}
	return p;
}

std::string deviceName()
{
	return dev().ok ? dev().name : std::string();
}

struct Scene::Impl
{
	Buf triOf, maskTris, maps, alpha;   // the alpha masks (both tracers)
	Buf verts, remap, blasBuf, tlasBuf; // rt
	VkAccelerationStructureKHR blas = VK_NULL_HANDLE, tlas = VK_NULL_HANDLE;
	Buf nodes, idx, tris;               // compute
	VkDescriptorSet ds = VK_NULL_HANDLE;
	uint32_t nTri = 0, nOpaque = 0, useMask = 0;
	int which = 0;                      // 0 compute, 1 rt

	void release( Dev & D )
	{
		if ( ds )
			vk.vkFreeDescriptorSets( D.dev, D.dpool, 1, &ds );
		if ( tlas )
			vk.vkDestroyAccelerationStructureKHR( D.dev, tlas, nullptr );
		if ( blas )
			vk.vkDestroyAccelerationStructureKHR( D.dev, blas, nullptr );
		for ( Buf * b : { &triOf, &maskTris, &maps, &alpha, &verts, &remap, &blasBuf, &tlasBuf, &nodes, &idx, &tris } )
			freeBuf( D, *b );
	}
};

namespace {

bool buildMasks( Dev & D, const probebvh::Bvh & bvh, Scene::Impl & I )
{
	const probebvh::AlphaMask * m = bvh.mask;
	I.nTri = uint32_t( bvh.t.size() / 9 );
	I.useMask = m && !m->empty() ? 1u : 0u;
	std::vector<int32_t> triOf;
	std::vector<uint32_t> mt, maps, ab;
	if ( I.useMask ) {
		triOf.assign( I.nTri, -1 );
		for ( size_t i = 0; i < m->triOf.size() && i < I.nTri; i++ )
			triOf[i] = m->triOf[i];
		std::vector<uint8_t> bytes;
		for ( const probebvh::AlphaMask::Map & mp : m->maps ) {
			maps.insert( maps.end(), { uint32_t( bytes.size() ), uint32_t( mp.w ), uint32_t( mp.h ), 0u } );
			bytes.insert( bytes.end(), mp.a.begin(), mp.a.end() );
		}
		bytes.resize( ( bytes.size() + 3 ) & ~size_t( 3 ), 0 );
		ab.resize( bytes.size() / 4 );
		std::memcpy( ab.data(), bytes.data(), bytes.size() );
		for ( const probebvh::AlphaMask::Tri & t : m->tris ) {
			uint32_t w[12];
			w[0] = uint32_t( t.map );
			w[1] = t.thr;
			std::memcpy( &w[2], t.uv, 24 );
			std::memcpy( &w[8], t.as, 12 );
			w[11] = t.scaled() ? 1u : 0u;
			mt.insert( mt.end(), w, w + 12 );
		}
	}
	const VkBufferUsageFlags u = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
	return upload( D, triOf.data(), triOf.size() * 4, u, I.triOf ) && upload( D, mt.data(), mt.size() * 4, u, I.maskTris )
		&& upload( D, maps.data(), maps.size() * 4, u, I.maps ) && upload( D, ab.data(), ab.size() * 4, u, I.alpha );
}

bool buildCompute( Dev & D, const probebvh::Bvh & bvh, Scene::Impl & I )
{
	std::vector<uint32_t> nd( bvh.nodes.size() * 9 );
	for ( size_t i = 0; i < bvh.nodes.size(); i++ ) {
		const probebvh::Bvh::Node & n = bvh.nodes[i];
		std::memcpy( &nd[i * 9], n.lo, 12 );
		std::memcpy( &nd[i * 9 + 3], n.hi, 12 );
		nd[i * 9 + 6] = uint32_t( n.left );
		nd[i * 9 + 7] = uint32_t( n.first );
		nd[i * 9 + 8] = uint32_t( n.count );
	}
	const VkBufferUsageFlags u = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
	return upload( D, nd.data(), nd.size() * 4, u, I.nodes ) && upload( D, bvh.idx.data(), bvh.idx.size() * 4, u, I.idx )
		&& upload( D, bvh.t.data(), bvh.t.size() * 4, u, I.tris );
}

bool buildRt( Dev & D, const probebvh::Bvh & bvh, Scene::Impl & I )
{
	// the opaque triangles first (geometry 0), then the alpha-tested ones (geometry 1); remap = their soup numbers
	const probebvh::AlphaMask * m = I.useMask ? bvh.mask : nullptr;
	auto masked = [&]( uint32_t t ) { return m && t < m->triOf.size() && m->triOf[t] >= 0; };
	std::vector<int32_t> remap;
	remap.reserve( I.nTri );
	for ( uint32_t t = 0; t < I.nTri; t++ )
		if ( !masked( t ) )
			remap.push_back( int32_t( t ) );
	I.nOpaque = uint32_t( remap.size() );
	for ( uint32_t t = 0; t < I.nTri; t++ )
		if ( masked( t ) )
			remap.push_back( int32_t( t ) );
	std::vector<float> v( size_t( I.nTri ) * 9 );
	for ( size_t i = 0; i < remap.size(); i++ )
		std::memcpy( &v[i * 9], &bvh.t[size_t( remap[i] ) * 9], 36 );
	const VkBufferUsageFlags in = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR
		| VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
	if ( !upload( D, v.data(), v.size() * 4, in, I.verts )
		|| !upload( D, remap.data(), remap.size() * 4, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, I.remap ) )
		return false;
	std::vector<float>().swap( v );

	VkAccelerationStructureGeometryKHR g[2] {};
	uint32_t counts[2] = { I.nOpaque, I.nTri - I.nOpaque };
	VkAccelerationStructureBuildRangeInfoKHR ranges[2] {};
	uint32_t ng = 0;
	for ( int k = 0; k < 2; k++ ) {
		if ( !counts[k] )
			continue;
		VkAccelerationStructureGeometryKHR & x = g[ng];
		x.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
		x.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
		x.flags = k == 0 ? VK_GEOMETRY_OPAQUE_BIT_KHR : 0;
		auto & tr = x.geometry.triangles;
		tr.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
		tr.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
		tr.vertexData.deviceAddress = I.verts.addr + ( k ? VkDeviceAddress( I.nOpaque ) * 36 : 0 );
		tr.vertexStride = 12;
		tr.maxVertex = counts[k] * 3 - 1;
		tr.indexType = VK_INDEX_TYPE_NONE_KHR;
		ranges[ng].primitiveCount = counts[k];
		ng++;
	}
	if ( !ng )
		return false;
	const uint32_t maxPrims[2] = { ranges[0].primitiveCount, ranges[1].primitiveCount };
	VkAccelerationStructureBuildGeometryInfoKHR bi { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR };
	bi.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
	bi.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
	bi.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
	bi.geometryCount = ng;
	bi.pGeometries = g;
	VkAccelerationStructureBuildSizesInfoKHR bs { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR };
	vk.vkGetAccelerationStructureBuildSizesKHR( D.dev, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &bi, maxPrims, &bs );
	const VkBufferUsageFlags asu = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
	if ( !makeBuf( D, bs.accelerationStructureSize, asu, { kDevLocal, kHost }, I.blasBuf ) )
		return false;
	VkAccelerationStructureCreateInfoKHR ci { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR };
	ci.buffer = I.blasBuf.b;
	ci.size = bs.accelerationStructureSize;
	ci.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
	if ( vk.vkCreateAccelerationStructureKHR( D.dev, &ci, nullptr, &I.blas ) != VK_SUCCESS )
		return false;
	VkAccelerationStructureDeviceAddressInfoKHR ai { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR };
	ai.accelerationStructure = I.blas;
	const VkDeviceAddress blasAddr = vk.vkGetAccelerationStructureDeviceAddressKHR( D.dev, &ai );

	// the TLAS: one instance, identity, double-sided
	VkAccelerationStructureInstanceKHR inst {};
	inst.transform.matrix[0][0] = inst.transform.matrix[1][1] = inst.transform.matrix[2][2] = 1.0f;
	inst.mask = 0xFF;
	inst.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
	inst.accelerationStructureReference = blasAddr;
	Buf ib;
	if ( !makeBuf( D, sizeof inst, in, { kHost }, ib ) )
		return false;
	std::memcpy( ib.map, &inst, sizeof inst );
	VkAccelerationStructureGeometryKHR tg { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR };
	tg.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
	tg.geometry.instances.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
	tg.geometry.instances.data.deviceAddress = ib.addr;
	VkAccelerationStructureBuildGeometryInfoKHR ti { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR };
	ti.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
	ti.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
	ti.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
	ti.geometryCount = 1;
	ti.pGeometries = &tg;
	const uint32_t one = 1;
	VkAccelerationStructureBuildSizesInfoKHR ts { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR };
	vk.vkGetAccelerationStructureBuildSizesKHR( D.dev, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &ti, &one, &ts );
	bool ok = makeBuf( D, ts.accelerationStructureSize, asu, { kDevLocal, kHost }, I.tlasBuf );
	if ( ok ) {
		VkAccelerationStructureCreateInfoKHR tci { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR };
		tci.buffer = I.tlasBuf.b;
		tci.size = ts.accelerationStructureSize;
		tci.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
		ok = vk.vkCreateAccelerationStructureKHR( D.dev, &tci, nullptr, &I.tlas ) == VK_SUCCESS;
	}
	Buf scratch;
	const VkDeviceSize sz = std::max( bs.buildScratchSize, ts.buildScratchSize ) + D.scratchAlign;
	ok = ok && makeBuf( D, sz, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, { kDevLocal, kHost }, scratch );
	if ( ok ) {
		const VkDeviceAddress sa = ( scratch.addr + D.scratchAlign - 1 ) / D.scratchAlign * D.scratchAlign;
		bi.dstAccelerationStructure = I.blas;
		bi.scratchData.deviceAddress = sa;
		ti.dstAccelerationStructure = I.tlas;
		ti.scratchData.deviceAddress = sa;
		VkAccelerationStructureBuildRangeInfoKHR tr {};
		tr.primitiveCount = 1;
		const VkAccelerationStructureBuildRangeInfoKHR * pr = ranges;
		const VkAccelerationStructureBuildRangeInfoKHR * ptr = &tr;
		ok = begin( D );
		if ( ok ) {
			vk.vkCmdBuildAccelerationStructuresKHR( D.cb, 1, &bi, &pr );
			VkMemoryBarrier mb { VK_STRUCTURE_TYPE_MEMORY_BARRIER };
			mb.srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
			mb.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR | VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
			vk.vkCmdPipelineBarrier( D.cb, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
				VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, 0, 1, &mb, 0, nullptr, 0, nullptr );
			vk.vkCmdBuildAccelerationStructuresKHR( D.cb, 1, &ti, &ptr );
			ok = submitWait( D );
		}
	}
	freeBuf( D, scratch );
	freeBuf( D, ib );
	return ok;
}

bool writeSet( Dev & D, Scene::Impl & I )
{
	VkDescriptorSetAllocateInfo ai { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
	ai.descriptorPool = D.dpool;
	ai.descriptorSetCount = 1;
	ai.pSetLayouts = &D.dsl[I.which];
	if ( vk.vkAllocateDescriptorSets( D.dev, &ai, &I.ds ) != VK_SUCCESS )
		return false;
	std::vector<const Buf *> bs = { &D.rays, &D.hits, &I.triOf, &I.maskTris, &I.maps, &I.alpha };
	if ( I.which )
		bs.insert( bs.end(), { nullptr, &I.remap } );
	else
		bs.insert( bs.end(), { &I.nodes, &I.idx, &I.tris } );
	std::vector<VkDescriptorBufferInfo> bi( bs.size() );
	std::vector<VkWriteDescriptorSet> w;
	VkWriteDescriptorSetAccelerationStructureKHR wa { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR };
	wa.accelerationStructureCount = 1;
	wa.pAccelerationStructures = &I.tlas;
	for ( size_t i = 0; i < bs.size(); i++ ) {
		VkWriteDescriptorSet x { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
		x.dstSet = I.ds;
		x.dstBinding = uint32_t( i );
		x.descriptorCount = 1;
		if ( !bs[i] ) {
			x.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
			x.pNext = &wa;
		} else {
			x.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			bi[i] = { bs[i]->b, 0, VK_WHOLE_SIZE };
			x.pBufferInfo = &bi[i];
		}
		w.push_back( x );
	}
	vk.vkUpdateDescriptorSets( D.dev, uint32_t( w.size() ), w.data(), 0, nullptr );
	return true;
}

} // namespace

std::unique_ptr<Scene> Scene::make( const probebvh::Bvh & bvh, std::string * log )
{
	std::unique_ptr<Scene> s( new Scene() );
	s->bvh_ = &bvh;
	std::string why;
	Path p = pickPath( &why );
	if ( bvh.t.empty() || bvh.nodes.empty() )
		p = Path::Cpu;
	if ( p != Path::Cpu ) {
		Dev & D = dev();
		std::lock_guard<std::mutex> lk( D.m );
		const auto t0 = std::chrono::steady_clock::now();
		Impl * I = new Impl();
		I->which = p == Path::Rt ? 1 : 0;
		bool ok = buildMasks( D, bvh, *I ) && ( I->which ? buildRt( D, bvh, *I ) : buildCompute( D, bvh, *I ) ) && writeSet( D, *I );
		if ( !ok ) {
			I->release( D );
			delete I;
			why += std::string( "; the " ) + pathName( p ) + " scene did not build (out of GPU memory?), cpu";
			p = Path::Cpu;
		} else {
			s->d_ = I;
			s->gpuUs += std::chrono::duration_cast<std::chrono::microseconds>( std::chrono::steady_clock::now() - t0 ).count();
		}
	}
	s->path_ = p;
	if ( log ) {
		std::ostringstream o;
		o << pathName( p ) << ( p != Path::Cpu ? " (" + deviceName() + ")" : std::string() ) << ", " << why
		  << ", " << bvh.t.size() / 9 << " triangles";
		*log = o.str();
	}
	return s;
}

Scene::~Scene()
{
	if ( d_ ) {
		Dev & D = dev();
		std::lock_guard<std::mutex> lk( D.m );
		d_->release( D );
		delete d_;
	}
}

void Scene::trace( const Ray * rays, size_t n, Hit * hits, bool anyHit )
{
	traced += (long long)n;
	if ( path_ == Path::Cpu || !d_ ) {
		std::atomic<size_t> next( 0 );
		std::vector<std::thread> pool;
		const size_t step = 4096;
		for ( int i = 0; i < threads(); i++ )
			pool.emplace_back( [&]() {
				for ( size_t b; ( b = next.fetch_add( step ) ) < n; )
					for ( size_t k = b; k < std::min( n, b + step ); k++ ) {
						const Ray & r = rays[k];
						const double o[3] = { r.o[0], r.o[1], r.o[2] }, d[3] = { r.d[0], r.d[1], r.d[2] };
						double t = 0;
						int tri = -1;
						const bool h = bvh_->ray( o, d, r.tmax, &t, &tri ) && tri >= 0;
						hits[k] = Hit { h ? float( t ) : 0.0f, h ? tri : -1 };
					}
			} );
		for ( std::thread & t : pool )
			t.join();
		return;
	}
	Dev & D = dev();
	std::lock_guard<std::mutex> lk( D.m );
	const auto t0 = std::chrono::steady_clock::now();
	// the compute walk is slower a ray: smaller submits keep each one far under the driver's 2 s watchdog
	const size_t cap = d_->which ? D.cap : D.cap / 4;
	for ( size_t b = 0; b < n; b += cap ) {
		const size_t m = std::min( cap, n - b );
		std::memcpy( D.rays.map, rays + b, m * sizeof( Ray ) );
		bool ok = begin( D );
		if ( ok ) {
			vk.vkCmdBindPipeline( D.cb, VK_PIPELINE_BIND_POINT_COMPUTE, D.pipe[d_->which] );
			vk.vkCmdBindDescriptorSets( D.cb, VK_PIPELINE_BIND_POINT_COMPUTE, D.pl[d_->which], 0, 1, &d_->ds, 0, nullptr );
			const uint32_t pcv[6] = { 0u, uint32_t( m ), d_->useMask, d_->nTri, d_->nOpaque, anyHit ? 1u : 0u };
			vk.vkCmdPushConstants( D.cb, D.pl[d_->which], VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof pcv, pcv );
			const uint32_t local = d_->which ? 128u : 64u;
			vk.vkCmdDispatch( D.cb, uint32_t( ( m + local - 1 ) / local ), 1, 1 );
			VkMemoryBarrier mb { VK_STRUCTURE_TYPE_MEMORY_BARRIER };
			mb.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
			mb.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
			vk.vkCmdPipelineBarrier( D.cb, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0, 1, &mb, 0, nullptr, 0, nullptr );
			ok = submitWait( D );
		}
		if ( !ok ) {   // a lost device: this batch and the rest on the CPU (each hit says so through refine's re-cast)
			for ( size_t k = b; k < n; k++ )
				hits[k] = Hit { 0.0f, -2 };
			break;
		}
		std::memcpy( hits + b, D.hits.map, m * sizeof( Hit ) );
	}
	gpuUs += std::chrono::duration_cast<std::chrono::microseconds>( std::chrono::steady_clock::now() - t0 ).count();
}

bool Scene::refine( const Hit & h, const double o[3], const double d[3], double tmax, double * t, int * tri, bool exact ) const
{
	if ( h.tri == -1 )
		return false;
	if ( h.tri >= 0 && size_t( h.tri ) < bvh_->t.size() / 9 ) {
		double tt, u, v;
		const bool useMask = bvh_->mask && !bvh_->mask->empty();
		if ( probebvh::Bvh::tri( &bvh_->t[size_t( h.tri ) * 9], o, d, tt, u, v ) && tt > 1e-4 && tt <= tmax
			&& !( useMask && bvh_->mask->hole( h.tri, u, v ) ) ) {
			*t = tt;
			*tri = h.tri;
			/* nearest: the CPU walk bounded at that t returns the CPU's own answer (its nearest is no farther; the
			 * pruning drops only nodes past it, so ties resolve in the same order), at a fraction of a full ray */
			if ( exact && !( bvh_->ray( o, d, tt, t, tri ) && *tri >= 0 ) ) {
				*t = tt;
				*tri = h.tri;
			}
			return true;
		}
	}
	const_cast<Scene *>( this )->recast++;
	*tri = -1;
	return bvh_->ray( o, d, tmax, t, tri ) && *tri >= 0;
}

std::string census( const char * who, const Scene & s )
{
	std::ostringstream o;
	o << who << " rays: " << pathName( s.path() );
	if ( s.path() != Path::Cpu )
		o << " (" << deviceName() << ")";
	o << ", traced " << s.traced.load() << ", re-cast on the CPU " << s.recast.load() << ", GPU " << s.gpuUs.load() / 1000 << " ms";
	return o.str();
}

} // namespace bakert
