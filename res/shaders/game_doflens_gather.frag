#version 410 core

/* lane DOFLENS1: the thin-lens depth of field's gather (src/gl/cellpost.cpp), quarter size, two layers.
 * t0 = the prep (rgb, a = the signed circle of confusion in full px). 32 taps on a Vogel disk of radius Rk (full px):
 * the centre's own far radius, or the largest near radius when the near layer is on.
 * Far (out 0): a tap counts where its disk covers the centre (coverage sat(rho - r + 0.5)); a tap behind the centre
 * spreads no wider than the centre's own disk, so a sharp or nearer surface is never blurred by what is behind it.
 * Near (out 1, premultiplied): each tap's colour spread over its disk (area pi rho^2) against the tap's share of the
 * kernel (pi Rk^2 / N); a = the covered share, rgb = its mean colour times a.
 * lensQ.x = the largest circle (px), .y = near on. */

uniform sampler2D t0;
uniform vec4 lensQ;

layout( location = 0 ) out vec4 farOut;
layout( location = 1 ) out vec4 nearOut;

const int N = 32;

float sat( float v )
{
	return v >= 0.0 ? min( v, 1.0 ) : 0.0;
}

void main()
{
	ivec2 q = ivec2( gl_FragCoord.xy );
	vec2 qs = vec2( textureSize( t0, 0 ) );
	vec4 c0 = texelFetch( t0, q, 0 );
	float cc = c0.a;
	float rc = max( cc, 0.0 ) * 0.5;
	float rn = lensQ.y != 0.0 ? lensQ.x * 0.5 : 0.0;
	float rk = max( rc, rn );
	if ( rk < 0.5 ) {
		farOut = vec4( c0.rgb, 1.0 );
		nearOut = vec4( 0.0 );
		return;
	}
	vec2 uc = ( vec2( q ) + 0.5 ) / qs;
	vec3 fa = vec3( 0.0 ), na = vec3( 0.0 );
	float fw = 0.0, nd = 0.0;
	float share = rk * rk / float( N );
	for ( int i = 0; i < N; i++ ) {
		float t = sqrt( ( float( i ) + 0.5 ) / float( N ) ) * rk;	// full px
		float an = float( i ) * 2.39996323;
		vec2 o = vec2( cos( an ), sin( an ) ) * t;
		vec4 cs = texture( t0, uc + o * 0.25 / qs );
		float rf = max( cs.a, 0.0 ) * 0.5;
		if ( cs.a > cc )
			rf = min( rf, rc );
		float w = sat( rf - t + 0.5 );
		fa += cs.rgb * w;
		fw += w;
		float rN = max( -cs.a, 0.0 ) * 0.5;
		float d = sat( rN - t + 0.5 ) * share / max( rN * rN, 1.0 );
		na += cs.rgb * d;
		nd += d;
	}
	farOut = vec4( fw > 0.0001 ? fa / fw : c0.rgb, 1.0 );
	float a = sat( nd );
	nearOut = nd > 0.0 ? vec4( na / nd * a, a ) : vec4( 0.0 );
}
