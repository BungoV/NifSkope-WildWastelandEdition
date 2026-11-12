// Bake tracer: the ray records and the push constants (lane VKCOMPUTE1, 2026-11-12; bakert/common.glsl, rqx.comp).
#ifndef BAKERT_TYPES_HLSLI
#define BAKERT_TYPES_HLSLI

struct BakeRTRay { float4 oT; float4 dT; };   // origin + tmin, direction + tmax (the tree's local frame)
struct BakeRTXRay { double3 o; double tmax; double3 d; double pad; };   // 64 bytes: the whole-ray tracer's, as the CPU casts it

// flags bit 0: any hit (an occlusion ray stops at its first accepted hit); rqx: bit 1 = kGraze off (its red).
// grow: how far bakert.cpp grew the BLAS triangles (rqx only; 0 = not grown). 32 bytes = bakert.cpp's push range
struct BakeRTPc { uint first; uint count; uint useMask; uint nTri; uint nOpaque; uint flags; float grow; uint pad; };

#endif
