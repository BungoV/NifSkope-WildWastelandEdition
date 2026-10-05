#version 410 core

/* lane GRASSMB1: the depth of field's downsample, instruction for instruction from the game's shader (Shaders011
 * entry 03706, ImageSpaceEffectDepthOfField's first pass, src/gl/cellpost.h). Four taps of the full-size frame
 * into the quarter-size target: uv = (v1 + tap.xy * c7.xy) * c4.xy, a tap whose colour is Inf or NaN counts as 0,
 * weighted by tap.z; alpha = c7.z.
 * c4 = (1, 1, 2/W, 2/H), c7 = (1/W, 1/H, 0, 0), taps = (+-1, +-1, 0.25). */

uniform sampler2D t0;
uniform vec4 c4;
uniform vec4 c7;
uniform vec4 taps[4];
uniform vec2 dstSize;

out vec4 fragColor;

void main()
{
	vec2 v1 = gl_FragCoord.xy / dstSize;					// the ceil(W/4) x ceil(H/4) target's uv
	vec3 acc = vec3( 0.0 );
	for ( int i = 0; i < 4; i++ ) {						// loop / ige / breakc_nz
		vec2 uv = ( taps[i].xy * c7.xy + v1 ) * c4.xy;		// mad, mul
		vec3 c = texture( t0, uv ).rgb;
		bvec3 bad = equal( floatBitsToUint( c ) & 0x7f800000u, uvec3( 0x7f800000u ) );	// and, ieq
		c = mix( c, vec3( 0.0 ), bad );					// movc
		acc = c * taps[i].z + acc;							// mad
	}
	fragColor = vec4( acc, c7.z );
}
