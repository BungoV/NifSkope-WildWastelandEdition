// res/Lighting/shared/f4fs_openpbr_layer_record.hlsli -- the OpenPBR layer record in MRT7.
//
// A pixel whose material has a coat or fuzz layer stores its layer-table index and
// subsurface coverage in MRT7 alpha: a = f16tof32(0x4400 + index * 64 + cov6).
// MRT7 RGB keep the subsurface tint. The value is always >= 4.0, which no other
// MRT7 record uses: subsurface a is 0..1, the skin tag is 2.0, translucency is
// below -0.5, and blend metadata has b > 0.5 (the decode gate excludes it).
//
// Host mirror: src/Materials/OpenPbrLayerTable.h. Declares no resource and no cbuffer.

#ifndef F4FS_OPENPBR_LAYER_RECORD_HLSLI_INCLUDED
#define F4FS_OPENPBR_LAYER_RECORD_HLSLI_INCLUDED

#define F4FS_OPENPBR_LAYER_CAPACITY 224u
#define F4FS_OPENPBR_LAYER_ROW_BYTES 80u
#define F4FS_OPENPBR_LAYER_SRV_REGISTER 56
#define F4FS_OPENPBR_LAYER_RECORD_BASE 0x4400u
#define F4FS_OPENPBR_LAYER_COVERAGE_STEPS 63u
#define F4FS_OPENPBR_LAYER_RECORD_MIN_ALPHA 4.0
#define F4FS_OPENPBR_LAYER_SLOT_SHIFT 8u
#define F4FS_OPENPBR_LAYER_SLOT_MASK 0xFFu

// b13 header.w: bits 0 to 7 the raw-input debug view, bits 8 to 15 the layer slot (index + 1, 0 = none).
uint F4FSOpenPbrLayerDebugView(uint headerW)
{
    return headerW & 0xFFu;
}

uint F4FSOpenPbrLayerSlot(uint headerW)
{
    return (headerW >> F4FS_OPENPBR_LAYER_SLOT_SHIFT) & F4FS_OPENPBR_LAYER_SLOT_MASK;
}

// The 224 x 64 codes fill every positive normal half from 0x4400 to 0x7BFF, so coverage has 63 steps. A coverage
// above zero keeps at least one step: rounding it to 0 would read as "no subsurface" and switch SSS off.
float F4FSOpenPbrLayerRecordAlpha(uint index, float coverage)
{
    uint cov6 = (uint)round(saturate(coverage) * (float)F4FS_OPENPBR_LAYER_COVERAGE_STEPS);
    if (coverage > 0.0)
        cov6 = max(cov6, 1u);
    return f16tof32(F4FS_OPENPBR_LAYER_RECORD_BASE + index * 64u + cov6);
}

bool F4FSOpenPbrLayerRecordDecode(float4 raw, out uint index, out float coverage)
{
    index = 0u;
    coverage = 0.0;
    if (!(raw.b <= 0.5 && raw.a >= F4FS_OPENPBR_LAYER_RECORD_MIN_ALPHA))
        return false;
    uint code = f32tof16(raw.a) - F4FS_OPENPBR_LAYER_RECORD_BASE;
    index = code >> 6u;
    coverage = (float)(code & 63u) / (float)F4FS_OPENPBR_LAYER_COVERAGE_STEPS;
    return true;
}

// The subsurface record of a pixel: RGB tint, A coverage. A layered pixel keeps
// its tint and has its coverage decoded from the layer record.
float4 F4FSPbrmSubsurfaceRecord(float4 raw)
{
    uint index;
    float coverage;
    if (F4FSOpenPbrLayerRecordDecode(raw, index, coverage))
        return float4(raw.rgb, coverage);
    return raw;
}

#endif
