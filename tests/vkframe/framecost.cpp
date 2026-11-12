// Lane VKFRAME1 (2026-11-12, build 79 fix): what GL pays to draw into and read the shared MSAA frame. No NifSkope: a hidden
// WGL context and a headless Vulkan device of its own. Each variant makes the frame (RGBA16F + D24S8, 4 samples, FC_W x FC_H),
// then GL times (GL_TIME_ELAPSED, median of 32 after 4 warm-ups): clear + 12 full-screen depth-tested draws (the fill), a
// resolve blit into a GL RGBA16F texture (the tone blit), and a full-screen pass that texelFetches every sample (the tone).
// Variants: gl (GL's own glTexStorage2DMultisample, the reference), then Vulkan-made images imported into GL as
// WwVkPost::makeFrame does, with and without GL_TEXTURE_TILING_EXT = GL_OPTIMAL_TILING_EXT before glTexStorageMem*, after a
// GL -> Vulkan -> GL semaphore hand-off in GENERAL or the read-only layouts, and (the red) in a memory type that is not
// device-local: the frame outside video memory, build 79's symptom. Each import also makes makeFrame's placement check
// (VK_EXT_memory_budget: the device-local heap grew by at least 3/4 of the image).
// Prints `FRAMECOST <variant> fill <ms> resolve <ms> read <ms> total <ms> (<x> of gl)`; last line `FRAMECOST GATE OK` when
// every variant named in argv (default: "asis", makeFrame's) is within 5 % of gl and passed the placement check, else
// `FRAMECOST GATE FAILED`. Usage: framecost [variant...]   (variants: gl asis tiling general readonly sysmem)
#define VK_NO_PROTOTYPES
#define VK_USE_PLATFORM_WIN32_KHR
#include <windows.h>
#include <vulkan/vulkan.h>
#include <GL/gl.h>
#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

typedef char GLchar;
typedef ptrdiff_t GLsizeiptr;
#define GLF( ret, name, args ) typedef ret( APIENTRY * PFN_##name ) args; static PFN_##name name
GLF( void, glGenFramebuffers, ( GLsizei, GLuint * ) );
GLF( void, glBindFramebuffer, ( GLenum, GLuint ) );
GLF( void, glFramebufferTexture2D, ( GLenum, GLenum, GLenum, GLuint, GLint ) );
GLF( GLenum, glCheckFramebufferStatus, ( GLenum ) );
GLF( void, glBlitFramebuffer, ( GLint, GLint, GLint, GLint, GLint, GLint, GLint, GLint, GLbitfield, GLenum ) );
GLF( GLuint, glCreateShader, ( GLenum ) );
GLF( void, glShaderSource, ( GLuint, GLsizei, const GLchar * const *, const GLint * ) );
GLF( void, glCompileShader, ( GLuint ) );
GLF( GLuint, glCreateProgram, ( void ) );
GLF( void, glAttachShader, ( GLuint, GLuint ) );
GLF( void, glLinkProgram, ( GLuint ) );
GLF( void, glGetProgramiv, ( GLuint, GLenum, GLint * ) );
GLF( void, glGetProgramInfoLog, ( GLuint, GLsizei, GLsizei *, GLchar * ) );
GLF( void, glUseProgram, ( GLuint ) );
GLF( void, glGenVertexArrays, ( GLsizei, GLuint * ) );
GLF( void, glBindVertexArray, ( GLuint ) );
GLF( GLint, glGetUniformLocation, ( GLuint, const GLchar * ) );
GLF( void, glUniform1f, ( GLint, GLfloat ) );
GLF( void, glUniform1i, ( GLint, GLint ) );
GLF( void, glActiveTexture, ( GLenum ) );
GLF( void, glTexStorage2DMultisample, ( GLenum, GLsizei, GLenum, GLsizei, GLsizei, GLboolean ) );
GLF( void, glTexStorage2D, ( GLenum, GLsizei, GLenum, GLsizei, GLsizei ) );
GLF( void, glGenQueries, ( GLsizei, GLuint * ) );
GLF( void, glBeginQuery, ( GLenum, GLuint ) );
GLF( void, glEndQuery, ( GLenum ) );
GLF( void, glGetQueryObjectui64v, ( GLuint, GLenum, uint64_t * ) );
GLF( void, glCreateMemoryObjectsEXT, ( GLsizei, GLuint * ) );
GLF( void, glMemoryObjectParameterivEXT, ( GLuint, GLenum, const GLint * ) );
GLF( void, glImportMemoryWin32HandleEXT, ( GLuint, uint64_t, GLenum, void * ) );
GLF( void, glTexStorageMem2DMultisampleEXT, ( GLenum, GLsizei, GLenum, GLsizei, GLsizei, GLboolean, GLuint, uint64_t ) );
GLF( void, glGetUnsignedBytei_vEXT, ( GLenum, GLuint, GLubyte * ) );
GLF( const GLubyte *, glGetStringi, ( GLenum, GLuint ) );
GLF( void, glGenSemaphoresEXT, ( GLsizei, GLuint * ) );
GLF( void, glImportSemaphoreWin32HandleEXT, ( GLuint, GLenum, void * ) );
GLF( void, glSignalSemaphoreEXT, ( GLuint, GLuint, const GLuint *, GLuint, const GLuint *, const GLenum * ) );
GLF( void, glWaitSemaphoreEXT, ( GLuint, GLuint, const GLuint *, GLuint, const GLuint *, const GLenum * ) );

constexpr GLenum kMs = 0x9100, kRgba16f = 0x881A, kD24S8 = 0x88F0, kFb = 0x8D40, kReadFb = 0x8CA8, kDrawFb = 0x8CA9;
constexpr GLenum kColor0 = 0x8CE0, kDepthStencilAtt = 0x821A, kTimeElapsed = 0x88BF, kQueryResult = 0x8866;
constexpr GLenum kTiling = 0x9580, kOptimalTiling = 0x9584, kDedicated = 0x9581, kOpaqueWin32 = 0x9587;
constexpr GLenum kFragment = 0x8B30, kVertex = 0x8B31, kLinkStatus = 0x8B82, kTex0 = 0x84C0, kFbComplete = 0x8CD5;
static const int W = getenv( "FC_W" ) ? atoi( getenv( "FC_W" ) ) : 1920, H = getenv( "FC_H" ) ? atoi( getenv( "FC_H" ) ) : 1080;
constexpr int S = 4, kDraws = 12, kRuns = 32, kWarm = 4;

#define VKF( name ) static PFN_##name name
VKF( vkCreateInstance ); VKF( vkEnumeratePhysicalDevices ); VKF( vkGetPhysicalDeviceQueueFamilyProperties );
VKF( vkGetPhysicalDeviceProperties ); VKF( vkGetPhysicalDeviceProperties2 ); VKF( vkGetPhysicalDeviceMemoryProperties );
VKF( vkCreateDevice ); VKF( vkGetDeviceQueue ); VKF( vkCreateImage ); VKF( vkGetImageMemoryRequirements ); VKF( vkAllocateMemory );
VKF( vkCreateSemaphore ); VKF( vkGetSemaphoreWin32HandleKHR ); VKF( vkGetPhysicalDeviceMemoryProperties2 );
VKF( vkBindImageMemory ); VKF( vkGetMemoryWin32HandleKHR ); VKF( vkCreateCommandPool ); VKF( vkAllocateCommandBuffers );
VKF( vkBeginCommandBuffer ); VKF( vkEndCommandBuffer ); VKF( vkCmdPipelineBarrier ); VKF( vkQueueSubmit ); VKF( vkQueueWaitIdle );

static VkDevice dev;
static VkPhysicalDevice phys;
static VkQueue queue;
static VkCommandBuffer cmd;
static VkPhysicalDeviceMemoryProperties memProps;

// VK_EXT_memory_budget: this process's use of the device-local heap (MB), the check vkframe.cpp's makeFrame makes
static double localUsageMb()
{
	VkPhysicalDeviceMemoryBudgetPropertiesEXT bp { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_BUDGET_PROPERTIES_EXT };
	VkPhysicalDeviceMemoryProperties2 m2 { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2 };
	m2.pNext = &bp;
	vkGetPhysicalDeviceMemoryProperties2( phys, &m2 );
	double mb = 0;
	for ( uint32_t h = 0; h < m2.memoryProperties.memoryHeapCount; h++ )
		if ( m2.memoryProperties.memoryHeaps[h].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT )
			mb += bp.heapUsage[h] / 1048576.0;
	return mb;
}

static bool initVk( uint8_t uuid[16] )
{
	HMODULE lib = LoadLibraryA( "vulkan-1.dll" );
	if ( !lib )
		return false;
	auto gipa = reinterpret_cast<PFN_vkGetInstanceProcAddr>( GetProcAddress( lib, "vkGetInstanceProcAddr" ) );
	vkCreateInstance = reinterpret_cast<PFN_vkCreateInstance>( gipa( nullptr, "vkCreateInstance" ) );
	VkApplicationInfo app { VK_STRUCTURE_TYPE_APPLICATION_INFO };
	app.apiVersion = VK_API_VERSION_1_2;
	VkInstanceCreateInfo ici { VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };
	ici.pApplicationInfo = &app;
	VkInstance inst;
	if ( vkCreateInstance( &ici, nullptr, &inst ) != VK_SUCCESS )
		return false;
#define LOADI( fn ) fn = reinterpret_cast<PFN_##fn>( gipa( inst, #fn ) )
	LOADI( vkEnumeratePhysicalDevices ); LOADI( vkGetPhysicalDeviceQueueFamilyProperties ); LOADI( vkGetPhysicalDeviceProperties );
	LOADI( vkGetPhysicalDeviceProperties2 ); LOADI( vkGetPhysicalDeviceMemoryProperties ); LOADI( vkCreateDevice ); LOADI( vkGetDeviceQueue );
	LOADI( vkGetPhysicalDeviceMemoryProperties2 );
	uint32_t nd = 0;
	vkEnumeratePhysicalDevices( inst, &nd, nullptr );
	std::vector<VkPhysicalDevice> pds( nd );
	vkEnumeratePhysicalDevices( inst, &nd, pds.data() );
	phys = VK_NULL_HANDLE;
	for ( VkPhysicalDevice p : pds ) {	// the GL device (same UUID), as sameGpu() in vkframe.cpp
		VkPhysicalDeviceIDProperties idp { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES };
		VkPhysicalDeviceProperties2 p2 { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2 };
		p2.pNext = &idp;
		vkGetPhysicalDeviceProperties2( p, &p2 );
		if ( !std::memcmp( idp.deviceUUID, uuid, 16 ) ) {
			phys = p;
			std::printf( "device %s\n", p2.properties.deviceName );
		}
	}
	if ( !phys )
		return false;
	vkGetPhysicalDeviceMemoryProperties( phys, &memProps );
	uint32_t nq = 0;
	vkGetPhysicalDeviceQueueFamilyProperties( phys, &nq, nullptr );
	std::vector<VkQueueFamilyProperties> qf( nq );
	vkGetPhysicalDeviceQueueFamilyProperties( phys, &nq, qf.data() );
	uint32_t family = UINT32_MAX;
	const bool computeOnly = getenv( "FC_COMPUTEQ" ) != nullptr;	// the app's device: the async compute-only family
	for ( uint32_t i = 0; i < nq && family == UINT32_MAX; i++ )
		if ( computeOnly ? ( qf[i].queueFlags & VK_QUEUE_COMPUTE_BIT ) && !( qf[i].queueFlags & VK_QUEUE_GRAPHICS_BIT )
			: ( qf[i].queueFlags & VK_QUEUE_GRAPHICS_BIT ) != 0 )
			family = i;
	std::printf( "queue family %u%s\n", family, computeOnly ? " (compute only)" : "" );
	const float prio = 1.0f;
	VkDeviceQueueCreateInfo qi { VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO };
	qi.queueFamilyIndex = family;
	qi.queueCount = 1;
	qi.pQueuePriorities = &prio;
	// FC_RT: bakert.cpp's device (ray query, acceleration structures, buffer device address, float64)
	const bool rt = getenv( "FC_RT" ) != nullptr;
	const char * exts[] = { "VK_KHR_external_memory_win32", "VK_KHR_external_semaphore_win32", VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
		VK_KHR_RAY_QUERY_EXTENSION_NAME, VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME };
	VkPhysicalDeviceRayQueryFeaturesKHR rq { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR };
	rq.rayQuery = VK_TRUE;
	VkPhysicalDeviceAccelerationStructureFeaturesKHR as { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR };
	as.accelerationStructure = VK_TRUE;
	as.pNext = &rq;
	VkPhysicalDeviceVulkan12Features v12 { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES };
	v12.bufferDeviceAddress = VK_TRUE;
	v12.pNext = &as;
	VkPhysicalDeviceFeatures2 f2 { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2 };
	f2.features.shaderFloat64 = VK_TRUE;
	f2.pNext = &v12;
	VkDeviceCreateInfo di { VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
	di.pNext = rt ? &f2 : nullptr;
	di.queueCreateInfoCount = 1;
	di.pQueueCreateInfos = &qi;
	di.enabledExtensionCount = rt ? 5 : 2;
	di.ppEnabledExtensionNames = exts;
	if ( vkCreateDevice( phys, &di, nullptr, &dev ) != VK_SUCCESS )
		return false;
	auto gdpa = reinterpret_cast<PFN_vkGetDeviceProcAddr>( gipa( inst, "vkGetDeviceProcAddr" ) );
#define LOADD( fn ) fn = reinterpret_cast<PFN_##fn>( gdpa( dev, #fn ) )
	LOADD( vkCreateImage ); LOADD( vkGetImageMemoryRequirements ); LOADD( vkAllocateMemory ); LOADD( vkBindImageMemory );
	LOADD( vkGetMemoryWin32HandleKHR ); LOADD( vkCreateCommandPool ); LOADD( vkAllocateCommandBuffers ); LOADD( vkBeginCommandBuffer );
	LOADD( vkEndCommandBuffer ); LOADD( vkCmdPipelineBarrier ); LOADD( vkQueueSubmit ); LOADD( vkQueueWaitIdle );
	LOADD( vkCreateSemaphore ); LOADD( vkGetSemaphoreWin32HandleKHR );
	vkGetDeviceQueue( dev, family, 0, &queue );
	VkCommandPoolCreateInfo pi { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
	pi.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	pi.queueFamilyIndex = family;
	VkCommandPool pool;
	vkCreateCommandPool( dev, &pi, nullptr, &pool );
	VkCommandBufferAllocateInfo ai { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
	ai.commandPool = pool;
	ai.commandBufferCount = 1;
	return vkAllocateCommandBuffers( dev, &ai, &cmd ) == VK_SUCCESS;
}

// makeImg's image for the frame (vkframe.cpp): OPTIMAL, external OPAQUE_WIN32, dedicated, imgUsage(), GENERAL once
static bool gSysmem = false;	// the red: a memory type without DEVICE_LOCAL
static bool gPlaced = true;	// every import of this variant passed the placement check

static bool vkFrameImage( VkFormat fmt, bool ds, VkImageUsageFlags usage, HANDLE & handle, uint64_t & size )
{
	VkExternalMemoryImageCreateInfo ext { VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO };
	ext.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT;
	VkImageCreateInfo ci { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
	ci.pNext = &ext;
	ci.imageType = VK_IMAGE_TYPE_2D;
	ci.format = fmt;
	ci.extent = { uint32_t( W ), uint32_t( H ), 1 };
	ci.mipLevels = 1;
	ci.arrayLayers = 1;
	ci.samples = VkSampleCountFlagBits( S );
	ci.tiling = VK_IMAGE_TILING_OPTIMAL;
	ci.usage = usage;
	VkImage img;
	if ( vkCreateImage( dev, &ci, nullptr, &img ) != VK_SUCCESS )
		return false;
	VkMemoryRequirements r;
	vkGetImageMemoryRequirements( dev, img, &r );
	VkMemoryDedicatedAllocateInfo ded { VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO };
	ded.image = img;
	VkExportMemoryAllocateInfo exp { VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO };
	exp.pNext = &ded;
	exp.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT;
	VkMemoryAllocateInfo ai { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
	ai.pNext = &exp;
	ai.allocationSize = r.size;
	ai.memoryTypeIndex = UINT32_MAX;
	for ( uint32_t i = 0; i < memProps.memoryTypeCount && ai.memoryTypeIndex == UINT32_MAX; i++ )
		if ( ( r.memoryTypeBits & ( 1u << i ) ) && !( memProps.memoryHeaps[memProps.memoryTypes[i].heapIndex].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT ) == gSysmem )
			ai.memoryTypeIndex = i;
	std::printf( "vk image format %d: memory types 0x%x allowed\n", int( fmt ), r.memoryTypeBits );
	if ( ai.memoryTypeIndex == UINT32_MAX ) {
		std::printf( "vk image format %d: no %s memory type for it\n", int( fmt ), gSysmem ? "host" : "device-local" );
		return false;
	}
	VkDeviceMemory mem;
	const double before = localUsageMb();
	if ( vkAllocateMemory( dev, &ai, nullptr, &mem ) != VK_SUCCESS || vkBindImageMemory( dev, img, mem, 0 ) != VK_SUCCESS )
		return false;
	const double grew = localUsageMb() - before;
	std::printf( "vk budget: the device-local heap grew %.1f MB for a %.1f MB image\n", grew, r.size / 1048576.0 );
	gPlaced = gPlaced && grew >= 0.75 * ( r.size / 1048576.0 );
	VkImageMemoryBarrier b { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
	b.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
	b.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	b.newLayout = VK_IMAGE_LAYOUT_GENERAL;
	b.srcQueueFamilyIndex = b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	b.image = img;
	b.subresourceRange = { ds ? VkImageAspectFlags( VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT )
		: VkImageAspectFlags( VK_IMAGE_ASPECT_COLOR_BIT ), 0, 1, 0, 1 };
	VkCommandBufferBeginInfo bi { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
	vkBeginCommandBuffer( cmd, &bi );
	vkCmdPipelineBarrier( cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &b );
	vkEndCommandBuffer( cmd );
	VkSubmitInfo sub { VK_STRUCTURE_TYPE_SUBMIT_INFO };
	sub.commandBufferCount = 1;
	sub.pCommandBuffers = &cmd;
	vkQueueSubmit( queue, 1, &sub, VK_NULL_HANDLE );
	vkQueueWaitIdle( queue );
	VkMemoryGetWin32HandleInfoKHR gi { VK_STRUCTURE_TYPE_MEMORY_GET_WIN32_HANDLE_INFO_KHR };
	gi.memory = mem;
	gi.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT;
	size = r.size;
	std::printf( "vk image format %d: %llu bytes, memory type %u\n", int( fmt ), (unsigned long long) r.size, ai.memoryTypeIndex );
	return vkGetMemoryWin32HandleKHR( dev, &gi, &handle ) == VK_SUCCESS && handle;
}

static GLuint glImport( GLenum glFmt, VkFormat vkFmt, bool ds, VkImageUsageFlags usage, bool tiling )
{
	HANDLE hnd = nullptr;
	uint64_t size = 0;
	if ( !vkFrameImage( vkFmt, ds, usage, hnd, size ) )
		return 0;
	GLuint mem = 0, tex = 0;
	while ( glGetError() != GL_NO_ERROR ) {}
	auto step = [&]( const char * what ) {
		if ( const GLenum e = glGetError() )
			std::printf( "import 0x%x: %s: GL error 0x%x\n", glFmt, what, e );
	};
	glCreateMemoryObjectsEXT( 1, &mem );
	step( "create" );
	const GLint one = 1;
	glMemoryObjectParameterivEXT( mem, kDedicated, &one );
	step( "dedicated" );
	glImportMemoryWin32HandleEXT( mem, size, kOpaqueWin32, hnd );
	step( "import" );
	CloseHandle( hnd );
	glGenTextures( 1, &tex );
	glBindTexture( kMs, tex );
	if ( tiling )
		glTexParameteri( kMs, kTiling, GLint( kOptimalTiling ) );
	step( "tiling" );
	glTexStorageMem2DMultisampleEXT( kMs, S, glFmt, W, H, GL_TRUE, mem, 0 );
	step( "storage" );
	glBindTexture( kMs, 0 );
	return tex;
}

static GLuint program( const char * vs, const char * fs )
{
	GLuint p = glCreateProgram();
	for ( int k = 0; k < 2; k++ ) {
		GLuint s = glCreateShader( k ? kFragment : kVertex );
		const char * src = k ? fs : vs;
		glShaderSource( s, 1, &src, nullptr );
		glCompileShader( s );
		glAttachShader( p, s );
	}
	glLinkProgram( p );
	GLint ok = 0;
	glGetProgramiv( p, kLinkStatus, &ok );
	if ( !ok ) {
		char log[2048];
		glGetProgramInfoLog( p, sizeof( log ), nullptr, log );
		std::printf( "link: %s\n", log );
		return 0;
	}
	return p;
}

static const char * kVs = "#version 430\nuniform float z;\nvoid main(){vec2 p=vec2((gl_VertexID<<1)&2,gl_VertexID&2);"
	"gl_Position=vec4(p*2.0-1.0,z,1.0);}\n";
static const char * kFill = "#version 430\nuniform float z;\nout vec4 o;\nvoid main(){vec2 f=gl_FragCoord.xy;"
	"o=vec4(fract(f.x*0.013+z),fract(f.y*0.017-z),z*0.5+0.5,1.0)*4.0;}\n";
static const char * kRead = "#version 430\nuniform sampler2DMS f;\nuniform usampler2DMS st;\nout vec4 o;\n"
	"void main(){ivec2 p=ivec2(gl_FragCoord.xy);vec4 a=vec4(0);for(int s=0;s<4;s++)"
	"if((texelFetch(st,p,s).r&3u)==1u)a+=sqrt(max(texelFetch(f,p,s),0.0));o=a*0.25;}\n";

struct Cost
{
	double fill, resolve, read;
};

static double median( std::vector<double> v )
{
	std::sort( v.begin(), v.end() );
	return v[v.size() / 2];
}

// one exported Vulkan semaphore imported into GL
static GLuint sharedSem( VkSemaphore & vs )
{
	VkExportSemaphoreCreateInfo ex { VK_STRUCTURE_TYPE_EXPORT_SEMAPHORE_CREATE_INFO };
	ex.handleTypes = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_WIN32_BIT;
	VkSemaphoreCreateInfo ci { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
	ci.pNext = &ex;
	if ( vkCreateSemaphore( dev, &ci, nullptr, &vs ) != VK_SUCCESS )
		return 0;
	VkSemaphoreGetWin32HandleInfoKHR gi { VK_STRUCTURE_TYPE_SEMAPHORE_GET_WIN32_HANDLE_INFO_KHR };
	gi.semaphore = vs;
	gi.handleType = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_WIN32_BIT;
	HANDLE hnd = nullptr;
	if ( vkGetSemaphoreWin32HandleKHR( dev, &gi, &hnd ) != VK_SUCCESS )
		return 0;
	GLuint gs = 0;
	glGenSemaphoresEXT( 1, &gs );
	glImportSemaphoreWin32HandleEXT( gs, kOpaqueWin32, hnd );
	CloseHandle( hnd );
	return gs;
}

// runBloom's hand-off (vkframe.cpp): GL signals with the frame's names and layouts, Vulkan waits and signals, GL waits
static bool handOff( GLuint color, GLuint ds, GLenum colorLayout, GLenum dsLayout )
{
	static VkSemaphore vGlDone, vVkDone;
	static GLuint gGlDone = sharedSem( vGlDone ), gVkDone = sharedSem( vVkDone );
	if ( !gGlDone || !gVkDone )
		return false;
	const GLuint texs[2] = { color, ds };
	const GLenum lays[2] = { colorLayout, dsLayout };
	glSignalSemaphoreEXT( gGlDone, 0, nullptr, 2, texs, lays );
	glFlush();
	const VkPipelineStageFlags st = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
	VkSubmitInfo sub { VK_STRUCTURE_TYPE_SUBMIT_INFO };
	sub.waitSemaphoreCount = 1;
	sub.pWaitSemaphores = &vGlDone;
	sub.pWaitDstStageMask = &st;
	sub.signalSemaphoreCount = 1;
	sub.pSignalSemaphores = &vVkDone;
	if ( vkQueueSubmit( queue, 1, &sub, VK_NULL_HANDLE ) != VK_SUCCESS )
		return false;
	glWaitSemaphoreEXT( gVkDone, 0, nullptr, 2, texs, lays );
	vkQueueWaitIdle( queue );
	return true;
}

static bool measure( GLuint color, GLuint ds, GLuint fill, GLuint read, Cost & c )
{
	GLuint fb, rfb, rtex, otex, ofb;
	glGenFramebuffers( 1, &fb );
	glBindFramebuffer( kFb, fb );
	glFramebufferTexture2D( kFb, kColor0, kMs, color, 0 );
	glFramebufferTexture2D( kFb, kDepthStencilAtt, kMs, ds, 0 );
	const GLenum st = glCheckFramebufferStatus( kFb );
	if ( st != kFbComplete ) {
		std::printf( "frame FBO status 0x%x, GL error 0x%x\n", st, glGetError() );
		return false;
	}
	auto single = [&]( GLuint & t, GLuint & f ) {
		glGenTextures( 1, &t );
		glBindTexture( GL_TEXTURE_2D, t );
		glTexStorage2D( GL_TEXTURE_2D, 1, kRgba16f, W, H );
		glGenFramebuffers( 1, &f );
		glBindFramebuffer( kFb, f );
		glFramebufferTexture2D( kFb, kColor0, GL_TEXTURE_2D, t, 0 );
		return glCheckFramebufferStatus( kFb ) == kFbComplete;
	};
	if ( !single( rtex, rfb ) || !single( otex, ofb ) )
		return false;
	GLuint q[3];
	glGenQueries( 3, q );
	const GLint zf = glGetUniformLocation( fill, "z" );
	std::vector<double> t[3];
	for ( int run = 0; run < kWarm + kRuns; run++ ) {
		glBeginQuery( kTimeElapsed, q[0] );
		glBindFramebuffer( kFb, fb );
		glViewport( 0, 0, W, H );
		glClearColor( 0, 0, 0, 0 );
		glClearDepth( 1.0 );
		glClearStencil( 0 );
		glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT );
		glEnable( GL_DEPTH_TEST );
		glDepthFunc( GL_LEQUAL );
		glEnable( GL_STENCIL_TEST );	// the cell's "lit" mark, as the opaque pass writes it
		glStencilFunc( GL_ALWAYS, 1, 0xFF );
		glStencilOp( GL_KEEP, GL_KEEP, GL_REPLACE );
		glUseProgram( fill );
		for ( int i = 0; i < kDraws; i++ ) {
			glUniform1f( zf, 0.9f - 0.07f * float( i ) );
			glDrawArrays( GL_TRIANGLES, 0, 3 );
		}
		glDisable( GL_DEPTH_TEST );
		glDisable( GL_STENCIL_TEST );
		glEndQuery( kTimeElapsed );
		glBeginQuery( kTimeElapsed, q[1] );
		glBindFramebuffer( kReadFb, fb );
		glBindFramebuffer( kDrawFb, rfb );
		glBlitFramebuffer( 0, 0, W, H, 0, 0, W, H, GL_COLOR_BUFFER_BIT, GL_NEAREST );
		glEndQuery( kTimeElapsed );
		glBeginQuery( kTimeElapsed, q[2] );
		glBindFramebuffer( kFb, ofb );
		glUseProgram( read );
		glActiveTexture( kTex0 );
		glBindTexture( kMs, color );
		glActiveTexture( kTex0 + 1 );
		glBindTexture( kMs, ds );
		glDrawArrays( GL_TRIANGLES, 0, 3 );
		glBindTexture( kMs, 0 );
		glActiveTexture( kTex0 );
		glBindTexture( kMs, 0 );
		glEndQuery( kTimeElapsed );
		if ( getenv( "FC_SWAP" ) )
			SwapBuffers( wglGetCurrentDC() );
		for ( int k = 0; k < 3; k++ ) {
			uint64_t ns = 0;
			glGetQueryObjectui64v( q[k], kQueryResult, &ns );
			if ( run >= kWarm )
				t[k].push_back( double( ns ) * 1e-6 );
		}
	}
	c = { median( t[0] ), median( t[1] ), median( t[2] ) };
	const GLenum err = glGetError();
	if ( err != GL_NO_ERROR )
		std::printf( "GL error 0x%x\n", err );
	return err == GL_NO_ERROR;
}

int main( int argc, char ** argv )
{
	WNDCLASSA wc {};
	wc.lpfnWndProc = DefWindowProcA;
	wc.hInstance = GetModuleHandleA( nullptr );
	wc.lpszClassName = "framecost";
	RegisterClassA( &wc );
	HWND wnd = CreateWindowA( "framecost", "framecost", WS_OVERLAPPEDWINDOW, 0, 0, 64, 64, nullptr, nullptr, wc.hInstance, nullptr );
	HDC dc = GetDC( wnd );
	PIXELFORMATDESCRIPTOR pfd { sizeof( pfd ), 1, PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER, PFD_TYPE_RGBA, 32 };
	SetPixelFormat( dc, ChoosePixelFormat( dc, &pfd ), &pfd );
	HGLRC rc = wglCreateContext( dc );
	if ( !rc || !wglMakeCurrent( dc, rc ) ) {
		std::printf( "FRAMECOST GATE FAILED (no GL context)\n" );
		return 2;
	}
#define LOADG( fn ) fn = reinterpret_cast<PFN_##fn>( wglGetProcAddress( #fn ) )
	LOADG( glGenFramebuffers ); LOADG( glBindFramebuffer ); LOADG( glFramebufferTexture2D ); LOADG( glCheckFramebufferStatus );
	LOADG( glBlitFramebuffer ); LOADG( glCreateShader ); LOADG( glShaderSource ); LOADG( glCompileShader ); LOADG( glCreateProgram );
	LOADG( glAttachShader ); LOADG( glLinkProgram ); LOADG( glGetProgramiv ); LOADG( glGetProgramInfoLog ); LOADG( glUseProgram );
	LOADG( glGenVertexArrays ); LOADG( glBindVertexArray ); LOADG( glGetUniformLocation ); LOADG( glUniform1f ); LOADG( glUniform1i );
	LOADG( glActiveTexture ); LOADG( glTexStorage2DMultisample ); LOADG( glTexStorage2D ); LOADG( glGenQueries ); LOADG( glBeginQuery );
	LOADG( glEndQuery ); LOADG( glGetQueryObjectui64v ); LOADG( glCreateMemoryObjectsEXT ); LOADG( glMemoryObjectParameterivEXT );
	LOADG( glGenSemaphoresEXT ); LOADG( glImportSemaphoreWin32HandleEXT ); LOADG( glSignalSemaphoreEXT ); LOADG( glWaitSemaphoreEXT );
	LOADG( glImportMemoryWin32HandleEXT ); LOADG( glTexStorageMem2DMultisampleEXT ); LOADG( glGetUnsignedBytei_vEXT );
	if ( !glTexStorageMem2DMultisampleEXT || !glGetUnsignedBytei_vEXT ) {
		std::printf( "FRAMECOST GATE FAILED (no GL_EXT_memory_object_win32)\n" );
		return 2;
	}
	std::printf( "gl %s\n", reinterpret_cast<const char *>( glGetString( GL_RENDERER ) ) );
	uint8_t uuid[16] {};
	glGetUnsignedBytei_vEXT( 0x9597, 0, uuid );	// GL_DEVICE_UUID_EXT, the first device
	if ( !initVk( uuid ) ) {
		std::printf( "FRAMECOST GATE FAILED (no Vulkan device for the GL one)\n" );
		return 2;
	}
	GLuint vao;
	glGenVertexArrays( 1, &vao );
	glBindVertexArray( vao );
	const GLuint fill = program( kVs, kFill ), read = program( kVs, kRead );
	if ( !fill || !read ) {
		std::printf( "FRAMECOST GATE FAILED (programs)\n" );
		return 2;
	}
	glUseProgram( read );
	glUniform1i( glGetUniformLocation( read, "f" ), 0 );
	glUniform1i( glGetUniformLocation( read, "st" ), 1 );
	const VkImageUsageFlags base = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	std::vector<std::string> gate;
	for ( int i = 1; i < argc; i++ )
		gate.push_back( argv[i] );
	if ( gate.empty() )
		gate.push_back( "asis" );
	// gl: GL's own; asis: makeFrame's import, never handed off; tiling: + GL_TEXTURE_TILING_EXT optimal; general: one
	// hand-off with GL_LAYOUT_GENERAL_EXT (build 79's runBloom); readonly: one hand-off in the read-only layouts (the fix)
	const char * names[] = { "gl", "asis", "tiling", "general", "readonly", "sysmem" };
	constexpr GLenum kLayGeneral = 0x958D, kLayDsReadOnly = 0x9590, kLayShaderReadOnly = 0x9591;
	double ref = 0;
	bool ok = true;
	for ( int v = 0; v < 6; v++ ) {
		gSysmem = v == 5;
		gPlaced = true;
		GLuint color = 0, ds = 0;
		if ( v == 0 ) {
			glGenTextures( 1, &color );
			glBindTexture( kMs, color );
			glTexStorage2DMultisample( kMs, S, kRgba16f, W, H, GL_TRUE );
			glGenTextures( 1, &ds );
			glBindTexture( kMs, ds );
			glTexStorage2DMultisample( kMs, S, kD24S8, W, H, GL_TRUE );
			glBindTexture( kMs, 0 );
		} else {
			color = glImport( kRgba16f, VK_FORMAT_R16G16B16A16_SFLOAT, false, base | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, v == 2 );
			ds = glImport( kD24S8, VK_FORMAT_D24_UNORM_S8_UINT, true, base | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, v == 2 );
		}
		glBindTexture( kMs, ds );	// read as its stencil (cellhdr.cpp's state)
		glTexParameteri( kMs, 0x90EA, GL_STENCIL_INDEX );
		glBindTexture( kMs, 0 );
		if ( v >= 3 && !handOff( color, ds, v == 3 ? kLayGeneral : kLayShaderReadOnly, v == 3 ? kLayGeneral : kLayDsReadOnly ) ) {
			std::printf( "FRAMECOST %s: the hand-off failed\n", names[v] );
			ok = false;
			continue;
		}
		Cost c {};
		if ( !color || !ds || !measure( color, ds, fill, read, c ) ) {
			std::printf( "FRAMECOST %s: could not make or draw the frame\n", names[v] );
			ok = false;
			continue;
		}
		const double total = c.fill + c.resolve + c.read;
		if ( v == 0 )
			ref = total;
		std::printf( "FRAMECOST %s fill %.4f resolve %.4f read %.4f total %.4f (%.3f of gl)\n", names[v], c.fill, c.resolve, c.read, total,
			total / ref );
		const bool gated = std::find( gate.begin(), gate.end(), names[v] ) != gate.end();
		if ( gated && total > ref * 1.05 ) {
			std::printf( "FRAMECOST %s is past 5 %% of gl\n", names[v] );
			ok = false;
		}
		if ( gated && v > 0 && !gPlaced ) {
			std::printf( "FRAMECOST %s failed the placement check (not in the device-local heap)\n", names[v] );
			ok = false;
		}
	}
	std::printf( ok ? "FRAMECOST GATE OK\n" : "FRAMECOST GATE FAILED\n" );
	return ok ? 0 : 1;
}
