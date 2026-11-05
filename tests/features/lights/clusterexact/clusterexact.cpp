// lane PERFFIX2 a (item 6): the light cluster build on one thread with its boxes from the tile edges
// (src/features/lights/cellcluster.cpp) against LIGHTSYS2's build (params.redOldBuild: up to 8 threads from 128 lights, 8 corners
// unprojected per box). Pure C++, no Qt, no GL.
//   1. the same lists, int for int (cells and index), and the same boxes, float for float, on every RECORDED light set
//      (tests/features/lights/clusterexact/data/*.txt: WW_CELL_CLUSTER_DUMP from live Concord / Goodneighbor rows) and on synthetic
//      sets of 16..1024 lights, under a forced split (threadsFrom 1) too
//   2. the new build's median <= 0.2 ms (the PERFRES target) on each recorded set
// Build: g++ -std=c++17 -O2 -I<tree>/src -I<tree>/src/gl tests/features/lights/clusterexact/clusterexact.cpp src/features/lights/cellcluster.cpp
// Run:   clusterexact.exe <data dir>      Red: -DCLUSTER_RED (the timed build is the old one): check 2 must FAIL.
#define _USE_MATH_DEFINES
#include "features/lights/cellcluster.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <random>
#include <string>
#include <vector>

namespace
{
int fails = 0, checks = 0;
void check( bool ok, const std::string & what )
{
	checks++;
	fails += ok ? 0 : 1;
	std::printf( "%s %s\n", ok ? "ok  " : "FAIL", what.c_str() );
}

struct Rec
{
	std::string name;
	float proj[16];
	int vp[4];
	double pad;
	double liveMs;
	std::vector<WwClusterLight> lights;
};

std::vector<Rec> readDir( const std::string & dir )
{
	std::vector<Rec> out;
	DIR * d = opendir( dir.c_str() );
	if ( !d )
		return out;
	std::vector<std::string> files;
	while ( dirent * e = readdir( d ) ) {
		const std::string n = e->d_name;
		if ( n.size() > 4 && n.substr( n.size() - 4 ) == ".txt" )
			files.push_back( n );
	}
	closedir( d );
	std::sort( files.begin(), files.end() );
	for ( const std::string & fn : files ) {
		FILE * f = std::fopen( ( dir + "/" + fn ).c_str(), "r" );
		if ( !f )
			continue;
		char line[4096];
		Rec * r = nullptr;
		while ( std::fgets( line, sizeof line, f ) ) {
			int b = 0, n = 0, refs = 0;
			Rec t {};
			if ( std::sscanf( line, "build %d lights %d vp %d %d %d %d pad %la ms %lf refs %d", &b, &n, &t.vp[0], &t.vp[1], &t.vp[2],
				     &t.vp[3], &t.pad, &t.liveMs, &refs ) == 9 ) {
				t.name = fn + " build " + std::to_string( b ) + " (" + std::to_string( n ) + " lights, " + std::to_string( t.vp[2] ) + "x"
					+ std::to_string( t.vp[3] ) + ")";
				out.push_back( t );
				r = &out.back();
			} else if ( r && std::strncmp( line, "proj", 4 ) == 0 ) {
				char * p = line + 4;
				for ( float & v : r->proj )
					v = float( std::strtod( p, &p ) );
			} else if ( r && line[0] == 'l' && line[1] == ' ' ) {
				WwClusterLight l;
				char * p = line + 2;
				for ( double & v : l.pos )
					v = std::strtod( p, &p );
				for ( double & v : l.dir )
					v = std::strtod( p, &p );
				l.radius = std::strtod( p, &p );
				l.shape = int( std::strtol( p, &p, 10 ) );
				l.cosOuter = std::strtod( p, &p );
				r->lights.push_back( l );
			}
		}
		std::fclose( f );
	}
	return out;
}

void perspective( float m[16], int W, int H, double fovY )
{
	const double kNear = 10.0, kFar = 60000.0;
	const double h2 = std::tan( fovY / 360.0 * M_PI ) * kNear, w2 = h2 * W / H;
	std::memset( m, 0, 16 * sizeof( float ) );
	m[0] = float( kNear / w2 );
	m[5] = float( kNear / h2 );
	m[10] = float( ( kNear + kFar ) / ( kNear - kFar ) );
	m[11] = -1.0f;
	m[14] = float( 2.0 * kNear * kFar / ( kNear - kFar ) );
}

Rec synthetic( int n, unsigned seed, int W, int H )
{
	Rec r {};
	r.name = "synthetic " + std::to_string( n ) + " lights " + std::to_string( W ) + "x" + std::to_string( H );
	perspective( r.proj, W, H, 55.0 );
	r.proj[8] = 0.37f / float( W );	// a TAA jitter
	r.proj[9] = -0.21f / float( H );
	r.vp[2] = W, r.vp[3] = H;
	r.pad = 1.0;
	std::mt19937 rng( seed );
	std::uniform_real_distribution<double> U( 0.0, 1.0 );
	for ( int i = 0; i < n; i++ ) {
		WwClusterLight l;
		const double d = 50.0 + 9000.0 * U( rng ), a = ( U( rng ) - 0.5 ) * M_PI * 1.6;
		l.pos[0] = d * std::sin( a ), l.pos[1] = ( U( rng ) - 0.5 ) * 900.0, l.pos[2] = -d * std::cos( a );
		l.radius = 64.0 * std::pow( 40.0, U( rng ) );
		l.shape = int( U( rng ) * 3.0 );
		double dx = U( rng ) - 0.5, dy = U( rng ) - 0.5, dz = U( rng ) - 0.5, len = std::sqrt( dx * dx + dy * dy + dz * dz );
		l.dir[0] = dx / len, l.dir[1] = dy / len, l.dir[2] = dz / len;
		l.cosOuter = l.shape == 1 ? std::cos( ( 10.0 + 70.0 * U( rng ) ) * M_PI / 180.0 ) : -1.0;
		r.lights.push_back( l );
	}
	return r;
}

bool sameGrid( const WwClusterGrid & a, const WwClusterGrid & b )
{
	return a.cells == b.cells && a.index == b.index && a.tilesX == b.tilesX && a.tilesY == b.tilesY && a.slices == b.slices
		&& a.logNear == b.logNear && a.sliceScale == b.sliceScale && a.refs == b.refs && a.overflowed == b.overflowed
		&& a.culledOffscreen == b.culledOffscreen;
}

bool sameBoxes( const WwClusterScratch & a, const WwClusterScratch & b )
{
	return a.boxes.size() == b.boxes.size()
		&& std::memcmp( a.boxes.data(), b.boxes.data(), a.boxes.size() * sizeof( WwClusterBox ) ) == 0;
}

double medianMs( const Rec & r, const WwClusterParams & prm, int reps )
{
	WwClusterScratch sc;
	WwClusterGrid g;
	wwClusterBuild( r.proj, r.vp, r.lights, prm, g, &sc );	// the boxes once (a moving camera keeps them)
	std::vector<double> ms;
	for ( int k = 0; k < reps; k++ ) {
		const auto t0 = std::chrono::steady_clock::now();
		wwClusterBuild( r.proj, r.vp, r.lights, prm, g, &sc );
		ms.push_back( std::chrono::duration<double, std::milli>( std::chrono::steady_clock::now() - t0 ).count() );
	}
	std::sort( ms.begin(), ms.end() );
	return ms[ms.size() / 2];
}
}	// namespace

int main( int argc, char ** argv )
{
	std::setvbuf( stdout, nullptr, _IONBF, 0 );
	std::vector<Rec> recs = argc > 1 ? readDir( argv[1] ) : std::vector<Rec> {};
	const size_t recorded = recs.size();
	check( recorded > 0 || ( argc > 1 && std::strcmp( argv[1], "--synthetic-only" ) == 0 ),
		"recorded light sets read: " + std::to_string( recorded ) );
	for ( int n : { 16, 64, 149, 256, 1024 } )
		recs.push_back( synthetic( n, 11u + unsigned( n ), 1920, 1026 ) );
	recs.push_back( synthetic( 149, 5u, 1246, 991 ) );
	// 1. the same lists and boxes, every set, old (threads from 128, corner boxes) vs new (one thread, edge boxes),
	//    and the new rule forced to split (threadsFrom 1) and fresh (no scratch)
	for ( const Rec & r : recs ) {
		WwClusterParams oldP, newP, splitP;
		oldP.jitterPadPx = newP.jitterPadPx = splitP.jitterPadPx = r.pad;
		oldP.redOldBuild = true;
		splitP.threadsFrom = 1;
		WwClusterScratch so, sn, ss;
		WwClusterGrid go, gn, gs, gf;
		const bool ok = wwClusterBuild( r.proj, r.vp, r.lights, oldP, go, &so ) && wwClusterBuild( r.proj, r.vp, r.lights, newP, gn, &sn )
			&& wwClusterBuild( r.proj, r.vp, r.lights, splitP, gs, &ss ) && wwClusterBuild( r.proj, r.vp, r.lights, newP, gf, nullptr );
		check( ok && sameGrid( go, gn ) && sameGrid( go, gs ) && sameGrid( go, gf ) && sameBoxes( so, sn ) && go.refs > 0,
			r.name + ": lists + boxes old == new == split == fresh, refs " + std::to_string( go.refs ) );
	}
	// control: the comparison sees a real change (LIGHTSYS2's own refuter, the slice-middle depths, drops lights)
	{
		const Rec & r = recs[recorded + 3];
		WwClusterParams a, b;
		b.redMidSlice = true;
		WwClusterScratch sa, sb;
		WwClusterGrid ga, gb;
		wwClusterBuild( r.proj, r.vp, r.lights, a, ga, &sa );
		wwClusterBuild( r.proj, r.vp, r.lights, b, gb, &sb );
		check( !sameGrid( ga, gb ) && !sameBoxes( sa, sb ), "control: the midslice refuter's lists and boxes differ" );
	}
	// 2. the cost: the new build's median <= 0.2 ms on each recorded set (the red times the old build)
	for ( size_t i = 0; i < recorded; i++ ) {
		const Rec & r = recs[i];
		WwClusterParams oldP, newP;
		oldP.jitterPadPx = newP.jitterPadPx = r.pad;
		oldP.redOldBuild = true;
#ifdef CLUSTER_RED
		newP.redOldBuild = true;
#endif
		const double mo = medianMs( r, oldP, 31 ), mn = medianMs( r, newP, 31 );
		char buf[256];
		std::snprintf( buf, sizeof buf, ": build median old %.3f ms, new %.3f ms (live %.3f), target <= 0.2", mo, mn, r.liveMs );
		check( mn <= 0.2, r.name + buf );
	}
	std::printf( "%d checks, %d failures\n", checks, fails );
	return fails ? 1 : 0;
}
