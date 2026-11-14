// HLSL source of features/water/water_depth.glsl (include-only GLSL), lane HLSLSRC3 (notes/hlslsrc3/tools/glsl2hlsl.py): the GLSL's comments,
// names and order kept. Its loose uniforms sit at the top under WW_LOOSE_UNIFORMS: the entry that includes this
// file includes it once more inside its LooseUniforms cbuffer with that defined (HLSL has one cbuffer for them).
#ifdef WW_HLSL_SRC
#if defined(WW_LOOSE_TYPES)	// the types its loose uniforms use

#elif defined(WW_LOOSE_UNIFORMS)
	int waterDepthRed;	// 1 flat (the bed depth read as 0), 2 noabsorb (no pure-water term), 3 nearest (no bilinear)
	bool waterDepthOn;	// lane WATERCOMBINE1: the Water depth colour row (fo4_waterlook carries every water look)
#else
/* lane WATERDEPTH1 (src/features/water/cellwater.h): the water's depth colour from the TRUE vertical bed depth.
 * Only fo4_waterlook.frag compiles this (WW_WATER_DEPTH); the default fo4_water program is the one it always was.
 *   bed depth  the baked .lodl depth (water_info.glsl), bilinear over the 4 texels around P (128 u a texel); a dry
 *              texel counts 0 (the shore), a wet texel at or under the ground (sloped / placed water) counts 0.
 *              Known only when a tap is wet and P lies within 128 u of that body's height (else: a placed water
 *              mesh at its own height, the game's term alone).
 *   who wins   the opaque scene behind the pixel (the game's term, per pixel, sees rocks and objects); the bake where
 *              the frame has no scene there, or the scene lies more than 256 u under the baked bed (the drawn
 *              bed is missing: far rings, holes in the LOD ground)
 *   along      the straight view ray to a flat bed at that depth: vert / |V.z| (the game's scene term's geometry)
 *   colour     the game's ramp (WATR shallow / deep colour, ranges, alphas, depth amount: fo4_water.frag depthFog);
 *              transmittance T = (1 - a) x exp( -k L ), a the game's opacity (the turbid share, flat in colour),
 *              k pure water's absorption (Pope & Fry 1997, 1/m at 650 / 550 / 450 nm), L the path in metres
 *   no scene   the bed is the record's shallow colour (the ramp's own zero-depth end)
 *   far        past the noise falloff (dfade 0) the game shows one reflection colour; with the depth known the
 *              body shows through it by 1 - F (Fresnel): grazing = the game's far colour
 * For later lanes (WATERCAUSTICS1): wwWaterBedDepth() is the vertical depth, wwWaterAbsorb() pure water's T. */



static const float kWwUnitMetres = 0.0142875;	// metres a game unit
static const float3 kWwPureWater = float3( 0.340, 0.0565, 0.00922 );	// Pope & Fry 1997, absorption 1/m at 650, 550, 450 nm

struct WwWaterBed
{
	bool baked;	// the bake wins at this pixel
	bool known;	// a depth is known (the scene's or the bake's)
	float vert;	// vertical depth, units
	float along;	// path along the view ray, units
	float bd;	// lane FIX1031B: the baked bed's vertical depth here (< 0 = not known)
};

// the vertical bed depth under the water surface point P (world units); < 0 = not known here
float wwWaterBedDepth( float3 P )
{
	if ( !waterInfoOn )
		return -1.0;
	float2 tc = ( P.xy - waterInfoGrid.xy ) / waterInfoGrid.z - 0.5;
	float2 t0 = floor( tc ), f = tc - t0;
	if ( waterDepthRed == 3 ) {
		t0 = floor( tc + 0.5 );
		f = (float2)( 0.0 );
	}
	float d[4];
	float h = 0.0;
	bool wet = false;
	for ( int k = 0; k < 4; k++ ) {
		float2 tk = t0 + float2( float( k & 1 ), float( k >> 1 ) );
		uint w = wwWaterInfoWord( waterInfoGrid.xy + ( tk + 0.5 ) * waterInfoGrid.z );
		uint id = w & 0xFFFFu;
		float code = float( w >> 24 ) / 255.0;
		d[k] = id != 0u ? code * code * 8192.0 : 0.0;
		if ( id != 0u && !wet ) {
			wet = true;
			h = waterInfoBodies.Load( int3(int2( int( id % 256u ), int( id / 256u ) ), 0) ).r;
		}
	}
	if ( or( !wet, abs( P.z - h ) > 128.0 ) )
		return -1.0;
	if ( waterDepthRed == 1 )
		return 0.0;
	float b = lerp( lerp( d[0], d[1], f.x ), lerp( d[2], d[3], f.x ), f.y );
	return max( b + ( P.z - h ), 0.0 );
}

// pure water's transmittance over `units` of path
float3 wwWaterAbsorb( float units )
{
	return waterDepthRed == 2 ? (float3)( 1.0 ) : exp( -kWwPureWater * ( units * kWwUnitMetres ) );
}

// which depth this pixel uses; sceneVert / sceneAlong = the game's (1e9 = no scene behind)
WwWaterBed wwWaterBedAt( float3 P, float3 V, float sceneVert, float sceneAlong )
{
	WwWaterBed b;
	float bd = wwWaterBedDepth( P );
	b.baked = bd >= 0.0 && ( or( sceneVert > 1e8, sceneVert > bd + 256.0 ) );
	b.known = or( b.baked, sceneVert < 1e8 );
	b.vert = b.baked ? bd : sceneVert;
	b.along = b.baked ? bd / max( -V.z, 1e-3 ) : sceneAlong;
	b.bd = bd;
	return b;
}

/* lane FIX1031B (the teeth, LATE ITEM 6.2): how much the baked bed counts at this pixel (0 = the drawn scene). The rule
 * above is a hard step where the scene lies 256 u under the baked bed; a coarse ground mesh (far LOD, 512-4096 u
 * triangles) crossing that line cut straight-edged teeth into the clear shallows (sheet_W0sea waterline 4x). soft = a
 * ramp from 64 u to 768 u under the baked bed, so the two beds blend; not soft = the step (red old) */
float wwWaterBedBakeW( float bd, float sceneVert, bool soft )
{
	if ( bd < 0.0 )
		return 0.0;
	if ( sceneVert > 1e8 )
		return 1.0;
	return soft ? smoothstep( bd + 64.0, bd + 768.0, sceneVert ) : ( sceneVert > bd + 256.0 ? 1.0 : 0.0 );
}

/* lane FIX1101A (LATE ITEM 7, bungo 10-31: "isn't the lod water too imprecise?"): the bake is 128 u a texel (1.8 m), so
 * near the camera the bed's depth and colour come PER PIXEL from the drawn scene behind the water (the bed, rocks, pier
 * legs: the same ground that shows above the waterline, so the two sides of the waterline match), and the bake only
 * stands in where the frame has no bed at all there (sceneVert > 1e8) or a true hole (the scene 2048 u under the bake).
 * Past the loaded cells (12288 u, 3 cells, to 16384 u) the drawn ground is the coarse far LOD: the rule above (the
 * scene loses where it lies 64..768 u under the bake) takes over, blended over that distance (no seam). The result
 * multiplies nothing: it REPLACES wwWaterBedBakeW's weight. Red WW_CELL_WATER_CLEAR_RED=bake (waterClearRed 6): the
 * rule above at every distance (today). */
float wwWaterBedNearW( float bd, float sceneVert, float dist, bool on )
{
	float far = wwWaterBedBakeW( bd, sceneVert, true );
	if ( or( or( !on, bd < 0.0 ), sceneVert > 1e8 ) )
		return far;
	float hole = smoothstep( bd + 2048.0, bd + 4096.0, sceneVert );
	return lerp( hole, far, smoothstep( 12288.0, 16384.0, dist ) );
}

// the game's depth colour over c (fogC its ramp colour, a its opacity) with pure water's absorption on top
float3 wwWaterDepthMix( float3 c, float3 fogC, float a, float along )
{
	float3 T = (float3)( 1.0 - a );
	if ( along < 1e8 )
		T *= wwWaterAbsorb( along );
	return lerp( fogC, c, T );
}

// past the noise falloff: the body through the game's far colour by 1 - F, where the depth is known
float3 wwWaterDepthFar( float3 far, float3 body, float F, bool known )
{
	return known ? lerp( body, far, F ) : far;
}
#endif	// WW_LOOSE_UNIFORMS
#else	// WW_HLSL_SRC: the per-variant files' half (HLSLBULK1, WW_FN_<name> picks functions); it goes with the last of them
// Shared HLSL for res/shaders/features/water/water_depth.glsl (include-only GLSL), HLSLBULK1 round 2 (notes/hlslbulk1/tools/factor.py). No bindings: the
// functions below are the converted ones, they read the declarations of the shader that includes them (its cbuffers,
// resources, statics and LooseUniforms: the per-module shim and the entry own those). Define WW_FN_<name> for each
// wanted function before the #include (and #undef after), or WW_FN_ALL for all of them; a _V<n> suffix is a variant
// of the same GLSL function (a different define set at the entry). Fusion Shaders (F4FS) reuses the math from here.
// Included by: fo4_waterlook_ps.hlsl, fo4_waterlookd_ps.hlsl, fo4_waterlookdp_ps.hlsl, fo4_waterlookm_ps.hlsl, fo4_waterlookp_ps.hlsl
#ifdef WW_FN_ALL
static const float kWwUnitMetres = 0.0142875;
#endif

#if defined(WW_FN_wwWaterBedDepth) || defined(WW_FN_ALL)
float wwWaterBedDepth(float3 P)
{
    if (!waterInfoOn)
    {
        return -1.0f;
    }
    float2 tc = ((P.xy - waterInfoGrid.xy) / waterInfoGrid.z.xx) - 0.5f.xx;
    float2 t0 = floor(tc);
    float2 f = tc - t0;
    if (waterDepthRed == 3)
    {
        t0 = floor(tc + 0.5f.xx);
        f = 0.0f.xx;
    }
    float h = 0.0f;
    bool wet = false;
    float d[4];
    float dPick;
    for (int k = 0; k < 4; k++)
    {
        float2 tk = t0 + float2(float(k & 1), float(k >> 1));
        float2 wPart = waterInfoGrid.xy + ((tk + 0.5f.xx) * waterInfoGrid.z);
        uint w = wwWaterInfoWord(wPart);
        uint id = w & 65535u;
        float code = float(w >> uint(24)) / 255.0f;
        if (id != 0u)
        {
            dPick = (code * code) * 8192.0f;
        }
        else
        {
            dPick = 0.0f;
        }
        d[k] = dPick;
        if ((id != 0u) && (!wet))
        {
            wet = true;
            h = waterInfoBodies.Load(int3(int2(int(id % 256u), int(id / 256u)), 0)).x;
        }
    }
    bool cond2 = !wet;
    bool cond;
    if (!cond2)
    {
        cond = abs(P.z - h) > 128.0f;
    }
    else
    {
        cond = cond2;
    }
    if (cond)
    {
        return -1.0f;
    }
    if (waterDepthRed == 1)
    {
        return 0.0f;
    }
    float b = lerp(lerp(d[0], d[1], f.x), lerp(d[2], d[3], f.x), f.y);
    return max(b + (P.z - h), 0.0f);
}
#endif

#if defined(WW_FN_wwWaterAbsorb) || defined(WW_FN_ALL)
float3 wwWaterAbsorb(float units)
{
    float3 dPick;
    if (waterDepthRed == 2)
    {
        dPick = 1.0f.xxx;
    }
    else
    {
        dPick = exp(float3(-0.3400000035762786865234375f, -0.0564999990165233612060546875f, -0.0092200003564357757568359375f) * (units * 0.01428749971091747283935546875f));
    }
    return dPick;
}
#endif

#if defined(WW_FN_wwWaterBedAt) || defined(WW_FN_ALL)
WwWaterBed wwWaterBedAt(float3 P, float3 V, float sceneVert, float sceneAlong)
{
    float3 dPick = P;
    float bd = wwWaterBedDepth(dPick);
    bool wPart = bd >= 0.0f;
    bool cond2;
    if (wPart)
    {
        bool cond = sceneVert > 100000000.0f;
        bool condPick;
        if (!cond)
        {
            condPick = sceneVert > (bd + 256.0f);
        }
        else
        {
            condPick = cond;
        }
        cond2 = condPick;
    }
    else
    {
        cond2 = wPart;
    }
    WwWaterBed b;
    b.baked = cond2;
    b.known = b.baked || (sceneVert < 100000000.0f);
    b.vert = b.baked ? bd : sceneVert;
    float bPick;
    if (b.baked)
    {
        bPick = bd / max(-V.z, 0.001000000047497451305389404296875f);
    }
    else
    {
        bPick = sceneAlong;
    }
    b.along = bPick;
    b.bd = bd;
    return b;
}
#endif

#if defined(WW_FN_wwWaterBedBakeW) || defined(WW_FN_ALL)
float wwWaterBedBakeW(float bd, float sceneVert, bool soft)
{
    if (bd < 0.0f)
    {
        return 0.0f;
    }
    if (sceneVert > 100000000.0f)
    {
        return 1.0f;
    }
    float dPick;
    if (soft)
    {
        dPick = smoothstep(bd + 64.0f, bd + 768.0f, sceneVert);
    }
    else
    {
        dPick = float(sceneVert > (bd + 256.0f));
    }
    return dPick;
}
#endif

#if defined(WW_FN_wwWaterBedNearW) || defined(WW_FN_ALL)
float wwWaterBedNearW(float bd, float sceneVert, float dist, bool on)
{
    float dPick = bd;
    float wPart = sceneVert;
    bool cond2 = true;
    float far = wwWaterBedBakeW(dPick, wPart, cond2);
    bool wwh0 = ((!on) || (bd < 0.0f));
    bool wwh1 = (sceneVert > 100000000.0f);
    if (or(wwh0, wwh1))   // HLSLFIX2 orfix.py: no short-circuit branch (NV took the wrong side)
    {
        return far;
    }
    float hole = smoothstep(bd + 2048.0f, bd + 4096.0f, sceneVert);
    return lerp(hole, far, smoothstep(12288.0f, 16384.0f, dist));
}
#endif

#if defined(WW_FN_wwWaterDepthMix) || defined(WW_FN_ALL)
float3 wwWaterDepthMix(float3 c, float3 fogC, float a, float along)
{
    float3 T = (1.0f - a).xxx;
    if (along < 100000000.0f)
    {
        float dPick = along;
        T *= wwWaterAbsorb(dPick);
    }
    return lerp(fogC, c, T);
}
#endif

#if defined(WW_FN_wwWaterDepthFar) || defined(WW_FN_ALL)
float3 wwWaterDepthFar(float3 far, float3 body, float F, bool known)
{
    float3 dPick;
    if (known)
    {
        dPick = lerp(body, far, F.xxx);
    }
    else
    {
        dPick = far;
    }
    return dPick;
}
#endif
#endif	// WW_HLSL_SRC
