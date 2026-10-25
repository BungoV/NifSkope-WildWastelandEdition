// path_reference -- a per-pixel, multi-bounce path-traced reference picture for the cell view's GI (lane DECK1,
// 2026-10-23; the Division deck's s67 "GPU path tracer" judge, DECK_MATRIX E5).
//
// Shares NO code with src/probebake.cpp, src/probegi.cpp or src/bakert*: its soup reader extends the one in
// tests/irradiance_reference.cpp, its BVH is its own (binned SAH), its light laws are re-typed from the relight's
// documented ones (PRTP2 radial curve, spot cone, hemisphere / box volumes) so a disagreement is a finding.
//
//   path_reference soup=<psp> sky=<gi_sky.txt> [lights=<gi_lights.txt>] out=<prefix>
//                  ( cam="ex ey ez tx ty tz fov" size=WxH | gbuf=<file.gbf> )
//                  [spp=256] [bounces=4] [seed=1] [threads=0] [red=<name>]
//   path_reference fixture [spp=4096] [threads=0] [red=<name>]      the analytic gates (exit 0 = all PASS)
//
// cam: the lib.sh / live path key "eye target fov" (vertical fov, degrees; notes/gishade1/proj.py's law:
//   f = t - e, r = f x z, u = r x f, a pixel centre (px + 0.5, py + 0.5) looks along f + x r - y u with
//   x = (px + 0.5 - W/2) / s, y = (py + 0.5 - H/2) / s, s = (H/2) / tan(fov/2); row 0 at the top).
//   The camera ray skips one-sided triangles seen from behind (the renderer culls them) and passes glass.
// gbuf: the cell view's own surface points (tools/deck_gbuf.py from probe 2/3/4 dumps): 'GBF1', W, H, then W x H x
//   7 float32 (x y z nx ny nz valid), row 0 at the top -- the irradiance at the very point and normal it drew.
//
// Light, in the probe store's unit (pi x the radiance a white Lambert surface shows; a surface of albedo a shows a E / pi):
//   sun   gi_sky.txt "sunTo" / "sun" (the colour already carries the weather's Sunlight Scale), one shadow ray;
//   sky   a miss: L(d) = sum d_i^2 x amb[axis of d_i's sign] (the weather's directional ambient, gi_sky.txt "amb");
//   lamp  gi_lights.txt (the relight's own placed lights, WW_CELL_GI_DUMP): E = pi x colour x radial x spot x n.l,
//         shadowed from 0.5 off the surface to fixtureClear short of the light, only inside its shape;
//   emit  EMT1 glowing triangles (Le = (glow x map)^2, added where a path hits one) + EMV1 veils (neon; never
//         occlude; next-event sampled by power, one-sided unless flagged).
// A path leaves the point along a stratified cosine direction and goes on by Lambert (ALB1 albedo, bytes / 255),
// through alpha-test holes (AMK1/AMK2) and glass panes (GLS1 tint). Every source is kept apart by ORDER: order 0 =
// the light straight onto the point, order k = after k diffuse bounces (3 = 3 and more).
//
// out: <prefix>.ptr = 'PTR1', W, H, layers L, then L names (16 bytes each), then L x H x W x 3 float32 (row 0 top):
//   sun0 sun1 sun2 sun3 sky0 sky1 sky2 sky3 lamp0 lamp1 lamp2 lamp3 emit0 emit1 emit2 emit3 (E, store units),
//   alb (the hit's albedo), pos (world), nrm (the normal used), misc (sky share, hit 0/1, glass crossings);
// and <prefix>.txt (arguments, counts, seconds). Deterministic: a pixel's numbers depend on (seed, pixel) only,
// never on the thread count or order.
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <thread>
#include <vector>

namespace {

const double PI = 3.14159265358979323846;

struct Soup
{
	std::vector<float> v;                  // 9 a triangle
	std::vector<uint8_t> alb;              // 3 a triangle
	std::vector<uint8_t> two;              // TWO1: 1 = two-sided (empty: all two-sided)
	std::vector<int16_t> vn;               // VNM1: 9 snorm16 a triangle (empty: none)
	std::vector<float> glass;              // GLS1 panes, 9 each
	std::vector<double> glassT;            // 3 each
	struct Map { int w = 0, h = 0; std::vector<uint8_t> a; };
	struct Masked { int map = -1; uint32_t thr = 0; float uv[6]; float as[3] = { 1, 1, 1 }; };
	std::vector<Map> maps;
	std::vector<int> maskOf;
	std::vector<Masked> masked;
	// EMT1 glow
	struct GMap { int w = 0, h = 0; std::vector<uint8_t> rgb; };
	struct GEm { float e[3]; int map = -1; };
	struct GTri { int em = -1; float uv[6]; };
	std::vector<GMap> gmaps;
	std::vector<GEm> gems;
	std::vector<int> glowOf;
	std::vector<GTri> gtris;
	// EMV1 veils
	std::vector<float> veil, veilLe;      // 9 / 3 a triangle
	std::vector<uint8_t> veilTwo;

	size_t n() const { return v.size() / 9; }
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
	bool glows( size_t tri ) const { return !glowOf.empty() && tri < glowOf.size() && glowOf[tri] >= 0; }
	void glowLe( size_t tri, double b1, double b2, double out[3] ) const
	{
		out[0] = out[1] = out[2] = 0.0;
		if ( !glows( tri ) )
			return;
		const GTri & t = gtris[size_t( glowOf[tri] )];
		const GEm & em = gems[size_t( t.em )];
		double g[3] = { 1, 1, 1 };
		if ( em.map >= 0 ) {
			const GMap & mp = gmaps[size_t( em.map )];
			const double b0 = 1.0 - b1 - b2;
			double u = b0 * t.uv[0] + b1 * t.uv[2] + b2 * t.uv[4], w = b0 * t.uv[1] + b1 * t.uv[3] + b2 * t.uv[5];
			u -= std::floor( u );
			w -= std::floor( w );
			const int x = std::min( std::max( int( u * mp.w ), 0 ), mp.w - 1 ), y = std::min( std::max( int( w * mp.h ), 0 ), mp.h - 1 );
			for ( int k = 0; k < 3; k++ )
				g[k] = mp.rgb[( size_t( y ) * size_t( mp.w ) + size_t( x ) ) * 3 + size_t( k )] / 255.0;
		}
		for ( int k = 0; k < 3; k++ ) {
			const double e = double( em.e[k] ) * g[k];
			out[k] = e * e;
		}
	}
};

bool readSoup( const char * path, Soup & s, std::string & tails )
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
		else
			tails += "ALB1 ";
	}
	bool more = std::fread( tail, 4, 2, f ) == 2;
	if ( more && tail[0] == 0x31534C47u ) {   // 'GLS1'
		s.glass.resize( size_t( tail[1] ) * 9 );
		std::vector<uint8_t> gt( size_t( tail[1] ) * 3 );
		if ( std::fread( s.glass.data(), 4, s.glass.size(), f ) == s.glass.size() && std::fread( gt.data(), 1, gt.size(), f ) == gt.size() ) {
			for ( uint8_t c : gt )
				s.glassT.push_back( c / 255.0 );
			tails += "GLS1 ";
		} else
			s.glass.clear();
		more = std::fread( tail, 4, 2, f ) == 2;
	}
	if ( more && tail[0] == 0x314F5754u ) {   // 'TWO1'
		if ( tail[1] == n ) {
			s.two.resize( n );
			if ( std::fread( s.two.data(), 1, n, f ) != n )
				s.two.clear();
			else
				tails += "TWO1 ";
		} else
			_fseeki64( f, static_cast<long long>( tail[1] ), SEEK_CUR );
		more = std::fread( tail, 4, 2, f ) == 2;
	}
	while ( more ) {   // sized tails: magic, count, u32 bytes, body
		uint32_t bytes = 0;
		if ( std::fread( &bytes, 4, 1, f ) != 1 )
			break;
		const long long body = _ftelli64( f );
		bool ok = true;
		auto u32 = [&]() { uint32_t x = 0; ok = ok && std::fread( &x, 4, 1, f ) == 1; return x; };
		if ( tail[0] == 0x314B4D41u || tail[0] == 0x324B4D41u ) {   // 'AMK1' / 'AMK2'
			const bool amk2 = tail[0] == 0x324B4D41u;
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
			tails += amk2 ? "AMK2 " : "AMK1 ";
		} else if ( tail[0] == 0x31544D45u ) {   // 'EMT1' (src/probeplace.cpp's writer layout)
			const uint32_t nm = u32();
			for ( uint32_t m = 0; ok && m < nm; m++ ) {
				_fseeki64( f, static_cast<long long>( u32() ), SEEK_CUR );   // the name
				Soup::GMap mp;
				mp.w = int( u32() );
				mp.h = int( u32() );
				ok = ok && mp.w > 0 && mp.h > 0 && mp.w <= 16384 && mp.h <= 16384;
				if ( !ok )
					break;
				mp.rgb.resize( size_t( mp.w ) * size_t( mp.h ) * 3 );
				ok = std::fread( mp.rgb.data(), 1, mp.rgb.size(), f ) == mp.rgb.size();
				s.gmaps.push_back( std::move( mp ) );
			}
			const uint32_t ne = u32();
			for ( uint32_t e = 0; ok && e < ne; e++ ) {
				Soup::GEm em;
				ok = std::fread( em.e, 4, 3, f ) == 3;
				em.map = int( int32_t( u32() ) );
				ok = ok && em.map < int( nm );
				s.gems.push_back( em );
			}
			s.glowOf.assign( n, -1 );
			for ( uint32_t i = 0; ok && i < tail[1]; i++ ) {
				const uint32_t tri = u32();
				Soup::GTri t;
				t.em = int( int32_t( u32() ) );
				ok = ok && std::fread( t.uv, 4, 6, f ) == 6 && tri < n && t.em >= 0 && t.em < int( ne );
				if ( !ok )
					break;
				s.glowOf[tri] = int( s.gtris.size() );
				s.gtris.push_back( t );
			}
			tails += "EMT1 ";
		} else if ( tail[0] == 0x314D4E56u && tail[1] == n ) {   // 'VNM1'
			s.vn.resize( n * 9 );
			ok = std::fread( s.vn.data(), 2, s.vn.size(), f ) == s.vn.size();
			tails += "VNM1 ";
		} else if ( tail[0] == 0x31564D45u && uint64_t( tail[1] ) * 52 == bytes ) {   // 'EMV1'
			for ( uint32_t i = 0; ok && i < tail[1]; i++ ) {
				float t[12];
				uint8_t two[4];
				ok = std::fread( t, 4, 12, f ) == 12 && std::fread( two, 1, 4, f ) == 4;
				s.veil.insert( s.veil.end(), t, t + 9 );
				s.veilLe.insert( s.veilLe.end(), t + 9, t + 12 );
				s.veilTwo.push_back( two[0] );
			}
			tails += "EMV1 ";
		}
		if ( !ok ) {
			std::fclose( f );
			return false;
		}
		more = _fseeki64( f, body + bytes, SEEK_SET ) == 0 && std::fread( tail, 4, 2, f ) == 2;
	}
	std::fclose( f );
	return !s.alb.empty();
}

// ---------------------------------------------------------------- BVH (binned SAH, 4 triangles a leaf)
struct Node { float lo[3], hi[3]; int left, first, count; };
struct Hit { long long tri = -1; double t = 0, b1 = 0, b2 = 0; bool back = false; };

struct Bvh
{
	std::vector<uint32_t> idx;
	std::vector<Node> nodes;
	std::vector<float> tv;   // the triangles in leaf order, 9 each
	void build( const float * v, size_t n )
	{
		idx.resize( n );
		std::vector<float> cen( n * 3 ), blo( n * 3 ), bhi( n * 3 );
		for ( size_t i = 0; i < n; i++ ) {
			idx[i] = uint32_t( i );
			for ( int c = 0; c < 3; c++ ) {
				const float a = v[i * 9 + size_t( c )], b = v[i * 9 + 3 + size_t( c )], d = v[i * 9 + 6 + size_t( c )];
				blo[i * 3 + size_t( c )] = std::min( a, std::min( b, d ) );
				bhi[i * 3 + size_t( c )] = std::max( a, std::max( b, d ) );
				cen[i * 3 + size_t( c )] = 0.5f * ( blo[i * 3 + size_t( c )] + bhi[i * 3 + size_t( c )] );
			}
		}
		nodes.clear();
		nodes.reserve( n / 2 + 16 );
		nodes.push_back( Node() );
		if ( n )
			split( 0, 0, int( n ), cen, blo, bhi );
		else
			nodes[0] = Node { { 1, 1, 1 }, { -1, -1, -1 }, -1, 0, 0 };
		tv.resize( n * 9 );
		for ( size_t k = 0; k < n; k++ )
			std::memcpy( &tv[k * 9], &v[size_t( idx[k] ) * 9], 36 );
	}
	void split( int ni, int first, int count, const std::vector<float> & cen, const std::vector<float> & blo, const std::vector<float> & bhi )
	{
		Node nd;
		float clo[3] = { 1e30f, 1e30f, 1e30f }, chi[3] = { -1e30f, -1e30f, -1e30f };
		for ( int c = 0; c < 3; c++ ) { nd.lo[c] = 1e30f; nd.hi[c] = -1e30f; }
		for ( int k = first; k < first + count; k++ ) {
			const size_t t = idx[size_t( k )];
			for ( int c = 0; c < 3; c++ ) {
				nd.lo[c] = std::min( nd.lo[c], blo[t * 3 + size_t( c )] );
				nd.hi[c] = std::max( nd.hi[c], bhi[t * 3 + size_t( c )] );
				clo[c] = std::min( clo[c], cen[t * 3 + size_t( c )] );
				chi[c] = std::max( chi[c], cen[t * 3 + size_t( c )] );
			}
		}
		nd.left = -1;
		nd.first = first;
		nd.count = count;
		if ( count <= 4 ) {
			nodes[size_t( ni )] = nd;
			return;
		}
		// 16 bins on each axis, the cheapest surface-area split
		const int NB = 16;
		int bestAx = -1, bestB = -1;
		double bestCost = double( count );   // vs a leaf (relative units)
		auto area = []( const float lo[3], const float hi[3] ) {
			const double e0 = std::max( 0.0f, hi[0] - lo[0] ), e1 = std::max( 0.0f, hi[1] - lo[1] ), e2 = std::max( 0.0f, hi[2] - lo[2] );
			return e0 * e1 + e1 * e2 + e2 * e0;
		};
		const double pa = std::max( area( nd.lo, nd.hi ), 1e-12 );
		for ( int ax = 0; ax < 3; ax++ ) {
			const float ext = chi[ax] - clo[ax];
			if ( !( ext > 0.0f ) )
				continue;
			int cnt[NB] = {};
			float bl[NB][3], bh[NB][3];
			for ( int b = 0; b < NB; b++ )
				for ( int c = 0; c < 3; c++ ) { bl[b][c] = 1e30f; bh[b][c] = -1e30f; }
			for ( int k = first; k < first + count; k++ ) {
				const size_t t = idx[size_t( k )];
				int b = int( ( cen[t * 3 + size_t( ax )] - clo[ax] ) / ext * NB );
				b = std::min( std::max( b, 0 ), NB - 1 );
				cnt[b]++;
				for ( int c = 0; c < 3; c++ ) {
					bl[b][c] = std::min( bl[b][c], blo[t * 3 + size_t( c )] );
					bh[b][c] = std::max( bh[b][c], bhi[t * 3 + size_t( c )] );
				}
			}
			double ra[NB];
			int rc[NB];
			float rl[3] = { 1e30f, 1e30f, 1e30f }, rh[3] = { -1e30f, -1e30f, -1e30f };
			int rn = 0;
			for ( int b = NB - 1; b > 0; b-- ) {
				rn += cnt[b];
				for ( int c = 0; c < 3; c++ ) { rl[c] = std::min( rl[c], bl[b][c] ); rh[c] = std::max( rh[c], bh[b][c] ); }
				ra[b] = rn ? area( rl, rh ) : 0.0;
				rc[b] = rn;
			}
			float ll[3] = { 1e30f, 1e30f, 1e30f }, lh[3] = { -1e30f, -1e30f, -1e30f };
			int ln = 0;
			for ( int b = 0; b < NB - 1; b++ ) {
				ln += cnt[b];
				for ( int c = 0; c < 3; c++ ) { ll[c] = std::min( ll[c], bl[b][c] ); lh[c] = std::max( lh[c], bh[b][c] ); }
				if ( !ln || !rc[b + 1] )
					continue;
				const double cost = 0.125 + ( ln * area( ll, lh ) + rc[b + 1] * ra[b + 1] ) / pa;
				if ( cost < bestCost ) {
					bestCost = cost;
					bestAx = ax;
					bestB = b;
				}
			}
		}
		int mid;
		if ( bestAx >= 0 ) {
			const float ext = chi[bestAx] - clo[bestAx];
			auto it = std::partition( idx.begin() + first, idx.begin() + first + count, [&]( uint32_t t ) {
				int b = int( ( cen[size_t( t ) * 3 + size_t( bestAx )] - clo[bestAx] ) / ext * NB );
				return std::min( std::max( b, 0 ), NB - 1 ) <= bestB;
			} );
			mid = int( it - idx.begin() );
		} else if ( count > 16 ) {   // no split beats a leaf but the leaf is big: a median split on the longest axis
			int ax = 0;
			for ( int c = 1; c < 3; c++ )
				if ( chi[c] - clo[c] > chi[ax] - clo[ax] )
					ax = c;
			mid = first + count / 2;
			std::nth_element( idx.begin() + first, idx.begin() + mid, idx.begin() + first + count,
				[&]( uint32_t x, uint32_t y ) { return cen[size_t( x ) * 3 + size_t( ax )] < cen[size_t( y ) * 3 + size_t( ax )]; } );
		} else {
			nodes[size_t( ni )] = nd;
			return;
		}
		if ( mid <= first || mid >= first + count ) {
			nodes[size_t( ni )] = nd;
			return;
		}
		nd.left = int( nodes.size() );
		nd.count = 0;
		nodes[size_t( ni )] = nd;
		nodes.push_back( Node() );
		nodes.push_back( Node() );
		split( nd.left, first, mid - first, cen, blo, bhi );
		split( nd.left + 1, mid, first + count - mid, cen, blo, bhi );
	}
	static bool box( const Node & n, const double o[3], const double inv[3], double tmax, double * tin )
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
		*tin = t0;
		return true;
	}
	// Moller-Trumbore in double; accept(soup tri, b1, b2, back) may refuse a candidate (a hole, a culled back face)
	template <class Accept, class Visit>
	void walk( const double o[3], const double d[3], double tmin, double tmax, bool any, Accept accept, Visit visit ) const
	{
		if ( nodes.empty() || ( nodes[0].count == 0 && nodes[0].left < 0 ) )
			return;
		const double inv[3] = { 1.0 / ( d[0] != 0 ? d[0] : 1e-30 ), 1.0 / ( d[1] != 0 ? d[1] : 1e-30 ), 1.0 / ( d[2] != 0 ? d[2] : 1e-30 ) };
		int stack[256];
		int sp = 0;
		stack[sp++] = 0;
		double tb = tmax;
		while ( sp ) {
			const Node & n = nodes[size_t( stack[--sp] )];
			double tin;
			if ( !box( n, o, inv, tb, &tin ) )
				continue;
			if ( n.left >= 0 ) {
				double ta = 0, tc = 0;
				const bool ha = box( nodes[size_t( n.left )], o, inv, tb, &ta ), hc = box( nodes[size_t( n.left + 1 )], o, inv, tb, &tc );
				if ( ha && hc ) {   // the nearer child on top
					if ( ta <= tc ) { stack[sp++] = n.left + 1; stack[sp++] = n.left; }
					else { stack[sp++] = n.left; stack[sp++] = n.left + 1; }
				} else if ( ha )
					stack[sp++] = n.left;
				else if ( hc )
					stack[sp++] = n.left + 1;
				continue;
			}
			for ( int k = n.first; k < n.first + n.count; k++ ) {
				const float * v = &tv[size_t( k ) * 9];
				const double e1[3] = { double( v[3] ) - v[0], double( v[4] ) - v[1], double( v[5] ) - v[2] };
				const double e2[3] = { double( v[6] ) - v[0], double( v[7] ) - v[1], double( v[8] ) - v[2] };
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
				if ( !( t > tmin && t < tb ) )
					continue;
				// det < 0: the ray meets the face (v1 - v0) x (v2 - v0) points away from, i.e. its back
				if ( !accept( idx[size_t( k )], u, w, det < 0 ) )
					continue;
				if ( !visit( idx[size_t( k )], t, u, w, det < 0 ) )
					continue;   // a visitor that collects every crossing keeps tb
				tb = t;
				if ( any )
					return;
			}
		}
	}
};

// ---------------------------------------------------------------- the lights
struct Lamp
{
	double pos[3] = { 0, 0, 0 }, radius = 0, color[3] = { 0, 0, 0 };
	bool spot = false;
	double dir[3] = { 1, 0, 0 }, cosOuter = 0, bias = 0, scale = 1, exponent = 2, cone = 1;
	int shape = 0;
	double box[3][4] = {};
	bool inside( const double p[3] ) const
	{
		if ( shape == 1 )
			return ( p[0] - pos[0] ) * dir[0] + ( p[1] - pos[1] ) * dir[1] + ( p[2] - pos[2] ) * dir[2] >= 0.0;
		if ( shape == 2 )
			for ( int k = 0; k < 3; k++ )
				if ( std::fabs( box[k][0] * p[0] + box[k][1] * p[1] + box[k][2] * p[2] + box[k][3] ) > 1.0 )
					return false;
		return true;
	}
	// PRTP2 section 1: k = 1 - clamp(scale x (d / r)^exponent + bias), then k^2.2
	double radial( double d ) const
	{
		const double x = std::min( std::max( d / std::max( radius, 0.001 ), 0.0 ), 1.0 );
		const double xe = exponent > 0.0 ? std::pow( x, exponent ) : 1.0;
		const double k = 1.0 - std::min( std::max( scale * xe + bias, 0.0 ), 1.0 );
		return std::pow( k, 2.2 );
	}
};

struct Sky
{
	double amb[6][3] = {};
	double sunTo[3] = { 0, 0, 1 }, sun[3] = { 0, 0, 0 };
	bool flat = false;   // red "flatsky": the mean of the six, every direction alike
	void L( const double d[3], double out[3] ) const
	{
		for ( int c = 0; c < 3; c++ ) {
			if ( flat ) {
				out[c] = ( amb[0][c] + amb[1][c] + amb[2][c] + amb[3][c] + amb[4][c] + amb[5][c] ) / 6.0;
				continue;
			}
			out[c] = d[0] * d[0] * amb[d[0] >= 0 ? 0 : 1][c] + d[1] * d[1] * amb[d[1] >= 0 ? 2 : 3][c] + d[2] * d[2] * amb[d[2] >= 0 ? 4 : 5][c];
		}
	}
	bool sunOn() const { return sunTo[2] > -1.0 && ( sun[0] > 0 || sun[1] > 0 || sun[2] > 0 ); }
};

bool readSky( const char * path, Sky & s )
{
	FILE * f = std::fopen( path, "r" );
	if ( !f )
		return false;
	char line[512];
	int got = 0;
	while ( std::fgets( line, sizeof line, f ) ) {
		int i;
		double a, b, c;
		if ( std::sscanf( line, "amb %d %lf %lf %lf", &i, &a, &b, &c ) == 4 && i >= 0 && i < 6 ) {
			s.amb[i][0] = a; s.amb[i][1] = b; s.amb[i][2] = c; got++;
		} else if ( std::sscanf( line, "sunTo %lf %lf %lf", &a, &b, &c ) == 3 ) {
			const double l = std::sqrt( a * a + b * b + c * c );
			if ( l > 0 ) { s.sunTo[0] = a / l; s.sunTo[1] = b / l; s.sunTo[2] = c / l; }
		} else if ( std::sscanf( line, "sun %lf %lf %lf", &a, &b, &c ) == 3 ) {
			s.sun[0] = a; s.sun[1] = b; s.sun[2] = c;
		}
	}
	std::fclose( f );
	return got == 6;
}

// gi_lights.txt (src/probegi.cpp probeGiDumpLights): "clear <fixtureClear>" then one "light ..." line per lamp
bool readLamps( const char * path, std::vector<Lamp> & L, double & clear )
{
	FILE * f = std::fopen( path, "r" );
	if ( !f )
		return false;
	char line[2048];
	while ( std::fgets( line, sizeof line, f ) ) {
		double c;
		if ( std::sscanf( line, "clear %lf", &c ) == 1 ) {
			clear = c;
			continue;
		}
		if ( std::strncmp( line, "light ", 6 ) )
			continue;
		Lamp l;
		int spot = 0;
		double b[12];
		const int n = std::sscanf( line + 6, "%lf %lf %lf %lf %lf %lf %lf %d %lf %lf %lf %lf %lf %lf %lf %lf %d %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf",
			&l.pos[0], &l.pos[1], &l.pos[2], &l.radius, &l.color[0], &l.color[1], &l.color[2], &spot, &l.dir[0], &l.dir[1], &l.dir[2],
			&l.cosOuter, &l.bias, &l.scale, &l.exponent, &l.cone, &l.shape, b, b + 1, b + 2, b + 3, b + 4, b + 5, b + 6, b + 7, b + 8, b + 9, b + 10, b + 11 );
		if ( n < 7 )
			continue;
		l.spot = spot != 0;
		if ( n >= 29 )
			for ( int k = 0; k < 12; k++ )
				l.box[k / 4][k % 4] = b[k];
		L.push_back( l );
	}
	std::fclose( f );
	return true;
}

// ---------------------------------------------------------------- sampling
struct Rng
{
	uint64_t s;
	explicit Rng( uint64_t seed ) : s( seed ) { next(); }
	uint64_t next()   // splitmix64
	{
		uint64_t z = ( s += 0x9E3779B97F4A7C15ull );
		z = ( z ^ ( z >> 30 ) ) * 0xBF58476D1CE4E5B9ull;
		z = ( z ^ ( z >> 27 ) ) * 0x94D049BB133111EBull;
		return z ^ ( z >> 31 );
	}
	double u() { return double( next() >> 11 ) / double( 1ull << 53 ); }
};

void cosDir( const double n[3], double u1, double u2, double d[3], bool uniformRed )
{
	double x, y, z;
	if ( uniformRed ) {   // red "uniform": a uniform hemisphere read as if it were cosine-weighted
		z = u1;
		const double r = std::sqrt( std::max( 0.0, 1.0 - z * z ) ), ph = 2.0 * PI * u2;
		x = r * std::cos( ph );
		y = r * std::sin( ph );
	} else {
		const double r = std::sqrt( u1 ), ph = 2.0 * PI * u2;
		x = r * std::cos( ph );
		y = r * std::sin( ph );
		z = std::sqrt( std::max( 0.0, 1.0 - u1 ) );
	}
	const double t[3] = { std::fabs( n[0] ) < 0.9 ? 1.0 : 0.0, std::fabs( n[0] ) < 0.9 ? 0.0 : 1.0, 0.0 };
	double b[3] = { n[1] * t[2] - n[2] * t[1], n[2] * t[0] - n[0] * t[2], n[0] * t[1] - n[1] * t[0] };
	const double bl = std::sqrt( b[0] * b[0] + b[1] * b[1] + b[2] * b[2] );
	for ( double & c : b )
		c /= bl;
	const double a[3] = { b[1] * n[2] - b[2] * n[1], b[2] * n[0] - b[0] * n[2], b[0] * n[1] - b[1] * n[0] };
	for ( int c = 0; c < 3; c++ )
		d[c] = a[c] * x + b[c] * y + n[c] * z;
}

// ---------------------------------------------------------------- the scene and the estimator
enum { SUN = 0, SKY = 1, LAMP = 2, EMIT = 3 };
const int ORDERS = 4;
const int NSRC = 4;
const int NE = NSRC * ORDERS * 3;   // the E accumulators of a point

struct Scene
{
	Soup s;
	Bvh bvh, glassBvh, veilBvh;
	Sky sky;
	std::vector<Lamp> lamps;
	double clear = 24.0;
	// EMV1 next-event: the veils' power CDF
	std::vector<double> veilCdf, veilArea, veilN;   // area, unit normal (3 each)
	double veilPower = 0;
	std::string red;
	bool redAlbedo1 = false, redUniform = false, redNoGlass = false, redOrder = false;

	void prepare()
	{
		bvh.build( s.v.data(), s.n() );
		if ( !s.glass.empty() )
			glassBvh.build( s.glass.data(), s.glass.size() / 9 );
		const size_t nv = s.veil.size() / 9;
		veilCdf.resize( nv );
		veilArea.resize( nv );
		veilN.resize( nv * 3 );
		double acc = 0;
		for ( size_t i = 0; i < nv; i++ ) {
			const float * v = &s.veil[i * 9];
			const double e1[3] = { double( v[3] ) - v[0], double( v[4] ) - v[1], double( v[5] ) - v[2] }, e2[3] = { double( v[6] ) - v[0], double( v[7] ) - v[1], double( v[8] ) - v[2] };
			const double c[3] = { e1[1] * e2[2] - e1[2] * e2[1], e1[2] * e2[0] - e1[0] * e2[2], e1[0] * e2[1] - e1[1] * e2[0] };
			const double l = std::sqrt( c[0] * c[0] + c[1] * c[1] + c[2] * c[2] );
			veilArea[i] = 0.5 * l;
			for ( int k = 0; k < 3; k++ )
				veilN[i * 3 + size_t( k )] = l > 0 ? c[k] / l : 0.0;
			const double lum = 0.2126 * s.veilLe[i * 3] + 0.7152 * s.veilLe[i * 3 + 1] + 0.0722 * s.veilLe[i * 3 + 2];
			acc += std::max( lum, 0.0 ) * veilArea[i] * ( s.veilTwo[i] ? 2.0 : 1.0 );
			veilCdf[i] = acc;
		}
		veilPower = acc;
	}
	// nearest opaque hit; primary: the camera's rule (one-sided back faces culled)
	Hit nearest( const double o[3], const double d[3], bool primary ) const
	{
		Hit h;
		bvh.walk( o, d, 0.01, 1e30, false,
			[&]( uint32_t ti, double u, double w, bool back ) {
				if ( primary && back && !s.two.empty() && !s.two[ti] )
					return false;
				return !s.hole( ti, u, w );
			},
			[&]( uint32_t ti, double t, double u, double w, bool back ) { h.tri = ti; h.t = t; h.b1 = u; h.b2 = w; h.back = back; return true; } );
		return h;
	}
	bool blocked( const double o[3], const double d[3], double tmax ) const
	{
		bool any = false;
		bvh.walk( o, d, 0.01, tmax, true, [&]( uint32_t ti, double u, double w, bool ) { return !s.hole( ti, u, w ); },
			[&]( uint32_t, double, double, double, bool ) { any = true; return true; } );
		return any;
	}
	// the glass tint between o and o + d tEnd (panes closer than 0.01 to each other count once); n: crossings
	void glassT( const double o[3], const double d[3], double tEnd, double T[3], int * n = nullptr ) const
	{
		T[0] = T[1] = T[2] = 1.0;
		if ( s.glass.empty() || redNoGlass )
			return;
		std::vector<std::pair<double, uint32_t>> hits;
		glassBvh.walk( o, d, 0.01, tEnd - 0.01, false, []( uint32_t, double, double, bool ) { return true; },
			[&]( uint32_t ti, double t, double, double, bool ) { hits.push_back( { t, ti } ); return false; } );
		std::sort( hits.begin(), hits.end() );
		double last = -1.0;
		for ( const auto & h : hits ) {
			if ( last >= 0 && h.first - last <= 0.01 )
				continue;
			for ( int c = 0; c < 3; c++ )
				T[c] *= s.glassT[size_t( h.second ) * 3 + size_t( c )];
			last = h.first;
			if ( n )
				( *n )++;
		}
	}
	void triNormal( long long ti, double n[3] ) const
	{
		const float * v = &s.v[size_t( ti ) * 9];
		const double e1[3] = { double( v[3] ) - v[0], double( v[4] ) - v[1], double( v[5] ) - v[2] }, e2[3] = { double( v[6] ) - v[0], double( v[7] ) - v[1], double( v[8] ) - v[2] };
		n[0] = e1[1] * e2[2] - e1[2] * e2[1];
		n[1] = e1[2] * e2[0] - e1[0] * e2[2];
		n[2] = e1[0] * e2[1] - e1[1] * e2[0];
		const double l = std::sqrt( n[0] * n[0] + n[1] * n[1] + n[2] * n[2] );
		for ( int c = 0; c < 3; c++ )
			n[c] = l > 0 ? n[c] / l : 0.0;
	}
	void albedo( long long ti, double a[3] ) const
	{
		for ( int c = 0; c < 3; c++ )
			a[c] = redAlbedo1 ? 1.0 : s.alb[size_t( ti ) * 3 + size_t( c )] / 255.0;
	}
	// the sun's irradiance (store units) at p facing n: pi x sun x n.s through glass, 0 when shadowed
	void sunE( const double p[3], const double n[3], double E[3] ) const
	{
		E[0] = E[1] = E[2] = 0.0;
		if ( !sky.sunOn() )
			return;
		const double ns = n[0] * sky.sunTo[0] + n[1] * sky.sunTo[1] + n[2] * sky.sunTo[2];
		if ( ns <= 0.0 || blocked( p, sky.sunTo, 1e30 ) )
			return;
		double T[3];
		glassT( p, sky.sunTo, 1e30, T );
		for ( int c = 0; c < 3; c++ )
			E[c] = PI * sky.sun[c] * ns * T[c];
	}
	// the placed lamps' irradiance (store units) at p facing n, as the relight lights a surfel
	void lampE( const double p[3], const double n[3], double E[3] ) const
	{
		E[0] = E[1] = E[2] = 0.0;
		for ( const Lamp & l : lamps ) {
			double Lv[3] = { l.pos[0] - p[0], l.pos[1] - p[1], l.pos[2] - p[2] };
			const double d = std::sqrt( Lv[0] * Lv[0] + Lv[1] * Lv[1] + Lv[2] * Lv[2] );
			if ( d >= l.radius || !l.inside( p ) )
				continue;
			const double inv = 1.0 / std::max( d, 0.001 );
			for ( double & c : Lv )
				c *= inv;
			const double nl = n[0] * Lv[0] + n[1] * Lv[1] + n[2] * Lv[2];
			if ( nl <= 0.0 )
				continue;
			double a = l.radial( d );
			if ( l.spot ) {
				const double dl = -( Lv[0] * l.dir[0] + Lv[1] * l.dir[1] + Lv[2] * l.dir[2] );
				const double base = std::min( std::max( 1.0 - ( 1.0 - dl ) / std::max( 1.0 - l.cosOuter, 1e-4 ), 0.0 ), 1.0 );
				a *= std::min( std::pow( base, std::max( l.cone, 1e-3 ) ), 1.0 );
			}
			if ( a * nl <= 0.0 )
				continue;
			if ( d - clear > 0.01 && blocked( p, Lv, d - clear ) )
				continue;
			double T[3];
			glassT( p, Lv, d, T );
			for ( int c = 0; c < 3; c++ )
				E[c] += PI * l.color[c] * a * nl * T[c];
		}
	}
	// one EMV1 next-event sample: the veils' irradiance estimate at p facing n (store units)
	void veilE( const double p[3], const double n[3], Rng & r, double E[3] ) const
	{
		E[0] = E[1] = E[2] = 0.0;
		if ( veilPower <= 0.0 )
			return;
		const double x = r.u() * veilPower;
		const size_t i = std::min( size_t( std::upper_bound( veilCdf.begin(), veilCdf.end(), x ) - veilCdf.begin() ), veilCdf.size() - 1 );
		const double pTri = ( veilCdf[i] - ( i ? veilCdf[i - 1] : 0.0 ) ) / veilPower;
		if ( pTri <= 0.0 )
			return;
		double a = r.u(), b = r.u();
		if ( a + b > 1.0 ) { a = 1.0 - a; b = 1.0 - b; }
		const float * v = &s.veil[i * 9];
		double q[3], w[3];
		for ( int c = 0; c < 3; c++ ) {
			q[c] = v[c] + a * ( double( v[3 + c] ) - v[c] ) + b * ( double( v[6 + c] ) - v[c] );
			w[c] = q[c] - p[c];
		}
		const double d2 = w[0] * w[0] + w[1] * w[1] + w[2] * w[2], d = std::sqrt( d2 );
		if ( d < 1e-3 )
			return;
		for ( double & c : w )
			c /= d;
		const double cosR = n[0] * w[0] + n[1] * w[1] + n[2] * w[2];
		if ( cosR <= 0.0 )
			return;
		const double * en = &veilN[i * 3];
		double cosE = -( en[0] * w[0] + en[1] * w[1] + en[2] * w[2] );   // the emitter's front faces p
		if ( s.veilTwo[i] )
			cosE = std::fabs( cosE );
		if ( cosE <= 0.0 )
			return;
		if ( blocked( p, w, d - 0.01 ) )
			return;
		double T[3];
		glassT( p, w, d, T );
		const double g = cosR * cosE / d2 * veilArea[i] / pTri;
		for ( int c = 0; c < 3; c++ )
			E[c] = s.veilLe[i * 3 + size_t( c )] * g * T[c];
	}
	void add( double * acc, int src, int order, const double v[3], double w ) const
	{
		const int o = redOrder ? 0 : std::min( order, ORDERS - 1 );
		for ( int c = 0; c < 3; c++ )
			acc[( src * ORDERS + o ) * 3 + c] += v[c] * w;
	}
	/* E at (p, n) by source and order: the direct sun and lamps once, everything else by spp cosine paths.
	 * acc: NE doubles (store units). Returns the sky share (the share of first rays that miss). */
	double estimate( const double p[3], const double n[3], int spp, int bounces, Rng & r, double * acc, int * glassN ) const
	{
		const double o0[3] = { p[0] + n[0] * 0.5, p[1] + n[1] * 0.5, p[2] + n[2] * 0.5 };
		double E[3];
		sunE( o0, n, E );
		add( acc, SUN, 0, E, 1.0 );
		lampE( o0, n, E );
		add( acc, LAMP, 0, E, 1.0 );
		const int rows = std::max( 1, int( std::lround( std::sqrt( double( spp ) ) ) ) ), cols = std::max( 1, spp / rows ), M = rows * cols;
		const double w = 1.0 / M;
		int miss = 0;
		for ( int k = 0; k < M; k++ ) {
			if ( veilPower > 0.0 ) {
				veilE( o0, n, r, E );
				add( acc, EMIT, 0, E, w );
			}
			double d[3];
			cosDir( n, ( k / cols + r.u() ) / rows, ( k % cols + r.u() ) / cols, d, redUniform );
			double o[3] = { o0[0], o0[1], o0[2] }, beta[3] = { 1, 1, 1 };
			for ( int depth = 0; depth <= bounces; depth++ ) {
				const Hit h = nearest( o, d, false );
				double T[3];
				glassT( o, d, h.tri < 0 ? 1e30 : h.t, T, depth == 0 ? glassN : nullptr );
				for ( int c = 0; c < 3; c++ )
					beta[c] *= T[c];
				if ( h.tri < 0 ) {   // the sky after `depth` bounces: pi x the mean radiance
					double L[3];
					sky.L( d, L );
					for ( int c = 0; c < 3; c++ )
						L[c] *= beta[c] * PI;
					add( acc, SKY, depth, L, w );
					miss += depth == 0;
					break;
				}
				if ( s.glows( size_t( h.tri ) ) ) {   // EMT1: the surface's own light
					double Le[3];
					s.glowLe( size_t( h.tri ), h.b1, h.b2, Le );
					for ( int c = 0; c < 3; c++ )
						Le[c] *= beta[c] * PI;
					add( acc, EMIT, depth, Le, w );
				}
				if ( depth == bounces )
					break;
				double hn[3];
				triNormal( h.tri, hn );
				if ( hn[0] * d[0] + hn[1] * d[1] + hn[2] * d[2] > 0 )
					for ( double & c : hn )
						c = -c;
				double a[3];
				albedo( h.tri, a );
				for ( int c = 0; c < 3; c++ ) {
					o[c] = o[c] + d[c] * h.t + hn[c] * 0.5;
					beta[c] *= a[c];
				}
				// next event at the hit: its E x albedo / pi is the radiance it sends back; x pi for E here
				sunE( o, hn, E );
				for ( int c = 0; c < 3; c++ )
					E[c] *= beta[c];
				add( acc, SUN, depth + 1, E, w );
				if ( !lamps.empty() ) {
					lampE( o, hn, E );
					for ( int c = 0; c < 3; c++ )
						E[c] *= beta[c];
					add( acc, LAMP, depth + 1, E, w );
				}
				if ( veilPower > 0.0 ) {
					veilE( o, hn, r, E );
					for ( int c = 0; c < 3; c++ )
						E[c] *= beta[c];
					add( acc, EMIT, depth + 1, E, w );
				}
				if ( std::max( beta[0], std::max( beta[1], beta[2] ) ) <= 0.0 )
					break;
				cosDir( hn, r.u(), r.u(), d, redUniform );
			}
		}
		return double( miss ) / M;
	}
};

const char * LAYER[] = { "sun0", "sun1", "sun2", "sun3", "sky0", "sky1", "sky2", "sky3", "lamp0", "lamp1", "lamp2", "lamp3",
	"emit0", "emit1", "emit2", "emit3", "alb", "pos", "nrm", "misc" };
const int NL = 20;

uint64_t pixelSeed( uint64_t seed, uint64_t i )
{
	Rng r( seed * 0x9E3779B97F4A7C15ull ^ ( i + 1 ) * 0xD1B54A32D192ED03ull );
	return r.next();
}

struct Args { std::map<std::string, std::string> kv; bool fixture = false; };

// ---------------------------------------------------------------- analytic fixtures (ww-analytic-fixture-gate)
struct FixRow { std::string name; double got, want, tol; };

void addTri( Soup & s, const double a[3], const double b[3], const double c[3], double alb )
{
	for ( const double * p : { a, b, c } )
		for ( int k = 0; k < 3; k++ )
			s.v.push_back( float( p[k] ) );
	for ( int k = 0; k < 3; k++ )
		s.alb.push_back( uint8_t( std::lround( alb * 255.0 ) ) );
}
// a quad as two triangles, facing (b - a) x (d - a)
void addQuad( Soup & s, const double a[3], const double b[3], const double c[3], const double d[3], double alb )
{
	addTri( s, a, b, c, alb );
	addTri( s, a, c, d, alb );
}

int runFixtures( const Args & A )
{
	const int spp = A.kv.count( "spp" ) ? std::atoi( A.kv.at( "spp" ).c_str() ) : 4096;
	const std::string red = A.kv.count( "red" ) ? A.kv.at( "red" ) : "";
	std::vector<FixRow> rows;
	auto mk = [&]( Scene & sc ) {
		sc.red = red;
		sc.redAlbedo1 = red == "albedo1";
		sc.redUniform = red == "uniform";
		sc.redNoGlass = red == "noglass";
		sc.redOrder = red == "order";
		sc.sky.flat = red == "flatsky";
		sc.prepare();
	};
	auto lum = []( const double * v ) { return v[0]; };
	auto sum = [&]( const double * acc, int src, int o ) { return lum( &acc[( src * ORDERS + o ) * 3] ); };
	auto run = [&]( const Scene & sc, const double p[3], const double n[3], int bounces, double * acc, double * share ) {
		std::fill( acc, acc + NE, 0.0 );
		Rng r( 12345 );
		int g = 0;
		const double sh = sc.estimate( p, n, spp, bounces, r, acc, &g );
		if ( share )
			*share = sh;
	};
	// MC tolerance: 4 sigma of a mean of spp samples of a quantity bounded by `range`, floor 0.3 %
	auto tol = [&]( double want, double range ) { return std::max( 0.003 * std::fabs( want ), 4.0 * range / std::sqrt( double( spp ) ) * 0.5 ); };
	double acc[NE];
	{	// F1 open sky, no geometry: E(+z) = pi/2 amb+z + pi/8 (amb+x + amb-x + amb+y + amb-y); E(+x) likewise
		Scene sc;
		const double a[6] = { 0.30, 0.10, 0.20, 0.05, 1.00, 0.02 };
		for ( int i = 0; i < 6; i++ )
			for ( int c = 0; c < 3; c++ )
				sc.sky.amb[i][c] = a[i];
		sc.s.alb.push_back( 0 );   // an empty soup still reads as one with albedo
		mk( sc );
		const double p[3] = { 0, 0, 0 }, nz[3] = { 0, 0, 1 }, nx[3] = { 1, 0, 0 };
		double share;
		run( sc, p, nz, 2, acc, &share );
		const double wz = PI / 2 * a[4] + PI / 8 * ( a[0] + a[1] + a[2] + a[3] );
		rows.push_back( { "F1 open sky E(+z) = pi/2 Az + pi/8 (Ax+Ax-Ay+Ay-)", sum( acc, SKY, 0 ), wz, 0.005 * wz } );
		rows.push_back( { "F1 open sky share = 1", share, 1.0, 1e-9 } );
		run( sc, p, nx, 2, acc, nullptr );
		const double wx = PI / 2 * a[0] + PI / 8 * ( a[2] + a[3] + a[4] + a[5] );
		rows.push_back( { "F1 open sky E(+x) = pi/2 Ax+ + pi/8 (Ay+Ay-Az+Az-)", sum( acc, SKY, 0 ), wx, 0.005 * wx } );
	}
	{	// F2 the furnace: a closed cube, every wall albedo a and glowing Le (EMT1); orders k = pi Le a^k exactly
		Scene sc;
		const double a = 0.6, Le = 0.5, h = 100.0;
		const double c[8][3] = { { -h, -h, -h }, { h, -h, -h }, { h, h, -h }, { -h, h, -h }, { -h, -h, h }, { h, -h, h }, { h, h, h }, { -h, h, h } };
		const int q[6][4] = { { 0, 1, 2, 3 }, { 4, 7, 6, 5 }, { 0, 4, 5, 1 }, { 1, 5, 6, 2 }, { 2, 6, 7, 3 }, { 3, 7, 4, 0 } };
		for ( auto & f : q )
			addQuad( sc.s, c[f[0]], c[f[1]], c[f[2]], c[f[3]], a );
		Soup::GEm em;
		em.e[0] = em.e[1] = em.e[2] = float( std::sqrt( Le ) );
		sc.s.gems.push_back( em );
		sc.s.glowOf.assign( sc.s.n(), -1 );
		for ( size_t i = 0; i < sc.s.n(); i++ ) {
			Soup::GTri t;
			t.em = 0;
			std::fill( t.uv, t.uv + 6, 0.0f );
			sc.s.glowOf[i] = int( sc.s.gtris.size() );
			sc.s.gtris.push_back( t );
		}
		mk( sc );
		const double p[3] = { 13, -7, 21 }, n[3] = { 0.36, 0.48, 0.8 };
		run( sc, p, n, 3, acc, nullptr );
		const double al = std::lround( a * 255.0 ) / 255.0;   // the albedo as the soup stores it
		for ( int k = 0; k < 3; k++ ) {
			const double want = PI * Le * std::pow( al, k );
			// every path meets a wall of the same Le and albedo: no sampling noise, float rounding only
			rows.push_back( { "F2 furnace emit order " + std::to_string( k ) + " = pi Le a^k", sum( acc, EMIT, k ), want, 1e-5 * want } );
		}
		rows.push_back( { "F2 furnace sky = 0 (closed)", sum( acc, SKY, 0 ) + sum( acc, SKY, 1 ) + sum( acc, SKY, 2 ) + sum( acc, SKY, 3 ), 0.0, 1e-12 } );
	}
	{	// F3 sun over a wide floor (albedo a), sky black: up-facing E = pi sun cos; down-facing 1 above it = pi a sun cos x F
		Scene sc;
		const double a = 0.5, H = 1e5;
		const double c0[3] = { -H, -H, 0 }, c1[3] = { H, -H, 0 }, c2[3] = { H, H, 0 }, c3[3] = { -H, H, 0 };
		addQuad( sc.s, c0, c1, c2, c3, a );
		const double sl = std::sqrt( 0.3 * 0.3 + 0.4 * 0.4 + 1.0 );
		sc.sky.sunTo[0] = 0.3 / sl; sc.sky.sunTo[1] = 0.4 / sl; sc.sky.sunTo[2] = 1.0 / sl;
		sc.sky.sun[0] = sc.sky.sun[1] = sc.sky.sun[2] = 2.0;
		mk( sc );
		const double up[3] = { 0, 0, 1 }, dn[3] = { 0, 0, -1 }, p0[3] = { 0, 0, 0 }, p1[3] = { 0, 0, 10 };
		run( sc, p0, up, 3, acc, nullptr );
		rows.push_back( { "F3 sun direct on the floor = pi sun cos", sum( acc, SUN, 0 ), PI * 2.0 * sc.sky.sunTo[2], 1e-9 } );
		run( sc, p1, dn, 3, acc, nullptr );
		const double al = std::lround( a * 255.0 ) / 255.0;
		// a down-facing point 10.5 above a 1e5-half square floor: view factor 1 - O(1e-8)
		const double want = PI * al * 2.0 * sc.sky.sunTo[2];
		rows.push_back( { "F3 sun bounce 1 under the floor-lit = pi a sun cos", sum( acc, SUN, 1 ), want, tol( want, 0.0 ) + 1e-6 } );
		rows.push_back( { "F3 sun bounce 2 = 0 (a plane sees no plane)", sum( acc, SUN, 2 ) + sum( acc, SUN, 3 ), 0.0, 1e-12 } );
	}
	{	// F4 a lamp over the floor: E = pi colour radial(d) cos at the floor, and the shadow of a blocker
		Scene sc;
		const double H = 1e4;
		const double c0[3] = { -H, -H, 0 }, c1[3] = { H, -H, 0 }, c2[3] = { H, H, 0 }, c3[3] = { -H, H, 0 };
		addQuad( sc.s, c0, c1, c2, c3, 0.5 );
		// a small blocker between the lamp and the point (200, 0, 0): a square at z 150 around x 125
		const double b0[3] = { 85, -15, 150 }, b1[3] = { 115, -15, 150 }, b2[3] = { 115, 15, 150 }, b3[3] = { 85, 15, 150 };
		addQuad( sc.s, b0, b1, b2, b3, 0.5 );
		Lamp l;
		l.pos[0] = 0; l.pos[1] = 0; l.pos[2] = 300;
		l.radius = 600;
		l.color[0] = l.color[1] = l.color[2] = 1.5;
		sc.lamps.push_back( l );
		mk( sc );
		const double up[3] = { 0, 0, 1 }, p0[3] = { -100, 50, 0 }, p1[3] = { 200, 0, 0 };
		run( sc, p0, up, 1, acc, nullptr );
		const double d = std::sqrt( 100.0 * 100 + 50 * 50 + 299.5 * 299.5 ), x = d / 600.0;
		const double want = PI * 1.5 * std::pow( 1.0 - x * x, 2.2 ) * 299.5 / d;
		rows.push_back( { "F4 lamp direct = pi c (1-(d/r)^2)^2.2 cos", sum( acc, LAMP, 0 ), want, 1e-9 * want } );
		run( sc, p1, up, 1, acc, nullptr );
		rows.push_back( { "F4 lamp shadowed by the blocker = 0", sum( acc, LAMP, 0 ), 0.0, 1e-12 } );
	}
	{	// F5 a one-sided square neon (EMV1) of side 2s at height h over an up-facing point: E = Le x 4 x F(s/h)
		Scene sc;
		sc.s.alb.push_back( 0 );
		const double s = 20.0, hgt = 100.0, Le = 3.0;
		// facing down: (b - a) x (c - a) = -z
		const double q0[3] = { -s, -s, hgt }, q1[3] = { -s, s, hgt }, q2[3] = { s, s, hgt }, q3[3] = { s, -s, hgt };
		auto veil = [&]( const double * a, const double * b, const double * c ) {
			for ( const double * p : { a, b, c } )
				for ( int k = 0; k < 3; k++ )
					sc.s.veil.push_back( float( p[k] ) );
			for ( int k = 0; k < 3; k++ )
				sc.s.veilLe.push_back( float( Le ) );
			sc.s.veilTwo.push_back( 0 );
		};
		veil( q0, q1, q2 );
		veil( q0, q2, q3 );
		mk( sc );
		const double p[3] = { 0, 0, -0.5 }, up[3] = { 0, 0, 1 }, dn[3] = { 0, 0, -1 };
		run( sc, p, up, 1, acc, nullptr );
		const double hh = hgt + 0.0, X = s / hh;   // the point sits 0.5 below 0 and is offset 0.5 up: h = hgt exactly
		const double F = 4.0 / ( 2.0 * PI ) * ( 2.0 * X / std::sqrt( 1 + X * X ) * std::atan( X / std::sqrt( 1 + X * X ) ) );
		const double want = PI * Le * F;
		rows.push_back( { "F5 neon square (one-sided) = pi Le F_parallel", sum( acc, EMIT, 0 ), want, 0.01 * want } );
		const double p2[3] = { 0, 0, 2 * hgt + 0.5 };
		run( sc, p2, dn, 1, acc, nullptr );
		rows.push_back( { "F5 neon back face (one-sided) = 0", sum( acc, EMIT, 0 ), 0.0, 1e-12 } );
	}
	{	// F6 glass: a huge pane of tint 0.5 over open sky halves the up-facing sky (the pane's own edges far away)
		Scene sc;
		sc.s.alb.push_back( 0 );
		for ( int i = 0; i < 6; i++ )
			for ( int c = 0; c < 3; c++ )
				sc.sky.amb[i][c] = 0.4;
		const double H = 1e7, z = 10;
		const double g[4][3] = { { -H, -H, z }, { H, -H, z }, { H, H, z }, { -H, H, z } };
		const int tr[2][3] = { { 0, 1, 2 }, { 0, 2, 3 } };
		for ( auto & t : tr ) {
			for ( int k = 0; k < 3; k++ )
				for ( int c = 0; c < 3; c++ )
					sc.s.glass.push_back( float( g[t[k]][c] ) );
			for ( int c = 0; c < 3; c++ )
				sc.s.glassT.push_back( 128 / 255.0 );
		}
		mk( sc );
		const double p[3] = { 0, 0, 0 }, up[3] = { 0, 0, 1 };
		run( sc, p, up, 1, acc, nullptr );
		const double want = PI * 0.4 * ( 128 / 255.0 );
		rows.push_back( { "F6 glass pane x0.5 over a uniform sky = pi L t", sum( acc, SKY, 0 ), want, 1e-6 } );
	}
	{	// F7 the camera law: a point projected by notes/gishade1/proj.py's formula is the pixel whose ray hits it
		Scene sc;
		const double e[3] = { 100, -50, 30 }, t[3] = { 900, 400, 120 };
		const double fov = 55;
		const int W = 1280, Hh = 685;
		double f[3] = { t[0] - e[0], t[1] - e[1], t[2] - e[2] };
		const double fl = std::sqrt( f[0] * f[0] + f[1] * f[1] + f[2] * f[2] );
		for ( double & c : f ) c /= fl;
		double r[3] = { f[1], -f[0], 0 };
		const double rl = std::sqrt( r[0] * r[0] + r[1] * r[1] );
		for ( double & c : r ) c /= rl;
		const double u[3] = { r[1] * f[2] - r[2] * f[1], r[2] * f[0] - r[0] * f[2], r[0] * f[1] - r[1] * f[0] };
		const double sS = ( Hh / 2.0 ) / std::tan( fov * PI / 360.0 );
		// a world point: pixel (321.5, 111.5) at depth 700 along the camera law, then the tool's own ray for (321, 111)
		const double px = 321.5, py = 111.5, z = 700;
		double P[3];
		for ( int c = 0; c < 3; c++ )
			P[c] = e[c] + z * f[c] + ( px - W / 2.0 ) / sS * z * r[c] - ( py - Hh / 2.0 ) / sS * z * u[c];
		// proj.py: x = d.r, y = d.u, z = d.f -> px = W/2 + s x/z, py = H/2 - s y/z
		const double dv[3] = { P[0] - e[0], P[1] - e[1], P[2] - e[2] };
		const double X = dv[0] * r[0] + dv[1] * r[1] + dv[2] * r[2], Y = dv[0] * u[0] + dv[1] * u[1] + dv[2] * u[2], Z = dv[0] * f[0] + dv[1] * f[1] + dv[2] * f[2];
		rows.push_back( { "F7 camera: proj.py px of the pixel-centre point", W / 2.0 + sS * X / Z, px, 1e-6 } );
		rows.push_back( { "F7 camera: proj.py py of the pixel-centre point", Hh / 2.0 - sS * Y / Z, py, 1e-6 } );
		(void)sc;
	}
	int fails = 0;
	std::printf( "path_reference fixtures (spp %d%s%s)\n", spp, red.empty() ? "" : ", RED ", red.c_str() );
	for ( const FixRow & r : rows ) {
		const bool ok = std::fabs( r.got - r.want ) <= r.tol;
		fails += !ok;
		std::printf( "  %-4s %-58s got %.6f want %.6f tol %.2g\n", ok ? "PASS" : "FAIL", r.name.c_str(), r.got, r.want, r.tol );
	}
	std::printf( "%s: %d of %zu rows pass\n", fails ? "FIXTURES FAIL" : "FIXTURES PASS", int( rows.size() ) - fails, rows.size() );
	return fails ? 1 : 0;
}

}   // namespace

int main( int argc, char ** argv )
{
	Args A;
	for ( int i = 1; i < argc; i++ ) {
		const std::string a = argv[i];
		const size_t eq = a.find( '=' );
		if ( a == "fixture" )
			A.fixture = true;
		else if ( eq != std::string::npos )
			A.kv[a.substr( 0, eq )] = a.substr( eq + 1 );
	}
	if ( A.fixture )
		return runFixtures( A );
	auto need = [&]( const char * k ) { return A.kv.count( k ) ? A.kv[k].c_str() : nullptr; };
	if ( !need( "soup" ) || !need( "sky" ) || !need( "out" ) || ( !need( "cam" ) && !need( "gbuf" ) ) ) {
		std::fprintf( stderr, "usage: path_reference soup=<psp> sky=<gi_sky.txt> [lights=<gi_lights.txt>] out=<prefix> "
			"(cam=\"ex ey ez tx ty tz fov\" size=WxH | gbuf=<file.gbf>) [spp=256] [bounces=4] [seed=1] [threads=0] [red=...]\n"
			"       path_reference fixture [spp=4096] [red=albedo1|uniform|noglass|order|flatsky]\n" );
		return 2;
	}
	const auto t0 = std::chrono::steady_clock::now();
	auto secs = [&]() { return std::chrono::duration<double>( std::chrono::steady_clock::now() - t0 ).count(); };
	Scene sc;
	std::string tails;
	if ( !readSoup( need( "soup" ), sc.s, tails ) ) {
		std::fprintf( stderr, "soup %s: unreadable or no ALB1 albedo\n", need( "soup" ) );
		return 1;
	}
	if ( !readSky( need( "sky" ), sc.sky ) ) {
		std::fprintf( stderr, "sky %s: unreadable (six amb lines needed)\n", need( "sky" ) );
		return 1;
	}
	if ( need( "lights" ) && !readLamps( need( "lights" ), sc.lamps, sc.clear ) ) {
		std::fprintf( stderr, "lights %s: unreadable\n", need( "lights" ) );
		return 1;
	}
	sc.red = A.kv.count( "red" ) ? A.kv["red"] : "";
	sc.redAlbedo1 = sc.red == "albedo1";
	sc.redUniform = sc.red == "uniform";
	sc.redNoGlass = sc.red == "noglass";
	sc.redOrder = sc.red == "order";
	sc.sky.flat = sc.red == "flatsky";
	const double tRead = secs();
	sc.prepare();
	const double tBuild = secs() - tRead;
	const int spp = A.kv.count( "spp" ) ? std::max( 1, std::atoi( A.kv["spp"].c_str() ) ) : 256;
	const int bounces = A.kv.count( "bounces" ) ? std::max( 0, std::atoi( A.kv["bounces"].c_str() ) ) : 4;
	const uint64_t seed = A.kv.count( "seed" ) ? std::strtoull( A.kv["seed"].c_str(), nullptr, 10 ) : 1;
	unsigned nth = A.kv.count( "threads" ) ? unsigned( std::atoi( A.kv["threads"].c_str() ) ) : 0;
	if ( !nth )
		nth = std::max( 1u, std::thread::hardware_concurrency() );
	int W = 0, H = 0;
	std::vector<float> gb;   // gbuf: 7 a pixel
	double cam[7] = {};
	if ( need( "gbuf" ) ) {
		FILE * f = std::fopen( need( "gbuf" ), "rb" );
		uint32_t h[3] = { 0, 0, 0 };
		if ( !f || std::fread( h, 4, 3, f ) != 3 || h[0] != 0x31464247u ) {
			std::fprintf( stderr, "gbuf %s: unreadable\n", need( "gbuf" ) );
			return 1;
		}
		W = int( h[1] );
		H = int( h[2] );
		gb.resize( size_t( W ) * size_t( H ) * 7 );
		if ( std::fread( gb.data(), 4, gb.size(), f ) != gb.size() ) {
			std::fprintf( stderr, "gbuf %s: short\n", need( "gbuf" ) );
			return 1;
		}
		std::fclose( f );
	} else {
		if ( std::sscanf( need( "cam" ), "%lf %lf %lf %lf %lf %lf %lf", cam, cam + 1, cam + 2, cam + 3, cam + 4, cam + 5, cam + 6 ) != 7
			|| !need( "size" ) || std::sscanf( need( "size" ), "%dx%d", &W, &H ) != 2 || W <= 0 || H <= 0 ) {
			std::fprintf( stderr, "cam needs 7 numbers and size=WxH\n" );
			return 2;
		}
	}
	// the camera basis (notes/gishade1/proj.py)
	double f[3] = { cam[3] - cam[0], cam[4] - cam[1], cam[5] - cam[2] }, r[3], u[3];
	{
		const double fl = std::sqrt( f[0] * f[0] + f[1] * f[1] + f[2] * f[2] );
		for ( double & c : f ) c = fl > 0 ? c / fl : 0;
		r[0] = f[1]; r[1] = -f[0]; r[2] = 0;
		const double rl = std::sqrt( r[0] * r[0] + r[1] * r[1] );
		for ( double & c : r ) c = rl > 0 ? c / rl : 0;
		u[0] = r[1] * f[2] - r[2] * f[1]; u[1] = r[2] * f[0] - r[0] * f[2]; u[2] = r[0] * f[1] - r[1] * f[0];
	}
	const double sS = ( H / 2.0 ) / std::tan( cam[6] * PI / 360.0 );
	const size_t NP = size_t( W ) * size_t( H );
	std::vector<float> out( size_t( NL ) * NP * 3, 0.0f );
	std::atomic<size_t> next( 0 ), done( 0 );
	std::atomic<long long> glassSum( 0 );
	const size_t CH = 64;
	auto work = [&]() {
		std::vector<double> acc( NE );
		for ( ;; ) {
			const size_t b = next.fetch_add( CH );
			if ( b >= NP )
				return;
			for ( size_t i = b; i < std::min( NP, b + CH ); i++ ) {
				const size_t px = i % size_t( W ), py = i / size_t( W );
				double P[3], N[3], alb[3] = { 0, 0, 0 };
				bool valid = false;
				if ( !gb.empty() ) {
					const float * g = &gb[i * 7];
					valid = g[6] > 0.5f;
					for ( int c = 0; c < 3; c++ ) { P[c] = g[c]; N[c] = g[3 + c]; }
					const double nl = std::sqrt( N[0] * N[0] + N[1] * N[1] + N[2] * N[2] );
					valid = valid && nl > 0.1;
					for ( double & c : N ) c = nl > 0 ? c / nl : 0;
				} else {
					const double x = ( double( px ) + 0.5 - W / 2.0 ) / sS, y = ( double( py ) + 0.5 - H / 2.0 ) / sS;
					double d[3];
					for ( int c = 0; c < 3; c++ )
						d[c] = f[c] + x * r[c] - y * u[c];
					const double dl = std::sqrt( d[0] * d[0] + d[1] * d[1] + d[2] * d[2] );
					for ( double & c : d ) c /= dl;
					const Hit h = sc.nearest( cam, d, true );
					if ( h.tri >= 0 ) {
						valid = true;
						for ( int c = 0; c < 3; c++ )
							P[c] = cam[c] + d[c] * h.t;
						sc.triNormal( h.tri, N );
						if ( !sc.s.vn.empty() ) {   // VNM1: the smooth normal, when the triangle has one
							const int16_t * q = &sc.s.vn[size_t( h.tri ) * 9];
							const double b0 = 1.0 - h.b1 - h.b2;
							double vn[3];
							for ( int c = 0; c < 3; c++ )
								vn[c] = ( b0 * q[c] + h.b1 * q[3 + c] + h.b2 * q[6 + c] ) / 32767.0;
							const double vl = std::sqrt( vn[0] * vn[0] + vn[1] * vn[1] + vn[2] * vn[2] );
							if ( vl > 0.5 && vn[0] * N[0] + vn[1] * N[1] + vn[2] * N[2] > 0.0 )
								for ( int c = 0; c < 3; c++ )
									N[c] = vn[c] / vl;
						}
						if ( N[0] * d[0] + N[1] * d[1] + N[2] * d[2] > 0 )
							for ( double & c : N )
								c = -c;
						sc.albedo( h.tri, alb );
					}
				}
				const size_t row = py * size_t( W ) + px;   // row 0 at the top in both modes
				auto put = [&]( int layer, const double * v ) {
					for ( int c = 0; c < 3; c++ )
						out[( size_t( layer ) * NP + row ) * 3 + size_t( c )] = float( v[c] );
				};
				if ( !valid )
					continue;
				std::fill( acc.begin(), acc.end(), 0.0 );
				Rng rng( pixelSeed( seed, i ) );
				int gn = 0;
				const double share = sc.estimate( P, N, spp, bounces, rng, acc.data(), &gn );
				glassSum += gn;
				for ( int l = 0; l < NSRC * ORDERS; l++ )
					put( l, &acc[size_t( l ) * 3] );
				put( 16, alb );
				put( 17, P );
				put( 18, N );
				const double misc[3] = { share, 1.0, double( gn ) / std::max( 1, spp ) };
				put( 19, misc );
			}
			const size_t d = done.fetch_add( CH ) + CH;
			if ( ( d / CH ) % std::max<size_t>( 1, NP / CH / 20 ) == 0 )
				std::fprintf( stderr, "  %3.0f%% %.0f s\n", 100.0 * double( std::min( d, NP ) ) / double( NP ), secs() );
		}
	};
	std::vector<std::thread> th;
	for ( unsigned k = 0; k < nth; k++ )
		th.emplace_back( work );
	for ( auto & t : th )
		t.join();
	const double tAll = secs();
	const std::string pre = need( "out" );
	FILE * o = std::fopen( ( pre + ".ptr" ).c_str(), "wb" );
	if ( !o )
		return 1;
	const uint32_t hd[4] = { 0x31525450u, uint32_t( W ), uint32_t( H ), uint32_t( NL ) };
	std::fwrite( hd, 4, 4, o );
	for ( int l = 0; l < NL; l++ ) {
		char nm[16] = {};
		std::strncpy( nm, LAYER[l], 15 );
		std::fwrite( nm, 1, 16, o );
	}
	std::fwrite( out.data(), 4, out.size(), o );
	std::fclose( o );
	FILE * t = std::fopen( ( pre + ".txt" ).c_str(), "w" );
	if ( t ) {
		std::fprintf( t, "path_reference %s\n", gb.empty() ? "camera" : "gbuf" );
		std::fprintf( t, "soup %s (tris %zu; tails %s; glass %zu, veils %zu, glowing %zu, masked %zu)\n", need( "soup" ), sc.s.n(), tails.c_str(),
			sc.s.glass.size() / 9, sc.s.veil.size() / 9, sc.s.gtris.size(), sc.s.masked.size() );
		std::fprintf( t, "sky %s sunTo %.5f %.5f %.5f sun %.5f %.5f %.5f\n", need( "sky" ), sc.sky.sunTo[0], sc.sky.sunTo[1], sc.sky.sunTo[2],
			sc.sky.sun[0], sc.sky.sun[1], sc.sky.sun[2] );
		std::fprintf( t, "lights %s (%zu lamps, clear %.1f)\n", need( "lights" ) ? need( "lights" ) : "-", sc.lamps.size(), sc.clear );
		if ( gb.empty() )
			std::fprintf( t, "cam %s size %dx%d\n", need( "cam" ), W, H );
		else
			std::fprintf( t, "gbuf %s size %dx%d\n", need( "gbuf" ), W, H );
		std::fprintf( t, "spp %d bounces %d seed %llu threads %u red %s\n", spp, bounces, (unsigned long long)seed, nth, sc.red.empty() ? "-" : sc.red.c_str() );
		std::fprintf( t, "seconds read %.1f build %.1f trace %.1f total %.1f\n", tRead, tBuild, tAll - tRead - tBuild, tAll );
		std::fclose( t );
	}
	std::fprintf( stderr, "path_reference done: %dx%d spp %d bounces %d in %.1f s (read %.1f, BVH %.1f)\n", W, H, spp, bounces, tAll, tRead, tBuild );
	return 0;
}
