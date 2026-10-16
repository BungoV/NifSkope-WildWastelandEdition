#version 410 core

/* The cell view's tone map (src/gl/cellhdr.h): draws the linear float frame back into the view, per sample.
 * The stencil picks the mode per pixel:
 *   hdrMode 0  pixels a cell program or effect wrote last: the cell's imagespace on the summed linear light
 *   hdrMode 1  the pixels any other program wrote last: the value as it was written
 *   hdrMode 2  mode-0 pixels no opaque draw reached: a cell effect composited over the sky by its coverage
 *   hdrMode 3  (FO4CS bloom only) every pixel mode 0 does not take: the bloom's glow added in display space */

#define WW_CELLLIGHTS 1
#define WW_CELL_FX 1
#define WW_CELL_PBR 1
#define WW_CELL_HDR 1
#include "uniforms.glsl"
#include "cell_lights.glsl"
#include "cell_bloom.glsl"

uniform sampler2DMS hdrSrc;
uniform int hdrMode;

out vec4 fragColor;

void main()
{
	vec4 x = texelFetch( hdrSrc, ivec2( gl_FragCoord.xy ), gl_SampleID );
	// the passes sort pixels by depth too (far plane = no opaque draw reached them)
	gl_FragDepth = 1.0;
	/* the FO4CS bloom at this pixel (cell_bloom.glsl), times the bloom weight; cellImageSpace adds it before
	 * its curve. */
	vec3 foBloom = vec3( 0.0 );
	if ( cellIsFoBloomOn )
		foBloom = bloomTentDispersed( cellIsFoBloom, ( floor( gl_FragCoord.xy ) + 0.5 ) / cellIsFoBloomFrame,
			cellIsFoBloomParams.x, cellIsFoBloomParams.z ) * cellIsFoBloomParams.y;
	if ( hdrMode == 3 ) {
		cellFoBloomHere = foBloom;
		vec3 lit = cellImageSpace( vec3( 0.0 ) );
		cellFoBloomHere = vec3( 0.0 );
		fragColor = vec4( max( lit - cellImageSpace( vec3( 0.0 ) ), vec3( 0.0 ) ), 0.0 );
		return;
	}
	// mode 2 (the effects over the sky) takes the glow from mode 3, as every pixel mode 0 does not take
	cellFoBloomHere = hdrMode == 0 ? foBloom : vec3( 0.0 );
	if ( hdrMode == 2 ) {
		/* a cell effect over the sky: the sky is in the view, not the linear frame, so the effect went over 0.
		 * Un-premultiply by coverage (effects blend alpha as ONE, dst), tone-map, lay over by coverage. */
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
