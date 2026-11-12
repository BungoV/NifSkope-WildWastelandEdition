// Point-to-quad path for cell_fardots_ps.hlsl, written by hand (HLSLBULK1 round 2). HLSL has no point sprites, so
// gl_PointCoord (which cell_fardots.frag reads for the bulb's Gaussian) cannot be a builtin: each bulb the vertex
// stage (cell_fardots_vs.hlsl) draws is expanded here to a screen-aligned quad of the size the GLSL vertex stage gave
// gl_PointSize, min( 2 ceil( 3 dotSigma ) + 1, 63 ) pixels, and the corner goes to the pixel stage as wwPointCoord
// (0..1, origin top left, as gl_PointCoord). GLSL drew size 0 where the fade is 0; here a bulb whose dotValue is
// all zero (fade 0 times anything) is skipped, which adds the same nothing. The quad is two triangles (cull off).
// Fusion Shaders (F4FS) draws the far bulbs through it. Not wired: nothing loads this file.
// profile: gs_6_0
#include "FarLodBindingsVk.hlsli"
cbuffer globalUniforms : register(b0, space0)
{
    row_major float3x3 viewMatrix : packoffset(c0);
    row_major float3x3 envMapRotation : packoffset(c3);
    row_major float4x4 projectionMatrix : packoffset(c6);
    float4 lightSourcePosition[3] : packoffset(c10);
    float4 lightSourceDiffuse[3] : packoffset(c13);
    float4 lightSourceAmbient : packoffset(c16);
    float toneMapScale : packoffset(c17);
    float brightnessScale : packoffset(c17.y);
    float glowScale : packoffset(c17.z);
    float glowScaleSRGB : packoffset(c17.w);
    int4 viewportDimensions : packoffset(c18);
    uint doSkinning : packoffset(c19);
    int sceneOptions : packoffset(c19.y);
    int cubeBgndMipLevel : packoffset(c19.z);
    int sfParallaxMaxSteps : packoffset(c19.w);
    float sfParallaxScale : packoffset(c20);
    float sfParallaxOffset : packoffset(c20.y);
    float unusedUniform1 : packoffset(c20.z);
    float unusedUniform2 : packoffset(c20.w);
};

struct StageInput
{
    float3 dotValue : TEXCOORD0;
    float3 dotView : TEXCOORD1;
    float dotSigma : TEXCOORD2;
    float4 gl_PositionIn : SV_Position;
};

struct StageOutput
{
    float2 wwPointCoord : TEXCOORD0;	// the semantics cell_fardots_ps.hlsl's input struct has
    float3 dotValue : TEXCOORD1;
    float dotSigma : TEXCOORD2;
    float3 dotView : TEXCOORD3;
    float4 gl_Position : SV_Position;
};

[maxvertexcount(4)]
void main(point StageInput stage_input[1], inout TriangleStream<StageOutput> geometry_stream)
{
    StageInput v = stage_input[0];
    if (!any(v.dotValue != float3(0.0f, 0.0f, 0.0f)))
    {
        return;
    }
    float size = min(2.0f * ceil(3.0f * v.dotSigma) + 1.0f, 63.0f);
    float2 px = float2(2.0f, 2.0f) * size / float2(max(viewportDimensions.z, 1), max(viewportDimensions.w, 1));
    for (int i = 0; i < 4; i++)
    {
        float2 corner = float2(float(i & 1), float(i >> 1));
        StageOutput o;
        o.dotValue = v.dotValue;
        o.dotView = v.dotView;
        o.dotSigma = v.dotSigma;
        o.wwPointCoord = corner;
        o.gl_Position = float4(v.gl_PositionIn.xy + float2(corner.x - 0.5f, 0.5f - corner.y) * px * v.gl_PositionIn.w, v.gl_PositionIn.zw);
        geometry_stream.Append(o);
    }
    geometry_stream.RestartStrip();
}
