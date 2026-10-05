#version 410 core

/* lane GRASSMB1: the depth of field's blurred picture (src/gl/cellpost.h). One direction per pass (stepUv), a
 * Gaussian of the record's radius with sigma = radius / 2. The game's own kernel (ImageSpaceEffectBlur) is not
 * ported: a stated gap. */

uniform sampler2D srcTex;
uniform vec2 stepUv;
uniform int radius;
uniform float sigma;

out vec4 fragColor;

void main()
{
	vec2 uv = gl_FragCoord.xy / vec2( textureSize( srcTex, 0 ) );
	vec4 acc = vec4( 0.0 );
	float wsum = 0.0;
	for ( int i = -radius; i <= radius; i++ ) {
		float w = exp( -0.5 * float( i * i ) / ( sigma * sigma ) );
		acc += w * texture( srcTex, uv + float( i ) * stepUv );
		wsum += w;
	}
	fragColor = acc / wsum;
}
