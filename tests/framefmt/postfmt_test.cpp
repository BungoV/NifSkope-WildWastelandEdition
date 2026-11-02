// lane PRESETPERF1 (FRAMEFMT1-B): the measure copy + bloom quarter format rule (src/gl/cellhdrformat.h wwPostFormatOf).
//   g++ -std=c++17 -I<tree>/src tests/framefmt/postfmt_test.cpp -o postfmt_test
// Red: -DPOSTFMT_RED puts back the code before the lane (RGBA32F whatever was asked); it must fail.
#include <cstdio>
#include <cstring>

#include "gl/cellhdrformat.h"
#ifdef POSTFMT_RED
#define wwPostFormatOf wwPostFormatBefore
inline WwHdrFormatRule wwPostFormatBefore( const char *, bool red )
{
	WwHdrFormatRule h;
	h.gl = 0x8814; h.bytes = 16; h.name = "rgba32f";
	h.red = red;
	return h;
}
#endif

static int fails = 0;
static void check( const char * what, const char * v, bool red, unsigned gl, int bytes, bool isRed )
{
	const WwHdrFormatRule h = wwPostFormatOf( v, red );
	const bool ok = h.gl == gl && h.bytes == bytes && h.red == isRed;
	std::printf( "%s %s: gl 0x%04X bytes %d %s%s\n", ok ? "ok  " : "FAIL", what, h.gl, h.bytes, h.name, h.red ? " (red)" : "" );
	fails += ok ? 0 : 1;
}

int main()
{
	std::setvbuf( stdout, nullptr, _IONBF, 0 );
	check( "default (unset) = today's RGBA32F", nullptr, false, 0x8814, 16, false );
	check( "unknown word = default", "fast", false, 0x8814, 16, false );
	check( "r11g11b10f is not offered here (needs alpha 0 clears)", "r11g11b10f", false, 0x8814, 16, false );
	check( "asked rgba16f", "rgba16f", false, 0x881A, 8, false );
	check( "asked rgba32f", "rgba32f", false, 0x8814, 16, false );
	check( "red beats rgba16f", "rgba16f", true, 0x8814, 16, true );
	// the measure copy at 1080p: full level + mips to the quarter (4/3 of level 0, two levels: 1 + 1/4 + 1/16)
	const double lv = 1.0 + 0.25 + 0.0625;
	const double mb16 = 1920.0 * 1080.0 * lv * wwPostFormatOf( "rgba16f", false ).bytes / ( 1024.0 * 1024.0 );
	const double mb32 = 1920.0 * 1080.0 * lv * wwPostFormatOf( nullptr, false ).bytes / ( 1024.0 * 1024.0 );
	const bool half = mb16 * 2.0 == mb32;
	std::printf( "%s 1080p measure copy: rgba16f %.1f MB, default %.1f MB\n", half ? "ok  " : "FAIL", mb16, mb32 );
	fails += half ? 0 : 1;
	std::printf( "postfmt: %d fail(s)\n", fails );
	return fails ? 1 : 0;
}
