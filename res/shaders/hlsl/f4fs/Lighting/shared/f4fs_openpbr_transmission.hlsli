// res/Lighting/shared/f4fs_openpbr_transmission.hlsli -- OpenPBR transmission of a PBRM effect draw.
//
// Host mirror: src/Materials/OpenPbrTransmission.h. Declares no resource and no cbuffer; the includer
// hands in the scene copy through F4FSOpenPbrTransmissionScene. Scatter, scatter anisotropy and
// dispersion are ignored.

#ifndef F4FS_OPENPBR_TRANSMISSION_HLSLI_INCLUDED
#define F4FS_OPENPBR_TRANSMISSION_HLSLI_INCLUDED

#define F4FS_OPENPBR_TRANSMISSION_HEADER_BIT (1u << 16)
#define F4FS_OPENPBR_TRANSMISSION_MAX_OFFSET_PIXELS 128.0
#define F4FS_OPENPBR_TRANSMISSION_MAX_MIPS 7u
#define F4FS_OPENPBR_TRANSMISSION_MIN_COSINE 0.2
#define F4FS_OPENPBR_TRANSMISSION_MIN_COLOR 1.0e-4

bool F4FSOpenPbrTransmissionOn(uint headerW)
{
    return (headerW & F4FS_OPENPBR_TRANSMISSION_HEADER_BIT) != 0u;
}

float3 F4FSOpenPbrTransmittedDirection(float3 viewToSurface, float3 n, float ior, bool thinWalled)
{
    if (thinWalled)
        return viewToSurface;
    return refract(viewToSurface, n, 1.0 / clamp(ior, 1.0, 3.0));
}

// Pixels the exit point moves on screen. dWx and dWy are ddx/ddy of the world position, taken
// before any branch; the shift is resolved on the surface plane they span.
float2 F4FSOpenPbrTransmissionOffsetPixels(float3 shift, float3 dWx, float3 dWy)
{
    float gxx = dot(dWx, dWx);
    float gxy = dot(dWx, dWy);
    float gyy = dot(dWy, dWy);
    float det = gxx * gyy - gxy * gxy;
    if (!(abs(det) > 1.0e-12))
        return 0.0.xx;
    float rx = dot(shift, dWx);
    float ry = dot(shift, dWy);
    float2 pixels = float2(rx * gyy - ry * gxy, ry * gxx - rx * gxy) / det;
    return clamp(pixels, -F4FS_OPENPBR_TRANSMISSION_MAX_OFFSET_PIXELS, F4FS_OPENPBR_TRANSMISSION_MAX_OFFSET_PIXELS);
}

float F4FSOpenPbrTransmissionBlurLod(float roughness, uint mips)
{
    return saturate(roughness) * (float)(mips > 0u ? mips - 1u : 0u);
}

float3 F4FSOpenPbrTransmissionAbsorption(float3 color, float depth, float thickness, float cosT, bool thinWalled)
{
    if (!(depth > 0.0) || thinWalled)
        return color;
    float path = thickness / max(abs(cosT), F4FS_OPENPBR_TRANSMISSION_MIN_COSINE);
    return exp(log(max(color, F4FS_OPENPBR_TRANSMISSION_MIN_COLOR)) / depth * path);
}

#endif
