/* lane CLOUD6e (RDR2 gap e, Bauer 2019 slide 19): the rain fog under the rain clouds, read by the air's froxel pass
 * (cell_volair.frag: one read a sub-step, added to its optical depth). The law and its numbers: src/gl/rainfogmath.h;
 * the uniforms: src/gl/cellvolair.cpp. rfOn 0 (a weather that does not rain, the red, no cloud map, indoors): never
 * read, the air exactly as before. */

uniform int rfOn;			// 1: the rain fog this frame
uniform sampler2D rfMap;	// CLOUD1's cloud map (R: the Low band's coverage), linear, clamped
uniform mat4 rfFromView;	// view space -> (u, v, the height below the Low band's base in km)
uniform vec4 rfLaw;			// x sigma per game unit near the ground (the rain's share in), y the base excess,
							// z 1 / the falloff (km), w 1 / the fade above the base (km)
uniform vec2 rfFar;			// x the band's coverage past the map, y the edge band (uv) where the map fades to it

// the rain fog's extinction per game unit at view point posView
float rfSigma( vec3 posView )
{
	vec3 q = ( rfFromView * vec4( posView, 1.0 ) ).xyz;
	vec2 e = abs( q.xy - 0.5 ) * 2.0;
	float c = mix( textureLod( rfMap, q.xy, 0.0 ).r, rfFar.x, smoothstep( 1.0 - rfFar.y, 1.0, max( e.x, e.y ) ) );
	float p = q.z >= 0.0 ? 1.0 + rfLaw.y * exp( -q.z * rfLaw.z ) : ( 1.0 + rfLaw.y ) * max( 1.0 + q.z * rfLaw.w, 0.0 );
	return rfLaw.x * c * p;
}
