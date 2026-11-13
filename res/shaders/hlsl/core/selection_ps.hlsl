// HLSL source of selection.frag, translated by notes/hlslsrc1/tools/glsl2hlsl.py (text level: comments, names and
// #if variants kept from the GLSL). Matrices hold GL's columns as rows (row_major blocks), so a GL product A * B
// reads mul(B, A).
// profile: ps_6_0
// variants: #if (the loader's WW_* defines select the variant; built with -D per set)
#include "CoreBindingsVk.hlsli"
#include "GlslCompat.hlsli"
#define WW_STAGE_PS 1
static uint gl_PrimitiveID;
static float2 wwPointCoord;

static float4 C;

// bit 0 = Scene::selecting
// bit 1 = vertex mode (drawing points instead of triangles)
// bit 2 = triangle selection mode (primitive ID << 15 is added to the color)
// bits 8 to 15 = point size * 8 (0: do not draw smooth points)
uniform int selectionFlags;

static float4 fragColor;

// stage I/O: the members (locations, semantics) of the generated port, each under its GLSL declaration's #if
struct SPIRV_Cross_Input
{
    float2 wwPointCoord : TEXCOORD0;
    float4 C : TEXCOORD1;
    uint gl_PrimitiveID : SV_PrimitiveID;
};
struct SPIRV_Cross_Output
{
    float4 fragColor : SV_Target0;
};


void frag_main()
{
	float4	color = C;

	if ( ( selectionFlags & 2 ) != 0 ) {
		// draw points as circles
		float2	d = wwPointCoord - (float2)(0.5);
		float	r2 = dot( d, d );
		if ( r2 > 0.25 )
			discard;

		// with anti-aliasing if enabled
		int	p = selectionFlags & 0xFF00;
		if ( p != 0 )
			color.a *= clamp( ( 0.5 - sqrt(r2) ) * (float)(p) / 2048.0, 0.0, 1.0 );
	} else if ( ( selectionFlags & 4 ) != 0 ) {
		color = wwUnpackUnorm4x8( wwPackUnorm4x8( color ) + ( (uint)(gl_PrimitiveID) << 15 ) ) + (float4)(0.0005);
	}

	fragColor = color;
}

// entry: copies the stage inputs into the GLSL-named globals, runs the GLSL main, returns the outputs
SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    gl_PrimitiveID = stage_input.gl_PrimitiveID;
    C = stage_input.C;
    wwPointCoord = stage_input.wwPointCoord;
    frag_main();
    SPIRV_Cross_Output stage_output;
    stage_output.fragColor = fragColor;
    return stage_output;
}
