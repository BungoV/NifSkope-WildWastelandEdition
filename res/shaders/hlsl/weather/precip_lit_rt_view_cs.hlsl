// lane VKFAR11 (row L8 route lift 3, 2026-11-15): precip_lit_view_cs in GL's ReSTIR variant (WW_CELL_RESTIR, the all-on
// route's cell_precip_gpu): cellShadowF gives each casting lamp the pixel's traced ratio (cellRestir, t7) at the particle
// centre's window position in place of its cube (cellShadow, t6, is not declared). Its b31 is precip_lit_view_cs's with
// cellRestirOn / cellRestirSize inserted at 4352 and every later field 32 bytes on (vkpresent.h kPrecipLitRtAt);
// vkprecip.inc repacks the lit block for a ReSTIR run.
// profile: cs_6_0
#define WW_CELL_RESTIR 1
#include "precip_lit_view_cs.hlsl"
