// Shared HLSL for res/shaders/features/air/cell_airmed.glsl (include-only GLSL), HLSLBULK1 round 2 (notes/hlslbulk1/tools/factor.py). No bindings: the
// functions below are the converted ones, they read the declarations of the shader that includes them (its cbuffers,
// resources, statics and LooseUniforms: the per-module shim and the entry own those). Define WW_FN_<name> for each
// wanted function before the #include (and #undef after), or WW_FN_ALL for all of them; a _V<n> suffix is a variant
// of the same GLSL function (a different define set at the entry). Fusion Shaders (F4FS) reuses the math from here.
// Included by: cell_precip_gpu_gs.hlsl, cell_precip_look_gs.hlsl, cell_precip_ps.hlsl, cell_volair_ps.hlsl, fo4_effectcell_ps.hlsl, fo4_effectcellfx_ps.hlsl, fo4_effectdecal_ps.hlsl, fo4_effectdecalcsm_ps.hlsl, particles_cell_ps.hlsl
// Not here (declarations, or no converted entry carried them): airF, airE1, air1mE, airEnvColourEnv
#ifdef WW_FN_ALL
static const float PI = 3.14159265;
#endif

#if defined(WW_FN_airTerrH) || defined(WW_FN_ALL)
float airTerrH(float2 Pw)
{
    float hc = airTerr.Sample(_airTerr_sampler, float3((Pw.x * airTerrXf.x) + airTerrXf.y, (Pw.y * airTerrXf.z) + airTerrXf.w, 0.0f)).x;
    float2 uf = float2((Pw.x * airTerrFineXf.x) + airTerrFineXf.y, (Pw.y * airTerrFineXf.z) + airTerrFineXf.w);
    float wf = clamp(min(min(uf.x, 1.0f - uf.x), min(uf.y, 1.0f - uf.y)) * 16.0f, 0.0f, 1.0f);
    float tmp1;
    if (wf > 0.0f)
    {
        tmp1 = lerp(hc, airTerr.Sample(_airTerr_sampler, float3(uf, 1.0f)).x, wf);
    }
    else
    {
        tmp1 = hc;
    }
    return tmp1;
}
#endif

#if defined(WW_FN_airTerrH_V2) || defined(WW_FN_ALL)
float airTerrH(float2 Pw)
{
    float hc = airTerr.SampleLevel(_airTerr_sampler, float3((Pw.x * airTerrXf.x) + airTerrXf.y, (Pw.y * airTerrXf.z) + airTerrXf.w, 0.0f), 0.0f).x;
    float2 uf = float2((Pw.x * airTerrFineXf.x) + airTerrFineXf.y, (Pw.y * airTerrFineXf.z) + airTerrFineXf.w);
    float wf = clamp(min(min(uf.x, 1.0f - uf.x), min(uf.y, 1.0f - uf.y)) * 16.0f, 0.0f, 1.0f);
    float tmp1;
    if (wf > 0.0f)
    {
        tmp1 = lerp(hc, airTerr.SampleLevel(_airTerr_sampler, float3(uf, 1.0f), 0.0f).x, wf);
    }
    else
    {
        tmp1 = hc;
    }
    return tmp1;
}
#endif

#if defined(WW_FN_airGroundAt) || defined(WW_FN_ALL)
void airGroundAt(float3 Pw)
{
    float tmp1;
    if (airTerrOn != 0)
    {
        float2 tmp2 = Pw.xy;
        tmp1 = airTerrH(tmp2) - airTerrRef;
    }
    else
    {
        tmp1 = 0.0f;
    }
    airGz = tmp1;
}
#endif

#if defined(WW_FN_volHg) || defined(WW_FN_ALL)
float volHg(float c, float g)
{
    float d = (1.0f + (g * g)) - ((2.0f * g) * c);
    return (1.0f - (g * g)) / ((12.56637096405029296875f * d) * sqrt(d));
}
#endif

#if defined(WW_FN_volPhasePi) || defined(WW_FN_ALL)
float3 volPhasePi(float c)
{
    float tmp1 = c;
    float tmp2 = volPhase[1].w;
    float tmp3 = c;
    float tmp4 = volPhase[2].w;
    return ((((volPhase[0].xyz * 0.0596831031143665313720703125f) * (1.0f + (c * c))) + (volPhase[1].xyz * volHg(tmp1, tmp2))) + (volPhase[2].xyz * volHg(tmp3, tmp4))) * 3.1415927410125732421875f;
}
#endif

#if defined(WW_FN_airRho) || defined(WW_FN_ALL)
float airRho(float z)
{
    float tmp1 = z;
    float4 tmp2 = airMed;
    float4 tmp3 = airLayer;
    float4 tmp4 = float4(airGround.x + airGz, airGround.yzw);
    float tmp5 = wwAirRhoLaw(tmp1, tmp2, tmp3, tmp4);
    return tmp5;
}
#endif

#if defined(WW_FN_airCol) || defined(WW_FN_ALL)
float airCol(float lo, float hi)
{
    float tmp1 = lo;
    float tmp2 = hi;
    float4 tmp3 = airMed;
    float4 tmp4 = airLayer;
    float4 tmp5 = float4(airGround.x + airGz, airGround.yzw);
    float tmp6 = wwAirColLaw(tmp1, tmp2, tmp3, tmp4, tmp5);
    return tmp6;
}
#endif

#if defined(WW_FN_airOd) || defined(WW_FN_ALL)
float airOd(float za, float zb, float len)
{
    float dz = zb - za;
    if (abs(dz) < 0.5f)
    {
        float tmp1 = 0.5f * (za + zb);
        return (airMed.x * len) * airRho(tmp1);
    }
    float tmp2 = min(za, zb);
    float tmp3 = max(za, zb);
    return ((airMed.x * len) * airCol(tmp2, tmp3)) / abs(dz);
}
#endif

#if defined(WW_FN_airTup) || defined(WW_FN_ALL)
float airTup(float z, inout float mu)
{
    float4 tmp1 = airMed;
    float4 tmp2 = airGround;
    if (z >= wwAirEnd(tmp1, tmp2))
    {
        return 1.0f;
    }
    mu = max(mu, 0.0199999995529651641845703125f);
    float4 tmp3 = airMed;
    float4 tmp4 = airGround;
    float tmp5 = z;
    float tmp6 = wwAirEnd(tmp3, tmp4);
    return exp(((-airMed.x) * airCol(tmp5, tmp6)) / mu);
}
#endif

#if defined(WW_FN_airTupSun) || defined(WW_FN_ALL)
float airTupSun(float z, float mu)
{
    if (!airSunLayer)
    {
        float tmp1 = z;
        float tmp2 = mu;
        float tmp3 = airTup(tmp1, tmp2);
        return tmp3;
    }
    bool tmp4 = z >= airMed.w;
    bool tmp5;
    if (!tmp4)
    {
        tmp5 = airLayer.w <= 0.0f;
    }
    else
    {
        tmp5 = tmp4;
    }
    if (tmp5)
    {
        return 1.0f;
    }
    float tmp6 = z;
    float tmp7 = airMed.w;
    float tmp8 = airGround.x + airGz;
    float tmp9 = airGround.y;
    return exp((((-airMed.x) * airLayer.w) * wwAirColOne(tmp6, tmp7, tmp8, tmp9)) / max(mu, 0.0199999995529651641845703125f));
}
#endif

#if defined(WW_FN_volRoomAxis) || defined(WW_FN_ALL)
float4 volRoomAxis(int3 c0, float3 f, int2 L, int a, float4 plain, int3 dm)
{
    float4 s = 0.0f.xxxx;
    float ws = 0.0f;
    int tmp1;
    int tmp2;
    for (int k = 0; k < 8; k++)
    {
        int3 o = int3(k & 1, (k >> 1) & 1, (k >> 2) & 1);
        int3 c = clamp(c0 + o, int3(0, 0, 0), dm - int3(1, 1, 1));
        float3 t = lerp(1.0f.xxx - f, f, float3(o));
        float w = (t.x * t.y) * t.z;
        float tmp3 = cellGiSlots.Load(int4(c, 0)).x;
        int2 S = cellRoomUnpack(tmp3);
        bool tmp4 = S.x >= 0;
        bool tmp5;
        if (tmp4)
        {
            bool tmp6 = S.x == L.x;
            bool tmp7;
            if (!tmp6)
            {
                tmp7 = S.x == L.y;
            }
            else
            {
                tmp7 = tmp6;
            }
            tmp5 = tmp7;
        }
        else
        {
            tmp5 = tmp4;
        }
        if (tmp5)
        {
            tmp1 = 0;
        }
        else
        {
            bool tmp8 = S.y >= 0;
            bool tmp9;
            if (tmp8)
            {
                bool tmp10 = S.y == L.x;
                bool tmp11;
                if (!tmp10)
                {
                    tmp11 = S.y == L.y;
                }
                else
                {
                    tmp11 = tmp10;
                }
                tmp9 = tmp11;
            }
            else
            {
                tmp9 = tmp8;
            }
            if (tmp9)
            {
                tmp2 = 6 * dm.z;
            }
            else
            {
                tmp2 = -1;
            }
            tmp1 = tmp2;
        }
        int base = tmp1;
        if ((base < 0) || (w <= 0.0f))
        {
            continue;
        }
        float4 v = cellGi.Load(int4(int3(c.xy, (c.z + base) + (a * dm.z)), 0));
        if (v.w > 0.0f)
        {
            s += (v * w);
            ws += w;
        }
    }
    if ((volRed & 4194304) != 0)
    {
        float4 tmp12;
        if (ws > 0.0f)
        {
            tmp12 = s / ws.xxxx;
        }
        else
        {
            tmp12 = plain;
        }
        return tmp12;
    }
    float4 tmp13;
    if (ws >= 0.25f)
    {
        tmp13 = s / ws.xxxx;
    }
    else
    {
        tmp13 = (s + (plain * (0.25f - ws))) / 0.25f.xxxx;
    }
    return tmp13;
}
#endif

#if defined(WW_FN_volCubeAll) || defined(WW_FN_ALL)
void volCubeAll(float3 P, inout float3 E[6], inout float share[6])
{
    if (cellGiAirOn)
    {
        float3 ga = (P - cellGiAirOrigin) / cellGiAirVoxel.xxx;
        float za = clamp(ga.z, 0.5f, cellGiAirDims.z - 0.5f);
        for (int a = 0; a < 6; a++)
        {
            E[a] = max(cellGiAir.Sample(_cellGiAir_sampler, float3(ga.xy / cellGiAirDims.xy, (za + (float(a) * cellGiAirDims.z)) / (6.0f * cellGiAirDims.z))).xyz, 0.0f.xxx);
            share[a] = 1.0f;
        }
        return;
    }
    int3 dm = int3(cellGiDims);
    float3 g = (P - cellGiOrigin) / cellGiVoxel.xxx;
    float2 xy = g.xy / cellGiDims.xy;
    float z = clamp(g.z, 0.5f, cellGiDims.z - 0.5f);
    float depth = float(int3(spvTextureSize(cellGi, uint(0), dummy_parameter)).z);
    float4 plain[6];
    float4 r[6];
    for (int a_1 = 0; a_1 < 6; a_1++)
    {
        plain[a_1] = cellGi.Sample(_cellGi_sampler, float3(xy, (z + (float(a_1) * cellGiDims.z)) / depth));
        r[a_1] = plain[a_1];
    }
    bool tmp1;
    if (cellGiRooms)
    {
        tmp1 = (cellPassRed & 8) == 0;
    }
    else
    {
        tmp1 = cellGiRooms;
    }
    if (tmp1)
    {
        float3 gv = g - 0.5f.xxx;
        int3 c0 = int3(floor(gv));
        float3 f = gv - float3(c0);
        if ((volRed & 4194304) != 0)
        {
            float3 tmp2 = P;
            int2 L = cellRoomAt(tmp2);
            if (L.x >= 0)
            {
                for (int a_2 = 0; a_2 < 6; a_2++)
                {
                    int3 tmp3 = c0;
                    float3 tmp4 = f;
                    int2 tmp5 = L;
                    int tmp6 = a_2;
                    float4 tmp7 = plain[a_2];
                    int3 tmp8 = dm;
                    r[a_2] = volRoomAxis(tmp3, tmp4, tmp5, tmp6, tmp7, tmp8);
                }
            }
        }
        else
        {
            float3 gr = ((P - cellRoomsOrigin) / cellRoomsCell.xxx) - 0.5f.xxx;
            int3 r0 = int3(floor(gr));
            float3 fr = gr - float3(r0);
            float wAir = 0.0f;
            bool same = true;
            float wk[8];
            int2 Lk[8];
            for (int k = 0; k < 8; k++)
            {
                int3 o = int3(k & 1, (k >> 1) & 1, (k >> 2) & 1);
                int3 c = r0 + o;
                float3 t = lerp(1.0f.xxx - fr, fr, float3(o));
                wk[k] = (t.x * t.y) * t.z;
                bool tmp9 = any(bool3(c.x < int3(0, 0, 0).x, c.y < int3(0, 0, 0).y, c.z < int3(0, 0, 0).z));
                bool tmp10;
                if (!tmp9)
                {
                    int3 tmp11 = int3(cellRoomsDims);
                    tmp10 = any(bool3(c.x >= tmp11.x, c.y >= tmp11.y, c.z >= tmp11.z));
                }
                else
                {
                    tmp10 = tmp9;
                }
                if (tmp10)
                {
                    Lk[k] = int2(-2, -2);
                }
                else
                {
                    float tmp12 = cellRooms.Load(int4(c, 0)).x;
                    Lk[k] = cellRoomUnpack(tmp12);
                }
                if (Lk[k].x == (-1))
                {
                    wk[k] = 0.0f;
                }
                wAir += wk[k];
                bool tmp13 = k > 0;
                bool tmp14;
                if (tmp13)
                {
                    tmp14 = any(bool2(Lk[k].x != Lk[0].x, Lk[k].y != Lk[0].y));
                }
                else
                {
                    tmp14 = tmp13;
                }
                if (tmp14)
                {
                    same = false;
                }
            }
            if (wAir > 0.0f)
            {
                if (same)
                {
                    if (Lk[0].x >= 0)
                    {
                        for (int a_3 = 0; a_3 < 6; a_3++)
                        {
                            int3 tmp15 = c0;
                            float3 tmp16 = f;
                            int2 tmp17 = Lk[0];
                            int tmp18 = a_3;
                            float4 tmp19 = plain[a_3];
                            int3 tmp20 = dm;
                            r[a_3] = volRoomAxis(tmp15, tmp16, tmp17, tmp18, tmp19, tmp20);
                        }
                    }
                }
                else
                {
                    for (int a_4 = 0; a_4 < 6; a_4++)
                    {
                        r[a_4] = 0.0f.xxxx;
                    }
                    float tmp21;
                    float4 tmp22;
                    for (int k_1 = 0; k_1 < 8; k_1++)
                    {
                        if (wk[k_1] <= 0.0f)
                        {
                            continue;
                        }
                        bool seen = false;
                        for (int j = 0; j < k_1; j++)
                        {
                            bool tmp23;
                            if (!seen)
                            {
                                bool tmp24 = wk[j] > 0.0f;
                                bool tmp25;
                                if (tmp24)
                                {
                                    tmp25 = all(bool2(Lk[j].x == Lk[k_1].x, Lk[j].y == Lk[k_1].y));
                                }
                                else
                                {
                                    tmp25 = tmp24;
                                }
                                tmp23 = tmp25;
                            }
                            else
                            {
                                tmp23 = seen;
                            }
                            seen = tmp23;
                        }
                        if (seen)
                        {
                            continue;
                        }
                        float wl = 0.0f;
                        for (int j_1 = k_1; j_1 < 8; j_1++)
                        {
                            if (all(bool2(Lk[j_1].x == Lk[k_1].x, Lk[j_1].y == Lk[k_1].y)))
                            {
                                tmp21 = wk[j_1];
                            }
                            else
                            {
                                tmp21 = 0.0f;
                            }
                            wl += tmp21;
                        }
                        wl /= wAir;
                        for (int a_5 = 0; a_5 < 6; a_5++)
                        {
                            if (Lk[k_1].x >= 0)
                            {
                                int3 tmp26 = c0;
                                float3 tmp27 = f;
                                int2 tmp28 = Lk[k_1];
                                int tmp29 = a_5;
                                float4 tmp30 = plain[a_5];
                                int3 tmp31 = dm;
                                tmp22 = volRoomAxis(tmp26, tmp27, tmp28, tmp29, tmp30, tmp31);
                            }
                            else
                            {
                                tmp22 = plain[a_5];
                            }
                            r[a_5] += (tmp22 * wl);
                        }
                    }
                }
            }
        }
    }
    bool tmp32 = volShell > 0.0f;
    bool tmp33;
    if (tmp32)
    {
        tmp33 = cellGiSky && false;
    }
    else
    {
        tmp33 = tmp32;
    }
    if (tmp33)
    {
        float4 wide[6];
        for (int a_6 = 0; a_6 < 6; a_6++)
        {
            wide[a_6] = 0.0f.xxxx;
        }
        for (int k_2 = 0; k_2 < 8; k_2++)
        {
            float3 q = g + (((float3(float(k_2 & 1), float((k_2 >> 1) & 1), float((k_2 >> 2) & 1)) * 2.0f) - 1.0f.xxx) * volShell);
            float qz = clamp(q.z, 0.5f, cellGiDims.z - 0.5f);
            for (int a_7 = 0; a_7 < 6; a_7++)
            {
                wide[a_7] += (cellGi.Sample(_cellGi_sampler, float3(q.xy / cellGiDims.xy, (qz + (float(a_7) * cellGiDims.z)) / depth)) * 0.125f);
            }
        }
        float3 tmp34;
        float3 tmp35;
        for (int a_8 = 0; a_8 < 6; a_8++)
        {
            if (r[a_8].w > 0.00999999977648258209228515625f)
            {
                tmp34 = r[a_8].xyz / r[a_8].w.xxx;
            }
            else
            {
                if (wide[a_8].w > 9.9999997473787516355514526367188e-05f)
                {
                    tmp35 = wide[a_8].xyz / wide[a_8].w.xxx;
                }
                else
                {
                    tmp35 = 0.0f.xxx;
                }
                tmp34 = tmp35;
            }
            float3 v = tmp34;
            share[a_8] = clamp(wide[a_8].w, 0.0f, 1.0f);
            E[a_8] = max(v * share[a_8], 0.0f.xxx);
        }
        return;
    }
    float3 tmp36;
    float3 tmp37;
    for (int a_9 = 0; a_9 < 6; a_9++)
    {
        if (cellGiSky && false)
        {
            tmp36 = max(r[a_9].xyz, 0.0f.xxx);
        }
        else
        {
            if (r[a_9].w > 0.00999999977648258209228515625f)
            {
                tmp37 = max(r[a_9].xyz / r[a_9].w.xxx, 0.0f.xxx);
            }
            else
            {
                tmp37 = 0.0f.xxx;
            }
            tmp36 = tmp37;
        }
        E[a_9] = tmp36;
        share[a_9] = clamp(plain[a_9].w, 0.0f, 1.0f);
    }
}
#endif

#if defined(WW_FN_volCubeAll_V2) || defined(WW_FN_ALL)
void volCubeAll(float3 P, inout float3 E[6], inout float share[6])
{
    if (cellGiAirOn)
    {
        float3 ga = (P - cellGiAirOrigin) / cellGiAirVoxel.xxx;
        float za = clamp(ga.z, 0.5f, cellGiAirDims.z - 0.5f);
        for (int a = 0; a < 6; a++)
        {
            E[a] = max(cellGiAir.SampleLevel(_cellGiAir_sampler, float3(ga.xy / cellGiAirDims.xy, (za + (float(a) * cellGiAirDims.z)) / (6.0f * cellGiAirDims.z)), 0.0f).xyz, 0.0f.xxx);
            share[a] = 1.0f;
        }
        return;
    }
    int3 dm = int3(cellGiDims);
    float3 g = (P - cellGiOrigin) / cellGiVoxel.xxx;
    float2 xy = g.xy / cellGiDims.xy;
    float z = clamp(g.z, 0.5f, cellGiDims.z - 0.5f);
    float depth = float(int3(spvTextureSize(cellGi, uint(0), dummy_parameter)).z);
    float4 plain[6];
    float4 r[6];
    for (int a_1 = 0; a_1 < 6; a_1++)
    {
        plain[a_1] = cellGi.SampleLevel(_cellGi_sampler, float3(xy, (z + (float(a_1) * cellGiDims.z)) / depth), 0.0f);
        r[a_1] = plain[a_1];
    }
    bool tmp1;
    if (cellGiRooms)
    {
        tmp1 = (cellPassRed & 8) == 0;
    }
    else
    {
        tmp1 = cellGiRooms;
    }
    if (tmp1)
    {
        float3 gv = g - 0.5f.xxx;
        int3 c0 = int3(floor(gv));
        float3 f = gv - float3(c0);
        if ((volRed & 4194304) != 0)
        {
            float3 tmp2 = P;
            int2 L = cellRoomAt(tmp2);
            if (L.x >= 0)
            {
                for (int a_2 = 0; a_2 < 6; a_2++)
                {
                    int3 tmp3 = c0;
                    float3 tmp4 = f;
                    int2 tmp5 = L;
                    int tmp6 = a_2;
                    float4 tmp7 = plain[a_2];
                    int3 tmp8 = dm;
                    r[a_2] = volRoomAxis(tmp3, tmp4, tmp5, tmp6, tmp7, tmp8);
                }
            }
        }
        else
        {
            float3 gr = ((P - cellRoomsOrigin) / cellRoomsCell.xxx) - 0.5f.xxx;
            int3 r0 = int3(floor(gr));
            float3 fr = gr - float3(r0);
            float wAir = 0.0f;
            bool same = true;
            float wk[8];
            int2 Lk[8];
            for (int k = 0; k < 8; k++)
            {
                int3 o = int3(k & 1, (k >> 1) & 1, (k >> 2) & 1);
                int3 c = r0 + o;
                float3 t = lerp(1.0f.xxx - fr, fr, float3(o));
                wk[k] = (t.x * t.y) * t.z;
                bool tmp9 = any(bool3(c.x < int3(0, 0, 0).x, c.y < int3(0, 0, 0).y, c.z < int3(0, 0, 0).z));
                bool tmp10;
                if (!tmp9)
                {
                    int3 tmp11 = int3(cellRoomsDims);
                    tmp10 = any(bool3(c.x >= tmp11.x, c.y >= tmp11.y, c.z >= tmp11.z));
                }
                else
                {
                    tmp10 = tmp9;
                }
                if (tmp10)
                {
                    Lk[k] = int2(-2, -2);
                }
                else
                {
                    float tmp12 = cellRooms.Load(int4(c, 0)).x;
                    Lk[k] = cellRoomUnpack(tmp12);
                }
                if (Lk[k].x == (-1))
                {
                    wk[k] = 0.0f;
                }
                wAir += wk[k];
                bool tmp13 = k > 0;
                bool tmp14;
                if (tmp13)
                {
                    tmp14 = any(bool2(Lk[k].x != Lk[0].x, Lk[k].y != Lk[0].y));
                }
                else
                {
                    tmp14 = tmp13;
                }
                if (tmp14)
                {
                    same = false;
                }
            }
            if (wAir > 0.0f)
            {
                if (same)
                {
                    if (Lk[0].x >= 0)
                    {
                        for (int a_3 = 0; a_3 < 6; a_3++)
                        {
                            int3 tmp15 = c0;
                            float3 tmp16 = f;
                            int2 tmp17 = Lk[0];
                            int tmp18 = a_3;
                            float4 tmp19 = plain[a_3];
                            int3 tmp20 = dm;
                            r[a_3] = volRoomAxis(tmp15, tmp16, tmp17, tmp18, tmp19, tmp20);
                        }
                    }
                }
                else
                {
                    for (int a_4 = 0; a_4 < 6; a_4++)
                    {
                        r[a_4] = 0.0f.xxxx;
                    }
                    float tmp21;
                    float4 tmp22;
                    for (int k_1 = 0; k_1 < 8; k_1++)
                    {
                        if (wk[k_1] <= 0.0f)
                        {
                            continue;
                        }
                        bool seen = false;
                        for (int j = 0; j < k_1; j++)
                        {
                            bool tmp23;
                            if (!seen)
                            {
                                bool tmp24 = wk[j] > 0.0f;
                                bool tmp25;
                                if (tmp24)
                                {
                                    tmp25 = all(bool2(Lk[j].x == Lk[k_1].x, Lk[j].y == Lk[k_1].y));
                                }
                                else
                                {
                                    tmp25 = tmp24;
                                }
                                tmp23 = tmp25;
                            }
                            else
                            {
                                tmp23 = seen;
                            }
                            seen = tmp23;
                        }
                        if (seen)
                        {
                            continue;
                        }
                        float wl = 0.0f;
                        for (int j_1 = k_1; j_1 < 8; j_1++)
                        {
                            if (all(bool2(Lk[j_1].x == Lk[k_1].x, Lk[j_1].y == Lk[k_1].y)))
                            {
                                tmp21 = wk[j_1];
                            }
                            else
                            {
                                tmp21 = 0.0f;
                            }
                            wl += tmp21;
                        }
                        wl /= wAir;
                        for (int a_5 = 0; a_5 < 6; a_5++)
                        {
                            if (Lk[k_1].x >= 0)
                            {
                                int3 tmp26 = c0;
                                float3 tmp27 = f;
                                int2 tmp28 = Lk[k_1];
                                int tmp29 = a_5;
                                float4 tmp30 = plain[a_5];
                                int3 tmp31 = dm;
                                tmp22 = volRoomAxis(tmp26, tmp27, tmp28, tmp29, tmp30, tmp31);
                            }
                            else
                            {
                                tmp22 = plain[a_5];
                            }
                            r[a_5] += (tmp22 * wl);
                        }
                    }
                }
            }
        }
    }
    bool tmp32 = volShell > 0.0f;
    bool tmp33;
    if (tmp32)
    {
        tmp33 = cellGiSky && false;
    }
    else
    {
        tmp33 = tmp32;
    }
    if (tmp33)
    {
        float4 wide[6];
        for (int a_6 = 0; a_6 < 6; a_6++)
        {
            wide[a_6] = 0.0f.xxxx;
        }
        for (int k_2 = 0; k_2 < 8; k_2++)
        {
            float3 q = g + (((float3(float(k_2 & 1), float((k_2 >> 1) & 1), float((k_2 >> 2) & 1)) * 2.0f) - 1.0f.xxx) * volShell);
            float qz = clamp(q.z, 0.5f, cellGiDims.z - 0.5f);
            for (int a_7 = 0; a_7 < 6; a_7++)
            {
                wide[a_7] += (cellGi.SampleLevel(_cellGi_sampler, float3(q.xy / cellGiDims.xy, (qz + (float(a_7) * cellGiDims.z)) / depth), 0.0f) * 0.125f);
            }
        }
        float3 tmp34;
        float3 tmp35;
        for (int a_8 = 0; a_8 < 6; a_8++)
        {
            if (r[a_8].w > 0.00999999977648258209228515625f)
            {
                tmp34 = r[a_8].xyz / r[a_8].w.xxx;
            }
            else
            {
                if (wide[a_8].w > 9.9999997473787516355514526367188e-05f)
                {
                    tmp35 = wide[a_8].xyz / wide[a_8].w.xxx;
                }
                else
                {
                    tmp35 = 0.0f.xxx;
                }
                tmp34 = tmp35;
            }
            float3 v = tmp34;
            share[a_8] = clamp(wide[a_8].w, 0.0f, 1.0f);
            E[a_8] = max(v * share[a_8], 0.0f.xxx);
        }
        return;
    }
    float3 tmp36;
    float3 tmp37;
    for (int a_9 = 0; a_9 < 6; a_9++)
    {
        if (cellGiSky && false)
        {
            tmp36 = max(r[a_9].xyz, 0.0f.xxx);
        }
        else
        {
            if (r[a_9].w > 0.00999999977648258209228515625f)
            {
                tmp37 = max(r[a_9].xyz / r[a_9].w.xxx, 0.0f.xxx);
            }
            else
            {
                tmp37 = 0.0f.xxx;
            }
            tmp36 = tmp37;
        }
        E[a_9] = tmp36;
        share[a_9] = clamp(plain[a_9].w, 0.0f, 1.0f);
    }
}
#endif

#if defined(WW_FN_volCubeScatter) || defined(WW_FN_ALL)
float3 volCubeScatter(float3 E[6], float3 dW)
{
    float3 fl = (((((E[0] + E[1]) + E[2]) + E[3]) + E[4]) + E[5]) * 0.666666686534881591796875f;
    float3 l1 = (((E[0] - E[1]) * dW.x) + ((E[2] - E[3]) * dW.y)) + ((E[4] - E[5]) * dW.z);
    return (max(fl + (l1 * (3.0f * volGi.w)), 0.0f.xxx) * 0.25f) * volGi.xyz;
}
#endif

#if defined(WW_FN_airAmbKeep) || defined(WW_FN_ALL)
float airAmbKeep(float fade, float r)
{
    float tmp1;
    if (r >= 0.0f)
    {
        tmp1 = (1.0f - fade) + (fade * r);
    }
    else
    {
        tmp1 = 1.0f - fade;
    }
    return tmp1;
}
#endif

#if defined(WW_FN_airEnvColour) || defined(WW_FN_ALL)
float3 airEnvColour(float3 J, float t, float z, float3 dirView)
{
    if ((volRed & 256) != 0)
    {
        return J;
    }
    float tmp1 = t;
    float tmp2 = z;
    float3 tmp3 = dirView;
    bool tmp4;
    float3 tmp5 = wwAirRecordColour(tmp1, tmp2, tmp3, tmp4);
    bool ok = tmp4;
    float3 c = tmp5;
    float3 tmp6;
    if (ok)
    {
        tmp6 = c * dot(J, float3(0.2125999927520751953125f, 0.715200006961822509765625f, 0.072200000286102294921875f));
    }
    else
    {
        tmp6 = J;
    }
    return tmp6;
}
#endif
