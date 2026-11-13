// HLSL source of features/post/game_doflens_tile.frag (fragment shader), lane HLSLSRC2 (notes/hlslsrc2/tools/glsl2hlsl.py): one readable file for every
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
/* lane DOFLENS1: the near layer's reach (src/features/post/cellpost.cpp), two passes over tiles of 8 x 8 quarter texels (32 full px).
 * dilate 0: t0 = the prep (a = the signed circle in full px); out = the largest near radius (full px) in the tile.
 * dilate 1: t0 = that; out = the largest of the 3 x 3 tiles around. The near circle is at most 64 px (radius 32 =
 * 8 quarter texels), so every near disk that can reach a texel starts in its tile or a neighbour: the gather's near
 * kernel takes this radius instead of the fixed 32 px. */

Texture2D<float4> t0 : register(t0, space0); SamplerState _t0_sampler : register(s0, space0);
cbuffer LooseUniforms : register(b31, space0)
{
	int dilate;
};


static float4 fragColor;

void frag_main()
{
	int2 q = int2( gl_FragCoord.xy );
	int2 sz = textureSize( t0, 0 );
	float m = 0.0;
	if ( dilate == 0 ) {
		for ( int j = 0; j < 8; j++ ) {
			for ( int i = 0; i < 8; i++ ) {
				int2 p = min( q * 8 + int2( i, j ), sz - 1 );
				m = max( m, -t0.Load( int3(p, 0) ).a * 0.5 );
			}
		}
	} else {
		for ( int j = -1; j <= 1; j++ ) {
			for ( int i = -1; i <= 1; i++ )
				m = max( m, t0.Load( int3(clamp( q + int2( i, j ), (int2)( 0 ), sz - 1 ), 0) ).r );
		}
	}
	fragColor = float4( m, 0.0, 0.0, 1.0 );
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
