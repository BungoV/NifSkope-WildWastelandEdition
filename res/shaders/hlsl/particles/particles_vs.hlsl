// HLSL source of features/particles/particles.vert (vertex shader), lane HLSLSRC2 (notes/hlslsrc2/tools/glsl2hlsl.py): one readable file for every
// variant, the GLSL's comments, names and order kept. Loose uniforms are the LooseUniforms cbuffer (b31).
// variants: #if none (the loader's defines; res/shaders/hlsl/variants.txt lists the sets built)
// profile: vs_6_0
#include "ParticlesBindingsVk.hlsli"
#define WW_HLSL_SRC 1	// the shared .hlsli files give their readable half
#define Sample( s, uv ) SampleLevel( s, uv, 0 )	// GLSL: texture() outside a fragment shader reads level 0
#define SampleCmp SampleCmpLevelZero	// the same for a shadow compare (SampleCmp is fragment-only)
// inlined (no readable .hlsli yet): uniforms.glsl
static float4 gl_Position;
static float3 vsLightDir;
static float3 vsViewDir;

static float4 vsColor;
static float vsParticleSize;
static float2 vsUVOffset;
static float vsAngle;

// uniforms.glsl inlined here: its .hlsli has no readable half yet (lane HLSLSRC1)
cbuffer globalUniforms : register(b0, space0)
{
	row_major float3x3	viewMatrix : packoffset(c0);					// rotation part of view transform
	row_major float3x3	envMapRotation : packoffset(c3);				// view space to environment map
	row_major float4x4	projectionMatrix : packoffset(c6);
	float4	lightSourcePosition[3] : packoffset(c10);
	float4	lightSourceDiffuse[3] : packoffset(c13);
	float4	lightSourceAmbient : packoffset(c16);
	float	toneMapScale : packoffset(c17);				// 1.0 = full tone mapping
	float	brightnessScale : packoffset(c17.y);
	float	glowScale : packoffset(c17.z);
	float	glowScaleSRGB : packoffset(c17.w);
	int4	viewportDimensions : packoffset(c18);			// X, Y, width, height
	uint	doSkinning : packoffset(c19);
	int	sceneOptions : packoffset(c19.y);
	int	cubeBgndMipLevel : packoffset(c19.z);
	int	sfParallaxMaxSteps : packoffset(c19.w);
	float	sfParallaxScale : packoffset(c20);
	float	sfParallaxOffset : packoffset(c20.y);
	float	unusedUniform1 : packoffset(c20.z);
	float	unusedUniform2 : packoffset(c20.w);
};
// end of uniforms.glsl

cbuffer LooseUniforms : register(b31, space0)
{
	row_major float4x4 modelViewMatrix;
	float4 vertexColorOverride;	// components greater than zero replace the vertex color
};


static float3 vertexPosition;
static float4 vertexColor;
// per-particle flipbook cell offset (defaults to 0,0 when not provided)
static float2 vertexUV;
// per-particle sprite rotation in radians (defaults to 0 when not provided)
static float vertexAngle;
// location 4 (bitangent) is used for sizes because the default is set to vec3(1, 0, 0)
static float particleSize;

void vert_main()
{
	float4	v = mul(float4( vertexPosition, 1.0 ), modelViewMatrix);

	gl_Position = v;

	if ( projectionMatrix[3][3] == 1.0 )
		vsViewDir = float3(0.0, 0.0, 1.0);	// orthographic view
	else
		vsViewDir = -v.xyz;
	vsLightDir = lightSourcePosition[0].xyz;

	vsColor = select((vertexColorOverride > (float4)( 0.0 )), vertexColorOverride, vertexColor);
	vsParticleSize = particleSize;
	vsUVOffset = vertexUV;
	vsAngle = vertexAngle;
}

struct SPIRV_Cross_Input
{
    float3 vertexPosition : TEXCOORD0;
    float4 vertexColor : TEXCOORD1;
    float2 vertexUV : TEXCOORD2;
    float vertexAngle : TEXCOORD3;
    float particleSize : TEXCOORD4;
};

struct SPIRV_Cross_Output
{
    WW_LOC(2) float3 vsLightDir : TEXCOORD2;
    WW_LOC(5) float3 vsViewDir : TEXCOORD5;
    WW_LOC(0) float4 vsColor : TEXCOORD0;
    WW_LOC(3) float vsParticleSize : TEXCOORD3;
    WW_LOC(4) float2 vsUVOffset : TEXCOORD4;
    WW_LOC(1) float vsAngle : TEXCOORD1;
    float4 gl_Position : SV_Position;
};

SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    vertexPosition = stage_input.vertexPosition;
    vertexColor = stage_input.vertexColor;
    vertexUV = stage_input.vertexUV;
    vertexAngle = stage_input.vertexAngle;
    particleSize = stage_input.particleSize;
    vert_main();
    SPIRV_Cross_Output stage_output;
    stage_output.gl_Position = gl_Position;
    stage_output.vsLightDir = vsLightDir;
    stage_output.vsViewDir = vsViewDir;
    stage_output.vsColor = vsColor;
    stage_output.vsParticleSize = vsParticleSize;
    stage_output.vsUVOffset = vsUVOffset;
    stage_output.vsAngle = vsAngle;
    return stage_output;
}
