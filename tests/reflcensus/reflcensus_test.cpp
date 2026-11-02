// Lane TOOLS1101 (REFLPROBECLI1): src/reflcensus.h against an analytic count. A square floor (z = 0, half 20000)
// through the bake's own tracer (probebvh::Bvh) vs ray-plane arithmetic per texel, for three cameras:
//   A outdoors over the floor, B outdoors past its edge (the ground taps all miss: "tapless"), C the same camera as
//   A indoors (the camera probe's smaller box). Placement is checked against hand-written positions.
//   reflcensus_test.exe            green: prints "REFLCENSUS PASS <n>" and exits 0
//   reflcensus_test.exe lift       the census runs the red horizon d.z < 0.1: must print "FAIL below:" and exit 1
//   reflcensus_test.exe noreach    the census runs with no far plane: must print "FAIL hits:" and exit 1
//   reflcensus_test.exe roof       lane INTGI1: the interior --auto cameras on the topmost surface: must print "FAIL floors:"
//   reflcensus_test.exe stand      every probe stands in the geometry (no split): must print "FAIL stand:"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

#include "reflcensus.h"

using namespace reflcensus;

static const double H = 20000.0;
static int fails = 0, checks = 0;

static void check( bool ok, const char * name, const char * fmt, long a, long b, const char * scen, int probe )
{
	checks++;
	if ( !ok ) {
		fails++;
		std::printf( "FAIL %s: %s probe %d: census %ld vs analytic %ld (%s)\n", name, scen, probe, a, b, fmt );
	}
}

// the floor's arithmetic: does a probe's capture draw the floor in unit direction d?
static bool floorSees( const Probe & p, const double d[3], double reach )
{
	if ( d[2] >= 0.0 )
		return false;
	const double m = std::max( { std::fabs( d[0] ), std::fabs( d[1] ), std::fabs( d[2] ) } );
	const double t = p.pos[2] / -d[2];
	if ( t < kNear / m || t > reach / m )
		return false;
	return std::fabs( p.pos[0] + t * d[0] ) <= H && std::fabs( p.pos[1] + t * d[1] ) <= H;
}

static Count analytic( const Probe & p )
{
	Count c;
	c.reach = reachOf( p );
	double d[3];
	for ( int k = 0; k < 9; k++ ) {
		tap( k, d );
		const double l = std::sqrt( d[0] * d[0] + d[1] * d[1] + d[2] * d[2] );
		for ( int a = 0; a < 3; a++ )
			d[a] /= l;
		c.tapsHit += floorSees( p, d, c.reach ) ? 1 : 0;
	}
	for ( int y = 0; y < kFace; y++ )
		for ( int x = 0; x < kFace; x++ ) {
			octDecode( ( x + 0.5 ) / kFace, ( y + 0.5 ) / kFace, d );
			c.texels++;
			if ( floorSees( p, d, c.reach ) ) {
				c.hits++;
				continue;
			}
			c.misses++;
			if ( d[2] < 0.0 ) {
				c.below++;
				c.belowTapless += c.tapsHit == 0 ? 1 : 0;
			} else {
				c.above++;
			}
		}
	return c;
}

int main( int argc, char ** argv )
{
	std::setvbuf( stdout, nullptr, _IONBF, 0 );
	int red = RedNone;
	if ( argc > 1 && !std::strcmp( argv[1], "lift" ) )
		red = RedLift;
	if ( argc > 1 && !std::strcmp( argv[1], "noreach" ) )
		red = RedNoReach;
	if ( argc > 1 && !std::strcmp( argv[1], "roof" ) )
		red = RedRoof;
	if ( argc > 1 && !std::strcmp( argv[1], "stand" ) )
		red = RedStand;

	{	// lane INTGI1 (REFLPROBECLI2): the interior --auto cameras. A two-storey box (half 3000): a roof slab 1000/990, a
		// floor slab 500/490, the ground at 0 under it and past it (half 20000)
		probebvh::Bvh b2;
		auto slab = [&b2]( float z, float hh ) {
			const float cx = 123.4f, cy = -98.7f;
			const float c[5][2] = { { -hh, -hh }, { hh, -hh }, { hh, hh }, { -hh, hh }, { -hh, -hh } };
			for ( int k = 0; k < 4; k++ ) {
				const float tri[9] = { cx, cy, z, c[k][0], c[k][1], z, c[k + 1][0], c[k + 1][1], z };
				b2.t.insert( b2.t.end(), tri, tri + 9 );
			}
		};
		slab( 1000.0f, 3000.0f );
		slab( 990.0f, 3000.0f );
		slab( 500.0f, 3000.0f );
		slab( 490.0f, 3000.0f );
		slab( 0.0f, 20000.0f );
		b2.build();
		std::vector<double> zs, cz, out;
		columnHits( b2, 777.0, 333.0, 1016.0, 1048.0, zs );
		checks++;
		const bool hitsOk = zs.size() == 5 && std::fabs( zs[0] - 1000.0 ) < 1e-3 && std::fabs( zs[1] - 990.0 ) < 1e-3
			&& std::fabs( zs[2] - 500.0 ) < 1e-3 && std::fabs( zs[3] - 490.0 ) < 1e-3 && std::fabs( zs[4] ) < 1e-3;
		if ( !hitsOk ) {
			fails++;
			std::printf( "FAIL column: %d surfaces under the box (want 1000 990 500 490 0)\n", int( zs.size() ) );
		}
		floorsOf( zs, 128.0, red, cz );
		checks++;
		if ( !( cz.size() == 2 && std::fabs( cz[0] - 628.0 ) < 1e-3 && std::fabs( cz[1] - 128.0 ) < 1e-3 ) ) {
			fails++;
			std::printf( "FAIL floors: %d cameras in the box, first %.1f (want 628 and 128: each storey, never the roof)\n",
				int( cz.size() ), cz.empty() ? -1.0 : cz[0] );
		}
		std::vector<double> open;
		columnHits( b2, 9000.0, 333.0, 1016.0, 1048.0, open );
		floorsOf( open, 128.0, red, out );
		checks++;
		if ( !( open.size() == 1 && out.empty() ) ) {
			fails++;
			std::printf( "FAIL floors: past the box %d surfaces, %d cameras (want 1, 0: open ground has no ceiling)\n",
				int( open.size() ), int( out.size() ) );
		}
		std::vector<double> lowCams;
		const std::vector<double> low = { 300.0, 150.0, 0.0 };	// 150 u storeys: under eye 128 + headroom 32
		floorsOf( low, 128.0, red, lowCams );
		checks++;
		if ( !lowCams.empty() ) {
			fails++;
			std::printf( "FAIL floors: %d cameras in 150 u storeys (want 0: under eye + headroom)\n", int( lowCams.size() ) );
		}
		std::printf( "interior auto: box column %d surfaces -> %d cameras; open ground %d surface(s) -> %d\n", int( zs.size() ),
			int( cz.size() ), int( open.size() ), int( out.size() ) );

		// where a probe stands (standOf): the soup box wider than the geometry, so one column inside it is empty
		const double O2[2] = { 0.0, 0.0 }, box2[4] = { -30000.0, -30000.0, 30000.0, 30000.0 };
		struct St { double x, y, z; bool interior; int want; const char * what; } st[] = {
			{ 777.0, 333.0, 628.0, true, StandGeometry, "between the storeys" },
			{ 777.0, 333.0, -200.0, true, StandVoid, "under the ground" },
			{ 777.0, 333.0, 1200.0, true, StandVoid, "over the roof indoors" },
			{ 777.0, 333.0, 1200.0, false, StandGeometry, "over the roof outdoors" },
			{ 25000.0, 0.0, 128.0, true, StandVoid, "an empty column in the box" },
			{ 40000.0, 0.0, 128.0, true, StandOutside, "past the box" },
		};
		int stOk = 0;
		for ( const St & s : st ) {
			Probe p;
			p.pos[0] = s.x;
			p.pos[1] = s.y;
			p.pos[2] = s.z;
			const int got = standOf( b2, O2, p, box2, 1016.0, 1048.0, s.interior, red );
			checks++;
			if ( got != s.want ) {
				fails++;
				std::printf( "FAIL stand: %s: stand %d (want %d)\n", s.what, got, s.want );
			} else {
				stOk++;
			}
		}
		std::printf( "stand: %d of %d probes placed right (geometry, outside, void)\n", stOk, int( sizeof st / sizeof st[0] ) );
	}

	probebvh::Bvh bvh;
	// the square as a fan of 4 round an off-lattice point: the tracer is not watertight on a shared edge, and a
	// two-triangle split along x = y runs through the grid probes' own xy (2 texels a probe fell through it)
	const float cx = 1234.567f, cy = -987.654f, h = float( H );
	const float corner[5][2] = { { -h, -h }, { h, -h }, { h, h }, { -h, h }, { -h, -h } };
	for ( int k = 0; k < 4; k++ ) {
		const float tri[9] = { cx, cy, 0, corner[k][0], corner[k][1], 0, corner[k + 1][0], corner[k + 1][1], 0 };
		bvh.t.insert( bvh.t.end(), tri, tri + 9 );
	}
	bvh.build();
	const double O[2] = { 0.0, 0.0 };

	// placement, by hand: camera (100, 50, 400): camera probe at (128, 0, 384); grid boxes round (0, 0) at z 512
	std::vector<Probe> P;
	const double camA[3] = { 100.0, 50.0, 400.0 };
	place( camA, false, P );
	checks++;
	bool okPlace = P.size() == 10 && P[0].kind == 0 && P[0].pos[0] == 128.0 && P[0].pos[1] == 0.0 && P[0].pos[2] == 384.0
		&& P[0].bmax[0] - P[0].bmin[0] == 8192.0 && P[0].bmax[2] - P[0].bmin[2] == 4096.0;
	for ( int j = -1, n = 1; j <= 1; j++ )
		for ( int i = -1; i <= 1; i++, n++ )
			okPlace = okPlace && P[size_t( n )].pos[0] == 1024.0 + 2048.0 * i && P[size_t( n )].pos[1] == 1024.0 + 2048.0 * j
				&& P[size_t( n )].pos[2] == 512.0 && P[size_t( n )].bmin[2] == -512.0 && P[size_t( n )].bmax[2] == 1536.0;
	if ( !okPlace ) {
		fails++;
		std::printf( "FAIL place: camera A's probes are not the hand-written ones\n" );
	}
	std::vector<Probe> PC;
	place( camA, true, PC );
	checks++;
	if ( PC.size() != 10 || PC[0].bmax[0] - PC[0].bmin[0] != 2048.0 || PC[0].bmax[2] - PC[0].bmin[2] != 1024.0
		|| reachOf( PC[0] ) != 4096.0 || std::fabs( reachOf( P[0] ) - 12288.0 ) > 1e-9 || std::fabs( reachOf( P[1] ) - 4096.0 ) > 1e-9 ) {
		fails++;
		std::printf( "FAIL place: interior box or reach (camera %g, grid %g, interior camera %g)\n", reachOf( P[0] ), reachOf( P[1] ),
			reachOf( PC[0] ) );
	}

	const double camB[3] = { 30000.0, 0.0, 400.0 };
	struct Scen { const char * name; const double * cam; bool interior; } scen[3] = {
		{ "A", camA, false }, { "B", camB, false }, { "C", camA, true } };
	long tot[3][3] = {};
	for ( int s = 0; s < 3; s++ ) {
		std::vector<Probe> ps;
		place( scen[s].cam, scen[s].interior, ps );
		for ( size_t i = 0; i < ps.size(); i++ ) {
			const double xy[4] = { -H, -H, H, H };
			const Count a = census( bvh, O, ps[i], red, xy ), b = analytic( ps[i] );
			const int pi = int( i );
			check( a.texels == b.texels, "texels", "64x64", a.texels, b.texels, scen[s].name, pi );
			check( a.hits == b.hits, "hits", "drawn texels", a.hits, b.hits, scen[s].name, pi );
			check( a.below == b.below, "below", "below-horizon misses", a.below, b.below, scen[s].name, pi );
			check( a.above == b.above, "above", "sky misses", a.above, b.above, scen[s].name, pi );
			check( a.tapsHit == b.tapsHit, "taps", "ground taps that hit", a.tapsHit, b.tapsHit, scen[s].name, pi );
			check( a.belowTapless == b.belowTapless, "tapless", "below misses with no tap", a.belowTapless, b.belowTapless,
				scen[s].name, pi );
			std::printf( "%s probe %d kind %d pos %.0f,%.0f,%.0f reach %.0f: hits %ld below %ld above %ld taps %d tapless %ld\n",
				scen[s].name, pi, ps[i].kind, ps[i].pos[0], ps[i].pos[1], ps[i].pos[2], a.reach, a.hits, a.below, a.above,
				a.tapsHit, a.belowTapless );
			tot[s][0] += a.below;
			tot[s][1] += a.belowTapless;
			tot[s][2] += a.hits;
			// belowEdge: over the floor no below miss runs off it (they stop at the reach); past it every one does
			if ( red == RedNone ) {
				checks++;
				// (B's camera probe reaches 12288 back over the floor: some of its shallow misses end above it)
				const bool edgeOk = s != 1 ? a.belowEdge == 0
					: ps[i].kind == 1 ? a.belowEdge == a.below : a.belowEdge > 0 && a.belowEdge < a.below;
				if ( !edgeOk ) {
					fails++;
					std::printf( "FAIL edge: %s probe %d: belowEdge %ld of below %ld\n", scen[s].name, pi, a.belowEdge, a.below );
				}
			}
		}
	}
	// the fields must MOVE between the scenes, or the census could be printing a constant
	checks++;
	if ( !( tot[0][1] == 0 && tot[1][1] > 0 && tot[0][2] > tot[1][2] && tot[0][0] > 0 ) ) {
		fails++;
		std::printf( "FAIL moves: A tapless %ld (want 0), B tapless %ld (want > 0), hits A %ld > B %ld, below A %ld > 0\n",
			tot[0][1], tot[1][1], tot[0][2], tot[1][2], tot[0][0] );
	}
	if ( fails ) {
		std::printf( "REFLCENSUS FAIL %d of %d\n", fails, checks );
		return 1;
	}
	std::printf( "REFLCENSUS PASS %d\n", checks );
	return 0;
}
