// Lane VKOBJ2 (2026-11-13, plan row G4 slice 2): GL's bindless handles as descriptor indexing on the present device. Included
// by vkpresent.cpp after the objects' block (it uses Vk, Gl, Shared, sharedFind, sharedGen, makeLocal, imageView, oneShot,
// drain). The API and the row's meaning: vkbindless.h. The shader side: res/shaders/hlsl/near/ww_bindless_vk.hlsli.
#include "features/vkframe/vkbindless.h"

#include <unordered_map>

namespace
{
constexpr std::size_t kBlRowBytes = 176, kBlTexAt = 112;  // WwBl::Row: mat4 (64) + vec4 nm[3] (48), then uvec2 tex[8]
constexpr std::uint64_t kBlGraveTranslates = 8;           // a freed image index waits this many translates (the ring)

// the Vulkan sampler a GL state maps to: every field 4 bytes (memcmp-safe), anisotropy 0 when off
struct BlSmpKey
{
	std::uint32_t mag = 0, min = 0, mip = 0, u = 0, v = 0, border = 0;
	float bias = 0.0f, minLod = 0.0f, maxLod = 0.0f, aniso = 0.0f;
};

struct BlImg
{
	std::uintptr_t key = 0;
	int refs = 0;
};

struct BlEntry
{
	std::uint32_t img = 0, smp = 0;
};

struct Bl
{
	bool broken = false;
	std::string brokenWhy;
	VkDescriptorSetLayout dsl = VK_NULL_HANDLE;
	VkDescriptorPool pool = VK_NULL_HANDLE;
	VkDescriptorSet set = VK_NULL_HANDLE;
	VkImage zeroImg = VK_NULL_HANDLE;
	VkDeviceMemory zeroMem = VK_NULL_HANDLE;
	VkImageView zeroView = VK_NULL_HANDLE;
	VkCommandBuffer cmd = VK_NULL_HANDLE;
	VkFence fence = VK_NULL_HANDLE;
	std::vector<BlSmpKey> smpKeys;  // index = the sampler's array element (0: GL's default state)
	std::vector<VkSampler> smps;
	std::vector<BlImg> imgs;        // index = the image's array element (0: the black 1x1, key 0)
	std::unordered_map<std::uintptr_t, std::uint32_t> keyImg;
	std::unordered_map<std::uint64_t, BlEntry> handles;
	std::vector<std::pair<std::uint32_t, std::uint64_t>> graves;  // (index, the translate it was freed at)
	std::uint64_t serial = 0, gen = 0;
	WwVkPresent::BlStat st;
};

Bl & blS()
{
	static Bl s;
	return s;
}

bool blIsSrgb( VkFormat f )
{
	switch ( f ) {
	case VK_FORMAT_R8G8B8A8_SRGB:
	case VK_FORMAT_B8G8R8A8_SRGB:
	case VK_FORMAT_BC1_RGB_SRGB_BLOCK:
	case VK_FORMAT_BC1_RGBA_SRGB_BLOCK:
	case VK_FORMAT_BC2_SRGB_BLOCK:
	case VK_FORMAT_BC3_SRGB_BLOCK:
	case VK_FORMAT_BC7_SRGB_BLOCK:
		return true;
	default:
		return false;
	}
}

/* GL's sampler state as Vulkan's; false with a reason for a state Vulkan cannot match. GL's minification without mips
 * (NEAREST / LINEAR) is Vulkan's mip NEAREST with maxLod 0.25 (the spec's own mapping: level 0 only, the magnification
 * choice at lambda <= 0 unchanged); GL's min LOD below 0 is 0 (lambda <= 0 magnifies at level 0 either way) */
bool blSamplerKey( const WwVkPresent::BlSampler & s, BlSmpKey & k, std::string & why )
{
	Vk & v = vk();
	k = BlSmpKey();
	if ( s.magFilter != 0x2600 && s.magFilter != 0x2601 ) {
		why = "a magnification filter Vulkan has no twin of (" + std::to_string( s.magFilter ) + ")";
		return false;
	}
	k.mag = s.magFilter == 0x2600 ? VK_FILTER_NEAREST : VK_FILTER_LINEAR;
	bool mips = true;
	switch ( s.minFilter ) {
	case 0x2600: k.min = VK_FILTER_NEAREST; k.mip = VK_SAMPLER_MIPMAP_MODE_NEAREST; mips = false; break;
	case 0x2601: k.min = VK_FILTER_LINEAR; k.mip = VK_SAMPLER_MIPMAP_MODE_NEAREST; mips = false; break;
	case 0x2700: k.min = VK_FILTER_NEAREST; k.mip = VK_SAMPLER_MIPMAP_MODE_NEAREST; break;
	case 0x2701: k.min = VK_FILTER_LINEAR; k.mip = VK_SAMPLER_MIPMAP_MODE_NEAREST; break;
	case 0x2702: k.min = VK_FILTER_NEAREST; k.mip = VK_SAMPLER_MIPMAP_MODE_LINEAR; break;
	case 0x2703: k.min = VK_FILTER_LINEAR; k.mip = VK_SAMPLER_MIPMAP_MODE_LINEAR; break;
	default:
		why = "a minification filter Vulkan has no twin of (" + std::to_string( s.minFilter ) + ")";
		return false;
	}
	auto wrap = [&]( unsigned w, std::uint32_t & out ) {
		switch ( w ) {
		case 0x2901: out = VK_SAMPLER_ADDRESS_MODE_REPEAT; return true;
		case 0x812F: out = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE; return true;
		case 0x8370: out = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT; return true;
		case 0x812D: out = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER; return true;
		default: return false;  // GL_MIRROR_CLAMP_TO_EDGE needs Vulkan's samplerMirrorClampToEdge
		}
	};
	if ( !wrap( s.wrapS, k.u ) || !wrap( s.wrapT, k.v ) ) {
		why = "a wrap mode Vulkan has no twin of here (" + std::to_string( s.wrapS ) + ", " + std::to_string( s.wrapT ) + ")";
		return false;
	}
	k.border = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
	if ( k.u == VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER || k.v == VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER ) {
		const float * b = s.border;
		if ( b[0] == 0.0f && b[1] == 0.0f && b[2] == 0.0f && b[3] == 0.0f )
			k.border = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
		else if ( b[0] == 0.0f && b[1] == 0.0f && b[2] == 0.0f && b[3] == 1.0f )
			k.border = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
		else if ( b[0] == 1.0f && b[1] == 1.0f && b[2] == 1.0f && b[3] == 1.0f )
			k.border = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
		else {
			why = "a border colour other than transparent black, opaque black or opaque white";
			return false;
		}
	}
	if ( s.compareMode != 0 ) {
		why = "a depth compare sampler (the maps are colour)";
		return false;
	}
	if ( s.swizzle[0] != 0x1903 || s.swizzle[1] != 0x1904 || s.swizzle[2] != 0x1905 || s.swizzle[3] != 0x1906 ) {
		why = "a texture swizzle (the view would need its own)";
		return false;
	}
	if ( s.baseLevel != 0 || s.maxLevel < s.levels - 1 ) {
		why = "a base or max level that cuts the texture's levels (the view would need its own range)";
		return false;
	}
	if ( s.maxLod < 0.0f || s.maxLod < s.minLod ) {
		why = "a max LOD below 0 or below the min LOD";
		return false;
	}
	k.bias = s.lodBias;
	k.minLod = std::max( s.minLod, 0.0f );
	k.maxLod = mips ? s.maxLod : std::min( s.maxLod, 0.25f );
	if ( s.aniso > 1.0f ) {
		if ( !v.anisotropy ) {
			why = "anisotropic filtering without Vulkan's samplerAnisotropy";
			return false;
		}
		k.aniso = std::min( s.aniso, v.maxAniso );
	}
	return true;
}

void blWrite( std::uint32_t binding, std::uint32_t at, VkImageView view, VkSampler smp )
{
	Vk & v = vk();
	VkDescriptorImageInfo ii { smp, view, VK_IMAGE_LAYOUT_GENERAL };
	VkWriteDescriptorSet w { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
	w.dstSet = blS().set;
	w.dstBinding = binding;
	w.dstArrayElement = at;
	w.descriptorCount = 1;
	w.descriptorType = binding == 0 ? VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE : VK_DESCRIPTOR_TYPE_SAMPLER;
	w.pImageInfo = &ii;
	v.vkUpdateDescriptorSets( v.dev, 1, &w, 0, nullptr );
}

// the view the table samples for an owner's key: the shared twin (2D, one sample, colour)
VkImageView blView( std::uintptr_t key, VkFormat & fmt )
{
	// VKFAR1 (BC sheet twins, cube arrays) joins here: farTwinFind( key ) when sharedFind has none
	const Shared * s = sharedFind( key );
	if ( !s || s->type != 0 || s->samples != 1 || s->glFmt == kDepth24 || s->glFmt == kDepth32f || s->glFmt == kDepth16
		|| s->glFmt == kDepth24Stencil8 )
		return VK_NULL_HANDLE;
	fmt = s->fmt;
	return s->view;
}

// the sampler's array element for a key, made when first asked
bool blSamplerIndex( const BlSmpKey & k, std::uint32_t & at, std::string & why )
{
	Vk & v = vk();
	Bl & B = blS();
	for ( std::size_t i = 0; i < B.smpKeys.size(); i++ )
		if ( std::memcmp( &B.smpKeys[i], &k, sizeof( k ) ) == 0 ) {
			at = std::uint32_t( i );
			return true;
		}
	if ( B.smps.size() >= v.blSamplerCap ) {
		why = "the bindless sampler array is full (" + std::to_string( v.blSamplerCap ) + ")";
		return false;
	}
	VkSamplerCreateInfo ci { VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
	ci.magFilter = VkFilter( k.mag );
	ci.minFilter = VkFilter( k.min );
	ci.mipmapMode = VkSamplerMipmapMode( k.mip );
	ci.addressModeU = VkSamplerAddressMode( k.u );
	ci.addressModeV = VkSamplerAddressMode( k.v );
	ci.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	ci.mipLodBias = k.bias;
	ci.anisotropyEnable = k.aniso > 1.0f ? VK_TRUE : VK_FALSE;
	ci.maxAnisotropy = k.aniso > 1.0f ? k.aniso : 1.0f;
	ci.minLod = k.minLod;
	ci.maxLod = k.maxLod;
	ci.borderColor = VkBorderColor( k.border );
	VkSampler smp = VK_NULL_HANDLE;
	if ( v.vkCreateSampler( v.dev, &ci, nullptr, &smp ) != VK_SUCCESS ) {
		why = "vkCreateSampler (a bindless map's) failed";
		return false;
	}
	at = std::uint32_t( B.smps.size() );
	B.smps.push_back( smp );
	B.smpKeys.push_back( k );
	if ( B.set )
		blWrite( 1, at, VK_NULL_HANDLE, smp );
	return true;
}

// the layout, pool and set (both arrays partially bound, updated after bind and while unused), the black 1x1, index 0
bool blInit( std::string & why )
{
	Vk & v = vk();
	Bl & B = blS();
	if ( B.set )
		return true;
	const VkDescriptorBindingFlags fl = VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT
		| VK_DESCRIPTOR_BINDING_UPDATE_UNUSED_WHILE_PENDING_BIT;
	const VkDescriptorBindingFlags fls[2] = { fl, fl };
	VkDescriptorSetLayoutBindingFlagsCreateInfo bf { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO };
	bf.bindingCount = 2;
	bf.pBindingFlags = fls;
	const VkDescriptorSetLayoutBinding b[2] = { { 0, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, v.blImageCap, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr },
		{ 1, VK_DESCRIPTOR_TYPE_SAMPLER, v.blSamplerCap, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr } };
	VkDescriptorSetLayoutCreateInfo lci { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
	lci.pNext = &bf;
	lci.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
	lci.bindingCount = 2;
	lci.pBindings = b;
	const VkDescriptorPoolSize ps[2] = { { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, v.blImageCap }, { VK_DESCRIPTOR_TYPE_SAMPLER, v.blSamplerCap } };
	VkDescriptorPoolCreateInfo pci { VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
	pci.flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;
	pci.maxSets = 1;
	pci.poolSizeCount = 2;
	pci.pPoolSizes = ps;
	if ( v.vkCreateDescriptorSetLayout( v.dev, &lci, nullptr, &B.dsl ) != VK_SUCCESS
		|| v.vkCreateDescriptorPool( v.dev, &pci, nullptr, &B.pool ) != VK_SUCCESS ) {
		why = "the bindless set's layout or pool failed";
		return false;
	}
	VkDescriptorSetAllocateInfo ai { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
	ai.descriptorPool = B.pool;
	ai.descriptorSetCount = 1;
	ai.pSetLayouts = &B.dsl;
	if ( v.vkAllocateDescriptorSets( v.dev, &ai, &B.set ) != VK_SUCCESS ) {
		why = "the bindless set failed";
		return false;
	}
	// the black 1x1 (GENERAL, cleared once): handle 0 and unknown handles read it
	double mb = 0.0;
	VkFenceCreateInfo fi { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
	VkCommandBufferAllocateInfo cai { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
	cai.commandPool = v.pool;
	cai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	cai.commandBufferCount = 1;
	if ( !makeLocal( B.zeroImg, B.zeroMem, VK_IMAGE_TYPE_2D, VK_FORMAT_R8G8B8A8_UNORM, 1, 1, 1, mb )
		|| !( B.zeroView = imageView( B.zeroImg, VK_IMAGE_VIEW_TYPE_2D, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT ) )
		|| v.vkCreateFence( v.dev, &fi, nullptr, &B.fence ) != VK_SUCCESS || v.vkAllocateCommandBuffers( v.dev, &cai, &B.cmd ) != VK_SUCCESS ) {
		why = "the bindless black image failed";
		return false;
	}
	drain();  // the queue is the worker's
	VkCommandBufferBeginInfo bi { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
	bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	v.vkBeginCommandBuffer( B.cmd, &bi );
	VkImageMemoryBarrier ib { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
	ib.srcQueueFamilyIndex = ib.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	ib.image = B.zeroImg;
	ib.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
	ib.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	ib.newLayout = VK_IMAGE_LAYOUT_GENERAL;
	ib.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	v.vkCmdPipelineBarrier( B.cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &ib );
	const VkClearColorValue zero {};
	v.vkCmdClearColorImage( B.cmd, B.zeroImg, VK_IMAGE_LAYOUT_GENERAL, &zero, 1, &ib.subresourceRange );
	ib.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
	ib.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	ib.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
	v.vkCmdPipelineBarrier( B.cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, 1, &ib );
	if ( v.vkEndCommandBuffer( B.cmd ) != VK_SUCCESS || !oneShot( B.cmd, B.fence ) ) {
		why = "the bindless black image's clear failed";
		return false;
	}
	B.imgs.assign( 1, BlImg() );
	blWrite( 0, 0, B.zeroView, VK_NULL_HANDLE );
	for ( std::size_t i = 0; i < B.smps.size(); i++ )
		blWrite( 1, std::uint32_t( i ), VK_NULL_HANDLE, B.smps[i] );
	std::uint32_t at = 0;
	BlSmpKey k0;
	if ( B.smps.empty() && ( !blSamplerKey( WwVkPresent::BlSampler(), k0, why ) || !blSamplerIndex( k0, at, why ) ) )
		return false;
	B.gen = sharedGen();
	return true;
}

// the image indices' views again (a shared target was made or dropped: its view changed); a gone twin reads black
void blRewrite()
{
	Bl & B = blS();
	for ( std::size_t i = 1; i < B.imgs.size(); i++ ) {
		if ( B.imgs[i].refs <= 0 )
			continue;
		VkFormat fmt = VK_FORMAT_UNDEFINED;
		VkImageView view = blView( B.imgs[i].key, fmt );
		if ( !view ) {
			view = B.zeroView;
			B.st.misses++;
		}
		blWrite( 0, std::uint32_t( i ), view, VK_NULL_HANDLE );
		B.st.rewrites++;
	}
	B.gen = sharedGen();
}

// for the draws: the set and its layout (set 1 of a pipeline layout); false with a reason when the table is not made
bool blSetGet( VkDescriptorSetLayout & dsl, VkDescriptorSet & set, std::string & why )
{
	Bl & B = blS();
	if ( B.broken ) {
		why = B.brokenWhy;
		return false;
	}
	if ( !B.set && !blInit( why ) ) {
		B.broken = true;
		B.brokenWhy = why;
		return false;
	}
	dsl = B.dsl;
	set = B.set;
	return true;
}

// shutdown: the set, samplers and black image (the shared views are their owners'); the caller is before vkDestroyDevice
void blFree()
{
	Vk & v = vk();
	Bl & B = blS();
	if ( !B.dsl && !B.pool && B.smps.empty() && !B.zeroImg && !B.fence )
		return;
	drain();
	v.vkDeviceWaitIdle( v.dev );
	for ( VkSampler s : B.smps )
		v.vkDestroySampler( v.dev, s, nullptr );
	if ( B.zeroView )
		v.vkDestroyImageView( v.dev, B.zeroView, nullptr );
	if ( B.zeroImg )
		v.vkDestroyImage( v.dev, B.zeroImg, nullptr );
	if ( B.zeroMem )
		v.vkFreeMemory( v.dev, B.zeroMem, nullptr );
	if ( B.pool )
		v.vkDestroyDescriptorPool( v.dev, B.pool, nullptr );
	if ( B.dsl )
		v.vkDestroyDescriptorSetLayout( v.dev, B.dsl, nullptr );
	if ( B.cmd )
		v.vkFreeCommandBuffers( v.dev, v.pool, 1, &B.cmd );
	if ( B.fence )
		v.vkDestroyFence( v.dev, B.fence, nullptr );
	B = Bl();
}
}  // namespace

namespace WwVkPresent
{
bool blSamplerRead( unsigned tex, BlSampler & s, GlProc gp, std::string & why )
{
	using GetIv = void( WINAPI * )( GLuint, GLenum, GLint * );
	using GetFv = void( WINAPI * )( GLuint, GLenum, float * );
	static GetIv iv = nullptr;
	static GetFv fv = nullptr;
	if ( gp && !iv ) {
		iv = reinterpret_cast<GetIv>( gp( "glGetTextureParameteriv" ) );
		fv = reinterpret_cast<GetFv>( gp( "glGetTextureParameterfv" ) );
	}
	if ( !iv || !fv || !tex ) {
		why = "no glGetTextureParameteriv / fv (GL 4.5) or no texture";
		return false;
	}
	s = BlSampler();
	GLint i = 0;
	iv( tex, 0x2801, &i ); s.minFilter = unsigned( i );
	iv( tex, 0x2800, &i ); s.magFilter = unsigned( i );
	iv( tex, 0x2802, &i ); s.wrapS = unsigned( i );
	iv( tex, 0x2803, &i ); s.wrapT = unsigned( i );
	iv( tex, 0x813C, &s.baseLevel );
	iv( tex, 0x813D, &s.maxLevel );
	iv( tex, 0x82DF, &s.levels );  // GL_TEXTURE_IMMUTABLE_LEVELS (0 for a mutable texture: then base..max as GL says)
	if ( s.levels < 1 )
		s.levels = 1;
	iv( tex, 0x884C, &i ); s.compareMode = unsigned( i );
	iv( tex, 0x8A48, &i ); s.srgbDecode = unsigned( i );
	iv( tex, 0x8E46, s.swizzle );
	fv( tex, 0x8501, &s.lodBias );
	fv( tex, 0x813A, &s.minLod );
	fv( tex, 0x813B, &s.maxLod );
	fv( tex, 0x84FE, &s.aniso );
	fv( tex, 0x1004, s.border );
	return true;
}

std::string blRefusal()
{
	Vk & v = vk();
	if ( blS().broken )
		return blS().brokenWhy;
	if ( !v.ok )
		return "the present device is not ready";
	if ( !v.descIndexing )
		return "no descriptor indexing on the present device (runtime arrays, non-uniform sampled image indexing, partially "
			   "bound, update after bind and while unused)";
	if ( !v.drawParams )
		return "no shaderDrawParameters (the row by BaseInstance)";
	return std::string();
}

bool blAdd( std::uint64_t handle, std::uintptr_t key, const BlSampler & s, std::string & why )
{
	Bl & B = blS();
	auto refuse = [&]( const std::string & w ) {
		why = w;
		B.st.fails++;
		B.st.failWhy = w;
		return false;
	};
	if ( const std::string no = blRefusal(); !no.empty() )
		return refuse( no );
	if ( !handle )
		return refuse( "handle 0" );
	VkDescriptorSetLayout dsl = VK_NULL_HANDLE;
	VkDescriptorSet set = VK_NULL_HANDLE;
	std::string w;
	if ( !blSetGet( dsl, set, w ) )
		return refuse( w );
	if ( B.gen != sharedGen() )
		blRewrite();
	VkFormat fmt = VK_FORMAT_UNDEFINED;
	const VkImageView view = blView( key, fmt );
	if ( !view )
		return refuse( "no shared 2D twin for this texture" );
	if ( blIsSrgb( fmt ) && s.srgbDecode == 0x8A4A )
		return refuse( "an sRGB texture with its decode skipped" );
	BlSmpKey k;
	std::uint32_t smp = 0;
	if ( !blSamplerKey( s, k, w ) || !blSamplerIndex( k, smp, w ) )
		return refuse( w );
	auto old = B.handles.find( handle );
	if ( old != B.handles.end() ) {  // the same handle again: GL froze its state, so only its key can be news
		if ( B.imgs[old->second.img].key == key ) {
			old->second.smp = smp;
			return true;
		}
		blDrop( handle );
	}
	std::uint32_t img = 0;
	auto ki = B.keyImg.find( key );
	if ( ki != B.keyImg.end() )
		img = ki->second;
	else {
		auto g = B.graves.begin();
		for ( ; g != B.graves.end(); ++g )
			if ( B.serial >= g->second + kBlGraveTranslates )
				break;
		if ( g != B.graves.end() ) {
			img = g->first;
			B.graves.erase( g );
		} else {
			if ( B.imgs.size() >= vk().blImageCap )
				return refuse( "the bindless image array is full (" + std::to_string( vk().blImageCap ) + ")" );
			img = std::uint32_t( B.imgs.size() );
			B.imgs.push_back( BlImg() );
		}
		B.imgs[img].key = key;
		B.imgs[img].refs = 0;
		B.keyImg[key] = img;
		blWrite( 0, img, view, VK_NULL_HANDLE );  // a new or long-unused element: no pending work reads it
	}
	B.imgs[img].refs++;
	B.handles[handle] = { img, smp };
	B.st.adds++;
	return true;
}

void blDrop( std::uint64_t handle )
{
	Bl & B = blS();
	auto it = B.handles.find( handle );
	if ( it == B.handles.end() )
		return;
	const std::uint32_t img = it->second.img;
	B.handles.erase( it );
	B.st.drops++;
	if ( img == 0 || img >= B.imgs.size() || --B.imgs[img].refs > 0 )
		return;
	B.keyImg.erase( B.imgs[img].key );
	B.imgs[img] = BlImg();
	B.graves.push_back( { img, B.serial } );
}

std::size_t blTranslate( const void * glRows, void * vkRows, std::size_t n, int red )
{
	Bl & B = blS();
	B.serial++;
	if ( B.set && B.gen != sharedGen() )
		blRewrite();
	std::uint32_t redSmp = 0;
	if ( red == 2 ) {  // red: every map with GL's linear repeat
		BlSampler lin;
		lin.minFilter = 0x2703;
		BlSmpKey k;
		std::string w;
		if ( !blSamplerKey( lin, k, w ) || !blSamplerIndex( k, redSmp, w ) )
			redSmp = 0;
	}
	const std::uint32_t live = B.imgs.size() > 1 ? std::uint32_t( B.imgs.size() - 1 ) : 1u;
	const auto * src = static_cast<const unsigned char *>( glRows );
	auto * dst = static_cast<unsigned char *>( vkRows );
	std::size_t whole = 0;
	for ( std::size_t r = 0; r < n; r++ ) {
		const unsigned char * s = src + r * kBlRowBytes;
		unsigned char * d = dst + r * kBlRowBytes;
		std::memcpy( d, s, kBlTexAt );
		bool all = true;
		for ( int i = 0; i < 8; i++ ) {
			std::uint64_t h = 0;
			std::memcpy( &h, s + kBlTexAt + 8 * i, 8 );
			BlEntry e;
			if ( h ) {
				auto it = B.handles.find( h );
				if ( it != B.handles.end() )
					e = it->second;
				else {
					all = false;
					B.st.misses++;
				}
			}
			if ( red == 1 && e.img )
				e.img = e.img % live + 1;
			if ( red == 2 )
				e.smp = redSmp;
			const std::uint32_t pair[2] = { e.img, e.smp };
			std::memcpy( d + kBlTexAt + 8 * i, pair, 8 );
		}
		whole += all ? 1 : 0;
	}
	B.st.rows += int( n );
	return whole;
}

BlStat takeBlStat()
{
	Bl & B = blS();
	BlStat s = B.st;
	s.images = int( B.keyImg.size() );
	s.samplers = int( B.smps.size() );
	s.imageCap = vk().blImageCap;
	s.samplerCap = vk().blSamplerCap;
	if ( B.broken && s.failWhy.empty() )
		s.failWhy = B.brokenWhy;
	B.st = BlStat();
	return s;
}
}  // namespace WwVkPresent
