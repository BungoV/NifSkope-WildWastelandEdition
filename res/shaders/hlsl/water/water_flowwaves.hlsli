// HLSL source of features/water/water_flowwaves.glsl (include-only GLSL), lane HLSLSRC3 (notes/hlslsrc3/tools/glsl2hlsl.py): the GLSL's comments,
// names and order kept. Its loose uniforms sit at the top under WW_LOOSE_UNIFORMS: the entry that includes this
// file includes it once more inside its LooseUniforms cbuffer with that defined (HLSL has one cbuffer for them).
#ifdef WW_HLSL_SRC
#if defined(WW_LOOSE_TYPES)	// the types its loose uniforms use

#elif defined(WW_LOOSE_UNIFORMS)
	// Two reads of the spectrum, each moved downstream by the flow for its own time, restarting every period half a period
	// apart; each fades to nothing at its restart, so the restarts never show (Vlachos 2010, "Water flow in Portal 2").
	// Two decorrelated fields mixed with weights a, b keep only a^2 + b^2 of the slope variance (the water would breathe
	// rougher and smoother each period): the mix is renormalised by the expected correlation of the two reads at the
	// distance the flow has carried them apart, taken from the spectrum itself (it dips below zero near the peak waves'
	// half wavelength), so still water reads exactly one sample (today's waves) and a slow current blends into it
	// continuously. The flow is bilinear between the bake's texels (its own texels are
	// nearest: the waves would tear along their edges).
	float4 waterOceanFlow;	// seconds moved of reads A and B (each wrapped by the period), their weights (sum 1)
	// per set (8 each): the slope field's correlation at d0 2^( i / 2 ), i 0..15, along the wind (4), then across it (4);
	// d0 = the finest cascade's texel (src/features/water/oceanspectrum.h slopeCorr)
	float4 waterOceanFlowCorr[16];
#else
// lane WATERLOOK1 step 5 (src/features/water/cellocean.h): the wind's waves carried along the baked river flow (WATERFLOWBAKE1).


float2 wwWaterFlowSmooth( float2 P )
{
	float tex = waterFlowGrid.z;
	float2 g = ( P - waterFlowGrid.xy ) / tex - 0.5;
	float2 t0 = floor( g ), f = g - t0;
	float2 c0 = waterFlowGrid.xy + ( t0 + 0.5 ) * tex;
	return lerp( lerp( wwWaterFlow( c0 ), wwWaterFlow( c0 + float2( tex, 0.0 ) ), f.x ),
		lerp( wwWaterFlow( c0 + float2( 0.0, tex ) ), wwWaterFlow( c0 + float2( tex, tex ) ), f.x ), f.y );
}

float wwOcFlowCorrAt( int base, float x )
{
	int i = min( int( x ), 14 );
	float f = x - float( i );
	return lerp( waterOceanFlowCorr[base + i / 4][i % 4], waterOceanFlowCorr[base + ( i + 1 ) / 4][( i + 1 ) % 4], f );
}

// 1 / the rms of a A + b B for unit fields correlated by how far apart the flow v (u/s) carried the two reads; set:
// the body's. Between the wind and across it the correlation is mixed by cos^2 of the flow's angle to the wind
float wwOcFlowNorm( float2 v, int set )
{
	float a = waterOceanFlow.z, b = waterOceanFlow.w;
	int st = max( set, 0 );
	float d = length( v ) * abs( waterOceanFlow.x - waterOceanFlow.y ) * kOcMetre;	// m
	float r = d / ( 1.0 / ( 128.0 * waterOceanInvL[st].z ) );	// in finest texels
	float x = clamp( 2.0 * log2( max( r, 1.0 ) ), 0.0, 15.0 );
	float c = dot( v, waterOceanWind.xy ) / max( length( v ), 1e-9 );
	float rho = lerp( wwOcFlowCorrAt( st * 8 + 4, x ), wwOcFlowCorrAt( st * 8, x ), c * c );
	if ( r < 1.0 )
		rho = 1.0 - ( 1.0 - rho ) * r * r;	// quadratic to 1 at no separation
	return rsqrt( max( a * a + b * b + 2.0 * rho * a * b, 1e-6 ) );
}

/* lane FIX1031B: the bake's flow is the WATR NAM0 linear velocity (WATERFLOWBAKE1: "FO4 NAM0 speeds are small; scale
 * in the look", and the look never did): median 0.40, max 1.6, read as u/s the waves moved 6 mm/s (bungo's 10-31 river
 * frames: flow on = off). NAM0 is the current the game's physics pushes floating objects with, and the physics works in
 * metres (kOcMetre): 0.4 m/s is a slow river. INFERRED from the numbers and the convention, not from a capture.
 * Rivers only: a lake's or the sea's one constant NAM0 (0.3-0.5) is the game's push on floating things, not a current
 * to carry the whole body's waves (an engineering choice; they keep the reading before).
 * Red WW_CELL_WATER_OCEAN_RED=rawflow (1024): the bake's numbers as u/s everywhere. */
static const float kOcFlowUnit = 1.0 / kOcMetre;	// u/s a bake unit (m/s)

// the spectrum at P (as wwWaterOcean) carried by the flow there; red noflow (256) reads it in place
WwOcean wwWaterOceanFlowed( float2 P, float2 dPx, float2 dPy )
{
	float2 v = ( waterOceanRed & 256 ) != 0 ? (float2)( 0.0 ) : wwWaterFlowSmooth( P )
		* ( or( ( waterOceanRed & 1024 ) != 0, abs( wwWaterInfo( P ).w - 1.0 ) > 0.5 ) ? 1.0 : kOcFlowUnit );
	if ( all(v == (float2)( 0.0 )) )
		return wwWaterOcean( P, dPx, dPy );
	int set;
	WwOcean A = wwWaterOceanQ( P, P - v * waterOceanFlow.x, dPx, dPy, set );
	WwOcean B = wwWaterOceanQ( P, P - v * waterOceanFlow.y, dPx, dPy, set );
	float a = waterOceanFlow.z, b = waterOceanFlow.w, n = wwOcFlowNorm( v, set );
	WwOcean o = A;
	o.slope = n * ( a * A.slope + b * B.slope );
	o.var = ( a * a * A.var + b * b * B.var ) / max( a * a + b * b, 1e-6 );
	o.white = a * A.white + b * B.white;
	o.cau = a * A.cau + b * B.cau;
	return o;
}
#endif	// WW_LOOSE_UNIFORMS
#else	// WW_HLSL_SRC: the per-variant files' half (HLSLBULK1, WW_FN_<name> picks functions); it goes with the last of them
// Shared HLSL for res/shaders/features/water/water_flowwaves.glsl (include-only GLSL), HLSLBULK1 round 2 (notes/hlslbulk1/tools/factor.py). No bindings: the
// functions below are the converted ones, they read the declarations of the shader that includes them (its cbuffers,
// resources, statics and LooseUniforms: the per-module shim and the entry own those). Define WW_FN_<name> for each
// wanted function before the #include (and #undef after), or WW_FN_ALL for all of them; a _V<n> suffix is a variant
// of the same GLSL function (a different define set at the entry). Fusion Shaders (F4FS) reuses the math from here.
// Included by: fo4_waterlook_ps.hlsl, fo4_waterlookd_ps.hlsl, fo4_waterlookdp_ps.hlsl, fo4_waterlookp_ps.hlsl

#if defined(WW_FN_wwWaterFlowSmooth) || defined(WW_FN_ALL)
float2 wwWaterFlowSmooth(float2 P)
{
    float tex = waterFlowGrid.z;
    float2 g = ((P - waterFlowGrid.xy) / tex.xx) - 0.5f.xx;
    float2 t0 = floor(g);
    float2 f = g - t0;
    float2 c0 = waterFlowGrid.xy + ((t0 + 0.5f.xx) * tex);
    float2 wwWaterFlowSmoothRet4 = c0;
    float2 wwWaterFlowSmoothRet3 = c0 + float2(tex, 0.0f);
    float2 wwWaterFlowSmoothRet2 = c0 + float2(0.0f, tex);
    float2 wwWaterFlowSmoothRet = c0 + tex.xx;
    return lerp(lerp(wwWaterFlow(wwWaterFlowSmoothRet4), wwWaterFlow(wwWaterFlowSmoothRet3), f.x.xx), lerp(wwWaterFlow(wwWaterFlowSmoothRet2), wwWaterFlow(wwWaterFlowSmoothRet), f.x.xx), f.y.xx);
}
#endif

#if defined(WW_FN_wwOcFlowCorrAt) || defined(WW_FN_ALL)
float wwOcFlowCorrAt(int base, float x)
{
    int i = min(int(x), 14);
    float f = x - float(i);
    return lerp(waterOceanFlowCorr[base + (i / 4)][i % 4], waterOceanFlowCorr[base + ((i + 1) / 4)][(i + 1) % 4], f);
}
#endif

#if defined(WW_FN_wwOcFlowNorm) || defined(WW_FN_ALL)
float wwOcFlowNorm(float2 v, int set)
{
    float a = waterOceanFlow.z;
    float b = waterOceanFlow.w;
    int st = max(set, 0);
    // m
    float d = (length(v) * abs(waterOceanFlow.x - waterOceanFlow.y)) * 0.01428749971091747283935546875f;
    // in finest texels
    float r = d / (1.0f / (128.0f * waterOceanInvL[st].z));
    float x = clamp(2.0f * log2(max(r, 1.0f)), 0.0f, 15.0f);
    float c = dot(v, waterOceanWind.xy) / max(length(v), 9.999999717180685365747194737196e-10f);
    int wwWaterFlowSmoothRet4 = (st * 8) + 4;
    float wwWaterFlowSmoothRet3 = x;
    int wwWaterFlowSmoothRet2 = st * 8;
    float wwWaterFlowSmoothRet = x;
    float rho = lerp(wwOcFlowCorrAt(wwWaterFlowSmoothRet4, wwWaterFlowSmoothRet3), wwOcFlowCorrAt(wwWaterFlowSmoothRet2, wwWaterFlowSmoothRet), c * c);
    if (r < 1.0f)
    {
        // quadratic to 1 at no separation
        rho = 1.0f - (((1.0f - rho) * r) * r);
    }
    return rsqrt(max(((a * a) + (b * b)) + (((2.0f * rho) * a) * b), 9.9999999747524270787835121154785e-07f));
}
#endif

#if defined(WW_FN_wwWaterOceanFlowed) || defined(WW_FN_ALL)
WwOcean wwWaterOceanFlowed(float2 P, float2 dPx, float2 dPy)
{
    float2 wwWaterFlowSmoothRet4;
    if ((waterOceanRed & 256) != 0)
    {
        wwWaterFlowSmoothRet4 = 0.0f.xx;
    }
    else
    {
        float2 wwWaterFlowSmoothRet3 = P;
        bool wwWaterFlowSmoothRet2 = (waterOceanRed & 1024) != 0;
        bool wwWaterFlowSmoothRet;
        if (!wwWaterFlowSmoothRet2)
        {
            float2 wwWaterFlowSmoothPart = P;
            wwWaterFlowSmoothRet = abs(wwWaterInfo(wwWaterFlowSmoothPart).w - 1.0f) > 0.5f;
        }
        else
        {
            wwWaterFlowSmoothRet = wwWaterFlowSmoothRet2;
        }
        wwWaterFlowSmoothRet4 = wwWaterFlowSmooth(wwWaterFlowSmoothRet3) * (wwWaterFlowSmoothRet ? 1.0f : 69.99124908447265625f);
    }
    float2 v = wwWaterFlowSmoothRet4;
    if (all(bool2(v.x == 0.0f.xx.x, v.y == 0.0f.xx.y)))
    {
        float2 wwWaterOceanFlowedRet3 = P;
        float2 wwWaterOceanFlowedRet2 = dPx;
        float2 wwWaterOceanFlowedRet = dPy;
        return wwWaterOcean(wwWaterOceanFlowedRet3, wwWaterOceanFlowedRet2, wwWaterOceanFlowedRet);
    }
    float2 APart5 = P;
    float2 APart4 = P - (v * waterOceanFlow.x);
    float2 APart3 = dPx;
    float2 APart2 = dPy;
    int APick;
    WwOcean APart = wwWaterOceanQ(APart5, APart4, APart3, APart2, APick);
    int set = APick;
    WwOcean A_1 = APart;
    float2 BPart5 = P;
    float2 BPart4 = P - (v * waterOceanFlow.y);
    float2 BPart3 = dPx;
    float2 BPart2 = dPy;
    int BPick;
    WwOcean BPart = wwWaterOceanQ(BPart5, BPart4, BPart3, BPart2, BPick);
    set = BPick;
    WwOcean B = BPart;
    float a = waterOceanFlow.z;
    float b = waterOceanFlow.w;
    float2 nPart2 = v;
    int nPart = set;
    float n = wwOcFlowNorm(nPart2, nPart);
    WwOcean o = A_1;
    o.slope = ((A_1.slope * a) + (B.slope * b)) * n;
    o.var = (((a * a) * A_1.var) + ((b * b) * B.var)) / max((a * a) + (b * b), 9.9999999747524270787835121154785e-07f);
    o.white = (a * A_1.white) + (b * B.white);
    o.cau = (a * A_1.cau) + (b * B.cau);
    return o;
}
#endif
#endif	// WW_HLSL_SRC
