#version 410 core

/* lane REFLPROBE1: the G-buffer texel. Target 0: the albedo as the colour pass takes it (base map x vertex colour,
 * before its square), a = 1 (a hit; the clear leaves 0 = the sky). Target 1: the world normal turned to face the
 * probe, and the distance to it. The alpha test is the colour pass's (base alpha x material alpha vs threshold). */

uniform sampler2D BaseMap;
uniform bool rpHasBase;
uniform bool rpHasVc;
uniform bool rpAlphaTest;
uniform float alpha;
uniform float alphaThreshold;

in vec3 rpRel;
in vec3 rpN;
in vec4 rpC;
in vec2 rpUv;

layout ( location = 0 ) out vec4 rpAlbedo;
layout ( location = 1 ) out vec4 rpNormalDist;

void main()
{
	vec4 b = rpHasBase ? texture( BaseMap, rpUv ) : vec4( 0.5, 0.5, 0.5, 1.0 );
	if ( rpAlphaTest && b.a * alpha < alphaThreshold )
		discard;
	vec3 c = rpHasVc ? rpC.rgb : vec3( 1.0 );
	vec3 n = normalize( rpN );
	if ( dot( n, rpRel ) > 0.0 )
		n = -n;	// a two-sided or flipped face: the side the probe sees
	rpAlbedo = vec4( clamp( b.rgb * c, 0.0, 1.0 ), 1.0 );
	rpNormalDist = vec4( n, length( rpRel ) );
}
