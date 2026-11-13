// HLSL source of wireframe.geom, translated by notes/hlslsrc1/tools/glsl2hlsl.py (text level: comments, names and
// #if variants kept from the GLSL). Matrices hold GL's columns as rows (row_major blocks), so a GL product A * B
// reads mul(B, A).
// profile: gs_6_0
// variants: #if (the loader's WW_* defines select the variant; built with -D per set)
#include "CoreBindingsVk.hlsli"
#include "GlslCompat.hlsli"
#define WW_STAGE_GS 1
#define WW_GS_STREAM TriangleStream<SPIRV_Cross_Output>
static float4 gl_PositionIn[3];
static float4 gl_Position;


#include "uniforms.src.hlsli"

uniform float lineWidth;

static float4 vsColor[3];
static float4 C;

// stage I/O: the members (locations, semantics) of the generated port, each under its GLSL declaration's #if
struct SPIRV_Cross_Input
{
    WW_LOC(0) float4 vsColor : TEXCOORD0;
    float4 gl_PositionIn : SV_Position;
};
struct SPIRV_Cross_Output
{
    WW_LOC(1) float4 C : TEXCOORD1;
    float4 gl_Position : SV_Position;
};

// GLSL EmitVertex(): the current outputs become one vertex of the strip
void EmitVertex(inout WW_GS_STREAM geometry_stream)
{
    SPIRV_Cross_Output stage_output;
    stage_output.C = C;
    stage_output.gl_Position = gl_Position;
    geometry_stream.Append(stage_output);
}


void drawLine( float4 p0, float4 p1, float4 p1Color , inout WW_GS_STREAM geometry_stream)
{
	float	p0zw = p0.z + p0.w;
	float	p1zw = p1.z + p1.w;
	if ( !( p0zw > 0.0 && p1zw > 0.0 ) ) {
		if ( p0zw > 0.000001 )
			p1 = lerp( p0, p1, p0zw / ( p0zw - p1zw ) );
		else if ( p1zw > 0.000001 )
			p0 = lerp( p1, p0, p1zw / ( p1zw - p0zw ) );
		else
			return;
	}

	float3	p0_ndc = p0.xyz / p0.w;
	float3	p1_ndc = p1.xyz / p1.w;

	float2	vpScale = (float2)(viewportDimensions.zw) * 0.5;
	float2	vpOffs = (float2)(viewportDimensions.xy) + vpScale;

	float2	p0_ss = p0_ndc.xy * vpScale;
	float2	p1_ss = p1_ndc.xy * vpScale;

	float2	d = normalize( p1_ss - p0_ss ) * max( lineWidth * 0.5, 0.5 );
	float2	n = float2( -d.y, d.x ) / vpScale;

	gl_Position = float4( p0_ndc.xy + n, p0_ndc.z, 1.0 );
	EmitVertex(geometry_stream);
	gl_Position = float4( p0_ndc.xy - n, p0_ndc.z, 1.0 );
	EmitVertex(geometry_stream);

	C = p1Color;

	gl_Position = float4( p1_ndc.xy + n, p1_ndc.z, 1.0 );
	EmitVertex(geometry_stream);
	gl_Position = float4( p1_ndc.xy - n, p1_ndc.z, 1.0 );
	EmitVertex(geometry_stream);

	geometry_stream.RestartStrip();
}

void geom_main(inout WW_GS_STREAM geometry_stream)
{
	float4	p0 = mul(gl_PositionIn[0], projectionMatrix);
	float4	p1 = mul(gl_PositionIn[1], projectionMatrix);
	float4	p2 = mul(gl_PositionIn[2], projectionMatrix);

	float	alphaMult = min( lineWidth, 1.0 );
	float4	c0 = float4( vsColor[0].rgb, vsColor[0].a * alphaMult );
	C = c0;

	drawLine( p0, p1, float4( vsColor[1].rgb, vsColor[1].a * alphaMult ) , geometry_stream);
	drawLine( p1, p2, float4( vsColor[2].rgb, vsColor[2].a * alphaMult ) , geometry_stream);
	drawLine( p2, p0, c0 , geometry_stream);
}

// entry: copies the stage inputs into the GLSL-named globals, runs the GLSL main, returns the outputs
[maxvertexcount(12)]
void main(triangle SPIRV_Cross_Input stage_input[3], inout TriangleStream<SPIRV_Cross_Output> geometry_stream)
{
    for (int i = 0; i < 3; i++)
    {
        gl_PositionIn[i] = stage_input[i].gl_PositionIn;
    }
    for (int i = 0; i < 3; i++)
    {
        vsColor[i] = stage_input[i].vsColor;
    }
    geom_main(geometry_stream);
}
