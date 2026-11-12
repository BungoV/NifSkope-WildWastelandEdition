// Lighting passes on Vulkan compute: NifSkope's shim (lane VKLIGHT1, 2026-11-12). The ONE file that binds the shared math
// (AoCommon.hlsli, ...) to a Vulkan descriptor set and push constants; Fusion Shaders keeps its own shim and calls the same
// functions. Same layout as VKFRAME1's PostFxBindingsVk.hlsli (tests/vklight/lightmatch.cpp runs both):
// set 0: 0-4 sampled, 5 linear clamp, 6 point clamp, 7 / 8 storage targets; a 128-byte push block.
// A kernel names its target's format before the include: LPX_OUT0_FORMAT / LPX_OUT0_TYPE (default "rgba32f" / float4).
// register() numbers give DXIL the same slots.
#ifndef LIGHT_BINDINGS_VK_HLSLI
#define LIGHT_BINDINGS_VK_HLSLI

#ifndef LPX_OUT0_FORMAT
#define LPX_OUT0_FORMAT "rgba32f"
#endif
#ifndef LPX_OUT0_TYPE
#define LPX_OUT0_TYPE float4
#endif

#ifdef __spirv__
#define LPX_BIND( b ) [[vk::binding( b, 0 )]]
#define LPX_FORMAT( f ) [[vk::image_format( f )]]
#else
#define LPX_BIND( b )
#define LPX_FORMAT( f )
#endif

// one dispatch's values (128 bytes); each kernel's header says what c[] and i hold
struct LpxJob
{
	float4 c[7];
	int4 i;
};

LPX_BIND( 0 ) Texture2D<float4> LpxT0 : register( t0 );
LPX_BIND( 1 ) Texture2D<float4> LpxT1 : register( t1 );
LPX_BIND( 2 ) Texture2D<float4> LpxT2 : register( t2 );
LPX_BIND( 3 ) Texture2D<float4> LpxT3 : register( t3 );
LPX_BIND( 4 ) Texture2D<float4> LpxT4 : register( t4 );
LPX_BIND( 5 ) SamplerState LpxLinear : register( s0 );	// linear, clamp to edge, no mips
LPX_BIND( 6 ) SamplerState LpxPoint : register( s1 );	// nearest, clamp to edge, no mips
LPX_BIND( 7 ) LPX_FORMAT( LPX_OUT0_FORMAT ) RWTexture2D<LPX_OUT0_TYPE> LpxOut0 : register( u0 );
#ifdef LPX_OUT1_FORMAT
LPX_BIND( 8 ) LPX_FORMAT( LPX_OUT1_FORMAT ) RWTexture2D<LPX_OUT1_TYPE> LpxOut1 : register( u1 );
#endif
#ifdef __spirv__
[[vk::push_constant]] ConstantBuffer<LpxJob> LpxJobPc;
#else
ConstantBuffer<LpxJob> LpxJobPc : register( b0 );
#endif

// this thread's texel and the target's size; false past the edge
bool LpxTexel( uint3 id, out uint2 size )
{
	uint w, h;
	LpxOut0.GetDimensions( w, h );
	size = uint2( w, h );
	return all( id.xy < size );
}

// texelFetch( t, p, 0 )
#define LPX_FETCH( t, p ) ( t ).Load( int3( ( p ), 0 ) )

#endif
