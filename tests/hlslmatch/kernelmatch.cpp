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
struct Case { int x, y, z; int skipAsleep; int skip; uint64_t seed; int N = 288; uint32_t mode = 3; uint32_t jobY = 0; int below = 1; };

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
	}
	if ( argc < 4 || !fill ) {
		std::printf( "usage: kernelmatch border|reloc|blend <glsl .spv> <hlsl .spv>\n" );
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
	LOADD( vkCreateFence ); LOADD( vkWaitForFences ); LOADD( vkDestroyFence );
	LOADD( vkQueueSubmit ); LOADD( vkQueueWaitIdle ); LOADD( vkResetCommandBuffer ); LOADD( vkDestroyPipeline );
	LOADD( vkDestroyShaderModule ); LOADD( vkDestroyBuffer ); LOADD( vkFreeMemory );
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
		const size_t sz[15] = { nP * size_t( c.N ) * 16, nP * 64 * 16, nP * 256 * 8, nP * 16, nP * 4, 256, 256, 256, 256, 256, 256, 256,
			256, 256, sizeof( Params ) };
		std::vector<Buf> A( 15 ), B( 15 );
		for ( int i = 0; i < 15; i++ ) {
			A[i] = makeBuf( sz[i] );
			B[i] = makeBuf( sz[i] );
		}
		uint64_t groups = 0;
		uint32_t pc[16] = {};
		fill( A, c, groups, pc );
		fill( B, c, groups, pc );
		std::vector<unsigned char> before( sz[watch] );
		std::memcpy( before.data(), A[watch].p, sz[watch] );
		const bool ra = run( a, A, pc, groups ), rb = run( b, B, pc, groups );
		size_t diff[15] = {};
		for ( int i = 0; i < 15; i++ ) {
			const unsigned char * x = static_cast<const unsigned char *>( A[i].p ), * y = static_cast<const unsigned char *>( B[i].p );
			for ( size_t k = 0; k < sz[i]; k++ )
				diff[i] += x[k] != y[k];
		}
		size_t moved = 0;   // the GLSL run's own writes to the watched binding: a case that wrote nothing proves nothing
		for ( size_t k = 0; k < sz[watch]; k++ )
			moved += before[k] != static_cast<const unsigned char *>( A[watch].p )[k];
		size_t all = 0;
		for ( int i = 0; i < 15; i++ )
			all += diff[i];
		const bool ok = ra && rb && all == 0 && moved > 0;
		fails += ok ? 0 : 1;
		total++;
		std::printf( "case %dx%dx%d rays %d mode %u.%u skipAsleep %d skip %d below %d groups %llu: ran %d/%d, bytes differing rays %zu "
			"irr %zu dist %zu age %zu probes %zu other %zu, glsl bytes written (binding %d) %zu: %s\n", c.x, c.y, c.z, c.N, pc[12], pc[13],
			c.skipAsleep, c.skip, c.below, (unsigned long long)groups, ra, rb, diff[0], diff[1], diff[2], diff[4], diff[3],
			all - diff[0] - diff[1] - diff[2] - diff[3] - diff[4], watch, moved, ok ? "same" : "DIFFERS" );
		for ( int i = 0; i < 15 && !ok; i++ ) {   // the first differing word a binding, both values, and the largest ulp gap
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
