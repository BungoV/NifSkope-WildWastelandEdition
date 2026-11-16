// Lane VKFLIGHT4 item 1 (2026-11-16): the view's QPainter overlay without GL (bungo 11-16: "OpenGL can stay in code for
// now, but not active while vulkan draws"). While the view is on Vulkan, glview's QPainter block (F3 line, pass timers,
// labels, gizmo) draws into a CPU QImage (painterTarget); painterDone hands its pixels here (overlaySet), present() copies
// the box that holds ink into ring slot k's host buffer once the slot is free (ovStage), and the worker blends it over the
// view in the slot's image before the flipped blit (ovRecord: overlay_blend_cs.hlsl). A single-sample view is copied into
// the slot first, so GL's own view image is never written.
#include "features/vkframe/overlay_blend_cs.spv.h"

namespace
{
struct OvSlot
{
	VkBuffer buf = VK_NULL_HANDLE;
	VkDeviceMemory mem = VK_NULL_HANDLE;
	void * map = nullptr;
	VkDeviceSize bytes = 0;
	VkDescriptorSet set = VK_NULL_HANDLE;
	VkImageView view = VK_NULL_HANDLE;  // of the slot's image, made at stage time, freed at the slot's next stage
	bool on = false;  // this slot's present blends
	std::uint32_t job[6] = {};  // x0, y0, w, h, viewH, red (overlay_blend_cs.hlsl's BlendJob)
};

constexpr int kOvSlots = kSlots + 1;  // the ring's, and one for the harness (overlayBlendTest)

struct Ov
{
	bool tried = false, ok = false;
	std::string why;
	VkDescriptorSetLayout dsl = VK_NULL_HANDLE;
	VkPipelineLayout layout = VK_NULL_HANDLE;
	VkPipeline pipe = VK_NULL_HANDLE;
	VkDescriptorPool pool = VK_NULL_HANDLE;
	OvSlot slot[kOvSlots];
	// the drawing thread's frame (painterDone -> present(), same thread)
	std::vector<std::uint32_t> pend;
	int px = 0, py = 0, pw = 0, ph = 0, viewW = 0, viewH = 0;
	std::uint32_t pred = 0;
	bool pendOn = false;
	int lastX = 0, lastY = 0, lastW = 0, lastH = 0;  // the last box with ink (the QImage clears only it)
	WwVkPresent::OverlayStat st;
};

Ov & ov()
{
	static Ov o;
	return o;
}

bool ovMake( std::string & why )
{
	Ov & O = ov();
	if ( O.tried ) {
		why = O.why;
		return O.ok;
	}
	O.tried = true;
	Vk & v = vk();
	auto fail = [&O, &why]( const std::string & w ) {
		O.why = why = w;
		return false;
	};
	if ( !v.ok || !v.dev )
		return O.tried = false, fail( "no present device yet" );
	if ( !v.vkCmdDispatch || !v.vkCreateComputePipelines )
		return fail( "no vkCmdDispatch / vkCreateComputePipelines" );
	VkDescriptorSetLayoutBinding b[2] = {};
	b[0].binding = 0;
	b[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	b[0].descriptorCount = 1;
	b[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	b[1] = b[0];
	b[1].binding = 1;
	b[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	VkDescriptorSetLayoutCreateInfo lci { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
	lci.bindingCount = 2;
	lci.pBindings = b;
	if ( v.vkCreateDescriptorSetLayout( v.dev, &lci, nullptr, &O.dsl ) != VK_SUCCESS )
		return fail( "the overlay's descriptor set layout failed" );
	const VkPushConstantRange pr { VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof( OvSlot::job ) };
	VkPipelineLayoutCreateInfo plci { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
	plci.setLayoutCount = 1;
	plci.pSetLayouts = &O.dsl;
	plci.pushConstantRangeCount = 1;
	plci.pPushConstantRanges = &pr;
	if ( v.vkCreatePipelineLayout( v.dev, &plci, nullptr, &O.layout ) != VK_SUCCESS )
		return fail( "the overlay's pipeline layout failed" );
	const VkDescriptorPoolSize ps[2] = { { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, kOvSlots }, { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, kOvSlots } };
	VkDescriptorPoolCreateInfo pci { VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
	pci.maxSets = kOvSlots;
	pci.poolSizeCount = 2;
	pci.pPoolSizes = ps;
	if ( v.vkCreateDescriptorPool( v.dev, &pci, nullptr, &O.pool ) != VK_SUCCESS )
		return fail( "the overlay's descriptor pool failed" );
	for ( int k = 0; k < kOvSlots; k++ ) {
		VkDescriptorSetAllocateInfo dai { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
		dai.descriptorPool = O.pool;
		dai.descriptorSetCount = 1;
		dai.pSetLayouts = &O.dsl;
		if ( v.vkAllocateDescriptorSets( v.dev, &dai, &O.slot[k].set ) != VK_SUCCESS )
			return fail( "the overlay's descriptor sets failed" );
	}
	VkShaderModuleCreateInfo mci { VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
	mci.codeSize = sizeof( vkframe_overlay_blend_cs_spv );
	mci.pCode = vkframe_overlay_blend_cs_spv;
	VkShaderModule m = VK_NULL_HANDLE;
	if ( v.vkCreateShaderModule( v.dev, &mci, nullptr, &m ) != VK_SUCCESS )
		return fail( "the overlay's shader module failed" );
	VkComputePipelineCreateInfo cp { VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO };
	cp.stage = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
	cp.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	cp.stage.module = m;
	cp.stage.pName = "main";
	cp.layout = O.layout;
	const VkResult r = v.vkCreateComputePipelines( v.dev, VK_NULL_HANDLE, 1, &cp, nullptr, &O.pipe );
	v.vkDestroyShaderModule( v.dev, m, nullptr );
	if ( r != VK_SUCCESS )
		return fail( "vkCreateComputePipelines (the overlay) returned " + std::to_string( int( r ) ) );
	O.ok = true;
	O.why.clear();
	return true;
}

/* slot k is free (present(): waitSlot + its fence; the harness: drained): the drawing thread's box goes into its host
 * buffer, a view of img and the set are written. Nothing pending: the slot does not blend */
bool ovStage( int k, VkImage img, std::string & why )
{
	Ov & O = ov();
	Vk & v = vk();
	OvSlot & S = O.slot[k];
	S.on = false;
	if ( S.view ) {
		v.vkDestroyImageView( v.dev, S.view, nullptr );
		S.view = VK_NULL_HANDLE;
	}
	if ( !O.pendOn )
		return true;
	O.pendOn = false;
	if ( !O.ok ) {
		why = "no overlay pipeline: " + O.why;
		return false;
	}
	const VkDeviceSize need = VkDeviceSize( O.pend.size() ) * 4;
	if ( need > S.bytes ) {
		if ( S.buf ) {
			v.vkUnmapMemory( v.dev, S.mem );
			v.vkDestroyBuffer( v.dev, S.buf, nullptr );
			v.vkFreeMemory( v.dev, S.mem, nullptr );
			S = OvSlot { VK_NULL_HANDLE, VK_NULL_HANDLE, nullptr, 0, S.set };
		}
		VkBufferCreateInfo bci { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
		bci.size = std::max<VkDeviceSize>( need, 65536 );
		bci.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
		if ( v.vkCreateBuffer( v.dev, &bci, nullptr, &S.buf ) != VK_SUCCESS ) {
			why = "the overlay's buffer failed";
			return false;
		}
		VkMemoryRequirements mr {};
		v.vkGetBufferMemoryRequirements( v.dev, S.buf, &mr );
		VkMemoryAllocateInfo ai { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
		ai.allocationSize = mr.size;
		ai.memoryTypeIndex = memType( mr.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT );
		if ( ai.memoryTypeIndex == UINT32_MAX || v.vkAllocateMemory( v.dev, &ai, nullptr, &S.mem ) != VK_SUCCESS
			|| v.vkBindBufferMemory( v.dev, S.buf, S.mem, 0 ) != VK_SUCCESS
			|| v.vkMapMemory( v.dev, S.mem, 0, VK_WHOLE_SIZE, 0, &S.map ) != VK_SUCCESS ) {
			why = "the overlay's buffer memory failed";
			return false;
		}
		S.bytes = bci.size;
	}
	std::memcpy( S.map, O.pend.data(), size_t( need ) );
	S.view = imageView( img, VK_IMAGE_VIEW_TYPE_2D, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT );
	if ( !S.view ) {
		why = "the overlay's image view failed";
		return false;
	}
	VkDescriptorImageInfo ii { VK_NULL_HANDLE, S.view, VK_IMAGE_LAYOUT_GENERAL };
	VkDescriptorBufferInfo bi { S.buf, 0, need };
	VkWriteDescriptorSet w[2] = { { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET }, { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET } };
	w[0].dstSet = S.set;
	w[0].dstBinding = 0;
	w[0].descriptorCount = 1;
	w[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	w[0].pImageInfo = &ii;
	w[1] = w[0];
	w[1].dstBinding = 1;
	w[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	w[1].pImageInfo = nullptr;
	w[1].pBufferInfo = &bi;
	v.vkUpdateDescriptorSets( v.dev, 2, w, 0, nullptr );
	const std::uint32_t job[6] = { std::uint32_t( O.px ), std::uint32_t( O.py ), std::uint32_t( O.pw ), std::uint32_t( O.ph ),
		std::uint32_t( O.viewH ), O.pred };
	std::memcpy( S.job, job, sizeof( job ) );
	S.on = true;
	return true;
}

/* the worker (or the harness): img (GENERAL) was last written by a transfer (the resolve or the copy); after this the
 * blend's writes are made visible to the blit by the caller's barrier from the compute stage */
void ovRecord( VkCommandBuffer cmd, int k, VkImage img )
{
	Ov & O = ov();
	Vk & v = vk();
	const OvSlot & S = O.slot[k];
	VkImageMemoryBarrier b { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
	b.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	b.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
	b.oldLayout = b.newLayout = VK_IMAGE_LAYOUT_GENERAL;
	b.srcQueueFamilyIndex = b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	b.image = img;
	b.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
	v.vkCmdPipelineBarrier( cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &b );
	v.vkCmdBindPipeline( cmd, VK_PIPELINE_BIND_POINT_COMPUTE, O.pipe );
	v.vkCmdBindDescriptorSets( cmd, VK_PIPELINE_BIND_POINT_COMPUTE, O.layout, 0, 1, &S.set, 0, nullptr );
	v.vkCmdPushConstants( cmd, O.layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof( S.job ), S.job );
	v.vkCmdDispatch( cmd, ( S.job[2] + 7 ) / 8, ( S.job[3] + 7 ) / 8, 1 );
}

bool ovOn( int k )
{
	return ov().slot[k].on;
}

// present(), slot k free: this frame's overlay into it; no view this frame (a new size): GL's back buffer, nothing blends
void ovPresent( int k, VkImage img, bool view )
{
	Ov & O = ov();
	if ( !view && O.pendOn ) {
		O.pendOn = false;
		O.st.lost++;
		O.st.lostWhy = "no view this frame (a new size): the frame came from GL's back buffer";
	}
	const bool had = O.pendOn;
	std::string why;
	if ( !ovStage( k, img, why ) ) {
		O.slot[k].on = false;
		O.st.refused++;
		O.st.refusedWhy = why;
	} else if ( had ) {
		O.st.drawn++;
	}
}
}  // namespace

namespace WwVkPresent
{
bool overlayReady( std::string & why )
{
	return ovMake( why );
}

void overlaySet( const std::uint32_t * bits, int w, int h, int strideWords, std::uint32_t red )
{
	Ov & O = ov();
	const double t0 = nowMs();
	if ( O.pendOn ) {
		O.st.lost++;  // the last frame's overlay never reached a present (a present that failed or went to GL's swap)
		O.st.lostWhy = "a frame's overlay was not presented";
	}
	O.pendOn = false;
	O.viewW = w;
	O.viewH = h;
	int x0 = w, y0 = h, x1 = -1, y1 = -1;
	for ( int y = 0; bits && y < h; y++ ) {
		const std::uint32_t * row = bits + size_t( y ) * size_t( strideWords );
		int a = 0;
		while ( a < w && !row[a] )
			a++;
		if ( a == w )
			continue;
		int e = w - 1;
		while ( !row[e] )
			e--;
		x0 = std::min( x0, a );
		x1 = std::max( x1, e );
		y0 = std::min( y0, y );
		y1 = y;
	}
	O.st.scanMs += nowMs() - t0;
	if ( x1 < 0 ) {
		O.lastW = O.lastH = 0;
		O.st.empty++;
		return;
	}
	O.px = x0;
	O.py = y0;
	O.pw = x1 - x0 + 1;
	O.ph = y1 - y0 + 1;
	O.lastX = x0;
	O.lastY = y0;
	O.lastW = O.pw;
	O.lastH = O.ph;
	O.pred = red;
	O.pend.resize( size_t( O.pw ) * size_t( O.ph ) );
	for ( int y = 0; y < O.ph; y++ )
		std::memcpy( O.pend.data() + size_t( y ) * size_t( O.pw ), bits + size_t( y0 + y ) * size_t( strideWords ) + x0, size_t( O.pw ) * 4 );
	O.pendOn = true;
	O.st.boxW = std::max( O.st.boxW, O.pw );
	O.st.boxH = std::max( O.st.boxH, O.ph );
}

void overlayLastBox( int & x, int & y, int & w, int & h )
{
	const Ov & O = ov();
	x = O.lastX;
	y = O.lastY;
	w = O.lastW;
	h = O.lastH;
}

void overlayGl( const std::string & why )
{
	Ov & O = ov();
	O.st.gl++;
	WwVkPresent::glGuard( "overlay", 1 );  // lane VKFLIGHT4: the view GL guard
	O.st.glWhy = why;
}

OverlayStat takeOverlayStat()
{
	Ov & O = ov();
	OverlayStat s = O.st;
	O.st = OverlayStat();
	return s;
}

/* the harness: backdrop (RGBA8, GL's rows, the bottom first) in an image of the present device, the overlay staged and
 * blended by ovStage / ovRecord (the ring's own code, on the extra slot), read back into out (same layout) */
bool overlayBlendTest( const std::vector<std::uint8_t> & backdrop, int w, int h, const std::uint32_t * bits, int strideWords,
	std::uint32_t red, std::vector<std::uint8_t> & out, std::string & why )
{
	Vk & v = vk();
	if ( !ovMake( why ) )
		return false;
	if ( backdrop.size() != size_t( w ) * size_t( h ) * 4 ) {
		why = "the backdrop is not w x h RGBA8";
		return false;
	}
	drain();
	VkImage img = VK_NULL_HANDLE;
	VkDeviceMemory imgMem = VK_NULL_HANDLE;
	VkImageCreateInfo ci { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
	ci.imageType = VK_IMAGE_TYPE_2D;
	ci.format = VK_FORMAT_R8G8B8A8_UNORM;
	ci.extent = { std::uint32_t( w ), std::uint32_t( h ), 1 };
	ci.mipLevels = 1;
	ci.arrayLayers = 1;
	ci.samples = VK_SAMPLE_COUNT_1_BIT;
	ci.tiling = VK_IMAGE_TILING_OPTIMAL;
	ci.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	VkBuffer buf = VK_NULL_HANDLE;
	VkDeviceMemory bufMem = VK_NULL_HANDLE;
	VkCommandBuffer cmd = VK_NULL_HANDLE;
	VkFence fence = VK_NULL_HANDLE;
	bool ok = false;
	const VkDeviceSize bytes = backdrop.size();
	do {
		if ( v.vkCreateImage( v.dev, &ci, nullptr, &img ) != VK_SUCCESS ) {
			why = "the test image failed";
			break;
		}
		VkMemoryRequirements r {};
		v.vkGetImageMemoryRequirements( v.dev, img, &r );
		VkMemoryAllocateInfo ai { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
		ai.allocationSize = r.size;
		ai.memoryTypeIndex = memType( r.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );
		if ( v.vkAllocateMemory( v.dev, &ai, nullptr, &imgMem ) != VK_SUCCESS || v.vkBindImageMemory( v.dev, img, imgMem, 0 ) != VK_SUCCESS ) {
			why = "the test image's memory failed";
			break;
		}
		VkBufferCreateInfo bci { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
		bci.size = bytes;
		bci.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
		if ( v.vkCreateBuffer( v.dev, &bci, nullptr, &buf ) != VK_SUCCESS ) {
			why = "the test buffer failed";
			break;
		}
		v.vkGetBufferMemoryRequirements( v.dev, buf, &r );
		ai.allocationSize = r.size;
		ai.memoryTypeIndex = memType( r.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT );
		void * m = nullptr;
		if ( v.vkAllocateMemory( v.dev, &ai, nullptr, &bufMem ) != VK_SUCCESS || v.vkBindBufferMemory( v.dev, buf, bufMem, 0 ) != VK_SUCCESS
			|| v.vkMapMemory( v.dev, bufMem, 0, bytes, 0, &m ) != VK_SUCCESS ) {
			why = "the test buffer's memory failed";
			break;
		}
		std::memcpy( m, backdrop.data(), size_t( bytes ) );
		// the app's hand-off: the drawing thread's scan, then the stage of a free slot
		overlaySet( bits, w, h, strideWords, red );
		if ( !ovStage( kSlots, img, why ) )
			break;
		VkCommandBufferAllocateInfo cai { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
		cai.commandPool = v.pool;
		cai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		cai.commandBufferCount = 1;
		VkFenceCreateInfo fi { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
		if ( v.vkAllocateCommandBuffers( v.dev, &cai, &cmd ) != VK_SUCCESS || v.vkCreateFence( v.dev, &fi, nullptr, &fence ) != VK_SUCCESS ) {
			why = "the test command buffer failed";
			break;
		}
		VkCommandBufferBeginInfo cbi { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
		cbi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		v.vkBeginCommandBuffer( cmd, &cbi );
		VkImageMemoryBarrier b { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
		b.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		b.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		b.newLayout = VK_IMAGE_LAYOUT_GENERAL;
		b.srcQueueFamilyIndex = b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		b.image = img;
		b.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
		v.vkCmdPipelineBarrier( cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &b );
		VkBufferImageCopy cp {};
		cp.imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
		cp.imageExtent = { std::uint32_t( w ), std::uint32_t( h ), 1 };
		v.vkCmdCopyBufferToImage( cmd, buf, img, VK_IMAGE_LAYOUT_GENERAL, 1, &cp );
		if ( ov().slot[kSlots].on )
			ovRecord( cmd, kSlots, img );
		b.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_SHADER_WRITE_BIT;
		b.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
		b.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
		v.vkCmdPipelineBarrier( cmd, VK_PIPELINE_STAGE_TRANSFER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
			0, 0, nullptr, 0, nullptr, 1, &b );
		v.vkCmdCopyImageToBuffer( cmd, img, VK_IMAGE_LAYOUT_GENERAL, buf, 1, &cp );
		v.vkEndCommandBuffer( cmd );
		if ( !oneShot( cmd, fence ) ) {
			why = "the test submit failed";
			break;
		}
		out.assign( static_cast<const std::uint8_t *>( m ), static_cast<const std::uint8_t *>( m ) + bytes );
		ok = true;
	} while ( false );
	if ( cmd )
		v.vkFreeCommandBuffers( v.dev, v.pool, 1, &cmd );
	if ( fence )
		v.vkDestroyFence( v.dev, fence, nullptr );
	if ( ov().slot[kSlots].view ) {
		v.vkDestroyImageView( v.dev, ov().slot[kSlots].view, nullptr );
		ov().slot[kSlots].view = VK_NULL_HANDLE;
	}
	if ( buf )
		v.vkDestroyBuffer( v.dev, buf, nullptr );
	if ( bufMem )
		v.vkFreeMemory( v.dev, bufMem, nullptr );
	if ( img )
		v.vkDestroyImage( v.dev, img, nullptr );
	if ( imgMem )
		v.vkFreeMemory( v.dev, imgMem, nullptr );
	return ok;
}
}  // namespace WwVkPresent
