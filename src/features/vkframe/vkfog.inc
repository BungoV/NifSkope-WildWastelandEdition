// Lane VKFAR2 (2026-11-14, the froxel item): the air's integrate on the present device. Included by vkpresent.cpp after
// vkskybody.inc (it uses the shared targets (sharedFind), the worker (wk, hand, Work::fn), exportSem, the pipeline cache
// (tone().cache) and drain). The API and the row: vkfog.h.
#include "features/vkframe/vkfog.h"
#include "features/vkframe/fog_integ_cs.spv.h"
#include "features/vkframe/fog_integ32_cs.spv.h"

namespace
{
struct FogPc
{
	std::int32_t grid[4];  // volair_integ_cs.hlsl's FogJob: columns, rows, base layer, layers
	std::int32_t opt[4];   // x: the red bits
};
static_assert( sizeof( FogPc ) == 32, "volair_integ_cs.hlsl's FogJob" );

struct Fog
{
	bool made = false, failed = false, broken = false, pending = false;
	std::string failWhy, brokenWhy;
	VkDescriptorSetLayout dsl = VK_NULL_HANDLE;
	VkPipelineLayout layout = VK_NULL_HANDLE;
	VkPipeline pipe[2] = {};  // the RGBA16F volume, the f32 red's RGBA32F one
	VkDescriptorPool dpool = VK_NULL_HANDLE;
	VkDescriptorSet set = VK_NULL_HANDLE;
	VkCommandBuffer cmd = VK_NULL_HANDLE;
	VkFence fence = VK_NULL_HANDLE;
	VkSemaphore glDone = VK_NULL_HANDLE, vkDone = VK_NULL_HANDLE;
	GLuint glDoneGl = 0, vkDoneGl = 0;
	void * semCtx = nullptr;
	std::uintptr_t setIn = 0, setOut = 0;  // the keys the set holds, and the shared generation it was written at
	std::uint64_t setGen = 0;
	int pipelines = 0;
	double buildMs = 0.0;
	// what the worker records (written on the drawing thread before hand(), read after)
	int pipeK = 0, nJobs = 0;
	FogPc job[2] = {};
	VkImage outImg = VK_NULL_HANDLE;
	WwVkPresent::FogStat st;
};

Fog & fogS()
{
	static Fog F;
	return F;
}

// once per device: the set (t0 the inject as a sampled image, u0 the integrated volume as a storage image), a 32-byte push
// block and the two kernels
bool fogStatic( std::string & why )
{
	Vk & v = vk();
	Fog & F = fogS();
	if ( F.made )
		return true;
	if ( F.failed ) {
		why = F.failWhy;
		return false;
	}
	auto fail = [&]( const std::string & w ) {
		F.failed = true;
		F.failWhy = why = w;
		return false;
	};
	const double t0 = nowMs();
	VkDescriptorSetLayoutBinding b[2] = {};
	for ( std::uint32_t i = 0; i < 2; i++ ) {
		b[i].binding = i;
		b[i].descriptorType = i == 0 ? VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE : VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		b[i].descriptorCount = 1;
		b[i].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	}
	VkDescriptorSetLayoutCreateInfo lci { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
	lci.bindingCount = 2;
	lci.pBindings = b;
	if ( v.vkCreateDescriptorSetLayout( v.dev, &lci, nullptr, &F.dsl ) != VK_SUCCESS )
		return fail( "the integrate's descriptor set layout failed" );
	const VkPushConstantRange pr { VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof( FogPc ) };
	VkPipelineLayoutCreateInfo plci { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
	plci.setLayoutCount = 1;
	plci.pSetLayouts = &F.dsl;
	plci.pushConstantRangeCount = 1;
	plci.pPushConstantRanges = &pr;
	if ( v.vkCreatePipelineLayout( v.dev, &plci, nullptr, &F.layout ) != VK_SUCCESS )
		return fail( "the integrate's pipeline layout failed" );
	const VkDescriptorPoolSize ps[2] = { { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1 }, { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1 } };
	VkDescriptorPoolCreateInfo pci { VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
	pci.maxSets = 1;
	pci.poolSizeCount = 2;
	pci.pPoolSizes = ps;
	if ( v.vkCreateDescriptorPool( v.dev, &pci, nullptr, &F.dpool ) != VK_SUCCESS )
		return fail( "the integrate's descriptor pool failed" );
	VkDescriptorSetAllocateInfo dai { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
	dai.descriptorPool = F.dpool;
	dai.descriptorSetCount = 1;
	dai.pSetLayouts = &F.dsl;
	VkFenceCreateInfo fi { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
	VkCommandBufferAllocateInfo cai { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
	cai.commandPool = v.pool;
	cai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	cai.commandBufferCount = 1;
	if ( v.vkAllocateDescriptorSets( v.dev, &dai, &F.set ) != VK_SUCCESS || v.vkCreateFence( v.dev, &fi, nullptr, &F.fence ) != VK_SUCCESS
		|| v.vkAllocateCommandBuffers( v.dev, &cai, &F.cmd ) != VK_SUCCESS )
		return fail( "the integrate's set / fence / command buffer failed" );
	struct Code { const std::uint32_t * p; size_t bytes; };
	const Code code[2] = { { vkframe_fog_integ_cs_spv, sizeof( vkframe_fog_integ_cs_spv ) },
		{ vkframe_fog_integ32_cs_spv, sizeof( vkframe_fog_integ32_cs_spv ) } };
	for ( int k = 0; k < 2; k++ ) {
		VkShaderModuleCreateInfo mci { VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
		mci.codeSize = code[k].bytes;
		mci.pCode = code[k].p;
		VkShaderModule m = VK_NULL_HANDLE;
		if ( v.vkCreateShaderModule( v.dev, &mci, nullptr, &m ) != VK_SUCCESS )
			return fail( "the integrate's shader module (" + std::to_string( k ) + ") failed" );
		VkComputePipelineCreateInfo cp { VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO };
		cp.stage = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
		cp.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
		cp.stage.module = m;
		cp.stage.pName = "main";
		cp.layout = F.layout;
		const VkResult r = v.vkCreateComputePipelines( v.dev, tone().cache, 1, &cp, nullptr, &F.pipe[k] );
		v.vkDestroyShaderModule( v.dev, m, nullptr );
		if ( r != VK_SUCCESS )
			return fail( "vkCreateComputePipelines (the integrate, " + std::to_string( k ) + ") returned " + std::to_string( int( r ) ) );
		F.pipelines++;
	}
	F.buildMs = nowMs() - t0;
	F.made = true;
	toneCacheSave();
	return true;
}

void fogSemsDrop( bool glAlive )
{
	Vk & v = vk();
	Fog & F = fogS();
	if ( glAlive )
		for ( GLuint * s : { &F.glDoneGl, &F.vkDoneGl } )
			if ( *s )
				gl().deleteSemaphores( 1, s );
	for ( VkSemaphore * s : { &F.glDone, &F.vkDone } )
		if ( *s )
			v.vkDestroySemaphore( v.dev, *s, nullptr );
	F.glDoneGl = F.vkDoneGl = 0;
	F.glDone = F.vkDone = VK_NULL_HANDLE;
	F.semCtx = nullptr;
}

bool fogSems( std::string & why )
{
	Fog & F = fogS();
	if ( F.glDone && F.semCtx == gl().ctx )
		return true;
	fogSemsDrop( false );
	if ( !exportSem( F.glDone, F.glDoneGl, why ) || !exportSem( F.vkDone, F.vkDoneGl, why ) ) {
		fogSemsDrop( true );
		return false;
	}
	F.semCtx = gl().ctx;
	return true;
}

// the worker: one dispatch a grid (8 x 8 columns a group), then the volume made visible to every later reader
bool fogWork( std::string & why )
{
	Vk & v = vk();
	Fog & F = fogS();
	v.vkResetFences( v.dev, 1, &F.fence );
	v.vkResetCommandBuffer( F.cmd, 0 );
	VkCommandBufferBeginInfo bi { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
	bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	v.vkBeginCommandBuffer( F.cmd, &bi );
	v.vkCmdBindPipeline( F.cmd, VK_PIPELINE_BIND_POINT_COMPUTE, F.pipe[F.pipeK] );
	v.vkCmdBindDescriptorSets( F.cmd, VK_PIPELINE_BIND_POINT_COMPUTE, F.layout, 0, 1, &F.set, 0, nullptr );
	for ( int k = 0; k < F.nJobs; k++ ) {  // the two grids write apart layers: no barrier between them
		v.vkCmdPushConstants( F.cmd, F.layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof( FogPc ), &F.job[k] );
		v.vkCmdDispatch( F.cmd, std::uint32_t( ( F.job[k].grid[0] + 7 ) / 8 ), std::uint32_t( ( F.job[k].grid[1] + 7 ) / 8 ), 1 );
	}
	VkMemoryBarrier mb { VK_STRUCTURE_TYPE_MEMORY_BARRIER };
	mb.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
	mb.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_READ_BIT
		| VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	v.vkCmdPipelineBarrier( F.cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 1, &mb, 0, nullptr, 0,
		nullptr );
	if ( v.vkEndCommandBuffer( F.cmd ) != VK_SUCCESS ) {
		why = "vkEndCommandBuffer (the integrate) failed";
		return false;
	}
	const VkPipelineStageFlags waitAt = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
	VkSubmitInfo si { VK_STRUCTURE_TYPE_SUBMIT_INFO };
	si.waitSemaphoreCount = 1;
	si.pWaitSemaphores = &F.glDone;
	si.pWaitDstStageMask = &waitAt;
	si.commandBufferCount = 1;
	si.pCommandBuffers = &F.cmd;
	si.signalSemaphoreCount = 1;
	si.pSignalSemaphores = &F.vkDone;
	const VkResult r = v.vkQueueSubmit( v.queue, 1, &si, F.fence );
	if ( r != VK_SUCCESS ) {
		why = "vkQueueSubmit (the integrate) returned " + std::to_string( int( r ) );
		return false;
	}
	return true;
}

// shutdown (before the shared targets go): the semaphores, the set's pool, the pipelines
void fogDrop()
{
	Vk & v = vk();
	Fog & F = fogS();
	if ( !v.dev || ( !F.made && !F.dsl && !F.glDone ) )
		return;
	drain();
	v.vkDeviceWaitIdle( v.dev );
	fogSemsDrop( gl().ok && F.semCtx == gl().ctx );
	if ( F.cmd )
		v.vkFreeCommandBuffers( v.dev, v.pool, 1, &F.cmd );
	if ( F.fence )
		v.vkDestroyFence( v.dev, F.fence, nullptr );
	if ( F.dpool )
		v.vkDestroyDescriptorPool( v.dev, F.dpool, nullptr );
	for ( VkPipeline & p : F.pipe )
		if ( p )
			v.vkDestroyPipeline( v.dev, p, nullptr );
	if ( F.layout )
		v.vkDestroyPipelineLayout( v.dev, F.layout, nullptr );
	if ( F.dsl )
		v.vkDestroyDescriptorSetLayout( v.dev, F.dsl, nullptr );
	const WwVkPresent::FogStat keep = F.st;
	F = Fog();
	F.st = keep;
}
}  // namespace

namespace WwVkPresent
{
bool fogIntegRun( const FogIntegRun & t, std::string & why )
{
	Fog & F = fogS();
	Gl & g = gl();
	Vk & v = vk();
	const double t0 = nowMs();
	auto refuse = [&]( const std::string & w ) {
		why = w;
		F.st.fails++;
		F.st.failWhy = w;
		return false;
	};
	auto breakOff = [&]( const std::string & w ) {
		F.broken = true;
		F.brokenWhy = "the integrate on Vulkan broke: " + w;
		return refuse( F.brokenWhy );
	};
	if ( F.broken )
		return refuse( F.brokenWhy );
	if ( !v.ok || !g.ok || g.ctx != reinterpret_cast<void *>( wglGetCurrentContext() ) )
		return refuse( "the present device or GL's entry points are not ready in this context" );
	if ( !t.inject || !t.integ || !sharedTargetIs( t.inject, t.injectTex ) || !sharedTargetIs( t.integ, t.integTex ) )
		return refuse( "the inject or the integrated volume is not a shared target" );
	const Shared * in = sharedFind( t.inject );
	const Shared * out = sharedFind( t.integ );
	if ( !in || !out || in->type != 1 || out->type != 1 || in->w != out->w || in->h != out->h || in->d != out->d )
		return refuse( "the two volumes are not 3D targets of one shape" );
	if ( in->glFmt != kRgba32f || out->glFmt != GLenum( t.f32 ? kRgba32f : kRgba16f ) )
		return refuse( "a volume's format (inject " + std::to_string( in->glFmt ) + ", integrated " + std::to_string( out->glFmt ) + ")" );
	VkImageFormatProperties ip {};
	if ( v.vkGetPhysicalDeviceImageFormatProperties( v.phys, out->fmt, VK_IMAGE_TYPE_3D, VK_IMAGE_TILING_OPTIMAL,
			VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, 0, &ip ) != VK_SUCCESS )
		return refuse( "the integrated volume's format takes no storage use on the device" );
	int nJobs = 0;
	FogPc job[2] = {};
	for ( const FogGrid & G : t.grid ) {
		if ( G.layers <= 0 )
			continue;
		if ( G.cols < 1 || G.rows < 1 || G.cols > out->w || G.rows > out->h || G.base < 0 || G.base + G.layers > out->d )
			return refuse( "a grid outside the volume" );
		job[nJobs] = FogPc { { G.cols, G.rows, G.base, G.layers }, { t.red, 0, 0, 0 } };
		nJobs++;
	}
	if ( !nJobs )
		return true;  // no layers asked: nothing to write
	// the last run is done before its set and command buffer are touched again
	if ( F.pending ) {
		v.vkWaitForFences( v.dev, 1, &F.fence, VK_TRUE, 1000000000ull );
		F.pending = false;
	}
	std::string w;
	if ( !fogStatic( w ) || !fogSems( w ) )
		return breakOff( w );
	if ( F.setIn != t.inject || F.setOut != t.integ || F.setGen != sharedGen() ) {
		const VkDescriptorImageInfo ii[2] = { { VK_NULL_HANDLE, in->view, VK_IMAGE_LAYOUT_GENERAL },
			{ VK_NULL_HANDLE, out->view, VK_IMAGE_LAYOUT_GENERAL } };
		VkWriteDescriptorSet wr[2] = {};
		for ( std::uint32_t i = 0; i < 2; i++ ) {
			wr[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			wr[i].dstSet = F.set;
			wr[i].dstBinding = i;
			wr[i].descriptorCount = 1;
			wr[i].descriptorType = i == 0 ? VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE : VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
			wr[i].pImageInfo = &ii[i];
		}
		v.vkUpdateDescriptorSets( v.dev, 2, wr, 0, nullptr );
		F.setIn = t.inject;
		F.setOut = t.integ;
		F.setGen = sharedGen();
	}
	F.pipeK = t.f32 ? 1 : 0;
	F.nJobs = nJobs;
	F.job[0] = job[0];
	F.job[1] = job[1];
	// GL's inject drawn -> the worker's dispatches -> GL's readers (the surfaces, the sky's air, the next frame's draws)
	GLuint tex[2] = { GLuint( t.injectTex ), GLuint( t.integTex ) };
	GLenum lay[2] = { kLayoutGeneral, kLayoutGeneral };
	g.signalSemaphore( F.glDoneGl, 0, nullptr, 2, tex, lay );
	g.flush();
	Worker & W = wk();
	std::uint64_t want = 0;
	{
		std::lock_guard<std::mutex> lk( W.m );
		want = W.toneDone + 1;
	}
	Work j;
	j.k = -1;
	j.fn = fogWork;
	hand( j );
	bool ok = false;
	{
		std::unique_lock<std::mutex> lk( W.m );
		W.idle.wait( lk, [&W, want]() { return W.toneDone >= want; } );
		ok = W.toneOk;
		w = W.toneWhy;
	}
	if ( !ok )
		return breakOff( w );  // nothing signals vkDone: GL does not wait it; GL draws its integrate this frame
	g.waitSemaphore( F.vkDoneGl, 0, nullptr, 2, tex, lay );
	F.pending = true;
	const double ms = nowMs() - t0;
	F.st.runs++;
	F.st.dispatches += nJobs;
	F.st.cpuMs += ms;
	F.st.maxMs = std::max( F.st.maxMs, ms );
	return true;
}

void fogOnGl( const char * why )
{
	Fog & F = fogS();
	F.st.gl++;
	F.st.glWhy = why ? why : "";
}

FogStat takeFogStat()
{
	Fog & F = fogS();
	FogStat s = F.st;
	s.pipelines = F.pipelines;
	s.buildMs = F.buildMs;
	if ( F.broken && s.failWhy.empty() )
		s.failWhy = F.brokenWhy;
	F.st = FogStat();
	return s;
}
}  // namespace WwVkPresent
