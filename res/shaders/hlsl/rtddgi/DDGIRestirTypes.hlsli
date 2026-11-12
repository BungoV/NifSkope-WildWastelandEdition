// ReSTIR DI: the parameter block (lane VKCOMPUTE1, 2026-11-12; restir.comp struct RestirParams, std430 = 11 float4).
#ifndef DDGI_RESTIR_TYPES_HLSLI
#define DDGI_RESTIR_TYPES_HLSLI

struct RestirParams
{
	int4 size;        // W, H, candidates in the list, frame
	int4 opts;        // candidates a pixel (C), neighbours (K), the ratio's window half-size, red bits
	float4 knobs;     // neighbour radius (pixels), shadow-ray bias, fixture clear, history (1: the previous g-buffer is valid)
	float4 rows[3];   // view -> world (the soup's local frame)
	float4 proj;      // P00, P11, P20, P21
	float4 prows[3];  // the previous frame's
	float4 pproj;
};

#endif
