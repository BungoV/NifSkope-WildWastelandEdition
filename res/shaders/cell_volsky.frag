#version 410 core

/* lane VOLAIR1 (bungo 10-14): the sky through the air (src/gl/cellvolair.h wwVolAirSkyDraw). The sky is a surface at
 * infinite distance; lookdev_fog.glsl wwVolAirSky gives the air's light L and transmittance T along its ray.
 *   volSkyMode 0  into the cell's linear frame, under everything the scene draws: the air's light with coverage 1 - T.
 *                 The resolve (cell_hdr.frag mode 2) lays it over the backdrop: the sky x T + the tone-mapped fog
 *                 colour at infinity L / (1 - T) x (1 - T), the colour far geometry fades to.
 *   volSkyMode 1  the gates' probes, raw, on the pixels no draw reached: WW_CELL_AIR_PROBE 1 (L) / 2 (T), and the far
 *                 gates' geometry probe (WW_LOOKDEV_FOGPROBE 9 / 10) marks the sky (1, 1, 1): the geometry writes B 0.
 *   volSkyOn      false under the red nosky: the sky without the air (T 1, L 0). */

#include "uniforms.glsl"
#include "lookdev_fog.glsl"

uniform int volSkyMode;
uniform bool volSkyOn;

out vec4 fragColor;

// the view-space unit ray through this fragment (a pass with no geometry: the sky's). Here, not in lookdev_fog.glsl:
// it reads uniforms.glsl's projectionMatrix, and cell_volair.frag includes the fog without that block (10-15: it
// stopped compiling, and the air was refused on every run)
vec3 wwSkyRay()
{
	vec2 ndc = ( gl_FragCoord.xy - volRect.xy ) * volRect.zw * 2.0 - 1.0;
	return normalize( vec3( ( ndc.x + projectionMatrix[2][0] ) / projectionMatrix[0][0],
		( ndc.y + projectionMatrix[2][1] ) / projectionMatrix[1][1], -1.0 ) );
}

void main()
{
	gl_FragDepth = 1.0;	// the probes' pass tests GL_LEQUAL against the frame: only the pixels at the far plane
	if ( !fogOn || !volOn ) {
		fragColor = vec4( 0.0 );
		return;
	}
	if ( volSkyMode == 1 && fogProbe.z > 8.5 ) {
		fragColor = vec4( 1.0 );
		return;
	}
	vec4 a = volSkyOn ? wwVolAirSky( wwSkyRay() ) : vec4( 0.0, 0.0, 0.0, 1.0 );
	if ( volSkyMode == 1 ) {
		fragColor = vec4( volProbe == 1 ? a.rgb : vec3( a.a ), 1.0 );
		return;
	}
	fragColor = vec4( a.rgb, 1.0 - a.a );
}
