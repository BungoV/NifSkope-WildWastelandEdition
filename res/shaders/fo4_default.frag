#version 410 core

#include "uniforms.glsl"
#ifdef WW_FOG
#include "lookdev_fog.glsl"
#endif
// FOGSUN1: the sun through the air; a variant without the fog compiled in (fo4_default, fo4_csm) has no air between
// the surface and the sun (lane VOLAIR1 10-15: calling wwVolAirSunT there broke both programs; the shader guard found it)
#ifdef WW_FOG
#define WW_AIR_SUN_T( p ) wwVolAirSunT( p )
#else
#define WW_AIR_SUN_T( p ) 1.0
#endif
#ifdef WW_SUNSHADOW
// lane SUNCELL1: the cascades (fo4_cellcsm, fo4_csm, fo4_fogcsm); the loader takes nothing after an include's quote
#include "ww_sunshadow.glsl"
#endif

#ifdef WW_BINDLESS
// lane BINDLESS2: the material's maps as resident handles in the draw's row (slots of src/gl/cellbindless.h)
#include "ww_bindless.glsl"
#define BaseMap sampler2D( wwDraws[wwDrawIdx].tex[0] )
#define NormalMap sampler2D( wwDraws[wwDrawIdx].tex[1] )
#define GlowMap sampler2D( wwDraws[wwDrawIdx].tex[2] )
#define BacklightMap sampler2D( wwDraws[wwDrawIdx].tex[3] )
#define SpecularMap sampler2D( wwDraws[wwDrawIdx].tex[4] )
#define GreyscaleMap sampler2D( wwDraws[wwDrawIdx].tex[5] )
#define EnvironmentMap sampler2D( wwDraws[wwDrawIdx].tex[6] )
#else
uniform sampler2D BaseMap;
uniform sampler2D NormalMap;
uniform sampler2D GlowMap;
uniform sampler2D BacklightMap;
uniform sampler2D SpecularMap;
uniform sampler2D GreyscaleMap;
uniform sampler2D EnvironmentMap;
#endif
uniform samplerCube CubeMap;

uniform vec3 specColor;
uniform float specStrength;
uniform float specGlossiness; // "Smoothness" in FO4; 0-1
uniform float fresnelPower;

uniform float paletteScale;

uniform vec3 glowColor;
uniform float glowMult;

uniform float alpha;
uniform vec2 wwDecalPom;	// lane DECALS1: placed decal parallax (DODT scale x 0.1, step cap); x = 0 off
uniform int alphaFlags;			// bits 0 to 2: alpha test mode, bit 3: alpha blending enabled
uniform bool landSplat;		// lane TERRBLEND1: vertex alpha = this blended-ground pass's share
uniform int landSplatDbg;	// lane ALLON2: 0 edge guard, 1 red WW_TERRBLEND_RED=edgepass (no guard), 2 probe WW_TERRBLEND_PROBE=edge
uniform float alphaThreshold;

uniform vec3 tintColor;

/* LOD channel preview: 0 off, 1 identity as a hashed colour, 2 raw index bytes,
 * 3 baked AO, 4 class parameter. Non-zero replaces the whole shading path with
 * the chosen vertex-colour channel, flat -- no textures, no normals, no light.
 * A per-vertex payload is otherwise invisible: it is multiplied into an albedo
 * and then lit, and what reaches the screen says nothing about the number that
 * is actually stored. */
uniform int lodChannelView;
uniform bool lodMaskRaw;		// channel 10: the specular slot raw (a source .lodm's third texture)
uniform bool lodEmissiveRaw;	// channel 13: the glow slot raw (a source .lodm's emissive texture)
uniform vec3 lodEmissiveColor;	// channel 13: the source's emissive colour, folded into the legacy sheet
uniform float lodSpecStrength;	// channel 10: the material's specular strength, lighting or not
uniform bool lodTreeAnim;		// channel 11: this shape carries the tree-animation flag
uniform bool lodMaskByTree;		// channel 11: the mask is the tree flag (a near tree), not the alpha test

uniform vec2 uvScale;
uniform vec2 uvOffset;

uniform bool hasEmit;
uniform bool hasGlowMap;
uniform bool hasSoftlight;
uniform bool hasBacklight;
uniform bool hasRimlight;
uniform bool hasTintColor;
uniform bool hasCubeMap;
uniform bool hasEnvMask;
uniform bool hasSpecularMap;
uniform bool greyscaleColor;

// Shader Flags 1 bit 12 (SLSF1_Model_Space_Normals): the bound normal map is an
// `_msn`, a MODEL-space map, not a tangent-space one.
uniform bool hasModelSpaceNormals;
// model -> view rotation, the same row-major uniform the vertex stage uses for
// the normal, tangent and bitangent.  Declared here because the model-space
// path has no tangent frame to ride on.
#ifdef WW_BINDLESS
// lane BINDLESS2FIX: the twin's normal matrix is in the draw's row (the uniform is never set for it: a zero matrix
// turned every model-space normal into NaN)
#define normalMatrix mat3( wwDraws[wwDrawIdx].nm[0].xyz, wwDraws[wwDrawIdx].nm[1].xyz, wwDraws[wwDrawIdx].nm[2].xyz )
#else
uniform mat3 normalMatrix;
#endif

uniform float subsurfaceRolloff;
uniform float rimPower;
uniform float backlightPower;

uniform float envReflection;

// screen-space refraction preview (SLSF1_Refraction)
uniform sampler2D RefractionSrc;
uniform bool doRefraction;
uniform bool editorBrdf;		// lane VANILLADEF1: the material editor's GGX lobe (Shading > Editor BRDF, WW_EDITOR_BRDF); off = the upstream law
uniform float refractionStrength;

in vec3 LightDir;
in vec3 ViewDir;

in vec2 texCoord;

flat in vec4 A;
in vec4 C;
flat in vec4 D;
in float rawVertexAlpha;	// the vertex colour's own alpha (channel 11 G only)

in mat3 btnMatrix;
flat in mat3 reflMatrix;

layout ( location = 0 ) out vec4 fragColor;
#ifdef WW_CELLLIGHTS
// lane SSGI2: the scene pass's second target, the emitted share alone (the bounce's emissive cast; 0 elsewhere)
layout ( location = 1 ) out vec4 cellEmitOut;
#endif

vec3 ViewDir_norm = normalize( ViewDir );
mat3 btnMatrix_norm = mat3( normalize( btnMatrix[0] ), normalize( btnMatrix[1] ), normalize( btnMatrix[2] ) );

#ifndef M_PI
	#define M_PI 3.1415926535897932384626433832795
#endif

#define FLT_EPSILON 1.192092896e-07F // smallest such that 1.0 + FLT_EPSILON != 1.0

float OrenNayar( vec3 L, vec3 V, vec3 N, float roughness, float NdotL )
{
	//float NdotL = dot(N, L);
	float NdotV = dot(N, V);
	float LdotV = dot(L, V);

	float rough2 = roughness * roughness;

	float A = 1.0 - 0.5 * (rough2 / (rough2 + 0.57));
	float B = 0.45 * (rough2 / (rough2 + 0.09));

	float a = min( NdotV, NdotL );
	float b = max( NdotV, NdotL );
	b = (sign(b) == 0.0) ? FLT_EPSILON : sign(b) * max( 0.01, abs(b) ); // For fudging the smoothness of C
	float C = sqrt( (1.0 - a * a) * (1.0 - b * b) ) / b;

	float gamma = LdotV - NdotL * NdotV;
	float L1 = A + B * max( gamma, FLT_EPSILON ) * C;

	return L1 * max( NdotL, FLT_EPSILON );
}

float OrenNayarFull(float NdotL, float NdotV, float LdotV, float roughness)
{
	float	NdotL0 = clamp(NdotL, FLT_EPSILON, 1.0);
	float	angleVN = acos(clamp(abs(NdotV), FLT_EPSILON, 1.0));
	float	angleLN = acos(NdotL0);

	float	alpha = max(angleVN, angleLN);
	float	beta = min(angleVN, angleLN);
	float	gamma = (LdotV - NdotL * NdotV) / sqrt(max((1.0 - NdotL * NdotL) * (1.0 - NdotV * NdotV), 0.000025));

	float roughnessSquared = roughness * roughness;
	float roughnessSquared9 = (roughnessSquared / (roughnessSquared + 0.09));

	// C1, C2, and C3
	float C1 = 1.0 - 0.5 * (roughnessSquared / (roughnessSquared + 0.33));
	float C2 = 0.45 * roughnessSquared9;

	if( gamma >= 0.0 ) {
		C2 *= sin(alpha);
	} else {
		C2 *= (sin(alpha) - pow((2.0 * beta) / M_PI, 3.0));
	}

	float powValue = (4.0 * alpha * beta) / (M_PI * M_PI);
	float C3 = 0.125 * roughnessSquared9 * powValue * powValue;

	// Avoid asymptote at pi/2
	float asym = M_PI / 2.0;
	float lim1 = asym + 0.005;
	float lim2 = asym - 0.005;

	float ab2 = (alpha + beta) / 2.0;

	if ( beta >= asym && beta < lim1 )
		beta = lim1;
	else if ( beta < asym && beta >= lim2 )
		beta = lim2;

	if ( ab2 >= asym && ab2 < lim1 )
		ab2 = lim1;
	else if ( ab2 < asym && ab2 >= lim2 )
		ab2 = lim2;

	// Reflection
	float A = gamma * C2 * tan(beta);
	float B = (1.0 - abs(gamma)) * C3 * tan(ab2);

	float L1 = NdotL0 * (C1 + A + B);

	// Interreflection
	float twoBetaPi = 2.0 * beta / M_PI;
	float L2 = 0.17 * NdotL0 * (roughnessSquared / (roughnessSquared + 0.13)) * (1.0 - gamma * twoBetaPi * twoBetaPi);

	return L1 + L2;
}

// GGX/Trowbridge-Reitz normal distribution. Ported from the PBR Material
// Editor's shader (D_GGX/G1 there) so the two tools agree on the spec/gloss
// response instead of NifSkope using a normalized Phong lobe.
float D_GGX( float NdotH, float alpha )
{
	float a2 = alpha * alpha;
	float d = NdotH * NdotH * ( a2 - 1.0 ) + 1.0;
	return a2 / max( M_PI * d * d, 0.0001 );
}

// Smith geometry term, one direction. k = (rough+1)^2/8 (the editor's split).
float G1( float NdotX, float k )
{
	return NdotX / max( NdotX * ( 1.0 - k ) + k, 0.0001 );
}

// Schlick's Fresnel approximation
float fresnelSchlick( float VdotH, float F0 )
{
	float base = 1.0 - VdotH;
	float exp = pow( base, fresnelPower );
	return clamp( exp + F0 * (1.0 - exp), 0.0, 1.0 );
}

// The Torrance-Sparrow visibility factor, G
float VisibDiv( float NdotL, float NdotV, float VdotH, float NdotH )
{
	float denom = max( VdotH, FLT_EPSILON );
	float numL = min( NdotV, NdotL );
	float numR = 2.0 * NdotH;
	if ( denom >= (numL * numR) ) {
		numL = (numL == NdotV) ? 1.0 : (NdotL / NdotV);
		return (numL * numR) / denom;
	}
	return 1.0 / NdotV;
}

// this is a normalized Phong model used in the Torrance-Sparrow model
vec3 TorranceSparrow(float NdotL, float NdotH, float NdotV, float VdotH, vec3 color, float power, float F0)
{
	// D: Normalized phong model
	float D = ((power + 2.0) / (2.0 * M_PI)) * pow( NdotH, power );

	// G: Torrance-Sparrow visibility term divided by NdotV
	float G_NdotV = VisibDiv( NdotL, NdotV, VdotH, NdotH );

	// F: Schlick's approximation
	float F = fresnelSchlick( VdotH, F0 );

	// Torrance-Sparrow:
	// (F * G * D) / (4 * NdotL * NdotV)
	// Division by NdotV is done in VisibDiv()
	// and division by NdotL is removed since
	// outgoing radiance is determined by:
	// BRDF * NdotL * L()
	float spec = (F * G_NdotV * D) / 4.0;

	return color * spec * M_PI;
}

vec3 tonemap(vec3 x)
{
	float a = 0.15;
	float b = 0.50;
	float c = 0.10;
	float d = 0.20;
	float e = 0.02;
	float f = 0.30;

	vec3 z = x * x * D.a * (A.a * 4.22978723);
	z = (z * (a * z + b * c) + d * e) / (z * (a * z + b) + d * f) - e / f;
	return sqrt(z / (A.a * 0.93333333));
}


#ifdef WW_CELLLIGHTS
#ifndef WW_CELL_GI_MARCH_COPIES	// red (tests/spells/cell_giblack_link.sh): a march copy per call site, over the link limit
#define WW_CELL_GI_MARCH_ONCE 1	// lane GIBLACKLINK1: the march red runs once (cellGiMarchPrime in main)
#endif
#include "cell_lights.glsl"
#include "cell_reflprobe.glsl"
#include "cell_ao.glsl"
#include "cell_ssr.glsl"
#include "cell_grass.glsl"
#endif
#ifdef WW_FARGROUND	// lane FARDRAW1: the far ring 0's live ground
#include "cell_farground.glsl"
#endif

/* lane DECALS1: the game's decal parallax. Height = the set's TX04 (bound as GlowMap, red); the eye ray in the
 * decal's unit uv axes x tan(angle) x scale, linear search from the top in n steps, then one secant step. */
vec2 wwDecalPomUv( vec2 uv0, vec3 P, vec3 N, vec3 E )
{
	vec3 dpx = dFdx( P ), dpy = dFdy( P );
	vec2 dux = dFdx( uv0 ), duy = dFdy( uv0 );
	float det = dux.x * duy.y - duy.x * dux.y;
	if ( abs( det ) < 1e-12 )
		return uv0;
	vec3 T = normalize( ( dpx * duy.y - dpy * dux.y ) / det );	// +u on the surface
	vec3 B = normalize( ( dpy * dux.x - dpx * duy.x ) / det );	// +v
	float ez = max( dot( E, N ), 1e-3 );
	vec2 ofs = -vec2( dot( E, T ), dot( E, B ) ) / ez * wwDecalPom.x;
	float n = min( 72.0 - 56.0 * clamp( dot( N, E ), 0.0, 1.0 ), wwDecalPom.y );
	float stepH = 1.0 / n;
	float layer = 1.0, prevLayer = 1.0, h = 0.0, prevH = 1.0;
	vec2 uv = uv0;
	for ( int i = 0; i < 72; i++ ) {
		if ( float( i ) >= n )
			break;
		uv = uv0 + ofs * ( 1.0 - layer );
		h = textureGrad( GlowMap, uv, dux, duy ).r;
		if ( layer < h )
			break;
		prevLayer = layer;
		prevH = h;
		layer -= stepH;
	}
	float d0 = prevLayer - prevH, d1 = layer - h;
	float t = ( d0 - d1 ) != 0.0 ? prevLayer + ( layer - prevLayer ) * d0 / ( d0 - d1 ) : layer;
	return uv0 + ofs * ( 1.0 - t );
}

void main()
{
#ifdef WW_CELLLIGHTS
	cellEmitOut = vec4( 0.0 );
	float wwGrassK = wwGrassFadeK( ViewDir );	// lane GRASSMB1: exactly 1.0 on every non-grass shape
	if ( wwGrassK <= 0.0 )
		discard;
#endif
	vec2 offset = texCoord.st * uvScale + uvOffset;
	if ( wwDecalPom.x > 0.0 ) {	// lane DECALS1: the uv leaving the decal's rect is clipped, as the game's decal pass does
		offset = wwDecalPomUv( offset, -ViewDir, normalize( btnMatrix[2] ), normalize( ViewDir ) );
		if ( any( lessThan( offset, vec2( 0.0 ) ) ) || any( greaterThan( offset, vec2( 1.0 ) ) ) )
			discard;
	}
#ifdef WW_SUNSHADOW
	/* lane SUNCELL1: the cascades on the SUN only (CSM1 spec 2.8). Every D.rgb below is the sun: D = sqrt(the
	 * linear sun), so D x sqrt(f) is the sun x f in the linear light the tonemap squares back. The macro takes
	 * every later D; tonemap() above still reads the varying (D.a only, unchanged). */
	float csmF = wwSunShadow( -ViewDir ) * WW_AIR_SUN_T( -ViewDir );	// FOGSUN1: and the air's transmittance
	vec4 Dcsm = vec4( D.rgb * sqrt( csmF ), D.a );
#define D Dcsm
#ifdef WW_CELLLIGHTS
	cellDirVis = csmF;	// the exterior cell sun (cellLit) takes the same cascades
#endif
#else
	// FOGSUN1: no cascades compiled in; the sun still crosses the air
	float airSunT = WW_AIR_SUN_T( -ViewDir );
	vec4 Dair = vec4( D.rgb * sqrt( airSunT ), D.a );
#define D Dair
#ifdef WW_CELLLIGHTS
	cellDirVis = airSunT;
#endif
#endif

	vec4 baseMap = texture( BaseMap, offset );
#ifdef WW_FARGROUND
	baseMap.rgb = fgGround( cellWorldPos( -ViewDir ), baseMap.rgb );
#endif

	vec4 color = vec4( baseMap.rgb, 1.0 );
	if ( alphaFlags > 0 ) {
		/* The class parameter LIVES in vertex alpha, so the alpha test has to
		 * run on the real value even when the preview is about to replace the
		 * colour -- otherwise a tree card previews as a solid quad and its
		 * silhouette, the thing you are looking at, is gone. Hence the preview
		 * early-out sits AFTER this and not at the top of main(). */
#ifdef WW_CELLLIGHTS
		float	a = wwGrassVertexA( C.a ) * baseMap.a * alpha;
		a *= wwGrassK;	// lane GRASSMB1: the grass fades out through its own alpha test
#else
		float	a = C.a * baseMap.a * alpha;
#endif
		// 0: always, 1: <, 2: ==, 3: <=, 4: >, 5: !=, 6: >=, 7: never
		int	m = ( a < alphaThreshold ? 0x2B2B : ( a > alphaThreshold ? 0x7171 : 0x4D4D ) );
		if ( ( m & ( 1 << alphaFlags ) ) == 0 )
			discard;
		if ( ( alphaFlags & 8 ) != 0 )
			color.a = a;
	}

	if ( lodChannelView != 0 ) {
		vec3 v = vec3( 0.0 );
		if ( lodChannelView == 1 ) {
			/* The index HASHED to a colour, not the raw bytes: consecutive
			 * indices differ by one part in 255 of red and are
			 * indistinguishable, which is exactly the case worth seeing. */
			float idx = floor( C.r * 255.0 + 0.5 ) + floor( C.g * 255.0 + 0.5 ) * 256.0;
			v = fract( sin( ( idx + 1.0 ) * vec3( 12.9898, 78.233, 45.164 ) ) * 43758.5453 );
			v = v * 0.8 + 0.2;
		} else if ( lodChannelView == 2 ) {
			v = vec3( C.r, C.g, 0.0 );      // the raw 16-bit index bytes
		} else if ( lodChannelView == 3 ) {
			v = C.bbb;                      // ambient occlusion (objects and terrain)
		} else if ( lodChannelView == 4 ) {
			v = C.aaa;                      // A: tree sway / terrain shore proximity
		} else if ( lodChannelView == 5 ) {
			/* Terrain R is a material CLASS id, not a magnitude -- shown hashed
			 * for the same reason the object index is: neighbouring classes
			 * differ by a few parts in 255 and read as one flat grey. */
			float cls = floor( C.r * 255.0 + 0.5 );
			v = fract( sin( ( cls + 1.0 ) * vec3( 12.9898, 78.233, 45.164 ) ) * 43758.5453 );
			v = v * 0.8 + 0.2;
		} else if ( lodChannelView == 6 ) {
			v = C.ggg;                      // terrain flow-accumulation wetness
		} else if ( lodChannelView == 8 ) {
			/* The impostor bake's normal sheet: the GEOMETRIC normal in the
			 * view's space, half-packed, flipped for back faces as the lit
			 * path flips it. After the alpha test above, so a leaf card
			 * writes its normal only where it has a leaf. */
			vec3 n = normalize( btnMatrix_norm[2] );
			if ( !gl_FrontFacing )
				n = -n;
			v = n * 0.5 + 0.5;
		} else if ( lodChannelView == 9 ) {
			// the impostor bake's height: window z, linear under the ortho projection
			v = vec3( gl_FragCoord.z );
		} else if ( lodChannelView == 10 ) {
			/* The impostor bake's material channel (docs/LODGEN_IMPOSTOR_SPEC.md).
			 * A shape retargeted to a source .lodm: its third texture, raw --
			 * roughness/metallic/AO or gloss/specular/AO as the file says.
			 * Otherwise the LEGACY pair from the vanilla material, the way the
			 * lit path composes it: R gloss = smoothness x the specular map's
			 * G, G specular = the map's R (the normal's alpha without a map)
			 * x the specular strength. */
			vec4 sm = texture( SpecularMap, offset );
			if ( lodMaskRaw ) {
				v = sm.rgb;
			} else {
				float gl = clamp( specGlossiness, 0.0, 1.0 ) * ( hasSpecularMap ? sm.g : 1.0 );
				float sp = ( hasSpecularMap ? sm.r : texture( NormalMap, offset ).a )
					* ( lodSpecStrength > 0.0 ? lodSpecStrength : 1.0 );
				v = vec3( gl, clamp( sp, 0.0, 1.0 ), 1.0 );	// B: neutral AO, one per-pixel law for mixed shapes
			}
		} else if ( lodChannelView == 12 ) {
			/* The impostor bake's colour sheet: the base colour as the source
			 * has it, times the vertex colour, and NOTHING else - the lit path
			 * below tone-maps before it writes, so an "unlit" render of it is
			 * a curved albedo. The consumer lights the card. */
			v = baseMap.rgb * C.rgb;
		} else if ( lodChannelView == 11 ) {
			/* The impostor bake's subsurface mask source. A LOD tree: the
			 * alpha-tested shapes are the leaf cards, the trunk is opaque. A
			 * near tree: every shape is alpha-tested (the bark card too), and
			 * the branch cards are the ones that carry the tree-animation
			 * flag; the bake picks the rule for the model. */
			float leaf = lodMaskByTree ? ( lodTreeAnim ? 1.0 : 0.0 ) : ( alphaFlags > 0 ? 1.0 : 0.0 );
			/* G: the WIND WEIGHT W (IMPOSTORWIND1, sway A): the raw vertex alpha on a
			 * tree-animation shape -- the only wind input the game's tree vertex
			 * shader reads -- and 0 on any other shape, which the game never moves.
			 * The mask reads only R. */
			v = vec3( leaf, lodTreeAnim ? rawVertexAlpha : 0.0, 0.0 );
		} else if ( lodChannelView == 13 ) {
			/* The impostor bake's emissive sheet (docs/LODGEN_IMPOSTOR_SPEC.md).
			 * A shape whose GLOW SLOT a source .lodm retargeted reads that
			 * texture raw - and an EMPTY retarget binds black, which is how a
			 * pbr set that names no emissive says it emits nothing.
			 * Otherwise the VANILLA LOD GLOW RULE, measured off the shipped
			 * atlases: a LOD chunk shape carries Own-Emit with a black emissive
			 * colour and no glow slot, so what lights it is its DIFFUSE'S ALPHA
			 * on the opaque shapes (Diamond City's DXT5 atlas: 216,521 of
			 * 262,144 alpha blocks varying, none fully transparent). An
			 * alpha-tested shape spends its alpha on the cut-out instead and
			 * emits nothing.
			 * The EMISSIVE COLOUR is folded in here, because the engine scales
			 * that alpha by it: a chunk shape own-emits with a black colour and
			 * therefore emits nothing, which is the whole answer to "an opaque
			 * source with alpha 255 throughout would light every wall". The
			 * MULTIPLE is not folded in - it can exceed 1 and this sheet is eight
			 * bits - and rides in the set's .lodm as `emissiveScale`. A retargeted
			 * glow slot stays RAW: a source .lodm's emissive is its own picture. */
			if ( lodEmissiveRaw )
				v = texture( GlowMap, offset ).rgb;
			else
				v = ( alphaFlags == 0 ) ? baseMap.rgb * baseMap.a * lodEmissiveColor : vec3( 0.0 );
		} else {
			v = C.rrr;                      // raw red: water depth
		}
		fragColor = vec4( v, 1.0 );
		return;
	}

	vec4 normalMap = texture( NormalMap, offset );
	vec4 specMap = texture( SpecularMap, offset );
	vec4 glowMap = texture( GlowMap, offset );

	vec3 normal;
	if ( hasModelSpaceNormals ) {
		/* A model-space map carries all three components already, in the
		 * model's own axes, so it needs model -> view and NOTHING else.  The
		 * tangent-space path below would instead read the map's two stored
		 * channels as offsets along the mesh's tangent and bitangent -- and a
		 * terrain LOD tile's tangent frame is arbitrary (btdterrain.cpp builds
		 * T = n x worldUp, B = n x T), so the sheet's "up" lands sideways and
		 * the surface shades in blotches.
		 *
		 * Channel order MEASURED on Bethesda's own shipped sheets
		 * (Data/Textures/Terrain/Commonwealth/Commonwealth.16.*_msn.DDS, six
		 * 512x512 tiles, correlated against the heights of the same cells):
		 *   R = EAST  (+x)   corr 0.364 with -dh/dx, 0.001 with -dh/dy
		 *   B = NORTH (+y)   corr 0.422 with -dh/dy, 0.002 with -dh/dx
		 *   G = UP    (+z)   mean 238.6 of 255
		 * Alpha is a constant 255 on every tile and carries nothing.
		 * sk_msn.frag's `.rbg` swizzle is this same order.
		 *
		 * The blue channel is NOT recomputed here: it is real data. */
		vec3 msn = normalMap.rgb * 2.0 - 1.0;
		normal = normalize( vec3( msn.r, msn.b, msn.g ) * normalMatrix );
	} else {
		normal = normalMap.rgb * 2.0 - 1.0;
		// Calculate missing blue channel
		normal.b = sqrt(max(1.0 - dot(normal.rg, normal.rg), 0.0));
		normal = normalize( btnMatrix_norm * normal );
	}
	if ( !gl_FrontFacing )
		normal *= -1.0;
#ifdef WW_CELLLIGHTS
	if ( cellOn && cellProbe == 20 && cellAoEarly == 1 ) {	// lane PERF1: the obscurance pass reads the normal and depth only
		fragColor = cellAoPassOut( normal, -ViewDir );
		return;
	}
#endif

	vec3 L = normalize(LightDir);
	vec3 V = ViewDir_norm;
	vec3 R = reflect(-V, normal);
	vec3 H = normalize( L + V );

	float NdotL = dot(normal, L);
	if ( hasModelSpaceNormals ) {
		/* A terrain LOD sheet stores steep banks the coarse LOD mesh does not
		 * have, so a texel can face away from the light on a surface that is
		 * plainly in view, and with the camera light it fell to near-black in
		 * hard-edged patches (bungo 2026-09-18). Squared half-Lambert: 1 at
		 * N.L = 1, 0.25 at the terminator, 0 only when facing straight away. */
		float hl = NdotL * 0.5 + 0.5;
		NdotL = hl * hl;
	}
	float NdotL0 = max( NdotL, FLT_EPSILON );
	float NdotH = max( dot(normal, H), FLT_EPSILON );
	float NdotV = max( dot(normal, V), FLT_EPSILON );
	float VdotH = max( dot(V, H), FLT_EPSILON );
	float NdotNegL = max( dot(normal, -L), FLT_EPSILON );

	vec3 reflectedWS = reflMatrix * R;

	vec3 albedo = baseMap.rgb * C.rgb;
	vec3 diffuse = A.rgb + D.rgb * NdotL0;
	if ( greyscaleColor )
		albedo = textureLod( GreyscaleMap, vec2( baseMap.g, paletteScale * C.r ), 0.0 ).rgb;

	// Emissive
	vec3 emissive = vec3(0.0);
	if ( hasEmit ) {
		emissive += glowColor * glowMult;

		if ( hasGlowMap ) {
			emissive *= glowMap.rgb;
		}
	}

	// Specular — GGX + Smith + Schlick, matching the material editor's BRDF:
	//   spec = D_GGX(NoH, a) * G1(NoL,k) * G1(NoV,k) * F / (4 NoL NoV)
	// with a = rough^2 and k = (rough+1)^2/8.
	float g = 1.0;
	float s = 1.0;
	float smoothness = clamp( specGlossiness, 0.0, 1.0 );
	if ( hasSpecularMap ) {
		// G is gloss, R is the specular mask (measured, not assumed). A flat
		// white G correctly falls back to the material's scalar smoothness.
		g = specMap.g;
		s = specMap.r;
		smoothness = g * smoothness;
	}
	// Specular is no longer gated on hasSpecularMap: a material with only the
	// scalar Smoothness/Specular Strength authored used to render completely
	// matte, which is not what the game does with it.
	float specMask = s * specStrength;

	// FO4 encodes NO metallicity, so F0 is the dielectric constant and _s.R
	// stays what it is authored as — a specular MASK, not a metalness channel.
	// The old hardcoded F0 of 0.2 was ~5x too reflective for a dielectric; the
	// editor's default is 0.04.
	float rough = clamp( 1.0 - smoothness, 0.02, 1.0 );
	float alphaR = rough * rough;
	float kSmith = ( rough + 1.0 ) * ( rough + 1.0 ) * 0.125;
	// fresnelSchlick honours the material's authored Fresnel Power rather than
	// hardwiring ^5 — that field exists in FO4 and the editor has no equivalent.
	float F = fresnelSchlick( VdotH, 0.04 );
	vec3 spec = vec3( D_GGX( NdotH, alphaR )
	                  * G1( NdotL0, kSmith ) * G1( NdotV, kSmith )
	                  * F / max( 4.0 * NdotL0 * NdotV, 0.001 ) )
	            * specMask * NdotL0 * D.rgb * specColor;
	float ambF0 = 0.04;
	float ambMask = specMask;
	if ( !editorBrdf ) {
		/* lane VANILLADEF1: the upstream law (and the game's light specular, cell_lights.glsl): normalized Phong,
		 * n = 2^(gloss x 10 + 1), F0 0.2, no specular without a specular map, no (1 - F) on the diffuse */
		F = 0.0;
		ambF0 = 0.2;
		spec = vec3( 0.0 );
		if ( hasSpecularMap )
			spec = TorranceSparrow( NdotL0, NdotH, NdotV, VdotH, vec3( specMask ), exp2( smoothness * 10.0 + 1.0 ), 0.2 )
			       * NdotL0 * D.rgb * specColor;
		else
			ambMask = 1.0;
	}
	vec3 specSun = spec;	// lane SUNCELL1: the sun's specular before the cube (the exterior cell sun keeps it)

	// Environment
	float specSky = 1.0;	// lane SPECOCC1: outdoors the cube reflection keeps the sky visibility along R (cell_lights.glsl)
#ifdef WW_CELLLIGHTS
	if ( cellOn )
		specSky = cellSpecSky( cellWorldPos( -ViewDir ), cellWorldDir( normal ), cellWorldDir( R ), rough );
#endif
	vec4 cube = textureLod( CubeMap, reflectedWS, 8.0 - smoothness * 8.0 );
	vec4 env = texture( EnvironmentMap, offset );
	vec3 envSpec = vec3( 0.0 );	// lane PRTPGI: the reflection before the viewport light, for the cell path
	if ( hasCubeMap ) {
		cube.rgb *= envReflection * specStrength;
		if ( hasEnvMask ) {
			cube.rgb *= env.r;
		} else {
			cube.rgb *= s;
		}

		envSpec = cube.rgb;
		spec += cube.rgb * diffuse * specSky;
	}
#ifdef WW_CELLLIGHTS
	// lane CUBE1: the reflection the cell path adds before its light, linear: the game's term, or the old one
	// (squared, x Lambert, the default cube too) under WW_CELL_LIT_RED=cubeold
	vec3 cubeK = envSpec * envSpec;
	if ( cellOn && ( cellRed & 512 ) == 0 )
		cubeK = hasCubeMap ? cellCubeGame( CubeMap, reflectedWS, cellCubeMat.y * ( hasSpecularMap ? specMap.g : 1.0 ),
		                                   cellCubeMat.x * ( hasSpecularMap ? specMap.r : 1.0 ), envReflection, -ViewDir )
		                   : vec3( 0.0 );
	// lane REFLPROBE1: the placed probes take the material cube's place where they cover (cell_reflprobe.glsl)
	if ( cellOn && hasCubeMap && ( cellRed & 512 ) == 0 )
		cubeK *= cellReflProbe( GreyscaleMap, -ViewDir, R, cellCubeMat.y * ( hasSpecularMap ? specMap.g : 1.0 ),
		                        cellCubeMat.x * ( hasSpecularMap ? specMap.r : 1.0 ), envReflection );
	// lane SSR1: the screen-space reflection over the cube term, by the march's confidence (cell_ssr.glsl)
	if ( cellOn )
		cubeK = cellSsrMix( cubeK, cellCubeMat.y * ( hasSpecularMap ? specMap.g : 1.0 ),
		                    cellCubeMat.x * ( hasSpecularMap ? specMap.r : 1.0 ), envReflection );
	cubeK *= specSky;	// lane SPECOCC1: after the SSR mix, as FO4CS scales its reflection after the SSR blend
#endif

	vec3 backlight = vec3(0.0);
	if ( backlightPower > 0.0 ) {
		backlight = albedo * NdotNegL * clamp( backlightPower, 0.0, 1.0 );

		emissive += backlight * D.rgb;
	}

	vec3 rim = vec3(0.0);
	if ( hasRimlight ) {
		rim = vec3(pow((1.0 - NdotV), rimPower));
		rim *= smoothstep( -0.2, 1.0, dot(-L, V) );

		//emissive += rim * D.rgb * specMask;
	}

	// Diffuse
	float diff = OrenNayarFull( NdotL, dot(normal, V), dot(L, V), 1.0 - smoothness );
	diffuse = vec3(diff);

	vec3 soft = vec3(0.0);
	float wrap = NdotL;
	if ( hasSoftlight || subsurfaceRolloff > 0.0 ) {
		wrap = (wrap + subsurfaceRolloff) / (1.0 + subsurfaceRolloff);
		soft = albedo * max( 0.0, wrap ) * smoothstep( 1.0, 0.0, sqrt(diff) );

		diffuse += soft;
	}

	if ( hasTintColor ) {
		albedo *= tintColor;
	}

	// Diffuse. (1 - F) is the editor's energy conservation: light reflected by
	// the specular lobe is no longer also counted as diffuse, so glancing angles
	// stop reading brighter than the material can physically be. The lobe shape
	// stays Oren-Nayar rather than the editor's Lambert — it is roughness-aware
	// and is what the rest of the FO4 path is tuned against.
	color.rgb = diffuse * albedo * D.rgb * ( 1.0 - F );
	// Ambient
	color.rgb += A.rgb * albedo;
	// Specular
	color.rgb += spec;
	color.rgb += A.rgb * ambMask * fresnelSchlick( VdotH, ambF0 ) * (1.0 - NdotV) * D.rgb;
	// Emissive
	color.rgb += emissive * glowScaleSRGB;
#ifdef WW_CELLLIGHTS
#if defined( WW_CELL_GI_MARCH ) && defined( WW_CELL_GI_MARCH_ONCE )
	// lane GIBLACKLINK1: the march red's one GI read, at the (P, N) every later caller passes (cellPassOut's nonormal red: up)
	if ( cellOn && cellGiOn )
		cellGiMarchPrime( cellWorldPos( -ViewDir ), cellPass > 0 && cellPass != 6 && cellProbe == 0 && ( cellPassRed & 2 ) != 0
		                                               ? vec3( 0.0, 0.0, 1.0 ) : cellWorldDir( normal ) );
#endif
	// lane SKY1: outdoors the bounce grid's sky stands in for the weather's unshadowed ambient (0 elsewhere)
	/* lane SUNCELL1: an exterior with the weather's sun (cellHasDir) hands cellLit the ambient alone; the sun's
	 * diffuse and the cube are the game's terms there, its specular the viewport's (cellExtSpec, linear) */
	if ( cellOn && !cellInterior && cellHasDir ) {
		// lane BOUNCE1: the sun's specular x the imagespace Sunlight Scale, as its diffuse (cellDirColor); sqrt space here
		vec3 sx = specSun * sqrt( max( cellSunScale, 0.0 ) ) + A.rgb * specMask * fresnelSchlick( VdotH, 0.04 ) * ( 1.0 - NdotV ) * D.rgb;
		cellExtSpec = sx * sx;
		/* lane SUNCELL1: the ambient is the weather's directional ambient (DALC, the hour's keys) through the
		 * interior's law (cellAmbient), not the flat NAM0 Ambient the viewport carries */
		vec3 Ax = A.rgb;
		if ( cellExtDalc )
			Ax = sqrt( cellAmbient( cellWorldDir( normal ), cellWorldPos( -ViewDir ) ) );
		cellExtAmb = Ax * Ax;
		color.rgb = Ax * albedo;
		/* lane GISHADE1: the grid's sky replaces the share k of THIS ambient (the weather's DALC) in linear light, as
		 * the PBR program does: the ambient keeps (1 - k) of itself, so its colour here (sqrt of linear) takes
		 * sqrt(1 - k), and the cube's ambient (cellExtAmb) keeps (1 - k) too. The old law (red WW_CELL_GISHADE_RED=sqrt)
		 * took k x the flat NAM0 A off in sqrt space: (Ax - k A)^2 stayed lit beside the grid's sky (k = 1, open sky
		 * facing up: +23% blue on CommonwealthClear's day), (1 - k)^2 where the grid fades out */
		if ( cellGiSky && ( cellRed & 131072 ) == 0 ) {
			float k = cellGiSkyK( cellWorldPos( -ViewDir ), cellWorldDir( normal ) );
			color.rgb *= sqrt( 1.0 - k );
			cellExtAmb *= 1.0 - k;
		}
	}
	if ( cellOn && cellGiSky && ( cellRed & 131072 ) == 0 && !( !cellInterior && cellHasDir ) ) {
		// lane GISHADE1: without the weather's sun the colour carries the flat A x albedo among its other terms: keep
		// sqrt(1 - k) of that ambient (linear (1 - k)), the rest as drawn
		float k = cellGiSkyK( cellWorldPos( -ViewDir ), cellWorldDir( normal ) );
		color.rgb -= ( 1.0 - sqrt( 1.0 - k ) ) * A.rgb * albedo;
	} else if ( cellOn && cellGiSky && ( cellRed & 131072 ) != 0 )
		color.rgb -= cellGiSkyK( cellWorldPos( -ViewDir ), cellWorldDir( normal ) ) * A.rgb * albedo;
	vec3 cellPreLit = color.rgb;	// lane SSGI2: an exterior without the sun adds the glow before cellLit's root
	// lane PRTP3: the cell's own lights (src/gl/celllights.h)
	// lane GAPFIX1: the game's deferred lights read only the spec scale (pool1 findings 5); blended draws are forward and keep the colour
	cellSpecTint = ( ( alphaFlags & 8 ) != 0 || ( cellRed & 262144 ) != 0 ) ? specColor : vec3( 1.0 );	// WW_CELL_SPEC_RED=tint
	if ( cellOn )
		color.rgb = cellLit( color.rgb, albedo, normal, -ViewDir, V, specMask, cellSpecTint, alphaR, kSmith,
		                     emissive * glowScaleSRGB, cubeK );
	vec3 cellSsrScene = color.rgb * color.rgb;	// lane SSR1: what the reflections' march samples (probe 60)
	if ( cellOn && cellProbe == 60 ) {
		// lane SSGI2: the glow's share of that (cellLit adds it after the root; c^2 - (c - e)^2), coverage in a
		vec3 e = max( emissive * glowScaleSRGB, vec3( 0.0 ) );
		vec3 c = ( !cellInterior && !cellHasDir ) ? cellPreLit : color.rgb;
		vec3 c0 = max( c - e, vec3( 0.0 ) );
		vec3 em = clamp( c * c - c0 * c0, vec3( 0.0 ), cellSsrScene );
		cellEmitOut = vec4( em, dot( em, vec3( 0.2126, 0.7152, 0.0722 ) ) > 1.0e-6 ? 1.0 : 0.0 );
	}
	// lane AO1: the ambient obscurance on the whole lit colour (linear, so its root here), before the fog
	if ( cellOn )
		color.rgb *= sqrt( cellAoFactor() );
#endif

	// lane FOG1 (Lookdev weather fog): this colour is sqrt of linear light (tonemap squares it),
	// so the fog runs on its square, before the exposure and the curve
#ifdef WW_FOG
	if ( fogOn || wwApOn() )	// lane SKYA: the sky's aerial perspective with the fog off too
		color.rgb = sqrt( max( wwFog( color.rgb * color.rgb, -ViewDir ), vec3( 0.0 ) ) );
#endif
#ifdef WW_CELLLIGHTS
	// lane IMGS1: the cell's imagespace in place of the viewer's curve; probe 6 writes the linear colour raw
	vec3 cellHdr = color.rgb * color.rgb;
	if ( cellOn && cellIsOn )
		color.rgb = cellIsLinear ? cellHdr : cellImageSpace( color.rgb );	// lane HDR1: linear into the HDR frame
	else
#endif
	color.rgb = tonemap( color.rgb );

	if ( doRefraction ) {
		// sample the already-rendered scene behind the shape, displaced by the
		// (normal-mapped) surface normal - glass / heat-haze preview. The base
		// texture of a refraction mesh is a distortion/noise map (not a diffuse
		// colour), so it must NOT tint the result: show the clean refracted
		// background. (Per-texel alpha blending garbled the render because the
		// base texture is not opacity, so it is deliberately not applied here.)
		vec2	suv = ( gl_FragCoord.xy - vec2( viewportDimensions.xy ) ) / vec2( viewportDimensions.zw );

		/* Every route that scales the effect, multiplied together:
		 *
		 *  - normal.xy is the eye-space shading normal, so the NORMAL MAP is
		 *    what decides where each texel of the background is fetched from;
		 *    a flat-normal region samples straight through and shows the scene
		 *    unchanged.
		 *  - refractionStrength is the property's Refraction Strength, static
		 *    or driven per frame by a BSLightingShaderPropertyFloatController
		 *    on Controlled Variable 0.
		 *  - C.a is the effective per-vertex alpha (1.0 when the mesh has no
		 *    vertex colours, because vertexColorOverride replaces it), so an
		 *    authored alpha-0 region distorts nothing and alpha-1 distorts
		 *    fully.
		 */
		float	rStrength = clamp( refractionStrength, 0.0, 1.0 ) * clamp( C.a, 0.0, 1.0 );

		/* Maximum displacement is a fraction of the viewport HEIGHT.
		 *
		 * Reading Refraction Strength as a fraction of the whole viewport threw
		 * the source hundreds of pixels into unrelated empty background and drew
		 * a giant dark silhouette; capping it at 8 screen pixels fixed that and
		 * then left nothing to see, because 8 px of a mostly flat backdrop is
		 * invisible and 8 px means something different on every monitor. A
		 * fraction of the height keeps the distortion LOCAL, and keeps it
		 * looking the SAME at every resolution — which is what
		 * resolution-independent has to mean for a screen-space effect.
		 *
		 * The x term converts that height fraction into u, so the displacement
		 * is isotropic instead of being stretched by the aspect ratio.
		 */
		const float refractionMaxScreenFraction = 0.05;
		vec2	roffs = normal.xy * rStrength * refractionMaxScreenFraction
			* vec2( float( viewportDimensions.w ) / float( max( viewportDimensions.z, 1 ) ), 1.0 );
		vec3	bg = texture( RefractionSrc, clamp( suv + roffs, vec2( 0.001 ), vec2( 0.999 ) ) ).rgb;
		color.rgb = bg;
		color.a = 1.0;
	}

	fragColor = color;
#ifdef WW_CELLLIGHTS
	if ( cellOn && cellProbe > 0 )
		fragColor = cellProbe == 6 ? vec4( cellHdr * cellProbeGain, color.a ) : vec4( cellProbeOut( normal, -ViewDir, alphaR, kSmith ), 1.0 );
	if ( cellOn && cellProbe >= 50 && cellProbe <= 53 )	// lane CUBE1
		fragColor = vec4( cellProbe == 50 ? clamp( cubeK * 0.25, 0.0, 1.0 ) : cellCubeProbe( offset, normal ), 1.0 );
	if ( cellOn && cellProbe == 66 )	// lane REFLPROBE1: the boxes' weight, the camera probe's share, the added light
		fragColor = vec4( cellReflW, cellReflFall, clamp( dot( cellReflAdd, vec3( 1.0 / 3.0 ) ), 0.0, 1.0 ), 1.0 );
	if ( cellOn && cellProbe == 80 )	// lane GICAL1 (80; 81-89 kept for it): the albedo as drawn, texture gamma space
		fragColor = vec4( clamp( albedo, 0.0, 1.0 ), 1.0 );
	if ( cellOn && cellProbe == 81 )	// lane GICAL1: the same, blends and decals composited over it with their own alpha
		fragColor = vec4( clamp( albedo, 0.0, 1.0 ), color.a );
	if ( cellOn && cellPass > 0 && cellProbe == 0 )	// lane PROBEVIEW1: the PRTP band's Pass
		fragColor = vec4( cellPassOut( cellWorldPos( -ViewDir ), cellWorldDir( normal ) ), 1.0 );
#endif
#ifdef WW_FOG
	vec3 fogProbeOut;
	if ( wwFogProbe( -ViewDir, fogProbeOut ) )
		fragColor = vec4( fogProbeOut, 1.0 );
#endif
#ifdef WW_SUNSHADOW
	vec3 csmProbeOut;	// lane SUNCELL1: the CSM1 probe views (WW_CSM_PROBE) on the cell-lit legacy draw
	if ( wwSunShadowProbe( -ViewDir, fragColor.rgb, csmProbeOut ) )
		fragColor = vec4( csmProbeOut, 1.0 );
#endif
#ifdef WW_CELLLIGHTS
	if ( cellOn && cellProbe == 20 ) {
		fragColor = cellAoPassOut( normal, -ViewDir );	// lane AO1
		if ( cellAoEarly == 2 )
			fragColor.w *= 1.001;	// lane PERF1 red: full shading runs (the old cost), and the gate must see the mark
	}
	if ( cellOn && ( cellProbe == 60 || cellProbe == 61 ) )
		fragColor = cellSsrProbeOut( cellSsrScene );	// lane SSR1
#endif
	/* lane TERRBLEND1: a blended-ground pass (src/cellsplat.h). The vertex alpha
	 * is this pass's SHARE of the engine's weighted sum: an opaque base is scaled
	 * by it, a layer is ADDED at it (SRC_ALPHA, ONE). Never the texture's alpha, and
	 * the RAW vertex alpha: vertexColorOverride may force C.a to 1. */
	/* water2fix: never over the data passes -- the obscurance pass (20: normal + view depth) and the
	 * reflections' scene pass (60/61): the ground wrote depth 1 there and the water SSR march died on it */
	bool splatData = false;
#ifdef WW_CELLLIGHTS
	splatData = cellOn && ( cellProbe == 20 || cellProbe == 60 || cellProbe == 61 );
#endif
	if ( landSplat && !splatData ) {
		/* lane ALLON2 (bungo's thin dark lines along the ground's edges, 21.png): with MSAA a pixel on the
		 * OUTER edge of a layer's quads is shaded once at its centre, which lies outside every triangle of
		 * that layer, so every input is extrapolated past the edge -- the share below zero, and the lit
		 * colour can go non-finite. The blend then adds NaN x 0 = NaN and the resolve drops the whole
		 * pixel to black: one dark render pixel along every place a layer's quads stop. A share at or
		 * below zero adds nothing, so it is written as exactly nothing; a non-finite colour likewise.
		 * Red WW_TERRBLEND_RED=edgepass: the old output. Probe WW_TERRBLEND_PROBE=edge: such pixels green. */
		float	w = rawVertexAlpha;
		bool	bad = any( isnan( fragColor.rgb ) ) || any( isinf( fragColor.rgb ) ) || isnan( w ) || isinf( w );
		if ( landSplatDbg == 2 ) {
			fragColor = bad ? vec4( 0.0, 1.0, 0.0, 1.0 ) : ( ( alphaFlags & 8 ) != 0 ? vec4( 0.0 ) : vec4( 0.0, 0.0, 0.0, 1.0 ) );
		} else if ( landSplatDbg == 0 && ( bad || ( ( alphaFlags & 8 ) != 0 && !( w > 0.0 ) ) ) ) {
			fragColor = ( alphaFlags & 8 ) != 0 ? vec4( 0.0 ) : vec4( 0.0, 0.0, 0.0, 1.0 );
		} else if ( ( alphaFlags & 8 ) != 0 )
			fragColor.a = clamp( rawVertexAlpha, 0.0, 1.0 );
		else
			fragColor = vec4( fragColor.rgb * clamp( rawVertexAlpha, 0.0, 1.0 ), 1.0 );
	}
}
