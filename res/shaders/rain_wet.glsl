// lane FO76WEATHER1: rain wetness on every material, Fallout 76's law (src/gl/rainwetlaw.h). Set by
// src/gl/cellprecip.cpp wwCellRainWetUniforms: x the wet amount (0 = off, the old path), y the material's custom
// porosity, z its weight (BGSM bCustomPorosity). (No name "smooth": it is a GLSL keyword.)
uniform vec4 rainWet;

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
