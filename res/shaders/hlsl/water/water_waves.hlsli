// HLSL source of features/water/water_waves.glsl (include-only GLSL), lane HLSLSRC2 (notes/hlslsrc2/tools/glsl2hlsl.py): the GLSL's comments,
// names and order kept. Its loose uniforms sit at the top under WW_LOOSE_UNIFORMS: the entry that includes this
// file includes it once more inside its LooseUniforms cbuffer with that defined (HLSL has one cbuffer for them).
#ifdef WW_HLSL_SRC
#if defined(WW_LOOSE_TYPES)	// the types its loose uniforms use

#elif defined(WW_LOOSE_UNIFORMS)
	// Four sine components (Tessendorf 2001, "Simulating Ocean Water", deep-water dispersion w = sqrt( g k )), read from
	// WATERDATA1's baked body table (class + area) and shore plane (water_info.glsl); the game's three noise layers stay.
	int waterWavesOn;		// 1 while the Water swell row is on
	int waterWavesRed;		// 1 flat (every body the same lake size), 2 noshore (no shore fade), 4 alias (no footprint fade)
	float4 waterWavesClass[3];	// sea, river, lake: body length / wavelength, wavelength min, max (u), total steepness a k
	float4 waterWavesK;		// travel direction x, y (unit), the world clock in s, amplitude factor (the weather's wind)
	float4 waterWavesShore;	// waterline u, fade length / wavelength, area ramp start, end (texels)
#else
// lane WATERWAVES1 (src/features/water/cellwater.h): a slow calm-water swell, normal only, sized by the body it sits on.


// the components: wavelength share, steepness share, turn off the wind (radians: 0, +23, -31, +47 deg), phase
static const float4 kSwellLen = float4( 1.0, 0.71, 0.53, 0.37 );
static const float4 kSwellShare = float4( 0.40, 0.28, 0.20, 0.12 );
static const float4 kSwellTurn = float4( 0.0, 0.401426, -0.541052, 0.820305 );
static const float4 kSwellPhase = float4( 0.0, 1.7, 4.1, 2.9 );
static const float kSwellGravity = 686.6;	// 9.81 m/s^2 in game units (1 u = 0.0142875 m)

/* (slope x, slope y, weight, wavelength) at world xy P, clock t, footprint (world units a pixel). The slopes are
 * -dh/dx, -dh/dy and add to a normal's xy / z. The body and its shore distance come from the 4 texels around P
 * (bilinear like WATERSHORE1's field); a dry or other-body texel counts distance 0, so the swell dies at every shore
 * and at every body-to-body seam. No baked body under P (placed or interior water, the lookup off): weight 0. */
float4 wwWaterSwell( float2 P, float t, float footprint )
{
	float tex = waterInfoGrid.z;
	float2 g = ( P - waterInfoGrid.xy ) / tex - 0.5;
	float2 t0 = floor( g );
	float2 f = g - t0;
	float2 c0 = waterInfoGrid.xy + ( t0 + 0.5 ) * tex;
	uint w0 = wwWaterInfoWord( c0 );
	uint w1 = wwWaterInfoWord( c0 + float2( tex, 0.0 ) );
	uint w2 = wwWaterInfoWord( c0 + float2( 0.0, tex ) );
	uint w3 = wwWaterInfoWord( c0 + float2( tex, tex ) );
	float4 bw = float4( ( 1.0 - f.x ) * ( 1.0 - f.y ), f.x * ( 1.0 - f.y ), ( 1.0 - f.x ) * f.y, f.x * f.y );
	uint4 id = uint4( w0, w1, w2, w3 ) & 0xFFFFu;
	// the body: the wet corner of the largest weight (first in order on a tie)
	uint body = 0u;
	float best = -1.0;
	for ( int k = 0; k < 4; k++ )
		if ( id[k] != 0u && bw[k] > best ) {
			best = bw[k];
			body = id[k];
		}
	if ( body == 0u )
		return (float4)( 0.0 );
	float4 steps = float4( uint4( w0, w1, w2, w3 ) >> 16 & 0xFFu );
	float4 same = float4( (id == (uint4)( body )) );
	float dist = dot( bw, same * steps ) * waterInfoGrid.w;
	float4 row = waterInfoBodies.Load( int3(int2( int( body % 256u ), int( body / 256u ) ), 0) );
	int cls = clamp( int( row.g + 0.5 ), 0, 2 );
	float area = row.a;
	if ( ( waterWavesRed & 1 ) != 0 ) {
		cls = 2;
		area = 4096.0;
	}
	float4 C = waterWavesClass[cls];
	float lam = clamp( sqrt( area ) * tex / C.x, C.y, C.z );
	float wt = ( int( row.b + 0.5 ) & 16 ) != 0 && ( waterWavesRed & 1 ) == 0 ? 0.0
		: smoothstep( 0.0, 1.0, clamp( ( area - waterWavesShore.z ) / ( waterWavesShore.w - waterWavesShore.z ), 0.0, 1.0 ) );
	if ( ( waterWavesRed & 2 ) == 0 )
		wt *= smoothstep( 0.0, 1.0, clamp( ( dist - waterWavesShore.x ) / ( waterWavesShore.y * lam ), 0.0, 1.0 ) );
	if ( wt <= 0.0 )
		return float4( 0.0, 0.0, 0.0, lam );
	float S = C.w * waterWavesK.w;
	float2 grad = (float2)( 0.0 );
	for ( int k = 0; k < 4; k++ ) {
		float li = lam * kSwellLen[k];
		float kk = 6.2831853 / li;
		float om = sqrt( kSwellGravity * kk );
		float ca = cos( kSwellTurn[k] ), sa = sin( kSwellTurn[k] );
		float2 d = float2( ca * waterWavesK.x - sa * waterWavesK.y, sa * waterWavesK.x + ca * waterWavesK.y );
		// below 4 pixels a wavelength the component fades out, gone at 2 (Nyquist): no shimmer far away
		float aa = ( waterWavesRed & 4 ) != 0 ? 1.0 : clamp( ( li / max( footprint, 1e-3 ) - 2.0 ) * 0.5, 0.0, 1.0 );
		float th = kk * dot( d, P ) - om * t + kSwellPhase[k];
		grad += ( S * kSwellShare[k] * aa * cos( th ) ) * d;
	}
	return float4( -grad * wt, wt, lam );
}
#endif	// WW_LOOSE_UNIFORMS
#else	// WW_HLSL_SRC: the per-variant files' half (HLSLBULK1, WW_FN_<name> picks functions); it goes with the last of them
// Shared HLSL for res/shaders/features/water/water_waves.glsl (include-only GLSL), HLSLBULK1 round 2 (notes/hlslbulk1/tools/factor.py). No bindings: the
// functions below are the converted ones, they read the declarations of the shader that includes them (its cbuffers,
// resources, statics and LooseUniforms: the per-module shim and the entry own those). Define WW_FN_<name> for each
// wanted function before the #include (and #undef after), or WW_FN_ALL for all of them; a _V<n> suffix is a variant
// of the same GLSL function (a different define set at the entry). Fusion Shaders (F4FS) reuses the math from here.
// Included by: fo4_waterlook_ps.hlsl, fo4_waterlookd_ps.hlsl, fo4_waterlookdp_ps.hlsl, fo4_waterlookp_ps.hlsl
#ifdef WW_FN_ALL
static const float kSwellGravity = 686.6;
#endif

#if defined(WW_FN_wwWaterSwell) || defined(WW_FN_ALL)
float4 wwWaterSwell(float2 P, float t, float footprint)
{
    float tex = waterInfoGrid.z;
    float2 g = ((P - waterInfoGrid.xy) / tex.xx) - 0.5f.xx;
    float2 t0 = floor(g);
    float2 f = g - t0;
    float2 c0 = waterInfoGrid.xy + ((t0 + 0.5f.xx) * tex);
    float2 w0Part = c0;
    uint w0 = wwWaterInfoWord(w0Part);
    float2 w1Part = c0 + float2(tex, 0.0f);
    uint w1 = wwWaterInfoWord(w1Part);
    float2 w2Part = c0 + float2(0.0f, tex);
    uint w2 = wwWaterInfoWord(w2Part);
    float2 w3Part = c0 + tex.xx;
    uint w3 = wwWaterInfoWord(w3Part);
    float4 bw = float4((1.0f - f.x) * (1.0f - f.y), f.x * (1.0f - f.y), (1.0f - f.x) * f.y, f.x * f.y);
    uint4 id = uint4(w0, w1, w2, w3) & uint4(65535u, 65535u, 65535u, 65535u);
    // the body: the wet corner of the largest weight (first in order on a tie)
    uint body = 0u;
    float best = -1.0f;
    for (int k = 0; k < 4; k++)
    {
        bool cond4 = id[k] != 0u;
        bool cond3;
        if (cond4)
        {
            cond3 = bw[k] > best;
        }
        else
        {
            cond3 = cond4;
        }
        if (cond3)
        {
            best = bw[k];
            body = id[k];
        }
    }
    if (body == 0u)
    {
        return 0.0f.xxxx;
    }
    float4 steps = float4((uint4(w0, w1, w2, w3) >> uint4(int4(16, 16, 16, 16))) & uint4(255u, 255u, 255u, 255u));
    uint4 samePart = body.xxxx;
    float4 same = float4(bool4(id.x == samePart.x, id.y == samePart.y, id.z == samePart.z, id.w == samePart.w));
    float dist = dot(bw, same * steps) * waterInfoGrid.w;
    float4 row = waterInfoBodies.Load(int3(int2(int(body % 256u), int(body / 256u)), 0));
    int cls = clamp(int(row.y + 0.5f), 0, 2);
    float area = row.w;
    if ((waterWavesRed & 1) != 0)
    {
        cls = 2;
        area = 4096.0f;
    }
    float4 C_1 = waterWavesClass[cls];
    float lam = clamp((sqrt(area) * tex) / C_1.x, C_1.y, C_1.z);
    bool cond2 = (int(row.z + 0.5f) & 16) != 0;
    bool cond;
    if (cond2)
    {
        cond = (waterWavesRed & 1) == 0;
    }
    else
    {
        cond = cond2;
    }
    float wtPick;
    if (cond)
    {
        wtPick = 0.0f;
    }
    else
    {
        wtPick = smoothstep(0.0f, 1.0f, clamp((area - waterWavesShore.z) / (waterWavesShore.w - waterWavesShore.z), 0.0f, 1.0f));
    }
    float wt = wtPick;
    if ((waterWavesRed & 2) == 0)
    {
        wt *= smoothstep(0.0f, 1.0f, clamp((dist - waterWavesShore.x) / (waterWavesShore.y * lam), 0.0f, 1.0f));
    }
    if (wt <= 0.0f)
    {
        return float4(0.0f, 0.0f, 0.0f, lam);
    }
    float S = C_1.w * waterWavesK.w;
    float2 grad = 0.0f.xx;
    float aaPick;
    for (int k_1 = 0; k_1 < 4; k_1++)
    {
        float li = lam * float4(1.0f, 0.709999978542327880859375f, 0.5299999713897705078125f, 0.37000000476837158203125f)[k_1];
        float kk = 6.283185482025146484375f / li;
        float om = sqrt(686.5999755859375f * kk);
        float ca = cos(float4(0.0f, 0.4014259874820709228515625f, -0.54105198383331298828125f, 0.82030498981475830078125f)[k_1]);
        float sa = sin(float4(0.0f, 0.4014259874820709228515625f, -0.54105198383331298828125f, 0.82030498981475830078125f)[k_1]);
        float2 d = float2((ca * waterWavesK.x) - (sa * waterWavesK.y), (sa * waterWavesK.x) + (ca * waterWavesK.y));
        if ((waterWavesRed & 4) != 0)
        {
            aaPick = 1.0f;
        }
        else
        {
            aaPick = clamp(((li / max(footprint, 0.001000000047497451305389404296875f)) - 2.0f) * 0.5f, 0.0f, 1.0f);
        }
        // below 4 pixels a wavelength the component fades out, gone at 2 (Nyquist): no shimmer far away
        float aa = aaPick;
        float th = ((kk * dot(d, P)) - (om * t)) + float4(0.0f, 1.7000000476837158203125f, 4.099999904632568359375f, 2.900000095367431640625f)[k_1];
        grad += (d * (((S * float4(0.4000000059604644775390625f, 0.2800000011920928955078125f, 0.20000000298023223876953125f, 0.119999997317790985107421875f)[k_1]) * aa) * cos(th)));
    }
    return float4((-grad) * wt, wt, lam);
}
#endif
#endif	// WW_HLSL_SRC
