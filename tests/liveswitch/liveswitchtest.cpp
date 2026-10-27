// LIVESET1: the live-set mechanism each converted subsystem uses (WW_SWITCH_STATIC, wwSwitchMovedFor) rebuilds on a
// changed value and not on an unchanged one; the converted switches read live in the stage table. Exit 0 = pass.
#include "wwswitch.h"
#include "gl/fogterrain.h"

#include <cstdio>

static int g_fail = 0;
static void check( bool ok, const char * what )
{
	std::printf( "%s %s\n", ok ? "PASS" : "FAIL", what );
	g_fail += ok ? 0 : 1;
}

static int g_builds = 0;
static int reader()	// the shape of cellqueue/cellgpucull sw(), celldoorlive req(), cellhdr ...
{
	WW_SWITCH_STATIC( int, v, ( ++g_builds, qEnvironmentVariableIntValue( "WW_SHADOWCACHE" ) ) );
	return v;
}

int main()
{
	qputenv( "WW_SHADOWCACHE", "1" );
	check( reader() == 1 && g_builds == 1, "first read builds once" );
	reader();
	check( g_builds == 1, "no set: no rebuild" );

	QString r = wwSwitchSet( "WW_SHADOWCACHE", "1", false );
	reader();
	check( r.startsWith( "same" ) && g_builds == 1, "unchanged value: no rebuild" );

	r = wwSwitchSet( "WW_SHADOWCACHE", "0", false );
	check( r.startsWith( "ok" ) && reader() == 0 && g_builds == 2, "changed value: one rebuild, new value" );
	reader();
	check( g_builds == 2, "after the rebuild: no more" );

	// the far LOD / probe sections and sunshadow/CSM_ECHO watch their prefixes only
	quint64 far = wwSwitchGen(), probes = wwSwitchGen(), csm = wwSwitchGen();
	wwSwitchSet( "WW_CELL_FARLOD_RED", "nocut", false );
	check( wwSwitchMovedFor( far, { "WW_CELL_FARLOD" } ), "far section sees WW_CELL_FARLOD_RED" );
	check( !wwSwitchMovedFor( probes, { "WW_CELL_PROBES_HIDE" } ), "probe section ignores WW_CELL_FARLOD_RED" );
	check( !wwSwitchMovedFor( csm, { "WW_CSM_ECHO" } ), "CSM echo ignores WW_CELL_FARLOD_RED" );
	check( !wwSwitchMovedFor( far, { "WW_CELL_FARLOD" } ), "far section: nothing new after it looked" );
	wwSwitchSet( "WW_CELL_FARLOD_RED", "nocut", false );
	check( !wwSwitchMovedFor( far, { "WW_CELL_FARLOD" } ), "same far value: no rebuild" );
	wwSwitchSet( "WW_CELL_PROBES_HIDE", "", true );
	check( !wwSwitchMovedFor( probes, { "WW_CELL_PROBES_HIDE" } ), "unset of an unset HIDE: no rebuild" );
	wwSwitchSet( "WW_CELL_PROBES_HIDE", "1", false );
	check( wwSwitchMovedFor( probes, { "WW_CELL_PROBES_HIDE" } ), "HIDE set: probe section moves" );

	// the converted switches classify live (a relaunch here would make the runner launch per row)
	for ( const char * n : { "WW_CELL_FARLOD_RED", "WW_CELL_PROBES_HIDE", "WW_SHADOWCACHE", "WW_CSM_ECHO", "WW_GPUDRIVEN",
			"WW_GPUDRIVEN_B", "WW_CELL_FARLOD_L0", "WW_CELL_FARLOD_NEARONLY" } ) {
		const bool live = wwSwitchStageOf( n ) == WwSwitchStage::Live;
		std::printf( "%s %s live\n", live ? "PASS" : "FAIL", n );
		g_fail += live ? 0 : 1;
	}
	// lane FAR6: Game LOD trees sway rebuilds the far section in place; the self-test rides the ground-gate row
	for ( const char * n : { "WW_CELL_FARLOD_BTOSWAY", "WW_CELL_FARLOD_BTOSWAY_RED", "WW_SHADOW_GROUNDGATE",
			"WW_SHADOW_GROUNDGATE_RED", "WW_SHADOW_SLOPEMAP", "WW_SHADOW_SLOPEMAP_RED" } ) {
		const bool live = wwSwitchStageOf( n ) == WwSwitchStage::Live;
		std::printf( "%s %s live\n", live ? "PASS" : "FAIL", n );
		g_fail += live ? 0 : 1;
	}
	far = wwSwitchGen();
	wwSwitchSet( "WW_CELL_FARLOD_BTOSWAY", "1", false );
	check( wwSwitchMovedFor( far, { "WW_CELL_FARLOD" } ), "far section sees WW_CELL_FARLOD_BTOSWAY" );
	check( wwSwitchStageOf( "WW_CELL_SPEED_RED" ) == WwSwitchStage::Reopen, "WW_CELL_SPEED_RED reopen (was relaunch)" );

	// LIVESET4: far on/off + type, the per-cell switches (streamed cells stream again) and the weather plugin list
	for ( const char * n : { "WW_CELL_FARLOD", "WW_CELL_FARLOD_TYPE", "WW_CELL_ACTORS_OUT", "WW_CELL_ACTORS_OUT_RED",
			"WW_CELL_WEAP_PARTS", "WW_CELL_WEAP_PARTS_RED", "WW_LOOKDEV_PLUGINS" } ) {
		const bool live = wwSwitchStageOf( n ) == WwSwitchStage::Live;
		std::printf( "%s %s live\n", live ? "PASS" : "FAIL", n );
		g_fail += live ? 0 : 1;
	}
	check( wwSwitchStageOf( "WW_MSAA" ) == WwSwitchStage::Relaunch, "WW_MSAA stays relaunch (the GL surface's samples)" );
	quint64 cells = wwSwitchGen(), probes2 = wwSwitchGen();
	wwSwitchSet( "WW_CELL_ACTORS_OUT", "0", false );
	check( wwSwitchMovedFor( cells, { "WW_CELL_FARLOD", "WW_CELL_ACTORS_OUT", "WW_CELL_WEAP_PARTS" } ),
		"cell section sees WW_CELL_ACTORS_OUT" );
	check( !wwSwitchMovedFor( probes2, { "WW_CELL_PROBES_HIDE" } ), "probe section ignores WW_CELL_ACTORS_OUT" );

	// FOGTERRAIN2: the terrain-following fog's red flips the anchor live (fogterrain.cpp wwFogTerrainWanted)
	check( wwSwitchStageOf( "WW_FOG_TERRAIN_RED" ) == WwSwitchStage::Live, "WW_FOG_TERRAIN_RED live" );
	check( wwFogTerrainWanted(), "fog terrain: follows the terrain by default" );
	wwSwitchSet( "WW_FOG_TERRAIN_RED", "absolute", false );
	check( !wwFogTerrainWanted(), "fog terrain: a live set of the red gives the absolute anchor" );
	wwSwitchSet( "WW_FOG_TERRAIN_RED", "", true );
	check( wwFogTerrainWanted(), "fog terrain: the unset follows the terrain again" );
	// FOGTERRAIN3: the water and fine-level reds, live too (shadowset.cpp reads them at each collect)
	check( wwFogTerrainRed() == WwFogTerrRed::None, "fog terrain: no red by default" );
	wwSwitchSet( "WW_FOG_TERRAIN_RED", "seafloor", false );
	check( wwFogTerrainRed() == WwFogTerrRed::Seafloor && wwFogTerrainWanted(), "fog terrain: live seafloor drops the water only" );
	wwSwitchSet( "WW_FOG_TERRAIN_RED", "coarse", false );
	check( wwFogTerrainRed() == WwFogTerrRed::Coarse && wwFogTerrainWanted(), "fog terrain: live coarse drops the fine level only" );
	wwSwitchSet( "WW_FOG_TERRAIN_RED", "", true );
	check( wwFogTerrainRed() == WwFogTerrRed::None, "fog terrain: the unset clears the red" );

	std::printf( "%s (%d failed)\n", g_fail ? "LIVESWITCH FAIL" : "LIVESWITCH PASS", g_fail );
	return g_fail ? 1 : 0;
}
