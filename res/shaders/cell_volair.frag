#version 410 core

/* lane VOLAIR1: the air, froxel by froxel (src/gl/cellvolair.h). One draw writes one layer k of a (columns x rows x
 * slices) volume; the slices are exponential from volSlices.x (near) over ln(far / near) = volSlices.y in volSlices.z
 * steps (game units along the view ray).
 *   stage 1  inject: the medium sigma_t(z) = sigma0 x rho(z), rho = min(1, exp(-(z - zb) / H)) (airMed: sigma0, zb,
 *            1 / H, the top z above which the air is empty; z in the fog's height frame). Four equal sub-steps across
 *            slice k; each adds T x J x (1 - exp(-tau)) and multiplies T by exp(-tau), tau the sub-step's optical
 *            depth in closed form. J = the light the air scatters toward the eye (display units, pi x radiance):
 *              1 the directional light x its cascade shadow x the air's transmittance up to the top x pi x phase
 *              2 every placed light (colour x radial x cone x its shadow cube) x the air's transmittance to it x pi x phase
 *              4 the probe GI and 8 the ambient (the share the GI grid leaves), each read as L0 + L1 of a six-axis cube
 *            The directional light is evaluated per sub-step (the shafts), the placed lights over the slice's stretch
 *            of the ray inside each light's sphere (four samples), the rest once at the slice's middle.
 *            rgb = the slice's light from its near edge, a = the slice's transmittance.
 *   stage 2  integrate: L_k = L_(k-1) + T_(k-1) x inj_k.rgb, T_k = T_(k-1) x inj_k.a. */

#define WW_CELL_FX 1
/* lane FOGLEAK1 (fog lit where no light reaches, notes/fogleak-r/RESEARCH.md): the leak fix, compiled only under the
 * loader's WW_CELL_FOGLEAK (off by default) and only with the ray-traced volume to read (WW_CELL_RTDDGI). Each fix
 * keeps the old path as a red bit of volLeakRed (cellvolair.cpp WW_CELL_AIR_RED= dalcenv / nosungate / lightleak) */
#if defined( WW_CELL_FOGLEAK ) && defined( WW_CELL_RTDDGI )
#define WW_AIR_LEAK 1
uniform int volLeakRed;		// 1 dalcenv, 2 nosungate, 4 lightleak, 8 nofartailshadow (lookdev_fog.glsl: volAirFlags 256), 16 nointsungate (INTGI1)
#endif
#include "lookdev_fog.glsl"
#include "ww_sunshadow.glsl"
#include "cell_lights.glsl"

uniform int volStage;			// 1 inject, 2 integrate
uniform int volLayer;
uniform ivec3 volDims;			// columns, rows, slices
uniform vec2 volProj;			// the projection's [0][0], [1][1]
uniform sampler3D volInject;	// stage 2 reads stage 1's volume
uniform bool volCsm;			// the sun's cascades are drawn this frame
uniform vec3 volFlat[6];		// red flat: one cube for every froxel (the grid's mean)
uniform int volTerms;			// gates only: 1 the directional light, 2 the placed lights, 4 the GI, 8 the ambient
// the optimisation (coordinator 10-14, cellvolair.cpp): the clustered lights and the conservative depth skip
uniform isamplerBuffer volClusters;	// per cluster (offset, count), then the light indices (ascending)
uniform ivec4 volClusterDims;		// clusters across, down; froxel columns a cluster side; slices a cluster (x 0: every light)
uniform int volDepthSkip;			// 0 off, 1 behind the farthest opaque distance (3 x 3 columns), 2 red nearskip
uniform sampler2D volGbuf;			// stage 0: lane AO1's depth buffer (volGbufZ 1: window depth) or g-buffer (a: linear depth, game units; 1e6 empty)
uniform int volGbufZ;
uniform vec2 volProjZ;			// the projection's [2][2], [3][2] (the window depth back to the view distance)
uniform ivec2 volGbufSize;
// lane FARGRID1 (cellvolair.h): 1 the far grid is on (both passes take the sun's shadow from airFarVis), 2 this is the
// far pass, 4 red farnoterr (the far pass ignores the terrain); 0 = as before
uniform int volFarMode;
uniform int volLayerBase;		// the grid's first layer in the packed volume (the far grid sits after the near one)
uniform float volJitter;		// red farjitter: a per-frame slice offset (0 otherwise)
uniform int volCount;			// lane PERF2, stage 3: the layers this draw writes (volLayer the first, at most 7)
uniform sampler2D volCarry;		// lane PERF2, stage 3: the running (L, T) after layer volLayer - 1
uniform int volTemporal;		// lane PERF2, stage 1: 0 the four sub-steps; 1 one fresh stratum + the history reprojected; 2 the same, camera unchanged
uniform int volQ;				// this frame's fresh stratum (0..3)
uniform sampler3D volHist;		// last frame's strata: layer p x slices + k = stratum p's light / (1 - a), a = 1 where valid
uniform vec4 volReproj[3];		// this frame's view -> last frame's view (rows, view units)
uniform vec4 volPrevGrid;		// last frame's [0][0], [1][1], near, ln(far / near)
uniform float volPrevScale;		// last frame's fogDistScale
uniform bool volHistOk;			// a history at these dims
uniform vec3 volLightShift;		// gates only (WW_CELL_AIR_LIGHTSHIFT): the air sees every placed light moved by this (world)
uniform bool volSurfClusters;	// lane GAPLOOK1 (LIGHTSYS-4): the lights from the surfaces' lists (cell_cluster.glsl)
uniform vec2 volSurfPx;			// the main viewport's pixels a froxel column (x, y)
uniform bool volSurfRed;		// red drop: each walk leaves out its first light
uniform vec4 volMs;				// lane GAPLOOK1 (FOGEXTRAS): the sun's scattering octaves a, b, c, w 1 = shadows lift (x 0: off)
uniform vec4 volMsLamp;		// lane AIRMS1 (AIR-MS-LAMPS): the lamps' orders >= 2: K0, K1, a, s (x 0: off; src/gl/airmslamp.h)
#ifdef WW_CELL_AIR_BOUNCE
uniform vec4 volGndLamp;		// lane AIRBOUNCE1 (AIR-LAMPS-BOUNCE): the lamps' floor bounce: K a, m, sb, floor depth (x 0: red; src/gl/airbouncelamp.h)
#endif
uniform bool volMsMedium;		// lane GLAREBLOW1: a = min(volMs.x, the air's re-scatter chance) (red WW_CELL_GLAREBLOW_RED=off: volMs.x)
// lane FOGDRIFT1 (cellvolair.h): x amp / span (0: smooth air), y the fade's start, z 1 / its length, w the noise's mean
uniform vec4 volDrift;
uniform vec4 volDriftOfs[2];	// per octave: xyz the wind's offset (world units), w 1 / its tile
uniform sampler3D volNoise;
uniform bool volDriftSun;		// lane WEATHERBLEND4: the sun's column drifts too (red WW_CELL_FOGDRIFT_RED=nosun: false)
// lane LIGHTS5 (FOGSCALE1): a light's fog.scale (.recx), 4 a texel from cellFogScaleBase; off = 1 for every light
uniform bool cellFogScaleOn;
uniform int cellFogScaleBase;
// lane NIFXLIGHT2: the .nifx bulb-tier lights (GI and fog only), texels from cellBulbBase after the lights; walked after
// the cluster or surface list, every froxel (no culling: a few at most). 0 = none: the loops as before
uniform int cellBulbBase;
uniform int cellBulbCount;
float cellFogK( int i )
{
	return cellFogScaleOn ? texelFetch( cellLights, cellFogScaleBase + i / 4 )[i % 4] : 1.0;
}

layout ( location = 0 ) out vec4 fragColor;
layout ( location = 1 ) out vec4 volMrt[7];	// stage 3: layers volLayer + 1 .. + 6, then the carry; stage 1 temporal: the 4 strata

// SKY2 FOGSLICE1: the air pass reads the air volume (cell_airmed.glsl); the lit particles keep the grid
#define WW_AIR_GIVOL 1
#include "cell_airmed.glsl"
// lane CLOUD6e: the rain fog under the rain clouds (cell_rainfog.glsl; rfOn 0 = the air exactly as before)
#include "cell_rainfog.glsl"
// lane AIRZONE1: the hand-placed volumes (cell_airzone.glsl): one read a sub-step, the light it scatters x the tint
#ifdef WW_CELL_AIRZONE
#include "cell_airzone.glsl"
#define AZ_TINT( q ) * azTintQ[q]
#else
#define AZ_TINT( q )
#endif

#ifdef WW_AIR_LEAK
/* lane FOGLEAK1 fix 2: the volume's gate on the sun at this froxel, set once a froxel by airLeakEnv (airRest runs
 * before the sub-steps' airSun); 1 = no gate (no volume, the red nosungate) */
float airLeakSun = 1.0;

/* lane FOGLEAK1 fix 1: the env share from the ray-traced volume. The six-axis cube's L0 + L1 scatter (volCubeScatter)
 * needs only the irradiance at +dW and -dW: for an L1 field (2/3) sum E = 2 (E+ + E-) and sum (E+a - E-a) dW.a =
 * E+ - E-. One walk of the 8 probes round P (the cascades dense to coarse by their tight fade, as cellDdgiRead walks
 * them) with ONE weight a probe, trilinear x Chebyshev toward P (a froxel has no normal: no backface weight, no bias;
 * fix 3's weights), reading each probe's tile at +dW, -dW and (fix 2) its sky visibility toward the sun. Two
 * cellDdgiRead calls cost 2.3 ms more on the offline 160 x 90 x 64 inject (tests/fogleak/fogleak_check.py C), this
 * walk 16 fetches a probe instead of 26. Returns fade x the scatter in the ambient's unit (E / pi, as cellLit's
 * gi = cellGiE / pi); fade = the volume's cover (the flat ambient takes 1 - fade, as on the surfaces) */
vec3 airLeakEnv( vec3 P, vec3 dW, out float fade )
{
	fade = 0.0;
	airLeakSun = 1.0;
	// read only for a term that takes it: the ambient (fix 1) or the sun outdoors (fix 2)
	bool needEnv = ( volTerms & 8 ) != 0 && ( volLeakRed & 1 ) == 0;
	// lane INTGI1 (FOGLEAK1-INT): indoors too (an unlit interior's directional; a sunlit one already shades as outdoors)
	bool needSun = ( volTerms & 1 ) != 0 && cellHasDir && ( volLeakRed & ( cellInterior ? 18 : 2 ) ) == 0;
	if ( !cellDdgiOn || !( needEnv || needSun ) )
		return vec3( 0.0 );
	vec3 sd = needSun ? normalize( cellDirTo ) : vec3( 0.0, 0.0, 1.0 );
	vec3 Ep = vec3( 0.0 ), Em = vec3( 0.0 );
	float vs = 0.0, left = 1.0;
#ifdef WW_CELL_RTDDGI_CASCADES
	for ( int c = 0; c < cellDdgiCount && left > 0.0; c++ ) {
		cellDdgiC = c;
		vec4 O = cellDdgiCOrigin[c], S = cellDdgiCStep[c];
		ivec4 Dm = cellDdgiCDims[c];
		int base = cellDdgiCBase[c];
		float tight = 1.0;
#else
	for ( int c = 0; c < 1; c++ ) {
		vec4 O = cellDdgiOrigin, S = cellDdgiStep;
		ivec4 Dm = cellDdgiDims;
		int base = 0;
		float tight = 0.0;
#endif
		vec3 g = ( P - O.xyz ) / S.xyz;
		ivec3 b = clamp( ivec3( floor( g ) ), ivec3( 0 ), max( Dm.xyz - 2, ivec3( 0 ) ) );
		vec3 a = clamp( g - vec3( b ), 0.0, 1.0 );
		vec3 e = min( g, vec3( Dm.xyz - 1 ) - g ) - tight;
		float fd = clamp( min( e.x, min( e.y, e.z ) ), 0.0, 1.0 );
		if ( fd <= 0.0 )
			continue;
		vec3 sp = vec3( 0.0 ), sm = vec3( 0.0 );
		float sv = 0.0, ws = 0.0, cover = 0.0;
		for ( int k = 0; k < cellGiLoop8; k++ ) {
			ivec3 o = ivec3( k % 2, ( k / 2 ) % 2, k / 4 );
			ivec3 q = min( b + o, Dm.xyz - 1 );
			int p = q.x + Dm.x * ( q.y + Dm.y * q.z );
			vec4 pr = texelFetch( cellDdgiAtlas, base + 192 * Dm.w + p );
			vec3 t = mix( vec3( 1.0 ) - a, a, vec3( o ) );
			float w = t.x * t.y * t.z;
			int st = int( pr.w + 0.5 );
			if ( st != 2 ) {	// cellDdgiRead's cover: Off keeps its share, a missing probe does not
				cover += st == 0 ? w : 0.0;
				continue;
			}
			cover += w;
			vec3 bp = O.xyz + vec3( q ) * S.xyz + pr.xyz - P;
			float rr = length( bp );
			if ( rr > 0.0 ) {
				vec4 m = cellDdgiTile( p, bp * ( -1.0 / rr ), 16 );
				w *= cellDdgiChebyshev( rr, m.x, m.y );
			}
			vec4 ea = cellDdgiTile( p, dW, 8 );
			sp += pow( max( ea.rgb, vec3( 0.0 ) ), vec3( 2.5 ) ) * w;
			if ( needEnv ) {
				vec4 eb = cellDdgiTile( p, -dW, 8 );
				sm += pow( max( eb.rgb, vec3( 0.0 ) ), vec3( 2.5 ) ) * w;
			}
			if ( needSun )
				sv += cellDdgiTile( p, sd, 8 ).w * w;
			ws += w;
		}
		if ( ws <= 0.0 )
			continue;
		float take = fd * min( cover, 1.0 ) * left;
		vec3 mp = sp / ws, mm = sm / ws;
		Ep += take * mp * mp;
		Em += take * mm * mm;
		vs += take * sv / ws;
		left -= take;
	}
	float f = 1.0 - left;
	if ( f <= 0.0 )
		return vec3( 0.0 );
	// fix 2 (a ruling, RESULTS.md): sky visibility toward the sun 0.1 -> 0 .. 0.4 -> 1 (open ground reads 0.6+)
	if ( needSun )
		airLeakSun = mix( 1.0, clamp( ( vs / f - 0.1 ) * ( 1.0 / 0.3 ), 0.0, 1.0 ), f );
	if ( !needEnv )
		return vec3( 0.0 );
	fade = f;
	Ep *= 2.0 / f;	// the read's decode: mean^2 x 2 pi, then / pi to the ambient's unit
	Em *= 2.0 / f;
	return f * 0.25 * max( 2.0 * ( Ep + Em ) + 3.0 * volGi.w * ( Ep - Em ), vec3( 0.0 ) ) * volGi.rgb;
}

/* fix 2: past the cascades' reach (their fade, as airSunMapVis weighs it) the sun takes the volume's gate; inside it
 * the shadow map stays the verdict */
float airLeakSunGate( vec3 posView, float vis )
{
	if ( ( volLeakRed & 2 ) != 0 || airLeakSun >= 1.0 )
		return vis;
	float cover = 0.0;
	if ( volCsm && -posView.z * csmParams.x <= csmParams.y + csmParams.w ) {
		float s = clamp( dot( posView, posView ) * csmParams.x * csmParams.x / ( csmParams.y * csmParams.y ), 0.0, 1.0 );
		float s2 = s * s;
		cover = 1.0 - s2 * s2;
	}
	return mix( min( vis, airLeakSun ), vis, cover );
}

/* fix 3: does light position Lp reach P, by the volume's depth: the 8 probes round P weighted as the read weighs them
 * (trilinear x Chebyshev toward P), each probe's Chebyshev toward Lp over r - D / 2 (a lamp inside its own fixture,
 * a ruling); the cascades dense to coarse by their tight fade; past the volume 1 (unshadowed, as before) */
float airLeakLightVis( vec3 P, vec3 Lp )
{
	if ( !cellDdgiOn )
		return 1.0;
	float vis = 0.0, left = 1.0;
#ifdef WW_CELL_RTDDGI_CASCADES
	for ( int c = 0; c < cellDdgiCount && left > 0.0; c++ ) {
		cellDdgiC = c;
		vec4 O = cellDdgiCOrigin[c], S = cellDdgiCStep[c];
		ivec4 Dm = cellDdgiCDims[c];
		int base = cellDdgiCBase[c];
		float tight = 1.0;
#else
	for ( int c = 0; c < 1; c++ ) {
		vec4 O = cellDdgiOrigin, S = cellDdgiStep;
		ivec4 Dm = cellDdgiDims;
		int base = 0;
		float tight = 0.0;
#endif
		vec3 g = ( P - O.xyz ) / S.xyz;
		ivec3 b = clamp( ivec3( floor( g ) ), ivec3( 0 ), max( Dm.xyz - 2, ivec3( 0 ) ) );
		vec3 a = clamp( g - vec3( b ), 0.0, 1.0 );
		vec3 e = min( g, vec3( Dm.xyz - 1 ) - g ) - tight;
		float fade = clamp( min( e.x, min( e.y, e.z ) ), 0.0, 1.0 );
		if ( fade <= 0.0 )
			continue;
		float v = 0.0, ws = 0.0;
		for ( int k = 0; k < cellGiLoop8; k++ ) {
			ivec3 o = ivec3( k % 2, ( k / 2 ) % 2, k / 4 );
			ivec3 q = min( b + o, Dm.xyz - 1 );
			int p = q.x + Dm.x * ( q.y + Dm.y * q.z );
			vec4 pr = texelFetch( cellDdgiAtlas, base + 192 * Dm.w + p );
			if ( int( pr.w + 0.5 ) != 2 )
				continue;
			vec3 pp = O.xyz + vec3( q ) * S.xyz + pr.xyz;
			vec3 t = mix( vec3( 1.0 ) - a, a, vec3( o ) );
			float w = t.x * t.y * t.z;
			vec3 bp = pp - P;
			float rr = length( bp );
			if ( rr > 0.0 ) {
				vec4 m = cellDdgiTile( p, bp * ( -1.0 / rr ), 16 );
				w *= cellDdgiChebyshev( rr, m.x, m.y );
			}
			vec3 bl = Lp - pp;
			float rl = length( bl );
			float cl = 1.0;
			if ( rl > 0.0 ) {
				vec4 m = cellDdgiTile( p, bl * ( 1.0 / rl ), 16 );
				cl = cellDdgiChebyshev( max( rl - 0.5 * O.w, 0.0 ), m.x, m.y );
			}
			v += w * cl;
			ws += w;
		}
		if ( ws <= 0.0 )
			continue;
		float take = fade * left;
		vis += take * v / ws;
		left -= take;
	}
	return vis + left;
}

// fix 3: light i's factor in the air: 1 for a light whose shadow cube is drawn (its map is the verdict), else the
// volume's (an unshadowed light; every light under ReSTIR, whose pixel ratio the air no longer reads: cellShadowF)
float airLeakLightK( int i, vec3 P, vec3 Lp )
{
	if ( ( volLeakRed & 4 ) != 0 )
		return 1.0;
	vec4 t4 = texelFetch( cellLights, i * CELL_TPL + 4 );
	bool cubed = cellShadowOn && t4.y > 0.5 && t4.x > -0.5;
#ifdef WW_CELL_RESTIR
	cubed = cubed && !cellRestirOn;
#endif
	return cubed ? 1.0 : airLeakLightVis( P, Lp );
}
#endif

/* lane FOGDRIFT1: the density factor at distance t along dirView, around 1 (the texture's exact mean subtracted, so
 * the mean density holds); fades to 1 with distance, the detail octave sooner (it would alias in the deep slices) */
float airDriftW( vec3 P, float t )
{
	float f = 1.0 - clamp( ( t - volDrift.y ) * volDrift.z, 0.0, 1.0 );
	if ( f <= 0.0 || !cellOn )
		return 1.0;
	float a = texture( volNoise, ( P - volDriftOfs[0].xyz ) * volDriftOfs[0].w ).r - volDrift.w;
	float b = texture( volNoise, ( P - volDriftOfs[1].xyz ) * volDriftOfs[1].w + vec3( 0.37, 0.61, 0.13 ) ).r - volDrift.w;
	return 1.0 + volDrift.x * f * ( a + 0.5 * f * b ) * ( 1.0 / 1.5 );
}

float airDrift( vec3 dirView, float t )
{
	return airDriftW( cellWorldPos( dirView * ( t / fogDistScale ) ), t );
}

// lane WEATHERBLEND4: the factor on the sun column's optical depth from posView (height z, the sun's rise mu): the drift's
// mean at 1/6, 1/2, 5/6 of the column, at most the noise's tile (4096 units) (lookdev_fog.glsl wwAirDriftSunK; WEATHERBLEND5)
float airDriftSunK( vec3 posView, float z, float mu )
{
	vec3 P = cellWorldPos( posView ), s = normalize( cellDirTo ) * min( max( airMed.w - z, 0.0 ) / max( mu, 0.02 ), 4096.0 );
	float t = length( posView ) * fogDistScale;
	if ( ( volAirDriftRed & 1 ) != 0 )
		return airDriftW( P + 0.5 * s, t );
	return ( airDriftW( P + s / 6.0, t ) + airDriftW( P + 0.5 * s, t ) + airDriftW( P + s * ( 5.0 / 6.0 ), t ) ) * ( 1.0 / 3.0 );
}

// lane FARGRID1: the cloud shadow's input, 1 = no clouds; lane CLOUD3 feeds it (ww_sunshadow.glsl ussCloudT: 1 when the
// row is off or its red noinject keeps the clouds out of the air)
float airCloudShadow( vec3 posView )
{
	return ussCloudT( posView );
}

// lane FARGRID1: the sun's visibility with the far grid on: min(cascades where they reach, the terrain's shadow-height
// map) x the clouds; no screen-space term (a froxel is not a pixel)
float airFarVis( vec3 posView )
{
	if ( ( volRed & 8 ) != 0 )
		return 1.0;
	float c = 1.0, t = 1.0;
	if ( ussOn != 0 ) {
		float w, inMap;
		c = mix( 1.0, ussCascades( posView, vec3( 0.0 ), vec3( 0.0 ), w ), w );
		if ( ( volFarMode & 6 ) != 6 )
			t = ussTerrain( posView, inMap, 0.0, false );	// SHADOWPAR1's 4-argument form, as the air tail calls it
	} else if ( volCsm ) {
		c = wwSunShadowGeo( posView );	// lane CLOUD3: the clouds once, below
	}
	return min( c, t ) * airCloudShadow( posView );
}

/* lane FOGGI1 (the sun gate): the Concord hardware store's air took most of its light from the sun's in-scatter (sun 35.7,
 * GI 23.8, lamps 2.5) where the room test calls it indoors, because every road to the sun's visibility that is not the
 * shadow map answers 1: past the cascades' reach (their fade), without cascades (the clouds alone), and the multiple-
 * scattering octaves' lift. Per sample, the GI's own evidence says how far that 1 may stand: the 8 room cells round P
 * (volCubeAll's labels and weights; indoors = both labels non-zero, room 0 the outdoors) and the probes' open-sky share
 * toward the sun (SPECOCC1's sky slot, cellSpecSky's read on the sun's direction; none packed or no evidence = 0).
 * gate = indoor share x (1 - sky share); exteriors only (indoors the directional light is the template's, not a sun).
 * Red WW_CELL_FOGGI_RED=sun: volSunRoom false, the sun as before. */
uniform bool volSunRoom;

float airIndoorK( vec3 P )
{
	if ( !cellGiOn || !cellGiRooms )
		return 0.0;
	vec3 gr = ( P - cellRoomsOrigin ) / cellRoomsCell - 0.5;
	ivec3 r0 = ivec3( floor( gr ) );
	vec3 fr = gr - vec3( r0 );
	float wAir = 0.0, wIn = 0.0;
	for ( int k = 0; k < 8; k++ ) {
		ivec3 o = ivec3( k & 1, ( k >> 1 ) & 1, ( k >> 2 ) & 1 );
		ivec3 c = r0 + o;
		vec3 t = mix( 1.0 - fr, fr, vec3( o ) );
		float w = t.x * t.y * t.z;
		if ( any( lessThan( c, ivec3( 0 ) ) ) || any( greaterThanEqual( c, ivec3( cellRoomsDims ) ) ) ) {
			wAir += w;	// outside the room grid: no evidence of a room, counted as outdoors
			continue;
		}
		ivec2 L = cellRoomUnpack( texelFetch( cellRooms, c, 0 ).r );
		if ( L.x == -1 )
			continue;	// solid
		wAir += w;
		if ( L.x != 0 && L.y != 0 )
			wIn += w;
	}
	return wAir > 0.0 ? wIn / wAir : 0.0;
}

float airSkyToward( vec3 P, vec3 D )
{
	if ( !cellGiOn || !cellGiSky || cellSpecSkyBase < 1.0 )
		return 0.0;
	vec3 t = ( P - cellGiOrigin ) / cellGiVoxel;
	float z = clamp( t.z, 0.5, cellGiDims.z - 0.5 );	// the slabs stack on z: never filter across one
	vec3 sl = vec3( D.x >= 0.0 ? 0.0 : 1.0, D.y >= 0.0 ? 2.0 : 3.0, D.z >= 0.0 ? 4.0 : 5.0 );
	vec3 w = D * D;
	float depth = float( textureSize( cellGi, 0 ).z );
	vec2 a = vec2( 0.0 );
	for ( int k = 0; k < 3; k++ ) {
		vec4 s = texture( cellGi, vec3( t.xy / cellGiDims.xy, ( ( cellSpecSkyBase + sl[k] ) * cellGiDims.z + z ) / depth ) );
		a += w[k] * s.ra;
	}
	return a.y < 0.01 ? 0.0 : cellGiEdgeFade( t.xy ) * clamp( a.x / a.y, 0.0, 1.0 );
}

// 0 the sun as before .. 1 the sun only where its shadow map says lit (P world, to the way to the sun)
float airSunGate( vec3 P, vec3 to )
{
	if ( !volSunRoom || cellInterior )
		return 0.0;
	float k = airIndoorK( P );
	return k > 0.0 ? k * ( 1.0 - airSkyToward( P, to ) ) : 0.0;
}

/* the shadow map's own verdict at posView, weighted by how far its cascades cover it (their fade), min the terrain's
 * shadow-height map, x the clouds: never more than the vis it replaces, and equal to it inside the cascades' reach. No
 * screen-space contact term (a froxel is not a pixel). Without cascades: 0 (no map says the sample is lit). */
float airSunMapVis( vec3 posView )
{
	if ( ( volRed & 8 ) != 0 )
		return 1.0;
	float c = 0.0;
	if ( csmForce >= 0.0 ) {
		c = csmForce;
	} else if ( ussOn != 0 ) {
		float w, inMap;
		float casc = ussCascades( posView, vec3( 0.0 ), vec3( 0.0 ), w );
		c = w * casc;
		if ( ( volFarMode & 6 ) != 6 )
			c = min( c, ussTerrain( posView, inMap, 0.0, false ) );
	} else if ( volCsm ) {
		float dv = -posView.z * csmParams.x;
		if ( dv <= csmParams.y + csmParams.w ) {
			float s = clamp( dot( posView, posView ) * csmParams.x * csmParams.x / ( csmParams.y * csmParams.y ), 0.0, 1.0 );
			float s2 = s * s;
			c = csmBlended( posView ) * ( 1.0 - s2 * s2 );
		}
	}
	return c * airCloudShadow( posView );
}

/* lane GLAREBLOW1: the octaves' a at height z. Oz's a = 1/2 is a thick cloud's (half the scattered light meets the
 * medium again); the clear air near the ground lets most of it leave, so the octaves gave 1.2-1.6x the multiple
 * scattering of a Monte Carlo of the same medium and phase, the share that took the sky near the sun over 1.0
 * (notes_cloud/GLAREBLOW1/RESULTS.md). a = min(1/2, f), f the chance that light scattered at z meets the air again:
 * half the sphere up through the column to the top, half down to the ground, each 1 - E2(tau) (4-point Gauss-Legendre
 * over mu), the light reaching the ground back at albedo 0.2. CPU twin: src/gl/airmsweight.h (tests/glareblow1_test.cpp). */
float airMsEsc( float tau )
{
	return 0.1739274226 * exp( -tau / 0.0694318442 ) + 0.3260725774 * exp( -tau / 0.3300094782 )
		+ 0.3260725774 * exp( -tau / 0.6699905218 ) + 0.1739274226 * exp( -tau / 0.9305681558 );
}

float airMsA( float z )
{
	if ( !volMsMedium )
		return volMs.x;
	float eu = airMsEsc( z < airMed.w ? airMed.x * airCol( z, airMed.w ) : 0.0 );
	float ed = airMsEsc( airMed.x * airCol( min( airGz, z ), z ) );
	return min( volMs.x, 0.5 * ( 1.0 - eu ) + 0.5 * ( 1.0 - ed ) + 0.5 * ed * 0.2 );
}

/* lane GAPLOOK1 (FOGEXTRAS): the multiple-scattering octaves (Wrenninge, Kulla, Lundqvist, "Oz: The Great and
 * Volumetric", SIGGRAPH 2013): octave i scatters a^i, sees the optical depth to the sun x b^i and the phase's g x c^i;
 * vis the sun's shadow, lifted toward 1 as b^i shrinks (w 1). Octave 0 is the single scattering. lane FOGGI1: hard (the
 * sun gate) takes the lift away, the octaves then see vis as the single scattering does. */
vec3 airMsSum( float vis, float T, float c, float hard, float a0 )
{
	vec3 r = vis * T * volPhasePi( c );
	float a = 1.0, b = 1.0, g = 1.0;
	for ( int i = 1; i < 3; i++ ) {
		a *= a0;
		b *= volMs.y;
		g *= volMs.z;
		float v = volMs.w > 0.5 ? mix( mix( 1.0, vis, b ), vis, hard ) : vis;
		vec3 ph = PI * ( volPhase[0].rgb * ( 3.0 / ( 16.0 * PI ) ) * ( 1.0 + c * c )
			+ volPhase[1].rgb * volHg( c, volPhase[1].w * g ) + volPhase[2].rgb * volHg( c, volPhase[2].w * g ) );
		r += a * v * pow( T, b ) * ph;
	}
	return r;
}

/* lane AIRMS1 (AIR-MS-LAMPS): a placed light's orders >= 2 over its e (the single scattering is e exp(-od) pi p(c)):
 * pi M = (K0 + K1 gbar) od exp(-a od) pi P2(c), P2 the phase with each HG lobe's g x s, fitted to a Monte Carlo of a
 * point lamp in the same medium (notes/airms1). CPU twin: src/gl/airmslamp.h (tests/airmslamp). */
vec3 airLampMs( float c, float od )
{
	float s = volMsLamp.w;
	vec3 ph = PI * ( volPhase[0].rgb * ( 3.0 / ( 16.0 * PI ) ) * ( 1.0 + c * c )
		+ volPhase[1].rgb * volHg( c, volPhase[1].w * s ) + volPhase[2].rgb * volHg( c, volPhase[2].w * s ) );
	return max( volMsLamp.x + volMsLamp.y * volGi.w, 0.0 ) * od * exp( -volMsLamp.z * od ) * ph;
}

#ifdef WW_CELL_AIR_BOUNCE
/* lane AIRBOUNCE1 (AIR-LAMPS-BOUNCE): a placed light's light bounced off the floor, over its e: the lit floor as a source
 * at the lamp's image (hl under the floor), pi B = K a (1 + m sigma D) 2 cos pi P2(c_s) (d / D)^2, P2 the phase with
 * each HG g x sb, fitted to a Monte Carlo of a lamp over a Lambertian floor (notes/airbounce1). CPU twin:
 * src/gl/airbouncelamp.h (tests/airbouncelamp). The floor: the terrain grid's, else volGndLamp.w under the lamp. Only
 * in the loader's WW_CELL_AIR_BOUNCE program (WW_CELL_AIR_BOUNCE_LAMPS=1): the off program is the base's. */
float airGndK = 1.0;	// 1 - the ray-traced volume's cover of the env share (where it covers, it holds the bounce already)
float airGndZ( vec3 L )
{
	return airTerrOn != 0 ? airTerrH( L.xy ) : L.z - volGndLamp.w;
}
vec3 airLampGnd( vec3 L, float hf, vec3 Pk, vec3 dW, float d, float od )
{
	float hl = L.z - hf, hx = Pk.z - hf;
	if ( hl <= 0.0 || hx < 0.0 )
		return vec3( 0.0 );
	vec3 v = vec3( L.xy, hf - hl ) - Pk;
	float D2 = dot( v, v ), D = sqrt( D2 ), c = dot( v, dW ) / D;
	vec3 ph = PI * ( volPhase[0].rgb * ( 3.0 / ( 16.0 * PI ) ) * ( 1.0 + c * c )
		+ volPhase[1].rgb * volHg( c, volPhase[1].w * volGndLamp.z ) + volPhase[2].rgb * volHg( c, volPhase[2].w * volGndLamp.z ) );
	return volGndLamp.x * ( 1.0 + volGndLamp.y * ( od / d ) * D ) * 2.0 * ( ( hx + hl ) / D ) * ph * ( d * d / D2 );
}
#endif

/* lane PERFFIX3: the sun's visibility for one air sample. The surfaces' read (wwSunShadow) evaluates its Vogel disc
 * (8..48 taps) with a cos, a sin and a sqrt per tap, rotated per point: offline that trigonometry, not the fetches, was
 * most of the inject's sun cost (tests/airsun: 32 taps 1.33 ms, the same disc from literal offsets 0.49 ms at 160x90x64).
 * The air takes the Vogel disc at 32 taps over the same radius (the preset's, or FO4CS's world radius) with the same
 * bias, its offsets literal and fixed in the map's texels (world-locked as the rotated one); no receiver plane, no normal
 * offset and no
 * screen-space contact (a froxel is neither a surface nor a pixel: the contact mask was read at the froxel's own
 * gl_FragCoord, the offset from the froxel grid's derivatives), min the terrain's map, x the clouds. Anything else
 * (no composition, the Poisson tier, a forced factor, the reds, a debug view, the Off tier's stable bias) keeps the
 * surfaces' read. volSunAir false (red WW_CELL_AIR_RED=sunpcf): the surfaces' read, as before. */
uniform bool volSunAir;

// the Vogel disc at 32 taps (r = sqrt((t + 0.5) / 32), t x 2.39996323 rad): literal offsets, the pattern fixed in the
// map's texels (so world-locked); 32 = 8..48 of the presets' range, as accurate as the rotated preset disc (tests/airsun A)
float airCsmVogel( int i, vec3 posView )
{
	vec3 q = csmUvd( i, posView );
	float zRef = q.z - ussCsmBias[i] * csmInvRange[i];
	float R = ussCsmFilter.y * mix( 0.8, 1.2, clamp( float( i ) * 0.5, 0.0, 1.0 ) );
	if ( ussCsmWorld.x > 0.5 )
		R = clamp( ussCsmWorld[i + 1] / max( ussCsmTexel[i], 1e-6 ), 0.5, 16.0 );
	vec2 k = vec2( R / csmParams.z );
	float s = 0.0;
	s += texture( csmMap, vec4( q.xy + vec2( 0.1250000, 0.0000000 ) * k, float( i ), zRef ) );
	s += texture( csmMap, vec4( q.xy + vec2( -0.1596450, 0.1462479 ) * k, float( i ), zRef ) );
	s += texture( csmMap, vec4( q.xy + vec2( 0.0244362, -0.2784383 ) * k, float( i ), zRef ) );
	s += texture( csmMap, vec4( q.xy + vec2( 0.2012222, 0.2624588 ) * k, float( i ), zRef ) );
	s += texture( csmMap, vec4( q.xy + vec2( -0.3692676, -0.0653182 ) * k, float( i ), zRef ) );
	s += texture( csmMap, vec4( q.xy + vec2( 0.3498025, -0.2225157 ) * k, float( i ), zRef ) );
	s += texture( csmMap, vec4( q.xy + vec2( -0.1170021, 0.4352419 ) * k, float( i ), zRef ) );
	s += texture( csmMap, vec4( q.xy + vec2( -0.2231357, -0.4296341 ) * k, float( i ), zRef ) );
	s += texture( csmMap, vec4( q.xy + vec2( 0.4841151, 0.1767981 ) * k, float( i ), zRef ) );
	s += texture( csmMap, vec4( q.xy + vec2( -0.5036411, 0.2078957 ) * k, float( i ), zRef ) );
	s += texture( csmMap, vec4( q.xy + vec2( 0.2427883, -0.5188245 ) * k, float( i ), zRef ) );
	s += texture( csmMap, vec4( q.xy + vec2( 0.1794144, 0.5720013 ) * k, float( i ), zRef ) );
	s += texture( csmMap, vec4( q.xy + vec2( -0.5407570, -0.3133797 ) * k, float( i ), zRef ) );
	s += texture( csmMap, vec4( q.xy + vec2( 0.6343695, -0.1394644 ) * k, float( i ), zRef ) );
	s += texture( csmMap, vec4( q.xy + vec2( -0.3871458, 0.5506751 ) * k, float( i ), zRef ) );
	s += texture( csmMap, vec4( q.xy + vec2( -0.0894397, -0.6901996 ) * k, float( i ), zRef ) );
	s += texture( csmMap, vec4( q.xy + vec2( 0.5490718, 0.4627583 ) * k, float( i ), zRef ) );
	s += texture( csmMap, vec4( q.xy + vec2( -0.7388785, 0.0305549 ) * k, float( i ), zRef ) );
	s += texture( csmMap, vec4( q.xy + vec2( 0.5389551, -0.5363323 ) * k, float( i ), zRef ) );
	s += texture( csmMap, vec4( q.xy + vec2( -0.0360582, 0.7797915 ) * k, float( i ), zRef ) );
	s += texture( csmMap, vec4( q.xy + vec2( -0.5128175, -0.6145268 ) * k, float( i ), zRef ) );
	s += texture( csmMap, vec4( q.xy + vec2( 0.8123596, 0.1093018 ) * k, float( i ), zRef ) );
	s += texture( csmMap, vec4( q.xy + vec2( -0.6883106, 0.4789086 ) * k, float( i ), zRef ) );
	s += texture( csmMap, vec4( q.xy + vec2( 0.1880861, -0.8360614 ) * k, float( i ), zRef ) );
	s += texture( csmMap, vec4( q.xy + vec2( 0.4350333, 0.7591911 ) * k, float( i ), zRef ) );
	s += texture( csmMap, vec4( q.xy + vec2( -0.8504484, -0.2713162 ) * k, float( i ), zRef ) );
	s += texture( csmMap, vec4( q.xy + vec2( 0.8261024, -0.3816803 ) * k, float( i ), zRef ) );
	s += texture( csmMap, vec4( q.xy + vec2( -0.3578882, 0.8551556 ) * k, float( i ), zRef ) );
	s += texture( csmMap, vec4( q.xy + vec2( -0.3194073, -0.8880338 ) * k, float( i ), zRef ) );
	s += texture( csmMap, vec4( q.xy + vec2( 0.8499086, 0.4466882 ) * k, float( i ), zRef ) );
	s += texture( csmMap, vec4( q.xy + vec2( -0.9440346, 0.2488445 ) * k, float( i ), zRef ) );
	s += texture( csmMap, vec4( q.xy + vec2( 0.5365958, -0.8345298 ) * k, float( i ), zRef ) );
	return s * ( 1.0 / 32.0 );
}

float airSunVis( vec3 posView )
{
	if ( !volSunAir || ussOn == 0 || ussCsmFilter.x < 0.5 || ussCsmFilter.w > 0.5 || csmForce >= 0.0 || csmRed != 0 || ussView != 0 || ussRed != 0
		|| ussCsmStable.w > 0.0 )
		return wwSunShadow( posView );
	float c = 1.0;
	if ( ussCsmOn != 0 && -posView.z * csmParams.x <= csmParams.y + csmParams.w ) {
		int a, b;
		float t;
		csmSelect( posView, a, b, t );
		float sA = t < 1.0 ? airCsmVogel( a, posView ) : 1.0;
		float sB = t > 0.0 ? airCsmVogel( b, posView ) : 1.0;
		float f = clamp( dot( posView, posView ) * csmParams.x * csmParams.x / ( csmParams.y * csmParams.y ), 0.0, 1.0 );
		f *= f;
		c = mix( 1.0, mix( sA, sB, t ), 1.0 - f * f );
	}
	float inMap;
	return min( c, ussTerrain( posView, inMap, 0.0, true ) ) * ussCloudT( posView );
}

// the directional light the air at posView (height z) scatters toward the eye
vec3 airSun( vec3 posView, vec3 dirView, float z )
{
	if ( ( volTerms & 1 ) == 0 )
		return vec3( 0.0 );
	if ( cellOn && cellHasDir ) {
		// the cell's directional light (outdoors the weather's sun, indoors the lighting template's): world space
		vec3 to = normalize( cellDirTo );
		float vis = ( volFarMode != 0 && !cellInterior ) ? airFarVis( posView )
			: ( cellInterior || ( volRed & 8 ) != 0 ) ? 1.0 : volCsm ? airSunVis( posView ) : airCloudShadow( posView );
		// lane FOGGI1: indoors with no sky toward the sun, only what the shadow map says is lit
		float hard = airSunGate( cellWorldPos( posView ), to );
		if ( hard > 0.0 )
			vis = mix( vis, min( vis, airSunMapVis( posView ) ), hard );
#ifdef WW_AIR_LEAK
		/* lane FOGLEAK1 fix 2. lane INTGI1 (FOGLEAK1-INT, a ruling): an interior that is not sunlit (cellInterior; a
		 * sunlit one shades as outdoors) has no shadow map for its lighting template's directional, so no cover: its
		 * share in the air takes the volume's gate alone (the miss share toward it: an opening such as the museum's open
		 * top keeps it, a closed room's air loses the beam; the bounce of it off the walls still reaches the air through
		 * fix 1's env share). The surfaces keep the directional unshadowed, as vanilla. Red nointsungate: indoors as before */
		vis = cellInterior ? min( vis, airLeakSun ) : airLeakSunGate( posView, vis );
#endif
		float Ts = airTupSun( z, to.z );	// lane FIX1101A: the ground layer only (red suncolumn: the whole column)
		if ( volDriftSun && volDrift.x > 0.0 )	// lane WEATHERBLEND4: the sun's light through the drifting air
			Ts = pow( Ts, airDriftSunK( posView, z, to.z ) );
		if ( volMs.x > 0.0 )
			return cellDirColor * airMsSum( vis, Ts, dot( to, cellWorldDir( dirView ) ), hard, airMsA( z ) );
		return cellDirColor * vis * Ts * volPhasePi( dot( to, cellWorldDir( dirView ) ) );
	}
	if ( fogSun.w > 0.0 ) {
		// Lookdev without the cell's lights: its sun (view space)
		float vis = volFarMode != 0 ? airFarVis( posView ) : ( volRed & 8 ) != 0 ? 1.0
			: volCsm ? airSunVis( posView ) : airCloudShadow( posView );
		float mu = dot( fogView.xyz, fogSun.xyz ) / length( fogView.xyz );
		if ( volMs.x > 0.0 )
			return fogSunColour.rgb * fogSun.w * airMsSum( vis, airTupSun( z, mu ), dot( dirView, fogSun.xyz ), 0.0, airMsA( z ) );
		return fogSunColour.rgb * fogSun.w * vis * airTupSun( z, mu ) * volPhasePi( dot( dirView, fogSun.xyz ) );
	}
	return vec3( 0.0 );
}

// the next light of the merged lists, ascending and once each (the flat loop's order); -1 at the end
int volSurfNext( inout ivec4 p, ivec4 e )
{
	int best = 0x7fffffff;
	for ( int k = 0; k < 4; k++ )
		if ( p[k] < e[k] )
			best = min( best, texelFetch( cellCluster, p[k] ).r );
	if ( best == 0x7fffffff )
		return -1;
	for ( int k = 0; k < 4; k++ )
		if ( p[k] < e[k] && texelFetch( cellCluster, p[k] ).r == best )
			p[k]++;
	return best;
}

/* lane GAPLOOK1 (LIGHTSYS-4): the surface lists of the column's centre ray over [sA, sB] (world P, Dw per game unit):
 * its tile, the 1-4 slices its view depth spans. False: walk every light (off, a flat-loop cluster, more slices). */
bool volSurfBegin( vec3 P, vec3 Dw, float sA, float sB, out ivec4 p, out ivec4 e )
{
	p = ivec4( 0 );
	e = ivec4( 0 );
	if ( !volSurfClusters || !cellClusterOn || any( notEqual( volLightShift, vec3( 0.0 ) ) ) )
		return false;
	ivec3 d = ivec3( cellClusterDims.xyz + 0.5 );
	ivec2 t = clamp( ivec2( floor( ( floor( gl_FragCoord.xy ) + 0.5 ) * volSurfPx / cellClusterDims.w ) ), ivec2( 0 ), d.xy - 1 );
	float za = dot( cellClusterDepth.xyz, P + Dw * sA ) + cellClusterDepth.w;
	float zb = dot( cellClusterDepth.xyz, P + Dw * sB ) + cellClusterDepth.w;
	int s0 = clamp( int( floor( ( log( max( min( za, zb ) * 0.999, 1e-6 ) ) - cellClusterLog.x ) * cellClusterLog.y ) ), 0, d.z - 1 );
	int s1 = clamp( int( floor( ( log( max( max( za, zb ) * 1.001, 1e-6 ) ) - cellClusterLog.x ) * cellClusterLog.y ) ), 0, d.z - 1 );
	if ( s1 - s0 > 3 )
		return false;
	for ( int k = 0; k < 4; k++ ) {
		if ( s0 + k > s1 )
			break;
		int c = ( ( s0 + k ) * d.y + t.y ) * d.x + t.x;
		int n = texelFetch( cellCluster, 2 * c + 1 ).r;
		if ( n < 0 )
			return false;
		p[k] = cellClusterBase + texelFetch( cellCluster, 2 * c ).r;
		e[k] = p[k] + n;
	}
	if ( volSurfRed )
		volSurfNext( p, e );
	return true;
}

// the rest of the light at posView (height z), once a froxel: the placed lights (returned) and the sky's share, the
// GI and the ambient (Jenv)
vec3 airRest( vec3 posView, vec3 dirView, float z, float sA, float sB, out vec3 Jenv, bool lights )
{
	Jenv = vec3( 0.0 );
	if ( !cellOn )
		return vec3( 0.0 );
	vec3 J = vec3( 0.0 );
	vec3 P = cellWorldPos( posView );
	vec3 dW = cellWorldDir( dirView );
	float zOff = z - P.z;	// the fog's height frame minus world z
	vec3 Dw = cellWorldPos( dirView * ( 1.0 / fogDistScale ) ) - cellWorldPos( vec3( 0.0 ) );	// world per game unit along the ray
	vec3 Pl = P - volLightShift;	// gates only: the lights moved (0 otherwise)
#ifdef WW_CELL_AIR_BOUNCE
	// lane AIRBOUNCE1: the leak walk and the env terms' flags before the lamps, so their bounce takes 1 - the volume's
	// env cover (airLightsQ too, after this call)
#ifdef WW_AIR_LEAK
	float leakFade;
	vec3 Jddgi = airLeakEnv( P, dW, leakFade );	// lane FOGLEAK1 fixes 1 + 2 (also sets airLeakSun)
#endif
	bool wantGi = ( volTerms & 4 ) != 0 && cellGiOn && ( volRed & 2 ) == 0;
	bool ambDalc = cellInterior ? cellHasDalc : cellExtDalc;
	bool wantAmb = ( volTerms & 8 ) != 0 && ambDalc && !( cellInterior && cellGiAmb == 2 );
#ifdef WW_AIR_LEAK
	airGndK = wantAmb && ( volLeakRed & 1 ) == 0 ? 1.0 - leakFade : 1.0;
#endif
#endif
	if ( ( volTerms & 2 ) != 0 && lights && ( volRed & 16 ) == 0 ) {
		// clustered: only the lights whose sphere can reach this cluster, in the same ascending order (a light left out
		// would add exactly nothing below: d >= radius)
		int n = cellLightCount, base = -1;
		ivec4 sp, se;
		bool surf = volSurfBegin( P, Dw, sA, sB, sp, se );
		if ( !surf && volClusterDims.x > 0 ) {
			ivec2 c = ivec2( gl_FragCoord.xy ) / volClusterDims.z;
			int ci = ( ( volLayer / volClusterDims.w ) * volClusterDims.y + c.y ) * volClusterDims.x + c.x;
			base = texelFetch( volClusters, 2 * ci ).r;
			n = texelFetch( volClusters, 2 * ci + 1 ).r;
		}
		int jb = 0;
		for ( int j = 0; surf || j < n || jb < cellBulbCount; j++ ) {
			int i = surf ? volSurfNext( sp, se ) : j < n ? ( base < 0 ? j : texelFetch( volClusters, base + j ).r ) : -1;
			if ( i < 0 ) {
				if ( jb >= cellBulbCount )
					break;
				i = cellBulbBase + jb++;	// lane NIFXLIGHT2: then the bulbs
				surf = false;
				j = n;
			}
			vec4 t0 = texelFetch( cellLights, i * CELL_TPL );
			if ( ( volRed & 262144 ) != 0 ) {	// red lightmid: the light at the slice's middle only (a small light
				float d = length( t0.xyz - Pl );	// between two middles adds nothing)
				if ( d >= t0.w || d < 0.001 )
					continue;
				vec3 L = ( t0.xyz - Pl ) / d, Lo;
				bool noSpec;
				vec3 e = cellLightE( i, Pl, L, Lo, noSpec ) * cellFogK( i );	// N = L: no N.L, the shadow lifted toward the light
				if ( all( equal( e, vec3( 0.0 ) ) ) )
					continue;
#ifdef WW_AIR_LEAK
				e *= airLeakLightK( i, P, t0.xyz + volLightShift );	// lane FOGLEAK1 fix 3
#endif
				J += e * exp( -airOd( z, t0.z + zOff, d ) ) * volPhasePi( dot( L, dW ) );
				continue;
			}
			// lane VOLAIR1 (LAMPAIR2): the light's mean over the slice's stretch of the ray. The stretch inside the
			// light's sphere, [s0, s1] game units from the middle, is sampled at four midpoints and weighted by its
			// share of the slice: a lamp smaller than a slice is caught wherever it sits in it.
			vec3 m = Pl - t0.xyz;
			float qa = dot( Dw, Dw ), qb = dot( Dw, m ), qc = dot( m, m ) - t0.w * t0.w;
			float disc = qb * qb - qa * qc;
			if ( disc <= 0.0 || qa <= 0.0 )
				continue;
			float sq = sqrt( disc );
			float s0 = max( ( -qb - sq ) / qa, sA ), s1 = min( ( -qb + sq ) / qa, sB );
			if ( s1 <= s0 )
				continue;
			vec3 Ji = vec3( 0.0 );
#ifdef WW_CELL_AIR_BOUNCE
			float hf = volGndLamp.x > 0.0 ? airGndZ( t0.xyz ) : 0.0;	// lane AIRBOUNCE1
#endif
			for ( int k = 0; k < 4; k++ ) {
				vec3 Pk = Pl + Dw * mix( s0, s1, ( float( k ) + 0.5 ) * 0.25 );
				float d = length( t0.xyz - Pk );
				if ( d >= t0.w || d < 0.001 )
					continue;
				vec3 L = ( t0.xyz - Pk ) / d, Lo;
				bool noSpec;
				vec3 e = cellLightE( i, Pk, L, Lo, noSpec ) * cellFogK( i );	// N = L: no N.L, the shadow lifted toward the light
				if ( all( equal( e, vec3( 0.0 ) ) ) )
					continue;
				float od = airOd( Pk.z + zOff, t0.z + zOff, d ), c = dot( L, dW );
				Ji += e * exp( -od ) * volPhasePi( c );
				if ( volMsLamp.x > 0.0 )
					Ji += e * airLampMs( c, od );
#ifdef WW_CELL_AIR_BOUNCE
				if ( volGndLamp.x > 0.0 )
					Ji += e * airLampGnd( t0.xyz, hf, Pk, dW, d, od ) * airGndK;
#endif
			}
#ifdef WW_AIR_LEAK
			if ( any( greaterThan( Ji, vec3( 0.0 ) ) ) )	// lane FOGLEAK1 fix 3
				Ji *= airLeakLightK( i, P, t0.xyz + volLightShift );
#endif
			J += Ji * ( 0.25 * ( s1 - s0 ) / max( sB - sA, 1e-6 ) );
		}
	}
#ifndef WW_CELL_AIR_BOUNCE
#ifdef WW_AIR_LEAK
	float leakFade;
	vec3 Jddgi = airLeakEnv( P, dW, leakFade );	// lane FOGLEAK1 fixes 1 + 2 (also sets airLeakSun)
#endif
	bool wantGi = ( volTerms & 4 ) != 0 && cellGiOn && ( volRed & 2 ) == 0;
	bool ambDalc = cellInterior ? cellHasDalc : cellExtDalc;
	bool wantAmb = ( volTerms & 8 ) != 0 && ambDalc && !( cellInterior && cellGiAmb == 2 );
#endif
	if ( !wantGi && !wantAmb )
		return J;
	vec3 E[6];
	float share[6];
	for ( int a = 0; a < 6; a++ ) {
		E[a] = vec3( 0.0 );
		share[a] = 0.0;
	}
	if ( cellGiOn ) {
		if ( ( volRed & 4 ) != 0 ) {
			for ( int a = 0; a < 6; a++ )
				E[a] = volFlat[a];
		} else {
			volCubeAll( P, E, share );
		}
	}
	if ( wantGi ) {
		vec3 Eg[6];
		for ( int a = 0; a < 6; a++ )
			Eg[a] = E[a] * ( 1.0 / PI );	// the surfaces' GI unit -> the ambient's (cellLit: gi = cellGiE / pi)
		Jenv += volCubeScatter( Eg, dW );
	}
	if ( wantAmb ) {
		// the share the GI replaced: indoors under the rule replace (the default), outdoors where the grid holds the sky
		bool replace = cellGiOn && ( cellInterior ? cellGiAmb == 1 : cellGiSky );
		const vec3 axes[6] = vec3[6]( vec3( 1, 0, 0 ), vec3( -1, 0, 0 ), vec3( 0, 1, 0 ), vec3( 0, -1, 0 ), vec3( 0, 0, 1 ), vec3( 0, 0, -1 ) );
		vec3 A[6];
		for ( int a = 0; a < 6; a++ )
			A[a] = cellAmbient( axes[a], P ) * ( replace ? 1.0 - share[a] : 1.0 );
#ifdef WW_AIR_LEAK
		Jenv += volCubeScatter( A, dW ) * ( 1.0 - leakFade ) + Jddgi;	// lane FOGLEAK1 fix 1: the volume where it covers
#else
		Jenv += volCubeScatter( A, dW );
#endif
	}
	return J;
}

// lane PERF2 (temporal): airRest's four light samples one by one (the strata in `need` only), each x the stretch's
// share of the slice; airRest's light term = 0.25 x their sum
void airLightsQ( vec3 posView, vec3 dirView, float z, float sA, float sB, int need, out vec3 Jq[4] )
{
	for ( int k = 0; k < 4; k++ )
		Jq[k] = vec3( 0.0 );
	if ( !cellOn || ( volTerms & 2 ) == 0 || ( volRed & 16 ) != 0 )
		return;
	vec3 P = cellWorldPos( posView );
	vec3 dW = cellWorldDir( dirView );
	float zOff = z - P.z;
	vec3 Dw = cellWorldPos( dirView * ( 1.0 / fogDistScale ) ) - cellWorldPos( vec3( 0.0 ) );
	vec3 Pl = P - volLightShift;
	int n = cellLightCount, base = -1;
	ivec4 sp, se;
	bool surf = volSurfBegin( P, Dw, sA, sB, sp, se );
	if ( !surf && volClusterDims.x > 0 ) {
		ivec2 c = ivec2( gl_FragCoord.xy ) / volClusterDims.z;
		int ci = ( ( volLayer / volClusterDims.w ) * volClusterDims.y + c.y ) * volClusterDims.x + c.x;
		base = texelFetch( volClusters, 2 * ci ).r;
		n = texelFetch( volClusters, 2 * ci + 1 ).r;
	}
	int jb = 0;
	for ( int j = 0; surf || j < n || jb < cellBulbCount; j++ ) {
		int i = surf ? volSurfNext( sp, se ) : j < n ? ( base < 0 ? j : texelFetch( volClusters, base + j ).r ) : -1;
		if ( i < 0 ) {
			if ( jb >= cellBulbCount )
				break;
			i = cellBulbBase + jb++;	// lane NIFXLIGHT2: then the bulbs
			surf = false;
			j = n;
		}
		vec4 t0 = texelFetch( cellLights, i * CELL_TPL );
		vec3 m = Pl - t0.xyz;
		float qa = dot( Dw, Dw ), qb = dot( Dw, m ), qc = dot( m, m ) - t0.w * t0.w;
		float disc = qb * qb - qa * qc;
		if ( disc <= 0.0 || qa <= 0.0 )
			continue;
		float sq = sqrt( disc );
		float s0 = max( ( -qb - sq ) / qa, sA ), s1 = min( ( -qb + sq ) / qa, sB );
		if ( s1 <= s0 )
			continue;
		float share = ( s1 - s0 ) / max( sB - sA, 1e-6 );
#ifdef WW_AIR_LEAK
		share *= airLeakLightK( i, P, t0.xyz + volLightShift );	// lane FOGLEAK1 fix 3
#endif
#ifdef WW_CELL_AIR_BOUNCE
		float hf = volGndLamp.x > 0.0 ? airGndZ( t0.xyz ) : 0.0;	// lane AIRBOUNCE1
#endif
		for ( int k = 0; k < 4; k++ ) {
			if ( ( need & ( 1 << k ) ) == 0 )
				continue;
			vec3 Pk = Pl + Dw * mix( s0, s1, ( float( k ) + 0.5 ) * 0.25 );
			float d = length( t0.xyz - Pk );
			if ( d >= t0.w || d < 0.001 )
				continue;
			vec3 L = ( t0.xyz - Pk ) / d, Lo;
			bool noSpec;
			vec3 e = cellLightE( i, Pk, L, Lo, noSpec ) * cellFogK( i );
			if ( all( equal( e, vec3( 0.0 ) ) ) )
				continue;
			float od = airOd( Pk.z + zOff, t0.z + zOff, d ), c = dot( L, dW );
			Jq[k] += e * exp( -od ) * volPhasePi( c ) * share;
			if ( volMsLamp.x > 0.0 )
				Jq[k] += e * airLampMs( c, od ) * share;
#ifdef WW_CELL_AIR_BOUNCE
			if ( volGndLamp.x > 0.0 )
				Jq[k] += e * airLampGnd( t0.xyz, hf, Pk, dW, d, od ) * ( share * airGndK );
#endif
		}
	}
}

void main()
{
	ivec2 col = ivec2( gl_FragCoord.xy );
	if ( volStage == 2 ) {
		vec3 L = vec3( 0.0 );
		float T = 1.0;
		for ( int j = 0; j <= volLayer; j++ ) {
			vec4 s = texelFetch( volInject, ivec3( col, volLayerBase + j ), 0 );
			L += T * s.rgb;
			T *= s.a;
		}
		fragColor = vec4( L, T );
		return;
	}
	if ( volStage == 3 ) {
		/* lane PERF2: the same running sum in the same order, once per column: up to 7 layers a draw into the
		 * attachments, the carry (L, T) beside them for the next draw (stage 2 re-summed layers 0..k for every k) */
		vec3 L = vec3( 0.0 );
		float T = 1.0;
		if ( volLayer > 0 ) {
			vec4 c = texelFetch( volCarry, col, 0 );
			L = c.rgb;
			T = c.a;
		}
		vec4 o[7];
		for ( int i = 0; i < 7; i++ ) {
			if ( i < volCount ) {
				vec4 s = texelFetch( volInject, ivec3( col, volLayerBase + volLayer + i ), 0 );	// BATCH1: FARGRID1's base (0 near)
				L += T * s.rgb;
				T *= s.a;
			}
			o[i] = vec4( L, T );
		}
		fragColor = o[0];
		volMrt[0] = o[1];
		volMrt[1] = o[2];
		volMrt[2] = o[3];
		volMrt[3] = o[4];
		volMrt[4] = o[5];
		volMrt[5] = o[6];
		volMrt[6] = vec4( L, T );
		return;
	}
	if ( volStage == 0 ) {
		// the column's opaque ray distances: its pixels (the column's share of the g-buffer, one pixel of margin)
		ivec2 p0 = max( ivec2( floor( vec2( col ) * vec2( volGbufSize ) / vec2( volDims.xy ) ) ) - 1, ivec2( 0 ) );
		ivec2 p1 = min( ivec2( ceil( vec2( col + 1 ) * vec2( volGbufSize ) / vec2( volDims.xy ) ) ) + 1, volGbufSize );
		float far = 0.0, near = 3.0e38;
		float k = fogDistScale / max( length( cellRow[0].xyz ), 1e-30 );
		for ( int y = p0.y; y < p1.y; y++ ) {
			for ( int x = p0.x; x < p1.x; x++ ) {
				float t = 3.0e38;
				vec2 n = ( vec2( x, y ) + 0.5 ) / vec2( volGbufSize ) * 2.0 - 1.0;
				if ( volGbufZ != 0 ) {
					// two 24-bit steps farther: the stored depth is rounded, the fragments' own distances are not
					float z = texelFetch( volGbuf, ivec2( x, y ), 0 ).r + 2.0 / 16777215.0;
					if ( z < 1.0 )
						t = max( volProjZ.y / ( 2.0 * z - 1.0 + volProjZ.x ), 0.0 ) * fogDistScale
							* length( vec3( n.x / volProj.x, n.y / volProj.y, 1.0 ) );
				} else {
					float a = texelFetch( volGbuf, ivec2( x, y ), 0 ).a;
					if ( a < 5.0e5 )
						t = max( a, 0.0 ) * k * length( vec3( n.x / volProj.x, n.y / volProj.y, 1.0 ) );
				}
				far = max( far, t );
				near = min( near, t );
			}
		}
		fragColor = vec4( far, near, 0.0, 1.0 );
		return;
	}
	if ( volDepthSkip != 0 && volLayer > 0 ) {
		// the conservative skip: no pixel's apply reads this froxel when the PREVIOUS slice already starts past the
		// farthest surface of the column and its neighbours (the bilinear's reach); (0, 1) keeps the integrate whole.
		// Red nearskip: behind the column's own nearest surface, from this slice (visible froxels skipped)
		float lim = 0.0, edge;
		if ( volDepthSkip == 2 ) {
			lim = texelFetch( volTile, col, 0 ).g;
			edge = volSlices.x * exp( volSlices.y * float( volLayer ) / volSlices.z );
		} else {
			for ( int dy = -1; dy <= 1; dy++ )
				for ( int dx = -1; dx <= 1; dx++ )
					lim = max( lim, texelFetch( volTile, clamp( col + ivec2( dx, dy ), ivec2( 0 ), volDims.xy - 1 ), 0 ).r );
			edge = volSlices.x * exp( volSlices.y * float( volLayer - 1 ) / volSlices.z );
			lim = lim * 1.001 + 1.0;
		}
		if ( edge > lim ) {
			fragColor = vec4( 0.0, 0.0, 0.0, 1.0 );
			volMrt[0] = volMrt[1] = volMrt[2] = volMrt[3] = vec4( 0.0 );	// temporal: no history here
			return;
		}
	}
	vec2 ndc = ( vec2( col ) + 0.5 ) / vec2( volDims.xy ) * 2.0 - 1.0;
	vec3 dirView = normalize( vec3( ndc.x / volProj.x, ndc.y / volProj.y, -1.0 ) );
	float t0 = volSlices.x * exp( volSlices.y * ( float( volLayer ) + volJitter ) / volSlices.z );
	float t1 = volSlices.x * exp( volSlices.y * ( float( volLayer + 1 ) + volJitter ) / volSlices.z );
	// height along the ray: z(t) = zc + dzdt x t (t in game units)
	float zc = fogView.w;
	float dzdt = dot( fogView.xyz, dirView ) / fogDistScale;
	float tm = 0.5 * ( t0 + t1 );
	// lane PERF2: the sub-steps' optical depths first; no weight in any of them (air above the top, or none) adds
	// exactly 0, so the lights, GI and ambient are skipped. Red noearly (536870912): airRest always, as before
	float tauQ[4];
#ifdef WW_CELL_AIRZONE
	vec3 azTintQ[4] = vec3[4]( vec3( 1.0 ), vec3( 1.0 ), vec3( 1.0 ), vec3( 1.0 ) );	// lane AIRZONE1: 1 outside every zone
#endif
	bool scatters = ( volRed & 536870912 ) != 0;
	for ( int q = 0; q < 4; q++ ) {
		float ta = mix( t0, t1, float( q ) * 0.25 ), tb = mix( t0, t1, float( q + 1 ) * 0.25 );
		if ( airTerrOn != 0 )	// FOGTERRAIN1: the ground layer stands on the terrain under each sub-step
			airGroundAt( cellWorldPos( dirView * ( 0.5 * ( ta + tb ) / fogDistScale ) ) );
		tauQ[q] = airOd( zc + dzdt * ta, zc + dzdt * tb, tb - ta );
		if ( volDrift.x > 0.0 )	// lane FOGDRIFT1: one noisy density for every term below (the lamps' glow too)
			tauQ[q] *= airDrift( dirView, 0.5 * ( ta + tb ) );
#ifdef WW_CELL_AIRZONE
		if ( azCount > 0 ) {	// lane AIRZONE1: the volumes' fog at the sub-step's middle (x scale + add len; outside: exactly as is)
			AzFog zf = azFogAt( cellWorldPos( dirView * ( 0.5 * ( ta + tb ) / fogDistScale ) ) );
			tauQ[q] = tauQ[q] * zf.k + zf.add * ( tb - ta );
			azTintQ[q] = zf.tint;
		}
#endif
		if ( rfOn != 0 )	// lane CLOUD6e: the rain fog at the sub-step's middle, x its length (WW_CLOUD_RAINFOG)
			tauQ[q] += rfSigma( dirView * ( 0.5 * ( ta + tb ) / fogDistScale ) ) * ( tb - ta );
		scatters = scatters || air1mE( tauQ[q] ) > 0.0;
	}
	float keep = fogK[0].x + fogK[1].x + fogK[2].x + fogK[3].x + fogK[4].x + fogK[5].x;
	if ( airTerrOn != 0 )	// the lights, sun and GI terms: the ground under the slice's middle
		airGroundAt( cellWorldPos( dirView * ( tm / fogDistScale ) ) );
	if ( volTemporal != 0 ) {
		/* lane PERF2: temporal reuse, as shipped games reuse past fog frames. One stratum (sub-step) a frame is fresh: its
		 * sun and its placed-light samples, the costly terms. The other three come from the history at this froxel's
		 * centre last frame. The medium, the GI and the ambient stay exact every frame. A stratum is at most 3 frames
		 * old; a froxel missing any history (a cut, off the last frustum, a skipped froxel) takes all four, so a still
		 * camera holds the 4-sub-step sum after 4 frames. History = stratum light / (1 - a): it moves between slices. */
		float Tq[4], wq[4];
		float T = 1.0, W = 0.0;
		for ( int q = 0; q < 4; q++ ) {
			Tq[q] = T;
			wq[q] = air1mE( tauQ[q] );
			W += T * wq[q];
			T *= exp( -tauQ[q] );
		}
		if ( !scatters || !( W > 0.0 ) ) {
			volMrt[0] = volMrt[1] = volMrt[2] = volMrt[3] = vec4( 0.0, 0.0, 0.0, 1.0 );
			fragColor = vec4( 0.0, 0.0, 0.0, T );
			return;
		}
		vec3 posM = dirView * ( tm / fogDistScale );
		vec4 h[4];
		bool have = volHistOk;
		if ( have && volTemporal == 2 ) {
			for ( int p = 0; p < 4; p++ )
				h[p] = texelFetch( volHist, ivec3( col, p * volDims.z + volLayer ), 0 );
		} else if ( have ) {
			vec4 pm4 = vec4( posM, 1.0 );
			vec3 pp = vec3( dot( volReproj[0], pm4 ), dot( volReproj[1], pm4 ), dot( volReproj[2], pm4 ) );
			vec2 uv = vec2( pp.x * volPrevGrid.x, pp.y * volPrevGrid.y ) / max( -pp.z, 1e-30 ) * 0.5 + 0.5;
			// the slice coordinate, offset so this froxel's own centre maps to its texel's middle
			float kc = log( tm / volSlices.x ) / volSlices.y * volSlices.z;
			float kp = log( max( length( pp ) * volPrevScale, 1e-6 ) / volPrevGrid.z ) / volPrevGrid.w * volSlices.z
				+ ( float( volLayer ) + 0.5 - kc );
			have = pp.z < 0.0 && all( greaterThanEqual( uv, vec2( 0.0 ) ) ) && all( lessThanEqual( uv, vec2( 1.0 ) ) )
				&& kp >= 0.0 && kp <= volSlices.z;
			kp = clamp( kp, 0.5, volSlices.z - 0.5 );	// the filter stays inside its stratum's layers
			for ( int p = 0; p < 4; p++ )
				h[p] = have ? texture( volHist, vec3( uv, ( float( p ) * volSlices.z + kp ) / ( 4.0 * volSlices.z ) ) ) : vec4( 0.0 );
		}
		int need = 15;
		if ( have ) {
			// every filter tap valid (a = 1); red histstale (134217728, gates): the history never refreshed
			bool stale = ( volRed & 134217728 ) != 0, ok = true;
			for ( int p = 0; p < 4; p++ )
				ok = ok && ( ( p == volQ && !stale ) || h[p].a > 0.9999 );
			if ( ok )
				need = stale ? 0 : 1 << volQ;
		}
		vec3 Jenv = vec3( 0.0 );
		airRest( posM, dirView, zc + dzdt * tm, t0 - tm, t1 - tm, Jenv, false );
		vec3 Jq[4];
		airLightsQ( posM, dirView, zc + dzdt * tm, t0 - tm, t1 - tm, need, Jq );
		vec3 acc = vec3( 0.0 );
		vec4 so[4];
		for ( int q = 0; q < 4; q++ ) {
			float ta = mix( t0, t1, float( q ) * 0.25 ), tb = mix( t0, t1, float( q + 1 ) * 0.25 );
			float t = 0.5 * ( ta + tb ), zt = zc + dzdt * t;
			vec3 Cq;
			if ( ( need & ( 1 << q ) ) != 0 ) {
				Cq = 0.25 * W * Jq[q] AZ_TINT( q );
				if ( wq[q] > 0.0 )
					Cq += Tq[q] * wq[q] * airEnvColour( airSun( dirView * ( t / fogDistScale ), dirView, zt ), t, zt, dirView ) AZ_TINT( q );
				so[q] = vec4( Cq / W, 1.0 );
			} else {
				Cq = h[q].rgb * W;
				so[q] = vec4( h[q].rgb, 1.0 );
			}
			if ( wq[q] > 0.0 )
				acc += Tq[q] * wq[q] * airEnvColourEnv( Jenv, t, zt, dirView ) AZ_TINT( q );	// lane FIX1101A: the env share without the sun glare
			acc += Cq;
		}
		volMrt[0] = so[0];
		volMrt[1] = so[1];
		volMrt[2] = so[2];
		volMrt[3] = so[3];
		if ( !fogOn || keep < -1e30 )
			acc = vec3( 0.0 );
		fragColor = vec4( acc, T );
		return;
	}
	vec3 Jenv = vec3( 0.0 );
	vec3 Jrest = vec3( 0.0 );
	if ( scatters )
		Jrest = airRest( dirView * ( tm / fogDistScale ), dirView, zc + dzdt * tm, t0 - tm, t1 - tm, Jenv, true );
	vec3 acc = vec3( 0.0 );
	float T = 1.0;
	for ( int q = 0; q < 4; q++ ) {
		float ta = mix( t0, t1, float( q ) * 0.25 ), tb = mix( t0, t1, float( q + 1 ) * 0.25 );
		float tau = tauQ[q];
		float w = air1mE( tau );
		if ( w > 0.0 ) {
			float t = 0.5 * ( ta + tb );
			float zt = zc + dzdt * t;
			// lane FIX1101A: the sun's share (shadowed) keeps the records' glare hue, the env share takes the fog colour only
			acc += T * w * ( Jrest + airEnvColourEnv( Jenv, t, zt, dirView ) + airEnvColour( airSun( dirView * ( t / fogDistScale ), dirView, zt ), t, zt, dirView ) ) AZ_TINT( q );
		}
		T *= exp( -tau );
	}
	// the uniforms the CPU reads back must stay live: fogK all six (the pass fits the medium to them; a stripped
	// fogK reads back as zeros and the pass refused with "fog scale 0", lane VOLAIR1 10-13)
	if ( !fogOn || keep < -1e30 )
		acc = vec3( 0.0 );
	fragColor = vec4( acc, T );
}
