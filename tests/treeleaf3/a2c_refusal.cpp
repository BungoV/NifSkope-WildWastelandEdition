// lane TREELEAF3 (FIX1030C): TREELEAF2's leftover "alpha-to-coverage needs a matching multisampled (or fractional)
// caster path" -- REFUSED with numbers (skill gate-refused-with-numbers). This test is the refusal: a later lane that
// wants a multisampled caster path deletes it on purpose.
//   g++ -std=c++17 -O2 -I<tree>/src a2c_refusal.cpp     green: prints the table, exits 0
//   -DTREELEAF3_A2C_RED: the same claim for a HARD shadow (the receiver's filter off: one compare per pixel), must FAIL
//
// The arithmetic. Sharpened alpha-to-coverage (coverage = clamp( ( a - ref ) / fwidth( a ) + 0.5, 0, 1 )) ramps a leaf's
// edge over ONE screen pixel whose 50 % line is exactly the hard test's contour a = ref (checked below). A caster that
// carried that ramp instead of the hard cut would convolve the shadow's edge with a 1-pixel box, centred on the same
// line. The receiver already convolves it with its PCF disc (ww_sunshadow.glsl ussCsmFilter.y = radius texels; CSM1's
// own 16-tap Poisson is radius 3). Variances add: sigma^2 = (R / 2)^2 + b^2 / 12 texels^2, b = one screen pixel in
// shadow texels at the cascade's far edge (the worst point: the pixel grows with distance, the texel is fixed). The
// texel comes from the shipping fit (gl/sunshadowfit.h sliceSphere, texel = 2 r / (map - 1)).
// PRE-REGISTERED bar: the penumbra's sigma moves by <= 0.1 screen px (a fifth of 4x MSAA's 0.5 px sample spacing,
// which is the finest step the colour pass's own edge can take), at 720p (the smallest window: the largest pixel), for
// every Cascade Quality tier and every cascade. A green here = a matching caster path cannot be seen, so it does not pay.
#include "gl/sunshadowfit.h"

#include <cmath>
#include <cstdio>

int main()
{
	struct Tier
	{
		const char * name;
		double D, s0, s1, blend;
		int map;
		double radius;	// receiver filter radius, texels
	};
	// shadowset.cpp wwShadowCascadePreset (tiers 1-4) and sunshadow.cpp's CSM1 numbers (Off: kDefaultDistance 3000,
	// kSplit0/1 800/3000, kBlend 100, kDefaultMap 2048, the 16-tap Poisson radius 3)
	const Tier tiers[] = {
		{ "off", 3000, 800, 3000, 100, 2048, 3.0 },
		{ "low", 4000, 900, 2600, 150, 2048, 2.0 },
		{ "medium", 6000, 1100, 3400, 200, 2048, 2.5 },
		{ "high", 10000, 1500, 4800, 300, 4096, 3.0 },
		{ "ultra", 16000, 2000, 6500, 400, 4096, 3.5 },
	};
	const double kPi = 3.14159265358979323846;
	const double tanY = std::tan( 55.0 * 0.5 * kPi / 180.0 );	// the view's vertical fov 55
	const double tanX = tanY * 16.0 / 9.0;
	const double bar = 0.1;
	int fails = 0;
	// the ramp's 50 % line is the hard test's contour (any fwidth)
	for ( double fw : { 0.01, 0.05, 0.2 } ) {
		const double ref = 185.0 / 255.0;
		const double cov = std::fmin( std::fmax( ( ref - ref ) / fw + 0.5, 0.0 ), 1.0 );
		if ( cov != 0.5 ) {
			std::printf( "FAIL sharpened coverage at a = ref is %.3f, not 0.5\n", cov );
			fails++;
		}
	}
	double worst = 0.0;
	for ( int H : { 720, 1080, 2160 } ) {
		for ( const Tier & t : tiers ) {
			const double far[3] = { t.s0, t.s1, t.D };
			for ( int i = 0; i < 3; i++ ) {
				WwCsmFit::In in;
				in.tanX = tanX;
				in.tanY = tanY;
				in.first = i == 0;
				in.zn = i == 0 ? 0.0 : far[i - 1] - t.blend;
				in.zf = far[i] + t.blend;
				in.map = t.map;
				double zc = 0, r = 0;
				WwCsmFit::sliceSphere( in, zc, r );
				const double texel = 2.0 * r / double( t.map - 1 );
				const double px = 2.0 * in.zf * tanY / double( H );	// one screen pixel at the cascade's far edge
				const double b = px / texel;	// texels
#if defined( TREELEAF3_A2C_RED )
				const double sK = std::sqrt( 1.0 / 12.0 );	// a hard compare: the texel's own box, no disc
#else
				const double sK = t.radius / 2.0;
#endif
				const double dSigmaPx = ( std::sqrt( sK * sK + b * b / 12.0 ) - sK ) / b;
				worst = std::fmax( worst, dSigmaPx );
				const bool ok = dSigmaPx <= bar;
				fails += !ok;
				if ( H == 720 || !ok )
					std::printf( "%s %dp %-6s cascade %d: texel %.3f u, pixel %.3f u = %.2f texels; penumbra sigma +%.4f px\n",
						ok ? "ok  " : "FAIL", H, t.name, i, texel, px, b, dSigmaPx );
			}
		}
	}
	// what the matching path would cost (MEASURED from the code: three D16 layers per tier, 4x MSAA by default)
	for ( const Tier & t : tiers ) {
		const double mib = 3.0 * double( t.map ) * double( t.map ) * 2.0 / ( 1024.0 * 1024.0 );
		std::printf( "cost %-6s cascades D16 %.0f MiB single-sample, %.0f MiB at the colour pass's 4 samples (+%.0f MiB)\n",
			t.name, mib, 4.0 * mib, 3.0 * mib );
	}
	std::printf( "A2C REFUSAL %s: worst penumbra sigma change %.4f px (bar %.2f px)\n", fails ? "FAIL" : "PASS", worst, bar );
	return fails ? 1 : 0;
}
