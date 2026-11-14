// HLSL source of features/clouds/cloud_flysdf.frag (fragment shader), lane HLSLSRC2 (notes/hlslsrc2/tools/glsl2hlsl.py): one readable file for every
// variant, the GLSL's comments, names and order kept. Loose uniforms are the LooseUniforms cbuffer (b31).
// variants: #if none (the loader's defines; res/shaders/hlsl/variants.txt lists the sets built)
// profile: ps_6_0
#include "CloudsBindingsVk.hlsli"
#define WW_HLSL_SRC 1	// the shared .hlsli files give their readable half
static float4 gl_FragCoord;
/* lane CLOUD4: the fly-through grid's distance field, one separable pass a draw (src/features/clouds/cloudfly.cpp). The Chebyshev
 * distance in cells to the nearest occupied cell, capped at kCap: pass x reads the occupancy, passes y and z the
 * previous pass (d = min over k of max(|k|, src(k))). Cells off the grid count empty. Twin: cloudflymath.h chebyshev. */

Texture3D<float4> cfSrc : register(t0, space0); SamplerState _cfSrc_sampler : register(s0, space0);
cbuffer LooseUniforms : register(b31, space0)
{
	int cfAxis;	// 0 x, 1 y, 2 z
	int cfLayer;
	float3 cfDim;	// cells x, y, z (whole numbers)
};


static float4 cfOut;

static const int kCap = 15;

void frag_main()
{
	int3 c = int3( int2( gl_FragCoord.xy ), cfLayer );
	int3 ax = int3( cfAxis == 0 ? 1 : 0, cfAxis == 1 ? 1 : 0, cfAxis == 2 ? 1 : 0 );
	int3 dim = int3( cfDim + 0.5 );
	int n = cfAxis == 0 ? dim.x : cfAxis == 1 ? dim.y : dim.z;
	int at = cfAxis == 0 ? c.x : cfAxis == 1 ? c.y : c.z;
	int best = kCap;
	for ( int k = -kCap; k <= kCap; k++ ) {
		if ( or( at + k < 0, at + k >= n ) )
			continue;
		float v = cfSrc.Load( int4(c + ax * k, 0) ).r;
		int d = cfAxis == 0 ? ( v > 0.5 ? abs( k ) : kCap ) : max( abs( k ), int( v * 255.0 + 0.5 ) );
		best = min( best, d );
	}
	cfOut = (float4)( float( best ) / 255.0 );
}

struct SPIRV_Cross_Input
{
    float4 gl_FragCoord : SV_Position;
};

struct SPIRV_Cross_Output
{
    float4 cfOut : SV_Target0;
};

SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    gl_FragCoord = stage_input.gl_FragCoord;
    gl_FragCoord.w = 1.0 / gl_FragCoord.w;
    frag_main();
    SPIRV_Cross_Output stage_output;
    stage_output.cfOut = cfOut;
    return stage_output;
}
