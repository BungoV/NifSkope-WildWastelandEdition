#version 410 core

#include "uniforms.glsl"

// PBRM (PBR Material Editor) metallic/roughness path.
//
// The BRDF is the editor's, ported from materialpreviewwidget.cpp so the two
// tools agree:
//   f0    = mix( dielectric, base, metal )
//   spec  = D_GGX(NoH,a) * G1(NoL,k) * G1(NoV,k) * F / (4 NoL NoV)
//   diff  = (1-F) * (1-metal) * base / PI
//   a     = rough^2      k = (rough+1)^2/8
//
// Not ported: multiScatter energy compensation (needs a BRDF LUT), the editor's
// 32-sample diffuse irradiance convolution, vegetation/displacement/parallax and
// the surface-state system. Ambient here is the scene's A term plus the existing
// cube map, which is what NifSkope has to offer.

uniform sampler2D BaseMap;      // sRGB RGB, A opacity
uniform sampler2D NormalMap;    // linear RG (+B height, A curvature)
uniform sampler2D RmaosMap;     // R rough, G metal, B AO, A dielectric F0/porosity
uniform sampler2D EmissiveMap;  // sRGB RGB, A intensity mask
uniform samplerCube CubeMap;

// Constants stand in wherever the corresponding channel is overridden. These
// mirror the reader's resolved values (src/io/pbrmfile.h).
uniform vec3 pbrBaseColor;
uniform float pbrOpacity;
uniform float pbrRoughness;
uniform float pbrMetallic;
uniform float pbrAo;
uniform float pbrF0;
uniform float pbrNormalStrength;
uniform vec3 pbrEmissiveColor;
uniform float pbrEmissiveIntensity;

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

uniform bool hasCubeMap;
uniform float envReflection;

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

float D_GGX( float NdotH, float alphaR )
{
	float a2 = alphaR * alphaR;
	float d = NdotH * NdotH * ( a2 - 1.0 ) + 1.0;
	return a2 / max( M_PI * d * d, 0.0001 );
}

float G1( float NdotX, float k )
{
	return NdotX / max( NdotX * ( 1.0 - k ) + k, 0.0001 );
}

vec3 fresnelSchlick( float VdotH, vec3 F0 )
{
	return F0 + ( vec3( 1.0 ) - F0 ) * pow( clamp( 1.0 - VdotH, 0.0, 1.0 ), 5.0 );
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

void main()
{
	vec2 offset = texCoord.st * uvScale + uvOffset;

	// --- base colour / opacity ---
	vec3 base = pbrBaseColor;
	float op = pbrOpacity;
	if ( hasFeature( F_BASECOLOR ) ) {
		vec4 bc = texture( BaseMap, offset );
		base = bc.rgb * pbrBaseColor;
		// Only treat the texture's alpha as coverage when the material says so.
		// Taking it unconditionally discarded every fragment on legacy materials,
		// whose diffuse alpha is frequently zero or unrelated to opacity.
		if ( hasFeature( F_OPACITY ) )
			op = bc.a * pbrOpacity;
	}
	base *= C.rgb;

	vec4 color = vec4( base, 1.0 );
	if ( alphaFlags > 0 ) {
		float a = C.a * op * alpha;
		int m = ( a < alphaThreshold ? 0x2B2B : ( a > alphaThreshold ? 0x7171 : 0x4D4D ) );
		if ( ( m & ( 1 << alphaFlags ) ) == 0 )
			discard;
		if ( ( alphaFlags & 8 ) != 0 )
			color.a = a;
	}

	// --- normal ---
	vec3 normal = vec3( 0.0, 0.0, 1.0 );
	if ( hasFeature( F_NORMAL ) ) {
		vec4 nm = texture( NormalMap, offset );
		normal.rg = ( nm.rg * 2.0 - 1.0 ) * pbrNormalStrength;
		// B is height when the material says so, never a normal component — so
		// the Z is always reconstructed rather than read.
		normal.b = sqrt( max( 1.0 - dot( normal.rg, normal.rg ), 0.0 ) );
	}
	normal = normalize( btnMatrix_norm * normal );
	if ( !gl_FrontFacing )
		normal *= -1.0;

	// --- surface response ---
	float rough = pbrRoughness;
	float metal = pbrMetallic;
	float ao = pbrAo;
	float dielectric = pbrF0;
	if ( hasFeature( F_RMAOS ) ) {
		vec4 rm = texture( RmaosMap, offset );
		if ( hasFeature( F_ROUGHNESS ) ) rough = rm.r;
		if ( hasFeature( F_METALLIC ) )  metal = rm.g;
		if ( hasFeature( F_AO ) )        ao    = rm.b;
		// Alpha is F0 only while the material says it carries Dielectric F0; the
		// reader clears this bit when it carries porosity instead.
		if ( hasFeature( F_F0 ) )        dielectric = rm.a;
	}
	rough = clamp( rough, 0.02, 1.0 );
	metal = clamp( metal, 0.0, 1.0 );

	vec3 L = normalize( LightDir );
	vec3 V = ViewDir_norm;
	vec3 H = normalize( L + V );
	vec3 R = reflect( -V, normal );

	float NdotL = max( dot( normal, L ), 0.0 );
	float NdotL0 = max( NdotL, 1e-7 );
	float NdotH = max( dot( normal, H ), 1e-7 );
	float NdotV = max( dot( normal, V ), 1e-7 );
	float VdotH = max( dot( V, H ), 1e-7 );

	// Metals tint their reflection with the base colour; dielectrics keep the
	// scalar F0. This is the one place FO4's spec/gloss path cannot go, since it
	// has no metalness channel at all.
	vec3 f0 = mix( vec3( dielectric ), base, metal );
	vec3 F = fresnelSchlick( VdotH, f0 );

	float alphaR = rough * rough;
	float kSmith = ( rough + 1.0 ) * ( rough + 1.0 ) * 0.125;
	vec3 spec = vec3( D_GGX( NdotH, alphaR ) * G1( NdotL0, kSmith ) * G1( NdotV, kSmith ) )
	            * F / max( 4.0 * NdotL0 * NdotV, 0.001 );

	// Lambert with the specular energy removed, and metals have no diffuse.
	vec3 diff = ( vec3( 1.0 ) - F ) * ( 1.0 - metal ) * base / M_PI;

	color.rgb = ( diff + spec ) * D.rgb * NdotL;

	// --- ambient ---
	// AO belongs on ambient, not on direct light: occlusion describes what the
	// surface cannot see of the environment, and applying it to a direct lobe
	// double-darkens contact shadows.
	color.rgb += A.rgb * base * ( 1.0 - metal ) * ao;

	if ( hasCubeMap ) {
		vec3 cube = textureLod( CubeMap, reflMatrix * R, rough * 8.0 ).rgb;
		// Split-sum without the BRDF LUT: f0 stands in for the scale term, which
		// over-darkens grazing angles on rough metals. Honest approximation, not
		// the editor's split-sum.
		color.rgb += cube * envReflection * f0 * ao;
	}

	// --- emissive ---
	if ( pbrEmissiveIntensity > 0.0 ) {
		vec3 emissive = pbrEmissiveColor * pbrEmissiveIntensity;
		if ( hasFeature( F_EMISSIVE ) ) {
			vec4 em = texture( EmissiveMap, offset );
			emissive *= em.rgb * em.a;
		}
		color.rgb += emissive * glowScaleSRGB;
	}

	color.rgb = tonemap( color.rgb );

	fragColor = color;
}
