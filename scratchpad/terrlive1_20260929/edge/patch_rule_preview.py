"""Lane TERRLIVE1 rule paint: src/terrainpreview.cpp (sources are LF)."""
import sys

P = 'E:/Projects/NifskopeWWE-terrlive1/src/terrainpreview.cpp'
s = open(P, 'rb').read().decode('utf-8')
assert '\r\n' not in s
pairs = [
# shader uniforms
(r'''uniform vec4 uBlendMap;
uniform int uLaw2;
''', r'''uniform vec4 uBlendMap;
uniform int uLaw2;
// the rule paint outside (lane TERRLIVE1, `--outside-paint rule`): the .lodr's ids and weights
uniform usampler2D uRIdx;
uniform sampler2D uRW;
uniform vec2 uROrigin;
uniform float uRSpacing;
uniform ivec2 uRGrid;
uniform int uRule;
'''),
# the splat, fed either grid
(r'''vec3 splat( vec2 wdx, vec2 wdy )
{
	vec2 g = ( vWorld.xy - uOrigin ) / uSpacing;
	vec2 g0 = floor( g );
	vec2 f = g - g0;
	vec2 uv = vWorld.xy / uTile;
	vec2 dx = wdx / uTile, dy = wdy / uTile;
	vec3 acc = vec3( 0.0 );
	for ( int c = 0; c < 4; c++ ) {
		ivec2 o = ivec2( c & 1, c >> 1 );
		ivec2 p = clamp( ivec2( g0 ) + o, ivec2( 0 ), uGrid - 1 );
		float bw = ( o.x == 1 ? f.x : 1.0 - f.x ) * ( o.y == 1 ? f.y : 1.0 - f.y );
		if ( bw <= 0.0 )
			continue;
		uvec4 id = texelFetch( uIdx, p, 0 );
		vec4 w = texelFetch( uW, p, 0 );
		for ( int k = 0; k < 4; k++ ) {
			if ( w[k] <= 0.0 )
				continue;
			vec3 t = id[k] == 65535u ? vec3( 0.5 ) : ltexFetch( float( id[k] ), uv, dx, dy );
			acc += bw * w[k] * t;
		}
	}
	vec3 vc = textureLod( uVclr, ( g + 0.5 ) / vec2( uGrid ), 0.0 ).rgb;
	return acc * vc;
}
''', r'''// one splat over an id/weight grid (the .lodl's, or the rule map's); `cover` = the
// bilinear weight of the samples that hold any texture
vec3 splatGrid( usampler2D tIdx, sampler2D tW, vec2 origin, float spacing, ivec2 grid, vec2 wdx, vec2 wdy,
	out vec2 g, out float cover )
{
	g = ( vWorld.xy - origin ) / spacing;
	vec2 g0 = floor( g );
	vec2 f = g - g0;
	vec2 uv = vWorld.xy / uTile;
	vec2 dx = wdx / uTile, dy = wdy / uTile;
	vec3 acc = vec3( 0.0 );
	cover = 0.0;
	for ( int c = 0; c < 4; c++ ) {
		ivec2 o = ivec2( c & 1, c >> 1 );
		ivec2 p = clamp( ivec2( g0 ) + o, ivec2( 0 ), grid - 1 );
		float bw = ( o.x == 1 ? f.x : 1.0 - f.x ) * ( o.y == 1 ? f.y : 1.0 - f.y );
		if ( bw <= 0.0 )
			continue;
		uvec4 id = texelFetch( tIdx, p, 0 );
		vec4 w = texelFetch( tW, p, 0 );
		if ( id.x != 65535u )
			cover += bw;
		for ( int k = 0; k < 4; k++ ) {
			if ( w[k] <= 0.0 )
				continue;
			vec3 t = id[k] == 65535u ? vec3( 0.5 ) : ltexFetch( float( id[k] ), uv, dx, dy );
			acc += bw * w[k] * t;
		}
	}
	return acc;
}

vec3 splat( vec2 wdx, vec2 wdy )
{
	vec2 g;
	float cover;
	vec3 acc = splatGrid( uIdx, uW, uOrigin, uSpacing, uGrid, wdx, wdy, g, cover );
	vec3 vc = textureLod( uVclr, ( g + 0.5 ) / vec2( uGrid ), 0.0 ).rgb;
	return acc * vc;
}

// the rule paint: the map's textures through the same hook, no vertex colour (the bake's rule);
// false where no rule sample reaches
bool ruleSplat( vec2 wdx, vec2 wdy, out vec3 R )
{
	vec2 g;
	float cover;
	R = splatGrid( uRIdx, uRW, uROrigin, uRSpacing, uRGrid, wdx, wdy, g, cover );
	if ( cover <= 0.0 )
		return false;
	R /= cover;
	return true;
}
'''),
# oursWeight: expose the band weight
(r'''float oursWeight( vec2 wdx, vec2 wdy, out vec3 V )
{
	V = vec3( 0.0 );
	if ( uLaw2 == 0 )
		return 1.0;
	vec4 v = baked4( uVan, uVanMap, wdx, wdy );
	V = v.rgb;
	float w = textureLod( uBlendW, vec2( ( vWorld.x - uBlendMap.x ) * uBlendMap.z, ( uBlendMap.y - vWorld.y ) * uBlendMap.w ), 0.0 ).r;
	return 1.0 - ( 1.0 - w ) * v.a;
}
''', r'''float oursWeight( vec2 wdx, vec2 wdy, out vec3 V )
{
	V = vec3( 0.0 );
	if ( uLaw2 == 0 )
		return 1.0;
	vec4 v = baked4( uVan, uVanMap, wdx, wdy );
	V = v.rgb;
	float w = textureLod( uBlendW, vec2( ( vWorld.x - uBlendMap.x ) * uBlendMap.z, ( uBlendMap.y - vWorld.y ) * uBlendMap.w ), 0.0 ).r;
	// the rule paint takes vanilla's place in the same band, where it has a sample
	if ( uRule == 1 && w < 1.0 ) {
		vec3 R;
		if ( ruleSplat( wdx, wdy, R ) ) {
			V = R;
			return w;
		}
	}
	return 1.0 - ( 1.0 - w ) * v.a;
}
'''),
# host: the spec's rule map, read before the LTEX array so its textures get layers
(r'''	/* ---- the LTEX array: every record the .lodl names, one layer each ---- */
	QElapsedTimer tm;
	tm.start();
	const int side = qBound( 64, spec.value( "ltex_side" ).toInt( 512 ), 2048 );
	// one layer per .lodl LTEX, plus the engine default land set (esmdata.h) as the last one
	const int nl = L.ltexCount() + 1;
''', r'''	/* ---- the rule paint map (lane TERRLIVE1): the spec's "rule", a .lodr ---- */
	LodgenRuleMap rule;
	bool haveRule = false;
	QVector<quint16> ruleLayer;         // palette index -> LTEX array layer
	QVector<quint32> ltexExtra;         // rule textures the .lodl does not name, as extra layers
	if ( !spec.value( "rule" ).toString().isEmpty() ) {
		if ( !lodgenRuleRead( spec.value( "rule" ).toString(), rule, &why ) )
			return fail( QStringLiteral( ".lodr: " ) + why );
		haveRule = true;
		for ( quint32 form : rule.palette ) {
			int layer = -1;
			if ( form == ESM_LTEX_ENGINE_DEFAULT )
				layer = L.ltexCount();
			for ( int i = 0; i < L.ltexCount() && layer < 0; i++ )
				if ( L.ltexForm( i ) == form )
					layer = i;
			if ( layer < 0 ) {
				int e = ltexExtra.indexOf( form );
				if ( e < 0 ) {
					e = ltexExtra.size();
					ltexExtra.append( form );
				}
				layer = L.ltexCount() + 1 + e;
			}
			ruleLayer.append( quint16( layer ) );
		}
		say( QString( "rule: %1 x %2 samples at %3 u, palette %4 (%5 not in the .lodl, added as layers)" )
			.arg( rule.nx() ).arg( rule.ny() ).arg( double( rule.spacing() ) ).arg( rule.palette.size() )
			.arg( ltexExtra.size() ) );
	}

	/* ---- the LTEX array: every record the .lodl names, one layer each ---- */
	QElapsedTimer tm;
	tm.start();
	const int side = qBound( 64, spec.value( "ltex_side" ).toInt( 512 ), 2048 );
	// one layer per .lodl LTEX, plus the engine default land set (esmdata.h), plus the rule's extras
	const int nl = L.ltexCount() + 1 + ltexExtra.size();
'''),
(r'''			const quint32 form = i < L.ltexCount() ? L.ltexForm( i ) : ESM_LTEX_ENGINE_DEFAULT;
''', r'''			const quint32 form = i < L.ltexCount() ? L.ltexForm( i )
				: ( i == L.ltexCount() ? ESM_LTEX_ENGINE_DEFAULT : ltexExtra.at( i - L.ltexCount() - 1 ) );
'''),
# the rule textures, once
(r'''	drainErrors( gl, QStringLiteral( "the LTEX array" ) );
''', r'''	drainErrors( gl, QStringLiteral( "the LTEX array" ) );
	GLuint ruleIdx = 0, ruleW = 0;
	if ( haveRule ) {
		const int rnx = rule.nx(), rny = rule.ny();
		std::vector<quint16> ri( size_t( rnx ) * rny * 4, 0xFFFFu );
		std::vector<quint8> rw( size_t( rnx ) * rny * 4, 0 );
		for ( size_t s = 0; s < size_t( rnx ) * rny; s++ ) {
			if ( rule.a[s] == 0xFF )
				continue;
			ri[s * 4 + 0] = ruleLayer.at( rule.a[s] );
			ri[s * 4 + 1] = ruleLayer.at( rule.b[s] );
			rw[s * 4 + 0] = rule.w[s];
			rw[s * 4 + 1] = quint8( 255 - rule.w[s] );
		}
		auto mk = [&]( GLenum ifmt, GLenum fmt, GLenum type, const void * data ) {
			GLuint t = 0;
			gl->glGenTextures( 1, &t );
			gl->glBindTexture( GL_TEXTURE_2D, t );
			gl->glPixelStorei( GL_UNPACK_ALIGNMENT, 1 );
			gl->glTexImage2D( GL_TEXTURE_2D, 0, ifmt, rnx, rny, 0, fmt, type, data );
			gl->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
			gl->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
			gl->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
			gl->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
			return t;
		};
		ruleIdx = mk( GL_RGBA16UI, GL_RGBA_INTEGER, GL_UNSIGNED_SHORT, ri.data() );
		ruleW = mk( GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, rw.data() );
		drainErrors( gl, QStringLiteral( "the rule map" ) );
	}
'''),
# per-draw uniforms
(r'''			bindTex( pTerrain, "uBlendW", 8, GL_TEXTURE_2D, blend.wTex );
''', r'''			bindTex( pTerrain, "uBlendW", 8, GL_TEXTURE_2D, blend.wTex );
			// the rule paint: on when the spec has a map and the view does not say "rule": false
			const bool ruleView = haveRule && haveBlend && V.value( "rule" ).toBool( true );
			gl->glUniform1i( U( gl, pTerrain, "uRule" ), ruleView ? 1 : 0 );
			gl->glUniform2f( U( gl, pTerrain, "uROrigin" ), rule.originX(), rule.originY() );
			gl->glUniform1f( U( gl, pTerrain, "uRSpacing" ), rule.spacing() );
			gl->glUniform2i( U( gl, pTerrain, "uRGrid" ), qMax( 1, rule.nx() ), qMax( 1, rule.ny() ) );
			bindTex( pTerrain, "uRIdx", 9, GL_TEXTURE_2D, ruleIdx );
			bindTex( pTerrain, "uRW", 10, GL_TEXTURE_2D, ruleW );
'''),
(r'''	ctx.doneCurrent();
	say( failures ? QString( "%1 failure(s)" ).arg( failures ) : QStringLiteral( "done, no failures" ) );''',
 r'''	if ( haveRule ) {
		GLuint rt[2] = { ruleIdx, ruleW };
		gl->glDeleteTextures( 2, rt );
	}
	ctx.doneCurrent();
	say( failures ? QString( "%1 failure(s)" ).arg( failures ) : QStringLiteral( "done, no failures" ) );'''),
]
for a, b in pairs:
    if s.count(a) != 1:
        sys.exit('count %d for %r' % (s.count(a), a[:70]))
    s = s.replace(a, b)
open(P, 'wb').write(s.encode('utf-8'))
print('patched')
