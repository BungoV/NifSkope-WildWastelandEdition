// HLSL source of features/shadows/ww_contact_farcomp.frag (fragment shader), lane HLSLSRC2 (notes/hlslsrc2/tools/glsl2hlsl.py): one readable file for every
// variant, the GLSL's comments, names and order kept. Loose uniforms are the LooseUniforms cbuffer (b31).
// variants: #if none (the loader's defines; res/shaders/hlsl/variants.txt lists the sets built)
// profile: ps_6_0
#include "ShadowsBindingsVk.hlsli"
#define WW_HLSL_SRC 1	// the shared .hlsli files give their readable half
static float4 gl_FragCoord;
// lane GAPLOOK1 (C7): the far tier's resolve (FO4CS FarComposeCS.hlsl). The coarse mask comes back to the mask's
// resolution through a depth-aware bilinear upsample (a coarse texel counts only if its view depth is within
// `tolerance` x this pixel's own); the blend (GL_MIN) keeps the more occluded of the two tiers. Where no coarse
// texel agrees the output is 1: the near mask is left as it was.

Texture2D<float4> gbuf : register(t0, space0); SamplerState _gbuf_sampler : register(s0, space0);		// the full-resolution opaque pass (.a linear depth)
Texture2D<float4> farDepth : register(t1, space0); SamplerState _farDepth_sampler : register(s1, space0);	// the coarse min depth (.a)
Texture2D<float4> farMask : register(t2, space0); SamplerState _farMask_sampler : register(s2, space0);	// the coarse march
cbuffer LooseUniforms : register(b31, space0)
{
	int2 fullSize;
	int2 coarseSize;
	float maskScale;
	int divisor;
	float tolerance;
	float4 farFade;		// lane SHADOWBAND1: x the cascades' reach D (game units), y 1 = the tier gives way to them, zw tan(fov/2) x, y
};


static float fragMask;

void frag_main()
{
	fragMask = 1.0;
	float2 full = gl_FragCoord.xy * maskScale;
	int2 fq = clamp( int2( floor( full ) ), (int2)( 0 ), fullSize - 1 );
	float fine = gbuf.Load( int3(fq, 0) ).a;
	if ( or( fine >= 9.0e5, fine <= 0.0 ) )
		return;
	float2 cp = full / float( divisor ) - 0.5;
	int2 base = int2( floor( cp ) );
	float2 fr = cp - float2( base );
	float wsum = 0.0, acc = 0.0;
	for ( int j = 0; j < 2; j++ ) {
		for ( int i = 0; i < 2; i++ ) {
			int2 c = base + int2( i, j );
			if ( or( any( (c < (int2)( 0 )) ), any( (c >= coarseSize) ) ) )
				continue;
			float bw = ( i == 0 ? 1.0 - fr.x : fr.x ) * ( j == 0 ? 1.0 - fr.y : fr.y );
			float cd = farDepth.Load( int3(c, 0) ).a;
			if ( or( bw <= 0.0, cd >= 9.0e5 ) )
				continue;
			float w = bw * clamp( 1.0 - abs( fine - cd ) / fine / tolerance, 0.0, 1.0 );
			if ( w <= 0.0 )
				continue;
			wsum += w;
			acc += w * farMask.Load( int3(c, 0) ).r;
		}
	}
	if ( wsum > 1e-4 )
		fragMask = acc / wsum;
	// lane SHADOWBAND1: inside the cascades' reach the cascade map holds the far casters (to the texel); the tier's
	// 250-unit slab there turns a thin caster in front (a cable) into a long false band. It counts by 1 - the cascades'
	// own fade weight, (r / D)^8 at the radial distance r: the cascades' complement, today's tier past D
	if ( farFade.y > 0.5 ) {
		float2 ndc = full / float2( fullSize ) * 2.0 - 1.0;
		float r = fine * length( float3( ndc * farFade.zw, 1.0 ) );
		float s = clamp( r * r / max( farFade.x * farFade.x, 1.0 ), 0.0, 1.0 );
		float s2 = s * s;
		fragMask = lerp( 1.0, fragMask, s2 * s2 );
	}
}

struct SPIRV_Cross_Input
{
    float4 gl_FragCoord : SV_Position;
};

struct SPIRV_Cross_Output
{
    float fragMask : SV_Target0;
};

SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    gl_FragCoord = stage_input.gl_FragCoord;
    gl_FragCoord.w = 1.0 / gl_FragCoord.w;
    frag_main();
    SPIRV_Cross_Output stage_output;
    stage_output.fragMask = fragMask;
    return stage_output;
}
