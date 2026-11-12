// Modern temporal AA resolve: NifSkope's shim (lane VKFRAME1 P5, 2026-11-12). The ONE file that binds the shared resolve
// (TaaCommon.hlsli) to NifSkope's Vulkan descriptor set and push constants; Fusion Shaders keeps its own shim on its D3D
// slots. The register() numbers give DXIL the same slots, so this shim also compiles to DXIL.
#ifndef TAA_BINDINGS_VK_HLSLI
#define TAA_BINDINGS_VK_HLSLI

#include "TaaCommon.hlsli"

#ifdef __spirv__
#define TAA_BIND( b ) [[vk::binding( b, 0 )]]
#define TAA_FORMAT16 [[vk::image_format( "rgba16f" )]]
#ifdef TAA_PIC_RGBA8	// lane VKSHARE1: the picture into the RGBA8 target GL's display route reads (taa_view_resolve_cs.hlsl)
#define TAA_FORMAT_PIC [[vk::image_format( "rgba8" )]]
#else
#define TAA_FORMAT_PIC TAA_FORMAT16
#endif
#else
#define TAA_BIND( b )
#define TAA_FORMAT16
#define TAA_FORMAT_PIC
#endif

// one dispatch's values (128 bytes): taa2.frag's uniforms
struct TaaJob
{
	float4 sizeJitter;	// invSize, jitterPx
	int4 ints;			// taa2Tier, taa2Red, taa2NoHist, taa2Inputs
	int4 ints2;			// taa2Linear, wwDitherOn, wwDitherSeed, spare
	float4 f;			// taa2Expose, spare
	float4 reproj[4];	// GL's mat4 columns
};

TAA_BIND( 0 ) Texture2D<float4> TaaCur : register( t0 );		// this frame (NEAREST: read by Load)
TAA_BIND( 1 ) Texture2D<float4> TaaHist : register( t1 );		// the history, sampled LINEAR
TAA_BIND( 2 ) Texture2D<float4> TaaMv : register( t2 );		// camera vectors, previous minus current
TAA_BIND( 3 ) Texture2D<float4> TaaDepth : register( t3 );		// the frame's depth (.r)
TAA_BIND( 4 ) Texture2D<float4> TaaLock : register( t4 );		// last frame's o2
TAA_BIND( 5 ) Texture2D<float4> TaaVel : register( t5 );		// TAAINPUTS1 velocity (bound to TaaMv when absent)
TAA_BIND( 6 ) Texture2D<float4> TaaMask : register( t6 );		// TAAINPUTS1 mask (bound to TaaMv when absent)
TAA_BIND( 7 ) SamplerState TaaLinearClamp : register( s0 );	// linear, clamp to edge, no mips
TAA_BIND( 8 ) TAA_FORMAT16 RWTexture2D<float4> TaaOutHist : register( u0 );	// o0
TAA_BIND( 9 ) TAA_FORMAT_PIC RWTexture2D<float4> TaaOutPic : register( u1 );	// o1
TAA_BIND( 10 ) TAA_FORMAT16 RWTexture2D<float4> TaaOutLock : register( u2 );	// o2
#ifdef __spirv__
[[vk::push_constant]] ConstantBuffer<TaaJob> TaaJobPc;
#else
ConstantBuffer<TaaJob> TaaJobPc : register( b0 );
#endif

F4FX_TaaParams TaaParams( TaaJob j )
{
	F4FX_TaaParams p;
	p.invSize = j.sizeJitter.xy;
	p.jitterPx = j.sizeJitter.zw;
	p.tier = j.ints.x;
	p.red = j.ints.y;
	p.noHist = j.ints.z;
	p.inputs = j.ints.w;
	p.linearIn = j.ints2.x;
	p.ditherOn = j.ints2.y;
	p.ditherSeed = j.ints2.z;
	p.expose = j.f.x;
	p.reproj = j.reproj;
	return p;
}

#endif
