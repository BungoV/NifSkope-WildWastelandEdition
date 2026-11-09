#ifndef CELLPRESETTABLE_H
#define CELLPRESETTABLE_H

/* lane TIMING1 (bungo 11-01, "yeah, each feature per preset"): what the built-in presets Low / Medium / High / Ultra
 * set, row by row, for the Display panel's Fusion Shaders rows. It replaces the rule "every Fusion Shaders row on,
 * the heavy rows off at Low" (cellshowpanel.cpp addStep before this lane).
 *
 * plan: the manager's 11-01 table (notes/perfres1101/RULINGS.md): Low = the features whose Low target is <= ~0.3 ms;
 *       reflection probes, probe GI, air from Medium; contact shadows, far air, air particles from High; SSGI Ultra;
 *       AO decals, sky specular occlusion off everywhere until fixed and timed; LOD on at every preset.
 * cols: what each preset sets. A feature ENTERS a column (on, or a level above its ship value) only when its MEASURED
 *       GPU ms there meets its PERFRES target (reasoned, normalised to the RTX 5070 Ti at 1080p, skill
 *       gpu-ms-normalise). Until then the column holds the ship value. Exceptions, each named: 'R' a ruling (LOD on
 *       everywhere), 'G' a group master (it only gates its rows; no pass of its own).
 *
 * Codes per column: '0' off, '1' on, 'L' 'M' 'H' 'U' that quality item (the row on), '-' left as it is (a method,
 * a source or a curve: not a cost step). A key the table lacks is left as it is (cellshowpanel.cpp counts it in the
 * "cell presets:" line). tests/presettable checks every row and column against these rules; its red is the old rule.
 * Pure C++ (no Qt): the unit test includes it alone. */

#include <cstring>

struct WwPresetRow
{
	const char * key;       //!< the row's objectName (its tick, else its choices submenu)
	char ship;              //!< what the row ships at (a code above)
	const char * plan;      //!< the manager's 11-01 column plan
	const char * cols;      //!< what Low / Medium / High / Ultra set
	char basis;             //!< 'M' measured (each entered column meets its target), 'R' ruling, 'G' master, '-' no cost step
	bool heavy;             //!< the old rule's flag (the unit test's red only)
	float target[4];        //!< PERFRES target GPU ms per preset (reasoned); -1 none
	float measured[4];      //!< measured GPU ms per preset on the RTX 5070 Ti (-1 none)
	const char * source;    //!< where the numbers come from
};

#define WW_PR_NONE { -1.0f, -1.0f, -1.0f, -1.0f }

inline const WwPresetRow * wwPresetTable( int * count )
{
	// measured: PERFRES1101 (notes/perfres1101/{A,B,C}/SUMMARY.md, MEASURED rows) and TIMING1's hold
	// (notes/timing1/RESULTS.md); targets: the PERFRES TABLE rows (reasoned)
	static const WwPresetRow t[] = {
		{ "CellWorkspaceRtDdgi", '0', "0111", "0000", 'M', true, { 0.71f, 0.81f, 1.05f, 1.28f }, { 33.0f, 33.0f, 33.0f, 33.0f },
			"PERFRES-A probe_gi: update 0.97-1.94 device ms + read +32..49 ms in the c3 opaque pass (FIX1101A hold 4)" },
		{ "CellWorkspaceLook_WW_CELL_GI_SHARE", '0', "0000", "0000", '-', false, WW_PR_NONE, WW_PR_NONE, "the PRT's probe rule (PRTOFF1): nothing to draw" },
		{ "CellWorkspaceLook_WW_CELL_GI_SHARE_WIDE", '0', "0000", "0000", '-', false, WW_PR_NONE, WW_PR_NONE, "the PRT's probe rule" },
		{ "CellWorkspaceLook_WW_CELL_GI_DEPTHVIS", '0', "0000", "0000", '-', false, WW_PR_NONE, WW_PR_NONE, "the PRT's probe rule" },
		{ "CellWorkspaceLook_WW_CELL_GI_SMOOTH", '0', "0000", "0000", '-', false, WW_PR_NONE, WW_PR_NONE, "the PRT's probe rule" },
		{ "CellWorkspaceLook_WW_CELL_PROBE_CLEAR", '0', "0000", "0000", '-', false, WW_PR_NONE, WW_PR_NONE, "a bake rule" },
		{ "CellWorkspaceEmissiveGiGrid", '0', "0000", "0000", '-', false, WW_PR_NONE, WW_PR_NONE, "the PRT's grid" },
		{ "CellWorkspaceGpuRelight", '0', "0000", "0000", '-', false, WW_PR_NONE, WW_PR_NONE, "the PRT's relight" },
		{ "CellWorkspaceLook_WW_CELL_XEMI_GI", '0', "0111", "0000", 'M', false, WW_PR_NONE, WW_PR_NONE, "under probe GI; no ms" },
		{ "CellWorkspaceSkySpecOcc", '0', "0000", "0000", 'M', false, { 0.0f, 0.02f, 0.12f, 0.12f }, WW_PR_NONE,
			"ruled off until fixed + timed: a no-op without the PRT grid (cell_lights.glsl cellGiOn && cellGiSky)" },
		{ "CellWorkspaceAoDecals", '0', "0000", "0000", 'M', false, { 0.03f, 0.05f, 0.08f, 0.10f }, WW_PR_NONE,
			"ruled off until fixed + timed: double darkening under RTXGI (PERFRES-A gap 10)" },
		{ "CellWorkspaceGroup_IMPROVED_LIGHTS", '1', "1111", "1111", 'G', false, WW_PR_NONE, WW_PR_NONE, "a master" },
		{ "CellWorkspaceLook_WW_CELL_LIGHT_PHYS", '0', "1111", "0000", 'M', false, { 0.06f, 0.10f, 0.15f, 0.19f }, WW_PR_NONE,
			"light extras target (PERFRES-A light_extras); no ms of its own" },
		{ "CellWorkspaceLook_WW_CELL_LIGHT_AREA", '0', "1111", "0000", 'M', false, { 0.06f, 0.10f, 0.15f, 0.19f }, WW_PR_NONE, "light extras; no ms" },
		{ "CellWorkspaceLook_WW_CELL_LIGHT_ADV", '0', "1111", "0000", 'M', false, { 0.06f, 0.10f, 0.15f, 0.19f }, WW_PR_NONE, "light extras; no ms" },
		{ "CellWorkspaceGroup_CELL_LIGHTS", '1', "1111", "1111", 'G', false, WW_PR_NONE, WW_PR_NONE, "a master" },
		{ "CellWorkspaceLook_WW_CELL_NIFX_LIGHTS", '0', "1111", "0000", 'M', false, { 0.06f, 0.10f, 0.15f, 0.19f }, WW_PR_NONE,
			"light extras; no ms (and needs Cell lights, a Vanilla row no preset touches)" },
		{ "CellWorkspaceLightCurve", '-', "----", "----", '-', false, WW_PR_NONE, WW_PR_NONE, "a look curve, not a cost step" },
		{ "CellWorkspaceCellFar", '0', "1111", "0000", 'M', false, { 0.06f, 0.10f, 0.15f, 0.19f }, WW_PR_NONE, "far lights; no ms" },
		{ "CellWorkspaceGroup_SHADOWS", '1', "1111", "1111", 'G', false, WW_PR_NONE, WW_PR_NONE, "a master" },
		{ "CellWorkspaceCascadedShadows", '1', "LMHU", "1111", 'M', false, { 0.21f, 0.27f, 0.40f, 0.50f }, WW_PR_NONE,
			"ships on (DEFAULTS1, OPTIN1 SHOULD BE ON): the preset holds it; PERFRES-A sun_shadows: no live ms since the cache "
			"(the old 0.2-0.3 was pre-cache, 10-19); TIMING1's sun-cascades zone" },
		{ "CellWorkspaceScreenSpaceShadows", '0', "00HU", "0000", 'M', false, { 0.013f, 0.018f, 0.062f, 0.088f },
			{ 0.19f, 0.33f, 0.65f, 1.01f }, "TIMING1 hold c1 contact zone, c3 (mus 0.21/0.36/0.68/1.07); was terrshadow1 0.14-0.73" },
		{ "CellWorkspaceTerrainShadows", '0', "LMHU", "0000", 'M', false, { 0.02f, 0.02f, 0.02f, 0.02f }, WW_PR_NONE,
			"PERFRES-A: terrain 0.02 target; CPU <=0.198, GPU no ms" },
		{ "CellWorkspaceShadowRules", '-', "----", "----", '-', false, WW_PR_NONE, WW_PR_NONE, "FO4CS's shadow parity rules, not a cost step" },
		{ "CellWorkspaceLampShadows", 'M', "LMHU", "MMMM", 'M', false, { 0.125f, 0.25f, 0.375f, 0.56f }, WW_PR_NONE,
			"ships Medium (SKYINT1, unproven: PERFRES-A gap 6); the preset holds it; first build 102 ms GPU hitch (perf2 t1)" },
		{ "CellWorkspaceLook_WW_CELL_CONTACT_LOCAL", '0', "0011", "0000", 'M', false, WW_PR_NONE, WW_PR_NONE, "contact tier; no ms" },
		{ "CellWorkspaceLook_WW_CELL_CONTACT_FAR", '0', "0011", "0000", 'M', false, WW_PR_NONE, WW_PR_NONE, "contact tier; no ms" },
		{ "CellWorkspaceLook_WW_SHADOW_MOONSKY", '0', "1111", "0000", 'M', false, WW_PR_NONE, WW_PR_NONE, "terrain shadow rule; no ms" },
		{ "CellWorkspaceLook_WW_SHADOW_SLOPEMAP", '0', "1111", "0000", 'M', false, WW_PR_NONE, WW_PR_NONE, "terrain shadow rule; no ms" },
		{ "CellWorkspaceLook_WW_SHADOW_GROUNDGATE", '0', "1111", "0000", 'M', false, WW_PR_NONE, WW_PR_NONE, "terrain shadow rule; no ms" },
		{ "CellWorkspaceLook_WW_SHADOW_FARCARDS", '0', "1111", "0000", 'M', false, WW_PR_NONE, WW_PR_NONE, "terrain shadow casters; no ms" },
		{ "CellWorkspaceLook_WW_SHADOW_PBRDIM", '0', "1111", "0000", 'M', false, WW_PR_NONE, WW_PR_NONE, "terrain shadow rule; no ms" },
		{ "CellWorkspaceAo", '1', "1111", "1111", 'M', false, { 0.10f, 0.15f, 0.25f, 0.35f }, { 32.6f, 29.2f, 26.7f, 26.1f },
			"ships on (the vanilla method): the preset holds it; TIMING1 hold c1 AO zone, c3 (mus 7-9.6: the scene walk dominates)" },
		{ "CellWorkspaceAoQuality", 'U', "LMHU", "LMHU", 'M', false, { 0.10f, 0.15f, 0.25f, 0.35f }, { 0.08f, 0.14f, 0.24f, 0.30f },
			"PERFFIX2 item 9: the presets step the AO tier (ships Ultra = the picture before); vanilla SAO offline 1080p "
			"(notes/perffix2 aox_offline). BUGS1102: the row was missing, so the presets left the tier as it was" },
		{ "CellWorkspaceSsgi", '0', "000U", "0000", 'M', true, { 0.44f, 0.62f, 0.88f, 1.76f }, { 5.06f, 6.56f, 7.26f, 16.98f },
			"TIMING1 hold c1 SSGI zone, c3 (mus 4.1/5.1/5.8/13.8); was PERFRES-A Vault 15.7-17.9" },
		{ "CellWorkspaceGroup_REFLECTIONS", '1', "1111", "1111", 'G', false, WW_PR_NONE, WW_PR_NONE, "a master" },
		{ "CellWorkspaceReflProbe", '0', "0111", "0000", 'M', false, { 0.0f, 0.15f, 0.25f, 0.40f }, { 0.67f, 0.67f, 0.67f, 0.67f },
			"PERFRES-A reflection_probes: 0.42-0.67 GPU / 5.9-10.4 CPU (merge1029c cost2)" },
		{ "CellWorkspaceVolAir", '0', "0111", "0000", 'M', true, { 0.55f, 1.0f, 1.9f, 2.3f }, { 9.96f, 9.23f, 8.50f, 4.90f },
			"TIMING1 hold c1 air + fog zone, c3 (mus 3.7-4.2); was PERFRES-B 3.75-4.69 (FIX1101A holds)" },
		{ "CellWorkspaceLook_WW_CELL_AIR_MS", '0', "0111", "0000", 'M', false, WW_PR_NONE, WW_PR_NONE, "under the air; no ms" },
		{ "CellWorkspaceLook_WW_CELL_AIR_BURNOFF", '0', "0111", "0000", 'M', false, WW_PR_NONE, WW_PR_NONE, "under the air; no ms" },
		{ "CellWorkspaceLook_WW_CELL_AIR_GROUND_DERIVED", '0', "0111", "0000", 'M', false, WW_PR_NONE, WW_PR_NONE, "under the air; no ms" },
		{ "CellWorkspaceFarAir", '0', "00HU", "0000", 'M', true, WW_PR_NONE, WW_PR_NONE, "far air (sub air-far); no target of its own" },
		{ "CellWorkspaceAirLampVis", '-', "----", "----", '-', false, WW_PR_NONE, WW_PR_NONE,
			"MODULES1 route (a): Atlas | Ray traced | Probe walk; left as it is until the Museum timing picks the default" },
		{ "CellWorkspaceVolPart", '0', "0011", "0000", 'M', true, WW_PR_NONE, WW_PR_NONE, "the air's particles; no ms" },
		{ "CellWorkspaceFogDrift", '0', "0111", "0000", 'M', false, WW_PR_NONE, WW_PR_NONE, "under the air; no ms" },
		{ "CellWorkspaceAirZones", '-', "----", "----", '-', false, WW_PR_NONE, WW_PR_NONE, "hand-placed volumes (data), not a cost step" },
		{ "CellWorkspaceGroup_SKY", '1', "1111", "1111", 'G', false, WW_PR_NONE, WW_PR_NONE, "a master" },
		{ "CellWorkspaceLook_WW_SKY_EPHEM", '0', "1111", "0000", 'M', false, { 0.17f, 0.19f, 0.23f, 0.30f }, WW_PR_NONE,
			"sky target (PERFRES-B sky); no ms of its own" },
		{ "CellWorkspaceLook_WW_WEATHER_RECX", '0', "1111", "0000", 'M', false, { 0.17f, 0.19f, 0.23f, 0.30f }, WW_PR_NONE, "sky; no ms" },
		{ "CellWorkspaceLook_WW_CELL_PRECIP", '0', "1111", "0000", 'M', false, { 0.05f, 0.10f, 0.15f, 0.25f }, WW_PR_NONE,
			"PERFRES-B precip: GPU never timed (TIMING1's precip zone), CPU 3.50 ms a layer" },
		{ "CellWorkspaceLook_WW_CELL_RAINWET", '0', "0000", "0000", 'M', false, WW_PR_NONE, WW_PR_NONE,
			"FO76WEATHER1: off until bungo sees it; no ms" },
		{ "CellWorkspaceGroup_WATER", '1', "1111", "1111", 'G', false, WW_PR_NONE, WW_PR_NONE, "a master" },
		{ "CellWorkspaceLook_WW_CELL_WATER_DEPTH", '0', "0111", "0000", 'M', false, { 0.40f, 0.75f, 1.00f, 1.30f }, WW_PR_NONE, "water; no ms of its own" },
		{ "CellWorkspaceLook_WW_CELL_WATER_CAUSTICS", '0', "0111", "0000", 'M', false, { 0.40f, 0.75f, 1.00f, 1.30f }, WW_PR_NONE, "water; no ms" },
		{ "CellWorkspaceLook_WW_CELL_WATER_EDGE", '0', "0111", "0000", 'M', false, { 0.40f, 0.75f, 1.00f, 1.30f }, WW_PR_NONE, "water; no ms" },
		{ "CellWorkspaceLook_WW_CELL_WATER_FOAM", '0', "0111", "0000", 'M', false, { 0.40f, 0.75f, 1.00f, 1.30f }, WW_PR_NONE, "water; no ms" },
		{ "CellWorkspaceLook_WW_CELL_WATER_WAVES", '0', "0111", "0000", 'M', false, { 0.40f, 0.75f, 1.00f, 1.30f }, WW_PR_NONE, "water; no ms" },
		{ "CellWorkspaceLook_WW_CELL_WATER_REFL", '0', "0111", "0000", 'M', false, { 0.40f, 0.75f, 1.00f, 1.30f }, WW_PR_NONE,
			"water; live water SSR +96-136 ms (PERFRES-B water)" },
		{ "CellWorkspaceLook_WW_CELL_WATER_GLINT", '0', "0111", "0000", 'M', false, { 0.40f, 0.75f, 1.00f, 1.30f }, WW_PR_NONE, "water; no ms" },
		{ "CellWorkspaceLook_WW_CELL_WATER_OCEAN", '0', "0111", "0000", 'M', false, { 0.40f, 0.75f, 1.00f, 1.30f }, WW_PR_NONE, "water; no ms" },
		{ "CellWorkspaceLook_WW_ENDLESS_SEA", '1', "1111", "1111", 'R', false, WW_PR_NONE, WW_PR_NONE,
			"FARSEA1: the endless sea past the bake, one draw; ruled on at every preset (bungo 11-09)" },
		{ "CellWorkspaceFarLod", '0', "1111", "1111", 'R', false, { 0.20f, 0.33f, 0.50f, 0.65f }, WW_PR_NONE,
			"ruled on at every preset (bungo 11-01); far field cards 0.39 + ring-0 ground 0.82 GPU (PERFRES-C farfield)" },
		{ "CellWorkspaceFarLodType", '-', "----", "----", '-', false, WW_PR_NONE, WW_PR_NONE, "a source, not a cost step" },
		{ "CellWorkspaceFarGround", '-', "----", "----", '-', false, WW_PR_NONE, WW_PR_NONE, "a method, not a cost step" },
		{ "CellWorkspaceFarGroundPages", '-', "----", "----", '-', false, WW_PR_NONE, WW_PR_NONE, "a method, not a cost step" },
		{ "CellWorkspaceLook_WW_CELL_FARLOD_BTOSWAY", '0', "1111", "0000", 'M', false, WW_PR_NONE, WW_PR_NONE, "far sway; no ms" },
		{ "CellWorkspaceModernTaa", 'H', "LMHU", "HHHH", 'M', false, { 0.10f, 0.12f, 0.16f, 0.20f }, { 0.24f, 0.24f, 0.26f, 0.37f },
			"ships High (FIX1101A d78ec45b): the preset holds it; post zone c3 still_close5 (PERFRES-C taa2)" },
		{ "CellWorkspaceBloom", '0', "1111", "0000", 'M', false, { 0.03f, 0.075f, 0.09f, 0.11f }, WW_PR_NONE,
			"FO4CS bloom (vanilla is the off item): no ms of its own" },
		{ "CellWorkspaceLook_WW_CELL_BLOOM_PERPIX", '0', "1111", "0000", 'M', false, WW_PR_NONE, WW_PR_NONE, "under the bloom; no ms" },
		{ "CellWorkspaceGroup_CAMERA", '1', "1111", "1111", 'G', false, WW_PR_NONE, WW_PR_NONE, "a master" },
		{ "CellWorkspaceLook_WW_CELL_EXPOSURE_WIDE", '0', "1111", "0000", 'M', false, WW_PR_NONE, WW_PR_NONE, "an exposure range rule; no ms" },
		{ "CellWorkspaceLook_WW_CELL_VRS", '1', "0000", "1111", 'M', false, WW_PR_NONE, WW_PR_NONE,
			"PERFFIX4 coarse shading; PERFHUNT2 show 11-03: on at every preset (Concord 56.7 -> 54.8 ms GPU, Memory Den 30.8 -> 30.5; stills within the repeat noise)" },
	};
	if ( count )
		*count = int( sizeof( t ) / sizeof( t[0] ) );
	return t;
}

#undef WW_PR_NONE

inline const WwPresetRow * wwPresetRow( const char * key )
{
	int n = 0;
	const WwPresetRow * t = wwPresetTable( &n );
	for ( int i = 0; i < n; i++ )
		if ( std::strcmp( t[i].key, key ) == 0 )
			return &t[i];
	return nullptr;
}

/*! The code the built-in preset p (0 Low .. 3 Ultra) sets for the row `key`; '-' for a key the table lacks.
 *  PRESETTABLE_RED (the unit test's red only): the old rule, every row on at the preset's level, heavy off at Low. */
inline char wwPresetCode( const char * key, int p )
{
	if ( p < 0 )
		p = 0;
	if ( p > 3 )
		p = 3;
	const WwPresetRow * r = wwPresetRow( key );
#ifdef PRESETTABLE_RED
	static const char kOld[4] = { 'L', 'M', 'H', 'U' };
	if ( !r )
		return '-';
	if ( r->heavy && p == 0 )
		return '0';
	return std::strchr( "LMHU", r->ship ) || std::strchr( "LMHU", r->plan[p] ) ? kOld[p] : ( r->cols[p] == '-' ? '-' : '1' );
#else
	return r ? r->cols[p] : '-';
#endif
}

#endif
