// SPDX-License-Identifier: GPL-3.0-or-later WITH FO4-CS-Modding-Exception
//
// S43 - THE FAR TIER'S DEPTH REDUCTION. One coarse texel per thread, holding the
// MINIMUM raw depth over its DIVISOR x DIVISOR block of the engine's depth
// target, i.e. the NEAREST surface in that block.
//
// WHY MIN AND NOT AVERAGE. This buffer is what the far raymarch treats as
// geometry, and a screen-space shadow ray is looking for OCCLUDERS. An average
// would dissolve a branch, a wire or a single tree trunk into the sky behind it
// and the far tier would find nothing where the near tier finds everything; a
// minimum keeps the nearest thing in the block, so a canopy that covers one
// pixel in sixteen still occludes. The cost of that choice is that the coarse
// texel's depth is also the RECEIVER depth for that texel's own ray, and it is
// biased toward the camera by however much depth structure the block holds -
// which is exactly what the compose pass's depth-aware upsample exists to
// contain (FarComposeCS.hlsl).
//
// WHY VIEWMODEL TEXELS ARE EXCLUDED. Fallout packs first-person geometry into
// raw depth [0, 0.01] and the world into (0.01, 1). A single viewmodel texel is
// the smallest raw value there is, so an unfiltered min would hand the whole
// coarse texel a depth the world domain's decode rejects, and one gun barrel
// would punch a hole through the far tier across a 4 x 4 block. They are mapped
// to 1.0 (sky) instead: not a world occluder, and not a world receiver either.
//
// The taps go through the point-BORDER sampler at the same
// texel * invMaskSize * dynamicResolution mapping the raymarch uses, so an
// out-of-frame tap reads the sampler's 1.0 border and is sky - the identical
// off-screen contract the near tier has. (And it inherits the same known limit:
// under dynamic resolution a tap between the active rect and the texture edge
// reads unrendered depth rather than the border. That is bend_sss_gpu.hlsli's
// standing CP3-round-5 note, not a new defect.)

#include "FarTierCommon.hlsli"

Texture2D<float> SSSFar_SourceDepth : register(t0);
RWTexture2D<float> SSSFar_CoarseDepthOut : register(u0);
SamplerState SSSFar_PointBorder : register(s0);

[numthreads(8, 8, 1)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    const uint2 coarseXY = dispatchThreadID.xy;
    if (coarseXY.x >= SSSFar_CoarseBounds.x || coarseXY.y >= SSSFar_CoarseBounds.y) {
        return;
    }

    const uint2 base = coarseXY * SSSFar_Divisor;
    float minRaw = 1.0f;
    // A dynamic loop: the divisor is an INI setting rather than a compile-time
    // constant, and this pass is one thread per coarse texel - 1/16th of the
    // screen at the shipped divisor - so the loop overhead is not where this
    // pass's cost lives.
    [loop]
    for (uint j = 0u; j < SSSFar_Divisor; ++j) {
        [loop]
        for (uint i = 0u; i < SSSFar_Divisor; ++i) {
            const float2 uv = (float2(base + uint2(i, j)) + 0.5f) *
                SSSFar_InvFullMaskSize * SSSFar_DynamicResolution;
            float raw = SSSFar_SourceDepth.SampleLevel(SSSFar_PointBorder, uv, 0.0f);
#if !F4FX_SSS_FP_BAND_OCCLUDERS
            // S44: the far tier's copy of the one compare per sample. It has to
            // happen HERE and not only in the coarse march's decode, because the
            // reduction is a MIN: a single first-person texel is the smallest raw
            // value in the block and would otherwise decide the whole coarse
            // texel. Mapping it to 1.0 (sky) is the exclusion - not a world
            // occluder, and not a world receiver either.
            raw = raw > F4FX_SSS_VIEWMODEL_DEPTH_MAX ? raw : 1.0f;
#endif
            minRaw = min(minRaw, raw);
        }
    }
    SSSFar_CoarseDepthOut[coarseXY] = minRaw;
}
