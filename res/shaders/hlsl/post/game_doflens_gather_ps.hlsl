// HLSL source of features/post/game_doflens_gather.frag (fragment shader), lane HLSLSRC3 (notes/hlslsrc3/tools/glsl2hlsl.py): one readable file for every
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
/* lane DOFLENS1: the thin-lens depth of field's gather (src/features/post/cellpost.cpp), quarter size, two layers.
 * t0 = the prep (rgb, a = the signed circle of confusion in full px). 32 taps on a Vogel disk of radius Rk (full px):
 * the centre's own far radius, or the largest near radius when the near layer is on.
 * Far (out 0): a tap counts where its disk covers the centre (coverage sat(rho - r + 0.5)); a tap behind the centre
 * spreads no wider than the centre's own disk, so a sharp or nearer surface is never blurred by what is behind it.
 * Near (out 1, premultiplied): each tap's colour spread over its disk (area pi rho^2) against the tap's share of the
 * kernel (pi Rk^2 / N); a = the covered share, rgb = its mean colour times a.
 * lensQ.x = the largest circle (px), .y = near on, .w = near reach from t1 (the tile max, game_doflens_tile.frag:
 * the largest near radius whose disk can reach this texel) instead of the fixed largest radius. */

Texture2D<float4> t0 : register(t0, space0); SamplerState _t0_sampler : register(s0, space0);
Texture2D<float4> t1 : register(t1, space0); SamplerState _t1_sampler : register(s1, space0);
cbuffer LooseUniforms : register(b31, space0)
{
	float4 lensQ;
};


static float4 farOut;
static float4 nearOut;

static const int N = 32;

float sat( float v )
{
	return v >= 0.0 ? min( v, 1.0 ) : 0.0;
}

void frag_main()
{
	int2 q = int2( gl_FragCoord.xy );
	float2 qs = float2( textureSize( t0, 0 ) );
	float4 c0 = t0.Load( int3(q, 0) );
	float cc = c0.a;
	float rc = max( cc, 0.0 ) * 0.5;
	float rn = lensQ.y == 0.0 ? 0.0 : lensQ.w > 0.5 ? t1.Load( int3(q / 8, 0) ).r : lensQ.x * 0.5;
	float rk = max( rc, rn );
	if ( rk < 0.5 ) {
		farOut = float4( c0.rgb, 1.0 );
		nearOut = (float4)( 0.0 );
		return;
	}
	float2 uc = ( float2( q ) + 0.5 ) / qs;
	float3 fa = (float3)( 0.0 ), na = (float3)( 0.0 );
	float fw = 0.0, nd = 0.0;
	float share = rk * rk / float( N );
	for ( int i = 0; i < N; i++ ) {
		float t = sqrt( ( float( i ) + 0.5 ) / float( N ) ) * rk;	// full px
		float an = float( i ) * 2.39996323;
		float2 o = float2( cos( an ), sin( an ) ) * t;
		float4 cs = t0.Sample( _t0_sampler, uc + o * 0.25 / qs );
		float rf = max( cs.a, 0.0 ) * 0.5;
		if ( cs.a > cc )
			rf = min( rf, rc );
		float w = sat( rf - t + 0.5 );
		fa += cs.rgb * w;
		fw += w;
		float rN = max( -cs.a, 0.0 ) * 0.5;
		float d = sat( rN - t + 0.5 ) * share / max( rN * rN, 1.0 );
		na += cs.rgb * d;
		nd += d;
	}
	farOut = float4( fw > 0.0001 ? fa / fw : c0.rgb, 1.0 );
	float a = sat( nd );
	nearOut = nd > 0.0 ? float4( na / nd * a, a ) : (float4)( 0.0 );
}

struct SPIRV_Cross_Input
{
    float4 gl_FragCoord : SV_Position;
};

struct SPIRV_Cross_Output
{
    float4 farOut : SV_Target0;
    float4 nearOut : SV_Target1;
};

SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    gl_FragCoord = stage_input.gl_FragCoord;
    gl_FragCoord.w = 1.0 / gl_FragCoord.w;
    frag_main();
    SPIRV_Cross_Output stage_output;
    stage_output.farOut = farOut;
    stage_output.nearOut = nearOut;
    return stage_output;
}
