// HLSL source of cell_shadowdepth.frag, translated by notes/hlslsrc1/tools/glsl2hlsl.py (text level: comments, names and
// #if variants kept from the GLSL). Matrices hold GL's columns as rows (row_major blocks), so a GL product A * B
// reads mul(B, A).
// profile: ps_6_0
// variants: #if (the loader's WW_* defines select the variant; built with -D per set)
#include "LightsBindingsVk.hlsli"
#include "../core/GlslCompat.hlsli"
#define WW_STAGE_PS 1
static float gl_FragDepth;

/* Cell light shadow caster pass: depth = distance to the light over its radius; nothing nearer than
 * the near clip casts. No color attachment. */

uniform float2 shRadiusNear;	// radius, near clip
// lane GAPFIX1: alpha-tested casters keep what the colour pass keeps (as lookdev_csmdepth_at.frag)
uniform int shAlphaTest;
Texture2D<float4> BaseMap; SamplerState _BaseMap_sampler;
uniform float2 uvScale;
uniform float2 uvOffset;
uniform float alpha;
uniform float alphaThreshold;

static float3 shRel;
static float2 texCoord;

// stage I/O: the members (locations, semantics) of the generated port, each under its GLSL declaration's #if
struct SPIRV_Cross_Input
{
    float3 shRel : TEXCOORD0;
    float2 texCoord : TEXCOORD1;
};
struct SPIRV_Cross_Output
{
    float gl_FragDepth : SV_Depth;
};


void frag_main()
{
	float d = length( shRel );
	if ( d < shRadiusNear.y )
		discard;
	if ( shAlphaTest != 0 && BaseMap.Sample(_BaseMap_sampler, texCoord * uvScale + uvOffset).a * alpha < alphaThreshold )
		discard;
	gl_FragDepth = clamp( d / shRadiusNear.x, 0.0, 1.0 );
}

// entry: copies the stage inputs into the GLSL-named globals, runs the GLSL main, returns the outputs
SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    shRel = stage_input.shRel;
    texCoord = stage_input.texCoord;
    frag_main();
    SPIRV_Cross_Output stage_output;
    stage_output.gl_FragDepth = gl_FragDepth;
    return stage_output;
}
