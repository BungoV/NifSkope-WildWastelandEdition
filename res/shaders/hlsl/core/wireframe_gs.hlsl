// HLSL source of wireframe.geom (geometry shader), lane HLSLSRC1 (notes/hlsltools/glsl2hlsl.py): one readable file for every
// variant, the GLSL's comments, names and order kept. Loose uniforms are the LooseUniforms cbuffer (b31).
// variants: #if none (the loader's defines; res/shaders/hlsl/variants.txt lists the sets built)
// profile: gs_6_0
#include "CoreBindingsVk.hlsli"
#define WW_HLSL_SRC 1	// the shared .hlsli files give their readable half
#define Sample( s, uv ) SampleLevel( s, uv, 0 )	// GLSL: texture() outside a fragment shader reads level 0
#define SampleCmp SampleCmpLevelZero	// the same for a shadow compare (SampleCmp is fragment-only)
struct WW_GL_PerVertex { float4 gl_Position; };
static WW_GL_PerVertex gl_in[3];
static float4 gl_Position;
// GLSL: layout ( triangles ) in; (main's signature)
// GLSL: layout ( triangle_strip , max_vertices = 12 ) out; (main's signature)

#include "uniforms.hlsli"

cbuffer LooseUniforms : register(b31, space0)
{
	float lineWidth;
};


static float4 vsColor[3];
static float4 C;


struct SPIRV_Cross_Output
{
    WW_LOC(1) float4 C : TEXCOORD1;
    float4 gl_Position : SV_Position;
};

#define WW_GS_STREAM TriangleStream<SPIRV_Cross_Output>
void EmitVertex(inout WW_GS_STREAM geometry_stream)
{
    SPIRV_Cross_Output stage_output;
    stage_output.gl_Position = gl_Position;
    stage_output.C = C;
    geometry_stream.Append(stage_output);
}

void EndPrimitive(inout WW_GS_STREAM geometry_stream)
{
    geometry_stream.RestartStrip();
}

void drawLine(inout WW_GS_STREAM geometry_stream,  float4 p0, float4 p1, float4 p1Color )
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

	float2	vpScale = float2( viewportDimensions.zw ) * 0.5;
	float2	vpOffs = float2( viewportDimensions.xy ) + vpScale;

	float2	p0_ss = p0_ndc.xy * vpScale;
	float2	p1_ss = p1_ndc.xy * vpScale;

	float2	d = normalize( p1_ss - p0_ss ) * max( lineWidth * 0.5, 0.5 );
	float2	n = float2( -d.y, d.x ) / vpScale;

	gl_Position = float4( p0_ndc.xy + n, p0_ndc.z, 1.0 );
	EmitVertex( geometry_stream );
	gl_Position = float4( p0_ndc.xy - n, p0_ndc.z, 1.0 );
	EmitVertex( geometry_stream );

	C = p1Color;

	gl_Position = float4( p1_ndc.xy + n, p1_ndc.z, 1.0 );
	EmitVertex( geometry_stream );
	gl_Position = float4( p1_ndc.xy - n, p1_ndc.z, 1.0 );
	EmitVertex( geometry_stream );

	EndPrimitive( geometry_stream );
}

void geom_main(inout WW_GS_STREAM geometry_stream)
{
	float4	p0 = mul(gl_in[0].gl_Position, projectionMatrix);
	float4	p1 = mul(gl_in[1].gl_Position, projectionMatrix);
	float4	p2 = mul(gl_in[2].gl_Position, projectionMatrix);

	float	alphaMult = min( lineWidth, 1.0 );
	float4	c0 = float4( vsColor[0].rgb, vsColor[0].a * alphaMult );
	C = c0;

	drawLine( geometry_stream, p0, p1, float4( vsColor[1].rgb, vsColor[1].a * alphaMult ) );
	drawLine( geometry_stream, p1, p2, float4( vsColor[2].rgb, vsColor[2].a * alphaMult ) );
	drawLine( geometry_stream, p2, p0, c0 );
}

struct SPIRV_Cross_Input
{
    WW_LOC(0) float4 vsColor : TEXCOORD0;
    float4 gl_PositionIn : SV_Position;
};

[maxvertexcount(12)]
void main(triangle SPIRV_Cross_Input stage_input[3], inout WW_GS_STREAM geometry_stream)
{
    for (int i = 0; i < 3; i++)
        gl_in[i].gl_Position = stage_input[i].gl_PositionIn;
    for (int i = 0; i < 3; i++)
        vsColor[i] = stage_input[i].vsColor;
    geom_main(geometry_stream);
}
