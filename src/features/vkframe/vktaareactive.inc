/* Lane VKFAR7 (2026-11-15): the TAA mask's reactive pass (taainputs.cpp pass 4) on the present device, one crossing a batch
 * (fxWork's kind 11, its own runner beside vkfogbloom.inc: taaRun can refuse after pass 4 ran, twinRunN's pool holds 32 sets,
 * waterRun takes u16 indices into the tone frame, twinRun refuses blending). Included once by vkpresent.cpp after
 * vkfarground.inc (waterSampler, waterTwin, farTwinFind, csmBuf, fenceDone and the fx parts of the anonymous namespace).
 * GL: drawBuffers { NONE, ATT1 }, colour mask R G on ATT1, blend MAX ONE / ONE, depth LEQUAL no writes, polygon offset
 * (-1, -4), no cull / stencil / scissor, viewport 0 0 W H. Here: the mask (RGBA8) and the depth copied into shared twins, one
 * render pass (subpass colour 0 unused, 1 the mask; the depth LOAD), one indexed draw a queued draw with its own set and
 * host slice (VS b31, PS b31, positions, colours, uvs, u32 indices), the mask copied back after the last crossing. */
#include "features/vkframe/vktaareactive.h"
#include "features/vkframe/taareactive_view_vs.spv.h"  // vkframe_spirv.sh's fxb / fx lines
#include "features/vkframe/taareactive_view_ps.spv.h"

namespace
{
constexpr int kTaBindings = 4;          // VS b31 at 32, PS b31 at 31, BaseMap at 64, its sampler at 128
constexpr VkDeviceSize kTaVsBytes = 64, kTaPsBytes = 48, kTaAlign = 256;
constexpr int kTaTexMax = 200;          // distinct base maps a crossing (FxNames holds 256 names)
constexpr int kTaKeyN = 4 + kTaTexMax;  // twin keys: 0 the mask, 1 the depth, 4.. the base maps

struct TaRec
{
	VkDescriptorSet set = VK_NULL_HANDLE;
	VkDeviceSize pos = 0, col = 0, uv = 0, idx = 0;
	std::uint32_t count = 0;
};

struct TaReactive
{
	bool made = false, broken = false;
	std::string brokenWhy;
	VkDescriptorSetLayout dsl = VK_NULL_HANDLE;
	VkDescriptorPool pool = VK_NULL_HANDLE;
	std::vector<VkDescriptorSet> sets;
	VkPipelineLayout layout = VK_NULL_HANDLE;
	VkShaderModule vs = VK_NULL_HANDLE, ps = VK_NULL_HANDLE;
	VkRenderPass pass = VK_NULL_HANDLE;
	VkPipeline pipe = VK_NULL_HANDLE;
	VkFormat dsFmt = VK_FORMAT_UNDEFINED;
	PFN_vkCmdDrawIndexed drawIndexed = nullptr;
	CsmBuf host;
	// the depth twin's depth | stencil view and the framebuffer, made for these images at this sharedGen
	VkImage dsImg = VK_NULL_HANDLE, maskImg = VK_NULL_HANDLE;
	VkImageView dsView = VK_NULL_HANDLE;
	VkFramebuffer fb = VK_NULL_HANDLE;
	std::uint64_t fbGen = 0;
	int fbW = 0, fbH = 0;
	unsigned depthTwin = 0;
	std::vector<WwVkPresent::TaReactiveDraw> queue;
	// this crossing's record (written before the hand)
	std::vector<TaRec> recs;
	int w = 0, h = 0;
	WwVkPresent::TaReactiveStat st;
	int pipelines = 0;
	double buildMs = 0.0;
};

TaReactive & taD()
{
	static TaReactive d;
	return d;
}

std::uintptr_t taKey( int i )
{
	static char keys[kTaKeyN];
	return reinterpret_cast<std::uintptr_t>( &keys[std::clamp( i, 0, kTaKeyN - 1 )] );
}

bool taStatic( std::string & why )
{
	Vk & v = vk();
	TaReactive & D = taD();
	if ( D.made )
		return true;
	const double t0 = nowMs();
	VkDescriptorSetLayoutBinding b[kTaBindings] = {};
	static const std::uint32_t kBind[kTaBindings] = { 32, 31, 64, 128 };
	static const VkDescriptorType kType[kTaBindings] = { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
		VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, VK_DESCRIPTOR_TYPE_SAMPLER };
	for ( int k = 0; k < kTaBindings; k++ ) {
		b[k].binding = kBind[k];
		b[k].descriptorType = kType[k];
		b[k].descriptorCount = 1;
		b[k].stageFlags = k == 0 ? VK_SHADER_STAGE_VERTEX_BIT : VK_SHADER_STAGE_FRAGMENT_BIT;
	}
	VkDescriptorSetLayoutCreateInfo lci { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
	lci.bindingCount = kTaBindings;
	lci.pBindings = b;
	VkPipelineLayoutCreateInfo plci { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
	if ( v.vkCreateDescriptorSetLayout( v.dev, &lci, nullptr, &D.dsl ) != VK_SUCCESS ) {
		why = "the reactive pass's descriptor set layout failed";
		return false;
	}
	plci.setLayoutCount = 1;
	plci.pSetLayouts = &D.dsl;
	if ( v.vkCreatePipelineLayout( v.dev, &plci, nullptr, &D.layout ) != VK_SUCCESS ) {
		why = "the reactive pass's pipeline layout failed";
		return false;
	}
	if ( !fxModule( vkframe_taareactive_view_vs_spv, sizeof( vkframe_taareactive_view_vs_spv ), D.vs )
		|| !fxModule( vkframe_taareactive_view_ps_spv, sizeof( vkframe_taareactive_view_ps_spv ), D.ps ) ) {
		why = "the reactive pass's shader modules failed";
		return false;
	}
	D.drawIndexed = reinterpret_cast<PFN_vkCmdDrawIndexed>( v.vkGetDeviceProcAddr( v.dev, "vkCmdDrawIndexed" ) );
	if ( !D.drawIndexed ) {
		why = "no vkCmdDrawIndexed";
		return false;
	}
	D.buildMs += nowMs() - t0;
	D.made = true;
	return true;
}

// n sets of the layout (the pool remade bigger; the caller waited the last crossing's fence)
bool taSets( int n, std::string & why )
{
	Vk & v = vk();
	TaReactive & D = taD();
	if ( int( D.sets.size() ) >= n )
		return true;
	if ( D.pool )
		v.vkDestroyDescriptorPool( v.dev, D.pool, nullptr );
	D.pool = VK_NULL_HANDLE;
	D.sets.clear();
	int cap = 64;
	while ( cap < n )
		cap *= 2;
	const VkDescriptorPoolSize ps[3] = { { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, std::uint32_t( 2 * cap ) },
		{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, std::uint32_t( cap ) }, { VK_DESCRIPTOR_TYPE_SAMPLER, std::uint32_t( cap ) } };
	VkDescriptorPoolCreateInfo pci { VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
	pci.maxSets = std::uint32_t( cap );
	pci.poolSizeCount = 3;
	pci.pPoolSizes = ps;
	if ( v.vkCreateDescriptorPool( v.dev, &pci, nullptr, &D.pool ) != VK_SUCCESS ) {
		why = "the reactive pass's descriptor pool (" + std::to_string( cap ) + " sets) failed";
		return false;
	}
	std::vector<VkDescriptorSetLayout> lay( size_t( cap ), D.dsl );
	D.sets.assign( size_t( cap ), VK_NULL_HANDLE );
	VkDescriptorSetAllocateInfo dai { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
	dai.descriptorPool = D.pool;
	dai.descriptorSetCount = std::uint32_t( cap );
	dai.pSetLayouts = lay.data();
	if ( v.vkAllocateDescriptorSets( v.dev, &dai, D.sets.data() ) != VK_SUCCESS ) {
		D.sets.clear();
		why = "the reactive pass's descriptor sets failed";
		return false;
	}
	return true;
}

// the render pass (RGBA8 mask LOAD / STORE behind an unused colour 0, the depth LOAD) and its pipeline, per depth format
bool taPipe( VkFormat dsFmt, std::string & why )
{
	Vk & v = vk();
	TaReactive & D = taD();
	if ( D.pipe && D.dsFmt == dsFmt )
		return true;
	const double t0 = nowMs();
	if ( D.pipe )
		v.vkDestroyPipeline( v.dev, D.pipe, nullptr );
	if ( D.fb )
		v.vkDestroyFramebuffer( v.dev, D.fb, nullptr );
	if ( D.pass )
		v.vkDestroyRenderPass( v.dev, D.pass, nullptr );
	D.pipe = VK_NULL_HANDLE;
	D.fb = VK_NULL_HANDLE;
	D.pass = VK_NULL_HANDLE;
	D.dsFmt = VK_FORMAT_UNDEFINED;
	const bool stencil = dsFmt == VK_FORMAT_D24_UNORM_S8_UINT || dsFmt == VK_FORMAT_D32_SFLOAT_S8_UINT;
	VkAttachmentDescription at[2] = {};
	at[0].format = VK_FORMAT_R8G8B8A8_UNORM;
	at[0].samples = VK_SAMPLE_COUNT_1_BIT;
	at[0].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
	at[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	at[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	at[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	at[0].initialLayout = at[0].finalLayout = VK_IMAGE_LAYOUT_GENERAL;
	at[1] = at[0];
	at[1].format = dsFmt;
	at[1].stencilLoadOp = stencil ? VK_ATTACHMENT_LOAD_OP_LOAD : VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	at[1].stencilStoreOp = stencil ? VK_ATTACHMENT_STORE_OP_STORE : VK_ATTACHMENT_STORE_OP_DONT_CARE;
	// SV_Target1: location 1 is the mask, location 0 nothing (GL's GL_NONE draw buffer)
	const VkAttachmentReference cr[2] = { { VK_ATTACHMENT_UNUSED, VK_IMAGE_LAYOUT_GENERAL }, { 0, VK_IMAGE_LAYOUT_GENERAL } };
	const VkAttachmentReference dr { 1, VK_IMAGE_LAYOUT_GENERAL };
	VkSubpassDescription sp {};
	sp.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	sp.colorAttachmentCount = 2;
	sp.pColorAttachments = cr;
	sp.pDepthStencilAttachment = &dr;
	VkRenderPassCreateInfo rci { VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO };
	rci.attachmentCount = 2;
	rci.pAttachments = at;
	rci.subpassCount = 1;
	rci.pSubpasses = &sp;
	if ( v.vkCreateRenderPass( v.dev, &rci, nullptr, &D.pass ) != VK_SUCCESS ) {
		why = "vkCreateRenderPass (the reactive pass) failed";
		return false;
	}
	VkPipelineShaderStageCreateInfo st[2] = { { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO },
		{ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO } };
	st[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
	st[0].module = D.vs;
	st[0].pName = "main";
	st[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	st[1].module = D.ps;
	st[1].pName = "main";
	// GL's locations 0 / 1 / 7 are the twin's 0 / 1 / 2 (TEXCOORD0 / 1 / 7 in declaration order)
	const VkVertexInputBindingDescription vb[3] = { { 0, 12, VK_VERTEX_INPUT_RATE_VERTEX }, { 1, 16, VK_VERTEX_INPUT_RATE_VERTEX },
		{ 2, 8, VK_VERTEX_INPUT_RATE_VERTEX } };
	const VkVertexInputAttributeDescription va[3] = { { 0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0 }, { 1, 1, VK_FORMAT_R32G32B32A32_SFLOAT, 0 },
		{ 2, 2, VK_FORMAT_R32G32_SFLOAT, 0 } };
	VkPipelineVertexInputStateCreateInfo vi { VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
	vi.vertexBindingDescriptionCount = 3;
	vi.pVertexBindingDescriptions = vb;
	vi.vertexAttributeDescriptionCount = 3;
	vi.pVertexAttributeDescriptions = va;
	VkPipelineInputAssemblyStateCreateInfo ia { VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
	ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	VkPipelineViewportDepthClipControlCreateInfoEXT cc { VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_DEPTH_CLIP_CONTROL_CREATE_INFO_EXT };
	cc.negativeOneToOne = VK_TRUE;  // GL's clip z in [-w, w]
	VkPipelineViewportStateCreateInfo vps { VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO };
	vps.pNext = &cc;
	vps.viewportCount = vps.scissorCount = 1;
	VkPipelineRasterizationStateCreateInfo rs { VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
	rs.polygonMode = VK_POLYGON_MODE_FILL;
	rs.cullMode = VK_CULL_MODE_NONE;  // no y flip: GL's rows from the bottom are the image's from the top; no cull either way
	rs.frontFace = VK_FRONT_FACE_CLOCKWISE;
	rs.lineWidth = 1.0f;
	rs.depthBiasEnable = VK_TRUE;  // glPolygonOffset( -1, -4 ): factor = slope, units = the constant
	rs.depthBiasSlopeFactor = -1.0f;
	rs.depthBiasConstantFactor = -4.0f;
	VkPipelineMultisampleStateCreateInfo ms { VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
	ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	VkPipelineDepthStencilStateCreateInfo ds { VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };
	ds.depthTestEnable = VK_TRUE;
	ds.depthWriteEnable = VK_FALSE;
	ds.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
	VkPipelineColorBlendAttachmentState cb[2] = {};
	cb[1].blendEnable = VK_TRUE;
	cb[1].srcColorBlendFactor = cb[1].dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
	cb[1].srcAlphaBlendFactor = cb[1].dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	cb[1].colorBlendOp = cb[1].alphaBlendOp = VK_BLEND_OP_MAX;
	cb[1].colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT;
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
	gp.pDepthStencilState = &ds;
	gp.pColorBlendState = &cbs;
	gp.pDynamicState = &dy;
	gp.layout = D.layout;
	gp.renderPass = D.pass;
	if ( v.vkCreateGraphicsPipelines( v.dev, tone().cache, 1, &gp, nullptr, &D.pipe ) != VK_SUCCESS ) {
		why = "vkCreateGraphicsPipelines (the reactive pass) failed";
		return false;
	}
	D.pipelines++;
	D.dsFmt = dsFmt;
	D.buildMs += nowMs() - t0;
	toneCacheSave();
	return true;
}

// the framebuffer over the mask twin and a depth | stencil view of the depth twin, remade when an image or sharedGen moved
bool taFb( const Shared & mask, const Shared & depth, std::string & why )
{
	Vk & v = vk();
	TaReactive & D = taD();
	if ( D.fb && D.maskImg == mask.img && D.dsImg == depth.img && D.fbGen == sharedGen() && D.fbW == mask.w && D.fbH == mask.h )
		return true;
	if ( D.fb )
		v.vkDestroyFramebuffer( v.dev, D.fb, nullptr );
	if ( D.dsView )
		v.vkDestroyImageView( v.dev, D.dsView, nullptr );
	D.fb = VK_NULL_HANDLE;
	D.dsView = VK_NULL_HANDLE;
	D.maskImg = D.dsImg = VK_NULL_HANDLE;
	const bool stencil = depth.fmt == VK_FORMAT_D24_UNORM_S8_UINT || depth.fmt == VK_FORMAT_D32_SFLOAT_S8_UINT;
	D.dsView = imageView( depth.img, VK_IMAGE_VIEW_TYPE_2D, depth.fmt,
		VK_IMAGE_ASPECT_DEPTH_BIT | ( stencil ? VK_IMAGE_ASPECT_STENCIL_BIT : 0 ) );
	const VkImageView views[2] = { mask.view, D.dsView };
	if ( !D.dsView || !fxFb( D.fb, D.pass, views, 2, mask.w, mask.h ) ) {
		why = "the reactive pass's framebuffer over its twins failed";
		return false;
	}
	D.maskImg = mask.img;
	D.dsImg = depth.img;
	D.fbGen = sharedGen();
	D.fbW = mask.w;
	D.fbH = mask.h;
	return true;
}

// fxWork's kind 11 (the worker): the queued draws of this crossing into the mask twin
bool taRecord( std::string & why )
{
	Vk & v = vk();
	Fx & F = fx();
	TaReactive & D = taD();
	if ( !D.fb || !D.pipe || !D.drawIndexed || !D.host.buf ) {
		why = "no framebuffer / pipeline / buffer for the reactive pass";
		return false;
	}
	VkRenderPassBeginInfo rbi { VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
	rbi.renderPass = D.pass;
	rbi.framebuffer = D.fb;
	rbi.renderArea = { { 0, 0 }, { std::uint32_t( D.w ), std::uint32_t( D.h ) } };
	v.vkCmdBeginRenderPass( F.cmd, &rbi, VK_SUBPASS_CONTENTS_INLINE );
	const VkViewport vpt { 0.0f, 0.0f, float( D.w ), float( D.h ), 0.0f, 1.0f };
	const VkRect2D sc { { 0, 0 }, { std::uint32_t( D.w ), std::uint32_t( D.h ) } };
	v.vkCmdSetViewport( F.cmd, 0, 1, &vpt );
	v.vkCmdSetScissor( F.cmd, 0, 1, &sc );
	v.vkCmdBindPipeline( F.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, D.pipe );
	for ( const TaRec & r : D.recs ) {
		v.vkCmdBindDescriptorSets( F.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, D.layout, 0, 1, &r.set, 0, nullptr );
		const VkBuffer bufs[3] = { D.host.buf, D.host.buf, D.host.buf };
		const VkDeviceSize offs[3] = { r.pos, r.col, r.uv };
		v.vkCmdBindVertexBuffers( F.cmd, 0, 3, bufs, offs );
		v.vkCmdBindIndexBuffer( F.cmd, D.host.buf, r.idx, VK_INDEX_TYPE_UINT32 );
		D.drawIndexed( F.cmd, r.count, 1, 0, 0, 0 );
	}
	v.vkCmdEndRenderPass( F.cmd );
	return true;
}
}  // namespace

namespace WwVkPresent
{
bool taReactiveAdd( TaReactiveDraw && d, std::string & why )
{
	const size_t n = d.pos.size() / 3;
	if ( !n || d.pos.size() != n * 3 || d.idx.empty() || d.idx.size() % 3 )
		return why = "an empty draw or a partial triangle", false;
	if ( ( !d.col.empty() && d.col.size() != n * 4 ) || ( !d.uv.empty() && d.uv.size() != n * 2 ) )
		return why = "a colour / uv array shorter than the positions", false;
	for ( std::uint32_t i : d.idx )
		if ( i >= n )
			return why = "an index past the vertices", false;
	if ( d.useTex && d.base.tex ) {
		if ( d.base.kind != 0 )
			return why = "BaseMap is not a 2D texture", false;
		if ( d.base.baseLevel != 0 )
			return why = "BaseMap: a base level above 0", false;
		const int b = waterSlotBlock( d.base, waterCaps() );
		if ( b != kWaterOk )
			return why = std::string( "BaseMap: " ) + waterBlockName( b ) + " (" + waterFmtName( d.base.glFmt ) + ")", false;
	}
	taD().queue.push_back( std::move( d ) );
	return true;
}

int taReactiveQueued()
{
	return int( taD().queue.size() );
}

void taReactiveDrop()
{
	taD().queue.clear();
}

unsigned taReactiveDepthTwin()
{
	return taD().depthTwin;
}

bool taReactiveFlush( const TaReactiveRun & r, std::string & why )
{
	Fx & F = fx();
	TaReactive & D = taD();
	Vk & v = vk();
	Gl & g = gl();
	TaReactiveStat & S = D.st;
	const double t0 = nowMs();
	std::vector<TaReactiveDraw> q;
	q.swap( D.queue );
	S.flushes++;
	auto refuse = [&]( const std::string & w ) {
		why = w;
		S.fails++;
		S.failWhy = w;
		return false;
	};
	auto breakOff = [&]( const std::string & w ) {
		D.broken = true;
		D.brokenWhy = "the reactive pass on Vulkan broke: " + w;
		return refuse( D.brokenWhy );
	};
	if ( D.broken )
		return refuse( D.brokenWhy );
	if ( q.empty() )
		return true;
	if ( !v.ok || !g.ok || g.ctx != reinterpret_cast<void *>( wglGetCurrentContext() ) )
		return refuse( "the present device or GL's entry points are not ready in this context" );
	if ( !g.copyImageSubData )
		return refuse( "no glCopyImageSubData (the mask, depth and base maps into their twins)" );
	if ( !v.clipControl )
		return refuse( "no VK_EXT_depth_clip_control (GL's clip z in [-w, w])" );
	if ( !r.mask || !r.depth || r.w < 1 || r.h < 1 )
		return refuse( "no mask / depth target" );
	bool isDepth = false;
	const VkFormat dsFmt = vkFormatOf( GLenum( r.dsFormat ), isDepth );
	if ( !isDepth || dsFmt == VK_FORMAT_D16_UNORM )
		return refuse( "the frame's depth format " + std::to_string( r.dsFormat ) + " has no twin here" );
	std::string w;
	// fence waits go through fenceDone -> fenceRed / fenceMiss (red WW_VKFENCE_RED=taareactive)
	if ( !fenceDone( F.fence, F.pending, "taareactive", w ) )
		return refuse( w );
	F.pending = false;
	if ( !fxStatic( w ) || !fxSems( w ) || !taStatic( w ) || !taPipe( dsFmt, w ) )
		return breakOff( w );
	unsigned maskTwin = 0, depthTwin = 0;
	if ( !sharedTarget( taKey( 0 ), 0, r.w, r.h, 1, kRgba8, maskTwin, w, 1, 1 )
		|| !sharedTarget( taKey( 1 ), 0, r.w, r.h, 1, r.dsFormat, depthTwin, w, 1, 1 ) )
		return refuse( "the reactive pass's twins: " + w );
	const Shared * mt = sharedFind( taKey( 0 ) );
	const Shared * dt = sharedFind( taKey( 1 ) );
	if ( !mt || !mt->view || !dt || !dt->img )
		return refuse( "the reactive pass's twins have no view" );
	if ( !taFb( *mt, *dt, w ) )
		return breakOff( w );
	D.depthTwin = depthTwin;
	g.copyImageSubData( r.mask, kTexture2D, 0, 0, 0, 0, maskTwin, kTexture2D, 0, 0, 0, 0, r.w, r.h, 1 );
	g.copyImageSubData( r.depth, kTexture2D, 0, 0, 0, 0, depthTwin, kTexture2D, 0, 0, 0, 0, r.w, r.h, 1 );
	WaterDraw & smpD = waterD();  // the water draw's VkSampler per GL sampling state
	const int n = int( q.size() );
	int i0 = 0, crossings = 0;
	while ( i0 < n ) {
		if ( crossings ) {
			if ( !fenceDone( F.fence, F.pending, "taareactive", w ) )
				return refuse( w );
			F.pending = false;
		}
		// this crossing's draws: up to maxPer, and no more distinct base maps than the names hold
		std::vector<unsigned> texs;
		int i1 = i0;
		while ( i1 < n && ( r.maxPer < 1 || i1 - i0 < r.maxPer ) ) {
			const TaReactiveDraw & d = q[size_t( i1 )];
			if ( d.useTex && d.base.tex && std::find( texs.begin(), texs.end(), d.base.tex ) == texs.end() ) {
				if ( int( texs.size() ) >= kTaTexMax )
					break;
				texs.push_back( d.base.tex );
			}
			i1++;
		}
		const int m = i1 - i0;
		// the host slices: VS block, PS block (each 256-aligned), positions, colours, uvs, indices
		std::vector<VkDeviceSize> at( size_t( m ) + 1, 0 );
		VkDeviceSize need = 0;
		for ( int k = 0; k < m; k++ ) {
			const TaReactiveDraw & d = q[size_t( i0 + k )];
			const VkDeviceSize nv = VkDeviceSize( d.pos.size() / 3 );
			at[size_t( k )] = need;
			need += 2 * kTaAlign + ( ( nv * ( 12 + 16 + 8 ) + VkDeviceSize( d.idx.size() ) * 4 + 63 ) / 64 ) * 64 + 64;
			need = ( need + kTaAlign - 1 ) / kTaAlign * kTaAlign;
		}
		at[size_t( m )] = need;
		if ( !csmBuf( D.host, need, false, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT
				| VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, w ) || !taSets( m, w ) )
			return breakOff( w );
		FxNames names;
		names.add( maskTwin );
		names.add( depthTwin );
		// the base maps: read in place when already shared, else copied (every level) into this crossing's twin j
		std::vector<VkImageView> texView( texs.size(), VK_NULL_HANDLE );
		for ( size_t j = 0; j < texs.size(); j++ ) {
			const TaReactiveDraw * src = nullptr;
			for ( int k = i0; k < i1 && !src; k++ )
				if ( q[size_t( k )].useTex && q[size_t( k )].base.tex == texs[j] )
					src = &q[size_t( k )];
			const WaterSlot & s = src->base;
			if ( const std::uintptr_t sk = farSharedOf( s.tex ) ) {
				texView[j] = sharedFind( sk )->view;
				names.add( s.tex );
				continue;
			}
			const std::uintptr_t key = taKey( 4 + int( j ) );
			unsigned twin = 0;
			if ( !waterTwin( key, s, twin, w ) )
				return refuse( "BaseMap: " + w );
			const Shared * sh = waterBcFormat( s.glFmt ) ? farTwinFind( key ) : sharedFind( key );
			if ( !sh || !sh->view )
				return refuse( "BaseMap: its twin has no view" );
			texView[j] = sh->view;
			for ( int l = 0; l < std::max( s.levels, 1 ); l++ )
				g.copyImageSubData( s.tex, kTexture2D, l, 0, 0, 0, twin, kTexture2D, l, 0, 0, 0, std::max( s.w >> l, 1 ),
					std::max( s.h >> l, 1 ), 1 );
			S.copies++;
			names.add( twin );
		}
		if ( names.full )
			return refuse( "more GL names than a crossing hands over" );
		D.recs.assign( size_t( m ), TaRec() );
		std::vector<VkDescriptorBufferInfo> bi( size_t( 2 * m ) );
		std::vector<VkDescriptorImageInfo> ii( size_t( 2 * m ) );
		std::vector<VkWriteDescriptorSet> ws( size_t( kTaBindings * m ) );
		std::uint8_t * base = static_cast<std::uint8_t *>( D.host.ptr );
		for ( int k = 0; k < m; k++ ) {
			const TaReactiveDraw & d = q[size_t( i0 + k )];
			const VkDeviceSize o = at[size_t( k )], nv = VkDeviceSize( d.pos.size() / 3 );
			std::uint8_t * p = base + o;
			std::memcpy( p, d.clipJit, 64 );
			std::uint8_t ps[kTaPsBytes] = {};  // uvScale @0, uvOffset @8, useTex @16, hasColor @20, matAlpha @24, outRG @32
			std::memcpy( ps, d.uvScale, 8 );
			std::memcpy( ps + 8, d.uvOffset, 8 );
			std::memcpy( ps + 16, &d.useTex, 4 );
			std::memcpy( ps + 20, &d.hasColor, 4 );
			std::memcpy( ps + 24, &d.matAlpha, 4 );
			std::memcpy( ps + 32, d.outRG, 8 );
			std::memcpy( p + kTaAlign, ps, size_t( kTaPsBytes ) );
			TaRec & rc = D.recs[size_t( k )];
			rc.pos = o + 2 * kTaAlign;
			rc.col = rc.pos + nv * 12;
			rc.uv = rc.col + nv * 16;
			rc.idx = ( ( rc.uv + nv * 8 ) + 63 ) / 64 * 64;
			rc.count = std::uint32_t( d.idx.size() );
			std::memcpy( base + rc.pos, d.pos.data(), size_t( nv * 12 ) );
			float * col = reinterpret_cast<float *>( base + rc.col );
			for ( VkDeviceSize i = 0; i < nv; i++ ) {
				if ( d.col.empty() ) {
					col[i * 4] = col[i * 4 + 1] = col[i * 4 + 2] = 0.0f;
					col[i * 4 + 3] = 1.0f;
				} else {
					std::memcpy( &col[i * 4], &d.col[size_t( i * 4 )], 16 );
				}
			}
			if ( d.uv.empty() )
				std::memset( base + rc.uv, 0, size_t( nv * 8 ) );
			else
				std::memcpy( base + rc.uv, d.uv.data(), size_t( nv * 8 ) );
			std::memcpy( base + rc.idx, d.idx.data(), d.idx.size() * 4 );
			rc.set = D.sets[size_t( k )];
			// the descriptors: b31 x 2, the base map (or GL's zero texture) and its sampler
			VkImageView view = F.d2;
			if ( d.useTex && d.base.tex )
				view = texView[size_t( std::find( texs.begin(), texs.end(), d.base.tex ) - texs.begin() )];
			VkSampler sm = waterSampler( smpD, d.base, false, false, w );
			if ( !sm )
				return breakOff( w );
			bi[size_t( 2 * k )] = { D.host.buf, o, kTaVsBytes };
			bi[size_t( 2 * k + 1 )] = { D.host.buf, o + kTaAlign, kTaPsBytes };
			ii[size_t( 2 * k )].imageView = view;
			ii[size_t( 2 * k )].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
			ii[size_t( 2 * k + 1 )].sampler = sm;
			static const std::uint32_t kBind[kTaBindings] = { 32, 31, 64, 128 };
			for ( int b = 0; b < kTaBindings; b++ ) {
				VkWriteDescriptorSet & x = ws[size_t( k * kTaBindings + b )];
				x.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
				x.dstSet = rc.set;
				x.dstBinding = kBind[b];
				x.descriptorCount = 1;
				if ( b < 2 ) {
					x.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
					x.pBufferInfo = &bi[size_t( 2 * k + b )];
				} else {
					x.descriptorType = b == 2 ? VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE : VK_DESCRIPTOR_TYPE_SAMPLER;
					x.pImageInfo = &ii[size_t( 2 * k + b - 2 )];
				}
			}
		}
		v.vkUpdateDescriptorSets( v.dev, std::uint32_t( ws.size() ), ws.data(), 0, nullptr );
		if ( r.red & 1024 )  // red: draw k binds draw k-1's set (its uniforms and base map), its own geometry
			for ( int k = m - 1; k > 0; k-- )
				D.recs[size_t( k )].set = D.recs[size_t( k - 1 )].set;
		if ( ( r.red & 2048 ) && m > 1 )  // red: draw 1 of each crossing dropped
			D.recs.erase( D.recs.begin() + 1 );
		D.w = r.w;
		D.h = r.h;
		F.kind = 11;
		F.w = r.w;
		F.h = r.h;
		if ( !fxCross( names, w ) )
			return breakOff( w );  // nothing signals vkDone; the mask is untouched (the twin is copied back only at the end)
		crossings++;
		S.draws += m;
		i0 = i1;
	}
	g.copyImageSubData( maskTwin, kTexture2D, 0, 0, 0, 0, r.mask, kTexture2D, 0, 0, 0, 0, r.w, r.h, 1 );
	S.crossings += crossings;
	const double ms = nowMs() - t0;
	S.cpuMs += ms;
	S.maxMs = std::max( S.maxMs, ms );
	return true;
}

void taReactiveOnGl( int draws, const char * why )
{
	TaReactiveStat & S = taD().st;
	S.gl += draws;
	S.glWhy = why ? why : "";
}

TaReactiveStat takeTaReactiveStat()
{
	TaReactive & D = taD();
	TaReactiveStat s = D.st;
	s.pipelines = D.pipelines;
	s.buildMs = D.buildMs;
	if ( D.broken && s.failWhy.empty() )
		s.failWhy = D.brokenWhy;
	D.st = TaReactiveStat();
	return s;
}
}  // namespace WwVkPresent

#ifndef WW_VKPRESENT_NO_QT
namespace WwVkPresent
{
void taReactiveSay( bool wanted, const char * glWhy )
{
	static QString said;
	static double at = 0.0;
	const QString head = QStringLiteral( "vk taareactive: " );
	if ( !wanted ) {
		const QString line = head + QStringLiteral( "on GL (%1)" ).arg( QString::fromUtf8( glWhy ? glWhy : "" ) );
		if ( line != said ) {
			said = line;
			qInfo().noquote() << line;
		}
		return;
	}
	const double now = nowMs();
	if ( now - at < 5000.0 && said.startsWith( head + QStringLiteral( "wanted" ) ) )
		return;
	at = now;
	const TaReactiveStat s = takeTaReactiveStat();
	QString line = head + QStringLiteral( "wanted on Vulkan, %1 draws in %2 crossings over %3 passes (taa_reactive twins into the "
		"TAA mask; %4 ms CPU a pass, max %5, %6 base map copies, %7 fails%8); on GL %9 draws" )
		.arg( s.draws ).arg( s.crossings ).arg( s.flushes ).arg( s.flushes ? s.cpuMs / s.flushes : 0.0, 0, 'f', 3 )
		.arg( s.maxMs, 0, 'f', 3 ).arg( s.copies ).arg( s.fails )
		.arg( s.failWhy.empty() ? QString() : QStringLiteral( ": " ) + QString::fromStdString( s.failWhy ) ).arg( s.gl );
	if ( !s.glWhy.empty() )
		line += QStringLiteral( " (refused: %1)" ).arg( QString::fromStdString( s.glWhy ) );
	said = line;
	qInfo().noquote() << line;
}
}  // namespace WwVkPresent
#endif
