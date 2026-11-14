// lane VKFAR2 (row L8 slice 3, 2026-11-14): precip_view_cs.hlsl with the precipitation look (cell_precip_look.prog:
// drops as a swept water lens, snow normal maps), as cell_precip_look_gs.hlsl wraps cell_precip_gpu_gs.hlsl.
// profile: cs_6_0
#define PC_LOOK 1
#include "precip_view_cs.hlsl"
