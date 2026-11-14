// HLSL source of features/post/game_mblur.frag (fragment shader), lane HLSLSRC2 (notes/hlslsrc2/tools/glsl2hlsl.py): one readable file for every
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
/* lane GRASSMB1: the game's motion blur, instruction for instruction (src/features/post/cellpost.h). The pass works in the
 * game's texture space (v down): the pixel, the vectors (y turned over) and c1's corner are taken there, every
 * tap is read back through V(). t0 the frame, t1 the vectors (previous minus current, texture units);
 * c0 = (scale x 0.001 / dt, max blur, threshold, 0), c1 = (1, 1, (W - 1) / W, (H - 1) / H). */

Texture2D<float4> t0 : register(t0, space0); SamplerState _t0_sampler : register(s0, space0);
Texture2D<float4> t1 : register(t1, space0); SamplerState _t1_sampler : register(s1, space0);
cbuffer LooseUniforms : register(b31, space0)
{
	float4 c0;
	float4 c1;
};


static float4 fragColor;

float sat( float v )	// D3D _sat: NaN -> 0
{
	return v >= 0.0 ? min( v, 1.0 ) : 0.0;
}

float2 V( float2 d )	// game texture space -> this view's
{
	return float2( d.x, 1.0 - d.y );
}

float2 mvAt( float2 d )
{
	float2 m = t1.Sample( _t1_sampler, V( d ) ).xy;
	return float2( m.x, -m.y );
}

float tapW( float2 m, float L )	// dp2, sqrt, add, mad_sat
{
	float l = sqrt( dot( m, m ) );
	return sat( -abs( L - l ) * c0.z + 1.0 );
}

void frag_main()
{
	float2 g = gl_FragCoord.xy / float2( textureSize( t0, 0 ) );
	float2 v1 = V( g );
	float2 m = mvAt( v1 );
	float l2 = dot( m, m );
	float L = sqrt( l2 );
	float2 dc = v1 - c1.zw;
	float rr = sqrt( dot( dc, dc ) );
	rr = sat( rr * 0.1 + -0.018 );
	rr = L * rr;
	if ( rr < 0.001 ) {
		fragColor = t0.Sample( _t0_sampler, g );
		return;
	}
	float2 dir = rsqrt( l2 ) * m;
	float len = min( rr, c0.y );
	dir = len * dir;
	dir = dir * c0.x;

	float2 p0 = min( v1, c1.xy );
	float w0 = tapW( mvAt( p0 ), L );
	float3 col = t0.Sample( _t0_sampler, V( p0 ) ).rgb;
	float4 p12 = min( dir.xyxy * float4( 0.25, 0.25, 0.5, 0.5 ) + v1.xyxy, c1.xyxy );
	float w1 = tapW( mvAt( p12.xy ), L );
	float ws = w0 + w1;
	float3 c1t = w1 * t0.Sample( _t0_sampler, V( p12.xy ) ).rgb;
	col = col * w0 + c1t;
	float w2 = tapW( mvAt( p12.zw ), L );
	ws = w2 + ws;
	col = t0.Sample( _t0_sampler, V( p12.zw ) ).rgb * w2 + col;
	float2 p3 = min( dir * 0.75 + v1, c1.xy );
	float w3 = tapW( mvAt( p3 ), L );
	ws = w3 + ws;
	col = t0.Sample( _t0_sampler, V( p3 ) ).rgb * w3 + col;
	ws = ws + 0.001;
	fragColor = float4( col / ws, 1.0 );
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
