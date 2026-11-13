// HLSL source of features/exposure/cell_hdr.frag (fragment shader), lane HLSLSRC2 (notes/hlslsrc2/tools/glsl2hlsl.py): one readable file for every
// variant, the GLSL's comments, names and order kept. Loose uniforms are the LooseUniforms cbuffer (b31).
// variants: #if WW_CELL_RESTIR, WW_CELL_RESTIRGI, WW_CELL_RTDDGI, WW_CELL_RTDDGI_CASCADES (the loader's defines; res/shaders/hlsl/variants.txt lists the sets built)
// profile: ps_6_0
#include "ExposureBindingsVk.hlsli"
#define WW_HLSL_SRC 1	// the shared .hlsli files give their readable half
// inlined (no readable .hlsli yet): uniforms.glsl, ww_dither.glsl
static float4 gl_FragCoord;
static int gl_SampleID;
/* The cell view's tone map (src/features/exposure/cellhdr.h): draws the linear float frame back into the view, per sample.
 * The stencil picks the mode per pixel (copied back, or read here through hdrStencilTest):
 *   hdrMode 0  pixels a cell program or effect wrote last: the cell's imagespace on the summed linear light
 *   hdrMode 1  the pixels any other program wrote last: the value as it was written
 *   hdrMode 2  mode-0 pixels no opaque draw reached: a cell effect composited over the sky by its coverage
 *   hdrMode 3  (FO4CS bloom only) every pixel mode 0 does not take: the bloom's glow added in display space,
 *              at the backdrop's own place on the curve (cellGlowShoulder) */

#define WW_CELLLIGHTS 1
#define WW_CELL_FX 1
#define WW_CELL_PBR 1
#define WW_CELL_HDR 1
// uniforms.glsl inlined here: its .hlsli has no readable half yet (lane HLSLSRC1)
cbuffer globalUniforms : register(b0, space0)
{
	row_major float3x3	viewMatrix : packoffset(c0);					// rotation part of view transform
	row_major float3x3	envMapRotation : packoffset(c3);				// view space to environment map
	row_major float4x4	projectionMatrix : packoffset(c6);
	float4	lightSourcePosition[3] : packoffset(c10);
	float4	lightSourceDiffuse[3] : packoffset(c13);
	float4	lightSourceAmbient : packoffset(c16);
	float	toneMapScale : packoffset(c17);				// 1.0 = full tone mapping
	float	brightnessScale : packoffset(c17.y);
	float	glowScale : packoffset(c17.z);
	float	glowScaleSRGB : packoffset(c17.w);
	int4	viewportDimensions : packoffset(c18);			// X, Y, width, height
	uint	doSkinning : packoffset(c19);
	int	sceneOptions : packoffset(c19.y);
	int	cubeBgndMipLevel : packoffset(c19.z);
	int	sfParallaxMaxSteps : packoffset(c19.w);
	float	sfParallaxScale : packoffset(c20);
	float	sfParallaxOffset : packoffset(c20.y);
	float	unusedUniform1 : packoffset(c20.z);
	float	unusedUniform2 : packoffset(c20.w);
};
// end of uniforms.glsl
#define WW_LOOSE_TYPES 1
#ifndef WW_CELL_FX
#define WW_CELL_FX 1	// as below, before this file
#endif
#ifndef WW_CELL_PBR
#define WW_CELL_PBR 1	// as below, before this file
#endif
#ifndef WW_CELL_HDR
#define WW_CELL_HDR 1	// as below, before this file
#endif
#include "../lights/cell_lights.hlsli"
#include "../lights/cell_cluster.hlsli"
#if defined(WW_DEBUG_VIEWS)
#include "../overlays/debug_view.hlsli"
#endif
#if defined(WW_CELL_PRT) && !(defined(WW_CELL_GI_MARCH))
#include "../ao/cell_gihalfat.hlsli"
#endif
#if defined(WW_CELL_PRT) && !(defined(WW_CELL_GI_MARCH)) && defined(WW_CELL_RTDDGI)
#include "../gi/cell_ddgi.hlsli"
#endif
#if !(defined(WW_CELL_PRT)) && defined(WW_CELL_RTDDGI)
#include "../gi/cell_ddgi.hlsli"
#include "../ao/cell_gihalfat.hlsli"
#endif
#undef WW_LOOSE_TYPES
cbuffer LooseUniforms : register(b31, space0)
{
#define WW_LOOSE_UNIFORMS 1
#ifndef WW_CELL_FX
#define WW_CELL_FX 1	// as below, before this file
#endif
#ifndef WW_CELL_PBR
#define WW_CELL_PBR 1	// as below, before this file
#endif
#ifndef WW_CELL_HDR
#define WW_CELL_HDR 1	// as below, before this file
#endif
#include "../lights/cell_lights.hlsli"
#include "../lights/cell_cluster.hlsli"
#if defined(WW_DEBUG_VIEWS)
#include "../overlays/debug_view.hlsli"
#endif
#if defined(WW_CELL_PRT) && !(defined(WW_CELL_GI_MARCH))
#include "../ao/cell_gihalfat.hlsli"
#endif
#if defined(WW_CELL_PRT) && !(defined(WW_CELL_GI_MARCH)) && defined(WW_CELL_RTDDGI)
#include "../gi/cell_ddgi.hlsli"
#endif
#if !(defined(WW_CELL_PRT)) && defined(WW_CELL_RTDDGI)
#include "../gi/cell_ddgi.hlsli"
#include "../ao/cell_gihalfat.hlsli"
#endif
#undef WW_LOOSE_UNIFORMS
	int wwDitherOn;
	int wwDitherSeed;
	int hdrMode;
	int hdrStencilTest;	// 0 the GL stencil test picked, 1 keep stencil & 3 == ref, 2 keep it != ref
	int hdrStencilRef;
	bool hdrBackdropOn;	// false: the glow as before (red WW_CELL_BLOOM_SKY_RED=add, or the copy refused)
	/* lane PERFFIX3 (item 8): the FO4CS bloom's tent once per pixel (cell_bloom_comb.frag, written while its lod is the same
	 * at every sample); on: one texelFetch per sample instead of 9-27 bilinear taps. Red WW_CELL_BLOOM_COMB_RED=1: off */
	bool cellIsFoBloomCombOn;
};
#include "../lights/cell_lights.hlsli"
#include "../post/cell_bloom.hlsli"
// ww_dither.glsl inlined here: its .hlsli has no readable half yet (lane HLSLSRC1)
/* lane FOGGLOW1 (bungo 10-29, Memory Den fog banding): a display value goes into an 8-bit target (the view, the TAA
 * picture) as one of 256 levels. A smooth dark gradient (the night fog on a wall, the glow round a lamp) then steps in
 * flat plateaus a level apart: the bands and rings. A triangular (TPDF) noise of +-1 level added before the write
 * turns the steps into a fine grain whose mean is the true value (Gjol, "Banding in Games: A Noisy Rant", Playdead,
 * 2016). The noise fades to 0 at exactly 0 and 1, so black stays black and white white.
 * wwDitherOn 0 (WW_CELL_DITHER=0, the red): the value unchanged. wwDitherSeed: the TAA frame index (a camera path
 * renders frame k as k: a repeated frame is the same picture), so TAA averages a different noise every frame. */



// a uniform [0, 1) from a pixel and a seed (an integer hash: no lattice, no pattern a zoom shows)
float wwDitherHash( uint2 p, uint s )
{
	uint h = p.x * 1664525u + p.y * 1013904223u + s * 2654435769u;
	h ^= h >> 16;
	h *= 2246822519u;
	h ^= h >> 13;
	h *= 3266489917u;
	h ^= h >> 16;
	return float( h >> 8 ) * ( 1.0 / 16777216.0 );
}

float3 wwDither8( float3 c, float2 fragCoord )
{
	if ( wwDitherOn == 0 )
		return c;
	uint2 p = uint2( fragCoord );
	uint s = uint( wwDitherSeed );
	float n = wwDitherHash( p, 2u * s ) + wwDitherHash( p, 2u * s + 1u ) - 1.0;	// triangular on (-1, 1)
	float3 w = clamp( min( c, 1.0 - c ) * 255.0, 0.0, 1.0 );
	return c + n * w * ( 1.0 / 255.0 );
}
// end of ww_dither.glsl

Texture2DMS<float4> hdrSrc : register(t46, space0);
Texture2DMS<uint4> hdrStencil : register(t47, space0);	// lane TONEMAP1: the frame's stencil, per sample
Texture2D<float4> hdrBackdrop : register(t48, space0); SamplerState _hdrBackdrop_sampler : register(s48, space0);	// lane GLAREBLOW2: the view as mode 3 finds it, resolved (cellhdr.cpp backdropCopy)
Texture2D<float4> cellIsFoBloomComb : register(t49, space0); SamplerState _cellIsFoBloomComb_sampler : register(s49, space0);

static float4 fragColor;

/* lane GLAREBLOW2: cellImageSpace's curve (its input: 2 x the exposed light; out: display-linear, before the
 * 1 / 2.2) and the curve's inverse, closed form (y' (0.15 x^2 + 0.5 x + 0.06) = 0.15 x^2 + 0.05 x + 0.2 E).
 * lane FIX1101A T: the Physical curve's grey axis and its inverse (cell_tonemap.glsl); the filmic one is the red. */
float cellCurveW( float x )
{
	if ( !cellIsVanCurve )
		return wwTmGrey( 0.5 * x );
	float E = cellIsE;
	float c = ( x * ( 0.15 * x + 0.05 ) + 0.2 * E ) / ( x * ( 0.15 * x + 0.5 ) + 0.06 ) - E / 0.3;
	return c / ( ( 0.2 * E + 19.376 ) * 0.040856 - E / 0.3 );
}

float cellCurveInv( float y )
{
	if ( !cellIsVanCurve )
		return 2.0 * wwTmGreyInv( y );
	float E = cellIsE;
	float yp = y * ( ( 0.2 * E + 19.376 ) * 0.040856 - E / 0.3 ) + E / 0.3;
	float a = 0.15 * ( yp - 1.0 ), b = 0.5 * yp - 0.05, c = 0.06 * yp - 0.2 * E;
	if ( abs( a ) < 1e-7 )
		return max( -c / b, 0.0 );
	return max( ( -b - sqrt( max( b * b - 4.0 * a * c, 0.0 ) ) ) / ( 2.0 * a ), 0.0 );
}

/* lane GLAREBLOW2: the share of the glow's response from black (mode 3's IS(bloom) - IS(0)) that the curve gives at
 * the backdrop's own place on it, per channel: C(u + du) - C(u) over C(du) - C(0), C = the curve ^ 1 / 2.2, u the
 * curve input that shows the backdrop d. 1 over black (the glow as before), toward 0 as d nears white: FO4CS runs
 * the sky through the curve WITH its bloom (IS(sky + b)); adding IS(b) - IS(0) after it took a sky pixel at 0.85 past
 * white from a glow of 0.02 (tools of lane GLAREBLOW2: composite_model.py). The grade and the LUT are left out of
 * the ratio (the glow it scales keeps them). */
float3 cellGlowShoulder( float3 d, float3 du )
{
	float3 k = (float3)( 1.0 );
	for ( int i = 0; i < 3; i++ ) {
		float u = cellCurveInv( pow( clamp( d[i], 0.0, 1.0 ), 2.2 ) );
		float from0 = pow( max( cellCurveW( du[i] ), 0.0 ), 1.0 / 2.2 );
		float here = pow( max( cellCurveW( u + du[i] ), 0.0 ), 1.0 / 2.2 ) - pow( max( cellCurveW( u ), 0.0 ), 1.0 / 2.2 );
		k[i] = from0 > 1e-6 ? clamp( here / from0, 0.0, 1.0 ) : 1.0;
	}
	return k;
}

void frag_main()
{
	if ( hdrStencilTest != 0 ) {
		uint st = hdrStencil.Load( int2( gl_FragCoord.xy ), gl_SampleID ).r & 3u;
		if ( ( st == uint( hdrStencilRef ) ) != ( hdrStencilTest == 1 ) )
			discard;
	}
	float4 x = hdrSrc.Load( int2( gl_FragCoord.xy ), gl_SampleID );
	/* the passes sort the pixels by depth too (the far plane = no opaque draw reached them);
	 * the quad sits at the far plane (cell_hdr.vert), not gl_FragDepth, so the stencil and depth reject before shading */
	// the bloom this pixel receives, raised where its view ray crosses fog
	if ( cellFogBloomOn )
		cellBloomGain = 1.0 + 4.0 * cellFogBloom.Load( int2( gl_FragCoord.xy ), gl_SampleID ).r;
	/* the FO4CS bloom at this pixel (cell_bloom.glsl), times the bloom weight; cellImageSpace adds it before
	 * its curve. */
	float3 foBloom = (float3)( 0.0 );
	/* lane GAPLOOK1 (BLOOMMIP): through fog the glow spreads wider (an atmospheric PSF), so the fog's receiver gain
	 * picks how far down the top's mips the tent reads: log2(gain) levels (w = 1; 0 in clear air: the same bits) */
	float bloomLod = cellIsFoBloomParams.w < 0.0 ? 2.0 : cellIsFoBloomParams.w * log2( max( cellBloomGain, 1.0 ) );
	if ( cellIsFoBloomOn && cellIsFoBloomCombOn )
		foBloom = cellIsFoBloomComb.Load( int3(int2( gl_FragCoord.xy ), 0) ).rgb * cellIsFoBloomParams.y;
	else if ( cellIsFoBloomOn && bloomLod > 0.0 )
		foBloom = bloomTentDispersedLod( cellIsFoBloom, _cellIsFoBloom_sampler, ( floor( gl_FragCoord.xy ) + 0.5 ) / cellIsFoBloomFrame,
			cellIsFoBloomParams.x, cellIsFoBloomParams.z, bloomLod ) * cellIsFoBloomParams.y;
	else if ( cellIsFoBloomOn )
		foBloom = bloomTentDispersed( cellIsFoBloom, _cellIsFoBloom_sampler, ( floor( gl_FragCoord.xy ) + 0.5 ) / cellIsFoBloomFrame,
			cellIsFoBloomParams.x, cellIsFoBloomParams.z ) * cellIsFoBloomParams.y;
	if ( hdrMode == 3 ) {
		cellFoBloomHere = foBloom;
		float3 lit = cellImageSpace( (float3)( 0.0 ) );
		cellFoBloomHere = (float3)( 0.0 );
		float3 glow = max( lit - cellImageSpace( (float3)( 0.0 ) ), (float3)( 0.0 ) );
		if ( hdrBackdropOn )	// lane GLAREBLOW2: at the backdrop's place on the curve (cellImageSpace adds 2 x gain x it)
			glow *= cellGlowShoulder( hdrBackdrop.Load( int3(int2( gl_FragCoord.xy ), 0) ).rgb, 2.0 * cellBloomGain * foBloom );
		fragColor = float4( glow, 0.0 );
		return;
	}
	// mode 2 (the effects over the sky) takes the glow from mode 3, as every pixel mode 0 does not take
	cellFoBloomHere = hdrMode == 0 ? foBloom : (float3)( 0.0 );
	if ( hdrMode == 2 ) {
		/* a cell effect over the sky: the sky is in the view, not the linear frame, so the effect went over 0.
		 * Un-premultiply by coverage (effects blend alpha as ONE, dst), tone-map, lay over by coverage. */
		float A = clamp( x.a, 0.0, 1.0 );
		float3 v = max( x.rgb, (float3)( 0.0 ) );
		if ( or( !cellOn, !cellIsOn ) ) {
			fragColor = float4( v, A );
			return;
		}
		if ( A > 1.0 / 1024.0 )
			fragColor = float4( wwDither8( cellImageSpace( sqrt( v / A ) ) * A, gl_FragCoord.xy ), A );	// FOGGLOW1: the sky's air too
		else	// no coverage (an additive glow): its light added to the sky
			fragColor = float4( max( cellImageSpace( sqrt( v ) ) - cellImageSpace( (float3)( 0.0 ) ), (float3)( 0.0 ) ), 0.0 );
		return;
	}
	if ( or( or( hdrMode != 0, !cellOn ), !cellIsOn ) ) {
		fragColor = x;
		return;
	}
	// lane FOGGLOW1: dithered before the 8-bit view (ww_dither.glsl; WW_CELL_DITHER=0: as before)
	fragColor = float4( wwDither8( cellImageSpace( sqrt( max( x.rgb, (float3)( 0.0 ) ) ) ), gl_FragCoord.xy ), 1.0 );
}

struct SPIRV_Cross_Input
{
    float4 gl_FragCoord : SV_Position;
    uint gl_SampleID : SV_SampleIndex;
};

struct SPIRV_Cross_Output
{
    float4 fragColor : SV_Target0;
};

SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    gl_FragCoord = stage_input.gl_FragCoord;
    gl_FragCoord.w = 1.0 / gl_FragCoord.w;
    gl_SampleID = int(stage_input.gl_SampleID);
    frag_main();
    SPIRV_Cross_Output stage_output;
    stage_output.fragColor = fragColor;
    return stage_output;
}
