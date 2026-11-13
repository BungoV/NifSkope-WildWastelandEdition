// HLSL source of features/post/cell_objblur.frag (fragment shader), lane HLSLSRC3 (notes/hlslsrc3/tools/glsl2hlsl.py): one readable file for every
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
/* lane POSTFX1: per-object motion blur, pass 4 of 4: a gather over the neighbour max as in McGuire et al. 2012 ("A
 * Reconstruction Filter for Plausible Motion Blur", I3D: tile max, neighbour max, a depth-compared gather), with
 * the sample jitter of Jimenez 2014 (interleaved gradient noise). colTex the frame, blurTex the pixel vectors
 * (pixels over the shutter, cell_objblur_vec.frag), neighTex the neighbour max (one texel per tile), depthTex the
 * frame's depth (window depth, as game_dof.frag reads it: distance = far near / ((1 - d)(far - near) + near)).
 * Weights (a lane ruling, written in notes/postfx1/RESULTS.md): every sample stands for an equal share of the
 * shutter, so the result is the time each surface covers the pixel (a box along the vector, the gate's bar):
 *   Y in front of X: counts while Y's own blur reaches X (pixel coverage over Y's radius);
 *   X in front of Y: counts while X's blur uncovers Y (pixel coverage over X's radius);
 *   the same surface: counts fully when both are still, else while the faster one's blur reaches.
 * The paper's cone weights blurred the bar's edge 0.54 of its travel; these give the box. A pixel whose
 * neighbourhood holds no vector over half a pixel is copied untouched, so a still world is the input bit for bit. */

Texture2D<float4> colTex : register(t0, space0); SamplerState _colTex_sampler : register(s0, space0);
Texture2D<float4> blurTex : register(t1, space0); SamplerState _blurTex_sampler : register(s1, space0);
Texture2D<float4> neighTex : register(t2, space0); SamplerState _neighTex_sampler : register(s2, space0);
Texture2D<float4> depthTex : register(t3, space0); SamplerState _depthTex_sampler : register(s3, space0);
cbuffer LooseUniforms : register(b31, space0)
{
	int tile;
	int samples;
	float zNear;
	float zFar;
	float noiseSeed;
};


static float4 fragColor;

float dist( int2 p )
{
	float x = 1.0 - depthTex.Load( int3(p, 0) ).r;
	return zFar * zNear / ( x * ( zFar - zNear ) + zNear );
}

float reach( float d, float r )	// the share of a pixel d away (one pixel wide) that lies inside radius r
{
	return clamp( r + 0.5 - d, 0.0, 1.0 );
}

float front( float za, float zb, float k )	// 1 when a is in front of (or level with) b, fading out over k behind it
{
	return clamp( 1.0 - ( za - zb ) / k, 0.0, 1.0 );
}

void frag_main()
{
	int2 X = int2( gl_FragCoord.xy );
	int2 sz = textureSize( colTex, 0 );
	float4 cX = colTex.Load( int3(X, 0) );
	float2 vn = neighTex.Load( int3(X / tile, 0) ).xy * 0.5;	// radius: half the shutter's travel each side
	float ln = length( vn );
	if ( ln < 0.5 ) {
		fragColor = cX;
		return;
	}
	float2 vs = vn * ( ( ln + 0.5 ) / ln );	// the span reaches the half pixel the last covered pixel overhangs
	float rx = length( blurTex.Load( int3(X, 0) ).xy ) * 0.5;
	float zx = dist( X );
	float k = max( 0.02 * zx, 1.0 );
	float wsum = 1.0;
	float4 sum = cX;
	float j = frac( 52.9829189 * frac( dot( gl_FragCoord.xy, float2( 0.06711056, 0.00583715 ) ) + noiseSeed ) ) - 0.5;
	int n = max( samples, 2 );
	for ( int i = 0; i < n; i++ ) {
		float t = lerp( -1.0, 1.0, ( float( i ) + j + 1.0 ) / float( n + 1 ) );
		int2 Y = clamp( int2( floor( float2( X ) + 0.5 + vs * t ) ), (int2)( 0 ), sz - 1 );
		float d = length( float2( Y - X ) );
		float ry = length( blurTex.Load( int3(Y, 0) ).xy ) * 0.5;
		float zy = dist( Y );
		float yf = front( zy, zx, k );
		float xf = front( zx, zy, k );
		float same = yf * xf;
		float rm = max( rx, ry );
		float a = same * ( rm < 0.5 ? 1.0 : reach( d, rm ) )
			+ ( yf - same ) * reach( d, ry )
			+ ( xf - same ) * reach( d, rx );
		wsum += a;
		sum += a * colTex.Load( int3(Y, 0) );
	}
	fragColor = sum / wsum;
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
