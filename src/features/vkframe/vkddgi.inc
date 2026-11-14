/* Lane VKFAR3 (row A1, 2026-11-15): the DDGI probe atlas on the present device. bakert's device (rtddgi_vk.inc) owns the
 * atlas and exports its memory (ddgiGpuShare); this file imports a second NT handle of that memory as a VkBuffer of the
 * exporter's size and usage, on the exporter's memory type, and views it as RGBA32F texels (cell_ddgi.glsl's samplerBuffer,
 * Buffer<float4> in HLSL). A consumer pass binds the view (ddgiAtlasView) and reads the atlas in place: no copy.
 * Order: see vkpresent.h DdgiAtlasShare (through GL's crossings; this file signals and waits nothing).
 * Included by vkpresent.cpp after vkpost7.inc. Gate: tests/vkframe/ddgiview.sh (export, import, read back through the view). */

namespace
{

struct DdgiImport
{
	VkBuffer buf = VK_NULL_HANDLE;
	VkDeviceMemory mem = VK_NULL_HANDLE;
	VkBufferView view = VK_NULL_HANDLE;
	VkDeviceSize atlasBytes = 0;
	std::uint64_t texels = 0;
};

// the gate's fetch (ddgi_atlas_fetch_cs): made on the first ddgiAtlasRead, freed with the imports
struct DdgiFetch
{
	VkDescriptorSetLayout dsl = VK_NULL_HANDLE;
	VkDescriptorPool pool = VK_NULL_HANDLE;
	VkDescriptorSet set = VK_NULL_HANDLE;
	VkPipelineLayout layout = VK_NULL_HANDLE;
	VkShaderModule cs = VK_NULL_HANDLE;
	VkPipeline pipe = VK_NULL_HANDLE;
	VkCommandPool cmdPool = VK_NULL_HANDLE;  // its own: v.pool belongs to the drawing thread
	VkCommandBuffer cmd = VK_NULL_HANDLE;
	VkFence fence = VK_NULL_HANDLE;
	VkBuffer out = VK_NULL_HANDLE;
	VkDeviceMemory outMem = VK_NULL_HANDLE;
	void * outPtr = nullptr;
	VkDeviceSize outCap = 0;
	// the job the worker runs (ddgiFetchWork)
	VkBufferView jobView = VK_NULL_HANDLE;
	std::uint32_t jobFirst = 0, jobCount = 0;
};

struct Ddgi
{
	std::map<const void *, DdgiImport> imports;
	DdgiFetch f;
	PFN_vkCreateBufferView createView = nullptr;
	PFN_vkDestroyBufferView destroyView = nullptr;
	PFN_vkDestroyCommandPool destroyPool = nullptr;  // Vk loads no vkDestroyCommandPool (v.pool lives as long as the device)
	VkDevice dev = VK_NULL_HANDLE;
	std::uint32_t maxTexels = 0, maxGroups = 0;
	VkDeviceSize texelAlign = 16;
};

Ddgi & ddgi()
{
	static Ddgi d;
	return d;
}

bool ddgiDev( std::string & why )
{
	Vk & v = vk();
	Ddgi & D = ddgi();
	if ( !v.dev ) {
		why = "no present device";
		return false;
	}
	if ( D.dev != v.dev ) {
		D.createView = reinterpret_cast<PFN_vkCreateBufferView>( v.vkGetDeviceProcAddr( v.dev, "vkCreateBufferView" ) );
		D.destroyView = reinterpret_cast<PFN_vkDestroyBufferView>( v.vkGetDeviceProcAddr( v.dev, "vkDestroyBufferView" ) );
		D.destroyPool = reinterpret_cast<PFN_vkDestroyCommandPool>( v.vkGetDeviceProcAddr( v.dev, "vkDestroyCommandPool" ) );
		VkPhysicalDeviceProperties2 p { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2 };
		v.vkGetPhysicalDeviceProperties2( v.phys, &p );
		D.maxTexels = p.properties.limits.maxTexelBufferElements;
		D.maxGroups = p.properties.limits.maxComputeWorkGroupCount[0];
		D.texelAlign = std::max<VkDeviceSize>( p.properties.limits.minTexelBufferOffsetAlignment, 1 );
		D.dev = v.dev;
	}
	if ( !D.createView || !D.destroyView || !D.destroyPool ) {
		why = "the present device has no buffer views / vkDestroyCommandPool";
		return false;
	}
	return true;
}

void ddgiImportFree( DdgiImport & a )
{
	Vk & v = vk();
	Ddgi & D = ddgi();
	if ( a.view && D.destroyView )
		D.destroyView( v.dev, a.view, nullptr );
	if ( a.buf )
		v.vkDestroyBuffer( v.dev, a.buf, nullptr );
	if ( a.mem )
		v.vkFreeMemory( v.dev, a.mem, nullptr );
	a = DdgiImport();
}

void ddgiFetchFree()
{
	Vk & v = vk();
	DdgiFetch & F = ddgi().f;
	if ( F.outMem && F.outPtr )
		v.vkUnmapMemory( v.dev, F.outMem );
	if ( F.out )
		v.vkDestroyBuffer( v.dev, F.out, nullptr );
	if ( F.outMem )
		v.vkFreeMemory( v.dev, F.outMem, nullptr );
	if ( F.fence )
		v.vkDestroyFence( v.dev, F.fence, nullptr );
	if ( F.cmdPool && ddgi().destroyPool )
		ddgi().destroyPool( v.dev, F.cmdPool, nullptr );  // frees the command buffer
	if ( F.pipe )
		v.vkDestroyPipeline( v.dev, F.pipe, nullptr );
	if ( F.cs )
		v.vkDestroyShaderModule( v.dev, F.cs, nullptr );
	if ( F.layout )
		v.vkDestroyPipelineLayout( v.dev, F.layout, nullptr );
	if ( F.pool )
		v.vkDestroyDescriptorPool( v.dev, F.pool, nullptr );
	if ( F.dsl )
		v.vkDestroyDescriptorSetLayout( v.dev, F.dsl, nullptr );
	F = DdgiFetch();
}

// declared beside the other drops (vkpresent.cpp); shutdown calls it
void ddgiFreeAll()
{
	Ddgi & D = ddgi();
	if ( D.imports.empty() && !D.f.pipe && !D.f.dsl && !D.f.cmdPool )
		return;
	drain();
	vk().vkDeviceWaitIdle( vk().dev );
	for ( auto & kv : D.imports )
		ddgiImportFree( kv.second );
	D.imports.clear();
	ddgiFetchFree();
}

bool ddgiFetchMake( std::string & why )
{
	Vk & v = vk();
	DdgiFetch & F = ddgi().f;
	if ( F.pipe )
		return true;
	auto fail = [&]( const char * w ) {
		ddgiFetchFree();
		why = w;
		return false;
	};
	if ( !v.vkCmdDispatch || !v.vkCreateComputePipelines )
		return fail( "no vkCmdDispatch / vkCreateComputePipelines" );
	// fxs shifts: t0 -> 64 (the atlas's texel view), u1 -> 193 (the texels out)
	VkDescriptorSetLayoutBinding b[2] {};
	b[0].binding = 64;
	b[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
	b[0].descriptorCount = 1;
	b[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	b[1].binding = 193;
	b[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	b[1].descriptorCount = 1;
	b[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	VkDescriptorSetLayoutCreateInfo dl { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
	dl.bindingCount = 2;
	dl.pBindings = b;
	VkDescriptorPoolSize ps[2] = { { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1 }, { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1 } };
	VkDescriptorPoolCreateInfo pi { VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
	pi.maxSets = 1;
	pi.poolSizeCount = 2;
	pi.pPoolSizes = ps;
	VkPushConstantRange pc { VK_SHADER_STAGE_COMPUTE_BIT, 0, 8 };
	if ( v.vkCreateDescriptorSetLayout( v.dev, &dl, nullptr, &F.dsl ) != VK_SUCCESS
		|| v.vkCreateDescriptorPool( v.dev, &pi, nullptr, &F.pool ) != VK_SUCCESS )
		return fail( "the DDGI fetch's descriptor set layout / pool failed" );
	VkDescriptorSetAllocateInfo dai { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
	dai.descriptorPool = F.pool;
	dai.descriptorSetCount = 1;
	dai.pSetLayouts = &F.dsl;
	VkPipelineLayoutCreateInfo plci { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
	plci.setLayoutCount = 1;
	plci.pSetLayouts = &F.dsl;
	plci.pushConstantRangeCount = 1;
	plci.pPushConstantRanges = &pc;
	if ( v.vkAllocateDescriptorSets( v.dev, &dai, &F.set ) != VK_SUCCESS
		|| v.vkCreatePipelineLayout( v.dev, &plci, nullptr, &F.layout ) != VK_SUCCESS )
		return fail( "the DDGI fetch's descriptor set / pipeline layout failed" );
	if ( !fxModule( vkframe_ddgi_atlas_fetch_cs_spv, sizeof( vkframe_ddgi_atlas_fetch_cs_spv ), F.cs ) )
		return fail( "the DDGI fetch's shader module failed" );
	VkComputePipelineCreateInfo cp { VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO };
	cp.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	cp.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	cp.stage.module = F.cs;
	cp.stage.pName = "main";
	cp.layout = F.layout;
	if ( v.vkCreateComputePipelines( v.dev, tone().cache, 1, &cp, nullptr, &F.pipe ) != VK_SUCCESS )
		return fail( "vkCreateComputePipelines (the DDGI fetch) failed" );
	VkCommandPoolCreateInfo cpi { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
	cpi.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	cpi.queueFamilyIndex = v.family;
	VkFenceCreateInfo fi { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
	if ( v.vkCreateCommandPool( v.dev, &cpi, nullptr, &F.cmdPool ) != VK_SUCCESS
		|| v.vkCreateFence( v.dev, &fi, nullptr, &F.fence ) != VK_SUCCESS )
		return fail( "the DDGI fetch's command pool / fence failed" );
	VkCommandBufferAllocateInfo cai { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
	cai.commandPool = F.cmdPool;
	cai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	cai.commandBufferCount = 1;
	if ( v.vkAllocateCommandBuffers( v.dev, &cai, &F.cmd ) != VK_SUCCESS )
		return fail( "the DDGI fetch's command buffer failed" );
	return true;
}

// the host-visible texels out, grown
bool ddgiFetchOut( VkDeviceSize bytes, std::string & why )
{
	Vk & v = vk();
	DdgiFetch & F = ddgi().f;
	if ( F.out && F.outCap >= bytes )
		return true;
	if ( F.outMem && F.outPtr )
		v.vkUnmapMemory( v.dev, F.outMem );
	if ( F.out )
		v.vkDestroyBuffer( v.dev, F.out, nullptr );
	if ( F.outMem )
		v.vkFreeMemory( v.dev, F.outMem, nullptr );
	F.out = VK_NULL_HANDLE;
	F.outMem = VK_NULL_HANDLE;
	F.outPtr = nullptr;
	F.outCap = 0;
	VkBufferCreateInfo bci { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
	bci.size = bytes;
	bci.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
	if ( v.vkCreateBuffer( v.dev, &bci, nullptr, &F.out ) != VK_SUCCESS ) {
		why = "vkCreateBuffer (the DDGI fetch's texels out) failed";
		return false;
	}
	VkMemoryRequirements r {};
	v.vkGetBufferMemoryRequirements( v.dev, F.out, &r );
	VkMemoryAllocateInfo ai { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
	ai.allocationSize = r.size;
	ai.memoryTypeIndex = memType( r.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT );
	if ( ai.memoryTypeIndex == UINT32_MAX || v.vkAllocateMemory( v.dev, &ai, nullptr, &F.outMem ) != VK_SUCCESS
		|| v.vkBindBufferMemory( v.dev, F.out, F.outMem, 0 ) != VK_SUCCESS
		|| v.vkMapMemory( v.dev, F.outMem, 0, VK_WHOLE_SIZE, 0, &F.outPtr ) != VK_SUCCESS ) {
		why = "no host-visible memory for the DDGI fetch's texels out";
		return false;
	}
	F.outCap = bytes;
	return true;
}

// on the worker (the queue is the worker's): the fetch recorded, submitted and waited (a gate's read, not a frame's pass)
bool ddgiFetchWork( std::string & why )
{
	Vk & v = vk();
	DdgiFetch & F = ddgi().f;
	VkDescriptorBufferInfo ob { F.out, 0, VkDeviceSize( F.jobCount ) * 16 };
	VkWriteDescriptorSet w[2] {};
	w[0].sType = w[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	w[0].dstSet = w[1].dstSet = F.set;
	w[0].dstBinding = 64;
	w[0].descriptorCount = 1;
	w[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
	w[0].pTexelBufferView = &F.jobView;
	w[1].dstBinding = 193;
	w[1].descriptorCount = 1;
	w[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	w[1].pBufferInfo = &ob;
	v.vkUpdateDescriptorSets( v.dev, 2, w, 0, nullptr );
	v.vkResetCommandBuffer( F.cmd, 0 );
	VkCommandBufferBeginInfo bi { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
	bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	v.vkBeginCommandBuffer( F.cmd, &bi );
	// the exporter's writes reach this queue by the gate's own fence wait (the app: GL's crossings, vkpresent.h)
	VkMemoryBarrier mb { VK_STRUCTURE_TYPE_MEMORY_BARRIER };
	mb.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;
	mb.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
	v.vkCmdPipelineBarrier( F.cmd, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &mb, 0, nullptr, 0, nullptr );
	v.vkCmdBindPipeline( F.cmd, VK_PIPELINE_BIND_POINT_COMPUTE, F.pipe );
	v.vkCmdBindDescriptorSets( F.cmd, VK_PIPELINE_BIND_POINT_COMPUTE, F.layout, 0, 1, &F.set, 0, nullptr );
	const std::uint32_t push[2] = { F.jobFirst, F.jobCount };
	v.vkCmdPushConstants( F.cmd, F.layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, 8, push );
	v.vkCmdDispatch( F.cmd, ( F.jobCount + 63 ) / 64, 1, 1 );
	VkMemoryBarrier hb { VK_STRUCTURE_TYPE_MEMORY_BARRIER };
	hb.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
	hb.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
	v.vkCmdPipelineBarrier( F.cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0, 1, &hb, 0, nullptr, 0, nullptr );
	if ( v.vkEndCommandBuffer( F.cmd ) != VK_SUCCESS ) {
		why = "the DDGI fetch's command buffer did not end";
		return false;
	}
	if ( !oneShot( F.cmd, F.fence ) ) {
		why = "the DDGI fetch's submit or its fence wait failed";
		return false;
	}
	return true;
}

}  // namespace

namespace WwVkPresent
{
bool ddgiAtlasImport( const void * key, const DdgiAtlasShare & in, std::string & why )
{
	// a Win32 NT handle stays the caller's after a Vulkan import: closed here on every way out
	struct Closer
	{
		HANDLE h;
		~Closer()
		{
			if ( h )
				CloseHandle( h );
		}
	} closer { static_cast<HANDLE>( in.memory ) };
	ddgiAtlasDrop( key );
	Vk & v = vk();
	Ddgi & D = ddgi();
	if ( !in.memory ) {
		why = "no memory handle";
		return false;
	}
	if ( !ddgiDev( why ) )
		return false;
	if ( std::memcmp( in.deviceUuid, v.uuid, VK_UUID_SIZE ) != 0 || std::memcmp( in.driverUuid, v.driverUuid, VK_UUID_SIZE ) != 0 ) {
		why = "the atlas is on another device or driver (device / driver UUID)";
		return false;
	}
	if ( !( in.usage & VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT ) ) {
		why = "the exporter's buffer is no uniform texel buffer (usage " + std::to_string( in.usage ) + ")";
		return false;
	}
	if ( in.atlasBytes == 0 || in.atlasBytes % 16 != 0 || in.atlasBytes > in.bufferSize || in.bufferSize > in.memorySize ) {
		why = "the atlas's sizes do not fit (atlas " + std::to_string( in.atlasBytes ) + ", buffer " + std::to_string( in.bufferSize )
			+ ", memory " + std::to_string( in.memorySize ) + ")";
		return false;
	}
	const std::uint64_t texels = in.atlasBytes / 16;
	if ( texels > D.maxTexels ) {
		why = std::to_string( texels ) + " texels, the device's texel buffers hold " + std::to_string( D.maxTexels );
		return false;
	}
	VkFormatProperties fp {};
	v.vkGetPhysicalDeviceFormatProperties( v.phys, VK_FORMAT_R32G32B32A32_SFLOAT, &fp );
	if ( !( fp.bufferFeatures & VK_FORMAT_FEATURE_UNIFORM_TEXEL_BUFFER_BIT ) ) {
		why = "RGBA32F is no uniform texel buffer on the present device";
		return false;
	}
	if ( in.memoryType >= v.mem.memoryTypeCount ) {
		why = "the exporter's memory type " + std::to_string( in.memoryType ) + " is not on the present device";
		return false;
	}
	DdgiImport a;
	auto fail = [&]( const std::string & w ) {
		ddgiImportFree( a );
		why = w;
		return false;
	};
	VkExternalMemoryBufferCreateInfo ext { VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_BUFFER_CREATE_INFO };
	ext.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT;
	VkBufferCreateInfo ci { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
	ci.pNext = &ext;
	ci.size = in.bufferSize;
	ci.usage = in.usage;
	ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	if ( v.vkCreateBuffer( v.dev, &ci, nullptr, &a.buf ) != VK_SUCCESS )
		return fail( "vkCreateBuffer (the DDGI atlas import) failed" );
	VkMemoryRequirements r {};
	v.vkGetBufferMemoryRequirements( v.dev, a.buf, &r );
	if ( !( r.memoryTypeBits & ( 1u << in.memoryType ) ) || r.size > in.memorySize )
		return fail( "the import's buffer does not take the exporter's memory (type bits " + std::to_string( r.memoryTypeBits ) + ", size "
			+ std::to_string( r.size ) + " of " + std::to_string( in.memorySize ) + ")" );
	VkImportMemoryWin32HandleInfoKHR im { VK_STRUCTURE_TYPE_IMPORT_MEMORY_WIN32_HANDLE_INFO_KHR };
	im.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT;
	im.handle = static_cast<HANDLE>( in.memory );
	VkMemoryDedicatedAllocateInfo ded { VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO };
	ded.buffer = a.buf;
	if ( in.dedicated )
		im.pNext = &ded;
	VkMemoryAllocateInfo ai { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
	ai.pNext = &im;
	ai.allocationSize = in.memorySize;
	ai.memoryTypeIndex = in.memoryType;
	const VkResult ar = v.vkAllocateMemory( v.dev, &ai, nullptr, &a.mem );
	if ( ar != VK_SUCCESS )
		return fail( "vkAllocateMemory (the DDGI atlas import) returned " + std::to_string( int( ar ) ) );
	if ( v.vkBindBufferMemory( v.dev, a.buf, a.mem, 0 ) != VK_SUCCESS )
		return fail( "vkBindBufferMemory (the DDGI atlas import) failed" );
	VkBufferViewCreateInfo vi { VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO };
	vi.buffer = a.buf;
	vi.format = VK_FORMAT_R32G32B32A32_SFLOAT;
	vi.offset = 0;
	vi.range = in.atlasBytes;
	if ( D.createView( v.dev, &vi, nullptr, &a.view ) != VK_SUCCESS )
		return fail( "vkCreateBufferView (the DDGI atlas) failed" );
	a.atlasBytes = in.atlasBytes;
	a.texels = texels;
	D.imports[key] = a;
	return true;
}

std::uint64_t ddgiAtlasView( const void * key, std::uint64_t * texels )
{
	const auto & m = ddgi().imports;
	const auto it = m.find( key );
	if ( texels )
		*texels = it == m.end() ? 0 : it->second.texels;
	return it == m.end() ? 0 : std::uint64_t( reinterpret_cast<std::uintptr_t>( it->second.view ) );
}

void ddgiAtlasDrop( const void * key )
{
	Ddgi & D = ddgi();
	const auto it = D.imports.find( key );
	if ( it == D.imports.end() )
		return;
	drain();
	vk().vkDeviceWaitIdle( vk().dev );
	ddgiImportFree( it->second );
	D.imports.erase( it );
}

bool ddgiAtlasRead( const void * key, std::uint32_t first, std::uint32_t count, std::vector<float> & out, int red, std::string & why )
{
	Vk & v = vk();
	Ddgi & D = ddgi();
	out.clear();
	const auto it = D.imports.find( key );
	if ( it == D.imports.end() ) {
		why = "no DDGI atlas under that key";
		return false;
	}
	const DdgiImport & a = it->second;
	if ( count == 0 || std::uint64_t( first ) + count > a.texels ) {
		why = "texels " + std::to_string( first ) + " + " + std::to_string( count ) + " past the atlas's " + std::to_string( a.texels );
		return false;
	}
	if ( ( count + 63 ) / 64 > D.maxGroups ) {
		why = "a read of " + std::to_string( count ) + " texels passes the device's work group count";
		return false;
	}
	if ( !ddgiFetchMake( why ) || !ddgiFetchOut( VkDeviceSize( count ) * 16, why ) )
		return false;
	// the reds' own views (red 1: one texel late at the device's offset alignment, red 2: the first half)
	VkBufferView redView = VK_NULL_HANDLE;
	if ( red == 1 || red == 2 ) {
		VkBufferViewCreateInfo vi { VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO };
		vi.buffer = a.buf;
		vi.format = VK_FORMAT_R32G32B32A32_SFLOAT;
		const VkDeviceSize late = ( 16 + D.texelAlign - 1 ) / D.texelAlign * D.texelAlign;
		vi.offset = red == 1 ? ( late + 15 ) / 16 * 16 : 0;
		vi.range = red == 1 ? a.atlasBytes - vi.offset : a.texels / 2 * 16;
		if ( D.createView( v.dev, &vi, nullptr, &redView ) != VK_SUCCESS ) {
			why = "vkCreateBufferView (a red's view) failed";
			return false;
		}
	}
	DdgiFetch & F = D.f;
	F.jobView = redView ? redView : a.view;
	F.jobFirst = first;
	F.jobCount = count;
	Worker & W = wk();
	std::uint64_t want = 0;
	{
		std::lock_guard<std::mutex> lk( W.m );
		want = W.toneDone + 1;
	}
	Work j;
	j.k = -1;
	j.fn = ddgiFetchWork;
	hand( j );
	bool ok = false;
	{
		std::unique_lock<std::mutex> lk( W.m );
		W.idle.wait( lk, [&W, want]() { return W.toneDone >= want; } );
		ok = W.toneOk;
		if ( !ok )
			why = W.toneWhy;
	}
	if ( redView )
		D.destroyView( v.dev, redView, nullptr );  // the work waited its fence
	F.jobView = VK_NULL_HANDLE;
	if ( !ok )
		return false;
	out.resize( size_t( count ) * 4 );
	std::memcpy( out.data(), F.outPtr, size_t( count ) * 16 );
	return true;
}
}  // namespace WwVkPresent
