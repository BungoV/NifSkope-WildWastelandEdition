/* Lane VKWATER1 (2026-11-13, plan row G5): the water's route on the present device (vkwater.h). Included at the end of
 * vkpresent.cpp (its vk() / vkFormatOf / nowMs). No draw moves yet: every refusal is named slot by slot, and GL draws. */
#include "features/vkframe/vkwater.h"
#include "features/vkframe/water_view_vs.spv.h"  // lane VKWATER3: the shaded draw's twins (vkframe_spirv.sh)
#include "features/vkframe/water_view_ps.spv.h"
#include "features/vkframe/water_view_hs.spv.h"  // item 4: the tessellated variant (fo4_waterlookd)
#include "features/vkframe/water_view_ds.spv.h"
#include "features/vkframe/water_view_d_ps.spv.h"
#include "features/vkframe/water_view_m_ps.spv.h"  // piece 2: fo4_waterlookm, fo4_waterlookp, fo4_waterlookdp
#include "features/vkframe/water_view_p_ps.spv.h"
#include "features/vkframe/water_view_dp_ps.spv.h"

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

/* Lane VKWATER3: the shaded draw. water_view_vs (fo4_default) + the variant's pixel twin (fo4_waterlook*, FOGLEAK +
 * FRAME_UBO), a tessellated variant with water_view_hs + water_view_ds between them, over the shape's GL buffers copied
 * into one exported buffer; the loose uniforms into a host buffer; every input through waterTwin; one VkSampler per GL
 * sampling state; the air's render pass over the tone frame; fx's crossing (kind 3). Every binding's census name is the
 * module's own (vkwater_tables.h kN_water*, read from the SPIR-V: a sampler under its image's name). */
constexpr VkDeviceSize kWaterAlign = 256;
constexpr VkDeviceSize waterUp( VkDeviceSize n )
{
	return ( n + kWaterAlign - 1 ) / kWaterAlign * kWaterAlign;
}
// the host buffer: zeros (a block GL has no buffer for), the loose blocks per stage, the disabled attributes' values
constexpr VkDeviceSize kWaterZeroAt = 0, kWaterVsLooseAt = 12288;
constexpr VkDeviceSize kWaterHsLooseAt = waterUp( kWaterVsLooseAt + WwVkPresent::kT_waterVsBytes );
constexpr VkDeviceSize kWaterDsLooseAt = waterUp( kWaterHsLooseAt + WwVkPresent::kT_waterHsBytes );
constexpr VkDeviceSize kWaterPsLooseAt = waterUp( kWaterDsLooseAt + WwVkPresent::kT_waterDsBytes );
constexpr VkDeviceSize kWaterConstAt = waterUp( kWaterPsLooseAt + WwVkPresent::kWaterPsLooseMax );
constexpr VkDeviceSize kWaterHostBytes = kWaterConstAt + kWaterAlign;
static_assert( 16 * WwVkPresent::kWaterAttribs <= int( kWaterAlign ), "the disabled attributes' values fit their row" );

// one variant's stages (vs, hs, ds, ps; a missing one is null) and their binding names
struct WaterStage
{
	const std::uint32_t * words;
	std::size_t bytes;
	VkShaderStageFlagBits stage;
	const WwVkPresent::WaterBindName * names;
	int namesN;
};
int waterStages( int v, WaterStage * out )
{
	using namespace WwVkPresent;
	const WaterTables & t = waterTables( v );
	int n = 0;
	out[n++] = { vkframe_water_view_vs_spv, sizeof( vkframe_water_view_vs_spv ), VK_SHADER_STAGE_VERTEX_BIT, kN_waterVs, waterN( kN_waterVs ) };
	if ( t.tess ) {
		out[n++] = { vkframe_water_view_hs_spv, sizeof( vkframe_water_view_hs_spv ), VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT, kN_waterHs,
			waterN( kN_waterHs ) };
		out[n++] = { vkframe_water_view_ds_spv, sizeof( vkframe_water_view_ds_spv ), VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT,
			kN_waterDs, waterN( kN_waterDs ) };
	}
	const std::uint32_t * ps = vkframe_water_view_ps_spv;
	std::size_t psBytes = sizeof( vkframe_water_view_ps_spv );
	switch ( v ) {
	case kWaterLookD: ps = vkframe_water_view_d_ps_spv, psBytes = sizeof( vkframe_water_view_d_ps_spv ); break;
	case kWaterLookM: ps = vkframe_water_view_m_ps_spv, psBytes = sizeof( vkframe_water_view_m_ps_spv ); break;
	case kWaterLookP: ps = vkframe_water_view_p_ps_spv, psBytes = sizeof( vkframe_water_view_p_ps_spv ); break;
	case kWaterLookDp: ps = vkframe_water_view_dp_ps_spv, psBytes = sizeof( vkframe_water_view_dp_ps_spv ); break;
	default: break;
	}
	out[n++] = { ps, psBytes, VK_SHADER_STAGE_FRAGMENT_BIT, t.psNames, t.psNamesN };
	return n;
}

// one variant's device parts: modules, the merged set (every stage reflected), its pool, one set, layout, pipelines
struct WaterProg
{
	bool made = false;
	int stageN = 0;
	VkShaderModule mod[4] = {};
	VkShaderStageFlagBits stage[4] = {};
	std::vector<WwVkPresent::FarSetSlot> binds;
	std::vector<VkShaderStageFlags> stages;
	std::vector<const char *> names;  // per bind: its census name (the module's)
	VkDescriptorSetLayout dsl = VK_NULL_HANDLE;
	VkDescriptorPool pool = VK_NULL_HANDLE;
	VkDescriptorSet set = VK_NULL_HANDLE;
	VkPipelineLayout layout = VK_NULL_HANDLE;
	std::map<std::string, VkPipeline> pipes;
	VkRenderPass pipePass = VK_NULL_HANDLE;
	int pipeSamples = 0;
};

struct WaterDraw
{
	bool made = false, broken = false;
	std::string brokenWhy;
	WaterProg prog[WwVkPresent::kWaterVariants];
	std::map<std::string, VkSampler> smp;
	CsmBuf geo, host;
	// the zero shadow cube (an input GL compiled out and the twin keeps): D16 1x1, one cube, depth 1, GENERAL
	VkImage zeroCube = VK_NULL_HANDLE;
	VkDeviceMemory zeroCubeMem = VK_NULL_HANDLE;
	VkImageView zeroCubeView = VK_NULL_HANDLE;
	PFN_vkCmdDrawIndexed drawIndexed = nullptr;  // not in the device table (Vk): loaded here
	// this run's record (written before the hand)
	int cur = 0;  // the variant
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

bool waterZeroCube( WaterDraw & D, std::string & why )
{
	Vk & v = vk();
	const auto clearDs = reinterpret_cast<PFN_vkCmdClearDepthStencilImage>( v.vkGetDeviceProcAddr( v.dev, "vkCmdClearDepthStencilImage" ) );
	if ( !clearDs ) {
		why = "no vkCmdClearDepthStencilImage";
		return false;
	}
	VkImageCreateInfo ci { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
	ci.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
	ci.imageType = VK_IMAGE_TYPE_2D;
	ci.format = VK_FORMAT_D16_UNORM;
	ci.extent = { 1, 1, 1 };
	ci.mipLevels = 1;
	ci.arrayLayers = 6;
	ci.samples = VK_SAMPLE_COUNT_1_BIT;
	ci.tiling = VK_IMAGE_TILING_OPTIMAL;
	ci.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	if ( v.vkCreateImage( v.dev, &ci, nullptr, &D.zeroCube ) != VK_SUCCESS ) {
		why = "the water draw's zero cube: vkCreateImage failed";
		return false;
	}
	VkMemoryRequirements r;
	v.vkGetImageMemoryRequirements( v.dev, D.zeroCube, &r );
	VkMemoryAllocateInfo ai { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
	ai.allocationSize = r.size;
	ai.memoryTypeIndex = memType( r.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );
	if ( ai.memoryTypeIndex == UINT32_MAX || v.vkAllocateMemory( v.dev, &ai, nullptr, &D.zeroCubeMem ) != VK_SUCCESS
		|| v.vkBindImageMemory( v.dev, D.zeroCube, D.zeroCubeMem, 0 ) != VK_SUCCESS ) {
		why = "the water draw's zero cube: no device memory";
		return false;
	}
	VkCommandBufferAllocateInfo cai { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
	cai.commandPool = v.pool;
	cai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	cai.commandBufferCount = 1;
	VkCommandBuffer cmd = VK_NULL_HANDLE;
	VkFence fence = VK_NULL_HANDLE;
	VkFenceCreateInfo fi { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
	bool laid = false;
	if ( v.vkAllocateCommandBuffers( v.dev, &cai, &cmd ) == VK_SUCCESS && v.vkCreateFence( v.dev, &fi, nullptr, &fence ) == VK_SUCCESS ) {
		VkCommandBufferBeginInfo bi { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
		bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		v.vkBeginCommandBuffer( cmd, &bi );
		const VkImageSubresourceRange rr { VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 6 };
		VkImageMemoryBarrier b { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
		b.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		b.newLayout = VK_IMAGE_LAYOUT_GENERAL;
		b.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		b.srcQueueFamilyIndex = b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		b.image = D.zeroCube;
		b.subresourceRange = rr;
		v.vkCmdPipelineBarrier( cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &b );
		const VkClearDepthStencilValue one { 1.0f, 0 };
		clearDs( cmd, D.zeroCube, VK_IMAGE_LAYOUT_GENERAL, &one, 1, &rr );
		b.oldLayout = b.newLayout = VK_IMAGE_LAYOUT_GENERAL;
		b.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		b.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		v.vkCmdPipelineBarrier( cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &b );
		v.vkEndCommandBuffer( cmd );
		laid = oneShot( cmd, fence );
	}
	if ( cmd )
		v.vkFreeCommandBuffers( v.dev, v.pool, 1, &cmd );
	if ( fence )
		v.vkDestroyFence( v.dev, fence, nullptr );
	D.zeroCubeView = laid ? imageView( D.zeroCube, VK_IMAGE_VIEW_TYPE_CUBE_ARRAY, VK_FORMAT_D16_UNORM, VK_IMAGE_ASPECT_DEPTH_BIT, 1, 6 )
		: VK_NULL_HANDLE;
	if ( !D.zeroCubeView ) {
		why = "the water draw's zero cube: clear / view failed";
		return false;
	}
	return true;
}

// the device-time parts every variant shares (once): vkCmdDrawIndexed, the zero cube
bool waterStatic( std::string & why )
{
	Vk & v = vk();
	WaterDraw & D = waterD();
	if ( D.made )
		return true;
	D.drawIndexed = reinterpret_cast<PFN_vkCmdDrawIndexed>( v.vkGetDeviceProcAddr( v.dev, "vkCmdDrawIndexed" ) );
	if ( !D.drawIndexed ) {
		why = "no vkCmdDrawIndexed";
		return false;
	}
	std::string cw;
	if ( fcFeatures( false, true, cw ) && !waterZeroCube( D, why ) )
		return false;
	D.made = true;
	return true;
}

// one variant's parts (on its first draw): every stage reflected into one set, each binding named by its module
bool waterProgMake( int variant, std::string & why )
{
	Vk & v = vk();
	WaterProg & P = waterD().prog[variant];
	if ( P.made )
		return true;
	WaterStage st[4];
	P.stageN = waterStages( variant, st );
	P.binds.clear();
	P.stages.clear();
	P.names.clear();
	for ( int s = 0; s < P.stageN; s++ ) {
		WwVkPresent::FarSetCounts c;
		if ( !WwVkPresent::farSetReflect( st[s].words, st[s].bytes / 4, c, why ) ) {
			why = "the water twins' reflection: " + why;
			return false;
		}
		for ( const WwVkPresent::FarSetSlot & b : c.binds ) {
			for ( const WwVkPresent::FarSetSlot & o : P.binds )
				if ( o.binding == b.binding ) {
					why = "two water twins declare binding " + std::to_string( b.binding );
					return false;
				}
			const char * nm = nullptr;
			for ( int k = 0; k < st[s].namesN; k++ )
				if ( st[s].names[k].binding == b.binding )
					nm = st[s].names[k].name;
			if ( !nm ) {
				why = "the water twins' binding " + std::to_string( b.binding ) + " has no name in vkwater_tables.h";
				return false;
			}
			P.binds.push_back( b );
			P.stages.push_back( st[s].stage );
			P.names.push_back( nm );
		}
	}
	std::vector<VkDescriptorSetLayoutBinding> lb;
	std::uint32_t n[4] = {};
	static const VkDescriptorType kType[4] = { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
		VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, VK_DESCRIPTOR_TYPE_SAMPLER };
	for ( std::size_t i = 0; i < P.binds.size(); i++ ) {
		VkDescriptorSetLayoutBinding b {};
		b.binding = P.binds[i].binding;
		b.descriptorType = kType[P.binds[i].kind];
		b.descriptorCount = 1;
		b.stageFlags = P.stages[i];
		lb.push_back( b );
		n[P.binds[i].kind]++;
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
	if ( v.vkCreateDescriptorSetLayout( v.dev, &li, nullptr, &P.dsl ) != VK_SUCCESS
		|| v.vkCreateDescriptorPool( v.dev, &pi, nullptr, &P.pool ) != VK_SUCCESS ) {
		why = "the water draw's set layout / pool failed";
		return false;
	}
	VkDescriptorSetAllocateInfo ai { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
	ai.descriptorPool = P.pool;
	ai.descriptorSetCount = 1;
	ai.pSetLayouts = &P.dsl;
	VkPipelineLayoutCreateInfo pli { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
	pli.setLayoutCount = 1;
	pli.pSetLayouts = &P.dsl;
	if ( v.vkAllocateDescriptorSets( v.dev, &ai, &P.set ) != VK_SUCCESS
		|| v.vkCreatePipelineLayout( v.dev, &pli, nullptr, &P.layout ) != VK_SUCCESS ) {
		why = "the water draw's set / layout failed";
		return false;
	}
	for ( int s = 0; s < P.stageN; s++ ) {
		P.stage[s] = st[s].stage;
		if ( !fxModule( st[s].words, st[s].bytes, P.mod[s] ) ) {
			why = "the water draw's modules failed";
			return false;
		}
	}
	P.made = true;
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
	WaterProg & P = D.prog[r.variant];
	const bool tess = WwVkPresent::waterTables( r.variant ).tess;
	if ( P.pipePass != F.airPass || P.pipeSamples != F.airSamples ) {
		for ( auto & kv : P.pipes )
			v.vkDestroyPipeline( v.dev, kv.second, nullptr );
		P.pipes.clear();
		P.pipePass = F.airPass;
		P.pipeSamples = F.airSamples;
	}
	// GL's tessellator: its domain's origin lower-left (red 16: Vulkan's default upper-left, the triangles' order turned)
	const bool upperLeft = tess && ( r.red & 16 ) != 0;
	std::string key = std::to_string( int( upperLeft ) ) + " ";
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
	auto it = P.pipes.find( key );
	if ( it != P.pipes.end() )
		return it->second;
	const double t0 = nowMs();
	VkPipelineShaderStageCreateInfo st[4] = {};
	for ( int s = 0; s < P.stageN; s++ ) {
		st[s].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		st[s].stage = P.stage[s];
		st[s].module = P.mod[s];
		st[s].pName = "main";
	}
	VkPipelineVertexInputStateCreateInfo vi { VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
	vi.vertexBindingDescriptionCount = std::uint32_t( WwVkPresent::kWaterAttribs );
	vi.pVertexBindingDescriptions = vb;
	vi.vertexAttributeDescriptionCount = std::uint32_t( WwVkPresent::kWaterAttribs );
	vi.pVertexAttributeDescriptions = va;
	VkPipelineInputAssemblyStateCreateInfo ia { VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
	ia.topology = tess ? VK_PRIMITIVE_TOPOLOGY_PATCH_LIST : VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	VkPipelineTessellationDomainOriginStateCreateInfo dor { VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_DOMAIN_ORIGIN_STATE_CREATE_INFO };
	dor.domainOrigin = upperLeft ? VK_TESSELLATION_DOMAIN_ORIGIN_UPPER_LEFT : VK_TESSELLATION_DOMAIN_ORIGIN_LOWER_LEFT;
	VkPipelineTessellationStateCreateInfo ts { VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO };
	ts.pNext = &dor;
	ts.patchControlPoints = 3;  // waterRun refuses GL_PATCH_VERTICES != 3
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
	gp.stageCount = std::uint32_t( P.stageN );
	gp.pStages = st;
	gp.pVertexInputState = &vi;
	gp.pInputAssemblyState = &ia;
	gp.pTessellationState = tess ? &ts : nullptr;
	gp.pViewportState = &vps;
	gp.pRasterizationState = &rs;
	gp.pMultisampleState = &ms;
	gp.pDepthStencilState = &ds;
	gp.pColorBlendState = &cbs;
	gp.pDynamicState = &dy;
	gp.layout = P.layout;
	gp.renderPass = F.airPass;
	VkPipeline out = VK_NULL_HANDLE;
	if ( v.vkCreateGraphicsPipelines( v.dev, tone().cache, 1, &gp, nullptr, &out ) != VK_SUCCESS ) {
		why = "vkCreateGraphicsPipelines (the water draw) failed";
		return VK_NULL_HANDLE;
	}
	P.pipes[key] = out;
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
	v.vkCmdBindDescriptorSets( F.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, D.prog[D.cur].layout, 0, 1, &D.prog[D.cur].set, 0, nullptr );
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
	if ( r.variant < 0 || r.variant >= kWaterVariants )
		return refuse( "a water program with no Vulkan twin" );
	const WaterTables & tab = waterTables( r.variant );
	if ( r.census.tess != tab.tess )
		return refuse( std::string( tab.prog ) + ": GL's program is " + ( r.census.tess ? "" : "not " ) + "tessellated, its twin is "
			+ ( tab.tess ? "" : "not" ) );
	if ( tab.tess && r.patchVerts != 3 )
		return refuse( "GL draws " + std::to_string( r.patchVerts ) + "-vertex patches (the twin's are 3)" );
	if ( r.census.inputs.size() >= 120 )
		return refuse( "more than 120 census inputs (the twins' keys)" );
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
	if ( !fxStatic( w ) || !fxSems( w ) || !fxPasses( samples, w ) || !waterStatic( w ) || !waterProgMake( r.variant, w ) )
		return breakOff( w );
	WaterProg & P = D.prog[r.variant];
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
	// the twins' block sizes (globalUniforms in every stage, wwShadowFrame, skinningUniforms): GL's range, never past the copy
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
	std::memcpy( host + kWaterHsLooseAt, r.hsLoose, size_t( kT_waterHsBytes ) );
	std::memcpy( host + kWaterDsLooseAt, r.dsLoose, size_t( kT_waterDsBytes ) );
	std::memcpy( host + kWaterPsLooseAt, r.psLoose, size_t( tab.psBytes ) );
	if ( r.red & 1 ) {  // red: the ps's loose block one vec4 off (a table offset wrong by a row)
		std::memmove( host + kWaterPsLooseAt + 16, r.psLoose, size_t( tab.psBytes - 16 ) );
		std::memset( host + kWaterPsLooseAt, 0, 16 );
	}
	if ( r.red & 32 ) {  // red: the ds's loose block one vec4 off
		std::memmove( host + kWaterDsLooseAt + 16, r.dsLoose, size_t( kT_waterDsBytes - 16 ) );
		std::memset( host + kWaterDsLooseAt, 0, 16 );
	}
	for ( int i = 0; i < kWaterAttribs; i++ )
		std::memcpy( host + kWaterConstAt + 16 * i, r.attr[i].cur, 16 );
	// the images and texel buffers: GL's names into their twins (each level), the zero ones where GL has none
	std::vector<GLuint> texNames { T.colorTex, T.dsTex };
	std::vector<GLuint> bufNames { D.geo.gl };
	std::vector<VkDescriptorImageInfo> ii( P.binds.size() );
	std::vector<VkDescriptorBufferInfo> bi( P.binds.size() );
	std::vector<VkBufferView> tv( P.binds.size() );
	std::vector<VkWriteDescriptorSet> ws;
	// a census input by the module's name; its index is its twins' key (key + index: one twin for every stage reading it)
	auto slotOf = [&]( const char * nm, std::size_t & at ) -> const WaterSlot * {
		for ( std::size_t k = 0; k < r.census.inputs.size(); k++ )
			if ( r.census.inputs[k].name == nm ) {
				at = k;
				return &r.census.inputs[k];
			}
		return nullptr;
	};
	std::map<std::size_t, VkImageView> viewOf;  // this run's views by census index (a second stage reads the first's)
	std::size_t zeroKeys = r.census.inputs.size();  // an unbound texel buffer's key: past the census
	for ( std::size_t i = 0; i < P.binds.size(); i++ ) {
		const FarSetSlot & b = P.binds[i];
		const char * nm = P.names[i];
		VkWriteDescriptorSet x { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
		x.dstSet = P.set;
		x.dstBinding = b.binding;
		x.descriptorCount = 1;
		if ( b.kind == 0 ) {
			x.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			const int k = std::strcmp( nm, "globalUniforms" ) == 0 ? 0 : std::strcmp( nm, "wwShadowFrame" ) == 0 ? 1
				: std::strcmp( nm, "skinningUniforms" ) == 0 ? 2 : -1;
			if ( k >= 0 )
				bi[i] = uboHost[k] ? VkDescriptorBufferInfo { D.host.buf, kWaterZeroAt, uboNeed[k] }
					: VkDescriptorBufferInfo { D.geo.buf, uboAt[k], uboNeed[k] };
			else if ( std::strcmp( nm, "LooseUniforms" ) != 0 )
				return breakOff( std::string( "the twins read a block " ) + nm + " the draw does not fill" );
			else if ( b.binding >= 768 )  // the stage's block shift (vkframe_spirv.sh: ps 0, vs 256, hs 512, ds 768)
				bi[i] = { D.host.buf, kWaterDsLooseAt, VkDeviceSize( kT_waterDsBytes ) };
			else if ( b.binding >= 512 )
				bi[i] = { D.host.buf, kWaterHsLooseAt, VkDeviceSize( kT_waterHsBytes ) };
			else if ( b.binding >= 256 )
				bi[i] = { D.host.buf, kWaterVsLooseAt, VkDeviceSize( kT_waterVsBytes ) };
			else
				bi[i] = { D.host.buf, kWaterPsLooseAt, VkDeviceSize( tab.psBytes ) };
			x.pBufferInfo = &bi[i];
		} else if ( b.kind == 3 ) {
			std::size_t at = 0;
			const WaterSlot * s = slotOf( nm, at );
			WaterSlot plain;
			VkSampler sm = waterSampler( s ? *s : plain, b.compare, ( r.red & 2 ) != 0 || !s, w );
			if ( !sm )
				return breakOff( w );
			if ( b.compare && s && s->tex && s->cmpMode != 0x884E )
				return refuse( std::string( nm ) + ": the twin compares, GL's texture does not" );
			x.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
			ii[i].sampler = sm;
			x.pImageInfo = &ii[i];
		} else {
			std::size_t at = 0;
			const WaterSlot * s = slotOf( nm, at );
			if ( !s )
				at = zeroKeys++;
			if ( at >= 128 )
				return refuse( std::string( nm ) + ": past the twins' 128 keys" );
			const std::uintptr_t key = r.key + at;
			if ( b.kind == 2 ) {  // a texel buffer (farStream): GL's data store into the twin, or a zero one
				const bool bound = s && s->tex && s->dataBuf && s->bufBytes > 0;
				unsigned tb = 0, tt = 0;
				const unsigned zeroFmt = std::strcmp( nm, "cellCluster" ) == 0 ? 0x8D82u : 0x8814u;  // Buffer<int4>: RGBA32I
				if ( !farTexelBuffer( key, bound ? s->glFmt : zeroFmt, bound ? std::size_t( s->bufBytes ) : 16u, tb, tt, w ) )
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
				if ( b.dim == 3 && !( b.arrayed && D.zeroCubeView ) )
					return refuse( std::string( nm ) + ": an unbound cube input (no zero cube view)" );
				if ( b.dim == 3 )
					view = D.zeroCubeView;
				else if ( b.dim == 2 )
					view = F.d3;
				else if ( b.arrayed )
					view = F.d2a;
				else
					view = F.d2;
				// an integer image reads GL's unbound zero only through an integer view: refuse it
				for ( const char * n : { "waterInfoIndex", "waterInfoAtlas", "waterFlowIndex", "waterFlowAtlas", "waterSkyIndex", "waterSkyAtlas" } )
					if ( std::strcmp( n, nm ) == 0 )
						return refuse( std::string( nm ) + ": an unbound integer input" );
			} else if ( viewOf.count( at ) ) {
				view = viewOf[at];  // another stage's binding twinned it this run
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
			if ( s && s->tex )
				viewOf[at] = view;
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
	D.cur = r.variant;
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
