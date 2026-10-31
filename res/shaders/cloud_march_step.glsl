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
#ifdef WW_CLOUD_SHAPE
	/* lane CLOUDSHAPE1 (the anvils' contour stripes, MEASURED in v2/zx: not the step length, not the frame count): the taps'
	 * jitter was the view sample's own j, so how deep the sample sat in the cloud and where its light taps fell moved
	 * together, a bias that followed the surface's phase on the step grid. The taps take their own white hash of (pixel, t,
	 * frame): a second IGN (R2 pair with j) left a crosshatch (notes/cloudshape1/v2/zr2_m.png); the history averages grain. */
	float jT = fract( sin( dot( vec3( gl_FragCoord.xy, t * 61.7 + cvFrame * 1.618 ), vec3( 12.9898, 78.233, 37.719 ) ) ) * 43758.5453 );
#else
	float jT = j;
#endif
	float tauL = 0.0;
	for ( int k = 0; k < cvTaps; k++ ) {
		float a = float( k ) / float( cvTaps ), b = float( k + 1 ) / float( cvTaps );
		float tk = cvLightKm * 0.5 * ( a * a + b * b ), dk = cvLightKm * ( b * b - a * a );
#ifdef WW_CLOUD_SHAPE
		/* lane CLOUDSHAPE1: a dense, crisp cloud bands the light at fixed tap places: each tap sits jittered inside its
		 * segment (the accumulation averages the steps away) and reads the density at its segment's footprint (HZD's
		 * cone of light samples smooths the same way, Schneider 2015 p. 85) */
		{
			float u = mix( a, b, fract( jT + 0.618034 * float( k + 1 ) + 0.5 * cvFrame ) );
			tk = cvLightKm * u * u;
		}
		cvFootKm = max( t * pixKm, kShpTapFoot * dk );
#endif
		vec3 pl = p + cvKeyW * tk;
		pl.z = p.z + dot( up, cvKeyW ) * tk;
		tauL += cvDens( pl ) * kCvSigma * dk;
	}
#if defined( WW_CLOUD_VARIETY ) || defined( WW_CLOUD_SHAPE )	// lane CLOUDSHAPE1: the shapes' towers too
	/* lane CLOUD5 round 2: a tower's column goes on past the taps' reach (6 km of a 7 km tower with the sun at 60 deg is
	 * 6.9 km along it): one tap at the middle of the rest, up to the Low band's top along the key, at most one reach
	 * long. Without it the floor's slab is too thin and, at albedo 0.99, the base 2.1x too bright (gate C, MEASURED). */
	{
		float muK = dot( up, cvKeyW );
		float rest = muK > 0.01 ? clamp( ( cvBands.topKm.x - p.z ) / muK - cvLightKm, 0.0, cvLightKm ) : 0.0;
#ifdef WW_CLOUD_SHAPE
		/* lane CLOUDSHAPE1: towers only (the storm type, cvShpT0.w > 0): on cumulus the extra tap took the mid view's
		 * contrast to 0.28, past RDR2's 0.22 (notes/cloudshape1/gate3.txt; RDR2 lightens the same bases with its
		 * back-scatter clamp, Bauer 2019 sl. 30) */
		rest = cvShpT0.w > 0.0 ? rest : 0.0;
#endif
		if ( rest > 0.0 ) {
			float tk = cvLightKm + 0.5 * rest;
#ifdef WW_CLOUD_SHAPE
			// lane CLOUDSHAPE1 (sheet review item 5, the anvils' contour stripes): jittered inside the rest like the taps; at
			// its fixed middle it fell in or out of the thin anvil plate with the sample's height, banding the underside
			tk = cvLightKm + rest * fract( jT + 0.754878 + 0.5 * cvFrame );
#endif
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
			/* lane CLOUDLIGHT1: cvTopOn 2 (the default, WW_CLOUDVOL_TOPCOLUMN) takes the slab under the sample as uniform
			 * at the sample's own density down to its band's base (at most the taps' reach): no read, where the taps
			 * cost cvTaps (3 Medium / 6 Ultra) of the lit sample's 10 / 16. Gate tests/spells/cloudlight_check.py
			 * (MEASURED: converged picture vs the taps <= 0.21 levels rms, gate R 0.957 / 1.253 vs MC). 1 = the taps. */
			if ( cvTopOn > 1.5 ) {
				float zb = p.z < cvBands.topKm.x ? cvBands.baseKm.x : cvBands.baseKm.y;
				tauB = dens * kCvSigma * clamp( p.z - zb, 0.0, cvLightKm );
			} else {
				for ( int k = 0; k < cvTaps; k++ ) {
					float a = float( k ) / float( cvTaps ), b = float( k + 1 ) / float( cvTaps );
					float tk = cvLightKm * 0.5 * ( a * a + b * b ), dk = cvLightKm * ( b * b - a * a );
					vec3 pb = p - up * tk;
					pb.z = p.z - tk;
					tauB += cvDens( pb ) * kCvSigma * dk;
				}
			}
			float fu = cvRdir( tauB, mu0, cvMsG, kCvAlbedo ) * ( mu0 * exp( -tauL ) + fd );
			fd += 0.5 * ( 1.0 - cvMsG * d.z ) * fu;
		}
#ifdef WW_CLOUD_SHAPE
		fd *= cvShpK( 0, kShpMs );
#endif
		ph = max( ph, fd / kPi );
	}
#ifdef WW_CLOUD_SHAPE
	/* lane CLOUDSHAPE1 (LATE ITEM 7, MEASURED: the light march alone left the sunlit side dark at back-scatter angles and
	 * the floor + ambient flattened every cloud to top/base 1.00, references 0.99-1.74): two published terms.
	 * RDR2's back-scatter clamp (Bauer 2019 sl. 30): the phase never under a Lambertian 1/pi where the cloud is dense,
	 * times the light that reaches the sample (exp(-tauL)): a sunlit face is bright from any side, a shaded one stays dark.
	 * Nubis 2017's in-scatter probability (Schneider p. 89-92): few scattering sources at the thin edges facing the light
	 * and at the cloud's bottom: depth 0.05 + d^(0.5..2 over the height), vertical (0.1 .. 1 over 7-14 % of the layer)^0.8;
	 * it scales the key light only (the ambient keeps AMBDARK's column). */
	{
		float hfB = clamp( ( p.z - cvBands.baseKm.x ) / max( cvBands.topKm.x - cvBands.baseKm.x, 1e-3 ), 0.0, 1.0 );
		ph = max( ph, kShpBack / kPi * cvSmooth( kShpBackLo, kShpBackHi, dens * kCvSigma ) * exp( -tauL ) );
		float dl = clamp( dens / max( kShpScale * cvBands.density.x, 1e-3 ), 0.0, 1.0 );
		float depthP = 0.05 + pow( dl, mix( 0.5, 2.0, clamp( ( hfB - 0.3 ) / 0.55, 0.0, 1.0 ) ) );
		// the base's floor 0.35, not Nubis' 0.1: our bases get no ground bounce (completeness row 15), and at 0.1 the sky's
		// blue ambient alone painted them blue (diag/up50_sp_lit_d2.png)
		float vertP = pow( mix( 0.35, 1.0, clamp( ( hfB - 0.07 ) / 0.07, 0.0, 1.0 ) ), 0.8 );
		// the in-scatter probability darkens the edges seen with the sun BEHIND the eye; looking into the sun the thin edges
		// glow (the droplet phase's forward peak: the silver lining, Schneider 2017 p. 93 keeps it for the forward lobe)
		ph *= mix( min( depthP, 1.0 ) * vertP, 1.0, cvSmooth( cvShpK( 1, 0.3 ), 0.95, cosT ) );
	}
#endif
	vec3 tint = p.z < cvBands.topKm.x ? cvTint[0] : cvTint[1];
	float ambF = mix( 0.5, 1.0, hf );
	/* lane CLOUDBAND1 (b), RDR2's darker sky light inside thick cloud (Bauer 2019 darkens it by a sun-ward sample): the open
	 * sky's light reaching the sample is its transmission through the column above: the delta-Eddington scaled direct
	 * part exp(-(1 - g^2 w) tau / 0.6) + the two-stream Tdif, both at mu 0.6 (the diffusivity cosine), tau from three
	 * taps straight up to the top of the sample's band.
	 * Off keeps the 0.5 .. 1 height ramp (a dense deck's base kept half the sky: 7.9x the Monte Carlo at tau 40). */
	if ( cvAmbDark > 0.5 ) {
		float topZ = p.z < cvBands.topKm.x ? cvBands.topKm.x : cvBands.topKm.y;
		float span = max( topZ - p.z, 0.0 ), tauU = 0.0;
		for ( int k = 0; k < 3; k++ ) {
			float tk = span * ( float( k ) + 0.5 ) / 3.0;
#ifdef WW_CLOUD_SHAPE
			// lane CLOUDSHAPE1: jittered inside its third (fixed places banded a thin anvil's underside in contour lines)
			tk = span * ( float( k ) + fract( jT + 0.381966 * float( k + 1 ) + 0.25 * cvFrame ) ) / 3.0;
#endif
			vec3 pu = p + up * tk;
			pu.z = p.z + tk;
			tauU += cvDens( pu ) * kCvSigma * span / 3.0;
		}
		// the direct part of the delta-Eddington scaled slab (the forward peak counts as passed through, as in cvTdif)
		ambF = exp( -( 1.0 - cvMsG * cvMsG * kCvAlbedo ) * tauU / 0.6 ) + cvTdif( tauU, 0.6, cvMsG, kCvAlbedo );
	}
#ifdef WW_CLOUD_SHAPE
	/* lane CLOUDSHAPE1: the ambient's split (Hillaire 2016 p. 30; Nubis 2017: the sky from above at the tops, the ground's
	 * bounce from below at the bases): the base takes a grey, darker light (the ground's albedo x the sky + sun on it) */
	vec3 ambC = mix( vec3( dot( cvAmb, vec3( 0.2126, 0.7152, 0.0722 ) ) ) * cvShpK( 2, kShpGround ), cvAmb,
		cvSmooth( 0.0, 0.6, clamp( ( p.z - cvBands.baseKm.x ) / max( cvBands.topKm.x - cvBands.baseKm.x, 1e-3 ), 0.0, 1.0 ) ) );
	vec3 S = kCvAlbedo * tint * ( key * ph + cvAmbOn * ambC * ambF * cvShpK( 3, kShpSkyOcc ) );
#else
	vec3 S = kCvAlbedo * tint * ( key * ph + cvAmbOn * cvAmb * ambF );
#endif
	float tr = exp( -dens * kCvSigma * dt );
	float w = T * ( 1.0 - tr );
	L += S * w;	// Frostbite: S (1 - e^(-sigma dt)) / sigma x sigma_s, sigma_s = albedo sigma
	dsum += w * t;
	wsum += w;
	T *= tr;
	if ( T < 0.005 )
		break;
