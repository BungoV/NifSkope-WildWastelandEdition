// Lane RTREFL1 (2026-11-02): ray-traced reflections on the DDGI volume's device and BVH (src/rtddgi/rtddgi_gpu.h
// reflGpu*). Included at the end of rtddgi_vk.inc's kernel branch, after restir_vk.inc (inside namespace bakert, with the
// session, the pipelines and the helpers above). Kernel: refl.comp (tools/rtddgi_spirv.sh -> refl.spv.h; without the
// header every call refuses). Set 0 = the volume's own descriptor set; set 1 = params, the ray target, the output.

#if defined( __has_include )
#if __has_include( "refl.spv.h" )
#define REFL_HAVE_SPIRV 1
#endif
#endif

#ifndef REFL_HAVE_SPIRV

namespace {
void reflFreeFor( Dev &, DdgiGpuSession * ) {}
}
bool reflGpuStep( DdgiGpuSession *, const ReflGpuIn &, ReflGpuOut * out )
{
	*out = ReflGpuOut();
	out->why = "the reflection kernel is not compiled into this build (run tools/rtddgi_spirv.sh, then rebuild)";
	return false;
}
bool reflGpuShare( DdgiGpuSession *, int, int, ReflGpuShare * out )
{
	*out = ReflGpuShare();
	out->why = "the reflection kernel is not compiled into this build";
	return false;
}

#else

namespace {
#include "refl.spv.h"

constexpr int kReflBindings = 3;                      // refl.comp set 1
constexpr VkDeviceSize kReflParamBytes = 7 * 16;      // ReflParams, std430: size, opts, knobs, rows[3], proj

struct ReflPipe
{
	bool tried = false, ok = false;
	std::string why;
	VkDescriptorSetLayout dsl = VK_NULL_HANDLE;
	VkPipelineLayout pl = VK_NULL_HANDLE;
	VkPipeline pipe = VK_NULL_HANDLE;
};

ReflPipe & reflPipe( Dev & D, DdgiPipes & S )
{
	static ReflPipe R;
	if ( R.tried )
		return R;
	R.tried = true;
	std::vector<VkDescriptorSetLayoutBinding> b;
	for ( int i = 0; i < kReflBindings; i++ ) {
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
		R.why = "no reflection descriptor set layout";
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
		R.why = "no reflection pipeline layout";
		return R;
	}
	R.pipe = pipeline( D, rtddgi_refl_spv, sizeof rtddgi_refl_spv, R.pl );
	R.ok = R.pipe != VK_NULL_HANDLE;
	if ( !R.ok )
		R.why = "the reflection kernel did not build";
	return R;
}

//! one session's reflection state (the W x H ray target in, the half-size output; the shared pair when GL imports them)
struct ReflGpu
{
	int W = 0, H = 0;
	bool shared = false, released = false;
	VkDeviceSize raysAlloc = 0, outAlloc = 0;
	Buf params, rays, out, outRb;
	VkDescriptorSet set = VK_NULL_HANDLE;
	VkCommandBuffer cb = VK_NULL_HANDLE;
	VkFence fence = VK_NULL_HANDLE;
	VkSemaphore vkDone = VK_NULL_HANDLE, glDone = VK_NULL_HANDLE;
};

std::map<const DdgiGpuSession *, ReflGpu *> & reflStates()
{
	static std::map<const DdgiGpuSession *, ReflGpu *> m;
	return m;
}

void reflFreeBuffers( Dev & D, ReflGpu * g )
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
	for ( Buf * b : { &g->params, &g->rays, &g->out, &g->outRb } )
		freeBuf( D, *b );
	g->W = g->H = 0;
	g->shared = g->released = false;
}

// called by ddgiFree (the session's TLAS goes with it), under the device's lock
void reflFreeFor( Dev & D, DdgiGpuSession * s )
{
	auto & m = reflStates();
	auto it = m.find( s );
	if ( it == m.end() )
		return;
	ReflGpu * g = it->second;
	DdgiExtra & X = ddgiExtra( D );
	if ( g->fence )
		vk.vkWaitForFences( D.dev, 1, &g->fence, VK_TRUE, UINT64_MAX );
	reflFreeBuffers( D, g );
	if ( g->cb && X.freeCbs )
		X.freeCbs( D.dev, D.pool, 1, &g->cb );
	if ( g->fence && X.destroyFence )
		X.destroyFence( D.dev, g->fence, nullptr );
	delete g;
	m.erase( it );
}

ReflGpu * reflStateFor( Dev & D, DdgiGpuSession * s )
{
	auto & m = reflStates();
	auto it = m.find( s );
	if ( it != m.end() )
		return it->second;
	ReflGpu * g = new ReflGpu;
	VkCommandBufferAllocateInfo cai { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
	cai.commandPool = D.pool;
	cai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	cai.commandBufferCount = 1;
	VkFenceCreateInfo fi { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
	fi.flags = VK_FENCE_CREATE_SIGNALED_BIT;   // every step and free waits on it first: born signalled (RESTIRDI1's trap)
	if ( vk.vkAllocateCommandBuffers( D.dev, &cai, &g->cb ) != VK_SUCCESS || vk.vkCreateFence( D.dev, &fi, nullptr, &g->fence ) != VK_SUCCESS ) {
		delete g;
		return nullptr;
	}
	m[s] = g;
	return g;
}

//! the W x H ray target and its half-size output; `share` makes both exportable (false + why when the device refuses)
bool reflMake( Dev & D, ReflGpu * g, ReflPipe & R, int W, int H, bool share, std::string * why )
{
	reflFreeBuffers( D, g );
	const VkDeviceSize n = VkDeviceSize( W ) * VkDeviceSize( H );
	const VkDeviceSize nh = VkDeviceSize( reflHalf( W ) ) * VkDeviceSize( reflHalf( H ) );
	const VkBufferUsageFlags u = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	DdgiExtra & X = ddgiExtra( D );
	bool ok = true;
	if ( share ) {
		if ( !D.share || !X.getMemWin32 || !X.getSemWin32 || !X.createSem ) {
			*why = "the bake device has no Win32 external memory / semaphore export";
			return false;
		}
		ok = ddgiMakeShared( D, n * 16, u, g->rays, &g->raysAlloc, why ) && ddgiMakeShared( D, nh * 16, u, g->out, &g->outAlloc, why );
		g->vkDone = ok ? ddgiMakeSemaphore( D, X ) : VK_NULL_HANDLE;
		g->glDone = ok ? ddgiMakeSemaphore( D, X ) : VK_NULL_HANDLE;
		if ( ok && ( !g->vkDone || !g->glDone ) ) {
			*why = "the device refused an exportable semaphore";
			ok = false;
		}
		if ( !ok ) {
			reflFreeBuffers( D, g );
			return false;
		}
		g->shared = true;
	} else {
		ok = makeBuf( D, n * 16, u, { kDevLocal, kHost }, g->rays ) && makeBuf( D, nh * 16, u, { kDevLocal, kHost }, g->out );
	}
	ok = ok && makeBuf( D, kReflParamBytes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, { kHost }, g->params ) && g->params.map;
	if ( ok ) {
		VkDescriptorSetAllocateInfo ai { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
		ai.descriptorPool = D.dpool;
		ai.descriptorSetCount = 1;
		ai.pSetLayouts = &R.dsl;
		ok = vk.vkAllocateDescriptorSets( D.dev, &ai, &g->set ) == VK_SUCCESS;
	}
	if ( ok ) {
		const Buf * bs[kReflBindings] = { &g->params, &g->rays, &g->out };
		VkDescriptorBufferInfo bi[kReflBindings] {};
		VkWriteDescriptorSet w[kReflBindings] {};
		for ( int i = 0; i < kReflBindings; i++ ) {
			bi[i] = { bs[i]->b, 0, VK_WHOLE_SIZE };
			w[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			w[i].dstSet = g->set;
			w[i].dstBinding = uint32_t( i );
			w[i].descriptorCount = 1;
			w[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			w[i].pBufferInfo = &bi[i];
		}
		vk.vkUpdateDescriptorSets( D.dev, kReflBindings, w, 0, nullptr );
	}
	if ( !ok ) {
		*why = "the reflection buffers did not allocate (out of GPU memory?)";
		reflFreeBuffers( D, g );
		return false;
	}
	g->W = W;
	g->H = H;
	return true;
}

} // namespace

bool reflGpuShare( DdgiGpuSession * s, int W, int H, ReflGpuShare * out )
{
	*out = ReflGpuShare();
	if ( !s || W <= 0 || H <= 0 ) {
		out->why = "no session or an empty target";
		return false;
	}
#ifdef _WIN32
	Dev & D = dev();
	std::lock_guard<std::mutex> lk( D.m );
	DdgiPipes & S = ddgiPipes( D );
	ReflPipe & R = reflPipe( D, S );
	if ( !S.ok || !R.ok ) {
		out->why = S.ok ? R.why : S.why;
		return false;
	}
	ReflGpu * g = reflStateFor( D, s );
	if ( !g ) {
		out->why = "no command buffer for the reflections";
		return false;
	}
	if ( g->fence )
		vk.vkWaitForFences( D.dev, 1, &g->fence, VK_TRUE, UINT64_MAX );
	if ( !reflMake( D, g, R, W, H, true, &out->why ) )
		return false;
	DdgiExtra & X = ddgiExtra( D );
	const auto getMem = reinterpret_cast<DdgiGetMemoryWin32>( X.getMemWin32 );
	const auto getSem = reinterpret_cast<DdgiGetSemaphoreWin32>( X.getSemWin32 );
	const DdgiMemoryWin32Get m1 { kMemoryGetWin32, nullptr, g->rays.m, VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT };
	const DdgiMemoryWin32Get m2 { kMemoryGetWin32, nullptr, g->out.m, VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT };
	const DdgiSemaphoreWin32Get s1 { kSemaphoreGetWin32, nullptr, g->vkDone, VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_WIN32_BIT };
	const DdgiSemaphoreWin32Get s2 { kSemaphoreGetWin32, nullptr, g->glDone, VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_WIN32_BIT };
	HANDLE h[4] = { nullptr, nullptr, nullptr, nullptr };
	if ( getMem( D.dev, &m1, &h[0] ) != VK_SUCCESS || getMem( D.dev, &m2, &h[1] ) != VK_SUCCESS || getSem( D.dev, &s1, &h[2] ) != VK_SUCCESS
		|| getSem( D.dev, &s2, &h[3] ) != VK_SUCCESS ) {
		for ( HANDLE x : h )
			if ( x )
				CloseHandle( x );
		reflFreeBuffers( D, g );
		out->why = "the device gave no Win32 handle for a reflection buffer or semaphore";
		return false;
	}
	out->raysMemory = h[0];
	out->outMemory = h[1];
	out->vkDone = h[2];
	out->glDone = h[3];
	out->raysSize = g->raysAlloc;
	out->outSize = g->outAlloc;
	out->ok = true;
	return true;
#else
	out->why = "GL interop is written for Win32 handles only";
	return false;
#endif
}

bool reflGpuStep( DdgiGpuSession * s, const ReflGpuIn & in, ReflGpuOut * out )
{
	*out = ReflGpuOut();
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
	ReflPipe & R = reflPipe( D, S );
	if ( !S.ok || !R.ok ) {
		out->why = S.ok ? R.why : S.why;
		return false;
	}
	ReflGpu * g = reflStateFor( D, s );
	if ( !g ) {
		out->why = "no command buffer for the reflections";
		return false;
	}
	vk.vkWaitForFences( D.dev, 1, &g->fence, VK_TRUE, UINT64_MAX );   // our own last submit (a step always waits)
	if ( g->W != in.W || g->H != in.H || ( g->shared && in.rays ) ) {
		if ( !in.rays && g->shared ) {
			out->why = "the shared ray target is for another size (reflGpuShare first)";
			return false;
		}
		if ( !reflMake( D, g, R, in.W, in.H, false, &out->why ) )
			return false;
	}
	const int Wh = reflHalf( in.W ), Hh = reflHalf( in.H );
	const VkDeviceSize n = VkDeviceSize( in.W ) * VkDeviceSize( in.H ), nh = VkDeviceSize( Wh ) * VkDeviceSize( Hh );
	{
		float p[28] = {};
		int32_t * pi = reinterpret_cast<int32_t *>( p );
		pi[0] = in.W;
		pi[1] = in.H;
		pi[2] = Wh;
		pi[3] = Hh;
		pi[4] = in.red;
		p[8] = in.bias;
		p[9] = in.tmax;
		std::memcpy( p + 12, in.rows, sizeof in.rows );
		std::memcpy( p + 24, in.proj, sizeof in.proj );
		static_assert( sizeof p == kReflParamBytes, "ReflParams" );
		std::memcpy( g->params.map, p, sizeof p );
	}
	if ( in.rays && !ddgiPut( D, in.rays, size_t( n ) * 16, g->rays ) ) {
		out->why = "the ray target upload failed";
		return false;
	}
	if ( in.readBack && !g->outRb.b ) {
		const VkMemoryPropertyFlags cached = kHost | VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
		if ( !makeBuf( D, nh * 16, VK_BUFFER_USAGE_TRANSFER_DST_BIT, { cached, kHost }, g->outRb ) ) {
			out->why = "the read-back buffer did not allocate";
			return false;
		}
	}
	const bool gl = g->shared && bool( in.beforeSubmit ) && !in.rays;
	const uint64_t groups = ( uint64_t( nh ) + 63 ) / 64;
	bool ok = true;
	{
		DdgiCbSwap sw( D, g->cb );
		ok = begin( D );
		if ( ok ) {
			if ( gl && g->released ) {
				ddgiOwner( D, g->rays.b, true );
				ddgiOwner( D, g->out.b, true );
			}
			vk.vkCmdBindPipeline( D.cb, VK_PIPELINE_BIND_POINT_COMPUTE, R.pipe );
			const VkDescriptorSet sets[2] = { s->set, g->set };
			vk.vkCmdBindDescriptorSets( D.cb, VK_PIPELINE_BIND_POINT_COMPUTE, R.pl, 0, 2, sets, 0, nullptr );
			const DdgiPc pc = ddgiPc( nullptr, 0, 0, 0, 0 );
			vk.vkCmdPushConstants( D.cb, R.pl, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof pc, &pc );
			const uint64_t gx = std::min<uint64_t>( groups, 65535 );
			const uint64_t gy = ( groups + gx - 1 ) / gx;
			vk.vkCmdDispatch( D.cb, uint32_t( gx ), uint32_t( gy ), 1 );
			ddgiBarrier( D );
			if ( in.readBack ) {
				ddgiCopyBarrier( D );
				ddgiCopy( D, g->out, g->outRb, nh * 16 );
			}
			ddgiEndBarrier( D );
			if ( gl ) {
				ddgiOwner( D, g->rays.b, false );
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
		out->why = "a reflection submit failed (lost device or out of memory)";
		return false;
	}
	g->released = gl;
	if ( in.readBack ) {
		const float * r = static_cast<const float *>( g->outRb.map );
		out->refl.assign( r, r + size_t( nh ) * 4 );
	}
	out->msWall = std::chrono::duration<double, std::milli>( std::chrono::steady_clock::now() - t0 ).count();
	out->ok = true;
	return true;
}

#endif // REFL_HAVE_SPIRV
