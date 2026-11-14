// HLSL source of features/clouds/lookdev_clouds.frag (fragment shader), lane HLSLSRC2 (notes/hlslsrc2/tools/glsl2hlsl.py): one readable file for every
// variant, the GLSL's comments, names and order kept. Loose uniforms are the LooseUniforms cbuffer (b31).
// variants: #if none (the loader's defines; res/shaders/hlsl/variants.txt lists the sets built)
// profile: ps_6_0
#include "CloudsBindingsVk.hlsli"
#define WW_HLSL_SRC 1	// the shared .hlsli files give their readable half
// inlined (no readable .hlsli yet): uniforms.glsl, lookdev_output.glsl
/* Lookdev cloud layer (lane PBRWX1, spec_clouds.md s2.4):
 * rgb = tex.rgb (sRGB-decoded) x vertex rgb x layer colour (linear) x SkyScale,
 * a = tex.a x vertex alpha x layer alpha; SRC_ALPHA / ONE_MINUS_SRC_ALPHA in
 * display space. probe: left half the shaded colour, right half the alpha as
 * grey (raw, no view transform). */

// uniforms.glsl inlined here: its .hlsli has no readable half yet (lane HLSLSRC1)
cbuffer globalUniforms : register(b0, space0)
{
	row_major float3x3	viewMatrix : packoffset(c0);					// rotation part of view transform
	row_major float3x3	envMapRotation : packoffset(c3);				// view space to environment map
	row_major float4x4	projectionMatrix : packoffset(c6);
	float4	lightSourcePosition[3] : packoffset(c10);
	float4	lightSourceDiffuse[3] : packoffset(c13);
	float4	lightSourceAmbient : packoffset(c16);
	float	toneMapScale : packoffset(c17);				// 1.0 = full tone mapping
	float	brightnessScale : packoffset(c17.y);
	float	glowScale : packoffset(c17.z);
	float	glowScaleSRGB : packoffset(c17.w);
	int4	viewportDimensions : packoffset(c18);			// X, Y, width, height
	uint	doSkinning : packoffset(c19);
	int	sceneOptions : packoffset(c19.y);
	int	cubeBgndMipLevel : packoffset(c19.z);
	int	sfParallaxMaxSteps : packoffset(c19.w);
	float	sfParallaxScale : packoffset(c20);
	float	sfParallaxOffset : packoffset(c20.y);
	float	unusedUniform1 : packoffset(c20.z);
	float	unusedUniform2 : packoffset(c20.w);
};
// end of uniforms.glsl

Texture2D<float4> Tex : register(t0, space0); SamplerState _Tex_sampler : register(s0, space0);
Texture2D<float4> Tex2 : register(t1, space0); SamplerState _Tex2_sampler : register(s1, space0);	// lane WEATHERBLEND2: during a change Tex = the left weather's layer, Tex2 the new one's
#define WW_LOOSE_TYPES 1
#include "../sky/atmo_common.hlsli"
#undef WW_LOOSE_TYPES
cbuffer LooseUniforms : register(b31, space0)
{
#define WW_LOOSE_UNIFORMS 1
#include "../sky/atmo_common.hlsli"
#undef WW_LOOSE_UNIFORMS
	float texBlend;	// pct; 0 = Tex alone
	float3 colLin;
	float alpha;
	float skyScale;
	bool probe;
	float sceneExposure;
	int viewTransform;
	int ldLtOn;
	float4 ldLtSrc;
	float4 ldLtPrm;
	float3 ldLtCol;
	float4 ldLtCam;	// the camera, world x, y km, height km; w = the deck's height km
	/* lane WXBAND (WEATHER-HORIZON-BAND): the sky's aerial perspective to the deck, as cloud_march.frag's cvApOn:
	 * tair = T(cam, mu) / T(deck, mu1) on the Low base's shell; drawn as alpha a x tm, colour c x tair / tm (tm = max
	 * channel), i.e. a x tair x c + (1 - a x tm) x the sky behind. ldApOn 0 (red WW_LOOKDEV_RED=cloudnoap, no physical
	 * sky this frame, camera above the deck): the layer as before */
	int ldApOn;
	float2 ldApR;	// km from the planet's centre: the camera, the deck's base
	/* MODULES1 10c: the Fusion Shaders cloud scatter law on the layer (cloudfusionlaw.h): a lobe toward the key light,
	 * a rim term on the cover's edge + a body term on the cover, capped, as a share of the layer's own lit luminance
	 * x the tint (no light of its own; the layer's blend weighs it by the alpha). ldFsOn 0 (red
	 * WW_LOOKDEV_RED=cloudnoscatter, the light under the gate, a class with no response): the layer as before */
	int ldFsOn;
	float4 ldFsSun;	// toward the key light (world), the lobe's g
	float4 ldFsRb;	// rim, body, the cap
	float3 ldFsTint;
};


static float4 vcol;
static float2 uv;
static float2 uv2;
static float2 ndc;
/* lane LIGHTNING2 (LIGHTNINGSKY1): a storm's flash on the layer (Volume Clouds Off / Low), the volume's strike: the texel
 * is lit as the Low band's base where its direction meets the deck (ldLtCam.w km, one extinction length in), the field
 * of lightning_field.glsl x ldLtCol (the flash's colour x level x power, the units of colLin x skyScale). ldLtOn 0 (no
 * storm, between flashes, WW_LIGHTNING_SKY off / red) skips it: the layer as before. */
static float3 wdir;
Texture2D<float4> ldTrans : register(t2, space0); SamplerState _ldTrans_sampler : register(s2, space0);	// the sky's transmittance LUT

static float4 fragColor;

// lookdev_output.glsl inlined here: its .hlsli has no readable half yet (lane HLSLSRC1)
// Lookdev output path (lane PBRR2B): a COPY of pbrm_default.frag's decode,
// sRGB encode, AgX, PBR Neutral and studioOutput, so the lookdev background and
// ground go out exactly as the PBR program does. A copy rather than a move:
// pbr_r2a_gates.sh's red "grey" patches linearToSrgb inside pbrm_default.frag.
// Keep the two in step (owed: one shared include once that red is re-pointed).
// Needs: uniform float sceneExposure; uniform int viewTransform;

float3 srgbToLinear( float3 c )
{
	return lerp( c / 12.92, pow( ( c + 0.055 ) / 1.055, (float3)( 2.4 ) ), step( (float3)( 0.04045 ), c ) );
}

// the sRGB OETF: the encode is here, never GL_FRAMEBUFFER_SRGB (docs s8)
float3 linearToSrgb( float3 c )
{
	c = clamp( c, 0.0, 1.0 );
	return lerp( c * 12.92, 1.055 * pow( c, (float3)( 1.0 / 2.4 ) ) - 0.055, step( (float3)( 0.0031308 ), c ) );
}

// AgX, Benjamin Wrensch's minimal fit (sRGB display out, the "base" look)
float3 agxContrast( float3 x )
{
	float3 x2 = x * x;
	float3 x4 = x2 * x2;
	return 15.5 * x4 * x2 - 40.14 * x4 * x + 31.96 * x4 - 6.868 * x2 * x + 0.4298 * x2 + 0.1191 * x - 0.00232;
}

float3 agxDisplay( float3 c )
{
	const float3x3 agxMat = float3x3( 0.842479062253094, 0.0423282422610123, 0.0423756549057051,
	                          0.0784335999999992, 0.878468636469772, 0.0784336,
	                          0.0792237451477643, 0.0791661274605434, 0.879142973793104 );
	const float3x3 agxMatInv = float3x3( 1.19687900512017, -0.0528968517574562, -0.0529716355144438,
	                             -0.0980208811401368, 1.15190312990417, -0.0980434501171241,
	                             -0.0990297440797205, -0.0989611768448433, 1.15107367264116 );
	const float minEv = -12.47393;
	const float maxEv = 4.026069;
	c = mul(max( c, (float3)( 1e-10 ) ), agxMat);
	c = clamp( log2( c ), minEv, maxEv );
	c = ( c - minEv ) / ( maxEv - minEv );
	c = agxContrast( c );
	return clamp( mul(c, agxMatInv), 0.0, 1.0 );
}

// Khronos PBR Neutral (linear in, linear out)
float3 pbrNeutral( float3 color )
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
	return lerp( color, (float3)( newPeak ), g );
}

float3 studioOutput( float3 c )
{
	c = max( c, (float3)( 0.0 ) ) * sceneExposure;
	if ( viewTransform == 3 )
		return c;	// lane SKYPIC1: the sky captured for the probes, light into a float target (never a view setting)
	if ( viewTransform == 1 )
		return agxDisplay( c );
	if ( viewTransform == 2 )
		return linearToSrgb( pbrNeutral( c ) );
	return linearToSrgb( c );	// Standard = "tonemap None"
}
// end of lookdev_output.glsl
#include "lightning_field.hlsli"
#include "../sky/atmo_common.hlsli"

void frag_main()
{
	float4 t = Tex.Sample( _Tex_sampler, uv );
	if ( texBlend > 0.0 )
		t = lerp( t, Tex2.Sample( _Tex2_sampler, uv2 ), texBlend );
	float3 c = srgbToLinear( t.rgb ) * vcol.rgb * colLin * skyScale;
	if ( ldLtOn != 0 ) {
		float3 d = normalize( wdir );
		float h = ldLtCam.w - ldLtCam.z;
		if ( d.z > 0.02 && h > 0.0 ) {
			float3 p = float3( ldLtCam.xy + d.xy * ( h / d.z ), ldLtCam.w );
			c += srgbToLinear( t.rgb ) * vcol.rgb * ldLtCol * ldLtField( ldLtSrc, ldLtPrm, p, d );
		}
	}
	float a = t.a * vcol.a * alpha;
	if ( ldFsOn != 0 ) {
		float edge = clamp( length( 2.0 * float2( ddx( a ), ddy( a ) ) ) * 8.0, 0.0, 1.0 );
		float g = ldFsSun.w, dn = 1.0 + g * g - 2.0 * g * dot( normalize( wdir ), ldFsSun.xyz );
		float phase = ( 1.0 - g * g ) / ( 12.566371 * dn * sqrt( max( dn, 1e-6 ) ) );
		float lum = dot( srgbToLinear( t.rgb ) * vcol.rgb * colLin, float3( 0.2126, 0.7152, 0.0722 ) ) * skyScale;
		c += ldFsTint * ( lum * min( ( ldFsRb.x * edge + ldFsRb.y * a ) * phase, ldFsRb.z ) );
	}
	if ( ldApOn != 0 ) {
		float3 d = normalize( wdir );
		float r0 = ldApR.x, R1 = ldApR.y, mu = d.z;
		float s = -r0 * mu + sqrt( max( 0.0, r0 * r0 * ( mu * mu - 1.0 ) + R1 * R1 ) );
		float r1 = sqrt( r0 * r0 + s * s + 2.0 * r0 * mu * s ), mu1 = ( r0 * mu + s ) / r1;
		float3 tair = clamp( atTrans( ldTrans, _ldTrans_sampler, r0, mu ) / max( atTrans( ldTrans, _ldTrans_sampler, r1, mu1 ), (float3)( 1e-6 ) ), 0.0, 1.0 );
		float tm = max( tair.r, max( tair.g, tair.b ) );
		c *= tair / max( tm, 1e-6 );
		a *= tm;
	}
	if ( probe ) {
		fragColor = ndc.x < 0.0 ? float4( studioOutput( c ), 1.0 ) : float4( (float3)( a ), 1.0 );
		return;
	}
	fragColor = float4( studioOutput( c ), a );
}

struct SPIRV_Cross_Input
{
    float4 vcol : TEXCOORD0;
    float2 uv : TEXCOORD1;
    float2 uv2 : TEXCOORD2;
    float2 ndc : TEXCOORD3;
    float3 wdir : TEXCOORD4;
};

struct SPIRV_Cross_Output
{
    float4 fragColor : SV_Target0;
};

SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    vcol = stage_input.vcol;
    uv = stage_input.uv;
    uv2 = stage_input.uv2;
    ndc = stage_input.ndc;
    wdir = stage_input.wdir;
    frag_main();
    SPIRV_Cross_Output stage_output;
    stage_output.fragColor = fragColor;
    return stage_output;
}
