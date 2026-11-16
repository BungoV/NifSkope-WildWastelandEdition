/* Lane VKOBJ18 (2026-11-16): the TAA inputs' step 2 (game_taa_mv.prog: cell_ao.vert's full-screen quad + game_taa_mv.frag, the
 * camera's vectors) on the present device (vkcamvel.h). Included once by vkpresent.cpp at its end. GL's pass (taainputs.cpp
 * step 2): camFbo = g.vel (RG16F) alone, no depth test / blend / cull, the frame's depth read by texelFetch. Here: an own render
 * pass over the shared vel (LOAD / STORE, GENERAL), g.depth's depth-aspect view sampled at binding 64, one pipeline, the b31
 * block packed by name (camvel_tables.h), the quad in a host buffer, one crossing (GL waits it). Every part sits in CvState
 * (its own command buffer, fence, semaphores, framebuffer, set and stat; nothing shared with objvel), behind its own mutex. */
#define WW_VK_CAMVEL 1
#include <mutex>
#include "features/vkframe/vkcamvel.h"
#include "features/vkframe/camvel_tables.h"
#include "features/vkframe/camvel_vs.spv.h"  // vkframe_spirv.sh's fxe camvel_vs line
#include "features/vkframe/camvel_ps.spv.h"  // its fxo camvel_ps line

namespace
{
namespace camvel
{
using namespace WwVkPresent;

constexpr VkDeviceSize kCvGeo = 64, kCvIdxAt = 48;  // the quad: 4 x vec3 at 0, 6 x u16 at 48

struct CvState
{
	std::mutex mx;  // camVelRun / camVelOnGl / takeCamVelStat from any thread
	bool made = false, broken = false;
	std::string brokenWhy;
	VkShaderModule vs = VK_NULL_HANDLE, ps = VK_NULL_HANDLE;
	VkDescriptorSetLayout dsl = VK_NULL_HANDLE;
	VkPipelineLayout layout = VK_NULL_HANDLE;
	VkSampler plain = VK_NULL_HANDLE;
	PFN_vkCmdDrawIndexed drawIndexed = nullptr;
	VkRenderPass pass = VK_NULL_HANDLE;
	VkPipeline pipe = VK_NULL_HANDLE;
	VkDescriptorPool pool = VK_NULL_HANDLE;
	VkDescriptorSet set = VK_NULL_HANDLE;
	VkSemaphore glDone = VK_NULL_HANDLE, vkDone = VK_NULL_HANDLE;
	GLuint glDoneGl = 0, vkDoneGl = 0;
	void * semCtx = nullptr;
	VkCommandBuffer cmd = VK_NULL_HANDLE;
	VkFence fence = VK_NULL_HANDLE;
	bool pending = false;
	CsmBuf geo, ubo;
	VkFramebuffer fb = VK_NULL_HANDLE;
	VkImageView fbView = VK_NULL_HANDLE;
	int fbW = 0, fbH = 0;
	std::uint64_t fbGen = 0;
	CamVelStat st;
};

CvState & cvState()
{
	static CvState s;
	return s;
}

// the device-time parts (once): modules, layouts, sampler, render pass, pipeline, pool + set, command buffer, fence
bool cvStatic( std::string & why )
{
	Vk & v = vk();
	CvState & S = cvState();
	if ( S.made )
		return true;
	const double t0 = nowMs();
	if ( !fxModule( vkframe_camvel_vs_spv, sizeof( vkframe_camvel_vs_spv ), S.vs )
		|| !fxModule( vkframe_camvel_ps_spv, sizeof( vkframe_camvel_ps_spv ), S.ps ) ) {
		why = "the camvel twins' modules failed";
		return false;
	}
	// camvel_ps (reflected): b31 at 31, depthTex at 64, its sampler at 128
	const VkDescriptorSetLayoutBinding lb[3] = { { 31, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr },
		{ 64, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr },
		{ 128, VK_DESCRIPTOR_TYPE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr } };
	VkDescriptorSetLayoutCreateInfo lci { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
	lci.bindingCount = 3;
	lci.pBindings = lb;
	VkPipelineLayoutCreateInfo pli { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
	pli.setLayoutCount = 1;
	pli.pSetLayouts = &S.dsl;
	if ( v.vkCreateDescriptorSetLayout( v.dev, &lci, nullptr, &S.dsl ) != VK_SUCCESS
		|| v.vkCreatePipelineLayout( v.dev, &pli, nullptr, &S.layout ) != VK_SUCCESS ) {
		why = "the camvel set / pipeline layout failed";
		return false;
	}
	VkSamplerCreateInfo sci { VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
	sci.magFilter = sci.minFilter = VK_FILTER_NEAREST;
	sci.addressModeU = sci.addressModeV = sci.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	sci.maxLod = 0.0f;
	if ( v.vkCreateSampler( v.dev, &sci, nullptr, &S.plain ) != VK_SUCCESS ) {
		why = "the camvel sampler failed";
		return false;
	}
	S.drawIndexed = reinterpret_cast<PFN_vkCmdDrawIndexed>( v.vkGetDeviceProcAddr( v.dev, "vkCmdDrawIndexed" ) );
	if ( !S.drawIndexed ) {
		why = "no vkCmdDrawIndexed on the present device";
		return false;
	}
	// the render pass: att0 RG16F, kept (GL's step 1 cleared it; the quad covers it), GENERAL throughout
	VkAttachmentDescription at {};
	at.format = VK_FORMAT_R16G16_SFLOAT;
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
	const VkPipelineStageFlags stg = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	const VkAccessFlags rw = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT;
	const VkSubpassDependency dep[2] = { { VK_SUBPASS_EXTERNAL, 0, stg, stg, rw, rw, 0 }, { 0, VK_SUBPASS_EXTERNAL, stg, stg, rw, rw, 0 } };
	VkRenderPassCreateInfo rci { VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO };
	rci.attachmentCount = 1;
	rci.pAttachments = &at;
	rci.subpassCount = 1;
	rci.pSubpasses = &sp;
	rci.dependencyCount = 2;
	rci.pDependencies = dep;
	if ( v.vkCreateRenderPass( v.dev, &rci, nullptr, &S.pass ) != VK_SUCCESS ) {
		S.pass = VK_NULL_HANDLE;
		why = "vkCreateRenderPass (camvel) failed";
		return false;
	}
	// the pipeline: GL's state in step 2 (no depth test, no blend, no cull, every channel written)
	VkPipelineShaderStageCreateInfo stv[2] { { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO },
		{ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO } };
	stv[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
	stv[0].module = S.vs;
	stv[0].pName = "main";
	stv[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	stv[1].module = S.ps;
	stv[1].pName = "main";
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
	gp.pStages = stv;
	gp.pVertexInputState = &vi;
	gp.pInputAssemblyState = &ia;
	gp.pViewportState = &vps;
	gp.pRasterizationState = &rs;
	gp.pMultisampleState = &ms;
	gp.pDepthStencilState = &ds;
	gp.pColorBlendState = &cbs;
	gp.pDynamicState = &dy;
	gp.layout = S.layout;
	gp.renderPass = S.pass;
	if ( v.vkCreateGraphicsPipelines( v.dev, tone().cache, 1, &gp, nullptr, &S.pipe ) != VK_SUCCESS ) {
		S.pipe = VK_NULL_HANDLE;
		why = "vkCreateGraphicsPipelines (camvel) failed";
		return false;
	}
	S.st.pipelines++;
	S.st.buildMs += nowMs() - t0;
	toneCacheSave();
	const VkDescriptorPoolSize ps[3] = { { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1 }, { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1 },
		{ VK_DESCRIPTOR_TYPE_SAMPLER, 1 } };
	VkDescriptorPoolCreateInfo pci { VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
	pci.maxSets = 1;
	pci.poolSizeCount = 3;
	pci.pPoolSizes = ps;
	VkDescriptorSetAllocateInfo ai { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
	ai.descriptorSetCount = 1;
	ai.pSetLayouts = &S.dsl;
	if ( v.vkCreateDescriptorPool( v.dev, &pci, nullptr, &S.pool ) != VK_SUCCESS ) {
		why = "the camvel descriptor pool failed";
		return false;
	}
	ai.descriptorPool = S.pool;
	if ( v.vkAllocateDescriptorSets( v.dev, &ai, &S.set ) != VK_SUCCESS ) {
		why = "the camvel descriptor set failed";
		return false;
	}
	VkFenceCreateInfo fi { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
	VkCommandBufferAllocateInfo cai { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
	cai.commandPool = v.pool;
	cai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	cai.commandBufferCount = 1;
	if ( v.vkCreateFence( v.dev, &fi, nullptr, &S.fence ) != VK_SUCCESS || v.vkAllocateCommandBuffers( v.dev, &cai, &S.cmd ) != VK_SUCCESS ) {
		why = "the camvel fence / command buffer failed";
		return false;
	}
	S.made = true;
	return true;
}

bool cvSems( std::string & why )
{
	Vk & v = vk();
	Gl & g = gl();
	CvState & S = cvState();
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

// the framebuffer over the shared vel (kept until a shared target is made or goes, or the size changes)
bool cvFb( const Shared * vel, std::string & why )
{
	Vk & v = vk();
	CvState & S = cvState();
	if ( S.fb && S.fbGen == sharedGen() && S.fbView == vel->view && S.fbW == vel->w && S.fbH == vel->h )
		return true;
	if ( S.fb )
		v.vkDestroyFramebuffer( v.dev, S.fb, nullptr );
	S.fb = VK_NULL_HANDLE;
	VkFramebufferCreateInfo fci { VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
	fci.renderPass = S.pass;
	fci.attachmentCount = 1;
	fci.pAttachments = &vel->view;
	fci.width = std::uint32_t( vel->w );
	fci.height = std::uint32_t( vel->h );
	fci.layers = 1;
	if ( v.vkCreateFramebuffer( v.dev, &fci, nullptr, &S.fb ) != VK_SUCCESS ) {
		why = "the camvel framebuffer failed";
		return false;
	}
	S.fbView = vel->view;
	S.fbW = vel->w;
	S.fbH = vel->h;
	S.fbGen = sharedGen();
	return true;
}

// the worker: one render pass, the quad, GL's signal waited, signalled back
bool cvWork( std::string & why )
{
	Vk & v = vk();
	CvState & S = cvState();
	v.vkResetFences( v.dev, 1, &S.fence );
	v.vkResetCommandBuffer( S.cmd, 0 );
	VkCommandBufferBeginInfo bi { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
	bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	v.vkBeginCommandBuffer( S.cmd, &bi );
	VkRenderPassBeginInfo rbi { VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
	rbi.renderPass = S.pass;
	rbi.framebuffer = S.fb;
	rbi.renderArea = { { 0, 0 }, { std::uint32_t( S.fbW ), std::uint32_t( S.fbH ) } };
	v.vkCmdBeginRenderPass( S.cmd, &rbi, VK_SUBPASS_CONTENTS_INLINE );
	// no y flip: GL's rows from the bottom are the image's rows from the top (gl_FragCoord and SV_Position agree per row)
	const VkViewport vp { 0.0f, 0.0f, float( S.fbW ), float( S.fbH ), 0.0f, 1.0f };
	const VkRect2D sc { { 0, 0 }, { std::uint32_t( S.fbW ), std::uint32_t( S.fbH ) } };
	v.vkCmdBindPipeline( S.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, S.pipe );
	v.vkCmdSetViewport( S.cmd, 0, 1, &vp );
	v.vkCmdSetScissor( S.cmd, 0, 1, &sc );
	const VkDeviceSize zero = 0;
	v.vkCmdBindVertexBuffers( S.cmd, 0, 1, &S.geo.buf, &zero );
	v.vkCmdBindIndexBuffer( S.cmd, S.geo.buf, kCvIdxAt, VK_INDEX_TYPE_UINT16 );
	v.vkCmdBindDescriptorSets( S.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, S.layout, 0, 1, &S.set, 0, nullptr );
	S.drawIndexed( S.cmd, 6, 1, 0, 0, 0 );
	v.vkCmdEndRenderPass( S.cmd );
	if ( v.vkEndCommandBuffer( S.cmd ) != VK_SUCCESS ) {
		why = "vkEndCommandBuffer (camvel) failed";
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
		why = "vkQueueSubmit (camvel) returned " + std::to_string( int( r ) );
		return false;
	}
	return true;
}

// a field by name into the b31 block (a matrix as GL's column c at + 16 c)
bool cvPut( std::uint8_t * at, const char * name, const float * src, int shift, std::string & why )
{
	for ( const FxField & f : kT_camvelPs )
		if ( std::strcmp( f.name, name ) == 0 ) {
			if ( f.cols > 0 )
				for ( int c = 0; c < f.cols; c++ )
					std::memcpy( at + f.offset + shift + 16 * c, src + f.words * c, std::size_t( f.words ) * 4 );
			else
				std::memcpy( at + f.offset + shift, src, std::size_t( f.words ) * 4 );
			return true;
		}
	why = std::string( "the camvel table has no " ) + name;
	return false;
}
}  // namespace camvel

// lane VKOBJ18: on a present shutdown (the shared targets go): the framebuffer and the host buffers
void camVelDrop()
{
	Vk & v = vk();
	camvel::CvState & S = camvel::cvState();
	std::lock_guard<std::mutex> lk( S.mx );
	if ( !S.fb && !S.geo.buf && !S.ubo.buf )
		return;
	v.vkDeviceWaitIdle( v.dev );
	if ( S.fb )
		v.vkDestroyFramebuffer( v.dev, S.fb, nullptr );
	S.fb = VK_NULL_HANDLE;
	S.fbView = VK_NULL_HANDLE;
	S.pending = false;
	csmBufFree( S.geo, false );
	csmBufFree( S.ubo, false );
}
}  // namespace

namespace WwVkPresent
{
// the refusal with S.mx already held (camVelRun holds it)
static std::string cvRefusalHeld( camvel::CvState & S )
{
	if ( S.broken )
		return S.brokenWhy;
	if ( !vk().ok || !gl().ok || gl().ctx != reinterpret_cast<void *>( wglGetCurrentContext() ) )
		return "the present device or GL's entry points are not ready in this context";
	return std::string();
}

std::string camVelRefusal()
{
	camvel::CvState & S = camvel::cvState();
	std::lock_guard<std::mutex> lk( S.mx );
	return cvRefusalHeld( S );
}

bool camVelRun( const CamVelRun & r, std::string & why )
{
	using namespace camvel;
	CvState & S = cvState();
	std::lock_guard<std::mutex> lk( S.mx );
	Gl & g = gl();
	Vk & v = vk();
	const double t0 = nowMs();
	auto refuse = [&]( const std::string & w ) {
		why = w;
		S.st.fails++;
		S.st.failWhy = w;
		return false;
	};
	auto breakOff = [&]( const std::string & w ) {
		S.broken = true;
		S.brokenWhy = "camvel on Vulkan broke: " + w;
		return refuse( S.brokenWhy );
	};
	if ( const std::string no = cvRefusalHeld( S ); !no.empty() )
		return refuse( no );
	// the targets: the owner's shared g.vel / g.depth at the pass's size
	const Shared * tv = sharedFind( r.velKey );
	const Shared * td = sharedFind( r.depthKey );
	if ( !tv || !tv->tex || tv->fmt != VK_FORMAT_R16G16_SFLOAT || !td || !td->tex || !td->view )
		return refuse( "the TAA inputs' targets are not shared (RG16F, depth)" );
	if ( tv->w != r.w || tv->h != r.h || td->w != r.w || td->h != r.h || tv->samples != 1 || td->samples != 1 )
		return refuse( "the shared targets are not the pass's size" );
	if ( td->fmt != VK_FORMAT_D24_UNORM_S8_UINT && td->fmt != VK_FORMAT_D32_SFLOAT_S8_UINT && td->fmt != VK_FORMAT_X8_D24_UNORM_PACK32
		&& td->fmt != VK_FORMAT_D32_SFLOAT )
		return refuse( "the depth target's format " + std::to_string( int( td->fmt ) ) + " is not a depth format camvel reads" );
	std::string w;
	if ( !fxStatic( w ) || !cvStatic( w ) || !cvSems( w ) )
		return breakOff( w );
	// the last crossing is done before its command buffer, set, block and quad are written again
	if ( std::string fw; !fenceDone( S.fence, S.pending, "camvel", fw ) )
		return refuse( fw );
	S.pending = false;
	if ( !cvFb( tv, w ) || !csmBuf( S.geo, kCvGeo, false, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, w )
		|| !csmBuf( S.ubo, 256, false, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, w ) )
		return breakOff( w );
	// taainputs.cpp's quad and indices, as GL draws them
	static const float quad[12] = { -1, -1, 0, 1, -1, 0, -1, 1, 0, 1, 1, 0 };
	static const std::uint16_t idx[6] = { 0, 1, 2, 2, 1, 3 };
	std::uint8_t * geo = static_cast<std::uint8_t *>( S.geo.ptr );
	std::memcpy( geo, quad, sizeof( quad ) );
	std::memcpy( geo + kCvIdxAt, idx, sizeof( idx ) );
	std::uint8_t * ubo = static_cast<std::uint8_t *>( S.ubo.ptr );
	std::memset( ubo, 0, 256 );
	const float noJit[2] = { 0.0f, 0.0f };
	if ( !cvPut( ubo, "reproj", r.reproj, r.red == 2 ? 16 : 0, w ) || !cvPut( ubo, "jitterNdc", r.red == 1 ? noJit : r.jitterNdc, 0, w )
		|| !cvPut( ubo, "invSize", r.invSize, 0, w ) )
		return breakOff( w );
	// the set: the block, the depth's depth-aspect view (red 3: the zero view), the plain sampler
	const VkDescriptorBufferInfo bi { S.ubo.buf, 0, VkDeviceSize( kT_camvelPsBytes ) };
	const VkDescriptorImageInfo ii[2] = { { VK_NULL_HANDLE, r.red == 3 ? fx().d2 : td->view, VK_IMAGE_LAYOUT_GENERAL },
		{ S.plain, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_UNDEFINED } };
	VkWriteDescriptorSet ws[3] {};
	for ( int k = 0; k < 3; k++ ) {
		ws[k].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		ws[k].dstSet = S.set;
		ws[k].descriptorCount = 1;
	}
	ws[0].dstBinding = 31;
	ws[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	ws[0].pBufferInfo = &bi;
	ws[1].dstBinding = 64;
	ws[1].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
	ws[1].pImageInfo = &ii[0];
	ws[2].dstBinding = 128;
	ws[2].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
	ws[2].pImageInfo = &ii[1];
	v.vkUpdateDescriptorSets( v.dev, 3, ws, 0, nullptr );
	// GL's step 1 (the blitted depth, the cleared vel) signalled -> the worker's job -> GL waits
	GLuint texNames[2] = { std::min( tv->tex, td->tex ), std::max( tv->tex, td->tex ) };
	const GLenum lay[2] = { kLayoutGeneral, kLayoutGeneral };
	g.signalSemaphore( S.glDoneGl, 0, nullptr, 2, texNames, lay );
	g.flush();
	if ( !opWorkerRun( cvWork, w ) )
		return breakOff( w );  // nothing signals vkDone: GL does not wait it; GL draws the quad
	g.waitSemaphore( S.vkDoneGl, 0, nullptr, 2, texNames, lay );
	S.pending = true;
	const double ms = nowMs() - t0;
	S.st.runs++;
	S.st.cpuMs += ms;
	S.st.maxMs = std::max( S.st.maxMs, ms );
	return true;
}

void camVelOnGl( const char * why )
{
	camvel::CvState & S = camvel::cvState();
	std::lock_guard<std::mutex> lk( S.mx );
	S.st.gl++;
	S.st.glWhy = why ? why : "";
}

CamVelStat takeCamVelStat()
{
	camvel::CvState & S = camvel::cvState();
	std::lock_guard<std::mutex> lk( S.mx );
	CamVelStat s = S.st;
	if ( S.broken && s.failWhy.empty() )
		s.failWhy = S.brokenWhy;
	const int pipes = S.st.pipelines;
	const double build = S.st.buildMs;
	S.st = CamVelStat();
	S.st.pipelines = pipes;
	S.st.buildMs = build;
	return s;
}
}  // namespace WwVkPresent
