/* Lane VKAIR14 (2026-11-16): the air inject's stage 1 with a history, the |3 temporal reader, on the present device
 * (vkairtemporal.h). Included once by vkpresent.cpp after vkairinject.inc, whose twins (kSpec), census (kImg), host slice
 * layout and crossing it reuses. GL's pass (cellvolair.cpp vaPassInner, stage 1, temporal): viewport cols x rows, no depth /
 * blend / cull, the quad, for k < slices: inject layer k at attachment 0, out history layer p x slices + k at 1 + p, volLayer
 * k. Here: a render pass of 1 + strata RGBA32F attachments (LOAD / STORE, GENERAL) and a pipeline per (twin, strata); one
 * framebuffer a slice over sliceView()s of the two shared targets (kept until a key, the grid or a shared target changes:
 * sharedGen); one descriptor set and one host slice (zeros + b31 with volLayer = k) a slice from the pass's own pool; the 64
 * render passes in one command buffer, one crossing (Work k = -1). */
#define WW_VK_AIRTEMPORAL 1
#include "features/vkframe/vkairtemporal.h"

namespace
{
namespace airtemporal
{
using namespace WwVkPresent;
using airinject::kSpec;
using airinject::kLooseAt;
using airinject::kZeroAt;
using airinject::kHostBytes;
using airinject::kGlobalBytes;
using airinject::kShadowBytes;
static_assert( kHostBytes % 256 == 0, "a slice's host bytes keep the uniform offset alignment" );

struct At
{
	bool made = false, broken = false;
	std::string brokenWhy;
	farlod::WaterDraw D[2];  // twinStatic's layout / modules / binds (the air inject's twins), the blocks' exported copy
	VkRenderPass pass[2] = { VK_NULL_HANDLE, VK_NULL_HANDLE };  // [0] strata 4, [1] strata 6
	VkPipeline pipe[2][2] = {};                                 // [twin][strata]
	VkDescriptorPool pool[2] = { VK_NULL_HANDLE, VK_NULL_HANDLE };
	std::vector<VkDescriptorSet> sets[2];
	CsmBuf host;  // slices x kHostBytes: a slice's zeros + b31
	VkSemaphore glDone = VK_NULL_HANDLE, vkDone = VK_NULL_HANDLE;
	GLuint glDoneGl = 0, vkDoneGl = 0;
	void * semCtx = nullptr;
	VkCommandBuffer cmd = VK_NULL_HANDLE;
	VkFence fence = VK_NULL_HANDLE;
	bool pending = false;
	CsmBuf quad;
	// the framebuffers' cache: a slice's views of the two shared targets
	std::uintptr_t fbInject = 0, fbOut = 0;
	int fbStrata = 0, fbSlices = 0, fbCols = 0, fbRows = 0, fbRot = -1;
	std::uint64_t fbGen = 0;
	std::vector<VkImageView> views;
	std::vector<VkFramebuffer> fbs;
	// this crossing
	int cur = 0, curS = 0, cols = 0, rows = 0, slices = 0;
	char keys[2][128] = {};  // the inputs' twin keys (slot + binding)
	WaterRun run;
	AirTemporalStat st;
};

At & atS()
{
	static At s;
	return s;
}

bool atStatic( std::string & why )
{
	Vk & v = vk();
	At & S = atS();
	if ( S.made )
		return true;
	for ( int i = 0; i < 2; i++ ) {
		const std::uint32_t n = 1 + ( i ? 6 : 4 );
		std::vector<VkAttachmentDescription> at( n );
		std::vector<VkAttachmentReference> cr( n );
		for ( std::uint32_t a = 0; a < n; a++ ) {
			at[a] = {};
			at[a].format = VK_FORMAT_R32G32B32A32_SFLOAT;
			at[a].samples = VK_SAMPLE_COUNT_1_BIT;
			at[a].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
			at[a].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
			at[a].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			at[a].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			at[a].initialLayout = at[a].finalLayout = VK_IMAGE_LAYOUT_GENERAL;
			cr[a] = { a, VK_IMAGE_LAYOUT_GENERAL };
		}
		VkSubpassDescription sp {};
		sp.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		sp.colorAttachmentCount = n;
		sp.pColorAttachments = cr.data();
		const VkPipelineStageFlags cs = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		const VkAccessFlags crw = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		const VkSubpassDependency dep[2] = { { VK_SUBPASS_EXTERNAL, 0, cs, cs, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, crw, 0 },
			{ 0, VK_SUBPASS_EXTERNAL, cs, cs, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, crw, 0 } };
		VkRenderPassCreateInfo rci { VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO };
		rci.attachmentCount = n;
		rci.pAttachments = at.data();
		rci.subpassCount = 1;
		rci.pSubpasses = &sp;
		rci.dependencyCount = 2;
		rci.pDependencies = dep;
		if ( v.vkCreateRenderPass( v.dev, &rci, nullptr, &S.pass[i] ) != VK_SUCCESS ) {
			why = "vkCreateRenderPass (the air temporal reader) failed";
			return false;
		}
	}
	if ( !csmBuf( S.quad, 6 * 12, false, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, why, "the air temporal reader's quad" ) )
		return false;
	const float q[18] = { -1, -1, 0, 1, -1, 0, -1, 1, 0, -1, 1, 0, 1, -1, 0, 1, 1, 0 };  // GL's indices 0 1 2, 2 1 3
	std::memcpy( S.quad.ptr, q, sizeof( q ) );
	VkFenceCreateInfo fi { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
	VkCommandBufferAllocateInfo cai { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
	cai.commandPool = v.pool;
	cai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	cai.commandBufferCount = 1;
	if ( v.vkCreateFence( v.dev, &fi, nullptr, &S.fence ) != VK_SUCCESS || v.vkAllocateCommandBuffers( v.dev, &cai, &S.cmd ) != VK_SUCCESS ) {
		why = "the air temporal reader's fence / command buffer failed";
		return false;
	}
	S.made = true;
	return true;
}

// a (twin, strata) pipeline, made at its first run: airinject's aiPipe with 1 + strata colour attachments
bool atPipe( int k, int si, std::string & why )
{
	Vk & v = vk();
	At & S = atS();
	if ( S.pipe[k][si] )
		return true;
	const double t0 = nowMs();
	VkPipelineShaderStageCreateInfo st[2] { { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO },
		{ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO } };
	st[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
	st[0].module = S.D[k].vs;
	st[0].pName = "main";
	st[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	st[1].module = S.D[k].ps;
	st[1].pName = "main";
	const VkVertexInputBindingDescription vb { 0, 12, VK_VERTEX_INPUT_RATE_VERTEX };
	const VkVertexInputAttributeDescription va { 0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0 };
	VkPipelineVertexInputStateCreateInfo vi { VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
	vi.vertexBindingDescriptionCount = 1;
	vi.pVertexBindingDescriptions = &vb;
	vi.vertexAttributeDescriptionCount = 1;
	vi.pVertexAttributeDescriptions = &va;
	VkPipelineInputAssemblyStateCreateInfo ia { VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
	ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	VkPipelineViewportStateCreateInfo vps { VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO };
	vps.viewportCount = vps.scissorCount = 1;
	VkPipelineRasterizationStateCreateInfo rs { VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
	rs.polygonMode = VK_POLYGON_MODE_FILL;
	rs.cullMode = VK_CULL_MODE_NONE;
	rs.frontFace = VK_FRONT_FACE_CLOCKWISE;
	rs.lineWidth = 1.0f;
	VkPipelineMultisampleStateCreateInfo ms { VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
	ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	VkPipelineDepthStencilStateCreateInfo ds { VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };
	VkPipelineColorBlendAttachmentState cb[7] {};
	for ( VkPipelineColorBlendAttachmentState & c : cb )
		c.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	VkPipelineColorBlendStateCreateInfo cbs { VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
	cbs.attachmentCount = 1 + ( si ? 6 : 4 );
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
	gp.layout = S.D[k].layout;
	gp.renderPass = S.pass[si];
	if ( v.vkCreateGraphicsPipelines( v.dev, tone().cache, 1, &gp, nullptr, &S.pipe[k][si] ) != VK_SUCCESS ) {
		why = std::string( "vkCreateGraphicsPipelines (the air temporal reader, " ) + kSpec[k].what + ") failed";
		return false;
	}
	S.st.pipelines++;
	S.st.buildMs += nowMs() - t0;
	toneCacheSave();
	return true;
}

bool atSems( std::string & why )
{
	Vk & v = vk();
	Gl & g = gl();
	At & S = atS();
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

// `n` sets of twin k's layout from the pass's own pool (twinPoolN holds 32, a grid has 64 slices)
bool atPool( int k, std::size_t n, std::string & why )
{
	Vk & v = vk();
	At & S = atS();
	if ( S.sets[k].size() >= n )
		return true;
	if ( S.pool[k] )
		v.vkDestroyDescriptorPool( v.dev, S.pool[k], nullptr );
	S.pool[k] = VK_NULL_HANDLE;
	S.sets[k].clear();
	std::uint32_t cnt[4] = {};
	for ( const FarSetSlot & b : S.D[k].binds )
		cnt[b.kind]++;
	static const VkDescriptorType kType[4] = { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
		VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, VK_DESCRIPTOR_TYPE_SAMPLER };
	std::vector<VkDescriptorPoolSize> ps;
	for ( int t = 0; t < 4; t++ )
		if ( cnt[t] )
			ps.push_back( { kType[t], cnt[t] * std::uint32_t( n ) } );
	VkDescriptorPoolCreateInfo pi { VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
	pi.maxSets = std::uint32_t( n );
	pi.poolSizeCount = std::uint32_t( ps.size() );
	pi.pPoolSizes = ps.data();
	if ( v.vkCreateDescriptorPool( v.dev, &pi, nullptr, &S.pool[k] ) != VK_SUCCESS ) {
		why = "the air temporal reader's descriptor pool failed";
		return false;
	}
	std::vector<VkDescriptorSetLayout> l( n, S.D[k].dsl );
	VkDescriptorSetAllocateInfo ai { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
	ai.descriptorPool = S.pool[k];
	ai.descriptorSetCount = std::uint32_t( n );
	ai.pSetLayouts = l.data();
	S.sets[k].resize( n );
	if ( v.vkAllocateDescriptorSets( v.dev, &ai, S.sets[k].data() ) != VK_SUCCESS ) {
		S.sets[k].clear();
		why = "the air temporal reader's descriptor sets failed";
		return false;
	}
	return true;
}

void atFbFree()
{
	Vk & v = vk();
	At & S = atS();
	for ( VkFramebuffer f : S.fbs )
		if ( f )
			v.vkDestroyFramebuffer( v.dev, f, nullptr );
	for ( VkImageView x : S.views )
		if ( x )
			v.vkDestroyImageView( v.dev, x, nullptr );
	S.fbs.clear();
	S.views.clear();
	S.fbInject = S.fbOut = 0;
	S.fbRot = -1;
}

/* a slice's framebuffer: inject layer k at 0, out history layer q x slices + k at 1 + p, q = p (red 2: (p + 1) mod strata);
 * the views are made once a key / grid / shared target generation */
bool atFbs( const AirTemporalRun & r, const Shared & inj, const Shared & out, std::string & why )
{
	Vk & v = vk();
	At & S = atS();
	const int rot = ( r.red & 2 ) ? 1 : 0;
	if ( !S.fbs.empty() && S.fbInject == r.injectKey && S.fbOut == r.histOutKey && S.fbStrata == r.strata && S.fbSlices == r.slices
		&& S.fbCols == r.cols && S.fbRows == r.rows && S.fbRot == rot && S.fbGen == sharedGen() )
		return true;
	atFbFree();
	const int ns = r.strata;
	S.views.assign( std::size_t( r.slices ) * std::size_t( 1 + ns ), VK_NULL_HANDLE );
	S.fbs.assign( std::size_t( r.slices ), VK_NULL_HANDLE );
	for ( int k = 0; k < r.slices; k++ ) {
		VkImageView * a = &S.views[std::size_t( k ) * std::size_t( 1 + ns )];
		a[0] = sliceView( inj, k, why );
		if ( !a[0] )
			return atFbFree(), why = "the inject: " + why, false;
		for ( int p = 0; p < ns; p++ ) {
			const int q = rot ? ( p + 1 ) % ns : p;
			a[1 + p] = sliceView( out, q * r.slices + k, why );
			if ( !a[1 + p] )
				return atFbFree(), why = "the out history: " + why, false;
		}
		VkFramebufferCreateInfo fci { VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
		fci.renderPass = S.pass[ns == 6 ? 1 : 0];
		fci.attachmentCount = std::uint32_t( 1 + ns );
		fci.pAttachments = a;
		fci.width = std::uint32_t( r.cols );
		fci.height = std::uint32_t( r.rows );
		fci.layers = 1;
		if ( v.vkCreateFramebuffer( v.dev, &fci, nullptr, &S.fbs[std::size_t( k )] ) != VK_SUCCESS )
			return atFbFree(), why = "the air temporal reader's framebuffer failed", false;
	}
	S.fbInject = r.injectKey;
	S.fbOut = r.histOutKey;
	S.fbStrata = ns;
	S.fbSlices = r.slices;
	S.fbCols = r.cols;
	S.fbRows = r.rows;
	S.fbRot = rot;
	S.fbGen = sharedGen();
	return true;
}

// the worker: a render pass and a draw a slice (its own set), GL's signal waited, signalled back, the fence
bool atWork( std::string & why )
{
	Vk & v = vk();
	At & S = atS();
	farlod::WaterDraw & D = S.D[S.cur];
	v.vkResetFences( v.dev, 1, &S.fence );
	v.vkResetCommandBuffer( S.cmd, 0 );
	VkCommandBufferBeginInfo bi { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
	bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	v.vkBeginCommandBuffer( S.cmd, &bi );
	v.vkCmdBindPipeline( S.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, S.pipe[S.cur][S.curS] );
	const VkDeviceSize zero = 0;
	v.vkCmdBindVertexBuffers( S.cmd, 0, 1, &S.quad.buf, &zero );
	// no y flip: GL's window rows from the bottom are the image's rows from the top (and gl_FragCoord.y with them)
	const VkViewport vp { 0.0f, 0.0f, float( S.cols ), float( S.rows ), 0.0f, 1.0f };
	const VkRect2D sc { { 0, 0 }, { std::uint32_t( S.cols ), std::uint32_t( S.rows ) } };
	v.vkCmdSetViewport( S.cmd, 0, 1, &vp );
	v.vkCmdSetScissor( S.cmd, 0, 1, &sc );
	for ( int k = 0; k < S.slices; k++ ) {
		VkRenderPassBeginInfo rbi { VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
		rbi.renderPass = S.pass[S.curS];
		rbi.framebuffer = S.fbs[std::size_t( k )];
		rbi.renderArea = sc;
		v.vkCmdBeginRenderPass( S.cmd, &rbi, VK_SUBPASS_CONTENTS_INLINE );
		v.vkCmdBindDescriptorSets( S.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, D.layout, 0, 1, &S.sets[S.cur][std::size_t( k )], 0, nullptr );
		v.vkCmdDraw( S.cmd, 6, 1, 0, 0 );
		v.vkCmdEndRenderPass( S.cmd );
	}
	if ( v.vkEndCommandBuffer( S.cmd ) != VK_SUCCESS ) {
		why = "vkEndCommandBuffer (the air temporal reader) failed";
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
		why = "vkQueueSubmit (the air temporal reader) returned " + std::to_string( int( r ) );
		return false;
	}
	return true;
}

// a GL buffer texture that is the present device's texel buffer (farTexelBuffer: GL writes it, Vulkan reads it in place)
const FcBuf * texelOnDevice( GLuint tex )
{
	if ( !tex )
		return nullptr;
	for ( const auto & kv : farCards().bufs )
		if ( kv.second.glTex == tex && kv.second.glCtx == gl().ctx && kv.second.view )
			return &kv.second;
	return nullptr;
}
}  // namespace airtemporal
}  // namespace

namespace WwVkPresent
{
std::string airTemporalRefusal()
{
	airtemporal::At & S = airtemporal::atS();
	if ( S.broken )
		return S.brokenWhy;
	if ( !vk().ok || !gl().ok || gl().ctx != reinterpret_cast<void *>( wglGetCurrentContext() ) )
		return "the present device or GL's entry points are not ready in this context";
	return std::string();
}

WaterRun & airTemporalScratch()
{
	return airtemporal::atS().run;
}

bool airTemporalRunState( const AirTemporalRun & r, WaterRun & s, std::string & why )
{
	using namespace airtemporal;
	At & S = atS();
	Gl & g = gl();
	Vk & v = vk();
	Fx & F = fx();
	const double t0 = nowMs();
	auto refuse = [&]( const std::string & w ) {
		why = w;
		S.st.fails++;
		S.st.failWhy = w;
		return false;
	};
	auto breakOff = [&]( const std::string & w ) {
		S.broken = true;
		S.brokenWhy = "the air temporal reader on Vulkan broke: " + w;
		return refuse( S.brokenWhy );
	};
	if ( const std::string no = airTemporalRefusal(); !no.empty() )
		return refuse( no );
	const int k = r.rt ? 1 : 0;
	const farlod::TwinSpec & P = kSpec[k];
	if ( r.wwDefs != airTemporalKey( r.rt ) )
		return refuse( "GL's air program has the key \"" + r.wwDefs + "\", the " + ( r.rt ? "RT GI" : "default" ) + " twin's key is \""
			+ airTemporalKey( r.rt ) + "\"" );
	if ( r.strata != 4 && r.strata != 6 )
		return refuse( "strata " + std::to_string( r.strata ) + " (the twin's targets are 4 or 6)" );
	if ( r.cols <= 0 || r.rows <= 0 || r.slices <= 0 )
		return refuse( "an empty grid" );
	// the targets: the inject (3D, at least the grid) and the out history (3D, the grid x strata exactly), both slice-viewable
	const Shared * inj = sharedFind( r.injectKey );
	const Shared * out = sharedFind( r.histOutKey );
	if ( !inj || !inj->img || !inj->tex || inj->type != 1 || inj->fmt != VK_FORMAT_R32G32B32A32_SFLOAT || !inj->slices )
		return refuse( "the inject is not a shared RGBA32F 3D target with slice views" );
	if ( !out || !out->img || !out->tex || out->type != 1 || out->fmt != VK_FORMAT_R32G32B32A32_SFLOAT || !out->slices )
		return refuse( "the out history is not a shared RGBA32F 3D target with slice views" );
	if ( inj->w < r.cols || inj->h < r.rows || inj->d < r.slices )
		return refuse( "the inject is " + std::to_string( inj->w ) + " x " + std::to_string( inj->h ) + " x " + std::to_string( inj->d )
			+ ", smaller than the grid" );
	if ( out->w != r.cols || out->h != r.rows || out->d != r.strata * r.slices )
		return refuse( "the out history is " + std::to_string( out->w ) + " x " + std::to_string( out->h ) + " x " + std::to_string( out->d )
			+ ", the grid x strata is " + std::to_string( r.cols ) + " x " + std::to_string( r.rows ) + " x "
			+ std::to_string( r.strata * r.slices ) );
	const GLuint injTex = inj->tex, outTex = out->tex;
	// the inputs refused by name, before any device work
	const WaterSlot * hist = nullptr;
	for ( const WaterSlot & c : s.census.inputs ) {
		if ( c.name == "volHist" )
			hist = &c;
		if ( !c.tex )
			continue;
		if ( c.tex == injTex || c.tex == outTex )
			return refuse( c.name + ": feedback (samples a target the pass draws into)" );
		if ( ( c.name == "cellDdgiAtlas" || c.name == "cellRestir" ) && !texelOnDevice( c.tex ) )
			return refuse( c.name + ": a GL-owned texel buffer (not the present device's; the RT GI inputs are never copied a run)" );
		if ( c.name == "volLampAtlas" && !( c.kind == 5 ? texelOnDevice( c.tex ) != nullptr : farSharedOf( c.tex ) != 0 ) )
			return refuse( "volLampAtlas: bound and not shared" );
	}
	if ( !hist || !hist->tex )
		return refuse( "volHist: nothing bound on its unit" );
	const std::uintptr_t histKey = farSharedOf( hist->tex );
	if ( histKey && histKey == r.histOutKey )
		return refuse( "volHist: feedback (the out history is the history in)" );
	if ( !histKey )
		return refuse( "volHist: the history in is not a shared target (never copied a run)" );
	const FxField * tab = r.rt ? kT_airinjectRtPs : kT_airinjectPs;
	const int nTab = r.rt ? int( sizeof( kT_airinjectRtPs ) / sizeof( kT_airinjectRtPs[0] ) ) : int( sizeof( kT_airinjectPs ) / sizeof( kT_airinjectPs[0] ) );
	const FxField * fLayer = airinject::fieldOf( tab, nTab, "volLayer" );
	if ( !fLayer )
		return breakOff( "the air inject's table has no volLayer" );
	std::string w;
	if ( !fxStatic( w ) || !atStatic( w ) || !atSems( w ) || !farlod::twinStatic( S.D[k], P, w ) || !atPool( k, std::size_t( r.slices ), w ) )
		return breakOff( w );
	const int si = r.strata == 6 ? 1 : 0;
	if ( !atPipe( k, si, w ) )
		return breakOff( w );
	farlod::WaterDraw & D = S.D[k];
	// the last crossing is done before its command buffer, sets, slices and framebuffers are written again
	if ( std::string fw; !fenceDone( S.fence, S.pending, "airtemporal", fw ) )
		return refuse( fw );
	S.pending = false;
	if ( !atFbs( r, *inj, *out, w ) )
		return refuse( w );
	// GL's blocks into the exported copy (airinject's layout); an unbound one reads slice 0's zeros
	const WaterUbo * ubos[2] = { &s.global, &s.shadow };
	const VkDeviceSize need[2] = { kGlobalBytes, kShadowBytes };
	const VkDeviceSize uboAt[2] = { 0, 512 };
	bool uboHost[2] = {};
	for ( int b = 0; b < 2; b++ ) {
		const WaterUbo & u = *ubos[b];
		uboHost[b] = !u.buf || u.size <= 0 || u.off < 0 || u.off + u.size > u.bufBytes || ( u.off % 4 );
	}
	if ( !csmBuf( D.geo, 1024, true, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, w, "the air temporal reader's blocks" )
		|| !csmBuf( S.host, kHostBytes * VkDeviceSize( r.slices ), false, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, w,
			"the air temporal reader's slices" ) )
		return breakOff( w );
	// a slice's host bytes: zeros, then GL's b31 with volLayer = k (red 1: 0 on every slice)
	for ( int z = 0; z < r.slices; z++ ) {
		std::uint8_t * h = static_cast<std::uint8_t *>( S.host.ptr ) + std::size_t( kHostBytes ) * std::size_t( z );
		std::memset( h + kZeroAt, 0, std::size_t( kLooseAt - kZeroAt ) );
		std::memcpy( h + kLooseAt, s.psLoose, std::size_t( P.psLoose ) );
		const int layer = ( r.red & 1 ) ? 0 : z;
		if ( fLayer->isInt ) {
			const std::int32_t x = layer;
			std::memcpy( h + kLooseAt + fLayer->offset, &x, 4 );
		} else {
			const float x = float( layer );
			std::memcpy( h + kLooseAt + fLayer->offset, &x, 4 );
		}
	}
	// the inputs: shared targets and the present device's texel buffers in place, others one copy a run, unbound ones zero
	std::vector<GLuint> texNames { injTex, outTex };
	std::vector<GLuint> bufNames { D.geo.gl };
	const std::size_t nb = D.binds.size();
	std::vector<VkDescriptorImageInfo> ii( nb );
	std::vector<VkBufferView> tv( nb );
	std::vector<VkDescriptorType> ty( nb, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER );
	std::vector<VkDescriptorBufferInfo> bi( nb );
	int copies = 0, bufCopies = 0;
	const std::uintptr_t keyBase = reinterpret_cast<std::uintptr_t>( &S.keys[k][0] );
	auto nameOf = [&]( std::uint32_t binding ) -> const char * {
		for ( std::size_t q = 0; q < P.nImg; q++ )
			if ( P.img[q].binding == binding )
				return P.img[q].name;
		return nullptr;
	};
	auto slotOf = [&]( const char * nm ) -> const WaterSlot * {
		if ( nm )
			for ( const WaterSlot & c : s.census.inputs )
				if ( c.name == nm )
					return &c;
		return nullptr;
	};
	for ( std::size_t i = 0; i < nb; i++ ) {
		const FarSetSlot & b = D.binds[i];
		if ( b.kind == 0 ) {
			if ( b.binding != 0 && b.binding != 1 && b.binding != 31 )
				return breakOff( "the twin reads a block at b" + std::to_string( b.binding ) + " the air temporal reader does not fill" );
			continue;
		}
		if ( b.kind == 3 ) {
			const char * nm = nameOf( b.binding - 64 );
			const WaterSlot * c = slotOf( nm );
			WaterSlot plain;
			VkSampler sm = farlod::waterSampler( D, c ? *c : plain, b.compare, !c || !c->tex, w );
			if ( !sm )
				return breakOff( w );
			if ( b.compare && c && c->tex && c->cmpMode != 0x884E )
				return refuse( std::string( nm ) + ": the twin compares, GL's texture does not" );
			ty[i] = VK_DESCRIPTOR_TYPE_SAMPLER;
			ii[i].sampler = sm;
			continue;
		}
		const char * nm = nameOf( b.binding );
		if ( !nm )
			return breakOff( "the twin reads an image at b" + std::to_string( b.binding ) + " no census name maps" );
		const WaterSlot * c = slotOf( nm );
		const std::uintptr_t key = keyBase + b.binding;
		if ( b.kind == 2 ) {
			ty[i] = VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
			if ( const FcBuf * fb = c ? texelOnDevice( c->tex ) : nullptr ) {  // the present device's (the DDGI cascades' buffer)
				tv[i] = fb->view;
				bufNames.push_back( fb->glBuf );
				continue;
			}
			const bool bound = c && c->tex && c->dataBuf && c->bufBytes > 0;
			if ( c && c->tex && !bound )
				return refuse( std::string( nm ) + ": a buffer texture with no data store" );
			unsigned tb = 0, tt = 0;
			const bool ints = std::strcmp( nm, "cellCluster" ) == 0 || std::strcmp( nm, "volClusters" ) == 0;  // Buffer<int4>: RGBA32I
			if ( !farTexelBuffer( key, bound ? c->glFmt : ints ? 0x8D82u : 0x8814u, bound ? std::size_t( c->bufBytes ) : 16u, tb, tt, w ) )
				return refuse( std::string( nm ) + ": " + w );
			if ( bound ) {
				g.copyNamedBufferSubData( c->dataBuf, tb, 0, 0, std::intptr_t( c->bufBytes ) );
				bufCopies++;
			}
			bufNames.push_back( tb );
			tv[i] = farCards().bufs[key].view;
			continue;
		}
		VkImageView view = VK_NULL_HANDLE;
		if ( !c || !c->tex ) {
			if ( b.dim == 3 && !( b.arrayed && D.zeroCubeView ) )
				return refuse( std::string( nm ) + ": an unbound cube input (no zero cube view)" );
			view = b.dim == 3 ? D.zeroCubeView : b.dim == 2 ? F.d3 : b.arrayed ? F.d2a : F.d2;
		} else if ( c->baseLevel != 0 ) {
			return refuse( std::string( nm ) + ": a base level above 0" );
		} else if ( const std::uintptr_t sk = farSharedOf( c->tex ) ) {
			view = sharedFind( sk )->view;  // a shared target: read in place
			texNames.push_back( c->tex );
		} else {
			unsigned twin = 0;
			if ( !waterTwin( key, *c, twin, w ) )
				return refuse( std::string( nm ) + ": " + w );
			const bool viaFar = c->kind == 4 || waterBcFormat( c->glFmt );
			const Shared * th = viaFar ? farTwinFind( key ) : sharedFind( key );
			if ( !th || !th->view )
				return refuse( std::string( nm ) + ": its twin has no view" );
			view = th->view;
			static const GLenum kTarget[5] = { 0x0DE1, 0x806F, 0x8C1A, 0x8513, 0x9009 };
			const GLenum target = kTarget[std::clamp( c->kind, 0, 4 )];
			for ( int l = 0; l < std::max( c->levels, 1 ); l++ ) {
				const int lw = std::max( c->w >> l, 1 ), lh = std::max( c->h >> l, 1 );
				const int ld = c->kind == 1 ? std::max( c->d >> l, 1 ) : ( c->kind == 2 || c->kind == 4 ) ? c->d : 1;
				g.copyImageSubData( c->tex, target, l, 0, 0, 0, twin, target, l, 0, 0, 0, lw, lh, ld );
			}
			copies++;
			texNames.push_back( twin );
		}
		ty[i] = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
		ii[i].imageView = view;
		ii[i].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	}
	// every slice's set: the same inputs, its own b31
	std::vector<VkDescriptorBufferInfo> b31( std::size_t( r.slices ) );
	std::vector<VkWriteDescriptorSet> ws;
	ws.reserve( nb * std::size_t( r.slices ) );
	for ( int z = 0; z < r.slices; z++ ) {
		b31[std::size_t( z )] = { S.host.buf, VkDeviceSize( z ) * kHostBytes + kLooseAt, VkDeviceSize( P.psLoose ) };
		for ( std::size_t i = 0; i < nb; i++ ) {
			const FarSetSlot & b = D.binds[i];
			VkWriteDescriptorSet x { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
			x.dstSet = S.sets[k][std::size_t( z )];
			x.dstBinding = b.binding;
			x.descriptorCount = 1;
			x.descriptorType = ty[i];
			if ( b.kind == 0 ) {
				if ( b.binding == 31 ) {
					x.pBufferInfo = &b31[std::size_t( z )];
				} else {
					bi[i] = uboHost[b.binding] ? VkDescriptorBufferInfo { S.host.buf, kZeroAt, need[b.binding] }
						: VkDescriptorBufferInfo { D.geo.buf, uboAt[b.binding], need[b.binding] };
					x.pBufferInfo = &bi[i];
				}
			} else if ( b.kind == 2 ) {
				x.pTexelBufferView = &tv[i];
			} else {
				x.pImageInfo = &ii[i];
			}
			ws.push_back( x );
		}
	}
	v.vkUpdateDescriptorSets( v.dev, std::uint32_t( ws.size() ), ws.data(), 0, nullptr );
	for ( int b = 0; b < 2; b++ )
		if ( !uboHost[b] )
			g.copyNamedBufferSubData( GLuint( ubos[b]->buf ), D.geo.gl, std::intptr_t( ubos[b]->off ), std::intptr_t( uboAt[b] ),
				std::intptr_t( std::min<long long>( ubos[b]->size, static_cast<long long>( need[b] ) ) ) );
	S.cur = k;
	S.curS = si;
	S.cols = r.cols;
	S.rows = r.rows;
	S.slices = r.slices;
	// GL's work so far signalled (the targets, the inputs, the copies; each name once) -> the worker's job -> GL waits
	std::sort( texNames.begin(), texNames.end() );
	texNames.erase( std::unique( texNames.begin(), texNames.end() ), texNames.end() );
	std::sort( bufNames.begin(), bufNames.end() );
	bufNames.erase( std::unique( bufNames.begin(), bufNames.end() ), bufNames.end() );
	std::vector<GLenum> lay( texNames.size(), kLayoutGeneral );
	g.signalSemaphore( S.glDoneGl, GLuint( bufNames.size() ), bufNames.data(), GLuint( texNames.size() ), texNames.data(), lay.data() );
	g.flush();
	Worker & W = wk();
	std::uint64_t want = 0;
	{
		std::lock_guard<std::mutex> lk( W.m );
		want = W.toneDone + 1;
	}
	Work j;
	j.k = -1;
	j.fn = atWork;
	hand( j );
	bool ok = false;
	{
		std::unique_lock<std::mutex> lk( W.m );
		W.idle.wait( lk, [&W, want]() { return W.toneDone >= want; } );
		ok = W.toneOk;
		w = W.toneWhy;
	}
	if ( !ok )
		return breakOff( w );  // nothing signals vkDone: GL does not wait it; GL draws the stage
	g.waitSemaphore( S.vkDoneGl, GLuint( bufNames.size() ), bufNames.data(), GLuint( texNames.size() ), texNames.data(), lay.data() );
	S.pending = true;
	const double ms = nowMs() - t0;
	S.st.runs++;
	S.st.draws += r.slices;
	S.st.copies += copies;
	S.st.bufCopies += bufCopies;
	S.st.cpuMs += ms;
	S.st.maxMs = std::max( S.st.maxMs, ms );
	return true;
}

void airTemporalOnGl( const char * why )
{
	airtemporal::At & S = airtemporal::atS();
	S.st.gl++;
	S.st.glWhy = why ? why : "";
}

AirTemporalStat takeAirTemporalStat()
{
	airtemporal::At & S = airtemporal::atS();
	AirTemporalStat s = S.st;
	if ( S.broken && s.failWhy.empty() )
		s.failWhy = S.brokenWhy;
	const int pipes = S.st.pipelines;
	const double build = S.st.buildMs;
	S.st = AirTemporalStat();
	S.st.pipelines = pipes;
	S.st.buildMs = build;
	return s;
}
}  // namespace WwVkPresent
