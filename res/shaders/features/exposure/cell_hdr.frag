#version 410 core

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
#include "uniforms.glsl"
#include "cell_lights.glsl"
#include "cell_bloom.glsl"
#include "ww_dither.glsl"

uniform sampler2DMS hdrSrc;
uniform int hdrMode;
uniform usampler2DMS hdrStencil;	// lane TONEMAP1: the frame's stencil, per sample
uniform int hdrStencilTest;	// 0 the GL stencil test picked, 1 keep stencil & 3 == ref, 2 keep it != ref
uniform int hdrStencilRef;
uniform sampler2D hdrBackdrop;	// lane GLAREBLOW2: the view as mode 3 finds it, resolved (cellhdr.cpp backdropCopy)
uniform bool hdrBackdropOn;	// false: the glow as before (red WW_CELL_BLOOM_SKY_RED=add, or the copy refused)
/* lane PERFFIX3 (item 8): the FO4CS bloom's tent once per pixel (cell_bloom_comb.frag, written while its lod is the same
 * at every sample); on: one texelFetch per sample instead of 9-27 bilinear taps. Red WW_CELL_BLOOM_COMB_RED=1: off */
uniform bool cellIsFoBloomCombOn;
uniform sampler2D cellIsFoBloomComb;

out vec4 fragColor;

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
vec3 cellGlowShoulder( vec3 d, vec3 du )
{
	vec3 k = vec3( 1.0 );
	for ( int i = 0; i < 3; i++ ) {
		float u = cellCurveInv( pow( clamp( d[i], 0.0, 1.0 ), 2.2 ) );
		float from0 = pow( max( cellCurveW( du[i] ), 0.0 ), 1.0 / 2.2 );
		float here = pow( max( cellCurveW( u + du[i] ), 0.0 ), 1.0 / 2.2 ) - pow( max( cellCurveW( u ), 0.0 ), 1.0 / 2.2 );
		k[i] = from0 > 1e-6 ? clamp( here / from0, 0.0, 1.0 ) : 1.0;
	}
	return k;
}

void main()
{
	if ( hdrStencilTest != 0 ) {
		uint st = texelFetch( hdrStencil, ivec2( gl_FragCoord.xy ), gl_SampleID ).r & 3u;
		if ( ( st == uint( hdrStencilRef ) ) != ( hdrStencilTest == 1 ) )
			discard;
	}
	vec4 x = texelFetch( hdrSrc, ivec2( gl_FragCoord.xy ), gl_SampleID );
	/* the passes sort the pixels by depth too (the far plane = no opaque draw reached them);
	 * the quad sits at the far plane (cell_hdr.vert), not gl_FragDepth, so the stencil and depth reject before shading */
	// the bloom this pixel receives, raised where its view ray crosses fog
	if ( cellFogBloomOn )
		cellBloomGain = 1.0 + 4.0 * texelFetch( cellFogBloom, ivec2( gl_FragCoord.xy ), gl_SampleID ).r;
	/* the FO4CS bloom at this pixel (cell_bloom.glsl), times the bloom weight; cellImageSpace adds it before
	 * its curve. */
	vec3 foBloom = vec3( 0.0 );
	/* lane GAPLOOK1 (BLOOMMIP): through fog the glow spreads wider (an atmospheric PSF), so the fog's receiver gain
	 * picks how far down the top's mips the tent reads: log2(gain) levels (w = 1; 0 in clear air: the same bits) */
	float bloomLod = cellIsFoBloomParams.w < 0.0 ? 2.0 : cellIsFoBloomParams.w * log2( max( cellBloomGain, 1.0 ) );
	if ( cellIsFoBloomOn && cellIsFoBloomCombOn )
		foBloom = texelFetch( cellIsFoBloomComb, ivec2( gl_FragCoord.xy ), 0 ).rgb * cellIsFoBloomParams.y;
	else if ( cellIsFoBloomOn && bloomLod > 0.0 )
		foBloom = bloomTentDispersedLod( cellIsFoBloom, ( floor( gl_FragCoord.xy ) + 0.5 ) / cellIsFoBloomFrame,
			cellIsFoBloomParams.x, cellIsFoBloomParams.z, bloomLod ) * cellIsFoBloomParams.y;
	else if ( cellIsFoBloomOn )
		foBloom = bloomTentDispersed( cellIsFoBloom, ( floor( gl_FragCoord.xy ) + 0.5 ) / cellIsFoBloomFrame,
			cellIsFoBloomParams.x, cellIsFoBloomParams.z ) * cellIsFoBloomParams.y;
	if ( hdrMode == 3 ) {
		cellFoBloomHere = foBloom;
		vec3 lit = cellImageSpace( vec3( 0.0 ) );
		cellFoBloomHere = vec3( 0.0 );
		vec3 glow = max( lit - cellImageSpace( vec3( 0.0 ) ), vec3( 0.0 ) );
		if ( hdrBackdropOn )	// lane GLAREBLOW2: at the backdrop's place on the curve (cellImageSpace adds 2 x gain x it)
			glow *= cellGlowShoulder( texelFetch( hdrBackdrop, ivec2( gl_FragCoord.xy ), 0 ).rgb, 2.0 * cellBloomGain * foBloom );
		fragColor = vec4( glow, 0.0 );
		return;
	}
	// mode 2 (the effects over the sky) takes the glow from mode 3, as every pixel mode 0 does not take
	cellFoBloomHere = hdrMode == 0 ? foBloom : vec3( 0.0 );
	if ( hdrMode == 2 ) {
		/* a cell effect over the sky: the sky is in the view, not the linear frame, so the effect went over 0.
		 * Un-premultiply by coverage (effects blend alpha as ONE, dst), tone-map, lay over by coverage. */
		float A = clamp( x.a, 0.0, 1.0 );
		vec3 v = max( x.rgb, vec3( 0.0 ) );
		if ( !cellOn || !cellIsOn ) {
			fragColor = vec4( v, A );
			return;
		}
		if ( A > 1.0 / 1024.0 )
			fragColor = vec4( wwDither8( cellImageSpace( sqrt( v / A ) ) * A, gl_FragCoord.xy ), A );	// FOGGLOW1: the sky's air too
		else	// no coverage (an additive glow): its light added to the sky
			fragColor = vec4( max( cellImageSpace( sqrt( v ) ) - cellImageSpace( vec3( 0.0 ) ), vec3( 0.0 ) ), 0.0 );
		return;
	}
	if ( hdrMode != 0 || !cellOn || !cellIsOn ) {
		fragColor = x;
		return;
	}
	// lane FOGGLOW1: dithered before the 8-bit view (ww_dither.glsl; WW_CELL_DITHER=0: as before)
	fragColor = vec4( wwDither8( cellImageSpace( sqrt( max( x.rgb, vec3( 0.0 ) ) ) ), gl_FragCoord.xy ), 1.0 );
}
