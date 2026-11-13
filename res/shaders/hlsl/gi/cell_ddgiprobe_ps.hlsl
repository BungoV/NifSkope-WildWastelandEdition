// HLSL source of features/gi/cell_ddgiprobe.frag (fragment shader), lane HLSLSRC2 (notes/hlslsrc2/tools/glsl2hlsl.py): one readable file for every
// variant, the GLSL's comments, names and order kept. Loose uniforms are the LooseUniforms cbuffer (b31).
// variants: #if WW_CELL_FOGLEAK, WW_CELL_RTDDGI, WW_CELL_RTDDGI_CASCADES (the loader's defines; res/shaders/hlsl/variants.txt lists the sets built)
// profile: ps_6_0
#include "GiBindingsVk.hlsli"
#define WW_HLSL_SRC 1	// the shared .hlsli files give their readable half
// inlined (no readable .hlsli yet): uniforms.glsl
static float4 gl_FragCoord;
/* Lane RTXFIX1 (2026-10-30): the Ray-traced GI probes view, fragment stage, inside the cell's linear frame (bungo 10-29:
 * "the spheres draw on top of the fog, unfogged, crisp at any distance"). Irradiance view: this probe alone at the
 * sphere's normal, the read's decode (cell_ddgi.glsl cellDdgiTile: bilinear on the encoded tile, ^2.5, squared, x 2 pi),
 * shown as a white Lambertian surface lit by it (E / pi), linear, so the exposure and the tone map treat it as they treat
 * every surface; a probe the read never takes is dark grey. States view: the flat state colours, divided by the scene's
 * exposure so they keep their look through it. Then the fog (lookdev_fog.glsl wwFog), as on every cell surface. */

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


// MERGE1030A (RTXFIX1 x RTXCASCADE1): the view reads cascade 0's own atlas in M3's layout (rtddgiCellUniforms' one-volume
// path: this program has no cellDdgiCount); the loader's cascades define would rename cellDdgiDims to the cascades' array
#undef WW_CELL_RTDDGI_CASCADES
#define WW_LOOSE_TYPES 1
#include "cell_ddgi.hlsli"
#include "../air/lookdev_fog.hlsli"
#undef WW_LOOSE_TYPES
cbuffer LooseUniforms : register(b31, space0)
{
#define WW_LOOSE_UNIFORMS 1
#include "cell_ddgi.hlsli"
#include "../air/lookdev_fog.hlsli"
#undef WW_LOOSE_UNIFORMS
	int cellGiLoop8;	// cell_ddgi.glsl's read names it (cellDdgiRead is not called here)
	int probeMode;		// 1 states, 2 irradiance
	bool probeFog;		// false: no fog (WW_CELL_RTDDGI_VIEW_FOG=nofog)
	float probeExposure;	// the scene's exposure scale (the states view's colours keep their look)
};
#include "cell_ddgi.hlsli"



static float3 probeN;
static float3 viewPos;
static int probeIdx;
static float probeState;

static float4 fragColor;

#include "../air/lookdev_fog.hlsli"

float3 srgbToLinear( float3 c )
{
	return pow( c, (float3)( 2.2 ) );
}

void frag_main()
{
	if ( !cellDdgiOn )
		discard;	// no volume up for this document (src/features/gi/rtddgicell.cpp rtddgiCellUniforms)
	float3 col;
	if ( probeMode == 1 ) {
		// Vigilant green, Asleep blue, Off red, fresh white (src/features/gi/rtddgicell.cpp's states view)
		float3 fc = float3( 0.2, 0.9, 0.3 );
		if ( probeState > 0.5 && probeState < 1.5 )
			fc = float3( 0.35, 0.45, 0.95 );
		else if ( probeState > 1.5 && probeState < 2.5 )
			fc = float3( 0.95, 0.15, 0.15 );
		else if ( probeState > 2.5 )
			fc = float3( 0.95, 0.95, 0.95 );
		col = srgbToLinear( fc ) / max( probeExposure, 1e-6 );
	} else {
		float4 pr = cellDdgiAtlas.Load( 192 * cellDdgiDims.w + probeIdx );
		if ( int( pr.w + 0.5 ) != 2 ) {
			col = srgbToLinear( (float3)( 0.12 ) ) / max( probeExposure, 1e-6 );	// the read never takes it
		} else {
			float4 e = cellDdgiTile( probeIdx, normalize( probeN ), 8 );
			float3 q = float3( pow( max( e.x, 0.0 ), 2.5 ), pow( max( e.y, 0.0 ), 2.5 ), pow( max( e.z, 0.0 ), 2.5 ) );
			col = q * q * 2.0;	// E / pi = q^2 x 2 pi / pi
		}
	}
	if ( probeFog )
		col = wwFog( col, viewPos );
	fragColor = float4( max( col, (float3)( 0.0 ) ), 1.0 );
}

struct SPIRV_Cross_Input
{
    float3 probeN : TEXCOORD0;
    float3 viewPos : TEXCOORD1;
    nointerpolation int probeIdx : TEXCOORD2;
    nointerpolation float probeState : TEXCOORD3;
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
    probeN = stage_input.probeN;
    viewPos = stage_input.viewPos;
    probeIdx = stage_input.probeIdx;
    probeState = stage_input.probeState;
    frag_main();
    SPIRV_Cross_Output stage_output;
    stage_output.fragColor = fragColor;
    return stage_output;
}
