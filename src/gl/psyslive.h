#ifndef PSYSLIVE_H
#define PSYSLIVE_H

/* lane GLARESB2: a live set of GLAREBLOW2's red WW_CELL_PSYS_OBJ_RED restarts the particle simulation.
 *
 * PSysSimController::updateTime reads the red each step (WW_SWITCH_STATIC), so the pull points follow a set at once.
 * The PARTICLES do not: every bit the red's step dragged off toward the unplaced object frame (the dust devils' long
 * pale streaks over Concord) stays in the system after the red goes off again, until it dies -- and a system with no
 * NiPSysAgeDeathModifier never kills one. A fresh open has none of them. So the first step after the red was set or
 * unset (any value, either way) empties the system and takes the step a freshly built controller takes first
 * (lastTime as constructed: one clamped step from empty). Nothing else moves the stamp: with no live set of this name
 * the simulation runs exactly as before. Unit: tests/psyslive (twin of the clock + spherical pull, red = no restart). */

#include "wwswitch.h"

//! The clock a freshly built PSysSimController starts from (its first step is one clamped dt from empty)
constexpr float kWwPsysFreshLastTime = -1.0e30f;

//! True once after WW_CELL_PSYS_OBJ_RED was set or unset live since `seen` (start it at 0); `seen` follows the generation
inline bool wwPsysObjRedMoved( quint64 & seen )
{
	return wwSwitchMovedFor( seen, { "WW_CELL_PSYS_OBJ_RED" } );
}

#endif // PSYSLIVE_H
