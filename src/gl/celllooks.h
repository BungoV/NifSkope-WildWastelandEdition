#ifndef CELLLOOKS_H
#define CELLLOOKS_H

/* lane GAPLOOK1 (10-23): the look switches of the gap sweep (notes/GAPS_20261023.md section 5). Each ships Off
 * (= the look before it), has a Show menu row, and an env pin that wins (read again on a live set).
 * Rows, pins, reds and costs: WW_CHANGES "GAPLOOK1". */

#include <QString>
#include "gl/lightphys.h"

enum class WwLook : int
{
	AirClusters = 0,	// LIGHTSYS-4: the fog reads the surface light clusters (WW_CELL_AIR_CLUSTERS)
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
	PlacementEmittance,	// XEMI1 (LIGHTS2): a placement's emittance colour tints its effects and lamp glow (WW_CELL_XEMI)
	RegionEmittance,	// XEMI-REGN (LIGHTS3): region-sourced emittance, the region weather's Effect Lighting (WW_CELL_XEMI_REGN)
	ParticleEmittance,	// XEMI-PARTICLES (LIGHTS3): particle systems of an emittance placement take its colour (WW_CELL_XEMI_PART)
	BounceEmittance,	// XEMI-GI (LIGHTS3): the bounce light of a recoloured emissive uses the emittance colour (WW_CELL_XEMI_GI)
	LensFlares,			// LENSFLARE1 (LIGHTS3): LENS sprites of lights, a post-process pass (WW_CELL_LENSFLARE)
	LightPhysFalloff,	// LIGHTPHYS1: windowed 1/d^2 for placed lights, surfaces + effects + air (WW_CELL_LIGHT_PHYS)
	LightAdvanced,		// LIGHTPHYS1: inverse square with a minimum distance; wins over the physical row (WW_CELL_LIGHT_ADV)
	LightArea,			// LIGHTPHYS1: PBR specular from a sphere of 8 units, not a point (WW_CELL_LIGHT_AREA)
	Count
};

//! the row's label, the pin's name, the QSettings key
const char * wwLookLabel( WwLook l );
const char * wwLookPinName( WwLook l );
bool wwLookOn( WwLook l );
bool wwLookPinned( WwLook l );
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
/* lane LIGHTPHYS1: the light intensity curve, colour x pow(luminance / 0.1043, curve - 1). Row: Light intensity curve
 * (Off | 0.75 | 1.25 | 1.5 | 2), WW/CellLightCurve; pin WW_CELL_LIGHT_CURVE=<curve>, 1 = off. */
float wwCellLightCurve();
bool wwCellLightCurvePinned();
void wwCellLightCurveSet( float curve );
//! this frame's light-physics values for the shaders and the light buffer (src/gl/lightphys.h)
wwlp::Params wwLightPhysParams();

#endif // CELLLOOKS_H
