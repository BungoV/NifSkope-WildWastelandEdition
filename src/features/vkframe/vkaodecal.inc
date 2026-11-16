/* Lane VKAO3 (2026-11-16): the baked AO decals (cellaodecalgl.cpp wwCellAoDecalRun) on the present device (vkaodecal.h).
 * Included once by vkpresent.cpp at its end. GL's pass: target (R32F, cleared 1) + selfId (R32F, cleared 0) + the opaque
 * D24S8; per copy draw 1 (the mask: aoMode 0 into selfId, LEQUAL, polygon offset (-1, -4), stencil bit 0x80 set), draw 2
 * (the AO: aoMode 1 into target, no depth test, cull FRONT, depth clamp, stencil NOTEQUAL 0x80, blend ONE ONE or DST_COLOR
 * ZERO), then the bit cleared. Here: one render pass over the three shared images (LOAD / STORE, GENERAL), the colours
 * cleared before it; the bit clear is a draw (no fragment stage, stencil op ZERO, write mask 0x80: a clear ignores the write
 * mask). The g-buffer and the models' volumes are GL's own: copied into route-owned shared twins (the volumes and vertices
 * only when the owner's generation moves). Self-contained: every helper is ad-prefixed. */
#include "features/vkframe/vkaodecal.h"
#include "features/vkframe/aodecal_tables.h"
#include "features/vkframe/aodecal_view_vs.spv.h"  // vkframe_spirv.sh's fxt / fxg aodecal lines
#include "features/vkframe/aodecal_view_ps.spv.h"

namespace
{
namespace aodecal
{
using namespace WwVkPresent;

// a draw's uniform slice: the vertex stage's b31 (binding 287) at 0, the pixel stage's (binding 31) at 256
constexpr VkDeviceSize kAdSlice = 768, kAdVsAt = 0, kAdPsAt = 256;
static_assert( VkDeviceSize( kT_aodecalVsBytes ) <= kAdPsAt && kAdPsAt + VkDeviceSize( kT_aodecalPsBytes ) <= kAdSlice, "aodecal's slice" );
constexpr int kAdMaxCopies = 4096, kAdMaxVols = 1024;
// pipelines: 0 the mask, 1 the bit clear, 2 + (add 1, cull BACK 2, no clamp 4) the AO
constexpr int kAdPipes = 10;

struct AdJob
{
	int pipe = 0, set = 0;
	VkDeviceSize geoOff = 0;
	std::uint32_t first = 0, count = 0;
};

struct AdVol
{
	unsigned gl = 0, tex = 0;
	std::uint64_t gen = 0;
	int dims[3] = {};
};

struct AdVbo
{
	std::uint64_t gen = 0;
	int verts = 0;
	std::vector<float> pos;
};

struct AdState
{
	bool made = false, broken = false;
	std::string brokenWhy;
	VkShaderModule vs = VK_NULL_HANDLE, ps = VK_NULL_HANDLE;
	VkDescriptorSetLayout dsl = VK_NULL_HANDLE;
	VkPipelineLayout layout = VK_NULL_HANDLE;
	VkSampler plain = VK_NULL_HANDLE;
	VkRenderPass pass = VK_NULL_HANDLE;
	VkPipeline pipe[kAdPipes] = {};
	VkDescriptorPool pool = VK_NULL_HANDLE;
	std::vector<VkDescriptorSet> sets;
	VkSemaphore glDone = VK_NULL_HANDLE, vkDone = VK_NULL_HANDLE;
	GLuint glDoneGl = 0, vkDoneGl = 0;
	void * semCtx = nullptr;
	VkCommandBuffer cmd = VK_NULL_HANDLE;
	VkFence fence = VK_NULL_HANDLE;
	bool pending = false;
	CsmBuf geo, ubo;
	VkFramebuffer fb = VK_NULL_HANDLE;
	VkImageView fbViews[3] = {};
	int fbW = 0, fbH = 0;
	std::uint64_t fbGen = 0;
	void( WINAPI * getNamedBufferSubData )( GLuint, std::intptr_t, std::ptrdiff_t, void * ) = nullptr;
	// the route's own twins: the g-buffer, the volumes (a slot a GL name), the vertices (by GL name)
	AdVol vols[kAdMaxVols];
	std::map<unsigned, AdVbo> vbos;
	// the crossing's job, written by the drawing thread before the hand
	VkImage clearImg[2] = {};
	std::vector<AdJob> jobs;
	AoDecalStat st;
};

AdState & adState()
{
	static AdState s;
	return s;
}

// owner keys of the route's twins (sharedTarget keys: addresses no other owner uses)
char g_adGbufKey;
char g_adVolKeys[kAdMaxVols];

// a field by name into a slice: an array (count > 1) at 16 bytes an element, as std140 / cbuffer packing lays it
bool adPut( std::uint8_t * at, const FxField * t, int nt, const char * name, const void * src, std::string & why )
{
	for ( int i = 0; i < nt; i++ ) {
		if ( std::strcmp( t[i].name, name ) != 0 )
			continue;
		const std::uint8_t * s = static_cast<const std::uint8_t *>( src );
		const int n = std::max( t[i].count, 1 );
		for ( int e = 0; e < n; e++ )
			std::memcpy( at + t[i].offset + 16 * e, s + std::size_t( 4 * t[i].words * e ), std::size_t( 4 * t[i].words ) );
		return true;
	}
	why = std::string( "the aodecal table has no " ) + name;
	return false;
}

bool adStatic( std::string & why )
{
	Vk & v = vk();
	AdState & S = adState();
	if ( S.made )
		return true;
	if ( !fxModule( vkframe_aodecal_view_vs_spv, sizeof( vkframe_aodecal_view_vs_spv ), S.vs )
		|| !fxModule( vkframe_aodecal_view_ps_spv, sizeof( vkframe_aodecal_view_ps_spv ), S.ps ) ) {
		why = "the aodecal twins' modules failed";
		return false;
	}
	const VkDescriptorSetLayoutBinding lb[6] = { { 287, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, nullptr },
		{ 31, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr },
		{ 64, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr },	// aoGbuf
		{ 65, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr },	// aoVol
		{ 128, VK_DESCRIPTOR_TYPE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr },
		{ 129, VK_DESCRIPTOR_TYPE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr } };
	VkDescriptorSetLayoutCreateInfo lci { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
	lci.bindingCount = 6;
	lci.pBindings = lb;
	VkPipelineLayoutCreateInfo pli { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
	pli.setLayoutCount = 1;
	pli.pSetLayouts = &S.dsl;
	if ( v.vkCreateDescriptorSetLayout( v.dev, &lci, nullptr, &S.dsl ) != VK_SUCCESS
		|| v.vkCreatePipelineLayout( v.dev, &pli, nullptr, &S.layout ) != VK_SUCCESS ) {
		why = "the aodecal set / pipeline layout failed";
		return false;
	}
	VkSamplerCreateInfo sci { VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
	sci.magFilter = sci.minFilter = VK_FILTER_NEAREST;
	sci.addressModeU = sci.addressModeV = sci.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	sci.maxLod = 0.0f;
	if ( v.vkCreateSampler( v.dev, &sci, nullptr, &S.plain ) != VK_SUCCESS ) {
		why = "the aodecal sampler failed";
		return false;
	}
	// the render pass: att0 target R32F, att1 selfId R32F, att2 the opaque D24S8 (depth and stencil read and kept)
	VkAttachmentDescription at[3] {};
	const VkFormat fmts[3] = { VK_FORMAT_R32_SFLOAT, VK_FORMAT_R32_SFLOAT, VK_FORMAT_D24_UNORM_S8_UINT };
	for ( int i = 0; i < 3; i++ ) {
		at[i].format = fmts[i];
		at[i].samples = VK_SAMPLE_COUNT_1_BIT;
		at[i].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
		at[i].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		at[i].stencilLoadOp = i == 2 ? VK_ATTACHMENT_LOAD_OP_LOAD : VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		at[i].stencilStoreOp = i == 2 ? VK_ATTACHMENT_STORE_OP_STORE : VK_ATTACHMENT_STORE_OP_DONT_CARE;
		at[i].initialLayout = at[i].finalLayout = VK_IMAGE_LAYOUT_GENERAL;
	}
	const VkAttachmentReference cr[2] = { { 0, VK_IMAGE_LAYOUT_GENERAL }, { 1, VK_IMAGE_LAYOUT_GENERAL } };
	const VkAttachmentReference dr { 2, VK_IMAGE_LAYOUT_GENERAL };
	VkSubpassDescription sp {};
	sp.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	sp.colorAttachmentCount = 2;
	sp.pColorAttachments = cr;
	sp.pDepthStencilAttachment = &dr;
	const VkPipelineStageFlags st = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT
		| VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
	const VkAccessFlags rw = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
		| VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	const VkSubpassDependency dep[2] = { { VK_SUBPASS_EXTERNAL, 0, st | VK_PIPELINE_STAGE_TRANSFER_BIT, st,
		rw | VK_ACCESS_TRANSFER_WRITE_BIT, rw, 0 }, { 0, VK_SUBPASS_EXTERNAL, st, st, rw, rw, 0 } };
	VkRenderPassCreateInfo rci { VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO };
	rci.attachmentCount = 3;
	rci.pAttachments = at;
	rci.subpassCount = 1;
	rci.pSubpasses = &sp;
	rci.dependencyCount = 2;
	rci.pDependencies = dep;
	if ( v.vkCreateRenderPass( v.dev, &rci, nullptr, &S.pass ) != VK_SUCCESS ) {
		S.pass = VK_NULL_HANDLE;
		why = "vkCreateRenderPass (aodecal) failed";
		return false;
	}
	S.getNamedBufferSubData = reinterpret_cast<decltype( S.getNamedBufferSubData )>( wglGetProcAddress( "glGetNamedBufferSubData" ) );
	if ( !S.getNamedBufferSubData ) {
		why = "no glGetNamedBufferSubData (the decal vertices' read)";
		return false;
	}
	VkFenceCreateInfo fi { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
	VkCommandBufferAllocateInfo cai { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
	cai.commandPool = v.pool;
	cai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	cai.commandBufferCount = 1;
	if ( v.vkCreateFence( v.dev, &fi, nullptr, &S.fence ) != VK_SUCCESS || v.vkAllocateCommandBuffers( v.dev, &cai, &S.cmd ) != VK_SUCCESS ) {
		why = "the aodecal fence / command buffer failed";
		return false;
	}
	S.made = true;
	return true;
}

// pipeline p: 0 the mask, 1 the bit clear, 2 + (1 add, 2 cull BACK, 4 no clamp) the AO; GL's state for each
bool adPipe( int p, std::string & why )
{
	Vk & v = vk();
	AdState & S = adState();
	if ( S.pipe[p] )
		return true;
	const double t0 = nowMs();
	const bool mask = p == 0, zero = p == 1, ao = p >= 2;
	const bool add = ao && ( ( p - 2 ) & 1 ), cullBack = ao && ( ( p - 2 ) & 2 ), noClamp = ao && ( ( p - 2 ) & 4 );
	VkPipelineShaderStageCreateInfo stg[2] { { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO },
		{ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO } };
	stg[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
	stg[0].module = S.vs;
	stg[0].pName = "main";
	stg[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	stg[1].module = S.ps;
	stg[1].pName = "main";
	const VkVertexInputBindingDescription vb { 0, 12, VK_VERTEX_INPUT_RATE_VERTEX };
	const VkVertexInputAttributeDescription va { 0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0 };
	VkPipelineVertexInputStateCreateInfo vi { VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
	vi.vertexBindingDescriptionCount = 1;
	vi.pVertexBindingDescriptions = &vb;
	vi.vertexAttributeDescriptionCount = 1;
	vi.pVertexAttributeDescriptions = &va;
	VkPipelineInputAssemblyStateCreateInfo ia { VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
	ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	VkPipelineViewportDepthClipControlCreateInfoEXT cc { VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_DEPTH_CLIP_CONTROL_CREATE_INFO_EXT };
	cc.negativeOneToOne = VK_TRUE;
	VkPipelineViewportStateCreateInfo vps { VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO };
	vps.pNext = &cc;
	vps.viewportCount = vps.scissorCount = 1;
	VkPipelineRasterizationStateCreateInfo rs { VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
	rs.polygonMode = VK_POLYGON_MODE_FILL;
	rs.cullMode = ao ? ( cullBack ? VK_CULL_MODE_BACK_BIT : VK_CULL_MODE_FRONT_BIT ) : VK_CULL_MODE_NONE;
	// GL's CCW with no y flip: the image's rows run the other way, so GL's front is Vulkan's clockwise
	rs.frontFace = VK_FRONT_FACE_CLOCKWISE;
	rs.depthClampEnable = ao && !noClamp ? VK_TRUE : VK_FALSE;
	rs.depthBiasEnable = mask ? VK_TRUE : VK_FALSE;  // glPolygonOffset( -1, -4 ): vkCmdSetDepthBias in the record
	rs.lineWidth = 1.0f;
	VkPipelineMultisampleStateCreateInfo ms { VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
	ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	VkPipelineDepthStencilStateCreateInfo ds { VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };
	ds.depthTestEnable = mask ? VK_TRUE : VK_FALSE;
	ds.depthWriteEnable = VK_FALSE;
	ds.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
	ds.stencilTestEnable = VK_TRUE;
	VkStencilOpState so {};
	so.reference = 0x80;
	so.compareMask = 0x80;
	if ( mask ) {  // ALWAYS, KEEP KEEP REPLACE, write 0x80
		so.compareOp = VK_COMPARE_OP_ALWAYS;
		so.failOp = so.depthFailOp = VK_STENCIL_OP_KEEP;
		so.passOp = VK_STENCIL_OP_REPLACE;
		so.writeMask = 0x80;
	} else if ( zero ) {  // the bit clear: every texel's 0x80 to 0
		so.compareOp = VK_COMPARE_OP_ALWAYS;
		so.failOp = so.depthFailOp = so.passOp = VK_STENCIL_OP_ZERO;
		so.writeMask = 0x80;
	} else {  // NOTEQUAL 0x80, KEEP, write 0
		so.compareOp = VK_COMPARE_OP_NOT_EQUAL;
		so.failOp = so.depthFailOp = so.passOp = VK_STENCIL_OP_KEEP;
		so.writeMask = 0;
	}
	ds.front = ds.back = so;
	VkPipelineColorBlendAttachmentState cb[2] {};
	const VkColorComponentFlags all = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	cb[0].colorWriteMask = ao ? all : 0;  // draw buffers {0, NONE}
	cb[1].colorWriteMask = mask ? all : 0;  // draw buffers {NONE, 1}
	if ( ao ) {
		cb[0].blendEnable = VK_TRUE;
		cb[0].srcColorBlendFactor = cb[0].srcAlphaBlendFactor = add ? VK_BLEND_FACTOR_ONE : VK_BLEND_FACTOR_DST_COLOR;
		cb[0].dstColorBlendFactor = cb[0].dstAlphaBlendFactor = add ? VK_BLEND_FACTOR_ONE : VK_BLEND_FACTOR_ZERO;
		cb[0].colorBlendOp = cb[0].alphaBlendOp = VK_BLEND_OP_ADD;
	}
	VkPipelineColorBlendStateCreateInfo cbs { VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
	cbs.attachmentCount = 2;
	cbs.pAttachments = cb;
	const VkDynamicState dyn[3] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR, VK_DYNAMIC_STATE_DEPTH_BIAS };
	VkPipelineDynamicStateCreateInfo dy { VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
	dy.dynamicStateCount = mask ? 3 : 2;
	dy.pDynamicStates = dyn;
	VkGraphicsPipelineCreateInfo gp { VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
	gp.stageCount = zero ? 1 : 2;  // the bit clear has no fragment stage
	gp.pStages = stg;
	gp.pVertexInputState = &vi;
	gp.pInputAssemblyState = &ia;
	gp.pViewportState = &vps;
	gp.pRasterizationState = &rs;
	gp.pMultisampleState = &ms;
	gp.pDepthStencilState = &ds;
	gp.pColorBlendState = &cbs;
	gp.pDynamicState = &dy;
	gp.layout = S.layout;
	gp.renderPass = S.pass;
	if ( v.vkCreateGraphicsPipelines( v.dev, tone().cache, 1, &gp, nullptr, &S.pipe[p] ) != VK_SUCCESS ) {
		S.pipe[p] = VK_NULL_HANDLE;
		why = "vkCreateGraphicsPipelines (aodecal " + std::to_string( p ) + ") failed";
		return false;
	}
	S.st.pipelines++;
	S.st.buildMs += nowMs() - t0;
	toneCacheSave();
	return true;
}

bool adSems( std::string & why )
{
	Vk & v = vk();
	Gl & g = gl();
	AdState & S = adState();
	if ( S.glDone && S.semCtx == g.ctx )
		return true;
	for ( VkSemaphore * s : { &S.glDone, &S.vkDone } )
		if ( *s )
			v.vkDestroySemaphore( v.dev, *s, nullptr );
	S.glDone = S.vkDone = VK_NULL_HANDLE;
	S.glDoneGl = S.vkDoneGl = 0;
	S.semCtx = nullptr;
	if ( !exportSem( S.glDone, S.glDoneGl, why ) || !exportSem( S.vkDone, S.vkDoneGl, why ) )
		return false;
	S.semCtx = g.ctx;
	return true;
}

bool adFb( const Shared * tg, const Shared * id, const Shared * dsT, std::string & why )
{
	Vk & v = vk();
	AdState & S = adState();
	const VkImageView views[3] = { tg->view, id->view, dsT->attView ? dsT->attView : dsT->view };
	if ( S.fb && S.fbGen == sharedGen() && S.fbW == tg->w && S.fbH == tg->h && std::equal( views, views + 3, S.fbViews ) )
		return true;
	if ( S.fb )
		v.vkDestroyFramebuffer( v.dev, S.fb, nullptr );
	S.fb = VK_NULL_HANDLE;
	VkFramebufferCreateInfo fci { VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
	fci.renderPass = S.pass;
	fci.attachmentCount = 3;
	fci.pAttachments = views;
	fci.width = std::uint32_t( tg->w );
	fci.height = std::uint32_t( tg->h );
	fci.layers = 1;
	if ( v.vkCreateFramebuffer( v.dev, &fci, nullptr, &S.fb ) != VK_SUCCESS ) {
		why = "the aodecal framebuffer failed";
		return false;
	}
	std::copy( views, views + 3, S.fbViews );
	S.fbW = tg->w;
	S.fbH = tg->h;
	S.fbGen = sharedGen();
	return true;
}

bool adSets( int n, std::string & why )
{
	Vk & v = vk();
	AdState & S = adState();
	if ( int( S.sets.size() ) >= n )
		return true;
	int cap = 64;
	while ( cap < n )
		cap *= 2;
	if ( S.pool )
		v.vkDestroyDescriptorPool( v.dev, S.pool, nullptr );
	S.pool = VK_NULL_HANDLE;
	S.sets.clear();
	const std::uint32_t c = std::uint32_t( cap );
	const VkDescriptorPoolSize ps[3] = { { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 2 * c }, { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 2 * c },
		{ VK_DESCRIPTOR_TYPE_SAMPLER, 2 * c } };
	VkDescriptorPoolCreateInfo pci { VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
	pci.maxSets = c;
	pci.poolSizeCount = 3;
	pci.pPoolSizes = ps;
	if ( v.vkCreateDescriptorPool( v.dev, &pci, nullptr, &S.pool ) != VK_SUCCESS ) {
		why = "the aodecal descriptor pool failed";
		return false;
	}
	std::vector<VkDescriptorSetLayout> lay( std::size_t( cap ), S.dsl );
	S.sets.assign( std::size_t( cap ), VK_NULL_HANDLE );
	VkDescriptorSetAllocateInfo ai { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
	ai.descriptorPool = S.pool;
	ai.descriptorSetCount = c;
	ai.pSetLayouts = lay.data();
	if ( v.vkAllocateDescriptorSets( v.dev, &ai, S.sets.data() ) != VK_SUCCESS ) {
		S.sets.clear();
		why = "the aodecal descriptor sets failed";
		return false;
	}
	return true;
}

// the worker: the colours cleared (target 1, selfId 0), one render pass, the jobs in GL's order; GL's signal waited, signalled back
bool adWork( std::string & why )
{
	Vk & v = vk();
	AdState & S = adState();
	v.vkResetFences( v.dev, 1, &S.fence );
	v.vkResetCommandBuffer( S.cmd, 0 );
	VkCommandBufferBeginInfo bi { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
	bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	v.vkBeginCommandBuffer( S.cmd, &bi );
	const VkImageSubresourceRange rg { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
	VkClearColorValue cv {};
	cv.float32[0] = 1.0f;
	v.vkCmdClearColorImage( S.cmd, S.clearImg[0], VK_IMAGE_LAYOUT_GENERAL, &cv, 1, &rg );
	cv.float32[0] = 0.0f;
	v.vkCmdClearColorImage( S.cmd, S.clearImg[1], VK_IMAGE_LAYOUT_GENERAL, &cv, 1, &rg );
	VkMemoryBarrier mb { VK_STRUCTURE_TYPE_MEMORY_BARRIER };
	mb.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	mb.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	v.vkCmdPipelineBarrier( S.cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 1, &mb, 0, nullptr,
		0, nullptr );
	VkRenderPassBeginInfo rbi { VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
	rbi.renderPass = S.pass;
	rbi.framebuffer = S.fb;
	rbi.renderArea = { { 0, 0 }, { std::uint32_t( S.fbW ), std::uint32_t( S.fbH ) } };
	v.vkCmdBeginRenderPass( S.cmd, &rbi, VK_SUBPASS_CONTENTS_INLINE );
	// no y flip: GL's rows from the bottom are the image's rows from the top
	const VkViewport vp { 0.0f, 0.0f, float( S.fbW ), float( S.fbH ), 0.0f, 1.0f };
	const VkRect2D sc { { 0, 0 }, { std::uint32_t( S.fbW ), std::uint32_t( S.fbH ) } };
	int bound = -1;
	for ( const AdJob & j : S.jobs ) {
		if ( j.pipe != bound ) {
			v.vkCmdBindPipeline( S.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, S.pipe[j.pipe] );
			v.vkCmdSetViewport( S.cmd, 0, 1, &vp );
			v.vkCmdSetScissor( S.cmd, 0, 1, &sc );
			if ( j.pipe == 0 )  // glPolygonOffset( -1, -4 ): factor = slope, units = constant
				v.vkCmdSetDepthBias( S.cmd, -4.0f, 0.0f, -1.0f );
			bound = j.pipe;
		}
		v.vkCmdBindVertexBuffers( S.cmd, 0, 1, &S.geo.buf, &j.geoOff );
		v.vkCmdBindDescriptorSets( S.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, S.layout, 0, 1, &S.sets[std::size_t( j.set )], 0, nullptr );
		v.vkCmdDraw( S.cmd, j.count, 1, j.first, 0 );
	}
	v.vkCmdEndRenderPass( S.cmd );
	if ( v.vkEndCommandBuffer( S.cmd ) != VK_SUCCESS ) {
		why = "vkEndCommandBuffer (aodecal) failed";
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
		why = "vkQueueSubmit (aodecal) returned " + std::to_string( int( r ) );
		return false;
	}
	return true;
}
}  // namespace aodecal
}  // namespace

namespace WwVkPresent
{
std::string aoDecalRefusal()
{
	aodecal::AdState & S = aodecal::adState();
	if ( S.broken )
		return S.brokenWhy;
	Vk & v = vk();
	if ( !v.ok || !gl().ok || gl().ctx != reinterpret_cast<void *>( wglGetCurrentContext() ) )
		return "the present device or GL's entry points are not ready in this context";
	if ( !v.clipControl )
		return "no depthClipControl on the present device (GL's -1..1 clip z)";
	if ( !v.depthClamp )
		return "no depthClamp on the present device (the AO box's GL_DEPTH_CLAMP)";
	if ( !v.independentBlend )
		return "no independentBlend on the present device (the per-attachment write masks)";
	if ( !gl().copyImageSubData )
		return "no glCopyImageSubData (the g-buffer / volume twins)";
	return std::string();
}

bool aoDecalRun( const AoDecalRun & r, std::string & why )
{
	using namespace aodecal;
	AdState & S = adState();
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
		S.brokenWhy = "aodecal on Vulkan broke: " + w;
		return refuse( S.brokenWhy );
	};
	if ( const std::string no = aoDecalRefusal(); !no.empty() )
		return refuse( no );
	const int n = int( r.copies.size() );
	if ( n > kAdMaxCopies )
		return refuse( std::to_string( n ) + " copies (the route takes " + std::to_string( kAdMaxCopies ) + ")" );
	// the targets: the owner's shared D24S8, target and selfId at the pass's size
	const Shared * tg = sharedFind( r.target );
	const Shared * id = sharedFind( r.selfId );
	const Shared * dsT = sharedFind( r.ds );
	if ( !tg || !tg->tex || tg->fmt != VK_FORMAT_R32_SFLOAT || !id || !id->tex || id->fmt != VK_FORMAT_R32_SFLOAT )
		return refuse( "the decal target / selfId are not shared R32F" );
	if ( !dsT || !dsT->tex || dsT->fmt != VK_FORMAT_D24_UNORM_S8_UINT )
		return refuse( "the opaque depth is not a shared D24S8" );
	if ( tg->w != r.w || tg->h != r.h || id->w != r.w || id->h != r.h || dsT->w != r.w || dsT->h != r.h || tg->samples != 1
		|| dsT->samples != 1 || tg->type != 0 || id->type != 0 || dsT->type != 0 )
		return refuse( "the shared targets are not the pass's size (single-sample 2D)" );
	if ( !r.gbuf )
		return refuse( "no g-buffer" );
	for ( int c = 0; c < n; c++ ) {
		const AoDecalCopy & x = r.copies[std::size_t( c )];
		if ( !x.vol || !x.vbo || x.maskVerts < 0 || x.dims[0] < 1 || x.dims[1] < 1 || x.dims[2] < 1 )
			return refuse( "copy " + std::to_string( c ) + ": no volume, vertices or dims" );
	}
	std::string w;
	if ( !fxStatic( w ) || !adStatic( w ) || !adSems( w ) )
		return breakOff( w );
	const bool add = r.add != ( ( r.red & 2 ) != 0 );
	const int aoPipe = 2 + ( add ? 1 : 0 ) + ( ( r.red & 8 ) ? 2 : 0 ) + ( ( r.red & 16 ) ? 4 : 0 );
	if ( !adPipe( 0, w ) || !adPipe( 1, w ) || !adPipe( aoPipe, w ) )
		return breakOff( w );
	if ( std::string fw; !fenceDone( S.fence, S.pending, "aodecal", fw ) )
		return refuse( fw );
	S.pending = false;
	// the inputs' twins (GL commands, before the signal): the g-buffer every run, a volume when its generation moves
	unsigned gbTex = 0;
	if ( !sharedTarget( reinterpret_cast<std::uintptr_t>( &g_adGbufKey ), 0, r.w, r.h, 1, kRgba32f, gbTex, w ) )
		return refuse( "the g-buffer twin: " + w );
	g.copyImageSubData( r.gbuf, kTexture2D, 0, 0, 0, 0, gbTex, kTexture2D, 0, 0, 0, 0, r.w, r.h, 1 );
	int twinCopies = 1;
	std::vector<int> volOf( std::size_t( n ), -1 );
	for ( int c = 0; c < n; c++ ) {
		const AoDecalCopy & x = r.copies[std::size_t( c )];
		int slot = -1, freeSlot = -1;
		for ( int i = 0; i < kAdMaxVols && slot < 0; i++ ) {
			if ( S.vols[i].gl == x.vol )
				slot = i;
			else if ( freeSlot < 0 && !S.vols[i].gl )
				freeSlot = i;
		}
		if ( slot < 0 ) {
			if ( freeSlot < 0 )
				return refuse( "more than " + std::to_string( kAdMaxVols ) + " decal volumes" );
			slot = freeSlot;
			S.vols[slot] = AdVol();
			S.vols[slot].gl = x.vol;
		}
		AdVol & vo = S.vols[slot];
		unsigned tex = 0;
		if ( !sharedTarget( reinterpret_cast<std::uintptr_t>( &g_adVolKeys[slot] ), 1, x.dims[0], x.dims[1], 3 * x.dims[2], kRgba32f, tex, w ) ) {
			vo = AdVol();
			return refuse( "a volume twin: " + w );
		}
		const bool same = vo.tex == tex && x.volGen != 0 && vo.gen == x.volGen && std::equal( x.dims, x.dims + 3, vo.dims );
		if ( !same ) {
			g.copyImageSubData( x.vol, kTexture3D, 0, 0, 0, 0, tex, kTexture3D, 0, 0, 0, 0, x.dims[0], x.dims[1], 3 * x.dims[2] );
			twinCopies++;
			vo.tex = tex;
			vo.gen = x.volGen;
			std::copy( x.dims, x.dims + 3, vo.dims );
		}
		volOf[std::size_t( c )] = slot;
	}
	// the vertices: GL's buffers read when their generation moves (vec3: the mask's, then the box's 36)
	std::size_t geoNeed = 48;
	for ( int c = 0; c < n; c++ ) {
		const AoDecalCopy & x = r.copies[std::size_t( c )];
		const int verts = x.maskVerts + 36;
		AdVbo & b = S.vbos[x.vbo];
		if ( x.volGen == 0 || b.gen != x.volGen || b.verts != verts ) {
			b.pos.assign( std::size_t( verts ) * 3, 0.0f );
			while ( g.getError() != 0 ) {
			}
			S.getNamedBufferSubData( x.vbo, 0, std::ptrdiff_t( b.pos.size() * 4 ), b.pos.data() );
			if ( g.getError() != 0 ) {
				S.vbos.erase( x.vbo );
				return refuse( "copy " + std::to_string( c ) + ": the vertex buffer read failed (" + std::to_string( verts ) + " vertices)" );
			}
			b.gen = x.volGen;
			b.verts = verts;
		}
		geoNeed += ( std::size_t( verts ) * 12 + 15 ) / 16 * 16;
	}
	const int nSlices = 2 * n + 1;
	const Shared * gb = sharedFind( reinterpret_cast<std::uintptr_t>( &g_adGbufKey ) );
	if ( !gb )
		return refuse( "the g-buffer twin is gone" );
	if ( !adFb( tg, id, dsT, w ) || !adSets( nSlices, w ) || !csmBuf( S.geo, VkDeviceSize( geoNeed ), false, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, w )
		|| !csmBuf( S.ubo, kAdSlice * VkDeviceSize( nSlices ), false, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, w ) )
		return breakOff( w );
	std::uint8_t * geo = static_cast<std::uint8_t *>( S.geo.ptr );
	std::uint8_t * ubo = static_cast<std::uint8_t *>( S.ubo.ptr );
	// slice 0: the bit clear's full-screen triangle (identity aoClip; z 0 inside GL's -1..1)
	const float tri[9] = { -1.0f, -1.0f, 0.0f, 3.0f, -1.0f, 0.0f, -1.0f, 3.0f, 0.0f };
	std::memset( geo, 0, 48 );
	std::memcpy( geo, tri, sizeof( tri ) );
	const int nVs = int( sizeof( kT_aodecalVs ) / sizeof( kT_aodecalVs[0] ) );
	const int nPs = int( sizeof( kT_aodecalPs ) / sizeof( kT_aodecalPs[0] ) );
	std::memset( ubo, 0, std::size_t( kAdSlice ) * std::size_t( nSlices ) );
	{
		const float ident[16] = { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 };
		if ( !adPut( ubo + kAdVsAt, kT_aodecalVs, nVs, "aoClip", ident, w ) )
			return breakOff( w );
	}
	VkDeviceSize at = 48;
	S.jobs.clear();
	S.jobs.push_back( AdJob { 1, 0, 0, 0, 3 } );
	const float size[2] = { float( r.w ), float( r.h ) };
	const int addI = r.add ? 1 : 0;
	int draws = 1;
	for ( int c = 0; c < n; c++ ) {
		const AoDecalCopy & x = r.copies[std::size_t( c )];
		const AdVbo & b = S.vbos[x.vbo];
		const VkDeviceSize off = at;
		std::memcpy( geo + off, b.pos.data(), b.pos.size() * 4 );
		at += VkDeviceSize( ( b.pos.size() * 4 + 15 ) / 16 * 16 );
		for ( int k = 0; k < 2; k++ ) {
			const int s = 1 + 2 * c + k;
			std::uint8_t * sl = ubo + kAdSlice * VkDeviceSize( s );
			const int mode = k == 0 ? 0 : ( r.red & 4 ) ? 0 : 1;
			const bool ok = adPut( sl + kAdVsAt, kT_aodecalVs, nVs, "aoClip", x.clip, w )
				&& adPut( sl + kAdPsAt, kT_aodecalPs, nPs, "aoMode", &mode, w ) && adPut( sl + kAdPsAt, kT_aodecalPs, nPs, "aoSelf", &x.self, w )
				&& adPut( sl + kAdPsAt, kT_aodecalPs, nPs, "aoAdd", &addI, w ) && adPut( sl + kAdPsAt, kT_aodecalPs, nPs, "aoProj", r.proj, w )
				&& adPut( sl + kAdPsAt, kT_aodecalPs, nPs, "aoSize", size, w ) && adPut( sl + kAdPsAt, kT_aodecalPs, nPs, "aoSc", &r.sc, w )
				&& adPut( sl + kAdPsAt, kT_aodecalPs, nPs, "aoM", x.M, w ) && adPut( sl + kAdPsAt, kT_aodecalPs, nPs, "aoN", x.N, w )
				&& adPut( sl + kAdPsAt, kT_aodecalPs, nPs, "aoLo", x.lo, w ) && adPut( sl + kAdPsAt, kT_aodecalPs, nPs, "aoHi", x.hi, w )
				&& adPut( sl + kAdPsAt, kT_aodecalPs, nPs, "aoDims", x.dims, w ) && adPut( sl + kAdPsAt, kT_aodecalPs, nPs, "aoFade", &x.fade, w )
				&& adPut( sl + kAdPsAt, kT_aodecalPs, nPs, "aoC", x.c, w ) && adPut( sl + kAdPsAt, kT_aodecalPs, nPs, "aoCoef", x.coef, w )
				&& adPut( sl + kAdPsAt, kT_aodecalPs, nPs, "aoRcut", &x.rcut, w );
			if ( !ok )
				return breakOff( w );
		}
		// 1. the mask (red 1: dropped), 2. the AO, 3. the bit cleared (red 32: kept)
		if ( x.maskVerts > 0 && !( r.red & 1 ) ) {
			S.jobs.push_back( AdJob { 0, 1 + 2 * c, off, 0, std::uint32_t( x.maskVerts ) } );
			draws++;
		}
		S.jobs.push_back( AdJob { aoPipe, 2 + 2 * c, off, std::uint32_t( x.maskVerts ), 36 } );
		draws++;
		if ( !( r.red & 32 ) ) {
			S.jobs.push_back( AdJob { 1, 0, 0, 0, 3 } );
			draws++;
		}
	}
	// the sets: a slice each, the g-buffer twin, the copy's volume twin (the bit clear's: the zero 3D view)
	Fx & F = fx();
	std::vector<VkDescriptorBufferInfo> bi( std::size_t( nSlices ) * 2 );
	std::vector<VkDescriptorImageInfo> ii( std::size_t( nSlices ) * 4 );
	std::vector<VkWriteDescriptorSet> ws;
	ws.reserve( std::size_t( nSlices ) * 6 );
	for ( int s = 0; s < nSlices; s++ ) {
		const std::size_t q = std::size_t( s );
		const VkDeviceSize sl = kAdSlice * VkDeviceSize( s );
		bi[q * 2] = { S.ubo.buf, sl + kAdVsAt, VkDeviceSize( kT_aodecalVsBytes ) };
		bi[q * 2 + 1] = { S.ubo.buf, sl + kAdPsAt, VkDeviceSize( kT_aodecalPsBytes ) };
		VkImageView volView = F.d3;
		if ( s > 0 ) {
			const Shared * vt = sharedFind( reinterpret_cast<std::uintptr_t>( &g_adVolKeys[volOf[std::size_t( ( s - 1 ) / 2 )]] ) );
			if ( !vt )
				return refuse( "a volume twin is gone" );
			volView = vt->view;
		}
		ii[q * 4] = { VK_NULL_HANDLE, gb->view, VK_IMAGE_LAYOUT_GENERAL };
		ii[q * 4 + 1] = { VK_NULL_HANDLE, volView, VK_IMAGE_LAYOUT_GENERAL };
		ii[q * 4 + 2] = ii[q * 4 + 3] = { S.plain, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_UNDEFINED };
		const std::uint32_t bind[6] = { 287, 31, 64, 65, 128, 129 };
		for ( int k = 0; k < 6; k++ ) {
			VkWriteDescriptorSet x { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
			x.dstSet = S.sets[q];
			x.dstBinding = bind[k];
			x.descriptorCount = 1;
			if ( k < 2 ) {
				x.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
				x.pBufferInfo = &bi[q * 2 + std::size_t( k )];
			} else {
				x.descriptorType = k < 4 ? VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE : VK_DESCRIPTOR_TYPE_SAMPLER;
				x.pImageInfo = &ii[q * 4 + std::size_t( k - 2 )];
			}
			ws.push_back( x );
		}
	}
	v.vkUpdateDescriptorSets( v.dev, std::uint32_t( ws.size() ), ws.data(), 0, nullptr );
	S.clearImg[0] = tg->img;
	S.clearImg[1] = id->img;
	// GL's targets and the twins signalled -> the worker's job -> GL waits
	std::vector<GLuint> texNames { tg->tex, id->tex, dsT->tex, gb->tex };
	for ( int c = 0; c < n; c++ )
		texNames.push_back( S.vols[volOf[std::size_t( c )]].tex );
	std::sort( texNames.begin(), texNames.end() );
	texNames.erase( std::unique( texNames.begin(), texNames.end() ), texNames.end() );
	std::vector<GLenum> lay( texNames.size(), kLayoutGeneral );
	g.signalSemaphore( S.glDoneGl, 0, nullptr, GLuint( texNames.size() ), texNames.data(), lay.data() );
	g.flush();
	if ( !opWorkerRun( adWork, w ) )
		return breakOff( w );  // nothing signals vkDone: GL does not wait it; GL draws the decals
	g.waitSemaphore( S.vkDoneGl, 0, nullptr, GLuint( texNames.size() ), texNames.data(), lay.data() );
	S.pending = true;
	const double ms = nowMs() - t0;
	S.st.runs++;
	S.st.copies += n;
	S.st.draws += draws;
	S.st.twinCopies += twinCopies;
	S.st.cpuMs += ms;
	S.st.maxMs = std::max( S.st.maxMs, ms );
	return true;
}

void aoDecalOnGl( const char * why )
{
	WwVkPresent::glGuard( "aoDecal", 1 );  // lane VKFLIGHT4: the view GL guard
	aodecal::AdState & S = aodecal::adState();
	S.st.gl++;
	S.st.glWhy = why ? why : "";
}

AoDecalStat takeAoDecalStat()
{
	aodecal::AdState & S = aodecal::adState();
	AoDecalStat s = S.st;
	if ( S.broken && s.failWhy.empty() )
		s.failWhy = S.brokenWhy;
	const int pipes = S.st.pipelines;
	const double build = S.st.buildMs;
	S.st = AoDecalStat();
	S.st.pipelines = pipes;
	S.st.buildMs = build;
	return s;
}
}  // namespace WwVkPresent
