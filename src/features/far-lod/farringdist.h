#ifndef FARRINGDIST_H
#define FARRINGDIST_H

/* Lane FARPRESET1 (2026-11-02, GFXR-GEO G2): the far ring distances of the player's preset. Pure C++ (tests/features/far-lod/farringdist,
 * g++ alone).
 * Before: the far LOD rings reached the Ultra distances on every preset (60000 / 90000 / 110000 / 250000).
 * Now: [TerrainManager] fBlockLevel0Distance / fBlockLevel1Distance / fBlockLevel2Distance / fBlockMaximumDistance from
 * the game's ini chain (the preset writes them into Fallout4Prefs.ini), as the game does. The game folder's preset inis
 * (read 11-02): Low 15000/25000/75000/100000, Medium 20000/32000/80000/100000, High 30000/60000/110000/180000,
 * Ultra 60000/90000/110000/250000. A key no ini sets keeps today's Ultra value. */

#include "gameinikey.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <string>

namespace FarRingDist
{

struct Dist
{
	float l0 = 60000.0f, l1 = 90000.0f, l2 = 110000.0f, max = 250000.0f;
};

//! Low, Medium, High, Ultra as the game folder's preset inis write them
constexpr float kPreset[4][4] = {
	{ 15000.0f, 25000.0f, 75000.0f, 100000.0f },
	{ 20000.0f, 32000.0f, 80000.0f, 100000.0f },
	{ 30000.0f, 60000.0f, 110000.0f, 180000.0f },
	{ 60000.0f, 90000.0f, 110000.0f, 250000.0f },
};

inline Dist preset( int p )
{
	Dist d;
	if ( p < 0 || p > 3 )
		return d;
	d.l0 = kPreset[p][0];
	d.l1 = kPreset[p][1];
	d.l2 = kPreset[p][2];
	d.max = kPreset[p][3];
	return d;
}

//! "low" / "medium" / "high" / "ultra" (any case) or "0".."3" -> 0..3; else -1
inline int presetByName( std::string s )
{
	for ( char & c : s )
		c = char( std::tolower( static_cast<unsigned char>( c ) ) );
	static const char * names[4] = { "low", "medium", "high", "ultra" };
	for ( int k = 0; k < 4; k++ )
		if ( s == names[k] || s == std::string( 1, char( '0' + k ) ) )
			return k;
	return -1;
}

/*! One ini line into `d`; `section` tracks the section (GameIniKey). Returns a bit mask of the keys this line set
 *  (1 l0, 2 l1, 4 l2, 8 max). */
inline int line( const std::string & raw, std::string & section, Dist & d )
{
	static const char * want = "[terrainmanager]";
	if ( GameIniKey::floatLine( raw, want, "fblocklevel0distance", section, d.l0 ) )
		return 1;
	if ( GameIniKey::floatLine( raw, want, "fblocklevel1distance", section, d.l1 ) )
		return 2;
	if ( GameIniKey::floatLine( raw, want, "fblocklevel2distance", section, d.l2 ) )
		return 4;
	if ( GameIniKey::floatLine( raw, want, "fblockmaximumdistance", section, d.max ) )
		return 8;
	return 0;
}

//! A value <= 0 (or not a number) keeps today's Ultra one; each ring reaches at least as far as the one inside it.
inline Dist ordered( Dist d )
{
	const Dist u;
	auto ok = []( float v, float fb ) { return std::isfinite( v ) && v > 0.0f ? v : fb; };
	d.l0 = ok( d.l0, u.l0 );
	d.l1 = std::max( ok( d.l1, u.l1 ), d.l0 );
	d.l2 = std::max( ok( d.l2, u.l2 ), d.l1 );
	d.max = std::max( ok( d.max, u.max ), d.l2 );
	return d;
}

//! A distance in whole cells from the block centre (cellfarlod.cpp's cellsTo)
inline int cellsTo( float d )
{
	return int( std::ceil( d / 4096.0f ) );
}

} // namespace FarRingDist

#endif // FARRINGDIST_H
