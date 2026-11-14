// Lane VKFAR2 (2026-11-14, plan row G2): the sky bodies in the sky's pass. Included by vkpresent.cpp after vkobjtex.inc (it
// uses the sky's state and pass, the cache's twins (ot, farTwinFind), the bindless sampler mapping (blSamplerRead,
// blSamplerKey, blSwizzleNorm, blIsSrgb), makeLocal, imageView, oneShot, drain). The API and the row: vkskybody.h.
#include "features/vkframe/vkskybody.h"
#include "features/vkframe/skystars_view_vs.spv.h"
#include "features/vkframe/skystars_view_ps.spv.h"
#include "features/vkframe/skymoon_view_vs.spv.h"
#include "features/vkframe/skymoon_view_ps.spv.h"

namespace
{
/* one host-coherent buffer for both bodies' blocks and the moon's quad (written while nothing reads it: skyRun waited the
 * last crossing's fence), the stars' mesh in its own (it grows) */
constexpr VkDeviceSize kSbStarsAt = 0, kSbMoonAt = 1024;            // each: b0 at +0, the vertex b31 at +512, the pixel b31 at +768
constexpr VkDeviceSize kSbVsAt = 512, kSbPsAt = 768;
constexpr VkDeviceSize kSbQuadAt = 2560, kSbQuadIdxAt = 2624, kSbUboBytes = 2816;

struct SbBody
{
	VkDescriptorSetLayout dsl = VK_NULL_HANDLE;
	VkDescriptorSet set = VK_NULL_HANDLE;
	VkPipelineLayout layout = VK_NULL_HANDLE;
	VkShaderModule vs = VK_NULL_HANDLE, ps = VK_NULL_HANDLE;
	VkPipeline pipe = VK_NULL_HANDLE;
};

struct Sb
{
	bool armed = false, made = false, broken = false;
	std::string brokenWhy;
	SbBody stars, moon;
	VkDescriptorPool dpool = VK_NULL_HANDLE;
	VkRenderPass pipePass = VK_NULL_HANDLE;  // the sky's pass the pipelines were made for
	VkImage zeroImg = VK_NULL_HANDLE;        // GL's unbound unit: (0, 0, 0, 1)
	VkDeviceMemory zeroMem = VK_NULL_HANDLE;
	VkImageView zeroView = VK_NULL_HANDLE;
	VkBuffer ubo = VK_NULL_HANDLE, geo = VK_NULL_HANDLE;
	VkDeviceMemory uboMem = VK_NULL_HANDLE, geoMem = VK_NULL_HANDLE;
	std::uint8_t * uboPtr = nullptr, * geoPtr = nullptr;
	VkDeviceSize geoCap = 0;
	std::vector<std::pair<BlSmpKey, VkSampler>> smps;
	// this pass: the joined bodies in GL's order (0 stars, 1 moon), the stars' mesh, the twins sampled
	int order[2] = {}, n = 0;
	std::uint32_t starsIdx = 0;
	VkDeviceSize posAt = 0, colAt = 0, uvAt = 0, idxAt = 0;
	std::vector<GLuint> names;
};

Sb & sb()
{
	static Sb s;
	return s;
}

VkDeviceSize sbAlign( VkDeviceSize x )
{
	return ( x + 255 ) & ~VkDeviceSize( 255 );
}

bool sbBuffer( VkDeviceSize bytes, VkBufferUsageFlags use, VkBuffer & buf, VkDeviceMemory & mem, std::uint8_t *& ptr )
{
	Vk & v = vk();
	VkBufferCreateInfo bci { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
	bci.size = bytes;
	bci.usage = use;
	if ( v.vkCreateBuffer( v.dev, &bci, nullptr, &buf ) != VK_SUCCESS )
		return false;
	VkMemoryRequirements br {};
	v.vkGetBufferMemoryRequirements( v.dev, buf, &br );
	VkMemoryAllocateInfo ai { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
	ai.allocationSize = br.size;
	ai.memoryTypeIndex = memType( br.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT );
	void * p = nullptr;
	const bool ok = ai.memoryTypeIndex != UINT32_MAX && v.vkAllocateMemory( v.dev, &ai, nullptr, &mem ) == VK_SUCCESS
		&& v.vkBindBufferMemory( v.dev, buf, mem, 0 ) == VK_SUCCESS && v.vkMapMemory( v.dev, mem, 0, VK_WHOLE_SIZE, 0, &p ) == VK_SUCCESS;
	ptr = static_cast<std::uint8_t *>( p );
	return ok && ptr;
}

/* a body's set: b0 (both stages) at 0, the pixel b31 at 31, the vertex b31 at 32, textures at 64.., samplers at 128..
 * (vkframe_spirv.sh's fxk / fxb), and its pipeline layout and modules */
bool sbBodyMake( SbBody & b, int ntex, const std::uint32_t * vs, size_t vsBytes, const std::uint32_t * ps, size_t psBytes )
{
	Vk & v = vk();
	std::vector<VkDescriptorSetLayoutBinding> bs;
	auto add = [&]( std::uint32_t at, VkDescriptorType t, VkShaderStageFlags st ) { bs.push_back( { at, t, 1, st, nullptr } ); };
	add( 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT );
	add( 31, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT );
	add( 32, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT );
	for ( int k = 0; k < ntex; k++ ) {
		add( std::uint32_t( 64 + k ), VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, VK_SHADER_STAGE_FRAGMENT_BIT );
		add( std::uint32_t( 128 + k ), VK_DESCRIPTOR_TYPE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT );
	}
	VkDescriptorSetLayoutCreateInfo lci { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
	lci.bindingCount = std::uint32_t( bs.size() );
	lci.pBindings = bs.data();
	if ( v.vkCreateDescriptorSetLayout( v.dev, &lci, nullptr, &b.dsl ) != VK_SUCCESS )
		return false;
	VkDescriptorSetAllocateInfo dai { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
	dai.descriptorPool = sb().dpool;
	dai.descriptorSetCount = 1;
	dai.pSetLayouts = &b.dsl;
	VkPipelineLayoutCreateInfo plci { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
	plci.setLayoutCount = 1;
	plci.pSetLayouts = &b.dsl;
	if ( v.vkAllocateDescriptorSets( v.dev, &dai, &b.set ) != VK_SUCCESS
		|| v.vkCreatePipelineLayout( v.dev, &plci, nullptr, &b.layout ) != VK_SUCCESS )
		return false;
	VkShaderModuleCreateInfo mci { VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
	mci.codeSize = vsBytes;
	mci.pCode = vs;
	if ( v.vkCreateShaderModule( v.dev, &mci, nullptr, &b.vs ) != VK_SUCCESS )
		return false;
	mci.codeSize = psBytes;
	mci.pCode = ps;
	return v.vkCreateShaderModule( v.dev, &mci, nullptr, &b.ps ) == VK_SUCCESS;
}

// the device-time parts (once): the pool, both bodies' sets and modules, the blocks' buffer with the moon's quad, the 1x1
bool sbStatic( std::string & why )
{
	Vk & v = vk();
	Sb & B = sb();
	if ( B.made )
		return true;
	const double t0 = nowMs();
	const VkDescriptorPoolSize ps[3] = { { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 6 }, { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 8 },
		{ VK_DESCRIPTOR_TYPE_SAMPLER, 8 } };
	VkDescriptorPoolCreateInfo pci { VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
	pci.maxSets = 2;
	pci.poolSizeCount = 3;
	pci.pPoolSizes = ps;
	if ( v.vkCreateDescriptorPool( v.dev, &pci, nullptr, &B.dpool ) != VK_SUCCESS
		|| !sbBodyMake( B.stars, WwVkPresent::kSkyStarsTex, vkframe_skystars_view_vs_spv, sizeof( vkframe_skystars_view_vs_spv ),
			vkframe_skystars_view_ps_spv, sizeof( vkframe_skystars_view_ps_spv ) )
		|| !sbBodyMake( B.moon, WwVkPresent::kSkyMoonTex, vkframe_skymoon_view_vs_spv, sizeof( vkframe_skymoon_view_vs_spv ),
			vkframe_skymoon_view_ps_spv, sizeof( vkframe_skymoon_view_ps_spv ) ) ) {
		why = "the sky bodies' sets, layouts or modules failed";
		return false;
	}
	if ( !sbBuffer( kSbUboBytes, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
			B.ubo, B.uboMem, B.uboPtr ) ) {
		why = "the sky bodies' uniform buffer failed";
		return false;
	}
	// drawMoonGlobe's quad (lookdev_bill spreads it about centerWorld) and its two triangles
	static const float quad[12] = { -1, -1, 0, 1, -1, 0, -1, 1, 0, 1, 1, 0 };
	static const std::uint16_t idx[6] = { 0, 1, 2, 2, 1, 3 };
	std::memcpy( B.uboPtr + kSbQuadAt, quad, sizeof( quad ) );
	std::memcpy( B.uboPtr + kSbQuadIdxAt, idx, sizeof( idx ) );
	// the 1x1 an unbound unit reads: GL's texture 0 samples (0, 0, 0, 1)
	double mb = 0.0;
	VkCommandBuffer cmd = VK_NULL_HANDLE;
	VkFence fence = VK_NULL_HANDLE;
	VkFenceCreateInfo fi { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
	VkCommandBufferAllocateInfo cai { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
	cai.commandPool = v.pool;
	cai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	cai.commandBufferCount = 1;
	if ( !makeLocal( B.zeroImg, B.zeroMem, VK_IMAGE_TYPE_2D, VK_FORMAT_R8G8B8A8_UNORM, 1, 1, 1, mb )
		|| !( B.zeroView = imageView( B.zeroImg, VK_IMAGE_VIEW_TYPE_2D, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT ) )
		|| v.vkCreateFence( v.dev, &fi, nullptr, &fence ) != VK_SUCCESS || v.vkAllocateCommandBuffers( v.dev, &cai, &cmd ) != VK_SUCCESS ) {
		why = "the sky bodies' unbound-unit image failed";
		return false;
	}
	drain();  // the queue is the worker's
	VkCommandBufferBeginInfo bi { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
	bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	v.vkBeginCommandBuffer( cmd, &bi );
	VkImageMemoryBarrier ib { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
	ib.srcQueueFamilyIndex = ib.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	ib.image = B.zeroImg;
	ib.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
	ib.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	ib.newLayout = VK_IMAGE_LAYOUT_GENERAL;
	ib.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	v.vkCmdPipelineBarrier( cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &ib );
	VkClearColorValue one {};
	one.float32[3] = 1.0f;
	v.vkCmdClearColorImage( cmd, B.zeroImg, VK_IMAGE_LAYOUT_GENERAL, &one, 1, &ib.subresourceRange );
	ib.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
	ib.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	ib.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
	v.vkCmdPipelineBarrier( cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, 1, &ib );
	const bool cleared = v.vkEndCommandBuffer( cmd ) == VK_SUCCESS && oneShot( cmd, fence );
	v.vkFreeCommandBuffers( v.dev, v.pool, 1, &cmd );
	v.vkDestroyFence( v.dev, fence, nullptr );
	if ( !cleared ) {
		why = "the sky bodies' unbound-unit image's clear failed";
		return false;
	}
	sky().buildMs += nowMs() - t0;
	B.made = true;
	return true;
}

/* a body's pipeline in the sky's pass (its samples; no depth: GL's skyState turns both tests off), blend as GL's: the
 * stars SRC_ALPHA / ONE_MINUS_SRC_ALPHA, the globe ONE / ONE_MINUS_SRC_ALPHA (premultiplied), alpha ZERO / ONE (kept) */
bool sbPipe( SbBody & b, bool moon, VkRenderPass pass, int samples, std::string & why )
{
	Vk & v = vk();
	if ( b.pipe )
		v.vkDestroyPipeline( v.dev, b.pipe, nullptr );
	b.pipe = VK_NULL_HANDLE;
	VkPipelineShaderStageCreateInfo st[2] = { { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO },
		{ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO } };
	st[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
	st[0].module = b.vs;
	st[0].pName = "main";
	st[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	st[1].module = b.ps;
	st[1].pName = "main";
	// drawShape's attributes: the stars 0x20000043 (pos 3, colour 4, uv 2 at GL's 7 = TEXCOORD7 at location 2), the globe 3
	const VkVertexInputBindingDescription vb[3] = { { 0, 12, VK_VERTEX_INPUT_RATE_VERTEX }, { 1, 16, VK_VERTEX_INPUT_RATE_VERTEX },
		{ 2, 8, VK_VERTEX_INPUT_RATE_VERTEX } };
	const VkVertexInputAttributeDescription va[3] = { { 0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0 }, { 1, 1, VK_FORMAT_R32G32B32A32_SFLOAT, 0 },
		{ 2, 2, VK_FORMAT_R32G32_SFLOAT, 0 } };
	VkPipelineVertexInputStateCreateInfo vi { VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
	vi.vertexBindingDescriptionCount = vi.vertexAttributeDescriptionCount = moon ? 1 : 3;
	vi.pVertexBindingDescriptions = vb;
	vi.pVertexAttributeDescriptions = va;
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
	ms.rasterizationSamples = VkSampleCountFlagBits( samples );
	/* GL draws the backdrop with GL_MULTISAMPLE off (lookdevstage's SingleSample): a pixel is in or out by its centre and
	 * all its samples take the colour. Every sample at the centre rasterizes the same (the stars are a pixel or two) */
	VkSampleLocationEXT centre[64];
	VkPipelineSampleLocationsStateCreateInfoEXT sl { VK_STRUCTURE_TYPE_PIPELINE_SAMPLE_LOCATIONS_STATE_CREATE_INFO_EXT };
	if ( samples > 1 ) {
		if ( !( v.centreSamples & VkSampleCountFlags( samples ) ) || samples > 64 ) {
			why = "no VK_EXT_sample_locations at " + std::to_string( samples ) + " samples (GL's single-sample backdrop)";
			return false;
		}
		for ( int k = 0; k < samples; k++ )
			centre[k] = { 0.5f, 0.5f };
		sl.sampleLocationsEnable = VK_TRUE;
		sl.sampleLocationsInfo.sType = VK_STRUCTURE_TYPE_SAMPLE_LOCATIONS_INFO_EXT;
		sl.sampleLocationsInfo.sampleLocationsPerPixel = VkSampleCountFlagBits( samples );
		sl.sampleLocationsInfo.sampleLocationGridSize = { 1, 1 };
		sl.sampleLocationsInfo.sampleLocationsCount = std::uint32_t( samples );
		sl.sampleLocationsInfo.pSampleLocations = centre;
		ms.pNext = &sl;
	}
	VkPipelineColorBlendAttachmentState cb {};
	cb.blendEnable = VK_TRUE;
	cb.srcColorBlendFactor = moon ? VK_BLEND_FACTOR_ONE : VK_BLEND_FACTOR_SRC_ALPHA;
	cb.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	cb.colorBlendOp = VK_BLEND_OP_ADD;
	cb.srcAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
	cb.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	cb.alphaBlendOp = VK_BLEND_OP_ADD;
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
	gp.pColorBlendState = &cbs;
	gp.pDynamicState = &dy;
	gp.layout = b.layout;
	gp.renderPass = pass;
	if ( v.vkCreateGraphicsPipelines( v.dev, tone().cache, 1, &gp, nullptr, &b.pipe ) != VK_SUCCESS ) {
		why = std::string( "vkCreateGraphicsPipelines (the " ) + ( moon ? "moon globe" : "stars" ) + ", " + std::to_string( samples ) + " samples) failed";
		return false;
	}
	return true;
}

// both pipelines for the sky's current pass (skyPipes remakes it when the frame's samples change)
bool sbPipes( std::string & why )
{
	Sky & S = sky();
	Sb & B = sb();
	if ( B.pipePass == S.pass && B.stars.pipe && B.moon.pipe )
		return true;
	const double t0 = nowMs();
	B.pipePass = VK_NULL_HANDLE;
	if ( !sbPipe( B.stars, false, S.pass, S.pipeSamples, why ) || !sbPipe( B.moon, true, S.pass, S.pipeSamples, why ) )
		return false;
	B.pipePass = S.pass;
	S.pipelines = 3;
	S.buildMs += nowMs() - t0;
	toneCacheSave();
	return true;
}

VkSampler sbSampler( const BlSmpKey & k, std::string & why )
{
	Vk & v = vk();
	Sb & B = sb();
	for ( const auto & s : B.smps )
		if ( std::memcmp( &s.first, &k, sizeof( k ) ) == 0 )
			return s.second;
	VkSamplerCreateInfo ci { VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };  // as blSamplerIndex makes the bindless table's
	ci.magFilter = VkFilter( k.mag );
	ci.minFilter = VkFilter( k.min );
	ci.mipmapMode = VkSamplerMipmapMode( k.mip );
	ci.addressModeU = VkSamplerAddressMode( k.u );
	ci.addressModeV = VkSamplerAddressMode( k.v );
	ci.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	ci.mipLodBias = k.bias;
	ci.anisotropyEnable = k.aniso > 1.0f ? VK_TRUE : VK_FALSE;
	ci.maxAnisotropy = k.aniso > 1.0f ? k.aniso : 1.0f;
	ci.minLod = k.minLod;
	ci.maxLod = k.maxLod;
	ci.borderColor = VkBorderColor( k.border );
	VkSampler s = VK_NULL_HANDLE;
	if ( v.vkCreateSampler( v.dev, &ci, nullptr, &s ) != VK_SUCCESS ) {
		why = "vkCreateSampler (a sky body's map) failed";
		return VK_NULL_HANDLE;
	}
	B.smps.push_back( { k, s } );
	return s;
}

/* what a sampler's unit holds, on Vulkan: nothing = the 1x1 (0, 0, 0, 1); one of the sky's LUTs = its twin copied this frame;
 * a cache texture = its BC twin with GL's sampler state (filters, wrap, levels, anisotropy) */
bool sbTex( unsigned name, WwVkPresent::GlProc gp, const char * what, VkImageView & view, VkSampler & smp, std::string & why )
{
	Sky & S = sky();
	Sb & B = sb();
	view = VK_NULL_HANDLE;
	smp = S.linear;
	if ( !name ) {
		view = B.zeroView;
		return true;
	}
	for ( int k = 0; k < 4; k++ )
		if ( name == S.lutGl[k] ) {
			view = S.lutView[k];
			return true;
		}
	auto it = ot().twins.find( GLuint( name ) );
	const Shared * s = it == ot().twins.end() ? nullptr : farTwinFind( it->second.key );
	if ( !s || !s->view || s->type != 0 ) {
		why = std::string( what ) + ": GL texture " + std::to_string( name ) + " has no Vulkan twin (not block-compressed, or loaded "
			"before the row was wanted)";
		return false;
	}
	WwVkPresent::BlSampler bs;
	BlSmpKey k;
	std::string w;
	if ( !WwVkPresent::blSamplerRead( name, bs, gp, w ) ) {
		why = std::string( what ) + ": " + w;
		return false;
	}
	if ( blIsSrgb( s->fmt ) && bs.srgbDecode == 0x8A4A ) {
		why = std::string( what ) + ": an sRGB map with its decode skipped";
		return false;
	}
	blSwizzleNorm( s->fmt, bs );
	if ( !blSamplerKey( bs, k, w ) || !( smp = sbSampler( k, w ) ) ) {
		why = std::string( what ) + ": " + w;
		return false;
	}
	view = s->view;
	if ( std::find( B.names.begin(), B.names.end(), GLuint( name ) ) == B.names.end() )
		B.names.push_back( GLuint( name ) );
	return true;
}

// a body's set: its three blocks at base (b0, vertex b31, pixel b31) and its maps
void sbWrite( const SbBody & b, VkDeviceSize base, VkDeviceSize vsBytes, VkDeviceSize psBytes, int ntex, const VkImageView * view,
	const VkSampler * smp )
{
	Vk & v = vk();
	Sb & B = sb();
	const VkDescriptorBufferInfo bi[3] = { { B.ubo, base, VkDeviceSize( WwVkPresent::kSkyGlobalBytes ) },
		{ B.ubo, base + kSbPsAt, psBytes }, { B.ubo, base + kSbVsAt, vsBytes } };
	VkDescriptorImageInfo ii[2 * WwVkPresent::kSkyMoonTex] = {};
	VkWriteDescriptorSet ws[3 + 2 * WwVkPresent::kSkyMoonTex] = {};
	const std::uint32_t at[3] = { 0, 31, 32 };
	int n = 0;
	for ( int k = 0; k < 3; k++, n++ ) {
		ws[n].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		ws[n].dstSet = b.set;
		ws[n].dstBinding = at[k];
		ws[n].descriptorCount = 1;
		ws[n].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		ws[n].pBufferInfo = &bi[k];
	}
	for ( int k = 0; k < ntex; k++ ) {
		ii[2 * k].imageView = view[k];
		ii[2 * k].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
		ii[2 * k + 1].sampler = smp[k];
		for ( int j = 0; j < 2; j++, n++ ) {
			ws[n].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			ws[n].dstSet = b.set;
			ws[n].dstBinding = std::uint32_t( ( j ? 128 : 64 ) + k );
			ws[n].descriptorCount = 1;
			ws[n].descriptorType = j ? VK_DESCRIPTOR_TYPE_SAMPLER : VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
			ws[n].pImageInfo = &ii[2 * k + j];
		}
	}
	v.vkUpdateDescriptorSets( v.dev, std::uint32_t( n ), ws, 0, nullptr );
}

// the checks every body passes before it joins: a held crossing, the sky's viewport, the device parts
bool sbReady( const int * vp, int body, std::string & why )
{
	Sky & S = sky();
	Sb & B = sb();
	if ( !S.held ) {
		why = "the sky's crossing is not held (the sky on GL this frame, or the bodies not armed)";
		return false;
	}
	for ( int k = 0; k < B.n; k++ )
		if ( B.order[k] == body ) {
			why = "the same body twice in one pass";
			return false;
		}
	if ( std::memcmp( vp, S.vp, sizeof( S.vp ) ) != 0 ) {
		why = "GL's viewport is not the sky's";
		return false;
	}
	if ( B.broken ) {
		why = B.brokenWhy;
		return false;
	}
	if ( !sbStatic( why ) || !sbPipes( why ) ) {
		B.broken = true;
		B.brokenWhy = "the sky bodies on Vulkan broke: " + why;
		why = B.brokenWhy;
		return false;
	}
	return true;
}

bool sbRefuse( const std::string & w, std::string & why )
{
	Sky & S = sky();
	why = w;
	S.st.bodyGl++;
	S.st.bodyWhy = w;
	return false;
}
}  // namespace

namespace
{
bool skyBodiesTakeArm()
{
	const bool a = sb().armed;
	sb().armed = false;
	return a;
}

void skyBodyTexNames( std::vector<GLuint> & out )
{
	for ( const GLuint n : sb().names )
		if ( std::find( out.begin(), out.end(), n ) == out.end() )
			out.push_back( n );
}

void skyBodyRecord( VkCommandBuffer cmd )
{
	Vk & v = vk();
	Sb & B = sb();
	for ( int k = 0; k < B.n; k++ ) {  // the sky's viewport and scissor stay set (the bodies' GL viewport is the sky's)
		const bool moon = B.order[k] == 1;
		const SbBody & b = moon ? B.moon : B.stars;
		v.vkCmdBindPipeline( cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, b.pipe );
		v.vkCmdBindDescriptorSets( cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, b.layout, 0, 1, &b.set, 0, nullptr );
		if ( moon ) {
			const VkDeviceSize at = kSbQuadAt;
			v.vkCmdBindVertexBuffers( cmd, 0, 1, &B.ubo, &at );
			v.vkCmdBindIndexBuffer( cmd, B.ubo, kSbQuadIdxAt, VK_INDEX_TYPE_UINT16 );
			v.vkCmdDrawIndexed( cmd, 6, 1, 0, 0, 0 );
		} else {
			const VkBuffer bufs[3] = { B.geo, B.geo, B.geo };
			const VkDeviceSize at[3] = { B.posAt, B.colAt, B.uvAt };
			v.vkCmdBindVertexBuffers( cmd, 0, 3, bufs, at );
			v.vkCmdBindIndexBuffer( cmd, B.geo, B.idxAt, VK_INDEX_TYPE_UINT16 );
			v.vkCmdDrawIndexed( cmd, B.starsIdx, 1, 0, 0, 0 );
		}
	}
}

void skyBodiesDone()
{
	Sb & B = sb();
	B.n = 0;
	B.names.clear();
}
}  // namespace

namespace WwVkPresent
{
void skyBodiesArm( bool on )
{
	sb().armed = on;
}

bool skyBodiesHeld()
{
	return sky().held;
}

bool skyStars( const SkyStars & s, std::string & why )
{
	Vk & v = vk();
	Sb & B = sb();
	std::string w;
	if ( !sbReady( s.vp, 0, w ) )
		return sbRefuse( w, why );
	if ( s.nv < 1 || s.nv > 65536 || s.ni < 3 || !s.pos || !s.col || !s.uv || !s.idx )
		return sbRefuse( "no stars mesh (or over 65536 vertices)", why );
	static const char * const kName[kSkyStarsTex] = { "Tex", "Tex2", "ldTrans" };
	VkImageView view[kSkyStarsTex];
	VkSampler smp[kSkyStarsTex];
	const std::size_t names0 = B.names.size();
	for ( int k = 0; k < kSkyStarsTex; k++ )
		if ( !sbTex( s.tex[k], s.gp, kName[k], view[k], smp[k], w ) ) {
			B.names.resize( names0 );
			return sbRefuse( w, why );
		}
	// the mesh into its buffer (grown to the next 64 KiB when short; the last crossing is done, nothing reads the old)
	const VkDeviceSize posB = VkDeviceSize( s.nv ) * 12, colB = VkDeviceSize( s.nv ) * 16, uvB = VkDeviceSize( s.nv ) * 8;
	const VkDeviceSize need = sbAlign( posB ) + sbAlign( colB ) + sbAlign( uvB ) + VkDeviceSize( s.ni ) * 2;
	if ( need > B.geoCap ) {
		if ( B.geo )
			v.vkDestroyBuffer( v.dev, B.geo, nullptr );
		if ( B.geoMem )
			v.vkFreeMemory( v.dev, B.geoMem, nullptr );
		B.geo = VK_NULL_HANDLE;
		B.geoMem = VK_NULL_HANDLE;
		B.geoPtr = nullptr;
		B.geoCap = 0;
		const VkDeviceSize cap = ( need + 65535 ) & ~VkDeviceSize( 65535 );
		if ( !sbBuffer( cap, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, B.geo, B.geoMem, B.geoPtr ) ) {
			B.names.resize( names0 );
			return sbRefuse( "the stars' mesh buffer failed", why );
		}
		B.geoCap = cap;
	}
	B.posAt = 0;
	B.colAt = sbAlign( posB );
	B.uvAt = B.colAt + sbAlign( colB );
	B.idxAt = B.uvAt + sbAlign( uvB );
	std::memcpy( B.geoPtr + B.posAt, s.pos, size_t( posB ) );
	std::memcpy( B.geoPtr + B.colAt, s.col, size_t( colB ) );
	std::memcpy( B.geoPtr + B.uvAt, s.uv, size_t( uvB ) );
	std::memcpy( B.geoPtr + B.idxAt, s.idx, size_t( s.ni ) * 2 );
	B.starsIdx = std::uint32_t( s.ni );
	std::memcpy( B.uboPtr + kSbStarsAt, s.global, sizeof( s.global ) );
	std::memcpy( B.uboPtr + kSbStarsAt + kSbVsAt, s.vs, sizeof( s.vs ) );
	std::memcpy( B.uboPtr + kSbStarsAt + kSbPsAt, s.ps, sizeof( s.ps ) );
	sbWrite( B.stars, kSbStarsAt, kT_skyStarsVsBytes, kT_skyStarsPsBytes, kSkyStarsTex, view, smp );
	B.order[B.n++] = 0;
	sky().st.stars++;
	return true;
}

bool skyMoon( const SkyMoon & m, std::string & why )
{
	Sb & B = sb();
	std::string w;
	if ( !sbReady( m.vp, 1, w ) )
		return sbRefuse( w, why );
	static const char * const kName[kSkyMoonTex] = { "atAlbedo", "atRelief", "atTransTex", "atSunView", "atMoonView" };
	VkImageView view[kSkyMoonTex];
	VkSampler smp[kSkyMoonTex];
	const std::size_t names0 = B.names.size();
	for ( int k = 0; k < kSkyMoonTex; k++ )
		if ( !sbTex( m.tex[k], m.gp, kName[k], view[k], smp[k], w ) ) {
			B.names.resize( names0 );
			return sbRefuse( w, why );
		}
	std::memcpy( B.uboPtr + kSbMoonAt, m.global, sizeof( m.global ) );
	std::memcpy( B.uboPtr + kSbMoonAt + kSbVsAt, m.vs, sizeof( m.vs ) );
	std::memcpy( B.uboPtr + kSbMoonAt + kSbPsAt, m.ps, sizeof( m.ps ) );
	sbWrite( B.moon, kSbMoonAt, kT_skyMoonVsBytes, kT_skyMoonPsBytes, kSkyMoonTex, view, smp );
	B.order[B.n++] = 1;
	sky().st.moons++;
	return true;
}

bool skyBodiesFlush( std::string & why )
{
	Sky & S = sky();
	if ( !S.held )
		return true;
	const double t0 = nowMs();
	std::string w;
	if ( !skyCross( w ) ) {  // nothing drew: the caller draws the sky and the bodies on GL
		S.broken = true;
		S.brokenWhy = "the sky on Vulkan broke: " + w;
		S.st.fails++;
		S.st.failWhy = S.brokenWhy;
		S.st.flushFails++;
		why = S.brokenWhy;
		return false;
	}
	const double ms = nowMs() - t0;
	S.st.cpuMs += ms;
	S.st.maxMs = std::max( S.st.maxMs, ms );
	return true;
}
}  // namespace WwVkPresent
