// HLSL source of lookdev_ground.frag (fragment shader), lane HLSLSRC1 (notes/hlsltools/glsl2hlsl.py): one readable file for every
// variant, the GLSL's comments, names and order kept. Loose uniforms are the LooseUniforms cbuffer (b31).
// variants: #if WW_CELL_FOGLEAK, WW_CELL_RESTIR, WW_CELL_RESTIRGI, WW_CELL_RTDDGI, WW_CELL_RTDDGI_CASCADES, WW_FRAME_UBO (the loader's defines; res/shaders/hlsl/variants.txt lists the sets built)
// profile: ps_6_0
#include "CoreBindingsVk.hlsli"
#define WW_HLSL_SRC 1	// the shared .hlsli files give their readable half
static float4 gl_FragCoord;
/* Lookdev ground plane (lane PBRR2B, docs s6.2 stage 1). Vanilla Commonwealth
 * ground (textures\landscape\Ground\CommonwealthDefault01_d/_n), tiled every
 * `tileSize` world units, lit by the lookdev sun (Lambert, linear units: a white
 * surface facing a sun of L reads L) and the DALC 6-axis ambient. Out through
 * the Studio output path. The _d is a legacy-header DXT5 (UNORM), decoded here. */

#include "uniforms.hlsli"

Texture2D<float4> BaseMap : register(t46, space0); SamplerState _BaseMap_sampler : register(s46, space0);
Texture2D<float4> NormalMap : register(t47, space0); SamplerState _NormalMap_sampler : register(s47, space0);
#define WW_LOOSE_TYPES 1
#include "../air/lookdev_fog.hlsli"
#if defined(WW_SUNSHADOW)
#include "../shadows/ww_sunshadow.hlsli"
#endif
#if defined(WW_GROUND_CELL)
#if defined(WW_GROUND_CELL)
#ifndef WW_CELL_FX
#define WW_CELL_FX 1	// as below, before this file
#endif
#endif
#include "../lights/cell_lights.hlsli"
#include "../lights/cell_cluster.hlsli"
#endif
#if defined(WW_GROUND_CELL) && defined(WW_DEBUG_VIEWS)
#include "../overlays/debug_view.hlsli"
#endif
#if defined(WW_GROUND_CELL) && defined(WW_CELL_PRT) && !(defined(WW_CELL_GI_MARCH))
#include "../ao/cell_gihalfat.hlsli"
#endif
#if defined(WW_GROUND_CELL) && defined(WW_CELL_PRT) && !(defined(WW_CELL_GI_MARCH)) && defined(WW_CELL_RTDDGI)
#include "../gi/cell_ddgi.hlsli"
#endif
#if defined(WW_GROUND_CELL) && !(defined(WW_CELL_PRT)) && defined(WW_CELL_RTDDGI)
#include "../gi/cell_ddgi.hlsli"
#include "../ao/cell_gihalfat.hlsli"
#endif
#undef WW_LOOSE_TYPES
cbuffer LooseUniforms : register(b31, space0)
{
#define WW_LOOSE_UNIFORMS 1
#include "../air/lookdev_fog.hlsli"
#if defined(WW_SUNSHADOW)
#include "../shadows/ww_sunshadow.hlsli"
#endif
#if defined(WW_GROUND_CELL)
#if defined(WW_GROUND_CELL)
#ifndef WW_CELL_FX
#define WW_CELL_FX 1	// as below, before this file
#endif
#endif
#include "../lights/cell_lights.hlsli"
#include "../lights/cell_cluster.hlsli"
#endif
#if defined(WW_GROUND_CELL) && defined(WW_DEBUG_VIEWS)
#include "../overlays/debug_view.hlsli"
#endif
#if defined(WW_GROUND_CELL) && defined(WW_CELL_PRT) && !(defined(WW_CELL_GI_MARCH))
#include "../ao/cell_gihalfat.hlsli"
#endif
#if defined(WW_GROUND_CELL) && defined(WW_CELL_PRT) && !(defined(WW_CELL_GI_MARCH)) && defined(WW_CELL_RTDDGI)
#include "../gi/cell_ddgi.hlsli"
#endif
#if defined(WW_GROUND_CELL) && !(defined(WW_CELL_PRT)) && defined(WW_CELL_RTDDGI)
#include "../gi/cell_ddgi.hlsli"
#include "../ao/cell_gihalfat.hlsli"
#endif
#undef WW_LOOSE_UNIFORMS
	bool hasBaseMap;
	bool hasNormalMap;
	float tileSize;
	float3 sunDirWorld;	// unit, TO the light
	float3 sunLinear;
	float3 dalc[6];		// linear: X+ X- Y+ Y- Z+ Z-
	bool dalcFlip;
	float groundLeak;	// < 0: normal draw; >= 0: red "groundleak" (the OFF path drawing a faint ground)
	float sceneExposure;
	int viewTransform;
};


static float3 worldPos;
static float3 viewPos;

static float4 fragColor;

#include "lookdev_output.hlsli"
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
	float3 base = (float3)( 0.18 );
	if ( hasBaseMap )
		base = srgbToLinear( BaseMap.Sample( _BaseMap_sampler, uv ).rgb );
	float3 n = float3( 0.0, 0.0, 1.0 );
	if ( hasNormalMap ) {
		float2 g = NormalMap.Sample( _NormalMap_sampler, uv ).rg * 2.0 - 1.0;
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
		fragColor = float4( max( color, (float3)( 0.0 ) ), 1.0 );
		return;
	}
#endif
	if ( groundLeak >= 0.0 )
		color = lerp( (float3)( 0.0 ), color, groundLeak );
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
