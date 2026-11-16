/* lane VKSMALL5 (2026-11-16, census row 77): the soft-effect layer's two full-screen passes (cellfxres.cpp
 * wwCellFxResDraw step 1 cell_fxres_down, step 3 cell_fxres_comp) on the present device; fxWork's kind 16.
 * Step 1: R32F z + D32F depth at the layer's size, depth ALWAYS written, fxrSrc = the opaque depth (in place when shared,
 * else copied into a same-format twin). Step 3: into the shared tone frame (colour + stencil, its sample count), per
 * pixel, depth off, stencil ALWAYS ref 1 mask 0x03 REPLACE, blend ONE / SRC1_COLOR (rgb), ONE / SRC1_ALPHA (a). Each pass
 * is its own crossing (GL waits on Vulkan: the soft draws read step 1, step 4's late draws land on step 3). */
#include <iterator>
#include <map>
#include <vector>

namespace
{
constexpr int kFxrDownBindings = 2;  // b31; t64 fxrSrc
constexpr int kFxrCompBindings = 6;  // b31; t64 fxrC, t65 fxrT, t66 fxrCov, t67 fxrZ, t68 fxrDepth
constexpr VkDeviceSize kFxrDownUbo = 0, kFxrDownBytes = 16, kFxrCompUbo = 256, kFxrCompBytes = 32, kFxrUboBytes = 512;
constexpr unsigned kFxrFmt[5] = { 0x822E, 0x8CAC, 0x881A, 0x881A, 0x8229 };  // z R32F, depth D32F, c / t RGBA16F, cov R8

struct FxresOwner
{
	char block[8] = {};  // its address keys the shared targets: +0..4 the layer (z, depth, c, t, cov), +5 fxrSrc's twin
	WwVkPresent::FxresTargets t;
	int w = 0, h = 0;
};

struct FxresVk
{
	bool made = false, broken = false;
	std::string brokenWhy;
	VkDescriptorSetLayout dsl[2] = {};  // 0 the downsample, 1 the composite
	VkDescriptorPool dpool = VK_NULL_HANDLE;
	VkDescriptorSet set[2] = {};
	VkPipelineLayout layout[2] = {};
	VkShaderModule ps[2] = {};
	VkShaderModule redPs = VK_NULL_HANDLE;  // red 4: the downsample with FMin patched to FMax
	int redPatches = 0;
	VkBuffer ubo = VK_NULL_HANDLE;
	VkDeviceMemory uboMem = VK_NULL_HANDLE;
	std::uint8_t * uboPtr = nullptr;
	VkRenderPass downPass = VK_NULL_HANDLE, compPass = VK_NULL_HANDLE;
	int compSamples = 0;
	VkPipeline downPipe = VK_NULL_HANDLE, downRedPipe = VK_NULL_HANDLE;
	std::map<int, VkPipeline> compPipes;  // by red (0 the route, 1 / 2 the gate's reds)
	VkFramebuffer downFb = VK_NULL_HANDLE, compFb = VK_NULL_HANDLE;
	unsigned copyTwin = 0;  // the twin whose copy was checked for a GL error
	std::map<const void *, FxresOwner> owners;  // node addresses are stable: the keys
	int pipelines = 0;
	double buildMs = 0.0;
	// this run's job (the drawing thread writes it before the hand, the worker records it)
	int phase = 0, w = 0, h = 0, vp[4] = {};
	VkPipeline pipe = VK_NULL_HANDLE;
	WwVkPresent::FxStat st;
};

FxresVk & fxresVk()
{
	static FxresVk d;
	return d;
}

std::uintptr_t fxrBase( FxresOwner & O )
{
	return reinterpret_cast<std::uintptr_t>( O.block );
}

// one pipeline over the full-screen triangle: comp false = the downsample (depth ALWAYS written), true = the composite
VkPipeline fxrPipe( VkRenderPass pass, VkPipelineLayout layout, VkShaderModule ps, int samples, bool comp, int red )
{
	Vk & v = vk();
	Fx & F = fx();
	VkPipelineShaderStageCreateInfo st[2] = { { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO },
		{ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO } };
	st[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
	st[0].module = F.vs;  // tone_view_vs: GL's quad covers the same pixels
	st[0].pName = "main";
	st[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	st[1].module = ps;
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
	ms.rasterizationSamples = VkSampleCountFlagBits( samples );  // per pixel: GL's composite reads no gl_SampleID
	VkPipelineDepthStencilStateCreateInfo ds { VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };
	VkPipelineColorBlendAttachmentState cb {};
	cb.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	if ( comp ) {
		// glStencilMask 0x03, glStencilFunc ALWAYS 1 0xFF, glStencilOp KEEP KEEP REPLACE; depth test off
		VkStencilOpState so {};
		so.failOp = VK_STENCIL_OP_KEEP;
		so.passOp = VK_STENCIL_OP_REPLACE;
		so.depthFailOp = VK_STENCIL_OP_KEEP;
		so.compareOp = VK_COMPARE_OP_ALWAYS;
		so.compareMask = 0xFF;
		so.writeMask = ( red & 1 ) ? 0u : 0x03u;  // red 1: no stencil written
		so.reference = 1;
		ds.stencilTestEnable = VK_TRUE;
		ds.front = ds.back = so;
		// glBlendFuncSeparate ONE SRC1_COLOR ONE SRC1_ALPHA, ADD (red 2: dst ONE)
		cb.blendEnable = VK_TRUE;
		cb.srcColorBlendFactor = cb.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
		cb.dstColorBlendFactor = ( red & 2 ) ? VK_BLEND_FACTOR_ONE : VK_BLEND_FACTOR_SRC1_COLOR;
		cb.dstAlphaBlendFactor = ( red & 2 ) ? VK_BLEND_FACTOR_ONE : VK_BLEND_FACTOR_SRC1_ALPHA;
		cb.colorBlendOp = cb.alphaBlendOp = VK_BLEND_OP_ADD;
	} else {
		ds.depthTestEnable = VK_TRUE;
		ds.depthWriteEnable = VK_TRUE;
		ds.depthCompareOp = VK_COMPARE_OP_ALWAYS;
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
	gp.layout = layout;
	gp.renderPass = pass;
	const double t0 = nowMs();
	VkPipeline p = VK_NULL_HANDLE;
	if ( v.vkCreateGraphicsPipelines( v.dev, tone().cache, 1, &gp, nullptr, &p ) != VK_SUCCESS )
		return VK_NULL_HANDLE;
	FxresVk & D = fxresVk();
	D.pipelines++;
	D.buildMs += nowMs() - t0;
	toneCacheSave();
	return p;
}

bool fxrStatic( std::string & why )
{
	Vk & v = vk();
	FxresVk & D = fxresVk();
	if ( D.made )
		return true;
	const double t0 = nowMs();
	const int nb[2] = { kFxrDownBindings, kFxrCompBindings };
	for ( int k = 0; k < 2; k++ ) {
		VkDescriptorSetLayoutBinding b[kFxrCompBindings] = {};
		b[0].binding = 31;
		b[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		for ( int i = 1; i < nb[k]; i++ ) {
			b[i].binding = std::uint32_t( 63 + i );
			b[i].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
		}
		for ( VkDescriptorSetLayoutBinding & x : b ) {
			x.descriptorCount = 1;
			x.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
		}
		VkDescriptorSetLayoutCreateInfo lci { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
		lci.bindingCount = std::uint32_t( nb[k] );
		lci.pBindings = b;
		if ( v.vkCreateDescriptorSetLayout( v.dev, &lci, nullptr, &D.dsl[k] ) != VK_SUCCESS ) {
			why = "the soft-effect layer's descriptor set layout failed";
			return false;
		}
	}
	VkDescriptorPoolSize ps[2] = { { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 2 },
		{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, std::uint32_t( kFxrDownBindings + kFxrCompBindings - 2 ) } };
	VkDescriptorPoolCreateInfo pci { VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
	pci.maxSets = 2;
	pci.poolSizeCount = 2;
	pci.pPoolSizes = ps;
	if ( v.vkCreateDescriptorPool( v.dev, &pci, nullptr, &D.dpool ) != VK_SUCCESS ) {
		why = "the soft-effect layer's descriptor pool failed";
		return false;
	}
	VkDescriptorSetAllocateInfo dai { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
	dai.descriptorPool = D.dpool;
	dai.descriptorSetCount = 2;
	dai.pSetLayouts = D.dsl;
	if ( v.vkAllocateDescriptorSets( v.dev, &dai, D.set ) != VK_SUCCESS ) {
		why = "the soft-effect layer's descriptor sets failed";
		return false;
	}
	for ( int k = 0; k < 2; k++ ) {
		VkPipelineLayoutCreateInfo plci { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
		plci.setLayoutCount = 1;
		plci.pSetLayouts = &D.dsl[k];
		if ( v.vkCreatePipelineLayout( v.dev, &plci, nullptr, &D.layout[k] ) != VK_SUCCESS ) {
			why = "the soft-effect layer's pipeline layout failed";
			return false;
		}
	}
	if ( !fxModule( vkframe_fxres_down_view_ps_spv, sizeof( vkframe_fxres_down_view_ps_spv ), D.ps[0] )
		|| !fxModule( vkframe_fxres_comp_view_ps_spv, sizeof( vkframe_fxres_comp_view_ps_spv ), D.ps[1] ) ) {
		why = "the soft-effect layer's shader modules failed";
		return false;
	}
	VkBufferCreateInfo bci { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
	bci.size = kFxrUboBytes;
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
		why = "the soft-effect layer's uniform buffer failed";
		return false;
	}
	std::memset( D.uboPtr, 0, size_t( kFxrUboBytes ) );
	// the downsample's pass: z R32F + depth D32F, one sample, every texel written (DONT_CARE), GENERAL
	VkAttachmentDescription at[2] = {};
	at[0].format = VK_FORMAT_R32_SFLOAT;
	at[1].format = VK_FORMAT_D32_SFLOAT;
	for ( VkAttachmentDescription & a : at ) {
		a.samples = VK_SAMPLE_COUNT_1_BIT;
		a.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		a.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		a.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		a.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
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
	if ( v.vkCreateRenderPass( v.dev, &rci, nullptr, &D.downPass ) != VK_SUCCESS ) {
		why = "vkCreateRenderPass (the layer's downsample) failed";
		return false;
	}
	D.buildMs += nowMs() - t0;
	D.downPipe = fxrPipe( D.downPass, D.layout[0], D.ps[0], 1, false, 0 );
	if ( !D.downPipe ) {
		why = "vkCreateGraphicsPipelines (the layer's downsample) failed";
		return false;
	}
	D.made = true;
	return true;
}

// the composite's pass over the tone frame (the air pass's shape: RGBA16F + D24S8 at its sample count, LOAD / STORE)
bool fxrCompPass( int samples, std::string & why )
{
	Vk & v = vk();
	FxresVk & D = fxresVk();
	if ( D.compPass && D.compSamples == samples )
		return true;
	for ( auto & kv : D.compPipes )
		if ( kv.second )
			v.vkDestroyPipeline( v.dev, kv.second, nullptr );
	D.compPipes.clear();
	if ( D.compPass )
		v.vkDestroyRenderPass( v.dev, D.compPass, nullptr );
	D.compPass = VK_NULL_HANDLE;
	D.compSamples = 0;
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
	if ( v.vkCreateRenderPass( v.dev, &rci, nullptr, &D.compPass ) != VK_SUCCESS ) {
		why = "vkCreateRenderPass (the layer's composite) failed";
		return false;
	}
	D.compSamples = samples;
	return true;
}

VkPipeline fxrCompPipe( int red )
{
	FxresVk & D = fxresVk();
	VkPipeline & p = D.compPipes[red];
	if ( !p )
		p = fxrPipe( D.compPass, D.layout[1], D.ps[1], D.compSamples, true, red );
	return p;
}

// red 4 (gates only): the downsample's SPIR-V with every GLSL.std.450 FMin (37) / NMin (79) patched to FMax (40) / NMax (80), in a copy
bool fxrDownRed( std::string & why )
{
	FxresVk & D = fxresVk();
	if ( D.downRedPipe )
		return true;
	std::vector<std::uint32_t> words( std::begin( vkframe_fxres_down_view_ps_spv ), std::end( vkframe_fxres_down_view_ps_spv ) );
	int patched = 0;
	for ( size_t i = 5; i < words.size(); ) {
		const std::uint32_t n = words[i] >> 16, op = words[i] & 0xFFFFu;
		if ( n == 0 || i + n > words.size() )
			break;
		if ( op == 12 && n >= 5 && ( words[i + 4] == 37 || words[i + 4] == 79 ) ) {  // OpExtInst: type, id, set, instruction
			words[i + 4] = words[i + 4] == 37 ? 40 : 80;  // FMin -> FMax, NMin (DXC's float min) -> NMax
			patched++;
		}
		i += n;
	}
	D.redPatches = patched;
	if ( !patched ) {
		why = "red 4: no FMin / NMin in the downsample's SPIR-V";
		return false;
	}
	if ( !fxModule( words.data(), words.size() * sizeof( std::uint32_t ), D.redPs ) ) {
		why = "red 4: the patched shader module failed";
		return false;
	}
	D.downRedPipe = fxrPipe( D.downPass, D.layout[0], D.redPs, 1, false, 0 );
	if ( !D.downRedPipe ) {
		why = "red 4: the patched pipeline failed";
		return false;
	}
	return true;
}

// fxrSrc / fxrDepth (the opaque depth): read in place when shared (srcKey), else copied into the owner's twin each crossing
bool fxrSource( FxresOwner & O, int W, int H, unsigned src, unsigned srcFmt, std::uintptr_t srcKey, VkImageView & view, unsigned & name,
	std::string & why )
{
	Gl & g = gl();
	FxresVk & D = fxresVk();
	view = VK_NULL_HANDLE;
	name = 0;
	if ( !src ) {
		why = "no opaque depth (fxrSrc)";
		return false;
	}
	if ( srcKey ) {
		const Shared * s = sharedFind( srcKey );
		if ( !s || s->tex != src || !s->view || s->w != W || s->h != H || s->samples != 1 ) {
			why = "fxrSrc's shared key does not hold it at the frame's size";
			return false;
		}
		view = s->view;
		name = src;
		return true;
	}
	const std::uintptr_t key = fxrBase( O ) + 5;
	unsigned twin = 0;
	std::string w;
	if ( !WwVkPresent::sharedTarget( key, 0, W, H, 1, srcFmt, twin, w ) || !twin ) {
		why = "fxrSrc's twin: " + w;
		return false;
	}
	const Shared * s = sharedFind( key );
	if ( !s || !s->view ) {
		why = "fxrSrc's twin has no view";
		return false;
	}
	const bool check = twin != D.copyTwin;
	if ( check )
		while ( g.getError() != 0 ) {
		}
	g.copyImageSubData( src, 0x0DE1, 0, 0, 0, 0, twin, 0x0DE1, 0, 0, 0, 0, W, H, 1 );  // GL_TEXTURE_2D
	if ( check ) {
		const unsigned e = g.getError();
		if ( e != 0 ) {
			why = "glCopyImageSubData into fxrSrc's twin raised GL error " + std::to_string( e );
			return false;
		}
		D.copyTwin = twin;
	}
	D.st.copies++;
	view = s->view;
	name = twin;
	return true;
}

FxresOwner * fxrOwner( const void * owner, int W, int H, int scale, std::string & why )
{
	FxresVk & D = fxresVk();
	auto it = D.owners.find( owner );
	if ( !owner || it == D.owners.end() ) {
		why = "no layer targets for this owner (fxresTargets first)";
		return nullptr;
	}
	FxresOwner & O = it->second;
	if ( scale < 1 || W < 1 || H < 1 || ( W + scale - 1 ) / scale != O.w || ( H + scale - 1 ) / scale != O.h ) {
		why = "the layer's size is not the frame's at fxrScale";
		return nullptr;
	}
	if ( !WwVkPresent::fxresTargetsAre( owner, O.t ) ) {
		why = "the layer's targets are no longer shared";
		return nullptr;
	}
	return &O;
}

// the worker (fxWork, F.kind 16): phase 1 the downsample, phase 3 the composite
bool fxresRecord( std::string & why )
{
	Vk & v = vk();
	Fx & F = fx();
	FxresVk & D = fxresVk();
	const bool comp = D.phase == 3;
	const VkFramebuffer fb = comp ? D.compFb : D.downFb;
	if ( ( D.phase != 1 && !comp ) || !fb || !D.pipe ) {
		why = "no framebuffer / pipeline for the soft-effect layer";
		return false;
	}
	VkRenderPassBeginInfo rbi { VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
	rbi.renderPass = comp ? D.compPass : D.downPass;
	rbi.framebuffer = fb;
	rbi.renderArea = { { 0, 0 }, { std::uint32_t( D.w ), std::uint32_t( D.h ) } };
	v.vkCmdBeginRenderPass( F.cmd, &rbi, VK_SUBPASS_CONTENTS_INLINE );
	const VkViewport vpt { float( D.vp[0] ), float( D.vp[1] ), float( D.vp[2] ), float( D.vp[3] ), 0.0f, 1.0f };
	const int x0 = std::clamp( D.vp[0], 0, D.w ), y0 = std::clamp( D.vp[1], 0, D.h );
	const int x1 = std::clamp( D.vp[0] + D.vp[2], 0, D.w ), y1 = std::clamp( D.vp[1] + D.vp[3], 0, D.h );
	if ( x1 > x0 && y1 > y0 ) {
		const VkRect2D sc { { x0, y0 }, { std::uint32_t( x1 - x0 ), std::uint32_t( y1 - y0 ) } };
		v.vkCmdSetViewport( F.cmd, 0, 1, &vpt );
		v.vkCmdSetScissor( F.cmd, 0, 1, &sc );
		v.vkCmdBindPipeline( F.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, D.pipe );
		v.vkCmdBindDescriptorSets( F.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, D.layout[comp ? 1 : 0], 0, 1, &D.set[comp ? 1 : 0], 0, nullptr );
		v.vkCmdDraw( F.cmd, 3, 1, 0, 0 );
	}
	v.vkCmdEndRenderPass( F.cmd );
	return true;
}
}  // namespace

namespace WwVkPresent
{
std::string fxresRefusal()
{
	FxresVk & D = fxresVk();
	Vk & v = vk();
	Gl & g = gl();
	if ( D.broken )
		return D.brokenWhy;
	if ( !v.ok || !g.ok )
		return "the present device or GL's entry points are not ready";
	if ( !v.dualSrcBlend )
		return "the device has no dualSrcBlend (the composite's SRC1 blend)";
	if ( !g.copyImageSubData || !g.getError )
		return "no glCopyImageSubData (fxrSrc's twin)";
	return std::string();
}

bool fxresTargets( const void * owner, int w, int h, FxresTargets & out, std::string & why )
{
	FxresVk & D = fxresVk();
	out = FxresTargets();
	if ( !owner || w < 1 || h < 1 ) {
		why = "no owner or an empty layer";
		return false;
	}
	if ( std::string r = fxresRefusal(); !r.empty() ) {
		why = r;
		return false;
	}
	FxresOwner & O = D.owners[owner];
	const std::uintptr_t base = fxrBase( O );
	unsigned * dst[5] = { &out.z, &out.depth, &out.c, &out.t, &out.cov };
	for ( int i = 0; i < 5; i++ ) {
		std::string w2;
		if ( !sharedTarget( base + std::uintptr_t( i ), 0, w, h, 1, kFxrFmt[i], *dst[i], w2 ) || !*dst[i] ) {
			fxresTargetsDrop( owner );
			out = FxresTargets();
			why = "the layer's shared target " + std::to_string( i ) + ": " + w2;
			return false;
		}
	}
	O.t = out;
	O.w = w;
	O.h = h;
	return true;
}

bool fxresTargetsAre( const void * owner, const FxresTargets & t )
{
	FxresVk & D = fxresVk();
	auto it = D.owners.find( owner );
	if ( !owner || it == D.owners.end() )
		return false;
	const FxresTargets & o = it->second.t;
	const unsigned mine[5] = { o.z, o.depth, o.c, o.t, o.cov }, theirs[5] = { t.z, t.depth, t.c, t.t, t.cov };
	const std::uintptr_t base = fxrBase( it->second );
	for ( int i = 0; i < 5; i++ )
		if ( mine[i] != theirs[i] || !sharedTargetIs( base + std::uintptr_t( i ), theirs[i] ) )
			return false;
	return true;
}

void fxresTargetsDrop( const void * owner )
{
	FxresVk & D = fxresVk();
	auto it = D.owners.find( owner );
	if ( it == D.owners.end() )
		return;
	const std::uintptr_t base = fxrBase( it->second );
	for ( int i = 0; i < 6; i++ )
		if ( sharedFind( base + std::uintptr_t( i ) ) )
			sharedTargetDrop( base + std::uintptr_t( i ) );
	D.copyTwin = 0;
	D.owners.erase( it );
}

bool fxresDownRun( const FxresDown & r, std::string & why )
{
	Fx & F = fx();
	FxresVk & D = fxresVk();
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
		D.brokenWhy = "the soft-effect layer on Vulkan broke: " + w;
		return refuse( D.brokenWhy );
	};
	if ( std::string rf = fxresRefusal(); !rf.empty() )
		return refuse( rf );
	if ( g.ctx != reinterpret_cast<void *>( wglGetCurrentContext() ) )
		return refuse( "GL's context is not the present device's" );
	std::string w;
	FxresOwner * O = fxrOwner( r.owner, r.W, r.H, r.scale, w );
	if ( !O )
		return refuse( w );
	// the last crossing (any fx pass) is done before its command buffer, sets, framebuffers and buffer are written again
	if ( std::string fw; !fenceDone( F.fence, F.pending, "fxres", fw ) )
		return refuse( fw );
	F.pending = false;
	if ( !fxStatic( w ) || !fxSems( w ) || !fxrStatic( w ) )
		return breakOff( w );
	VkPipeline pipe = D.downPipe;
	if ( r.red & 4 ) {
		if ( !fxrDownRed( w ) )
			return refuse( w );
		pipe = D.downRedPipe;
	}
	const std::uintptr_t base = fxrBase( *O );
	const Shared * zs = sharedFind( base + 0 );
	const Shared * ds = sharedFind( base + 1 );
	if ( !zs || !zs->view || !ds || !ds->view )
		return refuse( "the layer's z / depth have no views" );
	VkImageView sv = VK_NULL_HANDLE;
	unsigned sname = 0;
	if ( !fxrSource( *O, r.W, r.H, r.src, r.srcFmt, r.srcKey, sv, sname, w ) )
		return refuse( w );
	const VkImageView at[2] = { zs->view, ds->view };
	if ( !fxFb( D.downFb, D.downPass, at, 2, O->w, O->h ) )
		return breakOff( "the layer's downsample framebuffer failed" );
	const std::int32_t u[3] = { r.scale, r.W, r.H };  // fxrScale @0, fxrSrcSize @4
	std::memcpy( D.uboPtr + kFxrDownUbo, u, sizeof( u ) );
	VkDescriptorBufferInfo bi { D.ubo, kFxrDownUbo, kFxrDownBytes };
	VkDescriptorImageInfo ii {};
	ii.imageView = sv;
	ii.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	VkWriteDescriptorSet ws[kFxrDownBindings] = { { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET }, { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET } };
	ws[0].dstSet = ws[1].dstSet = D.set[0];
	ws[0].dstBinding = 31;
	ws[0].descriptorCount = 1;
	ws[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	ws[0].pBufferInfo = &bi;
	ws[1].dstBinding = 64;
	ws[1].descriptorCount = 1;
	ws[1].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
	ws[1].pImageInfo = &ii;
	v.vkUpdateDescriptorSets( v.dev, std::uint32_t( kFxrDownBindings ), ws, 0, nullptr );
	D.phase = 1;
	D.w = O->w;
	D.h = O->h;
	const int vp[4] = { 0, 0, O->w, O->h };
	std::memcpy( D.vp, vp, sizeof( D.vp ) );
	D.pipe = pipe;
	F.kind = 16;
	F.w = O->w;
	F.h = O->h;
	std::memcpy( F.vp, vp, sizeof( F.vp ) );
	FxNames names;
	names.add( sname );
	names.add( O->t.z );
	names.add( O->t.depth );
	if ( !fxCross( names, w ) )
		return breakOff( w );  // nothing signals vkDone: GL does not wait it; GL draws its own pass this frame
	const double ms = nowMs() - t0;
	S.runs++;
	S.cpuMs += ms;
	S.maxMs = std::max( S.maxMs, ms );
	return true;
}

bool fxresCompRun( const FxresComp & r, std::string & why )
{
	Fx & F = fx();
	FxresVk & D = fxresVk();
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
		D.brokenWhy = "the soft-effect layer on Vulkan broke: " + w;
		return refuse( D.brokenWhy );
	};
	if ( std::string rf = fxresRefusal(); !rf.empty() )
		return refuse( rf );
	if ( g.ctx != reinterpret_cast<void *>( wglGetCurrentContext() ) )
		return refuse( "GL's context is not the present device's" );
	if ( !toneFrameIs( r.color ) )
		return refuse( "the composite's draw target is not the shared tone frame" );
	if ( !T.colorView || !T.dsView || !T.dsTex || T.w != r.W || T.h != r.H )
		return refuse( "the tone frame's colour / stencil or size is not the composite's" );
	std::string w;
	FxresOwner * O = fxrOwner( r.owner, r.W, r.H, r.scale, w );
	if ( !O )
		return refuse( w );
	if ( std::string fw; !fenceDone( F.fence, F.pending, "fxres", fw ) )
		return refuse( fw );
	F.pending = false;
	if ( !fxStatic( w ) || !fxSems( w ) || !fxrStatic( w ) || !fxrCompPass( std::max( T.samples, 1 ), w ) )
		return breakOff( w );
	const VkPipeline pipe = fxrCompPipe( r.red & 3 );
	if ( !pipe )
		return breakOff( "vkCreateGraphicsPipelines (the layer's composite) failed" );
	const std::uintptr_t base = fxrBase( *O );
	const Shared * in[4] = { sharedFind( base + 2 ), sharedFind( base + 3 ), sharedFind( base + 4 ), sharedFind( base + 0 ) };  // c t cov z
	for ( const Shared * s : in )
		if ( !s || !s->view )
			return refuse( "the layer's c / t / cov / z have no views" );
	VkImageView sv = VK_NULL_HANDLE;
	unsigned sname = 0;
	if ( !fxrSource( *O, r.W, r.H, r.src, r.srcFmt, r.srcKey, sv, sname, w ) )
		return refuse( w );
	const VkImageView at[2] = { T.colorView, T.dsView };
	if ( !fxFb( D.compFb, D.compPass, at, 2, T.w, T.h ) )
		return breakOff( "the layer's composite framebuffer over the tone frame failed" );
	// fxrScale @0, fxrSize (the layer) @4, fxrProj @16, fxrEdge @24, fxrMode @28
	std::uint8_t * u = D.uboPtr + kFxrCompUbo;
	const std::int32_t iv[3] = { r.scale, O->w, O->h };
	const float fv[3] = { r.proj[0], r.proj[1], r.edge };
	const std::int32_t mode = r.mode;
	std::memcpy( u, iv, sizeof( iv ) );
	std::memcpy( u + 16, fv, sizeof( fv ) );
	std::memcpy( u + 28, &mode, 4 );
	VkDescriptorBufferInfo bi { D.ubo, kFxrCompUbo, kFxrCompBytes };
	VkDescriptorImageInfo ii[5] = {};
	for ( int i = 0; i < 5; i++ ) {
		ii[i].imageView = i < 4 ? in[i]->view : sv;
		ii[i].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	}
	VkWriteDescriptorSet ws[kFxrCompBindings] = {};
	for ( int i = 0; i < kFxrCompBindings; i++ ) {
		ws[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		ws[i].dstSet = D.set[1];
		ws[i].dstBinding = i ? std::uint32_t( 63 + i ) : 31u;
		ws[i].descriptorCount = 1;
		ws[i].descriptorType = i ? VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE : VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		if ( i )
			ws[i].pImageInfo = &ii[i - 1];
		else
			ws[i].pBufferInfo = &bi;
	}
	v.vkUpdateDescriptorSets( v.dev, std::uint32_t( kFxrCompBindings ), ws, 0, nullptr );
	D.phase = 3;
	D.w = T.w;
	D.h = T.h;
	std::memcpy( D.vp, r.vp, sizeof( D.vp ) );
	D.pipe = pipe;
	F.kind = 16;
	F.w = T.w;
	F.h = T.h;
	std::memcpy( F.vp, r.vp, sizeof( F.vp ) );
	FxNames names;
	names.add( T.colorTex );
	names.add( T.dsTex );
	names.add( O->t.c );
	names.add( O->t.t );
	names.add( O->t.cov );
	names.add( O->t.z );
	names.add( sname );
	if ( !fxCross( names, w ) )
		return breakOff( w );
	const double ms = nowMs() - t0;
	S.runs++;
	S.cpuMs += ms;
	S.maxMs = std::max( S.maxMs, ms );
	return true;
}

void fxresOnGl( const char * why )
{
	FxStat & S = fxresVk().st;
	S.gl++;
	S.glWhy = why ? why : "";
}

FxStat takeFxresStat()
{
	FxresVk & D = fxresVk();
	FxStat s = D.st;
	s.pipelines = D.pipelines;
	s.buildMs = D.buildMs;
	if ( D.broken && s.failWhy.empty() )
		s.failWhy = D.brokenWhy;
	D.st = FxStat();
	return s;
}

int fxresRedPatches()
{
	return fxresVk().redPatches;
}
}  // namespace WwVkPresent
