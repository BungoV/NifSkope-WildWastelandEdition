/* Lane RTXGI1 (2026-10-29): the DDGI volume's GPU run (src/rtddgi/rtddgi_gpu.h). INCLUDED AT THE END OF src/bakert.cpp,
 * after its namespace closes, so it reopens namespace bakert and reuses that file's device (dev()), loader table (vk),
 * buffer helpers (makeBuf, upload, begin, submitWait, pipeline, freeBuf) and the scene build (buildMasks, buildRt).
 * Not a translation unit of its own: do not add it to the .pro's SOURCES.
 *
 * The kernels (src/rtddgi/NAME.comp) compile to src/rtddgi/NAME.spv.h with tools/rtddgi_spirv.sh. Until those headers
 * exist this file compiles to a stub that says so, and the volume runs on the CPU twin.
 *
 * M2 (2026-10-29): the run is a session (open, steps, close) so the cell view keeps a volume converging over frames.
 * The working buffers are device local (M1 had them host visible); each step copies irr, dist and age to host
 * buffers. The kernels and their order are M1FIX's: the same dispatches, a barrier between every two. */

#include "rtddgi_gpu.h"

#include <algorithm>
#include <thread>   // ddgiGpuRun waits for the device's lock (bakert.cpp does not include it)

#if defined( __has_include )
#if __has_include( "trace.spv.h" ) && __has_include( "blend.spv.h" ) && __has_include( "border.spv.h" ) && __has_include( "reloc.spv.h" )
#define RTDDGI_HAVE_SPIRV 1
#endif
#endif

namespace bakert {

#ifndef RTDDGI_HAVE_SPIRV

static const char * kDdgiNoKernels = "the DDGI kernels are not compiled into this build (run tools/rtddgi_spirv.sh, then rebuild)";

bool ddgiGpuRun( const DdgiGpuJob &, DdgiGpuOut * out )
{
	out->ok = false;
	out->why = kDdgiNoKernels;
	return false;
}
DdgiGpuSession * ddgiGpuOpen( const DdgiGpuJob &, DdgiGpuOut * out )
{
	*out = DdgiGpuOut();
	out->why = kDdgiNoKernels;
	return nullptr;
}
bool ddgiGpuStep( DdgiGpuSession *, const float *, int, uint32_t, bool, DdgiGpuStepOut * out )
{
	*out = DdgiGpuStepOut();
	out->why = kDdgiNoKernels;
	return false;
}
const float * ddgiGpuHostIrr( const DdgiGpuSession * ) { return nullptr; }
const float * ddgiGpuHostDist( const DdgiGpuSession * ) { return nullptr; }
const uint32_t * ddgiGpuHostAge( const DdgiGpuSession * ) { return nullptr; }
const float * ddgiGpuHostProbes( const DdgiGpuSession * ) { return nullptr; }
const float * ddgiGpuHostRays( const DdgiGpuSession * ) { return nullptr; }
void ddgiGpuClose( DdgiGpuSession * ) {}

#else

namespace {
#include "trace.spv.h"
#include "blend.spv.h"
#include "border.spv.h"
#include "reloc.spv.h"

constexpr int kDdgiBindings = 15;   // ddgi_common.glsl bindings 0..14, 6 = the TLAS
struct DdgiPc { float rot0[4]; float rot1[4]; float rot2[4]; uint32_t job[4]; };
static_assert( sizeof( DdgiPc ) == 64, "push constants" );
constexpr int kDdgiStepMax = 16;     // updates a step times (ddgiGpuRun steps in chunks of this); 3 stamps each + 2
constexpr uint32_t kDdgiStamps = 3 * kDdgiStepMax + 2;

// the four pipelines live as long as the process, as bakert's own do (made once, under D.m)
struct DdgiPipes
{
	bool tried = false, ok = false;
	std::string why;
	VkDescriptorSetLayout dsl = VK_NULL_HANDLE;
	VkPipelineLayout pl = VK_NULL_HANDLE;
	VkPipeline pipe[4] = { VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE };   // trace, blend, border, reloc
};

DdgiPipes & ddgiPipes( Dev & D )
{
	static DdgiPipes S;
	if ( S.tried )
		return S;
	S.tried = true;
	std::vector<VkDescriptorSetLayoutBinding> b;
	for ( int i = 0; i < kDdgiBindings; i++ ) {
		VkDescriptorSetLayoutBinding x {};
		x.binding = uint32_t( i );
		x.descriptorType = i == 6 ? VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR : VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		x.descriptorCount = 1;
		x.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
		b.push_back( x );
	}
	VkDescriptorSetLayoutCreateInfo li { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
	li.bindingCount = uint32_t( b.size() );
	li.pBindings = b.data();
	if ( vk.vkCreateDescriptorSetLayout( D.dev, &li, nullptr, &S.dsl ) != VK_SUCCESS ) {
		S.why = "no descriptor set layout";
		return S;
	}
	VkPushConstantRange pr { VK_SHADER_STAGE_COMPUTE_BIT, 0, uint32_t( sizeof( DdgiPc ) ) };
	VkPipelineLayoutCreateInfo pi { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
	pi.setLayoutCount = 1;
	pi.pSetLayouts = &S.dsl;
	pi.pushConstantRangeCount = 1;
	pi.pPushConstantRanges = &pr;
	if ( vk.vkCreatePipelineLayout( D.dev, &pi, nullptr, &S.pl ) != VK_SUCCESS ) {
		S.why = "no pipeline layout";
		return S;
	}
	S.pipe[0] = pipeline( D, rtddgi_trace_spv, sizeof rtddgi_trace_spv, S.pl );
	S.pipe[1] = pipeline( D, rtddgi_blend_spv, sizeof rtddgi_blend_spv, S.pl );
	S.pipe[2] = pipeline( D, rtddgi_border_spv, sizeof rtddgi_border_spv, S.pl );
	S.pipe[3] = pipeline( D, rtddgi_reloc_spv, sizeof rtddgi_reloc_spv, S.pl );
	S.ok = S.pipe[0] && S.pipe[1] && S.pipe[2] && S.pipe[3];
	if ( !S.ok )
		S.why = "a DDGI kernel did not build (trace, blend, border, reloc)";
	return S;
}

/* M2: the GPU timer and the copy helpers bakert's table does not load (vkGetDeviceProcAddr, once). Without them the
 * census says "no timestamps" and the copies still run (vkCmdCopyBuffer is in the table). */
struct DdgiTimer
{
	bool tried = false, ok = false;
	double nsPerTick = 0.0;
	PFN_vkCreateQueryPool create = nullptr;
	PFN_vkDestroyQueryPool destroy = nullptr;
	PFN_vkCmdResetQueryPool reset = nullptr;
	PFN_vkCmdWriteTimestamp write = nullptr;
	PFN_vkGetQueryPoolResults get = nullptr;
};

DdgiTimer & ddgiTimer( Dev & D )
{
	static DdgiTimer T;
	if ( T.tried )
		return T;
	T.tried = true;
	if ( !vk.vkGetDeviceProcAddr )
		return T;
	T.create = reinterpret_cast<PFN_vkCreateQueryPool>( vk.vkGetDeviceProcAddr( D.dev, "vkCreateQueryPool" ) );
	T.destroy = reinterpret_cast<PFN_vkDestroyQueryPool>( vk.vkGetDeviceProcAddr( D.dev, "vkDestroyQueryPool" ) );
	T.reset = reinterpret_cast<PFN_vkCmdResetQueryPool>( vk.vkGetDeviceProcAddr( D.dev, "vkCmdResetQueryPool" ) );
	T.write = reinterpret_cast<PFN_vkCmdWriteTimestamp>( vk.vkGetDeviceProcAddr( D.dev, "vkCmdWriteTimestamp" ) );
	T.get = reinterpret_cast<PFN_vkGetQueryPoolResults>( vk.vkGetDeviceProcAddr( D.dev, "vkGetQueryPoolResults" ) );
	VkPhysicalDeviceProperties pp {};
	vk.vkGetPhysicalDeviceProperties( D.pd, &pp );
	uint32_t nq = 0;
	vk.vkGetPhysicalDeviceQueueFamilyProperties( D.pd, &nq, nullptr );
	std::vector<VkQueueFamilyProperties> qf( nq );
	vk.vkGetPhysicalDeviceQueueFamilyProperties( D.pd, &nq, qf.data() );
	const bool bits = D.qf < nq && qf[D.qf].timestampValidBits > 0;
	T.nsPerTick = double( pp.limits.timestampPeriod );
	T.ok = T.create && T.destroy && T.reset && T.write && T.get && bits && T.nsPerTick > 0.0;
	return T;
}

void ddgiBarrier( Dev & D )
{
	VkMemoryBarrier mb { VK_STRUCTURE_TYPE_MEMORY_BARRIER };
	mb.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
	mb.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
	vk.vkCmdPipelineBarrier( D.cb, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &mb, 0, nullptr, 0, nullptr );
}

// the shaders' writes made ready for the copies to the host buffers
void ddgiCopyBarrier( Dev & D )
{
	VkMemoryBarrier mb { VK_STRUCTURE_TYPE_MEMORY_BARRIER };
	mb.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
	mb.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
	vk.vkCmdPipelineBarrier( D.cb, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 1, &mb, 0, nullptr, 0, nullptr );
}

// the end of a submit: the copies visible to the host's reads (after the fence), every write to the next submit's kernels
void ddgiEndBarrier( Dev & D )
{
	VkMemoryBarrier mb { VK_STRUCTURE_TYPE_MEMORY_BARRIER };
	mb.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
	mb.dstAccessMask = VK_ACCESS_HOST_READ_BIT | VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_READ_BIT;
	vk.vkCmdPipelineBarrier( D.cb, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT,
		VK_PIPELINE_STAGE_HOST_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 1, &mb, 0, nullptr, 0, nullptr );
}

// one dispatch of `groups` workgroups, split over y past 65535 (the shaders read a linear id)
void ddgiDispatch( Dev & D, DdgiPipes & S, VkDescriptorSet set, int which, const DdgiPc & pc, uint64_t groups )
{
	if ( !groups )
		return;
	vk.vkCmdBindPipeline( D.cb, VK_PIPELINE_BIND_POINT_COMPUTE, S.pipe[which] );
	vk.vkCmdBindDescriptorSets( D.cb, VK_PIPELINE_BIND_POINT_COMPUTE, S.pl, 0, 1, &set, 0, nullptr );
	vk.vkCmdPushConstants( D.cb, S.pl, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof pc, &pc );
	const uint64_t gx = std::min<uint64_t>( groups, 65535 );
	const uint64_t gy = ( groups + gx - 1 ) / gx;
	vk.vkCmdDispatch( D.cb, uint32_t( gx ), uint32_t( gy ), 1 );
}

DdgiPc ddgiPc( const float * rot, uint32_t mode, uint32_t frame, uint32_t first, uint32_t count )
{
	DdgiPc pc {};
	static const float id[9] = { 1, 0, 0, 0, 1, 0, 0, 0, 1 };
	const float * r = rot ? rot : id;
	for ( int k = 0; k < 3; k++ ) {
		pc.rot0[k] = r[k];
		pc.rot1[k] = r[3 + k];
		pc.rot2[k] = r[6 + k];
	}
	pc.job[0] = mode;
	pc.job[1] = frame;
	pc.job[2] = first;
	pc.job[3] = count;
	return pc;
}

// M2: `bytes` from `data` into an existing buffer: straight when it is mapped, else through a staging copy
bool ddgiPut( Dev & D, const void * data, size_t bytes, Buf & dst )
{
	if ( !bytes )
		return true;
	if ( dst.map ) {
		std::memcpy( dst.map, data, bytes );
		return true;
	}
	Buf st;
	if ( !makeBuf( D, bytes, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, { kHost }, st ) )
		return false;
	std::memcpy( st.map, data, bytes );
	bool ok = begin( D );
	if ( ok ) {
		VkBufferCopy c { 0, 0, bytes };
		vk.vkCmdCopyBuffer( D.cb, st.b, dst.b, 1, &c );
		ddgiEndBarrier( D );
		ok = submitWait( D );
	}
	freeBuf( D, st );
	return ok;
}

void ddgiCopy( Dev & D, Buf & from, Buf & to, VkDeviceSize bytes )
{
	VkBufferCopy c { 0, 0, bytes };
	vk.vkCmdCopyBuffer( D.cb, from.b, to.b, 1, &c );
}

} // namespace

struct DdgiGpuSession
{
	Scene::Impl I;
	DdgiGpuParams prm;
	Buf rays, irr, dist, probes, age, attr, lights, params;   // the device's working set
	Buf irrRb, distRb, probesRb, ageRb, raysRb;              // the host copies
	VkDescriptorSet set = VK_NULL_HANDLE;
	VkQueryPool qp = VK_NULL_HANDLE;
	uint64_t nProbes = 0;
	uint32_t N = 0;
	long long nActive = 0;
	bool stepped = false, raysCopied = false;
};

namespace {

void ddgiFree( Dev & D, DdgiGpuSession * s )
{
	if ( s->set )
		vk.vkFreeDescriptorSets( D.dev, D.dpool, 1, &s->set );
	DdgiTimer & T = ddgiTimer( D );
	if ( s->qp && T.destroy )
		T.destroy( D.dev, s->qp, nullptr );
	for ( Buf * b : { &s->rays, &s->irr, &s->dist, &s->probes, &s->age, &s->attr, &s->lights, &s->params, &s->irrRb, &s->distRb,
			&s->probesRb, &s->ageRb, &s->raysRb } )
		freeBuf( D, *b );
	s->I.release( D );
	delete s;
}

} // namespace

DdgiGpuSession * ddgiGpuOpen( const DdgiGpuJob & job, DdgiGpuOut * out )
{
	*out = DdgiGpuOut();
	Dev & D = dev();
	if ( !D.ok || !D.rt ) {
		out->why = D.ok ? "the bake device has no ray query" : ( "no bake device: " + D.why );
		return nullptr;
	}
	if ( !job.bvh || job.bvh->t.empty() ) {
		out->why = "an empty soup";
		return nullptr;
	}
	const uint64_t nProbes = uint64_t( job.params.dims[3] );
	const uint32_t N = uint32_t( job.params.counts[0] );
	if ( !nProbes || N <= 32 || N > 256 ) {
		out->why = "no probes, or rays per probe outside 33..256";
		return nullptr;
	}
	std::lock_guard<std::mutex> lk( D.m );
	out->device = D.name;
	const auto t0 = std::chrono::steady_clock::now();
	DdgiPipes & S = ddgiPipes( D );
	if ( !S.ok ) {
		out->why = S.why;
		return nullptr;
	}
	DdgiGpuSession * s = new DdgiGpuSession;
	s->nProbes = nProbes;
	s->N = N;
	if ( !buildMasks( D, *job.bvh, s->I ) || !buildRt( D, *job.bvh, s->I ) ) {
		ddgiFree( D, s );
		out->why = "the soup's BLAS did not build (out of GPU memory?)";
		return nullptr;
	}
	out->blasGrow = s->I.inflate;   // M1FIX: the BLAS holds grown triangles when > 0; trace.comp re-tests the true ones
	out->blasGrowTris = s->I.inflateTris;
	s->prm = job.params;
	s->prm.counts[2] = int32_t( s->I.nTri );
	s->prm.counts[3] = int32_t( s->I.nOpaque );
	s->prm.flags[2] = int32_t( s->I.useMask );

	const VkBufferUsageFlags u = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	const VkBufferUsageFlags rb = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	const VkMemoryPropertyFlags cached = kHost | VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
	std::vector<float> p0( size_t( nProbes ) * 4, 0.0f );
	for ( uint64_t p = 0; p < nProbes; p++ ) {
		for ( int k = 0; k < 3 && ( p * 3 + 2 ) < job.offsets.size(); k++ )
			p0[size_t( p ) * 4 + size_t( k )] = job.offsets[size_t( p ) * 3 + size_t( k )];
		p0[size_t( p ) * 4 + 3] = 2.0f;   // Vigilant until classified
	}
	const std::vector<float> zIrr( size_t( nProbes ) * 64 * 4, 0.0f ), zDist( size_t( nProbes ) * 256 * 2, 0.0f );
	const std::vector<uint32_t> zAge( size_t( nProbes ), 0u );
	bool ok = makeBuf( D, nProbes * N * 16, u, { kDevLocal, kHost }, s->rays )
		&& makeBuf( D, nProbes * 64 * 16, u, { kDevLocal, kHost }, s->irr )
		&& makeBuf( D, nProbes * 256 * 8, u, { kDevLocal, kHost }, s->dist )
		&& makeBuf( D, nProbes * 16, u, { kDevLocal, kHost }, s->probes )
		&& makeBuf( D, nProbes * 4, u, { kDevLocal, kHost }, s->age )
		&& makeBuf( D, nProbes * 64 * 16, rb, { cached, kHost }, s->irrRb )
		&& makeBuf( D, nProbes * 256 * 8, rb, { cached, kHost }, s->distRb )
		&& makeBuf( D, nProbes * 16, rb, { cached, kHost }, s->probesRb )
		&& makeBuf( D, nProbes * 4, rb, { cached, kHost }, s->ageRb )
		&& ( !job.dumpFixed || makeBuf( D, nProbes * N * 16, rb, { cached, kHost }, s->raysRb ) )
		&& upload( D, job.triAttr.data(), job.triAttr.size() * 4, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, s->attr )
		&& upload( D, job.lights.data(), job.lights.size() * 4, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, s->lights )
		&& upload( D, &s->prm, sizeof s->prm, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, s->params );
	ok = ok && s->irrRb.map && s->distRb.map && s->probesRb.map && s->ageRb.map && ( !job.dumpFixed || s->raysRb.map );
	ok = ok && ddgiPut( D, zIrr.data(), zIrr.size() * 4, s->irr ) && ddgiPut( D, zDist.data(), zDist.size() * 4, s->dist )
		&& ddgiPut( D, zAge.data(), zAge.size() * 4, s->age ) && ddgiPut( D, p0.data(), p0.size() * 4, s->probes );
	if ( ok ) {
		VkDescriptorSetAllocateInfo ai { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
		ai.descriptorPool = D.dpool;
		ai.descriptorSetCount = 1;
		ai.pSetLayouts = &S.dsl;
		ok = vk.vkAllocateDescriptorSets( D.dev, &ai, &s->set ) == VK_SUCCESS;
	}
	if ( ok ) {
		const Buf * bs[kDdgiBindings] = { &s->rays, &s->irr, &s->dist, &s->probes, &s->age, &s->attr, nullptr, &s->I.remap, &s->I.verts,
			&s->I.triOf, &s->I.maskTris, &s->I.maps, &s->I.alpha, &s->lights, &s->params };
		VkDescriptorBufferInfo bi[kDdgiBindings] {};
		std::vector<VkWriteDescriptorSet> w;
		VkWriteDescriptorSetAccelerationStructureKHR wa { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR };
		wa.accelerationStructureCount = 1;
		wa.pAccelerationStructures = &s->I.tlas;
		for ( int i = 0; i < kDdgiBindings; i++ ) {
			VkWriteDescriptorSet x { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
			x.dstSet = s->set;
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
	}
	DdgiTimer & T = ddgiTimer( D );
	if ( ok && T.ok ) {
		VkQueryPoolCreateInfo qi { VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO };
		qi.queryType = VK_QUERY_TYPE_TIMESTAMP;
		qi.queryCount = kDdgiStamps;
		if ( T.create( D.dev, &qi, nullptr, &s->qp ) != VK_SUCCESS )
			s->qp = VK_NULL_HANDLE;   // no timer: the census says so
	}
	const auto t1 = std::chrono::steady_clock::now();
	out->msBuild = std::chrono::duration<double, std::milli>( t1 - t0 ).count();

	// relocation (5 iterations) and classification from the fixed rays [J21 5, 6]
	const uint64_t fixedGroups = ( nProbes * 32 + 63 ) / 64, probeGroups = ( nProbes + 63 ) / 64;
	const int iters = job.relocate ? 5 : 0;
	for ( int it = 0; ok && it < iters + ( job.classify ? 1 : 0 ); it++ ) {
		ok = begin( D );
		if ( !ok )
			break;
		ddgiDispatch( D, S, s->set, 0, ddgiPc( nullptr, 1, 0, 0, 32 ), fixedGroups );
		ddgiBarrier( D );
		ddgiDispatch( D, S, s->set, 3, ddgiPc( nullptr, it < iters ? 4u : 5u, 0, 0, 0 ), probeGroups );
		ddgiEndBarrier( D );
		ok = submitWait( D );
		out->rays += (long long)( nProbes * 32 );
	}
	// the states (and offsets) to the host once: the updates do not change them
	if ( ok ) {
		ok = begin( D );
		if ( ok ) {
			ddgiCopy( D, s->probes, s->probesRb, nProbes * 16 );
			ddgiEndBarrier( D );
			ok = submitWait( D );
		}
	}
	out->msReloc = std::chrono::duration<double, std::milli>( std::chrono::steady_clock::now() - t1 ).count();
	if ( !ok ) {
		out->why = "a DDGI submit failed (lost device or out of memory)";
		ddgiFree( D, s );
		return nullptr;
	}
	// M1FIX: the updates trace only the active probes (trace.comp mode 0 returns for Off and skipped Asleep), and the
	// states do not change during them, so count those (the twin's count), not every probe
	const float * pr = static_cast<const float *>( s->probesRb.map );
	for ( uint64_t p = 0; p < nProbes; p++ ) {
		const int st = int( pr[p * 4 + 3] + 0.5f );
		s->nActive += ( st == 0 || ( st == 1 && s->prm.flags[1] ) ) ? 0 : 1;   // rtddgimath.h kOff / kAsleep
	}
	out->probes.assign( pr, pr + size_t( nProbes ) * 4 );
	out->ok = true;
	return s;
}

bool ddgiGpuStep( DdgiGpuSession * s, const float * rotations, int k, uint32_t frame0, bool copyRays, DdgiGpuStepOut * out )
{
	*out = DdgiGpuStepOut();
	if ( !s ) {
		out->why = "no session";
		return false;
	}
	Dev & D = dev();
	std::unique_lock<std::mutex> lk( D.m, std::try_to_lock );
	if ( !lk.owns_lock() ) {   // a bake holds the device: this frame runs no update
		out->busy = true;
		out->why = "the bake device is busy";
		return false;
	}
	DdgiPipes & S = ddgiPipes( D );
	DdgiTimer & T = ddgiTimer( D );
	k = std::max( 0, std::min( k, kDdgiStepMax ) );
	copyRays = copyRays && s->raysRb.b;
	const uint64_t nProbes = s->nProbes;
	const uint32_t N = s->N;
	const uint64_t traceGroups = ( nProbes * N + 63 ) / 64;
	const uint64_t borderGroups = ( nProbes * 88 + 63 ) / 64;
	const bool timed = s->qp != VK_NULL_HANDLE;
	const auto t0 = std::chrono::steady_clock::now();
	bool ok = begin( D );
	if ( ok ) {
		uint32_t q = 0;
		auto stamp = [&]() {
			if ( timed )
				T.write( D.cb, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, s->qp, q++ );
		};
		if ( timed )
			T.reset( D.cb, s->qp, 0, kDdgiStamps );
		stamp();
		// the updates: trace every ray of the active probes, blend, border (M1FIX's order, one barrier between each two)
		for ( int f = 0; f < k; f++ ) {
			const float * rot = rotations ? rotations + size_t( f ) * 9 : nullptr;
			const uint32_t fr = frame0 + uint32_t( f );
			ddgiDispatch( D, S, s->set, 0, ddgiPc( rot, 0, fr, 0, N ), traceGroups );
			ddgiBarrier( D );
			stamp();
			ddgiDispatch( D, S, s->set, 1, ddgiPc( rot, 2, fr, 0, N ), nProbes );
			ddgiBarrier( D );
			stamp();
			ddgiDispatch( D, S, s->set, 2, ddgiPc( rot, 3, fr, 0, 0 ), borderGroups );
			ddgiBarrier( D );
			stamp();
		}
		ddgiCopyBarrier( D );
		ddgiCopy( D, s->irr, s->irrRb, nProbes * 64 * 16 );
		ddgiCopy( D, s->dist, s->distRb, nProbes * 256 * 8 );
		ddgiCopy( D, s->age, s->ageRb, nProbes * 4 );
		if ( copyRays )
			ddgiCopy( D, s->rays, s->raysRb, nProbes * N * 16 );
		stamp();
		ddgiEndBarrier( D );
		ok = submitWait( D );
		out->msWall = std::chrono::duration<double, std::milli>( std::chrono::steady_clock::now() - t0 ).count();
		if ( ok && timed ) {
			std::vector<uint64_t> ts( q, 0 );
			if ( q && T.get( D.dev, s->qp, 0, q, ts.size() * 8, ts.data(), 8, VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT ) == VK_SUCCESS ) {
				const double ms = T.nsPerTick / 1e6;
				for ( int c = 0; c < 4; c++ )
					out->ms[c] = 0.0;
				for ( int f = 0; f < k; f++ )
					for ( int c = 0; c < 3; c++ )
						out->ms[c] += double( ts[size_t( f ) * 3 + size_t( c ) + 1] - ts[size_t( f ) * 3 + size_t( c )] ) * ms;
				out->ms[3] = double( ts[q - 1] - ts[q - 2] ) * ms;
			}
		}
	}
	if ( !ok ) {
		out->why = "a DDGI submit failed (lost device or out of memory)";
		return false;
	}
	s->stepped = true;
	s->raysCopied = s->raysCopied || copyRays;
	out->rays = s->nActive * (long long)N * k;
	out->ok = true;
	return true;
}

const float * ddgiGpuHostIrr( const DdgiGpuSession * s ) { return s && s->stepped ? static_cast<const float *>( s->irrRb.map ) : nullptr; }
const float * ddgiGpuHostDist( const DdgiGpuSession * s ) { return s && s->stepped ? static_cast<const float *>( s->distRb.map ) : nullptr; }
const uint32_t * ddgiGpuHostAge( const DdgiGpuSession * s ) { return s && s->stepped ? static_cast<const uint32_t *>( s->ageRb.map ) : nullptr; }
const float * ddgiGpuHostProbes( const DdgiGpuSession * s ) { return s ? static_cast<const float *>( s->probesRb.map ) : nullptr; }
const float * ddgiGpuHostRays( const DdgiGpuSession * s ) { return s && s->raysCopied ? static_cast<const float *>( s->raysRb.map ) : nullptr; }

void ddgiGpuClose( DdgiGpuSession * s )
{
	if ( !s )
		return;
	Dev & D = dev();
	std::lock_guard<std::mutex> lk( D.m );
	ddgiFree( D, s );
}

bool ddgiGpuRun( const DdgiGpuJob & job, DdgiGpuOut * out )
{
	DdgiGpuSession * s = ddgiGpuOpen( job, out );
	if ( !s )
		return false;
	const auto t2 = std::chrono::steady_clock::now();
	bool ok = true;
	int f = 0;
	do {   // chunks of up to 16 updates; the last chunk (or a 0-update run) copies the tiles back
		const int k = std::min( kDdgiStepMax, job.frames - f );
		const float * rot = job.rotations.size() >= size_t( f + std::max( k, 0 ) ) * 9 && k > 0 ? &job.rotations[size_t( f ) * 9] : nullptr;
		DdgiGpuStepOut so;
		for ( int tries = 0; tries < 10000; tries++ ) {   // ddgiGpuStep never waits for the lock; the CLI does
			ok = ddgiGpuStep( s, rot, std::max( k, 0 ), uint32_t( f ), job.dumpFixed && f + k >= job.frames, &so );
			if ( ok || !so.busy )
				break;
			std::this_thread::sleep_for( std::chrono::milliseconds( 5 ) );
		}
		if ( !ok ) {
			out->why = so.why;
			break;
		}
		out->rays += so.rays;
		f += std::max( k, 0 );
	} while ( f < job.frames );
	out->msFrames = std::chrono::duration<double, std::milli>( std::chrono::steady_clock::now() - t2 ).count();
	if ( ok ) {
		const uint64_t nProbes = uint64_t( job.params.dims[3] );
		const uint32_t N = uint32_t( job.params.counts[0] );
		const float * irr = ddgiGpuHostIrr( s ), * dist = ddgiGpuHostDist( s ), * probes = ddgiGpuHostProbes( s );
		const uint32_t * age = ddgiGpuHostAge( s );
		out->probes.assign( probes, probes + size_t( nProbes ) * 4 );
		out->age.assign( age, age + size_t( nProbes ) );
		out->irr.assign( irr, irr + size_t( nProbes ) * 64 * 4 );
		out->dist.assign( dist, dist + size_t( nProbes ) * 256 * 2 );
		if ( job.dumpFixed ) {   // rays[p * N + i].w, i < 32: the last trace of each probe's fixed rays
			const float * r = ddgiGpuHostRays( s );
			out->fixedDist.assign( size_t( nProbes ) * 32, 0.0f );
			for ( uint64_t p = 0; r && p < nProbes; p++ )
				for ( int i = 0; i < 32; i++ )
					out->fixedDist[size_t( p ) * 32 + size_t( i )] = r[( p * N + uint64_t( i ) ) * 4 + 3];
		}
	}
	ddgiGpuClose( s );
	out->ok = ok;
	return ok;
}

#endif // RTDDGI_HAVE_SPIRV

} // namespace bakert
