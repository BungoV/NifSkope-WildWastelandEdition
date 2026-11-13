// HLSL source of features/water/ocean_fft.frag (fragment shader), lane HLSLSRC2 (notes/hlslsrc2/tools/glsl2hlsl.py): one readable file for every
// variant, the GLSL's comments, names and order kept. Loose uniforms are the LooseUniforms cbuffer (b31).
// variants: #if none (the loader's defines; res/shaders/hlsl/variants.txt lists the sets built)
// profile: ps_6_0
#include "WaterBindingsVk.hlsli"
#define WW_HLSL_SRC 1	// the shared .hlsli files give their readable half
static float4 gl_FragCoord;
// lane WATERLOOK1 (src/features/water/cellocean.cpp): one radix-2 Stockham stage of the inverse FFT (sign +, no 1 / N), N = 128, on
// every 128-texel row (axis 0) or column (axis 1) of the six tiles; four complex numbers a texel. Stage Ns = 1, 2 .. 64;
// output i = ( i / 2Ns ) Ns + i mod Ns = j:  y = x[ j ] + ( -1 )^( i / Ns ) e^( i pi ( i mod Ns ) / Ns ) x[ j + N / 2 ].
// Seven stages a direction leave the transform in natural order (Stockham's autosort).

Texture2D<float4> oceanIn0 : register(t0, space0); SamplerState _oceanIn0_sampler : register(s0, space0);
Texture2D<float4> oceanIn1 : register(t1, space0); SamplerState _oceanIn1_sampler : register(s1, space0);
cbuffer LooseUniforms : register(b31, space0)
{
	int oceanNs;
	int oceanAxis;
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
	int i = oceanAxis == 0 ? p.x & 127 : p.y;
	int k = i % oceanNs;
	int j = ( i / ( 2 * oceanNs ) ) * oceanNs + k;
	float sg = ( ( i / oceanNs ) & 1 ) != 0 ? -1.0 : 1.0;
	int2 a = oceanAxis == 0 ? int2( p.x - i + j, p.y ) : int2( p.x, j );
	int2 b = oceanAxis == 0 ? int2( a.x + 64, p.y ) : int2( p.x, j + 64 );
	float th = 3.14159265 * float( k ) / float( oceanNs );
	float2 w = sg * float2( cos( th ), sin( th ) );
	float4 a0 = oceanIn0.Load( int3(a, 0) ), a1 = oceanIn1.Load( int3(a, 0) );
	float4 b0 = oceanIn0.Load( int3(b, 0) ), b1 = oceanIn1.Load( int3(b, 0) );
	o0 = a0 + float4( cmul( b0.xy, w ), cmul( b0.zw, w ) );
	o1 = a1 + float4( cmul( b1.xy, w ), cmul( b1.zw, w ) );
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
