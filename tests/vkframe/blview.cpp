// Lane VKOBJ2 (2026-11-13, plan row G4 slice 2, the bindless half): GL's bindless maps (GL_ARB_bindless_texture handles in
// the draw row, sampler2D( wwDraws[row].tex[k] ) as fo4_default.frag reads them) against the Vulkan route of the shipped
// table (vkbindless.inc, built in here with vkpresent.cpp and WW_VKPRESENT_NO_QT) and the shipped include
// (ww_bindless_vk.hlsli: descriptor-indexed image + sampler arrays in set 1). Same textures (shared targets GL filled and
// made resident), same rows, one draw per cell on each side.
// Usage: blview <tree root> <spv dir> [bc]   (blview.sh compiles blview_vs / blview_ps / the red-slot ps with DXC into <spv dir>)
// Lane VKOBJ3, mode `bc`: the 12 textures are the texture cache's BC twins (vkobjtex.inc: objTexMake = VKFAR1's farTwin,
// BC1/2/3/4/5/7 and sRGB, random blocks GL uploads with glCompressedTextureSubImage2D; BC4 / BC5 carry gli's swizzle) and
// their handles join through objTexHandle. Frame 1: texture 3 is gone (objTexGone, deleted) and a new twin takes its slot
// (the deferred free, the graves blacked). Red `key` (objTexHandle red 1: each handle with the twin made before) must FAIL.
// Fixture: 12 textures (RGBA8 / RGBA16F / RGBA32F, 8..128 px, mips or not) with 12 GL sampler states (filters, wraps,
// border, LOD bias / clamps, anisotropy 4); 128 rows of 8 handles; 128 cells of 24 x 24 px (384 x 192 RGBA32F), cell % 8
// = the slot and the op (Sample, SampleBias 1.25, SampleLevel 1.5, SampleGrad, Sample, SampleLevel 0, SampleBias -0.75,
// Sample), uv scales 1/16..1/2 (magnified and minified). Frame 1: then one handle dropped and added again (a new index).
// Prints `BLVIEW frame<n> EXACT|FAIL`, `BLVIEW RED <name> ...` (each must FAIL) and `BLVIEW GATE OK|FAILED`.
// BAR (stated before the first run, never widened): EXACT (0 pixels differ in bits; covered = either side non-zero; a
// frame with 0 covered pixels FAILS).
#define WW_VKPRESENT_NO_QT
#include "../../src/features/vkframe/vkpresent.cpp"

#include <GL/gl.h>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <random>
#include <sstream>
#include <string>
#include <vector>

namespace blv
{
HMODULE gOpenGl = nullptr;
void * glProc( const char * name )
{
	void * p = reinterpret_cast<void *>( wglGetProcAddress( name ) );
	const intptr_t i = reinterpret_cast<intptr_t>( p );
	if ( !p || i == 1 || i == 2 || i == 3 || i == -1 )
		p = reinterpret_cast<void *>( GetProcAddress( gOpenGl, name ) );
	return p;
}

LRESULT CALLBACK wndProc( HWND hw, UINT m, WPARAM wp, LPARAM lp )
{
	return DefWindowProcA( hw, m, wp, lp );
}

void pump()
{
	MSG msg;
	while ( PeekMessageA( &msg, nullptr, 0, 0, PM_REMOVE ) ) {
		TranslateMessage( &msg );
		DispatchMessageA( &msg );
	}
}

constexpr GLenum kFb = 0x8D40, kColorAtt0 = 0x8CE0, kComplete = 0x8CD5, kFrag = 0x8B30, kVert = 0x8B31;
constexpr GLenum kCompiled = 0x8B81, kLinked = 0x8B82, kSsbo = 0x90D2, kDynamic = 0x88E8;
constexpr GLenum kRgba8i = 0x8058, kRgba16fi = 0x881A, kRgba32fi = 0x8814, kHalf = 0x140B;
constexpr int kW = 384, kH = 192, kCells = 128, kTex = 12;

struct Fns
{
	GLuint( WINAPI * createShader )( GLenum ) = nullptr;
	void( WINAPI * shaderSource )( GLuint, GLsizei, const char * const *, const GLint * ) = nullptr;
	void( WINAPI * compileShader )( GLuint ) = nullptr;
	void( WINAPI * getShaderiv )( GLuint, GLenum, GLint * ) = nullptr;
	void( WINAPI * getShaderInfoLog )( GLuint, GLsizei, GLsizei *, char * ) = nullptr;
	GLuint( WINAPI * createProgram )() = nullptr;
	void( WINAPI * attachShader )( GLuint, GLuint ) = nullptr;
	void( WINAPI * linkProgram )( GLuint ) = nullptr;
	void( WINAPI * getProgramiv )( GLuint, GLenum, GLint * ) = nullptr;
	void( WINAPI * getProgramInfoLog )( GLuint, GLsizei, GLsizei *, char * ) = nullptr;
	void( WINAPI * useProgram )( GLuint ) = nullptr;
	void( WINAPI * genBuffers )( GLsizei, GLuint * ) = nullptr;
	void( WINAPI * bindBuffer )( GLenum, GLuint ) = nullptr;
	void( WINAPI * bufferData )( GLenum, std::ptrdiff_t, const void *, GLenum ) = nullptr;
	void( WINAPI * bindBufferBase )( GLenum, GLuint, GLuint ) = nullptr;
	void( WINAPI * genVertexArrays )( GLsizei, GLuint * ) = nullptr;
	void( WINAPI * bindVertexArray )( GLuint ) = nullptr;
	void( WINAPI * drawArraysInstancedBaseInstance )( GLenum, GLint, GLsizei, GLsizei, GLuint ) = nullptr;
	void( WINAPI * genFramebuffers )( GLsizei, GLuint * ) = nullptr;
	void( WINAPI * bindFramebuffer )( GLenum, GLuint ) = nullptr;
	void( WINAPI * framebufferTexture2D )( GLenum, GLenum, GLenum, GLuint, GLint ) = nullptr;
	GLenum( WINAPI * checkFramebufferStatus )( GLenum ) = nullptr;
	void( WINAPI * textureSubImage2D )( GLuint, GLint, GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, const void * ) = nullptr;
	void( WINAPI * textureParameteri )( GLuint, GLenum, GLint ) = nullptr;
	void( WINAPI * textureParameterf )( GLuint, GLenum, GLfloat ) = nullptr;
	void( WINAPI * textureParameterfv )( GLuint, GLenum, const GLfloat * ) = nullptr;
	void( WINAPI * getTextureImage )( GLuint, GLint, GLenum, GLenum, GLsizei, void * ) = nullptr;
	void( WINAPI * compressedTextureSubImage2D )( GLuint, GLint, GLint, GLint, GLsizei, GLsizei, GLenum, GLsizei, const void * ) = nullptr;
	GLuint64( WINAPI * getTextureHandle )( GLuint ) = nullptr;
	void( WINAPI * makeResident )( GLuint64 ) = nullptr;
	void( WINAPI * makeNonResident )( GLuint64 ) = nullptr;
};
Fns f;

template <class P> bool load( P & p, const char * n )
{
	p = reinterpret_cast<P>( glProc( n ) );
	return p != nullptr;
}

bool loadAll()
{
	return load( f.createShader, "glCreateShader" ) && load( f.shaderSource, "glShaderSource" )
		&& load( f.compileShader, "glCompileShader" ) && load( f.getShaderiv, "glGetShaderiv" )
		&& load( f.getShaderInfoLog, "glGetShaderInfoLog" ) && load( f.createProgram, "glCreateProgram" )
		&& load( f.attachShader, "glAttachShader" ) && load( f.linkProgram, "glLinkProgram" )
		&& load( f.getProgramiv, "glGetProgramiv" ) && load( f.getProgramInfoLog, "glGetProgramInfoLog" )
		&& load( f.useProgram, "glUseProgram" ) && load( f.genBuffers, "glGenBuffers" ) && load( f.bindBuffer, "glBindBuffer" )
		&& load( f.bufferData, "glBufferData" ) && load( f.bindBufferBase, "glBindBufferBase" )
		&& load( f.genVertexArrays, "glGenVertexArrays" ) && load( f.bindVertexArray, "glBindVertexArray" )
		&& load( f.drawArraysInstancedBaseInstance, "glDrawArraysInstancedBaseInstance" )
		&& load( f.genFramebuffers, "glGenFramebuffers" ) && load( f.bindFramebuffer, "glBindFramebuffer" )
		&& load( f.framebufferTexture2D, "glFramebufferTexture2D" ) && load( f.checkFramebufferStatus, "glCheckFramebufferStatus" )
		&& load( f.textureSubImage2D, "glTextureSubImage2D" ) && load( f.textureParameteri, "glTextureParameteri" )
		&& load( f.textureParameterf, "glTextureParameterf" ) && load( f.textureParameterfv, "glTextureParameterfv" )
		&& load( f.getTextureImage, "glGetTextureImage" ) && load( f.getTextureHandle, "glGetTextureHandleARB" )
		&& load( f.compressedTextureSubImage2D, "glCompressedTextureSubImage2D" )
		&& load( f.makeResident, "glMakeTextureHandleResidentARB" ) && load( f.makeNonResident, "glMakeTextureHandleNonResidentARB" );
}

std::string readFile( const std::string & p )
{
	std::ifstream in( p, std::ios::binary );
	std::stringstream ss;
	ss << in.rdbuf();
	return ss.str();
}

GLuint shader( GLenum type, const std::string & src, const char * tag )
{
	const GLuint s = f.createShader( type );
	const char * p = src.c_str();
	f.shaderSource( s, 1, &p, nullptr );
	f.compileShader( s );
	GLint ok = 0;
	f.getShaderiv( s, kCompiled, &ok );
	if ( !ok ) {
		std::vector<char> log( 65536 );
		f.getShaderInfoLog( s, GLsizei( log.size() ), nullptr, log.data() );
		std::printf( "BLVIEW compile failed (%s): %.1500s\n", tag, log.data() );
		return 0;
	}
	return s;
}

GLuint program( const std::string & vs, const std::string & fs )
{
	const GLuint a = shader( kVert, vs, "vert" ), b = shader( kFrag, fs, "frag" );
	if ( !a || !b )
		return 0;
	const GLuint p = f.createProgram();
	f.attachShader( p, a );
	f.attachShader( p, b );
	f.linkProgram( p );
	GLint ok = 0;
	f.getProgramiv( p, kLinked, &ok );
	if ( !ok ) {
		std::vector<char> log( 65536 );
		f.getProgramInfoLog( p, GLsizei( log.size() ), nullptr, log.data() );
		std::printf( "BLVIEW link failed: %.1500s\n", log.data() );
		return 0;
	}
	return p;
}

std::vector<std::uint32_t> readSpv( const std::string & p )
{
	const std::string b = readFile( p );
	std::vector<std::uint32_t> w( b.size() / 4 );
	if ( !w.empty() )
		std::memcpy( w.data(), b.data(), w.size() * 4 );
	return w;
}

// the Vulkan side of the gate: a render pass on the shared RGBA32F target, set 0 the rows (binding 12), set 1 the table's
struct Gv
{
	VkRenderPass pass = VK_NULL_HANDLE;
	VkFramebuffer fb = VK_NULL_HANDLE;
	VkPipelineLayout layout = VK_NULL_HANDLE;
	VkPipeline pipe[2] = {};  // 0 the shipped include, 1 its red slot (every map reads its neighbour's pair)
	VkDescriptorSetLayout dsl0 = VK_NULL_HANDLE;
	VkDescriptorPool pool0 = VK_NULL_HANDLE;
	VkDescriptorSet set0 = VK_NULL_HANDLE, blSet = VK_NULL_HANDLE;
	CsmBuf rows;
	VkCommandBuffer cmd = VK_NULL_HANDLE;
	VkFence fence = VK_NULL_HANDLE;
	VkSemaphore glDone = VK_NULL_HANDLE, vkDone = VK_NULL_HANDLE;
	GLuint glDoneGl = 0, vkDoneGl = 0;
	int pipeIdx = 0;
};
Gv gv;

bool gvWork( std::string & why )
{
	Vk & v = vk();
	v.vkResetFences( v.dev, 1, &gv.fence );
	v.vkResetCommandBuffer( gv.cmd, 0 );
	VkCommandBufferBeginInfo bi { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
	bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	v.vkBeginCommandBuffer( gv.cmd, &bi );
	VkClearValue cv {};
	VkRenderPassBeginInfo rbi { VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
	rbi.renderPass = gv.pass;
	rbi.framebuffer = gv.fb;
	rbi.renderArea = { { 0, 0 }, { std::uint32_t( kW ), std::uint32_t( kH ) } };
	rbi.clearValueCount = 1;
	rbi.pClearValues = &cv;
	v.vkCmdBeginRenderPass( gv.cmd, &rbi, VK_SUBPASS_CONTENTS_INLINE );
	v.vkCmdBindPipeline( gv.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, gv.pipe[gv.pipeIdx] );
	const VkDescriptorSet sets[2] = { gv.set0, gv.blSet };
	v.vkCmdBindDescriptorSets( gv.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, gv.layout, 0, 2, sets, 0, nullptr );
	for ( int c = 0; c < kCells; c++ )
		v.vkCmdDraw( gv.cmd, 6, 1, 0, std::uint32_t( c ) );
	v.vkCmdEndRenderPass( gv.cmd );
	if ( v.vkEndCommandBuffer( gv.cmd ) != VK_SUCCESS ) {
		why = "vkEndCommandBuffer (the gate) failed";
		return false;
	}
	const VkPipelineStageFlags waitAt = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
	VkSubmitInfo si { VK_STRUCTURE_TYPE_SUBMIT_INFO };
	si.waitSemaphoreCount = 1;
	si.pWaitSemaphores = &gv.glDone;
	si.pWaitDstStageMask = &waitAt;
	si.commandBufferCount = 1;
	si.pCommandBuffers = &gv.cmd;
	si.signalSemaphoreCount = 1;
	si.pSignalSemaphores = &gv.vkDone;
	if ( v.vkQueueSubmit( v.queue, 1, &si, gv.fence ) != VK_SUCCESS ) {
		why = "vkQueueSubmit (the gate) failed";
		return false;
	}
	return true;
}

bool gvBuild( std::uintptr_t targetKey, const std::string & spvDir, std::string & why )
{
	Vk & v = vk();
	const Shared * t = sharedFind( targetKey );
	if ( !t ) {
		why = "no shared target";
		return false;
	}
	VkDescriptorSetLayout blDsl = VK_NULL_HANDLE;
	if ( !blSetGet( blDsl, gv.blSet, why ) )
		return false;
	VkAttachmentDescription at {};
	at.format = VK_FORMAT_R32G32B32A32_SFLOAT;
	at.samples = VK_SAMPLE_COUNT_1_BIT;
	at.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
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
	if ( v.vkCreateRenderPass( v.dev, &rci, nullptr, &gv.pass ) != VK_SUCCESS ) {
		why = "render pass";
		return false;
	}
	VkFramebufferCreateInfo fci { VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
	fci.renderPass = gv.pass;
	fci.attachmentCount = 1;
	fci.pAttachments = &t->view;
	fci.width = kW;
	fci.height = kH;
	fci.layers = 1;
	const VkDescriptorSetLayoutBinding b0 { 12, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr };
	VkDescriptorSetLayoutCreateInfo lci { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
	lci.bindingCount = 1;
	lci.pBindings = &b0;
	const VkDescriptorPoolSize ps { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1 };
	VkDescriptorPoolCreateInfo pci { VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
	pci.maxSets = 1;
	pci.poolSizeCount = 1;
	pci.pPoolSizes = &ps;
	if ( v.vkCreateFramebuffer( v.dev, &fci, nullptr, &gv.fb ) != VK_SUCCESS
		|| v.vkCreateDescriptorSetLayout( v.dev, &lci, nullptr, &gv.dsl0 ) != VK_SUCCESS
		|| v.vkCreateDescriptorPool( v.dev, &pci, nullptr, &gv.pool0 ) != VK_SUCCESS ) {
		why = "framebuffer / set 0";
		return false;
	}
	VkDescriptorSetAllocateInfo ai { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
	ai.descriptorPool = gv.pool0;
	ai.descriptorSetCount = 1;
	ai.pSetLayouts = &gv.dsl0;
	if ( v.vkAllocateDescriptorSets( v.dev, &ai, &gv.set0 ) != VK_SUCCESS
		|| !csmBuf( gv.rows, VkDeviceSize( kCells ) * kBlRowBytes, false, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, why ) )
		return false;
	VkDescriptorBufferInfo dbi { gv.rows.buf, 0, VkDeviceSize( kCells ) * kBlRowBytes };
	VkWriteDescriptorSet w { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
	w.dstSet = gv.set0;
	w.dstBinding = 12;
	w.descriptorCount = 1;
	w.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	w.pBufferInfo = &dbi;
	v.vkUpdateDescriptorSets( v.dev, 1, &w, 0, nullptr );
	const VkDescriptorSetLayout dsls[2] = { gv.dsl0, blDsl };
	VkPipelineLayoutCreateInfo plci { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
	plci.setLayoutCount = 2;
	plci.pSetLayouts = dsls;
	VkFenceCreateInfo fi { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
	VkCommandBufferAllocateInfo cai { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
	cai.commandPool = v.pool;
	cai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	cai.commandBufferCount = 1;
	if ( v.vkCreatePipelineLayout( v.dev, &plci, nullptr, &gv.layout ) != VK_SUCCESS || v.vkCreateFence( v.dev, &fi, nullptr, &gv.fence ) != VK_SUCCESS
		|| v.vkAllocateCommandBuffers( v.dev, &cai, &gv.cmd ) != VK_SUCCESS || !exportSem( gv.glDone, gv.glDoneGl, why )
		|| !exportSem( gv.vkDone, gv.vkDoneGl, why ) ) {
		why = "layout / fence / command buffer / semaphores: " + why;
		return false;
	}
	const char * psName[2] = { "blview_ps.spv", "blview_ps_redslot.spv" };
	const std::vector<std::uint32_t> vsCode = readSpv( spvDir + "/blview_vs.spv" );
	for ( int p = 0; p < 2; p++ ) {
		const std::vector<std::uint32_t> psCode = readSpv( spvDir + "/" + psName[p] );
		if ( vsCode.empty() || psCode.empty() ) {
			why = std::string( "no SPIR-V in " ) + spvDir;
			return false;
		}
		VkShaderModule mods[2] = {};
		const std::vector<std::uint32_t> * code[2] = { &vsCode, &psCode };
		for ( int s = 0; s < 2; s++ ) {
			VkShaderModuleCreateInfo mci { VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
			mci.codeSize = code[s]->size() * 4;
			mci.pCode = code[s]->data();
			if ( v.vkCreateShaderModule( v.dev, &mci, nullptr, &mods[s] ) != VK_SUCCESS ) {
				why = "shader module";
				return false;
			}
		}
		VkPipelineShaderStageCreateInfo st[2] = { { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO },
			{ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO } };
		st[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
		st[0].module = mods[0];
		st[0].pName = "main";
		st[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
		st[1].module = mods[1];
		st[1].pName = "main";
		VkPipelineVertexInputStateCreateInfo vi { VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
		VkPipelineInputAssemblyStateCreateInfo ia { VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
		ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		const VkViewport vp { 0.0f, 0.0f, float( kW ), float( kH ), 0.0f, 1.0f };
		const VkRect2D sc { { 0, 0 }, { std::uint32_t( kW ), std::uint32_t( kH ) } };
		VkPipelineViewportStateCreateInfo vps { VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO };
		vps.viewportCount = vps.scissorCount = 1;
		vps.pViewports = &vp;
		vps.pScissors = &sc;
		VkPipelineRasterizationStateCreateInfo rs { VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
		rs.polygonMode = VK_POLYGON_MODE_FILL;
		rs.cullMode = VK_CULL_MODE_NONE;
		rs.lineWidth = 1.0f;
		VkPipelineMultisampleStateCreateInfo ms { VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
		ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
		VkPipelineColorBlendAttachmentState cba {};
		cba.colorWriteMask = 0xF;
		VkPipelineColorBlendStateCreateInfo cbs { VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
		cbs.attachmentCount = 1;
		cbs.pAttachments = &cba;
		VkGraphicsPipelineCreateInfo gp { VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
		gp.stageCount = 2;
		gp.pStages = st;
		gp.pVertexInputState = &vi;
		gp.pInputAssemblyState = &ia;
		gp.pViewportState = &vps;
		gp.pRasterizationState = &rs;
		gp.pMultisampleState = &ms;
		gp.pColorBlendState = &cbs;
		gp.layout = gv.layout;
		gp.renderPass = gv.pass;
		const bool ok = v.vkCreateGraphicsPipelines( v.dev, VK_NULL_HANDLE, 1, &gp, nullptr, &gv.pipe[p] ) == VK_SUCCESS;
		v.vkDestroyShaderModule( v.dev, mods[0], nullptr );
		v.vkDestroyShaderModule( v.dev, mods[1], nullptr );
		if ( !ok ) {
			why = "graphics pipeline";
			return false;
		}
	}
	return true;
}

void gvFree()
{
	Vk & v = vk();
	drain();
	v.vkDeviceWaitIdle( v.dev );
	for ( VkPipeline & p : gv.pipe )
		if ( p )
			v.vkDestroyPipeline( v.dev, p, nullptr );
	if ( gv.layout )
		v.vkDestroyPipelineLayout( v.dev, gv.layout, nullptr );
	if ( gv.fb )
		v.vkDestroyFramebuffer( v.dev, gv.fb, nullptr );
	if ( gv.pass )
		v.vkDestroyRenderPass( v.dev, gv.pass, nullptr );
	if ( gv.pool0 )
		v.vkDestroyDescriptorPool( v.dev, gv.pool0, nullptr );
	if ( gv.dsl0 )
		v.vkDestroyDescriptorSetLayout( v.dev, gv.dsl0, nullptr );
	if ( gv.fence )
		v.vkDestroyFence( v.dev, gv.fence, nullptr );
	if ( gv.cmd )
		v.vkFreeCommandBuffers( v.dev, v.pool, 1, &gv.cmd );
	csmBufFree( gv.rows, false );
	if ( gv.glDoneGl )
		gl().deleteSemaphores( 1, &gv.glDoneGl );
	if ( gv.vkDoneGl )
		gl().deleteSemaphores( 1, &gv.vkDoneGl );
	for ( VkSemaphore s : { gv.glDone, gv.vkDone } )
		if ( s )
			v.vkDestroySemaphore( v.dev, s, nullptr );
	gv = Gv();
}

// one Vulkan run: the rows translated, GL's signal (every texture + the target), the worker's draws, GL waits ours
bool gvRun( const std::vector<unsigned char> & glRows, int red, int pipeIdx, const std::vector<GLuint> & texs, std::size_t & whole,
	std::string & why )
{
	Vk & v = vk();
	Gl & g = gl();
	v.vkWaitForFences( v.dev, 1, &gv.fence, VK_TRUE, 1000000000ull );  // the last run (a fresh fence: not signalled, see below)
	whole = WwVkPresent::blTranslate( glRows.data(), gv.rows.ptr, std::size_t( kCells ), red );
	gv.pipeIdx = pipeIdx;
	std::vector<GLenum> lay( texs.size(), kLayoutGeneral );
	g.signalSemaphore( gv.glDoneGl, 0, nullptr, GLuint( texs.size() ), texs.data(), lay.data() );
	g.flush();
	Worker & W = wk();
	std::uint64_t want = 0;
	{
		std::lock_guard<std::mutex> lk( W.m );
		want = W.toneDone + 1;
	}
	Work j;
	j.k = -1;
	j.fn = gvWork;
	hand( j );
	bool ok = false;
	{
		std::unique_lock<std::mutex> lk( W.m );
		W.idle.wait( lk, [&W, want]() { return W.toneDone >= want; } );
		ok = W.toneOk;
		why = W.toneWhy;
	}
	if ( !ok )
		return false;
	g.waitSemaphore( gv.vkDoneGl, 0, nullptr, GLuint( texs.size() ), texs.data(), lay.data() );
	return true;
}

struct Diff
{
	std::size_t covered = 0, diff = 0;
	float maxAbs = 0.0f;
	bool ok() const { return covered > 0 && diff == 0; }
	const char * word() const { return ok() ? "EXACT" : "FAIL"; }
};

Diff compare( const std::vector<float> & a, const std::vector<float> & b )
{
	Diff d;
	if ( a.size() != b.size() )
		return d;
	for ( std::size_t i = 0; i < a.size(); i += 4 ) {
		if ( std::memcmp( &a[i], &b[i], 16 ) == 0 ) {
			if ( a[i] != 0.0f || a[i + 1] != 0.0f || a[i + 2] != 0.0f || a[i + 3] != 0.0f )
				d.covered++;
			continue;
		}
		d.covered++;
		d.diff++;
		for ( int c = 0; c < 4; c++ )
			d.maxAbs = std::max( d.maxAbs, std::fabs( a[i + c] - b[i + c] ) );
	}
	return d;
}

std::uint16_t toHalf( float x )
{
	std::uint32_t u;
	std::memcpy( &u, &x, 4 );
	const std::uint32_t s = ( u >> 16 ) & 0x8000u;
	const int e = int( ( u >> 23 ) & 0xFF ) - 127 + 15;
	const std::uint32_t m = u & 0x7FFFFFu;
	if ( e <= 0 )
		return std::uint16_t( s );
	if ( e >= 31 )
		return std::uint16_t( s | 0x7C00u );
	return std::uint16_t( s | ( std::uint32_t( e ) << 10 ) | ( m >> 13 ) );
}
}  // namespace blv

int main( int argc, char ** argv )
{
	using namespace blv;
	std::setvbuf( stdout, nullptr, _IONBF, 0 );
	if ( argc < 3 ) {
		std::printf( "usage: blview <tree root> <spv dir> [bc]\nBLVIEW GATE FAILED\n" );
		return 2;
	}
	const std::string root = argv[1], spvDir = argv[2];
	const bool bc = argc > 3 && std::string( argv[3] ) == "bc";  // lane VKOBJ3
	gOpenGl = LoadLibraryA( "opengl32.dll" );
	WNDCLASSA wc {};
	wc.style = CS_OWNDC;
	wc.lpfnWndProc = wndProc;
	wc.hInstance = GetModuleHandleA( nullptr );
	wc.lpszClassName = "wwblview";
	RegisterClassA( &wc );
	HWND dummy = CreateWindowA( "wwblview", "dummy", WS_OVERLAPPEDWINDOW, 0, 0, 64, 64, nullptr, nullptr, wc.hInstance, nullptr );
	HDC ddc = GetDC( dummy );
	PIXELFORMATDESCRIPTOR pfd { sizeof( pfd ), 1, PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER, PFD_TYPE_RGBA, 32 };
	SetPixelFormat( ddc, ChoosePixelFormat( ddc, &pfd ), &pfd );
	HGLRC drc = wglCreateContext( ddc );
	wglMakeCurrent( ddc, drc );
	using ChooseFmt = BOOL( WINAPI * )( HDC, const int *, const float *, UINT, int *, UINT * );
	auto choose = reinterpret_cast<ChooseFmt>( wglGetProcAddress( "wglChoosePixelFormatARB" ) );
	const int VW = 256, VH = 256;
	RECT rc { 0, 0, VW, VH };
	AdjustWindowRectEx( &rc, WS_OVERLAPPEDWINDOW, FALSE, WS_EX_NOACTIVATE );
	HWND wnd = CreateWindowExA( WS_EX_NOACTIVATE, "wwblview", "bindless harness", WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, 40, 40,
		rc.right - rc.left, rc.bottom - rc.top, nullptr, nullptr, wc.hInstance, nullptr );
	HDC dc = GetDC( wnd );
	const int attr[] = { 0x2001, 1, 0x2010, 1, 0x2011, 1, 0x2003, 0x2027, 0x2013, 0x202B, 0x2015, 8, 0x2017, 8, 0x2019, 8,
		0x201B, 8, 0x2022, 24, 0x2023, 8, 0x2041, 1, 0x2042, 4, 0 };
	int fmt = 0;
	UINT nf = 0;
	if ( !choose || !choose( dc, attr, nullptr, 1, &fmt, &nf ) || !nf ) {
		std::printf( "BLVIEW GATE FAILED (no 4x MSAA pixel format)\n" );
		return 2;
	}
	PIXELFORMATDESCRIPTOR got {};
	DescribePixelFormat( dc, fmt, sizeof( got ), &got );
	SetPixelFormat( dc, fmt, &got );
	HGLRC glrc = wglCreateContext( dc );
	wglMakeCurrent( dc, glrc );
	wglDeleteContext( drc );
	ReleaseDC( dummy, ddc );
	DestroyWindow( dummy );
	ShowWindow( wnd, SW_SHOWNOACTIVATE );
	SetWindowPos( wnd, HWND_BOTTOM, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE );
	pump();
	if ( !loadAll() ) {
		std::printf( "BLVIEW GATE FAILED (GL entry points: bindless texture, DSA, base instance)\n" );
		return 2;
	}
	std::printf( "target %d x %d, %d cells, GL on %s\n", kW, kH, kCells, reinterpret_cast<const char *>( glGetString( GL_RENDERER ) ) );

	// ---- GL's reference program: the tree's row include and fo4_default.frag's sampler2D( row.tex[k] ) form
	const std::string inc = readFile( root + "/res/shaders/features/near/ww_bindless.glsl" );
	if ( inc.empty() ) {
		std::printf( "BLVIEW GATE FAILED (no ww_bindless.glsl)\n" );
		return 2;
	}
	const std::string vs = "#version 450\n#extension GL_ARB_shader_draw_parameters : require\n#define WW_BL_VERT\n" + inc
		+ "\nvoid main() {\n"
		  "\tconst vec2 corner[6] = vec2[6]( vec2( 0.0, 0.0 ), vec2( 1.0, 0.0 ), vec2( 0.0, 1.0 ), vec2( 0.0, 1.0 ), vec2( 1.0, 0.0 ), vec2( 1.0, 1.0 ) );\n"
		  "\tuint cell = wwDrawIdx;\n"
		  "\tvec2 px = ( vec2( float( cell % 16u ), float( cell / 16u ) ) + corner[gl_VertexID] ) * 24.0;\n"
		  "\tgl_Position = vec4( px / vec2( 384.0, 192.0 ) * 2.0 - 1.0, 0.0, 1.0 );\n"
		  "\twwBlRow = wwDrawIdx;\n}\n";
	const std::string fs = "#version 450\n#extension GL_ARB_bindless_texture : require\n" + inc
		+ "\n#define MAP( k ) sampler2D( wwDraws[wwDrawIdx].tex[k] )\n"
		  "layout( location = 0 ) out vec4 o;\n"
		  "void main() {\n"
		  "\tuint cell = wwBlRow;\n"
		  "\tuint k = cell % 8u;\n"
		  "\tvec2 org = vec2( float( cell % 16u ), float( cell / 16u ) ) * 24.0;\n"
		  "\tfloat scl = float( 1u << ( ( cell / 8u ) % 4u ) ) * 0.0625;\n"
		  "\tfloat off = 0.125 * float( cell % 3u );\n"
		  "\tvec2 uv = ( gl_FragCoord.xy - org ) * scl + off;\n"
		  "\tif ( k == 0u ) o = texture( MAP( 0 ), uv );\n"
		  "\telse if ( k == 1u ) o = texture( MAP( 1 ), uv, 1.25 );\n"
		  "\telse if ( k == 2u ) o = textureLod( MAP( 2 ), uv, 1.5 );\n"
		  "\telse if ( k == 3u ) o = textureGrad( MAP( 3 ), uv, vec2( scl * 1.7, 0.0 ), vec2( 0.0, scl * 0.6 ) );\n"
		  "\telse if ( k == 4u ) o = texture( MAP( 4 ), uv );\n"
		  "\telse if ( k == 5u ) o = textureLod( MAP( 5 ), uv, 0.0 );\n"
		  "\telse if ( k == 6u ) o = texture( MAP( 6 ), uv, -0.75 );\n"
		  "\telse o = texture( MAP( 7 ), uv );\n}\n";
	const GLuint prog = program( vs, fs );
	if ( !prog ) {
		std::printf( "BLVIEW GATE FAILED (GL program)\n" );
		return 2;
	}

	std::string why;
	if ( !WwVkPresent::viewBegin( VW, VH, 4, glProc, why ) ) {
		std::printf( "BLVIEW GATE FAILED (the view: %s)\n", why.c_str() );
		return 2;
	}
	if ( const std::string no = WwVkPresent::blRefusal(); !no.empty() ) {
		std::printf( "BLVIEW GATE FAILED (blRefusal: %s)\n", no.c_str() );
		return 2;
	}
	std::printf( "device: descriptor indexing caps %u images, %u samplers; anisotropy %s (max %g)\n", vk().blImageCap, vk().blSamplerCap,
		vk().anisotropy ? "on" : "off", double( vk().maxAniso ) );

	// ---- the textures: shared targets GL fills level by level, then its sampler state, then a resident handle
	struct TexDef
	{
		GLenum fmt;
		int w, h, levels;
		GLenum minF, magF, wrapS, wrapT;
		float bias, minLod, maxLod, aniso;
		float border[4];
	};
	const TexDef defs[kTex] = {
		{ kRgba8i, 64, 64, 7, 0x2703, 0x2601, 0x2901, 0x2901, 0.0f, -1000.0f, 1000.0f, 1.0f, {} },
		{ kRgba16fi, 32, 32, 6, 0x2700, 0x2600, 0x812F, 0x812F, 0.0f, -1000.0f, 1000.0f, 1.0f, {} },
		{ kRgba32fi, 16, 16, 5, 0x2701, 0x2601, 0x8370, 0x8370, 0.0f, -1000.0f, 1000.0f, 1.0f, {} },
		{ kRgba8i, 128, 128, 8, 0x2702, 0x2601, 0x2901, 0x812F, 0.5f, -1000.0f, 1000.0f, 1.0f, {} },
		{ kRgba8i, 8, 8, 1, 0x2601, 0x2600, 0x2901, 0x2901, 0.0f, -1000.0f, 1000.0f, 1.0f, {} },
		{ kRgba16fi, 64, 16, 7, 0x2703, 0x2601, 0x2901, 0x2901, 0.0f, -1000.0f, 1000.0f, 4.0f, {} },
		{ kRgba8i, 32, 32, 6, 0x2703, 0x2601, 0x812D, 0x812D, 0.0f, 1.0f, 3.5f, 1.0f, { 0.0f, 0.0f, 0.0f, 1.0f } },
		{ kRgba32fi, 32, 32, 1, 0x2600, 0x2600, 0x2901, 0x2901, 0.0f, -1000.0f, 1000.0f, 1.0f, {} },
		{ kRgba8i, 16, 16, 5, 0x2703, 0x2600, 0x8370, 0x8370, 0.0f, -1000.0f, 1000.0f, 1.0f, {} },
		{ kRgba16fi, 128, 128, 8, 0x2701, 0x2601, 0x2901, 0x2901, -1.0f, -1000.0f, 1000.0f, 1.0f, {} },
		{ kRgba8i, 64, 64, 7, 0x2700, 0x2601, 0x812F, 0x812F, 0.0f, -1000.0f, 2.0f, 1.0f, {} },
		{ kRgba8i, 32, 32, 6, 0x2601, 0x2601, 0x2901, 0x2901, 0.0f, -1000.0f, 1000.0f, 1.0f, {} },
	};
	static unsigned keyHome[kTex + 1];
	std::vector<GLuint> texs;
	std::vector<GLuint64> handles( kTex, 0 );
	std::mt19937 rng( 23 );
	std::uniform_real_distribution<float> u01( 0.0f, 1.0f );
	bool ok = true;
	// lane VKOBJ3: the cache's BC twin of def t (random blocks per level, the def's sampler state, gli's swizzle on BC4 / BC5)
	const GLenum bcFmt[kTex] = { 0x83F0, 0x83F1, 0x83F2, 0x83F3, 0x8DBB, 0x8DBD, 0x8E8C, 0x8C4C, 0x8C4F, 0x8DBE, 0x8E8D, 0x8C4D };
	double makeMs = 0.0;
	int made = 0;
	auto makeBc = [&]( int t, GLuint & texOut, GLuint64 & hOut ) {
		const TexDef & d = defs[t];
		unsigned tex = 0;
		const auto t0 = std::chrono::steady_clock::now();
		if ( !WwVkPresent::objTexMake( bcFmt[t], d.w, d.h, d.levels, tex, why ) ) {
			std::printf( "BLVIEW GATE FAILED (objTexMake %d: %s)\n", t, why.c_str() );
			return false;
		}
		makeMs += std::chrono::duration<double, std::milli>( std::chrono::steady_clock::now() - t0 ).count();
		made++;
		const int blockBytes = fcFormat( bcFmt[t] ).block;
		for ( int l = 0; l < d.levels; l++ ) {
			const int lw = std::max( d.w >> l, 1 ), lh = std::max( d.h >> l, 1 );
			std::vector<unsigned char> blocks( size_t( ( lw + 3 ) / 4 ) * size_t( ( lh + 3 ) / 4 ) * size_t( blockBytes ) );
			for ( unsigned char & b : blocks )
				b = static_cast<unsigned char>( rng() & 0xFF );
			f.compressedTextureSubImage2D( tex, l, 0, 0, lw, lh, bcFmt[t], GLsizei( blocks.size() ), blocks.data() );
		}
		f.textureParameteri( tex, GL_TEXTURE_MIN_FILTER, GLint( d.minF ) );
		f.textureParameteri( tex, GL_TEXTURE_MAG_FILTER, GLint( d.magF ) );
		f.textureParameteri( tex, GL_TEXTURE_WRAP_S, GLint( d.wrapS ) );
		f.textureParameteri( tex, GL_TEXTURE_WRAP_T, GLint( d.wrapT ) );
		f.textureParameterf( tex, 0x8501, d.bias );
		f.textureParameterf( tex, 0x813A, d.minLod );
		f.textureParameterf( tex, 0x813B, d.maxLod );
		f.textureParameterf( tex, 0x84FE, d.aniso );
		f.textureParameterfv( tex, 0x1004, d.border );
		if ( bcFmt[t] == 0x8DBB || bcFmt[t] == 0x8DBD || bcFmt[t] == 0x8DBE ) {  // gli: R,0,0,1 / R,G,0,1
			const bool two = bcFmt[t] != 0x8DBB;
			f.textureParameteri( tex, 0x8E42, GL_RED );
			f.textureParameteri( tex, 0x8E43, two ? GL_GREEN : GL_ZERO );
			f.textureParameteri( tex, 0x8E44, GL_ZERO );
			f.textureParameteri( tex, 0x8E45, GL_ONE );
		}
		hOut = f.getTextureHandle( tex );
		f.makeResident( hOut );
		if ( !hOut ) {
			std::printf( "BLVIEW GATE FAILED (BC texture %d: no handle)\n", t );
			return false;
		}
		WwVkPresent::objTexHandle( tex, hOut, glProc );
		texOut = GLuint( tex );
		return true;
	};
	for ( int t = 0; t < kTex; t++ ) {
		if ( bc ) {
			GLuint tx = 0;
			if ( !makeBc( t, tx, handles[t] ) )
				return 2;
			texs.push_back( tx );
			continue;
		}
		const TexDef & d = defs[t];
		const std::uintptr_t key = reinterpret_cast<std::uintptr_t>( &keyHome[t] );
		unsigned tex = 0;
		if ( !WwVkPresent::sharedTarget( key, 0, d.w, d.h, 1, d.fmt, tex, why, d.levels, 1 ) ) {
			std::printf( "BLVIEW GATE FAILED (sharedTarget %d: %s)\n", t, why.c_str() );
			return 2;
		}
		for ( int l = 0; l < d.levels; l++ ) {
			const int lw = std::max( d.w >> l, 1 ), lh = std::max( d.h >> l, 1 );
			std::vector<float> px( size_t( lw ) * lh * 4 );
			for ( float & x : px )
				x = d.fmt == kRgba8i ? float( int( u01( rng ) * 255.0f ) ) / 255.0f : 4.0f * u01( rng ) - 1.0f;
			if ( d.fmt == kRgba16fi ) {
				std::vector<std::uint16_t> hx( px.size() );
				for ( std::size_t i = 0; i < px.size(); i++ )
					hx[i] = toHalf( px[i] );
				f.textureSubImage2D( tex, l, 0, 0, lw, lh, GL_RGBA, kHalf, hx.data() );
			} else
				f.textureSubImage2D( tex, l, 0, 0, lw, lh, GL_RGBA, GL_FLOAT, px.data() );
		}
		f.textureParameteri( tex, GL_TEXTURE_MIN_FILTER, GLint( d.minF ) );
		f.textureParameteri( tex, GL_TEXTURE_MAG_FILTER, GLint( d.magF ) );
		f.textureParameteri( tex, GL_TEXTURE_WRAP_S, GLint( d.wrapS ) );
		f.textureParameteri( tex, GL_TEXTURE_WRAP_T, GLint( d.wrapT ) );
		f.textureParameterf( tex, 0x8501, d.bias );
		f.textureParameterf( tex, 0x813A, d.minLod );
		f.textureParameterf( tex, 0x813B, d.maxLod );
		f.textureParameterf( tex, 0x84FE, d.aniso );
		f.textureParameterfv( tex, 0x1004, d.border );
		WwVkPresent::BlSampler s;
		if ( !WwVkPresent::blSamplerRead( tex, s, glProc, why ) ) {
			std::printf( "BLVIEW GATE FAILED (blSamplerRead %d: %s)\n", t, why.c_str() );
			return 2;
		}
		handles[t] = f.getTextureHandle( tex );
		f.makeResident( handles[t] );
		if ( !handles[t] || !WwVkPresent::blAdd( handles[t], key, s, why ) ) {
			std::printf( "BLVIEW GATE FAILED (texture %d: handle %llx, blAdd: %s)\n", t, static_cast<unsigned long long>( handles[t] ),
				why.c_str() );
			return 2;
		}
		texs.push_back( tex );
	}
	if ( const GLenum e = glGetError(); e != GL_NO_ERROR ) {
		std::printf( "BLVIEW GATE FAILED (GL error 0x%x making the textures)\n", unsigned( e ) );
		return 2;
	}
	if ( bc ) {
		const WwVkPresent::ObjTexStat s = WwVkPresent::objTexFrame();
		std::printf( "BLVIEW bc twins: %d made, %d refused, %d handles added, %d add fails (%s), %.1f MB, objTexMake %.2f ms each\n",
			s.made, s.refused, s.adds, s.addFails, s.addWhy.c_str(), s.mb, made ? makeMs / made : 0.0 );
		if ( s.made != kTex || s.adds != kTex || s.addFails ) {
			std::printf( "BLVIEW GATE FAILED (the BC twins did not all join)\n" );
			return 2;
		}
	}

	// ---- the rows: a skewed matrix block (copied through as is) and 8 handles each, every texture in every slot somewhere
	std::vector<unsigned char> rows( size_t( kCells ) * kBlRowBytes, 0 );
	auto fillRows = [&]() {
		for ( int r = 0; r < kCells; r++ ) {
			float m[28];
			for ( int i = 0; i < 28; i++ )
				m[i] = float( r ) * 0.25f + float( i ) * 0.125f;
			std::memcpy( &rows[size_t( r ) * kBlRowBytes], m, sizeof( m ) );
			for ( int i = 0; i < 8; i++ ) {
				const GLuint64 h = handles[size_t( ( r * 5 + i * 7 ) % kTex )];
				std::memcpy( &rows[size_t( r ) * kBlRowBytes + kBlTexAt + 8 * size_t( i )], &h, 8 );
			}
		}
	};
	fillRows();
	GLuint ssbo = 0, vao = 0, fbo = 0, refT = 0;
	f.genBuffers( 1, &ssbo );
	f.bindBuffer( kSsbo, ssbo );
	f.bufferData( kSsbo, std::ptrdiff_t( rows.size() ), rows.data(), kDynamic );
	f.genVertexArrays( 1, &vao );
	f.genFramebuffers( 1, &fbo );
	glGenTextures( 1, &refT );
	glBindTexture( GL_TEXTURE_2D, refT );
	glTexImage2D( GL_TEXTURE_2D, 0, GLint( kRgba32fi ), kW, kH, 0, GL_RGBA, GL_FLOAT, nullptr );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
	glBindTexture( GL_TEXTURE_2D, 0 );
	const std::uintptr_t targetKey = reinterpret_cast<std::uintptr_t>( &keyHome[kTex] );
	unsigned target = 0;
	if ( !WwVkPresent::sharedTarget( targetKey, 0, kW, kH, 1, kRgba32fi, target, why, 1, 1 ) || !gvBuild( targetKey, spvDir, why ) ) {
		std::printf( "BLVIEW GATE FAILED (the Vulkan side: %s)\n", why.c_str() );
		return 2;
	}
	std::vector<GLuint> crossing = texs;
	crossing.push_back( GLuint( target ) );
	std::size_t bcRowsWhole = 0, bcRows = 0;  // lane VKOBJ3: objTexRows over the frames' rows (the census the app line prints)

	auto runGl = [&]( std::vector<float> & out ) {
		f.bindFramebuffer( kFb, fbo );
		f.framebufferTexture2D( kFb, kColorAtt0, GL_TEXTURE_2D, refT, 0 );
		const GLenum att = kColorAtt0;
		glDrawBuffer( att );
		if ( f.checkFramebufferStatus( kFb ) != kComplete )
			return false;
		glViewport( 0, 0, kW, kH );
		glDisable( GL_SCISSOR_TEST );
		glDisable( GL_DEPTH_TEST );
		glDisable( GL_BLEND );
		glDisable( GL_CULL_FACE );
		glColorMask( GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE );
		glClearColor( 0.0f, 0.0f, 0.0f, 0.0f );
		glClear( GL_COLOR_BUFFER_BIT );
		f.useProgram( prog );
		f.bindBufferBase( kSsbo, 12, ssbo );
		f.bindVertexArray( vao );
		for ( int c = 0; c < kCells; c++ )
			f.drawArraysInstancedBaseInstance( GL_TRIANGLES, 0, 6, 1, GLuint( c ) );
		f.bindVertexArray( 0 );
		f.bindFramebuffer( kFb, 0 );
		out.assign( size_t( kW ) * kH * 4, 0.0f );
		f.getTextureImage( refT, 0, GL_RGBA, GL_FLOAT, GLsizei( out.size() * 4 ), out.data() );
		return glGetError() == GL_NO_ERROR;
	};
	auto runVk = [&]( int red, int pipeIdx, std::vector<float> & out, std::size_t & whole ) {
		std::string w;
		if ( !gvRun( rows, red, pipeIdx, crossing, whole, w ) ) {
			std::printf( "BLVIEW the Vulkan run failed: %s\n", w.c_str() );
			return false;
		}
		out.assign( size_t( kW ) * kH * 4, 0.0f );
		f.getTextureImage( GLuint( target ), 0, GL_RGBA, GL_FLOAT, GLsizei( out.size() * 4 ), out.data() );
		return glGetError() == GL_NO_ERROR;
	};

	std::vector<float> ref, gotV;
	for ( int fr = 0; fr < 2; fr++ ) {
		if ( fr == 1 && bc ) {  // lane VKOBJ3: texture 3 deleted by the cache, a new twin in its slot (new data, new handle)
			WwVkPresent::objTexGone( texs[3] );
			f.makeNonResident( handles[3] );
			glDeleteTextures( 1, &texs[3] );
			GLuint tx = 0;
			if ( !makeBc( 3, tx, handles[3] ) ) {
				ok = false;
				continue;
			}
			texs[3] = crossing[3] = tx;
			fillRows();
			f.bindBuffer( kSsbo, ssbo );
			f.bufferData( kSsbo, std::ptrdiff_t( rows.size() ), rows.data(), kDynamic );
			const WwVkPresent::ObjTexStat s = WwVkPresent::objTexFrame();
			std::printf( "BLVIEW bc frame1: gone %d, freed %d (%.2f ms), made %d, adds %d, add fails %d; twins %d\n", s.gone, s.freed,
				s.freeMs, s.made, s.adds, s.addFails, s.twins );
			if ( s.gone != 1 || s.freed != 1 || s.adds != 1 || s.twins != kTex ) {
				std::printf( "BLVIEW frame1 FAIL (the BC replace)\n" );
				ok = false;
				continue;
			}
		} else if ( fr == 1 ) {  // texture 3 non-resident and back: its image index goes to the graves, a new one is made
			f.makeNonResident( handles[3] );
			WwVkPresent::blDrop( handles[3] );
			f.makeResident( handles[3] );
			WwVkPresent::BlSampler s;
			const bool again = WwVkPresent::blSamplerRead( texs[3], s, glProc, why )
				&& WwVkPresent::blAdd( handles[3], reinterpret_cast<std::uintptr_t>( &keyHome[3] ), s, why );
			if ( !again ) {
				std::printf( "BLVIEW frame1 FAIL (the re-add: %s)\n", why.c_str() );
				ok = false;
				continue;
			}
		}
		std::size_t whole = 0;
		if ( !runGl( ref ) || !runVk( 0, 0, gotV, whole ) ) {
			std::printf( "BLVIEW frame%d FAIL (a run failed)\n", fr );
			ok = false;
			continue;
		}
		const Diff d = compare( gotV, ref );
		std::printf( "BLVIEW frame%d %s (%zu of %zu covered pixels differ, max %g; rows with every handle known %zu of %d)\n", fr, d.word(),
			d.diff, d.covered, double( d.maxAbs ), whole, kCells );
		ok = ok && d.ok() && whole == std::size_t( kCells );
		if ( bc ) {
			WwVkPresent::objTexRows( rows.data(), std::size_t( kCells ) );
			const WwVkPresent::ObjTexStat s = WwVkPresent::objTexFrame();
			bcRows += s.rows;
			bcRowsWhole += s.whole;
		}
	}
	if ( bc ) {  // lane VKOBJ3: the census the app's line prints, then the red `key`
		std::printf( "BLVIEW bc rows: %zu of %zu whole through objTexRows\n", bcRowsWhole, bcRows );
		ok = ok && bcRows == std::size_t( 2 * kCells ) && bcRowsWhole == bcRows;
		for ( int t = 0; t < kTex; t++ )
			WwVkPresent::objTexHandle( texs[t], handles[t], glProc, 1 );
		std::size_t whole = 0;
		const bool ran = runVk( 0, 0, gotV, whole );
		const Diff d = compare( gotV, ref );
		const bool redFails = !ran || !d.ok();
		std::printf( "BLVIEW RED %s key (each handle with the twin made before its texture's; must FAIL) (%zu of %zu covered pixels differ)\n",
			redFails ? "FAIL" : "EXACT", d.diff, d.covered );
		ok = ok && redFails;
		// the red's own refusals (a BC4/BC5 swizzle met another format's image) stay with the red
		const WwVkPresent::ObjTexStat sr = WwVkPresent::objTexFrame();
		std::printf( "BLVIEW bc red: adds %d, refused %d (%s)\n", sr.adds, sr.addFails, sr.addWhy.c_str() );
		for ( int t = 0; t < kTex; t++ )
			WwVkPresent::objTexHandle( texs[t], handles[t], glProc, 0 );
		const WwVkPresent::ObjTexStat s = WwVkPresent::objTexFrame();
		std::printf( "BLVIEW bc back: adds %d, add fails %d (%s)\n", s.adds, s.addFails, s.addWhy.c_str() );
		ok = ok && s.addFails == 0;
	}
	// the reds against frame 1's reference: each must FAIL
	const struct
	{
		const char * name;
		int red, pipe;
	} reds[3] = { { "table (every image index shifted by one)", 1, 0 }, { "sampler (every map with GL's linear repeat)", 2, 0 },
		{ "slot (the include's WW_VK_BINDLESS_RED_SLOT: every map reads its neighbour's pair)", 0, 1 } };
	for ( const auto & r : reds ) {
		std::size_t whole = 0;
		const bool ran = runVk( r.red, r.pipe, gotV, whole );
		const Diff d = compare( gotV, ref );
		const bool redFails = !ran || !d.ok();
		std::printf( "BLVIEW RED %s %s (%zu of %zu covered pixels differ; must FAIL)\n", redFails ? "FAIL" : "EXACT", r.name, d.diff, d.covered );
		ok = ok && redFails;
	}
	vk().vkWaitForFences( vk().dev, 1, &gv.fence, VK_TRUE, 1000000000ull );
	const WwVkPresent::BlStat st = WwVkPresent::takeBlStat();
	std::printf( "BLVIEW stat: %d images, %d samplers (caps %u / %u), %d adds, %d drops, %d rows, %d misses, %d rewrites, %d fails (%s)\n",
		st.images, st.samplers, st.imageCap, st.samplerCap, st.adds, st.drops, st.rows, st.misses, st.rewrites, st.fails, st.failWhy.c_str() );
	for ( GLuint64 h : handles )
		f.makeNonResident( h );
	gvFree();
	WwVkPresent::shutdown();
	if ( bc ) {  // lane VKOBJ3: the cache deletes its names after the present went (shutdown kept them)
		glDeleteTextures( GLsizei( texs.size() ), texs.data() );
		const GLenum e = glGetError();
		std::printf( "BLVIEW bc names deleted after shutdown: GL error 0x%x\n", unsigned( e ) );
		ok = ok && e == GL_NO_ERROR;
	}
	wglMakeCurrent( nullptr, nullptr );
	wglDeleteContext( glrc );
	DestroyWindow( wnd );
	std::printf( "BLVIEW GATE %s\n", ok ? "OK" : "FAILED" );
	return ok ? 0 : 1;
}
