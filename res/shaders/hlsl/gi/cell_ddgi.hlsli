// Shared HLSL for res/shaders/features/gi/cell_ddgi.glsl (include-only GLSL), HLSLBULK1 round 2 (notes/hlslbulk1/tools/factor.py). No bindings: the
// functions below are the converted ones, they read the declarations of the shader that includes them (its cbuffers,
// resources, statics and LooseUniforms: the per-module shim and the entry own those). Define WW_FN_<name> for each
// wanted function before the #include (and #undef after), or WW_FN_ALL for all of them; a _V<n> suffix is a variant
// of the same GLSL function (a different define set at the entry). Fusion Shaders (F4FS) reuses the math from here.
// Not here (declarations, or no converted entry carried them): cellDdgiIMin, cellDdgiIMax, cellDdgiSignNotZero, cellDdgiTile, cellDdgiChebyshev, cellDdgiRead
#ifdef WW_FN_ALL
#ifndef CELL_DDGI_TIGHT
#define CELL_DDGI_TIGHT - 1.0
#endif
#endif
