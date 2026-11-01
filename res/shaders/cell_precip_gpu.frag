#version 410 core

/* lane PERFFIX3 D (2026-11-01): a precipitation particle in the cell view's linear frame, lit per PARTICLE
 * (cell_precip_gpu.geom: gA, gB). Per fragment only the texture, the opacity and the near-eye fade remain:
 * colour = albedo x gA + gB (= wwFogFx( albedo x lit ), the fog being affine in the colour). Alpha blended, no depth
 * write (the scene's depth tests it). */

uniform sampler2D pcTex;
uniform bool pcHasTex;
uniform int pcFar;
uniform vec4 pcAtlas;		// cell uv size, the deepest mip a near quad reads, the one-texel-a-cell mip
uniform vec2 pcNear;		// view units from the eye: gone at x, whole at y (a flake never covers the lens)

in vec2 gUv;
in vec2 gQ;
in vec3 gView;
flat in vec4 gCol;
flat in vec4 gBox;
flat in vec3 gA;
flat in vec3 gB;

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
	fragColor = vec4( max( alb * gA + gB, vec3( 0.0 ) ), a );
}
