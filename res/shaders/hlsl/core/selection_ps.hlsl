// HLSL source of selection.frag (fragment shader), lane HLSLSRC1 (notes/hlsltools/glsl2hlsl.py): one readable file for every
// variant, the GLSL's comments, names and order kept. Loose uniforms are the LooseUniforms cbuffer (b31).
// variants: #if none (the loader's defines; res/shaders/hlsl/variants.txt lists the sets built)
// profile: ps_6_0
#include "CoreBindingsVk.hlsli"
#define WW_HLSL_SRC 1	// the shared .hlsli files give their readable half
#ifndef WW_GLSL_UNPACKUNORM4
#define WW_GLSL_UNPACKUNORM4
float4 ww_unpackUnorm4x8(uint v) { return float4(v & 0xffu, (v >> 8) & 0xffu, (v >> 16) & 0xffu, v >> 24) / 255.0; }
#endif
#ifndef WW_GLSL_PACKUNORM4
#define WW_GLSL_PACKUNORM4
uint ww_packUnorm4x8(float4 v) { uint4 u = uint4(round(saturate(v) * 255.0)); return u.x | (u.y << 8) | (u.z << 16) | (u.w << 24); }
#endif
static int gl_PrimitiveID;
static float2 gl_PointCoord;	// HLSL has no point sprites: the stage input wwPointCoord
static float4 C;
cbuffer LooseUniforms : register(b31, space0)
{
	// bit 0 = Scene::selecting
	// bit 1 = vertex mode (drawing points instead of triangles)
	// bit 2 = triangle selection mode (primitive ID << 15 is added to the color)
	// bits 8 to 15 = point size * 8 (0: do not draw smooth points)
	int selectionFlags;
};


static float4 fragColor;

void frag_main()
{
	float4	color = C;

	if ( ( selectionFlags & 2 ) != 0 ) {
		// draw points as circles
		float2	d = gl_PointCoord - (float2)( 0.5 );
		float	r2 = dot( d, d );
		if ( r2 > 0.25 )
			discard;

		// with anti-aliasing if enabled
		int	p = selectionFlags & 0xFF00;
		if ( p != 0 )
			color.a *= clamp( ( 0.5 - sqrt(r2) ) * float( p ) / 2048.0, 0.0, 1.0 );
	} else if ( ( selectionFlags & 4 ) != 0 ) {
		color = ww_unpackUnorm4x8( ww_packUnorm4x8( color ) + ( uint( gl_PrimitiveID ) << 15 ) ) + (float4)( 0.0005 );
	}

	fragColor = color;
}

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

SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    gl_PointCoord = stage_input.wwPointCoord;
    C = stage_input.C;
    gl_PrimitiveID = int(stage_input.gl_PrimitiveID);
    frag_main();
    SPIRV_Cross_Output stage_output;
    stage_output.fragColor = fragColor;
    return stage_output;
}
