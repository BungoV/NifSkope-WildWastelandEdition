#version 410 core

/* lane FARDRAW1: the cached far-ground option's stamp (src/gl/cellfarground.cpp). One page a cell: every decal of
 * the ring over the cell's ground, premultiplied, at fgStampSide texels a cell (the "Far ground pages" preset). */

#define WW_FG_STAMP 1
#define WW_FARGROUND 0
#include "cell_farground.glsl"

uniform sampler2D fgHeight;		// the .lodl heights, a texel a sample
uniform vec2 fgStampCell;		// the page's cell origin, world
uniform float fgStampSide;

out vec4 fragColor;

void main()
{
	vec2 w = fgStampCell + gl_FragCoord.xy / fgStampSide * 4096.0;
	vec2 g = ( w - fgGridMap.xy ) * fgGridMap.z;
	float z = texture( fgHeight, ( g + 0.5 ) / fgGridN ).r;
	fragColor = fgDecals( vec3( w, z ), 4096.0 / fgStampSide );
}
