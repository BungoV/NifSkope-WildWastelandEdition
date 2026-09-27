#version 410 core

// The far-LOD view under a real sun and sky (lane LIT1, WW_LODL_LIT=1; src/gl/lodlit.h).
//
// NOT a second BRDF: pbrm_default.frag is included whole, its main() renamed out of the
// way, and this main() resolves a LOD surface into its `Surface` and lights it with its
// `directLight`, `dfgLazarov`, `multiScatter` and `specAlbedo`. pbrm_default.frag itself
// is not edited, so the PBR programs compile exactly what they compiled before.
//
//   terrain (lodLitSurface 1): colour sheet, `_msn` (R east, B north, G up, measured:
//            fo4_default.frag:394-403), mask sheet in slot 7 (R rough, G metal, B sky AO)
//   terrain without a mask (2): rough 1, metal 0, visibility 1
//   objects (0): diffuse, tangent `_n`, `_s` R = specular mask, G = gloss (fo4_default's
//            reading, :459-465); F0 0.04 x specColor, weight = mask x specStrength;
//            visibility = the vertex alpha when lodLitOccAlpha (.lodi AO x sky, per vertex)
//
// One sun (lodLitSun, lodLitSunE) and one sky (zenith/nadir mix by the world normal's up
// component). The visibility and the AO darken the SKY term only, never the sun.
// Result through the legacy Hable curve, as pbrm_default.frag's Legacy branch does.

#define WW_LODLIT 1
#define main wwPbrmMainUnused
#include "pbrm_default.frag"
#undef main

uniform sampler2D SpecularMap;
uniform sampler2D GlowMap;
uniform bool hasModelSpaceNormals;
uniform mat3 normalMatrix;			// model -> view, row-major, as fo4_default.frag reads it
uniform bool hasSpecularMap;
uniform float specStrength;
uniform float specGlossiness;
uniform vec3 specColor;
uniform bool hasEmit;
uniform bool hasGlowMap;
uniform vec3 glowColor;
uniform float glowMult;
uniform bool hasTintColor;
uniform vec3 tintColor;

in float rawVertexAlpha;

uniform int lodLitSurface;			// 0 object, 1 terrain with its mask, 2 terrain without one
uniform bool lodLitOccAlpha;		// the vertex alpha is the baked ambient visibility
uniform vec3 lodLitSun;				// WORLD direction to the sun, unit
uniform vec3 lodLitSunE;			// sun colour x intensity x PI (pbrm's sunE scale)
uniform vec3 lodLitSkyZenith;		// linear radiance
uniform vec3 lodLitSkyNadir;
uniform int lodLitTerm;				// 0 all, 1 sun, 2 sky, 3 specular
uniform int lodLitRed;				// 1 flipnorth, 2 flipup, 4 nospec, 8 sunmirror

vec3 lodLitSky( vec3 dirWorld )
{
	return mix( lodLitSkyNadir, lodLitSkyZenith, clamp( dirWorld.z * 0.5 + 0.5, 0.0, 1.0 ) );
}

void main()
{
	vec2 offset = texCoord.st * uvScale + uvOffset;
	vec4 baseMap = texture( BaseMap, offset );

	vec4 color = vec4( 1.0 );
	if ( alphaFlags > 0 ) {
		// fo4_default's test, except that the vertex alpha is visibility, not opacity, here
		float a = ( lodLitOccAlpha ? 1.0 : C.a ) * baseMap.a * alpha;
		int m = ( a < alphaThreshold ? 0x2B2B : ( a > alphaThreshold ? 0x7171 : 0x4D4D ) );
		if ( ( m & ( 1 << alphaFlags ) ) == 0 )
			discard;
		if ( ( alphaFlags & 8 ) != 0 )
			color.a = a;
	}

	// the legacy albedo (texture x vertex colour, display space), decoded once
	vec3 bd = baseIsSrgbTex ? linearToSrgb( baseMap.rgb ) : baseMap.rgb;
	vec3 base = srgbToLinear( clamp( bd * C.rgb, 0.0, 1.0 ) );
	if ( hasTintColor )
		base *= srgbToLinear( clamp( tintColor, 0.0, 1.0 ) );

	// --- normal, view space ---
	vec4 nm = texture( NormalMap, offset );
	vec3 N;
	if ( hasModelSpaceNormals ) {
		vec3 msn = nm.rgb * 2.0 - 1.0;
		vec3 nw = vec3( msn.r, msn.b, msn.g );
		if ( ( lodLitRed & 1 ) != 0 )
			nw.y = -nw.y;
		if ( ( lodLitRed & 2 ) != 0 )
			nw.z = -nw.z;
		N = normalize( nw * normalMatrix );
	} else {
		vec3 n = nm.rgb * 2.0 - 1.0;
		n.b = sqrt( max( 1.0 - dot( n.rg, n.rg ), 0.0 ) );
		N = normalize( btnMatrix_norm * n );
	}
	if ( !gl_FrontFacing )
		N = -N;

	// --- the surface ---
	vec4 sm = texture( SpecularMap, offset );
	Surface s;
	s.base = base;
	s.N = N;
	s.metal = 0.0;
	s.ao = 1.0;
	s.weight = 1.0;
	s.tint = vec3( 1.0 );
	s.f0d = vec3( 0.04 );
	s.diffRough = 0.0;
	float vis = 1.0;
	if ( lodLitSurface == 1 ) {
		s.rough = sm.r;
		s.metal = sm.g;
		vis = sm.b;
	} else if ( lodLitSurface == 2 ) {
		s.rough = 1.0;
	} else {
		float smoothness = clamp( specGlossiness, 0.0, 1.0 );
		float mask = 1.0;
		if ( hasSpecularMap ) {
			smoothness *= sm.g;
			mask = sm.r;
		}
		s.rough = 1.0 - smoothness;
		s.weight = mask * specStrength;
		s.tint = clamp( specColor, 0.0, 1.0 );
		s.f0d = s.tint * 0.04;
		if ( lodLitOccAlpha )
			vis = rawVertexAlpha;
	}
	if ( ( lodLitRed & 4 ) != 0 ) {
		s.weight = 0.0;
		s.metal = 0.0;
	}
	s.rough = clamp( s.rough, 0.035, 1.0 );
	s.metal = clamp( s.metal, 0.0, 1.0 );
	s.weight = clamp( s.weight, 0.0, 1.0 );
	vis = clamp( vis, 0.0, 1.0 );

	// --- light: pbrm_default.frag's BRDF ---
	vec3 V = ViewDir_norm;
	vec3 sunW = lodLitSun;
	if ( ( lodLitRed & 8 ) != 0 )
		sunW.y = -sunW.y;
	vec3 L = normalize( sunW * normalMatrix );
	float NdotV = clamp( dot( s.N, V ), 1e-4, 1.0 );
	vec2 dfg = dfgLazarov( NdotV, s.rough );
	vec3 ms = multiScatter( s, dfg );
	vec3 Espec = specAlbedo( s, dfg, ms );
	vec3 keepInd = clamp( vec3( 1.0 ) - Espec, 0.0, 1.0 );
	vec3 rho = s.base * ( 1.0 - s.metal );

	vec3 dDiff, dSpec;
	directLight( s, L, V, ms, dDiff, dSpec );
	vec3 sunDiff = dDiff * lodLitSunE;
	vec3 sunSpec = dSpec * lodLitSunE;

	// the sky: irradiance on the WORLD normal, reflection on the rough-bent reflected ray
	vec3 nW = normalMatrix * s.N;
	vec3 rW = normalMatrix * reflect( -V, s.N );
	vec3 skyDiff = lodLitSky( nW ) * rho * keepInd * vis;
	vec3 skySpec = lodLitSky( normalize( mix( rW, nW, s.rough * s.rough ) ) ) * Espec * vis;

	vec3 lit;
	if ( lodLitTerm == 1 )
		lit = sunDiff + sunSpec;
	else if ( lodLitTerm == 2 )
		lit = skyDiff + skySpec;
	else if ( lodLitTerm == 3 )
		lit = sunSpec + skySpec;
	else {
		lit = sunDiff + sunSpec + skyDiff + skySpec;
		if ( hasEmit ) {
			// the legacy glow is sqrt-space (fo4_default.frag:443-451, :544): squared to linear
			vec3 e = glowColor * glowMult * glowScaleSRGB;
			if ( hasGlowMap )
				e *= texture( GlowMap, offset ).rgb;
			lit += e * e;
		}
	}

	color.rgb = tonemap( sqrt( max( lit, vec3( 0.0 ) ) ) );
	fragColor = color;
}
