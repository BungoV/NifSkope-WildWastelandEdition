#version 410 core

/* lane TAAINPUTS1: viewMode 1 velocity (hue = direction, brightness = pixels a frame / viewScale, white tint where
 * it is an object vector), 2 the mask (R reactive, G composition, B own motion), 3 velocity left | mask right. */

uniform sampler2D velTex;
uniform sampler2D maskTex;
uniform int viewMode;
uniform vec2 size;
uniform float viewScale;

out vec4 fragColor;

vec3 hue( float h )
{
	return clamp( abs( fract( h + vec3( 0.0, 2.0 / 3.0, 1.0 / 3.0 ) ) * 6.0 - 3.0 ) - 1.0, 0.0, 1.0 );
}

void main()
{
	ivec2 p = ivec2( gl_FragCoord.xy );
	int mode = viewMode;
	if ( mode == 3 )
		mode = gl_FragCoord.x < size.x * 0.5 ? 1 : 2;
	vec4 m = texelFetch( maskTex, p, 0 );
	if ( mode == 2 ) {
		fragColor = vec4( m.rgb, 1.0 );
		return;
	}
	vec2 px = texelFetch( velTex, p, 0 ).xy * size;
	float len = length( px );
	vec3 c = len > 1e-4 ? hue( atan( px.y, px.x ) / 6.28318531 + 0.5 ) * clamp( len / viewScale, 0.0, 1.0 ) : vec3( 0.0 );
	fragColor = vec4( mix( c, vec3( 1.0 ), m.b * 0.25 ), 1.0 );
}
