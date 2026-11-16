/* Lane VKFAR19 (2026-11-15): the reflection probe relight on the present device (vkreflrelight.h). Included once by
 * vkpresent.cpp at its end, after vkwater.inc (the far LOD rings' TwinSpec / WaterDraw / twinStatic / waterSampler and
 * farlodrun::twinPoolN) and vklamp.inc. GL's pass (cellreflprobe.cpp): per todo tile, viewport = the tile, no depth, no
 * blend, no cull, the quad { -1 -1, 1 -1, -1 1, 1 1 }, fragColor into level 0 of the atlas. Here: its own colour-only
 * render pass (B10G11R11, LOAD / STORE, GENERAL) and pipeline per twin, a level-0 view + framebuffer of the shared atlas
 * (made again when a shared target is made or goes: sharedGen), every tile a draw with its own b31 slice and descriptor set
 * (twinRunN's way), up to kTwinBatchMax tiles a crossing (one fence, j.fn on the worker, as lampRun). */
#define WW_VK_REFLRELIGHT 1
#include "features/vkframe/vkreflrelight.h"
#include "features/vkframe/reflrelight_view_vs.spv.h"  // vkframe_spirv.sh's fxt / fxg / fxr lines
#include "features/vkframe/reflrelight_view_ps.spv.h"
#include "features/vkframe/reflrelight_view_rt_ps.spv.h"

namespace
{
namespace reflrelight
{
using namespace WwVkPresent;

// the pixel stages' declared images (t n -> 64 + n, samplers at + 64; the RT twin's set, the base one's is a subset)
const farlod::WaterImgName kImg[] = { { 64, "cellLights" }, { 65, "cellGi" }, { 66, "cellGiSlots" }, { 67, "cellRooms" },
	{ 68, "cellIsLut" }, { 69, "cellIsBloom" }, { 70, "cellShadow" }, { 71, "cellRestir" }, { 72, "cellFarSlots" },
	{ 73, "cellFarRecs" }, { 74, "cellFarPlaced" }, { 77, "cellCtGbuf" }, { 79, "cellAoDecal" }, { 80, "cellSsgi" },
	{ 89, "atApTex" }, { 90, "volTex" }, { 91, "volTile" }, { 92, "volAirTerr" }, { 93, "volAirRainMap" }, { 94, "volAirNoise" },
	{ 95, "csmMap" }, { 96, "ussTerrMap" }, { 97, "ussContactMap" }, { 98, "ussCloudMap" }, { 104, "cellGiHalf" },
	{ 105, "cellGiHalfKey" }, { 108, "cellDdgiAtlas" }, { 109, "cellCluster" }, { 110, "rpAlb" }, { 111, "rpNd" } };
// a tile's host slice: zeros for an unbound block (globalUniforms 336, wwShadowFrame 248 bytes), then its b31
constexpr VkDeviceSize kZeroAt = 0, kLooseAt = 512, kHostBytes = 4864;
constexpr VkDeviceSize kGlobalBytes = 336, kShadowBytes = 248;
static_assert( kLooseAt + VkDeviceSize( kT_reflrelightRtPsBytes ) <= kHostBytes && kLooseAt + VkDeviceSize( kT_reflrelightPsBytes ) <= kHostBytes
		&& kT_reflrelightRtPsBytes <= kTwinPsMax && kGlobalBytes <= kLooseAt && kShadowBytes <= kLooseAt,
	"the relight's host slice" );
// [0] the base key, [1] the RT GI key; the pixel stage's blocks b0 global, b1 shadow, b31 loose; the kind is never fx's
const farlod::TwinSpec kSpec[2] = {
	{ "reflection relight", vkframe_reflrelight_view_vs_spv, sizeof( vkframe_reflrelight_view_vs_spv ), vkframe_reflrelight_view_ps_spv,
		sizeof( vkframe_reflrelight_view_ps_spv ), kImg, sizeof( kImg ) / sizeof( kImg[0] ), nullptr, 0, kT_reflrelightPsBytes, kLooseAt,
		kHostBytes, false, 0, true },
	{ "reflection relight (RT GI)", vkframe_reflrelight_view_vs_spv, sizeof( vkframe_reflrelight_view_vs_spv ),
		vkframe_reflrelight_view_rt_ps_spv, sizeof( vkframe_reflrelight_view_rt_ps_spv ), kImg, sizeof( kImg ) / sizeof( kImg[0] ), nullptr,
		0, kT_reflrelightRtPsBytes, kLooseAt, kHostBytes, false, 0, true } };
// the twins' keys (slot + binding; never an address the app's shared targets use)
char kKeys[2][256];

struct Rr
{
	bool made = false, broken = false;
	std::string brokenWhy;
	farlod::WaterDraw D[2];  // twinStatic's layout / modules / binds, twinPoolN's sets, the exported block copy and the slices
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
	int cur = 0;                  // the crossing's twin
	std::vector<VkRect2D> rects;  // the crossing's tiles (viewport = scissor)
	ReflRelightStat st;
};

Rr & rrS()
{
	static Rr s;
	return s;
}

// the device-time parts (once): the colour-only render pass, the quad, the command buffer and its fence
bool rrStatic( std::string & why )
{
	Vk & v = vk();
	Rr & S = rrS();
	if ( S.made )
		return true;
	VkAttachmentDescription at {};
	at.format = VK_FORMAT_B10G11R11_UFLOAT_PACK32;
	at.samples = VK_SAMPLE_COUNT_1_BIT;
	at.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;  // the held tiles keep GL's (or the last relight's) texels
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
		why = "vkCreateRenderPass (the reflection relight) failed";
		return false;
	}
	if ( !csmBuf( S.quad, 6 * 12, false, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, why, "the reflection relight's quad" ) )
		return false;
	const float q[18] = { -1, -1, 0, 1, -1, 0, -1, 1, 0, -1, 1, 0, 1, -1, 0, 1, 1, 0 };  // GL's indices 0 1 2, 2 1 3
	std::memcpy( S.quad.ptr, q, sizeof( q ) );
	VkFenceCreateInfo fi { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
	VkCommandBufferAllocateInfo cai { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
	cai.commandPool = v.pool;
	cai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	cai.commandBufferCount = 1;
	if ( v.vkCreateFence( v.dev, &fi, nullptr, &S.fence ) != VK_SUCCESS || v.vkAllocateCommandBuffers( v.dev, &cai, &S.cmd ) != VK_SUCCESS ) {
		why = "the reflection relight's fence / command buffer failed";
		return false;
	}
	S.made = true;
	return true;
}

// a twin's pipeline (made at its first run): position at location 0, no cull / depth / blend, viewport and scissor dynamic
bool rrPipe( int k, std::string & why )
{
	Vk & v = vk();
	Rr & S = rrS();
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

bool rrSems( std::string & why )
{
	Vk & v = vk();
	Gl & g = gl();
	Rr & S = rrS();
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

// the level-0 view of the shared atlas and its framebuffer (kept until the key or a shared target changes: sharedGen)
bool rrFb( std::uintptr_t key, std::string & why )
{
	Vk & v = vk();
	Rr & S = rrS();
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
	if ( !sh || !sh->img || sh->fmt != VK_FORMAT_B10G11R11_UFLOAT_PACK32 || sh->type != 0 ) {
		why = "the probe atlas is not a shared B10G11R11 2D target";
		return false;
	}
	VkImageViewCreateInfo ci { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
	ci.image = sh->img;
	ci.viewType = VK_IMAGE_VIEW_TYPE_2D;
	ci.format = VK_FORMAT_B10G11R11_UFLOAT_PACK32;
	ci.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
	if ( v.vkCreateImageView( v.dev, &ci, nullptr, &S.fbView ) != VK_SUCCESS ) {
		why = "the probe atlas' level-0 view failed";
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
		why = "the probe atlas' framebuffer failed";
		return false;
	}
	S.fbKey = key;
	S.fbW = sh->w;
	S.fbH = sh->h;
	S.fbGen = sharedGen();
	return true;
}

// the worker: one render pass, a draw a tile (its set: its b31 slice), GL's signal waited, signalled back, with the fence
bool rrWork( std::string & why )
{
	Vk & v = vk();
	Rr & S = rrS();
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
	for ( std::size_t d = 0; d < S.rects.size(); d++ ) {
		const VkRect2D & e = S.rects[d];
		// no y flip: GL's window rows from the bottom are the image's rows from the top (and gl_FragCoord.y with them)
		const VkViewport vp { float( e.offset.x ), float( e.offset.y ), float( e.extent.width ), float( e.extent.height ), 0.0f, 1.0f };
		v.vkCmdSetViewport( S.cmd, 0, 1, &vp );
		v.vkCmdSetScissor( S.cmd, 0, 1, &e );
		v.vkCmdBindDescriptorSets( S.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, D.layout, 0, 1, &D.setsN[d], 0, nullptr );
		v.vkCmdDraw( S.cmd, 6, 1, 0, 0 );
	}
	v.vkCmdEndRenderPass( S.cmd );
	if ( v.vkEndCommandBuffer( S.cmd ) != VK_SUCCESS ) {
		why = "vkEndCommandBuffer (the reflection relight) failed";
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
		why = "vkQueueSubmit (the reflection relight) returned " + std::to_string( int( r ) );
		return false;
	}
	return true;
}

int fieldAt( const FxField * t, int n, const char * name )
{
	for ( int i = 0; i < n; i++ )
		if ( std::strcmp( t[i].name, name ) == 0 )
			return t[i].offset;
	return -1;
}
}  // namespace reflrelight
}  // namespace

namespace WwVkPresent
{
std::string reflRelightRefusal()
{
	reflrelight::Rr & S = reflrelight::rrS();
	if ( S.broken )
		return S.brokenWhy;
	if ( !vk().ok || !gl().ok || gl().ctx != reinterpret_cast<void *>( wglGetCurrentContext() ) )
		return "the present device or GL's entry points are not ready in this context";
	return std::string();
}

bool reflRelightRunState( const ReflRelightRun & r, WaterRun & s, std::string & why )
{
	using namespace reflrelight;
	Rr & S = rrS();
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
		S.brokenWhy = "the reflection relight on Vulkan broke: " + w;
		return refuse( S.brokenWhy );
	};
	if ( const std::string no = reflRelightRefusal(); !no.empty() )
		return refuse( no );
	const int k = r.rt ? 1 : 0;
	const farlod::TwinSpec & P = kSpec[k];
	if ( r.wwDefs != reflRelightKey( r.rt ) )  // the twin is one variant of GL's program: any other key stays on GL
		return refuse( "GL's relight program has the key \"" + r.wwDefs + "\", the " + ( r.rt ? "RT GI" : "base" ) + " twin's key is \""
			+ reflRelightKey( r.rt ) + "\"" );
	if ( r.tiles.empty() )
		return true;
	const Shared * sh = sharedFind( r.atlasKey );
	if ( !sh || !sh->img || !sh->tex || sh->fmt != VK_FORMAT_B10G11R11_UFLOAT_PACK32 || sh->type != 0 )
		return refuse( "the probe atlas is not a shared B10G11R11 2D target" );
	const GLuint atlasTex = sh->tex;
	// the G-buffer cubes are read in place: never copied (a copy of 64 x 64 x 6 x 33 x 2 cubes a run)
	for ( const char * nm : { "rpAlb", "rpNd" } ) {
		const std::uintptr_t key = std::strcmp( nm, "rpAlb" ) == 0 ? r.albKey : r.ndKey;
		const WaterSlot * hit = nullptr;
		for ( const WaterSlot & c : s.census.inputs )
			if ( c.name == nm )
				hit = &c;
		if ( !hit || !hit->tex || !key || !sharedTargetIs( key, hit->tex ) )
			return refuse( std::string( nm ) + ": not the shared G-buffer cube array" );
	}
	const int nT = int( sizeof( kT_reflrelightPs ) / sizeof( kT_reflrelightPs[0] ) );
	const int nTr = int( sizeof( kT_reflrelightRtPs ) / sizeof( kT_reflrelightRtPs[0] ) );
	const FxField * tab = r.rt ? kT_reflrelightRtPs : kT_reflrelightPs;
	const int nTab = r.rt ? nTr : nT;
	const int atLayer = fieldAt( tab, nTab, "rpLayer" ), atTile = fieldAt( tab, nTab, "rpTile" ), atPos = fieldAt( tab, nTab, "rpPos" );
	if ( atLayer < 0 || atTile < 0 || atPos < 0 )
		return breakOff( "the relight table has no rpLayer / rpTile / rpPos" );
	std::string w;
	if ( !fxStatic( w ) || !rrStatic( w ) || !rrSems( w ) || !farlod::twinStatic( S.D[k], P, w ) || !farlodrun::twinPoolN( S.D[k], w )
		|| !rrPipe( k, w ) )
		return breakOff( w );
	farlod::WaterDraw & D = S.D[k];
	const std::uintptr_t keyBase = reinterpret_cast<std::uintptr_t>( &kKeys[k][0] );
	for ( std::size_t at0 = 0; at0 < r.tiles.size(); at0 += std::size_t( farlod::kTwinBatchMax ) ) {
		const int n = int( std::min( r.tiles.size() - at0, std::size_t( farlod::kTwinBatchMax ) ) );
		// the last crossing is done before its command buffer, sets, slices and framebuffer are written again
		if ( std::string fw; !fenceDone( S.fence, S.pending, "reflrelight", fw ) )
			return refuse( fw );
		S.pending = false;
		if ( !rrFb( r.atlasKey, w ) )
			return breakOff( w );
		// GL's blocks (globalUniforms, wwShadowFrame) into the exported copy; an unbound one reads the slice's zeros
		const WaterUbo * ubos[2] = { &s.global, &s.shadow };
		const VkDeviceSize need[2] = { kGlobalBytes, kShadowBytes };
		VkDeviceSize uboAt[2] = { 0, 256 };
		bool uboHost[2] = {};
		for ( int b = 0; b < 2; b++ ) {
			const WaterUbo & u = *ubos[b];
			uboHost[b] = !u.buf || u.size <= 0 || u.off < 0 || u.off + u.size > u.bufBytes || ( u.off % 4 );
		}
		if ( !csmBuf( D.geo, 1024, true, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, w, "the reflection relight's blocks" )
			|| !csmBuf( D.host, kHostBytes * VkDeviceSize( n ), false, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, w, "the reflection relight's slices" ) )
			return breakOff( w );
		std::uint8_t * host = static_cast<std::uint8_t *>( D.host.ptr );
		S.rects.clear();
		for ( int d = 0; d < n; d++ ) {
			const ReflRelightTile & t = r.tiles[at0 + std::size_t( d )];
			std::uint8_t * h = host + kHostBytes * VkDeviceSize( d );
			std::memset( h + kZeroAt, 0, std::size_t( kLooseAt - kZeroAt ) );
			std::memcpy( h + kLooseAt, s.psLoose, std::size_t( P.psLoose ) );
			const float layer = float( t.layer );
			std::memcpy( h + kLooseAt + atLayer, &layer, 4 );
			std::memcpy( h + kLooseAt + atTile, t.tile, 12 );
			std::memcpy( h + kLooseAt + atPos, t.pos, 12 );
			// GL's viewport: the tile (origin, size), clipped to the atlas
			const int x0 = std::clamp( int( t.tile[0] ), 0, S.fbW ), y0 = std::clamp( int( t.tile[1] ), 0, S.fbH );
			const int x1 = std::clamp( int( t.tile[0] + t.tile[2] ), 0, S.fbW ), y1 = std::clamp( int( t.tile[1] + t.tile[2] ), 0, S.fbH );
			if ( x1 <= x0 || y1 <= y0 )
				return refuse( "tile " + std::to_string( at0 + std::size_t( d ) ) + " lies outside the atlas" );
			S.rects.push_back( { { x0, y0 }, { std::uint32_t( x1 - x0 ), std::uint32_t( y1 - y0 ) } } );
		}
		// the inputs (the same GL names for every tile): written once, then every tile's set
		std::vector<GLuint> texNames { atlasTex };
		std::vector<GLuint> bufNames { D.geo.gl };
		const std::size_t nb = D.binds.size();
		std::vector<VkDescriptorImageInfo> ii( nb );
		std::vector<VkBufferView> tv( nb );
		std::vector<VkDescriptorType> ty( nb, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER );
		std::vector<VkDescriptorBufferInfo> bi( nb * std::size_t( n ) );
		std::vector<VkWriteDescriptorSet> ws;
		int copies = 0;
		auto slotOf = [&]( std::uint32_t binding, const char ** nameOut ) -> const WaterSlot * {
			for ( std::size_t q = 0; q < P.nImg; q++ )
				if ( const farlod::WaterImgName & nm = P.img[q]; nm.binding == binding ) {
					*nameOut = nm.name;
					for ( const WaterSlot & c : s.census.inputs )
						if ( c.name == nm.name )
							return &c;
					return nullptr;
				}
			*nameOut = nullptr;
			return nullptr;
		};
		for ( std::size_t i = 0; i < nb; i++ ) {
			const FarSetSlot & b = D.binds[i];
			if ( b.kind == 0 ) {
				if ( b.binding != 0 && b.binding != 1 && b.binding != 31 )
					return breakOff( "the twin reads a block at b" + std::to_string( b.binding ) + " the relight does not fill" );
				continue;
			}
			if ( b.kind == 3 ) {
				const char * nm = nullptr;
				const WaterSlot * c = slotOf( b.binding - 64, &nm );
				WaterSlot plain;
				VkSampler sm = farlod::waterSampler( D, c ? *c : plain, b.compare, !c, w );
				if ( !sm )
					return breakOff( w );
				if ( b.compare && c && c->tex && c->cmpMode != 0x884E )
					return refuse( std::string( nm ? nm : "?" ) + ": the twin compares, GL's texture does not" );
				ty[i] = VK_DESCRIPTOR_TYPE_SAMPLER;
				ii[i].sampler = sm;
				continue;
			}
			const char * nm = nullptr;
			const WaterSlot * c = slotOf( b.binding, &nm );
			if ( !nm )
				return breakOff( "the twin reads an image at b" + std::to_string( b.binding ) + " no census name maps" );
			const std::uintptr_t key = keyBase + b.binding;
			if ( b.kind == 2 ) {  // a texel buffer: GL's data store into the twin, or a zero one
				const bool bound = c && c->tex && c->dataBuf && c->bufBytes > 0;
				unsigned tb = 0, tt = 0;
				const unsigned zeroFmt = std::strcmp( nm, "cellCluster" ) == 0 ? 0x8D82u : 0x8814u;  // Buffer<int4>: RGBA32I
				if ( !farTexelBuffer( key, bound ? c->glFmt : zeroFmt, bound ? std::size_t( c->bufBytes ) : 16u, tb, tt, w ) )
					return refuse( std::string( nm ) + ": " + w );
				if ( bound )
					g.copyNamedBufferSubData( c->dataBuf, tb, 0, 0, std::intptr_t( c->bufBytes ) );
				bufNames.push_back( tb );
				tv[i] = farCards().bufs[key].view;
				ty[i] = VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
				continue;
			}
			VkImageView view = VK_NULL_HANDLE;
			if ( !c || !c->tex ) {
				if ( b.dim == 3 && !( b.arrayed && D.zeroCubeView ) )
					return refuse( std::string( nm ) + ": an unbound cube input (no zero cube view)" );
				view = b.dim == 3 ? D.zeroCubeView : b.dim == 2 ? F.d3 : b.arrayed ? F.d2a : F.d2;
			} else if ( c->tex == atlasTex ) {
				return refuse( std::string( nm ) + ": samples the atlas it draws into" );
			} else if ( c->baseLevel != 0 ) {
				return refuse( std::string( nm ) + ": a base level above 0" );
			} else if ( const std::uintptr_t sk = farSharedOf( c->tex ) ) {
				view = sharedFind( sk )->view;  // already on the present device (rpAlb / rpNd, another pass's target)
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
		for ( int d = 0; d < n; d++ ) {
			const VkDeviceSize hb = kHostBytes * VkDeviceSize( d );
			for ( std::size_t i = 0; i < nb; i++ ) {
				const FarSetSlot & b = D.binds[i];
				VkWriteDescriptorSet x { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
				x.dstSet = D.setsN[std::size_t( d )];
				x.dstBinding = b.binding;
				x.descriptorCount = 1;
				x.descriptorType = ty[i];
				if ( b.kind == 0 ) {
					VkDescriptorBufferInfo & o = bi[std::size_t( d ) * nb + i];
					if ( b.binding == 31 )
						o = { D.host.buf, hb + kLooseAt, VkDeviceSize( P.psLoose ) };
					else
						o = uboHost[b.binding] ? VkDescriptorBufferInfo { D.host.buf, hb + kZeroAt, need[b.binding] }
							: VkDescriptorBufferInfo { D.geo.buf, uboAt[b.binding], need[b.binding] };
					x.pBufferInfo = &o;
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
		// GL's work so far signalled (the atlas, the inputs, the copies; each name once) -> the worker's job -> GL waits
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
		j.fn = rrWork;
		hand( j );
		bool ok = false;
		{
			std::unique_lock<std::mutex> lk( W.m );
			W.idle.wait( lk, [&W, want]() { return W.toneDone >= want; } );
			ok = W.toneOk;
			w = W.toneWhy;
		}
		if ( !ok )
			return breakOff( w );  // nothing signals vkDone: GL does not wait it; GL draws the tiles
		g.waitSemaphore( S.vkDoneGl, GLuint( bufNames.size() ), bufNames.data(), GLuint( texNames.size() ), texNames.data(), lay.data() );
		S.pending = true;
		S.st.crossings++;
		S.st.copies += copies;
	}
	const double ms = nowMs() - t0;
	S.st.runs++;
	S.st.tiles += int( r.tiles.size() );
	S.st.cpuMs += ms;
	S.st.maxMs = std::max( S.st.maxMs, ms );
	return true;
}

void reflRelightOnGl( const char * why )
{
	WwVkPresent::glGuard( "reflRelight", 1 );  // lane VKFLIGHT4: the view GL guard
	reflrelight::Rr & S = reflrelight::rrS();
	S.st.gl++;
	S.st.glWhy = why ? why : "";
}

ReflRelightStat takeReflRelightStat()
{
	reflrelight::Rr & S = reflrelight::rrS();
	ReflRelightStat s = S.st;
	if ( S.broken && s.failWhy.empty() )
		s.failWhy = S.brokenWhy;
	const int pipes = S.st.pipelines;
	const double build = S.st.buildMs;
	S.st = ReflRelightStat();
	S.st.pipelines = pipes;
	S.st.buildMs = build;
	return s;
}
}  // namespace WwVkPresent
