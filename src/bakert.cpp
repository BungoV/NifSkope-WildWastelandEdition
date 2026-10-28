/* Lane RTBAKE1 (2026-10-21): the bakes' shared ray back end (see bakert.h). A headless Vulkan compute device: no
 * window, no GL interop. vulkan-1.dll is loaded at run time, so the exe starts (and bakes on the CPU) without it. */

#include "bakert.h"
#include "bakertwalk.h"
#include "probebvh.h"
#include "harnesswindow.h"   // wwHarnessSettingsSuffix
#include "version.h"

#include <QSettings>

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
#include <cmath>
#include <cstdio>
#include <functional>
#include <cstring>
#include <mutex>
#include <sstream>

namespace {
#include "bakert/rq.spv.h"
#include "bakert/bvh.spv.h"
#include "bakert/rqx.spv.h"
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
	bool ok = false, rt = false, x = false;   // x: lane RTGI1's whole-ray tracer (rt + shaderFloat64)
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
	VkDescriptorSetLayout dsl[3] = { VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE };   // [0] compute, [1] rt, [2] rqx
	VkPipelineLayout pl[3] = { VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE };
	VkPipeline pipe[3] = { VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE };
	VkDescriptorPool dpool = VK_NULL_HANDLE;
	VkDeviceSize scratchAlign = 256;
	Buf rays, hits;            // one batch's rays and hits, shared by every Scene (under m)
	Buf xrays, xhits;          // lane RTGI1: the whole-ray batch (XRay, uvec2)
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
	const int n = which == 1 ? 8 : 9;
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
	VkPushConstantRange pr { VK_SHADER_STAGE_COMPUTE_BIT, 0, 32 };   // ROOMFIT10: rqx.comp's grow + pad
	VkPipelineLayoutCreateInfo pi { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
	pi.setLayoutCount = 1;
	pi.pSetLayouts = &D.dsl[which];
	pi.pushConstantRangeCount = 1;
	pi.pPushConstantRanges = &pr;
	if ( vk.vkCreatePipelineLayout( D.dev, &pi, nullptr, &D.pl[which] ) != VK_SUCCESS )
		return false;
	D.pipe[which] = which == 2 ? pipeline( D, bakert_rqx_spv, sizeof bakert_rqx_spv, D.pl[2] )
		: which ? pipeline( D, bakert_rq_spv, sizeof bakert_rq_spv, D.pl[1] )
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
	bool bestRt = false, bestF64 = false;
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
			bestF64 = f2.features.shaderFloat64 == VK_TRUE;
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
	// lane RTGI1: the whole-ray tracer re-tests in double (WW_BAKE_RT_WHOLE=0 keeps RTBAKE1's trace + refine)
	const char * wh = std::getenv( "WW_BAKE_RT_WHOLE" );
	const bool wantX = wantRt && bestF64 && !( wh && !std::strcmp( wh, "0" ) );
	VkPhysicalDeviceFeatures2 f2on { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2 };
	f2on.features.shaderFloat64 = wantX ? VK_TRUE : VK_FALSE;
	f2on.pNext = &v12;
	const char * exts[] = { VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME, VK_KHR_RAY_QUERY_EXTENSION_NAME,
		VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME };
	VkDeviceCreateInfo di { VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
	di.pNext = &f2on;
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
	VkDescriptorPoolSize ps[2] = { { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 128 * 9 }, { VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 128 } };
	VkDescriptorPoolCreateInfo dpi { VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
	dpi.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
	dpi.maxSets = 128;
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
	if ( wantX && !layout( D, 2 ) ) {   // no whole-ray tracer: the bakes keep trace + refine, and say so
		D.why = "the whole-ray tracer did not build";
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
	// lane RTGI1: the whole-ray batch, 1M rays (64 MB) and their answers
	D.x = wantX && D.pipe[2] != VK_NULL_HANDLE
		&& makeBuf( D, D.cap * sizeof( XRay ), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, { kHost | kDevLocal, kHost }, D.xrays )
		&& makeBuf( D, D.cap * 8, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, { kHost | VK_MEMORY_PROPERTY_HOST_CACHED_BIT, kHost }, D.xhits );
	if ( wantX && !D.x && D.why.empty() )
		D.why = "no memory for the whole-ray batch";
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

const char * const kBakeRtSettingKey = "Settings/Nif/Gpu Ray Traced Bake";

bool gpuWanted( std::string * why )
{
	const char * e = std::getenv( "WW_BAKE_RT" );
	if ( e && *e ) {
		if ( why )
			*why = std::string( "WW_BAKE_RT=" ) + e;
		return std::strcmp( e, "0" ) != 0;
	}
	// the GUI's settings file, also from the CLI (as lodgenGpuSettingEnabled reads Use GPU)
	QSettings s( QStringLiteral( "NifTools" ),
		QStringLiteral( "NifSkope " ) + NifSkopeVersion::rawToMajMin( NIFSKOPE_VERSION ) + wwHarnessSettingsSuffix() );
	const bool on = s.value( QLatin1String( kBakeRtSettingKey ), false ).toBool();
	if ( why )
		*why = on ? "setting GPU Ray-Traced Bake on" : "setting GPU Ray-Traced Bake off";
	return on;
}

Path pickPath( std::string * why, bool fastest )
{
	/* the CPU bake is the default: no Vulkan is touched unless the GPU bake is asked for (the off path is the bakes'
	 * own loops, byte-identical to before RTBAKE1) */
	std::string want;
	const char * pin = std::getenv( "WW_BAKE_RT" );
	if ( fastest && !( pin && !std::strcmp( pin, "0" ) ) ) {
		want = "fastest path (exact answers)";
	} else if ( !gpuWanted( &want ) ) {
		if ( why )
			*why = want;
		return Path::Cpu;
	}
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
		*why = want + ( r.empty() ? std::string( "; picked" ) : "; red " + r );
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
	Buf vertsBlas;                      // lane GIBAKE2: the grown triangles the BLAS is built from (whole rays only)
	double inflate = 0.0;               // how far each edge moved out (0 = the true triangles)
	long long inflateTris = 0, inflateCapped = 0;
	VkAccelerationStructureKHR blas = VK_NULL_HANDLE, tlas = VK_NULL_HANDLE;
	Buf nodes, idx, tris;               // compute
	VkDescriptorSet ds = VK_NULL_HANDLE;
	VkDescriptorSet dsx = VK_NULL_HANDLE;   // lane RTGI1: the whole-ray tracer's set (rt only)
	uint32_t nTri = 0, nOpaque = 0, useMask = 0;
	int which = 0;                      // 0 compute, 1 rt

	void release( Dev & D )
	{
		if ( ds )
			vk.vkFreeDescriptorSets( D.dev, D.dpool, 1, &ds );
		if ( dsx )
			vk.vkFreeDescriptorSets( D.dev, D.dpool, 1, &dsx );
		if ( tlas )
			vk.vkDestroyAccelerationStructureKHR( D.dev, tlas, nullptr );
		if ( blas )
			vk.vkDestroyAccelerationStructureKHR( D.dev, blas, nullptr );
		for ( Buf * b : { &triOf, &maskTris, &maps, &alpha, &verts, &vertsBlas, &remap, &blasBuf, &tlasBuf, &nodes, &idx, &tris } )
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
	// lane RTGI1: the whole-ray tracer reads the vertices too (its double re-test)
	if ( !upload( D, v.data(), v.size() * 4, in | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, I.verts )
		|| !upload( D, remap.data(), remap.size() * 4, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, I.remap ) )
		return false;
	/* lane GIBAKE2 (RTGI1-CONCORD): the float hardware can miss a triangle the double re-test accepts near its edge,
	 * with no neighbour reported to make the ray unsure. The BLAS takes each triangle grown in its own plane by the
	 * re-test's error bound, so every such triangle is reported; rqx.comp re-tests the true ones (binding 8). */
	static const bool inflateRed = [] { const char * r = std::getenv( "WW_BAKE_RT_INFLATE_RED" ); return r && !std::strcmp( r, "off" ); }();
	if ( D.x && !inflateRed && !v.empty() ) {
		double lo[3] = { 1e300, 1e300, 1e300 }, hi[3] = { -1e300, -1e300, -1e300 };
		for ( size_t i = 0; i < v.size(); i++ ) {
			lo[i % 3] = std::min( lo[i % 3], double( v[i] ) );
			hi[i % 3] = std::max( hi[i % 3], double( v[i] ) );
		}
		double mag = 0.0, diag2 = 0.0;
		for ( int k = 0; k < 3; k++ ) {
			mag = std::max( mag, std::max( std::abs( lo[k] ), std::abs( hi[k] ) ) );
			diag2 += ( hi[k] - lo[k] ) * ( hi[k] - lo[k] );
		}
		const double del = 4e-6 * ( mag + std::sqrt( diag2 ) + 1.0 );   // rqx.comp's kErr x (|o| + t + 1), t <= diag
		I.inflate = del;
		std::vector<float> g( v.size() );
		int capped = 0;
		for ( size_t t = 0; t < size_t( I.nTri ); t++ )
			I.inflateTris += growTri( &v[t * 9], del, &g[t * 9], &capped ) ? 1 : 0;   // src/bakertwalk.h
		I.inflateCapped = capped;
		if ( !upload( D, g.data(), g.size() * 4, in, I.vertsBlas ) )
			return false;
	}
	std::vector<float>().swap( v );
	const VkDeviceAddress blasVerts = I.vertsBlas.addr ? I.vertsBlas.addr : I.verts.addr;

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
		tr.vertexData.deviceAddress = blasVerts + ( k ? VkDeviceAddress( I.nOpaque ) * 36 : 0 );
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

bool writeSet( Dev & D, Scene::Impl & I, int which )
{
	VkDescriptorSetAllocateInfo ai { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
	ai.descriptorPool = D.dpool;
	ai.descriptorSetCount = 1;
	ai.pSetLayouts = &D.dsl[which];
	VkDescriptorSet & set = which == 2 ? I.dsx : I.ds;
	if ( vk.vkAllocateDescriptorSets( D.dev, &ai, &set ) != VK_SUCCESS )
		return false;
	std::vector<const Buf *> bs = { which == 2 ? &D.xrays : &D.rays, which == 2 ? &D.xhits : &D.hits, &I.triOf,
		&I.maskTris, &I.maps, &I.alpha };
	if ( which == 2 )
		bs.insert( bs.end(), { nullptr, &I.remap, &I.verts } );
	else if ( which )
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
		x.dstSet = set;
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

std::unique_ptr<Scene> Scene::make( const probebvh::Bvh & bvh, std::string * log, bool fastest )
{
	std::unique_ptr<Scene> s( new Scene() );
	s->bvh_ = &bvh;
	std::string why;
	Path p = pickPath( &why, fastest );
	if ( bvh.t.empty() || bvh.nodes.empty() )
		p = Path::Cpu;
	if ( p != Path::Cpu ) {
		Dev & D = dev();
		std::lock_guard<std::mutex> lk( D.m );
		const auto t0 = std::chrono::steady_clock::now();
		Impl * I = new Impl();
		I->which = p == Path::Rt ? 1 : 0;
		bool ok = buildMasks( D, bvh, *I ) && ( I->which ? buildRt( D, bvh, *I ) : buildCompute( D, bvh, *I ) )
			&& writeSet( D, *I, I->which );
		if ( ok && I->which && D.x && !writeSet( D, *I, 2 ) )   // lane RTGI1: no set, no whole rays (trace + refine)
			I->dsx = VK_NULL_HANDLE;
		if ( !ok ) {
			I->release( D );
			delete I;
			why += std::string( "; the " ) + pathName( p ) + " scene did not build (out of GPU memory?), cpu";
			p = Path::Cpu;
		} else {
			s->d_ = I;
			s->inflate = I->inflate;
			s->inflateTris = I->inflateTris;
			s->inflateCapped = I->inflateCapped;
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
	// an occlusion miss stays a miss; a nearest-hit miss is re-cast below (the float test can miss a CPU hit)
	if ( h.tri == -1 && !exact )
		return false;
	if ( h.tri >= 0 && size_t( h.tri ) < bvh_->t.size() / 9 ) {
		double tt, u, v;
		const bool useMask = bvh_->mask && !bvh_->mask->empty();
		if ( probebvh::Bvh::tri( &bvh_->t[size_t( h.tri ) * 9], o, d, tt, u, v ) && tt > 1e-4 && tt <= tmax
			&& !( useMask && bvh_->mask->hole( h.tri, u, v ) ) ) {
			*t = tt;
			*tri = h.tri;
			if ( !exact )
				return true;
			/* nearest: the CPU walk from that t gives the full walk's answer unless it left out a node or triangle a
			 * hair past its best (bakertwalk.h); then, or when it finds nothing, the full re-cast below decides.
			 * WW_BAKE_RT_WALK_RED=bounded: the walk bounded at t alone (its near ties differ from the CPU bake's). */
			static const bool redBounded = [] { const char * r = std::getenv( "WW_BAKE_RT_WALK_RED" ); return r && !std::strcmp( r, "bounded" ); }();
			if ( redBounded ) {
				if ( bvh_->ray( o, d, tt, t, tri ) && *tri >= 0 )
					return true;
			} else {
				bool sure = true;
				const double from = std::min( tmax, tt + kNearPad * ( 1.0 + tt ) );
				if ( rayFrom( *bvh_, o, d, from, t, tri, &sure ) && sure && *tri >= 0 )
					return true;
			}
		}
	}
	const_cast<Scene *>( this )->recast++;
	*tri = -1;
	return bvh_->ray( o, d, tmax, t, tri ) && *tri >= 0;
}

bool Scene::whole() const
{
	return path_ == Path::Rt && d_ && d_->dsx != VK_NULL_HANDLE;
}

CastTimes & castTimes()
{
	thread_local CastTimes t;
	return t;
}

namespace {

// n jobs over the bake pool, in blocks
void poolFor( size_t n, const std::function<void( size_t, size_t )> & fn )
{
	const size_t step = 2048;
	std::atomic<size_t> next( 0 );
	std::vector<std::thread> pool;
	const int nt = int( std::min<size_t>( size_t( threads() ), ( n + step - 1 ) / step ) );
	for ( int i = 0; i < nt; i++ )
		pool.emplace_back( [&]() {
			for ( size_t b; ( b = next.fetch_add( step ) ) < n; )
				fn( b, std::min( n, b + step ) );
		} );
	for ( std::thread & t : pool )
		t.join();
}

} // namespace

void Scene::cast( const XRay * rays, size_t n, XHit * out, bool anyHit )
{
	static const bool check = [] { const char * c = std::getenv( "WW_BAKE_RT_CHECK" ); return c && *c && std::strcmp( c, "0" ); }();
	const probebvh::Bvh & B = *bvh_;
	const bool useMask = B.mask && !B.mask->empty();
	// the CPU's own answer: the full walk (a boolean for an occlusion ray, its t and tri kept all the same)
	auto walk = [&]( const XRay & r, XHit & h ) {
		h.tri = -1;
		h.t = 0.0;
		if ( !( B.ray( r.o, r.d, r.tmax, &h.t, &h.tri ) && h.tri >= 0 ) )
			h.tri = -1;
	};
	if ( !whole() ) {   // the CPU walk, or RTBAKE1's trace + refine
		if ( path_ == Path::Cpu || !d_ ) {
			traced += (long long)n;
			poolFor( n, [&]( size_t b, size_t e ) {
				for ( size_t k = b; k < e; k++ )
					walk( rays[k], out[k] );
			} );
			return;
		}
		std::vector<Ray> fr( n );
		std::vector<Hit> fh( n );
		for ( size_t k = 0; k < n; k++ ) {
			const XRay & r = rays[k];
			fr[k] = { { float( r.o[0] ), float( r.o[1] ), float( r.o[2] ) }, 1e-4f, { float( r.d[0] ), float( r.d[1] ), float( r.d[2] ) },
				float( r.tmax ) };
		}
		trace( fr.data(), n, fh.data(), anyHit );
		poolFor( n, [&]( size_t b, size_t e ) {
			for ( size_t k = b; k < e; k++ ) {
				XHit & h = out[k];
				h.t = 0.0;
				h.tri = -1;
				if ( !refine( fh[k], rays[k].o, rays[k].d, rays[k].tmax, &h.t, &h.tri, !anyHit ) )
					h.tri = -1;
			}
		} );
		return;
	}
	traced += (long long)n;
	Dev & D = dev();
	CastTimes & ct = castTimes();
	ct.calls++;
	std::vector<uint32_t> ans( std::min( n, D.cap ) * 2 );
	/* lane ROOMFIT10: rqx.comp narrows its edge band to the double rounding on a grown BLAS (pc.grow = the growth).
	 * Red WW_BAKE_RT_SURE_RED: `float` keeps RTGI1's float band (grow 0); `ungrown` forces the narrow band on a BLAS
	 * that was not grown (with WW_BAKE_RT_INFLATE_RED=off: WW_BAKE_RT_CHECK=1 must then find unlike answers). */
	static const int sureRed = [] {
		const char * r = std::getenv( "WW_BAKE_RT_SURE_RED" );
		return !r ? 0 : !std::strcmp( r, "float" ) ? 1 : !std::strcmp( r, "ungrown" ) ? 2 : 0;
	}();
	const float growPc = sureRed == 1 ? 0.0f : sureRed == 2 ? 1e30f : float( inflate );
	uint32_t growBits;
	std::memcpy( &growBits, &growPc, 4 );
	/* lane ROOMFIT10: an unsure ray re-walks bounded at its GPU triangle's t. Red WW_BAKE_RT_TIE_RED: `full` walks
	 * every unsure ray whole (the old pass); `short` bounds just below that t (WW_BAKE_RT_CHECK=1 must then differ). */
	static const int tieRed = [] {
		const char * r = std::getenv( "WW_BAKE_RT_TIE_RED" );
		return !r ? 0 : !std::strcmp( r, "full" ) ? 1 : !std::strcmp( r, "short" ) ? 2 : 0;
	}();
	// lane ROOMFIT10: red WW_BAKE_RT_GRAZE_RED=off: phase 2 reaches only kErr past the committed t (rqx.comp kGraze)
	static const uint32_t grazeOff = [] { const char * r = std::getenv( "WW_BAKE_RT_GRAZE_RED" ); return r && !std::strcmp( r, "off" ) ? 2u : 0u; }();
	for ( size_t b0 = 0; b0 < n; b0 += D.cap ) {
		const size_t m = std::min( D.cap, n - b0 );
		bool ok;
		{
			const auto tw = std::chrono::steady_clock::now();
			std::lock_guard<std::mutex> lk( D.m );
			const auto t0 = std::chrono::steady_clock::now();
			ct.lockNs += std::chrono::duration_cast<std::chrono::nanoseconds>( t0 - tw ).count();
			std::memcpy( D.xrays.map, rays + b0, m * sizeof( XRay ) );
			ok = begin( D );
			if ( ok ) {
				vk.vkCmdBindPipeline( D.cb, VK_PIPELINE_BIND_POINT_COMPUTE, D.pipe[2] );
				vk.vkCmdBindDescriptorSets( D.cb, VK_PIPELINE_BIND_POINT_COMPUTE, D.pl[2], 0, 1, &d_->dsx, 0, nullptr );
				const uint32_t pcv[8] = { 0u, uint32_t( m ), d_->useMask, d_->nTri, d_->nOpaque, ( anyHit ? 1u : 0u ) | grazeOff, growBits, 0u };
				vk.vkCmdPushConstants( D.cb, D.pl[2],VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof pcv, pcv );
				vk.vkCmdDispatch( D.cb, uint32_t( ( m + 127 ) / 128 ), 1, 1 );
				VkMemoryBarrier mb { VK_STRUCTURE_TYPE_MEMORY_BARRIER };
				mb.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
				mb.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
				vk.vkCmdPipelineBarrier( D.cb, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0, 1, &mb, 0, nullptr, 0, nullptr );
				ok = submitWait( D );
			}
			if ( ok )
				std::memcpy( ans.data(), D.xhits.map, m * 8 );
			gpuUs += std::chrono::duration_cast<std::chrono::microseconds>( std::chrono::steady_clock::now() - t0 ).count();
			ct.gpuNs += std::chrono::duration_cast<std::chrono::nanoseconds>( std::chrono::steady_clock::now() - t0 ).count();
		}
		const auto tp = std::chrono::steady_clock::now();
		if ( !ok )   // a lost device: this batch walks on the CPU
			for ( size_t k = 0; k < m; k++ ) {
				ans[k * 2] = 0xFFFFFFFFu;
				ans[k * 2 + 1] = 32u;   // unsure: no answer
			}
		std::atomic<long long> nSure( 0 ), nRecast( 0 ), nDiff( 0 ), nBounded( 0 ), nChecked( 0 );
		poolFor( m, [&]( size_t b, size_t e ) {
			long long sr = 0, rc = 0, df = 0, bw = 0, ck = 0, wy[6] = { 0, 0, 0, 0, 0, 0 };
			for ( size_t k = b; k < e; k++ ) {
				const XRay & r = rays[b0 + k];
				XHit & h = out[b0 + k];
				const int gt = int( ans[k * 2] );
				for ( int w = 0; w < 6; w++ )   // ROOMFIT10: rqx.comp's reason bits
					wy[w] += ( ans[k * 2 + 1] >> w ) & 1u;
				bool done = false;
				if ( ans[k * 2 + 1] == 0u ) {
					if ( gt < 0 ) {   // a sure miss
						h.tri = -1;
						h.t = 0.0;
						done = true;
					} else if ( size_t( gt ) < B.t.size() / 9 ) {   // a sure hit: its t from probebvh's own test
						double tt, u, v;
						if ( probebvh::Bvh::tri( &B.t[size_t( gt ) * 9], r.o, r.d, tt, u, v ) && tt > 1e-4 && tt <= r.tmax
							&& !( useMask && B.mask->hole( gt, u, v ) ) ) {
							h.tri = gt;
							h.t = tt;
							done = true;
						}
					}
				}
				if ( !done ) {
					rc++;
					/* ROOMFIT10: an unsure ray whose GPU triangle probebvh's own test accepts (t tg): the nearest hit is at
					 * most tg, so the walk starts bounded there (padded far past the slab and triangle rounding). Its DFS
					 * order is fixed, and a box it prunes holds no triangle at <= the bound, so it keeps the full walk's
					 * last-visited triangle at the nearest t. An occlusion ray just takes that triangle. */
					double tg = 0.0, gu, gv;
					const bool own = tieRed != 1 && gt >= 0 && size_t( gt ) < B.t.size() / 9
						&& probebvh::Bvh::tri( &B.t[size_t( gt ) * 9], r.o, r.d, tg, gu, gv ) && tg > 1e-4 && tg <= r.tmax
						&& !( useMask && B.mask->hole( gt, gu, gv ) );
					if ( !own ) {
						walk( r, h );
						continue;
					}
					bw++;
					if ( anyHit ) {
						h.tri = gt;
						h.t = tg;
					} else {
						XRay rb = r;
						rb.tmax = tieRed == 2 ? tg * ( 1.0 - 1e-6 ) : std::min( r.tmax, tg * ( 1.0 + 1e-6 ) + 1e-4 );
						walk( rb, h );
					}
					if ( check ) {
						XHit c;
						walk( r, c );
						ck++;
						const bool same = anyHit ? ( c.tri >= 0 ) == ( h.tri >= 0 ) : ( c.tri == h.tri && ( c.tri < 0 || c.t == h.t ) );
						df += same ? 0 : 1;
						static std::atomic<int> shownB( 0 );
						if ( !same && shownB.fetch_add( 1 ) < 20 )
							fprintf( stderr, "rt check unlike (bounded)%s: o %.17g %.17g %.17g d %.17g %.17g %.17g tmax %.17g | bounded tri %d t %.17g | cpu tri %d t %.17g\n",
								anyHit ? " (any)" : "", r.o[0], r.o[1], r.o[2], r.d[0], r.d[1], r.d[2], r.tmax, h.tri, h.t, c.tri, c.t );
					}
					continue;
				}
				sr++;
				if ( check ) {
					ck++;
					XHit c;
					walk( r, c );
					const bool same = anyHit ? ( c.tri >= 0 ) == ( h.tri >= 0 ) : ( c.tri == h.tri && ( c.tri < 0 || c.t == h.t ) );
					df += same ? 0 : 1;
					static std::atomic<int> shown( 0 );   // lane GIBAKE2: the first unlike rays, for the edge/tie hunt
					if ( !same && shown.fetch_add( 1 ) < 20 )
						fprintf( stderr, "rt check unlike%s: o %.17g %.17g %.17g d %.17g %.17g %.17g tmax %.17g | gpu tri %d t %.17g | cpu tri %d t %.17g\n",
							anyHit ? " (any)" : "", r.o[0], r.o[1], r.o[2], r.d[0], r.d[1], r.d[2], r.tmax, h.tri, h.t, c.tri, c.t );
					if ( !same && c.tri >= 0 && shown.load() <= 20 ) {   // ROOMFIT10: the CPU's triangle, for the miss hunt
						const float * p = &B.t[size_t( c.tri ) * 9];
						fprintf( stderr, "rt check unlike cpu tri %d verts %.9g %.9g %.9g | %.9g %.9g %.9g | %.9g %.9g %.9g\n", c.tri,
							p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7], p[8] );
					}
				}
			}
			nSure += sr;
			nRecast += rc;
			nDiff += df;
			nBounded += bw;
			nChecked += ck;
			for ( int w = 0; w < 6; w++ )
				unsureWhy[w] += wy[w];
		} );
		sure += nSure.load();
		recast += nRecast.load();
		ct.recast += nRecast.load();
		bounded += nBounded.load();
		ct.postNs += std::chrono::duration_cast<std::chrono::nanoseconds>( std::chrono::steady_clock::now() - tp ).count();
		if ( check ) {
			checked += nChecked.load();
			diff += nDiff.load();
		}
	}
}

std::string census( const char * who, const Scene & s )
{
	std::ostringstream o;
	o << who << " rays: " << pathName( s.path() );
	if ( s.path() != Path::Cpu )
		o << " (" << deviceName() << ")";
	o << ", traced " << s.traced.load() << ", re-cast on the CPU " << s.recast.load() << ", GPU " << s.gpuUs.load() / 1000 << " ms";
	if ( s.sure.load() )
		o << ", whole: sure " << s.sure.load();
	if ( s.checked.load() )
		o << ", check: answers unlike the CPU's " << s.diff.load() << " of " << s.checked.load();
	if ( s.whole() )   // lane ROOMFIT10
		o << ", unsure: edge " << s.unsureWhy[0].load() << " hole " << s.unsureWhy[1].load() << " det " << s.unsureWhy[2].load()
		  << " tlimit " << s.unsureWhy[3].load() << " tie " << s.unsureWhy[4].load() << " none " << s.unsureWhy[5].load()
		  << ", re-cast bounded " << s.bounded.load();
	if ( s.whole() )   // lane GIBAKE2
		o << ", BLAS grown " << s.inflate << " u (" << s.inflateTris << " triangles, " << s.inflateCapped << " corners capped"
		  << ( s.inflate > 0.0 ? "" : "; off" ) << ")";
	return o.str();
}

} // namespace bakert
