#version 410 core

/* lane FARVIEW1: one bulb a point. World -> view by dotRow (rows of the view transform), view -> clip by dotProj.
 * Its value spreads over a normalised Gaussian of sigma = max(0.6 px, the bulb's projected size): a pixel shows
 * I x (f / d)^2 x G / (2 pi sigma^2), so the dot's sum over the screen is the light that reaches the camera.
 * Faded in by w = smoothstep(band) of the camera distance, as the surfel light is. */

layout ( location = 0 ) in vec3 vertexPosition;	// world
layout ( location = 1 ) in vec3 vertexColor;	// the bulb's intensity I (the frame's linear units x area)

uniform vec4 dotRow[3];
uniform mat4 dotProj;
uniform vec3 dotCam;	// the camera, world
uniform vec2 dotBand;
uniform float dotFocal;	// f, pixels
uniform float dotBulb;	// the bulb radius, world units
uniform int dotRed;	// 1 = never faded (the dots at every distance)

out vec3 dotValue;
out float dotSigma;

void main()
{
	vec4 w = vec4( vertexPosition, 1.0 );
	vec3 v = vec3( dot( dotRow[0], w ), dot( dotRow[1], w ), dot( dotRow[2], w ) );
	gl_Position = dotProj * vec4( v, 1.0 );
	float d = max( length( vertexPosition - dotCam ), 1.0 );
	float fade = ( dotRed & 1 ) != 0 ? 1.0 : smoothstep( dotBand.x, dotBand.y, d );
	dotSigma = max( 0.6, dotFocal * dotBulb / d );
	float k = dotFocal / d;
	dotValue = fade * vertexColor * k * k / ( 6.2831853 * dotSigma * dotSigma );
	gl_PointSize = fade > 0.0 ? min( 2.0 * ceil( 3.0 * dotSigma ) + 1.0, 63.0 ) : 0.0;
}
