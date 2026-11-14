// Lane VKFAR2 P7 (2026-11-14): cellpost.cpp's depth of field (the game's passes or the thin lens), motion blur (alone or fused
// with the depth of field) and object blur recorded in the deferred tone pass, after its modes and before the flare (GL's
// order: the post passes, the flare, the TAA). toneRecord calls p7Record: the pass ends, the view's samples and depth are
// copied, resolved as GL's blits keep them (post7_resolve.hlsl), the kernels run as compute (res/shaders/hlsl/post, the
// offline-proven twins of the GL programs), the pass begins again and post7_copy_ps.hlsl writes the picture into every sample
// (game_taa_copy's draw). Included by vkpresent.cpp after vkflare.inc (flDefAdd). The API: vkpresent.h (PostRun).
#include "features/vkframe/p7_res.spv.h"
#include "features/vkframe/p7_dep.spv.h"
#include "features/vkframe/p7_copy_ps.spv.h"
#include "features/vkframe/p7_dof_down.spv.h"
#include "features/vkframe/p7_dof_blur.spv.h"
#include "features/vkframe/p7_dof_comp.spv.h"
#include "features/vkframe/p7_lens_comp.spv.h"
#include "features/vkframe/p7_lens_prep.spv.h"
#include "features/vkframe/p7_lens_tile.spv.h"
#include "features/vkframe/p7_lens_gather.spv.h"
#include "features/vkframe/p7_mblur.spv.h"
#include "features/vkframe/p7_dofmblur.spv.h"
#include "features/vkframe/p7_lensmblur.spv.h"
#include "features/vkframe/p7_mv.spv.h"
#include "features/vkframe/p7_ob_vec.spv.h"
#include "features/vkframe/p7_ob_tile.spv.h"
#include "features/vkframe/p7_ob.spv.h"

namespace
{
enum P7Pipe { kP7Res, kP7Dep, kP7Down, kP7Blur, kP7Comp, kP7LensComp, kP7LensPrep, kP7LensTile, kP7Gather, kP7Mblur, kP7DofMblur,
	kP7LensMblur, kP7Mv, kP7ObVec, kP7ObTile, kP7Ob, kP7Pipes };
constexpr int kP7Sets = 24;      // one per op (at most 20) + the copy back's
constexpr int kP7Ops = 24;
constexpr int kP7LensTileSize = 8;  // cellpost.cpp's kLensTile

// one recorded step: a dispatch (its own set and push block), an image copy, or a clear of up to two images
struct P7Op
{
	int kind = 0;  // 0 dispatch, 1 copy src -> dst, 2 clear
	int pipe = 0, set = 0, w = 0, h = 0;
	WwVkPresent::PfxJob job;
	VkImage a = VK_NULL_HANDLE, b = VK_NULL_HANDLE;
	int w2 = 0, h2 = 0;
};

struct Post7
{
	bool made = false, broken = false;
	std::string brokenWhy;
	VkDescriptorSetLayout dsl = VK_NULL_HANDLE;
	VkDescriptorPool dpool = VK_NULL_HANDLE;
	VkDescriptorSet set[kP7Sets] = {};
	VkPipelineLayout layout = VK_NULL_HANDLE;
	VkSampler linear = VK_NULL_HANDLE, point = VK_NULL_HANDLE;
	VkShaderModule copyPs = VK_NULL_HANDLE;
	VkPipeline pipe[kP7Pipes] = {};
	VkPipeline copy = VK_NULL_HANDLE;
	VkRenderPass copyPass = VK_NULL_HANDLE;
	int copySamples = 0, pipelines = 0;
	double buildMs = 0.0;
	// the images, per frame shape (W x H, the view's samples); fresh: made since the last record (UNDEFINED)
	int W = 0, H = 0, S = 0;
	BloomImg msCol, cur, depth, blur[2], mv, out, lensFar, lensNear, lensTile[2], obVec, obTile[2], fill;
	VkImage msDep = VK_NULL_HANDLE;
	VkDeviceMemory msDepMem = VK_NULL_HANDLE;
	VkImageView msDepView = VK_NULL_HANDLE;
	int qw[2] = {}, qh[2] = {}, ltw = 0, lth = 0, otw = 0, oth = 0, obTileSize = 0;
	double mb = 0.0;
	bool fresh = false;
	// this pass (written by the drawing thread after toneRun, recorded by the worker)
	bool queued = false;
	int slot = -1;
	int nOps = 0;
	P7Op ops[kP7Ops];
	int copySet = 0;
	int vp[4] = {};
	WwVkPresent::FxStat st;
};

Post7 & post7()
{
	static Post7 p;
	return p;
}

std::uintptr_t p7VelKey()
{
	return reinterpret_cast<std::uintptr_t>( &post7() );
}

/* device-time parts (once): the set layout (0-4 sampled, 5 linear, 6 point, 7 / 8 storage; compute and the copy's fragment
 * stage), the pool, the samplers and the 16 compute pipelines */
bool p7Static( std::string & why )
{
	Vk & v = vk();
	Tone & T = tone();
	Post7 & P = post7();
	if ( P.made )
		return true;
	const double t0 = nowMs();
	const VkShaderStageFlags st = VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
	VkDescriptorSetLayoutBinding b[9] = {};
	for ( std::uint32_t k = 0; k < 9; k++ ) {
		b[k].binding = k;
		b[k].descriptorCount = 1;
		b[k].stageFlags = st;
		b[k].descriptorType = k < 5 ? VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE : k < 7 ? VK_DESCRIPTOR_TYPE_SAMPLER : VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	}
	VkDescriptorSetLayoutCreateInfo lci { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
	lci.bindingCount = 9;
	lci.pBindings = b;
	const VkDescriptorPoolSize ps[3] = { { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 5 * kP7Sets }, { VK_DESCRIPTOR_TYPE_SAMPLER, 2 * kP7Sets },
		{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 2 * kP7Sets } };
	VkDescriptorPoolCreateInfo pci { VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
	pci.maxSets = kP7Sets;
	pci.poolSizeCount = 3;
	pci.pPoolSizes = ps;
	if ( v.vkCreateDescriptorSetLayout( v.dev, &lci, nullptr, &P.dsl ) != VK_SUCCESS
		|| v.vkCreateDescriptorPool( v.dev, &pci, nullptr, &P.dpool ) != VK_SUCCESS ) {
		why = "the post passes' set layout or pool failed";
		return false;
	}
	VkDescriptorSetLayout dsls[kP7Sets];
	for ( VkDescriptorSetLayout & d : dsls )
		d = P.dsl;
	VkDescriptorSetAllocateInfo dai { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
	dai.descriptorPool = P.dpool;
	dai.descriptorSetCount = kP7Sets;
	dai.pSetLayouts = dsls;
	const VkPushConstantRange pr { st, 0, std::uint32_t( sizeof( WwVkPresent::PfxJob ) ) };
	VkPipelineLayoutCreateInfo plci { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
	plci.setLayoutCount = 1;
	plci.pSetLayouts = &P.dsl;
	plci.pushConstantRangeCount = 1;
	plci.pPushConstantRanges = &pr;
	VkSamplerCreateInfo sci { VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
	sci.magFilter = sci.minFilter = VK_FILTER_LINEAR;
	sci.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
	sci.addressModeU = sci.addressModeV = sci.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	sci.maxLod = 0.0f;
	VkSamplerCreateInfo nci = sci;
	nci.magFilter = nci.minFilter = VK_FILTER_NEAREST;
	VkShaderModuleCreateInfo mci { VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
	mci.codeSize = sizeof( vkframe_p7_copy_ps_spv );
	mci.pCode = vkframe_p7_copy_ps_spv;
	if ( v.vkAllocateDescriptorSets( v.dev, &dai, P.set ) != VK_SUCCESS
		|| v.vkCreatePipelineLayout( v.dev, &plci, nullptr, &P.layout ) != VK_SUCCESS
		|| v.vkCreateSampler( v.dev, &sci, nullptr, &P.linear ) != VK_SUCCESS
		|| v.vkCreateSampler( v.dev, &nci, nullptr, &P.point ) != VK_SUCCESS
		|| v.vkCreateShaderModule( v.dev, &mci, nullptr, &P.copyPs ) != VK_SUCCESS ) {
		why = "the post passes' sets, layout, samplers or copy module failed";
		return false;
	}
	struct Code
	{
		const std::uint32_t * code;
		std::size_t bytes;
	};
	const Code codes[kP7Pipes] = { { vkframe_p7_res_spv, sizeof( vkframe_p7_res_spv ) }, { vkframe_p7_dep_spv, sizeof( vkframe_p7_dep_spv ) },
		{ vkframe_p7_dof_down_spv, sizeof( vkframe_p7_dof_down_spv ) }, { vkframe_p7_dof_blur_spv, sizeof( vkframe_p7_dof_blur_spv ) },
		{ vkframe_p7_dof_comp_spv, sizeof( vkframe_p7_dof_comp_spv ) }, { vkframe_p7_lens_comp_spv, sizeof( vkframe_p7_lens_comp_spv ) },
		{ vkframe_p7_lens_prep_spv, sizeof( vkframe_p7_lens_prep_spv ) }, { vkframe_p7_lens_tile_spv, sizeof( vkframe_p7_lens_tile_spv ) },
		{ vkframe_p7_lens_gather_spv, sizeof( vkframe_p7_lens_gather_spv ) }, { vkframe_p7_mblur_spv, sizeof( vkframe_p7_mblur_spv ) },
		{ vkframe_p7_dofmblur_spv, sizeof( vkframe_p7_dofmblur_spv ) }, { vkframe_p7_lensmblur_spv, sizeof( vkframe_p7_lensmblur_spv ) },
		{ vkframe_p7_mv_spv, sizeof( vkframe_p7_mv_spv ) }, { vkframe_p7_ob_vec_spv, sizeof( vkframe_p7_ob_vec_spv ) },
		{ vkframe_p7_ob_tile_spv, sizeof( vkframe_p7_ob_tile_spv ) }, { vkframe_p7_ob_spv, sizeof( vkframe_p7_ob_spv ) } };
	for ( int k = 0; k < kP7Pipes; k++ ) {
		VkShaderModuleCreateInfo cm { VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
		cm.codeSize = codes[k].bytes;
		cm.pCode = codes[k].code;
		VkShaderModule mod = VK_NULL_HANDLE;
		if ( v.vkCreateShaderModule( v.dev, &cm, nullptr, &mod ) != VK_SUCCESS ) {
			why = "the post passes' kernel " + std::to_string( k ) + ": vkCreateShaderModule failed";
			return false;
		}
		VkComputePipelineCreateInfo cp { VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO };
		cp.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		cp.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
		cp.stage.module = mod;
		cp.stage.pName = "main";
		cp.layout = P.layout;
		const VkResult r = v.vkCreateComputePipelines( v.dev, T.cache, 1, &cp, nullptr, &P.pipe[k] );
		v.vkDestroyShaderModule( v.dev, mod, nullptr );
		if ( r != VK_SUCCESS ) {
			P.pipe[k] = VK_NULL_HANDLE;
			why = "the post passes' kernel " + std::to_string( k ) + ": vkCreateComputePipelines failed";
			return false;
		}
		P.pipelines++;
	}
	toneCacheSave();
	P.buildMs += nowMs() - t0;
	P.made = true;
	return true;
}

/* the copy back in the tone pass (its samples): tone_view_vs's triangle, post7_copy_ps, no blend, no depth or stencil test
 * (game_taa_copy draws with both off), every sample the fragment's value as GL's draw without per-sample shading */
bool p7CopyPipe( std::string & why )
{
	Vk & v = vk();
	Tone & T = tone();
	Post7 & P = post7();
	if ( P.copy && P.copyPass == T.pass && P.copySamples == T.pipeSamples )
		return true;
	const double t0 = nowMs();
	if ( P.copy )
		v.vkDestroyPipeline( v.dev, P.copy, nullptr );
	P.copy = VK_NULL_HANDLE;
	P.copyPass = VK_NULL_HANDLE;
	VkPipelineShaderStageCreateInfo st[2] = { { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO },
		{ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO } };
	st[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
	st[0].module = T.vs;
	st[0].pName = "main";
	st[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	st[1].module = P.copyPs;
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
	gp.layout = P.layout;
	gp.renderPass = T.pass;
	if ( v.vkCreateGraphicsPipelines( v.dev, T.cache, 1, &gp, nullptr, &P.copy ) != VK_SUCCESS ) {
		P.copy = VK_NULL_HANDLE;
		why = "vkCreateGraphicsPipelines (the post passes' copy back, " + std::to_string( T.pipeSamples ) + " samples) failed";
		return false;
	}
	P.copyPass = T.pass;
	P.copySamples = T.pipeSamples;
	P.pipelines++;
	P.buildMs += nowMs() - t0;
	toneCacheSave();
	return true;
}

// every image goes (the caller made sure no tone pass is in flight)
void p7ImagesDrop()
{
	Vk & v = vk();
	Post7 & P = post7();
	for ( BloomImg * I : { &P.msCol, &P.cur, &P.depth, &P.blur[0], &P.blur[1], &P.mv, &P.out, &P.lensFar, &P.lensNear, &P.lensTile[0],
		&P.lensTile[1], &P.obVec, &P.obTile[0], &P.obTile[1], &P.fill } )
		bloomImgDrop( *I, false );
	if ( P.msDepView )
		v.vkDestroyImageView( v.dev, P.msDepView, nullptr );
	if ( P.msDep )
		v.vkDestroyImage( v.dev, P.msDep, nullptr );
	if ( P.msDepMem )
		v.vkFreeMemory( v.dev, P.msDepMem, nullptr );
	P.msDepView = VK_NULL_HANDLE;
	P.msDep = VK_NULL_HANDLE;
	P.msDepMem = VK_NULL_HANDLE;
	P.W = P.H = P.S = 0;
	P.mb = 0.0;
}

// the images for a frame of W x H at S samples (cellpost.cpp's sizes); all of them, whatever runs, so a toggle never remakes
bool p7Images( int W, int H, int S, std::string & why )
{
	Vk & v = vk();
	Post7 & P = post7();
	if ( P.W == W && P.H == H && P.S == S && P.cur.img )
		return true;
	p7ImagesDrop();
	const VkImageUsageFlags rw = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT
		| VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	P.qw[0] = ( W + 3 ) / 4;
	P.qh[0] = ( H + 3 ) / 4;
	P.qw[1] = std::max( W >> 2, 1 );
	P.qh[1] = std::max( H >> 2, 1 );
	P.ltw = ( P.qw[0] + kP7LensTileSize - 1 ) / kP7LensTileSize;
	P.lth = ( P.qh[0] + kP7LensTileSize - 1 ) / kP7LensTileSize;
	const VkFormat f16 = VK_FORMAT_R16G16B16A16_SFLOAT, rg16 = VK_FORMAT_R16G16_SFLOAT;
	bool ok = bloomLocal( P.msCol, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, W, H, 1, P.mb, S )
		&& bloomLocal( P.cur, f16, rw, W, H, 1, P.mb ) && bloomLocal( P.depth, VK_FORMAT_R32_SFLOAT, rw, W, H, 1, P.mb )
		&& bloomLocal( P.blur[0], f16, rw, P.qw[0], P.qh[0], 1, P.mb ) && bloomLocal( P.blur[1], f16, rw, P.qw[1], P.qh[1], 1, P.mb )
		&& bloomLocal( P.mv, rg16, rw, W, H, 1, P.mb ) && bloomLocal( P.out, f16, rw, W, H, 1, P.mb )
		&& bloomLocal( P.lensFar, f16, rw, P.qw[0], P.qh[0], 1, P.mb ) && bloomLocal( P.lensNear, f16, rw, P.qw[0], P.qh[0], 1, P.mb )
		&& bloomLocal( P.lensTile[0], VK_FORMAT_R16_SFLOAT, rw, P.ltw, P.lth, 1, P.mb )
		&& bloomLocal( P.lensTile[1], VK_FORMAT_R16_SFLOAT, rw, P.ltw, P.lth, 1, P.mb )
		&& bloomLocal( P.obVec, rg16, rw, W, H, 1, P.mb ) && bloomLocal( P.fill, f16, rw, 1, 1, 1, P.mb );
	if ( ok ) {
		VkImageCreateInfo ci { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
		ci.imageType = VK_IMAGE_TYPE_2D;
		ci.format = VK_FORMAT_D24_UNORM_S8_UINT;
		ci.extent = { std::uint32_t( W ), std::uint32_t( H ), 1 };
		ci.mipLevels = 1;
		ci.arrayLayers = 1;
		ci.samples = VkSampleCountFlagBits( S );
		ci.tiling = VK_IMAGE_TILING_OPTIMAL;
		ci.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
		ok = v.vkCreateImage( v.dev, &ci, nullptr, &P.msDep ) == VK_SUCCESS;
		if ( ok ) {
			VkMemoryRequirements r;
			v.vkGetImageMemoryRequirements( v.dev, P.msDep, &r );
			VkMemoryAllocateInfo ai { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
			ai.allocationSize = r.size;
			ai.memoryTypeIndex = memType( r.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );
			ok = ai.memoryTypeIndex != UINT32_MAX && v.vkAllocateMemory( v.dev, &ai, nullptr, &P.msDepMem ) == VK_SUCCESS
				&& v.vkBindImageMemory( v.dev, P.msDep, P.msDepMem, 0 ) == VK_SUCCESS
				&& ( P.msDepView = imageView( P.msDep, VK_IMAGE_VIEW_TYPE_2D, VK_FORMAT_D24_UNORM_S8_UINT, VK_IMAGE_ASPECT_DEPTH_BIT ) );
			P.mb += double( r.size ) / 1048576.0;
		}
	}
	if ( !ok ) {
		p7ImagesDrop();
		why = "the post passes' images (" + std::to_string( W ) + " x " + std::to_string( H ) + ", " + std::to_string( S ) + " samples) failed";
		return false;
	}
	P.W = W;
	P.H = H;
	P.S = S;
	P.obTileSize = 0;
	P.fresh = true;
	return true;
}

// the object blur's tiles (their size is the tile's): made again when the tile changes
bool p7ObTiles( int tile, std::string & why )
{
	Post7 & P = post7();
	if ( P.obTileSize == tile && P.obTile[0].img )
		return true;
	bloomImgDrop( P.obTile[0], false );
	bloomImgDrop( P.obTile[1], false );
	const VkImageUsageFlags rw = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT
		| VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	P.otw = ( P.W + tile - 1 ) / tile;
	P.oth = ( P.H + tile - 1 ) / tile;
	if ( !bloomLocal( P.obTile[0], VK_FORMAT_R16G16_SFLOAT, rw, P.otw, P.oth, 1, P.mb )
		|| !bloomLocal( P.obTile[1], VK_FORMAT_R16G16_SFLOAT, rw, P.otw, P.oth, 1, P.mb ) ) {
		why = "the object blur's tiles failed";
		return false;
	}
	P.obTileSize = tile;
	P.fresh = true;
	return true;
}
}  // namespace

namespace
{
void p7ToneReset()
{
	post7().queued = false;
}

// inside the tone pass, after its modes: the pass ends, the post passes run, the pass begins again with the copy back
bool p7Record( VkCommandBuffer cmd, const VkRenderPassBeginInfo & rbi )
{
	Post7 & P = post7();
	if ( !P.queued || !P.copy )
		return false;
	Vk & v = vk();
	View & V = view();
	v.vkCmdEndRenderPass( cmd );
	VkMemoryBarrier mb { VK_STRUCTURE_TYPE_MEMORY_BARRIER };
	mb.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_SHADER_WRITE_BIT;
	mb.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
	const VkPipelineStageFlags before = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT
		| VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
	const VkPipelineStageFlags work = VK_PIPELINE_STAGE_TRANSFER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
	VkImageMemoryBarrier ib[16] = {};
	std::uint32_t nb = 0;
	if ( P.fresh ) {  // the images made since the last record: UNDEFINED -> GENERAL (every texel is written before it is read)
		auto lay = [&]( VkImage img, VkImageAspectFlags aspect ) {
			if ( !img )
				return;
			VkImageMemoryBarrier & b = ib[nb++];
			b.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			b.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			b.newLayout = VK_IMAGE_LAYOUT_GENERAL;
			b.dstAccessMask = mb.dstAccessMask;
			b.srcQueueFamilyIndex = b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			b.image = img;
			b.subresourceRange = { aspect, 0, 1, 0, 1 };
		};
		lay( P.msDep, VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT );
		for ( BloomImg * I : { &P.msCol, &P.cur, &P.depth, &P.blur[0], &P.blur[1], &P.mv, &P.out, &P.lensFar, &P.lensNear, &P.lensTile[0],
			&P.lensTile[1], &P.obVec, &P.obTile[0], &P.obTile[1], &P.fill } )
			lay( I->img, VK_IMAGE_ASPECT_COLOR_BIT );
		P.fresh = false;
	}
	v.vkCmdPipelineBarrier( cmd, before, work, 0, 1, &mb, 0, nullptr, nb, ib );
	// the view's samples as they stand (GL's blits read the same)
	VkImageCopy cp {};
	cp.srcSubresource = cp.dstSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
	cp.extent = { std::uint32_t( P.W ), std::uint32_t( P.H ), 1 };
	v.vkCmdCopyImage( cmd, V.slot[P.slot].color, VK_IMAGE_LAYOUT_GENERAL, P.msCol.img, VK_IMAGE_LAYOUT_GENERAL, 1, &cp );
	cp.srcSubresource = cp.dstSubresource = { VK_IMAGE_ASPECT_DEPTH_BIT, 0, 0, 1 };
	v.vkCmdCopyImage( cmd, V.slot[P.slot].ds, VK_IMAGE_LAYOUT_GENERAL, P.msDep, VK_IMAGE_LAYOUT_GENERAL, 1, &cp );
	VkMemoryBarrier gb { VK_STRUCTURE_TYPE_MEMORY_BARRIER };
	gb.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_SHADER_WRITE_BIT;
	gb.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
	v.vkCmdPipelineBarrier( cmd, work, work, 0, 1, &gb, 0, nullptr, 0, nullptr );
	for ( int k = 0; k < P.nOps; k++ ) {
		const P7Op & o = P.ops[k];
		if ( o.kind == 0 ) {
			v.vkCmdBindPipeline( cmd, VK_PIPELINE_BIND_POINT_COMPUTE, P.pipe[o.pipe] );
			v.vkCmdBindDescriptorSets( cmd, VK_PIPELINE_BIND_POINT_COMPUTE, P.layout, 0, 1, &P.set[o.set], 0, nullptr );
			v.vkCmdPushConstants( cmd, P.layout, VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
				std::uint32_t( sizeof( o.job ) ), &o.job );
			v.vkCmdDispatch( cmd, std::uint32_t( ( o.w + 7 ) / 8 ), std::uint32_t( ( o.h + 7 ) / 8 ), 1 );
		} else if ( o.kind == 1 ) {
			VkImageCopy c {};
			c.srcSubresource = c.dstSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
			c.extent = { std::uint32_t( o.w ), std::uint32_t( o.h ), 1 };
			v.vkCmdCopyImage( cmd, o.a, VK_IMAGE_LAYOUT_GENERAL, o.b, VK_IMAGE_LAYOUT_GENERAL, 1, &c );
		} else {
			const VkClearColorValue zero {};
			const VkImageSubresourceRange rr { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
			v.vkCmdClearColorImage( cmd, o.a, VK_IMAGE_LAYOUT_GENERAL, &zero, 1, &rr );
			if ( o.b )
				v.vkCmdClearColorImage( cmd, o.b, VK_IMAGE_LAYOUT_GENERAL, &zero, 1, &rr );
		}
		v.vkCmdPipelineBarrier( cmd, work, work, 0, 1, &gb, 0, nullptr, 0, nullptr );
	}
	gb.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
		| VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	v.vkCmdPipelineBarrier( cmd, work, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
		| VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, 0, 1, &gb, 0, nullptr, 0, nullptr );
	v.vkCmdBeginRenderPass( cmd, &rbi, VK_SUBPASS_CONTENTS_INLINE );
	const VkViewport vp { float( P.vp[0] ), float( P.vp[1] ), float( P.vp[2] ), float( P.vp[3] ), 0.0f, 1.0f };
	const int sx = std::max( P.vp[0], 0 ), sy = std::max( P.vp[1], 0 );
	const VkRect2D sc { { sx, sy }, { std::uint32_t( std::max( P.vp[0] + P.vp[2] - sx, 0 ) ), std::uint32_t( std::max( P.vp[1] + P.vp[3] - sy, 0 ) ) } };
	v.vkCmdSetViewport( cmd, 0, 1, &vp );
	v.vkCmdSetScissor( cmd, 0, 1, &sc );
	v.vkCmdBindPipeline( cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, P.copy );
	v.vkCmdBindDescriptorSets( cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, P.layout, 0, 1, &P.set[P.copySet], 0, nullptr );
	const WwVkPresent::PfxJob none;
	v.vkCmdPushConstants( cmd, P.layout, VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, std::uint32_t( sizeof( none ) ), &none );
	v.vkCmdDraw( cmd, 3, 1, 0, 0 );
	return true;
}
}  // namespace

namespace WwVkPresent
{
bool tonePostQueue( const PostRun & p, std::string & why )
{
	Post7 & P = post7();
	Tone & T = tone();
	View & V = view();
	Vk & v = vk();
	Gl & g = gl();
	FxStat & S = P.st;
	const double t0 = nowMs();
	auto refuse = [&]( const std::string & w ) {
		why = w;
		S.fails++;
		S.failWhy = w;
		return false;
	};
	auto breakOff = [&]( const std::string & w ) {
		P.broken = true;
		P.brokenWhy = "the post passes on Vulkan broke: " + w;
		return refuse( P.brokenWhy );
	};
	if ( P.broken )
		return refuse( P.brokenWhy );
	if ( !T.deferred || T.pending )
		return refuse( "no deferred tone pass to record the post passes into" );
	if ( P.queued )
		return refuse( "the post passes were already queued into this tone pass" );
	if ( !p.dof && !p.lens && !p.mb && !p.ob )
		return refuse( "no post pass is on" );
	if ( !v.ok || !g.ok || g.ctx != reinterpret_cast<void *>( wglGetCurrentContext() ) )
		return refuse( "the present device or GL's entry points are not ready in this context" );
	if ( !v.storageExt )
		return refuse( "the device stores no RG16F / R16F (shaderStorageImageExtendedFormats)" );
	if ( T.slot < 0 || T.slot >= kViewSlots || !V.slot[T.slot].color || !V.slot[T.slot].ds )
		return refuse( "no view slot under the tone pass" );
	if ( V.samples < 2 )
		return refuse( "a view of " + std::to_string( V.samples ) + " sample (the resolve reads a multisampled copy)" );
	if ( p.w != V.w || p.h != V.h || p.w < 2 || p.h < 2 )
		return refuse( "frame " + std::to_string( p.w ) + " x " + std::to_string( p.h ) + ", view " + std::to_string( V.w ) + " x "
			+ std::to_string( V.h ) );
	if ( p.vp[2] < 1 || p.vp[3] < 1 )
		return refuse( "an empty viewport" );
	const bool game = p.dof && !p.lens;
	if ( game && ( p.radius < 1 || p.radius > 7 || p.row < 0 || p.row > 6 ) )
		return refuse( "blur radius " + std::to_string( p.radius ) + " row " + std::to_string( p.row ) + " (1..7, 0..6)" );
	if ( p.ob && ( !p.obVel || !p.obMask || !g.copyImageSubData || !p.gp ) )
		return refuse( "the object blur without its velocity, mask, glCopyImageSubData or GL's entry points" );
	if ( p.ob && ( p.tile < 1 || p.samples < 1 ) )
		return refuse( "object blur tile " + std::to_string( p.tile ) + " samples " + std::to_string( p.samples ) );
	std::string w;
	if ( !p7Static( w ) || !p7CopyPipe( w ) || !p7Images( p.w, p.h, V.samples, w ) || ( p.ob && !p7ObTiles( p.tile, w ) ) )
		return breakOff( w );
	// the object blur's inputs: GL's velocity and mask copied into shared twins (the crossing's names)
	const Shared * sVel = nullptr, * sMask = nullptr;
	GLuint twin[2] = {};
	if ( p.ob ) {
		int newNames = 0;
		using LevelParam = void( WINAPI * )( GLuint, GLint, GLenum, GLint * );
		const auto lp = reinterpret_cast<LevelParam>( p.gp( "glGetTextureLevelParameteriv" ) );
		if ( !lp )
			return refuse( "no glGetTextureLevelParameteriv (the object vectors' formats)" );
		const unsigned src[2] = { p.obVel, p.obMask };
		GLint fmt[2] = {};
		for ( int k = 0; k < 2; k++ ) {
			GLint tw = 0, th = 0;
			lp( GLuint( src[k] ), 0, 0x1003, &fmt[k] );  // GL_TEXTURE_INTERNAL_FORMAT
			lp( GLuint( src[k] ), 0, 0x1000, &tw );      // GL_TEXTURE_WIDTH
			lp( GLuint( src[k] ), 0, 0x1001, &th );      // GL_TEXTURE_HEIGHT
			if ( !fmt[k] || tw != p.w || th != p.h )
				return refuse( std::string( k ? "the mask" : "the velocity" ) + " is " + std::to_string( tw ) + " x " + std::to_string( th )
					+ " format " + std::to_string( fmt[k] ) + " (the frame's size wanted)" );
		}
		for ( int k = 0; k < 2; k++ ) {
			if ( !sharedTarget( p7VelKey() + std::uintptr_t( k ), 0, p.w, p.h, 1, unsigned( fmt[k] ), twin[k], w ) )
				return refuse( std::string( k ? "the mask's" : "the velocity's" ) + " shared twin: " + w );
			newNames += std::find( T.defTex, T.defTex + T.defN, twin[k] ) == T.defTex + T.defN ? 1 : 0;
		}
		if ( T.defN + GLuint( newNames ) > GLuint( kToneDefMax ) )
			return refuse( "the crossing's texture list is full" );
		sVel = sharedFind( p7VelKey() );
		sMask = sharedFind( p7VelKey() + 1 );
		if ( !sVel || !sVel->view || !sMask || !sMask->view )
			return refuse( "the object vectors' twins have no view" );
		for ( int k = 0; k < 2; k++ )
			g.copyImageSubData( GLuint( src[k] ), kTexture2D, 0, 0, 0, 0, twin[k], kTexture2D, 0, 0, 0, 0, p.w, p.h, 1 );
		S.copies += 2;
	}

	// the ops and their sets (no tone pass is in flight: toneRun waited its fence)
	const int W = p.w, H = p.h;
	const bool fuse = p.fuse && p.mb && ( p.dof || p.lens ) && postRedTest != 6;
	const bool fuseRed = p.fuse && p.mb && ( p.dof || p.lens ) && postRedTest == 6;
	VkDescriptorImageInfo ii[kP7Sets][9] = {};
	VkWriteDescriptorSet ws[kP7Sets * 9] = {};
	int nw = 0, nSets = 0, nOps = 0;
	auto setOf = [&]( std::initializer_list<VkImageView> t, VkImageView o0, VkImageView o1 ) {
		const int s = nSets++;
		int k = 0;
		for ( VkImageView x : t )
			ii[s][k++].imageView = x;
		for ( ; k < 5; k++ )
			ii[s][k].imageView = P.fill.view;
		ii[s][5].sampler = P.linear;
		ii[s][6].sampler = P.point;
		ii[s][7].imageView = o0 ? o0 : P.fill.view;
		ii[s][8].imageView = o1 ? o1 : P.fill.view;
		for ( std::uint32_t b = 0; b < 9; b++ ) {
			if ( b != 5 && b != 6 )
				ii[s][b].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
			VkWriteDescriptorSet & x = ws[nw++];
			x.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			x.dstSet = P.set[s];
			x.dstBinding = b;
			x.descriptorCount = 1;
			x.descriptorType = b < 5 ? VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE : b < 7 ? VK_DESCRIPTOR_TYPE_SAMPLER : VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
			x.pImageInfo = &ii[s][b];
		}
		return s;
	};
	auto run = [&]( int pipe, std::initializer_list<VkImageView> t, const BloomImg & o0, const BloomImg * o1, const PfxJob & j ) {
		P7Op & o = P.ops[nOps++];
		o = P7Op();
		o.kind = 0;
		o.pipe = pipe;
		o.set = setOf( t, o0.view, o1 ? o1->view : VK_NULL_HANDLE );
		o.w = o0.w;
		o.h = o0.h;
		o.job = j;
	};
	auto copyImg = [&]( const BloomImg & a, const BloomImg & b ) {
		P7Op & o = P.ops[nOps++];
		o = P7Op();
		o.kind = 1;
		o.a = a.img;
		o.b = b.img;
		o.w = a.w;
		o.h = a.h;
	};
	auto clear = [&]( const BloomImg & a, const BloomImg * b ) {
		P7Op & o = P.ops[nOps++];
		o = P7Op();
		o.kind = 2;
		o.a = a.img;
		o.b = b ? b->img : VK_NULL_HANDLE;
	};
	auto put4 = []( PfxJob & j, int at, const float * f ) {
		for ( int r = 0; r < 4; r++ )
			j.c[at][r] = f[r];
	};
	// the sources: GL's two blits (the colour's mean, the depth's sample 0)
	{
		PfxJob j;
		j.i[0] = postRedTest == 1 ? 1 : 0;
		run( kP7Res, { P.msCol.view }, P.cur, nullptr, j );
		PfxJob d;
		d.i[1] = 0;
		run( kP7Dep, { P.msDepView }, P.depth, nullptr, d );
	}
	if ( p.lens ) {  // the thin lens: prep, the near reach, the gather, the composite (inside the motion blur when fused)
		PfxJob j;
		put4( j, 0, p.lz );
		put4( j, 1, p.lensP );
		put4( j, 2, p.lensQ );
		run( kP7LensPrep, { P.cur.view, P.depth.view }, P.blur[0], nullptr, j );
		const bool nearTile = p.lensQ[1] != 0.0f && p.lensQ[3] > 0.5f;
		if ( nearTile ) {
			PfxJob t;
			t.i[0] = 0;
			run( kP7LensTile, { P.blur[0].view }, P.lensTile[0], nullptr, t );
			t.i[0] = 1;
			run( kP7LensTile, { P.lensTile[0].view }, P.lensTile[1], nullptr, t );
		}
		if ( postRedTest == 3 ) {
			clear( P.lensFar, &P.lensNear );
		} else {
			PfxJob q;
			put4( q, 2, p.lensQ );
			run( kP7Gather, { P.blur[0].view, nearTile ? P.lensTile[1].view : P.blur[0].view }, P.lensFar, &P.lensNear, q );
		}
		if ( !fuse && !fuseRed ) {
			run( kP7LensComp, { P.cur.view, P.lensFar.view, P.depth.view, P.lensNear.view }, P.out, nullptr, j );
			copyImg( P.out, P.cur );
		}
	} else if ( p.dof ) {  // the game's: the downsample, the blur down the columns then along the rows, the composite
		PfxJob j;
		const float c4[4] = { 1.0f, 1.0f, 2.0f / float( W ), 2.0f / float( H ) };
		const float c7[4] = { 1.0f / float( W ), 1.0f / float( H ), 0.0f, 0.0f };
		put4( j, 0, c4 );
		put4( j, 1, c7 );
		const float taps[4][4] = { { -1.0f, -1.0f, 0.25f, 0.0f }, { 1.0f, -1.0f, 0.25f, 0.0f }, { -1.0f, 1.0f, 0.25f, 0.0f },
			{ 1.0f, 1.0f, 0.25f, 0.0f } };
		for ( int k = 0; k < 4; k++ )
			put4( j, 2 + k, taps[k] );
		run( kP7Down, { P.cur.view }, P.blur[0], nullptr, j );
		if ( postRedTest != 2 ) {
			const int taps = 2 * p.radius + 1;
			for ( int pass = 0; pass < 2; pass++ ) {  // 0: the columns (y), blur0 -> blur1; 1: the rows (x), blur1 -> blur0
				const int size = pass == 0 ? P.qh[0] : P.qw[1];
				PfxJob b;
				for ( int t = 0; t < taps; t++ ) {
					const int k = 7 - p.radius + t;
					b.c[t >> 2][t & 3] = float( k - 7 ) / float( size );
				}
				b.i[0] = taps;
				b.i[1] = p.row;
				b.i[2] = 7 - p.radius;
				b.i[3] = pass;
				run( kP7Blur, { P.blur[pass].view }, P.blur[1 - pass], nullptr, b );
			}
		}
		if ( !fuse && !fuseRed ) {
			PfxJob c;
			for ( int k = 0; k < 4; k++ )
				put4( c, k, p.dc[k] );
			c.i[0] = p.farOnly;
			c.i[1] = p.dofRed;
			run( kP7Comp, { P.cur.view, P.blur[0].view, P.depth.view }, P.out, nullptr, c );
			copyImg( P.out, P.cur );
		}
	}
	if ( p.mb ) {  // the camera's vectors, then the blur (the depth of field's composite inside it when fused)
		if ( p.noVelocity || postRedTest == 4 ) {
			clear( P.mv, nullptr );
		} else {
			PfxJob j;
			for ( int k = 0; k < 4; k++ )
				put4( j, k, p.reproj + k * 4 );
			const float c4[4] = { p.jitter[0], p.jitter[1], 1.0f / float( W ), 1.0f / float( H ) };
			put4( j, 4, c4 );
			run( kP7Mv, { P.depth.view }, P.mv, nullptr, j );
		}
		PfxJob m;
		put4( m, 4, p.mc[0] );
		put4( m, 5, p.mc[1] );
		if ( fuse && p.lens ) {
			put4( m, 0, p.lz );
			put4( m, 1, p.lensP );
			put4( m, 2, p.lensQ );
			run( kP7LensMblur, { P.cur.view, P.mv.view, P.lensFar.view, P.depth.view, P.lensNear.view }, P.out, nullptr, m );
		} else if ( fuse ) {
			for ( int k = 0; k < 4; k++ )
				put4( m, k, p.dc[k] );
			m.i[0] = p.farOnly;
			m.i[1] = p.dofRed;
			run( kP7DofMblur, { P.cur.view, P.mv.view, P.blur[0].view, P.depth.view }, P.out, nullptr, m );
		} else {
			run( kP7Mblur, { P.cur.view, P.mv.view }, P.out, nullptr, m );
		}
	}
	const BloomImg * picture = p.mb ? &P.out : &P.cur;
	if ( p.ob ) {  // the object blur on the picture so far, into the other target
		const BloomImg & dst = p.mb ? P.cur : P.out;
		PfxJob j;
		for ( int k = 0; k < 4; k++ )
			put4( j, k, p.obReproj + k * 4 );
		const float c4[4] = { p.obJitter[0], p.obJitter[1], 1.0f / float( W ), 1.0f / float( H ) };
		const float c5[4] = { p.shutter, p.maxPx, 0.0f, 0.0f };
		put4( j, 4, c4 );
		put4( j, 5, c5 );
		j.i[0] = p.objRed;
		run( kP7ObVec, { sVel->view, sMask->view, P.depth.view }, P.obVec, nullptr, j );
		if ( postRedTest == 5 ) {
			clear( P.obTile[0], &P.obTile[1] );
		} else {
			PfxJob t;
			t.i[0] = p.tile;
			t.i[1] = 0;
			run( kP7ObTile, { P.obVec.view }, P.obTile[0], nullptr, t );
			t.i[1] = 1;
			run( kP7ObTile, { P.obTile[0].view }, P.obTile[1], nullptr, t );
		}
		PfxJob b;
		const float c0[4] = { p.zNear, p.zFar, p.seed, 0.0f };
		put4( b, 0, c0 );
		b.i[0] = p.tile;
		b.i[1] = p.samples;
		run( kP7Ob, { picture->view, P.obVec.view, P.obTile[1].view, P.depth.view }, dst, nullptr, b );
		picture = &dst;
	}
	P.copySet = setOf( { picture->view }, VK_NULL_HANDLE, VK_NULL_HANDLE );
	v.vkUpdateDescriptorSets( v.dev, std::uint32_t( nw ), ws, 0, nullptr );
	for ( int k = 0; k < 2; k++ )
		if ( twin[k] )
			flDefAdd( twin[k] );
	P.nOps = nOps;
	P.slot = T.slot;
	std::memcpy( P.vp, p.vp, sizeof( P.vp ) );
	P.queued = true;
	const double ms = nowMs() - t0;
	S.runs++;
	S.cpuMs += ms;
	S.maxMs = std::max( S.maxMs, ms );
	return true;
}

void tonePostOnGl( const char * why )
{
	FxStat & S = post7().st;
	S.gl++;
	S.glWhy = why ? why : "";
}

FxStat takeTonePostStat()
{
	Post7 & P = post7();
	FxStat s = P.st;
	s.pipelines = P.pipelines;
	s.buildMs = P.buildMs;
	if ( P.broken && s.failWhy.empty() )
		s.failWhy = P.brokenWhy;
	P.st = FxStat();
	return s;
}
}  // namespace WwVkPresent
