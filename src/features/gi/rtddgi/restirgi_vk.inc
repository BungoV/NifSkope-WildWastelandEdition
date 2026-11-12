// Lane RTEMIS1 part 2 (2026-11-06): ReSTIR GI on the DDGI volume's device and BVH (src/features/gi/rtddgi/rtddgi_gpu.h
// restirGiGpu*), on restir_vk.inc's pattern. Included at the end of rtddgi_vk.inc's kernel branch (inside namespace bakert).
// Kernel: restirgi.comp (src/features/gi/rtddgi_spirv.sh -> restirgi.spv.h; without the header every call refuses).
// Set 0 = the volume's own descriptor set (TLAS, true triangles, alpha masks, lights, probes); set 1 = the 11 buffers below.

#if defined( __has_include )
#if __has_include( "restirgi.spv.h" )
#define RESTIRGI_HAVE_SPIRV 1
#endif
#endif

#ifndef RESTIRGI_HAVE_SPIRV

namespace {
void restirGiFreeFor( Dev &, DdgiGpuSession * ) {}
}
bool restirGiGpuStep( DdgiGpuSession *, const RestirGiGpuIn &, RestirGiGpuOut * out )
{
	*out = RestirGiGpuOut();
	out->why = "the ReSTIR GI kernel is not compiled into this build (run src/features/gi/rtddgi_spirv.sh, then rebuild)";
	return false;
}
bool restirGiGpuShare( DdgiGpuSession *, int, int, RestirGpuShare * out )
{
	*out = RestirGpuShare();
	out->why = "the ReSTIR GI kernel is not compiled into this build";
	return false;
}

#else

namespace {
#include "features/gi/rtddgi/restirgi.spv.h"
#if __has_include( "restirgi_filt.spv.h" )
#include "features/gi/rtddgi/restirgi_filt.spv.h"
#define RESTIRGI_HAVE_FILT 1
#endif
// HLSLRESTIR1: the two headers above are DXC's (res/shaders/hlsl/rtddgi/restirgi.hlsl); glslang's of restirgi.comp stay as
// the red WW_CELL_RESTIRGI_RED=glsl (tests/hlslmatch restirgi / restirgi_filt: the two byte-exact on the same buffers)
#if __has_include( "restirgi_glsl.spv.h" ) && __has_include( "restirgi_filt_glsl.spv.h" )
#include "features/gi/rtddgi/restirgi_glsl.spv.h"
#include "features/gi/rtddgi/restirgi_filt_glsl.spv.h"
#define RESTIRGI_HAVE_GLSL 1
#endif

constexpr int kRgBindings = 15;   // restirgi.comp set 1
constexpr VkDeviceSize kRgStbnBytes = 128 * 128 * 64;   // res/air/stbn_scalar_128x128x64.r8
constexpr uint32_t kRgStamps = 10;   // one before each mode (4 + up to 5 a-trous passes), the end
constexpr VkDeviceSize kRgParamBytes = 11 * 16;   // RestirGiParams, std430: size, opts, knobs, rows[3], proj, prows[3], pproj
constexpr VkDeviceSize kRgResBytes = 64;          // restirgi_common.glsl RgRes

struct RgPipe
{
	bool tried = false, ok = false;
	std::string why;
	VkDescriptorSetLayout dsl = VK_NULL_HANDLE;
	VkPipelineLayout pl = VK_NULL_HANDLE;
	VkPipeline pipe = VK_NULL_HANDLE;
	VkPipeline pipeF = VK_NULL_HANDLE;   // modes 2.. (restirgi.comp -DRESTIRGI_FILTER); none: pipe runs them
	const char * kernel = "hlsl";        // the SPIR-V the pipelines came from (the red: "glsl")
};

RgPipe & rgPipe( Dev & D, DdgiPipes & S )
{
	static RgPipe R;
	if ( R.tried )
		return R;
	R.tried = true;
	std::vector<VkDescriptorSetLayoutBinding> b;
	for ( int i = 0; i < kRgBindings; i++ ) {
		VkDescriptorSetLayoutBinding x {};
		x.binding = uint32_t( i );
		x.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		x.descriptorCount = 1;
		x.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
		b.push_back( x );
	}
	VkDescriptorSetLayoutCreateInfo li { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
	li.bindingCount = uint32_t( b.size() );
	li.pBindings = b.data();
	if ( vk.vkCreateDescriptorSetLayout( D.dev, &li, nullptr, &R.dsl ) != VK_SUCCESS ) {
		R.why = "no ReSTIR GI descriptor set layout";
		return R;
	}
	const VkDescriptorSetLayout sets[2] = { S.dsl, R.dsl };
	VkPushConstantRange pr { VK_SHADER_STAGE_COMPUTE_BIT, 0, uint32_t( sizeof( DdgiPc ) ) };
	VkPipelineLayoutCreateInfo pi { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
	pi.setLayoutCount = 2;
	pi.pSetLayouts = sets;
	pi.pushConstantRangeCount = 1;
	pi.pPushConstantRanges = &pr;
	if ( vk.vkCreatePipelineLayout( D.dev, &pi, nullptr, &R.pl ) != VK_SUCCESS ) {
		R.why = "no ReSTIR GI pipeline layout";
		return R;
	}
	// the red list's `glsl` token (beside rtddgicell.cpp restirGiRedBits' kernel bits), read once at the first frame
	const char * red = std::getenv( "WW_CELL_RESTIRGI_RED" );
	bool glsl = false;
	for ( const char * p = red; p && *p; ) {
		while ( *p == ',' || *p == ' ' )
			p++;
		const char * e = p;
		while ( *e && *e != ',' && *e != ' ' )
			e++;
		glsl = glsl || ( e - p == 4 && std::strncmp( p, "glsl", 4 ) == 0 );
		p = e;
	}
#ifndef RESTIRGI_HAVE_GLSL
	glsl = false;
#endif
	R.kernel = glsl ? "glsl" : "hlsl";
#ifdef RESTIRGI_HAVE_GLSL
	if ( glsl )
		R.pipe = pipeline( D, rtddgi_restirgi_glsl_spv, sizeof rtddgi_restirgi_glsl_spv, R.pl );
	else
#endif
		R.pipe = pipeline( D, rtddgi_restirgi_spv, sizeof rtddgi_restirgi_spv, R.pl );
	R.ok = R.pipe != VK_NULL_HANDLE;
	if ( !R.ok )
		R.why = "the ReSTIR GI kernel did not build";
#ifdef RESTIRGI_HAVE_FILT
	if ( R.ok && !std::getenv( "WW_CELL_RESTIRGI_ONEKERNEL" ) ) {
#ifdef RESTIRGI_HAVE_GLSL
		if ( glsl )
			R.pipeF = pipeline( D, rtddgi_restirgi_filt_glsl_spv, sizeof rtddgi_restirgi_filt_glsl_spv, R.pl );
		else
#endif
			R.pipeF = pipeline( D, rtddgi_restirgi_filt_spv, sizeof rtddgi_restirgi_filt_spv, R.pl );
	}
#endif
	return R;
}

//! one session's ReSTIR GI state (W x H buffers; the shared pair when GL imports them)
struct RgGpu
{
	int W = 0, H = 0;
	bool shared = false, released = false;
	VkDeviceSize gbufAlloc = 0, outAlloc = 0;
	Buf params, gbuf, prevGbuf, cur, hist, histPix, est, acc, accPrev, out, cand, stbn, atrA, atrB, gpk;
	Buf estRb, candRb, outRb;
	bool stbnUp = false;                  // the mask is in stbn (uploaded once a working set)
	VkQueryPool qp = VK_NULL_HANDLE;
	VkDescriptorSet set = VK_NULL_HANDLE;
	VkCommandBuffer cb = VK_NULL_HANDLE;
	VkFence fence = VK_NULL_HANDLE;
	VkSemaphore vkDone = VK_NULL_HANDLE, glDone = VK_NULL_HANDLE;
	uint64_t sceneKey = 0;
	bool keyed = false;
	uint32_t cuts = 0, relights = 0;
};

/*! a cut: between the previous view and this one the camera jumped more than 256 units (3.6 m in a frame: no walk or
 *  vehicle, a teleport or a shot change), turned more than 30 degrees, or the lens changed by more than 5 %. The
 *  reprojection would still find "like" pixels by chance on a cut; this drops the whole history instead. */
static bool rgCut( const RestirGiGpuIn & in )
{
	const float dx = in.rows[3] - in.prows[3], dy = in.rows[7] - in.prows[7], dz = in.rows[11] - in.prows[11];
	if ( dx * dx + dy * dy + dz * dz > 256.0f * 256.0f )
		return true;
	const float fw = in.rows[2] * in.prows[2] + in.rows[6] * in.prows[6] + in.rows[10] * in.prows[10];   // the view axes' cosine
	if ( !( fw > 0.866f ) )
		return true;
	for ( int k = 0; k < 2; k++ )
		if ( !( std::fabs( in.proj[k] - in.pproj[k] ) <= 0.05f * std::fabs( in.pproj[k] ) ) )
			return true;
	return false;
}

std::map<const DdgiGpuSession *, RgGpu *> & rgStates()
{
	static std::map<const DdgiGpuSession *, RgGpu *> m;
	return m;
}

void rgFreeBuffers( Dev & D, RgGpu * g )
{
	if ( g->set )
		vk.vkFreeDescriptorSets( D.dev, D.dpool, 1, &g->set );
	g->set = VK_NULL_HANDLE;
	DdgiExtra & X = ddgiExtra( D );
	for ( VkSemaphore * sem : { &g->vkDone, &g->glDone } ) {
		if ( *sem && X.destroySem )
			X.destroySem( D.dev, *sem, nullptr );
		*sem = VK_NULL_HANDLE;
	}
	for ( Buf * b : { &g->params, &g->gbuf, &g->prevGbuf, &g->cur, &g->hist, &g->histPix, &g->est, &g->acc, &g->accPrev, &g->out,
			&g->cand, &g->stbn, &g->atrA, &g->atrB, &g->gpk, &g->estRb, &g->candRb, &g->outRb } )
		freeBuf( D, *b );
	DdgiTimer & T = ddgiTimer( D );
	if ( g->qp && T.destroy )
		T.destroy( D.dev, g->qp, nullptr );
	g->qp = VK_NULL_HANDLE;
	g->stbnUp = false;
	g->W = g->H = 0;
	g->shared = g->released = false;
}

// called by ddgiFree (the session's TLAS goes with it), under the device's lock
void restirGiFreeFor( Dev & D, DdgiGpuSession * s )
{
	auto & m = rgStates();
	auto it = m.find( s );
	if ( it == m.end() )
		return;
	RgGpu * g = it->second;
	DdgiExtra & X = ddgiExtra( D );
	if ( g->fence )
		vk.vkWaitForFences( D.dev, 1, &g->fence, VK_TRUE, UINT64_MAX );
	rgFreeBuffers( D, g );
	if ( g->cb && X.freeCbs )
		X.freeCbs( D.dev, D.pool, 1, &g->cb );
	if ( g->fence && X.destroyFence )
		X.destroyFence( D.dev, g->fence, nullptr );
	delete g;
	m.erase( it );
}

RgGpu * rgStateFor( Dev & D, DdgiGpuSession * s )
{
	auto & m = rgStates();
	auto it = m.find( s );
	if ( it != m.end() )
		return it->second;
	RgGpu * g = new RgGpu;
	VkCommandBufferAllocateInfo cai { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
	cai.commandPool = D.pool;
	cai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	cai.commandBufferCount = 1;
	VkFenceCreateInfo fi { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
	fi.flags = VK_FENCE_CREATE_SIGNALED_BIT;   // born signalled: every step and free waits on it first
	if ( vk.vkAllocateCommandBuffers( D.dev, &cai, &g->cb ) != VK_SUCCESS || vk.vkCreateFence( D.dev, &fi, nullptr, &g->fence ) != VK_SUCCESS ) {
		delete g;
		return nullptr;
	}
	m[s] = g;
	return g;
}

//! the W x H working set; `share` makes the g-buffer and the output exportable (false + why when the device refuses)
bool rgMake( Dev & D, RgGpu * g, RgPipe & R, int W, int H, bool share, std::string * why )
{
	rgFreeBuffers( D, g );
	const VkDeviceSize n = VkDeviceSize( W ) * VkDeviceSize( H );
	const VkBufferUsageFlags u = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	DdgiExtra & X = ddgiExtra( D );
	bool ok = true;
	if ( share ) {
		if ( !D.share || !X.getMemWin32 || !X.getSemWin32 || !X.createSem ) {
			*why = "the bake device has no Win32 external memory / semaphore export";
			return false;
		}
		ok = ddgiMakeShared( D, n * 16, u, g->gbuf, &g->gbufAlloc, why ) && ddgiMakeShared( D, n * 16, u, g->out, &g->outAlloc, why );
		g->vkDone = ok ? ddgiMakeSemaphore( D, X ) : VK_NULL_HANDLE;
		g->glDone = ok ? ddgiMakeSemaphore( D, X ) : VK_NULL_HANDLE;
		if ( ok && ( !g->vkDone || !g->glDone ) ) {
			*why = "the device refused an exportable semaphore";
			ok = false;
		}
		if ( !ok ) {
			rgFreeBuffers( D, g );
			return false;
		}
		g->shared = true;
	} else {
		ok = makeBuf( D, n * 16, u, { kDevLocal, kHost }, g->gbuf ) && makeBuf( D, n * 16, u, { kDevLocal, kHost }, g->out );
	}
	ok = ok && makeBuf( D, kRgParamBytes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, { kHost }, g->params )
		&& makeBuf( D, n * 16, u, { kDevLocal, kHost }, g->prevGbuf ) && makeBuf( D, n * kRgResBytes, u, { kDevLocal, kHost }, g->cur )
		&& makeBuf( D, n * kRgResBytes, u, { kDevLocal, kHost }, g->hist ) && makeBuf( D, n * 4, u, { kDevLocal, kHost }, g->histPix )
		&& makeBuf( D, n * 16, u, { kDevLocal, kHost }, g->est ) && makeBuf( D, n * 16, u, { kDevLocal, kHost }, g->acc )
		&& makeBuf( D, n * 16, u, { kDevLocal, kHost }, g->accPrev ) && makeBuf( D, n * 16, u, { kDevLocal, kHost }, g->cand )
		&& makeBuf( D, kRgStbnBytes, u, { kDevLocal, kHost }, g->stbn ) && makeBuf( D, n * 16, u, { kDevLocal, kHost }, g->atrA )
		&& makeBuf( D, n * 16, u, { kDevLocal, kHost }, g->atrB ) && makeBuf( D, n * 8, u, { kDevLocal, kHost }, g->gpk )
		&& g->params.map;
	DdgiTimer & T = ddgiTimer( D );
	if ( ok && T.ok ) {
		VkQueryPoolCreateInfo qi { VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO };
		qi.queryType = VK_QUERY_TYPE_TIMESTAMP;
		qi.queryCount = kRgStamps;
		if ( T.create( D.dev, &qi, nullptr, &g->qp ) != VK_SUCCESS )
			g->qp = VK_NULL_HANDLE;   // no timer: msGpu stays -1
	}
	if ( ok ) {
		VkDescriptorSetAllocateInfo ai { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
		ai.descriptorPool = D.dpool;
		ai.descriptorSetCount = 1;
		ai.pSetLayouts = &R.dsl;
		ok = vk.vkAllocateDescriptorSets( D.dev, &ai, &g->set ) == VK_SUCCESS;
	}
	if ( ok ) {
		const Buf * bs[kRgBindings] = { &g->params, &g->gbuf, &g->prevGbuf, &g->cur, &g->hist, &g->histPix, &g->est, &g->acc,
			&g->accPrev, &g->out, &g->cand, &g->stbn, &g->atrA, &g->atrB, &g->gpk };
		VkDescriptorBufferInfo bi[kRgBindings] {};
		VkWriteDescriptorSet w[kRgBindings] {};
		for ( int i = 0; i < kRgBindings; i++ ) {
			bi[i] = { bs[i]->b, 0, VK_WHOLE_SIZE };
			w[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			w[i].dstSet = g->set;
			w[i].dstBinding = uint32_t( i );
			w[i].descriptorCount = 1;
			w[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			w[i].pBufferInfo = &bi[i];
		}
		vk.vkUpdateDescriptorSets( D.dev, kRgBindings, w, 0, nullptr );
	}
	if ( !ok ) {
		*why = "the ReSTIR GI buffers did not allocate (out of GPU memory?)";
		rgFreeBuffers( D, g );
		return false;
	}
	g->W = W;
	g->H = H;
	return true;
}

void rgDispatch( Dev & D, RgPipe & R, DdgiGpuSession * s, RgGpu * g, uint32_t mode, uint64_t groups )
{
	vk.vkCmdBindPipeline( D.cb, VK_PIPELINE_BIND_POINT_COMPUTE, mode >= 2 && R.pipeF ? R.pipeF : R.pipe );
	const VkDescriptorSet sets[2] = { s->set, g->set };
	vk.vkCmdBindDescriptorSets( D.cb, VK_PIPELINE_BIND_POINT_COMPUTE, R.pl, 0, 2, sets, 0, nullptr );
	const DdgiPc pc = ddgiPc( nullptr, mode, 0, 0, 0 );
	vk.vkCmdPushConstants( D.cb, R.pl, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof pc, &pc );
	const uint64_t gx = std::min<uint64_t>( groups, 65535 );
	const uint64_t gy = ( groups + gx - 1 ) / gx;
	vk.vkCmdDispatch( D.cb, uint32_t( gx ), uint32_t( gy ), 1 );
}

} // namespace

bool restirGiGpuShare( DdgiGpuSession * s, int W, int H, RestirGpuShare * out )
{
	*out = RestirGpuShare();
	if ( !s || W <= 0 || H <= 0 ) {
		out->why = "no session or an empty target";
		return false;
	}
#ifdef _WIN32
	Dev & D = dev();
	std::lock_guard<std::mutex> lk( D.m );
	DdgiPipes & S = ddgiPipes( D );
	RgPipe & R = rgPipe( D, S );
	if ( !S.ok || !R.ok ) {
		out->why = S.ok ? R.why : S.why;
		return false;
	}
	RgGpu * g = rgStateFor( D, s );
	if ( !g ) {
		out->why = "no command buffer for ReSTIR GI";
		return false;
	}
	if ( g->fence )
		vk.vkWaitForFences( D.dev, 1, &g->fence, VK_TRUE, UINT64_MAX );
	if ( !rgMake( D, g, R, W, H, true, &out->why ) )
		return false;
	DdgiExtra & X = ddgiExtra( D );
	const auto getMem = reinterpret_cast<DdgiGetMemoryWin32>( X.getMemWin32 );
	const auto getSem = reinterpret_cast<DdgiGetSemaphoreWin32>( X.getSemWin32 );
	const DdgiMemoryWin32Get m1 { kMemoryGetWin32, nullptr, g->gbuf.m, VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT };
	const DdgiMemoryWin32Get m2 { kMemoryGetWin32, nullptr, g->out.m, VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT };
	const DdgiSemaphoreWin32Get s1 { kSemaphoreGetWin32, nullptr, g->vkDone, VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_WIN32_BIT };
	const DdgiSemaphoreWin32Get s2 { kSemaphoreGetWin32, nullptr, g->glDone, VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_WIN32_BIT };
	HANDLE h[4] = { nullptr, nullptr, nullptr, nullptr };
	if ( getMem( D.dev, &m1, &h[0] ) != VK_SUCCESS || getMem( D.dev, &m2, &h[1] ) != VK_SUCCESS || getSem( D.dev, &s1, &h[2] ) != VK_SUCCESS
		|| getSem( D.dev, &s2, &h[3] ) != VK_SUCCESS ) {
		for ( HANDLE x : h )
			if ( x )
				CloseHandle( x );
		rgFreeBuffers( D, g );
		out->why = "the device gave no Win32 handle for a ReSTIR GI buffer or semaphore";
		return false;
	}
	out->gbufMemory = h[0];
	out->outMemory = h[1];
	out->vkDone = h[2];
	out->glDone = h[3];
	out->gbufSize = g->gbufAlloc;
	out->outSize = g->outAlloc;
	out->ok = true;
	return true;
#else
	out->why = "GL interop is written for Win32 handles only";
	return false;
#endif
}

bool restirGiGpuStep( DdgiGpuSession * s, const RestirGiGpuIn & in, RestirGiGpuOut * out )
{
	*out = RestirGiGpuOut();
	if ( !s || in.W <= 0 || in.H <= 0 ) {
		out->why = "no session or an empty target";
		return false;
	}
	Dev & D = dev();
	std::unique_lock<std::mutex> lk( D.m, std::try_to_lock );
	if ( !lk.owns_lock() ) {
		out->busy = true;
		out->why = "the bake device is busy";
		return false;
	}
	const auto t0 = std::chrono::steady_clock::now();
	DdgiPipes & S = ddgiPipes( D );
	RgPipe & R = rgPipe( D, S );
	if ( !S.ok || !R.ok ) {
		out->why = S.ok ? R.why : S.why;
		return false;
	}
	out->kernel = R.kernel;
	RgGpu * g = rgStateFor( D, s );
	if ( !g ) {
		out->why = "no command buffer for ReSTIR GI";
		return false;
	}
	vk.vkWaitForFences( D.dev, 1, &g->fence, VK_TRUE, UINT64_MAX );
	// strict history: a cut, a light change or new buffers start every pixel over (disocclusion is mode 0's, a pixel each)
	bool history = in.history;
	if ( history && rgCut( in ) ) {
		history = false;
		g->cuts++;
	}
	if ( history && g->keyed && g->sceneKey != in.sceneKey ) {
		history = false;
		g->relights++;
	}
	g->sceneKey = in.sceneKey;
	g->keyed = true;
	out->cuts = g->cuts;
	out->relights = g->relights;
	if ( g->W != in.W || g->H != in.H || ( g->shared && in.gbuf ) ) {
		history = false;
		if ( !in.gbuf && g->shared ) {
			out->why = "the shared g-buffer is for another size (restirGiGpuShare first)";
			return false;
		}
		if ( !rgMake( D, g, R, in.W, in.H, false, &out->why ) )
			return false;
	}
	const VkDeviceSize n = VkDeviceSize( in.W ) * VkDeviceSize( in.H );
	{
		float p[44] = {};
		int32_t * pi = reinterpret_cast<int32_t *>( p );
		pi[0] = in.W;
		pi[1] = in.H;
		pi[2] = int32_t( in.frame );
		pi[3] = int32_t( in.seed );
		pi[4] = in.spatial;
		pi[5] = in.radius;
		pi[6] = in.filter;
		pi[7] = in.red;
		p[8] = history ? 1.0f : 0.0f;
		p[9] = in.accFrames;
		p[10] = in.stbn ? 1.0f : 0.0f;
		p[11] = float( std::clamp( in.atrous, 0, 5 ) );
		std::memcpy( p + 12, in.rows, sizeof in.rows );
		std::memcpy( p + 24, in.proj, sizeof in.proj );
		std::memcpy( p + 28, in.prows, sizeof in.prows );
		std::memcpy( p + 40, in.pproj, sizeof in.pproj );
		static_assert( sizeof p == kRgParamBytes, "RestirGiParams" );
		std::memcpy( g->params.map, p, sizeof p );
	}
	if ( in.gbuf && !ddgiPut( D, in.gbuf, size_t( n ) * 16, g->gbuf ) ) {
		out->why = "the g-buffer upload failed";
		return false;
	}
	if ( in.stbn && !g->stbnUp ) {
		if ( !ddgiPut( D, in.stbn, size_t( kRgStbnBytes ), g->stbn ) ) {
			out->why = "the STBN mask upload failed";
			return false;
		}
		g->stbnUp = true;
	}
	if ( in.readBack && !g->estRb.b ) {
		const VkMemoryPropertyFlags cached = kHost | VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
		const VkBufferUsageFlags rb = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
		if ( !makeBuf( D, n * 16, rb, { cached, kHost }, g->estRb ) || !makeBuf( D, n * 16, rb, { cached, kHost }, g->candRb )
			|| !makeBuf( D, n * 16, rb, { cached, kHost }, g->outRb ) ) {
			out->why = "the read-back buffers did not allocate";
			return false;
		}
	}
	const bool gl = g->shared && bool( in.beforeSubmit ) && !in.gbuf;
	const uint64_t groups = uint64_t( ( in.W + 7 ) / 8 ) * uint64_t( ( in.H + 7 ) / 8 );   // 8 x 8 tiles (>= n / 64: the linear red fits too)
	// the half / quarter trace's modes 0 and 1: 8 x 8 tiles of 2 x 1 / 2 x 2 blocks (restirgi.comp main; not with red linear)
	const int tb = in.red & ( 512 | 1024 );
	const int bw = ( in.W + 1 ) / 2, bh = ( tb & 1024 ) ? ( in.H + 1 ) / 2 : in.H;
	const uint64_t groupsTrace = tb && !( in.red & 256 ) ? uint64_t( ( bw + 7 ) / 8 ) * uint64_t( ( bh + 7 ) / 8 ) : groups;
	bool ok = true;
	{
		DdgiCbSwap sw( D, g->cb );
		ok = begin( D );
		if ( ok ) {
			if ( gl && g->released ) {
				ddgiOwner( D, g->gbuf.b, true );
				ddgiOwner( D, g->out.b, true );
			}
			DdgiTimer & T = ddgiTimer( D );
			if ( g->qp )
				T.reset( D.cb, g->qp, 0, kRgStamps );
			const uint32_t modes = 4 + uint32_t( std::clamp( in.atrous, 0, 5 ) );
			for ( uint32_t mode = 0; mode < modes; mode++ ) {
				if ( g->qp )
					T.write( D.cb, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, g->qp, mode );
				rgDispatch( D, R, s, g, mode, mode <= 1 ? groupsTrace : groups );
				ddgiBarrier( D );
			}
			if ( g->qp )
				T.write( D.cb, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, g->qp, modes );
			ddgiCopyBarrier( D );
			ddgiCopy( D, g->cur, g->hist, n * kRgResBytes );   // the temporal reservoirs: the next frame's history
			ddgiCopy( D, g->acc, g->accPrev, n * 16 );          // the running means
			ddgiCopy( D, g->gbuf, g->prevGbuf, n * 16 );        // and the g-buffer they are for
			if ( in.readBack ) {
				ddgiCopy( D, g->est, g->estRb, n * 16 );
				ddgiCopy( D, g->cand, g->candRb, n * 16 );
				ddgiCopy( D, g->out, g->outRb, n * 16 );
			}
			ddgiEndBarrier( D );
			if ( gl ) {
				ddgiOwner( D, g->gbuf.b, false );
				ddgiOwner( D, g->out.b, false );
			}
			ok = vk.vkEndCommandBuffer( D.cb ) == VK_SUCCESS;
		}
		if ( ok ) {
			const bool waitGl = gl ? in.beforeSubmit() : false;
			const VkPipelineStageFlags ws = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT;
			VkSubmitInfo si { VK_STRUCTURE_TYPE_SUBMIT_INFO };
			si.waitSemaphoreCount = waitGl ? 1 : 0;
			si.pWaitSemaphores = &g->glDone;
			si.pWaitDstStageMask = &ws;
			si.commandBufferCount = 1;
			si.pCommandBuffers = &g->cb;
			si.signalSemaphoreCount = gl ? 1 : 0;
			si.pSignalSemaphores = &g->vkDone;
			vk.vkResetFences( D.dev, 1, &g->fence );
			ok = vk.vkQueueSubmit( D.q, 1, &si, g->fence ) == VK_SUCCESS;
		}
	}
	if ( ok )
		ok = vk.vkWaitForFences( D.dev, 1, &g->fence, VK_TRUE, UINT64_MAX ) == VK_SUCCESS;
	if ( !ok ) {
		out->why = "a ReSTIR GI submit failed (lost device or out of memory)";
		return false;
	}
	g->released = gl;
	if ( g->qp ) {
		DdgiTimer & T = ddgiTimer( D );
		const uint32_t used = 5 + uint32_t( std::clamp( in.atrous, 0, 5 ) );   // the stamps written: modes + the end
		uint64_t ts[kRgStamps] = {};
		if ( T.get( D.dev, g->qp, 0, used, sizeof ts, ts, 8, VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT ) == VK_SUCCESS ) {
			const double k = T.nsPerTick * 1e-6;
			out->msGpu = double( ts[used - 1] - ts[0] ) * k;
			out->msDenoise = double( ts[used - 1] - ts[3] ) * k;
			for ( uint32_t m = 0; m + 1 < used; m++ )
				out->msStage[m] = double( ts[m + 1] - ts[m] ) * k;
		}
	}
	if ( in.readBack ) {
		const float * e = static_cast<const float *>( g->estRb.map );
		const float * c = static_cast<const float *>( g->candRb.map );
		const float * o = static_cast<const float *>( g->outRb.map );
		out->est.assign( e, e + size_t( n ) * 4 );
		out->cand.assign( c, c + size_t( n ) * 4 );
		out->out.assign( o, o + size_t( n ) * 4 );
	}
	out->msWall = std::chrono::duration<double, std::milli>( std::chrono::steady_clock::now() - t0 ).count();
	out->ok = true;
	return true;
}

#endif // RESTIRGI_HAVE_SPIRV
