// Lane VKCOMPUTE1 (2026-11-12): run a converted DDGI kernel and its GLSL original on the same buffers and compare every
// buffer byte for byte. No NifSkope: a headless Vulkan device of its own (the compute-only family when there is one),
// the DDGI set's layout (src/features/gi/rtddgi/ddgi_common.glsl: storage buffers 0-5, 7-14, the TLAS at 6 partially
// bound and never written), the dispatch split of rtddgi_vk.inc ddgiDispatch.
// Usage: kernelmatch <kernel> <glsl .spv> <hlsl .spv>   (kernel: border). Prints one line a case, then
// `HLSLMATCH <kernel> PASS n cases` or `HLSLMATCH <kernel> FAIL d of n cases`.
#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include <windows.h>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

#define VKF( name ) static PFN_##name name
VKF( vkCreateInstance ); VKF( vkEnumeratePhysicalDevices ); VKF( vkGetPhysicalDeviceQueueFamilyProperties );
VKF( vkGetPhysicalDeviceProperties ); VKF( vkGetPhysicalDeviceMemoryProperties ); VKF( vkCreateDevice ); VKF( vkGetDeviceQueue );
VKF( vkCreateBuffer ); VKF( vkGetBufferMemoryRequirements ); VKF( vkAllocateMemory ); VKF( vkBindBufferMemory ); VKF( vkMapMemory );
VKF( vkCreateDescriptorSetLayout ); VKF( vkCreatePipelineLayout ); VKF( vkCreateShaderModule ); VKF( vkCreateComputePipelines );
VKF( vkCreateDescriptorPool ); VKF( vkAllocateDescriptorSets ); VKF( vkUpdateDescriptorSets ); VKF( vkCreateCommandPool );
VKF( vkAllocateCommandBuffers ); VKF( vkBeginCommandBuffer ); VKF( vkEndCommandBuffer ); VKF( vkCmdBindPipeline );
VKF( vkCmdBindDescriptorSets ); VKF( vkCmdPushConstants ); VKF( vkCmdDispatch ); VKF( vkQueueSubmit ); VKF( vkQueueWaitIdle );
VKF( vkResetCommandBuffer ); VKF( vkDestroyPipeline ); VKF( vkDestroyShaderModule ); VKF( vkDestroyBuffer ); VKF( vkFreeMemory );

static std::vector<uint32_t> readSpv( const char * path )
{
	std::vector<uint32_t> v;
	FILE * f = std::fopen( path, "rb" );
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

struct Rng
{
	uint64_t s;
	uint32_t next() { s = s * 6364136223846793005ull + 1442695040888963407ull; return uint32_t( s >> 33 ); }
	float unit() { return float( next() & 0xFFFFFF ) / float( 0x1000000 ); }
};

struct Buf { VkBuffer b = VK_NULL_HANDLE; VkDeviceMemory m = VK_NULL_HANDLE; void * p = nullptr; size_t n = 0; };

static VkDevice dev;
static VkPhysicalDevice phys;
static VkQueue queue;
static uint32_t family;

static Buf makeBuf( size_t n )
{
	Buf B;
	B.n = n;
	VkBufferCreateInfo bi { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
	bi.size = n;
	bi.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
	vkCreateBuffer( dev, &bi, nullptr, &B.b );
	VkMemoryRequirements r;
	vkGetBufferMemoryRequirements( dev, B.b, &r );
	VkPhysicalDeviceMemoryProperties mp;
	vkGetPhysicalDeviceMemoryProperties( phys, &mp );
	const VkMemoryPropertyFlags want = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
	uint32_t t = 0;
	for ( ; t < mp.memoryTypeCount; t++ )
		if ( ( r.memoryTypeBits & ( 1u << t ) ) && ( mp.memoryTypes[t].propertyFlags & want ) == want )
			break;
	VkMemoryAllocateInfo ai { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
	ai.allocationSize = r.size;
	ai.memoryTypeIndex = t;
	vkAllocateMemory( dev, &ai, nullptr, &B.m );
	vkBindBufferMemory( dev, B.b, B.m, 0 );
	vkMapMemory( dev, B.m, 0, VK_WHOLE_SIZE, 0, &B.p );
	return B;
}

static void freeBuf( Buf & B )
{
	vkDestroyBuffer( dev, B.b, nullptr );
	vkFreeMemory( dev, B.m, nullptr );
	B = Buf();
}

// the DDGI set: bindings 0-14, 6 = the TLAS (partially bound, never written: no converted kernel reads it yet)
static VkDescriptorSetLayout setLayout;
static VkPipelineLayout pipeLayout;
static VkDescriptorPool pool;
static VkCommandPool cpool;
static VkCommandBuffer cb;

static bool makeLayout()
{
	VkDescriptorSetLayoutBinding b[15];
	VkDescriptorBindingFlags fl[15];
	for ( uint32_t i = 0; i < 15; i++ ) {
		b[i] = VkDescriptorSetLayoutBinding { i, i == 6 ? VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR : VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1,
			VK_SHADER_STAGE_COMPUTE_BIT, nullptr };
		fl[i] = i == 6 ? VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT : 0;
	}
	VkDescriptorSetLayoutBindingFlagsCreateInfo fi { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO };
	fi.bindingCount = 15;
	fi.pBindingFlags = fl;
	VkDescriptorSetLayoutCreateInfo li { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
	li.pNext = &fi;
	li.bindingCount = 15;
	li.pBindings = b;
	if ( vkCreateDescriptorSetLayout( dev, &li, nullptr, &setLayout ) != VK_SUCCESS )
		return false;
	VkPushConstantRange pr { VK_SHADER_STAGE_COMPUTE_BIT, 0, 64 };
	VkPipelineLayoutCreateInfo pi { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
	pi.setLayoutCount = 1;
	pi.pSetLayouts = &setLayout;
	pi.pushConstantRangeCount = 1;
	pi.pPushConstantRanges = &pr;
	if ( vkCreatePipelineLayout( dev, &pi, nullptr, &pipeLayout ) != VK_SUCCESS )
		return false;
	VkDescriptorPoolSize ps[2] = { { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 64 }, { VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 8 } };
	VkDescriptorPoolCreateInfo dp { VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
	dp.maxSets = 4;
	dp.poolSizeCount = 2;
	dp.pPoolSizes = ps;
	if ( vkCreateDescriptorPool( dev, &dp, nullptr, &pool ) != VK_SUCCESS )
		return false;
	VkCommandPoolCreateInfo ci { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
	ci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	ci.queueFamilyIndex = family;
	vkCreateCommandPool( dev, &ci, nullptr, &cpool );
	VkCommandBufferAllocateInfo ca { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
	ca.commandPool = cpool;
	ca.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	ca.commandBufferCount = 1;
	return vkAllocateCommandBuffers( dev, &ca, &cb ) == VK_SUCCESS;
}

// one dispatch of `spv` over bufs (index = binding; 6 unused), groups split as rtddgi_vk.inc ddgiDispatch
static bool run( const std::vector<uint32_t> & spv, std::vector<Buf> & bufs, const uint32_t pc[16], uint64_t groups )
{
	VkShaderModuleCreateInfo mi { VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
	mi.codeSize = spv.size() * 4;
	mi.pCode = spv.data();
	VkShaderModule sm;
	if ( vkCreateShaderModule( dev, &mi, nullptr, &sm ) != VK_SUCCESS )
		return false;
	VkComputePipelineCreateInfo ci { VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO };
	ci.stage = VkPipelineShaderStageCreateInfo { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
	ci.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	ci.stage.module = sm;
	ci.stage.pName = "main";
	ci.layout = pipeLayout;
	VkPipeline pipe;
	if ( vkCreateComputePipelines( dev, VK_NULL_HANDLE, 1, &ci, nullptr, &pipe ) != VK_SUCCESS )
		return false;
	VkDescriptorSetAllocateInfo da { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
	da.descriptorPool = pool;
	da.descriptorSetCount = 1;
	da.pSetLayouts = &setLayout;
	static VkDescriptorSet set = VK_NULL_HANDLE;
	if ( !set && vkAllocateDescriptorSets( dev, &da, &set ) != VK_SUCCESS )
		return false;
	std::vector<VkDescriptorBufferInfo> bi( 15 );
	std::vector<VkWriteDescriptorSet> w;
	for ( uint32_t i = 0; i < 15; i++ ) {
		if ( i == 6 )
			continue;
		bi[i] = VkDescriptorBufferInfo { bufs[i].b, 0, VK_WHOLE_SIZE };
		VkWriteDescriptorSet x { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
		x.dstSet = set;
		x.dstBinding = i;
		x.descriptorCount = 1;
		x.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		x.pBufferInfo = &bi[i];
		w.push_back( x );
	}
	vkUpdateDescriptorSets( dev, uint32_t( w.size() ), w.data(), 0, nullptr );
	vkResetCommandBuffer( cb, 0 );
	VkCommandBufferBeginInfo bb { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
	vkBeginCommandBuffer( cb, &bb );
	vkCmdBindPipeline( cb, VK_PIPELINE_BIND_POINT_COMPUTE, pipe );
	vkCmdBindDescriptorSets( cb, VK_PIPELINE_BIND_POINT_COMPUTE, pipeLayout, 0, 1, &set, 0, nullptr );
	vkCmdPushConstants( cb, pipeLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, 64, pc );
	const uint64_t gx = groups < 65535 ? groups : 65535;
	const uint64_t gy = ( groups + gx - 1 ) / gx;
	vkCmdDispatch( cb, uint32_t( gx ), uint32_t( gy ), 1 );
	vkEndCommandBuffer( cb );
	VkSubmitInfo si { VK_STRUCTURE_TYPE_SUBMIT_INFO };
	si.commandBufferCount = 1;
	si.pCommandBuffers = &cb;
	const bool ok = vkQueueSubmit( queue, 1, &si, VK_NULL_HANDLE ) == VK_SUCCESS && vkQueueWaitIdle( queue ) == VK_SUCCESS;
	vkDestroyPipeline( dev, pipe, nullptr );
	vkDestroyShaderModule( dev, sm, nullptr );
	return ok;
}

// the Params block (ddgi_common.glsl struct Params, std430: 18 vec4)
struct Params
{
	int32_t dims[4];
	float origin[4], stepv[4], sunTo[4], sun[4], dirTo[4], dirColor[4], sky[6][4];
	int32_t counts[4], flags[4];
	float blendp[4];
	int32_t skipLo[4], skipHi[4];
};
static_assert( sizeof( Params ) == 18 * 16, "Params is 18 vec4" );

struct Case { int x, y, z; int skipAsleep; int skip; uint64_t seed; };

// border: binding 1 irr (64 vec4 a probe), 2 dist (256 vec2), 3 probes (vec4), 4 age (uint), 14 params; the rest 256 B
static void fillBorder( std::vector<Buf> & bufs, const Case & c, uint64_t & groups )
{
	const uint32_t nP = uint32_t( c.x * c.y * c.z );
	Rng r { c.seed };
	float * irr = static_cast<float *>( bufs[1].p );
	for ( size_t i = 0; i < size_t( nP ) * 64 * 4; i++ )
		irr[i] = r.unit();
	float * dist = static_cast<float *>( bufs[2].p );
	for ( size_t i = 0; i < size_t( nP ) * 256 * 2; i++ )
		dist[i] = r.unit() * 300.0f;
	float * pr = static_cast<float *>( bufs[3].p );
	for ( uint32_t p = 0; p < nP; p++ ) {
		pr[p * 4 + 0] = r.unit();
		pr[p * 4 + 1] = r.unit();
		pr[p * 4 + 2] = r.unit();
		pr[p * 4 + 3] = float( r.next() % 4 );
	}
	uint32_t * age = static_cast<uint32_t *>( bufs[4].p );
	for ( uint32_t p = 0; p < nP; p++ )
		age[p] = r.next() % 1000;
	Params P;
	std::memset( &P, 0, sizeof P );
	P.dims[0] = c.x; P.dims[1] = c.y; P.dims[2] = c.z; P.dims[3] = int32_t( nP );
	P.flags[1] = c.skipAsleep;
	if ( c.skip ) {
		P.skipLo[0] = c.x / 4; P.skipLo[1] = c.y / 4; P.skipLo[2] = c.z / 4; P.skipLo[3] = 1;
		P.skipHi[0] = c.x / 2; P.skipHi[1] = c.y / 2; P.skipHi[2] = c.z / 2;
	}
	std::memcpy( bufs[14].p, &P, sizeof P );
	groups = ( uint64_t( nP ) * 88 + 63 ) / 64;   // rtddgi_vk.inc borderGroups
}

int main( int argc, char ** argv )
{
	if ( argc < 4 || std::strcmp( argv[1], "border" ) ) {
		std::printf( "usage: kernelmatch border <glsl .spv> <hlsl .spv>\n" );
		return 2;
	}
	const std::vector<uint32_t> a = readSpv( argv[2] ), b = readSpv( argv[3] );
	if ( a.empty() || b.empty() ) {
		std::printf( "HLSLMATCH %s FAIL (a .spv did not read)\n", argv[1] );
		return 1;
	}
	HMODULE lib = LoadLibraryA( "vulkan-1.dll" );
	if ( !lib ) {
		std::printf( "HLSLMATCH FAIL (no vulkan-1.dll)\n" );
		return 1;
	}
	auto gipa = reinterpret_cast<PFN_vkGetInstanceProcAddr>( reinterpret_cast<void *>( GetProcAddress( lib, "vkGetInstanceProcAddr" ) ) );
	vkCreateInstance = reinterpret_cast<PFN_vkCreateInstance>( gipa( VK_NULL_HANDLE, "vkCreateInstance" ) );
	VkApplicationInfo app { VK_STRUCTURE_TYPE_APPLICATION_INFO };
	app.apiVersion = VK_API_VERSION_1_2;   // bakert.cpp's device version
	VkInstanceCreateInfo ii { VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };
	ii.pApplicationInfo = &app;
	VkInstance inst;
	if ( vkCreateInstance( &ii, nullptr, &inst ) != VK_SUCCESS ) {
		std::printf( "HLSLMATCH FAIL (no instance)\n" );
		return 1;
	}
#define LOADI( name ) name = reinterpret_cast<PFN_##name>( gipa( inst, #name ) )
	LOADI( vkEnumeratePhysicalDevices ); LOADI( vkGetPhysicalDeviceQueueFamilyProperties ); LOADI( vkGetPhysicalDeviceProperties );
	LOADI( vkGetPhysicalDeviceMemoryProperties ); LOADI( vkCreateDevice ); LOADI( vkGetDeviceQueue );
	uint32_t n = 0;
	vkEnumeratePhysicalDevices( inst, &n, nullptr );
	std::vector<VkPhysicalDevice> pd( n );
	vkEnumeratePhysicalDevices( inst, &n, pd.data() );
	phys = VK_NULL_HANDLE;
	for ( VkPhysicalDevice d : pd ) {
		VkPhysicalDeviceProperties p;
		vkGetPhysicalDeviceProperties( d, &p );
		if ( p.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU || !phys )
			phys = d;
	}
	if ( !phys ) {
		std::printf( "HLSLMATCH FAIL (no device)\n" );
		return 1;
	}
	uint32_t qn = 0;
	vkGetPhysicalDeviceQueueFamilyProperties( phys, &qn, nullptr );
	std::vector<VkQueueFamilyProperties> qf( qn );
	vkGetPhysicalDeviceQueueFamilyProperties( phys, &qn, qf.data() );
	family = UINT32_MAX;
	for ( uint32_t i = 0; i < qn; i++ )   // the compute-only family first (the one the traces run on since item 2)
		if ( ( qf[i].queueFlags & VK_QUEUE_COMPUTE_BIT ) && !( qf[i].queueFlags & VK_QUEUE_GRAPHICS_BIT ) && family == UINT32_MAX )
			family = i;
	for ( uint32_t i = 0; i < qn && family == UINT32_MAX; i++ )
		if ( qf[i].queueFlags & VK_QUEUE_COMPUTE_BIT )
			family = i;
	const float prio = 1.0f;
	VkDeviceQueueCreateInfo qi { VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO };
	qi.queueFamilyIndex = family;
	qi.queueCount = 1;
	qi.pQueuePriorities = &prio;
	// the GLSL modules declare the TLAS (ray query capability): the device takes the ray query features as bakert's does
	VkPhysicalDeviceRayQueryFeaturesKHR rq { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR };
	rq.rayQuery = VK_TRUE;
	VkPhysicalDeviceAccelerationStructureFeaturesKHR as { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR };
	as.accelerationStructure = VK_TRUE;
	as.pNext = &rq;
	VkPhysicalDeviceVulkan12Features v12 { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES };
	v12.descriptorBindingPartiallyBound = VK_TRUE;
	v12.bufferDeviceAddress = VK_TRUE;
	v12.pNext = &as;
	const char * ext[] = { "VK_KHR_acceleration_structure", "VK_KHR_ray_query", "VK_KHR_deferred_host_operations" };
	VkDeviceCreateInfo di { VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
	di.pNext = &v12;
	di.queueCreateInfoCount = 1;
	di.pQueueCreateInfos = &qi;
	di.enabledExtensionCount = 3;
	di.ppEnabledExtensionNames = ext;
	if ( vkCreateDevice( phys, &di, nullptr, &dev ) != VK_SUCCESS ) {
		std::printf( "HLSLMATCH FAIL (no device with ray query)\n" );
		return 1;
	}
	auto gdpa = reinterpret_cast<PFN_vkGetDeviceProcAddr>( gipa( inst, "vkGetDeviceProcAddr" ) );
#define LOADD( name ) name = reinterpret_cast<PFN_##name>( gdpa( dev, #name ) )
	LOADD( vkCreateBuffer ); LOADD( vkGetBufferMemoryRequirements ); LOADD( vkAllocateMemory ); LOADD( vkBindBufferMemory );
	LOADD( vkMapMemory ); LOADD( vkCreateDescriptorSetLayout ); LOADD( vkCreatePipelineLayout ); LOADD( vkCreateShaderModule );
	LOADD( vkCreateComputePipelines ); LOADD( vkCreateDescriptorPool ); LOADD( vkAllocateDescriptorSets ); LOADD( vkUpdateDescriptorSets );
	LOADD( vkCreateCommandPool ); LOADD( vkAllocateCommandBuffers ); LOADD( vkBeginCommandBuffer ); LOADD( vkEndCommandBuffer );
	LOADD( vkCmdBindPipeline ); LOADD( vkCmdBindDescriptorSets ); LOADD( vkCmdPushConstants ); LOADD( vkCmdDispatch );
	LOADD( vkQueueSubmit ); LOADD( vkQueueWaitIdle ); LOADD( vkResetCommandBuffer ); LOADD( vkDestroyPipeline );
	LOADD( vkDestroyShaderModule ); LOADD( vkDestroyBuffer ); LOADD( vkFreeMemory );
	vkGetDeviceQueue( dev, family, 0, &queue );
	if ( !makeLayout() ) {
		std::printf( "HLSLMATCH FAIL (layout)\n" );
		return 1;
	}
	std::printf( "device queue family %u\n", family );
	// cases: small grids with every state and both skip rules, and one past 65535 groups (the y split of the dispatch)
	const Case cases[] = {
		{ 4, 3, 2, 0, 0, 1 }, { 4, 3, 2, 1, 0, 2 }, { 9, 7, 5, 0, 1, 3 }, { 9, 7, 5, 1, 1, 4 },
		{ 16, 16, 8, 1, 1, 5 }, { 50, 50, 20, 0, 1, 6 },
	};
	const uint32_t pc[16] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 1, 0, 0 };   // job: mode 3 (border), frame 1
	int fails = 0, total = 0;
	for ( const Case & c : cases ) {
		const size_t nP = size_t( c.x * c.y * c.z );
		const size_t sz[15] = { 256, nP * 64 * 16, nP * 256 * 8, nP * 16, nP * 4, 256, 256, 256, 256, 256, 256, 256, 256, 256, sizeof( Params ) };
		std::vector<Buf> A( 15 ), B( 15 );
		for ( int i = 0; i < 15; i++ ) {
			A[i] = makeBuf( sz[i] );
			B[i] = makeBuf( sz[i] );
		}
		uint64_t groups = 0;
		fillBorder( A, c, groups );
		fillBorder( B, c, groups );
		std::vector<unsigned char> before( sz[1] );
		std::memcpy( before.data(), A[1].p, sz[1] );
		const bool ra = run( a, A, pc, groups ), rb = run( b, B, pc, groups );
		size_t diff[15] = {};
		for ( int i = 0; i < 15; i++ ) {
			const unsigned char * x = static_cast<const unsigned char *>( A[i].p ), * y = static_cast<const unsigned char *>( B[i].p );
			for ( size_t k = 0; k < sz[i]; k++ )
				diff[i] += x[k] != y[k];
		}
		size_t moved = 0;   // the GLSL run's own irradiance writes: a case that wrote nothing proves nothing
		for ( size_t k = 0; k < sz[1]; k++ )
			moved += before[k] != static_cast<const unsigned char *>( A[1].p )[k];
		size_t all = 0;
		for ( int i = 0; i < 15; i++ )
			all += diff[i];
		const bool ok = ra && rb && all == 0 && moved > 0;
		fails += ok ? 0 : 1;
		total++;
		std::printf( "case %dx%dx%d skipAsleep %d skip %d groups %llu: ran %d/%d, bytes differing irr %zu dist %zu age %zu probes %zu "
			"other %zu, glsl bytes written irr %zu: %s\n", c.x, c.y, c.z, c.skipAsleep, c.skip, (unsigned long long)groups, ra, rb,
			diff[1], diff[2], diff[4], diff[3], all - diff[1] - diff[2] - diff[3] - diff[4], moved, ok ? "same" : "DIFFERS" );
		for ( int i = 0; i < 15; i++ ) {
			freeBuf( A[i] );
			freeBuf( B[i] );
		}
	}
	if ( fails )
		std::printf( "HLSLMATCH %s FAIL %d of %d cases\n", argv[1], fails, total );
	else
		std::printf( "HLSLMATCH %s PASS %d cases\n", argv[1], total );
	return fails ? 1 : 0;
}
