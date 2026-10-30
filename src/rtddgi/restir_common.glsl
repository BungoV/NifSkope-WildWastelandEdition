// Lane RESTIRDI1 (2026-10-30): ReSTIR DI's per-pixel rules (Bitterli et al. 2020 reservoirs, Heitz et al. 2018 ratio),
// included by src/rtddgi/restir.comp after ddgi_common.glsl (it calls ddgiRadial / ddgiSpot) and compiled as C++ by
// tests/restir_math (through tests/restir_math/restir_shim.h). Every function marked MIRROR is src/restirmath.h's
// function of that name; change both or neither. Written in a C++-compatible subset: no swizzles, no out/inout
// parameters, no vec2, struct returns, literals that read the same in both languages.
//
// A light is the DDGI volume's packed light (src/rtddgi.cpp packLight, 8 vec4): l0 pos + radius, l1 colour + spot flag,
// l2 dir + cosOuter, l3 bias scale exponent cone, b0..b2 the box rows, l7 shape (0 omni, 1 hemisphere, 2 box),
// source radius (the shadow ray's sphere), shadow flag, 0.

#define RESTIR_BIT_NOVIS 1         // the shadowed term without its visibility (= the unshadowed term)
#define RESTIR_BIT_NOPDF 2         // W without 1 / p_hat(y) (a biased weight)
#define RESTIR_BIT_NOTEMPORAL 4    // no temporal reuse (each frame alone)
#define RESTIR_BIT_NOCLAMP 8       // the history's M not clamped (a new light waits behind a long history)
#define RESTIR_BIT_NOHOLE 16       // GPU / twin shadow rays: alpha-tested triangles block whole (RESTIRDI0's red)
#define RESTIR_HISTORY_CAP 20.0    // [Bitterli 2020 sec. 6] the history's M at most 20 x the current frame's
#define RESTIR_DEPTH_TOL 0.1       // a neighbour is reused when its depth is within 10 %
#define RESTIR_NORMAL_TOL 0.906    // and its normal within 25 degrees (cos 25)
#define RESTIR_ACC_MAX 16.0        // the two estimates' running means keep at most 16 frames (a moved shadow settles in ~16)

struct RestirRes
{
	int y;        // the light kept (index into the packed list), -1 none
	float wsum;   // the resampling weights' sum
	float M;      // the candidates it stands for
	float W;      // the unbiased contribution weight (0 when empty)
};

// MIRROR restirmath.h resMake
RestirRes restirMake( int y, float wsum, float M, float W )
{
	RestirRes r;
	r.y = y;
	r.wsum = wsum;
	r.M = M;
	r.W = W;
	return r;
}

// MIRROR restirmath.h hash: lowbias32 (Wellons), the same bits on the GPU and the CPU
uint restirHash( uint x )
{
	x = x ^ ( x >> 16u );
	x = x * 0x7feb352du;
	x = x ^ ( x >> 15u );
	x = x * 0x846ca68bu;
	x = x ^ ( x >> 16u );
	return x;
}

// MIRROR restirmath.h rand01: number k of a pixel's stream `seed`, in [0, 1) (24 bits)
float restirRand( uint seed, uint k )
{
	return float( restirHash( seed ^ restirHash( k * 0x9e3779b9u + 0x632be5abu ) ) >> 8u ) * ( 1.0 / 16777216.0 );
}

// MIRROR restirmath.h pixelSeed: pixel id, frame and pass mixed (pass 0 candidates, 1 temporal, 2 spatial, 3 shadow)
uint restirSeed( uint pixel, uint frame, uint pass )
{
	return restirHash( pixel * 0x27d4eb2du ^ restirHash( frame * 4u + pass ) );
}

// MIRROR restirmath.h luminance (Rec. 709)
float restirLum( float r, float g, float b ) { return 0.2126 * r + 0.7152 * g + 0.0722 * b; }

/* MIRROR restirmath.h target: p_hat = the light's unshadowed Lambert irradiance luminance at P (normal N), with its
 * shape clip (hemisphere half-space, box), radial curve and spot cone: trace.comp ddgiDirect's term without the ray */
float restirTarget( vec4 l0, vec4 l1, vec4 l2, vec4 l3, vec4 b0, vec4 b1, vec4 b2, vec4 l7, vec3 P, vec3 N )
{
	const vec3 Lv = vec3( l0.x - P.x, l0.y - P.y, l0.z - P.z );
	const float d = length( Lv );
	if ( d >= l0.w )
		return 0.0;
	const int shape = int( l7.x + 0.5 );
	if ( shape == 1 && ( P.x - l0.x ) * l2.x + ( P.y - l0.y ) * l2.y + ( P.z - l0.z ) * l2.z < 0.0 )
		return 0.0;
	if ( shape == 2 ) {
		if ( abs( b0.x * P.x + b0.y * P.y + b0.z * P.z + b0.w ) > 1.0 || abs( b1.x * P.x + b1.y * P.y + b1.z * P.z + b1.w ) > 1.0
			|| abs( b2.x * P.x + b2.y * P.y + b2.z * P.z + b2.w ) > 1.0 )
			return 0.0;
	}
	const float id = 1.0 / max( d, 0.001 );
	const float nl = ( N.x * Lv.x + N.y * Lv.y + N.z * Lv.z ) * id;
	if ( nl <= 0.0 )
		return 0.0;
	float att = ddgiRadial( d, l0.w, l3.x, l3.y, l3.z );
	if ( l1.w > 0.5 )
		att = att * ddgiSpot( -( Lv.x * l2.x + Lv.y * l2.y + Lv.z * l2.z ) * id, l2.w, l3.w );
	return max( restirLum( l1.x, l1.y, l1.z ) * att * nl, 0.0 );
}

// MIRROR restirmath.h update: stream one candidate of weight w standing for m candidates; kept with chance w / wsum
RestirRes restirUpdate( RestirRes r, int y, float w, float m, float u )
{
	RestirRes o = r;
	o.wsum = r.wsum + w;
	o.M = r.M + m;
	if ( w > 0.0 && u * o.wsum < w )
		o.y = y;
	return o;
}

// MIRROR restirmath.h finalize: W = wsum / (Z p_hat(y)), Z the candidates whose domain holds y (M for a single pixel)
RestirRes restirFinalize( RestirRes r, float pHat, float Z, int red )
{
	RestirRes o = r;
	if ( r.y < 0 || pHat <= 0.0 || Z <= 0.0 ) {
		o.W = 0.0;
		return o;
	}
	o.W = ( red & RESTIR_BIT_NOPDF ) != 0 ? r.wsum / Z : r.wsum / ( Z * pHat );
	return o;
}

// MIRROR restirmath.h clampHistory: the previous reservoir's M at most cap x the current frame's candidates
float restirClampM( float Mprev, float Mcur, int red )
{
	if ( ( red & RESTIR_BIT_NOCLAMP ) != 0 )
		return Mprev;
	return min( Mprev, RESTIR_HISTORY_CAP * Mcur );
}

// MIRROR restirmath.h similar: a neighbour or the history pixel may be reused (depth within 10 %, normal within 25 deg)
bool restirSimilar( float depthA, vec3 nA, float depthB, vec3 nB )
{
	if ( depthA <= 0.0 || depthB <= 0.0 || depthA >= 1e5 || depthB >= 1e5 )
		return false;
	if ( abs( depthA - depthB ) > RESTIR_DEPTH_TOL * depthA )
		return false;
	return nA.x * nB.x + nA.y * nB.y + nA.z * nB.z >= RESTIR_NORMAL_TOL;
}

/* MIRROR restirmath.h spherePoint: a point on the light's sphere (centre c, radius rad) uniform over the half that
 * faces P (a point on the far half is mirrored to the near one) */
vec3 restirSpherePoint( vec3 c, float rad, vec3 P, float u1, float u2 )
{
	const float z = 1.0 - 2.0 * u1;
	const float s = sqrt( max( 1.0 - z * z, 0.0 ) );
	const float ph = 6.283185307179586 * u2;
	float dx = s * cos( ph );
	float dy = s * sin( ph );
	float dz = z;
	if ( dx * ( P.x - c.x ) + dy * ( P.y - c.y ) + dz * ( P.z - c.z ) < 0.0 ) {
		dx = -dx;
		dy = -dy;
		dz = -dz;
	}
	return vec3( c.x + dx * rad, c.y + dy * rad, c.z + dz * rad );
}

// MIRROR restirmath.h shadowed: the shadowed estimate's sample term p_hat(y) W V (the unshadowed one is p_hat(y) W)
float restirShadowed( float pHat, float W, bool visible, int red )
{
	if ( ( red & RESTIR_BIT_NOVIS ) != 0 )
		return pHat * W;
	return visible ? pHat * W : 0.0;
}

// MIRROR restirmath.h ratio: Heitz 2018's ratio of the (filtered) shadowed and unshadowed estimates; 1 with no light
float restirRatio( float num, float den )
{
	if ( den <= 0.0 )
		return 1.0;
	return clamp( num / den, 0.0, 1.0 );
}

// MIRROR restirmath.h viewPos: the view-space point of pixel (px, py) of a W x H target at linear depth D, from the
// projection's P00, P11, P20, P21 (cell_lights.glsl cellContactLocal's projection, inverted); bottom row first
vec3 restirViewPos( float px, float py, float W, float H, float D, vec4 proj )
{
	const float nx = ( px + 0.5 ) / W * 2.0 - 1.0;
	const float ny = ( py + 0.5 ) / H * 2.0 - 1.0;
	return vec3( D * ( nx + proj.z ) / proj.x, D * ( ny + proj.w ) / proj.y, -D );
}

/* MIRROR restirmath.h accumulate: the two estimates' running means at the pixel (prev = the history pixel's num, den,
 * frames; valid false = no history), at most RESTIR_ACC_MAX frames deep. Without it the windowed ratio reads the
 * neighbours' reused samples, which the reuse makes alike (the unit test's temporal check measured it) */
vec4 restirAccumulate( vec4 prev, float num, float den, bool valid, int red )
{
	if ( !valid || ( red & RESTIR_BIT_NOTEMPORAL ) != 0 || prev.z < 0.5 )
		return vec4( num, den, 1.0, 0.0 );
	const float n = min( prev.z + 1.0, RESTIR_ACC_MAX );
	const float a = 1.0 / n;
	return vec4( prev.x + ( num - prev.x ) * a, prev.y + ( den - prev.y ) * a, n, 0.0 );
}
