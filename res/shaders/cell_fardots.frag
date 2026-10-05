#version 410 core

// lane FARVIEW1: a bulb's Gaussian, added into the linear frame (cell_fardots.vert)

in vec3 dotValue;
in float dotSigma;
out vec4 fragColor;

void main()
{
	vec2 r = ( gl_PointCoord - 0.5 ) * ( 2.0 * ceil( 3.0 * dotSigma ) + 1.0 );
	float g = exp( -dot( r, r ) / ( 2.0 * dotSigma * dotSigma ) );
	if ( g < 0.011 )	// past 3 sigma: the sprite's corners stay the sky's
		discard;
	fragColor = vec4( dotValue * g, 0.0 );
}
