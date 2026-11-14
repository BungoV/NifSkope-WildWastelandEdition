// Lane VKFAR2 P9 (2026-11-14): the lens-flare sprites (celllensflare.cpp, cell_lensflare.vert/.frag) recorded in the deferred
// tone pass, after its modes and before the crossing (toneRecord: flRecordPre outside the pass, flRecordIn before its end).
// Included by vkpresent.cpp after vkfog.inc: it uses the tone's state and pass, the cache's twins (ot, farTwinFind), the
// bindless sampler mapping (blSamplerRead, blSamplerKey, blSwizzleNorm, blIsSrgb), sbBuffer / sbSampler (vkskybody.inc),
// makeLocal, imageView, sharedTarget. The API: vkpresent.h (FlareRun). Shader: res/shaders/hlsl/post/lens_flare.hlsl.
#include "features/vkframe/lensflare_view_vs.spv.h"
#include "features/vkframe/lensflare_view_ps.spv.h"

namespace
{
constexpr VkDeviceSize kFlareRowBytes = 4 * 4 * sizeof( float );  // one corner: pos, col, uv, lit
constexpr int kFlarePush = 128;                                    // PfxJob: float4 c[7], int4 i

struct Flare
{
	bool made = false, broken = false;
	std::string brokenWhy;
	VkDescriptorSetLayout dsl = VK_NULL_HANDLE;
	VkDescriptorPool dpool = VK_NULL_HANDLE;
	VkDescriptorSet set[kFlareTexMax] = {};
	VkPipelineLayout layout = VK_NULL_HANDLE;
	VkShaderModule vs = VK_NULL_HANDLE, ps = VK_NULL_HANDLE;
	VkPipeline pipe = VK_NULL_HANDLE;
	VkRenderPass pipePass = VK_NULL_HANDLE;
	int pipeSamples = 0, pipelines = 0;
	double buildMs = 0.0;
	// the sprites' table: RGBA32F, 4 wide (the attributes), a row per corner; filled from staging before the pass
	VkImage table = VK_NULL_HANDLE;
	VkDeviceMemory tableMem = VK_NULL_HANDLE;
	VkImageView tableView = VK_NULL_HANDLE;
	VkBuffer stage = VK_NULL_HANDLE;
	VkDeviceMemory stageMem = VK_NULL_HANDLE;
	std::uint8_t * stagePtr = nullptr;
	// the opaque depth's GL format, asked once per name and size
	GLuint depthName = 0;
	int depthW = 0, depthH = 0;
	GLint depthFmt = 0;
	// this pass (written by the drawing thread after toneRun, recorded by the worker)
	int n = 0;
	std::uint8_t setOf[WwVkPresent::kFlareMaxSprites] = {};
	int vp[4] = {};
	std::uint8_t push[kFlarePush] = {};
	WwVkPresent::FxStat st;
};

Flare & flare()
{
	static Flare f;
	return f;
}

std::uintptr_t flDepthKey()
{
	return reinterpret_cast<std::uintptr_t>( &flare() );
}

// device-time parts (once): the set layout (t0 frag, t1 / t2 vert, s5 frag), the sets, the modules, the table and staging
bool flStatic( std::string & why )
{
	Vk & v = vk();
	Flare & F = flare();
	if ( F.made )
		return true;
	const double t0 = nowMs();
	const VkShaderStageFlags both = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
	const VkDescriptorSetLayoutBinding b[4] = { { 0, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, both, nullptr },
		{ 1, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, both, nullptr }, { 2, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, both, nullptr },
		{ 5, VK_DESCRIPTOR_TYPE_SAMPLER, 1, both, nullptr } };
	VkDescriptorSetLayoutCreateInfo lci { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
	lci.bindingCount = 4;
	lci.pBindings = b;
	const VkDescriptorPoolSize ps[2] = { { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 3 * kFlareTexMax }, { VK_DESCRIPTOR_TYPE_SAMPLER, kFlareTexMax } };
	VkDescriptorPoolCreateInfo pci { VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
	pci.maxSets = kFlareTexMax;
	pci.poolSizeCount = 2;
	pci.pPoolSizes = ps;
	if ( v.vkCreateDescriptorSetLayout( v.dev, &lci, nullptr, &F.dsl ) != VK_SUCCESS
		|| v.vkCreateDescriptorPool( v.dev, &pci, nullptr, &F.dpool ) != VK_SUCCESS ) {
		why = "the lens flare's set layout or pool failed";
		return false;
	}
	VkDescriptorSetLayout dsls[kFlareTexMax];
	for ( VkDescriptorSetLayout & d : dsls )
		d = F.dsl;
	VkDescriptorSetAllocateInfo dai { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
	dai.descriptorPool = F.dpool;
	dai.descriptorSetCount = kFlareTexMax;
	dai.pSetLayouts = dsls;
	const VkPushConstantRange pr { both, 0, kFlarePush };
	VkPipelineLayoutCreateInfo plci { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
	plci.setLayoutCount = 1;
	plci.pSetLayouts = &F.dsl;
	plci.pushConstantRangeCount = 1;
	plci.pPushConstantRanges = &pr;
	VkShaderModuleCreateInfo mvs { VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
	mvs.codeSize = sizeof( vkframe_lensflare_view_vs_spv );
	mvs.pCode = vkframe_lensflare_view_vs_spv;
	VkShaderModuleCreateInfo mps { VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
	mps.codeSize = sizeof( vkframe_lensflare_view_ps_spv );
	mps.pCode = vkframe_lensflare_view_ps_spv;
	if ( v.vkAllocateDescriptorSets( v.dev, &dai, F.set ) != VK_SUCCESS
		|| v.vkCreatePipelineLayout( v.dev, &plci, nullptr, &F.layout ) != VK_SUCCESS
		|| v.vkCreateShaderModule( v.dev, &mvs, nullptr, &F.vs ) != VK_SUCCESS
		|| v.vkCreateShaderModule( v.dev, &mps, nullptr, &F.ps ) != VK_SUCCESS ) {
		why = "the lens flare's sets, layout or modules failed";
		return false;
	}
	double mb = 0.0;
	if ( !makeLocal( F.table, F.tableMem, VK_IMAGE_TYPE_2D, VK_FORMAT_R32G32B32A32_SFLOAT, 4, 4 * WwVkPresent::kFlareMaxSprites, 1, mb )
		|| !( F.tableView = imageView( F.table, VK_IMAGE_VIEW_TYPE_2D, VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_ASPECT_COLOR_BIT ) )
		|| !sbBuffer( kFlareRowBytes * 4 * WwVkPresent::kFlareMaxSprites, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, F.stage, F.stageMem, F.stagePtr ) ) {
		why = "the lens flare's sprite table or its staging failed";
		return false;
	}
	F.buildMs += nowMs() - t0;
	F.made = true;
	return true;
}

/* the sprites' pipeline in the tone pass (its samples): no vertex input (the table), no depth or stencil test (GL turns
 * both off), blend ONE / ONE (alpha too: the shader writes 0), at pixel rate as GL's MSAA draw */
bool flPipe( std::string & why )
{
	Vk & v = vk();
	Tone & T = tone();
	Flare & F = flare();
	if ( F.pipe && F.pipePass == T.pass && F.pipeSamples == T.pipeSamples )
		return true;
	const double t0 = nowMs();
	if ( F.pipe )
		v.vkDestroyPipeline( v.dev, F.pipe, nullptr );
	F.pipe = VK_NULL_HANDLE;
	F.pipePass = VK_NULL_HANDLE;
	VkPipelineShaderStageCreateInfo st[2] = { { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO },
		{ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO } };
	st[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
	st[0].module = F.vs;
	st[0].pName = "main";
	st[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	st[1].module = F.ps;
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
	ms.rasterizationSamples = VkSampleCountFlagBits( std::max( T.pipeSamples, 1 ) );
	VkPipelineDepthStencilStateCreateInfo ds { VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };
	VkPipelineColorBlendAttachmentState cb {};
	cb.blendEnable = VK_TRUE;
	cb.srcColorBlendFactor = cb.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
	cb.srcAlphaBlendFactor = cb.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	cb.colorBlendOp = cb.alphaBlendOp = VK_BLEND_OP_ADD;
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
	gp.layout = F.layout;
	gp.renderPass = T.pass;
	if ( v.vkCreateGraphicsPipelines( v.dev, T.cache, 1, &gp, nullptr, &F.pipe ) != VK_SUCCESS ) {
		F.pipe = VK_NULL_HANDLE;
		why = "vkCreateGraphicsPipelines (the lens flare, " + std::to_string( T.pipeSamples ) + " samples) failed";
		return false;
	}
	F.pipePass = T.pass;
	F.pipeSamples = T.pipeSamples;
	F.pipelines++;
	F.buildMs += nowMs() - t0;
	toneCacheSave();
	return true;
}

/* a sprite map on Vulkan: the cache's BC twin of the GL name, with GL's sampler state (sbTex's twin half; the sky's own
 * LUTs and its unbound unit are not a flare's) */
bool flTex( unsigned name, WwVkPresent::GlProc gp, VkImageView & view, VkSampler & smp, std::string & why )
{
	view = VK_NULL_HANDLE;
	smp = VK_NULL_HANDLE;
	auto it = ot().twins.find( GLuint( name ) );
	const Shared * s = it == ot().twins.end() ? nullptr : farTwinFind( it->second.key );
	if ( !s || !s->view || s->type != 0 ) {
		why = "sprite map " + std::to_string( name ) + " has no Vulkan twin (not block-compressed, or loaded before the flare "
			"wanted it)";
		return false;
	}
	WwVkPresent::BlSampler bs;
	BlSmpKey k;
	std::string w;
	if ( !WwVkPresent::blSamplerRead( name, bs, gp, w ) ) {
		why = "sprite map " + std::to_string( name ) + ": " + w;
		return false;
	}
	if ( blIsSrgb( s->fmt ) && bs.srgbDecode == 0x8A4A ) {
		why = "sprite map " + std::to_string( name ) + ": an sRGB map with its decode skipped";
		return false;
	}
	blSwizzleNorm( s->fmt, bs );
	if ( !blSamplerKey( bs, k, w ) || !( smp = sbSampler( k, w ) ) ) {
		why = "sprite map " + std::to_string( name ) + ": " + w;
		return false;
	}
	view = s->view;
	return true;
}

bool flDefAdd( GLuint name )
{
	Tone & T = tone();
	if ( std::find( T.defTex, T.defTex + T.defN, name ) != T.defTex + T.defN )
		return true;
	if ( T.defN >= GLuint( kToneDefMax ) )
		return false;
	T.defTex[T.defN++] = name;
	return true;
}
}  // namespace

namespace
{
void flToneReset()
{
	flare().n = 0;
}

// before the tone pass: the table from staging (the drawing thread wrote it; the last pass's fence was waited)
void flRecordPre( VkCommandBuffer cmd )
{
	Flare & F = flare();
	if ( F.n <= 0 )
		return;
	Vk & v = vk();
	VkImageMemoryBarrier ib { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
	ib.srcQueueFamilyIndex = ib.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	ib.image = F.table;
	ib.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
	ib.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;  // every row the draws read is written below
	ib.newLayout = VK_IMAGE_LAYOUT_GENERAL;
	ib.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
	ib.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	v.vkCmdPipelineBarrier( cmd, VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
		0, 0, nullptr, 0, nullptr, 1, &ib );
	VkBufferImageCopy cp {};
	cp.imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
	cp.imageExtent = { 4, std::uint32_t( 4 * F.n ), 1 };
	v.vkCmdCopyBufferToImage( cmd, F.stage, F.table, VK_IMAGE_LAYOUT_GENERAL, 1, &cp );
	ib.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
	ib.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	ib.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
	v.vkCmdPipelineBarrier( cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_VERTEX_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &ib );
}

// inside the tone pass: GL's draw order, one draw per run of sprites sharing a map. true: something was recorded
bool flRecordIn( VkCommandBuffer cmd )
{
	Flare & F = flare();
	if ( F.n <= 0 || !F.pipe )
		return false;
	Vk & v = vk();
	const VkViewport vp { float( F.vp[0] ), float( F.vp[1] ), float( F.vp[2] ), float( F.vp[3] ), 0.0f, 1.0f };
	const int sx = std::max( F.vp[0], 0 ), sy = std::max( F.vp[1], 0 );
	const VkRect2D sc { { sx, sy }, { std::uint32_t( std::max( F.vp[0] + F.vp[2] - sx, 0 ) ), std::uint32_t( std::max( F.vp[1] + F.vp[3] - sy, 0 ) ) } };
	v.vkCmdSetViewport( cmd, 0, 1, &vp );
	v.vkCmdSetScissor( cmd, 0, 1, &sc );
	v.vkCmdBindPipeline( cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, F.pipe );
	v.vkCmdPushConstants( cmd, F.layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, kFlarePush, F.push );
	int at = 0;
	while ( at < F.n ) {
		int end = at + 1;
		while ( end < F.n && F.setOf[end] == F.setOf[at] )
			end++;
		v.vkCmdBindDescriptorSets( cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, F.layout, 0, 1, &F.set[F.setOf[at]], 0, nullptr );
		v.vkCmdDraw( cmd, std::uint32_t( 6 * ( end - at ) ), 1, std::uint32_t( 6 * at ), 0 );
		at = end;
	}
	return true;
}
}  // namespace

namespace WwVkPresent
{
bool toneFlareQueue( const FlareRun & f, std::string & why )
{
	Flare & F = flare();
	Tone & T = tone();
	Vk & v = vk();
	Gl & g = gl();
	FxStat & S = F.st;
	const double t0 = nowMs();
	auto refuse = [&]( const std::string & w ) {
		why = w;
		S.fails++;
		S.failWhy = w;
		return false;
	};
	auto breakOff = [&]( const std::string & w ) {
		F.broken = true;
		F.brokenWhy = "the lens flare on Vulkan broke: " + w;
		return refuse( F.brokenWhy );
	};
	if ( F.broken )
		return refuse( F.brokenWhy );
	if ( !T.deferred || T.pending )
		return refuse( "no deferred tone pass to record the sprites into" );
	if ( F.n > 0 )
		return refuse( "the sprites were already queued into this tone pass" );
	if ( f.n < 1 || f.n > kFlareMaxSprites || !f.rows || !f.tex || !f.gp )
		return refuse( "sprites " + std::to_string( f.n ) + " (1.." + std::to_string( kFlareMaxSprites ) + " with rows, maps and GL's entry points)" );
	if ( f.vp[2] < 1 || f.vp[3] < 1 )
		return refuse( "an empty viewport" );
	if ( !v.ok || !g.ok || g.ctx != reinterpret_cast<void *>( wglGetCurrentContext() ) )
		return refuse( "the present device or GL's entry points are not ready in this context" );
	std::string w;
	if ( !flStatic( w ) || !flPipe( w ) )
		return breakOff( w );
	// the maps: one set per distinct map, at most kFlareTexMax
	unsigned names[kFlareTexMax] = {};
	VkImageView views[kFlareTexMax] = {};
	VkSampler smps[kFlareTexMax] = {};
	int nNames = 0;
	std::uint8_t setOf[kFlareMaxSprites];
	for ( int k = 0; k < f.n; k++ ) {
		int at = 0;
		while ( at < nNames && names[at] != f.tex[k] )
			at++;
		if ( at == nNames ) {
			if ( nNames == kFlareTexMax )
				return refuse( "more than " + std::to_string( kFlareTexMax ) + " sprite maps in one frame" );
			if ( !flTex( f.tex[k], f.gp, views[at], smps[at], w ) )
				return refuse( w );
			names[nNames++] = f.tex[k];
		}
		setOf[k] = std::uint8_t( at );
	}
	// the crossing's names: what is new to it must fit
	int newNames = 0;
	for ( int k = 0; k < nNames; k++ )
		newNames += std::find( T.defTex, T.defTex + T.defN, GLuint( names[k] ) ) == T.defTex + T.defN ? 1 : 0;
	int red = f.red;
	unsigned depthTex = 0;
	if ( !f.depth )
		red |= 1;  // no depth kept: unoccluded, as GL's
	if ( flareRedTest == 2 )
		red |= 1;  // the red: the depth ignored
	if ( T.defN + GLuint( newNames + ( ( red & 1 ) ? 0 : 1 ) ) > GLuint( kToneDefMax ) )
		return refuse( "the crossing's texture list is full" );
	if ( !( red & 1 ) ) {
		if ( !g.copyImageSubData )
			return refuse( "no glCopyImageSubData (the opaque depth's copy into its shared twin)" );
		if ( F.depthName != f.depth || F.depthW != f.vp[2] || F.depthH != f.vp[3] ) {
			using LevelParam = void( WINAPI * )( GLuint, GLint, GLenum, GLint * );
			const auto lp = reinterpret_cast<LevelParam>( f.gp( "glGetTextureLevelParameteriv" ) );
			GLint fmt = 0;
			if ( lp )
				lp( GLuint( f.depth ), 0, 0x1003, &fmt );  // GL_TEXTURE_INTERNAL_FORMAT
			if ( !fmt )
				return refuse( "the opaque depth's GL format could not be read" );
			F.depthName = GLuint( f.depth );
			F.depthW = f.vp[2];
			F.depthH = f.vp[3];
			F.depthFmt = fmt;
		}
		if ( !sharedTarget( flDepthKey(), 0, f.vp[2], f.vp[3], 1, unsigned( F.depthFmt ), depthTex, w ) )
			return refuse( "the opaque depth's twin: " + w );
		g.copyImageSubData( GLuint( f.depth ), kTexture2D, 0, 0, 0, 0, GLuint( depthTex ), kTexture2D, 0, 0, 0, 0, f.vp[2], f.vp[3], 1 );
		S.copies++;
	}
	const Shared * ds = depthTex ? sharedFind( flDepthKey() ) : nullptr;
	if ( depthTex && ( !ds || !ds->view ) )
		return refuse( "the opaque depth's twin has no view" );
	// the table's rows, the push block, the sets (no tone pass is in flight: toneRun waited its fence)
	std::memcpy( F.stagePtr, f.rows, size_t( f.n ) * 4 * size_t( kFlareRowBytes ) );
	float c[7][4] = {};
	for ( int k = 0; k < 4; k++ )
		for ( int r = 0; r < 4; r++ )
			c[k][r] = f.proj[k * 4 + r];
	c[4][0] = float( f.vp[2] );
	c[4][1] = float( f.vp[3] );
	c[4][2] = f.slack;
	const std::int32_t ints[4] = { red, 0, 0, 0 };
	std::memcpy( F.push, c, sizeof( c ) );
	std::memcpy( F.push + sizeof( c ), ints, sizeof( ints ) );
	VkDescriptorImageInfo ii[kFlareTexMax][4] = {};
	VkWriteDescriptorSet ws[kFlareTexMax * 4] = {};
	int nw = 0;
	for ( int k = 0; k < nNames; k++ ) {
		ii[k][0].imageView = views[k];
		ii[k][1].imageView = F.tableView;
		ii[k][2].imageView = ds ? ds->view : F.tableView;  // unoccluded: the shader reads no depth
		for ( int j = 0; j < 3; j++ )
			ii[k][j].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
		ii[k][3].sampler = smps[k];
		static const std::uint32_t kBind[4] = { 0, 1, 2, 5 };
		for ( int j = 0; j < 4; j++ ) {
			VkWriteDescriptorSet & x = ws[nw++];
			x.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			x.dstSet = F.set[k];
			x.dstBinding = kBind[j];
			x.descriptorCount = 1;
			x.descriptorType = j == 3 ? VK_DESCRIPTOR_TYPE_SAMPLER : VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
			x.pImageInfo = &ii[k][j];
		}
	}
	v.vkUpdateDescriptorSets( v.dev, std::uint32_t( nw ), ws, 0, nullptr );
	for ( int k = 0; k < nNames; k++ )
		flDefAdd( GLuint( names[k] ) );
	if ( depthTex )
		flDefAdd( GLuint( depthTex ) );
	std::memcpy( F.setOf, setOf, size_t( f.n ) );
	std::memcpy( F.vp, f.vp, sizeof( F.vp ) );
	F.n = f.n;
	const double ms = nowMs() - t0;
	S.runs++;
	S.cpuMs += ms;
	S.maxMs = std::max( S.maxMs, ms );
	return true;
}

void toneFlareOnGl( const char * why )
{
	FxStat & S = flare().st;
	S.gl++;
	S.glWhy = why ? why : "";
}

FxStat takeToneFlareStat()
{
	Flare & F = flare();
	FxStat s = F.st;
	s.pipelines = F.pipelines;
	s.buildMs = F.buildMs;
	if ( F.broken && s.failWhy.empty() )
		s.failWhy = F.brokenWhy;
	F.st = FxStat();
	return s;
}
}  // namespace WwVkPresent
