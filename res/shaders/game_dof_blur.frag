#version 410 core

/* lane GRASSMB1: the depth of field's blur, instruction for instruction from the game's Blur3..Blur15 shaders
 * (Shaders011 entries 03725..03737, ImageSpaceEffectBlur, src/gl/cellpost.h):
 *   acc = sum over the taps of texture(t0, v1 + o.xy) * o.z
 * o = (offset x, offset y, weight, -): row r of the game's aBlurWeights table (2r + 1 taps, r <= 7), the offsets
 * (k - 7) / the input's size along this pass's axis. v1 = this pass's target uv (the target's size: dstSize). */

uniform sampler2D t0;
uniform vec4 o[15];
uniform int taps;
uniform vec2 dstSize;

out vec4 fragColor;

void main()
{
	vec2 v1 = gl_FragCoord.xy / dstSize;
	vec4 acc = vec4( 0.0 );
	for ( int i = 0; i < taps; i++ )					// loop / ige / breakc_nz
		acc = texture( t0, v1 + o[i].xy ) * o[i].z + acc;	// add, sample, mad
	fragColor = acc;
}
