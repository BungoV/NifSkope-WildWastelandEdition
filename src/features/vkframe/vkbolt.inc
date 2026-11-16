/* Lane VKOBJ15 (2026-11-16, census row 146): the lightning strips' preview (controllers.cpp ProcLightningController::drawPreview,
 * boltstrip.prog) on the present device, into the tone frame through the VKAIR1 crossing (fx(): its fence, command buffer,
 * semaphores, the air's render pass over the frame). Included at the end of vkpresent.cpp after vkobjvel.inc (its objvel::ovBase
 * reads a texture-cache twin with the twin's own view and sampler).
 * GL's state (drawPreview): blend SRC_ALPHA / ONE, depth test with the scene's func, no depth write; the scene's culling and
 * front face (vkbolt.h boltStateOf refuses every other state). GL_TRIANGLES from three CPU arrays (pos, colour, uv). */
#include "features/vkframe/vkbolt.h"
#include "features/vkframe/bolt_view_vs.spv.h"
#include "features/vkframe/bolt_view_ps.spv.h"

namespace
{
constexpr VkDeviceSize kBoltVsLooseAt = 512;  // boltstrip_vs b31 (binding 287) past b0 (336)
constexpr VkDeviceSize kBoltPsLooseAt = 768;  // boltstrip_ps b31 (binding 31)
constexpr int kBoltPipes = 8 * 4 * 2 * 2;     // depth op x cull x front face x red no blend

struct Bolt
{
	bool made = false, broken = false;
	std::string brokenWhy;
	PFN_vkCmdBindVertexBuffers bindVb = nullptr;  // not in the device table (Vk): loaded here
	VkDescriptorSetLayout dsl = VK_NULL_HANDLE;
	VkDescriptorPool dpool = VK_NULL_HANDLE;
	VkDescriptorSet set = VK_NULL_HANDLE;
	VkPipelineLayout layout = VK_NULL_HANDLE;
	VkShaderModule vs = VK_NULL_HANDLE, ps = VK_NULL_HANDLE;
	VkBuffer ubo = VK_NULL_HANDLE;  // VS b0 at 0, VS b31 at kBoltVsLooseAt, PS b31 at kBoltPsLooseAt
	VkDeviceMemory uboMem = VK_NULL_HANDLE;
	std::uint8_t * uboPtr = nullptr;
	VkBuffer vb = VK_NULL_HANDLE;  // per vertex: n positions, n colours, n uvs (each section on 16 bytes)
	VkDeviceMemory vbMem = VK_NULL_HANDLE;
	std::uint8_t * vbPtr = nullptr;
	VkDeviceSize vbCap = 0;
	VkPipeline pipe[kBoltPipes] = {};  // made on first use, dropped when the air's render pass or samples change
	VkRenderPass pipePass = VK_NULL_HANDLE;
	int pipeSamples = 0, pipelines = 0;
	double buildMs = 0.0;
	// this run's job (the drawing thread writes it before the hand, the worker records it)
	int n = 0, pipeIx = 0;
	VkDeviceSize colAt = 0, uvAt = 0;
	WwVkPresent::FxStat st;
};

Bolt & bolt()
{
	static Bolt d;
	return d;
}

bool boltStatic( std::string & why )
{
	Vk & v = vk();
	Bolt & D = bolt();
	if ( D.made )
		return true;
	const double t0 = nowMs();
	D.bindVb = reinterpret_cast<PFN_vkCmdBindVertexBuffers>( v.vkGetDeviceProcAddr( v.dev, "vkCmdBindVertexBuffers" ) );
	if ( !D.bindVb ) {
		why = "no vkCmdBindVertexBuffers on the present device";
		return false;
	}
	// VS b0 (256) and b31 (287), PS b31 (31), t0 boltTexture / t1 GreyscaleMap at 64.., s0 / s1 at 128.. (vkframe_spirv.sh's shifts)
	const std::uint32_t at[7] = { 256, 287, 31, 64, 65, 128, 129 };
	VkDescriptorSetLayoutBinding b[7] = {};
	for ( int k = 0; k < 7; k++ ) {
		b[k].binding = at[k];
		b[k].descriptorType = k < 3 ? VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER : k < 5 ? VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE : VK_DESCRIPTOR_TYPE_SAMPLER;
		b[k].descriptorCount = 1;
		b[k].stageFlags = k < 2 ? VK_SHADER_STAGE_VERTEX_BIT : VK_SHADER_STAGE_FRAGMENT_BIT;
	}
	VkDescriptorSetLayoutCreateInfo lci { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
	lci.bindingCount = 7;
	lci.pBindings = b;
	VkDescriptorPoolSize ps[3] = { { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 3 }, { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 2 }, { VK_DESCRIPTOR_TYPE_SAMPLER, 2 } };
	VkDescriptorPoolCreateInfo pci { VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
	pci.maxSets = 1;
	pci.poolSizeCount = 3;
	pci.pPoolSizes = ps;
	if ( v.vkCreateDescriptorSetLayout( v.dev, &lci, nullptr, &D.dsl ) != VK_SUCCESS
		|| v.vkCreateDescriptorPool( v.dev, &pci, nullptr, &D.dpool ) != VK_SUCCESS ) {
		why = "the lightning strips' descriptor set layout / pool failed";
		return false;
	}
	VkDescriptorSetAllocateInfo dai { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
	dai.descriptorPool = D.dpool;
	dai.descriptorSetCount = 1;
	dai.pSetLayouts = &D.dsl;
	VkPipelineLayoutCreateInfo plci { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
	plci.setLayoutCount = 1;
	plci.pSetLayouts = &D.dsl;
	if ( v.vkAllocateDescriptorSets( v.dev, &dai, &D.set ) != VK_SUCCESS
		|| v.vkCreatePipelineLayout( v.dev, &plci, nullptr, &D.layout ) != VK_SUCCESS ) {
		why = "the lightning strips' descriptor set / pipeline layout failed";
		return false;
	}
	if ( !fxModule( vkframe_bolt_view_vs_spv, sizeof( vkframe_bolt_view_vs_spv ), D.vs )
		|| !fxModule( vkframe_bolt_view_ps_spv, sizeof( vkframe_bolt_view_ps_spv ), D.ps ) ) {
		why = "the lightning strips' shader modules failed";
		return false;
	}
	VkBufferCreateInfo bci { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
	bci.size = kBoltPsLooseAt + VkDeviceSize( WwVkPresent::kT_boltPsBytes );
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
		why = "the lightning strips' uniform buffer failed";
		return false;
	}
	std::memset( D.uboPtr, 0, size_t( bci.size ) );
	D.buildMs += nowMs() - t0;
	D.made = true;
	return true;
}

// the vertex buffer, grown (the last run's fence already waited)
bool boltVertices( VkDeviceSize need, std::string & why )
{
	Vk & v = vk();
	Bolt & D = bolt();
	if ( D.vb && D.vbCap >= need )
		return true;
	if ( D.vb )
		v.vkDestroyBuffer( v.dev, D.vb, nullptr );
	if ( D.vbMem )
		v.vkFreeMemory( v.dev, D.vbMem, nullptr );
	D.vb = VK_NULL_HANDLE;
	D.vbMem = VK_NULL_HANDLE;
	D.vbPtr = nullptr;
	D.vbCap = 0;
	const VkDeviceSize cap = std::max<VkDeviceSize>( need + need / 2, 64 * 1024 );
	VkBufferCreateInfo bci { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
	bci.size = cap;
	bci.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
	bool ok = v.vkCreateBuffer( v.dev, &bci, nullptr, &D.vb ) == VK_SUCCESS;
	if ( ok ) {
		VkMemoryRequirements br {};
		v.vkGetBufferMemoryRequirements( v.dev, D.vb, &br );
		VkMemoryAllocateInfo ai { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
		ai.allocationSize = br.size;
		ai.memoryTypeIndex = memType( br.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT );
		void * p = nullptr;
		ok = ai.memoryTypeIndex != UINT32_MAX && v.vkAllocateMemory( v.dev, &ai, nullptr, &D.vbMem ) == VK_SUCCESS
			&& v.vkBindBufferMemory( v.dev, D.vb, D.vbMem, 0 ) == VK_SUCCESS && v.vkMapMemory( v.dev, D.vbMem, 0, VK_WHOLE_SIZE, 0, &p ) == VK_SUCCESS;
		D.vbPtr = static_cast<std::uint8_t *>( p );
	}
	if ( !ok || !D.vbPtr ) {
		why = "the lightning strips' vertex buffer (" + std::to_string( need ) + " bytes) failed";
		return false;
	}
	D.vbCap = cap;
	return true;
}

// pipeline ix = ( ( op * 4 + cull ) * 2 + ccw ) * 2 + noBlend over the air's render pass (made on first use)
bool boltPipe( int ix, std::string & why )
{
	Vk & v = vk();
	Fx & F = fx();
	Bolt & D = bolt();
	if ( D.pipePass != F.airPass || D.pipeSamples != F.airSamples ) {
		for ( VkPipeline & p : D.pipe )
			if ( p ) {
				v.vkDestroyPipeline( v.dev, p, nullptr );
				p = VK_NULL_HANDLE;
			}
		D.pipePass = F.airPass;
		D.pipeSamples = F.airSamples;
	}
	if ( D.pipe[ix] )
		return true;
	const double t0 = nowMs();
	const int noBlend = ix & 1, ccw = ( ix >> 1 ) & 1, cull = ( ix >> 2 ) & 3, op = ix >> 4;
	VkPipelineShaderStageCreateInfo st[2] = { { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO },
		{ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO } };
	st[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
	st[0].module = D.vs;
	st[0].pName = "main";
	st[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	st[1].module = D.ps;
	st[1].pName = "main";
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
	rs.cullMode = VkCullModeFlags( cull );  // 1 front, 2 back, 3 both (Vulkan's bits)
	// no y flip (GL's rows from the bottom = the image's from the top): GL's CCW front is Vulkan's clockwise
	rs.frontFace = ccw ? VK_FRONT_FACE_CLOCKWISE : VK_FRONT_FACE_COUNTER_CLOCKWISE;
	rs.lineWidth = 1.0f;
	VkPipelineMultisampleStateCreateInfo ms { VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
	ms.rasterizationSamples = VkSampleCountFlagBits( F.airSamples );
	VkPipelineDepthStencilStateCreateInfo ds { VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };
	ds.depthTestEnable = VK_TRUE;
	ds.depthWriteEnable = VK_FALSE;
	ds.depthCompareOp = VkCompareOp( op );  // GL's func - GL_NEVER
	VkPipelineColorBlendAttachmentState cb {};
	cb.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	if ( !noBlend ) {  // glBlendFunc( SRC_ALPHA, ONE )
		cb.blendEnable = VK_TRUE;
		cb.srcColorBlendFactor = cb.srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
		cb.dstColorBlendFactor = cb.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
		cb.colorBlendOp = cb.alphaBlendOp = VK_BLEND_OP_ADD;
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
	gp.layout = D.layout;
	gp.renderPass = F.airPass;
	if ( v.vkCreateGraphicsPipelines( v.dev, tone().cache, 1, &gp, nullptr, &D.pipe[ix] ) != VK_SUCCESS ) {
		D.pipe[ix] = VK_NULL_HANDLE;
		why = "vkCreateGraphicsPipelines (the lightning strips) failed";
		return false;
	}
	D.pipelines++;
	D.buildMs += nowMs() - t0;
	toneCacheSave();
	return true;
}

// the worker (fxWork, F.kind 17; VKMERGE1: was 5, taken by the far LOD rings): the strips' triangles into the tone frame
bool boltRecord( std::string & why )
{
	Vk & v = vk();
	Fx & F = fx();
	Bolt & D = bolt();
	if ( !F.fbAir || !D.pipe[D.pipeIx] || !D.vb ) {
		why = "no framebuffer / pipeline / vertex buffer for the lightning strips";
		return false;
	}
	VkRenderPassBeginInfo rbi { VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
	rbi.renderPass = F.airPass;
	rbi.framebuffer = F.fbAir;
	rbi.renderArea = { { 0, 0 }, { std::uint32_t( F.w ), std::uint32_t( F.h ) } };
	v.vkCmdBeginRenderPass( F.cmd, &rbi, VK_SUBPASS_CONTENTS_INLINE );
	const VkViewport vpt { float( F.vp[0] ), float( F.vp[1] ), float( F.vp[2] ), float( F.vp[3] ), 0.0f, 1.0f };
	const int x0 = std::clamp( F.vp[0], 0, F.w ), y0 = std::clamp( F.vp[1], 0, F.h );
	const int x1 = std::clamp( F.vp[0] + F.vp[2], 0, F.w ), y1 = std::clamp( F.vp[1] + F.vp[3], 0, F.h );
	const VkRect2D sc { { x0, y0 }, { std::uint32_t( std::max( x1 - x0, 0 ) ), std::uint32_t( std::max( y1 - y0, 0 ) ) } };
	v.vkCmdSetViewport( F.cmd, 0, 1, &vpt );
	v.vkCmdSetScissor( F.cmd, 0, 1, &sc );
	v.vkCmdBindPipeline( F.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, D.pipe[D.pipeIx] );
	v.vkCmdBindDescriptorSets( F.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, D.layout, 0, 1, &D.set, 0, nullptr );
	const VkBuffer bufs[3] = { D.vb, D.vb, D.vb };
	const VkDeviceSize offs[3] = { 0, D.colAt, D.uvAt };
	D.bindVb( F.cmd, 0, 3, bufs, offs );
	v.vkCmdDraw( F.cmd, std::uint32_t( D.n ), 1, 0, 0 );
	v.vkCmdEndRenderPass( F.cmd );
	return true;
}
}  // namespace

namespace WwVkPresent
{
bool boltRun( const BoltRun & t, std::string & why )
{
	Fx & F = fx();
	Bolt & D = bolt();
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
		D.brokenWhy = "the lightning strips on Vulkan broke: " + w;
		return refuse( D.brokenWhy );
	};
	if ( D.broken )
		return refuse( D.brokenWhy );
	if ( !toneFrameIs( t.color ) || !T.colorView || !T.dsView )
		return refuse( "the draw target is not the shared tone frame" );
	if ( T.w != t.w || T.h != t.h )
		return refuse( "the strips' size is not the tone frame's" );
	if ( t.vp[2] < 1 || t.vp[3] < 1 )
		return refuse( "an empty viewport" );
	if ( t.n < 3 || t.n % 3 || !t.pos || !t.col || !t.uv )
		return refuse( "no triangles" );
	if ( t.depthFunc < 0x0200 || t.depthFunc > 0x0207 )
		return refuse( "GL's depth func " + std::to_string( t.depthFunc ) );
	if ( t.cullFace != 0 && t.cullFace != 0x0404 && t.cullFace != 0x0405 && t.cullFace != 0x0408 )
		return refuse( "GL's cull face " + std::to_string( t.cullFace ) );
	if ( !v.ok || !g.ok || g.ctx != reinterpret_cast<void *>( wglGetCurrentContext() ) )
		return refuse( "the present device or GL's entry points are not ready in this context" );
	if ( !v.clipControl )
		return refuse( "no depthClipControl on the present device (GL's -1..1 clip z)" );
	if ( !g.copyImageSubData )
		return refuse( "no glCopyImageSubData (the inputs' copies into their shared twins)" );
	// the textures first: a texture-cache twin with its own view and sampler, else a copy (fxSlot) with fxTexOf's sampler
	std::string w;
	objvel::OvBase tw[2];
	for ( int k = 0; k < 2; k++ )
		if ( t.tex[k].tex && objTexKey( t.tex[k].tex ) && !objvel::ovBase( t.tex[k].tex, tw[k], w ) )
			return refuse( std::string( k ? "GreyscaleMap" : "boltTexture" ) + ": " + w );
	// the last crossing (any fx pass) is done before its command buffer, sets, framebuffer and buffers are written again
	if ( F.pending ) {
		v.vkWaitForFences( v.dev, 1, &F.fence, VK_TRUE, 1000000000ull );
		F.pending = false;
	}
	const int samples = T.samples > 0 ? T.samples : std::max( F.airSamples, 1 );
	if ( !fxStatic( w ) || !fxSems( w ) || !fxPasses( samples, w ) || !boltStatic( w ) )
		return breakOff( w );
	if ( F.airSamples != T.samples )
		return breakOff( "the lightning strips' pipelines are not at the frame's sample count" );
	const int cull = t.cullFace == 0x0404 ? 1 : t.cullFace == 0x0405 ? 2 : t.cullFace == 0x0408 ? 3 : 0;
	const int ix = ( ( int( t.depthFunc - 0x0200 ) * 4 + cull ) * 2 + ( t.ccw ? 1 : 0 ) ) * 2 + ( ( t.red & 1 ) ? 1 : 0 );
	if ( !boltPipe( ix, w ) )
		return breakOff( w );
	if ( !objTexFlush( w ) )
		return refuse( "the texture cache's flush: " + w );
	const VkDeviceSize n = VkDeviceSize( t.n );
	const VkDeviceSize colAt = ( n * 12 + 15 ) / 16 * 16, uvAt = colAt + ( n * 16 + 15 ) / 16 * 16;
	if ( !boltVertices( uvAt + n * 8, w ) )
		return breakOff( w );
	FxNames names;
	names.add( T.colorTex );
	names.add( T.dsTex );
	VkImageView views[2] = {};
	VkSampler smps[2] = {};
	for ( int k = 0; k < 2; k++ ) {
		if ( tw[k].view ) {
			views[k] = tw[k].view;
			smps[k] = tw[k].smp;
			names.add( tw[k].gl );
			continue;
		}
		FxTex x = t.tex[k];
		if ( !fxSlot( x, views[k], names, S.copies, w ) )
			return refuse( std::string( k ? "GreyscaleMap" : "boltTexture" ) + ": " + w );
		smps[k] = F.smp[std::clamp( x.sampler, 0, int( kFxSamplers ) - 1 )];
	}
	const VkImageView att[2] = { T.colorView, T.dsView };
	if ( !fxFb( F.fbAir, F.airPass, att, 2, T.w, T.h ) )
		return breakOff( "the lightning strips' framebuffer over the tone frame failed" );
	// the vertices (red 4: u and v swapped), VS b0 / b31, PS b31 (red 2: the palette off)
	std::memcpy( D.vbPtr, t.pos, size_t( n * 12 ) );
	std::memcpy( D.vbPtr + colAt, t.col, size_t( n * 16 ) );
	if ( t.red & 4 ) {
		float * uv = reinterpret_cast<float *>( D.vbPtr + uvAt );
		for ( VkDeviceSize i = 0; i < n; i++ ) {
			uv[2 * i] = t.uv[2 * i + 1];
			uv[2 * i + 1] = t.uv[2 * i];
		}
	} else {
		std::memcpy( D.vbPtr + uvAt, t.uv, size_t( n * 8 ) );
	}
	std::uint8_t * u = D.uboPtr;
	std::memcpy( u, t.global, size_t( kFxGlobalBytes ) );
	std::memcpy( u + kBoltVsLooseAt, t.vsLoose, size_t( kT_boltVsBytes ) );
	std::memcpy( u + kBoltPsLooseAt, t.psLoose, size_t( kT_boltPsBytes ) );
	if ( t.red & 2 )
		std::memset( u + kBoltPsLooseAt, 0, 8 );
	const VkDescriptorBufferInfo bi[3] = { { D.ubo, 0, VkDeviceSize( kFxGlobalBytes ) }, { D.ubo, kBoltVsLooseAt, VkDeviceSize( kT_boltVsBytes ) },
		{ D.ubo, kBoltPsLooseAt, VkDeviceSize( kT_boltPsBytes ) } };
	VkDescriptorImageInfo ii[4] = {};
	VkWriteDescriptorSet ws[7] = {};
	const std::uint32_t at[7] = { 256, 287, 31, 64, 65, 128, 129 };
	for ( int k = 0; k < 7; k++ ) {
		ws[k].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		ws[k].dstSet = D.set;
		ws[k].dstBinding = at[k];
		ws[k].descriptorCount = 1;
		if ( k < 3 ) {
			ws[k].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			ws[k].pBufferInfo = &bi[k];
		} else if ( k < 5 ) {
			ii[k - 3].imageView = views[k - 3];
			ii[k - 3].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
			ws[k].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
			ws[k].pImageInfo = &ii[k - 3];
		} else {
			ii[k - 3].sampler = smps[k - 5];
			ws[k].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
			ws[k].pImageInfo = &ii[k - 3];
		}
	}
	v.vkUpdateDescriptorSets( v.dev, 7, ws, 0, nullptr );
	D.n = t.n;
	D.colAt = colAt;
	D.uvAt = uvAt;
	D.pipeIx = ix;
	F.kind = 17;  // VKMERGE1: was 5 (the far LOD rings); 0-16 are taken across the vkframe branches
	F.w = t.w;
	F.h = t.h;
	std::memcpy( F.vp, t.vp, sizeof( F.vp ) );
	if ( !fxCross( names, w ) )
		return breakOff( w );  // nothing signals vkDone: GL does not wait it; GL draws its strip this frame
	const double ms = nowMs() - t0;
	S.runs++;
	S.cpuMs += ms;
	S.maxMs = std::max( S.maxMs, ms );
	return true;
}

void boltOnGl( const char * why )
{
	FxStat & S = bolt().st;
	S.gl++;
	S.glWhy = why ? why : "";
}

FxStat takeBoltStat()
{
	Bolt & D = bolt();
	FxStat s = D.st;
	s.pipelines = D.pipelines;
	s.buildMs = D.buildMs;
	if ( D.broken && s.failWhy.empty() )
		s.failWhy = D.brokenWhy;
	D.st = FxStat();
	return s;
}
}  // namespace WwVkPresent
