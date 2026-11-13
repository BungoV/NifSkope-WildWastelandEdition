// HLSL source of features/post/game_taa.frag (fragment shader), lane HLSLSRC3 (notes/hlslsrc3/tools/glsl2hlsl.py): one readable file for every
// variant, the GLSL's comments, names and order kept. Loose uniforms are the LooseUniforms cbuffer (b31).
// variants: #if none (the loader's defines; res/shaders/hlsl/variants.txt lists the sets built)
// profile: ps_6_0
#include "PostBindingsVk.hlsli"
#define WW_HLSL_SRC 1	// the shared .hlsli files give their readable half
// inlined (no readable .hlsli yet): ww_dither.glsl
static float4 gl_FragCoord;
/* lane MOTION1: the game's temporal AA resolve (src/features/post/gametaa.h), operation for operation in the game's order,
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

Texture2D<float4> curTex : register(t0, space0); SamplerState _curTex_sampler : register(s0, space0);
Texture2D<float4> histTex : register(t1, space0); SamplerState _histTex_sampler : register(s1, space0);
Texture2D<float4> mvTex : register(t2, space0); SamplerState _mvTex_sampler : register(s2, space0);
Texture2D<float4> depthTex : register(t3, space0); SamplerState _depthTex_sampler : register(s3, space0);
cbuffer LooseUniforms : register(b31, space0)
{
	float4 c0;
	float4 c2;
	float4 c3;
	float4 c4;
	float4 c5;
	int taaRed;	// 1: the red control "noclamp", the history luma used unclamped
	int taaTap;	// 1 (the gate's dump only): o0 = the history exactly as the filtering hardware returned it
	int wwDitherOn;
	int wwDitherSeed;
};


static float4 o0;
static float4 o1;

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

// the luma the history holds: .5 G + .25 R + .25 B
float luma( float3 c )
{
	return dot( float3( c.g, c.b, c.r ), float3( 0.5, 0.25, 0.25 ) );
}

void frag_main()
{
	float2 uv = gl_FragCoord.xy * c0.xy;
	float2 o = c3.xy;

	// the 3x3 taps, named by their offset in units of c3
	float2 tA = uv - o;                      // (-1, -1)
	float2 tB = uv + o;                      // (+1, +1)
	float2 tC = uv + o * float2( 1.0, 0.0 );   // (+1,  0)
	float2 tD = uv + o * float2( 1.0, -1.0 );  // (+1, -1)
	float2 tE = uv + o * float2( -1.0, 1.0 );  // (-1, +1)
	float2 tF = uv + o * float2( 0.0, -1.0 );  // ( 0, -1)
	float2 tG = uv + o * float2( 0.0, 1.0 );   // ( 0, +1)
	float2 tH = uv + o * float2( -1.0, 0.0 );  // (-1,  0)

	// the closest depth of the nine, its tap chosen in the game's order (later equal taps win)
	float dA = depthTex.Sample( _depthTex_sampler, tA ).x;
	float dB = depthTex.Sample( _depthTex_sampler, tB ).x;
	float dD = depthTex.Sample( _depthTex_sampler, tD ).x;
	float m = min( min( dB, dD ), dA );
	float2 sel = ( m == dA ) ? tA : tB;
	sel = ( m == dD ) ? tD : sel;
	float dF = depthTex.Sample( _depthTex_sampler, tF ).x;
	m = min( m, dF );
	float dC = depthTex.Sample( _depthTex_sampler, tC ).x;
	m = min( m, dC );
	sel = ( m == dC ) ? tC : sel;
	sel = ( m == dF ) ? tF : sel;
	float dH = depthTex.Sample( _depthTex_sampler, tH ).x;
	m = min( m, dH );
	float dE = depthTex.Sample( _depthTex_sampler, tE ).x;
	m = min( m, dE );
	sel = ( m == dE ) ? tE : sel;
	sel = ( m == dH ) ? tH : sel;
	float dZ = depthTex.Sample( _depthTex_sampler, uv ).x;
	m = min( m, dZ );
	float dG = depthTex.Sample( _depthTex_sampler, tG ).x;
	m = min( m, dG );
	sel = ( m == dG ) ? tG : sel;
	sel = ( m == dZ ) ? uv : sel;

	// that tap's motion vector; the history where this pixel was
	float2 mv = mvTex.Sample( _mvTex_sampler, sel ).xy;
	float2 mvs = mv * c5.zw;
	float mvLen = sqrt( dot( mv, mv ) );
	float2 huv = uv * c5.zw + mvs * c0.zw;
	float4 h = histTex.Sample( _histTex_sampler, huv );
	float Yh = h.x;
	float vh = h.z;
	if ( taaTap == 1 ) {
		o0 = h;
		o1 = (float4)( 0.0 );
		return;
	}

	// the nine colours and their lumas
	float3 cA = curTex.Sample( _curTex_sampler, tA ).rgb, cB = curTex.Sample( _curTex_sampler, tB ).rgb, cC = curTex.Sample( _curTex_sampler, tC ).rgb;
	float3 cD = curTex.Sample( _curTex_sampler, tD ).rgb, cE = curTex.Sample( _curTex_sampler, tE ).rgb, cF = curTex.Sample( _curTex_sampler, tF ).rgb;
	float3 cG = curTex.Sample( _curTex_sampler, tG ).rgb, cH = curTex.Sample( _curTex_sampler, tH ).rgb, cZ = curTex.Sample( _curTex_sampler, uv ).rgb;
	float YA = luma( cA ), YB = luma( cB ), YC = luma( cC ), YD = luma( cD ), YE = luma( cE );
	float YF = luma( cF ), YG = luma( cG ), YH = luma( cH ), YZ = luma( cZ );

	// the 2x2 tent resample (the frame without its jitter): centre, (0,+1), (-1,0), (-1,+1)
	float3 R = cH * c2.z;
	R = cE * c2.w + R;
	R = cG * c2.y + R;
	R = cZ * c2.x + R;

	/* THE BRACKET (the neighbourhood clamp): over the centre then G, H, E, F, C, D, A, B,
	 *   upper = the darkest with luma >= the history's (the centre only when it is not darker; none: 1.001)
	 *   lower = the brightest with luma < the history's (none: -0.001)
	 * each kept as (R, B, Y); G comes back from Y. */
	float3 up = ( YZ < 1.001 ) ? float3( cZ.r, cZ.b, YZ ) : (float3)( 1.001 );
	up = ( YZ < Yh ) ? (float3)( 1.001 ) : up;
	float3 lo = ( -0.001 < YZ ) ? float3( cZ.r, cZ.b, YZ ) : (float3)( -0.001 );
	lo = ( YZ < Yh ) ? lo : (float3)( -0.001 );
#define WW_BRACKET( c, Y ) \
	up = ( Y < Yh ) ? up : ( ( Y < up.z ) ? float3( c.r, c.b, Y ) : up ); \
	lo = ( Y < Yh ) ? ( ( lo.z < Y ) ? float3( c.r, c.b, Y ) : lo ) : lo;
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
	float4 L = float4( lo.z, lo.x, ( lo.z - lo.x * 0.25 - lo.y * 0.25 ) * 2.0, lo.y );
	float4 U = float4( up.z, up.x, ( up.z - up.x * 0.25 - up.y * 0.25 ) * 2.0, up.y );
	bool noUp = 1.0 < U.x;
	bool noLo = L.x < 0.0;
	L = noLo ? U : L;
	U = noUp ? L : U;
	float YhC = min( U.x, max( L.x, Yh ) );
	if ( taaRed == 1 )
		YhC = Yh;
	float4 span = U - L;

	// off the previous frame: no history
	bool offscreen = or( any( (huv >= c0.zw) ), 0.0 >= min( huv.x, huv.y ) );
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
	o0 = float4( clamp( Yn, 0.0, 1.0 ), 0.0, vlen, 0.0 );

	// the history's colour: along the bracket at the history's luma (the middle when the bracket is flat)
	float tt = ( 0.01 < span.x ) ? t : 0.5;
	float3 hist = tt * span.yzw + L.yzw;
	hist = offscreen ? cZ : hist;
	float3 Rr = offscreen ? cZ : R;
	float3 cur = k * ( cZ - Rr ) + Rr;
	float3 col = clamp( w * ( hist - cur ) + cur, 0.0, 1.0 );
	col = clamp( ( col - Rr ) * c4.z + col, 0.0, 1.0 );
	// lane FOGGLOW1: the picture goes into an 8-bit target: dithered (the tent above averages the tone map's dither away)
	o1 = float4( wwDither8( clamp( c4.w * ( Rr - col ) + col, 0.0, 1.0 ), gl_FragCoord.xy ), 1.0 );
}

struct SPIRV_Cross_Input
{
    float4 gl_FragCoord : SV_Position;
};

struct SPIRV_Cross_Output
{
    float4 o0 : SV_Target0;
    float4 o1 : SV_Target1;
};

SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    gl_FragCoord = stage_input.gl_FragCoord;
    gl_FragCoord.w = 1.0 / gl_FragCoord.w;
    frag_main();
    SPIRV_Cross_Output stage_output;
    stage_output.o0 = o0;
    stage_output.o1 = o1;
    return stage_output;
}
