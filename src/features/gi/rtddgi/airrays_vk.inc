// MODULES1 RESUME 15 item 2 (2026-11-06): the lamps' shadow in the air by rays on the DDGI volume's device and BVH
// (src/features/gi/rtddgi/rtddgi_gpu.h airraysGpu*; route b of notes RESEARCH_AIR.md). Included after restir_vk.inc, inside
// namespace bakert with the session, the pipelines and the helpers above. Kernel: airrays.comp (rtddgi_spirv.sh ->
// airrays.spv.h). Set 0 = the volume's own descriptor set; set 1 = params, acc, accPrev, out. No g-buffer: the froxel grid
// is the camera's, so only the view and the slice law go over each frame.

#if defined( __has_include )
#if __has_include( "airrays.spv.h" )
#define AIRRAYS_HAVE_SPIRV 1
#endif
#endif

#ifndef AIRRAYS_HAVE_SPIRV

namespace {
void airraysFreeFor( Dev &, DdgiGpuSession * ) {}
}
bool airraysGpuStep( DdgiGpuSession *, const AirRaysGpuIn &, AirRaysGpuOut * out )
{
	*out = AirRaysGpuOut();
	out->why = "the air rays kernel is not compiled into this build (run src/features/gi/rtddgi_spirv.sh, then rebuild)";
	return false;
}
bool airraysGpuShare( DdgiGpuSession *, int, int, int, AirRaysGpuShare * out )
{
	*out = AirRaysGpuShare();
	out->why = "the air rays kernel is not compiled into this build";
	return false;
}

#else

namespace {
#include "features/gi/rtddgi/airrays.spv.h"

constexpr int kAirRaysBindings = 4;   // airrays.comp set 1
constexpr VkDeviceSize kAirRaysParamBytes = 11 * 16;   // AirParams, std430: size, opts, slice, rows[3], proj, prows[3], pproj

struct AirRaysPipe
{
	bool tried = false, ok = false;
	std::string why;
	VkDescriptorSetLayout dsl = VK_NULL_HANDLE;
	VkPipelineLayout pl = VK_NULL_HANDLE;
	VkPipeline pipe = VK_NULL_HANDLE;
};

AirRaysPipe & airraysPipe( Dev & D, DdgiPipes & S )
{
	static AirRaysPipe R;
	if ( R.tried )
		return R;
	R.tried = true;
	std::vector<VkDescriptorSetLayoutBinding> b;
	for ( int i = 0; i < kAirRaysBindings; i++ ) {
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
		R.why = "no air rays descriptor set layout";
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
		R.why = "no air rays pipeline layout";
		return R;
	}
	R.pipe = pipeline( D, rtddgi_airrays_spv, sizeof rtddgi_airrays_spv, R.pl );
	R.ok = R.pipe != VK_NULL_HANDLE;
	if ( !R.ok )
		R.why = "the air rays kernel did not build";
	return R;
}

//! one session's air rays state (X x Y x Z froxels; the ratio shared when GL imports it)
struct AirRaysGpu
{
	int X = 0, Y = 0, Z = 0;
	bool shared = false, released = false;
	VkDeviceSize outAlloc = 0;
	Buf params, acc, accPrev, out;
	Buf outRb, accRb;
	VkDescriptorSet set = VK_NULL_HANDLE;
	VkCommandBuffer cb = VK_NULL_HANDLE;
	VkFence fence = VK_NULL_HANDLE;
	VkSemaphore vkDone = VK_NULL_HANDLE, glDone = VK_NULL_HANDLE;
};

std::map<const DdgiGpuSession *, AirRaysGpu *> & airraysStates()
{
	static std::map<const DdgiGpuSession *, AirRaysGpu *> m;
	return m;
}

void airraysFreeBuffers( Dev & D, AirRaysGpu * g )
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
	for ( Buf * b : { &g->params, &g->acc, &g->accPrev, &g->out, &g->outRb, &g->accRb } )
		freeBuf( D, *b );
	g->X = g->Y = g->Z = 0;
	g->shared = g->released = false;
}

// called by ddgiFree (the session's TLAS goes with it), under the device's lock
void airraysFreeFor( Dev & D, DdgiGpuSession * s )
{
	auto & m = airraysStates();
	auto it = m.find( s );
	if ( it == m.end() )
		return;
	AirRaysGpu * g = it->second;
	DdgiExtra & X = ddgiExtra( D );
	if ( g->fence )
		vk.vkWaitForFences( D.dev, 1, &g->fence, VK_TRUE, UINT64_MAX );
	airraysFreeBuffers( D, g );
	if ( g->cb && X.freeCbs )
		X.freeCbs( D.dev, D.pool, 1, &g->cb );
	if ( g->fence && X.destroyFence )
		X.destroyFence( D.dev, g->fence, nullptr );
	delete g;
	m.erase( it );
}

AirRaysGpu * airraysStateFor( Dev & D, DdgiGpuSession * s )
{
	auto & m = airraysStates();
	auto it = m.find( s );
	if ( it != m.end() )
		return it->second;
	AirRaysGpu * g = new AirRaysGpu;
	VkCommandBufferAllocateInfo cai { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
	cai.commandPool = D.pool;
	cai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	cai.commandBufferCount = 1;
	VkFenceCreateInfo fi { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
	fi.flags = VK_FENCE_CREATE_SIGNALED_BIT;   // every step and free waits on it first: born signalled (skill ww-rtddgi-volume)
	if ( vk.vkAllocateCommandBuffers( D.dev, &cai, &g->cb ) != VK_SUCCESS || vk.vkCreateFence( D.dev, &fi, nullptr, &g->fence ) != VK_SUCCESS ) {
		delete g;
		return nullptr;
	}
	m[s] = g;
	return g;
}

//! the X x Y x Z working set; `share` makes the ratio exportable (false + why when the device refuses)
bool airraysMake( Dev & D, AirRaysGpu * g, AirRaysPipe & R, int X, int Y, int Z, bool share, std::string * why )
{
	airraysFreeBuffers( D, g );
	const VkDeviceSize n = VkDeviceSize( X ) * VkDeviceSize( Y ) * VkDeviceSize( Z );
	const VkBufferUsageFlags u = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	DdgiExtra & E = ddgiExtra( D );
	bool ok = true;
	if ( share ) {
		if ( !D.share || !E.getMemWin32 || !E.getSemWin32 || !E.createSem ) {
			*why = "the bake device has no Win32 external memory / semaphore export";
			return false;
		}
		ok = ddgiMakeShared( D, n * 8, u, g->out, &g->outAlloc, why );
		g->vkDone = ok ? ddgiMakeSemaphore( D, E ) : VK_NULL_HANDLE;
		g->glDone = ok ? ddgiMakeSemaphore( D, E ) : VK_NULL_HANDLE;
		if ( ok && ( !g->vkDone || !g->glDone ) ) {
			*why = "the device refused an exportable semaphore";
			ok = false;
		}
		if ( !ok ) {
			airraysFreeBuffers( D, g );
			return false;
		}
		g->shared = true;
	} else {
		ok = makeBuf( D, n * 8, u, { kDevLocal, kHost }, g->out );
	}
	ok = ok && makeBuf( D, kAirRaysParamBytes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, { kHost }, g->params )
		&& makeBuf( D, n * 16, u, { kDevLocal, kHost }, g->acc ) && makeBuf( D, n * 16, u, { kDevLocal, kHost }, g->accPrev )
		&& g->params.map;
	if ( ok ) {
		VkDescriptorSetAllocateInfo ai { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
		ai.descriptorPool = D.dpool;
		ai.descriptorSetCount = 1;
		ai.pSetLayouts = &R.dsl;
		ok = vk.vkAllocateDescriptorSets( D.dev, &ai, &g->set ) == VK_SUCCESS;
	}
	if ( ok ) {
		const Buf * bs[kAirRaysBindings] = { &g->params, &g->acc, &g->accPrev, &g->out };
		VkDescriptorBufferInfo bi[kAirRaysBindings] {};
		VkWriteDescriptorSet w[kAirRaysBindings] {};
		for ( int i = 0; i < kAirRaysBindings; i++ ) {
			bi[i] = { bs[i]->b, 0, VK_WHOLE_SIZE };
			w[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			w[i].dstSet = g->set;
			w[i].dstBinding = uint32_t( i );
			w[i].descriptorCount = 1;
			w[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			w[i].pBufferInfo = &bi[i];
		}
		vk.vkUpdateDescriptorSets( D.dev, kAirRaysBindings, w, 0, nullptr );
	}
	if ( !ok ) {
		*why = "the air rays buffers did not allocate (out of GPU memory?)";
		airraysFreeBuffers( D, g );
		return false;
	}
	g->X = X;
	g->Y = Y;
	g->Z = Z;
	return true;
}

} // namespace

bool airraysGpuShare( DdgiGpuSession * s, int X, int Y, int Z, AirRaysGpuShare * out )
{
	*out = AirRaysGpuShare();
	if ( !s || X <= 0 || Y <= 0 || Z <= 0 ) {
		out->why = "no session or an empty grid";
		return false;
	}
#ifdef _WIN32
	Dev & D = dev();
	std::lock_guard<std::mutex> lk( D.m );
	DdgiPipes & S = ddgiPipes( D );
	AirRaysPipe & R = airraysPipe( D, S );
	if ( !S.ok || !R.ok ) {
		out->why = S.ok ? R.why : S.why;
		return false;
	}
	AirRaysGpu * g = airraysStateFor( D, s );
	if ( !g ) {
		out->why = "no command buffer for the air rays";
		return false;
	}
	vk.vkWaitForFences( D.dev, 1, &g->fence, VK_TRUE, UINT64_MAX );
	if ( !airraysMake( D, g, R, X, Y, Z, true, &out->why ) )
		return false;
	DdgiExtra & E = ddgiExtra( D );
	const auto getMem = reinterpret_cast<DdgiGetMemoryWin32>( E.getMemWin32 );
	const auto getSem = reinterpret_cast<DdgiGetSemaphoreWin32>( E.getSemWin32 );
	const DdgiMemoryWin32Get m1 { kMemoryGetWin32, nullptr, g->out.m, VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT };
	const DdgiSemaphoreWin32Get s1 { kSemaphoreGetWin32, nullptr, g->vkDone, VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_WIN32_BIT };
	const DdgiSemaphoreWin32Get s2 { kSemaphoreGetWin32, nullptr, g->glDone, VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_WIN32_BIT };
	HANDLE h[3] = { nullptr, nullptr, nullptr };
	if ( getMem( D.dev, &m1, &h[0] ) != VK_SUCCESS || getSem( D.dev, &s1, &h[1] ) != VK_SUCCESS || getSem( D.dev, &s2, &h[2] ) != VK_SUCCESS ) {
		for ( HANDLE x : h )
			if ( x )
				CloseHandle( x );
		airraysFreeBuffers( D, g );
		out->why = "the device gave no Win32 handle for the air rays buffer or a semaphore";
		return false;
	}
	out->outMemory = h[0];
	out->vkDone = h[1];
	out->glDone = h[2];
	out->outSize = g->outAlloc;
	out->ok = true;
	return true;
#else
	out->why = "GL interop is written for Win32 handles only";
	return false;
#endif
}

bool airraysGpuStep( DdgiGpuSession * s, const AirRaysGpuIn & in, AirRaysGpuOut * out )
{
	*out = AirRaysGpuOut();
	if ( !s || in.X <= 0 || in.Y <= 0 || in.Z <= 0 ) {
		out->why = "no session or an empty grid";
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
	AirRaysPipe & R = airraysPipe( D, S );
	if ( !S.ok || !R.ok ) {
		out->why = S.ok ? R.why : S.why;
		return false;
	}
	AirRaysGpu * g = airraysStateFor( D, s );
	if ( !g ) {
		out->why = "no command buffer for the air rays";
		return false;
	}
	vk.vkWaitForFences( D.dev, 1, &g->fence, VK_TRUE, UINT64_MAX );   // our own last submit (a step always waits: never pending)
	if ( g->X != in.X || g->Y != in.Y || g->Z != in.Z ) {
		if ( g->shared && in.beforeSubmit ) {
			out->why = "the shared ratio is for another grid (airraysGpuShare first)";
			return false;
		}
		if ( !airraysMake( D, g, R, in.X, in.Y, in.Z, false, &out->why ) )
			return false;
	}
	const VkDeviceSize n = VkDeviceSize( in.X ) * VkDeviceSize( in.Y ) * VkDeviceSize( in.Z );
	{
		float p[44] = {};
		int32_t * pi = reinterpret_cast<int32_t *>( p );
		pi[0] = in.X;
		pi[1] = in.Y;
		pi[2] = in.Z;
		pi[3] = int32_t( in.frame );
		pi[4] = in.samples;
		pi[5] = in.frames;
		pi[6] = in.red;
		p[8] = in.nearZ;
		p[9] = in.logRange;
		p[10] = in.bias;
		p[11] = in.history ? 1.0f : 0.0f;
		std::memcpy( p + 12, in.rows, sizeof in.rows );
		std::memcpy( p + 24, in.proj, sizeof in.proj );
		std::memcpy( p + 28, in.prows, sizeof in.prows );
		std::memcpy( p + 40, in.pproj, sizeof in.pproj );
		static_assert( sizeof p == kAirRaysParamBytes, "AirParams" );
		std::memcpy( g->params.map, p, sizeof p );
	}
	if ( in.readBack && !g->outRb.b ) {
		const VkMemoryPropertyFlags cached = kHost | VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
		const VkBufferUsageFlags rb = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
		if ( !makeBuf( D, n * 8, rb, { cached, kHost }, g->outRb ) || !makeBuf( D, n * 16, rb, { cached, kHost }, g->accRb ) ) {
			out->why = "the read-back buffers did not allocate";
			return false;
		}
	}
	const bool gl = g->shared && bool( in.beforeSubmit );
	const uint64_t groups = ( uint64_t( n ) + 63 ) / 64;
	bool ok = true;
	{
		DdgiCbSwap sw( D, g->cb );
		ok = begin( D );
		if ( ok ) {
			if ( gl && g->released )
				ddgiOwner( D, g->out.b, true );
			vk.vkCmdBindPipeline( D.cb, VK_PIPELINE_BIND_POINT_COMPUTE, R.pipe );
			const VkDescriptorSet sets[2] = { s->set, g->set };
			vk.vkCmdBindDescriptorSets( D.cb, VK_PIPELINE_BIND_POINT_COMPUTE, R.pl, 0, 2, sets, 0, nullptr );
			const DdgiPc pc = ddgiPc( nullptr, 0, 0, 0, 0 );
			vk.vkCmdPushConstants( D.cb, R.pl, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof pc, &pc );
			const uint64_t gx = std::min<uint64_t>( groups, 65535 );
			const uint64_t gy = ( groups + gx - 1 ) / gx;
			vk.vkCmdDispatch( D.cb, uint32_t( gx ), uint32_t( gy ), 1 );
			ddgiCopyBarrier( D );
			ddgiCopy( D, g->acc, g->accPrev, n * 16 );   // this frame's running means: the next one's history
			if ( in.readBack ) {
				ddgiCopy( D, g->out, g->outRb, n * 8 );
				ddgiCopy( D, g->acc, g->accRb, n * 16 );
			}
			ddgiEndBarrier( D );
			if ( gl )
				ddgiOwner( D, g->out.b, false );
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
		out->why = "an air rays submit failed (lost device or out of memory)";
		return false;
	}
	g->released = gl;
	if ( in.readBack ) {
		const float * r = static_cast<const float *>( g->outRb.map );
		const float * a = static_cast<const float *>( g->accRb.map );
		out->ratio.assign( r, r + size_t( n ) * 2 );
		out->acc.assign( a, a + size_t( n ) * 4 );
	}
	out->msWall = std::chrono::duration<double, std::milli>( std::chrono::steady_clock::now() - t0 ).count();
	out->ok = true;
	return true;
}

#endif // AIRRAYS_HAVE_SPIRV
