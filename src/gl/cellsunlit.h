#ifndef CELLSUNLIT_H
#define CELLSUNLIT_H

/* lane INTSKY1 (bungo 11-01, LATE ITEMs 30, 30e): does an interior get the real sun + sky through its openings
 * (`on`) and the live exterior weather inside (`weather`)? The rule, the same in NifSkope and Fusion Shaders:
 *   - a .recx Cells `sunlight` block's `enabled` (a boolean), else the WINNING CELL record's DATA flag 0x100;
 *   - its `weather` (a boolean), else `on`;
 *   - a key that is not a boolean counts as absent;
 *   - the red off keeps `weather` and turns `on` off (the batch's own Use Sky Lighting look: the weather's colours,
 *     no sun). Exteriors are always on (not decided here).
 * Pure header (no Qt): tests/cellsunlit checks it. */

namespace wwsunlit {

enum Key { Absent = -1, False = 0, True = 1 };	// a block key: absent / not a boolean = Absent

struct Decision
{
	bool on = false, weather = false;
	bool fromFile = false;
};

inline Decision decide( bool useSkyLightingFlag, bool haveBlock, int enabled, int weather, bool redOff )
{
	Decision d;
	d.on = useSkyLightingFlag;
	if ( haveBlock && enabled != Absent ) {
		d.on = enabled == True;
		d.fromFile = true;
	}
	d.weather = d.on;
	if ( haveBlock && weather != Absent ) {
		d.weather = weather == True;
		d.fromFile = true;
	}
	if ( redOff )
		d.on = false;
	return d;
}

}	// namespace wwsunlit

#endif // CELLSUNLIT_H
