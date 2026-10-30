// Lane RESTIRDI1 (2026-10-30): ReSTIR DI on the DDGI volume's device and BVH (src/rtddgi/rtddgi_gpu.h restirGpu*).
// Included at the end of rtddgi_vk.inc's kernel branch (inside namespace bakert, with the session, the pipelines and the
// helpers above). Kernel: restir.comp (tools/rtddgi_spirv.sh -> restir.spv.h; without the header every call refuses).
// Set 0 = the volume's own descriptor set (TLAS, true triangles, alpha masks, packed lights); set 1 = the 11 buffers below.

#if defined( __has_include )
#if __has_include( "restir.spv.h" )
#define RESTIR_HAVE_SPIRV 1
#endif
#endif

#ifndef RESTIR_HAVE_SPIRV

namespace {
void restirFreeFor( Dev &, DdgiGpuSession * ) {}
}
bool restirGpuStep( DdgiGpuSession *, const RestirGpuIn &, RestirGpuOut * out )
{
	*out = RestirGpuOut();
	out->why = "the ReSTIR kernel is not compiled into this build (run tools/rtddgi_spirv.sh, then rebuild)";
	return false;
}
bool restirGpuShare( DdgiGpuSession *, int, int, RestirGpuShare * out )
{
	*out = RestirGpuShare();
	out->why = "the ReSTIR kernel is not compiled into this build";
	return false;
}

#else

namespace {
#include "restir.spv.h"

constexpr int kRestirBindings = 11;   // restir.comp set 1
constexpr VkDeviceSize kRestirParamBytes = 11 * 16;   // RestirParams, std430: size, opts, knobs, rows[3], proj, prows[3], pproj

struct RestirPipe
{
	bool tried = false, ok = false;
	std::string why;
	VkDescriptorSetLayout dsl = VK_NULL_HANDLE;
	VkPipelineLayout pl = VK_NULL_HANDLE;
	VkPipeline pipe = VK_NULL_HANDLE;
};

RestirPipe & restirPipe( Dev & D, DdgiPipes & S )
{
	static RestirPipe R;
	if ( R.tried )
		return R;
	R.tried = true;
	std::vector<VkDescriptorSetLayoutBinding> b;
	for ( int i = 0; i < kRestirBindings; i++ ) {
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
		R.why = "no ReSTIR descriptor set layout";
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
		R.why = "no ReSTIR pipeline layout";
		return R;
	}
	R.pipe = pipeline( D, rtddgi_restir_spv, sizeof rtddgi_restir_spv, R.pl );
	R.ok = R.pipe != VK_NULL_HANDLE;
	if ( !R.ok )
		R.why = "the ReSTIR kernel did not build";
	return R;
}

//! one session's ReSTIR state (W x H buffers; the shared pair when GL imports them)
struct RestirGpu
{
	int W = 0, H = 0;
	bool shared = false, released = false;
	VkDeviceSize gbufAlloc = 0, outAlloc = 0;
	Buf params, gbuf, prevGbuf, cur, hist, histPix, numDen, acc, accPrev, cand, out;
	Buf ratioRb, numDenRb, resRb, accRb;
	size_t candCap = 0;
	VkDescriptorSet set = VK_NULL_HANDLE;
	VkCommandBuffer cb = VK_NULL_HANDLE;
	VkFence fence = VK_NULL_HANDLE;
	VkSemaphore vkDone = VK_NULL_HANDLE, glDone = VK_NULL_HANDLE;
};

std::map<const DdgiGpuSession *, RestirGpu *> & restirStates()
{
	static std::map<const DdgiGpuSession *, RestirGpu *> m;
	return m;
}

void restirFreeBuffers( Dev & D, RestirGpu * g )
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
	for ( Buf * b : { &g->params, &g->gbuf, &g->prevGbuf, &g->cur, &g->hist, &g->histPix, &g->numDen, &g->acc, &g->accPrev, &g->cand,
			&g->out, &g->ratioRb, &g->numDenRb, &g->resRb, &g->accRb } )
		freeBuf( D, *b );
	g->W = g->H = 0;
	g->shared = g->released = false;
	g->candCap = 0;
}

// called by ddgiFree (the session's TLAS goes with it), under the device's lock
void restirFreeFor( Dev & D, DdgiGpuSession * s )
{
	auto & m = restirStates();
	auto it = m.find( s );
	if ( it == m.end() )
		return;
	RestirGpu * g = it->second;
	DdgiExtra & X = ddgiExtra( D );
	if ( g->fence )
		vk.vkWaitForFences( D.dev, 1, &g->fence, VK_TRUE, UINT64_MAX );
	restirFreeBuffers( D, g );
	if ( g->cb && X.freeCbs )
		X.freeCbs( D.dev, D.pool, 1, &g->cb );
	if ( g->fence && X.destroyFence )
		X.destroyFence( D.dev, g->fence, nullptr );
	delete g;
	m.erase( it );
}

RestirGpu * restirStateFor( Dev & D, DdgiGpuSession * s )
{
	auto & m = restirStates();
	auto it = m.find( s );
	if ( it != m.end() )
		return it->second;
	RestirGpu * g = new RestirGpu;
	VkCommandBufferAllocateInfo cai { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
	cai.commandPool = D.pool;
	cai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	cai.commandBufferCount = 1;
	VkFenceCreateInfo fi { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
	fi.flags = VK_FENCE_CREATE_SIGNALED_BIT;   // every step and free waits on it first: born signalled (nothing in flight)
	if ( vk.vkAllocateCommandBuffers( D.dev, &cai, &g->cb ) != VK_SUCCESS || vk.vkCreateFence( D.dev, &fi, nullptr, &g->fence ) != VK_SUCCESS ) {
		delete g;
		return nullptr;
	}
	m[s] = g;
	return g;
}

//! the W x H working set; `share` makes the g-buffer and the ratio exportable (false + why when the device refuses)
bool restirMake( Dev & D, DdgiGpuSession * s, RestirGpu * g, RestirPipe & R, int W, int H, bool share, std::string * why )
{
	restirFreeBuffers( D, g );
	const VkDeviceSize n = VkDeviceSize( W ) * VkDeviceSize( H );
	const VkBufferUsageFlags u = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	const VkMemoryPropertyFlags cached = kHost | VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
	DdgiExtra & X = ddgiExtra( D );
	bool ok = true;
	if ( share ) {
		if ( !D.share || !X.getMemWin32 || !X.getSemWin32 || !X.createSem ) {
			*why = "the bake device has no Win32 external memory / semaphore export";
			return false;
		}
		ok = ddgiMakeShared( D, n * 16, u, g->gbuf, &g->gbufAlloc, why ) && ddgiMakeShared( D, n * 8, u, g->out, &g->outAlloc, why );
		g->vkDone = ok ? ddgiMakeSemaphore( D, X ) : VK_NULL_HANDLE;
		g->glDone = ok ? ddgiMakeSemaphore( D, X ) : VK_NULL_HANDLE;
		if ( ok && ( !g->vkDone || !g->glDone ) ) {
			*why = "the device refused an exportable semaphore";
			ok = false;
		}
		if ( !ok ) {
			restirFreeBuffers( D, g );
			return false;
		}
		g->shared = true;
	} else {
		ok = makeBuf( D, n * 16, u, { kDevLocal, kHost }, g->gbuf ) && makeBuf( D, n * 8, u, { kDevLocal, kHost }, g->out );
	}
	ok = ok && makeBuf( D, kRestirParamBytes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, { kHost }, g->params )
		&& makeBuf( D, n * 16, u, { kDevLocal, kHost }, g->prevGbuf ) && makeBuf( D, n * 16, u, { kDevLocal, kHost }, g->cur )
		&& makeBuf( D, n * 16, u, { kDevLocal, kHost }, g->hist ) && makeBuf( D, n * 4, u, { kDevLocal, kHost }, g->histPix )
		&& makeBuf( D, n * 16, u, { kDevLocal, kHost }, g->numDen ) && makeBuf( D, n * 16, u, { kDevLocal, kHost }, g->acc )
		&& makeBuf( D, n * 16, u, { kDevLocal, kHost }, g->accPrev ) && makeBuf( D, 4096 * 4, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, { kHost }, g->cand )
		&& g->params.map && g->cand.map;
	g->candCap = 4096;
	if ( ok ) {
		VkDescriptorSetAllocateInfo ai { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
		ai.descriptorPool = D.dpool;
		ai.descriptorSetCount = 1;
		ai.pSetLayouts = &R.dsl;
		ok = vk.vkAllocateDescriptorSets( D.dev, &ai, &g->set ) == VK_SUCCESS;
	}
	if ( ok ) {
		const Buf * bs[kRestirBindings] = { &g->params, &g->gbuf, &g->prevGbuf, &g->cur, &g->hist, &g->histPix, &g->numDen, &g->acc,
			&g->accPrev, &g->cand, &g->out };
		VkDescriptorBufferInfo bi[kRestirBindings] {};
		VkWriteDescriptorSet w[kRestirBindings] {};
		for ( int i = 0; i < kRestirBindings; i++ ) {
			bi[i] = { bs[i]->b, 0, VK_WHOLE_SIZE };
			w[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			w[i].dstSet = g->set;
			w[i].dstBinding = uint32_t( i );
			w[i].descriptorCount = 1;
			w[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			w[i].pBufferInfo = &bi[i];
		}
		vk.vkUpdateDescriptorSets( D.dev, kRestirBindings, w, 0, nullptr );
	}
	if ( !ok ) {
		*why = "the ReSTIR buffers did not allocate (out of GPU memory?)";
		restirFreeBuffers( D, g );
		return false;
	}
	g->W = W;
	g->H = H;
	(void)s;
	return true;
}

void restirDispatch( Dev & D, DdgiPipes & S, RestirPipe & R, DdgiGpuSession * s, RestirGpu * g, uint32_t mode, uint64_t groups )
{
	vk.vkCmdBindPipeline( D.cb, VK_PIPELINE_BIND_POINT_COMPUTE, R.pipe );
	const VkDescriptorSet sets[2] = { s->set, g->set };
	vk.vkCmdBindDescriptorSets( D.cb, VK_PIPELINE_BIND_POINT_COMPUTE, R.pl, 0, 2, sets, 0, nullptr );
	const DdgiPc pc = ddgiPc( nullptr, mode, 0, 0, 0 );
	vk.vkCmdPushConstants( D.cb, R.pl, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof pc, &pc );
	const uint64_t gx = std::min<uint64_t>( groups, 65535 );
	const uint64_t gy = ( groups + gx - 1 ) / gx;
	vk.vkCmdDispatch( D.cb, uint32_t( gx ), uint32_t( gy ), 1 );
	(void)S;
}

} // namespace

bool restirGpuShare( DdgiGpuSession * s, int W, int H, RestirGpuShare * out )
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
	RestirPipe & R = restirPipe( D, S );
	if ( !S.ok || !R.ok ) {
		out->why = S.ok ? R.why : S.why;
		return false;
	}
	RestirGpu * g = restirStateFor( D, s );
	if ( !g ) {
		out->why = "no command buffer for ReSTIR";
		return false;
	}
	if ( g->fence )
		vk.vkWaitForFences( D.dev, 1, &g->fence, VK_TRUE, UINT64_MAX );
	if ( !restirMake( D, s, g, R, W, H, true, &out->why ) )
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
		restirFreeBuffers( D, g );
		out->why = "the device gave no Win32 handle for a ReSTIR buffer or semaphore";
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

bool restirGpuStep( DdgiGpuSession * s, const RestirGpuIn & in, RestirGpuOut * out )
{
	*out = RestirGpuOut();
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
	RestirPipe & R = restirPipe( D, S );
	if ( !S.ok || !R.ok ) {
		out->why = S.ok ? R.why : S.why;
		return false;
	}
	RestirGpu * g = restirStateFor( D, s );
	if ( !g ) {
		out->why = "no command buffer for ReSTIR";
		return false;
	}
	vk.vkWaitForFences( D.dev, 1, &g->fence, VK_TRUE, UINT64_MAX );   // our own last submit (a step always waits: never pending)
	if ( g->W != in.W || g->H != in.H || ( g->shared && in.gbuf ) ) {
		if ( !in.gbuf && g->shared ) {
			out->why = "the shared g-buffer is for another size (restirGpuShare first)";
			return false;
		}
		if ( !restirMake( D, s, g, R, in.W, in.H, false, &out->why ) )
			return false;
	}
	const VkDeviceSize n = VkDeviceSize( in.W ) * VkDeviceSize( in.H );
	// the parameters (RestirParams, std430) and the candidate list, straight into host memory (no submit reads them now)
	{
		float p[44] = {};
		int32_t * pi = reinterpret_cast<int32_t *>( p );
		pi[0] = in.W;
		pi[1] = in.H;
		pi[2] = int32_t( std::min( in.cand.size(), g->candCap ) );
		pi[3] = int32_t( in.frame );
		pi[4] = in.candidates;
		pi[5] = in.spatial;
		pi[6] = in.filter;
		pi[7] = in.red;
		p[8] = in.radius;
		p[9] = in.bias;
		p[10] = in.clear;
		p[11] = in.history ? 1.0f : 0.0f;
		std::memcpy( p + 12, in.rows, sizeof in.rows );
		std::memcpy( p + 24, in.proj, sizeof in.proj );
		std::memcpy( p + 28, in.prows, sizeof in.prows );
		std::memcpy( p + 40, in.pproj, sizeof in.pproj );
		static_assert( sizeof p == kRestirParamBytes, "RestirParams" );
		std::memcpy( g->params.map, p, sizeof p );
		if ( !in.cand.empty() )
			std::memcpy( g->cand.map, in.cand.data(), std::min( in.cand.size(), g->candCap ) * 4 );
	}
	if ( in.gbuf && !ddgiPut( D, in.gbuf, size_t( n ) * 16, g->gbuf ) ) {
		out->why = "the g-buffer upload failed";
		return false;
	}
	if ( in.readBack && !g->ratioRb.b ) {
		const VkMemoryPropertyFlags cached = kHost | VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
		const VkBufferUsageFlags rb = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
		if ( !makeBuf( D, n * 8, rb, { cached, kHost }, g->ratioRb ) || !makeBuf( D, n * 16, rb, { cached, kHost }, g->numDenRb )
			|| !makeBuf( D, n * 16, rb, { cached, kHost }, g->resRb ) || !makeBuf( D, n * 16, rb, { cached, kHost }, g->accRb ) ) {
			out->why = "the read-back buffers did not allocate";
			return false;
		}
	}
	const bool gl = g->shared && bool( in.beforeSubmit ) && !in.gbuf;
	const uint64_t groups = ( uint64_t( n ) + 63 ) / 64;
	bool ok = true;
	{
		DdgiCbSwap sw( D, g->cb );
		ok = begin( D );
		if ( ok ) {
			if ( gl && g->released ) {
				ddgiOwner( D, g->gbuf.b, true );
				ddgiOwner( D, g->out.b, true );
			}
			for ( uint32_t mode = 0; mode < 4; mode++ ) {
				restirDispatch( D, S, R, s, g, mode, groups );
				ddgiBarrier( D );
			}
			ddgiCopyBarrier( D );
			ddgiCopy( D, g->acc, g->accPrev, n * 16 );          // this frame's running means: the next one's history
			ddgiCopy( D, g->gbuf, g->prevGbuf, n * 16 );        // and its g-buffer (the reservoirs: pass 1 wrote hist)
			if ( in.readBack ) {
				ddgiCopy( D, g->out, g->ratioRb, n * 8 );
				ddgiCopy( D, g->numDen, g->numDenRb, n * 16 );
				ddgiCopy( D, g->hist, g->resRb, n * 16 );
				ddgiCopy( D, g->acc, g->accRb, n * 16 );
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
		out->why = "a ReSTIR submit failed (lost device or out of memory)";
		return false;
	}
	g->released = gl;
	if ( in.readBack ) {
		const float * r = static_cast<const float *>( g->ratioRb.map );
		const float * nd = static_cast<const float *>( g->numDenRb.map );
		const float * a = static_cast<const float *>( g->accRb.map );
		const unsigned char * rs = static_cast<const unsigned char *>( g->resRb.map );
		out->ratio.assign( r, r + size_t( n ) * 2 );
		out->numDen.assign( nd, nd + size_t( n ) * 4 );
		out->acc.assign( a, a + size_t( n ) * 4 );
		out->res.resize( size_t( n ) * 4 );
		for ( size_t i = 0; i < size_t( n ); i++ ) {
			int32_t y = 0;
			std::memcpy( &y, rs + i * 16, 4 );
			out->res[i * 4] = float( y );
			std::memcpy( &out->res[i * 4 + 1], rs + i * 16 + 4, 12 );
		}
	}
	out->msWall = std::chrono::duration<double, std::milli>( std::chrono::steady_clock::now() - t0 ).count();
	out->ok = true;
	return true;
}

#endif // RESTIR_HAVE_SPIRV
