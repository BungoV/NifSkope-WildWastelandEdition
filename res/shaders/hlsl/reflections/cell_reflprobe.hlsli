// Shared HLSL for res/shaders/features/reflections/cell_reflprobe.glsl (include-only GLSL), HLSLBULK1 round 2 (notes/hlslbulk1/tools/factor.py). No bindings: the
// functions below are the converted ones, they read the declarations of the shader that includes them (its cbuffers,
// resources, statics and LooseUniforms: the per-module shim and the entry own those). Define WW_FN_<name> for each
// wanted function before the #include (and #undef after), or WW_FN_ALL for all of them; a _V<n> suffix is a variant
// of the same GLSL function (a different define set at the entry). Fusion Shaders (F4FS) reuses the math from here.
// Included by: fo4_cell_ps.hlsl, fo4_cellcsm_ps.hlsl, fo4_cellcsmfgc_ps.hlsl, fo4_cellcsmfgl_ps.hlsl, fo4_cellcsmfgp_ps.hlsl, fo4_cellcsmfs_ps.hlsl, fo4_cellfgc_ps.hlsl, fo4_cellfgl_ps.hlsl, fo4_cellfgp_ps.hlsl, fo4_cellfs_ps.hlsl

#if defined(WW_FN_cellReflOct) || defined(WW_FN_ALL)
float2 cellReflOct(float3 d)
{
    float3 n = d / ((abs(d.x) + abs(d.y)) + abs(d.z)).xxx;
    float2 f = n.xy;
    if (n.z < 0.0f)
    {
        f = (1.0f.xx - abs(n.yx)) * float2((n.x >= 0.0f) ? 1.0f : (-1.0f), (n.y >= 0.0f) ? 1.0f : (-1.0f));
    }
    return (f * 0.5f) + 0.5f.xx;
}
#endif

#if defined(WW_FN_cellReflSample) || defined(WW_FN_ALL)
float3 cellReflSample(Texture2D<float4> atlas, SamplerState _atlas_sampler, int i, float3 P, float3 R, float lod)
{
    float3 bmin = cellReflMin[i].xyz;
    float3 bmax = cellReflMax[i].xyz;
    float tmp1;
    if (abs(R.x) < 9.9999997473787516355514526367188e-06f)
    {
        tmp1 = 9.9999997473787516355514526367188e-06f;
    }
    else
    {
        tmp1 = R.x;
    }
    float tmp2;
    if (abs(R.y) < 9.9999997473787516355514526367188e-06f)
    {
        tmp2 = 9.9999997473787516355514526367188e-06f;
    }
    else
    {
        tmp2 = R.y;
    }
    float tmp3;
    if (abs(R.z) < 9.9999997473787516355514526367188e-06f)
    {
        tmp3 = 9.9999997473787516355514526367188e-06f;
    }
    else
    {
        tmp3 = R.z;
    }
    float3 Rs = float3(tmp1, tmp2, tmp3);
    float3 tf = max((bmax - P) / Rs, (bmin - P) / Rs);
    float t = min(tf.x, min(tf.y, tf.z));
    float3 tmp4;
    if (t > 0.0f)
    {
        tmp4 = (P + (R * t)) - cellReflPos[i].xyz;
    }
    else
    {
        tmp4 = R;
    }
    float3 d = tmp4;
    float T = cellReflAtlas.z;
    float tile = cellReflPos[i].w;
    float2 org = float2(mod(tile, cellReflAtlas.w), floor(tile / cellReflAtlas.w)) * T;
    float inset = (0.5f * exp2(ceil(lod))) / T;
    float3 tmp5 = normalize(d);
    float2 uv = clamp(cellReflOct(tmp5), inset.xx, (1.0f - inset).xx);
    return atlas.SampleLevel(_atlas_sampler, (org + (uv * T)) * cellReflAtlas.xy, lod).xyz;
}
#endif

#if defined(WW_FN_cellReflProbe) || defined(WW_FN_ALL)
float cellReflProbe(Texture2D<float4> atlas, SamplerState _atlas_sampler, float3 posView, float3 reflView, float gloss, float spec, float envScale)
{
    bool tmp1 = !cellReflOn;
    bool tmp2;
    if (!tmp1)
    {
        tmp2 = !cellCubeGameOn();
    }
    else
    {
        tmp2 = tmp1;
    }
    bool tmp3;
    if (!tmp2)
    {
        tmp3 = (cellProbe != 0) && (cellProbe != 66);
    }
    else
    {
        tmp3 = tmp2;
    }
    if (tmp3)
    {
        return 1.0f;
    }
    float3 tmp4 = posView;
    float3 P = cellWorldPos(tmp4);
    float3 tmp5 = reflView;
    float3 R = normalize(cellWorldDir(tmp5));
    float g = clamp(gloss, 0.0f, 1.0f);
    float lod = clamp(((1.0f - g) * 6.0f) - 1.0f, 0.0f, cellReflLods);
    float3 sum = 0.0f.xxx;
    float W = 0.0f;
    int fall = -1;
    for (int i = 0; i < cellReflCount; i++)
    {
        if (cellReflMax[i].w > 0.5f)
        {
            fall = i;
            continue;
        }
        float3 dmin = min(P - cellReflMin[i].xyz, cellReflMax[i].xyz - P);
        float w = clamp(min(dmin.x, min(dmin.y, dmin.z)) / max(cellReflMin[i].w, 1.0f), 0.0f, 1.0f);
        if (w <= 0.0f)
        {
            continue;
        }
        int tmp6 = i;
        float3 tmp7 = P;
        float3 tmp8 = R;
        float tmp9 = lod;
        sum += (cellReflSample(atlas, _atlas_sampler, tmp6, tmp7, tmp8, tmp9) * w);
        W += w;
    }
    if (W > 1.0f)
    {
        sum /= W.xxx;
        W = 1.0f;
    }
    cellReflW = W;
    if ((fall >= 0) && (W < 1.0f))
    {
        cellReflFall = 1.0f - W;
        int tmp10 = fall;
        float3 tmp11 = P;
        float3 tmp12 = R;
        float tmp13 = lod;
        sum += (cellReflSample(atlas, _atlas_sampler, tmp10, tmp11, tmp12, tmp13) * cellReflFall);
        W = 1.0f;
    }
    float k = ((3.0f * clamp(spec, 0.0f, 1.0f)) * min(sqrt(clamp(g - 0.300000011920928955078125f, 0.0f, 1.0f)), 1.0f)) * clamp(envScale, 0.0f, 50.0f);
    cellReflAdd = sum * k;
    return 1.0f - W;
}
#endif
