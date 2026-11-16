/* lane VKSMALL3 (2026-11-16): the next frame's shading rate images (cellvrs.cpp wwCellVrsBuild, cell_vrs_rate) on the
 * present device, through the VKAIR1 crossing (fx(): its fence, command buffer and semaphores); fxWork's kind 15.
 * GL: blit the view (MS RGBA8) into an RGBA8 texture (NEAREST, the whole view), then one quad over a tw x th viewport
 * into two R8UI targets (no depth / stencil / blend / cull / scissor). Here: the view slot's colour resolved into the
 * route's RGBA8 twin (vkCmdResolveImage: GL's MS RGBA8 blit resolve matched it EXACT in PRESENTMATCH), then one render
 * pass over the two R8UI twins (every texel written: DONT_CARE), one full-screen triangle. */
namespace
{
constexpr int kVrsBindings = 3;  // b31; t64 vrsSrc, t65 vrsMv (Load only: no samplers in the SPIR-V)

struct VrsVk
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
	int pipelines = 0;
	double buildMs = 0.0;
	// this run's job (the drawing thread writes it before the hand, the worker records it)
	VkImage viewImg = VK_NULL_HANDLE, srcImg = VK_NULL_HANDLE;
	int samples = 1, w = 0, h = 0, tw = 0, th = 0;
	bool clear = false;
	WwVkPresent::FxStat st;
};

VrsVk & vrsVk()
{
	static VrsVk d;
	return d;
}

bool vrsStatic( std::string & why )
{
	Vk & v = vk();
	VrsVk & D = vrsVk();
	if ( D.made )
		return true;
	const double t0 = nowMs();
	VkDescriptorSetLayoutBinding b[kVrsBindings] = {};
	b[0].binding = 31;
	b[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	b[1].binding = 64;
	b[1].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
	b[2].binding = 65;
	b[2].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
	for ( VkDescriptorSetLayoutBinding & x : b ) {
		x.descriptorCount = 1;
		x.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	}
	VkDescriptorSetLayoutCreateInfo lci { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
	lci.bindingCount = kVrsBindings;
	lci.pBindings = b;
	VkDescriptorPoolSize ps[2] = { { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1 }, { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 2 } };
	VkDescriptorPoolCreateInfo pci { VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
	pci.maxSets = 1;
	pci.poolSizeCount = 2;
	pci.pPoolSizes = ps;
	if ( v.vkCreateDescriptorSetLayout( v.dev, &lci, nullptr, &D.dsl ) != VK_SUCCESS
		|| v.vkCreateDescriptorPool( v.dev, &pci, nullptr, &D.dpool ) != VK_SUCCESS ) {
		why = "the shading rate's descriptor set layout / pool failed";
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
		why = "the shading rate's descriptor set / pipeline layout failed";
		return false;
	}
	if ( !fxModule( vkframe_vrs_rate_view_ps_spv, sizeof( vkframe_vrs_rate_view_ps_spv ), D.ps ) ) {
		why = "the shading rate's shader module failed";
		return false;
	}
	VkBufferCreateInfo bci { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
	bci.size = kFxLooseAt + VkDeviceSize( WwVkPresent::kT_vrsBytes );
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
		why = "the shading rate's uniform buffer failed";
		return false;
	}
	std::memset( D.uboPtr, 0, size_t( bci.size ) );
	// the render passes (two R8UI, one sample, STORE, GENERAL): DONT_CARE for the route (the triangle writes every texel),
	// CLEAR for red 2 (compatible: one pipeline)
	for ( int c = 0; c < 2; c++ ) {
		VkAttachmentDescription at[2] = {};
		for ( VkAttachmentDescription & a : at ) {
			a.format = VK_FORMAT_R8_UINT;
			a.samples = VK_SAMPLE_COUNT_1_BIT;
			a.loadOp = c ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			a.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
			a.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			a.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			a.initialLayout = a.finalLayout = VK_IMAGE_LAYOUT_GENERAL;
		}
		const VkAttachmentReference cr[2] = { { 0, VK_IMAGE_LAYOUT_GENERAL }, { 1, VK_IMAGE_LAYOUT_GENERAL } };
		VkSubpassDescription sp {};
		sp.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		sp.colorAttachmentCount = 2;
		sp.pColorAttachments = cr;
		VkRenderPassCreateInfo rci { VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO };
		rci.attachmentCount = 2;
		rci.pAttachments = at;
		rci.subpassCount = 1;
		rci.pSubpasses = &sp;
		if ( v.vkCreateRenderPass( v.dev, &rci, nullptr, c ? &D.passClear : &D.pass ) != VK_SUCCESS ) {
			why = "vkCreateRenderPass (the shading rate) failed";
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
	VkPipelineColorBlendAttachmentState cb[2] = {};
	for ( VkPipelineColorBlendAttachmentState & x : cb )
		x.colorWriteMask = VK_COLOR_COMPONENT_R_BIT;
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
	gp.pColorBlendState = &cbs;
	gp.pDynamicState = &dy;
	gp.layout = D.layout;
	gp.renderPass = D.pass;
	if ( v.vkCreateGraphicsPipelines( v.dev, tone().cache, 1, &gp, nullptr, &D.pipe ) != VK_SUCCESS ) {
		why = "vkCreateGraphicsPipelines (the shading rate) failed";
		return false;
	}
	D.pipelines++;
	toneCacheSave();
	D.buildMs += nowMs() - t0;
	D.made = true;
	return true;
}

// the worker (fxWork, F.kind 15): the view resolved into the source twin, then every texel of both rate twins
bool vrsRecord( std::string & why )
{
	Vk & v = vk();
	Fx & F = fx();
	VrsVk & D = vrsVk();
	if ( !D.fb || !D.pipe || !D.viewImg || !D.srcImg ) {
		why = "no framebuffer / pipeline / images for the shading rate";
		return false;
	}
	// 1. GL's blit: the view (GL drew it, earlier in submission order) into the source twin, MS -> 1 sample
	VkMemoryBarrier in { VK_STRUCTURE_TYPE_MEMORY_BARRIER };
	in.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
	in.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
	v.vkCmdPipelineBarrier( F.cmd, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 1, &in, 0, nullptr, 0, nullptr );
	if ( D.samples > 1 ) {
		VkImageResolve r {};
		r.srcSubresource = r.dstSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
		r.extent = { std::uint32_t( D.w ), std::uint32_t( D.h ), 1 };
		v.vkCmdResolveImage( F.cmd, D.viewImg, VK_IMAGE_LAYOUT_GENERAL, D.srcImg, VK_IMAGE_LAYOUT_GENERAL, 1, &r );
	} else {
		VkImageCopy c {};
		c.srcSubresource = c.dstSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
		c.extent = { std::uint32_t( D.w ), std::uint32_t( D.h ), 1 };
		v.vkCmdCopyImage( F.cmd, D.viewImg, VK_IMAGE_LAYOUT_GENERAL, D.srcImg, VK_IMAGE_LAYOUT_GENERAL, 1, &c );
	}
	VkMemoryBarrier out { VK_STRUCTURE_TYPE_MEMORY_BARRIER };
	out.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	out.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
	v.vkCmdPipelineBarrier( F.cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 1, &out, 0, nullptr, 0, nullptr );
	// 2. the rate quad over the tile grid
	VkClearValue clear[2] = {};
	VkRenderPassBeginInfo rbi { VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
	rbi.renderPass = D.clear ? D.passClear : D.pass;
	rbi.framebuffer = D.fb;
	rbi.renderArea = { { 0, 0 }, { std::uint32_t( D.tw ), std::uint32_t( D.th ) } };
	rbi.clearValueCount = 2;
	rbi.pClearValues = clear;
	v.vkCmdBeginRenderPass( F.cmd, &rbi, VK_SUBPASS_CONTENTS_INLINE );
	if ( !D.clear ) {
		const VkViewport vpt { 0.0f, 0.0f, float( D.tw ), float( D.th ), 0.0f, 1.0f };
		const VkRect2D sc { { 0, 0 }, { std::uint32_t( D.tw ), std::uint32_t( D.th ) } };
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
bool vrsRun( const VrsRun & r, unsigned tex[2], std::string & why )
{
	Fx & F = fx();
	VrsVk & D = vrsVk();
	View & V = view();
	Vk & v = vk();
	Gl & g = gl();
	FxStat & S = D.st;
	const double t0 = nowMs();
	tex[0] = tex[1] = 0;
	auto refuse = [&]( const std::string & w ) {
		why = w;
		S.fails++;
		S.failWhy = w;
		return false;
	};
	auto breakOff = [&]( const std::string & w ) {
		D.broken = true;
		D.brokenWhy = "the shading rate on Vulkan broke: " + w;
		return refuse( D.brokenWhy );
	};
	if ( D.broken )
		return refuse( D.brokenWhy );
	if ( !v.ok || !g.ok || g.ctx != reinterpret_cast<void *>( wglGetCurrentContext() ) )
		return refuse( "the present device or GL's entry points are not ready in this context" );
	if ( V.cur < 0 || !V.fbo || r.drawFb != V.fbo || !V.slot[V.cur].color || !V.slot[V.cur].colorTex )
		return refuse( "the picture is not in the view (GL's draw framebuffer is not viewFbo())" );
	if ( V.w != r.w || V.h != r.h || r.w < 1 || r.h < 1 || r.tw < 1 || r.th < 1 )
		return refuse( "the shading rate's size is not the view's" );
	if ( tonePending() )
		return refuse( "the tone pass is deferred to the TAA's crossing: the view does not hold it yet" );
	// the last crossing (any fx pass) is done before its command buffer, set, framebuffer and buffer are written again
	if ( std::string fw; !fenceDone( F.fence, F.pending, "vrs", fw ) )
		return refuse( fw );
	F.pending = false;
	std::string w;
	if ( !fxStatic( w ) || !fxSems( w ) || !vrsStatic( w ) )
		return breakOff( w );
	// the route's twins: the resolved picture (RGBA8, the view's size) and the two rate images (R8UI, the tile grid)
	const std::uintptr_t key = reinterpret_cast<std::uintptr_t>( &D );
	unsigned srcTw = 0, rate[2] = {};
	if ( !sharedTarget( key, 0, r.w, r.h, 1, kRgba8, srcTw, w ) || !srcTw )
		return refuse( "the shading rate's source twin: " + w );
	for ( int k = 0; k < 2; k++ )
		if ( !sharedTarget( key + 1 + std::uintptr_t( k ), 0, r.tw, r.th, 1, kGlR8ui, rate[k], w ) || !rate[k] )
			return refuse( "the shading rate's target twin: " + w );
	const Shared * sTw = sharedFind( key );
	const Shared * rTw[2] = { sharedFind( key + 1 ), sharedFind( key + 2 ) };
	if ( !sTw || !sTw->view || !sTw->img || !rTw[0] || !rTw[0]->view || !rTw[1] || !rTw[1]->view )
		return refuse( "the shading rate's twins have no view" );
	FxNames names;
	// vrsMv: a shared target read as is (the TAA on Vulkan), else GL's RG32F copied into a twin; off: the source (unread)
	VkImageView mvView = sTw->view;
	if ( r.mv ) {
		WwVkPresent::FxTex t;
		for ( const auto & kv : shareds() )
			if ( kv.second.tex == r.mv && !kv.second.borrowed && kv.second.glCtx == g.ctx ) {
				t.shared = kv.first;
				break;
			}
		t.tex = r.mv;
		t.type = 0;
		t.w = r.w;
		t.h = r.h;
		t.glFmt = kGlRg32f;
		t.twin = key + 3;
		int copies = 0;
		if ( !fxSlot( t, mvView, names, copies, w ) )
			return refuse( "vrsMv: " + w );
	}
	const VkImageView views[2] = { rTw[0]->view, rTw[1]->view };
	if ( !fxFb( D.fb, D.pass, views, 2, r.tw, r.th ) )
		return breakOff( "the shading rate's framebuffer over its twins failed" );
	// b31 (kT_vrs)
	std::uint8_t * u = D.uboPtr + kFxLooseAt;
	std::memset( u, 0, size_t( kT_vrsBytes ) );
	const std::int32_t mvOn = r.mv ? 1 : 0, maxRate = ( r.red == 1 ) ? 2 : r.maxRate;  // red 1: no 4x
	const float size[2] = { float( r.w ), float( r.h ) }, tile[2] = { float( r.texelW ), float( r.texelH ) };
	std::memcpy( u + 0, &mvOn, 4 );
	std::memcpy( u + 4, size, 8 );
	std::memcpy( u + 16, tile, 8 );
	std::memcpy( u + 24, &r.cut, 4 );
	std::memcpy( u + 28, &r.cutSorted, 4 );
	std::memcpy( u + 32, &maxRate, 4 );
	std::memcpy( u + 36, &r.motion, 4 );
	VkDescriptorBufferInfo bi { D.ubo, kFxLooseAt, VkDeviceSize( kT_vrsBytes ) };
	VkDescriptorImageInfo ii[2] = {};
	ii[0].imageView = sTw->view;
	ii[1].imageView = mvView;
	ii[0].imageLayout = ii[1].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	VkWriteDescriptorSet ws[kVrsBindings] = { { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET }, { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET },
		{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET } };
	for ( int k = 0; k < kVrsBindings; k++ ) {
		ws[k].dstSet = D.set;
		ws[k].descriptorCount = 1;
		ws[k].dstBinding = k ? std::uint32_t( 63 + k ) : 31;
		ws[k].descriptorType = k ? VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE : VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		if ( k )
			ws[k].pImageInfo = &ii[k - 1];
		else
			ws[k].pBufferInfo = &bi;
	}
	v.vkUpdateDescriptorSets( v.dev, std::uint32_t( kVrsBindings ), ws, 0, nullptr );
	D.viewImg = V.slot[V.cur].color;
	D.srcImg = sTw->img;
	D.samples = std::max( V.samples, 1 );
	D.w = r.w;
	D.h = r.h;
	D.tw = r.tw;
	D.th = r.th;
	D.clear = r.red == 2;
	F.kind = 15;
	F.w = r.tw;
	F.h = r.th;
	const int vp[4] = { 0, 0, r.tw, r.th };
	std::memcpy( F.vp, vp, sizeof( F.vp ) );
	names.add( V.slot[V.cur].colorTex );
	names.add( srcTw );
	names.add( rate[0] );
	names.add( rate[1] );
	if ( names.full )
		return refuse( "the shading rate's GL names did not fit" );
	if ( !fxCross( names, w ) )
		return breakOff( w );  // nothing signals vkDone: GL does not wait it; GL draws its own pass this frame
	tex[0] = rate[0];
	tex[1] = rate[1];
	const double ms = nowMs() - t0;
	S.runs++;
	S.cpuMs += ms;
	S.maxMs = std::max( S.maxMs, ms );
	return true;
}

void vrsOnGl( const char * why )
{
	WwVkPresent::glGuard( "vrs", 1 );  // lane VKFLIGHT4: the view GL guard
	FxStat & S = vrsVk().st;
	S.gl++;
	S.glWhy = why ? why : "";
}

FxStat takeVrsStat()
{
	VrsVk & D = vrsVk();
	FxStat s = D.st;
	s.pipelines = D.pipelines;
	s.buildMs = D.buildMs;
	if ( D.broken && s.failWhy.empty() )
		s.failWhy = D.brokenWhy;
	D.st = FxStat();
	return s;
}
}  // namespace WwVkPresent
