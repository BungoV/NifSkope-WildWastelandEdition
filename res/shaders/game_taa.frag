#version 410 core

/* lane MOTION1: the game's temporal AA resolve (src/gl/gametaa.h), operation for operation in the game's order,
 * because its choices break ties by that order (the closest depth, the bracketing neighbours).
 *
 * Inputs as the game binds them: curTex the display frame (NEAREST), histTex the history (BILERP: luma in .x,
 * the motion length in .z), mvTex the motion vectors (NEAREST), depthTex the depth (NEAREST), every one
 * CLAMPed. Constants as the game sets them (gametaa.cpp builds them the same way):
 *   c0 = (1/W, 1/H, the dynamic-resolution fraction x, y)   c2 = the 2x2 tent's four weights
 *   c3 = (s_x/W, s_y/H): one texel toward the jitter's sign  c4 = (LowFreq, HighFreq, PostSharpen, PostOverlay)
 *   c5 = (previous 1/W, 1/H, previous / current resolution fraction x, y)
 * The one change: this texture space has v UP, the game's v down, so c3.y arrives already turned over.
 * Outputs: o0 the next history (luma, 0, motion length, 0), o1 the picture. */

uniform sampler2D curTex;
uniform sampler2D histTex;
uniform sampler2D mvTex;
uniform sampler2D depthTex;
uniform vec4 c0;
uniform vec4 c2;
uniform vec4 c3;
uniform vec4 c4;
uniform vec4 c5;
uniform int taaRed;	// 1: the red control "noclamp", the history luma used unclamped
uniform int taaTap;	// 1 (the gate's dump only): o0 = the history exactly as the filtering hardware returned it

layout ( location = 0 ) out vec4 o0;
layout ( location = 1 ) out vec4 o1;

// the luma the history holds: .5 G + .25 R + .25 B
float luma( vec3 c )
{
	return dot( vec3( c.g, c.b, c.r ), vec3( 0.5, 0.25, 0.25 ) );
}

void main()
{
	vec2 uv = gl_FragCoord.xy * c0.xy;
	vec2 o = c3.xy;

	// the 3x3 taps, named by their offset in units of c3
	vec2 tA = uv - o;                      // (-1, -1)
	vec2 tB = uv + o;                      // (+1, +1)
	vec2 tC = uv + o * vec2( 1.0, 0.0 );   // (+1,  0)
	vec2 tD = uv + o * vec2( 1.0, -1.0 );  // (+1, -1)
	vec2 tE = uv + o * vec2( -1.0, 1.0 );  // (-1, +1)
	vec2 tF = uv + o * vec2( 0.0, -1.0 );  // ( 0, -1)
	vec2 tG = uv + o * vec2( 0.0, 1.0 );   // ( 0, +1)
	vec2 tH = uv + o * vec2( -1.0, 0.0 );  // (-1,  0)

	// the closest depth of the nine, its tap chosen in the game's order (later equal taps win)
	float dA = texture( depthTex, tA ).x;
	float dB = texture( depthTex, tB ).x;
	float dD = texture( depthTex, tD ).x;
	float m = min( min( dB, dD ), dA );
	vec2 sel = ( m == dA ) ? tA : tB;
	sel = ( m == dD ) ? tD : sel;
	float dF = texture( depthTex, tF ).x;
	m = min( m, dF );
	float dC = texture( depthTex, tC ).x;
	m = min( m, dC );
	sel = ( m == dC ) ? tC : sel;
	sel = ( m == dF ) ? tF : sel;
	float dH = texture( depthTex, tH ).x;
	m = min( m, dH );
	float dE = texture( depthTex, tE ).x;
	m = min( m, dE );
	sel = ( m == dE ) ? tE : sel;
	sel = ( m == dH ) ? tH : sel;
	float dZ = texture( depthTex, uv ).x;
	m = min( m, dZ );
	float dG = texture( depthTex, tG ).x;
	m = min( m, dG );
	sel = ( m == dG ) ? tG : sel;
	sel = ( m == dZ ) ? uv : sel;

	// that tap's motion vector; the history where this pixel was
	vec2 mv = texture( mvTex, sel ).xy;
	vec2 mvs = mv * c5.zw;
	float mvLen = sqrt( dot( mv, mv ) );
	vec2 huv = uv * c5.zw + mvs * c0.zw;
	vec4 h = texture( histTex, huv );
	float Yh = h.x;
	float vh = h.z;
	if ( taaTap == 1 ) {
		o0 = h;
		o1 = vec4( 0.0 );
		return;
	}

	// the nine colours and their lumas
	vec3 cA = texture( curTex, tA ).rgb, cB = texture( curTex, tB ).rgb, cC = texture( curTex, tC ).rgb;
	vec3 cD = texture( curTex, tD ).rgb, cE = texture( curTex, tE ).rgb, cF = texture( curTex, tF ).rgb;
	vec3 cG = texture( curTex, tG ).rgb, cH = texture( curTex, tH ).rgb, cZ = texture( curTex, uv ).rgb;
	float YA = luma( cA ), YB = luma( cB ), YC = luma( cC ), YD = luma( cD ), YE = luma( cE );
	float YF = luma( cF ), YG = luma( cG ), YH = luma( cH ), YZ = luma( cZ );

	// the 2x2 tent resample (the frame without its jitter): centre, (0,+1), (-1,0), (-1,+1)
	vec3 R = cH * c2.z;
	R = cE * c2.w + R;
	R = cG * c2.y + R;
	R = cZ * c2.x + R;

	/* THE BRACKET (the neighbourhood clamp): over the centre then G, H, E, F, C, D, A, B,
	 *   upper = the darkest with luma >= the history's (the centre only when it is not darker; none: 1.001)
	 *   lower = the brightest with luma < the history's (none: -0.001)
	 * each kept as (R, B, Y); G comes back from Y. */
	vec3 up = ( YZ < 1.001 ) ? vec3( cZ.r, cZ.b, YZ ) : vec3( 1.001 );
	up = ( YZ < Yh ) ? vec3( 1.001 ) : up;
	vec3 lo = ( -0.001 < YZ ) ? vec3( cZ.r, cZ.b, YZ ) : vec3( -0.001 );
	lo = ( YZ < Yh ) ? lo : vec3( -0.001 );
#define WW_BRACKET( c, Y ) \
	up = ( Y < Yh ) ? up : ( ( Y < up.z ) ? vec3( c.r, c.b, Y ) : up ); \
	lo = ( Y < Yh ) ? ( ( lo.z < Y ) ? vec3( c.r, c.b, Y ) : lo ) : lo;
	WW_BRACKET( cG, YG )
	WW_BRACKET( cH, YH )
	WW_BRACKET( cE, YE )
	WW_BRACKET( cF, YF )
	WW_BRACKET( cC, YC )
	WW_BRACKET( cD, YD )
	WW_BRACKET( cA, YA )
	WW_BRACKET( cB, YB )
#undef WW_BRACKET
	// (Y, R, G, B) of each
	vec4 L = vec4( lo.z, lo.x, ( lo.z - lo.x * 0.25 - lo.y * 0.25 ) * 2.0, lo.y );
	vec4 U = vec4( up.z, up.x, ( up.z - up.x * 0.25 - up.y * 0.25 ) * 2.0, up.y );
	bool noUp = 1.0 < U.x;
	bool noLo = L.x < 0.0;
	L = noLo ? U : L;
	U = noUp ? L : U;
	float YhC = min( U.x, max( L.x, Yh ) );
	if ( taaRed == 1 )
		YhC = Yh;
	vec4 span = U - L;

	// off the previous frame: no history
	bool offscreen = any( greaterThanEqual( huv, c0.zw ) ) || 0.0 >= min( huv.x, huv.y );
	float YhP = offscreen ? YZ : YhC;
	float vhP = offscreen ? 0.0 : vh;
	float t = ( YhC - L.x ) / span.x;
	float dY = YhP - YZ;

	// the motion: 128 pixels is all of it; the weight falls from HighFreq at rest to LowFreq
	float vlen = clamp( mvLen / ( c0.x * 128.0 ), 0.0, 1.0 );
	float wf = vlen * ( c4.x - c4.y ) + c4.y;
	float k = max( 1.0 - abs( vlen - vhP ) * 20.0, 0.0 );
	float w = min( k, wf );

	// the next history: the luma, unless the change is under 0.01
	float Yn = ( abs( dY * w ) < 0.01 ) ? YZ : ( w * dY + YZ );
	o0 = vec4( clamp( Yn, 0.0, 1.0 ), 0.0, vlen, 0.0 );

	// the history's colour: along the bracket at the history's luma (the middle when the bracket is flat)
	float tt = ( 0.01 < span.x ) ? t : 0.5;
	vec3 hist = tt * span.yzw + L.yzw;
	hist = offscreen ? cZ : hist;
	vec3 Rr = offscreen ? cZ : R;
	vec3 cur = k * ( cZ - Rr ) + Rr;
	vec3 col = clamp( w * ( hist - cur ) + cur, 0.0, 1.0 );
	col = clamp( ( col - Rr ) * c4.z + col, 0.0, 1.0 );
	o1 = vec4( clamp( c4.w * ( Rr - col ) + col, 0.0, 1.0 ), 1.0 );
}
