// HLSL source of features/water/water_caustics.glsl (include-only GLSL), lane HLSLSRC3 (notes/hlslsrc3/tools/glsl2hlsl.py): the GLSL's comments,
// names and order kept. Its loose uniforms sit at the top under WW_LOOSE_UNIFORMS: the entry that includes this
// file includes it once more inside its LooseUniforms cbuffer with that defined (HLSL has one cbuffer for them).
#ifdef WW_HLSL_SRC
#if defined(WW_LOOSE_TYPES)	// the types its loose uniforms use

#elif defined(WW_LOOSE_UNIFORMS)
	float waterCauTime;	// the world clock mod kWwCauLoop, seconds (0 with the World animation row off)
	int waterCauRed;	// 1 nofade, 2 noshadow, 3 still (time 0), 4 flat (no net: E = 1), 5 lattice (the 4-sine law)
	bool waterCauShadow;	// the sun's cascades are bound this frame (wwSunShadowWanted)
	bool waterCauOn;	// lane WATERCOMBINE1: the Water caustics row (fo4_waterlook carries every water look)
#else
#ifndef WW_GLSL_MOD
#define WW_GLSL_MOD	// GLSL mod: x - y floor(x / y) (HLSL fmod truncates)
float mod(float x, float y) { return x - y * floor(x / y); }
float2 mod(float2 x, float2 y) { return x - y * floor(x / y); }
float3 mod(float3 x, float3 y) { return x - y * floor(x / y); }
float4 mod(float4 x, float4 y) { return x - y * floor(x / y); }
float2 mod(float2 x, float y) { return x - y * floor(x / y); }
float3 mod(float3 x, float y) { return x - y * floor(x / y); }
float4 mod(float4 x, float y) { return x - y * floor(x / y); }
#endif
/* lane WATERCAUSTICS1 (src/features/water/cellwater.h): the sun's caustics on the bed seen through the water.
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



static const float kWwCauN = 1.333;
static const float kWwCauG = 686.6;	// 9.81 m/s^2 in units ( / 0.0142875 )
static const float kWwCauLoop = 64.0;	// s: the pattern repeats (Tessendorf's looping ocean); C++ sends the clock mod this
static const float kWwCauW0 = 6.2831853 / kWwCauLoop;
static const float kWwCauTile = 8192.0;	// u: the pattern tiles (117 m)
static const float kWwCauK0 = 6.2831853 / kWwCauTile;

// the net (CAUSTICPATTERN1; values from notes causticpattern1 tools/cand.py PAR)
static const int kWwCauQ1 = 203;	// octave 1: cells per tile (cell 40.4 u)
static const int kWwCauQ2 = 420;	// octave 2 (cell 19.5 u)
static const uint kWwCauSeed1 = 101u;
static const uint kWwCauSeed2 = 202u;
static const float kWwCauR1 = 9.0;	// turns per 64 s of the feature points, x ( 0.6 + 0.8 jitter )
static const float kWwCauR2 = 14.0;
static const float kWwCauRad = 0.25;	// the points' circle radius, cells
static const float kWwCauAw = 6.811;	// the warp, u per unit noise gradient
static const float kWwCauKmax = 0.995;	// the cells' darkness at the focus
// lane WATERLOOK1: the sun path the net takes, x this factor (the spectrum's curvature over the net's own; 1 without it)
static float wwCauPathGain = 1.0;
static const float kWwCauLk = 77.57;	// u of sun path: k rises to kmax
static const float kWwCauW0w = 0.5358;	// the half-width: w0 + w1 L + wn exp( -L / Ln ), u
static const float kWwCauW1w = 0.007923;
static const float kWwCauWn = 4.491;
static const float kWwCauLn = 52.5;
static const float kWwCauWv = 1.0;	// the half-width varies along the net: x exp( wv x the warp's x )
static const float kWwCauPn = 3.38;	// the line profile's exponent
static const float kWwCauA2lo = 0.0;	// octave 2's weight: a2lo + ( a2hi - a2lo ) ( 1 - exp( -( L / La )^2 ) ) + a2s exp( -L / Ls )
static const float kWwCauA2hi = 1.2;
static const float kWwCauLa = 260.0;
static const float kWwCauA2s = 0.5811;
static const float kWwCauLs = 34.37;
static const float3 kWwCauM = float3( 1.691869, 1.677045, 0.071092 );	// m( r ) = A r / ( 1 + B r + C sqrt r ), r = w / cell (fit error 0.14 %)
static const float3 kWwCauChi = float3( 0.12, 0.0, -0.12 );	// the edges' colour split: red lines wider, blue narrower
// the warp layer: gradient noise on the lattice rotated by ( 3, 4, 5 ), cell 8192 / 240 u, hashed mod 1200
static const float kWwCauWc = 8192.0 / 240.0;
static const int kWwCauWq = 1200;
static const float kWwCauWr = 81.0;	// turns per 64 s of its gradients, x ( 0.75 + 0.5 jitter )

// the old law (red 5 "lattice"): direction (deg), wavelength (u), amplitude (u), phase
static const float4 kWwCauWave[4] = {
	float4( 12.0, 22.0, 0.50, 0.0 ), float4( 77.0, 31.0, 0.60, 1.7 ), float4( 141.0, 41.0, 0.80, 4.1 ), float4( 203.0, 57.0, 1.00, 2.6 ) };

float wwCauLum( float3 c )
{
	return dot( c, float3( 0.2126, 0.7152, 0.0722 ) );
}

// the old law's Hessian of the ripple height at xy: ( hxx, hyy, hxy )
float3 wwCauHessian( float2 xy, float t )
{
	float3 H = (float3)( 0.0 );
	float2 xw = xy - kWwCauTile * floor( xy / kWwCauTile );	// exact in float32: the phase stays small far from 0,0
	for ( int i = 0; i < 4; i++ ) {
		float4 w = kWwCauWave[i];
		float a = radians( w.x );
		float k = 6.2831853 / w.y;
		float2 kv = round( k * float2( cos( a ), sin( a ) ) / kWwCauK0 ) * kWwCauK0;	// whole turns per tile
		float om = round( sqrt( kWwCauG * k ) / kWwCauW0 ) * kWwCauW0;	// a whole number of turns per loop
		float s = -w.z * sin( dot( kv, xw ) - om * t + w.w );
		H += s * float3( kv.x * kv.x, kv.y * kv.y, kv.x * kv.y );
	}
	return H;
}

uint wwCauHash( int2 p, uint s )
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
float2 wwCauTurn( uint v, float R, float lo, float span, float t, out float jit )
{
	float ph = float( v & 0xffffu ) / 65536.0;
	jit = float( ( v >> 16 ) & 0x7fffu ) / 32768.0;
	float r = ( ( v >> 31 ) != 0u ? -1.0 : 1.0 ) * round( R * ( lo + span * jit ) );
	float turns = ph + r * t / kWwCauLoop;
	float th = 6.2831853 * frac( turns );
	return float2( cos( th ), sin( th ) );
}

float2 wwCauFade( float x )	// quintic fade and its derivative
{
	return float2( x * x * x * ( x * ( 6.0 * x - 15.0 ) + 10.0 ), 30.0 * x * x * ( x - 1.0 ) * ( x - 1.0 ) );
}

// the warp: the gradient of the flow-noise layer at the tile-wrapped xy, in world axes (unit noise)
float2 wwCauWarp( float2 xw, float t )
{
	float2 u = float2( 3.0 * xw.x + 4.0 * xw.y, -4.0 * xw.x + 3.0 * xw.y ) / ( 5.0 * kWwCauWc );
	float2 iu = floor( u );
	float2 f = u - iu;
	int2 i = int2( iu );
	float2 g[4];
	float jit;
	for ( int c = 0; c < 4; c++ ) {
		int2 ic = i + int2( c & 1, c >> 1 );
		int2 j = int2( ( 3 * ic.x - 4 * ic.y + 3 * kWwCauWq ) % kWwCauWq, ( 4 * ic.x + 3 * ic.y + 3 * kWwCauWq ) % kWwCauWq );
		g[c] = wwCauTurn( wwCauHash( j, 11u ), kWwCauWr, 0.75, 0.5, t, jit );
	}
	float va = dot( g[0], f ), vb = dot( g[1], f - float2( 1.0, 0.0 ) );
	float vc = dot( g[2], f - float2( 0.0, 1.0 ) ), vd = dot( g[3], f - (float2)( 1.0 ) );
	float k1 = vb - va, k2 = vc - va, k3 = va - vb - vc + vd;
	float2 G1 = g[1] - g[0], G2 = g[2] - g[0], G3 = g[0] - g[1] - g[2] + g[3];
	float2 sx = wwCauFade( f.x ), sy = wwCauFade( f.y );
	float nx = g[0].x + sx.y * k1 + sx.x * G1.x + sy.x * G2.x + sx.y * sy.x * k3 + sx.x * sy.x * G3.x;
	float ny = g[0].y + sx.x * G1.y + sy.y * k2 + sy.x * G2.y + sx.x * sy.y * k3 + sx.x * sy.x * G3.y;
	return float2( 3.0 * nx - 4.0 * ny, 4.0 * nx + 3.0 * ny ) / 5.0;
}

// the distance to the nearest Voronoi edge, in cells; p in cells, wrapped to [0, q)
float wwCauEdge( float2 p, int q, uint seed, float R, float t )
{
	float2 ip = floor( p );
	float2 f = p - ip;
	int2 i = int2( ip );
	float d1 = 1e9, d2 = 1e9;
	float2 b1 = (float2)( 0.0 ), b2 = (float2)( 0.0 );
	for ( int oy = -1; oy <= 1; oy++ ) {
		for ( int ox = -1; ox <= 1; ox++ ) {
			int2 c = ( i + int2( ox, oy ) + q ) % q;
			float jit;
			uint h = wwCauHash( c, seed );
			float2 dir = wwCauTurn( h, R, 0.6, 0.8, t, jit );
			uint h2 = h * 0x2c1b3c6du;	// the orbit's centre, uniform in the cell: no lattice line in the spectrum
			float2 c0 = float2( float( ( h2 >> 8 ) & 0xfffu ), float( ( h2 >> 20 ) & 0xfffu ) ) / 4096.0;
			float2 v = float2( float( ox ), float( oy ) ) + c0 + kWwCauRad * ( 0.7 + 0.3 * jit ) * dir - f;
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
float3 wwCauNet( float2 xw, float L, float fw, float t )
{
	float2 wn = wwCauWarp( xw, t );
	float2 wp = xw + kWwCauAw * wn;
	float c1 = kWwCauTile / float( kWwCauQ1 ), c2 = kWwCauTile / float( kWwCauQ2 );
	float e1 = wwCauEdge( mod( wp / c1, float( kWwCauQ1 ) ), kWwCauQ1, kWwCauSeed1, kWwCauR1, t ) * c1;
	float e2 = wwCauEdge( mod( wp / c2, float( kWwCauQ2 ) ), kWwCauQ2, kWwCauSeed2, kWwCauR2, t ) * c2;
	float w0 = ( kWwCauW0w + kWwCauW1w * L + kWwCauWn * exp( -L / kWwCauLn ) ) * exp( kWwCauWv * wn.x );
	float a2 = kWwCauA2lo + ( kWwCauA2hi - kWwCauA2lo ) * ( 1.0 - exp( -( L / kWwCauLa ) * ( L / kWwCauLa ) ) )
		+ kWwCauA2s * exp( -L / kWwCauLs );
	float k = kWwCauKmax * ( 1.0 - exp( -( L / kWwCauLk ) * ( L / kWwCauLk ) ) );
	float3 E;
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
float3 wwWaterCaustic( float3 S, float d, float3 Ldir, float3 sun, float3 sky, float shadow, float fw, out float3 dbg )
{
	dbg = float3( 1.0, 0.0, 0.0 );
	if ( or( Ldir.z <= 0.0, d <= 0.0 ) )
		return (float3)( 1.0 );
	float3 r = refract( -Ldir, float3( 0.0, 0.0, 1.0 ), 1.0 / kWwCauN );
	float L = d / max( -r.z, 0.05 );
	float2 X = S.xy - r.xy * L;
	float t = waterCauRed == 3 ? 0.0 : waterCauTime;
	float3 I = (float3)( 1.0 );
	if ( waterCauRed == 5 ) {	// lattice: WATERCAUSTICS1's law
		float3 H = wwCauHessian( X, t );
		float s = L * ( 1.0 - 1.0 / kWwCauN );
		float det = ( 1.0 - s * H.x ) * ( 1.0 - s * H.y ) - s * s * H.z * H.z;
		float e = 0.05 + 0.002 * L;
		I = (float3)( min( sqrt( 1.0 + e * e ) * rsqrt( det * det + e * e ), 8.0 ) );
	} else if ( waterCauRed != 4 )
		I = wwCauNet( X - kWwCauTile * floor( X / kWwCauTile ), L * wwCauPathGain, fw, t );
	float sh = waterCauRed == 2 ? 1.0 : shadow;
	float Es = wwCauLum( sun ) * Ldir.z * sh;
	float share = Es / max( Es + wwCauLum( sky ), 1e-6 );
	float env = wwCauEnvelope( d, L );
	dbg = float3( I.g, env, share );
	return max( 1.0 + share * env * ( I - 1.0 ), 0.0 );
}
#endif	// WW_LOOSE_UNIFORMS
#else	// WW_HLSL_SRC: the per-variant files' half (HLSLBULK1, WW_FN_<name> picks functions); it goes with the last of them
// Shared HLSL for res/shaders/features/water/water_caustics.glsl (include-only GLSL), HLSLBULK1 round 2 (notes/hlslbulk1/tools/factor.py). No bindings: the
// functions below are the converted ones, they read the declarations of the shader that includes them (its cbuffers,
// resources, statics and LooseUniforms: the per-module shim and the entry own those). Define WW_FN_<name> for each
// wanted function before the #include (and #undef after), or WW_FN_ALL for all of them; a _V<n> suffix is a variant
// of the same GLSL function (a different define set at the entry). Fusion Shaders (F4FS) reuses the math from here.
// Included by: fo4_waterlook_ps.hlsl, fo4_waterlookd_ps.hlsl, fo4_waterlookdp_ps.hlsl, fo4_waterlookp_ps.hlsl
// Not here (declarations, or no converted entry carried them): wwCauHessian
#ifdef WW_FN_ALL
static const float kWwCauN = 1.333;
static const float kWwCauG = 686.6;
static const float kWwCauLoop = 64.0;
static const float kWwCauTile = 8192.0;
static const int kWwCauQ1 = 203;
static const int kWwCauQ2 = 420;
static const uint kWwCauSeed1 = 101u;
static const uint kWwCauSeed2 = 202u;
static const float kWwCauR1 = 9.0;
static const float kWwCauR2 = 14.0;
static const float kWwCauRad = 0.25;
static const float kWwCauAw = 6.811;
static const float kWwCauKmax = 0.995;
static const float kWwCauLk = 77.57;
static const float kWwCauW0w = 0.5358;
static const float kWwCauW1w = 0.007923;
static const float kWwCauWn = 4.491;
static const float kWwCauLn = 52.5;
static const float kWwCauWv = 1.0;
static const float kWwCauPn = 3.38;
static const float kWwCauA2lo = 0.0;
static const float kWwCauA2hi = 1.2;
static const float kWwCauLa = 260.0;
static const float kWwCauA2s = 0.5811;
static const float kWwCauLs = 34.37;
static const float kWwCauWc = 8192.0 / 240.0;
static const int kWwCauWq = 1200;
static const float kWwCauWr = 81.0;
#endif

#if defined(WW_FN_wwCauLum) || defined(WW_FN_ALL)
float wwCauLum(float3 c)
{
    return dot(c, float3(0.2125999927520751953125f, 0.715200006961822509765625f, 0.072200000286102294921875f));
}
#endif

#if defined(WW_FN_wwCauHash) || defined(WW_FN_ALL)
uint wwCauHash(int2 p, uint s)
{
    uint v = ((uint(p.x) * 2376512323u) + (uint(p.y) * 3625334849u)) + (s * 3407524639u);
    v ^= (v >> uint(16));
    v *= 2146121005u;
    v ^= (v >> uint(15));
    v *= 2221713035u;
    v ^= (v >> uint(16));
    return v;
}
#endif

#if defined(WW_FN_wwCauTurn) || defined(WW_FN_ALL)
float2 wwCauTurn(uint v, float R, float lo, float span, float t, inout float jit)
{
    float ph = float(v & 65535u) / 65536.0f;
    jit = float((v >> uint(16)) & 32767u) / 32768.0f;
    float r = (((v >> uint(31)) != 0u) ? (-1.0f) : 1.0f) * round(R * (lo + (span * jit)));
    float turns = ph + ((r * t) / 64.0f);
    float th = 6.283185482025146484375f * frac(turns);
    return float2(cos(th), sin(th));
}
#endif

#if defined(WW_FN_wwCauFade) || defined(WW_FN_ALL)
float2 wwCauFade(float x)
{
    return float2(((x * x) * x) * ((x * ((6.0f * x) - 15.0f)) + 10.0f), (((30.0f * x) * x) * (x - 1.0f)) * (x - 1.0f));
}
#endif

#if defined(WW_FN_wwCauWarp) || defined(WW_FN_ALL)
float2 wwCauWarp(float2 xw, float t)
{
    float2 u = float2((3.0f * xw.x) + (4.0f * xw.y), ((-4.0f) * xw.x) + (3.0f * xw.y)) / 170.6666717529296875f.xx;
    float2 iu = floor(u);
    float2 f = u - iu;
    int2 i = int2(iu);
    float2 g[4];
    float wwCauTurnPick;
    for (int c = 0; c < 4; c++)
    {
        int2 ic = i + int2(c & 1, c >> 1);
        int2 j = int2((((3 * ic.x) - (4 * ic.y)) + 3600) % 1200, (((4 * ic.x) + (3 * ic.y)) + 3600) % 1200);
        int2 wwCauHashPart2 = j;
        uint wwCauHashPart = 11u;
        uint wwCauHashRet = wwCauHash(wwCauHashPart2, wwCauHashPart);
        float wwCauTurnPart4 = 81.0f;
        float wwCauTurnPart3 = 0.75f;
        float wwCauTurnPart2 = 0.5f;
        float wwCauTurnPart = t;
        float2 wwCauTurnRet = wwCauTurn(wwCauHashRet, wwCauTurnPart4, wwCauTurnPart3, wwCauTurnPart2, wwCauTurnPart, wwCauTurnPick);
        float jit = wwCauTurnPick;
        g[c] = wwCauTurnRet;
    }
    float va = dot(g[0], f);
    float vb = dot(g[1], f - float2(1.0f, 0.0f));
    float vc = dot(g[2], f - float2(0.0f, 1.0f));
    float vd = dot(g[3], f - 1.0f.xx);
    float k1 = vb - va;
    float k2 = vc - va;
    float k3 = ((va - vb) - vc) + vd;
    float2 G1 = g[1] - g[0];
    float2 G2 = g[2] - g[0];
    float2 G3 = ((g[0] - g[1]) - g[2]) + g[3];
    float sxPart = f.x;
    float2 sx = wwCauFade(sxPart);
    float syPart = f.y;
    float2 sy = wwCauFade(syPart);
    float nx = ((((g[0].x + (sx.y * k1)) + (sx.x * G1.x)) + (sy.x * G2.x)) + ((sx.y * sy.x) * k3)) + ((sx.x * sy.x) * G3.x);
    float ny = ((((g[0].y + (sx.x * G1.y)) + (sy.y * k2)) + (sy.x * G2.y)) + ((sx.x * sy.y) * k3)) + ((sx.x * sy.x) * G3.y);
    return float2((3.0f * nx) - (4.0f * ny), (4.0f * nx) + (3.0f * ny)) / 5.0f.xx;
}
#endif

#if defined(WW_FN_wwCauEdge) || defined(WW_FN_ALL)
float wwCauEdge(float2 p, int q, uint seed, float R, float t)
{
    float2 ip = floor(p);
    float2 f = p - ip;
    int2 i = int2(ip);
    float d1 = 1000000000.0f;
    float d2 = 1000000000.0f;
    float2 b1 = 0.0f.xx;
    float2 b2 = 0.0f.xx;
    float wwCauTurnPick;
    for (int oy = -1; oy <= 1; oy++)
    {
        for (int ox = -1; ox <= 1; ox++)
        {
            int2 c = ((i + int2(ox, oy)) + q.xx) % q.xx;
            int2 wwCauHashPart2 = c;
            uint wwCauHashPart = seed;
            uint h = wwCauHash(wwCauHashPart2, wwCauHashPart);
            uint wwCauHashRet = h;
            float wwCauTurnPart4 = R;
            float wwCauTurnPart3 = 0.60000002384185791015625f;
            float wwCauTurnPart2 = 0.800000011920928955078125f;
            float wwCauTurnPart = t;
            float2 wwCauTurnRet = wwCauTurn(wwCauHashRet, wwCauTurnPart4, wwCauTurnPart3, wwCauTurnPart2, wwCauTurnPart, wwCauTurnPick);
            float jit = wwCauTurnPick;
            float2 dir = wwCauTurnRet;
            // the orbit's centre, uniform in the cell: no lattice line in the spectrum
            uint h2 = h * 739982445u;
            float2 c0 = float2(float((h2 >> uint(8)) & 4095u), float((h2 >> uint(20)) & 4095u)) / 4096.0f.xx;
            float2 v = ((float2(float(ox), float(oy)) + c0) + (dir * (0.25f * (0.699999988079071044921875f + (0.300000011920928955078125f * jit))))) - f;
            float dd = dot(v, v);
            if (dd < d1)
            {
                d2 = d1;
                b2 = b1;
                d1 = dd;
                b1 = v;
            }
            else
            {
                if (dd < d2)
                {
                    d2 = dd;
                    b2 = v;
                }
            }
        }
    }
    return (d2 - d1) / (2.0f * max(length(b2 - b1), 9.9999999747524270787835121154785e-07f));
}
#endif

#if defined(WW_FN_wwCauMean) || defined(WW_FN_ALL)
float wwCauMean(float r)
{
    return (1.6918690204620361328125f * r) / ((1.0f + (1.67704498767852783203125f * r)) + (0.071092002093791961669921875f * sqrt(r)));
}
#endif

#if defined(WW_FN_wwCauNet) || defined(WW_FN_ALL)
float3 wwCauNet(float2 xw, float L, float fw, float t)
{
    float2 wwCauTurnPick = xw;
    float wwCauHashPart2 = t;
    float2 wn = wwCauWarp(wwCauTurnPick, wwCauHashPart2);
    float2 wp = xw + (wn * 6.81099987030029296875f);
    float c1 = 40.354679107666015625f;
    float c2 = 19.5047626495361328125f;
    float2 wwCauHashPart = mod(wp / c1.xx, 203.0f.xx);
    int wwCauHashRet = 203;
    uint wwCauTurnPart4 = 101u;
    float wwCauTurnPart3 = 9.0f;
    float wwCauTurnPart2 = t;
    float e1 = wwCauEdge(wwCauHashPart, wwCauHashRet, wwCauTurnPart4, wwCauTurnPart3, wwCauTurnPart2) * c1;
    float2 wwCauTurnPart = mod(wp / c2.xx, 420.0f.xx);
    int wwCauTurnRet = 420;
    uint sxPart = 202u;
    float syPart = 14.0f;
    float e2Part = t;
    float e2 = wwCauEdge(wwCauTurnPart, wwCauTurnRet, sxPart, syPart, e2Part) * c2;
    float w0 = ((0.53579998016357421875f + (0.007922999560832977294921875f * L)) + (4.49100017547607421875f * exp((-L) / 52.5f))) * exp(1.0f * wn.x);
    float a2 = (0.0f + (1.2000000476837158203125f * (1.0f - exp((-(L / 260.0f)) * (L / 260.0f))))) + (0.581099987030029296875f * exp((-L) / 34.369998931884765625f));
    float k = 0.99500000476837158203125f * (1.0f - exp((-(L / 77.56999969482421875f)) * (L / 77.56999969482421875f)));
    float3 E;
    for (int ch = 0; ch < 3; ch++)
    {
        float wc = w0 * (1.0f + float3(0.119999997317790985107421875f, 0.0f, -0.119999997317790985107421875f)[ch]);
        float w = sqrt((wc * wc) + ((0.25f * fw) * fw));
        float P = pow(1.0f + (e1 / w), -3.38000011444091796875f) + (a2 * pow(1.0f + (e2 / w), -3.38000011444091796875f));
        float EPart2 = w / c1;
        float EPart = w / c2;
        E[ch] = (1.0f - k) + ((k * P) / (wwCauMean(EPart2) + (a2 * wwCauMean(EPart))));
    }
    return E;
}
#endif

#if defined(WW_FN_wwCauEnvelope) || defined(WW_FN_ALL)
float wwCauEnvelope(float d, float L)
{
    if (waterCauRed == 1)
    {
        return 1.0f;
    }
    float wwCauTurnPick = L;
    float3 wwCauHashPart2 = wwWaterAbsorb(wwCauTurnPick);
    return (1.0f - smoothstep(0.0f, 700.0f, d)) * wwCauLum(wwCauHashPart2);
}
#endif

#if defined(WW_FN_wwWaterCaustic) || defined(WW_FN_ALL)
float3 wwWaterCaustic(float3 S, float d, float3 Ldir, float3 sun, float3 sky, float shadow, float fw, out float3 dbg)
{
    dbg = float3(1.0f, 0.0f, 0.0f);
    bool wwh0 = (Ldir.z <= 0.0f);
    bool wwh1 = (d <= 0.0f);
    if (or(wwh0, wwh1))   // HLSLFIX2 orfix.py: no short-circuit branch (NV took the wrong side)
    {
        return 1.0f.xxx;
    }
    float3 r = refract(-Ldir, float3(0.0f, 0.0f, 1.0f), 0.750187575817108154296875f);
    float L = d / max(-r.z, 0.0500000007450580596923828125f);
    float2 X = S.xy - (r.xy * L);
    float t = (waterCauRed == 3) ? 0.0f : waterCauTime;
    float3 I = 1.0f.xxx;
    if (waterCauRed == 5)
    {
        float2 wwCauTurnPick = X;
        float wwCauHashPart2 = t;
        float3 H = wwCauHessian(wwCauTurnPick, wwCauHashPart2);
        float s = L * 0.2498124539852142333984375f;
        float det = ((1.0f - (s * H.x)) * (1.0f - (s * H.y))) - (((s * s) * H.z) * H.z);
        float e = 0.0500000007450580596923828125f + (0.00200000009499490261077880859375f * L);
        I = min(sqrt(1.0f + (e * e)) * rsqrt((det * det) + (e * e)), 8.0f).xxx;
    }
    else
    {
        if (waterCauRed != 4)
        {
            float2 wwCauHashPart = X - (floor(X / 8192.0f.xx) * 8192.0f);
            float wwCauHashRet = L * wwCauPathGain;
            float wwCauTurnPart4 = fw;
            float wwCauTurnPart3 = t;
            I = wwCauNet(wwCauHashPart, wwCauHashRet, wwCauTurnPart4, wwCauTurnPart3);
        }
    }
    float sh = (waterCauRed == 2) ? 1.0f : shadow;
    float3 wwCauTurnPart2 = sun;
    float Es = (wwCauLum(wwCauTurnPart2) * Ldir.z) * sh;
    float3 wwCauTurnPart = sky;
    float share = Es / max(Es + wwCauLum(wwCauTurnPart), 9.9999999747524270787835121154785e-07f);
    float wwCauTurnRet = d;
    float sxPart = L;
    float env = wwCauEnvelope(wwCauTurnRet, sxPart);
    dbg = float3(I.y, env, share);
    return max(1.0f.xxx + ((I - 1.0f.xxx) * (share * env)), 0.0f.xxx);
}
#endif
#endif	// WW_HLSL_SRC
