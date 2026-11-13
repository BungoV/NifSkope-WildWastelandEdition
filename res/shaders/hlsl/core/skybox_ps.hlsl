// HLSL source of skybox.frag (fragment shader), lane HLSLSRC1 (notes/hlsltools/glsl2hlsl.py): one readable file for every
// variant, the GLSL's comments, names and order kept. Loose uniforms are the LooseUniforms cbuffer (b31).
// variants: #if none (the loader's defines; res/shaders/hlsl/variants.txt lists the sets built)
// profile: ps_6_0
#include "CoreBindingsVk.hlsli"
#define WW_HLSL_SRC 1	// the shared .hlsli files give their readable half
#include "uniforms.hlsli"

TextureCube<float4> CubeMap : register(t0, space0); SamplerState _CubeMap_sampler : register(s0, space0);
cbuffer LooseUniforms : register(b31, space0)
{
	bool hasCubeMap;
};


static float3 LightDir;
static float3 ViewDir;

static float3x3 reflMatrix;

static float4 fragColor;

float LightingFuncGGX_REF( float LdotR, float roughness )
{
	float alpha = roughness * roughness;
	// D (GGX normal distribution)
	float alphaSqr = alpha * alpha;
	// denom = NdotH * NdotH * (alphaSqr - 1.0) + 1.0,
	// LdotR = NdotH * NdotH * 2.0 - 1.0
	float denom = LdotR * alphaSqr + alphaSqr + max(1.0 - LdotR, 0.0);
	// no pi because BRDF -> lighting
	return alphaSqr / (denom * denom);
}

float3 tonemap(float3 x, float y)
{
	float a = 0.15;
	float b = 0.50;
	float c = 0.10;
	float d = 0.20;
	float e = 0.02;
	float f = 0.30;

	float3 z = x * (y * 4.22978723);
	z = (z * (a * z + b * c) + d * e) / (z * (a * z + b) + d * f) - e / f;
	return z / (y * 0.93333333);
}

void frag_main()
{
	float3	L = normalize( LightDir );
	float3	V = normalize( -ViewDir );

	float	VdotL = dot( V, L );

	float3	viewWS = mul(V, reflMatrix);

	float	m = clamp( float(cubeBgndMipLevel), 0.0, 6.0 );

	// Environment
	float3	color = lightSourceAmbient.rgb;
	if ( hasCubeMap ) {
		color *= CubeMap.SampleLevel( _CubeMap_sampler, viewWS, m ).rgb;
	} else {
		color *= 0.08;
	}
	// Directional light
	if ( VdotL > 0.0 ) {
		float	roughness = ( 5.0 - sqrt( 25.0 - 4.0 * m ) ) / 4.0;
		color += lightSourceDiffuse[0].rgb * LightingFuncGGX_REF( VdotL, max(roughness, 0.02) ) * VdotL;
	}

	fragColor = float4( tonemap( color * brightnessScale, toneMapScale ), 0.0 );
}

struct SPIRV_Cross_Input
{
    float3 LightDir : TEXCOORD0;
    float3 ViewDir : TEXCOORD1;
    nointerpolation float3x3 reflMatrix : TEXCOORD2;
};

struct SPIRV_Cross_Output
{
    float4 fragColor : SV_Target0;
};

SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    LightDir = stage_input.LightDir;
    ViewDir = stage_input.ViewDir;
    reflMatrix = stage_input.reflMatrix;
    frag_main();
    SPIRV_Cross_Output stage_output;
    stage_output.fragColor = fragColor;
    return stage_output;
}
