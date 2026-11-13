// HLSL source of features/shadows/lookdev_csmremap.frag (fragment shader), lane HLSLSRC2 (notes/hlslsrc2/tools/glsl2hlsl.py): one readable file for every
// variant, the GLSL's comments, names and order kept. Loose uniforms are the LooseUniforms cbuffer (b31).
// variants: #if none (the loader's defines; res/shaders/hlsl/variants.txt lists the sets built)
// profile: ps_6_0
#include "ShadowsBindingsVk.hlsli"
#define WW_HLSL_SRC 1	// the shared .hlsli files give their readable half
static float4 gl_FragCoord;
static float gl_FragDepth;
/* lane SHADOWCACHE1: the old static depth at this texel's place in the old window, mapped into the new depth range;
 * cleared depth stays 1, and texels the old window never covered get 1 (their strips are drawn after). */
Texture2DArray<float4> srcMap : register(t0, space0); SamplerState _srcMap_sampler : register(s0, space0);
cbuffer LooseUniforms : register(b31, space0)
{
	int srcX, srcY, srcW, srcH;
	float zScale, zBias;
};


void frag_main()
{
	int2 p = int2( gl_FragCoord.xy ) + int2( srcX, srcY );
	float d = 1.0;
	if ( p.x >= 0 && p.y >= 0 && p.x < srcW && p.y < srcH ) {
		float o = srcMap.Load( int4(int3( p, 0 ), 0) ).r;
		d = o >= 1.0 ? 1.0 : clamp( o * zScale + zBias, 0.0, 1.0 );
	}
	gl_FragDepth = d;
}

struct SPIRV_Cross_Input
{
    float4 gl_FragCoord : SV_Position;
};

struct SPIRV_Cross_Output
{
    float gl_FragDepth : SV_Depth;
};

SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    gl_FragCoord = stage_input.gl_FragCoord;
    gl_FragCoord.w = 1.0 / gl_FragCoord.w;
    frag_main();
    SPIRV_Cross_Output stage_output;
    stage_output.gl_FragDepth = gl_FragDepth;
    return stage_output;
}
