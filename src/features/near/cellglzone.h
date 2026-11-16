// Lane VKFLIGHT5 (2026-11-16): two pure pieces of the Vulkan view's opaque wiring (tests/vkframe/opaqueview.cpp gates them).
// Lane VKFLIGHT6: the zones' twin draws and GL's draws by the refusal before them (tests/vkframe/sortedview.cpp).
#pragma once

#include <algorithm>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace WwBl
{
/* The opaque twins' switch: WW_VKOBJ_SHADE unset follows the Vulkan view (WW_VKVIEW=1), as WW_VKOBJ_TEX does; "0" forces
 * them off, "1" on. Before VKFLIGHT5 only "1" turned them on, so the flight's Vulkan view drew every opaque shape on GL. */
inline bool shadeWanted( const char * shade, const char * view )
{
	auto one = []( const char * v ) { return v && v[0] == '1' && !v[1]; };
	return shade && *shade ? one( shade ) : one( view );
}

/* bungo 11-16: every GL draw in the Vulkan view is a bug. The GL draw calls of the passes with no route of their own,
 * counted at the GL call itself (never PassTime's countDraw): the opaque / sorted brackets (glscene.cpp), obscurance and
 * the overlays (glview.cpp) set the zone, the rest counts as "other". WwBl::frame() hands them to the view's guard
 * (`vk view gl:`) once a frame. Drawing thread only. */
enum GlZone { ZoneOther, ZoneOpaque, ZoneSorted, ZoneObscurance, ZoneOverlays, ZoneCount };
inline const char * glZoneName( int z )
{
	static const char * const n[ZoneCount] = { "gl other", "gl opaque", "gl sorted", "gl obscurance", "gl overlays" };
	return z >= 0 && z < ZoneCount ? n[z] : "gl ?";
}
struct GlZones
{
	long long draws[ZoneCount] = {};
	int cur = ZoneOther;
	long long vk[ZoneCount] = {};                     // lane VKFLIGHT6: the twins' draws in the zone
	std::string why;                                  // the refusal of the draw on hand (the twins ask before GL draws it)
	bool whyNamed = false;                            // a twin's own reason (else the program has no twin)
	std::map<std::string, long long> whyN[ZoneCount];  // GL's draw calls by the refusal before them
};
inline GlZones & glZones()
{
	static GlZones z;
	return z;
}
inline void countGl( long long calls )
{
	GlZones & z = glZones();
	z.draws[z.cur] += calls;
	static const std::string none( "no twin tried" );
	z.whyN[z.cur][z.why.empty() ? none : z.why] += calls;
	z.why.clear();
	z.whyNamed = false;
}
/* lane VKFLIGHT6: a twin refused the draw on hand. named = the twin's own reason (it took the program and could not draw
 * it); a weak one (the program has no twin here, the twins are off) never replaces a named one, and the first weak stays */
inline void zoneWhy( const std::string & w, bool named )
{
	GlZones & z = glZones();
	if ( named || ( !z.whyNamed && z.why.empty() ) ) {
		z.why = w;
		z.whyNamed = named;
	}
}
inline void zoneVk( long long draws )	// a twin drew the draw on hand
{
	GlZones & z = glZones();
	z.vk[z.cur] += draws;
	z.why.clear();
	z.whyNamed = false;
}
//! `vk <name>: drawn=N gl=G why: "<reason>" xK; ...`, the reasons with the most GL draws first (at most `top`)
inline std::string zoneLine( int zone, const char * name, int top = 6 )
{
	GlZones & z = glZones();
	std::vector<std::pair<long long, std::string>> v;
	for ( const auto & e : z.whyN[zone] )
		v.emplace_back( e.second, e.first );
	std::sort( v.begin(), v.end(), []( const std::pair<long long, std::string> & a, const std::pair<long long, std::string> & b ) {
		return a.first != b.first ? a.first > b.first : a.second < b.second;
	} );
	std::string s = std::string( "vk " ) + name + ": drawn=" + std::to_string( z.vk[zone] ) + " gl=" + std::to_string( z.draws[zone] );
	for ( int i = 0; i < int( v.size() ) && i < top; i++ )
		s += ( i ? "; \"" : " why: \"" ) + v[i].second + "\" x" + std::to_string( v[i].first );
	if ( int( v.size() ) > top )
		s += "; +" + std::to_string( int( v.size() ) - top ) + " more";
	return s;
}
inline void zoneReset()
{
	GlZones & z = glZones();
	for ( int i = 0; i < ZoneCount; i++ ) {
		z.draws[i] = z.vk[i] = 0;
		z.whyN[i].clear();
	}
	z.why.clear();
	z.whyNamed = false;
}
inline int glZoneSet( int zone )	// returns the zone before, for the bracket's end
{
	GlZones & z = glZones();
	const int prev = z.cur;
	z.cur = zone >= 0 && zone < ZoneCount ? zone : ZoneOther;
	return prev;
}
}  // namespace WwBl
