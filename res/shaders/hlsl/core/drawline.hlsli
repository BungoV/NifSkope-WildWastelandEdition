// Shared HLSL for res/shaders/drawline.glsl (include-only GLSL), HLSLBULK1 round 2 (notes/hlslbulk1/tools/factor.py). No bindings: the
// functions below are the converted ones, they read the declarations of the shader that includes them (its cbuffers,
// resources, statics and LooseUniforms: the per-module shim and the entry own those). Define WW_FN_<name> for each
// wanted function before the #include (and #undef after), or WW_FN_ALL for all of them; a _V<n> suffix is a variant
// of the same GLSL function (a different define set at the entry). Fusion Shaders (F4FS) reuses the math from here.
// Included by: lines_gs.hlsl, normals_gs.hlsl

#if defined(WW_FN_drawLine) || defined(WW_FN_ALL)
void drawLine(inout float4 p0, inout float4 p1, inout float4 c0, inout float4 c1, inout TriangleStream<SPIRV_Cross_Output> geometry_stream)
{
    float p0zw = p0.z + p0.w;
    float p1zw = p1.z + p1.w;
    if (!((p0zw > 0.0f) && (p1zw > 0.0f)))
    {
        if (p0zw > 9.9999999747524270787835121154785e-07f)
        {
            float t = p0zw / (p0zw - p1zw);
            p1 = lerp(p0, p1, t.xxxx);
            c1 = lerp(c0, c1, t.xxxx);
        }
        else
        {
            if (p1zw > 9.9999999747524270787835121154785e-07f)
            {
                float t_1 = p1zw / (p1zw - p0zw);
                p0 = lerp(p1, p0, t_1.xxxx);
                c0 = lerp(c1, c0, t_1.xxxx);
            }
            else
            {
                return;
            }
        }
    }
    float3 p0_ndc = p0.xyz / p0.w.xxx;
    float3 p1_ndc = p1.xyz / p1.w.xxx;
    float2 vpScale = float2(viewportDimensions.zw) * 0.5f;
    float2 vpOffs = float2(viewportDimensions.xy) + vpScale;
    float2 p0_ss = p0_ndc.xy * vpScale;
    float2 p1_ss = p1_ndc.xy * vpScale;
    float2 d = normalize(p1_ss - p0_ss) * max(lineWidth * 0.5f, 0.5f);
    float2 n = float2(-d.y, d.x) / vpScale;
    C = c0;
    gl_Position = float4(p0_ndc.xy + n, p0_ndc.z, 1.0f);
    {
        SPIRV_Cross_Output stage_output;
        stage_output.gl_Position = gl_Position;
        stage_output.C = C;
        geometry_stream.Append(stage_output);
    }
    C = c0;
    gl_Position = float4(p0_ndc.xy - n, p0_ndc.z, 1.0f);
    {
        SPIRV_Cross_Output stage_output;
        stage_output.gl_Position = gl_Position;
        stage_output.C = C;
        geometry_stream.Append(stage_output);
    }
    C = c1;
    gl_Position = float4(p1_ndc.xy + n, p1_ndc.z, 1.0f);
    {
        SPIRV_Cross_Output stage_output;
        stage_output.gl_Position = gl_Position;
        stage_output.C = C;
        geometry_stream.Append(stage_output);
    }
    C = c1;
    gl_Position = float4(p1_ndc.xy - n, p1_ndc.z, 1.0f);
    {
        SPIRV_Cross_Output stage_output;
        stage_output.gl_Position = gl_Position;
        stage_output.C = C;
        geometry_stream.Append(stage_output);
    }
    geometry_stream.RestartStrip();
}
#endif
