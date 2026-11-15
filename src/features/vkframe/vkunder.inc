/* Lane VKWATER4 (2026-11-14, plan row L7): the camera-underwater fog on the present device (vkunder.h). Included at the
 * end of vkpresent.cpp after vkwater.inc (its vk() / fx() / tone() / sharedTarget / csmBuf). The tone_view_vs triangle,
 * underwater_view_ps then underwater_view_ms_ps (sample shading) in one render pass over the tone frame's colour at its
 * samples, dual-source blended as GL's glBlendFuncSeparate( ONE, SRC1_COLOR, ZERO, ONE ) with alpha written off. */
#include "features/vkframe/vkunder.h"
#include "features/vkframe/underwater_view_ps.spv.h"
#include "features/vkframe/underwater_view_ms_ps.spv.h"

namespace
{
constexpr VkDeviceSize kUnderLooseStride = 512;  // two b31 blocks, each on any minUniformBufferOffsetAlignment up to 256

struct UnderDraw
{
	bool made = false, broken = false;
	std::string brokenWhy;
	VkShaderModule mod[2] = {};  // [0] per pixel, [1] per sample
	VkDescriptorSetLayout dsl = VK_NULL_HANDLE;
	VkDescriptorPool pool = VK_NULL_HANDLE;
	VkDescriptorSet set[2] = {};
	VkPipelineLayout layout = VK_NULL_HANDLE;
	VkRenderPass pass = VK_NULL_HANDLE;
	int passSamples = 0;
	std::map<int, VkPipeline> pipes;  // key: program * 4 + red's blend (2) at passSamples
	CsmBuf host;
	VkImage msImg = VK_NULL_HANDLE;  // the tone frame's depth, its depth-aspect view for uwDepthMs
	std::uint64_t msGen = 0;  // sharedGen() when msView was made (toneFrame bumps it: a remade frame can reuse the handle)
	VkImageView msView = VK_NULL_HANDLE;
	VkFramebuffer fb = VK_NULL_HANDLE;
	// this run's record (written before the hand)
	VkPipeline pipe[2] = {};
	bool two = true;
	int w = 0, h = 0, vp[4] = {};
	WwVkPresent::FxStat st;
	int pipelines = 0;
	double buildMs = 0.0;
};
UnderDraw & underD()
{
	static UnderDraw D;
	return D;
}

// the device-time parts (once): both modules, one layout over the twins' 14 bindings (fragment), two sets, the pool
bool underStatic( std::string & why )
{
	Vk & v = vk();
	UnderDraw & D = underD();
	if ( D.made )
		return true;
	if ( !fxModule( vkframe_underwater_view_ps_spv, sizeof( vkframe_underwater_view_ps_spv ), D.mod[0] )
		|| !fxModule( vkframe_underwater_view_ms_ps_spv, sizeof( vkframe_underwater_view_ms_ps_spv ), D.mod[1] ) ) {
		why = "the underwater draw's modules failed";
		return false;
	}
	// kN_waterUw: b31; t99..t105 (t104 uwDepth, t105 uwDepthMs; the rest declared, never read); s163..s168 (never read)
	std::vector<VkDescriptorSetLayoutBinding> lb;
	std::uint32_t n[3] = {};
	for ( const WwVkPresent::WaterBindName & b : WwVkPresent::kN_waterUw ) {
		VkDescriptorSetLayoutBinding x {};
		x.binding = b.binding;
		const int k = b.binding < 64 ? 0 : b.binding < 128 ? 1 : 2;
		static const VkDescriptorType kType[3] = { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
			VK_DESCRIPTOR_TYPE_SAMPLER };
		x.descriptorType = kType[k];
		x.descriptorCount = 1;
		x.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
		lb.push_back( x );
		n[k]++;
	}
	VkDescriptorSetLayoutCreateInfo li { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
	li.bindingCount = std::uint32_t( lb.size() );
	li.pBindings = lb.data();
	const VkDescriptorPoolSize ps[3] = { { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 2 * n[0] }, { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 2 * n[1] },
		{ VK_DESCRIPTOR_TYPE_SAMPLER, 2 * n[2] } };
	VkDescriptorPoolCreateInfo pi { VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
	pi.maxSets = 2;
	pi.poolSizeCount = 3;
	pi.pPoolSizes = ps;
	if ( v.vkCreateDescriptorSetLayout( v.dev, &li, nullptr, &D.dsl ) != VK_SUCCESS
		|| v.vkCreateDescriptorPool( v.dev, &pi, nullptr, &D.pool ) != VK_SUCCESS ) {
		why = "the underwater draw's set layout / pool failed";
		return false;
	}
	const VkDescriptorSetLayout two[2] = { D.dsl, D.dsl };
	VkDescriptorSetAllocateInfo ai { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
	ai.descriptorPool = D.pool;
	ai.descriptorSetCount = 2;
	ai.pSetLayouts = two;
	VkPipelineLayoutCreateInfo pli { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
	pli.setLayoutCount = 1;
	pli.pSetLayouts = &D.dsl;
	if ( v.vkAllocateDescriptorSets( v.dev, &ai, D.set ) != VK_SUCCESS
		|| v.vkCreatePipelineLayout( v.dev, &pli, nullptr, &D.layout ) != VK_SUCCESS ) {
		why = "the underwater draw's sets / pipeline layout failed";
		return false;
	}
	D.made = true;
	return true;
}

// the render pass: the tone frame's RGBA16F colour alone, at its samples, LOAD / STORE, GENERAL (GL's colour-only FBO)
bool underPass( int samples, std::string & why )
{
	Vk & v = vk();
	UnderDraw & D = underD();
	if ( D.pass && D.passSamples == samples )
		return true;
	for ( auto & kv : D.pipes )
		v.vkDestroyPipeline( v.dev, kv.second, nullptr );
	D.pipes.clear();
	if ( D.pass )
		v.vkDestroyRenderPass( v.dev, D.pass, nullptr );
	D.pass = VK_NULL_HANDLE;
	D.passSamples = 0;
	VkAttachmentDescription at {};
	at.format = VK_FORMAT_R16G16B16A16_SFLOAT;
	at.samples = VkSampleCountFlagBits( samples );
	at.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
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
	if ( v.vkCreateRenderPass( v.dev, &rci, nullptr, &D.pass ) != VK_SUCCESS ) {
		why = "vkCreateRenderPass (the underwater draw) failed";
		return false;
	}
	D.passSamples = samples;
	return true;
}

/* one pipeline: GL's state in wwCellUnderwaterDraw (cellunderwater.cpp): no depth, stencil, cull or scissor test, colour
 * mask RGB, blend ADD ( ONE, SRC1_COLOR ) / ( ZERO, ONE ). The per-sample program shades per sample (GL: gl_SampleID read).
 * red 2: ( ONE, ONE ), the multiply lost */
VkPipeline underPipe( int k, bool redBlend, std::string & why )
{
	Vk & v = vk();
	UnderDraw & D = underD();
	const int key = k * 4 + ( redBlend ? 2 : 0 );
	auto it = D.pipes.find( key );
	if ( it != D.pipes.end() )
		return it->second;
	const double t0 = nowMs();
	VkPipelineShaderStageCreateInfo st[2] = { { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO },
		{ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO } };
	st[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
	st[0].module = fx().vs;  // tone_view_vs: z = 1 like cell_hdr.vert's quad
	st[0].pName = "main";
	st[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	st[1].module = D.mod[k];
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
	ms.rasterizationSamples = VkSampleCountFlagBits( D.passSamples );
	if ( k == 1 ) {
		ms.sampleShadingEnable = VK_TRUE;
		ms.minSampleShading = 1.0f;
	}
	VkPipelineColorBlendAttachmentState cb {};
	cb.blendEnable = VK_TRUE;
	cb.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
	cb.dstColorBlendFactor = redBlend ? VK_BLEND_FACTOR_ONE : VK_BLEND_FACTOR_SRC1_COLOR;
	cb.colorBlendOp = VK_BLEND_OP_ADD;
	cb.srcAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
	cb.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	cb.alphaBlendOp = VK_BLEND_OP_ADD;
	cb.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT;
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
	VkPipeline out = VK_NULL_HANDLE;
	if ( v.vkCreateGraphicsPipelines( v.dev, tone().cache, 1, &gp, nullptr, &out ) != VK_SUCCESS ) {
		why = "vkCreateGraphicsPipelines (the underwater draw) failed";
		return VK_NULL_HANDLE;
	}
	D.pipes[key] = out;
	D.pipelines++;
	D.buildMs += nowMs() - t0;
	toneCacheSave();
	return out;
}
}  // namespace

namespace
{
// fxWork's kind 10 (the worker): both draws in one render pass (blending keeps their order)
bool underRecord( std::string & why )
{
	Vk & v = vk();
	Fx & F = fx();
	UnderDraw & D = underD();
	if ( !D.fb || !D.pass || !D.pipe[0] || ( D.two && !D.pipe[1] ) ) {
		why = "no framebuffer / pipeline for the underwater draw";
		return false;
	}
	VkRenderPassBeginInfo rbi { VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
	rbi.renderPass = D.pass;
	rbi.framebuffer = D.fb;
	rbi.renderArea = { { 0, 0 }, { std::uint32_t( D.w ), std::uint32_t( D.h ) } };
	v.vkCmdBeginRenderPass( F.cmd, &rbi, VK_SUBPASS_CONTENTS_INLINE );
	// GL's viewport rows from the bottom = the image's rows from the top: no flip (FragCoord.y = GL's)
	const VkViewport vpt { float( D.vp[0] ), float( D.vp[1] ), float( D.vp[2] ), float( D.vp[3] ), 0.0f, 1.0f };
	const int x0 = std::clamp( D.vp[0], 0, D.w ), y0 = std::clamp( D.vp[1], 0, D.h );
	const int x1 = std::clamp( D.vp[0] + D.vp[2], 0, D.w ), y1 = std::clamp( D.vp[1] + D.vp[3], 0, D.h );
	const VkRect2D sc { { x0, y0 }, { std::uint32_t( std::max( x1 - x0, 0 ) ), std::uint32_t( std::max( y1 - y0, 0 ) ) } };
	v.vkCmdSetViewport( F.cmd, 0, 1, &vpt );
	v.vkCmdSetScissor( F.cmd, 0, 1, &sc );
	for ( int k = 0; k < ( D.two ? 2 : 1 ); k++ ) {
		v.vkCmdBindPipeline( F.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, D.pipe[k] );
		v.vkCmdBindDescriptorSets( F.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, D.layout, 0, 1, &D.set[k], 0, nullptr );
		v.vkCmdDraw( F.cmd, 3, 1, 0, 0 );
	}
	v.vkCmdEndRenderPass( F.cmd );
	return true;
}

// shutdown (after fxDrop waited the device): the framebuffer and the depth view; pipelines and sets stay with the device
void underDrop()
{
	Vk & v = vk();
	UnderDraw & D = underD();
	if ( !D.fb && !D.msView && !D.host.buf )
		return;
	v.vkDeviceWaitIdle( v.dev );
	if ( D.fb )
		v.vkDestroyFramebuffer( v.dev, D.fb, nullptr );
	if ( D.msView )
		v.vkDestroyImageView( v.dev, D.msView, nullptr );
	D.fb = VK_NULL_HANDLE;
	D.msView = VK_NULL_HANDLE;
	D.msImg = VK_NULL_HANDLE;
	csmBufFree( D.host, false );
}
}  // namespace

namespace WwVkPresent
{
std::string underRefusal()
{
	Vk & v = vk();
	Gl & g = gl();
	if ( underD().broken )
		return underD().brokenWhy;
	if ( !v.ok || !g.ok )
		return "the present device or GL's entry points are not ready";
	if ( !v.dualSrcBlend )
		return "no dualSrcBlend on the present device (GL's SRC1_COLOR blend)";
	if ( !v.sampleShading )
		return "no sampleRateShading on the present device (the per-sample draw)";
	if ( !g.copyImageSubData )
		return "no glCopyImageSubData (uwDepth's copy)";
	return std::string();
}

void underOnGl( const char * why )
{
	FxStat & S = underD().st;
	S.gl++;
	S.glWhy = why ? why : "";
}

FxStat takeUnderStat()
{
	UnderDraw & D = underD();
	FxStat s = D.st;
	s.pipelines = D.pipelines;
	s.buildMs = D.buildMs;
	D.st = FxStat();
	return s;
}

bool underRun( const UnderRun & r, std::string & why )
{
	Fx & F = fx();
	UnderDraw & D = underD();
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
		D.brokenWhy = "the underwater draw on Vulkan broke: " + w;
		return refuse( D.brokenWhy );
	};
	if ( D.broken )
		return refuse( D.brokenWhy );
	if ( !v.ok || !g.ok || g.ctx != reinterpret_cast<void *>( wglGetCurrentContext() ) )
		return refuse( "the present device or GL's entry points are not ready in this context" );
	if ( const std::string no = underRefusal(); !no.empty() )
		return refuse( no );
	if ( !toneFrameIs( r.color ) || !T.colorView || !T.ds )
		return refuse( "the draw target is not the shared tone frame" );
	if ( T.w != r.w || T.h != r.h )
		return refuse( "the underwater draw's size is not the tone frame's" );
	// GL's per-pixel draw alone (uwSamples 0: one sample, red maxdepth, no per-sample program) reads no depth per sample
	if ( r.samples < 2 || T.samples < 2 )
		return refuse( "GL draws one fog per pixel (uwSamples 0): the per-sample twin pair needs the frame's samples" );
	if ( r.samples != T.samples || r.depthMs != T.dsTex )
		return refuse( "uwDepthMs is not the tone frame's own depth at its samples" );
	if ( !r.depth || r.depthFmt != 0x822E || r.depthW != T.w || r.depthH != T.h )  // GL_R32F
		return refuse( "uwDepth is not an R32F texture at the frame's size" );
	if ( r.vp[2] < 1 || r.vp[3] < 1 )
		return refuse( "no viewport" );
	// the last crossing (any fx pass) is done before its command buffer, sets and framebuffer are written again
	if ( std::string fw; !fenceDone( F.fence, F.pending, "under", fw ) )
		return refuse( fw );
	F.pending = false;
	std::string w;
	if ( !fxStatic( w ) || !fxSems( w ) || !underStatic( w ) || !underPass( T.samples, w ) )
		return breakOff( w );
	D.pipe[0] = underPipe( 0, ( r.red & 2 ) != 0, w );
	D.pipe[1] = underPipe( 1, ( r.red & 2 ) != 0, w );
	if ( !D.pipe[0] || !D.pipe[1] )
		return breakOff( w );
	// uwDepth: GL's R32F into its shared twin (every run: GL makes it again each frame)
	const std::uintptr_t key = reinterpret_cast<std::uintptr_t>( &D );
	unsigned twin = 0;
	if ( !sharedTarget( key, 0, r.depthW, r.depthH, 1, 0x822E, twin, w ) || !twin )
		return refuse( "uwDepth's twin: " + w );
	const Shared * tw = sharedFind( key );
	if ( !tw || !tw->view )
		return refuse( "uwDepth's twin has no view" );
	if ( !( r.red & 4 ) ) {
		static unsigned checked = 0;
		static int checkedW = 0, checkedH = 0;  // the copy's GL error read once a new twin or size
		const bool ask = twin != checked || r.depthW != checkedW || r.depthH != checkedH;
		if ( ask )
			while ( g.getError() != 0 ) {}
		g.copyImageSubData( r.depth, 0x0DE1, 0, 0, 0, 0, twin, 0x0DE1, 0, 0, 0, 0, r.depthW, r.depthH, 1 );
		if ( ask ) {
			if ( const GLenum e = g.getError() )
				return refuse( "glCopyImageSubData of uwDepth: GL error " + std::to_string( e ) );
			checked = twin;
			checkedW = r.depthW;
			checkedH = r.depthH;
		}
		S.copies++;
	}
	// uwDepthMs: the tone frame's depth read in place through a depth-aspect view (T.dsView is depth + stencil)
	if ( T.ds != D.msImg || D.msGen != sharedGen() ) {
		D.msGen = sharedGen();
		if ( D.msView )
			v.vkDestroyImageView( v.dev, D.msView, nullptr );
		D.msView = imageView( T.ds, VK_IMAGE_VIEW_TYPE_2D, VK_FORMAT_D24_UNORM_S8_UINT, VK_IMAGE_ASPECT_DEPTH_BIT );
		D.msImg = D.msView ? T.ds : VK_NULL_HANDLE;
		if ( !D.msView )
			return breakOff( "the tone frame's depth view failed" );
	}
	// both loose blocks into the host buffer
	if ( !csmBuf( D.host, 2 * kUnderLooseStride, false, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, w ) )
		return breakOff( w );
	std::uint8_t * host = static_cast<std::uint8_t *>( D.host.ptr );
	for ( int k = 0; k < 2; k++ ) {
		std::uint8_t * at = host + k * kUnderLooseStride;
		std::memcpy( at, r.loose[k], size_t( kT_waterUwBytes ) );
		if ( r.red & 8 ) {  // red: uwRow one vec4 off (a table row wrong; the whole block would move uwSamples: a GPU hang)
			for ( const FxField & x : kT_waterUw )
				if ( std::strcmp( x.name, "uwRow" ) == 0 ) {
					std::memmove( at + x.offset + 16, r.loose[k] + x.offset, 32 );
					std::memset( at + x.offset, 0, 16 );
				}
		}
	}
	// the sets: b31, uwDepth's twin, the frame's depth; the declared-unread images get the zero 2D, every sampler one
	VkDescriptorBufferInfo bi[2] = {};
	VkDescriptorImageInfo ii[2][16] = {};
	std::vector<VkWriteDescriptorSet> ws;
	for ( int k = 0; k < 2; k++ ) {
		int n = 0;
		for ( const WaterBindName & b : kN_waterUw ) {
			VkWriteDescriptorSet x { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
			x.dstSet = D.set[k];
			x.dstBinding = b.binding;
			x.descriptorCount = 1;
			if ( b.binding < 64 ) {
				bi[k] = { D.host.buf, VkDeviceSize( k ) * kUnderLooseStride, VkDeviceSize( kT_waterUwBytes ) };
				x.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
				x.pBufferInfo = &bi[k];
			} else if ( b.binding < 128 ) {
				VkDescriptorImageInfo & d = ii[k][n++];
				d.imageView = std::strcmp( b.name, "uwDepth" ) == 0 ? tw->view : std::strcmp( b.name, "uwDepthMs" ) == 0 ? D.msView : F.d2;
				d.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
				x.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
				x.pImageInfo = &d;
			} else {
				VkDescriptorImageInfo & d = ii[k][n++];
				d.sampler = F.smp[kFxNearestClamp];
				x.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
				x.pImageInfo = &d;
			}
			ws.push_back( x );
		}
	}
	const VkImageView att = T.colorView;
	if ( !fxFb( D.fb, D.pass, &att, 1, T.w, T.h ) )
		return breakOff( "the underwater draw's framebuffer over the tone frame failed" );
	v.vkUpdateDescriptorSets( v.dev, std::uint32_t( ws.size() ), ws.data(), 0, nullptr );
	D.two = !( r.red & 1 );
	D.w = r.w;
	D.h = r.h;
	std::memcpy( D.vp, r.vp, sizeof( D.vp ) );
	F.kind = 10;  // fxWork: the underwater fog (4 is the water, 9 the far ground)
	F.w = r.w;
	F.h = r.h;
	// GL's work so far signalled (the frame, its depth, the twin and its copy) -> the worker's job -> GL waits
	const GLuint texNames[3] = { T.colorTex, T.dsTex, GLuint( twin ) };
	const GLenum lay[3] = { kLayoutGeneral, kLayoutGeneral, kLayoutGeneral };
	g.signalSemaphore( F.glDoneGl, 0, nullptr, 3, texNames, lay );
	g.flush();
	Worker & W = wk();
	std::uint64_t want = 0;
	{
		std::lock_guard<std::mutex> lk( W.m );
		want = W.toneDone + 1;
	}
	Work j;
	j.k = -1;
	j.fx = true;
	hand( j );
	bool ok = false;
	{
		std::unique_lock<std::mutex> lk( W.m );
		W.idle.wait( lk, [&W, want]() { return W.toneDone >= want; } );
		ok = W.toneOk;
		w = W.toneWhy;
	}
	if ( !ok )
		return breakOff( w );  // nothing signals vkDone: GL does not wait it; GL draws the fog this frame
	g.waitSemaphore( F.vkDoneGl, 0, nullptr, 3, texNames, lay );
	F.pending = true;
	const double ms = nowMs() - t0;
	S.runs++;
	S.cpuMs += ms;
	S.maxMs = std::max( S.maxMs, ms );
	return true;
}
}  // namespace WwVkPresent

#ifndef WW_VKPRESENT_NO_QT
namespace WwVkPresent
{
void underSay( bool wanted, const char * glWhy )
{
	static QString said;
	static double at = 0.0;
	const QString head = QStringLiteral( "vk under: " );
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
	const FxStat st = takeUnderStat();
	QString line = head + QStringLiteral( "wanted on Vulkan, %1 frames run (both draws, one crossing; %2 ms CPU each, max %3; "
		"%4 pipelines built in %5 ms)" )
		.arg( st.runs ).arg( st.runs ? st.cpuMs / st.runs : 0.0, 0, 'f', 3 ).arg( st.maxMs, 0, 'f', 3 ).arg( st.pipelines )
		.arg( st.buildMs, 0, 'f', 1 );
	if ( st.gl > 0 )
		line += QStringLiteral( "; on GL %1 frames (last: %2)" ).arg( st.gl ).arg( QString::fromStdString( st.glWhy ) );
	if ( st.fails > 0 )
		line += QStringLiteral( "; %1 refused (last: %2)" ).arg( st.fails ).arg( QString::fromStdString( st.failWhy ) );
	said = line;
	qInfo().noquote() << line;
}
}  // namespace WwVkPresent
#endif
