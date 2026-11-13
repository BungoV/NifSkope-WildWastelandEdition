// HLSL source of features/gi/cell_ddgiprobe.vert (vertex shader), lane HLSLSRC2 (notes/hlslsrc2/tools/glsl2hlsl.py): one readable file for every
// variant, the GLSL's comments, names and order kept. Loose uniforms are the LooseUniforms cbuffer (b31).
// variants: #if none (the loader's defines; res/shaders/hlsl/variants.txt lists the sets built)
// profile: vs_6_0
#include "GiBindingsVk.hlsli"
#define WW_HLSL_SRC 1	// the shared .hlsli files give their readable half
// inlined (no readable .hlsli yet): uniforms.glsl
static float4 gl_Position;
static int gl_VertexID;
static int gl_InstanceID;
/* Lane RTXFIX1 (2026-10-30): the Ray-traced GI probes view, vertex stage. No vertex attributes: gl_VertexID picks the
 * unit sphere's corner (probeMesh, from meshBase), gl_InstanceID the probe (probeList, from listBase: x = the probe's
 * index, y = its state code for the states view). The probe's centre is the volume's own (the grid point + the atlas's
 * relocation offset), so the sphere sits where the read takes the probe, scroll and relocation included. */

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
	int meshBase;
	int listBase;
	float probeRad;
	float4 cellDdgiOrigin;
	float4 cellDdgiStep;
	int4 cellDdgiDims;
};

Buffer<float4> probeMesh : register(t0, space0);
Buffer<float4> probeList : register(t1, space0);
Buffer<float4> cellDdgiAtlas : register(t2, space0);

static float3 probeN;
static float3 viewPos;
static int probeIdx;
static float probeState;

void vert_main()
{
	float3 m = probeMesh.Load( meshBase + gl_VertexID ).xyz;
	float4 inst = probeList.Load( listBase + gl_InstanceID );
	int p = int( inst.x + 0.5 );
	int px = p % cellDdgiDims.x;
	int py = ( p / cellDdgiDims.x ) % cellDdgiDims.y;
	int pz = p / ( cellDdgiDims.x * cellDdgiDims.y );
	float4 pr = cellDdgiAtlas.Load( 192 * cellDdgiDims.w + p );
	float3 c = cellDdgiOrigin.xyz + float3( float( px ), float( py ), float( pz ) ) * cellDdgiStep.xyz + pr.xyz;
	float4 v = mul(float4( c + m * probeRad, 1.0 ), modelViewMatrix);
	probeN = m;
	viewPos = v.xyz;
	probeIdx = p;
	probeState = inst.y;
	gl_Position = mul(v, projectionMatrix);
}

struct SPIRV_Cross_Input
{
    uint gl_VertexID : SV_VertexID;
    uint gl_InstanceID : SV_InstanceID;
};

struct SPIRV_Cross_Output
{
    float3 probeN : TEXCOORD0;
    float3 viewPos : TEXCOORD1;
    nointerpolation int probeIdx : TEXCOORD2;
    nointerpolation float probeState : TEXCOORD3;
    float4 gl_Position : SV_Position;
};

SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    gl_VertexID = int(stage_input.gl_VertexID);
    gl_InstanceID = int(stage_input.gl_InstanceID);
    vert_main();
    SPIRV_Cross_Output stage_output;
    stage_output.gl_Position = gl_Position;
    stage_output.probeN = probeN;
    stage_output.viewPos = viewPos;
    stage_output.probeIdx = probeIdx;
    stage_output.probeState = probeState;
    return stage_output;
}
