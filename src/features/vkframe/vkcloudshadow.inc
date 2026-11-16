/* lane VKCLOUD3 (2026-11-16): the cloud shadow map's build (cloudshadow.cpp, cloud_shadowmap) on the present device,
 * through the VKAIR1 crossing (fx(): its fence, command buffer and semaphores); fxWork's kind 12. Included once by
 * vkpresent.cpp after vkobjtex.inc (it reads objTexKey / objTexFlush and farTwinFind / farSharedOf).
 * GL: the map's fbo (RGBA16F n x n), viewport n x n, scissor ( 0, row0, n, rows ), no depth / blend / cull / stencil,
 * one quad. Here: one render pass over the shared map, LOAD (a slice keeps the rows it does not write), the same scissor
 * (GL's rows from the bottom = the image's rows from the top in memory: no flip), one full-screen triangle.
 * The sheets (BC layer textures from the texture cache) are read in place through the cache's twins (objTexKey); a
 * sheet with no cache twin is copied into its own twin when its format can be shared, else the run is refused by name. */
namespace
{
constexpr int kCsBindings = 11;  // b31; t64..t66, t67[8], t75[8]; s128..s130, s131[8], s139[8]

struct CloudShadow
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
	VkRenderPass pass = VK_NULL_HANDLE, passClear = VK_NULL_HANDLE;  // LOAD (the route), CLEAR (red 2: same compatibility)
	VkPipeline pipe = VK_NULL_HANDLE;
	VkFramebuffer fb = VK_NULL_HANDLE;
	int pipelines = 0;
	double buildMs = 0.0;
	// this run's job (the drawing thread writes it before the hand, the worker records it)
	int n = 0, row0 = 0, rows = 0;
	bool clear = false;
	WwVkPresent::FxStat st;
	int inPlace = 0;  // sheets read in place through the texture cache's twins (since the last take)
};

CloudShadow & cloudShadow()
{
	static CloudShadow d;
	return d;
}

bool csStatic( std::string & why )
{
	Vk & v = vk();
	CloudShadow & D = cloudShadow();
	if ( D.made )
		return true;
	const double t0 = nowMs();
	static const std::uint32_t kBind[kCsBindings] = { 31, 64, 65, 66, 67, 75, 128, 129, 130, 131, 139 };
	static const std::uint32_t kCount[kCsBindings] = { 1, 1, 1, 1, 8, 8, 1, 1, 1, 8, 8 };
	VkDescriptorSetLayoutBinding b[kCsBindings] = {};
	for ( int k = 0; k < kCsBindings; k++ ) {
		b[k].binding = kBind[k];
		b[k].descriptorType = k == 0 ? VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER : k < 6 ? VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE : VK_DESCRIPTOR_TYPE_SAMPLER;
		b[k].descriptorCount = kCount[k];
		b[k].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	}
	VkDescriptorSetLayoutCreateInfo lci { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
	lci.bindingCount = kCsBindings;
	lci.pBindings = b;
	VkDescriptorPoolSize ps[3] = { { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1 }, { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 19 }, { VK_DESCRIPTOR_TYPE_SAMPLER, 19 } };
	VkDescriptorPoolCreateInfo pci { VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
	pci.maxSets = 1;
	pci.poolSizeCount = 3;
	pci.pPoolSizes = ps;
	if ( v.vkCreateDescriptorSetLayout( v.dev, &lci, nullptr, &D.dsl ) != VK_SUCCESS
		|| v.vkCreateDescriptorPool( v.dev, &pci, nullptr, &D.dpool ) != VK_SUCCESS ) {
		why = "the cloud shadow's descriptor set layout / pool failed";
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
		why = "the cloud shadow's descriptor set / pipeline layout failed";
		return false;
	}
	if ( !fxModule( vkframe_cloud_shadowmap_view_ps_spv, sizeof( vkframe_cloud_shadowmap_view_ps_spv ), D.ps ) ) {
		why = "the cloud shadow's shader module failed";
		return false;
	}
	VkBufferCreateInfo bci { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
	bci.size = kFxLooseAt + VkDeviceSize( WwVkPresent::kT_cloudshadowBytes );
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
		why = "the cloud shadow's uniform buffer failed";
		return false;
	}
	std::memset( D.uboPtr, 0, size_t( bci.size ) );
	// the render passes (RGBA16F, one sample, STORE, GENERAL): LOAD for the route, CLEAR for red 2 (compatible: one pipeline)
	for ( int c = 0; c < 2; c++ ) {
		VkAttachmentDescription at {};
		at.format = VK_FORMAT_R16G16B16A16_SFLOAT;
		at.samples = VK_SAMPLE_COUNT_1_BIT;
		at.loadOp = c ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
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
			why = "vkCreateRenderPass (the cloud shadow) failed";
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
		why = "vkCreateGraphicsPipelines (the cloud shadow) failed";
		return false;
	}
	D.pipelines++;
	toneCacheSave();
	D.buildMs += nowMs() - t0;
	D.made = true;
	return true;
}

// the worker (fxWork, F.kind 12): the slice's rows of the map
bool csRecord( std::string & why )
{
	Vk & v = vk();
	Fx & F = fx();
	CloudShadow & D = cloudShadow();
	if ( !D.fb || !D.pipe ) {
		why = "no framebuffer / pipeline for the cloud shadow";
		return false;
	}
	VkClearValue clear {};
	VkRenderPassBeginInfo rbi { VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
	rbi.renderPass = D.clear ? D.passClear : D.pass;
	rbi.framebuffer = D.fb;
	rbi.renderArea = { { 0, 0 }, { std::uint32_t( D.n ), std::uint32_t( D.n ) } };
	rbi.clearValueCount = 1;
	rbi.pClearValues = &clear;
	v.vkCmdBeginRenderPass( F.cmd, &rbi, VK_SUBPASS_CONTENTS_INLINE );
	const VkViewport vpt { 0.0f, 0.0f, float( D.n ), float( D.n ), 0.0f, 1.0f };
	const int y0 = std::clamp( D.row0, 0, D.n ), y1 = std::clamp( D.row0 + D.rows, 0, D.n );
	const VkRect2D sc { { 0, y0 }, { std::uint32_t( D.n ), std::uint32_t( std::max( y1 - y0, 0 ) ) } };
	v.vkCmdSetViewport( F.cmd, 0, 1, &vpt );
	v.vkCmdSetScissor( F.cmd, 0, 1, &sc );
	v.vkCmdBindPipeline( F.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, D.pipe );
	v.vkCmdBindDescriptorSets( F.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, D.layout, 0, 1, &D.set, 0, nullptr );
	v.vkCmdDraw( F.cmd, 3, 1, 0, 0 );
	v.vkCmdEndRenderPass( F.cmd );
	return true;
}
}  // namespace

namespace WwVkPresent
{
bool cloudShadowRun( const CloudShadowRun & t, std::string & why )
{
	Fx & F = fx();
	CloudShadow & D = cloudShadow();
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
		D.brokenWhy = "the cloud shadow on Vulkan broke: " + w;
		return refuse( D.brokenWhy );
	};
	if ( D.broken )
		return refuse( D.brokenWhy );
	if ( t.n < 1 || t.rows < 1 || t.row0 < 0 || t.row0 + t.rows > t.n )
		return refuse( "rows outside the map" );
	if ( !t.key )
		return refuse( "no map key" );
	if ( !v.ok || !g.ok || g.ctx != reinterpret_cast<void *>( wglGetCurrentContext() ) )
		return refuse( "the present device or GL's entry points are not ready in this context" );
	if ( !g.copyImageSubData )
		return refuse( "no glCopyImageSubData (the inputs' copies into their shared twins)" );
	const Shared * tgt = sharedFind( t.key );
	if ( !tgt || !tgt->view || tgt->w != t.n || tgt->h != t.n )
		return refuse( "the map is not a shared RGBA16F n x n target" );
	// the last crossing (any fx pass) is done before its command buffer, sets, framebuffer and buffers are written again
	if ( F.pending ) {
		v.vkWaitForFences( v.dev, 1, &F.fence, VK_TRUE, 1000000000ull );
		F.pending = false;
	}
	std::string w;
	if ( !fxStatic( w ) || !fxSems( w ) || !csStatic( w ) )
		return breakOff( w );
	FxNames names;
	names.add( tgt->tex );
	VkImageView views[3 + 2 * kCloudShadowSheets] = {};
	int smp[3 + 2 * kCloudShadowSheets] = {};
	bool cacheTwins = false;
	for ( int k = 0; k < 3 + 2 * kCloudShadowSheets; k++ ) {
		FxTex x = k < 3 ? t.tex[k] : k < 3 + kCloudShadowSheets ? t.sheet[k - 3] : t.sheet2[k - 3 - kCloudShadowSheets];
		smp[k] = x.sampler;
		if ( k >= 3 && x.tex && !x.shared ) {
			const std::uintptr_t ck = objTexKey( x.tex, 0 );
			const Shared * cs = ck ? farTwinFind( ck ) : nullptr;
			if ( cs && cs->view ) {  // the texture cache's twin of this sheet: read in place, no copy
				views[k] = cs->view;
				if ( cs->tex )
					names.add( cs->tex );
				cacheTwins = true;
				D.inPlace++;
				continue;
			}
		}
		if ( !x.shared && x.tex )
			x.shared = farSharedOf( x.tex );
		if ( !fxSlot( x, views[k], names, S.copies, w ) )
			return refuse( std::string( k < 3 ? "the cloud shadow's input " : "sheets: a layer texture with no cache twin (" )
				+ std::to_string( k ) + ( k < 3 ? ": " : "): " ) + w );
	}
	if ( cacheTwins && !objTexFlush( w ) )
		return refuse( "sheets: the texture cache's twins are not on the device: " + w );
	if ( !fxFb( D.fb, D.pass, &tgt->view, 1, t.n, t.n ) )
		return breakOff( "the cloud shadow's framebuffer over its map failed" );
	std::uint8_t * u = D.uboPtr + kFxLooseAt;
	std::memcpy( u, t.loose, size_t( kT_cloudshadowBytes ) );
	if ( t.red & 4 )  // red: csS shifted 4 bytes (a b31 field off its offset)
		std::memmove( u + 676, u + 672, 12 );
	const bool swapSheets = ( t.red & 1 ) != 0;  // red: csSheetTex <-> csSheetTex2
	VkDescriptorBufferInfo bi { D.ubo, kFxLooseAt, VkDeviceSize( kT_cloudshadowBytes ) };
	VkDescriptorImageInfo ii[2][3 + 2 * kCloudShadowSheets] = {};
	for ( int k = 0; k < 3 + 2 * kCloudShadowSheets; k++ ) {
		int src = k;
		if ( swapSheets && k >= 3 )
			src = k < 3 + kCloudShadowSheets ? k + kCloudShadowSheets : k - kCloudShadowSheets;
		ii[0][k].imageView = views[src];
		ii[0][k].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
		ii[1][k].sampler = F.smp[std::clamp( smp[src], 0, int( kFxSamplers ) - 1 )];
	}
	static const std::uint32_t kImg[5] = { 64, 65, 66, 67, 75 }, kSmp[5] = { 128, 129, 130, 131, 139 };
	static const int kFirst[5] = { 0, 1, 2, 3, 3 + kCloudShadowSheets }, kN[5] = { 1, 1, 1, kCloudShadowSheets, kCloudShadowSheets };
	VkWriteDescriptorSet ws[kCsBindings] = {};
	for ( int k = 0; k < kCsBindings; k++ ) {
		VkWriteDescriptorSet & x = ws[k];
		x.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		x.dstSet = D.set;
		if ( k == 0 ) {
			x.dstBinding = 31;
			x.descriptorCount = 1;
			x.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			x.pBufferInfo = &bi;
			continue;
		}
		const int e = ( k - 1 ) % 5, im = ( k - 1 ) < 5;
		x.dstBinding = im ? kImg[e] : kSmp[e];
		x.descriptorCount = std::uint32_t( kN[e] );
		x.descriptorType = im ? VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE : VK_DESCRIPTOR_TYPE_SAMPLER;
		x.pImageInfo = &ii[im ? 0 : 1][kFirst[e]];
	}
	v.vkUpdateDescriptorSets( v.dev, std::uint32_t( kCsBindings ), ws, 0, nullptr );
	D.n = t.n;
	D.row0 = t.row0;
	D.rows = t.rows;
	D.clear = ( t.red & 2 ) != 0;
	F.kind = 12;
	F.w = t.n;
	F.h = t.n;
	const int vp[4] = { 0, 0, t.n, t.n };
	std::memcpy( F.vp, vp, sizeof( F.vp ) );
	if ( !fxCross( names, w ) )
		return breakOff( w );  // nothing signals vkDone: GL does not wait it; GL draws its own pass this frame
	const double ms = nowMs() - t0;
	S.runs++;
	S.cpuMs += ms;
	S.maxMs = std::max( S.maxMs, ms );
	return true;
}

void cloudShadowOnGl( const char * why )
{
	WwVkPresent::glGuard( "cloudShadow", 1 );  // lane VKFLIGHT4: the view GL guard
	FxStat & S = cloudShadow().st;
	S.gl++;
	S.glWhy = why ? why : "";
}

FxStat takeCloudShadowStat()
{
	CloudShadow & D = cloudShadow();
	FxStat s = D.st;
	s.pipelines = D.pipelines;
	s.buildMs = D.buildMs;
	s.refusedPacks = D.inPlace;  // the cloud shadow: sheets read in place through the texture cache's twins
	if ( D.broken && s.failWhy.empty() )
		s.failWhy = D.brokenWhy;
	D.st = FxStat();
	D.inPlace = 0;
	return s;
}
}  // namespace WwVkPresent
