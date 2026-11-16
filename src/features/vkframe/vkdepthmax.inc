/* lane VKSMALL1 (2026-11-16): the frame's farthest depth per pixel (cellhdr.cpp depthMaxMake, cell_depthmax) on the
 * present device, through the VKAIR1 crossing (fx(): its fence, command buffer and semaphores); fxWork's kind 14.
 * GL: an R32F fbo at the frame's size, no depth / stencil / blend / cull / scissor, one quad, depthMs = the frame's
 * multisample depth in DEPTH_COMPONENT mode. Here: one render pass over the route's shared R32F twin (every texel written:
 * DONT_CARE), the tone frame's depth read in place through a depth-aspect view, one full-screen triangle. */
namespace
{
constexpr int kDmBindings = 2;  // b31; t64 depthMs

struct DepthMax
{
	bool made = false, broken = false;
	std::string brokenWhy;
	VkDescriptorSetLayout dsl = VK_NULL_HANDLE;
	VkDescriptorPool dpool = VK_NULL_HANDLE;
	VkDescriptorSet set = VK_NULL_HANDLE;
	VkPipelineLayout layout = VK_NULL_HANDLE;
	VkShaderModule ps = VK_NULL_HANDLE;
	VkBuffer ubo = VK_NULL_HANDLE;
	VkDeviceMemory uboMem = VK_NULL_HANDLE;
	std::uint8_t * uboPtr = nullptr;
	VkRenderPass pass = VK_NULL_HANDLE, passClear = VK_NULL_HANDLE;  // DONT_CARE (the route), CLEAR (red 2: same compatibility)
	VkPipeline pipe = VK_NULL_HANDLE;
	VkFramebuffer fb = VK_NULL_HANDLE;
	VkImage msImg = VK_NULL_HANDLE;  // the tone frame's depth, its depth-aspect view for depthMs
	VkImageView msView = VK_NULL_HANDLE;
	std::uint64_t msGen = 0;
	int pipelines = 0;
	double buildMs = 0.0;
	// this run's job (the drawing thread writes it before the hand, the worker records it)
	int w = 0, h = 0;
	bool clear = false;
	WwVkPresent::FxStat st;
};

DepthMax & depthMax()
{
	static DepthMax d;
	return d;
}

bool dmStatic( std::string & why )
{
	Vk & v = vk();
	DepthMax & D = depthMax();
	if ( D.made )
		return true;
	const double t0 = nowMs();
	VkDescriptorSetLayoutBinding b[kDmBindings] = {};
	b[0].binding = 31;
	b[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	b[1].binding = 64;
	b[1].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
	for ( VkDescriptorSetLayoutBinding & x : b ) {
		x.descriptorCount = 1;
		x.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	}
	VkDescriptorSetLayoutCreateInfo lci { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
	lci.bindingCount = kDmBindings;
	lci.pBindings = b;
	VkDescriptorPoolSize ps[2] = { { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1 }, { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1 } };
	VkDescriptorPoolCreateInfo pci { VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
	pci.maxSets = 1;
	pci.poolSizeCount = 2;
	pci.pPoolSizes = ps;
	if ( v.vkCreateDescriptorSetLayout( v.dev, &lci, nullptr, &D.dsl ) != VK_SUCCESS
		|| v.vkCreateDescriptorPool( v.dev, &pci, nullptr, &D.dpool ) != VK_SUCCESS ) {
		why = "the depth max's descriptor set layout / pool failed";
		return false;
	}
	VkDescriptorSetAllocateInfo dai { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
	dai.descriptorPool = D.dpool;
	dai.descriptorSetCount = 1;
	dai.pSetLayouts = &D.dsl;
	VkPipelineLayoutCreateInfo plci { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
	plci.setLayoutCount = 1;
	plci.pSetLayouts = &D.dsl;
	if ( v.vkAllocateDescriptorSets( v.dev, &dai, &D.set ) != VK_SUCCESS
		|| v.vkCreatePipelineLayout( v.dev, &plci, nullptr, &D.layout ) != VK_SUCCESS ) {
		why = "the depth max's descriptor set / pipeline layout failed";
		return false;
	}
	if ( !fxModule( vkframe_depthmax_view_ps_spv, sizeof( vkframe_depthmax_view_ps_spv ), D.ps ) ) {
		why = "the depth max's shader module failed";
		return false;
	}
	VkBufferCreateInfo bci { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
	bci.size = kFxLooseAt + VkDeviceSize( WwVkPresent::kT_depthmaxBytes );
	bci.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
	bool mapped = v.vkCreateBuffer( v.dev, &bci, nullptr, &D.ubo ) == VK_SUCCESS;
	if ( mapped ) {
		VkMemoryRequirements br {};
		v.vkGetBufferMemoryRequirements( v.dev, D.ubo, &br );
		VkMemoryAllocateInfo ai { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
		ai.allocationSize = br.size;
		ai.memoryTypeIndex = memType( br.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT );
		void * p = nullptr;
		mapped = ai.memoryTypeIndex != UINT32_MAX && v.vkAllocateMemory( v.dev, &ai, nullptr, &D.uboMem ) == VK_SUCCESS
			&& v.vkBindBufferMemory( v.dev, D.ubo, D.uboMem, 0 ) == VK_SUCCESS && v.vkMapMemory( v.dev, D.uboMem, 0, VK_WHOLE_SIZE, 0, &p ) == VK_SUCCESS;
		D.uboPtr = static_cast<std::uint8_t *>( p );
	}
	if ( !mapped || !D.uboPtr ) {
		why = "the depth max's uniform buffer failed";
		return false;
	}
	std::memset( D.uboPtr, 0, size_t( bci.size ) );
	// the render passes (R32F, one sample, STORE, GENERAL): DONT_CARE for the route (the triangle writes every texel),
	// CLEAR for red 2 (compatible: one pipeline)
	for ( int c = 0; c < 2; c++ ) {
		VkAttachmentDescription at {};
		at.format = VK_FORMAT_R32_SFLOAT;
		at.samples = VK_SAMPLE_COUNT_1_BIT;
		at.loadOp = c ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		at.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		at.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		at.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		at.initialLayout = at.finalLayout = VK_IMAGE_LAYOUT_GENERAL;
		const VkAttachmentReference cr { 0, VK_IMAGE_LAYOUT_GENERAL };
		VkSubpassDescription sp {};
		sp.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		sp.colorAttachmentCount = 1;
		sp.pColorAttachments = &cr;
		VkRenderPassCreateInfo rci { VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO };
		rci.attachmentCount = 1;
		rci.pAttachments = &at;
		rci.subpassCount = 1;
		rci.pSubpasses = &sp;
		if ( v.vkCreateRenderPass( v.dev, &rci, nullptr, c ? &D.passClear : &D.pass ) != VK_SUCCESS ) {
			why = "vkCreateRenderPass (the depth max) failed";
			return false;
		}
	}
	Fx & F = fx();
	VkPipelineShaderStageCreateInfo st[2] = { { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO },
		{ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO } };
	st[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
	st[0].module = F.vs;  // the full-screen triangle (tone_view_vs); GL's quad covers the same texels
	st[0].pName = "main";
	st[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	st[1].module = D.ps;
	st[1].pName = "main";
	VkPipelineVertexInputStateCreateInfo vi { VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
	VkPipelineInputAssemblyStateCreateInfo ia { VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
	ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	VkPipelineViewportStateCreateInfo vps { VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO };
	vps.viewportCount = vps.scissorCount = 1;
	VkPipelineRasterizationStateCreateInfo rs { VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
	rs.polygonMode = VK_POLYGON_MODE_FILL;
	rs.cullMode = VK_CULL_MODE_NONE;
	rs.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	rs.lineWidth = 1.0f;
	VkPipelineMultisampleStateCreateInfo ms { VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
	ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	VkPipelineColorBlendAttachmentState cb {};
	cb.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	VkPipelineColorBlendStateCreateInfo cbs { VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
	cbs.attachmentCount = 1;
	cbs.pAttachments = &cb;
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
	gp.pColorBlendState = &cbs;
	gp.pDynamicState = &dy;
	gp.layout = D.layout;
	gp.renderPass = D.pass;
	if ( v.vkCreateGraphicsPipelines( v.dev, tone().cache, 1, &gp, nullptr, &D.pipe ) != VK_SUCCESS ) {
		why = "vkCreateGraphicsPipelines (the depth max) failed";
		return false;
	}
	D.pipelines++;
	toneCacheSave();
	D.buildMs += nowMs() - t0;
	D.made = true;
	return true;
}

// the worker (fxWork, F.kind 14): every texel of the twin
bool dmRecord( std::string & why )
{
	Vk & v = vk();
	Fx & F = fx();
	DepthMax & D = depthMax();
	if ( !D.fb || !D.pipe ) {
		why = "no framebuffer / pipeline for the depth max";
		return false;
	}
	VkClearValue clear {};
	VkRenderPassBeginInfo rbi { VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
	rbi.renderPass = D.clear ? D.passClear : D.pass;
	rbi.framebuffer = D.fb;
	rbi.renderArea = { { 0, 0 }, { std::uint32_t( D.w ), std::uint32_t( D.h ) } };
	rbi.clearValueCount = 1;
	rbi.pClearValues = &clear;
	v.vkCmdBeginRenderPass( F.cmd, &rbi, VK_SUBPASS_CONTENTS_INLINE );
	if ( !D.clear ) {
		const VkViewport vpt { 0.0f, 0.0f, float( D.w ), float( D.h ), 0.0f, 1.0f };
		const VkRect2D sc { { 0, 0 }, { std::uint32_t( D.w ), std::uint32_t( D.h ) } };
		v.vkCmdSetViewport( F.cmd, 0, 1, &vpt );
		v.vkCmdSetScissor( F.cmd, 0, 1, &sc );
		v.vkCmdBindPipeline( F.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, D.pipe );
		v.vkCmdBindDescriptorSets( F.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, D.layout, 0, 1, &D.set, 0, nullptr );
		v.vkCmdDraw( F.cmd, 3, 1, 0, 0 );
	}
	v.vkCmdEndRenderPass( F.cmd );
	return true;
}
}  // namespace

namespace WwVkPresent
{
bool depthMaxRun( const DepthMaxRun & r, unsigned & tex, std::string & why )
{
	Fx & F = fx();
	DepthMax & D = depthMax();
	Tone & T = tone();
	Vk & v = vk();
	Gl & g = gl();
	FxStat & S = D.st;
	const double t0 = nowMs();
	tex = 0;
	auto refuse = [&]( const std::string & w ) {
		why = w;
		S.fails++;
		S.failWhy = w;
		return false;
	};
	auto breakOff = [&]( const std::string & w ) {
		D.broken = true;
		D.brokenWhy = "the depth max on Vulkan broke: " + w;
		return refuse( D.brokenWhy );
	};
	if ( D.broken )
		return refuse( D.brokenWhy );
	if ( !v.ok || !g.ok || g.ctx != reinterpret_cast<void *>( wglGetCurrentContext() ) )
		return refuse( "the present device or GL's entry points are not ready in this context" );
	if ( !T.ds || !T.dsTex || r.ds != T.dsTex )
		return refuse( "depthMs is not the shared tone frame's own depth" );
	if ( T.w != r.w || T.h != r.h || r.w < 1 || r.h < 1 )
		return refuse( "the depth max's size is not the tone frame's" );
	if ( r.samples != std::max( T.samples, 1 ) )
		return refuse( "depthSamples is not the tone frame's sample count" );
	// the last crossing (any fx pass) is done before its command buffer, set, framebuffer and buffer are written again
	if ( std::string fw; !fenceDone( F.fence, F.pending, "depthmax", fw ) )
		return refuse( fw );
	F.pending = false;
	std::string w;
	if ( !fxStatic( w ) || !fxSems( w ) || !dmStatic( w ) )
		return breakOff( w );
	// the target: the route's own shared R32F twin at the frame's size (made again on a resize)
	const std::uintptr_t key = reinterpret_cast<std::uintptr_t>( &D );
	unsigned twin = 0;
	if ( !sharedTarget( key, 0, r.w, r.h, 1, 0x822E, twin, w ) || !twin )  // GL_R32F
		return refuse( "the depth max's twin: " + w );
	const Shared * tw = sharedFind( key );
	if ( !tw || !tw->view )
		return refuse( "the depth max's twin has no view" );
	// depthMs: the tone frame's depth read in place through a depth-aspect view (T.dsView is depth + stencil)
	if ( T.ds != D.msImg || D.msGen != sharedGen() ) {
		D.msGen = sharedGen();
		if ( D.msView )
			v.vkDestroyImageView( v.dev, D.msView, nullptr );
		D.msView = imageView( T.ds, VK_IMAGE_VIEW_TYPE_2D, VK_FORMAT_D24_UNORM_S8_UINT, VK_IMAGE_ASPECT_DEPTH_BIT );
		D.msImg = D.msView ? T.ds : VK_NULL_HANDLE;
		if ( !D.msView )
			return breakOff( "the tone frame's depth view failed" );
	}
	if ( !fxFb( D.fb, D.pass, &tw->view, 1, r.w, r.h ) )
		return breakOff( "the depth max's framebuffer over its twin failed" );
	const std::int32_t samples = ( r.red & 1 ) ? 1 : r.samples;  // red 1: sample 0 alone
	std::memcpy( D.uboPtr + kFxLooseAt, &samples, 4 );
	VkDescriptorBufferInfo bi { D.ubo, kFxLooseAt, VkDeviceSize( kT_depthmaxBytes ) };
	VkDescriptorImageInfo ii {};
	ii.imageView = D.msView;
	ii.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	VkWriteDescriptorSet ws[kDmBindings] = { { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET }, { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET } };
	ws[0].dstSet = ws[1].dstSet = D.set;
	ws[0].dstBinding = 31;
	ws[0].descriptorCount = 1;
	ws[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	ws[0].pBufferInfo = &bi;
	ws[1].dstBinding = 64;
	ws[1].descriptorCount = 1;
	ws[1].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
	ws[1].pImageInfo = &ii;
	v.vkUpdateDescriptorSets( v.dev, std::uint32_t( kDmBindings ), ws, 0, nullptr );
	D.w = r.w;
	D.h = r.h;
	D.clear = ( r.red & 2 ) != 0;
	F.kind = 14;
	F.w = r.w;
	F.h = r.h;
	const int vp[4] = { 0, 0, r.w, r.h };
	std::memcpy( F.vp, vp, sizeof( F.vp ) );
	FxNames names;
	names.add( T.dsTex );
	names.add( twin );
	if ( !fxCross( names, w ) )
		return breakOff( w );  // nothing signals vkDone: GL does not wait it; GL draws its own pass this frame
	tex = twin;
	const double ms = nowMs() - t0;
	S.runs++;
	S.cpuMs += ms;
	S.maxMs = std::max( S.maxMs, ms );
	return true;
}

void depthMaxOnGl( const char * why )
{
	FxStat & S = depthMax().st;
	S.gl++;
	S.glWhy = why ? why : "";
}

FxStat takeDepthMaxStat()
{
	DepthMax & D = depthMax();
	FxStat s = D.st;
	s.pipelines = D.pipelines;
	s.buildMs = D.buildMs;
	if ( D.broken && s.failWhy.empty() )
		s.failWhy = D.brokenWhy;
	D.st = FxStat();
	return s;
}
}  // namespace WwVkPresent
