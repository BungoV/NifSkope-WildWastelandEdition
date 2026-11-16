// Lane VKFLIGHT5 (2026-11-16): the opaque twins' switch and the GL zone count (src/features/near/cellglzone.h).
// Usage: opaqueview [red]   red = the old rule (only WW_VKOBJ_SHADE=1 turns the twins on): must FAIL.
#include "features/near/cellglzone.h"

#include <cstdio>
#include <cstring>

static bool oldRule( const char * shade, const char * ) { return shade && !std::strcmp( shade, "1" ); }

int main( int argc, char ** argv )
{
	const bool red = argc > 1 && !std::strcmp( argv[1], "red" );
	struct Row { const char * shade, * view; bool want; const char * what; };
	const Row rows[] = {
		{ nullptr, "1", true, "unset + the view on Vulkan (the flight): on" },
		{ "", "1", true, "empty + the view on Vulkan: on" },
		{ "0", "1", false, "WW_VKOBJ_SHADE=0 + the view on: off (forced)" },
		{ nullptr, nullptr, false, "unset + the view off: off" },
		{ nullptr, "0", false, "unset + WW_VKVIEW=0: off" },
		{ "1", nullptr, true, "WW_VKOBJ_SHADE=1 + the view off: on (the old opt-in)" },
		{ "1", "1", true, "both 1: on" },
		{ "10", "1", false, "WW_VKOBJ_SHADE=10: off (only 1 is on)" },
	};
	int fails = 0, n = 0;
	for ( const Row & r : rows ) {
		const bool got = red ? oldRule( r.shade, r.view ) : WwBl::shadeWanted( r.shade, r.view );
		n++;
		std::printf( "  %s shade=%s view=%s want=%d got=%d  %s\n", got == r.want ? "ok  " : "FAIL", r.shade ? r.shade : "(unset)",
			r.view ? r.view : "(unset)", int( r.want ), int( got ), r.what );
		fails += got != r.want;
	}
	// the zone count: a bracket's draws land in its zone, the bracket's end gives the zone before back
	WwBl::GlZones & z = WwBl::glZones();
	WwBl::glDrawn( 2 );
	const int prev = WwBl::glZoneSet( WwBl::ZoneOpaque );
	WwBl::glDrawn( 3 );
	WwBl::glDrawn( 4 );
	const int inner = WwBl::glZoneSet( WwBl::ZoneSorted );
	WwBl::glDrawn( 5 );
	WwBl::glZoneSet( inner );
	WwBl::glZoneSet( prev );
	WwBl::glDrawn( 1 );
	const bool zOk = z.draws[WwBl::ZoneOther] == 3 && z.draws[WwBl::ZoneOpaque] == 7 && z.draws[WwBl::ZoneSorted] == 5
		&& z.cur == WwBl::ZoneOther && !std::strcmp( WwBl::glZoneName( WwBl::ZoneOpaque ), "gl opaque" );
	std::printf( "  %s zones other=%lld (want 3) opaque=%lld (want 7) sorted=%lld (want 5) cur=%d (want 0)\n", zOk ? "ok  " : "FAIL",
		z.draws[WwBl::ZoneOther], z.draws[WwBl::ZoneOpaque], z.draws[WwBl::ZoneSorted], z.cur );
	fails += !zOk;
	std::printf( "OPAQUEVIEW %s (%d rows compared, %d failed)\n", fails ? "FAILED" : "OK", n + 1, fails );
	return fails ? 1 : 0;
}
