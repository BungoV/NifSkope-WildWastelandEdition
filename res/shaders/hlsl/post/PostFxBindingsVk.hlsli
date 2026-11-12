// Depth of field, motion blur and object blur: NifSkope's shim (lane VKFRAME1 P7, 2026-11-12). The ONE file that binds the
// shared math (DofCommon.hlsli, ObjBlurCommon.hlsli) to a Vulkan descriptor set and push constants; Fusion Shaders keeps its
// own shim and calls the same functions. Set: 0-4 sampled, 5 linear clamp, 6 point clamp, 7 / 8 storage targets.
// A kernel names its targets' formats before the include: PFX_OUT0_FORMAT / PFX_OUT0_TYPE (default "rgba16f" / float4),
// PFX_OUT1 for a second RGBA16F target. register() numbers give DXIL the same slots.
#ifndef POSTFX_BINDINGS_VK_HLSLI
#define POSTFX_BINDINGS_VK_HLSLI

#include "DofCommon.hlsli"
#include "ObjBlurCommon.hlsli"

#ifndef PFX_OUT0_FORMAT
#define PFX_OUT0_FORMAT "rgba16f"
#endif
#ifndef PFX_OUT0_TYPE
#define PFX_OUT0_TYPE float4
#endif

#ifdef __spirv__
#define PFX_BIND( b ) [[vk::binding( b, 0 )]]
#define PFX_FORMAT( f ) [[vk::image_format( f )]]
#else
#define PFX_BIND( b )
#define PFX_FORMAT( f )
#endif

// one dispatch's values (128 bytes); each kernel's header says what c[] and i hold
struct PfxJob
{
	float4 c[7];
	int4 i;
};

PFX_BIND( 0 ) Texture2D<float4> PfxT0 : register( t0 );
PFX_BIND( 1 ) Texture2D<float4> PfxT1 : register( t1 );
PFX_BIND( 2 ) Texture2D<float4> PfxT2 : register( t2 );
PFX_BIND( 3 ) Texture2D<float4> PfxT3 : register( t3 );
PFX_BIND( 4 ) Texture2D<float4> PfxT4 : register( t4 );
PFX_BIND( 5 ) SamplerState PfxLinear : register( s0 );	// linear, clamp to edge, no mips
PFX_BIND( 6 ) SamplerState PfxPoint : register( s1 );	// nearest, clamp to edge, no mips
PFX_BIND( 7 ) PFX_FORMAT( PFX_OUT0_FORMAT ) RWTexture2D<PFX_OUT0_TYPE> PfxOut0 : register( u0 );
#ifdef PFX_OUT1
PFX_BIND( 8 ) PFX_FORMAT( "rgba16f" ) RWTexture2D<float4> PfxOut1 : register( u1 );
#endif
#ifdef __spirv__
[[vk::push_constant]] ConstantBuffer<PfxJob> PfxJobPc;
#else
ConstantBuffer<PfxJob> PfxJobPc : register( b0 );
#endif

// this thread's texel and the target's size; false past the edge
bool PfxTexel( uint3 id, out uint2 size )
{
	uint w, h;
	PfxOut0.GetDimensions( w, h );
	size = uint2( w, h );
	return all( id.xy < size );
}

#endif
