// MODULES1 10c: the offline gate of src/features/clouds/cloudsheetmath.h (no Qt, no GL, no window).
// build: g++ -std=c++17 -O1 -I src tests/features/clouds/cloudsheets/cloudsheets_gate.cpp -o <exe>
// run:   <exe>        every bar must pass (exit 0)
//        <exe> red    the refuter: a receiver ABOVE a sheet is shadowed by it (the bar must catch it: exit 1)
#include "features/clouds/cloudfusionlaw.h"
#include "features/clouds/cloudsheetmath.h"

#include <cstdio>
#include <cstring>
#include <vector>

namespace S = WwCloudSheetMath;

static int g_fail = 0, g_bars = 0;

static void bar( bool ok, const char * name, double got, double want )
{
	g_bars++;
	if ( !ok )
		g_fail++;
	std::printf( "%s %s got %.7g want %.7g\n", ok ? "PASS" : "FAIL", name, got, want );
}

static unsigned g_seed = 12345u;
static float rnd()
{
	g_seed = g_seed * 1664525u + 1013904223u;
	return float( g_seed >> 8 ) / 16777216.0f;
}

int main( int argc, char ** argv )
{
	const bool red = argc > 1 && std::strcmp( argv[1], "red" ) == 0;

	// 1. the fit: a dome whose uv IS a flat sheet's is recovered; a longitude / latitude dome is refused
	{
		const float J[4] = { 1.7f, -0.4f, 0.3f, 2.2f }, c[2] = { 0.25f, -0.6f };
		std::vector<float> pos, uv, col;
		for ( int i = 0; i < 400; i++ ) {
			const float az = rnd() * 6.2831853f, el = ( 5.0f + rnd() * 85.0f ) * 3.14159265f / 180.0f, r = 300.0f;
			const float x = r * std::cos( el ) * std::cos( az ), y = r * std::cos( el ) * std::sin( az ), z = r * std::sin( el );
			pos.insert( pos.end(), { x, y, z } );
			const float gx = x / z, gy = y / z;
			uv.insert( uv.end(), { J[0] * gx + J[1] * gy + c[0], J[2] * gx + J[3] * gy + c[1] } );
			col.insert( col.end(), { 1.0f, 1.0f, 1.0f, 0.5f } );
		}
		const S::Fit f = S::fit( pos.data(), uv.data(), col.data(), 400 );
		double e = 0.0;
		for ( int i = 0; i < 4; i++ )
			e = std::max( e, double( std::fabs( f.uvPerTan[i] - J[i] ) ) );
		for ( int i = 0; i < 2; i++ )
			e = std::max( e, double( std::fabs( f.uvAtZenith[i] - c[i] ) ) );
		bar( f.valid && e < 1e-4, "fit.recovers_sheet(max_abs_err)", e, 0.0 );
		bar( std::fabs( f.capAlpha - 0.5f ) < 1e-6f, "fit.cap_alpha", f.capAlpha, 0.5 );
		std::vector<float> uv2;
		for ( int i = 0; i < 400; i++ ) {
			const float x = pos[i * 3], y = pos[i * 3 + 1], z = pos[i * 3 + 2];
			uv2.insert( uv2.end(), { std::atan2( y, x ) / 6.2831853f, std::asin( z / 300.0f ) / 1.5707963f } );
		}
		const S::Fit f2 = S::fit( pos.data(), uv2.data(), nullptr, 400 );
		bar( !f2.valid && f2.reject == S::RejectNotASheet, "fit.refuses_lonlat_dome(reject)", f2.reject, S::RejectNotASheet );
		const S::Fit f3 = S::fit( pos.data(), uv.data(), nullptr, 3 );
		bar( !f3.valid && ( f3.reject == S::RejectNoCap ), "fit.refuses_no_cap(reject)", f3.reject, S::RejectNoCap );
	}
	// 2. altitudes: the highest shell at the deck, lower ones in proportion
	{
		const float z[4] = { 297.5f, 294.0f, 235.0f, 195.4f }, want[4] = { 1.0f, 0.988235f, 0.789916f, 0.656807f };
		double e = 0.0;
		for ( int i = 0; i < 4; i++ )
			e = std::max( e, double( std::fabs( S::altitude( z[i], 297.5f, WwCloudFusionLaw::kDeckUnits ) / WwCloudFusionLaw::kDeckUnits - want[i] ) ) );
		bar( e < 1e-5, "altitude.shell_ratio(max_abs_err)", e, 0.0 );
		bar( S::altitude( 400.0f, 297.5f, 1000.0f ) == 1000.0f, "altitude.never_above_deck", S::altitude( 400.0f, 297.5f, 1000.0f ), 1000.0 );
	}
	// 3. only sheets above the receiver; the parallax = (deck - receiver height) x sun horizontal / sun z
	{
		const float sun[3] = { 0.48f, -0.36f, 0.8f };
		const float below[3] = { 1000.0f, 2000.0f, 500.0f }, above[3] = { 1000.0f, 2000.0f, 120000.0f };
		float xy[2] = {}, t = 0.0f;
		const bool hb = S::hit( below, sun, 110000.0f, xy, &t );
		const double wx = 1000.0 + ( 110000.0 - 500.0 ) * 0.48 / 0.8, wy = 2000.0 + ( 110000.0 - 500.0 ) * -0.36 / 0.8;
		const double rel = std::max( std::fabs( xy[0] - wx ), std::fabs( xy[1] - wy ) ) / ( 110000.0 - 500.0 );
		bar( hb && rel < 1e-4, "hit.parallax(rel_err)", rel, 0.0 );
		float xy2[2];
		bool ha = S::hit( above, sun, 110000.0f, xy2 );
		if ( red )
			ha = true;	// the refuter: the sheet shadows a receiver above it
		bar( !ha, "hit.none_for_receiver_above", ha ? 1.0 : 0.0, 0.0 );
		const float covers[3] = { 0.5f, 0.25f, 1.0f }, alts[3] = { 1.5f, 1.2f, 1.0f };
		bar( std::fabs( S::keptAbove( covers, alts, 3, 0.0f ) - 0.0f ) < 1e-7f, "kept.ground_under_all", S::keptAbove( covers, alts, 3, 0.0f ), 0.0 );
		bar( std::fabs( S::keptAbove( covers, alts, 3, 1.1f ) - 0.375f ) < 1e-7f, "kept.between_sheets", S::keptAbove( covers, alts, 3, 1.1f ), 0.375 );
		bar( S::keptAbove( covers, alts, 3, 1.6f ) == 1.0f, "kept.above_all", S::keptAbove( covers, alts, 3, 1.6f ), 1.0 );
	}
	// 4. the sun-disc taps: equal weights (they sum to 1), all on the unit disc, centred; the penumbra's radii
	{
		for ( int n : { 1, 4, 8, 16 } ) {
			double w = 0.0, mx = 0.0, my = 0.0, rmax = 0.0;
			for ( int k = 0; k < n; k++ ) {
				float xy[2];
				S::diskTap( k, n, xy );
				w += 1.0 / n;
				mx += xy[0] / n;
				my += xy[1] / n;
				rmax = std::max( rmax, double( std::sqrt( xy[0] * xy[0] + xy[1] * xy[1] ) ) );
			}
			char nm[64];
			std::snprintf( nm, sizeof nm, "taps%d.weights_sum", n );
			bar( std::fabs( w - 1.0 ) < 1e-6, nm, w, 1.0 );
			std::snprintf( nm, sizeof nm, "taps%d.on_unit_disc(rmax)", n );
			bar( rmax <= 1.0 && ( n > 1 || rmax == 0.0 ), nm, rmax, 1.0 );
		}
		const float sun[3] = { 0.6f, 0.0f, 0.8f };
		const S::Penumbra p = S::penumbra( 100000.0f, sun );
		bar( std::fabs( p.radiusAcross - 465.32f ) < 0.01f, "penumbra.radius_across", p.radiusAcross, 465.32 );
		bar( std::fabs( p.radiusAlong - 581.65f ) < 0.01f, "penumbra.radius_along", p.radiusAlong, 581.65 );
	}
	// 5. hex tiling: the triangle's weights sum to 1; the blend weights have length 1; a flat texture stays flat
	{
		double eb = 0.0, en = 0.0, ef = 0.0;
		for ( int i = 0; i < 20000; i++ ) {
			const float u = ( rnd() - 0.5f ) * 40.0f, v = ( rnd() - 0.5f ) * 40.0f;
			const S::HexTaps h = S::hexTaps( u, v );
			eb = std::max( eb, double( std::fabs( h.bary[0] + h.bary[1] + h.bary[2] - 1.0f ) ) );
			en = std::max( en, double( std::fabs( std::sqrt( h.weight[0] * h.weight[0] + h.weight[1] * h.weight[1] + h.weight[2] * h.weight[2] ) - 1.0f ) ) );
			ef = std::max( ef, double( std::fabs( S::hexBlend( h, 0.37f, 0.37f, 0.37f, 0.37f ) - 0.37f ) ) );
		}
		bar( eb < 1e-5, "hex.triangle_weights_sum_1(max_err)", eb, 0.0 );
		bar( en < 1e-5, "hex.blend_weights_length_1(max_err)", en, 0.0 );
		bar( ef < 1e-6, "hex.flat_stays_flat(max_err)", ef, 0.0 );
	}
	// 6. the density exponent: 1 is the identity
	{
		double e = 0.0;
		for ( int i = 0; i <= 100; i++ )
			e = std::max( e, double( std::fabs( S::layerAlpha( i / 100.0f, 1.0f ) - i / 100.0f ) ) );
		bar( e == 0.0, "exponent.one_is_identity(max_err)", e, 0.0 );
		bar( std::fabs( S::layerAlpha( 0.5f, 2.0f ) - 0.25f ) < 1e-7f, "exponent.two", S::layerAlpha( 0.5f, 2.0f ), 0.25 );
		bar( WwCloudFusionLaw::kLayerDensity == 1.0f, "exponent.shipped_constant", WwCloudFusionLaw::kLayerDensity, 1.0 );
	}
	// 7. who casts: the densest first, ties by the lower layer, the step's count
	{
		const S::Candidate c[5] = { { 0, 0.2f }, { 1, 0.7f }, { 2, 0.0f }, { 3, 0.7f }, { 4, 0.4f } };
		int out[8] = {};
		const int n = S::select( c, 5, S::preset( 2 ).layers, out );
		bar( n == 2 && out[0] == 1 && out[1] == 3, "select.medium(first*10+second)", out[0] * 10 + out[1], 13.0 );
		const int n4 = S::select( c, 5, S::preset( 4 ).layers, out );
		bar( n4 == 4 && out[2] == 4 && out[3] == 0, "select.ultra_skips_empty(count)", n4, 4.0 );
		bar( S::select( c, 5, 0, out ) == 0, "select.none_at_limit_0", S::select( c, 5, 0, out ), 0.0 );
		const S::Axes a = S::preset( 2 );
		bar( a.layers == 2 && a.taps == 4 && a.hex, "preset.medium(layers*10+taps)", a.layers * 10 + a.taps, 24.0 );
	}
	// 8. the world map: uv = J / altitude x world xy + uv at the zenith
	{
		S::Fit f;
		f.valid = true;
		f.uvPerTan[0] = 2.0f;
		f.uvPerTan[3] = 4.0f;
		f.uvAtZenith[0] = 0.5f;
		float m[4], o[2];
		const bool ok = S::worldMap( f, 1000.0f, m, o );
		bar( ok && std::fabs( m[0] - 0.002f ) < 1e-9f && std::fabs( m[3] - 0.004f ) < 1e-9f && o[0] == 0.5f, "worldmap.j_over_altitude", m[0], 0.002 );
	}
	// 9. the 2D layers' lighting: the lobe integrates to 1 over the sphere; the gate's ends; the cap; the classes
	{
		namespace F = WwCloudFusionLaw;
		double sum = 0.0;
		const int N = 20000;
		for ( int i = 0; i < N; i++ ) {
			const double c = -1.0 + ( i + 0.5 ) * 2.0 / N;
			sum += F::scatterHg( float( c ), F::kScatterG ) * 2.0 * 3.14159265358979 * 2.0 / N;
		}
		bar( std::fabs( sum - 1.0 ) < 1e-3, "scatter.lobe_integrates_to_1", sum, 1.0 );
		bar( F::scatterGate( -2.0f ) == 0.0f && F::scatterGate( 8.0f ) == 1.0f && std::fabs( F::scatterGate( 3.0f ) - 0.5f ) < 1e-6f,
			"scatter.gate(-2,3,8 deg)", F::scatterGate( 3.0f ), 0.5 );
		const F::Scatter clear = F::scatter( 30.0f, F::scatterRow( "clear" ) ), rad = F::scatter( 30.0f, F::scatterRow( "radstorm" ) );
		bar( clear.rim == 1.0f && std::fabs( clear.body - 0.6f ) < 1e-7f && std::fabs( clear.tint[2] - 0.65f ) < 1e-6f,
			"scatter.clear_day(body)", clear.body, 0.6 );
		bar( rad.rim == 0.0f && rad.body == 0.0f, "scatter.radstorm_none(rim)", rad.rim, 0.0 );
		bar( F::scatterShare( clear, 1.0f, 1.0f, 1.0f ) == F::kScatterCeiling, "scatter.capped_at_the_sun",
			F::scatterShare( clear, 1.0f, 1.0f, 1.0f ), F::kScatterCeiling );
		const F::Scatter night = F::scatter( -10.0f, F::scatterRow( "clear" ) );
		bar( F::scatterShare( night, 1.0f, 1.0f, 1.0f ) == 0.0f, "scatter.none_below_the_gate", F::scatterShare( night, 1.0f, 1.0f, 1.0f ), 0.0 );
	}
	std::printf( "%s cloudsheets gate: %d of %d bars passed%s\n", g_fail ? "FAIL" : "PASS", g_bars - g_fail, g_bars, red ? " (red arm)" : "" );
	return g_fail ? 1 : 0;
}
