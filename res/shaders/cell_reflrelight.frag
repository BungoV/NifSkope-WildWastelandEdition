#version 410 core

/* lane REFLPROBE1 (src/gl/cellreflprobe.h): THE RELIGHT. Per texel of a probe's octahedral tile: the direction, the
 * G-buffer cube's hit there (albedo, world normal, distance), lit by the same code the surfaces take
 * (cell_lights.glsl): the placed lights with their shadow cubes, the probe GI (its bounce, and outdoors its sky:
 * the sky visibility stands in for RDR2's baked AO cube), the ambient (indoors the DALC under the GI's rule,
 * outdoors the weather's DALC x what the sky grid leaves), the sun x its cascades and terrain map. A miss is the
 * ambient for that direction (the sky). Linear radiance out (R11G11B10F).
 * lane SKYBELOW2: a miss below the horizon is the ground, never the sky (cell_skybelow.glsl; red WW_SKYBELOW_RED). */

#define WW_CELL_FX 1
#include "uniforms.glsl"
#include "lookdev_fog.glsl"
#include "ww_sunshadow.glsl"
#include "cell_lights.glsl"
#include "cell_skybelow.glsl"

uniform samplerCubeArray rpAlb;		// rgb albedo (before its square), a 1 hit / 0 sky
uniform samplerCubeArray rpNd;		// xyz world normal, w distance to the probe
uniform float rpLayer;				// the probe's G-buffer layer
uniform vec3 rpTile;				// the tile's origin in the atlas (pixels), its size
uniform vec3 rpPos;					// the probe, world
uniform mat4 rpViewFromWorld;		// for the cascades (they take view space)
uniform int rpRed;					// 1 nosun, 2 nolights, 4 nogi, 8 white (albedo 1: the light alone)
uniform int rpSkyBelow;				// lane SKYBELOW2: 1 a miss below the horizon is ground, 0 the red (the sky ambient)

out vec4 fragColor;

vec3 rpOctDecode( vec2 uv )
{
	vec2 f = uv * 2.0 - 1.0;
	vec3 n = vec3( f, 1.0 - abs( f.x ) - abs( f.y ) );
	float t = max( -n.z, 0.0 );
	n.x += n.x >= 0.0 ? -t : t;
	n.y += n.y >= 0.0 ? -t : t;
	return normalize( n );
}

vec3 rpAmbient( vec3 N, vec3 P )
{
	if ( cellInterior )
		return cellHasDalc ? cellAmbient( N, P ) : vec3( 0.0 );
	vec3 A = sqrt( max( lightSourceAmbient.rgb, vec3( 0.0 ) ) ) * 0.375;	// fo4_default.vert's flat ambient
	return cellExtDalc ? cellAmbient( N, P ) : A * A;
}

void main()
{
	if ( !cellOn ) {	// never: the setters ran; it keeps cellOn live (wwCellLightsUniforms returns early without it)
		fragColor = vec4( 0.0 );
		return;
	}
	vec2 uv = ( gl_FragCoord.xy - rpTile.xy ) / rpTile.z;
	vec3 dir = rpOctDecode( uv );
	vec4 a = texture( rpAlb, vec4( dir, rpLayer ) );
	if ( a.a < 0.5 ) {	// the sky (or, indoors, the void past the walls); below the horizon the ground (SKYBELOW2)
		vec3 gs = vec3( 0.0 );
		float gn = 0.0;
		if ( rpSkyBelow != 0 && !cellInterior && wwSkyBelow( dir ) ) {
			for ( int k = 0; k < 9; k++ ) {	// the ground's albedo: this probe's own below-horizon hits
				vec4 g = textureLod( rpAlb, vec4( wwSkyBelowTap( k ), rpLayer ), 0.0 );	// in a branch: no derivatives
				if ( g.a >= 0.5 ) {
					gs += g.rgb * g.rgb;
					gn += 1.0;
				}
			}
		}
		fragColor = vec4( wwSkyBelowMiss( dir, rpAmbient( dir, rpPos ), wwSkyBelowAlbedo( gs, gn ),
			rpAmbient( vec3( 0.0, 0.0, 1.0 ), rpPos ), cellInterior, rpSkyBelow ), 1.0 );
		return;
	}
	vec4 nd = texture( rpNd, vec4( dir, rpLayer ) );
	vec3 N = normalize( nd.xyz );
	vec3 P = rpPos + dir * nd.w + N * 2.0;	// a hair off the surface (the shadow lookups' own bias does the rest)
	vec3 E = vec3( 0.0 );
	if ( ( rpRed & 2 ) == 0 ) {
		for ( int i = 0; i < cellLightCount; i++ ) {
			vec3 L;
			bool noSpec;
			vec3 e = cellLightE( i, P, N, L, noSpec );
			E += e * max( dot( N, L ), 0.0 );
		}
	}
	vec3 gi = ( cellGiOn && ( rpRed & 4 ) == 0 ) ? cellGiE( P, N ) * 0.31830989 : vec3( 0.0 );
	E += gi;
	vec3 amb = rpAmbient( N, P );
	if ( cellInterior ) {
		if ( cellHasDalc && cellGiAmb != 0 )
			amb = cellAmbGi( amb, gi, P, N );
	} else {
		amb *= 1.0 - cellGiSkyK( P, N );
	}
	E += amb;
	if ( cellHasDir && ( rpRed & 1 ) == 0 ) {
		vec3 Ld = normalize( cellDirTo );
		float vis = cellInterior ? 1.0 : wwSunShadow( ( rpViewFromWorld * vec4( P, 1.0 ) ).xyz );
		E += cellDirColor * max( dot( N, Ld ), 0.0 ) * vis;
	}
	vec3 alb = ( rpRed & 8 ) != 0 ? vec3( 1.0 ) : a.rgb * a.rgb;
	fragColor = vec4( max( alb * E, vec3( 0.0 ) ), 1.0 );
}
