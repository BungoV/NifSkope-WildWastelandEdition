/* Lane VKFAR22 (2026-11-16): the reflection probes' G-buffer capture on the present device (vkreflcapture.h). Included once
 * by vkpresent.cpp at its end, after vkreflrelight.inc (farlod::TwinSpec / WaterDraw / twinStatic / waterSampler). GL's pass
 * (cellreflprobe.cpp): per job, layer 6 * slot + face of rpAlb (RGBA8) and rpNd (RGBA16F) with a 64 x 64 D24 depth, cleared
 * (colour 0, depth 1), depth LEQUAL with writes, no cull, no blend, viewport 64 x 64, every caster drawn from client arrays.
 * Here: its own render pass (two colour attachments + depth, CLEAR / STORE, GENERAL), 2D views of the job's layer + a
 * framebuffer per layer (made again when a shared target is made or goes: sharedGen), a pipeline per set of absent streams
 * (an absent stream reads GL's current value at stride 0), a descriptor set and a VS / PS slice per draw, every job of a run
 * in one crossing (one fence, j.fn on the worker, as the relight). */
#define WW_VK_REFLCAPTURE 1
#include "features/vkframe/vkreflcapture.h"
#include "features/vkframe/reflcap_view_vs.spv.h"  // vkframe_spirv.sh's fxt / fxg lines
#include "features/vkframe/reflcap_view_ps.spv.h"

namespace
{
namespace reflcapture
{
using namespace WwVkPresent;

const farlod::WaterImgName kImg[] = { { 64, "BaseMap" } };
// a draw's host slice: its VS block (b287) at 0, its PS block (b31) at 256
constexpr VkDeviceSize kVsAt = 0, kPsAt = 256, kSliceBytes = 512;
static_assert( kT_reflcapVsBytes <= 256 && kT_reflcapPsBytes <= 256, "the capture's host slice" );
const farlod::TwinSpec kSpec { "reflection capture", vkframe_reflcap_view_vs_spv, sizeof( vkframe_reflcap_view_vs_spv ),
	vkframe_reflcap_view_ps_spv, sizeof( vkframe_reflcap_view_ps_spv ), kImg, sizeof( kImg ) / sizeof( kImg[0] ), nullptr, 0,
	kT_reflcapPsBytes, kPsAt, kSliceBytes, false, 0, false };
// the route's own keys (never an address the app's shared targets use): the depth, and a twin slot per BaseMap GL name
char kDepthKey[1];
constexpr int kBaseSlots = 256;
char kBaseKeys[kBaseSlots];
constexpr VkFormat kAlbFmt = VK_FORMAT_R8G8B8A8_UNORM, kNdFmt = VK_FORMAT_R16G16B16A16_SFLOAT, kDepthFmt = VK_FORMAT_X8_D24_UNORM_PACK32;

struct LayerFb
{
	VkImageView view[3] = { VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE };  // albedo, normal + depth, the depth
	VkFramebuffer fb = VK_NULL_HANDLE;
};

// a recorded draw: its pipeline, its streams' offsets in the ring (pos, col, nrm, uv), its triangles
struct Rec
{
	VkPipeline pipe = VK_NULL_HANDLE;
	VkDeviceSize vb[4] = {}, ib = 0;
	std::uint32_t count = 0;
};

struct Rc
{
	bool made = false, broken = false;
	std::string brokenWhy;
	farlod::WaterDraw D;  // twinStatic's layout / modules / binds; D.smp holds the samplers
	VkRenderPass pass = VK_NULL_HANDLE;
	std::map<int, VkPipeline> pipes;  // absent-stream bits (1 col, 2 nrm, 4 uv) + 8 depth ALWAYS (red)
	VkSemaphore glDone = VK_NULL_HANDLE, vkDone = VK_NULL_HANDLE;
	GLuint glDoneGl = 0, vkDoneGl = 0;
	void * semCtx = nullptr;
	VkCommandBuffer cmd = VK_NULL_HANDLE;
	VkFence fence = VK_NULL_HANDLE;
	bool pending = false;
	VkDescriptorPool pool = VK_NULL_HANDLE;
	std::vector<VkDescriptorSet> sets;
	CsmBuf ring, host;  // the client arrays + triangles; the per-draw blocks
	std::map<int, LayerFb> fbs;
	std::uintptr_t fbAlb = 0, fbNd = 0;
	std::uint64_t fbGen = 0;
	unsigned depthTex = 0;
	std::map<unsigned, int> baseSlot;  // BaseMap GL name -> kBaseKeys slot
	// the crossing's record (written before the hand)
	std::vector<ReflCaptureJob> jobs;
	std::vector<Rec> recs;
	ReflCaptureStat st;
};

Rc & rcS()
{
	static Rc s;
	return s;
}

// the device-time parts (once): the render pass, the command buffer and its fence
bool rcStatic( std::string & why )
{
	Vk & v = vk();
	Rc & S = rcS();
	if ( S.made )
		return true;
	VkAttachmentDescription at[3] {};
	const VkFormat fmt[3] = { kAlbFmt, kNdFmt, kDepthFmt };
	for ( int i = 0; i < 3; i++ ) {
		at[i].format = fmt[i];
		at[i].samples = VK_SAMPLE_COUNT_1_BIT;
		at[i].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;  // GL's glClear: colour 0, depth 1
		at[i].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		at[i].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		at[i].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		at[i].initialLayout = at[i].finalLayout = VK_IMAGE_LAYOUT_GENERAL;
	}
	const VkAttachmentReference cr[2] = { { 0, VK_IMAGE_LAYOUT_GENERAL }, { 1, VK_IMAGE_LAYOUT_GENERAL } };
	const VkAttachmentReference dr { 2, VK_IMAGE_LAYOUT_GENERAL };
	VkSubpassDescription sp {};
	sp.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	sp.colorAttachmentCount = 2;
	sp.pColorAttachments = cr;
	sp.pDepthStencilAttachment = &dr;
	const VkPipelineStageFlags cs = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT
		| VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
	const VkAccessFlags rw = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
		| VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	const VkAccessFlags wo = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	const VkSubpassDependency dep[2] = { { VK_SUBPASS_EXTERNAL, 0, cs, cs, wo, rw, 0 }, { 0, VK_SUBPASS_EXTERNAL, cs, cs, wo, rw, 0 } };
	VkRenderPassCreateInfo rci { VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO };
	rci.attachmentCount = 3;
	rci.pAttachments = at;
	rci.subpassCount = 1;
	rci.pSubpasses = &sp;
	rci.dependencyCount = 2;
	rci.pDependencies = dep;
	if ( v.vkCreateRenderPass( v.dev, &rci, nullptr, &S.pass ) != VK_SUCCESS ) {
		why = "vkCreateRenderPass (the reflection capture) failed";
		return false;
	}
	VkFenceCreateInfo fi { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
	VkCommandBufferAllocateInfo cai { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
	cai.commandPool = v.pool;
	cai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	cai.commandBufferCount = 1;
	if ( v.vkCreateFence( v.dev, &fi, nullptr, &S.fence ) != VK_SUCCESS || v.vkAllocateCommandBuffers( v.dev, &cai, &S.cmd ) != VK_SUCCESS ) {
		why = "the reflection capture's fence / command buffer failed";
		return false;
	}
	S.made = true;
	return true;
}

/* a pipeline per set of absent streams: locations 0 position, 1 colour, 2 normal, 3 uv (TEXCOORD7: dxc numbers the inputs
 * in order), one binding each, an absent one at stride 0 (GL's current value); depth LEQUAL with writes, no cull / blend */
bool rcPipe( int key, VkPipeline & out, std::string & why )
{
	Vk & v = vk();
	Rc & S = rcS();
	if ( auto it = S.pipes.find( key ); it != S.pipes.end() ) {
		out = it->second;
		return true;
	}
	const double t0 = nowMs();
	VkPipelineShaderStageCreateInfo st[2] { { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO },
		{ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO } };
	st[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
	st[0].module = S.D.vs;
	st[0].pName = "main";
	st[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	st[1].module = S.D.ps;
	st[1].pName = "main";
	const std::uint32_t stride[4] = { 12, ( key & 1 ) ? 0u : 16u, ( key & 2 ) ? 0u : 12u, ( key & 4 ) ? 0u : 8u };
	const VkFormat vf[4] = { VK_FORMAT_R32G32B32_SFLOAT, VK_FORMAT_R32G32B32A32_SFLOAT, VK_FORMAT_R32G32B32_SFLOAT, VK_FORMAT_R32G32_SFLOAT };
	VkVertexInputBindingDescription vb[4];
	VkVertexInputAttributeDescription va[4];
	for ( std::uint32_t i = 0; i < 4; i++ ) {
		vb[i] = { i, stride[i], VK_VERTEX_INPUT_RATE_VERTEX };
		va[i] = { i, i, vf[i], 0 };
	}
	VkPipelineVertexInputStateCreateInfo vi { VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
	vi.vertexBindingDescriptionCount = 4;
	vi.pVertexBindingDescriptions = vb;
	vi.vertexAttributeDescriptionCount = 4;
	vi.pVertexAttributeDescriptions = va;
	VkPipelineInputAssemblyStateCreateInfo ia { VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
	ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	VkPipelineViewportDepthClipControlCreateInfoEXT cc { VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_DEPTH_CLIP_CONTROL_CREATE_INFO_EXT };
	cc.negativeOneToOne = VK_TRUE;  // GL's clip z in [-w, w]
	VkPipelineViewportStateCreateInfo vps { VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO };
	vps.pNext = &cc;
	vps.viewportCount = vps.scissorCount = 1;
	VkPipelineRasterizationStateCreateInfo rs { VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
	rs.polygonMode = VK_POLYGON_MODE_FILL;
	rs.cullMode = VK_CULL_MODE_NONE;
	rs.frontFace = VK_FRONT_FACE_CLOCKWISE;
	rs.lineWidth = 1.0f;
	VkPipelineMultisampleStateCreateInfo ms { VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
	ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	VkPipelineDepthStencilStateCreateInfo ds { VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };
	ds.depthTestEnable = VK_TRUE;
	ds.depthWriteEnable = VK_TRUE;
	ds.depthCompareOp = ( key & 8 ) ? VK_COMPARE_OP_ALWAYS : VK_COMPARE_OP_LESS_OR_EQUAL;
	VkPipelineColorBlendAttachmentState cb[2] {};
	for ( VkPipelineColorBlendAttachmentState & c : cb )
		c.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	VkPipelineColorBlendStateCreateInfo cbs { VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
	cbs.attachmentCount = 2;
	cbs.pAttachments = cb;
	const VkDynamicState dyn[2] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
	VkPipelineDynamicStateCreateInfo dy { VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
	dy.dynamicStateCount = 2;
	dy.pDynamicStates = dyn;
	VkGraphicsPipelineCreateInfo gp { VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
	gp.stageCount = 2;
	gp.pStages = st;
	gp.pVertexInputState = &vi;
	gp.pInputAssemblyState = &ia;
	gp.pViewportState = &vps;
	gp.pRasterizationState = &rs;
	gp.pMultisampleState = &ms;
	gp.pDepthStencilState = &ds;
	gp.pColorBlendState = &cbs;
	gp.pDynamicState = &dy;
	gp.layout = S.D.layout;
	gp.renderPass = S.pass;
	if ( v.vkCreateGraphicsPipelines( v.dev, tone().cache, 1, &gp, nullptr, &out ) != VK_SUCCESS ) {
		why = "vkCreateGraphicsPipelines (the reflection capture) failed";
		return false;
	}
	S.pipes[key] = out;
	S.st.pipelines++;
	S.st.buildMs += nowMs() - t0;
	toneCacheSave();
	return true;
}

bool rcSems( std::string & why )
{
	Vk & v = vk();
	Gl & g = gl();
	Rc & S = rcS();
	if ( S.glDone && S.semCtx == g.ctx )
		return true;
	for ( VkSemaphore * s : { &S.glDone, &S.vkDone } )  // another context's names: that context went
		if ( *s )
			v.vkDestroySemaphore( v.dev, *s, nullptr );
	S.glDone = S.vkDone = VK_NULL_HANDLE;
	S.glDoneGl = S.vkDoneGl = 0;
	S.semCtx = nullptr;
	if ( !exportSem( S.glDone, S.glDoneGl, why ) || !exportSem( S.vkDone, S.vkDoneGl, why ) )
		return false;
	S.semCtx = g.ctx;
	return true;
}

// n descriptor sets of the twins' layout (the pool made again, larger, when a run needs more; the fence is done by then)
bool rcSets( std::size_t n, std::string & why )
{
	Vk & v = vk();
	Rc & S = rcS();
	if ( S.sets.size() >= n )
		return true;
	const std::size_t cap = std::max<std::size_t>( n, S.sets.size() * 2 );
	if ( S.pool )
		v.vkDestroyDescriptorPool( v.dev, S.pool, nullptr );
	S.pool = VK_NULL_HANDLE;
	S.sets.clear();
	std::uint32_t cnt[4] = {};
	for ( const FarSetSlot & b : S.D.binds )
		cnt[b.kind]++;
	static const VkDescriptorType kType[4] = { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
		VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, VK_DESCRIPTOR_TYPE_SAMPLER };
	std::vector<VkDescriptorPoolSize> ps;
	for ( int k = 0; k < 4; k++ )
		if ( cnt[k] )
			ps.push_back( { kType[k], cnt[k] * std::uint32_t( cap ) } );
	VkDescriptorPoolCreateInfo pi { VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
	pi.maxSets = std::uint32_t( cap );
	pi.poolSizeCount = std::uint32_t( ps.size() );
	pi.pPoolSizes = ps.data();
	if ( v.vkCreateDescriptorPool( v.dev, &pi, nullptr, &S.pool ) != VK_SUCCESS ) {
		why = "the reflection capture's descriptor pool failed";
		return false;
	}
	std::vector<VkDescriptorSetLayout> l( cap, S.D.dsl );
	VkDescriptorSetAllocateInfo ai { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
	ai.descriptorPool = S.pool;
	ai.descriptorSetCount = std::uint32_t( cap );
	ai.pSetLayouts = l.data();
	S.sets.resize( cap );
	if ( v.vkAllocateDescriptorSets( v.dev, &ai, S.sets.data() ) != VK_SUCCESS ) {
		S.sets.clear();
		why = "the reflection capture's descriptor sets failed";
		return false;
	}
	return true;
}

void rcFbDrop()
{
	Vk & v = vk();
	Rc & S = rcS();
	for ( auto & e : S.fbs ) {
		if ( e.second.fb )
			v.vkDestroyFramebuffer( v.dev, e.second.fb, nullptr );
		for ( VkImageView vw : e.second.view )
			if ( vw )
				v.vkDestroyImageView( v.dev, vw, nullptr );
	}
	S.fbs.clear();
}

// a layer's 2D views of the shared cube arrays (+ the depth) and their framebuffer (kept until a key or a shared target changes)
bool rcFb( std::uintptr_t albKey, std::uintptr_t ndKey, int layer, VkFramebuffer & out, std::string & why )
{
	Vk & v = vk();
	Rc & S = rcS();
	if ( S.fbAlb != albKey || S.fbNd != ndKey || S.fbGen != sharedGen() ) {
		rcFbDrop();
		S.fbAlb = albKey;
		S.fbNd = ndKey;
		S.fbGen = sharedGen();
	}
	if ( auto it = S.fbs.find( layer ); it != S.fbs.end() ) {
		out = it->second.fb;
		return true;
	}
	const Shared * sh[3] = { sharedFind( albKey ), sharedFind( ndKey ), sharedFind( reinterpret_cast<std::uintptr_t>( &kDepthKey[0] ) ) };
	const VkFormat fmt[3] = { kAlbFmt, kNdFmt, kDepthFmt };
	LayerFb L;
	for ( int i = 0; i < 3; i++ ) {
		if ( !sh[i] || !sh[i]->img || sh[i]->fmt != fmt[i] || sh[i]->w != 64 || sh[i]->h != 64 ) {
			why = "the capture's targets are not the shared 64 x 64 G-buffer cube arrays and depth";
			return false;
		}
		VkImageViewCreateInfo ci { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
		ci.image = sh[i]->img;
		ci.viewType = VK_IMAGE_VIEW_TYPE_2D;
		ci.format = fmt[i];
		ci.subresourceRange = { i == 2 ? VkImageAspectFlags( VK_IMAGE_ASPECT_DEPTH_BIT ) : VkImageAspectFlags( VK_IMAGE_ASPECT_COLOR_BIT ), 0, 1,
			i == 2 ? 0u : std::uint32_t( layer ), 1 };
		if ( v.vkCreateImageView( v.dev, &ci, nullptr, &L.view[i] ) != VK_SUCCESS ) {
			for ( VkImageView vw : L.view )
				if ( vw )
					v.vkDestroyImageView( v.dev, vw, nullptr );
			why = "a G-buffer layer's view failed";
			return false;
		}
	}
	VkFramebufferCreateInfo fci { VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
	fci.renderPass = S.pass;
	fci.attachmentCount = 3;
	fci.pAttachments = L.view;
	fci.width = fci.height = 64;
	fci.layers = 1;
	if ( v.vkCreateFramebuffer( v.dev, &fci, nullptr, &L.fb ) != VK_SUCCESS ) {
		for ( VkImageView vw : L.view )
			v.vkDestroyImageView( v.dev, vw, nullptr );
		why = "a G-buffer layer's framebuffer failed";
		return false;
	}
	S.fbs[layer] = L;
	out = L.fb;
	return true;
}

// the worker: a render pass a job (cleared), a draw a caster (its set: its slices, its BaseMap), GL's signal waited, signalled back
bool rcWork( std::string & why )
{
	Vk & v = vk();
	Rc & S = rcS();
	v.vkResetFences( v.dev, 1, &S.fence );
	v.vkResetCommandBuffer( S.cmd, 0 );
	VkCommandBufferBeginInfo bi { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
	bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	v.vkBeginCommandBuffer( S.cmd, &bi );
	VkClearValue clr[3] {};
	clr[2].depthStencil = { 1.0f, 0 };
	// no y flip: GL's window rows from the bottom are the image's rows from the top
	const VkViewport vp { 0.0f, 0.0f, 64.0f, 64.0f, 0.0f, 1.0f };
	const VkRect2D sc { { 0, 0 }, { 64, 64 } };
	const VkBuffer bufs[4] = { S.ring.buf, S.ring.buf, S.ring.buf, S.ring.buf };
	for ( const ReflCaptureJob & j : S.jobs ) {
		VkRenderPassBeginInfo rbi { VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
		rbi.renderPass = S.pass;
		rbi.framebuffer = S.fbs[j.layer].fb;
		rbi.renderArea = sc;
		rbi.clearValueCount = 3;
		rbi.pClearValues = clr;
		v.vkCmdBeginRenderPass( S.cmd, &rbi, VK_SUBPASS_CONTENTS_INLINE );
		v.vkCmdSetViewport( S.cmd, 0, 1, &vp );
		v.vkCmdSetScissor( S.cmd, 0, 1, &sc );
		for ( int d = j.first; d < j.first + j.count; d++ ) {
			const Rec & e = S.recs[std::size_t( d )];
			v.vkCmdBindPipeline( S.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, e.pipe );
			v.vkCmdBindVertexBuffers( S.cmd, 0, 4, bufs, e.vb );
			v.vkCmdBindIndexBuffer( S.cmd, S.ring.buf, e.ib, VK_INDEX_TYPE_UINT16 );
			v.vkCmdBindDescriptorSets( S.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, S.D.layout, 0, 1, &S.sets[std::size_t( d )], 0, nullptr );
			v.vkCmdDrawIndexed( S.cmd, e.count, 1, 0, 0, 0 );
		}
		v.vkCmdEndRenderPass( S.cmd );
	}
	if ( v.vkEndCommandBuffer( S.cmd ) != VK_SUCCESS ) {
		why = "vkEndCommandBuffer (the reflection capture) failed";
		return false;
	}
	const VkPipelineStageFlags waitAt = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
	VkSubmitInfo si { VK_STRUCTURE_TYPE_SUBMIT_INFO };
	si.waitSemaphoreCount = 1;
	si.pWaitSemaphores = &S.glDone;
	si.pWaitDstStageMask = &waitAt;
	si.commandBufferCount = 1;
	si.pCommandBuffers = &S.cmd;
	si.signalSemaphoreCount = 1;
	si.pSignalSemaphores = &S.vkDone;
	const VkResult r = v.vkQueueSubmit( v.queue, 1, &si, S.fence );
	if ( r != VK_SUCCESS ) {
		why = "vkQueueSubmit (the reflection capture) returned " + std::to_string( int( r ) );
		return false;
	}
	return true;
}

int capFieldAt( const FxField * t, int n, const char * name )
{
	for ( int i = 0; i < n; i++ )
		if ( std::strcmp( t[i].name, name ) == 0 )
			return t[i].offset;
	return -1;
}
}  // namespace reflcapture
}  // namespace

namespace WwVkPresent
{
std::string reflCaptureRefusal()
{
	reflcapture::Rc & S = reflcapture::rcS();
	if ( S.broken )
		return S.brokenWhy;
	if ( !vk().ok || !gl().ok || gl().ctx != reinterpret_cast<void *>( wglGetCurrentContext() ) )
		return "the present device or GL's entry points are not ready in this context";
	if ( !vk().clipControl )
		return "the present device has no depth clip control (GL's clip z in [-w, w])";
	return std::string();
}

unsigned reflCaptureDepthTex()
{
	return reflcapture::rcS().depthTex;
}

bool reflCaptureRun( const ReflCaptureRun & r, std::string & why )
{
	using namespace reflcapture;
	Rc & S = rcS();
	Gl & g = gl();
	Vk & v = vk();
	Fx & F = fx();
	const double t0 = nowMs();
	auto refuse = [&]( const std::string & w ) {
		why = w;
		S.st.fails++;
		S.st.failWhy = w;
		return false;
	};
	auto breakOff = [&]( const std::string & w ) {
		S.broken = true;
		S.brokenWhy = "the reflection capture on Vulkan broke: " + w;
		return refuse( S.brokenWhy );
	};
	if ( const std::string no = reflCaptureRefusal(); !no.empty() )
		return refuse( no );
	if ( r.wwDefs != reflCaptureKey() )  // the twin is GL's define-free build: any other key stays on GL
		return refuse( "GL's capture program has the key \"" + r.wwDefs + "\", the twin's key is \"\"" );
	if ( r.jobs.empty() )
		return true;
	// the G-buffer cube arrays are drawn in place: never copied
	const Shared * shA = sharedFind( r.albKey );
	const Shared * shN = sharedFind( r.ndKey );
	if ( !shA || !shA->tex || !shN || !shN->tex || shA->fmt != kAlbFmt || shN->fmt != kNdFmt )
		return refuse( "rpAlb / rpNd: not the shared G-buffer cube arrays" );
	for ( const ReflCaptureJob & j : r.jobs )
		if ( j.layer < 0 || j.layer >= shA->d || j.layer >= shN->d || j.first < 0 || j.count < 0
			|| std::size_t( j.first + j.count ) > r.draws.size() )
			return refuse( "a job's layer or draws lie outside the G-buffer / the run" );
	for ( const ReflCaptureDraw & d : r.draws )
		if ( !d.pos || !d.idx || d.verts <= 0 || d.indices <= 0 || ( d.indices % 3 ) )
			return refuse( "a caster without positions or triangles" );
	const int nV = int( sizeof( kT_reflcapVs ) / sizeof( kT_reflcapVs[0] ) );
	const int nP = int( sizeof( kT_reflcapPs ) / sizeof( kT_reflcapPs[0] ) );
	const int atClip = capFieldAt( kT_reflcapVs, nV, "rpClipFromRel" ), atUvOff = capFieldAt( kT_reflcapVs, nV, "uvOffset" );
	const int atAt = capFieldAt( kT_reflcapPs, nP, "rpAlphaTest" );
	if ( atClip < 0 || atUvOff < 0 || atAt < 0 )
		return breakOff( "the capture tables have no rpClipFromRel / uvOffset / rpAlphaTest" );
	std::string w;
	if ( !fxStatic( w ) || !rcStatic( w ) || !rcSems( w ) || !farlod::twinStatic( S.D, kSpec, w ) )
		return breakOff( w );
	for ( const FarSetSlot & b : S.D.binds )
		if ( !( ( b.kind == 0 && ( b.binding == 31 || b.binding == 287 ) ) || ( b.kind == 1 && b.binding == 64 )
				|| ( b.kind == 3 && b.binding == 128 ) ) )
			return breakOff( "the twins declare binding " + std::to_string( b.binding ) + " the capture does not fill" );
	// the last crossing is done before its command buffer, sets, slices, ring and framebuffers are written again
	if ( std::string fw; !fenceDone( S.fence, S.pending, "reflcapture", fw ) )
		return refuse( fw );
	S.pending = false;
	unsigned dTex = 0;
	if ( !sharedTarget( reinterpret_cast<std::uintptr_t>( &kDepthKey[0] ), 0, 64, 64, 1, 0x81A6, dTex, w ) )  // GL_DEPTH_COMPONENT24
		return breakOff( "the capture's depth: " + w );
	S.depthTex = dTex;
	const std::size_t nd = r.draws.size();
	// the ring: every draw's streams (an absent one: 16 bytes of GL's current value) and triangles, 16-byte aligned
	auto up16 = []( VkDeviceSize x ) { return ( x + 15 ) & ~VkDeviceSize( 15 ); };
	VkDeviceSize ringBytes = 0;
	for ( const ReflCaptureDraw & d : r.draws ) {
		const VkDeviceSize vn = VkDeviceSize( d.verts );
		const bool col = d.col && !( r.red & 16 );
		ringBytes += up16( vn * 12 ) + ( col ? up16( vn * 16 ) : 16 ) + ( d.nrm ? up16( vn * 12 ) : 16 ) + ( d.uv ? up16( vn * 8 ) : 16 )
			+ up16( VkDeviceSize( d.indices ) * 2 );
	}
	if ( !rcSets( nd, w ) || !csmBuf( S.ring, std::max<VkDeviceSize>( ringBytes, 4096 ), false,
			 VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, w, "the reflection capture's ring" )
		|| !csmBuf( S.host, kSliceBytes * VkDeviceSize( nd ), false, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, w, "the reflection capture's slices" ) )
		return breakOff( w );
	std::vector<GLuint> texNames { shA->tex, shN->tex, dTex };
	S.jobs = r.jobs;
	if ( r.red & 1 )
		S.jobs[0].layer = std::min( S.jobs[0].layer + 1, std::min( shA->d, shN->d ) - 1 );
	for ( const ReflCaptureJob & j : S.jobs ) {
		VkFramebuffer fb = VK_NULL_HANDLE;
		if ( !rcFb( r.albKey, r.ndKey, j.layer, fb, w ) )
			return breakOff( w );
	}
	if ( S.baseSlot.size() + nd > std::size_t( kBaseSlots ) )
		S.baseSlot.clear();  // the slots are reused (every twin is copied again each run): never a refusal for good
	S.recs.assign( nd, Rec() );
	std::uint8_t * ring = static_cast<std::uint8_t *>( S.ring.ptr );
	std::uint8_t * host = static_cast<std::uint8_t *>( S.host.ptr );
	std::vector<VkDescriptorImageInfo> ii( nd * 2 );
	std::vector<VkDescriptorBufferInfo> bi( nd * 2 );
	std::vector<VkWriteDescriptorSet> ws;
	std::map<unsigned, VkImageView> baseViews;  // this run's BaseMap views by GL name (each twin copied once a run)
	int copies = 0, inPlace = 0;
	VkDeviceSize at = 0;
	for ( std::size_t d = 0; d < nd; d++ ) {
		const ReflCaptureDraw & c = r.draws[d];
		Rec & e = S.recs[d];
		const VkDeviceSize vn = VkDeviceSize( c.verts );
		// the streams: present ones copied, absent ones GL's current value (red 16: colour (1, 1, 1, 1))
		static const float kOne[4] = { 1, 1, 1, 1 };
		const float * src[4] = { c.pos, ( r.red & 16 ) ? nullptr : c.col, c.nrm, c.uv };
		const float * cur[4] = { nullptr, ( r.red & 16 ) ? kOne : c.cur[0], c.cur[1], c.cur[2] };
		const VkDeviceSize per[4] = { 12, 16, 12, 8 };
		int key = 0;
		for ( int s = 0; s < 4; s++ ) {
			e.vb[s] = at;
			if ( src[s] ) {
				std::memcpy( ring + at, src[s], std::size_t( vn * per[s] ) );
				at += up16( vn * per[s] );
			} else {
				std::memcpy( ring + at, cur[s], 16 );
				at += 16;
				key |= 1 << ( s - 1 );
			}
		}
		e.ib = at;
		std::memcpy( ring + at, c.idx, std::size_t( c.indices ) * 2 );
		at += up16( VkDeviceSize( c.indices ) * 2 );
		e.count = std::uint32_t( c.indices );
		if ( r.red & 32 )
			key |= 8;
		if ( !rcPipe( key, e.pipe, w ) )
			return breakOff( w );
		// the blocks as GL's program holds them (+ the reds)
		std::uint8_t * h = host + kSliceBytes * VkDeviceSize( d );
		std::memcpy( h + kVsAt, c.vs, sizeof( c.vs ) );
		std::memcpy( h + kPsAt, c.ps, sizeof( c.ps ) );
		if ( r.red & 2 )
			std::memset( h + kVsAt + atClip + 4 * 14, 0, 4 );  // rpClipFromRel's B (column 3, z)
		if ( r.red & 4 )
			std::memset( h + kVsAt + atUvOff, 0, 8 );
		if ( r.red & 8 )
			std::memset( h + kPsAt + atAt, 0, 4 );
		// BaseMap: none (the zero 2D view), a shared image read in place, else its twin copied once a run
		const WaterSlot & b = c.base;
		VkImageView view = F.d2;
		if ( b.tex ) {
			if ( b.kind != 0 )
				return refuse( "BaseMap: not a 2D texture" );
			if ( b.baseLevel != 0 )
				return refuse( "BaseMap: a base level above 0" );
			if ( b.tex == shA->tex || b.tex == shN->tex )
				return refuse( "BaseMap: samples the G-buffer it draws into" );
			if ( auto it = baseViews.find( b.tex ); it != baseViews.end() ) {
				view = it->second;
			} else if ( const std::uintptr_t sk = farSharedOf( b.tex ) ) {
				view = sharedFind( sk )->view;
				texNames.push_back( b.tex );
				inPlace++;
			} else {
				auto slot = S.baseSlot.find( b.tex );
				if ( slot == S.baseSlot.end() ) {
					if ( int( S.baseSlot.size() ) >= kBaseSlots )
						return refuse( "BaseMap: more than 256 caster textures" );
					slot = S.baseSlot.emplace( b.tex, int( S.baseSlot.size() ) ).first;
				}
				const std::uintptr_t tk = reinterpret_cast<std::uintptr_t>( &kBaseKeys[slot->second] );
				unsigned twin = 0;
				if ( !waterTwin( tk, b, twin, w ) )
					return refuse( "BaseMap: " + w );
				const bool viaFar = waterBcFormat( b.glFmt );
				const Shared * th = viaFar ? farTwinFind( tk ) : sharedFind( tk );
				if ( !th || !th->view )
					return refuse( "BaseMap: its twin has no view" );
				view = th->view;
				for ( int l = 0; l < std::max( b.levels, 1 ); l++ )
					g.copyImageSubData( b.tex, 0x0DE1, l, 0, 0, 0, twin, 0x0DE1, l, 0, 0, 0, std::max( b.w >> l, 1 ), std::max( b.h >> l, 1 ), 1 );
				copies++;
				texNames.push_back( twin );
			}
			baseViews[b.tex] = view;
		}
		WaterSlot plain;
		VkSampler sm = farlod::waterSampler( S.D, b.tex ? b : plain, false, !b.tex, w );
		if ( !sm )
			return breakOff( w );
		ii[d * 2] = { VK_NULL_HANDLE, view, VK_IMAGE_LAYOUT_GENERAL };
		ii[d * 2 + 1] = { sm, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL };
		bi[d * 2] = { S.host.buf, kSliceBytes * VkDeviceSize( d ) + kVsAt, VkDeviceSize( kT_reflcapVsBytes ) };
		bi[d * 2 + 1] = { S.host.buf, kSliceBytes * VkDeviceSize( d ) + kPsAt, VkDeviceSize( kT_reflcapPsBytes ) };
		for ( const FarSetSlot & sb : S.D.binds ) {
			VkWriteDescriptorSet x { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
			x.dstSet = S.sets[d];
			x.dstBinding = sb.binding;
			x.descriptorCount = 1;
			if ( sb.kind == 0 ) {
				x.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
				x.pBufferInfo = &bi[d * 2 + ( sb.binding == 287 ? 0 : 1 )];
			} else if ( sb.kind == 1 ) {
				x.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
				x.pImageInfo = &ii[d * 2];
			} else {
				x.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
				x.pImageInfo = &ii[d * 2 + 1];
			}
			ws.push_back( x );
		}
	}
	v.vkUpdateDescriptorSets( v.dev, std::uint32_t( ws.size() ), ws.data(), 0, nullptr );
	// GL's work so far signalled (the targets, the inputs, the copies; each name once) -> the worker's job -> GL waits
	std::sort( texNames.begin(), texNames.end() );
	texNames.erase( std::unique( texNames.begin(), texNames.end() ), texNames.end() );
	std::vector<GLenum> lay( texNames.size(), kLayoutGeneral );
	g.signalSemaphore( S.glDoneGl, 0, nullptr, GLuint( texNames.size() ), texNames.data(), lay.data() );
	g.flush();
	Worker & W = wk();
	std::uint64_t want = 0;
	{
		std::lock_guard<std::mutex> lk( W.m );
		want = W.toneDone + 1;
	}
	Work j;
	j.k = -1;
	j.fn = rcWork;
	hand( j );
	bool ok = false;
	{
		std::unique_lock<std::mutex> lk( W.m );
		W.idle.wait( lk, [&W, want]() { return W.toneDone >= want; } );
		ok = W.toneOk;
		w = W.toneWhy;
	}
	if ( !ok )
		return breakOff( w );  // nothing signals vkDone: GL does not wait it; GL draws the faces
	g.waitSemaphore( S.vkDoneGl, 0, nullptr, GLuint( texNames.size() ), texNames.data(), lay.data() );
	S.pending = true;
	const double ms = nowMs() - t0;
	S.st.crossings++;
	S.st.copies += copies;
	S.st.inPlace += inPlace;
	S.st.runs++;
	S.st.faces += int( r.jobs.size() );
	S.st.draws += int( nd );
	S.st.cpuMs += ms;
	S.st.maxMs = std::max( S.st.maxMs, ms );
	return true;
}

void reflCaptureOnGl( const char * why )
{
	reflcapture::Rc & S = reflcapture::rcS();
	S.st.gl++;
	S.st.glWhy = why ? why : "";
}

ReflCaptureStat takeReflCaptureStat()
{
	reflcapture::Rc & S = reflcapture::rcS();
	ReflCaptureStat s = S.st;
	if ( S.broken && s.failWhy.empty() )
		s.failWhy = S.brokenWhy;
	const int pipes = S.st.pipelines;
	const double build = S.st.buildMs;
	S.st = ReflCaptureStat();
	S.st.pipelines = pipes;
	S.st.buildMs = build;
	return s;
}
}  // namespace WwVkPresent
