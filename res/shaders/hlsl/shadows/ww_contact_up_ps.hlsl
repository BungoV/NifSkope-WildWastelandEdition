// HLSL source of features/shadows/ww_contact_up.frag (fragment shader), lane HLSLSRC2 (notes/hlslsrc2/tools/glsl2hlsl.py): one readable file for every
// variant, the GLSL's comments, names and order kept. Loose uniforms are the LooseUniforms cbuffer (b31).
// variants: #if none (the loader's defines; res/shaders/hlsl/variants.txt lists the sets built)
// profile: ps_6_0
#include "ShadowsBindingsVk.hlsli"
#define WW_HLSL_SRC 1	// the shared .hlsli files give their readable half
static float4 gl_FragCoord;
// lane PERFFIX2 (item 7): the half-size contact mask (Low / Medium, and every SHADOWPAR1 tier) brought to full size
// with a depth-aware bilinear upsample, so a silhouette's shadow does not bleed onto the surface behind it (the
// receivers used to read the half-size mask with a plain bilinear tap: halos). A mask texel counts only as far as
// its receiver's depth (the full-size texel its march started from) agrees with this pixel's own, within `tolerance`
// x depth (lane GAPLOOK1's far resolve rule, ww_contact_farcomp.frag); where no texel agrees, the one nearest in depth.

Texture2D<float4> gbuf : register(t0, space0); SamplerState _gbuf_sampler : register(s0, space0);		// the full-size opaque pass (.a linear depth; 1e6 none)
Texture2D<float4> mask : register(t1, space0); SamplerState _mask_sampler : register(s1, space0);		// the half-size contact mask (R8)
cbuffer LooseUniforms : register(b31, space0)
{
	int2 fullSize;
	int2 maskSize;
	float maskScale;	// full pixels per mask pixel (2)
	float tolerance;
};


static float fragMask;

void frag_main()
{
	int2 fq = int2( gl_FragCoord.xy );
	float z = gbuf.Load( int3(fq, 0) ).a;
	float2 mp = gl_FragCoord.xy / maskScale - 0.5;
	int2 base = int2( floor( mp ) );
	float2 fr = mp - float2( base );
	if ( or( z >= 9.0e5, z <= 0.0 ) ) {
		fragMask = mask.Load( int3(clamp( int2( floor( gl_FragCoord.xy / maskScale ) ), (int2)( 0 ), maskSize - 1 ), 0) ).r;
		return;
	}
	float wsum = 0.0, acc = 0.0, best = 1e30, bestM = 1.0;
	for ( int j = 0; j < 2; j++ ) {
		for ( int i = 0; i < 2; i++ ) {
			int2 c = clamp( base + int2( i, j ), (int2)( 0 ), maskSize - 1 );
			float m = mask.Load( int3(c, 0) ).r;
			int2 rq = clamp( int2( floor( ( float2( c ) + 0.5 ) * maskScale ) ), (int2)( 0 ), fullSize - 1 );
			float cz = gbuf.Load( int3(rq, 0) ).a;
			float dz = abs( cz - z );
			if ( dz < best ) {
				best = dz;
				bestM = m;
			}
			float bw = ( i == 0 ? 1.0 - fr.x : fr.x ) * ( j == 0 ? 1.0 - fr.y : fr.y );
			float w = bw * clamp( 1.0 - dz / ( z * tolerance ), 0.0, 1.0 );
			wsum += w;
			acc += w * m;
		}
	}
	fragMask = wsum > 1e-4 ? acc / wsum : bestM;
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
