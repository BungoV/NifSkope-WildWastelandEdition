#version 410 core

/* lane VOLAIR1: the air, froxel by froxel (src/gl/cellvolair.h). One draw writes one layer k of a (columns x rows x
 * slices) volume; the slices are exponential from volSlices.x (near) over ln(far / near) = volSlices.y in volSlices.z
 * steps (game units along the view ray).
 *   stage 1  inject: the medium sigma_t(z) = sigma0 x rho(z), rho = min(1, exp(-(z - zb) / H)) (airMed: sigma0, zb,
 *            1 / H, the top z above which the air is empty; z in the fog's height frame). Four equal sub-steps across
 *            slice k; each adds T x J x (1 - exp(-tau)) and multiplies T by exp(-tau), tau the sub-step's optical
 *            depth in closed form. J = the light the air scatters toward the eye (display units, pi x radiance):
 *              1 the directional light x its cascade shadow x the air's transmittance up to the top x pi x phase
 *              2 every placed light (colour x radial x cone x its shadow cube) x the air's transmittance to it x pi x phase
 *              4 the probe GI and 8 the ambient (the share the GI grid leaves), each read as L0 + L1 of a six-axis cube
 *            The directional light is evaluated per sub-step (the shafts), the rest once at the slice's middle.
 *            rgb = the slice's light from its near edge, a = the slice's transmittance.
 *   stage 2  integrate: L_k = L_(k-1) + T_(k-1) x inj_k.rgb, T_k = T_(k-1) x inj_k.a. */

#define WW_CELL_FX 1
#include "lookdev_fog.glsl"
#include "ww_sunshadow.glsl"
#include "cell_lights.glsl"

uniform int volStage;			// 1 inject, 2 integrate
uniform int volLayer;
uniform ivec3 volDims;			// columns, rows, slices
uniform vec2 volProj;			// the projection's [0][0], [1][1]
uniform sampler3D volInject;	// stage 2 reads stage 1's volume
uniform vec4 airMed;			// sigma0 (per game unit), zb, 1 / H (0: uniform), the top (the fog's height frame)
uniform bool volCsm;			// the sun's cascades are drawn this frame
uniform int volRed;				// 2 gioff, 4 flat, 8 noshadow, 16 nolights (1 off and 32 wrongmed are the CPU's)
uniform vec4 volPhase[3];		// rgb: the weight of the air (Rayleigh), forward and back terms; w: the HG g (0 for air)
uniform vec4 volGi;				// rgb: the phase's tint (the three weights summed), w: its L1 g
uniform vec3 volFlat[6];		// red flat: one cube for every froxel (the grid's mean)
uniform int volTerms;			// gates only: 1 the directional light, 2 the placed lights, 4 the GI, 8 the ambient

out vec4 fragColor;

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

// the directional light the air at posView (height z) scatters toward the eye
vec3 airSun( vec3 posView, vec3 dirView, float z )
{
	if ( ( volTerms & 1 ) == 0 )
		return vec3( 0.0 );
	if ( cellOn && cellHasDir ) {
		// the cell's directional light (outdoors the weather's sun, indoors the lighting template's): world space
		vec3 to = normalize( cellDirTo );
		float vis = ( !cellInterior && volCsm && ( volRed & 8 ) == 0 ) ? wwSunShadow( posView ) : 1.0;
		return cellDirColor * vis * airTup( z, to.z ) * volPhasePi( dot( to, cellWorldDir( dirView ) ) );
	}
	if ( fogSun.w > 0.0 ) {
		// Lookdev without the cell's lights: its sun (view space)
		float vis = ( volCsm && ( volRed & 8 ) == 0 ) ? wwSunShadow( posView ) : 1.0;
		float mu = dot( fogView.xyz, fogSun.xyz ) / length( fogView.xyz );
		return fogSunColour.rgb * fogSun.w * vis * airTup( z, mu ) * volPhasePi( dot( dirView, fogSun.xyz ) );
	}
	return vec3( 0.0 );
}

// the rest of the light at posView (height z): the placed lights, the GI and the ambient, once a froxel
vec3 airRest( vec3 posView, vec3 dirView, float z )
{
	if ( !cellOn )
		return vec3( 0.0 );
	vec3 J = vec3( 0.0 );
	vec3 P = cellWorldPos( posView );
	vec3 dW = cellWorldDir( dirView );
	float zOff = z - P.z;	// the fog's height frame minus world z
	if ( ( volTerms & 2 ) != 0 && ( volRed & 16 ) == 0 ) {
		for ( int i = 0; i < cellLightCount; i++ ) {
			vec4 t0 = texelFetch( cellLights, i * CELL_TPL );
			float d = length( t0.xyz - P );
			if ( d >= t0.w || d < 0.001 )
				continue;
			vec3 L = ( t0.xyz - P ) / d, Lo;
			bool noSpec;
			vec3 e = cellLightE( i, P, L, Lo, noSpec );	// N = L: no N.L, the shadow lifted toward the light
			if ( all( equal( e, vec3( 0.0 ) ) ) )
				continue;
			J += e * exp( -airOd( z, t0.z + zOff, d ) ) * volPhasePi( dot( L, dW ) );
		}
	}
	bool wantGi = ( volTerms & 4 ) != 0 && cellGiOn && ( volRed & 2 ) == 0;
	bool ambDalc = cellInterior ? cellHasDalc : cellExtDalc;
	bool wantAmb = ( volTerms & 8 ) != 0 && ambDalc && !( cellInterior && cellGiAmb == 2 );
	if ( !wantGi && !wantAmb )
		return J;
	vec3 E[6];
	float share[6];
	for ( int a = 0; a < 6; a++ ) {
		E[a] = vec3( 0.0 );
		share[a] = 0.0;
	}
	if ( cellGiOn ) {
		if ( ( volRed & 4 ) != 0 ) {
			for ( int a = 0; a < 6; a++ )
				E[a] = volFlat[a];
		} else {
			volCubeAll( P, E, share );
		}
	}
	if ( wantGi ) {
		vec3 Eg[6];
		for ( int a = 0; a < 6; a++ )
			Eg[a] = E[a] * ( 1.0 / PI );	// the surfaces' GI unit -> the ambient's (cellLit: gi = cellGiE / pi)
		J += volCubeScatter( Eg, dW );
	}
	if ( wantAmb ) {
		// the share the GI replaced: indoors under the rule replace (the default), outdoors where the grid holds the sky
		bool replace = cellGiOn && ( cellInterior ? cellGiAmb == 1 : cellGiSky );
		const vec3 axes[6] = vec3[6]( vec3( 1, 0, 0 ), vec3( -1, 0, 0 ), vec3( 0, 1, 0 ), vec3( 0, -1, 0 ), vec3( 0, 0, 1 ), vec3( 0, 0, -1 ) );
		vec3 A[6];
		for ( int a = 0; a < 6; a++ )
			A[a] = cellAmbient( axes[a], P ) * ( replace ? 1.0 - share[a] : 1.0 );
		J += volCubeScatter( A, dW );
	}
	return J;
}

void main()
{
	ivec2 col = ivec2( gl_FragCoord.xy );
	if ( volStage == 2 ) {
		vec3 L = vec3( 0.0 );
		float T = 1.0;
		for ( int j = 0; j <= volLayer; j++ ) {
			vec4 s = texelFetch( volInject, ivec3( col, j ), 0 );
			L += T * s.rgb;
			T *= s.a;
		}
		fragColor = vec4( L, T );
		return;
	}
	vec2 ndc = ( vec2( col ) + 0.5 ) / vec2( volDims.xy ) * 2.0 - 1.0;
	vec3 dirView = normalize( vec3( ndc.x / volProj.x, ndc.y / volProj.y, -1.0 ) );
	float t0 = volSlices.x * exp( volSlices.y * float( volLayer ) / volSlices.z );
	float t1 = volSlices.x * exp( volSlices.y * float( volLayer + 1 ) / volSlices.z );
	// height along the ray: z(t) = zc + dzdt x t (t in game units)
	float zc = fogView.w;
	float dzdt = dot( fogView.xyz, dirView ) / fogDistScale;
	float tm = 0.5 * ( t0 + t1 );
	vec3 Jrest = airRest( dirView * ( tm / fogDistScale ), dirView, zc + dzdt * tm );
	vec3 acc = vec3( 0.0 );
	float T = 1.0;
	for ( int q = 0; q < 4; q++ ) {
		float ta = mix( t0, t1, float( q ) * 0.25 ), tb = mix( t0, t1, float( q + 1 ) * 0.25 );
		float tau = airOd( zc + dzdt * ta, zc + dzdt * tb, tb - ta );
		float w = air1mE( tau );
		if ( w > 0.0 ) {
			float t = 0.5 * ( ta + tb );
			acc += T * w * ( Jrest + airSun( dirView * ( t / fogDistScale ), dirView, zc + dzdt * t ) );
		}
		T *= exp( -tau );
	}
	// the uniforms the CPU reads back must stay live: fogK all six (the pass fits the medium to them; a stripped
	// fogK reads back as zeros and the pass refused with "fog scale 0", lane VOLAIR1 10-13)
	float keep = fogK[0].x + fogK[1].x + fogK[2].x + fogK[3].x + fogK[4].x + fogK[5].x;
	if ( !fogOn || keep < -1e30 )
		acc = vec3( 0.0 );
	fragColor = vec4( acc, T );
}
