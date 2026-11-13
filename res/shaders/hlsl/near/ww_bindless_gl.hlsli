// GL route of the bindless maps (lane HLSLFIX3), included by fo4_cellbl_ps.hlsl / fo4_cellcsmbl_ps.hlsl when the GL build
// defines WW_GL_BINDLESS (tests/hlsllive/build_spv.py adds it, with -Vd, for a source that names it). The row's tex[i] is then
// the GL_ARB_bindless_texture 64-bit handle (lo, hi), as in the GLSL sampler2D( wwDraws[wwDrawIdx].tex[i] ): it becomes a
// sampled image through SPV_NV_bindless_texture (OpConvertUToSampledImageNV), which GL_ARB_gl_spirv takes on NVIDIA; the
// patcher (spvgl.py) adds the OpSamplerImageAddressingModeNV 64 the extension requires. Vulkan and DXIL (Fusion Shaders) keep
// the descriptor-heap route of the including file. The maps are a small struct whose methods have the Texture2D names the
// converted body calls (Sample, SampleBias, SampleLevel, SampleGrad); a map's sampler is folded into its handle, so the
// _<map>_sampler names are a dummy uint. Needs wwDraws and wwBlRow declared first.
#ifndef WW_BINDLESS_GL_HLSLI
#define WW_BINDLESS_GL_HLSLI

#define WW_BL_LIT(n) vk::Literal<vk::integral_constant<uint, n> >
// OpTypeImage float 2D, depth 0, arrayed 0, ms 0, sampled 1, format Unknown (a sampler2D's image)
typedef vk::SpirvOpaqueType</* OpTypeImage */ 25, float, WW_BL_LIT(1), WW_BL_LIT(0), WW_BL_LIT(0), WW_BL_LIT(0), WW_BL_LIT(1), WW_BL_LIT(0)> WwBlImage2D;
typedef vk::SpirvOpaqueType</* OpTypeSampledImage */ 27, WwBlImage2D> WwBlSampled2D;

[[vk::ext_extension("SPV_NV_bindless_texture")]]
[[vk::ext_capability(/* BindlessTextureNV */ 5390)]]
[[vk::ext_instruction(/* OpConvertUToSampledImageNV */ 5395)]]
WwBlSampled2D wwBlHandle(uint2 h);

// image operands: Bias 0x1, Lod 0x2, Grad 0x4
[[vk::ext_instruction(/* OpImageSampleImplicitLod */ 87)]]
float4 wwBlSampleImplicit(WwBlSampled2D s, float2 uv);
[[vk::ext_instruction(/* OpImageSampleImplicitLod */ 87)]]
float4 wwBlSampleBiasOp(WwBlSampled2D s, float2 uv, [[vk::ext_literal]] uint ops, float bias);
[[vk::ext_instruction(/* OpImageSampleExplicitLod */ 88)]]
float4 wwBlSampleLodOp(WwBlSampled2D s, float2 uv, [[vk::ext_literal]] uint ops, float lod);
[[vk::ext_instruction(/* OpImageSampleExplicitLod */ 88)]]
float4 wwBlSampleGradOp(WwBlSampled2D s, float2 uv, [[vk::ext_literal]] uint ops, float2 dx, float2 dy);

#ifdef WW_GL_BINDLESS_RED_SLOT
#define WW_BL_SLOT(i) ((i) ^ 1u)	// red: every map reads its neighbour's handle (tests/hlsllive red, must FAIL A4)
#else
#define WW_BL_SLOT(i) (i)
#endif

template<typename T> struct WwBlTexT
{
    uint slot;
    float4 Sample(uint s, float2 uv)
    {
        return wwBlSampleImplicit(wwBlHandle(wwDraws[wwBlRow].tex[WW_BL_SLOT(slot)]), uv);
    }
    float4 SampleBias(uint s, float2 uv, float bias)
    {
        return wwBlSampleBiasOp(wwBlHandle(wwDraws[wwBlRow].tex[WW_BL_SLOT(slot)]), uv, 0x1, bias);
    }
    float4 SampleLevel(uint s, float2 uv, float lod)
    {
        return wwBlSampleLodOp(wwBlHandle(wwDraws[wwBlRow].tex[WW_BL_SLOT(slot)]), uv, 0x2, lod);
    }
    float4 SampleGrad(uint s, float2 uv, float2 dx, float2 dy)
    {
        return wwBlSampleGradOp(wwBlHandle(wwDraws[wwBlRow].tex[WW_BL_SLOT(slot)]), uv, 0x4, dx, dy);
    }
};
typedef WwBlTexT<float4> WwBlTex;

WwBlTex wwBlTex(uint i)
{
    WwBlTex t;
    t.slot = i;
    return t;
}

#define BaseMap wwBlTex(0)
#define _BaseMap_sampler 0u
#define NormalMap wwBlTex(1)
#define _NormalMap_sampler 0u
#define GlowMap wwBlTex(2)
#define _GlowMap_sampler 0u
#define BacklightMap wwBlTex(3)
#define _BacklightMap_sampler 0u
#define SpecularMap wwBlTex(4)
#define _SpecularMap_sampler 0u
#define GreyscaleMap wwBlTex(5)
#define _GreyscaleMap_sampler 0u
#define EnvironmentMap wwBlTex(6)
#define _EnvironmentMap_sampler 0u

// a shared include that takes a map as a Texture2D<float4> + SamplerState parameter (cell_reflprobe.hlsli's atlas) is
// included with `#define Texture2D WwBlTexT` and `#define SamplerState uint` around it (the including file does that).

#endif
