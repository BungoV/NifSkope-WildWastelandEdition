// HLSL source of features/water/fo4_waterdisp.tese (tessellation evaluation shader), HLSLBULK1 round 2 (readable
// header and the GLSL's comments: lane FO4MATCH1). One file, no variants (variants.txt: the stage has no defines).
// (notes/hlslbulk1/tools/build_ds.py): SPIRV-Cross cannot emit tessellation HLSL, so the tese's GLSL was converted as a
// vertex-shader twin (glslang -G, spirv-cross --hlsl, the same post-processing as convert.py) and wrapped here by a
// domain-shader entry: GLSL `layout(triangles, fractional_odd_spacing, ccw) in` is [domain("tri")] (the spacing and
// winding live on fo4_waterdisp_hs.hlsl), gl_TessCoord is SV_DomainLocation, gl_in[i] / tc*[i] are the OutputPatch.
// Each vertex of the split water moved by the wave spectrum's heights and chop (the water ocean math inlined below);
// the fragment reads the spectrum at the undisplaced point (wwOcBase). Fusion Shaders (F4FS) feeds the hull shader's
// patch, the loose uniforms and the ocean textures. Not wired: nothing loads this file.
// variants: #if none (the loader's defines; res/shaders/hlsl/variants.txt lists the sets built)
// profile: ds_6_0
#include "WaterBindingsVk.hlsli"
#define WW_HLSL_SRC 1	// the shared .hlsli files give their readable half (none included here: the ocean math is inlined)

// lane WATERLOOK1 step 2 (src/gl/cellocean.h): each vertex of the split water moved by the wave spectrum's heights and
// chop (water_ocean.glsl wwWaterOceanDisp, the same cascades, offsets and gains the normals read), full out to
// waterOceanDisp.x, none past .y. Positions are interpolated in clip space (exact: a linear map of the view position)
// and the move added through the projection; the fragment reads the spectrum at the undisplaced point (wwOcBase), as the
// normals are defined there (Tessendorf's Lagrangian grid).
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
    float4 waterSeamBox;
    float4 waterSeam;
    bool waterInfoOn;	// built, uploaded, and not WW_CELL_WATER_INFO_RED=off
    float4 waterInfoGrid;	// world x, y of texel (0, 0), world units a texel, world units a shore step
    float4 waterInfoLayout;	// tiles x, tiles y, texels a tile edge, tiles an atlas row
    float4 waterOceanPatch;	// the wind patches' drift, sea x, y, fresh x, y (u, wrapped by their period)
    float4 waterOceanSetB[2];	// per set: the tiles' fetch (m), resolved curvature rms (1/m), patch cell (u), 0
    float4 waterOceanWind;	// the waves' travel direction x, y (unit), the wind (m/s), the caustic net's curvature (1/m)
    float4 waterOceanSet[2];	// per set: Hs of its tiles (m), their wind sea's Hs (m), the swell / ripple Hs (m), tail mss
    int waterOceanRed;	// 2 tile (plain repeats), 4 nopatch (no wind patches), 8 novar (no lost variance), 16 nochop
    float4 waterOceanInvL[2];	// per set: 1 / tile size of cascades 0..2 (1/m), the chop lambda
    float4 cellRow[3];	// world = dot( row, ( posView, 1 ) ) (cell_lights.glsl)
    float4 waterOceanDisp;	// full out to (u), none past (u), the split's target edge (px), the most a GPU splits an edge
    bool waterOceanOn;	// the Ocean waves row
    float4 waterFlowGrid;	// world x, y of texel (0, 0), world units a texel, counts a world unit a second
    float4 waterFlowLayout;	// tiles x, tiles y, texels a tile edge, tiles an atlas row
    bool waterFlowOn;	// built, uploaded, the file has a flow plane, and not WW_CELL_WATER_INFO_RED=off
};
Texture2D<uint4> waterInfoIndex : register(t0, space0);	// RG32UI: slot + 1 (0 = uniform tile), the uniform tile's word
SamplerState _waterInfoIndex_sampler : register(s0, space0);
Texture2D<uint4> waterInfoAtlas : register(t1, space0);	// R32UI: the non-uniform tiles' words, 64 tiles a row
SamplerState _waterInfoAtlas_sampler : register(s1, space0);
Texture2D<float4> waterInfoBodies : register(t2, space0);	// RGBA32F, 256 bodies a row: height, class, flags, area (body 0 = none)
SamplerState _waterInfoBodies_sampler : register(s2, space0);
Texture2DArray<float4> waterOceanD : register(t3, space0);	// ( h, Dx, Dy, 0 ) m, mipmapped (box means)
SamplerState _waterOceanD_sampler : register(s3, space0);
Texture2D<uint4> waterFlowIndex : register(t4, space0);	// RG32UI: slot + 1 (0 = uniform tile), the uniform tile's word
SamplerState _waterFlowIndex_sampler : register(s4, space0);
Texture2D<uint4> waterFlowAtlas : register(t5, space0);	// R32UI: the non-uniform tiles' words, 64 tiles a row
SamplerState _waterFlowAtlas_sampler : register(s5, space0);
Texture2DArray<float4> waterOceanB : register(t6, space0);	// 6 layers ( sea 0-2, fresh 3-5 ): ( sx, sy, Dxx, Dyy ), mipmapped
SamplerState _waterOceanB_sampler : register(s6, space0);
Texture2DArray<float4> waterOceanC : register(t7, space0);	// ( sx^2, sy^2, sx sy, Dxy ), mipmapped
SamplerState _waterOceanC_sampler : register(s7, space0);

static float3 sTessCoord;
static float4 sGlIn[3];
static float3 tcViewDir[3];
static float3 tcLightDir[3];
static float2 tcTexCoord[3];
static float3x3 tcBtn[3];
static float4 tcA[3];
static float4 tcC[3];
static float4 tcD[3];
static float tcRawAlpha[3];
static float3x3 tcRefl[3];
static float4 gl_Position;
static float3 LightDir;
static float2 texCoord;
static float3x3 btnMatrix;
static float4 A;
static float4 C;
static float4 D;
static float rawVertexAlpha;
static float3x3 reflMatrix;
static float2 wwOcBase;
static float3 ViewDir;

// The packed word as baked: body id | shore steps << 16 | depth code << 24; the neutral word off the map
uint wwWaterInfoRaw(float2 worldXY)
{
    if (!waterInfoOn)
    {
        return 16711680u;
    }
    int2 t = int2(floor((worldXY - waterInfoGrid.xy) / waterInfoGrid.z.xx));
    int E = int(waterInfoLayout.z);
    int2 tile = t / E.xx;
    bool cond4 = t.x < 0;
    bool cond3;
    if (!cond4)
    {
        cond3 = t.y < 0;
    }
    else
    {
        cond3 = cond4;
    }
    bool cond2;
    if (!cond3)
    {
        cond2 = tile.x >= int(waterInfoLayout.x);
    }
    else
    {
        cond2 = cond3;
    }
    bool cond;
    if (!cond2)
    {
        cond = tile.y >= int(waterInfoLayout.y);
    }
    else
    {
        cond = cond2;
    }
    if (cond)
    {
        return 16711680u;
    }
    uint2 ix = waterInfoIndex.Load(int3(tile, 0)).xy;
    if (ix.x == 0u)
    {
        return ix.y;
    }
    int slot = int(ix.x) - 1;
    int perRow = int(waterInfoLayout.w);
    return waterInfoAtlas.Load(int3((int2(slot % perRow, slot / perRow) * E.xx) + (t - (tile * E.xx)), 0)).x;
}

// lane FARSEA1: 1 a band inside the bake's box, 0 at its edge (the open sea)
float wwSeamFade(float2 worldXY)
{
    float2 a = worldXY - waterSeamBox.xy;
    float2 b = waterSeamBox.zw - worldXY;
    return clamp(min(min(a.x, a.y), min(b.x, b.y)) / max(waterSeam.x, 1.0f), 0.0f, 1.0f);
}

uint wwWaterInfoWord(float2 worldXY)
{
    float2 worldXYArg2 = worldXY;
    uint w = wwWaterInfoRaw(worldXYArg2);
    bool wwh0 = (waterSeam.x <= 0.0f);
    if (wwh0 || (!waterInfoOn))
    {
        return w;
    }
    uint id = w & 65535u;
    uint sea = uint(waterSeam.y);
    if (id == 0u)
    {
        uint wwWaterInfoWordRet;
        if (waterSeam.z > 0.0f)
        {
            wwWaterInfoWordRet = sea | 4294901760u;
        }
        else
        {
            wwWaterInfoWordRet = w;
        }
        return wwWaterInfoWordRet;
    }
    if (id != sea)
    {
        return w;
    }
    float2 worldXYArg = worldXY;
    float f = wwSeamFade(worldXYArg);
    uint sh = uint(lerp(255.0f, float((w >> uint(16)) & 255u), f) + 0.5f);
    uint dc = uint(lerp(255.0f, float(w >> uint(24)), f) + 0.5f);
    return (id | (sh << uint(16))) | (dc << uint(24));
}

// the wind sea's Hs at fetch F (m): fetch-limited, capped at fully developed (the foam's law, WATERFOAMDETAIL1)
float wwOcHw(float U, float F)
{
    return min((0.001599999959580600261688232421875f * U) * sqrt(F / 9.81000041961669921875f), ((0.20999999344348907470703125f * U) * U) / 9.81000041961669921875f);
}

uint wwOcMix(inout uint x)
{
    x ^= (x >> uint(16));
    x *= 2146121005u;
    x ^= (x >> uint(15));
    x *= 2221713035u;
    x ^= (x >> uint(16));
    return x;
}

float wwOcHash01(int2 c, uint seed)
{
    uint2 u = uint2(c & int2(1023, 1023));
    uint xArg = ((u.x * 2376512323u) ^ (u.y * 3625334849u)) ^ seed;
    uint wwOcMixRet = wwOcMix(xArg);
    return float(wwOcMixRet) * 2.3283064365386962890625e-10f;
}

float wwOcValue(float2 p, uint seed)
{
    float2 i = floor(p);
    float2 f = p - i;
    f = (f * f) * (3.0f.xx - (f * 2.0f));
    int2 c = int2(i);
    int2 cArg4 = c;
    uint seedArg4 = seed;
    float a = wwOcHash01(cArg4, seedArg4);
    int2 cArg3 = c + int2(1, 0);
    uint seedArg3 = seed;
    float b = wwOcHash01(cArg3, seedArg3);
    int2 cArg2 = c + int2(0, 1);
    uint seedArg2 = seed;
    float d = wwOcHash01(cArg2, seedArg2);
    int2 cArg = c + int2(1, 1);
    uint seedArg = seed;
    float e = wwOcHash01(cArg, seedArg);
    return lerp(lerp(a, b, f.x), lerp(d, e, f.x), f.y);
}

// the wind patch factor (mean square 1): glassy where the gusts do not reach, cat's-paws where they do
float wwOcPatch(float2 P, int set)
{
    float2 pPick;
    if (set == 0)
    {
        pPick = waterOceanPatch.xy;
    }
    else
    {
        pPick = waterOceanPatch.zw;
    }
    float2 p = (P + pPick) / waterOceanSetB[set].z.xx;
    float2 pArg2 = p;
    uint seedArg6 = 625341585u;
    float2 pArg = (p * 2.0f) + float2(17.0f, 31.0f);
    uint seedArg5 = 2654435769u;
    float n = (0.64999997615814208984375f * wwOcValue(pArg2, seedArg6)) + (0.3499999940395355224609375f * wwOcValue(pArg, seedArg5));
    return 1.60209500789642333984375f * smoothstep(0.300000011920928955078125f, 0.699999988079071044921875f, n);
}

int wwOcBody(float2 P, inout float ws, out float a, out float3 gc, inout float m, inout float pw)
{
    ws = 0.0f;
    a = 0.0f;
    gc = 1.0f.xxx;
    m = 1.0f;
    pw = 0.0f;
    float tex = waterInfoGrid.z;
    float2 g = ((P - waterInfoGrid.xy) / tex.xx) - 0.5f.xx;
    float2 t0 = floor(g);
    float2 f = g - t0;
    float2 c0 = waterInfoGrid.xy + ((t0 + 0.5f.xx) * tex);
    float2 worldXYArg6 = c0;
    float2 worldXYArg5 = c0 + float2(tex, 0.0f);
    float2 worldXYArg4 = c0 + float2(0.0f, tex);
    float2 worldXYArg3 = c0 + tex.xx;
    uint4 wd = uint4(wwWaterInfoWord(worldXYArg6), wwWaterInfoWord(worldXYArg5), wwWaterInfoWord(worldXYArg4), wwWaterInfoWord(worldXYArg3));
    float4 bw = float4((1.0f - f.x) * (1.0f - f.y), f.x * (1.0f - f.y), (1.0f - f.x) * f.y, f.x * f.y);
    uint4 id = wd & uint4(65535u, 65535u, 65535u, 65535u);
    float U = waterOceanWind.z;
    // each wet corner's set and gain; the set of the largest weight wins, its corners' gains blend
    float4 cset = (-1.0f).xxxx;
    float4 cgain = 0.0f.xxxx;
    float best = -1.0f;
    int set = -1;
    for (int k = 0; k < 4; k++)
    {
        if (id[k] == 0u)
        {
            continue;
        }
        float4 row = waterInfoBodies.Load(int3(int2(int(id[k] % 256u), int(id[k] / 256u)), 0));
        int cls = clamp(int(row.y + 0.5f), 0, 2);
        if ((int(row.z + 0.5f) & 16) != 0)
        {
            continue;
        }
        int s = (cls == 0) ? 0 : 1;
        float F = (sqrt(max(row.w, 1.0f)) * tex) * 0.01428749971091747283935546875f;
        if (cls == 1)
        {
            F = min(F, 40.0f);
        }
        float4 S = waterOceanSet[s];
        float UArg2 = U;
        float FArg2 = F;
        float UArg = U;
        float FArg = waterOceanSetB[s].x;
        float hw = (S.y * wwOcHw(UArg2, FArg2)) / max(wwOcHw(UArg, FArg), 9.9999999747524270787835121154785e-07f);
        cset[k] = float(s);
        cgain[k] = clamp(sqrt((hw * hw) + (S.z * S.z)) / max(S.x, 9.9999999747524270787835121154785e-07f), 0.1500000059604644775390625f, 3.0f);
        if (bw[k] > best)
        {
            best = bw[k];
            set = s;
        }
    }
    if (set < 0)
    {
        return -1;
    }
    float4 samePart = float(set).xxxx;
    float4 same = float4(bool4(cset.x == samePart.x, cset.y == samePart.y, cset.z == samePart.z, cset.w == samePart.w));
    ws = dot(bw, same);
    a = dot(bw * same, cgain) / max(ws, 9.9999999747524270787835121154785e-07f);
    float mPick;
    if ((waterOceanRed & 4) != 0)
    {
        mPick = 1.0f;
    }
    else
    {
        float2 PArg = P;
        int setArg = set;
        mPick = wwOcPatch(PArg, setArg);
    }
    m = mPick;
    float pwPick;
    if ((waterOceanRed & 4) != 0)
    {
        pwPick = 0.0f;
    }
    else
    {
        pwPick = 1.0f - (0.699999988079071044921875f * clamp((U - 3.0f) / 7.0f, 0.0f, 1.0f));
    }
    pw = pwPick;
    gc = float3(1.0f, lerp(1.0f, m, 0.5f * pw), lerp(1.0f, m, pw));
    return set;
}

float2 wwOcOffset(int2 v, uint seed)
{
    uint xArg3 = ((uint(v.x) * 2376512323u) ^ (uint(v.y) * 3625334849u)) ^ seed;
    uint wwOcMixRet3 = wwOcMix(xArg3);
    uint h = wwOcMixRet3;
    uint xArg2 = h ^ 1759714724u;
    uint wwOcMixRet2 = wwOcMix(xArg2);
    uint h2 = wwOcMixRet2;
    return float2(float(h), float(h2)) * 2.3283064365386962890625e-10f;
}

// one cascade's ( h, Dx, Dy ) at tile uv and mip lod: the slopes' blend and offsets, so heights and normals agree
float3 wwOcCascadeD(float2 uv, float lod, float layer, uint seed)
{
    if ((waterOceanRed & 2) != 0)
    {
        return waterOceanD.SampleLevel(_waterOceanD_sampler, float3(uv, layer), lod).xyz;
    }
    float2 s = float2(uv.x, ((-0.57735025882720947265625f) * uv.x) + (1.1547005176544189453125f * uv.y)) * 2.3255813121795654296875f;
    int2 b = int2(floor(s));
    float2 f = s - float2(b);
    float z = (1.0f - f.x) - f.y;
    float3 w;
    int2 v0;
    int2 v1;
    int2 v2;
    if (z > 0.0f)
    {
        w = float3(z, f.y, f.x);
        v0 = b;
        v1 = b + int2(0, 1);
        v2 = b + int2(1, 0);
    }
    else
    {
        w = float3(-z, 1.0f - f.y, 1.0f - f.x);
        v0 = b + int2(1, 1);
        v1 = b + int2(1, 0);
        v2 = b + int2(0, 1);
    }
    int2 vArg3 = v0;
    uint seedArg9 = seed;
    float3 D0 = waterOceanD.SampleLevel(_waterOceanD_sampler, float3(uv + wwOcOffset(vArg3, seedArg9), layer), lod).xyz;
    int2 vArg2 = v1;
    uint seedArg8 = seed;
    float3 D1 = waterOceanD.SampleLevel(_waterOceanD_sampler, float3(uv + wwOcOffset(vArg2, seedArg8), layer), lod).xyz;
    int2 vArg = v2;
    uint seedArg7 = seed;
    float3 D2 = waterOceanD.SampleLevel(_waterOceanD_sampler, float3(uv + wwOcOffset(vArg, seedArg7), layer), lod).xyz;
    return (((D0 * w.x) + (D1 * w.y)) + (D2 * w.z)) * rsqrt(max(dot(w, w), 9.9999999747524270787835121154785e-07f));
}

float3 wwWaterOceanDisp(float2 P, float _step)
{
    float2 PArg2 = P;
    float wsArg;
    float aArg;
    float3 gcArg;
    float mArg;
    float pwArg;
    int wwOcBodyRet = wwOcBody(PArg2, wsArg, aArg, gcArg, mArg, pwArg);
    float ws = wsArg;
    float a = aArg;
    float3 gc = gcArg;
    float m = mArg;
    float pw = pwArg;
    int set = wwOcBodyRet;
    if (set < 0)
    {
        return 0.0f.xxx;
    }
    float2 d = waterOceanWind.xy;
    float2 e = float2(-d.y, d.x);
    float2 Pm = P * 0.01428749971091747283935546875f;
    float2 q = float2(dot(Pm, d), dot(Pm, e));
    float4 iL = waterOceanInvL[set];
    // tile texels a vertex step, times the tile's 1 / L below
    float texels = (_step * 0.01428749971091747283935546875f) * 128.0f;
    float3 H = 0.0f.xxx;
    for (int c = 0; c < 3; c++)
    {
        float il = iL[c];
        float2 uvArg = q * il;
        float lodArg = max(log2(max(texels * il, 9.9999999747524270787835121154785e-07f)), 0.0f);
        float layerArg = float((set * 3) + c);
        uint seedArg10 = 461845907u * uint(((set * 3) + c) + 1);
        H += (wwOcCascadeD(uvArg, lodArg, layerArg, seedArg10) * gc[c]);
    }
    H *= (a * ws);
    float lamPick;
    if ((waterOceanRed & 16) != 0)
    {
        lamPick = 0.0f;
    }
    else
    {
        lamPick = iL.w;
    }
    float lam = lamPick;
    return float3(((d * H.y) + (e * H.z)) * lam, H.x) / 0.01428749971091747283935546875f.xxx;
}

void wwDomainBody()
{
    float3 w = sTessCoord;
    precise float4 part15 = sGlIn[0] * w.x;
    precise float4 part14 = sGlIn[1] * w.y;
    precise float4 part13 = part15 + part14;
    precise float4 part12 = sGlIn[2] * w.z;
    precise float4 clipPart2 = part13 + part12;
    float4 clip = clipPart2;
    precise float3 part11 = tcViewDir[0] * w.x;
    precise float3 part10 = tcViewDir[1] * w.y;
    precise float3 part9 = part11 + part10;
    precise float3 part8 = tcViewDir[2] * w.z;
    precise float3 pvPart2 = part9 + part8;
    float3 pv = -pvPart2;
    LightDir = ((tcLightDir[0] * w.x) + (tcLightDir[1] * w.y)) + (tcLightDir[2] * w.z);
    texCoord = ((tcTexCoord[0] * w.x) + (tcTexCoord[1] * w.y)) + (tcTexCoord[2] * w.z);
    float3x3 btnMatrixPart4 = tcBtn[0] * w.x;
    float3x3 btnMatrixPart3 = tcBtn[1] * w.y;
    float3x3 btnMatrixPart2 = float3x3(btnMatrixPart4[0] + btnMatrixPart3[0], btnMatrixPart4[1] + btnMatrixPart3[1], btnMatrixPart4[2] + btnMatrixPart3[2]);
    float3x3 btnMatrixPart = tcBtn[2] * w.z;
    btnMatrix = float3x3(btnMatrixPart2[0] + btnMatrixPart[0], btnMatrixPart2[1] + btnMatrixPart[1], btnMatrixPart2[2] + btnMatrixPart[2]);
    A = tcA[0];
    C = ((tcC[0] * w.x) + (tcC[1] * w.y)) + (tcC[2] * w.z);
    D = tcD[0];
    rawVertexAlpha = ((w.x * tcRawAlpha[0]) + (w.y * tcRawAlpha[1])) + (w.z * tcRawAlpha[2]);
    reflMatrix = tcRefl[0];
    float4 p = float4(pv, 1.0f);
    float3 world = float3(dot(cellRow[0], p), dot(cellRow[1], p), dot(cellRow[2], p));
    wwOcBase = world.xy;
    float dist = length(pv);
    precise float fadePart = 1.0f - smoothstep(waterOceanDisp.x, waterOceanDisp.y, dist);
    float fade = fadePart;
    bool cond5;
    if (waterOceanOn)
    {
        cond5 = projectionMatrix[3].w != 1.0f;
    }
    else
    {
        cond5 = waterOceanOn;
    }
    if (cond5 && (fade > 0.0f))
    {
        // the vertices' spacing here: the split's target in pixels, or the finest a GPU splits a quad
        precise float part7 = projectionMatrix[1].y * 0.5f;
        precise float part6 = part7 * float(viewportDimensions.w);
        precise float pxuPart = part6 / max(dist, 1.0f);
        float pxu = pxuPart;
        precise float _stepPart2 = waterOceanDisp.z / pxu;
        // 4096 is GLSL's const kOcCellEdge, kept a literal (a static const changes the -O0 SPIR-V):
        precise float _stepPart = 4096.0f / waterOceanDisp.w;	// u: the cell view's water quad (src/cellview.cpp), split at most waterOceanDisp.w times
        float _step = max(_stepPart2, _stepPart);
        float2 PArg3 = world.xy;
        float _stepArg = _step;
        precise float3 wwWaterOceanDispRet = wwWaterOceanDisp(PArg3, _stepArg) * fade;
        float3 dw = wwWaterOceanDispRet;
        // world -> view: cellRow's 3 x 3 is R^T / s, so R s dw = ( sum row_k dw_k ) s^2
        precise float3 part5 = cellRow[0].xyz * dw.x;
        precise float3 part4 = cellRow[1].xyz * dw.y;
        precise float3 part3 = part5 + part4;
        precise float3 part2 = cellRow[2].xyz * dw.z;
        precise float3 part = part3 + part2;
        precise float3 dvPart = part / dot(cellRow[0].xyz, cellRow[0].xyz).xxx;
        float3 dv = dvPart;
        precise float3 pvPart = pv + dv;
        pv = pvPart;
        precise float4 clipPart = clip + mul(float4(dv, 0.0f), projectionMatrix);
        clip = clipPart;
    }
    ViewDir = -pv;
    gl_Position = clip;
}

struct StageInput
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
    WW_LOC(6) float4 gl_Position : POSITION;	// the hull shader's gl_out[].gl_Position
};

struct StageOutput
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
    WW_LOC(13) float2 wwOcBase : TEXCOORD13;	// the undisplaced world xy (u)
    precise float4 gl_Position : SV_Position;
};

struct PatchConstants
{
    float edges[3] : SV_TessFactor;
    float inside : SV_InsideTessFactor;
};

[domain("tri")]
StageOutput main(PatchConstants pc, float3 domainLocation : SV_DomainLocation, const OutputPatch<StageInput, 3> patch)
{
    sTessCoord = domainLocation;
    for (int i = 0; i < 3; i++)
    {
        tcLightDir[i] = patch[i].tcLightDir;
        tcViewDir[i] = patch[i].tcViewDir;
        tcTexCoord[i] = patch[i].tcTexCoord;
        tcBtn[i] = patch[i].tcBtn;
        tcA[i] = patch[i].tcA;
        tcC[i] = patch[i].tcC;
        tcD[i] = patch[i].tcD;
        tcRawAlpha[i] = patch[i].tcRawAlpha;
        tcRefl[i] = patch[i].tcRefl;
        sGlIn[i] = patch[i].gl_Position;
    }
    wwDomainBody();
    StageOutput o;
    o.LightDir = LightDir;
    o.ViewDir = ViewDir;
    o.texCoord = texCoord;
    o.btnMatrix = btnMatrix;
    o.A = A;
    o.C = C;
    o.D = D;
    o.rawVertexAlpha = rawVertexAlpha;
    o.reflMatrix = reflMatrix;
    o.wwOcBase = wwOcBase;
    o.gl_Position = gl_Position;
    return o;
}
