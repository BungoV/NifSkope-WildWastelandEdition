// HLSL source of features/water/ocean_final.frag (fragment shader), lane HLSLSRC2 (notes/hlslsrc2/tools/glsl2hlsl.py): one readable file for every
// variant, the GLSL's comments, names and order kept. Loose uniforms are the LooseUniforms cbuffer (b31).
// variants: #if none (the loader's defines; res/shaders/hlsl/variants.txt lists the sets built)
// profile: ps_6_0
#include "WaterBindingsVk.hlsli"
#define WW_HLSL_SRC 1	// the shared .hlsli files give their readable half
static float4 gl_FragCoord;
// lane WATERLOOK1 (src/features/water/cellocean.cpp): each tile of the transformed fields into its layer of the two arrays (ocean_layers.geom) the water
// reads (mipmapped after): B = ( sx, sy, Dxx, Dyy ), C = ( sx^2, sy^2, sx sy, Dxy ); C's mips are the slopes' second
// moments, so a pixel's lost slope variance is C.xy - B.xy^2 (LEAN mapping, Olano and Baker 2010). D = ( h, Dx, Dy, 0 ):
// the heights and the chop that move the vertices near the camera (step 2, fo4_waterdisp.tese).

Texture2D<float4> oceanX0 : register(t0, space0); SamplerState _oceanX0_sampler : register(s0, space0);	// ( sx, sy, Dxx, Dyy ) after both directions
Texture2D<float4> oceanX1 : register(t1, space0); SamplerState _oceanX1_sampler : register(s1, space0);	// ( Dxy, h, Dx, Dy )
static int oceanLayerG;	// the layer drawn (ocean_layers.geom) = the tile read

static float4 oB;
static float4 oC;
static float4 oD;

void frag_main()
{
	int2 p = int2( gl_FragCoord.xy ) + int2( 128 * oceanLayerG, 0 );
	float4 x0 = oceanX0.Load( int3(p, 0) );
	float4 x1 = oceanX1.Load( int3(p, 0) );
	oB = x0;
	oC = float4( x0.x * x0.x, x0.y * x0.y, x0.x * x0.y, x1.x );
	oD = float4( x1.yzw, 0.0 );
}

struct SPIRV_Cross_Input
{
    nointerpolation int oceanLayerG : TEXCOORD0;
    float4 gl_FragCoord : SV_Position;
};

struct SPIRV_Cross_Output
{
    float4 oB : SV_Target0;
    float4 oC : SV_Target1;
    float4 oD : SV_Target2;
};

SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    gl_FragCoord = stage_input.gl_FragCoord;
    gl_FragCoord.w = 1.0 / gl_FragCoord.w;
    oceanLayerG = stage_input.oceanLayerG;
    frag_main();
    SPIRV_Cross_Output stage_output;
    stage_output.oB = oB;
    stage_output.oC = oC;
    stage_output.oD = oD;
    return stage_output;
}
