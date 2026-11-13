// HLSL source of lookdev_ground.frag, translated by notes/hlslsrc1/tools/glsl2hlsl.py (text level: comments, names and
// #if variants kept from the GLSL). Matrices hold GL's columns as rows (row_major blocks), so a GL product A * B
// reads mul(B, A).
// profile: ps_6_0
// variants: #if (the loader's WW_* defines select the variant; built with -D per set)
#include "CoreBindingsVk.hlsli"
#include "GlslCompat.hlsli"
#define WW_STAGE_PS 1
#define WW_HLSL_SRC 1	// the readable half of the other modules' includes
#ifdef WW_GROUND_CELL
#define WW_CELL_FX 1
#endif
#define WW_LOOSE_TYPES 1
#include "../air/lookdev_fog.hlsli"
#include "../shadows/ww_sunshadow.hlsli"
#include "../lights/cell_lights.hlsli"
#include "../lights/cell_cluster.hlsli"
#include "../overlays/debug_view.hlsli"
#include "../ao/cell_gihalfat.hlsli"
#include "../gi/cell_ddgi.hlsli"
#undef WW_LOOSE_TYPES
#define WW_LOOSE_UNIFORMS 1
#include "../air/lookdev_fog.hlsli"
#include "../shadows/ww_sunshadow.hlsli"
#include "../lights/cell_lights.hlsli"
#include "../lights/cell_cluster.hlsli"
#include "../overlays/debug_view.hlsli"
#include "../ao/cell_gihalfat.hlsli"
#include "../gi/cell_ddgi.hlsli"
#undef WW_LOOSE_UNIFORMS
// this program's registers for blocks its includes declare (glslang numbers them per program and variant)
#ifndef WW_REGS_SET
#define WW_REGS_SET
#if defined(WW_CELL_FOGLEAK) && defined(WW_FRAME_UBO)
#define WW_REG_wwShadowFrame b1
#endif
#endif
static float4 gl_FragCoord;

/* Lookdev ground plane (lane PBRR2B, docs s6.2 stage 1). Vanilla Commonwealth
 * ground (textures\landscape\Ground\CommonwealthDefault01_d/_n), tiled every
 * `tileSize` world units, lit by the lookdev sun (Lambert, linear units: a white
 * surface facing a sun of L reads L) and the DALC 6-axis ambient. Out through
 * the Studio output path. The _d is a legacy-header DXT5 (UNORM), decoded here. */

#include "uniforms.src.hlsli"

Texture2D<float4> BaseMap; SamplerState _BaseMap_sampler;
Texture2D<float4> NormalMap; SamplerState _NormalMap_sampler;
uniform bool hasBaseMap;
uniform bool hasNormalMap;
uniform float tileSize;
uniform float3 sunDirWorld;	// unit, TO the light
uniform float3 sunLinear;
uniform float3 dalc[6];		// linear: X+ X- Y+ Y- Z+ Z-
uniform bool dalcFlip;
uniform float groundLeak;	// < 0: normal draw; >= 0: red "groundleak" (the OFF path drawing a faint ground)
uniform float sceneExposure;
uniform int viewTransform;

static float3 worldPos;
static float3 viewPos;

static float4 fragColor;

// stage I/O: the members (locations, semantics) of the generated port, each under its GLSL declaration's #if
struct SPIRV_Cross_Input
{
    float3 worldPos : TEXCOORD0;
    float3 viewPos : TEXCOORD1;
    float4 gl_FragCoord : SV_Position;
};
struct SPIRV_Cross_Output
{
    float4 fragColor : SV_Target0;
};


#include "lookdev_output.src.hlsli"
#include "../air/lookdev_fog.hlsli"
#ifdef WW_SUNSHADOW
#include "../shadows/ww_sunshadow.hlsli"
#endif
#ifdef WW_GROUND_CELL
// lane LIGHTBOOTH1 (lookdev_ground_cell.prog, the light booth's floor): the placed cell lights' diffuse on the ground
#define WW_CELL_FX 1
#include "../lights/cell_lights.hlsli"
#endif

float3 dalcAmbient( float3 n )
{
	// the axis name is the light's TRAVEL direction (assumption, owed to Todd's treat):
	// an up-facing normal takes the Z- colour
	float3 n2 = n * n;
	bool fx = dalcFlip;
	float3 x = ( ( n.x > 0.0 ) != fx ) ? dalc[1] : dalc[0];
	float3 y = ( ( n.y > 0.0 ) != fx ) ? dalc[3] : dalc[2];
	float3 z = ( ( n.z > 0.0 ) != fx ) ? dalc[5] : dalc[4];
	return n2.x * x + n2.y * y + n2.z * z;
}

void frag_main()
{
	float2 uv = float2( worldPos.x, -worldPos.y ) / tileSize;
	float3 base = (float3)(0.18);
	if ( hasBaseMap )
		base = srgbToLinear( BaseMap.Sample(_BaseMap_sampler, uv).rgb );
	float3 n = float3( 0.0, 0.0, 1.0 );
	if ( hasNormalMap ) {
		float2 g = NormalMap.Sample(_NormalMap_sampler, uv).rg * 2.0 - 1.0;
		// DirectX-style green (+G = image down = world -Y here)
		n = normalize( float3( g.x, -g.y, sqrt( max( 1.0 - dot( g, g ), 0.0 ) ) ) );
	}
	float NdotL = max( dot( n, sunDirWorld ), 0.0 );
#ifdef WW_SUNSHADOW
	// lane CSM1 (spec 2.8): the cascade factor on the sun term only
	float3 color = base * sunLinear * NdotL * wwSunShadow( viewPos ) + base * dalcAmbient( n );
#else
	float3 color = base * sunLinear * NdotL + base * dalcAmbient( n );
#endif
#ifdef WW_GROUND_CELL
	// each placed light: its colour x radial curve x cone x shadow cube (cellLightE), x N.L, on the ground's albedo
	if ( cellOn ) {
		for ( int i = 0; i < cellLightCount; i++ ) {
			float3 Ld;
			bool noSpec;
			float3 E = cellLightE( i, worldPos, n, Ld, noSpec );
			color += base * E * max( dot( n, Ld ), 0.0 );
		}
	}
#endif
	color = wwFog( color, viewPos );	// lane FOG1: linear, before the exposure
#ifdef WW_GROUND_CELL
	// lane LIGHTBOOTH1: inside the cell's HDR frame, linear light like the cell's own programs (tone-mapped once)
	if ( cellOn && cellIsLinear ) {
		fragColor = float4( max( color, (float3)(0.0) ), 1.0 );
		return;
	}
#endif
	if ( groundLeak >= 0.0 )
		color = lerp( (float3)(0.0), color, groundLeak );
	fragColor = float4( studioOutput( color ), 1.0 );
	if ( groundLeak >= 0.0 )
		fragColor.a = groundLeak;
	float3 probe;
	if ( wwFogProbe( viewPos, probe ) )
		fragColor = float4( probe, 1.0 );
#ifdef WW_SUNSHADOW
	float3 csmProbeOut;
	if ( wwSunShadowProbe( viewPos, fragColor.rgb, csmProbeOut ) )
		fragColor = float4( csmProbeOut, 1.0 );
#endif
}

// entry: copies the stage inputs into the GLSL-named globals, runs the GLSL main, returns the outputs
SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    gl_FragCoord = stage_input.gl_FragCoord;
    gl_FragCoord.w = 1.0 / gl_FragCoord.w;
    worldPos = stage_input.worldPos;
    viewPos = stage_input.viewPos;
    frag_main();
    SPIRV_Cross_Output stage_output;
    stage_output.fragColor = fragColor;
    return stage_output;
}
