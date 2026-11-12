// Shared HLSL for res/shaders/features/ao/cell_givol.glsl (include-only GLSL), HLSLBULK1 round 2 (notes/hlslbulk1/tools/factor.py). No bindings: the
// functions below are the converted ones, they read the declarations of the shader that includes them (its cbuffers,
// resources, statics and LooseUniforms: the per-module shim and the entry own those). Define WW_FN_<name> for each
// wanted function before the #include (and #undef after), or WW_FN_ALL for all of them; a _V<n> suffix is a variant
// of the same GLSL function (a different define set at the entry). Fusion Shaders (F4FS) reuses the math from here.
// Included by: cell_givol_ps.hlsl, cell_givol_vs.hlsl
// Not here (declarations, or no converted entry carried them): volInside

#if defined(WW_FN_volRot) || defined(WW_FN_ALL)
float3x3 volRot(float3 deg)
{
    float3 r = radians(deg);
    float3 c = cos(r);
    float3 s = sin(r);
    float3x3 X = float3x3(float3(1.0f, 0.0f, 0.0f), float3(0.0f, c.x, s.x), float3(0.0f, -s.x, c.x));
    float3x3 Y = float3x3(float3(c.y, 0.0f, -s.y), float3(0.0f, 1.0f, 0.0f), float3(s.y, 0.0f, c.y));
    float3x3 Z = float3x3(float3(c.z, s.z, 0.0f), float3(-s.z, c.z, 0.0f), float3(0.0f, 0.0f, 1.0f));
    return mul(X, mul(Y, Z));
}
#endif
