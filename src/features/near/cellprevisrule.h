#ifndef CELLPREVISRULE_H
#define CELLPREVISRULE_H

/* Lane PERFFIX4 (2026-11-01): when the game's previs applies in the cell view. Pure C++, no Qt, so
 * tests/features/near/previsrule tests it with g++ alone.
 *
 * 1. Interiors take rooms, portals and occlusion planes ALWAYS (the game draws them that way; UMBRA1 measured the
 *    picture identical at Vault111Cryo, -51 % calls). Exteriors keep the Previs row (default off): our Hi-Z culls
 *    them. WW_CELL_PREVIS=0 still turns previs off everywhere; WW_CELL_PREVIS_INT=0 is the old rule (interiors follow
 *    the row too).
 * 2. The Umbra tome is used only where it is not STALE. The tome was baked against the refs the plugin that wrote the
 *    cell's previs fields (XPRI / VISI on the winning CELL record) saw. A later plugin that adds, moves, deletes or
 *    disables refs in the cell (ELFX: 80,995 new refs over 209 interior cells it gives copied XPRI/VISI) leaves the
 *    tome culling against geometry that is no longer there: refused. No previs fields on the winning CELL = no writer
 *    = stale (the game itself has no previs then).
 * 3. PREVISSTALE2 (manager ruling 11-01): refs a later plugin only ADDS keep the tome (it cannot cull what it does not
 *    know). An override after the writer counts only if its placement differs from the version the writer saw: DATA
 *    position/rotation, XSCL scale, or enable state (deleted / disabled flags, XESP), each read from its own plugin
 *    file (EsmWorld::refSources). A light-radius or any other edit does not count. WW_CELL_UMBRA_RED=anyedit is the
 *    previous rule (every add and every override after the writer = stale). */

#include <cstdint>
#include <vector>

namespace WwPrevisRule
{

//! One ref of the cell: the plugin that owns its form id, the plugin of its winning version, its record flags.
//! `settled`: the plugin of the first version in the trailing run of versions whose placement (DATA, XSCL, enable
//! state) equals the winner's; kUnknown = not compared (counts as changed by the winner's own plugin).
struct RefSrc
{
	static constexpr std::uint32_t kUnknown = 0xFFFFFFFFu;
	std::uint32_t owner = 0, src = 0;
	bool deleted = false, disabled = false;
	std::uint32_t settled = kUnknown;
};

struct Stale
{
	bool stale = true;
	int added = 0, overridden = 0, deleted = 0, disabled = 0;
	int moved = 0;	//!< PREVISSTALE2: overrides whose placement differs from the version the writer saw
	std::uint32_t firstLater = 0xFFFFFFFFu;	//!< the first plugin after the writer that touched a ref
	const char * why = "no previs fields on the winning CELL";
};

/*! `hasFields`: the winning CELL record carries XPRI or VISI; `writer`: that record's plugin (load-order index).
 *  `red` (WW_CELL_UMBRA_RED=stale): the old rule, every tome used. `anyEdit` (WW_CELL_UMBRA_RED=anyedit): the
 *  pre-PREVISSTALE2 rule, every later add or override is stale. */
inline Stale tomeStale( bool hasFields, std::uint32_t writer, const std::vector<RefSrc> & refs, bool red = false,
	bool anyEdit = false )
{
	Stale s;
	if ( !hasFields && !red )
		return s;
	for ( const RefSrc & r : refs ) {
		if ( r.src <= writer && r.owner <= writer )
			continue;
		bool counts = true;
		if ( r.owner > writer ) {
			s.added++;
			counts = anyEdit;
		} else {
			s.overridden++;
			const std::uint32_t settled = r.settled == RefSrc::kUnknown ? r.src : r.settled;
			const bool moved = settled > writer;
			s.moved += moved ? 1 : 0;
			counts = anyEdit || moved;
			s.deleted += moved && r.deleted ? 1 : 0;
			s.disabled += moved && r.disabled ? 1 : 0;
		}
		const std::uint32_t p = r.owner > writer ? r.owner : r.src;
		if ( counts && p < s.firstLater )
			s.firstLater = p;
	}
	if ( red ) {
		s.stale = false;
		s.why = "RED=stale: tome used whatever later plugins did";
		return s;
	}
	if ( anyEdit ) {
		s.stale = s.added + s.overridden > 0;
		s.why = s.stale ? "RED=anyedit: a later plugin added or overrode refs" : "no later plugin touched its refs";
		return s;
	}
	s.stale = s.moved > 0;
	s.why = s.stale ? "a later plugin moved, scaled, deleted or disabled refs the tome knows"
		: ( s.added + s.overridden > 0 ? "later plugins only added refs or edited non-placement fields"
			: "no later plugin touched its refs" );
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
