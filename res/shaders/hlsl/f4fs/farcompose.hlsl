// NifSkope entry of Fusion Shaders' (F4FS) far screen-space shadow compose pass (lane VKCOMPUTE1, 2026-11-12): the shim,
// then F4FS's own Effects/ScreenSpaceShadows/FarComposeCS.hlsl unedited. F4FS_SOURCE = the installed file (set by
// tests/spells/hlsl_source.py, later the pass loader); unset = our copy below this folder.
#include "F4FSShimVk.hlsli"
#ifndef F4FS_SOURCE
#define F4FS_SOURCE "Effects/ScreenSpaceShadows/FarComposeCS.hlsl"
#endif
#include F4FS_SOURCE
