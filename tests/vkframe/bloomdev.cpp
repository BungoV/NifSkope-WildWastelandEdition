// Lane VKOBJ4 item 2 (2026-11-13): the bloom chain (vkframe.cpp, the app's own TU) on the present device (vkpresent.cpp:
// shareDevice registers it first, every submit a job on the present worker) against the same chain on a device of its own
// (made here, registered first: the route before item 2). Same GL source, same job.
// Usage: bloomdev <out dir> separate|present
//   separate: this test's device runs the chain; the frame made on it is no shared target of the present device
//             (objBlFrameRead must refuse: "not a shared target").
//   present:  the route's own ask (wanted -> kickDevice -> shareDevice); the frame is lent to the present device
//             (objBlFrameRead must find it: colour key + 1, depth key + 2), red 4 (no borrowed target) must refuse,
//             and job.red = 1 (level 0 a plain box) must move the output.
// Writes <out>/<mode>.bin (green) and, present, <out>/present_red.bin; bloomdev.sh compares separate.bin with present.bin
// (BAR, stated before the first run: EXACT, every byte of every level and of the combine).
// Prints `BLOOMDEV <check> OK|FAIL ...` and `BLOOMDEV <mode> OK|FAILED`.
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include "features/vkframe/vkframe.h"
#include "features/vkframe/vkobjpipe.h"

#include <QCoreApplication>
#include <QGuiApplication>
#include <QOffscreenSurface>
#include <QOpenGLContext>
#include <QOpenGLExtraFunctions>
#include <QSurfaceFormat>

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

// the app's symbols vkframe.cpp links against (the test's own stand-ins)
QString wwHarnessSettingsSuffix()
{
	return QStringLiteral( "_vkbloomdev" );
}
#include "version.h"
QString NifSkopeVersion::rawToMajMin( const QString & ) { return QStringLiteral( "2.0" ); }
int wwEnvInt( const char * name, bool * ok )
{
	const char * v = std::getenv( name );
	if ( ok )
		*ok = v != nullptr;
	return v ? std::atoi( v ) : 0;
}
namespace PassTime
{
void subAddMs( int, double ) {}
}

namespace
{
int fails = 0;

void check( bool ok, const char * name, const std::string & said )
{
	std::printf( "BLOOMDEV %s %s%s%s\n", name, ok ? "OK" : "FAIL", said.empty() ? "" : ": ", said.c_str() );
	if ( !ok )
		fails++;
}

void * glProc( const char * name )
{
	QOpenGLContext * c = QOpenGLContext::currentContext();
	return c ? reinterpret_cast<void *>( c->getProcAddress( name ) ) : nullptr;
}

// the route before item 2: a Vulkan device of the test's own on GL's GPU, external memory + semaphores, registered first
bool ownDevice( std::string & why )
{
	HMODULE lib = LoadLibraryA( "vulkan-1.dll" );
	auto gipa = lib ? reinterpret_cast<PFN_vkGetInstanceProcAddr>( reinterpret_cast<void *>( GetProcAddress( lib, "vkGetInstanceProcAddr" ) ) ) : nullptr;
	if ( !gipa )
		return why = "no vulkan-1.dll", false;
	auto createInstance = reinterpret_cast<PFN_vkCreateInstance>( gipa( nullptr, "vkCreateInstance" ) );
	VkApplicationInfo app { VK_STRUCTURE_TYPE_APPLICATION_INFO };
	app.pApplicationName = "bloomdev separate";
	app.apiVersion = VK_API_VERSION_1_2;
	VkInstanceCreateInfo ici { VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };
	ici.pApplicationInfo = &app;
	VkInstance inst = VK_NULL_HANDLE;
	if ( createInstance( &ici, nullptr, &inst ) != VK_SUCCESS )
		return why = "vkCreateInstance failed", false;
#define BD_I( f ) auto f = reinterpret_cast<PFN_##f>( gipa( inst, #f ) )
	BD_I( vkEnumeratePhysicalDevices ); BD_I( vkGetPhysicalDeviceProperties2 ); BD_I( vkGetPhysicalDeviceQueueFamilyProperties );
	BD_I( vkEnumerateDeviceExtensionProperties ); BD_I( vkGetPhysicalDeviceFeatures ); BD_I( vkCreateDevice ); BD_I( vkGetDeviceProcAddr );
#undef BD_I
	using GetUb = void( APIENTRY * )( GLenum, GLuint, GLubyte * );
	auto getUb = reinterpret_cast<GetUb>( glProc( "glGetUnsignedBytei_vEXT" ) );
	unsigned char glId[VK_UUID_SIZE] = {};
	if ( !getUb )
		return why = "no glGetUnsignedBytei_vEXT", false;
	getUb( 0x9597, 0, glId );  // GL_DEVICE_UUID_EXT
	std::uint32_t n = 0;
	vkEnumeratePhysicalDevices( inst, &n, nullptr );
	std::vector<VkPhysicalDevice> pds( n );
	vkEnumeratePhysicalDevices( inst, &n, pds.data() );
	VkPhysicalDevice pd = VK_NULL_HANDLE;
	for ( VkPhysicalDevice p : pds ) {
		VkPhysicalDeviceIDProperties id { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES };
		VkPhysicalDeviceProperties2 pp { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2 };
		pp.pNext = &id;
		vkGetPhysicalDeviceProperties2( p, &pp );
		if ( std::memcmp( id.deviceUUID, glId, VK_UUID_SIZE ) == 0 ) {
			pd = p;
			break;
		}
	}
	if ( !pd )
		return why = "no Vulkan device on GL's GPU", false;
	std::uint32_t nq = 0;
	vkGetPhysicalDeviceQueueFamilyProperties( pd, &nq, nullptr );
	std::vector<VkQueueFamilyProperties> qf( nq );
	vkGetPhysicalDeviceQueueFamilyProperties( pd, &nq, qf.data() );
	std::uint32_t fam = UINT32_MAX;
	for ( std::uint32_t i = 0; i < nq && fam == UINT32_MAX; i++ )
		if ( qf[i].queueFlags & VK_QUEUE_COMPUTE_BIT )
			fam = i;
	if ( fam == UINT32_MAX )
		return why = "no compute queue family", false;
	std::vector<const char *> ext = { "VK_KHR_external_memory_win32", "VK_KHR_external_semaphore_win32" };
	std::uint32_t ne = 0;
	vkEnumerateDeviceExtensionProperties( pd, nullptr, &ne, nullptr );
	std::vector<VkExtensionProperties> ep( ne );
	vkEnumerateDeviceExtensionProperties( pd, nullptr, &ne, ep.data() );
	for ( const VkExtensionProperties & e : ep )
		if ( std::strcmp( e.extensionName, VK_EXT_MEMORY_BUDGET_EXTENSION_NAME ) == 0 )
			ext.push_back( VK_EXT_MEMORY_BUDGET_EXTENSION_NAME );
	VkPhysicalDeviceFeatures offered {}, en {};
	vkGetPhysicalDeviceFeatures( pd, &offered );
	en.shaderStorageImageExtendedFormats = offered.shaderStorageImageExtendedFormats;
	const float prio = 1.0f;
	VkDeviceQueueCreateInfo qi { VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO };
	qi.queueFamilyIndex = fam;
	qi.queueCount = 1;
	qi.pQueuePriorities = &prio;
	VkDeviceCreateInfo di { VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
	di.queueCreateInfoCount = 1;
	di.pQueueCreateInfos = &qi;
	di.enabledExtensionCount = std::uint32_t( ext.size() );
	di.ppEnabledExtensionNames = ext.data();
	di.pEnabledFeatures = &en;
	VkDevice dev = VK_NULL_HANDLE;
	if ( vkCreateDevice( pd, &di, nullptr, &dev ) != VK_SUCCESS )
		return why = "vkCreateDevice failed", false;
	auto getQueue = reinterpret_cast<PFN_vkGetDeviceQueue>( vkGetDeviceProcAddr( dev, "vkGetDeviceQueue" ) );
	VkQueue q = VK_NULL_HANDLE;
	getQueue( dev, fam, 0, &q );
	wwVkFrameSetDevice( inst, pd, dev, fam, q, reinterpret_cast<void *>( vkGetDeviceProcAddr ), glId );
	return true;
}

using GetTexImage = void( APIENTRY * )( GLenum, GLint, GLenum, GLenum, void * );

// every level the chain writes, and the combine, as RGBA32F bytes
std::vector<char> readChain( const std::vector<std::pair<GLuint, std::pair<int, int>>> & texs )
{
	auto getTex = reinterpret_cast<GetTexImage>( glProc( "glGetTexImage" ) );
	QOpenGLFunctions * f = QOpenGLContext::currentContext()->functions();
	f->glFinish();
	std::vector<char> out;
	for ( const auto & t : texs ) {
		std::vector<float> px( std::size_t( t.second.first ) * t.second.second * 4 );
		f->glBindTexture( GL_TEXTURE_2D, t.first );
		getTex( GL_TEXTURE_2D, 0, GL_RGBA, GL_FLOAT, px.data() );
		const char * b = reinterpret_cast<const char *>( px.data() );
		out.insert( out.end(), b, b + px.size() * sizeof( float ) );
	}
	f->glBindTexture( GL_TEXTURE_2D, 0 );
	return out;
}

bool write( const std::string & path, const std::vector<char> & b )
{
	std::ofstream o( path, std::ios::binary );
	o.write( b.data(), std::streamsize( b.size() ) );
	return bool( o );
}
}  // namespace

int main( int argc, char ** argv )
{
	std::setvbuf( stdout, nullptr, _IONBF, 0 );
	if ( argc < 3 ) {
		std::printf( "usage: bloomdev <out dir> separate|present\n" );
		return 2;
	}
	const std::string outDir = argv[1], mode = argv[2];
	const bool present = mode == "present";
	// the pipeline cache path (QStandardPaths' cache folder) cannot be made with this name: nothing is written there
	QCoreApplication::setOrganizationName( QStringLiteral( "ww|bloomdev" ) );
	QCoreApplication::setApplicationName( QStringLiteral( "ww|bloomdev" ) );
	QGuiApplication app( argc, argv );
	QSurfaceFormat fmt;
	fmt.setVersion( 4, 5 );
	fmt.setProfile( QSurfaceFormat::CompatibilityProfile );
	QOffscreenSurface surf;
	surf.setFormat( fmt );
	surf.create();
	QOpenGLContext ctx;
	ctx.setFormat( fmt );
	if ( !ctx.create() || !ctx.makeCurrent( &surf ) ) {
		std::printf( "BLOOMDEV %s FAILED (no GL 4.5 context)\n", mode.c_str() );
		return 2;
	}
	QOpenGLExtraFunctions * f = ctx.extraFunctions();
	std::printf( "GL on %s, mode %s\n", reinterpret_cast<const char *>( f->glGetString( GL_RENDERER ) ), mode.c_str() );

	std::string why;
	if ( !present ) {
		check( ownDevice( why ), "own-device", why );
		// the present device comes up too (its worker takes the submits), registered nowhere: the first registration won
		WwVkPresent::Device p;
		check( WwVkPresent::shareDevice( glProc, p, why ), "present-device-up", why );
	}
	check( WwVkPost::wanted() && WwVkPost::ready(), "ready", present ? "the route's own ask" : "the own device" );

	// the chain: 256 x 160, five mips (cellbloom.cpp's wwBloomPlanMips)
	static int keyObj = 0;
	const void * key = &keyObj;
	const int W = 256, H = 160, mips = 5;
	int dims[WwVkPost::kMaxMips][2] = {};
	for ( int i = 0, w = W, h = H; i < mips; i++ ) {
		w = std::max( ( w + 1 ) / 2, 1 );
		h = std::max( ( h + 1 ) / 2, 1 );
		dims[i][0] = w;
		dims[i][1] = h;
	}
	unsigned src = 0, ring = 0, mip[WwVkPost::kMaxMips] = {}, up[WwVkPost::kMaxMips] = {};
	QString qwhy;
	const bool chain = WwVkPost::makeChain( key, W, H, mips, dims, src, mip, up, ring, qwhy );
	check( chain, "chain", qwhy.toStdString() );
	if ( !chain ) {
		std::printf( "BLOOMDEV %s FAILED\n", mode.c_str() );
		return 1;
	}
	// the source: deterministic HDR values (a few bright spots over a ramp)
	{
		std::vector<float> px( std::size_t( W ) * H * 4 );
		for ( int y = 0; y < H; y++ )
			for ( int x = 0; x < W; x++ ) {
				float * p = &px[( std::size_t( y ) * W + x ) * 4];
				const bool spot = ( x % 37 == 5 && y % 23 == 7 ) || ( x / 8 == 20 && y / 8 == 9 );
				p[0] = spot ? 24.0f + float( x % 7 ) : float( x ) / W * 1.5f;
				p[1] = spot ? 12.0f + float( y % 5 ) : float( y ) / H * 1.2f;
				p[2] = spot ? 6.0f : float( ( x * 31 + y * 17 ) % 97 ) / 97.0f;
				p[3] = 1.0f;
			}
		f->glBindTexture( GL_TEXTURE_2D, src );
		f->glTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, W, H, GL_RGBA, GL_FLOAT, px.data() );
		f->glBindTexture( GL_TEXTURE_2D, 0 );
	}
	WwVkPost::BloomJob job;
	job.thr = 1.0f;
	job.knee = 0.5f;
	job.radius = 1.0f;
	job.dispersion = 0.5f;
	job.comb = true;
	for ( int i = 0; i + 1 < mips; i++ ) {
		job.srcW[i] = 0.25f + 0.1f * float( i );
		job.accW[i] = 0.9f - 0.1f * float( i );
	}
	auto run = [&]( const WwVkPost::BloomJob & j, const char * name, std::vector<char> & out ) {
		QString w;
		const bool ok = WwVkPost::runBloom( key, j, w );
		check( ok, name, w.toStdString() );
		const unsigned comb = WwVkPost::combTexture( key );
		check( comb != 0, "combine-ran", std::string() );
		std::vector<std::pair<GLuint, std::pair<int, int>>> texs;
		for ( int i = 0; i < mips; i++ ) {
			texs.push_back( { mip[i], { dims[i][0], dims[i][1] } } );
			if ( up[i] )
				texs.push_back( { up[i], { dims[i][0], dims[i][1] } } );
		}
		if ( comb )
			texs.push_back( { comb, { W, H } } );
		out = readChain( texs );
	};
	std::vector<char> green;
	run( job, "bloom-green", green );
	check( write( outDir + "/" + mode + ".bin", green ), "write", outDir + "/" + mode + ".bin" );
	std::printf( "BLOOMDEV bytes %zu\n", green.size() );

	// the frame: 4 samples, D24S8 (cellhdr.cpp's), and GL's draw framebuffer over it
	unsigned color = 0, ds = 0;
	const bool frame = WwVkPost::makeFrame( key, W, H, 4, 0x88F0, color, ds, qwhy );  // GL_DEPTH24_STENCIL8
	check( frame, "frame", qwhy.toStdString() );
	if ( frame ) {
		GLuint fbo = 0;
		f->glGenFramebuffers( 1, &fbo );
		f->glBindFramebuffer( GL_DRAW_FRAMEBUFFER, fbo );
		f->glFramebufferTexture2D( GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D_MULTISAMPLE, color, 0 );
		f->glFramebufferTexture2D( GL_DRAW_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D_MULTISAMPLE, ds, 0 );
		const GLenum db = GL_COLOR_ATTACHMENT0;
		f->glDrawBuffers( 1, &db );
		f->glViewport( 0, 0, W, H );
		f->glEnable( GL_DEPTH_TEST );  // the object passes' state (objBlTargetState refuses a depth test that is off)
		const GLenum st = f->glCheckFramebufferStatus( GL_DRAW_FRAMEBUFFER );
		check( st == GL_FRAMEBUFFER_COMPLETE, "fbo", std::to_string( st ) );
		const std::uintptr_t k = reinterpret_cast<std::uintptr_t>( key );
		WwVkPresent::ObjBlDraw d;
		std::string w;
		const bool read = WwVkPresent::objBlFrameRead( glProc, d, w, 0 );
		if ( present ) {
			check( read && d.colorKeys[0] == k + 1 && d.depthKey == k + 2, "frame-read",
				read ? "colour key +" + std::to_string( d.colorKeys[0] - k ) + ", depth key +" + std::to_string( d.depthKey - k ) : w );
			WwVkPresent::ObjBlDraw d4;
			std::string w4;
			const bool read4 = WwVkPresent::objBlFrameRead( glProc, d4, w4, 4 );
			check( !read4 && w4.find( "not a shared target" ) != std::string::npos, "red-noborrow-refuses", w4 );
		} else {
			check( !read && w.find( "not a shared target" ) != std::string::npos, "red-other-device-refuses", w );
		}
		f->glBindFramebuffer( GL_DRAW_FRAMEBUFFER, 0 );
		f->glDeleteFramebuffers( 1, &fbo );
	}

	if ( present ) {
		WwVkPost::BloomJob rj = job;
		rj.red = 1;
		std::vector<char> red;
		run( rj, "bloom-red", red );
		write( outDir + "/present_red.bin", red );
		std::size_t diff = 0;
		for ( std::size_t i = 0; i < red.size() && i < green.size(); i++ )
			diff += red[i] != green[i];
		check( red.size() == green.size() && diff > 0, "red-box-moves", std::to_string( diff ) + " bytes differ" );
	}
	WwVkPost::freeFrame( key );
	WwVkPost::freeChain( key );
	std::printf( "BLOOMDEV %s %s\n", mode.c_str(), fails ? "FAILED" : "OK" );
	return fails ? 1 : 0;
}
