#version 410 core

/* lane FXRES1 (src/gl/cellfxres.h): the soft-effect layer's depth, the NEAREST opaque depth under each of its texels
 * (Cantlay, GPU Gems 3 ch. 23: an effect is hidden in the layer only where the nearest surface hides it; the
 * composite's depth test picks the texel that belongs to the pixel's own surface). Written as the layer's depth
 * buffer and as a colour copy the soft fade reads (fxDepth). */

uniform sampler2D fxrSrc;	// the opaque pass's depth at full resolution (Scene::fxDepthTexId)
uniform int fxrScale;		// full-resolution pixels per layer texel, per axis
uniform ivec2 fxrSrcSize;

layout ( location = 0 ) out vec4 fxrZ;

void main()
{
	ivec2 base = ivec2( gl_FragCoord.xy ) * fxrScale;
	float d = 1.0;
	for ( int y = 0; y < fxrScale; y++ )
		for ( int x = 0; x < fxrScale; x++ )
			d = min( d, texelFetch( fxrSrc, min( base + ivec2( x, y ), fxrSrcSize - 1 ), 0 ).r );
	fxrZ = vec4( d );
	gl_FragDepth = d;
}
