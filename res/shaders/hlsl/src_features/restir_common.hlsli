// Shared HLSL for src/features/gi/rtddgi/restir_common.glsl (include-only GLSL), HLSLBULK1 round 2 (notes/hlslbulk1/tools/factor.py). No bindings: the
// functions below are the converted ones, they read the declarations of the shader that includes them (its cbuffers,
// resources, statics and LooseUniforms: the per-module shim and the entry own those). Define WW_FN_<name> for each
// wanted function before the #include (and #undef after), or WW_FN_ALL for all of them; a _V<n> suffix is a variant
// of the same GLSL function (a different define set at the entry). Fusion Shaders (F4FS) reuses the math from here.
// Not here (declarations, or no converted entry carried them): restirMake, restirHash, restirRand, restirSeed, restirLum, restirTarget, restirUpdate, restirFinalize, restirClampM, restirSimilar, restirSpherePoint, restirShadowed, restirRatio, restirViewPos, restirAccumulate
#ifdef WW_FN_ALL
#ifndef RESTIR_BIT_NOVIS
#define RESTIR_BIT_NOVIS 1
#endif
#ifndef RESTIR_BIT_NOPDF
#define RESTIR_BIT_NOPDF 2
#endif
#ifndef RESTIR_BIT_NOTEMPORAL
#define RESTIR_BIT_NOTEMPORAL 4
#endif
#ifndef RESTIR_BIT_NOCLAMP
#define RESTIR_BIT_NOCLAMP 8
#endif
#ifndef RESTIR_BIT_NOHOLE
#define RESTIR_BIT_NOHOLE 16
#endif
#ifndef RESTIR_HISTORY_CAP
#define RESTIR_HISTORY_CAP 20.0
#endif
#ifndef RESTIR_DEPTH_TOL
#define RESTIR_DEPTH_TOL 0.1
#endif
#ifndef RESTIR_NORMAL_TOL
#define RESTIR_NORMAL_TOL 0.906
#endif
#ifndef RESTIR_ACC_MAX
#define RESTIR_ACC_MAX 16.0
#endif
#endif
