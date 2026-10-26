// lane CLOUD2: the droplet phase for clouds and fog (Jendersie & d'Eon 2023), HG + Draine blend. prm = (gHG, gD,
// alpha, wD) from src/gl/cloudvolmath.h dropletParams(d); k scales both g (the octaves' c^i). Twin: dropletPhase.

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
