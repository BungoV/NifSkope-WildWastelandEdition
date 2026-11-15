// lane VKFAR2 (row L8 slice 2b, 2026-11-15): precip_view_cs.hlsl lit at the particle's centre (cell_precip_gpu_gs.hlsl's
// vpLight call and its include chain). vkprecip.inc runs it for a lit cell with the sun only; the placed lights, the GI,
// the ambient, the cascades, the drift and an interior stay refused until a gate judges them. Not with PC_LOOK yet.
// profile: cs_6_0
#define PC_LIT 1
#include "precip_view_cs.hlsl"
