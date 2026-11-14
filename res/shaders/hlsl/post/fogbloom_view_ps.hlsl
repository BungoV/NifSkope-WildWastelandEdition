// profile: ps_6_0
// Lane VKFAR2 (2026-11-14, row P4b): the fog bloom's own pass on the present device (vkfogbloom.inc, WW_VKVIEW=1; red
// WW_VKVIEW_FOGBLOOM_RED=1 keeps GL's). Hand-kept: the body is features/post/cell_fogbloom.frag (its readable twin
// cell_fogbloom_ps.hlsl, HLSLSRC2) as the app builds it (WW_CELL_FOGLEAK). Hand parts, against the GL program:
//   - fbDepth: GL blits the frame's multisample depth NEAREST into a one-sample texture first; here the pass reads the
//     tone frame's depth itself, sample FB_DEPTH_SAMPLE of the pixel (tests/vkframe/fogbloomview.sh proves which one
//     GL's blit keeps).
//   - the second draw: GL tests the frame's stencil (EQUAL 0x04, mask 0x04) per sample; here the pass has no depth /
//     stencil attachment, reads the stencil (fbStencil) and writes only those samples (SV_Coverage).
//   - globalUniforms (uniforms.glsl) is left out: the program reads none of it.
// Built by src/features/vkframe/vkframe_spirv.sh fx() (b 0 / t 64 / s 128); the b31 layout is kT_fogbloom (vkpresent.h).
#include "PostBindingsVk.hlsli"
#define WW_HLSL_SRC 1	// the shared .hlsli files give their readable half
#define WW_CELL_FOGLEAK 1	// the loader's define for the cell programs (the air's one-tail shape)
#ifndef FB_DEPTH_SAMPLE
#define FB_DEPTH_SAMPLE 0	// the depth sample GL's NEAREST blit keeps (gates only: the red reads another)
#endif
static float4 gl_FragCoord;
/* lane FOGBLOOM1 (src/features/post/cellfogbloom.h): the bloom-intensity target (RDR2's R8 "bloom intensity"). Per pixel, how much
 * more bloom the tone map adds there: gain = 1 + k f (1 - T) (1 + b translucent), stored as (gain - 1) / 4.
 *   T  the air's transmittance along the view ray to the pixel's depth (the froxel volume, lookdev_fog.glsl), so any
 *      change in the air's density moves it; a sky pixel takes the ray to the grid's end
 *   f  smoothstep over the ray's MEAN extinction tau / d, from mist (V 2 km) to fog (V 1 km): 0 for every ray through
 *      clearer air, so clear weather writes exactly 0 and the tone map draws today's bits */

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

Texture2DMS<float> fbDepth : register(t31, space0);		// the tone frame's depth (D24, the depth aspect), per sample
Texture2DMS<uint> fbStencil : register(t24, space0);	// the tone frame's stencil (a stencil-aspect view, as the tone pass reads it)

static float4 fragColor;

void frag_main()
{
	fragColor = (float4)( 0.0 );
	if ( or( or( !fogOn, !volOn ), volSlices.w < 0.5 ) )
		return;
	float2 ndc = ( gl_FragCoord.xy - volRect.xy ) * volRect.zw * 2.0 - 1.0;
	float3 dirView = float3( ndc.x / fbProj.x, ndc.y / fbProj.y, -1.0 );
	float z = fbDepth.Load( int2( gl_FragCoord.xy ), FB_DEPTH_SAMPLE ).r;
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

// GL's stencil test of the second draw: EQUAL 0x04 under mask 0x04, per sample; the first draw takes every sample
uint fbCoverage()
{
	if ( fbTranslucent != 1 )
		return 0xFFFFFFFFu;
	uint w, h, n;
	fbStencil.GetDimensions( w, h, n );
	uint m = 0u;
	for ( uint s = 0u; s < n; s++ )
		if ( ( fbStencil.Load( int2( gl_FragCoord.xy ), int( s ) ) & 4u ) != 0u )
			m |= 1u << s;
	return m;
}

struct SPIRV_Cross_Input
{
	float4 gl_FragCoord : SV_Position;
};

struct SPIRV_Cross_Output
{
	float4 fragColor : SV_Target0;
	uint coverage : SV_Coverage;
};

SPIRV_Cross_Output main( SPIRV_Cross_Input stage_input )
{
	gl_FragCoord = stage_input.gl_FragCoord;
	gl_FragCoord.w = 1.0 / gl_FragCoord.w;
	frag_main();
	SPIRV_Cross_Output stage_output;
	stage_output.fragColor = fragColor;
	stage_output.coverage = fbCoverage();
	return stage_output;
}
