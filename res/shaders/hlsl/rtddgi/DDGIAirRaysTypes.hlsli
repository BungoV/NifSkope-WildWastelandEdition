// Air rays: the parameter block (lane VKCOMPUTE1, 2026-11-12; airrays.comp struct AirParams, std430 = 11 float4).
#ifndef DDGI_AIR_RAYS_TYPES_HLSLI
#define DDGI_AIR_RAYS_TYPES_HLSLI

#define AIR_BIT_NOHOLE 1     // red: alpha holes block
#define AIR_BIT_NOHIST 2     // red: no reprojection, this frame's two rays alone
#define AIR_BIT_NOSHADOW 4   // red: every ray unblocked (ratio 1: the unshadowed air)

struct AirParams
{
	int4 size;        // froxel columns, rows, slices, frame
	int4 opts;        // samples a froxel, the frames clamp, red bits, history valid (1)
	float4 slice;     // near, ln(far / near), shadow-ray bias, t per view unit (fogDistScale)
	float4 rows[3];   // view -> world (the soup's local frame)
	float4 proj;      // P00, P11, P20, P21
	float4 prows[3];  // the previous frame's
	float4 pproj;
};

#endif
