// bounce_reference -- the brute-force ONE-BOUNCE reference for the probe GI's sun bounce (lane BOUNCE1, 2026-10-13).
//
// Shares NO code with src/probebake.cpp or src/probegi.cpp: no BVH (every ray is tested against every triangle
// kept), its own jittered equal-area strata, its own Moller-Trumbore, its own soup reader (a copy of
// tests/prtp_reference.cpp's, which itself shares nothing with the bake).
//
//   bounce_reference <soup.psp> <points.txt> <rays> <sx> <sy> <sz> <sr> <sg> <sb> <cull> <out.tsv> [lights.txt]
//
// points.txt: one "x y z nx ny nz" per line (a receiver: a point and the way it faces; a probe axis is a point
// with the axis as its normal). s = toward the sun, (sr sg sb) its colour (linear, the viewport's units).
// For each point, every stratum direction d with n.d > 0 is traced to its nearest hit (alpha-test holes pass,
// glass panes multiply their tint, as prtp_reference does). The hit's light is the sun's first bounce:
//     L = albedo x sun x max(0, n_hit . s) x V,  n_hit turned toward the point,
// V = 1 when a ray from the hit (0.5 unit off the surface) toward the sun meets no triangle, else 0. Then
//     E = sum over the rays of L x (n . d) x 4 pi / M
// is the point's sun-bounce irradiance in the unit the relight's probes store (pi x the radiance a white
// Lambert surface shows): a surface of albedo a shows a x E / pi. A miss is sky: its share is printed (cosine
// weighted, / pi), its light is not added (the relight keeps the sky in its own term, gi_sky.bin).
// cull: only triangles with a corner within this distance of the point are kept (the hits and the shadow rays
// beyond it are taken as open); the out header prints the count kept per point.
//
// lights.txt (optional): one "x y z radius r g b" per line, an omni lamp with the game's default falloff (DATA
// Constant 0, Scalar 1, Exponent 2): a = (1 - clamp((d / radius)^2, 0, 1))^2.2 (PRTP2 section 1), so a hit adds
// albedo x colour x a x max(0, n_hit . l) when a ray from the hit to the lamp meets no triangle.
//
// out.tsv: index, E r g b, sky share, hit share, sunlit-hit share, triangles kept.
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>
#include <vector>

namespace {

struct Soup
{
	std::vector<float> v;                 // 9 a triangle
	std::vector<uint8_t> alb;             // 3 a triangle
	std::vector<double> glass, glassT;    // panes (9 each), their tint (3 each)
	struct Map { int w = 0, h = 0; std::vector<uint8_t> a; };
	struct Masked { int map = -1; uint32_t thr = 0; float uv[6]; float as[3] = { 1, 1, 1 }; };
	std::vector<Map> maps;
	std::vector<int> maskOf;
	std::vector<Masked> masked;
	bool hole( size_t tri, double b1, double b2 ) const
	{
		if ( maskOf.empty() || maskOf[tri] < 0 )
			return false;
		const Masked & m = masked[size_t( maskOf[tri] )];
		const Map & mp = maps[size_t( m.map )];
		const double b0 = 1.0 - b1 - b2;
		double u = b0 * m.uv[0] + b1 * m.uv[2] + b2 * m.uv[4];
		double w = b0 * m.uv[1] + b1 * m.uv[3] + b2 * m.uv[5];
		u -= std::floor( u );
		w -= std::floor( w );
		const int x = std::min( std::max( int( u * mp.w ), 0 ), mp.w - 1 );
		const int y = std::min( std::max( int( w * mp.h ), 0 ), mp.h - 1 );
		const int a = mp.a[size_t( y ) * size_t( mp.w ) + size_t( x )];
		const double sc = b0 * m.as[0] + b1 * m.as[1] + b2 * m.as[2];
		return double( a ) * sc < double( m.thr );
	}
};

bool readSoup( const char * path, Soup & s )
{
	FILE * f = std::fopen( path, "rb" );
	if ( !f )
		return false;
	uint32_t head[3];
	if ( std::fread( head, 4, 3, f ) != 3 || head[0] != 0x31505350u ) {
		std::fclose( f );
		return false;
	}
	const size_t n = head[1];
	s.v.resize( n * 9 );
	if ( std::fread( s.v.data(), 4, s.v.size(), f ) != s.v.size() ) {
		std::fclose( f );
		return false;
	}
	_fseeki64( f, static_cast<long long>( head[2] ) * 28, SEEK_CUR );
	uint32_t tail[2] = { 0, 0 };
	if ( std::fread( tail, 4, 2, f ) == 2 && tail[0] == 0x31424C41u && tail[1] == n ) {
		s.alb.resize( n * 3 );
		if ( std::fread( s.alb.data(), 1, s.alb.size(), f ) != s.alb.size() )
			s.alb.clear();
	}
	bool more = std::fread( tail, 4, 2, f ) == 2;
	if ( more && tail[0] == 0x31534C47u ) {   // 'GLS1'
		std::vector<float> g( size_t( tail[1] ) * 9 );
		std::vector<uint8_t> gt( size_t( tail[1] ) * 3 );
		if ( std::fread( g.data(), 4, g.size(), f ) == g.size() && std::fread( gt.data(), 1, gt.size(), f ) == gt.size() ) {
			s.glass.assign( g.begin(), g.end() );
			for ( uint8_t c : gt )
				s.glassT.push_back( c / 255.0 );
		}
		more = std::fread( tail, 4, 2, f ) == 2;
	}
	if ( more && tail[0] == 0x314F5754u ) {   // 'TWO1'
		_fseeki64( f, static_cast<long long>( tail[1] ), SEEK_CUR );
		more = std::fread( tail, 4, 2, f ) == 2;
	}
	while ( more ) {   // sized tails: magic, count, u32 bytes, body
		uint32_t bytes = 0;
		if ( std::fread( &bytes, 4, 1, f ) != 1 )
			break;
		const long long body = _ftelli64( f );
		if ( tail[0] == 0x314B4D41u || tail[0] == 0x324B4D41u ) {   // 'AMK1' / 'AMK2'
			const bool amk2 = tail[0] == 0x324B4D41u;
			bool ok = true;
			auto u32 = [&]() { uint32_t x = 0; ok = ok && std::fread( &x, 4, 1, f ) == 1; return x; };
			const uint32_t nm = u32();
			for ( uint32_t m = 0; ok && m < nm; m++ ) {
				_fseeki64( f, static_cast<long long>( u32() ), SEEK_CUR );
				Soup::Map mp;
				mp.w = int( u32() );
				mp.h = int( u32() );
				ok = ok && mp.w > 0 && mp.h > 0 && mp.w <= 16384 && mp.h <= 16384;
				if ( !ok )
					break;
				mp.a.resize( size_t( mp.w ) * size_t( mp.h ) );
				ok = std::fread( mp.a.data(), 1, mp.a.size(), f ) == mp.a.size();
				s.maps.push_back( std::move( mp ) );
			}
			const uint32_t nmod = u32();
			for ( uint32_t m = 0; ok && m < nmod; m++ )
				_fseeki64( f, static_cast<long long>( u32() ), SEEK_CUR );
			s.maskOf.assign( n, -1 );
			for ( uint32_t i = 0; ok && i < tail[1]; i++ ) {
				const uint32_t tri = u32(), map = u32();
				u32();
				Soup::Masked mk;
				mk.thr = u32();
				mk.map = int( map );
				ok = ok && std::fread( mk.uv, 4, 6, f ) == 6 && tri < n && map < nm;
				if ( amk2 )
					ok = ok && std::fread( mk.as, 4, 3, f ) == 3;
				if ( !ok )
					break;
				s.maskOf[tri] = int( s.masked.size() );
				s.masked.push_back( mk );
			}
			if ( !ok ) {
				std::fclose( f );
				return false;
			}
		}
		more = _fseeki64( f, body + bytes, SEEK_SET ) == 0 && std::fread( tail, 4, 2, f ) == 2;
	}
	std::fclose( f );
	return !s.alb.empty();
}

// A kept triangle: v0, e1, e2, the unit face normal, its index in the soup
struct Tri { float o[3], a[3], b[3], n[3]; uint32_t i; };

// nearest hit along (p, d) up to tmax; any = the first hit is enough. Returns the tri index into T, or -1.
int trace( const Soup & s, const std::vector<Tri> & T, const double p[3], const double d[3], double tmax, bool any,
	double * tOut, double * b1Out, double * b2Out )
{
	int best = -1;
	double tb = tmax;
	for ( size_t k = 0; k < T.size(); k++ ) {
		const Tri & t = T[k];
		const double h[3] = { d[1] * t.b[2] - d[2] * t.b[1], d[2] * t.b[0] - d[0] * t.b[2], d[0] * t.b[1] - d[1] * t.b[0] };
		const double det = t.a[0] * h[0] + t.a[1] * h[1] + t.a[2] * h[2];
		if ( std::fabs( det ) < 1e-12 )
			continue;
		const double inv = 1.0 / det;
		const double sv[3] = { p[0] - t.o[0], p[1] - t.o[1], p[2] - t.o[2] };
		const double u = ( sv[0] * h[0] + sv[1] * h[1] + sv[2] * h[2] ) * inv;
		if ( u < 0.0 || u > 1.0 )
			continue;
		const double q[3] = { sv[1] * t.a[2] - sv[2] * t.a[1], sv[2] * t.a[0] - sv[0] * t.a[2], sv[0] * t.a[1] - sv[1] * t.a[0] };
		const double v = ( d[0] * q[0] + d[1] * q[1] + d[2] * q[2] ) * inv;
		if ( v < 0.0 || u + v > 1.0 )
			continue;
		const double tt = ( t.b[0] * q[0] + t.b[1] * q[1] + t.b[2] * q[2] ) * inv;
		if ( tt > 0.01 && tt < tb && !s.hole( t.i, u, v ) ) {
			tb = tt;
			best = int( k );
			if ( b1Out ) { *b1Out = u; *b2Out = v; }
			if ( any )
				break;
		}
	}
	if ( tOut )
		*tOut = tb;
	return best;
}

void throughGlass( const Soup & s, const double o[3], const double d[3], double tEnd, double T[3] )
{
	T[0] = T[1] = T[2] = 1.0;
	std::vector<std::pair<double, size_t>> hits;
	for ( size_t i = 0; i * 9 < s.glass.size(); i++ ) {
		const double * v = &s.glass[i * 9];
		const double e1[3] = { v[3] - v[0], v[4] - v[1], v[5] - v[2] }, e2[3] = { v[6] - v[0], v[7] - v[1], v[8] - v[2] };
		const double n[3] = { e1[1] * e2[2] - e1[2] * e2[1], e1[2] * e2[0] - e1[0] * e2[2], e1[0] * e2[1] - e1[1] * e2[0] };
		const double dn = n[0] * d[0] + n[1] * d[1] + n[2] * d[2];
		if ( std::fabs( dn ) < 1e-12 )
			continue;
		const double t = ( n[0] * ( v[0] - o[0] ) + n[1] * ( v[1] - o[1] ) + n[2] * ( v[2] - o[2] ) ) / dn;
		if ( !( t > 0.01 && t < tEnd - 0.01 ) )
			continue;
		const double p[3] = { o[0] + d[0] * t - v[0], o[1] + d[1] * t - v[1], o[2] + d[2] * t - v[2] };
		const double d00 = e1[0] * e1[0] + e1[1] * e1[1] + e1[2] * e1[2], d01 = e1[0] * e2[0] + e1[1] * e2[1] + e1[2] * e2[2];
		const double d11 = e2[0] * e2[0] + e2[1] * e2[1] + e2[2] * e2[2];
		const double d20 = p[0] * e1[0] + p[1] * e1[1] + p[2] * e1[2], d21 = p[0] * e2[0] + p[1] * e2[1] + p[2] * e2[2];
		const double den = d00 * d11 - d01 * d01;
		if ( std::fabs( den ) < 1e-18 )
			continue;
		const double b1 = ( d11 * d20 - d01 * d21 ) / den, b2 = ( d00 * d21 - d01 * d20 ) / den;
		if ( b1 < 0 || b2 < 0 || b1 + b2 > 1 )
			continue;
		hits.push_back( { t, i } );
	}
	std::sort( hits.begin(), hits.end() );
	double last = -1.0;
	for ( const auto & h : hits ) {
		if ( last >= 0 && h.first - last <= 0.01 )
			continue;
		for ( int c = 0; c < 3; c++ )
			T[c] *= s.glassT[h.second * 3 + size_t( c )];
		last = h.first;
	}
}

}   // namespace

int main( int argc, char ** argv )
{
	if ( argc != 12 && argc != 13 ) {
		std::fprintf( stderr, "usage: bounce_reference <soup.psp> <points.txt> <rays> <sx> <sy> <sz> <sr> <sg> <sb> <cull> <out.tsv>\n" );
		return 2;
	}
	Soup s;
	if ( !readSoup( argv[1], s ) ) {
		std::fprintf( stderr, "soup %s: unreadable or no ALB1 albedo\n", argv[1] );
		return 1;
	}
	std::vector<double> P;
	if ( FILE * f = std::fopen( argv[2], "r" ) ) {
		double x[6];
		while ( std::fscanf( f, "%lf %lf %lf %lf %lf %lf", x, x + 1, x + 2, x + 3, x + 4, x + 5 ) == 6 )
			P.insert( P.end(), x, x + 6 );
		std::fclose( f );
	}
	const int rays = std::atoi( argv[3] );
	double sd[3] = { std::atof( argv[4] ), std::atof( argv[5] ), std::atof( argv[6] ) };
	const double sl = std::sqrt( sd[0] * sd[0] + sd[1] * sd[1] + sd[2] * sd[2] );
	for ( double & c : sd )
		c /= sl;
	const double sun[3] = { std::atof( argv[7] ), std::atof( argv[8] ), std::atof( argv[9] ) };
	const double cull = std::atof( argv[10] );
	std::vector<double> lamps;   // x y z radius r g b
	if ( argc == 13 )
		if ( FILE * f = std::fopen( argv[12], "r" ) ) {
			double x[7];
			while ( std::fscanf( f, "%lf %lf %lf %lf %lf %lf %lf", x, x + 1, x + 2, x + 3, x + 4, x + 5, x + 6 ) == 7 )
				lamps.insert( lamps.end(), x, x + 7 );
			std::fclose( f );
		}
	const bool sunOn = sd[2] > 0.0 && ( sun[0] > 0.0 || sun[1] > 0.0 || sun[2] > 0.0 );
	// jittered equal-area strata over the sphere (rows in z, columns in phi), fixed seed
	std::vector<double> D;
	{
		const int rows = std::max( 1, int( std::lround( std::sqrt( rays / 2.0 ) ) ) ), cols = std::max( 1, rays / rows );
		uint64_t st = 0x2545F4914F6CDD1Dull;
		auto rnd = [&]() { st ^= st << 13; st ^= st >> 7; st ^= st << 17; return double( st >> 11 ) / double( 1ull << 53 ); };
		for ( int r = 0; r < rows; r++ )
			for ( int c = 0; c < cols; c++ ) {
				const double z = 1.0 - 2.0 * ( r + rnd() ) / rows, ph = 2.0 * 3.14159265358979323846 * ( c + rnd() ) / cols;
				const double q = std::sqrt( std::max( 0.0, 1.0 - z * z ) );
				D.insert( D.end(), { q * std::cos( ph ), q * std::sin( ph ), z } );
			}
	}
	const size_t M = D.size() / 3, NP = P.size() / 6, NT = s.v.size() / 9;
	FILE * out = std::fopen( argv[11], "w" );
	if ( !out )
		return 1;
	std::fprintf( out, "# bounce_reference rays %zu tris %zu sun to %.5f %.5f %.5f colour %.5f %.5f %.5f cull %.0f\n", M, NT,
		sd[0], sd[1], sd[2], sun[0], sun[1], sun[2], cull );
	const unsigned nth = std::max( 1u, std::thread::hardware_concurrency() );
	for ( size_t p = 0; p < NP; p++ ) {
		const double * pt = &P[p * 6];
		double n[3] = { pt[3], pt[4], pt[5] };
		const double nl = std::sqrt( n[0] * n[0] + n[1] * n[1] + n[2] * n[2] );
		for ( double & c : n )
			c /= nl;
		std::vector<Tri> T;
		for ( size_t i = 0; i < NT; i++ ) {
			const float * v = &s.v[i * 9];
			bool near = false;
			for ( int k = 0; k < 3 && !near; k++ ) {
				const double dx = v[k * 3] - pt[0], dy = v[k * 3 + 1] - pt[1], dz = v[k * 3 + 2] - pt[2];
				near = dx * dx + dy * dy + dz * dz < cull * cull;
			}
			if ( !near )
				continue;
			Tri t;
			for ( int c = 0; c < 3; c++ ) {
				t.o[c] = v[c];
				t.a[c] = v[3 + c] - v[c];
				t.b[c] = v[6 + c] - v[c];
			}
			double cx = double( t.a[1] ) * t.b[2] - double( t.a[2] ) * t.b[1], cy = double( t.a[2] ) * t.b[0] - double( t.a[0] ) * t.b[2],
				cz = double( t.a[0] ) * t.b[1] - double( t.a[1] ) * t.b[0];
			const double l = std::sqrt( cx * cx + cy * cy + cz * cz );
			if ( !( l > 0 ) )
				continue;
			t.n[0] = float( cx / l ); t.n[1] = float( cy / l ); t.n[2] = float( cz / l );
			t.i = uint32_t( i );
			T.push_back( t );
		}
		std::vector<double> acc( M * 3, 0.0 );
		std::vector<int> kind( M, 0 );   // 0 back, 1 sky, 2 hit, 3 sunlit hit
		std::atomic<size_t> next( 0 );
		auto work = [&]() {
			for ( ;; ) {
				const size_t r = next.fetch_add( 1 );
				if ( r >= M )
					return;
				const double * d = &D[r * 3];
				const double cosN = d[0] * n[0] + d[1] * n[1] + d[2] * n[2];
				if ( cosN <= 0.0 )
					continue;
				const double o[3] = { pt[0] + n[0] * 0.5, pt[1] + n[1] * 0.5, pt[2] + n[2] * 0.5 };
				double th, b1, b2;
				const int k = trace( s, T, o, d, 1.0e30, false, &th, &b1, &b2 );
				double Tg[3] = { 1, 1, 1 };
				if ( !s.glass.empty() )
					throughGlass( s, o, d, k < 0 ? 1.0e30 : th, Tg );
				if ( k < 0 ) {
					kind[r] = 1;
					continue;
				}
				kind[r] = 2;
				const Tri & t = T[size_t( k )];
				double hn[3] = { t.n[0], t.n[1], t.n[2] };
				if ( hn[0] * d[0] + hn[1] * d[1] + hn[2] * d[2] > 0 )
					for ( double & c : hn )
						c = -c;
				const double h[3] = { o[0] + d[0] * th + hn[0] * 0.5, o[1] + d[1] * th + hn[1] * 0.5, o[2] + d[2] * th + hn[2] * 0.5 };
				const uint8_t * a = &s.alb[size_t( t.i ) * 3];
				double Lh[3] = { 0, 0, 0 };
				const double ns = hn[0] * sd[0] + hn[1] * sd[1] + hn[2] * sd[2];
				if ( sunOn && ns > 0.0 && trace( s, T, h, sd, 1.0e30, true, nullptr, nullptr, nullptr ) < 0 )
					for ( int c = 0; c < 3; c++ )
						Lh[c] += sun[c] * ns;
				for ( size_t li = 0; li * 7 < lamps.size(); li++ ) {
					const double * lp = &lamps[li * 7];
					double lv[3] = { lp[0] - h[0], lp[1] - h[1], lp[2] - h[2] };
					const double dl = std::sqrt( lv[0] * lv[0] + lv[1] * lv[1] + lv[2] * lv[2] );
					if ( dl >= lp[3] || dl <= 0.0 )
						continue;
					for ( double & c : lv )
						c /= dl;
					const double nl = hn[0] * lv[0] + hn[1] * lv[1] + hn[2] * lv[2];
					if ( nl <= 0.0 )
						continue;
					const double x = dl / lp[3], k = 1.0 - std::min( x * x, 1.0 );
					if ( trace( s, T, h, lv, dl - 0.5, true, nullptr, nullptr, nullptr ) >= 0 )
						continue;
					for ( int c = 0; c < 3; c++ )
						Lh[c] += lp[4 + c] * std::pow( k, 2.2 ) * nl;
				}
				if ( Lh[0] + Lh[1] + Lh[2] <= 0.0 )
					continue;
				kind[r] = 3;
				for ( int c = 0; c < 3; c++ )
					acc[r * 3 + size_t( c )] = a[c] / 255.0 * Lh[c] * Tg[c] * cosN;
			}
		};
		std::vector<std::thread> th;
		for ( unsigned k = 0; k < nth; k++ )
			th.emplace_back( work );
		for ( auto & t : th )
			t.join();
		double E[3] = { 0, 0, 0 }, sky = 0, hit = 0, lit = 0;
		const double w = 4.0 * 3.14159265358979323846 / double( M );
		for ( size_t r = 0; r < M; r++ ) {
			const double * d = &D[r * 3];
			const double cosN = std::max( 0.0, d[0] * n[0] + d[1] * n[1] + d[2] * n[2] );
			for ( int c = 0; c < 3; c++ )
				E[c] += acc[r * 3 + size_t( c )] * w;
			sky += kind[r] == 1 ? cosN * w / 3.14159265358979323846 : 0.0;
			hit += kind[r] >= 2 ? cosN * w / 3.14159265358979323846 : 0.0;
			lit += kind[r] == 3 ? cosN * w / 3.14159265358979323846 : 0.0;
		}
		std::fprintf( out, "%zu\t%.6f\t%.6f\t%.6f\t%.5f\t%.5f\t%.5f\t%zu\n", p, E[0], E[1], E[2], sky, hit, lit, T.size() );
		std::fflush( out );
	}
	std::fclose( out );
	return 0;
}
