/* Lane VKGROUND7 (2026-11-16): the far ground's cached-page paint on the present device (vkfgpaint.h). Included once by
 * vkpresent.cpp at its end, after vkairinject.inc (whose shape this copies). GL's paint (cellfarground.cpp paintJobs): the page
 * (cell_fgpage) into an RGBA8 scratch at viewport side, glGenerateMipmap, then per level the BC1 encoder (cell_fgbc1) into an
 * RG32UI block target and the blocks into one layer of the BC1 page pool. Here all of it is one crossing (Work k = -1): the
 * page draw into the route's own shared scratch (one per side), the mips by linear blits, per level an encode draw into the
 * route's shared block target and a vkCmdCopyImage of the blocks into the pool's far twin (one RG32UI texel = one 8-byte BC1
 * block). The page's inputs are read as twinRunN reads them: shared or far twin in place, else one copy a run. */
#define WW_VK_FGPAINT 1
#include "features/vkframe/vkfgpaint.h"
#include "features/vkframe/fgpage_view_vs.spv.h"  // vkframe_spirv.sh's fxt / fxg lines
#include "features/vkframe/fgpage_view_ps.spv.h"
#include "features/vkframe/fgbc1_view_ps.spv.h"

namespace
{
namespace fgpaint
{
using namespace WwVkPresent;

// the page's declared images (t n -> 64 + n, samplers at + 64); the encoder's one source
const farlod::WaterImgName kPageImg[] = { { 81, "fgGrid" }, { 82, "fgLtex" }, { 83, "fgDecal" }, { 84, "fgAtlas" }, { 89, "fgHeight" } };
const farlod::WaterImgName kEncImg[] = { { 64, "fgSrc" } };
constexpr int kMaxLevels = 12;  // a 8192 side; the app's largest is 1024 (11 levels)
// the host slices: the page's b31 at 0, level L's encoder b31 at (L + 1) x the stride (a uniform offset's alignment)
constexpr VkDeviceSize kSliceStride = 256, kHostBytes = kSliceStride * VkDeviceSize( kMaxLevels + 1 );
static_assert( kT_fgpagePsBytes <= int( kSliceStride ) && kT_fgbc1PsBytes <= int( kSliceStride ), "the far ground paint's host slices" );
const farlod::TwinSpec kSpec[2] = {
	{ "far ground page", vkframe_fgpage_view_vs_spv, sizeof( vkframe_fgpage_view_vs_spv ), vkframe_fgpage_view_ps_spv,
		sizeof( vkframe_fgpage_view_ps_spv ), kPageImg, sizeof( kPageImg ) / sizeof( kPageImg[0] ), nullptr, 0, kT_fgpagePsBytes, 0,
		kSliceStride, false, 0 },
	{ "far ground BC1 encode", vkframe_fgpage_view_vs_spv, sizeof( vkframe_fgpage_view_vs_spv ), vkframe_fgbc1_view_ps_spv,
		sizeof( vkframe_fgbc1_view_ps_spv ), kEncImg, 1, nullptr, 0, kT_fgbc1PsBytes, 0, kSliceStride, false, 0 } };
const VkFormat kPassFmt[2] = { VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_R32G32_UINT };
constexpr unsigned kFpGlRgba8 = 0x8058, kFpGlRg32ui = 0x823C;

// a level-0 view of a shared target and its framebuffer, made again when a shared target is made or goes (sharedGen)
struct TargetFb
{
	VkImage img = VK_NULL_HANDLE;
	VkImageView view = VK_NULL_HANDLE;
	VkFramebuffer fb = VK_NULL_HANDLE;
	std::uint64_t gen = 0;
	int side = 0;
};

struct Fp
{
	bool made = false, broken = false;
	std::string brokenWhy;
	farlod::WaterDraw D[2];  // [0] the page, [1] the encoder: twinStatic's layout / modules / binds, twinPoolN's sets
	VkRenderPass pass[2] = { VK_NULL_HANDLE, VK_NULL_HANDLE };
	VkPipeline pipe[2] = { VK_NULL_HANDLE, VK_NULL_HANDLE };
	VkSemaphore glDone = VK_NULL_HANDLE, vkDone = VK_NULL_HANDLE;
	GLuint glDoneGl = 0, vkDoneGl = 0;
	void * semCtx = nullptr;
	VkCommandBuffer cmd = VK_NULL_HANDLE;
	VkFence fence = VK_NULL_HANDLE;
	bool pending = false;
	CsmBuf quad, host;  // six vec3 corners (GL's quad as two triangles); the b31 slices
	std::map<int, TargetFb> pageFb;  // by side
	TargetFb blkFb;
	int blkSide = 0;  // the block target's side: the largest side / 4 asked (re-made when a bigger one is)
	char scrKeys[16] = {};  // the scratch per side (4 << n), the blocks, the inputs' twins (+ binding): never an app address
	char blkKey[1] = {};
	char inKeys[256] = {};
	// the crossing's record (written before the hand)
	struct Rec
	{
		VkImage scr = VK_NULL_HANDLE, blk = VK_NULL_HANDLE, pool = VK_NULL_HANDLE;
		VkFramebuffer pageFb = VK_NULL_HANDLE, blkFb = VK_NULL_HANDLE;
		int side = 0, levels = 0, layer = 0, red = 0;
		bool encodeOnly = false;
	} rec;
	WaterRun run;  // GL's side of a run (fgPaintRun)
	FgPaintStat st;
};

Fp & fpS()
{
	static Fp s;
	return s;
}

int fpLevels( int side )  // as FgSched::levels: down to 4 x 4 (one block)
{
	int n = 0;
	for ( int s = side; s >= 4; s >>= 1 )
		n++;
	return n;
}

std::uintptr_t fpScrKey( int side )
{
	int n = 0;
	while ( ( 4 << n ) < side && n < 15 )
		n++;
	return reinterpret_cast<std::uintptr_t>( &fpS().scrKeys[n] );
}

const FxField * fpField( const FxField * t, int n, const char * name )
{
	for ( int i = 0; i < n; i++ )
		if ( std::strcmp( t[i].name, name ) == 0 )
			return &t[i];
	return nullptr;
}

// a colour-only render pass of fmt (LOAD / STORE, GENERAL); the crossing's barriers order everything around it
bool fpPass( VkFormat fmt, VkRenderPass & pass, std::string & why )
{
	Vk & v = vk();
	VkAttachmentDescription at {};
	at.format = fmt;
	at.samples = VK_SAMPLE_COUNT_1_BIT;
	at.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;  // the quad covers the render area; a miss keeps the old texel, never garbage
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
	if ( v.vkCreateRenderPass( v.dev, &rci, nullptr, &pass ) != VK_SUCCESS ) {
		why = "vkCreateRenderPass (the far ground paint) failed";
		return false;
	}
	return true;
}

// the device-time parts (once): both render passes, the quad, the command buffer and its fence
bool fpStatic( std::string & why )
{
	Vk & v = vk();
	Fp & S = fpS();
	if ( S.made )
		return true;
	for ( int k = 0; k < 2; k++ )
		if ( !S.pass[k] && !fpPass( kPassFmt[k], S.pass[k], why ) )
			return false;
	if ( !csmBuf( S.quad, 6 * 12, false, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, why, "the far ground paint's quad" ) )
		return false;
	const float q[18] = { -1, -1, 0, 1, -1, 0, -1, 1, 0, -1, 1, 0, 1, -1, 0, 1, 1, 0 };  // GL's indices 0 1 2, 2 1 3
	std::memcpy( S.quad.ptr, q, sizeof( q ) );
	VkFenceCreateInfo fi { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
	VkCommandBufferAllocateInfo cai { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
	cai.commandPool = v.pool;
	cai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	cai.commandBufferCount = 1;
	if ( v.vkCreateFence( v.dev, &fi, nullptr, &S.fence ) != VK_SUCCESS || v.vkAllocateCommandBuffers( v.dev, &cai, &S.cmd ) != VK_SUCCESS ) {
		why = "the far ground paint's fence / command buffer failed";
		return false;
	}
	S.made = true;
	return true;
}

// a pipeline (made at the first run): position at location 0, no cull / depth / blend, viewport and scissor dynamic
bool fpPipe( int k, std::string & why )
{
	Vk & v = vk();
	Fp & S = fpS();
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
	gp.renderPass = S.pass[k];
	if ( v.vkCreateGraphicsPipelines( v.dev, tone().cache, 1, &gp, nullptr, &S.pipe[k] ) != VK_SUCCESS ) {
		why = std::string( "vkCreateGraphicsPipelines (the " ) + kSpec[k].what + ") failed";
		return false;
	}
	S.st.pipelines++;
	S.st.buildMs += nowMs() - t0;
	toneCacheSave();
	return true;
}

bool fpSems( std::string & why )
{
	Vk & v = vk();
	Gl & g = gl();
	Fp & S = fpS();
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

void fpFbFree( TargetFb & f )
{
	Vk & v = vk();
	if ( f.fb )
		v.vkDestroyFramebuffer( v.dev, f.fb, nullptr );
	if ( f.view )
		v.vkDestroyImageView( v.dev, f.view, nullptr );
	f = TargetFb();
}

// the level-0 view of a shared target and its framebuffer over side x side (kept while the image and sharedGen hold)
bool fpFb( TargetFb & f, const Shared & sh, VkFormat fmt, VkRenderPass pass, std::string & why )
{
	Vk & v = vk();
	if ( f.fb && f.img == sh.img && f.gen == sharedGen() && f.side == sh.w )
		return true;
	fpFbFree( f );
	VkImageViewCreateInfo ci { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
	ci.image = sh.img;
	ci.viewType = VK_IMAGE_VIEW_TYPE_2D;
	ci.format = fmt;
	ci.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
	if ( v.vkCreateImageView( v.dev, &ci, nullptr, &f.view ) != VK_SUCCESS ) {
		why = "the far ground paint's level-0 view failed";
		return false;
	}
	VkFramebufferCreateInfo fci { VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
	fci.renderPass = pass;
	fci.attachmentCount = 1;
	fci.pAttachments = &f.view;
	fci.width = std::uint32_t( sh.w );
	fci.height = std::uint32_t( sh.h );
	fci.layers = 1;
	if ( v.vkCreateFramebuffer( v.dev, &fci, nullptr, &f.fb ) != VK_SUCCESS ) {
		fpFbFree( f );
		why = "the far ground paint's framebuffer failed";
		return false;
	}
	f.img = sh.img;
	f.gen = sharedGen();
	f.side = sh.w;
	return true;
}

/* the worker: page draw, mips, per level encode + copy into the pool; one memory barrier between every step (each step
 * reads or overwrites what the last one wrote), GL's signal waited, signalled back, the fence */
bool fpWork( std::string & why )
{
	Vk & v = vk();
	Fp & S = fpS();
	const Fp::Rec & R = S.rec;
	v.vkResetFences( v.dev, 1, &S.fence );
	v.vkResetCommandBuffer( S.cmd, 0 );
	VkCommandBufferBeginInfo bi { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
	bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	v.vkBeginCommandBuffer( S.cmd, &bi );
	auto barrier = [&]() {
		VkMemoryBarrier mb { VK_STRUCTURE_TYPE_MEMORY_BARRIER };
		mb.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;
		mb.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
		v.vkCmdPipelineBarrier( S.cmd, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 1, &mb, 0, nullptr, 0,
			nullptr );
	};
	auto draw = [&]( int k, VkFramebuffer fb, int size, VkDescriptorSet set ) {
		VkRenderPassBeginInfo rbi { VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
		rbi.renderPass = S.pass[k];
		rbi.framebuffer = fb;
		rbi.renderArea = { { 0, 0 }, { std::uint32_t( size ), std::uint32_t( size ) } };
		v.vkCmdBeginRenderPass( S.cmd, &rbi, VK_SUBPASS_CONTENTS_INLINE );
		v.vkCmdBindPipeline( S.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, S.pipe[k] );
		const VkDeviceSize zero = 0;
		v.vkCmdBindVertexBuffers( S.cmd, 0, 1, &S.quad.buf, &zero );
		// no y flip: GL's window rows from the bottom are the image's rows from the top (and gl_FragCoord.y with them)
		const VkViewport vp { 0.0f, 0.0f, float( size ), float( size ), 0.0f, 1.0f };
		const VkRect2D sc { { 0, 0 }, { std::uint32_t( size ), std::uint32_t( size ) } };
		v.vkCmdSetViewport( S.cmd, 0, 1, &vp );
		v.vkCmdSetScissor( S.cmd, 0, 1, &sc );
		v.vkCmdBindDescriptorSets( S.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, S.D[k].layout, 0, 1, &set, 0, nullptr );
		v.vkCmdDraw( S.cmd, 6, 1, 0, 0 );
		v.vkCmdEndRenderPass( S.cmd );
	};
	barrier();  // GL's writes (the semaphore) and the last crossing's, before anything here
	if ( !R.encodeOnly ) {
		draw( 0, R.pageFb, R.side, S.D[0].setsN[0] );
		for ( int L = 1; L < R.levels; L++ ) {  // glGenerateMipmap: each level a 2:1 linear blit of the one above
			barrier();
			const std::int32_t a = std::max( R.side >> ( L - 1 ), 1 ), b = std::max( R.side >> L, 1 );
			VkImageBlit bl {};
			bl.srcSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, std::uint32_t( L - 1 ), 0, 1 };
			bl.srcOffsets[1] = { a, a, 1 };
			bl.dstSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, std::uint32_t( L ), 0, 1 };
			bl.dstOffsets[1] = { b, b, 1 };
			v.vkCmdBlitImage( S.cmd, R.scr, VK_IMAGE_LAYOUT_GENERAL, R.scr, VK_IMAGE_LAYOUT_GENERAL, 1, &bl, VK_FILTER_LINEAR );
		}
		barrier();
	}
	if ( R.red & 4 ) {  // red 4: the encoder skipped, the blocks zero
		const VkClearColorValue z {};
		const VkImageSubresourceRange rg { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
		v.vkCmdClearColorImage( S.cmd, R.blk, VK_IMAGE_LAYOUT_GENERAL, &z, 1, &rg );
		barrier();
	}
	for ( int L = 0; L < R.levels; L++ ) {
		const int bw = std::max( ( R.side >> L ) / 4, 1 );
		if ( !( R.red & 4 ) ) {
			draw( 1, R.blkFb, bw, S.D[1].setsN[static_cast<size_t>( L )] );
			barrier();
		}
		if ( !( R.red & 2 ) ) {  // red 2: the blocks never reach the pool
			// uncompressed -> compressed: the extent is in the SOURCE's texels (one RG32UI texel = one 8-byte BC1 block)
			VkImageCopy c {};
			c.srcSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
			c.dstSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, std::uint32_t( L ), std::uint32_t( R.layer ), 1 };
			c.extent = { std::uint32_t( bw ), std::uint32_t( bw ), 1 };
			v.vkCmdCopyImage( S.cmd, R.blk, VK_IMAGE_LAYOUT_GENERAL, R.pool, VK_IMAGE_LAYOUT_GENERAL, 1, &c );
			barrier();  // the next level's encode overwrites the blocks this copy reads
		}
	}
	if ( v.vkEndCommandBuffer( S.cmd ) != VK_SUCCESS ) {
		why = "vkEndCommandBuffer (the far ground paint) failed";
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
		why = "vkQueueSubmit (the far ground paint) returned " + std::to_string( int( r ) );
		return false;
	}
	return true;
}
}  // namespace fgpaint

// a present shutdown: the views and framebuffers go before the shared targets they view (the targets are made again)
void fgPaintDrop()
{
	using namespace fgpaint;
	Fp & S = fpS();
	bool any = S.blkFb.fb != VK_NULL_HANDLE;
	for ( auto & f : S.pageFb )
		any = any || f.second.fb;
	if ( !any )
		return;
	vk().vkDeviceWaitIdle( vk().dev );
	S.pending = false;
	for ( auto & f : S.pageFb )
		fpFbFree( f.second );
	S.pageFb.clear();
	fpFbFree( S.blkFb );
	S.blkSide = 0;
}
}  // namespace

namespace WwVkPresent
{
std::string fgPaintRefusal()
{
	fgpaint::Fp & S = fgpaint::fpS();
	if ( S.broken )
		return S.brokenWhy;
	if ( !vk().ok || !gl().ok || gl().ctx != reinterpret_cast<void *>( wglGetCurrentContext() ) )
		return "the present device or GL's entry points are not ready in this context";
	return std::string();
}

WaterRun & fgPaintScratch()
{
	return fgpaint::fpS().run;
}

unsigned fgPaintPageTex( int side )
{
	const std::uintptr_t key = fgpaint::fpScrKey( side );
	const Shared * sh = sharedFind( key );
	return sh && sh->w == side && sharedTargetIs( key, sh->tex ) ? sh->tex : 0u;
}

bool fgPaintRunState( const FgPaintRun & r, WaterRun & s, std::string & why )
{
	using namespace fgpaint;
	Fp & S = fpS();
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
		S.brokenWhy = "the far ground paint on Vulkan broke: " + w;
		return refuse( S.brokenWhy );
	};
	if ( const std::string no = fgPaintRefusal(); !no.empty() )
		return refuse( no );
	if ( !r.wwDefs.empty() )  // the twin is GL's one variant (no loader defines): any other key stays on GL
		return refuse( "GL's page program has the key \"" + r.wwDefs + "\", the twin's key is \"\"" );
	const int levels = fpLevels( r.side );
	if ( r.side < 4 || ( r.side & ( r.side - 1 ) ) || levels > kMaxLevels )
		return refuse( "a page side of " + std::to_string( r.side ) + " (a power of two, 4 .. " + std::to_string( 2 << kMaxLevels ) + ")" );
	const Shared * pool = farTwinFind( r.poolKey );
	if ( !pool || !pool->img || !pool->tex )
		return refuse( "the pool key is not a far twin" );
	if ( pool->fmt != VK_FORMAT_BC1_RGB_UNORM_BLOCK && pool->fmt != VK_FORMAT_BC1_RGBA_UNORM_BLOCK && pool->fmt != VK_FORMAT_BC1_RGB_SRGB_BLOCK
		&& pool->fmt != VK_FORMAT_BC1_RGBA_SRGB_BLOCK )
		return refuse( "the pool is not BC1" );
	if ( pool->type != 2 || pool->w != r.side || pool->h != r.side )
		return refuse( "the pool is not a " + std::to_string( r.side ) + " x " + std::to_string( r.side ) + " 2D array" );
	if ( pool->levels < levels )
		return refuse( "the pool has " + std::to_string( pool->levels ) + " levels, the page " + std::to_string( levels ) );
	const int layer = ( r.red & 1 ) ? r.layer + 1 : r.layer;  // red 1: the blocks into the next layer
	if ( layer < 0 || layer >= pool->d )
		return refuse( "layer " + std::to_string( layer ) + " is outside the pool's " + std::to_string( pool->d ) + " layers" );
	const FxField * fLevel = fpField( kT_fgbc1Ps, int( sizeof( kT_fgbc1Ps ) / sizeof( kT_fgbc1Ps[0] ) ), "fgSrcLevel" );
	const FxField * fRed = fpField( kT_fgbc1Ps, int( sizeof( kT_fgbc1Ps ) / sizeof( kT_fgbc1Ps[0] ) ), "fgBc1Red" );
	if ( !fLevel || !fRed || !fLevel->isInt || !fRed->isInt )
		return breakOff( "the encoder's table has no fgSrcLevel / fgBc1Red of the read types" );
	std::string w;
	if ( !fxStatic( w ) || !fpStatic( w ) || !fpSems( w ) || !farlod::twinStatic( S.D[0], kSpec[0], w ) || !farlod::twinStatic( S.D[1], kSpec[1], w )
		|| !farlodrun::twinPoolN( S.D[0], w ) || !farlodrun::twinPoolN( S.D[1], w ) || !fpPipe( 0, w ) || !fpPipe( 1, w ) )
		return breakOff( w );
	// the last crossing is done before its command buffer, sets, slices and framebuffers are written again
	if ( std::string fw; !fenceDone( S.fence, S.pending, "fgpaint", fw ) )
		return refuse( fw );
	S.pending = false;
	// the route's targets first (a made one moves sharedGen, and every view below keys on it)
	const std::uintptr_t scrKey = fpScrKey( r.side ), blkKey = reinterpret_cast<std::uintptr_t>( &S.blkKey[0] );
	unsigned scrTex = 0, blkTex = 0;
	if ( !sharedTarget( scrKey, 0, r.side, r.side, 1, kFpGlRgba8, scrTex, w, levels ) )
		return refuse( "the page scratch: " + w );
	S.blkSide = std::max( S.blkSide, std::max( r.side / 4, 1 ) );
	if ( !sharedTarget( blkKey, 0, S.blkSide, S.blkSide, 1, kFpGlRg32ui, blkTex, w, 1 ) )
		return refuse( "the block target: " + w );
	const Shared * scr = sharedFind( scrKey );
	const Shared * blk = sharedFind( blkKey );
	if ( !scr || !scr->img || !scr->view || scr->fmt != kPassFmt[0] || scr->levels != levels || !blk || !blk->img || blk->fmt != kPassFmt[1] )
		return breakOff( "the page scratch / block target are not RGBA8 / RG32UI shared targets" );
	TargetFb & pfb = S.pageFb[r.side];
	if ( !fpFb( pfb, *scr, kPassFmt[0], S.pass[0], w ) || !fpFb( S.blkFb, *blk, kPassFmt[1], S.pass[1], w ) )
		return breakOff( w );
	if ( !csmBuf( S.host, kHostBytes, false, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, w, "the far ground paint's slices" ) )
		return breakOff( w );
	std::uint8_t * h = static_cast<std::uint8_t *>( S.host.ptr );
	std::memset( h, 0, static_cast<size_t>( kHostBytes ) );
	std::memcpy( h, s.psLoose, static_cast<size_t>( kT_fgpagePsBytes ) );
	for ( int L = 0; L < levels; L++ ) {
		const std::int32_t lv = L, red = 0;
		std::uint8_t * b31 = h + kSliceStride * VkDeviceSize( L + 1 );
		std::memcpy( b31 + fLevel->offset, &lv, 4 );
		std::memcpy( b31 + fRed->offset, &red, 4 );
	}
	std::vector<GLuint> texNames { scrTex, blkTex, GLuint( pool->tex ) };
	std::vector<GLuint> bufNames;
	std::vector<VkWriteDescriptorSet> ws;
	int copies = 0;
	// the page's inputs (not on an encode-only run: its scratch is the caller's)
	const farlod::WaterDraw & D0 = S.D[0];
	const std::size_t nb = D0.binds.size();
	std::vector<VkDescriptorImageInfo> ii( nb );
	std::vector<VkBufferView> tv( nb );
	std::vector<VkDescriptorType> ty( nb, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER );
	std::vector<VkDescriptorBufferInfo> bi( nb );
	if ( !r.encodeOnly ) {
		const std::uintptr_t keyBase = reinterpret_cast<std::uintptr_t>( &S.inKeys[0] );
		auto slotOf = [&]( std::uint32_t binding, const char ** nm ) -> const WaterSlot * {
			*nm = nullptr;
			for ( const farlod::WaterImgName & q : kPageImg )
				if ( q.binding == binding )
					*nm = q.name;
			if ( !*nm )
				return nullptr;
			for ( const WaterSlot & c : s.census.inputs )
				if ( c.name == *nm )
					return &c;
			return nullptr;
		};
		for ( std::size_t i = 0; i < nb; i++ ) {
			const FarSetSlot & b = D0.binds[i];
			if ( b.kind == 0 ) {
				if ( b.binding != 31 )
					return breakOff( "the page twin reads a block at b" + std::to_string( b.binding ) + " the paint does not fill" );
				bi[i] = { S.host.buf, 0, VkDeviceSize( kT_fgpagePsBytes ) };
				continue;
			}
			if ( b.kind == 3 ) {
				const char * nm = nullptr;
				const WaterSlot * sl = slotOf( b.binding - 64, &nm );
				WaterSlot plain;
				VkSampler sm = farlod::waterSampler( S.D[0], sl ? *sl : plain, b.compare, !sl || !sl->tex, w );
				if ( !sm )
					return breakOff( w );
				if ( b.compare )
					return refuse( std::string( nm ? nm : "?" ) + ": the page twin compares" );
				ty[i] = VK_DESCRIPTOR_TYPE_SAMPLER;
				ii[i].sampler = sm;
				continue;
			}
			const char * nm = nullptr;
			const WaterSlot * sl = slotOf( b.binding, &nm );
			if ( !nm )
				return breakOff( "the page twin reads an image at b" + std::to_string( b.binding ) + " no census name maps" );
			const std::uintptr_t key = keyBase + b.binding;
			if ( b.kind == 2 ) {  // the decal records: GL's data store into the twin, or a zero one
				const bool bound = sl && sl->tex && sl->dataBuf && sl->bufBytes > 0;
				unsigned tb = 0, tt = 0;
				if ( !farTexelBuffer( key, bound ? sl->glFmt : 0x8814u, bound ? static_cast<size_t>( sl->bufBytes ) : 16u, tb, tt, w ) )
					return refuse( std::string( nm ) + ": " + w );
				if ( bound )
					g.copyNamedBufferSubData( sl->dataBuf, tb, 0, 0, std::intptr_t( sl->bufBytes ) );
				bufNames.push_back( tb );
				tv[i] = farCards().bufs[key].view;
				ty[i] = VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
				continue;
			}
			VkImageView view = VK_NULL_HANDLE;
			if ( !sl || !sl->tex ) {
				if ( std::strcmp( nm, "fgGrid" ) == 0 || b.dim == 3 )
					return refuse( std::string( nm ) + ": nothing bound on its unit" );
				view = b.dim == 2 ? F.d3 : b.arrayed ? F.d2a : F.d2;
			} else if ( sl->tex == scrTex ) {
				return refuse( std::string( nm ) + ": samples the page it draws into" );
			} else if ( sl->baseLevel != 0 ) {
				return refuse( std::string( nm ) + ": a base level above 0" );
			} else if ( const std::uintptr_t sk = farSharedOf( sl->tex ) ) {
				view = sharedFind( sk )->view;  // a shared target (the owner's): read in place
				texNames.push_back( sl->tex );
			} else if ( const Shared * ft = farTwinOfTex( sl->tex ) ) {
				view = ft->view;  // a far twin the owner made: read in place
				texNames.push_back( sl->tex );
			} else {
				unsigned twin = 0;
				if ( !waterTwin( key, *sl, twin, w ) )
					return refuse( std::string( nm ) + ": " + w );
				const bool viaFar = sl->kind == 4 || waterBcFormat( sl->glFmt );
				const Shared * th = viaFar ? farTwinFind( key ) : sharedFind( key );
				if ( !th || !th->view )
					return refuse( std::string( nm ) + ": its twin has no view" );
				view = th->view;
				static const GLenum kTarget[5] = { 0x0DE1, 0x806F, 0x8C1A, 0x8513, 0x9009 };
				const GLenum target = kTarget[std::clamp( sl->kind, 0, 4 )];
				for ( int l = 0; l < std::max( sl->levels, 1 ); l++ ) {
					const int lw = std::max( sl->w >> l, 1 ), lh = std::max( sl->h >> l, 1 );
					const int ld = sl->kind == 1 ? std::max( sl->d >> l, 1 ) : ( sl->kind == 2 || sl->kind == 4 ) ? sl->d : 1;
					g.copyImageSubData( sl->tex, target, l, 0, 0, 0, twin, target, l, 0, 0, 0, lw, lh, ld );
				}
				copies++;
				texNames.push_back( twin );
			}
			ty[i] = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
			ii[i].imageView = view;
			ii[i].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
		}
		for ( std::size_t i = 0; i < nb; i++ ) {
			VkWriteDescriptorSet x { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
			x.dstSet = D0.setsN[0];
			x.dstBinding = D0.binds[i].binding;
			x.descriptorCount = 1;
			x.descriptorType = ty[i];
			if ( D0.binds[i].kind == 0 )
				x.pBufferInfo = &bi[i];
			else if ( D0.binds[i].kind == 2 )
				x.pTexelBufferView = &tv[i];
			else
				x.pImageInfo = &ii[i];
			ws.push_back( x );
		}
	}
	// the encoder's sets: one a level (its b31 slice), fgSrc = the scratch over every level (the shader Loads fgSrcLevel)
	const farlod::WaterDraw & D1 = S.D[1];
	const std::size_t ne = D1.binds.size();
	WaterSlot plain;
	VkSampler encSm = farlod::waterSampler( S.D[1], plain, false, true, w );
	if ( !encSm )
		return breakOff( w );
	std::vector<VkDescriptorBufferInfo> ebi( ne * static_cast<size_t>( levels ) );
	VkDescriptorImageInfo srcInfo {};
	srcInfo.imageView = scr->view;
	srcInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	VkDescriptorImageInfo smInfo {};
	smInfo.sampler = encSm;
	for ( int L = 0; L < levels; L++ )
		for ( std::size_t i = 0; i < ne; i++ ) {
			const FarSetSlot & b = D1.binds[i];
			VkWriteDescriptorSet x { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
			x.dstSet = D1.setsN[static_cast<size_t>( L )];
			x.dstBinding = b.binding;
			x.descriptorCount = 1;
			if ( b.kind == 0 && b.binding == 31 ) {
				VkDescriptorBufferInfo & o = ebi[static_cast<size_t>( L ) * ne + i];
				o = { S.host.buf, kSliceStride * VkDeviceSize( L + 1 ), VkDeviceSize( kT_fgbc1PsBytes ) };
				x.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
				x.pBufferInfo = &o;
			} else if ( b.kind == 1 && b.binding == 64 ) {
				x.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
				x.pImageInfo = &srcInfo;
			} else if ( b.kind == 3 ) {
				x.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
				x.pImageInfo = &smInfo;
			} else {
				return breakOff( "the encoder twin reads binding " + std::to_string( b.binding ) + " (kind " + std::to_string( b.kind )
					+ ") the paint does not fill" );
			}
			ws.push_back( x );
		}
	v.vkUpdateDescriptorSets( v.dev, std::uint32_t( ws.size() ), ws.data(), 0, nullptr );
	S.rec = Fp::Rec();
	S.rec.scr = scr->img;
	S.rec.blk = blk->img;
	S.rec.pool = pool->img;
	S.rec.pageFb = pfb.fb;
	S.rec.blkFb = S.blkFb.fb;
	S.rec.side = r.side;
	S.rec.levels = levels;
	S.rec.layer = layer;
	S.rec.red = r.red;
	S.rec.encodeOnly = r.encodeOnly;
	// GL's work so far signalled (the scratch, the blocks, the pool, the inputs, the copies; each name once) -> the worker -> GL waits
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
	j.fn = fpWork;
	hand( j );
	bool ok = false;
	{
		std::unique_lock<std::mutex> lk( W.m );
		W.idle.wait( lk, [&W, want]() { return W.toneDone >= want; } );
		ok = W.toneOk;
		w = W.toneWhy;
	}
	if ( !ok )
		return breakOff( w );  // nothing signals vkDone: GL does not wait it; GL paints the job
	g.waitSemaphore( S.vkDoneGl, GLuint( bufNames.size() ), bufNames.data(), GLuint( texNames.size() ), texNames.data(), lay.data() );
	S.pending = true;
	const double ms = nowMs() - t0;
	S.st.runs++;
	S.st.copies += copies;
	S.st.cpuMs += ms;
	S.st.maxMs = std::max( S.st.maxMs, ms );
	return true;
}

void fgPaintOnGl( const char * why )
{
	fgpaint::Fp & S = fgpaint::fpS();
	S.st.gl++;
	S.st.glWhy = why ? why : "";
}

FgPaintStat takeFgPaintStat()
{
	fgpaint::Fp & S = fgpaint::fpS();
	FgPaintStat s = S.st;
	if ( S.broken && s.failWhy.empty() )
		s.failWhy = S.brokenWhy;
	const int pipes = S.st.pipelines;
	const double build = S.st.buildMs;
	S.st = FgPaintStat();
	S.st.pipelines = pipes;
	S.st.buildMs = build;
	return s;
}

#ifndef WW_VKPRESENT_NO_QT
/* lane VKGROUND7: the `vk fgpaint:` line of cellfarground.cpp paintJobs; said at once when the GL reason changes, else at most
 * every 5 s, and (force) at the paint's settle so a fill's last jobs are said; true when said (the caller zeroes its counts) */
bool fgPaintSay( int jobs, int vkN, int glN, const char * glWhy, bool red, bool force )
{
	static std::string said;
	static double at = 0.0;
	const std::string why = glWhy ? glWhy : "";
	const double now = nowMs();
	if ( why == said && !force && now - at < 5000.0 )
		return false;
	said = why;
	at = now;
	const FgPaintStat st = takeFgPaintStat();
	QString line = QStringLiteral( "vk fgpaint: jobs=%1 vk=%2 gl=%3 copies=%4" ).arg( jobs ).arg( vkN ).arg( glN ).arg( st.copies );
	if ( st.fails )
		line += QStringLiteral( " fails=%1 (%2)" ).arg( st.fails ).arg( QString::fromStdString( st.failWhy ) );
	if ( !why.empty() )
		line += QStringLiteral( " glWhy=%1" ).arg( QString::fromStdString( why ) );
	if ( red )
		line += QStringLiteral( " RED" );
	qInfo().noquote() << line;
	return true;
}
#endif
}  // namespace WwVkPresent
