/* Lane VKOBJ12 (2026-11-16): the TAA inputs' step 3 (taa_objvel, the movers' own motion) on the present device (vkobjvel.h).
 * Included once by vkpresent.cpp at its end. GL's pass (taainputs.cpp step 3): fbo = g.vel (RG16F) + g.mask (RGBA8) + g.depth,
 * LEQUAL, depth write off, no cull / blend, glPolygonOffset( -1, -4 ), att1's mask B (pass 1: att0 none, att1 R), a draw a
 * mover from CPU arrays. Here: an own render pass over the three shared images (all LOAD / STORE, GENERAL), two pipelines a
 * depth format (pass 0 / pass 1 masks) with the bias as dynamic state, a b31 slice and a set a draw (packed by name from
 * objvel_tables.h), vertices and indices in one host buffer, one crossing for the whole list (GL waits it). Self-contained:
 * every helper here is ov-prefixed (no farlod / farlodrun parts). */
#define WW_VK_OBJVEL 1
#include "features/vkframe/vkobjvel.h"
#include "features/vkframe/objvel_tables.h"
#include "features/vkframe/objvel_vs.spv.h"  // vkframe_spirv.sh's fxo lines
#include "features/vkframe/objvel_ps.spv.h"

namespace
{
namespace objvel
{
using namespace WwVkPresent;

// a draw's uniform slice: the vertex stage's b31 (binding 287) at 0, the pixel stage's (binding 31) at 512
constexpr VkDeviceSize kOvSlice = 768, kOvVsAt = 0, kOvPsAt = 512;
static_assert( VkDeviceSize( kT_objvelVsBytes ) <= kOvPsAt && kOvPsAt + VkDeviceSize( kT_objvelPsBytes ) <= kOvSlice, "objvel's slice" );
constexpr int kOvMaxDraws = 8192;
// the vertex stage's inputs (reflected): 0 position, 1 colour, 2-4 N / T / B, 5 uv (GL's 7), 6 last frame's position (GL's 8)
constexpr std::uint32_t kOvStride[7] = { 12, 16, 12, 12, 12, 8, 12 };
const VkFormat kOvFmt[7] = { VK_FORMAT_R32G32B32_SFLOAT, VK_FORMAT_R32G32B32A32_SFLOAT, VK_FORMAT_R32G32B32_SFLOAT,
	VK_FORMAT_R32G32B32_SFLOAT, VK_FORMAT_R32G32B32_SFLOAT, VK_FORMAT_R32G32_SFLOAT, VK_FORMAT_R32G32B32_SFLOAT };
constexpr int kOvDepthSlots = 4;  // D24S8, D32F_S8, X8_D24, D32F

struct OvJob
{
	VkDeviceSize off[7] = {}, idxOff = 0;
	std::uint32_t nIdx = 0;
	int pass = 0;
};

struct OvState
{
	bool made = false, broken = false;
	std::string brokenWhy;
	VkShaderModule vs = VK_NULL_HANDLE, ps = VK_NULL_HANDLE;
	VkDescriptorSetLayout dsl = VK_NULL_HANDLE;
	VkPipelineLayout layout = VK_NULL_HANDLE;
	VkSampler plain = VK_NULL_HANDLE;  // the unread slots' sampler
	PFN_vkCmdDrawIndexed drawIndexed = nullptr;
	VkFormat passFmt[kOvDepthSlots] = {};
	VkRenderPass pass[kOvDepthSlots] = {};
	VkPipeline pipe[kOvDepthSlots][2] = {};
	VkDescriptorPool pool = VK_NULL_HANDLE;
	std::vector<VkDescriptorSet> sets;
	VkSemaphore glDone = VK_NULL_HANDLE, vkDone = VK_NULL_HANDLE;
	GLuint glDoneGl = 0, vkDoneGl = 0;
	void * semCtx = nullptr;
	VkCommandBuffer cmd = VK_NULL_HANDLE;
	VkFence fence = VK_NULL_HANDLE;
	bool pending = false;
	CsmBuf geo, ubo;  // host-visible: the vertices + indices; the slices
	VkFramebuffer fb = VK_NULL_HANDLE;
	VkImageView fbViews[3] = {};
	int fbW = 0, fbH = 0, fbSlot = -1;
	std::uint64_t fbGen = 0;
	// the crossing's job, written by the drawing thread before the hand
	int slot = 0, red = 0;
	std::vector<OvJob> jobs;
	ObjVelStat st;
};

OvState & ovState()
{
	static OvState s;
	return s;
}

void * ovGp( const char * name )
{
	return reinterpret_cast<void *>( wglGetProcAddress( name ) );
}

int ovField( const FxField * t, int n, const char * name, const FxField ** out )
{
	for ( int i = 0; i < n; i++ )
		if ( std::strcmp( t[i].name, name ) == 0 ) {
			*out = &t[i];
			return t[i].offset;
		}
	*out = nullptr;
	return -1;
}

// the device-time parts (once): modules, the set layout, the pipeline layout, the plain sampler, the command buffer, the fence
bool ovStatic( std::string & why )
{
	Vk & v = vk();
	OvState & S = ovState();
	if ( S.made )
		return true;
	if ( !fxModule( vkframe_objvel_vs_spv, sizeof( vkframe_objvel_vs_spv ), S.vs )
		|| !fxModule( vkframe_objvel_ps_spv, sizeof( vkframe_objvel_ps_spv ), S.ps ) ) {
		why = "the objvel twins' modules failed";
		return false;
	}
	std::vector<VkDescriptorSetLayoutBinding> lb;
	lb.push_back( { 287, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, nullptr } );
	lb.push_back( { 31, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr } );
	for ( std::uint32_t b = 64; b < 68; b++ )  // BaseMap, GreyscaleMap, NormalMap, fxDepth
		lb.push_back( { b, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr } );
	for ( std::uint32_t b = 128; b < 132; b++ )
		lb.push_back( { b, VK_DESCRIPTOR_TYPE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr } );
	VkDescriptorSetLayoutCreateInfo lci { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
	lci.bindingCount = std::uint32_t( lb.size() );
	lci.pBindings = lb.data();
	VkPipelineLayoutCreateInfo pli { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
	pli.setLayoutCount = 1;
	pli.pSetLayouts = &S.dsl;
	if ( v.vkCreateDescriptorSetLayout( v.dev, &lci, nullptr, &S.dsl ) != VK_SUCCESS
		|| v.vkCreatePipelineLayout( v.dev, &pli, nullptr, &S.layout ) != VK_SUCCESS ) {
		why = "the objvel set / pipeline layout failed";
		return false;
	}
	VkSamplerCreateInfo sci { VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
	sci.magFilter = sci.minFilter = VK_FILTER_NEAREST;
	sci.addressModeU = sci.addressModeV = sci.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	sci.maxLod = 0.0f;
	if ( v.vkCreateSampler( v.dev, &sci, nullptr, &S.plain ) != VK_SUCCESS ) {
		why = "the objvel plain sampler failed";
		return false;
	}
	S.drawIndexed = reinterpret_cast<PFN_vkCmdDrawIndexed>( v.vkGetDeviceProcAddr( v.dev, "vkCmdDrawIndexed" ) );
	if ( !S.drawIndexed ) {
		why = "no vkCmdDrawIndexed on the present device";
		return false;
	}
	VkFenceCreateInfo fi { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
	VkCommandBufferAllocateInfo cai { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
	cai.commandPool = v.pool;
	cai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	cai.commandBufferCount = 1;
	if ( v.vkCreateFence( v.dev, &fi, nullptr, &S.fence ) != VK_SUCCESS || v.vkAllocateCommandBuffers( v.dev, &cai, &S.cmd ) != VK_SUCCESS ) {
		why = "the objvel fence / command buffer failed";
		return false;
	}
	S.made = true;
	return true;
}

// the render pass of a depth format: att0 RG16F, att1 RGBA8, the depth (and stencil) read in place; everything kept
bool ovPass( VkFormat dsFmt, int & slot, std::string & why )
{
	Vk & v = vk();
	OvState & S = ovState();
	slot = -1;
	for ( int i = 0; i < kOvDepthSlots && slot < 0; i++ )
		if ( S.pass[i] && S.passFmt[i] == dsFmt )
			slot = i;
	if ( slot >= 0 )
		return true;
	for ( int i = 0; i < kOvDepthSlots && slot < 0; i++ )
		if ( !S.pass[i] )
			slot = i;
	if ( slot < 0 ) {
		why = "the objvel render passes are full (a fifth depth format)";
		return false;
	}
	const bool stencil = dsFmt == VK_FORMAT_D24_UNORM_S8_UINT || dsFmt == VK_FORMAT_D32_SFLOAT_S8_UINT;
	VkAttachmentDescription at[3] {};
	const VkFormat fmts[3] = { VK_FORMAT_R16G16_SFLOAT, VK_FORMAT_R8G8B8A8_UNORM, dsFmt };
	for ( int i = 0; i < 3; i++ ) {
		at[i].format = fmts[i];
		at[i].samples = VK_SAMPLE_COUNT_1_BIT;
		at[i].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;  // steps 1-2 (GL) wrote them; step 4 (GL) reads them
		at[i].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		at[i].stencilLoadOp = i == 2 && stencil ? VK_ATTACHMENT_LOAD_OP_LOAD : VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		at[i].stencilStoreOp = i == 2 && stencil ? VK_ATTACHMENT_STORE_OP_STORE : VK_ATTACHMENT_STORE_OP_DONT_CARE;
		at[i].initialLayout = at[i].finalLayout = VK_IMAGE_LAYOUT_GENERAL;
	}
	const VkAttachmentReference cr[2] = { { 0, VK_IMAGE_LAYOUT_GENERAL }, { 1, VK_IMAGE_LAYOUT_GENERAL } };
	const VkAttachmentReference dr { 2, VK_IMAGE_LAYOUT_GENERAL };
	VkSubpassDescription sp {};
	sp.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	sp.colorAttachmentCount = 2;
	sp.pColorAttachments = cr;
	sp.pDepthStencilAttachment = &dr;
	const VkPipelineStageFlags st = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT
		| VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
	const VkAccessFlags rw = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
		| VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	const VkSubpassDependency dep[2] = { { VK_SUBPASS_EXTERNAL, 0, st, st, rw, rw, 0 }, { 0, VK_SUBPASS_EXTERNAL, st, st, rw, rw, 0 } };
	VkRenderPassCreateInfo rci { VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO };
	rci.attachmentCount = 3;
	rci.pAttachments = at;
	rci.subpassCount = 1;
	rci.pSubpasses = &sp;
	rci.dependencyCount = 2;
	rci.pDependencies = dep;
	if ( v.vkCreateRenderPass( v.dev, &rci, nullptr, &S.pass[slot] ) != VK_SUCCESS ) {
		S.pass[slot] = VK_NULL_HANDLE;
		why = "vkCreateRenderPass (objvel) failed";
		return false;
	}
	S.passFmt[slot] = dsFmt;
	return true;
}

// a pass's pipeline: m 0 = pass 0's masks (att0 RGBA, att1 B), 1 = pass 1's (att0 none, att1 R); GL's state otherwise
bool ovPipe( int slot, int m, std::string & why )
{
	Vk & v = vk();
	OvState & S = ovState();
	if ( S.pipe[slot][m] )
		return true;
	if ( !v.clipControl ) {
		why = "no depthClipControl on the present device (GL's -1..1 clip z)";
		return false;
	}
	const double t0 = nowMs();
	VkPipelineShaderStageCreateInfo st[2] { { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO },
		{ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO } };
	st[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
	st[0].module = S.vs;
	st[0].pName = "main";
	st[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	st[1].module = S.ps;
	st[1].pName = "main";
	VkVertexInputBindingDescription vb[7];
	VkVertexInputAttributeDescription va[7];
	for ( std::uint32_t i = 0; i < 7; i++ ) {
		vb[i] = { i, kOvStride[i], VK_VERTEX_INPUT_RATE_VERTEX };
		va[i] = { i, i, kOvFmt[i], 0 };
	}
	VkPipelineVertexInputStateCreateInfo vi { VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
	vi.vertexBindingDescriptionCount = 7;
	vi.pVertexBindingDescriptions = vb;
	vi.vertexAttributeDescriptionCount = 7;
	vi.pVertexAttributeDescriptions = va;
	VkPipelineInputAssemblyStateCreateInfo ia { VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
	ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	VkPipelineViewportDepthClipControlCreateInfoEXT cc { VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_DEPTH_CLIP_CONTROL_CREATE_INFO_EXT };
	cc.negativeOneToOne = VK_TRUE;
	VkPipelineViewportStateCreateInfo vps { VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO };
	vps.pNext = &cc;
	vps.viewportCount = vps.scissorCount = 1;
	VkPipelineRasterizationStateCreateInfo rs { VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
	rs.polygonMode = VK_POLYGON_MODE_FILL;
	rs.cullMode = VK_CULL_MODE_NONE;
	rs.frontFace = VK_FRONT_FACE_CLOCKWISE;
	rs.depthBiasEnable = VK_TRUE;  // glPolygonOffset's factor / units: vkCmdSetDepthBias in the record
	rs.lineWidth = 1.0f;
	VkPipelineMultisampleStateCreateInfo ms { VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
	ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	VkPipelineDepthStencilStateCreateInfo ds { VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };
	ds.depthTestEnable = VK_TRUE;
	ds.depthWriteEnable = VK_FALSE;
	ds.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
	VkPipelineColorBlendAttachmentState cb[2] {};
	cb[0].colorWriteMask = m == 0 ? VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT : 0;
	cb[1].colorWriteMask = m == 0 ? VK_COLOR_COMPONENT_B_BIT : VK_COLOR_COMPONENT_R_BIT;
	VkPipelineColorBlendStateCreateInfo cbs { VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
	cbs.attachmentCount = 2;
	cbs.pAttachments = cb;
	const VkDynamicState dyn[3] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR, VK_DYNAMIC_STATE_DEPTH_BIAS };
	VkPipelineDynamicStateCreateInfo dy { VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
	dy.dynamicStateCount = 3;
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
	gp.layout = S.layout;
	gp.renderPass = S.pass[slot];
	if ( v.vkCreateGraphicsPipelines( v.dev, tone().cache, 1, &gp, nullptr, &S.pipe[slot][m] ) != VK_SUCCESS ) {
		S.pipe[slot][m] = VK_NULL_HANDLE;
		why = "vkCreateGraphicsPipelines (objvel) failed";
		return false;
	}
	S.st.pipelines++;
	S.st.buildMs += nowMs() - t0;
	toneCacheSave();
	return true;
}

bool ovSems( std::string & why )
{
	Vk & v = vk();
	Gl & g = gl();
	OvState & S = ovState();
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

// the framebuffer over the three shared images (kept until a shared target is made or goes, or the size / pass changes)
bool ovFb( const Shared * vel, const Shared * mask, const Shared * depth, int slot, std::string & why )
{
	Vk & v = vk();
	OvState & S = ovState();
	const VkImageView views[3] = { vel->view, mask->view, depth->attView ? depth->attView : depth->view };
	if ( S.fb && S.fbGen == sharedGen() && S.fbSlot == slot && S.fbW == vel->w && S.fbH == vel->h
		&& std::equal( views, views + 3, S.fbViews ) )
		return true;
	if ( S.fb )
		v.vkDestroyFramebuffer( v.dev, S.fb, nullptr );
	S.fb = VK_NULL_HANDLE;
	VkFramebufferCreateInfo fci { VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
	fci.renderPass = S.pass[slot];
	fci.attachmentCount = 3;
	fci.pAttachments = views;
	fci.width = std::uint32_t( vel->w );
	fci.height = std::uint32_t( vel->h );
	fci.layers = 1;
	if ( v.vkCreateFramebuffer( v.dev, &fci, nullptr, &S.fb ) != VK_SUCCESS ) {
		why = "the objvel framebuffer failed";
		return false;
	}
	std::copy( views, views + 3, S.fbViews );
	S.fbSlot = slot;
	S.fbW = vel->w;
	S.fbH = vel->h;
	S.fbGen = sharedGen();
	return true;
}

// sets for n draws (the pool grown by doubling; written again every run)
bool ovSets( int n, std::string & why )
{
	Vk & v = vk();
	OvState & S = ovState();
	if ( int( S.sets.size() ) >= n )
		return true;
	int cap = 64;
	while ( cap < n )
		cap *= 2;
	if ( S.pool )
		v.vkDestroyDescriptorPool( v.dev, S.pool, nullptr );
	S.pool = VK_NULL_HANDLE;
	S.sets.clear();
	const std::uint32_t c = std::uint32_t( cap );
	const VkDescriptorPoolSize ps[3] = { { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 2 * c }, { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 4 * c },
		{ VK_DESCRIPTOR_TYPE_SAMPLER, 4 * c } };
	VkDescriptorPoolCreateInfo pci { VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
	pci.maxSets = c;
	pci.poolSizeCount = 3;
	pci.pPoolSizes = ps;
	if ( v.vkCreateDescriptorPool( v.dev, &pci, nullptr, &S.pool ) != VK_SUCCESS ) {
		why = "the objvel descriptor pool failed";
		return false;
	}
	std::vector<VkDescriptorSetLayout> lay( std::size_t( cap ), S.dsl );
	S.sets.assign( std::size_t( cap ), VK_NULL_HANDLE );
	VkDescriptorSetAllocateInfo ai { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
	ai.descriptorPool = S.pool;
	ai.descriptorSetCount = c;
	ai.pSetLayouts = lay.data();
	if ( v.vkAllocateDescriptorSets( v.dev, &ai, S.sets.data() ) != VK_SUCCESS ) {
		S.sets.clear();
		why = "the objvel descriptor sets failed";
		return false;
	}
	return true;
}

// the worker: one render pass, a draw a job (pass 1 with its own masks; red 2: pass 0's), GL's signal waited, signalled back
bool ovWork( std::string & why )
{
	Vk & v = vk();
	OvState & S = ovState();
	v.vkResetFences( v.dev, 1, &S.fence );
	v.vkResetCommandBuffer( S.cmd, 0 );
	VkCommandBufferBeginInfo bi { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
	bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	v.vkBeginCommandBuffer( S.cmd, &bi );
	VkRenderPassBeginInfo rbi { VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
	rbi.renderPass = S.pass[S.slot];
	rbi.framebuffer = S.fb;
	rbi.renderArea = { { 0, 0 }, { std::uint32_t( S.fbW ), std::uint32_t( S.fbH ) } };
	v.vkCmdBeginRenderPass( S.cmd, &rbi, VK_SUBPASS_CONTENTS_INLINE );
	// no y flip: GL's rows from the bottom are the image's rows from the top
	const VkViewport vp { 0.0f, 0.0f, float( S.fbW ), float( S.fbH ), 0.0f, 1.0f };
	const VkRect2D sc { { 0, 0 }, { std::uint32_t( S.fbW ), std::uint32_t( S.fbH ) } };
	int bound = -1;
	for ( std::size_t d = 0; d < S.jobs.size(); d++ ) {
		const OvJob & j = S.jobs[d];
		const int m = j.pass == 1 && S.red != 2 ? 1 : 0;
		if ( m != bound ) {
			v.vkCmdBindPipeline( S.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, S.pipe[S.slot][m] );
			v.vkCmdSetViewport( S.cmd, 0, 1, &vp );
			v.vkCmdSetScissor( S.cmd, 0, 1, &sc );
			// glPolygonOffset( -1, -4 ): factor = slope, units = constant (red 3: none)
			if ( S.red == 3 )
				v.vkCmdSetDepthBias( S.cmd, 0.0f, 0.0f, 0.0f );
			else
				v.vkCmdSetDepthBias( S.cmd, -4.0f, 0.0f, -1.0f );
			bound = m;
		}
		const VkBuffer bufs[7] = { S.geo.buf, S.geo.buf, S.geo.buf, S.geo.buf, S.geo.buf, S.geo.buf, S.geo.buf };
		v.vkCmdBindVertexBuffers( S.cmd, 0, 7, bufs, j.off );
		v.vkCmdBindIndexBuffer( S.cmd, S.geo.buf, j.idxOff, VK_INDEX_TYPE_UINT16 );
		v.vkCmdBindDescriptorSets( S.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, S.layout, 0, 1, &S.sets[d], 0, nullptr );
		S.drawIndexed( S.cmd, j.nIdx, 1, 0, 0, 0 );
	}
	v.vkCmdEndRenderPass( S.cmd );
	if ( v.vkEndCommandBuffer( S.cmd ) != VK_SUCCESS ) {
		why = "vkEndCommandBuffer (objvel) failed";
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
		why = "vkQueueSubmit (objvel) returned " + std::to_string( int( r ) );
		return false;
	}
	return true;
}

// a field by name into a slice: a matrix (cols > 0) as GL's column c at + 16 c (src: n x n column-major, as glUniformMatrix takes)
bool ovPut( std::uint8_t * at, const FxField * t, int nt, const char * name, const void * src, int shift, std::string & why )
{
	const FxField * f = nullptr;
	const int off = ovField( t, nt, name, &f );
	if ( off < 0 ) {
		why = std::string( "the objvel table has no " ) + name;
		return false;
	}
	if ( f->cols > 0 ) {
		const float * m = static_cast<const float *>( src );
		for ( int c = 0; c < f->cols; c++ )
			std::memcpy( at + off + shift + 16 * c, m + f->words * c, std::size_t( f->words ) * 4 );
		return true;
	}
	std::memcpy( at + off + shift, src, std::size_t( f->words ) * 4 );
	return true;
}

// a BaseMap's twin: its view (GL's swizzle as the view's own), sampler and the GL name the crossing signals
struct OvBase
{
	VkImageView view = VK_NULL_HANDLE;
	VkSampler smp = VK_NULL_HANDLE;
	GLuint gl = 0;
};

bool ovBase( unsigned tex, OvBase & out, std::string & why )
{
	const std::uintptr_t key = objTexKey( tex, 0 );
	if ( !key ) {
		why = "BaseMap: GL's texture " + std::to_string( tex ) + " has no texture-cache twin (a coverage copy or a GL-only format)";
		return false;
	}
	BlSampler bs;
	VkFormat fmt = VK_FORMAT_UNDEFINED;
	std::uint32_t swz = 0;
	std::string w;
	if ( !blView( key, fmt ) || !blSamplerRead( tex, bs, ovGp, w ) ) {
		why = "BaseMap: " + ( w.empty() ? std::string( "the twin is no 2D view" ) : w );
		return false;
	}
	if ( blIsSrgb( fmt ) && bs.srgbDecode == 0x8A4A ) {
		why = "BaseMap: an sRGB texture with its decode skipped";
		return false;
	}
	blSwizzleNorm( fmt, bs );
	if ( !blSwizzleCode( bs, swz, w ) ) {
		why = "BaseMap: " + w;
		return false;
	}
	out.view = blView( key, fmt, swz );
	ObjBlSampler s;
	if ( !out.view || !objBlSamplerRead( ovGp, tex, s, w ) || !opSampler( s, out.smp, w ) ) {
		why = "BaseMap: " + ( w.empty() ? std::string( "no swizzled view" ) : w );
		return false;
	}
	const Shared * th = sharedFind( key );
	if ( !th )
		th = farTwinFind( key );
	if ( !th || !th->tex ) {
		why = "BaseMap: the twin has no GL name";
		return false;
	}
	out.gl = th->tex;
	return true;
}
}  // namespace objvel

// lane VKOBJ12: on a present shutdown (the shared targets go): the framebuffer and the host buffers
void objVelDrop()
{
	Vk & v = vk();
	objvel::OvState & S = objvel::ovState();
	if ( !S.fb && !S.geo.buf && !S.ubo.buf )
		return;
	v.vkDeviceWaitIdle( v.dev );
	if ( S.fb )
		v.vkDestroyFramebuffer( v.dev, S.fb, nullptr );
	S.fb = VK_NULL_HANDLE;
	S.fbSlot = -1;
	S.pending = false;
	csmBufFree( S.geo, false );
	csmBufFree( S.ubo, false );
}
}  // namespace

namespace WwVkPresent
{
std::string objVelRefusal()
{
	objvel::OvState & S = objvel::ovState();
	if ( S.broken )
		return S.brokenWhy;
	if ( !vk().ok || !gl().ok || gl().ctx != reinterpret_cast<void *>( wglGetCurrentContext() ) )
		return "the present device or GL's entry points are not ready in this context";
	return std::string();
}

bool objVelRun( const ObjVelRun & r, std::string & why )
{
	using namespace objvel;
	OvState & S = ovState();
	Gl & g = gl();
	Vk & v = vk();
	const double t0 = nowMs();
	auto refuse = [&]( const std::string & w ) {
		why = w;
		S.st.fails++;
		S.st.failWhy = w;
		return false;
	};
	auto breakOff = [&]( const std::string & w ) {
		S.broken = true;
		S.brokenWhy = "objvel on Vulkan broke: " + w;
		return refuse( S.brokenWhy );
	};
	if ( const std::string no = objVelRefusal(); !no.empty() )
		return refuse( no );
	const int n = int( r.draws.size() );
	if ( !n )
		return true;
	if ( n > kOvMaxDraws )
		return refuse( std::to_string( n ) + " draws (the route takes " + std::to_string( kOvMaxDraws ) + ")" );
	// the targets: the owner's shared g.vel / g.mask / g.depth at the pass's size
	const Shared * tv = sharedFind( r.velKey );
	const Shared * tm = sharedFind( r.maskKey );
	const Shared * td = sharedFind( r.depthKey );
	if ( !tv || !tv->tex || tv->fmt != VK_FORMAT_R16G16_SFLOAT || !tm || !tm->tex || tm->fmt != VK_FORMAT_R8G8B8A8_UNORM || !td || !td->tex )
		return refuse( "the TAA inputs' targets are not shared (RG16F, RGBA8, depth)" );
	if ( tv->w != r.w || tv->h != r.h || tm->w != r.w || tm->h != r.h || td->w != r.w || td->h != r.h || tv->samples != 1 || td->samples != 1 )
		return refuse( "the shared targets are not the pass's size" );
	if ( td->fmt != VK_FORMAT_D24_UNORM_S8_UINT && td->fmt != VK_FORMAT_D32_SFLOAT_S8_UINT && td->fmt != VK_FORMAT_X8_D24_UNORM_PACK32
		&& td->fmt != VK_FORMAT_D32_SFLOAT )
		return refuse( "the depth target's format " + std::to_string( int( td->fmt ) ) + " has no objvel pass" );
	// the whole list first: a refusal draws nothing here (GL draws every mover)
	std::size_t geoNeed = 0;
	int maxNv = 1;
	for ( int d = 0; d < n; d++ ) {
		const ObjVelDraw & x = r.draws[std::size_t( d )];
		const std::string at = "draw " + std::to_string( d ) + ": ";
		if ( x.fxMode & 2 )
			return refuse( at + "fxMode 2 (the palette's alpha) has no Vulkan slice yet" );
		if ( x.fxMode & 8 )
			return refuse( at + "fxMode 8 (the falloff) has no Vulkan slice yet" );
		if ( x.fxMode & 16 )
			return refuse( at + "fxMode 16 (the soft fade) has no Vulkan slice yet" );
		if ( x.velMode == 2 )
			return refuse( at + "velMode 2 (grass wind) has no Vulkan slice yet" );
		if ( x.velMode != 0 && x.velMode != 1 )
			return refuse( at + "velMode " + std::to_string( x.velMode ) );
		if ( !x.pos || x.nv <= 0 || !x.idx || x.nIdx <= 0 || x.nIdx % 3 )
			return refuse( at + "no vertices or triangles" );
		if ( ( x.fxMode & 1 ) && !x.color )
			return refuse( at + "fxMode 1 (vertex alpha) without colours" );
		if ( ( x.alphaTest || x.covMode ) && !x.uv )
			return refuse( at + "an alpha test / coverage without coordinates" );
		if ( x.pass != 0 && x.pass != 1 )
			return refuse( at + "pass " + std::to_string( x.pass ) );
		if ( x.pass == 1 && x.covMode != 2 )
			return refuse( at + "pass 1 without covMode 2" );
		maxNv = std::max( maxNv, x.nv );
		const std::size_t nv = std::size_t( x.nv );
		geoNeed += ( nv * 12 + 15 ) / 16 * 16 * 2 + ( nv * 16 + 15 ) / 16 * 16 + ( nv * 8 + 15 ) / 16 * 16
			+ ( std::size_t( x.nIdx ) * 2 + 15 ) / 16 * 16;
	}
	const std::size_t zeroBytes = std::size_t( maxNv ) * 16;
	geoNeed += zeroBytes;
	// the BaseMaps read (an alpha test or a coverage): their twins, once a GL name
	std::vector<std::pair<unsigned, OvBase>> bases;
	std::vector<int> baseOf( std::size_t( n ), -1 );
	for ( int d = 0; d < n; d++ ) {
		const ObjVelDraw & x = r.draws[std::size_t( d )];
		if ( !x.alphaTest && !x.covMode )
			continue;
		int hit = -1;
		for ( std::size_t i = 0; i < bases.size() && hit < 0; i++ )
			if ( bases[i].first == x.baseTex )
				hit = int( i );
		if ( hit < 0 ) {
			OvBase b;
			std::string w;
			if ( !x.baseTex || !ovBase( x.baseTex, b, w ) )
				return refuse( "draw " + std::to_string( d ) + ": " + ( x.baseTex ? w : std::string( "BaseMap: no texture bound" ) ) );
			bases.emplace_back( x.baseTex, b );
			hit = int( bases.size() ) - 1;
		}
		baseOf[std::size_t( d )] = hit;
	}
	std::string w;
	int slot = -1;
	if ( !fxStatic( w ) || !ovStatic( w ) || !ovSems( w ) || !ovPass( td->fmt, slot, w ) )
		return breakOff( w );
	if ( !ovPipe( slot, 0, w ) || !ovPipe( slot, 1, w ) )
		return w.rfind( "no depthClipControl", 0 ) == 0 ? refuse( w ) : breakOff( w );
	// the last crossing is done before its command buffer, sets, slices and buffers are written again
	if ( std::string fw; !fenceDone( S.fence, S.pending, "objvel", fw ) )
		return refuse( fw );
	S.pending = false;
	if ( !objTexFlush( w ) )
		return refuse( "the texture cache's flush: " + w );
	if ( !ovFb( tv, tm, td, slot, w ) || !ovSets( n, w )
		|| !csmBuf( S.geo, VkDeviceSize( geoNeed ), false, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, w )
		|| !csmBuf( S.ubo, kOvSlice * VkDeviceSize( n ), false, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, w ) )
		return breakOff( w );
	// the vertices (a zero region for every stream a draw does not give) and indices; a slice a draw
	std::uint8_t * geo = static_cast<std::uint8_t *>( S.geo.ptr );
	std::uint8_t * ubo = static_cast<std::uint8_t *>( S.ubo.ptr );
	std::memset( geo, 0, zeroBytes );
	VkDeviceSize at = VkDeviceSize( zeroBytes );
	auto put = [&]( const void * src, std::size_t bytes ) {
		const VkDeviceSize o = at;
		std::memcpy( geo + o, src, bytes );
		at += VkDeviceSize( ( bytes + 15 ) / 16 * 16 );
		return o;
	};
	const int nVs = int( sizeof( kT_objvelVs ) / sizeof( kT_objvelVs[0] ) );
	const int nPs = int( sizeof( kT_objvelPs ) / sizeof( kT_objvelPs[0] ) );
	S.jobs.assign( std::size_t( n ), OvJob() );
	for ( int d = 0; d < n; d++ ) {
		const ObjVelDraw & x = r.draws[std::size_t( d )];
		const std::size_t nv = std::size_t( x.nv );
		OvJob & j = S.jobs[std::size_t( d )];
		j.off[0] = put( x.pos, nv * 12 );
		// red 1: a skinned draw's last-frame position is this frame's (no own motion)
		j.off[6] = x.velMode == 1 && x.prev && r.red != 1 ? put( x.prev, nv * 12 ) : j.off[0];
		j.off[1] = x.color ? put( x.color, nv * 16 ) : 0;
		j.off[5] = x.uv ? put( x.uv, nv * 8 ) : 0;
		j.off[2] = j.off[3] = j.off[4] = 0;  // the falloff's frame: refused above, never read
		j.idxOff = put( x.idx, std::size_t( x.nIdx ) * 2 );
		j.nIdx = std::uint32_t( x.nIdx );
		j.pass = x.pass;
		std::uint8_t * sl = ubo + kOvSlice * VkDeviceSize( d );
		std::memset( sl, 0, std::size_t( kOvSlice ) );
		std::uint8_t * vs = sl + kOvVsAt;
		std::uint8_t * ps = sl + kOvPsAt;
		const float zero3[3] = {};
		const bool ok = ovPut( vs, kT_objvelVs, nVs, "clipJit", x.clipJit, 0, w ) && ovPut( vs, kT_objvelVs, nVs, "clipCur", x.clipCur, 0, w )
			&& ovPut( vs, kT_objvelVs, nVs, "clipPrev", x.clipPrev, r.red == 4 ? 16 : 0, w )
			&& ovPut( vs, kT_objvelVs, nVs, "modelViewMatrix", x.modelView, 0, w )
			&& ovPut( vs, kT_objvelVs, nVs, "normalMatrix", x.normalMatrix, 0, w ) && ovPut( vs, kT_objvelVs, nVs, "velMode", &x.velMode, 0, w )
			&& ovPut( ps, kT_objvelPs, nPs, "uvScale", x.uvScale, 0, w ) && ovPut( ps, kT_objvelPs, nPs, "uvOffset", x.uvOffset, 0, w )
			&& ovPut( ps, kT_objvelPs, nPs, "alphaTest", &x.alphaTest, 0, w ) && ovPut( ps, kT_objvelPs, nPs, "alpha", &x.alpha, 0, w )
			&& ovPut( ps, kT_objvelPs, nPs, "alphaThreshold", &x.alphaThreshold, 0, w )
			&& ovPut( ps, kT_objvelPs, nPs, "wwGrassFade", zero3, 0, w ) && ovPut( ps, kT_objvelPs, nPs, "fxMode", &x.fxMode, 0, w )
			&& ovPut( ps, kT_objvelPs, nPs, "covMode", &x.covMode, 0, w ) && ovPut( ps, kT_objvelPs, nPs, "covThreshold", &x.covThreshold, 0, w );
		if ( !ok )
			return breakOff( w );
	}
	// the sets: the two slices, BaseMap (a twin or the zero view), the unread maps as zero views
	Fx & F = fx();
	std::vector<VkDescriptorBufferInfo> bi( std::size_t( n ) * 2 );
	std::vector<VkDescriptorImageInfo> ii( std::size_t( n ) * 8 );
	std::vector<VkWriteDescriptorSet> ws;
	ws.reserve( std::size_t( n ) * 10 );
	for ( int d = 0; d < n; d++ ) {
		const std::size_t q = std::size_t( d );
		const VkDeviceSize sl = kOvSlice * VkDeviceSize( d );
		bi[q * 2] = { S.ubo.buf, sl + kOvVsAt, VkDeviceSize( kT_objvelVsBytes ) };
		bi[q * 2 + 1] = { S.ubo.buf, sl + kOvPsAt, VkDeviceSize( kT_objvelPsBytes ) };
		const OvBase * b = baseOf[q] >= 0 ? &bases[std::size_t( baseOf[q] )].second : nullptr;
		for ( int i = 0; i < 4; i++ ) {
			ii[q * 8 + std::size_t( i )] = { VK_NULL_HANDLE, i == 0 && b ? b->view : F.d2, VK_IMAGE_LAYOUT_GENERAL };
			ii[q * 8 + 4 + std::size_t( i )] = { i == 0 && b ? b->smp : S.plain, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_UNDEFINED };
		}
		for ( int k = 0; k < 10; k++ ) {
			VkWriteDescriptorSet x { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
			x.dstSet = S.sets[q];
			x.descriptorCount = 1;
			if ( k < 2 ) {
				x.dstBinding = k == 0 ? 287 : 31;
				x.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
				x.pBufferInfo = &bi[q * 2 + std::size_t( k )];
			} else if ( k < 6 ) {
				x.dstBinding = std::uint32_t( 64 + k - 2 );
				x.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
				x.pImageInfo = &ii[q * 8 + std::size_t( k - 2 )];
			} else {
				x.dstBinding = std::uint32_t( 128 + k - 6 );
				x.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
				x.pImageInfo = &ii[q * 8 + 4 + std::size_t( k - 6 )];
			}
			ws.push_back( x );
		}
	}
	v.vkUpdateDescriptorSets( v.dev, std::uint32_t( ws.size() ), ws.data(), 0, nullptr );
	S.slot = slot;
	S.red = r.red;
	// GL's steps 1-2 (the targets) and the BaseMaps' twins signalled -> the worker's job -> GL waits
	std::vector<GLuint> texNames { tv->tex, tm->tex, td->tex };
	for ( const auto & b : bases )
		texNames.push_back( b.second.gl );
	std::sort( texNames.begin(), texNames.end() );
	texNames.erase( std::unique( texNames.begin(), texNames.end() ), texNames.end() );
	std::vector<GLenum> lay( texNames.size(), kLayoutGeneral );
	g.signalSemaphore( S.glDoneGl, 0, nullptr, GLuint( texNames.size() ), texNames.data(), lay.data() );
	g.flush();
	if ( !opWorkerRun( ovWork, w ) )
		return breakOff( w );  // nothing signals vkDone: GL does not wait it; GL draws the movers
	g.waitSemaphore( S.vkDoneGl, 0, nullptr, GLuint( texNames.size() ), texNames.data(), lay.data() );
	S.pending = true;
	const double ms = nowMs() - t0;
	S.st.runs++;
	S.st.crossings++;
	S.st.draws += n;
	S.st.twins += int( bases.size() );
	S.st.cpuMs += ms;
	S.st.maxMs = std::max( S.st.maxMs, ms );
	return true;
}

void objVelOnGl( const char * why )
{
	objvel::OvState & S = objvel::ovState();
	S.st.gl++;
	S.st.glWhy = why ? why : "";
}

ObjVelStat takeObjVelStat()
{
	objvel::OvState & S = objvel::ovState();
	ObjVelStat s = S.st;
	if ( S.broken && s.failWhy.empty() )
		s.failWhy = S.brokenWhy;
	const int pipes = S.st.pipelines;
	const double build = S.st.buildMs;
	S.st = ObjVelStat();
	S.st.pipelines = pipes;
	S.st.buildMs = build;
	return s;
}
}  // namespace WwVkPresent
