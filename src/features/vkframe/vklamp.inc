/* Lane VKLAMP1 (2026-11-15, VKFAR7 item 3 slice 1): the lamps' plain shadow cubes on the present device (vklamp.h). Included
 * once by vkpresent.cpp at its end (csmBuf / CsmBuf, exportSem, fenceDone, sharedFind and the worker of the anonymous
 * namespace). GL's pass (celllamps.cpp plain path): per layer glClear depth 1, then the casters with no cull, no polygon
 * offset, depth LEQUAL with writes, no blend / scissor, viewport 0 0 face face, gl_FragDepth = |rel| / radius. Here: one
 * render pass an op in the ops' order (CLEAR or LOAD, a dependency orders it after the last one), the face block (rel, clip,
 * radius / near) at a 256-byte dynamic offset, the casters' commands by baseInstance (their matrices), GL's clip z kept
 * (negativeOneToOne). The pool is CPU arrays (GL streams client arrays here): copied into host buffers every run. */
#include "features/vkframe/vklamp.h"
#include "features/vkframe/lamp_view_vs.spv.h"  // vkframe_spirv.sh's fxv / fx lines
#include "features/vkframe/lamp_view_ps.spv.h"

namespace
{
constexpr VkDeviceSize kLampFaceBytes = 256;  // one face block (136 bytes used), every device's offset alignment
constexpr std::uint32_t kLampCmdBytes = 20;   // VkDrawIndexedIndirectCommand

struct LampFb
{
	std::uintptr_t key = 0;
	int layer = 0, w = 0, h = 0;
	VkImageView view = VK_NULL_HANDLE;
	VkFramebuffer fb = VK_NULL_HANDLE;
};

struct Lamp
{
	bool made = false, broken = false;
	std::string brokenWhy;
	VkDescriptorSetLayout dsl = VK_NULL_HANDLE;
	VkDescriptorPool dp = VK_NULL_HANDLE;
	VkDescriptorSet set = VK_NULL_HANDLE;
	VkBuffer setBuf = VK_NULL_HANDLE;  // the face buffer the set points at
	VkPipelineLayout layout = VK_NULL_HANDLE;
	VkShaderModule vs = VK_NULL_HANDLE, ps = VK_NULL_HANDLE;
	VkRenderPass pass[2] = { VK_NULL_HANDLE, VK_NULL_HANDLE };  // 0 clears the layer to 1, 1 keeps it
	VkPipeline pipe = VK_NULL_HANDLE;
	VkSemaphore glDone = VK_NULL_HANDLE, vkDone = VK_NULL_HANDLE;
	GLuint glDoneGl = 0, vkDoneGl = 0;
	void * semCtx = nullptr;
	VkCommandBuffer cmd = VK_NULL_HANDLE;
	VkFence fence = VK_NULL_HANDLE;
	bool pending = false;
	CsmBuf pos, idx, mats, cmds, faces;
	std::vector<LampFb> fbs;
	std::uint64_t fbGen = 0;
	std::vector<WwVkPresent::LampOp> ops;
	std::vector<int> opFb;  // per op: its framebuffer in fbs
	int pipelines = 0;
	double buildMs = 0.0;
	WwVkPresent::LampStat st;
};

Lamp & lampS()
{
	static Lamp s;
	return s;
}

// the device-time parts (once): the face block's set, the layout, both modules, the two compatible render passes, the
// pipeline, the command buffer and its fence
bool lampStatic( std::string & why )
{
	Vk & v = vk();
	Lamp & S = lampS();
	if ( S.made )
		return true;
	const double t0 = nowMs();
	VkDescriptorSetLayoutBinding b {};
	b.binding = 1;  // b1, fx()'s shift 0
	b.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
	b.descriptorCount = 1;
	b.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
	VkDescriptorSetLayoutCreateInfo dci { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
	dci.bindingCount = 1;
	dci.pBindings = &b;
	const VkDescriptorPoolSize ps { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1 };
	VkDescriptorPoolCreateInfo pci { VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
	pci.maxSets = 1;
	pci.poolSizeCount = 1;
	pci.pPoolSizes = &ps;
	if ( v.vkCreateDescriptorSetLayout( v.dev, &dci, nullptr, &S.dsl ) != VK_SUCCESS
		|| v.vkCreateDescriptorPool( v.dev, &pci, nullptr, &S.dp ) != VK_SUCCESS ) {
		why = "the lamp cubes' set layout / pool failed";
		return false;
	}
	VkDescriptorSetAllocateInfo sai { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
	sai.descriptorPool = S.dp;
	sai.descriptorSetCount = 1;
	sai.pSetLayouts = &S.dsl;
	VkPipelineLayoutCreateInfo plci { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
	plci.setLayoutCount = 1;
	plci.pSetLayouts = &S.dsl;
	VkShaderModuleCreateInfo mv { VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
	mv.codeSize = sizeof( vkframe_lamp_view_vs_spv );
	mv.pCode = vkframe_lamp_view_vs_spv;
	VkShaderModuleCreateInfo mp { VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
	mp.codeSize = sizeof( vkframe_lamp_view_ps_spv );
	mp.pCode = vkframe_lamp_view_ps_spv;
	if ( v.vkAllocateDescriptorSets( v.dev, &sai, &S.set ) != VK_SUCCESS
		|| v.vkCreatePipelineLayout( v.dev, &plci, nullptr, &S.layout ) != VK_SUCCESS
		|| v.vkCreateShaderModule( v.dev, &mv, nullptr, &S.vs ) != VK_SUCCESS
		|| v.vkCreateShaderModule( v.dev, &mp, nullptr, &S.ps ) != VK_SUCCESS ) {
		why = "the lamp cubes' set / pipeline layout / shader modules failed";
		return false;
	}
	// ops run in order on the same layers: each pass waits the depth writes of the one before (and GL's, by the semaphore)
	VkSubpassDependency dep[2] {};
	const VkPipelineStageFlags dsStages = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
	const VkAccessFlags dsRw = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	dep[0] = { VK_SUBPASS_EXTERNAL, 0, dsStages, dsStages, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, dsRw, 0 };
	dep[1] = { 0, VK_SUBPASS_EXTERNAL, dsStages, dsStages, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, dsRw, 0 };
	for ( int k = 0; k < 2; k++ ) {
		VkAttachmentDescription at {};
		at.format = VK_FORMAT_D16_UNORM;
		at.samples = VK_SAMPLE_COUNT_1_BIT;
		at.loadOp = k ? VK_ATTACHMENT_LOAD_OP_LOAD : VK_ATTACHMENT_LOAD_OP_CLEAR;
		at.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		at.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		at.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		at.initialLayout = at.finalLayout = VK_IMAGE_LAYOUT_GENERAL;
		const VkAttachmentReference dr { 0, VK_IMAGE_LAYOUT_GENERAL };
		VkSubpassDescription sp {};
		sp.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		sp.pDepthStencilAttachment = &dr;
		VkRenderPassCreateInfo rci { VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO };
		rci.attachmentCount = 1;
		rci.pAttachments = &at;
		rci.subpassCount = 1;
		rci.pSubpasses = &sp;
		rci.dependencyCount = 2;
		rci.pDependencies = dep;
		if ( v.vkCreateRenderPass( v.dev, &rci, nullptr, &S.pass[k] ) != VK_SUCCESS ) {
			why = "vkCreateRenderPass (the lamp cubes) failed";
			return false;
		}
	}
	VkPipelineShaderStageCreateInfo st[2] { { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO },
		{ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO } };
	st[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
	st[0].module = S.vs;
	st[0].pName = "main";
	st[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	st[1].module = S.ps;
	st[1].pName = "main";
	// binding 0: the pool's vec3 positions; binding 1: one model-view matrix per caster (baseInstance), GL columns at 1-4
	const VkVertexInputBindingDescription vb[2] = { { 0, 12, VK_VERTEX_INPUT_RATE_VERTEX }, { 1, 64, VK_VERTEX_INPUT_RATE_INSTANCE } };
	const VkVertexInputAttributeDescription va[5] = { { 0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0 },
		{ 1, 1, VK_FORMAT_R32G32B32A32_SFLOAT, 0 }, { 2, 1, VK_FORMAT_R32G32B32A32_SFLOAT, 16 },
		{ 3, 1, VK_FORMAT_R32G32B32A32_SFLOAT, 32 }, { 4, 1, VK_FORMAT_R32G32B32A32_SFLOAT, 48 } };
	VkPipelineVertexInputStateCreateInfo vi { VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
	vi.vertexBindingDescriptionCount = 2;
	vi.pVertexBindingDescriptions = vb;
	vi.vertexAttributeDescriptionCount = 5;
	vi.pVertexAttributeDescriptions = va;
	VkPipelineInputAssemblyStateCreateInfo ia { VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
	ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	VkPipelineViewportDepthClipControlCreateInfoEXT cc { VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_DEPTH_CLIP_CONTROL_CREATE_INFO_EXT };
	cc.negativeOneToOne = VK_TRUE;  // GL's clip z in [-w, w], as the face projection writes it
	VkPipelineViewportStateCreateInfo vps { VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO };
	vps.pNext = &cc;
	vps.viewportCount = vps.scissorCount = 1;
	VkPipelineRasterizationStateCreateInfo rs { VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
	rs.polygonMode = VK_POLYGON_MODE_FILL;
	rs.cullMode = VK_CULL_MODE_NONE;  // both sides cast, as GL's pass
	rs.frontFace = VK_FRONT_FACE_CLOCKWISE;
	rs.lineWidth = 1.0f;
	VkPipelineMultisampleStateCreateInfo ms { VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
	ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	VkPipelineDepthStencilStateCreateInfo ds { VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };
	ds.depthTestEnable = VK_TRUE;
	ds.depthWriteEnable = VK_TRUE;
	ds.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
	VkPipelineColorBlendStateCreateInfo cbs { VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
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
	gp.layout = S.layout;
	gp.renderPass = S.pass[1];  // the clear pass is compatible (only the load op differs)
	if ( v.vkCreateGraphicsPipelines( v.dev, tone().cache, 1, &gp, nullptr, &S.pipe ) != VK_SUCCESS ) {
		why = "vkCreateGraphicsPipelines (the lamp cubes) failed";
		return false;
	}
	VkFenceCreateInfo fi { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
	VkCommandBufferAllocateInfo cai { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
	cai.commandPool = v.pool;
	cai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	cai.commandBufferCount = 1;
	if ( v.vkCreateFence( v.dev, &fi, nullptr, &S.fence ) != VK_SUCCESS || v.vkAllocateCommandBuffers( v.dev, &cai, &S.cmd ) != VK_SUCCESS ) {
		why = "the lamp cubes' fence / command buffer failed";
		return false;
	}
	S.pipelines = 1;
	S.buildMs = nowMs() - t0;
	toneCacheSave();
	S.made = true;
	return true;
}

void lampSemsDrop( bool glAlive )
{
	Vk & v = vk();
	Gl & g = gl();
	Lamp & S = lampS();
	if ( glAlive )
		for ( GLuint * s : { &S.glDoneGl, &S.vkDoneGl } )
			if ( *s )
				g.deleteSemaphores( 1, s );
	for ( VkSemaphore * s : { &S.glDone, &S.vkDone } )
		if ( *s )
			v.vkDestroySemaphore( v.dev, *s, nullptr );
	S.glDoneGl = S.vkDoneGl = 0;
	S.glDone = S.vkDone = VK_NULL_HANDLE;
	S.semCtx = nullptr;
}

bool lampSems( std::string & why )
{
	Gl & g = gl();
	Lamp & S = lampS();
	if ( S.glDone && S.semCtx == g.ctx )
		return true;
	lampSemsDrop( false );  // another context's names: that context went
	if ( !exportSem( S.glDone, S.glDoneGl, why ) || !exportSem( S.vkDone, S.vkDoneGl, why ) ) {
		lampSemsDrop( true );
		return false;
	}
	S.semCtx = g.ctx;
	return true;
}

void lampFbsDrop()
{
	Vk & v = vk();
	Lamp & S = lampS();
	for ( LampFb & f : S.fbs ) {
		if ( f.fb )
			v.vkDestroyFramebuffer( v.dev, f.fb, nullptr );
		if ( f.view )
			v.vkDestroyImageView( v.dev, f.view, nullptr );
	}
	S.fbs.clear();
}

// the framebuffer over one layer of the shared cube array (kept until a shared target is made or goes: sharedGen)
int lampFb( std::uintptr_t key, int layer, std::string & why )
{
	Vk & v = vk();
	Lamp & S = lampS();
	for ( std::size_t k = 0; k < S.fbs.size(); k++ )
		if ( S.fbs[k].key == key && S.fbs[k].layer == layer )
			return int( k );
	const Shared * sh = sharedFind( key );
	if ( !sh || !sh->img || sh->fmt != VK_FORMAT_D16_UNORM || sh->type != 3 ) {
		why = "the lamp cubes are not a shared D16 cube array";
		return -1;
	}
	LampFb f;
	f.key = key;
	f.layer = layer;
	f.w = sh->w;
	f.h = sh->h;
	VkImageViewCreateInfo ci { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
	ci.image = sh->img;
	ci.viewType = VK_IMAGE_VIEW_TYPE_2D;
	ci.format = VK_FORMAT_D16_UNORM;
	ci.subresourceRange = { VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, std::uint32_t( layer ), 1 };
	if ( v.vkCreateImageView( v.dev, &ci, nullptr, &f.view ) != VK_SUCCESS ) {
		why = "a lamp cube layer's image view failed";
		return -1;
	}
	VkFramebufferCreateInfo fci { VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
	fci.renderPass = S.pass[1];
	fci.attachmentCount = 1;
	fci.pAttachments = &f.view;
	fci.width = std::uint32_t( f.w );
	fci.height = std::uint32_t( f.h );
	fci.layers = 1;
	if ( v.vkCreateFramebuffer( v.dev, &fci, nullptr, &f.fb ) != VK_SUCCESS ) {
		v.vkDestroyImageView( v.dev, f.view, nullptr );
		why = "a lamp cube layer's framebuffer failed";
		return -1;
	}
	S.fbs.push_back( f );
	return int( S.fbs.size() ) - 1;
}

// the worker: the ops in order, one render pass each, GL's signal waited, signalled back, with the fence
bool lampWork( std::string & why )
{
	Vk & v = vk();
	Lamp & S = lampS();
	v.vkResetFences( v.dev, 1, &S.fence );
	v.vkResetCommandBuffer( S.cmd, 0 );
	VkCommandBufferBeginInfo bi { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
	bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	v.vkBeginCommandBuffer( S.cmd, &bi );
	const VkBuffer vbs[2] = { S.pos.buf, S.mats.buf };
	const VkDeviceSize vbo[2] = { 0, 0 };
	v.vkCmdBindPipeline( S.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, S.pipe );
	v.vkCmdBindVertexBuffers( S.cmd, 0, 2, vbs, vbo );
	v.vkCmdBindIndexBuffer( S.cmd, S.idx.buf, 0, VK_INDEX_TYPE_UINT16 );
	for ( std::size_t o = 0; o < S.ops.size(); o++ ) {
		const WwVkPresent::LampOp & op = S.ops[o];
		const LampFb & F = S.fbs[std::size_t( S.opFb[o] )];
		VkClearValue cv {};
		cv.depthStencil.depth = 1.0f;
		VkRenderPassBeginInfo rbi { VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
		rbi.renderPass = S.pass[op.clear ? 0 : 1];
		rbi.framebuffer = F.fb;
		rbi.renderArea = { { 0, 0 }, { std::uint32_t( F.w ), std::uint32_t( F.h ) } };
		rbi.clearValueCount = 1;
		rbi.pClearValues = &cv;
		v.vkCmdBeginRenderPass( S.cmd, &rbi, VK_SUBPASS_CONTENTS_INLINE );
		if ( op.n ) {
			const VkViewport vp { 0.0f, 0.0f, float( F.w ), float( F.h ), 0.0f, 1.0f };
			const VkRect2D sc { { 0, 0 }, { std::uint32_t( F.w ), std::uint32_t( F.h ) } };
			v.vkCmdSetViewport( S.cmd, 0, 1, &vp );
			v.vkCmdSetScissor( S.cmd, 0, 1, &sc );
			const std::uint32_t off = std::uint32_t( VkDeviceSize( o ) * kLampFaceBytes );
			v.vkCmdBindDescriptorSets( S.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, S.layout, 0, 1, &S.set, 1, &off );
			if ( v.multiDrawIndirect ) {
				for ( std::uint32_t at = 0; at < op.n; at += 65535u )
					v.vkCmdDrawIndexedIndirect( S.cmd, S.cmds.buf, VkDeviceSize( op.first + at ) * kLampCmdBytes,
						std::min( op.n - at, 65535u ), kLampCmdBytes );
			} else {
				for ( std::uint32_t at = 0; at < op.n; at++ )
					v.vkCmdDrawIndexedIndirect( S.cmd, S.cmds.buf, VkDeviceSize( op.first + at ) * kLampCmdBytes, 1, kLampCmdBytes );
			}
		}
		v.vkCmdEndRenderPass( S.cmd );
	}
	if ( v.vkEndCommandBuffer( S.cmd ) != VK_SUCCESS ) {
		why = "vkEndCommandBuffer (the lamp cubes) failed";
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
		why = "vkQueueSubmit (the lamp cubes) returned " + std::to_string( int( r ) );
		return false;
	}
	return true;
}

// shutdown (vkpresent.cpp, before the shared targets go): the layer views, the semaphores, the buffers
void lampDrop()
{
	Lamp & S = lampS();
	if ( !S.glDone && !S.pending && S.fbs.empty() && !S.faces.buf && !S.pos.buf )
		return;
	drain();
	vk().vkDeviceWaitIdle( vk().dev );
	S.pending = false;
	lampFbsDrop();
	S.fbGen = 0;
	for ( CsmBuf * b : { &S.pos, &S.idx, &S.mats, &S.cmds, &S.faces } )
		csmBufFree( *b, false );  // host buffers: no GL names
	S.setBuf = VK_NULL_HANDLE;
	lampSemsDrop( gl().ok && S.semCtx == gl().ctx );
}
}  // namespace

namespace WwVkPresent
{
std::string lampRefusal()
{
	Vk & v = vk();
	Gl & g = gl();
	if ( lampS().broken )
		return lampS().brokenWhy;
	if ( !v.ok || !g.ok )
		return "the present device or GL's entry points are not ready";
	if ( !g.texStorageMem3D )
		return "no glTexStorageMem3DEXT (the shared cube array)";
	if ( !v.firstInstance )
		return "no drawIndirectFirstInstance (the casters' matrices by baseInstance)";
	if ( !v.clipControl )
		return "no VK_EXT_depth_clip_control (GL's clip z in [-w, w])";
	return std::string();
}

bool lampRun( const LampRun & t, std::string & why )
{
	Lamp & S = lampS();
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
		S.brokenWhy = "the lamp cubes on Vulkan broke: " + w;
		return refuse( S.brokenWhy );
	};
	if ( S.broken )
		return refuse( S.brokenWhy );
	if ( !v.ok || !g.ok || g.ctx != reinterpret_cast<void *>( wglGetCurrentContext() ) )
		return refuse( "the present device or GL's entry points are not ready in this context" );
	if ( const std::string no = lampRefusal(); !no.empty() )
		return refuse( no );
	if ( t.ops.empty() )
		return true;
	if ( !t.tex || !t.key || !sharedTargetIs( t.key, t.tex ) )
		return refuse( "the lamp cubes are not the shared cube array" );
	const Shared * sh = sharedFind( t.key );
	if ( !sh || sh->w != t.face || sh->h != t.face || sh->d != t.layers )
		return refuse( "the lamp cubes' size is not the shared array's" );
	std::size_t draws = 0;
	for ( const LampOp & op : t.ops ) {
		if ( op.layer < 0 || op.layer >= t.layers )
			return refuse( "an op names no layer of the cube array" );
		if ( std::size_t( op.first ) + op.n > t.nCmds )
			return refuse( "an op's commands are out of range" );
		draws += op.n;
	}
	if ( draws && ( !t.pos || !t.idx || !t.posBytes || !t.idxBytes || !t.mats || !t.nMats || !t.cmds ) )
		return refuse( "no pool, matrices or commands" );
	// the last run is done before its command buffer, buffers and framebuffers are touched again
	if ( std::string fw; !fenceDone( S.fence, S.pending, "lamp", fw ) )
		return refuse( fw );
	S.pending = false;
	std::string w;
	if ( !lampStatic( w ) || !lampSems( w ) )
		return breakOff( w );
	if ( S.fbGen != sharedGen() ) {
		lampFbsDrop();
		S.fbGen = sharedGen();
	}
	S.ops = t.ops;
	S.opFb.assign( t.ops.size(), -1 );
	for ( std::size_t k = 0; k < t.ops.size(); k++ ) {
		S.opFb[k] = lampFb( t.key, t.ops[k].layer, w );
		if ( S.opFb[k] < 0 )
			return breakOff( w );
	}
	if ( !csmBuf( S.pos, VkDeviceSize( std::max<std::size_t>( t.posBytes, 12 ) ), false, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, w )
		|| !csmBuf( S.idx, VkDeviceSize( std::max<std::size_t>( t.idxBytes, 2 ) ), false, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, w )
		|| !csmBuf( S.mats, VkDeviceSize( std::max<std::size_t>( t.nMats, 1 ) ) * 64, false, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, w )
		|| !csmBuf( S.cmds, VkDeviceSize( std::max<std::size_t>( t.nCmds, 1 ) ) * kLampCmdBytes, false,
			VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT, w )
		|| !csmBuf( S.faces, VkDeviceSize( t.ops.size() ) * kLampFaceBytes, false, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, w ) )
		return breakOff( w );
	if ( draws ) {
		std::memcpy( S.pos.ptr, t.pos, t.posBytes );
		std::memcpy( S.idx.ptr, t.idx, t.idxBytes );
		std::memcpy( S.mats.ptr, t.mats, t.nMats * 64 );
		std::memcpy( S.cmds.ptr, t.cmds, t.nCmds * kLampCmdBytes );
	}
	for ( std::size_t k = 0; k < t.ops.size(); k++ ) {
		char * p = static_cast<char *>( S.faces.ptr ) + k * kLampFaceBytes;
		std::memcpy( p, t.ops[k].rel, 64 );
		std::memcpy( p + 64, t.ops[k].clip, 64 );
		std::memcpy( p + 128, t.ops[k].rn, 8 );
	}
	if ( S.setBuf != S.faces.buf ) {
		VkDescriptorBufferInfo dbi { S.faces.buf, 0, 136 };
		VkWriteDescriptorSet wr { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
		wr.dstSet = S.set;
		wr.dstBinding = 1;
		wr.descriptorCount = 1;
		wr.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
		wr.pBufferInfo = &dbi;
		v.vkUpdateDescriptorSets( v.dev, 1, &wr, 0, nullptr );
		S.setBuf = S.faces.buf;
	}
	// GL's pass so far -> the worker -> GL's readers (and GL's alpha-tested casters after it)
	GLuint tex[1] = { GLuint( t.tex ) };
	GLenum lay[1] = { kLayoutGeneral };
	g.signalSemaphore( S.glDoneGl, 0, nullptr, 1, tex, lay );
	g.flush();
	Worker & W = wk();
	std::uint64_t want = 0;
	{
		std::lock_guard<std::mutex> lk( W.m );
		want = W.toneDone + 1;
	}
	Work j;
	j.k = -1;
	j.fn = lampWork;
	hand( j );
	bool ok = false;
	{
		std::unique_lock<std::mutex> lk( W.m );
		W.idle.wait( lk, [&W, want]() { return W.toneDone >= want; } );
		ok = W.toneOk;
		w = W.toneWhy;
	}
	if ( !ok )
		return breakOff( w );  // nothing signals vkDone: GL does not wait it; GL draws the pass
	g.waitSemaphore( S.vkDoneGl, 0, nullptr, 1, tex, lay );
	S.pending = true;
	const double ms = nowMs() - t0;
	S.st.runs++;
	S.st.ops += int( t.ops.size() );
	S.st.draws += int( draws );
	S.st.cpuMs += ms;
	S.st.maxMs = std::max( S.st.maxMs, ms );
	return true;
}

void lampOnGl( const char * why )
{
	Lamp & S = lampS();
	S.st.gl++;
	S.st.glWhy = why ? why : "";
}

LampStat takeLampStat()
{
	Lamp & S = lampS();
	LampStat s = S.st;
	s.pipelines = S.pipelines;
	s.buildMs = S.buildMs;
	if ( S.broken && s.failWhy.empty() )
		s.failWhy = S.brokenWhy;
	S.st = LampStat();
	return s;
}
}  // namespace WwVkPresent
