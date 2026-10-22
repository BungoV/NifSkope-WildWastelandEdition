// bakert_walk_gate -- lane RTBAKE1FIX (2026-10-22): does refine()'s CPU walk give the full walk's answer?
//
//   bakert_walk_gate <soup.psp> <probes.tsv> [batches=16] [rays=2048]
//
// The probe bake's primary rays (the Fibonacci set and SMOOTHN1's rotated batches, as src/probebake.cpp makes
// them) from every probe of a probes.tsv, over the soup's BVH with its alpha masks. For each ray the full walk
// (probebvh ray to rayMax = the CPU bake's answer) is the reference. A GPU may return any triangle hit within float
// error of it, so every triangle hit within 1e-5 (1 + t) of the reference is a candidate GPU answer; for each:
//   old: ray() bounded at the candidate's t (refine before this lane), a miss re-cast in full;
//   new: bakert::rayFrom() from t + pad, re-cast in full when it is not sure (refine now).
// Prints rays, candidates, old/new answers unlike the reference, new re-casts. Exit 0 only when new = 0 unlike;
// the old line is the red (it must show > 0 on a soup where the in-cell bake flipped). No GPU, no Qt.
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "../src/bakertwalk.h"

static bool readSoup( const char * path, std::vector<float> & tris, probebvh::AlphaMask & am )
{
	FILE * f = std::fopen( path, "rb" );
	if ( !f )
		return false;
	auto rd = [&]( void * p, size_t n ) { return std::fread( p, 1, n, f ) == n; };
	uint32_t head[3];
	if ( !rd( head, 12 ) || head[0] != 0x31505350u )
		return false;
	tris.resize( size_t( head[1] ) * 9 );
	if ( !rd( tris.data(), tris.size() * 4 ) )
		return false;
	std::fseek( f, long( head[2] ) * 28, SEEK_CUR );
	uint32_t tail[2];
	bool more = rd( tail, 8 );
	if ( more && tail[0] == 0x31424C41u ) {   // ALB1
		std::fseek( f, long( tail[1] ) * 3, SEEK_CUR );
		more = rd( tail, 8 );
	}
	if ( more && tail[0] == 0x31534C47u ) {   // GLS1
		std::fseek( f, long( tail[1] ) * 39, SEEK_CUR );
		more = rd( tail, 8 );
	}
	if ( more && tail[0] == 0x314F5754u ) {   // TWO1
		std::fseek( f, long( tail[1] ), SEEK_CUR );
		more = rd( tail, 8 );
	}
	while ( more ) {   // the sized tails: AMK1/AMK2 is the one the walk reads
		uint32_t bytes;
		if ( !rd( &bytes, 4 ) )
			break;
		const long end = std::ftell( f ) + long( bytes );
		if ( tail[0] == 0x314B4D41u || tail[0] == 0x324B4D41u ) {
			const bool amk2 = tail[0] == 0x324B4D41u;
			uint32_t n, len, w, h;
			rd( &n, 4 );
			am.maps.resize( n );
			for ( uint32_t m = 0; m < n; m++ ) {
				rd( &len, 4 );
				std::fseek( f, long( len ), SEEK_CUR );
				rd( &w, 4 );
				rd( &h, 4 );
				am.maps[m].w = int( w );
				am.maps[m].h = int( h );
				am.maps[m].a.resize( size_t( w ) * h );
				rd( am.maps[m].a.data(), am.maps[m].a.size() );
			}
			rd( &n, 4 );
			for ( uint32_t m = 0; m < n; m++ ) {
				rd( &len, 4 );
				std::fseek( f, long( len ), SEEK_CUR );
			}
			am.triOf.assign( tris.size() / 9, -1 );
			for ( uint32_t i = 0; i < tail[1]; i++ ) {
				uint32_t ti, thr;
				int32_t map, model;
				probebvh::AlphaMask::Tri t;
				rd( &ti, 4 );
				rd( &map, 4 );
				rd( &model, 4 );
				rd( &thr, 4 );
				rd( t.uv, 24 );
				if ( amk2 )
					rd( t.as, 12 );
				t.map = map;
				t.model = model;
				t.thr = (unsigned char)thr;
				if ( ti < am.triOf.size() ) {
					am.triOf[ti] = int( am.tris.size() );
					am.tris.push_back( t );
				}
			}
			break;
		}
		std::fseek( f, end, SEEK_SET );
		more = rd( tail, 8 );
	}
	std::fclose( f );
	return true;
}

int main( int argc, char ** argv )
{
	if ( argc < 3 ) {
		std::fprintf( stderr, "usage: bakert_walk_gate <soup.psp> <probes.tsv> [batches=16] [rays=2048]\n" );
		return 2;
	}
	const int nBatch = argc > 3 ? std::atoi( argv[3] ) : 16, N = argc > 4 ? std::atoi( argv[4] ) : 2048;
	const double rayMax = 131072.0;
	std::vector<float> soupT;
	probebvh::AlphaMask am;
	if ( !readSoup( argv[1], soupT, am ) ) {
		std::fprintf( stderr, "soup unreadable\n" );
		return 2;
	}
	std::vector<std::array<double, 3>> P;
	{
		std::ifstream in( argv[2] );
		std::string line;
		while ( std::getline( in, line ) ) {
			if ( line.empty() || line[0] == '#' || line.compare( 0, 2, "id" ) == 0 )
				continue;
			std::istringstream ss( line );
			std::string id, cls, lvl;
			double x, y, z;
			if ( ss >> id >> cls >> lvl >> x >> y >> z )
				P.push_back( { double( float( x ) ), double( float( y ) ), double( float( z ) ) } );
		}
	}
	if ( P.empty() )
		return 2;
	// the bake's local origin and BVH, as probebake.cpp builds them
	double O[2] = { 0, 0 };
	for ( auto & p : P ) {
		O[0] += float( p[0] );
		O[1] += float( p[1] );
	}
	O[0] = std::floor( O[0] / double( P.size() ) );
	O[1] = std::floor( O[1] / double( P.size() ) );
	probebvh::Bvh bvh;
	bvh.t.resize( soupT.size() );
	for ( size_t i = 0; i < soupT.size(); i += 3 ) {
		bvh.t[i] = float( double( soupT[i] ) - O[0] );
		bvh.t[i + 1] = float( double( soupT[i + 1] ) - O[1] );
		bvh.t[i + 2] = soupT[i + 2];
	}
	bvh.build();
	bvh.mask = &am;
	std::vector<double> dirs( size_t( N ) * 3 );
	const double golden = 3.14159265358979323846 * ( 3.0 - std::sqrt( 5.0 ) );
	for ( int i = 0; i < N; i++ ) {
		const double z = 1.0 - ( 2.0 * i + 1.0 ) / N, r = std::sqrt( std::max( 0.0, 1.0 - z * z ) ), ph = golden * i;
		dirs[size_t( i ) * 3] = r * std::cos( ph );
		dirs[size_t( i ) * 3 + 1] = r * std::sin( ph );
		dirs[size_t( i ) * 3 + 2] = z;
	}
	auto batchDirs = [&]( int pi, int b, std::vector<double> & buf ) -> const double * {   // probebake.cpp's
		if ( b == 0 )
			return dirs.data();
		uint64_t s = uint64_t( pi ) * 0x9E3779B97F4A7C15ull ^ ( uint64_t( b ) * 0xD1B54A32D192ED03ull + 0x5851F42D4C957F2Dull );
		auto next = [&s]() {
			s += 0x9E3779B97F4A7C15ull;
			uint64_t z = s;
			z = ( z ^ ( z >> 30 ) ) * 0xBF58476D1CE4E5B9ull;
			z = ( z ^ ( z >> 27 ) ) * 0x94D049BB133111EBull;
			z ^= z >> 31;
			return double( z >> 11 ) * ( 1.0 / 9007199254740992.0 );
		};
		const double u1 = next(), u2 = next(), u3 = next(), tp = 2.0 * 3.14159265358979323846;
		const double a = std::sqrt( 1.0 - u1 ), c = std::sqrt( u1 );
		const double x = a * std::sin( tp * u2 ), y = a * std::cos( tp * u2 ), z = c * std::sin( tp * u3 ), w = c * std::cos( tp * u3 );
		const double M[9] = { 1 - 2 * ( y * y + z * z ), 2 * ( x * y - z * w ), 2 * ( x * z + y * w ),
			2 * ( x * y + z * w ), 1 - 2 * ( x * x + z * z ), 2 * ( y * z - x * w ),
			2 * ( x * z - y * w ), 2 * ( y * z + x * w ), 1 - 2 * ( x * x + y * y ) };
		buf.resize( dirs.size() );
		for ( size_t i = 0; i < dirs.size(); i += 3 )
			for ( int r = 0; r < 3; r++ )
				buf[i + size_t( r )] = M[r * 3] * dirs[i] + M[r * 3 + 1] * dirs[i + 1] + M[r * 3 + 2] * dirs[i + 2];
		return buf.data();
	};
	std::atomic<long long> rays { 0 }, cands { 0 }, oldBad { 0 }, newBad { 0 }, newRecast { 0 }, oldProbes { 0 }, newProbes { 0 };
	std::atomic<int> next { 0 };
	const int nT = std::max( 1u, std::thread::hardware_concurrency() );
	std::vector<std::thread> pool;
	for ( int th = 0; th < nT; th++ )
		pool.emplace_back( [&]() {
			std::vector<double> rbuf;
			std::vector<std::pair<double, int>> C;
			for ( int pi = next++; pi < int( P.size() ); pi = next++ ) {
				const double o[3] = { P[size_t( pi )][0] - O[0], P[size_t( pi )][1] - O[1], P[size_t( pi )][2] };
				long long ob = 0, nb = 0;
				for ( int b = 0; b < nBatch; b++ ) {
					const double * bd = batchDirs( pi, b, rbuf );
					for ( int i = 0; i < N; i++ ) {
						const double * d = &bd[size_t( i ) * 3];
						rays++;
						double tF = 0;
						int triF = -1;
						if ( !( bvh.ray( o, d, rayMax, &tF, &triF ) && triF >= 0 ) )
							continue;
						// every triangle a float tracer could return here: valid double hits within the window
						const double win = 1e-5 * ( 1.0 + tF );
						C.clear();
						double inv[3];
						for ( int k = 0; k < 3; k++ )
							inv[k] = std::fabs( d[k] ) > 1e-300 ? 1.0 / d[k] : 0.0;
						int st[128], sp = 0;
						st[sp++] = 0;
						while ( sp ) {
							const probebvh::Bvh::Node & nd = bvh.nodes[size_t( st[--sp] )];
							if ( !probebvh::Bvh::slab( nd, o, inv, tF + win ) )
								continue;
							if ( nd.left < 0 ) {
								for ( int j = nd.first; j < nd.first + nd.count; j++ ) {
									double tt, bu, bv;
									const int ti = bvh.idx[size_t( j )];
									if ( probebvh::Bvh::tri( &bvh.t[size_t( ti ) * 9], o, d, tt, bu, bv ) && tt > 1e-4
										&& tt <= rayMax && std::fabs( tt - tF ) <= win && !am.hole( ti, bu, bv ) )
										C.push_back( { tt, ti } );
								}
							} else if ( sp < 126 ) {
								st[sp++] = nd.first;
								st[sp++] = nd.left;
							}
						}
						for ( const auto & c : C ) {
							cands++;
							double t = 0;
							int tri = -1;
							// old refine: ray() bounded at the candidate's t, a miss re-cast in full
							if ( !( bvh.ray( o, d, c.first, &t, &tri ) && tri >= 0 ) ) {
								t = tF;
								tri = triF;
							}
							if ( tri != triF || t != tF ) {
								oldBad++;
								ob++;
							}
							// new refine: rayFrom() from t + pad, re-cast in full when not sure (or nothing found)
							bool sure = true;
							const double B = std::min( rayMax, c.first + bakert::kNearPad * ( 1.0 + c.first ) );
							t = 0;
							tri = -1;
							if ( !( bakert::rayFrom( bvh, o, d, B, &t, &tri, &sure ) && sure && tri >= 0 ) ) {
								newRecast++;
								t = tF;
								tri = triF;
							}
							if ( tri != triF || t != tF ) {
								newBad++;
								nb++;
							}
						}
					}
				}
				oldProbes += ob ? 1 : 0;
				newProbes += nb ? 1 : 0;
			}
		} );
	for ( auto & t : pool )
		t.join();
	std::printf( "soup %zu tris (%zu masked), %zu probes, %d batches of %d rays\n", soupT.size() / 9, am.tris.size(), P.size(), nBatch, N );
	std::printf( "rays %lld, candidate GPU answers %lld\n", rays.load(), cands.load() );
	std::printf( "old refine (bounded at the GPU t): %lld answers unlike the full walk, in %lld probes\n", oldBad.load(), oldProbes.load() );
	std::printf( "new refine (rayFrom + re-cast when unsure): %lld unlike, in %lld probes; re-cast %lld (%.3f%% of candidates)\n",
		newBad.load(), newProbes.load(), newRecast.load(), cands ? 100.0 * double( newRecast.load() ) / double( cands.load() ) : 0.0 );
	std::printf( "%s\n", newBad.load() == 0 ? "PASS" : "FAIL" );
	return newBad.load() == 0 ? 0 : 1;
}
