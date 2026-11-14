// Lane VKFAR3 (2026-11-15, plan row A1): the DDGI probe atlas imported on the present device (vkddgi.inc, the shipped
// vkpresent.cpp with WW_VKPRESENT_NO_QT). A second Vulkan device stands in for bakert's: it makes the atlas buffer the way
// rtddgi_vk.inc ddgiMakeShared does (exportable OPAQUE_WIN32, dedicated, device-local, the shared atlas's usage), writes a
// pattern, exports a handle; WwVkPresent::ddgiAtlasImport takes it and ddgiAtlasRead fetches every texel THROUGH THE VIEW.
// Greens: the read is byte-EXACT; a second pattern written by the exporter reads EXACT with no new import; every import closes
// its handle. Reds (must FAIL): a view one texel late, a view over half the atlas. Refusals (must refuse, handle closed):
// another driver UUID, an exporter buffer without UNIFORM_TEXEL usage. The cascades' route: a farTexelBuffer GL writes is
// found by the same lookup (ddgiAtlasView), reads EXACT, its red 1 FAILS, ddgiAtlasGen moves at its make and its drop.
// Last line DDGIVIEW GATE OK / FAILED.
#define WW_VKPRESENT_NO_QT
#include "../../src/features/vkframe/vkpresent.cpp"

#include <GL/gl.h>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

namespace dv
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
	MSG m;
	while ( PeekMessageA( &m, nullptr, 0, 0, PM_REMOVE ) ) {
		TranslateMessage( &m );
		DispatchMessageA( &m );
	}
}

// rtddgi_vk.inc: kAtlasProbe texels a probe (64 irradiance + 128 distance + 1 probe), 16 bytes each
constexpr std::uint64_t kProbes = 4096, kTexels = kProbes * 193, kBytes = kTexels * 16;
// the shared atlas's usage (rtddgi_vk.inc: STORAGE | TRANSFER_SRC | TRANSFER_DST, + UNIFORM_TEXEL since lane VKFAR3)
constexpr VkBufferUsageFlags kAtlasUsage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT
	| VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT;

// finite normal floats (no NaN, no denormal: a fetch may canonicalise or flush those), signed, 2^-4 .. 2^11
float patternAt( std::uint64_t i, std::uint32_t seed )
{
	std::uint32_t h = std::uint32_t( i ) * 0x9E3779B1u ^ seed;
	h ^= h >> 15;
	h *= 0x85EBCA77u;
	h ^= h >> 13;
	h *= 0xC2B2AE3Du;
	h ^= h >> 16;
	const float m = 1.0f + float( h >> 9 ) / 8388608.0f;
	const float s = std::ldexp( m, int( h & 15 ) - 4 );
	return ( h & 16 ) ? -s : s;
}

// the stand-in for bakert's device: its own VkDevice on the present device's GPU
struct Exporter
{
	VkDevice dev = VK_NULL_HANDLE;
	VkQueue q = VK_NULL_HANDLE;
	VkCommandPool pool = VK_NULL_HANDLE;
	VkFence fence = VK_NULL_HANDLE;
	VkBuffer atlas = VK_NULL_HANDLE, stage = VK_NULL_HANDLE;
	VkDeviceMemory atlasMem = VK_NULL_HANDLE, stageMem = VK_NULL_HANDLE;
	VkDeviceSize alloc = 0;
	std::uint32_t type = 0;
	void * stagePtr = nullptr;
#define DVP( fn ) PFN_##fn fn = nullptr
	DVP( vkGetDeviceQueue ); DVP( vkCreateBuffer ); DVP( vkDestroyBuffer ); DVP( vkGetBufferMemoryRequirements );
	DVP( vkAllocateMemory ); DVP( vkFreeMemory ); DVP( vkBindBufferMemory ); DVP( vkMapMemory ); DVP( vkUnmapMemory );
	DVP( vkGetMemoryWin32HandleKHR ); DVP( vkCreateCommandPool ); DVP( vkDestroyCommandPool ); DVP( vkAllocateCommandBuffers );
	DVP( vkBeginCommandBuffer ); DVP( vkEndCommandBuffer ); DVP( vkCmdCopyBuffer ); DVP( vkQueueSubmit ); DVP( vkCreateFence );
	DVP( vkDestroyFence ); DVP( vkWaitForFences ); DVP( vkResetFences ); DVP( vkDeviceWaitIdle ); DVP( vkDestroyDevice );
#undef DVP
};

std::uint32_t typeOf( std::uint32_t bits, VkMemoryPropertyFlags want )
{
	const Vk & v = vk();
	for ( std::uint32_t i = 0; i < v.mem.memoryTypeCount; i++ )
		if ( ( bits & ( 1u << i ) ) && ( v.mem.memoryTypes[i].propertyFlags & want ) == want )
			return i;
	return UINT32_MAX;
}

bool exporterMake( Exporter & E, VkBufferUsageFlags usage, std::string & why )
{
	Vk & v = vk();
	const float prio = 1.0f;
	VkDeviceQueueCreateInfo qi { VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO };
	qi.queueFamilyIndex = v.family;
	qi.queueCount = 1;
	qi.pQueuePriorities = &prio;
	const char * ext[] = { VK_KHR_EXTERNAL_MEMORY_WIN32_EXTENSION_NAME };
	VkDeviceCreateInfo di { VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
	di.queueCreateInfoCount = 1;
	di.pQueueCreateInfos = &qi;
	di.enabledExtensionCount = 1;
	di.ppEnabledExtensionNames = ext;
	if ( v.vkCreateDevice( v.phys, &di, nullptr, &E.dev ) != VK_SUCCESS ) {
		why = "the exporter's vkCreateDevice failed";
		return false;
	}
#define DLD( fn ) E.fn = reinterpret_cast<PFN_##fn>( v.vkGetDeviceProcAddr( E.dev, #fn ) )
	DLD( vkGetDeviceQueue ); DLD( vkCreateBuffer ); DLD( vkDestroyBuffer ); DLD( vkGetBufferMemoryRequirements );
	DLD( vkAllocateMemory ); DLD( vkFreeMemory ); DLD( vkBindBufferMemory ); DLD( vkMapMemory ); DLD( vkUnmapMemory );
	DLD( vkGetMemoryWin32HandleKHR ); DLD( vkCreateCommandPool ); DLD( vkDestroyCommandPool ); DLD( vkAllocateCommandBuffers );
	DLD( vkBeginCommandBuffer ); DLD( vkEndCommandBuffer ); DLD( vkCmdCopyBuffer ); DLD( vkQueueSubmit ); DLD( vkCreateFence );
	DLD( vkDestroyFence ); DLD( vkWaitForFences ); DLD( vkResetFences ); DLD( vkDeviceWaitIdle ); DLD( vkDestroyDevice );
#undef DLD
	if ( !E.vkGetMemoryWin32HandleKHR || !E.vkCmdCopyBuffer || !E.vkDestroyDevice ) {
		why = "the exporter's entry points did not load";
		return false;
	}
	E.vkGetDeviceQueue( E.dev, v.family, 0, &E.q );
	// ddgiMakeShared's buffer and memory
	VkExternalMemoryBufferCreateInfo eb { VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_BUFFER_CREATE_INFO };
	eb.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT;
	VkBufferCreateInfo bi { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
	bi.pNext = &eb;
	bi.size = kBytes;
	bi.usage = usage;
	bi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	if ( E.vkCreateBuffer( E.dev, &bi, nullptr, &E.atlas ) != VK_SUCCESS ) {
		why = "the exporter's atlas buffer failed";
		return false;
	}
	VkMemoryRequirements mr {};
	E.vkGetBufferMemoryRequirements( E.dev, E.atlas, &mr );
	E.type = typeOf( mr.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );
	VkMemoryDedicatedAllocateInfo dd { VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO };
	dd.buffer = E.atlas;
	VkExportMemoryAllocateInfo ex { VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO };
	ex.pNext = &dd;
	ex.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT;
	VkMemoryAllocateInfo ai { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
	ai.pNext = &ex;
	ai.allocationSize = mr.size;
	ai.memoryTypeIndex = E.type;
	if ( E.type == UINT32_MAX || E.vkAllocateMemory( E.dev, &ai, nullptr, &E.atlasMem ) != VK_SUCCESS
		|| E.vkBindBufferMemory( E.dev, E.atlas, E.atlasMem, 0 ) != VK_SUCCESS ) {
		why = "the exporter's exportable memory failed";
		return false;
	}
	E.alloc = mr.size;
	// the staging buffer the pattern goes through
	VkBufferCreateInfo si { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
	si.size = kBytes;
	si.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
	if ( E.vkCreateBuffer( E.dev, &si, nullptr, &E.stage ) != VK_SUCCESS ) {
		why = "the exporter's staging buffer failed";
		return false;
	}
	E.vkGetBufferMemoryRequirements( E.dev, E.stage, &mr );
	VkMemoryAllocateInfo sa { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
	sa.allocationSize = mr.size;
	sa.memoryTypeIndex = typeOf( mr.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT );
	if ( sa.memoryTypeIndex == UINT32_MAX || E.vkAllocateMemory( E.dev, &sa, nullptr, &E.stageMem ) != VK_SUCCESS
		|| E.vkBindBufferMemory( E.dev, E.stage, E.stageMem, 0 ) != VK_SUCCESS
		|| E.vkMapMemory( E.dev, E.stageMem, 0, VK_WHOLE_SIZE, 0, &E.stagePtr ) != VK_SUCCESS ) {
		why = "the exporter's staging memory failed";
		return false;
	}
	VkCommandPoolCreateInfo cpi { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
	cpi.queueFamilyIndex = v.family;
	VkFenceCreateInfo fi { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
	if ( E.vkCreateCommandPool( E.dev, &cpi, nullptr, &E.pool ) != VK_SUCCESS || E.vkCreateFence( E.dev, &fi, nullptr, &E.fence ) != VK_SUCCESS ) {
		why = "the exporter's command pool / fence failed";
		return false;
	}
	return true;
}

// the pattern of seed into the atlas on the exporter's queue, waited (the gate's order: the import reads after this fence)
bool exporterWrite( Exporter & E, std::uint32_t seed, std::vector<float> & want )
{
	want.resize( size_t( kTexels ) * 4 );
	for ( size_t i = 0; i < want.size(); i++ )
		want[i] = patternAt( i, seed );
	std::memcpy( E.stagePtr, want.data(), size_t( kBytes ) );
	VkCommandBufferAllocateInfo cai { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
	cai.commandPool = E.pool;
	cai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	cai.commandBufferCount = 1;
	VkCommandBuffer cmd = VK_NULL_HANDLE;
	if ( E.vkAllocateCommandBuffers( E.dev, &cai, &cmd ) != VK_SUCCESS )
		return false;
	VkCommandBufferBeginInfo bi { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
	bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	E.vkBeginCommandBuffer( cmd, &bi );
	const VkBufferCopy c { 0, 0, kBytes };
	E.vkCmdCopyBuffer( cmd, E.stage, E.atlas, 1, &c );
	E.vkEndCommandBuffer( cmd );
	VkSubmitInfo si { VK_STRUCTURE_TYPE_SUBMIT_INFO };
	si.commandBufferCount = 1;
	si.pCommandBuffers = &cmd;
	const bool ok = E.vkQueueSubmit( E.q, 1, &si, E.fence ) == VK_SUCCESS
		&& E.vkWaitForFences( E.dev, 1, &E.fence, VK_TRUE, 1000000000ull ) == VK_SUCCESS && E.vkResetFences( E.dev, 1, &E.fence ) == VK_SUCCESS;
	return ok;  // the command buffer goes with the pool
}

HANDLE exporterHandle( Exporter & E )
{
	VkMemoryGetWin32HandleInfoKHR mi { VK_STRUCTURE_TYPE_MEMORY_GET_WIN32_HANDLE_INFO_KHR };
	mi.memory = E.atlasMem;
	mi.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT;
	HANDLE h = nullptr;
	return E.vkGetMemoryWin32HandleKHR( E.dev, &mi, &h ) == VK_SUCCESS ? h : nullptr;
}

void exporterFree( Exporter & E )
{
	if ( !E.dev )
		return;
	E.vkDeviceWaitIdle( E.dev );
	if ( E.stagePtr )
		E.vkUnmapMemory( E.dev, E.stageMem );
	if ( E.stage )
		E.vkDestroyBuffer( E.dev, E.stage, nullptr );
	if ( E.stageMem )
		E.vkFreeMemory( E.dev, E.stageMem, nullptr );
	if ( E.atlas )
		E.vkDestroyBuffer( E.dev, E.atlas, nullptr );
	if ( E.atlasMem )
		E.vkFreeMemory( E.dev, E.atlasMem, nullptr );
	if ( E.fence )
		E.vkDestroyFence( E.dev, E.fence, nullptr );
	if ( E.pool )
		E.vkDestroyCommandPool( E.dev, E.pool, nullptr );
	E.vkDestroyDevice( E.dev, nullptr );
	E = Exporter();
}

WwVkPresent::DdgiAtlasShare shareOf( Exporter & E, HANDLE h, VkBufferUsageFlags usage )
{
	WwVkPresent::DdgiAtlasShare s;
	s.memory = h;
	s.memorySize = E.alloc;
	s.bufferSize = kBytes;
	s.atlasBytes = kBytes;
	s.usage = usage;
	s.memoryType = E.type;
	s.dedicated = true;
	std::memcpy( s.deviceUuid, vk().uuid, 16 );
	std::memcpy( s.driverUuid, vk().driverUuid, 16 );
	return s;
}

bool closed( HANDLE h )
{
	DWORD f = 0;
	return !GetHandleInformation( h, &f );
}

// texels whose 16 bytes differ (the fetch read whole, in one dispatch)
std::uint64_t differ( const std::vector<float> & got, const std::vector<float> & want, std::uint64_t * first )
{
	std::uint64_t n = 0;
	*first = UINT64_MAX;
	for ( size_t t = 0; t < size_t( kTexels ); t++ )
		if ( got.size() < ( t + 1 ) * 4 || std::memcmp( &got[t * 4], &want[t * 4], 16 ) != 0 ) {
			if ( !n )
				*first = t;
			n++;
		}
	return n;
}
}  // namespace dv

int main()
{
	using namespace dv;
	const int W = 320, H = 180, S = 4;
	gOpenGl = LoadLibraryA( "opengl32.dll" );
	WNDCLASSA wc {};
	wc.style = CS_OWNDC;
	wc.lpfnWndProc = wndProc;
	wc.hInstance = GetModuleHandleA( nullptr );
	wc.lpszClassName = "wwddgiview";
	RegisterClassA( &wc );
	HWND dummy = CreateWindowA( "wwddgiview", "dummy", WS_OVERLAPPEDWINDOW, 0, 0, 64, 64, nullptr, nullptr, wc.hInstance, nullptr );
	HDC ddc = GetDC( dummy );
	PIXELFORMATDESCRIPTOR pfd { sizeof( pfd ), 1, PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER, PFD_TYPE_RGBA, 32 };
	SetPixelFormat( ddc, ChoosePixelFormat( ddc, &pfd ), &pfd );
	HGLRC drc = wglCreateContext( ddc );
	wglMakeCurrent( ddc, drc );
	using ChooseFmt = BOOL( WINAPI * )( HDC, const int *, const float *, UINT, int *, UINT * );
	auto choose = reinterpret_cast<ChooseFmt>( wglGetProcAddress( "wglChoosePixelFormatARB" ) );
	RECT rc { 0, 0, W, H };
	AdjustWindowRectEx( &rc, WS_OVERLAPPEDWINDOW, FALSE, WS_EX_NOACTIVATE );
	HWND wnd = CreateWindowExA( WS_EX_NOACTIVATE, "wwddgiview", "ddgi view harness", WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, 40, 40,
		rc.right - rc.left, rc.bottom - rc.top, nullptr, nullptr, wc.hInstance, nullptr );
	HDC dc = GetDC( wnd );
	const int attr[] = { 0x2001, 1, 0x2010, 1, 0x2011, 1, 0x2003, 0x2027, 0x2013, 0x202B, 0x2015, 8, 0x2017, 8, 0x2019, 8,
		0x201B, 8, 0x2022, 24, 0x2023, 8, 0x2041, 1, 0x2042, 4, 0 };
	int fmt = 0;
	UINT nf = 0;
	if ( !choose || !choose( dc, attr, nullptr, 1, &fmt, &nf ) || !nf ) {
		std::printf( "DDGIVIEW GATE FAILED (no 4x MSAA pixel format)\n" );
		return 2;
	}
	PIXELFORMATDESCRIPTOR pf {};
	DescribePixelFormat( dc, fmt, sizeof( pf ), &pf );
	SetPixelFormat( dc, fmt, &pf );
	HGLRC glrc = wglCreateContext( dc );
	wglMakeCurrent( dc, glrc );
	wglDeleteContext( drc );
	ReleaseDC( dummy, ddc );
	DestroyWindow( dummy );
	ShowWindow( wnd, SW_SHOWNOACTIVATE );
	SetWindowPos( wnd, HWND_BOTTOM, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE );
	pump();
	std::printf( "GL on %s\n", reinterpret_cast<const char *>( glGetString( GL_RENDERER ) ) );
	std::string why;
	if ( !WwVkPresent::viewBegin( W, H, S, glProc, why ) ) {
		std::printf( "DDGIVIEW GATE FAILED (the view: %s)\n", why.c_str() );
		return 2;
	}
	bool ok = true;
	auto verdict = [&]( const char * what, bool pass, const std::string & detail ) {
		std::printf( "DDGIVIEW %s %s (%s)\n", what, pass ? "OK" : "FAILED", detail.c_str() );
		ok = ok && pass;
	};
	Exporter E;
	if ( !exporterMake( E, kAtlasUsage, why ) ) {
		std::printf( "DDGIVIEW GATE FAILED (%s)\n", why.c_str() );
		return 2;
	}
	std::printf( "atlas: %llu probes, %llu texels, %llu bytes, allocation %llu, memory type %u\n", (unsigned long long)kProbes,
		(unsigned long long)kTexels, (unsigned long long)kBytes, (unsigned long long)E.alloc, E.type );
	std::vector<float> want, got;
	const int key = 0;
	std::uint64_t first = 0;
	// 1. export, import, read through the view
	if ( !exporterWrite( E, 0x1234u, want ) ) {
		std::printf( "DDGIVIEW GATE FAILED (the exporter's write)\n" );
		return 2;
	}
	HANDLE h = exporterHandle( E );
	double t0 = nowMs();
	const bool imp = h && WwVkPresent::ddgiAtlasImport( &key, shareOf( E, h, kAtlasUsage ), why );
	const double importMs = nowMs() - t0;
	verdict( "import", imp, imp ? "made in " + std::to_string( importMs ) + " ms" : why );
	verdict( "import closes its handle", h && closed( h ), "GetHandleInformation after the import" );
	std::uint64_t texels = 0;
	const std::uint64_t view = WwVkPresent::ddgiAtlasView( &key, &texels );
	verdict( "view", view != 0 && texels == kTexels, std::to_string( texels ) + " texels" );
	if ( imp ) {
		t0 = nowMs();
		const bool rd = WwVkPresent::ddgiAtlasRead( &key, 0, std::uint32_t( kTexels ), got, 0, why );
		const double readMs = nowMs() - t0;
		const std::uint64_t n = rd ? differ( got, want, &first ) : kTexels;
		verdict( "read 1 EXACT", rd && n == 0, rd ? std::to_string( n ) + " of " + std::to_string( kTexels ) + " texels differ, read in "
			+ std::to_string( readMs ) + " ms" : why );
		// 2. the exporter writes again; the same import reads the new bytes (the memory is one, not a copy)
		if ( exporterWrite( E, 0xBEEFu, want ) ) {
			const bool rd2 = WwVkPresent::ddgiAtlasRead( &key, 0, std::uint32_t( kTexels ), got, 0, why );
			const std::uint64_t n2 = rd2 ? differ( got, want, &first ) : kTexels;
			verdict( "read 2 EXACT, no new import", rd2 && n2 == 0, rd2 ? std::to_string( n2 ) + " texels differ" : why );
		} else {
			verdict( "read 2 EXACT, no new import", false, "the exporter's second write" );
		}
		// a window from the middle (first > 0: the push constant's first texel)
		const std::uint32_t mid = std::uint32_t( kTexels / 3 ), cnt = 4099;
		const bool rd3 = WwVkPresent::ddgiAtlasRead( &key, mid, cnt, got, 0, why );
		bool same3 = rd3 && got.size() == size_t( cnt ) * 4 && std::memcmp( got.data(), &want[size_t( mid ) * 4], size_t( cnt ) * 16 ) == 0;
		verdict( "read of a window EXACT", same3, std::to_string( cnt ) + " texels from " + std::to_string( mid ) );
		// reds: must FAIL
		const char * redName[3] = { "", "red 1 (a view one texel late)", "red 2 (a view over half the atlas)" };
		for ( int red = 1; red <= 2; red++ ) {
			const bool rr = WwVkPresent::ddgiAtlasRead( &key, 0, std::uint32_t( kTexels ), got, red, why );
			const std::uint64_t nr = rr ? differ( got, want, &first ) : 0;
			std::printf( "DDGIVIEW %s: %s, %llu texels differ (first %llu)\n", redName[red], rr ? "read" : why.c_str(), (unsigned long long)nr,
				(unsigned long long)( nr ? first : 0 ) );
			verdict( red == 1 ? "red 1 FAILS" : "red 2 FAILS", rr && nr > 0, std::to_string( nr ) + " texels differ" );
		}
	}
	// refusals: another driver, an exporter buffer with no texel use; each closes its handle and keeps nothing
	{
		const int key2 = 0;
		HANDLE h2 = exporterHandle( E );
		WwVkPresent::DdgiAtlasShare s = shareOf( E, h2, kAtlasUsage );
		s.driverUuid[0] ^= 0xFF;
		const bool r2 = h2 && WwVkPresent::ddgiAtlasImport( &key2, s, why );
		verdict( "another driver UUID refused", h2 && !r2 && WwVkPresent::ddgiAtlasView( &key2 ) == 0, why );
		verdict( "refusal closes its handle", h2 && closed( h2 ), "GetHandleInformation" );
		HANDLE h3 = exporterHandle( E );
		const bool r3 = h3 && WwVkPresent::ddgiAtlasImport( &key2, shareOf( E, h3, kAtlasUsage & ~VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT ), why );
		verdict( "no texel usage refused", h3 && !r3 && WwVkPresent::ddgiAtlasView( &key2 ) == 0, why );
		verdict( "refusal closes its handle (usage)", h3 && closed( h3 ), "GetHandleInformation" );
	}
	WwVkPresent::ddgiAtlasDrop( &key );
	verdict( "drop", WwVkPresent::ddgiAtlasView( &key ) == 0, "no view under the key after the drop" );
	// the cascades' route (rtddgicell.cpp combinedShare): GL writes a farTexelBuffer, the same lookup finds its view
	{
		using BindBuf = void( APIENTRY * )( GLenum, GLuint );
		using SubData = void( APIENTRY * )( GLenum, std::intptr_t, std::intptr_t, const void * );
		auto bindBuf = reinterpret_cast<BindBuf>( glProc( "glBindBuffer" ) );
		auto subData = reinterpret_cast<SubData>( glProc( "glBufferSubData" ) );
		const int ckey = 0;
		const std::uintptr_t ck = reinterpret_cast<std::uintptr_t>( &ckey );
		const std::uint64_t g0 = WwVkPresent::ddgiAtlasGen();
		unsigned gb = 0, gt = 0;
		const bool made = bindBuf && subData && WwVkPresent::farTexelBuffer( ck, 0x8814u, size_t( kBytes ), gb, gt, why );
		const std::uint64_t g1 = WwVkPresent::ddgiAtlasGen();
		std::uint64_t ct = 0;
		const bool cview = made && WwVkPresent::ddgiAtlasView( &ckey, &ct ) != 0 && ct == kTexels;
		verdict( "cascades' buffer seen by the lookup", cview && g1 != g0, made ? std::to_string( ct ) + " texels, gen "
			+ std::to_string( g0 ) + " -> " + std::to_string( g1 ) : why );
		if ( made ) {
			exporterWrite( E, 0xCA5Cu, want );   // only for its pattern: GL writes these bytes
			bindBuf( 0x8892u, gb );   // GL_ARRAY_BUFFER
			subData( 0x8892u, 0, std::intptr_t( kBytes ), want.data() );
			bindBuf( 0x8892u, 0 );
			glFinish();
			const bool rc = WwVkPresent::ddgiAtlasRead( &ckey, 0, std::uint32_t( kTexels ), got, 0, why );
			const std::uint64_t nc = rc ? differ( got, want, &first ) : kTexels;
			verdict( "cascades' buffer read EXACT", rc && nc == 0, rc ? std::to_string( nc ) + " of " + std::to_string( kTexels ) + " texels differ" : why );
			const bool rr = WwVkPresent::ddgiAtlasRead( &ckey, 0, std::uint32_t( kTexels ), got, 1, why );
			const std::uint64_t nr = rr ? differ( got, want, &first ) : 0;
			verdict( "cascades' red 1 FAILS", rr && nr > 0, std::to_string( nr ) + " texels differ" );
		} else {
			verdict( "cascades' buffer read EXACT", false, why );
			verdict( "cascades' red 1 FAILS", false, why );
		}
		WwVkPresent::farTexelBufferDrop( ck );
		const std::uint64_t g2 = WwVkPresent::ddgiAtlasGen();
		verdict( "cascades' buffer dropped", WwVkPresent::ddgiAtlasView( &ckey ) == 0 && g2 != g1, "gen " + std::to_string( g2 ) );
	}
	exporterFree( E );
	WwVkPresent::shutdown();
	wglMakeCurrent( nullptr, nullptr );
	wglDeleteContext( glrc );
	DestroyWindow( wnd );
	std::printf( "DDGIVIEW GATE %s\n", ok ? "OK" : "FAILED" );
	return ok ? 0 : 1;
}
