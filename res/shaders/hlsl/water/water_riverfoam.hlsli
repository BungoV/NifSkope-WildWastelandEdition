// HLSL source of features/water/water_riverfoam.glsl (include-only GLSL), lane HLSLSRC3 (notes/hlslsrc3/tools/glsl2hlsl.py): the GLSL's comments,
// names and order kept. Its loose uniforms sit at the top under WW_LOOSE_UNIFORMS: the entry that includes this
// file includes it once more inside its LooseUniforms cbuffer with that defined (HLSL has one cbuffer for them).
#ifdef WW_HLSL_SRC
#if defined(WW_LOOSE_TYPES)	// the types its loose uniforms use

#elif defined(WW_LOOSE_UNIFORMS)
	// (WATERFLOWBAKE1) and carried downstream by it. Pattern: each of the four flow texels around the pixel lays a streak
	// noise along its own flow (long along, narrow across) and the four are blended bilinearly (tiled directional flow,
	// van Hoesel 2010); each is read twice, half a foam life apart, fading out at its restart (Vlachos 2010, "Water flow
	// in Portal 2"). The texels share one noise in world space (origin snapped to 2048 u), so texels that flow alike draw
	// the same streaks and a streak crosses texels without dissolving; the blend is renormalised by the reads' own
	// correlation, so the foam keeps its cover anywhere. Amount: foam made by the moving water (half at the bake's median
	// speed) and gathered where the surface flow converges: steady foam c = s / ( 1 / T - C ) for a source s, life T and
	// convergence C = -div v, so the cover grows 1 / ( 1 - C T ) where the flow squeezes together, thins where it spreads.
	// Needs water_info.glsl (wwWaterFlow, wwWaterInfo) and water_shore.glsl (wwShoreNoise, wwShoreHash) before it.
	// Lane FIX1031B (LATE ITEM 8, foam): the bake's flow is m/s (the first FIX1031B agent's measure: 0.36-0.79 at Cutler Bend),
	// so the streaks are now carried at v / kRfMetre u/s (they crawled at 1/70 of the river) with an 8 s life (the Vlachos
	// period ~ one streak length at the river's speed), the source half at 0.4 m/s, and the source grows where the river runs
	// SHALLOW (under 1.5 m) or SHOALS along its flow (the bed rising downstream: turbulence, rapids): x ( 1 + 2 shallow +
	// 2 shoal ). Red WW_CELL_WATER_RIVERFOAM_RED=raw (16): the old reading (raw flow, no depth term; life from C++).
	bool waterRiverFoamOn;
	int waterRiverFoamRed;	// 1 nodiv (no gathering), 2 still (not carried), 4 iso (round blobs), 8 alias (no far fade), 16 raw
	float4 waterRiverFoamK;	// foam life (s), cover at full source, half-source speed (m/s), opacity
	float4 waterRiverFoamK2;	// streak width (u), length / width, edge softness (sd), gathering cap (C T)
#else
// lane WATERLOOK1 step 15 (WATERFLOW1, src/features/water/cellwater.h): foam streaks on baked rivers, drawn along the baked flow

static const float kRfMetre = 0.0142875;	// m per world unit

// one read's sd, and the value noise's correlation exp( -d^2 / kRfCorr ) at a lattice offset d (fit within 0.01),
// both measured: notes/waterlook1/tools/riverfoam_gl.py, vn_corr.py
static const float kRfSigma = 0.1586;
static const float kRfCorr = 0.593;

// one read of the streak noise, zero mean, at s = (along / length, across / width), o the read's offset
float wwRfStreak( float2 s, float2 o )
{
	return 0.65 * wwShoreNoise( s + o ) + 0.35 * wwShoreNoise( 2.03 * s + o.yx ) - 0.5;
}

// the read made a unit normal (Heitz and Neyret 2018, histogram-preserving blending), so any blend of reads has one
// distribution and the cover follows D at every weight; fit: notes/waterlook1/tools/rf_gauss_fit.py
float wwRfGauss( float x )
{
	float u = x * ( 1.0 / kRfSigma ), u2 = u * u;
	return u * ( 0.91999 + u2 * ( 0.02318 + u2 * 0.002227 ) );
}

// the correlation of two reads (same offset) whose coordinates differ by d
float wwRfRho( float2 d )
{
	float e = dot( d, d ) / kRfCorr;
	return ( 0.4225 * exp( -e ) + 0.1225 * exp( -4.1209 * e ) ) * ( 1.0 / 0.545 );
}

// (foam density 0..1, alpha 0..1, convergence x life, the flow's speed u/s) at world P; t the clock (s), foot the pixel (u)
float4 wwWaterRiverFoam( float2 P, float t, float foot )
{
	if ( or( !waterRiverFoamOn, !waterFlowOn ) )
		return (float4)( 0.0 );
	if ( abs( wwWaterInfo( P ).w - 1.0 ) > 0.5 )
		return (float4)( 0.0 );	// rivers only
	float tex = waterFlowGrid.z;
	float2 g = ( P - waterFlowGrid.xy ) / tex - 0.5;
	float2 t0 = floor( g ), f = g - t0;
	float2 c0 = waterFlowGrid.xy + ( t0 + 0.5 ) * tex;
	bool raw = ( waterRiverFoamRed & 16 ) != 0;
	float uq = raw ? 1.0 : 1.0 / kRfMetre;	// bake units -> u/s
	float2 vc[4] = { wwWaterFlow( c0 ), wwWaterFlow( c0 + float2( tex, 0.0 ) ), wwWaterFlow( c0 + float2( 0.0, tex ) ),
		wwWaterFlow( c0 + float2( tex, tex ) ) };
	for ( int i = 0; i < 4; i++ )
		vc[i] *= uq;
	float2 v = lerp( lerp( vc[0], vc[1], f.x ), lerp( vc[2], vc[3], f.x ), f.y );
	float spd = length( v );
	if ( spd < 1e-4 )
		return (float4)( 0.0 );
	// the bilinear field's divergence: d vx / dx + d vy / dy
	float div = ( lerp( vc[1].x - vc[0].x, vc[3].x - vc[2].x, f.y ) + lerp( vc[2].y - vc[0].y, vc[3].y - vc[1].y, f.x ) ) / tex;
	float T = waterRiverFoamK.x;
	float CT = ( waterRiverFoamRed & 1 ) != 0 ? 0.0 : clamp( -div * T, -9.0, waterRiverFoamK2.w );
	float sm = spd / uq;	// the source's speed in bake units (m/s; raw: u/s as before)
	float src = waterRiverFoamK.y * sm / ( sm + waterRiverFoamK.z );
	if ( !raw ) {
		float dHere = wwWaterInfo( P ).y * kRfMetre, dDown = wwWaterInfo( P + v / spd * tex ).y * kRfMetre;
		float shallow = 1.0 - smoothstep( 0.3, 1.5, dHere );
		float shoal = clamp( ( dHere - dDown ) / 0.5, 0.0, 1.0 );	// the bed rises 0.5 m within a texel downstream
		src *= 1.0 + 2.0 * shallow + 2.0 * shoal;
	}
	float D = clamp( src / ( 1.0 - CT ), 0.0, 1.0 );
	if ( D < 0.002 )
		return float4( 0.0, 0.0, CT, spd );
	float W = waterRiverFoamK2.x;
	float vis = ( waterRiverFoamRed & 8 ) != 0 ? 1.0 : 1.0 - smoothstep( 0.35, 0.7, foot / W );
	// cover D over the threshold of a unit-sd read (logistic fit of the normal quantile)
	float z = log( ( 1.0 - D ) / D ) / 1.702;
	float a = 0.0;
	if ( vis > 0.0 ) {
		float L = ( waterRiverFoamRed & 4 ) != 0 ? W : W * waterRiverFoamK2.y;
		float ph = frac( t / T );
		float2 wp = float2( 1.0 - abs( 2.0 * ph - 1.0 ), abs( 2.0 * ph - 1.0 ) );
		float2 tp = ( waterRiverFoamRed & 2 ) != 0 ? (float2)( 0.0 ) : float2( ph, frac( ph + 0.5 ) ) * T;
		float2 wc = (float2)( 1.0 ) - f;
		float4 wq = float4( wc.x * wc.y, f.x * wc.y, wc.x * f.y, f.x * f.y );
		float n = 0.0, var = 0.0;
		for ( int k = 0; k < 2; k++ ) {
			float2 o = k == 0 ? float2( 0.37, 0.71 ) : float2( 311.37, 97.71 );
			float2 s[4];
			for ( int i = 0; i < 4; i++ ) {
				float sp = length( vc[i] );
				float2 dir = sp > 1e-4 ? vc[i] / sp : float2( 1.0, 0.0 );
				float2 Pc = c0 + float2( float( i & 1 ), float( i >> 1 ) ) * tex;
				float2 blk = floor( Pc * ( 1.0 / 2048.0 ) );
				float2 r = P - 2048.0 * blk - vc[i] * tp[k];
				// each 2048 u block its own noise (a lattice offset by its hash): no repeat from block to block
				uint hb = wwShoreHash( int2( blk ) );
				s[i] = float2( dot( r, dir ) / L, dot( r, float2( -dir.y, dir.x ) ) / W )
					+ float2( float( hb & 1023u ), float( ( hb >> 10 ) & 1023u ) ) * 2.0;
				n += wp[k] * wq[i] * wwRfGauss( wwRfStreak( s[i], o ) );
			}
			for ( int i = 0; i < 4; i++ )
				for ( int j = 0; j < 4; j++ )
					var += wp[k] * wp[k] * wq[i] * wq[j] * ( i == j ? 1.0 : wwRfRho( s[i] - s[j] ) );
		}
		n /= sqrt( max( var, 1e-8 ) );	// unit sd at every weight
		float sf = waterRiverFoamK2.z;
		a = smoothstep( z - sf, z + sf, n );
	}
	a = lerp( D, a, vis );
	/* lane FIX1101A (P0, the manager's rvc6_on 10-31: "flat grey hard-edged blotches, like paint"): the streaks say where
	 * the foam GATHERS; inside them river foam is a lace of bubbles with holes, never a solid sheet. The shared foam
	 * texture (water_shore.glsl wwFoamDetail: 24 .. 0.3 u noise + a 1.2 u bubble lace) is thresholded by the streak's
	 * density (at most 60 % foam in a full streak: 40 % holes in its cores, tests/features/water/waterfoam2), as the contact and shore foam are, carried with the water (two reads
	 * half a life apart, crossfaded like the streaks). Red WW_CELL_WATER_RIVERFOAM_RED=paint (32): the solid streaks */
	if ( ( waterRiverFoamRed & 32 ) == 0 && a > 0.002 ) {
		float T2 = waterRiverFoamK.x, ph2 = frac( t / T2 );
		float w0 = 1.0 - abs( 2.0 * ph2 - 1.0 );
		float3 d0 = wwFoamDetail( P - v * ( ph2 * T2 ), foot, t ), d1 = wwFoamDetail( P - v * ( frac( ph2 + 0.5 ) * T2 ), foot, t );
		float3 det = lerp( d1, d0, w0 );
		float dens = 0.6 * a;
		float s = lerp( 0.5, waterShoreK0.w, det.z );
		a = smoothstep( 1.0 - dens - s, 1.0 - dens + s, det.x ) * lerp( det.y, 1.0, dens * dens ) * min( 1.0, 1.5 * dens );
	}
	a *= waterRiverFoamK.w;
	return float4( D, a, CT, spd );
}

/* lane FIX1101A (P0: a grey shape on the dry bank): the floating foams (river, contact, whitecaps) only on water: x the
 * water's own share the soft edge leaves (edgeV 1 = the water has become the scene it covers) and nothing over a column
 * under 4 u (6 cm), full from 16 u. Red WW_CELL_WATER_RIVERFOAM_RED=paint (32): 1 everywhere, as before */
float wwFoamWet( float edgeV, float column )
{
	return ( waterRiverFoamRed & 32 ) != 0 ? 1.0 : ( 1.0 - clamp( edgeV, 0.0, 1.0 ) ) * smoothstep( 4.0, 16.0, column );
}
#endif	// WW_LOOSE_UNIFORMS
#else	// WW_HLSL_SRC: the per-variant files' half (HLSLBULK1, WW_FN_<name> picks functions); it goes with the last of them
// Shared HLSL for res/shaders/features/water/water_riverfoam.glsl (include-only GLSL), HLSLBULK1 round 2 (notes/hlslbulk1/tools/factor.py). No bindings: the
// functions below are the converted ones, they read the declarations of the shader that includes them (its cbuffers,
// resources, statics and LooseUniforms: the per-module shim and the entry own those). Define WW_FN_<name> for each
// wanted function before the #include (and #undef after), or WW_FN_ALL for all of them; a _V<n> suffix is a variant
// of the same GLSL function (a different define set at the entry). Fusion Shaders (F4FS) reuses the math from here.
// Included by: fo4_waterlook_ps.hlsl, fo4_waterlookd_ps.hlsl, fo4_waterlookdp_ps.hlsl, fo4_waterlookm_ps.hlsl, fo4_waterlookp_ps.hlsl
#ifdef WW_FN_ALL
static const float kRfMetre = 0.0142875;
static const float kRfSigma = 0.1586;
static const float kRfCorr = 0.593;
#endif

#if defined(WW_FN_wwRfStreak) || defined(WW_FN_ALL)
float wwRfStreak(float2 s, float2 o)
{
    float2 wwRfStreakRet2 = s + o;
    float2 wwRfStreakRet = (s * 2.0299999713897705078125f) + o.yx;
    return ((0.64999997615814208984375f * wwShoreNoise(wwRfStreakRet2)) + (0.3499999940395355224609375f * wwShoreNoise(wwRfStreakRet))) - 0.5f;
}
#endif

#if defined(WW_FN_wwRfGauss) || defined(WW_FN_ALL)
float wwRfGauss(float x)
{
    float u = x * 6.3051700592041015625f;
    float u2 = u * u;
    return u * (0.919990003108978271484375f + (u2 * (0.023180000483989715576171875f + (u2 * 0.002226999960839748382568359375f))));
}
#endif

#if defined(WW_FN_wwRfRho) || defined(WW_FN_ALL)
float wwRfRho(float2 d)
{
    float e = dot(d, d) / 0.592999994754791259765625f;
    return ((0.42250001430511474609375f * exp(-e)) + (0.122500002384185791015625f * exp((-4.12090015411376953125f) * e))) * 1.83486235141754150390625f;
}
#endif

#if defined(WW_FN_wwWaterRiverFoam) || defined(WW_FN_ALL)
float4 wwWaterRiverFoam(float2 P, float t, float foot)
{
    if ((!waterRiverFoamOn) || (!waterFlowOn))
    {
        return 0.0f.xxxx;
    }
    float2 wwRfStreakRet2 = P;
    if (abs(wwWaterInfo(wwRfStreakRet2).w - 1.0f) > 0.5f)
    {
        return 0.0f.xxxx;
    }
    float tex = waterFlowGrid.z;
    float2 g = ((P - waterFlowGrid.xy) / tex.xx) - 0.5f.xx;
    float2 t0 = floor(g);
    float2 f = g - t0;
    float2 c0 = waterFlowGrid.xy + ((t0 + 0.5f.xx) * tex);
    bool raw = (waterRiverFoamRed & 16) != 0;
    // bake units -> u/s
    float uq = raw ? 1.0f : 69.99124908447265625f;
    float2 wwRfStreakRet = c0;
    float2 vcPart4 = c0 + float2(tex, 0.0f);
    float2 vcPart3 = c0 + float2(0.0f, tex);
    float2 vcPart2 = c0 + tex.xx;
    float2 vcPart[4] = { wwWaterFlow(wwRfStreakRet), wwWaterFlow(vcPart4), wwWaterFlow(vcPart3), wwWaterFlow(vcPart2) };
    float2 vc[4] = vcPart;
    for (int i = 0; i < 4; i++)
    {
        vc[i] *= uq;
    }
    float2 v = lerp(lerp(vc[0], vc[1], f.x.xx), lerp(vc[2], vc[3], f.x.xx), f.y.xx);
    float spd = length(v);
    if (spd < 9.9999997473787516355514526367188e-05f)
    {
        return 0.0f.xxxx;
    }
    // the bilinear field's divergence: d vx / dx + d vy / dy
    float div = (lerp(vc[1].x - vc[0].x, vc[3].x - vc[2].x, f.y) + lerp(vc[2].y - vc[0].y, vc[3].y - vc[1].y, f.x)) / tex;
    float T = waterRiverFoamK.x;
    float CTPick;
    if ((waterRiverFoamRed & 1) != 0)
    {
        CTPick = 0.0f;
    }
    else
    {
        CTPick = clamp((-div) * T, -9.0f, waterRiverFoamK2.w);
    }
    float CT = CTPick;
    // the source's speed in bake units (m/s; raw: u/s as before)
    float sm = spd / uq;
    float src = (waterRiverFoamK.y * sm) / (sm + waterRiverFoamK.z);
    if (!raw)
    {
        float2 dHerePart = P;
        float dHere = wwWaterInfo(dHerePart).y * 0.01428749971091747283935546875f;
        float2 dDownPart = P + ((v / spd.xx) * tex);
        float dDown = wwWaterInfo(dDownPart).y * 0.01428749971091747283935546875f;
        float shallow = 1.0f - smoothstep(0.300000011920928955078125f, 1.5f, dHere);
        // the bed rises 0.5 m within a texel downstream
        float shoal = clamp((dHere - dDown) / 0.5f, 0.0f, 1.0f);
        src *= ((1.0f + (2.0f * shallow)) + (2.0f * shoal));
    }
    float D_1 = clamp(src / (1.0f - CT), 0.0f, 1.0f);
    if (D_1 < 0.00200000009499490261077880859375f)
    {
        return float4(0.0f, 0.0f, CT, spd);
    }
    float W = waterRiverFoamK2.x;
    float visPick;
    if ((waterRiverFoamRed & 8) != 0)
    {
        visPick = 1.0f;
    }
    else
    {
        visPick = 1.0f - smoothstep(0.3499999940395355224609375f, 0.699999988079071044921875f, foot / W);
    }
    float vis = visPick;
    // cover D over the threshold of a unit-sd read (logistic fit of the normal quantile)
    float z = log((1.0f - D_1) / D_1) / 1.70200002193450927734375f;
    float a = 0.0f;
    if (vis > 0.0f)
    {
        float LPick;
        if ((waterRiverFoamRed & 4) != 0)
        {
            LPick = W;
        }
        else
        {
            LPick = W * waterRiverFoamK2.y;
        }
        float L = LPick;
        float ph = frac(t / T);
        float2 wp = float2(1.0f - abs((2.0f * ph) - 1.0f), abs((2.0f * ph) - 1.0f));
        float2 tpPick;
        if ((waterRiverFoamRed & 2) != 0)
        {
            tpPick = 0.0f.xx;
        }
        else
        {
            tpPick = float2(ph, frac(ph + 0.5f)) * T;
        }
        float2 tp = tpPick;
        float2 wc = 1.0f.xx - f;
        float4 wq = float4(wc.x * wc.y, f.x * wc.y, wc.x * f.y, f.x * f.y);
        float n = 0.0f;
        float var = 0.0f;
        float2 dirPick;
        float2 s[4];
        float varPick;
        for (int k = 0; k < 2; k++)
        {
            bool2 oPart = (k == 0).xx;
            float2 o = float2(oPart.x ? float2(0.37000000476837158203125f, 0.709999978542327880859375f).x : float2(311.3699951171875f, 97.70999908447265625f).x, oPart.y ? float2(0.37000000476837158203125f, 0.709999978542327880859375f).y : float2(311.3699951171875f, 97.70999908447265625f).y);
            for (int i_1 = 0; i_1 < 4; i_1++)
            {
                float sp = length(vc[i_1]);
                if (sp > 9.9999997473787516355514526367188e-05f)
                {
                    dirPick = vc[i_1] / sp.xx;
                }
                else
                {
                    dirPick = float2(1.0f, 0.0f);
                }
                float2 dir = dirPick;
                float2 Pc = c0 + (float2(float(i_1 & 1), float(i_1 >> 1)) * tex);
                float2 blk = floor(Pc * 0.00048828125f);
                float2 r = (P - (blk * 2048.0f)) - (vc[i_1] * tp[k]);
                int2 hbPart = int2(blk);
                // each 2048 u block its own noise (a lattice offset by its hash): no repeat from block to block
                uint hb = wwShoreHash(hbPart);
                s[i_1] = float2(dot(r, dir) / L, dot(r, float2(-dir.y, dir.x)) / W) + (float2(float(hb & 1023u), float((hb >> uint(10)) & 1023u)) * 2.0f);
                float2 wwRfStreakPart2 = s[i_1];
                float2 wwRfStreakPart = o;
                float wwRfStreakRet3 = wwRfStreak(wwRfStreakPart2, wwRfStreakPart);
                n += ((wp[k] * wq[i_1]) * wwRfGauss(wwRfStreakRet3));
            }
            for (int i_2 = 0; i_2 < 4; i_2++)
            {
                for (int j = 0; j < 4; j++)
                {
                    if (i_2 == j)
                    {
                        varPick = 1.0f;
                    }
                    else
                    {
                        float2 varPart = s[i_2] - s[j];
                        varPick = wwRfRho(varPart);
                    }
                    var += ((((wp[k] * wp[k]) * wq[i_2]) * wq[j]) * varPick);
                }
            }
        }
        // unit sd at every weight
        n /= sqrt(max(var, 9.9999999392252902907785028219223e-09f));
        float sf = waterRiverFoamK2.z;
        a = smoothstep(z - sf, z + sf, n);
    }
    a = lerp(D_1, a, vis);
    // lane FIX1101A (P0, the manager's rvc6_on 10-31: "flat grey hard-edged blotches, like paint"): the streaks say where
    // the foam GATHERS; inside them river foam is a lace of bubbles with holes, never a solid sheet. The shared foam
    // density (at most 60 % foam in a full streak: 40 % holes in its cores, tests/features/water/waterfoam2), as the contact and shore foam are, carried with the water (two reads
    // half a life apart, crossfaded like the streaks). Red WW_CELL_WATER_RIVERFOAM_RED=paint (32): the solid streaks
    if (((waterRiverFoamRed & 32) == 0) && (a > 0.00200000009499490261077880859375f))
    {
        float T2 = waterRiverFoamK.x;
        float ph2 = frac(t / T2);
        float w0 = 1.0f - abs((2.0f * ph2) - 1.0f);
        float2 d0Part3 = P - (v * (ph2 * T2));
        float d0Part2 = foot;
        float d0Part = t;
        float3 d0 = wwFoamDetail(d0Part3, d0Part2, d0Part);
        float2 d1Part3 = P - (v * (frac(ph2 + 0.5f) * T2));
        float d1Part2 = foot;
        float d1Part = t;
        float3 d1 = wwFoamDetail(d1Part3, d1Part2, d1Part);
        float3 det = lerp(d1, d0, w0.xxx);
        float dens = 0.60000002384185791015625f * a;
        float s_1 = lerp(0.5f, waterShoreK0.w, det.z);
        a = (smoothstep((1.0f - dens) - s_1, (1.0f - dens) + s_1, det.x) * lerp(det.y, 1.0f, dens * dens)) * min(1.0f, 1.5f * dens);
    }
    a *= waterRiverFoamK.w;
    return float4(D_1, a, CT, spd);
}
#endif

#if defined(WW_FN_wwFoamWet) || defined(WW_FN_ALL)
float wwFoamWet(float edgeV, float column)
{
    float wwRfStreakRet2;
    if ((waterRiverFoamRed & 32) != 0)
    {
        wwRfStreakRet2 = 1.0f;
    }
    else
    {
        wwRfStreakRet2 = (1.0f - clamp(edgeV, 0.0f, 1.0f)) * smoothstep(4.0f, 16.0f, column);
    }
    return wwRfStreakRet2;
}
#endif
#endif	// WW_HLSL_SRC
