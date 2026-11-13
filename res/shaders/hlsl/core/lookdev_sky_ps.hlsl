// HLSL source of lookdev_sky.frag, translated by notes/hlslsrc1/tools/glsl2hlsl.py (text level: comments, names and
// #if variants kept from the GLSL). Matrices hold GL's columns as rows (row_major blocks), so a GL product A * B
// reads mul(B, A).
// profile: ps_6_0
// variants: #if (the loader's WW_* defines select the variant; built with -D per set)
#include "CoreBindingsVk.hlsli"
#include "GlslCompat.hlsli"
#define WW_STAGE_PS 1

/* Lookdev background (lane PBRR2B, docs s6.2 stage 1: cube-only background).
 * The lookdev cube (linear, SFCubeMapCache level 0), UNTINTED by the weather
 * until W2 (ruling Q8), plus a GGX glow at the sun DISC (the tent-arc position,
 * not the floored light) while the disc is above the horizon. Out through the
 * Studio output path (exposure, view transform, sRGB encode). */

#include "uniforms.src.hlsli"

TextureCube<float4> CubeMap; SamplerState _CubeMap_sampler;
uniform bool hasCubeMap;
uniform bool invertZAxis;
uniform float3 sunDiscView;	// view space, unit; z of the WORLD disc <= 0 -> sunDiscUp false
uniform bool sunDiscUp;
uniform float3 sunLinear;
uniform float sceneExposure;
uniform int viewTransform;

static float2 ndc;

static float4 fragColor;

// stage I/O: the members (locations, semantics) of the generated port, each under its GLSL declaration's #if
struct SPIRV_Cross_Input
{
    float2 ndc : TEXCOORD0;
};
struct SPIRV_Cross_Output
{
    float4 fragColor : SV_Target0;
};


#include "lookdev_output.src.hlsli"

void frag_main()
{
	if ( projectionMatrix[3][3] == 1.0 ) {
		// orthographic: no eye ray; a flat mid-grey stands in
		fragColor = float4( studioOutput( (float3)(0.18) ), 1.0 );
		return;
	}
	float3 ray = normalize( float3( ( ndc.x + projectionMatrix[2][0] ) / projectionMatrix[0][0],
	                            ( ndc.y + projectionMatrix[2][1] ) / projectionMatrix[1][1], -1.0 ) );
	float3x3 r = envMapRotation;
	if ( invertZAxis ) {
		r[0][2] *= -1.0;
		r[1][2] *= -1.0;
		r[2][2] *= -1.0;
	}
	float3 color = hasCubeMap ? CubeMap.SampleLevel(_CubeMap_sampler, mul(ray, r), 0.0).rgb : (float3)(0.18);
	if ( sunDiscUp ) {
		float VdotL = dot( ray, normalize( sunDiscView ) );
		if ( VdotL > 0.0 ) {
			float alpha = 0.1 * 0.1;
			float a2 = alpha * alpha;
			float denom = VdotL * a2 + a2 + max( 1.0 - VdotL, 0.0 );
			color += sunLinear * ( a2 / ( denom * denom ) ) * VdotL;
		}
	}
	fragColor = float4( studioOutput( color ), 1.0 );
}

// entry: copies the stage inputs into the GLSL-named globals, runs the GLSL main, returns the outputs
SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    ndc = stage_input.ndc;
    frag_main();
    SPIRV_Cross_Output stage_output;
    stage_output.fragColor = fragColor;
    return stage_output;
}
