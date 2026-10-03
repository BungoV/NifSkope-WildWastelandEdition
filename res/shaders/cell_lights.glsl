// lane PRTP3 (cell lights): the cell's placed lights, interior ambient and directional, per
// docs/PRTP2_LIGHT_MODEL.md. Included by fo4_default.frag only when WW_CELLLIGHTS is defined
// (fo4_cell.frag); src/gl/celllights.cpp sets every uniform.

uniform bool cellOn;
uniform samplerBuffer cellLights;	// 8 texels a light: pos+radius, color+cosOuter (-2 omni, -3 hemisphere, -4 box), dir+cone,
									// bias scale exponent flags (1 noSpec, 2 noRim, 4 ignoreRoughness), shadow slot (-1 none)
									// kind near-clip xlig-bias, then the box's three rows (lane HEMI1)
#define CELL_TPL 8					// texels a light (celllights.cpp kTexelsPerLight)
uniform int cellLightCount;
uniform vec4 cellRow[3];			// world = (dot(row.xyz, posView) + row.w), the view's inverse
uniform bool cellHasDalc;
uniform int cellGiAmb;				// lane GICAL1: WW_CELL_GI_AMB, the interior ambient beside the GI: 0 keep, 1 replace, 2 off, 3 max, 4 asgi (red)
uniform bool cellGiFill;			// lane GICAL1: a GI gap takes the weighted mean round it (WW_CELL_GI_FILL=0: off, black as before)
uniform vec4 cellDalc[3];			// per channel: (p - n) / 2 per axis, mean of the six (byte / 255)
uniform bool cellHasDir;
uniform vec3 cellDirColor;			// linear
uniform vec3 cellDirTo;				// world, TO the light
uniform bool cellInterior;
uniform vec3 cellCenter;
uniform int cellProbe;
// lane AMBO2: the Ambient Only volumes (src/gl/celllights.h): world centre + 1.22077 x radius, and the
// per-channel scale of the ambient's affine sum; the first in plugin order that holds a point wins
uniform int cellAmboCount;
uniform vec4 cellAmbo[16];
uniform vec4 cellAmboK[16];
uniform vec4 cellAmboBox[48];		// lane HEMI1: where cellAmboK[i].w is 1 the volume is a box, rows i * 3 .. i * 3 + 2
uniform int cellRed;				// 1 linear: the radial curve without its 2.2; 8 lambert, 16 normalised, 32 norim,
									// 64 rimflags (the lights' rim / roughness flags ignored); 512 cubeold (lane CUBE1)
// lane PRTPGI: the bake relit by these lights (src/probegi.h), six axis slabs of dims.z each, x fastest;
// rgb = irradiance x valid, a = valid (so a filtered sample divides by its own valid)
uniform bool cellGiOn;
uniform sampler3D cellGi;
uniform vec3 cellGiOrigin;
uniform float cellGiVoxel;
uniform vec3 cellGiDims;
// lane ROOMCLAMP1 (src/proberooms.h): with rooms the grid holds 12 slabs (slot 0's six, then slot 1's); cellGiSlots
// per voxel and cellRooms per fine cell hold two rooms packed as (a + 1) x 4096 + (b + 1), -1 none
uniform bool cellGiRooms;
uniform sampler3D cellGiSlots;
uniform sampler3D cellRooms;
uniform vec3 cellRoomsOrigin;
uniform float cellRoomsCell;
uniform vec3 cellRoomsDims;
// lane SKY1 (src/probesky.h): a weather-lit exterior's grid holds the sky the probes see, so it stands in
// for the weather's unshadowed ambient by the grid's valid share (0 where the grid does not reach)
uniform bool cellGiSky;
// lane PROBEVIEW1: the PRTP band's Pass (0 Combined, 1 GI, 2 Sky visibility, 3 Surfel color, 4 Surfel light).
// In the Sky visibility pass the CPU binds the sky grid on cellGi (same layout, rgb = the probes' sky share).
uniform int cellPass;
uniform int cellPassRed;			// the gate's reds: 1 direct (the lights leak into GI), 2 nonormal (sampled facing up)

// lane IMGS1: the cell's imagespace, the game's own HDR -> display chain (src/gl/celllights.h)
uniform bool cellIsOn;
uniform bool cellIsLinear;			// lane HDR1: write linear light, the frame is tone-mapped once (cell_hdr.frag)
uniform float cellIsExposure;		// clamp( middle gray / (adapted + 0.001), min, max ), the CPU's
uniform float cellIsE;				// HNAM Tonemap E: the curve's toe numerator
uniform float cellIsAdapted;		// the frame's mean luminance, the contrast pivot
uniform vec3 cellIsCine;			// CNAM saturation, brightness, contrast
uniform vec4 cellIsTint;			// TNAM amount, r, g, b
uniform bool cellIsLutOn;
uniform sampler3D cellIsLut;
uniform int cellIsRed;				// 1 nolut, 2 noexp, 4 nograde, 8 nobloom (the CPU drops cellIsBloomOn)
// lane BLOOM1: the measure's blurred bright pass, a quarter of the view; rect = viewport origin, 1 / size
uniform bool cellIsBloomOn;
uniform sampler2D cellIsBloom;
uniform vec4 cellIsBloomRect;
// lane SHADOW1: one depth cube a shadow-casting light (distance to the caster / radius); texel = 2 / face
uniform bool cellShadowOn;
uniform samplerCubeArrayShadow cellShadow;
uniform float cellShadowTexel;

// sqrt-of-linear in (this program's convention), display out. Shaders011.fxp, the tonemap PS and the LUT PS.
vec3 cellImageSpace( vec3 sqrtColor )
{
	vec3 x = max( sqrtColor, vec3( 0.0 ) );
	x = x * x;
// lane EFX2: an effect blends over a surface that already took the bloom; lane HDR1: the one tone map takes it
#if !defined( WW_CELL_FX ) || defined( WW_CELL_HDR )
	if ( cellIsBloomOn )	// the tonemap PS adds the bloom target before its exposure multiply
		x += texture( cellIsBloom, ( gl_FragCoord.xy - cellIsBloomRect.xy ) * cellIsBloomRect.zw ).rgb;
#endif
	x = x * ( ( cellIsRed & 2 ) != 0 ? 1.0 : cellIsExposure ) * 2.0;
	float E = cellIsE;
	vec3 c = ( x * ( 0.15 * x + 0.05 ) + 0.2 * E ) / ( x * ( 0.15 * x + 0.5 ) + 0.06 ) - E / 0.3;
	c /= ( 0.2 * E + 19.376 ) * 0.040856 - E / 0.3;		// the curve at W 11.2
	if ( ( cellIsRed & 4 ) == 0 ) {
		float luma = dot( c, vec3( 0.2125, 0.7154, 0.0721 ) );
		c = mix( vec3( luma ), c, cellIsCine.x );
		c = mix( c, luma * cellIsTint.yzw, cellIsTint.x );
		c = cellIsCine.z * ( cellIsCine.y * c - cellIsAdapted ) + cellIsAdapted;
	}
	c = pow( max( c, vec3( 0.0 ) ), vec3( 1.0 / 2.2 ) );
	if ( cellIsLutOn && ( cellIsRed & 1 ) == 0 )
		c = texture( cellIsLut, c * 0.9375 + 0.03125 ).rgb;
	return c;
}

vec3 cellWorldPos( vec3 posView )
{
	vec4 p = vec4( posView, 1.0 );
	return vec3( dot( cellRow[0], p ), dot( cellRow[1], p ), dot( cellRow[2], p ) );
}

vec3 cellWorldDir( vec3 dirView )
{
	return normalize( vec3( dot( cellRow[0].xyz, dirView ), dot( cellRow[1].xyz, dirView ), dot( cellRow[2].xyz, dirView ) ) );
}

// PRTP2 section 1: x = saturate(d / r); atten = pow(1 - saturate(scale * x^exponent + bias), 2.2)
float cellRadial( float d, float r, vec3 bse )
{
	float x = clamp( d / max( r, 0.001 ), 0.0, 1.0 );
	float xe = bse.z > 0.0 ? pow( x, bse.z ) : 1.0;
	float k = 1.0 - clamp( bse.y * xe + bse.x, 0.0, 1.0 );
	return ( cellRed & 1 ) != 0 ? k : pow( k, 2.2 );
}

/* lane SHADOW1: light i's shadow factor at P (normal N, facing it): 1 lit, 0 shadowed. The point lifted
 * 1.5 texels along N (the slope bias), then its distance - 1 unit against 3x3 taps a texel apart (each
 * a hardware 2x2 compare), / 9, as the game's 9 taps. A hemisphere lights nothing behind its plane. */
float cellShadowF( int i, vec3 P, vec3 N )
{
	vec4 t4 = texelFetch( cellLights, i * CELL_TPL + 4 );
	if ( !cellShadowOn || t4.y < 0.5 )
		return 1.0;
	vec4 t0 = texelFetch( cellLights, i * CELL_TPL );
	if ( t4.y > 1.5 && t4.y < 2.5 && dot( P - t0.xyz, texelFetch( cellLights, i * CELL_TPL + 2 ).xyz ) < 0.0 )
		return 0.0;	// the mask's paraboloid: behind the hemisphere's plane is unlit (in a slot or not)
	if ( t4.x < -0.5 )
		return 1.0;	// a shadow light beyond the slot budget: unshadowed (the game's budget is unread)
	vec3 r = P - t0.xyz;
	vec3 rn = r + N * ( 1.5 * length( r ) * cellShadowTexel );
	float d = length( rn );
	float ref = ( d - 1.0 ) / max( t0.w, 0.001 );
	vec3 a = rn / max( d, 0.001 );
	vec3 u = normalize( cross( a, abs( a.z ) < 0.9 ? vec3( 0.0, 0.0, 1.0 ) : vec3( 1.0, 0.0, 0.0 ) ) );
	vec3 v = cross( a, u );
	float s = 0.0;
	for ( int y = -1; y <= 1; y++ )
		for ( int x = -1; x <= 1; x++ )
			s += texture( cellShadow, vec4( a + ( float( x ) * u + float( y ) * v ) * cellShadowTexel, t4.x ), ref );
	return s / 9.0;
}

/* probe 7: the factors of shadow slots 0, 1, 2 in r, g, b as 2/255 + f x 253/255; 0 where that light is out
 * of reach or faces away (N.L under 0.05), or no light holds the slot */
vec3 cellShadowProbe( vec3 P, vec3 N )
{
	vec3 o = vec3( 0.0 );
	for ( int i = 0; i < cellLightCount; i++ ) {
		float slot = texelFetch( cellLights, i * CELL_TPL + 4 ).x;
		if ( slot < -0.5 || slot > 2.5 )
			continue;
		vec4 t0 = texelFetch( cellLights, i * CELL_TPL );
		vec3 Lv = t0.xyz - P;
		float d = length( Lv );
		if ( d >= t0.w || dot( N, Lv / max( d, 0.001 ) ) < 0.05 )
			continue;
		o[int( slot + 0.5 )] = ( 2.0 + cellShadowF( i, P, N ) * 253.0 ) / 255.0;
	}
	return o;
}

// the bounce's irradiance at P, normal N: the three facing slabs blended by n^2, sampled half a
// voxel off the surface (the grid's voxels behind a wall are its other room's)
ivec2 cellRoomUnpack( float v )
{
	int k = int( v + 0.5 );
	return ivec2( k / 4096 - 1, k - ( k / 4096 ) * 4096 - 1 );
}
ivec2 cellRoomAt( vec3 q )
{
	vec3 c = floor( ( q - cellRoomsOrigin ) / cellRoomsCell );
	if ( any( lessThan( c, vec3( 0.0 ) ) ) || any( greaterThanEqual( c, cellRoomsDims ) ) )
		return ivec2( -1 );
	return cellRoomUnpack( texelFetch( cellRooms, ivec3( c ), 0 ).r );
}
// lane ROOMCLAMP1: the blend of the 8 voxels' slots of room L, trilinear weights (sum in .rgba, weight in ws)
vec4 cellGiRoomBlend( vec3 P, vec3 N, ivec2 L, out float ws )
{
	vec3 g = ( P + N * ( 0.5 * cellGiVoxel ) - cellGiOrigin ) / cellGiVoxel - 0.5;
	ivec3 i0 = ivec3( floor( g ) );
	vec3 f = g - vec3( i0 );
	ivec3 dm = ivec3( cellGiDims );
	ivec3 sl = ivec3( N.x >= 0.0 ? 0 : 1, N.y >= 0.0 ? 2 : 3, N.z >= 0.0 ? 4 : 5 ) * dm.z;
	vec3 n2 = N * N;
	vec4 s = vec4( 0.0 );
	ws = 0.0;
	for ( int k = 0; k < 8; k++ ) {
		ivec3 o = ivec3( k & 1, ( k >> 1 ) & 1, ( k >> 2 ) & 1 );
		ivec3 c = clamp( i0 + o, ivec3( 0 ), dm - 1 );
		vec3 t = mix( 1.0 - f, f, vec3( o ) );
		float w = t.x * t.y * t.z;
		ivec2 S = cellRoomUnpack( texelFetch( cellGiSlots, c, 0 ).r );
		int base = ( S.x >= 0 && ( S.x == L.x || S.x == L.y ) ) ? 0 : ( S.y >= 0 && ( S.y == L.x || S.y == L.y ) ) ? 6 * dm.z : -1;
		if ( base < 0 || w <= 0.0 )
			continue;
		vec4 v = n2.x * texelFetch( cellGi, ivec3( c.xy, c.z + base + sl.x ), 0 )
		       + n2.y * texelFetch( cellGi, ivec3( c.xy, c.z + base + sl.y ), 0 )
		       + n2.z * texelFetch( cellGi, ivec3( c.xy, c.z + base + sl.z ), 0 );
		if ( v.a <= 0.0 )
			continue;   // lane ROOMCLAMP1: an empty slot (no probe of its room reached the voxel): no weight
		s += w * v;
		ws += w;
	}
	return s;
}
// lane ROOMCLAMP1: the blend of only the voxels (slots) of the surface's own room, trilinear weights renormalized;
// the room read at P + N x 0.75 cell, else 1.75 cell (the surface's own cell is solid). Both in a wall's cells (a
// floor beside a thin wall, an inner corner): of the 8 cells round either read in the surface's plane, the air cell
// nearest its read (the sum of the faces crossed) whose blend has weight, the 0.75 read first on a tie (an inner
// corner's nearest air is the outdoors across the wall, which no voxel there holds). The same search when the
// read room's blend has no weight (a pocket behind a pipe: no voxel holds it). .a < 0: no room with weight (the
// plain trilinear; black was the old answer)
// Ls (lane GICAL1): the surface's room whether or not its blend has weight (the read, else the nearest air cell beside
// it; -1: none), for the gap fill
vec4 cellGiRoomSample( vec3 P, vec3 N, out ivec2 Ls )
{
	float ws;
	ivec2 L = cellRoomAt( P + N * ( 0.75 * cellRoomsCell ) );
	if ( L.x < 0 )
		L = cellRoomAt( P + N * ( 1.75 * cellRoomsCell ) );
	Ls = L;
	if ( L.x >= 0 ) {
		vec4 s = cellGiRoomBlend( P, N, L, ws );
		if ( ws > 0.0 )
			return s / ws;
	}
	vec3 aN = abs( N );
	int m = ( aN.x >= aN.y && aN.x >= aN.z ) ? 0 : ( aN.y >= aN.z ? 1 : 2 );
	int ta = m == 0 ? 1 : 0, tb = m == 2 ? 1 : 2;
	float best = 3.0, bestAny = 3.0;
	bool direct = L.x >= 0;
	bool found = false;
	vec4 r = vec4( 0.0 );
	for ( int k = 0; k < 2; k++ ) {
		vec3 q = P + N * ( ( k == 0 ? 0.75 : 1.75 ) * cellRoomsCell );
		vec3 u = ( q - cellRoomsOrigin ) / cellRoomsCell;
		vec3 f = u - floor( u );
		for ( int sa = -1; sa <= 1; sa++ ) {
			for ( int sb = -1; sb <= 1; sb++ ) {
				if ( sa == 0 && sb == 0 )
					continue;
				vec3 e = vec3( 0.0 );
				e[ta] = float( sa );
				e[tb] = float( sb );
				ivec2 c = cellRoomAt( q + e * cellRoomsCell );
				if ( c.x < 0 )
					continue;
				float d = ( sa > 0 ? 1.0 - f[ta] : sa < 0 ? f[ta] : 0.0 ) + ( sb > 0 ? 1.0 - f[tb] : sb < 0 ? f[tb] : 0.0 );
				if ( !direct && d < bestAny ) {
					bestAny = d;
					Ls = c;	// lane GICAL1: the nearest air cell, with weight or not
				}
				if ( d >= best )
					continue;
				vec4 s = cellGiRoomBlend( P, N, c, ws );
				if ( ws > 0.0 ) {
					found = true;
					best = d;
					r = s / ws;
				}
			}
		}
	}
	return found ? r : vec4( -1.0 );
}

/* lane GICAL1 (item 5): the gap fill. A surface whose blend has no valid weight (no probe reached the 8 voxels: today
 * black, or with rooms the plain trilinear, which ignores walls) takes the weighted mean of the valid voxels within
 * 2 voxels (the 4x4x4 block round the sample), of the surface's own room (L) when the grid has rooms; weight
 * (1 - d / 2.5)^2 in voxels. Returns rgba / the weight (cellGiE divides by .a); .a = 0: nothing in reach. */
vec4 cellGiFillAt( vec3 P, vec3 N, ivec2 L )
{
	vec3 g = ( P + N * ( 0.5 * cellGiVoxel ) - cellGiOrigin ) / cellGiVoxel - 0.5;
	ivec3 i0 = ivec3( floor( g ) );
	ivec3 dm = ivec3( cellGiDims );
	ivec3 sl = ivec3( N.x >= 0.0 ? 0 : 1, N.y >= 0.0 ? 2 : 3, N.z >= 0.0 ? 4 : 5 ) * dm.z;
	vec3 n2 = N * N;
	vec4 s = vec4( 0.0 );
	float ws = 0.0;
	for ( int k = 0; k < 64; k++ ) {
		ivec3 c = i0 + ivec3( k & 3, ( k >> 2 ) & 3, ( k >> 4 ) & 3 ) - 1;
		if ( any( lessThan( c, ivec3( 0 ) ) ) || any( greaterThanEqual( c, dm ) ) )
			continue;
		float w = 1.0 - length( vec3( c ) - g ) / 2.5;
		if ( w <= 0.0 )
			continue;
		int base = 0;
		if ( cellGiRooms ) {
			if ( L.x < 0 )
				return vec4( 0.0 );	// rooms but no room here: nothing safe to borrow
			ivec2 S = cellRoomUnpack( texelFetch( cellGiSlots, c, 0 ).r );
			base = ( S.x >= 0 && ( S.x == L.x || S.x == L.y ) ) ? 0 : ( S.y >= 0 && ( S.y == L.x || S.y == L.y ) ) ? 6 * dm.z : -1;
			if ( base < 0 )
				continue;
		}
		vec4 v = n2.x * texelFetch( cellGi, ivec3( c.xy, c.z + base + sl.x ), 0 )
		       + n2.y * texelFetch( cellGi, ivec3( c.xy, c.z + base + sl.y ), 0 )
		       + n2.z * texelFetch( cellGi, ivec3( c.xy, c.z + base + sl.z ), 0 );
		if ( v.a <= 0.0 )
			continue;
		w *= w;
		s += w * v;
		ws += w;
	}
	return ws > 0.0 ? s / ws : vec4( 0.0 );
}

vec4 cellGiSample( vec3 P, vec3 N )
{
	bool fill = cellGiFill && cellInterior && !cellGiSky;	// interiors only: an exterior's gap keeps the weather's ambient (cellGiSkyK), also under the keepamb red
	ivec2 Ls = ivec2( -1 );
	if ( cellGiRooms && ( cellPassRed & 8 ) == 0 ) {	// lane ROOMCLAMP1 (red noclamp: WW_CELL_PV_RED)
		vec4 r = cellGiRoomSample( P, N, Ls );
		if ( r.a > 0.01 || ( r.a >= 0.0 && !fill ) )
			return r;
		if ( fill ) {	// lane GICAL1: a gap, or no room with weight (the plain trilinear ignores walls)
			vec4 f = cellGiFillAt( P, N, Ls );
			if ( f.a > 0.01 || r.a >= 0.0 )
				return f.a > 0.01 ? f : r;
		}
	}
	vec3 g = ( P + N * ( 0.5 * cellGiVoxel ) - cellGiOrigin ) / cellGiVoxel;
	vec2 xy = g.xy / cellGiDims.xy;
	float z = clamp( g.z, 0.5, cellGiDims.z - 0.5 );
	float depth = ( cellGiRooms ? 12.0 : 6.0 ) * cellGiDims.z;
	vec3 n2 = N * N;
	vec4 s = n2.x * texture( cellGi, vec3( xy, ( z + ( N.x >= 0.0 ? 0.0 : 1.0 ) * cellGiDims.z ) / depth ) )
	       + n2.y * texture( cellGi, vec3( xy, ( z + ( N.y >= 0.0 ? 2.0 : 3.0 ) * cellGiDims.z ) / depth ) )
	       + n2.z * texture( cellGi, vec3( xy, ( z + ( N.z >= 0.0 ? 4.0 : 5.0 ) * cellGiDims.z ) / depth ) );
	if ( fill && !cellGiRooms && s.a <= 0.01 ) {	// lane GICAL1: a gap in a grid without rooms
		vec4 f = cellGiFillAt( P, N, Ls );
		if ( f.a > 0.01 )
			return f;
	}
	return s;
}

vec3 cellGiE( vec3 P, vec3 N )
{
	vec4 s = cellGiSample( P, N );
	// lane SKY1: with the sky in the grid the sample keeps its valid share (E x share), the other
	// ( 1 - share ) being the weather's own ambient the program leaves in (cellGiSkyK)
	if ( cellGiSky )
		return max( s.rgb, vec3( 0.0 ) );
	return s.a > 0.01 ? max( s.rgb / s.a, vec3( 0.0 ) ) : vec3( 0.0 );
}
// lane SKY1: the share of the weather's ambient the grid's sky replaces at P (0: not a sky grid)
float cellGiSkyK( vec3 P, vec3 N )
{
	if ( !cellGiOn || !cellGiSky || cellInterior )
		return 0.0;
	return clamp( cellGiSample( P, N ).a, 0.0, 1.0 );
}

/* lane HEMI1: the game draws a hemisphere or box light as an omni light clipped by its volume (celllights.h):
 * true when P lies inside light i's. shape = texel 1.w (-3 hemisphere, -4 box; anything else has none). */
bool cellShapeIn( int i, vec3 P, vec3 Lpos, float shape )
{
	if ( shape > -2.5 )
		return true;
	if ( shape > -3.5 )
		return dot( P - Lpos, texelFetch( cellLights, i * CELL_TPL + 2 ).xyz ) >= 0.0;
	vec4 b0 = texelFetch( cellLights, i * CELL_TPL + 5 );
	vec4 b1 = texelFetch( cellLights, i * CELL_TPL + 6 );
	vec4 b2 = texelFetch( cellLights, i * CELL_TPL + 7 );
	vec3 k = vec3( dot( b0.xyz, P ) + b0.w, dot( b1.xyz, P ) + b1.w, dot( b2.xyz, P ) + b2.w );
	return all( lessThanEqual( abs( k ), vec3( 1.0 ) ) );
}

/* light i at world point P, normal N: its colour x the radial curve x the spot cone (no N.L), and
 * the direction to it; zero when out of reach or behind the surface. The PBR path's per-light term. */
vec3 cellLightE( int i, vec3 P, vec3 N, out vec3 L, out bool noSpec )
{
	vec4 t0 = texelFetch( cellLights, i * CELL_TPL );
	vec3 Lv = t0.xyz - P;
	float d = length( Lv );
	L = Lv / max( d, 0.001 );
	noSpec = true;
	if ( d >= t0.w || dot( N, L ) <= 0.0 )
		return vec3( 0.0 );
	vec4 t1 = texelFetch( cellLights, i * CELL_TPL + 1 );
	if ( !cellShapeIn( i, P, t0.xyz, t1.w ) )
		return vec3( 0.0 );	// lane HEMI1
	vec4 t3 = texelFetch( cellLights, i * CELL_TPL + 3 );
	float a = cellRadial( d, t0.w, t3.xyz );
	if ( t1.w > -1.5 ) {
		vec4 t2 = texelFetch( cellLights, i * CELL_TPL + 2 );
		float base = clamp( 1.0 - ( 1.0 - dot( -L, t2.xyz ) ) / max( 1.0 - t1.w, 1e-4 ), 0.0, 1.0 );
		a *= min( pow( base, max( t2.w, 1e-3 ) ), 1.0 );
	}
	if ( a > 0.0 )
		a *= cellShadowF( i, P, N );
	noSpec = ( int( t3.w + 0.5 ) & 1 ) != 0;
	return t1.rgb * a;
}

// the harness probes for a program without the legacy BRDF helpers (pbrm_cell)
vec3 cellProbeRaw( vec3 P, vec3 N )
{
	if ( cellProbe >= 70 && cellProbe <= 74 )	// lane FXLIT1's probes: the lit effects alone, a surface writes black
		return vec3( 0.0 );
	if ( cellProbe == 5 )
		return cellGiOn ? clamp( cellGiE( P, N ) * 0.31830989, 0.0, 1.0 ) : vec3( 0.0 );
	if ( cellProbe == 90 )	// lane SKY1: the share of the weather's ambient the grid's sky replaced
		return vec3( cellGiSkyK( P, N ) );
	if ( cellProbe == 1 ) {
		vec3 E = vec3( 0.0 );
		for ( int i = 0; i < cellLightCount; i++ ) {
			vec3 L;
			bool ns;
			vec3 c = cellLightE( i, P, N, L, ns );
			E += c * max( dot( N, L ), 0.0 );
		}
		return clamp( E * 0.25, 0.0, 1.0 );
	}
	if ( cellProbe == 7 )
		return cellShadowProbe( P, N );
	vec3 q = clamp( floor( P - cellCenter + 32768.0 ), 0.0, 65535.0 );
	if ( cellProbe == 2 )
		return floor( q / 256.0 ) / 255.0;
	if ( cellProbe == 3 )
		return mod( q, 256.0 ) / 255.0;
	return N * 0.5 + 0.5;
}

/* lane PROBEVIEW1: a probe pass's picture on a white surface (the deck's s40 / s18): no albedo, no direct light,
 * no fog, no imagespace. GI = E(N) / pi, the GI row's own sample; Sky visibility = the sky grid read the same way.
 * Display: pow(clamp(x), 1 / 2.2), a fixed curve so two bakes compare. Magenta = no probe reaches here (s65);
 * in the surfel passes every surface is magenta and the surfel tiles draw over it (src/gl/cellprobeview.h). */
vec3 cellPassOut( vec3 P, vec3 N )
{
	const vec3 none = vec3( 1.0, 0.0, 1.0 );
	if ( cellPass >= 3 || !cellGiOn )
		return none;
	vec4 s = cellGiSample( P, ( cellPassRed & 2 ) != 0 ? vec3( 0.0, 0.0, 1.0 ) : N );
	if ( s.a <= 0.01 )
		return none;
	vec3 v = max( s.rgb / s.a, vec3( 0.0 ) );
	if ( cellPass == 1 ) {
		v *= 0.31830989;
		if ( ( cellPassRed & 1 ) != 0 ) {	// red "direct": the cell's lights leak into the pass
			for ( int i = 0; i < cellLightCount; i++ ) {
				vec3 L;
				bool ns;
				vec3 c = cellLightE( i, P, N, L, ns );
				v += c * max( dot( N, L ), 0.0 );
			}
		}
	}
	return pow( clamp( v, 0.0, 1.0 ), vec3( 1.0 / 2.2 ) );
}

#if !defined( WW_CELL_PBR ) && !defined( WW_CELL_FX )	// lane EFX2: the effect program takes none of the surface lobes
/* The game's own light specular (the shipped deferred point/spot light shaders, read op for op):
 * normalized Blinn-Phong, n = 2^(gloss x 10 + 1), D = NdotH^n (n + 2) / 2pi, Schlick Fresnel with
 * F0 = 0.2, a Cook-Torrance geometry select with the 1/NdotV folded in, x 1/4, clamped at 15, x pi.
 * The caller multiplies the light's irradiance (N.L inside) and the material's mask and colour. */
float cellSpecGame( vec3 N, vec3 L, vec3 V, float gloss )
{
	float n = exp2( gloss * 10.0 + 1.0 );
	vec3 H = normalize( V + L );
	float NdotL = clamp( dot( N, L ), 0.0, 1.0 );
	float NdotV = clamp( dot( N, V ), 0.0, 1.0 );
	float VdotH = clamp( dot( V, H ), 0.0, 1.0 );
	float NdotH = clamp( dot( N, H ), 0.0, 1.0 );
	float D = pow( NdotH, n ) * ( n + 2.0 ) * 0.159155;
	float m = min( NdotL, NdotV );
	float G = ( VdotH >= 2.0 * NdotH * m ) ? 2.0 * NdotH * ( NdotV == m ? 1.0 : NdotL / max( NdotV, 1e-4 ) ) / max( VdotH, 1e-4 )
	                                       : 1.0 / max( NdotV, 1e-4 );
	float f1 = 1.0 - VdotH;
	float f4 = f1 * f1 * f1 * f1;
	float F = min( ( 1.0 - f1 * f4 ) * 0.2 + f4 * f1, 1.0 );
	return min( D * G * F * 0.25, 15.0 ) * 3.141593;
}

/* Lane ON1: the game's legacy diffuse (the same shaders, the spec / gloss branch, and its sun): Oren-Nayar
 * with sigma = 1 - gloss, A = 1 - 0.5 s2 / (s2 + 0.57), B = 0.45 s2 / (s2 + 0.09), the azimuth cosine from the
 * UNnormalised tangent-plane projections, x sinL sinV / max(NdotL, NdotV). The factor that multiplies NdotL. */
float cellOren( vec3 N, vec3 L, vec3 V, float NdotL, float gloss )
{
	if ( ( cellRed & 8 ) != 0 )
		return 1.0;	// WW_CELL_LIT_RED=lambert
	float s2 = ( 1.0 - gloss ) * ( 1.0 - gloss );
	float A = 1.0 - 0.5 * s2 / ( s2 + 0.57 );
	float B = 0.45 * s2 / ( s2 + 0.09 );
	float NdotV = dot( N, V );
	vec3 tV = V - N * NdotV, tL = L - N * NdotL;
	float cosPhi = dot( tV, tL );
	if ( ( cellRed & 16 ) != 0 )
		cosPhi /= max( length( tV ) * length( tL ), 1e-6 );	// WW_CELL_LIT_RED=normalised: the textbook azimuth
	float geom = sqrt( clamp( ( 1.0 - NdotL * NdotL ) * ( 1.0 - NdotV * NdotV ), 0.0, 1.0 ) ) / max( max( NdotL, NdotV ), 1e-4 );
	return max( cosPhi, 0.0 ) * B * geom + A;
}

/* Lane RIM1: the game's back-light term, added to the diffuse by every light and the sun (legacy materials; the
 * PBR branch drops it): saturate(dot(V, -L)) x (1 - NdotV)^0.01 x (1 - gloss), times NdotL and the light like the
 * diffuse (the shadow scales it too). Bright where the camera looks toward the lamp across a rough surface. */
float cellRim( vec3 N, vec3 L, vec3 V, float gloss )
{
	if ( ( cellRed & ( 8 | 32 ) ) != 0 )
		return 0.0;	// WW_CELL_LIT_RED=lambert / norim
	float edge = pow( clamp( 1.0 - dot( N, V ), 0.0, 1.0 ), 0.01 );
	return clamp( dot( V, -L ), 0.0, 1.0 ) * edge * ( 1.0 - gloss );
}

/* the placed lights at world point P, world normal N: the irradiance (Lambert, what the GI, the reflection and
 * probe 1 read), the game's Oren-Nayar diffuse (what the albedo takes) and the specular sum */
vec3 cellRimSum;	// lane RIM1: the rim part of diffOn alone, for probe 10 (set by cellSumLights)
void cellSumLights( vec3 P, vec3 N, vec3 Vw, float gloss, out vec3 diff, out vec3 diffOn, out vec3 spec )
{
	diff = vec3( 0.0 );
	diffOn = vec3( 0.0 );
	cellRimSum = vec3( 0.0 );
	spec = vec3( 0.0 );
	for ( int i = 0; i < cellLightCount; i++ ) {
		vec4 t0 = texelFetch( cellLights, i * CELL_TPL );
		vec3 Lv = t0.xyz - P;
		float d = length( Lv );
		if ( d >= t0.w )
			continue;
		vec3 L = Lv / max( d, 0.001 );
		float NdotL = dot( N, L );
		if ( NdotL <= 0.0 )
			continue;
		vec4 t1 = texelFetch( cellLights, i * CELL_TPL + 1 );
		if ( !cellShapeIn( i, P, t0.xyz, t1.w ) )
			continue;	// lane HEMI1
		vec4 t3 = texelFetch( cellLights, i * CELL_TPL + 3 );
		float a = cellRadial( d, t0.w, t3.xyz );
		if ( t1.w > -1.5 ) {
			// PRTP2 section 2: base = saturate(1 - (1 - dot(-L, dir)) / (1 - cosOuter)), cone = min(base^falloff, 1)
			vec4 t2 = texelFetch( cellLights, i * CELL_TPL + 2 );
			float base = clamp( 1.0 - ( 1.0 - dot( -L, t2.xyz ) ) / max( 1.0 - t1.w, 1e-4 ), 0.0, 1.0 );
			a *= min( pow( base, max( t2.w, 1e-3 ) ), 1.0 );
		}
		if ( a > 0.0 )
			a *= cellShadowF( i, P, N );	// lane SHADOW1
		vec3 E = t1.rgb * a * NdotL;
		diff += E;
		// lane RIM1: No Rim Lighting (2) drops the back-light; Ignore Roughness (4) also takes Lambert's diffuse
		int fl = int( t3.w + 0.5 ) & ( ( cellRed & 64 ) != 0 ? 1 : 7 );	// WW_CELL_LIT_RED=rimflags
		float rim = ( fl & 6 ) != 0 ? 0.0 : cellRim( N, L, Vw, gloss );
		diffOn += E * ( ( ( fl & 4 ) != 0 ? 1.0 : cellOren( N, L, Vw, NdotL, gloss ) ) + rim );
		cellRimSum += E * rim;
		// lane POOL1: Non Specular (1) has no specular in the game's light shaders either
		if ( ( fl & 1 ) == 0 || ( cellRed & 65536 ) != 0 )	// WW_CELL_SPEC_RED=nonspec
			spec += E * cellSpecGame( N, L, Vw, gloss );
	}
}


#endif

// lane AMBO2: the scale an Ambient Only volume puts on the ambient at P (1 outside every volume)
vec3 cellAmboScale( vec3 P )
{
	for ( int i = 0; i < cellAmboCount; i++ ) {
		if ( cellAmboK[i].w > 0.5 ) {	// lane HEMI1: linked to a box, it fills the box (not the sphere)
			vec4 b0 = cellAmboBox[i * 3], b1 = cellAmboBox[i * 3 + 1], b2 = cellAmboBox[i * 3 + 2];
			vec3 k = vec3( dot( b0.xyz, P ) + b0.w, dot( b1.xyz, P ) + b1.w, dot( b2.xyz, P ) + b2.w );
			if ( all( lessThan( abs( k ), vec3( 1.0 ) ) ) )
				return cellAmboK[i].rgb;
			continue;
		}
		vec3 d = P - cellAmbo[i].xyz;
		if ( dot( d, d ) < cellAmbo[i].w * cellAmbo[i].w )
			return cellAmboK[i].rgb;
	}
	return vec3( 1.0 );
}

// the ambient's affine sum per channel, before its power (lane AMBO2: scaled inside an Ambient Only volume)
vec3 cellAmbientSum( vec3 N, vec3 P )
{
	vec4 n1 = vec4( N, 1.0 );
	return vec3( dot( cellDalc[0], n1 ), dot( cellDalc[1], n1 ), dot( cellDalc[2], n1 ) ) * cellAmboScale( P );
}

// PRTP2 section 4: ambient(n) = pow(max(dot(row, (n, 1)), 0), 2.2) per channel
vec3 cellAmbient( vec3 N, vec3 P )
{
	return pow( max( cellAmbientSum( N, P ), vec3( 0.0 ) ), vec3( 2.2 ) );
}

/* lane GICAL1 (item 2, docs/cloud/GICAL1_DESIGN.md 6.3): the interior ambient beside our GI, as WW_CELL_GI_AMB asks.
 * gi = the GI's irradiance / pi, the ambient's own units. keep (0, the default: both, as before), replace (1: the
 * ambient x (1 - the grid's valid share), as SKY1 does outdoors), off (2), max (3: the larger of the two), asgi
 * (4, the measure's red: the ambient := the GI). Without the GI every rule but off keeps the ambient. */
vec3 cellAmbGi( vec3 amb, vec3 gi, vec3 P, vec3 N )
{
	if ( cellGiAmb == 2 )
		return vec3( 0.0 );	// off: also with the GI off (the energy check's "neither" arm)
	if ( cellGiAmb == 0 || !cellGiOn )
		return amb;
	if ( cellGiAmb == 1 )
		return amb * ( 1.0 - clamp( cellGiSample( P, N ).a, 0.0, 1.0 ) );
	if ( cellGiAmb == 3 )
		return max( amb - gi, vec3( 0.0 ) );
	return gi;
}

#ifndef WW_CELL_FX
/* lane CUBE1: per draw (src/gl/renderer.cpp): the material's specular scale (0 with its specular switch off),
 * its smoothness, 3 when its OWN env map is bound and the sampler decodes its sRGB (1: own, but untagged, decoded
 * here; 2: a .pbrm shape, which keeps the PBR law), the gate's number */
uniform vec4 cellCubeMat;

bool cellCubeGameOn()
{
	return abs( cellCubeMat.z - 1.0 ) < 0.5 || abs( cellCubeMat.z - 3.0 ) < 0.5;
}

/* lane CUBE1: the game's interior env reflection before the light (its deferred composite, read op for op).
 * Indoors only the material's own env map reflects (no default cube). sRGB texel at mip (1 - gloss) x 6 + view
 * depth / 512 on a 128 cube, x 3 x saturate(spec) x min(sqrt(saturate(gloss - 0.3)), 1) x min(env scale, 50).
 * No fresnel, no specular colour, no env mask. Linear; the caller multiplies the diffuse light. */
vec3 cellCubeGame( samplerCube cube, vec3 dir, float gloss, float spec, float envScale, vec3 posView )
{
	if ( !cellCubeGameOn() )
		return vec3( 0.0 );
	float g = clamp( gloss, 0.0, 1.0 );
	float depth = abs( posView.z ) * length( cellRow[0].xyz );	// view units -> game units
	float lod = ( 1.0 - g ) * 6.0 + depth * 0.001953 + log2( float( textureSize( cube, 0 ).x ) / 128.0 );
	vec3 c = textureLod( cube, dir, max( lod, 0.0 ) ).rgb;
	if ( cellCubeMat.z < 2.0 )	// an untagged cube: the game's sampler decodes sRGB before the filter, this after
		c = mix( c / 12.92, pow( ( c + 0.055 ) / 1.055, vec3( 2.4 ) ), step( vec3( 0.04045 ), c ) );
	float k = 3.0 * clamp( spec, 0.0, 1.0 ) * min( sqrt( clamp( g - 0.3, 0.0, 1.0 ) ), 1.0 ) * clamp( envScale, 0.0, 50.0 );
	return c * k;
}

/* lane CUBE1's gate probes: 50 the reflection above / 4 (the caller's), 51 (u hi, u lo, number), 52 (v hi, v lo,
 * number) of the texture coordinate's fraction, 53 probe 4's normal rounding residual (16 bits a component) */
vec3 cellCubeProbe( vec2 uv, vec3 normalView )
{
	vec2 f = clamp( floor( fract( uv ) * 65535.0 + 0.5 ), 0.0, 65535.0 );
	float tag = cellCubeMat.w / 255.0;
	if ( cellProbe == 51 )
		return vec3( floor( f.x / 256.0 ) / 255.0, mod( f.x, 256.0 ) / 255.0, tag );
	if ( cellProbe == 52 )
		return vec3( floor( f.y / 256.0 ) / 255.0, mod( f.y, 256.0 ) / 255.0, tag );
	vec3 n = ( cellWorldDir( normalView ) * 0.5 + 0.5 ) * 255.0;
	return n - floor( n + 0.5 ) + 0.5;
}
#endif

#if !defined( WW_CELL_PBR ) && !defined( WW_CELL_FX )

/* The lit colour, in the program's sqrt-of-linear space. Interior: the cell's ambient, directional
 * and placed lights replace the viewport light. Exterior: the placed lights add to what the
 * viewport (or the Lookdev sun) already lit. */
vec3 cellLit( vec3 color, vec3 albedo, vec3 normalView, vec3 posView, vec3 Vview, float specMask, vec3 specCol,
              float alphaR, float kSmith, vec3 emissive, vec3 cubeK )
{
	vec3 P = cellWorldPos( posView );
	vec3 N = cellWorldDir( normalView );
	vec3 Vw = cellWorldDir( Vview );
	vec3 diff, diffOn, spec;
	float gloss = 1.0 - sqrt( alphaR );	// the program's rough = 1 - gloss, alphaR = rough^2
	cellSumLights( P, N, Vw, gloss, diff, diffOn, spec );
	vec3 alb = albedo * albedo;
	vec3 gi = cellGiOn ? cellGiE( P, N ) * 0.31830989 : vec3( 0.0 );
	vec3 add = alb * ( diffOn + gi ) + spec * specMask * specCol;	// albedo x the game's diffuse; the GI stays Lambert
	if ( !cellInterior )
		return sqrt( color * color + add );
	// the light reaching this point (a white surface would show it): the cubemap reflection is lit by it,
	// so a dark corner's reflection is dark (the viewport path scales it by its own light the same way)
	vec3 E = diff + gi;
	vec3 Ed = diffOn + gi;	// what the albedo takes: the direct terms through Oren-Nayar (lane ON1)
	if ( cellHasDalc ) {
		vec3 amb = cellAmbient( N, P );
		if ( cellGiAmb != 0 )
			amb = cellAmbGi( amb, gi, P, N );	// lane GICAL1
		E += amb;
		Ed += amb;
	}
	if ( cellHasDir ) {
		vec3 Ld = normalize( cellDirTo );
		float nl = dot( N, Ld );
		vec3 dir = cellDirColor * max( nl, 0.0 );
		E += dir;
		Ed += dir * ( cellOren( N, Ld, Vw, nl, gloss ) + cellRim( N, Ld, Vw, gloss ) );
	}
	// lane CUBE1: the game multiplies its cube term by the diffuse light (Ed); the old law took the Lambert sum
	vec3 lin = alb * Ed + spec * specMask * specCol + cubeK * ( ( cellRed & 512 ) != 0 ? E : Ed );
	return sqrt( max( lin, vec3( 0.0 ) ) ) + emissive;
}

// the harness probes (WW_CELL_LIT_PROBE): raw values, no tonemap
vec3 cellProbeOut( vec3 normalView, vec3 posView, float alphaR, float kSmith )
{
	vec3 P = cellWorldPos( posView );
	vec3 N = cellWorldDir( normalView );
	if ( cellProbe >= 70 && cellProbe <= 74 )	// lane FXLIT1's probes: the lit effects alone, a surface writes black
		return vec3( 0.0 );
	if ( cellProbe == 5 )
		return cellGiOn ? clamp( cellGiE( P, N ) * 0.31830989, 0.0, 1.0 ) : vec3( 0.0 );
	if ( cellProbe == 90 )	// lane SKY1 (60-74 are other lanes'): the share of the weather's ambient the grid's sky replaced
		return vec3( cellGiSkyK( P, N ) );
	if ( cellProbe == 1 || cellProbe == 8 || cellProbe == 10 || cellProbe == 30 ) {
		// 1: the irradiance / 4; 8 (lanes ON1, RIM1): the game's diffuse (Oren-Nayar + rim) / 4, seen from the camera;
		// 10 (lane RIM1): the rim alone x 4, sixteen times probe 8's reach, so the per-light rim flags show
		vec3 diff, diffOn, spec;
		cellSumLights( P, N, cellProbe == 1 ? N : cellWorldDir( -posView ), 1.0 - sqrt( alphaR ), diff, diffOn, spec );
		if ( cellProbe == 10 )
			return clamp( cellRimSum * 4.0, 0.0, 1.0 );
		if ( cellProbe == 30 )	// lane POOL1 (11 is lane AMBO2's): the placed lights' specular / 4, before the mask
			return clamp( spec * 0.25, 0.0, 1.0 );
		return clamp( ( cellProbe == 8 ? diffOn : diff ) * 0.25, 0.0, 1.0 );
	}
	if ( cellProbe == 9 )
		return vec3( 1.0 - sqrt( alphaR ), 0.0, 0.0 );	// lane ON1: the gloss the diffuse used
	if ( cellProbe == 11 )	// lane AMBO2: the interior ambient's affine sum before its 2.2, x 8 (it is dim)
		return cellHasDalc ? clamp( cellAmbientSum( N, P ) * 8.0, 0.0, 1.0 ) : vec3( 0.0 );
	if ( cellProbe == 7 )
		return cellShadowProbe( P, N );
	// the position, 16 bits an axis over the 65536-unit box around the centre
	vec3 q = clamp( floor( P - cellCenter + 32768.0 ), 0.0, 65535.0 );
	if ( cellProbe == 2 )
		return floor( q / 256.0 ) / 255.0;
	if ( cellProbe == 3 )
		return mod( q, 256.0 ) / 255.0;
	return N * 0.5 + 0.5;
}
#endif
