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

// a kernel's stores; PFX_RASTER (tests/vkframe/postfx.sh's raster arm) builds the same kernel as a fragment pass: its main
// becomes PfxKernel, called once a pixel by the entry ps, and the stores go to SV_Target0 / 1 (the way GL draws them)
#ifdef PFX_RASTER
static PFX_OUT0_TYPE PfxStore0;
static float4 PfxStore1;
#define PFX_STORE0( p, v ) PfxStore0 = ( v )
#define PFX_STORE1( p, v ) PfxStore1 = ( v )
#define main PfxKernel
void PfxKernel( uint3 id );

struct PfxPsOut
{
	PFX_OUT0_TYPE o0 : SV_Target0;
#ifdef PFX_OUT1
	float4 o1 : SV_Target1;
#endif
};

float4 vs( uint v : SV_VertexID ) : SV_Position
{
	const float2 p = float2( ( v << 1 ) & 2, v & 2 );
	return float4( p * 2.0f - 1.0f, 0.0f, 1.0f );
}

PfxPsOut ps( float4 pos : SV_Position )
{
	PfxKernel( uint3( uint2( pos.xy ), 0 ) );
	PfxPsOut o;
	o.o0 = PfxStore0;
#ifdef PFX_OUT1
	o.o1 = PfxStore1;
#endif
	return o;
}
#else
#define PFX_STORE0( p, v ) PfxOut0[p] = ( v )
#define PFX_STORE1( p, v ) PfxOut1[p] = ( v )
#endif

#endif
