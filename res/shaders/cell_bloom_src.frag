#version 410 core

/* The bloom's source: the linear frame's cell-lit samples times the imagespace exposure (FO4CS order: exposure,
 * bloom, tone map). Multisampled beside the frame's depth / stencil; a blit resolves it for the chain. */

#define WW_CELLLIGHTS 1
#define WW_CELL_FX 1
#define WW_CELL_PBR 1
#define WW_CELL_HDR 1
#include "uniforms.glsl"
#include "cell_lights.glsl"

uniform sampler2DMS hdrSrc;
uniform vec3 bloomImpulse;	// test probe: x, y, value (value > 0: only the 2x2 block holding x, y, at that value;
							// a lone pixel would be dropped by the Karis average)

out vec4 fragColor;

void main()
{
	if ( bloomImpulse.z > 0.0 ) {
		fragColor = vec4( ivec2( gl_FragCoord.xy ) / 2 == ivec2( bloomImpulse.xy ) / 2 ? vec3( bloomImpulse.z ) : vec3( 0.0 ), 1.0 );
		return;
	}
	if ( !cellOn || !cellIsOn ) {
		fragColor = vec4( 0.0 );
		return;
	}
	vec4 x = texelFetch( hdrSrc, ivec2( gl_FragCoord.xy ), gl_SampleID );
	fragColor = vec4( max( x.rgb, vec3( 0.0 ) ) * ( ( cellIsRed & 2 ) != 0 ? 1.0 : cellIsExposure ), 1.0 );
}
