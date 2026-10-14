/* lane VOLPART1: the air's medium, its phase and its six-axis cube, shared by the froxel passes (cell_volair.frag)
 * and the volumetric particles (particles.frag), so a steam puff is lit by the very law the air is. Needs
 * cell_lights.glsl included first (the GI grid and its rooms). Moved out of cell_volair.frag unchanged (lane VOLAIR1). */

uniform vec4 airMed;			// sigma0 (per game unit), zb, 1 / H (0: uniform), the top (the fog's height frame)
uniform vec4 volPhase[3];		// rgb: the weight of the air (Rayleigh), forward and back terms; w: the HG g (0 for air)
uniform vec4 volGi;				// rgb: the phase's tint (the three weights summed), w: its L1 g

const float PI = 3.14159265;

float volHg( float c, float g )
{
	float d = 1.0 + g * g - 2.0 * g * c;
	return ( 1.0 - g * g ) / ( 4.0 * PI * d * sqrt( d ) );
}

// pi x the phase (display units): c = the cosine between the light's travel and the scattered ray's (to the eye)
vec3 volPhasePi( float c )
{
	return PI * ( volPhase[0].rgb * ( 3.0 / ( 16.0 * PI ) ) * ( 1.0 + c * c )
		+ volPhase[1].rgb * volHg( c, volPhase[1].w ) + volPhase[2].rgb * volHg( c, volPhase[2].w ) );
}

// the column of air from zb up to z, per unit sigma0: F(z) = z - zb below zb, H (1 - exp(-(z - zb) / H)) above,
// constant above the top
float airF( float z )
{
	z = min( z, airMed.w );
	float x = z - airMed.y;
	if ( x <= 0.0 || airMed.z <= 0.0 )
		return x;
	float u = x * airMed.z;
	float e = u < 1e-3 ? u * ( 1.0 - u * ( 0.5 - u / 6.0 ) ) : 1.0 - exp( -u );
	return e / airMed.z;
}

float airRho( float z )
{
	if ( z > airMed.w )
		return 0.0;
	return airMed.z > 0.0 ? min( 1.0, exp( -( z - airMed.y ) * airMed.z ) ) : 1.0;
}

// the optical depth of a straight segment of length len (game units) from height za to height zb
float airOd( float za, float zb, float len )
{
	float dz = zb - za;
	if ( abs( dz ) < 0.5 )
		return airMed.x * len * airRho( 0.5 * ( za + zb ) );
	return airMed.x * len * ( airF( zb ) - airF( za ) ) / dz;
}

// 1 - exp(-tau), precise for small tau
float air1mE( float tau )
{
	return tau < 1e-3 ? tau * ( 1.0 - tau * ( 0.5 - tau / 6.0 ) ) : 1.0 - exp( -tau );
}

// the transmittance from height z toward a direction whose height grows mu per unit, to the top
float airTup( float z, float mu )
{
	if ( z >= airMed.w )
		return 1.0;
	mu = max( mu, 0.02 );
	return exp( -airMed.x * ( airF( airMed.w ) - airF( z ) ) / mu );
}

/* the six-axis cube at P, axis a = +X -X +Y -Y +Z -Z, in the surfaces' GI unit, with ONE room lookup (lane VOLFOG1b):
 * a froxel is a point in the air, not a surface -- the room read at P itself, one pass over the 8 voxels fetches all
 * six slabs of the room's slot; an axis with no weight takes the grid's plain trilinear. share[a] = the grid's valid
 * share there (the ambient's replace rule). */
void volCubeAll( vec3 P, out vec3 E[6], out float share[6] )
{
	vec4 s[6];
	float ws[6];
	for ( int a = 0; a < 6; a++ ) {
		s[a] = vec4( 0.0 );
		ws[a] = 0.0;
	}
	ivec3 dm = ivec3( cellGiDims );
	if ( cellGiRooms && ( cellPassRed & 8 ) == 0 ) {
		ivec2 L = cellRoomAt( P );
		if ( L.x >= 0 ) {
			vec3 g = ( P - cellGiOrigin ) / cellGiVoxel - 0.5;
			ivec3 i0 = ivec3( floor( g ) );
			vec3 f = g - vec3( i0 );
			for ( int k = 0; k < 8; k++ ) {
				ivec3 o = ivec3( k & 1, ( k >> 1 ) & 1, ( k >> 2 ) & 1 );
				ivec3 c = clamp( i0 + o, ivec3( 0 ), dm - 1 );
				vec3 t = mix( 1.0 - f, f, vec3( o ) );
				float w = t.x * t.y * t.z;
				ivec2 S = cellRoomUnpack( texelFetch( cellGiSlots, c, 0 ).r );
				int base = ( S.x >= 0 && ( S.x == L.x || S.x == L.y ) ) ? 0 : ( S.y >= 0 && ( S.y == L.x || S.y == L.y ) ) ? 6 * dm.z : -1;
				if ( base < 0 || w <= 0.0 )
					continue;
				for ( int a = 0; a < 6; a++ ) {
					vec4 v = texelFetch( cellGi, ivec3( c.xy, c.z + base + a * dm.z ), 0 );
					if ( v.a > 0.0 ) {
						s[a] += w * v;
						ws[a] += w;
					}
				}
			}
		}
	}
	vec3 g = ( P - cellGiOrigin ) / cellGiVoxel;
	vec2 xy = g.xy / cellGiDims.xy;
	float z = clamp( g.z, 0.5, cellGiDims.z - 0.5 );
	float depth = ( cellGiRooms ? 12.0 : 6.0 ) * cellGiDims.z;
	for ( int a = 0; a < 6; a++ ) {
		vec4 plain = texture( cellGi, vec3( xy, ( z + float( a ) * cellGiDims.z ) / depth ) );
		vec4 r = ws[a] > 0.0 ? s[a] / ws[a] : plain;
		// as cellGiE0: a sky grid keeps the valid share, else the mean over the valid share
		E[a] = cellGiSky ? max( r.rgb, vec3( 0.0 ) ) : ( r.a > 0.01 ? max( r.rgb / r.a, vec3( 0.0 ) ) : vec3( 0.0 ) );
		share[a] = clamp( plain.a, 0.0, 1.0 );
	}
}

// L0 + L1 of a six-axis cube of irradiances (display units), scattered toward the eye along dW (the view ray's
// direction, world): 1/4 x max(0, (2/3) sum E + 3 g (E+ - E-).d) x the phase's tint
vec3 volCubeScatter( vec3 E[6], vec3 dW )
{
	vec3 fl = ( 2.0 / 3.0 ) * ( E[0] + E[1] + E[2] + E[3] + E[4] + E[5] );
	vec3 l1 = ( E[0] - E[1] ) * dW.x + ( E[2] - E[3] ) * dW.y + ( E[4] - E[5] ) * dW.z;
	return 0.25 * max( fl + 3.0 * volGi.w * l1, vec3( 0.0 ) ) * volGi.rgb;
}
