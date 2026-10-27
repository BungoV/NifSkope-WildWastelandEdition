#version 410 core

// lane WATERLOOK1 (src/gl/cellocean.cpp): one mip level of the three arrays, all six layers in one draw (ocean_layers.geom),
// the 2 x 2 mean of the level above (C's mean of squares keeps the second moments the water's lost-variance term
// reads). Seven draws instead of glGenerateMipmap on the arrays: the whole build is 0.14 ms with these, 0.6-0.85 ms with
// glGenerateMipmap (notes/waterlook1/tools/ocean_gl.py). Both arrays' base and max level are the level read while a
// level draws (no feedback loop), so lod 0 below.

uniform sampler2DArray oceanB;
uniform sampler2DArray oceanC;
uniform sampler2DArray oceanD;	// step 2: the heights and the chop
flat in int oceanLayerG;	// the layer drawn (ocean_layers.geom)

layout ( location = 0 ) out vec4 o0;
layout ( location = 1 ) out vec4 o1;
layout ( location = 2 ) out vec4 o2;

void main()
{
	ivec2 p = 2 * ivec2( gl_FragCoord.xy );
	o0 = 0.25 * ( texelFetch( oceanB, ivec3( p, oceanLayerG ), 0 ) + texelFetch( oceanB, ivec3( p + ivec2( 1, 0 ), oceanLayerG ), 0 )
		+ texelFetch( oceanB, ivec3( p + ivec2( 0, 1 ), oceanLayerG ), 0 ) + texelFetch( oceanB, ivec3( p + ivec2( 1, 1 ), oceanLayerG ), 0 ) );
	o1 = 0.25 * ( texelFetch( oceanC, ivec3( p, oceanLayerG ), 0 ) + texelFetch( oceanC, ivec3( p + ivec2( 1, 0 ), oceanLayerG ), 0 )
		+ texelFetch( oceanC, ivec3( p + ivec2( 0, 1 ), oceanLayerG ), 0 ) + texelFetch( oceanC, ivec3( p + ivec2( 1, 1 ), oceanLayerG ), 0 ) );
	o2 = 0.25 * ( texelFetch( oceanD, ivec3( p, oceanLayerG ), 0 ) + texelFetch( oceanD, ivec3( p + ivec2( 1, 0 ), oceanLayerG ), 0 )
		+ texelFetch( oceanD, ivec3( p + ivec2( 0, 1 ), oceanLayerG ), 0 ) + texelFetch( oceanD, ivec3( p + ivec2( 1, 1 ), oceanLayerG ), 0 ) );
}
