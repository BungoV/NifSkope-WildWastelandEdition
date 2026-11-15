// res/Lighting/shared/f4fs_pbr_aux_flags.hlsli -- which surface owns MRT6/MRT7.
//
// A G-buffer draw that writes MRT6 (emission) or MRT7 (PBRM metadata) says so in
// the alpha of o2 (RT25.w): every opaque G-buffer draw writes it, and every
// engine blend state masks it out (RGB write mask), so it always belongs to the
// front opaque surface. The byte is 1 + (roughness6 << 2 | flags); 1..252 is
// ours, while the engine writes saturate(material id) = 0 or 255, which reads
// as "wrote neither". The resolve (pbr_aux_resolve.hlsl) zeroes MRT6/MRT7
// wherever the front surface did not write them, so the draws that have nothing
// to write leave both targets unbound. The six bits beside the flags keep the
// raw authored roughness for the material-input debug view.
//
// Host mirror: src/Materials/PbrAuxOutputs.h. Declares no resource and no cbuffer.

#ifndef F4FS_PBR_AUX_FLAGS_HLSLI_INCLUDED
#define F4FS_PBR_AUX_FLAGS_HLSLI_INCLUDED

#define F4FS_PBR_AUX_EMISSIVE 1u
#define F4FS_PBR_AUX_METADATA 2u
#define F4FS_PBR_AUX_FLAG_BIAS 1u
#define F4FS_PBR_AUX_ROUGHNESS_STEPS 62u
#define F4FS_PBR_AUX_MAX_BYTE (F4FS_PBR_AUX_FLAG_BIAS + ((F4FS_PBR_AUX_ROUGHNESS_STEPS << 2u) | 3u))

float F4FSEncodePbrAuxFlags(uint flags, float roughness)
{
    uint steps = (uint)round(saturate(roughness) * (float)F4FS_PBR_AUX_ROUGHNESS_STEPS);
    return (float)(F4FS_PBR_AUX_FLAG_BIAS + ((steps << 2u) | (flags & 3u))) * (1.0 / 255.0);
}

uint F4FSPbrAuxByte(float encoded)
{
    return (uint)round(saturate(encoded) * 255.0);
}

bool F4FSPbrAuxByteValid(uint byte)
{
    return byte >= F4FS_PBR_AUX_FLAG_BIAS && byte <= F4FS_PBR_AUX_MAX_BYTE;
}

uint F4FSPbrAuxFlags(float encoded)
{
    uint byte = F4FSPbrAuxByte(encoded);
    return F4FSPbrAuxByteValid(byte) ? ((byte - F4FS_PBR_AUX_FLAG_BIAS) & 3u) : 0u;
}

// The authored roughness the encoder kept, or the raw channel for a byte the
// engine wrote.
float F4FSPbrAuxRoughness(float encoded)
{
    uint byte = F4FSPbrAuxByte(encoded);
    return F4FSPbrAuxByteValid(byte)
        ? (float)((byte - F4FS_PBR_AUX_FLAG_BIAS) >> 2u) / (float)F4FS_PBR_AUX_ROUGHNESS_STEPS
        : encoded;
}

#endif
