/* Lane VKFRAME1 (2026-11-12, PRESENT FIRST): GL's finished frame presented through a Vulkan swapchain (vkpresent.h).
 * The core (present / shutdown) has no Qt in it: tests/vkframe/present.cpp builds it with WW_VKPRESENT_NO_QT and
 * times it against SwapBuffers, and checks the presented image against GL's back buffer. */

#ifndef NOMINMAX  // the pch defines it
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include "features/vkframe/vkpresent.h"

#define VK_NO_PROTOTYPES
#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.h>

#include "features/vkframe/tone_view_vs.spv.h"  // round 11: the tone pass on the view (vkframe_spirv.sh)
#include "features/vkframe/tone_view_ps.spv.h"
#include "features/vkframe/taa_view_mv_cs.spv.h"  // lane VKSHARE1: the Modern temporal AA on the view
#include "features/vkframe/taa_view_resolve_cs.spv.h"
#include "features/vkframe/taa_view_rcas_ps.spv.h"
#include "features/vkframe/bloom_down.spv.h"  // lane VKBLOOM1: the Fusion Shaders bloom and the measure in the tone pass
#include "features/vkframe/bloom_hold.spv.h"
#include "features/vkframe/bloom_up.spv.h"
#include "features/vkframe/bloom_src.spv.h"
#include "features/vkframe/bloom_comb.spv.h"
#include "features/vkframe/bloom_back.spv.h"
#include "features/vkframe/measure_q.spv.h"
#include "features/vkframe/sky_view_ps.spv.h"  // lane VKSKY1: the cell sky on the tone frame

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <map>
#include <mutex>
#include <thread>

namespace
{

// GL, by number and by pointer (no GL header: the Qt build and the harness each have their own)
using GLenum = unsigned int;
using GLuint = unsigned int;
using GLint = int;
using GLsizei = int;
using GLbitfield = unsigned int;
using GLubyte = unsigned char;
using GLuint64 = unsigned long long;
constexpr GLenum kTexture2D = 0x0DE1, kRgba8 = 0x8058, kScissorTest = 0x0C11, kReadFb = 0x8CA8, kDrawFb = 0x8CA9,
	kReadFbBinding = 0x8CAA, kDrawFbBinding = 0x8CA6, kColorAttach0 = 0x8CE0, kColorBit = 0x4000, kNearest = 0x2600,
	kTexMinFilter = 0x2801, kTexMagFilter = 0x2800, kTexBinding2D = 0x8069, kLayoutGeneral = 0x958D,
	kHandleOpaqueWin32 = 0x9587, kDedicated = 0x9581, kNumDeviceUuids = 0x9596, kDeviceUuid = 0x9597,
	kFbComplete = 0x8CD5, kSyncGpuDone = 0x9117, kAlreadySignaled = 0x911A, kConditionSatisfied = 0x911C,
	kTexture2DMs = 0x9100, kDepth24Stencil8 = 0x88F0, kDepthStencilAttach = 0x821A, kFramebuffer = 0x8D40,
	kTexBinding2DMs = 0x9104, kRgba16f = 0x881A, kRgba32f = 0x8814, kDsTexMode = 0x90EA, kStencilIndex = 0x1901;
// lane VKSHARE1: volumes and layered targets, and the single-channel formats the cloud / TAA targets use
constexpr GLenum kTexture3D = 0x806F, kTexBinding3D = 0x806A, kTexture2DArray = 0x8C1A, kTexBinding2DArray = 0x8C1D,
	kGlRg32f = 0x8230, kGlRg16f = 0x822F, kGlR32f = 0x822E, kGlR8 = 0x8229;
constexpr GLuint64 kTimeoutIgnored = ~0ull;

struct Gl
{
	void * ctx = nullptr;
	bool ok = false;
	void( WINAPI * getIntegerv )( GLenum, GLint * ) = nullptr;
	GLenum( WINAPI * getError )() = nullptr;
	void( WINAPI * enable )( GLenum ) = nullptr;
	void( WINAPI * disable )( GLenum ) = nullptr;
	unsigned char( WINAPI * isEnabled )( GLenum ) = nullptr;
	void( WINAPI * flush )() = nullptr;
	void( WINAPI * genTextures )( GLsizei, GLuint * ) = nullptr;
	void( WINAPI * deleteTextures )( GLsizei, const GLuint * ) = nullptr;
	void( WINAPI * bindTexture )( GLenum, GLuint ) = nullptr;
	void( WINAPI * texParameteri )( GLenum, GLenum, GLint ) = nullptr;
	void( WINAPI * genFramebuffers )( GLsizei, GLuint * ) = nullptr;
	void( WINAPI * deleteFramebuffers )( GLsizei, const GLuint * ) = nullptr;
	void( WINAPI * bindFramebuffer )( GLenum, GLuint ) = nullptr;
	void( WINAPI * framebufferTexture2D )( GLenum, GLenum, GLenum, GLuint, GLint ) = nullptr;
	GLenum( WINAPI * checkFramebufferStatus )( GLenum ) = nullptr;
	void( WINAPI * blitFramebuffer )( GLint, GLint, GLint, GLint, GLint, GLint, GLint, GLint, GLbitfield, GLenum ) = nullptr;
	// GL 4.5 (optional): the resolve with no binding asked or changed
	void( WINAPI * blitNamedFramebuffer )( GLuint, GLuint, GLint, GLint, GLint, GLint, GLint, GLint, GLint, GLint, GLbitfield, GLenum ) = nullptr;
	void( WINAPI * createMemoryObjects )( GLsizei, GLuint * ) = nullptr;
	void( WINAPI * deleteMemoryObjects )( GLsizei, const GLuint * ) = nullptr;
	void( WINAPI * memoryObjectParameteriv )( GLuint, GLenum, const GLint * ) = nullptr;
	void( WINAPI * importMemoryWin32Handle )( GLuint, GLuint64, GLenum, void * ) = nullptr;
	void( WINAPI * texStorageMem2D )( GLenum, GLsizei, GLenum, GLsizei, GLsizei, GLuint, GLuint64 ) = nullptr;
	void( WINAPI * genSemaphores )( GLsizei, GLuint * ) = nullptr;
	void( WINAPI * deleteSemaphores )( GLsizei, const GLuint * ) = nullptr;
	void( WINAPI * importSemaphoreWin32Handle )( GLuint, GLenum, void * ) = nullptr;
	void( WINAPI * waitSemaphore )( GLuint, GLuint, const GLuint *, GLuint, const GLuint *, const GLenum * ) = nullptr;
	void( WINAPI * signalSemaphore )( GLuint, GLuint, const GLuint *, GLuint, const GLuint *, const GLenum * ) = nullptr;
	void( WINAPI * getUnsignedBytei )( GLenum, GLuint, GLubyte * ) = nullptr;
	// GL 3.2 (optional): the fence the worker's GL context waits before it signals Vulkan (build 88)
	void *( WINAPI * fenceSync )( GLenum, GLbitfield ) = nullptr;
	void( WINAPI * deleteSync )( void * ) = nullptr;
	// round 10 (optional): the view's multisample images (the view on Vulkan)
	void( WINAPI * texStorageMem2DMultisample )( GLenum, GLsizei, GLenum, GLsizei, GLsizei, unsigned char, GLuint, GLuint64 ) = nullptr;
	// lane VKSHARE1 (optional): 3D and 2D-array shared targets
	void( WINAPI * texStorageMem3D )( GLenum, GLsizei, GLenum, GLsizei, GLsizei, GLsizei, GLuint, GLuint64 ) = nullptr;
	// lane VKSHARE1 (optional, GL 4.3): TAA's velocity and mask into their shared copies
	void( WINAPI * copyImageSubData )( GLuint, GLenum, GLint, GLint, GLint, GLint, GLuint, GLenum, GLint, GLint, GLint, GLint, GLsizei,
		GLsizei, GLsizei ) = nullptr;
};

template <class F> void load( F & f, WwVkPresent::GlProc gp, const char * name )
{
	f = reinterpret_cast<F>( gp( name ) );
}

bool loadGl( Gl & g, WwVkPresent::GlProc gp, std::string & why )
{
	load( g.getIntegerv, gp, "glGetIntegerv" );
	load( g.getError, gp, "glGetError" );
	load( g.enable, gp, "glEnable" );
	load( g.disable, gp, "glDisable" );
	load( g.isEnabled, gp, "glIsEnabled" );
	load( g.flush, gp, "glFlush" );
	load( g.genTextures, gp, "glGenTextures" );
	load( g.deleteTextures, gp, "glDeleteTextures" );
	load( g.bindTexture, gp, "glBindTexture" );
	load( g.texParameteri, gp, "glTexParameteri" );
	load( g.genFramebuffers, gp, "glGenFramebuffers" );
	load( g.deleteFramebuffers, gp, "glDeleteFramebuffers" );
	load( g.bindFramebuffer, gp, "glBindFramebuffer" );
	load( g.framebufferTexture2D, gp, "glFramebufferTexture2D" );
	load( g.checkFramebufferStatus, gp, "glCheckFramebufferStatus" );
	load( g.blitFramebuffer, gp, "glBlitFramebuffer" );
	load( g.blitNamedFramebuffer, gp, "glBlitNamedFramebuffer" );
	load( g.createMemoryObjects, gp, "glCreateMemoryObjectsEXT" );
	load( g.deleteMemoryObjects, gp, "glDeleteMemoryObjectsEXT" );
	load( g.memoryObjectParameteriv, gp, "glMemoryObjectParameterivEXT" );
	load( g.importMemoryWin32Handle, gp, "glImportMemoryWin32HandleEXT" );
	load( g.texStorageMem2D, gp, "glTexStorageMem2DEXT" );
	load( g.genSemaphores, gp, "glGenSemaphoresEXT" );
	load( g.deleteSemaphores, gp, "glDeleteSemaphoresEXT" );
	load( g.importSemaphoreWin32Handle, gp, "glImportSemaphoreWin32HandleEXT" );
	load( g.waitSemaphore, gp, "glWaitSemaphoreEXT" );
	load( g.signalSemaphore, gp, "glSignalSemaphoreEXT" );
	load( g.getUnsignedBytei, gp, "glGetUnsignedBytei_vEXT" );
	load( g.fenceSync, gp, "glFenceSync" );
	load( g.deleteSync, gp, "glDeleteSync" );
	load( g.texStorageMem2DMultisample, gp, "glTexStorageMem2DMultisampleEXT" );
	load( g.texStorageMem3D, gp, "glTexStorageMem3DEXT" );
	load( g.copyImageSubData, gp, "glCopyImageSubData" );
	g.ok = g.getIntegerv && g.getError && g.enable && g.disable && g.isEnabled && g.flush && g.genTextures && g.deleteTextures
		&& g.bindTexture && g.texParameteri && g.genFramebuffers && g.deleteFramebuffers && g.bindFramebuffer
		&& g.framebufferTexture2D && g.checkFramebufferStatus && g.blitFramebuffer && g.createMemoryObjects
		&& g.deleteMemoryObjects && g.memoryObjectParameteriv && g.importMemoryWin32Handle && g.texStorageMem2D
		&& g.genSemaphores && g.deleteSemaphores && g.importSemaphoreWin32Handle && g.waitSemaphore && g.signalSemaphore
		&& g.getUnsignedBytei;
	if ( !g.ok )
		why = "no GL memory / semaphore import (a GL_EXT_memory_object / GL_EXT_semaphore entry point is missing)";
	return g.ok;
}

#define VKP( fn ) PFN_##fn fn = nullptr
struct Vk
{
	bool tried = false, ok = false;
	std::string why;
	HMODULE lib = nullptr;
	VkInstance inst = VK_NULL_HANDLE;
	VkPhysicalDevice phys = VK_NULL_HANDLE;
	VkDevice dev = VK_NULL_HANDLE;
	VkQueue queue = VK_NULL_HANDLE;
	std::uint32_t family = UINT32_MAX;
	std::uint8_t uuid[VK_UUID_SIZE] = {};
	VkPhysicalDeviceMemoryProperties mem {};
	VkCommandPool pool = VK_NULL_HANDLE;
	VKP( vkGetInstanceProcAddr ); VKP( vkCreateInstance ); VKP( vkEnumeratePhysicalDevices );
	VKP( vkGetPhysicalDeviceProperties2 ); VKP( vkGetPhysicalDeviceQueueFamilyProperties ); VKP( vkGetPhysicalDeviceMemoryProperties );
	VKP( vkGetPhysicalDeviceFormatProperties ); VKP( vkCreateDevice ); VKP( vkGetDeviceProcAddr );
	VKP( vkGetPhysicalDeviceImageFormatProperties );
	VKP( vkCreateWin32SurfaceKHR ); VKP( vkDestroySurfaceKHR ); VKP( vkGetPhysicalDeviceSurfaceSupportKHR );
	VKP( vkGetPhysicalDeviceSurfaceCapabilitiesKHR ); VKP( vkGetPhysicalDeviceSurfaceFormatsKHR );
	VKP( vkGetPhysicalDeviceSurfacePresentModesKHR );
	VKP( vkGetDeviceQueue ); VKP( vkDeviceWaitIdle ); VKP( vkCreateSwapchainKHR ); VKP( vkDestroySwapchainKHR );
	VKP( vkGetSwapchainImagesKHR ); VKP( vkAcquireNextImageKHR ); VKP( vkQueuePresentKHR ); VKP( vkQueueSubmit );
	VKP( vkCreateImage ); VKP( vkDestroyImage ); VKP( vkGetImageMemoryRequirements ); VKP( vkAllocateMemory ); VKP( vkFreeMemory );
	VKP( vkBindImageMemory ); VKP( vkGetMemoryWin32HandleKHR ); VKP( vkCreateSemaphore ); VKP( vkDestroySemaphore );
	VKP( vkGetSemaphoreWin32HandleKHR ); VKP( vkCreateFence ); VKP( vkDestroyFence ); VKP( vkWaitForFences ); VKP( vkResetFences );
	VKP( vkGetFenceStatus );
	VKP( vkCreateCommandPool ); VKP( vkAllocateCommandBuffers ); VKP( vkFreeCommandBuffers ); VKP( vkBeginCommandBuffer );
	VKP( vkEndCommandBuffer ); VKP( vkResetCommandBuffer ); VKP( vkCmdPipelineBarrier ); VKP( vkCmdBlitImage );
	VKP( vkCreateBuffer ); VKP( vkDestroyBuffer ); VKP( vkGetBufferMemoryRequirements ); VKP( vkBindBufferMemory );
	VKP( vkMapMemory ); VKP( vkUnmapMemory ); VKP( vkCmdCopyImageToBuffer ); VKP( vkCmdResolveImage ); VKP( vkCmdCopyImage );
	// round 11: the tone pass on the view (a render pass of the present device)
	bool sampleShading = false;  // the device feature SV_SampleIndex needs (enabled when offered)
	VKP( vkGetPhysicalDeviceFeatures );
	VKP( vkCreateImageView ); VKP( vkDestroyImageView ); VKP( vkCreateSampler ); VKP( vkDestroySampler );
	VKP( vkCreateDescriptorSetLayout ); VKP( vkDestroyDescriptorSetLayout ); VKP( vkCreateDescriptorPool );
	VKP( vkDestroyDescriptorPool ); VKP( vkAllocateDescriptorSets ); VKP( vkUpdateDescriptorSets );
	VKP( vkCreatePipelineLayout ); VKP( vkDestroyPipelineLayout ); VKP( vkCreateShaderModule ); VKP( vkDestroyShaderModule );
	VKP( vkCreateGraphicsPipelines ); VKP( vkDestroyPipeline ); VKP( vkCreateRenderPass ); VKP( vkDestroyRenderPass );
	VKP( vkCreateFramebuffer ); VKP( vkDestroyFramebuffer ); VKP( vkCmdBeginRenderPass ); VKP( vkCmdEndRenderPass );
	VKP( vkCmdBindPipeline ); VKP( vkCmdBindDescriptorSets ); VKP( vkCmdPushConstants ); VKP( vkCmdDraw );
	VKP( vkCmdSetViewport ); VKP( vkCmdSetScissor ); VKP( vkCmdCopyBufferToImage );
	VKP( vkCreatePipelineCache ); VKP( vkDestroyPipelineCache ); VKP( vkGetPipelineCacheData );
	// lane VKSHARE1: the temporal AA's compute passes on the view (rg32f vectors need the extended storage formats)
	bool storageExt = false;
	VKP( vkCmdDispatch ); VKP( vkCreateComputePipelines );
};
#undef VKP

/* build 87: with 3 slots GL's queue waited (vkDone) on the present of 3 frames back, and the frame cap fence then
 * waited 0.72 ms on that GL work. 5 slots, and GL no longer waits on Vulkan at all: the drawing thread checks the slot's
 * fence (almost always long signaled) before GL writes into it */
constexpr int kSlots = 5;

struct Slot
{
	VkImage img = VK_NULL_HANDLE;
	VkDeviceMemory mem = VK_NULL_HANDLE;
	VkSemaphore glDone = VK_NULL_HANDLE, vkDone = VK_NULL_HANDLE, acquired = VK_NULL_HANDLE;
	VkFence fence = VK_NULL_HANDLE;
	VkCommandBuffer cmd = VK_NULL_HANDLE;
	GLuint tex = 0, memObj = 0, fbo = 0, glDoneGl = 0, vkDoneGl = 0;
	bool used = false;     // GL waits vkDone before its next blit
	bool pending = false;  // the fence is armed
	bool glWaited = false;  // build 91: the worker saw GL's fence done on the CPU: no GL signal, Vulkan waits no semaphore
};

struct Chain
{
	HWND parent = nullptr, child = nullptr;
	void * glCtx = nullptr;
	VkSurfaceKHR surface = VK_NULL_HANDLE;
	VkSwapchainKHR swap = VK_NULL_HANDLE;
	VkFormat format = VK_FORMAT_UNDEFINED;
	VkExtent2D extent {};
	VkPresentModeKHR mode = VK_PRESENT_MODE_FIFO_KHR;
	bool vsync = false, rebuild = false;
	std::vector<VkImage> images;
	std::vector<VkSemaphore> renderDone;  // one per swapchain image (the present waits it)
	Slot slot[kSlots];
	int w = 0, h = 0;
	std::uint64_t frame = 0;
	std::vector<std::uint8_t> * capture = nullptr;
	/* build 88: GL's signal to Vulkan (a submit: 0.16 ms of the drawing thread with the blit) is sent by a GL context of
	 * the worker's own, sharing GL's objects, on a hidden 1x1 window with GL's pixel format; the drawing thread fences */
	HWND sigWnd = nullptr;
	HDC sigDc = nullptr;
	HGLRC sigCtx = nullptr;
	bool sigTried = false;
	std::string sigWhy;  // why the signal stays on the drawing thread
};

/* Round 10, the view on Vulkan (WW_VKVIEW=1): GL draws the view into images the present device made (MS colour RGBA8
 * + depth / stencil D24S8, the back buffer's format), not into its back buffer; the worker resolves the colour into the
 * ring slot on Vulkan and the drawing thread's GL blit goes. The ring holds 3 views: GL draws view v only once the
 * present that read it (3 frames back) is done (its slot's fence). One GL FBO name for every view (its attachments
 * change): a pass that remembers the frame's FBO across frames still finds it. */
constexpr int kViewSlots = 3;

struct ViewSlot
{
	VkImage color = VK_NULL_HANDLE, ds = VK_NULL_HANDLE;
	VkDeviceMemory colorMem = VK_NULL_HANDLE, dsMem = VK_NULL_HANDLE;
	GLuint colorTex = 0, dsTex = 0, colorObj = 0, dsObj = 0;
	int reader = -1;  // the present ring slot that read it last (-1: none yet)
};

struct View
{
	void * glCtx = nullptr;
	int w = 0, h = 0, samples = 0;
	GLuint fbo = 0;
	ViewSlot slot[kViewSlots];
	std::uint64_t frame = 0;
	int cur = -1;            // drawn this frame, not yet presented
	double mb = 0.0, makeMs = 0.0;
	int busy = 0;            // views whose last present was still running when GL wanted them (since the last take)
	double waitMs = 0.0;
	std::string failWhy;     // the last make that failed (the route stays off at that shape)
	int failW = 0, failH = 0, failS = 0;
	std::uint64_t gen = 0;   // round 11: one more each make (the tone pass's framebuffers follow it)
};

View & view()
{
	static View v;
	return v;
}

/* build 91: who tells Vulkan that GL's frame is done. 0 = the drawing thread's GL signal (0.07-0.16 ms there); 1 = the
 * worker's GL context signals after a GPU wait on the drawing thread's fence (build 88: build 90 read the passes up
 * 1.40 to 1.50, a second context's GPU work); 2 = the worker's GL context waits that fence on the CPU and Vulkan's
 * submit waits nothing (no GL work from a second context). WW_VKPRESENT_SIGNAL, or the Vulkan Present Signal row */
constexpr int kSignalDefault = 2;
int & signalRoute()
{
	static int route = kSignalDefault;
	return route;
}

Vk & vk()
{
	static Vk v;
	return v;
}
Chain & ch()
{
	static Chain c;
	return c;
}
Gl & gl()
{
	static Gl g;
	return g;
}
WwVkPresent::Stat & stat()
{
	static WwVkPresent::Stat s;
	return s;
}

double nowMs()
{
	using C = std::chrono::steady_clock;
	return std::chrono::duration<double, std::milli>( C::now().time_since_epoch() ).count();
}

const char * modeName( VkPresentModeKHR m )
{
	switch ( m ) {
	case VK_PRESENT_MODE_MAILBOX_KHR: return "MAILBOX";
	case VK_PRESENT_MODE_IMMEDIATE_KHR: return "IMMEDIATE";
	case VK_PRESENT_MODE_FIFO_RELAXED_KHR: return "FIFO_RELAXED";
	default: return "FIFO";
	}
}

std::uint32_t memType( std::uint32_t bits, VkMemoryPropertyFlags want )
{
	const Vk & v = vk();
	for ( std::uint32_t i = 0; i < v.mem.memoryTypeCount; i++ )
		if ( ( bits & ( 1u << i ) ) && ( v.mem.memoryTypes[i].propertyFlags & want ) == want )
			return i;
	return UINT32_MAX;
}

// GL's GPU, by GL_EXT_memory_object's device UUIDs (none read: the first discrete device)
bool glUuid( std::uint8_t out[VK_UUID_SIZE] )
{
	Gl & g = gl();
	GLint n = 0;
	g.getIntegerv( kNumDeviceUuids, &n );
	if ( n < 1 )
		return false;
	g.getUnsignedBytei( kDeviceUuid, 0, out );
	return true;
}

bool makeDevice( std::string & why )
{
	Vk & v = vk();
	if ( v.tried ) {
		why = v.why;
		return v.ok;
	}
	v.tried = true;
	auto fail = [&]( const char * w ) {
		v.why = w;
		why = v.why;
		return false;
	};
	v.lib = LoadLibraryA( "vulkan-1.dll" );
	if ( !v.lib )
		return fail( "no vulkan-1.dll" );
	v.vkGetInstanceProcAddr = reinterpret_cast<PFN_vkGetInstanceProcAddr>( GetProcAddress( v.lib, "vkGetInstanceProcAddr" ) );
	if ( !v.vkGetInstanceProcAddr )
		return fail( "vulkan-1.dll has no vkGetInstanceProcAddr" );
	v.vkCreateInstance = reinterpret_cast<PFN_vkCreateInstance>( v.vkGetInstanceProcAddr( nullptr, "vkCreateInstance" ) );
	VkApplicationInfo app { VK_STRUCTURE_TYPE_APPLICATION_INFO };
	app.pApplicationName = "NifSkope WW present";
	app.apiVersion = VK_API_VERSION_1_2;
	const char * iext[] = { VK_KHR_SURFACE_EXTENSION_NAME, VK_KHR_WIN32_SURFACE_EXTENSION_NAME };
	VkInstanceCreateInfo ici { VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };
	ici.pApplicationInfo = &app;
	ici.enabledExtensionCount = 2;
	ici.ppEnabledExtensionNames = iext;
	if ( !v.vkCreateInstance || v.vkCreateInstance( &ici, nullptr, &v.inst ) != VK_SUCCESS )
		return fail( "vkCreateInstance (surface + win32 surface) failed" );
#define LI( fn ) v.fn = reinterpret_cast<PFN_##fn>( v.vkGetInstanceProcAddr( v.inst, #fn ) )
	LI( vkEnumeratePhysicalDevices ); LI( vkGetPhysicalDeviceProperties2 ); LI( vkGetPhysicalDeviceQueueFamilyProperties );
	LI( vkGetPhysicalDeviceMemoryProperties ); LI( vkGetPhysicalDeviceFormatProperties ); LI( vkCreateDevice ); LI( vkGetDeviceProcAddr );
	LI( vkGetPhysicalDeviceImageFormatProperties );
	LI( vkCreateWin32SurfaceKHR ); LI( vkDestroySurfaceKHR ); LI( vkGetPhysicalDeviceSurfaceSupportKHR );
	LI( vkGetPhysicalDeviceSurfaceCapabilitiesKHR ); LI( vkGetPhysicalDeviceSurfaceFormatsKHR );
	LI( vkGetPhysicalDeviceSurfacePresentModesKHR ); LI( vkGetPhysicalDeviceFeatures );
#undef LI
	std::uint8_t want[VK_UUID_SIZE] = {};
	const bool haveUuid = glUuid( want );
	std::uint32_t n = 0;
	v.vkEnumeratePhysicalDevices( v.inst, &n, nullptr );
	std::vector<VkPhysicalDevice> pds( n );
	v.vkEnumeratePhysicalDevices( v.inst, &n, pds.data() );
	for ( VkPhysicalDevice p : pds ) {
		VkPhysicalDeviceIDProperties id { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES };
		VkPhysicalDeviceProperties2 pp { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2 };
		pp.pNext = &id;
		v.vkGetPhysicalDeviceProperties2( p, &pp );
		const bool match = haveUuid ? std::memcmp( id.deviceUUID, want, VK_UUID_SIZE ) == 0
		                            : pp.properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU;
		if ( match && pp.properties.apiVersion >= VK_API_VERSION_1_1 ) {
			v.phys = p;
			std::memcpy( v.uuid, id.deviceUUID, VK_UUID_SIZE );
			break;
		}
	}
	if ( !v.phys )
		return fail( "no Vulkan device on GL's GPU (device UUID)" );
	v.vkGetPhysicalDeviceMemoryProperties( v.phys, &v.mem );
	std::uint32_t nq = 0;
	v.vkGetPhysicalDeviceQueueFamilyProperties( v.phys, &nq, nullptr );
	std::vector<VkQueueFamilyProperties> qf( nq );
	v.vkGetPhysicalDeviceQueueFamilyProperties( v.phys, &nq, qf.data() );
	for ( std::uint32_t i = 0; i < nq && v.family == UINT32_MAX; i++ )
		if ( qf[i].queueFlags & VK_QUEUE_GRAPHICS_BIT )	// the blit needs a graphics queue; the present support is asked per surface
			v.family = i;
	if ( v.family == UINT32_MAX )
		return fail( "no graphics queue family" );
	const float prio = 1.0f;
	VkDeviceQueueCreateInfo qi { VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO };
	qi.queueFamilyIndex = v.family;
	qi.queueCount = 1;
	qi.pQueuePriorities = &prio;
	const char * dext[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME, VK_KHR_EXTERNAL_MEMORY_WIN32_EXTENSION_NAME,
		VK_KHR_EXTERNAL_SEMAPHORE_WIN32_EXTENSION_NAME };
	// round 11: the tone pass's fragment shader reads SV_SampleIndex (the SampleRateShading capability)
	VkPhysicalDeviceFeatures offered {}, en {};
	if ( v.vkGetPhysicalDeviceFeatures )
		v.vkGetPhysicalDeviceFeatures( v.phys, &offered );
	en.sampleRateShading = offered.sampleRateShading;
	v.sampleShading = offered.sampleRateShading == VK_TRUE;
	en.shaderStorageImageExtendedFormats = offered.shaderStorageImageExtendedFormats;
	v.storageExt = offered.shaderStorageImageExtendedFormats == VK_TRUE;
	VkDeviceCreateInfo di { VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
	di.pEnabledFeatures = &en;
	di.queueCreateInfoCount = 1;
	di.pQueueCreateInfos = &qi;
	di.enabledExtensionCount = 3;
	di.ppEnabledExtensionNames = dext;
	if ( v.vkCreateDevice( v.phys, &di, nullptr, &v.dev ) != VK_SUCCESS )
		return fail( "vkCreateDevice (swapchain + external memory / semaphore win32) failed" );
#define LD( fn ) v.fn = reinterpret_cast<PFN_##fn>( v.vkGetDeviceProcAddr( v.dev, #fn ) )
	LD( vkGetDeviceQueue ); LD( vkDeviceWaitIdle ); LD( vkCreateSwapchainKHR ); LD( vkDestroySwapchainKHR );
	LD( vkGetSwapchainImagesKHR ); LD( vkAcquireNextImageKHR ); LD( vkQueuePresentKHR ); LD( vkQueueSubmit );
	LD( vkCreateImage ); LD( vkDestroyImage ); LD( vkGetImageMemoryRequirements ); LD( vkAllocateMemory ); LD( vkFreeMemory );
	LD( vkBindImageMemory ); LD( vkGetMemoryWin32HandleKHR ); LD( vkCreateSemaphore ); LD( vkDestroySemaphore );
	LD( vkGetSemaphoreWin32HandleKHR ); LD( vkCreateFence ); LD( vkDestroyFence ); LD( vkWaitForFences ); LD( vkResetFences );
	LD( vkGetFenceStatus );
	LD( vkCreateCommandPool ); LD( vkAllocateCommandBuffers ); LD( vkFreeCommandBuffers ); LD( vkBeginCommandBuffer );
	LD( vkEndCommandBuffer ); LD( vkResetCommandBuffer ); LD( vkCmdPipelineBarrier ); LD( vkCmdBlitImage );
	LD( vkCreateBuffer ); LD( vkDestroyBuffer ); LD( vkGetBufferMemoryRequirements ); LD( vkBindBufferMemory );
	LD( vkMapMemory ); LD( vkUnmapMemory ); LD( vkCmdCopyImageToBuffer ); LD( vkCmdResolveImage ); LD( vkCmdCopyImage );
	LD( vkCreateImageView ); LD( vkDestroyImageView ); LD( vkCreateSampler ); LD( vkDestroySampler );
	LD( vkCreateDescriptorSetLayout ); LD( vkDestroyDescriptorSetLayout ); LD( vkCreateDescriptorPool );
	LD( vkDestroyDescriptorPool ); LD( vkAllocateDescriptorSets ); LD( vkUpdateDescriptorSets );
	LD( vkCreatePipelineLayout ); LD( vkDestroyPipelineLayout ); LD( vkCreateShaderModule ); LD( vkDestroyShaderModule );
	LD( vkCreateGraphicsPipelines ); LD( vkDestroyPipeline ); LD( vkCreateRenderPass ); LD( vkDestroyRenderPass );
	LD( vkCreateFramebuffer ); LD( vkDestroyFramebuffer ); LD( vkCmdBeginRenderPass ); LD( vkCmdEndRenderPass );
	LD( vkCmdBindPipeline ); LD( vkCmdBindDescriptorSets ); LD( vkCmdPushConstants ); LD( vkCmdDraw );
	LD( vkCmdSetViewport ); LD( vkCmdSetScissor ); LD( vkCmdCopyBufferToImage );
	LD( vkCreatePipelineCache ); LD( vkDestroyPipelineCache ); LD( vkGetPipelineCacheData );
	LD( vkCmdDispatch ); LD( vkCreateComputePipelines );
#undef LD
	if ( !v.vkGetMemoryWin32HandleKHR || !v.vkGetSemaphoreWin32HandleKHR || !v.vkCreateSwapchainKHR )
		return fail( "the device lacks a swapchain / win32 export entry point" );
	v.vkGetDeviceQueue( v.dev, v.family, 0, &v.queue );
	VkCommandPoolCreateInfo pci { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
	pci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	pci.queueFamilyIndex = v.family;
	if ( v.vkCreateCommandPool( v.dev, &pci, nullptr, &v.pool ) != VK_SUCCESS )
		return fail( "vkCreateCommandPool failed" );
	v.ok = true;
	return true;
}

// the child window: over the view's client area, transparent to the mouse (HTTRANSPARENT: the view under it, same
// thread, takes the click), never activated, no erase (the swapchain fills it)
LRESULT CALLBACK childProc( HWND h, UINT m, WPARAM wp, LPARAM lp )
{
	switch ( m ) {
	case WM_NCHITTEST:
		return HTTRANSPARENT;
	case WM_MOUSEACTIVATE:
		return MA_NOACTIVATE;
	case WM_ERASEBKGND:
		return 1;
	case WM_PAINT:
		ValidateRect( h, nullptr );
		return 0;
	default:
		return DefWindowProcW( h, m, wp, lp );
	}
}

HWND makeChild( HWND parent, int w, int h )
{
	static ATOM cls = 0;
	if ( !cls ) {
		WNDCLASSEXW wc { sizeof( wc ) };
		wc.lpfnWndProc = childProc;
		wc.hInstance = GetModuleHandleW( nullptr );
		wc.lpszClassName = L"WwVkPresentChild";
		cls = RegisterClassExW( &wc );
		if ( !cls && GetLastError() == ERROR_CLASS_ALREADY_EXISTS )
			cls = 1;
	}
	return CreateWindowExW( WS_EX_NOPARENTNOTIFY, L"WwVkPresentChild", L"", WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_DISABLED,
		0, 0, w, h, parent, nullptr, GetModuleHandleW( nullptr ), nullptr );
}

/* the present worker (build 85: the present held the drawing thread 1.09 ms a frame): the Vulkan half (fence, acquire,
 * the flipped blit, submit, present) runs on its own thread; GL hands it the slot once its signal is flushed. The queue
 * and the swapchain are the worker's alone until drain() returns: every rebuild and teardown drains first. */
bool vkPart( int k, int w, int h, int vs, std::string & why, bool & rebuild, WwVkPresent::Split & sp );

struct Work
{
	int k = 0, w = 0, h = 0;
	int vs = -1;  // round 10: the view GL drew this frame (resolved on Vulkan into slot k); -1: GL blitted into slot k
	void * sync = nullptr;  // build 88: the worker's GL context waits this fence, then signals sem (null: signaled already)
	GLuint sem = 0, tex = 0;
	bool cpuWait = false;  // build 91: wait the fence on the CPU, no GL signal (signalRoute 2)
	HDC dc = nullptr;  // k < 0: the worker takes ctx current and keeps it (null ctx: lets its context go)
	HGLRC ctx = nullptr;
	bool tone = false;  // round 11 (k = -1): record and submit the tone pass on the view (toneWork)
	bool taa = false;   // lane VKSHARE1 (k = -1): the temporal AA's passes on the view (taaWork); shares tone's counters
	bool merge = false; // lane VKPOST2 (k = -1): the deferred tone pass, then the TAA, one crossing (mergeWork)
	bool sky = false;   // lane VKSKY1 (k = -1): the cell sky into the tone frame (skyWork); shares tone's counters
};

// round 11: the tone pass's record + submit, on the worker (the queue is the worker's)
bool toneWork( std::string & why );
bool taaWork( std::string & why );
bool mergeWork( std::string & why );
bool skyWork( std::string & why );
void skyDrop();

struct Worker
{
	std::mutex m;
	std::condition_variable cv, idle;
	std::deque<Work> q;
	bool started = false, busy = false, rebuild = false;
	bool inFlight[kSlots] = {};  // queued or running: its vkDone signal is not submitted yet
	WwVkPresent::Split split;
	bool bindOk = false, lastOk = true;
	std::string bindWhy, lastWhy;
	std::uint64_t toneDone = 0;  // round 11: tone jobs the worker finished (submitted, or failed: toneOk)
	bool toneOk = true;
	std::string toneWhy;
};

Worker & wk()
{
	static Worker * w = new Worker;  // never destroyed: the detached thread waits on it until the process ends
	return *w;
}

// the worker's GL context: current on the worker thread from its bind job to its release job
struct SigGl
{
	HGLRC ctx = nullptr;
	void( WINAPI * waitSync )( void *, GLbitfield, GLuint64 ) = nullptr;
	void( WINAPI * deleteSync )( void * ) = nullptr;
	GLenum( WINAPI * clientWaitSync )( void *, GLbitfield, GLuint64 ) = nullptr;
	void( WINAPI * signalSemaphore )( GLuint, GLuint, const GLuint *, GLuint, const GLuint *, const GLenum * ) = nullptr;
};

bool sigBind( SigGl & sg, HDC dc, HGLRC ctx, std::string & why )
{
	if ( !wglMakeCurrent( dc, ctx ) ) {
		why = "wglMakeCurrent on the worker failed (error " + std::to_string( GetLastError() ) + ")";
		sg = SigGl();
		return false;
	}
	sg.ctx = ctx;
	sg.waitSync = reinterpret_cast<decltype( sg.waitSync )>( wglGetProcAddress( "glWaitSync" ) );
	sg.deleteSync = reinterpret_cast<decltype( sg.deleteSync )>( wglGetProcAddress( "glDeleteSync" ) );
	sg.clientWaitSync = reinterpret_cast<decltype( sg.clientWaitSync )>( wglGetProcAddress( "glClientWaitSync" ) );
	sg.signalSemaphore = reinterpret_cast<decltype( sg.signalSemaphore )>( wglGetProcAddress( "glSignalSemaphoreEXT" ) );
	if ( sg.waitSync && sg.deleteSync && sg.clientWaitSync && sg.signalSemaphore )
		return true;
	why = "no glWaitSync / glClientWaitSync / glSignalSemaphoreEXT in the worker's GL context";
	wglMakeCurrent( nullptr, nullptr );
	sg = SigGl();
	return false;
}

void workerLoop()
{
	Worker & W = wk();
	SigGl sg;
	for ( ;; ) {
		Work j;
		{
			std::unique_lock<std::mutex> lk( W.m );
			W.cv.wait( lk, [&W]() { return !W.q.empty(); } );
			j = W.q.front();
			W.q.pop_front();
			W.busy = true;
		}
		if ( j.tone || j.taa || j.merge || j.sky ) {
			std::string why;
			const bool ok = j.merge ? mergeWork( why ) : j.sky ? skyWork( why ) : j.taa ? taaWork( why ) : toneWork( why );
			{
				std::lock_guard<std::mutex> lk( W.m );
				W.toneDone++;
				W.toneOk = ok;
				W.toneWhy = why;
				W.busy = false;
			}
			W.idle.notify_all();
			continue;
		}
		if ( j.k < 0 ) {
			std::string why;
			bool ok = true;
			if ( j.ctx )
				ok = sigBind( sg, j.dc, j.ctx, why );
			else if ( sg.ctx ) {
				wglMakeCurrent( nullptr, nullptr );
				sg = SigGl();
			}
			{
				std::lock_guard<std::mutex> lk( W.m );
				W.bindOk = ok;
				W.bindWhy = why;
				W.busy = false;
			}
			W.idle.notify_all();
			continue;
		}
		const double t0 = nowMs();
		std::string why;
		bool rebuild = false, ok = false;
		WwVkPresent::Split one;
		if ( j.sync && !sg.ctx ) {
			why = "the worker has no GL context: GL's signal was not sent";
			rebuild = true;
		} else {
			if ( j.sync && j.cpuWait ) {  // build 91: GL's frame done, seen on the CPU; nothing goes to the GPU from here
				const double ts = nowMs();
				const GLenum r = sg.clientWaitSync( j.sync, 0, 1000000000ull );
				sg.deleteSync( j.sync );
				if ( r != kAlreadySignaled && r != kConditionSatisfied )
					why = "glClientWaitSync returned " + std::to_string( r ) + " (the frame went on)";
				one.signalMs = nowMs() - ts;
			} else if ( j.sync ) {  // build 88: GL's frame fence (a GPU wait, in order), then the signal Vulkan's submit waits
				const double ts = nowMs();
				const GLenum general = kLayoutGeneral;
				sg.waitSync( j.sync, 0, kTimeoutIgnored );
				sg.deleteSync( j.sync );
				sg.signalSemaphore( j.sem, 0, nullptr, 1, &j.tex, &general );
				one.signalMs = nowMs() - ts;
			}
			ok = vkPart( j.k, j.w, j.h, j.vs, why, rebuild, one );
		}
		const double ms = nowMs() - t0;
		{
			std::lock_guard<std::mutex> lk( W.m );
			WwVkPresent::Split & s = W.split;
			s.n++;
			s.signalMs += one.signalMs;
			s.fenceMs += one.fenceMs;
			s.acquireMs += one.acquireMs;
			s.recordMs += one.recordMs;
			s.submitMs += one.submitMs;
			s.presentMs += one.presentMs;
			s.totalMs += ms;
			s.maxMs = std::max( s.maxMs, ms );
			if ( !ok ) {
				s.failed++;
				s.why = why;
			}
			W.rebuild = W.rebuild || rebuild;
			W.lastOk = ok;
			W.lastWhy = why;
			W.busy = false;
			W.inFlight[j.k] = false;
		}
		W.idle.notify_all();
	}
}

void hand( const Work & j )
{
	Worker & W = wk();
	{
		std::lock_guard<std::mutex> lk( W.m );
		if ( !W.started ) {
			W.started = true;
			std::thread( workerLoop ).detach();
		}
		if ( j.k >= 0 )
			W.inFlight[j.k] = true;
		W.q.push_back( j );
	}
	W.cv.notify_one();
}

void waitSlot( int k )
{
	Worker & W = wk();
	std::unique_lock<std::mutex> lk( W.m );
	W.idle.wait( lk, [&W, k]() { return !W.inFlight[k]; } );
}

void drain()
{
	Worker & W = wk();
	std::unique_lock<std::mutex> lk( W.m );
	W.idle.wait( lk, [&W]() { return W.q.empty() && !W.busy; } );
}

bool takeRebuild()
{
	Worker & W = wk();
	std::lock_guard<std::mutex> lk( W.m );
	const bool r = W.rebuild;
	W.rebuild = false;
	return r;
}

WwVkPresent::Split takeWorkerSplit()
{
	Worker & W = wk();
	std::lock_guard<std::mutex> lk( W.m );
	const WwVkPresent::Split s = W.split;
	W.split = WwVkPresent::Split();
	return s;
}

void dropSlots( bool glAlive )
{
	Vk & v = vk();
	Chain & c = ch();
	Gl & g = gl();
	for ( Slot & s : c.slot ) {
		if ( s.pending )
			v.vkWaitForFences( v.dev, 1, &s.fence, VK_TRUE, 1000000000ull );
		if ( glAlive ) {
			if ( s.fbo )
				g.deleteFramebuffers( 1, &s.fbo );
			if ( s.tex )
				g.deleteTextures( 1, &s.tex );
			if ( s.memObj )
				g.deleteMemoryObjects( 1, &s.memObj );
			if ( s.glDoneGl )
				g.deleteSemaphores( 1, &s.glDoneGl );
			if ( s.vkDoneGl )
				g.deleteSemaphores( 1, &s.vkDoneGl );
		}
		for ( VkSemaphore * x : { &s.glDone, &s.vkDone, &s.acquired } )
			if ( *x )
				v.vkDestroySemaphore( v.dev, *x, nullptr );
		if ( s.fence )
			v.vkDestroyFence( v.dev, s.fence, nullptr );
		if ( s.cmd )
			v.vkFreeCommandBuffers( v.dev, v.pool, 1, &s.cmd );
		if ( s.img )
			v.vkDestroyImage( v.dev, s.img, nullptr );
		if ( s.mem )
			v.vkFreeMemory( v.dev, s.mem, nullptr );
		s = Slot();
	}
}

void dropSwap( bool keepOld, VkSwapchainKHR * old )
{
	Vk & v = vk();
	Chain & c = ch();
	for ( VkSemaphore s : c.renderDone )
		v.vkDestroySemaphore( v.dev, s, nullptr );
	c.renderDone.clear();
	c.images.clear();
	if ( keepOld && old )
		*old = c.swap;
	else if ( c.swap )
		v.vkDestroySwapchainKHR( v.dev, c.swap, nullptr );
	c.swap = VK_NULL_HANDLE;
}

// the worker's GL context goes (the worker lets it go first: a context current on another thread is not deleted)
void dropSig( Chain & c )
{
	if ( c.sigCtx ) {
		Work j;
		j.k = -1;
		hand( j );
		drain();
		wglDeleteContext( c.sigCtx );
	}
	if ( c.sigDc )
		ReleaseDC( c.sigWnd, c.sigDc );
	if ( c.sigWnd )
		DestroyWindow( c.sigWnd );
	c.sigCtx = nullptr;
	c.sigDc = nullptr;
	c.sigWnd = nullptr;
}

// GL current: the worker's GL context, sharing GL's (the slot textures, the semaphores, the fence), bound once and checked
bool makeSig( Chain & c, std::string & why )
{
	HDC mdc = wglGetCurrentDC();
	HGLRC mctx = wglGetCurrentContext();
	const int pf = mdc ? GetPixelFormat( mdc ) : 0;
	if ( !pf || !mctx ) {
		why = "GL's window has no pixel format";
		return false;
	}
	PIXELFORMATDESCRIPTOR pfd {};
	DescribePixelFormat( mdc, pf, sizeof( pfd ), &pfd );
	static const bool registered = []() {
		WNDCLASSA wc {};
		wc.style = CS_OWNDC;
		wc.lpfnWndProc = DefWindowProcA;
		wc.hInstance = GetModuleHandleA( nullptr );
		wc.lpszClassName = "WwVkPresentSignal";
		return RegisterClassA( &wc ) != 0;
	}();
	( void ) registered;
	c.sigWnd = CreateWindowExA( WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW, "WwVkPresentSignal", "", WS_POPUP, 0, 0, 1, 1, nullptr,
		nullptr, GetModuleHandleA( nullptr ), nullptr );
	c.sigDc = c.sigWnd ? GetDC( c.sigWnd ) : nullptr;
	if ( !c.sigDc || !SetPixelFormat( c.sigDc, pf, &pfd ) ) {
		why = "the signal window took no pixel format (error " + std::to_string( GetLastError() ) + ")";
		dropSig( c );
		return false;
	}
	using CreateCtx = HGLRC( WINAPI * )( HDC, HGLRC, const int * );
	const auto create = reinterpret_cast<CreateCtx>( wglGetProcAddress( "wglCreateContextAttribsARB" ) );
	c.sigCtx = create ? create( c.sigDc, mctx, nullptr ) : nullptr;
	if ( !c.sigCtx ) {
		c.sigCtx = wglCreateContext( c.sigDc );
		if ( c.sigCtx && !wglShareLists( mctx, c.sigCtx ) ) {
			wglDeleteContext( c.sigCtx );
			c.sigCtx = nullptr;
		}
	}
	if ( !c.sigCtx ) {
		why = "no GL context sharing GL's objects (error " + std::to_string( GetLastError() ) + ")";
		dropSig( c );
		return false;
	}
	Work j;
	j.k = -1;
	j.dc = c.sigDc;
	j.ctx = c.sigCtx;
	hand( j );
	drain();
	bool ok = false;
	{
		Worker & W = wk();
		std::lock_guard<std::mutex> lk( W.m );
		ok = W.bindOk;
		why = W.bindWhy;
	}
	if ( !ok )
		dropSig( c );
	return ok;
}

void dropAll( bool glAlive )
{
	Vk & v = vk();
	Chain & c = ch();
	drain();
	dropSig( c );
	if ( v.ok ) {
		v.vkDeviceWaitIdle( v.dev );
		dropSlots( glAlive );
		dropSwap( false, nullptr );
		if ( c.surface )
			v.vkDestroySurfaceKHR( v.inst, c.surface, nullptr );
	}
	if ( c.child )
		DestroyWindow( c.child );
	c = Chain();
}

bool exportSem( VkSemaphore & s, GLuint & gs, std::string & why )
{
	Vk & v = vk();
	Gl & g = gl();
	VkExportSemaphoreCreateInfo ex { VK_STRUCTURE_TYPE_EXPORT_SEMAPHORE_CREATE_INFO };
	ex.handleTypes = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_WIN32_BIT;
	VkSemaphoreCreateInfo si { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
	si.pNext = &ex;
	if ( v.vkCreateSemaphore( v.dev, &si, nullptr, &s ) != VK_SUCCESS ) {
		why = "vkCreateSemaphore (exportable) failed";
		return false;
	}
	VkSemaphoreGetWin32HandleInfoKHR gi { VK_STRUCTURE_TYPE_SEMAPHORE_GET_WIN32_HANDLE_INFO_KHR };
	gi.semaphore = s;
	gi.handleType = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_WIN32_BIT;
	HANDLE hd = nullptr;
	if ( v.vkGetSemaphoreWin32HandleKHR( v.dev, &gi, &hd ) != VK_SUCCESS || !hd ) {
		why = "vkGetSemaphoreWin32HandleKHR failed";
		return false;
	}
	g.genSemaphores( 1, &gs );
	while ( g.getError() != 0 ) {}
	g.importSemaphoreWin32Handle( gs, kHandleOpaqueWin32, hd );
	const GLenum err = g.getError();	// before the close (see makeSlot)
	CloseHandle( hd );
	if ( err != 0 ) {
		why = "the GL import of a semaphore failed (GL error " + std::to_string( err ) + ")";
		return false;
	}
	return true;
}

bool oneShot( VkCommandBuffer cmd, VkFence fence )
{
	Vk & v = vk();
	VkSubmitInfo si { VK_STRUCTURE_TYPE_SUBMIT_INFO };
	si.commandBufferCount = 1;
	si.pCommandBuffers = &cmd;
	if ( v.vkQueueSubmit( v.queue, 1, &si, fence ) != VK_SUCCESS )
		return false;
	return v.vkWaitForFences( v.dev, 1, &fence, VK_TRUE, 1000000000ull ) == VK_SUCCESS && v.vkResetFences( v.dev, 1, &fence ) == VK_SUCCESS;
}

// a ring slot: an RGBA8 image GL imported (its frame lands there), its two semaphores, its fence and command buffer
bool makeSlot( Slot & s, int w, int h, std::string & why )
{
	Vk & v = vk();
	Gl & g = gl();
	VkExternalMemoryImageCreateInfo ext { VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO };
	ext.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT;
	VkImageCreateInfo ci { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
	ci.pNext = &ext;
	ci.imageType = VK_IMAGE_TYPE_2D;
	ci.format = VK_FORMAT_R8G8B8A8_UNORM;
	ci.extent = { std::uint32_t( w ), std::uint32_t( h ), 1 };
	ci.mipLevels = 1;
	ci.arrayLayers = 1;
	ci.samples = VK_SAMPLE_COUNT_1_BIT;
	ci.tiling = VK_IMAGE_TILING_OPTIMAL;
	// every use GL's texture could have (vkframe.cpp imgUsage's single-sample set): the import fails with fewer
	ci.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
		| VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT;
	if ( v.vkCreateImage( v.dev, &ci, nullptr, &s.img ) != VK_SUCCESS ) {
		why = "vkCreateImage (a ring image) failed";
		return false;
	}
	VkMemoryRequirements r;
	v.vkGetImageMemoryRequirements( v.dev, s.img, &r );
	VkMemoryDedicatedAllocateInfo ded { VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO };
	ded.image = s.img;
	VkExportMemoryAllocateInfo exp { VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO };
	exp.pNext = &ded;
	exp.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT;
	VkMemoryAllocateInfo ai { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
	ai.pNext = &exp;
	ai.allocationSize = r.size;
	ai.memoryTypeIndex = memType( r.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );
	if ( ai.memoryTypeIndex == UINT32_MAX || v.vkAllocateMemory( v.dev, &ai, nullptr, &s.mem ) != VK_SUCCESS
		|| v.vkBindImageMemory( v.dev, s.img, s.mem, 0 ) != VK_SUCCESS ) {
		why = "no exportable device memory for a ring image";
		return false;
	}
	VkSemaphoreCreateInfo si { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
	VkFenceCreateInfo fi { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
	VkCommandBufferAllocateInfo cai { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
	cai.commandPool = v.pool;
	cai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	cai.commandBufferCount = 1;
	if ( v.vkCreateSemaphore( v.dev, &si, nullptr, &s.acquired ) != VK_SUCCESS || v.vkCreateFence( v.dev, &fi, nullptr, &s.fence ) != VK_SUCCESS
		|| v.vkAllocateCommandBuffers( v.dev, &cai, &s.cmd ) != VK_SUCCESS ) {
		why = "a ring slot's semaphore / fence / command buffer failed";
		return false;
	}
	// GENERAL from the start (both sides keep it there: GL's semaphores name GL_LAYOUT_GENERAL_EXT)
	VkCommandBufferBeginInfo bi { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
	bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	v.vkBeginCommandBuffer( s.cmd, &bi );
	VkImageMemoryBarrier b { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
	b.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	b.newLayout = VK_IMAGE_LAYOUT_GENERAL;
	b.srcQueueFamilyIndex = b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	b.image = s.img;
	b.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
	v.vkCmdPipelineBarrier( s.cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 1, &b );
	v.vkEndCommandBuffer( s.cmd );
	if ( !oneShot( s.cmd, s.fence ) ) {
		why = "the ring image's first layout change failed";
		return false;
	}
	VkMemoryGetWin32HandleInfoKHR mi { VK_STRUCTURE_TYPE_MEMORY_GET_WIN32_HANDLE_INFO_KHR };
	mi.memory = s.mem;
	mi.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT;
	HANDLE mh = nullptr;
	if ( v.vkGetMemoryWin32HandleKHR( v.dev, &mi, &mh ) != VK_SUCCESS || !mh ) {
		why = "vkGetMemoryWin32HandleKHR failed";
		return false;
	}
	while ( g.getError() != 0 ) {}
	GLint was = 0;
	g.getIntegerv( kTexBinding2D, &was );
	g.createMemoryObjects( 1, &s.memObj );
	const GLint one = 1;
	g.memoryObjectParameteriv( s.memObj, kDedicated, &one );
	g.importMemoryWin32Handle( s.memObj, GLuint64( r.size ), kHandleOpaqueWin32, mh );
	// read GL's error BEFORE the handle closes: glGetError waits for the driver's thread, which imports the handle
	// later (closed first: GL_OUT_OF_MEMORY on every import, 5070 Ti 11-12)
	const GLenum errImport = g.getError();
	CloseHandle( mh );
	g.genTextures( 1, &s.tex );
	g.bindTexture( kTexture2D, s.tex );
	g.texStorageMem2D( kTexture2D, 1, kRgba8, w, h, s.memObj, 0 );
	const GLenum errStorage = g.getError();
	g.texParameteri( kTexture2D, kTexMinFilter, GLint( kNearest ) );
	g.texParameteri( kTexture2D, kTexMagFilter, GLint( kNearest ) );
	g.bindTexture( kTexture2D, GLuint( was ) );
	GLint rd = 0, dr = 0;
	g.getIntegerv( kReadFbBinding, &rd );
	g.getIntegerv( kDrawFbBinding, &dr );
	g.genFramebuffers( 1, &s.fbo );
	g.bindFramebuffer( kDrawFb, s.fbo );
	g.framebufferTexture2D( kDrawFb, kColorAttach0, kTexture2D, s.tex, 0 );
	const GLenum st = g.checkFramebufferStatus( kDrawFb );
	g.bindFramebuffer( kReadFb, GLuint( rd ) );
	g.bindFramebuffer( kDrawFb, GLuint( dr ) );
	const GLenum err = g.getError();
	if ( errImport || errStorage || err != 0 || st != kFbComplete ) {
		why = "the GL import of a ring image failed (GL error at the import " + std::to_string( errImport ) + ", the storage "
			+ std::to_string( errStorage ) + ", the framebuffer " + std::to_string( err ) + ", its status " + std::to_string( st ) + "; " + std::to_string( r.size ) + " bytes, memory type "
			+ std::to_string( ai.memoryTypeIndex ) + " of 0x" + std::to_string( r.memoryTypeBits ) + ")";
		return false;
	}
	return exportSem( s.glDone, s.glDoneGl, why ) && exportSem( s.vkDone, s.vkDoneGl, why );
}

/* round 10: one image the present device makes and GL imports (the view's colour or depth / stencil), GENERAL on both
 * sides; samples 1 = a plain 2D texture. The route's other shared targets (the LUT, TAA's history and vectors, the
 * overlay) take the same steps: this is the one place they are written. Lane VKSHARE1: type 3D (d deep) or a 2D array
 * (layers), single-sample, GL_TEXTURE_3D / GL_TEXTURE_2D_ARRAY through glTexStorageMem3DEXT; one mip level always. */
bool makeShared( VkImage & img, VkDeviceMemory & mem, GLuint & tex, GLuint & obj, VkFormat fmt, GLenum glFmt, int samples, int w,
	int h, bool depth, double & mb, std::string & why, VkImageType type = VK_IMAGE_TYPE_2D, int d = 1, int layers = 1 )
{
	Vk & v = vk();
	Gl & g = gl();
	const bool is3d = type == VK_IMAGE_TYPE_3D, layered = !is3d && layers > 1;
	if ( ( is3d || layered ) && ( samples != 1 || depth || !g.texStorageMem3D ) ) {
		why = !g.texStorageMem3D ? "no glTexStorageMem3DEXT (a 3D / layered shared target)" : "a 3D / layered shared target is single-sample colour only";
		return false;
	}
	d = is3d ? std::max( d, 1 ) : 1;
	layers = is3d ? 1 : std::max( layers, 1 );
	const VkSampleCountFlagBits sc = VkSampleCountFlagBits( samples );
	// every use GL's texture could have (the import fails with fewer); multisample storage is optional: never asked
	VkImageUsageFlags usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT
		| ( depth ? VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT : VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT );
	if ( samples == 1 && !depth )
		usage |= VK_IMAGE_USAGE_STORAGE_BIT;
	VkImageFormatProperties ip {};
	auto offered = [&]() {
		return v.vkGetPhysicalDeviceImageFormatProperties( v.phys, fmt, type, VK_IMAGE_TILING_OPTIMAL, usage, 0, &ip ) == VK_SUCCESS
			&& ( ip.sampleCounts & sc ) && ip.maxExtent.width >= std::uint32_t( w ) && ip.maxExtent.height >= std::uint32_t( h )
			&& ip.maxExtent.depth >= std::uint32_t( d ) && ip.maxArrayLayers >= std::uint32_t( layers );
	};
	bool ok = offered();
	if ( !ok && ( usage & VK_IMAGE_USAGE_STORAGE_BIT ) ) {  // a format without storage (R8 / RG16F on some devices): sampled only
		usage &= ~VkImageUsageFlags( VK_IMAGE_USAGE_STORAGE_BIT );
		ok = offered();
	}
	if ( !ok ) {
		why = std::string( depth ? "depth / stencil" : "colour" ) + " format " + std::to_string( int( fmt ) ) + " at " + std::to_string( samples )
			+ " samples (" + ( is3d ? "3D" : layered ? "2D array" : "2D" ) + ") is not offered by the device";
		return false;
	}
	VkExternalMemoryImageCreateInfo ext { VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO };
	ext.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT;
	VkImageCreateInfo ci { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
	ci.pNext = &ext;
	ci.imageType = type;
	ci.format = fmt;
	ci.extent = { std::uint32_t( w ), std::uint32_t( h ), std::uint32_t( d ) };
	ci.mipLevels = 1;
	ci.arrayLayers = std::uint32_t( layers );
	ci.samples = sc;
	ci.tiling = VK_IMAGE_TILING_OPTIMAL;
	ci.usage = usage;
	if ( v.vkCreateImage( v.dev, &ci, nullptr, &img ) != VK_SUCCESS ) {
		why = "vkCreateImage (a view image) failed";
		return false;
	}
	VkMemoryRequirements r;
	v.vkGetImageMemoryRequirements( v.dev, img, &r );
	VkMemoryDedicatedAllocateInfo ded { VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO };
	ded.image = img;
	VkExportMemoryAllocateInfo exp { VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO };
	exp.pNext = &ded;
	exp.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT;
	VkMemoryAllocateInfo ai { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
	ai.pNext = &exp;
	ai.allocationSize = r.size;
	ai.memoryTypeIndex = memType( r.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );
	if ( ai.memoryTypeIndex == UINT32_MAX || v.vkAllocateMemory( v.dev, &ai, nullptr, &mem ) != VK_SUCCESS
		|| v.vkBindImageMemory( v.dev, img, mem, 0 ) != VK_SUCCESS ) {
		why = "no exportable device memory for a view image";
		return false;
	}
	mb += double( r.size ) / 1048576.0;
	// UNDEFINED -> GENERAL once, in a command buffer of its own
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
		VkImageMemoryBarrier b { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
		b.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		b.newLayout = VK_IMAGE_LAYOUT_GENERAL;
		b.srcQueueFamilyIndex = b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		b.image = img;
		b.subresourceRange = { VkImageAspectFlags( depth ? VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT : VK_IMAGE_ASPECT_COLOR_BIT ),
			0, 1, 0, std::uint32_t( layers ) };
		v.vkCmdPipelineBarrier( cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 1, &b );
		v.vkEndCommandBuffer( cmd );
		laid = oneShot( cmd, fence );
	}
	if ( cmd )
		v.vkFreeCommandBuffers( v.dev, v.pool, 1, &cmd );
	if ( fence )
		v.vkDestroyFence( v.dev, fence, nullptr );
	if ( !laid ) {
		why = "a view image's first layout change failed";
		return false;
	}
	VkMemoryGetWin32HandleInfoKHR mi { VK_STRUCTURE_TYPE_MEMORY_GET_WIN32_HANDLE_INFO_KHR };
	mi.memory = mem;
	mi.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT;
	HANDLE mh = nullptr;
	if ( v.vkGetMemoryWin32HandleKHR( v.dev, &mi, &mh ) != VK_SUCCESS || !mh ) {
		why = "vkGetMemoryWin32HandleKHR (a view image) failed";
		return false;
	}
	const GLenum target = is3d ? kTexture3D : layered ? kTexture2DArray : samples > 1 ? kTexture2DMs : kTexture2D;
	while ( g.getError() != 0 ) {}
	GLint was = 0;
	g.getIntegerv( is3d ? kTexBinding3D : layered ? kTexBinding2DArray : samples > 1 ? kTexBinding2DMs : kTexBinding2D, &was );
	g.createMemoryObjects( 1, &obj );
	const GLint one = 1;
	g.memoryObjectParameteriv( obj, kDedicated, &one );
	g.importMemoryWin32Handle( obj, GLuint64( r.size ), kHandleOpaqueWin32, mh );
	const GLenum errImport = g.getError();  // before the close (makeSlot)
	CloseHandle( mh );
	g.genTextures( 1, &tex );
	g.bindTexture( target, tex );
	if ( samples > 1 )
		g.texStorageMem2DMultisample( target, samples, glFmt, w, h, 1, obj, 0 );
	else {
		if ( is3d || layered )
			g.texStorageMem3D( target, 1, glFmt, w, h, is3d ? d : layers, obj, 0 );
		else
			g.texStorageMem2D( target, 1, glFmt, w, h, obj, 0 );
		g.texParameteri( target, kTexMinFilter, GLint( kNearest ) );
		g.texParameteri( target, kTexMagFilter, GLint( kNearest ) );
	}
	const GLenum errStorage = g.getError();
	g.bindTexture( target, GLuint( was ) );
	if ( errImport || errStorage ) {
		why = std::string( "the GL import of a view image failed (GL error at the import " ) + std::to_string( errImport )
			+ ", the storage " + std::to_string( errStorage ) + "; " + std::to_string( r.size ) + " bytes)";
		return false;
	}
	return true;
}

// round 11: the tone pass's framebuffers and image views of the view ring (defined with the tone pass)
void toneFbsDrop();
void taaFbsDrop();  // lane VKSHARE1: the temporal AA's copy-back framebuffers over the same ring

// the views go (GL alive: its textures, memory objects and FBO too); the caller drained the worker
/* lane VKSHARE1: the keyed single-sample targets other passes keep on the present device (sharedTarget): GL draws and
 * reads them by their GL names as before; a Vulkan pass finds the image by its key (sharedFind) */
struct Shared
{
	VkImage img = VK_NULL_HANDLE;
	VkDeviceMemory mem = VK_NULL_HANDLE;
	VkImageView view = VK_NULL_HANDLE;
	GLuint tex = 0, obj = 0;
	void * glCtx = nullptr;
	int type = 0, w = 0, h = 0, d = 0;
	GLenum glFmt = 0;
	VkFormat fmt = VK_FORMAT_UNDEFINED;
	double mb = 0.0;
};

std::map<std::uintptr_t, Shared> & shareds()
{
	static std::map<std::uintptr_t, Shared> m;
	return m;
}

WwVkPresent::SharedStat & sharedStat()
{
	static WwVkPresent::SharedStat s;
	return s;
}

VkFormat vkFormatOf( GLenum f, bool & depth )
{
	depth = f == kDepth24Stencil8;
	switch ( f ) {
	case kRgba16f: return VK_FORMAT_R16G16B16A16_SFLOAT;
	case kRgba32f: return VK_FORMAT_R32G32B32A32_SFLOAT;
	case kRgba8: return VK_FORMAT_R8G8B8A8_UNORM;
	case kGlRg32f: return VK_FORMAT_R32G32_SFLOAT;
	case kGlRg16f: return VK_FORMAT_R16G16_SFLOAT;
	case kGlR32f: return VK_FORMAT_R32_SFLOAT;
	case kGlR8: return VK_FORMAT_R8_UNORM;
	case kDepth24Stencil8: return VK_FORMAT_D24_UNORM_S8_UINT;
	default: return VK_FORMAT_UNDEFINED;
	}
}

// the caller drained the worker and waited the device idle
void sharedFree( Shared & s, bool glAlive )
{
	Vk & v = vk();
	if ( glAlive ) {
		if ( s.tex )
			gl().deleteTextures( 1, &s.tex );
		if ( s.obj )
			gl().deleteMemoryObjects( 1, &s.obj );
	}
	if ( s.view )
		v.vkDestroyImageView( v.dev, s.view, nullptr );
	if ( s.img )
		v.vkDestroyImage( v.dev, s.img, nullptr );
	if ( s.mem )
		v.vkFreeMemory( v.dev, s.mem, nullptr );
	s = Shared();
}

const Shared * sharedFind( std::uintptr_t key )
{
	auto it = shareds().find( key );
	return it != shareds().end() && it->second.glCtx == gl().ctx ? &it->second : nullptr;
}

void dropView( bool glAlive )
{
	Vk & v = vk();
	Gl & g = gl();
	View & V = view();
	toneFbsDrop();
	taaFbsDrop();
	for ( ViewSlot & s : V.slot ) {
		if ( glAlive ) {
			for ( GLuint * t : { &s.colorTex, &s.dsTex } )
				if ( *t )
					g.deleteTextures( 1, t );
			for ( GLuint * o : { &s.colorObj, &s.dsObj } )
				if ( *o )
					g.deleteMemoryObjects( 1, o );
		}
		for ( VkImage * i : { &s.color, &s.ds } )
			if ( *i )
				v.vkDestroyImage( v.dev, *i, nullptr );
		for ( VkDeviceMemory * m : { &s.colorMem, &s.dsMem } )
			if ( *m )
				v.vkFreeMemory( v.dev, *m, nullptr );
		s = ViewSlot();
	}
	if ( glAlive && V.fbo )
		g.deleteFramebuffers( 1, &V.fbo );
	V.fbo = 0;
	V.w = V.h = V.samples = 0;
	V.cur = -1;
	V.mb = 0.0;
	V.glCtx = nullptr;
}

// the ring of views at w x h x samples (the back buffer's shape), each slot's FBO completeness asked once here
bool makeView( int w, int h, int samples, std::string & why )
{
	Vk & v = vk();
	Gl & g = gl();
	View & V = view();
	const double t0 = nowMs();
	drain();
	v.vkDeviceWaitIdle( v.dev );
	dropView( V.glCtx == g.ctx );
	if ( samples > 1 && !g.texStorageMem2DMultisample ) {
		why = "no glTexStorageMem2DMultisampleEXT";
		return false;
	}
	V.glCtx = g.ctx;
	for ( ViewSlot & s : V.slot )
		if ( !makeShared( s.color, s.colorMem, s.colorTex, s.colorObj, VK_FORMAT_R8G8B8A8_UNORM, kRgba8, samples, w, h, false, V.mb, why )
			|| !makeShared( s.ds, s.dsMem, s.dsTex, s.dsObj, VK_FORMAT_D24_UNORM_S8_UINT, kDepth24Stencil8, samples, w, h, true, V.mb, why ) ) {
			dropView( true );
			return false;
		}
	GLint rd = 0, dr = 0;
	g.getIntegerv( kReadFbBinding, &rd );
	g.getIntegerv( kDrawFbBinding, &dr );
	g.genFramebuffers( 1, &V.fbo );
	g.bindFramebuffer( kFramebuffer, V.fbo );
	const GLenum target = samples > 1 ? kTexture2DMs : kTexture2D;
	GLenum st = kFbComplete;
	for ( ViewSlot & s : V.slot ) {
		g.framebufferTexture2D( kFramebuffer, kColorAttach0, target, s.colorTex, 0 );
		g.framebufferTexture2D( kFramebuffer, kDepthStencilAttach, target, s.dsTex, 0 );
		const GLenum one = g.checkFramebufferStatus( kFramebuffer );
		if ( one != kFbComplete )
			st = one;
	}
	g.bindFramebuffer( kReadFb, GLuint( rd ) );
	g.bindFramebuffer( kDrawFb, GLuint( dr ) );
	const GLenum err = g.getError();
	if ( st != kFbComplete || err != 0 ) {
		why = "the view's framebuffer is not complete (status " + std::to_string( st ) + ", GL error " + std::to_string( err ) + ")";
		dropView( true );
		return false;
	}
	V.w = w;
	V.h = h;
	V.samples = samples;
	V.makeMs = nowMs() - t0;
	V.gen++;
	return true;
}

/* Round 11, stage 1: the cell tone map (cell_hdr.frag modes 0, 2, 1) as a render pass of the present device into the
 * view slot GL draws this frame. GL draws the linear frame into images made here (toneFrame: RGBA16F + D24S8 at the
 * view's samples). The crossing: GL signals A (the frame, the view slot, the measure), the worker's submit waits A and
 * signals B, GL waits B before its next pass on the view; the drawing thread waits the worker's submit first (GL may
 * wait a binary semaphore only after its signal is submitted). The LUT goes up from the CPU bytes GL uploads (16^3,
 * 16 KB), not through a GL import; the fog bloom stays GL's. Lane VKBLOOM1: the Fusion Shaders bloom (its chain as
 * compute dispatches, then mode 3 over the backdrop) and the exposure measure run in the same command buffer. */
// lane VKBLOOM1: one image of the bloom chain (device-local, or shared when GL reads it: the combine, the measure)
struct BloomImg
{
	VkImage img = VK_NULL_HANDLE;
	VkDeviceMemory mem = VK_NULL_HANDLE;
	VkImageView view = VK_NULL_HANDLE;
	VkImageView layer[8] = {};  // the ring's layers, each a 2D storage view
	GLuint tex = 0, obj = 0;
	int w = 0, h = 0, layers = 1, samples = 1;
};
struct ToneBloomState
{
	bool made = false, broken = false;
	std::string brokenWhy;
	VkDescriptorSetLayout dsl = VK_NULL_HANDLE;
	VkPipelineLayout layout = VK_NULL_HANDLE;
	VkPipeline pipe[7] = {};  // down, hold, up, src, comb, measure_q (vkframe.cpp's order), back (the backdrop's resolve)
	// per chain shape (W x H, the mips' dims); the frame's views are in the sets: made again with the frame
	int W = 0, H = 0, mips = 0, dims[WwVkPresent::kToneBloomMips][2] = {};
	BloomImg src, ring, comb, mip[WwVkPresent::kToneBloomMips], up[WwVkPresent::kToneBloomMips];
	VkDescriptorPool pool = VK_NULL_HANDLE;
	VkDescriptorSet down[WwVkPresent::kToneBloomMips] = {}, downRing[8] = {}, hold = VK_NULL_HANDLE,
		upSet[WwVkPresent::kToneBloomMips] = {}, srcSet = VK_NULL_HANDLE, combSet = VK_NULL_HANDLE;
	VkImage setFrame = VK_NULL_HANDLE;  // the frame image the sets hold
	bool fresh = false;   // made this run: the layouts change in the pass, the hold starts again
	double mb = 0.0;
	// the measure's quarter target (shared: GL blurs it) and its host read buffer
	BloomImg meas;
	BloomImg measFill;  // 1 x 1, 2 layers: the measure set's unused bindings (the measure runs without the chain)
	VkDescriptorPool measPool = VK_NULL_HANDLE;
	VkDescriptorSet measSet = VK_NULL_HANDLE;
	VkBuffer read = VK_NULL_HANDLE;
	VkDeviceMemory readMem = VK_NULL_HANDLE;
	float * readPtr = nullptr;
	bool measFresh = false;
	double measMb = 0.0;
	/* the backdrop mode 3 reads: the view slot copied (backMs: RGBA8, its samples), resolved by bloom_back.hlsl into back
	 * (RGBA16F, as GL's blit resolve keeps the mean); backFill: its set's unused bindings */
	BloomImg back, backMs, backFill;
	VkDescriptorPool backPool = VK_NULL_HANDLE;
	VkDescriptorSet backSet = VK_NULL_HANDLE;
	double backMb = 0.0;
	// this frame's draw (the drawing thread writes it before the hand, the worker reads it)
	bool on = false, backdrop = false;
	int measQw = 0, measQh = 0;
	WwVkPresent::ToneBloom job;
	std::uint64_t runs = 0, records = 0, measRecord = 0;
	bool measRan = false, measHave = false, measDone = false;
	int measW = 0, measH = 0;
};
struct Tone
{
	void * glCtx = nullptr;
	int w = 0, h = 0, samples = 0;
	VkImage color = VK_NULL_HANDLE, ds = VK_NULL_HANDLE;
	VkDeviceMemory colorMem = VK_NULL_HANDLE, dsMem = VK_NULL_HANDLE;
	GLuint colorTex = 0, dsTex = 0, colorObj = 0, dsObj = 0;
	VkImageView colorView = VK_NULL_HANDLE, stencilView = VK_NULL_HANDLE;
	double frameMb = 0.0;
	// the measure's bloom, copied by GL into an image of this device (RGBA32F, 1 sample) when it changes
	int mw = 0, mh = 0;
	VkImage measure = VK_NULL_HANDLE;
	VkDeviceMemory measureMem = VK_NULL_HANDLE;
	GLuint measureTex = 0, measureObj = 0, copyFb[2] = {};
	GLuint measureSrc = 0;  // GL's bloom texture attached to copyFb[0] (its completeness asked once per name)
	VkImageView measureView = VK_NULL_HANDLE;
	double measureMb = 0.0;
	std::uint64_t measureGen = 0;
	bool measureHave = false;
	// made once on the device
	bool made = false, broken = false;
	std::string brokenWhy;
	VkSampler sampler = VK_NULL_HANDLE;
	VkDescriptorSetLayout dsl = VK_NULL_HANDLE;
	VkDescriptorPool dpool = VK_NULL_HANDLE;
	VkDescriptorSet set = VK_NULL_HANDLE;
	VkPipelineLayout layout = VK_NULL_HANDLE;
	VkShaderModule vs = VK_NULL_HANDLE, ps = VK_NULL_HANDLE;
	VkImage dummy = VK_NULL_HANDLE, lut = VK_NULL_HANDLE;
	VkDeviceMemory dummyMem = VK_NULL_HANDLE, lutMem = VK_NULL_HANDLE, stagingMem = VK_NULL_HANDLE;
	VkImageView dummyView = VK_NULL_HANDLE, lutView = VK_NULL_HANDLE;
	VkBuffer staging = VK_NULL_HANDLE;
	std::uint8_t * stagingPtr = nullptr;
	double fixedMb = 0.0;
	std::uint64_t lutGen = 0;
	bool lutCopy = false;  // the worker copies the staging buffer into the LUT before the pass
	bool lutHave = false;
	VkSemaphore glDone = VK_NULL_HANDLE, vkDone = VK_NULL_HANDLE;
	GLuint glDoneGl = 0, vkDoneGl = 0;
	void * semCtx = nullptr;  // the GL context the two semaphores were imported into
	VkCommandBuffer cmd = VK_NULL_HANDLE;
	VkFence fence = VK_NULL_HANDLE;
	bool pending = false;  // the fence is armed (the last pass may still run)
	std::uint64_t handed = 0;
	VkPipelineCache cache = VK_NULL_HANDLE;
	std::string cachePath;
	// per sample count
	int pipeSamples = 0;
	VkRenderPass pass = VK_NULL_HANDLE;
	// 0: no depth test; 1: GREATER (an opaque draw's depth); 2: LEQUAL + premultiplied blend; lane VKBLOOM1, mode 3's
	// glow added (ONE, ONE): 3 no depth test, 4 LEQUAL
	VkPipeline pipe[5] = {};
	int pipelines = 0;
	double buildMs = 0.0;
	// per view slot (made again with the view ring)
	VkImageView vColor[kViewSlots] = {}, vDs[kViewSlots] = {};
	VkFramebuffer fb[kViewSlots] = {};
	std::uint64_t fbGen = 0;
	bool descDirty = true;
	// this frame's draw: written by the drawing thread before the hand, read by the worker
	int slot = -1;
	int vp[4] = {};
	bool splitSky = false, sorted = true;
	WwVkPresent::ToneJob job;
	WwVkPresent::ToneStat st;  // since the last take
	/* lane VKPOST2: the merged crossing. A deferred pass is recorded state only: GL's signal goes with the TAA's (one
	 * crossing), or toneFlush sends it alone when a GL pass needs the view first. mergeHint: the last TAA ran on Vulkan
	 * right after a tone pass; backoff: frames without deferring after a flush; red WW_VKVIEW_MERGE_RED=1 never defers */
	bool deferred = false, ranSinceTaa = false, mergeHint = false, mergeRed = false, subOk = false;
	int backoff = 0;
	GLuint defTex[8] = {};  // lane VKBLOOM1: + the combine and the measure's quarter target
	GLuint defN = 0;
	ToneBloomState bloom;  // lane VKBLOOM1
};

Tone & tone()
{
	static Tone t;
	return t;
}

VkImageView imageView( VkImage img, VkImageViewType type, VkFormat fmt, VkImageAspectFlags aspect )
{
	Vk & v = vk();
	VkImageViewCreateInfo ci { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
	ci.image = img;
	ci.viewType = type;
	ci.format = fmt;
	ci.subresourceRange = { aspect, 0, 1, 0, 1 };
	VkImageView out = VK_NULL_HANDLE;
	if ( v.vkCreateImageView( v.dev, &ci, nullptr, &out ) != VK_SUCCESS )
		return VK_NULL_HANDLE;
	return out;
}

// an image only this device reads (the LUT, the dummy), device-local
bool makeLocal( VkImage & img, VkDeviceMemory & mem, VkImageType type, VkFormat fmt, int w, int h, int d, double & mb )
{
	Vk & v = vk();
	VkImageCreateInfo ci { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
	ci.imageType = type;
	ci.format = fmt;
	ci.extent = { std::uint32_t( w ), std::uint32_t( h ), std::uint32_t( d ) };
	ci.mipLevels = 1;
	ci.arrayLayers = 1;
	ci.samples = VK_SAMPLE_COUNT_1_BIT;
	ci.tiling = VK_IMAGE_TILING_OPTIMAL;
	ci.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	if ( v.vkCreateImage( v.dev, &ci, nullptr, &img ) != VK_SUCCESS )
		return false;
	VkMemoryRequirements r;
	v.vkGetImageMemoryRequirements( v.dev, img, &r );
	VkMemoryAllocateInfo ai { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
	ai.allocationSize = r.size;
	ai.memoryTypeIndex = memType( r.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );
	if ( ai.memoryTypeIndex == UINT32_MAX || v.vkAllocateMemory( v.dev, &ai, nullptr, &mem ) != VK_SUCCESS
		|| v.vkBindImageMemory( v.dev, img, mem, 0 ) != VK_SUCCESS )
		return false;
	mb += double( r.size ) / 1048576.0;
	return true;
}

void toneFbsDrop()
{
	Vk & v = vk();
	Tone & T = tone();
	for ( int k = 0; k < kViewSlots; k++ ) {
		if ( T.fb[k] )
			v.vkDestroyFramebuffer( v.dev, T.fb[k], nullptr );
		for ( VkImageView * iv : { &T.vColor[k], &T.vDs[k] } )
			if ( *iv )
				v.vkDestroyImageView( v.dev, *iv, nullptr );
		T.fb[k] = VK_NULL_HANDLE;
		T.vColor[k] = T.vDs[k] = VK_NULL_HANDLE;
	}
	T.fbGen = 0;
}

void tonePipesDrop()
{
	Vk & v = vk();
	Tone & T = tone();
	toneFbsDrop();
	for ( VkPipeline & p : T.pipe ) {
		if ( p )
			v.vkDestroyPipeline( v.dev, p, nullptr );
		p = VK_NULL_HANDLE;
	}
	if ( T.pass )
		v.vkDestroyRenderPass( v.dev, T.pass, nullptr );
	T.pass = VK_NULL_HANDLE;
	T.pipeSamples = 0;
	T.pipelines = 0;
}

// the measure's copy goes (GL alive: its texture, memory object and copy FBOs too); the caller waited the device
void toneMeasureDrop( bool glAlive )
{
	Vk & v = vk();
	Gl & g = gl();
	Tone & T = tone();
	if ( glAlive ) {
		if ( T.measureTex )
			g.deleteTextures( 1, &T.measureTex );
		if ( T.measureObj )
			g.deleteMemoryObjects( 1, &T.measureObj );
		if ( T.copyFb[0] )
			g.deleteFramebuffers( 2, T.copyFb );
	}
	if ( T.measureView )
		v.vkDestroyImageView( v.dev, T.measureView, nullptr );
	if ( T.measure )
		v.vkDestroyImage( v.dev, T.measure, nullptr );
	if ( T.measureMem )
		v.vkFreeMemory( v.dev, T.measureMem, nullptr );
	T.measureTex = T.measureObj = T.copyFb[0] = T.copyFb[1] = T.measureSrc = 0;
	T.measureView = VK_NULL_HANDLE;
	T.measure = VK_NULL_HANDLE;
	T.measureMem = VK_NULL_HANDLE;
	T.mw = T.mh = 0;
	T.measureMb = 0.0;
	T.measureHave = false;
	T.descDirty = true;
}

void toneCacheSave();

// lane VKBLOOM1: one bloom image goes (GL alive: its texture and memory object too); the caller waited the device
void bloomImgDrop( BloomImg & I, bool glAlive )
{
	Vk & v = vk();
	Gl & g = gl();
	if ( glAlive ) {
		if ( I.tex )
			g.deleteTextures( 1, &I.tex );
		if ( I.obj )
			g.deleteMemoryObjects( 1, &I.obj );
	}
	for ( VkImageView & iv : I.layer )
		if ( iv )
			v.vkDestroyImageView( v.dev, iv, nullptr );
	if ( I.view )
		v.vkDestroyImageView( v.dev, I.view, nullptr );
	if ( I.img )
		v.vkDestroyImage( v.dev, I.img, nullptr );
	if ( I.mem )
		v.vkFreeMemory( v.dev, I.mem, nullptr );
	I = BloomImg();
}

VkImageView layerView( VkImage img, VkImageViewType type, VkFormat fmt, int base, int count )
{
	Vk & v = vk();
	VkImageViewCreateInfo ci { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
	ci.image = img;
	ci.viewType = type;
	ci.format = fmt;
	ci.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, std::uint32_t( base ), std::uint32_t( count ) };
	VkImageView out = VK_NULL_HANDLE;
	if ( v.vkCreateImageView( v.dev, &ci, nullptr, &out ) != VK_SUCCESS )
		return VK_NULL_HANDLE;
	return out;
}

/* lane VKBLOOM1: a chain image only this device reads (device-local; layers > 1: the ring, a 2D array view and a 2D view
 * per layer), left UNDEFINED (bloomGeneral lays it out) */
bool bloomLocal( BloomImg & I, VkFormat fmt, VkImageUsageFlags usage, int w, int h, int layers, double & mb, int samples = 1 )
{
	Vk & v = vk();
	VkImageCreateInfo ci { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
	ci.imageType = VK_IMAGE_TYPE_2D;
	ci.format = fmt;
	ci.extent = { std::uint32_t( w ), std::uint32_t( h ), 1 };
	ci.mipLevels = 1;
	ci.arrayLayers = std::uint32_t( layers );
	ci.samples = VkSampleCountFlagBits( samples );
	ci.tiling = VK_IMAGE_TILING_OPTIMAL;
	ci.usage = usage;
	if ( v.vkCreateImage( v.dev, &ci, nullptr, &I.img ) != VK_SUCCESS )
		return false;
	VkMemoryRequirements r;
	v.vkGetImageMemoryRequirements( v.dev, I.img, &r );
	VkMemoryAllocateInfo ai { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
	ai.allocationSize = r.size;
	ai.memoryTypeIndex = memType( r.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );
	if ( ai.memoryTypeIndex == UINT32_MAX || v.vkAllocateMemory( v.dev, &ai, nullptr, &I.mem ) != VK_SUCCESS
		|| v.vkBindImageMemory( v.dev, I.img, I.mem, 0 ) != VK_SUCCESS )
		return false;
	mb += double( r.size ) / 1048576.0;
	I.w = w;
	I.h = h;
	I.layers = layers;
	I.samples = samples;
	I.view = layerView( I.img, layers > 1 ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D, fmt, 0, layers );
	if ( !I.view )
		return false;
	for ( int l = 0; layers > 1 && l < layers && l < 8; l++ )
		if ( !( I.layer[l] = layerView( I.img, VK_IMAGE_VIEW_TYPE_2D, fmt, l, 1 ) ) )
			return false;
	return true;
}

// lane VKBLOOM1: the device-local chain images UNDEFINED -> GENERAL once (a submit of its own, waited: once per shape)
bool bloomGeneral( const std::vector<const BloomImg *> & imgs )
{
	Vk & v = vk();
	std::vector<VkImageMemoryBarrier> bars;
	for ( const BloomImg * I : imgs ) {
		if ( !I->img )
			continue;
		VkImageMemoryBarrier b { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
		b.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
		b.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		b.newLayout = VK_IMAGE_LAYOUT_GENERAL;
		b.srcQueueFamilyIndex = b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		b.image = I->img;
		b.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, std::uint32_t( I->layers ) };
		bars.push_back( b );
	}
	if ( bars.empty() )
		return true;
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
		v.vkCmdPipelineBarrier( cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr,
			std::uint32_t( bars.size() ), bars.data() );
		v.vkEndCommandBuffer( cmd );
		laid = oneShot( cmd, fence );
	}
	if ( cmd )
		v.vkFreeCommandBuffers( v.dev, v.pool, 1, &cmd );
	if ( fence )
		v.vkDestroyFence( v.dev, fence, nullptr );
	return laid;
}

// lane VKBLOOM1: the chain (and its sets), the measure's target and read buffer, the backdrop go; the caller waited the device
void toneBloomFree( bool glAlive )
{
	Vk & v = vk();
	Tone & T = tone();
	ToneBloomState & B = T.bloom;
	bloomImgDrop( B.src, glAlive );
	bloomImgDrop( B.ring, glAlive );
	bloomImgDrop( B.comb, glAlive );
	for ( int i = 0; i < WwVkPresent::kToneBloomMips; i++ ) {
		bloomImgDrop( B.mip[i], glAlive );
		bloomImgDrop( B.up[i], glAlive );
	}
	if ( B.pool )
		v.vkDestroyDescriptorPool( v.dev, B.pool, nullptr );
	B.pool = VK_NULL_HANDLE;
	B.W = B.H = B.mips = 0;
	B.setFrame = VK_NULL_HANDLE;
	B.mb = 0.0;
	bloomImgDrop( B.meas, glAlive );
	bloomImgDrop( B.measFill, glAlive );
	if ( B.measPool )
		v.vkDestroyDescriptorPool( v.dev, B.measPool, nullptr );
	B.measPool = VK_NULL_HANDLE;
	B.measSet = VK_NULL_HANDLE;
	if ( B.read ) {
		v.vkUnmapMemory( v.dev, B.readMem );
		v.vkDestroyBuffer( v.dev, B.read, nullptr );
	}
	if ( B.readMem )
		v.vkFreeMemory( v.dev, B.readMem, nullptr );
	B.read = VK_NULL_HANDLE;
	B.readMem = VK_NULL_HANDLE;
	B.readPtr = nullptr;
	B.measW = B.measH = 0;
	B.measHave = B.measRan = false;
	B.measMb = 0.0;
	bloomImgDrop( B.back, glAlive );
	bloomImgDrop( B.backMs, glAlive );
	bloomImgDrop( B.backFill, glAlive );
	if ( B.backPool )
		v.vkDestroyDescriptorPool( v.dev, B.backPool, nullptr );
	B.backPool = VK_NULL_HANDLE;
	B.backSet = VK_NULL_HANDLE;
	B.backMb = 0.0;
	B.on = B.backdrop = false;
	B.measQw = B.measQh = 0;
	T.descDirty = true;
}

// lane VKBLOOM1: the set layout (BloomBindingsVk.hlsli, 8 bindings), the pipeline layout and the six compute pipelines, once
bool toneBloomStatic( std::string & why )
{
	Vk & v = vk();
	Tone & T = tone();
	ToneBloomState & B = T.bloom;
	if ( B.made )
		return true;
	if ( !v.vkCmdDispatch || !v.vkCreateComputePipelines ) {
		why = "no vkCmdDispatch / vkCreateComputePipelines";
		return false;
	}
	const VkDescriptorType t[8] = { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
		VK_DESCRIPTOR_TYPE_SAMPLER, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
		VK_DESCRIPTOR_TYPE_STORAGE_IMAGE };
	VkDescriptorSetLayoutBinding b[8] = {};
	for ( std::uint32_t k = 0; k < 8; k++ )
		b[k] = VkDescriptorSetLayoutBinding { k, t[k], 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr };
	VkDescriptorSetLayoutCreateInfo lci { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
	lci.bindingCount = 8;
	lci.pBindings = b;
	VkPushConstantRange pcr { VK_SHADER_STAGE_COMPUTE_BIT, 0, 48 };  // vkframe.cpp's Job
	VkPipelineLayoutCreateInfo plci { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
	plci.setLayoutCount = 1;
	plci.pSetLayouts = &B.dsl;
	plci.pushConstantRangeCount = 1;
	plci.pPushConstantRanges = &pcr;
	if ( v.vkCreateDescriptorSetLayout( v.dev, &lci, nullptr, &B.dsl ) != VK_SUCCESS
		|| v.vkCreatePipelineLayout( v.dev, &plci, nullptr, &B.layout ) != VK_SUCCESS ) {
		why = "the bloom's set / pipeline layout failed";
		return false;
	}
	const std::uint32_t * code[7] = { vkframe_bloom_down_spv, vkframe_bloom_hold_spv, vkframe_bloom_up_spv, vkframe_bloom_src_spv,
		vkframe_bloom_comb_spv, vkframe_measure_q_spv, vkframe_bloom_back_spv };
	const size_t bytes[7] = { sizeof( vkframe_bloom_down_spv ), sizeof( vkframe_bloom_hold_spv ), sizeof( vkframe_bloom_up_spv ),
		sizeof( vkframe_bloom_src_spv ), sizeof( vkframe_bloom_comb_spv ), sizeof( vkframe_measure_q_spv ), sizeof( vkframe_bloom_back_spv ) };
	for ( int k = 0; k < 7; k++ ) {
		VkShaderModuleCreateInfo mi { VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
		mi.codeSize = bytes[k];
		mi.pCode = code[k];
		VkShaderModule m = VK_NULL_HANDLE;
		if ( v.vkCreateShaderModule( v.dev, &mi, nullptr, &m ) != VK_SUCCESS ) {
			why = "the bloom's shader module " + std::to_string( k ) + " failed";
			return false;
		}
		VkComputePipelineCreateInfo pc { VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO };
		pc.stage = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_COMPUTE_BIT, m, "main", nullptr };
		pc.layout = B.layout;
		const VkResult r = v.vkCreateComputePipelines( v.dev, T.cache, 1, &pc, nullptr, &B.pipe[k] );
		v.vkDestroyShaderModule( v.dev, m, nullptr );
		if ( r != VK_SUCCESS ) {
			why = "vkCreateComputePipelines (the bloom's kernel " + std::to_string( k ) + ") failed";
			return false;
		}
	}
	toneCacheSave();
	B.made = true;
	return true;
}

// lane VKBLOOM1: every binding of one set (0-2 sampled, 3 the sampler, 4 storage, 5-6 the frame, 7 the RGBA32F target)
void bloomWriteSet( VkDescriptorSet set, VkImageView src, VkImageView mip, VkImageView ring, VkImageView dst, VkImageView out32,
	VkImageView frame = VK_NULL_HANDLE )
{
	Vk & v = vk();
	Tone & T = tone();
	const VkImageView views[8] = { src, mip, ring, VK_NULL_HANDLE, dst, frame ? frame : T.colorView, T.stencilView, out32 };
	VkDescriptorImageInfo ii[8] = {};
	VkWriteDescriptorSet wr[8] = {};
	for ( std::uint32_t k = 0; k < 8; k++ ) {
		ii[k].imageLayout = k == 3 ? VK_IMAGE_LAYOUT_UNDEFINED : VK_IMAGE_LAYOUT_GENERAL;
		ii[k].imageView = views[k];
		ii[k].sampler = k == 3 ? T.sampler : VK_NULL_HANDLE;
		wr[k].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		wr[k].dstSet = set;
		wr[k].dstBinding = k;
		wr[k].descriptorCount = 1;
		wr[k].descriptorType = k == 3 ? VK_DESCRIPTOR_TYPE_SAMPLER
			: ( k == 4 || k == 7 ) ? VK_DESCRIPTOR_TYPE_STORAGE_IMAGE : VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
		wr[k].pImageInfo = &ii[k];
	}
	v.vkUpdateDescriptorSets( v.dev, 8, wr, 0, nullptr );
}

/* lane VKBLOOM1: the chain for this shape (source, ring, mips, ups as RGBA16F on this device alone; the combine RGBA32F
 * shared, GL may read it) and its sets over the frame's views. The caller waited the last pass and drained the worker. */
bool toneBloomChain( const WwVkPresent::ToneBloom & b, std::string & why )
{
	Vk & v = vk();
	Tone & T = tone();
	ToneBloomState & B = T.bloom;
	const int W = T.w, H = T.h, n = b.mips;
	bool same = B.pool && B.W == W && B.H == H && B.mips == n && B.setFrame == T.color;
	for ( int i = 0; same && i < n; i++ )
		same = B.dims[i][0] == b.dims[i][0] && B.dims[i][1] == b.dims[i][1];
	if ( same )
		return true;
	if ( n < 1 || n > WwVkPresent::kToneBloomMips ) {
		why = "no mip chain";
		return false;
	}
	// the measure and the backdrop stay; the chain goes
	bloomImgDrop( B.src, true );
	bloomImgDrop( B.ring, true );
	bloomImgDrop( B.comb, true );
	for ( int i = 0; i < WwVkPresent::kToneBloomMips; i++ ) {
		bloomImgDrop( B.mip[i], true );
		bloomImgDrop( B.up[i], true );
	}
	if ( B.pool )
		v.vkDestroyDescriptorPool( v.dev, B.pool, nullptr );
	B.pool = VK_NULL_HANDLE;
	B.mb = 0.0;
	B.setFrame = VK_NULL_HANDLE;
	const VkFormat f16 = VK_FORMAT_R16G16B16A16_SFLOAT;
	const VkImageUsageFlags use = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT;
	bool ok = bloomLocal( B.src, f16, use, W, H, 1, B.mb ) && bloomLocal( B.ring, f16, use, b.dims[0][0], b.dims[0][1], 8, B.mb );
	for ( int i = 0; ok && i < n; i++ ) {
		B.dims[i][0] = b.dims[i][0];
		B.dims[i][1] = b.dims[i][1];
		ok = bloomLocal( B.mip[i], f16, use, b.dims[i][0], b.dims[i][1], 1, B.mb );
		if ( ok && i + 1 < n )
			ok = bloomLocal( B.up[i], f16, use, b.dims[i][0], b.dims[i][1], 1, B.mb );
	}
	if ( !ok ) {
		why = "the bloom chain's images failed";
		return false;
	}
	std::vector<const BloomImg *> lay { &B.src, &B.ring };
	for ( int i = 0; i < n; i++ ) {
		lay.push_back( &B.mip[i] );
		lay.push_back( &B.up[i] );
	}
	if ( !bloomGeneral( lay ) ) {
		why = "the bloom chain's first layout change failed";
		return false;
	}
	if ( !makeShared( B.comb.img, B.comb.mem, B.comb.tex, B.comb.obj, VK_FORMAT_R32G32B32A32_SFLOAT, kRgba32f, 1, W, H, false, B.mb, why ) ) {
		why = "the bloom combine's shared target: " + why;
		return false;
	}
	B.comb.w = W;
	B.comb.h = H;
	B.comb.view = imageView( B.comb.img, VK_IMAGE_VIEW_TYPE_2D, VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_ASPECT_COLOR_BIT );
	if ( !B.comb.view ) {
		why = "the bloom combine's view failed";
		return false;
	}
	const int sets = n + 8 + 1 + n + 2;
	VkDescriptorPoolSize ps[3] = { { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, std::uint32_t( 5 * sets ) }, { VK_DESCRIPTOR_TYPE_SAMPLER,
		std::uint32_t( sets ) }, { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, std::uint32_t( 2 * sets ) } };
	VkDescriptorPoolCreateInfo dp { VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
	dp.maxSets = std::uint32_t( sets );
	dp.poolSizeCount = 3;
	dp.pPoolSizes = ps;
	std::vector<VkDescriptorSetLayout> lays( size_t( sets ), B.dsl );
	std::vector<VkDescriptorSet> all( static_cast<size_t>( sets ) );
	if ( v.vkCreateDescriptorPool( v.dev, &dp, nullptr, &B.pool ) != VK_SUCCESS ) {
		why = "the bloom's descriptor pool failed";
		return false;
	}
	VkDescriptorSetAllocateInfo ai { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
	ai.descriptorPool = B.pool;
	ai.descriptorSetCount = std::uint32_t( sets );
	ai.pSetLayouts = lays.data();
	if ( v.vkAllocateDescriptorSets( v.dev, &ai, all.data() ) != VK_SUCCESS ) {
		why = "the bloom's descriptor sets failed";
		return false;
	}
	// vkframe.cpp makeChain's sets; every binding written (the frame's views, the combine as the RGBA32F target)
	const VkImageView src = B.src.view, ring = B.ring.view, c32 = B.comb.view;
	int k = 0;
	for ( int i = 0; i < n; i++ ) {
		B.down[i] = all[size_t( k++ )];
		bloomWriteSet( B.down[i], i == 0 ? src : B.mip[i - 1].view, src, ring, B.mip[i].view, c32 );
	}
	for ( int l = 0; l < 8; l++ ) {
		B.downRing[l] = all[size_t( k++ )];
		bloomWriteSet( B.downRing[l], src, src, ring, B.ring.layer[l], c32 );
	}
	B.hold = all[size_t( k++ )];
	bloomWriteSet( B.hold, src, src, ring, B.mip[0].view, c32 );
	for ( int level = 0; level + 1 < n; level++ ) {
		B.upSet[level] = all[size_t( k++ )];
		const VkImageView acc = level + 2 == n ? B.mip[n - 1].view : B.up[level + 1].view;
		bloomWriteSet( B.upSet[level], B.mip[level].view, acc, ring, B.up[level].view, c32 );
	}
	B.srcSet = all[size_t( k++ )];
	bloomWriteSet( B.srcSet, src, src, ring, src, c32 );
	B.combSet = all[size_t( k++ )];
	const VkImageView top = n == 1 ? B.mip[0].view : B.up[0].view;
	bloomWriteSet( B.combSet, top, top, ring, src, c32 );
	B.W = W;
	B.H = H;
	B.mips = n;
	B.setFrame = T.color;
	B.fresh = true;
	T.descDirty = true;
	std::fprintf( stderr, "vk view: bloom chain made %dx%d, %d mips (%.1f MB; the Fusion Shaders bloom in the tone pass)\n", W, H, n,
		B.mb );
	return true;
}

/* lane VKBLOOM1: the measure's quarter target (RGBA32F, shared: GL blurs it into the measure's bloom) and its host read
 * buffer; the measure set's target written again. The caller waited the last pass and drained the worker. */
bool toneBloomMeasure( int qw, int qh, std::string & why )
{
	Vk & v = vk();
	Tone & T = tone();
	ToneBloomState & B = T.bloom;
	if ( B.meas.img && B.measW == qw && B.measH == qh && B.read && B.measSet )
		return true;
	bloomImgDrop( B.meas, true );
	bloomImgDrop( B.measFill, true );
	if ( B.measPool )
		v.vkDestroyDescriptorPool( v.dev, B.measPool, nullptr );
	B.measPool = VK_NULL_HANDLE;
	B.measSet = VK_NULL_HANDLE;
	if ( B.read ) {
		v.vkUnmapMemory( v.dev, B.readMem );
		v.vkDestroyBuffer( v.dev, B.read, nullptr );
	}
	if ( B.readMem )
		v.vkFreeMemory( v.dev, B.readMem, nullptr );
	B.read = VK_NULL_HANDLE;
	B.readMem = VK_NULL_HANDLE;
	B.readPtr = nullptr;
	B.measHave = false;
	B.measMb = 0.0;
	B.measW = B.measH = 0;
	if ( !makeShared( B.meas.img, B.meas.mem, B.meas.tex, B.meas.obj, VK_FORMAT_R32G32B32A32_SFLOAT, kRgba32f, 1, qw, qh, false, B.measMb,
			why ) ) {
		why = "the measure's shared target: " + why;
		bloomImgDrop( B.meas, true );
		return false;
	}
	B.meas.w = qw;
	B.meas.h = qh;
	B.meas.view = imageView( B.meas.img, VK_IMAGE_VIEW_TYPE_2D, VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_ASPECT_COLOR_BIT );
	VkBufferCreateInfo bci { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
	bci.size = VkDeviceSize( qw ) * VkDeviceSize( qh ) * 16;
	bci.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	bool ok = B.meas.view && v.vkCreateBuffer( v.dev, &bci, nullptr, &B.read ) == VK_SUCCESS;
	if ( ok ) {
		VkMemoryRequirements br {};
		v.vkGetBufferMemoryRequirements( v.dev, B.read, &br );
		VkMemoryAllocateInfo ai { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
		ai.allocationSize = br.size;
		ai.memoryTypeIndex = memType( br.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT );
		void * p = nullptr;
		ok = ai.memoryTypeIndex != UINT32_MAX && v.vkAllocateMemory( v.dev, &ai, nullptr, &B.readMem ) == VK_SUCCESS
			&& v.vkBindBufferMemory( v.dev, B.read, B.readMem, 0 ) == VK_SUCCESS
			&& v.vkMapMemory( v.dev, B.readMem, 0, VK_WHOLE_SIZE, 0, &p ) == VK_SUCCESS;
		B.readPtr = static_cast<float *>( p );
		B.measMb += double( br.size ) / 1048576.0;
	}
	if ( !ok || !B.readPtr ) {
		why = "the measure's view / read buffer failed";
		return false;
	}
	// its own set (measure_q.hlsl reads the frame, writes binding 7); the bindings it does not read hold a 1 x 1 fill
	ok = bloomLocal( B.measFill, VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT, 1, 1, 2, B.measMb )
		&& bloomGeneral( { &B.measFill } );
	if ( ok ) {
		VkDescriptorPoolSize ps[3] = { { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 5 }, { VK_DESCRIPTOR_TYPE_SAMPLER, 1 },
			{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 2 } };
		VkDescriptorPoolCreateInfo dp { VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
		dp.maxSets = 1;
		dp.poolSizeCount = 3;
		dp.pPoolSizes = ps;
		ok = v.vkCreateDescriptorPool( v.dev, &dp, nullptr, &B.measPool ) == VK_SUCCESS;
		VkDescriptorSetAllocateInfo ai { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
		ai.descriptorPool = B.measPool;
		ai.descriptorSetCount = 1;
		ai.pSetLayouts = &B.dsl;
		ok = ok && v.vkAllocateDescriptorSets( v.dev, &ai, &B.measSet ) == VK_SUCCESS;
	}
	if ( !ok ) {
		why = "the measure's descriptor set failed";
		return false;
	}
	const VkImageView fill = B.measFill.layer[0];
	bloomWriteSet( B.measSet, fill, fill, B.measFill.view, fill, B.meas.view );
	B.measW = qw;
	B.measH = qh;
	std::fprintf( stderr, "vk view: exposure measure target made %dx%d RGBA32F (read back in the tone pass's command buffer)\n", qw, qh );
	return true;
}

/* lane VKBLOOM1: the backdrop mode 3 reads (cellhdr.cpp's backdropCopy: the view resolved into RGBA16F by a blit): the
 * view slot's copy (RGBA8, its samples), the resolve's target (RGBA16F) and bloom_back.hlsl's set. The caller drained. */
bool toneBloomBack( int samples, std::string & why )
{
	Vk & v = vk();
	Tone & T = tone();
	ToneBloomState & B = T.bloom;
	bloomImgDrop( B.back, true );
	bloomImgDrop( B.backMs, true );
	bloomImgDrop( B.backFill, true );
	if ( B.backPool )
		v.vkDestroyDescriptorPool( v.dev, B.backPool, nullptr );
	B.backPool = VK_NULL_HANDLE;
	B.backSet = VK_NULL_HANDLE;
	B.backMb = 0.0;
	const VkFormat f16 = VK_FORMAT_R16G16B16A16_SFLOAT;
	const VkImageUsageFlags use = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT;
	bool ok = bloomLocal( B.backMs, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, T.w, T.h, 1,
				  B.backMb, samples )
		&& bloomLocal( B.back, f16, use, T.w, T.h, 1, B.backMb ) && bloomLocal( B.backFill, f16, use, 1, 1, 2, B.backMb )
		&& bloomGeneral( { &B.backMs, &B.back, &B.backFill } );
	if ( ok ) {
		VkDescriptorPoolSize ps[3] = { { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 5 }, { VK_DESCRIPTOR_TYPE_SAMPLER, 1 },
			{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 2 } };
		VkDescriptorPoolCreateInfo dp { VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
		dp.maxSets = 1;
		dp.poolSizeCount = 3;
		dp.pPoolSizes = ps;
		ok = v.vkCreateDescriptorPool( v.dev, &dp, nullptr, &B.backPool ) == VK_SUCCESS;
		VkDescriptorSetAllocateInfo ai { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
		ai.descriptorPool = B.backPool;
		ai.descriptorSetCount = 1;
		ai.pSetLayouts = &B.dsl;
		ok = ok && v.vkAllocateDescriptorSets( v.dev, &ai, &B.backSet ) == VK_SUCCESS;
	}
	if ( !ok ) {
		why = "the backdrop's images / set failed";
		return false;
	}
	const VkImageView fill = B.backFill.layer[0];
	bloomWriteSet( B.backSet, fill, fill, B.backFill.view, B.back.view, fill, B.backMs.view );
	return true;
}

// the shared frame goes (GL alive: its textures and memory objects too); the caller waited the device
void toneFrameFree( bool glAlive )
{
	Vk & v = vk();
	Gl & g = gl();
	Tone & T = tone();
	if ( glAlive ) {
		for ( GLuint * t : { &T.colorTex, &T.dsTex } )
			if ( *t )
				g.deleteTextures( 1, t );
		for ( GLuint * o : { &T.colorObj, &T.dsObj } )
			if ( *o )
				g.deleteMemoryObjects( 1, o );
	}
	for ( VkImageView * iv : { &T.colorView, &T.stencilView } )
		if ( *iv )
			v.vkDestroyImageView( v.dev, *iv, nullptr );
	for ( VkImage * i : { &T.color, &T.ds } )
		if ( *i )
			v.vkDestroyImage( v.dev, *i, nullptr );
	for ( VkDeviceMemory * m : { &T.colorMem, &T.dsMem } )
		if ( *m )
			v.vkFreeMemory( v.dev, *m, nullptr );
	toneMeasureDrop( glAlive );
	toneBloomFree( glAlive );
	T.colorTex = T.dsTex = T.colorObj = T.dsObj = 0;
	T.colorView = T.stencilView = VK_NULL_HANDLE;
	T.color = T.ds = VK_NULL_HANDLE;
	T.colorMem = T.dsMem = VK_NULL_HANDLE;
	T.w = T.h = T.samples = 0;
	T.frameMb = 0.0;
	T.glCtx = nullptr;
	T.descDirty = true;
}

// the pipeline cache: read from the file the Qt part names (none: an empty cache), written after a build
void toneCacheSave()
{
	Vk & v = vk();
	Tone & T = tone();
	if ( !T.cache || T.cachePath.empty() )
		return;
	size_t n = 0;
	if ( v.vkGetPipelineCacheData( v.dev, T.cache, &n, nullptr ) != VK_SUCCESS || !n )
		return;
	std::vector<char> data( n );
	if ( v.vkGetPipelineCacheData( v.dev, T.cache, &n, data.data() ) != VK_SUCCESS )
		return;
	if ( FILE * f = std::fopen( T.cachePath.c_str(), "wb" ) ) {
		std::fwrite( data.data(), 1, n, f );
		std::fclose( f );
	}
}

/* the device-time parts (once): sampler, set layout, pool + set, pipeline layout, shader modules, pipeline cache, the
 * dummy and the LUT images, the LUT's staging buffer, the two semaphores, the command buffer and its fence. The caller
 * drained the worker (the dummy's first fill is a submit). */
bool toneStatic( std::string & why )
{
	Vk & v = vk();
	Tone & T = tone();
	if ( T.made )
		return true;
	VkSamplerCreateInfo sci { VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
	sci.magFilter = sci.minFilter = VK_FILTER_LINEAR;
	sci.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
	sci.addressModeU = sci.addressModeV = sci.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	sci.maxLod = 0.0f;
	if ( v.vkCreateSampler( v.dev, &sci, nullptr, &T.sampler ) != VK_SUCCESS ) {
		why = "vkCreateSampler (the tone pass) failed";
		return false;
	}
	VkDescriptorSetLayoutBinding b[8] = {};
	for ( std::uint32_t k = 0; k < 8; k++ ) {
		b[k].binding = k;
		b[k].descriptorType = k == 7 ? VK_DESCRIPTOR_TYPE_SAMPLER : VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
		b[k].descriptorCount = 1;
		b[k].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	}
	VkDescriptorSetLayoutCreateInfo lci { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
	lci.bindingCount = 8;
	lci.pBindings = b;
	VkDescriptorPoolSize ps[2] = { { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 7 }, { VK_DESCRIPTOR_TYPE_SAMPLER, 1 } };
	VkDescriptorPoolCreateInfo pci { VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
	pci.maxSets = 1;
	pci.poolSizeCount = 2;
	pci.pPoolSizes = ps;
	if ( v.vkCreateDescriptorSetLayout( v.dev, &lci, nullptr, &T.dsl ) != VK_SUCCESS
		|| v.vkCreateDescriptorPool( v.dev, &pci, nullptr, &T.dpool ) != VK_SUCCESS ) {
		why = "the tone pass's descriptor set layout / pool failed";
		return false;
	}
	VkDescriptorSetAllocateInfo dai { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
	dai.descriptorPool = T.dpool;
	dai.descriptorSetCount = 1;
	dai.pSetLayouts = &T.dsl;
	VkPushConstantRange pcr { VK_SHADER_STAGE_FRAGMENT_BIT, 0, std::uint32_t( sizeof( WwVkPresent::ToneJob ) ) };
	VkPipelineLayoutCreateInfo plci { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
	plci.setLayoutCount = 1;
	plci.pSetLayouts = &T.dsl;
	plci.pushConstantRangeCount = 1;
	plci.pPushConstantRanges = &pcr;
	if ( v.vkAllocateDescriptorSets( v.dev, &dai, &T.set ) != VK_SUCCESS
		|| v.vkCreatePipelineLayout( v.dev, &plci, nullptr, &T.layout ) != VK_SUCCESS ) {
		why = "the tone pass's descriptor set / pipeline layout failed";
		return false;
	}
	VkShaderModuleCreateInfo mci { VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
	mci.codeSize = sizeof( vkframe_tone_view_vs_spv );
	mci.pCode = vkframe_tone_view_vs_spv;
	const bool vsOk = v.vkCreateShaderModule( v.dev, &mci, nullptr, &T.vs ) == VK_SUCCESS;
	mci.codeSize = sizeof( vkframe_tone_view_ps_spv );
	mci.pCode = vkframe_tone_view_ps_spv;
	if ( !vsOk || v.vkCreateShaderModule( v.dev, &mci, nullptr, &T.ps ) != VK_SUCCESS ) {
		why = "the tone pass's shader modules failed";
		return false;
	}
	std::vector<char> seed;
	if ( !T.cachePath.empty() ) {
		if ( FILE * f = std::fopen( T.cachePath.c_str(), "rb" ) ) {
			char buf[65536];
			size_t n = 0;
			while ( ( n = std::fread( buf, 1, sizeof( buf ), f ) ) > 0 )
				seed.insert( seed.end(), buf, buf + n );
			std::fclose( f );
		}
	}
	VkPipelineCacheCreateInfo cci { VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO };
	cci.initialDataSize = seed.size();
	cci.pInitialData = seed.empty() ? nullptr : seed.data();
	if ( v.vkCreatePipelineCache( v.dev, &cci, nullptr, &T.cache ) != VK_SUCCESS ) {
		cci.initialDataSize = 0;  // a stale file: start empty
		cci.pInitialData = nullptr;
		if ( v.vkCreatePipelineCache( v.dev, &cci, nullptr, &T.cache ) != VK_SUCCESS )
			T.cache = VK_NULL_HANDLE;
	}
	if ( !makeLocal( T.dummy, T.dummyMem, VK_IMAGE_TYPE_2D, VK_FORMAT_R8G8B8A8_UNORM, 1, 1, 1, T.fixedMb )
		|| !makeLocal( T.lut, T.lutMem, VK_IMAGE_TYPE_3D, VK_FORMAT_R8G8B8A8_UNORM, 16, 16, 16, T.fixedMb ) ) {
		why = "the tone pass's dummy / LUT image failed";
		return false;
	}
	T.dummyView = imageView( T.dummy, VK_IMAGE_VIEW_TYPE_2D, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT );
	T.lutView = imageView( T.lut, VK_IMAGE_VIEW_TYPE_3D, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT );
	VkBufferCreateInfo bci { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
	bci.size = 16 * 16 * 16 * 4;
	bci.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
	VkMemoryRequirements br {};
	bool staged = v.vkCreateBuffer( v.dev, &bci, nullptr, &T.staging ) == VK_SUCCESS;
	if ( staged ) {
		v.vkGetBufferMemoryRequirements( v.dev, T.staging, &br );
		VkMemoryAllocateInfo ai { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
		ai.allocationSize = br.size;
		ai.memoryTypeIndex = memType( br.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT );
		void * p = nullptr;
		staged = ai.memoryTypeIndex != UINT32_MAX && v.vkAllocateMemory( v.dev, &ai, nullptr, &T.stagingMem ) == VK_SUCCESS
			&& v.vkBindBufferMemory( v.dev, T.staging, T.stagingMem, 0 ) == VK_SUCCESS
			&& v.vkMapMemory( v.dev, T.stagingMem, 0, VK_WHOLE_SIZE, 0, &p ) == VK_SUCCESS;
		T.stagingPtr = static_cast<std::uint8_t *>( p );
	}
	if ( !staged || !T.dummyView || !T.lutView ) {
		why = "the tone pass's LUT staging buffer / image views failed";
		return false;
	}
	VkFenceCreateInfo fi { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
	VkCommandBufferAllocateInfo cai { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
	cai.commandPool = v.pool;
	cai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	cai.commandBufferCount = 1;
	if ( v.vkCreateFence( v.dev, &fi, nullptr, &T.fence ) != VK_SUCCESS || v.vkAllocateCommandBuffers( v.dev, &cai, &T.cmd ) != VK_SUCCESS ) {
		why = "the tone pass's fence / command buffer failed";
		return false;
	}
	// the dummy and the LUT: GENERAL, zero (a zero dummy reads as "no bloom", "no fog gain")
	std::memset( T.stagingPtr, 0, size_t( bci.size ) );
	VkCommandBufferBeginInfo bi { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
	bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	v.vkBeginCommandBuffer( T.cmd, &bi );
	VkImageMemoryBarrier ib[2] = { { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER }, { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER } };
	for ( int k = 0; k < 2; k++ ) {
		ib[k].oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		ib[k].newLayout = VK_IMAGE_LAYOUT_GENERAL;
		ib[k].dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		ib[k].srcQueueFamilyIndex = ib[k].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		ib[k].image = k ? T.lut : T.dummy;
		ib[k].subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
	}
	v.vkCmdPipelineBarrier( T.cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 2, ib );
	VkBufferImageCopy c[2] = {};
	c[0].imageSubresource = c[1].imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
	c[0].imageExtent = { 1, 1, 1 };
	c[1].imageExtent = { 16, 16, 16 };
	v.vkCmdCopyBufferToImage( T.cmd, T.staging, T.dummy, VK_IMAGE_LAYOUT_GENERAL, 1, &c[0] );
	v.vkCmdCopyBufferToImage( T.cmd, T.staging, T.lut, VK_IMAGE_LAYOUT_GENERAL, 1, &c[1] );
	for ( int k = 0; k < 2; k++ ) {
		ib[k].oldLayout = ib[k].newLayout;
		ib[k].srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		ib[k].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
	}
	v.vkCmdPipelineBarrier( T.cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 2, ib );
	v.vkEndCommandBuffer( T.cmd );
	if ( !oneShot( T.cmd, T.fence ) ) {
		why = "the tone pass's first fill failed";
		return false;
	}
	T.made = true;
	return true;
}

// the crossing's two semaphores go (GL alive: GL's names too); the caller waited the device
void toneSemsDrop( bool glAlive )
{
	Vk & v = vk();
	Gl & g = gl();
	Tone & T = tone();
	if ( glAlive )
		for ( GLuint * s : { &T.glDoneGl, &T.vkDoneGl } )
			if ( *s )
				g.deleteSemaphores( 1, s );
	for ( VkSemaphore * s : { &T.glDone, &T.vkDone } )
		if ( *s )
			v.vkDestroySemaphore( v.dev, *s, nullptr );
	T.glDoneGl = T.vkDoneGl = 0;
	T.glDone = T.vkDone = VK_NULL_HANDLE;
	T.semCtx = nullptr;
}

// the crossing's semaphores, imported into the GL context current (made again when that context changes)
bool toneSems( std::string & why )
{
	Gl & g = gl();
	Tone & T = tone();
	if ( T.glDone && T.semCtx == g.ctx )
		return true;
	toneSemsDrop( false );  // another context's names: that context went
	if ( !exportSem( T.glDone, T.glDoneGl, why ) || !exportSem( T.vkDone, T.vkDoneGl, why ) ) {
		toneSemsDrop( true );
		return false;
	}
	T.semCtx = g.ctx;
	return true;
}

// the render pass (LOAD / STORE, GENERAL) and the three pipelines at the view's samples
bool tonePipes( int samples, std::string & why )
{
	Vk & v = vk();
	Tone & T = tone();
	if ( T.pass && T.pipeSamples == samples )
		return true;
	if ( samples > 1 && !v.sampleShading ) {
		why = "the device offers no sampleRateShading (the pass reads SV_SampleIndex)";
		return false;
	}
	tonePipesDrop();
	const double t0 = nowMs();
	const VkSampleCountFlagBits sc = VkSampleCountFlagBits( samples );
	VkAttachmentDescription at[2] = {};
	at[0].format = VK_FORMAT_R8G8B8A8_UNORM;
	at[1].format = VK_FORMAT_D24_UNORM_S8_UINT;
	for ( VkAttachmentDescription & a : at ) {
		a.samples = sc;
		a.loadOp = a.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
		a.storeOp = a.stencilStoreOp = VK_ATTACHMENT_STORE_OP_STORE;
		a.initialLayout = a.finalLayout = VK_IMAGE_LAYOUT_GENERAL;
	}
	const VkAttachmentReference cr { 0, VK_IMAGE_LAYOUT_GENERAL }, dr { 1, VK_IMAGE_LAYOUT_GENERAL };
	VkSubpassDescription sp {};
	sp.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	sp.colorAttachmentCount = 1;
	sp.pColorAttachments = &cr;
	sp.pDepthStencilAttachment = &dr;
	VkRenderPassCreateInfo rci { VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO };
	rci.attachmentCount = 2;
	rci.pAttachments = at;
	rci.subpassCount = 1;
	rci.pSubpasses = &sp;
	if ( v.vkCreateRenderPass( v.dev, &rci, nullptr, &T.pass ) != VK_SUCCESS ) {
		why = "vkCreateRenderPass (the tone pass) failed";
		return false;
	}
	VkPipelineShaderStageCreateInfo st[2] = { { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO },
		{ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO } };
	st[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
	st[0].module = T.vs;
	st[0].pName = "main";
	st[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	st[1].module = T.ps;
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
	ms.rasterizationSamples = sc;
	ms.sampleShadingEnable = samples > 1 ? VK_TRUE : VK_FALSE;  // per sample, as GL's gl_SampleID read makes it
	ms.minSampleShading = 1.0f;
	VkPipelineDepthStencilStateCreateInfo dss[3] = { { VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO },
		{ VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO }, { VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO } };
	dss[1].depthTestEnable = dss[2].depthTestEnable = VK_TRUE;  // never written: the quad at the far plane only tests
	dss[1].depthCompareOp = VK_COMPARE_OP_GREATER;
	dss[2].depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
	VkPipelineColorBlendAttachmentState cb[3] = {};
	cb[0].colorWriteMask = cb[1].colorWriteMask = cb[2].colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT
		| VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	cb[1].blendEnable = VK_TRUE;  // mode 2: premultiplied over the backdrop (GL_ONE, GL_ONE_MINUS_SRC_ALPHA)
	cb[1].srcColorBlendFactor = cb[1].srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	cb[1].dstColorBlendFactor = cb[1].dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	cb[1].colorBlendOp = cb[1].alphaBlendOp = VK_BLEND_OP_ADD;
	cb[2].blendEnable = VK_TRUE;  // lane VKBLOOM1, mode 3: the glow added (GL_ONE, GL_ONE)
	cb[2].srcColorBlendFactor = cb[2].srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	cb[2].dstColorBlendFactor = cb[2].dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	cb[2].colorBlendOp = cb[2].alphaBlendOp = VK_BLEND_OP_ADD;
	VkPipelineColorBlendStateCreateInfo cbs[3] = { { VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO },
		{ VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO }, { VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO } };
	for ( int k = 0; k < 3; k++ ) {
		cbs[k].attachmentCount = 1;
		cbs[k].pAttachments = &cb[k];
	}
	const VkDynamicState dyn[2] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
	VkPipelineDynamicStateCreateInfo dy { VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
	dy.dynamicStateCount = 2;
	dy.pDynamicStates = dyn;
	VkGraphicsPipelineCreateInfo gp[5] = {};
	const int dsOf[5] = { 0, 1, 2, 0, 2 }, cbOf[5] = { 0, 0, 1, 2, 2 };
	for ( int k = 0; k < 5; k++ ) {
		gp[k].sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		gp[k].stageCount = 2;
		gp[k].pStages = st;
		gp[k].pVertexInputState = &vi;
		gp[k].pInputAssemblyState = &ia;
		gp[k].pViewportState = &vps;
		gp[k].pRasterizationState = &rs;
		gp[k].pMultisampleState = &ms;
		gp[k].pDepthStencilState = &dss[dsOf[k]];
		gp[k].pColorBlendState = &cbs[cbOf[k]];
		gp[k].pDynamicState = &dy;
		gp[k].layout = T.layout;
		gp[k].renderPass = T.pass;
	}
	if ( v.vkCreateGraphicsPipelines( v.dev, T.cache, 5, gp, nullptr, T.pipe ) != VK_SUCCESS ) {
		why = "vkCreateGraphicsPipelines (the tone pass, " + std::to_string( samples ) + " samples) failed";
		return false;
	}
	T.pipeSamples = samples;
	T.pipelines = 5;
	T.buildMs = nowMs() - t0;
	toneCacheSave();
	return true;
}

// the framebuffers over the view ring's slots (made again when the ring is)
bool toneFbs( std::string & why )
{
	Vk & v = vk();
	Tone & T = tone();
	View & V = view();
	if ( T.fb[0] && T.fbGen == V.gen )
		return true;
	toneFbsDrop();
	for ( int k = 0; k < kViewSlots; k++ ) {
		T.vColor[k] = imageView( V.slot[k].color, VK_IMAGE_VIEW_TYPE_2D, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT );
		T.vDs[k] = imageView( V.slot[k].ds, VK_IMAGE_VIEW_TYPE_2D, VK_FORMAT_D24_UNORM_S8_UINT,
			VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT );
		const VkImageView att[2] = { T.vColor[k], T.vDs[k] };
		VkFramebufferCreateInfo fci { VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
		fci.renderPass = T.pass;
		fci.attachmentCount = 2;
		fci.pAttachments = att;
		fci.width = std::uint32_t( V.w );
		fci.height = std::uint32_t( V.h );
		fci.layers = 1;
		if ( !att[0] || !att[1] || v.vkCreateFramebuffer( v.dev, &fci, nullptr, &T.fb[k] ) != VK_SUCCESS ) {
			toneFbsDrop();
			why = "the tone pass's framebuffer over a view failed";
			return false;
		}
	}
	T.fbGen = V.gen;
	return true;
}

// lane VKBLOOM1: harness only (tests/vkframe/toneview.cpp's reds, must FAIL); 1 = mode 3 not drawn, 2 = the measure not
// dispatched, 3 = mode 3 reads no backdrop
int bloomRedTest = 0;

// the worker: record the LUT's copy (when new) and the pass into this frame's view slot (toneWork submits it)
bool toneRecord( std::string & why )
{
	Vk & v = vk();
	Tone & T = tone();
	View & V = view();
	if ( T.slot < 0 || !T.fb[T.slot] ) {
		why = "no view slot to draw into";
		return false;
	}
	v.vkResetFences( v.dev, 1, &T.fence );
	v.vkResetCommandBuffer( T.cmd, 0 );
	VkCommandBufferBeginInfo bi { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
	bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	v.vkBeginCommandBuffer( T.cmd, &bi );
	if ( T.lutCopy ) {
		VkImageMemoryBarrier b { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
		b.oldLayout = b.newLayout = VK_IMAGE_LAYOUT_GENERAL;
		b.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
		b.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		b.srcQueueFamilyIndex = b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		b.image = T.lut;
		b.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
		v.vkCmdPipelineBarrier( T.cmd, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &b );
		VkBufferImageCopy c {};
		c.imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
		c.imageExtent = { 16, 16, 16 };
		v.vkCmdCopyBufferToImage( T.cmd, T.staging, T.lut, VK_IMAGE_LAYOUT_GENERAL, 1, &c );
		b.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		b.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		v.vkCmdPipelineBarrier( T.cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &b );
		T.lutCopy = false;
	}
	ToneBloomState & B = T.bloom;
	if ( B.on || B.measQw > 0 ) {
		/* lane VKBLOOM1: vkframe.cpp runBloom's dispatches on this command buffer, from the shared frame GL just drew (the
		 * semaphore's wait + this barrier): the source, the exposure measure, down / hold / up, the combine. The tone draws
		 * read the combine; GL reads the combine and the measure after B */
		VkMemoryBarrier mb { VK_STRUCTURE_TYPE_MEMORY_BARRIER };
		mb.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;
		mb.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
		v.vkCmdPipelineBarrier( T.cmd, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &mb, 0, nullptr, 0,
			nullptr );
		struct Pc
		{
			float params[4];
			float spare[4];
			std::int32_t ints[4];
		};
		auto dispatch = [&]( int pipe, VkDescriptorSet set, int w, int h, const Pc & j, int group ) {
			v.vkCmdBindPipeline( T.cmd, VK_PIPELINE_BIND_POINT_COMPUTE, B.pipe[pipe] );
			v.vkCmdBindDescriptorSets( T.cmd, VK_PIPELINE_BIND_POINT_COMPUTE, B.layout, 0, 1, &set, 0, nullptr );
			v.vkCmdPushConstants( T.cmd, B.layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, std::uint32_t( sizeof( Pc ) ), &j );
			v.vkCmdDispatch( T.cmd, std::uint32_t( ( w + group - 1 ) / group ), std::uint32_t( ( h + group - 1 ) / group ), 1 );
			VkMemoryBarrier cb { VK_STRUCTURE_TYPE_MEMORY_BARRIER };
			cb.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
			cb.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
			v.vkCmdPipelineBarrier( T.cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &cb, 0, nullptr,
				0, nullptr );
		};
		const WwVkPresent::ToneBloom & b = B.job;
		if ( B.on ) {  // the source (bloom_src.hlsl)
			Pc j {};
			j.params[0] = b.exposure;
			j.params[1] = b.impulseX;
			j.params[2] = b.impulseY;
			j.params[3] = b.impulse;
			j.ints[0] = b.on ? 1 : 0;
			dispatch( 3, B.srcSet, B.W, B.H, j, 8 );
		}
		if ( B.measQw > 0 && bloomRedTest != 2 ) {  // the measure (measure_q.hlsl): 4 x 4 quarter texels a group
			const Pc j {};
			dispatch( 5, B.measSet, B.measQw, B.measQh, j, 4 );
		}
		if ( B.on ) {
			const int n = B.mips;
			for ( int i = 0; i < n; i++ ) {
				Pc j {};
				j.params[0] = b.thr;
				j.params[1] = b.knee;
				j.params[2] = i == 0 ? 1.0f : 0.0f;
				j.params[3] = 1.0f;
				j.ints[0] = b.red;
				if ( i == 0 && b.hold ) {
					dispatch( 0, B.downRing[b.holdK & 7], B.dims[0][0], B.dims[0][1], j, 8 );
					Pc h {};
					h.ints[1] = b.holdK;
					h.ints[2] = b.holdN;
					dispatch( 1, B.hold, B.dims[0][0], B.dims[0][1], h, 8 );
				} else {
					dispatch( 0, B.down[i], B.dims[i][0], B.dims[i][1], j, 8 );
				}
			}
			for ( int offset = n - 1; offset > 0; offset-- ) {
				const int level = offset - 1;
				Pc j {};
				j.params[0] = b.radius;
				j.params[1] = b.srcW[level];
				j.params[2] = b.accW[level];
				j.params[3] = b.dispersion;
				dispatch( 2, B.upSet[level], B.dims[level][0], B.dims[level][1], j, 8 );
			}
			Pc j {};  // the final tent once per pixel (bloom_comb.hlsl), lod 0 of the top; the tone pass weighs it
			j.params[0] = b.radius;
			j.params[2] = b.dispersion;
			dispatch( 4, B.combSet, B.W, B.H, j, 8 );
			B.records++;
		}
		if ( B.measQw > 0 ) {  // the measure's texels into the host buffer: read at the next frame (toneMeasureTake)
			mb.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
			mb.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
			v.vkCmdPipelineBarrier( T.cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 1, &mb, 0, nullptr, 0,
				nullptr );
			VkBufferImageCopy c {};
			c.imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
			c.imageExtent = { std::uint32_t( B.measQw ), std::uint32_t( B.measQh ), 1 };
			v.vkCmdCopyImageToBuffer( T.cmd, B.meas.img, VK_IMAGE_LAYOUT_GENERAL, B.read, 1, &c );
			mb.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			mb.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
			v.vkCmdPipelineBarrier( T.cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0, 1, &mb, 0, nullptr, 0, nullptr );
		}
		mb.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
		mb.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_MEMORY_READ_BIT;
		v.vkCmdPipelineBarrier( T.cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
			| VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 1, &mb, 0, nullptr, 0, nullptr );
	}
	VkRenderPassBeginInfo rbi { VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
	rbi.renderPass = T.pass;
	rbi.framebuffer = T.fb[T.slot];
	rbi.renderArea = { { 0, 0 }, { std::uint32_t( V.w ), std::uint32_t( V.h ) } };
	v.vkCmdBeginRenderPass( T.cmd, &rbi, VK_SUBPASS_CONTENTS_INLINE );
	// GL's window rows are the image's rows (no flip: GL's row 0 and Vulkan's row 0 are the same texels)
	const VkViewport vp { float( T.vp[0] ), float( T.vp[1] ), float( T.vp[2] ), float( T.vp[3] ), 0.0f, 1.0f };
	const VkRect2D sc { { T.vp[0], T.vp[1] }, { std::uint32_t( T.vp[2] ), std::uint32_t( T.vp[3] ) } };
	v.vkCmdSetViewport( T.cmd, 0, 1, &vp );
	v.vkCmdSetScissor( T.cmd, 0, 1, &sc );
	v.vkCmdBindDescriptorSets( T.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, T.layout, 0, 1, &T.set, 0, nullptr );
	auto draw = [&]( int p, int mode, int test, int ref ) {
		WwVkPresent::ToneJob j = T.job;
		j.ints[0] = ( j.ints[0] & ~( ( 3 << 16 ) | ( 3 << 20 ) | ( 3 << 24 ) ) ) | ( mode << 16 ) | ( test << 20 ) | ( ref << 24 );
		v.vkCmdBindPipeline( T.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, T.pipe[p] );
		v.vkCmdPushConstants( T.cmd, T.layout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, std::uint32_t( sizeof( j ) ), &j );
		v.vkCmdDraw( T.cmd, 3, 1, 0, 0 );
	};
	// cellhdr.cpp's quads: mode 0 where an opaque draw reached (GREATER), mode 2 over the far plane, mode 1 stencil 2
	if ( T.splitSky ) {
		draw( 1, 0, 1, 1 );
		draw( 2, 2, 1, 1 );
	} else {
		draw( 0, 0, T.sorted ? 1 : 0, 1 );
	}
	if ( T.sorted )
		draw( 0, 1, 1, 2 );
	if ( B.on && T.sorted && bloomRedTest != 1 ) {
		/* lane VKBLOOM1: cellhdr.cpp's mode-3 tail, the glow over the pixels mode 0 did not take (added, GL_ONE / GL_ONE):
		 * stencil not 1 everywhere, then (split sky) the 1s at the far plane. First the view as mode 3 finds it, resolved
		 * (GLAREBLOW2's backdrop: the pass ends, the resolve, the pass again with its contents loaded) */
		if ( B.backdrop ) {
			v.vkCmdEndRenderPass( T.cmd );
			VkMemoryBarrier mb { VK_STRUCTURE_TYPE_MEMORY_BARRIER };
			mb.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			mb.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
			v.vkCmdPipelineBarrier( T.cmd, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
				| VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &mb, 0,
				nullptr, 0, nullptr );
			/* every sample copied, then their mean in half float (bloom_back.hlsl) as GL's blit resolve into RGBA16F keeps it:
			 * an RGBA8 resolve rounds the mean to 8 bits, and the vanilla curve's shoulder turned that into 199 levels */
			VkImageCopy cp {};
			cp.srcSubresource = cp.dstSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
			cp.extent = { std::uint32_t( B.backMs.w ), std::uint32_t( B.backMs.h ), 1 };
			v.vkCmdCopyImage( T.cmd, V.slot[T.slot].color, VK_IMAGE_LAYOUT_GENERAL, B.backMs.img, VK_IMAGE_LAYOUT_GENERAL, 1, &cp );
			mb.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			mb.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
			v.vkCmdPipelineBarrier( T.cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &mb, 0, nullptr, 0,
				nullptr );
			const std::int32_t noJob[12] = {};
			v.vkCmdBindPipeline( T.cmd, VK_PIPELINE_BIND_POINT_COMPUTE, B.pipe[6] );
			v.vkCmdBindDescriptorSets( T.cmd, VK_PIPELINE_BIND_POINT_COMPUTE, B.layout, 0, 1, &B.backSet, 0, nullptr );
			v.vkCmdPushConstants( T.cmd, B.layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, std::uint32_t( sizeof( noJob ) ), noJob );
			v.vkCmdDispatch( T.cmd, std::uint32_t( ( B.back.w + 7 ) / 8 ), std::uint32_t( ( B.back.h + 7 ) / 8 ), 1 );
			mb.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
			mb.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			v.vkCmdPipelineBarrier( T.cmd, VK_PIPELINE_STAGE_TRANSFER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
				VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 1, &mb, 0, nullptr, 0, nullptr );
			v.vkCmdBeginRenderPass( T.cmd, &rbi, VK_SUBPASS_CONTENTS_INLINE );
			v.vkCmdSetViewport( T.cmd, 0, 1, &vp );
			v.vkCmdSetScissor( T.cmd, 0, 1, &sc );
			v.vkCmdBindDescriptorSets( T.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, T.layout, 0, 1, &T.set, 0, nullptr );
		}
		draw( 3, 3, 2, 1 );
		if ( T.splitSky )
			draw( 4, 3, 1, 1 );
	}
	v.vkCmdEndRenderPass( T.cmd );
	if ( v.vkEndCommandBuffer( T.cmd ) != VK_SUCCESS ) {
		why = "vkEndCommandBuffer (the tone pass) failed";
		return false;
	}
	return true;
}

// the tone pass alone between A and B
bool toneWork( std::string & why )
{
	Vk & v = vk();
	Tone & T = tone();
	if ( !toneRecord( why ) )
		return false;
	const VkPipelineStageFlags waitAt = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
	VkSubmitInfo si { VK_STRUCTURE_TYPE_SUBMIT_INFO };
	si.waitSemaphoreCount = 1;
	si.pWaitSemaphores = &T.glDone;
	si.pWaitDstStageMask = &waitAt;
	si.commandBufferCount = 1;
	si.pCommandBuffers = &T.cmd;
	si.signalSemaphoreCount = 1;
	si.pSignalSemaphores = &T.vkDone;
	const VkResult r = v.vkQueueSubmit( v.queue, 1, &si, T.fence );
	if ( r != VK_SUCCESS ) {
		why = "vkQueueSubmit (the tone pass) returned " + std::to_string( int( r ) );
		return false;
	}
	return true;
}

/* Lane VKSHARE1: the Modern temporal AA (gametaa.cpp, tier High) on the view. GL blits the frame's colour and depth into
 * shared targets (sharedTarget) and clears the lock on a history reset, as before; the crossing has the tone pass's
 * shape: GL signals A, the worker's ONE submit waits A, runs the camera vectors (compute), the resolve (compute) and the
 * RCAS copy-back (a raster pass into the view slot, every sample the pixel's value, as GL's quad), signals B; GL waits B
 * before its next pass on the view. */
struct Taa
{
	bool made = false, broken = false;
	std::string brokenWhy;
	VkSampler linear = VK_NULL_HANDLE, point = VK_NULL_HANDLE;
	VkDescriptorSetLayout rdsl = VK_NULL_HANDLE, pdsl = VK_NULL_HANDLE;  // the resolve's 11 bindings; the PostFx shim's 8
	VkDescriptorPool dpool = VK_NULL_HANDLE;
	VkDescriptorSet rset[2] = {}, mvSet = VK_NULL_HANDLE, rcasSet = VK_NULL_HANDLE;  // rset[wr]: writes history wr
	VkImageView rHeld[2][10] = {}, mvHeld[2] = {}, rcasHeld = VK_NULL_HANDLE;  // what each set holds
	std::uint64_t heldGen = 0;
	VkPipelineLayout rlayout = VK_NULL_HANDLE, playout = VK_NULL_HANDLE;
	VkShaderModule mvCs = VK_NULL_HANDLE, resolveCs = VK_NULL_HANDLE, vs = VK_NULL_HANDLE, ps = VK_NULL_HANDLE;
	VkPipeline mvPipe = VK_NULL_HANDLE, resolvePipe = VK_NULL_HANDLE;
	VkSemaphore glDone = VK_NULL_HANDLE, vkDone = VK_NULL_HANDLE;
	GLuint glDoneGl = 0, vkDoneGl = 0;
	void * semCtx = nullptr;
	VkCommandBuffer cmd = VK_NULL_HANDLE;
	VkFence fence = VK_NULL_HANDLE;
	bool pending = false;
	// per sample count: the copy-back's render pass and pipeline
	int pipeSamples = 0;
	VkRenderPass pass = VK_NULL_HANDLE;
	VkPipeline rcasPipe = VK_NULL_HANDLE;
	int pipelines = 0;
	double buildMs = 0.0;
	// per view slot (made again with the view ring)
	VkImageView vColor[kViewSlots] = {};
	VkFramebuffer fb[kViewSlots] = {};
	std::uint64_t fbGen = 0;
	// this frame's passes: written by the drawing thread before the hand, read by the worker
	int slot = -1, w = 0, h = 0, wr = 0;
	WwVkPresent::TaaJob job;
	WwVkPresent::PfxJob mvJob, rcasJob;
	WwVkPresent::TaaStat st;  // since the last take
	// lane VKPOST2: the colour from the view slot on Vulkan (merged crossing: GL skipped its colour blit), into cur
	bool fromView = false;
	VkImage curImg = VK_NULL_HANDLE;
};

Taa & taa()
{
	static Taa t;
	return t;
}

// bumped whenever a keyed shared target is made or goes: a descriptor set holding its view is written again
std::uint64_t & sharedGen()
{
	static std::uint64_t n = 1;
	return n;
}

void taaFbsDrop()
{
	Vk & v = vk();
	Taa & A = taa();
	for ( int k = 0; k < kViewSlots; k++ ) {
		if ( A.fb[k] )
			v.vkDestroyFramebuffer( v.dev, A.fb[k], nullptr );
		if ( A.vColor[k] )
			v.vkDestroyImageView( v.dev, A.vColor[k], nullptr );
		A.fb[k] = VK_NULL_HANDLE;
		A.vColor[k] = VK_NULL_HANDLE;
	}
	A.fbGen = 0;
}

/* the device-time parts (once): two samplers, the two set layouts, pool + 4 sets, two pipeline layouts, the shader
 * modules, the two compute pipelines, the command buffer and its fence. No submit. */
bool taaStatic( std::string & why )
{
	Vk & v = vk();
	Taa & A = taa();
	if ( A.made )
		return true;
	if ( !v.vkCmdDispatch || !v.vkCreateComputePipelines ) {
		why = "no vkCmdDispatch / vkCreateComputePipelines";
		return false;
	}
	const double t0 = nowMs();
	VkSamplerCreateInfo sci { VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
	sci.magFilter = sci.minFilter = VK_FILTER_LINEAR;
	sci.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
	sci.addressModeU = sci.addressModeV = sci.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	sci.maxLod = 0.0f;
	const bool linOk = v.vkCreateSampler( v.dev, &sci, nullptr, &A.linear ) == VK_SUCCESS;
	sci.magFilter = sci.minFilter = VK_FILTER_NEAREST;
	if ( !linOk || v.vkCreateSampler( v.dev, &sci, nullptr, &A.point ) != VK_SUCCESS ) {
		why = "vkCreateSampler (the temporal AA) failed";
		return false;
	}
	// TaaBindingsVk.hlsli: 0-6 sampled, 7 sampler, 8-10 storage; PostFxBindingsVk.hlsli: 0-4 sampled, 5-6 samplers, 7 storage
	VkDescriptorSetLayoutBinding rb[11] = {}, pb[8] = {};
	for ( std::uint32_t k = 0; k < 11; k++ ) {
		rb[k].binding = k;
		rb[k].descriptorType = k < 7 ? VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE : k == 7 ? VK_DESCRIPTOR_TYPE_SAMPLER : VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		rb[k].descriptorCount = 1;
		rb[k].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	}
	for ( std::uint32_t k = 0; k < 8; k++ ) {
		pb[k].binding = k;
		pb[k].descriptorType = k < 5 ? VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE : k < 7 ? VK_DESCRIPTOR_TYPE_SAMPLER : VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		pb[k].descriptorCount = 1;
		pb[k].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
	}
	VkDescriptorSetLayoutCreateInfo lci { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
	lci.bindingCount = 11;
	lci.pBindings = rb;
	const bool rOk = v.vkCreateDescriptorSetLayout( v.dev, &lci, nullptr, &A.rdsl ) == VK_SUCCESS;
	lci.bindingCount = 8;
	lci.pBindings = pb;
	const bool pOk = rOk && v.vkCreateDescriptorSetLayout( v.dev, &lci, nullptr, &A.pdsl ) == VK_SUCCESS;
	VkDescriptorPoolSize ps[3] = { { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 2 * 7 + 2 * 5 }, { VK_DESCRIPTOR_TYPE_SAMPLER, 2 * 1 + 2 * 2 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 2 * 3 + 2 * 1 } };
	VkDescriptorPoolCreateInfo pci { VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
	pci.maxSets = 4;
	pci.poolSizeCount = 3;
	pci.pPoolSizes = ps;
	if ( !pOk || v.vkCreateDescriptorPool( v.dev, &pci, nullptr, &A.dpool ) != VK_SUCCESS ) {
		why = "the temporal AA's descriptor set layouts / pool failed";
		return false;
	}
	const VkDescriptorSetLayout layouts[4] = { A.rdsl, A.rdsl, A.pdsl, A.pdsl };
	VkDescriptorSet sets[4] = {};
	VkDescriptorSetAllocateInfo dai { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
	dai.descriptorPool = A.dpool;
	dai.descriptorSetCount = 4;
	dai.pSetLayouts = layouts;
	const VkPushConstantRange rpc { VK_SHADER_STAGE_COMPUTE_BIT, 0, std::uint32_t( sizeof( WwVkPresent::TaaJob ) ) };
	const VkPushConstantRange ppc { VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
		std::uint32_t( sizeof( WwVkPresent::PfxJob ) ) };
	VkPipelineLayoutCreateInfo plci { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
	plci.setLayoutCount = 1;
	plci.pSetLayouts = &A.rdsl;
	plci.pushConstantRangeCount = 1;
	plci.pPushConstantRanges = &rpc;
	const bool lOk = v.vkAllocateDescriptorSets( v.dev, &dai, sets ) == VK_SUCCESS
		&& v.vkCreatePipelineLayout( v.dev, &plci, nullptr, &A.rlayout ) == VK_SUCCESS;
	plci.pSetLayouts = &A.pdsl;
	plci.pPushConstantRanges = &ppc;
	if ( !lOk || v.vkCreatePipelineLayout( v.dev, &plci, nullptr, &A.playout ) != VK_SUCCESS ) {
		why = "the temporal AA's descriptor sets / pipeline layouts failed";
		return false;
	}
	A.rset[0] = sets[0];
	A.rset[1] = sets[1];
	A.mvSet = sets[2];
	A.rcasSet = sets[3];
	auto module = [&]( const std::uint32_t * code, size_t bytes, VkShaderModule & out ) {
		VkShaderModuleCreateInfo mci { VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
		mci.codeSize = bytes;
		mci.pCode = code;
		return v.vkCreateShaderModule( v.dev, &mci, nullptr, &out ) == VK_SUCCESS;
	};
	if ( !module( vkframe_taa_view_mv_cs_spv, sizeof( vkframe_taa_view_mv_cs_spv ), A.mvCs )
		|| !module( vkframe_taa_view_resolve_cs_spv, sizeof( vkframe_taa_view_resolve_cs_spv ), A.resolveCs )
		|| !module( vkframe_tone_view_vs_spv, sizeof( vkframe_tone_view_vs_spv ), A.vs )
		|| !module( vkframe_taa_view_rcas_ps_spv, sizeof( vkframe_taa_view_rcas_ps_spv ), A.ps ) ) {
		why = "the temporal AA's shader modules failed";
		return false;
	}
	VkComputePipelineCreateInfo cp[2] = { { VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO }, { VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO } };
	for ( int k = 0; k < 2; k++ ) {
		cp[k].stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		cp[k].stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
		cp[k].stage.module = k ? A.resolveCs : A.mvCs;
		cp[k].stage.pName = "main";
		cp[k].layout = k ? A.rlayout : A.playout;
	}
	VkPipeline pipes[2] = {};
	if ( v.vkCreateComputePipelines( v.dev, tone().cache, 2, cp, nullptr, pipes ) != VK_SUCCESS ) {
		why = "vkCreateComputePipelines (the temporal AA's vectors / resolve) failed";
		return false;
	}
	A.mvPipe = pipes[0];
	A.resolvePipe = pipes[1];
	VkFenceCreateInfo fi { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
	VkCommandBufferAllocateInfo cai { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
	cai.commandPool = v.pool;
	cai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	cai.commandBufferCount = 1;
	if ( v.vkCreateFence( v.dev, &fi, nullptr, &A.fence ) != VK_SUCCESS || v.vkAllocateCommandBuffers( v.dev, &cai, &A.cmd ) != VK_SUCCESS ) {
		why = "the temporal AA's fence / command buffer failed";
		return false;
	}
	A.pipelines = 2;
	A.buildMs = nowMs() - t0;
	toneCacheSave();
	A.made = true;
	return true;
}

// the crossing's two semaphores go (GL alive: GL's names too); the caller waited the device
void taaSemsDrop( bool glAlive )
{
	Vk & v = vk();
	Gl & g = gl();
	Taa & A = taa();
	if ( glAlive )
		for ( GLuint * s : { &A.glDoneGl, &A.vkDoneGl } )
			if ( *s )
				g.deleteSemaphores( 1, s );
	for ( VkSemaphore * s : { &A.glDone, &A.vkDone } )
		if ( *s )
			v.vkDestroySemaphore( v.dev, *s, nullptr );
	A.glDoneGl = A.vkDoneGl = 0;
	A.glDone = A.vkDone = VK_NULL_HANDLE;
	A.semCtx = nullptr;
}

bool taaSems( std::string & why )
{
	Gl & g = gl();
	Taa & A = taa();
	if ( A.glDone && A.semCtx == g.ctx )
		return true;
	taaSemsDrop( false );  // another context's names: that context went
	if ( !exportSem( A.glDone, A.glDoneGl, why ) || !exportSem( A.vkDone, A.vkDoneGl, why ) ) {
		taaSemsDrop( true );
		return false;
	}
	A.semCtx = g.ctx;
	return true;
}

// the copy-back's render pass (one RGBA8 colour, LOAD / STORE, GENERAL) and pipeline at the view's samples
bool taaPipes( int samples, std::string & why )
{
	Vk & v = vk();
	Taa & A = taa();
	if ( A.pass && A.pipeSamples == samples )
		return true;
	taaFbsDrop();
	if ( A.rcasPipe )
		v.vkDestroyPipeline( v.dev, A.rcasPipe, nullptr );
	if ( A.pass )
		v.vkDestroyRenderPass( v.dev, A.pass, nullptr );
	A.rcasPipe = VK_NULL_HANDLE;
	A.pass = VK_NULL_HANDLE;
	A.pipeSamples = 0;
	const double t0 = nowMs();
	const VkSampleCountFlagBits sc = VkSampleCountFlagBits( samples );
	VkAttachmentDescription at {};
	at.format = VK_FORMAT_R8G8B8A8_UNORM;
	at.samples = sc;
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
	if ( v.vkCreateRenderPass( v.dev, &rci, nullptr, &A.pass ) != VK_SUCCESS ) {
		why = "vkCreateRenderPass (the temporal AA's copy-back) failed";
		return false;
	}
	VkPipelineShaderStageCreateInfo st[2] = { { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO },
		{ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO } };
	st[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
	st[0].module = A.vs;
	st[0].pName = "main";
	st[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	st[1].module = A.ps;
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
	ms.rasterizationSamples = sc;  // once a pixel: every covered sample takes the value (GL's copy quad)
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
	gp.pColorBlendState = &cbs;
	gp.pDynamicState = &dy;
	gp.layout = A.playout;
	gp.renderPass = A.pass;
	if ( v.vkCreateGraphicsPipelines( v.dev, tone().cache, 1, &gp, nullptr, &A.rcasPipe ) != VK_SUCCESS ) {
		why = "vkCreateGraphicsPipelines (the temporal AA's copy-back, " + std::to_string( samples ) + " samples) failed";
		return false;
	}
	A.pipeSamples = samples;
	A.pipelines = 3;
	A.buildMs += nowMs() - t0;
	toneCacheSave();
	return true;
}

// the copy-back's framebuffers over the view ring's colour (made again when the ring is)
bool taaFbs( std::string & why )
{
	Vk & v = vk();
	Taa & A = taa();
	View & V = view();
	if ( A.fb[0] && A.fbGen == V.gen )
		return true;
	taaFbsDrop();
	for ( int k = 0; k < kViewSlots; k++ ) {
		A.vColor[k] = imageView( V.slot[k].color, VK_IMAGE_VIEW_TYPE_2D, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT );
		VkFramebufferCreateInfo fci { VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
		fci.renderPass = A.pass;
		fci.attachmentCount = 1;
		fci.pAttachments = &A.vColor[k];
		fci.width = std::uint32_t( V.w );
		fci.height = std::uint32_t( V.h );
		fci.layers = 1;
		if ( !A.vColor[k] || v.vkCreateFramebuffer( v.dev, &fci, nullptr, &A.fb[k] ) != VK_SUCCESS ) {
			taaFbsDrop();
			why = "the temporal AA's framebuffer over a view failed";
			return false;
		}
	}
	A.fbGen = V.gen;
	return true;
}

// lane VKPOST2: harness only; 1 = the merged crossing skips the view's colour into cur (the gate's red, must FAIL)
int mergeRedTest = 0;

// the worker: the vectors, the resolve and the copy-back into this frame's view slot (taaWork / mergeWork submit it)
bool taaRecord( std::string & why )
{
	Vk & v = vk();
	Taa & A = taa();
	if ( A.slot < 0 || !A.fb[A.slot] ) {
		why = "no view slot to draw into";
		return false;
	}
	v.vkResetFences( v.dev, 1, &A.fence );
	v.vkResetCommandBuffer( A.cmd, 0 );
	VkCommandBufferBeginInfo bi { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
	bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	v.vkBeginCommandBuffer( A.cmd, &bi );
	if ( A.fromView && mergeRedTest != 1 ) {
		/* 1. the frame's colour: the view slot (the tone pass just drew it, earlier in submission order) resolved into cur,
		 * what GL's colour blit did (glBlitFramebuffer MS -> 1 sample, NEAREST, the whole view) */
		const ViewSlot & vsl = view().slot[A.slot];
		VkMemoryBarrier in { VK_STRUCTURE_TYPE_MEMORY_BARRIER };
		in.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
		in.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
		v.vkCmdPipelineBarrier( A.cmd, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 1, &in, 0, nullptr, 0, nullptr );
		if ( view().samples > 1 ) {
			VkImageResolve r {};
			r.srcSubresource = r.dstSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
			r.extent = { std::uint32_t( A.w ), std::uint32_t( A.h ), 1 };
			v.vkCmdResolveImage( A.cmd, vsl.color, VK_IMAGE_LAYOUT_GENERAL, A.curImg, VK_IMAGE_LAYOUT_GENERAL, 1, &r );
		} else {
			VkImageBlit b {};
			b.srcSubresource = b.dstSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
			b.srcOffsets[1] = b.dstOffsets[1] = { A.w, A.h, 1 };
			v.vkCmdBlitImage( A.cmd, vsl.color, VK_IMAGE_LAYOUT_GENERAL, A.curImg, VK_IMAGE_LAYOUT_GENERAL, 1, &b, VK_FILTER_NEAREST );
		}
		VkMemoryBarrier out { VK_STRUCTURE_TYPE_MEMORY_BARRIER };
		out.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		out.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		v.vkCmdPipelineBarrier( A.cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &out, 0, nullptr, 0, nullptr );
	}
	const std::uint32_t gx = std::uint32_t( ( A.w + 7 ) / 8 ), gy = std::uint32_t( ( A.h + 7 ) / 8 );
	VkMemoryBarrier mb { VK_STRUCTURE_TYPE_MEMORY_BARRIER };
	mb.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
	mb.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
	// 2. the camera vectors (game_taa_mv.frag's twin) from the shared depth
	v.vkCmdBindPipeline( A.cmd, VK_PIPELINE_BIND_POINT_COMPUTE, A.mvPipe );
	v.vkCmdBindDescriptorSets( A.cmd, VK_PIPELINE_BIND_POINT_COMPUTE, A.playout, 0, 1, &A.mvSet, 0, nullptr );
	v.vkCmdPushConstants( A.cmd, A.playout, VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
		std::uint32_t( sizeof( A.mvJob ) ), &A.mvJob );
	v.vkCmdDispatch( A.cmd, gx, gy, 1 );
	v.vkCmdPipelineBarrier( A.cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &mb, 0, nullptr, 0, nullptr );
	// 3b. the resolve (taa2.frag's twin): the next history, the picture, the lock
	v.vkCmdBindPipeline( A.cmd, VK_PIPELINE_BIND_POINT_COMPUTE, A.resolvePipe );
	v.vkCmdBindDescriptorSets( A.cmd, VK_PIPELINE_BIND_POINT_COMPUTE, A.rlayout, 0, 1, &A.rset[A.wr], 0, nullptr );
	v.vkCmdPushConstants( A.cmd, A.rlayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, std::uint32_t( sizeof( A.job ) ), &A.job );
	v.vkCmdDispatch( A.cmd, gx, gy, 1 );
	v.vkCmdPipelineBarrier( A.cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 1, &mb, 0, nullptr, 0, nullptr );
	// 4b. the picture back through RCAS into the view (taa2_rcas.frag's quad)
	View & V = view();
	VkRenderPassBeginInfo rbi { VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
	rbi.renderPass = A.pass;
	rbi.framebuffer = A.fb[A.slot];
	rbi.renderArea = { { 0, 0 }, { std::uint32_t( V.w ), std::uint32_t( V.h ) } };
	v.vkCmdBeginRenderPass( A.cmd, &rbi, VK_SUBPASS_CONTENTS_INLINE );
	const VkViewport vp { 0.0f, 0.0f, float( A.w ), float( A.h ), 0.0f, 1.0f };
	const VkRect2D sc { { 0, 0 }, { std::uint32_t( A.w ), std::uint32_t( A.h ) } };
	v.vkCmdSetViewport( A.cmd, 0, 1, &vp );
	v.vkCmdSetScissor( A.cmd, 0, 1, &sc );
	v.vkCmdBindPipeline( A.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, A.rcasPipe );
	v.vkCmdBindDescriptorSets( A.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, A.playout, 0, 1, &A.rcasSet, 0, nullptr );
	v.vkCmdPushConstants( A.cmd, A.playout, VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
		std::uint32_t( sizeof( A.rcasJob ) ), &A.rcasJob );
	v.vkCmdDraw( A.cmd, 3, 1, 0, 0 );
	v.vkCmdEndRenderPass( A.cmd );
	if ( v.vkEndCommandBuffer( A.cmd ) != VK_SUCCESS ) {
		why = "vkEndCommandBuffer (the temporal AA) failed";
		return false;
	}
	return true;
}

// the TAA alone between A and B
bool taaWork( std::string & why )
{
	Vk & v = vk();
	Taa & A = taa();
	if ( !taaRecord( why ) )
		return false;
	const VkPipelineStageFlags waitAt = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
	VkSubmitInfo si { VK_STRUCTURE_TYPE_SUBMIT_INFO };
	si.waitSemaphoreCount = 1;
	si.pWaitSemaphores = &A.glDone;
	si.pWaitDstStageMask = &waitAt;
	si.commandBufferCount = 1;
	si.pCommandBuffers = &A.cmd;
	si.signalSemaphoreCount = 1;
	si.pSignalSemaphores = &A.vkDone;
	const VkResult r = v.vkQueueSubmit( v.queue, 1, &si, A.fence );
	if ( r != VK_SUCCESS ) {
		why = "vkQueueSubmit (the temporal AA) returned " + std::to_string( int( r ) );
		return false;
	}
	return true;
}

/* lane VKPOST2: the deferred tone pass and the TAA in ONE crossing. Submit 1 waits the TAA's GL signal (it covers the
 * tone frame's names too) and runs the tone pass (its own fence); submit 2 resolves the view into cur behind a full
 * barrier (taaRecord, fromView), runs the TAA and signals B. subOk tells the drawing thread the tone fence is armed. */
bool mergeWork( std::string & why )
{
	Vk & v = vk();
	Tone & T = tone();
	Taa & A = taa();
	T.subOk = false;
	if ( !toneRecord( why ) || !taaRecord( why ) )
		return false;
	const VkPipelineStageFlags waitAt = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
	VkSubmitInfo si { VK_STRUCTURE_TYPE_SUBMIT_INFO };
	si.waitSemaphoreCount = 1;
	si.pWaitSemaphores = &A.glDone;
	si.pWaitDstStageMask = &waitAt;
	si.commandBufferCount = 1;
	si.pCommandBuffers = &T.cmd;
	VkResult r = v.vkQueueSubmit( v.queue, 1, &si, T.fence );
	if ( r != VK_SUCCESS ) {
		why = "vkQueueSubmit (the merged crossing's tone pass) returned " + std::to_string( int( r ) );
		return false;
	}
	T.subOk = true;
	VkSubmitInfo sa { VK_STRUCTURE_TYPE_SUBMIT_INFO };
	sa.commandBufferCount = 1;
	sa.pCommandBuffers = &A.cmd;
	sa.signalSemaphoreCount = 1;
	sa.pSignalSemaphores = &A.vkDone;
	r = v.vkQueueSubmit( v.queue, 1, &sa, A.fence );
	if ( r != VK_SUCCESS ) {
		why = "vkQueueSubmit (the merged crossing's TAA) returned " + std::to_string( int( r ) );
		return false;
	}
	return true;
}

bool makeSwap( int w, int h, std::string & why )
{
	Vk & v = vk();
	Chain & c = ch();
	VkBool32 can = VK_FALSE;
	v.vkGetPhysicalDeviceSurfaceSupportKHR( v.phys, v.family, c.surface, &can );
	if ( !can ) {
		why = "the graphics family cannot present to the window";
		return false;
	}
	VkSurfaceCapabilitiesKHR caps {};
	v.vkGetPhysicalDeviceSurfaceCapabilitiesKHR( v.phys, c.surface, &caps );
	if ( ( caps.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT ) == 0 ) {
		why = "the swapchain images take no transfer writes (the blit)";
		return false;
	}
	std::uint32_t nf = 0;
	v.vkGetPhysicalDeviceSurfaceFormatsKHR( v.phys, c.surface, &nf, nullptr );
	std::vector<VkSurfaceFormatKHR> fs( nf );
	v.vkGetPhysicalDeviceSurfaceFormatsKHR( v.phys, c.surface, &nf, fs.data() );
	// UNORM, as GL's own back buffer: the bytes go to the screen as GL wrote them (no sRGB encode on the way)
	VkSurfaceFormatKHR pick { VK_FORMAT_UNDEFINED, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR };
	for ( VkFormat want : { VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_R8G8B8A8_UNORM } )
		for ( const VkSurfaceFormatKHR & f : fs )
			if ( pick.format == VK_FORMAT_UNDEFINED && f.format == want && f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR )
				pick = f;
	if ( pick.format == VK_FORMAT_UNDEFINED ) {
		why = "the window offers no 8-bit UNORM swapchain format";
		return false;
	}
	VkFormatProperties fp {};
	v.vkGetPhysicalDeviceFormatProperties( v.phys, pick.format, &fp );
	if ( ( fp.optimalTilingFeatures & VK_FORMAT_FEATURE_BLIT_DST_BIT ) == 0 ) {
		why = "the swapchain format takes no blit";
		return false;
	}
	std::uint32_t nm = 0;
	v.vkGetPhysicalDeviceSurfacePresentModesKHR( v.phys, c.surface, &nm, nullptr );
	std::vector<VkPresentModeKHR> ms( nm );
	v.vkGetPhysicalDeviceSurfacePresentModesKHR( v.phys, c.surface, &nm, ms.data() );
	auto has = [&]( VkPresentModeKHR m ) { return std::find( ms.begin(), ms.end(), m ) != ms.end(); };
	c.mode = VK_PRESENT_MODE_FIFO_KHR;
	if ( !c.vsync ) {
		if ( has( VK_PRESENT_MODE_MAILBOX_KHR ) )
			c.mode = VK_PRESENT_MODE_MAILBOX_KHR;
		else if ( has( VK_PRESENT_MODE_IMMEDIATE_KHR ) )
			c.mode = VK_PRESENT_MODE_IMMEDIATE_KHR;
	}
	std::uint32_t count = std::max( caps.minImageCount + 1, 4u );	// 4: MAILBOX's acquire never waits on a held image
	if ( caps.maxImageCount )
		count = std::min( count, caps.maxImageCount );
	VkExtent2D ext { std::uint32_t( w ), std::uint32_t( h ) };
	if ( caps.currentExtent.width != UINT32_MAX )
		ext = caps.currentExtent;
	if ( !ext.width || !ext.height ) {
		why = "the window has no area";
		return false;
	}
	VkSwapchainKHR old = VK_NULL_HANDLE;
	dropSwap( true, &old );
	VkSwapchainCreateInfoKHR sci { VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR };
	sci.surface = c.surface;
	sci.minImageCount = count;
	sci.imageFormat = pick.format;
	sci.imageColorSpace = pick.colorSpace;
	sci.imageExtent = ext;
	sci.imageArrayLayers = 1;
	sci.imageUsage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | ( caps.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_SRC_BIT );
	sci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	sci.preTransform = caps.currentTransform;
	sci.compositeAlpha = ( caps.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR ) ? VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR
		: VkCompositeAlphaFlagBitsKHR( caps.supportedCompositeAlpha & ( ~caps.supportedCompositeAlpha + 1 ) );
	sci.presentMode = c.mode;
	sci.clipped = VK_TRUE;
	sci.oldSwapchain = old;
	const VkResult r = v.vkCreateSwapchainKHR( v.dev, &sci, nullptr, &c.swap );
	if ( old )
		v.vkDestroySwapchainKHR( v.dev, old, nullptr );
	if ( r != VK_SUCCESS ) {
		why = "vkCreateSwapchainKHR failed (" + std::to_string( int( r ) ) + ")";
		c.swap = VK_NULL_HANDLE;
		return false;
	}
	c.format = pick.format;
	c.extent = ext;
	std::uint32_t ni = 0;
	v.vkGetSwapchainImagesKHR( v.dev, c.swap, &ni, nullptr );
	c.images.resize( ni );
	v.vkGetSwapchainImagesKHR( v.dev, c.swap, &ni, c.images.data() );
	VkSemaphoreCreateInfo si { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
	c.renderDone.assign( ni, VK_NULL_HANDLE );
	for ( VkSemaphore & s : c.renderDone )
		if ( v.vkCreateSemaphore( v.dev, &si, nullptr, &s ) != VK_SUCCESS ) {
			why = "vkCreateSemaphore (render done) failed";
			return false;
		}
	stat().builds++;
	return true;
}

// the child window, surface, swapchain and ring at w x h; rebuilt on a new size, window, GL context or vsync
bool ensure( HWND parent, int w, int h, bool vsync, std::string & why )
{
	Vk & v = vk();
	Chain & c = ch();
	Gl & g = gl();
	void * ctx = g.ctx;
	if ( c.parent && ( c.parent != parent || c.glCtx != ctx || !IsWindow( c.child ) ) )
		dropAll( c.glCtx == ctx );
	if ( !c.child ) {
		c.parent = parent;
		c.glCtx = ctx;
		c.child = makeChild( parent, w, h );
		if ( !c.child ) {
			why = "the present child window was not made (error " + std::to_string( GetLastError() ) + ")";
			return false;
		}
		c.rebuild = true;
	}
	if ( c.w != w || c.h != h || c.vsync != vsync || !c.swap )
		c.rebuild = true;
	if ( !c.rebuild )
		return true;
	drain();
	v.vkDeviceWaitIdle( v.dev );
	SetWindowPos( c.child, HWND_TOP, 0, 0, w, h, SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_SHOWWINDOW );
	c.vsync = vsync;
	if ( c.w != w || c.h != h || !c.slot[0].img ) {
		dropSlots( true );
		for ( Slot & s : c.slot )
			if ( !makeSlot( s, w, h, why ) )
				return false;
	}
	c.w = w;
	c.h = h;
	if ( !c.surface ) {
		VkWin32SurfaceCreateInfoKHR sci { VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR };
		sci.hinstance = GetModuleHandleW( nullptr );
		sci.hwnd = c.child;
		if ( v.vkCreateWin32SurfaceKHR( v.inst, &sci, nullptr, &c.surface ) != VK_SUCCESS ) {
			why = "vkCreateWin32SurfaceKHR failed";
			return false;
		}
	}
	if ( !makeSwap( w, h, why ) )
		return false;
	c.rebuild = false;
	return true;
}

// GL's entry points for the context current (loaded again when it changes)
bool glCurrent( WwVkPresent::GlProc gp, std::string & why )
{
	Gl & g = gl();
	void * ctx = reinterpret_cast<void *>( wglGetCurrentContext() );
	if ( !ctx ) {
		why = "no GL context current";
		return false;
	}
	if ( g.ctx != ctx ) {
		Gl fresh;
		if ( !loadGl( fresh, gp, why ) )
			return false;
		fresh.ctx = ctx;
		g = fresh;
	}
	return true;
}

}  // namespace

namespace WwVkPresent
{

const Stat & last()
{
	return stat();
}

void captureNext( std::vector<std::uint8_t> * out )
{
	drain();  // the worker reads capture in vkPart
	ch().capture = out;
}

void shutdown()
{
	tone().deferred = false;  // lane VKPOST2: a deferred pass is dropped with its frame
	if ( ch().parent || ch().child )
		dropAll( gl().ok );
	if ( view().glCtx ) {
		drain();
		vk().vkDeviceWaitIdle( vk().dev );
		dropView( gl().ok && view().glCtx == gl().ctx );
	}
	// round 11: the tone frame and the crossing's semaphores (GL's names); the pipelines and the LUT stay with the device
	Tone & T = tone();
	if ( T.color || T.colorTex || T.glDone ) {
		drain();
		vk().vkDeviceWaitIdle( vk().dev );
		T.pending = false;
		toneFrameFree( gl().ok && T.glCtx == gl().ctx );
		toneSemsDrop( gl().ok && T.semCtx == gl().ctx );
	}
	// lane VKSHARE1: the keyed shared targets (their owners see sharedTargetIs false and make GL's own again)
	if ( !shareds().empty() ) {
		drain();
		vk().vkDeviceWaitIdle( vk().dev );
		for ( auto & kv : shareds() )
			sharedFree( kv.second, gl().ok && kv.second.glCtx == gl().ctx );
		shareds().clear();
		sharedStat().mb = 0.0;
		sharedGen()++;
	}
	// the temporal AA's crossing semaphores (GL's names); its pipelines stay with the device
	Taa & A = taa();
	if ( A.glDone || A.pending ) {
		drain();
		vk().vkDeviceWaitIdle( vk().dev );
		A.pending = false;
		taaSemsDrop( gl().ok && A.semCtx == gl().ctx );
	}
	skyDrop();  // lane VKSKY1: the sky's crossing semaphores and framebuffer
}

bool viewBegin( int w, int h, int samples, GlProc gp, std::string & why )
{
	View & V = view();
	V.cur = -1;
	if ( w < 1 || h < 1 ) {
		why = "the window has no area";
		return false;
	}
	samples = std::max( samples, 1 );
	if ( !glCurrent( gp, why ) || !makeDevice( why ) )
		return false;
	Gl & g = gl();
	if ( !V.failWhy.empty() && V.failW == w && V.failH == h && V.failS == samples ) {
		why = V.failWhy;
		return false;
	}
	if ( V.w != w || V.h != h || V.samples != samples || !V.fbo || V.glCtx != g.ctx ) {
		if ( !makeView( w, h, samples, why ) ) {
			V.failWhy = why;
			V.failW = w;
			V.failH = h;
			V.failS = samples;
			return false;
		}
		V.failWhy.clear();
	}
	const int vs = int( V.frame++ % kViewSlots );
	ViewSlot & s = V.slot[vs];
	// the present that read this view last (kViewSlots frames back): submitted by the worker, then its fence
	if ( s.reader >= 0 ) {
		const double tw = nowMs();
		waitSlot( s.reader );
		Vk & v = vk();
		Slot & p = ch().slot[s.reader];
		if ( p.pending && p.fence && v.vkGetFenceStatus( v.dev, p.fence ) == VK_NOT_READY ) {
			V.busy++;
			v.vkWaitForFences( v.dev, 1, &p.fence, VK_TRUE, 1000000000ull );
		}
		V.waitMs += nowMs() - tw;
		s.reader = -1;
	}
	const GLenum target = V.samples > 1 ? kTexture2DMs : kTexture2D;
	g.bindFramebuffer( kFramebuffer, V.fbo );
	g.framebufferTexture2D( kFramebuffer, kColorAttach0, target, s.colorTex, 0 );
	g.framebufferTexture2D( kFramebuffer, kDepthStencilAttach, target, s.dsTex, 0 );
	V.cur = vs;
	return true;
}

unsigned viewFbo()
{
	return view().cur >= 0 ? view().fbo : 0u;
}

bool viewRescue()
{
	toneFlush( nullptr );  // lane VKPOST2: a deferred tone pass goes before the view is read
	View & V = view();
	Gl & g = gl();
	if ( V.cur < 0 || !V.fbo || !g.ok )
		return false;
	V.cur = -1;
	g.disable( kScissorTest );
	g.bindFramebuffer( kReadFb, V.fbo );
	g.bindFramebuffer( kDrawFb, 0 );
	g.blitFramebuffer( 0, 0, V.w, V.h, 0, 0, V.w, V.h, kColorBit, kNearest );
	g.bindFramebuffer( kFramebuffer, 0 );
	return true;
}

ViewStat takeViewStat()
{
	View & V = view();
	ViewStat s;
	s.on = V.fbo != 0;
	s.w = V.w;
	s.h = V.h;
	s.samples = V.samples;
	s.views = kViewSlots;
	s.mb = V.mb;
	s.makeMs = V.makeMs;
	s.busy = V.busy;
	s.waitMs = V.waitMs;
	s.why = V.failWhy;
	V.busy = 0;
	V.waitMs = 0.0;
	return s;
}

void viewDrop()
{
	View & V = view();
	if ( !V.glCtx )
		return;
	drain();
	vk().vkDeviceWaitIdle( vk().dev );
	dropView( gl().ok && V.glCtx == gl().ctx );
}

bool toneFrame( int w, int h, int samples, unsigned & color, unsigned & ds, std::string & why )
{
	Tone & T = tone();
	Gl & g = gl();
	Vk & v = vk();
	color = ds = 0;
	if ( !v.ok || !g.ok || g.ctx != reinterpret_cast<void *>( wglGetCurrentContext() ) ) {
		why = "the present device or GL's entry points are not ready (the view is not on Vulkan in this context)";
		return false;
	}
	if ( T.broken ) {
		why = T.brokenWhy;
		return false;
	}
	samples = std::max( samples, 1 );
	if ( samples < 2 ) {
		why = "1 sample: the pass reads the frame as Texture2DMS (a multisample image only)";
		return false;
	}
	if ( samples > 1 && !g.texStorageMem2DMultisample ) {
		why = "no glTexStorageMem2DMultisampleEXT";
		return false;
	}
	drain();  // the first fills below are submits; the queue is the worker's
	v.vkDeviceWaitIdle( v.dev );
	T.pending = false;
	toneFrameFree( T.glCtx == g.ctx );
	if ( !toneStatic( why ) || !toneSems( why ) )
		return false;
	T.glCtx = g.ctx;
	if ( !makeShared( T.color, T.colorMem, T.colorTex, T.colorObj, VK_FORMAT_R16G16B16A16_SFLOAT, kRgba16f, samples, w, h, false, T.frameMb, why )
		|| !makeShared( T.ds, T.dsMem, T.dsTex, T.dsObj, VK_FORMAT_D24_UNORM_S8_UINT, kDepth24Stencil8, samples, w, h, true, T.frameMb, why ) ) {
		toneFrameFree( true );
		return false;
	}
	// the stencil as GL's cell_hdr.frag reads it (usampler2DMS), and the pass's two views of the frame
	GLint was = 0;
	g.getIntegerv( kTexBinding2DMs, &was );
	g.bindTexture( kTexture2DMs, T.dsTex );
	g.texParameteri( kTexture2DMs, kDsTexMode, GLint( kStencilIndex ) );
	g.bindTexture( kTexture2DMs, GLuint( was ) );
	T.colorView = imageView( T.color, VK_IMAGE_VIEW_TYPE_2D, VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_ASPECT_COLOR_BIT );
	T.stencilView = imageView( T.ds, VK_IMAGE_VIEW_TYPE_2D, VK_FORMAT_D24_UNORM_S8_UINT, VK_IMAGE_ASPECT_STENCIL_BIT );
	const GLenum err = g.getError();
	if ( !T.colorView || !T.stencilView || err != 0 ) {
		why = "the tone frame's views / stencil mode failed (GL error " + std::to_string( err ) + ")";
		toneFrameFree( true );
		return false;
	}
	T.w = w;
	T.h = h;
	T.samples = samples;
	T.descDirty = true;
	color = T.colorTex;
	ds = T.dsTex;
	return true;
}

bool toneFrameIs( unsigned color )
{
	return color && tone().colorTex == color && tone().glCtx == gl().ctx;
}

void toneFrameDrop()
{
	Tone & T = tone();
	T.deferred = false;  // lane VKPOST2: the frame it would draw from goes
	if ( !T.color && !T.colorTex )
		return;
	drain();
	vk().vkDeviceWaitIdle( vk().dev );
	T.pending = false;
	toneFrameFree( gl().ok && T.glCtx == gl().ctx );
}

bool sharedTarget( std::uintptr_t key, int type, int w, int h, int d, unsigned glFmt, unsigned & tex, std::string & why )
{
	Gl & g = gl();
	Vk & v = vk();
	SharedStat & st = sharedStat();
	tex = 0;
	auto fail = [&]( const std::string & r ) {
		why = r;
		st.fails++;
		st.failWhy = r;
		return false;
	};
	if ( !v.ok || !g.ok || g.ctx != reinterpret_cast<void *>( wglGetCurrentContext() ) )
		return fail( "the present device or GL's entry points are not ready in this context" );
	if ( w < 1 || h < 1 || type < 0 || type > 2 )
		return fail( "a shared target of size " + std::to_string( w ) + "x" + std::to_string( h ) + " type " + std::to_string( type ) );
	bool depth = false;
	const VkFormat fmt = vkFormatOf( GLenum( glFmt ), depth );
	if ( fmt == VK_FORMAT_UNDEFINED )
		return fail( "GL format " + std::to_string( glFmt ) + " has no shared twin" );
	d = type == 0 ? 1 : std::max( d, 1 );
	auto it = shareds().find( key );
	if ( it != shareds().end() ) {
		Shared & s = it->second;
		if ( s.glCtx == g.ctx && s.type == type && s.w == w && s.h == h && s.d == d && s.glFmt == glFmt ) {
			tex = s.tex;
			return true;
		}
	}
	drain();  // the first layout change below is a submit; the queue is the worker's
	v.vkDeviceWaitIdle( v.dev );
	if ( it != shareds().end() ) {
		st.mb -= it->second.mb;
		sharedFree( it->second, it->second.glCtx == g.ctx );
		shareds().erase( it );
		st.dropped++;
	}
	Shared s;
	const VkImageType vt = type == 1 ? VK_IMAGE_TYPE_3D : VK_IMAGE_TYPE_2D;
	if ( !makeShared( s.img, s.mem, s.tex, s.obj, fmt, GLenum( glFmt ), 1, w, h, depth, s.mb, why, vt, type == 1 ? d : 1, type == 2 ? d : 1 ) ) {
		sharedFree( s, true );
		return fail( why );
	}
	const VkImageViewType viewType = type == 1 ? VK_IMAGE_VIEW_TYPE_3D : type == 2 ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D;
	s.view = imageView( s.img, viewType, fmt, depth ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT );
	if ( !s.view ) {
		sharedFree( s, true );
		return fail( "a shared target's image view failed" );
	}
	s.glCtx = g.ctx;
	s.type = type;
	s.w = w;
	s.h = h;
	s.d = d;
	s.glFmt = GLenum( glFmt );
	s.fmt = fmt;
	st.made++;
	st.mb += s.mb;
	tex = s.tex;
	shareds()[key] = s;
	sharedGen()++;
	return true;
}

bool sharedTargetIs( std::uintptr_t key, unsigned tex )
{
	const Shared * s = sharedFind( key );
	return s && tex && s->tex == tex;
}

void sharedTargetDrop( std::uintptr_t key )
{
	auto it = shareds().find( key );
	if ( it == shareds().end() )
		return;
	drain();
	vk().vkDeviceWaitIdle( vk().dev );
	sharedStat().mb -= it->second.mb;
	sharedStat().dropped++;
	sharedFree( it->second, gl().ok && it->second.glCtx == gl().ctx );
	shareds().erase( it );
	sharedGen()++;
}

SharedStat takeSharedStat()
{
	SharedStat s = sharedStat();
	s.live = int( shareds().size() );
	sharedStat().made = sharedStat().dropped = sharedStat().fails = 0;
	return s;
}

/* the tone pass's crossing alone (GL current): GL signals A with the deferred names, the worker submits, GL waits B.
 * false: why (nothing signals B: GL does not wait it) */
static bool toneCross( std::string & w )
{
	Tone & T = tone();
	Gl & g = gl();
	GLenum lay[8];
	for ( GLenum & l : lay )
		l = kLayoutGeneral;
	g.signalSemaphore( T.glDoneGl, 0, nullptr, T.defN, T.defTex, lay );
	g.flush();  // Vulkan's wait sees GL's signal only once it is flushed
	Worker & W = wk();
	std::uint64_t want = 0;
	{
		std::lock_guard<std::mutex> lk( W.m );
		want = W.toneDone + 1;
	}
	Work j;
	j.k = -1;
	j.tone = true;
	hand( j );
	bool ok = false;
	{
		// GL may wait Vulkan's signal only once its submit is in the queue
		std::unique_lock<std::mutex> lk( W.m );
		W.idle.wait( lk, [&W, want]() { return W.toneDone >= want; } );
		ok = W.toneOk;
		w = W.toneWhy;
	}
	if ( !ok )
		return false;
	g.waitSemaphore( T.vkDoneGl, 0, nullptr, T.defN, T.defTex, lay );
	T.pending = true;
	return true;
}

bool tonePending()
{
	return tone().deferred;
}

bool toneFlush( std::string * why )
{
	Tone & T = tone();
	if ( !T.deferred )
		return true;
	// a GL pass needs the view before the TAA's crossing: this one crosses alone, the next frames do not defer
	T.deferred = false;
	T.mergeHint = false;
	T.backoff = 120;
	T.st.flushed++;
	std::string w = "the GL context changed under a deferred pass (dropped)";
	if ( T.semCtx == gl().ctx && toneCross( w ) )
		return true;
	T.broken = true;
	T.brokenWhy = "the tone pass broke (deferred, crossing alone): " + w;
	T.st.fails++;
	T.st.failWhy = T.brokenWhy;
	if ( why )
		*why = T.brokenWhy;
	return false;
}

bool toneRun( const ToneRun & t, std::string & why )
{
	Tone & T = tone();
	View & V = view();
	Gl & g = gl();
	Vk & v = vk();
	const double t0 = nowMs();
	toneFlush( nullptr );  // never two deferred passes
	auto refuse = [&]( const std::string & w ) {
		why = w;
		T.st.fails++;
		T.st.failWhy = w;
		return false;
	};
	// the pass for good off after a failed submit or build (no retry each frame); toneFrame says it too
	auto breakOff = [&]( const std::string & w ) {
		T.broken = true;
		T.brokenWhy = "the tone pass broke: " + w;
		return refuse( T.brokenWhy );
	};
	if ( T.broken )
		return refuse( T.brokenWhy );
	if ( !g.ok || !T.colorTex || T.glCtx != g.ctx || !T.made || T.semCtx != g.ctx )
		return refuse( "no shared tone frame in this GL context" );
	if ( V.cur < 0 || !V.fbo )
		return refuse( "the view is not bound this frame" );
	if ( V.samples != T.samples || T.w > V.w || T.h > V.h )
		return refuse( "the tone frame's shape is not the view's" );
	const std::int32_t f = t.job.ints[0];
	if ( f & ( 1 << 9 ) )
		return refuse( "the fog bloom is on (GL's)" );
	const bool bloomOn = ( f & ( 1 << 7 ) ) != 0, backOn = ( f & ( 1 << 11 ) ) != 0;
	if ( ( bloomOn || backOn ) != ( t.bloom != nullptr ) || ( backOn && !bloomOn ) )
		return refuse( "the Fusion Shaders bloom's flags and its job do not agree (GL's bloom)" );
	const bool lutOn = ( f & ( 1 << 5 ) ) != 0, measureOn = ( f & ( 1 << 6 ) ) != 0;
	if ( lutOn && !t.lut && !( T.lutHave && T.lutGen == t.lutGen ) )
		return refuse( "the LUT is on but no LUT bytes" );
	if ( measureOn && ( !t.measure || t.measureW < 1 || t.measureH < 1 ) )
		return refuse( "the measure is on but no measure texture" );
	// the last pass is done before its command buffer, set and staging are written again (the worker submitted it)
	ToneBloomState & B = T.bloom;
	if ( T.pending ) {
		v.vkWaitForFences( v.dev, 1, &T.fence, VK_TRUE, 1000000000ull );
		T.pending = false;
		if ( B.measHave )
			B.measDone = true;  // lane VKBLOOM1: its read buffer is filled (toneMeasureTake)
	}
	std::string w;
	if ( !tonePipes( V.samples, w ) || !toneFbs( w ) )
		return breakOff( w );
	if ( lutOn && t.lut && ( !T.lutHave || T.lutGen != t.lutGen ) ) {
		for ( int k = 0; k < 16 * 16 * 16; k++ ) {
			T.stagingPtr[k * 4 + 0] = t.lut[k * 3 + 0];
			T.stagingPtr[k * 4 + 1] = t.lut[k * 3 + 1];
			T.stagingPtr[k * 4 + 2] = t.lut[k * 3 + 2];
			T.stagingPtr[k * 4 + 3] = 255;
		}
		T.lutCopy = true;
		T.lutHave = true;
		T.lutGen = t.lutGen;
	}
	if ( measureOn ) {
		if ( !T.measureTex || T.mw != t.measureW || T.mh != t.measureH ) {
			drain();  // makeShared's layout change is a submit
			toneMeasureDrop( true );
			if ( !makeShared( T.measure, T.measureMem, T.measureTex, T.measureObj, VK_FORMAT_R32G32B32A32_SFLOAT, kRgba32f, 1, t.measureW,
					t.measureH, false, T.measureMb, w ) ) {
				toneMeasureDrop( true );
				return refuse( w );
			}
			T.measureView = imageView( T.measure, VK_IMAGE_VIEW_TYPE_2D, VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_ASPECT_COLOR_BIT );
			g.genFramebuffers( 2, T.copyFb );
			g.bindFramebuffer( kDrawFb, T.copyFb[1] );
			g.framebufferTexture2D( kDrawFb, kColorAttach0, kTexture2D, T.measureTex, 0 );
			g.bindFramebuffer( kDrawFb, t.drawFb );
			T.mw = t.measureW;
			T.mh = t.measureH;
			T.descDirty = true;
		}
		if ( T.measureSrc != t.measure ) {
			// a new GL bloom texture: attached once, its completeness asked once (an RGB32F upload is not renderable everywhere)
			g.bindFramebuffer( kReadFb, T.copyFb[0] );
			g.framebufferTexture2D( kReadFb, kColorAttach0, kTexture2D, t.measure, 0 );
			const GLenum st = g.checkFramebufferStatus( kReadFb );
			g.bindFramebuffer( kReadFb, t.readFb );
			if ( st != kFbComplete ) {
				T.measureSrc = 0;
				return refuse( "GL's measure bloom cannot be read through a framebuffer (status " + std::to_string( st ) + ")" );
			}
			T.measureSrc = t.measure;
			T.measureHave = false;
		}
		if ( !T.measureHave || T.measureGen != t.measureGen ) {
			// GL's bloom into the shared copy (nearest, same size; the caller turned GL's scissor off); the
			// frame's bindings go back as the caller named them
			g.bindFramebuffer( kReadFb, T.copyFb[0] );
			g.bindFramebuffer( kDrawFb, T.copyFb[1] );
			g.blitFramebuffer( 0, 0, T.mw, T.mh, 0, 0, T.mw, T.mh, kColorBit, kNearest );
			g.bindFramebuffer( kReadFb, t.readFb );
			g.bindFramebuffer( kDrawFb, t.drawFb );
			T.measureHave = true;
			T.measureGen = t.measureGen;
		}
	}
	/* lane VKBLOOM1: the Fusion Shaders bloom (cellbloom.cpp's job) and the exposure measure (cellhdr.cpp's ask) recorded
	 * in this pass's command buffer: their images made once per shape (the worker drained first: makeShared submits) */
	B.on = B.backdrop = false;
	B.measQw = B.measQh = 0;
	B.measRan = false;
	const bool measAsk = t.measQw > 0 && t.measQh > 0 && !B.broken;
	bool bloomFresh = false;
	if ( t.bloom || measAsk ) {
		const WwVkPresent::ToneBloom * b = t.bloom;
		if ( B.broken )
			return refuse( B.brokenWhy );
		bool same = B.made && ( !b || ( B.pool && B.W == T.w && B.H == T.h && B.mips == b->mips && B.setFrame == T.color ) );
		for ( int i = 0; same && b && i < b->mips; i++ )
			same = B.dims[i][0] == b->dims[i][0] && B.dims[i][1] == b->dims[i][1];
		if ( measAsk && ( !B.meas.img || !B.measSet || B.measW != t.measQw || B.measH != t.measQh ) )
			same = false;
		const int backS = view().samples;
		const bool backNew = backOn && ( !B.back.img || !B.backSet || B.back.w != T.w || B.back.h != T.h || B.backMs.samples != backS );
		if ( backNew )
			same = false;
		if ( !same ) {
			drain();
			std::string bw;
			bool ok = toneBloomStatic( bw ) && ( !measAsk || toneBloomMeasure( t.measQw, t.measQh, bw ) ) && ( !b || toneBloomChain( *b, bw ) );
			if ( ok && backNew ) {
				ok = toneBloomBack( backS, bw );
				T.descDirty = true;
			}
			if ( !ok ) {
				B.broken = true;
				B.brokenWhy = "the bloom in the tone pass broke: " + bw;
				return refuse( B.brokenWhy );
			}
		}
		if ( b ) {
			B.job = *b;
			bloomFresh = B.fresh;
			if ( B.fresh )
				B.job.holdN = 1;  // a new ring: its other layers hold nothing yet
			B.fresh = false;
			B.on = true;
			B.backdrop = backOn;
		}
		if ( measAsk && B.measSet && B.readPtr ) {
			B.measQw = t.measQw;
			B.measQh = t.measQh;
		}
	}
	const bool measureBound = measureOn && T.measureView;
	static bool boundMeasure = false;
	static VkImageView boundComb = VK_NULL_HANDLE, boundBack = VK_NULL_HANDLE;
	const VkImageView combView = B.on && B.comb.view ? B.comb.view : T.dummyView;
	const VkImageView backView = B.backdrop && B.back.view && bloomRedTest != 3 ? B.back.view : T.dummyView;
	if ( T.descDirty || boundMeasure != measureBound || boundComb != combView || boundBack != backView ) {
		const VkImageView views[7] = { T.colorView, T.stencilView, combView, measureBound ? T.measureView : T.dummyView, T.lutView,
			T.colorView, backView };
		VkDescriptorImageInfo ii[8] = {};
		VkWriteDescriptorSet wr[8] = {};
		for ( std::uint32_t k = 0; k < 8; k++ ) {
			ii[k].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
			if ( k < 7 )
				ii[k].imageView = views[k];
			else
				ii[k].sampler = T.sampler;
			wr[k].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			wr[k].dstSet = T.set;
			wr[k].dstBinding = k;
			wr[k].descriptorCount = 1;
			wr[k].descriptorType = k == 7 ? VK_DESCRIPTOR_TYPE_SAMPLER : VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
			wr[k].pImageInfo = &ii[k];
		}
		v.vkUpdateDescriptorSets( v.dev, 8, wr, 0, nullptr );
		T.descDirty = false;
		boundMeasure = measureBound;
		boundComb = combView;
		boundBack = backView;
	}
	T.slot = V.cur;
	if ( t.vp[2] > 0 && t.vp[3] > 0 ) {
		for ( int k = 0; k < 4; k++ )
			T.vp[k] = t.vp[k];
	} else {
		T.vp[0] = T.vp[1] = 0;
		T.vp[2] = T.w;
		T.vp[3] = T.h;
	}
	T.splitSky = t.splitSky;
	T.sorted = t.sorted;
	T.job = t.job;
	// GL's frame (and the view's depth GL blitted) done -> the worker's pass -> GL's later passes on the view
	T.defTex[0] = T.colorTex;
	T.defTex[1] = T.dsTex;
	T.defTex[2] = V.slot[V.cur].colorTex;
	T.defTex[3] = V.slot[V.cur].dsTex;
	T.defN = 4u;
	if ( measureBound )
		T.defTex[T.defN++] = T.measureTex;
	if ( B.measQw > 0 )
		T.defTex[T.defN++] = B.meas.tex;  // lane VKBLOOM1: GL blurs the measure's quarter target at the next frame
	if ( T.backoff > 0 )
		T.backoff--;
	if ( T.mergeHint && !T.mergeRed && T.backoff == 0 ) {
		// lane VKPOST2: no crossing now; the TAA's carries this pass (or toneFlush, before a GL pass on the view)
		T.deferred = true;
		T.st.deferred++;
	} else if ( !toneCross( w ) ) {
		B.on = false;
		B.measQw = B.measQh = 0;
		return breakOff( w );  // nothing signals vkDone: GL does not wait it; GL draws its quads this frame
	}
	if ( B.on ) {
		B.runs += bloomFresh ? 2 : 1;  // a new chain: cellbloom.cpp sees a gap and starts its hold again
		T.st.bloom++;
	}
	if ( B.measQw > 0 ) {
		T.st.measure++;
		B.measRan = B.measHave = true;
		B.measDone = false;
		B.measRecord++;
	}
	T.ranSinceTaa = true;
	const double ms = nowMs() - t0;
	T.st.runs++;
	T.st.cpuMs += ms;
	T.st.maxMs = std::max( T.st.maxMs, ms );
	return true;
}

bool toneBloomReady()
{
	const Tone & T = tone();
	return vk().ok && T.made && !T.broken && !T.bloom.broken && T.colorTex && T.samples > 1 && vk().vkCmdDispatch
		&& vk().vkCreateComputePipelines;
}

std::uint64_t toneBloomRuns()
{
	return tone().bloom.runs;
}

bool toneMeasureRan()
{
	return tone().bloom.measRan;
}

int toneMeasureTake( const float *& px, int & qw, int & qh, unsigned & tex )
{
	Tone & T = tone();
	ToneBloomState & B = T.bloom;
	px = nullptr;
	qw = qh = 0;
	tex = 0;
	if ( !B.measHave || B.broken || !B.readPtr || !B.meas.tex )
		return 0;
	bool done = B.measDone;
	if ( !done && T.deferred )
		return -1;  // its pass waits for the TAA's crossing
	if ( !done && T.pending ) {
		const VkResult r = vk().vkGetFenceStatus( vk().dev, T.fence );
		if ( r != VK_SUCCESS )
			return r == VK_NOT_READY ? -1 : 0;
		done = true;
	}
	B.measHave = false;
	if ( !done )
		return 0;  // its pass never went in (a failed submit): the caller measures on GL
	px = B.readPtr;
	qw = B.measW;
	qh = B.measH;
	tex = B.meas.tex;
	return 1;
}

void toneOnGl( const char * why )
{
	Tone & T = tone();
	T.st.gl++;
	T.st.glWhy = why ? why : "";
}

ToneStat takeToneStat()
{
	Tone & T = tone();
	ToneStat s = T.st;
	s.mb = T.frameMb + T.measureMb + T.fixedMb + T.bloom.mb + T.bloom.measMb + T.bloom.backMb;
	s.pipelines = T.pipelines;
	s.buildMs = T.buildMs;
	if ( T.broken && s.failWhy.empty() )
		s.failWhy = T.brokenWhy;
	T.st = ToneStat();
	return s;
}

void toneCachePath( const std::string & path )
{
	tone().cachePath = path;
}

bool taaRun( const TaaRun & t, std::string & why )
{
	Taa & A = taa();
	View & V = view();
	Gl & g = gl();
	Vk & v = vk();
	const double t0 = nowMs();
	auto refuse = [&]( const std::string & w ) {
		why = w;
		A.st.fails++;
		A.st.failWhy = w;
		return false;
	};
	auto breakOff = [&]( const std::string & w ) {
		A.broken = true;
		A.brokenWhy = "the temporal AA on Vulkan broke: " + w;
		return refuse( A.brokenWhy );
	};
	if ( A.broken )
		return refuse( A.brokenWhy );
	if ( !v.ok || !g.ok || g.ctx != reinterpret_cast<void *>( wglGetCurrentContext() ) )
		return refuse( "the present device or GL's entry points are not ready in this context" );
	if ( V.cur < 0 || !V.fbo )
		return refuse( "the view is not bound this frame" );
	if ( V.w != t.w || V.h != t.h )
		return refuse( "the frame is not the view's size" );
	if ( !v.storageExt )
		return refuse( "the device lacks shaderStorageImageExtendedFormats (the vectors' RG32F store)" );
	if ( t.wr < 0 || t.wr > 1 )
		return refuse( "a history index out of 0..1" );
	// lane VKPOST2: the merged crossing takes the colour from the view (GL skipped its blit) behind the deferred tone pass
	Tone & T = tone();
	const bool merge = t.colorFromView;
	if ( merge && !T.deferred )
		return refuse( "the colour from the view, but no deferred tone pass" );
	if ( !merge )
		toneFlush( nullptr );
	auto storable = [&]( VkFormat f ) {
		VkFormatProperties fp {};
		v.vkGetPhysicalDeviceFormatProperties( v.phys, f, &fp );
		return ( fp.optimalTilingFeatures & VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT ) != 0;
	};
	static const bool formatsOk = storable( VK_FORMAT_R32G32_SFLOAT ) && storable( VK_FORMAT_R16G16B16A16_SFLOAT )
		&& storable( VK_FORMAT_R8G8B8A8_UNORM );
	if ( !formatsOk )
		return refuse( "RG32F / RGBA16F / RGBA8 storage is not offered by the device" );
	// every target this frame's passes touch, shared in this context
	const std::uintptr_t keys[9] = { t.cur, t.depth, t.mv, t.pic, t.hist[0], t.hist[1], t.lock[0], t.lock[1], 0 };
	const Shared * s[8] = {};
	for ( int k = 0; k < 8; k++ ) {
		s[k] = sharedFind( keys[k] );
		if ( !s[k] || s[k]->glCtx != g.ctx || !s[k]->view || s[k]->w != t.w || s[k]->h != t.h )
			return refuse( "a TAA target is not shared in this context at the frame's size (key " + std::to_string( k ) + ")" );
	}
	const Shared & sCur = *s[0], & sDepth = *s[1], & sMv = *s[2], & sPic = *s[3];
	const Shared * sHist[2] = { s[4], s[5] }, * sLock[2] = { s[6], s[7] };
	if ( sMv.fmt != VK_FORMAT_R32G32_SFLOAT || sPic.fmt != VK_FORMAT_R8G8B8A8_UNORM || sHist[0]->fmt != VK_FORMAT_R16G16B16A16_SFLOAT
		|| sLock[0]->fmt != VK_FORMAT_R16G16B16A16_SFLOAT || sDepth.glFmt != kDepth24Stencil8 )
		return refuse( "a TAA target's format is not the pass's" );
	if ( merge && sCur.fmt != VK_FORMAT_R8G8B8A8_UNORM )
		return refuse( "the colour target is not RGBA8 (the view's resolve needs the same format)" );
	const int inputs = t.job.ints[3];
	if ( ( inputs & 3 ) && !g.copyImageSubData )
		return refuse( "TAAINPUTS1's velocity / mask are on but no glCopyImageSubData" );
	// the last run is done before its command buffer and sets are written again
	if ( A.pending ) {
		v.vkWaitForFences( v.dev, 1, &A.fence, VK_TRUE, 1000000000ull );
		A.pending = false;
	}
	std::string w;
	if ( !taaStatic( w ) || !taaSems( w ) || !taaPipes( V.samples, w ) || !taaFbs( w ) )
		return breakOff( w );
	// TAAINPUTS1's velocity and mask: GL's own textures copied into shared twins (made on first use, between frames' passes)
	GLuint velTex = 0, maskTex = 0;
	const Shared * sVel = nullptr, * sMask = nullptr;
	if ( inputs & 1 ) {
		if ( !t.vel || !t.velKey || !sharedTarget( t.velKey, 0, t.w, t.h, 1, kGlRg16f, velTex, w ) )
			return refuse( "the velocity's shared twin: " + ( w.empty() ? std::string( "no texture" ) : w ) );
		g.copyImageSubData( t.vel, kTexture2D, 0, 0, 0, 0, velTex, kTexture2D, 0, 0, 0, 0, t.w, t.h, 1 );
		sVel = sharedFind( t.velKey );
	}
	if ( inputs & 2 ) {
		if ( !t.mask || !t.maskKey || !sharedTarget( t.maskKey, 0, t.w, t.h, 1, kRgba8, maskTex, w ) )
			return refuse( "the mask's shared twin: " + ( w.empty() ? std::string( "no texture" ) : w ) );
		g.copyImageSubData( t.mask, kTexture2D, 0, 0, 0, 0, maskTex, kTexture2D, 0, 0, 0, 0, t.w, t.h, 1 );
		sMask = sharedFind( t.maskKey );
	}
	// the sets, written again when a view in them changed (a remade target) or a shared target came or went
	const int wr = t.wr, rd = wr ^ 1;
	const VkImageView rViews[10] = { sCur.view, sHist[rd]->view, sMv.view, sDepth.view, sLock[rd]->view, sVel ? sVel->view : sMv.view,
		sMask ? sMask->view : sMv.view, sHist[wr]->view, sPic.view, sLock[wr]->view };
	const bool genMoved = A.heldGen != sharedGen();
	if ( genMoved || std::memcmp( rViews, A.rHeld[wr], sizeof( rViews ) ) != 0 ) {
		VkDescriptorImageInfo ii[11] = {};
		VkWriteDescriptorSet ws[11] = {};
		for ( std::uint32_t k = 0; k < 11; k++ ) {
			ii[k].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
			if ( k == 7 )
				ii[k].sampler = A.linear;
			else
				ii[k].imageView = rViews[k < 7 ? k : k - 1];
			ws[k].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			ws[k].dstSet = A.rset[wr];
			ws[k].dstBinding = k;
			ws[k].descriptorCount = 1;
			ws[k].descriptorType = k < 7 ? VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE : k == 7 ? VK_DESCRIPTOR_TYPE_SAMPLER : VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
			ws[k].pImageInfo = &ii[k];
		}
		v.vkUpdateDescriptorSets( v.dev, 11, ws, 0, nullptr );
		std::memcpy( A.rHeld[wr], rViews, sizeof( rViews ) );
	}
	// the PostFx shim's sets: the vectors (T0 the depth, out 7 the vectors) and the copy-back (T0 the picture, 7 the same)
	auto pfxSet = [&]( VkDescriptorSet set, VkImageView t0v, VkImageView outv ) {
		VkDescriptorImageInfo ii[8] = {};
		VkWriteDescriptorSet ws[8] = {};
		for ( std::uint32_t k = 0; k < 8; k++ ) {
			ii[k].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
			if ( k == 5 || k == 6 )
				ii[k].sampler = k == 5 ? A.linear : A.point;
			else
				ii[k].imageView = k == 7 ? outv : t0v;
			ws[k].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			ws[k].dstSet = set;
			ws[k].dstBinding = k;
			ws[k].descriptorCount = 1;
			ws[k].descriptorType = k < 5 ? VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE : k < 7 ? VK_DESCRIPTOR_TYPE_SAMPLER : VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
			ws[k].pImageInfo = &ii[k];
		}
		v.vkUpdateDescriptorSets( v.dev, 8, ws, 0, nullptr );
	};
	if ( genMoved || A.mvHeld[0] != sDepth.view || A.mvHeld[1] != sMv.view ) {
		pfxSet( A.mvSet, sDepth.view, sMv.view );
		A.mvHeld[0] = sDepth.view;
		A.mvHeld[1] = sMv.view;
	}
	if ( genMoved || A.rcasHeld != sPic.view ) {
		pfxSet( A.rcasSet, sPic.view, sPic.view );
		A.rcasHeld = sPic.view;
	}
	if ( genMoved ) {
		// the other history slot's set holds old views too: written again on its frame
		std::memset( A.rHeld[rd], 0, sizeof( A.rHeld[rd] ) );
		A.heldGen = sharedGen();
	}
	A.slot = V.cur;
	A.w = t.w;
	A.h = t.h;
	A.wr = wr;
	A.job = t.job;
	A.mvJob = t.mvJob;
	A.rcasJob = t.rcasJob;
	A.fromView = merge;
	A.curImg = sCur.img;
	// GL's blits (colour, depth), the lock clears and the inputs' copies done -> the worker's passes -> GL's later passes
	GLuint tex[18] = { sCur.tex, sDepth.tex, sMv.tex, sPic.tex, sHist[0]->tex, sHist[1]->tex, sLock[0]->tex, sLock[1]->tex,
		V.slot[V.cur].colorTex };
	GLuint nTex = 9;
	if ( velTex )
		tex[nTex++] = velTex;
	if ( maskTex )
		tex[nTex++] = maskTex;
	if ( merge ) {
		// the tone pass's names too (the view slot's colour is in both lists once)
		for ( GLuint k = 0; k < T.defN; k++ ) {
			if ( std::find( tex, tex + nTex, T.defTex[k] ) == tex + nTex )
				tex[nTex++] = T.defTex[k];
		}
	}
	GLenum lay[18];
	for ( GLenum & l : lay )
		l = kLayoutGeneral;
	g.signalSemaphore( A.glDoneGl, 0, nullptr, nTex, tex, lay );
	g.flush();
	Worker & W = wk();
	std::uint64_t want = 0;
	{
		std::lock_guard<std::mutex> lk( W.m );
		want = W.toneDone + 1;
	}
	Work j;
	j.k = -1;
	j.taa = !merge;
	j.merge = merge;
	hand( j );
	bool ok = false;
	{
		std::unique_lock<std::mutex> lk( W.m );
		W.idle.wait( lk, [&W, want]() { return W.toneDone >= want; } );
		ok = W.toneOk;
		w = W.toneWhy;
	}
	if ( merge ) {
		T.deferred = false;
		T.pending = T.subOk;  // the tone fence is armed once its submit went in
	}
	if ( !ok ) {
		if ( merge ) {
			// the tone map of this frame is lost too: both stages off for good, GL's own from the next frame
			T.broken = true;
			T.brokenWhy = "the tone pass broke (merged crossing): " + w;
		}
		return breakOff( w );  // nothing signals vkDone: GL does not wait it; GL runs its passes this frame
	}
	g.waitSemaphore( A.vkDoneGl, 0, nullptr, nTex, tex, lay );
	A.pending = true;
	if ( merge )
		A.st.merged++;
	T.mergeHint = merge || T.ranSinceTaa;
	T.ranSinceTaa = false;
	const double ms = nowMs() - t0;
	A.st.runs++;
	A.st.cpuMs += ms;
	A.st.maxMs = std::max( A.st.maxMs, ms );
	return true;
}

void taaOnGl( const char * why )
{
	Taa & A = taa();
	A.st.gl++;
	A.st.glWhy = why ? why : "";
}

TaaStat takeTaaStat()
{
	Taa & A = taa();
	TaaStat s = A.st;
	s.pipelines = A.pipelines;
	s.buildMs = A.buildMs;
	if ( A.broken && s.failWhy.empty() )
		s.failWhy = A.brokenWhy;
	A.st = TaaStat();
	return s;
}

bool present( void * hwnd, int w, int h, bool vsync, GlProc gp, std::string & why )
{
	const double t0 = nowMs();
	Gl & g = gl();
	void * ctx = reinterpret_cast<void *>( wglGetCurrentContext() );
	if ( !ctx ) {
		why = "no GL context current";
		return false;
	}
	if ( g.ctx != ctx ) {
		Gl fresh;
		if ( !loadGl( fresh, gp, why ) )
			return false;
		fresh.ctx = ctx;
		g = fresh;
	}
	toneFlush( nullptr );  // lane VKPOST2: a deferred tone pass no TAA took crosses before the present reads the view
	if ( w < 1 || h < 1 ) {
		why = "the window has no area";
		return false;
	}
	if ( !makeDevice( why ) )
		return false;
	Vk & v = vk();
	Chain & c = ch();
	// a window / size that failed once is not tried again each frame (the reason stays the answer until either moves)
	static std::string failWhy;
	static HWND failHwnd = nullptr;
	static int failW = 0, failH = 0;
	if ( !failWhy.empty() && failHwnd == HWND( hwnd ) && failW == w && failH == h ) {
		why = failWhy;
		return false;
	}
	if ( takeRebuild() )
		c.rebuild = true;
	if ( !ensure( HWND( hwnd ), w, h, vsync, why ) ) {
		failWhy = why;
		failHwnd = HWND( hwnd );
		failW = w;
		failH = h;
		dropAll( true );
		return false;
	}
	failWhy.clear();
	( void ) v;
	if ( !c.sigTried ) {
		c.sigTried = true;
		makeSig( c, c.sigWhy );
	}
	const int k = int( c.frame++ % kSlots );
	Slot & s = c.slot[k];
	/* the worker has submitted this slot's last Vulkan half, and that read of the slot is done (its fence): GL then
	 * writes the slot with no GPU wait on Vulkan (the frame cap fence measures GL's own work only) */
	const double tw = nowMs();
	waitSlot( k );
	bool busy = false;
	if ( s.pending && v.vkGetFenceStatus( v.dev, s.fence ) == VK_NOT_READY ) {
		busy = true;
		v.vkWaitForFences( v.dev, 1, &s.fence, VK_TRUE, 1000000000ull );
	}
	const double waited = nowMs() - tw;
	/* GL: resolve the back buffer into the slot, signal (GENERAL both sides). Nothing is asked: with
	 * glBlitNamedFramebuffer no binding changes, and the scissor test is switched off without asking (glIsEnabled
	 * cost 0.13-0.20 ms on build 87, a wait for the driver's own thread); the paint sets its own scissor */
	const double tq = nowMs();
	GLint rd = 0, dr = 0;
	if ( !g.blitNamedFramebuffer ) {
		g.getIntegerv( kReadFbBinding, &rd );
		g.getIntegerv( kDrawFbBinding, &dr );
	}
	const double tg = nowMs();
	const GLenum general = kLayoutGeneral;
	/* round 10, the view on Vulkan: GL drew the frame into view vs (viewBegin); the worker resolves it into the slot on
	 * Vulkan, so no GL blit here. Size mismatch (a view made at another size): the old blit from the back buffer */
	View & V = view();
	const int vs = V.cur >= 0 && V.w == w && V.h == h ? V.cur : -1;
	V.cur = -1;
	GLuint sigTex = s.tex;
	if ( vs >= 0 ) {
		V.slot[vs].reader = k;
		sigTex = V.slot[vs].colorTex;
	} else if ( g.blitNamedFramebuffer ) {
		g.disable( kScissorTest );
		g.blitNamedFramebuffer( 0, s.fbo, 0, 0, w, h, 0, 0, w, h, kColorBit, kNearest );
	} else {
		g.disable( kScissorTest );
		g.bindFramebuffer( kReadFb, 0 );
		g.bindFramebuffer( kDrawFb, s.fbo );
		g.blitFramebuffer( 0, 0, w, h, 0, 0, w, h, kColorBit, kNearest );
		g.bindFramebuffer( kReadFb, GLuint( rd ) );
		g.bindFramebuffer( kDrawFb, GLuint( dr ) );
	}
	const double tb = nowMs();
	/* build 88: the signal is a submit of the whole frame (0.16 ms of the drawing thread with the blit): the worker's GL
	 * context sends it after a GPU wait on this fence; here only the fence and the flush that sends it */
	void * sync = nullptr;
	const int route = signalRoute();
	if ( c.sigCtx && g.fenceSync && g.deleteSync && route != 0 )
		sync = g.fenceSync( kSyncGpuDone, 0 );
	if ( !sync )
		g.signalSemaphore( s.glDoneGl, 0, nullptr, 1, &sigTex, &general );
	s.glWaited = sync && route == 2;
	const double tf = nowMs();
	g.flush();  // Vulkan's wait (or the worker's fence wait) sees GL's work only once it is flushed
	const double tv = nowMs();
	s.used = true;
	// Vulkan: on the present worker; a capture (the harness's picture) waits for it here
	Stat & st = stat();
	const bool capture = c.capture != nullptr;
	st.worker = sync || !capture;
	if ( st.worker ) {
		Work j;
		j.k = k;
		j.w = w;
		j.h = h;
		j.sync = sync;
		j.sem = s.glDoneGl;
		j.tex = sigTex;
		j.cpuWait = s.glWaited;
		j.vs = vs;
		hand( j );
		if ( capture ) {
			drain();
			Worker & W = wk();
			std::lock_guard<std::mutex> lk( W.m );
			if ( !W.lastOk ) {
				why = W.lastWhy;
				return false;
			}
		}
	} else {
		drain();
		bool rebuild = false;
		Split one;
		const bool ok = vkPart( k, w, h, vs, why, rebuild, one );
		if ( rebuild )
			c.rebuild = true;
		if ( !ok )
			return false;
	}
	st.ms = nowMs() - t0;
	st.waitMs = waited;
	st.stateMs = tg - tq;
	st.blitMs = tb - tg;
	st.glMs = tf - tb;
	st.flushMs = tv - tf;
	st.sigWorker = sync != nullptr;
	st.sigWait = s.glWaited;
	st.sigWhy = c.sigCtx ? std::string() : c.sigWhy;
	st.dsa = g.blitNamedFramebuffer != nullptr;
	st.busy = busy;
	st.view = vs >= 0;
	st.mode = modeName( c.mode );
	st.images = int( c.images.size() );
	st.w = int( c.extent.width );
	st.h = int( c.extent.height );
	return true;
}

Split takeSplit()
{
	return takeWorkerSplit();
}

namespace
{
const char *& routeWord()
{
	static const char * r = "gl swap";
	return r;
}
}  // namespace

const char * routeName()
{
	return routeWord();
}

}  // namespace WwVkPresent

// lane VKSKY1: the cell sky on the tone frame (row G's first pass), its own crossing like the temporal AA's
namespace
{
struct Sky
{
	bool made = false, broken = false;
	std::string brokenWhy;
	VkSampler linear = VK_NULL_HANDLE;
	VkDescriptorSetLayout dsl = VK_NULL_HANDLE;
	VkDescriptorPool dpool = VK_NULL_HANDLE;
	VkDescriptorSet set = VK_NULL_HANDLE;
	VkPipelineLayout layout = VK_NULL_HANDLE;
	VkShaderModule vs = VK_NULL_HANDLE, ps = VK_NULL_HANDLE;
	VkBuffer ubo = VK_NULL_HANDLE;  // b0 at 0, b31 at kSkyLooseAt
	VkDeviceMemory uboMem = VK_NULL_HANDLE;
	std::uint8_t * uboPtr = nullptr;
	VkSemaphore glDone = VK_NULL_HANDLE, vkDone = VK_NULL_HANDLE;
	GLuint glDoneGl = 0, vkDoneGl = 0;
	void * semCtx = nullptr;
	VkCommandBuffer cmd = VK_NULL_HANDLE;
	VkFence fence = VK_NULL_HANDLE;
	bool pending = false;
	int pipeSamples = 0;
	VkRenderPass pass = VK_NULL_HANDLE;
	VkPipeline pipe = VK_NULL_HANDLE;
	int pipelines = 0;
	double buildMs = 0.0;
	VkFramebuffer fb = VK_NULL_HANDLE;  // over the tone frame's colour, made each run (the frame may be remade between)
	int w = 0, h = 0, vp[4] = {};
	WwVkPresent::SkyStat st;
};
constexpr VkDeviceSize kSkyLooseAt = 512;  // past b0 (336), on any minUniformBufferOffsetAlignment up to 256

Sky & sky()
{
	static Sky s;
	return s;
}

// the device-time parts (once): the sampler, the set layout, pool + set, the pipeline layout, both modules, the uniform
// buffer (host-coherent, mapped), the command buffer and its fence
bool skyStatic( std::string & why )
{
	Vk & v = vk();
	Sky & S = sky();
	if ( S.made )
		return true;
	const double t0 = nowMs();
	VkSamplerCreateInfo sci { VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };  // GL's LUTs: LINEAR, CLAMP_TO_EDGE, one level
	sci.magFilter = sci.minFilter = VK_FILTER_LINEAR;
	sci.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
	sci.addressModeU = sci.addressModeV = sci.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	sci.maxLod = 0.0f;
	if ( v.vkCreateSampler( v.dev, &sci, nullptr, &S.linear ) != VK_SUCCESS ) {
		why = "vkCreateSampler (the sky) failed";
		return false;
	}
	// sky_view_ps.spv.h: b0 / b31 uniform buffers, t0-3 at 64-67, s0-3 at 128-131 (vkframe_spirv.sh's shifts)
	VkDescriptorSetLayoutBinding b[10] = {};
	const std::uint32_t num[10] = { 0, 31, 64, 65, 66, 67, 128, 129, 130, 131 };
	for ( int k = 0; k < 10; k++ ) {
		b[k].binding = num[k];
		b[k].descriptorType = k < 2 ? VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER : k < 6 ? VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE : VK_DESCRIPTOR_TYPE_SAMPLER;
		b[k].descriptorCount = 1;
		b[k].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	}
	VkDescriptorSetLayoutCreateInfo lci { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
	lci.bindingCount = 10;
	lci.pBindings = b;
	VkDescriptorPoolSize ps[3] = { { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 2 }, { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 4 },
		{ VK_DESCRIPTOR_TYPE_SAMPLER, 4 } };
	VkDescriptorPoolCreateInfo pci { VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
	pci.maxSets = 1;
	pci.poolSizeCount = 3;
	pci.pPoolSizes = ps;
	if ( v.vkCreateDescriptorSetLayout( v.dev, &lci, nullptr, &S.dsl ) != VK_SUCCESS
		|| v.vkCreateDescriptorPool( v.dev, &pci, nullptr, &S.dpool ) != VK_SUCCESS ) {
		why = "the sky's descriptor set layout / pool failed";
		return false;
	}
	VkDescriptorSetAllocateInfo dai { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
	dai.descriptorPool = S.dpool;
	dai.descriptorSetCount = 1;
	dai.pSetLayouts = &S.dsl;
	VkPipelineLayoutCreateInfo plci { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
	plci.setLayoutCount = 1;
	plci.pSetLayouts = &S.dsl;
	if ( v.vkAllocateDescriptorSets( v.dev, &dai, &S.set ) != VK_SUCCESS
		|| v.vkCreatePipelineLayout( v.dev, &plci, nullptr, &S.layout ) != VK_SUCCESS ) {
		why = "the sky's descriptor set / pipeline layout failed";
		return false;
	}
	auto module = [&]( const std::uint32_t * code, size_t bytes, VkShaderModule & out ) {
		VkShaderModuleCreateInfo mci { VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
		mci.codeSize = bytes;
		mci.pCode = code;
		return v.vkCreateShaderModule( v.dev, &mci, nullptr, &out ) == VK_SUCCESS;
	};
	if ( !module( vkframe_tone_view_vs_spv, sizeof( vkframe_tone_view_vs_spv ), S.vs )
		|| !module( vkframe_sky_view_ps_spv, sizeof( vkframe_sky_view_ps_spv ), S.ps ) ) {
		why = "the sky's shader modules failed";
		return false;
	}
	VkBufferCreateInfo bci { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
	bci.size = kSkyLooseAt + VkDeviceSize( WwVkPresent::kSkyLooseBytes );
	bci.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
	bool mapped = v.vkCreateBuffer( v.dev, &bci, nullptr, &S.ubo ) == VK_SUCCESS;
	if ( mapped ) {
		VkMemoryRequirements br {};
		v.vkGetBufferMemoryRequirements( v.dev, S.ubo, &br );
		VkMemoryAllocateInfo ai { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
		ai.allocationSize = br.size;
		ai.memoryTypeIndex = memType( br.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT );
		void * p = nullptr;
		mapped = ai.memoryTypeIndex != UINT32_MAX && v.vkAllocateMemory( v.dev, &ai, nullptr, &S.uboMem ) == VK_SUCCESS
			&& v.vkBindBufferMemory( v.dev, S.ubo, S.uboMem, 0 ) == VK_SUCCESS
			&& v.vkMapMemory( v.dev, S.uboMem, 0, VK_WHOLE_SIZE, 0, &p ) == VK_SUCCESS;
		S.uboPtr = static_cast<std::uint8_t *>( p );
	}
	if ( !mapped || !S.uboPtr ) {
		why = "the sky's uniform buffer failed";
		return false;
	}
	VkFenceCreateInfo fi { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
	VkCommandBufferAllocateInfo cai { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
	cai.commandPool = v.pool;
	cai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	cai.commandBufferCount = 1;
	if ( v.vkCreateFence( v.dev, &fi, nullptr, &S.fence ) != VK_SUCCESS || v.vkAllocateCommandBuffers( v.dev, &cai, &S.cmd ) != VK_SUCCESS ) {
		why = "the sky's fence / command buffer failed";
		return false;
	}
	S.buildMs = nowMs() - t0;
	S.made = true;
	return true;
}

void skySemsDrop( bool glAlive )
{
	Vk & v = vk();
	Gl & g = gl();
	Sky & S = sky();
	if ( glAlive )
		for ( GLuint * s : { &S.glDoneGl, &S.vkDoneGl } )
			if ( *s )
				g.deleteSemaphores( 1, s );
	for ( VkSemaphore * s : { &S.glDone, &S.vkDone } )
		if ( *s )
			v.vkDestroySemaphore( v.dev, *s, nullptr );
	S.glDoneGl = S.vkDoneGl = 0;
	S.glDone = S.vkDone = VK_NULL_HANDLE;
	S.semCtx = nullptr;
}

bool skySems( std::string & why )
{
	Gl & g = gl();
	Sky & S = sky();
	if ( S.glDone && S.semCtx == g.ctx )
		return true;
	skySemsDrop( false );  // another context's names: that context went
	if ( !exportSem( S.glDone, S.glDoneGl, why ) || !exportSem( S.vkDone, S.vkDoneGl, why ) ) {
		skySemsDrop( true );
		return false;
	}
	S.semCtx = g.ctx;
	return true;
}

// the render pass (one RGBA16F colour at the frame's samples, LOAD / STORE, GENERAL) and the pipeline: one fragment a
// pixel writes every covered sample, as GL's quad does; no depth or stencil (GL draws the quad with both tests off)
bool skyPipes( int samples, std::string & why )
{
	Vk & v = vk();
	Sky & S = sky();
	if ( S.pass && S.pipeSamples == samples )
		return true;
	if ( S.pipe )
		v.vkDestroyPipeline( v.dev, S.pipe, nullptr );
	if ( S.pass )
		v.vkDestroyRenderPass( v.dev, S.pass, nullptr );
	S.pipe = VK_NULL_HANDLE;
	S.pass = VK_NULL_HANDLE;
	S.pipeSamples = 0;
	const double t0 = nowMs();
	const VkSampleCountFlagBits sc = VkSampleCountFlagBits( samples );
	VkAttachmentDescription at {};
	at.format = VK_FORMAT_R16G16B16A16_SFLOAT;
	at.samples = sc;
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
	if ( v.vkCreateRenderPass( v.dev, &rci, nullptr, &S.pass ) != VK_SUCCESS ) {
		why = "vkCreateRenderPass (the sky) failed";
		return false;
	}
	VkPipelineShaderStageCreateInfo st[2] = { { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO },
		{ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO } };
	st[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
	st[0].module = S.vs;
	st[0].pName = "main";
	st[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	st[1].module = S.ps;
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
	ms.rasterizationSamples = sc;
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
	gp.pColorBlendState = &cbs;
	gp.pDynamicState = &dy;
	gp.layout = S.layout;
	gp.renderPass = S.pass;
	if ( v.vkCreateGraphicsPipelines( v.dev, tone().cache, 1, &gp, nullptr, &S.pipe ) != VK_SUCCESS ) {
		why = "vkCreateGraphicsPipelines (the sky, " + std::to_string( samples ) + " samples) failed";
		return false;
	}
	S.pipeSamples = samples;
	S.pipelines = 1;
	S.buildMs += nowMs() - t0;
	toneCacheSave();
	return true;
}

// the worker: the sky's triangle into the tone frame, waiting GL's signal, signalling back, with the fence
bool skyWork( std::string & why )
{
	Vk & v = vk();
	Sky & S = sky();
	if ( !S.fb ) {
		why = "no framebuffer over the tone frame";
		return false;
	}
	v.vkResetFences( v.dev, 1, &S.fence );
	v.vkResetCommandBuffer( S.cmd, 0 );
	VkCommandBufferBeginInfo bi { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
	bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	v.vkBeginCommandBuffer( S.cmd, &bi );
	VkRenderPassBeginInfo rbi { VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
	rbi.renderPass = S.pass;
	rbi.framebuffer = S.fb;
	rbi.renderArea = { { 0, 0 }, { std::uint32_t( S.w ), std::uint32_t( S.h ) } };
	v.vkCmdBeginRenderPass( S.cmd, &rbi, VK_SUBPASS_CONTENTS_INLINE );
	// GL's viewport rows from the bottom = the image's rows from the top in memory: no flip (FragCoord.y = GL's)
	const VkViewport vp { float( S.vp[0] ), float( S.vp[1] ), float( S.vp[2] ), float( S.vp[3] ), 0.0f, 1.0f };
	const int x0 = std::clamp( S.vp[0], 0, S.w ), y0 = std::clamp( S.vp[1], 0, S.h );
	const int x1 = std::clamp( S.vp[0] + S.vp[2], 0, S.w ), y1 = std::clamp( S.vp[1] + S.vp[3], 0, S.h );
	const VkRect2D sc { { x0, y0 }, { std::uint32_t( std::max( x1 - x0, 0 ) ), std::uint32_t( std::max( y1 - y0, 0 ) ) } };
	v.vkCmdSetViewport( S.cmd, 0, 1, &vp );
	v.vkCmdSetScissor( S.cmd, 0, 1, &sc );
	v.vkCmdBindPipeline( S.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, S.pipe );
	v.vkCmdBindDescriptorSets( S.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, S.layout, 0, 1, &S.set, 0, nullptr );
	v.vkCmdDraw( S.cmd, 3, 1, 0, 0 );
	v.vkCmdEndRenderPass( S.cmd );
	if ( v.vkEndCommandBuffer( S.cmd ) != VK_SUCCESS ) {
		why = "vkEndCommandBuffer (the sky) failed";
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
		why = "vkQueueSubmit (the sky) returned " + std::to_string( int( r ) );
		return false;
	}
	return true;
}

// shutdown: the crossing's semaphores (GL's names) and the framebuffer; the pipelines stay with the device
void skyDrop()
{
	Sky & S = sky();
	if ( !S.glDone && !S.pending && !S.fb )
		return;
	drain();
	vk().vkDeviceWaitIdle( vk().dev );
	S.pending = false;
	if ( S.fb )
		vk().vkDestroyFramebuffer( vk().dev, S.fb, nullptr );
	S.fb = VK_NULL_HANDLE;
	skySemsDrop( gl().ok && S.semCtx == gl().ctx );
}
}  // namespace

namespace WwVkPresent
{
bool skyRun( const SkyRun & t, std::string & why )
{
	Sky & S = sky();
	Tone & T = tone();
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
		S.brokenWhy = "the sky on Vulkan broke: " + w;
		return refuse( S.brokenWhy );
	};
	if ( S.broken )
		return refuse( S.brokenWhy );
	if ( !v.ok || !g.ok || g.ctx != reinterpret_cast<void *>( wglGetCurrentContext() ) )
		return refuse( "the present device or GL's entry points are not ready in this context" );
	if ( !toneFrameIs( t.color ) || !T.colorView )
		return refuse( "the draw target is not the shared tone frame" );
	if ( T.w != t.w || T.h != t.h )
		return refuse( "the sky's size is not the tone frame's" );
	if ( t.vp[2] < 1 || t.vp[3] < 1 )
		return refuse( "an empty viewport" );
	if ( !g.copyImageSubData )
		return refuse( "no glCopyImageSubData (the LUTs' copies into their shared twins)" );
	// the last run is done before its command buffer, set, framebuffer and uniforms are written again
	if ( S.pending ) {
		v.vkWaitForFences( v.dev, 1, &S.fence, VK_TRUE, 1000000000ull );
		S.pending = false;
	}
	std::string w;
	if ( !skyStatic( w ) || !skySems( w ) || !skyPipes( T.samples, w ) )
		return breakOff( w );
	// the four LUTs GL drew this frame, copied into shared twins (made on first use; a tier change remakes them)
	GLuint lutTex[4] = {};
	const Shared * sLut[4] = {};
	for ( int k = 0; k < 4; k++ ) {
		if ( !t.lut[k] || !t.lutKey[k] || !sharedTarget( t.lutKey[k], 0, t.lutW[k], t.lutH[k], 1, kRgba16f, lutTex[k], w ) )
			return refuse( "a LUT's shared twin (" + std::to_string( k ) + "): " + ( w.empty() ? std::string( "no texture" ) : w ) );
		sLut[k] = sharedFind( t.lutKey[k] );
		if ( !sLut[k] || !sLut[k]->view )
			return refuse( "a LUT's shared twin has no view" );
	}
	for ( int k = 0; k < 4; k++ )
		g.copyImageSubData( t.lut[k], kTexture2D, 0, 0, 0, 0, lutTex[k], kTexture2D, 0, 0, 0, 0, t.lutW[k], t.lutH[k], 1 );
	// the framebuffer over the frame (made again each run: the tone frame may be remade between runs)
	if ( S.fb )
		v.vkDestroyFramebuffer( v.dev, S.fb, nullptr );
	S.fb = VK_NULL_HANDLE;
	VkFramebufferCreateInfo fci { VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
	fci.renderPass = S.pass;
	fci.attachmentCount = 1;
	fci.pAttachments = &T.colorView;
	fci.width = std::uint32_t( T.w );
	fci.height = std::uint32_t( T.h );
	fci.layers = 1;
	if ( v.vkCreateFramebuffer( v.dev, &fci, nullptr, &S.fb ) != VK_SUCCESS )
		return breakOff( "the sky's framebuffer over the tone frame failed" );
	// the uniforms and the set (the fence above: the GPU no longer reads either)
	std::memcpy( S.uboPtr, t.global, size_t( kSkyGlobalBytes ) );
	std::memcpy( S.uboPtr + kSkyLooseAt, t.loose, size_t( kSkyLooseBytes ) );
	VkDescriptorBufferInfo bi[2] = { { S.ubo, 0, VkDeviceSize( kSkyGlobalBytes ) }, { S.ubo, kSkyLooseAt, VkDeviceSize( kSkyLooseBytes ) } };
	VkDescriptorImageInfo ii[8] = {};
	VkWriteDescriptorSet ws[10] = {};
	// HLSL t0 atTransTex, t1 atMsTex, t2 atSunView, t3 atMoonView = SkyRun's lut 0, 1, 2, 3
	for ( int k = 0; k < 10; k++ ) {
		ws[k].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		ws[k].dstSet = S.set;
		ws[k].descriptorCount = 1;
		if ( k < 2 ) {
			ws[k].dstBinding = k ? 31 : 0;
			ws[k].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			ws[k].pBufferInfo = &bi[k];
		} else if ( k < 6 ) {
			ii[k - 2].imageView = sLut[k - 2]->view;
			ii[k - 2].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
			ws[k].dstBinding = std::uint32_t( 64 + k - 2 );
			ws[k].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
			ws[k].pImageInfo = &ii[k - 2];
		} else {
			ii[k - 2].sampler = S.linear;
			ws[k].dstBinding = std::uint32_t( 128 + k - 6 );
			ws[k].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
			ws[k].pImageInfo = &ii[k - 2];
		}
	}
	v.vkUpdateDescriptorSets( v.dev, 10, ws, 0, nullptr );
	S.w = t.w;
	S.h = t.h;
	std::memcpy( S.vp, t.vp, sizeof( S.vp ) );
	// GL's frame so far and the LUT copies done -> the worker's sky -> GL's backdrop layers and objects
	GLuint tex[5] = { T.colorTex, lutTex[0], lutTex[1], lutTex[2], lutTex[3] };
	GLenum lay[5];
	for ( GLenum & l : lay )
		l = kLayoutGeneral;
	g.signalSemaphore( S.glDoneGl, 0, nullptr, 5, tex, lay );
	g.flush();
	Worker & W = wk();
	std::uint64_t want = 0;
	{
		std::lock_guard<std::mutex> lk( W.m );
		want = W.toneDone + 1;
	}
	Work j;
	j.k = -1;
	j.sky = true;
	hand( j );
	bool ok = false;
	{
		std::unique_lock<std::mutex> lk( W.m );
		W.idle.wait( lk, [&W, want]() { return W.toneDone >= want; } );
		ok = W.toneOk;
		w = W.toneWhy;
	}
	if ( !ok )
		return breakOff( w );  // nothing signals vkDone: GL does not wait it; GL draws its quad this frame
	g.waitSemaphore( S.vkDoneGl, 0, nullptr, 5, tex, lay );
	S.pending = true;
	const double ms = nowMs() - t0;
	S.st.runs++;
	S.st.cpuMs += ms;
	S.st.maxMs = std::max( S.st.maxMs, ms );
	return true;
}

void skyOnGl( const char * why )
{
	Sky & S = sky();
	S.st.gl++;
	S.st.glWhy = why ? why : "";
}

SkyStat takeSkyStat()
{
	Sky & S = sky();
	SkyStat s = S.st;
	s.pipelines = S.pipelines;
	s.buildMs = S.buildMs;
	if ( S.broken && s.failWhy.empty() )
		s.failWhy = S.brokenWhy;
	S.st = SkyStat();
	return s;
}
}  // namespace WwVkPresent

namespace
{

// the Vulkan half of slot k's frame: the fence of its last use, the next swapchain image, the flipped blit, present
bool vkPart( int k, int w, int h, int vs, std::string & why, bool & rebuild, WwVkPresent::Split & sp )
{
	Vk & v = vk();
	Chain & c = ch();
	Slot & s = c.slot[k];
	const ViewSlot * vw = vs >= 0 ? &view().slot[vs] : nullptr;
	const int vSamples = view().samples;
	const double tw = nowMs();
	if ( s.pending ) {
		v.vkWaitForFences( v.dev, 1, &s.fence, VK_TRUE, 1000000000ull );
		v.vkResetFences( v.dev, 1, &s.fence );
		s.pending = false;
	}
	sp.fenceMs += nowMs() - tw;
	// Vulkan: the next swapchain image, the flipped blit (GL's row 0 is the bottom), present
	std::uint32_t idx = 0;
	const double ta = nowMs();
	VkResult ar = v.vkAcquireNextImageKHR( v.dev, c.swap, 100000000ull, s.acquired, VK_NULL_HANDLE, &idx );
	sp.acquireMs += nowMs() - ta;
	VkSubmitInfo si { VK_STRUCTURE_TYPE_SUBMIT_INFO };
	const VkPipelineStageFlags stages[2] = { VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT };
	if ( ar != VK_SUCCESS && ar != VK_SUBOPTIMAL_KHR ) {
		// GL's signal is still owed a wait: an empty batch carries it (and the fence the next use checks)
		si.waitSemaphoreCount = s.glWaited ? 0 : 1;
		si.pWaitSemaphores = &s.glDone;
		si.pWaitDstStageMask = stages;
		s.pending = v.vkQueueSubmit( v.queue, 1, &si, s.fence ) == VK_SUCCESS;
		rebuild = true;
		why = "vkAcquireNextImageKHR returned " + std::to_string( int( ar ) );
		return false;
	}
	const double tr = nowMs();
	VkCommandBuffer cmd = s.cmd;
	v.vkResetCommandBuffer( cmd, 0 );
	VkCommandBufferBeginInfo bi { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
	bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	v.vkBeginCommandBuffer( cmd, &bi );
	VkImageMemoryBarrier b[2] = { { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER }, { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER } };
	b[0].srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;
	b[0].dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
	b[0].oldLayout = b[0].newLayout = VK_IMAGE_LAYOUT_GENERAL;
	b[0].srcQueueFamilyIndex = b[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	b[0].image = s.img;
	b[0].subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
	VkImage src = s.img;
	if ( vw && vSamples > 1 ) {
		/* round 10: GL's multisample view resolved here into the slot (GL's blit before): the view's GL writes before
		 * the transfer read, the slot's last read (the blit of kSlots frames back) before the resolve's write */
		VkImageMemoryBarrier r2[2] = { b[0], b[0] };
		r2[0].image = vw->color;
		r2[1].srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
		r2[1].dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		v.vkCmdPipelineBarrier( cmd, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 2, r2 );
		VkImageResolve rs {};
		rs.srcSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
		rs.dstSubresource = rs.srcSubresource;
		rs.extent = { std::uint32_t( w ), std::uint32_t( h ), 1 };
		v.vkCmdResolveImage( cmd, vw->color, VK_IMAGE_LAYOUT_GENERAL, s.img, VK_IMAGE_LAYOUT_GENERAL, 1, &rs );
		b[0].srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	} else if ( vw ) {
		src = vw->color;  // a single-sample view: the flipped blit reads it as it is
		b[0].image = src;
	}
	b[1] = b[0];
	b[1].srcAccessMask = 0;
	b[1].dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	b[1].oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	b[1].newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	b[1].image = c.images[idx];
	v.vkCmdPipelineBarrier( cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 2, b );
	const int bw = std::min( w, int( c.extent.width ) ), bh = std::min( h, int( c.extent.height ) );
	VkImageBlit r {};
	r.srcSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
	r.dstSubresource = r.srcSubresource;
	r.srcOffsets[0] = { 0, h, 0 };
	r.srcOffsets[1] = { bw, h - bh, 1 };
	r.dstOffsets[0] = { 0, 0, 0 };
	r.dstOffsets[1] = { bw, bh, 1 };
	v.vkCmdBlitImage( cmd, src, VK_IMAGE_LAYOUT_GENERAL, c.images[idx], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &r, VK_FILTER_NEAREST );
	VkBuffer buf = VK_NULL_HANDLE;
	VkDeviceMemory bufMem = VK_NULL_HANDLE;
	const VkDeviceSize bytes = VkDeviceSize( c.extent.width ) * c.extent.height * 4;
	VkImageLayout at = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	if ( c.capture ) {
		VkBufferCreateInfo bci { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
		bci.size = bytes;
		bci.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
		VkMemoryRequirements mr {};
		if ( v.vkCreateBuffer( v.dev, &bci, nullptr, &buf ) == VK_SUCCESS ) {
			v.vkGetBufferMemoryRequirements( v.dev, buf, &mr );
			VkMemoryAllocateInfo ai { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
			ai.allocationSize = mr.size;
			ai.memoryTypeIndex = memType( mr.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT );
			if ( v.vkAllocateMemory( v.dev, &ai, nullptr, &bufMem ) == VK_SUCCESS )
				v.vkBindBufferMemory( v.dev, buf, bufMem, 0 );
		}
		VkImageMemoryBarrier t = b[1];
		t.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		t.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
		t.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		t.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		v.vkCmdPipelineBarrier( cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &t );
		VkBufferImageCopy cp {};
		cp.imageSubresource = r.dstSubresource;
		cp.imageExtent = { c.extent.width, c.extent.height, 1 };
		v.vkCmdCopyImageToBuffer( cmd, c.images[idx], VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, buf, 1, &cp );
		at = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	}
	VkImageMemoryBarrier p = b[1];
	p.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_TRANSFER_READ_BIT;
	p.dstAccessMask = 0;
	p.oldLayout = at;
	p.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	v.vkCmdPipelineBarrier( cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 1, &p );
	v.vkEndCommandBuffer( cmd );
	const VkSemaphore waits[2] = { s.acquired, s.glDone };
	si.waitSemaphoreCount = s.glWaited ? 1 : 2;  // build 91: GL's frame seen done on the CPU: the image's wait only
	si.pWaitSemaphores = waits;
	si.pWaitDstStageMask = stages;
	si.commandBufferCount = 1;
	si.pCommandBuffers = &cmd;
	si.signalSemaphoreCount = 1;  // the present's only (GL checks the fence, not vkDone: no GPU wait on Vulkan)
	si.pSignalSemaphores = &c.renderDone[idx];
	sp.recordMs += nowMs() - tr;
	const double ts = nowMs();
	const VkResult sr = v.vkQueueSubmit( v.queue, 1, &si, s.fence );
	sp.submitMs += nowMs() - ts;
	if ( sr != VK_SUCCESS ) {
		why = "vkQueueSubmit (the present blit) failed";
		rebuild = true;
		return false;
	}
	s.pending = true;
	VkPresentInfoKHR pi { VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
	pi.waitSemaphoreCount = 1;
	pi.pWaitSemaphores = &c.renderDone[idx];
	pi.swapchainCount = 1;
	pi.pSwapchains = &c.swap;
	pi.pImageIndices = &idx;
	const double tp = nowMs();
	const VkResult pr = v.vkQueuePresentKHR( v.queue, &pi );
	sp.presentMs += nowMs() - tp;
	if ( pr == VK_SUBOPTIMAL_KHR || pr == VK_ERROR_OUT_OF_DATE_KHR )
		rebuild = true;
	if ( c.capture ) {
		v.vkWaitForFences( v.dev, 1, &s.fence, VK_TRUE, 1000000000ull );
		void * m = nullptr;
		c.capture->assign( size_t( bytes ), 0 );
		if ( bufMem && v.vkMapMemory( v.dev, bufMem, 0, bytes, 0, &m ) == VK_SUCCESS ) {
			std::memcpy( c.capture->data(), m, size_t( bytes ) );
			v.vkUnmapMemory( v.dev, bufMem );
		}
		if ( c.format == VK_FORMAT_R8G8B8A8_UNORM )	// the caller reads BGRA
			for ( size_t i = 0; i + 3 < c.capture->size(); i += 4 )
				std::swap( ( *c.capture )[i], ( *c.capture )[i + 2] );
		if ( buf )
			v.vkDestroyBuffer( v.dev, buf, nullptr );
		if ( bufMem )
			v.vkFreeMemory( v.dev, bufMem, nullptr );
		c.capture = nullptr;
	}
	if ( pr != VK_SUCCESS && pr != VK_SUBOPTIMAL_KHR ) {
		why = "vkQueuePresentKHR returned " + std::to_string( int( pr ) );
		return false;
	}
	sp.n = 1;
	return true;
}

}  // namespace

#ifndef WW_VKPRESENT_NO_QT
#include "framepacer.h"   // wwVsyncOn
#include "harnesswindow.h"   // wwHarnessSettingsSuffix
#include "version.h"
#include "wwswitch.h"

#include <QDebug>
#include <QOpenGLContext>
#include <QOpenGLFunctions>
#include <QOpenGLPaintDevice>
#include <QOpenGLWindow>
#include <QDir>
#include <QSettings>
#include <QStandardPaths>

#include <memory>

namespace WwVkPresent
{

namespace
{
void * qtProc( const char * name )
{
	QOpenGLContext * q = QOpenGLContext::currentContext();
	return q ? reinterpret_cast<void *>( q->getProcAddress( name ) ) : nullptr;
}

// WW_VKPRESENT_RED=1 (live), or Settings > Ray tracing device > Vulkan Present unticked (read again each second).
// Build 91: the signal route, WW_VKPRESENT_SIGNAL=0|1|2 (live), else the Vulkan Present Signal row (signalRoute)
bool wanted( QString & off )
{
	if ( wwEnvInt( "WW_VKPRESENT_RED" ) == 1 ) {
		off = QStringLiteral( "red WW_VKPRESENT_RED=1" );
		return false;
	}
	static double at = -1.0e9;
	static bool row = true;
	static int routeRow = kSignalDefault;
	const double now = nowMs();
	if ( now - at >= 1000.0 ) {
		at = now;
		QSettings s( "NifTools", "NifSkope " + NifSkopeVersion::rawToMajMin( NIFSKOPE_VERSION ) + wwHarnessSettingsSuffix() );
		row = s.value( "Settings/Nif/Vulkan Present", true ).toBool();
		routeRow = s.value( "Settings/Nif/Vulkan Present Signal", kSignalDefault ).toInt();
	}
	const int route = wwEnvIsSet( "WW_VKPRESENT_SIGNAL" ) ? wwEnvInt( "WW_VKPRESENT_SIGNAL" ) : routeRow;
	signalRoute() = route >= 0 && route <= 2 ? route : kSignalDefault;
	if ( !row )
		off = QStringLiteral( "the Vulkan Present row is off" );
	return row;
}

/* round 10: the view on Vulkan. Opt-in WW_VKVIEW=1 (live): no flight has measured it a win yet, so it ships off; the
 * red WW_VKVIEW_RED=1 forces the old route (GL draws its back buffer and blits it into the slot) over the key */
bool viewWanted( QString & off )
{
	if ( wwEnvInt( "WW_VKVIEW_RED" ) == 1 ) {
		off = QStringLiteral( "red WW_VKVIEW_RED=1" );
		return false;
	}
	if ( wwEnvInt( "WW_VKVIEW" ) != 1 ) {
		off = QStringLiteral( "WW_VKVIEW unset: off until a flight measures it a win" );
		return false;
	}
	return true;
}

// QPainter through the view's FBO (QOpenGLWindow's own paint device binds the back buffer)
class ViewPaintDevice : public QOpenGLPaintDevice
{
public:
	explicit ViewPaintDevice( const QSize & s ) : QOpenGLPaintDevice( s ) {}
	void ensureActiveTarget() override
	{
		if ( QOpenGLContext * q = QOpenGLContext::currentContext() )
			q->functions()->glBindFramebuffer( GL_FRAMEBUFFER, viewFbo() );
	}
};
}  // namespace

QPaintDevice * painterTarget( QOpenGLWindow * w )
{
	static std::unique_ptr<ViewPaintDevice> dev;
	static QOpenGLContext * devCtx = nullptr;
	if ( !viewFbo() )
		return w;
	const QSize size( view().w, view().h );
	if ( !dev || dev->size() != size || devCtx != QOpenGLContext::currentContext() ) {
		dev = std::make_unique<ViewPaintDevice>( size );
		devCtx = QOpenGLContext::currentContext();
	}
	dev->setDevicePixelRatio( w->devicePixelRatio() );
	return dev.get();
}

bool toneWanted()
{
	static bool pathSet = false;
	if ( !pathSet ) {  // before the first toneFrame (the pipeline cache is read once, at the device-time make)
		pathSet = true;
		QString dir = QStandardPaths::writableLocation( QStandardPaths::CacheLocation );
		if ( dir.isEmpty() )
			dir = QDir::tempPath();
		QDir().mkpath( dir );
		toneCachePath( ( dir + QStringLiteral( "/vkpresent_tone" ) + wwHarnessSettingsSuffix() + QStringLiteral( ".bin" ) ).toLocal8Bit().toStdString() );
	}
	if ( wwEnvInt( "WW_VKVIEW_TONE_RED" ) == 1 )
		return false;
	tone().mergeRed = wwEnvInt( "WW_VKVIEW_MERGE_RED" ) == 1;  // lane VKPOST2: two crossings again (tone, then TAA)
	QString off;
	return viewWanted( off ) && viewFbo() != 0;
}

bool shareWanted( const char * redEnv )
{
	if ( redEnv && wwEnvInt( redEnv ) == 1 )
		return false;
	QString off;
	return viewWanted( off ) && viewFbo() != 0;
}

bool frame( QOpenGLWindow * w, const std::function<void()> & paint )
{
	static QString said;
	static QSize baseSize;
	static double winAt = 0.0, winSum = 0.0, winMax = 0.0, winWait = 0.0, winState = 0.0, winBlit = 0.0, winGl = 0.0,
		winFlush = 0.0;
	static int winN = 0, winBusy = 0;
	static std::uint64_t builtSaid = 0;
	auto say = [&]( const QString & line ) {
		if ( line != said ) {
			said = line;
			qInfo().noquote() << "vk present:" << line;
		}
	};
	static QString routeSaid, viewWhy;
	static double sizeAt = 0.0;
	static bool viewLast = false, viewMade = false;
	static int winView = 0;
	auto sayRoute = [&]( const QString & line ) {
		if ( line != routeSaid ) {
			routeSaid = line;
			qInfo().noquote() << "vk post route:" << line;
		}
	};
	QString off;
	const bool on = wanted( off );
	if ( !on ) {
		if ( ch().child || viewMade ) {
			w->makeCurrent();
			shutdown();
			viewMade = viewLast = false;
		}
		routeWord() = "gl swap";
		say( QStringLiteral( "off (%1): GL's SwapBuffers" ).arg( off ) );
		sayRoute( QStringLiteral( "gl swap (the Vulkan present is off): GL draws its back buffer" ) );
		return false;
	}
	const qreal dpr = w->devicePixelRatio();
	const QSize ds( int( w->width() * dpr + 0.5 ), int( w->height() * dpr + 0.5 ) );
	// a new size (and the first frame): QOpenGLWindow's own paint sizes the paint device QPainter draws through
	if ( ds != baseSize || !w->isExposed() || !w->context() ) {
		baseSize = ds;
		sizeAt = nowMs();
		viewLast = false;  // QOpenGLWindow's paint binds its back buffer itself
		return false;
	}
	w->makeCurrent();
	/* round 10: the view on Vulkan (WW_VKVIEW=1) once the size has held 500 ms (a view ring is a full sync to rebuild,
	 * skill ww-gl-vk-interop build 83); otherwise, and on any refusal, the old route: the back buffer and GL's blit */
	QString viewOff;
	bool viewOn = viewWanted( viewOff );
	if ( viewOn && nowMs() - sizeAt < 500.0 ) {
		viewOn = false;
		viewOff = QStringLiteral( "the size is still moving" );
	}
	if ( !viewOn && viewMade && viewOff != QStringLiteral( "the size is still moving" ) ) {
		viewDrop();
		viewMade = false;
	}
	if ( viewOn ) {
		std::string vwhy;
		const int samples = std::max( w->context()->format().samples(), 1 );
		viewOn = viewBegin( ds.width(), ds.height(), samples, qtProc, vwhy );
		if ( viewOn )
			viewMade = true;
		else
			viewOff = QStringLiteral( "refused: %1" ).arg( QString::fromStdString( vwhy ) );
	}
	if ( !viewOn && viewLast )
		w->context()->functions()->glBindFramebuffer( GL_FRAMEBUFFER, 0 );
	viewLast = viewOn;
	viewWhy = viewOff;
	paint();
	std::string why;
	if ( !present( reinterpret_cast<void *>( w->winId() ), ds.width(), ds.height(), wwVsyncOn(), qtProc, why ) ) {
		say( QStringLiteral( "failed (%1): GL's SwapBuffers this frame" ).arg( QString::fromStdString( why ) ) );
		viewRescue();
		w->context()->swapBuffers( w );
		return true;
	}
	routeWord() = last().view ? "vk view" : "vk present";
	winView += last().view ? 1 : 0;
	const Stat & st = last();
	if ( st.builds != builtSaid ) {
		builtSaid = st.builds;
		say( QStringLiteral( "swapchain %1x%2, mode %3, %4 images, own device on GL's GPU (build %5)" )
			.arg( st.w ).arg( st.h ).arg( QLatin1String( st.mode ) ).arg( st.images ).arg( st.builds ) );
	}
	winSum += st.ms;
	winWait += st.waitMs;
	winState += st.stateMs;
	winBlit += st.blitMs;
	winGl += st.glMs;
	winFlush += st.flushMs;
	winBusy += st.busy ? 1 : 0;
	winMax = std::max( winMax, st.ms );
	winN++;
	const double now = nowMs();
	if ( now - winAt >= 5000.0 ) {
		const Split sp = takeSplit();
		if ( winAt > 0.0 && winN > 0 ) {
			qInfo().noquote() << QStringLiteral( "vk present: %1 ms (max %2, slot wait %3), mode %4, %5 images, %6x%7, %8 frames, "
				"%9 slots, %10 reused while Vulkan still read them" )
				.arg( winSum / winN, 0, 'f', 3 ).arg( winMax, 0, 'f', 3 ).arg( winWait / winN, 0, 'f', 3 )
				.arg( QLatin1String( st.mode ) ).arg( st.images ).arg( st.w ).arg( st.h ).arg( winN ).arg( kSlots ).arg( winBusy );
			// the drawing thread's share by part, and the worker's (its ms are off the drawing thread)
			const double n = std::max( sp.n, 1 );
			const QString how = QStringLiteral( "%1, signal route %2" )
				.arg( st.view ? QStringLiteral( "view on Vulkan: no GL blit, the worker resolves (record)" )
					: st.dsa ? QStringLiteral( "named blit" ) : QStringLiteral( "bound blit" ) )
				.arg( st.sigWait ? QStringLiteral( "2 worker CPU wait, no GL signal" ) : st.sigWorker
					? QStringLiteral( "1 worker GL signal" ) : st.sigWhy.empty() ? QStringLiteral( "0 drawing thread GL signal" )
					: QStringLiteral( "0 drawing thread GL signal (%1)" ).arg( QString::fromStdString( st.sigWhy ) ) );
			qInfo().noquote() << QStringLiteral( "vk present split: drawing thread slot wait %1 + GL state %2 + blit %3 + %4 %5 + "
				"flush %6 ms (%7); worker %8 ms (max %9) = GL signal %10 + fence %11 + acquire %12 + record %13 + submit %14 + "
				"present %15, %16 frames, %17 failed%18" )
				.arg( winWait / winN, 0, 'f', 3 ).arg( winState / winN, 0, 'f', 3 ).arg( winBlit / winN, 0, 'f', 3 )
				.arg( st.sigWorker ? QLatin1String( "fence" ) : QLatin1String( "GL signal" ) ).arg( winGl / winN, 0, 'f', 3 )
				.arg( winFlush / winN, 0, 'f', 3 ).arg( how )
				.arg( sp.totalMs / n, 0, 'f', 3 ).arg( sp.maxMs, 0, 'f', 3 ).arg( sp.signalMs / n, 0, 'f', 3 )
				.arg( sp.fenceMs / n, 0, 'f', 3 )
				.arg( sp.acquireMs / n, 0, 'f', 3 ).arg( sp.recordMs / n, 0, 'f', 3 ).arg( sp.submitMs / n, 0, 'f', 3 )
				.arg( sp.presentMs / n, 0, 'f', 3 ).arg( sp.n ).arg( sp.failed )
				.arg( sp.failed ? QStringLiteral( " (last: %1)" ).arg( QString::fromStdString( sp.why ) ) : QString() );
			/* round 10: which route carried the frame, and what still runs on GL before it. The view on Vulkan is the
			 * step every later move stands on: the tone map's backdrop and depth are its images on the present device */
			const ViewStat vs = takeViewStat();
			/* round 11: the stages on the present device and the ones still on GL; each stage's red puts it back on GL
			 * (stage 1, the tone map: WW_VKVIEW_TONE_RED=1) */
			const ToneStat ts = takeToneStat();
			QString tone;
			if ( ts.runs > 0 ) {
				tone = QStringLiteral( "tone map on Vulkan %1 frames (a render pass of the present device into the view: %2 "
					"pipelines built in %3 ms, %4 MB frame + measure + LUT, drawing thread %5 ms, max %6)" )
					.arg( ts.runs ).arg( ts.pipelines ).arg( ts.buildMs, 0, 'f', 1 ).arg( ts.mb, 0, 'f', 1 )
					.arg( ts.cpuMs / ts.runs, 0, 'f', 3 ).arg( ts.maxMs, 0, 'f', 3 );
				// lane VKBLOOM1: the Fusion Shaders bloom and the exposure measure in the same command buffer
				tone += QStringLiteral( "; bloom in the pass %1 frames (%2), measure in the pass %3 frames (%4)" )
					.arg( ts.bloom )
					.arg( wwEnvInt( "WW_VKVIEW_BLOOM_RED" ) == 1 ? QStringLiteral( "red WW_VKVIEW_BLOOM_RED=1" )
						: ::tone().bloom.broken ? QString::fromStdString( ::tone().bloom.brokenWhy ) : QStringLiteral( "on" ) )
					.arg( ts.measure )
					.arg( wwEnvInt( "WW_IS_MEASURE_VK_RED" ) == 1 ? QStringLiteral( "red WW_IS_MEASURE_VK_RED=1" ) : QStringLiteral( "on" ) );
				if ( ts.gl > 0 )
					tone +=QStringLiteral( ", on GL %1 frames (last: %2)" ).arg( ts.gl ).arg( QString::fromStdString( ts.glWhy ) );
				if ( ts.fails > 0 )
					tone += QStringLiteral( ", %1 refused (last: %2)" ).arg( ts.fails ).arg( QString::fromStdString( ts.failWhy ) );
			} else {
				tone = QStringLiteral( "tone map on GL (%1)" )
					.arg( wwEnvInt( "WW_VKVIEW_TONE_RED" ) == 1 ? QStringLiteral( "red WW_VKVIEW_TONE_RED=1" )
						: ts.fails > 0 ? QString::fromStdString( ts.failWhy )
						: ts.gl > 0 ? QString::fromStdString( ts.glWhy ) : QStringLiteral( "no HDR cell frame asked for it" ) );
			}
			// lane VKSHARE1: the keyed targets on the present device (the clouds' march + history: red WW_VKVIEW_CLOUD_RED=1)
			const SharedStat ss = takeSharedStat();
			QString shared = QStringLiteral( "%1 targets shared on the present device, %2 MB; %3 made, %4 dropped" )
				.arg( ss.live ).arg( ss.mb, 0, 'f', 1 ).arg( ss.made ).arg( ss.dropped );
			if ( ss.fails > 0 )
				shared += QStringLiteral( ", %1 refused to GL (last: %2)" ).arg( ss.fails ).arg( QString::fromStdString( ss.failWhy ) );
			if ( wwEnvInt( "WW_VKVIEW_CLOUD_RED" ) == 1 )
				shared += QStringLiteral( "; the clouds' own: red WW_VKVIEW_CLOUD_RED=1" );
			// lane VKSHARE1: the Modern temporal AA's vectors, resolve and copy-back (red WW_VKVIEW_TAA_RED=1)
			const TaaStat as = takeTaaStat();
			QString taa;
			if ( as.runs > 0 ) {
				taa = QStringLiteral( "TAA on Vulkan %1 frames (vectors + resolve + RCAS copy-back, one crossing: %2 pipelines built in "
					"%3 ms, drawing thread %4 ms, max %5; GL keeps the colour / depth blits and the lock clears)" )
					.arg( as.runs ).arg( as.pipelines ).arg( as.buildMs, 0, 'f', 1 ).arg( as.cpuMs / as.runs, 0, 'f', 3 )
					.arg( as.maxMs, 0, 'f', 3 );
				// lane VKPOST2: frames whose TAA crossing also carried the tone pass (GL's colour blit skipped)
				taa += QStringLiteral( "; tone + TAA in one crossing %1 frames, tone deferred %2, of those crossed alone %3 (%4)" )
					.arg( as.merged ).arg( ts.deferred ).arg( ts.flushed )
					.arg( ::tone().mergeRed ? QStringLiteral( "red WW_VKVIEW_MERGE_RED=1" ) : QStringLiteral( "merge on" ) );
				if ( as.gl > 0 )
					taa += QStringLiteral( ", on GL %1 frames (last: %2)" ).arg( as.gl ).arg( QString::fromStdString( as.glWhy ) );
				if ( as.fails > 0 )
					taa += QStringLiteral( ", %1 refused (last: %2)" ).arg( as.fails ).arg( QString::fromStdString( as.failWhy ) );
			} else {
				taa = QStringLiteral( "TAA on GL (%1%2)" )
					.arg( wwEnvInt( "WW_VKVIEW_TAA_RED" ) == 1 ? QStringLiteral( "red WW_VKVIEW_TAA_RED=1" )
						: as.fails > 0 ? QString::fromStdString( as.failWhy )
						: as.gl > 0 ? QString::fromStdString( as.glWhy ) : QStringLiteral( "the Modern TAA did not run" ) )
					.arg( as.gl > 0 ? QStringLiteral( ", %1 frames" ).arg( as.gl ) : QString() );
			}
			if ( winView > 0 ) {
				routeSaid.clear();  // one a window while it runs
				sayRoute( QStringLiteral( "vk view, %1 of %2 frames: GL draws the view into the present device's images (%3x%4 "
					"MS%5 RGBA8 + D24S8, %6 views, %7 MB, made in %8 ms), the worker resolves on Vulkan (no GL blit: drawing "
					"thread present %9 ms); %10 views still read when GL wanted them (wait %11 ms); bakert hop none (the view is "
					"made on the present device); %12; still on GL into the view: the fog bloom's tone (and the Fusion "
					"Shaders bloom's mode 3 under WW_VKVIEW_BLOOM_RED=1), clouds in front (%13), P7, lens flare, overlays + QPainter (the overlay target is the view); %14" )
					.arg( winView ).arg( winN ).arg( vs.w ).arg( vs.h ).arg( vs.samples ).arg( vs.views ).arg( vs.mb, 0, 'f', 1 )
					.arg( vs.makeMs, 0, 'f', 1 ).arg( winSum / winN, 0, 'f', 3 ).arg( vs.busy ).arg( vs.waitMs, 0, 'f', 3 )
					.arg( tone ).arg( shared ).arg( taa ) );
			} else {
				sayRoute( QStringLiteral( "vk present (old route: GL draws its back buffer, GL blits it into the slot; view on "
					"Vulkan off: %1)" ).arg( viewWhy ) );
			}
		}
		winView = 0;
		winAt = now;
		winSum = winMax = winWait = winState = winBlit = winGl = winFlush = 0.0;
		winBusy = 0;
		winN = 0;
	}
	return true;
}

}  // namespace WwVkPresent
#endif
