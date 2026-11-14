// OpenPBR coat and fuzz layers for the direct-light passes: reads the pixel's
// layer-table row (t56, through the MRT7 layer record) and layers it over the
// base specular and diffuse the caller already computed. A pixel with no
// valid record leaves everything unchanged.
#ifndef F4FS_OPENPBR_LAYERS_HLSLI_INCLUDED
#define F4FS_OPENPBR_LAYERS_HLSLI_INCLUDED

#include "openpbr_brdf.hlsli"
#include "shared/f4fs_openpbr_layer_record.hlsli"
#include "shared/f4fs_openpbr_layer_table.hlsli"
#include "openpbr_film.hlsli"

struct OpenPBR_Layers
{
    bool valid;
    float4 coat;
    float4 coatShape;
    float4 fuzz;
    float4 fuzzShape;
    float4 base;
};

OpenPBR_Layers OpenPBR_LayersFromRow(F4FSOpenPbrLayerRow row)
{
    OpenPBR_Layers layers;
    layers.valid = true;
    layers.coat = row.coat;
    layers.coatShape = row.coatShape;
    layers.fuzz = row.fuzz;
    layers.fuzzShape = row.fuzzShape;
    layers.base = row.base;
    return layers;
}

OpenPBR_Layers OpenPBR_LayersLoad(float4 mrt7)
{
    OpenPBR_Layers layers = (OpenPBR_Layers)0;
    F4FSOpenPbrLayerRow row;
    float coverage;
    [branch] if (F4FSOpenPbrLayerFetch(mrt7, row, coverage))
        layers = OpenPBR_LayersFromRow(row);
    return layers;
}

// Forward draws: the row named by the slot in b13 header.w.
OpenPBR_Layers OpenPBR_LayersLoadSlot(uint slot)
{
    OpenPBR_Layers layers = (OpenPBR_Layers)0;
    F4FSOpenPbrLayerRow row;
    [branch] if (F4FSOpenPbrLayerFetchSlot(slot, row))
        layers = OpenPBR_LayersFromRow(row);
    return layers;
}

// base_diffuse_roughness for the EON lobe: the row's value, or the spec default without one.
float OpenPBR_LayersDiffuseRoughness(OpenPBR_Layers layers)
{
    return layers.valid ? saturate(layers.base.w) : OPENPBR_BASE_DIFFUSE_ROUGHNESS;
}

// The edge tint each pixel takes: specular_color on its metallic share, white on its dielectric share, which the
// prepass already tinted through F0. F82 is affine in the tint, so lerp(1, tint, metal) is exactly the metal share's
// correction weighted by metalness. White (no row, or an untinted material) is the identity.
float3 OpenPBR_LayersEdgeTint(OpenPBR_Layers layers, float metal)
{
    return layers.valid ? lerp(1.0.xxx, saturate(layers.base.rgb), saturate(metal)) : 1.0.xxx;
}

// A direct lobe evaluated with Schlick on F0, rescaled to F82 with this edge tint. White is the identity.
void OpenPBR_ApplyEdgeTint(float3 F0, float3 edgeTint, float VdotH, inout float3 specular)
{
    [branch] if (any(edgeTint != 1.0.xxx))
        specular *= OpenPBR_F82Metal(F0, edgeTint, VdotH) / max(OpenPBR_F_Schlick(F0, VdotH), 1e-4.xxx);
}

// Direct base specular with F82. Call before the film and the coat, like OpenPBR_LayersApplyFilm.
void OpenPBR_LayersApplyEdgeTint(OpenPBR_Layers layers, float3 F0, float metal, float VdotH, inout float3 specular)
{
    OpenPBR_ApplyEdgeTint(F0, OpenPBR_LayersEdgeTint(layers, metal), VdotH, specular);
}

float OpenPBR_LayersBaseRoughness(OpenPBR_Layers layers, float baseRoughness)
{
    [branch] if (layers.valid && layers.coat.w > 0.0)
        return OpenPBR_CoatRoughening(baseRoughness, layers.coatShape.x, layers.coat.w, layers.coatShape.z);
    return baseRoughness;
}

// specular is the base lobe times N.L and the light scale; diffuseWeight multiplies the base diffuse.
void OpenPBR_LayersApply(
    OpenPBR_Layers layers, float3 N, float3 V, float3 L, float3 H,
    float3 albedo, float baseRoughness,
    inout float3 specular, inout float3 diffuseWeight)
{
    [branch] if (!layers.valid)
        return;
    float NdotV = saturate(dot(N, V));
    float NdotL = saturate(dot(N, L));
    float NdotH = saturate(dot(N, H));
    float VdotH = saturate(dot(V, H));
    float3 below = specular;
    float3 belowDiffuse = diffuseWeight;
    float3 added = 0.0.xxx;
    [branch] if (layers.coat.w > 0.0)
    {
        OpenPBR_CoatResult coat = OpenPBR_Coat(layers.coat.rgb, layers.coat.w,
            layers.coatShape.x, layers.coatShape.z, layers.coatShape.w,
            albedo, baseRoughness, NdotL, NdotV, NdotH, VdotH);
        below *= coat.specularTransmission;
        belowDiffuse *= coat.diffuseTransmission;
        added += coat.specular;
    }
    [branch] if (layers.fuzz.w > 0.0)
    {
        OpenPBR_FuzzResult fuzz = OpenPBR_Fuzz(layers.fuzz.rgb, layers.fuzz.w, layers.fuzzShape.x, N, V, L);
        below *= fuzz.transmission;
        belowDiffuse *= fuzz.transmission;
        added = added * fuzz.transmission + fuzz.specular;
    }
    specular = below + added;
    diffuseWeight = belowDiffuse;
}

// Thin film on the base specular: fuzz shape y/z/w = film weight, thickness (micrometres), IOR. The table row
// holds a weight only while [PBRMaterials.OpenPbr] bOpenPbrThinFilm is on, so a pixel without film skips this.
// Call it before OpenPBR_LayersApply: the film sits on the base, under the coat and fuzz, so their lobes stay untinted.
void OpenPBR_LayersApplyFilm(OpenPBR_Layers layers, float3 F0, float VdotH, inout float3 specular)
{
    [branch] if (layers.valid && layers.fuzzShape.y > 0.0)
        specular *= OpenPbrThinFilmScale(F0, VdotH, layers.fuzzShape.z, layers.fuzzShape.w, layers.fuzzShape.y);
}

// What the coat and fuzz pass to the base's indirect (environment) diffuse: OpenPBR's
// f = f_coat + (1 - E_coat) f_base, with the coat's absorption and darkening, so coated
// surfaces in shade do not return more than the light they receive. 1 without a record.
float3 OpenPBR_LayersIndirectDiffuseWeight(OpenPBR_Layers layers, float NdotV, float3 albedo, float baseRoughness)
{
    float3 weight = 1.0.xxx;
    [branch] if (!layers.valid)
        return weight;
    [branch] if (layers.coat.w > 0.0)
    {
        float w = saturate(layers.coat.w);
        float coatAlbedo = OpenPBR_CoatAlbedo(layers.coatShape.z, layers.coatShape.x, NdotV);
        float3 covered = (1.0 - coatAlbedo) * OpenPBR_CoatAbsorption(layers.coat.rgb, layers.coatShape.z, NdotV, NdotV);
        float3 dark = OpenPBR_CoatDarkening(albedo, layers.coatShape.z, baseRoughness, NdotV);
        weight *= lerp(1.0.xxx, covered * lerp(1.0.xxx, dark, saturate(layers.coatShape.w)), w);
    }
    [branch] if (layers.fuzz.w > 0.0)
        weight *= 1.0 - saturate(layers.fuzz.w * OpenPBR_FuzzCoefficients(layers.fuzzShape.x, NdotV).z);
    return weight;
}

// Ambient counterpart for environment-lit specular. `environment` is the radiance the base lobe
// reflects (before its split-sum weight); the coat reflects the same radiance with its own albedo.
// The base specular is attenuated by what the coat and fuzz let through; the caller dims the base's
// indirect diffuse with OpenPBR_LayersIndirectDiffuseWeight. Fuzz sheen is not added here.
float3 OpenPBR_LayersApplyIndirect(
    OpenPBR_Layers layers, float NdotV, float3 environment, float3 baseSpecular)
{
    [branch] if (!layers.valid)
        return baseSpecular;
    float3 below = baseSpecular;
    float3 added = 0.0.xxx;
    [branch] if (layers.coat.w > 0.0)
    {
        float w = saturate(layers.coat.w);
        float albedo = OpenPBR_CoatAlbedo(layers.coatShape.z, layers.coatShape.x, NdotV);
        float3 covered = (1.0 - albedo) * OpenPBR_CoatAbsorption(layers.coat.rgb, layers.coatShape.z, NdotV, NdotV);
        below *= lerp(1.0.xxx, covered, w);
        added += w * albedo * environment;
    }
    [branch] if (layers.fuzz.w > 0.0)
        below *= 1.0 - saturate(layers.fuzz.w * OpenPBR_FuzzCoefficients(layers.fuzzShape.x, NdotV).z);
    return below + added;
}

#endif
