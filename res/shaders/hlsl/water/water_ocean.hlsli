// HLSL source of features/water/water_ocean.glsl (include-only GLSL), lane HLSLSRC2 (notes/hlslsrc2/tools/glsl2hlsl.py): the GLSL's comments,
// names and order kept. Its loose uniforms sit at the top under WW_LOOSE_UNIFORMS: the entry that includes this
// file includes it once more inside its LooseUniforms cbuffer with that defined (HLSL has one cbuffer for them).
#ifdef WW_HLSL_SRC
#if defined(WW_LOOSE_TYPES)	// the types its loose uniforms use

#elif defined(WW_LOOSE_UNIFORMS)
	// JONSWAP (Hasselmann et al. 1973) with Donelan-Banner spreading, plus the sea's swell or a lake's long ripples, as
	// three FFT cascades a set (Tessendorf 2001), sea and fresh water; built by src/features/water/cellocean.cpp each frame into
	// waterOceanB / waterOceanC. Tiles are hidden by the triangle-grid random-offset blend (Heitz and Neyret 2018,
	// Deliot and Heitz 2019), the slope variance the mips lose goes to the glare's roughness (LEAN, Olano and Baker 2010),
	// and the unresolved tail below the finest cascade (Cox and Munk 1954 minus what the cascades hold) too.
	bool waterOceanOn;		// the Ocean waves row
	int waterOceanRed;		// 2 tile (plain repeats), 4 nopatch (no wind patches), 8 novar (no lost variance), 16 nochop
	float4 waterOceanInvL[2];	// per set: 1 / tile size of cascades 0..2 (1/m), the chop lambda
	float4 waterOceanSet[2];	// per set: Hs of its tiles (m), their wind sea's Hs (m), the swell / ripple Hs (m), tail mss
	float4 waterOceanSetB[2];	// per set: the tiles' fetch (m), resolved curvature rms (1/m), patch cell (u), 0
	float4 waterOceanWind;	// the waves' travel direction x, y (unit), the wind (m/s), the caustic net's curvature (1/m)
	float4 waterOceanPatch;	// the wind patches' drift, sea x, y, fresh x, y (u, wrapped by their period)
	float4 waterOceanDisp;	// full out to (u), none past (u), the split's target edge (px), the most a GPU splits an edge
#else
// lane WATERLOOK1 (src/features/water/cellocean.h): the wind's wave spectrum on every baked water body, normals only.

Texture2DArray<float4> waterOceanB : register(t21, space0); SamplerState _waterOceanB_sampler : register(s21, space0);	// 6 layers ( sea 0-2, fresh 3-5 ): ( sx, sy, Dxx, Dyy ), mipmapped
Texture2DArray<float4> waterOceanC : register(t22, space0); SamplerState _waterOceanC_sampler : register(s22, space0);	// ( sx^2, sy^2, sx sy, Dxy ), mipmapped
// step 2: the heights and the chop move the water's vertices near the camera (fo4_waterdisp.tesc / .tese)
Texture2DArray<float4> waterOceanD : register(t23, space0); SamplerState _waterOceanD_sampler : register(s23, space0);	// ( h, Dx, Dy, 0 ) m, mipmapped (box means)

static const float kOcMetre = 0.0142875;	// m a game unit
static const float kOcGrid = 2.3255814;	// triangle grid cells a tile (1 / 0.43)
static const float kOcPatchK = 1.602095;	// E[ m^2 ] = 1 (notes/waterlook1/tools/patch_k.py)

struct WwOcean
{
	float2 slope;	// the normal's xy / z (-dh/dx, -dh/dy)
	float var;	// slope variance (x + y) the pixel cannot show: the mips' loss and the tail
	float w;	// how much of the pixel is a baked body (0: today's water)
	float white;	// whitecap cover
	float cau;	// the caustic net's path factor (curvature / the net's)
};

uint wwOcMix( uint x )
{
	x ^= x >> 16;
	x *= 0x7feb352du;
	x ^= x >> 15;
	x *= 0x846ca68bu;
	x ^= x >> 16;
	return x;
}

float wwOcHash01( int2 c, uint seed )
{
	uint2 u = uint2( c & 1023 );
	return float( wwOcMix( u.x * 0x8da6b343u ^ u.y * 0xd8163841u ^ seed ) ) * ( 1.0 / 4294967296.0 );
}

float wwOcValue( float2 p, uint seed )
{
	float2 i = floor( p );
	float2 f = p - i;
	f = f * f * ( 3.0 - 2.0 * f );
	int2 c = int2( i );
	float a = wwOcHash01( c, seed ), b = wwOcHash01( c + int2( 1, 0 ), seed );
	float d = wwOcHash01( c + int2( 0, 1 ), seed ), e = wwOcHash01( c + int2( 1, 1 ), seed );
	return lerp( lerp( a, b, f.x ), lerp( d, e, f.x ), f.y );
}

// the wind patch factor (mean square 1): glassy where the gusts do not reach, cat's-paws where they do
float wwOcPatch( float2 P, int set )
{
	float2 p = ( P + ( set == 0 ? waterOceanPatch.xy : waterOceanPatch.zw ) ) / waterOceanSetB[set].z;
	float n = 0.65 * wwOcValue( p, 0x2545f491u ) + 0.35 * wwOcValue( 2.0 * p + float2( 17.0, 31.0 ), 0x9e3779b9u );
	return kOcPatchK * smoothstep( 0.30, 0.70, n );
}

float2 wwOcOffset( int2 v, uint seed )
{
	uint h = wwOcMix( uint( v.x ) * 0x8da6b343u ^ uint( v.y ) * 0xd8163841u ^ seed );
	uint h2 = wwOcMix( h ^ 0x68e31da4u );
	return float2( float( h ), float( h2 ) ) * ( 1.0 / 4294967296.0 );
}

/* one cascade at tile uv: the blend of three randomly shifted copies on a triangle grid, weights normalised to keep
 * the variance (S = sum w S_i / sqrt( sum w^2 )); out B ( sx, sy, Dxx, Dyy ), Dxy, the lost slope variance */
float4 wwOcCascade( float2 uv, float2 gx, float2 gy, float layer, uint seed, out float dxy, out float lost )
{
	if ( ( waterOceanRed & 2 ) != 0 ) {
		float4 B = waterOceanB.SampleGrad( _waterOceanB_sampler, float3( uv, layer ), gx, gy );
		float4 C = waterOceanC.SampleGrad( _waterOceanC_sampler, float3( uv, layer ), gx, gy );
		dxy = C.w;
		lost = max( C.x - B.x * B.x, 0.0 ) + max( C.y - B.y * B.y, 0.0 );
		return B;
	}
	float2 s = float2( uv.x, -0.57735027 * uv.x + 1.15470054 * uv.y ) * kOcGrid;
	int2 b = int2( floor( s ) );
	float2 f = s - float2( b );
	float z = 1.0 - f.x - f.y;
	float3 w;
	int2 v0, v1, v2;
	if ( z > 0.0 ) {
		w = float3( z, f.y, f.x );
		v0 = b;
		v1 = b + int2( 0, 1 );
		v2 = b + int2( 1, 0 );
	} else {
		w = float3( -z, 1.0 - f.y, 1.0 - f.x );
		v0 = b + int2( 1, 1 );
		v1 = b + int2( 1, 0 );
		v2 = b + int2( 0, 1 );
	}
	float4 B0 = waterOceanB.SampleGrad( _waterOceanB_sampler, float3( uv + wwOcOffset( v0, seed ), layer ), gx, gy );
	float4 B1 = waterOceanB.SampleGrad( _waterOceanB_sampler, float3( uv + wwOcOffset( v1, seed ), layer ), gx, gy );
	float4 B2 = waterOceanB.SampleGrad( _waterOceanB_sampler, float3( uv + wwOcOffset( v2, seed ), layer ), gx, gy );
	float4 C0 = waterOceanC.SampleGrad( _waterOceanC_sampler, float3( uv + wwOcOffset( v0, seed ), layer ), gx, gy );
	float4 C1 = waterOceanC.SampleGrad( _waterOceanC_sampler, float3( uv + wwOcOffset( v1, seed ), layer ), gx, gy );
	float4 C2 = waterOceanC.SampleGrad( _waterOceanC_sampler, float3( uv + wwOcOffset( v2, seed ), layer ), gx, gy );
	float n2 = dot( w, w );
	float rn = rsqrt( max( n2, 1e-6 ) );
	dxy = ( w.x * C0.w + w.y * C1.w + w.z * C2.w ) * rn;
	float3 l = float3( max( C0.x - B0.x * B0.x, 0.0 ) + max( C0.y - B0.y * B0.y, 0.0 ),
		max( C1.x - B1.x * B1.x, 0.0 ) + max( C1.y - B1.y * B1.y, 0.0 ),
		max( C2.x - B2.x * B2.x, 0.0 ) + max( C2.y - B2.y * B2.y, 0.0 ) );
	lost = dot( w * w, l ) / max( n2, 1e-6 );
	return ( w.x * B0 + w.y * B1 + w.z * B2 ) * rn;
}

// the wind sea's Hs at fetch F (m): fetch-limited, capped at fully developed (the foam's law, WATERFOAMDETAIL1)
float wwOcHw( float U, float F )
{
	return min( 0.0016 * U * sqrt( F / 9.81 ), 0.21 * U * U / 9.81 );
}

/* the body at world xy P (u) from the 4 texels around it as the swell reads it (water_waves.glsl): the sea takes the
 * sea set, rivers and lakes the fresh one, each scaled by its own fetch (sqrt of its baked area; a river's is its width,
 * 40 m); tiny bodies, placed and interior water: -1. Out: the share of P on that set's corners, their blended gain,
 * the cascades' wind patch gains, the patch factor and its weight. */
int wwOcBody( float2 P, out float ws, out float a, out float3 gc, out float m, out float pw )
{
	ws = 0.0;
	a = 0.0;
	gc = (float3)( 1.0 );
	m = 1.0;
	pw = 0.0;
	float tex = waterInfoGrid.z;
	float2 g = ( P - waterInfoGrid.xy ) / tex - 0.5;
	float2 t0 = floor( g );
	float2 f = g - t0;
	float2 c0 = waterInfoGrid.xy + ( t0 + 0.5 ) * tex;
	uint4 wd = uint4( wwWaterInfoWord( c0 ), wwWaterInfoWord( c0 + float2( tex, 0.0 ) ),
		wwWaterInfoWord( c0 + float2( 0.0, tex ) ), wwWaterInfoWord( c0 + float2( tex, tex ) ) );
	float4 bw = float4( ( 1.0 - f.x ) * ( 1.0 - f.y ), f.x * ( 1.0 - f.y ), ( 1.0 - f.x ) * f.y, f.x * f.y );
	uint4 id = wd & 0xFFFFu;
	float U = waterOceanWind.z;
	// each wet corner's set and gain; the set of the largest weight wins, its corners' gains blend
	float4 cset = (float4)( -1.0 ), cgain = (float4)( 0.0 );
	float best = -1.0;
	int set = -1;
	for ( int k = 0; k < 4; k++ ) {
		if ( id[k] == 0u )
			continue;
		float4 row = waterInfoBodies.Load( int3(int2( int( id[k] % 256u ), int( id[k] / 256u ) ), 0) );
		int cls = clamp( int( row.g + 0.5 ), 0, 2 );
		if ( ( int( row.b + 0.5 ) & 16 ) != 0 )
			continue;	// tiny: today's water
		int s = cls == 0 ? 0 : 1;
		float F = sqrt( max( row.a, 1.0 ) ) * tex * kOcMetre;
		if ( cls == 1 )
			F = min( F, 40.0 );
		float4 S = waterOceanSet[s];
		float hw = S.y * wwOcHw( U, F ) / max( wwOcHw( U, waterOceanSetB[s].x ), 1e-6 );
		cset[k] = float( s );
		cgain[k] = clamp( sqrt( hw * hw + S.z * S.z ) / max( S.x, 1e-6 ), 0.15, 3.0 );
		if ( bw[k] > best ) {
			best = bw[k];
			set = s;
		}
	}
	if ( set < 0 )
		return -1;
	float4 same = float4( (cset == (float4)( float( set ) )) );
	ws = dot( bw, same );
	a = dot( bw * same, cgain ) / max( ws, 1e-6 );
	m = ( waterOceanRed & 4 ) != 0 ? 1.0 : wwOcPatch( P, set );
	pw = ( waterOceanRed & 4 ) != 0 ? 0.0 : 1.0 - 0.7 * clamp( ( U - 3.0 ) / 7.0, 0.0, 1.0 );
	gc = float3( 1.0, lerp( 1.0, m, 0.5 * pw ), lerp( 1.0, m, pw ) );
	return set;
}

/* lane FIX1031B (LATE ITEM 8, foam): whitecap cover from the folding chop. The old ramp smoothstep( 0.15, 0.55, J ) sat
 * under the lowest J our spectrum makes (p0.1 0.63 at 9.9 m/s): 0 cover at every wind (red oldwhite). The threshold is
 * now fitted per wind so the near sea's mean cover equals the measured whitecap fraction, Monahan and O'Muircheartaigh
 * 1980: W = 3.84e-6 U^3.41 (0.09 % at 5 m/s, 1 % at 10, 3.9 % at 15), on WATERLOOK1's float64 ocean model over the 3 sea
 * cascades (notes/fix1031b/water/tools/whitecap_fit.py); ramp half width 0.10. */
float wwOcWhite( float J, float U )
{
	const float kU[8] = { 3.0, 4.0, 5.0, 6.0, 8.0, 10.0, 12.0, 15.0 };
	const float kJ[8] = { 0.7000, 0.6907, 0.6852, 0.6827, 0.6836, 0.6870, 0.6944, 0.7151 };
	float u = clamp( U, 3.0, 15.0 ), jt = kJ[7];
	for ( int i = 0; i < 7; i++ )
		if ( u <= kU[i + 1] ) {
			jt = lerp( kJ[i], kJ[i + 1], ( u - kU[i] ) / ( kU[i + 1] - kU[i] ) );
			break;
		}
	return ( 1.0 - smoothstep( jt - 0.10, jt + 0.10, J ) ) * smoothstep( 2.0, 3.0, U );
}

/* the spectrum at world xy P (u); dPx, dPy its screen derivatives; w = 0 off a baked body (today's water). Q: where
 * the cascades are read (step 5: the flow carries the waves, so a river reads them upstream); the body is P's, its
 * set (0 sea, 1 fresh, -1 none) in setOut */
WwOcean wwWaterOceanQ( float2 P, float2 Q, float2 dPx, float2 dPy, out int setOut )
{
	WwOcean o = { (float2)( 0.0 ), 0.0, 0.0, 0.0, 1.0 };
	float ws, a, m, pw;
	float3 gc;
	int set = wwOcBody( P, ws, a, gc, m, pw );
	setOut = set;
	if ( set < 0 )
		return o;
	o.w = ws;
	float U = waterOceanWind.z;

	// the tiles run along the wind: rotate world metres into them, and the slopes back
	float2 d = waterOceanWind.xy, e = float2( -d.y, d.x );
	float2 Pm = Q * kOcMetre, gxm = dPx * kOcMetre, gym = dPy * kOcMetre;
	float2 q = float2( dot( Pm, d ), dot( Pm, e ) ), qx = float2( dot( gxm, d ), dot( gxm, e ) ), qy = float2( dot( gym, d ), dot( gym, e ) );
	float4 iL = waterOceanInvL[set];
	float4 B = (float4)( 0.0 );
	float dxy = 0.0, var = 0.0;
	for ( int c = 0; c < 3; c++ ) {
		float il = iL[c], dc, lc;
		float4 Bc = wwOcCascade( q * il, qx * il, qy * il, float( set * 3 + c ), 0x1b873593u * uint( set * 3 + c + 1 ), dc, lc );
		B += gc[c] * Bc;
		dxy += gc[c] * dc;
		var += gc[c] * gc[c] * lc;
	}
	B *= a;
	dxy *= a;
	var = ( waterOceanRed & 8 ) != 0 ? 0.0 : a * a * ( var + waterOceanSet[set].w * lerp( 1.0, m * m, pw ) );
	float lam = ( waterOceanRed & 16 ) != 0 ? 0.0 : iL.w;
	float jx = 1.0 + lam * B.z, jy = 1.0 + lam * B.w;
	float2 st = float2( B.x / max( jx, 0.2 ), B.y / max( jy, 0.2 ) );
	o.slope = -( st.x * d + st.y * e );
	o.var = var;
	float J = jx * jy - lam * lam * dxy * dxy;
	o.white = ( waterOceanRed & 2048 ) != 0 ? ( 1.0 - smoothstep( 0.15, 0.55, J ) ) * smoothstep( 4.0, 8.0, U ) * ws
		: wwOcWhite( J, U ) * ws;
	o.cau = clamp( a * waterOceanSetB[set].y * gc.z / max( waterOceanWind.w, 1e-3 ), 0.0, 3.0 );
	return o;
}

WwOcean wwWaterOcean( float2 P, float2 dPx, float2 dPy )
{
	int set;
	return wwWaterOceanQ( P, P, dPx, dPy, set );
}

// one cascade's ( h, Dx, Dy ) at tile uv and mip lod: the slopes' blend and offsets, so heights and normals agree
float3 wwOcCascadeD( float2 uv, float lod, float layer, uint seed )
{
	if ( ( waterOceanRed & 2 ) != 0 )
		return waterOceanD.SampleLevel( _waterOceanD_sampler, float3( uv, layer ), lod ).xyz;
	float2 s = float2( uv.x, -0.57735027 * uv.x + 1.15470054 * uv.y ) * kOcGrid;
	int2 b = int2( floor( s ) );
	float2 f = s - float2( b );
	float z = 1.0 - f.x - f.y;
	float3 w;
	int2 v0, v1, v2;
	if ( z > 0.0 ) {
		w = float3( z, f.y, f.x );
		v0 = b;
		v1 = b + int2( 0, 1 );
		v2 = b + int2( 1, 0 );
	} else {
		w = float3( -z, 1.0 - f.y, 1.0 - f.x );
		v0 = b + int2( 1, 1 );
		v1 = b + int2( 1, 0 );
		v2 = b + int2( 0, 1 );
	}
	float3 D0 = waterOceanD.SampleLevel( _waterOceanD_sampler, float3( uv + wwOcOffset( v0, seed ), layer ), lod ).xyz;
	float3 D1 = waterOceanD.SampleLevel( _waterOceanD_sampler, float3( uv + wwOcOffset( v1, seed ), layer ), lod ).xyz;
	float3 D2 = waterOceanD.SampleLevel( _waterOceanD_sampler, float3( uv + wwOcOffset( v2, seed ), layer ), lod ).xyz;
	return ( w.x * D0 + w.y * D1 + w.z * D2 ) * rsqrt( max( dot( w, w ), 1e-6 ) );
}

/* the water's displacement (u) at undisplaced world xy P: x, y the chop (lambda D, as the normals' Jacobian), z the
 * height; step = the vertices' spacing there (u), which picks each cascade's mip (no wave shorter than two vertices) */
float3 wwWaterOceanDisp( float2 P, float step )
{
	float ws, a, m, pw;
	float3 gc;
	int set = wwOcBody( P, ws, a, gc, m, pw );
	if ( set < 0 )
		return (float3)( 0.0 );
	float2 d = waterOceanWind.xy, e = float2( -d.y, d.x );
	float2 Pm = P * kOcMetre;
	float2 q = float2( dot( Pm, d ), dot( Pm, e ) );
	float4 iL = waterOceanInvL[set];
	float texels = step * kOcMetre * 128.0;	// tile texels a vertex step, times the tile's 1 / L below
	float3 H = (float3)( 0.0 );
	for ( int c = 0; c < 3; c++ ) {
		float il = iL[c];
		H += gc[c] * wwOcCascadeD( q * il, max( log2( max( texels * il, 1e-6 ) ), 0.0 ), float( set * 3 + c ),
			0x1b873593u * uint( set * 3 + c + 1 ) );
	}
	H *= a * ws;
	float lam = ( waterOceanRed & 16 ) != 0 ? 0.0 : iL.w;
	return float3( lam * ( H.y * d + H.z * e ), H.x ) / kOcMetre;
}
#endif	// WW_LOOSE_UNIFORMS
#else	// WW_HLSL_SRC: the per-variant files' half (HLSLBULK1, WW_FN_<name> picks functions); it goes with the last of them
// Shared HLSL for res/shaders/features/water/water_ocean.glsl (include-only GLSL), HLSLBULK1 round 2 (notes/hlslbulk1/tools/factor.py). No bindings: the
// functions below are the converted ones, they read the declarations of the shader that includes them (its cbuffers,
// resources, statics and LooseUniforms: the per-module shim and the entry own those). Define WW_FN_<name> for each
// wanted function before the #include (and #undef after), or WW_FN_ALL for all of them; a _V<n> suffix is a variant
// of the same GLSL function (a different define set at the entry). Fusion Shaders (F4FS) reuses the math from here.
// Included by: fo4_waterlook_ps.hlsl, fo4_waterlookd_ps.hlsl, fo4_waterlookdp_ps.hlsl, fo4_waterlookp_ps.hlsl
// Not here (declarations, or no converted entry carried them): wwOcWhite, wwWaterOceanQ, wwOcCascadeD, wwWaterOceanDisp
#ifdef WW_FN_ALL
static const float kOcMetre = 0.0142875;
static const float kOcGrid = 2.3255814;
static const float kOcPatchK = 1.602095;
#endif

#if defined(WW_FN_wwOcMix) || defined(WW_FN_ALL)
uint wwOcMix(inout uint x)
{
    x ^= (x >> uint(16));
    x *= 2146121005u;
    x ^= (x >> uint(15));
    x *= 2221713035u;
    x ^= (x >> uint(16));
    return x;
}
#endif

#if defined(WW_FN_wwOcHash01) || defined(WW_FN_ALL)
float wwOcHash01(int2 c, uint seed)
{
    uint2 u = uint2(c & int2(1023, 1023));
    uint wwOcMixPart = ((u.x * 2376512323u) ^ (u.y * 3625334849u)) ^ seed;
    uint wwOcMixRet = wwOcMix(wwOcMixPart);
    return float(wwOcMixRet) * 2.3283064365386962890625e-10f;
}
#endif

#if defined(WW_FN_wwOcValue) || defined(WW_FN_ALL)
float wwOcValue(float2 p, uint seed)
{
    float2 i = floor(p);
    float2 f = p - i;
    f = (f * f) * (3.0f.xx - (f * 2.0f));
    int2 c = int2(i);
    int2 wwOcMixPart = c;
    uint wwOcMixRet = seed;
    float a = wwOcHash01(wwOcMixPart, wwOcMixRet);
    int2 bPart2 = c + int2(1, 0);
    uint bPart = seed;
    float b = wwOcHash01(bPart2, bPart);
    int2 dPart2 = c + int2(0, 1);
    uint dPart = seed;
    // the tiles run along the wind: rotate world metres into them, and the slopes back
    float d = wwOcHash01(dPart2, dPart);
    int2 ePart2 = c + int2(1, 1);
    uint ePart = seed;
    float e = wwOcHash01(ePart2, ePart);
    return lerp(lerp(a, b, f.x), lerp(d, e, f.x), f.y);
}
#endif

#if defined(WW_FN_wwOcPatch) || defined(WW_FN_ALL)
float wwOcPatch(float2 P, int set)
{
    float2 wwOcMixPart;
    if (set == 0)
    {
        wwOcMixPart = waterOceanPatch.xy;
    }
    else
    {
        wwOcMixPart = waterOceanPatch.zw;
    }
    float2 p = (P + wwOcMixPart) / waterOceanSetB[set].z.xx;
    float2 wwOcMixRet = p;
    uint bPart2 = 625341585u;
    float2 bPart = (p * 2.0f) + float2(17.0f, 31.0f);
    uint dPart2 = 2654435769u;
    float n = (0.64999997615814208984375f * wwOcValue(wwOcMixRet, bPart2)) + (0.3499999940395355224609375f * wwOcValue(bPart, dPart2));
    return 1.60209500789642333984375f * smoothstep(0.300000011920928955078125f, 0.699999988079071044921875f, n);
}
#endif

#if defined(WW_FN_wwOcOffset) || defined(WW_FN_ALL)
float2 wwOcOffset(int2 v, uint seed)
{
    uint wwOcMixPart = ((uint(v.x) * 2376512323u) ^ (uint(v.y) * 3625334849u)) ^ seed;
    uint wwOcMixRet = wwOcMix(wwOcMixPart);
    uint h = wwOcMixRet;
    uint bPart2 = h ^ 1759714724u;
    uint bPart = wwOcMix(bPart2);
    uint h2 = bPart;
    return float2(float(h), float(h2)) * 2.3283064365386962890625e-10f;
}
#endif

#if defined(WW_FN_wwOcCascade) || defined(WW_FN_ALL)
float4 wwOcCascade(float2 uv, float2 gx, float2 gy, float layer, uint seed, out float dxy, out float lost)
{
    if ((waterOceanRed & 2) != 0)
    {
        float4 B = waterOceanB.SampleGrad(_waterOceanB_sampler, float3(uv, layer), gx, gy);
        float4 C_1 = waterOceanC.SampleGrad(_waterOceanC_sampler, float3(uv, layer), gx, gy);
        dxy = C_1.w;
        lost = max(C_1.x - (B.x * B.x), 0.0f) + max(C_1.y - (B.y * B.y), 0.0f);
        return B;
    }
    float2 s = float2(uv.x, ((-0.57735025882720947265625f) * uv.x) + (1.1547005176544189453125f * uv.y)) * 2.3255813121795654296875f;
    int2 b = int2(floor(s));
    float2 f = s - float2(b);
    float z = (1.0f - f.x) - f.y;
    float3 w;
    int2 v0;
    int2 v1;
    int2 v2;
    if (z > 0.0f)
    {
        w = float3(z, f.y, f.x);
        v0 = b;
        v1 = b + int2(0, 1);
        v2 = b + int2(1, 0);
    }
    else
    {
        w = float3(-z, 1.0f - f.y, 1.0f - f.x);
        v0 = b + int2(1, 1);
        v1 = b + int2(1, 0);
        v2 = b + int2(0, 1);
    }
    int2 wwOcMixPart = v0;
    uint wwOcMixRet = seed;
    float4 B0 = waterOceanB.SampleGrad(_waterOceanB_sampler, float3(uv + wwOcOffset(wwOcMixPart, wwOcMixRet), layer), gx, gy);
    int2 bPart2 = v1;
    uint bPart = seed;
    float4 B1 = waterOceanB.SampleGrad(_waterOceanB_sampler, float3(uv + wwOcOffset(bPart2, bPart), layer), gx, gy);
    int2 dPart2 = v2;
    uint dPart = seed;
    float4 B2 = waterOceanB.SampleGrad(_waterOceanB_sampler, float3(uv + wwOcOffset(dPart2, dPart), layer), gx, gy);
    int2 ePart2 = v0;
    uint ePart = seed;
    float4 C0 = waterOceanC.SampleGrad(_waterOceanC_sampler, float3(uv + wwOcOffset(ePart2, ePart), layer), gx, gy);
    int2 C1Part2 = v1;
    uint C1Part = seed;
    float4 C1 = waterOceanC.SampleGrad(_waterOceanC_sampler, float3(uv + wwOcOffset(C1Part2, C1Part), layer), gx, gy);
    int2 C2Part2 = v2;
    uint C2Part = seed;
    float4 C2 = waterOceanC.SampleGrad(_waterOceanC_sampler, float3(uv + wwOcOffset(C2Part2, C2Part), layer), gx, gy);
    float n2 = dot(w, w);
    float rn = rsqrt(max(n2, 9.9999999747524270787835121154785e-07f));
    dxy = (((w.x * C0.w) + (w.y * C1.w)) + (w.z * C2.w)) * rn;
    float3 l = float3(max(C0.x - (B0.x * B0.x), 0.0f) + max(C0.y - (B0.y * B0.y), 0.0f), max(C1.x - (B1.x * B1.x), 0.0f) + max(C1.y - (B1.y * B1.y), 0.0f), max(C2.x - (B2.x * B2.x), 0.0f) + max(C2.y - (B2.y * B2.y), 0.0f));
    lost = dot(w * w, l) / max(n2, 9.9999999747524270787835121154785e-07f);
    return (((B0 * w.x) + (B1 * w.y)) + (B2 * w.z)) * rn;
}
#endif

#if defined(WW_FN_wwOcHw) || defined(WW_FN_ALL)
float wwOcHw(float U, float F)
{
    return min((0.001599999959580600261688232421875f * U) * sqrt(F / 9.81000041961669921875f), ((0.20999999344348907470703125f * U) * U) / 9.81000041961669921875f);
}
#endif

#if defined(WW_FN_wwOcBody) || defined(WW_FN_ALL)
int wwOcBody(float2 P, inout float ws, out float a, out float3 gc, inout float m, inout float pw)
{
    ws = 0.0f;
    a = 0.0f;
    gc = 1.0f.xxx;
    m = 1.0f;
    pw = 0.0f;
    float tex = waterInfoGrid.z;
    float2 g = ((P - waterInfoGrid.xy) / tex.xx) - 0.5f.xx;
    float2 t0 = floor(g);
    float2 f = g - t0;
    float2 c0 = waterInfoGrid.xy + ((t0 + 0.5f.xx) * tex);
    float2 wwOcMixPart = c0;
    float2 wwOcMixRet = c0 + float2(tex, 0.0f);
    float2 bPart2 = c0 + float2(0.0f, tex);
    float2 bPart = c0 + tex.xx;
    uint4 wd = uint4(wwWaterInfoWord(wwOcMixPart), wwWaterInfoWord(wwOcMixRet), wwWaterInfoWord(bPart2), wwWaterInfoWord(bPart));
    float4 bw = float4((1.0f - f.x) * (1.0f - f.y), f.x * (1.0f - f.y), (1.0f - f.x) * f.y, f.x * f.y);
    uint4 id = wd & uint4(65535u, 65535u, 65535u, 65535u);
    float U = waterOceanWind.z;
    // each wet corner's set and gain; the set of the largest weight wins, its corners' gains blend
    float4 cset = (-1.0f).xxxx;
    float4 cgain = 0.0f.xxxx;
    float best = -1.0f;
    int set = -1;
    for (int k = 0; k < 4; k++)
    {
        if (id[k] == 0u)
        {
            continue;
        }
        float4 row = waterInfoBodies.Load(int3(int2(int(id[k] % 256u), int(id[k] / 256u)), 0));
        int cls = clamp(int(row.y + 0.5f), 0, 2);
        if ((int(row.z + 0.5f) & 16) != 0)
        {
            continue;
        }
        int s = (cls == 0) ? 0 : 1;
        float F = (sqrt(max(row.w, 1.0f)) * tex) * 0.01428749971091747283935546875f;
        if (cls == 1)
        {
            F = min(F, 40.0f);
        }
        float4 S = waterOceanSet[s];
        float dPart2 = U;
        float dPart = F;
        float ePart2 = U;
        float ePart = waterOceanSetB[s].x;
        float hw = (S.y * wwOcHw(dPart2, dPart)) / max(wwOcHw(ePart2, ePart), 9.9999999747524270787835121154785e-07f);
        cset[k] = float(s);
        cgain[k] = clamp(sqrt((hw * hw) + (S.z * S.z)) / max(S.x, 9.9999999747524270787835121154785e-07f), 0.1500000059604644775390625f, 3.0f);
        if (bw[k] > best)
        {
            best = bw[k];
            set = s;
        }
    }
    if (set < 0)
    {
        return -1;
    }
    float4 C1Part2 = float(set).xxxx;
    float4 same = float4(bool4(cset.x == C1Part2.x, cset.y == C1Part2.y, cset.z == C1Part2.z, cset.w == C1Part2.w));
    ws = dot(bw, same);
    a = dot(bw * same, cgain) / max(ws, 9.9999999747524270787835121154785e-07f);
    float C1Part;
    if ((waterOceanRed & 4) != 0)
    {
        C1Part = 1.0f;
    }
    else
    {
        float2 C2Part2 = P;
        int C2Part = set;
        C1Part = wwOcPatch(C2Part2, C2Part);
    }
    m = C1Part;
    float pwPick;
    if ((waterOceanRed & 4) != 0)
    {
        pwPick = 0.0f;
    }
    else
    {
        pwPick = 1.0f - (0.699999988079071044921875f * clamp((U - 3.0f) / 7.0f, 0.0f, 1.0f));
    }
    pw = pwPick;
    gc = float3(1.0f, lerp(1.0f, m, 0.5f * pw), lerp(1.0f, m, pw));
    return set;
}
#endif

#if defined(WW_FN_wwWaterOcean) || defined(WW_FN_ALL)
WwOcean wwWaterOcean(float2 P, float2 dPx, float2 dPy)
{
    float2 wwOcMixPart = P;
    float2 wwOcMixRet = P;
    float2 bPart2 = dPx;
    float2 bPart = dPy;
    int dPart2;
    WwOcean dPart = wwWaterOceanQ(wwOcMixPart, wwOcMixRet, bPart2, bPart, dPart2);
    int set = dPart2;
    return dPart;
}
#endif
#endif	// WW_HLSL_SRC
