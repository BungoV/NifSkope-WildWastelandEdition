#version 410 core

/* Lane RTXFIX1 (2026-10-30): the Ray-traced GI probes view, fragment stage, inside the cell's linear frame (bungo 10-29:
 * "the spheres draw on top of the fog, unfogged, crisp at any distance"). Irradiance view: this probe alone at the
 * sphere's normal, the read's decode (cell_ddgi.glsl cellDdgiTile: bilinear on the encoded tile, ^2.5, squared, x 2 pi),
 * shown as a white Lambertian surface lit by it (E / pi), linear, so the exposure and the tone map treat it as they treat
 * every surface; a probe the read never takes is dark grey. States view: the flat state colours, divided by the scene's
 * exposure so they keep their look through it. Then the fog (lookdev_fog.glsl wwFog), as on every cell surface. */

#include "uniforms.glsl"

uniform int cellGiLoop8;	// cell_ddgi.glsl's read names it (cellDdgiRead is not called here)
#include "cell_ddgi.glsl"

uniform int probeMode;		// 1 states, 2 irradiance
uniform bool probeFog;		// false: no fog (WW_CELL_RTDDGI_VIEW_FOG=nofog)
uniform float probeExposure;	// the scene's exposure scale (the states view's colours keep their look)

in vec3 probeN;
in vec3 viewPos;
flat in int probeIdx;
flat in float probeState;

out vec4 fragColor;

#include "lookdev_fog.glsl"

vec3 srgbToLinear( vec3 c )
{
	return pow( c, vec3( 2.2 ) );
}

void main()
{
	if ( !cellDdgiOn )
		discard;	// no volume up for this document (src/rtddgicell.cpp rtddgiCellUniforms)
	vec3 col;
	if ( probeMode == 1 ) {
		// Vigilant green, Asleep blue, Off red, fresh white (src/rtddgicell.cpp's states view)
		vec3 fc = vec3( 0.2, 0.9, 0.3 );
		if ( probeState > 0.5 && probeState < 1.5 )
			fc = vec3( 0.35, 0.45, 0.95 );
		else if ( probeState > 1.5 && probeState < 2.5 )
			fc = vec3( 0.95, 0.15, 0.15 );
		else if ( probeState > 2.5 )
			fc = vec3( 0.95, 0.95, 0.95 );
		col = srgbToLinear( fc ) / max( probeExposure, 1e-6 );
	} else {
		vec4 pr = texelFetch( cellDdgiAtlas, 192 * cellDdgiDims.w + probeIdx );
		if ( int( pr.w + 0.5 ) != 2 ) {
			col = srgbToLinear( vec3( 0.12 ) ) / max( probeExposure, 1e-6 );	// the read never takes it
		} else {
			vec4 e = cellDdgiTile( probeIdx, normalize( probeN ), 8 );
			vec3 q = vec3( pow( max( e.x, 0.0 ), 2.5 ), pow( max( e.y, 0.0 ), 2.5 ), pow( max( e.z, 0.0 ), 2.5 ) );
			col = q * q * 2.0;	// E / pi = q^2 x 2 pi / pi
		}
	}
	if ( probeFog )
		col = wwFog( col, viewPos );
	fragColor = vec4( max( col, vec3( 0.0 ) ), 1.0 );
}
