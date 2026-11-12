// Shared HLSL for res/shaders/features/ao/cell_aox_xe.glsl (include-only GLSL), HLSLBULK1 round 2 (notes/hlslbulk1/tools/factor.py). No bindings: the
// functions below are the converted ones, they read the declarations of the shader that includes them (its cbuffers,
// resources, statics and LooseUniforms: the per-module shim and the entry own those). Define WW_FN_<name> for each
// wanted function before the #include (and #undef after), or WW_FN_ALL for all of them; a _V<n> suffix is a variant
// of the same GLSL function (a different define set at the entry). Fusion Shaders (F4FS) reuses the math from here.
// Not here (declarations, or no converted entry carried them): XeGTAO_CalculateEdges, XeGTAO_PackEdges, XeGTAO_UnpackEdges, XeGTAO_FastSqrt, XeGTAO_FastACos, XeGTAO_SampleDepth, XeGTAO_MainPass, XeGTAO_Denoise
#ifdef WW_FN_ALL
#ifndef XE_GTAO_PI
#define XE_GTAO_PI 3.1415926535897932384626433832795
#endif
#ifndef XE_GTAO_PI_HALF
#define XE_GTAO_PI_HALF 1.5707963267948966192313216916398
#endif
#ifndef XE_GTAO_OCCLUSION_TERM_SCALE
#define XE_GTAO_OCCLUSION_TERM_SCALE 1.5
#endif
#endif
