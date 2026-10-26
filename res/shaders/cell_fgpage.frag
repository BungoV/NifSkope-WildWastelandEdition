#version 410 core

/* lane FARPAGES1: one far ground page (src/gl/cellfarground.cpp) -- the cell's live layer blend (fgSplat, filtered at
 * the page's own texel) with the ring's decals composited over it, opaque. The draw then samples only the page. */

#define WW_FG_STAMP 1
#define WW_FARGROUND 0
#include "cell_farground.glsl"

uniform sampler2D fgHeight;		// the .lodl heights, a texel a sample
uniform vec2 fgStampCell;		// the page's cell origin, world
uniform float fgStampSide;
uniform int fgPageDecals;		// red WW_CELL_FARGROUND_RED=nodecals: 0

out vec4 fragColor;

void main()
{
	float texel = 4096.0 / fgStampSide;
	vec2 w = fgStampCell + gl_FragCoord.xy * texel;
	vec3 col = fgSplat( w, vec2( texel, 0.0 ), vec2( 0.0, texel ) );
	if ( fgPageDecals != 0 ) {
		vec2 g = ( w - fgGridMap.xy ) * fgGridMap.z;
		float z = texture( fgHeight, ( g + 0.5 ) / fgGridN ).r;
		vec4 d = fgDecals( vec3( w, z ), texel );
		col = col * ( 1.0 - d.a ) + d.rgb;
	}
	fragColor = vec4( col, 1.0 );
}
