// HLSL source of selection.vert (vertex shader), lane HLSLSRC1 (notes/hlsltools/glsl2hlsl.py): one readable file for every
// variant, the GLSL's comments, names and order kept. Loose uniforms are the LooseUniforms cbuffer (b31).
// variants: #if none (the loader's defines; res/shaders/hlsl/variants.txt lists the sets built)
// profile: vs_6_0
#include "CoreBindingsVk.hlsli"
#define WW_HLSL_SRC 1	// the shared .hlsli files give their readable half
#define Sample( s, uv ) SampleLevel( s, uv, 0 )	// GLSL: texture() outside a fragment shader reads level 0
#define SampleCmp SampleCmpLevelZero	// the same for a shadow compare (SampleCmp is fragment-only)
#ifndef WW_GLSL_UNPACKUNORM4
#define WW_GLSL_UNPACKUNORM4
float4 ww_unpackUnorm4x8(uint v) { return float4(v & 0xffu, (v >> 8) & 0xffu, (v >> 16) & 0xffu, v >> 24) / 255.0; }
#endif
static float4 gl_Position;
static int gl_VertexID;
static float4 C;

#include "uniforms.hlsli"

cbuffer LooseUniforms : register(b31, space0)
{
	row_major float4x4 modelViewMatrix;
	float4 vertexColorOverride;	// components greater than zero replace the vertex color
	float4 highlightColor;
	// bit 0 = Scene::selecting
	// bit 1 = vertex mode (drawing points instead of triangles)
	// bit 2 = triangle selection mode (primitive ID << 15 is added to the color)
	// bit 3 = draw bone weights
	// bits 8 to 15 = point size * 8 (0: do not draw smooth points)
	int selectionFlags;
	// if Scene::selecting == false: vertex selected (-1: none)
	// if Scene::selecting == true: value to add to color key (e.g. shapeID << 16)
	int selectionParam;
};


static float3 vertexPosition;
static float4 vertexColor;

#define BT_POSITION_ONLY 1
#include "bonetransform.hlsli"

float4 boneWeightsColor()
{
	float4	vcSum = (float4)( 0.0 );
	float	wSum = 0.0;
	for ( int i = 0; i < 8; i++ ) {
		float	w = boneWeights[i >> 2][i & 3];
		if ( !( w > 0.0 ) )
			break;
		int	b = int( w );
		w = frac( w );
		int	vc = ( b & 0x0049 ) | ( ( b & 0x0092 ) << 7 ) | ( ( b & 0x0124 ) << 14 );
		vc = ( ( vc & 0x00010101 ) << 7 ) | ( ( vc & 0x00080808 ) << 3 ) | ( ( vc & 0x00404040 ) >> 1 );
		vcSum += ww_unpackUnorm4x8( uint( vc ) ^ 0xE0E0E0E0u ) * w;
		wSum += w;
	}
	if ( wSum > 0.0 )
		vcSum /= wSum;
	return vcSum;
}

void vert_main()
{
	if ( ( selectionFlags & 1 ) != 0 ) {
		int	colorKey = selectionParam + 1;
		if ( ( selectionFlags & 2 ) != 0 )
			colorKey = colorKey + gl_VertexID;
		C = ww_unpackUnorm4x8( uint( colorKey ) ) + (float4)( 0.0005 );
	} else if ( !( gl_VertexID == selectionParam && ( selectionFlags & 2 ) != 0 ) ) {
		C = select((vertexColorOverride > (float4)( 0.0 )), vertexColorOverride, vertexColor);
	} else {
		C = highlightColor;
	}

	float4	v = float4( vertexPosition, 1.0 );

	if ( boneWeights[0].x > 0.0 ) {
		if ( doSkinning )
			boneTransform( v );
		if ( ( selectionFlags & 8 ) != 0 )
			C = boneWeightsColor();
	}

	v = mul(( mul(v, modelViewMatrix) ), projectionMatrix);

	gl_Position = v;
}

struct SPIRV_Cross_Input
{
    float3 vertexPosition : TEXCOORD0;
    float4 vertexColor : TEXCOORD1;
    float4 boneWeights[2] : TEXCOORD5;
    uint gl_VertexID : SV_VertexID;
};

struct SPIRV_Cross_Output
{
    float4 C : TEXCOORD0;
    float4 gl_Position : SV_Position;
};

SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    vertexPosition = stage_input.vertexPosition;
    vertexColor = stage_input.vertexColor;
    boneWeights = stage_input.boneWeights;
    gl_VertexID = int(stage_input.gl_VertexID);
    vert_main();
    SPIRV_Cross_Output stage_output;
    stage_output.gl_Position = gl_Position;
    stage_output.C = C;
    return stage_output;
}
