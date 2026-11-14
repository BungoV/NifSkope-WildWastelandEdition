/* lane VKFAR2 (row P8, 2026-11-14): the clouds in front of the scene (cloudvol.cpp frontPass( 2 ), cloud_composite cvFront 2)
 * on the present device, through the VKAIR1 crossing (fx(): its fence, command buffer and semaphores). Included once by
 * vkpresent.cpp after vkfogbloom.inc (it reads farSharedOf and the fx parts of the anonymous namespace).
 * GL: one quad over the tone frame, blend ONE / ONE_MINUS_SRC_ALPHA (alpha ZERO / ONE), stencil EQUAL 1 mask 0x03 with no
 * write, no depth test; the history (RGBA16F colour, RG32F depth: shared on this device) and the scene's depth max (R32F,
 * GL's, copied into a twin). Here: one render pass over the tone frame's colour + depth / stencil (LOAD / STORE), the same
 * state, one full-screen triangle at pixel rate (the program reads no sample, as GL's). */
namespace
{
constexpr int kCfBindings = 7;  // b31; t64..t66; s128..s130 (vkframe_spirv.sh fx shifts)

struct CloudFront
{
	bool made = false, broken = false;
	std::string brokenWhy;
	VkDescriptorSetLayout dsl = VK_NULL_HANDLE;
	VkDescriptorPool dpool = VK_NULL_HANDLE;
	VkDescriptorSet set = VK_NULL_HANDLE;
	VkPipelineLayout layout = VK_NULL_HANDLE;
	VkShaderModule ps = VK_NULL_HANDLE;
	VkBuffer ubo = VK_NULL_HANDLE;  // b31 at kFxLooseAt
	VkDeviceMemory uboMem = VK_NULL_HANDLE;
	std::uint8_t * uboPtr = nullptr;
	VkRenderPass pass = VK_NULL_HANDLE;
	VkPipeline pipe[4] = {};  // [red & 3]: 0 GL's state; 1 stencil off; 2 no blend; 3 both (gates only)
	VkFramebuffer fb = VK_NULL_HANDLE;
	int samples = 0, pipelines = 0;
	double buildMs = 0.0;
	// this run's job (the drawing thread writes it before the hand, the worker records it)
	int w = 0, h = 0, vp[4] = {}, variant = 0;
	WwVkPresent::FxStat st;
};

CloudFront & cloudFront()
{
	static CloudFront d;
	return d;
}

bool cfStatic( std::string & why )
{
	Vk & v = vk();
	CloudFront & D = cloudFront();
	if ( D.made )
		return true;
	const double t0 = nowMs();
	static const std::uint32_t kBind[kCfBindings] = { 31, 64, 65, 66, 128, 129, 130 };
	VkDescriptorSetLayoutBinding b[kCfBindings] = {};
	for ( int k = 0; k < kCfBindings; k++ ) {
		b[k].binding = kBind[k];
		b[k].descriptorType = k == 0 ? VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER : k < 4 ? VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE : VK_DESCRIPTOR_TYPE_SAMPLER;
		b[k].descriptorCount = 1;
		b[k].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	}
	VkDescriptorSetLayoutCreateInfo lci { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
	lci.bindingCount = kCfBindings;
	lci.pBindings = b;
	VkDescriptorPoolSize ps[3] = { { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1 }, { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 3 }, { VK_DESCRIPTOR_TYPE_SAMPLER, 3 } };
	VkDescriptorPoolCreateInfo pci { VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
	pci.maxSets = 1;
	pci.poolSizeCount = 3;
	pci.pPoolSizes = ps;
	if ( v.vkCreateDescriptorSetLayout( v.dev, &lci, nullptr, &D.dsl ) != VK_SUCCESS
		|| v.vkCreateDescriptorPool( v.dev, &pci, nullptr, &D.dpool ) != VK_SUCCESS ) {
		why = "the clouds-in-front descriptor set layout / pool failed";
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
		why = "the clouds-in-front descriptor set / pipeline layout failed";
		return false;
	}
	if ( !fxModule( vkframe_cloud_composite_view_ps_spv, sizeof( vkframe_cloud_composite_view_ps_spv ), D.ps ) ) {
		why = "the clouds-in-front shader module failed";
		return false;
	}
	VkBufferCreateInfo bci { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
	bci.size = kFxLooseAt + VkDeviceSize( WwVkPresent::kT_cloudfrontBytes );
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
		why = "the clouds-in-front uniform buffer failed";
		return false;
	}
	std::memset( D.uboPtr, 0, size_t( bci.size ) );
	D.buildMs += nowMs() - t0;
	D.made = true;
	return true;
}

// the render pass (the tone frame's RGBA16F + D24S8 at its samples, LOAD / STORE, GENERAL) and the pipeline of this variant
bool cfPipe( int samples, int variant, std::string & why )
{
	Vk & v = vk();
	Fx & F = fx();
	CloudFront & D = cloudFront();
	if ( D.pass && D.samples != samples ) {
		for ( VkPipeline & p : D.pipe )
			if ( p ) {
				v.vkDestroyPipeline( v.dev, p, nullptr );
				p = VK_NULL_HANDLE;
			}
		v.vkDestroyRenderPass( v.dev, D.pass, nullptr );
		D.pass = VK_NULL_HANDLE;
		D.samples = 0;
	}
	if ( D.pass && D.pipe[variant] )
		return true;
	const double t0 = nowMs();
	if ( !D.pass ) {
		VkAttachmentDescription at[2] = {};
		at[0].format = VK_FORMAT_R16G16B16A16_SFLOAT;
		at[1].format = VK_FORMAT_D24_UNORM_S8_UINT;
		for ( VkAttachmentDescription & a : at ) {
			a.samples = VkSampleCountFlagBits( samples );
			a.loadOp = a.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
			a.storeOp = a.stencilStoreOp = VK_ATTACHMENT_STORE_OP_STORE;
			a.initialLayout = a.finalLayout = VK_IMAGE_LAYOUT_GENERAL;
		}
		const VkAttachmentReference cr { 0, VK_IMAGE_LAYOUT_GENERAL }, dr { 1, VK_IMAGE_LAYOUT_GENERAL };
		VkSubpassDescription sp {};
		sp.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		sp.colorAttachmentCount = 1;
		sp.pColorAttachments = &cr;
		sp.pDepthStencilAttachment = &dr;
		VkRenderPassCreateInfo rci { VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO };
		rci.attachmentCount = 2;
		rci.pAttachments = at;
		rci.subpassCount = 1;
		rci.pSubpasses = &sp;
		if ( v.vkCreateRenderPass( v.dev, &rci, nullptr, &D.pass ) != VK_SUCCESS ) {
			why = "vkCreateRenderPass (the clouds in front) failed";
			return false;
		}
		D.samples = samples;
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
	// frontPass( 2 ): stencil EQUAL 1, compare mask 0x03, write mask 0, KEEP everywhere; no depth test or write
	VkPipelineDepthStencilStateCreateInfo ds { VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };
	ds.stencilTestEnable = ( variant & 1 ) ? VK_FALSE : VK_TRUE;
	ds.front.failOp = ds.front.depthFailOp = ds.front.passOp = VK_STENCIL_OP_KEEP;
	ds.front.compareOp = VK_COMPARE_OP_EQUAL;
	ds.front.compareMask = 0x03;
	ds.front.writeMask = 0;
	ds.front.reference = 1;
	ds.back = ds.front;
	VkPipelineColorBlendAttachmentState cb {};
	cb.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	if ( !( variant & 2 ) ) {  // glBlendFuncSeparate( ONE, ONE_MINUS_SRC_ALPHA, ZERO, ONE )
		cb.blendEnable = VK_TRUE;
		cb.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
		cb.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		cb.colorBlendOp = VK_BLEND_OP_ADD;
		cb.srcAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
		cb.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
		cb.alphaBlendOp = VK_BLEND_OP_ADD;
	}
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
	gp.pDepthStencilState = &ds;
	gp.pColorBlendState = &cbs;
	gp.pDynamicState = &dy;
	gp.layout = D.layout;
	gp.renderPass = D.pass;
	if ( v.vkCreateGraphicsPipelines( v.dev, tone().cache, 1, &gp, nullptr, &D.pipe[variant] ) != VK_SUCCESS ) {
		why = "vkCreateGraphicsPipelines (the clouds in front) failed";
		return false;
	}
	D.pipelines++;
	D.buildMs += nowMs() - t0;
	toneCacheSave();
	return true;
}

// the worker (fxWork, F.kind 7): one triangle over the tone frame
bool cfRecord( std::string & why )
{
	Vk & v = vk();
	Fx & F = fx();
	CloudFront & D = cloudFront();
	if ( !D.fb || !D.pipe[D.variant] ) {
		why = "no framebuffer / pipeline for the clouds in front";
		return false;
	}
	VkRenderPassBeginInfo rbi { VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
	rbi.renderPass = D.pass;
	rbi.framebuffer = D.fb;
	rbi.renderArea = { { 0, 0 }, { std::uint32_t( D.w ), std::uint32_t( D.h ) } };
	v.vkCmdBeginRenderPass( F.cmd, &rbi, VK_SUBPASS_CONTENTS_INLINE );
	// GL's viewport rows from the bottom = the image's rows from the top in memory: no flip (FragCoord.y = GL's)
	const VkViewport vpt { float( D.vp[0] ), float( D.vp[1] ), float( D.vp[2] ), float( D.vp[3] ), 0.0f, 1.0f };
	const int x0 = std::clamp( D.vp[0], 0, D.w ), y0 = std::clamp( D.vp[1], 0, D.h );
	const int x1 = std::clamp( D.vp[0] + D.vp[2], 0, D.w ), y1 = std::clamp( D.vp[1] + D.vp[3], 0, D.h );
	const VkRect2D sc { { x0, y0 }, { std::uint32_t( std::max( x1 - x0, 0 ) ), std::uint32_t( std::max( y1 - y0, 0 ) ) } };
	v.vkCmdSetViewport( F.cmd, 0, 1, &vpt );
	v.vkCmdSetScissor( F.cmd, 0, 1, &sc );
	v.vkCmdBindPipeline( F.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, D.pipe[D.variant] );
	v.vkCmdBindDescriptorSets( F.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, D.layout, 0, 1, &D.set, 0, nullptr );
	v.vkCmdDraw( F.cmd, 3, 1, 0, 0 );
	v.vkCmdEndRenderPass( F.cmd );
	return true;
}
}  // namespace

namespace WwVkPresent
{
bool cloudFrontRun( const CloudFrontRun & t, std::string & why )
{
	Fx & F = fx();
	CloudFront & D = cloudFront();
	Tone & T = tone();
	Vk & v = vk();
	Gl & g = gl();
	FxStat & S = D.st;
	const double t0 = nowMs();
	auto refuse = [&]( const std::string & w ) {
		why = w;
		S.fails++;
		S.failWhy = w;
		return false;
	};
	auto breakOff = [&]( const std::string & w ) {
		D.broken = true;
		D.brokenWhy = "the clouds in front on Vulkan broke: " + w;
		return refuse( D.brokenWhy );
	};
	if ( D.broken )
		return refuse( D.brokenWhy );
	if ( !toneFrameIs( t.color ) || !T.colorView || !T.dsView )
		return refuse( "the frame is not the shared tone frame" );
	if ( T.w != t.w || T.h != t.h || T.samples != t.samples )
		return refuse( "the pass's size / samples are not the tone frame's" );
	if ( t.vp[2] < 1 || t.vp[3] < 1 )
		return refuse( "an empty viewport" );
	int front = 0;
	std::memcpy( &front, t.loose + kT_cloudfrontFrontAt, 4 );
	if ( front != 2 )
		return refuse( "cvFront is not 2 (the pass is the linear-frame composite only)" );
	int flash = 0;
	std::memcpy( &flash, t.loose + kT_cloudfrontFlashAt, 4 );
	if ( flash != 0 )  // cloudfrontview flash job: 1.5 % of the halves off, up to 7 ulp (GL's km altitude off the 6360 km radius)
		return refuse( "a lightning flash (cvLtOn 1): GL draws the flash frames" );
	if ( !v.ok || !g.ok || g.ctx != reinterpret_cast<void *>( wglGetCurrentContext() ) )
		return refuse( "the present device or GL's entry points are not ready in this context" );
	if ( !g.copyImageSubData )
		return refuse( "no glCopyImageSubData (the scene depth's copy into its shared twin)" );
	// the last crossing (any fx pass) is done before its command buffer, sets, framebuffer and buffers are written again
	if ( F.pending ) {
		v.vkWaitForFences( v.dev, 1, &F.fence, VK_TRUE, 1000000000ull );
		F.pending = false;
	}
	const int variant = t.red & 3;
	std::string w;
	if ( !fxStatic( w ) || !fxSems( w ) || !cfStatic( w ) || !cfPipe( t.samples, variant, w ) )
		return breakOff( w );
	FxNames names;
	names.add( T.colorTex );
	names.add( T.dsTex );
	VkImageView views[3] = {};
	int smp[3] = {};
	for ( int k = 0; k < 3; k++ ) {
		FxTex x = t.tex[k];
		if ( !x.shared && x.tex )
			x.shared = farSharedOf( x.tex );  // the history: read where it is
		if ( !fxSlot( x, views[k], names, S.copies, w ) )
			return refuse( "the clouds-in-front input " + std::to_string( k ) + ": " + w );
		smp[k] = x.sampler;
	}
	const VkImageView att[2] = { T.colorView, T.dsView };
	if ( !fxFb( D.fb, D.pass, att, 2, T.w, T.h ) )
		return breakOff( "the clouds-in-front framebuffer over the tone frame failed" );
	std::memcpy( D.uboPtr + kFxLooseAt, t.loose, size_t( kT_cloudfrontBytes ) );
	if ( t.red & 4 ) {  // red: the display composite (cvFront 1: exposure, view transform, un-premultiplied) in the linear frame
		const int one = 1;
		std::memcpy( D.uboPtr + kFxLooseAt + kT_cloudfrontFrontAt, &one, 4 );
	}
	VkDescriptorBufferInfo bi { D.ubo, kFxLooseAt, VkDeviceSize( kT_cloudfrontBytes ) };
	VkDescriptorImageInfo ii[6] = {};
	VkWriteDescriptorSet ws[kCfBindings] = {};
	for ( int k = 0; k < kCfBindings; k++ ) {
		VkWriteDescriptorSet & x = ws[k];
		x.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		x.dstSet = D.set;
		x.descriptorCount = 1;
		if ( k == 0 ) {
			x.dstBinding = 31;
			x.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			x.pBufferInfo = &bi;
		} else if ( k < 4 ) {
			ii[k - 1].imageView = views[k - 1];
			ii[k - 1].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
			x.dstBinding = std::uint32_t( 64 + k - 1 );
			x.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
			x.pImageInfo = &ii[k - 1];
		} else {
			ii[k - 1].sampler = F.smp[std::clamp( smp[k - 4], 0, int( kFxSamplers ) - 1 )];
			x.dstBinding = std::uint32_t( 128 + k - 4 );
			x.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
			x.pImageInfo = &ii[k - 1];
		}
	}
	v.vkUpdateDescriptorSets( v.dev, std::uint32_t( kCfBindings ), ws, 0, nullptr );
	D.w = t.w;
	D.h = t.h;
	std::memcpy( D.vp, t.vp, sizeof( D.vp ) );
	D.variant = variant;
	F.kind = 7;
	F.w = t.w;
	F.h = t.h;
	std::memcpy( F.vp, t.vp, sizeof( F.vp ) );
	if ( !fxCross( names, w ) )
		return breakOff( w );  // nothing signals vkDone: GL does not wait it; GL draws its own quad this frame
	const double ms = nowMs() - t0;
	S.runs++;
	S.cpuMs += ms;
	S.maxMs = std::max( S.maxMs, ms );
	return true;
}

void cloudFrontOnGl( const char * why )
{
	FxStat & S = cloudFront().st;
	S.gl++;
	S.glWhy = why ? why : "";
}

FxStat takeCloudFrontStat()
{
	CloudFront & D = cloudFront();
	FxStat s = D.st;
	s.pipelines = D.pipelines;
	s.buildMs = D.buildMs;
	if ( D.broken && s.failWhy.empty() )
		s.failWhy = D.brokenWhy;
	D.st = FxStat();
	return s;
}
}  // namespace WwVkPresent
