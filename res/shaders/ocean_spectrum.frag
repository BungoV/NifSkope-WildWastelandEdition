#version 410 core

// lane WATERLOOK1 (src/gl/cellocean.cpp): the ocean spectrum at this frame's clock (Tessendorf 2001, "Simulating Ocean
// Water", eq. 43): h( k, t ) = h0( k ) e^( i w t ) + conj( h0( -k ) ) e^( -i w t ), w quantised to 2 pi q / T so the sea
// loops exactly every T. One texel per mode; six 128 x 128 tiles side by side (sea 0-2, fresh water 3-5). Out: the
// spectra of eight real fields packed two to a complex number (the inverse FFT of a Hermitian pair is real):
//   o0 = ( sx + i sy, Dxx + i Dyy ), o1 = ( Dxy + i h, Dx + i Dy ); s the slope dh/dx, D the horizontal displacement
//   ( D = i k / |k| h: the crests pull together, as Gerstner's ), Dab = d Da / db.

uniform sampler2D oceanH0;	// RGBA32F: h0( k ), conj( h0( -k ) )
uniform sampler2D oceanQ;	// R32F: q = floor( w T / 2 pi )
uniform float oceanPhase;	// ( clock mod T ) / T
uniform float oceanDk[6];	// 2 pi / L of each tile, rad/m

layout ( location = 0 ) out vec4 o0;
layout ( location = 1 ) out vec4 o1;

vec2 cmul( vec2 a, vec2 b )
{
	return vec2( a.x * b.x - a.y * b.y, a.x * b.y + a.y * b.x );
}

void main()
{
	ivec2 p = ivec2( gl_FragCoord.xy );
	int tile = p.x / 128;
	ivec2 m = ivec2( p.x - tile * 128, p.y );
	vec2 n = vec2( m.x < 64 ? m.x : m.x - 128, m.y < 64 ? m.y : m.y - 128 );
	vec2 k = n * oceanDk[tile];
	float kl = length( k );
	vec4 h0 = texelFetch( oceanH0, p, 0 );
	float q = texelFetch( oceanQ, p, 0 ).r;
	float ph = 6.2831853 * fract( q * oceanPhase );
	vec2 e = vec2( cos( ph ), sin( ph ) );
	vec2 h = cmul( h0.xy, e ) + cmul( h0.zw, vec2( e.x, -e.y ) );
	vec2 ih = vec2( -h.y, h.x );	// i h
	float ki = kl > 0.0 ? 1.0 / kl : 0.0;
	vec2 sx = k.x * ih, sy = k.y * ih;
	vec2 dx = k.x * ki * ih, dy = k.y * ki * ih;
	vec2 dxx = -k.x * k.x * ki * h, dyy = -k.y * k.y * ki * h, dxy = -k.x * k.y * ki * h;
	// a + i b for complex a, b: ( a.x - b.y, a.y + b.x )
	o0 = vec4( sx.x - sy.y, sx.y + sy.x, dxx.x - dyy.y, dxx.y + dyy.x );
	o1 = vec4( dxy.x - h.y, dxy.y + h.x, dx.x - dy.y, dx.y + dy.x );
}
