/* lane VKFAR2 (row P4b, 2026-11-14): the fog bloom's own pass (cellfogbloom.cpp, cell_fogbloom) on the present device,
 * through the VKAIR1 crossing (fx(): its fence, command buffer and semaphores). Included once by vkpresent.cpp after
 * vkfardots.inc (it reads farSharedOf and the fx parts of the anonymous namespace).
 * GL: blit the frame's depth NEAREST into a one-sample texture, then into an R8 target at the frame's samples beside the
 * frame's depth / stencil: clear 0, draw 1 (stencil off, fbTranslucent 0), draw 2 (stencil EQUAL 0x04 mask 0x04,
 * fbTranslucent 1); no depth test, blend, cull or scissor. Here: one render pass over the shared R8 target (CLEAR to 0),
 * the same two full-screen draws; fogbloom_view_ps reads the tone frame's depth (sample 0) and stencil per sample itself
 * and writes the second draw's samples through SV_Coverage (no depth / stencil attachment, so no feedback loop). */
namespace
{
constexpr VkDeviceSize kFbSetStep = 1024;  // the second draw's b31 past the first (832 bytes, any offset alignment up to 256)
constexpr int kFbBindings = 11;            // b31; t24 stencil, t26..t29, t31 depth; s26..s29 (vkframe_spirv.sh fx shifts)

struct FogBloom
{
	bool made = false, broken = false;
	std::string brokenWhy;
	VkDescriptorSetLayout dsl = VK_NULL_HANDLE;
	VkDescriptorPool dpool = VK_NULL_HANDLE;
	VkDescriptorSet set[2] = {};  // [0] fbTranslucent 0, [1] fbTranslucent 1
	VkPipelineLayout layout = VK_NULL_HANDLE;
	VkShaderModule ps = VK_NULL_HANDLE;
	VkBuffer ubo = VK_NULL_HANDLE;  // set 0's b31 at kFxLooseAt, set 1's at kFxLooseAt + kFbSetStep
	VkDeviceMemory uboMem = VK_NULL_HANDLE;
	std::uint8_t * uboPtr = nullptr;
	VkRenderPass pass = VK_NULL_HANDLE;
	VkPipeline pipe = VK_NULL_HANDLE;
	VkFramebuffer fb = VK_NULL_HANDLE;
	int samples = 0, pipelines = 0;
	double buildMs = 0.0;
	// this run's job (the drawing thread writes it before the hand, the worker records it)
	int w = 0, h = 0, vp[4] = {}, draws = 2, firstSet = 0;
	WwVkPresent::FxStat st;
};

FogBloom & fogBloom()
{
	static FogBloom d;
	return d;
}

bool fbStatic( std::string & why )
{
	Vk & v = vk();
	FogBloom & D = fogBloom();
	if ( D.made )
		return true;
	const double t0 = nowMs();
	static const std::uint32_t kBind[kFbBindings] = { 31, 88, 90, 91, 92, 93, 95, 154, 155, 156, 157 };
	VkDescriptorSetLayoutBinding b[kFbBindings] = {};
	for ( int k = 0; k < kFbBindings; k++ ) {
		b[k].binding = kBind[k];
		b[k].descriptorType = k == 0 ? VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER : k < 7 ? VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE : VK_DESCRIPTOR_TYPE_SAMPLER;
		b[k].descriptorCount = 1;
		b[k].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	}
	VkDescriptorSetLayoutCreateInfo lci { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
	lci.bindingCount = kFbBindings;
	lci.pBindings = b;
	VkDescriptorPoolSize ps[3] = { { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 2 }, { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 12 }, { VK_DESCRIPTOR_TYPE_SAMPLER, 8 } };
	VkDescriptorPoolCreateInfo pci { VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
	pci.maxSets = 2;
	pci.poolSizeCount = 3;
	pci.pPoolSizes = ps;
	if ( v.vkCreateDescriptorSetLayout( v.dev, &lci, nullptr, &D.dsl ) != VK_SUCCESS
		|| v.vkCreateDescriptorPool( v.dev, &pci, nullptr, &D.dpool ) != VK_SUCCESS ) {
		why = "the fog bloom's descriptor set layout / pool failed";
		return false;
	}
	const VkDescriptorSetLayout two[2] = { D.dsl, D.dsl };
	VkDescriptorSetAllocateInfo dai { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
	dai.descriptorPool = D.dpool;
	dai.descriptorSetCount = 2;
	dai.pSetLayouts = two;
	VkPipelineLayoutCreateInfo plci { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
	plci.setLayoutCount = 1;
	plci.pSetLayouts = &D.dsl;
	if ( v.vkAllocateDescriptorSets( v.dev, &dai, D.set ) != VK_SUCCESS
		|| v.vkCreatePipelineLayout( v.dev, &plci, nullptr, &D.layout ) != VK_SUCCESS ) {
		why = "the fog bloom's descriptor sets / pipeline layout failed";
		return false;
	}
	if ( !fxModule( vkframe_fogbloom_view_ps_spv, sizeof( vkframe_fogbloom_view_ps_spv ), D.ps ) ) {
		why = "the fog bloom's shader module failed";
		return false;
	}
	VkBufferCreateInfo bci { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
	bci.size = kFxLooseAt + kFbSetStep + VkDeviceSize( WwVkPresent::kT_fogbloomBytes );
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
		why = "the fog bloom's uniform buffer failed";
		return false;
	}
	std::memset( D.uboPtr, 0, size_t( bci.size ) );
	D.buildMs += nowMs() - t0;
	D.made = true;
	return true;
}

// the render pass (R8 at the frame's samples, CLEAR / STORE, GENERAL) and its pipeline, remade when the samples change
bool fbPipe( int samples, std::string & why )
{
	Vk & v = vk();
	Fx & F = fx();
	FogBloom & D = fogBloom();
	if ( D.pipe && D.samples == samples )
		return true;
	const double t0 = nowMs();
	if ( D.pipe )
		v.vkDestroyPipeline( v.dev, D.pipe, nullptr );
	if ( D.pass )
		v.vkDestroyRenderPass( v.dev, D.pass, nullptr );
	D.pipe = VK_NULL_HANDLE;
	D.pass = VK_NULL_HANDLE;
	D.samples = 0;
	VkAttachmentDescription at {};
	at.format = VK_FORMAT_R8_UNORM;
	at.samples = VkSampleCountFlagBits( samples );
	at.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;  // GL's glClear( 0 ) over the whole target (scissor off)
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
	if ( v.vkCreateRenderPass( v.dev, &rci, nullptr, &D.pass ) != VK_SUCCESS ) {
		why = "vkCreateRenderPass (the fog bloom) failed";
		return false;
	}
	VkPipelineShaderStageCreateInfo st[2] = { { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO },
		{ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO } };
	st[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
	st[0].module = F.vs;  // the full-screen triangle (tone_view_vs); GL's two-triangle quad covers the same samples
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
	ms.rasterizationSamples = VkSampleCountFlagBits( samples );  // pixel rate, as GL's (the program reads no sample)
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
		why = "vkCreateGraphicsPipelines (the fog bloom) failed";
		return false;
	}
	D.pipelines++;
	D.samples = samples;
	D.buildMs += nowMs() - t0;
	toneCacheSave();
	return true;
}

// the worker (fxWork, F.kind 6): clear the target, the two draws
bool fbRecord( std::string & why )
{
	Vk & v = vk();
	Fx & F = fx();
	FogBloom & D = fogBloom();
	if ( !D.fb || !D.pipe ) {
		why = "no framebuffer / pipeline for the fog bloom";
		return false;
	}
	VkClearValue clear {};
	VkRenderPassBeginInfo rbi { VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
	rbi.renderPass = D.pass;
	rbi.framebuffer = D.fb;
	rbi.renderArea = { { 0, 0 }, { std::uint32_t( D.w ), std::uint32_t( D.h ) } };
	rbi.clearValueCount = 1;
	rbi.pClearValues = &clear;
	v.vkCmdBeginRenderPass( F.cmd, &rbi, VK_SUBPASS_CONTENTS_INLINE );
	// GL's viewport rows from the bottom = the image's rows from the top in memory: no flip (FragCoord.y = GL's)
	const VkViewport vpt { float( D.vp[0] ), float( D.vp[1] ), float( D.vp[2] ), float( D.vp[3] ), 0.0f, 1.0f };
	const int x0 = std::clamp( D.vp[0], 0, D.w ), y0 = std::clamp( D.vp[1], 0, D.h );
	const int x1 = std::clamp( D.vp[0] + D.vp[2], 0, D.w ), y1 = std::clamp( D.vp[1] + D.vp[3], 0, D.h );
	const VkRect2D sc { { x0, y0 }, { std::uint32_t( std::max( x1 - x0, 0 ) ), std::uint32_t( std::max( y1 - y0, 0 ) ) } };
	v.vkCmdSetViewport( F.cmd, 0, 1, &vpt );
	v.vkCmdSetScissor( F.cmd, 0, 1, &sc );
	v.vkCmdBindPipeline( F.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, D.pipe );
	for ( int k = 0; k < D.draws; k++ ) {
		// draw 2 overwrites draw 1 on the stencil samples (GL's order: no blend, the last write stays)
		v.vkCmdBindDescriptorSets( F.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, D.layout, 0, 1, &D.set[k ? 1 : D.firstSet], 0, nullptr );
		v.vkCmdDraw( F.cmd, 3, 1, 0, 0 );
	}
	v.vkCmdEndRenderPass( F.cmd );
	return true;
}
}  // namespace

namespace WwVkPresent
{
bool fogBloomRun( const FogBloomRun & t, unsigned & target, std::string & why )
{
	Fx & F = fx();
	FogBloom & D = fogBloom();
	Tone & T = tone();
	Vk & v = vk();
	Gl & g = gl();
	FxStat & S = D.st;
	const double t0 = nowMs();
	target = 0;
	auto refuse = [&]( const std::string & w ) {
		why = w;
		S.fails++;
		S.failWhy = w;
		return false;
	};
	auto breakOff = [&]( const std::string & w ) {
		D.broken = true;
		D.brokenWhy = "the fog bloom on Vulkan broke: " + w;
		return refuse( D.brokenWhy );
	};
	if ( D.broken )
		return refuse( D.brokenWhy );
	if ( !toneFrameIs( t.color ) || !T.depthView || !T.stencilView )
		return refuse( "the frame is not the shared tone frame" );
	if ( T.w != t.w || T.h != t.h || T.samples != t.samples )
		return refuse( "the fog bloom's size / samples are not the tone frame's" );
	if ( t.samples < 2 )
		return refuse( "a one-sample frame (GL's target is multisample)" );
	if ( t.vp[2] < 1 || t.vp[3] < 1 )
		return refuse( "an empty viewport" );
	if ( !t.key )
		return refuse( "no target key" );
	if ( !v.ok || !g.ok || g.ctx != reinterpret_cast<void *>( wglGetCurrentContext() ) )
		return refuse( "the present device or GL's entry points are not ready in this context" );
	if ( !g.copyImageSubData )
		return refuse( "no glCopyImageSubData (the inputs' copies into their shared twins)" );
	// the last crossing (any fx pass) is done before its command buffer, sets, framebuffer and buffers are written again
	if ( F.pending ) {
		v.vkWaitForFences( v.dev, 1, &F.fence, VK_TRUE, 1000000000ull );
		F.pending = false;
	}
	std::string w;
	if ( !fxStatic( w ) || !fxSems( w ) || !fbStatic( w ) || !fbPipe( t.samples, w ) )
		return breakOff( w );
	unsigned tex = 0;
	if ( !sharedTarget( t.key, 0, t.w, t.h, 1, kGlR8, tex, w, 1, t.samples ) )
		return refuse( "the fog bloom's target: " + w );
	const Shared * tgt = sharedFind( t.key );
	if ( !tgt || !tgt->view )
		return refuse( "the fog bloom's target has no view" );
	FxNames names;
	names.add( T.dsTex );
	names.add( tex );
	VkImageView views[4] = {};
	int smp[4] = {};
	for ( int k = 0; k < 4; k++ ) {
		FxTex x = t.tex[k];
		if ( !x.shared && x.tex )
			x.shared = farSharedOf( x.tex );  // the air's integrated volume: read where it is
		if ( !fxSlot( x, views[k], names, S.copies, w ) )
			return refuse( "the fog bloom's input " + std::to_string( k ) + ": " + w );
		smp[k] = x.sampler;
	}
	if ( !fxFb( D.fb, D.pass, &tgt->view, 1, t.w, t.h ) )
		return breakOff( "the fog bloom's framebuffer over its target failed" );
	// b31 twice: fbTranslucent 0 (draw 1), 1 (draw 2)
	for ( int s = 0; s < 2; s++ ) {
		std::uint8_t * u = D.uboPtr + kFxLooseAt + VkDeviceSize( s ) * kFbSetStep;
		std::memcpy( u, t.loose, size_t( kT_fogbloomBytes ) );
		const int tr = s;
		std::memcpy( u + kT_fogbloomTranslucentAt, &tr, 4 );
		if ( t.red & 4 ) {  // red: the projection's y doubled (a wrong aspect: every ray's length moves; a sign flip is
			                    // invisible, the air's uv comes from the pixel and posView only gives the distance)
			float py = 0.0f;
			std::memcpy( &py, u + 776, 4 );
			py *= 2.0f;
			std::memcpy( u + 776, &py, 4 );
		}
	}
	const VkImageView img[6] = { T.stencilView, views[0], views[1], views[2], views[3], T.depthView };
	static const std::uint32_t kImg[6] = { 88, 90, 91, 92, 93, 95 };
	VkDescriptorBufferInfo bi[2] = {};
	VkDescriptorImageInfo ii[2][10] = {};
	VkWriteDescriptorSet ws[2 * kFbBindings] = {};
	int n = 0;
	for ( int s = 0; s < 2; s++ ) {
		bi[s] = { D.ubo, kFxLooseAt + VkDeviceSize( s ) * kFbSetStep, VkDeviceSize( kT_fogbloomBytes ) };
		for ( int k = 0; k < kFbBindings; k++ ) {
			VkWriteDescriptorSet & x = ws[n++];
			x.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			x.dstSet = D.set[s];
			x.descriptorCount = 1;
			if ( k == 0 ) {
				x.dstBinding = 31;
				x.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
				x.pBufferInfo = &bi[s];
			} else if ( k < 7 ) {
				ii[s][k - 1].imageView = img[k - 1];
				ii[s][k - 1].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
				x.dstBinding = kImg[k - 1];
				x.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
				x.pImageInfo = &ii[s][k - 1];
			} else {
				ii[s][k - 1].sampler = F.smp[std::clamp( smp[k - 7], 0, int( kFxSamplers ) - 1 )];
				x.dstBinding = std::uint32_t( 154 + k - 7 );
				x.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
				x.pImageInfo = &ii[s][k - 1];
			}
		}
	}
	v.vkUpdateDescriptorSets( v.dev, std::uint32_t( n ), ws, 0, nullptr );
	D.w = t.w;
	D.h = t.h;
	std::memcpy( D.vp, t.vp, sizeof( D.vp ) );
	D.draws = ( t.red & 1 ) ? 1 : 2;
	D.firstSet = ( t.red & 2 ) ? 1 : 0;
	F.kind = 6;
	F.w = t.w;
	F.h = t.h;
	std::memcpy( F.vp, t.vp, sizeof( F.vp ) );
	if ( !fxCross( names, w ) )
		return breakOff( w );  // nothing signals vkDone: GL does not wait it; GL draws its own pass this frame
	target = tex;
	const double ms = nowMs() - t0;
	S.runs++;
	S.cpuMs += ms;
	S.maxMs = std::max( S.maxMs, ms );
	return true;
}

void fogBloomOnGl( const char * why )
{
	FxStat & S = fogBloom().st;
	S.gl++;
	S.glWhy = why ? why : "";
}

FxStat takeFogBloomStat()
{
	FogBloom & D = fogBloom();
	FxStat s = D.st;
	s.pipelines = D.pipelines;
	s.buildMs = D.buildMs;
	if ( D.broken && s.failWhy.empty() )
		s.failWhy = D.brokenWhy;
	D.st = FxStat();
	return s;
}
}  // namespace WwVkPresent
