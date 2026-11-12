// Shared HLSL for res/shaders/features/overlays/debug_view.glsl (include-only GLSL), HLSLBULK1 round 2 (notes/hlslbulk1/tools/factor.py). No bindings: the
// functions below are the converted ones, they read the declarations of the shader that includes them (its cbuffers,
// resources, statics and LooseUniforms: the per-module shim and the entry own those). Define WW_FN_<name> for each
// wanted function before the #include (and #undef after), or WW_FN_ALL for all of them; a _V<n> suffix is a variant
// of the same GLSL function (a different define set at the entry). Fusion Shaders (F4FS) reuses the math from here.
// Not here (declarations, or no converted entry carried them): wwDvLum, wwDvRamp, wwDvAbs, wwDvDepth, wwDvOwnerColour, wwDvSurface
#ifdef WW_FN_ALL
#ifndef WW_DV
#define WW_DV 1
#endif
#ifndef WW_DV_ALBEDO
#define WW_DV_ALBEDO 1
#endif
#ifndef WW_DV_NORMALS
#define WW_DV_NORMALS 2
#endif
#ifndef WW_DV_GLOSS
#define WW_DV_GLOSS 3
#endif
#ifndef WW_DV_AO
#define WW_DV_AO 4
#endif
#ifndef WW_DV_SUN
#define WW_DV_SUN 5
#endif
#ifndef WW_DV_SHADOW
#define WW_DV_SHADOW 6
#endif
#ifndef WW_DV_AMBIENT
#define WW_DV_AMBIENT 7
#endif
#ifndef WW_DV_GI
#define WW_DV_GI 8
#endif
#ifndef WW_DV_EMISSIVE
#define WW_DV_EMISSIVE 9
#endif
#ifndef WW_DV_FOG
#define WW_DV_FOG 10
#endif
#ifndef WW_DV_AIR
#define WW_DV_AIR 11
#endif
#ifndef WW_DV_CLOUDS
#define WW_DV_CLOUDS 12
#endif
#ifndef WW_DV_CLOUDSHADOW
#define WW_DV_CLOUDSHADOW 13
#endif
#ifndef WW_DV_WATER
#define WW_DV_WATER 14
#endif
#ifndef WW_DV_WATERREFL
#define WW_DV_WATERREFL 15
#endif
#ifndef WW_DV_WATERHEIGHT
#define WW_DV_WATERHEIGHT 16
#endif
#ifndef WW_DV_FAROWNER
#define WW_DV_FAROWNER 17
#endif
#ifndef WW_DV_EXPOSURE
#define WW_DV_EXPOSURE 18
#endif
#ifndef WW_DV_DEPTH
#define WW_DV_DEPTH 19
#endif
#ifndef WW_DV_OVERDRAW
#define WW_DV_OVERDRAW 20
#endif
#ifndef WW_DV_SPECULAR
#define WW_DV_SPECULAR 21
#endif
#ifndef WW_DV_REFLECTION
#define WW_DV_REFLECTION 22
#endif
#ifndef WW_DV_WATERSKY
#define WW_DV_WATERSKY 23
#endif
#endif
