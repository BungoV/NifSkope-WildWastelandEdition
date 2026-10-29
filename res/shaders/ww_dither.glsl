/* lane FOGGLOW1 (bungo 10-29, Memory Den fog banding): a display value goes into an 8-bit target (the view, the TAA
 * picture) as one of 256 levels. A smooth dark gradient (the night fog on a wall, the glow round a lamp) then steps in
 * flat plateaus a level apart: the bands and rings. A triangular (TPDF) noise of +-1 level added before the write
 * turns the steps into a fine grain whose mean is the true value (Gjol, "Banding in Games: A Noisy Rant", Playdead,
 * 2016). The noise fades to 0 at exactly 0 and 1, so black stays black and white white.
 * wwDitherOn 0 (WW_CELL_DITHER=0, the red): the value unchanged. wwDitherSeed: the TAA frame index (a camera path
 * renders frame k as k: a repeated frame is the same picture), so TAA averages a different noise every frame. */

uniform int wwDitherOn;
uniform int wwDitherSeed;

// a uniform [0, 1) from a pixel and a seed (an integer hash: no lattice, no pattern a zoom shows)
float wwDitherHash( uvec2 p, uint s )
{
	uint h = p.x * 1664525u + p.y * 1013904223u + s * 2654435769u;
	h ^= h >> 16;
	h *= 2246822519u;
	h ^= h >> 13;
	h *= 3266489917u;
	h ^= h >> 16;
	return float( h >> 8 ) * ( 1.0 / 16777216.0 );
}

vec3 wwDither8( vec3 c, vec2 fragCoord )
{
	if ( wwDitherOn == 0 )
		return c;
	uvec2 p = uvec2( fragCoord );
	uint s = uint( wwDitherSeed );
	float n = wwDitherHash( p, 2u * s ) + wwDitherHash( p, 2u * s + 1u ) - 1.0;	// triangular on (-1, 1)
	vec3 w = clamp( min( c, 1.0 - c ) * 255.0, 0.0, 1.0 );
	return c + n * w * ( 1.0 / 255.0 );
}
