// HLSL source of features/water/water_contact.glsl (include-only GLSL), lane HLSLSRC3 (notes/hlslsrc3/tools/glsl2hlsl.py): the GLSL's comments,
// names and order kept. Its loose uniforms sit at the top under WW_LOOSE_UNIFORMS: the entry that includes this
// file includes it once more inside its LooseUniforms cbuffer with that defined (HLSL has one cbuffer for them).
#ifdef WW_HLSL_SRC
#if defined(WW_LOOSE_TYPES)	// the types its loose uniforms use

#elif defined(WW_LOOSE_UNIFORMS)
	// (rocks, posts, piers, hulls, actors). Each wave that strikes the object breaks against it and leaves a patch of foam
	// that drifts outward and decays; the next wave lays a fresh one, so rings of foam pulse out from the contact once a
	// wave period. The contact and its distance come from the depth behind the pixel (wwContactDist, water_ripple.glsl),
	// so no object list is needed. The strength follows WATERFOAMDETAIL1's energy rule (H^2.5, less in fresh water):
	// calm water gives at most a thin lap line. A bank the baked shore field already knows is left to the Shore foam row.
	// Needs water_shore.glsl and water_ripple.glsl before it.
	bool waterContactOn;
	int waterContactRed;	// 1 nodepth (no contact found), 2 flat (full energy on any water), 4 still (no pulse), 8 noshorecut,
	// 16 noflow (FIX1031B: no current-made foam)
	float4 waterContactK;	// band width / Hs, foam life (wave periods), wind-sea steepness Hs / L, foam opacity
#else
// lane WATERLOOK1 step 12 (WATERCONTACT1, src/features/water/cellwater.h): foam where the water meets anything standing in it


// the foam density 0..1 at distance d (u) from the contact: hs the significant wave height (u), pers the foam's
// persistence (1 sea, under 1 fresh water), t the clock (s), cxy the contact point (world xy), foot the pixel (u)
float wwContactLaw( float d, float hs, float pers, float t, float2 cxy, float foot )
{
	float hm = hs * kShoreM;
	float amount = ( waterContactRed & 2 ) != 0 ? 1.0 : clamp( pow( hm / waterShoreK2.w, 2.5 ) * pers, 0.0, 1.0 );
	// the period of a wind sea of this height at its usual steepness (L = Hs / s, deep water: T = sqrt( 2 pi L / g ))
	float T = clamp( sqrt( 6.2831853 * hm / ( waterContactK.z * 9.81 ) ), 1.0, 12.0 );
	float W = clamp( waterContactK.x * hs, 0.3 / kShoreM, 6.0 / kShoreM );	// the foam band's e-fold width (u)
	float v = W / ( waterContactK.y * T );	// the foam drifts out a band width in its life (u/s)
	// born at the contact when a wave strikes (bright), thinning until the next; the strike time drifts along the object
	float ph = ( t - d / v ) / T + 2.0 * wwShoreNoise( cxy / 140.0 );
	float pulse = exp( -4.0 * frac( ph ) );
	float env = 0.35 + 0.65 * ( 0.5 + 0.5 * cos( 6.2831853 * ph / 7.0 ) );	// wave groups of 7
	const float kMean = 0.2454;	// ( 1 - e^-4 ) / 4
	if ( ( waterContactRed & 4 ) != 0 ) {
		pulse = kMean;
		env = 0.675;
	}
	// rings closer than a few pixels show their mean, a band thinner than a pixel is fainter, not wider
	pulse = lerp( pulse, kMean, smoothstep( 0.25, 0.5, foot / ( v * T ) ) );
	float ring = amount * exp( -d / W ) * pulse * env * ( W / ( W + foot ) );
	// ripples run up the object about H: a thin translucent lap line (WATERFOAMDETAIL1's, from K3.w metres)
	float lap = ( 1.0 - smoothstep( 0.0, hs + foot, d ) ) * ( hs / ( hs + foot + 1e-4 ) )
		* smoothstep( 0.0, waterShoreK3.w, hm ) * waterShoreK3.z;
	return clamp( max( ring, lap ), 0.0, 1.0 );
}

// the contact foam at this pixel: vert = the water's height over the opaque point S behind it (u, 1e9 none).
// Returns (density 0..1, the contact distance u, trust, the pixel's foot u). Every pixel of a quad must call it.
float4 wwWaterContact( float vert, float3 P, float3 S, float3 eye, float t )
{
	float2 dir;
	float foot, trust;
	float d = wwContactDist( ( waterContactRed & 1 ) != 0 ? 1e9 : vert, P, S, eye, dir, foot, trust );
	if ( or( or( !waterContactOn, trust <= 0.0 ), d > 1e8 ) )
		return float4( 0.0, d, trust, foot );
	float4 wv = wwShoreWave( P.xy );
	/* lane FIX1031B (LATE ITEM 8): a river's current breaking on the object (piers, boats, rocks) makes foam by itself; the
	 * wind waves of a 40 m fetch give none (amount 0.001 at Cutler Bend). Amount ( v / 1.2 m/s )^2 (0.17 at 0.5 m/s, 0.43
	 * at 0.79), a steady band 1 m wide (e-fold) on the object's sides, broken by the shared foam texture. Red
	 * WW_CELL_WATER_CONTACT_RED=noflow (16). */
	float flowD = 0.0;
	if ( ( waterContactRed & 16 ) == 0 && waterFlowOn && abs( wwWaterInfo( P.xy ).w - 1.0 ) < 0.5 ) {
		float sm = length( wwWaterFlow( P.xy ) );	// m/s (the bake's flow)
		float Wf = 1.0 / kShoreM;
		flowD = clamp( sm * sm / 1.44, 0.0, 1.0 ) * exp( -d / Wf ) * ( Wf / ( Wf + foot ) );
	}
	if ( wv.w < 0.5 && flowD <= 0.0 )
		return float4( 0.0, d, trust, foot );
	float hs = max( wv.x, 1e-3 );
	float cut = 1.0;
	if ( ( waterContactRed & 8 ) == 0 ) {
		// a bank: the baked land lies about as far as the contact (within the bake's error); an object in open water
		// stands well inside the field's distance
		float2 field = wwShoreField( P.xy );
		if ( field.y > 0.5 )
			cut = smoothstep( 1.5, 2.5, max( field.x - waterShoreK0.x, 0.0 ) / ( d + waterShoreK4.z + 1.0 ) );
	}
	float wave = wv.w < 0.5 ? 0.0 : wwContactLaw( d, hs, wv.y, t, P.xy - d * dir, foot );
	float D = trust * cut * max( wave, flowD );
	return float4( D, d, trust, foot );
}

// the foam's alpha from its density, broken up by WATERFOAMDETAIL1's texture (as the shore and surf foam)
float wwContactFoamAlpha( float D, float2 P, float foot, float t )
{
	if ( D < 0.002 )
		return 0.0;
	float3 det = wwFoamDetail( P, foot, t );
	float s = lerp( 0.5, waterShoreK0.w, det.z );
	float a = smoothstep( 1.0 - D - s, 1.0 - D + s, det.x );
	return a * lerp( det.y, 1.0, D * D ) * min( 1.0, 1.5 * D ) * waterContactK.w;
}
#endif	// WW_LOOSE_UNIFORMS
#else	// WW_HLSL_SRC: the per-variant files' half (HLSLBULK1, WW_FN_<name> picks functions); it goes with the last of them
// Shared HLSL for res/shaders/features/water/water_contact.glsl (include-only GLSL), HLSLBULK1 round 2 (notes/hlslbulk1/tools/factor.py). No bindings: the
// functions below are the converted ones, they read the declarations of the shader that includes them (its cbuffers,
// resources, statics and LooseUniforms: the per-module shim and the entry own those). Define WW_FN_<name> for each
// wanted function before the #include (and #undef after), or WW_FN_ALL for all of them; a _V<n> suffix is a variant
// of the same GLSL function (a different define set at the entry). Fusion Shaders (F4FS) reuses the math from here.
// Included by: fo4_waterlook_ps.hlsl, fo4_waterlookd_ps.hlsl, fo4_waterlookdp_ps.hlsl, fo4_waterlookp_ps.hlsl

#if defined(WW_FN_wwContactLaw) || defined(WW_FN_ALL)
float wwContactLaw(float d, float hs, float pers, float t, float2 cxy, float foot)
{
    float hm = hs * 0.01428749971091747283935546875f;
    float amountPick;
    if ((waterContactRed & 2) != 0)
    {
        amountPick = 1.0f;
    }
    else
    {
        amountPick = clamp(pow(hm / waterShoreK2.w, 2.5f) * pers, 0.0f, 1.0f);
    }
    float amount = amountPick;
    // the period of a wind sea of this height at its usual steepness (L = Hs / s, deep water: T = sqrt( 2 pi L / g ))
    float T = clamp(sqrt((6.283185482025146484375f * hm) / (waterContactK.z * 9.81000041961669921875f)), 1.0f, 12.0f);
    // the foam band's e-fold width (u)
    float W = clamp(waterContactK.x * hs, 20.99737548828125f, 419.947509765625f);
    // the foam drifts out a band width in its life (u/s)
    float v = W / (waterContactK.y * T);
    float2 phPart = cxy / 140.0f.xx;
    // born at the contact when a wave strikes (bright), thinning until the next; the strike time drifts along the object
    float ph = ((t - (d / v)) / T) + (2.0f * wwShoreNoise(phPart));
    float pulse = exp((-4.0f) * frac(ph));
    // wave groups of 7
    float env = 0.3499999940395355224609375f + (0.64999997615814208984375f * (0.5f + (0.5f * cos((6.283185482025146484375f * ph) / 7.0f))));
    if ((waterContactRed & 4) != 0)
    {
        pulse = 0.24539999663829803466796875f;
        env = 0.675000011920928955078125f;
    }
    // rings closer than a few pixels show their mean, a band thinner than a pixel is fainter, not wider
    pulse = lerp(pulse, 0.24539999663829803466796875f, smoothstep(0.25f, 0.5f, foot / (v * T)));
    float ring = (((amount * exp((-d) / W)) * pulse) * env) * (W / (W + foot));
    // ripples run up the object about H: a thin translucent lap line (WATERFOAMDETAIL1's, from K3.w metres)
    float lap = (((1.0f - smoothstep(0.0f, hs + foot, d)) * (hs / ((hs + foot) + 9.9999997473787516355514526367188e-05f))) * smoothstep(0.0f, waterShoreK3.w, hm)) * waterShoreK3.z;
    return clamp(max(ring, lap), 0.0f, 1.0f);
}
#endif

#if defined(WW_FN_wwWaterContact) || defined(WW_FN_ALL)
float4 wwWaterContact(float vert, float3 P, float3 S, float3 eye, float t)
{
    float amountPick = ((waterContactRed & 1) != 0) ? 1000000000.0f : vert;
    float3 phPart = P;
    float3 dPart3 = S;
    float3 dPart2 = eye;
    float2 dPick3;
    float dPick2;
    float dPick;
    float dPart = wwContactDist(amountPick, phPart, dPart3, dPart2, dPick3, dPick2, dPick);
    float2 dir = dPick3;
    float foot = dPick2;
    float trust = dPick;
    float d = dPart;
    bool wwh0 = ((!waterContactOn) || (trust <= 0.0f));
    bool wwh1 = (d > 100000000.0f);
    if (or(wwh0, wwh1))   // HLSLFIX2 orfix.py: no short-circuit branch (NV took the wrong side)
    {
        return float4(0.0f, d, trust, foot);
    }
    float2 wvPart = P.xy;
    float4 wv = wwShoreWave(wvPart);
    // lane FIX1031B (LATE ITEM 8): a river's current breaking on the object (piers, boats, rocks) makes foam by itself; the
    // wind waves of a 40 m fetch give none (amount 0.001 at Cutler Bend). Amount ( v / 1.2 m/s )^2 (0.17 at 0.5 m/s, 0.43
    // at 0.79), a steady band 1 m wide (e-fold) on the object's sides, broken by the shared foam texture. Red
    // WW_CELL_WATER_CONTACT_RED=noflow (16).
    float flowD = 0.0f;
    bool cond2 = ((waterContactRed & 16) == 0) && waterFlowOn;
    bool cond;
    if (cond2)
    {
        float2 condPart = P.xy;
        cond = abs(wwWaterInfo(condPart).w - 1.0f) < 0.5f;
    }
    else
    {
        cond = cond2;
    }
    if (cond)
    {
        float2 smPart = P.xy;
        // m/s (the bake's flow)
        float sm = length(wwWaterFlow(smPart));
        float Wf = 69.99124908447265625f;
        flowD = (clamp((sm * sm) / 1.440000057220458984375f, 0.0f, 1.0f) * exp((-d) / Wf)) * (Wf / (Wf + foot));
    }
    if ((wv.w < 0.5f) && (flowD <= 0.0f))
    {
        return float4(0.0f, d, trust, foot);
    }
    float hs = max(wv.x, 0.001000000047497451305389404296875f);
    float cut = 1.0f;
    if ((waterContactRed & 8) == 0)
    {
        float2 fieldPart = P.xy;
        // a bank: the baked land lies about as far as the contact (within the bake's error); an object in open water
        // stands well inside the field's distance
        float2 field = wwShoreField(fieldPart);
        if (field.y > 0.5f)
        {
            cut = smoothstep(1.5f, 2.5f, max(field.x - waterShoreK0.x, 0.0f) / ((d + waterShoreK4.z) + 1.0f));
        }
    }
    float wavePick;
    if (wv.w < 0.5f)
    {
        wavePick = 0.0f;
    }
    else
    {
        float wavePart6 = d;
        float wavePart5 = hs;
        float wavePart4 = wv.y;
        float wavePart3 = t;
        float2 wavePart2 = P.xy - (dir * d);
        float wavePart = foot;
        wavePick = wwContactLaw(wavePart6, wavePart5, wavePart4, wavePart3, wavePart2, wavePart);
    }
    float wave = wavePick;
    float D_1 = (trust * cut) * max(wave, flowD);
    return float4(D_1, d, trust, foot);
}
#endif

#if defined(WW_FN_wwContactFoamAlpha) || defined(WW_FN_ALL)
float wwContactFoamAlpha(float D_1, float2 P, float foot, float t)
{
    if (D_1 < 0.00200000009499490261077880859375f)
    {
        return 0.0f;
    }
    float2 amountPick = P;
    float phPart = foot;
    float dPart3 = t;
    float3 det = wwFoamDetail(amountPick, phPart, dPart3);
    float s = lerp(0.5f, waterShoreK0.w, det.z);
    float a = smoothstep((1.0f - D_1) - s, (1.0f - D_1) + s, det.x);
    return ((a * lerp(det.y, 1.0f, D_1 * D_1)) * min(1.0f, 1.5f * D_1)) * waterContactK.w;
}
#endif
#endif	// WW_HLSL_SRC
