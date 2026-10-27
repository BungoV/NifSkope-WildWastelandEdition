#version 410 core

// lane WATERLOOK1 (src/gl/cellocean.cpp): each tile of the transformed fields into its layer of the two arrays (ocean_layers.geom) the water
// reads (mipmapped after): B = ( sx, sy, Dxx, Dyy ), C = ( sx^2, sy^2, sx sy, Dxy ); C's mips are the slopes' second
// moments, so a pixel's lost slope variance is C.xy - B.xy^2 (LEAN mapping, Olano and Baker 2010).

uniform sampler2D oceanX0;	// ( sx, sy, Dxx, Dyy ) after both directions
uniform sampler2D oceanX1;	// ( Dxy, h, Dx, Dy )
flat in int oceanLayerG;	// the layer drawn (ocean_layers.geom) = the tile read

layout ( location = 0 ) out vec4 oB;
layout ( location = 1 ) out vec4 oC;

void main()
{
	ivec2 p = ivec2( gl_FragCoord.xy ) + ivec2( 128 * oceanLayerG, 0 );
	vec4 x0 = texelFetch( oceanX0, p, 0 );
	vec4 x1 = texelFetch( oceanX1, p, 0 );
	oB = x0;
	oC = vec4( x0.x * x0.x, x0.y * x0.y, x0.x * x0.y, x1.x );
}
