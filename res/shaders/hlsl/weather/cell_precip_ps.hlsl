// HLSL source of features/weather/cell_precip.frag (fragment shader), lane HLSLSRC2 (notes/hlslsrc2/tools/glsl2hlsl.py): one readable file for every
// variant, the GLSL's comments, names and order kept. Loose uniforms are the LooseUniforms cbuffer (b31).
// variants: #if WW_CELL_AIRZONE, WW_CELL_FOGLEAK, WW_CELL_RESTIR, WW_CELL_RESTIRGI, WW_CELL_RTDDGI, WW_CELL_RTDDGI_CASCADES, WW_FRAME_UBO (the loader's defines; res/shaders/hlsl/variants.txt lists the sets built)
// profile: ps_6_0
#include "WeatherBindingsVk.hlsli"
#define WW_HLSL_SRC 1	// the shared .hlsli files give their readable half
// inlined (no readable .hlsli yet): uniforms.glsl
static float4 gl_FragCoord;
/* lane WEATHERLOOK1 phase B: a precipitation particle in the cell view's linear frame (src/features/weather/cellprecip.cpp).
 * Lit by the volumetric particles' law (cell_volpart.glsl vpLight, VOLPART1): the sun x its cascades, every placed light
 * x the air to it, the GI and the ambient, through the air's phase. A flake faces the eye (flat-on); its thin share
 * (translucency, airpart_data.md: dry snow 0.6, wet 0.3) scatters through the phase, so a flake between the eye and a
 * lamp glows in the lamp's halo; the rest is the wrapped diffuse at its face. Then seen through the air (wwFogFx), so
 * far flakes fade into the fog. Alpha blended; no depth write (the scene's depth tests it). */

#define WW_CELLLIGHTS 1
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
#include "../lights/cell_lights.hlsli"
#include "../lights/cell_cluster.hlsli"
#if defined(WW_DEBUG_VIEWS)
#include "../overlays/debug_view.hlsli"
#endif
#if defined(WW_CELL_PRT) && !(defined(WW_CELL_GI_MARCH))
#include "../ao/cell_gihalfat.hlsli"
#endif
#if defined(WW_CELL_PRT) && !(defined(WW_CELL_GI_MARCH)) && defined(WW_CELL_RTDDGI)
#include "../gi/cell_ddgi.hlsli"
#endif
#if !(defined(WW_CELL_PRT)) && defined(WW_CELL_RTDDGI)
#include "../gi/cell_ddgi.hlsli"
#include "../ao/cell_gihalfat.hlsli"
#endif
#include "../air/cell_volpart.hlsli"
#include "../lights/cell_fxlaw.hlsli"
#if !defined(WW_SUNSHADOW)
#include "../shadows/ww_sunshadow.hlsli"
#endif
#ifndef WW_AIR_GIVOL
#define WW_AIR_GIVOL 1	// as below, before this file
#endif
#include "../air/cell_airmed.hlsli"
#if ((defined( WW_CELL_AIRZONE ) && !defined( WW_FX_DECALLIT )))
#include "../air/cell_airzone.hlsli"
#endif
#undef WW_LOOSE_TYPES
cbuffer LooseUniforms : register(b31, space0)
{
#define WW_LOOSE_UNIFORMS 1
#include "../air/lookdev_fog.hlsli"
#include "../lights/cell_lights.hlsli"
#include "../lights/cell_cluster.hlsli"
#if defined(WW_DEBUG_VIEWS)
#include "../overlays/debug_view.hlsli"
#endif
#if defined(WW_CELL_PRT) && !(defined(WW_CELL_GI_MARCH))
#include "../ao/cell_gihalfat.hlsli"
#endif
#if defined(WW_CELL_PRT) && !(defined(WW_CELL_GI_MARCH)) && defined(WW_CELL_RTDDGI)
#include "../gi/cell_ddgi.hlsli"
#endif
#if !(defined(WW_CELL_PRT)) && defined(WW_CELL_RTDDGI)
#include "../gi/cell_ddgi.hlsli"
#include "../ao/cell_gihalfat.hlsli"
#endif
#include "../air/cell_volpart.hlsli"
#include "../lights/cell_fxlaw.hlsli"
#if !defined(WW_SUNSHADOW)
#include "../shadows/ww_sunshadow.hlsli"
#endif
#ifndef WW_AIR_GIVOL
#define WW_AIR_GIVOL 1	// as below, before this file
#endif
#include "../air/cell_airmed.hlsli"
#if ((defined( WW_CELL_AIRZONE ) && !defined( WW_FX_DECALLIT )))
#include "../air/cell_airzone.hlsli"
#endif
#undef WW_LOOSE_UNIFORMS
	float lightingInfluence;	// cell_volpart.glsl's vpLit reads them (not used here)
	int fxLitMode;
	bool pcHasTex;
	int pcFar;
	float4 pcAtlas;		// cell uv size, the deepest mip a near quad reads, the one-texel-a-cell mip
	float pcTrans;		// the thin (scattering) share
	float2 pcNear;		// view units from the eye: gone at x, whole at y (a flake never covers the lens)
};
#include "../air/lookdev_fog.hlsli"
#include "../lights/cell_lights.hlsli"

#include "../air/cell_volpart.hlsli"

Texture2D<float4> pcTex : register(t46, space0); SamplerState _pcTex_sampler : register(s46, space0);

static float2 gUv;
static float2 gQ;
static float3 gView;
static float4 gCol;
static float4 gBox;

static float4 fragColor;

void frag_main()
{
	float4 t;
	if ( pcFar != 0 ) {
		// a far dot: the cell's mean (gCol) over a soft round footprint whose mean over the quad is 1
		t = float4( 1.0, 1.0, 1.0, exp( -2.5 * dot( gQ, gQ ) ) / 0.2985 );
	} else if ( pcHasTex ) {
		float lod = min( float2(pcTex.CalculateLevelOfDetail( _pcTex_sampler, gUv ), pcTex.CalculateLevelOfDetailUnclamped(_pcTex_sampler, gUv)).x, pcAtlas.z );
		t = pcTex.SampleLevel( _pcTex_sampler, clamp( gUv, gBox.xy, gBox.zw ), lod );
	} else {
		t = float4( 1.0, 1.0, 1.0, clamp( 1.0 - dot( gQ, gQ ), 0.0, 1.0 ) );
	}
	float a = clamp( t.a * gCol.a, 0.0, 1.0 ) * smoothstep( pcNear.x, pcNear.y, -gView.z );
	if ( a < 1.0 / 512.0 )
		discard;
	float3 alb = t.rgb * gCol.rgb;	// linear: the sRGB texture decodes on read
	float3 lit = (float3)( 1.0 );
	if ( or( cellInterior, cellHasDir ) ) {
		float3 thin, thick;
		vpLight( gView, cellWorldPos( gView ), cellWorldDir( gView ), dot( fogView.xyz, gView ) + fogView.w,
			cellWorldDir( float3( 0.0, 0.0, 1.0 ) ), pcTrans, thin, thick );
		lit = pcTrans * thin + ( 1.0 - pcTrans ) * thick;
	}
	fragColor = float4( wwFogFx( alb * lit, gView, false ), a );
}

struct SPIRV_Cross_Input
{
    float2 gUv : TEXCOORD0;
    float2 gQ : TEXCOORD1;
    float3 gView : TEXCOORD2;
    nointerpolation float4 gCol : TEXCOORD3;
    nointerpolation float4 gBox : TEXCOORD4;
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
    gUv = stage_input.gUv;
    gQ = stage_input.gQ;
    gView = stage_input.gView;
    gCol = stage_input.gCol;
    gBox = stage_input.gBox;
    frag_main();
    SPIRV_Cross_Output stage_output;
    stage_output.fragColor = fragColor;
    return stage_output;
}
