/* lane PERF1: the GI volumes' shapes (src/proberooms.h GiVolumeShape), 3 RGBA32F texels each: (center xyz, type),
 * (size xyz, room), (rotation xyz degrees, 0). Types: 0 box (size = edges), 1 cylinder (x, y diameters, height on z),
 * 2 wedge (full height at -y, none at +y), 3 capsule (diameter on x, y; end to end on z), 4 sphere (x, y, z diameters). */

uniform samplerBuffer volShapes;

// shape -> world: Rz Ry Rx
mat3 volRot( vec3 deg )
{
	vec3 r = radians( deg );
	vec3 c = cos( r ), s = sin( r );
	mat3 X = mat3( 1.0, 0.0, 0.0, 0.0, c.x, s.x, 0.0, -s.x, c.x );
	mat3 Y = mat3( c.y, 0.0, -s.y, 0.0, 1.0, 0.0, s.y, 0.0, c.y );
	mat3 Z = mat3( c.z, s.z, 0.0, -s.z, c.z, 0.0, 0.0, 0.0, 1.0 );
	return Z * Y * X;
}

// l: the point in the shape's own frame (centered, unrotated)
bool volInside( vec3 l, int type, vec3 s )
{
	vec3 h = max( 0.5 * s, vec3( 1.0e-3 ) );
	if ( type == 4 )
		return dot( l / h, l / h ) <= 1.0;
	if ( type == 3 ) {
		float r = min( h.x, h.y );
		float hz = max( h.z - r, 0.0 );
		vec3 d = vec3( l.xy, l.z - clamp( l.z, -hz, hz ) );
		return dot( d, d ) <= r * r;
	}
	if ( any( greaterThan( abs( l ), h ) ) )
		return false;
	if ( type == 1 )
		return dot( l.xy / h.xy, l.xy / h.xy ) <= 1.0;
	if ( type == 2 )
		return l.z + h.z <= 2.0 * h.z * ( 0.5 - 0.5 * l.y / h.y );
	return true;
}
