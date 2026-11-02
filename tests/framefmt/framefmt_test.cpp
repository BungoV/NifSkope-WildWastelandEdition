// lane FRAMEFMT1: the linear HDR frame's format rule (src/gl/cellhdrformat.h). Pure header, no Qt:
//   g++ -std=c++17 -I<tree>/src tests/framefmt/framefmt_test.cpp -o framefmt_test
// Red: -DFRAMEFMT_RED puts back the rule before the lane (RGBA32F unless rgba16f was asked); it must fail.
#include <cstdio>
#include <cstring>

#ifndef FRAMEFMT_RED
#include "gl/cellhdrformat.h"
#else
struct WwHdrFormatRule
{
	unsigned gl = 0x8814;
	int bytes = 16;
	const char * name = "rgba32f";
	bool red = false;
};
inline WwHdrFormatRule wwHdrFormatOf( const char * v, bool red )
{
	WwHdrFormatRule h;
	h.red = red;
	if ( red )
		return h;
	if ( v && std::strcmp( v, "rgba16f" ) == 0 ) {
		h.gl = 0x881A; h.bytes = 8; h.name = "rgba16f";
	} else if ( v && std::strcmp( v, "r11g11b10f" ) == 0 ) {
		h.gl = 0x8C3A; h.bytes = 4; h.name = "r11g11b10f";
	}
	return h;
}
#endif

static int fails = 0;
static void check( const char * what, const char * v, bool red, unsigned gl, int bytes, const char * name, bool isRed )
{
	const WwHdrFormatRule h = wwHdrFormatOf( v, red );
	const bool ok = h.gl == gl && h.bytes == bytes && std::strcmp( h.name, name ) == 0 && h.red == isRed;
	std::printf( "%s %s: gl 0x%04X bytes %d %s%s\n", ok ? "ok  " : "FAIL", what, h.gl, h.bytes, h.name, h.red ? " (red)" : "" );
	fails += ok ? 0 : 1;
}

int main()
{
	std::setvbuf( stdout, nullptr, _IONBF, 0 );
	check( "default (unset)", nullptr, false, 0x881A, 8, "rgba16f", false );
	check( "default (unknown word)", "fast", false, 0x881A, 8, "rgba16f", false );
	check( "asked rgba16f", "rgba16f", false, 0x881A, 8, "rgba16f", false );
	check( "asked rgba32f (the old frame)", "rgba32f", false, 0x8814, 16, "rgba32f", false );
	check( "asked r11g11b10f (cost arm)", "r11g11b10f", false, 0x8C3A, 4, "r11g11b10f", false );
	check( "red, unset", nullptr, true, 0x8814, 16, "rgba32f", true );
	check( "red beats rgba16f", "rgba16f", true, 0x8814, 16, "rgba32f", true );
	check( "red beats r11g11b10f", "r11g11b10f", true, 0x8814, 16, "rgba32f", true );
	// the bytes a 1080p 4x MSAA frame takes: half of RGBA32F's
	const double mb16 = 1920.0 * 1080.0 * 4 * wwHdrFormatOf( nullptr, false ).bytes / ( 1024.0 * 1024.0 );
	const double mb32 = 1920.0 * 1080.0 * 4 * wwHdrFormatOf( nullptr, true ).bytes / ( 1024.0 * 1024.0 );
	const bool half = mb16 * 2.0 == mb32;
	std::printf( "%s 1080p 4x colour: default %.1f MB, red %.1f MB\n", half ? "ok  " : "FAIL", mb16, mb32 );
	fails += half ? 0 : 1;
	std::printf( "framefmt: %d fail(s)\n", fails );
	return fails ? 1 : 0;
}
