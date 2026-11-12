// SPDX-License-Identifier: GPL-3.0-or-later WITH FO4-CS-Modding-Exception
//
// S43 - THE FAR TIER'S RESOLVE. Reads the coarse far mask back to full
// resolution with a DEPTH-AWARE (bilateral) upsample and composes it into the
// near tier's mask by min().
//
// WHY min() AND NOT A MULTIPLY. It is the operation this module already
// composes with: bsdf_light_sun.hlsl's F4FX_SSS_MIN_COMPOSITION, and the
// operation CP3 round 5's whole safety argument is stated over (monotone
// non-decreasing in the mask, identity at 1.0). Two independent estimates of
// "is this pixel occluded" combined by min() take the more occluded answer and
// can never manufacture light; a multiply would double-darken every pixel both
// tiers agree on, and the near tier's shadows would go black wherever the far
// tier also found their caster.
//
// WHY THE UPSAMPLE HAS TO BE DEPTH-AWARE. The coarse depth is a MINIMUM over its
// block (FarDepthMinCS.hlsl), so a coarse texel that clipped the edge of a trunk
// describes the TRUNK, at the trunk's distance, and its ray was marched from
// there. Reading that texel bilinearly onto the ground beside the trunk paints
// the trunk's answer onto a surface tens of metres further away - a halo along
// every silhouette. The fix is the standard bilateral form: bilinear weights
// multiplied by an agreement term on LINEAR VIEW DEPTH, so a fine pixel only
// takes far occlusion from coarse texels that are describing its own surface.
// This is the pattern the repo already uses for its half-resolution SSGI resolve
// (fUpsampleNormalWeight's depth half, Settings.cpp's SSGI block: a relative
// view-Z tolerance), applied to a min-reduced source instead of an averaged one.
//
// WHERE NO COARSE TEXEL AGREES, THE ANSWER IS "NO FAR OCCLUSION" - the mask is
// left exactly as the near tier wrote it. That is this module's standing law
// (bend_sss_gpu.hlsli, CP3 round 5): screen space only ever ADDS occlusion, and
// where it cannot see it must add nothing. It is also what makes this pass safe
// to run over the whole frame: sky, first-person geometry, an unrecoverable
// depth linearization and a disagreeing neighbourhood all take the same
// early-out, and none of them can move a mask value.

#include "FarTierCommon.hlsli"

Texture2D<float> SSSFar_SourceDepth : register(t0);
Texture2D<float> SSSFar_CoarseDepth : register(t1);
Texture2D<float> SSSFar_FarMask : register(t2);
RWTexture2D<float> SSSFar_OutputMask : register(u0);
SamplerState SSSFar_PointBorder : register(s0);

[numthreads(8, 8, 1)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    const uint2 writeXY = dispatchThreadID.xy;
    if (writeXY.x >= SSSFar_FullBounds.x || writeXY.y >= SSSFar_FullBounds.y) {
        return;
    }
    const float tolerance = SSSFar_DepthLinearAndTolerance.w;
    if (!(tolerance > 0.0f)) {
        return;
    }

    const float2 uv = (float2(writeXY) + 0.5f) *
        SSSFar_InvFullMaskSize * SSSFar_DynamicResolution;
    const float rawDepth = SSSFar_SourceDepth.SampleLevel(SSSFar_PointBorder, uv, 0.0f);
    float fineDepth;
    if (!F4FXFarDecodeDomainDepth(rawDepth, 0u, fineDepth)) {
        return;
    }
    const float fineViewDepth = F4FXFarViewDepth(
        SSSFar_DepthLinearAndTolerance.xyz, max(abs(1.0f - fineDepth), 1.0e-4f));
    if (!(fineViewDepth > 0.0f)) {
        return;
    }

    // This pixel's position in COARSE TEXEL CENTRE coordinates, so `base` is the
    // lower-left of the 2x2 whose centres bracket it and `fraction` is the
    // ordinary bilinear weight pair.
    const float2 coarsePos =
        (float2(writeXY) + 0.5f) / float(SSSFar_Divisor) - 0.5f;
    const int2 base = int2(floor(coarsePos));
    const float2 fraction = coarsePos - float2(base);
    const int2 coarseBounds = int2(SSSFar_CoarseBounds);

    float totalWeight = 0.0f;
    float accumulated = 0.0f;
    [unroll]
    for (int j = 0; j < 2; ++j) {
        [unroll]
        for (int i = 0; i < 2; ++i) {
            const int2 coarseXY = base + int2(i, j);
            if (any(coarseXY < int2(0, 0)) || any(coarseXY >= coarseBounds)) {
                continue;
            }
            const float bilinearWeight =
                (i == 0 ? 1.0f - fraction.x : fraction.x) *
                (j == 0 ? 1.0f - fraction.y : fraction.y);
            if (!(bilinearWeight > 0.0f)) {
                continue;
            }
            const float2 coarseUv = (float2(coarseXY) + 0.5f) *
                SSSFar_InvCoarseMaskSize * SSSFar_CoarseDynamicResolution;
            const float coarseRaw =
                SSSFar_CoarseDepth.SampleLevel(SSSFar_PointBorder, coarseUv, 0.0f);
            float coarseDepth;
            if (!F4FXFarDecodeDomainDepth(coarseRaw, 0u, coarseDepth)) {
                continue;
            }
            const float coarseViewDepth = F4FXFarViewDepth(
                SSSFar_DepthLinearAndTolerance.xyz,
                max(abs(1.0f - coarseDepth), 1.0e-4f));
            if (!(coarseViewDepth > 0.0f)) {
                continue;
            }
            // The agreement term. Relative rather than absolute because every
            // screen-space length in this module scales with view depth: a fixed
            // world tolerance would reject everything at 20 cm and accept
            // everything at 200 m.
            const float relative =
                abs(fineViewDepth - coarseViewDepth) / fineViewDepth;
            const float depthWeight = saturate(1.0f - relative / tolerance);
            const float weight = bilinearWeight * depthWeight;
            if (!(weight > 0.0f)) {
                continue;
            }
            totalWeight += weight;
            accumulated += weight *
                SSSFar_FarMask.SampleLevel(SSSFar_PointBorder, coarseUv, 0.0f);
        }
    }

    if (!(totalWeight > 1.0e-4f)) {
        return;
    }
    const float farValue = accumulated / totalWeight;
    SSSFar_OutputMask[writeXY] = min(SSSFar_OutputMask[writeXY], farValue);
}
