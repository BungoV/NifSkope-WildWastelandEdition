#ifndef OPENPBR_CONSTANTS_HLSLI_INCLUDED
#define OPENPBR_CONSTANTS_HLSLI_INCLUDED

// Shared perceptual-roughness floor for every code-84 PBR stage. Keeping this
// value identical at G-buffer write and BRDF read time prevents direct and
// indirect lighting from evaluating different material roughness values.
static const float OPENPBR_MIN_PERCEPTUAL_ROUGHNESS = 0.045;

// Geometric specular AA (Kaplanyan 2016 / Tokuyoshi-Kaplanyan 2019, Filament's
// normalFiltering path). The variance term is built from screen-space derivatives
// of the normal. The kernel is capped before it is added in alpha^2 space so
// distant/noisy normal maps broaden highlights without erasing authored shape.
// The prepass reads the live [PBRMaterials] fSpecularAAVariance / fSpecularAAThreshold
// from b11 .yz (0 variance = off); these are the values the diagnostic force
// variant, which has no b11, uses.
static const float OPENPBR_SPECULAR_AA_VARIANCE = 0.15;
static const float OPENPBR_SPECULAR_AA_THRESHOLD = 0.20;

// Blended PBR and opaque PBR subsurface share the RGBA16F auxiliary target.
// A high positive marker keeps even the lowest surviving blended coverage
// positive after blending over the negative subsurface tag. R/G remain
// premultiplied by actual coverage; consumers recover coverage from B.
static const float OPENPBR_BLEND_METADATA_MARKER = 64.0;

bool OpenPBR_IsBlendMetadata(float encodedMarker)
{
    return encodedMarker > 0.5;
}

float OpenPBR_DecodeBlendCoverage(float encodedMarker)
{
    return max(encodedMarker / OPENPBR_BLEND_METADATA_MARKER, 1e-5);
}

// PBR skin retains its identity after replacing the engine's code 5 with 84/86.
// Plain skin uses A=2; translucent skin scales its negative alpha by eight so
// both consumers retain their record. Authored diffusion keeps its own coverage.
static const float OPENPBR_SKIN_SUBSURFACE_TAG = 2.0;
static const float OPENPBR_SKIN_TRANSLUCENCY_SCALE = 8.0;

float4 OpenPBR_MarkSkinSubsurface(float4 aux)
{
    // Power-of-two scaling preserves the transmission payload's half-float precision.
    if (aux.a < -0.5)
        aux.a *= OPENPBR_SKIN_TRANSLUCENCY_SCALE;
    else if (all(aux == 0.0))
        aux.a = OPENPBR_SKIN_SUBSURFACE_TAG;
    return aux;
}

float OpenPBR_TranslucencyAlpha(float4 aux)
{
    return aux.a < -4.0 ? aux.a / OPENPBR_SKIN_TRANSLUCENCY_SCALE : aux.a;
}

bool OpenPBR_IsSkinSubsurfaceTag(float4 aux)
{
    return (all(aux.rgb == 0.0) && abs(aux.a - OPENPBR_SKIN_SUBSURFACE_TAG) < 0.25) || aux.a < -4.0;
}

// The tag carries the skin's transmittance mask (BGSM v22+ `_l`.B, the
// thinness map) as A = tag + mask * scale, inside the tag's 0.25 window.
static const float OPENPBR_SKIN_TRANSMITTANCE_MASK_SCALE = 0.2;

float OpenPBR_SkinTransmittanceMask(float4 aux)
{
    return saturate((aux.a - OPENPBR_SKIN_SUBSURFACE_TAG) / OPENPBR_SKIN_TRANSMITTANCE_MASK_SCALE);
}

float OpenPBR_FilterPerceptualRoughness(
    float perceptualRoughness,
    float normalDerivativeEnergy,
    float varianceScale,
    float threshold)
{
    float inputPerceptualRoughness = saturate(perceptualRoughness);
    float linearRoughness = inputPerceptualRoughness * inputPerceptualRoughness;
    float variance = max(varianceScale, 0.0) * max(normalDerivativeEnergy, 0.0);
    float kernelRoughness = min(2.0 * variance, max(threshold, 0.0));
    float squareRoughness = saturate(
        linearRoughness * linearRoughness + kernelRoughness);
    return sqrt(sqrt(squareRoughness));
}

#endif
