/* Lane VKAO4 (2026-11-16): the GI volume mask (ao.cpp giVolMaskPass) on the present device (vkvolmask.h). Included once by
 * vkpresent.cpp at its end. GL's pass: the R32F volMask cleared 0 + the opaque D24S8; one glDrawArraysInstanced( TRIANGLES, 0,
 * 36, n ) with depth GEQUAL (no writes), cull FRONT (front face CCW), depth clamp, blend MAX. Here: one render pass (the volMask
 * cleared by its load op, the depth / stencil loaded and stored, GENERAL), one vkCmdDraw( 36, n ). The g-buffer is GL's own:
 * copied into a route-owned shared twin every run. The shapes go into a host-visible uniform texel buffer (RGBA32F, the
 * stages' volShapes at 200 / 64). Self-contained: every helper is vm-prefixed. */
#include "features/vkframe/vkvolmask.h"
#include "features/vkframe/volmask_tables.h"
#include "features/vkframe/volmask_view_vs.spv.h"  // vkframe_spirv.sh's fxt / fxg volmask lines
#include "features/vkframe/volmask_view_ps.spv.h"

namespace
{
namespace volmask
{
using namespace WwVkPresent;

// the draw's uniform slice: the vertex stage's b31 (binding 287) at 0, the pixel stage's (binding 31) at 256
constexpr VkDeviceSize kVmSlice = 512, kVmVsAt = 0, kVmPsAt = 256;
static_assert( VkDeviceSize( kT_volmaskVsBytes ) <= kVmPsAt && kVmPsAt + VkDeviceSize( kT_volmaskPsBytes ) <= kVmSlice, "volmask's slice" );
constexpr int kVmMaxShapes = 65536;
// pipelines: GL's state + (1 blend ADD, 2 depth LESS, 4 no clamp, 8 cull BACK): the reds' arms
constexpr int kVmPipes = 16;

struct VmState
{
	bool made = false, broken = false;
	std::string brokenWhy;
	VkShaderModule vs = VK_NULL_HANDLE, ps = VK_NULL_HANDLE;
	VkDescriptorSetLayout dsl = VK_NULL_HANDLE;
	VkPipelineLayout layout = VK_NULL_HANDLE;
	VkSampler plain = VK_NULL_HANDLE;
	VkRenderPass pass = VK_NULL_HANDLE;
	VkPipeline pipe[kVmPipes] = {};
	VkDescriptorPool pool = VK_NULL_HANDLE;
	VkDescriptorSet set = VK_NULL_HANDLE;
	VkSemaphore glDone = VK_NULL_HANDLE, vkDone = VK_NULL_HANDLE;
	GLuint glDoneGl = 0, vkDoneGl = 0;
	void * semCtx = nullptr;
	VkCommandBuffer cmd = VK_NULL_HANDLE;
	VkFence fence = VK_NULL_HANDLE;
	bool pending = false;
	CsmBuf ubo, shp;
	PFN_vkCreateBufferView createView = nullptr;
	PFN_vkDestroyBufferView destroyView = nullptr;
	VkBufferView shpView = VK_NULL_HANDLE;
	VkBuffer shpViewOf = VK_NULL_HANDLE;
	VkFramebuffer fb = VK_NULL_HANDLE;
	VkImageView fbViews[2] = {};
	int fbW = 0, fbH = 0;
	std::uint64_t fbGen = 0;
	// the crossing's job, written by the drawing thread before the hand
	int jobPipe = 0;
	std::uint32_t jobInstances = 0;
	VolMaskStat st;
};

VmState & vmState()
{
	static VmState s;
	return s;
}

// owner key of the route's g-buffer twin (a sharedTarget key: an address no other owner uses)
char g_vmGbufKey;

// a field by name into a slice: an array (count > 1) at 16 bytes an element, as std140 / cbuffer packing lays it
bool vmPut( std::uint8_t * at, const FxField * t, int nt, const char * name, const void * src, std::string & why )
{
	for ( int i = 0; i < nt; i++ ) {
		if ( std::strcmp( t[i].name, name ) != 0 )
			continue;
		const std::uint8_t * s = static_cast<const std::uint8_t *>( src );
		const int n = std::max( t[i].count, 1 );
		for ( int e = 0; e < n; e++ )
			std::memcpy( at + t[i].offset + 16 * e, s + std::size_t( 4 * t[i].words * e ), std::size_t( 4 * t[i].words ) );
		return true;
	}
	why = std::string( "the volmask table has no " ) + name;
	return false;
}

bool vmStatic( std::string & why )
{
	Vk & v = vk();
	VmState & S = vmState();
	if ( S.made )
		return true;
	S.createView = reinterpret_cast<PFN_vkCreateBufferView>( v.vkGetDeviceProcAddr( v.dev, "vkCreateBufferView" ) );
	S.destroyView = reinterpret_cast<PFN_vkDestroyBufferView>( v.vkGetDeviceProcAddr( v.dev, "vkDestroyBufferView" ) );
	if ( !S.createView || !S.destroyView ) {
		why = "no vkCreateBufferView / vkDestroyBufferView (the shapes' texel buffer)";
		return false;
	}
	if ( !fxModule( vkframe_volmask_view_vs_spv, sizeof( vkframe_volmask_view_vs_spv ), S.vs )
		|| !fxModule( vkframe_volmask_view_ps_spv, sizeof( vkframe_volmask_view_ps_spv ), S.ps ) ) {
		why = "the volmask twins' modules failed";
		return false;
	}
	const VkDescriptorSetLayoutBinding lb[6] = { { 287, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, nullptr },
		{ 31, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr },
		{ 200, VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, nullptr },	// volShapes (vertex)
		{ 64, VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr },	// volShapes (pixel)
		{ 65, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr },	// volGbuf
		{ 129, VK_DESCRIPTOR_TYPE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr } };
	VkDescriptorSetLayoutCreateInfo lci { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
	lci.bindingCount = 6;
	lci.pBindings = lb;
	VkPipelineLayoutCreateInfo pli { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
	pli.setLayoutCount = 1;
	pli.pSetLayouts = &S.dsl;
	if ( v.vkCreateDescriptorSetLayout( v.dev, &lci, nullptr, &S.dsl ) != VK_SUCCESS
		|| v.vkCreatePipelineLayout( v.dev, &pli, nullptr, &S.layout ) != VK_SUCCESS ) {
		why = "the volmask set / pipeline layout failed";
		return false;
	}
	const VkDescriptorPoolSize ps[4] = { { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 2 }, { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 2 },
		{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1 }, { VK_DESCRIPTOR_TYPE_SAMPLER, 1 } };
	VkDescriptorPoolCreateInfo pci { VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
	pci.maxSets = 1;
	pci.poolSizeCount = 4;
	pci.pPoolSizes = ps;
	VkDescriptorSetAllocateInfo ai { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
	ai.descriptorSetCount = 1;
	ai.pSetLayouts = &S.dsl;
	if ( v.vkCreateDescriptorPool( v.dev, &pci, nullptr, &S.pool ) != VK_SUCCESS ) {
		why = "the volmask descriptor pool failed";
		return false;
	}
	ai.descriptorPool = S.pool;
	if ( v.vkAllocateDescriptorSets( v.dev, &ai, &S.set ) != VK_SUCCESS ) {
		why = "the volmask descriptor set failed";
		return false;
	}
	VkSamplerCreateInfo sci { VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
	sci.magFilter = sci.minFilter = VK_FILTER_NEAREST;
	sci.addressModeU = sci.addressModeV = sci.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	sci.maxLod = 0.0f;
	if ( v.vkCreateSampler( v.dev, &sci, nullptr, &S.plain ) != VK_SUCCESS ) {
		why = "the volmask sampler failed";
		return false;
	}
	// the render pass: att0 the volMask R32F (cleared 0 by the load op), att1 the opaque D24S8 (depth and stencil read and kept)
	VkAttachmentDescription at[2] {};
	const VkFormat fmts[2] = { VK_FORMAT_R32_SFLOAT, VK_FORMAT_D24_UNORM_S8_UINT };
	for ( int i = 0; i < 2; i++ ) {
		at[i].format = fmts[i];
		at[i].samples = VK_SAMPLE_COUNT_1_BIT;
		at[i].loadOp = i == 0 ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
		at[i].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		at[i].stencilLoadOp = i == 1 ? VK_ATTACHMENT_LOAD_OP_LOAD : VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		at[i].stencilStoreOp = i == 1 ? VK_ATTACHMENT_STORE_OP_STORE : VK_ATTACHMENT_STORE_OP_DONT_CARE;
		at[i].initialLayout = at[i].finalLayout = VK_IMAGE_LAYOUT_GENERAL;
	}
	const VkAttachmentReference cr { 0, VK_IMAGE_LAYOUT_GENERAL };
	const VkAttachmentReference dr { 1, VK_IMAGE_LAYOUT_GENERAL };
	VkSubpassDescription sp {};
	sp.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	sp.colorAttachmentCount = 1;
	sp.pColorAttachments = &cr;
	sp.pDepthStencilAttachment = &dr;
	const VkPipelineStageFlags st = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT
		| VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
	const VkAccessFlags rw = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
		| VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	const VkSubpassDependency dep[2] = { { VK_SUBPASS_EXTERNAL, 0, st | VK_PIPELINE_STAGE_TRANSFER_BIT, st,
		rw | VK_ACCESS_TRANSFER_WRITE_BIT, rw, 0 }, { 0, VK_SUBPASS_EXTERNAL, st, st, rw, rw, 0 } };
	VkRenderPassCreateInfo rci { VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO };
	rci.attachmentCount = 2;
	rci.pAttachments = at;
	rci.subpassCount = 1;
	rci.pSubpasses = &sp;
	rci.dependencyCount = 2;
	rci.pDependencies = dep;
	if ( v.vkCreateRenderPass( v.dev, &rci, nullptr, &S.pass ) != VK_SUCCESS ) {
		S.pass = VK_NULL_HANDLE;
		why = "vkCreateRenderPass (volmask) failed";
		return false;
	}
	VkFenceCreateInfo fi { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
	VkCommandBufferAllocateInfo cai { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
	cai.commandPool = v.pool;
	cai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	cai.commandBufferCount = 1;
	if ( v.vkCreateFence( v.dev, &fi, nullptr, &S.fence ) != VK_SUCCESS || v.vkAllocateCommandBuffers( v.dev, &cai, &S.cmd ) != VK_SUCCESS ) {
		why = "the volmask fence / command buffer failed";
		return false;
	}
	S.made = true;
	return true;
}

// pipeline p: GL's state, p's bits the reds' arms (1 blend ADD, 2 depth LESS, 4 no clamp, 8 cull BACK)
bool vmPipe( int p, std::string & why )
{
	Vk & v = vk();
	VmState & S = vmState();
	if ( S.pipe[p] )
		return true;
	const double t0 = nowMs();
	VkPipelineShaderStageCreateInfo stg[2] { { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO },
		{ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO } };
	stg[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
	stg[0].module = S.vs;
	stg[0].pName = "main";
	stg[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	stg[1].module = S.ps;
	stg[1].pName = "main";
	VkPipelineVertexInputStateCreateInfo vi { VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };  // the hull from gl_VertexID
	VkPipelineInputAssemblyStateCreateInfo ia { VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
	ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	VkPipelineViewportDepthClipControlCreateInfoEXT cc { VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_DEPTH_CLIP_CONTROL_CREATE_INFO_EXT };
	cc.negativeOneToOne = VK_TRUE;
	VkPipelineViewportStateCreateInfo vps { VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO };
	vps.pNext = &cc;
	vps.viewportCount = vps.scissorCount = 1;
	VkPipelineRasterizationStateCreateInfo rs { VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
	rs.polygonMode = VK_POLYGON_MODE_FILL;
	rs.cullMode = ( p & 8 ) ? VK_CULL_MODE_BACK_BIT : VK_CULL_MODE_FRONT_BIT;
	// GL's CCW with no y flip: the image's rows run the other way, so GL's front is Vulkan's clockwise
	rs.frontFace = VK_FRONT_FACE_CLOCKWISE;
	rs.depthClampEnable = ( p & 4 ) ? VK_FALSE : VK_TRUE;
	rs.lineWidth = 1.0f;
	VkPipelineMultisampleStateCreateInfo ms { VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
	ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	VkPipelineDepthStencilStateCreateInfo ds { VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };
	ds.depthTestEnable = VK_TRUE;
	ds.depthWriteEnable = VK_FALSE;
	ds.depthCompareOp = ( p & 2 ) ? VK_COMPARE_OP_LESS : VK_COMPARE_OP_GREATER_OR_EQUAL;
	ds.stencilTestEnable = VK_FALSE;
	VkPipelineColorBlendAttachmentState cb {};
	cb.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	cb.blendEnable = VK_TRUE;
	cb.srcColorBlendFactor = cb.srcAlphaBlendFactor = cb.dstColorBlendFactor = cb.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	cb.colorBlendOp = cb.alphaBlendOp = ( p & 1 ) ? VK_BLEND_OP_ADD : VK_BLEND_OP_MAX;
	VkPipelineColorBlendStateCreateInfo cbs { VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
	cbs.attachmentCount = 1;
	cbs.pAttachments = &cb;
	const VkDynamicState dyn[2] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
	VkPipelineDynamicStateCreateInfo dy { VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
	dy.dynamicStateCount = 2;
	dy.pDynamicStates = dyn;
	VkGraphicsPipelineCreateInfo gp { VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
	gp.stageCount = 2;
	gp.pStages = stg;
	gp.pVertexInputState = &vi;
	gp.pInputAssemblyState = &ia;
	gp.pViewportState = &vps;
	gp.pRasterizationState = &rs;
	gp.pMultisampleState = &ms;
	gp.pDepthStencilState = &ds;
	gp.pColorBlendState = &cbs;
	gp.pDynamicState = &dy;
	gp.layout = S.layout;
	gp.renderPass = S.pass;
	if ( v.vkCreateGraphicsPipelines( v.dev, tone().cache, 1, &gp, nullptr, &S.pipe[p] ) != VK_SUCCESS ) {
		S.pipe[p] = VK_NULL_HANDLE;
		why = "vkCreateGraphicsPipelines (volmask " + std::to_string( p ) + ") failed";
		return false;
	}
	S.st.pipelines++;
	S.st.buildMs += nowMs() - t0;
	toneCacheSave();
	return true;
}

bool vmSems( std::string & why )
{
	Vk & v = vk();
	Gl & g = gl();
	VmState & S = vmState();
	if ( S.glDone && S.semCtx == g.ctx )
		return true;
	for ( VkSemaphore * s : { &S.glDone, &S.vkDone } )
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

bool vmFb( const Shared * tg, const Shared * dsT, std::string & why )
{
	Vk & v = vk();
	VmState & S = vmState();
	const VkImageView views[2] = { tg->view, dsT->attView ? dsT->attView : dsT->view };
	if ( S.fb && S.fbGen == sharedGen() && S.fbW == tg->w && S.fbH == tg->h && std::equal( views, views + 2, S.fbViews ) )
		return true;
	if ( S.fb )
		v.vkDestroyFramebuffer( v.dev, S.fb, nullptr );
	S.fb = VK_NULL_HANDLE;
	VkFramebufferCreateInfo fci { VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
	fci.renderPass = S.pass;
	fci.attachmentCount = 2;
	fci.pAttachments = views;
	fci.width = std::uint32_t( tg->w );
	fci.height = std::uint32_t( tg->h );
	fci.layers = 1;
	if ( v.vkCreateFramebuffer( v.dev, &fci, nullptr, &S.fb ) != VK_SUCCESS ) {
		why = "the volmask framebuffer failed";
		return false;
	}
	std::copy( views, views + 2, S.fbViews );
	S.fbW = tg->w;
	S.fbH = tg->h;
	S.fbGen = sharedGen();
	return true;
}

// the shapes' texel buffer view, made again when csmBuf replaced the buffer
bool vmShapeView( std::string & why )
{
	Vk & v = vk();
	VmState & S = vmState();
	if ( S.shpView && S.shpViewOf == S.shp.buf )
		return true;
	if ( S.shpView )
		S.destroyView( v.dev, S.shpView, nullptr );
	S.shpView = VK_NULL_HANDLE;
	S.shpViewOf = VK_NULL_HANDLE;
	VkBufferViewCreateInfo bci { VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO };
	bci.buffer = S.shp.buf;
	bci.format = VK_FORMAT_R32G32B32A32_SFLOAT;
	bci.offset = 0;
	bci.range = VK_WHOLE_SIZE;
	if ( S.createView( v.dev, &bci, nullptr, &S.shpView ) != VK_SUCCESS ) {
		S.shpView = VK_NULL_HANDLE;
		why = "vkCreateBufferView (the volmask shapes) failed";
		return false;
	}
	S.shpViewOf = S.shp.buf;
	return true;
}

// the worker: one render pass (the volMask cleared 0), one instanced draw; GL's signal waited, signalled back
bool vmWork( std::string & why )
{
	Vk & v = vk();
	VmState & S = vmState();
	v.vkResetFences( v.dev, 1, &S.fence );
	v.vkResetCommandBuffer( S.cmd, 0 );
	VkCommandBufferBeginInfo bi { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
	bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	v.vkBeginCommandBuffer( S.cmd, &bi );
	VkClearValue cv[2] {};
	VkRenderPassBeginInfo rbi { VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
	rbi.renderPass = S.pass;
	rbi.framebuffer = S.fb;
	rbi.renderArea = { { 0, 0 }, { std::uint32_t( S.fbW ), std::uint32_t( S.fbH ) } };
	rbi.clearValueCount = 2;
	rbi.pClearValues = cv;
	v.vkCmdBeginRenderPass( S.cmd, &rbi, VK_SUBPASS_CONTENTS_INLINE );
	// no y flip: GL's rows from the bottom are the image's rows from the top
	const VkViewport vp { 0.0f, 0.0f, float( S.fbW ), float( S.fbH ), 0.0f, 1.0f };
	const VkRect2D sc { { 0, 0 }, { std::uint32_t( S.fbW ), std::uint32_t( S.fbH ) } };
	if ( S.jobInstances > 0 ) {
		v.vkCmdBindPipeline( S.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, S.pipe[S.jobPipe] );
		v.vkCmdSetViewport( S.cmd, 0, 1, &vp );
		v.vkCmdSetScissor( S.cmd, 0, 1, &sc );
		v.vkCmdBindDescriptorSets( S.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, S.layout, 0, 1, &S.set, 0, nullptr );
		v.vkCmdDraw( S.cmd, 36, S.jobInstances, 0, 0 );
	}
	v.vkCmdEndRenderPass( S.cmd );
	if ( v.vkEndCommandBuffer( S.cmd ) != VK_SUCCESS ) {
		why = "vkEndCommandBuffer (volmask) failed";
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
		why = "vkQueueSubmit (volmask) returned " + std::to_string( int( r ) );
		return false;
	}
	return true;
}
}  // namespace volmask
}  // namespace

namespace WwVkPresent
{
std::string volMaskRefusal()
{
	volmask::VmState & S = volmask::vmState();
	if ( S.broken )
		return S.brokenWhy;
	Vk & v = vk();
	if ( !v.ok || !gl().ok || gl().ctx != reinterpret_cast<void *>( wglGetCurrentContext() ) )
		return "the present device or GL's entry points are not ready in this context";
	if ( !v.clipControl )
		return "no depthClipControl on the present device (GL's -1..1 clip z)";
	if ( !v.depthClamp )
		return "no depthClamp on the present device (the hulls' GL_DEPTH_CLAMP)";
	if ( !gl().copyImageSubData )
		return "no glCopyImageSubData (the g-buffer twin)";
	return std::string();
}

bool volMaskRun( const VolMaskRun & r, std::string & why )
{
	using namespace volmask;
	VmState & S = vmState();
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
		S.brokenWhy = "volmask on Vulkan broke: " + w;
		return refuse( S.brokenWhy );
	};
	if ( const std::string no = volMaskRefusal(); !no.empty() )
		return refuse( no );
	if ( r.shapes.empty() || r.shapes.size() % 12 != 0 )
		return refuse( "the shapes are not 12 floats a shape (" + std::to_string( r.shapes.size() ) + " floats)" );
	const int n = int( r.shapes.size() / 12 );
	if ( n > kVmMaxShapes )
		return refuse( std::to_string( n ) + " shapes (the route takes " + std::to_string( kVmMaxShapes ) + ")" );
	// the targets: the owner's shared D24S8 and volMask at the pass's size
	const Shared * tg = sharedFind( r.target );
	const Shared * dsT = sharedFind( r.ds );
	if ( !tg || !tg->tex || tg->fmt != VK_FORMAT_R32_SFLOAT )
		return refuse( "the volume mask is not a shared R32F" );
	if ( !dsT || !dsT->tex || dsT->fmt != VK_FORMAT_D24_UNORM_S8_UINT )
		return refuse( "the opaque depth is not a shared D24S8" );
	if ( tg->w != r.w || tg->h != r.h || dsT->w != r.w || dsT->h != r.h || tg->samples != 1 || dsT->samples != 1 || tg->type != 0
		|| dsT->type != 0 )
		return refuse( "the shared targets are not the pass's size (single-sample 2D)" );
	if ( !r.gbuf )
		return refuse( "no g-buffer" );
	std::string w;
	if ( !fxStatic( w ) || !vmStatic( w ) || !vmSems( w ) )
		return breakOff( w );
	const int pipe = ( ( r.red & 1 ) ? 1 : 0 ) | ( ( r.red & 2 ) ? 2 : 0 ) | ( ( r.red & 8 ) ? 4 : 0 ) | ( ( r.red & 16 ) ? 8 : 0 );
	if ( !vmPipe( pipe, w ) )
		return breakOff( w );
	if ( std::string fw; !fenceDone( S.fence, S.pending, "volmask", fw ) )
		return refuse( fw );
	S.pending = false;
	// the g-buffer's twin (a GL command, before the signal)
	unsigned gbTex = 0;
	if ( !sharedTarget( reinterpret_cast<std::uintptr_t>( &g_vmGbufKey ), 0, r.w, r.h, 1, kRgba32f, gbTex, w ) )
		return refuse( "the g-buffer twin: " + w );
	g.copyImageSubData( r.gbuf, kTexture2D, 0, 0, 0, 0, gbTex, kTexture2D, 0, 0, 0, 0, r.w, r.h, 1 );
	const Shared * gb = sharedFind( reinterpret_cast<std::uintptr_t>( &g_vmGbufKey ) );
	if ( !gb )
		return refuse( "the g-buffer twin is gone" );
	const VkDeviceSize shapeBytes = VkDeviceSize( r.shapes.size() * sizeof( float ) );
	if ( !vmFb( tg, dsT, w ) || !csmBuf( S.ubo, kVmSlice, false, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, w )
		|| !csmBuf( S.shp, shapeBytes, false, VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT, w ) || !vmShapeView( w ) )
		return breakOff( w );
	std::memcpy( S.shp.ptr, r.shapes.data(), std::size_t( shapeBytes ) );
	// the slice: the vertex stage's volClip / volGrow (red 32: 0), the pixel stage's volFull / volProj / volRow / volBias
	std::uint8_t * ubo = static_cast<std::uint8_t *>( S.ubo.ptr );
	std::memset( ubo, 0, std::size_t( kVmSlice ) );
	const int nVs = int( sizeof( kT_volmaskVs ) / sizeof( kT_volmaskVs[0] ) );
	const int nPs = int( sizeof( kT_volmaskPs ) / sizeof( kT_volmaskPs[0] ) );
	const float grow = ( r.red & 32 ) ? 0.0f : r.grow;
	const int full[2] = { r.w, r.h };
	if ( !vmPut( ubo + kVmVsAt, kT_volmaskVs, nVs, "volClip", r.clip, w ) || !vmPut( ubo + kVmVsAt, kT_volmaskVs, nVs, "volGrow", &grow, w )
		|| !vmPut( ubo + kVmPsAt, kT_volmaskPs, nPs, "volFull", full, w ) || !vmPut( ubo + kVmPsAt, kT_volmaskPs, nPs, "volProj", r.proj, w )
		|| !vmPut( ubo + kVmPsAt, kT_volmaskPs, nPs, "volRow", r.row, w ) || !vmPut( ubo + kVmPsAt, kT_volmaskPs, nPs, "volBias", &r.bias, w ) )
		return breakOff( w );
	// the set: the two slices, the shapes' view in both stages, the g-buffer twin
	const VkDescriptorBufferInfo bi[2] = { { S.ubo.buf, kVmVsAt, VkDeviceSize( kT_volmaskVsBytes ) },
		{ S.ubo.buf, kVmPsAt, VkDeviceSize( kT_volmaskPsBytes ) } };
	const VkDescriptorImageInfo ii[2] = { { VK_NULL_HANDLE, gb->view, VK_IMAGE_LAYOUT_GENERAL }, { S.plain, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_UNDEFINED } };
	VkWriteDescriptorSet ws[6] {};
	const std::uint32_t bind[6] = { 287, 31, 200, 64, 65, 129 };
	for ( int k = 0; k < 6; k++ ) {
		ws[k].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		ws[k].dstSet = S.set;
		ws[k].dstBinding = bind[k];
		ws[k].descriptorCount = 1;
		if ( k < 2 ) {
			ws[k].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			ws[k].pBufferInfo = &bi[k];
		} else if ( k < 4 ) {
			ws[k].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
			ws[k].pTexelBufferView = &S.shpView;
		} else {
			ws[k].descriptorType = k == 4 ? VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE : VK_DESCRIPTOR_TYPE_SAMPLER;
			ws[k].pImageInfo = &ii[k - 4];
		}
	}
	v.vkUpdateDescriptorSets( v.dev, 6, ws, 0, nullptr );
	S.jobPipe = pipe;
	S.jobInstances = std::uint32_t( ( r.red & 4 ) ? n - 1 : n );
	// GL's targets and the twin signalled -> the worker's job -> GL waits
	std::vector<GLuint> texNames { tg->tex, dsT->tex, gb->tex };
	std::sort( texNames.begin(), texNames.end() );
	texNames.erase( std::unique( texNames.begin(), texNames.end() ), texNames.end() );
	std::vector<GLenum> lay( texNames.size(), kLayoutGeneral );
	g.signalSemaphore( S.glDoneGl, 0, nullptr, GLuint( texNames.size() ), texNames.data(), lay.data() );
	g.flush();
	if ( !opWorkerRun( vmWork, w ) )
		return breakOff( w );  // nothing signals vkDone: GL does not wait it; GL draws the mask
	g.waitSemaphore( S.vkDoneGl, 0, nullptr, GLuint( texNames.size() ), texNames.data(), lay.data() );
	S.pending = true;
	const double ms = nowMs() - t0;
	S.st.runs++;
	S.st.draws++;
	S.st.shapes += n;
	S.st.twinCopies++;
	S.st.cpuMs += ms;
	S.st.maxMs = std::max( S.st.maxMs, ms );
	return true;
}

void volMaskOnGl( const char * why )
{
	volmask::VmState & S = volmask::vmState();
	S.st.gl++;
	S.st.glWhy = why ? why : "";
}

VolMaskStat takeVolMaskStat()
{
	volmask::VmState & S = volmask::vmState();
	VolMaskStat s = S.st;
	if ( S.broken && s.failWhy.empty() )
		s.failWhy = S.brokenWhy;
	const int pipes = S.st.pipelines;
	const double build = S.st.buildMs;
	S.st = VolMaskStat();
	S.st.pipelines = pipes;
	S.st.buildMs = build;
	return s;
}
}  // namespace WwVkPresent
