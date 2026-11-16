// Lane VKFLIGHT5 (2026-11-16): two pure pieces of the Vulkan view's opaque wiring (tests/vkframe/opaqueview.cpp gates them).
#pragma once

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
}
inline int glZoneSet( int zone )	// returns the zone before, for the bracket's end
{
	GlZones & z = glZones();
	const int prev = z.cur;
	z.cur = zone >= 0 && zone < ZoneCount ? zone : ZoneOther;
	return prev;
}
}  // namespace WwBl
