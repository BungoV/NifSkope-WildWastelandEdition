// lane CLOUDSTEPS1: the march's step count per tier through the REAL rule (src/gl/cloudvolmath.h marchSteps) for this
// process's WW_CLOUDVOL_MORESTEPS / WW_CLOUDVOL_TOPCOLUMN (defaultOnSwitch on the trimmed, lower-cased value, as
// cloudvol.cpp cs() reads them; the more steps need the column on). No GL, no Qt; tests/spells/cloudsteps_check.py builds it.
//   cloudsteps_dump <tier> <presetSteps>   -> "steps <n> more <0|1>"
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

int main( int argc, char ** argv )
{
	if ( argc < 3 )
		return 2;
	const std::string m = norm( "WW_CLOUDVOL_MORESTEPS" ), t = norm( "WW_CLOUDVOL_TOPCOLUMN" );
	const bool more = WwCloudVolMath::defaultOnSwitch( m.c_str() ) && WwCloudVolMath::defaultOnSwitch( t.c_str() );
	std::printf( "steps %d more %d\n", WwCloudVolMath::marchSteps( std::atoi( argv[1] ), std::atoi( argv[2] ), more ),
				 more ? 1 : 0 );
	return 0;
}
