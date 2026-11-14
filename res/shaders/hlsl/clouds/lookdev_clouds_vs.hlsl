// HLSL source of features/clouds/lookdev_clouds.vert (vertex shader), lane HLSLSRC2 (notes/hlslsrc2/tools/glsl2hlsl.py): one readable file for every
// variant, the GLSL's comments, names and order kept. Loose uniforms are the LooseUniforms cbuffer (b31).
// variants: #if none (the loader's defines; res/shaders/hlsl/variants.txt lists the sets built)
// profile: vs_6_0
#include "CloudsBindingsVk.hlsli"
#define WW_HLSL_SRC 1	// the shared .hlsli files give their readable half
#define Sample( s, uv ) SampleLevel( s, uv, 0 )	// GLSL: texture() outside a fragment shader reads level 0
#define SampleCmp SampleCmpLevelZero	// the same for a shadow compare (SampleCmp is fragment-only)
// inlined (no readable .hlsli yet): uniforms.glsl
static float4 gl_Position;
/* Lookdev cloud layer (lane PBRWX1), vertex stage: view rotation only, the
 * layer's uv plus its scroll offset (fract(speed * 0.1 * real seconds)).
 * probe: a full-screen quad that samples ONE uv (the texel gate). */

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

static float3 vertexPosition;
static float4 vertexColor;	// white, alpha = the edge fade
static float2 multiTexCoord0;
cbuffer LooseUniforms : register(b31, space0)
{
	float2 uvOffset;
	bool probe;
	float2 probeUV;
	float2 uvOffset2;	// lane WEATHERBLEND2: the new weather's scroll while a change cross-fades
};


static float4 vcol;
static float2 uv;
static float2 uv2;
static float2 ndc;
static float3 wdir;	// lane LIGHTNING2: the world direction of the texel (the dome is the eye's, rotation only)

void vert_main()
{
	wdir = probe ? float3( 0.0, 0.0, 1.0 ) : vertexPosition;
	if ( probe ) {
		ndc = vertexPosition.xy;
		vcol = (float4)( 1.0 );
		uv = probeUV + uvOffset;
		uv2 = probeUV + uvOffset2;
		gl_Position = float4( vertexPosition.xy, 0.0, 1.0 );
		return;
	}
	ndc = (float2)( 0.0 );
	vcol = vertexColor;
	uv = multiTexCoord0 + uvOffset;
	uv2 = multiTexCoord0 + uvOffset2;
	float4 q = mul(float4( mul(vertexPosition, viewMatrix), 1.0 ), projectionMatrix);
	gl_Position = float4( q.xy, 0.0, q.w );
}

struct SPIRV_Cross_Input
{
    float3 vertexPosition : TEXCOORD0;
    float4 vertexColor : TEXCOORD1;
    float2 multiTexCoord0 : TEXCOORD7;
};

struct SPIRV_Cross_Output
{
    float4 vcol : TEXCOORD0;
    float2 uv : TEXCOORD1;
    float2 uv2 : TEXCOORD2;
    float2 ndc : TEXCOORD3;
    float3 wdir : TEXCOORD4;
    float4 gl_Position : SV_Position;
};

SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    vertexPosition = stage_input.vertexPosition;
    vertexColor = stage_input.vertexColor;
    multiTexCoord0 = stage_input.multiTexCoord0;
    vert_main();
    SPIRV_Cross_Output stage_output;
    stage_output.gl_Position = gl_Position;
    stage_output.vcol = vcol;
    stage_output.uv = uv;
    stage_output.uv2 = uv2;
    stage_output.ndc = ndc;
    stage_output.wdir = wdir;
    return stage_output;
}
