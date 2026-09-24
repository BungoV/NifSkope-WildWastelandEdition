#version 410 core

#include "uniforms.glsl"
#include "lookdev_fog.glsl"
#ifdef WW_SUNSHADOW
#include "ww_sunshadow.glsl"
#endif

// PBRM (PBR Material Editor) metallic/roughness path.
//
// Stage R3 (lane PBRR3, docs/NIFSKOPE_PBR_RENDERER.md s3.2): the FO4CS runtime's
// BRDF (truepbr_brdf.hlsli, wave 89 SPECV6) with the v6 specular of
// PBRM-v6-Specular.md, evaluated from ONE resolved surface (evalSurface):
//   F0_ior  = ((ior-1)/(ior+1))^2        ior = constant, or the spec map's A x iorMax
//   F_diel  = Schlick( F0' = weight * tint * F0_ior, F90' = sat( 50 F0' ) )
//             OpenPBR v1.1.1's IOR remap (lane PBRR4): the weight scales F0, so the
//             remapped IOR's Fresnel keeps its grazing rise; weight 0 = ior' 1 = no lobe
//   F_metal = weight * F82( base, tint )  (the editor's; tint 1 = plain Schlick)
//   D       = GGX( alpha = rough^2 )      Vis = height-correlated Smith
//   env     = cube * [ (F0 A + B) ms ]    Lazarov analytic DFG, ms = 1 + F0 (1/(A+B) - 1)
//   split   = indirect diffuse x ( 1 - E_spec )   (Q6, FO4CS bIndirectEnergySplit)
//   diffuse = Burley x ( 1 - F ), or EON when diffuseRoughness > 0 (direct light)
//   tint    = max( 0, (1 - sum m) + sum c m ), base *= tint   (lane PBRR4, ED:2310-2316)
//   emit    = colour * mask * luminance/100, a map REPLACES the constant (ED:2519-2521)
//   opacity = the .pbrm composition (ED:2282-2283, 2336-2337, 2921)
// Not here: the editor's vegetation/parallax/surface states, coat/sheen/anisotropy,
// refraction (merge, s3.3).

uniform sampler2D BaseMap;      // sRGB RGB, A opacity
uniform sampler2D NormalMap;    // linear RG (+B height, A curvature)
uniform sampler2D RmaosMap;     // R rough, G metal, B AO, A dielectric F0/porosity
uniform sampler2D EmissiveMap;  // sRGB RGB, A intensity mask
uniform sampler2D SpecColorMap; // v6: sRGB RGB specular tint (bit 25), A = IOR / iorMax (bit 30)
uniform sampler2D TintMaskMap;  // linear RGBA: the four tint masks (special layer 0)
uniform samplerCube CubeMap;

// Constants stand in wherever the corresponding channel is overridden. These
// mirror the reader's resolved values (src/io/pbrmfile.h).
uniform vec3 pbrBaseColor;
uniform float pbrOpacity;
uniform float pbrRoughness;
uniform float pbrMetallic;
uniform float pbrAo;
uniform float pbrF0;            // the untinted F0 of a weight-1 surface (v6 IorF0(ior), v4/v5 min(f0, .16))
uniform bool pbrSpecV6;         // bit 7 is the specular WEIGHT map (v6), not an F0 map (v4/v5)
uniform float pbrSpecWeight;    // v6 specularWeight (1 for v4/v5)
uniform vec3 pbrSpecTint;       // the constant specular colour, linear (white = none)
uniform float pbrIorMax;        // the IOR map's ceiling
uniform float pbrDiffuseRoughness;	// EON above 0
uniform float pbrNormalStrength;
uniform vec3 pbrEmissiveColor;		// raw sRGB as authored; decoded pow 2.2 here (the editor's)
uniform float pbrEmissiveIntensity;	// luminance / 100
uniform float pbrEmissiveMask;		// the constant A mask
uniform bool pbrEmissiveMapColor;	// the map's RGB replaces the constant (!overrideColor)
uniform bool pbrEmissiveMapMask;	// the map's A replaces the constant mask (!overrideMask)

// Tint masks (lane PBRR4): the editor's special layer 0 (ED:2310-2316)
uniform bool pbrTintEnabled;
uniform int pbrTintUseTex;			// bit c: mask c comes from TintMaskMap
uniform vec4 pbrTintMasks;			// the constant masks
uniform vec3 pbrTintColors[4];		// raw sRGB (Q13: the editor does not decode them)
uniform int pbrTintMode;			// 0 Normalize, 1 Add, 2 Priority RGBA

// Composition (lane PBRR4): -1 = the NIF alpha property (derived/legacy shapes);
// 0 Opaque, 1 Alpha Test, 2 Alpha Blend, 3 Premultiplied, 4 Additive, 5 Multiply,
// 6 Physical Transmission, 7 Water (6/7 blend like 2 until the merge)
uniform int pbrComposition;
uniform float pbrGlobalOpacity;
uniform float pbrAlphaThreshold;
uniform bool pbrAlphaConst;			// alphaSource "Constant"

// Feature bits, matching PbrmMaterial::Feature so the shader never guesses which
// channels are real. Bit set = sample the texture; clear = use the constant.
uniform int pbrFeatures;
#define F_BASECOLOR   1
#define F_NORMAL      2
#define F_HEIGHTBLUE  4
#define F_RMAOS       8
#define F_ROUGHNESS   16
#define F_METALLIC    32
#define F_AO          64
#define F_F0          128
#define F_EMISSIVE    256
#define F_OPACITY     2048
#define F_SPECCOLOR   0x2000000	// bit 25: the spec map's RGB is the tint
#define F_SPECIOR     0x40000000	// bit 30: the spec map's A x iorMax is the IOR

uniform bool hasCubeMap;
uniform float envReflection;

// Scene lighting (lane PBRR2A, docs s5.2). Only this program reads these, so the
// legacy programs cannot move. sceneMode 0 = Legacy, 1 = Studio, 2 = Lookdev (Studio + the weather DALC).
uniform int sceneMode;
uniform float sceneExposure;		// 2^EV, Studio only
uniform int viewTransform;			// 0 Standard (clamp + sRGB), 1 AgX, 2 Khronos PBR Neutral
uniform bool baseIsSrgbTex;			// the bound base texture is sRGB-tagged: GL already decoded it
uniform bool emissiveIsSrgbTex;
uniform bool specIsSrgbTex;
uniform bool hasStudioCube;
uniform samplerCube StudioCube;		// SFCubeMapCache GGX-prefiltered specular cube (Studio)
uniform samplerCube IrradianceMap;	// its 32 px diffuse cube (Studio)
uniform float studioProbe;			// >= 0: replace the lit result by this linear value (output gate)
uniform vec3 lookdevDalc[6];		// Lookdev (sceneMode 2, lane PBRR2B): DALC ambient, linear, X+ X- Y+ Y- Z+ Z-
uniform bool lookdevDalcFlip;		// red "dalcflip": the other axis-sign convention
uniform float studioSun;			// WW_STUDIO_SUN: the Studio/Lookdev sun scale (0 = the white furnace)
uniform int r3Term;					// WW_R3_TERM: 0 all, 1 diffuse only, 2 specular only
uniform int r3Red;					// WW_R3_RED: 1 noms, 2 nosplit, 4 fo4csweight, 8 notint, 16 f90scaled, 32 lambert
uniform int r4Red;					// WW_R4_RED: 1 nodiv, 2 emitmul, 4 notintmask, 8 emitraw, 16 nocomp

uniform float alpha;
uniform int alphaFlags;
uniform float alphaThreshold;

uniform vec2 uvScale;
uniform vec2 uvOffset;

in vec3 LightDir;
in vec3 ViewDir;
in vec2 texCoord;

flat in vec4 A;
in vec4 C;
flat in vec4 D;

in mat3 btnMatrix;
flat in mat3 reflMatrix;

out vec4 fragColor;

#ifndef M_PI
	#define M_PI 3.1415926535897932384626433832795
#endif

vec3 ViewDir_norm = normalize( ViewDir );
mat3 btnMatrix_norm = mat3( normalize( btnMatrix[0] ), normalize( btnMatrix[1] ), normalize( btnMatrix[2] ) );

bool hasFeature( int bit ) { return ( pbrFeatures & bit ) != 0; }

float pow5( float x )
{
	float x2 = x * x;
	return x2 * x2 * x;
}

vec3 fresnelSchlick( float c, vec3 F0 )
{
	return F0 + ( vec3( 1.0 ) - F0 ) * pow5( clamp( 1.0 - c, 0.0, 1.0 ) );
}

// OpenPBR F82 conductor Fresnel (the editor's, materialpreviewwidget.cpp:1789):
// Schlick bent so the 82-degree reflectance is tint x Schlick(82); tint 1 = Schlick.
vec3 fresnelF82( float mu, vec3 f0, vec3 tint )
{
	const float mb = 1.0 / 7.0;
	vec3 fs = fresnelSchlick( mu, f0 );
	vec3 fsb = fresnelSchlick( mb, f0 );
	vec3 b = fsb * ( vec3( 1.0 ) - tint ) / ( mb * pow( 1.0 - mb, 6.0 ) );
	return clamp( fs - b * mu * pow( 1.0 - mu, 6.0 ), 0.0, 1.0 );
}

// Lazarov's analytic environment DFG (FO4CS truepbr_brdf.hlsli:90-93), perceptual
// roughness: env specular = F0 * x + y. No LUT texture.
vec2 dfgLazarov( float NdotV, float r )
{
	vec4 rr = r * vec4( -1.0, -0.0275, -0.572, 0.022 ) + vec4( 1.0, 0.0425, 1.04, -0.04 );
	float a004 = min( rr.x * rr.x, exp2( -9.28 * NdotV ) ) * rr.x + rr.y;
	return vec2( -1.04, 1.04 ) * a004 + rr.zw;
}

// EON directional albedo (Portsmouth-Kutz-Hill 2024 fit; the editor's eonAlbedo)
float eonAlbedo( float mu, float r )
{
	float mc = 1.0 - mu, mc2 = mc * mc;
	float g = 0.0571085289 * mc + 0.491881867 * mc2 + ( -0.332181442 * mc + 0.0714429953 * mc2 ) * mc2;
	return ( 1.0 + r * g ) / ( 1.0 + 0.287861 * r );
}

vec3 srgbToLinear( vec3 c )
{
	return mix( c / 12.92, pow( ( c + 0.055 ) / 1.055, vec3( 2.4 ) ), step( vec3( 0.04045 ), c ) );
}

// the sRGB OETF: the encode is here, never GL_FRAMEBUFFER_SRGB (docs s8)
vec3 linearToSrgb( vec3 c )
{
	c = clamp( c, 0.0, 1.0 );
	return mix( c * 12.92, 1.055 * pow( c, vec3( 1.0 / 2.4 ) ) - 0.055, step( vec3( 0.0031308 ), c ) );
}

// AgX, Benjamin Wrensch's minimal fit (sRGB display out, the "base" look)
vec3 agxContrast( vec3 x )
{
	vec3 x2 = x * x;
	vec3 x4 = x2 * x2;
	return 15.5 * x4 * x2 - 40.14 * x4 * x + 31.96 * x4 - 6.868 * x2 * x + 0.4298 * x2 + 0.1191 * x - 0.00232;
}

vec3 agxDisplay( vec3 c )
{
	const mat3 agxMat = mat3( 0.842479062253094, 0.0423282422610123, 0.0423756549057051,
	                          0.0784335999999992, 0.878468636469772, 0.0784336,
	                          0.0792237451477643, 0.0791661274605434, 0.879142973793104 );
	const mat3 agxMatInv = mat3( 1.19687900512017, -0.0528968517574562, -0.0529716355144438,
	                             -0.0980208811401368, 1.15190312990417, -0.0980434501171241,
	                             -0.0990297440797205, -0.0989611768448433, 1.15107367264116 );
	const float minEv = -12.47393;
	const float maxEv = 4.026069;
	c = agxMat * max( c, vec3( 1e-10 ) );
	c = clamp( log2( c ), minEv, maxEv );
	c = ( c - minEv ) / ( maxEv - minEv );
	c = agxContrast( c );
	return clamp( agxMatInv * c, 0.0, 1.0 );
}

// Khronos PBR Neutral (linear in, linear out)
vec3 pbrNeutral( vec3 color )
{
	const float startCompression = 0.8 - 0.04;
	const float desaturation = 0.15;
	float x = min( color.r, min( color.g, color.b ) );
	float offset = x < 0.08 ? x - 6.25 * x * x : 0.04;
	color -= offset;
	float peak = max( color.r, max( color.g, color.b ) );
	if ( peak < startCompression )
		return color;
	const float d = 1.0 - startCompression;
	float newPeak = 1.0 - d * d / ( peak + d - startCompression );
	color *= newPeak / peak;
	float g = 1.0 - 1.0 / ( desaturation * ( peak - newPeak ) + 1.0 );
	return mix( color, vec3( newPeak ), g );
}

vec3 studioOutput( vec3 c )
{
	c = max( c, vec3( 0.0 ) ) * sceneExposure;
	if ( viewTransform == 1 )
		return agxDisplay( c );
	if ( viewTransform == 2 )
		return linearToSrgb( pbrNeutral( c ) );
	return linearToSrgb( c );	// Standard = "tonemap None"
}

// Same Hable/Uncharted curve the FO4 path uses, so a PBRM material and a BGSM
// material in the same scene are tonemapped identically instead of one looking
// washed out next to the other.
vec3 tonemap( vec3 x )
{
	float a = 0.15;
	float b = 0.50;
	float c = 0.10;
	float d = 0.20;
	float e = 0.02;
	float f = 0.30;

	vec3 z = x * x * D.a * ( A.a * 4.22978723 );
	z = ( z * ( a * z + b * c ) + d * e ) / ( z * ( a * z + b ) + d * f ) - e / f;
	return sqrt( z / ( A.a * 0.93333333 ) );
}

/* ---- the resolved surface (evalSurface) ---- */
struct Surface
{
	vec3 base;			// linear base colour
	vec3 N;				// view-space shading normal
	float rough;		// perceptual roughness, floored
	float metal;
	float ao;
	float weight;		// dielectric specular weight
	vec3 f0d;			// tint x F0_ior: the dielectric F0 at weight 1
	vec3 tint;			// the specular colour (the F82 edge tint of metals)
	float diffRough;	// EON roughness, 0 = Burley
};

/* The specular weight, OpenPBR v1.1.1 (bungo RULED 2026-09-24, lane PBRR4): the weight
 * remaps the IOR, F0' = clamp( w F0(ior) ), ior' = (1 + sqrt F0') / (1 - sqrt F0'), and the
 * dielectric Fresnel is the one of ior'. Its Schlick form keeps F90 = 1 for any real ior'
 * and must reach 0 at ior' = 1 (weight 0 = no lobe): F90' = sat( 50 F0' ), the 2% knee
 * below which no real dielectric sits. So weight 0.5 halves the head-on reflectance and
 * keeps the grazing rise (80 deg: 0.970 of weight 1 at ior 1.5). Reds: "fo4csweight" =
 * the runtime's F90 = 1 at every weight, "f90scaled" = lane PBRR3's w x Schlick (F90 scaled). */
vec3 dielF0( Surface s ) { return clamp( s.weight * s.f0d, 0.0, 1.0 ); }
vec3 dielF90( Surface s )
{
	if ( ( r3Red & 4 ) != 0 )
		return vec3( 1.0 );
	return clamp( 50.0 * dielF0( s ), 0.0, 1.0 );
}

// The whole specular Fresnel of the surface at cosine c
vec3 surfaceF( Surface s, float c )
{
	vec3 fd;
	if ( ( r3Red & 16 ) != 0 ) {
		fd = s.weight * fresnelSchlick( c, s.f0d );
	} else {
		vec3 F0 = dielF0( s );
		fd = F0 + ( dielF90( s ) - F0 ) * pow5( clamp( 1.0 - c, 0.0, 1.0 ) );
	}
	// metals: the weight scales the F82 conductor Fresnel (the specular colour is its edge tint)
	return mix( fd, s.weight * fresnelF82( c, s.base, s.tint ), s.metal );
}

// The specular F0 the multiscatter term takes (FO4CS: lerp(F0_diel, base, metal))
vec3 surfaceF0( Surface s )
{
	return mix( s.weight * s.f0d, s.weight * s.base, s.metal );
}

// Multiscatter energy compensation (FO4CS truepbr_brdf.hlsli:104-105). Red "noms": 1.
vec3 multiScatter( Surface s, vec2 dfg )
{
	if ( ( r3Red & 1 ) != 0 )
		return vec3( 1.0 );
	float Ess = max( clamp( dfg.x + dfg.y, 0.0, 1.0 ), 0.05 );
	return vec3( 1.0 ) + clamp( surfaceF0( s ), 0.0, 1.0 ) * ( 1.0 / Ess - 1.0 );
}

// E_spec: the specular lobe's directional albedo, multiscatter included -- the env
// specular scale and the energy the indirect diffuse gives up (Q6)
vec3 specAlbedo( Surface s, vec2 dfg, vec3 ms )
{
	// the split sum of Schlick( F0, F90 ) is F0 A + F90 B
	vec3 diel = ( ( r3Red & 16 ) != 0 ) ? s.weight * ( s.f0d * dfg.x + dfg.y ) : dielF0( s ) * dfg.x + dielF90( s ) * dfg.y;
	vec3 metl = s.weight * ( s.base * dfg.x + dfg.y * s.tint );	// the editor's F82 split-sum: B x tint
	return mix( diel, metl, s.metal ) * ms;
}

// One directional light: diffuse and specular BRDF x NoL (radiance per unit irradiance)
void directLight( Surface s, vec3 L, vec3 V, vec3 ms, out vec3 dDiff, out vec3 dSpec )
{
	vec3 H = normalize( L + V );
	float NoL = clamp( dot( s.N, L ), 0.0, 1.0 );
	float NoV = clamp( dot( s.N, V ), 1e-4, 1.0 );
	float NoH = clamp( dot( s.N, H ), 0.0, 1.0 );
	float VoH = clamp( dot( V, H ), 0.0, 1.0 );
	float LoH = clamp( dot( L, H ), 0.0, 1.0 );

	float alpha = s.rough * s.rough;
	float a2 = alpha * alpha;
	float d = max( NoH * NoH * ( a2 - 1.0 ) + 1.0, 1e-4 );
	float D = a2 / ( M_PI * d * d );
	float lambdaV = NoL * sqrt( NoV * NoV * ( 1.0 - a2 ) + a2 );
	float lambdaL = NoV * sqrt( NoL * NoL * ( 1.0 - a2 ) + a2 );
	float Vis = 0.5 / max( lambdaV + lambdaL, 1e-4 );
	vec3 F = surfaceF( s, VoH );
	dSpec = D * Vis * F * ms * NoL;

	vec3 rho = ( 1.0 - s.metal ) * s.base;
	vec3 keep = clamp( vec3( 1.0 ) - F, 0.0, 1.0 );
	if ( s.diffRough > 0.0 ) {
		// EON (OpenPBR base_diffuse_roughness; the editor's lines 2683-2689)
		float dr = s.diffRough, AF = 1.0 / ( 1.0 + 0.287861 * dr );
		float sON = dot( L, V ) - NoL * NoV;
		float stinv = sON > 0.0 ? sON / max( max( NoL, NoV ), 0.001 ) : sON;
		vec3 fss = rho / M_PI * AF * ( 1.0 + dr * stinv );
		float EFo = eonAlbedo( NoV, dr ), EFi = eonAlbedo( NoL, dr ), avgEF = AF * ( 1.0 + 0.072488 * dr );
		vec3 rhoMs = rho * rho * avgEF / max( vec3( 1.0 ) - rho * ( 1.0 - avgEF ), vec3( 0.001 ) );
		vec3 fms = rhoMs / M_PI * max( 0.01, 1.0 - EFo ) * max( 0.01, 1.0 - EFi ) / max( 0.01, 1.0 - avgEF );
		dDiff = keep * ( fss + fms ) * NoL;
	} else {
		// Burley, the game's (FO4CS wt-spec1 F4FX/Lighting/truepbr_brdf.hlsli:57-66, the
		// 1 - F keep :74-83), with the specular energy removed. Red "lambert": fd = 1.
		float f90 = 0.5 + 2.0 * s.rough * LoH * LoH;
		float fd = ( 1.0 + ( f90 - 1.0 ) * pow5( 1.0 - NoL ) ) * ( 1.0 + ( f90 - 1.0 ) * pow5( 1.0 - NoV ) );
		if ( ( r3Red & 32 ) != 0 )
			fd = 1.0;
		dDiff = keep * rho / M_PI * fd * NoL;
	}
}

void main()
{
	vec2 offset = texCoord.st * uvScale + uvOffset;

	// --- base colour / opacity ---
	vec3 base = pbrBaseColor;
	float op = pbrOpacity;
	if ( hasFeature( F_BASECOLOR ) ) {
		vec4 bc = texture( BaseMap, offset );
		// decoded as sRGB whatever the DXGI tag says (s5.2): an sRGB-tagged
		// texture arrives decoded by GL, an UNORM one is decoded here
		base = ( baseIsSrgbTex ? bc.rgb : srgbToLinear( bc.rgb ) ) * pbrBaseColor;
		// Only treat the texture's alpha as coverage when the material says so.
		// Taking it unconditionally discarded every fragment on legacy materials,
		// whose diffuse alpha is frequently zero or unrelated to opacity.
		if ( hasFeature( F_OPACITY ) )
			op = ( pbrComposition >= 0 ) ? bc.a : bc.a * pbrOpacity;	// the editor's baseAlpha replaces (ED:2258)
	}

	// --- tint masks (ED:2310-2316, the law verbatim) ---
	if ( pbrTintEnabled && ( r4Red & 4 ) == 0 ) {
		vec4 masks = pbrTintMasks;
		if ( pbrTintUseTex != 0 ) {
			vec4 tm = texture( TintMaskMap, offset );
			if ( ( pbrTintUseTex & 1 ) != 0 ) masks.r = tm.r;
			if ( ( pbrTintUseTex & 2 ) != 0 ) masks.g = tm.g;
			if ( ( pbrTintUseTex & 4 ) != 0 ) masks.b = tm.b;
			if ( ( pbrTintUseTex & 8 ) != 0 ) masks.a = tm.a;
		}
		float total = dot( masks, vec4( 1.0 ) );
		if ( pbrTintMode == 0 && total > 1.0 && ( r4Red & 1 ) == 0 )
			masks /= total;
		else if ( pbrTintMode == 2 )
			masks = vec4( masks.r, masks.g * ( 1.0 - masks.r ), masks.b * ( 1.0 - masks.r ) * ( 1.0 - masks.g ),
				masks.a * ( 1.0 - masks.r ) * ( 1.0 - masks.g ) * ( 1.0 - masks.b ) );
		vec3 tintMix = max( vec3( 0.0 ), vec3( 1.0 - dot( masks, vec4( 1.0 ) ) ) + pbrTintColors[0] * masks.r
			+ pbrTintColors[1] * masks.g + pbrTintColors[2] * masks.b + pbrTintColors[3] * masks.a );
		base *= tintMix;
	}
	base *= C.rgb;

	vec4 color = vec4( base, 1.0 );
	if ( pbrComposition >= 0 && ( r4Red & 16 ) == 0 ) {
		// the .pbrm composition (ED:2282-2283, 2336-2337); vertex alpha is not read (the editor's)
		float opacity = ( pbrAlphaConst ? 1.0 : op ) * pbrGlobalOpacity;
		if ( pbrComposition == 0 )
			opacity = 1.0;
		else if ( pbrComposition == 1 ) {
			if ( opacity < pbrAlphaThreshold )
				discard;
			opacity = 1.0;
		}
		color.a = opacity;
	} else if ( alphaFlags > 0 ) {
		float a = C.a * op * alpha;
		int m = ( a < alphaThreshold ? 0x2B2B : ( a > alphaThreshold ? 0x7171 : 0x4D4D ) );
		if ( ( m & ( 1 << alphaFlags ) ) == 0 )
			discard;
		if ( ( alphaFlags & 8 ) != 0 )
			color.a = a;
	}

	// --- normal ---
	// (s*255 - 128)/127 like the game's unpack (docs s3.2 item 3), Z rebuilt (B is
	// height when the material says so, never a normal component)
	vec3 normal = vec3( 0.0, 0.0, 1.0 );
	if ( hasFeature( F_NORMAL ) ) {
		vec4 nm = texture( NormalMap, offset );
		normal.rg = clamp( ( nm.rg * 255.0 - 128.0 ) / 127.0, -1.0, 1.0 ) * pbrNormalStrength;
		normal.b = sqrt( max( 1.0 - dot( normal.rg, normal.rg ), 0.0 ) );
	}
	// back faces: the GEOMETRIC normal flips before the tangent frame is applied
	mat3 tbn = btnMatrix_norm;
	if ( !gl_FrontFacing )
		tbn[2] = -tbn[2];

	// --- evalSurface ---
	Surface s;
	s.base = base;
	s.N = normalize( tbn * normal );
	s.rough = pbrRoughness;
	s.metal = pbrMetallic;
	s.ao = pbrAo;
	s.weight = pbrSpecWeight;
	float f0ior = pbrF0;
	if ( hasFeature( F_RMAOS ) ) {
		vec4 rm = texture( RmaosMap, offset );
		if ( hasFeature( F_ROUGHNESS ) ) s.rough = rm.r;
		if ( hasFeature( F_METALLIC ) )  s.metal = rm.g;
		if ( hasFeature( F_AO ) )        s.ao    = rm.b;
		// bit 7: v6 the specular weight map, v4/v5 the dielectric F0 (clamped 0.16);
		// the reader clears it when alpha carries porosity
		if ( hasFeature( F_F0 ) ) {
			if ( pbrSpecV6 )
				s.weight = rm.a;
			else
				f0ior = min( rm.a, 0.16 );
		}
	}
	s.tint = pbrSpecTint;
	if ( hasFeature( F_SPECCOLOR ) || hasFeature( F_SPECIOR ) ) {
		vec4 sc = texture( SpecColorMap, offset );
		if ( hasFeature( F_SPECCOLOR ) )
			s.tint = specIsSrgbTex ? sc.rgb : srgbToLinear( sc.rgb );
		if ( hasFeature( F_SPECIOR ) ) {
			float ior = sc.a * pbrIorMax;
			float r = ( ior - 1.0 ) / ( ior + 1.0 );
			f0ior = min( r * r, 1.0 );
		}
	}
	if ( ( r3Red & 8 ) != 0 )
		s.tint = vec3( 1.0 );
	s.f0d = s.tint * f0ior;
	s.rough = clamp( s.rough, 0.035, 1.0 );	// the design's floor (docs s3.2 item 3; FO4CS 0.045)
	s.metal = clamp( s.metal, 0.0, 1.0 );
	s.weight = clamp( s.weight, 0.0, 1.0 );
	s.diffRough = clamp( pbrDiffuseRoughness, 0.0, 1.0 );

	vec3 V = ViewDir_norm;
	vec3 R = reflect( -V, s.N );
	float NdotV = clamp( dot( s.N, V ), 1e-4, 1.0 );
	vec2 dfg = dfgLazarov( NdotV, s.rough );
	vec3 ms = multiScatter( s, dfg );
	vec3 Espec = specAlbedo( s, dfg, ms );
	// Q6: the indirect diffuse gives up the energy the specular lobe took. Red "nosplit".
	vec3 keepInd = ( ( r3Red & 2 ) != 0 ) ? vec3( 1.0 ) : clamp( vec3( 1.0 ) - Espec, 0.0, 1.0 );
	vec3 rho = s.base * ( 1.0 - s.metal );

	// Linear throughout (lane PBRR2A, docs s5.2). The sun is the one directional
	// light in LINEAR units: its irradiance is the uploaded diffuse colour x PI, so
	// a white Lambert surface facing a sun of 1 reads 1 -- the scale the Studio
	// environment has. The vertex stage's D.rgb is sqrt(diffuse), the legacy sqrt
	// space; this program reads the uniform itself and never squares a sqrt.
	vec3 sunE = lightSourceDiffuse[0].rgb * M_PI * ( sceneMode >= 1 ? studioSun : 1.0 );
	vec3 dDiff, dSpec;
	directLight( s, normalize( LightDir ), V, ms, dDiff, dSpec );
	vec3 outDiff = dDiff * sunE;
	vec3 outSpec = dSpec * sunE;
#ifdef WW_SUNSHADOW
	// lane CSM1 (spec 2.8): the cascade factor on the SUN's diffuse and specular only
	float csmF = wwSunShadow( -ViewDir );
	outDiff *= csmF;
	if ( ( csmRed & 4 ) == 0 )
		outSpec *= csmF;
#endif

	// --- ambient + environment ---
	// AO belongs on ambient, not on direct light: occlusion describes what the
	// surface cannot see of the environment, and applying it to a direct lobe
	// double-darkens contact shadows.
	if ( sceneMode >= 1 && hasStudioCube ) {
		// Studio: the SFCubeMapCache pair. The irradiance cube is the radiance
		// average (a uniform cube L stays L), so Lambert takes it without 1/PI.
		// FO76's roughness->lod law m = r*(10-4r) over the 7 filtered levels
		// (f76_default.frag:188).
		vec3 irr = texture( IrradianceMap, reflMatrix * s.N ).rgb;
		if ( sceneMode == 2 ) {
			// Lookdev: the weather's DALC 6-axis ambient on the WORLD normal (Z up).
			// An up-facing normal takes the Z- colour: MEASURED (lane PBRR3, 1.10.155
			// BSShaderManager::SetDirectionalAmbientColors RVA 0x27D64D0 builds the Z
			// row as 0.5 (Z- - Z+), dotted with the normal, never negated). The engine
			// blends linearly in the normal, in gamma space; this n^2 pick owes that.
			vec3 wn = envMapRotation * s.N;
			vec3 n2 = wn * wn;
			vec3 ax = ( ( wn.x > 0.0 ) != lookdevDalcFlip ) ? lookdevDalc[1] : lookdevDalc[0];
			vec3 ay = ( ( wn.y > 0.0 ) != lookdevDalcFlip ) ? lookdevDalc[3] : lookdevDalc[2];
			vec3 az = ( ( wn.z > 0.0 ) != lookdevDalcFlip ) ? lookdevDalc[5] : lookdevDalc[4];
			irr = n2.x * ax + n2.y * ay + n2.z * az;
		}
		outDiff += irr * rho * keepInd * s.ao;
		vec3 env = textureLod( StudioCube, reflMatrix * R, s.rough * ( 10.0 - 4.0 * s.rough ) ).rgb;
		outSpec += env * Espec * s.ao;
	} else {
		// Legacy (or Studio without a cube): the legacy light inputs un-squared.
		// A.rgb = sqrt(ambient) * 0.375, so A.rgb^2 is the ambient the legacy
		// shapes see in linear terms.
		outDiff += A.rgb * A.rgb * rho * keepInd * s.ao;
		if ( hasCubeMap ) {
			vec3 cube = textureLod( CubeMap, reflMatrix * R, s.rough * 8.0 ).rgb;
			// the legacy cube is display-encoded: squared into the same linear space
			outSpec += cube * cube * envReflection * Espec * s.ao;
		}
	}
	color.rgb = ( r3Term == 1 ) ? outDiff : ( r3Term == 2 ) ? outSpec : outDiff + outSpec;
	if ( r3Term == 3 )	// gate s1b: the surface Fresnel at N.V
		color.rgb = surfaceF( s, NdotV );

	// --- emission (ED:2519-2521): colour x mask x luminance/100, added before exposure,
	// never the legacy glow scale. A map REPLACES the constant colour / mask unless that
	// channel is overridden. Red "emitmul": the old constant x map law; "emitraw": no /100.
	if ( pbrEmissiveIntensity > 0.0 && r3Term == 0 ) {
		vec3 ecol = pow( max( pbrEmissiveColor, vec3( 0.0 ) ), vec3( 2.2 ) );
		float emask = pbrEmissiveMask;
		if ( hasFeature( F_EMISSIVE ) ) {
			vec4 em = texture( EmissiveMap, offset );
			vec3 emap = emissiveIsSrgbTex ? em.rgb : srgbToLinear( em.rgb );
			if ( ( r4Red & 2 ) != 0 ) {
				ecol *= emap;
				emask *= em.a;
			} else {
				if ( pbrEmissiveMapColor ) ecol = emap;
				if ( pbrEmissiveMapMask ) emask = em.a;
			}
		}
		color.rgb += ecol * emask * pbrEmissiveIntensity * ( ( r4Red & 8 ) != 0 ? 100.0 : 1.0 );
	}

	if ( sceneMode >= 1 ) {
		if ( sceneMode == 2 )
			color.rgb = wwFog( color.rgb, -ViewDir );	// lane FOG1: linear, before the exposure
		if ( studioProbe >= 0.0 )
			color.rgb = vec3( studioProbe );
		color.rgb = studioOutput( color.rgb );
	} else {
		// Legacy lighting: the linear result goes out through the legacy Hable +
		// sqrt, which expects its input in sqrt space.
		color.rgb = tonemap( sqrt( max( color.rgb, vec3( 0.0 ) ) ) );
	}
	// Premultiplied: the mapped colour times opacity (ED:2921), blended ONE, 1 - SRC_ALPHA
	if ( pbrComposition == 3 && ( r4Red & 16 ) == 0 )
		color.rgb *= color.a;

	fragColor = color;
	vec3 fogProbeOut;
	if ( wwFogProbe( -ViewDir, fogProbeOut ) )
		fragColor = vec4( fogProbeOut, 1.0 );
#ifdef WW_SUNSHADOW
	vec3 csmProbeOut;
	if ( wwSunShadowProbe( -ViewDir, fragColor.rgb, csmProbeOut ) )
		fragColor = vec4( csmProbeOut, 1.0 );
#endif
}
