#version 410 core

/* lane EMPTY240 R11-1: the measure drawn straight at the quarter size (src/features/exposure/cellhdr.cpp isMeasureFrame).
 * Each quarter texel reads its 4 x 4 frame pixels: a pixel counts where the frame's stencil (sample 0, as the old stencil
 * blit kept) reads 1 in its low two bits (a cell program or a cell effect wrote it last: linear light), its samples
 * averaged (a non-finite one read as 0). rgb = the sum over the counted pixels / 16, a = their count / 16: the same
 * texel the full-size draw's mip level 2 held, with no full-size draw, stencil copy or mip build. */

uniform sampler2DMS hdrSrc;
uniform usampler2DMS hdrStencil;
uniform int hdrSamples;

out vec4 fragColor;

float finiteOr0( float v )
{
	return isnan( v ) || isinf( v ) ? 0.0 : v;
}

void main()
{
	ivec2 size = textureSize( hdrSrc );
	ivec2 base = ivec2( gl_FragCoord.xy ) * 4;
	vec3 sum = vec3( 0.0 );
	float cover = 0.0;
	for ( int y = 0; y < 4; y++ ) {
		for ( int x = 0; x < 4; x++ ) {
			ivec2 p = base + ivec2( x, y );
			if ( p.x >= size.x || p.y >= size.y )
				continue;
			if ( ( texelFetch( hdrStencil, p, 0 ).r & 3u ) != 1u )
				continue;
			vec3 px = vec3( 0.0 );
			for ( int i = 0; i < hdrSamples; i++ ) {
				vec3 v = texelFetch( hdrSrc, p, i ).rgb;
				px += vec3( finiteOr0( v.r ), finiteOr0( v.g ), finiteOr0( v.b ) );
			}
			sum += px / float( max( hdrSamples, 1 ) );
			cover += 1.0;
		}
	}
	fragColor = vec4( sum / 16.0, cover / 16.0 );
}
