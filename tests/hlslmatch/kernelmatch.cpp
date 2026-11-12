// Lane VKCOMPUTE1 (2026-11-12): run a converted DDGI kernel and its GLSL original on the same buffers and compare every
// buffer byte for byte. No NifSkope: a headless Vulkan device of its own (the compute-only family when there is one),
// the DDGI set's layout (src/features/gi/rtddgi/ddgi_common.glsl: storage buffers 0-5, 7-14, the TLAS at 6 partially
// bound; trace builds a real TLAS in bakert's shape and writes it), the dispatch split of rtddgi_vk.inc ddgiDispatch.
// Usage: kernelmatch <kernel> <glsl .spv> <hlsl .spv>   (kernel: border reloc blend trace). Prints one line a case, then
// `HLSLMATCH <kernel> PASS n cases` or `HLSLMATCH <kernel> FAIL d of n cases`.
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
VKF( vkCreateBuffer ); VKF( vkGetBufferMemoryRequirements ); VKF( vkAllocateMemory ); VKF( vkBindBufferMemory ); VKF( vkMapMemory );
VKF( vkCreateDescriptorSetLayout ); VKF( vkCreatePipelineLayout ); VKF( vkCreateShaderModule ); VKF( vkCreateComputePipelines );
VKF( vkCreateDescriptorPool ); VKF( vkAllocateDescriptorSets ); VKF( vkUpdateDescriptorSets ); VKF( vkCreateCommandPool );
VKF( vkAllocateCommandBuffers ); VKF( vkBeginCommandBuffer ); VKF( vkEndCommandBuffer ); VKF( vkCmdBindPipeline );
VKF( vkCmdBindDescriptorSets ); VKF( vkCmdPushConstants ); VKF( vkCmdDispatch ); VKF( vkQueueSubmit ); VKF( vkQueueWaitIdle );
VKF( vkCreateFence ); VKF( vkWaitForFences ); VKF( vkDestroyFence );
VKF( vkResetCommandBuffer ); VKF( vkDestroyPipeline ); VKF( vkDestroyShaderModule ); VKF( vkDestroyBuffer ); VKF( vkFreeMemory );
// round 2 (trace): a real TLAS for the ray-query kernels
VKF( vkGetBufferDeviceAddress ); VKF( vkCreateAccelerationStructureKHR ); VKF( vkDestroyAccelerationStructureKHR );
VKF( vkGetAccelerationStructureBuildSizesKHR ); VKF( vkCmdBuildAccelerationStructuresKHR );
VKF( vkGetAccelerationStructureDeviceAddressKHR ); VKF( vkCmdPipelineBarrier );

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

// host-visible + coherent, mapped; addr: usable by device address (acceleration structure inputs, storage, scratch)
static Buf makeBuf( size_t n, VkBufferUsageFlags usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, bool addr = false )
{
	Buf B;
	B.n = n;
	VkBufferCreateInfo bi { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
	bi.size = n;
	bi.usage = usage | ( addr ? VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT : 0 );
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
	VkMemoryAllocateFlagsInfo af { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO };
	af.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;
	VkMemoryAllocateInfo ai { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
	ai.pNext = addr ? &af : nullptr;
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

// the DDGI set: bindings 0-14, 6 = the TLAS (partially bound: written only when a case built one, the trace's)
static VkAccelerationStructureKHR tlas = VK_NULL_HANDLE;
static VkDescriptorSetLayout setLayout, set1Layout;
static VkPipelineLayout pipeLayout;
// a kernel's own set 1 (air rays 4 buffers, restir 11, restirgi 15): bufs[15 + i] = set 1 binding i; every binding partially bound
constexpr uint32_t kSet1Max = 15;
static uint32_t nSet1 = 0;
// bakert's bvh kernel: binding 6 is a storage buffer (the tree's nodes), so it runs on a second layout
static bool storage6 = false;
static VkDescriptorSetLayout setLayoutB;
static VkPipelineLayout pipeLayoutB;
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
	b[6].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	fl[6] = 0;
	if ( vkCreateDescriptorSetLayout( dev, &li, nullptr, &setLayoutB ) != VK_SUCCESS )
		return false;
	for ( uint32_t i = 0; i < kSet1Max; i++ ) {
		b[i] = VkDescriptorSetLayoutBinding { i, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr };
		fl[i] = VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT;
	}
	fi.bindingCount = kSet1Max;
	li.bindingCount = kSet1Max;
	if ( vkCreateDescriptorSetLayout( dev, &li, nullptr, &set1Layout ) != VK_SUCCESS )
		return false;
	const VkDescriptorSetLayout both[2] = { setLayout, set1Layout };
	VkPushConstantRange pr { VK_SHADER_STAGE_COMPUTE_BIT, 0, 64 };
	VkPipelineLayoutCreateInfo pi { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
	pi.setLayoutCount = 2;
	pi.pSetLayouts = both;
	pi.pushConstantRangeCount = 1;
	pi.pPushConstantRanges = &pr;
	if ( vkCreatePipelineLayout( dev, &pi, nullptr, &pipeLayout ) != VK_SUCCESS )
		return false;
	pi.setLayoutCount = 1;
	pi.pSetLayouts = &setLayoutB;
	if ( vkCreatePipelineLayout( dev, &pi, nullptr, &pipeLayoutB ) != VK_SUCCESS )
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
	const VkPipelineLayout pl = storage6 ? pipeLayoutB : pipeLayout;
	ci.layout = pl;
	VkPipeline pipe;
	if ( vkCreateComputePipelines( dev, VK_NULL_HANDLE, 1, &ci, nullptr, &pipe ) != VK_SUCCESS )
		return false;
	VkDescriptorSetAllocateInfo da { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
	da.descriptorPool = pool;
	da.descriptorSetCount = 1;
	da.pSetLayouts = &setLayout;
	static VkDescriptorSet setA = VK_NULL_HANDLE, setB = VK_NULL_HANDLE, set1 = VK_NULL_HANDLE;
	if ( !setA && vkAllocateDescriptorSets( dev, &da, &setA ) != VK_SUCCESS )
		return false;
	da.pSetLayouts = &setLayoutB;
	if ( !setB && vkAllocateDescriptorSets( dev, &da, &setB ) != VK_SUCCESS )
		return false;
	const VkDescriptorSet set = storage6 ? setB : setA;
	da.pSetLayouts = &set1Layout;
	if ( !set1 && vkAllocateDescriptorSets( dev, &da, &set1 ) != VK_SUCCESS )
		return false;
	std::vector<VkDescriptorBufferInfo> bi( 15 + kSet1Max );
	std::vector<VkWriteDescriptorSet> w;
	for ( uint32_t i = 0; i < nSet1; i++ ) {
		bi[15 + i] = VkDescriptorBufferInfo { bufs[15 + i].b, 0, VK_WHOLE_SIZE };
		VkWriteDescriptorSet x { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
		x.dstSet = set1;
		x.dstBinding = i;
		x.descriptorCount = 1;
		x.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		x.pBufferInfo = &bi[15 + i];
		w.push_back( x );
	}
	for ( uint32_t i = 0; i < 15; i++ ) {
		if ( i == 6 && !storage6 )
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
	VkWriteDescriptorSetAccelerationStructureKHR wa { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR };
	wa.accelerationStructureCount = 1;
	wa.pAccelerationStructures = &tlas;
	if ( tlas && !storage6 ) {
		VkWriteDescriptorSet x { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
		x.pNext = &wa;
		x.dstSet = set;
		x.dstBinding = 6;
		x.descriptorCount = 1;
		x.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
		w.push_back( x );
	}
	vkUpdateDescriptorSets( dev, uint32_t( w.size() ), w.data(), 0, nullptr );
	vkResetCommandBuffer( cb, 0 );
	VkCommandBufferBeginInfo bb { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
	vkBeginCommandBuffer( cb, &bb );
	vkCmdBindPipeline( cb, VK_PIPELINE_BIND_POINT_COMPUTE, pipe );
	const VkDescriptorSet sets[2] = { set, set1 };
	vkCmdBindDescriptorSets( cb, VK_PIPELINE_BIND_POINT_COMPUTE, pl, 0, nSet1 && !storage6 ? 2 : 1, sets, 0, nullptr );
	vkCmdPushConstants( cb, pl, VK_SHADER_STAGE_COMPUTE_BIT, 0, 64, pc );
	const uint64_t gx = groups < 65535 ? groups : 65535;
	const uint64_t gy = ( groups + gx - 1 ) / gx;
	vkCmdDispatch( cb, uint32_t( gx ), uint32_t( gy ), 1 );
	vkEndCommandBuffer( cb );
	VkSubmitInfo si { VK_STRUCTURE_TYPE_SUBMIT_INFO };
	si.commandBufferCount = 1;
	si.pCommandBuffers = &cb;
	// a fence with a 20 s bound: a kernel that hangs the GPU fails its case instead of spinning the gate forever
	VkFenceCreateInfo fci { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
	VkFence fence;
	vkCreateFence( dev, &fci, nullptr, &fence );
	const bool ok = vkQueueSubmit( queue, 1, &si, fence ) == VK_SUCCESS
		&& vkWaitForFences( dev, 1, &fence, VK_TRUE, 20000000000ull ) == VK_SUCCESS;
	if ( !ok ) {
		std::printf( "HLSLMATCH FAIL (a dispatch did not finish in 20 s)\n" );
		std::fflush( stdout );
		std::exit( 1 );
	}
	vkDestroyFence( dev, fence, nullptr );
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

// x y z probes, skipAsleep, the skip range on, seed; round 2: rays a probe, job mode, job.y (fresh only), sky-below rule
// trace: the red bits (flags.w), one bounce (flags.x), the sky on (sky[0].w)
struct Case { int x, y, z; int skipAsleep; int skip; uint64_t seed; int N = 288; uint32_t mode = 3; uint32_t jobY = 0; int below = 1;
	int red = 0; int oneBounce = 0; int sky = 1; };

// the rays buffer (binding 0, nP x N vec4): radiance rgb, signed distance with every kind the kernels branch on --
// a miss (1e27), a backface (-0.2 t), a hit nearer or farther than a probe step. backShare: a probe's chance a ray is a back
static void fillRays( float * rays, size_t nP, int N, Rng & r, float stepMax )
{
	for ( size_t p = 0; p < nP; p++ ) {
		const float backShare = r.unit() < 0.3f ? 0.5f : 0.1f * r.unit();
		for ( int i = 0; i < N; i++ ) {
			float * v = rays + ( p * size_t( N ) + size_t( i ) ) * 4;
			v[0] = r.unit() * 4.0f; v[1] = r.unit() * 4.0f; v[2] = r.unit() * 4.0f;
			const float u = r.unit();
			if ( u < backShare )
				v[3] = -0.2f * r.unit() * stepMax * 2.0f;
			else if ( u < backShare + 0.3f )
				v[3] = 1e27f;
			else
				v[3] = ( r.unit() * 2.0f + 0.001f ) * stepMax;
		}
	}
}

// a random rotation's rows (Gram-Schmidt of two random vectors), as the CPU's updateRotation hands the kernels
static void randomRotation( Rng & r, uint32_t pc[16] )
{
	float a[3] = { r.unit() - 0.5f, r.unit() - 0.5f, r.unit() - 0.5f }, b[3] = { r.unit() - 0.5f, r.unit() - 0.5f, r.unit() - 0.5f };
	auto norm = []( float * v ) { const float l = std::sqrt( v[0] * v[0] + v[1] * v[1] + v[2] * v[2] ); v[0] /= l; v[1] /= l; v[2] /= l; };
	norm( a );
	const float d = a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
	for ( int k = 0; k < 3; k++ )
		b[k] -= d * a[k];
	norm( b );
	const float c[3] = { a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0] };
	float rows[12] = { a[0], a[1], a[2], 0, b[0], b[1], b[2], 0, c[0], c[1], c[2], 0 };
	std::memcpy( pc, rows, sizeof rows );
}

static void fillParamsBase( Params & P, const Case & c, uint32_t nP, Rng & r )
{
	std::memset( &P, 0, sizeof P );
	P.dims[0] = c.x; P.dims[1] = c.y; P.dims[2] = c.z; P.dims[3] = int32_t( nP );
	P.flags[1] = c.skipAsleep;
	if ( c.skip ) {
		P.skipLo[0] = c.x / 4; P.skipLo[1] = c.y / 4; P.skipLo[2] = c.z / 4; P.skipLo[3] = 1;
		P.skipHi[0] = c.x / 2; P.skipHi[1] = c.y / 2; P.skipHi[2] = c.z / 2;
	}
	P.counts[0] = c.N;
	for ( int k = 0; k < 3; k++ ) {
		P.origin[k] = ( r.unit() - 0.5f ) * 2000.0f;
		P.stepv[k] = 40.0f + r.unit() * 120.0f;
	}
	P.stepv[3] = std::fmin( P.stepv[0], std::fmin( P.stepv[1], P.stepv[2] ) );
	P.origin[3] = 2.0f * P.stepv[3];   // distClamp
	for ( int f = 0; f < 6; f++ )
		for ( int k = 0; k < 4; k++ )
			P.sky[f][k] = r.unit() * 2.0f;
	P.sky[0][3] = 1.0f;
	P.sky[4][3] = float( c.below );
	P.blendp[0] = 0.97f;
	P.blendp[1] = 50.0f;
	P.blendp[2] = 0.3f;
}

// reloc (modes 4 relocate / 5 classify): binding 0 rays, 3 probes, 14 params; one invocation a probe
static void fillReloc( std::vector<Buf> & bufs, const Case & c, uint64_t & groups, uint32_t pc[16] )
{
	const uint32_t nP = uint32_t( c.x * c.y * c.z );
	Rng r { c.seed };
	Params P;
	fillParamsBase( P, c, nP, r );
	fillRays( static_cast<float *>( bufs[0].p ), nP, c.N, r, P.stepv[3] );
	float * pr = static_cast<float *>( bufs[3].p );
	for ( uint32_t p = 0; p < nP; p++ ) {
		for ( int k = 0; k < 3; k++ )
			pr[p * 4 + k] = ( r.unit() - 0.5f ) * 0.9f * P.stepv[k];
		pr[p * 4 + 3] = float( r.next() % 4 );
	}
	std::memcpy( bufs[14].p, &P, sizeof P );
	pc[12] = c.mode; pc[13] = c.jobY;
	groups = ( uint64_t( nP ) + 63 ) / 64;   // rtddgi_vk.inc probe-wide passes
}

// blend: binding 0 rays, 1 irr, 2 dist, 3 probes, 4 age, 14 params; one group of 256 a probe
static void fillBlend( std::vector<Buf> & bufs, const Case & c, uint64_t & groups, uint32_t pc[16] )
{
	const uint32_t nP = uint32_t( c.x * c.y * c.z );
	Rng r { c.seed };
	Params P;
	fillParamsBase( P, c, nP, r );
	fillRays( static_cast<float *>( bufs[0].p ), nP, c.N, r, P.stepv[3] );
	float * irr = static_cast<float *>( bufs[1].p );
	for ( size_t i = 0; i < size_t( nP ) * 64 * 4; i++ )
		irr[i] = r.unit() * 1.5f;   // an encoded change past 0.25 and 0.8 both happen
	float * dist = static_cast<float *>( bufs[2].p );
	for ( size_t i = 0; i < size_t( nP ) * 256 * 2; i++ )
		dist[i] = r.unit() * 300.0f;
	float * pr = static_cast<float *>( bufs[3].p );
	for ( uint32_t p = 0; p < nP; p++ ) {
		pr[p * 4 + 0] = r.unit(); pr[p * 4 + 1] = r.unit(); pr[p * 4 + 2] = r.unit();
		pr[p * 4 + 3] = float( r.next() % 4 );
	}
	uint32_t * age = static_cast<uint32_t *>( bufs[4].p );
	for ( uint32_t p = 0; p < nP; p++ )
		age[p] = r.next() % 4 == 0 ? 0u : r.next() % 1000;
	std::memcpy( bufs[14].p, &P, sizeof P );
	randomRotation( r, pc );
	pc[12] = 2; pc[13] = 1;
	groups = nP;
}

// border: binding 1 irr (64 vec4 a probe), 2 dist (256 vec2), 3 probes (vec4), 4 age (uint), 14 params; the rest 256 B
static void fillBorder( std::vector<Buf> & bufs, const Case & c, uint64_t & groups, uint32_t pc[16] )
{
	pc[12] = 3; pc[13] = 1;   // job: mode 3 (border), frame 1
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

// ---- trace: a random soup in bakert's TLAS shape (rtddgi_vk.inc buildRt) ----
// Part 0 = the single-soup layout: one BLAS, geometry 0 opaque rows [0, nOp), geometry 1 alpha rows [nOp, nOp + nAl),
// custom index 0 (counts.w = nOp moves geometry 1). Parts 1, 2 = AIRPERF1's per-part layout: one geometry each, custom
// index = the part's first row. The BLAS takes GROWN triangles, binding 8 the true ones (the kernel re-tests every
// candidate); every 16th row repeats the one before (winding flipped every other time) so the tie pass has ties to settle.
struct Scene
{
	std::vector<float> verts;     // binding 8: rows x 9, the true triangles
	std::vector<int32_t> remap;   // binding 7: row -> soup triangle (a permutation)
	std::vector<float> attr;      // binding 5: soup triangle x 2 vec4 (albedo + two-sided, Le)
	std::vector<int32_t> triOf;   // binding 9: soup triangle -> masked record or -1
	std::vector<uint32_t> mask;   // binding 10: 12 words a record
	std::vector<uint32_t> maps;   // binding 11: offset, w, h, 0
	std::vector<uint32_t> alpha;  // binding 12: bytes, packed 4 a word
	std::vector<float> lights;    // binding 13: 8 vec4 a light
	uint32_t partFirst[3], partN0[3], partN1[3];
	int nOp = 0, nLights = 0;
};
static Scene S;
static bool sceneOn = false;
struct AsParts { Buf vb[3], blasBuf[3], tlasBuf, inst, scratch; VkAccelerationStructureKHR blas[3] = {}; };
static AsParts AS;
static bool bakeScene = false;   // bakert's BLAS: ONE instance, geometry 0 the opaque rows, geometry 1 the alpha-tested ones

static void makeScene( const Case & c, const Params & P )
{
	Rng r { c.seed * 7919 + 3 };
	S = Scene();
	const uint32_t n0 = bakeScene ? 280 : 120, nAl = 60, n1 = bakeScene ? 0 : 90, n2 = bakeScene ? 0 : 70;
	S.nOp = int( n0 );
	S.partFirst[0] = 0; S.partN0[0] = n0; S.partN1[0] = nAl;
	S.partFirst[1] = n0 + nAl; S.partN0[1] = n1; S.partN1[1] = 0;
	S.partFirst[2] = n0 + nAl + n1; S.partN0[2] = n2; S.partN1[2] = 0;
	const uint32_t rows = n0 + nAl + n1 + n2;
	S.verts.resize( size_t( rows ) * 9 );
	for ( uint32_t i = 0; i < rows; i++ ) {
		float * v = &S.verts[size_t( i ) * 9];
		if ( i % 16 == 15 ) {   // a duplicate of the row before: an exact tie
			const float * u = &S.verts[size_t( i - 1 ) * 9];
			const bool flip = ( i / 16 ) % 2 == 1;
			for ( int k = 0; k < 3; k++ ) {
				v[k] = u[k];
				v[3 + k] = flip ? u[6 + k] : u[3 + k];
				v[6 + k] = flip ? u[3 + k] : u[6 + k];
			}
			continue;
		}
		float ctr[3];
		for ( int k = 0; k < 3; k++ )
			ctr[k] = P.origin[k] + ( r.unit() * float( ( k == 0 ? c.x : k == 1 ? c.y : c.z ) + 1 ) - 0.5f ) * P.stepv[k];
		for ( int j = 0; j < 3; j++ )
			for ( int k = 0; k < 3; k++ )
				v[j * 3 + k] = ctr[k] + ( r.unit() - 0.5f ) * 2.4f * P.stepv[k];
	}
	S.remap.resize( rows );
	for ( uint32_t i = 0; i < rows; i++ )
		S.remap[i] = int32_t( i );
	for ( uint32_t i = rows - 1; i > 0; i-- ) {
		const uint32_t j = r.next() % ( i + 1 );
		std::swap( S.remap[i], S.remap[j] );
	}
	S.attr.resize( size_t( rows ) * 8 );
	for ( uint32_t t = 0; t < rows; t++ ) {
		float * a = &S.attr[size_t( t ) * 8];
		a[0] = r.unit(); a[1] = r.unit(); a[2] = r.unit(); a[3] = r.unit() < 0.5f ? 0.0f : 1.0f;
		const bool glow = r.unit() < 0.1f;
		a[4] = glow ? r.unit() * 2.0f : 0.0f; a[5] = glow ? r.unit() * 2.0f : 0.0f; a[6] = glow ? r.unit() * 2.0f : 0.0f; a[7] = 0.0f;
	}
	// alpha: every geometry-1 row's soup triangle and ~1 in 5 of the rest is masked; two maps
	S.maps = { 0, 16, 16, 0, 256, 8, 32, 0 };
	S.alpha.resize( 128 );
	for ( uint32_t & w : S.alpha )
		w = r.next();
	S.triOf.assign( rows, -1 );
	for ( uint32_t i = 0; i < rows; i++ ) {
		const bool masked = ( i >= n0 && i < n0 + nAl ) || r.unit() < 0.2f;
		if ( !masked )
			continue;
		S.triOf[size_t( S.remap[i] )] = int32_t( S.mask.size() / 12 );
		auto fl = []( float f ) { uint32_t u; std::memcpy( &u, &f, 4 ); return u; };
		S.mask.push_back( r.next() % 2 );
		S.mask.push_back( 64 + r.next() % 128 );
		for ( int k = 0; k < 6; k++ )
			S.mask.push_back( fl( r.unit() * 3.0f - 1.0f ) );
		for ( int k = 0; k < 3; k++ )
			S.mask.push_back( fl( 0.5f + r.unit() ) );
		S.mask.push_back( r.next() % 2 );
	}
	// lights: point (shape 0), half-space (1), box (2); some spots
	S.nLights = 5;
	for ( int l = 0; l < S.nLights; l++ ) {
		float L[32] = {};
		for ( int k = 0; k < 3; k++ )
			L[k] = P.origin[k] + r.unit() * float( ( k == 0 ? c.x : k == 1 ? c.y : c.z ) ) * P.stepv[k];
		L[3] = ( 2.0f + r.unit() * 3.0f ) * P.stepv[3];
		L[4] = r.unit() * 3.0f; L[5] = r.unit() * 3.0f; L[6] = r.unit() * 3.0f; L[7] = r.unit() < 0.5f ? 1.0f : 0.0f;
		float d[3] = { r.unit() - 0.5f, r.unit() - 0.5f, r.unit() - 0.5f };
		const float dl = std::sqrt( d[0] * d[0] + d[1] * d[1] + d[2] * d[2] ) + 1e-6f;
		L[8] = d[0] / dl; L[9] = d[1] / dl; L[10] = d[2] / dl; L[11] = 0.5f + r.unit() * 0.4f;
		L[12] = r.unit() * 0.2f; L[13] = 0.8f + r.unit() * 0.2f; L[14] = 1.0f + r.unit(); L[15] = 1.0f + 2.0f * r.unit();
		const float h = 2.0f * P.stepv[3];
		for ( int k = 0; k < 3; k++ ) {   // the box planes: |dot(axis / h, x) - axis . centre / h| <= 1 inside
			L[16 + k * 4 + k] = 1.0f / h;
			L[16 + k * 4 + 3] = -L[k] / h;
		}
		L[28] = float( l % 3 );
		S.lights.insert( S.lights.end(), L, L + 32 );
	}
	sceneOn = true;
}

static VkDeviceAddress addrOf( const Buf & B )
{
	VkBufferDeviceAddressInfo i { VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO };
	i.buffer = B.b;
	return vkGetBufferDeviceAddress( dev, &i );
}

static bool submitWait()
{
	VkSubmitInfo si { VK_STRUCTURE_TYPE_SUBMIT_INFO };
	si.commandBufferCount = 1;
	si.pCommandBuffers = &cb;
	VkFenceCreateInfo fci { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
	VkFence fence;
	vkCreateFence( dev, &fci, nullptr, &fence );
	const bool ok = vkQueueSubmit( queue, 1, &si, fence ) == VK_SUCCESS
		&& vkWaitForFences( dev, 1, &fence, VK_TRUE, 20000000000ull ) == VK_SUCCESS;
	vkDestroyFence( dev, fence, nullptr );
	return ok;
}

// one build: geometry info + its sizes -> storage buffer + AS + scratch, recorded and waited on
static bool buildAs( VkAccelerationStructureTypeKHR type, const VkAccelerationStructureGeometryKHR * g, const uint32_t * counts,
	uint32_t nGeo, Buf & store, VkAccelerationStructureKHR & as )
{
	VkAccelerationStructureBuildGeometryInfoKHR bi { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR };
	bi.type = type;
	bi.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
	bi.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
	bi.geometryCount = nGeo;
	bi.pGeometries = g;
	VkAccelerationStructureBuildSizesInfoKHR sz { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR };
	vkGetAccelerationStructureBuildSizesKHR( dev, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &bi, counts, &sz );
	store = makeBuf( sz.accelerationStructureSize, VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR, true );
	VkAccelerationStructureCreateInfoKHR ci { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR };
	ci.buffer = store.b;
	ci.size = sz.accelerationStructureSize;
	ci.type = type;
	if ( vkCreateAccelerationStructureKHR( dev, &ci, nullptr, &as ) != VK_SUCCESS )
		return false;
	if ( AS.scratch.b )
		freeBuf( AS.scratch );
	AS.scratch = makeBuf( sz.buildScratchSize + 256, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, true );
	bi.dstAccelerationStructure = as;
	bi.scratchData.deviceAddress = ( addrOf( AS.scratch ) + 255 ) & ~VkDeviceAddress( 255 );
	std::vector<VkAccelerationStructureBuildRangeInfoKHR> ranges( nGeo );
	for ( uint32_t i = 0; i < nGeo; i++ )
		ranges[i] = VkAccelerationStructureBuildRangeInfoKHR { counts[i], 0, 0, 0 };
	const VkAccelerationStructureBuildRangeInfoKHR * pr = ranges.data();
	vkResetCommandBuffer( cb, 0 );
	VkCommandBufferBeginInfo bb { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
	vkBeginCommandBuffer( cb, &bb );
	vkCmdBuildAccelerationStructuresKHR( cb, 1, &bi, &pr );
	vkEndCommandBuffer( cb );
	return submitWait();
}

static bool buildScene()
{
	// grown copies for the BLAS (bakert grows each edge outward; here every vertex moves 1e-4 off the centroid)
	auto grown = []( uint32_t row, float * out ) {
		const float * v = &S.verts[size_t( row ) * 9];
		for ( int k = 0; k < 3; k++ ) {
			const float m = ( v[k] + v[3 + k] + v[6 + k] ) / 3.0f;
			for ( int j = 0; j < 3; j++ )
				out[j * 3 + k] = m + ( v[j * 3 + k] - m ) * 1.0001f;
		}
	};
	const VkBufferUsageFlags in = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR;
	std::vector<VkAccelerationStructureInstanceKHR> inst( 3 );
	uint32_t nInst = 0;
	for ( int p = 0; p < 3; p++ ) {
		const uint32_t n = S.partN0[p] + S.partN1[p];
		if ( n == 0 )   // bakert's scene: one part
			continue;
		AS.vb[p] = makeBuf( size_t( n ) * 36, in, true );
		float * dst = static_cast<float *>( AS.vb[p].p );
		for ( uint32_t i = 0; i < n; i++ )
			grown( S.partFirst[p] + i, dst + size_t( i ) * 9 );
		VkAccelerationStructureGeometryKHR g[2];
		uint32_t counts[2] = { S.partN0[p], S.partN1[p] };
		const uint32_t nGeo = S.partN1[p] ? 2 : 1;
		for ( uint32_t k = 0; k < nGeo; k++ ) {
			g[k] = VkAccelerationStructureGeometryKHR { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR };
			g[k].geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
			g[k].flags = k == 0 ? VK_GEOMETRY_OPAQUE_BIT_KHR : 0;
			VkAccelerationStructureGeometryTrianglesDataKHR & t = g[k].geometry.triangles;
			t = VkAccelerationStructureGeometryTrianglesDataKHR { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR };
			t.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
			t.vertexData.deviceAddress = addrOf( AS.vb[p] ) + ( k == 0 ? 0 : VkDeviceAddress( S.partN0[p] ) * 36 );
			t.vertexStride = 12;
			t.maxVertex = counts[k] * 3 - 1;
			t.indexType = VK_INDEX_TYPE_NONE_KHR;
		}
		if ( !buildAs( VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR, g, counts, nGeo, AS.blasBuf[p], AS.blas[p] ) )
			return false;
		VkAccelerationStructureDeviceAddressInfoKHR ai { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR };
		ai.accelerationStructure = AS.blas[p];
		VkAccelerationStructureInstanceKHR & I = inst[nInst++];
		std::memset( &I, 0, sizeof I );
		I.transform.matrix[0][0] = I.transform.matrix[1][1] = I.transform.matrix[2][2] = 1.0f;
		I.instanceCustomIndex = S.partFirst[p];
		I.mask = 0xFF;
		I.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
		I.accelerationStructureReference = vkGetAccelerationStructureDeviceAddressKHR( dev, &ai );
	}
	AS.inst = makeBuf( sizeof( VkAccelerationStructureInstanceKHR ) * nInst, in, true );
	std::memcpy( AS.inst.p, inst.data(), sizeof( VkAccelerationStructureInstanceKHR ) * nInst );
	VkAccelerationStructureGeometryKHR g { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR };
	g.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
	g.geometry.instances = VkAccelerationStructureGeometryInstancesDataKHR { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR };
	g.geometry.instances.data.deviceAddress = addrOf( AS.inst );
	return buildAs( VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR, &g, &nInst, 1, AS.tlasBuf, tlas );
}

static void freeScene()
{
	if ( tlas )
		vkDestroyAccelerationStructureKHR( dev, tlas, nullptr );
	tlas = VK_NULL_HANDLE;
	for ( int p = 0; p < 3; p++ ) {
		if ( AS.blas[p] )
			vkDestroyAccelerationStructureKHR( dev, AS.blas[p], nullptr );
		for ( Buf * b : { &AS.vb[p], &AS.blasBuf[p] } )
			if ( b->b )
				freeBuf( *b );
	}
	for ( Buf * b : { &AS.tlasBuf, &AS.inst, &AS.scratch } )
		if ( b->b )
			freeBuf( *b );
	AS = AsParts();
	sceneOn = false;
}

static void traceParams( Params & P, const Case & c, uint32_t nP, Rng & r )
{
	fillParamsBase( P, c, nP, r );
	P.sky[0][3] = float( c.sky );
	float s[3] = { r.unit() - 0.5f, r.unit() - 0.5f, 0.2f + r.unit() };
	const float sl = std::sqrt( s[0] * s[0] + s[1] * s[1] + s[2] * s[2] );
	for ( int k = 0; k < 3; k++ ) {
		P.sunTo[k] = s[k] / sl;
		P.sun[k] = r.unit() * 5.0f;
		P.dirTo[k] = -s[k] / sl;
		P.dirColor[k] = r.unit();
	}
	P.sunTo[3] = 1.0f;
	P.sun[3] = 4.0f * P.stepv[3] * float( c.x + c.y + c.z );   // the trace tmax
	P.dirTo[3] = c.seed % 2 ? 1.0f : 0.0f;
	P.dirColor[3] = 2.0f;   // fixtureClear
	P.flags[0] = c.oneBounce;
	P.flags[2] = 1;          // useMask
	P.flags[3] = c.red;
	P.counts[1] = 5;
	P.counts[2] = int32_t( S.remap.size() );
	P.counts[3] = S.nOp;
}

// trace: binding 0 rays (written), 1 irr, 2 dist, 3 probes, 4 age (the bounce read), 5 attr, 6 TLAS, 7 remap, 8 verts,
// 9 triOf, 10 mask, 11 maps, 12 alpha, 13 lights, 14 params; one invocation a (probe, ray) of job.z .. job.z + job.w
static void fillTrace( std::vector<Buf> & bufs, const Case & c, uint64_t & groups, uint32_t pc[16] )
{
	const uint32_t nP = uint32_t( c.x * c.y * c.z );
	Rng r { c.seed };
	Params P;
	traceParams( P, c, nP, r );
	fillRays( static_cast<float *>( bufs[0].p ), nP, c.N, r, P.stepv[3] );   // old contents the kernel overwrites
	float * irr = static_cast<float *>( bufs[1].p );
	for ( size_t i = 0; i < size_t( nP ) * 64 * 4; i++ )
		irr[i] = r.unit() * 1.5f;
	float * dist = static_cast<float *>( bufs[2].p );
	for ( size_t i = 0; i < size_t( nP ) * 256 * 2; i++ )
		dist[i] = r.unit() * 3.0f * P.stepv[3];
	float * pr = static_cast<float *>( bufs[3].p );
	for ( uint32_t p = 0; p < nP; p++ ) {
		for ( int k = 0; k < 3; k++ )
			pr[p * 4 + k] = ( r.unit() - 0.5f ) * 0.8f * P.stepv[k];
		pr[p * 4 + 3] = float( r.next() % 4 );
	}
	uint32_t * age = static_cast<uint32_t *>( bufs[4].p );
	for ( uint32_t p = 0; p < nP; p++ )
		age[p] = r.next() % 4 == 0 ? 0u : 1 + r.next() % 100;
	std::memcpy( bufs[5].p, S.attr.data(), S.attr.size() * 4 );
	std::memcpy( bufs[7].p, S.remap.data(), S.remap.size() * 4 );
	std::memcpy( bufs[8].p, S.verts.data(), S.verts.size() * 4 );
	std::memcpy( bufs[9].p, S.triOf.data(), S.triOf.size() * 4 );
	std::memcpy( bufs[10].p, S.mask.data(), S.mask.size() * 4 );
	std::memcpy( bufs[11].p, S.maps.data(), S.maps.size() * 4 );
	std::memcpy( bufs[12].p, S.alpha.data(), S.alpha.size() * 4 );
	std::memcpy( bufs[13].p, S.lights.data(), S.lights.size() * 4 );
	std::memcpy( bufs[14].p, &P, sizeof P );
	randomRotation( r, pc );
	// mode 1: the 32 fixed rays; mode 0: every ray, or (jobY 1, which mode 0 does not read) the rays past the fixed ones
	const uint32_t first = c.mode == 0 && c.jobY ? 32 : 0, count = c.mode == 1 ? 32 : uint32_t( c.N ) - first;
	pc[12] = c.mode; pc[13] = c.jobY; pc[14] = first; pc[15] = count;
	groups = ( uint64_t( nP ) * count + 63 ) / 64;
}

// air rays: airrays.comp struct AirParams (std430, 11 vec4); the froxel grid and options by case seed (airOf)
struct AirParams
{
	int32_t size[4], opts[4];
	float slice[4], rows[3][4], proj[4], prows[3][4], pproj[4];
};
static_assert( sizeof( AirParams ) == 11 * 16, "AirParams is 11 vec4" );
struct AirCase { int fx, fy, fz, samples, red, hist; };
static AirCase airOf[64];   // by case seed

// air rays: set 0 = the trace's scene + TLAS + params (lights with the shadow flag), set 1 = 0 params, 1 acc (written),
// 2 the previous frame's acc, 3 the ratio (written); a camera at the grid's centre, a random turn, the previous one moved
static void fillAirRays( std::vector<Buf> & bufs, const Case & c, uint64_t & groups, uint32_t pc[16] )
{
	const uint32_t nP = uint32_t( c.x * c.y * c.z );
	const AirCase & q = airOf[c.seed % 64];
	const uint64_t nF = uint64_t( q.fx ) * q.fy * q.fz;
	Rng r { c.seed };
	Params P;
	traceParams( P, c, nP, r );
	std::memcpy( bufs[5].p, S.attr.data(), S.attr.size() * 4 );
	std::memcpy( bufs[7].p, S.remap.data(), S.remap.size() * 4 );
	std::memcpy( bufs[8].p, S.verts.data(), S.verts.size() * 4 );
	std::memcpy( bufs[9].p, S.triOf.data(), S.triOf.size() * 4 );
	std::memcpy( bufs[10].p, S.mask.data(), S.mask.size() * 4 );
	std::memcpy( bufs[11].p, S.maps.data(), S.maps.size() * 4 );
	std::memcpy( bufs[12].p, S.alpha.data(), S.alpha.size() * 4 );
	std::vector<float> L = S.lights;
	for ( int l = 0; l < S.nLights; l++ )
		L[size_t( l ) * 32 + 30] = l % 4 == 3 ? 0.0f : 1.0f;   // the shadow flag (l7.z): one lamp casts none
	std::memcpy( bufs[13].p, L.data(), L.size() * 4 );
	std::memcpy( bufs[14].p, &P, sizeof P );
	AirParams A;
	std::memset( &A, 0, sizeof A );
	A.size[0] = q.fx; A.size[1] = q.fy; A.size[2] = q.fz; A.size[3] = int32_t( c.seed * 37 % 1000 );
	A.opts[0] = q.samples; A.opts[1] = 8; A.opts[2] = q.red; A.opts[3] = q.hist;
	const float ext[3] = { float( c.x ) * P.stepv[0], float( c.y ) * P.stepv[1], float( c.z ) * P.stepv[2] };
	const float diag = std::sqrt( ext[0] * ext[0] + ext[1] * ext[1] + ext[2] * ext[2] );
	A.slice[0] = 0.05f * P.stepv[3];
	A.slice[1] = std::log( diag / A.slice[0] );
	A.slice[2] = 1e-3f * P.stepv[3];
	A.slice[3] = 1.0f;
	// a random rotation (unit quaternion) as the rows of view -> world, the camera at the box centre
	float qq[4] = { r.unit() - 0.5f, r.unit() - 0.5f, r.unit() - 0.5f, r.unit() - 0.5f };
	const float ql = std::sqrt( qq[0] * qq[0] + qq[1] * qq[1] + qq[2] * qq[2] + qq[3] * qq[3] ) + 1e-6f;
	for ( float & v : qq )
		v /= ql;
	const float w = qq[0], x = qq[1], y = qq[2], z = qq[3];
	const float R[3][3] = { { 1 - 2 * ( y * y + z * z ), 2 * ( x * y - w * z ), 2 * ( x * z + w * y ) },
		{ 2 * ( x * y + w * z ), 1 - 2 * ( x * x + z * z ), 2 * ( y * z - w * x ) },
		{ 2 * ( x * z - w * y ), 2 * ( y * z + w * x ), 1 - 2 * ( x * x + y * y ) } };
	for ( int i = 0; i < 3; i++ ) {
		const float cam = P.origin[i] + 0.5f * ext[i];
		for ( int k = 0; k < 3; k++ )
			A.rows[i][k] = A.prows[i][k] = R[i][k];
		A.rows[i][3] = cam;
		A.prows[i][3] = cam + ( r.unit() - 0.5f ) * 0.3f * P.stepv[3];
	}
	A.proj[0] = 1.3f; A.proj[1] = 2.0f; A.proj[2] = 0.02f; A.proj[3] = -0.01f;
	A.pproj[0] = 1.31f; A.pproj[1] = 1.99f; A.pproj[2] = 0.025f; A.pproj[3] = -0.012f;
	std::memcpy( bufs[15].p, &A, sizeof A );
	float * acc = static_cast<float *>( bufs[16].p ), * prev = static_cast<float *>( bufs[17].p ), * out = static_cast<float *>( bufs[18].p );
	for ( uint64_t i = 0; i < nF; i++ ) {
		for ( int k = 0; k < 4; k++ )
			acc[i * 4 + k] = r.unit();
		prev[i * 4 + 0] = r.unit() * 2.0f;
		prev[i * 4 + 1] = r.unit() * 2.0f;
		prev[i * 4 + 2] = float( r.next() % 20 );   // frames: past the clamp (8) about half the time
		prev[i * 4 + 3] = 0.0f;
		out[i * 2] = r.unit();
		out[i * 2 + 1] = r.unit();
	}
	randomRotation( r, pc );
	groups = ( nF + 63 ) / 64;
}

// ReSTIR DI: restir.comp struct RestirParams (std430, 11 vec4); the target size and options by case seed (restirOf)
struct RestirParams
{
	int32_t size[4], opts[4];
	float knobs[4], rows[3][4], proj[4], prows[3][4], pproj[4];
};
static_assert( sizeof( RestirParams ) == 11 * 16, "RestirParams is 11 vec4" );
struct RestirCase { int w, h, C, K, f, red, hist; };
static RestirCase restirOf[128];   // by case seed
static const int kRestirWatch[4] = { 15 + 3, 15 + 6, 15 + 7, 15 + 10 };   // the binding each mode writes: cur, numDen, acc, ratio

// set 0 = the trace's scene + TLAS + params (lamps with a source radius), set 1 = 0 params, 1 g-buffer, 2 the previous one,
// 3 cur, 4 hist reservoirs, 5 history pixel, 6 num/den, 7 acc, 8 previous acc, 9 the candidate list, 10 the ratio. The
// g-buffer: 8x8 blocks of one view normal (jittered) on a smooth depth (neighbours similar), 1 in 10 empty (1e6) or 0
static void fillRestir( std::vector<Buf> & bufs, const Case & c, uint64_t & groups, uint32_t pc[16] )
{
	const uint32_t nP = uint32_t( c.x * c.y * c.z );
	const RestirCase & q = restirOf[c.seed % 128];
	const uint64_t nPix = uint64_t( q.w ) * q.h;
	Rng r { c.seed };
	Params P;
	traceParams( P, c, nP, r );
	std::memcpy( bufs[5].p, S.attr.data(), S.attr.size() * 4 );
	std::memcpy( bufs[7].p, S.remap.data(), S.remap.size() * 4 );
	std::memcpy( bufs[8].p, S.verts.data(), S.verts.size() * 4 );
	std::memcpy( bufs[9].p, S.triOf.data(), S.triOf.size() * 4 );
	std::memcpy( bufs[10].p, S.mask.data(), S.mask.size() * 4 );
	std::memcpy( bufs[11].p, S.maps.data(), S.maps.size() * 4 );
	std::memcpy( bufs[12].p, S.alpha.data(), S.alpha.size() * 4 );
	std::vector<float> L = S.lights;
	for ( int l = 0; l < S.nLights; l++ )
		L[size_t( l ) * 32 + 29] = 0.1f * P.stepv[3] * float( l % 3 );   // the source radius (l7.y), 0 for some
	std::memcpy( bufs[13].p, L.data(), L.size() * 4 );
	std::memcpy( bufs[14].p, &P, sizeof P );
	RestirParams R;
	std::memset( &R, 0, sizeof R );
	const int nCand = 7;
	R.size[0] = q.w; R.size[1] = q.h; R.size[2] = nCand; R.size[3] = int32_t( c.seed * 41 % 1000 );
	R.opts[0] = q.C; R.opts[1] = q.K; R.opts[2] = q.f; R.opts[3] = q.red;
	const float ext[3] = { float( c.x ) * P.stepv[0], float( c.y ) * P.stepv[1], float( c.z ) * P.stepv[2] };
	const float diag = std::sqrt( ext[0] * ext[0] + ext[1] * ext[1] + ext[2] * ext[2] );
	R.knobs[0] = 6.0f; R.knobs[1] = 1e-3f * P.stepv[3]; R.knobs[2] = 1e-3f * P.stepv[3]; R.knobs[3] = float( q.hist );
	float qq[4] = { r.unit() - 0.5f, r.unit() - 0.5f, r.unit() - 0.5f, r.unit() - 0.5f };
	const float ql = std::sqrt( qq[0] * qq[0] + qq[1] * qq[1] + qq[2] * qq[2] + qq[3] * qq[3] ) + 1e-6f;
	for ( float & v : qq )
		v /= ql;
	const float w = qq[0], x = qq[1], y = qq[2], z = qq[3];
	const float Rm[3][3] = { { 1 - 2 * ( y * y + z * z ), 2 * ( x * y - w * z ), 2 * ( x * z + w * y ) },
		{ 2 * ( x * y + w * z ), 1 - 2 * ( x * x + z * z ), 2 * ( y * z - w * x ) },
		{ 2 * ( x * z - w * y ), 2 * ( y * z + w * x ), 1 - 2 * ( x * x + y * y ) } };
	for ( int i = 0; i < 3; i++ ) {
		const float cam = P.origin[i] + 0.5f * ext[i];
		for ( int k = 0; k < 3; k++ )
			R.rows[i][k] = R.prows[i][k] = Rm[i][k];
		R.rows[i][3] = cam;
		R.prows[i][3] = cam + ( r.unit() - 0.5f ) * 0.05f * P.stepv[3];
	}
	R.proj[0] = 1.3f; R.proj[1] = 1.7f; R.proj[2] = 0.02f; R.proj[3] = -0.01f;
	R.pproj[0] = 1.3f; R.pproj[1] = 1.7f; R.pproj[2] = 0.021f; R.pproj[3] = -0.011f;
	std::memcpy( bufs[15].p, &R, sizeof R );
	float * g = static_cast<float *>( bufs[16].p ), * pg = static_cast<float *>( bufs[17].p );
	const int bw = ( q.w + 7 ) / 8, bh = ( q.h + 7 ) / 8;
	std::vector<float> bn( size_t( bw ) * bh * 3 );
	for ( size_t i = 0; i < size_t( bw ) * bh; i++ ) {
		float n[3] = { r.unit() - 0.5f, r.unit() - 0.5f, 0.3f + r.unit() };
		const float nl = std::sqrt( n[0] * n[0] + n[1] * n[1] + n[2] * n[2] );
		for ( int k = 0; k < 3; k++ )
			bn[i * 3 + k] = n[k] / nl;
	}
	for ( uint64_t i = 0; i < nPix; i++ ) {
		const int px = int( i % uint64_t( q.w ) ), py = int( i / uint64_t( q.w ) );
		const float * n = &bn[( size_t( py / 8 ) * bw + size_t( px / 8 ) ) * 3];
		const float u = r.unit();
		float D = diag * ( 0.15f + 0.3f * ( 1.0f + std::sin( float( px ) * 0.02f ) * std::cos( float( py ) * 0.03f ) ) );
		if ( u < 0.05f )
			D = 1e6f;
		else if ( u < 0.08f )
			D = 0.0f;
		for ( int k = 0; k < 3; k++ ) {
			g[i * 4 + k] = n[k] + ( r.unit() - 0.5f ) * 0.02f;
			pg[i * 4 + k] = g[i * 4 + k] + ( r.unit() - 0.5f ) * 0.02f;
		}
		g[i * 4 + 3] = D;
		pg[i * 4 + 3] = D < 1e5f && D > 0.0f ? D * ( 1.0f + ( r.unit() - 0.5f ) * 0.04f ) : D;
	}
	auto res = [&]( float * o ) {
		o[0] = 0.0f;
		const int32_t yy = int32_t( r.next() % uint32_t( S.nLights + 1 ) ) - 1;
		std::memcpy( o, &yy, 4 );
		o[1] = r.unit() * 5.0f;
		o[2] = float( 1 + r.next() % 400 );   // M: past the history cap (20 x C) now and then
		o[3] = r.unit() * 2.0f;
	};
	float * cur = static_cast<float *>( bufs[18].p ), * hist = static_cast<float *>( bufs[19].p );
	int32_t * hp = static_cast<int32_t *>( bufs[20].p );
	float * nd = static_cast<float *>( bufs[21].p ), * acc = static_cast<float *>( bufs[22].p ), * prev = static_cast<float *>( bufs[23].p );
	int32_t * cand = static_cast<int32_t *>( bufs[24].p );
	float * out = static_cast<float *>( bufs[25].p );
	for ( uint64_t i = 0; i < nPix; i++ ) {
		res( cur + i * 4 );
		res( hist + i * 4 );
		hp[i] = int32_t( r.next() % uint32_t( nPix + 1 ) ) - 1;
		for ( int k = 0; k < 4; k++ ) {
			nd[i * 4 + k] = r.unit() * 3.0f;
			acc[i * 4 + k] = r.unit() * 3.0f;
		}
		prev[i * 4] = r.unit() * 3.0f;
		prev[i * 4 + 1] = r.unit() * 3.0f;
		prev[i * 4 + 2] = float( r.next() % 24 ) * 0.99f;   // frames: 0 (no history), past the 16 cap
		prev[i * 4 + 3] = 0.0f;
		out[i * 2] = r.unit();
		out[i * 2 + 1] = r.unit();
	}
	for ( int j = 0; j < nCand; j++ )
		cand[j] = j < S.nLights ? j : int32_t( r.next() % uint32_t( S.nLights ) );
	randomRotation( r, pc );
	pc[12] = c.mode;
	groups = ( nPix + 63 ) / 64;
}

// ReSTIR GI (HLSLRESTIR1): restirgi.comp's RestirGiParams has RestirParams' layout (11 vec4); size / options by case seed
// (rgOf). knobs: x history, y the running mean's frames cap, z the STBN mask bound, w the a-trous passes
struct RgCase { int w, h, K, R, f, red, hist, frames, mask, passes; };
static RgCase rgOf[128];   // by case seed
// the binding each mode writes: 0 cur, 1 est, 2 acc, 3 out, a-trous it even atrB, odd atrA
static int rgWatch( uint32_t mode ) { return mode == 0 ? 15 + 3 : mode == 1 ? 15 + 6 : mode == 2 ? 15 + 7 : mode == 3 ? 15 + 9 : ( mode & 1 ) ? 15 + 12 : 15 + 13; }

static uint32_t rgPackSnorm( float v )
{
	const float c = v < -1.0f ? -1.0f : v > 1.0f ? 1.0f : v;
	return uint32_t( int32_t( std::lround( c * 32767.0f ) ) ) & 0xFFFFu;
}

// set 0 = the trace's scene + TLAS + params and the bounce read's volume (1-4, as fillTrace); set 1 = 0 params, 1 g-buffer,
// 2 the previous one, 3 cur, 4 hist reservoirs (64 B), 5 history pixel, 6 estimate, 7 acc, 8 previous acc, 9 out, 10 the
// bounce's Lo, 11 STBN bytes, 12 / 13 a-trous ping-pong, 14 the packed g-buffer (host-packed for modes 3.., random below, so
// mode 2's own write shows and its red fails). The g-buffer as fillRestir's, some normals facing away (the octahedral fold)
static void fillRestirGi( std::vector<Buf> & bufs, const Case & c, uint64_t & groups, uint32_t pc[16] )
{
	const uint32_t nP = uint32_t( c.x * c.y * c.z );
	const RgCase & q = rgOf[c.seed % 128];
	const uint64_t nPix = uint64_t( q.w ) * q.h;
	Rng r { c.seed };
	Params P;
	traceParams( P, c, nP, r );
	fillRays( static_cast<float *>( bufs[0].p ), nP, c.N, r, P.stepv[3] );
	float * irr = static_cast<float *>( bufs[1].p );
	for ( size_t i = 0; i < size_t( nP ) * 64 * 4; i++ )
		irr[i] = r.unit() * 1.5f;
	float * dist = static_cast<float *>( bufs[2].p );
	for ( size_t i = 0; i < size_t( nP ) * 256 * 2; i++ )
		dist[i] = r.unit() * 3.0f * P.stepv[3];
	float * prb = static_cast<float *>( bufs[3].p );
	for ( uint32_t p = 0; p < nP; p++ ) {
		for ( int k = 0; k < 3; k++ )
			prb[p * 4 + k] = ( r.unit() - 0.5f ) * 0.8f * P.stepv[k];
		prb[p * 4 + 3] = float( r.next() % 4 );
	}
	uint32_t * age = static_cast<uint32_t *>( bufs[4].p );
	for ( uint32_t p = 0; p < nP; p++ )
		age[p] = r.next() % 4 == 0 ? 0u : 1 + r.next() % 100;
	std::memcpy( bufs[5].p, S.attr.data(), S.attr.size() * 4 );
	std::memcpy( bufs[7].p, S.remap.data(), S.remap.size() * 4 );
	std::memcpy( bufs[8].p, S.verts.data(), S.verts.size() * 4 );
	std::memcpy( bufs[9].p, S.triOf.data(), S.triOf.size() * 4 );
	std::memcpy( bufs[10].p, S.mask.data(), S.mask.size() * 4 );
	std::memcpy( bufs[11].p, S.maps.data(), S.maps.size() * 4 );
	std::memcpy( bufs[12].p, S.alpha.data(), S.alpha.size() * 4 );
	std::memcpy( bufs[13].p, S.lights.data(), S.lights.size() * 4 );
	std::memcpy( bufs[14].p, &P, sizeof P );
	RestirParams R;
	std::memset( &R, 0, sizeof R );
	R.size[0] = q.w; R.size[1] = q.h; R.size[2] = int32_t( c.seed * 7 % 64 ); R.size[3] = int32_t( c.seed * 41 % 100000 );
	R.opts[0] = q.K; R.opts[1] = q.R; R.opts[2] = q.f; R.opts[3] = q.red;
	const float ext[3] = { float( c.x ) * P.stepv[0], float( c.y ) * P.stepv[1], float( c.z ) * P.stepv[2] };
	const float diag = std::sqrt( ext[0] * ext[0] + ext[1] * ext[1] + ext[2] * ext[2] );
	R.knobs[0] = float( q.hist ); R.knobs[1] = float( q.frames ); R.knobs[2] = float( q.mask ); R.knobs[3] = float( q.passes );
	float qq[4] = { r.unit() - 0.5f, r.unit() - 0.5f, r.unit() - 0.5f, r.unit() - 0.5f };
	const float ql = std::sqrt( qq[0] * qq[0] + qq[1] * qq[1] + qq[2] * qq[2] + qq[3] * qq[3] ) + 1e-6f;
	for ( float & v : qq )
		v /= ql;
	const float w = qq[0], x = qq[1], y = qq[2], z = qq[3];
	const float Rm[3][3] = { { 1 - 2 * ( y * y + z * z ), 2 * ( x * y - w * z ), 2 * ( x * z + w * y ) },
		{ 2 * ( x * y + w * z ), 1 - 2 * ( x * x + z * z ), 2 * ( y * z - w * x ) },
		{ 2 * ( x * z - w * y ), 2 * ( y * z + w * x ), 1 - 2 * ( x * x + y * y ) } };
	for ( int i = 0; i < 3; i++ ) {
		const float cam = P.origin[i] + 0.5f * ext[i];
		for ( int k = 0; k < 3; k++ )
			R.rows[i][k] = R.prows[i][k] = Rm[i][k];
		R.rows[i][3] = cam;
		R.prows[i][3] = cam + ( r.unit() - 0.5f ) * 0.05f * P.stepv[3];
	}
	R.proj[0] = 1.3f; R.proj[1] = 1.7f; R.proj[2] = 0.02f; R.proj[3] = -0.01f;
	R.pproj[0] = 1.3f; R.pproj[1] = 1.7f; R.pproj[2] = 0.021f; R.pproj[3] = -0.011f;
	std::memcpy( bufs[15].p, &R, sizeof R );
	float * g = static_cast<float *>( bufs[16].p ), * pg = static_cast<float *>( bufs[17].p );
	const int bw = ( q.w + 7 ) / 8, bh = ( q.h + 7 ) / 8;
	std::vector<float> bn( size_t( bw ) * bh * 3 );
	for ( size_t i = 0; i < size_t( bw ) * bh; i++ ) {
		float n[3] = { r.unit() - 0.5f, r.unit() - 0.5f, 0.3f + r.unit() };
		if ( i % 5 == 0 )
			n[2] = -n[2];
		const float nl = std::sqrt( n[0] * n[0] + n[1] * n[1] + n[2] * n[2] );
		for ( int k = 0; k < 3; k++ )
			bn[i * 3 + k] = n[k] / nl;
	}
	for ( uint64_t i = 0; i < nPix; i++ ) {
		const int px = int( i % uint64_t( q.w ) ), py = int( i / uint64_t( q.w ) );
		const float * n = &bn[( size_t( py / 8 ) * bw + size_t( px / 8 ) ) * 3];
		const float u = r.unit();
		float D = diag * ( 0.15f + 0.3f * ( 1.0f + std::sin( float( px ) * 0.02f ) * std::cos( float( py ) * 0.03f ) ) );
		if ( u < 0.05f )
			D = 1e6f;
		else if ( u < 0.08f )
			D = 0.0f;
		for ( int k = 0; k < 3; k++ ) {
			g[i * 4 + k] = n[k] + ( r.unit() - 0.5f ) * 0.02f;
			pg[i * 4 + k] = g[i * 4 + k] + ( r.unit() - 0.5f ) * 0.02f;
		}
		g[i * 4 + 3] = D;
		pg[i * 4 + 3] = D < 1e5f && D > 0.0f ? D * ( 1.0f + ( r.unit() - 0.5f ) * 0.04f ) : D;
	}
	// a reservoir (RgRes, 16 floats): xs (a point in the volume, or a miss's direction), hit, ns, wsum, Lo, M, W, any, pads
	auto res = [&]( float * o ) {
		const bool hit = r.unit() < 0.8f;
		float d[3] = { r.unit() - 0.5f, r.unit() - 0.5f, r.unit() - 0.5f };
		const float dl = std::sqrt( d[0] * d[0] + d[1] * d[1] + d[2] * d[2] ) + 1e-6f;
		for ( int k = 0; k < 3; k++ )
			o[k] = hit ? P.origin[k] + r.unit() * ext[k] : d[k] / dl;
		o[3] = hit ? 1.0f : 0.0f;
		float nn[3] = { r.unit() - 0.5f, r.unit() - 0.5f, r.unit() - 0.5f };
		const float nl = std::sqrt( nn[0] * nn[0] + nn[1] * nn[1] + nn[2] * nn[2] ) + 1e-6f;
		for ( int k = 0; k < 3; k++ )
			o[4 + k] = nn[k] / nl;
		o[7] = r.unit() * 5.0f;
		for ( int k = 0; k < 3; k++ )
			o[8 + k] = r.unit() * 3.0f;
		o[11] = float( 1 + r.next() % 40 );   // M: past the history cap (20) now and then
		o[12] = r.unit() * 2.0f;
		o[13] = r.unit() < 0.85f ? 1.0f : 0.0f;
		o[14] = 0.0f;
		o[15] = 0.0f;
	};
	float * cur = static_cast<float *>( bufs[18].p ), * hist = static_cast<float *>( bufs[19].p );
	int32_t * hp = static_cast<int32_t *>( bufs[20].p );
	float * est = static_cast<float *>( bufs[21].p ), * acc = static_cast<float *>( bufs[22].p ), * prev = static_cast<float *>( bufs[23].p );
	float * out = static_cast<float *>( bufs[24].p ), * cand = static_cast<float *>( bufs[25].p );
	float * atA = static_cast<float *>( bufs[27].p ), * atB = static_cast<float *>( bufs[28].p );
	uint32_t * pk = static_cast<uint32_t *>( bufs[29].p );
	for ( uint64_t i = 0; i < nPix; i++ ) {
		res( cur + i * 16 );
		res( hist + i * 16 );
		hp[i] = r.unit() < 0.3f ? -1 : int32_t( r.next() % uint32_t( nPix ) );
		for ( int k = 0; k < 3; k++ ) {
			est[i * 4 + k] = r.unit() * 3.0f;
			acc[i * 4 + k] = r.unit() * 3.0f;
			prev[i * 4 + k] = r.unit() * 3.0f;
			out[i * 4 + k] = r.unit();
			cand[i * 4 + k] = r.unit();
			atA[i * 4 + k] = r.unit() * 3.0f;
			atB[i * 4 + k] = r.unit() * 3.0f;
		}
		const bool valid = g[i * 4 + 3] > 0.0f && g[i * 4 + 3] < 1e5f;
		est[i * 4 + 3] = valid && r.unit() < 0.7f ? 1.0f : 0.0f;
		acc[i * 4 + 3] = valid ? float( r.next() % 31 ) : 0.0f;   // frames 0..30: young (< 4) and past the cap
		prev[i * 4 + 3] = float( r.next() % 31 );
		out[i * 4 + 3] = r.unit();
		cand[i * 4 + 3] = r.unit() < 0.2f ? -1.0f : r.unit();
		atA[i * 4 + 3] = r.unit() * 0.5f;
		atB[i * 4 + 3] = r.unit() * 0.5f;
		if ( c.mode >= 3 ) {   // what mode 2 wrote (rgPack): the depth's bits, the octahedral normal in two snorm16
			const float * gg = g + i * 4;
			const float s = std::fabs( gg[0] ) + std::fabs( gg[1] ) + std::fabs( gg[2] );
			float p0 = s > 0.0f ? gg[0] / s : 0.0f, p1 = s > 0.0f ? gg[1] / s : 0.0f;
			if ( s > 0.0f && gg[2] < 0.0f ) {
				const float a0 = ( 1.0f - std::fabs( p1 ) ) * ( p0 >= 0.0f ? 1.0f : -1.0f ), a1 = ( 1.0f - std::fabs( p0 ) ) * ( p1 >= 0.0f ? 1.0f : -1.0f );
				p0 = a0;
				p1 = a1;
			}
			std::memcpy( pk + i * 2, gg + 3, 4 );
			pk[i * 2 + 1] = rgPackSnorm( p0 ) | ( rgPackSnorm( p1 ) << 16 );
		} else {
			pk[i * 2] = r.next();
			pk[i * 2 + 1] = r.next();
		}
	}
	unsigned char * stbn = static_cast<unsigned char *>( bufs[26].p );
	for ( size_t i = 0; i < size_t( 128 ) * 128 * 64; i++ )
		stbn[i] = static_cast<unsigned char>( r.next() & 255u );
	randomRotation( r, pc );
	pc[12] = c.mode;
	// restirgi_vk.inc: 8 x 8 tiles; modes 0 / 1 of the half / quarter trace (not red linear) tile the 2 x 1 / 2 x 2 blocks
	groups = uint64_t( ( q.w + 7 ) / 8 ) * uint64_t( ( q.h + 7 ) / 8 );
	const int tb = q.red & ( 512 | 1024 );
	if ( c.mode <= 1 && tb && !( q.red & 256 ) ) {
		const int bbw = ( q.w + 1 ) / 2, bbh = ( tb & 1024 ) ? ( q.h + 1 ) / 2 : q.h;
		groups = uint64_t( ( bbw + 7 ) / 8 ) * uint64_t( ( bbh + 7 ) / 8 );
	}
}

// bakert (bvh / rq / rqx, src/features/gi/bakert): bindings 0 rays, 1 hits, 2 triOf, 3 mask records, 4 maps, 5 alpha;
// rq / rqx 6 TLAS, 7 remap (BLAS primitive -> soup triangle), rqx 8 the true triangles a BLAS primitive; bvh 6 nodes,
// 7 idx, 8 the triangles in soup order. Push constants: first count useMask nTri nOpaque flags grow pad. One invocation a ray
struct BakeCase { uint32_t rays, first, useMask, flags; float grow; };
static BakeCase bakeOf[64];   // by case seed
static std::string bakeKernel;
static std::vector<float> bakeTris;       // bvh binding 8
static std::vector<uint32_t> bakeNodes;   // bvh binding 6: lo3 hi3 (float bits), left (-1 = a leaf), first (inner: the right child), count
static std::vector<int32_t> bakeIdx;      // bvh binding 7

static uint32_t bvhNode( uint32_t first, uint32_t count )
{
	const uint32_t n = uint32_t( bakeNodes.size() / 9 );
	bakeNodes.resize( bakeNodes.size() + 9 );
	float lo[3] = { 1e30f, 1e30f, 1e30f }, hi[3] = { -1e30f, -1e30f, -1e30f };
	for ( uint32_t i = first; i < first + count; i++ )
		for ( int j = 0; j < 3; j++ )
			for ( int k = 0; k < 3; k++ ) {
				const float v = bakeTris[size_t( bakeIdx[i] ) * 9 + size_t( j * 3 + k )];
				lo[k] = std::min( lo[k], v );
				hi[k] = std::max( hi[k], v );
			}
	for ( int k = 0; k < 3; k++ ) {
		std::memcpy( &bakeNodes[n * 9 + uint32_t( k )], &lo[k], 4 );
		std::memcpy( &bakeNodes[n * 9 + 3 + uint32_t( k )], &hi[k], 4 );
	}
	if ( count <= 4 ) {
		bakeNodes[n * 9 + 6] = 0xFFFFFFFFu;
		bakeNodes[n * 9 + 7] = first;
		bakeNodes[n * 9 + 8] = count;
		return n;
	}
	int ax = 0;
	for ( int k = 1; k < 3; k++ )
		ax = hi[k] - lo[k] > hi[ax] - lo[ax] ? k : ax;
	auto ctr = [ax]( int32_t t ) { const float * v = &bakeTris[size_t( t ) * 9]; return v[ax] + v[3 + ax] + v[6 + ax]; };
	std::sort( bakeIdx.begin() + first, bakeIdx.begin() + first + count,
		[&]( int32_t a, int32_t b ) { return ctr( a ) < ctr( b ) || ( ctr( a ) == ctr( b ) && a < b ); } );
	const uint32_t half = count / 2;
	const uint32_t l = bvhNode( first, half );
	const uint32_t r = bvhNode( first + half, count - half );
	bakeNodes[n * 9 + 6] = l;
	bakeNodes[n * 9 + 7] = r;
	bakeNodes[n * 9 + 8] = count;
	return n;
}

static void buildBvh()
{
	const size_t rows = S.remap.size();
	bakeTris.assign( rows * 9, 0.0f );
	for ( size_t i = 0; i < rows; i++ )
		std::memcpy( &bakeTris[size_t( S.remap[i] ) * 9], &S.verts[i * 9], 36 );
	bakeIdx.resize( rows );
	for ( size_t t = 0; t < rows; t++ )
		bakeIdx[t] = int32_t( t );
	bakeNodes.clear();
	bvhNode( 0, uint32_t( rows ) );
}

// rays: ~60 % aimed at a row's true triangle (some on an edge, some starting ~1e-4 off the surface, some ending just
// short of it), the rest random in the scene's box; ~5 % with a zero direction axis (bvh's flat slab); tmin 1e-4 or 0
static void fillBake( std::vector<Buf> & bufs, const Case & c, uint64_t & groups, uint32_t pc[16] )
{
	const BakeCase & q = bakeOf[c.seed % 64];
	const bool xr = bakeKernel == "rqx", bvh = bakeKernel == "bvh";
	for ( size_t i = 0; i < 15; i++ )   // makeBuf does not clear: every byte both runs see is written here
		std::memset( bufs[i].p, i == 1 ? 0x5A : 0, bufs[i].n );
	Rng r { c.seed * 31 + 7 };
	const size_t rows = S.remap.size();
	float lo[3] = { 1e30f, 1e30f, 1e30f }, hi[3] = { -1e30f, -1e30f, -1e30f };
	for ( size_t i = 0; i < S.verts.size(); i++ ) {
		lo[i % 3] = std::min( lo[i % 3], S.verts[i] );
		hi[i % 3] = std::max( hi[i % 3], S.verts[i] );
	}
	const float ext = std::max( hi[0] - lo[0], std::max( hi[1] - lo[1], hi[2] - lo[2] ) ), step = ext / 8.0f;
	const uint32_t n = q.first + q.rays;
	for ( uint32_t i = 0; i < n; i++ ) {
		float d[3] = { r.unit() - 0.5f, r.unit() - 0.5f, r.unit() - 0.5f };
		if ( r.unit() < 0.05f )
			d[r.next() % 3] = 0.0f;
		float dl = std::sqrt( d[0] * d[0] + d[1] * d[1] + d[2] * d[2] );
		if ( dl < 1e-3f ) {
			d[0] = 0.0f; d[1] = 0.0f; d[2] = 1.0f; dl = 1.0f;
		}
		for ( int k = 0; k < 3; k++ )
			d[k] /= dl;
		float o[3], tmax;
		const float tmin = r.unit() < 0.5f ? 1e-4f : 0.0f;
		if ( r.unit() < 0.6f ) {
			const float * v = &S.verts[size_t( r.next() % rows ) * 9];
			float b1 = r.unit(), b2 = r.unit();
			if ( b1 + b2 > 1.0f ) {
				b1 = 1.0f - b1;
				b2 = 1.0f - b2;
			}
			const float e = r.unit();
			if ( e < 0.1f )
				b1 = 0.0f;
			else if ( e < 0.2f )
				b2 = 1.0f - b1;
			const float b0 = 1.0f - b1 - b2;
			const float w = r.unit();
			const float dist = w < 0.15f ? 1e-4f * ( 1.0f + r.unit() ) : step * ( 0.2f + 2.0f * r.unit() );
			for ( int k = 0; k < 3; k++ )
				o[k] = b0 * v[k] + b1 * v[3 + k] + b2 * v[6 + k] - d[k] * dist;
			tmax = r.unit() < 0.15f ? dist * 0.99999f : dist + step * 4.0f * r.unit();
		} else {
			for ( int k = 0; k < 3; k++ )
				o[k] = lo[k] - step + r.unit() * ( hi[k] - lo[k] + 2.0f * step );
			tmax = ext * ( 0.2f + r.unit() );
		}
		if ( xr ) {
			double * x = static_cast<double *>( bufs[0].p ) + size_t( i ) * 8;
			double dd[3], ddl = 0.0;
			for ( int k = 0; k < 3; k++ ) {
				x[k] = double( o[k] ) + double( r.unit() ) * 1e-9;
				dd[k] = double( d[k] );
				ddl += dd[k] * dd[k];
			}
			ddl = std::sqrt( ddl );
			x[3] = double( tmax );
			for ( int k = 0; k < 3; k++ )
				x[4 + k] = dd[k] / ddl;
			x[7] = 0.0;
		} else {
			float * x = static_cast<float *>( bufs[0].p ) + size_t( i ) * 8;
			x[0] = o[0]; x[1] = o[1]; x[2] = o[2]; x[3] = tmin;
			x[4] = d[0]; x[5] = d[1]; x[6] = d[2]; x[7] = tmax;
		}
	}
	std::memcpy( bufs[2].p, S.triOf.data(), S.triOf.size() * 4 );
	std::memcpy( bufs[3].p, S.mask.data(), S.mask.size() * 4 );
	std::memcpy( bufs[4].p, S.maps.data(), S.maps.size() * 4 );
	std::memcpy( bufs[5].p, S.alpha.data(), S.alpha.size() * 4 );
	if ( bvh ) {
		std::memcpy( bufs[6].p, bakeNodes.data(), bakeNodes.size() * 4 );
		std::memcpy( bufs[7].p, bakeIdx.data(), bakeIdx.size() * 4 );
		std::memcpy( bufs[8].p, bakeTris.data(), bakeTris.size() * 4 );
	} else {
		std::memcpy( bufs[7].p, S.remap.data(), S.remap.size() * 4 );
		if ( xr )
			std::memcpy( bufs[8].p, S.verts.data(), S.verts.size() * 4 );
	}
	pc[0] = q.first; pc[1] = q.rays; pc[2] = q.useMask; pc[3] = uint32_t( rows ); pc[4] = uint32_t( S.nOp ); pc[5] = q.flags;
	std::memcpy( &pc[6], &q.grow, 4 );
	pc[7] = 0;
	const uint32_t local = bvh ? 64 : 128;
	groups = ( q.rays + local - 1 ) / local;
}

int main( int argc, char ** argv )
{
	const std::string kernel = argc > 1 ? argv[1] : "";
	typedef void ( *Fill )( std::vector<Buf> &, const Case &, uint64_t &, uint32_t * );
	Fill fill = nullptr;
	int watch = 1;   // the binding the GLSL run must change (a case that wrote nothing proves nothing)
	std::vector<Case> cases;
	if ( kernel == "border" ) {
		fill = fillBorder;
		cases = { { 4, 3, 2, 0, 0, 1 }, { 4, 3, 2, 1, 0, 2 }, { 9, 7, 5, 0, 1, 3 }, { 9, 7, 5, 1, 1, 4 },
			{ 16, 16, 8, 1, 1, 5 }, { 50, 50, 20, 0, 1, 6 } };
	} else if ( kernel == "reloc" ) {
		fill = fillReloc;
		watch = 3;
		// modes 4 / 5, every probe and fresh only, several ray counts. Past 65535 groups of 64 would take 4.2 M probes
		// (2 GB of rays a run): the y split is DDGI_LinearId's, shared with border, whose last case proves it
		cases = { { 4, 3, 2, 0, 0, 11, 64, 4, 0 }, { 4, 3, 2, 0, 0, 12, 64, 5, 0 }, { 9, 7, 5, 0, 0, 13, 96, 4, 1 },
			{ 9, 7, 5, 0, 0, 14, 96, 5, 1 }, { 20, 20, 10, 0, 0, 15, 288, 4, 0 }, { 20, 20, 10, 0, 0, 16, 288, 5, 0 },
			{ 64, 64, 16, 0, 0, 17, 32, 5, 0 }, { 64, 64, 16, 0, 0, 18, 32, 4, 0 } };
	} else if ( kernel == "blend" ) {
		fill = fillBlend;
		// rays 288 (all 256 threads load), 160, 64; skipAsleep and the skip range; sky-below rule on and off
		cases = { { 4, 3, 2, 0, 0, 21, 288, 2, 1, 1 }, { 4, 3, 2, 1, 1, 22, 288, 2, 1, 0 }, { 9, 7, 5, 0, 1, 23, 160, 2, 1, 1 },
			{ 9, 7, 5, 1, 0, 24, 64, 2, 1, 0 }, { 16, 16, 8, 0, 1, 25, 288, 2, 1, 1 } };
	} else if ( kernel == "trace" ) {
		fill = fillTrace;
		watch = 0;
		// modes 0 (all rays / past the fixed) and 1 (fixed, fresh only), skipAsleep + skip range, sky on / off, below on /
		// off, one bounce, and each runtime red the kernel reads: 8 HWHIT, 16 NOTIE, 1|2|4 NOBACK NOCHEB NOBIAS, 32 BOUNCENOCOVER.
		// The last case passes 65535 groups (the y split)
		cases = { { 4, 3, 2, 0, 0, 31, 64, 0, 0, 1 }, { 4, 3, 2, 0, 0, 32, 64, 1, 0, 1 }, { 4, 3, 2, 1, 0, 33, 64, 1, 1, 1 },
			{ 6, 5, 4, 1, 1, 34, 96, 0, 1, 0 }, { 6, 5, 4, 0, 0, 35, 96, 0, 0, 1, 8 }, { 6, 5, 4, 0, 0, 36, 96, 0, 0, 1, 16 },
			{ 6, 5, 4, 0, 0, 37, 96, 0, 0, 0, 7 }, { 6, 5, 4, 0, 0, 38, 96, 0, 0, 1, 32 }, { 6, 5, 4, 0, 0, 39, 96, 0, 0, 1, 0, 1 },
			{ 6, 5, 4, 0, 0, 40, 96, 0, 0, 1, 0, 0, 0 }, { 32, 32, 16, 0, 0, 41, 288, 0, 0, 1 } };
	} else if ( kernel == "airrays" ) {
		fill = fillAirRays;
		nSet1 = 4;
		watch = 15 + 3;
		// froxel grids, 1-4 samples (0 and 5 clamp), history on / off, each red bit (1 NOHOLE, 2 NOHIST, 4 NOSHADOW); the last
		// passes 65535 groups (the y split)
		const AirCase ac[] = { { 16, 9, 8, 2, 0, 1 }, { 16, 9, 8, 1, 0, 0 }, { 16, 9, 8, 4, 1, 1 }, { 16, 9, 8, 2, 2, 1 },
			{ 16, 9, 8, 3, 4, 1 }, { 32, 18, 16, 0, 0, 1 }, { 32, 18, 16, 5, 0, 1 }, { 256, 144, 128, 2, 0, 1 } };
		for ( int i = 0; i < 8; i++ ) {
			airOf[( 51 + i ) % 64] = ac[i];
			cases.push_back( Case { 6, 5, 4, 0, 0, uint64_t( 51 + i ) } );
		}
	} else if ( kernel == "restir" ) {
		fill = fillRestir;
		nSet1 = 11;
		// each mode (0 candidates + temporal, 1 spatial + the shadow ray, 2 accumulate, 3 ratio) with the red bits it reads
		// (1 NOVIS, 2 NOPDF, 4 NOTEMPORAL, 8 NOCLAMP, 16 NOHOLE), history on / off, K past the 16 cap, windows 0-2; the
		// 2048 x 2080 spatial case passes 65535 groups (the y split)
		struct { uint32_t mode; RestirCase q; } rc[] = { { 0, { 64, 48, 8, 4, 1, 0, 1 } }, { 0, { 64, 48, 4, 4, 1, 4, 1 } },
			{ 0, { 96, 64, 16, 4, 1, 8, 1 } }, { 0, { 64, 48, 8, 4, 1, 2, 0 } }, { 1, { 64, 48, 8, 4, 1, 0, 1 } },
			{ 1, { 64, 48, 8, 20, 1, 1, 1 } }, { 1, { 96, 64, 8, 8, 1, 16, 1 } }, { 1, { 64, 48, 8, 6, 1, 2, 1 } },
			{ 2, { 64, 48, 8, 4, 1, 0, 1 } }, { 2, { 64, 48, 8, 4, 1, 4, 1 } }, { 3, { 64, 48, 8, 4, 1, 0, 1 } },
			{ 3, { 96, 64, 8, 4, 2, 0, 1 } }, { 3, { 64, 48, 8, 4, 0, 0, 1 } }, { 1, { 2048, 2080, 8, 4, 1, 0, 1 } } };
		for ( int i = 0; i < int( sizeof rc / sizeof rc[0] ); i++ ) {
			restirOf[( 61 + i ) % 128] = rc[i].q;
			Case k { 6, 5, 4, 0, 0, uint64_t( 61 + i ) };
			k.mode = rc[i].mode;
			cases.push_back( k );
		}
	} else if ( kernel == "restirgi" || kernel == "restirgi_filt" ) {
		fill = fillRestirGi;
		nSet1 = 15;
		// { w h K R f red hist frames mask passes }: modes 0 (bounce + temporal) and 1 (spatial) with each red bit the kernel
		// reads (1 NOJACOBIAN, 2 NOVIS, 4 NOPDF, 8 NOTEMPORAL, 16 WHITE, 32 R2, 64 NOJCLAMP, 256 LINEAR, 512 HALF, 1024 QUARTER,
		// 2048 MERGEVIS, 4096 ALLLIGHTS), history and the STBN mask on / off, K past the 15 cap, the DDGI one bounce / sky off /
		// hardware hit; 2 accumulate, 3 filter (f 0-2), 4.. a-trous (the last pass writes out). The filter kernel runs modes 2..
		// only. The 2048 x 2080 filter case passes 65535 groups (the y split)
		struct { uint32_t mode; RgCase q; int oneBounce, sky, ddgiRed; } gc[] = {
			{ 0, { 64, 48, 4, 3, 1, 0, 1, 16, 1, 3 }, 0, 1, 0 }, { 0, { 64, 48, 4, 3, 1, 16, 1, 16, 1, 3 }, 0, 1, 0 },
			{ 0, { 64, 48, 4, 3, 1, 32, 1, 16, 0, 3 }, 0, 1, 0 }, { 0, { 64, 48, 4, 3, 1, 8, 1, 16, 1, 3 }, 0, 1, 0 },
			{ 0, { 64, 48, 4, 3, 1, 0, 0, 16, 1, 3 }, 0, 1, 0 }, { 0, { 66, 50, 4, 3, 1, 512, 1, 16, 1, 3 }, 0, 1, 0 },
			{ 0, { 66, 50, 4, 3, 1, 1024, 1, 16, 1, 3 }, 0, 1, 0 }, { 0, { 64, 48, 4, 3, 1, 256, 1, 16, 1, 3 }, 0, 1, 0 },
			{ 0, { 64, 48, 4, 3, 1, 4096, 1, 16, 1, 3 }, 0, 1, 0 }, { 0, { 64, 48, 4, 3, 1, 4, 1, 16, 1, 3 }, 0, 1, 0 },
			{ 0, { 64, 48, 4, 3, 1, 0, 1, 16, 1, 3 }, 1, 1, 0 }, { 0, { 64, 48, 4, 3, 1, 0, 1, 16, 1, 3 }, 0, 0, 0 },
			{ 0, { 64, 48, 4, 3, 1, 0, 1, 16, 1, 3 }, 0, 1, 8 }, { 0, { 64, 48, 4, 3, 1, 1024 | 256, 1, 16, 1, 3 }, 0, 1, 0 },
			{ 1, { 64, 48, 4, 3, 1, 0, 1, 16, 1, 3 }, 0, 1, 0 }, { 1, { 64, 48, 20, 3, 1, 0, 1, 16, 1, 3 }, 0, 1, 0 },
			{ 1, { 64, 48, 6, 8, 1, 1, 1, 16, 1, 3 }, 0, 1, 0 }, { 1, { 64, 48, 4, 3, 1, 2, 1, 16, 1, 3 }, 0, 1, 0 },
			{ 1, { 64, 48, 4, 3, 1, 64, 1, 16, 1, 3 }, 0, 1, 0 }, { 1, { 64, 48, 4, 3, 1, 2048, 1, 16, 1, 3 }, 0, 1, 0 },
			{ 1, { 66, 50, 4, 3, 1, 512, 1, 16, 1, 3 }, 0, 1, 0 }, { 1, { 66, 50, 4, 3, 1, 1024, 1, 16, 1, 3 }, 0, 1, 0 },
			{ 1, { 64, 48, 4, 3, 1, 256, 1, 16, 1, 3 }, 0, 1, 0 }, { 1, { 64, 48, 4, 3, 1, 4, 1, 16, 1, 3 }, 0, 1, 0 },
			{ 2, { 64, 48, 4, 3, 1, 0, 1, 16, 1, 3 }, 0, 1, 0 }, { 2, { 64, 48, 4, 3, 1, 0, 0, 16, 1, 3 }, 0, 1, 0 },
			{ 2, { 66, 50, 4, 3, 1, 512, 1, 1, 1, 3 }, 0, 1, 0 }, { 2, { 64, 48, 4, 3, 1, 1024, 1, 30, 1, 3 }, 0, 1, 0 },
			{ 3, { 64, 48, 4, 3, 1, 0, 1, 16, 1, 3 }, 0, 1, 0 }, { 3, { 64, 48, 4, 3, 0, 0, 1, 16, 1, 3 }, 0, 1, 0 },
			{ 3, { 66, 50, 4, 3, 2, 0, 1, 16, 1, 3 }, 0, 1, 0 }, { 4, { 64, 48, 4, 3, 1, 0, 1, 16, 1, 3 }, 0, 1, 0 },
			{ 5, { 64, 48, 4, 3, 1, 0, 1, 16, 1, 3 }, 0, 1, 0 }, { 6, { 64, 48, 4, 3, 1, 0, 1, 16, 1, 3 }, 0, 1, 0 },
			{ 4, { 66, 50, 4, 3, 1, 0, 1, 16, 1, 1 }, 0, 1, 0 }, { 3, { 2048, 2080, 4, 3, 1, 0, 1, 16, 1, 3 }, 0, 1, 0 } };
		for ( int i = 0; i < int( sizeof gc / sizeof gc[0] ); i++ ) {
			if ( kernel == "restirgi_filt" && gc[i].mode < 2 )
				continue;
			rgOf[( 1 + i ) % 128] = gc[i].q;
			Case k { 6, 5, 4, 0, 0, uint64_t( 1 + i ) };
			k.mode = gc[i].mode;
			k.oneBounce = gc[i].oneBounce;
			k.sky = gc[i].sky;
			k.red = gc[i].ddgiRed;
			cases.push_back( k );
		}
	} else if ( kernel == "bvh"|| kernel == "rq" || kernel == "rqx" ) {
		fill = fillBake;
		bakeKernel = kernel;
		bakeScene = true;
		storage6 = kernel == "bvh";
		// rays, first, useMask, flags (1 any hit / occlusion; rqx 2 = the graze red), grow (rqx's narrowed edge band)
		std::vector<BakeCase> bc;
		if ( kernel == "rqx" )
			bc = { { 30000, 0, 1, 0, 0.0f }, { 30000, 37, 1, 1, 0.0f }, { 30000, 0, 1, 0, 0.05f }, { 30000, 0, 1, 2, 0.0f },
				{ 20000, 0, 0, 0, 0.05f }, { 30000, 11, 1, 1, 0.05f } };
		else
			bc = { { 30000, 0, 1, 0, 0.0f }, { 30000, 37, 1, 1, 0.0f }, { 20000, 0, 0, 0, 0.0f }, { 70000, 5, 1, 0, 0.0f } };
		for ( size_t i = 0; i < bc.size(); i++ ) {
			bakeOf[( 81 + i ) % 64] = bc[i];
			cases.push_back( Case { 4, 3, 2, 0, 0, uint64_t( 81 + i ) } );
		}
	}
	if ( argc < 4 || !fill ) {
		std::printf( "usage: kernelmatch border|reloc|blend|trace <glsl .spv> <hlsl .spv>\n" );
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
	VkPhysicalDeviceFeatures feat {};
	feat.shaderFloat64 = VK_TRUE;   // bakert's rqx device (never shaderInt64)
	di.pEnabledFeatures = &feat;
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
	LOADD( vkCreateFence ); LOADD( vkWaitForFences ); LOADD( vkDestroyFence );
	LOADD( vkQueueSubmit ); LOADD( vkQueueWaitIdle ); LOADD( vkResetCommandBuffer ); LOADD( vkDestroyPipeline );
	LOADD( vkDestroyShaderModule ); LOADD( vkDestroyBuffer ); LOADD( vkFreeMemory );
	LOADD( vkGetBufferDeviceAddress ); LOADD( vkCreateAccelerationStructureKHR ); LOADD( vkDestroyAccelerationStructureKHR );
	LOADD( vkGetAccelerationStructureBuildSizesKHR ); LOADD( vkCmdBuildAccelerationStructuresKHR );
	LOADD( vkGetAccelerationStructureDeviceAddressKHR ); LOADD( vkCmdPipelineBarrier );
	vkGetDeviceQueue( dev, family, 0, &queue );
	if ( !makeLayout() ) {
		std::printf( "HLSLMATCH FAIL (layout)\n" );
		return 1;
	}
	std::printf( "device queue family %u\n", family );
	// cases (per kernel above): small grids with every state and both skip rules, border's last past 65535 groups
	int fails = 0, total = 0;
	for ( const Case & c : cases ) {
		const size_t nP = size_t( c.x * c.y * c.z );
		const size_t nb = 15 + nSet1;
		std::vector<size_t> sz = { nP * size_t( c.N ) * 16, nP * 64 * 16, nP * 256 * 8, nP * 16, nP * 4, 256, 256, 256, 256, 256, 256, 256,
			256, 256, sizeof( Params ) };
		if ( kernel == "airrays" ) {
			const AirCase & q = airOf[c.seed % 64];
			const size_t nF = size_t( q.fx ) * q.fy * q.fz;
			sz.insert( sz.end(), { sizeof( AirParams ), nF * 16, nF * 16, nF * 8 } );
		}
		if ( kernel == "restir" ) {
			const RestirCase & q = restirOf[c.seed % 128];
			const size_t n = size_t( q.w ) * q.h;
			sz.insert( sz.end(), { sizeof( RestirParams ), n * 16, n * 16, n * 16, n * 16, n * 4, n * 16, n * 16, n * 16, 64, n * 8 } );
			watch = kRestirWatch[c.mode & 3];
		}
		const bool rg = kernel == "restirgi" || kernel == "restirgi_filt";
		if ( rg ) {
			const RgCase & q = rgOf[c.seed % 128];
			const size_t n = size_t( q.w ) * q.h;
			sz.insert( sz.end(), { sizeof( RestirParams ), n * 16, n * 16, n * 64, n * 64, n * 4, n * 16, n * 16, n * 16, n * 16, n * 16,
				size_t( 128 ) * 128 * 64, n * 16, n * 16, n * 8 } );
			watch = rgWatch( c.mode );
		}
		if ( kernel == "trace" || kernel == "airrays" || kernel == "restir" || rg ) {   // the soup, sized per binding (one scene, one TLAS, read by both runs)
			Params P0;
			Rng r0 { c.seed };
			fillParamsBase( P0, c, uint32_t( nP ), r0 );
			makeScene( c, P0 );
			const size_t s[15] = { 0, 0, 0, 0, 0, S.attr.size() * 4, 0, S.remap.size() * 4, S.verts.size() * 4, S.triOf.size() * 4,
				S.mask.size() * 4, S.maps.size() * 4, S.alpha.size() * 4, S.lights.size() * 4, 0 };
			for ( int i = 5; i < 14; i++ )
				sz[i] = std::max( sz[i], s[i] );
		}
		if ( bakeScene ) {   // bakert's own binding sizes (the scene's, one BLAS)
			Params P0;
			Rng r0 { c.seed };
			fillParamsBase( P0, c, uint32_t( nP ), r0 );
			makeScene( c, P0 );
			if ( storage6 )
				buildBvh();
			const BakeCase & q = bakeOf[c.seed % 64];
			const size_t nr = size_t( q.first ) + q.rays;
			sz = { nr * ( kernel == "rqx" ? 64 : 32 ), nr * 8, S.triOf.size() * 4, S.mask.size() * 4, S.maps.size() * 4, S.alpha.size() * 4,
				storage6 ? bakeNodes.size() * 4 : 256, storage6 ? bakeIdx.size() * 4 : S.remap.size() * 4,
				storage6 ? bakeTris.size() * 4 : kernel == "rqx" ? S.verts.size() * 4 : 256, 256, 256, 256, 256, 256, 256 };
		}
		std::vector<Buf> A( nb ), B( nb );
		for ( size_t i = 0; i < nb; i++ ) {
			A[i] = makeBuf( sz[i] );
			B[i] = makeBuf( sz[i] );
		}
		if ( sceneOn && !buildScene() ) {
			std::printf( "HLSLMATCH FAIL (the TLAS did not build)\n" );
			return 1;
		}
		uint64_t groups = 0;
		uint32_t pc[16] = {};
		fill( A, c, groups, pc );
		fill( B, c, groups, pc );
		std::vector<unsigned char> before( sz[watch] );
		std::memcpy( before.data(), A[watch].p, sz[watch] );
		const bool ra = run( a, A, pc, groups ), rb = run( b, B, pc, groups );
		std::vector<size_t> diff( nb, 0 );
		for ( size_t i = 0; i < nb; i++ ) {
			const unsigned char * x = static_cast<const unsigned char *>( A[i].p ), * y = static_cast<const unsigned char *>( B[i].p );
			for ( size_t k = 0; k < sz[i]; k++ )
				diff[i] += x[k] != y[k];
		}
		size_t moved = 0;   // the GLSL run's own writes to the watched binding: a case that wrote nothing proves nothing
		for ( size_t k = 0; k < sz[watch]; k++ )
			moved += before[k] != static_cast<const unsigned char *>( A[watch].p )[k];
		size_t all = 0;
		for ( size_t i = 0; i < nb; i++ )
			all += diff[i];
		if ( kernel == "airrays" ) {   // coverage: froxels lit by a lamp (frames > 0), and those with a shadowed share (ratio < 1)
			const float * o = static_cast<const float *>( A[18].p );
			size_t lit = 0, shadowed = 0;
			for ( size_t k = 0; k < sz[18] / 8; k++ ) {
				lit += o[k * 2 + 1] > 0.0f;
				shadowed += o[k * 2 + 1] > 0.0f && o[k * 2] < 1.0f;
			}
			std::printf( "  air: samples %d red %d history %d, froxels with a lamp %zu, with a shadowed share %zu\n", airOf[c.seed % 64].samples,
				airOf[c.seed % 64].red, airOf[c.seed % 64].hist, lit, shadowed );
		}
		if ( kernel == "restir" ) {   // coverage: mode 0 pixels with a light kept / a history pixel; mode 1 shadow rays blocked / open
			const int32_t * cur = static_cast<const int32_t *>( A[18].p ), * hp = static_cast<const int32_t *>( A[20].p );
			const float * nd = static_cast<const float *>( A[21].p );
			size_t kept = 0, withHist = 0, blocked = 0, open = 0;
			for ( size_t k = 0; k < sz[20] / 4; k++ ) {
				kept += cur[k * 4] >= 0;
				withHist += hp[k] >= 0;
				blocked += nd[k * 4 + 2] == 1.0f;
				open += nd[k * 4 + 2] == 0.0f;
			}
			const RestirCase & q = restirOf[c.seed % 128];
			std::printf( "  restir: mode %u C %d K %d f %d red %d history %d; light kept %zu, history pixel %zu, rays blocked %zu open %zu\n",
				c.mode, q.C, q.K, q.f, q.red, q.hist, kept, withHist, blocked, open );
		}
		if ( rg ) {   // coverage: reservoirs holding a sample / a hit, history pixels, valid estimates, bounce hits (cand.w >= 0)
			const float * cur = static_cast<const float *>( A[18].p ), * est = static_cast<const float *>( A[21].p ), * cd = static_cast<const float *>( A[25].p );
			const int32_t * hp = static_cast<const int32_t *>( A[20].p );
			size_t held = 0, hits = 0, withHist = 0, valid = 0, bounce = 0;
			for ( size_t k = 0; k < sz[20] / 4; k++ ) {
				held += cur[k * 16 + 13] > 0.5f;
				hits += cur[k * 16 + 13] > 0.5f && cur[k * 16 + 3] > 0.5f;
				withHist += hp[k] >= 0;
				valid += est[k * 4 + 3] > 0.5f;
				bounce += cd[k * 4 + 3] >= 0.0f;
			}
			const RgCase & q = rgOf[c.seed % 128];
			std::printf( "  restirgi: mode %u %dx%d K %d R %d f %d red %d history %d frames %d mask %d passes %d; held %zu hit %zu, "
				"history pixel %zu, estimate valid %zu, bounce %zu\n", c.mode, q.w, q.h, q.K, q.R, q.f, q.red, q.hist, q.frames, q.mask,
				q.passes, held, hits, withHist, valid, bounce );
		}
		if ( bakeScene ) {   // coverage: hits, hits on an alpha-tested triangle; rqx the unsure reasons by bit
			const BakeCase & q = bakeOf[c.seed % 64];
			const uint32_t * h = static_cast<const uint32_t *>( A[1].p ) + size_t( q.first ) * 2;
			size_t hit = 0, masked = 0, why[6] = {};
			for ( uint32_t k = 0; k < q.rays; k++ ) {
				const uint32_t t = kernel == "rqx" ? h[k * 2] : h[k * 2 + 1];
				if ( t != 0xFFFFFFFFu ) {
					hit++;
					masked += t < S.triOf.size() && S.triOf[t] >= 0;
				}
				for ( int b = 0; b < 6 && kernel == "rqx"; b++ )
					why[b] += ( h[k * 2 + 1] >> b ) & 1u;
			}
			std::printf( "  %s: rays %u first %u useMask %u flags %u grow %g; hits %zu, on a masked triangle %zu", kernel.c_str(), q.rays,
				q.first, q.useMask, q.flags, double( q.grow ), hit, masked );
			if ( kernel == "rqx" )
				std::printf( "; unsure 1:%zu 2:%zu 4:%zu 8:%zu 16:%zu 32:%zu", why[0], why[1], why[2], why[3], why[4], why[5] );
			std::printf( "\n" );
		}
		const bool ok = ra && rb && all == 0 && moved > 0;
		fails += ok ? 0 : 1;
		total++;
		std::printf( "case %dx%dx%d rays %d mode %u.%u skipAsleep %d skip %d below %d groups %llu: ran %d/%d, bytes differing rays %zu "
			"irr %zu dist %zu age %zu probes %zu other %zu, glsl bytes written (binding %d) %zu: %s\n", c.x, c.y, c.z, c.N, pc[12], pc[13],
			c.skipAsleep, c.skip, c.below, (unsigned long long)groups, ra, rb, diff[0], diff[1], diff[2], diff[4], diff[3],
			all - diff[0] - diff[1] - diff[2] - diff[3] - diff[4], watch, moved, ok ? "same" : "DIFFERS" );
		for ( int i = 0; i < int( nb ) && !ok; i++ ) {   // the first differing word a binding (15+ = set 1), both values, the largest ulp gap
			const uint32_t * x = static_cast<const uint32_t *>( A[i].p ), * y = static_cast<const uint32_t *>( B[i].p );
			size_t first = SIZE_MAX;
			int64_t worst = 0;
			for ( size_t k = 0; k < sz[i] / 4; k++ )
				if ( x[k] != y[k] ) {
					first = first == SIZE_MAX ? k : first;
					const int64_t g = int64_t( x[k] ) - int64_t( y[k] );
					worst = std::max( worst, g < 0 ? -g : g );
				}
			if ( first != SIZE_MAX ) {
				float fx, fy;
				std::memcpy( &fx, x + first, 4 );
				std::memcpy( &fy, y + first, 4 );
				std::printf( "  binding %d word %zu: glsl %08x (%g) hlsl %08x (%g); largest gap %lld ulp\n", i, first, x[first], fx, y[first], fy,
					(long long)worst );
			}
		}
		std::fflush( stdout );
		for ( size_t i = 0; i < nb; i++ ) {
			freeBuf( A[i] );
			freeBuf( B[i] );
		}
		if ( sceneOn )
			freeScene();
	}
	if ( fails )
		std::printf( "HLSLMATCH %s FAIL %d of %d cases\n", argv[1], fails, total );
	else
		std::printf( "HLSLMATCH %s PASS %d cases\n", argv[1], total );
	return fails ? 1 : 0;
}
