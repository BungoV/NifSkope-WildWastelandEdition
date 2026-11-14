// HLSL source of features/clouds/cloud_march_step.glsl (include-only GLSL), lane HLSLSRC2 (notes/hlslsrc2/tools/glsl2hlsl.py): the GLSL's comments,
// names and order kept. Its loose uniforms sit at the top under WW_LOOSE_UNIFORMS: the entry that includes this
// file includes it once more inside its LooseUniforms cbuffer with that defined (HLSL has one cbuffer for them).
#ifdef WW_HLSL_SRC
// lane CLOUD4: one march sample at t over dt (cloud_march.frag includes it inside its loops: the plain one and the
// fly-through grid's fine one). Reads o, d, t, dt, pixKm, cosT, lo, hi; adds to L, T, dsum, wsum; `continue` = an
// empty sample, `break` = the ray is opaque.
	float3 p = o + d * t;
	float3 up;
	p.z = cvHeight( o, d, t, up );
	cvFootKm = t * pixKm;
	float dens = cvDens( p );
	if ( cvShaft.x > 0.0 && p.z < cvBands.baseKm.x )
		dens += cvShaftDens( p );	// lane CLOUDRECX1: the precipitation shafts (0 = none, today)
	if ( dens <= 0.0 )
		continue;
	cvIceNow = p.z < cvBands.topKm.x ? cvIce.x : cvIce.y;	// lane CLOUDRECX1: the band's ice share
	float tauL = 0.0;
	for ( int k = 0; k < cvTaps; k++ ) {
		float a = float( k ) / float( cvTaps ), b = float( k + 1 ) / float( cvTaps );
		float tk = cvLightKm * 0.5 * ( a * a + b * b ), dk = cvLightKm * ( b * b - a * a );
		float3 pl = p + cvKeyW * tk;
		pl.z = p.z + dot( up, cvKeyW ) * tk;
		tauL += cvDens( pl ) * kCvSigma * dk;
	}
#ifdef WW_CLOUD_VARIETY
	/* lane CLOUD5 round 2: a tower's column goes on past the taps' reach (6 km of a 7 km tower with the sun at 60 deg is
	 * 6.9 km along it): one tap at the middle of the rest, up to the Low band's top along the key, at most one reach
	 * long. Without it the floor's slab is too thin and, at albedo 0.99, the base 2.1x too bright (gate C, MEASURED). */
	{
		float muK = dot( up, cvKeyW );
		float rest = muK > 0.01 ? clamp( ( cvBands.topKm.x - p.z ) / muK - cvLightKm, 0.0, cvLightKm ) : 0.0;
		if ( rest > 0.0 ) {
			float tk = cvLightKm + 0.5 * rest;
			float3 pl = p + cvKeyW * tk;
			pl.z = p.z + muK * tk;
			tauL += cvDens( pl ) * kCvSigma * rest;
		}
	}
#endif
#ifdef WW_CLOUD_VARIETY
	// lane CLOUD5: the ambient's height ramp over the bands' span without the towers (cvAmbKm), so a storm tower in the
	// sky does not dim the small cumulus beside it
	float hf = clamp( ( p.z - lo ) / max( cvAmbKm, 1e-3 ), 0.0, 1.0 );
#else
	float hf = clamp( ( p.z - lo ) / max( hi - lo, 1e-3 ), 0.0, 1.0 );
#endif
	float mu0 = dot( up, cvKeyW );
	float3 key = cvKeyK * atTrans( cvTrans, _cvTrans_sampler, kRb + p.z, mu0 );
	float ph = cvOctaveSum( cosT, tauL );
	float3 phC = (float3)( ph );	// lane NIGHTHAZE1: the two-stream floor per colour when clouds.albedo is not grey
	bool alC = any(cvAlbedoD != (float3)( 0.0 ));
	// lane CLOUD2B: the octaves' floor: the diffuse light through the cloud above, as a slab of tauL mu0 (gate C)
	if ( cvMsOn > 0.5 && mu0 > 0.01 ) {
		float fd = mu0 * cvTdif( tauL * mu0, mu0, cvMsG, kCvAlbedo );
		float3 fdC = (float3)( fd );
		if ( alC )
			for ( int c = 0; c < 3; c++ )
				fdC[c] = mu0 * cvTdif( tauL * mu0, mu0, cvMsG, kCvAlbedo + cvAlbedoD[c] );
		/* lane CLOUD2C: what the cloud UNDER the sample sends back up (a thick top seen from above): its slab
		 * reflection (cvRdir) of the light reaching the sample, the direct beam and fd, the slab's depth from
		 * taps straight down. Seen along d it weighs (1 - g d.z) / 2: the phase's share of up-going light
		 * scattered toward the eye (Coakley & Chylek 1975: a backscatter fraction (1 - g) / 2). Gate R. */
		if ( cvTopOn > 0.5 ) {
			float tauB = 0.0;
			/* lane CLOUDLIGHT1: cvTopOn 2 (the default, WW_CLOUDVOL_TOPCOLUMN) takes the slab under the sample as uniform
			 * at the sample's own density down to its band's base (at most the taps' reach): no read, where the taps
			 * cost cvTaps (3 Medium / 6 Ultra) of the lit sample's 10 / 16. Gate tests/features/clouds/cloudlight_check.py
			 * (MEASURED: converged picture vs the taps <= 0.21 levels rms, gate R 0.957 / 1.253 vs MC). 1 = the taps. */
			if ( cvTopOn > 1.5 ) {
				float zb = p.z < cvBands.topKm.x ? cvBands.baseKm.x : cvBands.baseKm.y;
				tauB = dens * kCvSigma * clamp( p.z - zb, 0.0, cvLightKm );
			} else {
				for ( int k = 0; k < cvTaps; k++ ) {
					float a = float( k ) / float( cvTaps ), b = float( k + 1 ) / float( cvTaps );
					float tk = cvLightKm * 0.5 * ( a * a + b * b ), dk = cvLightKm * ( b * b - a * a );
					float3 pb = p - up * tk;
					pb.z = p.z - tk;
					tauB += cvDens( pb ) * kCvSigma * dk;
				}
			}
			float fu = cvRdir( tauB, mu0, cvMsG, kCvAlbedo ) * ( mu0 * exp( -tauL ) + fd );
			fd += 0.5 * ( 1.0 - cvMsG * d.z ) * fu;
			if ( alC )
				for ( int c = 0; c < 3; c++ )
					fdC[c] += 0.5 * ( 1.0 - cvMsG * d.z ) * cvRdir( tauB, mu0, cvMsG, kCvAlbedo + cvAlbedoD[c] ) * ( mu0 * exp( -tauL ) + fdC[c] );
		}
		ph = max( ph, fd / kPi );
		phC = alC ? max( phC, fdC / kPi ) : (float3)( ph );
	}
	float3 tint = p.z < cvBands.topKm.x ? cvTint[0] : cvTint[1];
	float ambF = lerp( 0.5, 1.0, hf );
	float3 ambC = (float3)( ambF );	// lane NIGHTHAZE1
	/* lane CLOUDBAND1 (b), RDR2's darker sky light inside thick cloud (Bauer 2019 darkens it by a sun-ward sample): the open
	 * sky's light reaching the sample is its transmission through the column above: the delta-Eddington scaled direct
	 * part exp(-(1 - g^2 w) tau / 0.6) + the two-stream Tdif, both at mu 0.6 (the diffusivity cosine), tau from three
	 * taps straight up to the top of the sample's band.
	 * Off keeps the 0.5 .. 1 height ramp (a dense deck's base kept half the sky: 7.9x the Monte Carlo at tau 40). */
	float tauU = 0.0;	// lane MODULES1: also the sky light's water path (0 with cvAmbDark off)
	if ( cvAmbDark > 0.5 ) {
		float topZ = p.z < cvBands.topKm.x ? cvBands.topKm.x : cvBands.topKm.y;
		float span = max( topZ - p.z, 0.0 );
		for ( int k = 0; k < 3; k++ ) {
			float tk = span * ( float( k ) + 0.5 ) / 3.0;
			float3 pu = p + up * tk;
			pu.z = p.z + tk;
			tauU += cvDens( pu ) * kCvSigma * span / 3.0;
		}
		// the direct part of the delta-Eddington scaled slab (the forward peak counts as passed through, as in cvTdif)
		ambF = exp( -( 1.0 - cvMsG * cvMsG * kCvAlbedo ) * tauU / 0.6 ) + cvTdif( tauU, 0.6, cvMsG, kCvAlbedo );
		ambC = (float3)( ambF );
		if ( alC )
			for ( int c = 0; c < 3; c++ ) {
				float wc = kCvAlbedo + cvAlbedoD[c];
				ambC[c] = exp( -( 1.0 - cvMsG * cvMsG * wc ) * tauU / 0.6 ) + cvTdif( tauU, 0.6, cvMsG, wc );
			}
	}
	// lane MODULES1: the light the droplets passed on, red absorbed by the water on its way in (cloudvolmath.h waterAbsorbK)
	float3 S = ( kCvAlbedo + cvAlbedoD ) * tint * ( key * phC * exp( -cvWaterK * tauL ) + cvAmbOn * cvAmb * ambC * exp( -cvWaterK * tauU ) );
	float tr = exp( -dens * kCvSigma * dt );
	float w = T * ( 1.0 - tr );
	L += S * w;	// Frostbite: S (1 - e^(-sigma dt)) / sigma x sigma_s, sigma_s = albedo sigma
	dsum += w * t;
	wsum += w;
	T *= tr;
	if ( T < 0.005 )
		break;
#else	// WW_HLSL_SRC: the per-variant files' half (HLSLBULK1, WW_FN_<name> picks functions); it goes with the last of them
// Shared HLSL for res/shaders/features/clouds/cloud_march_step.glsl (include-only GLSL), HLSLBULK1 round 2 (notes/hlslbulk1/tools/factor.py). No bindings: the
// functions below are the converted ones, they read the declarations of the shader that includes them (its cbuffers,
// resources, statics and LooseUniforms: the per-module shim and the entry own those). Define WW_FN_<name> for each
// wanted function before the #include (and #undef after), or WW_FN_ALL for all of them; a _V<n> suffix is a variant
// of the same GLSL function (a different define set at the entry). Fusion Shaders (F4FS) reuses the math from here.
// This GLSL holds declarations and macros only; the declarations live in the per-module shim.
#endif	// WW_HLSL_SRC
