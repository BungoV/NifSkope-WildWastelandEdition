// Bake tracer: the ray records and the push constants (lane VKCOMPUTE1, 2026-11-12; bakert/common.glsl, rqx.comp).
#ifndef BAKERT_TYPES_HLSLI
#define BAKERT_TYPES_HLSLI

struct BakeRTRay { float4 oT; float4 dT; };   // origin + tmin, direction + tmax (local frame of the tree)
// the whole-ray tracer's ray (rqx.comp XRay):
struct BakeRTXRay { double3 o; double tmax; double3 d; double pad; };   // 64 bytes; the tree's local frame, as the CPU casts it

// flags bit 0: any hit (an occlusion ray stops at its first accepted hit, which need not be the nearest)
// rqx.comp's:
// flags bit 0: occlusion ray (any accepted hit answers)
// grow: how far bakert.cpp grew the BLAS triangles (0 = not grown: the float-scale edge band, RTGI1's rule)
// rqx: bit 1 = kGraze off (its red); grow is read by rqx only. 32 bytes = bakert.cpp's push range
struct BakeRTPc { uint first; uint count; uint useMask; uint nTri; uint nOpaque; uint flags; float grow; uint pad; };

#endif
