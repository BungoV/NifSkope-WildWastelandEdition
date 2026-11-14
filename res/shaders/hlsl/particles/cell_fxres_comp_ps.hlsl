// HLSL source of features/particles/cell_fxres_comp.frag (fragment shader), lane HLSLSRC2 (notes/hlslsrc2/tools/glsl2hlsl.py): one readable file for every
// variant, the GLSL's comments, names and order kept. Loose uniforms are the LooseUniforms cbuffer (b31).
// variants: #if none (the loader's defines; res/shaders/hlsl/variants.txt lists the sets built)
// profile: ps_6_0
#include "ParticlesBindingsVk.hlsli"
#define WW_HLSL_SRC 1	// the shared .hlsli files give their readable half
static float4 gl_FragCoord;
/* lane FXRES1 (src/features/particles/cellfxres.h): the soft-effect layer over the full-resolution frame. The layer holds the sorted
 * soft draws' sum as an offset C and a transmittance T (every draw is dst = src x A + dst x B, B a function of the
 * draw's alpha only), so the frame takes frame x T + C, exactly what the draws in place would have made at the
 * layer's resolution. Upsampled depth-aware (Jansen + Bavoil 2011, Cantlay 2007): where the four layer texels around
 * the pixel lie at the pixel's own depth, bilinear; at an edge, the one texel nearest the pixel's depth. Written by
 * dual-source blending: frame = outC + frame x outT. Where no draw landed, nothing is written (the stencil keeps
 * who drew last, as the tone map reads it). */

Texture2D<float4> fxrC : register(t0, space0); SamplerState _fxrC_sampler : register(s0, space0);		// the offset, linear light (RGBA16F)
Texture2D<float4> fxrT : register(t1, space0); SamplerState _fxrT_sampler : register(s1, space0);		// the transmittance: r colour, a alpha (RGBA16F)
Texture2D<float4> fxrCov : register(t2, space0); SamplerState _fxrCov_sampler : register(s2, space0);	// 1 where a draw landed (R8)
Texture2D<float4> fxrZ : register(t3, space0); SamplerState _fxrZ_sampler : register(s3, space0);		// the layer's depth (cell_fxres_down)
Texture2D<float4> fxrDepth : register(t4, space0); SamplerState _fxrDepth_sampler : register(s4, space0);	// the frame's opaque depth, full resolution
cbuffer LooseUniforms : register(b31, space0)
{
	int fxrScale;
	int2 fxrSize;		// the layer's texels
	float2 fxrProj;		// the projection's [2][2] and [3][2]
	float fxrEdge;		// the relative view-depth step that counts as an edge
	int fxrMode;		// 0 depth-aware; 1 plain bilinear (information only)
};


static float4 outC;
static float4 outT;

float viewW( float d )
{
	return fxrProj.y / ( fxrProj.x + ( 2.0 * d - 1.0 ) );
}

void frag_main()
{
	float2 lc = gl_FragCoord.xy / float( fxrScale ) - 0.5;
	int2 b = int2( floor( lc ) );
	float2 f = lc - float2( b );
	int2 t[4] = { b, b + int2( 1, 0 ), b + int2( 0, 1 ), b + int2( 1, 1 ) };
	float wb[4] = { ( 1.0 - f.x ) * ( 1.0 - f.y ), f.x * ( 1.0 - f.y ), ( 1.0 - f.x ) * f.y, f.x * f.y };
	float w = viewW( fxrDepth.Load( int3(int2( gl_FragCoord.xy ), 0) ).r );
	bool edge = false;
	int best = 0;
	float bestD = 1e30;
	for ( int i = 0; i < 4; i++ ) {
		t[i] = clamp( t[i], (int2)( 0 ), fxrSize - 1 );
		float dz = abs( viewW( fxrZ.Load( int3(t[i], 0) ).r ) - w );
		if ( dz > fxrEdge * w )
			edge = true;
		if ( dz < bestD ) {
			bestD = dz;
			best = i;
		}
	}
	float4 c = (float4)( 0.0 ), tr = (float4)( 0.0 );
	float cov = 0.0;
	if ( or( fxrMode == 1, !edge ) ) {
		for ( int i = 0; i < 4; i++ ) {
			c += fxrC.Load( int3(t[i], 0) ) * wb[i];
			tr += fxrT.Load( int3(t[i], 0) ) * wb[i];
			cov += fxrCov.Load( int3(t[i], 0) ).r * wb[i];
		}
	} else {
		c = fxrC.Load( int3(t[best], 0) );
		tr = fxrT.Load( int3(t[best], 0) );
		cov = fxrCov.Load( int3(t[best], 0) ).r;
	}
	if ( cov <= 0.0 )
		discard;
	outC = c;
	outT = float4( tr.rrr, tr.a );
}

struct SPIRV_Cross_Input
{
    float4 gl_FragCoord : SV_Position;
};

struct SPIRV_Cross_Output
{
    float4 outC : SV_Target0;
    float4 outT : SV_Target1;
};

SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    gl_FragCoord = stage_input.gl_FragCoord;
    gl_FragCoord.w = 1.0 / gl_FragCoord.w;
    frag_main();
    SPIRV_Cross_Output stage_output;
    stage_output.outC = outC;
    stage_output.outT = outT;
    return stage_output;
}
