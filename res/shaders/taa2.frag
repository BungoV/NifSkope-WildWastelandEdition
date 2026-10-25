#version 410 core

/* lane TAA2: our modern temporal AA resolve ("Modern temporal AA", src/gl/gametaa.h), beside the game's copy
 * (game_taa.frag). Published recipe, no game code:
 *   Karis 2014 (SIGGRAPH Advances, "High Quality Temporal Supersampling"): full-colour YCoCg history, clip toward
 *     the current value, the jitter-aware Blackman-Harris current filter (its Gaussian fit exp(-2.29 d^2)),
 *     luma-weighted blend against flicker;
 *   Salvi 2016 (GDC, "An Excursion in Temporal Supersampling"): the variance clip box (mean +/- gamma sigma);
 *   Jimenez 2016 (SIGGRAPH Advances, "Filmic SMAA"): the 5-tap Catmull-Rom history fetch.
 * Tier 1 Low: centre sample, variance clip (gamma 1.25), bilinear history. 2 Medium: + filter, gamma 1, Catmull-Rom.
 * 3 High: + luma weights, depth disocclusion. 4 Ultra: + a tighter box while moving (gamma 1 -> 0.75).
 * Every tier: a history the clip moved far gets less weight (offline ghost gate, notes/taa2/sim).
 * Outputs: o0 the next history (rgb, a = 1 - the closest depth), o1 the picture. */

uniform sampler2D curTex;	// this frame, NEAREST
uniform sampler2D histTex;	// the history, LINEAR (RGBA16F)
uniform sampler2D mvTex;	// previous minus current, texture units (game_taa_mv.frag)
uniform sampler2D depthTex;
uniform vec2 invSize;
uniform vec2 jitterPx;	// how far this frame's content moved (pixels, x right, y up): pixel p saw the scene at p - jitterPx
uniform int taa2Tier;
uniform int taa2Red;	// bits: 1 noclip, 2 nobicubic, 4 wrongjitter (the filter told the opposite jitter)
uniform int taa2NoHist;	// 1: no history this frame (first frame, reset, size change)
uniform mat4 reproj;	// current unjittered clip -> previous unjittered clip

layout ( location = 0 ) out vec4 o0;
layout ( location = 1 ) out vec4 o1;

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
			float w = exp( -2.29 * dot( o, o ) );
			sumC += c * w;
			sumW += w;
			if ( dx == 0 && dy == 0 )
				cC = c;
		}
	}
	vec3 cur = taa2Tier >= 2 ? sumC / sumW : cC;

	vec2 mv = texelFetch( mvTex, dTap, 0 ).xy;
	vec2 huv = uv + mv;
	float motionPx = length( mv * vec2( size ) );
	bool offscreen = any( lessThan( huv, vec2( 0.0 ) ) ) || any( greaterThan( huv, vec2( 1.0 ) ) );
	vec3 res = cur;
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
		if ( ( taa2Red & 1 ) == 0 )
			h = clipToward( h, clamp( cur, bmn, bmx ), bmn, bmx );
		// a history the box had to pull far is likely stale (an unvectored mover): trust it less, so trails die fast
		float rej = clamp( length( h0 - h ) / max( length( bmx - bmn ), 1e-3 ), 0.0, 1.0 );
		float rest = taa2Tier >= 2 ? 0.93 : 0.9;
		float alpha = mix( rest, 0.8, clamp( motionPx / 32.0, 0.0, 1.0 ) ) * ( 1.0 - rej );
		if ( taa2Tier >= 3 ) {
			// disoccluded: the history there was clearly nearer than where this pixel's surface was last frame
			vec4 p = reproj * vec4( ( vec2( dTap ) + 0.5 ) * invSize * 2.0 - 1.0, dMin * 2.0 - 1.0, 1.0 );
			float expect = 1.0 - ( p.z / p.w * 0.5 + 0.5 );
			float was = texelFetch( histTex, clamp( ivec2( huv * vec2( size ) ), ivec2( 0 ), size - 1 ), 0 ).a;
			if ( was > expect * 1.05 + 1e-4 )
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
	o1 = vec4( clamp( rgb, 0.0, 1.0 ), 1.0 );
}
