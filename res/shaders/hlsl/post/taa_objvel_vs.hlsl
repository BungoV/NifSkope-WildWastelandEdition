// HLSL source of features/post/taa_objvel.vert (vertex shader), lane HLSLSRC3 (notes/hlslsrc3/tools/glsl2hlsl.py): one readable file for every
// variant, the GLSL's comments, names and order kept. Loose uniforms are the LooseUniforms cbuffer (b31).
// variants: #if none (the loader's defines; res/shaders/hlsl/variants.txt lists the sets built)
// profile: vs_6_0
#include "PostBindingsVk.hlsli"
#define WW_HLSL_SRC 1	// the shared .hlsli files give their readable half
#define Sample( s, uv ) SampleLevel( s, uv, 0 )	// GLSL: texture() outside a fragment shader reads level 0
static float4 gl_Position;
/* lane TAAINPUTS1: one moving shape's current and previous clip positions (src/features/post/taainputs.h).
 * velMode 0: rigid (the same vertex, last frame's matrix); 1: skinned (the CPU skin now in 0, last frame's in 8);
 * 2: the cell's grass wind at this and last frame's phase (fo4_default.vert's offset, step for step). */

cbuffer LooseUniforms : register(b31, space0)
{
	row_major float4x4 clipJit;	// the frame's jittered projection x model-view: lands on the frame's depth
	row_major float4x4 clipCur;	// unjittered, this frame
	row_major float4x4 clipPrev;	// unjittered, last frame
	row_major float4x4 modelViewMatrix;
	int velMode;
	float4 wwGrassWind;
	float4 wwGrassWind2;
	row_major float3x3 normalMatrix;	// lane TREELEAF4: the shape's view rotation, as Shape::setUniforms gives the colour pass
};


static float3 vertexPosition;
static float4 vertexColor;
static float3 normalVector;
static float3 tangentVector;
static float3 bitangentVector;
static float2 multiTexCoord0;
static float3 prevPosition;

static float4 cCur;
static float4 cPrev;
static float3 viewPos;
static float2 texCoord;
static float vAlpha;	// lane TREELEAF3: the vertex alpha an effect shape's colour pass multiplies in (fxMode & 1)
// lane TREELEAF4 (FXCOVRESID1): fo4_default.vert's btnMatrix (bitangent, tangent, normal in view space), for the falloff
static float3x3 btnMatrix;

float3 grassOffset( float phase )
{
	float L = length( bitangentVector );
	float off = L > 2.0 ? L - 1024.0 : 0.0;
	float p = ( phase - off ) * wwGrassWind2.z;
	float half_range = ( wwGrassWind2.y - wwGrassWind2.x ) * 0.5;
	float sp = sin( p ), cp = cos( p );
	float w = sin( sp * 3.14159274 ) + sin( sp * 6.28318548 );
	w = w * 0.3 + cos( cp * 3.14159274 ) * 0.2;
	w = w + 1.0;
	w = w * half_range + wwGrassWind2.x;
	w = w * ( vertexColor.a * vertexColor.a * 0.5 );
	return float3( cos( wwGrassWind.x ), sin( wwGrassWind.x ), 0.0 ) * w;
}

void vert_main()
{
	float3 cur = vertexPosition;
	float3 prev = velMode == 1 ? prevPosition : vertexPosition;
	if ( velMode == 2 && wwGrassWind2.w > 0.5 ) {
		cur += grassOffset( wwGrassWind.w );
		prev += grassOffset( wwGrassWind.z );
	}
	cCur = mul(float4( cur, 1.0 ), clipCur);
	cPrev = mul(float4( prev, 1.0 ), clipPrev);
	viewPos = ( mul(float4( cur, 1.0 ), modelViewMatrix) ).xyz;
	texCoord = multiTexCoord0;
	vAlpha = vertexColor.a;
	btnMatrix[2] = normalize( mul(normalMatrix, normalVector) );
	btnMatrix[1] = normalize( mul(normalMatrix, tangentVector) );
	btnMatrix[0] = normalize( mul(normalMatrix, bitangentVector) );
	gl_Position = mul(float4( cur, 1.0 ), clipJit);
}

struct SPIRV_Cross_Input
{
    float3 vertexPosition : TEXCOORD0;
    float4 vertexColor : TEXCOORD1;
    float3 normalVector : TEXCOORD2;
    float3 tangentVector : TEXCOORD3;
    float3 bitangentVector : TEXCOORD4;
    float2 multiTexCoord0 : TEXCOORD7;
    float3 prevPosition : TEXCOORD8;
};

struct SPIRV_Cross_Output
{
    float4 cCur : TEXCOORD0;
    float4 cPrev : TEXCOORD1;
    float3 viewPos : TEXCOORD2;
    float2 texCoord : TEXCOORD3;
    float vAlpha : TEXCOORD4;
    float3x3 btnMatrix : TEXCOORD5;
    float4 gl_Position : SV_Position;
};

SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    vertexPosition = stage_input.vertexPosition;
    vertexColor = stage_input.vertexColor;
    normalVector = stage_input.normalVector;
    tangentVector = stage_input.tangentVector;
    bitangentVector = stage_input.bitangentVector;
    multiTexCoord0 = stage_input.multiTexCoord0;
    prevPosition = stage_input.prevPosition;
    vert_main();
    SPIRV_Cross_Output stage_output;
    stage_output.gl_Position = gl_Position;
    stage_output.cCur = cCur;
    stage_output.cPrev = cPrev;
    stage_output.viewPos = viewPos;
    stage_output.texCoord = texCoord;
    stage_output.vAlpha = vAlpha;
    stage_output.btnMatrix = btnMatrix;
    return stage_output;
}
