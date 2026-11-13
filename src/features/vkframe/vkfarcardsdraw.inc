/* lane VKFAR2 (row G3, 2026-11-13): the far cards (impostor_instez.vert + impostor_cellinstez.frag, FOGLEAK) drawn on the
 * present device into the tone frame and its depth, through the VKAIR1 crossing (fx(): F.kind 3). Included once by
 * vkpresent.cpp after vkfarcards.inc (it reads that file's twins, texel buffers and set, and vkfardots.inc's farSharedOf).
 * GL (impostordraw.cpp drawCardsInstanced): one glDrawElementsInstanced of the unit quad a card set, depth LEQUAL with
 * write, no blend, no cull, the stencil as GL left it; per set the four sheets on units 0..3 and the loose uniforms.
 * Here: one descriptor set a card set (the sheets, both stages' b31), one indexed instanced draw each. The vertex stage
 * writes GL's clip z (depth clip control, negativeOneToOne). Every sampler is GL's own state, read off the texture (a
 * compare sampler for the shadow cubes from GL_TEXTURE_COMPARE_MODE / FUNC). */
namespace
{
constexpr VkDeviceSize kFcSetStride = 4096, kFcSetAt = 512, kFcPsAt = 256;  // b0 at 0; set i: its VS b31, its PS b31 at +256
constexpr std::uintptr_t kFcSlotKey = std::uintptr_t( 0xFCA0ull << 32 );   // + t slot (sharedTarget twins, types 0 / 1 / 2)
constexpr std::uintptr_t kFcSheetKey = std::uintptr_t( 0xFCB0ull << 32 );  // + GL name (farTwin, the sheets)
constexpr std::uintptr_t kFcCubeKey = std::uintptr_t( 0xFCC0ull << 32 );   // + 0 the shadow cubes, + 1 the zero cube
constexpr std::uintptr_t kFcBufKey = std::uintptr_t( 0xFCD0ull << 32 );    // + slot (0..25, 26 cardData, 27 cardFrames); 64+ zero

struct FcDraw
{
	bool made = false, broken = false;
	std::string brokenWhy;
	PFN_vkCmdDrawIndexed drawIndexed = nullptr;  // not in the device table (Vk): loaded here
	VkBuffer ubo = VK_NULL_HANDLE, quad = VK_NULL_HANDLE;
	VkDeviceMemory uboMem = VK_NULL_HANDLE, quadMem = VK_NULL_HANDLE;
	std::uint8_t * uboPtr = nullptr;
	std::map<std::string, VkPipeline> pipes;  // by GL's stencil state + red, over pipePass at pipeSamples
	VkRenderPass pipePass = VK_NULL_HANDLE;
	int pipeSamples = 0, pipelines = 0;
	std::map<std::string, VkSampler> samplers;  // by GL's sampling state
	std::map<std::uintptr_t, std::pair<unsigned, VkImage>> copied;  // a sheet twin -> the GL name / image copied into it
	VkImage zeroCube = VK_NULL_HANDLE;  // the zero cube twin's image when it was cleared
	double buildMs = 0.0;
	void * glCtx = nullptr;
	void( WINAPI * copyBuffer )( GLuint, GLuint, std::intptr_t, std::intptr_t, std::intptr_t ) = nullptr;  // glCopyNamedBufferSubData
	void( WINAPI * bufferSub )( GLuint, std::intptr_t, std::intptr_t, const void * ) = nullptr;            // glNamedBufferSubData
	void( WINAPI * clearTex )( GLuint, GLint, GLenum, GLenum, const void * ) = nullptr;                     // glClearTexImage
	// this run's job (the drawing thread writes it before the hand, the worker records it)
	VkPipeline pipe = VK_NULL_HANDLE;
	std::vector<int> counts;
	std::uint32_t stencilRef = 0;
	WwVkPresent::FxStat st;
};

FcDraw & fcDraw()
{
	static FcDraw d;
	return d;
}

bool fcHostBuffer( VkDeviceSize bytes, VkBufferUsageFlags usage, VkBuffer & buf, VkDeviceMemory & mem, std::uint8_t *& ptr )
{
	Vk & v = vk();
	VkBufferCreateInfo bci { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
	bci.size = bytes;
	bci.usage = usage;
	if ( v.vkCreateBuffer( v.dev, &bci, nullptr, &buf ) != VK_SUCCESS )
		return false;
	VkMemoryRequirements br {};
	v.vkGetBufferMemoryRequirements( v.dev, buf, &br );
	VkMemoryAllocateInfo ai { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
	ai.allocationSize = br.size;
	ai.memoryTypeIndex = memType( br.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT );
	void * p = nullptr;
	const bool ok = ai.memoryTypeIndex != UINT32_MAX && v.vkAllocateMemory( v.dev, &ai, nullptr, &mem ) == VK_SUCCESS
		&& v.vkBindBufferMemory( v.dev, buf, mem, 0 ) == VK_SUCCESS && v.vkMapMemory( v.dev, mem, 0, VK_WHOLE_SIZE, 0, &p ) == VK_SUCCESS;
	ptr = static_cast<std::uint8_t *>( p );
	return ok && ptr;
}

bool fcStatic( std::string & why )
{
	Vk & v = vk();
	FcDraw & D = fcDraw();
	void * ctx = reinterpret_cast<void *>( wglGetCurrentContext() );
	if ( D.glCtx != ctx ) {
		auto ld = []( auto & p, const char * n ) { p = reinterpret_cast<std::remove_reference_t<decltype( p )>>( wglGetProcAddress( n ) ); };
		ld( D.copyBuffer, "glCopyNamedBufferSubData" );
		ld( D.bufferSub, "glNamedBufferSubData" );
		ld( D.clearTex, "glClearTexImage" );
		D.glCtx = ctx;
		D.copied.clear();
		D.zeroCube = VK_NULL_HANDLE;
	}
	if ( !D.copyBuffer || !D.bufferSub || !D.clearTex ) {
		why = "GL has no glCopyNamedBufferSubData / glNamedBufferSubData / glClearTexImage in this context";
		return false;
	}
	if ( D.made )
		return true;
	const double t0 = nowMs();
	if ( !v.clipControl ) {
		why = "the present device has no depth clip control (the cards' vertex stage writes GL's clip z)";
		return false;
	}
	D.drawIndexed = reinterpret_cast<PFN_vkCmdDrawIndexed>( v.vkGetDeviceProcAddr( v.dev, "vkCmdDrawIndexed" ) );
	if ( !D.drawIndexed || !v.vkCmdBindVertexBuffers || !v.vkCmdBindIndexBuffer ) {
		why = "no vkCmdDrawIndexed / vkCmdBindVertexBuffers / vkCmdBindIndexBuffer on the present device";
		return false;
	}
	std::uint8_t * q = nullptr;
	if ( !fcHostBuffer( kFcSetAt + kFcSetStride * kFcSets, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, D.ubo, D.uboMem, D.uboPtr )
		|| !fcHostBuffer( 128, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, D.quad, D.quadMem, q ) ) {
		why = "the far cards' uniform / quad buffers failed";
		return false;
	}
	// impostordraw.cpp's unit quad: positions at 0 (vec3), uvs at 48 (vec2), indices at 80 (6 x uint16)
	static const float kPos[12] = { -1.0f, -1.0f, 0.0f, 1.0f, -1.0f, 0.0f, -1.0f, 1.0f, 0.0f, 1.0f, 1.0f, 0.0f };
	static const float kUv[8] = { 0.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f };
	static const std::uint16_t kIdx[6] = { 0, 1, 2, 2, 1, 3 };
	std::memcpy( q, kPos, sizeof( kPos ) );
	std::memcpy( q + 48, kUv, sizeof( kUv ) );
	std::memcpy( q + 80, kIdx, sizeof( kIdx ) );
	std::memset( D.uboPtr, 0, size_t( kFcSetAt + kFcSetStride * kFcSets ) );
	D.buildMs += nowMs() - t0;
	D.made = true;
	return true;
}

VkCompareOp fcCmp( int glFunc )
{
	return VkCompareOp( std::clamp( glFunc - 0x0200, 0, 7 ) );  // GL_NEVER..GL_ALWAYS = VK_COMPARE_OP_NEVER..ALWAYS
}

VkStencilOp fcStencilOp( int op )
{
	switch ( op ) {
	case 0: return VK_STENCIL_OP_ZERO;
	case 0x1E01: return VK_STENCIL_OP_REPLACE;
	case 0x1E02: return VK_STENCIL_OP_INCREMENT_AND_CLAMP;
	case 0x1E03: return VK_STENCIL_OP_DECREMENT_AND_CLAMP;
	case 0x150A: return VK_STENCIL_OP_INVERT;
	case 0x8507: return VK_STENCIL_OP_INCREMENT_AND_WRAP;
	case 0x8508: return VK_STENCIL_OP_DECREMENT_AND_WRAP;
	default: return VK_STENCIL_OP_KEEP;
	}
}

// GL's state at the draw: depth LEQUAL with write (red 1: no write), no blend, no cull, GL's stencil
bool fcPipe( const WwVkPresent::FarCardsStencil & s, int red, VkPipeline & out, std::string & why )
{
	Vk & v = vk();
	Fx & F = fx();
	FarCards & C = farCards();
	FcDraw & D = fcDraw();
	const std::string key = std::to_string( s.on ) + "," + std::to_string( s.func ) + "," + std::to_string( s.mask ) + ","
		+ std::to_string( s.write ) + "," + std::to_string( s.sfail ) + "," + std::to_string( s.zfail ) + "," + std::to_string( s.zpass )
		+ "," + std::to_string( red & 1 );
	if ( D.pipePass != F.airPass || D.pipeSamples != F.airSamples ) {
		for ( auto & kv : D.pipes )
			v.vkDestroyPipeline( v.dev, kv.second, nullptr );
		D.pipes.clear();
		D.pipePass = F.airPass;
		D.pipeSamples = F.airSamples;
	}
	auto it = D.pipes.find( key );
	if ( it != D.pipes.end() ) {
		out = it->second;
		return true;
	}
	const double t0 = nowMs();
	VkPipelineShaderStageCreateInfo st[2] = { { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO },
		{ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO } };
	st[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
	st[0].module = C.vs;
	st[0].pName = "main";
	st[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	st[1].module = C.ps;
	st[1].pName = "main";
	// impostor_instez_vs: vertexPosition (TEXCOORD0) location 0, multiTexCoord0 (TEXCOORD7) location 1, in declaration order
	const VkVertexInputBindingDescription vb[2] = { { 0, 12, VK_VERTEX_INPUT_RATE_VERTEX }, { 1, 8, VK_VERTEX_INPUT_RATE_VERTEX } };
	const VkVertexInputAttributeDescription va[2] = { { 0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0 }, { 1, 1, VK_FORMAT_R32G32_SFLOAT, 0 } };
	VkPipelineVertexInputStateCreateInfo vi { VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
	vi.vertexBindingDescriptionCount = 2;
	vi.pVertexBindingDescriptions = vb;
	vi.vertexAttributeDescriptionCount = 2;
	vi.pVertexAttributeDescriptions = va;
	VkPipelineInputAssemblyStateCreateInfo ia { VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
	ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	VkPipelineViewportDepthClipControlCreateInfoEXT cc { VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_DEPTH_CLIP_CONTROL_CREATE_INFO_EXT };
	cc.negativeOneToOne = VK_TRUE;
	VkPipelineViewportStateCreateInfo vps { VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO };
	vps.pNext = &cc;
	vps.viewportCount = vps.scissorCount = 1;
	VkPipelineRasterizationStateCreateInfo rs { VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
	rs.polygonMode = VK_POLYGON_MODE_FILL;
	rs.cullMode = VK_CULL_MODE_NONE;
	rs.frontFace = VK_FRONT_FACE_CLOCKWISE;
	rs.lineWidth = 1.0f;
	VkPipelineMultisampleStateCreateInfo ms { VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
	ms.rasterizationSamples = VkSampleCountFlagBits( F.airSamples );
	VkPipelineDepthStencilStateCreateInfo ds { VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };
	ds.depthTestEnable = VK_TRUE;
	ds.depthWriteEnable = ( red & 1 ) ? VK_FALSE : VK_TRUE;
	ds.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
	ds.stencilTestEnable = s.on ? VK_TRUE : VK_FALSE;
	ds.front.failOp = fcStencilOp( s.sfail );
	ds.front.depthFailOp = fcStencilOp( s.zfail );
	ds.front.passOp = fcStencilOp( s.zpass );
	ds.front.compareOp = fcCmp( s.func );
	ds.front.compareMask = std::uint32_t( s.mask ) & 0xFFu;
	ds.front.writeMask = std::uint32_t( s.write ) & 0xFFu;
	ds.back = ds.front;
	VkPipelineColorBlendAttachmentState cb {};
	cb.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	VkPipelineColorBlendStateCreateInfo cbs { VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
	cbs.attachmentCount = 1;
	cbs.pAttachments = &cb;
	const VkDynamicState dyn[3] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR, VK_DYNAMIC_STATE_STENCIL_REFERENCE };
	VkPipelineDynamicStateCreateInfo dy { VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
	dy.dynamicStateCount = 3;
	dy.pDynamicStates = dyn;
	VkGraphicsPipelineCreateInfo gp { VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
	gp.stageCount = 2;
	gp.pStages = st;
	gp.pVertexInputState = &vi;
	gp.pInputAssemblyState = &ia;
	gp.pViewportState = &vps;
	gp.pRasterizationState = &rs;
	gp.pMultisampleState = &ms;
	gp.pDepthStencilState = &ds;
	gp.pColorBlendState = &cbs;
	gp.pDynamicState = &dy;
	gp.layout = C.pipeLayout;
	gp.renderPass = F.airPass;
	if ( v.vkCreateGraphicsPipelines( v.dev, tone().cache, 1, &gp, nullptr, &out ) != VK_SUCCESS ) {
		why = "vkCreateGraphicsPipelines (the far cards) failed";
		return false;
	}
	D.pipes[key] = out;
	D.pipelines++;
	D.buildMs += nowMs() - t0;
	toneCacheSave();
	return true;
}

/* GL's sampling state on the texture -> one sampler. explicitLod: the slot is read with SampleLevel / Load only (the
 * sheets), so GL's anisotropy cannot act and is not asked of the device. compare: the slot is a SamplerComparisonState */
bool fcSampler( const WwVkPresent::FarCardTex & t, bool explicitLod, bool compare, int red, VkSampler & out, std::string & why )
{
	Vk & v = vk();
	FcDraw & D = fcDraw();
	auto addr = [&]( int w, VkSamplerAddressMode & m ) {
		switch ( w ) {
		case 0x2901: m = VK_SAMPLER_ADDRESS_MODE_REPEAT; return true;
		case 0x812F: m = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE; return true;
		case 0x8370: m = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT; return true;
		case 0x812D: m = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER; return true;  // GL's default border (0, 0, 0, 0)
		default: return false;
		}
	};
	VkSamplerCreateInfo sci { VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
	sci.magFilter = t.magF == 0x2600 ? VK_FILTER_NEAREST : VK_FILTER_LINEAR;
	const bool mip = t.minF >= 0x2700 && t.minF <= 0x2703;
	sci.minFilter = ( t.minF == 0x2600 || t.minF == 0x2700 || t.minF == 0x2702 ) ? VK_FILTER_NEAREST : VK_FILTER_LINEAR;
	sci.mipmapMode = ( t.minF == 0x2702 || t.minF == 0x2703 ) ? VK_SAMPLER_MIPMAP_MODE_LINEAR : VK_SAMPLER_MIPMAP_MODE_NEAREST;
	if ( !addr( t.wrap[0], sci.addressModeU ) || !addr( t.wrap[1], sci.addressModeV ) || !addr( t.wrap[2], sci.addressModeW ) ) {
		why = "a wrap mode the cards' samplers do not take (" + std::to_string( t.wrap[0] ) + " / " + std::to_string( t.wrap[1] ) + " / "
			+ std::to_string( t.wrap[2] ) + ")";
		return false;
	}
	sci.borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
	sci.mipLodBias = float( t.bias );
	if ( mip ) {
		sci.minLod = float( std::max( t.minLod, 0 ) );
		sci.maxLod = float( std::max( std::min( t.maxLod, t.levels - 1 ), std::max( t.minLod, 0 ) ) );
	} else {
		sci.minLod = 0.0f;
		sci.maxLod = 0.25f;  // GL without mipmaps: level 0, the min / mag switch kept
	}
	if ( t.aniso > 1 && !explicitLod ) {
		why = "GL's anisotropy " + std::to_string( t.aniso ) + " on a slot sampled with implicit level";
		return false;
	}
	if ( compare ) {
		if ( t.tex && t.cmpMode != 0x884E ) {
			why = "the shadow cubes have no GL_COMPARE_REF_TO_TEXTURE";
			return false;
		}
		sci.compareEnable = VK_TRUE;
		VkCompareOp op = t.tex ? fcCmp( t.cmpFunc ) : VK_COMPARE_OP_LESS_OR_EQUAL;
		if ( red & 4 )
			op = VkCompareOp( 7 - int( op ) );  // red: the compare inverted (LEQUAL <-> GREATER)
		sci.compareOp = op;
	} else if ( t.cmpMode == 0x884E ) {
		why = "a compare mode on a slot the shader samples without compare";
		return false;
	}
	const std::string key = std::to_string( sci.magFilter ) + "," + std::to_string( sci.minFilter ) + "," + std::to_string( sci.mipmapMode ) + ","
		+ std::to_string( sci.addressModeU ) + std::to_string( sci.addressModeV ) + std::to_string( sci.addressModeW ) + ","
		+ std::to_string( sci.mipLodBias ) + "," + std::to_string( sci.minLod ) + "," + std::to_string( sci.maxLod ) + ","
		+ std::to_string( sci.compareEnable ) + std::to_string( sci.compareOp );
	auto it = D.samplers.find( key );
	if ( it != D.samplers.end() ) {
		out = it->second;
		return true;
	}
	if ( v.vkCreateSampler( v.dev, &sci, nullptr, &out ) != VK_SUCCESS ) {
		why = "vkCreateSampler (the far cards) failed";
		return false;
	}
	D.samplers[key] = out;
	return true;
}

// a texel buffer slot: GL's range copied into the shared buffer (capacity a power of two: the card counts move every frame)
bool fcTexel( int slot, const WwVkPresent::FarCardTex & t, bool isInt, VkBufferView & view, FxNames & names, std::string & why )
{
	FcDraw & D = fcDraw();
	FarCards & C = farCards();
	unsigned glBuf = 0, glTex = 0;
	if ( !t.tex || !t.buf || t.bufBytes < 16 ) {  // GL's unbound buffer texture reads 0: a zeroed buffer of the slot's format
		const unsigned fmt = isInt ? kFcRgba32i : kFcRgba32f;
		const std::uintptr_t key = kFcBufKey + 64 + ( isInt ? 1 : 0 );
		const bool fresh = !C.bufs.count( key );
		if ( !WwVkPresent::farTexelBuffer( key, fmt, 16, glBuf, glTex, why ) )
			return false;
		if ( fresh ) {
			static const float zero[4] = {};
			D.bufferSub( glBuf, 0, 16, zero );
		}
		view = C.bufs[key].view;
		names.addBuf( glBuf );
		return true;
	}
	if ( isInt != ( t.glFmt == kFcRgba32i || t.glFmt == kFcRgba32ui ) ) {
		why = "texel buffer t" + std::to_string( slot ) + " has format " + std::to_string( t.glFmt ) + ", the shader reads the other kind";
		return false;
	}
	std::size_t cap = 4096;
	while ( cap < std::size_t( t.bufBytes ) )
		cap *= 2;
	const std::uintptr_t key = kFcBufKey + std::uintptr_t( slot );
	if ( !WwVkPresent::farTexelBuffer( key, t.glFmt, cap, glBuf, glTex, why ) )
		return false;
	D.copyBuffer( t.buf, glBuf, std::intptr_t( t.bufOff ), 0, std::intptr_t( t.bufBytes / 16 * 16 ) );
	view = C.bufs[key].view;
	names.addBuf( glBuf );
	return true;
}

// a sheet (any of the four): its twin, copied once a GL name (the sheets are written once, when their paint lands)
bool fcSheet( const WwVkPresent::FarCardTex & t, VkImageView & view, FxNames & names, int & copies, std::string & why )
{
	Gl & g = gl();
	FcDraw & D = fcDraw();
	if ( !t.tex ) {
		view = fx().d2;
		return true;
	}
	const std::uintptr_t key = kFcSheetKey + std::uintptr_t( t.tex );
	unsigned twin = 0;
	if ( !WwVkPresent::farTwin( key, 0, t.w, t.h, 1, t.glFmt, twin, why, t.levels ) )
		return false;
	const Shared * s = farTwinFind( key );
	if ( !s || !s->view ) {
		why = "a sheet's twin has no view";
		return false;
	}
	auto it = D.copied.find( key );
	if ( it == D.copied.end() || it->second.first != t.tex || it->second.second != s->img ) {
		for ( int l = 0; l < s->levels; l++ )
			g.copyImageSubData( t.tex, kTexture2D, l, 0, 0, 0, twin, kTexture2D, l, 0, 0, 0, std::max( t.w >> l, 1 ), std::max( t.h >> l, 1 ), 1 );
		D.copied[key] = { t.tex, s->img };
		copies++;
	}
	view = s->view;
	names.add( twin );
	return true;
}

// the shadow cubes (a cube-map array): copied every run; unbound, a zero cube (cleared once)
bool fcCube( const WwVkPresent::FarCardTex & t, VkImageView & view, FxNames & names, int & copies, std::string & why )
{
	Gl & g = gl();
	FcDraw & D = fcDraw();
	unsigned twin = 0;
	if ( !t.tex ) {
		if ( !WwVkPresent::farTwin( kFcCubeKey + 1, 3, 1, 1, 6, kFcDepth16, twin, why, 1 ) )
			return false;
		const Shared * s = farTwinFind( kFcCubeKey + 1 );
		if ( !s || !s->view ) {
			why = "the zero cube has no view";
			return false;
		}
		if ( D.zeroCube != s->img ) {
			const float one = 1.0f;  // depth 1: nothing in the zero cube shadows
			D.clearTex( twin, 0, 0x1902, 0x1406, &one );  // GL_DEPTH_COMPONENT, GL_FLOAT
			D.zeroCube = s->img;
		}
		view = s->view;
		names.add( twin );
		return true;
	}
	if ( !WwVkPresent::farTwin( kFcCubeKey, 3, t.w, t.h, t.d, t.glFmt, twin, why, 1 ) )
		return false;
	const Shared * s = farTwinFind( kFcCubeKey );
	if ( !s || !s->view ) {
		why = "the shadow cubes' twin has no view";
		return false;
	}
	g.copyImageSubData( t.tex, kFcCubeArray, 0, 0, 0, 0, twin, kFcCubeArray, 0, 0, 0, 0, t.w, t.h, t.d );
	copies++;
	view = s->view;
	names.add( twin );
	return true;
}

// an image slot other than the sheets and the cubes: a shared target read as is, or a per-slot twin copied every run
bool fcImage( int slot, const WwVkPresent::FarCardTex & t, int wantType, VkImageView & view, FxNames & names, int & copies, std::string & why )
{
	WwVkPresent::FxTex x;
	x.tex = t.tex;
	x.type = wantType;
	x.w = t.w;
	x.h = t.h;
	x.d = t.d;
	x.levels = std::max( t.levels, 1 );
	x.glFmt = t.glFmt;
	x.twin = kFcSlotKey + std::uintptr_t( slot );
	x.shared = farSharedOf( t.tex );
	if ( x.shared ) {
		const Shared * s = sharedFind( x.shared );
		if ( s && s->levels != x.levels ) {
			why = "t" + std::to_string( slot ) + ": the shared target has " + std::to_string( s->levels ) + " levels, GL samples "
				+ std::to_string( x.levels );
			return false;
		}
	}
	return fxSlot( x, view, names, copies, why );
}

// the worker (fxWork, F.kind 3): one indexed instanced draw a card set into the tone frame
bool fcRecord( std::string & why )
{
	Vk & v = vk();
	Fx & F = fx();
	FcDraw & D = fcDraw();
	FarCards & C = farCards();
	if ( !F.fbAir || !D.pipe || !D.quad ) {
		why = "no framebuffer / pipeline / quad for the far cards";
		return false;
	}
	VkRenderPassBeginInfo rbi { VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
	rbi.renderPass = F.airPass;
	rbi.framebuffer = F.fbAir;
	rbi.renderArea = { { 0, 0 }, { std::uint32_t( F.w ), std::uint32_t( F.h ) } };
	v.vkCmdBeginRenderPass( F.cmd, &rbi, VK_SUBPASS_CONTENTS_INLINE );
	// GL's viewport rows from the bottom = the image's rows from the top in memory: no flip (FragCoord.y = GL's)
	const VkViewport vpt { float( F.vp[0] ), float( F.vp[1] ), float( F.vp[2] ), float( F.vp[3] ), 0.0f, 1.0f };
	const int x0 = std::clamp( F.vp[0], 0, F.w ), y0 = std::clamp( F.vp[1], 0, F.h );
	const int x1 = std::clamp( F.vp[0] + F.vp[2], 0, F.w ), y1 = std::clamp( F.vp[1] + F.vp[3], 0, F.h );
	const VkRect2D sc { { x0, y0 }, { std::uint32_t( std::max( x1 - x0, 0 ) ), std::uint32_t( std::max( y1 - y0, 0 ) ) } };
	v.vkCmdSetViewport( F.cmd, 0, 1, &vpt );
	v.vkCmdSetScissor( F.cmd, 0, 1, &sc );
	v.vkCmdBindPipeline( F.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, D.pipe );
	v.vkCmdSetStencilReference( F.cmd, VK_STENCIL_FACE_FRONT_AND_BACK, D.stencilRef );
	const VkBuffer vbs[2] = { D.quad, D.quad };
	const VkDeviceSize offs[2] = { 0, 48 };
	v.vkCmdBindVertexBuffers( F.cmd, 0, 2, vbs, offs );
	v.vkCmdBindIndexBuffer( F.cmd, D.quad, 80, VK_INDEX_TYPE_UINT16 );
	for ( std::size_t i = 0; i < D.counts.size(); i++ ) {
		if ( D.counts[i] < 1 )
			continue;
		v.vkCmdBindDescriptorSets( F.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, C.pipeLayout, 0, 1, &C.sets[i], 0, nullptr );
		D.drawIndexed( F.cmd, 6, std::uint32_t( D.counts[i] ), 0, 0, 0 );  // the instance index from 0: cardBase is in b31
	}
	v.vkCmdEndRenderPass( F.cmd );
	return true;
}
}  // namespace

namespace WwVkPresent
{
bool farCardsRun( const FarCardsRun & t, std::string & why )
{
	Fx & F = fx();
	FcDraw & D = fcDraw();
	FarCards & C = farCards();
	Tone & T = tone();
	Vk & v = vk();
	Gl & g = gl();
	FxStat & S = D.st;
	const double t0 = nowMs();
	auto refuse = [&]( const std::string & w ) {
		why = w;
		S.fails++;
		S.failWhy = w;
		return false;
	};
	auto breakOff = [&]( const std::string & w ) {
		D.broken = true;
		D.brokenWhy = "the far cards on Vulkan broke: " + w;
		return refuse( D.brokenWhy );
	};
	if ( D.broken )
		return refuse( D.brokenWhy );
	if ( !toneFrameIs( t.color ) || !T.colorView || !T.dsView )
		return refuse( "the draw target is not the shared tone frame" );
	if ( T.w != t.w || T.h != t.h )
		return refuse( "the far cards' size is not the tone frame's" );
	if ( t.vp[2] < 1 || t.vp[3] < 1 )
		return refuse( "an empty viewport" );
	if ( t.sets.empty() )
		return refuse( "no card sets" );
	if ( t.sets.size() > kFcSets )
		return refuse( std::to_string( t.sets.size() ) + " card sets, the view's descriptor sets hold " + std::to_string( kFcSets ) );
	if ( !v.ok || !g.ok || g.ctx != reinterpret_cast<void *>( wglGetCurrentContext() ) )
		return refuse( "the present device or GL's entry points are not ready in this context" );
	if ( !g.copyImageSubData )
		return refuse( "no glCopyImageSubData (the inputs' copies into their twins)" );
	if ( F.pending ) {  // the last crossing is done before its command buffer, sets and buffers are written again
		v.vkWaitForFences( v.dev, 1, &F.fence, VK_TRUE, 1000000000ull );
		F.pending = false;
	}
	std::string w;
	FarSetCounts counts;
	const int samples = T.samples > 0 ? T.samples : std::max( F.airSamples, 1 );
	if ( !fxStatic( w ) || !fxSems( w ) || !fxPasses( samples, w ) || !farCardsSet( counts, w ) || !fcStatic( w ) )
		return breakOff( w );
	if ( F.airSamples != T.samples )
		return breakOff( "the far cards' pipelines are not at the frame's sample count" );
	if ( !fcPipe( t.stencil, t.red, D.pipe, w ) )
		return breakOff( w );
	FxNames names;
	names.add( T.colorTex );
	names.add( T.dsTex );
	// the slots every set shares: t0..t25 but the sheets, and the vertex stage's two texel buffers
	static const int kSheetSlot[4] = { 16, 19, 20, 21 };
	auto isSheet = []( int k ) { return k == 16 || k == 19 || k == 20 || k == 21; };
	VkImageView views[26] = {};
	VkBufferView bviews[26] = {};
	VkBufferView vsBuf[2] = {};
	VkSampler smp[26] = {};
	std::map<std::uint32_t, const FarSetSlot *> bind;
	for ( const FarSetSlot & s : counts.binds )
		bind[s.binding] = &s;
	for ( int k = 0; k < 26; k++ ) {
		auto it = bind.find( std::uint32_t( 64 + k ) );
		if ( it == bind.end() || isSheet( k ) )
			continue;
		const FarSetSlot & s = *it->second;
		const FarCardTex & x = t.tex[k];
		bool ok = true;
		if ( s.kind == 2 )
			ok = fcTexel( k, x, k == 6, bviews[k], names, w );
		else if ( s.dim == 3 && s.arrayed )
			ok = fcCube( x, views[k], names, S.copies, w );
		else {
			const int want = s.dim == 2 ? 1 : s.arrayed ? 2 : 0;
			if ( x.tex && x.type != want )
				return refuse( "t" + std::to_string( k ) + ": GL's slot type is not the shader's" );
			ok = fcImage( k, x, want, views[k], names, S.copies, w );
		}
		if ( !ok )
			return refuse( "the far cards' input t" + std::to_string( k ) + ": " + w );
		auto si = bind.find( std::uint32_t( 128 + k ) );
		if ( si != bind.end() && !fcSampler( x, false, si->second->compare, t.red, smp[k], w ) )
			return refuse( "the far cards' sampler s" + std::to_string( k ) + ": " + w );
	}
	if ( !fcTexel( 26, t.cardData, false, vsBuf[0], names, w ) || !fcTexel( 27, t.cardFrames, false, vsBuf[1], names, w ) )
		return refuse( "the far cards' card buffers: " + w );
	// per set: its sheets, its samplers, both stages' b31
	std::memcpy( D.uboPtr, t.global, size_t( kFxGlobalBytes ) );
	std::vector<VkWriteDescriptorSet> ws;
	std::vector<VkDescriptorImageInfo> ii;
	std::vector<VkDescriptorBufferInfo> bi;
	std::vector<VkBufferView> bv;
	const std::size_t perSet = counts.binds.size() + 3;
	ws.reserve( perSet * t.sets.size() );
	ii.reserve( perSet * t.sets.size() );
	bi.reserve( 3 * t.sets.size() );
	bv.reserve( perSet * t.sets.size() );
	D.counts.assign( t.sets.size(), 0 );
	for ( std::size_t i = 0; i < t.sets.size(); i++ ) {
		const FarCardsSet & cs = t.sets[i];
		VkImageView sv[26] = {};
		VkSampler ss[26] = {};
		for ( int j = 0; j < 4; j++ ) {
			const int k = kSheetSlot[j];
			if ( !bind.count( std::uint32_t( 64 + k ) ) )
				continue;
			if ( !fcSheet( cs.sheet[j], sv[k], names, S.copies, w ) )
				return refuse( "a card set's sheet t" + std::to_string( k ) + ": " + w );
			if ( bind.count( std::uint32_t( 128 + k ) ) && !fcSampler( cs.sheet[j], true, false, t.red, ss[k], w ) )
				return refuse( "a card set's sheet sampler s" + std::to_string( k ) + ": " + w );
		}
		const VkDeviceSize at = kFcSetAt + kFcSetStride * VkDeviceSize( i );
		std::memcpy( D.uboPtr + at, cs.vsLoose, size_t( kT_farcardsVsBytes ) );
		if ( t.red & 2 ) {  // red: the vertex stage's per-set cardBase dropped (every set draws from card 0)
			const int base = 0;
			std::memcpy( D.uboPtr + at, &base, 4 );
		}
		std::memcpy( D.uboPtr + at + kFcPsAt, cs.loose, size_t( kT_farcardsBytes ) );
		const VkDescriptorSet set = C.sets[i];
		auto ubo = [&]( std::uint32_t b, VkDeviceSize off, VkDeviceSize range ) {
			bi.push_back( { D.ubo, off, range } );
			VkWriteDescriptorSet x { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
			x.dstSet = set;
			x.dstBinding = b;
			x.descriptorCount = 1;
			x.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			x.pBufferInfo = &bi.back();
			ws.push_back( x );
		};
		auto texel = [&]( std::uint32_t b, VkBufferView view ) {
			bv.push_back( view );
			VkWriteDescriptorSet x { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
			x.dstSet = set;
			x.dstBinding = b;
			x.descriptorCount = 1;
			x.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
			x.pTexelBufferView = &bv.back();
			ws.push_back( x );
		};
		ubo( 0, 0, VkDeviceSize( kFxGlobalBytes ) );
		ubo( 32, at, VkDeviceSize( kT_farcardsVsBytes ) );
		texel( 96, vsBuf[0] );
		texel( 97, vsBuf[1] );
		for ( const FarSetSlot & s : counts.binds ) {
			if ( s.kind == 0 ) {
				if ( s.binding == 31 )
					ubo( 31, at + kFcPsAt, VkDeviceSize( kT_farcardsBytes ) );
				else if ( s.binding != 0 )
					return breakOff( "the far cards' pixel stage has a uniform block at binding " + std::to_string( s.binding ) );
				continue;
			}
			const int k = int( s.binding ) - ( s.kind == 3 ? 128 : 64 );
			if ( k < 0 || k > 25 )
				return breakOff( "the far cards' binding " + std::to_string( s.binding ) + " is out of t0..t25 / s0..s25" );
			if ( s.kind == 2 ) {
				texel( s.binding, bviews[k] );
				continue;
			}
			VkDescriptorImageInfo info {};
			VkWriteDescriptorSet x { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
			x.dstSet = set;
			x.dstBinding = s.binding;
			x.descriptorCount = 1;
			if ( s.kind == 1 ) {
				info.imageView = isSheet( k ) ? sv[k] : views[k];
				info.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
				x.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
			} else {
				info.sampler = isSheet( k ) ? ss[k] : smp[k];
				x.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
			}
			if ( ( s.kind == 1 && !info.imageView ) || ( s.kind == 3 && !info.sampler ) )
				return refuse( "binding " + std::to_string( s.binding ) + " has nothing to bind" );
			ii.push_back( info );
			x.pImageInfo = &ii.back();
			ws.push_back( x );
		}
		D.counts[i] = cs.count;
	}
	if ( names.full )
		return refuse( "more GL names than a crossing hands over (" + std::to_string( t.sets.size() ) + " card sets)" );
	const VkImageView att[2] = { T.colorView, T.dsView };
	if ( !fxFb( F.fbAir, F.airPass, att, 2, T.w, T.h ) )
		return breakOff( "the far cards' framebuffer over the tone frame failed" );
	v.vkUpdateDescriptorSets( v.dev, std::uint32_t( ws.size() ), ws.data(), 0, nullptr );
	D.stencilRef = std::uint32_t( t.stencil.ref ) & 0xFFu;
	F.kind = 3;
	F.w = t.w;
	F.h = t.h;
	std::memcpy( F.vp, t.vp, sizeof( F.vp ) );
	if ( !fxCross( names, w ) )
		return breakOff( w );  // nothing signals vkDone: GL does not wait it; GL draws its cards this frame
	const double ms = nowMs() - t0;
	S.runs++;
	S.cpuMs += ms;
	S.maxMs = std::max( S.maxMs, ms );
	return true;
}

void farCardsOnGl( const char * why )
{
	FxStat & S = fcDraw().st;
	S.gl++;
	S.glWhy = why ? why : "";
}

FxStat takeFarCardsStat()
{
	FcDraw & D = fcDraw();
	FxStat s = D.st;
	s.pipelines = D.pipelines;
	s.buildMs = D.buildMs;
	if ( D.broken && s.failWhy.empty() )
		s.failWhy = D.brokenWhy;
	D.st = FxStat();
	return s;
}
}  // namespace WwVkPresent
