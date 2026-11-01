#ifndef CELLPREVISRULE_H
#define CELLPREVISRULE_H

/* Lane PERFFIX4 (2026-11-01): when the game's previs applies in the cell view. Pure C++, no Qt, so
 * tests/previsrule tests it with g++ alone.
 *
 * 1. Interiors take rooms, portals and occlusion planes ALWAYS (the game draws them that way; UMBRA1 measured the
 *    picture identical at Vault111Cryo, -51 % calls). Exteriors keep the Previs row (default off): our Hi-Z culls
 *    them. WW_CELL_PREVIS=0 still turns previs off everywhere; WW_CELL_PREVIS_INT=0 is the old rule (interiors follow
 *    the row too).
 * 2. The Umbra tome is used only where it is not STALE. The tome was baked against the refs the plugin that wrote the
 *    cell's previs fields (XPRI / VISI on the winning CELL record) saw. A later plugin that adds, moves, deletes or
 *    disables refs in the cell (ELFX: 80,995 new refs over 209 interior cells it gives copied XPRI/VISI) leaves the
 *    tome culling against geometry that is no longer there: refused. Any winning REFR version from a plugin after the
 *    writer counts (an override is counted as a move: conservative, it may only change a light's radius). No previs
 *    fields on the winning CELL = no writer = stale (the game itself has no previs then). */

#include <cstdint>
#include <vector>

namespace WwPrevisRule
{

//! One ref of the cell: the plugin that owns its form id, the plugin of its winning version, its record flags.
struct RefSrc
{
	std::uint32_t owner = 0, src = 0;
	bool deleted = false, disabled = false;
};

struct Stale
{
	bool stale = true;
	int added = 0, overridden = 0, deleted = 0, disabled = 0;
	std::uint32_t firstLater = 0xFFFFFFFFu;	//!< the first plugin after the writer that touched a ref
	const char * why = "no previs fields on the winning CELL";
};

/*! `hasFields`: the winning CELL record carries XPRI or VISI; `writer`: that record's plugin (load-order index).
 *  `red` (WW_CELL_UMBRA_RED=stale): the old rule, every tome used. */
inline Stale tomeStale( bool hasFields, std::uint32_t writer, const std::vector<RefSrc> & refs, bool red = false )
{
	Stale s;
	if ( !hasFields && !red )
		return s;
	for ( const RefSrc & r : refs ) {
		if ( r.src <= writer && r.owner <= writer )
			continue;
		if ( r.owner > writer )
			s.added++;
		else
			s.overridden++;
		s.deleted += r.deleted ? 1 : 0;
		s.disabled += r.disabled ? 1 : 0;
		const std::uint32_t p = r.owner > writer ? r.owner : r.src;
		if ( p < s.firstLater )
			s.firstLater = p;
	}
	if ( red ) {
		s.stale = false;
		s.why = "RED=stale: tome used whatever later plugins did";
		return s;
	}
	s.stale = s.added + s.overridden > 0;
	s.why = s.stale ? "a later plugin added, moved, deleted or disabled refs" : "no later plugin touched its refs";
	return s;
}

/*! Previs (rooms, portals, occluders) for one document. `rowOn`: the Previs row or its pin; `pinnedOff`:
 *  WW_CELL_PREVIS=0; `interiorRule`: WW_CELL_PREVIS_INT is not "0". */
inline bool previsFor( bool rowOn, bool pinnedOff, bool interior, bool interiorRule )
{
	if ( pinnedOff )
		return false;
	if ( interior && interiorRule )
		return true;
	return rowOn;
}

//! The tome for one document: previs applies, the Umbra switch is on, and the tome is fresh (or the red says use it).
inline bool tomeFor( bool previs, bool umbraOn, bool stale, bool staleRed )
{
	return previs && umbraOn && ( !stale || staleRed );
}

} // namespace WwPrevisRule

#endif // CELLPREVISRULE_H
