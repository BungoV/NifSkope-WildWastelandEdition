"""TERRLIVE1 law 2: splice the vanilla blend into src/terrainpreview.cpp (run once from the worktree root)."""
p = 'src/terrainpreview.cpp'
s = open(p, 'rb').read().decode('utf-8')
assert '\r' not in s


def rep(a, b, n=1):
    global s
    assert s.count(a) == n, (a[:70], s.count(a))
    s = s.replace(a, b)


blend = open('scratchpad/terrlive1_20260929/edge/blend.cpp.txt', 'rb').read().decode('utf-8').replace('\r', '')
rep('struct Camera\n', blend + 'struct Camera\n')
rep('#include "io/lodvfile.h"\n', '#include "io/lodvfile.h"\n\n#include "ddstxt16.hpp"\n')

# ---- shader
rep('''uniform int uOption;
uniform vec2 uFade;
uniform vec3 uEye;

// THE ANTI-REPEAT HOOK''', '''uniform int uOption;
uniform vec2 uFade;
uniform vec3 uEye;
// law 2 (lane TERRLIVE1): vanilla's dim-4 diffuse and our weight against it
uniform sampler2D uVan;
uniform sampler2D uBlendW;
uniform vec4 uVanMap;
uniform vec4 uBlendMap;
uniform int uLaw2;

// THE ANTI-REPEAT HOOK''')
rep('''vec3 baked( sampler2D s, vec4 m, vec2 wdx, vec2 wdy )
{
	vec2 uv = vec2( ( vWorld.x - m.x ) * m.z, ( m.y - vWorld.y ) * m.w );
	return textureGrad( s, uv, vec2( wdx.x * m.z, -wdx.y * m.w ), vec2( wdy.x * m.z, -wdy.y * m.w ) ).rgb;
}
''', '''vec4 baked4( sampler2D s, vec4 m, vec2 wdx, vec2 wdy )
{
	vec2 uv = vec2( ( vWorld.x - m.x ) * m.z, ( m.y - vWorld.y ) * m.w );
	return textureGrad( s, uv, vec2( wdx.x * m.z, -wdx.y * m.w ), vec2( wdy.x * m.z, -wdy.y * m.w ) );
}

vec3 baked( sampler2D s, vec4 m, vec2 wdx, vec2 wdy )
{
	return baked4( s, m, wdx, wdy ).rgb;
}

// our weight against vanilla (0 = vanilla untouched, 1 = ours); a texel with no vanilla sheet stays ours
float oursWeight( vec2 wdx, vec2 wdy, out vec3 V )
{
	V = vec3( 0.0 );
	if ( uLaw2 == 0 )
		return 1.0;
	vec4 v = baked4( uVan, uVanMap, wdx, wdy );
	V = v.rgb;
	float w = textureLod( uBlendW, vec2( ( vWorld.x - uBlendMap.x ) * uBlendMap.z, ( uBlendMap.y - vWorld.y ) * uBlendMap.w ), 0.0 ).r;
	return 1.0 - ( 1.0 - w ) * v.a;
}
''')
rep('''		vec3 L = vec3( 0.0 ), B = vec3( 0.0 );
		if ( wl > 0.0 )
			L = splat( wdx, wdy );''', '''		vec3 L = vec3( 0.0 ), B = vec3( 0.0 );
		if ( wl > 0.0 ) {
			vec3 V;
			float wo = oursWeight( wdx, wdy, V );
			L = wo > 0.0 ? splat( wdx, wdy ) : vec3( 0.0 );
			L = mix( V, L, wo );
		}''')

# ---- build the blend per view, after the baked levels
rep('''		say( QString( "view %1: sheets decoded in %2 ms" ).arg( vname ).arg( tm.elapsed() ) );
''', '''		say( QString( "view %1: sheets decoded in %2 ms" ).arg( vname ).arg( tm.elapsed() ) );

		// law 2: the blend to vanilla's diffuse, unless the spec turns it off (a "before" picture)
		Blend blend;
		bool haveBlend = false;
		if ( V.value( "blend" ).toBool( true ) && !lodgenVanillaLodRoot().isEmpty() ) {
			tm.restart();
			haveBlend = buildBlend( gl, L, cells.at( 0 ).toInt(), cells.at( 1 ).toInt(), cells.at( 2 ).toInt(),
				cells.at( 3 ).toInt(), spec.value( "ws" ).toString( QStringLiteral( "Commonwealth" ) ), blend, &why );
			if ( haveBlend )
				say( QString( "view %1: blend to vanilla: band %2 u, weight map %3 x %4 at %5 u, painted quadrants %6; "
						"vanilla dim-4 diffuse read live: %7 sheet(s), %8 bytes (game files, not shipped), %9 missing, "
						"mip %10; built in %11 ms" )
					.arg( vname ).arg( double( LODGEN_VT_FILL_BAND ) ).arg( blend.wn ).arg( blend.hn ).arg( double( blend.res ) )
					.arg( blend.paintedQuads ).arg( blend.vanSheets ).arg( blend.vanBytes ).arg( blend.vanMissing )
					.arg( blend.vanMip ).arg( tm.elapsed() ) );
			else
				say( QString( "view %1: blend to vanilla: REFUSED: %2" ).arg( vname, why ) );
		} else
			say( QString( "view %1: blend to vanilla: off (%2)" ).arg( vname,
				lodgenVanillaLodRoot().isEmpty() ? QStringLiteral( "no --vanilla-lod-root" ) : QStringLiteral( "the spec's \\"blend\\": false" ) ) );
''')
rep('''			bindTex( pTerrain, "uFar", 6, GL_TEXTURE_2D, texFar );
''', '''			bindTex( pTerrain, "uFar", 6, GL_TEXTURE_2D, texFar );
			gl->glUniform1i( U( gl, pTerrain, "uLaw2" ), haveBlend ? 1 : 0 );
			gl->glUniform4fv( U( gl, pTerrain, "uVanMap" ), 1, blend.vMap );
			gl->glUniform4fv( U( gl, pTerrain, "uBlendMap" ), 1, blend.wMap );
			bindTex( pTerrain, "uVan", 7, GL_TEXTURE_2D, blend.vTex );
			bindTex( pTerrain, "uBlendW", 8, GL_TEXTURE_2D, blend.wTex );
''')

# ---- the crossover counts only ground that is wholly ours (w = 1): the band is vanilla's, not a baked-vs-live error
rep('''		auto distances = [&]() {''', '''		auto distances = [&]( bool oursOnly ) {''')
rep('''					// image rows run top-down after the mirror
					out[size_t( H - 1 - y ) * W + x] = ( w.toVector3D() / w.w() - cam.eye ).length();''', '''					const QVector3D wp = w.toVector3D() / w.w();
					if ( oursOnly && haveBlend && blend.weightAt( wp.x(), wp.y() ) < 0.999f )
						continue;
					// image rows run top-down after the mirror
					out[size_t( H - 1 - y ) * W + x] = ( wp - cam.eye ).length();''')
open(p, 'wb').write(s.encode('utf-8'))
print('distances() callers to update:', s.count('distances()'))
