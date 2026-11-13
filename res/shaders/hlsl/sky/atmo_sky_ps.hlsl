// HLSL source of features/sky/atmo_sky.frag (fragment shader), the hand-kept twin of lane HLSLSRC3: notes/hlsltools/glsl2hlsl.py's
// text with the GLSL's comments, names and order, plus what Vulkan needs of it. It is built into the Vulkan sky
// (src/features/vkframe/vkframe_spirv.sh, vkpresent.cpp skyRun, WW_VKVIEW=1), so four things stay as the old port had
// them and g2h_mods.sh never regenerates it:
//   - LooseUniforms keeps the old member order (vkpresent.h packs b31 by the offsets of sky_view_ps.spv.h);
//   - the texture registers keep the old order (t0 transmittance, t1 ms, t2 sun view, t3 moon view);
//   - acos, atHitsGround and the view ray are spelled as NVIDIA's GL compiler lowers them (tests/vkframe/skyview.sh).
// variants: #if none (the loader's defines; res/shaders/hlsl/variants.txt lists the sets built)
// profile: ps_6_0
#include "SkyBindingsVk.hlsli"
#define WW_HLSL_SRC 1	// the shared .hlsli files give their readable half
// inlined (no readable .hlsli yet): uniforms.glsl, lookdev_output.glsl
static float4 gl_FragCoord;
// lane ATMO1: the physical sky's pixels. A world ray per pixel, the sky-view LUT of the sun (+ the moon's own LUT),
// the sun disc through the transmittance LUT (limb darkening on High and up). Scene units: displayed = pi * k * L,
// k = the weather's Day sunlight luminance / the 45-degree transmittance (src/features/sky/atmosphere.cpp).

// NVIDIA's GL acos is this polynomial (Abramowitz-Stegun 4.4.45), not SPIR-V's Acos (skill ww-vk-image-pass-match)
float AtNvAcos( float x )
{
	float xa = abs( x );
	float r = mad( mad( mad( -0.0187293f, xa, 0.0742610f ), xa, -0.2121144f ), xa, 1.5707288f ) * sqrt( 1.0f - xa );
	return x < 0.0f ? 3.14159265358979f - r : r;
}
#define acos( x ) AtNvAcos( x )

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
// the loose uniforms of this file and of atmo_common.glsl, in the old port's order (the Vulkan side's offsets)
cbuffer LooseUniforms : register(b31, space0)
{
	float3 atHaze;	// the weather fog's haze layer: ground extinction /km, scale height km, plateau top km
	float3 atRay;	// Rayleigh scattering /km (5.802, 13.558, 33.1)e-3
	float atMieS;	// Mie scattering /km (3.996e-3)
	float3 atHazeSpec;	// lane WEATHERRECX1: the haze extinction's (lambda / 550)^-angstrom (1 = grey, before 1.3)
	float3 atHazeAlbedo;	// lane WEATHERRECX1: the haze's albedo at 680 / 550 / 440 nm (3.996 / 4.40 grey before 1.3)
	float atMieE;	// Mie extinction /km (4.40e-3)
	float3 atOzone;	// ozone absorption /km (0.650, 1.881, 0.085)e-3; 0 under the red "noozone"
	float atMsOld;	// atmo_common.glsl: 1 = the red WW_ATMO_RED=msold
	float atG;	// Mie Henyey-Greenstein g (lane WEATHERRECX1: the haze's, atmomath.h hazeOptics)
	float sceneExposure;
	int viewTransform;
	float atCamR;
	float2 atViewSize;
	float3 atTint[17];	// the weather dome's linear color at 90 (i / 16)^2 degrees (gl/atmomath.h tintTable)
	float4 atRect;	// viewport x, y, 1/w, 1/h
	float atPerPixel;	// 1 = march per pixel instead of the sky-view LUT (the LUT's resolution measured against it)
	float atSunK;	// pi * k for the sun
	float2 atMsSize;
	float atMsOn;
	float atGround;
	float3 atSunW;	// world, TO the sun (may be below the horizon)
	float atMoonK;	// pi * k for the moon; 0 = no moon term
	float3 atMoonW;
	/* lane FIX1101A (atmo): the moonless night sky's own light (airglow + zodiacal light + unresolved starlight), per unit
	 * sun irradiance at the top like the LUTs, zenith ~4e-4 cd/m2 (natural dark sky 21-22 V mag / arcsec2, Leinert et al.
	 * 1998); 0 = the red WW_ATMO_RED=nofloor (the sky fell 30-160x under that at sun -18). gl/atmomath.h kNightFloor. */
	float3 atFloor;
	float2 atRecNight;	// MODULES1 R7: the record's night sky: the sky x (1 - x) + the dome's color x y (0 0 = none)
	float atTintOn;	// lane SKYA (ATMOCHECK1): 1 = the Sky Color row's Weather Tint
	/* lane NIGHTHAZE1: UE's height-fog inscattering overrides, last resort for special weathers (weather .recx
	 * atmosphere.inscatterColor / directionalInscatter): a flat colour and a lobe around atDirL (the sun up, else the moon),
	 * scene units, drawn over the haze's opacity along the view. Unset = 0 = skipped */
	float3 atInscatter;
	float3 atDirIns;
	float3 atDirL;
	float atDirExp;
	float atSunSize;	// MODULES1 R6-12 b: the drawn disc's radius over the true one (the Sun and Moon Size row; <= 1 = true)
	float atLimb;	// 1 = limb darkening (High+)
	/* lane FIX1101A (moon, LATE ITEM 24d(2)): the disc's gain through the Volumetric Air (gl/moonlight.h discAirGain): the
	 * air's composite keeps T_air / T_sky of the sky, the bands included, while the sun's light takes only the ground layer;
	 * the disc (and its cap) takes this gain first so that it ends dimmed as its light. <= 0 or unset = 1 */
	float atDiscAir;
	float atDiscHue;	// lane SKYDISC1: 1 = one scale for the whole disc (its centre to the cap): hue and limb kept
	float atDiscCap;	// the disc's display cap (scene units)
};

// GL's r * r * (mu * mu - 1) + kRb * kRb, in NVIDIA's order: the shared form flipped the horizon row's ground test
bool atHitsGround( float r, float mu )
{
	return mu < 0.0f && mad( mu * mu, r * r, mad( r, -r, 40449600.0f ) ) >= 0.0f;
}
#define WW_AT_HITSGROUND_OWN 1
#include "atmo_common.hlsli"

Texture2D<float4> atTransTex : register(t0, space0); SamplerState _atTransTex_sampler : register(s0, space0);
Texture2D<float4> atMsTex : register(t1, space0); SamplerState _atMsTex_sampler : register(s1, space0);	// lane SKY2B (SKYBLUR1): the per-pixel sky marches each pixel (red WW_ATMO_RED=perpixel)
Texture2D<float4> atSunView : register(t2, space0); SamplerState _atSunView_sampler : register(s2, space0);
Texture2D<float4> atMoonView : register(t3, space0); SamplerState _atMoonView_sampler : register(s3, space0);

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

static const float kSunCos = 0.99998918;	// cos 0.2665 deg
static const float kSunSolid = 6.7967e-5;	// 2 pi (1 - cos 0.2665 deg) sr

float3 atLookup( Texture2D<float4> lut, SamplerState _lut_sampler, float3 d, float3 l )
{
	float2 a = d.xy, b = l.xy;
	float la = length( a ), lb = length( b );
	float cosAz = ( la > 1e-5 && lb > 1e-5 ) ? dot( a / la, b / lb ) : 1.0;
	return lut.Sample( _lut_sampler, atSkyDirToUv( atCamR, d.z, cosAz, atViewSize ) ).rgb;
}

// the GLSL twin of gl/atmomath.h weatherTint: the physical luminance in the weather's color at this elevation
float3 atWeatherTint( float3 c, float elevDeg )
{
	float t = sqrt( clamp( elevDeg, 0.0, 90.0 ) / 90.0 ) * 16.0;
	int i = min( int( t ), 15 );
	float3 w = lerp( atTint[i], atTint[i + 1], t - float( i ) );
	const float3 y = float3( 0.2126, 0.7152, 0.0722 );
	float lw = dot( w, y );
	return lw > 1e-8 ? w * ( dot( c, y ) / lw ) : c;
}

void frag_main()
{
	float2 ndc = ( gl_FragCoord.xy - atRect.xy ) * atRect.zw * 2.0 - 1.0;
	// normalize( vr ) and normalize( vr * viewMatrix ) as NVIDIA lowers them: the dot products as nested mads, x first
	float3 vr0 = float3( ( ndc.x + projectionMatrix[2][0] ) / projectionMatrix[0][0],
		( ndc.y + projectionMatrix[2][1] ) / projectionMatrix[1][1], -1.0 );
	float3 vr = vr0 * rsqrt( mad( vr0.z, vr0.z, mad( vr0.y, vr0.y, vr0.x * vr0.x ) ) );
	float3 d0 = float3( mad( vr.z, viewMatrix[0].z, mad( vr.y, viewMatrix[0].y, vr.x * viewMatrix[0].x ) ),
		mad( vr.z, viewMatrix[1].z, mad( vr.y, viewMatrix[1].y, vr.x * viewMatrix[1].x ) ),
		mad( vr.z, viewMatrix[2].z, mad( vr.y, viewMatrix[2].y, vr.x * viewMatrix[2].x ) ) );
	float3 d = d0 * rsqrt( mad( d0.z, d0.z, mad( d0.y, d0.y, d0.x * d0.x ) ) );	// view to world: the transpose of the rotation
	float3 c;
	if ( atPerPixel > 0.5 ) {
		c = atSunK * atSkyMarch( atTransTex, _atTransTex_sampler, atMsTex, _atMsTex_sampler, atMsSize, atMsOn, atGround, atCamR, d, atSunW );
		if ( atMoonK > 0.0 )
			c += atMoonK * atSkyMarch( atTransTex, _atTransTex_sampler, atMsTex, _atMsTex_sampler, atMsSize, atMsOn, atGround, atCamR, d, atMoonW );
	} else {
		c = atSunK * atLookup( atSunView, _atSunView_sampler, d, atSunW );
		if ( atMoonK > 0.0 )
			c += atMoonK * atLookup( atMoonView, _atMoonView_sampler, d, atMoonW );
	}
	if ( atFloor.g > 0.0 ) {
		// the emitting shell at ~90 km seen slant (van Rhijn 1921): brighter toward the horizon, then dimmed by the air
		// below it (the view ray's transmittance: 0 where the ray meets the ground, so no edge at the horizon)
		float sz2 = 1.0 - d.z * d.z;
		float vr = rsqrt( max( 1.0 - 0.97226 * sz2, 0.02 ) );	// (6360 / 6450)^2
		c += atSunK * atFloor * vr * atTrans( atTransTex, _atTransTex_sampler, atCamR, d.z );
	}
	if ( atRecNight.y > 0.0 ) {	// MODULES1 R7: master Off: a crossfade to the record's dome (x = y); On: the dome added (x = 0)
		float tn = sqrt( clamp( degrees( asin( clamp( d.z, -1.0, 1.0 ) ) ), 0.0, 90.0 ) / 90.0 ) * 16.0;
		int jn = min( int( tn ), 15 );
		c = c * ( 1.0 - atRecNight.x ) + lerp( atTint[jn], atTint[jn + 1], tn - float( jn ) ) * atRecNight.y;
	}
	if ( atTintOn > 0.5 )	// the disc below keeps the physical color
		c = atWeatherTint( c, degrees( asin( clamp( d.z, -1.0, 1.0 ) ) ) );
	if ( dot( atInscatter + atDirIns, (float3)( 1.0 ) ) > 0.0 ) {	// NIGHTHAZE1: the overrides, x (1 - the view's transmittance)
		float3 op = (float3)( 1.0 ) - atTrans( atTransTex, _atTransTex_sampler, atCamR, d.z );
		c += ( atInscatter + atDirIns * pow( max( dot( d, atDirL ), 0.0 ), max( atDirExp, 1.0 ) ) ) * op;
	}
	float cs = dot( d, atSunW );
	// R6-12 b: a bigger drawn disc keeps its radiance (it sits at the cap); 1 - cos grows by the square of the radius
	float sunEdge = ( 1.0 - kSunCos ) * ( atSunSize > 1.0 ? atSunSize * atSunSize : 1.0 );
	if ( atSunSize > 1.0 ? ( 1.0 - cs < sunEdge ) : ( cs > kSunCos ) ) {
		float x = clamp( ( 1.0 - cs ) / sunEdge, 0.0, 1.0 );	// ~ (theta / alpha)^2
		float3 limb = (float3)( 1.0 );
		if ( atLimb > 0.5 ) {
			float3 a = float3( 0.397, 0.503, 0.652 );
			limb = pow( (float3)( sqrt( max( 0.0, 1.0 - x ) ) ), a ) * ( a + 2.0 ) * 0.5;
		}
		float3 disc = atSunK * atTrans( atTransTex, _atTransTex_sampler, atCamR, d.z ) * limb / kSunSolid;
		float ga = atDiscAir > 0.0 ? atDiscAir : 1.0;	// lane FIX1101A: the air's gain (the cap capped first, as before)
		if ( atDiscHue > 0.5 ) {
			float3 a = float3( 0.397, 0.503, 0.652 );
			float3 centre = atSunK * atTrans( atTransTex, _atTransTex_sampler, atCamR, atSunW.z ) * ( a + 2.0 ) * 0.5 / kSunSolid;
			float m = max( centre.r, max( centre.g, centre.b ) );
			c += disc * ( m > atDiscCap ? atDiscCap / m : 1.0 ) * ga;
		} else {
			c += min( disc, (float3)( atDiscCap ) ) * ga;
		}
	}
	fragColor = float4( studioOutput( c ), 1.0 );
}

struct SPIRV_Cross_Input
{
    float4 gl_FragCoord : SV_Position;
};

struct SPIRV_Cross_Output
{
    float4 fragColor : SV_Target0;
};

SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    gl_FragCoord = stage_input.gl_FragCoord;
    gl_FragCoord.w = 1.0 / gl_FragCoord.w;
    frag_main();
    SPIRV_Cross_Output stage_output;
    stage_output.fragColor = fragColor;
    return stage_output;
}
