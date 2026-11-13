// HLSL source of features/post/game_dofmblur.frag (fragment shader), lane HLSLSRC3 (notes/hlslsrc3/tools/glsl2hlsl.py): one readable file for every
// variant, the GLSL's comments, names and order kept. Loose uniforms are the LooseUniforms cbuffer (b31).
// variants: #if none (the loader's defines; res/shaders/hlsl/variants.txt lists the sets built)
// profile: ps_6_0
#include "PostBindingsVk.hlsli"
#define WW_HLSL_SRC 1	// the shared .hlsli files give their readable half
#ifndef WW_GLSL_SIZE_Texture2D_float4_
#define WW_GLSL_SIZE_Texture2D_float4_
int2 textureSize(Texture2D<float4> t, int lod) { uint w, h, l; t.GetDimensions(uint(lod), w, h, l); return int2(w, h); }
#endif
static float4 gl_FragCoord;
/* lane POSTFX2: the game's depth of field composite (game_dof.frag) and its motion blur (game_mblur.frag) in ONE
 * full-screen pass, for frames where both run (src/features/post/cellpost.cpp). The two-pass chain writes the composite to a
 * half-float target, copies it back and reads it with linear filtering at the blur's taps; here every tap takes
 * the composite of the four texels the filter would read and blends them with the filter's weights, so no target,
 * copy or second pass is needed. Red WW_CELL_POSTFUSE_RED=1 keeps the two passes.
 * t0 the frame, t1 the motion vectors, t2 the depth of field's blur, t3 the depth; dc0..dc3 = game_dof's c0..c3,
 * mc0, mc1 = game_mblur's c0, c1. */

Texture2D<float4> t0 : register(t1, space0); SamplerState _t0_sampler : register(s1, space0);
Texture2D<float4> t1 : register(t2, space0); SamplerState _t1_sampler : register(s2, space0);
Texture2D<float4> t2 : register(t3, space0); SamplerState _t2_sampler : register(s3, space0);
Texture2D<float4> t3 : register(t4, space0); SamplerState _t3_sampler : register(s4, space0);
#define WW_LOOSE_TYPES 1
#if defined(WW_DOF_LENS)
#if defined(WW_DOF_LENS)
#ifndef LENS_FRAME
#define LENS_FRAME t0	// as below, before this file
#endif
#endif
#include "game_doflens.hlsli"
#endif
#undef WW_LOOSE_TYPES
cbuffer LooseUniforms : register(b31, space0)
{
#define WW_LOOSE_UNIFORMS 1
#if defined(WW_DOF_LENS)
#if defined(WW_DOF_LENS)
#ifndef LENS_FRAME
#define LENS_FRAME t0	// as below, before this file
#endif
#endif
#include "game_doflens.hlsli"
#endif
#undef WW_LOOSE_UNIFORMS
	float4 dc0;
	float4 dc1;
	float4 dc2;
	float4 dc3;
	float4 mc0;
	float4 mc1;
	int farOnly;
	int dofRed;
};


static float4 fragColor;

float sat( float v )	// D3D _sat: NaN -> 0
{
	return v >= 0.0 ? min( v, 1.0 ) : 0.0;
}

float2 V( float2 d )	// game texture space -> this view's
{
	return float2( d.x, 1.0 - d.y );
}

// lane DOFLENS1: the thin-lens composite (twin game_doflensmblur.frag; t2 = the far layer)
#ifdef WW_DOF_LENS
#define LENS_FRAME t0
#define _LENS_FRAME_sampler _t0_sampler
#define LENS_FAR t2
#define _LENS_FAR_sampler _t2_sampler
#define LENS_DEPTH t3
#define _LENS_DEPTH_sampler _t3_sampler
#include "game_doflens.hlsli"
#endif

// game_dof.frag's composite at one texel
float4 dofTexel( int2 p, float2 size )
{
#ifdef WW_DOF_LENS
	return lensTexel( p );
#endif
	float2 v1 = ( float2( p ) + 0.5 ) / size;
	float x = 1.0 - t3.Load( int3(p, 0) ).r;
	float y = x * dc2.z + dc2.y;
	bool notSky = 0.00001 < x;
	y = dc2.w / y;
	float k;
	if ( farOnly != 0 ) {
		k = sat( ( y - dc0.z ) / dc0.y );
		k = k * dc1.x;
		notSky = or( notSky, !( 0.0 != dc1.w ) );
	} else {
		float nearT = ( dc0.z - y ) / dc0.x;
		bool nearOn = ( y < dc0.z ) && ( 0.0 != dc1.y );
		nearT = nearOn ? nearT : 0.0;
		float farT = y - dc0.z;
		bool farOn = ( dc0.z < y ) && ( 0.0 != dc1.z );
		notSky = or( notSky, !( 0.0 != dc1.w ) );
		farT = farT / dc0.y;
		k = sat( farOn ? farT : nearT );
		k = k * dc1.x;
	}
	if ( dofRed != 0 )
		k = dc1.x;
	k = notSky ? k : 0.0;
	float4 b = t2.Sample( _t2_sampler, v1 * dc3.zw );
	float4 a = t0.Load( int3(p, 0) );
	return k * ( b - a ) + a;
}

// the composite read as a linear filter reads its target at uv (clamped to the edge)
float3 dofAt( float2 uv, float2 size )
{
	float2 x = uv * size - 0.5;
	float2 i0 = floor( x );
	float2 f = x - i0;
	int2 lo = int2( i0 );
	int2 hi = min( lo + 1, int2( size ) - 1 );
	lo = max( lo, (int2)( 0 ) );
	hi = max( hi, (int2)( 0 ) );
	float3 r = dofTexel( lo, size ).rgb * ( ( 1.0 - f.x ) * ( 1.0 - f.y ) );
	if ( f.x > 0.0 )
		r += dofTexel( int2( hi.x, lo.y ), size ).rgb * ( f.x * ( 1.0 - f.y ) );
	if ( f.y > 0.0 ) {
		r += dofTexel( int2( lo.x, hi.y ), size ).rgb * ( ( 1.0 - f.x ) * f.y );
		if ( f.x > 0.0 )
			r += dofTexel( hi, size ).rgb * ( f.x * f.y );
	}
	return r;
}

float2 mvAt( float2 d )
{
	float2 m = t1.Sample( _t1_sampler, V( d ) ).xy;
	return float2( m.x, -m.y );
}

float tapW( float2 m, float L )
{
	float l = sqrt( dot( m, m ) );
	return sat( -abs( L - l ) * mc0.z + 1.0 );
}

void frag_main()
{
#ifdef WW_DOF_LENS
	lensS1 = lensFocus( LENS_DEPTH, _LENS_DEPTH_sampler );
#endif
	float2 size = float2( textureSize( t0, 0 ) );
	float2 g = gl_FragCoord.xy / size;
	float2 v1 = V( g );
	float2 m = mvAt( v1 );
	float l2 = dot( m, m );
	float L = sqrt( l2 );
	float2 dc = v1 - mc1.zw;
	float rr = sqrt( dot( dc, dc ) );
	rr = sat( rr * 0.1 + -0.018 );
	rr = L * rr;
	if ( rr < 0.001 ) {
		fragColor = dofTexel( int2( gl_FragCoord.xy ), size );
		return;
	}
	float2 dir = rsqrt( l2 ) * m;
	float len = min( rr, mc0.y );
	dir = len * dir;
	dir = dir * mc0.x;

	float2 p0 = min( v1, mc1.xy );
	float w0 = tapW( mvAt( p0 ), L );
	float3 col = dofAt( V( p0 ), size );
	float4 p12 = min( dir.xyxy * float4( 0.25, 0.25, 0.5, 0.5 ) + v1.xyxy, mc1.xyxy );
	float w1 = tapW( mvAt( p12.xy ), L );
	float ws = w0 + w1;
	float3 c1t = w1 * dofAt( V( p12.xy ), size );
	col = col * w0 + c1t;
	float w2 = tapW( mvAt( p12.zw ), L );
	ws = w2 + ws;
	col = dofAt( V( p12.zw ), size ) * w2 + col;
	float2 p3 = min( dir * 0.75 + v1, mc1.xy );
	float w3 = tapW( mvAt( p3 ), L );
	ws = w3 + ws;
	col = dofAt( V( p3 ), size ) * w3 + col;
	ws = ws + 0.001;
	fragColor = float4( col / ws, 1.0 );
}

struct SPIRV_Cross_Input
{
    float4 gl_FragCoord : SV_Position;
};

struct SPIRV_Cross_Output
{
    float4 fragColor : SV_Target0;
};

SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    gl_FragCoord = stage_input.gl_FragCoord;
    gl_FragCoord.w = 1.0 / gl_FragCoord.w;
    frag_main();
    SPIRV_Cross_Output stage_output;
    stage_output.fragColor = fragColor;
    return stage_output;
}
