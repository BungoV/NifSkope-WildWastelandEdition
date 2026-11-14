// HLSL source of features/post/cell_objblur_tile.frag (fragment shader), lane HLSLSRC2 (notes/hlslsrc2/tools/glsl2hlsl.py): one readable file for every
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
/* lane POSTFX1: per-object motion blur, passes 2 and 3 of 4 (McGuire et al. 2012, "A Reconstruction Filter for
 * Plausible Motion Blur": tile max, then neighbour max). tileMode 0: the longest vector of the tile x tile pixels
 * under this tile; tileMode 1: the longest of the 3 x 3 tiles around this one. srcTex holds pixel vectors (RG16F). */

Texture2D<float4> srcTex : register(t0, space0); SamplerState _srcTex_sampler : register(s0, space0);
cbuffer LooseUniforms : register(b31, space0)
{
	int tile;
	int tileMode;
};


static float2 vmax;

void frag_main()
{
	int2 t = int2( gl_FragCoord.xy );
	int2 sz = textureSize( srcTex, 0 );
	float2 best = (float2)( 0.0 );
	float bl = 0.0;
	if ( tileMode == 0 ) {
		int2 o = t * tile;
		for ( int y = 0; y < tile; y++ )
			for ( int x = 0; x < tile; x++ ) {
				int2 q = o + int2( x, y );
				if ( or( q.x >= sz.x, q.y >= sz.y ) )
					continue;
				float2 v = srcTex.Load( int3(q, 0) ).xy;
				float l = dot( v, v );
				if ( l > bl ) { bl = l; best = v; }
			}
	} else {
		for ( int y = -1; y <= 1; y++ )
			for ( int x = -1; x <= 1; x++ ) {
				int2 q = clamp( t + int2( x, y ), (int2)( 0 ), sz - 1 );
				float2 v = srcTex.Load( int3(q, 0) ).xy;
				float l = dot( v, v );
				if ( l > bl ) { bl = l; best = v; }
			}
	}
	vmax = best;
}

struct SPIRV_Cross_Input
{
    float4 gl_FragCoord : SV_Position;
};

struct SPIRV_Cross_Output
{
    float2 vmax : SV_Target0;
};

SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    gl_FragCoord = stage_input.gl_FragCoord;
    gl_FragCoord.w = 1.0 / gl_FragCoord.w;
    frag_main();
    SPIRV_Cross_Output stage_output;
    stage_output.vmax = vmax;
    return stage_output;
}
