#ifndef CELLLOOKS_H
#define CELLLOOKS_H

/* lane GAPLOOK1 (10-23): the look switches of the gap sweep (notes/GAPS_20261023.md section 5). Each ships Off
 * (= the look before it), has a Show menu row, and an env pin that wins (read again on a live set).
 * Rows, pins, reds and costs: WW_CHANGES "GAPLOOK1". */

#include <QString>

enum class WwLook : int
{
	AirClusters = 0,	// LIGHTSYS-4: the fog reads the surface light clusters (WW_CELL_AIR_CLUSTERS); FOGPERF1: a speed switch, on
	AirMultiScatter,	// FOGEXTRAS: the sun's multiple-scattering octaves in the air (WW_CELL_AIR_MS)
	GroundBurnOff,		// FOGEXTRAS: the ground fog layer burns off after sunrise (WW_CELL_AIR_BURNOFF)
	ContactFar,			// C7: the contact shadows' far tier, a coarse long march (WW_CELL_CONTACT_FAR)
	ContactLocal,		// C8: contact shadows for the lamps without a shadow map (WW_CELL_CONTACT_LOCAL)
	TerrMoonSky,		// TERRSHADOWLEFT: the moon's terrain shadow from phase, moon fade and sky clearness (WW_SHADOW_MOONSKY)
	TerrPbrDim,			// TERRSHADOWLEFT: terrain shadow dims the PBR path's ambient + environment (WW_SHADOW_PBRDIM)
	BloomMip,			// BLOOMMIP: the bloom's blur width chosen per pixel by the fog it crosses (WW_CELL_BLOOM_PERPIX)
	Ephemeris,			// EPHEM1 (GAPLOOK2): Sun Path Real uses FO4CS Solar's sun + the real moon, phase, date (WW_SKY_EPHEM)
	ActorsOutdoors,		// PLACED1 (GAPLOOK2): placed actors in exterior cells, not only interiors (WW_CELL_ACTORS_OUT)
	WeaponParts,		// MISS1 (GAPLOOK2): placed weapons get their default mod parts on the connect points (WW_CELL_WEAP_PARTS)
	WorldAnim,			// FXPARITY2: the cell's world clock runs controllers, particles, water scroll (WW_CELL_WORLDANIM)
	AnimObjects,		// FXPARITY2: placed models with controllers are copied whole, not welded (WW_CELL_ANIMOBJ)
	ExposureWide,		// GIBAKE2 (GISHADE1): the eye may open past the imagespace's exposure cap, to 16 (WW_CELL_EXPOSURE_WIDE)
	ProbeShare,			// GPURELIGHT1: neighbouring probes share light, never across a wall (WW_CELL_GI_SHARE)
	DecalFade,		// DECALS1: decal-flag model shapes fade out with distance, the game's LOD rule (WW_CELL_DECAL_FADE)
	DecalParallax,	// DECALS1: placed decals with a height map (TX04) get the game's parallax (WW_CELL_DECAL_POM)
	TerrSlopeMap,		// FAR5: the terrain shadow's slope bias from the height map's gradient (WW_SHADOW_SLOPEMAP)
	TerrGroundGate,		// FAR5: object-LOD shadows land on ground receivers only, FO4CS's ground band (WW_SHADOW_GROUNDGATE)
	CableShadowSway,	// FAR5: a wind cable's shadow follows its sway (WW_CELL_CABLES_SHADOWSWAY)
	BtoSway,			// FAR5: the game's .bto trees sway with the far wind (WW_CELL_FARLOD_BTOSWAY)
	FarCardShadows,		// FAR5: far tree cards cast into the terrain shadow map (WW_SHADOW_FARCARDS)
	Count
};

//! the row's label, the pin's name, the QSettings key
const char * wwLookLabel( WwLook l );
const char * wwLookPinName( WwLook l );
bool wwLookOn( WwLook l );
bool wwLookPinned( WwLook l );
/* lane FOGPERF1 (bungo 10-23): a speed switch inside a feature has no row and is on with it; its pin "0" stays as
 * the test red. False for those. */
bool wwLookHasRow( WwLook l );
//! true when the look is read while the cell is built: the menu row reopens the cell
bool wwLookReopens( WwLook l );
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
