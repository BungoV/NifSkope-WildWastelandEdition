/* lane VKCLOUD4 (2026-11-16): the cloud map's build (cloudmap.cpp buildMap, cloudmap.prog) on the present device,
 * through the VKAIR1 crossing (fx(): its fence, command buffer and semaphores); fxWork's kind 13. Included once by
 * vkpresent.cpp after vkcloudshadow.inc (it reads objTexKey / objTexFlush and farTwinFind / farSharedOf).
 * GL: the map's fbo (RGBA8 n x n), viewport n x n, clear 0, blend ONE_MINUS_DST_COLOR / ONE (the red "max": MAX), per
 * layer its band's colour mask, Tex / Tex2 on units 0 / 1 and one drawShape (the world-laid quad or the dome mesh).
 * Here: one render pass over the shared map, CLEAR to 0, the layers' vertices and indices in one host buffer, one set and
 * one indexed draw a layer, a pipeline per (colour mask, max). The viewport is not flipped: GL's NDC y -1 is its row 0
 * in memory, as Vulkan's. The layer textures (BC sheets from the texture cache) are read in place through the cache's
 * twins (objTexKey); a texture with no cache twin is copied into its own twin, else the run is refused by name. */
namespace
{
constexpr int kCmBindings = 6;  // b31 (PS) 31, b31 (VS) 32; t64, t65; s128, s129
constexpr VkDeviceSize kCmUboStride = 512, kCmVsAt = 256;

struct CloudMapDev
{
	bool made = false, broken = false;
	std::string brokenWhy;
	VkDescriptorSetLayout dsl = VK_NULL_HANDLE;
	VkDescriptorPool dpool = VK_NULL_HANDLE;
	VkDescriptorSet set[WwVkPresent::kCloudMapLayers] = {};
	VkPipelineLayout layout = VK_NULL_HANDLE;
	VkShaderModule vs = VK_NULL_HANDLE, ps = VK_NULL_HANDLE;
	VkBuffer ubo = VK_NULL_HANDLE, geo = VK_NULL_HANDLE;
	VkDeviceMemory uboMem = VK_NULL_HANDLE, geoMem = VK_NULL_HANDLE;
	std::uint8_t * uboPtr = nullptr, * geoPtr = nullptr;
	VkRenderPass pass = VK_NULL_HANDLE, passLoad = VK_NULL_HANDLE;  // CLEAR (the route), LOAD (red 2: same compatibility)
	VkPipeline pipe[16][2] = {};  // [colour mask][max]
	VkFramebuffer fb = VK_NULL_HANDLE;
	int pipelines = 0;
	double buildMs = 0.0;
	// this run's job (the drawing thread writes it before the hand, the worker records it)
	int n = 0, layers = 0;
	bool load = false;
	VkPipeline layerPipe[WwVkPresent::kCloudMapLayers] = {};
	int firstVertex[WwVkPresent::kCloudMapLayers] = {}, firstIndex[WwVkPresent::kCloudMapLayers] = {}, nIdx[WwVkPresent::kCloudMapLayers] = {};
	WwVkPresent::FxStat st;
	int inPlace = 0;  // layer textures read in place through the texture cache's twins (since the last take)
};
constexpr VkDeviceSize kCmIdxAt = VkDeviceSize( WwVkPresent::kCloudMapVerts ) * 36;
constexpr VkDeviceSize kCmGeoBytes = kCmIdxAt + VkDeviceSize( WwVkPresent::kCloudMapIndices ) * 2;

CloudMapDev & cloudMapDev()
{
	static CloudMapDev d;
	return d;
}

bool cmHostBuffer( VkDeviceSize size, VkBufferUsageFlags usage, VkBuffer & buf, VkDeviceMemory & mem, std::uint8_t *& ptr )
{
	Vk & v = vk();
	VkBufferCreateInfo bci { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
	bci.size = size;
	bci.usage = usage;
	if ( v.vkCreateBuffer( v.dev, &bci, nullptr, &buf ) != VK_SUCCESS )
		return false;
	VkMemoryRequirements br {};
	v.vkGetBufferMemoryRequirements( v.dev, buf, &br );
	VkMemoryAllocateInfo ai { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
	ai.allocationSize = br.size;
	ai.memoryTypeIndex = memType( br.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT );
	void * p = nullptr;
	const bool ok = ai.memoryTypeIndex != UINT32_MAX && v.vkAllocateMemory( v.dev, &ai, nullptr, &mem ) == VK_SUCCESS
		&& v.vkBindBufferMemory( v.dev, buf, mem, 0 ) == VK_SUCCESS && v.vkMapMemory( v.dev, mem, 0, VK_WHOLE_SIZE, 0, &p ) == VK_SUCCESS;
	ptr = static_cast<std::uint8_t *>( p );
	return ok && ptr;
}

bool cmStatic( std::string & why )
{
	Vk & v = vk();
	CloudMapDev & D = cloudMapDev();
	if ( D.made )
		return true;
	const double t0 = nowMs();
	static const std::uint32_t kBind[kCmBindings] = { 31, 32, 64, 65, 128, 129 };
	VkDescriptorSetLayoutBinding b[kCmBindings] = {};
	for ( int k = 0; k < kCmBindings; k++ ) {
		b[k].binding = kBind[k];
		b[k].descriptorType = k < 2 ? VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER : k < 4 ? VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE : VK_DESCRIPTOR_TYPE_SAMPLER;
		b[k].descriptorCount = 1;
		b[k].stageFlags = k == 1 ? VK_SHADER_STAGE_VERTEX_BIT : VK_SHADER_STAGE_FRAGMENT_BIT;
	}
	VkDescriptorSetLayoutCreateInfo lci { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
	lci.bindingCount = kCmBindings;
	lci.pBindings = b;
	const std::uint32_t L = WwVkPresent::kCloudMapLayers;
	VkDescriptorPoolSize ps[3] = { { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 2 * L }, { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 2 * L },
		{ VK_DESCRIPTOR_TYPE_SAMPLER, 2 * L } };
	VkDescriptorPoolCreateInfo pci { VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
	pci.maxSets = L;
	pci.poolSizeCount = 3;
	pci.pPoolSizes = ps;
	if ( v.vkCreateDescriptorSetLayout( v.dev, &lci, nullptr, &D.dsl ) != VK_SUCCESS
		|| v.vkCreateDescriptorPool( v.dev, &pci, nullptr, &D.dpool ) != VK_SUCCESS ) {
		why = "the cloud map's descriptor set layout / pool failed";
		return false;
	}
	VkDescriptorSetLayout dsls[WwVkPresent::kCloudMapLayers];
	for ( auto & x : dsls )
		x = D.dsl;
	VkDescriptorSetAllocateInfo dai { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
	dai.descriptorPool = D.dpool;
	dai.descriptorSetCount = L;
	dai.pSetLayouts = dsls;
	VkPipelineLayoutCreateInfo plci { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
	plci.setLayoutCount = 1;
	plci.pSetLayouts = &D.dsl;
	if ( v.vkAllocateDescriptorSets( v.dev, &dai, D.set ) != VK_SUCCESS
		|| v.vkCreatePipelineLayout( v.dev, &plci, nullptr, &D.layout ) != VK_SUCCESS ) {
		why = "the cloud map's descriptor sets / pipeline layout failed";
		return false;
	}
	if ( !fxModule( vkframe_cloudmap_view_vs_spv, sizeof( vkframe_cloudmap_view_vs_spv ), D.vs )
		|| !fxModule( vkframe_cloudmap_view_ps_spv, sizeof( vkframe_cloudmap_view_ps_spv ), D.ps ) ) {
		why = "the cloud map's shader modules failed";
		return false;
	}
	if ( !cmHostBuffer( kCmUboStride * L, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, D.ubo, D.uboMem, D.uboPtr )
		|| !cmHostBuffer( kCmGeoBytes, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, D.geo, D.geoMem, D.geoPtr ) ) {
		why = "the cloud map's uniform / vertex buffers failed";
		return false;
	}
	std::memset( D.uboPtr, 0, size_t( kCmUboStride * L ) );
	// the render passes (RGBA8, one sample, STORE, GENERAL): CLEAR for the route, LOAD for red 2 (compatible)
	for ( int c = 0; c < 2; c++ ) {
		VkAttachmentDescription at {};
		at.format = VK_FORMAT_R8G8B8A8_UNORM;
		at.samples = VK_SAMPLE_COUNT_1_BIT;
		at.loadOp = c ? VK_ATTACHMENT_LOAD_OP_LOAD : VK_ATTACHMENT_LOAD_OP_CLEAR;
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
		if ( v.vkCreateRenderPass( v.dev, &rci, nullptr, c ? &D.passLoad : &D.pass ) != VK_SUCCESS ) {
			why = "vkCreateRenderPass (the cloud map) failed";
			return false;
		}
	}
	D.buildMs += nowMs() - t0;
	D.made = true;
	return true;
}

// one pipeline a (GL colour mask, max) pair, made on first use
VkPipeline cmPipe( int mask, bool max, std::string & why )
{
	Vk & v = vk();
	CloudMapDev & D = cloudMapDev();
	VkPipeline & p = D.pipe[mask & 15][max ? 1 : 0];
	if ( p )
		return p;
	const double t0 = nowMs();
	VkPipelineShaderStageCreateInfo st[2] = { { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO },
		{ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO } };
	st[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
	st[0].module = D.vs;
	st[0].pName = "main";
	st[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	st[1].module = D.ps;
	st[1].pName = "main";
	// drawShape's attributes 0 (position), 1 (colour), 7 (uv): the twin's TEXCOORD0 / 1 / 7 at locations 0 / 1 / 2
	const VkVertexInputBindingDescription vb { 0, 36, VK_VERTEX_INPUT_RATE_VERTEX };
	const VkVertexInputAttributeDescription va[3] = { { 0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0 }, { 1, 0, VK_FORMAT_R32G32B32A32_SFLOAT, 12 },
		{ 2, 0, VK_FORMAT_R32G32_SFLOAT, 28 } };
	VkPipelineVertexInputStateCreateInfo vi { VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
	vi.vertexBindingDescriptionCount = 1;
	vi.pVertexBindingDescriptions = &vb;
	vi.vertexAttributeDescriptionCount = 3;
	vi.pVertexAttributeDescriptions = va;
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
	cb.blendEnable = VK_TRUE;
	// GL's glBlendFunc( ONE_MINUS_DST_COLOR, ONE ) sets the alpha factors too (alpha's ONE_MINUS_DST_COLOR is 1 - dst a)
	cb.srcColorBlendFactor = cb.srcAlphaBlendFactor = max ? VK_BLEND_FACTOR_ONE : VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
	cb.dstColorBlendFactor = cb.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	cb.colorBlendOp = cb.alphaBlendOp = max ? VK_BLEND_OP_MAX : VK_BLEND_OP_ADD;
	cb.colorWriteMask = ( mask & 1 ? VK_COLOR_COMPONENT_R_BIT : 0 ) | ( mask & 2 ? VK_COLOR_COMPONENT_G_BIT : 0 )
		| ( mask & 4 ? VK_COLOR_COMPONENT_B_BIT : 0 ) | ( mask & 8 ? VK_COLOR_COMPONENT_A_BIT : 0 );
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
	if ( v.vkCreateGraphicsPipelines( v.dev, tone().cache, 1, &gp, nullptr, &p ) != VK_SUCCESS ) {
		p = VK_NULL_HANDLE;
		why = "vkCreateGraphicsPipelines (the cloud map, mask " + std::to_string( mask ) + ( max ? ", max" : "" ) + ") failed";
		return VK_NULL_HANDLE;
	}
	D.pipelines++;
	toneCacheSave();
	D.buildMs += nowMs() - t0;
	return p;
}

// the worker (fxWork, F.kind 13): every layer into the map
bool cmRecord( std::string & why )
{
	Vk & v = vk();
	Fx & F = fx();
	CloudMapDev & D = cloudMapDev();
	if ( !D.fb || D.layers < 1 ) {
		why = "no framebuffer / layers for the cloud map";
		return false;
	}
	VkClearValue clear {};
	VkRenderPassBeginInfo rbi { VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
	rbi.renderPass = D.load ? D.passLoad : D.pass;
	rbi.framebuffer = D.fb;
	rbi.renderArea = { { 0, 0 }, { std::uint32_t( D.n ), std::uint32_t( D.n ) } };
	rbi.clearValueCount = 1;
	rbi.pClearValues = &clear;
	v.vkCmdBeginRenderPass( F.cmd, &rbi, VK_SUBPASS_CONTENTS_INLINE );
	const VkViewport vpt { 0.0f, 0.0f, float( D.n ), float( D.n ), 0.0f, 1.0f };
	const VkRect2D sc { { 0, 0 }, { std::uint32_t( D.n ), std::uint32_t( D.n ) } };
	v.vkCmdSetViewport( F.cmd, 0, 1, &vpt );
	v.vkCmdSetScissor( F.cmd, 0, 1, &sc );
	const VkDeviceSize zero = 0;
	v.vkCmdBindVertexBuffers( F.cmd, 0, 1, &D.geo, &zero );
	v.vkCmdBindIndexBuffer( F.cmd, D.geo, kCmIdxAt, VK_INDEX_TYPE_UINT16 );
	VkPipeline bound = VK_NULL_HANDLE;
	for ( int k = 0; k < D.layers; k++ ) {
		if ( D.layerPipe[k] != bound ) {
			bound = D.layerPipe[k];
			v.vkCmdBindPipeline( F.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, bound );
		}
		v.vkCmdBindDescriptorSets( F.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, D.layout, 0, 1, &D.set[k], 0, nullptr );
		v.vkCmdDrawIndexed( F.cmd, std::uint32_t( D.nIdx[k] ), 1, std::uint32_t( D.firstIndex[k] ), D.firstVertex[k], 0 );
	}
	v.vkCmdEndRenderPass( F.cmd );
	return true;
}
}  // namespace

namespace WwVkPresent
{
bool cloudMapRun( const CloudMapRun & t, std::string & why )
{
	Fx & F = fx();
	CloudMapDev & D = cloudMapDev();
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
		D.brokenWhy = "the cloud map on Vulkan broke: " + w;
		return refuse( D.brokenWhy );
	};
	if ( D.broken )
		return refuse( D.brokenWhy );
	if ( t.n < 1 || t.layers < 1 || t.layers > kCloudMapLayers )
		return refuse( "no layers, or more than " + std::to_string( kCloudMapLayers ) );
	if ( !t.key )
		return refuse( "no map key" );
	if ( t.verts.size() > size_t( kCloudMapVerts ) * 9 || t.idx.size() > size_t( kCloudMapIndices ) )
		return refuse( "the layers' meshes are past the vertex buffer" );
	if ( !v.ok || !g.ok || g.ctx != reinterpret_cast<void *>( wglGetCurrentContext() ) )
		return refuse( "the present device or GL's entry points are not ready in this context" );
	if ( !g.copyImageSubData )
		return refuse( "no glCopyImageSubData (the inputs' copies into their shared twins)" );
	const Shared * tgt = sharedFind( t.key );
	if ( !tgt || !tgt->view || tgt->w != t.n || tgt->h != t.n || tgt->fmt != VK_FORMAT_R8G8B8A8_UNORM )
		return refuse( "the map is not a shared RGBA8 n x n target" );
	// the last crossing (any fx pass) is done before its command buffer, sets, framebuffer and buffers are written again
	if ( F.pending ) {
		v.vkWaitForFences( v.dev, 1, &F.fence, VK_TRUE, 1000000000ull );
		F.pending = false;
	}
	std::string w;
	if ( !fxStatic( w ) || !fxSems( w ) || !cmStatic( w ) )
		return breakOff( w );
	FxNames names;
	names.add( tgt->tex );
	VkImageView views[kCloudMapLayers][2] = {};
	int smp[kCloudMapLayers][2] = {};
	bool cacheTwins = false;
	for ( int k = 0; k < t.layers; k++ ) {
		const CloudMapLayer & l = t.layer[k];
		if ( l.nIdx < 1 || l.firstIndex + l.nIdx > int( t.idx.size() ) || l.firstVertex < 0 || l.firstVertex >= int( t.verts.size() / 9 ) )
			return refuse( "layer " + std::to_string( k ) + ": its mesh is outside the run's buffers" );
		for ( int s = 0; s < 2; s++ ) {
			FxTex x = s ? l.tex2 : l.tex;
			smp[k][s] = x.sampler;
			if ( x.tex && !x.shared ) {
				const std::uintptr_t ck = objTexKey( x.tex, 0 );
				const Shared * cs = ck ? farTwinFind( ck ) : nullptr;
				if ( cs && cs->view ) {  // the texture cache's twin of this layer texture: read in place, no copy
					views[k][s] = cs->view;
					if ( cs->tex )
						names.add( cs->tex );
					cacheTwins = true;
					D.inPlace++;
					continue;
				}
				x.shared = farSharedOf( x.tex );
			}
			if ( !fxSlot( x, views[k][s], names, S.copies, w ) )
				return refuse( "layer " + std::to_string( k ) + ( s ? " Tex2" : " Tex" ) + ": a texture with no cache twin: " + w );
		}
		const VkPipeline p = cmPipe( ( t.red & 8 ) ? 15 : l.mask, t.max, w );
		if ( !p )
			return breakOff( w );
		D.layerPipe[k] = p;
		D.firstVertex[k] = l.firstVertex;
		D.firstIndex[k] = l.firstIndex;
		D.nIdx[k] = l.nIdx;
	}
	if ( names.full )
		return refuse( "more GL names than a crossing hands over" );
	if ( cacheTwins && !objTexFlush( w ) )
		return refuse( "layer textures: the texture cache's twins are not on the device: " + w );
	if ( !fxFb( D.fb, D.pass, &tgt->view, 1, t.n, t.n ) )
		return breakOff( "the cloud map's framebuffer over its map failed" );
	std::memcpy( D.geoPtr, t.verts.data(), t.verts.size() * sizeof( float ) );
	std::memcpy( D.geoPtr + kCmIdxAt, t.idx.data(), t.idx.size() * sizeof( std::uint16_t ) );
	const bool swapTex = ( t.red & 1 ) != 0;  // red: Tex <-> Tex2
	for ( int k = 0; k < t.layers; k++ ) {
		const CloudMapLayer & l = t.layer[k];
		std::uint8_t * u = D.uboPtr + kCmUboStride * VkDeviceSize( k );
		std::memcpy( u, l.ps, size_t( kT_cloudmap_psBytes ) );
		std::memcpy( u + kCmVsAt, l.vs, size_t( kT_cloudmap_vsBytes ) );
		if ( t.red & 4 )  // red: uvOffset.y read 4 bytes early (a b31 field off its offset)
			std::memmove( u + kCmVsAt + 12, u + kCmVsAt + 8, 4 );
		VkDescriptorBufferInfo bi[2] = { { D.ubo, kCmUboStride * VkDeviceSize( k ), VkDeviceSize( kT_cloudmap_psBytes ) },
			{ D.ubo, kCmUboStride * VkDeviceSize( k ) + kCmVsAt, VkDeviceSize( kT_cloudmap_vsBytes ) } };
		VkDescriptorImageInfo ii[2][2] = {};
		for ( int s = 0; s < 2; s++ ) {
			const int src = swapTex ? 1 - s : s;
			ii[0][s].imageView = views[k][src];
			ii[0][s].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
			ii[1][s].sampler = F.smp[std::clamp( smp[k][src], 0, int( kFxSamplers ) - 1 )];
		}
		static const std::uint32_t kBind[kCmBindings] = { 31, 32, 64, 65, 128, 129 };
		VkWriteDescriptorSet ws[kCmBindings] = {};
		for ( int b = 0; b < kCmBindings; b++ ) {
			VkWriteDescriptorSet & x = ws[b];
			x.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			x.dstSet = D.set[k];
			x.dstBinding = kBind[b];
			x.descriptorCount = 1;
			if ( b < 2 ) {
				x.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
				x.pBufferInfo = &bi[b];
			} else {
				x.descriptorType = b < 4 ? VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE : VK_DESCRIPTOR_TYPE_SAMPLER;
				x.pImageInfo = &ii[b < 4 ? 0 : 1][( b - 2 ) & 1];
			}
		}
		v.vkUpdateDescriptorSets( v.dev, std::uint32_t( kCmBindings ), ws, 0, nullptr );
	}
	D.n = t.n;
	D.layers = t.layers;
	D.load = ( t.red & 2 ) != 0;
	F.kind = 13;
	F.w = t.n;
	F.h = t.n;
	const int vp[4] = { 0, 0, t.n, t.n };
	std::memcpy( F.vp, vp, sizeof( F.vp ) );
	if ( !fxCross( names, w ) )
		return breakOff( w );  // nothing signals vkDone: GL does not wait it; GL draws its own layers this frame
	const double ms = nowMs() - t0;
	S.runs++;
	S.cpuMs += ms;
	S.maxMs = std::max( S.maxMs, ms );
	return true;
}

void cloudMapOnGl( const char * why )
{
	WwVkPresent::glGuard( "cloudMap", 1 );  // lane VKFLIGHT4: the view GL guard
	FxStat & S = cloudMapDev().st;
	S.gl++;
	S.glWhy = why ? why : "";
}

FxStat takeCloudMapStat()
{
	CloudMapDev & D = cloudMapDev();
	FxStat s = D.st;
	s.pipelines = D.pipelines;
	s.buildMs = D.buildMs;
	s.refusedPacks = D.inPlace;  // the cloud map: layer textures read in place through the texture cache's twins
	if ( D.broken && s.failWhy.empty() )
		s.failWhy = D.brokenWhy;
	D.st = FxStat();
	D.inPlace = 0;
	return s;
}
}  // namespace WwVkPresent
