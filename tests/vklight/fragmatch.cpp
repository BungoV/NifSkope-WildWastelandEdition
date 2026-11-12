// Lane VKLIGHT1 round 2 (2026-11-12): a converted fragment shader (res/shaders/hlsl/<module>/*_ps.hlsl, HLSLBULK1's
// spirv-cross ports) drawn on Vulkan as a full-screen triangle, on the inputs tests/vklight/frag_gate.py wrote and drew
// through the GL .frag. Generic: the bindings, uniform blocks and textures come from the job's job.txt (frag_gate.py
// builds it from spirv-cross --reflect of the SPIR-V this exe loads), so a new pass is a job, not new C++.
// Helpers copied from tests/vklight/lightmatch.cpp.
//
//   fragmatch <job.txt> [time]      writes vk_rt<i>.bin (int32 W, H, then W*H*4 float32) beside job.txt
// job.txt lines:
//   size <W> <H>
//   vs <spv>  /  ps <spv>                     (paths relative to job.txt's folder or absolute)
//   rt <count>                                (RGBA32F targets, cleared to 0)
//   ubo <binding> <file>                      (the block's bytes)
//   tex <binding> <2d|3d|2darray|2dms> <fmt> <w> <h> <d> <file>   (d: depth / layers / samples; file: raw texels)
//   smp <binding> <linear|nearest> <clamp|repeat>
// Last line `FRAGMATCH <job> RAN` (+ ` vk_ms <median>` with time) or `FRAGMATCH <job> FAIL (<why>)`.
#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include <windows.h>
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <string>
#include <vector>

#define VKF( name ) static PFN_##name name
VKF( vkCreateInstance ); VKF( vkEnumeratePhysicalDevices ); VKF( vkGetPhysicalDeviceQueueFamilyProperties );
VKF( vkGetPhysicalDeviceProperties ); VKF( vkGetPhysicalDeviceMemoryProperties ); VKF( vkCreateDevice ); VKF( vkGetDeviceQueue );
VKF( vkGetPhysicalDeviceFeatures );
VKF( vkCreateBuffer ); VKF( vkGetBufferMemoryRequirements ); VKF( vkAllocateMemory ); VKF( vkBindBufferMemory ); VKF( vkMapMemory );
VKF( vkCreateImage ); VKF( vkGetImageMemoryRequirements ); VKF( vkBindImageMemory ); VKF( vkCreateImageView ); VKF( vkCreateSampler );
VKF( vkCreateDescriptorSetLayout ); VKF( vkCreatePipelineLayout ); VKF( vkCreateShaderModule );
VKF( vkCreateDescriptorPool ); VKF( vkAllocateDescriptorSets ); VKF( vkUpdateDescriptorSets ); VKF( vkCreateCommandPool );
VKF( vkAllocateCommandBuffers ); VKF( vkBeginCommandBuffer ); VKF( vkEndCommandBuffer ); VKF( vkCmdBindPipeline );
VKF( vkCmdBindDescriptorSets ); VKF( vkQueueSubmit ); VKF( vkQueueWaitIdle );
VKF( vkCmdPipelineBarrier ); VKF( vkCmdCopyBufferToImage ); VKF( vkCmdCopyImageToBuffer ); VKF( vkCreateQueryPool );
VKF( vkCmdResetQueryPool ); VKF( vkCmdWriteTimestamp ); VKF( vkGetQueryPoolResults ); VKF( vkCmdClearColorImage );
VKF( vkCreateGraphicsPipelines ); VKF( vkCreateRenderPass ); VKF( vkCreateFramebuffer ); VKF( vkCmdBeginRenderPass );
VKF( vkCmdEndRenderPass ); VKF( vkCmdDraw );

static VkDevice dev;
static VkQueue queue;
static uint32_t family;
static VkPhysicalDeviceMemoryProperties memProps;
static VkCommandPool cpool;

static std::vector<uint8_t> readFile( const std::string & path )
{
	std::vector<uint8_t> v;
	FILE * f = std::fopen( path.c_str(), "rb" );
	if ( !f )
		return v;
	std::fseek( f, 0, SEEK_END );
	const long n = std::ftell( f );
	std::fseek( f, 0, SEEK_SET );
	v.resize( static_cast<size_t>( n ) );
	if ( std::fread( v.data(), 1, v.size(), f ) != v.size() )
		v.clear();
	std::fclose( f );
	return v;
}

static uint32_t memType( uint32_t bits, VkMemoryPropertyFlags want )
{
	for ( uint32_t t = 0; t < memProps.memoryTypeCount; t++ )
		if ( ( bits & ( 1u << t ) ) && ( memProps.memoryTypes[t].propertyFlags & want ) == want )
			return t;
	return 0;
}

struct Buf { VkBuffer b = VK_NULL_HANDLE; VkDeviceMemory m = VK_NULL_HANDLE; void * p = nullptr; size_t n = 0; };

static Buf makeBuf( size_t n )
{
	Buf B;
	B.n = n;
	VkBufferCreateInfo bi { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
	bi.size = n;
	bi.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
	vkCreateBuffer( dev, &bi, nullptr, &B.b );
	VkMemoryRequirements r;
	vkGetBufferMemoryRequirements( dev, B.b, &r );
	VkMemoryAllocateInfo ai { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
	ai.allocationSize = r.size;
	ai.memoryTypeIndex = memType( r.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT );
	vkAllocateMemory( dev, &ai, nullptr, &B.m );
	vkBindBufferMemory( dev, B.b, B.m, 0 );
	vkMapMemory( dev, B.m, 0, VK_WHOLE_SIZE, 0, &B.p );
	return B;
}

struct Fmt { const char * name; VkFormat f; int tb; };
static const Fmt kFmts[] = { { "rgba32f", VK_FORMAT_R32G32B32A32_SFLOAT, 16 }, { "rgba16f", VK_FORMAT_R16G16B16A16_SFLOAT, 8 },
	{ "rg32f", VK_FORMAT_R32G32_SFLOAT, 8 }, { "r32f", VK_FORMAT_R32_SFLOAT, 4 }, { "r16f", VK_FORMAT_R16_SFLOAT, 2 },
	{ "r8", VK_FORMAT_R8_UNORM, 1 }, { "rgba8", VK_FORMAT_R8G8B8A8_UNORM, 4 }, { "rg32ui", VK_FORMAT_R32G32_UINT, 8 },
	{ "r32ui", VK_FORMAT_R32_UINT, 4 }, { "rgba32ui", VK_FORMAT_R32G32B32A32_UINT, 16 } };

struct Img { VkImage i = VK_NULL_HANDLE; VkDeviceMemory m = VK_NULL_HANDLE; VkImageView v = VK_NULL_HANDLE; int w = 1, h = 1, d = 1, tb = 16;
	std::string kind = "2d"; VkFormat f = VK_FORMAT_R32G32B32A32_SFLOAT; };

static Img makeImg( const std::string & kind, int w, int h, int d, const Fmt & fm, bool target )
{
	Img I;
	I.kind = kind;
	I.w = w;
	I.h = h;
	I.d = d;
	I.tb = fm.tb;
	I.f = fm.f;
	VkImageCreateInfo ci { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
	ci.imageType = kind == "3d" ? VK_IMAGE_TYPE_3D : VK_IMAGE_TYPE_2D;
	ci.format = fm.f;
	ci.extent = { uint32_t( w ), uint32_t( h ), kind == "3d" ? uint32_t( d ) : 1u };
	ci.mipLevels = 1;
	ci.arrayLayers = kind == "2darray" ? uint32_t( d ) : 1u;
	ci.samples = kind == "2dms" ? VkSampleCountFlagBits( d ) : VK_SAMPLE_COUNT_1_BIT;
	ci.tiling = VK_IMAGE_TILING_OPTIMAL;
	ci.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT
		| ( target || kind == "2dms" ? VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT : 0 );
	ci.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	if ( vkCreateImage( dev, &ci, nullptr, &I.i ) != VK_SUCCESS )
		return Img();
	VkMemoryRequirements r;
	vkGetImageMemoryRequirements( dev, I.i, &r );
	VkMemoryAllocateInfo ai { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
	ai.allocationSize = r.size;
	ai.memoryTypeIndex = memType( r.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );
	vkAllocateMemory( dev, &ai, nullptr, &I.m );
	vkBindImageMemory( dev, I.i, I.m, 0 );
	VkImageViewCreateInfo vi { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
	vi.image = I.i;
	vi.viewType = kind == "3d" ? VK_IMAGE_VIEW_TYPE_3D : kind == "2darray" ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D;
	vi.format = fm.f;
	vi.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, kind == "2darray" ? uint32_t( d ) : 1u };
	vkCreateImageView( dev, &vi, nullptr, &I.v );
	return I;
}

static VkShaderModule loadModule( const std::string & path )
{
	const std::vector<uint8_t> spv = readFile( path );
	if ( spv.empty() )
		return VK_NULL_HANDLE;
	VkShaderModuleCreateInfo mi { VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
	mi.codeSize = spv.size();
	mi.pCode = reinterpret_cast<const uint32_t *>( spv.data() );
	VkShaderModule m = VK_NULL_HANDLE;
	vkCreateShaderModule( dev, &mi, nullptr, &m );
	return m;
}

static VkCommandBuffer newCb()
{
	VkCommandBufferAllocateInfo ca { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
	ca.commandPool = cpool;
	ca.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	ca.commandBufferCount = 1;
	VkCommandBuffer cb = VK_NULL_HANDLE;
	vkAllocateCommandBuffers( dev, &ca, &cb );
	VkCommandBufferBeginInfo bi { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
	vkBeginCommandBuffer( cb, &bi );
	return cb;
}

static void submitWait( VkCommandBuffer cb )
{
	VkSubmitInfo si { VK_STRUCTURE_TYPE_SUBMIT_INFO };
	si.commandBufferCount = 1;
	si.pCommandBuffers = &cb;
	vkQueueSubmit( queue, 1, &si, VK_NULL_HANDLE );
	vkQueueWaitIdle( queue );
}

static void toGeneral( VkCommandBuffer cb, const Img & x )
{
	VkImageMemoryBarrier ib { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
	ib.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	ib.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	ib.newLayout = VK_IMAGE_LAYOUT_GENERAL;
	ib.srcQueueFamilyIndex = ib.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	ib.image = x.i;
	ib.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, VK_REMAINING_ARRAY_LAYERS };
	vkCmdPipelineBarrier( cb, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
		0, 0, nullptr, 0, nullptr, 1, &ib );
}

static void barrierAll( VkCommandBuffer cb )
{
	VkMemoryBarrier mb { VK_STRUCTURE_TYPE_MEMORY_BARRIER };
	mb.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	mb.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	vkCmdPipelineBarrier( cb, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 1, &mb, 0, nullptr, 0, nullptr );
}

struct Tex { uint32_t binding; std::string kind, fmt, file; int w, h, d; };
struct Smp { uint32_t binding; std::string filt, wrap; };
struct Ubo { uint32_t binding; std::string file; };

int main( int argc, char ** argv )
{
	std::setvbuf( stdout, nullptr, _IONBF, 0 );
	if ( argc < 2 ) {
		std::printf( "usage: fragmatch <job.txt> [time]\n" );
		return 2;
	}
	const std::string jobPath = argv[1];
	const bool timeIt = argc > 2 && std::string( argv[2] ) == "time";
	const std::string dir = jobPath.substr( 0, jobPath.find_last_of( "/\\" ) + 1 );
	std::string name = dir.substr( 0, dir.size() - 1 );
	name = name.substr( name.find_last_of( "/\\" ) + 1 );
	auto fail = [&]( const char * why ) {
		std::printf( "FRAGMATCH %s FAIL (%s)\n", name.c_str(), why );
		return 2;
	};
	auto rel = [&]( const std::string & p ) { return ( p.size() > 1 && ( p[1] == ':' || p[0] == '/' ) ) ? p : dir + p; };

	int W = 0, H = 0, nrt = 1;
	std::string vsPath, psPath;
	std::vector<Tex> texs;
	std::vector<Smp> smps;
	std::vector<Ubo> ubos;
	{
		const std::vector<uint8_t> txt = readFile( jobPath );
		if ( txt.empty() )
			return fail( "job file" );
		std::istringstream in( std::string( txt.begin(), txt.end() ) );
		std::string line;
		while ( std::getline( in, line ) ) {
			std::istringstream l( line );
			std::string k;
			l >> k;
			if ( k == "size" )
				l >> W >> H;
			else if ( k == "vs" )
				l >> vsPath;
			else if ( k == "ps" )
				l >> psPath;
			else if ( k == "rt" )
				l >> nrt;
			else if ( k == "ubo" ) {
				Ubo u;
				l >> u.binding >> u.file;
				ubos.push_back( u );
			} else if ( k == "tex" ) {
				Tex t;
				l >> t.binding >> t.kind >> t.fmt >> t.w >> t.h >> t.d >> t.file;
				texs.push_back( t );
			} else if ( k == "smp" ) {
				Smp s;
				l >> s.binding >> s.filt >> s.wrap;
				smps.push_back( s );
			}
		}
	}
	if ( W <= 0 || H <= 0 || nrt < 1 || nrt > 4 || vsPath.empty() || psPath.empty() )
		return fail( "job has no size, shaders or targets" );

	HMODULE lib = LoadLibraryA( "vulkan-1.dll" );
	if ( !lib )
		return fail( "no vulkan-1.dll" );
	auto gipa = reinterpret_cast<PFN_vkGetInstanceProcAddr>( GetProcAddress( lib, "vkGetInstanceProcAddr" ) );
	vkCreateInstance = reinterpret_cast<PFN_vkCreateInstance>( gipa( nullptr, "vkCreateInstance" ) );
	VkApplicationInfo app { VK_STRUCTURE_TYPE_APPLICATION_INFO };
	app.apiVersion = VK_API_VERSION_1_2;
	VkInstanceCreateInfo ici { VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };
	ici.pApplicationInfo = &app;
	VkInstance inst;
	if ( vkCreateInstance( &ici, nullptr, &inst ) != VK_SUCCESS )
		return fail( "instance" );
#define LOADI( fn ) fn = reinterpret_cast<PFN_##fn>( gipa( inst, #fn ) )
	LOADI( vkEnumeratePhysicalDevices ); LOADI( vkGetPhysicalDeviceQueueFamilyProperties ); LOADI( vkGetPhysicalDeviceProperties );
	LOADI( vkGetPhysicalDeviceMemoryProperties ); LOADI( vkCreateDevice ); LOADI( vkGetDeviceQueue ); LOADI( vkGetPhysicalDeviceFeatures );
	uint32_t nd = 0;
	vkEnumeratePhysicalDevices( inst, &nd, nullptr );
	std::vector<VkPhysicalDevice> pds( nd );
	vkEnumeratePhysicalDevices( inst, &nd, pds.data() );
	if ( !nd )
		return fail( "no device" );
	VkPhysicalDevice phys = pds[0];
	for ( VkPhysicalDevice p : pds ) {
		VkPhysicalDeviceProperties pp;
		vkGetPhysicalDeviceProperties( p, &pp );
		if ( pp.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU ) {
			phys = p;
			break;
		}
	}
	VkPhysicalDeviceProperties props;
	vkGetPhysicalDeviceProperties( phys, &props );
	vkGetPhysicalDeviceMemoryProperties( phys, &memProps );
	VkPhysicalDeviceFeatures have {};
	vkGetPhysicalDeviceFeatures( phys, &have );
	uint32_t nq = 0;
	vkGetPhysicalDeviceQueueFamilyProperties( phys, &nq, nullptr );
	std::vector<VkQueueFamilyProperties> qf( nq );
	vkGetPhysicalDeviceQueueFamilyProperties( phys, &nq, qf.data() );
	family = UINT32_MAX;
	for ( uint32_t i = 0; family == UINT32_MAX && i < nq; i++ )
		if ( ( qf[i].queueFlags & VK_QUEUE_GRAPHICS_BIT ) && qf[i].timestampValidBits )
			family = i;
	if ( family == UINT32_MAX )
		return fail( "no graphics queue" );
	const float prio = 1.0f;
	VkDeviceQueueCreateInfo qi { VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO };
	qi.queueFamilyIndex = family;
	qi.queueCount = 1;
	qi.pQueuePriorities = &prio;
	VkPhysicalDeviceFeatures want {};
	want.shaderFloat64 = have.shaderFloat64;
	want.sampleRateShading = have.sampleRateShading;
	VkDeviceCreateInfo di { VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
	di.queueCreateInfoCount = 1;
	di.pQueueCreateInfos = &qi;
	di.pEnabledFeatures = &want;
	if ( vkCreateDevice( phys, &di, nullptr, &dev ) != VK_SUCCESS )
		return fail( "device" );
	auto gdpa = reinterpret_cast<PFN_vkGetDeviceProcAddr>( gipa( inst, "vkGetDeviceProcAddr" ) );
#define LOADD( fn ) fn = reinterpret_cast<PFN_##fn>( gdpa( dev, #fn ) )
	LOADD( vkCreateBuffer ); LOADD( vkGetBufferMemoryRequirements ); LOADD( vkAllocateMemory ); LOADD( vkBindBufferMemory );
	LOADD( vkMapMemory ); LOADD( vkCreateImage ); LOADD( vkGetImageMemoryRequirements ); LOADD( vkBindImageMemory );
	LOADD( vkCreateImageView ); LOADD( vkCreateSampler ); LOADD( vkCreateDescriptorSetLayout ); LOADD( vkCreatePipelineLayout );
	LOADD( vkCreateShaderModule ); LOADD( vkCreateDescriptorPool ); LOADD( vkAllocateDescriptorSets );
	LOADD( vkUpdateDescriptorSets ); LOADD( vkCreateCommandPool ); LOADD( vkAllocateCommandBuffers ); LOADD( vkBeginCommandBuffer );
	LOADD( vkEndCommandBuffer ); LOADD( vkCmdBindPipeline ); LOADD( vkCmdBindDescriptorSets );
	LOADD( vkQueueSubmit ); LOADD( vkQueueWaitIdle ); LOADD( vkCmdPipelineBarrier );
	LOADD( vkCmdCopyBufferToImage ); LOADD( vkCmdCopyImageToBuffer ); LOADD( vkCreateQueryPool ); LOADD( vkCmdResetQueryPool );
	LOADD( vkCmdWriteTimestamp ); LOADD( vkGetQueryPoolResults ); LOADD( vkCmdClearColorImage );
	LOADD( vkCreateGraphicsPipelines ); LOADD( vkCreateRenderPass ); LOADD( vkCreateFramebuffer ); LOADD( vkCmdBeginRenderPass );
	LOADD( vkCmdEndRenderPass ); LOADD( vkCmdDraw );
	vkGetDeviceQueue( dev, family, 0, &queue );
	{
		VkCommandPoolCreateInfo ci { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
		ci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
		ci.queueFamilyIndex = family;
		vkCreateCommandPool( dev, &ci, nullptr, &cpool );
	}

	// inputs
	std::vector<Img> inImgs;
	size_t maxBytes = size_t( W ) * H * 16;
	for ( const Tex & t : texs ) {
		const Fmt * fm = nullptr;
		for ( const Fmt & x : kFmts )
			if ( t.fmt == x.name )
				fm = &x;
		if ( !fm )
			return fail( "tex format" );
		inImgs.push_back( makeImg( t.kind, t.w, t.h, t.d, *fm, false ) );
		if ( !inImgs.back().i )
			return fail( "tex image" );
		maxBytes = std::max( maxBytes, size_t( t.w ) * t.h * t.d * fm->tb );
	}
	std::vector<Img> rts;
	for ( int k = 0; k < nrt; k++ )
		rts.push_back( makeImg( "2d", W, H, 1, kFmts[0], true ) );
	Buf stage = makeBuf( maxBytes + 256 );
	{
		VkCommandBuffer cb = newCb();
		for ( const Img & x : inImgs )
			toGeneral( cb, x );
		for ( const Img & x : rts )
			toGeneral( cb, x );
		vkEndCommandBuffer( cb );
		submitWait( cb );
	}
	for ( size_t i = 0; i < texs.size(); i++ ) {
		const Tex & t = texs[i];
		const Img & x = inImgs[i];
		VkCommandBuffer cb = newCb();
		if ( t.kind == "2dms" ) {	// a multisample input: cleared (the passes here read it only where a define asks)
			VkClearColorValue c {};
			VkImageSubresourceRange r { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
			vkCmdClearColorImage( cb, x.i, VK_IMAGE_LAYOUT_GENERAL, &c, 1, &r );
		} else {
			const std::vector<uint8_t> f = readFile( rel( t.file ) );
			const size_t want = size_t( t.w ) * t.h * t.d * x.tb;
			if ( f.size() != want ) {
				std::printf( "tex %u %s: %zu bytes, want %zu\n", t.binding, t.file.c_str(), f.size(), want );
				return fail( "tex file" );
			}
			std::memcpy( stage.p, f.data(), want );
			VkBufferImageCopy bc {};
			bc.imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, t.kind == "2darray" ? uint32_t( t.d ) : 1u };
			bc.imageExtent = { uint32_t( t.w ), uint32_t( t.h ), t.kind == "3d" ? uint32_t( t.d ) : 1u };
			vkCmdCopyBufferToImage( cb, stage.b, x.i, VK_IMAGE_LAYOUT_GENERAL, 1, &bc );
		}
		vkEndCommandBuffer( cb );
		submitWait( cb );
	}
	std::vector<Buf> uboBufs;
	for ( const Ubo & u : ubos ) {
		const std::vector<uint8_t> f = readFile( rel( u.file ) );
		if ( f.empty() )
			return fail( "ubo file" );
		uboBufs.push_back( makeBuf( f.size() ) );
		std::memcpy( uboBufs.back().p, f.data(), f.size() );
	}
	std::vector<VkSampler> samplers;
	for ( const Smp & s : smps ) {
		VkSamplerCreateInfo si { VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
		si.magFilter = si.minFilter = s.filt == "linear" ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;
		si.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
		si.addressModeU = si.addressModeV = si.addressModeW = s.wrap == "repeat" ? VK_SAMPLER_ADDRESS_MODE_REPEAT
																			   : VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		si.maxLod = 0.0f;
		samplers.push_back( VK_NULL_HANDLE );
		vkCreateSampler( dev, &si, nullptr, &samplers.back() );
	}

	// one set: every binding the job names
	std::vector<VkDescriptorSetLayoutBinding> lb;
	for ( const Ubo & u : ubos )
		lb.push_back( { u.binding, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr } );
	for ( const Tex & t : texs )
		lb.push_back( { t.binding, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr } );
	for ( const Smp & s : smps )
		lb.push_back( { s.binding, VK_DESCRIPTOR_TYPE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr } );
	VkDescriptorSetLayout setLayout = VK_NULL_HANDLE;
	VkDescriptorSetLayoutCreateInfo li { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
	li.bindingCount = uint32_t( lb.size() );
	li.pBindings = lb.data();
	if ( vkCreateDescriptorSetLayout( dev, &li, nullptr, &setLayout ) != VK_SUCCESS )
		return fail( "set layout" );
	VkPipelineLayout pipeLayout = VK_NULL_HANDLE;
	VkPipelineLayoutCreateInfo pli { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
	pli.setLayoutCount = 1;
	pli.pSetLayouts = &setLayout;
	if ( vkCreatePipelineLayout( dev, &pli, nullptr, &pipeLayout ) != VK_SUCCESS )
		return fail( "pipeline layout" );
	VkDescriptorPoolSize ps[3] = { { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, uint32_t( ubos.size() + 1 ) },
		{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, uint32_t( texs.size() + 1 ) }, { VK_DESCRIPTOR_TYPE_SAMPLER, uint32_t( smps.size() + 1 ) } };
	VkDescriptorPoolCreateInfo dp { VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
	dp.maxSets = 1;
	dp.poolSizeCount = 3;
	dp.pPoolSizes = ps;
	VkDescriptorPool pool = VK_NULL_HANDLE;
	if ( vkCreateDescriptorPool( dev, &dp, nullptr, &pool ) != VK_SUCCESS )
		return fail( "pool" );
	VkDescriptorSet set = VK_NULL_HANDLE;
	VkDescriptorSetAllocateInfo da { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
	da.descriptorPool = pool;
	da.descriptorSetCount = 1;
	da.pSetLayouts = &setLayout;
	if ( vkAllocateDescriptorSets( dev, &da, &set ) != VK_SUCCESS )
		return fail( "set" );
	{
		std::vector<VkDescriptorBufferInfo> bis( ubos.size() );
		std::vector<VkDescriptorImageInfo> iis( texs.size() + smps.size() );
		std::vector<VkWriteDescriptorSet> ws;
		for ( size_t i = 0; i < ubos.size(); i++ ) {
			bis[i] = { uboBufs[i].b, 0, VK_WHOLE_SIZE };
			VkWriteDescriptorSet w { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
			w.dstSet = set;
			w.dstBinding = ubos[i].binding;
			w.descriptorCount = 1;
			w.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			w.pBufferInfo = &bis[i];
			ws.push_back( w );
		}
		for ( size_t i = 0; i < texs.size(); i++ ) {
			iis[i] = { VK_NULL_HANDLE, inImgs[i].v, VK_IMAGE_LAYOUT_GENERAL };
			VkWriteDescriptorSet w { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
			w.dstSet = set;
			w.dstBinding = texs[i].binding;
			w.descriptorCount = 1;
			w.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
			w.pImageInfo = &iis[i];
			ws.push_back( w );
		}
		for ( size_t i = 0; i < smps.size(); i++ ) {
			iis[texs.size() + i] = { samplers[i], VK_NULL_HANDLE, VK_IMAGE_LAYOUT_UNDEFINED };
			VkWriteDescriptorSet w { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
			w.dstSet = set;
			w.dstBinding = smps[i].binding;
			w.descriptorCount = 1;
			w.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
			w.pImageInfo = &iis[texs.size() + i];
			ws.push_back( w );
		}
		vkUpdateDescriptorSets( dev, uint32_t( ws.size() ), ws.data(), 0, nullptr );
	}

	// render pass + pipeline: RGBA32F targets, LOAD + STORE, GENERAL throughout
	VkAttachmentDescription at[4] {};
	VkAttachmentReference cr[4];
	VkImageView views[4];
	for ( int k = 0; k < nrt; k++ ) {
		at[k].format = VK_FORMAT_R32G32B32A32_SFLOAT;
		at[k].samples = VK_SAMPLE_COUNT_1_BIT;
		at[k].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
		at[k].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		at[k].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		at[k].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		at[k].initialLayout = at[k].finalLayout = VK_IMAGE_LAYOUT_GENERAL;
		cr[k] = { uint32_t( k ), VK_IMAGE_LAYOUT_GENERAL };
		views[k] = rts[k].v;
	}
	VkSubpassDescription sp {};
	sp.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	sp.colorAttachmentCount = uint32_t( nrt );
	sp.pColorAttachments = cr;
	VkRenderPassCreateInfo rpi { VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO };
	rpi.attachmentCount = uint32_t( nrt );
	rpi.pAttachments = at;
	rpi.subpassCount = 1;
	rpi.pSubpasses = &sp;
	VkRenderPass rp = VK_NULL_HANDLE;
	if ( vkCreateRenderPass( dev, &rpi, nullptr, &rp ) != VK_SUCCESS )
		return fail( "render pass" );
	VkFramebufferCreateInfo fbi { VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
	fbi.renderPass = rp;
	fbi.attachmentCount = uint32_t( nrt );
	fbi.pAttachments = views;
	fbi.width = uint32_t( W );
	fbi.height = uint32_t( H );
	fbi.layers = 1;
	VkFramebuffer fb = VK_NULL_HANDLE;
	if ( vkCreateFramebuffer( dev, &fbi, nullptr, &fb ) != VK_SUCCESS )
		return fail( "framebuffer" );
	VkShaderModule vs = loadModule( rel( vsPath ) ), psm = loadModule( rel( psPath ) );
	if ( !vs || !psm )
		return fail( "shader module" );
	VkPipelineShaderStageCreateInfo st[2] = {
		{ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_VERTEX_BIT, vs, "main", nullptr },
		{ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_FRAGMENT_BIT, psm, "main", nullptr } };
	VkPipelineVertexInputStateCreateInfo vin { VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
	VkPipelineInputAssemblyStateCreateInfo ia { VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
	ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	VkViewport vp { 0, 0, float( W ), float( H ), 0, 1 };
	VkRect2D sc { { 0, 0 }, { uint32_t( W ), uint32_t( H ) } };
	VkPipelineViewportStateCreateInfo vps { VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO };
	vps.viewportCount = 1;
	vps.pViewports = &vp;
	vps.scissorCount = 1;
	vps.pScissors = &sc;
	VkPipelineRasterizationStateCreateInfo rs { VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
	rs.polygonMode = VK_POLYGON_MODE_FILL;
	rs.cullMode = VK_CULL_MODE_NONE;
	rs.lineWidth = 1.0f;
	VkPipelineMultisampleStateCreateInfo ms { VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
	ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	VkPipelineColorBlendAttachmentState cba[4] {};
	for ( auto & c : cba )
		c.colorWriteMask = 0xF;
	VkPipelineColorBlendStateCreateInfo cbs { VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
	cbs.attachmentCount = uint32_t( nrt );
	cbs.pAttachments = cba;
	VkGraphicsPipelineCreateInfo gpc { VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
	gpc.stageCount = 2;
	gpc.pStages = st;
	gpc.pVertexInputState = &vin;
	gpc.pInputAssemblyState = &ia;
	gpc.pViewportState = &vps;
	gpc.pRasterizationState = &rs;
	gpc.pMultisampleState = &ms;
	gpc.pColorBlendState = &cbs;
	gpc.layout = pipeLayout;
	gpc.renderPass = rp;
	VkPipeline pipe = VK_NULL_HANDLE;
	if ( vkCreateGraphicsPipelines( dev, VK_NULL_HANDLE, 1, &gpc, nullptr, &pipe ) != VK_SUCCESS || !pipe )
		return fail( "pipeline" );

	VkQueryPool qp = VK_NULL_HANDLE;
	VkQueryPoolCreateInfo qpi { VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO };
	qpi.queryType = VK_QUERY_TYPE_TIMESTAMP;
	qpi.queryCount = 2;
	vkCreateQueryPool( dev, &qpi, nullptr, &qp );
	auto draw = [&]( bool stamp, bool clear ) {
		VkCommandBuffer cb = newCb();
		if ( clear ) {
			VkClearColorValue c {};
			VkImageSubresourceRange r { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
			for ( const Img & x : rts )
				vkCmdClearColorImage( cb, x.i, VK_IMAGE_LAYOUT_GENERAL, &c, 1, &r );
			barrierAll( cb );
		}
		if ( stamp ) {
			vkCmdResetQueryPool( cb, qp, 0, 2 );
			vkCmdWriteTimestamp( cb, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, qp, 0 );
		}
		VkRenderPassBeginInfo rb { VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
		rb.renderPass = rp;
		rb.framebuffer = fb;
		rb.renderArea = sc;
		vkCmdBeginRenderPass( cb, &rb, VK_SUBPASS_CONTENTS_INLINE );
		vkCmdBindPipeline( cb, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe );
		vkCmdBindDescriptorSets( cb, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeLayout, 0, 1, &set, 0, nullptr );
		vkCmdDraw( cb, 3, 1, 0, 0 );
		vkCmdEndRenderPass( cb );
		if ( stamp )
			vkCmdWriteTimestamp( cb, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, qp, 1 );
		barrierAll( cb );
		vkEndCommandBuffer( cb );
		submitWait( cb );
	};
	draw( false, true );
	for ( int k = 0; k < nrt; k++ ) {
		VkCommandBuffer cb = newCb();
		VkBufferImageCopy bc {};
		bc.imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
		bc.imageExtent = { uint32_t( W ), uint32_t( H ), 1 };
		vkCmdCopyImageToBuffer( cb, rts[k].i, VK_IMAGE_LAYOUT_GENERAL, stage.b, 1, &bc );
		vkEndCommandBuffer( cb );
		submitWait( cb );
		const std::string out = dir + "vk_rt" + std::to_string( k ) + ".bin";
		FILE * f = std::fopen( out.c_str(), "wb" );
		if ( !f )
			return fail( "write target" );
		const int32_t wh[2] = { W, H };
		std::fwrite( wh, 4, 2, f );
		std::fwrite( stage.p, 1, size_t( W ) * H * 16, f );
		std::fclose( f );
	}
	std::string tail;
	if ( timeIt ) {	// warm the clock 0.5 s, then the median of 32
		const DWORD t0 = GetTickCount();
		while ( GetTickCount() - t0 < 500 )
			draw( false, false );
		std::vector<double> ms;
		for ( int r = 0; r < 32; r++ ) {
			draw( true, false );
			uint64_t ts[2] = { 0, 0 };
			vkGetQueryPoolResults( dev, qp, 0, 2, sizeof( ts ), ts, 8, VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT );
			ms.push_back( double( ts[1] - ts[0] ) * props.limits.timestampPeriod * 1e-6 );
		}
		std::sort( ms.begin(), ms.end() );
		char b[64];
		std::snprintf( b, sizeof( b ), " vk_ms %.4f", ms[16] );
		tail = b;
	}
	std::printf( "FRAGMATCH %s RAN%s\n", name.c_str(), tail.c_str() );
	return 0;
}
