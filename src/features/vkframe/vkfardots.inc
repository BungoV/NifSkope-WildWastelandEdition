/* lane VKFAR1 (row G3, 2026-11-13): the far bulb dots (cell_fardots) on the present device, into the tone frame and its
 * stencil, through the VKAIR1 crossing (fx(): its fence, command buffer, semaphores, the air's render pass over the frame).
 * Included once by vkpresent.cpp after the VKAIR1 block (it reads that block's anonymous-namespace parts).
 * GL draws one GL_POINT a bulb with gl_PointSize; the present device has no geometry stage, so a bulb is an instance of
 * four strip vertices (cell_fardots_view_vs.hlsl) and cell_fardots_ps reads the quad's corner as gl_PointCoord.
 * GL's state (celllights.cpp wwCellFarDotsDraw): depth LEQUAL, no depth write, cull off, blend ONE / ONE, stencil ALWAYS
 * ref 1, REPLACE on depth pass, write mask 0x03. */
namespace
{
constexpr VkDeviceSize kFarViewBytes = 160;  // cell_fardots_view_vs FarDotsView (b1)

struct FarDots
{
	bool made = false, broken = false;
	std::string brokenWhy;
	PFN_vkCmdBindVertexBuffers bindVb = nullptr;  // not in the device table (Vk): loaded here
	VkDescriptorSetLayout dsl = VK_NULL_HANDLE;
	VkDescriptorPool dpool = VK_NULL_HANDLE;
	VkDescriptorSet set = VK_NULL_HANDLE;
	VkPipelineLayout layout = VK_NULL_HANDLE;
	VkShaderModule vs = VK_NULL_HANDLE, ps = VK_NULL_HANDLE;
	VkBuffer ubo = VK_NULL_HANDLE;  // the vertex block at 0, the fragment's b31 at kFxLooseAt
	VkDeviceMemory uboMem = VK_NULL_HANDLE;
	std::uint8_t * uboPtr = nullptr;
	VkBuffer vb = VK_NULL_HANDLE;  // per instance: n positions, then n intensities (3 floats each)
	VkDeviceMemory vbMem = VK_NULL_HANDLE;
	std::uint8_t * vbPtr = nullptr;
	VkDeviceSize vbCap = 0;
	VkPipeline pipe[3] = {};  // [0] GL's state, [1] red: no blend, [2] red: stencil write mask 0
	VkRenderPass pipePass = VK_NULL_HANDLE;
	int pipeSamples = 0, pipelines = 0;
	double buildMs = 0.0;
	// this run's job (the drawing thread writes it before the hand, the worker records it)
	int n = 0, pipeIx = 0;
	VkDeviceSize valueAt = 0;
	WwVkPresent::FxStat st;
};

FarDots & farDots()
{
	static FarDots d;
	return d;
}

bool farStatic( std::string & why )
{
	Vk & v = vk();
	FarDots & D = farDots();
	if ( D.made )
		return true;
	const double t0 = nowMs();
	D.bindVb = reinterpret_cast<PFN_vkCmdBindVertexBuffers>( v.vkGetDeviceProcAddr( v.dev, "vkCmdBindVertexBuffers" ) );
	if ( !D.bindVb ) {
		why = "no vkCmdBindVertexBuffers on the present device";
		return false;
	}
	// cell_fardots_view_vs snaps its corners to a 1/256 pixel grid, as this rasterizer does: other precisions stay on GL
	const auto props = reinterpret_cast<PFN_vkGetPhysicalDeviceProperties>( v.vkGetInstanceProcAddr( v.inst, "vkGetPhysicalDeviceProperties" ) );
	VkPhysicalDeviceProperties pp {};
	if ( props )
		props( v.phys, &pp );
	if ( !props || pp.limits.subPixelPrecisionBits != 8 ) {
		why = "the rasterizer's subpixel precision is " + std::to_string( props ? int( pp.limits.subPixelPrecisionBits ) : -1 )
			+ " bits, the far dots' vertex stage snaps to 8";
		return false;
	}
	// b1 the vertex block, b31 the fragment's loose uniforms, t0..t4 at 64.., s0..s4 at 128.. (vkframe_spirv.sh's shifts)
	VkDescriptorSetLayoutBinding b[12] = {};
	for ( int k = 0; k < 12; k++ ) {
		b[k].binding = k == 0 ? 1u : k == 1 ? 31u : k < 7 ? std::uint32_t( 64 + k - 2 ) : std::uint32_t( 128 + k - 7 );
		b[k].descriptorType = k < 2 ? VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER : k < 7 ? VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE : VK_DESCRIPTOR_TYPE_SAMPLER;
		b[k].descriptorCount = 1;
		b[k].stageFlags = k == 0 ? VK_SHADER_STAGE_VERTEX_BIT : VK_SHADER_STAGE_FRAGMENT_BIT;
	}
	VkDescriptorSetLayoutCreateInfo lci { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
	lci.bindingCount = 12;
	lci.pBindings = b;
	VkDescriptorPoolSize ps[3] = { { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 2 }, { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 5 }, { VK_DESCRIPTOR_TYPE_SAMPLER, 5 } };
	VkDescriptorPoolCreateInfo pci { VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
	pci.maxSets = 1;
	pci.poolSizeCount = 3;
	pci.pPoolSizes = ps;
	if ( v.vkCreateDescriptorSetLayout( v.dev, &lci, nullptr, &D.dsl ) != VK_SUCCESS
		|| v.vkCreateDescriptorPool( v.dev, &pci, nullptr, &D.dpool ) != VK_SUCCESS ) {
		why = "the far dots' descriptor set layout / pool failed";
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
		why = "the far dots' descriptor set / pipeline layout failed";
		return false;
	}
	if ( !fxModule( vkframe_fardots_view_vs_spv, sizeof( vkframe_fardots_view_vs_spv ), D.vs )
		|| !fxModule( vkframe_fardots_view_ps_spv, sizeof( vkframe_fardots_view_ps_spv ), D.ps ) ) {
		why = "the far dots' shader modules failed";
		return false;
	}
	VkBufferCreateInfo bci { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
	bci.size = kFxLooseAt + VkDeviceSize( WwVkPresent::kT_fardotsBytes );
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
		why = "the far dots' uniform buffer failed";
		return false;
	}
	std::memset( D.uboPtr, 0, size_t( bci.size ) );
	D.buildMs += nowMs() - t0;
	D.made = true;
	return true;
}

// the instance buffer, grown (the last run's fence already waited)
bool farVertices( VkDeviceSize need, std::string & why )
{
	Vk & v = vk();
	FarDots & D = farDots();
	if ( D.vb && D.vbCap >= need )
		return true;
	if ( D.vb )
		v.vkDestroyBuffer( v.dev, D.vb, nullptr );
	if ( D.vbMem )
		v.vkFreeMemory( v.dev, D.vbMem, nullptr );
	D.vb = VK_NULL_HANDLE;
	D.vbMem = VK_NULL_HANDLE;
	D.vbPtr = nullptr;
	D.vbCap = 0;
	const VkDeviceSize cap = std::max<VkDeviceSize>( need + need / 2, 64 * 1024 );
	VkBufferCreateInfo bci { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
	bci.size = cap;
	bci.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
	bool ok = v.vkCreateBuffer( v.dev, &bci, nullptr, &D.vb ) == VK_SUCCESS;
	if ( ok ) {
		VkMemoryRequirements br {};
		v.vkGetBufferMemoryRequirements( v.dev, D.vb, &br );
		VkMemoryAllocateInfo ai { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
		ai.allocationSize = br.size;
		ai.memoryTypeIndex = memType( br.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT );
		void * p = nullptr;
		ok = ai.memoryTypeIndex != UINT32_MAX && v.vkAllocateMemory( v.dev, &ai, nullptr, &D.vbMem ) == VK_SUCCESS
			&& v.vkBindBufferMemory( v.dev, D.vb, D.vbMem, 0 ) == VK_SUCCESS && v.vkMapMemory( v.dev, D.vbMem, 0, VK_WHOLE_SIZE, 0, &p ) == VK_SUCCESS;
		D.vbPtr = static_cast<std::uint8_t *>( p );
	}
	if ( !ok || !D.vbPtr ) {
		why = "the far dots' instance buffer (" + std::to_string( need ) + " bytes) failed";
		return false;
	}
	D.vbCap = cap;
	return true;
}

// one pipeline over the air's render pass (the tone frame + its depth / stencil at the frame's samples)
bool farPipe( int ix, VkRenderPass pass, int samples, VkPipeline & out, std::string & why )
{
	Vk & v = vk();
	FarDots & D = farDots();
	VkPipelineShaderStageCreateInfo st[2] = { { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO },
		{ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO } };
	st[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
	st[0].module = D.vs;
	st[0].pName = "main";
	st[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	st[1].module = D.ps;
	st[1].pName = "main";
	const VkVertexInputBindingDescription vb[2] = { { 0, 12, VK_VERTEX_INPUT_RATE_INSTANCE }, { 1, 12, VK_VERTEX_INPUT_RATE_INSTANCE } };
	const VkVertexInputAttributeDescription va[2] = { { 0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0 }, { 1, 1, VK_FORMAT_R32G32B32_SFLOAT, 0 } };
	VkPipelineVertexInputStateCreateInfo vi { VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
	vi.vertexBindingDescriptionCount = 2;
	vi.pVertexBindingDescriptions = vb;
	vi.vertexAttributeDescriptionCount = 2;
	vi.pVertexAttributeDescriptions = va;
	VkPipelineInputAssemblyStateCreateInfo ia { VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
	ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
	VkPipelineViewportStateCreateInfo vps { VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO };
	vps.viewportCount = vps.scissorCount = 1;
	VkPipelineRasterizationStateCreateInfo rs { VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
	rs.polygonMode = VK_POLYGON_MODE_FILL;
	rs.cullMode = VK_CULL_MODE_NONE;
	rs.frontFace = VK_FRONT_FACE_CLOCKWISE;
	rs.lineWidth = 1.0f;
	VkPipelineMultisampleStateCreateInfo ms { VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
	ms.rasterizationSamples = VkSampleCountFlagBits( samples );
	VkPipelineDepthStencilStateCreateInfo ds { VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };
	ds.depthTestEnable = VK_TRUE;
	ds.depthWriteEnable = VK_FALSE;
	ds.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
	ds.stencilTestEnable = VK_TRUE;
	ds.front.failOp = ds.front.depthFailOp = VK_STENCIL_OP_KEEP;
	ds.front.passOp = VK_STENCIL_OP_REPLACE;
	ds.front.compareOp = VK_COMPARE_OP_ALWAYS;
	ds.front.compareMask = 0xFF;
	ds.front.writeMask = ix == 2 ? 0x00 : 0x03;
	ds.back = ds.front;
	VkPipelineColorBlendAttachmentState cb {};
	cb.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	if ( ix != 1 ) {  // glBlendFunc( ONE, ONE )
		cb.blendEnable = VK_TRUE;
		cb.srcColorBlendFactor = cb.dstColorBlendFactor = cb.srcAlphaBlendFactor = cb.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
		cb.colorBlendOp = cb.alphaBlendOp = VK_BLEND_OP_ADD;
	}
	VkPipelineColorBlendStateCreateInfo cbs { VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
	cbs.attachmentCount = 1;
	cbs.pAttachments = &cb;
	const VkDynamicState dyn[3] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR, VK_DYNAMIC_STATE_STENCIL_REFERENCE };
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
	gp.layout = D.layout;
	gp.renderPass = pass;
	if ( v.vkCreateGraphicsPipelines( v.dev, tone().cache, 1, &gp, nullptr, &out ) != VK_SUCCESS ) {
		why = "vkCreateGraphicsPipelines (the far dots) failed";
		return false;
	}
	D.pipelines++;
	return true;
}

// the pipelines follow the air's render pass (remade when the frame's sample count changes)
bool farPipes( std::string & why )
{
	Vk & v = vk();
	Fx & F = fx();
	FarDots & D = farDots();
	if ( D.pipePass == F.airPass && D.pipeSamples == F.airSamples && D.pipe[0] )
		return true;
	const double t0 = nowMs();
	for ( VkPipeline & p : D.pipe )
		if ( p ) {
			v.vkDestroyPipeline( v.dev, p, nullptr );
			p = VK_NULL_HANDLE;
		}
	D.pipePass = VK_NULL_HANDLE;
	for ( int k = 0; k < 3; k++ )
		if ( !farPipe( k, F.airPass, F.airSamples, D.pipe[k], why ) )
			return false;
	D.pipePass = F.airPass;
	D.pipeSamples = F.airSamples;
	D.buildMs += nowMs() - t0;
	toneCacheSave();
	return true;
}

// a GL texture name another pass already made on the present device (the air's integrated volume): read where it is
std::uintptr_t farSharedOf( unsigned tex )
{
	if ( !tex )
		return 0;
	for ( const auto & s : shareds() )
		if ( s.second.tex == tex && s.second.glCtx == gl().ctx && s.second.view )
			return s.first;
	return 0;
}

// the worker (fxWork, F.kind 2): the instanced quads into the tone frame
bool farRecord( std::string & why )
{
	Vk & v = vk();
	Fx & F = fx();
	FarDots & D = farDots();
	if ( !F.fbAir || !D.pipe[D.pipeIx] || !D.vb ) {
		why = "no framebuffer / pipeline / instance buffer for the far dots";
		return false;
	}
	VkRenderPassBeginInfo rbi { VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
	rbi.renderPass = F.airPass;
	rbi.framebuffer = F.fbAir;
	rbi.renderArea = { { 0, 0 }, { std::uint32_t( F.w ), std::uint32_t( F.h ) } };
	v.vkCmdBeginRenderPass( F.cmd, &rbi, VK_SUBPASS_CONTENTS_INLINE );
	// GL's viewport rows from the bottom = the image's rows from the top in memory: no flip (FragCoord.y = GL's)
	const VkViewport vpt { float( F.vp[0] ), float( F.vp[1] ), float( F.vp[2] ), float( F.vp[3] ), 0.0f, 1.0f };
	const int x0 = std::clamp( F.vp[0], 0, F.w ), y0 = std::clamp( F.vp[1], 0, F.h );
	const int x1 = std::clamp( F.vp[0] + F.vp[2], 0, F.w ), y1 = std::clamp( F.vp[1] + F.vp[3], 0, F.h );
	const VkRect2D sc { { x0, y0 }, { std::uint32_t( std::max( x1 - x0, 0 ) ), std::uint32_t( std::max( y1 - y0, 0 ) ) } };
	v.vkCmdSetViewport( F.cmd, 0, 1, &vpt );
	v.vkCmdSetScissor( F.cmd, 0, 1, &sc );
	v.vkCmdBindPipeline( F.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, D.pipe[D.pipeIx] );
	v.vkCmdSetStencilReference( F.cmd, VK_STENCIL_FACE_FRONT_AND_BACK, 1u );
	v.vkCmdBindDescriptorSets( F.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, D.layout, 0, 1, &D.set, 0, nullptr );
	const VkBuffer bufs[2] = { D.vb, D.vb };
	const VkDeviceSize offs[2] = { 0, D.valueAt };
	D.bindVb( F.cmd, 0, 2, bufs, offs );
	v.vkCmdDraw( F.cmd, 4, std::uint32_t( D.n ), 0, 0 );
	v.vkCmdEndRenderPass( F.cmd );
	return true;
}
}  // namespace

namespace WwVkPresent
{
bool farDotsRun( const FarDotsRun & t, std::string & why )
{
	Fx & F = fx();
	FarDots & D = farDots();
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
		D.brokenWhy = "the far dots on Vulkan broke: " + w;
		return refuse( D.brokenWhy );
	};
	if ( D.broken )
		return refuse( D.brokenWhy );
	if ( !toneFrameIs( t.color ) || !T.colorView || !T.dsView )
		return refuse( "the draw target is not the shared tone frame" );
	if ( T.w != t.w || T.h != t.h )
		return refuse( "the far dots' size is not the tone frame's" );
	if ( t.vp[2] < 1 || t.vp[3] < 1 )
		return refuse( "an empty viewport" );
	if ( t.n < 1 || !t.pos || !t.value )
		return refuse( "no dots" );
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
	const int samples = T.samples > 0 ? T.samples : std::max( F.airSamples, 1 );
	if ( !fxStatic( w ) || !fxSems( w ) || !fxPasses( samples, w ) || !farStatic( w ) || !farPipes( w ) )
		return breakOff( w );
	if ( F.airSamples != T.samples )
		return breakOff( "the far dots' pipelines are not at the frame's sample count" );
	const VkDeviceSize half = VkDeviceSize( t.n ) * 12;
	if ( !farVertices( 2 * half, w ) )
		return breakOff( w );
	FxNames names;
	names.add( T.colorTex );
	names.add( T.dsTex );
	VkImageView views[5] = {};
	int smp[5] = {};
	for ( int k = 0; k < 5; k++ ) {
		FxTex x = t.tex[k];
		if ( !x.shared && x.tex )
			x.shared = farSharedOf( x.tex );
		if ( !fxSlot( x, views[k], names, S.copies, w ) )
			return refuse( "the far dots' input t" + std::to_string( k ) + ": " + w );
		smp[k] = x.sampler;
	}
	const VkImageView att[2] = { T.colorView, T.dsView };
	if ( !fxFb( F.fbAir, F.airPass, att, 2, T.w, T.h ) )
		return breakOff( "the far dots' framebuffer over the tone frame failed" );
	// the instances, the vertex block (cell_fardots_view_vs FarDotsView), the fragment's b31
	std::memcpy( D.vbPtr, t.pos, size_t( half ) );
	std::memcpy( D.vbPtr + half, t.value, size_t( half ) );
	std::uint8_t * u = D.uboPtr;
	std::memset( u, 0, size_t( kFarViewBytes ) );
	std::memcpy( u, t.dotRow, 48 );
	std::memcpy( u + 48, t.dotProj, 64 );
	std::memcpy( u + 112, t.dotCam, 12 );
	std::memcpy( u + 124, &t.dotRed, 4 );
	std::memcpy( u + 128, t.dotBand, 8 );
	std::memcpy( u + 136, &t.dotFocal, 4 );
	std::memcpy( u + 140, &t.dotBulb, 4 );
	int vpv[4] = { t.vp[0], t.vp[1], t.vp[2], t.vp[3] };
	if ( t.red & 4 ) {  // red: the quads twice GL's point size
		vpv[2] = std::max( vpv[2] / 2, 1 );
		vpv[3] = std::max( vpv[3] / 2, 1 );
	}
	std::memcpy( u + 144, vpv, 16 );
	std::memcpy( u + kFxLooseAt, t.loose, size_t( kT_fardotsBytes ) );
	VkDescriptorBufferInfo bi[2] = { { D.ubo, 0, kFarViewBytes }, { D.ubo, kFxLooseAt, VkDeviceSize( kT_fardotsBytes ) } };
	VkDescriptorImageInfo ii[10] = {};
	VkWriteDescriptorSet ws[12] = {};
	for ( int k = 0; k < 12; k++ ) {
		ws[k].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		ws[k].dstSet = D.set;
		ws[k].descriptorCount = 1;
		if ( k < 2 ) {
			ws[k].dstBinding = k ? 31u : 1u;
			ws[k].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			ws[k].pBufferInfo = &bi[k];
		} else if ( k < 7 ) {
			ii[k - 2].imageView = views[k - 2];
			ii[k - 2].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
			ws[k].dstBinding = std::uint32_t( 64 + k - 2 );
			ws[k].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
			ws[k].pImageInfo = &ii[k - 2];
		} else {
			ii[k - 2].sampler = F.smp[std::clamp( smp[k - 7], 0, int( kFxSamplers ) - 1 )];
			ws[k].dstBinding = std::uint32_t( 128 + k - 7 );
			ws[k].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
			ws[k].pImageInfo = &ii[k - 2];
		}
	}
	v.vkUpdateDescriptorSets( v.dev, 12, ws, 0, nullptr );
	D.n = t.n;
	D.valueAt = half;
	D.pipeIx = ( t.red & 2 ) ? 1 : ( t.red & 1 ) ? 2 : 0;
	F.kind = 2;
	F.w = t.w;
	F.h = t.h;
	std::memcpy( F.vp, t.vp, sizeof( F.vp ) );
	if ( !fxCross( names, w ) )
		return breakOff( w );  // nothing signals vkDone: GL does not wait it; GL draws its points this frame
	const double ms = nowMs() - t0;
	S.runs++;
	S.cpuMs += ms;
	S.maxMs = std::max( S.maxMs, ms );
	return true;
}

void farDotsOnGl( const char * why )
{
	FxStat & S = farDots().st;
	S.gl++;
	S.glWhy = why ? why : "";
}

FxStat takeFarDotsStat()
{
	FarDots & D = farDots();
	FxStat s = D.st;
	s.pipelines = D.pipelines;
	s.buildMs = D.buildMs;
	if ( D.broken && s.failWhy.empty() )
		s.failWhy = D.brokenWhy;
	D.st = FxStat();
	return s;
}
}  // namespace WwVkPresent
