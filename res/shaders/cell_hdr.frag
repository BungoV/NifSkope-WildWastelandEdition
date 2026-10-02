#version 410 core

/* lane HDR1: the cell view's one tone map (src/gl/cellhdr.h). The main draw went into a linear float frame;
 * this full-screen pass draws it back into the view, sample for sample:
 *   hdrMode 0  the pixels a cell program or a cell effect wrote last: the cell's imagespace (the bloom added
 *              once, the exposure, the curve, the grade, the LUT) on the summed linear light, as the game runs
 *              its imagespace once on its HDR target
 *   hdrMode 1  the pixels any other program wrote last: the value as it was written
 * The stencil (copied back before this pass) picks the pixels for each mode. */

#define WW_CELLLIGHTS 1
#define WW_CELL_FX 1
#define WW_CELL_PBR 1
#define WW_CELL_HDR 1
#include "uniforms.glsl"
#include "cell_lights.glsl"

uniform sampler2DMS hdrSrc;
uniform int hdrMode;

out vec4 fragColor;

void main()
{
	vec4 x = texelFetch( hdrSrc, ivec2( gl_FragCoord.xy ), gl_SampleID );
	if ( hdrMode != 0 || !cellOn || !cellIsOn ) {
		fragColor = x;
		return;
	}
	fragColor = vec4( cellImageSpace( sqrt( max( x.rgb, vec3( 0.0 ) ) ) ), 1.0 );
}
