// The cell view's screen-space ambient obscurance (the game's, res/shaders/features/ao/cell_ao.frag), bindings-free
// (lane VKLIGHT1, 2026-11-12). Every expression keeps the GL twin's order (tests/vklight/ao gate: GL vs Vulkan per stage).
// Buffers: gbuf full size, bottom row first (view normal, linear depth in game units); depth mips, raw and blur half size
// and down, top row first. Fusion Shaders feeds its own textures and sizes; NifSkope's shim is LightBindingsVk.hlsli.
#ifndef AO_COMMON_HLSLI
#define AO_COMMON_HLSLI

struct F4FX_AoSizes
{
	int2 full;		// mip 0
	int2 half_;		// the obscurance target
	int2 mip[5];	// [0] = full
};

float F4FX_AoZFull( Texture2D<float4> gbuf, F4FX_AoSizes s, int2 t )
{
	t = clamp( t, int2( 0, 0 ), s.full - 1 );
	return gbuf.Load( int3( t.x, s.full.y - 1 - t.y, 0 ) ).a;
}

float F4FX_AoZAt( Texture2D<float4> gbuf, Texture2D<float4> z1, Texture2D<float4> z2, Texture2D<float4> z3, Texture2D<float4> z4,
	F4FX_AoSizes s, int m, int2 t )
{
	if ( m == 0 )
		return F4FX_AoZFull( gbuf, s, t );
	t = clamp( t, int2( 0, 0 ), s.mip[m] - 1 );
	if ( m == 1 )
		return z1.Load( int3( t, 0 ) ).r;
	if ( m == 2 )
		return z2.Load( int3( t, 0 ) ).r;
	if ( m == 3 )
		return z3.Load( int3( t, 0 ) ).r;
	return z4.Load( int3( t, 0 ) ).r;
}

// the texel of mip m a point sample at the center of half-size pixel q reads ((q + 0.5) / halfSize x mip size)
int2 F4FX_AoTexelOf( F4FX_AoSizes s, int m, int2 q )
{
	int2 n = 2 * q + 1;
	int2 t = ( max( n, int2( 0, 0 ) ) * s.mip[m] ) / ( 2 * s.half_ );
	return int2( n.x < 0 ? -1 : t.x, n.y < 0 ? -1 : t.y );
}

// stage 5: the view position at full-size pixel t from the depth pre-pass; w = 0 where nothing was drawn (the sky).
// invProj = clip -> view as 4 GL columns; zlin: zl = the stored linear view depth (game units), else d = window depth
float4 F4FX_AoZPos( Texture2D<float4> zres, Texture2D<float4> zlin, int2 full, float4 invProj[4], float unit, bool useZLin, int2 t )
{
	t = clamp( t, int2( 0, 0 ), full - 1 );
	float2 ndc = ( float2( t ) + 0.5f ) / float2( full ) * 2.0f - 1.0f;
	if ( useZLin ) {
		float zl = zlin.Load( int3( t, 0 ) ).r;
		float4 r = invProj[0] * ndc.x + invProj[1] * ndc.y + invProj[2] * 0.0f + invProj[3] * 1.0f;
		float3 q = r.xyz / r.w;
		// precise: without it the scale is folded into q's divide and x/y miss GL by up to 1e7 ulp (gbuf normals)
		precise float s = -zl / ( unit * q.z );
		precise float3 o = q * s;
		return float4( o, zl > 0.0f ? 1.0f : 0.0f );
	}
	// NVIDIA's GL compiler distributes c2 * ( 2 d - 1 ) into ( c2 d ) 2 - c2 (measured: the plain order is up to 800 ulp off
	// GL's depth, this form matches every bit; notes/vklight1/tools/gbuf_probe.py)
	float d = zres.Load( int3( t, 0 ) ).r;
	precise float4 t2 = invProj[2] * d;
	precise float4 u2 = mad( t2, 2.0f, -invProj[2] );
	float4 v = invProj[0] * ndc.x + invProj[1] * ndc.y + u2 + invProj[3];
	return float4( v.xyz / v.w, d < 1.0f ? 1.0f : 0.0f );
}

// stage 5: view normal + linear depth (game units) from the 3-tap crosses (SAO: the side with the smaller depth step)
float4 F4FX_AoGbuf( Texture2D<float4> zres, Texture2D<float4> zlin, int2 full, float4 invProj[4], float unit, bool useZLin, int2 P )
{
	float4 c = F4FX_AoZPos( zres, zlin, full, invProj, unit, useZLin, P );
	if ( c.w < 0.5f )
		return float4( 0.0f, 0.0f, 0.0f, 1.0e6f );
	float4 l = F4FX_AoZPos( zres, zlin, full, invProj, unit, useZLin, P - int2( 1, 0 ) );
	float4 r = F4FX_AoZPos( zres, zlin, full, invProj, unit, useZLin, P + int2( 1, 0 ) );
	float4 b = F4FX_AoZPos( zres, zlin, full, invProj, unit, useZLin, P - int2( 0, 1 ) );
	float4 u = F4FX_AoZPos( zres, zlin, full, invProj, unit, useZLin, P + int2( 0, 1 ) );
	float3 dl = c.xyz - l.xyz, dr = r.xyz - c.xyz, db = c.xyz - b.xyz, du = u.xyz - c.xyz;
	float el = l.w > 0.5f ? abs( dl.z ) : 1.0e30f, er = r.w > 0.5f ? abs( dr.z ) : 1.0e30f;
	float eb = b.w > 0.5f ? abs( db.z ) : 1.0e30f, eu = u.w > 0.5f ? abs( du.z ) : 1.0e30f;
#ifdef AO_GBUF_RED
	float3 dx = dr, dy = du;	// red: no smaller-step side
#else
	float3 dx = min( el, er ) >= 1.0e30f ? float3( 1.0f, 0.0f, 0.0f ) : el < er ? dl : dr;
	float3 dy = min( eb, eu ) >= 1.0e30f ? float3( 0.0f, 1.0f, 0.0f ) : eb < eu ? db : du;
#endif
	float3 n = cross( dx, dy );
	n = dot( n, n ) > 0.0f ? normalize( n ) : float3( 0.0f, 0.0f, 1.0f );
	if ( dot( n, c.xyz ) > 0.0f )
		n = -n;
	return float4( n, -c.z * unit );
}

// stage 1: the min of 2x2 of the level above (level 1 reads gbuf's depth)
float F4FX_AoMip( Texture2D<float4> src, F4FX_AoSizes s, int level, int2 P )
{
	int2 p = P * 2;
	if ( level == 1 ) {
#ifdef AO_MIP_RED
		return F4FX_AoZFull( src, s, p );	// red: one texel
#else
		return min( min( F4FX_AoZFull( src, s, p ), F4FX_AoZFull( src, s, p + int2( 1, 0 ) ) ),
		            min( F4FX_AoZFull( src, s, p + int2( 0, 1 ) ), F4FX_AoZFull( src, s, p + int2( 1, 1 ) ) ) );
#endif
	}
	int2 e = s.mip[level - 1] - 1;
	return min( min( src.Load( int3( min( p, e ), 0 ) ).r, src.Load( int3( min( p + int2( 1, 0 ), e ), 0 ) ).r ),
	            min( src.Load( int3( min( p + int2( 0, 1 ), e ), 0 ) ).r, src.Load( int3( min( p + int2( 1, 1 ), e ), 0 ) ).r ) );
}

// x - 2 pi floor( x / 2 pi ) in doubles (the GL twin reduces the ~5e6 radian angle exactly); floor through a 32-bit int,
// exact for |x / 2 pi| < 2^31 (DXC narrows a double floor() to float, and widens double( int ) through int64)
#ifdef __spirv__
[[vk::ext_instruction( 111 )]] double F4FX_AoIntToDouble( int x );	// OpConvertSToF
#else
double F4FX_AoIntToDouble( int x ) { return double( x ); }
#endif
float F4FX_AoReduce( float ang )
{
	double ad = double( ang );
	double q = ad * 0.15915494309189535L;
	double t = F4FX_AoIntToDouble( int( q ) );
	double f = t > q ? t - 1.0L : t;
	return float( ad - 6.283185307179586L * f );
}

// stage 2: the raw obscurance and the blur's depth key at half-size pixel P. proj = (P00, P11); params = radius, bias,
// intensity (game units); angles = the still view's angle count; noReset = the plain mean
float2 F4FX_AoRaw( Texture2D<float4> gbuf, Texture2D<float4> z1, Texture2D<float4> z2, Texture2D<float4> z3, Texture2D<float4> z4,
	F4FX_AoSizes s, float2 proj, float3 params, int angles, bool noReset, int2 P )
{
	float2 hs = float2( s.half_ );
	float4 projInfo = float4( -2.0f / ( hs.x * proj.x ), -2.0f / ( hs.y * proj.y ), 1.0f / proj.x, 1.0f / proj.y );
	float z = F4FX_AoZAt( gbuf, z1, z2, z3, z4, s, 0, F4FX_AoTexelOf( s, 0, P ) );
	float3 C = float3( ( float2( P ) + 0.5f ) * projInfo.xy + projInfo.zw, 1.0f ) * z;
	int2 nt = min( 2 * P, s.full - 1 );
	float3 ng = gbuf.Load( int3( nt.x, s.full.y - 1 - nt.y, 0 ) ).xyz;
	float3 n = float3( -ng.x, ng.y, -ng.z );
	float d = clamp( z * 1.42857141e-4f, 0.0f, 1.0f );
	uint hx = uint( P.x ), hy = uint( P.y );
	float hashAngle = float( ( ( hx * 3u ) ^ ( hx * hy + hy ) ) * 10u );
	float2 ndc = ( float2( P ) + 0.5f ) / hs * 2.0f - 1.0f;
#ifdef AO_RAW_RED
	float biasP = params.y + 10.0f * max( d - 0.3f, 0.0f );	// red: no edge bias
#else
	float biasP = params.y + 10.0f * max( d - 0.3f, 0.0f ) + 5.0f * dot( ndc, ndc );
#endif
	float r = params.x;
	float r2 = r * r;
	float r6 = r2 * r;
	r6 *= r6;
	float ssDisk = r * 100.0f / z;
	int K = d <= 0.5f ? max( angles, 1 ) : 1;
	float A = 0.0f, topSum = 0.0f;
	int topN = 0;
	for ( int k = 0; k < K; k++ ) {
		float rnd = d <= 0.5f ? ( float( k ) + 0.5f ) * 3.14159265f / float( K ) : 0.0f;
		precise float rot = rnd + hashAngle;
		float sum = 0.0f;
		for ( int i = 0; i < 5; i++ ) {
			precise float reach = ssDisk * ( float( i ) + 0.5f );
			precise float ssR = reach * 0.2f;
			precise float ang = mad( float( i ) + 0.5f, 2.512f, rot );
			float a = F4FX_AoReduce( ang );
			int m = clamp( int( floor( log2( ssR ) ) ) - 3, 0, 4 );
			int2 Q = P + int2( ssR * float2( cos( a ), sin( a ) ) );
			float qz = F4FX_AoZAt( gbuf, z1, z2, z3, z4, s, m, F4FX_AoTexelOf( s, m, Q ) );
			float3 v = float3( ( float2( Q ) + 0.5f ) * projInfo.xy + projInfo.zw, 1.0f ) * qz - C;
			// the dots as mad chains (GL's order; probes: every tap and sum float-exact)
			precise float vv = mad( v.z, v.z, mad( v.y, v.y, v.x * v.x ) );
			precise float vn = mad( v.z, n.z, mad( v.y, n.y, v.x * n.x ) );
			float f = max( r2 - vv, 0.0f );
			sum += f * f * f * max( ( vn - biasP ) / ( vv + 0.01f ), 0.0f );
		}
		// NV GL: fma( -sum, z / r6, 1 ) (else 12 % of the obscured texels miss by 1-1280 ulp)
		precise float zr = params.z / r6;
		float Ak = max( mad( -sum, zr, 1.0f ), 0.0f );
		A += Ak;
		if ( Ak >= 0.95f ) {
			topSum += Ak;
			topN++;
		}
	}
	A /= float( K );
	if ( !noReset && topN > 0 && A < 0.7f ) {
		const float lam = 0.01005034f;
		// NV GL's lowering, spelled out: top as a reciprocal multiply, the sum's first term and the last add fused.
		// No precise here: it reaches log() and changes its lowering (133 ulp)
		float p = float( topN ) / float( K ), top = topSum * ( 1.0f / float( topN ) );
		float t1 = log( ( top - A ) / ( 0.7f - A ) ) / lam;
		float nu = mad( top - 0.7f, 1.0f / lam, ( 0.7f - A ) * ( 1.0f / ( p + lam ) ) );
		A = mad( nu, 1.0f / ( t1 + 1.0f / p ), A );
	}
	float hi = floor( d * 256.0f );
	float key = hi * 0.00390625f * 0.996108949f + ( d * 256.0f - hi ) * 0.00389105058f;
	return float2( A, key );
}

// stages 3 / 4: the bilateral blur along dir (7 taps 2 pixels apart, cut by the depth key); half = the target's size
float2 F4FX_AoBlur( Texture2D<float4> src, int2 half_, int2 dir, bool noBlur, int2 P )
{
	float2 c = src.Load( int3( P, 0 ) ).rg;
	if ( noBlur )
		return c;
	if ( c.y == 1.0f )
		return float2( 0.0f, c.y );
	const float wt[4] = { 0.15317f, 0.444893f, 0.422649f, 0.392902f };
	float sum = c.x * wt[0];
	float ws = wt[0];
	for ( int j = 1; j <= 3; j++ ) {
		for ( int sg = -1; sg <= 1; sg += 2 ) {
			int2 q = P + dir * ( 2 * j * sg );
			float2 t = float2( 0.0f, 0.0f );
			if ( q.x >= 0 && q.y >= 0 && q.x < half_.x && q.y < half_.y )
				t = src.Load( int3( q, 0 ) ).rg;
#ifdef AO_BLUR_RED
			float w = wt[j];	// red: no depth cut
#else
			float w = max( 1.0f - abs( t.y - c.y ) * 2000.0f, 0.0f ) * wt[j];
#endif
			sum += t.x * w;
			ws += w;
		}
	}
	return float2( sum / ( ws + 0.0001f ), c.y );
}

#endif
