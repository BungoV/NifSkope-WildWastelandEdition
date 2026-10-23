#ifndef CELLLOOKS_H
#define CELLLOOKS_H

/* lane GAPLOOK1 (10-23): the look switches of the gap sweep (notes/GAPS_20261023.md section 5). Each ships Off
 * (= the look before it), has a Show menu row, and an env pin that wins (read again on a live set).
 * Rows, pins, reds and costs: WW_CHANGES "GAPLOOK1". */

#include <QString>

enum class WwLook : int
{
	AirClusters = 0,	// LIGHTSYS-4: the fog reads the surface light clusters (WW_CELL_AIR_CLUSTERS)
	Count
};

//! the row's label, the pin's name, the QSettings key
const char * wwLookLabel( WwLook l );
const char * wwLookPinName( WwLook l );
bool wwLookOn( WwLook l );
bool wwLookPinned( WwLook l );
void wwLookSet( WwLook l, bool on );

/* LIGHTSYS-3: the lamp-shadow budget. 0 Off (the 16 nearest shadow lights, as before); else at most N slots, ranked
 * by brightness x radius / distance with 20 % hysteresis; a light's shadow fades out over the last 25 % of score
 * above the cut and by camera distance (kShadowFadeBegin..End), so a demoted light never pops.
 * Row: Lamp shadow budget (Off | 4 | 8 | 16), WW/CellShadowBudget; pin WW_CELL_SHADOWBUDGET=0|4|8|16.
 * Red WW_CELL_SHADOWBUDGET_RED=worst (the lowest scores win the slots). */
int wwCellShadowBudget();
bool wwCellShadowBudgetPinned();
void wwCellShadowBudgetSet( int n );
bool wwCellShadowBudgetRedWorst();

#endif // CELLLOOKS_H
