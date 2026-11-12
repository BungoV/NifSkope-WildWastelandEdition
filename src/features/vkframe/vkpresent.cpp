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

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
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
	kTexBinding2DMs = 0x9104;
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
	VKP( vkMapMemory ); VKP( vkUnmapMemory ); VKP( vkCmdCopyImageToBuffer ); VKP( vkCmdResolveImage );
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
	LI( vkGetPhysicalDeviceSurfacePresentModesKHR );
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
	VkDeviceCreateInfo di { VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
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
	LD( vkMapMemory ); LD( vkUnmapMemory ); LD( vkCmdCopyImageToBuffer ); LD( vkCmdResolveImage );
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
};

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
 * overlay) take the same steps: this is the one place they are written. */
bool makeShared( VkImage & img, VkDeviceMemory & mem, GLuint & tex, GLuint & obj, VkFormat fmt, GLenum glFmt, int samples, int w,
	int h, bool depth, double & mb, std::string & why )
{
	Vk & v = vk();
	Gl & g = gl();
	const VkSampleCountFlagBits sc = VkSampleCountFlagBits( samples );
	// every use GL's texture could have (the import fails with fewer); multisample storage is optional: never asked
	VkImageUsageFlags usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT
		| ( depth ? VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT : VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT );
	if ( samples == 1 && !depth )
		usage |= VK_IMAGE_USAGE_STORAGE_BIT;
	VkImageFormatProperties ip {};
	if ( v.vkGetPhysicalDeviceImageFormatProperties( v.phys, fmt, VK_IMAGE_TYPE_2D, VK_IMAGE_TILING_OPTIMAL, usage, 0, &ip ) != VK_SUCCESS
		|| !( ip.sampleCounts & sc ) || ip.maxExtent.width < std::uint32_t( w ) || ip.maxExtent.height < std::uint32_t( h ) ) {
		why = std::string( depth ? "depth / stencil" : "colour" ) + " format " + std::to_string( int( fmt ) ) + " at " + std::to_string( samples )
			+ " samples is not offered by the device";
		return false;
	}
	VkExternalMemoryImageCreateInfo ext { VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO };
	ext.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT;
	VkImageCreateInfo ci { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
	ci.pNext = &ext;
	ci.imageType = VK_IMAGE_TYPE_2D;
	ci.format = fmt;
	ci.extent = { std::uint32_t( w ), std::uint32_t( h ), 1 };
	ci.mipLevels = 1;
	ci.arrayLayers = 1;
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
			0, 1, 0, 1 };
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
	const GLenum target = samples > 1 ? kTexture2DMs : kTexture2D;
	while ( g.getError() != 0 ) {}
	GLint was = 0;
	g.getIntegerv( samples > 1 ? kTexBinding2DMs : kTexBinding2D, &was );
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

// the views go (GL alive: its textures, memory objects and FBO too); the caller drained the worker
void dropView( bool glAlive )
{
	Vk & v = vk();
	Gl & g = gl();
	View & V = view();
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
	if ( ch().parent || ch().child )
		dropAll( gl().ok );
	if ( view().glCtx ) {
		drain();
		vk().vkDeviceWaitIdle( vk().dev );
		dropView( gl().ok && view().glCtx == gl().ctx );
	}
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
#include <QSettings>

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
			if ( winView > 0 ) {
				routeSaid.clear();  // one a window while it runs
				sayRoute( QStringLiteral( "vk view, %1 of %2 frames: GL draws the view into the present device's images (%3x%4 "
					"MS%5 RGBA8 + D24S8, %6 views, %7 MB, made in %8 ms), the worker resolves on Vulkan (no GL blit: drawing "
					"thread present %9 ms); %10 views still read when GL wanted them (wait %11 ms); bakert hop none (the view is "
					"made on the present device); on GL into the view: tone map (backdrop + LUT GL's), clouds in front (their "
					"targets not shared: GL clouds in front), TAA (history + vectors GL's), lens flare, overlays + QPainter (the "
					"overlay target is the view)" )
					.arg( winView ).arg( winN ).arg( vs.w ).arg( vs.h ).arg( vs.samples ).arg( vs.views ).arg( vs.mb, 0, 'f', 1 )
					.arg( vs.makeMs, 0, 'f', 1 ).arg( winSum / winN, 0, 'f', 3 ).arg( vs.busy ).arg( vs.waitMs, 0, 'f', 3 ) );
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
