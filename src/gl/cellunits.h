#ifndef WW_CELLUNITS_H
#define WW_CELLUNITS_H

/* The texture units of the cell view's surface programs (fo4_cell, pbrm_cell, their effect and particle swaps) and
 * of the air pass (cell_volair.prog), in ONE table (lane VOLAIR1, 2026-10-15). Every sampler these programs declare
 * sits on its own unit: two sampler types on one unit (a samplerBuffer beside a sampler3D) make the draw invalid,
 * and binding one feature's texture over another's unbinds it. Units 0..9 belong to TexCache (the material's own
 * maps, allocated from 0 upward); passes with programs of their own (bloom, the HDR resolve, TAA, the post chain,
 * the AO and SSR passes) bind low units and are not in this table. A new cell unit is added HERE; the static_assert
 * below refuses a duplicate at compile time and tests/spells/cell_units_check.py refuses a literal unit elsewhere. */
namespace WwCellUnit
{
constexpr int Shadow = 10;		// lane SHADOW1: the lights' depth cubes (samplerCubeArrayShadow)
constexpr int Bloom = 11;		// lane BLOOM1: the imagespace bloom (sampler2D)
constexpr int Lut = 12;			// lane IMGS1: the imagespace LUT (sampler3D)
constexpr int Gi = 13;			// lane PRTPGI: the bounce grid (sampler3D)
constexpr int Lights = 14;		// the light buffer (samplerBuffer)
constexpr int Csm = 15;			// lane CSM1: the sun's cascades
constexpr int Ao = 16;			// lane AO1: the obscurance (sampler2D)
constexpr int Ssr = 17;			// lane SSR1: the reflection (sampler2D)
constexpr int GiSlot = 18;		// lane ROOMCLAMP1: the grid's slot rooms (sampler3D)
constexpr int Room = 19;		// lane ROOMCLAMP1: the fine rooms (sampler3D)
constexpr int FarSlot = 20;		// lane FARVIEW1: the far table's slots (usamplerBuffer)
constexpr int FarRec = 21;		// lane FARVIEW1: its records (samplerBuffer)
constexpr int FarPlaced = 22;	// lane FARVIEW1: the GI grid's placed share (sampler3D)
constexpr int AoDecal = 23;		// lane AODECAL1: the decal target (sampler2D; was 20, under the far slots)
constexpr int AirVol = 24;		// lane VOLAIR1: the integrated air the surfaces read (sampler3D; was 21, under FarRec)
constexpr int AirInject = 25;	// lane VOLAIR1: the air pass, stage 2 reads stage 1 (sampler3D; was 22)
constexpr int AirCluster = 26;	// lane VOLAIR1: the air pass's cluster light lists (isamplerBuffer; was 23)
constexpr int AirGbuf = 27;		// lane VOLAIR1: the air pass, stage 0 reads AO1's g-buffer (sampler2D)
constexpr int AirTile = 28;		// lane VOLAIR1: the air pass, stage 1 reads stage 0's column distances (sampler2D)
constexpr int GiHalf = 29;		// lane PERF1: the GI at half resolution (sampler2D)
constexpr int GiHalfKey = 30;	// lane PERF1: its keys, the upsample's weights (sampler2D)
constexpr int TerrShadow = 31;	// lane TERRSHADOW1: the terrain's shadow map (sampler2D; testint moved it off PERF1's 29)
constexpr int ContactShadow = 32;	// lane TERRSHADOW1: the contact shadows (sampler2D; testint moved it off PERF1's 30)
constexpr int Ssgi = 33;		// lane SSGI1: the screen-space bounce, the open share (sampler2D) (moved off TerrShadow 31 at MERGEALL1)
constexpr int LightCluster = 34;	// lane LIGHTSYS2: the surfaces' clustered light lists (isamplerBuffer; was 24 = AirVol)
constexpr int CardData = 35;	// lane FARCARD1: the instanced far cards' per-card data (samplerBuffer, vertex stage)
constexpr int CardFrames = 36;	// lane FARCARD1: their sets' frame tables (samplerBuffer, vertex stage)
constexpr int ReflProbe = 37;	// lane REFLPROBE1: the probe atlas (sampler2D, read through GreyscaleMap: no new sampler; 35 in the lane, FARCARD1 holds it)
constexpr int ReflGbufAlb = 38;	// lane REFLPROBE1: the relight pass reads the G-buffer cubes (samplerCubeArray)
constexpr int ReflGbufNd = 39;
constexpr int AirNoise = 40;		// lane FOGDRIFT1: the air's drifting density noise (sampler3D; 37 in the lane, REFLPROBE1 holds it)
constexpr int AtmoAp = 41;		// lane SKYA (ATMO2): the physical sky's aerial-perspective froxels (sampler3D)
constexpr int AtmoTrans = 42;	// lane SKYA (ATMO2): the sky's transmittance LUT, read by the sky's air (cell_volsky)
constexpr int ContactGbuf = 43;	// lane GAPLOOK1 (C8): the lamps' contact march reads the shared opaque pass (sampler2D; 41 in the lane, SKYA holds it)
constexpr int CardDepth = 44;	// lane PERF4: the far cards' early reject reads the scene's farthest depth per pixel (sampler2D)

constexpr int kAll[] = { Shadow, Bloom, Lut, Gi, Lights, Csm, Ao, Ssr, GiSlot, Room, FarSlot, FarRec, FarPlaced, AoDecal,
	AirVol, AirInject, AirCluster, AirGbuf, AirTile, GiHalf, GiHalfKey, TerrShadow, ContactShadow, Ssgi, LightCluster,
	CardData, CardFrames, ReflProbe, ReflGbufAlb, ReflGbufNd, AirNoise, AtmoAp, AtmoTrans, ContactGbuf, CardDepth };

constexpr bool distinct()
{
	constexpr int n = int( sizeof( kAll ) / sizeof( kAll[0] ) );
	for ( int i = 0; i < n; i++ ) {
		if ( kAll[i] < 10 )
			return false;	// 0..9 are TexCache's
		for ( int j = i + 1; j < n; j++ )
			if ( kAll[i] == kAll[j] )
				return false;
	}
	return true;
}
static_assert( distinct(), "two cell-view samplers share a texture unit (src/gl/cellunits.h)" );
}

#endif
