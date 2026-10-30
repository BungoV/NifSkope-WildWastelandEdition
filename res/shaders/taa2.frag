#version 410 core

/* lane TAA2: our modern temporal AA resolve ("Modern temporal AA", src/gl/gametaa.h), beside the game's copy
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

uniform sampler2D curTex;	// this frame, NEAREST
uniform sampler2D histTex;	// the history, LINEAR (RGBA16F)
uniform sampler2D mvTex;	// previous minus current, texture units (game_taa_mv.frag)
uniform sampler2D depthTex;
uniform sampler2D lockTex;	// lane TAA3: last frame's o2, NEAREST (FARTOGGLE3: RGBA16F, b a the isolated range)
uniform sampler2D velTex;	// lane TAA3: TAAINPUTS1 velocity (the mvTex convention, with things that move on their own)
uniform sampler2D maskTex;	// lane TAA3: TAAINPUTS1 mask: r reactive, g composition, b own motion
uniform vec2 invSize;
uniform vec2 jitterPx;	// how far this frame's content moved (pixels, x right, y up): pixel p saw the scene at p - jitterPx
uniform int taa2Tier;
uniform int taa2Red;	// bits: 1 noclip, 2 nobicubic, 4 wrongjitter (the filter told the opposite jitter), 8 nolock (both locks),
						// 32 oldmotion, 64 oldfilter (lane TAA4: the TAA2 motion weight / current filter width),
						// 128 oldlock (lane FARTOGGLE3: no recent-range lock, the resolve before it)
uniform int taa2NoHist;	// 1: no history this frame (first frame, reset, size change)
uniform int taa2Inputs;	// lane TAA3: bits 1 velTex bound, 2 maskTex bound
uniform mat4 reproj;	// current unjittered clip -> previous unjittered clip

layout ( location = 0 ) out vec4 o0;
layout ( location = 1 ) out vec4 o1;
layout ( location = 2 ) out vec4 o2;

#include "ww_dither.glsl"

vec3 toYCoCg( vec3 c )
{
	return vec3( dot( c, vec3( 0.25, 0.5, 0.25 ) ), dot( c, vec3( 0.5, 0.0, -0.5 ) ), dot( c, vec3( -0.25, 0.5, -0.25 ) ) );
}

vec3 toRgb( vec3 y )
{
	return vec3( y.x + y.y - y.z, y.x + y.z, y.x - y.y - y.z );
}

// Jimenez 2016: Catmull-Rom from 5 bilinear taps (the four corner taps dropped, weights renormalised)
vec3 histCatmullRom( vec2 uv )
{
	vec2 size = 1.0 / invSize;
	vec2 pos = uv * size;
	vec2 tc = floor( pos - 0.5 ) + 0.5;
	vec2 f = pos - tc;
	vec2 w0 = f * ( -0.5 + f * ( 1.0 - 0.5 * f ) );
	vec2 w1 = 1.0 + f * f * ( -2.5 + 1.5 * f );
	vec2 w2 = f * ( 0.5 + f * ( 2.0 - 1.5 * f ) );
	vec2 w3 = f * f * ( -0.5 + 0.5 * f );
	vec2 w12 = w1 + w2;
	vec2 t0 = ( tc - 1.0 ) * invSize;
	vec2 t3 = ( tc + 2.0 ) * invSize;
	vec2 t12 = ( tc + w2 / w12 ) * invSize;
	float a = w12.x * w0.y, b = w0.x * w12.y, c = w12.x * w12.y, d = w3.x * w12.y, e = w12.x * w3.y;
	vec3 r = texture( histTex, vec2( t12.x, t0.y ) ).rgb * a + texture( histTex, vec2( t0.x, t12.y ) ).rgb * b
		+ texture( histTex, t12 ).rgb * c + texture( histTex, vec2( t3.x, t12.y ) ).rgb * d
		+ texture( histTex, vec2( t12.x, t3.y ) ).rgb * e;
	return max( r / ( a + b + c + d + e ), vec3( 0.0 ) );
}

// the history pulled along the line toward c (inside the box) until it is inside the box
vec3 clipToward( vec3 h, vec3 c, vec3 mn, vec3 mx )
{
	vec3 dir = h - c;
	vec3 lim = mix( mn, mx, step( vec3( 0.0 ), dir ) ) - c;
	vec3 t = vec3( 1.0 );
	for ( int i = 0; i < 3; i++ )
		if ( abs( dir[i] ) > 1e-7 )
			t[i] = clamp( lim[i] / dir[i], 0.0, 1.0 );
	return c + dir * min( t.x, min( t.y, t.z ) );
}

void main()
{
	ivec2 size = textureSize( curTex, 0 );
	ivec2 ip = ivec2( gl_FragCoord.xy );
	vec2 uv = gl_FragCoord.xy * invSize;
	vec2 jit = ( taa2Red & 4 ) != 0 ? -jitterPx : jitterPx;

	// the 3x3: its colour moments, min/max, the filtered current value, the closest depth
	vec3 m1 = vec3( 0.0 ), m2 = vec3( 0.0 ), mn = vec3( 1e9 ), mx = vec3( -1e9 ), sumC = vec3( 0.0 ), cC = vec3( 0.0 );
	float sumW = 0.0, dMin = 2.0;
	ivec2 dTap = ip;
	for ( int dy = -1; dy <= 1; dy++ ) {
		for ( int dx = -1; dx <= 1; dx++ ) {
			ivec2 q = clamp( ip + ivec2( dx, dy ), ivec2( 0 ), size - 1 );
			vec3 c = toYCoCg( texelFetch( curTex, q, 0 ).rgb );
			float d = texelFetch( depthTex, q, 0 ).r;
			if ( d < dMin ) {
				dMin = d;
				dTap = q;
			}
			m1 += c;
			m2 += c * c;
			mn = min( mn, c );
			mx = max( mx, c );
			vec2 o = vec2( dx, dy ) - jit;	// where that pixel's sample lies, from this pixel's centre
			// lane TAA4: 4.0, not the Blackman-Harris fit 2.29: the wider filter kept the picture 10-21% behind the
			// frame (live lag vs the game's), the narrower one stays under the game's still shimmer
			float w = exp( -( ( taa2Red & 64 ) != 0 ? 2.29 : 4.0 ) * dot( o, o ) );
			sumC += c * w;
			sumW += w;
			if ( dx == 0 && dy == 0 )
				cC = c;
		}
	}
	vec3 cur = taa2Tier >= 2 ? sumC / sumW : cC;

	// lane TAA3: TAAINPUTS1's vectors know what moved on its own; its mask what the history cannot hold
	vec2 mv = ( taa2Inputs & 1 ) != 0 ? texelFetch( velTex, dTap, 0 ).xy : texelFetch( mvTex, dTap, 0 ).xy;
	vec3 msk = vec3( 0.0 );
	if ( ( taa2Inputs & 2 ) != 0 )
		msk = vec3( texelFetch( maskTex, ip, 0 ).rg, texelFetch( maskTex, dTap, 0 ).b );
	vec2 huv = uv + mv;
	float motionPx = length( mv * vec2( size ) );
	bool offscreen = any( lessThan( huv, vec2( 0.0 ) ) ) || any( greaterThan( huv, vec2( 1.0 ) ) );
	vec3 res = cur;
	vec2 lockOut = vec2( 0.0 );
	/* lane FARTOGGLE3: a far object under a pixel in size is caught by the jitter in some frames only. In the frames
	 * it misses, the 3x3 holds no trace of it, so the clip threw its history away and the object blinked on a still
	 * camera (offline twin tests/taafar_twin: 75 of 228 seen objects at Medium). Lock b, a carry how far this pixel's
	 * luma recently stood above / below the mean of its 8 neighbours (an isolated peak or dip: a sub-pixel thing, never
	 * the inside or the edge of a broad mover), each fading by kRangeRelax a frame, so a dip the jitter brings back
	 * within its 8-frame cycle is still there on the frames it misses. A history inside [cur - below, cur + above] is
	 * kept (no clip, no depth disocclusion, and a longer average); a mover's trail never raised that range, so the
	 * clip still takes it (the twin's ghost bar). taa2Red 128 is the resolve before this. */
	vec4 lk4 = vec4( 0.0 );
	float m8 = ( m1.x - cC.x ) / 8.0;
	vec2 rng = vec2( max( cC.x - m8, 0.0 ), max( m8 - cC.x, 0.0 ) );
	if ( taa2NoHist == 0 && !offscreen ) {
		lk4 = texelFetch( lockTex, clamp( ivec2( huv * vec2( size ) ), ivec2( 0 ), size - 1 ), 0 );
		const float kRangeRelax = 0.08;	// twin: 0.04 .. 0.15 all pass the ghost bar; 0.25+ lets more objects go
		rng = max( rng, lk4.ba * ( 1.0 - kRangeRelax ) );
	}
	if ( taa2NoHist == 0 && !offscreen ) {
		bool bicubic = taa2Tier >= 2 && ( taa2Red & 2 ) == 0;
		vec3 h = toYCoCg( bicubic ? histCatmullRom( huv ) : texture( histTex, huv ).rgb );
		vec3 h0 = h;
		vec3 bmn = mn, bmx = mx;
		{
			// Low's looser box (1.25) keeps its shimmer under the game's without the current filter
			vec3 mu = m1 / 9.0;
			vec3 sigma = sqrt( max( m2 / 9.0 - mu * mu, vec3( 0.0 ) ) );
			float gamma = taa2Tier >= 4 ? mix( 1.0, 0.75, clamp( motionPx / 8.0, 0.0, 1.0 ) ) : ( taa2Tier == 1 ? 1.25 : 1.0 );
			bmn = max( mn, mu - gamma * sigma );
			bmx = min( mx, mu + gamma * sigma );
		}
		/* lane TAA3: a sub-pixel wire the jitter hits every other frame empties the 3x3 on the misses, and the clip
		 * threw its history away (detail 0.17 vs the game's 0.29). Where the current keeps flipping sides of the
		 * history (aliasing, not change) the history is kept; a real change flips once, so the clip still takes it. */
		vec2 lk = lk4.rg;
		float dY = cur.x - h0.x;
		float sgn = abs( dY ) > 0.02 ? sign( dY ) : lk.g;
		float flipRate = mix( lk.r, sgn * lk.g < 0.0 ? 1.0 : 0.0, 0.2 );
		lockOut = vec2( flipRate, sgn );
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
		vec3 hc = h;
		if ( ( taa2Red & 1 ) == 0 )
			hc = clipToward( h, clamp( cur, bmn, bmx ), bmn, bmx );
		h = mix( hc, h0, keep );
		// a history the box had to pull far is likely stale (an unvectored mover): trust it less, so trails die fast
		float rej = clamp( length( h0 - hc ) / max( length( bmx - bmn ), 1e-3 ), 0.0, 1.0 ) * ( 1.0 - keep );
		float rest = taa2Tier >= 2 ? 0.93 : 0.9;
		// lane TAA4: moving, the history weight falls to 0.4 by 2 px a frame (was 0.8 by 32 px: the lag over the game's)
		bool oldMotion = ( taa2Red & 32 ) != 0;
		float alpha = mix( rest, oldMotion ? 0.8 : 0.4, clamp( motionPx / ( oldMotion ? 32.0 : 2.0 ), 0.0, 1.0 ) ) * ( 1.0 - rej );
		// lane FARTOGGLE3: a kept sub-pixel thing averages over more than one 8-frame jitter cycle, else the history
		// saws up at each hit and down between (0.07 a frame) and the object still pulses
		const float kLockRest = 0.96;
		alpha = mix( alpha, max( alpha, kLockRest ), keepR );
		alpha *= 1.0 - msk.r;	// reactive: particles and effects follow this frame
		alpha = min( alpha, mix( alpha, 0.5, msk.g ) );	// water, glass: their depth is the thing behind them
		if ( taa2Tier >= 3 ) {
			// disoccluded: the history there was clearly nearer than where this pixel's surface was last frame
			// (only for the camera's own vectors: a thing that moved on its own was not where reproj puts it)
			vec4 p = reproj * vec4( ( vec2( dTap ) + 0.5 ) * invSize * 2.0 - 1.0, dMin * 2.0 - 1.0, 1.0 );
			float expect = 1.0 - ( p.z / p.w * 0.5 + 0.5 );
			float was = texelFetch( histTex, clamp( ivec2( huv * vec2( size ) ), ivec2( 0 ), size - 1 ), 0 ).a;
			// lane FARTOGGLE3: not on a kept history: the jitter moves a sub-pixel object in and out of the 3x3's
			// closest depth, which read as a disocclusion every frame it missed (High/Ultra blinked with no clip at all)
			if ( msk.b < 0.5 && was > expect * 1.05 + 1e-4 && ( ( taa2Red & 128 ) != 0 || keep < 0.5 ) )
				alpha = 0.0;
			// Karis 2014: each side weighted by 1 / (1 + luma), so a lone bright sample cannot flicker the pixel
			float wc = ( 1.0 - alpha ) / ( 1.0 + cur.x ), wh = alpha / ( 1.0 + h.x );
			res = ( cur * wc + h * wh ) / max( wc + wh, 1e-6 );
		} else {
			res = mix( cur, h, alpha );
		}
	}
	vec3 rgb = max( toRgb( res ), vec3( 0.0 ) );
	o0 = vec4( rgb, 1.0 - min( dMin, 1.0 ) );
	o1 = vec4( wwDither8( clamp( rgb, 0.0, 1.0 ), gl_FragCoord.xy ), 1.0 );	// lane FOGGLOW1: into the 8-bit picture, dithered
	o2 = vec4( lockOut, rng );	// lane FARTOGGLE3: b, a the recent range (was 0, 1; the lock is RGBA16F now)
}
