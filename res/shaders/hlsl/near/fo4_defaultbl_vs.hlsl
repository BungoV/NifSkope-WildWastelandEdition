// HLSL port of fo4_defaultbl.vert (vert shader), HLSLBULK1 round 2, written from the converted twin core/fo4_default_vs.hlsl
// (notes/hlslbulk1/tools/build_bl.py): bindless. The GLSL reads the material's seven maps through
// sampler2D( wwDraws[wwDrawIdx].tex[i] ) (GL_ARB_bindless_texture handles); here the same row (struct WwDraw below) holds
// descriptor-heap indices and the maps are ResourceDescriptorHeap / SamplerDescriptorHeap lookups (SM 6.6; Vulkan
// descriptor indexing), NonUniformResourceIndex because a multi-draw's pixels read different rows. The row index (wwDrawBase + the draw's base instance, the BaseInstance builtin; SM 6.8 SV_StartInstanceLocation) goes to the pixel stage flat. Everything else is the
// twin's code. Fusion Shaders (F4FS) fills the row buffer and the heaps. Not wired: nothing loads this file.
// profile: vs_6_8
#include "NearBindingsVk.hlsli"
cbuffer skinningUniforms : register(b0, space0)
{
    row_major float3x4 boneTransforms[256] : packoffset(c0);
};

cbuffer globalUniforms : register(b1, space0)
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
    float4 wwTreeWind5;
    float4 wwTreeWind3;
    float4 wwTreeWind4;
    float4 wwTreeWind2;
    float4 wwTreeWind;
    bool farInst;
    bool farStreamOn;
    float3 farBakeK;
    float4 farVcAo;
    float4 wwGrassWind2;
    float4 wwGrassWind;
    uint wwDrawBase;
    bool wwBlUniformRow;	// an instanced far ring: the base instance is a placement row, the row reads wwDrawBase alone
    float4 farSwayBlock;
    float farSwayAmp;
    bool lodTreeAnim;
    float farSwayTime;
    float lodViewCode;	// lane LODLIVE1: the LOD coloration's band + 1 of a shape (0 = untinted), renderer.cpp
    float4 vertexColorOverride;	// components greater than zero replace the vertex color
};
// the per-draw row (src/features/near/cellbindless.h WwBl::Row, std430, 176 bytes): the draw's matrices and the
// material's seven 2D maps as handles. A GL handle is a 64-bit texture+sampler pair; here .x is the index of the texture
// in ResourceDescriptorHeap and .y the index of its sampler in SamplerDescriptorHeap (the row is written by the host).
struct WwDraw
{
    row_major float4x4 mv;
    float4 nm[3];
    uint2 tex[8];
};
StructuredBuffer<WwDraw> wwDraws : register(t48, space0);	// GL binding 12
static uint wwBlRow;	// wwDrawIdx: the vertex stage's row, flat to the pixel stage
#define modelViewMatrix (wwDraws[wwBlRow].mv)
#define normalMatrix float3x3(wwDraws[wwBlRow].nm[0].xyz, wwDraws[wwBlRow].nm[1].xyz, wwDraws[wwBlRow].nm[2].xyz)
Buffer<float4> farStream : register(t0, space0);

static float4 gl_Position;
static int gl_VertexID;
static float4 boneWeights[2];
static float3 vertexPosition;
static float3 normalVector;
static float3 tangentVector;
static float3 bitangentVector;
static float4 vertexColor;
static float2 multiTexCoord0;
static float wwFarSkyVis;
static float wwFarAo;
static float4 farInstRow0;
static float4 farInstRow1;
static float4 farInstRow2;
static float4 farInstExtra;
static float2 farLadder;
static float2 farLadderIn;
static float2 texCoord;
static float3x3 btnMatrix;
static float3x3 reflMatrix;
static float3 ViewDir;
static float3 LightDir;
static float4 A;
static float4 C;
static float rawVertexAlpha;
static float4 D;

struct SPIRV_Cross_Input
{
    float3 vertexPosition : TEXCOORD0;
    float4 vertexColor : TEXCOORD1;
    float3 normalVector : TEXCOORD2;
    float3 tangentVector : TEXCOORD3;
    float3 bitangentVector : TEXCOORD4;
    float4 boneWeights[2] : TEXCOORD5;
    float2 multiTexCoord0 : TEXCOORD7;
    float2 farLadderIn : TEXCOORD9;
    float4 farInstRow0 : TEXCOORD10;
    float4 farInstRow1 : TEXCOORD11;
    float4 farInstRow2 : TEXCOORD12;
    float4 farInstExtra : TEXCOORD13;
    uint gl_VertexID : SV_VertexID;
#ifdef __spirv__
    [[vk::builtin("BaseInstance")]] uint wwBaseInstance : BASEINSTANCE;
#else
    uint wwBaseInstance : SV_StartInstanceLocation;
#endif
};

struct SPIRV_Cross_Output
{
    float3 LightDir : TEXCOORD0;
    float3 ViewDir : TEXCOORD1;
    float2 texCoord : TEXCOORD2;
    float3x3 btnMatrix : TEXCOORD3;
    nointerpolation float4 A : TEXCOORD6;
    float4 C : TEXCOORD7;
    nointerpolation float4 D : TEXCOORD8;
    float rawVertexAlpha : TEXCOORD9;
    nointerpolation float3x3 reflMatrix : TEXCOORD10;
    nointerpolation float2 farLadder : TEXCOORD13;
    float wwFarSkyVis : TEXCOORD14;
    float wwFarAo : TEXCOORD15;
    nointerpolation uint wwBlRow : TEXCOORD16;
    precise float4 gl_Position : SV_Position;
};

void boneTransform(inout float4 v, inout float3 n, inout float3 t, inout float3 b)
{
    float3x4 mTmp = float3x4(0.0f.xxxx, 0.0f.xxxx, 0.0f.xxxx);
    float wSum = 0.0f;
    for (int i = 0; i < 8; i++)
    {
        float bw = boneWeights[i >> 2][i & 3];
        if (!(bw > 0.0f))
        {
            break;
        }
        int bone = int(bw) & 255;
        float w = frac(bw);
        float3x4 tmp87 = boneTransforms[bone] * w;
        mTmp = float3x4(mTmp[0] + tmp87[0], mTmp[1] + tmp87[1], mTmp[2] + tmp87[2]);
        wSum += w;
    }
    if (wSum > 0.0f)
    {
        v = float4(mul(mTmp, v) / wSum.xxx, 1.0f);
        float3x3 r = float3x3(mTmp[0].xyz, mTmp[1].xyz, mTmp[2].xyz);
        n = mul(r, n);
        t = mul(r, t);
        b = mul(r, b);
    }
}

float3 wwTreeWindApply(float3 p, float3 nrm, float bitLen, float4x4 mv)
{
    float tmp144;
    if (bitLen > 1.5f)
    {
        tmp144 = clamp(bitLen - 2.0f, 0.0f, 1.0f);
    }
    else
    {
        tmp144 = 0.0f;
    }
    float a = tmp144;
    float3 n = nrm * rsqrt(max(dot(nrm, nrm), 1.0000000031710768509710513471353e-30f));
    float s = dot(n, wwTreeWind5.xyz);
    float base = (3.0f * s) + wwTreeWind3.w;
    float F = wwTreeWind4.z * 20.0f;
    float dist = length(mul(float4(wwTreeWind3.xyz, 1.0f), mv).xyz) / length(mv[0].xyz);
    float A_1 = (wwTreeWind4.y * clamp((4600.0f - dist) * 0.001000000047497451305389404296875f, 0.0f, 1.0f)) * 0.0350000001490116119384765625f;
    float halfR = (wwTreeWind2.y - wwTreeWind2.x) * 0.5f;
    float sp = wwTreeWind2.x + (halfR * sin((wwTreeWind.w * ((F * wwTreeWind2.z) * 5.0f)) + base));
    float3 off = (p + ((n * (sp * A_1)) * a)) - wwTreeWind3.xyz;
    float h = ((off.x + off.y) + off.z) / wwTreeWind4.x;
    float c = sin(((wwTreeWind.w * wwTreeWind2.z) * F) + ((h * (-1.5f)) + wwTreeWind3.w));
    float g = wwTreeWind2.x + (halfR * (((0.300000011920928955078125f * sin(3.1415927410125732421875f * c)) + sin(6.283185482025146484375f * c)) + 2.0f));
    float b = ((h * g) / 3.0f) * a;
    float3 q = off + float3(cos(wwTreeWind.x) * b, sin(wwTreeWind.x) * b, 0.0f);
    float ql = length(q);
    float3 tmp328;
    if (ql > 0.0f)
    {
        tmp328 = q * (length(off) / ql);
    }
    else
    {
        tmp328 = q;
    }
    return tmp328 + wwTreeWind3.xyz;
}

void vert_main()
{
    float4 v = float4(vertexPosition, 1.0f);
    float3 n = normalVector;
    float3 t = tangentVector;
    float3 b = bitangentVector;
    if ((boneWeights[0].x > 0.0f) && (doSkinning != 0u))
    {
        float4 param = v;
        float3 param_1 = n;
        float3 param_2 = t;
        float3 param_3 = b;
        boneTransform(param, param_1, param_2, param_3);
        v = param;
        n = param_1;
        t = param_2;
        b = param_3;
    }
    float4 vc = vertexColor;
    float2 uv0 = multiTexCoord0;
    float2 farXY = vertexPosition.xy;
    wwFarSkyVis = 1.0f;
    wwFarAo = 1.0f;
    if (farInst)
    {
        float3x3 Mt = float3x3(float3(farInstRow0.xyz), float3(farInstRow1.xyz), float3(farInstRow2.xyz));
        float4 tmp437 = v;
        float4 tmp440 = v;
        float4 tmp443 = v;
        float3 tmp445 = float3(dot(farInstRow0, tmp437), dot(farInstRow1, tmp440), dot(farInstRow2, tmp443));
        v.x = tmp445.x;
        v.y = tmp445.y;
        v.z = tmp445.z;
        n = mul(Mt, n);
        t = mul(Mt, t);
        b = mul(Mt, b);
        farXY = v.xy;
        float e = farInstExtra.w;
        if (e > 1.5f)
        {
            uv0.x = farInstExtra.x - uv0.x;
            e -= 2.0f;
        }
        if (farStreamOn)
        {
            float2 ch = farStream.Load(asint(farInstExtra.y) + gl_VertexID).xy;
            float ao = 1.0f - ((1.0f - ch.x) * farBakeK.x);
            wwFarAo = (farBakeK.z > 0.5f) ? 1.0f : ao;
            float4 tmp521 = vc;
            float3 tmp523 = tmp521.xyz * (((farBakeK.z > 0.5f) ? ao : 1.0f) * e);
            vc.x = tmp523.x;
            vc.y = tmp523.y;
            vc.z = tmp523.z;
            wwFarSkyVis = 1.0f - ((1.0f - ch.y) * farBakeK.y);
        }
        else
        {
            bool tmp542 = farVcAo.z > 2.5f;
            bool tmp548;
            if (tmp542)
            {
                tmp548 = farVcAo.w < 0.5f;
            }
            else
            {
                tmp548 = tmp542;
            }
            if (tmp548)
            {
                wwFarAo = 1.0f - ((1.0f - clamp(vc.x * farInstExtra.y, 0.0f, 1.0f)) * farVcAo.x);
                vc.x = 1.0f;
                vc.y = 1.0f;
                vc.z = 1.0f;
            }
            else
            {
                float4 tmp576 = vc;
                float3 tmp578 = tmp576.xyz * float3(farInstExtra.yz, e);
                vc.x = tmp578.x;
                vc.y = tmp578.y;
                vc.z = tmp578.z;
            }
        }
    }
    else
    {
        bool tmp588 = farVcAo.z > 0.5f;
        bool tmp594;
        if (tmp588)
        {
            tmp594 = farVcAo.w < 0.5f;
        }
        else
        {
            tmp594 = tmp588;
        }
        if (tmp594)
        {
            if (farVcAo.z < 1.5f)
            {
                wwFarAo = 1.0f - ((1.0f - vc.x) * farVcAo.x);
            }
            else
            {
                wwFarSkyVis = 1.0f - ((1.0f - vc.x) * farVcAo.y);
            }
            vc.x = 1.0f;
            vc.y = 1.0f;
            vc.z = 1.0f;
        }
    }
    if (wwGrassWind2.w > 0.5f)
    {
        float L = length(b);
        float tmp635;
        if (L > 2.0f)
        {
            tmp635 = L - 1024.0f;
        }
        else
        {
            tmp635 = 0.0f;
        }
        float off = tmp635;
        b *= rsqrt(dot(b, b));
        n *= rsqrt(dot(n, n));
        t *= rsqrt(dot(t, t));
        float p = (wwGrassWind.w - off) * wwGrassWind2.z;
        float half_range = (wwGrassWind2.y - wwGrassWind2.x) * 0.5f;
        float sp = sin(p);
        float cp = cos(p);
        float w = sin(sp * 3.1415927410125732421875f) + sin(sp * 6.283185482025146484375f);
        w = (w * 0.300000011920928955078125f) + (cos(cp * 3.1415927410125732421875f) * 0.20000000298023223876953125f);
        w += 1.0f;
        w = (w * half_range) + wwGrassWind2.x;
        w *= ((vertexColor.w * vertexColor.w) * 0.5f);
        float3 d = float3(cos(wwGrassWind.x), sin(wwGrassWind.x), 0.0f);
        if (wwGrassWind2.w < 1.5f)
        {
            n += (d * w);
            n *= rsqrt(dot(n, n));
            t += (d * w);
            t *= rsqrt(dot(t, t));
            b += (d * w);
            b *= rsqrt(dot(b, b));
        }
        float4 tmp764 = v;
        float3 tmp766 = (d * w) + tmp764.xyz;
        v.x = tmp766.x;
        v.y = tmp766.y;
        v.z = tmp766.z;
        float3 tmp778 = exp2(log2(vertexColor.xyz) * 2.2000000476837158203125f);
        vc.x = tmp778.x;
        vc.y = tmp778.y;
        vc.z = tmp778.z;
    }
    if (wwTreeWind2.w > 0.5f)
    {
        float3 param_4 = v.xyz;
        float3 param_5 = normalVector;
        float param_6 = length(bitangentVector);
        float4x4 param_7 = modelViewMatrix;
        float3 tmp802 = wwTreeWindApply(param_4, param_5, param_6, param_7);
        v.x = tmp802.x;
        v.y = tmp802.y;
        v.z = tmp802.z;
    }
    v = mul(v, modelViewMatrix);
    float3 farRel = mul(v.xyz - farSwayBlock.xyz, transpose(viewMatrix));
    bool tmp829 = (farSwayAmp > 0.0f) && lodTreeAnim;
    bool tmp842;
    if (tmp829)
    {
        tmp842 = max(abs(farRel.x), abs(farRel.y)) > farSwayBlock.w;
    }
    else
    {
        tmp842 = tmp829;
    }
    if (tmp842)
    {
        float ph = dot(farXY, float2(0.00209999992512166500091552734375f, 0.001700000022538006305694580078125f));
        float3 w_1 = float3(sin((farSwayTime * 1.2999999523162841796875f) + ph), cos((farSwayTime * 0.89999997615814208984375f) + (ph * 1.7000000476837158203125f)) * 0.60000002384185791015625f, 0.0f);
        float4 tmp879 = v;
        float3 tmp881 = tmp879.xyz + mul(w_1 * (farSwayAmp * vertexColor.w), viewMatrix);
        v.x = tmp881.x;
        v.y = tmp881.y;
        v.z = tmp881.z;
    }
    float4 tmp896 = mul(v, projectionMatrix);
    gl_Position = tmp896;
    float2 tmp902;
    if (farInst)
    {
        float tmp911;
        if (farLadderIn.y > 0.5f)
        {
            tmp911 = farLadderIn.y;
        }
        else
        {
            tmp911 = lodViewCode;
        }
        tmp902 = float2(farLadderIn.x, tmp911);
    }
    else
    {
        tmp902 = float2(0.0f, lodViewCode);
    }
    farLadder = tmp902;
    texCoord = uv0;
    btnMatrix[2] = normalize(mul(normalMatrix, n));
    btnMatrix[1] = normalize(mul(normalMatrix, t));
    btnMatrix[0] = normalize(mul(normalMatrix, b));
    reflMatrix = envMapRotation;
    reflMatrix[0].z *= (-1.0f);
    reflMatrix[1].z *= (-1.0f);
    reflMatrix[2].z *= (-1.0f);
    if (projectionMatrix[3].w == 1.0f)
    {
        ViewDir = float3(0.0f, 0.0f, 1.0f);
    }
    else
    {
        ViewDir = -v.xyz;
    }
    LightDir = lightSourcePosition[0].xyz;
    A = float4(sqrt(lightSourceAmbient.xyz) * 0.375f, toneMapScale);
    bool4 tmp1001 = bool4(vertexColorOverride.x > 0.0f.xxxx.x, vertexColorOverride.y > 0.0f.xxxx.y, vertexColorOverride.z > 0.0f.xxxx.z, vertexColorOverride.w > 0.0f.xxxx.w);
    C = float4(tmp1001.x ? vertexColorOverride.x : vc.x, tmp1001.y ? vertexColorOverride.y : vc.y, tmp1001.z ? vertexColorOverride.z : vc.z, tmp1001.w ? vertexColorOverride.w : vc.w);
    rawVertexAlpha = vertexColor.w;
    D = float4(sqrt(lightSourceDiffuse[0].xyz), brightnessScale);
}

SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    gl_VertexID = int(stage_input.gl_VertexID);
    wwBlRow = wwDrawBase + (wwBlUniformRow ? 0u : stage_input.wwBaseInstance);
    boneWeights = stage_input.boneWeights;
    vertexPosition = stage_input.vertexPosition;
    normalVector = stage_input.normalVector;
    tangentVector = stage_input.tangentVector;
    bitangentVector = stage_input.bitangentVector;
    vertexColor = stage_input.vertexColor;
    multiTexCoord0 = stage_input.multiTexCoord0;
    farInstRow0 = stage_input.farInstRow0;
    farInstRow1 = stage_input.farInstRow1;
    farInstRow2 = stage_input.farInstRow2;
    farInstExtra = stage_input.farInstExtra;
    farLadderIn = stage_input.farLadderIn;
    vert_main();
    SPIRV_Cross_Output stage_output;
    stage_output.gl_Position = gl_Position;
    stage_output.wwBlRow = wwBlRow;
    stage_output.wwFarSkyVis = wwFarSkyVis;
    stage_output.wwFarAo = wwFarAo;
    stage_output.farLadder = farLadder;
    stage_output.texCoord = texCoord;
    stage_output.btnMatrix = btnMatrix;
    stage_output.reflMatrix = reflMatrix;
    stage_output.ViewDir = ViewDir;
    stage_output.LightDir = LightDir;
    stage_output.A = A;
    stage_output.C = C;
    stage_output.rawVertexAlpha = rawVertexAlpha;
    stage_output.D = D;
    return stage_output;
}
