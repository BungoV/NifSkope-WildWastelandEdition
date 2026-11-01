// lane PERFFIX3 (item 4): the clouds' coverage tables built off the GL thread (src/gl/cloudcaljobs.h) give the same
// bytes as the GL thread's old memo, with the real noise volumes and the real tables (cloudvolmath.h calibrate,
// varCalibrate). No GL, no Qt.
//   g++ -O2 -std=c++17 -I src tests/cloudcaljobs/cloudcaljobs_test.cpp -o x.exe     (red: -DCLOUDCALJOBS_RED)
// Checks:
//  A  a camera walk of band inputs (the eye's climb moves the bands: cloudmap.cpp), each step settled: the slot's table
//     == the OLD memo's table at that step, byte for byte (OLD = this file's transcription of cloudvol.cpp calibrated
//     at 4c9ea88e: recompute from the frame's exact inputs when the quantised key moves)
//  B  a quantum revisited with other exact inputs (heights 5 m apart) is rebuilt from the new inputs, as OLD does
//  C  the first table is waited for (Fresh at once), later moves are Pending while the worker runs, and the old table
//     stands in meanwhile (byte-equal to the previous step's)
//  D  revisiting exact inputs starts no job (A/B/A/B: 2 started, OLD computes 4)
//  E  the Variety set: 14 tables on 14 threads == the 14 tables one after another, byte for byte
//  T  timing: OLD Variety on the calling thread vs the GL thread's wait on a later move (0 while pending)
// The red (CLOUDCALJOBS_RED: jobs keyed on the quantised key) must fail B.
#include "gl/cloudcaljobs.h"
#include "gl/cloudnoise.h"
#include "gl/cloudvolmath.h"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <thread>
#include <vector>

namespace V = WwCloudVolMath;
namespace J = WwCloudCalJobs;

static std::vector<std::uint8_t> g_shape, g_detail;
static int g_fail = 0, g_checks = 0;

static void check( bool ok, const char * what )
{
	g_checks++;
	if ( !ok )
		g_fail++;
	std::printf( "%s %s\n", ok ? "ok  " : "FAIL", what );
}

struct In
{
	float base, top, dens, sheet;
};

// OLD: cloudvol.cpp calibrated() at 4c9ea88e, one band
struct OldMemo
{
	int key[4] = { -1, 0, 0, 0 };
	float cal[V::kCalN] = {};
	int computes = 0;
	void update( int band, const In & x )
	{
		const int k[4] = { int( std::lround( x.base * 50.0f ) ), int( std::lround( x.top * 50.0f ) ),
			int( std::lround( x.dens * 100.0f ) ), int( std::lround( x.sheet * 32.0f ) ) };
		if ( std::memcmp( k, key, sizeof( k ) ) ) {
			std::memcpy( key, k, sizeof( k ) );
			V::calibrate( g_shape.data(), WwCloudNoise::kShapeN, g_detail.data(), WwCloudNoise::kDetailN, band, x.base, x.top,
				x.dens, x.sheet, false, cal );
			computes++;
		}
	}
};

// NEW: the shipped slot call, as cloudvol.cpp calibrated() makes it
static J::Got newUpdate( J::Slot & s, J::Jobs & jobs, int band, const In & x )
{
	const J::Bits qkey = { std::uint32_t( std::lround( x.base * 50.0f ) ), std::uint32_t( std::lround( x.top * 50.0f ) ),
		std::uint32_t( std::lround( x.dens * 100.0f ) ), std::uint32_t( std::lround( x.sheet * 32.0f ) ), 0u };
	J::Bits exact;
	J::put( exact, band );
	J::put( exact, x.base );
	J::put( exact, x.top );
	J::put( exact, x.dens );
	J::put( exact, x.sheet );
	J::put( exact, 0 );
	const In c = x;
	return s.update( jobs, qkey, exact, [band, c]() {
		J::Table t( V::kCalN );
		V::calibrate( g_shape.data(), WwCloudNoise::kShapeN, g_detail.data(), WwCloudNoise::kDetailN, band, c.base, c.top,
			c.dens, c.sheet, false, t.data() );
		return t;
	}, J::Mode::Async );
}

static bool sameBytes( const float * a, const float * b, int n )
{
	return std::memcmp( a, b, sizeof( float ) * size_t( n ) ) == 0;
}

int main()
{
	std::setvbuf( stdout, nullptr, _IONBF, 0 );
	g_shape = WwCloudNoise::shapeVolume();
	g_detail = WwCloudNoise::detailVolume();
	const int band = 1;	// the Mid band (3..5 km over the eye in CommonwealthOvercast)

	// A + C: a camera walk; the eye's z moves both heights by the same km
	const float eyes[] = { 0.075f, 0.075f, 0.080f, 0.800f, 0.800f, 0.0751f, 0.1500f, 0.1504f, 0.9000f };
	OldMemo old;
	J::Jobs jobs;
	J::Slot slot;
	bool allA = true, sawPending = false, keptOk = true, firstFresh = false;
	std::vector<float> prev;
	for ( size_t i = 0; i < sizeof( eyes ) / sizeof( eyes[0] ); i++ ) {
		const In x{ 3.0f + eyes[i], 5.0f + eyes[i], 0.6f, 0.0f };
		old.update( band, x );
		J::Got g = newUpdate( slot, jobs, band, x );
		if ( i == 0 )
			firstFresh = g == J::Got::Fresh && sameBytes( slot.table().data(), old.cal, V::kCalN );
		if ( g == J::Got::Pending ) {
			sawPending = true;
			keptOk = keptOk && prev.size() == size_t( V::kCalN ) && sameBytes( slot.table().data(), prev.data(), V::kCalN );
		}
		for ( int n = 0; slot.pending() && n < 4000; n++ ) {	// the settle: frames until the worker's table is in
			std::this_thread::sleep_for( std::chrono::milliseconds( 1 ) );
			newUpdate( slot, jobs, band, x );
		}
		const bool same = sameBytes( slot.table().data(), old.cal, V::kCalN );
		allA = allA && same;
		std::printf( "  step %zu eye %.4f km: old computes %d, jobs started %d, %s\n", i, double( eyes[i] ), old.computes,
			jobs.started(), same ? "same bytes" : "DIFFERENT" );
		prev = slot.table();
	}
	check( allA, "A: every settled step's table == the old memo's, byte for byte" );
	check( firstFresh, "C: the first table is waited for and right at once" );
	check( sawPending, "C: a later move is Pending while its worker runs" );
	check( keptOk, "C: while Pending the previous table stands in, unchanged" );

	// B: a quantum revisited with other exact inputs (the same 20 m key, 5 m apart)
	{
		OldMemo o;
		J::Jobs jb;
		J::Slot s;
		const In seq[] = { { 3.0000f, 5.0000f, 0.6f, 0.0f }, { 3.5000f, 5.5000f, 0.6f, 0.0f }, { 3.0050f, 5.0050f, 0.6f, 0.0f } };
		bool ok = true;
		for ( const In & x : seq ) {
			o.update( band, x );
			newUpdate( s, jb, band, x );
			for ( int n = 0; s.pending() && n < 4000; n++ ) {
				std::this_thread::sleep_for( std::chrono::milliseconds( 1 ) );
				newUpdate( s, jb, band, x );
			}
			ok = ok && sameBytes( s.table().data(), o.cal, V::kCalN );
		}
		check( ok, "B: a revisited quantum with other exact inputs gives the old memo's table (built from the new inputs)" );
	}

	// D: A/B/A/B starts 2 jobs, the old memo computes 4
	{
		OldMemo o;
		J::Jobs jb;
		J::Slot s;
		const In a{ 3.075f, 5.075f, 0.6f, 0.0f }, b{ 3.800f, 5.800f, 0.6f, 0.0f };
		bool ok = true;
		for ( const In & x : { a, b, a, b } ) {
			o.update( band, x );
			newUpdate( s, jb, band, x );
			for ( int n = 0; s.pending() && n < 4000; n++ ) {
				std::this_thread::sleep_for( std::chrono::milliseconds( 1 ) );
				newUpdate( s, jb, band, x );
			}
			ok = ok && sameBytes( s.table().data(), o.cal, V::kCalN );
		}
		std::printf( "  D: old computes %d, jobs started %d, hits %d\n", o.computes, jb.started(), jb.hits() );
		check( ok && o.computes == 4 && jb.started() == 2, "D: the settle row's camera and the path's cost one build each, once" );
	}

	// E: the Variety set, threads vs one after another
	{
		const V::VarVolumes vol{ g_shape.data(), WwCloudNoise::kShapeN, g_detail.data(), WwCloudNoise::kDetailN };
		V::Variety var;
		const float lo[3] = { 1.2f, 1.2f + 6.0f, 0.5f }, mid[3] = { 3.80f, 5.80f, 0.6f };
		std::vector<float> seq( ( V::kVarCal + 1 ) * V::kCalN ), par( ( V::kVarCal + 1 ) * V::kCalN );
		const auto t0 = std::chrono::steady_clock::now();
		for ( int k = 0; k <= V::kVarCal; k++ ) {
			if ( k < V::kVarCal )
				V::varCalibrate( vol, var, 0, V::varCalT( k ), lo[0], lo[1], lo[2], false, seq.data() + k * V::kCalN );
			else
				V::varCalibrate( vol, var, 1, 0.0f, mid[0], mid[1], mid[2], false, seq.data() + k * V::kCalN );
		}
		const double msSeq = std::chrono::duration<double, std::milli>( std::chrono::steady_clock::now() - t0 ).count();
		const auto t1 = std::chrono::steady_clock::now();
		std::vector<std::thread> th;
		for ( int k = 0; k <= V::kVarCal; k++ )
			th.emplace_back( [&, k]() {
				if ( k < V::kVarCal )
					V::varCalibrate( vol, var, 0, V::varCalT( k ), lo[0], lo[1], lo[2], false, par.data() + k * V::kCalN );
				else
					V::varCalibrate( vol, var, 1, 0.0f, mid[0], mid[1], mid[2], false, par.data() + k * V::kCalN );
			} );
		for ( std::thread & x : th )
			x.join();
		const double msPar = std::chrono::duration<double, std::milli>( std::chrono::steady_clock::now() - t1 ).count();
		std::printf( "  E/T: Variety set %.1f ms one after another (the old GL-thread cost), %.1f ms wall on 14 threads (a worker's)\n",
			msSeq, msPar );
		check( sameBytes( seq.data(), par.data(), int( seq.size() ) ), "E: the Variety set on threads == one after another, byte for byte" );
	}

	// T: what the GL thread pays on a later move: the slot's update returns at once (Pending)
	{
		J::Jobs jb;
		J::Slot s;
		newUpdate( s, jb, band, { 3.0f, 5.0f, 0.6f, 0.0f } );
		const auto t0 = std::chrono::steady_clock::now();
		const J::Got g = newUpdate( s, jb, band, { 3.7f, 5.7f, 0.6f, 0.0f } );
		const double ms = std::chrono::duration<double, std::milli>( std::chrono::steady_clock::now() - t0 ).count();
		std::printf( "  T: a band move costs the calling thread %.3f ms (%s); the old memo paid the whole build there\n", ms,
			g == J::Got::Pending ? "pending" : "not pending" );
		check( ms < 1.0, "T: a move after the first costs the calling thread < 1 ms" );
		for ( int n = 0; s.pending() && n < 4000; n++ ) {
			std::this_thread::sleep_for( std::chrono::milliseconds( 1 ) );
			newUpdate( s, jb, band, { 3.7f, 5.7f, 0.6f, 0.0f } );
		}
	}

	std::printf( "CLOUDCALJOBS %d checks, %d failures: %s\n", g_checks, g_fail, g_fail ? "FAIL" : "PASS" );
	return g_fail ? 1 : 0;
}
