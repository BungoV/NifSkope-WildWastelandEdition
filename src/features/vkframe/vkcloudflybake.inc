/* lane VKCLOUD5 (2026-11-16): the fly-through box's bake (cloudfly.cpp runPass) on the present device, through the VKAIR1
 * crossing (fx(): its fence, command buffer and semaphores), fxWork kind 19. Included once by vkpresent.cpp after
 * vkcloudfront.inc. No Qt (WW_VKPRESENT_NO_QT).
 * GL: one quad a slice k in [from, to) into layer k of a 3D R8 target (glFramebufferTextureLayer, cfLayer = k), viewport
 * nxy x nxy, no blend / depth / stencil. Pass 0 cloud_flybake (cvShape, cvDetail, cvMap on t0..t2), passes 1..3
 * cloud_flysdf (cfSrc, a 3D R8 target, on t0). Here: one render pass a slice over sliceView( dst, k ) (LOAD / STORE,
 * GENERAL), b31 a dynamic uniform buffer (one slot a slice, cfLayer patched), the full-screen triangle, no y flip
 * (gl_FragCoord.y = GL's). The slice views and framebuffers live until the next run's fence wait (or flyBakeDrop). */
namespace
{
constexpr int kFlyBakeBindings = 7;  // b31 (dynamic); t64..t66; s128..s130 (vkframe_spirv.sh fx shifts)
constexpr int kFlyBakeSlots = 64;    // slices a run (the Ultra preset is 40 deep)
constexpr VkDeviceSize kFlyBakeStride = 768;  // 672 rounded up to 256 (the largest minUniformBufferOffsetAlignment)

struct FlyBakeJob
{
	bool made = false, broken = false;
	std::string brokenWhy;
	VkDescriptorSetLayout dsl = VK_NULL_HANDLE;
	VkDescriptorPool dpool = VK_NULL_HANDLE;
	VkDescriptorSet set[WwVkPresent::kFlyBakePasses] = {};  // lane VKCLOUD9: one a run of the crossing
	VkPipelineLayout layout = VK_NULL_HANDLE;
	VkShaderModule ps[2] = {};  // 0 cloud_flybake, 1 cloud_flysdf
	VkBuffer ubo = VK_NULL_HANDLE;
	VkDeviceMemory uboMem = VK_NULL_HANDLE;
	std::uint8_t * uboPtr = nullptr;
	VkRenderPass pass = VK_NULL_HANDLE;
	VkPipeline pipe[2] = {};
	int pipelines = 0;
	double buildMs = 0.0;
	// this crossing's job (the drawing thread writes it before the hand, the worker records it). Lane VKCLOUD9: up to
	// kFlyBakePasses runs, run b's slices at [b * kFlyBakeSlots, + its count) of views / fbs / the b31 slots
	struct Rec
	{
		int n = 0, w = 0, h = 0, prog = 0;
		int set = 0, slot0 = 0;  // the descriptor set and the first b31 slot it records with (red 32: run 0's)
	};
	static constexpr int kSlices = kFlyBakeSlots * WwVkPresent::kFlyBakePasses;
	VkImageView views[kSlices] = {};
	VkFramebuffer fbs[kSlices] = {};
	Rec rec[WwVkPresent::kFlyBakePasses];
	int nrec = 0;
	int n = 0;  // slices of every run (0: nothing to free)
	WwVkPresent::FxStat st;
};

FlyBakeJob & flyBakeJob()
{
	static FlyBakeJob d;
	return d;
}

// the slice views and framebuffers of the last run (its crossing is done: the caller waited the fence)
void flyBFreeSlices()
{
	Vk & v = vk();
	FlyBakeJob & D = flyBakeJob();
	for ( int i = 0; i < FlyBakeJob::kSlices; i++ ) {
		if ( D.fbs[i] )
			v.vkDestroyFramebuffer( v.dev, D.fbs[i], nullptr );
		if ( D.views[i] )
			v.vkDestroyImageView( v.dev, D.views[i], nullptr );
		D.fbs[i] = VK_NULL_HANDLE;
		D.views[i] = VK_NULL_HANDLE;
	}
	D.n = 0;
	D.nrec = 0;
}

bool flyBStatic( std::string & why )
{
	Vk & v = vk();
	Fx & F = fx();
	FlyBakeJob & D = flyBakeJob();
	if ( D.made )
		return true;
	const double t0 = nowMs();
	static const std::uint32_t kBind[kFlyBakeBindings] = { 31, 64, 65, 66, 128, 129, 130 };
	VkDescriptorSetLayoutBinding b[kFlyBakeBindings] = {};
	for ( int k = 0; k < kFlyBakeBindings; k++ ) {
		b[k].binding = kBind[k];
		b[k].descriptorType = k == 0 ? VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC : k < 4 ? VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE : VK_DESCRIPTOR_TYPE_SAMPLER;
		b[k].descriptorCount = 1;
		b[k].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	}
	VkDescriptorSetLayoutCreateInfo lci { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
	lci.bindingCount = kFlyBakeBindings;
	lci.pBindings = b;
	constexpr std::uint32_t kP = WwVkPresent::kFlyBakePasses;
	VkDescriptorPoolSize ps[3] = { { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, kP }, { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 3 * kP },
		{ VK_DESCRIPTOR_TYPE_SAMPLER, 3 * kP } };
	VkDescriptorPoolCreateInfo pci { VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
	pci.maxSets = kP;
	pci.poolSizeCount = 3;
	pci.pPoolSizes = ps;
	if ( v.vkCreateDescriptorSetLayout( v.dev, &lci, nullptr, &D.dsl ) != VK_SUCCESS
		|| v.vkCreateDescriptorPool( v.dev, &pci, nullptr, &D.dpool ) != VK_SUCCESS ) {
		why = "the fly bake's descriptor set layout / pool failed";
		return false;
	}
	VkDescriptorSetLayout dsls[kP];
	for ( VkDescriptorSetLayout & l : dsls )
		l = D.dsl;
	VkDescriptorSetAllocateInfo dai { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
	dai.descriptorPool = D.dpool;
	dai.descriptorSetCount = kP;
	dai.pSetLayouts = dsls;
	VkPipelineLayoutCreateInfo plci { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
	plci.setLayoutCount = 1;
	plci.pSetLayouts = &D.dsl;
	if ( v.vkAllocateDescriptorSets( v.dev, &dai, D.set ) != VK_SUCCESS
		|| v.vkCreatePipelineLayout( v.dev, &plci, nullptr, &D.layout ) != VK_SUCCESS ) {
		why = "the fly bake's descriptor set / pipeline layout failed";
		return false;
	}
	if ( !fxModule( vkframe_cloud_flybake_view_ps_spv, sizeof( vkframe_cloud_flybake_view_ps_spv ), D.ps[0] )
		|| !fxModule( vkframe_cloud_flysdf_view_ps_spv, sizeof( vkframe_cloud_flysdf_view_ps_spv ), D.ps[1] ) ) {
		why = "the fly bake's shader modules failed";
		return false;
	}
	VkBufferCreateInfo bci { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
	bci.size = kFlyBakeStride * VkDeviceSize( FlyBakeJob::kSlices );
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
		why = "the fly bake's uniform buffer failed";
		return false;
	}
	std::memset( D.uboPtr, 0, size_t( bci.size ) );
	// the render pass: one R8 slice, LOAD / STORE (a slice not drawn keeps its texels), GENERAL
	VkAttachmentDescription at {};
	at.format = VK_FORMAT_R8_UNORM;
	at.samples = VK_SAMPLE_COUNT_1_BIT;
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
		why = "vkCreateRenderPass (the fly bake) failed";
		return false;
	}
	for ( int p = 0; p < 2; p++ ) {
		VkPipelineShaderStageCreateInfo st[2] = { { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO },
			{ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO } };
		st[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
		st[0].module = F.vs;  // the full-screen triangle; GL's quad covers the same pixels
		st[0].pName = "main";
		st[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
		st[1].module = D.ps[p];
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
		gp.layout = D.layout;
		gp.renderPass = D.pass;
		if ( v.vkCreateGraphicsPipelines( v.dev, tone().cache, 1, &gp, nullptr, &D.pipe[p] ) != VK_SUCCESS ) {
			why = "vkCreateGraphicsPipelines (the fly bake) failed";
			return false;
		}
		D.pipelines++;
	}
	toneCacheSave();
	D.buildMs += nowMs() - t0;
	D.made = true;
	return true;
}

// a 3D R8 shared target with slice views, or why not
const Shared * flyBVolume( std::uintptr_t key, const char * what, std::string & why )
{
	const Shared * s = key ? sharedFind( key ) : nullptr;
	if ( !s || !s->img || !s->view )
		why = std::string( what ) + " is not a shared target";
	else if ( s->type != 1 || !s->slices )
		why = std::string( what ) + " is not a 3D shared target with slice views";
	else if ( s->fmt != VK_FORMAT_R8_UNORM )
		why = std::string( what ) + " is not R8";
	else
		return s;
	return nullptr;
}

// the worker (fxWork, F.kind 19): one render pass a slice
bool flyBakeRecord( std::string & why )
{
	Vk & v = vk();
	Fx & F = fx();
	FlyBakeJob & D = flyBakeJob();
	if ( D.nrec < 1 ) {
		why = "no slices for the fly bake";
		return false;
	}
	for ( int b = 0; b < D.nrec; b++ ) {
		const FlyBakeJob::Rec & R = D.rec[b];
		if ( R.n < 1 || !D.pipe[R.prog] ) {
			why = "no slices / pipeline for the fly bake's run " + std::to_string( b );
			return false;
		}
		// the source written by an earlier crossing or run (passes 1..3) and the target written by GL or an earlier run (lane
		// VKCLOUD9: before every run of the crossing: the next pass reads the last one's volume, or overwrites one it read)
		VkMemoryBarrier mb { VK_STRUCTURE_TYPE_MEMORY_BARRIER };
		mb.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT;
		mb.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		v.vkCmdPipelineBarrier( F.cmd, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
			VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 1, &mb, 0, nullptr, 0, nullptr );
		const VkViewport vpt { 0.0f, 0.0f, float( R.w ), float( R.h ), 0.0f, 1.0f };
		const VkRect2D sc { { 0, 0 }, { std::uint32_t( R.w ), std::uint32_t( R.h ) } };
		const int base = b * kFlyBakeSlots;
		for ( int i = 0; i < R.n; i++ ) {
			if ( !D.fbs[base + i] )
				continue;  // red 2's last slice (no slice past the end)
			VkRenderPassBeginInfo rbi { VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
			rbi.renderPass = D.pass;
			rbi.framebuffer = D.fbs[base + i];
			rbi.renderArea = sc;
			v.vkCmdBeginRenderPass( F.cmd, &rbi, VK_SUBPASS_CONTENTS_INLINE );
			v.vkCmdSetViewport( F.cmd, 0, 1, &vpt );
			v.vkCmdSetScissor( F.cmd, 0, 1, &sc );
			v.vkCmdBindPipeline( F.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, D.pipe[R.prog] );
			const std::uint32_t off = std::uint32_t( kFlyBakeStride * VkDeviceSize( R.slot0 + i ) );
			v.vkCmdBindDescriptorSets( F.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, D.layout, 0, 1, &D.set[R.set], 1, &off );
			v.vkCmdDraw( F.cmd, 3, 1, 0, 0 );
			v.vkCmdEndRenderPass( F.cmd );
			if ( i + 1 < R.n ) {  // red 1 draws every slice into one: keep GL's draw order between them
				VkMemoryBarrier ob { VK_STRUCTURE_TYPE_MEMORY_BARRIER };
				ob.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
				ob.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
				v.vkCmdPipelineBarrier( F.cmd, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 1, &ob,
					0, nullptr, 0, nullptr );
			}
		}
	}
	return true;
}

void flyBakeDrop()
{
	Vk & v = vk();
	FlyBakeJob & D = flyBakeJob();
	if ( !D.n )
		return;
	v.vkDeviceWaitIdle( v.dev );
	fx().pending = false;
	flyBFreeSlices();
}

// one run's checks, nothing written (lane VKCLOUD9: every run of a crossing is checked before any is recorded)
bool flyBCheck( const WwVkPresent::FlyBakeRun & t, const Shared *& dst, const Shared *& src, std::string & why )
{
	src = nullptr;
	dst = nullptr;
	if ( t.pass < 0 || t.pass > 3 ) {
		why = "pass " + std::to_string( t.pass ) + " (0 the bake, 1..3 the distance passes)";
		return false;
	}
	if ( t.from < 0 || t.to <= t.from ) {
		why = "an empty slice range";
		return false;
	}
	if ( t.to - t.from > kFlyBakeSlots ) {
		why = "more than " + std::to_string( kFlyBakeSlots ) + " slices in one run";
		return false;
	}
	dst = flyBVolume( t.dst, "the target", why );
	if ( !dst )
		return false;
	if ( t.to > dst->d ) {
		why = "slices to " + std::to_string( t.to ) + " of a target " + std::to_string( dst->d ) + " deep";
		return false;
	}
	if ( t.pass > 0 ) {
		src = flyBVolume( t.src, "the distance pass's source", why );
		if ( !src )
			return false;
		if ( src == dst ) {
			why = "the distance pass reads its own target";
			return false;
		}
		if ( src->w != dst->w || src->h != dst->h || src->d != dst->d ) {
			why = "the distance pass's source is not the target's size";
			return false;
		}
	}
	return true;
}
}  // namespace

namespace WwVkPresent
{
bool flyBakeRun( const FlyBakeRun & t, std::string & why )
{
	return flyBakeRunN( &t, 1, why );
}

bool flyBakeRunN( const FlyBakeRun * t, int n, std::string & why )
{
	Fx & F = fx();
	FlyBakeJob & D = flyBakeJob();
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
		D.brokenWhy = "the fly bake on Vulkan broke: " + w;
		return refuse( D.brokenWhy );
	};
	if ( D.broken )
		return refuse( D.brokenWhy );
	if ( !t || n < 1 || n > kFlyBakePasses )
		return refuse( "a crossing of " + std::to_string( n ) + " runs (1.." + std::to_string( kFlyBakePasses ) + ")" );
	if ( n > 1 && ( t[0].red & 16 ) ) {  // red 16 (gates): a crossing a run, the submit before lane VKCLOUD9
		for ( int b = 0; b < n; b++ ) {
			FlyBakeRun one = t[b];
			one.red &= ~16;
			if ( !flyBakeRunN( &one, 1, why ) )
				return false;
		}
		return true;
	}
	if ( !v.ok || !g.ok || g.ctx != reinterpret_cast<void *>( wglGetCurrentContext() ) )
		return refuse( "the present device or GL's entry points are not ready in this context" );
	std::string w;
	const Shared * dsts[kFlyBakePasses] = {};
	const Shared * srcs[kFlyBakePasses] = {};
	for ( int b = 0; b < n; b++ )
		if ( !flyBCheck( t[b], dsts[b], srcs[b], w ) )
			return refuse( ( n > 1 ? "run " + std::to_string( b ) + ": " : std::string() ) + w );
	// the last crossing (any fx pass) is done before its command buffer, sets, framebuffers and buffers are written again
	if ( F.pending ) {
		v.vkWaitForFences( v.dev, 1, &F.fence, VK_TRUE, 1000000000ull );
		F.pending = false;
	}
	flyBFreeSlices();
	if ( !fxStatic( w ) || !fxSems( w ) || !flyBStatic( w ) )
		return breakOff( w );
	FxNames names;
	const bool oneSet = ( t[0].red & 32 ) != 0;  // red 32 (gates): every run records with run 0's set and b31 slots
	for ( int b = 0; b < n; b++ ) {
	const FlyBakeRun & r = t[b];
	const Shared * dst = dsts[b];
	const Shared * src = srcs[b];
	names.add( dst->tex );
	VkImageView views[3] = { F.d3, F.d3, F.d2 };
	int smp[3] = {};
	if ( r.pass == 0 ) {
		for ( int k = 0; k < kFlyBakeTex; k++ ) {
			// lane VKCLOUD9: an input already on the present device is read in place (as cloudRun); red 8 skips the lookup
			FxTex x = r.tex[k];
			if ( !x.shared && x.tex && !( r.red & 8 ) ) {
				const std::uintptr_t sk = farSharedOf( x.tex );
				const Shared * s = sk ? sharedFind( sk ) : nullptr;
				if ( s && s->type == x.type && s->samples == 1 && s->levels == std::max( x.levels, 1 ) )
					x.shared = sk;
			}
			if ( !fxSlot( x, views[k], names, S.copies, w ) )
				return refuse( "the fly bake's input " + std::to_string( k ) + ": " + w );
			smp[k] = x.sampler;
			if ( x.shared )  // lane VKCLOUD10: shape, detail, map = kInShape, kInDetail, kInMap
				S.inPlace |= 1u << k;
		}
	} else {
		views[0] = src->view;
		names.add( src->tex );
	}
	// red 4: the other pass's program, b31 zero but cfLayer
	const int prog = ( r.pass == 0 ? 0 : 1 ) ^ ( ( r.red & 4 ) ? 1 : 0 );
	const int bytes = r.pass == 0 ? kT_flybakeBytes : kT_flysdfBytes;
	const int layerAt = prog == 0 ? kT_flybakeLayerAt : kT_flysdfLayerAt;
	VkFramebufferCreateInfo fci { VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
	fci.renderPass = D.pass;
	fci.attachmentCount = 1;
	fci.width = std::uint32_t( dst->w );
	fci.height = std::uint32_t( dst->h );
	fci.layers = 1;
	FlyBakeJob::Rec & R = D.rec[b];
	R = FlyBakeJob::Rec();
	R.n = r.to - r.from;
	R.w = dst->w;
	R.h = dst->h;
	R.prog = prog;
	R.set = oneSet ? 0 : b;
	R.slot0 = oneSet ? 0 : b * kFlyBakeSlots;
	D.nrec = b + 1;
	D.n += R.n;
	const int base = b * kFlyBakeSlots;
	for ( int i = 0; i < R.n; i++ ) {
		const int k = r.from + i;
		const int z = ( r.red & 1 ) ? 0 : ( r.red & 2 ) ? k + 1 : k;  // red 1 every slice into slice 0, red 2 slice k into k + 1
		std::uint8_t * slot = D.uboPtr + kFlyBakeStride * VkDeviceSize( base + i );
		std::memset( slot, 0, size_t( kFlyBakeStride ) );
		if ( !( r.red & 4 ) )
			std::memcpy( slot, r.loose, size_t( bytes ) );
		std::memcpy( slot + layerAt, &k, 4 );
		if ( z >= dst->d )
			continue;
		D.views[base + i] = sliceView( *dst, z, w );
		if ( !D.views[base + i] )
			return breakOff( w );
		fci.pAttachments = &D.views[base + i];
		if ( v.vkCreateFramebuffer( v.dev, &fci, nullptr, &D.fbs[base + i] ) != VK_SUCCESS )
			return breakOff( "the fly bake's slice framebuffer failed" );
	}
	VkDescriptorBufferInfo bi { D.ubo, 0, VkDeviceSize( kT_flybakeBytes ) };
	VkDescriptorImageInfo ii[6] = {};
	VkWriteDescriptorSet ws[kFlyBakeBindings] = {};
	for ( int k = 0; k < kFlyBakeBindings; k++ ) {
		VkWriteDescriptorSet & x = ws[k];
		x.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		x.dstSet = D.set[b];
		x.descriptorCount = 1;
		if ( k == 0 ) {
			x.dstBinding = 31;
			x.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
			x.pBufferInfo = &bi;
		} else if ( k < 4 ) {
			ii[k - 1].imageView = views[k - 1];
			ii[k - 1].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
			x.dstBinding = std::uint32_t( 64 + k - 1 );
			x.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
			x.pImageInfo = &ii[k - 1];
		} else {
			ii[k - 1].sampler = F.smp[std::clamp( smp[k - 4], 0, int( kFxSamplers ) - 1 )];
			x.dstBinding = std::uint32_t( 128 + k - 4 );
			x.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
			x.pImageInfo = &ii[k - 1];
		}
	}
	v.vkUpdateDescriptorSets( v.dev, std::uint32_t( kFlyBakeBindings ), ws, 0, nullptr );
	}
	F.kind = 19;
	F.w = D.rec[0].w;
	F.h = D.rec[0].h;
	if ( !fxCross( names, w ) )
		return breakOff( w );
	const double ms = nowMs() - t0;
	S.crossings++;
	S.runs += n;
	S.cpuMs += ms;
	S.maxMs = std::max( S.maxMs, ms );
	return true;
}

void flyBakeOnGl( const char * why )
{
	WwVkPresent::glGuard( "flyBake", 1 );  // lane VKFLIGHT4: the view GL guard
	FxStat & S = flyBakeJob().st;
	S.gl++;
	S.glWhy = why ? why : "";
}

FxStat takeFlyBakeStat()
{
	FlyBakeJob & D = flyBakeJob();
	FxStat s = D.st;
	s.pipelines = D.pipelines;
	s.buildMs = D.buildMs;
	if ( D.broken && s.failWhy.empty() )
		s.failWhy = D.brokenWhy;
	D.st = FxStat();
	return s;
}
}  // namespace WwVkPresent
