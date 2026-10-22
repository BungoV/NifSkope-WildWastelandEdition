/* lane VOLPART1: the air's medium, its phase, its six-axis cube and its records' colour, shared by the froxel passes
 * (cell_volair.frag) and the volumetric particles and effect cards (cell_volpart.glsl), so a steam puff is lit by the
 * very law the air is. Needs lookdev_fog.glsl (wwAirRhoLaw, wwAirColLaw, wwAirRecordColour) and cell_lights.glsl (the
 * GI grid and its rooms) included first. Moved out of cell_volair.frag unchanged (the landed VOLAIR1). */

uniform vec4 airMed;			// sigma0 (per game unit), zb, 1 / H (0: uniform), the top (the fog's height frame)
uniform vec4 airLayer;			// the near band's share w, zn, 1 / Hn, the ground layer's strength s (cellvolair.cpp VaLayer)
uniform vec4 airGround;			// the ground layer's anchor zg, 1 / its thickness
uniform int volRed;				// 2 gioff, 4 flat, 8 noshadow, 16 nolights, 256 physcol, 262144 lightmid, 524288 bluescatter, 4194304 roomstep (1 off, 32 wrongmed, 128 farfit: the CPU's)
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

// the density per unit sigma0 (lookdev_fog.glsl wwAirRhoLaw: the far band, the near band's share, the ground layer)
float airRho( float z )
{
	return wwAirRhoLaw( z, airMed, airLayer, airGround );
}

// (1 - exp(-u)) / u, precise for small u
float airE1( float u )
{
	return u < 1e-3 ? 1.0 - u * ( 0.5 - u / 6.0 ) : ( 1.0 - exp( -u ) ) / u;
}

// the column of air between heights lo < hi, per unit sigma0, without the cancellation of F(hi) - F(lo): F is a
// large number (the column from zb, up to H ~ 1e6 units), its difference over a few units lost to float precision
// near the horizon (the gate's R found it at Sanctuary's foggy band)
float airCol( float lo, float hi )
{
	return wwAirColLaw( lo, hi, airMed, airLayer, airGround );
}

// the optical depth of a straight segment of length len (game units) from height za to height zb
float airOd( float za, float zb, float len )
{
	float dz = zb - za;
	if ( abs( dz ) < 0.5 )
		return airMed.x * len * airRho( 0.5 * ( za + zb ) );
	return airMed.x * len * airCol( min( za, zb ), max( za, zb ) ) / abs( dz );
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
	return exp( -airMed.x * airCol( z, airMed.w ) / mu );
}

/* the six-axis cube at P, axis a = +X -X +Y -Y +Z -Z, in the surfaces' GI unit (lane VOLFOG1b): a froxel is a point in
 * the air, not a surface; the voxels of P's room only, an axis with no weight takes the grid's plain trilinear.
 * share[a] = the grid's valid share there (the ambient's replace rule).
 * FOGFIX1 (bungo 10-16, the squares in the Concord roof fog): one room label read at P switched at every fine room cell
 * and at the room grid's box, and a room with one voxel left held that voxel flat -- hard-edged boxes in the air. Now the
 * 8 room cells around P are blended trilinearly, each label's answer fading to the plain trilinear below a quarter of
 * the voxel weight; solid cells drop out, cells outside the grid read plain. Continuous, and still no voxel of another
 * room. Red WW_CELL_AIR_RED=roomstep: the one label at P, as before. */
vec4 volRoomAxis( ivec3 c0, vec3 f, ivec2 L, int a, vec4 plain, ivec3 dm )
{
	vec4 s = vec4( 0.0 );
	float ws = 0.0;
	for ( int k = 0; k < 8; k++ ) {
		ivec3 o = ivec3( k & 1, ( k >> 1 ) & 1, ( k >> 2 ) & 1 );
		ivec3 c = clamp( c0 + o, ivec3( 0 ), dm - 1 );
		vec3 t = mix( 1.0 - f, f, vec3( o ) );
		float w = t.x * t.y * t.z;
		ivec2 S = cellRoomUnpack( texelFetch( cellGiSlots, c, 0 ).r );
		int base = ( S.x >= 0 && ( S.x == L.x || S.x == L.y ) ) ? 0 : ( S.y >= 0 && ( S.y == L.x || S.y == L.y ) ) ? 6 * dm.z : -1;
		if ( base < 0 || w <= 0.0 )
			continue;
		vec4 v = texelFetch( cellGi, ivec3( c.xy, c.z + base + a * dm.z ), 0 );
		if ( v.a > 0.0 ) {
			s += w * v;
			ws += w;
		}
	}
	if ( ( volRed & 4194304 ) != 0 )
		return ws > 0.0 ? s / ws : plain;
	const float wMin = 0.25;
	return ws >= wMin ? s / ws : ( s + ( wMin - ws ) * plain ) / wMin;
}

void volCubeAll( vec3 P, out vec3 E[6], out float share[6] )
{
	ivec3 dm = ivec3( cellGiDims );
	vec3 g = ( P - cellGiOrigin ) / cellGiVoxel;
	vec2 xy = g.xy / cellGiDims.xy;
	float z = clamp( g.z, 0.5, cellGiDims.z - 0.5 );
	float depth = float( textureSize( cellGi, 0 ).z );	// lanes EMISSIVEGRID1 + SPECOCC1: the fine atlas / sky slabs may follow
	vec4 plain[6];
	vec4 r[6];
	for ( int a = 0; a < 6; a++ ) {
		plain[a] = texture( cellGi, vec3( xy, ( z + float( a ) * cellGiDims.z ) / depth ) );
		r[a] = plain[a];
	}
	if ( cellGiRooms && ( cellPassRed & 8 ) == 0 ) {
		vec3 gv = g - 0.5;
		ivec3 c0 = ivec3( floor( gv ) );
		vec3 f = gv - vec3( c0 );
		if ( ( volRed & 4194304 ) != 0 ) {
			ivec2 L = cellRoomAt( P );
			if ( L.x >= 0 )
				for ( int a = 0; a < 6; a++ )
					r[a] = volRoomAxis( c0, f, L, a, plain[a], dm );
		} else {
			// the 8 room cells around P: labels, trilinear weights; solid cells drop out, outside the grid = plain
			vec3 gr = ( P - cellRoomsOrigin ) / cellRoomsCell - 0.5;
			ivec3 r0 = ivec3( floor( gr ) );
			vec3 fr = gr - vec3( r0 );
			ivec2 Lk[8];
			float wk[8];
			float wAir = 0.0;
			bool same = true;
			for ( int k = 0; k < 8; k++ ) {
				ivec3 o = ivec3( k & 1, ( k >> 1 ) & 1, ( k >> 2 ) & 1 );
				ivec3 c = r0 + o;
				vec3 t = mix( 1.0 - fr, fr, vec3( o ) );
				wk[k] = t.x * t.y * t.z;
				if ( any( lessThan( c, ivec3( 0 ) ) ) || any( greaterThanEqual( c, ivec3( cellRoomsDims ) ) ) )
					Lk[k] = ivec2( -2 );	// outside the room grid: the plain read
				else
					Lk[k] = cellRoomUnpack( texelFetch( cellRooms, c, 0 ).r );
				if ( Lk[k].x == -1 )
					wk[k] = 0.0;	// solid
				wAir += wk[k];
				if ( k > 0 && Lk[k] != Lk[0] )
					same = false;
			}
			if ( wAir > 0.0 ) {
				if ( same ) {
					if ( Lk[0].x >= 0 )
						for ( int a = 0; a < 6; a++ )
							r[a] = volRoomAxis( c0, f, Lk[0], a, plain[a], dm );
				} else {
					for ( int a = 0; a < 6; a++ )
						r[a] = vec4( 0.0 );
					for ( int k = 0; k < 8; k++ ) {
						if ( wk[k] <= 0.0 )
							continue;
						bool seen = false;
						for ( int j = 0; j < k; j++ )
							seen = seen || ( wk[j] > 0.0 && Lk[j] == Lk[k] );
						if ( seen )
							continue;
						float wl = 0.0;
						for ( int j = k; j < 8; j++ )
							wl += Lk[j] == Lk[k] ? wk[j] : 0.0;
						wl /= wAir;
						for ( int a = 0; a < 6; a++ )
							r[a] += wl * ( Lk[k].x >= 0 ? volRoomAxis( c0, f, Lk[k], a, plain[a], dm ) : plain[a] );
					}
				}
			}
		}
	}
	for ( int a = 0; a < 6; a++ ) {
		// as cellGiE0: a sky grid keeps the valid share, else the mean over the valid share
		E[a] = cellGiSky ? max( r[a].rgb, vec3( 0.0 ) ) : ( r[a].a > 0.01 ? max( r[a].rgb / r[a].a, vec3( 0.0 ) ) : vec3( 0.0 ) );
		share[a] = clamp( plain[a].a, 0.0, 1.0 );
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

/* the haze's colour from the records (owner's ruling 10-14): the game's own fog colour at this distance and height
 * (the WTHR / lighting template near, far and high colours by hour, wwFogEval) mixed toward the fog-sun colour as the
 * game mixes it (fDirectionalFogPower), at unit luminance; the physical light keeps its brightness and direction.
 * Applied to the sky's share (the directional light, the GI, the ambient); the placed lights keep their own records'
 * colours. Red physcol (256): the physical colour (the phase's tint x the light's). */
vec3 airEnvColour( vec3 J, float t, float z, vec3 dirView )
{
	if ( ( volRed & 256 ) != 0 )
		return J;
	bool ok;
	vec3 c = wwAirRecordColour( t, z, dirView, ok );	// lookdev_fog.glsl, at unit luminance
	return ok ? dot( J, vec3( 0.2126, 0.7152, 0.0722 ) ) * c : J;
}
