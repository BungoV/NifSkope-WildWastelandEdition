// HLSL source of features/sky/atmo_transmittance.frag (fragment shader), lane HLSLSRC2 (notes/hlslsrc2/tools/glsl2hlsl.py): one readable file for every
// variant, the GLSL's comments, names and order kept. Loose uniforms are the LooseUniforms cbuffer (b31).
// variants: #if none (the loader's defines; res/shaders/hlsl/variants.txt lists the sets built)
// profile: ps_6_0
#include "SkyBindingsVk.hlsli"
#define WW_HLSL_SRC 1	// the shared .hlsli files give their readable half
static float4 gl_FragCoord;
// lane ATMO1: the transmittance LUT (256x64): exp(-optical depth) to the top of the atmosphere

#define WW_LOOSE_TYPES 1
#include "atmo_common.hlsli"
#undef WW_LOOSE_TYPES
cbuffer LooseUniforms : register(b31, space0)
{
#define WW_LOOSE_UNIFORMS 1
#include "atmo_common.hlsli"
#undef WW_LOOSE_UNIFORMS
};
#include "atmo_common.hlsli"

static float4 fragColor;

void frag_main()
{
	float r, mu;
	atTransRmu( gl_FragCoord.xy / kTransSize, r, mu );
	float d = atDistTop( r, mu );
	// two segments meeting at the ray's lowest point, samples packed toward it: the fog's haze layer can be ~100 m
	// thick on a 600 km grazing ray (uniform steps missed it: tests/features/sky/atmo_lut_check.py transmittance-haze)
	float tm = clamp( -r * mu, 0.0, d );
	const int N = 40;
	float3 od = (float3)( 0.0 );
	for ( int i = 0; i < N; i++ ) {
		float a0 = float( i ) / float( N ), a1 = float( i + 1 ) / float( N ), am = ( a0 + a1 ) * 0.5;
		// segment A [0, tm]: t = tm (1 - (1 - a)^2); segment B [tm, d]: t = tm + (d - tm) a^2
		float tA = tm * ( 1.0 - ( 1.0 - am ) * ( 1.0 - am ) ), wA = tm * ( ( 1.0 - a0 ) * ( 1.0 - a0 ) - ( 1.0 - a1 ) * ( 1.0 - a1 ) );
		float tB = tm + ( d - tm ) * am * am, wB = ( d - tm ) * ( a1 * a1 - a0 * a0 );
		float3 rs, ms, e;
		float rr = sqrt( r * r + tA * tA + 2.0 * r * mu * tA );
		atMedium( rr - kRb, rs, ms, e );
		od += e * wA;
		rr = sqrt( r * r + tB * tB + 2.0 * r * mu * tB );
		atMedium( rr - kRb, rs, ms, e );
		od += e * wB;
	}
	fragColor = float4( exp( -od ), 1.0 );
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
