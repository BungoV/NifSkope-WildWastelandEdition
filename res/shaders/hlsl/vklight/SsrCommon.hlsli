// The game's screen-space reflections, shared math (lane VKLIGHT1, 2026-11-12): the twin of
// res/shaders/features/reflections/cell_ssr.frag stages 1 (the ray), 2 (the hierarchical march) and 3 / 4 (the 5-tap blur),
// in the GL file's expression order. No bindings: the caller defines
//   SSR_GBUF( t )          full size g-buffer texel (view normal, linear depth), bottom row first
//   SSR_ZMIP( m, t )       depth mip m (1..4) texel, inside the mip
//   SSR_SCENE( t )         full size scene texel (alpha = reflection flag); SSR_SCENE_AT( uv ) the same, bilinear
//   SSR_WATER( t )         the water's ray (only read when haveWaterRay)
//   SSR_SRC( q )           the previous stage's target texel; SSR_SRC_AT( uv ) the same, bilinear
// Not here: stage 0 (the stencil flag), 5 (the bounce donor), the previous-frame hit (ssrPrev) and the water look
// (cell_ssrwater.prog, WW_SSR_WATER_LOOK).
#ifndef F4FX_SSR_COMMON_HLSLI
#define F4FX_SSR_COMMON_HLSLI

struct F4FX_SsrParams
{
	int2 fullSize;
	int2 halfSize;
	float2 proj;		// P00, P11
	float2 clip;		// near, far (game units)
	float4 k;			// the game's four: color scale, angle gate, normal z scale, confidence scale
	float3 viewToWorld[3];	// GL columns
	int red;			// 2 no gap refusal, 4 no fade
	bool haveWaterRay;
	bool opaqueRays;
};

int2 F4FX_SsrMipSize( int2 full, int m )
{
	int2 s = full;
	for ( int i = 0; i < m; i++ )
		s = max( s / 2, int2( 1, 1 ) );
	return s;
}

// the game's load: outside the mip reads 0
float F4FX_SsrZLoad( int m, float2 cell, F4FX_SsrParams P_ )
{
	int2 t = int2( cell );
	int2 ms = F4FX_SsrMipSize( P_.fullSize, m );
	if ( t.x < 0 || t.y < 0 || t.x >= ms.x || t.y >= ms.y )
		return 0.0f;
	return SSR_ZMIP( m, t );
}

bool F4FX_SsrRayOut( float3 p, F4FX_SsrParams P_ )
{
	return p.x <= 0.0f || p.x >= 1.0f || p.y <= 0.0f || p.y >= 1.0f || 1.0f / p.z >= P_.clip.y;
}

// the view position at a half-size uv: GL (NV) lowers ( 2u - 1 ) * z / p as ( 2u * z - z ) * ( 1 / p ), unfused
float3 F4FX_SsrViewPos( float2 uv, float z, F4FX_SsrParams P_ )
{
	precise float tx = ( 2.0f * uv.x ) * z;
	precise float ty = ( -2.0f * uv.y ) * z;
	precise float x = ( tx - z ) * ( 1.0f / P_.proj.x );
	precise float y = ( ty + z ) * ( 1.0f / P_.proj.y );
	return float3( x, y, -z );
}

// stage 1: the reflected ray run back to the near plane (u0, v0, start depth, 1)
float4 F4FX_SsrRay( int2 q, F4FX_SsrParams P_ )
{
	float2 uv = ( float2( q ) + 0.5f ) / float2( P_.halfSize );
	int2 t = min( ( ( 2 * q + 1 ) * P_.fullSize ) / ( 2 * P_.halfSize ), P_.fullSize - 1 );
	t.y = P_.fullSize.y - 1 - t.y;
	float3 P, R;
	float z;
	float4 wr = P_.haveWaterRay ? SSR_WATER( t ) : float4( 0.0f, 0.0f, 0.0f, 0.0f );
	if ( wr.a > 0.0f ) {
		if ( dot( wr.xyz, wr.xyz ) == 0.0f )
			return float4( 0.0f, 0.0f, 0.0f, 0.0f );
		z = wr.a;
		P = F4FX_SsrViewPos( uv, z, P_ );
		R = wr.xyz;
	} else {
		if ( !P_.opaqueRays || SSR_SCENE( t ).a * P_.k.w - 0.01f < 0.0f )
			return float4( 0.0f, 0.0f, 0.0f, 0.0f );
		float4 g = SSR_GBUF( t );
		z = g.a;
		P = F4FX_SsrViewPos( uv, z, P_ );
		float3 V = -normalize( P );
		if ( dot( g.xyz, V ) < 0.0f )
			return float4( 0.0f, 0.0f, 0.0f, 0.0f );
		float3 Nw = mad( P_.viewToWorld[2], g.z, mad( P_.viewToWorld[0], g.x, P_.viewToWorld[1] * g.y ) );	// GL's order (y, x, z)
		Nw.z *= P_.k.z;
		float3 nn = normalize( Nw );
		float3 N = float3( dot( nn, P_.viewToWorld[0] ), dot( nn, P_.viewToWorld[1] ), dot( nn, P_.viewToWorld[2] ) );	// back into the view
		float3 I = -V;	// reflect() as GL lowers it: an fma dot, then I - ( 2 d ) N unfused
		float dI = mad( I.z, N.z, mad( I.y, N.y, I.x * N.x ) );
		precise float3 Rr = I - ( 2.0f * dI ) * N;
		R = Rr;
		if ( -R.z <= P_.k.y )	// GL (NV) flips !( a > b ) to a <= b: a NaN ray (sky, zero normal) goes on, as there
			return float4( 0.0f, 0.0f, 0.0f, 0.0f );
	}
	float3 Q = P + 1000.0f * R;
	float zq = -Q.z;
	precise float2 uvq = float2( 0.5f + 0.5f * P_.proj.x * Q.x / zq, 0.5f - 0.5f * P_.proj.y * Q.y / zq );	// GL: unfused
	// the screen line is straight in (u, v, 1 / depth): run it back to depth = near
	float s = ( 1.0f / P_.clip.x - 1.0f / z ) / ( 1.0f / zq - 1.0f / z );
	precise float2 uv0 = mad( uvq - uv, s, uv );	// GL: one fma
	return float4( uv0, z, 1.0f );
}

static const float F4FX_SsrDither[16] = { 0.0f, 0.5f, 0.125f, 0.625f, 0.75f, 0.22f, 0.875f, 0.375f,
                                          0.1875f, 0.6875f, 0.0625f, 0.5625f, 0.9375f, 0.4375f, 0.8125f, 0.3125f };

// stage 2: the hierarchical march over depth mips 1..4 (32 steps); rgb the hit's colour, alpha the confidence squared
float4 F4FX_SsrMarch( int2 q, F4FX_SsrParams P_ )
{
	const int steps = 32;
	float2 uv = ( float2( q ) + 0.5f ) / float2( P_.halfSize );
	float4 ray = SSR_SRC( q );
	float z0 = ray.z;
	if ( !( P_.clip.x < z0 ) )
		return float4( 0.0f, 0.0f, 0.0f, 0.0f );
	float dj = F4FX_SsrDither[( q.y & 3 ) + 4 * ( q.x & 3 )];
	float3 A = float3( ray.xy, 1.0f / P_.clip.x + ( dj - 0.5f ) * 0.004f );
	float3 D = float3( uv, 1.0f / z0 ) - A;
	float2 S = uv + D.xy * dj * 0.002f;
	float2 G = floor( float2( P_.halfSize ) / 8.0f );
	float2 cell = floor( G * S );
	int level = 4;
	float2 s01 = float2( D.x >= 0.0f ? 1.0f : 0.0f, D.y >= 0.0f ? 1.0f : 0.0f );
	float2 sgn = 2.0f * s01 - 1.0f;
	float2 stepv;
	float3 p;
	// one cell on: the nearer of the cell's two far edges
	float2 tc = ( ( cell + s01 ) / G - A.xy ) / D.xy;
	bool xfirst = tc.y >= tc.x;
	stepv = sgn * float2( xfirst ? 1.0f : 0.0f, xfirst ? 0.0f : 1.0f );
	cell += stepv;
	p = A + min( tc.x, tc.y ) * D;
	float zfirst = 1.0f / p.z;
	bool alive = true, refused = false;
	int count = 0;
	float d = 0.0f;
	for ( int n = 1; n <= steps; n++ ) {
		alive = alive && level >= 1 && !F4FX_SsrRayOut( p, P_ );
		if ( !alive )
			break;
		d = F4FX_SsrZLoad( level, cell, P_ );
		float rz = 1.0f / p.z;
		bool isL1 = level == 1 && ( P_.red & 2 ) == 0;
		bool ref = refused || ( isL1 && rz - d >= 50.0f );
		if ( d < rz && !ref ) {
			level -= 1;
			float2 hp = p.xy - ( cell + 0.5f ) / G;
			float2 h = float2( hp.x >= 0.0f ? 1.0f : 0.0f, hp.y >= 0.0f ? 1.0f : 0.0f );
			cell = 2.0f * cell + h;
			G *= 2.0f;
			refused = false;
		} else {
			tc = ( ( cell + s01 ) / G - A.xy ) / D.xy;
			xfirst = tc.y >= tc.x;
			stepv = sgn * float2( xfirst ? 1.0f : 0.0f, xfirst ? 0.0f : 1.0f );
			cell += stepv;
			p = A + min( tc.x, tc.y ) * D;
			float rz2 = 1.0f / p.z;
			refused = ref || ( isL1 && rz2 - d >= 50.0f );
			if ( !refused ) {
				if ( d < rz2 ) {
					p = A + ( ( 1.0f / d - A.z ) / D.z ) * D;
					level -= 1;
					G *= 2.0f;
					cell = floor( G * p.xy );
				} else if ( level != 4 ) {
					G = floor( G / 2.0f );
					cell = cell / 2.0f + float2( stepv.x >= 0.0f ? 0.0f : -0.5f, stepv.y >= 0.0f ? 0.0f : -0.5f );
					level += 1;
				}
			}
		}
		count = n;
	}
	if ( F4FX_SsrRayOut( p, P_ ) || count == steps || refused )
		return float4( 0.0f, 0.0f, 0.0f, 0.0f );
	// confidence as GL (NV) lowers it: ( 1 - m^2 ) f2 distributed to fma( -m^2, f2, f2 ); 1 - 25 dz / range as fma( -25, dz * ( 1 / range ), 1 )
	float m1 = min( 2.0f * length( p.xy - 0.5f ), 1.0f );
	float m2 = m1 * m1;
	float f2 = max( 1.0f - 2.0f * length( S - p.xy ), 0.0f );
	float c = mad( -m2, f2, f2 );
	precise float q3 = ( d - zfirst ) * ( 1.0f / ( P_.clip.y - P_.clip.x ) );
	c *= clamp( mad( -25.0f, q3, 1.0f ), 0.0f, 1.0f );
	if ( ( P_.red & 4 ) != 0 )
		c = 1.0f;
	return float4( clamp( SSR_SCENE_AT( float2( p.x, 1.0f - p.y ) ).rgb, 0.0f, 1.0f ), c * c );
}

// stages 3 (across) and 4 (down): the 5-tap blur of the march, lost taps' weight shared by the present ones
float4 F4FX_SsrBlur( int2 q, int stage, F4FX_SsrParams P_ )
{
	const float off[5] = { -3.294215f, -1.407333f, 0.0f, 1.407333f, 3.294215f };
	const float wgt[5] = { 0.093913f, 0.304005f, 0.204164f, 0.304005f, 0.093913f };
	float2 uv = ( float2( q ) + 0.5f ) / float2( P_.halfSize );
	float2 dir = stage == 3 ? float2( 1.0f / float( P_.halfSize.x ), 0.0f ) : float2( 0.0f, 1.0f / float( P_.halfSize.y ) );
	float4 tap[5];
	float lost = 0.0f, present = 0.0f, a = 0.0f;
	for ( int i = 0; i < 5; i++ ) {
		tap[i] = SSR_SRC_AT( uv + dir * off[i] );
		a += wgt[i] * tap[i].a;
		if ( tap[i].a < 0.01f )
			lost += wgt[i];
		else
			present += 1.0f;
	}
	float3 rgb = float3( 0.0f, 0.0f, 0.0f );
	if ( present > 0.0f ) {
		float e = lost / present;
		for ( int j = 0; j < 5; j++ )
			if ( !( tap[j].a < 0.01f ) )
				rgb += ( wgt[j] + e ) * tap[j].rgb;
	}
	return float4( rgb, a );
}

#endif
