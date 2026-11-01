#pragma once

/* lane PERFFIX1: where a per-frame setter's CPU goes. WW_CELL_UNI_PROF=1 (live): marks inside wwCellLightsUniforms
 * (site 0) and wwLookdevFogUniforms (site 1) add the time since the previous mark to a named part; wwCellUniFrame
 * prints one `uni prof:` line per site every 60 painted frames (us per call of each part, and its share). Off: one
 * branch on a cached bool per mark. */

#include <chrono>

namespace WwUniProf
{
constexpr int kParts = 32;
constexpr int kSites = 2;

struct Table
{
	const char * name[kParts] = {};
	double us[kParts] = {};
	long long calls = 0;
	const void * progs[256] = {};	// the distinct programs the calls set (calls / programs = the passes each one sees)
	int nProgs = 0;
	void prog( const void * p )
	{
		for ( int i = 0; i < nProgs; i++ )
			if ( progs[i] == p )
				return;
		if ( nProgs < 256 )
			progs[nProgs++] = p;
	}
};

Table & table( int site );	// celllights.cpp
bool on();	// celllights.cpp
void print();	// celllights.cpp: one line per site, then the sums restart

struct Marks
{
	int site;
	bool live;
	std::chrono::steady_clock::time_point t;
	explicit Marks( int s ) : site( s ), live( on() )
	{
		if ( live ) {
			t = std::chrono::steady_clock::now();
			table( site ).calls++;
		}
	}
	void prog( const void * p )
	{
		if ( live )
			table( site ).prog( p );
	}
	void at( int part, const char * name )
	{
		if ( !live || part < 0 || part >= kParts )
			return;
		const auto n = std::chrono::steady_clock::now();
		Table & T = table( site );
		T.name[part] = name;
		T.us[part] += std::chrono::duration<double, std::micro>( n - t ).count();
		t = n;
	}
};
}
