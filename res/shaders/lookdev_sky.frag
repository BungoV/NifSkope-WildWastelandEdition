#version 410 core

/* Lookdev background (lane PBRR2B, docs s6.2 stage 1: cube-only background).
 * The lookdev cube (linear, SFCubeMapCache level 0), UNTINTED by the weather
 * until W2 (ruling Q8), plus a GGX glow at the sun DISC (the tent-arc position,
 * not the floored light) while the disc is above the horizon. Out through the
 * Studio output path (exposure, view transform, sRGB encode). */

#include "uniforms.glsl"

uniform samplerCube CubeMap;
uniform bool hasCubeMap;
uniform bool invertZAxis;
uniform vec3 sunDiscView;	// view space, unit; z of the WORLD disc <= 0 -> sunDiscUp false
uniform bool sunDiscUp;
uniform vec3 sunLinear;
uniform float sceneExposure;
uniform int viewTransform;

in vec2 ndc;

out vec4 fragColor;

#include "lookdev_output.glsl"

void main()
{
	if ( projectionMatrix[3][3] == 1.0 ) {
		// orthographic: no eye ray; a flat mid-grey stands in
		fragColor = vec4( studioOutput( vec3( 0.18 ) ), 1.0 );
		return;
	}
	vec3 ray = normalize( vec3( ( ndc.x + projectionMatrix[2][0] ) / projectionMatrix[0][0],
	                            ( ndc.y + projectionMatrix[2][1] ) / projectionMatrix[1][1], -1.0 ) );
	mat3 r = envMapRotation;
	if ( invertZAxis ) {
		r[0][2] *= -1.0;
		r[1][2] *= -1.0;
		r[2][2] *= -1.0;
	}
	vec3 color = hasCubeMap ? textureLod( CubeMap, r * ray, 0.0 ).rgb : vec3( 0.18 );
	if ( sunDiscUp ) {
		float VdotL = dot( ray, normalize( sunDiscView ) );
		if ( VdotL > 0.0 ) {
			float alpha = 0.1 * 0.1;
			float a2 = alpha * alpha;
			float denom = VdotL * a2 + a2 + max( 1.0 - VdotL, 0.0 );
			color += sunLinear * ( a2 / ( denom * denom ) ) * VdotL;
		}
	}
	fragColor = vec4( studioOutput( color ), 1.0 );
}
