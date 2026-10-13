#version 410 core

/* lane BLOOM1b (src/gl/cellbloom.h): FO4CS's BloomUpCS as a fragment pass. combined = source x params.y +
 * tent(accumulated) x params.z, the tent's red and blue pushed along the radial by the dispersion. The chain's
 * levels, and (the dump only) the final combine over a zero source. */

#include "cell_bloom.glsl"

uniform sampler2D bloomSrc;		// this level's mip (the source band)
uniform sampler2D bloomMip;		// the accumulated chain below it
uniform vec2 bloomDst;			// this level's size
uniform vec4 bloomParams;		// radius (pixels), source weight, bloom weight, dispersion

out vec4 fragColor;

void main()
{
	vec2 uv = ( floor( gl_FragCoord.xy ) + 0.5 ) / bloomDst;
	vec3 source = bloomSanitize( textureLod( bloomSrc, uv, 0.0 ).rgb );
	vec3 bloom = bloomTentDispersed( bloomMip, uv, bloomParams.x, bloomParams.w );
	fragColor = vec4( source * bloomParams.y + bloom * bloomParams.z, 1.0 );
}
