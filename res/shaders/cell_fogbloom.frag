#version 410 core

/* lane FOGBLOOM1 (src/gl/cellfogbloom.h): the bloom-intensity target (RDR2's R8 "bloom intensity"). Per pixel, how much
 * more bloom the tone map adds there: gain = 1 + k f (1 - T) (1 + b translucent), stored as (gain - 1) / 4.
 *   T  the air's transmittance along the view ray to the pixel's depth (the froxel volume, lookdev_fog.glsl), so any
 *      change in the air's density moves it; a sky pixel takes the ray to the grid's end
 *   f  smoothstep over the ray's MEAN extinction tau / d, from mist (V 2 km) to fog (V 1 km): 0 for every ray through
 *      clearer air, so clear weather writes exactly 0 and the tone map draws today's bits */

#include "uniforms.glsl"
#include "lookdev_fog.glsl"

uniform sampler2D fbDepth;		// the frame's depth, samples nearest-resolved (window depth)
uniform vec2 fbProj;			// the projection's [0][0], [1][1]
uniform vec2 fbProjZ;			// the projection's [2][2], [3][2]
uniform vec4 fbParams;			// k, b, sigma at f = 0, sigma at f = 1 (per game unit)
uniform int fbTranslucent;		// 1: the second draw, over the samples a blended linear draw wrote last (stencil 0x04)

out vec4 fragColor;

void main()
{
	fragColor = vec4( 0.0 );
	if ( !fogOn || !volOn || volSlices.w < 0.5 )
		return;
	vec2 ndc = ( gl_FragCoord.xy - volRect.xy ) * volRect.zw * 2.0 - 1.0;
	vec3 dirView = vec3( ndc.x / fbProj.x, ndc.y / fbProj.y, -1.0 );
	float z = texelFetch( fbDepth, ivec2( gl_FragCoord.xy ), 0 ).r;
	float viewZ;	// -z in view units
	if ( z >= 1.0 ) {	// the sky: the ray to the grid's end, not the horizon's endless tail (T 0 in any air)
		float far = volFar.w > 0.5 ? volFar.x * exp( volFar.y ) : volSlices.x * exp( volSlices.y );
		viewZ = 0.999 * far / ( max( fogDistScale, 1e-12 ) * length( dirView ) );
	} else {
		viewZ = max( fbProjZ.y / ( 2.0 * z - 1.0 + fbProjZ.x ), 0.0 );
	}
	vec3 posView = dirView * viewZ;
	float d = length( posView ) * fogDistScale;
	float T = clamp( wwVolAirAt( posView ).a, 1e-6, 1.0 );
	float f = smoothstep( fbParams.z, fbParams.w, -log( T ) / max( d, 1.0 ) );
	float extra = fbParams.x * f * ( 1.0 - T ) * ( fbTranslucent == 1 ? 1.0 + fbParams.y : 1.0 );
	fragColor = vec4( clamp( extra * 0.25, 0.0, 1.0 ), 0.0, 0.0, 1.0 );
}
