/* Lane VKWATER1 (2026-11-13, plan row G5): the water's route on the present device (vkwater.h). Included at the end of
 * vkpresent.cpp (its vk() / vkFormatOf / nowMs). No draw moves yet: every refusal is named slot by slot, and GL draws. */
#include "features/vkframe/vkwater.h"
#include "features/vkframe/water_view_vs.spv.h"  // lane VKWATER3: the shaded draw's twins (vkframe_spirv.sh)
#include "features/vkframe/water_view_ps.spv.h"

namespace
{
struct WaterRoute
{
	WwVkPresent::WaterStat st;
};
WaterRoute & waterS()
{
	static WaterRoute W;
	return W;
}
bool waterSharedFmt( unsigned f )
{
	bool depth = false;
	return vkFormatOf( GLenum( f ), depth ) != VK_FORMAT_UNDEFINED;
}

/* Lane VKWATER3: the shaded draw. water_view_vs (fo4_default) + water_view_ps (fo4_waterlook, FOGLEAK + FRAME_UBO) over
 * the shape's GL buffers copied into one exported buffer; the loose uniforms into a host buffer; every input through
 * waterTwin; one VkSampler per GL sampling state; the air's render pass over the tone frame; fx's crossing (kind 3). */
struct WaterImgName
{
	std::uint32_t binding;
	const char * name;
};
// the twins' image bindings (water_view_ps t n -> 64 + n, water_view_vs t 0 -> 110), their samplers at + 64
const WaterImgName kWaterImg[] = { { 64, "atApTex" }, { 65, "volAirTerr" }, { 66, "volAirRainMap" }, { 67, "volTex" },
	{ 68, "volTile" }, { 69, "cellIsBloom" }, { 70, "cellIsLut" }, { 71, "waterInfoIndex" }, { 72, "waterInfoAtlas" },
	{ 73, "waterFlowIndex" }, { 74, "waterFlowAtlas" }, { 75, "waterInfoBodies" }, { 77, "csmMap" }, { 78, "ussTerrMap" },
	{ 79, "ussContactMap" }, { 80, "ussCloudMap" }, { 81, "waterMoment0" }, { 82, "waterMoment1" }, { 83, "waterMoment2" },
	{ 84, "waterCloudMap" }, { 85, "waterOceanB" }, { 86, "waterOceanC" }, { 87, "waterNoise2" }, { 88, "waterSkyIndex" },
	{ 89, "waterSkyAtlas" }, { 90, "waterDepth" }, { 91, "waterNoise0" }, { 92, "waterNoise1" }, { 93, "waterSsrRaw" },
	{ 94, "waterSsrFin" }, { 95, "waterScene" }, { 110, "farStream" } };
constexpr VkDeviceSize kWaterAlign = 256;
// the host buffer: zeros (a block GL has no buffer for), the loose blocks, the disabled attributes' values
constexpr VkDeviceSize kWaterZeroAt = 0, kWaterVsLooseAt = 12288, kWaterPsLooseAt = 12800, kWaterConstAt = 18176, kWaterHostBytes = 18432;

struct WaterDraw
{
	bool made = false, broken = false;
	std::string brokenWhy;
	VkShaderModule vs = VK_NULL_HANDLE, ps = VK_NULL_HANDLE;
	std::vector<WwVkPresent::FarSetSlot> binds;
	std::vector<VkShaderStageFlags> stages;
	VkDescriptorSetLayout dsl = VK_NULL_HANDLE;
	VkDescriptorPool pool = VK_NULL_HANDLE;
	VkDescriptorSet set = VK_NULL_HANDLE;
	VkPipelineLayout layout = VK_NULL_HANDLE;
	std::map<std::string, VkPipeline> pipes;
	VkRenderPass pipePass = VK_NULL_HANDLE;
	int pipeSamples = 0;
	std::map<std::string, VkSampler> smp;
	CsmBuf geo, host;
	PFN_vkCmdDrawIndexed drawIndexed = nullptr;  // not in the device table (Vk): loaded here
	// this run's record (written before the hand)
	VkPipeline pipe = VK_NULL_HANDLE;
	VkBuffer vb[WwVkPresent::kWaterAttribs] = {};
	VkDeviceSize vbOff[WwVkPresent::kWaterAttribs] = {};
	VkDeviceSize ibOff = 0;
	std::uint32_t count = 0, first = 0;
	std::int32_t baseVertex = 0;
	int vp[4] = {};
	VkRect2D sc {};
	WwVkPresent::FxStat st;
	int pipelines = 0;
	double buildMs = 0.0;
};
WaterDraw & waterD()
{
	static WaterDraw D;
	return D;
}

VkFormat waterAttrFormat( const WwVkPresent::WaterAttrib & a )
{
	const int n = std::clamp( a.size, 1, 4 ) - 1;
	static const VkFormat f32[4] = { VK_FORMAT_R32_SFLOAT, VK_FORMAT_R32G32_SFLOAT, VK_FORMAT_R32G32B32_SFLOAT, VK_FORMAT_R32G32B32A32_SFLOAT };
	static const VkFormat f16[4] = { VK_FORMAT_R16_SFLOAT, VK_FORMAT_R16G16_SFLOAT, VK_FORMAT_R16G16B16_SFLOAT, VK_FORMAT_R16G16B16A16_SFLOAT };
	static const VkFormat u8n[4] = { VK_FORMAT_R8_UNORM, VK_FORMAT_R8G8_UNORM, VK_FORMAT_R8G8B8_UNORM, VK_FORMAT_R8G8B8A8_UNORM };
	static const VkFormat u8s[4] = { VK_FORMAT_R8_USCALED, VK_FORMAT_R8G8_USCALED, VK_FORMAT_R8G8B8_USCALED, VK_FORMAT_R8G8B8A8_USCALED };
	static const VkFormat s8n[4] = { VK_FORMAT_R8_SNORM, VK_FORMAT_R8G8_SNORM, VK_FORMAT_R8G8B8_SNORM, VK_FORMAT_R8G8B8A8_SNORM };
	static const VkFormat s8s[4] = { VK_FORMAT_R8_SSCALED, VK_FORMAT_R8G8_SSCALED, VK_FORMAT_R8G8B8_SSCALED, VK_FORMAT_R8G8B8A8_SSCALED };
	static const VkFormat u16n[4] = { VK_FORMAT_R16_UNORM, VK_FORMAT_R16G16_UNORM, VK_FORMAT_R16G16B16_UNORM, VK_FORMAT_R16G16B16A16_UNORM };
	static const VkFormat u16s[4] = { VK_FORMAT_R16_USCALED, VK_FORMAT_R16G16_USCALED, VK_FORMAT_R16G16B16_USCALED, VK_FORMAT_R16G16B16A16_USCALED };
	static const VkFormat s16n[4] = { VK_FORMAT_R16_SNORM, VK_FORMAT_R16G16_SNORM, VK_FORMAT_R16G16B16_SNORM, VK_FORMAT_R16G16B16A16_SNORM };
	static const VkFormat s16s[4] = { VK_FORMAT_R16_SSCALED, VK_FORMAT_R16G16_SSCALED, VK_FORMAT_R16G16B16_SSCALED, VK_FORMAT_R16G16B16A16_SSCALED };
	switch ( a.type ) {
	case 0x1406: return f32[n];
	case 0x140B: return f16[n];
	case 0x1401: return a.norm ? u8n[n] : u8s[n];
	case 0x1400: return a.norm ? s8n[n] : s8s[n];
	case 0x1403: return a.norm ? u16n[n] : u16s[n];
	case 0x1402: return a.norm ? s16n[n] : s16s[n];
	default: return VK_FORMAT_UNDEFINED;
	}
}

int waterAttrBytes( const WwVkPresent::WaterAttrib & a )
{
	const int c = a.type == 0x1406 ? 4 : ( a.type == 0x140B || a.type == 0x1403 || a.type == 0x1402 ) ? 2 : 1;
	return c * std::clamp( a.size, 1, 4 );
}

// the device-time parts (once): modules, the merged set (vs + ps reflected), its pool, one set, the pipeline layout
bool waterStatic( std::string & why )
{
	Vk & v = vk();
	WaterDraw & D = waterD();
	if ( D.made )
		return true;
	WwVkPresent::FarSetCounts cv, cp;
	if ( !WwVkPresent::farSetReflect( vkframe_water_view_vs_spv, sizeof( vkframe_water_view_vs_spv ) / 4, cv, why )
		|| !WwVkPresent::farSetReflect( vkframe_water_view_ps_spv, sizeof( vkframe_water_view_ps_spv ) / 4, cp, why ) ) {
		why = "the water twins' reflection: " + why;
		return false;
	}
	D.binds.clear();
	D.stages.clear();
	for ( int s = 0; s < 2; s++ )
		for ( const WwVkPresent::FarSetSlot & b : ( s ? cp : cv ).binds ) {
			for ( const WwVkPresent::FarSetSlot & o : D.binds )
				if ( o.binding == b.binding ) {
					why = "the water twins both declare binding " + std::to_string( b.binding );
					return false;
				}
			D.binds.push_back( b );
			D.stages.push_back( s ? VK_SHADER_STAGE_FRAGMENT_BIT : VK_SHADER_STAGE_VERTEX_BIT );
		}
	std::vector<VkDescriptorSetLayoutBinding> lb;
	std::uint32_t n[4] = {};
	static const VkDescriptorType kType[4] = { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
		VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, VK_DESCRIPTOR_TYPE_SAMPLER };
	for ( std::size_t i = 0; i < D.binds.size(); i++ ) {
		VkDescriptorSetLayoutBinding b {};
		b.binding = D.binds[i].binding;
		b.descriptorType = kType[D.binds[i].kind];
		b.descriptorCount = 1;
		b.stageFlags = D.stages[i];
		lb.push_back( b );
		n[D.binds[i].kind]++;
	}
	VkDescriptorSetLayoutCreateInfo li { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
	li.bindingCount = std::uint32_t( lb.size() );
	li.pBindings = lb.data();
	std::vector<VkDescriptorPoolSize> ps;
	for ( int k = 0; k < 4; k++ )
		if ( n[k] )
			ps.push_back( { kType[k], n[k] } );
	VkDescriptorPoolCreateInfo pi { VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
	pi.maxSets = 1;
	pi.poolSizeCount = std::uint32_t( ps.size() );
	pi.pPoolSizes = ps.data();
	if ( v.vkCreateDescriptorSetLayout( v.dev, &li, nullptr, &D.dsl ) != VK_SUCCESS
		|| v.vkCreateDescriptorPool( v.dev, &pi, nullptr, &D.pool ) != VK_SUCCESS ) {
		why = "the water draw's set layout / pool failed";
		return false;
	}
	VkDescriptorSetAllocateInfo ai { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
	ai.descriptorPool = D.pool;
	ai.descriptorSetCount = 1;
	ai.pSetLayouts = &D.dsl;
	VkPipelineLayoutCreateInfo pli { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
	pli.setLayoutCount = 1;
	pli.pSetLayouts = &D.dsl;
	if ( v.vkAllocateDescriptorSets( v.dev, &ai, &D.set ) != VK_SUCCESS
		|| v.vkCreatePipelineLayout( v.dev, &pli, nullptr, &D.layout ) != VK_SUCCESS
		|| !fxModule( vkframe_water_view_vs_spv, sizeof( vkframe_water_view_vs_spv ), D.vs )
		|| !fxModule( vkframe_water_view_ps_spv, sizeof( vkframe_water_view_ps_spv ), D.ps ) ) {
		why = "the water draw's set / layout / modules failed";
		return false;
	}
	D.drawIndexed = reinterpret_cast<PFN_vkCmdDrawIndexed>( v.vkGetDeviceProcAddr( v.dev, "vkCmdDrawIndexed" ) );
	if ( !D.drawIndexed ) {
		why = "no vkCmdDrawIndexed";
		return false;
	}
	D.made = true;
	return true;
}

// one VkSampler per GL sampling state (plain: the red's linear clamp, no mips)
VkSampler waterSampler( const WwVkPresent::WaterSlot & s, bool compare, bool plain, std::string & why )
{
	Vk & v = vk();
	WaterDraw & D = waterD();
	const bool aniso = !plain && s.aniso > 1.0f && v.anisotropy;
	const bool cmp = compare && s.cmpMode == 0x884E;  // GL_COMPARE_REF_TO_TEXTURE
	char key[160];
	std::snprintf( key, sizeof( key ), "%d %x %x %x %x %x %d %.3f %d %x", int( plain ), s.magF, s.minF, s.wrap[0], s.wrap[1], s.wrap[2],
		int( aniso ), double( s.aniso ), int( cmp ), s.cmpFunc );
	auto it = D.smp.find( key );
	if ( it != D.smp.end() )
		return it->second;
	auto wrap = []( unsigned w ) {
		switch ( w ) {
		case 0x2901: return VK_SAMPLER_ADDRESS_MODE_REPEAT;
		case 0x8370: return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
		case 0x812D: return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;  // GL's default border (0, 0, 0, 0)
		default: return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		}
	};
	const bool mip = !plain && s.minF >= 0x2700 && s.minF <= 0x2703;
	VkSamplerCreateInfo sci { VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
	sci.magFilter = !plain && s.magF == 0x2600 ? VK_FILTER_NEAREST : VK_FILTER_LINEAR;
	sci.minFilter = !plain && ( s.minF == 0x2600 || s.minF == 0x2700 || s.minF == 0x2702 ) ? VK_FILTER_NEAREST : VK_FILTER_LINEAR;
	sci.mipmapMode = mip && ( s.minF == 0x2702 || s.minF == 0x2703 ) ? VK_SAMPLER_MIPMAP_MODE_LINEAR : VK_SAMPLER_MIPMAP_MODE_NEAREST;
	sci.addressModeU = plain ? VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE : wrap( s.wrap[0] );
	sci.addressModeV = plain ? VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE : wrap( s.wrap[1] );
	sci.addressModeW = plain ? VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE : wrap( s.wrap[2] );
	sci.borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
	sci.maxLod = mip ? VK_LOD_CLAMP_NONE : 0.0f;
	if ( aniso ) {
		sci.anisotropyEnable = VK_TRUE;
		sci.maxAnisotropy = std::min( s.aniso, v.maxAniso );
	}
	if ( cmp ) {
		sci.compareEnable = VK_TRUE;
		sci.compareOp = VkCompareOp( std::clamp( int( s.cmpFunc ) - 0x0200, 0, 7 ) );  // GL NEVER .. ALWAYS = Vulkan's order
	}
	VkSampler out = VK_NULL_HANDLE;
	if ( v.vkCreateSampler( v.dev, &sci, nullptr, &out ) != VK_SUCCESS ) {
		why = "vkCreateSampler (the water draw) failed";
		return VK_NULL_HANDLE;
	}
	D.smp[key] = out;
	return out;
}

// the pipeline for GL's state this draw (vertex layout, cull, depth, colour mask, the reds), over the air's render pass
VkPipeline waterPipe( const WwVkPresent::WaterRun & r, const VkFormat * fmt, std::string & why )
{
	Vk & v = vk();
	Fx & F = fx();
	WaterDraw & D = waterD();
	if ( D.pipePass != F.airPass || D.pipeSamples != F.airSamples ) {
		for ( auto & kv : D.pipes )
			v.vkDestroyPipeline( v.dev, kv.second, nullptr );
		D.pipes.clear();
		D.pipePass = F.airPass;
		D.pipeSamples = F.airSamples;
	}
	std::string key;
	VkVertexInputBindingDescription vb[WwVkPresent::kWaterAttribs] = {};
	VkVertexInputAttributeDescription va[WwVkPresent::kWaterAttribs] = {};
	for ( int i = 0; i < WwVkPresent::kWaterAttribs; i++ ) {
		const WwVkPresent::WaterAttrib & a = r.attr[i];
		vb[i].binding = std::uint32_t( i );
		vb[i].stride = a.on ? std::uint32_t( a.stride ? a.stride : waterAttrBytes( a ) ) : 0u;
		vb[i].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
		va[i].location = std::uint32_t( WwVkPresent::kWaterVsIn[i].spv );
		va[i].binding = std::uint32_t( i );
		va[i].format = fmt[i];
		va[i].offset = 0;
		key += std::to_string( int( fmt[i] ) ) + "/" + std::to_string( vb[i].stride ) + " ";
	}
	const bool flip = ( r.red & 4 ) != 0;
	const bool ccw = ( r.frontFace != 0x0900 ) != flip;  // GL_CW 0x0900
	const VkCullModeFlags cull = !r.cullOn ? VK_CULL_MODE_NONE : r.cullFace == 0x0404 ? VK_CULL_MODE_FRONT_BIT
		: r.cullFace == 0x0408 ? VK_CULL_MODE_FRONT_AND_BACK : VK_CULL_MODE_BACK_BIT;
	const VkCompareOp dop = ( r.red & 8 ) ? VK_COMPARE_OP_ALWAYS : VkCompareOp( std::clamp( int( r.depthFunc ) - 0x0200, 0, 7 ) );
	key += std::to_string( int( ccw ) ) + " " + std::to_string( int( cull ) ) + " " + std::to_string( int( r.depthTest ) ) + " "
		+ std::to_string( int( dop ) ) + " " + std::to_string( int( r.depthMask ) );
	for ( bool m : r.colorMask )
		key += m ? "1" : "0";
	auto it = D.pipes.find( key );
	if ( it != D.pipes.end() )
		return it->second;
	const double t0 = nowMs();
	VkPipelineShaderStageCreateInfo st[2] = { { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO },
		{ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO } };
	st[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
	st[0].module = D.vs;
	st[0].pName = "main";
	st[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	st[1].module = D.ps;
	st[1].pName = "main";
	VkPipelineVertexInputStateCreateInfo vi { VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
	vi.vertexBindingDescriptionCount = std::uint32_t( WwVkPresent::kWaterAttribs );
	vi.pVertexBindingDescriptions = vb;
	vi.vertexAttributeDescriptionCount = std::uint32_t( WwVkPresent::kWaterAttribs );
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
	// no y flip: GL's window rows from the bottom are the image's rows from the top, so GL's CCW front is Vulkan's CW
	rs.frontFace = ccw ? VK_FRONT_FACE_CLOCKWISE : VK_FRONT_FACE_COUNTER_CLOCKWISE;
	rs.cullMode = cull;
	rs.lineWidth = 1.0f;
	VkPipelineMultisampleStateCreateInfo ms { VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
	ms.rasterizationSamples = VkSampleCountFlagBits( F.airSamples );
	VkPipelineDepthStencilStateCreateInfo ds { VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };
	ds.depthTestEnable = r.depthTest ? VK_TRUE : VK_FALSE;
	ds.depthWriteEnable = r.depthTest && r.depthMask ? VK_TRUE : VK_FALSE;
	ds.depthCompareOp = dop;
	VkPipelineColorBlendAttachmentState cb {};
	cb.colorWriteMask = ( r.colorMask[0] ? VK_COLOR_COMPONENT_R_BIT : 0 ) | ( r.colorMask[1] ? VK_COLOR_COMPONENT_G_BIT : 0 )
		| ( r.colorMask[2] ? VK_COLOR_COMPONENT_B_BIT : 0 ) | ( r.colorMask[3] ? VK_COLOR_COMPONENT_A_BIT : 0 );
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
	VkPipeline out = VK_NULL_HANDLE;
	if ( v.vkCreateGraphicsPipelines( v.dev, tone().cache, 1, &gp, nullptr, &out ) != VK_SUCCESS ) {
		why = "vkCreateGraphicsPipelines (the water draw) failed";
		return VK_NULL_HANDLE;
	}
	D.pipes[key] = out;
	D.pipelines++;
	D.buildMs += nowMs() - t0;
	toneCacheSave();
	return out;
}

// fxWork's kind 3 (the worker): the shape into the tone frame
bool waterRecord( std::string & why )
{
	Vk & v = vk();
	Fx & F = fx();
	WaterDraw & D = waterD();
	if ( !F.fbAir || !D.pipe ) {
		why = "no framebuffer / pipeline for the water draw";
		return false;
	}
	VkRenderPassBeginInfo rbi { VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
	rbi.renderPass = F.airPass;
	rbi.framebuffer = F.fbAir;
	rbi.renderArea = { { 0, 0 }, { std::uint32_t( F.w ), std::uint32_t( F.h ) } };
	v.vkCmdBeginRenderPass( F.cmd, &rbi, VK_SUBPASS_CONTENTS_INLINE );
	const VkViewport vpt { float( D.vp[0] ), float( D.vp[1] ), float( D.vp[2] ), float( D.vp[3] ), 0.0f, 1.0f };
	v.vkCmdSetViewport( F.cmd, 0, 1, &vpt );
	v.vkCmdSetScissor( F.cmd, 0, 1, &D.sc );
	v.vkCmdBindPipeline( F.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, D.pipe );
	v.vkCmdBindDescriptorSets( F.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, D.layout, 0, 1, &D.set, 0, nullptr );
	v.vkCmdBindVertexBuffers( F.cmd, 0, std::uint32_t( WwVkPresent::kWaterAttribs ), D.vb, D.vbOff );
	v.vkCmdBindIndexBuffer( F.cmd, D.geo.buf, D.ibOff, VK_INDEX_TYPE_UINT16 );
	D.drawIndexed( F.cmd, D.count, 1, D.first, D.baseVertex, 0 );
	v.vkCmdEndRenderPass( F.cmd );
	return true;
}

// shutdown (after fxDrop waited the device): the exported copy is GL's name too
void waterDrop()
{
	WaterDraw & D = waterD();
	if ( !D.geo.buf && !D.host.buf )
		return;
	vk().vkDeviceWaitIdle( vk().dev );
	csmBufFree( D.geo, gl().ok && D.geo.glCtx == gl().ctx );
	csmBufFree( D.host, false );
}
}  // namespace

namespace WwVkPresent
{
WaterCaps waterCaps()
{
	WaterCaps c;
	/* the present device as vkpresent.cpp creates it (lane VKWATER2): textureCompressionBC and imageCubeArray (VKFAR1's
	 * farTwin), VkBufferView texel buffers (farTexelBuffer), when offered; no shaded mesh pipeline yet */
	c.sharedFmt = &waterSharedFmt;  // lane VKWATER2: + R32UI / RG32UI / R8UI and D32F_S8 (waterDepth on a float frame)
	c.intFormats = waterSharedFmt( 0x8236 ) && waterSharedFmt( 0x823C ) && waterSharedFmt( 0x8232 );  // R32UI RG32UI R8UI
	if ( vk().ok ) {
		std::string why;
		c.bc = fcFeatures( true, false, why );
		c.cubeArray = fcFeatures( false, true, why );
		c.texelBuffer = farTexelBufferMax() > 0;
		c.aniso = vk().anisotropy;  // VKOBJ2's samplerAnisotropy
		c.tess = vk().tess;  // lane VKWATER2: tessellationShader
	}
	return c;
}

bool waterTwin( std::uintptr_t key, const WaterSlot & s, unsigned & tex, std::string & why )
{
	tex = 0;
	if ( s.kind == 5 ) {
		unsigned buf = 0;
		return farTexelBuffer( key, s.glFmt, std::size_t( std::max( s.w, 1 ) ) * 16u, buf, tex, why );
	}
	if ( s.kind == 4 )
		return farTwin( key, 3, s.w, s.h, s.d, s.glFmt, tex, why, s.levels );
	if ( s.kind < 0 || s.kind > 2 || ( s.kind == 1 && waterBcFormat( s.glFmt ) ) ) {
		why = s.name + ": no twin for this sampler kind / format";
		return false;
	}
	if ( waterBcFormat( s.glFmt ) )
		return farTwin( key, s.kind, s.w, s.h, s.kind == 2 ? s.d : 1, s.glFmt, tex, why, s.levels );
	return sharedTarget( key, s.kind, s.w, s.h, s.d, s.glFmt, tex, why, s.levels );
}

void waterTwinDrop( std::uintptr_t key, const WaterSlot & s )
{
	if ( s.kind == 5 )
		farTexelBufferDrop( key );
	else if ( s.kind == 4 || waterBcFormat( s.glFmt ) )
		farTwinDrop( key );
	else
		sharedTargetDrop( key );
}

void waterOnGl( const char * why )
{
	WaterStat & s = waterS().st;
	s.gl++;
	s.glWhy = why ? why : "";
}

void waterCensusTook( double ms )
{
	WaterStat & s = waterS().st;
	s.census++;
	s.censusMs += ms;
}

WaterStat takeWaterStat()
{
	WaterStat s = waterS().st;
	waterS().st = WaterStat();
	return s;
}

std::string waterDrawRefusal()
{
	Vk & v = vk();
	Gl & g = gl();
	if ( waterD().broken )
		return waterD().brokenWhy;
	if ( !v.ok || !g.ok )
		return "the present device or GL's entry points are not ready";
	if ( !v.clipControl )
		return "no VK_EXT_depth_clip_control (GL's clip z in [-w, w])";
	if ( !g.createBuffers || !g.namedBufferStorageMem || !g.copyNamedBufferSubData || !g.copyImageSubData )
		return "no glNamedBufferStorageMemEXT / glCopyNamedBufferSubData / glCopyImageSubData (the shape's and inputs' copies)";
	return std::string();
}

bool waterRun( const WaterRun & r, std::string & why )
{
	Fx & F = fx();
	WaterDraw & D = waterD();
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
		D.brokenWhy = "the water draw on Vulkan broke: " + w;
		return refuse( D.brokenWhy );
	};
	if ( D.broken )
		return refuse( D.brokenWhy );
	if ( !v.ok || !g.ok || g.ctx != reinterpret_cast<void *>( wglGetCurrentContext() ) )
		return refuse( "the present device or GL's entry points are not ready in this context" );
	if ( const std::string no = waterDrawRefusal(); !no.empty() )
		return refuse( no );
	if ( !toneFrameIs( r.color ) || !T.colorView || !T.dsView )
		return refuse( "the draw target is not the shared tone frame" );
	if ( T.w != r.w || T.h != r.h )
		return refuse( "the water draw's size is not the tone frame's" );
	if ( r.census.tess )
		return refuse( "a tessellated water program (its own twin is not in yet)" );
	WaterCaps caps = waterCaps();
	caps.shadedDraw = true;
	const std::string verdict = waterVerdict( r.census, caps );
	if ( !verdict.empty() )
		return refuse( verdict );
	if ( r.blend || r.stencil || r.polyOffset )
		return refuse( "GL draws the water with blending / a stencil test / a polygon offset" );
	if ( r.count < 3 || r.vp[2] < 1 || r.vp[3] < 1 || ( r.idxOff & 1 ) || !r.ebo || r.idxOff + 2LL * r.count > r.eboBytes )
		return refuse( "no triangles / viewport / element range" );
	VkFormat fmt[kWaterAttribs] = {};
	for ( int i = 0; i < kWaterAttribs; i++ ) {
		const WaterAttrib & a = r.attr[i];
		if ( !a.on ) {
			fmt[i] = VK_FORMAT_R32G32B32A32_SFLOAT;
			continue;
		}
		fmt[i] = waterAttrFormat( a );
		VkFormatProperties fp {};
		if ( fmt[i] != VK_FORMAT_UNDEFINED )
			v.vkGetPhysicalDeviceFormatProperties( v.phys, fmt[i], &fp );
		if ( a.integer || a.divisor || !a.buf || fmt[i] == VK_FORMAT_UNDEFINED || !( fp.bufferFeatures & VK_FORMAT_FEATURE_VERTEX_BUFFER_BIT ) )
			return refuse( "vertex input " + std::to_string( kWaterVsIn[i].gl ) + ": an integer / instanced / client / unsupported array" );
		if ( a.bufBytes <= 0 || std::int64_t( a.off ) >= a.bufBytes )
			return refuse( "vertex input " + std::to_string( kWaterVsIn[i].gl ) + ": its offset is past its buffer" );
	}
	if ( !g.copyImageSubData )
		return refuse( "no glCopyImageSubData" );
	// the last crossing (any fx pass) is done before its command buffer, set, framebuffer and buffers are written again
	if ( F.pending ) {
		v.vkWaitForFences( v.dev, 1, &F.fence, VK_TRUE, 1000000000ull );
		F.pending = false;
	}
	std::string w;
	const int samples = T.samples > 0 ? T.samples : std::max( F.airSamples, 1 );
	if ( !fxStatic( w ) || !fxSems( w ) || !fxPasses( samples, w ) || !waterStatic( w ) )
		return breakOff( w );
	if ( F.airSamples != T.samples )
		return breakOff( "the air's render pass is not at the frame's sample count" );
	// the exported buffer: every distinct vertex buffer, the elements, the GL blocks; the host one: loose blocks, zeros
	struct Part
	{
		unsigned buf;
		long long src, bytes;
		VkDeviceSize at;
	};
	std::vector<Part> parts;
	VkDeviceSize total = 0;
	auto place = [&]( unsigned buf, long long src, long long bytes, long long room ) -> VkDeviceSize {
		for ( const Part & p : parts )
			if ( p.buf == buf && p.src == src && p.bytes == bytes )
				return p.at;
		const VkDeviceSize at = total;
		parts.push_back( { buf, src, bytes, at } );
		total += ( VkDeviceSize( std::max( bytes, room ) ) + kWaterAlign - 1 ) / kWaterAlign * kWaterAlign;
		return at;
	};
	VkDeviceSize vbAt[kWaterAttribs] = {};
	for ( int i = 0; i < kWaterAttribs; i++ )
		if ( r.attr[i].on )
			vbAt[i] = place( r.attr[i].buf, 0, r.attr[i].bufBytes, 0 ) + VkDeviceSize( r.attr[i].off );
	const VkDeviceSize ibAt = place( r.ebo, 0, r.eboBytes, 0 );
	// the twins' block sizes (water_view_vs b40 / b41, water_view_ps b0 / b1): GL's range, never past the copy
	const WaterUbo * ubos[3] = { &r.global, &r.shadow, &r.skin };
	const VkDeviceSize uboNeed[3] = { 336, 248, 12288 };
	VkDeviceSize uboAt[3] = {};
	bool uboHost[3] = {};
	for ( int k = 0; k < 3; k++ ) {
		const WaterUbo & u = *ubos[k];
		if ( !u.buf || u.size <= 0 || u.off < 0 || u.off + u.size > u.bufBytes || ( u.off % 4 ) ) {
			uboHost[k] = true;  // no buffer: GL reads zeros there too
			continue;
		}
		const long long need = static_cast<long long>( uboNeed[k] );
		uboAt[k] = place( u.buf, u.off, std::min( u.size, need ), need );
	}
	if ( !csmBuf( D.geo, total, true, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, w )
		|| !csmBuf( D.host, kWaterHostBytes, false, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, w ) )
		return breakOff( w );
	std::uint8_t * host = static_cast<std::uint8_t *>( D.host.ptr );
	std::memset( host + kWaterZeroAt, 0, size_t( kWaterVsLooseAt - kWaterZeroAt ) );
	std::memcpy( host + kWaterVsLooseAt, r.vsLoose, size_t( kT_waterVsBytes ) );
	std::memcpy( host + kWaterPsLooseAt, r.psLoose, size_t( kT_waterPsBytes ) );
	if ( r.red & 1 ) {  // red: the ps's loose block one vec4 off (a table offset wrong by a row)
		std::memmove( host + kWaterPsLooseAt + 16, r.psLoose, size_t( kT_waterPsBytes - 16 ) );
		std::memset( host + kWaterPsLooseAt, 0, 16 );
	}
	for ( int i = 0; i < kWaterAttribs; i++ )
		std::memcpy( host + kWaterConstAt + 16 * i, r.attr[i].cur, 16 );
	// the images and texel buffers: GL's names into their twins (each level), the zero ones where GL has none
	std::vector<GLuint> texNames { T.colorTex, T.dsTex };
	std::vector<GLuint> bufNames { D.geo.gl };
	std::vector<VkDescriptorImageInfo> ii( D.binds.size() );
	std::vector<VkDescriptorBufferInfo> bi( D.binds.size() );
	std::vector<VkBufferView> tv( D.binds.size() );
	std::vector<VkWriteDescriptorSet> ws;
	auto slotOf = [&]( std::uint32_t binding, const char ** nameOut ) -> const WaterSlot * {
		for ( const WaterImgName & n : kWaterImg )
			if ( n.binding == binding ) {
				*nameOut = n.name;
				for ( const WaterSlot & s : r.census.inputs )
					if ( s.name == n.name )
						return &s;
				return nullptr;
			}
		*nameOut = nullptr;
		return nullptr;
	};
	for ( std::size_t i = 0; i < D.binds.size(); i++ ) {
		const FarSetSlot & b = D.binds[i];
		VkWriteDescriptorSet x { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
		x.dstSet = D.set;
		x.dstBinding = b.binding;
		x.descriptorCount = 1;
		if ( b.kind == 0 ) {
			x.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			const int k = ( b.binding == 41 || b.binding == 1 ) ? 0 : b.binding == 0 ? 1 : b.binding == 40 ? 2 : -1;
			if ( k >= 0 )
				bi[i] = uboHost[k] ? VkDescriptorBufferInfo { D.host.buf, kWaterZeroAt, uboNeed[k] }
					: VkDescriptorBufferInfo { D.geo.buf, uboAt[k], uboNeed[k] };
			else if ( b.binding == 71 )
				bi[i] = { D.host.buf, kWaterVsLooseAt, VkDeviceSize( kT_waterVsBytes ) };
			else if ( b.binding == 31 )
				bi[i] = { D.host.buf, kWaterPsLooseAt, VkDeviceSize( kT_waterPsBytes ) };
			else
				return breakOff( "the twins read a block at b" + std::to_string( b.binding ) + " the draw does not fill" );
			x.pBufferInfo = &bi[i];
		} else if ( b.kind == 3 ) {
			const char * nm = nullptr;
			const WaterSlot * s = slotOf( b.binding - 64, &nm );
			WaterSlot plain;
			VkSampler sm = waterSampler( s ? *s : plain, b.compare, ( r.red & 2 ) != 0 || !s, w );
			if ( !sm )
				return breakOff( w );
			if ( b.compare && s && s->tex && s->cmpMode != 0x884E )
				return refuse( std::string( nm ? nm : "?" ) + ": the twin compares, GL's texture does not" );
			x.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
			ii[i].sampler = sm;
			x.pImageInfo = &ii[i];
		} else {
			const char * nm = nullptr;
			const WaterSlot * s = slotOf( b.binding, &nm );
			if ( !nm )
				return breakOff( "the twins read an image at b" + std::to_string( b.binding ) + " no census name maps" );
			const std::uintptr_t key = r.key + b.binding;
			if ( b.kind == 2 ) {  // a texel buffer (farStream): GL's data store into the twin, or a zero one
				const bool bound = s && s->tex && s->dataBuf && s->bufBytes > 0;
				unsigned tb = 0, tt = 0;
				if ( !farTexelBuffer( key, bound ? s->glFmt : 0x8814u, bound ? std::size_t( s->bufBytes ) : 16u, tb, tt, w ) )
					return refuse( std::string( nm ) + ": " + w );
				if ( bound )
					g.copyNamedBufferSubData( s->dataBuf, tb, 0, 0, std::intptr_t( s->bufBytes ) );
				bufNames.push_back( tb );
				tv[i] = farCards().bufs[key].view;
				x.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
				x.pTexelBufferView = &tv[i];
				ws.push_back( x );
				continue;
			}
			VkImageView view = VK_NULL_HANDLE;
			if ( !s || !s->tex ) {
				if ( b.dim == 2 )
					view = F.d3;
				else if ( b.arrayed )
					view = F.d2a;
				else
					view = F.d2;
				// an integer image reads GL's unbound zero only through an integer view: refuse it
				for ( const WaterImgName & n : { WaterImgName { 71, "" }, WaterImgName { 72, "" }, WaterImgName { 73, "" },
						 WaterImgName { 74, "" }, WaterImgName { 88, "" }, WaterImgName { 89, "" } } )
					if ( n.binding == b.binding )
						return refuse( std::string( nm ) + ": an unbound integer input" );
			} else if ( s->tex == T.colorTex || s->tex == T.dsTex ) {
				return refuse( std::string( nm ) + ": samples the frame it draws into" );
			} else if ( s->baseLevel != 0 ) {
				return refuse( std::string( nm ) + ": a base level above 0" );
			} else if ( const std::uintptr_t sk = farSharedOf( s->tex ) ) {
				view = sharedFind( sk )->view;  // already on the present device (another pass's target)
				texNames.push_back( s->tex );
			} else {
				unsigned twin = 0;
				if ( !waterTwin( key, *s, twin, w ) )
					return refuse( std::string( nm ) + ": " + w );
				const bool viaFar = s->kind == 4 || waterBcFormat( s->glFmt );
				const Shared * sh = viaFar ? farTwinFind( key ) : sharedFind( key );
				if ( !sh || !sh->view )
					return refuse( std::string( nm ) + ": its twin has no view" );
				view = sh->view;
				static const GLenum kTarget[5] = { 0x0DE1, 0x806F, 0x8C1A, 0x8513, 0x9009 };
				const GLenum target = kTarget[std::clamp( s->kind, 0, 4 )];
				for ( int l = 0; l < std::max( s->levels, 1 ); l++ ) {
					const int lw = std::max( s->w >> l, 1 ), lh = std::max( s->h >> l, 1 );
					const int ld = s->kind == 1 ? std::max( s->d >> l, 1 ) : ( s->kind == 2 || s->kind == 4 ) ? s->d : 1;
					g.copyImageSubData( s->tex, target, l, 0, 0, 0, twin, target, l, 0, 0, 0, lw, lh, ld );
				}
				S.copies++;
				texNames.push_back( twin );
			}
			x.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
			ii[i].imageView = view;
			ii[i].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
			x.pImageInfo = &ii[i];
		}
		ws.push_back( x );
	}
	const VkImageView att[2] = { T.colorView, T.dsView };
	if ( !fxFb( F.fbAir, F.airPass, att, 2, T.w, T.h ) )
		return breakOff( "the water draw's framebuffer over the tone frame failed" );
	D.pipe = waterPipe( r, fmt, w );
	if ( !D.pipe )
		return breakOff( w );
	v.vkUpdateDescriptorSets( v.dev, std::uint32_t( ws.size() ), ws.data(), 0, nullptr );
	// the shape's buffers and GL's blocks into the exported copy (every run: GL may have written them this frame)
	for ( const Part & p : parts )
		g.copyNamedBufferSubData( GLuint( p.buf ), D.geo.gl, std::intptr_t( p.src ), std::intptr_t( p.at ), std::intptr_t( p.bytes ) );
	for ( int i = 0; i < kWaterAttribs; i++ ) {
		D.vb[i] = r.attr[i].on ? D.geo.buf : D.host.buf;
		D.vbOff[i] = r.attr[i].on ? vbAt[i] : kWaterConstAt + 16 * VkDeviceSize( i );
	}
	D.ibOff = ibAt;
	D.count = std::uint32_t( r.count );
	D.first = std::uint32_t( r.idxOff / 2 );
	D.baseVertex = r.baseVertex;
	std::memcpy( D.vp, r.vp, sizeof( D.vp ) );
	int x0 = r.vp[0], y0 = r.vp[1], x1 = r.vp[0] + r.vp[2], y1 = r.vp[1] + r.vp[3];
	if ( r.scissorOn ) {
		x0 = std::max( x0, r.scissor[0] );
		y0 = std::max( y0, r.scissor[1] );
		x1 = std::min( x1, r.scissor[0] + r.scissor[2] );
		y1 = std::min( y1, r.scissor[1] + r.scissor[3] );
	}
	x0 = std::clamp( x0, 0, T.w );
	y0 = std::clamp( y0, 0, T.h );
	x1 = std::clamp( x1, 0, T.w );
	y1 = std::clamp( y1, 0, T.h );
	D.sc = { { x0, y0 }, { std::uint32_t( std::max( x1 - x0, 0 ) ), std::uint32_t( std::max( y1 - y0, 0 ) ) } };
	F.kind = 3;
	F.w = r.w;
	F.h = r.h;
	// GL's work so far signalled (the frame, every twin, the copies) -> the worker's job -> GL waits
	std::vector<GLenum> lay( texNames.size(), kLayoutGeneral );
	g.signalSemaphore( F.glDoneGl, GLuint( bufNames.size() ), bufNames.data(), GLuint( texNames.size() ), texNames.data(), lay.data() );
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
		return breakOff( w );  // nothing signals vkDone: GL does not wait it; GL draws the shape this frame
	g.waitSemaphore( F.vkDoneGl, GLuint( bufNames.size() ), bufNames.data(), GLuint( texNames.size() ), texNames.data(), lay.data() );
	F.pending = true;
	const double ms = nowMs() - t0;
	S.runs++;
	S.cpuMs += ms;
	S.maxMs = std::max( S.maxMs, ms );
	return true;
}

FxStat takeWaterDrawStat()
{
	WaterDraw & D = waterD();
	FxStat s = D.st;
	s.pipelines = D.pipelines;
	s.buildMs = D.buildMs;
	if ( D.broken && s.failWhy.empty() )
		s.failWhy = D.brokenWhy;
	D.st = FxStat();
	return s;
}
}  // namespace WwVkPresent

#ifndef WW_VKPRESENT_NO_QT
namespace WwVkPresent
{
void waterSay( bool wanted, const char * glWhy )
{
	static QString said;
	static double at = 0.0;
	const QString head = QStringLiteral( "vk water: " );
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
	const WaterStat st = takeWaterStat();
	const FxStat ds = takeWaterDrawStat();
	QString line = head + QStringLiteral( "wanted on Vulkan, %1 draws run (%2 ms CPU each, max %3, %4 copies, %5 fails%6); on GL %7 draws" )
		.arg( ds.runs ).arg( ds.runs ? ds.cpuMs / ds.runs : 0.0, 0, 'f', 3 ).arg( ds.maxMs, 0, 'f', 3 ).arg( ds.copies ).arg( ds.fails )
		.arg( ds.failWhy.empty() ? QString() : QStringLiteral( ": " ) + QString::fromStdString( ds.failWhy ) ).arg( st.gl );
	if ( !st.glWhy.empty() )
		line += QStringLiteral( " (blocked: %1)" ).arg( QString::fromStdString( st.glWhy ) );
	if ( st.census > 0 )
		line += QStringLiteral( "; %1 censuses, %2 ms each" ).arg( st.census ).arg( st.censusMs / st.census, 0, 'f', 3 );
	said = line;
	qInfo().noquote() << line;
}
}  // namespace WwVkPresent
#endif
