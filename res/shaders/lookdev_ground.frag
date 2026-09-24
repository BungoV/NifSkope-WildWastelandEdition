#version 410 core

/* Lookdev ground plane (lane PBRR2B, docs s6.2 stage 1). Vanilla Commonwealth
 * ground (textures\landscape\Ground\CommonwealthDefault01_d/_n), tiled every
 * `tileSize` world units, lit by the lookdev sun (Lambert, linear units: a white
 * surface facing a sun of L reads L) and the DALC 6-axis ambient. Out through
 * the Studio output path. The _d is a legacy-header DXT5 (UNORM), decoded here. */

#include "uniforms.glsl"

uniform sampler2D BaseMap;
uniform sampler2D NormalMap;
uniform bool hasBaseMap;
uniform bool hasNormalMap;
uniform float tileSize;
uniform vec3 sunDirWorld;	// unit, TO the light
uniform vec3 sunLinear;
uniform vec3 dalc[6];		// linear: X+ X- Y+ Y- Z+ Z-
uniform bool dalcFlip;
uniform float groundLeak;	// < 0: normal draw; >= 0: red "groundleak" (the OFF path drawing a faint ground)
uniform float sceneExposure;
uniform int viewTransform;

in vec3 worldPos;
in vec3 viewPos;

out vec4 fragColor;

#include "lookdev_output.glsl"
#include "lookdev_fog.glsl"
#ifdef WW_SUNSHADOW
#include "ww_sunshadow.glsl"
#endif

vec3 dalcAmbient( vec3 n )
{
	// the axis name is the light's TRAVEL direction (assumption, owed to Todd's treat):
	// an up-facing normal takes the Z- colour
	vec3 n2 = n * n;
	bool fx = dalcFlip;
	vec3 x = ( ( n.x > 0.0 ) != fx ) ? dalc[1] : dalc[0];
	vec3 y = ( ( n.y > 0.0 ) != fx ) ? dalc[3] : dalc[2];
	vec3 z = ( ( n.z > 0.0 ) != fx ) ? dalc[5] : dalc[4];
	return n2.x * x + n2.y * y + n2.z * z;
}

void main()
{
	vec2 uv = vec2( worldPos.x, -worldPos.y ) / tileSize;
	vec3 base = vec3( 0.18 );
	if ( hasBaseMap )
		base = srgbToLinear( texture( BaseMap, uv ).rgb );
	vec3 n = vec3( 0.0, 0.0, 1.0 );
	if ( hasNormalMap ) {
		vec2 g = texture( NormalMap, uv ).rg * 2.0 - 1.0;
		// DirectX-style green (+G = image down = world -Y here)
		n = normalize( vec3( g.x, -g.y, sqrt( max( 1.0 - dot( g, g ), 0.0 ) ) ) );
	}
	float NdotL = max( dot( n, sunDirWorld ), 0.0 );
#ifdef WW_SUNSHADOW
	// lane CSM1 (spec 2.8): the cascade factor on the sun term only
	vec3 color = base * sunLinear * NdotL * wwSunShadow( viewPos ) + base * dalcAmbient( n );
#else
	vec3 color = base * sunLinear * NdotL + base * dalcAmbient( n );
#endif
	color = wwFog( color, viewPos );	// lane FOG1: linear, before the exposure
	if ( groundLeak >= 0.0 )
		color = mix( vec3( 0.0 ), color, groundLeak );
	fragColor = vec4( studioOutput( color ), 1.0 );
	if ( groundLeak >= 0.0 )
		fragColor.a = groundLeak;
	vec3 probe;
	if ( wwFogProbe( viewPos, probe ) )
		fragColor = vec4( probe, 1.0 );
#ifdef WW_SUNSHADOW
	vec3 csmProbeOut;
	if ( wwSunShadowProbe( viewPos, fragColor.rgb, csmProbeOut ) )
		fragColor = vec4( csmProbeOut, 1.0 );
#endif
}
