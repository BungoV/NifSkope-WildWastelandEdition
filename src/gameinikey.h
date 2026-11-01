#ifndef GAMEINIKEY_H
#define GAMEINIKEY_H

/* Lane PERFFIX4 (2026-11-01): one float key of the game's inis, the way the game reads them (the section, then
 * `key=value`, case-insensitive; a later line or file wins). Pure C++ (tests/gameinikey, g++ alone).
 * First user: the grass fade. The game's presets write it into Fallout4Prefs.ini [Grass]: fGrassStartFadeDistance
 * Low 1000, Medium 2300, High 4000, Ultra 7000 (the game folder's Low/Medium/High/Ultra.ini, read 11-01);
 * fGrassFadeRange is not in those files, so it keeps the engine default (1000, as cellgrass.cpp had it). */

#include <cctype>
#include <cstdlib>
#include <string>

namespace GameIniKey
{

//! One ini line: tracks the section in `section` (lower case, with brackets); true when the line sets `key` (lower case)
//! inside `want` (lower case, with brackets), the value in `out`.
inline bool floatLine( const std::string & raw, const char * want, const char * key, std::string & section, float & out )
{
	const size_t a = raw.find_first_not_of( " \t" ), b = raw.find_last_not_of( " \t\r\n" );
	if ( a == std::string::npos )
		return false;
	std::string l = raw.substr( a, b - a + 1 );
	for ( char & c : l )
		c = char( std::tolower( static_cast<unsigned char>( c ) ) );
	if ( l[0] == '[' ) {
		section = l;
		return false;
	}
	if ( l[0] == ';' || l[0] == '#' || section != want )
		return false;
	const size_t eq = l.find( '=' );
	if ( eq == std::string::npos )
		return false;
	std::string k = l.substr( 0, eq );
	k.erase( k.find_last_not_of( " \t" ) + 1 );
	if ( k != key )
		return false;
	const char * v = l.c_str() + eq + 1;
	char * end = nullptr;
	const float f = std::strtof( v, &end );
	if ( end == v )
		return false;
	out = f;
	return true;
}

} // namespace GameIniKey

#endif // GAMEINIKEY_H
