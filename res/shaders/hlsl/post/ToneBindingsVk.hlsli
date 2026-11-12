// Cell tone map: NifSkope's shim (lane VKFRAME1 P6, 2026-11-12). The ONE file that binds the shared tone math
// (ToneCommon.hlsli) to NifSkope's Vulkan descriptor set and push constants. Fusion Shaders keeps its own shim on its D3D
// slots. The register() numbers give DXIL the same slots (t = sampled, u = storage), so this shim also compiles to DXIL.
#ifndef TONE_BINDINGS_VK_HLSLI
#define TONE_BINDINGS_VK_HLSLI

#include "ToneCommon.hlsli"

#ifdef __spirv__
#define TONE_BIND( b ) [[vk::binding( b, 0 )]]
#define TONE_FORMAT32 [[vk::image_format( "rgba32f" )]]
#else
#define TONE_BIND( b )
#define TONE_FORMAT32
#endif

// one dispatch's values (96 bytes): cell_hdr.frag's uniforms
struct ToneJob
{
	float4 cineE;		// cellIsCine (saturation, brightness, contrast), cellIsE
	float4 tint;		// cellIsTint
	float4 wbExposure;	// cellIsWbD, cellIsExposure
	float4 bloomRect;	// cellIsBloomRect: the view's origin, 1 / its size
	float4 fo;			// the Fusion Shaders bloom's weight (cellIsFoBloomParams.y), cellIsFoBloomEc, cellIsFoBloomFogCap, cellIsAdapted
	int4 ints;			// x: flags (TONE_F_*), y: wwDitherOn, z: wwDitherSeed, w: spare
};

// ints.x: bits 0-3 cellIsRed, then the switches, then hdrMode (16-17), hdrStencilTest (20-21), hdrStencilRef (24-25)
#define TONE_F_VANCURVE		( 1 << 4 )
#define TONE_F_LUT			( 1 << 5 )
#define TONE_F_MEASURE		( 1 << 6 )	// cellIsBloomOn
#define TONE_F_FOBLOOM		( 1 << 7 )	// cellIsFoBloomOn with its combine texture (the per-pixel tent)
#define TONE_F_FOGBLOOM		( 1 << 9 )	// cellFogBloomOn
#define TONE_F_CELLON		( 1 << 10 )	// cellOn && cellIsOn
#define TONE_F_BACKDROP		( 1 << 11 )	// hdrBackdropOn

TONE_BIND( 0 ) Texture2DMS<float4> ToneFrame : register( t0 );			// the linear frame (GL draws it, shared)
TONE_BIND( 1 ) Texture2DMS<uint> ToneStencil : register( t1 );			// its stencil (a stencil-aspect view)
TONE_BIND( 2 ) Texture2D<float4> ToneComb : register( t2 );				// the bloom's tent once per pixel (bloom_comb.hlsl)
TONE_BIND( 3 ) Texture2D<float4> ToneMeasureBloom : register( t3 );		// the measure's blurred bright pass
TONE_BIND( 4 ) Texture3D<float4> ToneLut : register( t4 );				// the weather's grading LUT
TONE_BIND( 5 ) Texture2DMS<float4> ToneFogBloom : register( t5 );		// the fog's bloom gain, (gain - 1) / 4 in .r, per sample
TONE_BIND( 6 ) Texture2D<float4> ToneBackdrop : register( t6 );			// the view as mode 3 finds it, resolved
TONE_BIND( 7 ) SamplerState ToneLinearClamp : register( s0 );			// linear, clamp to edge, no mips
TONE_BIND( 8 ) TONE_FORMAT32 RWTexture2D<float4> ToneOut : register( u0 );	// per sample: column x * samples + s
#ifdef __spirv__
[[vk::push_constant]] ConstantBuffer<ToneJob> ToneJobPc;
#else
ConstantBuffer<ToneJob> ToneJobPc : register( b0 );
#endif

F4FX_ImageSpace ToneImageSpace( ToneJob j )
{
	F4FX_ImageSpace p;
	p.exposure = j.wbExposure.w;
	p.E = j.cineE.w;
	p.adapted = j.fo.w;
	p.cine = j.cineE.xyz;
	p.tint = j.tint;
	p.wbD = j.wbExposure.xyz;
	p.red = j.ints.x & 15;
	p.vanCurve = ( j.ints.x & TONE_F_VANCURVE ) != 0;
	p.lutOn = ( j.ints.x & TONE_F_LUT ) != 0;
	p.foOn = ( j.ints.x & TONE_F_FOBLOOM ) != 0;
	p.foEc = j.fo.y;
	p.foFogCap = j.fo.z;
	return p;
}

// gl_FragCoord.xy: the pixel's centre at every sample (measured on the GL driver: per-sample shading leaves it there;
// the sample's own place in the pixel gave 70 levels off where the measure's bloom is sampled)
float2 ToneFragCoord( uint2 px )
{
	return float2( px ) + 0.5f;
}

#endif
