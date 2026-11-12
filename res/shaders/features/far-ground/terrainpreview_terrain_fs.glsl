#version 430 core
in vec3 vWorld;
layout(location = 0) out vec4 oAlbedo;
layout(location = 1) out vec4 oNormal;
uniform sampler2D uHeight;
uniform usampler2D uIdx;
uniform sampler2D uW;
uniform sampler2D uVclr;
uniform sampler2DArray uLtex;
uniform sampler2D uFull;
uniform sampler2D uFar;
uniform vec2 uOrigin;
uniform float uSpacing;
uniform ivec2 uGrid;
uniform float uTile;
uniform vec4 uFullMap;
uniform vec4 uFarMap;
uniform int uOption;
uniform vec2 uFade;
uniform vec3 uEye;
// law 2 (lane TERRLIVE1): vanilla's dim-4 diffuse and our weight against it
uniform sampler2D uVan;
uniform sampler2D uBlendW;
uniform vec4 uVanMap;
uniform vec4 uBlendMap;
uniform int uLaw2;
// the rule paint outside (lane TERRLIVE1, `--outside-paint rule`): the .lodr's ids and weights
uniform usampler2D uRIdx;
uniform sampler2D uRW;
uniform sampler2D uRG;
uniform vec2 uROrigin;
uniform float uRSpacing;
uniform ivec2 uRGrid;
uniform int uRule;
// the AO map (lane TERRLIVE2, `<ws>.loda`): 32 u sky visibility over our painted ground, 1 elsewhere
uniform sampler2D uLoda;
uniform vec4 uLodaMap;
uniform int uLodaOn;

float lodaAo()
{
	if ( uLodaOn == 0 )
		return 1.0;
	return textureLod( uLoda, vec2( ( vWorld.x - uLodaMap.x ) * uLodaMap.z, ( uLodaMap.y - vWorld.y ) * uLodaMap.w ), 0.0 ).r;
}

// THE ANTI-REPEAT HOOK (lane TILING6 replaces this body): one LTEX layer at a
// world-tiled uv, with the caller's gradients.
vec3 ltexFetch( float layer, vec2 uv, vec2 dx, vec2 dy )
{
	return textureGrad( uLtex, vec3( uv, layer ), dx, dy ).rgb;
}

float liveWeight()
{
	if ( uOption == 2 || uOption == 3 )
		return 1.0;
	if ( uOption == 0 || uOption == 4 )
		return 0.0;
	float d = distance( vWorld, uEye );
	return 1.0 - smoothstep( uFade.x, uFade.y, d );
}

// one splat over an id/weight grid (the .lodl's, or the rule map's); `cover` = the
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

// the rule map's brightness gain (TERRLIVE2, .lodr v2 plane G, x128), bilinear over the samples
// that exist -- LodgenRuleMap::mixAt's weighting
float ruleGain( vec2 g )
{
	vec2 g0 = floor( g ), f = g - g0;
	float s = 0.0, t = 0.0;
	for ( int c = 0; c < 4; c++ ) {
		ivec2 o = ivec2( c & 1, c >> 1 );
		ivec2 p = clamp( ivec2( g0 ) + o, ivec2( 0 ), uRGrid - 1 );
		float bw = ( o.x == 1 ? f.x : 1.0 - f.x ) * ( o.y == 1 ? f.y : 1.0 - f.y );
		if ( bw <= 0.0 || texelFetch( uRIdx, p, 0 ).x == 65535u )
			continue;
		s += bw * texelFetch( uRG, p, 0 ).r;
		t += bw;
	}
	return t > 0.0 ? s / t * ( 255.0 / 128.0 ) : 1.0;
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
	R *= ruleGain( g );
	return true;
}

vec4 baked4( sampler2D s, vec4 m, vec2 wdx, vec2 wdy )
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

vec3 terrainNormal()
{
	vec2 g = ( vWorld.xy - uOrigin ) / uSpacing;
	vec2 e = 1.0 / vec2( uGrid );
	vec2 uvh = ( g + 0.5 ) * e;
	float hl = textureLod( uHeight, uvh - vec2( e.x, 0.0 ), 0.0 ).r;
	float hr = textureLod( uHeight, uvh + vec2( e.x, 0.0 ), 0.0 ).r;
	float hd = textureLod( uHeight, uvh - vec2( 0.0, e.y ), 0.0 ).r;
	float hu = textureLod( uHeight, uvh + vec2( 0.0, e.y ), 0.0 ).r;
	return normalize( vec3( ( hl - hr ) / ( 2.0 * uSpacing ), ( hd - hu ) / ( 2.0 * uSpacing ), 1.0 ) );
}

void main()
{
	vec2 wdx = dFdx( vWorld.xy ), wdy = dFdy( vWorld.xy );
	float wl = liveWeight();
	vec3 col;
	if ( uOption == 0 )
		col = baked( uFull, uFullMap, wdx, wdy );
	else if ( uOption == 4 )
		col = baked( uFar, uFarMap, wdx, wdy );
	else {
		vec3 L = vec3( 0.0 ), B = vec3( 0.0 );
		if ( wl > 0.0 ) {
			vec3 V;
			float wo = oursWeight( wdx, wdy, V );
			L = wo > 0.0 ? splat( wdx, wdy ) * lodaAo() : vec3( 0.0 );
			L = mix( V, L, wo );
		}
		if ( wl < 1.0 )
			B = baked( uFar, uFarMap, wdx, wdy );
		col = mix( B, L, wl );
	}
	oAlbedo = vec4( col, 1.0 );
	oNormal = vec4( terrainNormal() * 0.5 + 0.5, 1.0 );
}
