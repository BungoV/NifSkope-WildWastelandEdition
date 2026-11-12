// hlsl profile: cs_6_0
// Lane VKSHARE1 (2026-11-12): the Modern temporal AA resolve on the present device (vkpresent.cpp taaRun, WW_VKVIEW): the
// same kernel as taa_resolve.hlsl with the picture (o1) stored into the RGBA8 target taa2.frag's attachment 1 is.
#define TAA_PIC_RGBA8 1
#include "taa_resolve.hlsl"
