// HLSL source of features/exposure/cell_is_measure_q.frag (fragment shader), lane HLSLSRC2 (notes/hlslsrc2/tools/glsl2hlsl.py): one readable file for every
// variant, the GLSL's comments, names and order kept. Loose uniforms are the LooseUniforms cbuffer (b31).
// variants: #if none (the loader's defines; res/shaders/hlsl/variants.txt lists the sets built)
// profile: ps_6_0
#include "ExposureBindingsVk.hlsli"
#define WW_HLSL_SRC 1	// the shared .hlsli files give their readable half
#ifndef WW_GLSL_SIZE_Texture2DMS_float4_
#define WW_GLSL_SIZE_Texture2DMS_float4_
int2 textureSize(Texture2DMS<float4> t) { uint w, h, n; t.GetDimensions(w, h, n); return int2(w, h); }
#endif
static float4 gl_FragCoord;
/* lane EMPTY240 R11-1: the measure drawn straight at the quarter size (src/features/exposure/cellhdr.cpp isMeasureFrame).
 * Each quarter texel reads its 4 x 4 frame pixels: a pixel counts where the frame's stencil (sample 0, as the old stencil
 * blit kept) reads 1 in its low two bits (a cell program or a cell effect wrote it last: linear light), its samples
 * averaged (a non-finite one read as 0). rgb = the sum over the counted pixels / 16, a = their count / 16: the same
 * texel the full-size draw's mip level 2 held, with no full-size draw, stencil copy or mip build. */

Texture2DMS<float4> hdrSrc : register(t0, space0);
Texture2DMS<uint4> hdrStencil : register(t1, space0);
cbuffer LooseUniforms : register(b31, space0)
{
	int hdrSamples;
};


static float4 fragColor;

float finiteOr0( float v )
{
	return or( isnan( v ), isinf( v ) ) ? 0.0 : v;
}

void frag_main()
{
	int2 size = textureSize( hdrSrc );
	int2 base = int2( gl_FragCoord.xy ) * 4;
	float3 sum = (float3)( 0.0 );
	float cover = 0.0;
	for ( int y = 0; y < 4; y++ ) {
		for ( int x = 0; x < 4; x++ ) {
			int2 p = base + int2( x, y );
			if ( or( p.x >= size.x, p.y >= size.y ) )
				continue;
			if ( ( hdrStencil.Load( p, 0 ).r & 3u ) != 1u )
				continue;
			float3 px = (float3)( 0.0 );
			for ( int i = 0; i < hdrSamples; i++ ) {
				float3 v = hdrSrc.Load( p, i ).rgb;
				px += float3( finiteOr0( v.r ), finiteOr0( v.g ), finiteOr0( v.b ) );
			}
			sum += px / float( max( hdrSamples, 1 ) );
			cover += 1.0;
		}
	}
	fragColor = float4( sum / 16.0, cover / 16.0 );
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
