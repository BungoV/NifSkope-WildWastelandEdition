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
 * buffers. The kernels and their order are M1FIX's: the same dispatches, a barrier between every two.
 *
 * M3 (2026-10-29): irr, dist and probes are three ranges of one buffer, the atlas (cell_ddgi.glsl's layout), exported
 * to GL when the session is shared; a session records into its own command buffer and fence, so a step may return
 * while its submit runs; a step can scroll the volume first (the kept probes' data moved through a scratch buffer, the
 * new planes reset to DDGI_FRESH, then relocated and classified alone: job.y = 1 in trace mode 1 and reloc). */

#include "rtddgi_gpu.h"

#include <algorithm>
#include <atomic>   // RTXCASCADE2: the BLAS-build and submit counters (the CLI's census)
#include <thread>   // ddgiGpuRun waits for the device's lock (bakert.cpp does not include it)

#if defined( __has_include )
#if __has_include( "trace.spv.h" ) && __has_include( "blend.spv.h" ) && __has_include( "border.spv.h" ) && __has_include( "reloc.spv.h" )
#define RTDDGI_HAVE_SPIRV 1
#endif
#endif

namespace bakert {

void ddgiGpuCloseHandle( void * h )
{
#ifdef _WIN32
	if ( h )
		CloseHandle( static_cast<HANDLE>( h ) );
#else
	(void)h;
#endif
}

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
bool ddgiGpuStepEx( DdgiGpuSession *, const DdgiGpuStepIn &, DdgiGpuStepOut * out )
{
	*out = DdgiGpuStepOut();
	out->why = kDdgiNoKernels;
	return false;
}
bool ddgiGpuFinish( DdgiGpuSession *, DdgiGpuStepOut * out )
{
	*out = DdgiGpuStepOut();
	out->why = kDdgiNoKernels;
	return false;
}
bool ddgiGpuShare( DdgiGpuSession *, DdgiGpuShare * out )
{
	*out = DdgiGpuShare();
	out->why = kDdgiNoKernels;
	return false;
}
const float * ddgiGpuHostIrr( const DdgiGpuSession * ) { return nullptr; }
const float * ddgiGpuHostDist( const DdgiGpuSession * ) { return nullptr; }
const uint32_t * ddgiGpuHostAge( const DdgiGpuSession * ) { return nullptr; }
const float * ddgiGpuHostProbes( const DdgiGpuSession * ) { return nullptr; }
const float * ddgiGpuHostRays( const DdgiGpuSession * ) { return nullptr; }
void ddgiGpuClose( DdgiGpuSession * ) {}
bool ddgiGpuStepGroup( DdgiGpuSession * const *, const DdgiGpuStepIn *, DdgiGpuStepOut * out, int n )
{
	for ( int i = 0; i < n; i++ ) {
		out[i] = DdgiGpuStepOut();
		out[i].why = kDdgiNoKernels;
	}
	return false;
}
bool ddgiGpuSameScene( const DdgiGpuSession *, const DdgiGpuSession * ) { return false; }
long long ddgiGpuBlasBuilds() { return 0; }
long long ddgiGpuSubmits() { return 0; }

#else

namespace {
#include "trace.spv.h"
#include "blend.spv.h"
#include "border.spv.h"
#include "reloc.spv.h"

constexpr int kDdgiBindings = 15;   // ddgi_common.glsl bindings 0..14, 6 = the TLAS
struct DdgiPc { float rot0[4]; float rot1[4]; float rot2[4]; uint32_t job[4]; };
static_assert( sizeof( DdgiPc ) == 64, "push constants" );
constexpr int kDdgiStepMax = 16;     // updates a step times (ddgiGpuRun steps in chunks of this); 3 stamps each + 3
constexpr uint32_t kDdgiStamps = 3 * kDdgiStepMax + 3;
constexpr int kDdgiRelocIters = 5;   // rtddgimath.h kRelocIterations
// the atlas's three ranges, bytes a probe (rtddgi_gpu.h, cell_ddgi.glsl): irr 64 vec4, dist 256 vec2, probes 1 vec4
constexpr VkDeviceSize kAtlasIrr = 64 * 16, kAtlasDist = 256 * 8, kAtlasProbe = 16;

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

/* M3: the session's own command buffer and fence, the scroll's fills and the params update, and the Win32 export
 * (bakert's table loads none of them; vkGetDeviceProcAddr, once) */
#ifdef _WIN32
// VK_KHR_external_memory_win32 / _semaphore_win32's get-handle structs (vulkan_win32.h, which bakert.cpp does not
// include: it would pull <windows.h> in before NOMINMAX), field for field
struct DdgiMemoryWin32Get { VkStructureType sType; const void * pNext; VkDeviceMemory memory; VkExternalMemoryHandleTypeFlagBits handleType; };
struct DdgiSemaphoreWin32Get { VkStructureType sType; const void * pNext; VkSemaphore semaphore; VkExternalSemaphoreHandleTypeFlagBits handleType; };
typedef VkResult ( VKAPI_PTR * DdgiGetMemoryWin32 )( VkDevice, const DdgiMemoryWin32Get *, HANDLE * );
typedef VkResult ( VKAPI_PTR * DdgiGetSemaphoreWin32 )( VkDevice, const DdgiSemaphoreWin32Get *, HANDLE * );
constexpr VkStructureType kMemoryGetWin32 = VkStructureType( 1000073003 );     // VK_STRUCTURE_TYPE_MEMORY_GET_WIN32_HANDLE_INFO_KHR
constexpr VkStructureType kSemaphoreGetWin32 = VkStructureType( 1000078003 );  // VK_STRUCTURE_TYPE_SEMAPHORE_GET_WIN32_HANDLE_INFO_KHR
#endif

struct DdgiExtra
{
	bool tried = false;
	PFN_vkGetFenceStatus fenceStatus = nullptr;
	PFN_vkDestroyFence destroyFence = nullptr;
	PFN_vkFreeCommandBuffers freeCbs = nullptr;
	PFN_vkCmdFillBuffer fill = nullptr;
	PFN_vkCmdUpdateBuffer update = nullptr;
	PFN_vkCreateSemaphore createSem = nullptr;
	PFN_vkDestroySemaphore destroySem = nullptr;
	PFN_vkVoidFunction getMemWin32 = nullptr, getSemWin32 = nullptr;   // cast at the call (Win32 only)
};

DdgiExtra & ddgiExtra( Dev & D )
{
	static DdgiExtra X;
	if ( X.tried )
		return X;
	X.tried = true;
	if ( !vk.vkGetDeviceProcAddr )
		return X;
	X.fenceStatus = reinterpret_cast<PFN_vkGetFenceStatus>( vk.vkGetDeviceProcAddr( D.dev, "vkGetFenceStatus" ) );
	X.destroyFence = reinterpret_cast<PFN_vkDestroyFence>( vk.vkGetDeviceProcAddr( D.dev, "vkDestroyFence" ) );
	X.freeCbs = reinterpret_cast<PFN_vkFreeCommandBuffers>( vk.vkGetDeviceProcAddr( D.dev, "vkFreeCommandBuffers" ) );
	X.fill = reinterpret_cast<PFN_vkCmdFillBuffer>( vk.vkGetDeviceProcAddr( D.dev, "vkCmdFillBuffer" ) );
	X.update = reinterpret_cast<PFN_vkCmdUpdateBuffer>( vk.vkGetDeviceProcAddr( D.dev, "vkCmdUpdateBuffer" ) );
	X.createSem = reinterpret_cast<PFN_vkCreateSemaphore>( vk.vkGetDeviceProcAddr( D.dev, "vkCreateSemaphore" ) );
	X.destroySem = reinterpret_cast<PFN_vkDestroySemaphore>( vk.vkGetDeviceProcAddr( D.dev, "vkDestroySemaphore" ) );
	if ( D.share ) {
		X.getMemWin32 = vk.vkGetDeviceProcAddr( D.dev, "vkGetMemoryWin32HandleKHR" );
		X.getSemWin32 = vk.vkGetDeviceProcAddr( D.dev, "vkGetSemaphoreWin32HandleKHR" );
	}
	return X;
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

// M3: any compute or transfer write before any compute or transfer access (the scroll's copy / fill / copy chain)
void ddgiXferBarrier( Dev & D )
{
	VkMemoryBarrier mb { VK_STRUCTURE_TYPE_MEMORY_BARRIER };
	mb.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
	mb.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
	const VkPipelineStageFlags st = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT;
	vk.vkCmdPipelineBarrier( D.cb, st, st, 0, 1, &mb, 0, nullptr, 0, nullptr );
}

/* M3: the shared atlas's queue family ownership: acquired from GL (VK_QUEUE_FAMILY_EXTERNAL) at a submit's start once
 * it was released, released to it at the end (the GL side's semaphore wait names the buffer) */
void ddgiOwner( Dev & D, VkBuffer b, bool acquire )
{
	VkBufferMemoryBarrier bb { VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER };
	const VkAccessFlags all = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
	bb.srcQueueFamilyIndex = acquire ? VK_QUEUE_FAMILY_EXTERNAL : D.qf;
	bb.dstQueueFamilyIndex = acquire ? D.qf : VK_QUEUE_FAMILY_EXTERNAL;
	bb.srcAccessMask = acquire ? 0 : VkAccessFlags( VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT );
	bb.dstAccessMask = acquire ? all : 0;
	bb.buffer = b;
	bb.offset = 0;
	bb.size = VK_WHOLE_SIZE;
	const VkPipelineStageFlags work = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT;
	vk.vkCmdPipelineBarrier( D.cb, acquire ? VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT : work, acquire ? work : VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
		0, 0, nullptr, 1, &bb, 0, nullptr );
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

// M2: `bytes` from `data` into an existing buffer at `off`: straight when it is mapped, else through a staging copy
bool ddgiPut( Dev & D, const void * data, size_t bytes, Buf & dst, VkDeviceSize off = 0 )
{
	if ( !bytes )
		return true;
	if ( dst.map ) {
		std::memcpy( static_cast<char *>( dst.map ) + off, data, bytes );
		return true;
	}
	Buf st;
	if ( !makeBuf( D, bytes, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, { kHost }, st ) )
		return false;
	std::memcpy( st.map, data, bytes );
	bool ok = begin( D );
	if ( ok ) {
		VkBufferCopy c { 0, off, bytes };
		vk.vkCmdCopyBuffer( D.cb, st.b, dst.b, 1, &c );
		ddgiEndBarrier( D );
		ok = submitWait( D );
	}
	freeBuf( D, st );
	return ok;
}

void ddgiCopy( Dev & D, Buf & from, Buf & to, VkDeviceSize bytes, VkDeviceSize fromOff = 0, VkDeviceSize toOff = 0 )
{
	VkBufferCopy c { fromOff, toOff, bytes };
	vk.vkCmdCopyBuffer( D.cb, from.b, to.b, 1, &c );
}

/* M3: a device-local buffer whose memory GL can import (Win32 opaque handle, a dedicated allocation); false + why when
 * the device refuses (the caller then makes a plain one and the volume reads back) */
bool ddgiMakeShared( Dev & D, VkDeviceSize size, VkBufferUsageFlags usage, Buf & out, VkDeviceSize * alloc, std::string * why )
{
	out = Buf();
#ifdef _WIN32
	VkExternalMemoryBufferCreateInfo eb { VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_BUFFER_CREATE_INFO };
	eb.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT;
	VkBufferCreateInfo bi { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
	bi.pNext = &eb;
	bi.size = std::max<VkDeviceSize>( size, 16 );
	bi.usage = usage;
	bi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	if ( vk.vkCreateBuffer( D.dev, &bi, nullptr, &out.b ) != VK_SUCCESS ) {
		*why = "the device refused an exportable buffer";
		return false;
	}
	VkMemoryRequirements mr;
	vk.vkGetBufferMemoryRequirements( D.dev, out.b, &mr );
	const uint32_t ti = memType( D, mr.memoryTypeBits, kDevLocal );
	if ( ti == ~0u ) {
		freeBuf( D, out );
		*why = "no device-local memory type takes the exportable buffer";
		return false;
	}
	VkMemoryDedicatedAllocateInfo dd { VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO };
	dd.buffer = out.b;
	VkExportMemoryAllocateInfo ex { VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO };
	ex.pNext = &dd;
	ex.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT;
	VkMemoryAllocateInfo ai { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
	ai.pNext = &ex;
	ai.allocationSize = mr.size;
	ai.memoryTypeIndex = ti;
	if ( vk.vkAllocateMemory( D.dev, &ai, nullptr, &out.m ) != VK_SUCCESS || vk.vkBindBufferMemory( D.dev, out.b, out.m, 0 ) != VK_SUCCESS ) {
		freeBuf( D, out );
		*why = "the device refused exportable memory";
		return false;
	}
	out.size = bi.size;
	*alloc = mr.size;
	return true;
#else
	(void)D;
	(void)size;
	(void)usage;
	(void)alloc;
	*why = "GL interop is written for Win32 handles only";
	return false;
#endif
}

VkSemaphore ddgiMakeSemaphore( Dev & D, DdgiExtra & X )
{
	if ( !X.createSem )
		return VK_NULL_HANDLE;
	VkExportSemaphoreCreateInfo es { VK_STRUCTURE_TYPE_EXPORT_SEMAPHORE_CREATE_INFO };
	es.handleTypes = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_WIN32_BIT;
	VkSemaphoreCreateInfo si { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
	si.pNext = &es;
	VkSemaphore sem = VK_NULL_HANDLE;
	if ( X.createSem( D.dev, &si, nullptr, &sem ) != VK_SUCCESS )
		return VK_NULL_HANDLE;
	return sem;
}

// M3: the helpers above record into D.cb; a session's steps put its own command buffer there while they hold D.m
struct DdgiCbSwap
{
	Dev & D;
	VkCommandBuffer keep;
	DdgiCbSwap( Dev & d, VkCommandBuffer cb ) : D( d ), keep( d.cb ) { D.cb = cb; }
	~DdgiCbSwap() { D.cb = keep; }
};

/* RTXCASCADE2 (2026-10-30): what every session opened on one soup traces alike, in the soup's local frame: the BLAS +
 * TLAS and alpha masks (bakert's Scene::Impl), the triangle attributes and the lights. A session opened with
 * job.sceneFrom takes the other session's (the cell view's cascades: one soup, so one BLAS, not one a cascade); counted,
 * the last session out frees it (under D.m). The group step (ddgiGpuStepGroup) records every member into the scene's
 * own command buffer and submits once on its fence (both made on the first group step). */
struct DdgiScene
{
	Scene::Impl I;
	Buf attr, lights;
	int refs = 0;
	VkCommandBuffer cb = VK_NULL_HANDLE;
	VkFence fence = VK_NULL_HANDLE;
	bool groupSubmitted = false;   // the fence belongs to a group submit not yet seen signalled
};
std::atomic<long long> g_ddgiBlasBuilds { 0 }, g_ddgiSubmits { 0 };

} // namespace

struct DdgiGpuSession
{
	DdgiScene * sc = nullptr;                      // RTXCASCADE2: the scene, maybe shared (was a Scene::Impl of its own)
	DdgiGpuParams prm;
	Buf rays, atlas, age, params;                  // the device's working set; the atlas holds irr, dist and probes
	Buf irrRb, distRb, probesRb, ageRb, raysRb;   // the host copies
	Buf scratch, freshTpl;                         // M3: a scroll's copy through, and nP x (0, 0, 0, DDGI_FRESH)
	VkDeviceSize offIrr = 0, offDist = 0, offProbes = 0, atlasBytes = 0, atlasAlloc = 0;
	VkDescriptorSet set = VK_NULL_HANDLE;
	VkQueryPool qp = VK_NULL_HANDLE;
	VkCommandBuffer cb = VK_NULL_HANDLE;           // M3: the steps' own (a submit may run past its step)
	VkFence fence = VK_NULL_HANDLE;
	VkFence inFence = VK_NULL_HANDLE;              // RTXCASCADE2: the fence of the submit in flight (its own, or the scene's group)
	VkSemaphore vkDone = VK_NULL_HANDLE, glDone = VK_NULL_HANDLE;
	bool shared = false, released = false, relocate = true;
	std::string shareWhy;
	uint64_t nProbes = 0;
	uint32_t N = 0;
	long long nActive = 0;
	bool stepped = false, raysCopied = false;
	// the last submit of a step, until a later step (or ddgiGpuFinish) harvests it
	bool inFlight = false, inScroll = false, inHost = false, inRays = false;
	uint32_t inStamps = 0;
	int inK = 0;
};

namespace {

void ddgiFree( Dev & D, DdgiGpuSession * s )
{
	DdgiExtra & X = ddgiExtra( D );
	if ( s->inFlight && s->inFence )
		vk.vkWaitForFences( D.dev, 1, &s->inFence, VK_TRUE, UINT64_MAX );
	if ( s->set )
		vk.vkFreeDescriptorSets( D.dev, D.dpool, 1, &s->set );
	DdgiTimer & T = ddgiTimer( D );
	if ( s->qp && T.destroy )
		T.destroy( D.dev, s->qp, nullptr );
	if ( s->cb && X.freeCbs )
		X.freeCbs( D.dev, D.pool, 1, &s->cb );
	if ( s->fence && X.destroyFence )
		X.destroyFence( D.dev, s->fence, nullptr );
	for ( VkSemaphore sem : { s->vkDone, s->glDone } )
		if ( sem && X.destroySem )
			X.destroySem( D.dev, sem, nullptr );
	for ( Buf * b : { &s->rays, &s->atlas, &s->age, &s->params, &s->irrRb, &s->distRb, &s->probesRb,
			&s->ageRb, &s->raysRb, &s->scratch, &s->freshTpl } )
		freeBuf( D, *b );
	if ( DdgiScene * sc = s->sc; sc && --sc->refs <= 0 ) {   // RTXCASCADE2: the last session on the scene frees it
		if ( sc->groupSubmitted && sc->fence )
			vk.vkWaitForFences( D.dev, 1, &sc->fence, VK_TRUE, UINT64_MAX );
		if ( sc->cb && X.freeCbs )
			X.freeCbs( D.dev, D.pool, 1, &sc->cb );
		if ( sc->fence && X.destroyFence )
			X.destroyFence( D.dev, sc->fence, nullptr );
		freeBuf( D, sc->attr );
		freeBuf( D, sc->lights );
		sc->I.release( D );
		delete sc;
	}
	s->sc = nullptr;
	delete s;
}

/* RTXCASCADE2: the session's active probes skip the coarse probes a finer cascade covers (params skipLo / skipHi, grid
 * indices inclusive, skipLo[3] = 1 on; ddgi_common.glsl ddgiSkipped) */
bool ddgiSkipped( const DdgiGpuParams & P, uint64_t p )
{
	if ( !P.skipLo[3] || P.dims[0] <= 0 || P.dims[1] <= 0 )
		return false;
	const int64_t ix = int64_t( p % uint64_t( P.dims[0] ) ), iy = int64_t( ( p / uint64_t( P.dims[0] ) ) % uint64_t( P.dims[1] ) ),
		iz = int64_t( p / uint64_t( P.dims[0] * P.dims[1] ) );
	return ix >= P.skipLo[0] && ix <= P.skipHi[0] && iy >= P.skipLo[1] && iy <= P.skipHi[1] && iz >= P.skipLo[2] && iz <= P.skipHi[2];
}

// the active probes (trace.comp mode 0 skips Off and, with skipAsleep, Asleep) from the host copy of the probes
void ddgiCountActive( DdgiGpuSession * s )
{
	const float * pr = static_cast<const float *>( s->probesRb.map );
	s->nActive = 0;
	for ( uint64_t p = 0; pr && p < s->nProbes; p++ ) {
		const int st = int( pr[p * 4 + 3] + 0.5f );
		s->nActive += ( st == 0 || ( st == 1 && s->prm.flags[1] ) || ddgiSkipped( s->prm, p ) ) ? 0 : 1;   // rtddgimath.h kOff / kAsleep
	}
}

// the finished submit into `out` (its fence is signalled): timestamps, what it copied; then nothing is in flight
void ddgiHarvest( Dev & D, DdgiGpuSession * s, DdgiGpuStepOut * out )
{
	DdgiTimer & T = ddgiTimer( D );
	out->done = true;
	out->updates = s->inK;
	out->hostNew = s->inHost;
	out->probesNew = s->inScroll;
	out->probes.clear();
	for ( int c = 0; c < 5; c++ )
		out->ms[c] = -1.0;
	const uint32_t q = s->inStamps;
	if ( s->qp && T.ok && q >= 2 ) {
		std::vector<uint64_t> ts( q, 0 );
		if ( T.get( D.dev, s->qp, 0, q, ts.size() * 8, ts.data(), 8, VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT ) == VK_SUCCESS ) {
			const double ms = T.nsPerTick / 1e6;
			const uint32_t b = s->inScroll ? 1u : 0u;   // stamp 0 the start, [1] the scroll's end, then 3 an update, the copies
			if ( s->inScroll )
				out->ms[4] = double( ts[1] - ts[0] ) * ms;
			for ( int c = 0; c < 3; c++ )
				out->ms[c] = 0.0;
			for ( int f = 0; f < s->inK; f++ )
				for ( int c = 0; c < 3; c++ )
					out->ms[c] += double( ts[b + uint32_t( f ) * 3 + uint32_t( c ) + 1] - ts[b + uint32_t( f ) * 3 + uint32_t( c )] ) * ms;
			out->ms[3] = double( ts[q - 1] - ts[q - 2] ) * ms;
		}
	}
	if ( s->inHost )
		s->stepped = true;
	s->raysCopied = s->raysCopied || s->inRays;
	if ( s->inScroll ) {
		ddgiCountActive( s );
		const float * pr = static_cast<const float *>( s->probesRb.map );
		if ( pr )
			out->probes.assign( pr, pr + size_t( s->nProbes ) * 4 );
	}
	s->inFlight = false;
}

/* M3: the scroll, recorded before the updates. Each per-probe array (irr, dist, probes in the atlas; age) goes to the
 * scratch whole, is reset (zeros; the probes from the fresh template), and the kept probes come back by the runs (old
 * first probe, new first probe, count). Then the params' first probe moves and the fresh probes are relocated and
 * classified (trace mode 1 + reloc, job.y = 1: fresh only), so no DDGI_FRESH probe is left when the updates run. */
void ddgiScrollRecord( Dev & D, DdgiPipes & S, DdgiExtra & X, DdgiGpuSession * s, const DdgiGpuStepIn & in )
{
	const uint64_t nP = s->nProbes;
	struct Part
	{
		Buf * b;
		VkDeviceSize off, stride;
		bool tpl;
	};
	const Part parts[4] = { { &s->atlas, s->offIrr, kAtlasIrr, false }, { &s->atlas, s->offDist, kAtlasDist, false },
		{ &s->atlas, s->offProbes, kAtlasProbe, true }, { &s->age, 0, 4, false } };
	std::vector<VkBufferCopy> rg;
	rg.reserve( in.runs.size() / 3 + 1 );
	ddgiXferBarrier( D );
	for ( const Part & p : parts ) {
		const VkDeviceSize bytes = VkDeviceSize( nP ) * p.stride;
		ddgiCopy( D, *p.b, s->scratch, bytes, p.off, 0 );
		ddgiXferBarrier( D );
		if ( p.tpl )
			ddgiCopy( D, s->freshTpl, *p.b, bytes, 0, p.off );
		else
			X.fill( D.cb, p.b->b, p.off, bytes, 0u );
		ddgiXferBarrier( D );
		rg.clear();
		for ( size_t i = 0; i + 2 < in.runs.size(); i += 3 )
			rg.push_back( VkBufferCopy { VkDeviceSize( in.runs[i] ) * p.stride, p.off + VkDeviceSize( in.runs[i + 1] ) * p.stride,
				VkDeviceSize( in.runs[i + 2] ) * p.stride } );
		if ( !rg.empty() )
			vk.vkCmdCopyBuffer( D.cb, s->scratch.b, p.b->b, uint32_t( rg.size() ), rg.data() );
		ddgiXferBarrier( D );
	}
	for ( int k = 0; k < 3; k++ )
		s->prm.origin[k] = in.origin[k];
	X.update( D.cb, s->params.b, 0, sizeof s->prm, &s->prm );
	ddgiXferBarrier( D );
	const uint64_t fixedGroups = ( nP * 32 + 63 ) / 64, probeGroups = ( nP + 63 ) / 64;
	const int iters = s->relocate ? kDdgiRelocIters : 0;
	for ( int it = 0; it <= iters; it++ ) {   // the last pass classifies, always: no probe may stay DDGI_FRESH
		ddgiDispatch( D, S, s->set, 0, ddgiPc( nullptr, 1, 1, 0, 32 ), fixedGroups );
		ddgiBarrier( D );
		ddgiDispatch( D, S, s->set, 3, ddgiPc( nullptr, it < iters ? 4u : 5u, 1, 0, 0 ), probeGroups );
		ddgiBarrier( D );
	}
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
	DdgiExtra & X = ddgiExtra( D );
	DdgiGpuSession * s = new DdgiGpuSession;
	s->nProbes = nProbes;
	s->N = N;
	s->relocate = job.relocate;
	if ( job.sceneFrom && job.sceneFrom->sc ) {   // RTXCASCADE2: the other session's scene (the caller keeps it open until this returns)
		s->sc = job.sceneFrom->sc;
		s->sc->refs++;
		out->blasShared = true;
	} else {
		s->sc = new DdgiScene;
		s->sc->refs = 1;
		if ( !buildMasks( D, *job.bvh, s->sc->I ) || !buildRt( D, *job.bvh, s->sc->I ) ) {
			ddgiFree( D, s );
			out->why = "the soup's BLAS did not build (out of GPU memory?)";
			return nullptr;
		}
		g_ddgiBlasBuilds++;
		out->blasBuilt = true;
	}
	Scene::Impl & I = s->sc->I;
	out->blasGrow = I.inflate;   // M1FIX: the BLAS holds grown triangles when > 0; trace.comp re-tests the true ones
	out->blasGrowTris = I.inflateTris;
	s->prm = job.params;
	s->prm.counts[2] = int32_t( I.nTri );
	s->prm.counts[3] = int32_t( I.nOpaque );
	s->prm.flags[2] = int32_t( I.useMask );

	const VkBufferUsageFlags u = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	const VkBufferUsageFlags rb = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	const VkMemoryPropertyFlags cached = kHost | VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
	// M3: the atlas's ranges (offsets are multiples of 1024 bytes: every storage-buffer offset alignment is met)
	s->offIrr = 0;
	s->offDist = VkDeviceSize( nProbes ) * kAtlasIrr;
	s->offProbes = s->offDist + VkDeviceSize( nProbes ) * kAtlasDist;
	s->atlasBytes = s->offProbes + VkDeviceSize( nProbes ) * kAtlasProbe;
	bool ok = true;
	if ( job.share ) {
		if ( !D.share )
			s->shareWhy = "the bake device has no Win32 external memory / semaphore extensions";
		else if ( !X.getMemWin32 || !X.getSemWin32 || !X.createSem )
			s->shareWhy = "the export entry points did not load";
		else if ( ddgiMakeShared( D, s->atlasBytes, u, s->atlas, &s->atlasAlloc, &s->shareWhy ) ) {
			s->vkDone = ddgiMakeSemaphore( D, X );
			s->glDone = ddgiMakeSemaphore( D, X );
			s->shared = s->vkDone && s->glDone;
			if ( !s->shared )
				s->shareWhy = "the device refused an exportable semaphore";
		}
	} else {
		s->shareWhy = "not asked";
	}
	if ( !s->atlas.b ) {
		ok = makeBuf( D, s->atlasBytes, u, { kDevLocal, kHost }, s->atlas );
		s->atlasAlloc = s->atlasBytes;
	}
	std::vector<float> p0( size_t( nProbes ) * 4, 0.0f );
	for ( uint64_t p = 0; p < nProbes; p++ ) {
		for ( int k = 0; k < 3 && ( p * 3 + 2 ) < job.offsets.size(); k++ )
			p0[size_t( p ) * 4 + size_t( k )] = job.offsets[size_t( p ) * 3 + size_t( k )];
		p0[size_t( p ) * 4 + 3] = 2.0f;   // Vigilant until classified
	}
	const std::vector<float> zTiles( size_t( nProbes ) * ( 64 * 4 + 256 * 2 ), 0.0f );
	const std::vector<uint32_t> zAge( size_t( nProbes ), 0u );
	ok = ok && makeBuf( D, nProbes * N * 16, u, { kDevLocal, kHost }, s->rays )
		&& makeBuf( D, nProbes * 4, u, { kDevLocal, kHost }, s->age )
		&& makeBuf( D, nProbes * 64 * 16, rb, { cached, kHost }, s->irrRb )
		&& makeBuf( D, nProbes * 256 * 8, rb, { cached, kHost }, s->distRb )
		&& makeBuf( D, nProbes * 16, rb, { cached, kHost }, s->probesRb )
		&& makeBuf( D, nProbes * 4, rb, { cached, kHost }, s->ageRb )
		&& ( !job.dumpFixed || makeBuf( D, nProbes * N * 16, rb, { cached, kHost }, s->raysRb ) )
		&& ( out->blasShared || upload( D, job.triAttr.data(), job.triAttr.size() * 4, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, s->sc->attr ) )
		&& ( out->blasShared || upload( D, job.lights.data(), job.lights.size() * 4, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, s->sc->lights ) )
		&& upload( D, &s->prm, sizeof s->prm, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, s->params );
	ok = ok && s->irrRb.map && s->distRb.map && s->probesRb.map && s->ageRb.map && ( !job.dumpFixed || s->raysRb.map );
	ok = ok && ddgiPut( D, zTiles.data(), zTiles.size() * 4, s->atlas, s->offIrr ) && ddgiPut( D, zAge.data(), zAge.size() * 4, s->age )
		&& ddgiPut( D, p0.data(), p0.size() * 4, s->atlas, s->offProbes );
	if ( ok && job.scroll ) {   // M3: the scroll's scratch (the largest range, dist) and the fresh probes' template
		std::vector<float> fresh( size_t( nProbes ) * 4, 0.0f );
		for ( uint64_t p = 0; p < nProbes; p++ )
			fresh[size_t( p ) * 4 + 3] = 3.0f;   // ddgi_common.glsl DDGI_FRESH
		ok = makeBuf( D, nProbes * kAtlasDist, VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, { kDevLocal, kHost }, s->scratch )
			&& upload( D, fresh.data(), fresh.size() * 4, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, s->freshTpl );
	}
	if ( ok ) {   // M3: the steps' own command buffer and fence
		VkCommandBufferAllocateInfo cai { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
		cai.commandPool = D.pool;
		cai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		cai.commandBufferCount = 1;
		VkFenceCreateInfo fi { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
		ok = vk.vkAllocateCommandBuffers( D.dev, &cai, &s->cb ) == VK_SUCCESS && vk.vkCreateFence( D.dev, &fi, nullptr, &s->fence ) == VK_SUCCESS;
	}
	if ( ok ) {
		VkDescriptorSetAllocateInfo ai { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
		ai.descriptorPool = D.dpool;
		ai.descriptorSetCount = 1;
		ai.pSetLayouts = &S.dsl;
		ok = vk.vkAllocateDescriptorSets( D.dev, &ai, &s->set ) == VK_SUCCESS;
	}
	if ( ok ) {
		// bindings 1, 2, 3 are the atlas's three ranges; the rest whole buffers
		const Buf * bs[kDdgiBindings] = { &s->rays, &s->atlas, &s->atlas, &s->atlas, &s->age, &s->sc->attr, nullptr, &I.remap, &I.verts,
			&I.triOf, &I.maskTris, &I.maps, &I.alpha, &s->sc->lights, &s->params };
		const VkDeviceSize off[kDdgiBindings] = { 0, s->offIrr, s->offDist, s->offProbes, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
		const VkDeviceSize len[kDdgiBindings] = { VK_WHOLE_SIZE, VkDeviceSize( nProbes ) * kAtlasIrr, VkDeviceSize( nProbes ) * kAtlasDist,
			VkDeviceSize( nProbes ) * kAtlasProbe, VK_WHOLE_SIZE, VK_WHOLE_SIZE, VK_WHOLE_SIZE, VK_WHOLE_SIZE, VK_WHOLE_SIZE, VK_WHOLE_SIZE,
			VK_WHOLE_SIZE, VK_WHOLE_SIZE, VK_WHOLE_SIZE, VK_WHOLE_SIZE, VK_WHOLE_SIZE };
		VkDescriptorBufferInfo bi[kDdgiBindings] {};
		std::vector<VkWriteDescriptorSet> w;
		VkWriteDescriptorSetAccelerationStructureKHR wa { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR };
		wa.accelerationStructureCount = 1;
		wa.pAccelerationStructures = &I.tlas;
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
				bi[i] = { bs[i]->b, off[i], len[i] };
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

	// relocation (5 iterations) and classification from the fixed rays [J21 5, 6], every probe (job.y = 0)
	const uint64_t fixedGroups = ( nProbes * 32 + 63 ) / 64, probeGroups = ( nProbes + 63 ) / 64;
	const int iters = job.relocate ? kDdgiRelocIters : 0;
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
	// the states (and offsets) to the host once: the updates do not change them (a scroll copies them again)
	if ( ok ) {
		ok = begin( D );
		if ( ok ) {
			ddgiCopyBarrier( D );
			ddgiCopy( D, s->atlas, s->probesRb, nProbes * kAtlasProbe, s->offProbes, 0 );
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
	ddgiCountActive( s );
	const float * pr = static_cast<const float *>( s->probesRb.map );
	out->probes.assign( pr, pr + size_t( nProbes ) * 4 );
	out->ok = true;
	return s;
}

namespace {

/* RTXCASCADE2: a step's parts, shared by the one-session step (ddgiGpuStepEx, M3's) and the group step: ready = the
 * session's last submit harvested if done (false: busy, or a scroll without its scratch); record = its commands into
 * D.cb (begun); submitted = the session's in-flight state once the submit went in */
bool ddgiStepReady( Dev & D, DdgiExtra & X, DdgiGpuSession * s, const DdgiGpuStepIn & in, DdgiGpuStepOut * out )
{
	if ( s->inFlight ) {   // M3: the last submit first (never waited on here: a step that would wait says busy)
		const VkResult fs = X.fenceStatus ? X.fenceStatus( D.dev, s->inFence ) : vk.vkWaitForFences( D.dev, 1, &s->inFence, VK_TRUE, 0 );
		if ( fs != VK_SUCCESS ) {
			out->busy = true;
			out->pending = true;
			out->why = "the last update set still runs on the device";
			return false;
		}
		ddgiHarvest( D, s, out );
	}
	if ( in.scroll && ( !s->scratch.b || !X.fill || !X.update ) ) {
		out->why = "a scroll needs the session's scratch (job.scroll) and vkCmdFillBuffer / vkCmdUpdateBuffer";
		return false;
	}
	if ( in.skipSet && !X.update ) {
		bool same = true;
		for ( int c = 0; c < 4; c++ )
			same = same && s->prm.skipLo[c] == in.skipLo[c] && s->prm.skipHi[c] == in.skipHi[c];
		if ( !same ) {
			out->why = "a skip range needs vkCmdUpdateBuffer";
			return false;
		}
	}
	return true;
}

uint32_t ddgiStepRecord( Dev & D, DdgiPipes & S, DdgiExtra & X, DdgiTimer & T, DdgiGpuSession * s, const DdgiGpuStepIn & in, int k,
	bool copyRays, bool gl )
{
	const uint64_t nProbes = s->nProbes;
	const uint32_t N = s->N;
	const uint64_t traceGroups = ( nProbes * N + 63 ) / 64;
	const uint64_t borderGroups = ( nProbes * 88 + 63 ) / 64;
	const bool timed = s->qp != VK_NULL_HANDLE;
	const bool scroll = in.scroll;
	uint32_t q = 0;
	auto stamp = [&]() {
		if ( timed )
			T.write( D.cb, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, s->qp, q++ );
	};
	if ( timed )
		T.reset( D.cb, s->qp, 0, kDdgiStamps );
	if ( s->released )
		ddgiOwner( D, s->atlas.b, true );
	stamp();
	if ( in.skipSet ) {   // RTXCASCADE2: the covered coarse probes the updates skip, written only when the range moved
		bool same = true;
		for ( int c = 0; c < 4; c++ )
			same = same && s->prm.skipLo[c] == in.skipLo[c] && s->prm.skipHi[c] == in.skipHi[c];
		if ( !same ) {
			for ( int c = 0; c < 4; c++ ) {
				s->prm.skipLo[c] = in.skipLo[c];
				s->prm.skipHi[c] = in.skipHi[c];
			}
			ddgiXferBarrier( D );
			X.update( D.cb, s->params.b, 0, sizeof s->prm, &s->prm );
			ddgiXferBarrier( D );
			ddgiCountActive( s );
		}
	}
	if ( scroll ) {
		ddgiScrollRecord( D, S, X, s, in );
		stamp();
	}
	// the updates: trace every ray of the active probes, blend, border (M1FIX's order, one barrier between each two)
	for ( int f = 0; f < k; f++ ) {
		const float * rot = in.rotations ? in.rotations + size_t( f ) * 9 : nullptr;
		const uint32_t fr = in.frame0 + uint32_t( f );
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
	if ( in.host ) {
		ddgiCopy( D, s->atlas, s->irrRb, nProbes * kAtlasIrr, s->offIrr, 0 );
		ddgiCopy( D, s->atlas, s->distRb, nProbes * kAtlasDist, s->offDist, 0 );
		ddgiCopy( D, s->age, s->ageRb, nProbes * 4 );
	}
	if ( scroll )   // the fresh probes' states for the CPU (the census, the debug view, the active count)
		ddgiCopy( D, s->atlas, s->probesRb, nProbes * kAtlasProbe, s->offProbes, 0 );
	if ( copyRays )
		ddgiCopy( D, s->rays, s->raysRb, nProbes * N * 16 );
	stamp();
	ddgiEndBarrier( D );
	if ( gl )
		ddgiOwner( D, s->atlas.b, false );
	return q;
}

void ddgiStepSubmitted( DdgiGpuSession * s, VkFence f, uint32_t q, int k, const DdgiGpuStepIn & in, bool copyRays, bool gl,
	DdgiGpuStepOut * out )
{
	s->inFlight = true;
	s->inFence = f;
	s->inStamps = q;
	s->inK = k;
	s->inScroll = in.scroll;
	s->inHost = in.host;
	s->inRays = copyRays;
	s->released = gl;
	out->submitted = true;
	out->rays = s->nActive * (long long)s->N * k;   // a scroll's new actives count from its harvest on
}

} // namespace

bool ddgiGpuStepEx( DdgiGpuSession * s, const DdgiGpuStepIn & in, DdgiGpuStepOut * out )
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
	DdgiExtra & X = ddgiExtra( D );
	const auto t0 = std::chrono::steady_clock::now();
	if ( !ddgiStepReady( D, X, s, in, out ) )
		return false;
	DdgiPipes & S = ddgiPipes( D );
	DdgiTimer & T = ddgiTimer( D );
	const int k = std::max( 0, std::min( in.k, kDdgiStepMax ) );
	const bool copyRays = in.copyRays && s->raysRb.b;
	// interop in use: a shared atlas AND a GL side that orders itself (beforeSubmit); a shared session whose GL import
	// failed reads back like an unshared one: no vkDone signal nobody would wait on, no release to GL
	const bool gl = s->shared && bool( in.beforeSubmit );
	uint32_t q = 0;
	bool ok = true;
	{
		DdgiCbSwap sw( D, s->cb );
		ok = begin( D );
		if ( ok ) {
			q = ddgiStepRecord( D, S, X, T, s, in, k, copyRays, gl );
			ok = vk.vkEndCommandBuffer( D.cb ) == VK_SUCCESS;
		}
		if ( ok ) {
			const bool waitGl = gl ? in.beforeSubmit() : false;
			const VkPipelineStageFlags ws = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT;
			VkSubmitInfo si { VK_STRUCTURE_TYPE_SUBMIT_INFO };
			si.waitSemaphoreCount = waitGl ? 1 : 0;
			si.pWaitSemaphores = &s->glDone;
			si.pWaitDstStageMask = &ws;
			si.commandBufferCount = 1;
			si.pCommandBuffers = &s->cb;
			si.signalSemaphoreCount = gl ? 1 : 0;
			si.pSignalSemaphores = &s->vkDone;
			vk.vkResetFences( D.dev, 1, &s->fence );
			ok = vk.vkQueueSubmit( D.q, 1, &si, s->fence ) == VK_SUCCESS;
		}
	}
	if ( !ok ) {
		out->why = "a DDGI submit failed (lost device or out of memory)";
		return false;
	}
	g_ddgiSubmits++;
	ddgiStepSubmitted( s, s->fence, q, k, in, copyRays, gl, out );
	if ( in.wait ) {
		if ( vk.vkWaitForFences( D.dev, 1, &s->fence, VK_TRUE, UINT64_MAX ) != VK_SUCCESS ) {
			out->why = "a DDGI submit failed (lost device or out of memory)";
			return false;
		}
		ddgiHarvest( D, s, out );
	} else {
		out->pending = true;
	}
	out->msWall = std::chrono::duration<double, std::milli>( std::chrono::steady_clock::now() - t0 ).count();
	out->ok = true;
	return true;
}

/* RTXCASCADE2: one submit for several sessions on ONE scene (the cell view's cascades): every member's step recorded in
 * order into the scene's command buffer (each with its own timestamps, scroll, updates, copies, ownership), then one
 * vkQueueSubmit that waits on every member's glDone its beforeSubmit signalled and signals every interop member's vkDone,
 * on the scene's fence. A member not ready (its last submit still running) makes the whole group busy (nothing recorded);
 * members harvested before that keep their reports in out[]. */
bool ddgiGpuStepGroup( DdgiGpuSession * const * ss, const DdgiGpuStepIn * ins, DdgiGpuStepOut * outs, int n )
{
	for ( int i = 0; i < n; i++ )
		outs[i] = DdgiGpuStepOut();
	if ( n <= 0 )
		return false;
	for ( int i = 0; i < n; i++ ) {
		if ( !ss[i] || !ss[i]->sc || ss[i]->sc != ss[0]->sc ) {
			for ( int j = 0; j < n; j++ )
				outs[j].why = ss[i] ? "the sessions do not share one scene (no group submit)" : "no session";
			return false;
		}
	}
	Dev & D = dev();
	std::unique_lock<std::mutex> lk( D.m, std::try_to_lock );
	if ( !lk.owns_lock() ) {
		for ( int i = 0; i < n; i++ ) {
			outs[i].busy = true;
			outs[i].why = "the bake device is busy";
		}
		return false;
	}
	DdgiExtra & X = ddgiExtra( D );
	const auto t0 = std::chrono::steady_clock::now();
	DdgiScene & G = *ss[0]->sc;
	auto allBusy = [&]( const char * why, bool pending ) {
		for ( int j = 0; j < n; j++ ) {
			outs[j].busy = true;
			outs[j].pending = outs[j].pending || pending;
			outs[j].why = why;
		}
		return false;
	};
	if ( G.groupSubmitted ) {   // its command buffer is recorded again only once that submit is done
		const VkResult fs = X.fenceStatus ? X.fenceStatus( D.dev, G.fence ) : vk.vkWaitForFences( D.dev, 1, &G.fence, VK_TRUE, 0 );
		if ( fs != VK_SUCCESS )
			return allBusy( "the last group submit still runs on the device", true );
	}
	for ( int i = 0; i < n; i++ ) {
		if ( !ddgiStepReady( D, X, ss[i], ins[i], &outs[i] ) ) {
			const bool busy = outs[i].busy;
			const std::string why = outs[i].why;
			for ( int j = 0; j < n; j++ ) {
				outs[j].busy = busy;
				outs[j].why = why;
			}
			return false;
		}
	}
	if ( !G.cb ) {
		VkCommandBufferAllocateInfo cai { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
		cai.commandPool = D.pool;
		cai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		cai.commandBufferCount = 1;
		VkFenceCreateInfo fi { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
		if ( vk.vkAllocateCommandBuffers( D.dev, &cai, &G.cb ) != VK_SUCCESS || vk.vkCreateFence( D.dev, &fi, nullptr, &G.fence ) != VK_SUCCESS ) {
			for ( int j = 0; j < n; j++ )
				outs[j].why = "no command buffer or fence for the group submit";
			return false;
		}
	}
	DdgiPipes & S = ddgiPipes( D );
	DdgiTimer & T = ddgiTimer( D );
	std::vector<int> ks( size_t( n ), 0 );
	std::vector<uint32_t> qs( size_t( n ), 0 );
	std::vector<char> rays( size_t( n ), 0 ), gls( size_t( n ), 0 );
	bool ok = true, wait = false;
	{
		DdgiCbSwap sw( D, G.cb );
		ok = begin( D );
		for ( int i = 0; ok && i < n; i++ ) {
			ks[size_t( i )] = std::max( 0, std::min( ins[i].k, kDdgiStepMax ) );
			rays[size_t( i )] = ins[i].copyRays && ss[i]->raysRb.b ? 1 : 0;
			gls[size_t( i )] = ss[i]->shared && bool( ins[i].beforeSubmit ) ? 1 : 0;
			qs[size_t( i )] = ddgiStepRecord( D, S, X, T, ss[i], ins[i], ks[size_t( i )], rays[size_t( i )] != 0, gls[size_t( i )] != 0 );
			wait = wait || ins[i].wait;
		}
		ok = ok && vk.vkEndCommandBuffer( D.cb ) == VK_SUCCESS;
		if ( ok ) {
			std::vector<VkSemaphore> waits, sigs;
			for ( int i = 0; i < n; i++ ) {
				if ( !gls[size_t( i )] )
					continue;
				if ( ins[i].beforeSubmit() )
					waits.push_back( ss[i]->glDone );
				sigs.push_back( ss[i]->vkDone );
			}
			const std::vector<VkPipelineStageFlags> ws( waits.size(), VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT );
			VkSubmitInfo si { VK_STRUCTURE_TYPE_SUBMIT_INFO };
			si.waitSemaphoreCount = uint32_t( waits.size() );
			si.pWaitSemaphores = waits.data();
			si.pWaitDstStageMask = ws.data();
			si.commandBufferCount = 1;
			si.pCommandBuffers = &G.cb;
			si.signalSemaphoreCount = uint32_t( sigs.size() );
			si.pSignalSemaphores = sigs.data();
			vk.vkResetFences( D.dev, 1, &G.fence );
			ok = vk.vkQueueSubmit( D.q, 1, &si, G.fence ) == VK_SUCCESS;
		}
	}
	if ( !ok ) {
		for ( int j = 0; j < n; j++ )
			outs[j].why = "a DDGI group submit failed (lost device or out of memory)";
		return false;
	}
	g_ddgiSubmits++;
	G.groupSubmitted = true;
	for ( int i = 0; i < n; i++ )
		ddgiStepSubmitted( ss[i], G.fence, qs[size_t( i )], ks[size_t( i )], ins[i], rays[size_t( i )] != 0, gls[size_t( i )] != 0, &outs[i] );
	if ( wait ) {
		if ( vk.vkWaitForFences( D.dev, 1, &G.fence, VK_TRUE, UINT64_MAX ) != VK_SUCCESS ) {
			for ( int j = 0; j < n; j++ )
				outs[j].why = "a DDGI group submit failed (lost device or out of memory)";
			return false;
		}
		for ( int i = 0; i < n; i++ )
			ddgiHarvest( D, ss[i], &outs[i] );
	} else {
		for ( int i = 0; i < n; i++ )
			outs[i].pending = true;
	}
	const double wall = std::chrono::duration<double, std::milli>( std::chrono::steady_clock::now() - t0 ).count();
	for ( int i = 0; i < n; i++ ) {
		outs[i].msWall = wall;
		outs[i].ok = true;
	}
	return true;
}

bool ddgiGpuSameScene( const DdgiGpuSession * a, const DdgiGpuSession * b )
{
	return a && b && a->sc && a->sc == b->sc;
}

long long ddgiGpuBlasBuilds() { return g_ddgiBlasBuilds.load(); }
long long ddgiGpuSubmits() { return g_ddgiSubmits.load(); }

bool ddgiGpuStep( DdgiGpuSession * s, const float * rotations, int k, uint32_t frame0, bool copyRays, DdgiGpuStepOut * out )
{
	DdgiGpuStepIn in;
	in.rotations = rotations;
	in.k = k;
	in.frame0 = frame0;
	in.copyRays = copyRays;
	in.host = true;
	in.wait = true;
	return ddgiGpuStepEx( s, in, out );
}

bool ddgiGpuFinish( DdgiGpuSession * s, DdgiGpuStepOut * out )
{
	*out = DdgiGpuStepOut();
	if ( !s ) {
		out->why = "no session";
		return false;
	}
	Dev & D = dev();
	std::lock_guard<std::mutex> lk( D.m );
	if ( s->inFlight ) {
		if ( vk.vkWaitForFences( D.dev, 1, &s->inFence, VK_TRUE, UINT64_MAX ) != VK_SUCCESS ) {
			out->why = "a DDGI submit failed (lost device or out of memory)";
			return false;
		}
		ddgiHarvest( D, s, out );
	}
	out->ok = true;
	return true;
}

bool ddgiGpuShare( DdgiGpuSession * s, DdgiGpuShare * out )
{
	*out = DdgiGpuShare();
	if ( !s ) {
		out->why = "no session";
		return false;
	}
	if ( !s->shared ) {
		out->why = s->shareWhy;
		return false;
	}
#ifdef _WIN32
	Dev & D = dev();
	DdgiExtra & X = ddgiExtra( D );
	const auto getMem = reinterpret_cast<DdgiGetMemoryWin32>( X.getMemWin32 );
	const auto getSem = reinterpret_cast<DdgiGetSemaphoreWin32>( X.getSemWin32 );
	const DdgiMemoryWin32Get mi { kMemoryGetWin32, nullptr, s->atlas.m, VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT };
	const DdgiSemaphoreWin32Get si1 { kSemaphoreGetWin32, nullptr, s->vkDone, VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_WIN32_BIT };
	const DdgiSemaphoreWin32Get si2 { kSemaphoreGetWin32, nullptr, s->glDone, VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_WIN32_BIT };
	HANDLE hm = nullptr, h1 = nullptr, h2 = nullptr;
	if ( getMem( D.dev, &mi, &hm ) != VK_SUCCESS || getSem( D.dev, &si1, &h1 ) != VK_SUCCESS || getSem( D.dev, &si2, &h2 ) != VK_SUCCESS ) {
		ddgiGpuCloseHandle( hm );
		ddgiGpuCloseHandle( h1 );
		ddgiGpuCloseHandle( h2 );
		out->why = "the device gave no Win32 handle for the atlas or a semaphore";
		return false;
	}
	out->memory = hm;
	out->vkDone = h1;
	out->glDone = h2;
	out->memorySize = uint64_t( s->atlasAlloc );
	out->atlasBytes = uint64_t( s->atlasBytes );
	out->dedicated = true;
	std::memcpy( out->deviceUuid, D.deviceUuid, 16 );
	std::memcpy( out->driverUuid, D.driverUuid, 16 );
	out->ok = true;
	return true;
#else
	out->why = "GL interop is written for Win32 handles only";
	return false;
#endif
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
