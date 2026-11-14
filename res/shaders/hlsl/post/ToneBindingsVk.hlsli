// Cell tone map: NifSkope's shim (lane VKFRAME1 P6, 2026-11-12). The ONE file that binds the shared tone math
// (ToneCommon.hlsli) to NifSkope's Vulkan descriptor set and push constants. Fusion Shaders keeps its own shim on its D3D
// slots. The register() numbers give DXIL the same slots (t = sampled, u = storage), so this shim also compiles to DXIL.
#ifndef TONE_BINDINGS_VK_HLSLI
#define TONE_BINDINGS_VK_HLSLI

#include "ToneCommon.hlsli"
#include "BloomCommon.hlsli"

#ifdef __spirv__
#define TONE_BIND( b ) [[vk::binding( b, 0 )]]
#define TONE_FORMAT32 [[vk::image_format( "rgba32f" )]]
#else
#define TONE_BIND( b )
#define TONE_FORMAT32
#endif

// one dispatch's values (112 bytes): cell_hdr.frag's uniforms
struct ToneJob
{
	float4 cineE;		// cellIsCine (saturation, brightness, contrast), cellIsE
	float4 tint;		// cellIsTint
	float4 wbExposure;	// cellIsWbD, cellIsExposure
	float4 bloomRect;	// cellIsBloomRect: the view's origin, 1 / its size
	float4 fo;			// the Fusion Shaders bloom's weight (cellIsFoBloomParams.y), cellIsFoBloomEc, cellIsFoBloomFogCap, cellIsAdapted
	int4 ints;			// x: flags (TONE_F_*), y: wwDitherOn, z: wwDitherSeed, w: cellIsFoBloomFrame (width | height << 16)
	float4 tent;		// TONE_F_FOBLOOMLOD: cellIsFoBloomParams x (radius), z (dispersion), w (Bloom Mip), 0
};

// ints.x: bits 0-3 cellIsRed, then the switches, then hdrMode (16-17), hdrStencilTest (20-21), hdrStencilRef (24-25)
#define TONE_F_VANCURVE		( 1 << 4 )
#define TONE_F_LUT			( 1 << 5 )
#define TONE_F_MEASURE		( 1 << 6 )	// cellIsBloomOn
#define TONE_F_FOBLOOM		( 1 << 7 )	// cellIsFoBloomOn with its combine texture (the per-pixel tent)
#define TONE_F_FOBLOOMLOD	( 1 << 8 )	// with FOBLOOM: no combine (Bloom Mip under the fog bloom); ToneComb is the top's mips
#define TONE_F_FOGBLOOM		( 1 << 9 )	// cellFogBloomOn
#define TONE_F_CELLON		( 1 << 10 )	// cellOn && cellIsOn
#define TONE_F_BACKDROP		( 1 << 11 )	// hdrBackdropOn

TONE_BIND( 0 ) Texture2DMS<float4> ToneFrame : register( t0 );			// the linear frame (GL draws it, shared)
TONE_BIND( 1 ) Texture2DMS<uint> ToneStencil : register( t1 );			// its stencil (a stencil-aspect view)
TONE_BIND( 2 ) Texture2D<float4> ToneComb : register( t2 );				// the bloom's tent once per pixel (bloom_comb.hlsl), or the top's mips
TONE_BIND( 3 ) Texture2D<float4> ToneMeasureBloom : register( t3 );		// the measure's blurred bright pass
TONE_BIND( 4 ) Texture3D<float4> ToneLut : register( t4 );				// the weather's grading LUT
TONE_BIND( 5 ) Texture2DMS<float4> ToneFogBloom : register( t5 );		// the fog's bloom gain, (gain - 1) / 4 in .r, per sample
TONE_BIND( 6 ) Texture2D<float4> ToneBackdrop : register( t6 );			// the view as mode 3 finds it, resolved
TONE_BIND( 7 ) SamplerState ToneLinearClamp : register( s0 );			// linear, clamp to edge, linear between mips
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

/* lane VKFAR2: the Fusion Shaders bloom per sample times its weight, as cell_hdr.frag reads it when Bloom Mip runs under the
 * fog bloom: the dispersed tent down the top's mips (glGenerateMipmap's levels 0-3) by Bloom Mip x log2 of the sample's fog
 * gain (a negative Bloom Mip: level 2), at the pixel's centre over the frame; lod 0 is the plain tent */
float3 ToneFoBloomLod( ToneJob j, int2 px, uint s )
{
	float gain = 1.0f;
	if ( ( j.ints.x & TONE_F_FOGBLOOM ) != 0 )
		gain = 1.0f + 4.0f * ToneFogBloom.Load( px, s ).r;
	const float lod = j.tent.z < 0.0f ? 2.0f : j.tent.z * log2( max( gain, 1.0f ) );
	uint tw, th, levels;
	ToneComb.GetDimensions( 0, tw, th, levels );
	const float2 texel = 1.0f / float2( max( uint2( tw, th ), uint2( 1, 1 ) ) );
	const float2 frame = float2( float( j.ints.w & 0xFFFF ), float( ( j.ints.w >> 16 ) & 0xFFFF ) );
	const float2 uv = ( float2( px ) + 0.5f ) / frame;
	if ( lod > 0.0f )
		return F4FX_UpsampleTentDispersedLod( ToneComb, ToneLinearClamp, uv, texel, j.tent.x, j.tent.y, lod ) * j.fo.x;
	return F4FX_UpsampleTentDispersed( ToneComb, ToneLinearClamp, uv, texel, j.tent.x, j.tent.y ) * j.fo.x;
}

// one sample's value as cell_hdr.frag writes it; false where its stencil test discards the sample. foBloom: the Fusion
// Shaders bloom at the pixel times its weight. tonemap.hlsl (compute) and tonemap_raster.hlsl (a fragment pass) share it.
bool ToneSample( ToneJob j, F4FX_ImageSpace p, int2 px, uint s, float3 foBloom, out float4 v )
{
	v = float4( 0.0f, 0.0f, 0.0f, 0.0f );
	const int flags = j.ints.x;
	const int mode = ( flags >> 16 ) & 3, test = ( flags >> 20 ) & 3, ref = ( flags >> 24 ) & 3;
	const bool cellOn = ( flags & TONE_F_CELLON ) != 0;
	if ( test != 0 ) {
		uint st = ToneStencil.Load( px, s ) & 3u;
		if ( ( st == uint( ref ) ) != ( test == 1 ) )
			return false;
	}
	const float4 x = ToneFrame.Load( px, s );
	const float2 fc = ToneFragCoord( uint2( px ) );
	float gain = 1.0f;
	if ( ( flags & TONE_F_FOGBLOOM ) != 0 )
		gain = 1.0f + 4.0f * ToneFogBloom.Load( px, s ).r;
	const bool measureOn = ( flags & TONE_F_MEASURE ) != 0;
	float3 mb = float3( 0.0f, 0.0f, 0.0f );
	if ( measureOn )
		mb = ToneMeasureBloom.SampleLevel( ToneLinearClamp, ( fc - j.bloomRect.xy ) * j.bloomRect.zw, 0.0f ).rgb;
	const float3 zero = float3( 0.0f, 0.0f, 0.0f );
	if ( mode == 3 ) {
		float3 lit = F4FX_CellImageSpace( zero, measureOn, mb, gain, foBloom, p, ToneLut, ToneLinearClamp );
		float3 glow = max( lit - F4FX_CellImageSpace( zero, measureOn, mb, gain, zero, p, ToneLut, ToneLinearClamp ), zero );
		if ( ( flags & TONE_F_BACKDROP ) != 0 )
			glow *= F4FX_CellGlowShoulder( ToneBackdrop.Load( int3( px, 0 ) ).rgb, 2.0f * gain * foBloom, p );
		v = float4( glow, 0.0f );
		return true;
	}
	const float3 foHere = mode == 0 ? foBloom : zero;
	if ( mode == 2 ) {
		float A = clamp( x.a, 0.0f, 1.0f );
		float3 c = max( x.rgb, zero );
		if ( !cellOn )
			v = float4( c, A );
		else if ( A > 1.0f / 1024.0f )
			v = float4( F4FX_Dither8( F4FX_CellImageSpace( sqrt( c / A ), measureOn, mb, gain, foHere, p, ToneLut, ToneLinearClamp ) * A,
				uint2( fc ), j.ints.y, j.ints.z ), A );
		else
			v = float4( max( F4FX_CellImageSpace( sqrt( c ), measureOn, mb, gain, foHere, p, ToneLut, ToneLinearClamp )
				- F4FX_CellImageSpace( zero, measureOn, mb, gain, foHere, p, ToneLut, ToneLinearClamp ), zero ), 0.0f );
		return true;
	}
	if ( mode != 0 || !cellOn ) {
		v = x;
		return true;
	}
	v = float4( F4FX_Dither8( F4FX_CellImageSpace( sqrt( max( x.rgb, zero ) ), measureOn, mb, gain, foHere, p, ToneLut,
		ToneLinearClamp ), uint2( fc ), j.ints.y, j.ints.z ), 1.0f );
	return true;
}

#endif
