// lane CLOUD6c: the plain march with edge refinement (RDR2, Bauer 2019 slide 50 and its note: "each time we hit the base
// shape of a cloud, we half the step length and move the sample position back by that distance", i_max = 3, "we return to
// the original step length after leaving the cloud shape"). cloud_march.frag includes it in place of its uniform loop
// while cvRefine > 0. Reads o, d, t0, t1, dt (the coarse step), j, pixKm, cosT, lo, hi; adds to L, T, dsum, wsum.
// A coarse sample that finds density after an empty one goes back to that empty sample and halves the gap cvRefine times
// (density reads only), then steps 1 / 2^cvRefine of the coarse step from the last empty point through the edge
// (cvRefineSpan fine steps at least, then on while the samples stay dense when cvRefineStay is 1); an empty fine sample
// returns to the coarse step. The fine samples read only their own density: the light's optical depths (cvTaus) are read
// every cvRefineLight fine steps along the ray and interpolated between, so a fine sample costs about one density read.
// The loop never takes more than cvRefineCap steps and keeps room to reach t1 at the coarse step, so the cost is bounded
// and the ray always ends where the uniform march ends.
	{
		float dtC = dt, sf = dt / float( 1 << cvRefine );
		float gLen = float( max( cvRefineLight, 1 ) ) * sf;
		float tc = t0, tn = t0, st = dt, tEmpty = t0, gT0 = t0, gT1 = t0;
		bool wasEmpty = true;
		int nFine = 0, cap = max( cvRefineCap, cvSteps );
		vec3 gTau0 = vec3( 0.0 ), gTau1 = vec3( 0.0 );
		for ( int i = 0; i < cap && tc < t1; i++, tc = tn ) {
			if ( st < dtC && float( cap - i - 1 ) * dtC < t1 - tc - sf )
				st = dtC;	// no room left for a fine step: the coarse step reaches t1 in the steps left
			float dt = min( st, t1 - tc );
			tn = tc + dt;
			float t = tc + j * dt;
#define CV_REFINE_STEP 1
			#include "cloud_march_step.glsl"
#undef CV_REFINE_STEP
		}
	}
