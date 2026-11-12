// Point-to-quad path for selection_ps.hlsl, written by hand (HLSLBULK1 round 2). HLSL has no point sprites, so
// gl_PointCoord (which selection.frag reads to draw a vertex as a smooth disc) cannot be a builtin: each point the
// selection vertex stage draws (vertex mode, selectionFlags bit 1) is expanded here to a screen-aligned quad of
// wwPointSize pixels, and the corner is handed to the pixel stage as wwPointCoord (0..1, origin top left, as
// gl_PointCoord). The quad is two triangles (cull off). Fusion Shaders (F4FS) draws the vertex markers through it.
// Not wired: nothing loads this file.
// profile: gs_6_0
#include "CoreBindingsVk.hlsli"
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

cbuffer LooseUniforms : register(b31, space0)
{
    float wwPointSize;	// glPointSize in pixels (Shape::drawVerts pointSize; selection_ps's smooth edge uses selectionFlags bits 8..15 = size * 8)
};

struct StageInput
{
    float4 C : TEXCOORD0;
    float4 gl_PositionIn : SV_Position;
};

struct StageOutput
{
    float2 wwPointCoord : TEXCOORD0;	// the semantics selection_ps.hlsl's input struct has
    float4 C : TEXCOORD1;
    float4 gl_Position : SV_Position;
};

[maxvertexcount(4)]
void main(point StageInput stage_input[1], inout TriangleStream<StageOutput> geometry_stream)
{
    float4 p = stage_input[0].gl_PositionIn;
    float2 px = float2(2.0f, 2.0f) * wwPointSize / float2(max(viewportDimensions.z, 1), max(viewportDimensions.w, 1));
    for (int i = 0; i < 4; i++)
    {
        float2 corner = float2(float(i & 1), float(i >> 1));
        StageOutput o;
        o.C = stage_input[0].C;
        o.wwPointCoord = corner;
        o.gl_Position = float4(p.xy + float2(corner.x - 0.5f, 0.5f - corner.y) * px * p.w, p.zw);
        geometry_stream.Append(o);
    }
    geometry_stream.RestartStrip();
}
