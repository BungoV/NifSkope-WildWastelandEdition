// NifSkope entry of Fusion Shaders' (F4FS) bloom up pass (lane VKCOMPUTE1, 2026-11-12): the shim, then F4FS's own
// Post/BloomUpCS.hlsl unedited. F4FS_SOURCE = the installed file (set by tests/spells/hlsl_source.py, later the pass
// loader); unset = our copy below this folder.
#include "F4FSShimVk.hlsli"
#ifndef F4FS_SOURCE
#define F4FS_SOURCE "Post/BloomUpCS.hlsl"
#endif
#include F4FS_SOURCE
