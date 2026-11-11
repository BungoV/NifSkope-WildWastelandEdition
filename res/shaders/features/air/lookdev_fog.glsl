// Lookdev weather fog (lane FOG1): the engine fog formula, a line-for-line port
// of the composite shader as transcribed in scratchpad/pbrprep1_20260924/spec_fog.md
// 2.4 (FO4CS ambient_ibl_pass.hlsl F4FXApplyEngineFog), fed the cb12[41..46]
// packing (fogK) that gl/lookdevstage.cpp builds from the WTHR (esmweather.cpp
// wwFogAt). Applied per fragment to scene geometry in LINEAR light, before the
// exposure and the view transform (studioOutput); the sky dome never takes the game's
// fog (lane VOLAIR1: it takes the air, cell_volsky.frag, wwVolAirSky below).
// The fog-sun term's colour and intensity are INFERRED (the lookdev sun: its
// direction, the Sunlight row, intensity 1); the power is fDirectionalFogPower.
// Needs: posView = the fragment's view-space position (game units x view scale).

#define WW_LOOKDEV_FOG_GLSL 1	// lane SHADOWPAR1: ww_sunshadow.glsl's air-tail reader needs fogDistScale
uniform bool fogOn;
uniform vec4 fogK[6];		// cb12[41..46]
uniform vec4 fogView;		// world z above the ground = dot( fogView.xyz, posView ) + fogView.w
uniform float fogDistScale;	// view units -> game units (1 / the view scale)
uniform vec4 fogSun;		// view space, unit, TO the sun; .w = intensity
uniform vec4 fogSunColour;	// linear; .w = fDirectionalFogPower
uniform float fogUnit;	// lane PHYSUNITS1: the light's scale over the weather's (0 = unset = 1)
uniform vec4 fogProbe;		// gates only: .z = 0 off, 1 alpha, 2 colour / 2, 3 hb (at d = .x, z = .y); 5 geometry echo (d / 4096, 0.5 + z / 2000);
				// 6 / 7 per fragment (lane FOG2): (alpha, hb, 0) / colour ^ 1/2.2; 8 the geometry, wide (d / 16384, 0.5 + z / 32768)
uniform int fogRed;		// gates only: 1 maxclamp, 2 noescape, 4 height0; 8 the air tail's source from the last slice alone (MODULES1 R7-4)
				// 16 the sky's level ray on the old branch (MODULES1 R7 item 6)
/* MODULES1 R7 night sky d: the saturated air's night colour x the night share (a: the share, 0 = day, the red
 * WW_NIGHTAIR_RED=1, no weather). Master Off: the weather record's far fog colour, what the game's fog ends at; On: the
 * sky's mean with the record's night level (the ambient axes' mean; the air's albedo is 1). The whole air of a ray
 * (L, T) holds at least that x (1 - T): the sky's cover (cell_volsky.frag) and the surfaces (wwFog) alike, so the far
 * land and the sky behind it end at one colour. */
uniform vec4 fogNight;

/* lane SKYA (ATMO2): the physical sky's aerial perspective (src/features/sky/atmosphere.h, Hillaire 2020 sec. 5.4). atApTex
 * holds per froxel the light the air sends toward the eye from the camera to slice k (rgb, the sky's scene units)
 * and the mean transmittance (a); slice k's centre sits at maxKm ((k + 0.5) / n)^2, so the texture's depth
 * coordinate is sqrt(d / maxKm). Off (atAp.x 0): every path below is the one before the lane. */
#include "atmo_apfog.glsl"
uniform sampler3D atApTex;
uniform vec4 atAp;		// 1 = on, maxKm, km per view unit, slices
uniform vec4 atApProj;		// the projection the volume was built for: P00, P11, P20, P21
uniform float atApDouble;	// red "double": the fog laid over the whole aerial perspective

bool wwApOn()
{
	return atAp.x > 0.5;
}

// (L, T) of the sky's air between the eye and posView (view space)
vec4 wwAtmoApAt( vec3 posView )
{
	float dKm = length( posView ) * atAp.z;
	float zv = max( -posView.z, 1e-6 );
	vec2 ndc = vec2( atApProj.x * posView.x + atApProj.z * posView.z, atApProj.y * posView.y + atApProj.w * posView.z ) / zv;
	vec2 uv = clamp( ndc * 0.5 + 0.5, 0.0, 1.0 );
	float n = atAp.w;
	float s = sqrt( clamp( dKm / atAp.y, 0.0, 1.0 ) );
	vec4 v;
	if ( s < 0.5 / n ) {
		// nearer than slice 0's centre: its (L, 1 - T) scaled by the distance (both linear there)
		v = texture( atApTex, vec3( uv, 0.5 / n ) );
		float f = s * n * 2.0;
		f *= f;
		v = vec4( v.rgb * f, 1.0 - ( 1.0 - v.a ) * f );
	} else {
		v = texture( atApTex, vec3( uv, s ) );
	}
	return vec4( max( v.rgb, vec3( 0.0 ) ), clamp( v.a, 0.0, 1.0 ) );
}

// fog alpha, height blend and colour (before the sun term) of one fragment
float wwFogEval( float d, float z, out float hb, out vec3 fogCol )
{
	if ( ( fogRed & 4 ) != 0 )
		z = 0.0;
	float ramp = d * fogK[0].x - fogK[0].z;
	float f = clamp( ramp, 0.0, 1.0 );
	vec2 pair = clamp( z * fogK[5].xy - fogK[5].zw, 0.0, 1.0 );
	hb = mix( pair.x, pair.y, f );
	float mx = fogK[2].w;
	float clampT = ( ramp > 0.75 && ( fogRed & 1 ) == 0 ) ? min( ( f - 0.75 ) * 4.0 * ( 1.0 - mx ) + mx, 1.0 ) : mx;
	float escape = ( ramp < 0.015 && ( fogRed & 2 ) == 0 ) ? f * 66.666672 : 1.0;
	float I = f > 0.0 ? min( clampT, pow( f, fogK[1].w ) ) : 0.0;
	float weight = hb * fogK[3].w + ( 1.0 - hb );
	fogCol = mix( mix( fogK[1].rgb, fogK[3].rgb, I ), mix( fogK[2].rgb, fogK[4].rgb, I ), hb );
	return weight * I * escape;
}

/* lane VOLAIR1 (src/features/air/cellvolair.h): the air IN PLACE of the game's fog. volTex holds, per froxel column, the
 * light the air sends toward the eye from the near plane to the far edge of slice k (rgb) and the transmittance
 * over the same stretch (a); the slices are exponential from volSlices.x over ln(far / near) = volSlices.y. A
 * surface at distance d shows colour x T(d) + L(d), linear in log-depth. Linear light. */
uniform bool volOn;
uniform sampler3D volTex;
uniform vec4 volRect;		// the viewport's origin, 1 / its size
uniform sampler2D volTile;	// the air pass's stage 0: per froxel column the farthest, nearest opaque ray distance (game units)
uniform int volFarClamp;	// 1: the lookup clamped to its column's farthest (red nofarclamp, and the programs without it: 0)
uniform vec4 volSlices;		// near, ln(far / near), slices, 1 = applied (0 under the red "off": the game's fog drawn)
uniform int volProbe;		// gates only: 1 the light term L alone, raw; 2 the transmittance T, grey; 3 the sun's T to the surface
uniform int volFogBlend;	// test only (WW_CELL_VOLAIR_FOGBLEND=1, option B): the game's fog drawn over the lit air
				// with its own alpha (0 nearer than its start distance); 0 = option A, the real light only

uniform vec4 volFar;		// lane FARGRID1: the far grid's start (the near grid's end), ln(end / start), slices, 1 = on
uniform vec4 volPack;		// the packed volume's shares: the near grid's width, height, the far grid's
uniform vec4 volPackDims;	// the packed volume's texels: width, height, depth; the near grid's slices

uniform vec4 volAirMed;		// the pass's medium (cell_volair.frag airMed): sigma0, zb, 1 / H, the top (the fog's height frame)
uniform vec4 volAirLayer;	// its near band and ground layer (airLayer): the near share w, zn, 1 / Hn, the ground strength s
uniform vec4 volAirGround;	// (airGround): the ground layer's anchor zg, 1 / its thickness, G2: 1 / the top's falloff height (0: a hard top)
uniform int volAirFlags;	// 1 the surfaces' sun through the air (FOGSUN1; red nosunt clears it), 2 the far tail (red
				// froxonly clears it), 4 the records' colour on the tail (red physcol clears it), 8 the sky
				// through the air (cell_volsky.frag; red nosky clears it), 16 the effects and particles through the
				// air (wwFogFx; red fxfog clears it), 32 the tricubic read (FOGTRICUBIC1, WW_CELL_VOLAIR_TRICUBIC=1),
				// 64 the ground layer on the terrain grid (FOGTERRAIN2; red WW_FOG_TERRAIN_RED=absolute clears it)
uniform sampler2DArray volAirTerr;	// FOGTERRAIN2: the pass's low-pass terrain (cell_airmed.glsl airTerr), world z
uniform vec4 volAirTerrU;	// its uv at view point p: (dot( U.xyz, p ) + U.w, dot( V.xyz, p ) + V.w)
uniform vec4 volAirTerrV;
uniform vec4 volAirTerrFU;	// FOGTERRAIN3: the fine level's (layer 1; uv -1 everywhere: none)
uniform vec4 volAirTerrFV;
uniform float volAirTerrRef;	// the world z of the fog frame's ground (airTerrRef)

// the column between heights lo < hi of one term B(z) = min(1, exp(-(z - z0) invH)): flat under z0, exponential
// above (invH 0: flat everywhere); no cancellation of two large columns. Shared with cell_volair.frag airCol.
float wwAirColOne( float lo, float hi, float z0, float invH )
{
	if ( hi <= lo )
		return 0.0;
	if ( invH <= 0.0 )
		return hi - lo;
	float below = max( min( hi, z0 ) - lo, 0.0 );
	float a = max( lo, z0 );
	if ( hi <= a )
		return below;
	float w = hi - a;
	float u = w * invH;
	float e1 = u < 1e-3 ? 1.0 - u * ( 0.5 - u / 6.0 ) : ( 1.0 - exp( -u ) ) / u;
	return below + exp( -( a - z0 ) * invH ) * w * e1;
}

/* MODULES1 G2 (bungo 11-09, flight items 10b, 10c: "a flat pale-blue slab with a hard, stair-stepped edge", "above the
 * top there is no haze at all"): the air does not end at its top. Above it the top's own density falls off with height,
 * exp(-(z - top) / Ht), Ht = 1 / ground.z (cellvolair.cpp vaTopFall: 120 m), in every site of the law at once: the
 * froxels, the surfaces' tail, the sky's tail, the sun's and the sky light's columns. ground.z 0 (the red
 * WW_CELL_AIR_TOPFALL_RED=1): empty above the top, as before, to the bit.
 * wwAirEnd: the height where the air counts as ended (16 falloff heights, 1e-7 of the top's density); the red: the top */
float wwAirEnd( vec4 med, vec4 ground )
{
	return ground.z > 0.0 ? med.w + 16.0 / ground.z : med.w;
}

// the density per unit sigma0 at height z (cell_volair.frag airRho)
float wwAirRhoLaw( float z, vec4 med, vec4 layer, vec4 ground )
{
	float k = 1.0;
	if ( z > med.w ) {
		if ( ground.z <= 0.0 )
			return 0.0;
		k = exp( -( z - med.w ) * ground.z );	// G2: the top's density, falling off
		z = med.w;
	}
	float r = med.z > 0.0 ? min( 1.0, exp( -( z - med.y ) * med.z ) ) : 1.0;
	if ( layer.x <= 0.0 && layer.w <= 0.0 )
		return k * r;
	if ( layer.x > 0.0 )
		r = ( 1.0 - layer.x ) * r + layer.x * ( layer.z > 0.0 ? min( 1.0, exp( -( z - layer.y ) * layer.z ) ) : 1.0 );
	if ( layer.w > 0.0 )
		r += layer.w * ( ground.y > 0.0 ? min( 1.0, exp( -( z - ground.x ) * ground.y ) ) : 1.0 );
	return k * r;
}

/* the column of the whole density (cellvolair.cpp VaLayer): (1 - w) far band + w near band + s ground layer, above the
 * top its falloff (G2; the red: empty); exactly the far band's column when w = s = 0 */
float wwAirColLaw( float lo, float hi, vec4 med, vec4 layer, vec4 ground )
{
	float up = ground.z > 0.0 && hi > med.w
		? wwAirRhoLaw( med.w, med, layer, ground ) * wwAirColOne( max( lo, med.w ), hi, med.w, ground.z ) : 0.0;
	hi = min( hi, med.w );
	float c = wwAirColOne( lo, hi, med.y, med.z );
	if ( layer.x <= 0.0 && layer.w <= 0.0 )
		return c + up;
	if ( layer.x > 0.0 )
		c = ( 1.0 - layer.x ) * c + layer.x * wwAirColOne( lo, hi, layer.y, layer.z );
	if ( layer.w > 0.0 )
		c += layer.w * wwAirColOne( lo, hi, ground.x, ground.y );
	return c + up;
}

// FOGTERRAIN3: the ground at view point p (cell_airmed.glsl airTerrH): the fine level over the coarse one, faded at its edge
float wwAirTerrH( vec3 p )
{
	vec2 uv = vec2( dot( volAirTerrU.xyz, p ) + volAirTerrU.w, dot( volAirTerrV.xyz, p ) + volAirTerrV.w );
	vec2 uf = vec2( dot( volAirTerrFU.xyz, p ) + volAirTerrFU.w, dot( volAirTerrFV.xyz, p ) + volAirTerrFV.w );
	float hc = texture( volAirTerr, vec3( uv, 0.0 ) ).r;
	float wf = clamp( min( min( uf.x, 1.0 - uf.x ), min( uf.y, 1.0 - uf.y ) ) * 16.0, 0.0, 1.0 );
	return wf > 0.0 ? mix( hc, texture( volAirTerr, vec3( uf, 1.0 ) ).r, wf ) : hc;
}

// FOGTERRAIN2: the ground layer's anchor at view point p, the froxels' (cell_airmed.glsl airGroundAt); flag 64 off: absolute
vec4 wwAirGroundAt( vec3 p )
{
	if ( ( volAirFlags & 64 ) == 0 )
		return volAirGround;
	return vec4( volAirGround.x + wwAirTerrH( p ) - volAirTerrRef, volAirGround.yzw );
}

/* FOGTERRAIN3: the anchor for a ray segment (the far tail, the sky): from view point p0 along unit view direction dirV for
 * len fog units at elevation sine s, inside the grid's window. The column grows as the layer-weighted mean of exp(h / H)
 * (weight exp(-s x / H), cut where it falls under e^-40). Six samples at the sixths of the wider exp(-s x / 3H), weighted
 * back to the layer's own weight: a ground climbing with the ray far out still counts (notes/fogterrain3 rayexp). */
vec4 wwAirGroundRay( vec3 p0, vec3 dirV, float len, float s )
{
	if ( ( volAirFlags & 64 ) == 0 )
		return volAirGround;
	float k = 1.0 / max( fogDistScale, 1e-12 );	// view units per fog unit
	vec2 u0 = vec2( dot( volAirTerrU.xyz, p0 ) + volAirTerrU.w, dot( volAirTerrV.xyz, p0 ) + volAirTerrV.w );
	vec2 du = vec2( dot( volAirTerrU.xyz, dirV ), dot( volAirTerrV.xyz, dirV ) ) * k;
	vec2 ex = vec2( du.x > 0.0 ? ( 1.0 - u0.x ) / du.x : du.x < 0.0 ? -u0.x / du.x : 1e30,
		du.y > 0.0 ? ( 1.0 - u0.y ) / du.y : du.y < 0.0 ? -u0.y / du.y : 1e30 );
	float L = clamp( min( min( ex.x, ex.y ), min( len, 1e7 ) ), 0.0, 1e7 );
	float H = volAirGround.y > 0.0 ? 1.0 / volAirGround.y : 1e9;
	float Lt = min( L, 40.0 * H / max( abs( s ), 1e-9 ) ), xs = s < 0.0 ? L - Lt : 0.0;	// the weight's live span
	float b = s * Lt / H, bp = b / 3.0, E = 1.0 - exp( -bp );
	float a[6], t[6], m = -1e30;
	for ( int q = 0; q < 6; q++ ) {
		float f = ( float( q ) + 0.5 ) / 6.0;
		t[q] = abs( bp ) < 1e-3 ? f : -log( 1.0 - f * E ) / bp;
		a[q] = wwAirTerrH( p0 + dirV * ( ( xs + t[q] * Lt ) * k ) ) / H - ( b - bp ) * t[q];
		m = max( m, a[q] );
	}
	// ln of the mean of exp(-c t) over t in 0..1 for c = b and bp: the importance weight's normalisers
	float lf[2];
	for ( int i = 0; i < 2; i++ ) {
		float c = i == 0 ? b : bp;
		lf[i] = abs( c ) < 1e-3 ? -0.5 * c : c > 0.0 ? log( ( 1.0 - exp( -c ) ) / c ) : -c + log( ( 1.0 - exp( c ) ) / -c );
	}
	float sum = 0.0;
	for ( int q = 0; q < 6; q++ )
		sum += exp( a[q] - m );
	float zg = H * ( m + log( sum / 6.0 ) + lf[1] - lf[0] );
	return vec4( volAirGround.x + zg - volAirTerrRef, volAirGround.yzw );
}

// the column of air between heights lo < hi per unit sigma0, the surfaces' copy of the pass's medium, its ground layer
// anchored by `ground` (wwAirGroundAt)
float wwAirCol( float lo, float hi, vec4 ground )
{
	return wwAirColLaw( lo, hi, volAirMed, volAirLayer, ground );
}

/* lane RAINFOG2 (CLOUD6e's leftover): the rain fog (cell_rainfog.glsl, src/features/weather/rainfogmath.h) in the closed-form tails
 * past the froxel grids (wwVolAirAtUv, wwVolAirAtFar) and in the sky's tail (wwVolAirSky). Its optical depth along a
 * view segment: the segment cut to the heights the fog reaches (above base + the fade nothing), then n sub-segments,
 * each the cloud map's coverage at its middle (faded to the band's scalar past the map) x the profile's EXACT mean over
 * its height range (wwAirRainP, the antiderivative). volAirRain.x 0 (no rain or snow, the red, no cloud map, the air
 * pass's rain fog off): 0, the tails exactly as before. The loop's bound is a uniform: one copy, not unrolled. */
uniform vec4 volAirRain;		// x sigma per game unit near the ground (0 = none), y the base excess, z 1 / falloff km, w 1 / fade km
uniform vec4 volAirRainFar;		// x the Low band's coverage past the map, y the map's edge band (uv), z the sub-segments
uniform mat4 volAirRainXf;		// view -> (u, v, km below the Low base): cell_rainfog.glsl's rfFromView
uniform sampler2D volAirRainMap;	// CLOUD1's cloud map (R: the Low band's coverage)

// the integral of the height profile p(h) from 0 to h (km below the base; above it the fade, flat past the fade's top)
float wwAirRainP( float h )
{
	if ( h >= 0.0 )
		return h + volAirRain.y * ( 1.0 - exp( -h * volAirRain.z ) ) / volAirRain.z;
	float hc = max( h, -1.0 / volAirRain.w );
	return ( 1.0 + volAirRain.y ) * ( hc + 0.5 * hc * hc * volAirRain.w );
}

// the rain fog's optical depth between view points pA and pB
float wwAirRainTau( vec3 pA, vec3 pB )
{
	if ( volAirRain.x <= 0.0 )
		return 0.0;
	vec3 qA = ( volAirRainXf * vec4( pA, 1.0 ) ).xyz;
	vec3 dq = ( volAirRainXf * vec4( pB, 1.0 ) ).xyz - qA;
	float hTop = -1.0 / volAirRain.w;	// the fade's top: no fog above it
	float t0 = 0.0, t1 = 1.0;
	if ( abs( dq.z ) > 1e-9 ) {
		float tc = ( hTop - qA.z ) / dq.z;
		if ( dq.z > 0.0 )
			t0 = max( t0, tc );
		else
			t1 = min( t1, tc );
	} else if ( qA.z <= hTop ) {
		return 0.0;
	}
	if ( t1 <= t0 )
		return 0.0;
	int n = int( volAirRainFar.z );
	float w = ( t1 - t0 ) / float( n );
	float dh = dq.z * w;	// every sub-segment's height range (km)
	bool level = abs( dh ) <= 1e-4;
	float Pa = level ? 0.0 : wwAirRainP( qA.z + dq.z * t0 );	// the profile's integral at the sub-segment's start
	float sum = 0.0;
	for ( int k = 0; k < n; k++ ) {
		vec3 qm = qA + dq * ( t0 + ( float( k ) + 0.5 ) * w );
		vec2 e = abs( qm.xy - 0.5 ) * 2.0;
		float c = mix( textureLod( volAirRainMap, qm.xy, 0.0 ).r, volAirRainFar.x, smoothstep( 1.0 - volAirRainFar.y, 1.0, max( e.x, e.y ) ) );
		float p;
		if ( level ) {
			p = qm.z >= 0.0 ? 1.0 + volAirRain.y * exp( -qm.z * volAirRain.z ) : ( 1.0 + volAirRain.y ) * max( 1.0 + qm.z * volAirRain.w, 0.0 );
		} else {
			float Pb = wwAirRainP( qA.z + dq.z * ( t0 + float( k + 1 ) * w ) );
			p = ( Pb - Pa ) / dh;
			Pa = Pb;
		}
		sum += c * p;
	}
	return volAirRain.x * length( pB - pA ) * fogDistScale * w * sum;
}

// the records' haze colour at distance d, height z, view direction dirView (cell_volair.frag airEnvColour), unit luminance
vec3 wwAirRecordColour( float d, float z, vec3 dirView, out bool ok );
#ifdef WW_SUNSHADOW
float wwAirTailShadow( vec3 dirView, float t0, float t1, float tau );	// ww_sunshadow.glsl (lane SHADOWPAR1)
#endif

/* lane FARGRID1: with the far grid on, one volume holds both grids: the near grid in layers 0..n-1 over its share of
 * the width and height, the far grid after it over its own share. (L, T) of one grid at slice coordinate f (0 at its
 * near edge), the bilinear kept inside the grid's share (its own clamp to edge). */
vec4 wwGridAt( vec2 uv, vec2 share, float base, float n, float f )
{
	vec2 lo = 0.5 / volPackDims.xy;
	vec2 st = clamp( uv * share, lo, share - lo );
	vec4 v;
	if ( f < 1.0 ) {
		v = texture( volTex, vec3( st, ( base + 0.5 ) / volPackDims.z ) );
		v = vec4( v.rgb * max( f, 0.0 ), 1.0 - ( 1.0 - v.a ) * max( f, 0.0 ) );
	} else {
		v = texture( volTex, vec3( st, ( base + min( f, n ) - 0.5 ) / volPackDims.z ) );
	}
	return vec4( max( v.rgb, vec3( 0.0 ) ), clamp( v.a, 0.0, 1.0 ) );
}

/* the closed-form tail's J: the light per unit (1 - T) of the grid's last slice, `last` its (L, T) at the end, `prev` one
 * slice nearer. MODULES1 G2: in thin air (the top's falloff) the last slice's dT falls under the volume's precision and
 * the tail went black; there the whole column's light per unit (1 - T) stands in (the red, ground.z 0: none, as before) */
/* MODULES1 R7-4 (bungo 11-10, level strips in the fog from above): the volume keeps T in 16F, steps of 4.9e-4 under 1.0,
 * and one slice's dT is 2 to 30 steps wide: dL / dT of the last slice alone stepped by 10 to 50 % from one row of rays
 * to the next (notes/modules1/r7/t/strips_model.py). The source now comes from a WINDOW of the grid's end, `prev` read
 * where the column's depth is 1/16 less (wwAirTailF), `last` the whole column:
 *   the window's dL / dT; where it holds under 64 steps of T (air thinning toward the grid's end), the whole column's;
 *   a thin column (depth <= 1/8: the window is the grid's far half) returns the window's dL alone and wwAirThinJ
 *   divides it by the LAW's depth of that half, no 16F step in it.
 * fogRed 8 (red WW_CELL_AIR_TAILJ_RED=1): the last slice's dL / dT, as before */
vec3 wwAirTailJ( vec4 last, vec4 prev )
{
	float dT = prev.a - last.a;
	if ( ( fogRed & 8 ) != 0 ) {
		if ( dT > 1e-7 )
			return max( ( last.rgb - prev.rgb ) / dT, vec3( 0.0 ) );
		return volAirGround.z > 0.0 && last.a < 0.9999 ? max( last.rgb, vec3( 0.0 ) ) / ( 1.0 - last.a ) : vec3( 0.0 );
	}
	vec3 dL = max( last.rgb - prev.rgb, vec3( 0.0 ) );
	if ( -log( max( last.a, 1e-6 ) ) <= 0.125 )
		return dL / max( prev.a, 1e-6 );
	float m = 1.0 - last.a;
	vec3 Jc = m > 1e-7 ? max( last.rgb, vec3( 0.0 ) ) / m : vec3( 0.0 );
	return mix( Jc, dT > 1e-7 ? dL / dT : vec3( 0.0 ), clamp( dT * 64.0 / max( last.a, 1e-6 ) - 1.0, 0.0, 1.0 ) );
}

// R7-4: the share of the grid's reach the window covers, for a column of transmittance T: 1/16 of its depth, at most half
float wwAirTailW( float T )
{
	return min( 0.0625 / max( -log( max( T, 1e-6 ) ), 1e-9 ), 0.5 );
}

// R7-4: the slice coordinate of the window's start in a grid of n slices over ln(far / near) = lnSpan; the red: n - 1
float wwAirTailF( float T, float lnSpan, float n )
{
	if ( ( fogRed & 8 ) != 0 )
		return n - 1.0;
	return clamp( n * ( 1.0 + log( 1.0 - wwAirTailW( T ) ) / lnSpan ), 1.0, n - 1.0 );
}

/* MODULES1 10c (a camera far above the fog): a column whose whole grid span lies in the top's falloff is too thin for
 * the volume's T (16F steps by 4.9e-4 under 1.0: it read 1.0 or 0.9995 for an optical depth of 2e-6), so wwAirTailJ had
 * no source, or one 2000 times too small, and the tail dimmed the land far below with no light of its own. There the
 * grid's light over the optical depth of the same span by the medium's law (the one the froxels march, wwAirCol) is
 * the source: v = the grid's (L, T) at its end `far`, z0 the ray's height there. Under an optical depth of 0.02 only
 * (40 steps of T: above it the volume's own T is exact enough). volAirFlags 32768; red WW_CELL_AIR_THINSRC_RED=1 */
vec3 wwAirThinJ( vec3 J, vec4 v, float z0, float far )
{
	float zc = fogView.w;
	bool thin = ( fogRed & 8 ) == 0 && -log( max( v.a, 1e-6 ) ) <= 0.125;
	if ( !thin && ( ( volAirFlags & 32768 ) == 0 || volAirGround.z <= 0.0 || min( zc, z0 ) < volAirMed.w ) )
		return J;
	// the law's depth of the whole reach (a level ray: the density at the camera's height), one call for both rules
	float z1 = abs( z0 - zc ) < 0.5 ? zc + 1.0 : z0;
	float k = volAirMed.x * far / abs( z1 - zc );
	float od = k * wwAirCol( min( zc, z1 ), max( zc, z1 ), volAirGround );
	if ( thin ) {
		/* MODULES1 R7-4: a thin column (the same test as wwAirTailJ's; J = its far half's dL over the T at the half's
		 * start). od, b = the law's depth of the whole reach and of its far half; the law is scaled so its whole-reach
		 * 1 - T lies within one 16F step of the stored one (zones, rain: the stored depth rules where they differ by
		 * more) */
		float zh = 0.5 * ( zc + z1 );
		float b = k * wwAirCol( min( zh, z1 ), max( zh, z1 ), volAirGround );
		float xa = 1.0 - exp( -od ), m = 1.0 - v.a;
		b *= clamp( xa, m - 4.8828125e-4, m + 4.8828125e-4 ) / max( xa, 1e-9 );
		return b > 1e-7 ? J / ( b * ( 1.0 - 0.5 * b ) ) : vec3( 0.0 );
	}
	return od > 1e-7 && od < 0.02 ? max( v.rgb, vec3( 0.0 ) ) / ( od * ( 1.0 - 0.5 * od ) ) : J;
}

// with the far grid: `b` its (L, T) at the end, `a` the near grid's, `whole` the two together. R7-4: the window runs over the
// two grids as one column (its start in either); the red: the far grid's last slice
vec3 wwGridTailJ( vec2 uv, vec4 a, vec4 b, vec4 whole )
{
	float nN = volPackDims.w;
	bool red = ( fogRed & 8 ) != 0;
	float db = volFar.x * exp( volFar.y ) * ( 1.0 - wwAirTailW( whole.a ) );
	bool inFar = red || db > volFar.x;
	float f = red ? volFar.z - 1.0 : inFar ? min( log( db / volFar.x ) / volFar.y * volFar.z, volFar.z - 1.0 )
		: log( db / volSlices.x ) / volSlices.y * nN;
	vec4 p = wwGridAt( uv, inFar ? volPack.zw : volPack.xy, inFar ? nN : 0.0, inFar ? volFar.z : nN, f );
	if ( red )
		return wwAirTailJ( b, p );
	return wwAirTailJ( whole, inFar ? vec4( a.rgb + a.a * p.rgb, a.a * p.a ) : p );
}

// near grid -> far grid -> closed-form tail: (L, T) at distance d (game units) along posView
vec4 wwVolAirAtFar( float d, vec2 uv, vec3 posView )
{
	float nN = volPackDims.w;
	if ( d <= volSlices.x )
		return vec4( 0.0, 0.0, 0.0, 1.0 );
	vec4 v = wwGridAt( uv, volPack.xy, 0.0, nN, log( d / volSlices.x ) / volSlices.y * nN );
	if ( d <= volFar.x )
		return v;
	float fF = log( d / volFar.x ) / volFar.y * volFar.z;
	vec4 w = wwGridAt( uv, volPack.zw, nN, volFar.z, fF );
	vec4 o = vec4( v.rgb + v.a * w.rgb, v.a * w.a );
	if ( fF <= volFar.z || ( volAirFlags & 2 ) == 0 || volAirMed.x <= 0.0 )
		return o;
	// the closed-form tail past the far grid (wwVolAirAt's, from the far grid's last slice)
	float far = volFar.x * exp( volFar.y );
	vec3 J = wwGridTailJ( uv, v, w, o );
	float z1 = dot( fogView.xyz, posView ) + fogView.w;
	float zc = fogView.w;
	float z0 = zc + ( z1 - zc ) * ( far / d );
	J = wwAirThinJ( J, o, z0, far );	// MODULES1 10c
	float len = d - far;
	float dz = z1 - z0;
	vec4 gT = wwAirGroundRay( posView * ( far / d ), posView / max( length( posView ), 1e-12 ), len, dz / max( len, 1e-6 ) );	// FOGTERRAIN3: along the tail
	float tau = abs( dz ) < 0.5 ? volAirMed.x * len * wwAirCol( z0, z0 + 1.0, gT ) : volAirMed.x * len * wwAirCol( min( z0, z1 ), max( z0, z1 ), gT ) / abs( dz );
	float Tt = exp( -( tau + wwAirRainTau( posView * ( far / d ), posView ) ) );	// lane RAINFOG2: + the rain fog
	if ( ( volAirFlags & 4 ) != 0 ) {
		vec3 dirV = posView / max( length( posView ), 1e-12 );
		bool ok0, ok1;
		vec3 c0 = wwAirRecordColour( far, z0, dirV, ok0 );
		vec3 c1 = wwAirRecordColour( 0.5 * ( far + d ), 0.5 * ( z0 + z1 ), dirV, ok1 );
		if ( ok0 && ok1 )
			J *= c1 / max( c0, vec3( 1e-6 ) );
	}
	return vec4( o.rgb + o.a * J * ( 1.0 - Tt ), o.a * Tt );
}

/* (L, T) of the air between the eye and posView. Past the froxels' far edge (bungo 10-14, the far tail: far
 * mountains and the far LOD) the same medium continues in closed form: T x= exp(-sigma0 x the column along the
 * segment / its slope), L += T_edge x J x (1 - T_tail), J the light the last slice's air scattered (its L and T steps,
 * so the tail starts continuous with the froxels), carried to the tail's middle in the records' colour. A ray that
 * climbs meets thinner air: a peak keeps more contrast than its foot. */
/* FOGTRICUBIC1 (bungo 10-21): a cubic B-spline read of the froxels, from linear fetches placed between texels (the
 * fractional offsets carry the weights). Smooths the grid's blocks; xyOnly keeps the slice exact (the far tail's and the
 * sky's step J needs the two slices themselves). Flag 32 off: the plain trilinear read. */
vec4 wwVolTex( vec3 p, bool xyOnly )
{
	if ( ( volAirFlags & 32 ) == 0 )
		return texture( volTex, p );
	vec3 n = vec3( textureSize( volTex, 0 ) );
	vec3 c = p * n - 0.5;
	vec3 i = floor( c );
	vec3 f = c - i;
	vec3 w0 = ( ( -f + 3.0 ) * f - 3.0 ) * f + 1.0;	// x 6: (1 - f)^3
	vec3 w1 = ( 3.0 * f - 6.0 ) * f * f + 4.0;
	vec3 w2 = ( ( -3.0 * f + 3.0 ) * f + 3.0 ) * f + 1.0;
	vec3 w3 = f * f * f;
	vec3 g0 = w0 + w1, g1 = w2 + w3;
	vec3 h0 = ( i - 0.5 + w1 / g0 ) / n, h1 = ( i + 1.5 + w3 / g1 ) / n;
	g0 /= 6.0;
	g1 /= 6.0;
	if ( xyOnly ) {
		return g0.y * ( g0.x * texture( volTex, vec3( h0.x, h0.y, p.z ) ) + g1.x * texture( volTex, vec3( h1.x, h0.y, p.z ) ) )
			+ g1.y * ( g0.x * texture( volTex, vec3( h0.x, h1.y, p.z ) ) + g1.x * texture( volTex, vec3( h1.x, h1.y, p.z ) ) );
	}
	vec4 a = g0.y * ( g0.x * texture( volTex, vec3( h0.x, h0.y, h0.z ) ) + g1.x * texture( volTex, vec3( h1.x, h0.y, h0.z ) ) )
		+ g1.y * ( g0.x * texture( volTex, vec3( h0.x, h1.y, h0.z ) ) + g1.x * texture( volTex, vec3( h1.x, h1.y, h0.z ) ) );
	vec4 b = g0.y * ( g0.x * texture( volTex, vec3( h0.x, h0.y, h1.z ) ) + g1.x * texture( volTex, vec3( h1.x, h0.y, h1.z ) ) )
		+ g1.y * ( g0.x * texture( volTex, vec3( h0.x, h1.y, h1.z ) ) + g1.x * texture( volTex, vec3( h1.x, h1.y, h1.z ) ) );
	return g0.z * a + g1.z * b;
}

// R7-4: the near grid's tail source (no far grid): `v` its (L, T) at the end, the window's start read at wwAirTailF
vec3 wwAirTailNearJ( vec2 uv, vec4 v )
{
	return wwAirTailJ( v, wwVolTex( vec3( uv, ( wwAirTailF( v.a, volSlices.y, volSlices.z ) - 0.5 ) / volSlices.z ), true ) );
}

/* MODULES1 10c: the lit-decal programs (WW_FX_DECALLIT) take the one-tail shape without WW_CELL_FOGLEAK too: the
 * old shape inlined a second whole tail (wwVolAirAtFar) and fo4_effectdecalcsm passed the instruction limit with the
 * leak red set. Same law there: the far grid's tail takes no sun shadow (WW_AIR_FARTAILSH false). Offline red
 * WW_AIR_TWOTAILS = the old shape (it must refuse) */
#ifdef WW_CELL_FOGLEAK
#define WW_AIR_ONETAIL
#define WW_AIR_FARTAILSH ( ( volAirFlags & 256 ) != 0 )
#elif defined( WW_FX_DECALLIT ) && !defined( WW_AIR_TWOTAILS )
#define WW_AIR_ONETAIL
#define WW_AIR_FARTAILSH false
#endif
#ifdef WW_AIR_ONETAIL
/* lane FOGLEAK1 (fix 4, the loader's WW_CELL_FOGLEAK only): the far grid's closed-form tail took no sun shadow
 * (wwVolAirAtFar), the near grid's did. Both grids' heads below end where their tail starts, and ONE tail takes the
 * sun's shadow for either (one wwAirTailShadow call site: a second inlined copy would tip the programs at the limit).
 * volAirFlags 256 off (red WW_CELL_AIR_RED=nofartailshadow): the far grid's tail unshadowed, as before */
bool wwVolAirFarHead( float d, vec2 uv, vec3 posView, out vec4 o, out vec3 J, out float far )
{
	J = vec3( 0.0 );
	far = 0.0;
	float nN = volPackDims.w;
	if ( d <= volSlices.x ) {
		o = vec4( 0.0, 0.0, 0.0, 1.0 );
		return false;
	}
	vec4 v = wwGridAt( uv, volPack.xy, 0.0, nN, log( d / volSlices.x ) / volSlices.y * nN );
	if ( d <= volFar.x ) {
		o = v;
		return false;
	}
	float fF = log( d / volFar.x ) / volFar.y * volFar.z;
	vec4 w = wwGridAt( uv, volPack.zw, nN, volFar.z, fF );
	o = vec4( v.rgb + v.a * w.rgb, v.a * w.a );
	if ( fF <= volFar.z || ( volAirFlags & 2 ) == 0 || volAirMed.x <= 0.0 )
		return false;
	far = volFar.x * exp( volFar.y );
	J = wwGridTailJ( uv, v, w, o );
	return true;
}

bool wwVolAirNearHead( float d, vec2 uv, out vec4 v, out vec3 J, out float far )
{
	J = vec3( 0.0 );
	far = 0.0;
	if ( d <= volSlices.x ) {
		v = vec4( 0.0, 0.0, 0.0, 1.0 );
		return false;
	}
	float f = log( d / volSlices.x ) / volSlices.y * volSlices.z;
	if ( f < 1.0 ) {
		v = wwVolTex( vec3( uv, 0.5 / volSlices.z ), true );
		v = vec4( v.rgb * f, 1.0 - ( 1.0 - v.a ) * f );
	} else {
		v = wwVolTex( vec3( uv, ( min( f, volSlices.z ) - 0.5 ) / volSlices.z ), f > volSlices.z );
	}
	v = vec4( max( v.rgb, vec3( 0.0 ) ), clamp( v.a, 0.0, 1.0 ) );
	if ( f <= volSlices.z || ( volAirFlags & 2 ) == 0 || volAirMed.x <= 0.0 )
		return false;
	far = volSlices.x * exp( volSlices.y );
	J = wwAirTailNearJ( uv, v );
	return true;
}

vec4 wwVolAirAtUv( vec3 posView, vec2 uv )	// lane VOLPART1: uv = the column (a particle's probe: its centre's)
{
	float d = length( posView ) * fogDistScale;
	if ( volFarClamp != 0 )
		d = min( d, texture( volTile, uv ).r );
	bool farGrid = volFar.w > 0.5;
	vec4 v;
	vec3 J;
	float far;
	bool tail = farGrid ? wwVolAirFarHead( d, uv, posView, v, J, far ) : wwVolAirNearHead( d, uv, v, J, far );
	if ( !tail )
		return v;
	float z1 = dot( fogView.xyz, posView ) + fogView.w;
	float zc = fogView.w;
	float z0 = zc + ( z1 - zc ) * ( far / d );
	J = wwAirThinJ( J, v, z0, far );	// MODULES1 10c
	float len = d - far;
	float dz = z1 - z0;
	vec4 gT = wwAirGroundRay( posView * ( far / d ), posView / max( length( posView ), 1e-12 ), len, dz / max( len, 1e-6 ) );	// FOGTERRAIN3: along the tail
	float tau = abs( dz ) < 0.5 ? volAirMed.x * len * wwAirCol( z0, z0 + 1.0, gT ) : volAirMed.x * len * wwAirCol( min( z0, z1 ), max( z0, z1 ), gT ) / abs( dz );
	float Tt = exp( -( tau + wwAirRainTau( posView * ( far / d ), posView ) ) );	// lane RAINFOG2: + the rain fog
#ifdef WW_SUNSHADOW
	if ( !farGrid || WW_AIR_FARTAILSH )
		J *= wwAirTailShadow( posView / max( length( posView ), 1e-12 ), far, d, tau );
#endif
	if ( ( volAirFlags & 4 ) != 0 ) {
		vec3 dirV = posView / max( length( posView ), 1e-12 );
		bool ok0, ok1;
		vec3 c0 = wwAirRecordColour( far, z0, dirV, ok0 );
		vec3 c1 = wwAirRecordColour( 0.5 * ( far + d ), 0.5 * ( z0 + z1 ), dirV, ok1 );
		if ( ok0 && ok1 )
			J *= c1 / max( c0, vec3( 1e-6 ) );
	}
	return vec4( v.rgb + v.a * J * ( 1.0 - Tt ), v.a * Tt );
}
#else
vec4 wwVolAirAtUv( vec3 posView, vec2 uv )	// lane VOLPART1: uv = the column (a particle's probe: its centre's)
{
	float d = length( posView ) * fogDistScale;
	/* 10-16: no farther than the column's farthest opaque surface (the obscurance pass's depth). At a thin sliver's
	 * edge the interpolated position can lie far past the depth the same fragment writes (a trim at 596 shaded as 763):
	 * that read the froxels the depth skip had left empty, and an air no surface there sits behind. */
	if ( volFarClamp != 0 )
		d = min( d, texture( volTile, uv ).r );
	if ( volFar.w > 0.5 )
		return wwVolAirAtFar( d, uv, posView );
	if ( d <= volSlices.x )
		return vec4( 0.0, 0.0, 0.0, 1.0 );
	float f = log( d / volSlices.x ) / volSlices.y * volSlices.z;
	vec4 v;
	if ( f < 1.0 ) {
		v = wwVolTex( vec3( uv, 0.5 / volSlices.z ), true );
		v = vec4( v.rgb * f, 1.0 - ( 1.0 - v.a ) * f );
	} else {
		v = wwVolTex( vec3( uv, ( min( f, volSlices.z ) - 0.5 ) / volSlices.z ), f > volSlices.z );
	}
	v = vec4( max( v.rgb, vec3( 0.0 ) ), clamp( v.a, 0.0, 1.0 ) );
	if ( f <= volSlices.z || ( volAirFlags & 2 ) == 0 || volAirMed.x <= 0.0 )
		return v;
	// the far tail
	float far = volSlices.x * exp( volSlices.y );
	vec3 J = wwAirTailNearJ( uv, v );
	float z1 = dot( fogView.xyz, posView ) + fogView.w;
	float zc = fogView.w;
	float z0 = zc + ( z1 - zc ) * ( far / d );
	J = wwAirThinJ( J, v, z0, far );	// MODULES1 10c
	float len = d - far;
	float dz = z1 - z0;
	vec4 gT = wwAirGroundRay( posView * ( far / d ), posView / max( length( posView ), 1e-12 ), len, dz / max( len, 1e-6 ) );	// FOGTERRAIN3: along the tail
	float tau = abs( dz ) < 0.5 ? volAirMed.x * len * wwAirCol( z0, z0 + 1.0, gT ) : volAirMed.x * len * wwAirCol( min( z0, z1 ), max( z0, z1 ), gT ) / abs( dz );
	float Tt = exp( -( tau + wwAirRainTau( posView * ( far / d ), posView ) ) );	// lane RAINFOG2: + the rain fog
#ifdef WW_SUNSHADOW
	J *= wwAirTailShadow( posView / max( length( posView ), 1e-12 ), far, d, tau );
#endif
	if ( ( volAirFlags & 4 ) != 0 ) {
		vec3 dirV = posView / max( length( posView ), 1e-12 );
		bool ok0, ok1;
		vec3 c0 = wwAirRecordColour( far, z0, dirV, ok0 );
		vec3 c1 = wwAirRecordColour( 0.5 * ( far + d ), 0.5 * ( z0 + z1 ), dirV, ok1 );
		if ( ok0 && ok1 )
			J *= c1 / max( c0, vec3( 1e-6 ) );
	}
	return vec4( v.rgb + v.a * J * ( 1.0 - Tt ), v.a * Tt );
}
#endif

/* (L, T) of the air between the eye and posView, at this fragment's column. Part 18 (volAirFlags 512): RDR2's single
 * dithered tap (Bauer 2019 slides 48, 57; notes/modules1/research/fog_temporal.md): the read moves up to half a froxel in
 * x, y and slice by a per-pixel gradient noise that turns each frame (bits 10-13), and TAA averages the blocks away */
vec4 wwVolAirAt( vec3 posView )
{
	vec2 uv = ( gl_FragCoord.xy - volRect.xy ) * volRect.zw;
	if ( ( volAirFlags & 512 ) != 0 ) {
		vec2 q = gl_FragCoord.xy + 5.588238 * float( ( volAirFlags >> 10 ) & 15 );
		vec3 n = fract( 52.9829189 * fract( vec3( dot( q, vec2( 0.06711056, 0.00583715 ) ), dot( q + vec2( 47.0, 17.0 ), vec2( 0.06711056, 0.00583715 ) ),
			dot( q + vec2( 13.0, 71.0 ), vec2( 0.06711056, 0.00583715 ) ) ) ) ) - 0.5;
		uv += n.xy / ( volPackDims.xy * volPack.xy );
		posView *= exp( n.z * volSlices.y / volPackDims.w );
	}
	return wwVolAirAtUv( posView, uv );
}

/* (L, T) of the whole air along a sky ray (bungo 10-14: the far tower stood out against an unfogged sky). The sky is a
 * surface at infinite distance: the froxels to their far edge, then the far tail of wwVolAirAt run to the end of the
 * air -- climbing, to the air's top (the column over the sine of the elevation: finite); level or down, the air never
 * ends (T 0, the fog's own colour). It is the limit of wwVolAirAt as the distance grows, so far geometry and the sky
 * behind it take the same air. The tail's colour is read halfway to where the ray leaves the air. */
vec4 wwVolAirSky( vec3 dirView )
{
	vec2 uv = ( gl_FragCoord.xy - volRect.xy ) * volRect.zw;
	float n = volSlices.z;
	vec4 v;
	float far;
	vec3 J;
	if ( volFar.w > 0.5 ) {
		// lane FARGRID1: the near grid's end, then the far grid's, the tail from the far grid's last slice
		vec4 a = wwGridAt( uv, volPack.xy, 0.0, volPackDims.w, volPackDims.w );
		vec4 b = wwGridAt( uv, volPack.zw, volPackDims.w, volFar.z, volFar.z );
		v = vec4( a.rgb + a.a * b.rgb, a.a * b.a );
		if ( ( volAirFlags & 2 ) == 0 || volAirMed.x <= 0.0 )
			return v;
		far = volFar.x * exp( volFar.y );
		J = wwGridTailJ( uv, a, b, v );
	} else {
	v = wwVolTex( vec3( uv, ( n - 0.5 ) / n ), true );
	v = vec4( max( v.rgb, vec3( 0.0 ) ), clamp( v.a, 0.0, 1.0 ) );
	if ( ( volAirFlags & 2 ) == 0 || volAirMed.x <= 0.0 )
		return v;
	far = volSlices.x * exp( volSlices.y );
	J = wwAirTailNearJ( uv, v );
	}
	float s = dot( fogView.xyz, dirView ) / max( fogDistScale, 1e-12 );	// the sine of the elevation
	/* MODULES1 R7 item 6 (bungo b61: "white horizontal line", 1 px at eye level from high up): a level ray (|sine| <=
	 * 1e-4, a sixth of a pixel row) took the branch below that knows only "above the top = no air" (T 1), while the
	 * top's falloff (G2) holds air there: with the eye above the top that one row let the sky through the grids' T
	 * (0.44 in the flight) between two opaque rows. The level ray ends like its neighbours: the sine held at +-1e-4.
	 * fogRed 16 = the red WW_CELL_AIR_LEVELRAY_RED=1. */
	if ( ( fogRed & 16 ) == 0 && abs( s ) <= 1e-4 )
		s = s >= 0.0 ? 1.0001e-4 : -1.0001e-4;
	float z0 = fogView.w + s * far;
	J = wwAirThinJ( J, v, z0, far );	// MODULES1 10c
	float top = volAirMed.w;
	float Tt, dEnd;
	/* MODULES1 M5 (bungo 11-10, horizon_band: a light-blue stripe over the far hills from high up): a downward ray ends
	 * where it meets the ground (the ground layer's anchor along it), as the drawn land under it does in the surfaces'
	 * tail: the column from the ground to the ray's height at the grid's end over the sine. It was T 0 (the air never
	 * ends) under the top and T 1 above it: rows over the hills switched from clear to the fog's colour. Bit 65536;
	 * red WW_CELL_AIR_GROUNDEND_RED=1. */
	vec4 gD = volAirGround;
	bool groundEnd = ( volAirFlags & 65536 ) != 0 && s < -1e-4;
	if ( groundEnd ) {
		gD = wwAirGroundRay( dirView * ( far / max( fogDistScale, 1e-12 ) ), dirView, 1e7, s );
		groundEnd = z0 > gD.x;
	}
	if ( groundEnd ) {
		dEnd = far + min( ( z0 - gD.x ) / -s, 1e7 );
		Tt = exp( -volAirMed.x * wwAirCol( gD.x, z0, gD ) / -s
			- wwAirRainTau( dirView * ( far / max( fogDistScale, 1e-12 ) ), dirView * ( dEnd / max( fogDistScale, 1e-12 ) ) ) );
	} else if ( s > 1e-4 ) {
		float end = wwAirEnd( volAirMed, volAirGround );	// G2: the climb runs through the top's falloff
		dEnd = far + min( max( end - z0, 0.0 ) / s, 1e7 );
		// FOGTERRAIN3: the ground along the climb, inside the grid's window (wwAirGroundRay)
		Tt = exp( -volAirMed.x * wwAirCol( z0, end, wwAirGroundRay( dirView * ( far / max( fogDistScale, 1e-12 ) ), dirView, dEnd - far, s ) ) / s
			- wwAirRainTau( dirView * ( far / max( fogDistScale, 1e-12 ) ), dirView * ( ( far + 1e7 ) / max( fogDistScale, 1e-12 ) ) ) );	// lane RAINFOG2: + the rain fog, to where the ray leaves it
	} else {
		/* MODULES1 item 18: a ray still above the air's top where the grid ends (a camera above the air looking down)
		 * has met no air yet: its tail is not opaque black; the sky behind it (the planet's ground through the haze
		 * layer) shows. Bit 16384; red WW_CELL_AIR_ABOVE_RED=1: every downward tail opaque, as before */
		Tt = ( s >= 0.0 && z0 >= top ) || ( ( volAirFlags & 16384 ) != 0 && z0 >= top ) ? 1.0 : 0.0;
		dEnd = far + 1e7;
	}
	if ( ( volAirFlags & 4 ) != 0 ) {
		bool ok0, ok1;
		float dm = 0.5 * ( far + dEnd );
		vec3 c0 = wwAirRecordColour( far, z0, dirView, ok0 );
		vec3 c1 = wwAirRecordColour( dm, z0 + s * ( dm - far ), dirView, ok1 );
		if ( ok0 && ok1 )
			J *= c1 / max( c0, vec3( 1e-6 ) );
	}
	return vec4( v.rgb + v.a * J * ( 1.0 - Tt ), v.a * Tt );
}

/* lane WEATHERBLEND4: the drifting air (cell_volair.frag airDrift) where the surfaces and the puffs read it: the density
 * factor at view point p, around 1; amp 0 (drift off, red WW_CELL_FOGDRIFT_RED=nosun) = 1 */
uniform vec4 volAirDrift;		// the air pass's volDrift: x amp, y the fade's start, z 1 / its length, w the noise's mean
uniform vec4 volAirDriftUv[6];	// per octave three rows: the noise's uvw at view point p = dot( row, vec4( p, 1 ) )
uniform sampler3D volAirNoise;
float wwAirDriftAt( vec3 p )
{
	float f = 1.0 - clamp( ( length( p ) * fogDistScale - volAirDrift.y ) * volAirDrift.z, 0.0, 1.0 );
	if ( volAirDrift.x <= 0.0 || f <= 0.0 )
		return 1.0;
	vec4 q = vec4( p, 1.0 );
	float a = texture( volAirNoise, vec3( dot( volAirDriftUv[0], q ), dot( volAirDriftUv[1], q ), dot( volAirDriftUv[2], q ) ) ).r;
	float b = texture( volAirNoise, vec3( dot( volAirDriftUv[3], q ), dot( volAirDriftUv[4], q ), dot( volAirDriftUv[5], q ) )
		+ vec3( 0.37, 0.61, 0.13 ) ).r;
	return 1.0 + volAirDrift.x * f * ( a - volAirDrift.w + 0.5 * f * ( b - volAirDrift.w ) ) * ( 1.0 / 1.5 );
}

uniform int volAirDriftRed;		// lane WEATHERBLEND5: 1 red onesun (the sun's column at one point), 2 red puffonly
// the factor on the sun column's optical depth from p (height lo, the sun's rise mu, the air's top): the drift's mean at
// 1/6, 1/2 and 5/6 of the column, at most the noise's tile (4096 units) along the sun; red onesun: half way only
float wwAirDriftSunK( vec3 p, float lo, float mu, float top )
{
	if ( volAirDrift.x <= 0.0 )
		return 1.0;
	vec3 s = fogSun.xyz * ( min( max( top - lo, 0.0 ) / max( mu, 0.02 ), 4096.0 ) / max( fogDistScale, 1e-12 ) );
	// a loop on a uniform count: one copy of the noise reads (three unrolled copies broke the decal program's limit)
	int n = ( volAirDriftRed & 1 ) != 0 ? 1 : 3;
	float f0 = n == 1 ? 0.5 : 1.0 / 6.0, k = 0.0;
	for ( int i = 0; i < n; i++ )
		k += wwAirDriftAt( p + s * ( f0 + float( i ) / 3.0 ) );
	return k / float( n );
}

/* FOGSUN1 (bungo 10-14): the sun reaching a surface through the air -- the air's transmittance from the surface's
 * height to its top along the sun, closed form (the air's own sun term, cell_volair.frag airTup): in heavy fog the
 * direct sun is dimmed by the medium; the sky's share the air scatters is its in-scattering. 1 where the air is off.
 * Red nosunt: the sun not dimmed. The grazing guard mu >= 0.02 is the air's own (a flat medium has no finite column at
 * the horizon). */
vec2 wwVolAirSunTD( vec3 posView )
{
	if ( !fogOn || !volOn || volSlices.w < 0.5 || fogSun.w <= 0.0 || volAirMed.x <= 0.0 || ( volAirFlags & 1 ) == 0 )
		return vec2( 1.0, 0.0 );
	float lo = dot( fogView.xyz, posView ) + fogView.w;
	float end = wwAirEnd( volAirMed, volAirGround );	// G2
	if ( lo >= end )
		return vec2( 1.0, 0.0 );
	float mu = max( dot( fogView.xyz, fogSun.xyz ) / max( length( fogView.xyz ), 1e-12 ), 0.02 );
	vec4 g = wwAirGroundAt( posView );
	/* lane FIX1101A (atmo, the fog-sun fault): flag 128 (shipped) = only the derived ground layer dims the sun. The far and
	 * near bands ARE the records' fog, and the weather's Sunlight colour is already the sun that weather gives the ground;
	 * their column to the scene's top (its tallest shape) took Foggy noon at c3 to Ts 0.026 (no shadows, no shafts).
	 * Red WW_CELL_AIR_RED=suncolumn clears the flag: the whole column, as before. cell_airmed.glsl airTupSun is the twin. */
	float c = ( volAirFlags & 128 ) != 0
		? ( volAirLayer.w > 0.0 ? volAirLayer.w * wwAirColOne( lo, volAirMed.w, g.x, g.y ) : 0.0 )
		: wwAirCol( lo, end, g );
	float tau = volAirMed.x * c * wwAirDriftSunK( posView, lo, mu, volAirMed.w ), Td = exp( -tau / mu );
	/* MODULES1 G3 (bungo 11-09 items 6, 10e: the lit fog far brighter than the sunlit land beside it): what the column
	 * takes out of the direct beam is mostly scattered forward and reaches the surface as soft light. The conservative
	 * delta-Eddington total transmission (Meador & Weaver 1980 eq. 24) with a low-sun divisor fitted to a Monte Carlo
	 * slab (notes/modules1/tools/g3_sundif_mc.py: 80 cases, worst 0.030), minus the direct beam; y = that share x mu,
	 * the irradiance on a level surface per unit of the sun. volAirGround.w = 1 + the air's phase g (0: the red
	 * WW_CELL_AIR_SUNDIF_RED=1, the direct beam alone) */
	if ( volAirGround.w <= 0.0 )
		return vec2( Td, 0.0 );
	float pg = volAirGround.w - 1.0, gp = pg / ( 1.0 + pg ), tp = ( 1.0 - pg * pg ) * tau, g1 = 0.75 * ( 1.0 - gp );
	float Tt = 1.0 - ( g1 * tp + ( 0.25 * ( 2.0 - 3.0 * gp * mu ) - g1 * mu ) * ( 1.0 - exp( -tp / mu ) ) ) / ( 1.0 + g1 * tp );
	Tt /= 1.0 + 0.035 * max( pg, 0.0 ) * ( 1.0 / mu - 1.0 ) * ( 1.0 - Td );
	return vec2( Td, max( Tt - Td, 0.0 ) * mu );
}

float wwVolAirSunT( vec3 posView )
{
	return wwVolAirSunTD( posView ).x;
}

vec3 wwFogBase( vec3 preFog, vec3 posView, float tAp, bool apOn );

/* linear in, linear out. Lane SKYA (ATMO2): with the aerial perspective on, the surface goes through the sky's air
 * first, then through the cell fog's residual (atmo_apfog.glsl: the combined transmittance is the min of the two, no
 * haze counted twice); one fog path for both, so no program carries the fog code twice. AP off: today's math. */
vec3 wwFog( vec3 preFog, vec3 posView )
{
	bool apOn = wwApOn();
	vec4 ap = apOn ? wwAtmoApAt( posView ) : vec4( 0.0, 0.0, 0.0, 1.0 );
	if ( apOn )
		preFog = preFog * ap.a + ap.rgb;
	if ( !fogOn )
		return preFog;
	if ( volOn && volSlices.w > 0.5 ) {
		vec4 a = wwVolAirAt( posView );
		if ( apOn ) {
			bool dbl = atApDouble > 0.5;
			float tr = wwApFogResidualT( ap.a, a.a, dbl );
			vec3 lr = dbl ? a.rgb : wwApFogResidualL( a.rgb, a.a, tr );
			if ( fogNight.a > 0.0 )	// R7 night sky d: the two airs' light together (ap.rgb x tr + lr), their T ap.a x tr
				lr = max( lr, fogNight.rgb * ( 1.0 - ap.a * tr ) - ap.rgb * tr );
			return preFog * tr + lr;
		}
		if ( fogNight.a > 0.0 )
			a.rgb = max( a.rgb, fogNight.rgb * ( 1.0 - a.a ) );
		vec3 lit = preFog * a.a + a.rgb;
		if ( volFogBlend == 1 )
			return wwFogBase( lit, posView, 1.0, false );
		if ( volFogBlend == 2 ) {	// red blendnear (gates only): weighted by the air's 1 - T, which has no start distance
			float hb;
			vec3 fc;
			wwFogEval( length( posView ) * fogDistScale, dot( fogView.xyz, posView ) + fogView.w, hb, fc );
			return mix( lit, fc, 1.0 - a.a );
		}
		return lit;
	}
	return wwFogBase( preFog, posView, ap.a, apOn );
}

vec3 wwAirRecordColour( float d, float z, vec3 dirView, out bool ok )
{
	float hb;
	vec3 fc;
	wwFogEval( d, z, hb, fc );
	float sunW = fogSun.w > 0.0 ? pow( max( dot( dirView, fogSun.xyz ), 0.0 ), fogSunColour.w ) * fogSun.w : 0.0;
	vec3 c = max( mix( fc, fogSunColour.rgb, sunW ), vec3( 0.0 ) );
	float lc = dot( c, vec3( 0.2126, 0.7152, 0.0722 ) );
	ok = lc > 1e-8;
	return ok ? c / lc : vec3( 1.0 );
}

// the game's fog alone; with the aerial perspective on, its alpha cut to the residual: 1 - alpha' = min(1, (1 - alpha) / Tap)
vec3 wwFogBase( vec3 preFog, vec3 posView, float tAp, bool apOn )
{
	float d = length( posView ) * fogDistScale;
	float z = dot( fogView.xyz, posView ) + fogView.w;
	float hb;
	vec3 fogCol;
	float alpha = wwFogEval( d, z, hb, fogCol );
	if ( apOn )
		alpha = 1.0 - wwApFogResidualT( tAp, 1.0 - alpha, atApDouble > 0.5 );
	vec3 viewDir = posView / max( length( posView ), 1e-6 );
	float sunW = pow( max( dot( viewDir, fogSun.xyz ), 0.0 ), fogSunColour.w ) * fogSun.w;
	vec3 sunlit = mix( fogCol, fogSunColour.rgb, sunW );
	float gray = dot( preFog, vec3( 1.0 / 3.0 ) );
	float grayW = fogUnit > 0.0 ? gray / fogUnit : gray;	// lane PHYSUNITS1: the weight on the weather's scale
	vec3 resolved = alpha < fogK[2].w ? mix( sunlit, vec3( gray ), grayW ) : sunlit;
	return mix( preFog, resolved, alpha );
}

// lane VOLAIR1: an effect's or a particle's linear colour through the same fog as the opaque draws (wwFog), so a
// card on a far wall fades with the wall. An additive draw takes the transmittance only: its light adds over a
// surface that already holds the in-scatter. Red fxfog (flag 16 clear) and the air off: the game's fog alone, as before.
// Lane SKYA (ATMO2): with the aerial perspective on, a lit effect takes wwFog's split; an additive one the sky's
// transmittance times the fog's residual.
vec3 wwFogFx( vec3 lin, vec3 posView, bool additive )
{
	bool apOn = wwApOn();
	if ( !fogOn && !apOn )
		return lin;
	bool volFx = volOn && volSlices.w > 0.5 && ( volAirFlags & 16 ) != 0;
	if ( !additive && ( volFx || apOn ) )
		return max( wwFog( lin, posView ), vec3( 0.0 ) );
	float tAp = apOn ? wwAtmoApAt( posView ).a : 1.0;
	if ( !fogOn )
		return lin * tAp;
	float hb;
	vec3 fc;
	float f = wwFogEval( length( posView ) * fogDistScale, dot( fogView.xyz, posView ) + fogView.w, hb, fc );
	if ( !volFx ) {
		if ( apOn )
			return lin * tAp * wwApFogResidualT( tAp, 1.0 - f, atApDouble > 0.5 );
		return additive ? lin * ( 1.0 - f ) : mix( lin, fc, f );
	}
	float tv = wwVolAirAt( posView ).a;
	if ( apOn )
		return lin * tAp * wwApFogResidualT( tAp, tv, atApDouble > 0.5 );
	lin *= tv;
	return volFogBlend == 1 ? lin * ( 1.0 - f ) : lin;
}

// gates only: true when the fragment must write `o` raw (no exposure, no view transform)
bool wwFogProbe( vec3 posView, out vec3 o )
{
	o = vec3( 0.0 );
	if ( fogOn && volOn && ( volProbe == 1 || volProbe == 2 ) ) {	// lane VOLAIR1: the air's L or T at this fragment
		vec4 a = wwVolAirAt( posView );
		o = volProbe == 1 ? a.rgb : vec3( a.a );
		return true;
	}
	if ( fogOn && volOn && volProbe == 3 ) {	// FOGSUN1: the sun's transmittance through the air to this fragment, grey
		o = vec3( wwVolAirSunT( posView ) );
		return true;
	}
	if ( !fogOn || fogProbe.z < 0.5 )
		return false;
	if ( fogProbe.z > 8.5 ) {
		// lane VOLAIR1 (the far tail's gate), 16 bits in R (high byte) and G (low): 9 = log2(d) / 20, 10 = (z + 65536)
		// / 131072 (d to ~1e6 units, z in 2-unit steps); B = 0
		float d = length( posView ) * fogDistScale;
		float z = dot( fogView.xyz, posView ) + fogView.w;
		float x = fogProbe.z < 9.5 ? log2( max( d, 1.0 ) ) / 20.0 : ( z + 65536.0 ) / 131072.0;
		float q = floor( clamp( x, 0.0, 1.0 ) * 65535.0 + 0.5 );
		o = vec3( floor( q / 256.0 ) / 255.0, mod( q, 256.0 ) / 255.0, 0.0 );
		return true;
	}
	if ( fogProbe.z > 7.5 ) {
		// lane FOG2, 8: the geometry this fragment's fog read, wide. The gate drops the pixels whose top surface
		// is not the one the position probes saw (a translucent card that serves this probe and not theirs)
		float d = length( posView ) * fogDistScale;
		float z = dot( fogView.xyz, posView ) + fogView.w;
		o = vec3( d / 16384.0, 0.5 + z / 32768.0, 0.0 );
		return true;
	}
	if ( fogProbe.z > 5.5 ) {
		// lane FOG2, per fragment: 6 = (alpha, height blend, 0), 7 = the fog colour ^ (1 / 2.2)
		float d = length( posView ) * fogDistScale;
		float z = dot( fogView.xyz, posView ) + fogView.w;
		float hb;
		vec3 fogCol;
		float alpha = wwFogEval( d, z, hb, fogCol );
		o = fogProbe.z < 6.5 ? vec3( alpha, hb, 0.0 ) : pow( max( fogCol, vec3( 0.0 ) ), vec3( 1.0 / 2.2 ) );
		return true;
	}
	if ( fogProbe.z > 4.5 ) {
		// the geometry the fog reads: R = d / 4096, G = 0.5 + z / 2000
		float d = length( posView ) * fogDistScale;
		float z = dot( fogView.xyz, posView ) + fogView.w;
		o = vec3( d / 4096.0, 0.5 + z / 2000.0, 0.0 );
		return true;
	}
	float hb;
	vec3 fogCol;
	float alpha = wwFogEval( fogProbe.x, fogProbe.y, hb, fogCol );
	if ( fogProbe.z < 1.5 )
		o = vec3( alpha );
	else if ( fogProbe.z < 2.5 )
		o = fogCol * 0.5;
	else
		o = vec3( hb );
	return true;
}
