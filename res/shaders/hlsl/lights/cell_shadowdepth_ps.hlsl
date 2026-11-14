// HLSL source of features/lights/cell_shadowdepth.frag (fragment shader), lane HLSLSRC1 (notes/hlsltools/glsl2hlsl.py): one readable file for every
// variant, the GLSL's comments, names and order kept. Loose uniforms are the LooseUniforms cbuffer (b31).
// variants: #if none (the loader's defines; res/shaders/hlsl/variants.txt lists the sets built)
// profile: ps_6_0
#include "LightsBindingsVk.hlsli"
#define WW_HLSL_SRC 1	// the shared .hlsli files give their readable half
static float gl_FragDepth;
/* Cell light shadow caster pass: depth = distance to the light over its radius; nothing nearer than
 * the near clip casts. No color attachment. */

cbuffer LooseUniforms : register(b31, space0)
{
	float2 shRadiusNear;	// radius, near clip
	// lane GAPFIX1: alpha-tested casters keep what the colour pass keeps (as lookdev_csmdepth_at.frag)
	int shAlphaTest;
	float2 uvScale;
	float2 uvOffset;
	float alpha;
	float alphaThreshold;
};

Texture2D<float4> BaseMap : register(t0, space0); SamplerState _BaseMap_sampler : register(s0, space0);

static float3 shRel;
static float2 texCoord;

void frag_main()
{
	float d = length( shRel );
	if ( d < shRadiusNear.y )
		discard;
	if ( shAlphaTest != 0 && BaseMap.Sample( _BaseMap_sampler, texCoord * uvScale + uvOffset ).a * alpha < alphaThreshold )
		discard;
	gl_FragDepth = clamp( d / shRadiusNear.x, 0.0, 1.0 );
}

struct SPIRV_Cross_Input
{
    float3 shRel : TEXCOORD0;
    float2 texCoord : TEXCOORD1;
};

struct SPIRV_Cross_Output
{
    float gl_FragDepth : SV_Depth;
};

SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    shRel = stage_input.shRel;
    texCoord = stage_input.texCoord;
    frag_main();
    SPIRV_Cross_Output stage_output;
    stage_output.gl_FragDepth = gl_FragDepth;
    return stage_output;
}
