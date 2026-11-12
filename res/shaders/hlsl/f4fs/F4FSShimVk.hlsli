// Fusion Shaders (F4FS) passes on NifSkope's Vulkan device (lane VKCOMPUTE1, 2026-11-12). NifSkope compiles F4FS's own
// .hlsl files unedited: the installed code mod first (<MO2>/mods/FO4CS/F4SE/Plugins/FO4CS, beside the Assets mod that
// recxmodroots.cpp finds), the copy in this folder (same paths below it) when F4FS is not installed.
// F4FS's files declare plain D3D registers (b0, t0-t4, s0, u0). Without [[vk::binding]] DXC makes each register number the
// binding number, so b0, t0, s0 and u0 would all be binding 0 of set 0. The loader moves each register class into its
// own range (DXC -fvk-<class>-shift <n> 0); these lines are the one place the numbers live (tests/spells/hlsl_source.py
// reads them): binding = register + shift.
// vk shift: b 0
// vk shift: t 8
// vk shift: s 16
// vk shift: u 24
// So: the pass cbuffer b0 -> binding 0, SourceColor t0 -> 8, SceneDepth t4 -> 12, LinearClamp / PointBorder s0 -> 16,
// the target u0 -> 24. NifSkope differences, if a pass ever needs one, go here behind #if, never into F4FS's file.
#ifndef F4FS_SHIM_VK_HLSLI
#define F4FS_SHIM_VK_HLSLI
#endif
