// HLSL source of features/reflections/cell_reflrelight.frag (fragment shader), lane HLSLSRC2 (notes/hlslsrc2/tools/glsl2hlsl.py): one readable file for every
// variant, the GLSL's comments, names and order kept. Loose uniforms are the LooseUniforms cbuffer (b31).
// variants: #if WW_CELL_FOGLEAK, WW_CELL_RESTIR, WW_CELL_RESTIRGI, WW_CELL_RTDDGI, WW_CELL_RTDDGI_CASCADES, WW_FRAME_UBO (the loader's defines; res/shaders/hlsl/variants.txt lists the sets built)
// profile: ps_6_0
#include "ReflectionsBindingsVk.hlsli"
#define WW_HLSL_SRC 1	// the shared .hlsli files give their readable half
// inlined (no readable .hlsli yet): uniforms.glsl
static float4 gl_FragCoord;
/* lane REFLPROBE1 (src/features/reflections/cellreflprobe.h): THE RELIGHT. Per texel of a probe's octahedral tile: the direction, the
 * G-buffer cube's hit there (albedo, world normal, distance), lit by the same code the surfaces take
 * (cell_lights.glsl): the placed lights with their shadow cubes, the probe GI (its bounce, and outdoors its sky:
 * the sky visibility stands in for RDR2's baked AO cube), the ambient (indoors the DALC under the GI's rule,
 * outdoors the weather's DALC x what the sky grid leaves), the sun x its cascades and terrain map. A miss is the
 * ambient for that direction (the sky). Linear radiance out (R11G11B10F).
 * lane SKYBELOW2: a miss below the horizon is the ground, never the sky (cell_skybelow.glsl; red WW_SKYBELOW_RED). */

#define WW_CELL_FX 1
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
#include "../shadows/ww_sunshadow.hlsli"
#ifndef WW_CELL_FX
#define WW_CELL_FX 1	// as below, before this file
#endif
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
#undef WW_LOOSE_TYPES
cbuffer LooseUniforms : register(b31, space0)
{
#define WW_LOOSE_UNIFORMS 1
#include "../air/lookdev_fog.hlsli"
#include "../shadows/ww_sunshadow.hlsli"
#ifndef WW_CELL_FX
#define WW_CELL_FX 1	// as below, before this file
#endif
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
#undef WW_LOOSE_UNIFORMS
	float rpLayer;				// the probe's G-buffer layer
	float3 rpTile;				// the tile's origin in the atlas (pixels), its size
	float3 rpPos;					// the probe, world
	row_major float4x4 rpViewFromWorld;		// for the cascades (they take view space)
	int rpRed;					// 1 nosun, 2 nolights, 4 nogi, 8 white (albedo 1: the light alone)
	int rpSkyBelow;				// lane SKYBELOW2: 1 a miss below the horizon is ground, 0 the red (the sky ambient)
};
#include "../air/lookdev_fog.hlsli"
#include "../shadows/ww_sunshadow.hlsli"
#include "../lights/cell_lights.hlsli"
#include "../sky/cell_skybelow.hlsli"

TextureCubeArray<float4> rpAlb : register(t46, space0); SamplerState _rpAlb_sampler : register(s46, space0);		// rgb albedo (before its square), a 1 hit / 0 sky
TextureCubeArray<float4> rpNd : register(t47, space0); SamplerState _rpNd_sampler : register(s47, space0);		// xyz world normal, w distance to the probe

static float4 fragColor;

float3 rpOctDecode( float2 uv )
{
	float2 f = uv * 2.0 - 1.0;
	float3 n = float3( f, 1.0 - abs( f.x ) - abs( f.y ) );
	float t = max( -n.z, 0.0 );
	n.x += n.x >= 0.0 ? -t : t;
	n.y += n.y >= 0.0 ? -t : t;
	return normalize( n );
}

float3 rpAmbient( float3 N, float3 P )
{
	if ( cellInterior )
		return cellHasDalc ? cellAmbient( N, P ) : (float3)( 0.0 );
	float3 A = sqrt( max( lightSourceAmbient.rgb, (float3)( 0.0 ) ) ) * 0.375;	// fo4_default.vert's flat ambient
	return cellExtDalc ? cellAmbient( N, P ) : A * A;
}

void frag_main()
{
	if ( !cellOn ) {	// never: the setters ran; it keeps cellOn live (wwCellLightsUniforms returns early without it)
		fragColor = (float4)( 0.0 );
		return;
	}
	float2 uv = ( gl_FragCoord.xy - rpTile.xy ) / rpTile.z;
	float3 dir = rpOctDecode( uv );
	float4 a = rpAlb.Sample( _rpAlb_sampler, float4( dir, rpLayer ) );
	if ( a.a < 0.5 ) {	// the sky (or, indoors, the void past the walls); below the horizon the ground (SKYBELOW2)
		float3 gs = (float3)( 0.0 );
		float gn = 0.0;
		if ( rpSkyBelow != 0 && !cellInterior && wwSkyBelow( dir ) ) {
			for ( int k = 0; k < 9; k++ ) {	// the ground's albedo: this probe's own below-horizon hits
				float4 g = rpAlb.SampleLevel( _rpAlb_sampler, float4( wwSkyBelowTap( k ), rpLayer ), 0.0 );	// in a branch: no derivatives
				if ( g.a >= 0.5 ) {
					gs += g.rgb * g.rgb;
					gn += 1.0;
				}
			}
		}
		fragColor = float4( wwSkyBelowMiss( dir, rpAmbient( dir, rpPos ), wwSkyBelowAlbedo( gs, gn ),
			rpAmbient( float3( 0.0, 0.0, 1.0 ), rpPos ), cellInterior, rpSkyBelow ), 1.0 );
		return;
	}
	float4 nd = rpNd.Sample( _rpNd_sampler, float4( dir, rpLayer ) );
	float3 N = normalize( nd.xyz );
	float3 P = rpPos + dir * nd.w + N * 2.0;	// a hair off the surface (the shadow lookups' own bias does the rest)
	float3 E = (float3)( 0.0 );
	if ( ( rpRed & 2 ) == 0 ) {
		for ( int i = 0; i < cellLightCount; i++ ) {
			float3 L;
			bool noSpec;
			float3 e = cellLightE( i, P, N, L, noSpec );
			E += e * max( dot( N, L ), 0.0 );
		}
	}
	float3 gi = ( cellGiOn && ( rpRed & 4 ) == 0 ) ? cellGiE( P, N ) * 0.31830989 : (float3)( 0.0 );
	E += gi;
	float3 amb = rpAmbient( N, P );
	if ( cellInterior ) {
		if ( cellHasDalc && cellGiAmb != 0 )
			amb = cellAmbGi( amb, gi, P, N );
	} else {
		amb *= 1.0 - cellGiSkyK( P, N );
	}
	E += amb;
	if ( cellHasDir && ( rpRed & 1 ) == 0 ) {
		float3 Ld = normalize( cellDirTo );
		float vis = cellInterior ? 1.0 : wwSunShadow( ( mul(float4( P, 1.0 ), rpViewFromWorld) ).xyz );
		E += cellDirColor * max( dot( N, Ld ), 0.0 ) * vis;
	}
	float3 alb = ( rpRed & 8 ) != 0 ? (float3)( 1.0 ) : a.rgb * a.rgb;
	fragColor = float4( max( alb * E, (float3)( 0.0 ) ), 1.0 );
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
