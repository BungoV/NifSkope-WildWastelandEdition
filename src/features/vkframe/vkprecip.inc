/* lane VKFAR2 (row L8 slice 1, 2026-11-14): rain / snow (cell_precip_gpu) on the present device, into the tone frame and its
 * stencil, through the VKAIR1 crossing (fx(): its fence, command buffer, semaphores, the air's render pass over the frame).
 * Included once by vkpresent.cpp after vkfardots.inc (it reads that block's anonymous-namespace parts).
 * GL draws one GL_POINT a particle and its geometry stage makes the quad; the present device has no geometry stage, so a
 * compute pass (precip_view_cs.hlsl: the field, the cull, the geometry stage's per-particle part) writes one record a
 * particle in GL's order, and each particle is an instance of four strip vertices (precip_view_vs.hlsl) read by
 * cell_precip_gpu_ps.hlsl's frozen copy (precip_view_ps.hlsl). Nothing is compacted: the blend needs GL's order.
 * Slice 1 is unlit; slice 2a adds the air (wwFogFx's A / B in the record pass, its five textures in the compute set);
 * slice 3 the precipitation look (cell_precip_look.prog: precip_look_view_cs / _vs / _ps, a 192-byte record, the flake's
 * normal map at t2 of the pixel stage). Slice 2b lights the particles by the sun alone (precip_lit_view_cs, its own set);
 * the lamps, GI, ambient, shadow maps, drift, terrain air, local sun, interiors and the roof map are refused (GL draws).
 * GL's state (cellprecip.cpp): depth LEQUAL, no depth write, cull off, blend SRC_ALPHA / ONE_MINUS_SRC_ALPHA, stencil ALWAYS
 * ref 1, REPLACE on depth pass, write mask 0x03. */
namespace
{
constexpr VkDeviceSize kPrecipViewBytes = 96;   // precip_view_vs PrecipView (b1)
constexpr VkDeviceSize kPrecipCsAt = 256;       // the compute pass's b31
constexpr VkDeviceSize kPrecipPsAt = 2560;      // the pixel stage's b31 (past the compute block's 2112)
constexpr VkDeviceSize kPrecipRecBytes = 112;   // PrecipRec: 7 float4
constexpr VkDeviceSize kPrecipLookRecBytes = 192;  // the look's PrecipRec: 12 float4
// slice 2b: the lit record pass's b31 and its globalUniforms (b0, zeros: only the lamps read it, refused) past the rest
constexpr VkDeviceSize kPrecipLitCsAt = 2816;
constexpr VkDeviceSize kPrecipGlobalAt = 8704, kPrecipGlobalBytes = 336;
static_assert( kPrecipLitCsAt >= kPrecipPsAt + VkDeviceSize( WwVkPresent::kT_preciplookpsBytes ), "the lit block overlaps the pixel block" );
static_assert( kPrecipGlobalAt >= kPrecipLitCsAt + VkDeviceSize( WwVkPresent::kT_preciplitcsBytes ), "globalUniforms overlaps the lit block" );

struct Precip
{
	bool made = false, broken = false;
	std::string brokenWhy;
	VkDescriptorSetLayout csDsl = VK_NULL_HANDLE, gDsl = VK_NULL_HANDLE;
	VkDescriptorPool dpool = VK_NULL_HANDLE;
	VkDescriptorSet csSet = VK_NULL_HANDLE, gSet = VK_NULL_HANDLE;
	VkPipelineLayout csLayout = VK_NULL_HANDLE, gLayout = VK_NULL_HANDLE;
	VkShaderModule cs = VK_NULL_HANDLE, vs = VK_NULL_HANDLE, ps = VK_NULL_HANDLE;
	VkShaderModule csLook = VK_NULL_HANDLE, vsLook = VK_NULL_HANDLE, psLook = VK_NULL_HANDLE;  // slice 3
	VkPipeline csPipe = VK_NULL_HANDLE, csPipeLook = VK_NULL_HANDLE;
	VkBuffer ubo = VK_NULL_HANDLE;  // the vertex block at 0, the compute b31 at kPrecipCsAt, the pixel b31 at kPrecipPsAt
	VkDeviceMemory uboMem = VK_NULL_HANDLE;
	std::uint8_t * uboPtr = nullptr;
	VkBuffer rec = VK_NULL_HANDLE;  // the records, one a particle
	VkDeviceMemory recMem = VK_NULL_HANDLE;
	VkDeviceSize recCap = 0;
	bool recHost = false;  // gates only (PrecipRun::recOut): host-visible, mapped at recPtr
	std::uint8_t * recPtr = nullptr;
	VkPipeline pipe = VK_NULL_HANDLE, pipeLook = VK_NULL_HANDLE;
	VkRenderPass pipePass = VK_NULL_HANDLE;
	int pipeSamples = 0, pipelines = 0;
	double buildMs = 0.0;
	// this run's job (the drawing thread writes it before the hand, the worker records it)
	int n = 0;
	bool look = false;  // the look's pipelines
	bool lit = false;   // slice 2b: the lit record pass
	// slice 2b, made on the first lit run (a failure refuses lit runs only): its set (32 bindings), pipeline, and the zero
	// inputs of what the sun alone does not read (a D16 1x1 x6 image as the shadow cubes / CSM, a zero texel buffer)
	bool litMade = false, litBroken = false, litLaid = false;  // litLaid: the zero depth image cleared (the first lit record)
	std::string litWhy;
	VkDescriptorSetLayout litDsl = VK_NULL_HANDLE;
	VkDescriptorPool litPool = VK_NULL_HANDLE;
	VkDescriptorSet litSet = VK_NULL_HANDLE;
	VkPipelineLayout litLayout = VK_NULL_HANDLE;
	VkShaderModule csLit = VK_NULL_HANDLE;
	VkPipeline csPipeLit = VK_NULL_HANDLE;
	VkImage litDepth = VK_NULL_HANDLE;
	VkDeviceMemory litDepthMem = VK_NULL_HANDLE;
	VkImageView litCube = VK_NULL_HANDLE, litD2a = VK_NULL_HANDLE;
	VkSampler litCmp = VK_NULL_HANDLE;
	VkBuffer litTb = VK_NULL_HANDLE;
	VkDeviceMemory litTbMem = VK_NULL_HANDLE;
	VkBufferView litTbF = VK_NULL_HANDLE, litTbI = VK_NULL_HANDLE;
	WwVkPresent::FxStat st;
	std::map<std::uintptr_t, std::pair<unsigned, VkImage>> atlasCopied;  // a BC atlas twin's GL source and image once copied
};

Precip & precip()
{
	static Precip d;
	return d;
}

// a member's offset in a reflected block (-1: absent)
int precipAt( const WwVkPresent::FxField * f, int n, const char * name )
{
	for ( int i = 0; i < n; i++ )
		if ( std::strcmp( f[i].name, name ) == 0 )
			return f[i].offset;
	return -1;
}

/* the app's atlases are BC DDS (cellprecip.cpp loadDds: BC1 / BC3 / BC7, sRGB kept, every level, trilinear clamp), which
 * sharedTarget cannot twin (a colour attachment): the far cards' farTwin (vkfarcards.inc), every level copied once a GL
 * name, as fcSheet (the DDS never changes once loaded: loadDds fills a fresh name once per path and nothing re-uploads or
 * deletes it; a reload in place there must also drop this name's atlasCopied entry) */
bool precipAtlas( const WwVkPresent::FxTex & t, VkImageView & view, FxNames & names, int & copies, std::string & why )
{
	Gl & g = gl();
	unsigned twin = 0;
	if ( !t.twin ) {
		why = "an atlas with no twin key";
		return false;
	}
	if ( !WwVkPresent::farTwin( t.twin, 0, t.w, t.h, 1, t.glFmt, twin, why, t.levels ) )
		return false;
	const Shared * s = farTwinFind( t.twin );
	if ( !s || !s->view ) {
		why = "an atlas twin has no view";
		return false;
	}
	if ( s->levels != std::max( t.levels, 1 ) ) {
		why = "an atlas twin has " + std::to_string( s->levels ) + " levels, the atlas " + std::to_string( t.levels );
		return false;
	}
	auto it = precip().atlasCopied.find( t.twin );
	if ( it == precip().atlasCopied.end() || it->second.first != t.tex || it->second.second != s->img ) {
		for ( int l = 0; l < s->levels; l++ )
			g.copyImageSubData( t.tex, kTexture2D, l, 0, 0, 0, twin, kTexture2D, l, 0, 0, 0, std::max( t.w >> l, 1 ), std::max( t.h >> l, 1 ), 1 );
		precip().atlasCopied[t.twin] = { t.tex, s->img };
		copies++;
	}
	view = s->view;
	names.add( twin );
	return true;
}

bool precipStatic( std::string & why )
{
	Vk & v = vk();
	Precip & D = precip();
	if ( D.made )
		return true;
	const double t0 = nowMs();
	if ( !v.vkCmdDispatch || !v.vkCreateComputePipelines || !v.vkCmdPipelineBarrier ) {
		why = "no vkCmdDispatch / vkCreateComputePipelines / vkCmdPipelineBarrier";
		return false;
	}
	// compute: b31, t0 pcRoof / t1 pcTex at 64 / 65, s1 at 129, u0 the records at 192, the air's t25..t29 at 89..93 and
	// s25..s29 at 153..157 (vkframe_spirv.sh fxs()'s shifts)
	VkDescriptorSetLayoutBinding cb[15] = {};
	const std::uint32_t cBind[15] = { 31, 64, 65, 129, 192, 89, 90, 91, 92, 93, 153, 154, 155, 156, 157 };
	VkDescriptorType cType[15] = { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
		VK_DESCRIPTOR_TYPE_SAMPLER, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER };
	for ( int k = 5; k < 15; k++ )
		cType[k] = k < 10 ? VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE : VK_DESCRIPTOR_TYPE_SAMPLER;
	for ( int k = 0; k < 15; k++ ) {
		cb[k].binding = cBind[k];
		cb[k].descriptorType = cType[k];
		cb[k].descriptorCount = 1;
		cb[k].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	}
	// graphics: b1 the vertex block, t1 the records at 65 (vertex); b31, t0 pcTex at 64, s0 at 128, the look's t2 pcTexN at
	// 66 and s2 at 130 (pixel)
	VkDescriptorSetLayoutBinding gb[7] = {};
	const std::uint32_t gBind[7] = { 1, 65, 31, 64, 128, 66, 130 };
	const VkDescriptorType gType[7] = { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
		VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, VK_DESCRIPTOR_TYPE_SAMPLER, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, VK_DESCRIPTOR_TYPE_SAMPLER };
	for ( int k = 0; k < 7; k++ ) {
		gb[k].binding = gBind[k];
		gb[k].descriptorType = gType[k];
		gb[k].descriptorCount = 1;
		gb[k].stageFlags = k < 2 ? VK_SHADER_STAGE_VERTEX_BIT : VK_SHADER_STAGE_FRAGMENT_BIT;
	}
	VkDescriptorSetLayoutCreateInfo lci { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
	lci.bindingCount = 15;
	lci.pBindings = cb;
	VkDescriptorSetLayoutCreateInfo lgi = lci;
	lgi.bindingCount = 7;
	lgi.pBindings = gb;
	VkDescriptorPoolSize ps[4] = { { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 3 }, { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 9 },
		{ VK_DESCRIPTOR_TYPE_SAMPLER, 8 }, { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2 } };
	VkDescriptorPoolCreateInfo pci { VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
	pci.maxSets = 2;
	pci.poolSizeCount = 4;
	pci.pPoolSizes = ps;
	if ( v.vkCreateDescriptorSetLayout( v.dev, &lci, nullptr, &D.csDsl ) != VK_SUCCESS
		|| v.vkCreateDescriptorSetLayout( v.dev, &lgi, nullptr, &D.gDsl ) != VK_SUCCESS
		|| v.vkCreateDescriptorPool( v.dev, &pci, nullptr, &D.dpool ) != VK_SUCCESS ) {
		why = "the rain / snow descriptor set layouts / pool failed";
		return false;
	}
	const VkDescriptorSetLayout dsls[2] = { D.csDsl, D.gDsl };
	VkDescriptorSet sets[2] = {};
	VkDescriptorSetAllocateInfo dai { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
	dai.descriptorPool = D.dpool;
	dai.descriptorSetCount = 2;
	dai.pSetLayouts = dsls;
	VkPipelineLayoutCreateInfo plci { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
	plci.setLayoutCount = 1;
	plci.pSetLayouts = &D.csDsl;
	VkPipelineLayoutCreateInfo plgi = plci;
	plgi.pSetLayouts = &D.gDsl;
	if ( v.vkAllocateDescriptorSets( v.dev, &dai, sets ) != VK_SUCCESS
		|| v.vkCreatePipelineLayout( v.dev, &plci, nullptr, &D.csLayout ) != VK_SUCCESS
		|| v.vkCreatePipelineLayout( v.dev, &plgi, nullptr, &D.gLayout ) != VK_SUCCESS ) {
		why = "the rain / snow descriptor sets / pipeline layouts failed";
		return false;
	}
	D.csSet = sets[0];
	D.gSet = sets[1];
	if ( !fxModule( vkframe_precip_view_cs_spv, sizeof( vkframe_precip_view_cs_spv ), D.cs )
		|| !fxModule( vkframe_precip_view_vs_spv, sizeof( vkframe_precip_view_vs_spv ), D.vs )
		|| !fxModule( vkframe_precip_view_ps_spv, sizeof( vkframe_precip_view_ps_spv ), D.ps )
		|| !fxModule( vkframe_precip_look_view_cs_spv, sizeof( vkframe_precip_look_view_cs_spv ), D.csLook )
		|| !fxModule( vkframe_precip_look_view_vs_spv, sizeof( vkframe_precip_look_view_vs_spv ), D.vsLook )
		|| !fxModule( vkframe_precip_look_view_ps_spv, sizeof( vkframe_precip_look_view_ps_spv ), D.psLook ) ) {
		why = "the rain / snow shader modules failed";
		return false;
	}
	VkComputePipelineCreateInfo cp { VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO };
	cp.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	cp.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	cp.stage.module = D.cs;
	cp.stage.pName = "main";
	cp.layout = D.csLayout;
	VkComputePipelineCreateInfo cpLook = cp;
	cpLook.stage.module = D.csLook;
	if ( v.vkCreateComputePipelines( v.dev, tone().cache, 1, &cp, nullptr, &D.csPipe ) != VK_SUCCESS
		|| v.vkCreateComputePipelines( v.dev, tone().cache, 1, &cpLook, nullptr, &D.csPipeLook ) != VK_SUCCESS ) {
		why = "vkCreateComputePipelines (the rain / snow records) failed";
		return false;
	}
	D.pipelines += 2;
	VkBufferCreateInfo bci { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
	bci.size = kPrecipGlobalAt + kPrecipGlobalBytes;  // the unlit blocks, then slice 2b's (offsets of the unlit ones unchanged)
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
		why = "the rain / snow uniform buffer failed";
		return false;
	}
	std::memset( D.uboPtr, 0, size_t( bci.size ) );
	D.buildMs += nowMs() - t0;
	D.made = true;
	toneCacheSave();
	return true;
}

/* slice 2b: the lit record pass (precip_lit_view_cs; its bindings as spirv-cross --reflect gives them, fxs()'s shifts):
 * b0 globalUniforms, b31, t0 cellLights / t45 cellCluster (texel buffers) at 64 / 109, u0 the records at 192, the images
 * below with their samplers. Made on the first lit run; a failure refuses lit runs only (the unlit ones keep drawing). */
constexpr std::uint32_t kPrecipLitImg[14] = { 70, 81, 83, 89, 90, 91, 92, 93, 94, 95, 96, 97, 98, 110 };
constexpr std::uint32_t kPrecipLitSmp[13] = { 134, 145, 153, 154, 155, 156, 157, 158, 159, 160, 161, 162, 174 };
PFN_vkCmdClearDepthStencilImage precipClearDs = nullptr;

bool precipLitStatic( std::string & why )
{
	Vk & v = vk();
	Precip & D = precip();
	if ( D.litMade )
		return true;
	if ( D.litBroken ) {
		why = D.litWhy;
		return false;
	}
	const double t0 = nowMs();
	auto fail = [&]( const std::string & w ) {
		D.litBroken = true;
		D.litWhy = "the lit rain / snow: " + w;
		why = D.litWhy;
		return false;
	};
	const auto createBufferView = reinterpret_cast<PFN_vkCreateBufferView>( v.vkGetDeviceProcAddr( v.dev, "vkCreateBufferView" ) );
	precipClearDs = reinterpret_cast<PFN_vkCmdClearDepthStencilImage>( v.vkGetDeviceProcAddr( v.dev, "vkCmdClearDepthStencilImage" ) );
	if ( !createBufferView || !precipClearDs )
		return fail( "no vkCreateBufferView / vkCmdClearDepthStencilImage" );
	VkDescriptorSetLayoutBinding b[32] = {};
	int nb = 0;
	auto add = [&]( std::uint32_t at, VkDescriptorType ty ) {
		b[nb].binding = at;
		b[nb].descriptorType = ty;
		b[nb].descriptorCount = 1;
		b[nb].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
		nb++;
	};
	add( 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER );
	add( 31, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER );
	add( 64, VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER );
	add( 109, VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER );
	add( 192, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER );
	for ( std::uint32_t at : kPrecipLitImg )
		add( at, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE );
	for ( std::uint32_t at : kPrecipLitSmp )
		add( at, VK_DESCRIPTOR_TYPE_SAMPLER );
	VkDescriptorSetLayoutCreateInfo lci { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
	lci.bindingCount = std::uint32_t( nb );
	lci.pBindings = b;
	const VkDescriptorPoolSize ps[5] = { { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 2 }, { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 2 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1 }, { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 14 }, { VK_DESCRIPTOR_TYPE_SAMPLER, 13 } };
	VkDescriptorPoolCreateInfo pci { VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
	pci.maxSets = 1;
	pci.poolSizeCount = 5;
	pci.pPoolSizes = ps;
	if ( v.vkCreateDescriptorSetLayout( v.dev, &lci, nullptr, &D.litDsl ) != VK_SUCCESS
		|| v.vkCreateDescriptorPool( v.dev, &pci, nullptr, &D.litPool ) != VK_SUCCESS )
		return fail( "the descriptor set layout / pool failed" );
	VkDescriptorSetAllocateInfo dai { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
	dai.descriptorPool = D.litPool;
	dai.descriptorSetCount = 1;
	dai.pSetLayouts = &D.litDsl;
	VkPipelineLayoutCreateInfo plci { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
	plci.setLayoutCount = 1;
	plci.pSetLayouts = &D.litDsl;
	if ( v.vkAllocateDescriptorSets( v.dev, &dai, &D.litSet ) != VK_SUCCESS
		|| v.vkCreatePipelineLayout( v.dev, &plci, nullptr, &D.litLayout ) != VK_SUCCESS )
		return fail( "the descriptor set / pipeline layout failed" );
	if ( !fxModule( vkframe_precip_lit_view_cs_spv, sizeof( vkframe_precip_lit_view_cs_spv ), D.csLit ) )
		return fail( "the shader module failed" );
	VkComputePipelineCreateInfo cp { VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO };
	cp.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	cp.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	cp.stage.module = D.csLit;
	cp.stage.pName = "main";
	cp.layout = D.litLayout;
	if ( v.vkCreateComputePipelines( v.dev, tone().cache, 1, &cp, nullptr, &D.csPipeLit ) != VK_SUCCESS )
		return fail( "vkCreateComputePipelines failed" );
	D.pipelines += 1;
	// the shadow cubes' and the CSM's zero input: one D16 1x1 image of 6 layers (depth 1: nothing shadows), cleared by
	// the first lit record; a cube-array view and a 2D-array view, a LEQUAL compare sampler (GL's shadow samplers)
	VkImageCreateInfo ici { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
	ici.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
	ici.imageType = VK_IMAGE_TYPE_2D;
	ici.format = VK_FORMAT_D16_UNORM;
	ici.extent = { 1, 1, 1 };
	ici.mipLevels = 1;
	ici.arrayLayers = 6;
	ici.samples = VK_SAMPLE_COUNT_1_BIT;
	ici.tiling = VK_IMAGE_TILING_OPTIMAL;
	ici.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	if ( v.vkCreateImage( v.dev, &ici, nullptr, &D.litDepth ) != VK_SUCCESS )
		return fail( "the zero depth image failed" );
	VkMemoryRequirements ir {};
	v.vkGetImageMemoryRequirements( v.dev, D.litDepth, &ir );
	VkMemoryAllocateInfo iai { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
	iai.allocationSize = ir.size;
	iai.memoryTypeIndex = memType( ir.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );
	if ( iai.memoryTypeIndex == UINT32_MAX || v.vkAllocateMemory( v.dev, &iai, nullptr, &D.litDepthMem ) != VK_SUCCESS
		|| v.vkBindImageMemory( v.dev, D.litDepth, D.litDepthMem, 0 ) != VK_SUCCESS )
		return fail( "the zero depth image: no device memory" );
	D.litCube = imageView( D.litDepth, VK_IMAGE_VIEW_TYPE_CUBE_ARRAY, VK_FORMAT_D16_UNORM, VK_IMAGE_ASPECT_DEPTH_BIT, 1, 6 );
	D.litD2a = imageView( D.litDepth, VK_IMAGE_VIEW_TYPE_2D_ARRAY, VK_FORMAT_D16_UNORM, VK_IMAGE_ASPECT_DEPTH_BIT, 1, 6 );
	if ( !D.litCube || !D.litD2a )
		return fail( "the zero depth image's views failed" );
	VkSamplerCreateInfo sci { VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
	sci.magFilter = sci.minFilter = VK_FILTER_NEAREST;
	sci.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
	sci.addressModeU = sci.addressModeV = sci.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	sci.compareEnable = VK_TRUE;
	sci.compareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
	if ( v.vkCreateSampler( v.dev, &sci, nullptr, &D.litCmp ) != VK_SUCCESS )
		return fail( "the compare sampler failed" );
	// the lamps' zero texel buffer (float4 cellLights, int4 cellCluster): 64 zero bytes, host-visible
	for ( VkFormat tf : { VK_FORMAT_R32G32B32A32_SFLOAT, VK_FORMAT_R32G32B32A32_SINT } ) {
		VkFormatProperties fp {};
		v.vkGetPhysicalDeviceFormatProperties( v.phys, tf, &fp );
		if ( !( fp.bufferFeatures & VK_FORMAT_FEATURE_UNIFORM_TEXEL_BUFFER_BIT ) )
			return fail( "format " + std::to_string( int( tf ) ) + " is no uniform texel buffer on the present device" );
	}
	VkBufferCreateInfo bci { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
	bci.size = 64;
	bci.usage = VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT;
	bool tb = v.vkCreateBuffer( v.dev, &bci, nullptr, &D.litTb ) == VK_SUCCESS;
	if ( tb ) {
		VkMemoryRequirements br {};
		v.vkGetBufferMemoryRequirements( v.dev, D.litTb, &br );
		VkMemoryAllocateInfo ai { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
		ai.allocationSize = br.size;
		ai.memoryTypeIndex = memType( br.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT );
		void * p = nullptr;
		tb = ai.memoryTypeIndex != UINT32_MAX && v.vkAllocateMemory( v.dev, &ai, nullptr, &D.litTbMem ) == VK_SUCCESS
			&& v.vkBindBufferMemory( v.dev, D.litTb, D.litTbMem, 0 ) == VK_SUCCESS && v.vkMapMemory( v.dev, D.litTbMem, 0, VK_WHOLE_SIZE, 0, &p ) == VK_SUCCESS;
		if ( tb && p ) {
			std::memset( p, 0, size_t( bci.size ) );
			v.vkUnmapMemory( v.dev, D.litTbMem );
		}
		tb = tb && p;
	}
	if ( !tb )
		return fail( "the zero texel buffer failed" );
	VkBufferViewCreateInfo bvi { VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO };
	bvi.buffer = D.litTb;
	bvi.range = VK_WHOLE_SIZE;
	bvi.format = VK_FORMAT_R32G32B32A32_SFLOAT;
	VkBufferViewCreateInfo bvi2 = bvi;
	bvi2.format = VK_FORMAT_R32G32B32A32_SINT;
	if ( createBufferView( v.dev, &bvi, nullptr, &D.litTbF ) != VK_SUCCESS || createBufferView( v.dev, &bvi2, nullptr, &D.litTbI ) != VK_SUCCESS )
		return fail( "the zero texel buffer's views failed" );
	D.buildMs += nowMs() - t0;
	D.litMade = true;
	toneCacheSave();
	return true;
}

// the record buffer, grown (the last run's fence already waited); device memory, written and read only on the device
// (host: a gate reads the records back, host-visible coherent memory mapped at recPtr)
bool precipRecords( VkDeviceSize need, bool host, std::string & why )
{
	Vk & v = vk();
	Precip & D = precip();
	if ( D.rec && D.recCap >= need && D.recHost == host )
		return true;
	D.recPtr = nullptr;
	if ( D.rec )
		v.vkDestroyBuffer( v.dev, D.rec, nullptr );
	if ( D.recMem )
		v.vkFreeMemory( v.dev, D.recMem, nullptr );
	D.rec = VK_NULL_HANDLE;
	D.recMem = VK_NULL_HANDLE;
	D.recCap = 0;
	const VkDeviceSize cap = std::max<VkDeviceSize>( need + need / 2, 64 * 1024 );
	VkBufferCreateInfo bci { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
	bci.size = cap;
	bci.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
	bool ok = v.vkCreateBuffer( v.dev, &bci, nullptr, &D.rec ) == VK_SUCCESS;
	if ( ok ) {
		VkMemoryRequirements br {};
		v.vkGetBufferMemoryRequirements( v.dev, D.rec, &br );
		VkMemoryAllocateInfo ai { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
		ai.allocationSize = br.size;
		ai.memoryTypeIndex = host ? memType( br.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT )
			: memType( br.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );
		if ( ai.memoryTypeIndex == UINT32_MAX && !host )
			ai.memoryTypeIndex = memType( br.memoryTypeBits, 0 );
		ok = ai.memoryTypeIndex != UINT32_MAX && v.vkAllocateMemory( v.dev, &ai, nullptr, &D.recMem ) == VK_SUCCESS
			&& v.vkBindBufferMemory( v.dev, D.rec, D.recMem, 0 ) == VK_SUCCESS;
		void * p = nullptr;
		if ( ok && host )
			ok = v.vkMapMemory( v.dev, D.recMem, 0, VK_WHOLE_SIZE, 0, &p ) == VK_SUCCESS;
		D.recPtr = static_cast<std::uint8_t *>( p );
	}
	if ( !ok ) {
		why = "the rain / snow record buffer (" + std::to_string( need ) + " bytes) failed";
		return false;
	}
	D.recCap = cap;
	D.recHost = host;
	return true;
}

// the pipeline over the air's render pass (remade when the frame's sample count changes)
bool precipPipe( std::string & why )
{
	Vk & v = vk();
	Fx & F = fx();
	Precip & D = precip();
	if ( D.pipePass == F.airPass && D.pipeSamples == F.airSamples && D.pipe && D.pipeLook )
		return true;
	const double t0 = nowMs();
	for ( VkPipeline * p : { &D.pipe, &D.pipeLook } )
		if ( *p ) {
			v.vkDestroyPipeline( v.dev, *p, nullptr );
			*p = VK_NULL_HANDLE;
		}
	D.pipePass = VK_NULL_HANDLE;
	VkPipelineShaderStageCreateInfo st[2] = { { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO },
		{ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO } };
	st[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
	st[0].module = D.vs;
	st[0].pName = "main";
	st[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	st[1].module = D.ps;
	st[1].pName = "main";
	VkPipelineVertexInputStateCreateInfo vi { VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
	VkPipelineInputAssemblyStateCreateInfo ia { VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
	ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
	VkPipelineViewportStateCreateInfo vps { VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO };
	vps.viewportCount = vps.scissorCount = 1;
	VkPipelineRasterizationStateCreateInfo rs { VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
	rs.polygonMode = VK_POLYGON_MODE_FILL;
	rs.cullMode = VK_CULL_MODE_NONE;
	rs.frontFace = VK_FRONT_FACE_CLOCKWISE;
	rs.lineWidth = 1.0f;
	VkPipelineMultisampleStateCreateInfo ms { VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
	ms.rasterizationSamples = VkSampleCountFlagBits( F.airSamples );
	VkPipelineDepthStencilStateCreateInfo ds { VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };
	ds.depthTestEnable = VK_TRUE;
	ds.depthWriteEnable = VK_FALSE;
	ds.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
	ds.stencilTestEnable = VK_TRUE;
	ds.front.failOp = ds.front.depthFailOp = VK_STENCIL_OP_KEEP;
	ds.front.passOp = VK_STENCIL_OP_REPLACE;
	ds.front.compareOp = VK_COMPARE_OP_ALWAYS;
	ds.front.compareMask = 0xFF;
	ds.front.writeMask = 0x03;
	ds.back = ds.front;
	VkPipelineColorBlendAttachmentState cb {};  // glBlendFunc( SRC_ALPHA, ONE_MINUS_SRC_ALPHA ), alpha too
	cb.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	cb.blendEnable = VK_TRUE;
	cb.srcColorBlendFactor = cb.srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
	cb.dstColorBlendFactor = cb.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	cb.colorBlendOp = cb.alphaBlendOp = VK_BLEND_OP_ADD;
	VkPipelineColorBlendStateCreateInfo cbs { VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
	cbs.attachmentCount = 1;
	cbs.pAttachments = &cb;
	const VkDynamicState dyn[3] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR, VK_DYNAMIC_STATE_STENCIL_REFERENCE };
	VkPipelineDynamicStateCreateInfo dy { VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
	dy.dynamicStateCount = 3;
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
	gp.layout = D.gLayout;
	gp.renderPass = F.airPass;
	if ( v.vkCreateGraphicsPipelines( v.dev, tone().cache, 1, &gp, nullptr, &D.pipe ) != VK_SUCCESS ) {
		why = "vkCreateGraphicsPipelines (rain / snow) failed";
		return false;
	}
	st[0].module = D.vsLook;  // slice 3: the same state with the look's stages
	st[1].module = D.psLook;
	if ( v.vkCreateGraphicsPipelines( v.dev, tone().cache, 1, &gp, nullptr, &D.pipeLook ) != VK_SUCCESS ) {
		why = "vkCreateGraphicsPipelines (rain / snow, the look) failed";
		return false;
	}
	D.pipelines += 2;
	D.pipePass = F.airPass;
	D.pipeSamples = F.airSamples;
	D.buildMs += nowMs() - t0;
	toneCacheSave();
	return true;
}

// the worker (fxWork, F.kind 8): the records, then the instanced quads into the tone frame
bool precipRecord( std::string & why )
{
	Vk & v = vk();
	Fx & F = fx();
	Precip & D = precip();
	const VkPipeline gPipe = D.look ? D.pipeLook : D.pipe, cPipe = D.lit ? D.csPipeLit : D.look ? D.csPipeLook : D.csPipe;
	if ( !F.fbAir || !gPipe || !cPipe || !D.rec ) {
		why = "no framebuffer / pipeline / record buffer for rain / snow";
		return false;
	}
	if ( D.lit && !D.litLaid ) {  // the zero depth image: UNDEFINED -> cleared to 1 (nothing shadows) -> read by the record pass
		VkImageMemoryBarrier ib { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
		ib.srcQueueFamilyIndex = ib.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		ib.image = D.litDepth;
		ib.subresourceRange = { VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 6 };
		ib.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		ib.newLayout = VK_IMAGE_LAYOUT_GENERAL;
		ib.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		v.vkCmdPipelineBarrier( F.cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &ib );
		const VkClearDepthStencilValue one { 1.0f, 0 };
		precipClearDs( F.cmd, D.litDepth, VK_IMAGE_LAYOUT_GENERAL, &one, 1, &ib.subresourceRange );
		ib.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
		ib.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		ib.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		v.vkCmdPipelineBarrier( F.cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &ib );
		D.litLaid = true;
	}
	v.vkCmdBindPipeline( F.cmd, VK_PIPELINE_BIND_POINT_COMPUTE, cPipe );
	v.vkCmdBindDescriptorSets( F.cmd, VK_PIPELINE_BIND_POINT_COMPUTE, D.lit ? D.litLayout : D.csLayout, 0, 1, D.lit ? &D.litSet : &D.csSet, 0, nullptr );
	v.vkCmdDispatch( F.cmd, std::uint32_t( ( D.n + 63 ) / 64 ), 1, 1 );
	VkMemoryBarrier mb { VK_STRUCTURE_TYPE_MEMORY_BARRIER };  // the records written -> the vertex stage reads them
	mb.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
	mb.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
	v.vkCmdPipelineBarrier( F.cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_VERTEX_SHADER_BIT, 0, 1, &mb, 0, nullptr, 0, nullptr );
	VkRenderPassBeginInfo rbi { VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
	rbi.renderPass = F.airPass;
	rbi.framebuffer = F.fbAir;
	rbi.renderArea = { { 0, 0 }, { std::uint32_t( F.w ), std::uint32_t( F.h ) } };
	v.vkCmdBeginRenderPass( F.cmd, &rbi, VK_SUBPASS_CONTENTS_INLINE );
	// GL's viewport rows from the bottom = the image's rows from the top in memory: no flip (FragCoord.y = GL's)
	const VkViewport vpt { float( F.vp[0] ), float( F.vp[1] ), float( F.vp[2] ), float( F.vp[3] ), 0.0f, 1.0f };
	const int x0 = std::clamp( F.vp[0], 0, F.w ), y0 = std::clamp( F.vp[1], 0, F.h );
	const int x1 = std::clamp( F.vp[0] + F.vp[2], 0, F.w ), y1 = std::clamp( F.vp[1] + F.vp[3], 0, F.h );
	const VkRect2D sc { { x0, y0 }, { std::uint32_t( std::max( x1 - x0, 0 ) ), std::uint32_t( std::max( y1 - y0, 0 ) ) } };
	v.vkCmdSetViewport( F.cmd, 0, 1, &vpt );
	v.vkCmdSetScissor( F.cmd, 0, 1, &sc );
	v.vkCmdBindPipeline( F.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, gPipe );
	v.vkCmdSetStencilReference( F.cmd, VK_STENCIL_FACE_FRONT_AND_BACK, 1u );
	v.vkCmdBindDescriptorSets( F.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, D.gLayout, 0, 1, &D.gSet, 0, nullptr );
	v.vkCmdDraw( F.cmd, 4, std::uint32_t( D.n ), 0, 0 );
	v.vkCmdEndRenderPass( F.cmd );
	if ( D.recHost ) {  // gates only: the records -> the host's read after the fence
		VkMemoryBarrier hb { VK_STRUCTURE_TYPE_MEMORY_BARRIER };
		hb.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
		hb.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
		v.vkCmdPipelineBarrier( F.cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0, 1, &hb, 0, nullptr, 0, nullptr );
	}
	return true;
}
}  // namespace

namespace WwVkPresent
{
bool precipRun( const PrecipRun & t, std::string & why )
{
	Fx & F = fx();
	Precip & D = precip();
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
		D.brokenWhy = "rain / snow on Vulkan broke: " + w;
		return refuse( D.brokenWhy );
	};
	if ( D.broken )
		return refuse( D.brokenWhy );
	if ( t.lit ) {  // slice 2b: the sun alone; the rest of GL's particle light is refused (GL draws)
		if ( !t.litBlock )
			return refuse( "a lit cell without its light block packed" );
		if ( t.look )
			return refuse( "a lit cell with the precipitation look (a later slice)" );
		auto litAt = [&]( const char * nm ) {  // the light block's own members past the head, or the head's (kT_precipcs)
			const int at = precipAt( kT_preciplitcs, int( std::size( kT_preciplitcs ) ), nm );
			return at >= 0 ? at : precipAt( kT_precipcs, int( std::size( kT_precipcs ) ), nm );
		};
		auto litI = [&]( const char * nm ) {
			const int at = litAt( nm );
			int x = 0;
			if ( at >= 0 )
				std::memcpy( &x, t.cs + at, 4 );
			return at < 0 ? -1 : x;
		};
		auto litF = [&]( const char * nm, int k ) {
			const int at = litAt( nm );
			float x = 0.0f;
			if ( at >= 0 )
				std::memcpy( &x, t.cs + at + 4 * k, 4 );
			return x;
		};
		const int terms = litI( "volTerms" );
		const bool local = litF( "vpSun", 3 ) > 0.5f;
		if ( terms < 0 || litI( "cellInterior" ) < 0 || litI( "volPartRed" ) < 0 )
			return refuse( "the light block lacks a member the refusals read" );
		if ( litI( "cellInterior" ) != 0 )
			return refuse( "an interior's lit particles (its law is not judged)" );
		// The gate judged volTerms 1 only: any other term refuses, idle or not.
		if ( terms & 2 )
			return refuse( "the lamps term on the particles (refused in slice 2b)" );
		if ( terms & 4 )
			return refuse( "the GI term on the particles (refused in slice 2b)" );
		if ( terms & 8 )
			return refuse( "the ambient term on the particles (refused in slice 2b)" );
		if ( terms & ~15 )
			return refuse( "an unknown volTerms bit (refused in slice 2b)" );
		if ( litI( "volCsm" ) != 0 )  // (the sun's only shadow read, wwSunShadow, is behind volCsm)
			return refuse( "the sun's shadow map on the particles (refused in slice 2b)" );
		if ( litF( "volAirDrift", 0 ) > 0.0f )
			return refuse( "the air's drift on the sun (refused in slice 2b)" );
		if ( litI( "airTerrOn" ) != 0 )
			return refuse( "the terrain air on the sun (refused in slice 2b)" );
		if ( local )
			return refuse( "the local sun law (refused in slice 2b)" );
		if ( litI( "volPartRed" ) != 0 )
			return refuse( "a particle-light red (volPartRed) in a lit run" );
	}
	if ( t.roofOn || t.tex[0].tex )
		return refuse( "the roof map on (slice 3's)" );
	if ( !toneFrameIs( t.color ) || !T.colorView || !T.dsView )
		return refuse( "the draw target is not the shared tone frame" );
	if ( T.w != t.w || T.h != t.h )
		return refuse( "rain / snow's size is not the tone frame's" );
	if ( t.vp[2] < 1 || t.vp[3] < 1 )
		return refuse( "an empty viewport" );
	if ( t.n < 1 || t.first < 0 )
		return refuse( "no particles" );
	if ( !v.ok || !g.ok || g.ctx != reinterpret_cast<void *>( wglGetCurrentContext() ) )
		return refuse( "the present device or GL's entry points are not ready in this context" );
	if ( !g.copyImageSubData )
		return refuse( "no glCopyImageSubData (the atlas' copy into its shared twin)" );
	const int atFirst = precipAt( kT_precipcs, int( std::size( kT_precipcs ) ), "pcFirst" );
	const int atCount = precipAt( kT_precipcs, int( std::size( kT_precipcs ) ), "pcCount" );
	const int atRed = precipAt( kT_precipcs, int( std::size( kT_precipcs ) ), "pcRed" );
	const int atProj = precipAt( kT_precipcs, int( std::size( kT_precipcs ) ), "pcProj" );
	const int atAtlas = precipAt( kT_precipcs, int( std::size( kT_precipcs ) ), "pcAtlas" );
	const int atNear = precipAt( kT_precipps, int( std::size( kT_precipps ) ), "pcNear" );
	if ( atFirst < 0 || atCount < 0 || atRed < 0 || atProj < 0 || atAtlas < 0 || atNear < 0 )
		return breakOff( "a member missing from the reflected blocks" );
	// the last crossing (any fx pass) is done before its command buffer, sets, framebuffer and buffers are written again
	if ( F.pending ) {
		v.vkWaitForFences( v.dev, 1, &F.fence, VK_TRUE, 1000000000ull );
		F.pending = false;
	}
	std::string w;
	const int samples = T.samples > 0 ? T.samples : std::max( F.airSamples, 1 );
	if ( !fxStatic( w ) || !fxSems( w ) || !fxPasses( samples, w ) || !precipStatic( w ) || !precipPipe( w ) )
		return breakOff( w );
	if ( F.airSamples != T.samples )
		return breakOff( "rain / snow's pipeline is not at the frame's sample count" );
	if ( t.lit && !precipLitStatic( w ) )
		return refuse( w );  // the lit pass alone: the unlit runs keep drawing
	const bool look = t.look && ( t.red & 16 ) == 0;  // red 16: a look run through the plain pipelines
	const VkDeviceSize recStride = look ? kPrecipLookRecBytes : kPrecipRecBytes;
	if ( !precipRecords( VkDeviceSize( t.n ) * recStride, t.recOut != nullptr, w ) )
		return breakOff( w );
	FxNames names;
	names.add( T.colorTex );
	names.add( T.dsTex );
	VkImageView views[8] = {};
	int smp[8] = {};
	const int slotType[8] = { 0, 0, 1, 1, 0, 2, 0, 0 };  // an empty slot binds the zero texture of the shader's type
	for ( int k = 0; k < 8; k++ ) {
		FxTex x = t.tex[k];
		if ( ( k == 1 || k == 7 ) && ( t.red & 32 ) ) {  // red: the atlas dropped (the zero texture)
			x = FxTex();
			x.sampler = t.tex[k].sampler;
		}
		if ( !x.tex && !x.shared )
			x.type = slotType[k];
		if ( !x.shared && x.tex )
			x.shared = farSharedOf( x.tex );
		const bool bc = ( k == 1 || k == 7 ) && x.tex && !x.shared && x.type == 0 && fcFormat( GLenum( x.glFmt ) ).block != 0;
		if ( bc ? !precipAtlas( x, views[k], names, S.copies, w ) : !fxSlot( x, views[k], names, S.copies, w ) )
			return refuse( "rain / snow's input t" + std::to_string( k ) + ": " + w );
		smp[k] = std::clamp( x.sampler, 0, int( kFxSamplers ) - 1 );
	}
	const VkImageView att[2] = { T.colorView, T.dsView };
	if ( !fxFb( F.fbAir, F.airPass, att, 2, T.w, T.h ) )
		return breakOff( "rain / snow's framebuffer over the tone frame failed" );
	// the compute b31 (GL's uniforms + the run), the vertex block (its pcProj / pcAtlas bytes + the red), the pixel b31
	std::uint8_t * u = D.uboPtr;
	std::uint8_t * c = u + kPrecipCsAt;
	std::memcpy( c, t.cs, size_t( kT_precipcsBytes ) );
	const std::uint32_t first = std::uint32_t( t.first ), count = std::uint32_t( t.n );
	const int csRed = ( ( t.red & 1 ) ? 1 : 0 ) | ( ( t.red & 8 ) ? 2 : 0 ) | ( ( t.red & 128 ) ? 4 : 0 );  // 4: the light dropped
	std::memcpy( c + atFirst, &first, 4 );
	std::memcpy( c + atCount, &count, 4 );
	std::memcpy( c + atRed, &csRed, 4 );
	if ( t.lit ) {  // the lit record pass's b31 (the head at kT_precipcs's offsets + the light block) and its zero globalUniforms
		std::uint8_t * l = u + kPrecipLitCsAt;
		std::memcpy( l, t.cs, size_t( kT_preciplitcsBytes ) );
		std::memcpy( l + atFirst, &first, 4 );
		std::memcpy( l + atCount, &count, 4 );
		std::memcpy( l + atRed, &csRed, 4 );
		std::memset( u + kPrecipGlobalAt, 0, size_t( kPrecipGlobalBytes ) );
	}
	std::memset( u, 0, size_t( kPrecipViewBytes ) );
	std::memcpy( u, t.cs + atProj, 64 );
	std::memcpy( u + 64, t.cs + atAtlas, 16 );
	const int vsRed = ( t.red & 2 ) ? 1 : 0;
	std::memcpy( u + 80, &vsRed, 4 );
	std::uint8_t * p = u + kPrecipPsAt;
	std::memcpy( p, t.ps, size_t( kT_preciplookpsBytes ) );
	if ( t.red & 4 ) {  // red: the near fade dropped (whole from the eye on)
		const float nf[2] = { -2.0f, -1.0f };
		std::memcpy( p + atNear, nf, 8 );
	}
	const VkDeviceSize recBytes = VkDeviceSize( t.n ) * recStride;
	VkDescriptorBufferInfo bi[5] = { { D.ubo, kPrecipCsAt, VkDeviceSize( kT_precipcsBytes ) }, { D.rec, 0, recBytes },
		{ D.ubo, 0, kPrecipViewBytes }, { D.rec, 0, recBytes }, { D.ubo, kPrecipPsAt, VkDeviceSize( kT_preciplookpsBytes ) } };
	VkDescriptorImageInfo ii[5] = {};
	ii[0].imageView = views[0];
	ii[1].imageView = views[1];
	ii[2].sampler = F.smp[smp[1]];
	ii[3].imageView = views[1];
	ii[4].sampler = F.smp[smp[1]];
	for ( int k : { 0, 1, 3 } )
		ii[k].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	VkDescriptorImageInfo fi[10] = {};  // the air: views 2..6 at 89..93, their samplers at 153..157
	for ( int k = 0; k < 5; k++ ) {
		fi[k].imageView = views[2 + k];
		fi[k].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
		fi[5 + k].sampler = F.smp[smp[2 + k]];
	}
	VkDescriptorImageInfo ni[2] = {};  // the look's pcTexN (slot 7) at 66, its sampler at 130
	ni[0].imageView = views[7];
	ni[0].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	ni[1].sampler = F.smp[smp[7]];
	VkWriteDescriptorSet ws[22] = {};
	auto put = [&]( int k, VkDescriptorSet s, std::uint32_t b, VkDescriptorType ty, const VkDescriptorBufferInfo * bp, const VkDescriptorImageInfo * ip ) {
		ws[k].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		ws[k].dstSet = s;
		ws[k].dstBinding = b;
		ws[k].descriptorCount = 1;
		ws[k].descriptorType = ty;
		ws[k].pBufferInfo = bp;
		ws[k].pImageInfo = ip;
	};
	put( 0, D.csSet, 31, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &bi[0], nullptr );
	put( 1, D.csSet, 64, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, nullptr, &ii[0] );
	put( 2, D.csSet, 65, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, nullptr, &ii[1] );
	put( 3, D.csSet, 129, VK_DESCRIPTOR_TYPE_SAMPLER, nullptr, &ii[2] );
	put( 4, D.csSet, 192, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &bi[1], nullptr );
	put( 5, D.gSet, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &bi[2], nullptr );
	put( 6, D.gSet, 65, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &bi[3], nullptr );
	put( 7, D.gSet, 31, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &bi[4], nullptr );
	put( 8, D.gSet, 64, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, nullptr, &ii[3] );
	put( 9, D.gSet, 128, VK_DESCRIPTOR_TYPE_SAMPLER, nullptr, &ii[4] );
	for ( int k = 0; k < 5; k++ ) {
		put( 10 + k, D.csSet, std::uint32_t( 89 + k ), VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, nullptr, &fi[k] );
		put( 15 + k, D.csSet, std::uint32_t( 153 + k ), VK_DESCRIPTOR_TYPE_SAMPLER, nullptr, &fi[5 + k] );
	}
	put( 20, D.gSet, 66, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, nullptr, &ni[0] );
	put( 21, D.gSet, 130, VK_DESCRIPTOR_TYPE_SAMPLER, nullptr, &ni[1] );
	v.vkUpdateDescriptorSets( v.dev, 22, ws, 0, nullptr );
	if ( t.lit ) {  // the lit record pass's set: the run's inputs as the unlit pass, the zero inputs of the refused terms
		const VkDescriptorBufferInfo lb[3] = { { D.ubo, kPrecipGlobalAt, kPrecipGlobalBytes },
			{ D.ubo, kPrecipLitCsAt, VkDeviceSize( kT_preciplitcsBytes ) }, { D.rec, 0, recBytes } };
		VkDescriptorImageInfo li[14] = {};
		const VkImageView liv[14] = { D.litCube, F.d2a, F.d3, views[2], views[3], views[4], views[5], views[6], F.d3, D.litD2a,
			F.d2, F.d2, F.d2, views[1] };  // kPrecipLitImg's order
		for ( int k = 0; k < 14; k++ ) {
			li[k].imageView = liv[k];
			li[k].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
		}
		VkDescriptorImageInfo ls[13] = {};
		const VkSampler lsv[13] = { D.litCmp, F.smp[0], F.smp[smp[2]], F.smp[smp[3]], F.smp[smp[4]], F.smp[smp[5]], F.smp[smp[6]],
			F.smp[0], D.litCmp, F.smp[0], F.smp[0], F.smp[0], F.smp[smp[1]] };  // kPrecipLitSmp's order
		for ( int k = 0; k < 13; k++ )
			ls[k].sampler = lsv[k];
		VkWriteDescriptorSet lw[32] = {};
		int nl = 0;
		auto lput = [&]( std::uint32_t b, VkDescriptorType ty, const VkDescriptorBufferInfo * bp, const VkDescriptorImageInfo * ip,
			const VkBufferView * tp ) {
			lw[nl].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			lw[nl].dstSet = D.litSet;
			lw[nl].dstBinding = b;
			lw[nl].descriptorCount = 1;
			lw[nl].descriptorType = ty;
			lw[nl].pBufferInfo = bp;
			lw[nl].pImageInfo = ip;
			lw[nl].pTexelBufferView = tp;
			nl++;
		};
		lput( 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &lb[0], nullptr, nullptr );
		lput( 31, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &lb[1], nullptr, nullptr );
		lput( 192, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &lb[2], nullptr, nullptr );
		lput( 64, VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, nullptr, nullptr, &D.litTbF );
		lput( 109, VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, nullptr, nullptr, &D.litTbI );
		for ( int k = 0; k < 14; k++ )
			lput( kPrecipLitImg[k], VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, nullptr, &li[k], nullptr );
		for ( int k = 0; k < 13; k++ )
			lput( kPrecipLitSmp[k], VK_DESCRIPTOR_TYPE_SAMPLER, nullptr, &ls[k], nullptr );
		v.vkUpdateDescriptorSets( v.dev, std::uint32_t( nl ), lw, 0, nullptr );
	}
	D.n = t.n;
	D.look = look;
	D.lit = t.lit;
	F.kind = 8;
	F.w = t.w;
	F.h = t.h;
	std::memcpy( F.vp, t.vp, sizeof( F.vp ) );
	if ( !fxCross( names, w ) )
		return breakOff( w );  // nothing signals vkDone: GL does not wait it; GL draws its points this frame
	if ( t.recOut && D.recPtr ) {  // gates only: wait the run, copy its records out
		v.vkWaitForFences( v.dev, 1, &F.fence, VK_TRUE, 1000000000ull );
		F.pending = false;
		std::memcpy( t.recOut, D.recPtr, size_t( recBytes ) );
	}
	const double ms = nowMs() - t0;
	S.runs++;
	S.cpuMs += ms;
	S.maxMs = std::max( S.maxMs, ms );
	return true;
}

void precipOnGl( const char * why )
{
	FxStat & S = precip().st;
	S.gl++;
	S.glWhy = why ? why : "";
}

FxStat takePrecipStat()
{
	Precip & D = precip();
	FxStat s = D.st;
	s.pipelines = D.pipelines;
	s.buildMs = D.buildMs;
	if ( D.broken && s.failWhy.empty() )
		s.failWhy = D.brokenWhy;
	D.st = FxStat();
	return s;
}
}  // namespace WwVkPresent
