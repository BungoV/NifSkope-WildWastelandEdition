// Vulkan and descriptor-heap routes of the bindless maps (lane VKOBJ2), included by fo4_cellbl_ps.hlsl /
// fo4_cellcsmbl_ps.hlsl in the branch that is not GL's (ww_bindless_gl.hlsli). The row's tex[i] is (image index, sampler
// index), written by the host: src/features/vkframe/vkbindless.inc turns each GL bindless handle into such a pair.
// - WW_VK_BINDLESS (with -spirv): two runtime-sized arrays in descriptor set 1, binding 0 the images and binding 1 the
//   samplers (VK_EXT_descriptor_indexing, partially bound, update after bind). DXC 1.9 cannot declare a runtime array of
//   combined image samplers (vk::combinedImageSampler applies to single resources only), so a map is an image index plus
//   a sampler index, the same pair the heap route reads.
// - otherwise: ResourceDescriptorHeap / SamplerDescriptorHeap (DXIL, Fusion Shaders).
// Needs wwDraws and wwBlRow declared first.
#ifndef WW_BINDLESS_VK_HLSLI
#define WW_BINDLESS_VK_HLSLI

#if defined(__spirv__) && defined(WW_VK_BINDLESS)
[[vk::binding(0, 1)]] Texture2D<float4> wwBlImages[];
[[vk::binding(1, 1)]] SamplerState wwBlSamplers[];

#ifdef WW_VK_BINDLESS_RED_SLOT
#define WW_BL_VK_SLOT(i) ((i) ^ 1u)	// red: every map reads its neighbour's pair (tests/vkframe/blview.sh, must FAIL)
#else
#define WW_BL_VK_SLOT(i) (i)
#endif

Texture2D<float4> wwBlMap(uint i)
{
    return wwBlImages[NonUniformResourceIndex(wwDraws[wwBlRow].tex[WW_BL_VK_SLOT(i)].x)];
}

SamplerState wwBlSmp(uint i)
{
    return wwBlSamplers[NonUniformResourceIndex(wwDraws[wwBlRow].tex[WW_BL_VK_SLOT(i)].y)];
}
#else
Texture2D<float4> wwBlMap(uint i)
{
    return ResourceDescriptorHeap[NonUniformResourceIndex(wwDraws[wwBlRow].tex[i].x)];
}

SamplerState wwBlSmp(uint i)
{
    return SamplerDescriptorHeap[NonUniformResourceIndex(wwDraws[wwBlRow].tex[i].y)];
}
#endif

#define BaseMap wwBlMap(0)
#define _BaseMap_sampler wwBlSmp(0)
#define NormalMap wwBlMap(1)
#define _NormalMap_sampler wwBlSmp(1)
#define GlowMap wwBlMap(2)
#define _GlowMap_sampler wwBlSmp(2)
#define BacklightMap wwBlMap(3)
#define _BacklightMap_sampler wwBlSmp(3)
#define SpecularMap wwBlMap(4)
#define _SpecularMap_sampler wwBlSmp(4)
#define GreyscaleMap wwBlMap(5)
#define _GreyscaleMap_sampler wwBlSmp(5)
#define EnvironmentMap wwBlMap(6)
#define _EnvironmentMap_sampler wwBlSmp(6)

#endif
