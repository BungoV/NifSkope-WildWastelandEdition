// lane GRASSMB1: the game's grass fade on the cell view's grass shapes (src/gl/cellgrass.h).
// x = fGrassStartFadeDistance, y = fGrassFadeRange (both in view units), z = 1 on a grass shape and 0 on every
// other shape, where the factor is exactly 1.0 and the picture is the before-lane picture.
uniform vec3 wwGrassFade;

float wwGrassFadeK( vec3 v )
{
	if ( wwGrassFade.z < 0.5 )
		return 1.0;
	return 1.0 - clamp( ( length( v ) - wwGrassFade.x ) / max( wwGrassFade.y, 1e-3 ), 0.0, 1.0 );
}
