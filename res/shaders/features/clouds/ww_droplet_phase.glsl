// lane CLOUD2: the droplet phase for clouds and fog (Jendersie & d'Eon 2023), HG + Draine blend. prm = (gHG, gD,
// alpha, wD) from src/features/clouds/cloudvolmath.h dropletParams(d); k scales both g (the octaves' c^i). Twin: dropletPhase.

float wwHg( float g, float c )
{
	return ( 1.0 - g * g ) / ( 12.5663706 * pow( max( 1e-4, 1.0 + g * g - 2.0 * g * c ), 1.5 ) );
}

float wwDropletPhase( vec4 prm, float c, float k )
{
	float gh = prm.x * k, gd = prm.y * k;
	float dr = wwHg( gd, c ) * ( 1.0 + prm.z * c * c ) / ( 1.0 + prm.z * ( 1.0 + 2.0 * gd * gd ) / 3.0 );
	return ( 1.0 - prm.w ) * wwHg( gh, c ) + prm.w * dr;
}

// lane CLOUDRECX1: the ice phase (clouds.iceFraction, SPEC.md 1.4): the roughened crystals' body (HG 0.75), the smooth
// faces' forward peak (HG 0.98, 10 %) and the 22 deg halo (a 1 deg Gaussian ring, 3 %); k as the octaves'. Twin and
// sources: src/features/clouds/cloudrecx14math.h icePhase.
float wwIcePhase( float c, float k )
{
	float a = acos( clamp( c, -1.0, 1.0 ) ) - 0.383972;
	float ring = exp( -( a * a ) / ( 0.0174533 * 0.0174533 ) ) / 0.0728124;
	return 0.87 * wwHg( 0.75 * k, c ) + 0.10 * wwHg( 0.98 * k, c ) + 0.03 * ( k * ring + ( 1.0 - k ) / 12.5663706 );
}
