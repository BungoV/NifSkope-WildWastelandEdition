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

/* lane GAPLOOK1 (FOGEXTRAS): the multiple-scattering octaves (Wrenninge, Kulla, Lundqvist, "Oz: The Great and
 * Volumetric", SIGGRAPH 2013): octave i scatters a^i, sees the optical depth to the sun x b^i and the phase's g x c^i;
 * vis the sun's shadow, lifted toward 1 as b^i shrinks (w 1). Octave 0 is the single scattering. lane FOGGI1: hard (the
 * sun gate) takes the lift away, the octaves then see vis as the single scattering does. */
vec3 airMsSum( float vis, float T, float c, float hard )
{
	vec3 r = vis * T * volPhasePi( c );
	float a = 1.0, b = 1.0, g = 1.0;
	for ( int i = 1; i < 3; i++ ) {
		a *= volMs.x;
		b *= volMs.y;
		g *= volMs.z;
		float v = volMs.w > 0.5 ? mix( mix( 1.0, vis, b ), vis, hard ) : vis;
		vec3 ph = PI * ( volPhase[0].rgb * ( 3.0 / ( 16.0 * PI ) ) * ( 1.0 + c * c )
			+ volPhase[1].rgb * volHg( c, volPhase[1].w * g ) + volPhase[2].rgb * volHg( c, volPhase[2].w * g ) );
		r += a * v * pow( T, b ) * ph;
	}
	return r;
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
			: ( cellInterior || ( volRed & 8 ) != 0 ) ? 1.0 : volCsm ? wwSunShadow( posView ) : airCloudShadow( posView );
		// lane FOGGI1: indoors with no sky toward the sun, only what the shadow map says is lit
		float hard = airSunGate( cellWorldPos( posView ), to );
		if ( hard > 0.0 )
			vis = mix( vis, min( vis, airSunMapVis( posView ) ), hard );
		float Ts = airTup( z, to.z );
		if ( volDriftSun && volDrift.x > 0.0 )	// lane WEATHERBLEND4: the sun's light through the drifting air
			Ts = pow( Ts, airDriftSunK( posView, z, to.z ) );
		if ( volMs.x > 0.0 )
			return cellDirColor * airMsSum( vis, Ts, dot( to, cellWorldDir( dirView ) ), hard );
		return cellDirColor * vis * Ts * volPhasePi( dot( to, cellWorldDir( dirView ) ) );
	}
	if ( fogSun.w > 0.0 ) {
		// Lookdev without the cell's lights: its sun (view space)
		float vis = volFarMode != 0 ? airFarVis( posView ) : ( volRed & 8 ) != 0 ? 1.0
			: volCsm ? wwSunShadow( posView ) : airCloudShadow( posView );
		float mu = dot( fogView.xyz, fogSun.xyz ) / length( fogView.xyz );
		if ( volMs.x > 0.0 )
			return fogSunColour.rgb * fogSun.w * airMsSum( vis, airTup( z, mu ), dot( dirView, fogSun.xyz ), 0.0 );
		return fogSunColour.rgb * fogSun.w * vis * airTup( z, mu ) * volPhasePi( dot( dirView, fogSun.xyz ) );
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
				Ji += e * exp( -airOd( Pk.z + zOff, t0.z + zOff, d ) ) * volPhasePi( dot( L, dW ) );
			}
			J += Ji * ( 0.25 * ( s1 - s0 ) / max( sB - sA, 1e-6 ) );
		}
	}
	bool wantGi = ( volTerms & 4 ) != 0 && cellGiOn && ( volRed & 2 ) == 0;
	bool ambDalc = cellInterior ? cellHasDalc : cellExtDalc;
	bool wantAmb = ( volTerms & 8 ) != 0 && ambDalc && !( cellInterior && cellGiAmb == 2 );
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
		Jenv += volCubeScatter( A, dW );
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
			Jq[k] += e * exp( -airOd( Pk.z + zOff, t0.z + zOff, d ) ) * volPhasePi( dot( L, dW ) ) * share;
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
	bool scatters = ( volRed & 536870912 ) != 0;
	for ( int q = 0; q < 4; q++ ) {
		float ta = mix( t0, t1, float( q ) * 0.25 ), tb = mix( t0, t1, float( q + 1 ) * 0.25 );
		if ( airTerrOn != 0 )	// FOGTERRAIN1: the ground layer stands on the terrain under each sub-step
			airGroundAt( cellWorldPos( dirView * ( 0.5 * ( ta + tb ) / fogDistScale ) ) );
		tauQ[q] = airOd( zc + dzdt * ta, zc + dzdt * tb, tb - ta );
		if ( volDrift.x > 0.0 )	// lane FOGDRIFT1: one noisy density for every term below (the lamps' glow too)
			tauQ[q] *= airDrift( dirView, 0.5 * ( ta + tb ) );
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
				Cq = 0.25 * W * Jq[q];
				if ( wq[q] > 0.0 )
					Cq += Tq[q] * wq[q] * airEnvColour( airSun( dirView * ( t / fogDistScale ), dirView, zt ), t, zt, dirView );
				so[q] = vec4( Cq / W, 1.0 );
			} else {
				Cq = h[q].rgb * W;
				so[q] = vec4( h[q].rgb, 1.0 );
			}
			if ( wq[q] > 0.0 )
				acc += Tq[q] * wq[q] * airEnvColour( Jenv, t, zt, dirView );
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
			acc += T * w * ( Jrest + airEnvColour( Jenv + airSun( dirView * ( t / fogDistScale ), dirView, zt ), t, zt, dirView ) );
		}
		T *= exp( -tau );
	}
	// the uniforms the CPU reads back must stay live: fogK all six (the pass fits the medium to them; a stripped
	// fogK reads back as zeros and the pass refused with "fog scale 0", lane VOLAIR1 10-13)
	if ( !fogOn || keep < -1e30 )
		acc = vec3( 0.0 );
	fragColor = vec4( acc, T );
}
