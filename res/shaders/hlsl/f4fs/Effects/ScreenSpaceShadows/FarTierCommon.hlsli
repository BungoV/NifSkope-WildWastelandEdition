// S43 - THE FAR TIER'S SHARED CONSTANT BUFFER AND THE TWO HELPERS IT NEEDS.
//
// Deliberately does NOT #include bend_sss_gpu.hlsli. That file declares the
// raymarch's groupshared depth array at file scope, so including it here would
// give the downsample and compose passes a groupshared allocation they never
// touch and cap their occupancy for nothing. The two functions below are copied
// verbatim from it instead - the same verbatim-copy precedent bend_sss_gpu.hlsli
// itself records for its RT20 decode ("this file is the vendored Sony algorithm
// plus FO4 adaptations and deliberately carries no dependency on the shared
// effects headers; SurfelExtractCS.hlsl sets the same verbatim-copy precedent").
// If either expression ever changes there, it changes here.

#ifndef F4FS_SSS_FAR_TIER_COMMON_HLSLI_INCLUDED
#define F4FS_SSS_FAR_TIER_COMMON_HLSLI_INCLUDED

// bend_sss_gpu.hlsli's depth-domain constants, verbatim. S44: the first two ARE
// the first-person band, and tests/screen_space_shadows_tests.cpp text-pins them
// here as well as there against src/Effects/FirstPersonDepthDomain.h.
static const float F4FS_SSS_VIEWMODEL_DEPTH_MAX = 0.01f;
static const float F4FS_SSS_FIRST_PERSON_DEPTH_MIN = 1.0e-6f;
static const float F4FS_SSS_SKY_DEPTH = 0.99999f;
static const float F4FS_SSS_DEPTH_WINDOW_EPSILON = 1.0e-6f;

// S44. The far tier's half of the first-person band lever, same define, same
// default, same meaning (see bend_sss_gpu.hlsli). It is passed to ALL THREE far
// shaders so the tier cannot end up half-levered: the reduction, the coarse
// march and the resolve must agree about what the band is.
#ifndef F4FS_SSS_FP_BAND_OCCLUDERS
    #define F4FS_SSS_FP_BAND_OCCLUDERS 0
#endif

// The far tier's own constants. Mirrored field for field by
// ScreenSpaceShadowsFarConstants in ScreenSpaceShadowsRuntime.cpp; the host
// asserts the size. Laid out one float4 register at a time so nothing straddles.
cbuffer ScreenSpaceShadowsFarCB : register(b0)
{
    // row 0: the ACTIVE coarse rect the downsample fills and the compose reads,
    // and the ACTIVE full-resolution rect the compose writes.
    uint2 SSSFar_CoarseBounds;
    uint2 SSSFar_FullBounds;
    // row 1: the resolution divisor, and 1 / ACTIVE full mask - the same
    // quantity the near tier uploads as SSS_InvMaskSize, so a tap taken here
    // lands on the same texel the raymarch would have tapped.
    uint SSSFar_Divisor;
    uint SSSFar_Pad0;
    float2 SSSFar_InvFullMaskSize;
    // row 2: the full-resolution and coarse dynamic-resolution ratios.
    float2 SSSFar_DynamicResolution;
    float2 SSSFar_CoarseDynamicResolution;
    // row 3: 1 / ACTIVE coarse mask, then spare.
    float2 SSSFar_InvCoarseMaskSize;
    float2 SSSFar_Pad1;
    // row 4: xyz = (span*n*f, f - n, span*n), the SAME depth linearization the
    // raymarch gets as SSS_DepthLinearAndThicknessFloor.xyz. w = the RELATIVE
    // view-depth tolerance of the upsample (a fraction of the fine pixel's own
    // view depth). w <= 0, or an unrecoverable linearization (x or y <= 0),
    // disables the far composition entirely rather than guessing - the same
    // discipline the thickness floor uses for a domain it has no parameters for.
    float4 SSSFar_DepthLinearAndTolerance;
};

// bend_sss_gpu.hlsli's F4FSDecodeDomainDepth, verbatim - S44 lever included, so
// the two files cannot disagree about the band.
bool F4FSFarDecodeDomainDepth(float rawDepth, uint depthDomain, out float domainDepth)
{
    if (depthDomain == 0u) {
        domainDepth = rawDepth;
#if F4FS_SSS_FP_BAND_OCCLUDERS
        return rawDepth > F4FS_SSS_FIRST_PERSON_DEPTH_MIN && rawDepth < F4FS_SSS_SKY_DEPTH;
#else
        return rawDepth > F4FS_SSS_VIEWMODEL_DEPTH_MAX && rawDepth < F4FS_SSS_SKY_DEPTH;
#endif
    }

    domainDepth = saturate(rawDepth * 100.0f);
    return rawDepth > F4FS_SSS_FIRST_PERSON_DEPTH_MIN &&
        rawDepth <= F4FS_SSS_VIEWMODEL_DEPTH_MAX;
}

// bend_sss_gpu.hlsli:682-691, verbatim (F4FSBendViewDepthFromOneMinusDepth).
float F4FSFarViewDepth(float3 depthLinearParams, float oneMinusDepth)
{
    const float numerator = depthLinearParams.x;
    const float range = depthLinearParams.y;
    if (numerator <= 0.0f || range <= 0.0f) {
        return 0.0f;
    }
    const float denominator = oneMinusDepth * range + depthLinearParams.z;
    return numerator / max(denominator, F4FS_SSS_DEPTH_WINDOW_EPSILON);
}

#endif
