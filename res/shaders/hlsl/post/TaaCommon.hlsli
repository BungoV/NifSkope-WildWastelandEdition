// Modern temporal AA resolve: the shared math (lane VKFRAME1 P5, 2026-11-12). No bindings and no constant block: each app's
// shim declares its resources and calls F4FX_TaaResolve (NifSkope: TaaBindingsVk.hlsli + taa2.hlsl on Vulkan; Fusion Shaders:
// its own resolve on its D3D slots). GL twin: res/shaders/features/post/taa2.frag (recipe and lane notes there); every
// expression keeps the GL twin's order and the literals carry an f (tests/vkframe/taa.sh compares the two on the GPU).
#ifndef F4FX_TAA_COMMON_HLSLI
#define F4FX_TAA_COMMON_HLSLI

#include "ToneCommon.hlsli"

// taa2.frag's uniforms
struct F4FX_TaaParams
{
	float2 invSize;
	float2 jitterPx;	// pixel p saw the scene at p - jitterPx
	int tier;			// 1 Low .. 4 Ultra
	int red;			// taa2Red bits: 1 noclip, 2 nobicubic, 4 wrongjitter, 8 nolock, 32 oldmotion, 64 oldfilter, 128 oldlock
	int noHist;
	int inputs;			// bits 1 velocity, 2 mask
	int linearIn;		// 1: the linear (HDR) frame, before the tone map
	int ditherOn;
	int ditherSeed;
	float expose;
	float4 reproj[4];	// current unjittered clip -> previous, GL's columns
};

float3 F4FX_TaaToYCoCg( float3 c )
{
	return float3( dot( c, float3( 0.25f, 0.5f, 0.25f ) ), dot( c, float3( 0.5f, 0.0f, -0.5f ) ), dot( c, float3( -0.25f, 0.5f, -0.25f ) ) );
}

float3 F4FX_TaaToRgb( float3 y )
{
	return float3( y.x + y.y - y.z, y.x + y.z, y.x - y.y - y.z );
}

// Karis 2014: c / (1 + max(c)) at about display level
float3 F4FX_TaaTonemapIn( float3 c, F4FX_TaaParams P )
{
	if ( P.linearIn == 0 )
		return c;
	c = max( c, float3( 0.0f, 0.0f, 0.0f ) ) * P.expose;
	return c / ( 1.0f + max( c.r, max( c.g, c.b ) ) );
}

float3 F4FX_TaaTonemapOut( float3 t, F4FX_TaaParams P )
{
	t = clamp( t, 0.0f, 0.999f );
	return t / ( ( 1.0f - max( t.r, max( t.g, t.b ) ) ) * max( P.expose, 1e-6f ) );
}

// Jimenez 2016: Catmull-Rom from 5 bilinear taps
float3 F4FX_TaaHistCatmullRom( Texture2D<float4> hist, SamplerState lin, float2 uv, F4FX_TaaParams P )
{
	float2 size = 1.0f / P.invSize;
	float2 pos = uv * size;
	float2 tc = floor( pos - 0.5f ) + 0.5f;
	float2 f = pos - tc;
	float2 w0 = f * ( -0.5f + f * ( 1.0f - 0.5f * f ) );
	float2 w1 = 1.0f + f * f * ( -2.5f + 1.5f * f );
	float2 w2 = f * ( 0.5f + f * ( 2.0f - 1.5f * f ) );
	float2 w3 = f * f * ( -0.5f + 0.5f * f );
	float2 w12 = w1 + w2;
	float2 t0 = ( tc - 1.0f ) * P.invSize;
	float2 t3 = ( tc + 2.0f ) * P.invSize;
	float2 t12 = ( tc + w2 / w12 ) * P.invSize;
	float a = w12.x * w0.y, b = w0.x * w12.y, c = w12.x * w12.y, d = w3.x * w12.y, e = w12.x * w3.y;
	float3 r = hist.SampleLevel( lin, float2( t12.x, t0.y ), 0.0f ).rgb * a + hist.SampleLevel( lin, float2( t0.x, t12.y ), 0.0f ).rgb * b
		+ hist.SampleLevel( lin, t12, 0.0f ).rgb * c + hist.SampleLevel( lin, float2( t3.x, t12.y ), 0.0f ).rgb * d
		+ hist.SampleLevel( lin, float2( t12.x, t3.y ), 0.0f ).rgb * e;
	return max( r / ( a + b + c + d + e ), float3( 0.0f, 0.0f, 0.0f ) );
}

// the history pulled along the line toward c until it is inside the box
float3 F4FX_TaaClipToward( float3 h, float3 c, float3 mn, float3 mx )
{
	float3 dir = h - c;
	float3 lim = lerp( mn, mx, step( float3( 0.0f, 0.0f, 0.0f ), dir ) ) - c;
	float3 t = float3( 1.0f, 1.0f, 1.0f );
	[unroll] for ( int i = 0; i < 3; i++ )
		if ( abs( dir[i] ) > 1e-7f )
			t[i] = clamp( lim[i] / dir[i], 0.0f, 1.0f );
	return c + dir * min( t.x, min( t.y, t.z ) );
}

/* One pixel ip of a size frame: o0 the next history (rgb, a = 1 - the closest depth), o1 the picture (the linear frame's
 * light, or the dithered display value), o2 the lock (r flip rate, g sign, b a the recent range). redNoLumaWeight drops
 * Karis' luma weights (the gate's red). */
void F4FX_TaaResolve( int2 ip, int2 size, F4FX_TaaParams P, Texture2D<float4> curTex, Texture2D<float4> histTex,
	Texture2D<float4> mvTex, Texture2D<float4> depthTex, Texture2D<float4> lockTex, Texture2D<float4> velTex,
	Texture2D<float4> maskTex, SamplerState lin, bool redNoLumaWeight, out float4 o0, out float4 o1, out float4 o2 )
{
	const float2 fragCoord = float2( ip ) + 0.5f;
	float2 uv = fragCoord * P.invSize;
	float2 jit = ( P.red & 4 ) != 0 ? -P.jitterPx : P.jitterPx;

	float3 m1 = float3( 0.0f, 0.0f, 0.0f ), m2 = float3( 0.0f, 0.0f, 0.0f ), mn = float3( 1e9f, 1e9f, 1e9f ), mx = float3( -1e9f, -1e9f, -1e9f );
	float3 sumC = float3( 0.0f, 0.0f, 0.0f ), cC = float3( 0.0f, 0.0f, 0.0f );
	float sumW = 0.0f, dMin = 2.0f;
	int2 dTap = ip;
	[unroll] for ( int dy = -1; dy <= 1; dy++ ) {
		[unroll] for ( int dx = -1; dx <= 1; dx++ ) {
			int2 q = clamp( ip + int2( dx, dy ), int2( 0, 0 ), size - 1 );
			float3 c = F4FX_TaaToYCoCg( F4FX_TaaTonemapIn( curTex.Load( int3( q, 0 ) ).rgb, P ) );
			float d = depthTex.Load( int3( q, 0 ) ).r;
			if ( d < dMin ) {
				dMin = d;
				dTap = q;
			}
			m1 += c;
			m2 += c * c;
			mn = min( mn, c );
			mx = max( mx, c );
			float2 o = float2( dx, dy ) - jit;
			float w = exp( -( ( P.red & 64 ) != 0 ? 2.29f : 4.0f ) * dot( o, o ) );
			sumC += c * w;
			sumW += w;
			if ( dx == 0 && dy == 0 )
				cC = c;
		}
	}
	float3 cur = P.tier >= 2 ? sumC / sumW : cC;

	float2 mv = ( P.inputs & 1 ) != 0 ? velTex.Load( int3( dTap, 0 ) ).xy : mvTex.Load( int3( dTap, 0 ) ).xy;
	float3 msk = float3( 0.0f, 0.0f, 0.0f );
	if ( ( P.inputs & 2 ) != 0 )
		msk = float3( maskTex.Load( int3( ip, 0 ) ).rg, maskTex.Load( int3( dTap, 0 ) ).b );
	float2 huv = uv + mv;
	float motionPx = length( mv * float2( size ) );
	bool offscreen = any( huv < float2( 0.0f, 0.0f ) ) || any( huv > float2( 1.0f, 1.0f ) );
	float3 res = cur;
	float2 lockOut = float2( 0.0f, 0.0f );
	float4 lk4 = float4( 0.0f, 0.0f, 0.0f, 0.0f );
	float m8 = ( m1.x - cC.x ) / 8.0f;
	float2 rng = float2( max( cC.x - m8, 0.0f ), max( m8 - cC.x, 0.0f ) );
	const int2 hp = clamp( int2( huv * float2( size ) ), int2( 0, 0 ), size - 1 );
	if ( P.noHist == 0 && !offscreen ) {
		lk4 = lockTex.Load( int3( hp, 0 ) );
		const float kRangeRelax = 0.08f;
		rng = max( rng, lk4.ba * ( 1.0f - kRangeRelax ) );
	}
	if ( P.noHist == 0 && !offscreen ) {
		bool bicubic = P.tier >= 2 && ( P.red & 2 ) == 0;
		float3 h = F4FX_TaaToYCoCg( bicubic ? F4FX_TaaHistCatmullRom( histTex, lin, huv, P ) : histTex.SampleLevel( lin, huv, 0.0f ).rgb );
		float3 h0 = h;
		float3 bmn = mn, bmx = mx;
		{
			float3 mu = m1 / 9.0f;
			float3 sigma = sqrt( max( m2 / 9.0f - mu * mu, float3( 0.0f, 0.0f, 0.0f ) ) );
			float gamma = P.tier >= 4 ? lerp( 1.0f, 0.75f, clamp( motionPx / 8.0f, 0.0f, 1.0f ) ) : ( P.tier == 1 ? 1.25f : 1.0f );
			bmn = max( mn, mu - gamma * sigma );
			bmx = min( mx, mu + gamma * sigma );
		}
		float2 lk = lk4.rg;
		float dY = cur.x - h0.x;
		float sgn = abs( dY ) > 0.02f ? float( sign( dY ) ) : lk.g;
		float flipRate = lerp( lk.r, sgn * lk.g < 0.0f ? 1.0f : 0.0f, 0.2f );
		lockOut = float2( flipRate, sgn );
		float keep = ( P.red & 8 ) == 0 ? smoothstep( 0.25f, 0.5f, flipRate ) * ( 1.0f - clamp( motionPx / 4.0f, 0.0f, 1.0f ) ) : 0.0f;
		float keepR = 0.0f;
		if ( ( P.red & ( 128 | 8 ) ) == 0 ) {
			float outside = max( h0.x - max( cur.x + rng.x, m8 ), min( cur.x - rng.y, m8 ) - h0.x );
			keepR = ( 1.0f - smoothstep( 0.0f, 6.0f / 255.0f, outside ) ) * ( 1.0f - clamp( motionPx / 2.0f, 0.0f, 1.0f ) )
				* ( 1.0f - max( msk.r, msk.b ) );
			keep = max( keep, keepR );
		}
		keep *= 1.0f - max( msk.r, msk.b );
		float3 hc = h;
		if ( ( P.red & 1 ) == 0 )
			hc = F4FX_TaaClipToward( h, clamp( cur, bmn, bmx ), bmn, bmx );
		h = lerp( hc, h0, keep );
		float rej = clamp( length( h0 - hc ) / max( length( bmx - bmn ), 1e-3f ), 0.0f, 1.0f ) * ( 1.0f - keep );
		float rest = P.tier >= 2 ? 0.93f : 0.9f;
		bool oldMotion = ( P.red & 32 ) != 0;
		float alpha = lerp( rest, oldMotion ? 0.8f : 0.4f, clamp( motionPx / ( oldMotion ? 32.0f : 2.0f ), 0.0f, 1.0f ) ) * ( 1.0f - rej );
		const float kLockRest = 0.96f;
		alpha = lerp( alpha, max( alpha, kLockRest ), keepR );
		alpha *= 1.0f - msk.r;
		alpha = min( alpha, lerp( alpha, 0.5f, msk.g ) );
		if ( P.tier >= 3 && !redNoLumaWeight ) {
			float4 v = float4( ( float2( dTap ) + 0.5f ) * P.invSize * 2.0f - 1.0f, dMin * 2.0f - 1.0f, 1.0f );
			float4 p = P.reproj[0] * v.x + P.reproj[1] * v.y + P.reproj[2] * v.z + P.reproj[3] * v.w;
			float expect = 1.0f - ( p.z / p.w * 0.5f + 0.5f );
			float was = histTex.Load( int3( hp, 0 ) ).a;
			if ( msk.b < 0.5f && was > expect * 1.05f + 1e-4f && ( ( P.red & 128 ) != 0 || keep < 0.5f ) )
				alpha = 0.0f;
			float wc = ( 1.0f - alpha ) / ( 1.0f + cur.x ), wh = alpha / ( 1.0f + h.x );
			res = ( cur * wc + h * wh ) / max( wc + wh, 1e-6f );
		} else {
			res = lerp( cur, h, alpha );
		}
	}
	float3 rgb = max( F4FX_TaaToRgb( res ), float3( 0.0f, 0.0f, 0.0f ) );
	o0 = float4( rgb, 1.0f - min( dMin, 1.0f ) );
	o1 = P.linearIn != 0 ? float4( F4FX_TaaTonemapOut( rgb, P ), 1.0f )
		: float4( F4FX_Dither8( clamp( rgb, 0.0f, 1.0f ), uint2( fragCoord ), P.ditherOn, P.ditherSeed ), 1.0f );
	o2 = float4( lockOut, rng );
}

#endif
