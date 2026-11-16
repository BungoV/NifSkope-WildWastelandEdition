/* Lane VKAIR5 (2026-11-16): the air inject's stage 0 on the present device (vkairinject.h). Included once by vkpresent.cpp at
 * its end, after vkreflrelight.inc (whose shape this copies: farlod::TwinSpec / twinStatic / waterSampler, farlodrun::twinPoolN).
 * GL's pass (cellvolair.cpp vaPassInner): viewport cols x rows, no depth, no blend, no cull, the quad { -1 -1, 1 -1, -1 1, 1 1 }
 * as { 0 1 2, 2 1 3 }, fragColor (far, near, 0, 1) into the RG32F tile. Here: its own colour-only render pass (R32G32, LOAD /
 * STORE, GENERAL) and pipeline per twin, a level-0 view + framebuffer of the shared tile (made again when a shared target is
 * made or goes: sharedGen), one draw, one crossing. Only the census row volGbuf is read from GL: in place when it is a shared
 * target (the depth path's D24S8), else one copy a run (the alpha path's RGBA32F); every other image is a zero view and every
 * texel buffer a 16-byte zero buffer. All state (keys, stat, the WaterRun) lives in the pass's struct (Ai). */
#define WW_VK_AIRINJECT 1
#include "features/vkframe/vkairinject.h"
#include "features/vkframe/airinject_view_vs.spv.h"  // vkframe_spirv.sh's fxt / fxa / fxar lines
#include "features/vkframe/airinject_view_ps.spv.h"
#include "features/vkframe/airinject_view_rt_ps.spv.h"

namespace
{
namespace airinject
{
using namespace WwVkPresent;

// the pixel stages' declared images (t n -> 64 + n, samplers at + 64; the RT twin's set, the default one's is a subset)
const farlod::WaterImgName kImg[] = { { 64, "cellLights" }, { 65, "cellGi" }, { 66, "cellGiSlots" }, { 67, "cellRooms" },
	{ 68, "cellIsLut" }, { 69, "cellIsBloom" }, { 70, "cellShadow" }, { 71, "cellRestir" }, { 72, "cellFarSlots" },
	{ 73, "cellFarRecs" }, { 74, "cellFarPlaced" }, { 77, "cellCtGbuf" }, { 79, "cellAoDecal" }, { 80, "cellSsgi" }, { 81, "airTerr" },
	{ 82, "cellGiAir" }, { 83, "rfMap" }, { 89, "atApTex" }, { 90, "volTex" }, { 91, "volTile" }, { 92, "volAirTerr" },
	{ 93, "volAirRainMap" }, { 94, "volAirNoise" }, { 95, "csmMap" }, { 96, "ussTerrMap" }, { 97, "ussContactMap" },
	{ 98, "ussCloudMap" }, { 104, "cellGiHalf" }, { 105, "cellGiHalfKey" }, { 108, "cellDdgiAtlas" }, { 109, "cellCluster" },
	{ 110, "volLampAtlas" }, { 111, "volLampTable" }, { 112, "volLampRays" }, { 113, "volInject" }, { 114, "volClusters" },
	{ 115, "volGbuf" }, { 116, "volCarry" }, { 117, "volHist" }, { 118, "volNoise" }, { 119, "volLampNow" }, { 120, "volLampHist" },
	{ 121, "volStbn" } };
constexpr std::uint32_t kGbufBinding = 115;
// the host slice: zeros for the unread blocks (globalUniforms 336, wwShadowFrame 248 bytes), then b31
constexpr VkDeviceSize kZeroAt = 0, kLooseAt = 512, kHostBytes = 5888;
constexpr VkDeviceSize kGlobalBytes = 336, kShadowBytes = 248;
static_assert( kLooseAt + VkDeviceSize( kT_airinjectRtPsBytes ) <= kHostBytes && kLooseAt + VkDeviceSize( kT_airinjectPsBytes ) <= kHostBytes
		&& kT_airinjectRtPsBytes <= kTwinPsMax && kGlobalBytes <= kLooseAt && kShadowBytes <= kLooseAt,
	"the air inject's host slice" );
// [0] the default key, [1] the RT GI key; the pixel stage's blocks b1 shadow, b31 loose; the kind is never fx's
const farlod::TwinSpec kSpec[2] = {
	{ "air inject", vkframe_airinject_view_vs_spv, sizeof( vkframe_airinject_view_vs_spv ), vkframe_airinject_view_ps_spv,
		sizeof( vkframe_airinject_view_ps_spv ), kImg, sizeof( kImg ) / sizeof( kImg[0] ), nullptr, 0, kT_airinjectPsBytes, kLooseAt,
		kHostBytes, false, 0, true },
	{ "air inject (RT GI)", vkframe_airinject_view_vs_spv, sizeof( vkframe_airinject_view_vs_spv ), vkframe_airinject_view_rt_ps_spv,
		sizeof( vkframe_airinject_view_rt_ps_spv ), kImg, sizeof( kImg ) / sizeof( kImg[0] ), nullptr, 0, kT_airinjectRtPsBytes, kLooseAt,
		kHostBytes, false, 0, true } };

struct Ai
{
	bool made = false, broken = false;
	std::string brokenWhy;
	farlod::WaterDraw D[2];  // twinStatic's layout / modules / binds, twinPoolN's sets, the slice
	VkRenderPass pass = VK_NULL_HANDLE;
	VkPipeline pipe[2] = { VK_NULL_HANDLE, VK_NULL_HANDLE };
	VkSemaphore glDone = VK_NULL_HANDLE, vkDone = VK_NULL_HANDLE;
	GLuint glDoneGl = 0, vkDoneGl = 0;
	void * semCtx = nullptr;
	VkCommandBuffer cmd = VK_NULL_HANDLE;
	VkFence fence = VK_NULL_HANDLE;
	bool pending = false;
	CsmBuf quad;  // six vec3 corners: the two triangles of GL's quad
	std::uintptr_t fbKey = 0;
	VkImageView fbView = VK_NULL_HANDLE;
	VkFramebuffer fb = VK_NULL_HANDLE;
	int fbW = 0, fbH = 0;
	std::uint64_t fbGen = 0;
	int cur = 0;  // the crossing's twin
	char keys[2][128] = {};  // the twins' input keys (slot + binding; never an address the app's shared targets use)
	WaterRun run;            // GL's side of a run (airInjectRun)
	AirInjectStat st;
};

Ai & aiS()
{
	static Ai s;
	return s;
}

// the device-time parts (once): the colour-only render pass, the quad, the command buffer and its fence
bool aiStatic( std::string & why )
{
	Vk & v = vk();
	Ai & S = aiS();
	if ( S.made )
		return true;
	VkAttachmentDescription at {};
	at.format = VK_FORMAT_R32G32_SFLOAT;
	at.samples = VK_SAMPLE_COUNT_1_BIT;
	at.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;  // the quad covers every texel; a miss keeps the old texel, never garbage
	at.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	at.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	at.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	at.initialLayout = at.finalLayout = VK_IMAGE_LAYOUT_GENERAL;
	const VkAttachmentReference cr { 0, VK_IMAGE_LAYOUT_GENERAL };
	VkSubpassDescription sp {};
	sp.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	sp.colorAttachmentCount = 1;
	sp.pColorAttachments = &cr;
	const VkPipelineStageFlags cs = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	const VkAccessFlags crw = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	const VkSubpassDependency dep[2] = { { VK_SUBPASS_EXTERNAL, 0, cs, cs, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, crw, 0 },
		{ 0, VK_SUBPASS_EXTERNAL, cs, cs, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, crw, 0 } };
	VkRenderPassCreateInfo rci { VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO };
	rci.attachmentCount = 1;
	rci.pAttachments = &at;
	rci.subpassCount = 1;
	rci.pSubpasses = &sp;
	rci.dependencyCount = 2;
	rci.pDependencies = dep;
	if ( v.vkCreateRenderPass( v.dev, &rci, nullptr, &S.pass ) != VK_SUCCESS ) {
		why = "vkCreateRenderPass (the air inject) failed";
		return false;
	}
	if ( !csmBuf( S.quad, 6 * 12, false, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, why, "the air inject's quad" ) )
		return false;
	const float q[18] = { -1, -1, 0, 1, -1, 0, -1, 1, 0, -1, 1, 0, 1, -1, 0, 1, 1, 0 };  // GL's indices 0 1 2, 2 1 3
	std::memcpy( S.quad.ptr, q, sizeof( q ) );
	VkFenceCreateInfo fi { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
	VkCommandBufferAllocateInfo cai { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
	cai.commandPool = v.pool;
	cai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	cai.commandBufferCount = 1;
	if ( v.vkCreateFence( v.dev, &fi, nullptr, &S.fence ) != VK_SUCCESS || v.vkAllocateCommandBuffers( v.dev, &cai, &S.cmd ) != VK_SUCCESS ) {
		why = "the air inject's fence / command buffer failed";
		return false;
	}
	S.made = true;
	return true;
}

// a twin's pipeline (made at its first run): position at location 0, no cull / depth / blend, viewport and scissor dynamic
bool aiPipe( int k, std::string & why )
{
	Vk & v = vk();
	Ai & S = aiS();
	if ( S.pipe[k] )
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
	gp.pDepthStencilState = &ds;
	gp.pColorBlendState = &cbs;
	gp.pDynamicState = &dy;
	gp.layout = S.D[k].layout;
	gp.renderPass = S.pass;
	if ( v.vkCreateGraphicsPipelines( v.dev, tone().cache, 1, &gp, nullptr, &S.pipe[k] ) != VK_SUCCESS ) {
		why = std::string( "vkCreateGraphicsPipelines (the " ) + kSpec[k].what + ") failed";
		return false;
	}
	S.st.pipelines++;
	S.st.buildMs += nowMs() - t0;
	toneCacheSave();
	return true;
}

bool aiSems( std::string & why )
{
	Vk & v = vk();
	Gl & g = gl();
	Ai & S = aiS();
	if ( S.glDone && S.semCtx == g.ctx )
		return true;
	for ( VkSemaphore * s : { &S.glDone, &S.vkDone } )  // another context's names: that context went
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

// the level-0 view of the shared tile and its framebuffer (kept until the key or a shared target changes: sharedGen)
bool aiFb( std::uintptr_t key, std::string & why )
{
	Vk & v = vk();
	Ai & S = aiS();
	if ( S.fb && S.fbKey == key && S.fbGen == sharedGen() )
		return true;
	if ( S.fb )
		v.vkDestroyFramebuffer( v.dev, S.fb, nullptr );
	if ( S.fbView )
		v.vkDestroyImageView( v.dev, S.fbView, nullptr );
	S.fb = VK_NULL_HANDLE;
	S.fbView = VK_NULL_HANDLE;
	S.fbKey = 0;
	const Shared * sh = sharedFind( key );
	if ( !sh || !sh->img || sh->fmt != VK_FORMAT_R32G32_SFLOAT || sh->type != 0 ) {
		why = "the air tile is not a shared RG32F 2D target";
		return false;
	}
	VkImageViewCreateInfo ci { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
	ci.image = sh->img;
	ci.viewType = VK_IMAGE_VIEW_TYPE_2D;
	ci.format = VK_FORMAT_R32G32_SFLOAT;
	ci.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
	if ( v.vkCreateImageView( v.dev, &ci, nullptr, &S.fbView ) != VK_SUCCESS ) {
		why = "the air tile's level-0 view failed";
		return false;
	}
	VkFramebufferCreateInfo fci { VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
	fci.renderPass = S.pass;
	fci.attachmentCount = 1;
	fci.pAttachments = &S.fbView;
	fci.width = std::uint32_t( sh->w );
	fci.height = std::uint32_t( sh->h );
	fci.layers = 1;
	if ( v.vkCreateFramebuffer( v.dev, &fci, nullptr, &S.fb ) != VK_SUCCESS ) {
		v.vkDestroyImageView( v.dev, S.fbView, nullptr );
		S.fbView = VK_NULL_HANDLE;
		why = "the air tile's framebuffer failed";
		return false;
	}
	S.fbKey = key;
	S.fbW = sh->w;
	S.fbH = sh->h;
	S.fbGen = sharedGen();
	return true;
}

// the worker: one render pass, one draw (viewport = the whole tile, as GL's), GL's signal waited, signalled back, the fence
bool aiWork( std::string & why )
{
	Vk & v = vk();
	Ai & S = aiS();
	farlod::WaterDraw & D = S.D[S.cur];
	v.vkResetFences( v.dev, 1, &S.fence );
	v.vkResetCommandBuffer( S.cmd, 0 );
	VkCommandBufferBeginInfo bi { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
	bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	v.vkBeginCommandBuffer( S.cmd, &bi );
	VkRenderPassBeginInfo rbi { VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
	rbi.renderPass = S.pass;
	rbi.framebuffer = S.fb;
	rbi.renderArea = { { 0, 0 }, { std::uint32_t( S.fbW ), std::uint32_t( S.fbH ) } };
	v.vkCmdBeginRenderPass( S.cmd, &rbi, VK_SUBPASS_CONTENTS_INLINE );
	v.vkCmdBindPipeline( S.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, S.pipe[S.cur] );
	const VkDeviceSize zero = 0;
	v.vkCmdBindVertexBuffers( S.cmd, 0, 1, &S.quad.buf, &zero );
	// no y flip: GL's window rows from the bottom are the image's rows from the top (and gl_FragCoord.y with them)
	const VkViewport vp { 0.0f, 0.0f, float( S.fbW ), float( S.fbH ), 0.0f, 1.0f };
	const VkRect2D sc { { 0, 0 }, { std::uint32_t( S.fbW ), std::uint32_t( S.fbH ) } };
	v.vkCmdSetViewport( S.cmd, 0, 1, &vp );
	v.vkCmdSetScissor( S.cmd, 0, 1, &sc );
	v.vkCmdBindDescriptorSets( S.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, D.layout, 0, 1, &D.setsN[0], 0, nullptr );
	v.vkCmdDraw( S.cmd, 6, 1, 0, 0 );
	v.vkCmdEndRenderPass( S.cmd );
	if ( v.vkEndCommandBuffer( S.cmd ) != VK_SUCCESS ) {
		why = "vkEndCommandBuffer (the air inject) failed";
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
		why = "vkQueueSubmit (the air inject) returned " + std::to_string( int( r ) );
		return false;
	}
	return true;
}

const FxField * fieldOf( const FxField * t, int n, const char * name )
{
	for ( int i = 0; i < n; i++ )
		if ( std::strcmp( t[i].name, name ) == 0 )
			return &t[i];
	return nullptr;
}
}  // namespace airinject
}  // namespace

namespace WwVkPresent
{
std::string airInjectRefusal()
{
	airinject::Ai & S = airinject::aiS();
	if ( S.broken )
		return S.brokenWhy;
	if ( !vk().ok || !gl().ok || gl().ctx != reinterpret_cast<void *>( wglGetCurrentContext() ) )
		return "the present device or GL's entry points are not ready in this context";
	return std::string();
}

WaterRun & airInjectScratch()
{
	return airinject::aiS().run;
}

bool airInjectRunState( const AirInjectRun & r, WaterRun & s, std::string & why )
{
	using namespace airinject;
	Ai & S = aiS();
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
		S.brokenWhy = "the air inject on Vulkan broke: " + w;
		return refuse( S.brokenWhy );
	};
	if ( const std::string no = airInjectRefusal(); !no.empty() )
		return refuse( no );
	const int k = r.rt ? 1 : 0;
	const farlod::TwinSpec & P = kSpec[k];
	if ( r.wwDefs != airInjectKey( r.rt ) )  // the twin is one variant of GL's program: any other key stays on GL
		return refuse( "GL's air program has the key \"" + r.wwDefs + "\", the " + ( r.rt ? "RT GI" : "default" ) + " twin's key is \""
			+ airInjectKey( r.rt ) + "\"" );
	const Shared * sh = sharedFind( r.tileKey );
	if ( !sh || !sh->img || !sh->tex || sh->fmt != VK_FORMAT_R32G32_SFLOAT || sh->type != 0 )
		return refuse( "the air tile is not a shared RG32F 2D target" );
	if ( sh->w != r.cols || sh->h != r.rows )
		return refuse( "the air tile is " + std::to_string( sh->w ) + " x " + std::to_string( sh->h ) + ", the run's columns "
			+ std::to_string( r.cols ) + " x " + std::to_string( r.rows ) );
	const GLuint tileTex = sh->tex;
	const WaterSlot * gb = nullptr;
	for ( const WaterSlot & c : s.census.inputs )
		if ( c.name == "volGbuf" )
			gb = &c;
	if ( !gb || !gb->tex )
		return refuse( "volGbuf: nothing bound on its unit" );
	if ( gb->tex == tileTex )
		return refuse( "volGbuf: samples the tile it draws into" );
	if ( gb->baseLevel != 0 )
		return refuse( "volGbuf: a base level above 0" );
	const FxField * tab = r.rt ? kT_airinjectRtPs : kT_airinjectPs;
	const int nTab = r.rt ? int( sizeof( kT_airinjectRtPs ) / sizeof( kT_airinjectRtPs[0] ) ) : int( sizeof( kT_airinjectPs ) / sizeof( kT_airinjectPs[0] ) );
	const FxField * fDist = fieldOf( tab, nTab, "fogDistScale" );
	const FxField * fZ = fieldOf( tab, nTab, "volGbufZ" );
	const FxField * fSize = fieldOf( tab, nTab, "volGbufSize" );
	const FxField * fDims = fieldOf( tab, nTab, "volDims" );
	if ( !fDist || !fZ || !fSize || !fDims || fDist->isInt || !fZ->isInt || !fSize->isInt || !fDims->isInt || fDims->words < 2 )
		return breakOff( "the air inject's table has no fogDistScale / volGbufZ / volGbufSize / volDims of the read types" );
	std::string w;
	if ( !fxStatic( w ) || !aiStatic( w ) || !aiSems( w ) || !farlod::twinStatic( S.D[k], P, w ) || !farlodrun::twinPoolN( S.D[k], w )
		|| !aiPipe( k, w ) )
		return breakOff( w );
	farlod::WaterDraw & D = S.D[k];
	// the last crossing is done before its command buffer, set, slice and framebuffer are written again
	if ( std::string fw; !fenceDone( S.fence, S.pending, "airinject", fw ) )
		return refuse( fw );
	S.pending = false;
	if ( !aiFb( r.tileKey, w ) )
		return breakOff( w );
	// GL's blocks (globalUniforms, wwShadowFrame) into the exported copy; an unbound one reads the slice's zeros
	const WaterUbo * ubos[2] = { &s.global, &s.shadow };
	const VkDeviceSize need[2] = { kGlobalBytes, kShadowBytes };
	const VkDeviceSize uboAt[2] = { 0, 512 };
	bool uboHost[2] = {};
	for ( int b = 0; b < 2; b++ ) {
		const WaterUbo & u = *ubos[b];
		uboHost[b] = !u.buf || u.size <= 0 || u.off < 0 || u.off + u.size > u.bufBytes || ( u.off % 4 );
	}
	if ( !csmBuf( D.geo, 1024, true, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, w, "the air inject's blocks" )
		|| !csmBuf( D.host, kHostBytes, false, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, w, "the air inject's slice" ) )
		return breakOff( w );
	std::uint8_t * h = static_cast<std::uint8_t *>( D.host.ptr );
	std::memset( h + kZeroAt, 0, std::size_t( kLooseAt - kZeroAt ) );
	std::memcpy( h + kLooseAt, s.psLoose, std::size_t( P.psLoose ) );
	std::uint8_t * b31 = h + kLooseAt;
	if ( r.red & 1 ) {
		float x = 0.0f;
		std::memcpy( &x, b31 + fDist->offset, 4 );
		x *= 2.0f;
		std::memcpy( b31 + fDist->offset, &x, 4 );
	}
	if ( r.red & 2 ) {
		std::int32_t x = 0;
		std::memcpy( &x, b31 + fZ->offset, 4 );
		x = x ? 0 : 1;
		std::memcpy( b31 + fZ->offset, &x, 4 );
	}
	if ( r.red & 4 ) {
		std::int32_t x = 0;
		std::memcpy( &x, b31 + fSize->offset, 4 );
		x -= 1;
		std::memcpy( b31 + fSize->offset, &x, 4 );
	}
	if ( r.red & 8 ) {
		std::int32_t x = 0;
		std::memcpy( &x, b31 + fDims->offset + 4, 4 );
		x += 1;
		std::memcpy( b31 + fDims->offset + 4, &x, 4 );
	}
	// the inputs: volGbuf from GL (in place or one copy), every other image a zero view, every texel buffer a zero one
	std::vector<GLuint> texNames { tileTex };
	std::vector<GLuint> bufNames { D.geo.gl };
	const std::size_t nb = D.binds.size();
	std::vector<VkDescriptorImageInfo> ii( nb );
	std::vector<VkBufferView> tv( nb );
	std::vector<VkDescriptorType> ty( nb, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER );
	std::vector<VkDescriptorBufferInfo> bi( nb );
	std::vector<VkWriteDescriptorSet> ws;
	int copies = 0;
	const std::uintptr_t keyBase = reinterpret_cast<std::uintptr_t>( &S.keys[k][0] );
	auto nameOf = [&]( std::uint32_t binding ) -> const char * {
		for ( std::size_t q = 0; q < P.nImg; q++ )
			if ( P.img[q].binding == binding )
				return P.img[q].name;
		return nullptr;
	};
	for ( std::size_t i = 0; i < nb; i++ ) {
		const FarSetSlot & b = D.binds[i];
		if ( b.kind == 0 ) {
			if ( b.binding != 0 && b.binding != 1 && b.binding != 31 )
				return breakOff( "the twin reads a block at b" + std::to_string( b.binding ) + " the air inject does not fill" );
			continue;
		}
		if ( b.kind == 3 ) {
			const bool mine = b.binding - 64 == kGbufBinding;
			WaterSlot plain;
			VkSampler sm = farlod::waterSampler( D, mine ? *gb : plain, b.compare, !mine, w );
			if ( !sm )
				return breakOff( w );
			if ( b.compare && mine )
				return refuse( "volGbuf: the twin compares" );
			ty[i] = VK_DESCRIPTOR_TYPE_SAMPLER;
			ii[i].sampler = sm;
			continue;
		}
		const char * nm = nameOf( b.binding );
		if ( !nm )
			return breakOff( "the twin reads an image at b" + std::to_string( b.binding ) + " no census name maps" );
		const std::uintptr_t key = keyBase + b.binding;
		if ( b.kind == 2 ) {  // a texel buffer stage 0 never reads: a 16-byte zero one of the declared type
			unsigned tb = 0, tt = 0;
			const bool ints = std::strcmp( nm, "cellCluster" ) == 0 || std::strcmp( nm, "volClusters" ) == 0;  // Buffer<int4>: RGBA32I
			if ( !farTexelBuffer( key, ints ? 0x8D82u : 0x8814u, 16u, tb, tt, w ) )
				return refuse( std::string( nm ) + ": " + w );
			bufNames.push_back( tb );
			tv[i] = farCards().bufs[key].view;
			ty[i] = VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
			continue;
		}
		VkImageView view = VK_NULL_HANDLE;
		if ( b.binding != kGbufBinding ) {
			if ( b.dim == 3 && !( b.arrayed && D.zeroCubeView ) )
				return refuse( std::string( nm ) + ": a cube input with no zero cube view" );
			view = b.dim == 3 ? D.zeroCubeView : b.dim == 2 ? F.d3 : b.arrayed ? F.d2a : F.d2;
		} else if ( const std::uintptr_t sk = farSharedOf( gb->tex ) ) {
			view = sharedFind( sk )->view;  // the shared depth: read in place
			texNames.push_back( gb->tex );
		} else {
			unsigned twin = 0;
			if ( !waterTwin( key, *gb, twin, w ) )
				return refuse( std::string( "volGbuf: " ) + w );
			const bool viaFar = gb->kind == 4 || waterBcFormat( gb->glFmt );
			const Shared * th = viaFar ? farTwinFind( key ) : sharedFind( key );
			if ( !th || !th->view )
				return refuse( "volGbuf: its twin has no view" );
			view = th->view;
			static const GLenum kTarget[5] = { 0x0DE1, 0x806F, 0x8C1A, 0x8513, 0x9009 };
			const GLenum target = kTarget[std::clamp( gb->kind, 0, 4 )];
			for ( int l = 0; l < std::max( gb->levels, 1 ); l++ ) {
				const int lw = std::max( gb->w >> l, 1 ), lh = std::max( gb->h >> l, 1 );
				const int ld = gb->kind == 1 ? std::max( gb->d >> l, 1 ) : ( gb->kind == 2 || gb->kind == 4 ) ? gb->d : 1;
				g.copyImageSubData( gb->tex, target, l, 0, 0, 0, twin, target, l, 0, 0, 0, lw, lh, ld );
			}
			copies++;
			texNames.push_back( twin );
		}
		ty[i] = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
		ii[i].imageView = view;
		ii[i].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	}
	for ( std::size_t i = 0; i < nb; i++ ) {
		const FarSetSlot & b = D.binds[i];
		VkWriteDescriptorSet x { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
		x.dstSet = D.setsN[0];
		x.dstBinding = b.binding;
		x.descriptorCount = 1;
		x.descriptorType = ty[i];
		if ( b.kind == 0 ) {
			if ( b.binding == 31 )
				bi[i] = { D.host.buf, kLooseAt, VkDeviceSize( P.psLoose ) };
			else
				bi[i] = uboHost[b.binding] ? VkDescriptorBufferInfo { D.host.buf, kZeroAt, need[b.binding] }
					: VkDescriptorBufferInfo { D.geo.buf, uboAt[b.binding], need[b.binding] };
			x.pBufferInfo = &bi[i];
		} else if ( b.kind == 2 ) {
			x.pTexelBufferView = &tv[i];
		} else {
			x.pImageInfo = &ii[i];
		}
		ws.push_back( x );
	}
	v.vkUpdateDescriptorSets( v.dev, std::uint32_t( ws.size() ), ws.data(), 0, nullptr );
	for ( int b = 0; b < 2; b++ )
		if ( !uboHost[b] )
			g.copyNamedBufferSubData( GLuint( ubos[b]->buf ), D.geo.gl, std::intptr_t( ubos[b]->off ), std::intptr_t( uboAt[b] ),
				std::intptr_t( std::min<long long>( ubos[b]->size, static_cast<long long>( need[b] ) ) ) );
	S.cur = k;
	// GL's work so far signalled (the tile, the input, the copy; each name once) -> the worker's job -> GL waits
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
	j.fn = aiWork;
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
	S.st.copies += copies;
	S.st.cpuMs += ms;
	S.st.maxMs = std::max( S.st.maxMs, ms );
	return true;
}

void airInjectOnGl( const char * why )
{
	airinject::Ai & S = airinject::aiS();
	S.st.gl++;
	S.st.glWhy = why ? why : "";
}

AirInjectStat takeAirInjectStat()
{
	airinject::Ai & S = airinject::aiS();
	AirInjectStat s = S.st;
	if ( S.broken && s.failWhy.empty() )
		s.failWhy = S.brokenWhy;
	const int pipes = S.st.pipelines;
	const double build = S.st.buildMs;
	S.st = AirInjectStat();
	S.st.pipelines = pipes;
	S.st.buildMs = build;
	return s;
}
}  // namespace WwVkPresent
