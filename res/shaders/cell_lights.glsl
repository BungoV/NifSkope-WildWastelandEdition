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
uniform int cellGiAmb;				// lane GICAL1: WW_CELL_GI_AMB, the interior ambient beside the GI: 0 keep (lane GIAMB1: the red), 1 replace (lane GIAMB1: the default), 2 off, 3 max, 4 asgi (red)
uniform bool cellGiFill;			// lane GICAL1: a GI gap takes the weighted mean round it (WW_CELL_GI_FILL=0: off, black as before)
uniform vec4 cellDalc[3];			// per channel: (p - n) / 2 per axis, mean of the six (byte / 255)
uniform bool cellHasDir;
uniform bool cellExtDalc;			// lane SUNCELL1: an exterior's ambient is the weather's DALC, in cellDalc (the same law as indoors)
uniform float cellSunScale;			// lane BOUNCE1: the imagespace Sunlight Scale outdoors (1 indoors; cellDirColor has it in)
// lane SUNCELL1: set by the caller before cellLit (exterior sun only): the cascades' light, the viewport ambient, the sun's specular (linear)
float cellDirVis = 1.0;
vec3 cellExtAmb = vec3( 0.0 );
vec3 cellExtSpec = vec3( 0.0 );
uniform vec3 cellDirColor;			// linear
uniform vec3 cellDirTo;				// world, TO the light
uniform bool cellInterior;
uniform vec3 cellCenter;
uniform int cellProbe;
// lane DECALLIT1: probe 6 written x this (WW_CELL_LIT_PROBE_GAIN, a harness probe only; 1 when the imagespace measures)
uniform float cellProbeGain;
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
// lane FARVIEW1 (src/farlight.h): distant light from the surfels, set by celllights.cpp
uniform bool cellFarOn;
uniform samplerBuffer cellFarSlots;	// a slot: x, y, z x 8 + side, record + 1 (0 empty); exact integers
uniform samplerBuffer cellFarRecs;	// 2 texels a record: pos, E (the shader's diff units)
uniform int cellFarBits;
uniform int cellFarProbe;			// the longest walk any record's key needs
uniform float cellFarCell;
uniform vec2 cellFarBand;			// D0, D1: w = smoothstep(D0, D1, the camera distance)
uniform int cellFarRed;				// WW_CELL_FAR_RED: 1 nosurfel, 2 noblend, 4 flipside
uniform bool cellFarPlacedOn;
uniform sampler3D cellFarPlaced;	// the GI grid relit by the placed lights alone: slot 0's six slabs
uniform vec3 cellFarPlacedOrigin;
uniform float cellFarPlacedVoxel;
uniform vec3 cellFarPlacedDims;
// lane BLOOM1b (src/gl/cellbloom.h): the FO4CS bloom, the tone map's final combine (cell_hdr.frag sets
// cellFoBloomHere to tent(the chain's top) x the bloom weight, exposed light, before it calls cellImageSpace)
#if defined( WW_CELL_HDR )
uniform bool cellIsFoBloomOn;
uniform sampler2D cellIsFoBloom;
uniform vec4 cellIsFoBloomParams;	// radius (pixels), bloom weight, dispersion, 0
uniform vec2 cellIsFoBloomFrame;	// the frame's size: the combine's uv
vec3 cellFoBloomHere = vec3( 0.0 );
#endif

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
#if defined( WW_CELL_HDR )
	if ( cellIsFoBloomOn )	// lane BLOOM1b: FO4CS adds its bloom to the exposed light, before the curve (x 2: the curve's input)
		x += 2.0 * cellFoBloomHere;
#endif
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
/* lane GISHADE1: what a surface sees of the grid. A room open to the street (a shop's doorway) is the outdoors' room,
 * so the room clamp keeps a voxel whose centre is across the shop's wall: the sunlit facade's light on the ceiling
 * along that wall (Concord's hardware store: 20-56x the brute-force reference). A voxel counts only when the fine
 * room grid shows no solid cell between the surface's view point and the voxel's centre, tapped every half cell, the
 * centre itself left out (a voxel's centre may sit in the wall it straddles). Red: WW_CELL_PV_RED=nosee. */
bool cellRoomSolid( vec3 q )
{
	vec3 c = floor( ( q - cellRoomsOrigin ) / cellRoomsCell );
	if ( any( lessThan( c, vec3( 0.0 ) ) ) || any( greaterThanEqual( c, cellRoomsDims ) ) )
		return false;
	return cellRoomUnpack( texelFetch( cellRooms, ivec3( c ), 0 ).r ).x < 0;
}
// the view point: the room read (P + N x 0.75 cell, else 1.75) when air, else the centre of the air cell beside
// either read nearest it (cellGiRoomSample's rule)
vec3 cellGiSeeFrom( vec3 P, vec3 N )
{
	for ( int k = 0; k < 2; k++ ) {
		vec3 q = P + N * ( ( k == 0 ? 0.75 : 1.75 ) * cellRoomsCell );
		if ( !cellRoomSolid( q ) )
			return q;
	}
	vec3 aN = abs( N );
	int m = ( aN.x >= aN.y && aN.x >= aN.z ) ? 0 : ( aN.y >= aN.z ? 1 : 2 );
	int ta = m == 0 ? 1 : 0, tb = m == 2 ? 1 : 2;
	float best = 3.0;
	vec3 r = P + N * ( 0.75 * cellRoomsCell );
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
				vec3 qq = q + e * cellRoomsCell;
				if ( cellRoomSolid( qq ) )
					continue;
				float d = ( sa > 0 ? 1.0 - f[ta] : sa < 0 ? f[ta] : 0.0 ) + ( sb > 0 ? 1.0 - f[tb] : sb < 0 ? f[tb] : 0.0 );
				if ( d < best ) {
					best = d;
					r = cellRoomsOrigin + ( floor( ( qq - cellRoomsOrigin ) / cellRoomsCell ) + 0.5 ) * cellRoomsCell;
				}
			}
		}
	}
	return r;
}
bool cellGiSeen( vec3 from, ivec3 c )
{
	if ( ( cellPassRed & 16 ) != 0 )
		return true;
	vec3 d = cellGiOrigin + ( vec3( c ) + 0.5 ) * cellGiVoxel - from;
	int nt = clamp( int( ceil( length( d ) / ( 0.5 * cellRoomsCell ) ) ), 1, 12 );
	for ( int j = 1; j <= nt; j++ )
		if ( cellRoomSolid( from + d * ( float( j ) / float( nt + 1 ) ) ) )
			return false;
	return true;
}
// lane ROOMCLAMP1: the blend of the 8 voxels' slots of room L, trilinear weights (sum in .rgba, weight in ws);
// lane GISHADE1: only the voxels seen from `from` (cellGiSeen)
vec4 cellGiRoomBlend( vec3 P, vec3 N, ivec2 L, vec3 from, out float ws )
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
		if ( base < 0 || w <= 0.0 || !cellGiSeen( from, c ) )
			continue;
		/* lane GIDARK1: the read cell holds two rooms (an opening's cell sees both) and so does the voxel: the mean of
		 * both slots that hold light. The first slot alone left Goodneighbor's recess2, beside a doorway, on the sealed
		 * room's dark. Red WW_CELL_PV_RED=firstslot. One loop for both slots: a second copy of the reads, inlined at
		 * every cellGiSample, took fo4_cell past what the driver links (the surfaces fell back to the legacy program). */
		bool two = ( cellPassRed & 256 ) == 0 && L.x >= 0 && L.y >= 0 && L.x != L.y && base == 0
			&& S.y >= 0 && ( S.y == L.x || S.y == L.y );
		vec4 v = vec4( 0.0 );
		float nv = 0.0;
		for ( int q = 0; q < ( two ? 2 : 1 ); q++ ) {
			int bq = q == 0 ? base : 6 * dm.z;
			vec4 u = n2.x * texelFetch( cellGi, ivec3( c.xy, c.z + bq + sl.x ), 0 )
			       + n2.y * texelFetch( cellGi, ivec3( c.xy, c.z + bq + sl.y ), 0 )
			       + n2.z * texelFetch( cellGi, ivec3( c.xy, c.z + bq + sl.z ), 0 );
			if ( u.a > 0.0 ) {
				v += u;
				nv += 1.0;
			}
		}
		if ( nv <= 0.0 )
			continue;   // lane ROOMCLAMP1: an empty slot (no probe of its room reached the voxel): no weight
		v /= nv;
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
// outdoorsOnly (lane GIBLACK1): the search borrows only an outdoors (0) cell (cellGiSample's lend rule)
vec4 cellGiRoomSample( vec3 P, vec3 N, vec3 from, bool outdoorsOnly, out ivec2 Ls )
{
	float ws;
	ivec2 L = cellRoomAt( P + N * ( 0.75 * cellRoomsCell ) );
	if ( L.x < 0 )
		L = cellRoomAt( P + N * ( 1.75 * cellRoomsCell ) );
	Ls = L;
	if ( L.x >= 0 && !( outdoorsOnly && L.x != 0 && L.y != 0 ) ) {	// lane GIBLACK1: lent, an indoors read is not taken
		vec4 s = cellGiRoomBlend( P, N, L, from, ws );
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
				if ( d >= best || ( outdoorsOnly && c.x != 0 && c.y != 0 ) )
					continue;
				vec4 s = cellGiRoomBlend( P, N, c, from, ws );
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
const ivec2 cellGiAnyRoom = ivec2( -2 );	// lane WALLSTACK1: cellGiFillAt's L for the last fill (every voxel's first slot, as the trilinear reads)
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
		if ( cellGiRooms && L != cellGiAnyRoom ) {
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

/* lane GISHADE1: no room's blend has weight: the voxels of the 4x4x4 block the surface sees (any room), weight
 * (1 - d / 2.5)^2 in voxels, before the plain trilinear (which sees through walls). .a = 0: none seen. */
vec4 cellGiSeenFill( vec3 P, vec3 N, vec3 from )
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
		if ( w <= 0.0 || !cellGiSeen( from, c ) )
			continue;
		vec4 v = n2.x * texelFetch( cellGi, ivec3( c.xy, c.z + sl.x ), 0 )
		       + n2.y * texelFetch( cellGi, ivec3( c.xy, c.z + sl.y ), 0 )
		       + n2.z * texelFetch( cellGi, ivec3( c.xy, c.z + sl.z ), 0 );
		if ( v.a <= 0.0 )
			continue;
		w *= w;
		s += w * v;
		ws += w;
	}
	return ws > 0.0 ? s / ws : vec4( 0.0 );
}

/* lane GIBLACK2: the stranded surface's evidence. Outdoors (a sky-lit grid; also an interior that shows the sky), a
 * surface whose own reads are both solid (cellGiSample's lend rule) sits in air the room grid could not resolve. The
 * outdoors is no safe guess for it: a building's inside in an exterior cell (a shop, the Memory Den's rooms, Concord's
 * hardware store) drew the sky's light where the tracer gives none (Goodneighbor: 25 of 42 such indoor spots drew the
 * open sky). It searches the air cells at P + N x (0.75, 1.75, 2.75) cell, each with up to 2 cells round it in the
 * plane of the two axes off the normal's largest (75 cells, those whose centre is in front of the surface), and keeps
 * the nearest indoor and the nearest outdoors (room 0) one by the distance from P to the cell's box. */
void cellGiStrandSearch( vec3 P, vec3 N, out float dIn, out ivec2 LIn, out vec3 CIn, out float dOut, out ivec2 LOut, out vec3 COut )
{
	dIn = 1e9;
	dOut = 1e9;
	LIn = ivec2( -1 );
	LOut = ivec2( -1 );
	CIn = P;
	COut = P;
	vec3 aN = abs( N );
	int m = ( aN.x >= aN.y && aN.x >= aN.z ) ? 0 : ( aN.y >= aN.z ? 1 : 2 );
	int ta = m == 0 ? 1 : 0, tb = m == 2 ? 1 : 2;
	for ( int t = 0; t < 3; t++ ) {
		vec3 q0 = P + N * ( ( float( t ) + 0.75 ) * cellRoomsCell );
		for ( int sa = -2; sa <= 2; sa++ ) {
			for ( int sb = -2; sb <= 2; sb++ ) {
				vec3 e = vec3( 0.0 );
				e[ta] = float( sa );
				e[tb] = float( sb );
				vec3 q = q0 + e * cellRoomsCell;
				ivec2 L = cellRoomAt( q );
				if ( L.x < 0 )
					continue;
				vec3 lo = cellRoomsOrigin + floor( ( q - cellRoomsOrigin ) / cellRoomsCell ) * cellRoomsCell;
				vec3 c = lo + 0.5 * cellRoomsCell;
				if ( dot( c - P, N ) <= 0.0 )
					continue;
				float d = length( max( max( lo - P, P - lo - cellRoomsCell ), vec3( 0.0 ) ) );
				if ( L.x == 0 || L.y == 0 ) {
					if ( d < dOut ) {
						dOut = d;
						LOut = L;
						COut = c;
					}
				} else if ( d < dIn ) {
					dIn = d;
					LIn = L;
					CIn = c;
				}
			}
		}
	}
}
/* lane GIBLACK2: a stranded surface's answer from room L seen from the air cell's centre C: its blend (path 6),
 * indoors the room's own fill (7), the seen fill from C (8), indoors with no light in reach the sealed room's dark
 * (9: sky 0, the ambient replaced; the tracer gives such spots 0 to 0.005 of the sky, the trilinear up to 0.9);
 * outdoors with nothing: .a < 0 (the plain trilinear). */
vec4 cellGiStrandAnswer( vec3 P, vec3 N, ivec2 L, vec3 C, out int path )
{
	bool indoor = L.x != 0 && L.y != 0;
	float ws;
	vec4 s = cellGiRoomBlend( P, N, L, C, ws );
	path = 6;
	if ( ws > 0.0 )
		return s / ws;
	if ( indoor ) {
		path = 7;
		vec4 f = cellGiFillAt( P, N, L );
		if ( f.a > 0.01 )
			return f;
	}
	path = 8;
	if ( ( cellPassRed & 16 ) == 0 ) {
		vec4 f = cellGiSeenFill( P, N, C );
		if ( f.a > 0.01 )
			return f;
	}
	path = indoor ? 9 : 5;
	return indoor ? vec4( 0.0, 0.0, 0.0, 1.0 ) : vec4( -1.0 );
}
// the hardware trilinear of the one grid (slot 0): the bake's own light at P, walls not seen
vec4 cellGiTrilinear( vec3 P, vec3 N )
{
	vec3 g = ( P + N * ( 0.5 * cellGiVoxel ) - cellGiOrigin ) / cellGiVoxel;
	vec2 xy = g.xy / cellGiDims.xy;
	float z = clamp( g.z, 0.5, cellGiDims.z - 0.5 );
	float depth = ( cellGiRooms ? 12.0 : 6.0 ) * cellGiDims.z;
	vec3 n2 = N * N;
	return n2.x * texture( cellGi, vec3( xy, ( z + ( N.x >= 0.0 ? 0.0 : 1.0 ) * cellGiDims.z ) / depth ) )
	     + n2.y * texture( cellGi, vec3( xy, ( z + ( N.y >= 0.0 ? 2.0 : 3.0 ) * cellGiDims.z ) / depth ) )
	     + n2.z * texture( cellGi, vec3( xy, ( z + ( N.z >= 0.0 ? 4.0 : 5.0 ) * cellGiDims.z ) / depth ) );
}
/* lane GIBLACK2: every reached cell indoors: the indoor room; only the outdoors: room 0; both: the nearer, and when
 * they lie within a cell of each other (the room grid cannot tell which side of a thin wall the surface is on: the
 * Memory Den recess's corner, the building's room a cell to the side) the answer whose light is nearer the surface's
 * own baked light (the plain trilinear). Nothing reached: .a < 0 (the plain trilinear). */
vec4 cellGiStrandPick( vec3 P, vec3 N, out int path )
{
	float dIn, dOut;
	ivec2 LIn, LOut;
	vec3 CIn, COut;
	cellGiStrandSearch( P, N, dIn, LIn, CIn, dOut, LOut, COut );
	path = 5;
	if ( LIn.x < 0 && LOut.x < 0 )
		return vec4( -1.0 );
	/* lane GIDARK1: buried three cells deep (the reads at 0.75, 1.75 and 2.75 cells all solid) with only the outdoors in
	 * reach, the search may have gone round a building's corner (Concord Hardware's front pillar took the street's
	 * light, 6.5x the tracer's 0.001-0.007): the darker of the outdoor answer and the plain trilinear (the bake's own
	 * visibility there). The sealed dark instead blacked out Goodneighbor's recess walls. Red WW_CELL_PV_RED=lonesky.
	 * A tie (both within a cell) is the room grid saying it cannot tell the two sides apart, and the plain trilinear
	 * reads the voxels' first slot only (beside a doorway the indoor room: Goodneighbor's recess2 took the sealed room's
	 * dark, 0.0001 where the tracer gives 0.007-0.01): the mean of both answers. Red WW_CELL_PV_RED=tiepick (GIBLACK2's
	 * pick, the answer nearer the trilinear). Two answer sites and one trilinear site: every one is inlined at each
	 * cellGiSample, and more took fo4_cell past what the driver links. */
	const vec3 Y = vec3( 0.2126, 0.7152, 0.0722 );
	bool lone = LIn.x < 0 && ( cellPassRed & 1024 ) == 0 && cellRoomSolid( P + N * ( 0.75 * cellRoomsCell ) )
		&& cellRoomSolid( P + N * ( 1.75 * cellRoomsCell ) ) && cellRoomSolid( P + N * ( 2.75 * cellRoomsCell ) );
	bool tie = LIn.x >= 0 && LOut.x >= 0 && abs( dIn - dOut ) < cellRoomsCell;
	bool wantIn = LIn.x >= 0 && ( tie || LOut.x < 0 || dIn < dOut );
	bool wantOut = LOut.x >= 0 && ( tie || !wantIn );
	int pi = 5, po = 5;
	vec4 a = vec4( -1.0 ), o = vec4( -1.0 );
	if ( wantIn )
		a = cellGiStrandAnswer( P, N, LIn, CIn, pi );
	if ( wantOut )
		o = cellGiStrandAnswer( P, N, LOut, COut, po );
	bool needT = lone || ( tie && o.a >= 0.0 && ( cellPassRed & 512 ) != 0 );
	vec4 tr = needT ? cellGiTrilinear( P, N ) : vec4( 0.0 );
	if ( lone ) {
		if ( o.a >= 0.0 && tr.a > 0.0 && dot( tr.rgb, Y ) / tr.a < dot( o.rgb, Y ) / max( o.a, 1e-6 ) ) {
			path = 4;
			return tr;
		}
		path = po;
		return o;
	}
	if ( !tie || o.a < 0.0 ) {
		path = wantIn ? pi : po;
		return wantIn ? a : o;
	}
	if ( ( cellPassRed & 512 ) == 0 ) {
		path = ( pi == 6 || po == 6 ) ? 6 : pi;
		return 0.5 * ( a + o );
	}
	float t = dot( tr.rgb, Y );
	bool useIn = abs( dot( a.rgb, Y ) - t ) <= abs( dot( o.rgb, Y ) - t );
	path = useIn ? pi : po;
	return useIn ? a : o;
}

int cellGiPath = 0;	// lane GIBLACK1: probe 95, the branch cellGiSample took (1 room blend, 2 room fill, 3 seen fill, 4 trilinear, 5 stranded: nothing reached, the trilinear; lane GIBLACK2: stranded 6 room blend, 7 room fill, 8 seen fill, 9 sealed dark; lane WALLSTACK1: 10 the last fill)
vec4 cellGiSampleFull( vec3 P, vec3 N )
{
	bool fill = cellGiFill && cellInterior && !cellGiSky;	// interiors only: an exterior's gap keeps the weather's ambient (cellGiSkyK), also under the keepamb red
	ivec2 Ls = ivec2( -1 );
	if ( cellGiRooms && ( cellPassRed & 8 ) == 0 ) {	// lane ROOMCLAMP1 (red noclamp: WW_CELL_PV_RED)
		vec3 from = cellGiSeeFrom( P, N );
		/* lane GIBLACK1: the lend rule. Outdoors (a sky-lit grid), a surface whose own reads (P + N x 0.75 and 1.75 cell)
		 * are both solid sits in air the room grid could not resolve (a recess narrower than two cells: every cell a
		 * triangle touches is solid). The air cell its search then borrows can be across the wall: a sealed pocket
		 * (a shaft, a building's hollow) whose probes see 1e-5 of the street's light. Its voxels are valid (a = 1),
		 * so k = 1 and the weather's ambient was replaced by black (the Memory Den recess). Such a surface borrows
		 * only the outdoors (room 0): its neighbour search keeps outdoors cells, its seen fill runs only from an
		 * outdoors view point, and with neither it takes the plain trilinear (lane GIBLACK2; it was a = 0, the weather's
		 * flat ambient). The same when the 0.75 read is solid and the 1.75 read lands in an indoors room (a normal turned
		 * a little toward the pocket reads it straight through the unresolved air: the strip at the recess's corner),
		 * and that indoors read is not taken either. Red WW_CELL_PV_RED=borrow: the old search. */
		ivec2 L2 = cellRoomAt( P + N * ( 1.75 * cellRoomsCell ) );
		bool lend = cellGiSky && ( cellPassRed & 32 ) == 0 && cellRoomSolid( P + N * ( 0.75 * cellRoomsCell ) )
		         && ( L2.x < 0 || ( L2.x != 0 && L2.y != 0 ) );
		/* lane GIBLACK2: such a stranded surface takes the room its evidence search reaches (cellGiStrandPick), indoors
		 * or out, never a guess at the outdoors; nothing reached: the plain trilinear (path 5), never the weather's flat
		 * ambient. Red WW_CELL_PV_RED=strand: GIBLACK1's law (the outdoors-only borrow below, then the flat ambient). */
		if ( lend && ( cellPassRed & 128 ) == 0 ) {
			int sp;
			vec4 sr = cellGiStrandPick( P, N, sp );
			cellGiPath = sp;
			if ( sr.a >= 0.0 )
				return sr;
			if ( ( cellPassRed & 64 ) != 0 )
				return vec4( 0.0 );	// red WW_CELL_PV_RED=lendamb: the flat ambient
			return cellGiTrilinear( P, N );
		}
		vec4 r = cellGiRoomSample( P, N, from, lend, Ls );
		cellGiPath = 1;
		if ( r.a > 0.01 || ( r.a >= 0.0 && !fill ) )
			return r;
		cellGiPath = 2;
		if ( fill ) {	// lane GICAL1: a gap, or no room with weight (the plain trilinear ignores walls)
			vec4 f = cellGiFillAt( P, N, Ls );
			if ( f.a > 0.01 || r.a >= 0.0 )
				return f.a > 0.01 ? f : r;
		}
		cellGiPath = 3;
		ivec2 Lf = cellRoomAt( from );
		if ( ( cellPassRed & 16 ) == 0 && ( !lend || Lf.x == 0 || Lf.y == 0 ) ) {
			vec4 f = cellGiSeenFill( P, N, from );
			if ( f.a > 0.01 )
				return f;
		}
		/* lane GIBLACK2: a lent surface with nothing outdoors to borrow takes the plain trilinear (probe 95 path 5), never the
		 * weather's flat ambient: that is the open sky's light, and such a surface sits in a recess (the tracer gives it
		 * 0 to 0.07 of the sky where the flat ambient drew 1). The room grid now resolves the recess (clipped to the GI
		 * grid's box), so few surfaces get here; the trilinear blends the probes around them, neither black nor open sky. Red WW_CELL_PV_RED=lendamb. */
		cellGiPath = lend ? 5 : 4;
		if ( lend && ( cellPassRed & ( 64 | 128 ) ) != 0 )
			return vec4( 0.0 );	// reds WW_CELL_PV_RED=lendamb, strand: GIBLACK1's flat ambient
	} else
		cellGiPath = 4;
	vec4 s = cellGiTrilinear( P, N );
	/* lane GICAL1: a gap in a grid without rooms. Lane WALLSTACK1: with rooms too, the last fill (probe 95 path 10). A surface
	 * whose room no probe reached (a pocket: Vault 111's 30-cell room 65 by the cryo pods) and whose seen fill finds no
	 * lit voxel (its view line to the one lit voxel grazes a wall cell, sub-pixel by sub-pixel) gets here; the plain
	 * trilinear then sits on a layer of empty voxels inside the wall (a 0.0009) and the surface drew black: the coverage
	 * gate's judge A pixel. The last fill is the trilinear's own reach and rule (every voxel's first slot, through walls),
	 * weighted over the valid voxels only. Red WW_CELL_PV_RED=nolastfill: the plain trilinear's black. One call site:
	 * the program's size (see cellPassOut). */
	if ( fill && s.a <= 0.01 && ( !cellGiRooms || ( cellPassRed & ( 8 | 4096 ) ) == 0 ) ) {	// the noclamp red keeps its old tail
		vec4 f = cellGiFillAt( P, N, cellGiRooms ? cellGiAnyRoom : Ls );
		if ( f.a > 0.01 ) {
			if ( cellGiRooms )
				cellGiPath = 10;
			return f;
		}
	}
	return s;
}

// a unit normal as one float: octahedral, 11 bits a component, exact in a float's mantissa
float cellGiKeyPack( vec3 n )
{
	vec2 o = n.xy / ( abs( n.x ) + abs( n.y ) + abs( n.z ) );
	if ( n.z < 0.0 )
		o = ( 1.0 - abs( o.yx ) ) * vec2( o.x >= 0.0 ? 1.0 : -1.0, o.y >= 0.0 ? 1.0 : -1.0 );
	vec2 q = floor( clamp( o * 0.5 + 0.5, 0.0, 1.0 ) * 2047.0 + 0.5 );
	return q.x * 2048.0 + q.y;
}

vec3 cellGiKeyNormal( float k )
{
	vec2 q = vec2( floor( k / 2048.0 ), mod( k, 2048.0 ) );
	vec2 o = q / 2047.0 * 2.0 - 1.0;
	vec3 n = vec3( o, 1.0 - abs( o.x ) - abs( o.y ) );
	if ( n.z < 0.0 )
		n.xy = ( 1.0 - abs( n.yx ) ) * vec2( n.x >= 0.0 ? 1.0 : -1.0, n.y >= 0.0 ? 1.0 : -1.0 );
	return normalize( n );
}

/* lane PERF1 (the GI volumes, the Division deck's D4): the leak guard is the volume a pixel stands in, not a march.
 * The outdoors is one volume (room 0); every interior room is a volume of its own, drawn as primitive shapes into a
 * screen mask (src/gl/celllights.cpp giVolPass, cell_givol.frag). A pixel reads only the grid voxels (slots) of its own
 * volume: the 8 voxels round P + N x half a voxel, trilinear, renormalized. Where a threshold joins two volumes the
 * voxels near it carry its weight t and the other side's room (baked, src/probegi.cpp probeGiRoomsInto), and the pixel
 * blends the other side's read in by t / 2: both ways, so light passes in and out through thresholds only.
 *   cellGiVolume: one copy of the read, loops kept as loops (the bound is a uniform), so a call site costs little.
 *   Its volume: the screen mask (the full-screen GI pass, cell_gihalf.frag), else the fine room grid at P + N x 0.75
 *   cell, else 1.75 cell (blended and effect draws, the probe passes); neither: every voxel's first slot.
 * Red WW_CELL_GI_VIS=march compiles WW_CELL_GI_MARCH: today's chain (cellGiSampleFull), every call, as before PERF1.
 * Probe 95 paths, in lane GIBLACK2's words (tests/spells/cell_giblack_check.py): 1 the volume's own voxels (a room's
 * blend; also with a threshold's), 2 the nearest own-volume voxel (a room's fill), 4 no volume known or none of its
 * voxels in reach (the trilinear); outdoors, a surface the room grid cannot place (lane GIBLACK2's stranded): 6 and 7
 * the outdoors' volume, 5 none in reach. A probe picture reads the mask at its own pixel (the screen pass's read). */
#ifdef WW_CELL_GI_MARCH
vec4 cellGiSample( vec3 P, vec3 N )
{
	return cellGiSampleFull( P, N );
}
#else
uniform int cellGiLoop8;		// 8: the corner loop's bound, a uniform so the loop stays one body
uniform bool cellGiHalfOn;
uniform sampler2D cellGiHalf;		// rgba: the volume read at the screen texel's pixel
uniform sampler2D cellGiHalfKey;	// xyz: that pixel's world position, w: its packed normal (< 0: no surface)
uniform vec4 cellGiHalfRect;		// the viewport's x, y; the screen target's size
uniform float cellGiHalfScale;		// 0.5 half resolution, 1 full

// the volume of a point with no mask: the fine room grid's two names at P + N x 0.75 cell, else 1.75; -2: unknown
ivec2 cellGiVolAt( vec3 P, vec3 N )
{
	if ( !cellGiRooms )
		return ivec2( -2 );
	for ( int k = 0; k < 2; k++ ) {
		ivec2 L = cellRoomAt( P + N * ( ( k == 0 ? 0.75 : 1.75 ) * cellRoomsCell ) );
		if ( L.x >= 0 )
			return L;
	}
	return ivec2( -2 );
}

vec4 cellGiVolume( vec3 P, vec3 N, ivec2 L )
{
	vec3 g = ( P + N * ( 0.5 * cellGiVoxel ) - cellGiOrigin ) / cellGiVoxel - 0.5;
	ivec3 i0 = ivec3( floor( g ) );
	vec3 f = g - vec3( i0 );
	ivec3 r0 = ivec3( floor( g + 0.5 ) );
	ivec3 dm = ivec3( cellGiDims );
	ivec3 sl = ivec3( N.x >= 0.0 ? 0 : 1, N.y >= 0.0 ? 2 : 3, N.z >= 0.0 ? 4 : 5 ) * dm.z;
	vec3 n2 = N * N;
	/* outdoors (a sky-lit grid), a surface in no interior volume whose own reads (0.75, 1.75 cell) are solid stands in the
	 * outdoors' volume (room 0), as the march's stranded pick takes it (probe 95: 6, 7); buried a third cell deep, the darker
	 * of that and the trilinear (lane GIDARK1's guard: a pillar beside a shop front took the street's light) */
	bool strand = cellGiRooms && cellGiSky && L.x == -2;
	if ( strand )
		L = ivec2( 0, -1 );
	bool anyRoom = !cellGiRooms || L.x == -2;
	vec4 own = vec4( 0.0 ), oth = vec4( 0.0 );
	float wOwn = 0.0, wOth = 0.0, tAcc = 0.0, tBest = 0.0;
	int other = -1;
	bool nearPick = false;
	// pass 0: the 8 corners, own volume (and the thresholds' weight); 1: none had it, the 27 round the nearest corner;
	// 2: the threshold's other side, the 8 corners
	for ( int ps = 0; ps < cellGiLoop8 - 5; ps++ ) {	// 3 passes; the bound a uniform: one body, never unrolled
		if ( ps == 1 && wOwn > 0.0 )
			continue;
		if ( ps == 2 && ( other < 0 || tAcc <= 0.0 || wOwn <= 0.0 ) )
			break;
		int n = ps == 1 ? 3 * cellGiLoop8 + 3 : cellGiLoop8;	// 27, 8
		ivec2 R = ps == 2 ? ivec2( other ) : L;
		for ( int k = 0; k < n; k++ ) {
			ivec3 c;
			float w;
			if ( ps == 1 ) {
				c = r0 + ivec3( k % 3, ( k / 3 ) % 3, k / 9 ) - 1;
				vec3 d = vec3( c ) - g;
				w = 1.0 / ( 1.0 + dot( d, d ) );
			} else {
				ivec3 o = ivec3( k & 1, ( k >> 1 ) & 1, ( k >> 2 ) & 1 );
				c = i0 + o;
				vec3 t = mix( 1.0 - f, f, vec3( o ) );
				w = t.x * t.y * t.z;
			}
			if ( w <= 0.0 || any( lessThan( c, ivec3( 0 ) ) ) || any( greaterThanEqual( c, dm ) ) )
				continue;
			vec4 S4 = cellGiRooms ? texelFetch( cellGiSlots, c, 0 ) : vec4( 0.0 );
			ivec2 S = cellRoomUnpack( S4.r );
			int q = ( anyRoom && ps < 2 ) ? 0
			      : ( S.x >= 0 && ( S.x == R.x || S.x == R.y ) ) ? 0 : ( S.y >= 0 && ( S.y == R.x || S.y == R.y ) ) ? 1 : -1;
			if ( q < 0 )
				continue;
			int bq = q * 6 * dm.z;
			vec4 u = n2.x * texelFetch( cellGi, ivec3( c.xy, c.z + bq + sl.x ), 0 )
			       + n2.y * texelFetch( cellGi, ivec3( c.xy, c.z + bq + sl.y ), 0 )
			       + n2.z * texelFetch( cellGi, ivec3( c.xy, c.z + bq + sl.z ), 0 );
			if ( u.a <= 0.0 )
				continue;	// an empty slot (no probe of its volume reached the voxel): no weight
			if ( ps == 2 ) {
				oth += w * u;
				wOth += w;
				continue;
			}
			own += w * u;
			wOwn += w;
			nearPick = nearPick || ps == 1;
			if ( ps == 0 && !anyRoom ) {	// the threshold this slot sits by: its weight, and the room across it
				float t = q == 0 ? S4.b : S4.a;
				ivec2 O = cellRoomUnpack( S4.g );
				int oq = q == 0 ? O.x : O.y;
				tAcc += w * t;
				if ( oq >= 0 && t > tBest ) {
					tBest = t;
					other = oq;
				}
			}
		}
	}
	vec4 s = wOwn > 0.0 ? own / wOwn : vec4( 0.0 );
	if ( wOth > 0.0 )
		s = mix( s, oth / wOth, 0.5 * clamp( tAcc / wOwn, 0.0, 1.0 ) );
	cellGiPath = anyRoom ? 4 : ( nearPick ? 2 : 1 ) + ( strand ? 5 : 0 );
	bool lone = strand && wOwn > 0.0 && cellRoomSolid( P + N * ( 2.75 * cellRoomsCell ) );
	if ( wOwn <= 0.0 || lone ) {	// one trilinear site: each cellGiSample inlines it
		const vec3 Y = vec3( 0.2126, 0.7152, 0.0722 );
		vec4 tr = cellGiTrilinear( P, N );
		if ( wOwn <= 0.0 || ( tr.a > 0.0 && dot( tr.rgb, Y ) / tr.a < dot( s.rgb, Y ) / max( s.a, 1e-6 ) ) ) {
			cellGiPath = strand && wOwn <= 0.0 ? 5 : 4;
			return tr;
		}
	}
	return s;
}

// the screen GI's upsample: the 4 screen texels round this pixel; false: none is this surface
bool cellGiHalfAt( vec3 P, vec3 N, out vec4 s )
{
	s = vec4( 0.0 );
	vec2 u = ( gl_FragCoord.xy - cellGiHalfRect.xy - 0.5 ) * cellGiHalfScale;	// texel h sits at pixel h / scale
	ivec2 h0 = ivec2( floor( u ) );
	vec2 t = u - vec2( h0 );
	ivec2 hs = ivec2( cellGiHalfRect.zw );
	float tol = 1.0 + 0.01 * distance( P, vec3( cellRow[0].w, cellRow[1].w, cellRow[2].w ) );	// game units
	vec4 acc = vec4( 0.0 );
	float wsum = 0.0;
	for ( int k = 0; k < 4; k++ ) {
		ivec2 o = ivec2( k & 1, k >> 1 );
		float wb = ( o.x == 1 ? t.x : 1.0 - t.x ) * ( o.y == 1 ? t.y : 1.0 - t.y );
		if ( wb <= 0.0 )
			continue;
		ivec2 h = clamp( h0 + o, ivec2( 0 ), hs - 1 );
		vec4 key = texelFetch( cellGiHalfKey, h, 0 );
		if ( key.w < 0.0 )
			continue;
		float w = wb * smoothstep( 0.7, 0.95, dot( N, cellGiKeyNormal( key.w ) ) )
		        * max( 1.0 - abs( dot( N, key.xyz - P ) ) / tol, 0.0 );
		if ( w <= 0.0 )
			continue;
		acc += w * texelFetch( cellGiHalf, h, 0 );
		wsum += w;
	}
	if ( wsum < 1e-3 )
		return false;
	s = acc / wsum;
	return true;
}

vec4 cellGiSample( vec3 P, vec3 N )
{
	vec4 s;
	float m = 0.0;
	if ( cellGiHalfOn && cellPass == 0 ) {
		if ( cellProbe == 0 && cellGiHalfAt( P, N, s ) ) {
			cellGiPath = 11;
			return s;
		}
		if ( cellProbe != 0 )	// a probe picture: cellGiHalf holds the volumes' mask (src/gl/celllights.cpp)
			m = texelFetch( cellGiHalf, ivec2( gl_FragCoord.xy - cellGiHalfRect.xy ), 0 ).r;
	}
	return cellGiVolume( P, N, m > 0.5 ? ivec2( int( m + 0.5 ) - 1, -1 ) : cellGiVolAt( P, N ) );
}
#endif

// lane AODECAL1 (src/gl/cellaodecalgl.h): the baked AO decals of the big movable statics, one texel per pixel,
// multiplied into the probe term only (the grid was rebuilt without those copies' blocked sky)
uniform bool cellAoDecalOn;
uniform sampler2D cellAoDecal;
uniform vec4 cellAoDecalRect;		// the viewport's x, y
uniform float cellSkyVis;		// lane SKYSTR1: the Sky visibility row, 0..1 (src/gl/celllights.h)
float cellAoDecalAt()
{
	return texelFetch( cellAoDecal, ivec2( gl_FragCoord.xy - cellAoDecalRect.xy ), 0 ).r;
}

vec3 cellGiE0( vec3 P, vec3 N )
{
	vec4 s = cellGiSample( P, N );
	// lane SKY1: with the sky in the grid the sample keeps its valid share (E x share), the other
	// ( 1 - share ) being the weather's own ambient the program leaves in (cellGiSkyK)
	if ( cellGiSky )
		return max( s.rgb, vec3( 0.0 ) );
	return s.a > 0.01 ? max( s.rgb / s.a, vec3( 0.0 ) ) : vec3( 0.0 );
}

vec3 cellGiE( vec3 P, vec3 N )
{
	vec3 e = cellGiE0( P, N );
	if ( !( cellAoDecalOn && cellPass < 2 ) )
		return e;
	float d = cellAoDecalAt();
	// lane SKYSTR1: outdoors the decal is blocked sky; the Sky visibility row opens it as the relight opens the grid's
	if ( cellGiSky && cellSkyVis < 1.0 )
		d = 1.0 + cellSkyVis * ( d - 1.0 );
	return e * d;
}
// lane SKY1: the share of the weather's ambient the grid's sky replaces at P (0: not a sky grid)
float cellGiSkyK( vec3 P, vec3 N )
{
	if ( !cellGiOn || !cellGiSky || cellInterior )
		return 0.0;
	return clamp( cellGiSample( P, N ).a, 0.0, 1.0 );
}
/* lane GIAMB1: probe 90 indoors, the share of the cell's ambient (DALC) the probe light replaced at P, under the
 * rule in force (cellAmbGi): the grid's valid share with replace (the default), 0 with keep or without the GI */
float cellAmbShare( vec3 P, vec3 N )
{
	if ( !cellInterior )
		return cellGiSkyK( P, N );
	if ( !cellGiOn || !cellHasDalc || cellGiAmb != 1 )
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

/* lane FARVIEW1: the far blend at world point P, 0 near, 1 far (red noblend: a hard switch at the band's middle) */
float cellFarW( vec3 P )
{
	if ( !cellFarOn )
		return 0.0;
	float d = length( P - vec3( cellRow[0].w, cellRow[1].w, cellRow[2].w ) );
	if ( ( cellFarRed & 2 ) != 0 )
		return step( 0.5 * ( cellFarBand.x + cellFarBand.y ), d );
	return smoothstep( cellFarBand.x, cellFarBand.y, d );
}

// the hash farLightHash (src/farlight.h) uses: an int's bits as uint, products wrap
uint cellFarHash( ivec3 k, int side )
{
	return ( uint( k.x ) * 73856093u ) ^ ( uint( k.y ) * 19349663u ) ^ ( uint( k.z ) * 83492791u ) ^ ( uint( side ) * 2654435761u );
}

int cellFarIters = 0;	// probe 94: table slots walked
/* F: the lit surfels on P's side in the 27 cells round P's key cell, w = (1 - d^2 / R^2)^2, R = 1.5 cells
 * (FARVIEW1_DESIGN.md section 3; the bake's sideOf and keyOf). Red flipside: the opposite side's. */
vec3 cellFarF( vec3 P, vec3 N )
{
	vec3 a = abs( N );
	int ax = 0;
	if ( a.y > a.x )
		ax = 1;
	if ( a.z > a[ax] )
		ax = 2;
	int side = 2 * ax + ( N[ax] >= 0.0 ? 0 : 1 );
	vec3 nudge = vec3( 0.0 );
	nudge[ax] = ( side & 1 ) != 0 ? -0.5 : 0.5;
	ivec3 k0 = ivec3( floor( ( P - nudge ) / cellFarCell ) );
	if ( ( cellFarRed & 4 ) != 0 )
		side ^= 1;
	uint mask = ( 1u << uint( cellFarBits ) ) - 1u;
	float R2 = 2.25 * cellFarCell * cellFarCell;
	vec3 s = vec3( 0.0 );
	float ws = 0.0;
	for ( int dz = -1; dz <= 1; dz++ )
	for ( int dy = -1; dy <= 1; dy++ )
	for ( int dx = -1; dx <= 1; dx++ ) {
		ivec3 k = k0 + ivec3( dx, dy, dz );
		uint h = cellFarHash( k, side ) & mask;
		vec3 key = vec3( float( k.x ), float( k.y ), float( k.z * 8 + side ) );
		for ( int j = 0; j < cellFarProbe && j < 64; j++ ) {
			cellFarIters++;
			vec4 t = texelFetch( cellFarSlots, int( h ) );
			if ( t.w == 0.0 )
				break;
			if ( t.xyz == key ) {
				int r = int( t.w ) - 1;
				vec3 d = P - texelFetch( cellFarRecs, r * 2 ).xyz;
				float q = 1.0 - dot( d, d ) / R2;
				if ( q > 0.0 ) {
					q *= q;
					s += q * texelFetch( cellFarRecs, r * 2 + 1 ).rgb;
					ws += q;
				}
			}
			h = ( h + 1u ) & mask;
		}
	}
	return ws > 0.0 ? s / ws : vec3( 0.0 );
}

// the GI grid's placed share at P (its E, read as cellGiE reads the grid: the sky grid's rgb carries its share)
vec3 cellFarPlacedE( vec3 P, vec3 N )
{
	if ( !cellFarPlacedOn )
		return vec3( 0.0 );
	vec3 g = ( P + N * ( 0.5 * cellFarPlacedVoxel ) - cellFarPlacedOrigin ) / cellFarPlacedVoxel;
	vec2 xy = g.xy / cellFarPlacedDims.xy;
	float z = clamp( g.z, 0.5, cellFarPlacedDims.z - 0.5 );
	float depth = 6.0 * cellFarPlacedDims.z;
	vec3 n2 = N * N;
	vec4 s = n2.x * texture( cellFarPlaced, vec3( xy, ( z + ( N.x >= 0.0 ? 0.0 : 1.0 ) * cellFarPlacedDims.z ) / depth ) )
	       + n2.y * texture( cellFarPlaced, vec3( xy, ( z + ( N.y >= 0.0 ? 2.0 : 3.0 ) * cellFarPlacedDims.z ) / depth ) )
	       + n2.z * texture( cellFarPlaced, vec3( xy, ( z + ( N.z >= 0.0 ? 4.0 : 5.0 ) * cellFarPlacedDims.z ) / depth ) );
	if ( cellGiSky )
		return max( s.rgb, vec3( 0.0 ) );
	return s.a > 0.01 ? max( s.rgb / s.a, vec3( 0.0 ) ) : vec3( 0.0 );
}

/* the far terms at P: returns w; F (red nosurfel: 0) and the GI's placed share / pi (gP, the caller takes w x gP
 * out of its GI). The caller's placed diffuse: (1 - w) x real (cellSumLights fades it) + w x F. */
float cellFarTerms( vec3 P, vec3 N, out vec3 F, out vec3 gP )
{
	F = vec3( 0.0 );
	gP = vec3( 0.0 );
	float w = cellFarW( P );
	if ( w <= 0.0 )
		return 0.0;
	if ( ( cellFarRed & 1 ) == 0 )
		F = cellFarF( P, N );
	gP = cellFarPlacedE( P, N ) * 0.31830989;
	return w;
}

/* the far gate's probes, sqrt( clamp( E / 4 ) ) so a dim far town keeps its bits: 91 F, 92 the real placed
 * irradiance (Lambert, unfaded) + the GI's placed share / pi, 93 w, 94 (lights the shading loops + table slots
 * walked) / 256 */
vec3 cellFarProbeOut( vec3 P, vec3 N )
{
	float w = cellFarW( P );
	if ( cellProbe == 93 )
		return vec3( w );
	if ( cellProbe == 91 )
		return sqrt( clamp( ( cellFarOn ? cellFarF( P, N ) : vec3( 0.0 ) ) * 0.25, 0.0, 1.0 ) );
	if ( cellProbe == 94 ) {
		if ( w > 0.0 && ( cellFarRed & 1 ) == 0 )
			cellFarF( P, N );
		return vec3( float( ( w >= 1.0 ? 0 : cellLightCount ) + cellFarIters ) / 256.0 );
	}
	vec3 E = vec3( 0.0 );
	for ( int i = 0; i < cellLightCount; i++ ) {
		vec3 L;
		bool ns;
		vec3 c = cellLightE( i, P, N, L, ns );
		E += c * max( dot( N, L ), 0.0 );
	}
	E += cellFarPlacedE( P, N ) * 0.31830989;
	return sqrt( clamp( E * 0.25, 0.0, 1.0 ) );
}

// the harness probes for a program without the legacy BRDF helpers (pbrm_cell)
vec3 cellProbeRaw( vec3 P, vec3 N )
{
	if ( cellProbe >= 91 && cellProbe <= 94 )	// lane FARVIEW1
		return cellFarProbeOut( P, N );
	if ( cellProbe >= 70 && cellProbe <= 74 )	// lane FXLIT1's probes: the lit effects alone, a surface writes black
		return vec3( 0.0 );
	if ( cellProbe == 5 )
		return cellGiOn ? clamp( cellGiE( P, N ) * 0.31830989, 0.0, 1.0 ) : vec3( 0.0 );
	if ( cellProbe == 90 )	// lane SKY1: the share of the weather's ambient the grid's sky replaced (lane GIAMB1: indoors the DALC's)
		return vec3( cellAmbShare( P, N ) );
	if ( cellProbe == 95 ) {	// lane GIBLACK1: r = the branch cellGiSample took / 16 (lane GIBLACK2; it was / 4), g = its .a, b = its E luminance / pi (sqrt)
		vec4 s = cellGiOn ? cellGiSample( P, N ) : vec4( 0.0 );
		return vec3( float( cellGiPath ) * 0.0625, clamp( s.a, 0.0, 1.0 ),
		             sqrt( clamp( dot( max( s.rgb, vec3( 0.0 ) ), vec3( 0.2126, 0.7152, 0.0722 ) ) * 0.31830989, 0.0, 1.0 ) ) );
	}
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
 * Display: pow(clamp(x), 1 / 2.2), a fixed curve so two bakes compare. Cyan = no probe reaches here (s65; FIXPACK1: was magenta, which means a missing texture);
 * in the surfel passes every surface is cyan and the surfel tiles draw over it (src/gl/cellprobeview.h). */
vec3 cellPassOut( vec3 P, vec3 N )
{
	const vec3 none = vec3( 0.0, 1.0, 1.0 );	// FIXPACK1 NOCOVER-COLOR: cyan, never magenta (magenta = missing texture)
	/* lane WALLSTACK1: Probe coverage (bungo 10-15, the deck's s-placement slide: empty upper floors in pink). Magenta
	 * where the probe term lights nothing: the valid share k of the GI row's own sample (cellGiSample: rooms, seen
	 * fill, lend rule, gap fill) is 0 (<= 0.01, the GI pass's own test); elsewhere grey 0.25 + 0.5 k. The pass draws no
	 * texture, so its magenta is never a missing one. Red WW_CELL_PV_RED=nofilter (bit 2048): the plain trilinear's k. It shares
	 * the one cellGiSample call below: each call site inlines the whole chain, and one more takes fo4_cell past what
	 * the driver links (GIDARK1: the surfaces then fall back to the legacy program, silently). */
	const vec3 hole = vec3( 1.0, 0.0, 1.0 );
	if ( ( cellPass >= 3 && cellPass != 6 ) || !cellGiOn )
		return cellPass == 6 ? hole : none;
	vec4 s = cellPass == 6 && ( cellPassRed & 2048 ) != 0 ? cellGiTrilinear( P, N )
	       : cellGiSample( P, cellPass != 6 && ( cellPassRed & 2 ) != 0 ? vec3( 0.0, 0.0, 1.0 ) : N );
	if ( cellPass == 6 )
		return s.a <= 0.01 ? hole : vec3( 0.25 + 0.5 * clamp( s.a, 0.0, 1.0 ) );
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

#if !defined( WW_CELL_PBR ) && ( !defined( WW_CELL_FX ) || defined( WW_FX_DECALLIT ) )	// lane EFX2: the effect program takes none of the surface lobes (lane DECALLIT1: a decal takes them)
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
bool cellFarFade = true;	// lane FARVIEW1: false = the lights unfaded
void cellSumLights( vec3 P, vec3 N, vec3 Vw, float gloss, out vec3 diff, out vec3 diffOn, out vec3 spec )
{
	diff = vec3( 0.0 );
	diffOn = vec3( 0.0 );
	cellRimSum = vec3( 0.0 );
	spec = vec3( 0.0 );
	float fw = cellFarFade ? cellFarW( P ) : 0.0;	// lane FARVIEW1: the real lights fade out over the band
	if ( fw >= 1.0 )
		return;
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
	if ( fw > 0.0 ) {
		diff *= 1.0 - fw;
		diffOn *= 1.0 - fw;
		cellRimSum *= 1.0 - fw;
		spec *= 1.0 - fw;
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
 * gi = the GI's irradiance / pi, the ambient's own units. keep (0: both, as before lane GIAMB1), replace (1, the default
 * since lane GIAMB1: the ambient x (1 - the grid's valid share), as SKY1 does outdoors), off (2), max (3: the larger of the two), asgi
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

#if !defined( WW_CELL_PBR ) && ( !defined( WW_CELL_FX ) || defined( WW_FX_DECALLIT ) )

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
	vec3 farF, farGP;	// lane FARVIEW1: the far placed light (diffuse only); the GI's placed share out x w
	float farW = cellFarTerms( P, N, farF, farGP );
	if ( farW > 0.0 ) {
		gi = max( gi - farW * farGP, vec3( 0.0 ) );
		diff += farW * farF;
		diffOn += farW * farF;
	}
	vec3 add = alb * ( diffOn + gi ) + spec * specMask * specCol;	// albedo x the game's diffuse; the GI stays Lambert
	/* lane SUNCELL1: an exterior with the weather's sun (cellHasDir, celllights.cpp) takes the game's terms:
	 * the sun through Oren-Nayar + rim, shadowed by the cascades (cellDirVis), and the cube at the game's strength.
	 * color is then the viewport's ambient alone (A x albedo) and cellExtSpec the sun's specular, both linear-squared
	 * by the caller; without the sun the old law (the viewport light plus the cell's lights). */
	if ( !cellInterior && !cellHasDir )
		return sqrt( color * color + add );
	if ( !cellInterior ) {
		vec3 Ld = normalize( cellDirTo );
		float nl = dot( N, Ld );
		vec3 dir = cellDirColor * max( nl, 0.0 ) * cellDirVis;
		vec3 Ex = diff + gi + dir + cellExtAmb;
		vec3 Edx = diffOn + gi + dir * ( cellOren( N, Ld, Vw, nl, gloss ) + cellRim( N, Ld, Vw, gloss ) );
		vec3 linx = color * color + alb * Edx + spec * specMask * specCol + cellExtSpec
		          + cubeK * ( ( cellRed & 512 ) != 0 ? Ex : Edx + cellExtAmb );
		return sqrt( max( linx, vec3( 0.0 ) ) ) + emissive;
	}
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
	if ( cellProbe == 90 )	// lane SKY1 (60-74 are other lanes'): the share of the weather's ambient the grid's sky replaced (lane GIAMB1: indoors the DALC's)
		return vec3( cellAmbShare( P, N ) );
	if ( cellProbe == 95 )	// lane GIBLACK1: the branch cellGiSample took, its .a and its E (cellProbeRaw's)
		return cellProbeRaw( P, N );
	if ( cellProbe >= 91 && cellProbe <= 94 )	// lane FARVIEW1
		return cellFarProbeOut( P, N );
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
