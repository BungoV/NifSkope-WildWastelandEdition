// HLSL source of features/water/cell_underwater.frag (fragment shader), lane HLSLSRC3 (notes/hlslsrc3/tools/glsl2hlsl.py): one readable file for every
// variant, the GLSL's comments, names and order kept. Loose uniforms are the LooseUniforms cbuffer (b31).
// variants: #if none (the loader's defines; res/shaders/hlsl/variants.txt lists the sets built)
// profile: ps_6_0
#include "WaterBindingsVk.hlsli"
#define WW_HLSL_SRC 1	// the shared .hlsli files give their readable half
static float4 gl_FragCoord;
static int gl_SampleID;
static int gl_SampleMask[1];
// lane WATERUNDER1: the camera-underwater fog over the linear frame (src/features/water/cellunderwater.cpp draws it with
// glBlendFuncSeparate( GL_ONE, GL_SRC1_COLOR, GL_ZERO, GL_ONE ): frame = frame * mul + add, alpha kept).
// lane WATERUNDER4: with the frame's depth per sample (uwSamples > 0) this draw fogs only the samples near the
// pixel's farthest depth; its twin cell_underwater_ms.prog (WW_UW_SAMPLE) runs per sample and fogs the rest.

#include "water_under.hlsli"
// lane WATERLOOK1 step 16 (WATERUNDER2): the Underwater light row, the clear colour's coefficients + the meniscus
#define WW_LOOSE_TYPES 1
#include "water_info.hlsli"
#include "water_depth.hlsli"
#include "water_clear.hlsli"
#include "water_underlight.hlsli"
#undef WW_LOOSE_TYPES
cbuffer LooseUniforms : register(b31, space0)
{
#define WW_LOOSE_UNIFORMS 1
#include "water_info.hlsli"
#include "water_depth.hlsli"
#include "water_clear.hlsli"
#include "water_underlight.hlsli"
#undef WW_LOOSE_UNIFORMS
	int uwSamples;		// its samples; 0 = one fog per pixel from the farthest depth (one sample, or red maxdepth)
	row_major float4x4 uwInvProj;		// clip -> view
	float4 uwRow[3];		// view -> world rows
	float4 uwVp;		// the viewport x, y, w, h
	float3 uwEye;		// the camera, world
	float4 uwFog;		// amount, near, far, surface height
	float3 uwColor;
	float3 uwTint;
	int uwRed;
};
#include "water_info.hlsli"
#include "water_depth.hlsli"
#include "water_clear.hlsli"
#include "water_underlight.hlsli"

Texture2D<float4> uwDepth : register(t40, space0); SamplerState _uwDepth_sampler : register(s40, space0);	// R32F: the frame's farthest window depth per pixel (cellhdr.cpp wwCellHdrDepthMax)
Texture2DMS<float4> uwDepthMs : register(t41, space0);	// lane WATERUNDER4: the frame's depth per sample (cellhdr.cpp wwCellHdrDepthMsBegin)

static float4 fragAdd;
static float4 fragMul;

float3 uwWorld( float2 ndc, float z )
{
	float4 v = mul(float4( ndc, z, 1.0 ), uwInvProj);
	float4 p = float4( v.xyz / v.w, 1.0 );
	return float3( dot( uwRow[0], p ), dot( uwRow[1], p ), dot( uwRow[2], p ) );
}

// a sample this near the farthest one takes its fog: the law moves under 1/512 over the gap
float uwEdgeTol()
{
	float t = min( max( uwFog.z - uwFog.y, 1e-3 ), max( uwFog.z, 1e-3 ) ) / ( 512.0 * max( uwFog.x, 1e-3 ) );
	if ( uwLight != 0 ) {
		float3 bb;
		float3 k = wwClearK( uwClass, bb );
		t = min( t, 1.0 / ( 1536.0 * max( max( k.r, max( k.g, k.b ) ), 1e-6 ) * kWwUnitMetres ) );
	}
	return t;
}

// the same answer in both draws (precise), so every sample is fogged exactly once
bool uwNearMax( float2 ndc, float dMax, float dS, float tol )
{
	if ( ( dS >= 1.0 ) != ( dMax >= 1.0 ) )
		return false;
	precise float3 a = uwWorld( ndc, dMax * 2.0 - 1.0 );
	precise float3 b = uwWorld( ndc, dS * 2.0 - 1.0 );
	precise float e = distance( a, b );
	return e <= tol;
}

void frag_main()
{
	float d = uwDepth.Load( int3(int2( gl_FragCoord.xy ), 0) ).r;
	float2 ndc = ( gl_FragCoord.xy - uwVp.xy ) / uwVp.zw * 2.0 - 1.0;
#ifdef WW_UW_SAMPLE
	float dS = uwDepthMs.Load( int2( gl_FragCoord.xy ), gl_SampleID ).r;
	if ( uwNearMax( ndc, d, dS, uwEdgeTol() ) )
		discard;
	d = dS;
#else
	int mask = -1;
	if ( uwSamples > 0 ) {
		float tol = uwEdgeTol();
		mask = 0;
		for ( int i = 0; i < uwSamples; i++ )
			if ( uwNearMax( ndc, d, uwDepthMs.Load( int2( gl_FragCoord.xy ), i ).r, tol ) )
				mask |= 1 << i;
	}
	gl_SampleMask[0] = mask;
#endif
	float3 Pn = uwWorld( ndc, -1.0 );
	float3 Pw = uwWorld( ndc, d * 2.0 - 1.0 );
	float3 add, mul;
	if ( uwLight != 0 && ( uwLightRed & 4 ) == 0 )
		wwUnderLight( uwEye, Pn, Pw, d >= 1.0, uwFog.w, add, mul );
	else
		wwUnderwater( uwEye, Pn, Pw, d >= 1.0, uwFog, uwColor, uwTint, uwRed,
			( uwRed & 4 ) != 0 ? (float3)( 0.0 ) : -normalize( float3( uwRow[0].z, uwRow[1].z, uwRow[2].z ) ), add, mul );
	float men = wwUnderMeniscus( Pn.z - uwFog.w, fwidth( Pn.z ) * uwMenK.x );	// 0 with the row off
	fragAdd = float4( add * ( 1.0 - men ), 0.0 );
	fragMul = float4( mul * ( 1.0 - men ), 1.0 );
}

struct SPIRV_Cross_Input
{
    float4 gl_FragCoord : SV_Position;
    uint gl_SampleID : SV_SampleIndex;
};

struct SPIRV_Cross_Output
{
    float4 fragAdd : SV_Target0;
    float4 fragMul : SV_Target1;
#if !(defined(WW_UW_SAMPLE))
    uint gl_SampleMask : SV_Coverage;
#endif
};

SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    gl_FragCoord = stage_input.gl_FragCoord;
    gl_FragCoord.w = 1.0 / gl_FragCoord.w;
    gl_SampleID = int(stage_input.gl_SampleID);
    frag_main();
    SPIRV_Cross_Output stage_output;
    stage_output.fragAdd = fragAdd;
    stage_output.fragMul = fragMul;
#if !(defined(WW_UW_SAMPLE))
    stage_output.gl_SampleMask = gl_SampleMask[0];
#endif
    return stage_output;
}
