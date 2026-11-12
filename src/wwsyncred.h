#pragma once

#include "wwswitch.h"

/* lane PERFFIX6 (PHASE 2, item 1): the per-frame CPU-GPU waits are polls. The exposure and sky read-backs test their fence
 * with a zero timeout (not done: the frame keeps the last value and the read waits for the next frame), and the GPU
 * timers read only a query that is already available. Red ^WW_SYNC_RED=1: every one of them waits as before. */
inline bool wwSyncRed()
{
	WW_SWITCH_STATIC( bool, r, wwEnvInt( "WW_SYNC_RED" ) == 1 );
	return r;
}
