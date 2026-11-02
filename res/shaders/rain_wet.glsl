// lane FO76WEATHER1: rain wetness on every material, Fallout 76's law (src/gl/rainwetlaw.h). Set by
// src/gl/cellprecip.cpp wwCellRainWetUniforms: x the wet amount (0 = off, the old path), y the material's custom
// porosity, z its weight (BGSM bCustomPorosity), w 1 = test the cover map (lane FO76WEATHER2). (No name "smooth": it
// is a GLSL keyword.)
uniform vec4 rainWet;
// lane FO76WEATHER2: the cover map (src/gl/preciproof.h WwRoofMap, R32F: the highest z over a texel, -1e30 open),
// rainCoverGrid = ( world x, y of texel (0, 0)'s corner, 1 / texel side, texel side )
uniform sampler2D rainCover;
uniform vec4 rainCoverGrid;

// the wetness of a pixel of this smoothness; metal 1 never wets
float wwRainWetW( float gloss, float metal )
{
	float por = clamp( mix( 1.1666 - 1.666 * gloss, rainWet.y, rainWet.z ), 0.0, 1.0 );
	return rainWet.x * por * ( 1.0 - metal );
}

// 1 where the reflectance averages above about 0.5
float wwRainWetMetal( vec3 f0 )
{
	return clamp( dot( f0, vec3( 0.33 ) ) * 1000.0 - 500.0, 0.0, 1.0 );
}

float wwRainWetSmooth( float gloss, float w )
{
	return 1.0 - ( 1.0 - 0.4 * w ) * ( 1.0 - gloss );
}

// rainwetlaw.h coverTexelOpen: off the map or low enough = open
float wwRainCoverOpen( ivec2 t, ivec2 n, float z )
{
	if ( any( lessThan( t, ivec2( 0 ) ) ) || any( greaterThanEqual( t, n ) ) )
		return 1.0;
	return texelFetch( rainCover, t, 0 ).r <= z ? 1.0 : 0.0;
}

// rainwetlaw.h exposed: 1 open sky, 0 under a roof or a bridge (P world, N the unit surface normal, world)
float wwRainWetExposed( vec3 P, vec3 N )
{
#ifndef RAIN_COVER_RED
	if ( rainWet.w < 0.5 )
		return 1.0;
	vec2 q = ( P.xy + N.xy * ( 1.5 * rainCoverGrid.w ) - rainCoverGrid.xy ) * rainCoverGrid.z - 0.5;
	ivec2 t = ivec2( floor( q ) );
	vec2 f = q - vec2( t );
	ivec2 n = textureSize( rainCover, 0 );
	float z = P.z + 8.0 + rainCoverGrid.w * min( length( N.xy ) / max( abs( N.z ), 0.05 ), 2.0 );
	float e00 = wwRainCoverOpen( t, n, z ), e10 = wwRainCoverOpen( t + ivec2( 1, 0 ), n, z );
	float e01 = wwRainCoverOpen( t + ivec2( 0, 1 ), n, z ), e11 = wwRainCoverOpen( t + ivec2( 1, 1 ), n, z );
	return mix( mix( e00, e10, f.x ), mix( e01, e11, f.x ), f.y );
#else
	return 1.0;	// tests/raincover red: no cover test (FO76WEATHER1)
#endif
}
