// lane CLOUD4: one march sample at t over dt (cloud_march.frag includes it inside its loops: the plain one and the
// fly-through grid's fine one). Reads o, d, t, dt, pixKm, cosT, lo, hi; adds to L, T, dsum, wsum; `continue` = an
// empty sample, `break` = the ray is opaque.
// lane CLOUD6c: cloud_march_refine.glsl includes it with CV_REFINE_STEP (edge refinement); without it the text below is
// the uniform march's, unchanged.
	vec3 p = o + d * t;
	vec3 up;
	p.z = cvHeight( o, d, t, up );
	cvFootKm = t * pixKm;
#ifdef CV_COUNT
	cvNSamp++;
#endif
	float dens = cvDens( p );
#ifdef CV_REFINE_STEP
	/* lane CLOUD6c (cloud_march_refine.glsl): a coarse sample that finds cloud after an empty one steps back to the empty
	 * one and halves the gap cvRefine times (RDR2's i_max), then the march goes on from the last empty point at the fine
	 * step; an empty fine sample past the span returns to the coarse step. The light's optical depths (cvTaus: the taps
	 * toward the key light, down and up) are read every cvRefineLight fine steps and interpolated between. */
	bool fineS = st < dtC;
	if ( !fineS ) {
		if ( dens > 0.0 && wasEmpty && float( cap - i - 1 ) * dtC >= t1 - tEmpty ) {
			float ra = tEmpty, rb = t;
			for ( int k = 0; k < cvRefine; k++ ) {
				float rm = 0.5 * ( ra + rb );
				vec3 rup;
				if ( cvDens( vec3( o.xy + d.xy * rm, cvHeight( o, d, rm, rup ) ) ) > 0.0 )
					rb = rm;
				else
					ra = rm;
			}
#ifdef CV_COUNT
			cvNRef++;
#endif
			tn = ra;
			st = sf;
			nFine = cvRefineSpan;
			wasEmpty = true;
			gT0 = ra;
			gT1 = ra + gLen;
			gTau0 = cvTaus( o, d, gT0, pixKm );
			gTau1 = cvTaus( o, d, gT1, pixKm );
			continue;
		}
	} else {
		nFine--;
		if ( nFine <= 0 && ( cvRefineStay == 0 || dens <= 0.0 ) )
			st = dtC;
	}
	wasEmpty = dens <= 0.0;
	if ( wasEmpty )
		tEmpty = t;
#endif
	if ( dens <= 0.0 )
		continue;
#ifdef CV_REFINE_STEP
	vec3 tauI;
	if ( fineS ) {
		if ( t > gT1 ) {	// the next stretch of cvRefineLight fine steps (a fine step is shorter than one)
			gT0 = gT1;
			gTau0 = gTau1;
			gT1 = gT0 + gLen;
			gTau1 = cvTaus( o, d, gT1, pixKm );
		}
		tauI = mix( gTau0, gTau1, clamp( ( t - gT0 ) / gLen, 0.0, 1.0 ) );
	} else {
		tauI = cvTaus( o, d, t, pixKm );
	}
	cvFootKm = t * pixKm;
	float tauL = tauI.x;
#else
#ifdef CV_COUNT
	cvNLit++;
#endif
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
#ifdef CV_REFINE_STEP
			float tauB = tauI.y;
#else
			float tauB = 0.0;
			for ( int k = 0; k < cvTaps; k++ ) {
				float a = float( k ) / float( cvTaps ), b = float( k + 1 ) / float( cvTaps );
				float tk = cvLightKm * 0.5 * ( a * a + b * b ), dk = cvLightKm * ( b * b - a * a );
				vec3 pb = p - up * tk;
				pb.z = p.z - tk;
				tauB += cvDens( pb ) * kCvSigma * dk;
			}
#endif
			float fu = cvRdir( tauB, mu0, cvMsG, kCvAlbedo ) * ( mu0 * exp( -tauL ) + fd );
			fd += 0.5 * ( 1.0 - cvMsG * d.z ) * fu;
		}
		ph = max( ph, fd / kPi );
	}
	vec3 tint = p.z < cvBands.topKm.x ? cvTint[0] : cvTint[1];
	float ambF = mix( 0.5, 1.0, hf );
	/* lane CLOUDBAND1 (b), RDR2's darker sky light inside thick cloud (Bauer 2019 darkens it by a sun-ward sample): the open
	 * sky's light reaching the sample is its transmission through the column above: the delta-Eddington scaled direct
	 * part exp(-(1 - g^2 w) tau / 0.6) + the two-stream Tdif, both at mu 0.6 (the diffusivity cosine), tau from three
	 * taps straight up to the top of the sample's band.
	 * Off keeps the 0.5 .. 1 height ramp (a dense deck's base kept half the sky: 7.9x the Monte Carlo at tau 40). */
	if ( cvAmbDark > 0.5 ) {
		float topZ = p.z < cvBands.topKm.x ? cvBands.topKm.x : cvBands.topKm.y;
#ifdef CV_REFINE_STEP
		float tauU = tauI.z;
#else
		float span = max( topZ - p.z, 0.0 ), tauU = 0.0;
		for ( int k = 0; k < 3; k++ ) {
			float tk = span * ( float( k ) + 0.5 ) / 3.0;
			vec3 pu = p + up * tk;
			pu.z = p.z + tk;
			tauU += cvDens( pu ) * kCvSigma * span / 3.0;
		}
#endif
		// the direct part of the delta-Eddington scaled slab (the forward peak counts as passed through, as in cvTdif)
		ambF = exp( -( 1.0 - cvMsG * cvMsG * kCvAlbedo ) * tauU / 0.6 ) + cvTdif( tauU, 0.6, cvMsG, kCvAlbedo );
	}
	vec3 S = kCvAlbedo * tint * ( key * ph + cvAmbOn * cvAmb * ambF );
	float tr = exp( -dens * kCvSigma * dt );
	float w = T * ( 1.0 - tr );
	L += S * w;	// Frostbite: S (1 - e^(-sigma dt)) / sigma x sigma_s, sigma_s = albedo sigma
	dsum += w * t;
	wsum += w;
	T *= tr;
	if ( T < 0.005 )
		break;
