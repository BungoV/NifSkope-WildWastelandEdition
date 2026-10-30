#version 410 core

/* lane WEATHERLOOK1 phase B: a precipitation particle in the cell view's linear frame (src/gl/cellprecip.cpp).
 * Lit by the volumetric particles' law (cell_volpart.glsl vpLight, VOLPART1): the sun x its cascades, every placed light
 * x the air to it, the GI and the ambient, through the air's phase. A flake faces the eye (flat-on); its thin share
 * (translucency, airpart_data.md: dry snow 0.6, wet 0.3) scatters through the phase, so a flake between the eye and a
 * lamp glows in the lamp's halo; the rest is the wrapped diffuse at its face. Then seen through the air (wwFogFx), so
 * far flakes fade into the fog. Alpha blended; no depth write (the scene's depth tests it). */

#define WW_CELLLIGHTS 1
#include "uniforms.glsl"
#include "lookdev_fog.glsl"
#include "cell_lights.glsl"
uniform float lightingInfluence;	// cell_volpart.glsl's vpLit reads them (not used here)
uniform int fxLitMode;
#include "cell_volpart.glsl"

uniform sampler2D pcTex;
uniform bool pcHasTex;
uniform int pcFar;
uniform vec4 pcAtlas;		// cell uv size, the deepest mip a near quad reads, the one-texel-a-cell mip
uniform float pcTrans;		// the thin (scattering) share
uniform vec2 pcNear;		// view units from the eye: gone at x, whole at y (a flake never covers the lens)

in vec2 gUv;
in vec2 gQ;
in vec3 gView;
flat in vec4 gCol;
flat in vec4 gBox;

out vec4 fragColor;

void main()
{
	vec4 t;
	if ( pcFar != 0 ) {
		// a far dot: the cell's mean (gCol) over a soft round footprint whose mean over the quad is 1
		t = vec4( 1.0, 1.0, 1.0, exp( -2.5 * dot( gQ, gQ ) ) / 0.2985 );
	} else if ( pcHasTex ) {
		float lod = min( textureQueryLod( pcTex, gUv ).x, pcAtlas.z );
		t = textureLod( pcTex, clamp( gUv, gBox.xy, gBox.zw ), lod );
	} else {
		t = vec4( 1.0, 1.0, 1.0, clamp( 1.0 - dot( gQ, gQ ), 0.0, 1.0 ) );
	}
	float a = clamp( t.a * gCol.a, 0.0, 1.0 ) * smoothstep( pcNear.x, pcNear.y, -gView.z );
	if ( a < 1.0 / 512.0 )
		discard;
	vec3 alb = t.rgb * gCol.rgb;	// linear: the sRGB texture decodes on read
	vec3 lit = vec3( 1.0 );
	if ( cellInterior || cellHasDir ) {
		vec3 thin, thick;
		vpLight( gView, cellWorldPos( gView ), cellWorldDir( gView ), dot( fogView.xyz, gView ) + fogView.w,
			cellWorldDir( vec3( 0.0, 0.0, 1.0 ) ), pcTrans, thin, thick );
		lit = pcTrans * thin + ( 1.0 - pcTrans ) * thick;
	}
	fragColor = vec4( wwFogFx( alb * lit, gView, false ), a );
}
