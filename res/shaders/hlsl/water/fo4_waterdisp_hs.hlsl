// HLSL source of features/water/fo4_waterdisp.tesc (tessellation control shader), written by hand (HLSLBULK1 round 2;
// readable header and the GLSL's comments: lane FO4MATCH1). One file, no variants (variants.txt: the stage has no defines).
// SPIRV-Cross cannot emit tessellation HLSL, so this is a hull shader. GLSL `layout(vertices = 3) out` is
// [outputcontrolpoints(3)], gl_TessLevelOuter / gl_TessLevelInner are the patch constant function's
// SV_TessFactor / SV_InsideTessFactor, and the per-vertex copy block (gl_InvocationID) is the control-point function.
// The water's triangles are split near the camera so the wave heights can move their vertices (fo4_waterdisp_ds.hlsl).
// An edge's level is a function of the edge alone, symmetric in its two ends, so two triangles that share an edge
// split it alike: no cracks. Past the fade every level is 1. Fusion Shaders (F4FS) feeds the VS outputs of
// fo4_default_vs.hlsl (the semantics below are its) and the loose uniforms. Not wired: nothing loads this file.
// variants: #if none (the loader's defines; res/shaders/hlsl/variants.txt lists the sets built)
// profile: hs_6_0
#include "WaterBindingsVk.hlsli"
#define WW_HLSL_SRC 1	// the shared .hlsli files give their readable half (none included here)

// lane WATERLOOK1 step 2 (src/gl/cellocean.h): the water's triangles are split near the camera so the wave heights can
// move its vertices (fo4_waterdisp.tese). An edge's level is a function of the edge alone, symmetric in its two ends
// (a + b, a - b up to sign), so the two triangles that share an edge split it alike: no cracks. Past the fade (no
// displacement) every level is 1, the triangle as fo4_default.vert drew it.
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
    bool waterOceanOn;
    float4 waterOceanDisp;	// full out to (u), none past (u), the target edge (px), the most a GPU splits an edge
    int waterOceanRed;	// lane PERFFIX3: 8192 densetess = every edge up to waterOceanDisp.w splits (before)
};

struct StageInput
{
    WW_LOC(3) float3 LightDir : TEXCOORD3;
    WW_LOC(4) float3 ViewDir : TEXCOORD4;
    WW_LOC(8) float2 texCoord : TEXCOORD8;
    WW_LOC(5) float3x3 btnMatrix : TEXCOORD5;
    WW_LOC(0) nointerpolation float4 A : TEXCOORD0;
    WW_LOC(1) float4 C : TEXCOORD1;
    WW_LOC(2) nointerpolation float4 D : TEXCOORD2;
    WW_LOC(12) float rawVertexAlpha : TEXCOORD12;
    WW_LOC(9) nointerpolation float3x3 reflMatrix : TEXCOORD9;
    float4 gl_PositionIn : SV_Position;
};

struct StageOutput
{
    WW_LOC(10) float3 tcLightDir : TEXCOORD10;
    WW_LOC(13) float3 tcViewDir : TEXCOORD13;
    WW_LOC(12) float2 tcTexCoord : TEXCOORD12;
    WW_LOC(0) float3x3 tcBtn : TEXCOORD0;
    WW_LOC(7) float4 tcA : TEXCOORD7;
    WW_LOC(8) float4 tcC : TEXCOORD8;
    WW_LOC(9) float4 tcD : TEXCOORD9;
    WW_LOC(11) float tcRawAlpha : TEXCOORD11;
    WW_LOC(3) float3x3 tcRefl : TEXCOORD3;
    WW_LOC(6) float4 gl_Position : POSITION;	// gl_out[].gl_Position: the clip position rides as a user semantic into the domain shader
};

struct PatchConstants
{
    float edges[3] : SV_TessFactor;
    float inside : SV_InsideTessFactor;
};

// the level of the edge a - b (view space)
float wwDispLevel(float3 a, float3 b)
{
    float3 mid = 0.5f * (a + b);
    float L = length(a - b);
    float dm = length(mid);
    if ((dm - (0.5f * L)) >= waterOceanDisp.y)
    {
        return 1.0f;
    }
    float pxu = ((projectionMatrix[1][1] * 0.5f) * float(viewportDimensions.w)) / max(dm, 1.0f);	// pixels a unit there
    float lv = max((L * pxu) / waterOceanDisp.z, L * 0.00390625f);	// and never coarser than 256 u where it moves
    /* lane PERFFIX3 (PERFRES1101 B water OPT 2): no vertex step finer than the heights' own filter (fo4_waterdisp.tese:
     * step >= 4096 / waterOceanDisp.w = 64 u): the pre-split 1024 u quads (src/cellview.cpp) split to 16 u and sampled
     * a field already smoothed over 64 u four times over, ~1 M triangles near the camera (+1.4 ms at 1080p 4x MSAA,
     * tests/watertess). An edge's own length bounds it, so two triangles sharing the edge still split it alike. */
    float cap = ((waterOceanRed & 8192) != 0) ? waterOceanDisp.w : clamp((L * waterOceanDisp.w) * 0.000244140625f, 1.0f, waterOceanDisp.w);
    return clamp(lv, 1.0f, cap);
}

PatchConstants wwPatchConstants(InputPatch<StageInput, 3> ip)
{
    // outer i is the edge facing vertex i; ViewDir = -position (an orthographic view has none: never split)
    bool on = waterOceanOn && (projectionMatrix[3][3] != 1.0f);
    float3 p0 = -ip[0].ViewDir;
    float3 p1 = -ip[1].ViewDir;
    float3 p2 = -ip[2].ViewDir;
    float e0 = on ? wwDispLevel(p1, p2) : 1.0f;
    float e1 = on ? wwDispLevel(p2, p0) : 1.0f;
    float e2 = on ? wwDispLevel(p0, p1) : 1.0f;
    PatchConstants pc;
    pc.edges[0] = e0;
    pc.edges[1] = e1;
    pc.edges[2] = e2;
    pc.inside = max(e0, max(e1, e2));
    return pc;
}

[domain("tri")]
[partitioning("fractional_odd")]
[outputtopology("triangle_ccw")]
[outputcontrolpoints(3)]
[patchconstantfunc("wwPatchConstants")]
StageOutput main(InputPatch<StageInput, 3> ip, uint gl_InvocationID : SV_OutputControlPointID)
{
    StageInput v = ip[gl_InvocationID];
    StageOutput o;
    o.tcLightDir = v.LightDir;
    o.tcViewDir = v.ViewDir;
    o.tcTexCoord = v.texCoord;
    o.tcBtn = v.btnMatrix;
    o.tcA = v.A;
    o.tcC = v.C;
    o.tcD = v.D;
    o.tcRawAlpha = v.rawVertexAlpha;
    o.tcRefl = v.reflMatrix;
    o.gl_Position = v.gl_PositionIn;
    return o;
}
