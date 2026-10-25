#version 410 core

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

uniform sampler2D srcTex;
uniform float rcasSharp;	// exp2(-stops): 1 = the strongest
uniform int rcasLinear;	// lane TAA3: 1 the source is the linear (HDR) frame: sharpen in c / (1 + max(c)), write linear
uniform float rcasExpose;	// lane TAA3: the linear frame's scale to about display level

out vec4 fragColor;

const float kRcasLimit = 0.25 - 1.0 / 16.0;

vec3 tap( ivec2 q )
{
	vec3 c = texelFetch( srcTex, q, 0 ).rgb;
	if ( rcasLinear == 0 )
		return c;
	c = max( c, vec3( 0.0 ) ) * rcasExpose;
	return c / ( 1.0 + max( c.r, max( c.g, c.b ) ) );
}

void main()
{
	ivec2 size = textureSize( srcTex, 0 );
	ivec2 p = ivec2( gl_FragCoord.xy );
	//    b
	//  d e f
	//    h
	vec3 b = tap( clamp( p + ivec2( 0, 1 ), ivec2( 0 ), size - 1 ) );
	vec3 d = tap( clamp( p + ivec2( -1, 0 ), ivec2( 0 ), size - 1 ) );
	vec3 e = tap( p );
	vec3 f = tap( clamp( p + ivec2( 1, 0 ), ivec2( 0 ), size - 1 ) );
	vec3 h = tap( clamp( p + ivec2( 0, -1 ), ivec2( 0 ), size - 1 ) );
	// luma times 2 for the noise test
	float bL = b.b * 0.5 + ( b.r * 0.5 + b.g ), dL = d.b * 0.5 + ( d.r * 0.5 + d.g ), eL = e.b * 0.5 + ( e.r * 0.5 + e.g );
	float fL = f.b * 0.5 + ( f.r * 0.5 + f.g ), hL = h.b * 0.5 + ( h.r * 0.5 + h.g );
	float nz = 0.25 * ( bL + dL + fL + hL ) - eL;
	float rangeL = max( max( max( bL, dL ), max( eL, fL ) ), hL ) - min( min( min( bL, dL ), min( eL, fL ) ), hL );
	nz = clamp( abs( nz ) / max( rangeL, 1e-6 ), 0.0, 1.0 );
	nz = -0.5 * nz + 1.0;
	// the ring's min/max, and the most negative lobe that keeps the result inside it
	vec3 mn4 = min( min( b, d ), min( f, h ) );
	vec3 mx4 = max( max( b, d ), max( f, h ) );
	vec2 peakC = vec2( 1.0, -4.0 );
	vec3 hitMin = min( mn4, e ) / max( 4.0 * mx4, vec3( 1e-6 ) );
	vec3 hitMax = ( peakC.x - max( mx4, e ) ) / min( 4.0 * mn4 + peakC.y, vec3( -1e-6 ) );
	vec3 lobeRGB = max( -hitMin, hitMax );
	float lobe = max( -kRcasLimit, min( max( lobeRGB.r, max( lobeRGB.g, lobeRGB.b ) ), 0.0 ) ) * rcasSharp;
	lobe *= nz;
	float rcpL = 1.0 / ( 4.0 * lobe + 1.0 );
	vec3 o = clamp( ( lobe * ( b + d + f + h ) + e ) * rcpL, 0.0, 1.0 );
	if ( rcasLinear != 0 ) {
		o = min( o, vec3( 0.999 ) );
		o = o / ( ( 1.0 - max( o.r, max( o.g, o.b ) ) ) * max( rcasExpose, 1e-6 ) );
	}
	fragColor = vec4( o, 1.0 );
}
