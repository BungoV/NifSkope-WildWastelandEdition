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
    float airTerrHRet;
    if (wf > 0.0f)
    {
        airTerrHRet = lerp(hc, airTerr.Sample(_airTerr_sampler, float3(uf, 1.0f)).x, wf);
    }
    else
    {
        airTerrHRet = hc;
    }
    return airTerrHRet;
}
#endif

#if defined(WW_FN_airTerrH_V2) || defined(WW_FN_ALL)
float airTerrH(float2 Pw)
{
    float hc = airTerr.SampleLevel(_airTerr_sampler, float3((Pw.x * airTerrXf.x) + airTerrXf.y, (Pw.y * airTerrXf.z) + airTerrXf.w, 0.0f), 0.0f).x;
    float2 uf = float2((Pw.x * airTerrFineXf.x) + airTerrFineXf.y, (Pw.y * airTerrFineXf.z) + airTerrFineXf.w);
    float wf = clamp(min(min(uf.x, 1.0f - uf.x), min(uf.y, 1.0f - uf.y)) * 16.0f, 0.0f, 1.0f);
    float airTerrHRet;
    if (wf > 0.0f)
    {
        airTerrHRet = lerp(hc, airTerr.SampleLevel(_airTerr_sampler, float3(uf, 1.0f), 0.0f).x, wf);
    }
    else
    {
        airTerrHRet = hc;
    }
    return airTerrHRet;
}
#endif

#if defined(WW_FN_airGroundAt) || defined(WW_FN_ALL)
void airGroundAt(float3 Pw)
{
    float airTerrHRet;
    if (airTerrOn != 0)
    {
        float2 airTerrHPart = Pw.xy;
        airTerrHRet = airTerrH(airTerrHPart) - airTerrRef;
    }
    else
    {
        airTerrHRet = 0.0f;
    }
    airGz = airTerrHRet;
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
    float airTerrHRet = c;
    float airTerrHPart = volPhase[1].w;
    float volPhasePiRet2 = c;
    float volPhasePiRet = volPhase[2].w;
    return ((((volPhase[0].xyz * 0.0596831031143665313720703125f) * (1.0f + (c * c))) + (volPhase[1].xyz * volHg(airTerrHRet, airTerrHPart))) + (volPhase[2].xyz * volHg(volPhasePiRet2, volPhasePiRet))) * 3.1415927410125732421875f;
}
#endif

#if defined(WW_FN_airRho) || defined(WW_FN_ALL)
float airRho(float z)
{
    float airTerrHRet = z;
    float4 airTerrHPart = airMed;
    float4 volPhasePiRet2 = airLayer;
    float4 volPhasePiRet = float4(airGround.x + airGz, airGround.yzw);
    float airRhoRet = wwAirRhoLaw(airTerrHRet, airTerrHPart, volPhasePiRet2, volPhasePiRet);
    return airRhoRet;
}
#endif

#if defined(WW_FN_airCol) || defined(WW_FN_ALL)
float airCol(float lo, float hi)
{
    float airTerrHRet = lo;
    float airTerrHPart = hi;
    float4 volPhasePiRet2 = airMed;
    float4 volPhasePiRet = airLayer;
    float4 airRhoRet = float4(airGround.x + airGz, airGround.yzw);
    float airColRet = wwAirColLaw(airTerrHRet, airTerrHPart, volPhasePiRet2, volPhasePiRet, airRhoRet);
    return airColRet;
}
#endif

#if defined(WW_FN_airOd) || defined(WW_FN_ALL)
float airOd(float za, float zb, float len)
{
    float dz = zb - za;
    if (abs(dz) < 0.5f)
    {
        float airTerrHRet = 0.5f * (za + zb);
        return (airMed.x * len) * airRho(airTerrHRet);
    }
    float airTerrHPart = min(za, zb);
    float volPhasePiRet2 = max(za, zb);
    return ((airMed.x * len) * airCol(airTerrHPart, volPhasePiRet2)) / abs(dz);
}
#endif

#if defined(WW_FN_airTup) || defined(WW_FN_ALL)
float airTup(float z, inout float mu)
{
    float4 airTerrHRet = airMed;
    float4 airTerrHPart = airGround;
    if (z >= wwAirEnd(airTerrHRet, airTerrHPart))
    {
        return 1.0f;
    }
    mu = max(mu, 0.0199999995529651641845703125f);
    float4 volPhasePiRet2 = airMed;
    float4 volPhasePiRet = airGround;
    float airRhoRet = z;
    float airColRet = wwAirEnd(volPhasePiRet2, volPhasePiRet);
    return exp(((-airMed.x) * airCol(airRhoRet, airColRet)) / mu);
}
#endif

#if defined(WW_FN_airTupSun) || defined(WW_FN_ALL)
float airTupSun(float z, float mu)
{
    if (!airSunLayer)
    {
        float airTerrHRet = z;
        float airTerrHPart = mu;
        float volPhasePiRet2 = airTup(airTerrHRet, airTerrHPart);
        return volPhasePiRet2;
    }
    bool volPhasePiRet = z >= airMed.w;
    bool airRhoRet;
    if (!volPhasePiRet)
    {
        airRhoRet = airLayer.w <= 0.0f;
    }
    else
    {
        airRhoRet = volPhasePiRet;
    }
    if (airRhoRet)
    {
        return 1.0f;
    }
    float airColRet = z;
    float airTupSunRet3 = airMed.w;
    float airTupSunRet2 = airGround.x + airGz;
    float airTupSunRet = airGround.y;
    return exp((((-airMed.x) * airLayer.w) * wwAirColOne(airColRet, airTupSunRet3, airTupSunRet2, airTupSunRet)) / max(mu, 0.0199999995529651641845703125f));
}
#endif

#if defined(WW_FN_volRoomAxis) || defined(WW_FN_ALL)
float4 volRoomAxis(int3 c0, float3 f, int2 L, int a, float4 plain, int3 dm)
{
    float4 s = 0.0f.xxxx;
    float ws = 0.0f;
    int airTerrHRet;
    int airTerrHPart;
    for (int k = 0; k < 8; k++)
    {
        int3 o = int3(k & 1, (k >> 1) & 1, (k >> 2) & 1);
        int3 c = clamp(c0 + o, int3(0, 0, 0), dm - int3(1, 1, 1));
        float3 t = lerp(1.0f.xxx - f, f, float3(o));
        float w = (t.x * t.y) * t.z;
        float volPhasePiRet2 = cellGiSlots.Load(int4(c, 0)).x;
        int2 S = cellRoomUnpack(volPhasePiRet2);
        bool volPhasePiRet = S.x >= 0;
        bool airRhoRet;
        if (volPhasePiRet)
        {
            bool airColRet = S.x == L.x;
            bool airTupSunRet3;
            if (!airColRet)
            {
                airTupSunRet3 = S.x == L.y;
            }
            else
            {
                airTupSunRet3 = airColRet;
            }
            airRhoRet = airTupSunRet3;
        }
        else
        {
            airRhoRet = volPhasePiRet;
        }
        if (airRhoRet)
        {
            airTerrHRet = 0;
        }
        else
        {
            bool airTupSunRet2 = S.y >= 0;
            bool airTupSunRet;
            if (airTupSunRet2)
            {
                bool cond = S.y == L.x;
                bool airTupSunPick;
                if (!cond)
                {
                    airTupSunPick = S.y == L.y;
                }
                else
                {
                    airTupSunPick = cond;
                }
                airTupSunRet = airTupSunPick;
            }
            else
            {
                airTupSunRet = airTupSunRet2;
            }
            if (airTupSunRet)
            {
                airTerrHPart = 6 * dm.z;
            }
            else
            {
                airTerrHPart = -1;
            }
            airTerrHRet = airTerrHPart;
        }
        int base = airTerrHRet;
        bool wwh0 = (base < 0);
        bool wwh1 = (w <= 0.0f);
        if (or(wwh0, wwh1))   // HLSLFIX2 orfix.py: no short-circuit branch (NV took the wrong side)
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
        float4 volRoomAxisRet2;
        if (ws > 0.0f)
        {
            volRoomAxisRet2 = s / ws.xxxx;
        }
        else
        {
            volRoomAxisRet2 = plain;
        }
        return volRoomAxisRet2;
    }
    float4 volRoomAxisRet;
    if (ws >= 0.25f)
    {
        volRoomAxisRet = s / ws.xxxx;
    }
    else
    {
        volRoomAxisRet = (s + (plain * (0.25f - ws))) / 0.25f.xxxx;
    }
    return volRoomAxisRet;
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
    bool airTerrHRet;
    if (cellGiRooms)
    {
        airTerrHRet = (cellPassRed & 8) == 0;
    }
    else
    {
        airTerrHRet = cellGiRooms;
    }
    if (airTerrHRet)
    {
        float3 gv = g - 0.5f.xxx;
        int3 c0 = int3(floor(gv));
        float3 f = gv - float3(c0);
        if ((volRed & 4194304) != 0)
        {
            float3 airTerrHPart = P;
            int2 L = cellRoomAt(airTerrHPart);
            if (L.x >= 0)
            {
                for (int a_2 = 0; a_2 < 6; a_2++)
                {
                    int3 volPhasePiRet2 = c0;
                    float3 volPhasePiRet = f;
                    int2 airRhoRet = L;
                    int airColRet = a_2;
                    float4 airTupSunRet3 = plain[a_2];
                    int3 airTupSunRet2 = dm;
                    r[a_2] = volRoomAxis(volPhasePiRet2, volPhasePiRet, airRhoRet, airColRet, airTupSunRet3, airTupSunRet2);
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
                bool airTupSunRet = any(bool3(c.x < int3(0, 0, 0).x, c.y < int3(0, 0, 0).y, c.z < int3(0, 0, 0).z));
                bool cond;
                if (!airTupSunRet)
                {
                    int3 airTupSunPick = int3(cellRoomsDims);
                    cond = any(bool3(c.x >= airTupSunPick.x, c.y >= airTupSunPick.y, c.z >= airTupSunPick.z));
                }
                else
                {
                    cond = airTupSunRet;
                }
                if (cond)
                {
                    Lk[k] = int2(-2, -2);
                }
                else
                {
                    float volRoomAxisRet2 = cellRooms.Load(int4(c, 0)).x;
                    Lk[k] = cellRoomUnpack(volRoomAxisRet2);
                }
                if (Lk[k].x == (-1))
                {
                    wk[k] = 0.0f;
                }
                wAir += wk[k];
                bool volRoomAxisRet = k > 0;
                bool cond5;
                if (volRoomAxisRet)
                {
                    cond5 = any(bool2(Lk[k].x != Lk[0].x, Lk[k].y != Lk[0].y));
                }
                else
                {
                    cond5 = volRoomAxisRet;
                }
                if (cond5)
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
                            int3 rPart6 = c0;
                            float3 rPart5 = f;
                            int2 rPart4 = Lk[0];
                            int rPart3 = a_3;
                            float4 rPart2 = plain[a_3];
                            int3 rPart = dm;
                            r[a_3] = volRoomAxis(rPart6, rPart5, rPart4, rPart3, rPart2, rPart);
                        }
                    }
                }
                else
                {
                    for (int a_4 = 0; a_4 < 6; a_4++)
                    {
                        r[a_4] = 0.0f.xxxx;
                    }
                    float wlPick;
                    float4 rPick;
                    for (int k_1 = 0; k_1 < 8; k_1++)
                    {
                        if (wk[k_1] <= 0.0f)
                        {
                            continue;
                        }
                        bool seen = false;
                        for (int j = 0; j < k_1; j++)
                        {
                            bool seenPick;
                            if (!seen)
                            {
                                bool cond4 = wk[j] > 0.0f;
                                bool seenPick2;
                                if (cond4)
                                {
                                    seenPick2 = all(bool2(Lk[j].x == Lk[k_1].x, Lk[j].y == Lk[k_1].y));
                                }
                                else
                                {
                                    seenPick2 = cond4;
                                }
                                seenPick = seenPick2;
                            }
                            else
                            {
                                seenPick = seen;
                            }
                            seen = seenPick;
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
                                wlPick = wk[j_1];
                            }
                            else
                            {
                                wlPick = 0.0f;
                            }
                            wl += wlPick;
                        }
                        wl /= wAir;
                        for (int a_5 = 0; a_5 < 6; a_5++)
                        {
                            if (Lk[k_1].x >= 0)
                            {
                                int3 rPart12 = c0;
                                float3 rPart11 = f;
                                int2 rPart10 = Lk[k_1];
                                int rPart9 = a_5;
                                float4 rPart8 = plain[a_5];
                                int3 rPart7 = dm;
                                rPick = volRoomAxis(rPart12, rPart11, rPart10, rPart9, rPart8, rPart7);
                            }
                            else
                            {
                                rPick = plain[a_5];
                            }
                            r[a_5] += (rPick * wl);
                        }
                    }
                }
            }
        }
    }
    bool cond3 = volShell > 0.0f;
    bool cond2;
    if (cond3)
    {
        cond2 = cellGiSky && false;
    }
    else
    {
        cond2 = cond3;
    }
    if (cond2)
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
        float3 vPick;
        float3 vPick2;
        for (int a_8 = 0; a_8 < 6; a_8++)
        {
            if (r[a_8].w > 0.00999999977648258209228515625f)
            {
                vPick = r[a_8].xyz / r[a_8].w.xxx;
            }
            else
            {
                if (wide[a_8].w > 9.9999997473787516355514526367188e-05f)
                {
                    vPick2 = wide[a_8].xyz / wide[a_8].w.xxx;
                }
                else
                {
                    vPick2 = 0.0f.xxx;
                }
                vPick = vPick2;
            }
            float3 v = vPick;
            share[a_8] = clamp(wide[a_8].w, 0.0f, 1.0f);
            E[a_8] = max(v * share[a_8], 0.0f.xxx);
        }
        return;
    }
    float3 EPick;
    float3 EPick2;
    for (int a_9 = 0; a_9 < 6; a_9++)
    {
        if (cellGiSky && false)
        {
            EPick = max(r[a_9].xyz, 0.0f.xxx);
        }
        else
        {
            if (r[a_9].w > 0.00999999977648258209228515625f)
            {
                EPick2 = max(r[a_9].xyz / r[a_9].w.xxx, 0.0f.xxx);
            }
            else
            {
                EPick2 = 0.0f.xxx;
            }
            EPick = EPick2;
        }
        E[a_9] = EPick;
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
    bool airTerrHRet;
    if (cellGiRooms)
    {
        airTerrHRet = (cellPassRed & 8) == 0;
    }
    else
    {
        airTerrHRet = cellGiRooms;
    }
    if (airTerrHRet)
    {
        float3 gv = g - 0.5f.xxx;
        int3 c0 = int3(floor(gv));
        float3 f = gv - float3(c0);
        if ((volRed & 4194304) != 0)
        {
            float3 airTerrHPart = P;
            int2 L = cellRoomAt(airTerrHPart);
            if (L.x >= 0)
            {
                for (int a_2 = 0; a_2 < 6; a_2++)
                {
                    int3 volPhasePiRet2 = c0;
                    float3 volPhasePiRet = f;
                    int2 airRhoRet = L;
                    int airColRet = a_2;
                    float4 airTupSunRet3 = plain[a_2];
                    int3 airTupSunRet2 = dm;
                    r[a_2] = volRoomAxis(volPhasePiRet2, volPhasePiRet, airRhoRet, airColRet, airTupSunRet3, airTupSunRet2);
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
                bool airTupSunRet = any(bool3(c.x < int3(0, 0, 0).x, c.y < int3(0, 0, 0).y, c.z < int3(0, 0, 0).z));
                bool cond;
                if (!airTupSunRet)
                {
                    int3 airTupSunPick = int3(cellRoomsDims);
                    cond = any(bool3(c.x >= airTupSunPick.x, c.y >= airTupSunPick.y, c.z >= airTupSunPick.z));
                }
                else
                {
                    cond = airTupSunRet;
                }
                if (cond)
                {
                    Lk[k] = int2(-2, -2);
                }
                else
                {
                    float volRoomAxisRet2 = cellRooms.Load(int4(c, 0)).x;
                    Lk[k] = cellRoomUnpack(volRoomAxisRet2);
                }
                if (Lk[k].x == (-1))
                {
                    wk[k] = 0.0f;
                }
                wAir += wk[k];
                bool volRoomAxisRet = k > 0;
                bool cond5;
                if (volRoomAxisRet)
                {
                    cond5 = any(bool2(Lk[k].x != Lk[0].x, Lk[k].y != Lk[0].y));
                }
                else
                {
                    cond5 = volRoomAxisRet;
                }
                if (cond5)
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
                            int3 rPart6 = c0;
                            float3 rPart5 = f;
                            int2 rPart4 = Lk[0];
                            int rPart3 = a_3;
                            float4 rPart2 = plain[a_3];
                            int3 rPart = dm;
                            r[a_3] = volRoomAxis(rPart6, rPart5, rPart4, rPart3, rPart2, rPart);
                        }
                    }
                }
                else
                {
                    for (int a_4 = 0; a_4 < 6; a_4++)
                    {
                        r[a_4] = 0.0f.xxxx;
                    }
                    float wlPick;
                    float4 rPick;
                    for (int k_1 = 0; k_1 < 8; k_1++)
                    {
                        if (wk[k_1] <= 0.0f)
                        {
                            continue;
                        }
                        bool seen = false;
                        for (int j = 0; j < k_1; j++)
                        {
                            bool seenPick;
                            if (!seen)
                            {
                                bool cond4 = wk[j] > 0.0f;
                                bool seenPick2;
                                if (cond4)
                                {
                                    seenPick2 = all(bool2(Lk[j].x == Lk[k_1].x, Lk[j].y == Lk[k_1].y));
                                }
                                else
                                {
                                    seenPick2 = cond4;
                                }
                                seenPick = seenPick2;
                            }
                            else
                            {
                                seenPick = seen;
                            }
                            seen = seenPick;
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
                                wlPick = wk[j_1];
                            }
                            else
                            {
                                wlPick = 0.0f;
                            }
                            wl += wlPick;
                        }
                        wl /= wAir;
                        for (int a_5 = 0; a_5 < 6; a_5++)
                        {
                            if (Lk[k_1].x >= 0)
                            {
                                int3 rPart12 = c0;
                                float3 rPart11 = f;
                                int2 rPart10 = Lk[k_1];
                                int rPart9 = a_5;
                                float4 rPart8 = plain[a_5];
                                int3 rPart7 = dm;
                                rPick = volRoomAxis(rPart12, rPart11, rPart10, rPart9, rPart8, rPart7);
                            }
                            else
                            {
                                rPick = plain[a_5];
                            }
                            r[a_5] += (rPick * wl);
                        }
                    }
                }
            }
        }
    }
    bool cond3 = volShell > 0.0f;
    bool cond2;
    if (cond3)
    {
        cond2 = cellGiSky && false;
    }
    else
    {
        cond2 = cond3;
    }
    if (cond2)
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
        float3 vPick;
        float3 vPick2;
        for (int a_8 = 0; a_8 < 6; a_8++)
        {
            if (r[a_8].w > 0.00999999977648258209228515625f)
            {
                vPick = r[a_8].xyz / r[a_8].w.xxx;
            }
            else
            {
                if (wide[a_8].w > 9.9999997473787516355514526367188e-05f)
                {
                    vPick2 = wide[a_8].xyz / wide[a_8].w.xxx;
                }
                else
                {
                    vPick2 = 0.0f.xxx;
                }
                vPick = vPick2;
            }
            float3 v = vPick;
            share[a_8] = clamp(wide[a_8].w, 0.0f, 1.0f);
            E[a_8] = max(v * share[a_8], 0.0f.xxx);
        }
        return;
    }
    float3 EPick;
    float3 EPick2;
    for (int a_9 = 0; a_9 < 6; a_9++)
    {
        if (cellGiSky && false)
        {
            EPick = max(r[a_9].xyz, 0.0f.xxx);
        }
        else
        {
            if (r[a_9].w > 0.00999999977648258209228515625f)
            {
                EPick2 = max(r[a_9].xyz / r[a_9].w.xxx, 0.0f.xxx);
            }
            else
            {
                EPick2 = 0.0f.xxx;
            }
            EPick = EPick2;
        }
        E[a_9] = EPick;
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
    float airTerrHRet;
    if (r >= 0.0f)
    {
        airTerrHRet = (1.0f - fade) + (fade * r);
    }
    else
    {
        airTerrHRet = 1.0f - fade;
    }
    return airTerrHRet;
}
#endif

#if defined(WW_FN_airEnvColour) || defined(WW_FN_ALL)
float3 airEnvColour(float3 J, float t, float z, float3 dirView)
{
    if ((volRed & 256) != 0)
    {
        return J;
    }
    float airTerrHRet = t;
    float airTerrHPart = z;
    float3 volPhasePiRet2 = dirView;
    bool volPhasePiRet;
    float3 airRhoRet = wwAirRecordColour(airTerrHRet, airTerrHPart, volPhasePiRet2, volPhasePiRet);
    bool ok = volPhasePiRet;
    float3 c = airRhoRet;
    float3 airColRet;
    if (ok)
    {
        airColRet = c * dot(J, float3(0.2125999927520751953125f, 0.715200006961822509765625f, 0.072200000286102294921875f));
    }
    else
    {
        airColRet = J;
    }
    return airColRet;
}
#endif
