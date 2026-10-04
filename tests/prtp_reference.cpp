// prtp_reference -- the brute-force reference for the PRTP probe bake (2026-10-01).
//
// Shares NO code with src/probebake.cpp: no BVH (every ray is tested against
// every triangle), no Fibonacci set (jittered equal-area strata from a fixed
// seed), its own Moller-Trumbore. tests/spells/prtp_reference.py compares what
// this sees from each probe against what the probe's .tbk links reconstruct.
//
//   prtp_reference <soup.psp> <probes.txt> <rays> <sx> <sy> <sz> <out.tsv>
//
// probes.txt: one "x y z" per line. The radiance field is a test field, not
// light: a hit surface gives albedo * (0.25 + 0.75 * max(0, n . s)), n turned
// toward the probe; a miss (sky) gives 0, as in FO4CS's relight. Per probe and
// octant (bit0 x<0, bit1 y<0, bit2 z<0) the out line holds the sky share, the
// surface share and the summed radiance (r g b), each as a fraction of the
// FULL sphere; then the grey field's 9 real SH coefficients (bands 0-2).
//
// Lane BAKE4: GLASS. The soup's optional GLS1 tail (panes + an rgb transmittance
// each) never stops a ray; every pane a ray crosses before its hit (or, for sky,
// at all) multiplies what it carries, per channel. A crossing is counted strictly
// between 0.01 and the hit's t - 0.01, and one within 0.01 of the crossing before
// it is that pane's twin face (counted once). The radiance columns carry it; 24 more
// columns follow the SH: the sky share seen through the glass, per octant, r g b.
//
// Lane ALPHATEST1: the soup's optional AMK1 tail (alpha-test masks). A hit on a
// masked triangle whose texel (nearest, wrapped uv, at the hit's own barycentrics)
// is under the threshold is no hit: the ray goes on, as in the game's alpha test.
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <thread>
#include <vector>

namespace {

struct Soup
{
	std::vector<float> ox, oy, oz, ax, ay, az, bx, by, bz;   // v0, e1 = v1-v0, e2 = v2-v0
	std::vector<float> nx, ny, nz;                         // unit geometric normal
	std::vector<float> alb;                                // linear, 3 per triangle
	std::vector<double> glass;                             // panes: 9 per pane
	std::vector<double> glassT;                            // 3 per pane, 0..1
	// lane ALPHATEST1: masks
	struct Map { int w = 0, h = 0; std::vector<uint8_t> a; };
	struct Masked { int map = -1; uint32_t thr = 0; float uv[6]; };
	std::vector<Map> maps;
	std::vector<int> maskOf;                               // per triangle, into `masked`; -1 = solid
	std::vector<Masked> masked;
	bool hole( size_t tri, float b1, float b2 ) const
	{
		if ( maskOf.empty() || maskOf[tri] < 0 )
			return false;
		const Masked & m = masked[size_t( maskOf[tri] )];
		const Map & mp = maps[size_t( m.map )];
		const double b0 = 1.0 - double( b1 ) - b2;
		double u = b0 * m.uv[0] + b1 * double( m.uv[2] ) + b2 * double( m.uv[4] );
		double v = b0 * m.uv[1] + b1 * double( m.uv[3] ) + b2 * double( m.uv[5] );
		u -= std::floor( u );
		v -= std::floor( v );
		const int x = std::min( std::max( int( u * mp.w ), 0 ), mp.w - 1 );
		const int y = std::min( std::max( int( v * mp.h ), 0 ), mp.h - 1 );
		return mp.a[size_t( y ) * size_t( mp.w ) + size_t( x )] < m.thr;
	}
	std::vector<int16_t> vn;                               // lane SMOOTHN1: 9 snorm16 a triangle (empty: none)
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
	std::vector<float> v( n * 9 );
	if ( std::fread( v.data(), 4, v.size(), f ) != v.size() ) {
		std::fclose( f );
		return false;
	}
	std::fseek( f, long( head[2] ) * 28, SEEK_CUR );   // doors: ref + lo + hi
	uint32_t tail[2] = { 0, 0 };
	std::vector<uint8_t> a;
	if ( std::fread( tail, 4, 2, f ) == 2 && tail[0] == 0x31424C41u && tail[1] == n ) {
		a.resize( n * 3 );
		if ( std::fread( a.data(), 1, a.size(), f ) != a.size() )
			a.clear();
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
	if ( more && tail[0] == 0x314F5754u ) {   // 'TWO1' (lane ROOMCLAMP1): one byte a triangle, not used here
		std::fseek( f, long( tail[1] ), SEEK_CUR );
		more = std::fread( tail, 4, 2, f ) == 2;
	}
	/* lane LAND5: every tail after TWO1 is sized (magic, count, u32 body bytes), order AMK1, EMT1, VNM1, DRG1;
	 * a tail this reader does not use (EMT1, DRG1, any newer one) is skipped by its byte count */
	while ( more ) {
		uint32_t bytes = 0;
		if ( std::fread( &bytes, 4, 1, f ) != 1 )
			break;
		const long long body = _ftelli64( f );
		if ( tail[0] == 0x314B4D41u ) {   // 'AMK1' (lane ALPHATEST1): its own reader of the masks
			bool ok = true;
			auto u32 = [&]() { uint32_t x = 0; ok = ok && std::fread( &x, 4, 1, f ) == 1; return x; };
			const uint32_t nm = u32();
			for ( uint32_t m = 0; ok && m < nm; m++ ) {
				std::fseek( f, long( u32() ), SEEK_CUR );   // the name
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
				std::fseek( f, long( u32() ), SEEK_CUR );
			s.maskOf.assign( n, -1 );
			for ( uint32_t i = 0; ok && i < tail[1]; i++ ) {
				const uint32_t tri = u32(), map = u32();
				u32();   // model
				Soup::Masked mk;
				mk.thr = u32();
				mk.map = int( map );
				ok = ok && std::fread( mk.uv, 4, 6, f ) == 6 && tri < n && map < nm;
				if ( !ok )
					break;
				s.maskOf[tri] = int( s.masked.size() );
				s.masked.push_back( mk );
			}
			if ( !ok || _ftelli64( f ) != body + bytes ) {
				std::fprintf( stderr, "soup %s: AMK1 tail unreadable\n", path );
				std::fclose( f );
				return false;
			}
		}
		// lane SMOOTHN1: 'VNM1', the vertex normals (9 snorm16 a triangle): the field's normal is their blend
		if ( tail[0] == 0x314D4E56u && tail[1] == n && bytes == n * 18 ) {
			s.vn.resize( n * 9 );
			if ( std::fread( s.vn.data(), 2, s.vn.size(), f ) != s.vn.size() )
				s.vn.clear();
		}
		more = _fseeki64( f, body + bytes, SEEK_SET ) == 0 && std::fread( tail, 4, 2, f ) == 2;
	}
	std::fclose( f );
	if ( a.empty() )
		return false;   // the test field needs the albedo the bake used
	auto res = [&]( std::vector<float> & x ) { x.resize( n ); };
	for ( auto * x : { &s.ox, &s.oy, &s.oz, &s.ax, &s.ay, &s.az, &s.bx, &s.by, &s.bz, &s.nx, &s.ny, &s.nz } )
		res( *x );
	s.alb.resize( n * 3 );
	for ( size_t i = 0; i < n; i++ ) {
		const float * t = &v[i * 9];
		s.ox[i] = t[0]; s.oy[i] = t[1]; s.oz[i] = t[2];
		s.ax[i] = t[3] - t[0]; s.ay[i] = t[4] - t[1]; s.az[i] = t[5] - t[2];
		s.bx[i] = t[6] - t[0]; s.by[i] = t[7] - t[1]; s.bz[i] = t[8] - t[2];
		double cx = double( s.ay[i] ) * s.bz[i] - double( s.az[i] ) * s.by[i];
		double cy = double( s.az[i] ) * s.bx[i] - double( s.ax[i] ) * s.bz[i];
		double cz = double( s.ax[i] ) * s.by[i] - double( s.ay[i] ) * s.bx[i];
		const double l = std::sqrt( cx * cx + cy * cy + cz * cz );
		if ( l > 0 ) {
			cx /= l; cy /= l; cz /= l;
		}
		s.nx[i] = float( cx ); s.ny[i] = float( cy ); s.nz[i] = float( cz );
		for ( int k = 0; k < 3; k++ )
			s.alb[i * 3 + k] = a[i * 3 + k] / 255.0f;
	}
	return true;
}

// Jittered equal-area strata: rows in z = cos(theta), columns in phi.
void directions( int rays, std::vector<float> & dx, std::vector<float> & dy, std::vector<float> & dz )
{
	int rows = std::max( 1, int( std::lround( std::sqrt( rays / 2.0 ) ) ) );
	int cols = std::max( 1, rays / rows );
	uint64_t st = 0x9E3779B97F4A7C15ull;
	auto rnd = [&]() {
		st ^= st << 13; st ^= st >> 7; st ^= st << 17;
		return double( st >> 11 ) / double( 1ull << 53 );
	};
	for ( int r = 0; r < rows; r++ )
		for ( int c = 0; c < cols; c++ ) {
			const double z = 1.0 - 2.0 * ( r + rnd() ) / rows;
			const double ph = 2.0 * 3.14159265358979323846 * ( c + rnd() ) / cols;
			const double s = std::sqrt( std::max( 0.0, 1.0 - z * z ) );
			dx.push_back( float( s * std::cos( ph ) ) );
			dy.push_back( float( s * std::sin( ph ) ) );
			dz.push_back( float( z ) );
		}
}

// What the glass between the probe and tEnd lets through: every pane hit tested
// (double precision, its own ray-plane-barycentric test), sorted, twins merged.
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
		// barycentrics of the plane point
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
	if ( argc != 8 ) {
		std::fprintf( stderr, "usage: prtp_reference <soup.psp> <probes.txt> <rays> <sx> <sy> <sz> <out.tsv>\n" );
		return 2;
	}
	Soup s;
	if ( !readSoup( argv[1], s ) ) {
		std::fprintf( stderr, "soup %s: unreadable or no ALB1 albedo\n", argv[1] );
		return 1;
	}
	std::vector<float> P;
	if ( FILE * f = std::fopen( argv[2], "r" ) ) {
		float x, y, z;
		while ( std::fscanf( f, "%f %f %f", &x, &y, &z ) == 3 ) {
			P.push_back( x ); P.push_back( y ); P.push_back( z );
		}
		std::fclose( f );
	}
	double sx = std::atof( argv[4] ), sy = std::atof( argv[5] ), sz = std::atof( argv[6] );
	const double sl = std::sqrt( sx * sx + sy * sy + sz * sz );
	sx /= sl; sy /= sl; sz /= sl;
	std::vector<float> dx, dy, dz;
	directions( std::atoi( argv[3] ), dx, dy, dz );
	const size_t M = dx.size(), T = s.ox.size(), NP = P.size() / 3;

	FILE * out = std::fopen( argv[7], "w" );
	if ( !out )
		return 1;
	std::fprintf( out, "# prtp_reference rays %zu tris %zu sun %.4f %.4f %.4f masked %zu\n", M, T, sx, sy, sz, s.masked.size() );
	const unsigned nth = std::max( 1u, std::thread::hardware_concurrency() );
	for ( size_t p = 0; p < NP; p++ ) {
		const float px = P[p * 3], py = P[p * 3 + 1], pz = P[p * 3 + 2];
		std::vector<float> bestT( M, 1.0e30f );
		std::vector<int> bestI( M, -1 );
		std::atomic<size_t> next( 0 );
		const size_t PK = 64;   // a packet of rays stays in cache while every triangle streams past
		auto work = [&]() {
			for ( ;; ) {
				const size_t r0 = next.fetch_add( PK );
				if ( r0 >= M )
					return;
				const size_t r1 = std::min( M, r0 + PK );
				float tb[PK]; int ib[PK];
				for ( size_t r = r0; r < r1; r++ ) {
					tb[r - r0] = 1.0e30f; ib[r - r0] = -1;
				}
				for ( size_t i = 0; i < T; i++ ) {
					const float ex = s.ax[i], ey = s.ay[i], ez = s.az[i];
					const float fx = s.bx[i], fy = s.by[i], fz = s.bz[i];
					const float tx = px - s.ox[i], ty = py - s.oy[i], tz = pz - s.oz[i];
					// q = t x e1 is ray-independent
					const float qx = ty * ez - tz * ey, qy = tz * ex - tx * ez, qz = tx * ey - ty * ex;
					const float tq = fx * qx + fy * qy + fz * qz;   // e2 . q
					for ( size_t r = r0; r < r1; r++ ) {
						const float ux = dx[r], uy = dy[r], uz = dz[r];
						const float hx = uy * fz - uz * fy, hy = uz * fx - ux * fz, hz = ux * fy - uy * fx;
						const float det = ex * hx + ey * hy + ez * hz;
						if ( std::fabs( det ) < 1.0e-12f )
							continue;
						const float inv = 1.0f / det;
						const float u = ( tx * hx + ty * hy + tz * hz ) * inv;
						if ( u < 0.0f || u > 1.0f )
							continue;
						const float v = ( ux * qx + uy * qy + uz * qz ) * inv;
						if ( v < 0.0f || u + v > 1.0f )
							continue;
						const float t = tq * inv;
						if ( t > 0.01f && t < tb[r - r0] && !s.hole( i, u, v ) ) {
							tb[r - r0] = t;
							ib[r - r0] = int( i );
						}
					}
				}
				for ( size_t r = r0; r < r1; r++ ) {
					bestT[r] = tb[r - r0];
					bestI[r] = ib[r - r0];
				}
			}
		};
		std::vector<std::thread> th;
		for ( unsigned k = 0; k < nth; k++ )
			th.emplace_back( work );
		for ( auto & t : th )
			t.join();
		double sky[8] = {}, surf[8] = {}, L[8][3] = {}, sh[9] = {}, skyT[8][3] = {};
		for ( size_t r = 0; r < M; r++ ) {
			const int o = ( dx[r] < 0 ) | ( ( dy[r] < 0 ) << 1 ) | ( ( dz[r] < 0 ) << 2 );
			const int i = bestI[r];
			double Tg[3] = { 1.0, 1.0, 1.0 };
			if ( !s.glass.empty() ) {
				const double po[3] = { px, py, pz }, pd[3] = { dx[r], dy[r], dz[r] };
				throughGlass( s, po, pd, i < 0 ? 1.0e30 : double( bestT[r] ), Tg );
			}
			if ( i < 0 ) {
				sky[o] += 1.0 / M;
				for ( int k = 0; k < 3; k++ )
					skyT[o][k] += Tg[k] / M;
				continue;
			}
			surf[o] += 1.0 / M;
			double nx = s.nx[i], ny = s.ny[i], nz = s.nz[i];
			if ( !s.vn.empty() ) {   // lane SMOOTHN1: the smooth normal at the hit (barycentric, renormalized)
				const double hx = px + dx[r] * bestT[r] - s.ox[i], hy = py + dy[r] * bestT[r] - s.oy[i],
					hz = pz + dz[r] * bestT[r] - s.oz[i];
				const double d00 = double( s.ax[i] ) * s.ax[i] + double( s.ay[i] ) * s.ay[i] + double( s.az[i] ) * s.az[i];
				const double d01 = double( s.ax[i] ) * s.bx[i] + double( s.ay[i] ) * s.by[i] + double( s.az[i] ) * s.bz[i];
				const double d11 = double( s.bx[i] ) * s.bx[i] + double( s.by[i] ) * s.by[i] + double( s.bz[i] ) * s.bz[i];
				const double d20 = hx * s.ax[i] + hy * s.ay[i] + hz * s.az[i], d21 = hx * s.bx[i] + hy * s.by[i] + hz * s.bz[i];
				const double den = d00 * d11 - d01 * d01;
				const int16_t * q = &s.vn[size_t( i ) * 9];
				if ( std::fabs( den ) > 0 && ( q[0] || q[1] || q[2] || q[3] || q[4] || q[5] || q[6] || q[7] || q[8] ) ) {
					double u = std::clamp( ( d11 * d20 - d01 * d21 ) / den, 0.0, 1.0 );
					double v = std::clamp( ( d00 * d21 - d01 * d20 ) / den, 0.0, 1.0 - u );
					const double w0 = 1.0 - u - v;
					double m[3];
					for ( int k = 0; k < 3; k++ )
						m[k] = ( w0 * q[k] + u * q[3 + k] + v * q[6 + k] ) / 32767.0;
					const double ml = std::sqrt( m[0] * m[0] + m[1] * m[1] + m[2] * m[2] );
					if ( ml > 1e-6 ) {
						const double sg = m[0] * nx + m[1] * ny + m[2] * nz < 0 ? -1.0 : 1.0;   // the face's side
						nx = sg * m[0] / ml; ny = sg * m[1] / ml; nz = sg * m[2] / ml;
					}
				}
			}
			if ( nx * dx[r] + ny * dy[r] + nz * dz[r] > 0 ) {   // turn it toward the probe
				nx = -nx; ny = -ny; nz = -nz;
			}
			const double g = 0.25 + 0.75 * std::max( 0.0, nx * sx + ny * sy + nz * sz );
			double grey = 0;
			for ( int k = 0; k < 3; k++ ) {
				L[o][k] += s.alb[size_t( i ) * 3 + k] * g * Tg[k] / M;
				grey += s.alb[size_t( i ) * 3 + k] * g * Tg[k] / 3.0;
			}
			// real SH to band 2 (what the relight stores), weight 4pi/M per ray
			const double x = dx[r], y = dy[r], z = dz[r], w = grey * 4.0 * 3.14159265358979323846 / M;
			const double Y[9] = { 0.282095, 0.488603 * y, 0.488603 * z, 0.488603 * x, 1.092548 * x * y,
				1.092548 * y * z, 0.315392 * ( 3 * z * z - 1 ), 1.092548 * x * z, 0.546274 * ( x * x - y * y ) };
			for ( int c = 0; c < 9; c++ )
				sh[c] += Y[c] * w;
		}
		std::fprintf( out, "%zu", p );
		for ( int o = 0; o < 8; o++ )
			std::fprintf( out, "\t%.6f\t%.6f\t%.6f\t%.6f\t%.6f", sky[o], surf[o], L[o][0], L[o][1], L[o][2] );
		for ( int c = 0; c < 9; c++ )
			std::fprintf( out, "\t%.7f", sh[c] );
		for ( int o = 0; o < 8; o++ )
			std::fprintf( out, "\t%.6f\t%.6f\t%.6f", skyT[o][0], skyT[o][1], skyT[o][2] );
		std::fprintf( out, "\n" );
		std::fflush( out );
	}
	std::fclose( out );
	return 0;
}
