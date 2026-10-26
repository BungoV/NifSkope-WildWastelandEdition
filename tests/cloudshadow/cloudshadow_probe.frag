#version 410 core

// lane CLOUD3: the offline gate's receiver probe (tests/spells/cloudshadow_check.py). It calls the receivers' own code
// (ww_sunshadow.glsl) on a grid of points: r = ussCloudT (the clouds), g = wwSunShadow (surfaces), b = wwSunShadowGeo.
// prMode 0: a level grid at height prPlane.x; 1: a vertical slice through (prPlane.y, prPlane.z) along prAxis.

#include "../../res/shaders/ww_sunshadow.glsl"

uniform int prMode;
uniform vec4 prGrid;	// x0, y0 (or z0), dx, dy (or dz), world units
uniform vec3 prPlane;
uniform vec3 prAxis;

out vec4 fragColor;

void main()
{
	vec2 ij = gl_FragCoord.xy - 0.5;
	vec3 p;
	if ( prMode == 0 )
		p = vec3( prGrid.x + ij.x * prGrid.z, prGrid.y + ij.y * prGrid.w, prPlane.x );
	else
		p = vec3( prPlane.y, prPlane.z, 0.0 ) + prAxis * ( prGrid.x + ij.x * prGrid.z ) + vec3( 0.0, 0.0, prGrid.y + ij.y * prGrid.w );
	fragColor = vec4( ussCloudT( p ), wwSunShadow( p ), wwSunShadowGeo( p ), 1.0 );
}
