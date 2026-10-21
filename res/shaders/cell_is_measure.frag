#version 410 core

/* lane PERF1: the exposure's and the bloom's source from the linear frame already drawn (src/gl/cellhdr.cpp),
 * as the game reads its own HDR target: the pixel's samples averaged (a non-finite one read as 0), alpha 1. The
 * stencil lets it write only where a cell program or a cell effect wrote last (linear light); its mips then hold
 * the mean light over those pixels (rgb / a). */

uniform sampler2DMS hdrSrc;
uniform int hdrSamples;

out vec4 fragColor;

float finiteOr0( float v )
{
	return isnan( v ) || isinf( v ) ? 0.0 : v;
}

void main()
{
	vec3 sum = vec3( 0.0 );
	for ( int i = 0; i < hdrSamples; i++ ) {
		vec3 v = texelFetch( hdrSrc, ivec2( gl_FragCoord.xy ), i ).rgb;
		sum += vec3( finiteOr0( v.r ), finiteOr0( v.g ), finiteOr0( v.b ) );
	}
	fragColor = vec4( sum / float( max( hdrSamples, 1 ) ), 1.0 );
}
