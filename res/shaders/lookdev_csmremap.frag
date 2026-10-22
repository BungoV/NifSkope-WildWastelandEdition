#version 410 core

/* lane SHADOWCACHE1: the old static depth at this texel's place in the old window, mapped into the new depth range;
 * cleared depth stays 1, and texels the old window never covered get 1 (their strips are drawn after). */
uniform sampler2DArray srcMap;
uniform int srcX, srcY, srcW, srcH;
uniform float zScale, zBias;

void main()
{
	ivec2 p = ivec2( gl_FragCoord.xy ) + ivec2( srcX, srcY );
	float d = 1.0;
	if ( p.x >= 0 && p.y >= 0 && p.x < srcW && p.y < srcH ) {
		float o = texelFetch( srcMap, ivec3( p, 0 ), 0 ).r;
		d = o >= 1.0 ? 1.0 : clamp( o * zScale + zBias, 0.0, 1.0 );
	}
	gl_FragDepth = d;
}
