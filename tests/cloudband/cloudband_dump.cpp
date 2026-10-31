// lane CLOUDBAND1: Cloud Accumulation's CPU side for the offline gate (tests/spells/cloudband_check.py builds this with
// g++). Prints one "acc <cycle> <weight>" line per refresh cycle 0..15 (cloudvolmath.h accumAlpha, the weight cloudvol.cpp
// hands cloud_resolve_a.frag as cvAccAlpha).
// lane CLOUDBAND2: and "default accum <0|1> ambdark <0|1>": what cloudvol.cpp cs() turns on for this process's
// WW_CLOUDVOL_ACCUM / WW_CLOUDVOL_AMBDARK (cloudvolmath.h defaultOnSwitch on the trimmed, lower-cased value). The
// checkers draw their default arm from this line, so unset = the shipped default and =0 = the old path (the red).
#include "gl/cloudvolmath.h"

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <string>

static std::string norm( const char * name )
{
	const char * v = std::getenv( name );
	std::string s = v ? v : "";
	size_t a = 0, b = s.size();
	while ( a < b && std::isspace( static_cast<unsigned char>( s[a] ) ) )
		a++;
	while ( b > a && std::isspace( static_cast<unsigned char>( s[b - 1] ) ) )
		b--;
	s = s.substr( a, b - a );
	for ( char & c : s )
		c = char( std::tolower( static_cast<unsigned char>( c ) ) );
	return s;
}

int main()
{
	for ( long long c = 0; c < 16; c++ )
		std::printf( "acc %lld %.9g\n", c, double( WwCloudVolMath::accumAlpha( c ) ) );
	const std::string a = norm( "WW_CLOUDVOL_ACCUM" ), d = norm( "WW_CLOUDVOL_AMBDARK" );
	std::printf( "default accum %d ambdark %d\n", WwCloudVolMath::defaultOnSwitch( a.c_str() ) ? 1 : 0,
				 WwCloudVolMath::defaultOnSwitch( d.c_str() ) ? 1 : 0 );
	// lane CLOUD6g: Move Clamp / Change Clamp for this env (cloudvol.cpp cs()) and Change Clamp's threshold, its own line
	// (the "default" line's readers split it by position)
	const std::string mc = norm( "WW_CLOUDVOL_MOVECLAMP" ), cc = norm( "WW_CLOUDVOL_CHANGECLAMP" );
	std::printf( "cloud6g moveclamp %d changeclamp %d reset %.9g sd %.9g\n", WwCloudVolMath::defaultOnSwitch( mc.c_str() ) ? 1 : 0,
				 WwCloudVolMath::defaultOnSwitch( cc.c_str() ) ? 1 : 0, double( WwCloudVolMath::kChangeReset ),
				 double( WwCloudVolMath::kChangeResetSd ) );
	// lane CLOUD6h: History Age / Lit Clamp for this env and their constants, their own line
	const std::string ha = norm( "WW_CLOUDVOL_HISTAGE" ), lc = norm( "WW_CLOUDVOL_LITCLAMP" );
	std::printf( "cloud6h histage %d old %.9g k %.9g litclamp %d reset %.9g floor %.9g\n",
				 WwCloudVolMath::defaultOnSwitch( ha.c_str() ) ? 1 : 0, double( WwCloudVolMath::kHistAgeOld ),
				 double( WwCloudVolMath::kAgeChangeK ), WwCloudVolMath::defaultOnSwitch( lc.c_str() ) ? 1 : 0,
				 double( WwCloudVolMath::kLitReset ), double( WwCloudVolMath::kLitFloor ) );
	return 0;
}
