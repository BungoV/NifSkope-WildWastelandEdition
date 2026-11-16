// Lane VKFLIGHT4 (2026-11-16): offline gate of the Vulkan view's GL guard (src/features/vkframe/vkglguard.inc).
// Bars (written before run 1, from the coordinator's brief): a fake frame with 2 GL zones (zoneA 3 draws, zoneB 1) must
// print "vk view gl: 4 draws, 2 passes (top: zoneA 3, zoneB 1)"; the next take (nothing added) must print
// "vk view gl: 0 draws, 0 passes" (ready line); 10 zones list only the top 8; two threads adding 10000 each sum exactly.
// Red (argv[1] == "red"): the fake frame feeds only zoneA, so the "2 passes" check must FAIL.
#include <algorithm>
#include <cstdio>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace WwVkPresent
{
void glGuard( const char * zone, long long draws );
std::string glGuardTake( long long * draws, int * passes );
}
#include "../../src/features/vkframe/vkglguard.inc"

static int checks = 0, fails = 0;
static void check( bool ok, const std::string & what, const std::string & got )
{
	checks++;
	if ( !ok )
		fails++;
	std::printf( "  %s %s (got \"%s\")\n", ok ? "ok  " : "FAIL", what.c_str(), got.c_str() );
}

int main( int argc, char ** argv )
{
	const bool red = argc > 1 && std::string( argv[1] ) == "red";
	using namespace WwVkPresent;
	glGuardTake( nullptr, nullptr );  // empty start

	// the fake frame: two GL zones
	glGuard( "zoneA", 1 );
	glGuard( "zoneA", 2 );
	if ( !red )
		glGuard( "zoneB", 1 );
	long long n = -1;
	int m = -1;
	std::string s = glGuardTake( &n, &m );
	check( m == 2, "a fake frame with 2 GL zones says 2 passes", s );
	check( s == "vk view gl: 4 draws, 2 passes (top: zoneA 3, zoneB 1)", "the line reads 4 draws, 2 passes, zoneA first", s );
	check( n == 4, "draws out = 4", std::to_string( n ) );

	s = glGuardTake( &n, &m );
	check( s == "vk view gl: 0 draws, 0 passes" && n == 0 && m == 0, "the next window (nothing on GL) is the ready line", s );

	for ( int i = 0; i < 10; i++ )
		glGuard( ( "z" + std::to_string( i ) ).c_str(), i + 1 );
	s = glGuardTake( &n, &m );
	check( m == 10 && n == 55 && s.find( "z9 10, z8 9" ) != std::string::npos && s.find( "z1 2" ) == std::string::npos
			&& s.find( "z2 3)" ) != std::string::npos,
		"10 zones: 10 passes, 55 draws, the top 8 listed by count", s );

	std::thread t1( [] { for ( int i = 0; i < 10000; i++ ) glGuard( "worker", 1 ); } );
	std::thread t2( [] { for ( int i = 0; i < 10000; i++ ) glGuard( "worker", 1 ); } );
	t1.join();
	t2.join();
	s = glGuardTake( &n, &m );
	check( n == 20000 && m == 1, "two threads adding 10000 each sum to 20000 in 1 pass", s );

	std::printf( "GLGUARD %d checks, %d failures%s\n", checks, fails, red ? " (red: one zone)" : "" );
	std::printf( fails ? "GLGUARD FAIL\n" : "GLGUARD OK\n" );
	return fails ? 1 : 0;
}
