/* Lane VKFRAME1 round 2 (2026-11-12): the live glue of the Vulkan frame (vkframe.h). Step P1: the bloom chain's down,
 * hold and up passes run on bakert's Vulkan device on images GL imported, between GL's source resolve and GL's reads.
 *
 * Sync (vkframegraph.h's rules on this device): bakert's device is Vulkan 1.2 without the timelineSemaphore feature on,
 * so the frames in flight are a ring of fences (frame n waits for frame n - 2 before it records; WW_FRAMEQUEUE_RED=1: no
 * cap, only a slot's own reuse) and the GL edges are two binary semaphores (GL_EXT_semaphore is binary): GL signals
 * `glDone` after the source resolve, the submit waits on it and signals `vkDone`, GL waits on that before it reads the
 * top. The CPU never waits on a semaphore; it can wait on a fence (the `blocked` figure of the `vk frame:` line). */

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

#include "features/vkframe/vkframe.h"
#include "features/vkframe/vkframegraph.h"
#include "features/shared/cellpasstime.h"
#include "harnesswindow.h"   // wwHarnessSettingsSuffix
#include "version.h"
#include "wwswitch.h"
#include "wwsyncwait.h"	// lane EMPTY240 R4-2: the `frame overlap:` window, read before its line resets it

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QHash>
#include <QOpenGLContext>
#include <QOpenGLFunctions>
#include <QSettings>
#include <QStandardPaths>

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

#ifdef _WIN32
#include "features/vkframe/bloom_down.spv.h"
#include "features/vkframe/bloom_hold.spv.h"
#include "features/vkframe/bloom_up.spv.h"
#include "features/vkframe/bloom_src.spv.h"	// round 3 (P3): the source from the shared frame
#include "features/vkframe/bloom_comb.spv.h"	// round 3 (P2): the final tent once per pixel
#endif

namespace
{

double nowMs()
{
	return double( std::chrono::duration_cast<std::chrono::nanoseconds>( std::chrono::steady_clock::now().time_since_epoch() ).count() ) * 1.0e-6;
}

// the 5 s window the `vk frame:` line and the `hardware use:` words read (GL thread)
struct Window
{
	double vkMs = 0.0;
	int vkN = 0;	// frames with a timestamp read back
	double blockedMs = 0.0;
	int vkFrames = 0, glFrames = 0;
	QString glWhy;
};

Window & win()
{
	static Window w;
	return w;
}

}	// namespace

#ifndef _WIN32

void wwVkFrameSetDevice( void *, void *, void *, unsigned, void *, void *, const unsigned char[16], std::mutex * ) {}

namespace WwVkPost
{
bool wanted() { return false; }
bool ready() { return false; }
bool makeChain( const void *, int, int, int, const int[][2], unsigned &, unsigned[], unsigned[], unsigned &, QString & why )
{
	why = QStringLiteral( "no device (Windows only)" );
	return false;
}
void freeChain( const void * ) {}
bool hasChain( const void * ) { return false; }
bool makeFrame( const void *, int, int, int, unsigned, unsigned &, unsigned &, QString & why )
{
	why = QStringLiteral( "no device (Windows only)" );
	return false;
}
void freeFrame( const void * ) {}
bool sourceReady( const void *, unsigned, unsigned ) { return false; }
unsigned combTexture( const void * ) { return 0; }
bool runBloom( const void *, const BloomJob &, QString & why )
{
	why = QStringLiteral( "no device (Windows only)" );
	return false;
}
}	// namespace WwVkPost

QString wwVkFrameUse( int, double, double ) { return QString(); }

#else

namespace
{

// VK_KHR_external_memory_win32 / _semaphore_win32's get-handle calls (rtddgi_vk.inc's copies, field for field)
struct MemoryWin32Get { VkStructureType sType; const void * pNext; VkDeviceMemory memory; VkExternalMemoryHandleTypeFlagBits handleType; };
struct SemaphoreWin32Get { VkStructureType sType; const void * pNext; VkSemaphore semaphore; VkExternalSemaphoreHandleTypeFlagBits handleType; };
typedef VkResult ( VKAPI_PTR * PfnGetMemoryWin32 )( VkDevice, const MemoryWin32Get *, HANDLE * );
typedef VkResult ( VKAPI_PTR * PfnGetSemaphoreWin32 )( VkDevice, const SemaphoreWin32Get *, HANDLE * );
constexpr VkStructureType kMemoryGetWin32 = VkStructureType( 1000073003 );     // VK_STRUCTURE_TYPE_MEMORY_GET_WIN32_HANDLE_INFO_KHR
constexpr VkStructureType kSemaphoreGetWin32 = VkStructureType( 1000078003 );  // VK_STRUCTURE_TYPE_SEMAPHORE_GET_WIN32_HANDLE_INFO_KHR

#define VKFRAME_DEVICE_FNS( X ) \
	X( vkCreateImage ) X( vkDestroyImage ) X( vkGetImageMemoryRequirements ) X( vkAllocateMemory ) X( vkFreeMemory ) \
	X( vkBindImageMemory ) X( vkCreateImageView ) X( vkDestroyImageView ) X( vkCreateSampler ) \
	X( vkCreateDescriptorSetLayout ) X( vkCreatePipelineLayout ) X( vkCreateShaderModule ) X( vkDestroyShaderModule ) \
	X( vkCreateComputePipelines ) X( vkCreatePipelineCache ) X( vkDestroyPipelineCache ) X( vkGetPipelineCacheData ) \
	X( vkCreateDescriptorPool ) X( vkDestroyDescriptorPool ) X( vkAllocateDescriptorSets ) X( vkUpdateDescriptorSets ) \
	X( vkCreateCommandPool ) X( vkAllocateCommandBuffers ) X( vkFreeCommandBuffers ) X( vkResetCommandBuffer ) \
	X( vkBeginCommandBuffer ) X( vkEndCommandBuffer ) X( vkCmdBindPipeline ) X( vkCmdBindDescriptorSets ) \
	X( vkCmdPushConstants ) X( vkCmdDispatch ) X( vkCmdPipelineBarrier ) X( vkCmdResetQueryPool ) X( vkCmdWriteTimestamp ) \
	X( vkCreateQueryPool ) X( vkGetQueryPoolResults ) X( vkCreateFence ) X( vkDestroyFence ) X( vkWaitForFences ) \
	X( vkResetFences ) X( vkQueueSubmit ) X( vkCreateSemaphore ) X( vkDestroySemaphore )

#define VKFRAME_INSTANCE_FNS( X ) \
	X( vkGetPhysicalDeviceProperties ) X( vkGetPhysicalDeviceMemoryProperties ) X( vkGetPhysicalDeviceQueueFamilyProperties ) \
	X( vkGetPhysicalDeviceImageFormatProperties2 )

struct Dev
{
	std::atomic<bool> ready { false };
	QString why = QStringLiteral( "no device (the ray-tracing device is not made with GL sharing yet)" );
	VkInstance inst = VK_NULL_HANDLE;
	VkPhysicalDevice pd = VK_NULL_HANDLE;
	VkDevice dev = VK_NULL_HANDLE;
	uint32_t qf = 0;
	VkQueue q = VK_NULL_HANDLE;
	std::mutex * qm = nullptr;
	unsigned char uuid[16] = {};
	double tsPeriod = 1.0;
	int tsBits = 0;
	VkPhysicalDeviceMemoryProperties mp {};
#define VKFRAME_MEMBER( f ) PFN_##f f = nullptr;
	VKFRAME_DEVICE_FNS( VKFRAME_MEMBER )
	VKFRAME_INSTANCE_FNS( VKFRAME_MEMBER )
#undef VKFRAME_MEMBER
	PfnGetMemoryWin32 getMemWin32 = nullptr;
	PfnGetSemaphoreWin32 getSemWin32 = nullptr;
	VkPipelineCache cache = VK_NULL_HANDLE;
	VkDescriptorSetLayout setLayout = VK_NULL_HANDLE;
	VkPipelineLayout pipeLayout = VK_NULL_HANDLE;
	VkSampler linearClamp = VK_NULL_HANDLE;
	VkCommandPool cpool = VK_NULL_HANDLE;
	VkPipeline pipe[5] = {};	// down, hold, up, source (round 3, P3), combine (round 3, P2)
	int pipesWanted = 5, pipesBuilt = 0;
	QString cacheWords;
	// the frames in flight: a ring of slots, each a command buffer, a fence and two timestamps
	static constexpr int kSlots = 8;
	VkCommandBuffer cb[kSlots] = {};
	VkFence fence[kSlots] = {};
	VkQueryPool qpool = VK_NULL_HANDLE;
	std::uint64_t frameOf[kSlots] = {};
	bool pending[kSlots] = {};
	std::uint64_t frame = 0;
};

Dev & dv()
{
	static Dev d;
	return d;
}

// the BloomBindingsVk.hlsli push constants (48 bytes)
struct Job
{
	float params[4];
	float spare[4];
	int32_t ints[4];
};

// GL_EXT_memory_object(_win32) + GL_EXT_semaphore(_win32) for textures, loaded for the current context
typedef void ( APIENTRY * PfnCreateMemoryObjects )( GLsizei, GLuint * );
typedef void ( APIENTRY * PfnDeleteMemoryObjects )( GLsizei, const GLuint * );
typedef void ( APIENTRY * PfnMemoryObjectParameteriv )( GLuint, GLenum, const GLint * );
typedef void ( APIENTRY * PfnImportMemoryWin32Handle )( GLuint, std::uint64_t, GLenum, void * );
typedef void ( APIENTRY * PfnTexStorageMem2D )( GLenum, GLsizei, GLenum, GLsizei, GLsizei, GLuint, std::uint64_t );
typedef void ( APIENTRY * PfnTexStorageMem3D )( GLenum, GLsizei, GLenum, GLsizei, GLsizei, GLsizei, GLuint, std::uint64_t );
typedef void ( APIENTRY * PfnTexStorageMem2DMs )( GLenum, GLsizei, GLenum, GLsizei, GLsizei, GLboolean, GLuint, std::uint64_t );
typedef void ( APIENTRY * PfnGenSemaphores )( GLsizei, GLuint * );
typedef void ( APIENTRY * PfnDeleteSemaphores )( GLsizei, const GLuint * );
typedef void ( APIENTRY * PfnImportSemaphoreWin32Handle )( GLuint, GLenum, void * );
typedef void ( APIENTRY * PfnSemaphoreOp )( GLuint, GLuint, const GLuint *, GLuint, const GLuint *, const GLenum * );
typedef void ( APIENTRY * PfnGetUnsignedBytei )( GLenum, GLuint, GLubyte * );
constexpr GLenum kGlDedicatedMemoryObject = 0x9581;   // GL_DEDICATED_MEMORY_OBJECT_EXT
constexpr GLenum kGlHandleOpaqueWin32 = 0x9587;       // GL_HANDLE_TYPE_OPAQUE_WIN32_EXT
constexpr GLenum kGlNumDeviceUuids = 0x9596;          // GL_NUM_DEVICE_UUIDS_EXT
constexpr GLenum kGlDeviceUuid = 0x9597;              // GL_DEVICE_UUID_EXT
constexpr GLenum kGlLayoutGeneral = 0x958D;           // GL_LAYOUT_GENERAL_EXT
constexpr GLenum kGlRgba16f = 0x881A;                 // GL_RGBA16F
constexpr GLenum kGlRgba32f = 0x8814;                 // GL_RGBA32F
constexpr GLenum kGlD24S8 = 0x88F0;                   // GL_DEPTH24_STENCIL8
constexpr GLenum kGlD32FS8 = 0x8CAD;                  // GL_DEPTH32F_STENCIL8
constexpr GLenum kGlTexture2DMultisample = 0x9100;    // GL_TEXTURE_2D_MULTISAMPLE
constexpr GLenum kGlDepthStencilTextureMode = 0x90EA; // GL_DEPTH_STENCIL_TEXTURE_MODE
constexpr GLenum kGlStencilIndex = 0x1901;            // GL_STENCIL_INDEX
constexpr GLenum kGlTexture2DArray = 0x8C1A;          // GL_TEXTURE_2D_ARRAY
constexpr GLenum kGlClampToEdge = 0x812F;             // GL_CLAMP_TO_EDGE
constexpr GLenum kGlTextureMaxLevel = 0x813D;         // GL_TEXTURE_MAX_LEVEL

struct GlShare
{
	bool tried = false, ok = false;
	const QOpenGLContext * ctx = nullptr;
	QString why;
	PfnCreateMemoryObjects createMem = nullptr;
	PfnDeleteMemoryObjects deleteMem = nullptr;
	PfnMemoryObjectParameteriv memParam = nullptr;
	PfnImportMemoryWin32Handle importMem = nullptr;
	PfnTexStorageMem2D storage2D = nullptr;
	PfnTexStorageMem3D storage3D = nullptr;
	PfnTexStorageMem2DMs storage2DMs = nullptr;	// round 3: the shared frame (makeFrame refuses without it)
	PfnGenSemaphores genSem = nullptr;
	PfnDeleteSemaphores deleteSem = nullptr;
	PfnImportSemaphoreWin32Handle importSem = nullptr;
	PfnSemaphoreOp waitSem = nullptr, signalSem = nullptr;
	PfnGetUnsignedBytei uuidAt = nullptr;
	bool uuidChecked = false, uuidMatch = false;
};

GlShare & glShare()
{
	static GlShare g;
	QOpenGLContext * q = QOpenGLContext::currentContext();
	if ( g.tried && g.ctx == q )
		return g;
	g = GlShare();
	g.tried = true;
	g.ctx = q;
	if ( !q ) {
		g.why = QStringLiteral( "no GL context" );
		return g;
	}
	for ( const char * e : { "GL_EXT_memory_object", "GL_EXT_memory_object_win32", "GL_EXT_semaphore", "GL_EXT_semaphore_win32" } )
		if ( !q->hasExtension( e ) ) {
			g.why = QStringLiteral( "no GL memory/semaphore import (the GL context has no %1)" ).arg( QLatin1String( e ) );
			return g;
		}
	g.createMem = reinterpret_cast<PfnCreateMemoryObjects>( q->getProcAddress( "glCreateMemoryObjectsEXT" ) );
	g.deleteMem = reinterpret_cast<PfnDeleteMemoryObjects>( q->getProcAddress( "glDeleteMemoryObjectsEXT" ) );
	g.memParam = reinterpret_cast<PfnMemoryObjectParameteriv>( q->getProcAddress( "glMemoryObjectParameterivEXT" ) );
	g.importMem = reinterpret_cast<PfnImportMemoryWin32Handle>( q->getProcAddress( "glImportMemoryWin32HandleEXT" ) );
	g.storage2D = reinterpret_cast<PfnTexStorageMem2D>( q->getProcAddress( "glTexStorageMem2DEXT" ) );
	g.storage3D = reinterpret_cast<PfnTexStorageMem3D>( q->getProcAddress( "glTexStorageMem3DEXT" ) );
	g.storage2DMs = reinterpret_cast<PfnTexStorageMem2DMs>( q->getProcAddress( "glTexStorageMem2DMultisampleEXT" ) );
	g.genSem = reinterpret_cast<PfnGenSemaphores>( q->getProcAddress( "glGenSemaphoresEXT" ) );
	g.deleteSem = reinterpret_cast<PfnDeleteSemaphores>( q->getProcAddress( "glDeleteSemaphoresEXT" ) );
	g.importSem = reinterpret_cast<PfnImportSemaphoreWin32Handle>( q->getProcAddress( "glImportSemaphoreWin32HandleEXT" ) );
	g.waitSem = reinterpret_cast<PfnSemaphoreOp>( q->getProcAddress( "glWaitSemaphoreEXT" ) );
	g.signalSem = reinterpret_cast<PfnSemaphoreOp>( q->getProcAddress( "glSignalSemaphoreEXT" ) );
	g.uuidAt = reinterpret_cast<PfnGetUnsignedBytei>( q->getProcAddress( "glGetUnsignedBytei_vEXT" ) );
	g.ok = g.createMem && g.deleteMem && g.memParam && g.importMem && g.storage2D && g.storage3D && g.genSem && g.deleteSem
		&& g.importSem && g.waitSem && g.signalSem && g.uuidAt;
	if ( !g.ok )
		g.why = QStringLiteral( "no GL memory/semaphore import (a GL_EXT_memory_object / semaphore entry point is missing)" );
	return g;
}

// the GL context and the Vulkan device are the same GPU (GL_EXT_memory_object's device UUIDs)
bool sameGpu( GlShare & g, QString & why )
{
	if ( !g.uuidChecked ) {
		g.uuidChecked = true;
		QOpenGLFunctions * f = QOpenGLContext::currentContext()->functions();
		GLint n = 0;
		f->glGetIntegerv( kGlNumDeviceUuids, &n );
		for ( GLint i = 0; i < n && !g.uuidMatch; i++ ) {
			GLubyte u[16] = {};
			g.uuidAt( kGlDeviceUuid, GLuint( i ), u );
			g.uuidMatch = std::memcmp( u, dv().uuid, 16 ) == 0;
		}
	}
	if ( !g.uuidMatch )
		why = QStringLiteral( "no GL memory/semaphore import (the GL context runs on another GPU than the Vulkan device)" );
	return g.uuidMatch;
}

uint32_t memType( uint32_t bits, VkMemoryPropertyFlags want )
{
	const VkPhysicalDeviceMemoryProperties & mp = dv().mp;
	for ( uint32_t i = 0; i < mp.memoryTypeCount; i++ )
		if ( ( bits & ( 1u << i ) ) && ( mp.memoryTypes[i].propertyFlags & want ) == want )
			return i;
	return UINT32_MAX;
}

bool isDsFormat( VkFormat fmt )
{
	return fmt == VK_FORMAT_D24_UNORM_S8_UINT || fmt == VK_FORMAT_D32_SFLOAT_S8_UINT;
}

GLenum glFormatOf( VkFormat fmt )
{
	return fmt == VK_FORMAT_R32G32B32A32_SFLOAT ? kGlRgba32f : fmt == VK_FORMAT_D24_UNORM_S8_UINT ? kGlD24S8
		: fmt == VK_FORMAT_D32_SFLOAT_S8_UINT ? kGlD32FS8 : kGlRgba16f;
}

/* what GL does with each kind of shared image: the chain's are sampled, stored, drawn and blitted; the frame's multisample
 * colour and depth / stencil are drawn, sampled and blitted (no storage: multisample storage is an optional feature) */
VkImageUsageFlags imgUsage( VkFormat fmt, int samples )
{
	const VkImageUsageFlags base = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	if ( isDsFormat( fmt ) )
		return base | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
	if ( samples > 1 )
		return base | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	return base | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
}

// the format at this sample count can be made with these uses and shared both ways with GL (OPAQUE_WIN32)
bool exportable( VkFormat fmt, int samples, QString & why )
{
	Dev & d = dv();
	VkPhysicalDeviceExternalImageFormatInfo xi { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_IMAGE_FORMAT_INFO };
	xi.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT;
	VkPhysicalDeviceImageFormatInfo2 fi { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_FORMAT_INFO_2 };
	fi.pNext = &xi;
	fi.format = fmt;
	fi.type = VK_IMAGE_TYPE_2D;
	fi.tiling = VK_IMAGE_TILING_OPTIMAL;
	fi.usage = imgUsage( fmt, samples );
	VkExternalImageFormatProperties xp { VK_STRUCTURE_TYPE_EXTERNAL_IMAGE_FORMAT_PROPERTIES };
	VkImageFormatProperties2 fp { VK_STRUCTURE_TYPE_IMAGE_FORMAT_PROPERTIES_2 };
	fp.pNext = &xp;
	const VkExternalMemoryFeatureFlags needF = VK_EXTERNAL_MEMORY_FEATURE_EXPORTABLE_BIT | VK_EXTERNAL_MEMORY_FEATURE_IMPORTABLE_BIT;
	if ( d.vkGetPhysicalDeviceImageFormatProperties2( d.pd, &fi, &fp ) != VK_SUCCESS
		|| ( xp.externalMemoryProperties.externalMemoryFeatures & needF ) != needF
		|| !( fp.imageFormatProperties.sampleCounts & VkSampleCountFlags( samples ) ) ) {
		why = QStringLiteral( "Vulkan format %1 at %2 sample(s) cannot be shared with GL here" ).arg( int( fmt ) ).arg( samples );
		return false;
	}
	return true;
}

// one image Vulkan made and GL imported (RGBA16F, GENERAL layout on the Vulkan side for its whole life)
struct Img
{
	VkImage i = VK_NULL_HANDLE;
	VkDeviceMemory m = VK_NULL_HANDLE;
	VkImageView v = VK_NULL_HANDLE;	// level 0, every layer (2D, or 2D_ARRAY for the ring); a depth / stencil: its stencil
	VkImageView layer[WwVkPost::kRingLayers] = {};
	GLuint tex = 0, mem = 0;
	int w = 0, h = 0, layers = 1, levels = 1, samples = 1;
	VkImageAspectFlags aspect = VK_IMAGE_ASPECT_COLOR_BIT;
};

struct Chain
{
	const QOpenGLContext * ctx = nullptr;
	bool ok = false;
	int W = 0, H = 0, mipCount = 0;
	int dims[WwVkPost::kMaxMips][2] = {};
	Img src, ring, mip[WwVkPost::kMaxMips], up[WwVkPost::kMaxMips];
	VkDescriptorPool pool = VK_NULL_HANDLE;
	VkDescriptorSet down[WwVkPost::kMaxMips] = {}, downRing[WwVkPost::kRingLayers] = {}, hold = VK_NULL_HANDLE,
		upSet[WwVkPost::kMaxMips] = {};
	/* round 3: the source's set (5, 6 = the shared frame's colour and stencil, written again when that frame is made
	 * again: srcFrame) and the combine's (7 = comb, made the first frame the combine is asked for) */
	VkDescriptorSet srcSet = VK_NULL_HANDLE, combSet = VK_NULL_HANDLE;
	VkImage srcFrame = VK_NULL_HANDLE;
	Img comb;
	bool combFailed = false, combRan = false;
	VkSemaphore vkGlDone = VK_NULL_HANDLE, vkVkDone = VK_NULL_HANDLE;
	GLuint glGlDone = 0, glVkDone = 0;
	std::vector<GLuint> texs;
	std::vector<GLenum> layouts;
};

QHash<const void *, Chain> & chains()
{
	static QHash<const void *, Chain> c;
	return c;
}

// the last makeChain refusal per renderer (the route's reason while the chain stays GL's)
QHash<const void *, QString> & refusals()
{
	static QHash<const void *, QString> r;
	return r;
}

// round 3 (P3): the linear frame cellhdr.cpp draws into, shared: multisample colour and depth / stencil (view = stencil)
struct Frame
{
	const QOpenGLContext * ctx = nullptr;
	Img color, ds;
};

QHash<const void *, Frame> & frames()
{
	static QHash<const void *, Frame> f;
	return f;
}

bool makeImg( Img & I, int w, int h, int layers, int levels, QString & why, VkFormat fmt = VK_FORMAT_R16G16B16A16_SFLOAT,
	int samples = 1 )
{
	Dev & d = dv();
	GlShare & g = glShare();
	I.w = w;
	I.h = h;
	I.layers = layers;
	I.levels = levels;
	I.samples = samples;
	const bool isDs = isDsFormat( fmt );
	I.aspect = isDs ? VkImageAspectFlags( VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT ) : VkImageAspectFlags( VK_IMAGE_ASPECT_COLOR_BIT );
	VkExternalMemoryImageCreateInfo ext { VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO };
	ext.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT;
	VkImageCreateInfo ci { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
	ci.pNext = &ext;
	ci.imageType = VK_IMAGE_TYPE_2D;
	ci.format = fmt;
	ci.extent = { uint32_t( w ), uint32_t( h ), 1 };
	ci.mipLevels = uint32_t( levels );
	ci.arrayLayers = uint32_t( layers );
	ci.samples = VkSampleCountFlagBits( samples );
	ci.tiling = VK_IMAGE_TILING_OPTIMAL;
	// GL draws into them too (its route, the dump, the top's mips; the frame): attachment and transfer as well
	ci.usage = imgUsage( fmt, samples );
	ci.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	if ( d.vkCreateImage( d.dev, &ci, nullptr, &I.i ) != VK_SUCCESS ) {
		why = QStringLiteral( "vkCreateImage failed (%1x%2x%3)" ).arg( w ).arg( h ).arg( layers );
		return false;
	}
	VkMemoryRequirements r;
	d.vkGetImageMemoryRequirements( d.dev, I.i, &r );
	VkMemoryDedicatedAllocateInfo ded { VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO };
	ded.image = I.i;
	VkExportMemoryAllocateInfo exp { VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO };
	exp.pNext = &ded;
	exp.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT;
	VkMemoryAllocateInfo ai { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
	ai.pNext = &exp;
	ai.allocationSize = r.size;
	ai.memoryTypeIndex = memType( r.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );
	if ( ai.memoryTypeIndex == UINT32_MAX || d.vkAllocateMemory( d.dev, &ai, nullptr, &I.m ) != VK_SUCCESS
		|| d.vkBindImageMemory( d.dev, I.i, I.m, 0 ) != VK_SUCCESS ) {
		why = QStringLiteral( "no exportable device memory for a %1x%2 image" ).arg( w ).arg( h );
		return false;
	}
	VkImageViewCreateInfo vi { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
	vi.image = I.i;
	vi.viewType = layers > 1 ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D;
	vi.format = ci.format;
	vi.subresourceRange = { isDs ? VkImageAspectFlags( VK_IMAGE_ASPECT_STENCIL_BIT ) : VkImageAspectFlags( VK_IMAGE_ASPECT_COLOR_BIT ), 0,
		1, 0, uint32_t( layers ) };
	if ( d.vkCreateImageView( d.dev, &vi, nullptr, &I.v ) != VK_SUCCESS ) {
		why = QStringLiteral( "vkCreateImageView failed" );
		return false;
	}
	if ( layers > 1 )
		for ( int l = 0; l < layers && l < WwVkPost::kRingLayers; l++ ) {
			vi.viewType = VK_IMAGE_VIEW_TYPE_2D;
			vi.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, uint32_t( l ), 1 };
			if ( d.vkCreateImageView( d.dev, &vi, nullptr, &I.layer[l] ) != VK_SUCCESS ) {
				why = QStringLiteral( "vkCreateImageView (a ring layer) failed" );
				return false;
			}
		}
	// GL imports the memory (a dedicated allocation) and lays its texture over it; the handle closes after the import
	HANDLE mh = nullptr;
	MemoryWin32Get gi { kMemoryGetWin32, nullptr, I.m, VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT };
	if ( d.getMemWin32( d.dev, &gi, &mh ) != VK_SUCCESS || !mh ) {
		why = QStringLiteral( "vkGetMemoryWin32HandleKHR failed" );
		return false;
	}
	QOpenGLFunctions * f = QOpenGLContext::currentContext()->functions();
	while ( f->glGetError() != GL_NO_ERROR ) {}
	g.createMem( 1, &I.mem );
	const GLint one = 1;
	g.memParam( I.mem, kGlDedicatedMemoryObject, &one );
	g.importMem( I.mem, std::uint64_t( r.size ), kGlHandleOpaqueWin32, mh );
	CloseHandle( mh );
	f->glGenTextures( 1, &I.tex );
	const GLenum target = samples > 1 ? kGlTexture2DMultisample : layers > 1 ? kGlTexture2DArray : GLenum( GL_TEXTURE_2D );
	f->glBindTexture( target, I.tex );
	if ( samples > 1 )
		g.storage2DMs( target, samples, glFormatOf( fmt ), w, h, GL_TRUE, I.mem, 0 );
	else if ( layers > 1 )
		g.storage3D( target, levels, glFormatOf( fmt ), w, h, layers, I.mem, 0 );
	else
		g.storage2D( target, levels, glFormatOf( fmt ), w, h, I.mem, 0 );
	if ( samples > 1 ) {
		// a multisample texture has no filter; the frame's depth / stencil reads as its stencil (cellhdr.cpp's state)
		if ( isDs )
			f->glTexParameteri( target, kGlDepthStencilTextureMode, GLint( kGlStencilIndex ) );
	} else {
		// cellbloom.cpp makeTex's state: bilinear and clamped, one level read (the ring and the combine: nearest, as their GL twins)
		const GLint filter = layers > 1 || fmt == VK_FORMAT_R32G32B32A32_SFLOAT ? GL_NEAREST : GL_LINEAR;
		f->glTexParameteri( target, GL_TEXTURE_MIN_FILTER, filter );
		f->glTexParameteri( target, GL_TEXTURE_MAG_FILTER, filter );
		f->glTexParameteri( target, GL_TEXTURE_WRAP_S, kGlClampToEdge );
		f->glTexParameteri( target, GL_TEXTURE_WRAP_T, kGlClampToEdge );
		if ( layers == 1 )
			f->glTexParameteri( target, kGlTextureMaxLevel, 0 );
	}
	f->glBindTexture( target, 0 );
	const GLenum err = f->glGetError();
	if ( err != GL_NO_ERROR ) {
		why = QStringLiteral( "the GL import of a %1x%2 image failed, GL error %3" ).arg( w ).arg( h ).arg( err );
		return false;
	}
	return true;
}

void dropImg( Img & I )
{
	Dev & d = dv();
	GlShare & g = glShare();
	if ( QOpenGLContext::currentContext() ) {
		QOpenGLFunctions * f = QOpenGLContext::currentContext()->functions();
		if ( I.tex )
			f->glDeleteTextures( 1, &I.tex );
		if ( I.mem && g.deleteMem )
			g.deleteMem( 1, &I.mem );
	}
	for ( VkImageView & v : I.layer )
		if ( v )
			d.vkDestroyImageView( d.dev, v, nullptr );
	if ( I.v )
		d.vkDestroyImageView( d.dev, I.v, nullptr );
	if ( I.i )
		d.vkDestroyImage( d.dev, I.i, nullptr );
	if ( I.m )
		d.vkFreeMemory( d.dev, I.m, nullptr );
	I = Img();
}

bool makeSemaphore( VkSemaphore & s, GLuint & gs, QString & why )
{
	Dev & d = dv();
	GlShare & g = glShare();
	VkExportSemaphoreCreateInfo ex { VK_STRUCTURE_TYPE_EXPORT_SEMAPHORE_CREATE_INFO };
	ex.handleTypes = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_WIN32_BIT;
	VkSemaphoreCreateInfo si { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
	si.pNext = &ex;
	if ( d.vkCreateSemaphore( d.dev, &si, nullptr, &s ) != VK_SUCCESS ) {
		why = QStringLiteral( "vkCreateSemaphore (exportable) failed" );
		return false;
	}
	HANDLE h = nullptr;
	SemaphoreWin32Get gi { kSemaphoreGetWin32, nullptr, s, VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_WIN32_BIT };
	if ( d.getSemWin32( d.dev, &gi, &h ) != VK_SUCCESS || !h ) {
		why = QStringLiteral( "vkGetSemaphoreWin32HandleKHR failed" );
		return false;
	}
	g.genSem( 1, &gs );
	g.importSem( gs, kGlHandleOpaqueWin32, h );
	CloseHandle( h );
	return true;
}

// every slot whose frame the cap says to wait for (and the slot this frame reuses); their timestamps read back
void waitSlots( std::uint64_t upTo, int reuse )
{
	Dev & d = dv();
	const double t0 = nowMs();
	for ( int s = 0; s < Dev::kSlots; s++ ) {
		if ( !d.pending[s] || ( d.frameOf[s] > upTo && s != reuse ) )
			continue;
		d.vkWaitForFences( d.dev, 1, &d.fence[s], VK_TRUE, 1000000000ull );
		d.pending[s] = false;
		if ( d.tsBits > 0 ) {
			std::uint64_t ts[2] = {};
			if ( d.vkGetQueryPoolResults( d.dev, d.qpool, uint32_t( s * 2 ), 2, sizeof( ts ), ts, sizeof( std::uint64_t ),
					VK_QUERY_RESULT_64_BIT ) == VK_SUCCESS ) {
				const double ms = WwVkFrame::ticksToMs( ts[0], ts[1], d.tsPeriod, d.tsBits );
				win().vkMs += ms;
				win().vkN++;
				PassTime::subAddMs( PassTime::VkBloom, ms );
			}
		}
	}
	win().blockedMs += nowMs() - t0;
	WwSync::add( WwSync::CapFence, ( nowMs() - t0 ) * 1.0e6 );	// the Vulkan frames-in-flight cap is the same wait
}

void dropChain( Chain & c )
{
	Dev & d = dv();
	waitSlots( ~std::uint64_t( 0 ), -1 );	// nothing in flight reads these
	GlShare & g = glShare();
	if ( QOpenGLContext::currentContext() && g.deleteSem ) {
		if ( c.glGlDone )
			g.deleteSem( 1, &c.glGlDone );
		if ( c.glVkDone )
			g.deleteSem( 1, &c.glVkDone );
	}
	if ( c.vkGlDone )
		d.vkDestroySemaphore( d.dev, c.vkGlDone, nullptr );
	if ( c.vkVkDone )
		d.vkDestroySemaphore( d.dev, c.vkVkDone, nullptr );
	if ( c.pool )
		d.vkDestroyDescriptorPool( d.dev, c.pool, nullptr );
	dropImg( c.src );
	dropImg( c.ring );
	dropImg( c.comb );
	for ( int i = 0; i < WwVkPost::kMaxMips; i++ ) {
		dropImg( c.mip[i] );
		dropImg( c.up[i] );
	}
	c = Chain();
}

// one descriptor set: source, mip, ring (sampled), the sampler, the target (storage)
void writeSet( VkDescriptorSet set, VkImageView src, VkImageView mip, VkImageView ring, VkImageView dst )
{
	Dev & d = dv();
	VkDescriptorImageInfo ii[5] = { { VK_NULL_HANDLE, src, VK_IMAGE_LAYOUT_GENERAL }, { VK_NULL_HANDLE, mip, VK_IMAGE_LAYOUT_GENERAL },
		{ VK_NULL_HANDLE, ring, VK_IMAGE_LAYOUT_GENERAL }, { d.linearClamp, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_UNDEFINED },
		{ VK_NULL_HANDLE, dst, VK_IMAGE_LAYOUT_GENERAL } };
	const VkDescriptorType t[5] = { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
		VK_DESCRIPTOR_TYPE_SAMPLER, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE };
	VkWriteDescriptorSet wr[5];
	for ( uint32_t i = 0; i < 5; i++ ) {
		wr[i] = VkWriteDescriptorSet { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
		wr[i].dstSet = set;
		wr[i].dstBinding = i;
		wr[i].descriptorCount = 1;
		wr[i].descriptorType = t[i];
		wr[i].pImageInfo = &ii[i];
	}
	d.vkUpdateDescriptorSets( d.dev, 5, wr, 0, nullptr );
}

// round 3: one more image in a set (5, 6: the shared frame; 7: the combine's target)
void writeImage( VkDescriptorSet set, uint32_t binding, VkImageView v, VkDescriptorType t )
{
	Dev & d = dv();
	VkDescriptorImageInfo ii { VK_NULL_HANDLE, v, VK_IMAGE_LAYOUT_GENERAL };
	VkWriteDescriptorSet wr { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
	wr.dstSet = set;
	wr.dstBinding = binding;
	wr.descriptorCount = 1;
	wr.descriptorType = t;
	wr.pImageInfo = &ii;
	d.vkUpdateDescriptorSets( d.dev, 1, &wr, 0, nullptr );
}

// round 3: new images to GENERAL once (they stay there), waited on: a frame is made once per size
bool toGeneralNow( std::initializer_list<const Img *> imgs, QString & why )
{
	Dev & d = dv();
	std::vector<VkImageMemoryBarrier> bars;
	for ( const Img * I : imgs ) {
		VkImageMemoryBarrier b { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
		b.srcAccessMask = 0;
		b.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
		b.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		b.newLayout = VK_IMAGE_LAYOUT_GENERAL;
		b.srcQueueFamilyIndex = b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		b.image = I->i;
		b.subresourceRange = { I->aspect, 0, uint32_t( I->levels ), 0, uint32_t( I->layers ) };
		bars.push_back( b );
	}
	const std::uint64_t f = ++d.frame;
	const int slot = int( f % Dev::kSlots );
	waitSlots( 0, slot );
	VkCommandBuffer cb = d.cb[slot];
	d.vkResetCommandBuffer( cb, 0 );
	VkCommandBufferBeginInfo bi { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
	bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	d.vkBeginCommandBuffer( cb, &bi );
	d.vkCmdPipelineBarrier( cb, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr,
		uint32_t( bars.size() ), bars.data() );
	d.vkEndCommandBuffer( cb );
	d.vkResetFences( d.dev, 1, &d.fence[slot] );
	VkSubmitInfo sub { VK_STRUCTURE_TYPE_SUBMIT_INFO };
	sub.commandBufferCount = 1;
	sub.pCommandBuffers = &cb;
	bool ok;
	{
		std::unique_lock<std::mutex> lk;
		if ( d.qm )
			lk = std::unique_lock<std::mutex>( *d.qm );
		ok = d.vkQueueSubmit( d.q, 1, &sub, d.fence[slot] ) == VK_SUCCESS;
	}
	if ( ok )
		d.vkWaitForFences( d.dev, 1, &d.fence[slot], VK_TRUE, 1000000000ull );
	else
		why = QStringLiteral( "the layout submit failed" );
	return ok;
}

void computeBarrier( VkCommandBuffer cb )
{
	Dev & d = dv();
	VkMemoryBarrier mb { VK_STRUCTURE_TYPE_MEMORY_BARRIER };
	mb.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
	mb.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
	d.vkCmdPipelineBarrier( cb, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &mb, 0, nullptr, 0,
		nullptr );
}

void dispatch( VkCommandBuffer cb, int pipe, VkDescriptorSet set, int w, int h, const Job & j )
{
	Dev & d = dv();
	d.vkCmdBindPipeline( cb, VK_PIPELINE_BIND_POINT_COMPUTE, d.pipe[pipe] );
	d.vkCmdBindDescriptorSets( cb, VK_PIPELINE_BIND_POINT_COMPUTE, d.pipeLayout, 0, 1, &set, 0, nullptr );
	d.vkCmdPushConstants( cb, d.pipeLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof( Job ), &j );
	d.vkCmdDispatch( cb, uint32_t( ( w + 7 ) / 8 ), uint32_t( ( h + 7 ) / 8 ), 1 );
	computeBarrier( cb );
}

// the route's reason, logged when it changes (GL thread)
void sayRoute( const QString & why )
{
	static QString said;
	if ( said != why ) {
		said = why;
		qInfo().noquote() << "vk post: route" << why;
	}
}

QString cachePath()
{
	QString dir = QStandardPaths::writableLocation( QStandardPaths::CacheLocation );
	if ( dir.isEmpty() )
		dir = QDir::tempPath();
	QDir().mkpath( dir );
	return dir + QStringLiteral( "/vkframe_pipelines" ) + wwHarnessSettingsSuffix() + QStringLiteral( ".bin" );
}

}	// namespace

void wwVkFrameSetDevice( void * instance, void * physical, void * device, unsigned family, void * queue, void * getDeviceProcAddr,
	const unsigned char deviceUuid[16], std::mutex * queueLock )
{
	Dev & d = dv();
	if ( d.ready.load( std::memory_order_acquire ) || !instance || !physical || !device || !queue || !getDeviceProcAddr )
		return;
	const double t0 = nowMs();
	d.inst = static_cast<VkInstance>( instance );
	d.pd = static_cast<VkPhysicalDevice>( physical );
	d.dev = static_cast<VkDevice>( device );
	d.qf = family;
	d.q = static_cast<VkQueue>( queue );
	d.qm = queueLock;
	std::memcpy( d.uuid, deviceUuid, 16 );
	auto gdpa = reinterpret_cast<PFN_vkGetDeviceProcAddr>( getDeviceProcAddr );
#define VKFRAME_LOAD_D( f ) d.f = reinterpret_cast<PFN_##f>( gdpa( d.dev, #f ) );
	VKFRAME_DEVICE_FNS( VKFRAME_LOAD_D )
#undef VKFRAME_LOAD_D
	d.getMemWin32 = reinterpret_cast<PfnGetMemoryWin32>( gdpa( d.dev, "vkGetMemoryWin32HandleKHR" ) );
	d.getSemWin32 = reinterpret_cast<PfnGetSemaphoreWin32>( gdpa( d.dev, "vkGetSemaphoreWin32HandleKHR" ) );
	HMODULE lib = LoadLibraryA( "vulkan-1.dll" );	// already loaded by bakert: this only takes a reference
	auto gipa = lib ? reinterpret_cast<PFN_vkGetInstanceProcAddr>( reinterpret_cast<void *>( GetProcAddress( lib, "vkGetInstanceProcAddr" ) ) )
		: nullptr;
	if ( gipa ) {
#define VKFRAME_LOAD_I( f ) d.f = reinterpret_cast<PFN_##f>( gipa( d.inst, #f ) );
		VKFRAME_INSTANCE_FNS( VKFRAME_LOAD_I )
#undef VKFRAME_LOAD_I
	}
	bool loaded = d.getMemWin32 && d.getSemWin32 && d.vkGetPhysicalDeviceProperties && d.vkGetPhysicalDeviceImageFormatProperties2;
#define VKFRAME_CHECK( f ) loaded = loaded && d.f;
	VKFRAME_DEVICE_FNS( VKFRAME_CHECK )
#undef VKFRAME_CHECK
	if ( !loaded ) {
		d.why = QStringLiteral( "no device (a Vulkan entry point is missing: the Win32 export extensions off?)" );
		std::fprintf( stderr, "vk frame: device refused (%s)\n", qPrintable( d.why ) );
		return;
	}
	VkPhysicalDeviceProperties pp;
	d.vkGetPhysicalDeviceProperties( d.pd, &pp );
	d.tsPeriod = double( pp.limits.timestampPeriod );
	d.vkGetPhysicalDeviceMemoryProperties( d.pd, &d.mp );
	uint32_t nq = 0;
	d.vkGetPhysicalDeviceQueueFamilyProperties( d.pd, &nq, nullptr );
	std::vector<VkQueueFamilyProperties> qp( nq );
	if ( nq )
		d.vkGetPhysicalDeviceQueueFamilyProperties( d.pd, &nq, qp.data() );
	d.tsBits = d.qf < nq ? int( qp[d.qf].timestampValidBits ) : 0;

	// the images must be exportable as OPAQUE_WIN32 (and importable back)
	VkPhysicalDeviceExternalImageFormatInfo xi { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_IMAGE_FORMAT_INFO };
	xi.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT;
	VkPhysicalDeviceImageFormatInfo2 fi { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_FORMAT_INFO_2 };
	fi.pNext = &xi;
	fi.format = VK_FORMAT_R16G16B16A16_SFLOAT;
	fi.type = VK_IMAGE_TYPE_2D;
	fi.tiling = VK_IMAGE_TILING_OPTIMAL;
	fi.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
		| VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	VkExternalImageFormatProperties xp { VK_STRUCTURE_TYPE_EXTERNAL_IMAGE_FORMAT_PROPERTIES };
	VkImageFormatProperties2 fp { VK_STRUCTURE_TYPE_IMAGE_FORMAT_PROPERTIES_2 };
	fp.pNext = &xp;
	const VkExternalMemoryFeatureFlags needF = VK_EXTERNAL_MEMORY_FEATURE_EXPORTABLE_BIT | VK_EXTERNAL_MEMORY_FEATURE_IMPORTABLE_BIT;
	if ( d.vkGetPhysicalDeviceImageFormatProperties2( d.pd, &fi, &fp ) != VK_SUCCESS
		|| ( xp.externalMemoryProperties.externalMemoryFeatures & needF ) != needF ) {
		d.why = QStringLiteral( "no device (RGBA16F storage images cannot be exported to GL here)" );
		std::fprintf( stderr, "vk frame: device refused (%s)\n", qPrintable( d.why ) );
		return;
	}

	/* the layout every bloom pipeline shares (BloomBindingsVk.hlsli: 0 source, 1 mip, 2 ring, 3 sampler, 4 target; round 3:
	 * 5 the frame's colour, 6 its stencil, 7 the combine's target; 48 bytes). A set fills only what its kernel reads. */
	VkDescriptorSetLayoutBinding b[8];
	const VkDescriptorType t[8] = { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
		VK_DESCRIPTOR_TYPE_SAMPLER, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
		VK_DESCRIPTOR_TYPE_STORAGE_IMAGE };
	for ( uint32_t i = 0; i < 8; i++ )
		b[i] = VkDescriptorSetLayoutBinding { i, t[i], 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr };
	VkDescriptorSetLayoutCreateInfo li { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
	li.bindingCount = 8;
	li.pBindings = b;
	VkPushConstantRange pr { VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof( Job ) };
	VkPipelineLayoutCreateInfo pi { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
	pi.setLayoutCount = 1;
	pi.pSetLayouts = &d.setLayout;
	pi.pushConstantRangeCount = 1;
	pi.pPushConstantRanges = &pr;
	VkSamplerCreateInfo si { VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
	si.magFilter = si.minFilter = VK_FILTER_LINEAR;
	si.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
	si.addressModeU = si.addressModeV = si.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	si.maxLod = 0.0f;
	VkCommandPoolCreateInfo cpi { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
	cpi.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	cpi.queueFamilyIndex = d.qf;
	VkQueryPoolCreateInfo qi { VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO };
	qi.queryType = VK_QUERY_TYPE_TIMESTAMP;
	qi.queryCount = 2 * Dev::kSlots;
	if ( d.vkCreateDescriptorSetLayout( d.dev, &li, nullptr, &d.setLayout ) != VK_SUCCESS
		|| d.vkCreatePipelineLayout( d.dev, &pi, nullptr, &d.pipeLayout ) != VK_SUCCESS
		|| d.vkCreateSampler( d.dev, &si, nullptr, &d.linearClamp ) != VK_SUCCESS
		|| d.vkCreateCommandPool( d.dev, &cpi, nullptr, &d.cpool ) != VK_SUCCESS
		|| d.vkCreateQueryPool( d.dev, &qi, nullptr, &d.qpool ) != VK_SUCCESS ) {
		d.why = QStringLiteral( "no device (the bloom layout, sampler, command pool or query pool failed)" );
		std::fprintf( stderr, "vk frame: device refused (%s)\n", qPrintable( d.why ) );
		return;
	}
	VkCommandBufferAllocateInfo ca { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
	ca.commandPool = d.cpool;
	ca.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	ca.commandBufferCount = Dev::kSlots;
	bool slotsOk = d.vkAllocateCommandBuffers( d.dev, &ca, d.cb ) == VK_SUCCESS;
	for ( int s = 0; slotsOk && s < Dev::kSlots; s++ ) {
		VkFenceCreateInfo fci { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
		slotsOk = d.vkCreateFence( d.dev, &fci, nullptr, &d.fence[s] ) == VK_SUCCESS;
	}
	if ( !slotsOk ) {
		d.why = QStringLiteral( "no device (the frame slots' command buffers or fences failed)" );
		std::fprintf( stderr, "vk frame: device refused (%s)\n", qPrintable( d.why ) );
		return;
	}

	// every pipeline now, before any frame uses one, behind the on-disk pipeline cache
	const QString path = cachePath();
	QByteArray before;
	{
		QFile f( path );
		if ( f.open( QIODevice::ReadOnly ) )
			before = f.readAll();
	}
	VkPipelineCacheCreateInfo pci { VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO };
	pci.initialDataSize = size_t( before.size() );
	pci.pInitialData = before.isEmpty() ? nullptr : before.constData();
	if ( d.vkCreatePipelineCache( d.dev, &pci, nullptr, &d.cache ) != VK_SUCCESS ) {
		pci.initialDataSize = 0;	// a cache from another driver: start empty
		pci.pInitialData = nullptr;
		d.vkCreatePipelineCache( d.dev, &pci, nullptr, &d.cache );
	}
	// round 3: the source and the combine too, so no frame builds a pipeline (the tone map's 265 ms first-use hitch)
	const uint32_t * code[5] = { vkframe_bloom_down_spv, vkframe_bloom_hold_spv, vkframe_bloom_up_spv, vkframe_bloom_src_spv,
		vkframe_bloom_comb_spv };
	const size_t bytes[5] = { sizeof( vkframe_bloom_down_spv ), sizeof( vkframe_bloom_hold_spv ), sizeof( vkframe_bloom_up_spv ),
		sizeof( vkframe_bloom_src_spv ), sizeof( vkframe_bloom_comb_spv ) };
	d.pipesBuilt = 0;
	for ( int k = 0; k < 5; k++ ) {
		VkShaderModuleCreateInfo mi { VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
		mi.codeSize = bytes[k];
		mi.pCode = code[k];
		VkShaderModule m = VK_NULL_HANDLE;
		if ( d.vkCreateShaderModule( d.dev, &mi, nullptr, &m ) != VK_SUCCESS )
			continue;
		VkComputePipelineCreateInfo pc { VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO };
		pc.stage = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_COMPUTE_BIT, m, "main", nullptr };
		pc.layout = d.pipeLayout;
		if ( d.vkCreateComputePipelines( d.dev, d.cache, 1, &pc, nullptr, &d.pipe[k] ) == VK_SUCCESS )
			d.pipesBuilt++;
		d.vkDestroyShaderModule( d.dev, m, nullptr );
	}
	size_t n = 0;
	QByteArray after;
	if ( d.cache && d.vkGetPipelineCacheData( d.dev, d.cache, &n, nullptr ) == VK_SUCCESS && n ) {
		after.resize( qsizetype( n ) );
		if ( d.vkGetPipelineCacheData( d.dev, d.cache, &n, after.data() ) != VK_SUCCESS )
			after.clear();
	}
	bool wrote = false;
	if ( !after.isEmpty() && after != before ) {
		QFile f( path );
		wrote = f.open( QIODevice::WriteOnly ) && f.write( after ) == after.size();
	}
	d.cacheWords = QStringLiteral( "%1 bytes read, %2" ).arg( before.size() )
		.arg( wrote ? QStringLiteral( "%1 written" ).arg( after.size() ) : QStringLiteral( "unchanged" ) );
	if ( !WwVkFrame::pipelinesUsable( d.pipesWanted, d.pipesBuilt, 0 ) ) {
		d.why = QStringLiteral( "pipelines not built (%1 of %2)" ).arg( d.pipesBuilt ).arg( d.pipesWanted );
		std::fprintf( stderr, "vk frame: device refused (%s)\n", qPrintable( d.why ) );
		return;
	}
	d.why = QStringLiteral( "vulkan" );
	std::fprintf( stderr, "vk frame: device registered (queue family %u, timestamps %d bits), %d/%d pipelines built in %.1f ms "
		"(pipeline cache %s: %s)\n", d.qf, d.tsBits, d.pipesBuilt, d.pipesWanted, nowMs() - t0, qPrintable( path ),
		qPrintable( d.cacheWords ) );
	d.ready.store( true, std::memory_order_release );
}

namespace WwVkPost
{

bool wanted()
{
	if ( wwEnvInt( "WW_VKPOST_RED" ) == 1 )
		return false;
	// Settings > Ray tracing device > Vulkan Post, read again each second (written on Apply / OK)
	static double at = -1.0e9;
	static bool row = true;
	const double now = nowMs();
	if ( now - at >= 1000.0 ) {
		at = now;
		QSettings s( "NifTools", "NifSkope " + NifSkopeVersion::rawToMajMin( NIFSKOPE_VERSION ) + wwHarnessSettingsSuffix() );
		row = s.value( "Settings/Nif/Vulkan Post", true ).toBool();
	}
	return row;
}

bool ready()
{
	return dv().ready.load( std::memory_order_acquire ) && wanted();
}

bool hasChain( const void * key )
{
	auto it = chains().find( key );
	return it != chains().end() && it->ok;
}

void freeChain( const void * key )
{
	auto it = chains().find( key );
	if ( it == chains().end() )
		return;
	dropChain( *it );
	chains().erase( it );
}

bool makeChain( const void * key, int W, int H, int mipCount, const int dims[][2], unsigned & src, unsigned mip[], unsigned up[],
	unsigned & ring, QString & why )
{
	Dev & d = dv();
	freeChain( key );
	refusals().remove( key );
	struct Refused	// any refusal below is remembered for the route's reason
	{
		const void * k;
		QString & w;
		bool made = false;
		~Refused() { if ( !made ) refusals()[k] = w; }
	} refused { key, why };
	GlShare & g = glShare();
	const bool device = d.ready.load( std::memory_order_acquire );
	const bool glImport = device && g.ok && sameGpu( g, why );
	const WwVkFrame::Route rt = WwVkFrame::route( device, glImport, device && d.pipesBuilt >= d.pipesWanted, !wanted() );
	if ( rt.side != WwVkFrame::Side::Vk ) {
		if ( why.isEmpty() )
			why = !device ? d.why : !g.ok ? g.why : QString::fromLatin1( rt.why );
		return false;
	}
	if ( mipCount < 1 || mipCount > kMaxMips ) {
		why = QStringLiteral( "no mip chain" );
		return false;
	}
	Chain & c = chains()[key];
	c.ctx = QOpenGLContext::currentContext();
	c.W = W;
	c.H = H;
	c.mipCount = mipCount;
	// the top (up 0, or mip 0 alone) keeps 4 levels: cellbloom.cpp grows its mips for Bloom Mip (MAX_LEVEL 3)
	auto topLevels = [&]( int w, int h ) {
		return std::min( 4, int( std::floor( std::log2( double( std::max( w, h ) ) ) ) ) + 1 );
	};
	bool ok = makeImg( c.src, W, H, 1, 1, why ) && makeImg( c.ring, dims[0][0], dims[0][1], kRingLayers, 1, why );
	for ( int i = 0; ok && i < mipCount; i++ ) {
		c.dims[i][0] = dims[i][0];
		c.dims[i][1] = dims[i][1];
		ok = makeImg( c.mip[i], dims[i][0], dims[i][1], 1, mipCount == 1 ? topLevels( dims[0][0], dims[0][1] ) : 1, why );
		if ( ok && i + 1 < mipCount )
			ok = makeImg( c.up[i], dims[i][0], dims[i][1], 1, i == 0 ? topLevels( dims[0][0], dims[0][1] ) : 1, why );
	}
	ok = ok && makeSemaphore( c.vkGlDone, c.glGlDone, why ) && makeSemaphore( c.vkVkDone, c.glVkDone, why );
	/* the descriptor sets, once per chain shape: down per level, level 0 into each ring layer, the hold, up per level; round 3:
	 * the source and the combine (each set takes the layout's 5 sampled images, 1 sampler, 2 storage images from the pool) */
	const int sets = mipCount + kRingLayers + 1 + mipCount + 2;
	if ( ok ) {
		VkDescriptorPoolSize ps[3] = { { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, uint32_t( 5 * sets ) }, { VK_DESCRIPTOR_TYPE_SAMPLER, uint32_t( sets ) },
			{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, uint32_t( 2 * sets ) } };
		VkDescriptorPoolCreateInfo dp { VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
		dp.maxSets = uint32_t( sets );
		dp.poolSizeCount = 3;
		dp.pPoolSizes = ps;
		std::vector<VkDescriptorSetLayout> lay( size_t( sets ), d.setLayout );
		std::vector<VkDescriptorSet> all( static_cast<size_t>( sets ) );
		VkDescriptorSetAllocateInfo ai { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
		ai.descriptorSetCount = uint32_t( sets );
		ai.pSetLayouts = lay.data();
		ok = d.vkCreateDescriptorPool( d.dev, &dp, nullptr, &c.pool ) == VK_SUCCESS;
		ai.descriptorPool = c.pool;
		ok = ok && d.vkAllocateDescriptorSets( d.dev, &ai, all.data() ) == VK_SUCCESS;
		if ( !ok ) {
			why = QStringLiteral( "the bloom descriptor sets failed" );
		} else {
			int k = 0;
			for ( int i = 0; i < mipCount; i++ ) {
				c.down[i] = all[size_t( k++ )];
				writeSet( c.down[i], i == 0 ? c.src.v : c.mip[i - 1].v, c.src.v, c.ring.v, c.mip[i].v );
			}
			for ( int l = 0; l < kRingLayers; l++ ) {
				c.downRing[l] = all[size_t( k++ )];
				writeSet( c.downRing[l], c.src.v, c.src.v, c.ring.v, c.ring.layer[l] );
			}
			c.hold = all[size_t( k++ )];
			writeSet( c.hold, c.src.v, c.src.v, c.ring.v, c.mip[0].v );
			for ( int level = 0; level + 1 < mipCount; level++ ) {
				c.upSet[level] = all[size_t( k++ )];
				const VkImageView acc = level + 2 == mipCount ? c.mip[mipCount - 1].v : c.up[level + 1].v;
				writeSet( c.upSet[level], c.mip[level].v, acc, c.ring.v, c.up[level].v );
			}
			// round 3: the source writes src (5, 6 when a frame is shared); the combine reads the top (7 when it is made)
			c.srcSet = all[size_t( k++ )];
			writeSet( c.srcSet, c.src.v, c.src.v, c.ring.v, c.src.v );
			c.combSet = all[size_t( k++ )];
			const VkImageView top = mipCount == 1 ? c.mip[0].v : c.up[0].v;
			writeSet( c.combSet, top, top, c.ring.v, c.src.v );
		}
	}
	// every image to GENERAL once (it stays there: GL reads and writes it in that layout too)
	if ( ok ) {
		std::vector<VkImageMemoryBarrier> bars;
		auto toGeneral = [&]( const Img & I ) {
			if ( !I.i )
				return;
			VkImageMemoryBarrier b { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
			b.srcAccessMask = 0;
			b.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
			b.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			b.newLayout = VK_IMAGE_LAYOUT_GENERAL;
			b.srcQueueFamilyIndex = b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			b.image = I.i;
			b.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, uint32_t( I.levels ), 0, uint32_t( I.layers ) };
			bars.push_back( b );
		};
		toGeneral( c.src );
		toGeneral( c.ring );
		for ( int i = 0; i < mipCount; i++ ) {
			toGeneral( c.mip[i] );
			toGeneral( c.up[i] );
		}
		const std::uint64_t f = ++d.frame;
		const int slot = int( f % Dev::kSlots );
		waitSlots( 0, slot );
		VkCommandBuffer cb = d.cb[slot];
		d.vkResetCommandBuffer( cb, 0 );
		VkCommandBufferBeginInfo bi { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
		bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		d.vkBeginCommandBuffer( cb, &bi );
		d.vkCmdPipelineBarrier( cb, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr,
			uint32_t( bars.size() ), bars.data() );
		d.vkEndCommandBuffer( cb );
		d.vkResetFences( d.dev, 1, &d.fence[slot] );
		VkSubmitInfo sub { VK_STRUCTURE_TYPE_SUBMIT_INFO };
		sub.commandBufferCount = 1;
		sub.pCommandBuffers = &cb;
		{
			std::unique_lock<std::mutex> lk;
			if ( d.qm )
				lk = std::unique_lock<std::mutex>( *d.qm );
			ok = d.vkQueueSubmit( d.q, 1, &sub, d.fence[slot] ) == VK_SUCCESS;
		}
		if ( ok ) {
			d.vkWaitForFences( d.dev, 1, &d.fence[slot], VK_TRUE, 1000000000ull );
		} else {
			why = QStringLiteral( "the layout submit failed" );
		}
	}
	if ( !ok ) {
		dropChain( c );
		chains().remove( key );
		return false;
	}
	c.texs.clear();
	auto keep = [&]( const Img & I ) {
		if ( I.tex )
			c.texs.push_back( I.tex );
	};
	keep( c.src );
	keep( c.ring );
	src = c.src.tex;
	ring = c.ring.tex;
	for ( int i = 0; i < mipCount; i++ ) {
		keep( c.mip[i] );
		keep( c.up[i] );
		mip[i] = c.mip[i].tex;
		up[i] = c.up[i].tex;
	}
	c.layouts.assign( c.texs.size(), kGlLayoutGeneral );
	c.ok = true;
	refused.made = true;
	double mb = ( double( W ) * H + double( dims[0][0] ) * dims[0][1] * kRingLayers ) * 8.0;
	for ( int i = 0; i < mipCount; i++ )
		mb += double( dims[i][0] ) * dims[i][1] * 8.0 * ( i + 1 < mipCount ? 2.0 : 1.0 );
	std::fprintf( stderr, "vk post: bloom chain shared %dx%d, %d mips, %d images (%.1f MB), %d descriptor sets\n", W, H, mipCount,
		int( c.texs.size() ), mb / 1048576.0, sets );
	return true;
}

bool makeFrame( const void * key, int W, int H, int samples, unsigned dsFormat, unsigned & color, unsigned & ds, QString & why )
{
	Dev & d = dv();
	freeFrame( key );
	GlShare & g = glShare();
	const bool device = d.ready.load( std::memory_order_acquire );
	const bool glImport = device && g.ok && sameGpu( g, why );
	const WwVkFrame::Route rt = WwVkFrame::route( device, glImport, device && d.pipesBuilt >= d.pipesWanted, !wanted() );
	if ( rt.side != WwVkFrame::Side::Vk ) {
		if ( why.isEmpty() )
			why = !device ? d.why : !g.ok ? g.why : QString::fromLatin1( rt.why );
		return false;
	}
	if ( !g.storage2DMs ) {
		why = QStringLiteral( "no GL multisample import (glTexStorageMem2DMultisampleEXT is missing)" );
		return false;
	}
	const VkFormat dsf = dsFormat == kGlD24S8 ? VK_FORMAT_D24_UNORM_S8_UINT
		: dsFormat == kGlD32FS8 ? VK_FORMAT_D32_SFLOAT_S8_UINT : VK_FORMAT_UNDEFINED;
	if ( dsf == VK_FORMAT_UNDEFINED || W < 1 || H < 1 || samples < 1 || samples > 64 || ( samples & ( samples - 1 ) ) ) {
		why = QStringLiteral( "no Vulkan twin for the frame (depth format 0x%1, %2 samples)" ).arg( dsFormat, 0, 16 ).arg( samples );
		return false;
	}
	if ( !exportable( VK_FORMAT_R16G16B16A16_SFLOAT, samples, why ) || !exportable( dsf, samples, why ) )
		return false;
	Frame & fr = frames()[key];
	fr.ctx = QOpenGLContext::currentContext();
	const bool ok = makeImg( fr.color, W, H, 1, 1, why, VK_FORMAT_R16G16B16A16_SFLOAT, samples )
		&& makeImg( fr.ds, W, H, 1, 1, why, dsf, samples ) && toGeneralNow( { &fr.color, &fr.ds }, why );
	if ( !ok ) {
		dropImg( fr.color );
		dropImg( fr.ds );
		frames().remove( key );
		return false;
	}
	color = fr.color.tex;
	ds = fr.ds.tex;
	const double mb = double( W ) * H * samples / 1048576.0;
	std::fprintf( stderr, "vk post: frame shared %dx%d, %d sample(s): colour RGBA16F %.1f MB, depth / stencil %s %.1f MB\n", W, H,
		samples, mb * 8.0, dsf == VK_FORMAT_D24_UNORM_S8_UINT ? "D24S8" : "D32FS8", mb * ( dsf == VK_FORMAT_D24_UNORM_S8_UINT ? 4.0 : 8.0 ) );
	return true;
}

void freeFrame( const void * key )
{
	auto it = frames().find( key );
	if ( it == frames().end() )
		return;
	waitSlots( ~std::uint64_t( 0 ), -1 );	// nothing in flight reads them
	for ( Chain & c : chains() )
		c.srcFrame = VK_NULL_HANDLE;	// a new frame can reuse the handle: its set is written again
	dropImg( it->color );
	dropImg( it->ds );
	frames().erase( it );
}

bool sourceReady( const void * key, unsigned color, unsigned ds )
{
	const auto it = chains().constFind( key );
	const auto ft = frames().constFind( key );
	const QOpenGLContext * q = QOpenGLContext::currentContext();
	return it != chains().constEnd() && it->ok && it->ctx == q && ft != frames().constEnd() && ft->ctx == q && color
		&& ft->color.tex == color && ft->ds.tex == ds && wanted();
}

unsigned combTexture( const void * key )
{
	const auto it = chains().constFind( key );
	return it != chains().constEnd() && it->ok && it->combRan ? it->comb.tex : 0u;
}

bool runBloom( const void * key, const BloomJob & job, QString & why )
{
	Dev & d = dv();
	auto it = chains().find( key );
	if ( it == chains().end() || !it->ok ) {
		const QString made = refusals().value( key );
		why = !d.ready.load( std::memory_order_acquire ) ? d.why
			: !wanted() ? ( wwEnvInt( "WW_VKPOST_RED" ) == 1 ? QStringLiteral( "red (WW_VKPOST_RED=1)" ) : QStringLiteral( "the Vulkan Post row is off" ) )
			: made.isEmpty() ? QStringLiteral( "the chain's images are GL's (made before the route was Vulkan)" )
			: QStringLiteral( "the shared images were refused: %1" ).arg( made );
		win().glFrames++;
		win().glWhy = why;
		sayRoute( QStringLiteral( "gl (%1)" ).arg( why ) );
		return false;
	}
	if ( !wanted() ) {
		why = wwEnvInt( "WW_VKPOST_RED" ) == 1 ? QStringLiteral( "red (WW_VKPOST_RED=1)" ) : QStringLiteral( "the Vulkan Post row is off" );
		win().glFrames++;
		win().glWhy = why;
		sayRoute( QStringLiteral( "gl (%1)" ).arg( why ) );
		return false;
	}
	Chain & c = *it;
	GlShare & g = glShare();
	if ( c.ctx != QOpenGLContext::currentContext() || !g.ok ) {
		why = QStringLiteral( "another GL context" );
		win().glFrames++;
		win().glWhy = why;
		sayRoute( QStringLiteral( "gl (%1)" ).arg( why ) );
		return false;
	}
	c.combRan = false;
	// round 3 (P3): the source's frame; the set is written again only when that frame was made again
	const Frame * fr = nullptr;
	if ( job.src ) {
		const auto ft = frames().constFind( key );
		if ( ft == frames().constEnd() || !ft->color.i || ft->color.tex != job.frameColor || ft->ds.tex != job.frameDs ) {
			why = QStringLiteral( "the source's frame is not the shared one" );
			win().glFrames++;
			win().glWhy = why;
			sayRoute( QStringLiteral( "gl (%1)" ).arg( why ) );
			return false;
		}
		fr = &*ft;
		if ( c.srcFrame != fr->color.i ) {
			waitSlots( ~std::uint64_t( 0 ), -1 );	// no frame in flight reads the set
			writeImage( c.srcSet, 5, fr->color.v, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE );
			writeImage( c.srcSet, 6, fr->ds.v, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE );
			c.srcFrame = fr->color.i;
		}
	}
	// round 3 (P2): the combine's RGBA32F target, made the first frame the combine is asked for (an allocation, no pipeline)
	bool combFresh = false;
	if ( job.comb && !c.comb.i && !c.combFailed ) {
		QString cw;
		if ( makeImg( c.comb, c.W, c.H, 1, 1, cw, VK_FORMAT_R32G32B32A32_SFLOAT ) ) {
			writeImage( c.combSet, 7, c.comb.v, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE );
			c.texs.push_back( c.comb.tex );
			c.layouts.push_back( kGlLayoutGeneral );
			combFresh = true;
			std::fprintf( stderr, "vk post: bloom combine target shared %dx%d RGBA32F (%.1f MB)\n", c.W, c.H,
				double( c.W ) * c.H * 16.0 / 1048576.0 );
		} else {
			dropImg( c.comb );
			c.combFailed = true;
			std::fprintf( stderr, "vk post: the combine's target was refused (%s): GL combines\n", qPrintable( cw ) );
		}
	}
	const bool comb = job.comb && c.comb.i;
	// the frames in flight: wait for frame n - cap (2; WW_FRAMEQUEUE_RED=1: none), and for this slot's last use
	const std::uint64_t f = ++d.frame;
	const int slot = int( f % Dev::kSlots );
	const int cap = wwEnvInt( "WW_FRAMEQUEUE_RED" ) == 1 ? 0 : 2;
	waitSlots( WwVkFrame::inFlightWait( f, cap ), slot );

	VkCommandBuffer cb = d.cb[slot];
	d.vkResetCommandBuffer( cb, 0 );
	VkCommandBufferBeginInfo bi { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
	bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	d.vkBeginCommandBuffer( cb, &bi );
	if ( d.tsBits > 0 ) {
		d.vkCmdResetQueryPool( cb, d.qpool, uint32_t( slot * 2 ), 2 );
		d.vkCmdWriteTimestamp( cb, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, d.qpool, uint32_t( slot * 2 ) );
	}
	// GL's writes (the source resolve, last frame's ring) reach the shaders: the semaphore's wait + this barrier
	VkMemoryBarrier mb { VK_STRUCTURE_TYPE_MEMORY_BARRIER };
	mb.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;
	mb.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
	d.vkCmdPipelineBarrier( cb, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &mb, 0, nullptr, 0,
		nullptr );
	if ( combFresh ) {	// its first use: to GENERAL, where it stays
		VkImageMemoryBarrier ib { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
		ib.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
		ib.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		ib.newLayout = VK_IMAGE_LAYOUT_GENERAL;
		ib.srcQueueFamilyIndex = ib.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		ib.image = c.comb.i;
		ib.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
		d.vkCmdPipelineBarrier( cb, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1,
			&ib );
	}
	if ( fr ) {	// round 3 (P3): the source (bloom_src.hlsl: the cell-lit samples exposed, their mean rounded to half)
		Job j {};
		j.params[0] = job.exposure;
		j.params[1] = job.impulseX;
		j.params[2] = job.impulseY;
		j.params[3] = job.impulse;
		j.ints[0] = job.on ? 1 : 0;
		dispatch( cb, 3, c.srcSet, c.W, c.H, j );
	}
	const int n = c.mipCount;
	for ( int i = 0; i < n; i++ ) {
		Job j {};
		j.params[0] = job.thr;
		j.params[1] = job.knee;
		j.params[2] = i == 0 ? 1.0f : 0.0f;
		j.params[3] = 1.0f;
		j.ints[0] = job.red;
		if ( i == 0 && job.hold ) {
			dispatch( cb, 0, c.downRing[job.holdK & ( kRingLayers - 1 )], c.dims[0][0], c.dims[0][1], j );
			Job h {};
			h.ints[1] = job.holdK;
			h.ints[2] = job.holdN;
			dispatch( cb, 1, c.hold, c.dims[0][0], c.dims[0][1], h );
		} else {
			dispatch( cb, 0, c.down[i], c.dims[i][0], c.dims[i][1], j );
		}
	}
	for ( int offset = n - 1; offset > 0; offset-- ) {
		const int level = offset - 1;
		Job j {};
		j.params[0] = job.radius;
		j.params[1] = job.srcW[level];
		j.params[2] = job.accW[level];
		j.params[3] = job.dispersion;
		dispatch( cb, 2, c.upSet[level], c.dims[level][0], c.dims[level][1], j );
	}
	if ( comb ) {	// round 3 (P2): the final tent once per pixel (bloom_comb.hlsl), lod 0 of the top
		Job j {};
		j.params[0] = job.radius;
		j.params[2] = job.dispersion;
		dispatch( cb, 4, c.combSet, c.W, c.H, j );
	}
	// the shaders' writes reach GL's reads after the signal
	mb.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
	mb.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
	d.vkCmdPipelineBarrier( cb, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 1, &mb, 0, nullptr, 0,
		nullptr );
	if ( d.tsBits > 0 )
		d.vkCmdWriteTimestamp( cb, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, d.qpool, uint32_t( slot * 2 + 1 ) );
	if ( d.vkEndCommandBuffer( cb ) != VK_SUCCESS ) {
		why = QStringLiteral( "the command buffer failed to record" );
		win().glFrames++;
		win().glWhy = why;
		sayRoute( QStringLiteral( "gl (%1)" ).arg( why ) );
		return false;
	}

	// GL -> Vulkan: GL signals after its source resolve (flushed, so the signal is queued before the submit waits on it)
	// the images both sides touch this frame: the chain's, and the shared frame while the source reads it
	std::vector<GLuint> & texs = c.texs;
	std::vector<GLenum> & lays = c.layouts;
	const size_t own = texs.size();
	if ( fr ) {
		texs.push_back( fr->color.tex );
		texs.push_back( fr->ds.tex );
		lays.resize( texs.size(), kGlLayoutGeneral );
	}
	const GLuint nt = GLuint( texs.size() );
	g.signalSem( c.glGlDone, 0, nullptr, nt, texs.data(), lays.data() );
	QOpenGLContext::currentContext()->functions()->glFlush();
	const VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;	// the first timestamp too: it times the work alone
	VkSubmitInfo sub { VK_STRUCTURE_TYPE_SUBMIT_INFO };
	sub.waitSemaphoreCount = 1;
	sub.pWaitSemaphores = &c.vkGlDone;
	sub.pWaitDstStageMask = &waitStage;
	sub.commandBufferCount = 1;
	sub.pCommandBuffers = &cb;
	sub.signalSemaphoreCount = 1;
	sub.pSignalSemaphores = &c.vkVkDone;
	d.vkResetFences( d.dev, 1, &d.fence[slot] );
	bool ok;
	{
		std::unique_lock<std::mutex> lk;
		if ( d.qm )
			lk = std::unique_lock<std::mutex>( *d.qm );
		ok = d.vkQueueSubmit( d.q, 1, &sub, d.fence[slot] ) == VK_SUCCESS;
	}
	if ( !ok ) {
		// GL's signal has no waiter now: the chain cannot pair its semaphores again, so it goes back to GL for good
		c.ok = false;
		texs.resize( own );
		lays.resize( own );
		why = QStringLiteral( "vkQueueSubmit failed: the chain stays on GL" );
		win().glFrames++;
		win().glWhy = why;
		sayRoute( QStringLiteral( "gl (%1)" ).arg( why ) );
		return false;
	}
	d.frameOf[slot] = f;
	d.pending[slot] = true;
	// Vulkan -> GL: GL's next reads (the top's mips, the combine, the tone map, the dump) wait on the GPU, not the CPU
	g.waitSem( c.glVkDone, 0, nullptr, nt, texs.data(), lays.data() );
	texs.resize( own );
	lays.resize( own );
	c.combRan = comb;
	win().vkFrames++;
	sayRoute( QStringLiteral( "vulkan (bloom %1down / hold / up%2 on the ray-tracing device, %3 mips%4)" )
		.arg( fr ? QStringLiteral( "source / " ) : QString() ).arg( comb ? QStringLiteral( " / combine" ) : QString() ).arg( n )
		.arg( job.hold ? QStringLiteral( ", hold" ) : QString() ) );
	return true;
}

}	// namespace WwVkPost

QString wwVkFrameUse( int frames, double glGpuMs, double wallMs )
{
	Window & w = win();
	Dev & d = dv();
	const double fr = double( std::max( 1, frames ) );
	const double wall = wallMs / fr;
	// the same CPU and GPU as `frame overlap:` (its window, not reset yet): GL waits on the Vulkan block, so the GL
	// timestamps hold it
	const WwSync::Stats & sy = WwSync::stats();
	const double cpu = sy.cpuMs / double( std::max( 1, sy.cpuFrames ) );
	const double gpu = sy.gpuFrames > 0 ? sy.gpuMs / double( sy.gpuFrames ) : glGpuMs / fr;
	const double vk = w.vkMs / double( std::max( 1, w.vkN ) );
	const double blocked = w.blockedMs / fr;
	const double lo = std::min( cpu, gpu ), hi = std::max( cpu, gpu );
	const double ov = lo > 0.0 ? std::clamp( ( cpu + gpu - wall ) / lo, 0.0, 1.0 ) : 0.0;
	const bool vkRoute = w.vkFrames > 0 && w.vkFrames >= w.glFrames;
	WwVkFrame::Ledger led;
	led.add( "gl-scene-to-bloom-source", WwVkFrame::Side::Gl, std::max( 0.0, gpu - ( vkRoute ? vk : 0.0 ) ) );
	led.add( "bloom-chain", vkRoute ? WwVkFrame::Side::Vk : WwVkFrame::Side::Gl, vkRoute ? vk : 0.0 );
	led.add( "gl-tone-map-to-present", WwVkFrame::Side::Gl, 0.0 );
	const QString why = vkRoute ? QStringLiteral( "vulkan" )
		: !d.ready.load( std::memory_order_acquire ) ? d.why
		: w.glWhy.isEmpty() ? QStringLiteral( "no bloom chain this window (bloom off or vanilla)" ) : w.glWhy;
	std::fprintf( stderr, "%s | frames vulkan %d gl %d | overlap: wall %.2f ms a frame, CPU %.2f (paint start to swap return), GPU %.2f "
		"(timestamps, the Vulkan block inside them), Vulkan %.3f; max %.2f, sum %.2f: %s (%.0f %% of the shorter side runs "
		"beside the other); blocked on Vulkan %.3f (the frames-in-flight fences; the semaphores wait on the GPU only) | "
		"pipelines %d/%d built before first use (cache %s)\n",
		led.line( qPrintable( why ), 0 ).c_str(), w.vkFrames, w.glFrames, wall, cpu, gpu, vk, hi, cpu + gpu,
		wall <= hi + 0.25 * lo ? "they overlap" : "they take turns", 100.0 * ov, blocked, d.pipesBuilt, d.pipesWanted,
		d.cacheWords.isEmpty() ? "not opened" : qPrintable( d.cacheWords ) );
	const QString words = vkRoute
		? QStringLiteral( "; Vulkan frame: bloom chain %1 ms a frame on the ray-tracing device (%2 of %3 frames)" )
			.arg( vk, 0, 'f', 3 ).arg( w.vkFrames ).arg( w.vkFrames + w.glFrames )
		: QStringLiteral( "; Vulkan frame: off (%1)" ).arg( why );
	w = Window();
	return words;
}

#endif
