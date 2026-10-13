#version 410 core

/* lane HDR1: the cell view's one tone map (src/gl/cellhdr.h). The main draw went into a linear float frame;
 * this full-screen pass draws it back into the view, sample for sample:
 *   hdrMode 0  the pixels a cell program or a cell effect wrote last: the cell's imagespace (the bloom added
 *              once, the exposure, the curve, the grade, the LUT) on the summed linear light, as the game runs
 *              its imagespace once on its HDR target
 *   hdrMode 1  the pixels any other program wrote last: the value as it was written
 *   hdrMode 2  (lane SKYHOLE1) the pixels of mode 0 no opaque draw reached (depth at the far plane): a cell
 *              effect over the sky, composited over the backdrop by its coverage instead of replacing it
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
	// lane SKYHOLE1: the passes sort the pixels by depth too (the far plane = no opaque draw reached them)
	gl_FragDepth = 1.0;
	if ( hdrMode == 2 ) {
		/* lane SKYHOLE1: a cell effect over the backdrop (the sky). The game draws its sky into the HDR target and
		 * blends the effect over it; here the sky is in the view, not the linear frame, so the effect went over 0.
		 * Its value un-premultiplied by its coverage (the frame's alpha: the effects blend alpha as ONE, dst),
		 * tone-mapped, then laid over the backdrop by that coverage (blend ONE, ONE_MINUS_SRC_ALPHA). */
		float A = clamp( x.a, 0.0, 1.0 );
		vec3 v = max( x.rgb, vec3( 0.0 ) );
		if ( !cellOn || !cellIsOn ) {
			fragColor = vec4( v, A );
			return;
		}
		if ( A > 1.0 / 1024.0 )
			fragColor = vec4( cellImageSpace( sqrt( v / A ) ) * A, A );
		else	// no coverage (an additive glow): its light added to the sky
			fragColor = vec4( max( cellImageSpace( sqrt( v ) ) - cellImageSpace( vec3( 0.0 ) ), vec3( 0.0 ) ), 0.0 );
		return;
	}
	if ( hdrMode != 0 || !cellOn || !cellIsOn ) {
		fragColor = x;
		return;
	}
	fragColor = vec4( cellImageSpace( sqrt( max( x.rgb, vec3( 0.0 ) ) ) ), 1.0 );
}
