#version 410 core

/* lane MOTION1: the temporal AA's motion vectors (src/gl/gametaa.h). The game draws them from its UNJITTERED
 * current and previous matrices; this viewer's scenes stand still, so the camera's reprojection of the frame's
 * depth is the whole of it: the pixel's unjittered position (its NDC less the jitter) taken to the previous
 * frame's clip space by reproj = previous view-projection x inverse current view-projection (built in double).
 * The vector is previous minus current in texture units: the history is read at uv + mv, as the game reads it. */

uniform sampler2D depthTex;
uniform mat4 reproj;
uniform vec2 jitterNdc;
uniform vec2 invSize;

out vec2 mv;

void main()
{
	vec2 uv = gl_FragCoord.xy * invSize;
	float d = texelFetch( depthTex, ivec2( gl_FragCoord.xy ), 0 ).r;
	vec3 ndc = vec3( uv * 2.0 - 1.0 - jitterNdc, d * 2.0 - 1.0 );
	vec4 p = reproj * vec4( ndc, 1.0 );
	mv = ( p.xy / p.w - ndc.xy ) * 0.5;
}
