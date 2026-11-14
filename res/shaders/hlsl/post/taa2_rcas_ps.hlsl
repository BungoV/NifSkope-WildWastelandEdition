// HLSL source of features/post/taa2_rcas.frag (fragment shader), lane HLSLSRC2 (notes/hlslsrc2/tools/glsl2hlsl.py): one readable file for every
// variant, the GLSL's comments, names and order kept. Loose uniforms are the LooseUniforms cbuffer (b31).
// variants: #if none (the loader's defines; res/shaders/hlsl/variants.txt lists the sets built)
// profile: ps_6_0
#include "PostBindingsVk.hlsli"
#define WW_HLSL_SRC 1	// the shared .hlsli files give their readable half
#ifndef WW_GLSL_SIZE_Texture2D_float4_
#define WW_GLSL_SIZE_Texture2D_float4_
int2 textureSize(Texture2D<float4> t, int lod) { uint w, h, l; t.GetDimensions(uint(lod), w, h, l); return int2(w, h); }
#endif
static float4 gl_FragCoord;
/* lane TAA2: AMD FidelityFX RCAS (robust contrast-adaptive sharpening, FSR 1), ported to GLSL for the Modern
 * temporal AA's High and Ultra tiers in place of the game's PostSharpen/PostOverlay pair. Writes the frame.
 *
 * Ported from FidelityFX FSR 1 (ffx_fsr1.h, FsrRcasF), github.com/GPUOpen-Effects/FidelityFX-FSR, MIT licence:
 * Copyright (c) 2021 Advanced Micro Devices, Inc. All rights reserved.
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated
 * documentation files (the "Software"), to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to the following conditions:
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of
 * the Software.
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
 * WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */

Texture2D<float4> srcTex : register(t0, space0); SamplerState _srcTex_sampler : register(s0, space0);
cbuffer LooseUniforms : register(b31, space0)
{
	float rcasSharp;	// exp2(-stops): 1 = the strongest
	int rcasLinear;	// lane POSTFX3: 1 the source is the linear (HDR) frame: sharpen in c / (1 + max(c)), write linear
	float rcasExpose;	// lane POSTFX3: the linear frame's scale to about display level
};


static float4 fragColor;

static const float kRcasLimit = 0.25 - 1.0 / 16.0;

float3 tap( int2 q )
{
	float3 c = srcTex.Load( int3(q, 0) ).rgb;
	if ( rcasLinear == 0 )
		return c;
	c = max( c, (float3)( 0.0 ) ) * rcasExpose;
	return c / ( 1.0 + max( c.r, max( c.g, c.b ) ) );
}

void frag_main()
{
	int2 size = textureSize( srcTex, 0 );
	int2 p = int2( gl_FragCoord.xy );
	//    b
	//  d e f
	//    h
	float3 b = tap( clamp( p + int2( 0, 1 ), (int2)( 0 ), size - 1 ) );
	float3 d = tap( clamp( p + int2( -1, 0 ), (int2)( 0 ), size - 1 ) );
	float3 e = tap( p );
	float3 f = tap( clamp( p + int2( 1, 0 ), (int2)( 0 ), size - 1 ) );
	float3 h = tap( clamp( p + int2( 0, -1 ), (int2)( 0 ), size - 1 ) );
	// luma times 2 for the noise test
	float bL = b.b * 0.5 + ( b.r * 0.5 + b.g ), dL = d.b * 0.5 + ( d.r * 0.5 + d.g ), eL = e.b * 0.5 + ( e.r * 0.5 + e.g );
	float fL = f.b * 0.5 + ( f.r * 0.5 + f.g ), hL = h.b * 0.5 + ( h.r * 0.5 + h.g );
	float nz = 0.25 * ( bL + dL + fL + hL ) - eL;
	float rangeL = max( max( max( bL, dL ), max( eL, fL ) ), hL ) - min( min( min( bL, dL ), min( eL, fL ) ), hL );
	nz = clamp( abs( nz ) / max( rangeL, 1e-6 ), 0.0, 1.0 );
	nz = -0.5 * nz + 1.0;
	// the ring's min/max, and the most negative lobe that keeps the result inside it
	float3 mn4 = min( min( b, d ), min( f, h ) );
	float3 mx4 = max( max( b, d ), max( f, h ) );
	float2 peakC = float2( 1.0, -4.0 );
	float3 hitMin = min( mn4, e ) / max( 4.0 * mx4, (float3)( 1e-6 ) );
	float3 hitMax = ( peakC.x - max( mx4, e ) ) / min( 4.0 * mn4 + peakC.y, (float3)( -1e-6 ) );
	float3 lobeRGB = max( -hitMin, hitMax );
	float lobe = max( -kRcasLimit, min( max( lobeRGB.r, max( lobeRGB.g, lobeRGB.b ) ), 0.0 ) ) * rcasSharp;
	lobe *= nz;
	float rcpL = 1.0 / ( 4.0 * lobe + 1.0 );
	float3 o = clamp( ( lobe * ( b + d + f + h ) + e ) * rcpL, 0.0, 1.0 );
	if ( rcasLinear != 0 ) {
		o = min( o, (float3)( 0.999 ) );
		o = o / ( ( 1.0 - max( o.r, max( o.g, o.b ) ) ) * max( rcasExpose, 1e-6 ) );
	}
	fragColor = float4( o, 1.0 );
}

struct SPIRV_Cross_Input
{
    float4 gl_FragCoord : SV_Position;
};

struct SPIRV_Cross_Output
{
    float4 fragColor : SV_Target0;
};

SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    gl_FragCoord = stage_input.gl_FragCoord;
    gl_FragCoord.w = 1.0 / gl_FragCoord.w;
    frag_main();
    SPIRV_Cross_Output stage_output;
    stage_output.fragColor = fragColor;
    return stage_output;
}
