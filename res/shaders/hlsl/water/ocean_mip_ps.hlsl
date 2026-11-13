// HLSL source of features/water/ocean_mip.frag (fragment shader), lane HLSLSRC2 (notes/hlslsrc2/tools/glsl2hlsl.py): one readable file for every
// variant, the GLSL's comments, names and order kept. Loose uniforms are the LooseUniforms cbuffer (b31).
// variants: #if none (the loader's defines; res/shaders/hlsl/variants.txt lists the sets built)
// profile: ps_6_0
#include "WaterBindingsVk.hlsli"
#define WW_HLSL_SRC 1	// the shared .hlsli files give their readable half
static float4 gl_FragCoord;
// lane WATERLOOK1 (src/features/water/cellocean.cpp): one mip level of the three arrays, all six layers in one draw (ocean_layers.geom),
// the 2 x 2 mean of the level above (C's mean of squares keeps the second moments the water's lost-variance term
// reads). Seven draws instead of glGenerateMipmap on the arrays: the whole build is 0.14 ms with these, 0.6-0.85 ms with
// glGenerateMipmap (notes/waterlook1/tools/ocean_gl.py). Both arrays' base and max level are the level read while a
// level draws (no feedback loop), so lod 0 below.

Texture2DArray<float4> oceanB : register(t0, space0); SamplerState _oceanB_sampler : register(s0, space0);
Texture2DArray<float4> oceanC : register(t1, space0); SamplerState _oceanC_sampler : register(s1, space0);
Texture2DArray<float4> oceanD : register(t2, space0); SamplerState _oceanD_sampler : register(s2, space0);	// step 2: the heights and the chop
static int oceanLayerG;	// the layer drawn (ocean_layers.geom)

static float4 o0;
static float4 o1;
static float4 o2;

void frag_main()
{
	int2 p = 2 * int2( gl_FragCoord.xy );
	o0 = 0.25 * ( oceanB.Load( int4(int3( p, oceanLayerG ), 0) ) + oceanB.Load( int4(int3( p + int2( 1, 0 ), oceanLayerG ), 0) )
		+ oceanB.Load( int4(int3( p + int2( 0, 1 ), oceanLayerG ), 0) ) + oceanB.Load( int4(int3( p + int2( 1, 1 ), oceanLayerG ), 0) ) );
	o1 = 0.25 * ( oceanC.Load( int4(int3( p, oceanLayerG ), 0) ) + oceanC.Load( int4(int3( p + int2( 1, 0 ), oceanLayerG ), 0) )
		+ oceanC.Load( int4(int3( p + int2( 0, 1 ), oceanLayerG ), 0) ) + oceanC.Load( int4(int3( p + int2( 1, 1 ), oceanLayerG ), 0) ) );
	o2 = 0.25 * ( oceanD.Load( int4(int3( p, oceanLayerG ), 0) ) + oceanD.Load( int4(int3( p + int2( 1, 0 ), oceanLayerG ), 0) )
		+ oceanD.Load( int4(int3( p + int2( 0, 1 ), oceanLayerG ), 0) ) + oceanD.Load( int4(int3( p + int2( 1, 1 ), oceanLayerG ), 0) ) );
}

struct SPIRV_Cross_Input
{
    nointerpolation int oceanLayerG : TEXCOORD0;
    float4 gl_FragCoord : SV_Position;
};

struct SPIRV_Cross_Output
{
    float4 o0 : SV_Target0;
    float4 o1 : SV_Target1;
    float4 o2 : SV_Target2;
};

SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    gl_FragCoord = stage_input.gl_FragCoord;
    gl_FragCoord.w = 1.0 / gl_FragCoord.w;
    oceanLayerG = stage_input.oceanLayerG;
    frag_main();
    SPIRV_Cross_Output stage_output;
    stage_output.o0 = o0;
    stage_output.o1 = o1;
    stage_output.o2 = o2;
    return stage_output;
}
