#version 410 core

/* lane POSTFX3: the linear frame's multisamples into one value for the Modern temporal AA before the tone map
 * (src/gl/gametaa.h, WW_TAA2_LINEAR). Each sample is weighted as the display will show it (Karis 2014: c / (1 + max c)
 * at about display level), the mean turned back to light. A plain average (the red, a blit) lets one sky sample
 * take an edge pixel over, so the tone-mapped edge steps where the toned resolve's does not. */

uniform sampler2DMS msTex;
uniform int msSamples;
uniform float msExpose;	// the linear frame's scale to about display level (last frame's exposure)

out vec4 fragColor;

void main()
{
	ivec2 p = ivec2( gl_FragCoord.xy );
	vec3 sum = vec3( 0.0 );
	float a = 0.0;
	for ( int i = 0; i < msSamples; i++ ) {
		vec4 c = texelFetch( msTex, p, i );
		vec3 t = max( c.rgb, vec3( 0.0 ) ) * msExpose;
		sum += t / ( 1.0 + max( t.r, max( t.g, t.b ) ) );
		a += c.a;
	}
	float inv = 1.0 / float( max( msSamples, 1 ) );
	vec3 t = clamp( sum * inv, 0.0, 0.999 );
	fragColor = vec4( t / ( ( 1.0 - max( t.r, max( t.g, t.b ) ) ) * max( msExpose, 1e-6 ) ), a * inv );
}
