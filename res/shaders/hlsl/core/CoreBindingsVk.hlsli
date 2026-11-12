// Vulkan bindings for the HLSL ports in this folder (HLSLBULK1). The files declare plain registers;
// the compiler gets -fvk-<class>-shift from the line below so b/t/s/u never share a binding.
// vk shift: b 0 / t 64 / s 128 / u 192
#ifdef __spirv__
#define WW_LOC(n) [[vk::location(n)]]
#else
#define WW_LOC(n)
#endif
