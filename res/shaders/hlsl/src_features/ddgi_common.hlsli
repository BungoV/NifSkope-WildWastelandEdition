// Shared HLSL for src/features/gi/rtddgi/ddgi_common.glsl (include-only GLSL), HLSLBULK1 round 2 (notes/hlslbulk1/tools/factor.py). No bindings: the
// functions below are the converted ones, they read the declarations of the shader that includes them (its cbuffers,
// resources, statics and LooseUniforms: the per-module shim and the entry own those). Define WW_FN_<name> for each
// wanted function before the #include (and #undef after), or WW_FN_ALL for all of them; a _V<n> suffix is a variant
// of the same GLSL function (a different define set at the entry). Fusion Shaders (F4FS) reuses the math from here.
// Not here (declarations, or no converted entry carried them): ddgiLinearId, ddgiGroupId, signNotZero, ddgiOctEncode, ddgiOctDecode, ddgiTexelDir, ddgiBorderSource, ddgiBorderXY, ddgiTileTaps, ddgiSampleIrr, ddgiSampleDist, ddgiFibonacci, ddgiRayDir, ddgiTieWindow, ddgiTieWins, ddgiGammaEncode, ddgiGammaDecode, ddgiTexelHysteresis, ddgiAgeHysteresis, ddgiBiasVector, ddgiBackfaceWeight, ddgiChebyshev, ddgiCrush, ddgiRadial, ddgiSpot, ddgiSky, ddgiBelowHorizon, ddgiSkyVisRay, ddgiMiss, ddgiProbeCoord, ddgiProbePos, ddgiProbeState, ddgiSkipped, ddgiHole
#ifdef WW_FN_ALL
#ifndef DDGI_PI
#define DDGI_PI 3.14159265358979323846
#endif
#ifndef DDGI_FIXED
#define DDGI_FIXED 32
#endif
#ifndef DDGI_IRR_N
#define DDGI_IRR_N 6
#endif
#ifndef DDGI_IRR_S
#define DDGI_IRR_S 8
#endif
#ifndef DDGI_DIST_N
#define DDGI_DIST_N 14
#endif
#ifndef DDGI_DIST_S
#define DDGI_DIST_S 16
#endif
#ifndef DDGI_MISS
#define DDGI_MISS 1e27
#endif
#ifndef DDGI_TMIN
#define DDGI_TMIN 1e-4
#endif
#ifndef DDGI_OFF
#define DDGI_OFF 0
#endif
#ifndef DDGI_ASLEEP
#define DDGI_ASLEEP 1
#endif
#ifndef DDGI_VIGILANT
#define DDGI_VIGILANT 2
#endif
#ifndef DDGI_FRESH
#define DDGI_FRESH 3
#endif
#ifndef DDGI_RED_NOBACK
#define DDGI_RED_NOBACK 1u
#endif
#ifndef DDGI_RED_NOCHEB
#define DDGI_RED_NOCHEB 2u
#endif
#ifndef DDGI_RED_NOBIAS
#define DDGI_RED_NOBIAS 4u
#endif
#ifndef DDGI_RED_HWHIT
#define DDGI_RED_HWHIT 8u
#endif
#ifndef DDGI_RED_NOTIE
#define DDGI_RED_NOTIE 16u
#endif
#endif
