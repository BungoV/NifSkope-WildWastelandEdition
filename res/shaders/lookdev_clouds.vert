#version 410 core

/* Lookdev cloud layer (lane PBRWX1), vertex stage: view rotation only, the
 * layer's uv plus its scroll offset (fract(speed * 0.1 * real seconds)).
 * probe: a full-screen quad that samples ONE uv (the texel gate). */

#include "uniforms.glsl"

layout ( location = 0 ) in vec3 vertexPosition;
layout ( location = 1 ) in vec4 vertexColor;	// white, alpha = the edge fade
layout ( location = 7 ) in vec2 multiTexCoord0;

uniform vec2 uvOffset;
uniform bool probe;
uniform vec2 probeUV;

out vec4 vcol;
out vec2 uv;
out vec2 ndc;

void main()
{
	if ( probe ) {
		ndc = vertexPosition.xy;
		vcol = vec4( 1.0 );
		uv = probeUV + uvOffset;
		gl_Position = vec4( vertexPosition.xy, 0.0, 1.0 );
		return;
	}
	ndc = vec2( 0.0 );
	vcol = vertexColor;
	uv = multiTexCoord0 + uvOffset;
	vec4 q = projectionMatrix * vec4( viewMatrix * vertexPosition, 1.0 );
	gl_Position = vec4( q.xy, 0.0, q.w );
}
