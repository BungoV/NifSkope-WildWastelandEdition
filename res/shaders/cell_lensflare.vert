#version 410 core

/* lane LIGHTS3 (LENSFLARE1): one lens-flare sprite corner. The CPU places the quad (centre + corner offset, NDC);
 * here the light's own screen point is tested against the opaque depth (a 5 x 5 kernel, 2 px apart: inferred,
 * the game averages its own visibility test over 4 frames) and the share seen scales the colour, and the size too
 * when the sprite Shrinks When Occluded. */

layout ( location = 0 ) in vec4 vertexPosition;	// centre xy, corner offset xy (NDC)
layout ( location = 1 ) in vec4 vertexColor;	// colour rgb, shrinks (0 / 1)
layout ( location = 2 ) in vec4 vertexUV;	// u, v, 0, 0
layout ( location = 3 ) in vec4 vertexLight;	// the light's NDC x, y and its view depth (game units, positive)

uniform sampler2D flareDepth;	// the opaque pass's depth, the viewport's corner at texel 0
uniform mat4 flareProj;
uniform vec2 flareViewport;	// pixels
uniform float flareSlack;	// view units a surface may sit in front of the light and still let it show
uniform int flareRed;		// 1 noocclude

out vec2 flareUV;
out vec3 flareColor;

void main()
{
	float seen = 1.0;
	if ( ( flareRed & 1 ) == 0 ) {
		ivec2 c = ivec2( ( vertexLight.xy * 0.5 + 0.5 ) * flareViewport );
		ivec2 hi = ivec2( flareViewport ) - 1;
		float n = 0.0;
		for ( int y = -2; y <= 2; y++ ) {
			for ( int x = -2; x <= 2; x++ ) {
				ivec2 p = clamp( c + ivec2( x, y ) * 2, ivec2( 0 ), hi );
				float d = texelFetch( flareDepth, p, 0 ).r;
				float sceneW = flareProj[3][2] / ( flareProj[2][2] + ( 2.0 * d - 1.0 ) );
				n += ( d >= 1.0 || sceneW + flareSlack >= vertexLight.z ) ? 1.0 : 0.0;
			}
		}
		seen = n / 25.0;
	}
	float size = vertexColor.a > 0.5 ? seen : 1.0;
	gl_Position = vec4( vertexPosition.xy + vertexPosition.zw * size, 0.0, 1.0 );
	flareUV = vertexUV.xy;
	flareColor = vertexColor.rgb * seen;
}
