// res/Lighting/shared/f4fs_openpbr_layer_table.hlsli -- the OpenPBR layer table (t56).
//
// One 80-byte row per layered material: coat (rgb, weight), coat shape (roughness,
// anisotropy, IOR, darkening), fuzz (rgb, weight), fuzz shape (roughness, film weight,
// film thickness, film IOR), base (specular_color as the F82 metal edge tint,
// base_diffuse_roughness).
// Light passes only. The prepass reads no table (t56 lies inside its Starfield
// texture array) and includes f4fs_openpbr_layer_record.hlsli alone.
//
// An unbound buffer has no elements: every fetch fails and the pixel lights as without layers.
//
// Host mirror: src/Materials/OpenPbrLayerTable.h.

#ifndef F4FS_OPENPBR_LAYER_TABLE_HLSLI_INCLUDED
#define F4FS_OPENPBR_LAYER_TABLE_HLSLI_INCLUDED

// Include shared/f4fs_openpbr_layer_record.hlsli first (an include here would resolve differently per compiler root).

struct F4FSOpenPbrLayerRow
{
    float4 coat;
    float4 coatShape;
    float4 fuzz;
    float4 fuzzShape;
    float4 base;
};

StructuredBuffer<F4FSOpenPbrLayerRow> g_tF4FSOpenPbrLayers : register(t56);

// False while no table is bound ([PBRMaterials.OpenPbr] bOpenPbrLayers off): callers skip the MRT7 read.
bool F4FSOpenPbrLayerTableBound()
{
    uint count, stride;
    g_tF4FSOpenPbrLayers.GetDimensions(count, stride);
    return count > 0u;
}

// Forward draws have no MRT7 record: they fetch their row by the slot stamped in b13 header.w.
bool F4FSOpenPbrLayerFetchSlot(uint slot, out F4FSOpenPbrLayerRow row)
{
    row = (F4FSOpenPbrLayerRow)0;
    if (slot == 0u)
        return false;
    uint count, stride;
    g_tF4FSOpenPbrLayers.GetDimensions(count, stride);
    if (slot - 1u >= count)
        return false;
    row = g_tF4FSOpenPbrLayers[slot - 1u];
    return true;
}

bool F4FSOpenPbrLayerFetch(float4 mrt7, out F4FSOpenPbrLayerRow row, out float coverage)
{
    row = (F4FSOpenPbrLayerRow)0;
    uint index;
    if (!F4FSOpenPbrLayerRecordDecode(mrt7, index, coverage))
        return false;
    uint count, stride;
    g_tF4FSOpenPbrLayers.GetDimensions(count, stride);
    if (index >= count)
        return false;
    row = g_tF4FSOpenPbrLayers[index];
    return true;
}

#endif
