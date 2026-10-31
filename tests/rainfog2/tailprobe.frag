#version 410 core

/* lane RAINFOG2 (tests/rainfog2/rainfog2_check.py): the REAL lookdev_fog.glsl tails at probe points. One fragment per
 * probe: probePos texel (x, y) holds a view-space point (xyz) and a mode (w): 0 the surface read wwVolAirAt(p), 1 the
 * sky read wwVolAirSky(normalize(p)). The checker copies this file beside a copy of res/shaders and links it with
 * cell_ao.vert (a full-screen quad). */

#include "lookdev_fog.glsl"

uniform sampler2D probePos;

out vec4 fragColor;

void main()
{
	vec4 p = texelFetch( probePos, ivec2( gl_FragCoord.xy ), 0 );
	fragColor = p.w < 0.5 ? wwVolAirAt( p.xyz ) : wwVolAirSky( normalize( p.xyz ) );
}
