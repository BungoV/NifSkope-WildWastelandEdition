// Shared HLSL for res/shaders/features/water/water_caustics.glsl (include-only GLSL), HLSLBULK1 round 2 (notes/hlslbulk1/tools/factor.py). No bindings: the
// functions below are the converted ones, they read the declarations of the shader that includes them (its cbuffers,
// resources, statics and LooseUniforms: the per-module shim and the entry own those). Define WW_FN_<name> for each
// wanted function before the #include (and #undef after), or WW_FN_ALL for all of them; a _V<n> suffix is a variant
// of the same GLSL function (a different define set at the entry). Fusion Shaders (F4FS) reuses the math from here.
// Included by: fo4_waterlook_ps.hlsl, fo4_waterlookd_ps.hlsl, fo4_waterlookdp_ps.hlsl, fo4_waterlookp_ps.hlsl
// Not here (declarations, or no converted entry carried them): wwCauHessian
#ifdef WW_FN_ALL
static const float kWwCauN = 1.333;
static const float kWwCauG = 686.6;
static const float kWwCauLoop = 64.0;
static const float kWwCauTile = 8192.0;
static const int kWwCauQ1 = 203;
static const int kWwCauQ2 = 420;
static const uint kWwCauSeed1 = 101u;
static const uint kWwCauSeed2 = 202u;
static const float kWwCauR1 = 9.0;
static const float kWwCauR2 = 14.0;
static const float kWwCauRad = 0.25;
static const float kWwCauAw = 6.811;
static const float kWwCauKmax = 0.995;
static const float kWwCauLk = 77.57;
static const float kWwCauW0w = 0.5358;
static const float kWwCauW1w = 0.007923;
static const float kWwCauWn = 4.491;
static const float kWwCauLn = 52.5;
static const float kWwCauWv = 1.0;
static const float kWwCauPn = 3.38;
static const float kWwCauA2lo = 0.0;
static const float kWwCauA2hi = 1.2;
static const float kWwCauLa = 260.0;
static const float kWwCauA2s = 0.5811;
static const float kWwCauLs = 34.37;
static const float kWwCauWc = 8192.0 / 240.0;
static const int kWwCauWq = 1200;
static const float kWwCauWr = 81.0;
#endif

#if defined(WW_FN_wwCauLum) || defined(WW_FN_ALL)
float wwCauLum(float3 c)
{
    return dot(c, float3(0.2125999927520751953125f, 0.715200006961822509765625f, 0.072200000286102294921875f));
}
#endif

#if defined(WW_FN_wwCauHash) || defined(WW_FN_ALL)
uint wwCauHash(int2 p, uint s)
{
    uint v = ((uint(p.x) * 2376512323u) + (uint(p.y) * 3625334849u)) + (s * 3407524639u);
    v ^= (v >> uint(16));
    v *= 2146121005u;
    v ^= (v >> uint(15));
    v *= 2221713035u;
    v ^= (v >> uint(16));
    return v;
}
#endif

#if defined(WW_FN_wwCauTurn) || defined(WW_FN_ALL)
float2 wwCauTurn(uint v, float R, float lo, float span, float t, inout float jit)
{
    float ph = float(v & 65535u) / 65536.0f;
    jit = float((v >> uint(16)) & 32767u) / 32768.0f;
    float r = (((v >> uint(31)) != 0u) ? (-1.0f) : 1.0f) * round(R * (lo + (span * jit)));
    float turns = ph + ((r * t) / 64.0f);
    float th = 6.283185482025146484375f * frac(turns);
    return float2(cos(th), sin(th));
}
#endif

#if defined(WW_FN_wwCauFade) || defined(WW_FN_ALL)
float2 wwCauFade(float x)
{
    return float2(((x * x) * x) * ((x * ((6.0f * x) - 15.0f)) + 10.0f), (((30.0f * x) * x) * (x - 1.0f)) * (x - 1.0f));
}
#endif

#if defined(WW_FN_wwCauWarp) || defined(WW_FN_ALL)
float2 wwCauWarp(float2 xw, float t)
{
    float2 u = float2((3.0f * xw.x) + (4.0f * xw.y), ((-4.0f) * xw.x) + (3.0f * xw.y)) / 170.6666717529296875f.xx;
    float2 iu = floor(u);
    float2 f = u - iu;
    int2 i = int2(iu);
    float2 g[4];
    float wwCauTurnPick;
    for (int c = 0; c < 4; c++)
    {
        int2 ic = i + int2(c & 1, c >> 1);
        int2 j = int2((((3 * ic.x) - (4 * ic.y)) + 3600) % 1200, (((4 * ic.x) + (3 * ic.y)) + 3600) % 1200);
        int2 wwCauHashPart2 = j;
        uint wwCauHashPart = 11u;
        uint wwCauHashRet = wwCauHash(wwCauHashPart2, wwCauHashPart);
        float wwCauTurnPart4 = 81.0f;
        float wwCauTurnPart3 = 0.75f;
        float wwCauTurnPart2 = 0.5f;
        float wwCauTurnPart = t;
        float2 wwCauTurnRet = wwCauTurn(wwCauHashRet, wwCauTurnPart4, wwCauTurnPart3, wwCauTurnPart2, wwCauTurnPart, wwCauTurnPick);
        float jit = wwCauTurnPick;
        g[c] = wwCauTurnRet;
    }
    float va = dot(g[0], f);
    float vb = dot(g[1], f - float2(1.0f, 0.0f));
    float vc = dot(g[2], f - float2(0.0f, 1.0f));
    float vd = dot(g[3], f - 1.0f.xx);
    float k1 = vb - va;
    float k2 = vc - va;
    float k3 = ((va - vb) - vc) + vd;
    float2 G1 = g[1] - g[0];
    float2 G2 = g[2] - g[0];
    float2 G3 = ((g[0] - g[1]) - g[2]) + g[3];
    float sxPart = f.x;
    float2 sx = wwCauFade(sxPart);
    float syPart = f.y;
    float2 sy = wwCauFade(syPart);
    float nx = ((((g[0].x + (sx.y * k1)) + (sx.x * G1.x)) + (sy.x * G2.x)) + ((sx.y * sy.x) * k3)) + ((sx.x * sy.x) * G3.x);
    float ny = ((((g[0].y + (sx.x * G1.y)) + (sy.y * k2)) + (sy.x * G2.y)) + ((sx.x * sy.y) * k3)) + ((sx.x * sy.x) * G3.y);
    return float2((3.0f * nx) - (4.0f * ny), (4.0f * nx) + (3.0f * ny)) / 5.0f.xx;
}
#endif

#if defined(WW_FN_wwCauEdge) || defined(WW_FN_ALL)
float wwCauEdge(float2 p, int q, uint seed, float R, float t)
{
    float2 ip = floor(p);
    float2 f = p - ip;
    int2 i = int2(ip);
    float d1 = 1000000000.0f;
    float d2 = 1000000000.0f;
    float2 b1 = 0.0f.xx;
    float2 b2 = 0.0f.xx;
    float wwCauTurnPick;
    for (int oy = -1; oy <= 1; oy++)
    {
        for (int ox = -1; ox <= 1; ox++)
        {
            int2 c = ((i + int2(ox, oy)) + q.xx) % q.xx;
            int2 wwCauHashPart2 = c;
            uint wwCauHashPart = seed;
            uint h = wwCauHash(wwCauHashPart2, wwCauHashPart);
            uint wwCauHashRet = h;
            float wwCauTurnPart4 = R;
            float wwCauTurnPart3 = 0.60000002384185791015625f;
            float wwCauTurnPart2 = 0.800000011920928955078125f;
            float wwCauTurnPart = t;
            float2 wwCauTurnRet = wwCauTurn(wwCauHashRet, wwCauTurnPart4, wwCauTurnPart3, wwCauTurnPart2, wwCauTurnPart, wwCauTurnPick);
            float jit = wwCauTurnPick;
            float2 dir = wwCauTurnRet;
            // the orbit's centre, uniform in the cell: no lattice line in the spectrum
            uint h2 = h * 739982445u;
            float2 c0 = float2(float((h2 >> uint(8)) & 4095u), float((h2 >> uint(20)) & 4095u)) / 4096.0f.xx;
            float2 v = ((float2(float(ox), float(oy)) + c0) + (dir * (0.25f * (0.699999988079071044921875f + (0.300000011920928955078125f * jit))))) - f;
            float dd = dot(v, v);
            if (dd < d1)
            {
                d2 = d1;
                b2 = b1;
                d1 = dd;
                b1 = v;
            }
            else
            {
                if (dd < d2)
                {
                    d2 = dd;
                    b2 = v;
                }
            }
        }
    }
    return (d2 - d1) / (2.0f * max(length(b2 - b1), 9.9999999747524270787835121154785e-07f));
}
#endif

#if defined(WW_FN_wwCauMean) || defined(WW_FN_ALL)
float wwCauMean(float r)
{
    return (1.6918690204620361328125f * r) / ((1.0f + (1.67704498767852783203125f * r)) + (0.071092002093791961669921875f * sqrt(r)));
}
#endif

#if defined(WW_FN_wwCauNet) || defined(WW_FN_ALL)
float3 wwCauNet(float2 xw, float L, float fw, float t)
{
    float2 wwCauTurnPick = xw;
    float wwCauHashPart2 = t;
    float2 wn = wwCauWarp(wwCauTurnPick, wwCauHashPart2);
    float2 wp = xw + (wn * 6.81099987030029296875f);
    float c1 = 40.354679107666015625f;
    float c2 = 19.5047626495361328125f;
    float2 wwCauHashPart = mod(wp / c1.xx, 203.0f.xx);
    int wwCauHashRet = 203;
    uint wwCauTurnPart4 = 101u;
    float wwCauTurnPart3 = 9.0f;
    float wwCauTurnPart2 = t;
    float e1 = wwCauEdge(wwCauHashPart, wwCauHashRet, wwCauTurnPart4, wwCauTurnPart3, wwCauTurnPart2) * c1;
    float2 wwCauTurnPart = mod(wp / c2.xx, 420.0f.xx);
    int wwCauTurnRet = 420;
    uint sxPart = 202u;
    float syPart = 14.0f;
    float e2Part = t;
    float e2 = wwCauEdge(wwCauTurnPart, wwCauTurnRet, sxPart, syPart, e2Part) * c2;
    float w0 = ((0.53579998016357421875f + (0.007922999560832977294921875f * L)) + (4.49100017547607421875f * exp((-L) / 52.5f))) * exp(1.0f * wn.x);
    float a2 = (0.0f + (1.2000000476837158203125f * (1.0f - exp((-(L / 260.0f)) * (L / 260.0f))))) + (0.581099987030029296875f * exp((-L) / 34.369998931884765625f));
    float k = 0.99500000476837158203125f * (1.0f - exp((-(L / 77.56999969482421875f)) * (L / 77.56999969482421875f)));
    float3 E;
    for (int ch = 0; ch < 3; ch++)
    {
        float wc = w0 * (1.0f + float3(0.119999997317790985107421875f, 0.0f, -0.119999997317790985107421875f)[ch]);
        float w = sqrt((wc * wc) + ((0.25f * fw) * fw));
        float P = pow(1.0f + (e1 / w), -3.38000011444091796875f) + (a2 * pow(1.0f + (e2 / w), -3.38000011444091796875f));
        float EPart2 = w / c1;
        float EPart = w / c2;
        E[ch] = (1.0f - k) + ((k * P) / (wwCauMean(EPart2) + (a2 * wwCauMean(EPart))));
    }
    return E;
}
#endif

#if defined(WW_FN_wwCauEnvelope) || defined(WW_FN_ALL)
float wwCauEnvelope(float d, float L)
{
    if (waterCauRed == 1)
    {
        return 1.0f;
    }
    float wwCauTurnPick = L;
    float3 wwCauHashPart2 = wwWaterAbsorb(wwCauTurnPick);
    return (1.0f - smoothstep(0.0f, 700.0f, d)) * wwCauLum(wwCauHashPart2);
}
#endif

#if defined(WW_FN_wwWaterCaustic) || defined(WW_FN_ALL)
float3 wwWaterCaustic(float3 S, float d, float3 Ldir, float3 sun, float3 sky, float shadow, float fw, out float3 dbg)
{
    dbg = float3(1.0f, 0.0f, 0.0f);
    bool wwh0 = (Ldir.z <= 0.0f);
    bool wwh1 = (d <= 0.0f);
    if (or(wwh0, wwh1))   // HLSLFIX2 orfix.py: no short-circuit branch (NV took the wrong side)
    {
        return 1.0f.xxx;
    }
    float3 r = refract(-Ldir, float3(0.0f, 0.0f, 1.0f), 0.750187575817108154296875f);
    float L = d / max(-r.z, 0.0500000007450580596923828125f);
    float2 X = S.xy - (r.xy * L);
    float t = (waterCauRed == 3) ? 0.0f : waterCauTime;
    float3 I = 1.0f.xxx;
    if (waterCauRed == 5)
    {
        float2 wwCauTurnPick = X;
        float wwCauHashPart2 = t;
        float3 H = wwCauHessian(wwCauTurnPick, wwCauHashPart2);
        float s = L * 0.2498124539852142333984375f;
        float det = ((1.0f - (s * H.x)) * (1.0f - (s * H.y))) - (((s * s) * H.z) * H.z);
        float e = 0.0500000007450580596923828125f + (0.00200000009499490261077880859375f * L);
        I = min(sqrt(1.0f + (e * e)) * rsqrt((det * det) + (e * e)), 8.0f).xxx;
    }
    else
    {
        if (waterCauRed != 4)
        {
            float2 wwCauHashPart = X - (floor(X / 8192.0f.xx) * 8192.0f);
            float wwCauHashRet = L * wwCauPathGain;
            float wwCauTurnPart4 = fw;
            float wwCauTurnPart3 = t;
            I = wwCauNet(wwCauHashPart, wwCauHashRet, wwCauTurnPart4, wwCauTurnPart3);
        }
    }
    float sh = (waterCauRed == 2) ? 1.0f : shadow;
    float3 wwCauTurnPart2 = sun;
    float Es = (wwCauLum(wwCauTurnPart2) * Ldir.z) * sh;
    float3 wwCauTurnPart = sky;
    float share = Es / max(Es + wwCauLum(wwCauTurnPart), 9.9999999747524270787835121154785e-07f);
    float wwCauTurnRet = d;
    float sxPart = L;
    float env = wwCauEnvelope(wwCauTurnRet, sxPart);
    dbg = float3(I.y, env, share);
    return max(1.0f.xxx + ((I - 1.0f.xxx) * (share * env)), 0.0f.xxx);
}
#endif
