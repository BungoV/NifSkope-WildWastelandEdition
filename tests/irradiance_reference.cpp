// irradiance_reference -- the brute-force FULL-irradiance reference for the probe GI (lane GISHADE1, 2026-10-13).
//
// Shares NO code with src/probebake.cpp or src/probegi.cpp: its soup reader and glass walk are copies of
// tests/bounce_reference.cpp's, its BVH is its own (a plain median split). Where bounce_reference stops at the
// sun's first bounce and leaves the sky out, this one follows each path through <bounces> surfaces and keeps
// every source apart, so a shortfall can be told as strength (the first bounce) or count (the later ones):
//
//   irradiance_reference <soup.psp> <points.txt> <rays> <bounces> <sx> <sy> <sz> <sr> <sg> <sb> <sky.txt> <out.tsv>
//
// points.txt: "x y z nx ny nz" a line. s = toward the sun, (sr sg sb) its colour in the viewport's units with the
// weather's Sunlight Scale (the bake's gi_sky.txt "sun" line). sky.txt: the six "amb i r g b" lines of gi_sky.txt
// (+X -X +Y -Y +Z -Z, the weather's directional ambient), read as the sky's radiance by direction,
// L(d) = sum d_i^2 x amb[the axis of d_i's sign] -- the radiance a white surface shows under it.
// A path starts with a stratified cosine-weighted direction at the point and goes on with a random cosine
// direction at each hit (Lambert, the soup's ALB1 albedo, bytes / 255). A miss is sky. At each hit the sun is
// added when a shadow ray reaches it (next event). Alpha-test holes pass, glass panes multiply their tint.
// Units: the probe store's (pi x the radiance a white Lambert surface shows): a surface of albedo a shows
// a x E / pi, the sun straight on is pi x its colour.
//
// out.tsv: index, then r g b for each of: sun direct (V x pi x sun x n.s at the point), sky direct (order 0),
// sun bounce 1, sun bounce 2, sun bounce 3+, sky bounce 1, sky bounce 2+, total; then the sky share (the share
// of the cosine-weighted rays that miss at once).
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

// the BVH: a median split on the longest centroid axis, 4 triangles a leaf
struct Node { float lo[3], hi[3]; int left, first, count; };
struct Bvh
{
	const Soup * s = nullptr;
	std::vector<uint32_t> idx;
	std::vector<Node> nodes;
	std::vector<float> cen;
	void build( const Soup & soup )
	{
		s = &soup;
		const size_t n = soup.v.size() / 9;
		idx.resize( n );
		cen.resize( n * 3 );
		for ( size_t i = 0; i < n; i++ ) {
			idx[i] = uint32_t( i );
			for ( int c = 0; c < 3; c++ )
				cen[i * 3 + c] = ( soup.v[i * 9 + c] + soup.v[i * 9 + 3 + c] + soup.v[i * 9 + 6 + c] ) / 3.0f;
		}
		nodes.reserve( n / 2 + 16 );
		nodes.push_back( Node() );
		split( 0, 0, int( n ) );
	}
	void split( int ni, int first, int count )
	{
		Node nd;
		for ( int c = 0; c < 3; c++ ) { nd.lo[c] = 1e30f; nd.hi[c] = -1e30f; }
		float clo[3] = { 1e30f, 1e30f, 1e30f }, chi[3] = { -1e30f, -1e30f, -1e30f };
		for ( int k = first; k < first + count; k++ ) {
			const float * v = &s->v[size_t( idx[size_t( k )] ) * 9];
			for ( int j = 0; j < 3; j++ )
				for ( int c = 0; c < 3; c++ ) {
					nd.lo[c] = std::min( nd.lo[c], v[j * 3 + c] );
					nd.hi[c] = std::max( nd.hi[c], v[j * 3 + c] );
				}
			for ( int c = 0; c < 3; c++ ) {
				clo[c] = std::min( clo[c], cen[size_t( idx[size_t( k )] ) * 3 + c] );
				chi[c] = std::max( chi[c], cen[size_t( idx[size_t( k )] ) * 3 + c] );
			}
		}
		nd.left = -1;
		nd.first = first;
		nd.count = count;
		int ax = 0;
		for ( int c = 1; c < 3; c++ )
			if ( chi[c] - clo[c] > chi[ax] - clo[ax] )
				ax = c;
		if ( count <= 4 || chi[ax] - clo[ax] <= 0.0f ) {
			nodes[size_t( ni )] = nd;
			return;
		}
		const int mid = first + count / 2;
		std::nth_element( idx.begin() + first, idx.begin() + mid, idx.begin() + first + count,
			[&]( uint32_t x, uint32_t y ) { return cen[size_t( x ) * 3 + size_t( ax )] < cen[size_t( y ) * 3 + size_t( ax )]; } );
		nd.left = int( nodes.size() );
		nd.count = 0;
		nodes[size_t( ni )] = nd;
		nodes.push_back( Node() );
		nodes.push_back( Node() );
		split( nd.left, first, mid - first );
		split( nd.left + 1, mid, first + count - mid );
	}
	static bool box( const Node & n, const double o[3], const double inv[3], double tmax )
	{
		double t0 = 0.0, t1 = tmax;
		for ( int c = 0; c < 3; c++ ) {
			double a = ( n.lo[c] - o[c] ) * inv[c], b = ( n.hi[c] - o[c] ) * inv[c];
			if ( a > b )
				std::swap( a, b );
			t0 = std::max( t0, a );
			t1 = std::min( t1, b );
			if ( t0 > t1 )
				return false;
		}
		return true;
	}
	// the nearest hit (or any, when any) along (o, d) before tmax: the soup triangle, or -1
	long long trace( const double o[3], const double d[3], double tmax, bool any, double * tOut ) const
	{
		const double inv[3] = { 1.0 / ( d[0] != 0 ? d[0] : 1e-30 ), 1.0 / ( d[1] != 0 ? d[1] : 1e-30 ), 1.0 / ( d[2] != 0 ? d[2] : 1e-30 ) };
		int stack[128];
		int sp = 0;
		stack[sp++] = 0;
		long long best = -1;
		double tb = tmax;
		while ( sp ) {
			const Node & n = nodes[size_t( stack[--sp] )];
			if ( !box( n, o, inv, tb ) )
				continue;
			if ( n.left >= 0 ) {
				stack[sp++] = n.left;
				stack[sp++] = n.left + 1;
				continue;
			}
			for ( int k = n.first; k < n.first + n.count; k++ ) {
				const uint32_t ti = idx[size_t( k )];
				const float * v = &s->v[size_t( ti ) * 9];
				const double e1[3] = { v[3] - v[0], v[4] - v[1], v[5] - v[2] }, e2[3] = { v[6] - v[0], v[7] - v[1], v[8] - v[2] };
				const double h[3] = { d[1] * e2[2] - d[2] * e2[1], d[2] * e2[0] - d[0] * e2[2], d[0] * e2[1] - d[1] * e2[0] };
				const double det = e1[0] * h[0] + e1[1] * h[1] + e1[2] * h[2];
				if ( std::fabs( det ) < 1e-12 )
					continue;
				const double iv = 1.0 / det;
				const double sv[3] = { o[0] - v[0], o[1] - v[1], o[2] - v[2] };
				const double u = ( sv[0] * h[0] + sv[1] * h[1] + sv[2] * h[2] ) * iv;
				if ( u < 0.0 || u > 1.0 )
					continue;
				const double q[3] = { sv[1] * e1[2] - sv[2] * e1[1], sv[2] * e1[0] - sv[0] * e1[2], sv[0] * e1[1] - sv[1] * e1[0] };
				const double w = ( d[0] * q[0] + d[1] * q[1] + d[2] * q[2] ) * iv;
				if ( w < 0.0 || u + w > 1.0 )
					continue;
				const double t = ( e2[0] * q[0] + e2[1] * q[1] + e2[2] * q[2] ) * iv;
				if ( t > 0.01 && t < tb && !s->hole( ti, u, w ) ) {
					tb = t;
					best = ti;
					if ( any ) {
						if ( tOut )
							*tOut = tb;
						return best;
					}
				}
			}
		}
		if ( tOut )
			*tOut = tb;
		return best;
	}
};

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
	if ( argc != 13 ) {
		std::fprintf( stderr, "usage: irradiance_reference <soup.psp> <points.txt> <rays> <bounces> <sx> <sy> <sz> <sr> <sg> <sb> <sky.txt> <out.tsv>\n" );
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
	const int rays = std::atoi( argv[3] ), bounces = std::max( 1, std::atoi( argv[4] ) );
	double sd[3] = { std::atof( argv[5] ), std::atof( argv[6] ), std::atof( argv[7] ) };
	const double sl = std::sqrt( sd[0] * sd[0] + sd[1] * sd[1] + sd[2] * sd[2] );
	for ( double & c : sd )
		c /= sl;
	const double sun[3] = { std::atof( argv[8] ), std::atof( argv[9] ), std::atof( argv[10] ) };
	double amb[6][3] = {};
	if ( FILE * f = std::fopen( argv[11], "r" ) ) {
		char line[256];
		while ( std::fgets( line, sizeof line, f ) ) {
			int i;
			double r, g, b;
			if ( std::sscanf( line, "amb %d %lf %lf %lf", &i, &r, &g, &b ) == 4 && i >= 0 && i < 6 ) {
				amb[i][0] = r;
				amb[i][1] = g;
				amb[i][2] = b;
			}
		}
		std::fclose( f );
	}
	const bool sunOn = sd[2] > 0.0 && ( sun[0] > 0.0 || sun[1] > 0.0 || sun[2] > 0.0 );
	Bvh bvh;
	bvh.build( s );
	const double PI = 3.14159265358979323846;
	auto skyL = [&]( const double d[3], double L[3] ) {
		for ( int c = 0; c < 3; c++ )
			L[c] = d[0] * d[0] * amb[d[0] >= 0 ? 0 : 1][c] + d[1] * d[1] * amb[d[1] >= 0 ? 2 : 3][c] + d[2] * d[2] * amb[d[2] >= 0 ? 4 : 5][c];
	};
	// a cosine-weighted direction about n from two numbers in [0, 1)
	auto cosDir = [&]( const double n[3], double u1, double u2, double d[3] ) {
		const double r = std::sqrt( u1 ), ph = 2.0 * PI * u2;
		const double x = r * std::cos( ph ), y = r * std::sin( ph ), z = std::sqrt( std::max( 0.0, 1.0 - u1 ) );
		const double t[3] = { std::fabs( n[0] ) < 0.9 ? 1.0 : 0.0, std::fabs( n[0] ) < 0.9 ? 0.0 : 1.0, 0.0 };
		double b[3] = { n[1] * t[2] - n[2] * t[1], n[2] * t[0] - n[0] * t[2], n[0] * t[1] - n[1] * t[0] };
		const double bl = std::sqrt( b[0] * b[0] + b[1] * b[1] + b[2] * b[2] );
		for ( double & c : b )
			c /= bl;
		const double a[3] = { b[1] * n[2] - b[2] * n[1], b[2] * n[0] - b[0] * n[2], b[0] * n[1] - b[1] * n[0] };
		for ( int c = 0; c < 3; c++ )
			d[c] = a[c] * x + b[c] * y + n[c] * z;
	};
	FILE * out = std::fopen( argv[12], "w" );
	if ( !out )
		return 1;
	std::fprintf( out, "# irradiance_reference rays %d bounces %d tris %zu sun to %.5f %.5f %.5f colour %.5f %.5f %.5f sky +z %.4f %.4f %.4f\n",
		rays, bounces, s.v.size() / 9, sd[0], sd[1], sd[2], sun[0], sun[1], sun[2], amb[4][0], amb[4][1], amb[4][2] );
	std::fprintf( out, "# idx, r g b of: sunDirect skyDirect sunB1 sunB2 sunB3+ skyB1 skyB2+ total; skyShare\n" );
	const unsigned nth = std::max( 1u, std::thread::hardware_concurrency() );
	const int rows = std::max( 1, int( std::lround( std::sqrt( double( rays ) ) ) ) ), cols = std::max( 1, rays / rows );
	const int M = rows * cols;
	for ( size_t p = 0; p * 6 < P.size(); p++ ) {
		const double * pt = &P[p * 6];
		double n[3] = { pt[3], pt[4], pt[5] };
		const double nl = std::sqrt( n[0] * n[0] + n[1] * n[1] + n[2] * n[2] );
		for ( double & c : n )
			c /= nl;
		const double o0[3] = { pt[0] + n[0] * 0.5, pt[1] + n[1] * 0.5, pt[2] + n[2] * 0.5 };
		double sunD[3] = { 0, 0, 0 };   // the sun straight onto the point
		const double ns0 = n[0] * sd[0] + n[1] * sd[1] + n[2] * sd[2];
		if ( sunOn && ns0 > 0 && bvh.trace( o0, sd, 1e30, true, nullptr ) < 0 ) {
			double Tg[3] = { 1, 1, 1 };
			if ( !s.glass.empty() )
				throughGlass( s, o0, sd, 1e30, Tg );
			for ( int c = 0; c < 3; c++ )
				sunD[c] = PI * sun[c] * ns0 * Tg[c];
		}
		// a thread's sums, 6 slots x rgb: 0 sky direct, 1 sunB1, 2 sunB2, 3 sunB3+, 4 skyB1, 5 skyB2+
		std::vector<double> part( size_t( nth ) * 18, 0.0 ), miss( nth, 0.0 );
		std::atomic<int> next( 0 );
		auto work = [&]( unsigned id ) {
			uint64_t st = 0x9E3779B97F4A7C15ull ^ ( uint64_t( p + 1 ) * 0xBF58476D1CE4E5B9ull ) ^ ( uint64_t( id + 1 ) * 0x94D049BB133111EBull );
			auto rnd = [&]() { st ^= st << 13; st ^= st >> 7; st ^= st << 17; return double( st >> 11 ) / double( 1ull << 53 ); };
			double * acc = &part[size_t( id ) * 18];
			for ( ;; ) {
				const int r = next.fetch_add( 1 );
				if ( r >= M )
					return;
				double d[3];
				cosDir( n, ( r / cols + rnd() ) / rows, ( r % cols + rnd() ) / cols, d );
				double o[3] = { o0[0], o0[1], o0[2] }, beta[3] = { 1, 1, 1 };
				for ( int k = 0; k <= bounces; k++ ) {
					double th = 0;
					const long long ti = bvh.trace( o, d, 1e30, false, &th );
					double Tg[3] = { 1, 1, 1 };
					if ( !s.glass.empty() )
						throughGlass( s, o, d, ti < 0 ? 1e30 : th, Tg );
					for ( int c = 0; c < 3; c++ )
						beta[c] *= Tg[c];
					if ( ti < 0 ) {
						double L[3];
						skyL( d, L );
						const int slot = k == 0 ? 0 : k == 1 ? 4 : 5;
						for ( int c = 0; c < 3; c++ )
							acc[slot * 3 + c] += beta[c] * L[c];
						if ( k == 0 )
							miss[id] += 1.0;
						break;
					}
					if ( k == bounces )
						break;
					const float * v = &s.v[size_t( ti ) * 9];
					const double e1[3] = { v[3] - v[0], v[4] - v[1], v[5] - v[2] }, e2[3] = { v[6] - v[0], v[7] - v[1], v[8] - v[2] };
					double hn[3] = { e1[1] * e2[2] - e1[2] * e2[1], e1[2] * e2[0] - e1[0] * e2[2], e1[0] * e2[1] - e1[1] * e2[0] };
					const double hl = std::sqrt( hn[0] * hn[0] + hn[1] * hn[1] + hn[2] * hn[2] );
					if ( !( hl > 0 ) )
						break;
					for ( double & c : hn )
						c /= hl;
					if ( hn[0] * d[0] + hn[1] * d[1] + hn[2] * d[2] > 0 )
						for ( double & c : hn )
							c = -c;
					const uint8_t * a = &s.alb[size_t( ti ) * 3];
					for ( int c = 0; c < 3; c++ ) {
						o[c] = o[c] + d[c] * th + hn[c] * 0.5;
						beta[c] *= a[c] / 255.0;
					}
					const double ns = hn[0] * sd[0] + hn[1] * sd[1] + hn[2] * sd[2];
					if ( sunOn && ns > 0 && bvh.trace( o, sd, 1e30, true, nullptr ) < 0 ) {
						double Ts[3] = { 1, 1, 1 };
						if ( !s.glass.empty() )
							throughGlass( s, o, sd, 1e30, Ts );
						const int slot = k == 0 ? 1 : k == 1 ? 2 : 3;
						for ( int c = 0; c < 3; c++ )
							acc[slot * 3 + c] += beta[c] * sun[c] * ns * Ts[c];
					}
					cosDir( hn, rnd(), rnd(), d );
				}
			}
		};
		std::vector<std::thread> th;
		for ( unsigned k = 0; k < nth; k++ )
			th.emplace_back( work, k );
		for ( auto & t : th )
			t.join();
		double R[18] = {}, sky = 0;
		for ( unsigned k = 0; k < nth; k++ ) {
			for ( int j = 0; j < 18; j++ )
				R[j] += part[size_t( k ) * 18 + size_t( j )];
			sky += miss[k];
		}
		for ( double & x : R )   // cosine sampling: E = pi x the mean radiance
			x *= PI / M;
		std::fprintf( out, "%zu\t%.6f\t%.6f\t%.6f", p, sunD[0], sunD[1], sunD[2] );
		for ( int j = 0; j < 6; j++ )
			std::fprintf( out, "\t%.6f\t%.6f\t%.6f", R[j * 3], R[j * 3 + 1], R[j * 3 + 2] );
		for ( int c = 0; c < 3; c++ ) {
			double t = sunD[c];
			for ( int j = 0; j < 6; j++ )
				t += R[j * 3 + c];
			std::fprintf( out, "\t%.6f", t );
		}
		std::fprintf( out, "\t%.5f\n", sky / M );
		std::fflush( out );
	}
	std::fclose( out );
	return 0;
}
