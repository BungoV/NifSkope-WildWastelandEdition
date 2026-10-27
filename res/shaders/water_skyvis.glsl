/* lane WATERLOOK1 step 10 (WATERSKYVIS1, src/gl/cellwater.h): the water's sky reflection and sun glare dimmed where the
 * sky is hidden (under a bridge, a pier, an overhang). Only fo4_waterlook.frag compiles this (WW_WATER_SKYVIS, after
 * cell_lights.glsl); the Water sky visibility row.
 * The read is SPECOCC1's (cellSpecSky): the probe sky grid's open-sky share rides cellGi after the GI's own slabs, three
 * axis slabs weighted n^2 on the direction read. The sky reflection reads it on the lobe's dominant direction (R for a
 * mirror, bending to N as the waves roughen); the sun's glare on the sun's own direction. Full strength (the water's own
 * row, not the surfaces' Sky visibility strength), faded out over the grid's padded edge; 1 where the grid has no
 * evidence, indoors, under water or with no sky grid packed (cellSpecSkyBase 0). Red WW_CELL_WATER_SKYVIS_RED: one
 * (visibility 1 everywhere), nosun (the glare not dimmed).
 * lane WATERREBASE1 (WATERSKYVISFAR2 goal 5): one factor, never two. Every "no near evidence" answer (row off, red, no
 * grid, the padded edge) falls back to farV, the far water's baked byte (1.0 without WW_WATER_SKYFAR), so the near grid
 * and the far byte never both dim the same sky or glare. */

uniform bool waterSkyVisOn;	// the Water sky visibility row
uniform int waterSkyVisRed;	// 1 one, 2 nosun

float wwWaterSkyVisDir( vec3 P, vec3 N, vec3 D, float farV )
{
	if ( !waterSkyVisOn || ( waterSkyVisRed & 1 ) != 0 || !cellGiOn || !cellGiSky || cellInterior || cellSpecSkyBase < 1.0 )
		return farV;
	vec3 t = ( P + N * ( 0.5 * cellGiVoxel ) - cellGiOrigin ) / cellGiVoxel;	// texels: voxel i's centre is i + 0.5
	float z = clamp( t.z, 0.5, cellGiDims.z - 0.5 );	// the slabs stack on z: never filter across one
	vec3 sl = vec3( D.x >= 0.0 ? 0.0 : 1.0, D.y >= 0.0 ? 2.0 : 3.0, D.z >= 0.0 ? 4.0 : 5.0 );
	vec3 w = D * D;
	float depth = float( textureSize( cellGi, 0 ).z );
	vec2 a = vec2( 0.0 );
	for ( int k = 0; k < 3; k++ ) {
		vec4 s = texture( cellGi, vec3( t.xy / cellGiDims.xy, ( ( cellSpecSkyBase + sl[k] ) * cellGiDims.z + z ) / depth ) );
		a += w[k] * s.ra;
	}
	if ( a.y < 0.01 )
		return farV;
	return mix( farV, clamp( a.x / a.y, 0.0, 1.0 ), cellGiEdgeFade( t.xy ) );
}

// the sky reflection's share of open sky: the lobe's dominant direction, R bending to N with the roughness (alpha)
float wwWaterSkyVis( vec3 P, vec3 N, vec3 R, float rough, float farV )
{
	float r = clamp( rough, 0.0, 1.0 );
	return wwWaterSkyVisDir( P, N, normalize( mix( N, R, ( 1.0 - r ) * ( sqrt( 1.0 - r ) + r ) ) ), farV );
}

// the sun's glare: the sky grid on the sun's direction (red nosun: the far byte alone)
float wwWaterSkyVisSun( vec3 P, vec3 N, vec3 L, float farV )
{
	return ( waterSkyVisRed & 2 ) != 0 ? farV : wwWaterSkyVisDir( P, N, L, farV );
}
