// HLSL source of features/post/cell_fogbloom.frag (fragment shader), lane HLSLSRC2 (notes/hlslsrc2/tools/glsl2hlsl.py): one readable file for every
// variant, the GLSL's comments, names and order kept. Loose uniforms are the LooseUniforms cbuffer (b31).
// variants: #if WW_CELL_FOGLEAK (the loader's defines; res/shaders/hlsl/variants.txt lists the sets built)
// profile: ps_6_0
#include "PostBindingsVk.hlsli"
#define WW_HLSL_SRC 1	// the shared .hlsli files give their readable half
// inlined (no readable .hlsli yet): uniforms.glsl
static float4 gl_FragCoord;
/* lane FOGBLOOM1 (src/features/post/cellfogbloom.h): the bloom-intensity target (RDR2's R8 "bloom intensity"). Per pixel, how much
 * more bloom the tone map adds there: gain = 1 + k f (1 - T) (1 + b translucent), stored as (gain - 1) / 4.
 *   T  the air's transmittance along the view ray to the pixel's depth (the froxel volume, lookdev_fog.glsl), so any
 *      change in the air's density moves it; a sky pixel takes the ray to the grid's end
 *   f  smoothstep over the ray's MEAN extinction tau / d, from mist (V 2 km) to fog (V 1 km): 0 for every ray through
 *      clearer air, so clear weather writes exactly 0 and the tone map draws today's bits */

// uniforms.glsl inlined here: its .hlsli has no readable half yet (lane HLSLSRC1)
cbuffer globalUniforms : register(b0, space0)
{
	row_major float3x3	viewMatrix : packoffset(c0);					// rotation part of view transform
	row_major float3x3	envMapRotation : packoffset(c3);				// view space to environment map
	row_major float4x4	projectionMatrix : packoffset(c6);
	float4	lightSourcePosition[3] : packoffset(c10);
	float4	lightSourceDiffuse[3] : packoffset(c13);
	float4	lightSourceAmbient : packoffset(c16);
	float	toneMapScale : packoffset(c17);				// 1.0 = full tone mapping
	float	brightnessScale : packoffset(c17.y);
	float	glowScale : packoffset(c17.z);
	float	glowScaleSRGB : packoffset(c17.w);
	int4	viewportDimensions : packoffset(c18);			// X, Y, width, height
	uint	doSkinning : packoffset(c19);
	int	sceneOptions : packoffset(c19.y);
	int	cubeBgndMipLevel : packoffset(c19.z);
	int	sfParallaxMaxSteps : packoffset(c19.w);
	float	sfParallaxScale : packoffset(c20);
	float	sfParallaxOffset : packoffset(c20.y);
	float	unusedUniform1 : packoffset(c20.z);
	float	unusedUniform2 : packoffset(c20.w);
};
// end of uniforms.glsl
#define WW_LOOSE_TYPES 1
#include "../air/lookdev_fog.hlsli"
#undef WW_LOOSE_TYPES
cbuffer LooseUniforms : register(b31, space0)
{
#define WW_LOOSE_UNIFORMS 1
#include "../air/lookdev_fog.hlsli"
#undef WW_LOOSE_UNIFORMS
	float2 fbProj;			// the projection's [0][0], [1][1]
	float2 fbProjZ;			// the projection's [2][2], [3][2]
	float4 fbParams;			// k, b, sigma at f = 0, sigma at f = 1 (per game unit)
	int fbTranslucent;		// 1: the second draw, over the samples a blended linear draw wrote last (stencil 0x04)
};
#include "../air/lookdev_fog.hlsli"

Texture2D<float4> fbDepth : register(t31, space0); SamplerState _fbDepth_sampler : register(s31, space0);		// the frame's depth, samples nearest-resolved (window depth)

static float4 fragColor;

void frag_main()
{
	fragColor = (float4)( 0.0 );
	if ( or( or( !fogOn, !volOn ), volSlices.w < 0.5 ) )
		return;
	float2 ndc = ( gl_FragCoord.xy - volRect.xy ) * volRect.zw * 2.0 - 1.0;
	float3 dirView = float3( ndc.x / fbProj.x, ndc.y / fbProj.y, -1.0 );
	float z = fbDepth.Load( int3(int2( gl_FragCoord.xy ), 0) ).r;
	float viewZ;	// -z in view units
	if ( z >= 1.0 ) {	// the sky: the ray to the grid's end, not the horizon's endless tail (T 0 in any air)
		float far = volFar.w > 0.5 ? volFar.x * exp( volFar.y ) : volSlices.x * exp( volSlices.y );
		viewZ = 0.999 * far / ( max( fogDistScale, 1e-12 ) * length( dirView ) );
	} else {
		viewZ = max( fbProjZ.y / ( 2.0 * z - 1.0 + fbProjZ.x ), 0.0 );
	}
	float3 posView = dirView * viewZ;
	float d = length( posView ) * fogDistScale;
	float T = clamp( wwVolAirAt( posView ).a, 1e-6, 1.0 );
	float f = smoothstep( fbParams.z, fbParams.w, -log( T ) / max( d, 1.0 ) );
	float extra = fbParams.x * f * ( 1.0 - T ) * ( fbTranslucent == 1 ? 1.0 + fbParams.y : 1.0 );
	fragColor = float4( clamp( extra * 0.25, 0.0, 1.0 ), 0.0, 0.0, 1.0 );
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
