/* Lane RTXGI1 (2026-10-29): the DDGI volume's GPU run (src/rtddgi/rtddgi_gpu.h). INCLUDED AT THE END OF src/bakert.cpp,
 * after its namespace closes, so it reopens namespace bakert and reuses that file's device (dev()), loader table (vk),
 * buffer helpers (makeBuf, upload, begin, submitWait, pipeline, freeBuf) and the scene build (buildMasks, buildRt).
 * Not a translation unit of its own: do not add it to the .pro's SOURCES.
 *
 * The kernels (src/rtddgi/NAME.comp) compile to src/rtddgi/NAME.spv.h with tools/rtddgi_spirv.sh. Until those headers
 * exist this file compiles to a stub that says so, and the volume runs on the CPU twin. */

#include "rtddgi_gpu.h"

#if defined( __has_include )
#if __has_include( "trace.spv.h" ) && __has_include( "blend.spv.h" ) && __has_include( "border.spv.h" ) && __has_include( "reloc.spv.h" )
#define RTDDGI_HAVE_SPIRV 1
#endif
#endif

namespace bakert {

#ifndef RTDDGI_HAVE_SPIRV

bool ddgiGpuRun( const DdgiGpuJob &, DdgiGpuOut * out )
{
	out->ok = false;
	out->why = "the DDGI kernels are not compiled into this build (run tools/rtddgi_spirv.sh, then rebuild)";
	return false;
}

#else

namespace {
#include "trace.spv.h"
#include "blend.spv.h"
#include "border.spv.h"
#include "reloc.spv.h"

constexpr int kDdgiBindings = 15;   // ddgi_common.glsl bindings 0..14, 6 = the TLAS
struct DdgiPc { float rot0[4]; float rot1[4]; float rot2[4]; uint32_t job[4]; };
static_assert( sizeof( DdgiPc ) == 64, "push constants" );

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

void ddgiBarrier( Dev & D )
{
	VkMemoryBarrier mb { VK_STRUCTURE_TYPE_MEMORY_BARRIER };
	mb.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
	mb.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
	vk.vkCmdPipelineBarrier( D.cb, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &mb, 0, nullptr, 0, nullptr );
}

// the shaders' writes made visible to the host's mapped reads (after the submit's fence) and to the next dispatch
void ddgiHostBarrier( Dev & D )
{
	VkMemoryBarrier mb { VK_STRUCTURE_TYPE_MEMORY_BARRIER };
	mb.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
	mb.dstAccessMask = VK_ACCESS_HOST_READ_BIT | VK_ACCESS_SHADER_READ_BIT;
	vk.vkCmdPipelineBarrier( D.cb, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
		0, 1, &mb, 0, nullptr, 0, nullptr );
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

} // namespace

bool ddgiGpuRun( const DdgiGpuJob & job, DdgiGpuOut * out )
{
	*out = DdgiGpuOut();
	Dev & D = dev();
	if ( !D.ok || !D.rt ) {
		out->why = D.ok ? "the bake device has no ray query" : ( "no bake device: " + D.why );
		return false;
	}
	if ( !job.bvh || job.bvh->t.empty() ) {
		out->why = "an empty soup";
		return false;
	}
	const uint64_t nProbes = uint64_t( job.params.dims[3] );
	const uint32_t N = uint32_t( job.params.counts[0] );
	if ( !nProbes || N <= 32 || N > 256 ) {
		out->why = "no probes, or rays per probe outside 33..256";
		return false;
	}
	std::lock_guard<std::mutex> lk( D.m );
	out->device = D.name;
	const auto t0 = std::chrono::steady_clock::now();
	DdgiPipes & S = ddgiPipes( D );
	if ( !S.ok ) {
		out->why = S.why;
		return false;
	}
	Scene::Impl I;
	if ( !buildMasks( D, *job.bvh, I ) || !buildRt( D, *job.bvh, I ) ) {
		I.release( D );
		out->why = "the soup's BLAS did not build (out of GPU memory?)";
		return false;
	}
	out->blasGrow = I.inflate;   // M1FIX: the BLAS holds grown triangles when > 0; trace.comp re-tests the true ones
	out->blasGrowTris = I.inflateTris;
	DdgiGpuParams prm = job.params;
	prm.counts[2] = int32_t( I.nTri );
	prm.counts[3] = int32_t( I.nOpaque );
	prm.flags[2] = int32_t( I.useMask );

	const VkBufferUsageFlags u = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
	Buf rays, irr, dist, probes, age, attr, lights, params;
	std::vector<float> p0( size_t( nProbes ) * 4, 0.0f );
	for ( uint64_t p = 0; p < nProbes; p++ ) {
		for ( int k = 0; k < 3 && ( p * 3 + 2 ) < job.offsets.size(); k++ )
			p0[size_t( p ) * 4 + size_t( k )] = job.offsets[size_t( p ) * 3 + size_t( k )];
		p0[size_t( p ) * 4 + 3] = 2.0f;   // Vigilant until classified
	}
	// the rays stay on the device unless the fixed distances are read back (--dump-fixed)
	bool ok = ( job.dumpFixed ? makeBuf( D, nProbes * N * 16, u, { kHost | kDevLocal, kHost }, rays )
			: makeBuf( D, nProbes * N * 16, u, { kDevLocal, kHost }, rays ) )
		&& makeBuf( D, nProbes * 64 * 16, u, { kHost | kDevLocal, kHost }, irr )
		&& makeBuf( D, nProbes * 256 * 8, u, { kHost | kDevLocal, kHost }, dist )
		&& makeBuf( D, nProbes * 16, u, { kHost | kDevLocal, kHost }, probes )
		&& makeBuf( D, nProbes * 4, u, { kHost | kDevLocal, kHost }, age )
		&& upload( D, job.triAttr.data(), job.triAttr.size() * 4, u, attr )
		&& upload( D, job.lights.data(), job.lights.size() * 4, u, lights )
		&& upload( D, &prm, sizeof prm, u, params );
	ok = ok && irr.map && dist.map && probes.map && age.map && ( !job.dumpFixed || rays.map );
	if ( ok ) {
		std::memset( irr.map, 0, size_t( nProbes * 64 * 16 ) );
		std::memset( dist.map, 0, size_t( nProbes * 256 * 8 ) );
		std::memset( age.map, 0, size_t( nProbes * 4 ) );
		std::memcpy( probes.map, p0.data(), p0.size() * 4 );
	}
	VkDescriptorSet set = VK_NULL_HANDLE;
	if ( ok ) {
		VkDescriptorSetAllocateInfo ai { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
		ai.descriptorPool = D.dpool;
		ai.descriptorSetCount = 1;
		ai.pSetLayouts = &S.dsl;
		ok = vk.vkAllocateDescriptorSets( D.dev, &ai, &set ) == VK_SUCCESS;
	}
	if ( ok ) {
		const Buf * bs[kDdgiBindings] = { &rays, &irr, &dist, &probes, &age, &attr, nullptr, &I.remap, &I.verts, &I.triOf,
			&I.maskTris, &I.maps, &I.alpha, &lights, &params };
		VkDescriptorBufferInfo bi[kDdgiBindings] {};
		std::vector<VkWriteDescriptorSet> w;
		VkWriteDescriptorSetAccelerationStructureKHR wa { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR };
		wa.accelerationStructureCount = 1;
		wa.pAccelerationStructures = &I.tlas;
		for ( int i = 0; i < kDdgiBindings; i++ ) {
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
		ddgiDispatch( D, S, set, 0, ddgiPc( nullptr, 1, 0, 0, 32 ), fixedGroups );
		ddgiBarrier( D );
		ddgiDispatch( D, S, set, 3, ddgiPc( nullptr, it < iters ? 4u : 5u, 0, 0, 0 ), probeGroups );
		ddgiHostBarrier( D );   // M1FIX: the host reads the states below
		ok = submitWait( D );
		out->rays += (long long)( nProbes * 32 );
	}
	const auto t2 = std::chrono::steady_clock::now();
	out->msReloc = std::chrono::duration<double, std::milli>( t2 - t1 ).count();
	// M1FIX: the updates trace only the active probes (trace.comp mode 0 returns for Off and skipped Asleep), and the
	// states do not change during them, so count those (the twin's count), not every probe
	long long nActive = 0;
	if ( ok )
		for ( uint64_t p = 0; p < nProbes; p++ ) {
			const int st = int( static_cast<const float *>( probes.map )[p * 4 + 3] + 0.5f );
			nActive += ( st == 0 || ( st == 1 && prm.flags[1] ) ) ? 0 : 1;   // rtddgimath.h kOff / kAsleep
		}

	// the updates: trace every ray of the active probes, blend, border
	const uint64_t traceGroups = ( nProbes * N + 63 ) / 64;
	const uint64_t borderGroups = ( nProbes * 88 + 63 ) / 64;
	for ( int f = 0; ok && f < job.frames; f++ ) {
		const float * rot = job.rotations.size() >= size_t( f + 1 ) * 9 ? &job.rotations[size_t( f ) * 9] : nullptr;
		ok = begin( D );
		if ( !ok )
			break;
		ddgiDispatch( D, S, set, 0, ddgiPc( rot, 0, uint32_t( f ), 0, N ), traceGroups );
		ddgiBarrier( D );
		ddgiDispatch( D, S, set, 1, ddgiPc( rot, 2, uint32_t( f ), 0, N ), nProbes );
		ddgiBarrier( D );
		ddgiDispatch( D, S, set, 2, ddgiPc( rot, 3, uint32_t( f ), 0, 0 ), borderGroups );
		ddgiHostBarrier( D );
		ok = submitWait( D );
		out->rays += nActive * (long long)N;
	}
	out->msFrames = std::chrono::duration<double, std::milli>( std::chrono::steady_clock::now() - t2 ).count();
	if ( ok ) {
		out->probes.resize( size_t( nProbes ) * 4 );
		out->age.resize( size_t( nProbes ) );
		out->irr.resize( size_t( nProbes ) * 64 * 4 );
		out->dist.resize( size_t( nProbes ) * 256 * 2 );
		std::memcpy( out->probes.data(), probes.map, out->probes.size() * 4 );
		std::memcpy( out->age.data(), age.map, out->age.size() * 4 );
		std::memcpy( out->irr.data(), irr.map, out->irr.size() * 4 );
		std::memcpy( out->dist.data(), dist.map, out->dist.size() * 4 );
		if ( job.dumpFixed ) {   // rays[p * N + i].w, i < 32: the last trace of each probe's fixed rays
			out->fixedDist.resize( size_t( nProbes ) * 32 );
			const float * r = static_cast<const float *>( rays.map );
			for ( uint64_t p = 0; p < nProbes; p++ )
				for ( int i = 0; i < 32; i++ )
					out->fixedDist[size_t( p ) * 32 + size_t( i )] = r[( p * N + uint64_t( i ) ) * 4 + 3];
		}
	} else {
		out->why = "a DDGI submit failed (lost device or out of memory)";
	}
	if ( set )
		vk.vkFreeDescriptorSets( D.dev, D.dpool, 1, &set );
	for ( Buf * b : { &rays, &irr, &dist, &probes, &age, &attr, &lights, &params } )
		freeBuf( D, *b );
	I.release( D );
	out->ok = ok;
	return ok;
}

#endif // RTDDGI_HAVE_SPIRV

} // namespace bakert
