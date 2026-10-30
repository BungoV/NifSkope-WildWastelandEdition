// lane CLOUD4: one march sample at t over dt (cloud_march.frag includes it inside its loops: the plain one and the
// fly-through grid's fine one). Reads o, d, t, dt, pixKm, cosT, lo, hi; adds to L, T, dsum, wsum; `continue` = an
// empty sample, `break` = the ray is opaque.
	vec3 p = o + d * t;
	vec3 up;
	p.z = cvHeight( o, d, t, up );
	cvFootKm = t * pixKm;
	float dens = cvDens( p );
	if ( dens <= 0.0 )
		continue;
	float tauL = 0.0;
	for ( int k = 0; k < cvTaps; k++ ) {
		float a = float( k ) / float( cvTaps ), b = float( k + 1 ) / float( cvTaps );
		float tk = cvLightKm * 0.5 * ( a * a + b * b ), dk = cvLightKm * ( b * b - a * a );
		vec3 pl = p + cvKeyW * tk;
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
			vec3 pl = p + cvKeyW * tk;
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
	vec3 key = cvKeyK * atTrans( cvTrans, kRb + p.z, mu0 );
	float ph = cvOctaveSum( cosT, tauL );
	// lane CLOUD2B: the octaves' floor: the diffuse light through the cloud above, as a slab of tauL mu0 (gate C)
	if ( cvMsOn > 0.5 && mu0 > 0.01 ) {
		float fd = mu0 * cvTdif( tauL * mu0, mu0, cvMsG, kCvAlbedo );
		/* lane CLOUD2C: what the cloud UNDER the sample sends back up (a thick top seen from above): its slab
		 * reflection (cvRdir) of the light reaching the sample, the direct beam and fd, the slab's depth from
		 * taps straight down. Seen along d it weighs (1 - g d.z) / 2: the phase's share of up-going light
		 * scattered toward the eye (Coakley & Chylek 1975: a backscatter fraction (1 - g) / 2). Gate R. */
		if ( cvTopOn > 0.5 ) {
			float tauB = 0.0;
			for ( int k = 0; k < cvTaps; k++ ) {
				float a = float( k ) / float( cvTaps ), b = float( k + 1 ) / float( cvTaps );
				float tk = cvLightKm * 0.5 * ( a * a + b * b ), dk = cvLightKm * ( b * b - a * a );
				vec3 pb = p - up * tk;
				pb.z = p.z - tk;
				tauB += cvDens( pb ) * kCvSigma * dk;
			}
			float fu = cvRdir( tauB, mu0, cvMsG, kCvAlbedo ) * ( mu0 * exp( -tauL ) + fd );
			fd += 0.5 * ( 1.0 - cvMsG * d.z ) * fu;
		}
		ph = max( ph, fd / kPi );
	}
	vec3 tint = p.z < cvBands.topKm.x ? cvTint[0] : cvTint[1];
	vec3 S = kCvAlbedo * tint * ( key * ph + cvAmbOn * cvAmb * mix( 0.5, 1.0, hf ) );
	float tr = exp( -dens * kCvSigma * dt );
	float w = T * ( 1.0 - tr );
	L += S * w;	// Frostbite: S (1 - e^(-sigma dt)) / sigma x sigma_s, sigma_s = albedo sigma
	dsum += w * t;
	wsum += w;
	T *= tr;
	if ( T < 0.005 )
		break;
