#version 430 core
in vec2 vUv;
layout(location = 0) out vec4 oColour;
uniform sampler2D uAlbedo;
uniform sampler2D uNormal;
uniform sampler2D uDepth;
uniform sampler2D uAo;
uniform mat4 uInvViewProj;
uniform vec4 uAoMap;
uniform float uAoLod;
uniform int uAoMode;
void main()
{
	ivec2 p = ivec2( gl_FragCoord.xy );
	float d = texelFetch( uDepth, p, 0 ).r;
	if ( d >= 1.0 ) {
		oColour = vec4( 0.55, 0.64, 0.78, 1.0 );
		return;
	}
	vec3 a = texelFetch( uAlbedo, p, 0 ).rgb;
	vec3 n = normalize( texelFetch( uNormal, p, 0 ).rgb * 2.0 - 1.0 );
	vec3 L = normalize( vec3( 0.35, 0.45, 0.82 ) );
	float ao = 1.0;
	if ( uAoMode != 0 ) {
		vec4 ndc = vec4( vUv * 2.0 - 1.0, d * 2.0 - 1.0, 1.0 );
		vec4 wp = uInvViewProj * ndc;
		vec3 w = wp.xyz / wp.w;
		vec2 uv = vec2( ( w.x - uAoMap.x ) * uAoMap.z, ( uAoMap.y - w.y ) * uAoMap.w );
		ao = textureLod( uAo, uv, uAoLod ).r;
	}
	oColour = vec4( a * ( 0.45 + 0.75 * max( 0.0, dot( n, L ) ) ) * ao, 1.0 );
}
