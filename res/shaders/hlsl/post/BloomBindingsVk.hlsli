// Bloom pyramid: NifSkope's shim (lane VKFRAME1, 2026-11-12). The ONE file that binds the shared bloom math
// (BloomCommon.hlsli) to NifSkope's Vulkan descriptor set (src/features/vkframe/vkframe.cpp fills it) and push constants.
// Fusion Shaders keeps its own shim (its Post/Common.hlsli constant block on its D3D slots) and calls the same functions.
// The register() numbers give DXIL the same slots (t = sampled, u = storage), so this shim also compiles to DXIL.
#ifndef BLOOM_BINDINGS_VK_HLSLI
#define BLOOM_BINDINGS_VK_HLSLI

#include "BloomCommon.hlsli"

#ifdef __spirv__
#define BLOOM_BIND( b ) [[vk::binding( b, 0 )]]
#define BLOOM_FORMAT [[vk::image_format( "rgba16f" )]]
#else
#define BLOOM_BIND( b )
#define BLOOM_FORMAT
#endif

// one dispatch's values (vkframe.cpp WwVkPost's Job, 48 bytes)
struct BloomJob
{
	float4 params;   // down: threshold, soft knee, prefilter (level 0), scale; up: radius (pixels), source weight, bloom weight, dispersion
	float4 spare;
	int4 ints;       // x: down's red (1 = level 0 a plain box, WW_CELL_BLOOM_RED=nokaris); y: hold's newest layer; z: hold's count
};

BLOOM_BIND( 0 ) Texture2D<float4> BloomSource : register( t0 );         // down: the resolved source or the previous mip; up: this level's band
BLOOM_BIND( 1 ) Texture2D<float4> BloomMip : register( t1 );            // up: the accumulated chain below
BLOOM_BIND( 2 ) Texture2DArray<float4> BloomRing : register( t2 );      // hold: level 0 of the last 8 frames
BLOOM_BIND( 3 ) SamplerState BloomLinearClamp : register( s0 );         // linear, clamp to edge, no mips
BLOOM_BIND( 4 ) BLOOM_FORMAT RWTexture2D<float4> BloomTarget : register( u0 );
#ifdef __spirv__
[[vk::push_constant]] ConstantBuffer<BloomJob> BloomJobPc;
#else
ConstantBuffer<BloomJob> BloomJobPc : register( b0 );
#endif

#ifdef BLOOM_FRAME	// lane VKFRAME1 P2 / P3: the source and the combine (bloom_src.hlsl, bloom_comb.hlsl) only
#ifdef __spirv__
#define BLOOM_FORMAT32 [[vk::image_format( "rgba32f" )]]
#else
#define BLOOM_FORMAT32
#endif
BLOOM_BIND( 5 ) Texture2DMS<float4> BloomFrameColor : register( t3 );  // the linear frame, multisampled (GL draws it, shared)
BLOOM_BIND( 6 ) Texture2DMS<uint> BloomFrameStencil : register( t4 );  // its stencil (a stencil-aspect view of the depth / stencil)
BLOOM_BIND( 7 ) BLOOM_FORMAT32 RWTexture2D<float4> BloomComb : register( u1 );  // the final tent once per pixel (RGBA32F)
#endif

// this thread's texel and the target's size; false past the edge
bool BloomTexel( uint3 id, out uint2 size )
{
	uint w, h;
	BloomTarget.GetDimensions( w, h );
	size = max( uint2( w, h ), uint2( 1, 1 ) );
	return all( id.xy < size );
}

#endif
