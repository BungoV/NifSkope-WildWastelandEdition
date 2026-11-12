// ReSTIR GI: the parameter block (lane HLSLRESTIR1, 2026-11-12; restirgi.comp struct RestirGiParams, std430 = 11 float4).
#ifndef DDGI_RESTIRGI_TYPES_HLSLI
#define DDGI_RESTIRGI_TYPES_HLSLI

struct RestirGiParams
{
	int4 size;        // W, H, frame, seed
	int4 opts;        // neighbours (K), the neighbour square's half side, the filter's half size, red bits (RESTIRGI_BIT_*)
	float4 knobs;     // history (1: the previous frame's buffers are valid), the running mean's frames cap, 1: the STBN
	                  // mask is bound, the a-trous passes
	float4 rows[3];   // view -> world (the soup's local frame)
	float4 proj;      // P00, P11, P20, P21
	float4 prows[3];  // the previous frame's
	float4 pproj;
};

#endif
