/* lane WATERCAUSTICS1 (src/gl/cellwater.h): the sun's caustics on the bed seen through the water.
 * Only the combined water twin compiles this (WW_WATER_CAUSTICS: fo4_waterlook); the default
 * fo4_water program is the one it always was. Applied in the water shader to the refracted scene texel (the bed),
 * the way Crest and Unity HDRP water apply theirs: only water pixels pay, and the bed is lit by the scene already.
 * lane CAUSTICPATTERN1 (10-27): the net. WATERCAUSTICS1's four sines made a lattice (autocorrelation 0.999 at one lag)
 * of fat lines, and its per-pixel Jacobian did not keep the light (bed mean 1.2-1.7 at the focus). Real shallow-water
 * caustics are thin bright filaments joined into irregular closed cells over a darker bed. The law now draws them
 * as the edges of an animated, warped Voronoi net, two octaves, calibrated against the forward splat (the exact
 * area-ratio caustic) of a broadband ripple field (notes causticpattern1: shape bars and FFT bars set beforehand):
 *   warp      the bed point moves by the gradient of one flow-noise layer (gradient noise on a lattice rotated by the
 *             triple 3-4-5, hashed mod the tile's image; its gradients turn whole times per 64 s): curved filaments
 *   cells     octave o: one feature point per cell (8192 / q_o u), around a centre anywhere in it, on a small circle turned a whole number of times
 *             per 64 s (sign and speed per point): the net morphs in place, never slides; it tiles at 8192 u
 *   edge      delta = the distance to the bisector of the two nearest points, ( F2^2 - F1^2 ) / ( 2 |p2 - p1| )
 *   line      P = ( 1 + delta / w )^-n, w the half-width: the sun disc + defocus with the sun path L, broad in the
 *             shallows (no focus yet), and the pixel footprint (no sparkle); per channel w ( 1 + chi ): the edges split
 *   energy    E = ( 1 - k ) + k ( P1 + a2 P2 ) / ( m1 + a2 m2 ), m_o the bed mean of P_o (a fit of w / c_o): the
 *             bed's mean stays 1 (light is only moved), the cells go dark by k(L); a2(L) the finer net deeper
 *   where     the bed point's own xy shifted up its refracted sun ray (S.xy - r.xy L, r = refract( -sun, up, 1 / 1.333 ))
 *   strength  the bed's lit colour x ( 1 + share x env x ( E - 1 ) ): share = the sun's part of the light at the bed
 *             (sun x cos x shadow over sun x cos x shadow + the sky colour), env = ( 1 - smoothstep( 0, 700, d ) )
 *             x the luminance of pure water's transmittance down the sun path (wwWaterAbsorb)
 * The red "lattice" (waterCauRed 5) draws WATERCAUSTICS1's four-sine Jacobian law as it was. */

uniform float waterCauTime;	// the world clock mod kWwCauLoop, seconds (0 with the World animation row off)
uniform int waterCauRed;	// 1 nofade, 2 noshadow, 3 still (time 0), 4 flat (no net: E = 1), 5 lattice (the 4-sine law)
uniform bool waterCauShadow;	// the sun's cascades are bound this frame (wwSunShadowWanted)
uniform bool waterCauOn;	// lane WATERCOMBINE1: the Water caustics row (fo4_waterlook carries every water look)

const float kWwCauN = 1.333;
const float kWwCauG = 686.6;	// 9.81 m/s^2 in units ( / 0.0142875 )
const float kWwCauLoop = 64.0;	// s: the pattern repeats (Tessendorf's looping ocean); C++ sends the clock mod this
const float kWwCauW0 = 6.2831853 / kWwCauLoop;
const float kWwCauTile = 8192.0;	// u: the pattern tiles (117 m)
const float kWwCauK0 = 6.2831853 / kWwCauTile;

// the net (CAUSTICPATTERN1; values from notes causticpattern1 tools/cand.py PAR)
const int kWwCauQ1 = 203;	// octave 1: cells per tile (cell 40.4 u)
const int kWwCauQ2 = 420;	// octave 2 (cell 19.5 u)
const uint kWwCauSeed1 = 101u;
const uint kWwCauSeed2 = 202u;
const float kWwCauR1 = 9.0;	// turns per 64 s of the feature points, x ( 0.6 + 0.8 jitter )
const float kWwCauR2 = 14.0;
const float kWwCauRad = 0.25;	// the points' circle radius, cells
const float kWwCauAw = 6.811;	// the warp, u per unit noise gradient
const float kWwCauKmax = 0.995;	// the cells' darkness at the focus
const float kWwCauLk = 77.57;	// u of sun path: k rises to kmax
const float kWwCauW0w = 0.5358;	// the half-width: w0 + w1 L + wn exp( -L / Ln ), u
const float kWwCauW1w = 0.007923;
const float kWwCauWn = 4.491;
const float kWwCauLn = 52.5;
const float kWwCauWv = 1.0;	// the half-width varies along the net: x exp( wv x the warp's x )
const float kWwCauPn = 3.38;	// the line profile's exponent
const float kWwCauA2lo = 0.0;	// octave 2's weight: a2lo + ( a2hi - a2lo ) ( 1 - exp( -( L / La )^2 ) ) + a2s exp( -L / Ls )
const float kWwCauA2hi = 1.2;
const float kWwCauLa = 260.0;
const float kWwCauA2s = 0.5811;
const float kWwCauLs = 34.37;
const vec3 kWwCauM = vec3( 1.691869, 1.677045, 0.071092 );	// m( r ) = A r / ( 1 + B r + C sqrt r ), r = w / cell (fit error 0.14 %)
const vec3 kWwCauChi = vec3( 0.12, 0.0, -0.12 );	// the edges' colour split: red lines wider, blue narrower
// the warp layer: gradient noise on the lattice rotated by ( 3, 4, 5 ), cell 8192 / 240 u, hashed mod 1200
const float kWwCauWc = 8192.0 / 240.0;
const int kWwCauWq = 1200;
const float kWwCauWr = 81.0;	// turns per 64 s of its gradients, x ( 0.75 + 0.5 jitter )

// the old law (red 5 "lattice"): direction (deg), wavelength (u), amplitude (u), phase
const vec4 kWwCauWave[4] = vec4[4](
	vec4( 12.0, 22.0, 0.50, 0.0 ), vec4( 77.0, 31.0, 0.60, 1.7 ), vec4( 141.0, 41.0, 0.80, 4.1 ), vec4( 203.0, 57.0, 1.00, 2.6 ) );

float wwCauLum( vec3 c )
{
	return dot( c, vec3( 0.2126, 0.7152, 0.0722 ) );
}

// the old law's Hessian of the ripple height at xy: ( hxx, hyy, hxy )
vec3 wwCauHessian( vec2 xy, float t )
{
	vec3 H = vec3( 0.0 );
	vec2 xw = xy - kWwCauTile * floor( xy / kWwCauTile );	// exact in float32: the phase stays small far from 0,0
	for ( int i = 0; i < 4; i++ ) {
		vec4 w = kWwCauWave[i];
		float a = radians( w.x );
		float k = 6.2831853 / w.y;
		vec2 kv = round( k * vec2( cos( a ), sin( a ) ) / kWwCauK0 ) * kWwCauK0;	// whole turns per tile
		float om = round( sqrt( kWwCauG * k ) / kWwCauW0 ) * kWwCauW0;	// a whole number of turns per loop
		float s = -w.z * sin( dot( kv, xw ) - om * t + w.w );
		H += s * vec3( kv.x * kv.x, kv.y * kv.y, kv.x * kv.y );
	}
	return H;
}

uint wwCauHash( ivec2 p, uint s )
{
	uint v = uint( p.x ) * 0x8da6b343u + uint( p.y ) * 0xd8163841u + s * 0xcb1ab31fu;
	v ^= v >> 16;
	v *= 0x7feb352du;
	v ^= v >> 15;
	v *= 0x846ca68bu;
	v ^= v >> 16;
	return v;
}

// a turning direction from a hash: ph the start (low 16 bits), jit (bits 16-30), the sign (bit 31); R turns per loop
vec2 wwCauTurn( uint v, float R, float lo, float span, float t, out float jit )
{
	float ph = float( v & 0xffffu ) / 65536.0;
	jit = float( ( v >> 16 ) & 0x7fffu ) / 32768.0;
	float r = ( ( v >> 31 ) != 0u ? -1.0 : 1.0 ) * round( R * ( lo + span * jit ) );
	float turns = ph + r * t / kWwCauLoop;
	float th = 6.2831853 * fract( turns );
	return vec2( cos( th ), sin( th ) );
}

vec2 wwCauFade( float x )	// quintic fade and its derivative
{
	return vec2( x * x * x * ( x * ( 6.0 * x - 15.0 ) + 10.0 ), 30.0 * x * x * ( x - 1.0 ) * ( x - 1.0 ) );
}

// the warp: the gradient of the flow-noise layer at the tile-wrapped xy, in world axes (unit noise)
vec2 wwCauWarp( vec2 xw, float t )
{
	vec2 u = vec2( 3.0 * xw.x + 4.0 * xw.y, -4.0 * xw.x + 3.0 * xw.y ) / ( 5.0 * kWwCauWc );
	vec2 iu = floor( u );
	vec2 f = u - iu;
	ivec2 i = ivec2( iu );
	vec2 g[4];
	float jit;
	for ( int c = 0; c < 4; c++ ) {
		ivec2 ic = i + ivec2( c & 1, c >> 1 );
		ivec2 j = ivec2( ( 3 * ic.x - 4 * ic.y + 3 * kWwCauWq ) % kWwCauWq, ( 4 * ic.x + 3 * ic.y + 3 * kWwCauWq ) % kWwCauWq );
		g[c] = wwCauTurn( wwCauHash( j, 11u ), kWwCauWr, 0.75, 0.5, t, jit );
	}
	float va = dot( g[0], f ), vb = dot( g[1], f - vec2( 1.0, 0.0 ) );
	float vc = dot( g[2], f - vec2( 0.0, 1.0 ) ), vd = dot( g[3], f - vec2( 1.0 ) );
	float k1 = vb - va, k2 = vc - va, k3 = va - vb - vc + vd;
	vec2 G1 = g[1] - g[0], G2 = g[2] - g[0], G3 = g[0] - g[1] - g[2] + g[3];
	vec2 sx = wwCauFade( f.x ), sy = wwCauFade( f.y );
	float nx = g[0].x + sx.y * k1 + sx.x * G1.x + sy.x * G2.x + sx.y * sy.x * k3 + sx.x * sy.x * G3.x;
	float ny = g[0].y + sx.x * G1.y + sy.y * k2 + sy.x * G2.y + sx.x * sy.y * k3 + sx.x * sy.x * G3.y;
	return vec2( 3.0 * nx - 4.0 * ny, 4.0 * nx + 3.0 * ny ) / 5.0;
}

// the distance to the nearest Voronoi edge, in cells; p in cells, wrapped to [0, q)
float wwCauEdge( vec2 p, int q, uint seed, float R, float t )
{
	vec2 ip = floor( p );
	vec2 f = p - ip;
	ivec2 i = ivec2( ip );
	float d1 = 1e9, d2 = 1e9;
	vec2 b1 = vec2( 0.0 ), b2 = vec2( 0.0 );
	for ( int oy = -1; oy <= 1; oy++ ) {
		for ( int ox = -1; ox <= 1; ox++ ) {
			ivec2 c = ( i + ivec2( ox, oy ) + q ) % q;
			float jit;
			uint h = wwCauHash( c, seed );
			vec2 dir = wwCauTurn( h, R, 0.6, 0.8, t, jit );
			uint h2 = h * 0x2c1b3c6du;	// the orbit's centre, uniform in the cell: no lattice line in the spectrum
			vec2 c0 = vec2( float( ( h2 >> 8 ) & 0xfffu ), float( ( h2 >> 20 ) & 0xfffu ) ) / 4096.0;
			vec2 v = vec2( float( ox ), float( oy ) ) + c0 + kWwCauRad * ( 0.7 + 0.3 * jit ) * dir - f;
			float dd = dot( v, v );
			if ( dd < d1 ) {
				d2 = d1; b2 = b1; d1 = dd; b1 = v;
			} else if ( dd < d2 ) {
				d2 = dd; b2 = v;
			}
		}
	}
	return ( d2 - d1 ) / ( 2.0 * max( length( b2 - b1 ), 1e-6 ) );
}

float wwCauMean( float r )	// the bed mean of the line profile at half-width / cell r
{
	return kWwCauM.x * r / ( 1.0 + kWwCauM.y * r + kWwCauM.z * sqrt( r ) );
}

// the net's light factor E per channel (mean 1 over the bed) at the tile-wrapped xy; L the sun path, fw the footprint
vec3 wwCauNet( vec2 xw, float L, float fw, float t )
{
	vec2 wn = wwCauWarp( xw, t );
	vec2 wp = xw + kWwCauAw * wn;
	float c1 = kWwCauTile / float( kWwCauQ1 ), c2 = kWwCauTile / float( kWwCauQ2 );
	float e1 = wwCauEdge( mod( wp / c1, float( kWwCauQ1 ) ), kWwCauQ1, kWwCauSeed1, kWwCauR1, t ) * c1;
	float e2 = wwCauEdge( mod( wp / c2, float( kWwCauQ2 ) ), kWwCauQ2, kWwCauSeed2, kWwCauR2, t ) * c2;
	float w0 = ( kWwCauW0w + kWwCauW1w * L + kWwCauWn * exp( -L / kWwCauLn ) ) * exp( kWwCauWv * wn.x );
	float a2 = kWwCauA2lo + ( kWwCauA2hi - kWwCauA2lo ) * ( 1.0 - exp( -( L / kWwCauLa ) * ( L / kWwCauLa ) ) )
		+ kWwCauA2s * exp( -L / kWwCauLs );
	float k = kWwCauKmax * ( 1.0 - exp( -( L / kWwCauLk ) * ( L / kWwCauLk ) ) );
	vec3 E;
	for ( int ch = 0; ch < 3; ch++ ) {
		float wc = w0 * ( 1.0 + kWwCauChi[ch] );
		float w = sqrt( wc * wc + 0.25 * fw * fw );
		float P = pow( 1.0 + e1 / w, -kWwCauPn ) + a2 * pow( 1.0 + e2 / w, -kWwCauPn );
		E[ch] = ( 1.0 - k ) + k * P / ( wwCauMean( w / c1 ) + a2 * wwCauMean( w / c2 ) );
	}
	return E;
}

// the depth envelope: 1 at the surface, falling to 0 at 700 u, x pure water's transmittance down the sun path
float wwCauEnvelope( float d, float L )
{
	if ( waterCauRed == 1 )
		return 1.0;
	return ( 1.0 - smoothstep( 0.0, 700.0, d ) ) * wwCauLum( wwWaterAbsorb( L ) );
}

/* the factor on the bed's colour at S (world), d its vertical depth under the surface; Ldir world TO the sun, sun its
 * colour (linear), sky the sky's colour (linear), shadow the sun's visibility at S, fw the bed's pixel footprint (u).
 * 1 = no caustic. dbg: (E green, env, share) */
vec3 wwWaterCaustic( vec3 S, float d, vec3 Ldir, vec3 sun, vec3 sky, float shadow, float fw, out vec3 dbg )
{
	dbg = vec3( 1.0, 0.0, 0.0 );
	if ( Ldir.z <= 0.0 || d <= 0.0 )
		return vec3( 1.0 );
	vec3 r = refract( -Ldir, vec3( 0.0, 0.0, 1.0 ), 1.0 / kWwCauN );
	float L = d / max( -r.z, 0.05 );
	vec2 X = S.xy - r.xy * L;
	float t = waterCauRed == 3 ? 0.0 : waterCauTime;
	vec3 I = vec3( 1.0 );
	if ( waterCauRed == 5 ) {	// lattice: WATERCAUSTICS1's law
		vec3 H = wwCauHessian( X, t );
		float s = L * ( 1.0 - 1.0 / kWwCauN );
		float det = ( 1.0 - s * H.x ) * ( 1.0 - s * H.y ) - s * s * H.z * H.z;
		float e = 0.05 + 0.002 * L;
		I = vec3( min( sqrt( 1.0 + e * e ) * inversesqrt( det * det + e * e ), 8.0 ) );
	} else if ( waterCauRed != 4 )
		I = wwCauNet( X - kWwCauTile * floor( X / kWwCauTile ), L, fw, t );
	float sh = waterCauRed == 2 ? 1.0 : shadow;
	float Es = wwCauLum( sun ) * Ldir.z * sh;
	float share = Es / max( Es + wwCauLum( sky ), 1e-6 );
	float env = wwCauEnvelope( d, L );
	dbg = vec3( I.g, env, share );
	return max( 1.0 + share * env * ( I - 1.0 ), 0.0 );
}
