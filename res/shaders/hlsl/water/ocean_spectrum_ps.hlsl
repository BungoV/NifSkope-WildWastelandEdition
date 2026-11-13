// HLSL source of features/water/ocean_spectrum.frag (fragment shader), lane HLSLSRC3 (notes/hlslsrc3/tools/glsl2hlsl.py): one readable file for every
// variant, the GLSL's comments, names and order kept. Loose uniforms are the LooseUniforms cbuffer (b31).
// variants: #if none (the loader's defines; res/shaders/hlsl/variants.txt lists the sets built)
// profile: ps_6_0
#include "WaterBindingsVk.hlsli"
#define WW_HLSL_SRC 1	// the shared .hlsli files give their readable half
static float4 gl_FragCoord;
// lane WATERLOOK1 (src/features/water/cellocean.cpp): the ocean spectrum at this frame's clock (Tessendorf 2001, "Simulating Ocean
// Water", eq. 43): h( k, t ) = h0( k ) e^( i w t ) + conj( h0( -k ) ) e^( -i w t ), w quantised to 2 pi q / T so the sea
// loops exactly every T. One texel per mode; six 128 x 128 tiles side by side (sea 0-2, fresh water 3-5). Out: the
// spectra of eight real fields packed two to a complex number (the inverse FFT of a Hermitian pair is real):
//   o0 = ( sx + i sy, Dxx + i Dyy ), o1 = ( Dxy + i h, Dx + i Dy ); s the slope dh/dx, D the horizontal displacement
//   ( D = i k / |k| h: the crests pull together, as Gerstner's ), Dab = d Da / db.

Texture2D<float4> oceanH0 : register(t0, space0); SamplerState _oceanH0_sampler : register(s0, space0);	// RGBA32F: h0( k ), conj( h0( -k ) )
Texture2D<float4> oceanQ : register(t1, space0); SamplerState _oceanQ_sampler : register(s1, space0);	// R32F: q = floor( w T / 2 pi )
cbuffer LooseUniforms : register(b31, space0)
{
	float oceanPhase;	// ( clock mod T ) / T
	float oceanDk[6];	// 2 pi / L of each tile, rad/m
};


static float4 o0;
static float4 o1;

float2 cmul( float2 a, float2 b )
{
	return float2( a.x * b.x - a.y * b.y, a.x * b.y + a.y * b.x );
}

void frag_main()
{
	int2 p = int2( gl_FragCoord.xy );
	int tile = p.x / 128;
	int2 m = int2( p.x - tile * 128, p.y );
	float2 n = float2( m.x < 64 ? m.x : m.x - 128, m.y < 64 ? m.y : m.y - 128 );
	float2 k = n * oceanDk[tile];
	float kl = length( k );
	float4 h0 = oceanH0.Load( int3(p, 0) );
	float q = oceanQ.Load( int3(p, 0) ).r;
	float ph = 6.2831853 * frac( q * oceanPhase );
	float2 e = float2( cos( ph ), sin( ph ) );
	float2 h = cmul( h0.xy, e ) + cmul( h0.zw, float2( e.x, -e.y ) );
	float2 ih = float2( -h.y, h.x );	// i h
	float ki = kl > 0.0 ? 1.0 / kl : 0.0;
	float2 sx = k.x * ih, sy = k.y * ih;
	float2 dx = k.x * ki * ih, dy = k.y * ki * ih;
	float2 dxx = -k.x * k.x * ki * h, dyy = -k.y * k.y * ki * h, dxy = -k.x * k.y * ki * h;
	// a + i b for complex a, b: ( a.x - b.y, a.y + b.x )
	o0 = float4( sx.x - sy.y, sx.y + sy.x, dxx.x - dyy.y, dxx.y + dyy.x );
	o1 = float4( dxy.x - h.y, dxy.y + h.x, dx.x - dy.y, dx.y + dy.x );
}

struct SPIRV_Cross_Input
{
    float4 gl_FragCoord : SV_Position;
};

struct SPIRV_Cross_Output
{
    float4 o0 : SV_Target0;
    float4 o1 : SV_Target1;
};

SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    gl_FragCoord = stage_input.gl_FragCoord;
    gl_FragCoord.w = 1.0 / gl_FragCoord.w;
    frag_main();
    SPIRV_Cross_Output stage_output;
    stage_output.o0 = o0;
    stage_output.o1 = o1;
    return stage_output;
}
