// lane VKLIGHT1 (2026-11-12): the screen-space GI cast (res/shaders/features/gi/cell_ssgi.frag stage 1, lanes SSGI1-3 and
// PERFFIX2: a visibility-bitmask horizon march per slice, the method of Fusion Shaders' SSGI), bindings-free (F4FX_ names).
// The caller defines three reads before the include, so Fusion Shaders can feed it from its own resources:
//   SSGI_GBUF( int2 fullTexel )  float4: view normal (GL axes), linear depth (texelFetch, clamped by the caller)
//   SSGI_SCENE( float2 uv, lod ) float3: the opaque lit linear colour, prefiltered levels 0..4 (nearest mip, linear texels)
//   SSGI_NOISE( int2 p )         float2: the spatiotemporal blue noise sheet, 128 x (64 x 128)
// Not here: the interval pyramid skip (red `hiz` only), the emissive donor subtraction (emissive cast on) and the old
// fill law (red `oldlaw`). Expressions keep the GL twin's order.
#ifndef SSGI_COMMON_HLSLI
#define SSGI_COMMON_HLSLI

#define F4FX_SSGI_PI 3.14159265f

struct F4FX_SsgiParams
{
	int2 fullSize;
	int2 workSize;
	float2 proj;		// the projection's P00, P11
	float radius;		// game units
	float thickness;	// game units behind a tap's front face
	float sourceBrightness;
	float saturation;
	int slices;
	int steps;
	int frameIndex;
	bool blueNoise;		// false: the closed-form interleaved gradient noise
	bool fullBreak;		// the full-mask early out
};

// NVIDIA's GL acos is this polynomial (Abramowitz-Stegun 4.4.45, max error 6.8e-5), not SPIR-V's Acos:
// the GL twin's three acos calls match only through it.
float F4FX_SsgiAcos( float x )
{
	float xa = abs( x );
	float r = mad( mad( mad( -0.0187293f, xa, 0.0742610f ), xa, -0.2121144f ), xa, 1.5707288f ) * sqrt( 1.0f - xa );
	return x < 0.0f ? F4FX_SSGI_PI - r : r;
}

float3 F4FX_SsgiViewPos( float2 uv, float z, float2 proj )
{
	float2 ndc = uv * 2.0f - 1.0f;
	return float3( ndc.x * z / proj.x, ndc.y * z / proj.y, -z );
}

float3 F4FX_SsgiFaceN( float3 n, float3 P )
{
	return dot( n, -P ) < 0.0f ? -n : n;
}

float F4FX_SsgiIgn( float2 p, int f )
{
	p += 5.588238f * float( f & 63 );
	return frac( 52.9829189f * frac( dot( p, float2( 0.06711056f, 0.00583715f ) ) ) );
}

uint F4FX_SsgiSectorBits( float a0, float a1 )
{
	int b0 = int( floor( clamp( a0, 0.0f, 1.0f ) * 32.0f ) );
	int b1 = int( ceil( clamp( a1, 0.0f, 1.0f ) * 32.0f ) );
	int n = b1 - b0;
	if ( n <= 0 )
		return 0u;
	uint m = n >= 32 ? 0xFFFFFFFFu : ( ( 1u << uint( n ) ) - 1u );
	return m << uint( b0 );
}

// rgb = bounce irradiance (x pi), a = the share the probe keeps
float4 F4FX_SsgiCast( int2 wp, F4FX_SsgiParams P_ )
{
	const float PI = F4FX_SSGI_PI;
	float2 uv = ( float2( wp ) + 0.5f ) / float2( P_.workSize );
	int2 ft = int2( uv * float2( P_.fullSize ) );
	float4 g = SSGI_GBUF( ft );
	if ( g.w <= 0.0f )
		return float4( 0.0f, 0.0f, 0.0f, 1.0f );
	float3 P = F4FX_SsgiViewPos( ( float2( ft ) + 0.5f ) / float2( P_.fullSize ), g.w, P_.proj );
	float3 V = normalize( -P );
	float3 N = F4FX_SsgiFaceN( normalize( g.xyz ), P );
	float rPx = P_.radius * P_.proj.y * 0.5f * float( P_.fullSize.y ) / g.w;
	if ( rPx < 1.0f )
		return float4( 0.0f, 0.0f, 0.0f, 1.0f );
	float n0 = F4FX_SsgiIgn( float2( wp ), P_.frameIndex ), n1 = F4FX_SsgiIgn( float2( wp ) + float2( 37.0f, 11.0f ), P_.frameIndex + 17 );
	if ( P_.blueNoise ) {
		float2 bn = SSGI_NOISE( int2( wp.x & 127, ( P_.frameIndex & 63 ) * 128 + ( wp.y & 127 ) ) );
		n0 = bn.x;
		n1 = bn.y;
	}
	float3 il = float3( 0.0f, 0.0f, 0.0f );
	float vis = 0.0f, wsum = 0.0f;
	for ( int s = 0; s < P_.slices; s++ ) {
		float phi = ( float( s ) + n0 ) * PI / float( P_.slices );
		float2 dir = float2( cos( phi ), sin( phi ) );
		float3 d3 = float3( dir, 0.0f );
		float3 ortho = normalize( d3 - V * dot( d3, V ) );
		float3 axis = normalize( cross( ortho, V ) );
		float3 Np = N - axis * dot( N, axis );
		float npLen = length( Np );
		if ( npLen < 1.0e-4f )
			continue;
		float n = float( sign( dot( Np, ortho ) ) ) * F4FX_SsgiAcos( clamp( dot( Np, V ) / npLen, -1.0f, 1.0f ) );
		uint occ = 0u;
		float3 sil = float3( 0.0f, 0.0f, 0.0f );
		float taken = 0.0f;
		float sliceStep = frac( n1 + float( s ) * 0.618034f );
		for ( int side = 0; side < 2; side++ ) {
			float sgn = side == 0 ? 1.0f : -1.0f;
			if ( P_.fullBreak && occ == 0xFFFFFFFFu )
				break;
			for ( int k = 0; k < P_.steps; k++ ) {
				float t = ( float( k ) + ( P_.blueNoise ? sliceStep : frac( n1 + float( k ) * 0.618034f ) ) ) / float( P_.steps );
				float off = max( t * t * rPx, 1.0f + float( k ) );
				float2 sp = float2( ft ) + 0.5f + sgn * dir * off;
				if ( sp.x < 0.0f || sp.y < 0.0f || sp.x >= float( P_.fullSize.x ) || sp.y >= float( P_.fullSize.y ) )
					break;
				if ( P_.fullBreak && occ == 0xFFFFFFFFu )
					break;
				float4 gs = SSGI_GBUF( int2( sp ) );
				if ( gs.w <= 0.0f )
					continue;
				float3 S = F4FX_SsgiViewPos( sp / float2( P_.fullSize ), gs.w, P_.proj );
				float3 dF = S - P;
				float len = length( dF );
				if ( len > P_.radius || len < 1.0e-3f )
					continue;
				float3 dB = dF - V * P_.thickness;
				float aF = sgn * F4FX_SsgiAcos( clamp( dot( dF / len, V ), -1.0f, 1.0f ) );
				float aB = sgn * F4FX_SsgiAcos( clamp( dot( normalize( dB ), V ), -1.0f, 1.0f ) );
				float uF = 0.5f + 0.5f * sin( clamp( aF - n, -0.5f * PI, 0.5f * PI ) );
				float uB = 0.5f + 0.5f * sin( clamp( aB - n, -0.5f * PI, 0.5f * PI ) );
				uint bits = F4FX_SsgiSectorBits( min( uF, uB ), max( uF, uB ) );
				uint fresh = bits & ~occ;
				if ( fresh != 0u ) {
					float rt = clamp( ( P_.radius - len ) / max( 0.25f * P_.radius, 1.0f ), 0.0f, 1.0f );
					float share = float( countbits( fresh ) ) / 32.0f * rt * rt * ( 3.0f - 2.0f * rt );
					float3 Ns = normalize( gs.xyz );
					float facing = clamp( dot( Ns, -dF / len ), 0.0f, 1.0f );
					float lod = clamp( log2( off ) - 3.3f, 0.0f, 4.0f );
					float3 L = SSGI_SCENE( sp / float2( P_.fullSize ), lod );
					float front = clamp( facing * 10.0f, 0.0f, 1.0f );
					sil += share * front * max( L, float3( 0.0f, 0.0f, 0.0f ) );
					taken += share * front;
					occ |= bits;
				}
			}
		}
		il += sil * npLen;
		vis += ( 1.0f - taken ) * npLen;
		wsum += npLen;
	}
	if ( wsum <= 0.0f )
		return float4( 0.0f, 0.0f, 0.0f, 1.0f );
	il = il / wsum * PI * P_.sourceBrightness;
	float y = dot( il, float3( 0.2126f, 0.7152f, 0.0722f ) );
	il = max( lerp( float3( y, y, y ), il, P_.saturation ), float3( 0.0f, 0.0f, 0.0f ) );
	return float4( il, vis / wsum );
}

#endif
