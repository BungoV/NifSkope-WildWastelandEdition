// Lane RTEMIS1 part 2: the ReSTIR GI rules (src/features/gi/restirgimath.h) against a brute-force reference, no Qt, no GPU.
// Fixture: a closed 6 x 6 x 3 room; the receivers are a 12 x 12 grid of floor points; the wall at x = 6 has a bright
// emissive band (z 1..2); a free-standing panel at x = 4 (y 1.5..4.5, z 0..2.5) hides the band from part of the floor.
// Reference: E = pi x mean Lo over 128 x 128 stratified cosine directions a pixel. ReSTIR: S seeds x F frames each.
// Checks (each red must fail its own; run.sh):
//   pdf       the first candidate alone (no reuse) is unbiased                        red RESTIRGI_RED_NOPDF
//   jacobian  full ReSTIR (temporal + spatial) is unbiased over every pixel            red RESTIRGI_RED_NOJACOBIAN
//   vis       full ReSTIR is unbiased over the pixels the panel shades                 red RESTIRGI_RED_NOVIS
//   temporal  the history lowers the relative variance (temporal on / off < 0.85)      red RESTIRGI_RED_NOTEMPORAL
#include "features/gi/restirgimath.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>

using rtddgi::V3;

static int fails = 0;

static void check( bool ok, const char * name, const char * what )
{
	std::printf( "%s %s: %s\n", ok ? "ok  " : "FAIL", name, what );
	if ( !ok )
		fails++;
}

static float comp( V3 v, int a ) { return a == 0 ? v.x : a == 1 ? v.y : v.z; }

struct Rect
{
	int axis;          // the plane's normal axis
	float c;           // the plane
	float lo0, hi0, lo1, hi1;   // bounds on the other two axes, in order
	V3 n, Lo;
};

struct Room
{
	std::vector<Rect> rects;
	bool hitRect( const Rect & r, V3 o, V3 d, float & t ) const
	{
		const float dd = comp( d, r.axis );
		if ( std::fabs( dd ) < 1e-9f )
			return false;
		t = ( r.c - comp( o, r.axis ) ) / dd;
		if ( !( t > 1e-5f ) )
			return false;
		const V3 p = o + d * t;
		const int a0 = r.axis == 0 ? 1 : 0, a1 = r.axis == 2 ? 1 : 2;
		const float u = comp( p, a0 ), v = comp( p, a1 );
		return u >= r.lo0 && u <= r.hi0 && v >= r.lo1 && v <= r.hi1;
	}
	restirgi::Sample trace( V3 o, V3 d ) const
	{
		restirgi::Sample s;
		float best = 1e30f;
		for ( const Rect & r : rects ) {
			float t;
			if ( hitRect( r, o, d, t ) && t < best ) {
				best = t;
				s.hit = true;
				s.xs = o + d * t;
				s.ns = r.n;
				s.Lo = r.Lo;
			}
		}
		if ( !s.hit ) {
			s.xs = d;
			s.Lo = V3( 0, 0, 0 );
		}
		return s;
	}
	bool blocked( V3 o, V3 d, float tmax ) const
	{
		for ( const Rect & r : rects ) {
			float t;
			if ( hitRect( r, o, d, t ) && ( tmax < 0.0f || t < tmax ) )
				return true;
		}
		return false;
	}
};

static Room room()
{
	Room R;
	const V3 grey( 0.08f, 0.08f, 0.08f ), dim( 0.15f, 0.12f, 0.10f );
	R.rects.push_back( { 2, 0.0f, 0, 6, 0, 6, V3( 0, 0, 1 ), V3( 0.05f, 0.05f, 0.05f ) } );    // floor
	R.rects.push_back( { 2, 3.0f, 0, 6, 0, 6, V3( 0, 0, -1 ), V3( 0.1f, 0.1f, 0.1f ) } );     // ceiling
	R.rects.push_back( { 0, 0.0f, 0, 6, 0, 3, V3( 1, 0, 0 ), grey } );                       // x = 0
	R.rects.push_back( { 1, 0.0f, 0, 6, 0, 3, V3( 0, 1, 0 ), grey } );                       // y = 0
	R.rects.push_back( { 1, 6.0f, 0, 6, 0, 3, V3( 0, -1, 0 ), grey } );                      // y = 6
	R.rects.push_back( { 0, 6.0f, 0, 6, 0, 1, V3( -1, 0, 0 ), dim } );                       // x = 6, below the band
	R.rects.push_back( { 0, 6.0f, 0, 6, 1, 2, V3( -1, 0, 0 ), V3( 6.0f, 5.0f, 4.0f ) } );    // the emissive band
	R.rects.push_back( { 0, 6.0f, 0, 6, 2, 3, V3( -1, 0, 0 ), dim } );                       // above it
	R.rects.push_back( { 0, 4.0f, 1.5f, 4.5f, 0, 2.5f, V3( 1, 0, 0 ), V3( 0.04f, 0.04f, 0.04f ) } );   // the panel (two-sided)
	return R;
}

constexpr int G = 12;

static std::vector<restirgi::Pixel> pixels()
{
	std::vector<restirgi::Pixel> px( G * G );
	for ( int j = 0; j < G; j++ )
		for ( int i = 0; i < G; i++ ) {
			restirgi::Pixel & p = px[size_t( j * G + i )];
			p.x = V3( 0.25f + 0.5f * float( i ), 0.25f + 0.5f * float( j ), 0.0f );
			p.n = V3( 0, 0, 1 );
			p.valid = true;
		}
	return px;
}

static bool shaded( const restirgi::Pixel & p ) { return p.x.x < 4.0f && p.x.y > 1.5f && p.x.y < 4.5f; }

struct Stats
{
	std::vector<double> sum, sum2;
	int S = 0;
};

static Stats run( const Room & R, const std::vector<restirgi::Pixel> & px, const restirgi::Settings & st, int frames, int seeds )
{
	Stats s;
	s.sum.assign( px.size(), 0.0 );
	s.sum2.assign( px.size(), 0.0 );
	s.S = seeds;
	for ( int k = 0; k < seeds; k++ ) {
		std::vector<restirgi::Res> hist;
		std::vector<V3> out;
		const uint32_t seed = restir::hash( uint32_t( k ) * 7919u + 17u );
		for ( int f = 0; f < frames; f++ )
			restirgi::frame( R, px, G, G, seed, uint32_t( f ), st, 0, hist, out );
		for ( size_t i = 0; i < px.size(); i++ ) {
			const double e = restirgi::lum( out[i] );
			s.sum[i] += e;
			s.sum2[i] += e * e;
		}
	}
	return s;
}

//! global relative bias and chi = rms of per-pixel z over the pixels `use` keeps
template<class Use>
static void bias( const Stats & s, const std::vector<double> & ref, const std::vector<restirgi::Pixel> & px, Use use, double & rel, double & chi )
{
	double m = 0, r = 0, z2 = 0;
	int n = 0;
	for ( size_t i = 0; i < ref.size(); i++ ) {
		if ( !use( px[i] ) )
			continue;
		const double mean = s.sum[i] / s.S, var = std::max( 1e-30, s.sum2[i] / s.S - mean * mean );
		const double z = ( mean - ref[i] ) / std::sqrt( var / s.S );
		if ( std::getenv( "RESTIRGI_DEBUG" ) && std::fabs( z ) > 3.0 )
			std::printf( "  pixel %zu (%.2f %.2f): mean %.4f ref %.4f sd %.4f z %+.2f\n", i, px[i].x.x, px[i].x.y, mean, ref[i], std::sqrt( var ), z );
		m += mean;
		r += ref[i];
		z2 += z * z;
		n++;
	}
	rel = m / r - 1.0;
	chi = std::sqrt( z2 / std::max( n, 1 ) );
}

static double relVar( const Stats & s )
{
	double a = 0;
	for ( size_t i = 0; i < s.sum.size(); i++ ) {
		const double mean = s.sum[i] / s.S, var = s.sum2[i] / s.S - mean * mean;
		a += mean > 0 ? var / ( mean * mean ) : 0.0;
	}
	return a / double( s.sum.size() );
}

int main()
{
	std::setvbuf( stdout, nullptr, _IONBF, 0 );
	const Room R = room();
	const std::vector<restirgi::Pixel> px = pixels();
	// the reference: pi x mean Lo over stratified cosine directions
	std::vector<double> ref( px.size(), 0.0 );
	const int N = 128;
	for ( size_t i = 0; i < px.size(); i++ ) {
		double a = 0;
		for ( int u = 0; u < N; u++ )
			for ( int v = 0; v < N; v++ ) {
				const uint32_t h = restir::hash( uint32_t( i * 65536 + u * N + v ) );
				const float j1 = float( h & 0xffff ) / 65536.0f, j2 = float( h >> 16 ) / 65536.0f;
				const V3 d = restirgi::cosineDir( px[i].n, ( float( u ) + j1 ) / N, ( float( v ) + j2 ) / N );
				a += restirgi::lum( R.trace( px[i].x + px[i].n * restirgi::kEps, d ).Lo );
			}
		ref[i] = rtddgi::kPi * a / double( N * N );
	}
	double shadedRef = 0, litRef = 0;
	int ns = 0;
	for ( size_t i = 0; i < px.size(); i++ ) {
		( shaded( px[i] ) ? shadedRef : litRef ) += ref[i];
		ns += shaded( px[i] ) ? 1 : 0;
	}
	std::printf( "reference: %zu pixels, shaded %d (mean E %.4f), lit %zu (mean E %.4f)\n", px.size(), ns, shadedRef / ns,
		px.size() - size_t( ns ), litRef / double( px.size() - size_t( ns ) ) );
	const int S = 2048, F = 4;   // 4096 gave the same verdicts (chi 1.09 / 1.28): unbiased, the far pixels' rare band hits
	auto all = []( const restirgi::Pixel & ) { return true; };
	char line[256];
	double rel, chi;
	// pdf: the first candidate alone
	{
		restirgi::Settings st;
		st.spatialK = 0;
		st.temporal = false;
		const Stats s = run( R, px, st, 1, 32 * S );	// a far pixel hits the band ~0.4 % of the time: enough draws to see it
		bias( s, ref, px, all, rel, chi );
		std::snprintf( line, sizeof line, "one candidate: bias %+.4f, chi %.3f (bars 0.02, 1.5)", rel, chi );
		check( std::fabs( rel ) < 0.02 && chi < 1.5, "pdf", line );
	}
	restirgi::Settings full;
	const Stats sf = run( R, px, full, F, S );
	bias( sf, ref, px, all, rel, chi );
	std::snprintf( line, sizeof line, "temporal + spatial (K %d, radius %d, %d frames): bias %+.4f, chi %.3f (bars 0.02, 1.5)",
		full.spatialK, full.radius, F, rel, chi );
	check( std::fabs( rel ) < 0.02 && chi < 1.5, "jacobian", line );
	bias( sf, ref, px, shaded, rel, chi );
	std::snprintf( line, sizeof line, "the panel's shade: bias %+.4f, chi %.3f (bars 0.03, 1.5)", rel, chi );
	check( std::fabs( rel ) < 0.03 && chi < 1.5, "vis", line );
	{
		restirgi::Settings st;
		st.temporal = false;
		const Stats s0 = run( R, px, st, F, S );
		const double a = relVar( sf ), b = relVar( s0 );
		std::snprintf( line, sizeof line, "relative variance temporal on %.4f / off %.4f = %.3f (bar 0.85)", a, b, a / b );
		check( a / b < 0.85, "temporal", line );
	}
	std::printf( "RESTIRGI_MATH %s %d failed\n", fails ? "FAIL" : "PASS", fails );
	return fails ? 1 : 0;
}
