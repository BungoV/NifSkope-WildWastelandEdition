// HLSL source of features/post/taa2.frag (fragment shader), lane HLSLSRC2 (notes/hlslsrc2/tools/glsl2hlsl.py): one readable file for every
// variant, the GLSL's comments, names and order kept. Loose uniforms are the LooseUniforms cbuffer (b31).
// variants: #if none (the loader's defines; res/shaders/hlsl/variants.txt lists the sets built)
// profile: ps_6_0
#include "PostBindingsVk.hlsli"
#define WW_HLSL_SRC 1	// the shared .hlsli files give their readable half
// inlined (no readable .hlsli yet): ww_dither.glsl
#ifndef WW_GLSL_SIZE_Texture2D_float4_
#define WW_GLSL_SIZE_Texture2D_float4_
int2 textureSize(Texture2D<float4> t, int lod) { uint w, h, l; t.GetDimensions(uint(lod), w, h, l); return int2(w, h); }
#endif
static float4 gl_FragCoord;
/* lane TAA2: our modern temporal AA resolve ("Modern temporal AA", src/features/post/gametaa.h), beside the game's copy
 * (game_taa.frag). Published recipe, no game code:
 *   Karis 2014 (SIGGRAPH Advances, "High Quality Temporal Supersampling"): full-colour YCoCg history, clip toward
 *     the current value, the jitter-aware Blackman-Harris current filter (its Gaussian fit exp(-2.29 d^2)),
 *     luma-weighted blend against flicker, the reversible tone map c / (1 + max(c)) for an HDR input;
 *   Salvi 2016 (GDC, "An Excursion in Temporal Supersampling"): the variance clip box (mean +/- gamma sigma);
 *   Jimenez 2016 (SIGGRAPH Advances, "Filmic SMAA"): the 5-tap Catmull-Rom history fetch;
 *   AMD FSR 2 (GPUOpen docs, "thin feature locking"): a lock that keeps a sub-pixel feature's history (TAA3).
 * Tier 1 Low: centre sample, variance clip (gamma 1.25), bilinear history. 2 Medium: + filter, gamma 1, Catmull-Rom.
 * 3 High: + luma weights, depth disocclusion. 4 Ultra: + a tighter box while moving (gamma 1 -> 0.75), and
 * TAAINPUTS1's velocity / mask when present (TAA3).
 * Every tier: a history the clip moved far gets less weight (offline ghost gate, notes/taa2/sim).
 * Outputs: o0 the next history (rgb, a = 1 - the closest depth), o1 the picture, o2 the lock (r flip rate, g sign; lane FARTOGGLE3: b, a how far the pixel
 * recently stood above / below its 8 neighbours). */

Texture2D<float4> curTex : register(t0, space0); SamplerState _curTex_sampler : register(s0, space0);	// this frame, NEAREST
Texture2D<float4> histTex : register(t1, space0); SamplerState _histTex_sampler : register(s1, space0);	// the history, LINEAR (RGBA16F)
Texture2D<float4> mvTex : register(t2, space0); SamplerState _mvTex_sampler : register(s2, space0);	// previous minus current, texture units (game_taa_mv.frag)
Texture2D<float4> depthTex : register(t3, space0); SamplerState _depthTex_sampler : register(s3, space0);
Texture2D<float4> lockTex : register(t4, space0); SamplerState _lockTex_sampler : register(s4, space0);	// lane TAA3: last frame's o2, NEAREST (FARTOGGLE3: RGBA16F, b a the isolated range)
Texture2D<float4> velTex : register(t5, space0); SamplerState _velTex_sampler : register(s5, space0);	// lane TAA3: TAAINPUTS1 velocity (the mvTex convention, with things that move on their own)
Texture2D<float4> maskTex : register(t6, space0); SamplerState _maskTex_sampler : register(s6, space0);	// lane TAA3: TAAINPUTS1 mask: r reactive, g composition, b own motion
cbuffer LooseUniforms : register(b31, space0)
{
	float2 invSize;
	float2 jitterPx;	// how far this frame's content moved (pixels, x right, y up): pixel p saw the scene at p - jitterPx
	int taa2Tier;
	int taa2Red;	// bits: 1 noclip, 2 nobicubic, 4 wrongjitter (the filter told the opposite jitter), 8 nolock (both locks),
	// 32 oldmotion, 64 oldfilter (lane TAA4: the TAA2 motion weight / current filter width),
	// 128 oldlock (lane FARTOGGLE3: no recent-range lock, the resolve before it)
	int taa2NoHist;	// 1: no history this frame (first frame, reset, size change)
	int taa2Inputs;	// lane TAA3: bits 1 velTex bound, 2 maskTex bound
	int taa2Linear;	// lane POSTFX3: 1 the input is the linear (HDR) frame, before the tone map
	float taa2Expose;	// lane POSTFX3: the linear frame's scale to about display level (last frame's exposure)
	row_major float4x4 reproj;	// current unjittered clip -> previous unjittered clip
	int wwDitherOn;
	int wwDitherSeed;
};


static float4 o0;
static float4 o1;
static float4 o2;

// ww_dither.glsl inlined here: its .hlsli has no readable half yet (lane HLSLSRC1)
/* lane FOGGLOW1 (bungo 10-29, Memory Den fog banding): a display value goes into an 8-bit target (the view, the TAA
 * picture) as one of 256 levels. A smooth dark gradient (the night fog on a wall, the glow round a lamp) then steps in
 * flat plateaus a level apart: the bands and rings. A triangular (TPDF) noise of +-1 level added before the write
 * turns the steps into a fine grain whose mean is the true value (Gjol, "Banding in Games: A Noisy Rant", Playdead,
 * 2016). The noise fades to 0 at exactly 0 and 1, so black stays black and white white.
 * wwDitherOn 0 (WW_CELL_DITHER=0, the red): the value unchanged. wwDitherSeed: the TAA frame index (a camera path
 * renders frame k as k: a repeated frame is the same picture), so TAA averages a different noise every frame. */



// a uniform [0, 1) from a pixel and a seed (an integer hash: no lattice, no pattern a zoom shows)
float wwDitherHash( uint2 p, uint s )
{
	uint h = p.x * 1664525u + p.y * 1013904223u + s * 2654435769u;
	h ^= h >> 16;
	h *= 2246822519u;
	h ^= h >> 13;
	h *= 3266489917u;
	h ^= h >> 16;
	return float( h >> 8 ) * ( 1.0 / 16777216.0 );
}

float3 wwDither8( float3 c, float2 fragCoord )
{
	if ( wwDitherOn == 0 )
		return c;
	uint2 p = uint2( fragCoord );
	uint s = uint( wwDitherSeed );
	float n = wwDitherHash( p, 2u * s ) + wwDitherHash( p, 2u * s + 1u ) - 1.0;	// triangular on (-1, 1)
	float3 w = clamp( min( c, 1.0 - c ) * 255.0, 0.0, 1.0 );
	return c + n * w * ( 1.0 / 255.0 );
}
// end of ww_dither.glsl

float3 toYCoCg( float3 c )
{
	return float3( dot( c, float3( 0.25, 0.5, 0.25 ) ), dot( c, float3( 0.5, 0.0, -0.5 ) ), dot( c, float3( -0.25, 0.5, -0.25 ) ) );
}

float3 toRgb( float3 y )
{
	return float3( y.x + y.y - y.z, y.x + y.z, y.x - y.y - y.z );
}

// Karis 2014: the resolve runs on c / (1 + max(c)), so one bright sample cannot take the box or the blend over
float3 tonemapIn( float3 c )
{
	if ( taa2Linear == 0 )
		return c;
	c = max( c, (float3)( 0.0 ) ) * taa2Expose;
	return c / ( 1.0 + max( c.r, max( c.g, c.b ) ) );
}

float3 tonemapOut( float3 t )
{
	t = clamp( t, 0.0, 0.999 );
	return t / ( ( 1.0 - max( t.r, max( t.g, t.b ) ) ) * max( taa2Expose, 1e-6 ) );
}

// Jimenez 2016: Catmull-Rom from 5 bilinear taps (the four corner taps dropped, weights renormalised)
float3 histCatmullRom( float2 uv )
{
	float2 size = 1.0 / invSize;
	float2 pos = uv * size;
	float2 tc = floor( pos - 0.5 ) + 0.5;
	float2 f = pos - tc;
	float2 w0 = f * ( -0.5 + f * ( 1.0 - 0.5 * f ) );
	float2 w1 = 1.0 + f * f * ( -2.5 + 1.5 * f );
	float2 w2 = f * ( 0.5 + f * ( 2.0 - 1.5 * f ) );
	float2 w3 = f * f * ( -0.5 + 0.5 * f );
	float2 w12 = w1 + w2;
	float2 t0 = ( tc - 1.0 ) * invSize;
	float2 t3 = ( tc + 2.0 ) * invSize;
	float2 t12 = ( tc + w2 / w12 ) * invSize;
	float a = w12.x * w0.y, b = w0.x * w12.y, c = w12.x * w12.y, d = w3.x * w12.y, e = w12.x * w3.y;
	float3 r = histTex.Sample( _histTex_sampler, float2( t12.x, t0.y ) ).rgb * a + histTex.Sample( _histTex_sampler, float2( t0.x, t12.y ) ).rgb * b
		+ histTex.Sample( _histTex_sampler, t12 ).rgb * c + histTex.Sample( _histTex_sampler, float2( t3.x, t12.y ) ).rgb * d
		+ histTex.Sample( _histTex_sampler, float2( t12.x, t3.y ) ).rgb * e;
	return max( r / ( a + b + c + d + e ), (float3)( 0.0 ) );
}

// the history pulled along the line toward c (inside the box) until it is inside the box
float3 clipToward( float3 h, float3 c, float3 mn, float3 mx )
{
	float3 dir = h - c;
	float3 lim = lerp( mn, mx, step( (float3)( 0.0 ), dir ) ) - c;
	float3 t = (float3)( 1.0 );
	for ( int i = 0; i < 3; i++ )
		if ( abs( dir[i] ) > 1e-7 )
			t[i] = clamp( lim[i] / dir[i], 0.0, 1.0 );
	return c + dir * min( t.x, min( t.y, t.z ) );
}

void frag_main()
{
	int2 size = textureSize( curTex, 0 );
	int2 ip = int2( gl_FragCoord.xy );
	float2 uv = gl_FragCoord.xy * invSize;
	float2 jit = ( taa2Red & 4 ) != 0 ? -jitterPx : jitterPx;

	// the 3x3: its colour moments, min/max, the filtered current value, the closest depth
	float3 m1 = (float3)( 0.0 ), m2 = (float3)( 0.0 ), mn = (float3)( 1e9 ), mx = (float3)( -1e9 ), sumC = (float3)( 0.0 ), cC = (float3)( 0.0 );
	float sumW = 0.0, dMin = 2.0;
	int2 dTap = ip;
	for ( int dy = -1; dy <= 1; dy++ ) {
		for ( int dx = -1; dx <= 1; dx++ ) {
			int2 q = clamp( ip + int2( dx, dy ), (int2)( 0 ), size - 1 );
			float3 c = toYCoCg( tonemapIn( curTex.Load( int3(q, 0) ).rgb ) );
			float d = depthTex.Load( int3(q, 0) ).r;
			if ( d < dMin ) {
				dMin = d;
				dTap = q;
			}
			m1 += c;
			m2 += c * c;
			mn = min( mn, c );
			mx = max( mx, c );
			float2 o = float2( dx, dy ) - jit;	// where that pixel's sample lies, from this pixel's centre
			// lane TAA4: 4.0, not the Blackman-Harris fit 2.29: the wider filter kept the picture 10-21% behind the
			// frame (live lag vs the game's), the narrower one stays under the game's still shimmer
			float w = exp( -( ( taa2Red & 64 ) != 0 ? 2.29 : 4.0 ) * dot( o, o ) );
			sumC += c * w;
			sumW += w;
			if ( dx == 0 && dy == 0 )
				cC = c;
		}
	}
	float3 cur = taa2Tier >= 2 ? sumC / sumW : cC;

	// lane TAA3: TAAINPUTS1's vectors know what moved on its own; its mask what the history cannot hold
	float2 mv = ( taa2Inputs & 1 ) != 0 ? velTex.Load( int3(dTap, 0) ).xy : mvTex.Load( int3(dTap, 0) ).xy;
	float3 msk = (float3)( 0.0 );
	if ( ( taa2Inputs & 2 ) != 0 )
		msk = float3( maskTex.Load( int3(ip, 0) ).rg, maskTex.Load( int3(dTap, 0) ).b );
	float2 huv = uv + mv;
	float motionPx = length( mv * float2( size ) );
	bool offscreen = or( any( (huv < (float2)( 0.0 )) ), any( (huv > (float2)( 1.0 )) ) );
	float3 res = cur;
	float2 lockOut = (float2)( 0.0 );
	/* lane FARTOGGLE3: a far object under a pixel in size is caught by the jitter in some frames only. In the frames
	 * it misses, the 3x3 holds no trace of it, so the clip threw its history away and the object blinked on a still
	 * camera (offline twin tests/features/post/taafar_twin: 75 of 228 seen objects at Medium). Lock b, a carry how far this pixel's
	 * luma recently stood above / below the mean of its 8 neighbours (an isolated peak or dip: a sub-pixel thing, never
	 * the inside or the edge of a broad mover), each fading by kRangeRelax a frame, so a dip the jitter brings back
	 * within its 8-frame cycle is still there on the frames it misses. A history inside [cur - below, cur + above] is
	 * kept (no clip, no depth disocclusion, and a longer average); a mover's trail never raised that range, so the
	 * clip still takes it (the twin's ghost bar). taa2Red 128 is the resolve before this. */
	float4 lk4 = (float4)( 0.0 );
	float m8 = ( m1.x - cC.x ) / 8.0;
	float2 rng = float2( max( cC.x - m8, 0.0 ), max( m8 - cC.x, 0.0 ) );
	if ( taa2NoHist == 0 && !offscreen ) {
		lk4 = lockTex.Load( int3(clamp( int2( huv * float2( size ) ), (int2)( 0 ), size - 1 ), 0) );
		const float kRangeRelax = 0.08;	// twin: 0.04 .. 0.15 all pass the ghost bar; 0.25+ lets more objects go
		rng = max( rng, lk4.ba * ( 1.0 - kRangeRelax ) );
	}
	if ( taa2NoHist == 0 && !offscreen ) {
		bool bicubic = taa2Tier >= 2 && ( taa2Red & 2 ) == 0;
		float3 h = toYCoCg( bicubic ? histCatmullRom( huv ) : histTex.Sample( _histTex_sampler, huv ).rgb );
		float3 h0 = h;
		float3 bmn = mn, bmx = mx;
		{
			// Low's looser box (1.25) keeps its shimmer under the game's without the current filter
			float3 mu = m1 / 9.0;
			float3 sigma = sqrt( max( m2 / 9.0 - mu * mu, (float3)( 0.0 ) ) );
			float gamma = taa2Tier >= 4 ? lerp( 1.0, 0.75, clamp( motionPx / 8.0, 0.0, 1.0 ) ) : ( taa2Tier == 1 ? 1.25 : 1.0 );
			bmn = max( mn, mu - gamma * sigma );
			bmx = min( mx, mu + gamma * sigma );
		}
		/* lane TAA3: a sub-pixel wire the jitter hits every other frame empties the 3x3 on the misses, and the clip
		 * threw its history away (detail 0.17 vs the game's 0.29). Where the current keeps flipping sides of the
		 * history (aliasing, not change) the history is kept; a real change flips once, so the clip still takes it. */
		float2 lk = lk4.rg;
		float dY = cur.x - h0.x;
		float sgn = abs( dY ) > 0.02 ? sign( dY ) : lk.g;
		float flipRate = lerp( lk.r, sgn * lk.g < 0.0 ? 1.0 : 0.0, 0.2 );
		lockOut = float2( flipRate, sgn );
		float keep = ( taa2Red & 8 ) == 0 ? smoothstep( 0.25, 0.5, flipRate ) * ( 1.0 - clamp( motionPx / 4.0, 0.0, 1.0 ) ) : 0.0;
		float keepR = 0.0;
		if ( ( taa2Red & ( 128 | 8 ) ) == 0 ) {	// lane FARTOGGLE3: a history inside the recent range is kept (above)
			// (the 8-neighbour mean is inside too: on a frame the jitter hits it, the history lies between the hit and
			// the ground around it, and the depth test below must not drop it there either)
			float outside = max( h0.x - max( cur.x + rng.x, m8 ), min( cur.x - rng.y, m8 ) - h0.x );
			// (fading out over 6 8-bit steps past it: 2 left a few 0.3 px objects going at Low / High / Ultra)
			// (gone by 2 px a frame of camera motion: by 4 the chart's 0.5 px pan read 0.0117 -> 0.0136 at Low)
			keepR = ( 1.0 - smoothstep( 0.0, 6.0 / 255.0, outside ) ) * ( 1.0 - clamp( motionPx / 2.0, 0.0, 1.0 ) )
				* ( 1.0 - max( msk.r, msk.b ) );
			keep = max( keep, keepR );
		}
		keep *= 1.0 - max( msk.r, msk.b );	// an effect or a thing moving on its own is no thin wire
		float3 hc = h;
		if ( ( taa2Red & 1 ) == 0 )
			hc = clipToward( h, clamp( cur, bmn, bmx ), bmn, bmx );
		h = lerp( hc, h0, keep );
		// a history the box had to pull far is likely stale (an unvectored mover): trust it less, so trails die fast
		float rej = clamp( length( h0 - hc ) / max( length( bmx - bmn ), 1e-3 ), 0.0, 1.0 ) * ( 1.0 - keep );
		float rest = taa2Tier >= 2 ? 0.93 : 0.9;
		// lane TAA4: moving, the history weight falls to 0.4 by 2 px a frame (was 0.8 by 32 px: the lag over the game's)
		bool oldMotion = ( taa2Red & 32 ) != 0;
		float alpha = lerp( rest, oldMotion ? 0.8 : 0.4, clamp( motionPx / ( oldMotion ? 32.0 : 2.0 ), 0.0, 1.0 ) ) * ( 1.0 - rej );
		// lane FARTOGGLE3: a kept sub-pixel thing averages over more than one 8-frame jitter cycle, else the history
		// saws up at each hit and down between (0.07 a frame) and the object still pulses
		const float kLockRest = 0.96;
		alpha = lerp( alpha, max( alpha, kLockRest ), keepR );
		alpha *= 1.0 - msk.r;	// reactive: particles and effects follow this frame
		alpha = min( alpha, lerp( alpha, 0.5, msk.g ) );	// water, glass: their depth is the thing behind them
		if ( taa2Tier >= 3 ) {
			// disoccluded: the history there was clearly nearer than where this pixel's surface was last frame
			// (only for the camera's own vectors: a thing that moved on its own was not where reproj puts it)
			float4 p = mul(float4( ( float2( dTap ) + 0.5 ) * invSize * 2.0 - 1.0, dMin * 2.0 - 1.0, 1.0 ), reproj);
			float expect = 1.0 - ( p.z / p.w * 0.5 + 0.5 );
			float was = histTex.Load( int3(clamp( int2( huv * float2( size ) ), (int2)( 0 ), size - 1 ), 0) ).a;
			// lane FARTOGGLE3: not on a kept history: the jitter moves a sub-pixel object in and out of the 3x3's
			// closest depth, which read as a disocclusion every frame it missed (High/Ultra blinked with no clip at all)
			if ( msk.b < 0.5 && was > expect * 1.05 + 1e-4 && ( or( ( taa2Red & 128 ) != 0, keep < 0.5 ) ) )
				alpha = 0.0;
			// Karis 2014: each side weighted by 1 / (1 + luma), so a lone bright sample cannot flicker the pixel
			float wc = ( 1.0 - alpha ) / ( 1.0 + cur.x ), wh = alpha / ( 1.0 + h.x );
			res = ( cur * wc + h * wh ) / max( wc + wh, 1e-6 );
		} else {
			res = lerp( cur, h, alpha );
		}
	}
	float3 rgb = max( toRgb( res ), (float3)( 0.0 ) );
	o0 = float4( rgb, 1.0 - min( dMin, 1.0 ) );
	// lane FOGGLOW1: into the 8-bit picture, dithered; lane POSTFX3: the linear frame's picture back to light (half floats)
	o1 = taa2Linear != 0 ? float4( tonemapOut( rgb ), 1.0 ) : float4( wwDither8( clamp( rgb, 0.0, 1.0 ), gl_FragCoord.xy ), 1.0 );
	o2 = float4( lockOut, rng );	// lane FARTOGGLE3: b, a the recent range (was 0, 1; the lock is RGBA16F now)
}

struct SPIRV_Cross_Input
{
    float4 gl_FragCoord : SV_Position;
};

struct SPIRV_Cross_Output
{
    float4 o0 : SV_Target0;
    float4 o1 : SV_Target1;
    float4 o2 : SV_Target2;
};

SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    gl_FragCoord = stage_input.gl_FragCoord;
    gl_FragCoord.w = 1.0 / gl_FragCoord.w;
    frag_main();
    SPIRV_Cross_Output stage_output;
    stage_output.o0 = o0;
    stage_output.o1 = o1;
    stage_output.o2 = o2;
    return stage_output;
}
