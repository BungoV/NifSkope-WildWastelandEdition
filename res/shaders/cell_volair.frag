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
 *            The directional light is evaluated per sub-step (the shafts), the placed lights over the slice's stretch
 *            of the ray inside each light's sphere (four samples), the rest once at the slice's middle.
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
uniform vec4 airLayer;			// the near band's share w, zn, 1 / Hn, the ground layer's strength s (cellvolair.cpp VaLayer)
uniform vec4 airGround;			// the ground layer's anchor zg, 1 / its thickness
uniform bool volCsm;			// the sun's cascades are drawn this frame
uniform int volRed;				// 2 gioff, 4 flat, 8 noshadow, 16 nolights, 256 physcol, 262144 lightmid, 524288 bluescatter (1 off, 32 wrongmed, 128 farfit: the CPU's)
uniform vec4 volPhase[3];		// rgb: the weight of the air (Rayleigh), forward and back terms; w: the HG g (0 for air)
uniform vec4 volGi;				// rgb: the phase's tint (the three weights summed), w: its L1 g
uniform vec3 volFlat[6];		// red flat: one cube for every froxel (the grid's mean)
uniform int volTerms;			// gates only: 1 the directional light, 2 the placed lights, 4 the GI, 8 the ambient
// the optimisation (coordinator 10-14, cellvolair.cpp): the clustered lights and the conservative depth skip
uniform isamplerBuffer volClusters;	// per cluster (offset, count), then the light indices (ascending)
uniform ivec4 volClusterDims;		// clusters across, down; froxel columns a cluster side; slices a cluster (x 0: every light)
uniform int volDepthSkip;			// 0 off, 1 behind the farthest opaque distance (3 x 3 columns), 2 red nearskip
uniform sampler2D volGbuf;			// stage 0: lane AO1's depth buffer (volGbufZ 1: window depth) or g-buffer (a: linear depth, game units; 1e6 empty)
uniform int volGbufZ;
uniform vec2 volProjZ;			// the projection's [2][2], [3][2] (the window depth back to the view distance)
uniform ivec2 volGbufSize;

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

// the rest of the light at posView (height z), once a froxel: the placed lights (returned) and the sky's share, the
// GI and the ambient (Jenv)
vec3 airRest( vec3 posView, vec3 dirView, float z, float sA, float sB, out vec3 Jenv )
{
	Jenv = vec3( 0.0 );
	if ( !cellOn )
		return vec3( 0.0 );
	vec3 J = vec3( 0.0 );
	vec3 P = cellWorldPos( posView );
	vec3 dW = cellWorldDir( dirView );
	float zOff = z - P.z;	// the fog's height frame minus world z
	vec3 Dw = cellWorldPos( dirView * ( 1.0 / fogDistScale ) ) - cellWorldPos( vec3( 0.0 ) );	// world per game unit along the ray
	if ( ( volTerms & 2 ) != 0 && ( volRed & 16 ) == 0 ) {
		// clustered: only the lights whose sphere can reach this cluster, in the same ascending order (a light left out
		// would add exactly nothing below: d >= radius)
		int n = cellLightCount, base = -1;
		if ( volClusterDims.x > 0 ) {
			ivec2 c = ivec2( gl_FragCoord.xy ) / volClusterDims.z;
			int ci = ( ( volLayer / volClusterDims.w ) * volClusterDims.y + c.y ) * volClusterDims.x + c.x;
			base = texelFetch( volClusters, 2 * ci ).r;
			n = texelFetch( volClusters, 2 * ci + 1 ).r;
		}
		for ( int j = 0; j < n; j++ ) {
			int i = base < 0 ? j : texelFetch( volClusters, base + j ).r;
			vec4 t0 = texelFetch( cellLights, i * CELL_TPL );
			if ( ( volRed & 262144 ) != 0 ) {	// red lightmid: the light at the slice's middle only (a small light
				float d = length( t0.xyz - P );	// between two middles adds nothing)
				if ( d >= t0.w || d < 0.001 )
					continue;
				vec3 L = ( t0.xyz - P ) / d, Lo;
				bool noSpec;
				vec3 e = cellLightE( i, P, L, Lo, noSpec );	// N = L: no N.L, the shadow lifted toward the light
				if ( all( equal( e, vec3( 0.0 ) ) ) )
					continue;
				J += e * exp( -airOd( z, t0.z + zOff, d ) ) * volPhasePi( dot( L, dW ) );
				continue;
			}
			// lane VOLAIR1 (LAMPAIR2): the light's mean over the slice's stretch of the ray. The stretch inside the
			// light's sphere, [s0, s1] game units from the middle, is sampled at four midpoints and weighted by its
			// share of the slice: a lamp smaller than a slice is caught wherever it sits in it.
			vec3 m = P - t0.xyz;
			float qa = dot( Dw, Dw ), qb = dot( Dw, m ), qc = dot( m, m ) - t0.w * t0.w;
			float disc = qb * qb - qa * qc;
			if ( disc <= 0.0 || qa <= 0.0 )
				continue;
			float sq = sqrt( disc );
			float s0 = max( ( -qb - sq ) / qa, sA ), s1 = min( ( -qb + sq ) / qa, sB );
			if ( s1 <= s0 )
				continue;
			vec3 Ji = vec3( 0.0 );
			for ( int k = 0; k < 4; k++ ) {
				vec3 Pk = P + Dw * mix( s0, s1, ( float( k ) + 0.5 ) * 0.25 );
				float d = length( t0.xyz - Pk );
				if ( d >= t0.w || d < 0.001 )
					continue;
				vec3 L = ( t0.xyz - Pk ) / d, Lo;
				bool noSpec;
				vec3 e = cellLightE( i, Pk, L, Lo, noSpec );	// N = L: no N.L, the shadow lifted toward the light
				if ( all( equal( e, vec3( 0.0 ) ) ) )
					continue;
				Ji += e * exp( -airOd( Pk.z + zOff, t0.z + zOff, d ) ) * volPhasePi( dot( L, dW ) );
			}
			J += Ji * ( 0.25 * ( s1 - s0 ) / max( sB - sA, 1e-6 ) );
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
		Jenv += volCubeScatter( Eg, dW );
	}
	if ( wantAmb ) {
		// the share the GI replaced: indoors under the rule replace (the default), outdoors where the grid holds the sky
		bool replace = cellGiOn && ( cellInterior ? cellGiAmb == 1 : cellGiSky );
		const vec3 axes[6] = vec3[6]( vec3( 1, 0, 0 ), vec3( -1, 0, 0 ), vec3( 0, 1, 0 ), vec3( 0, -1, 0 ), vec3( 0, 0, 1 ), vec3( 0, 0, -1 ) );
		vec3 A[6];
		for ( int a = 0; a < 6; a++ )
			A[a] = cellAmbient( axes[a], P ) * ( replace ? 1.0 - share[a] : 1.0 );
		Jenv += volCubeScatter( A, dW );
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
	if ( volStage == 0 ) {
		// the column's opaque ray distances: its pixels (the column's share of the g-buffer, one pixel of margin)
		ivec2 p0 = max( ivec2( floor( vec2( col ) * vec2( volGbufSize ) / vec2( volDims.xy ) ) ) - 1, ivec2( 0 ) );
		ivec2 p1 = min( ivec2( ceil( vec2( col + 1 ) * vec2( volGbufSize ) / vec2( volDims.xy ) ) ) + 1, volGbufSize );
		float far = 0.0, near = 3.0e38;
		float k = fogDistScale / max( length( cellRow[0].xyz ), 1e-30 );
		for ( int y = p0.y; y < p1.y; y++ ) {
			for ( int x = p0.x; x < p1.x; x++ ) {
				float t = 3.0e38;
				vec2 n = ( vec2( x, y ) + 0.5 ) / vec2( volGbufSize ) * 2.0 - 1.0;
				if ( volGbufZ != 0 ) {
					// two 24-bit steps farther: the stored depth is rounded, the fragments' own distances are not
					float z = texelFetch( volGbuf, ivec2( x, y ), 0 ).r + 2.0 / 16777215.0;
					if ( z < 1.0 )
						t = max( volProjZ.y / ( 2.0 * z - 1.0 + volProjZ.x ), 0.0 ) * fogDistScale
							* length( vec3( n.x / volProj.x, n.y / volProj.y, 1.0 ) );
				} else {
					float a = texelFetch( volGbuf, ivec2( x, y ), 0 ).a;
					if ( a < 5.0e5 )
						t = max( a, 0.0 ) * k * length( vec3( n.x / volProj.x, n.y / volProj.y, 1.0 ) );
				}
				far = max( far, t );
				near = min( near, t );
			}
		}
		fragColor = vec4( far, near, 0.0, 1.0 );
		return;
	}
	if ( volDepthSkip != 0 && volLayer > 0 ) {
		// the conservative skip: no pixel's apply reads this froxel when the PREVIOUS slice already starts past the
		// farthest surface of the column and its neighbours (the bilinear's reach); (0, 1) keeps the integrate whole.
		// Red nearskip: behind the column's own nearest surface, from this slice (visible froxels skipped)
		float lim = 0.0, edge;
		if ( volDepthSkip == 2 ) {
			lim = texelFetch( volTile, col, 0 ).g;
			edge = volSlices.x * exp( volSlices.y * float( volLayer ) / volSlices.z );
		} else {
			for ( int dy = -1; dy <= 1; dy++ )
				for ( int dx = -1; dx <= 1; dx++ )
					lim = max( lim, texelFetch( volTile, clamp( col + ivec2( dx, dy ), ivec2( 0 ), volDims.xy - 1 ), 0 ).r );
			edge = volSlices.x * exp( volSlices.y * float( volLayer - 1 ) / volSlices.z );
			lim = lim * 1.001 + 1.0;
		}
		if ( edge > lim ) {
			fragColor = vec4( 0.0, 0.0, 0.0, 1.0 );
			return;
		}
	}
	vec2 ndc = ( vec2( col ) + 0.5 ) / vec2( volDims.xy ) * 2.0 - 1.0;
	vec3 dirView = normalize( vec3( ndc.x / volProj.x, ndc.y / volProj.y, -1.0 ) );
	float t0 = volSlices.x * exp( volSlices.y * float( volLayer ) / volSlices.z );
	float t1 = volSlices.x * exp( volSlices.y * float( volLayer + 1 ) / volSlices.z );
	// height along the ray: z(t) = zc + dzdt x t (t in game units)
	float zc = fogView.w;
	float dzdt = dot( fogView.xyz, dirView ) / fogDistScale;
	float tm = 0.5 * ( t0 + t1 );
	vec3 Jenv;
	vec3 Jrest = airRest( dirView * ( tm / fogDistScale ), dirView, zc + dzdt * tm, t0 - tm, t1 - tm, Jenv );
	vec3 acc = vec3( 0.0 );
	float T = 1.0;
	for ( int q = 0; q < 4; q++ ) {
		float ta = mix( t0, t1, float( q ) * 0.25 ), tb = mix( t0, t1, float( q + 1 ) * 0.25 );
		float tau = airOd( zc + dzdt * ta, zc + dzdt * tb, tb - ta );
		float w = air1mE( tau );
		if ( w > 0.0 ) {
			float t = 0.5 * ( ta + tb );
			float zt = zc + dzdt * t;
			acc += T * w * ( Jrest + airEnvColour( Jenv + airSun( dirView * ( t / fogDistScale ), dirView, zt ), t, zt, dirView ) );
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
