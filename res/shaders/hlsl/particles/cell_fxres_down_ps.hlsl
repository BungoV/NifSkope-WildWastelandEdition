// HLSL source of features/particles/cell_fxres_down.frag (fragment shader), lane HLSLSRC2 (notes/hlslsrc2/tools/glsl2hlsl.py): one readable file for every
// variant, the GLSL's comments, names and order kept. Loose uniforms are the LooseUniforms cbuffer (b31).
// variants: #if none (the loader's defines; res/shaders/hlsl/variants.txt lists the sets built)
// profile: ps_6_0
#include "ParticlesBindingsVk.hlsli"
#define WW_HLSL_SRC 1	// the shared .hlsli files give their readable half
static float4 gl_FragCoord;
static float gl_FragDepth;
/* lane FXRES1 (src/features/particles/cellfxres.h): the soft-effect layer's depth, the NEAREST opaque depth under each of its texels
 * (Cantlay, GPU Gems 3 ch. 23: an effect is hidden in the layer only where the nearest surface hides it; the
 * composite's depth test picks the texel that belongs to the pixel's own surface). Written as the layer's depth
 * buffer and as a colour copy the soft fade reads (fxDepth). */

Texture2D<float4> fxrSrc : register(t0, space0); SamplerState _fxrSrc_sampler : register(s0, space0);	// the opaque pass's depth at full resolution (Scene::fxDepthTexId)
cbuffer LooseUniforms : register(b31, space0)
{
	int fxrScale;		// full-resolution pixels per layer texel, per axis
	int2 fxrSrcSize;
};


static float4 fxrZ;

void frag_main()
{
	int2 base = int2( gl_FragCoord.xy ) * fxrScale;
	float d = 1.0;
	for ( int y = 0; y < fxrScale; y++ )
		for ( int x = 0; x < fxrScale; x++ )
			d = min( d, fxrSrc.Load( int3(min( base + int2( x, y ), fxrSrcSize - 1 ), 0) ).r );
	fxrZ = (float4)( d );
	gl_FragDepth = d;
}

struct SPIRV_Cross_Input
{
    float4 gl_FragCoord : SV_Position;
};

struct SPIRV_Cross_Output
{
    float4 fxrZ : SV_Target0;
    float gl_FragDepth : SV_Depth;
};

SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    gl_FragCoord = stage_input.gl_FragCoord;
    gl_FragCoord.w = 1.0 / gl_FragCoord.w;
    frag_main();
    SPIRV_Cross_Output stage_output;
    stage_output.gl_FragDepth = gl_FragDepth;
    stage_output.fxrZ = fxrZ;
    return stage_output;
}
