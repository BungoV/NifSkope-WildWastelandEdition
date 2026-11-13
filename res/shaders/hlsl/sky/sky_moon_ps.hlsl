// HLSL source of features/sky/sky_moon.frag (fragment shader), lane HLSLSRC3 (notes/hlslsrc3/tools/glsl2hlsl.py): one readable file for every
// variant, the GLSL's comments, names and order kept. Loose uniforms are the LooseUniforms cbuffer (b31).
// variants: #if none (the loader's defines; res/shaders/hlsl/variants.txt lists the sets built)
// profile: ps_6_0
#include "SkyBindingsVk.hlsli"
#define WW_HLSL_SRC 1	// the shared .hlsli files give their readable half
// inlined (no readable .hlsli yet): uniforms.glsl, lookdev_output.glsl
static float4 gl_FragCoord;
/* lane SKYDISC1: the moon as a lit globe of 0.26 deg half-angle (gl/skydisc.h). Per pixel: the world ray against the
 * sphere, the surface point's selenographic longitude/latitude (NASA SVS CGI Moon Kit albedo + relief, loose files
 * Textures/Sky/Moon), Lommel-Seeliger mu0 / (mu0 + mu) from the real sun direction (so the phase is the light's), a
 * faint earthshine, through the air toward it (atTrans). Over it the sky in front, the same LUTs as atmo_sky.frag:
 * the moon hides the stars, not the air. Premultiplied coverage; nothing outside the disc (no glow, no outline). */

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
#define WW_LOOSE_TYPES 1
#include "atmo_common.hlsli"
#undef WW_LOOSE_TYPES
cbuffer LooseUniforms : register(b31, space0)
{
#define WW_LOOSE_UNIFORMS 1
#include "atmo_common.hlsli"
#undef WW_LOOSE_UNIFORMS
	float2 atViewSize;
	float4 atRect;	// viewport x, y, 1/w, 1/h
	float atCamR;
	float3 atSunW;	// the sky's sun (its LUT)
	float3 atMoonW;
	float atSunK;
	float atMoonK;
	float atTintOn;
	float3 atTint[17];
	float2 atRecNight;	// MODULES1 R7: atmo_sky.frag's twin
	// MODULES1 R7 night sky d: the record's dome's share of the sky (0 = none, the red WW_NIGHTAIR_RED=1): over it the globe
	// stands in the dome, with none of the physical sky's air; the cell's air (cell_volsky.frag) is then its one extinction
	float atDomeAir;
	float3 moonD;	// TO the moon's centre (world, unit)
	float3 moonS;	// TO the sun from the moon (world, unit): the phase
	float3 bodyX;	// the moon's axes: x = longitude 0, y = 90 east, z = the spin pole
	float3 bodyY;
	float3 bodyZ;
	float sinR;	// sin of the half-angle
	float pxAng;	// one pixel's angle (radians)
	float albedoInv;	// 1 / the stored albedo's linear mean
	float sunTerm;	// scene units: R x KS(alpha) / the LS disc mean at alpha
	float earthTerm;	// scene units: the earthshine
	float alpha;	// the game clock's fade x the row's leak multiplier
	float sceneExposure;
	int viewTransform;
};
#include "atmo_common.hlsli"

Texture2D<float4> atAlbedo : register(t0, space0); SamplerState _atAlbedo_sampler : register(s0, space0);	// Moon_d.dds (BC1, sRGB values sampled raw)
Texture2D<float4> atRelief : register(t1, space0); SamplerState _atRelief_sampler : register(s1, space0);	// Moon_n.dds (BC5: R = east slope, G = north slope)
Texture2D<float4> atTransTex : register(t2, space0); SamplerState _atTransTex_sampler : register(s2, space0);
Texture2D<float4> atSunView : register(t3, space0); SamplerState _atSunView_sampler : register(s3, space0);
Texture2D<float4> atMoonView : register(t4, space0); SamplerState _atMoonView_sampler : register(s4, space0);

static float2 uv;

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

static const float kTau = 6.28318531;

float3 atLookup( Texture2D<float4> lut, SamplerState _lut_sampler, float3 d, float3 l )	// atmo_sky.frag's twin
{
	float2 a = d.xy, b = l.xy;
	float la = length( a ), lb = length( b );
	float cosAz = ( la > 1e-5 && lb > 1e-5 ) ? dot( a / la, b / lb ) : 1.0;
	return lut.Sample( _lut_sampler, atSkyDirToUv( atCamR, d.z, cosAz, atViewSize ) ).rgb;
}

float3 atWeatherTint( float3 c, float elevDeg )	// atmo_sky.frag's twin (gl/atmomath.h weatherTint)
{
	float t = sqrt( clamp( elevDeg, 0.0, 90.0 ) / 90.0 ) * 16.0;
	int i = min( int( t ), 15 );
	float3 w = lerp( atTint[i], atTint[i + 1], t - float( i ) );
	const float3 y = float3( 0.2126, 0.7152, 0.0722 );
	float lw = dot( w, y );
	return lw > 1e-8 ? w * ( dot( c, y ) / lw ) : c;
}

// the moon's radiance along the ray qs off the centre toward rad, filtered over 2 pr x the radius across and the arc
// that span covers radially
float3 moonShade( float3 rad, float qs, float pr )
{
	float3 ds = moonD * sqrt( 1.0 - qs * qs ) + rad * qs;
	float h = sqrt( max( 0.0, sinR * sinR - qs * qs ) );
	float3 n = normalize( ds * ( dot( ds, moonD ) - h ) - moonD );	// the sphere normal
	float lon = atan2( dot( n, bodyY ), dot( n, bodyX ) );
	float lat = asin( clamp( dot( n, bodyZ ), -1.0, 1.0 ) );
	float2 tuv = float2( lon / kTau + 0.5, 0.5 - lat / kPi );
	float3 east = cross( bodyZ, n );
	float le = length( east );
	east = le > 1e-4 ? east / le : bodyX;
	float3 north = cross( n, east );
	// the pixel's footprint on the unit sphere: radially the arc its q span covers (wide at the limb), across it one
	// pixel (one mip for the whole disc aliased at the limb)
	float qr = qs / sinR;
	float arcR = max( asin( min( qr + pr, 1.0 ) ) - asin( max( qr - pr, -1.0 ) ), 2.0 * pr );
	float3 tr = rad - dot( rad, n ) * n;
	float lr = length( tr );
	tr = lr > 1e-6 ? tr / lr : east;
	float3 tt = cross( n, tr );
	float2 uvPer = float2( 1.0 / ( kTau * max( le, 1e-3 ) ), -1.0 / kPi );
	float2 gR = float2( dot( tr, east ), dot( tr, north ) ) * uvPer * arcR;
	float2 gT = float2( dot( tt, east ), dot( tt, north ) ) * uvPer * ( 2.0 * pr );
	float3 alb = srgbToLinear( atAlbedo.SampleGrad( _atAlbedo_sampler, tuv, gR, gT ).rgb ) * albedoInv;
	float2 sl = atRelief.SampleGrad( _atRelief_sampler, tuv, gR, gT ).rg * 2.0 - 1.0;
	// the relief fades to the sphere as the view grazes: at the limb a slope flipped mu and mu0 per texel
	sl *= smoothstep( 0.0, 0.3, max( 0.0, dot( n, -ds ) ) );
	float3 nm = normalize( east * sl.x + north * sl.y + n * sqrt( max( 0.0, 1.0 - dot( sl, sl ) ) ) );
	float lit = clamp( dot( n, moonS ) / 0.01 + 0.5, 0.0, 1.0 );	// the sphere's terminator, not the slopes'
	float mu0 = max( 0.0, dot( nm, moonS ) ) * lit;
	float mu = max( 1e-3, dot( nm, -ds ) );
	return alb * ( sunTerm * mu0 / ( mu0 + mu ) + earthTerm );
}

static const float2 kRgss[4] = { float2( 0.125, 0.375 ), float2( 0.375, -0.125 ), float2( -0.125, -0.375 ), float2( -0.375, 0.125 ) };

void frag_main()
{
	float2 ndc = ( gl_FragCoord.xy - atRect.xy ) * atRect.zw * 2.0 - 1.0;
	float3 vr = normalize( float3( ( ndc.x + projectionMatrix[2][0] ) / projectionMatrix[0][0],
		( ndc.y + projectionMatrix[2][1] ) / projectionMatrix[1][1], -1.0 ) );
	float3 d = normalize( mul(viewMatrix, vr) );
	float b = dot( d, moonD );
	float q = length( cross( d, moonD ) );	// sine of the angle off the centre
	float cov = clamp( ( sinR - q ) / pxAng + 0.5, 0.0, 1.0 ) * clamp( d.z / pxAng + 0.5, 0.0, 1.0 );
	cov *= alpha;
	if ( or( b <= 0.0, cov <= 0.0 ) )
		discard;
	// SKYDISC1B: a limb pixel averages 4 shading points of its covered part (one point past the edge chipped the rim)
	float3 rad = q > 1e-7 ? ( d - b * moonD ) / q : bodyX;
	float3 m = (float3)( 0.0 );
	if ( q < sinR - pxAng ) {
		m = moonShade( rad, q, 0.5 * pxAng / sinR );
	} else {
		float3 tng = cross( moonD, rad );
		float k = 0.0;
		for ( int i = 0; i < 4; i++ ) {
			float3 v = rad * ( q + kRgss[i].x * pxAng ) + tng * ( kRgss[i].y * pxAng );
			float qv = length( v );
			if ( qv < sinR ) {
				m += moonShade( v / qv, qv, 0.25 * pxAng / sinR );
				k += 1.0;
			}
		}
		m = k > 0.0 ? m / k : moonShade( rad, 0.5 * ( q - 0.5 * pxAng + sinR ), 0.25 * pxAng / sinR );
	}
	m *= lerp( atTrans( atTransTex, _atTransTex_sampler, atCamR, d.z ), (float3)( 1.0 ), atDomeAir );
	float3 sky = atSunK * atLookup( atSunView, _atSunView_sampler, d, atSunW );
	if ( atMoonK > 0.0 )
		sky += atMoonK * atLookup( atMoonView, _atMoonView_sampler, d, atMoonW );
	if ( atRecNight.y > 0.0 ) {
		float tn = sqrt( clamp( degrees( asin( clamp( d.z, -1.0, 1.0 ) ) ), 0.0, 90.0 ) / 90.0 ) * 16.0;
		int jn = min( int( tn ), 15 );
		sky = sky * ( 1.0 - atRecNight.x ) + lerp( atTint[jn], atTint[jn + 1], tn - float( jn ) ) * atRecNight.y;
	}
	if ( atTintOn > 0.5 )
		sky = atWeatherTint( sky, degrees( asin( clamp( d.z, -1.0, 1.0 ) ) ) );
	fragColor = float4( studioOutput( sky + m ) * cov, cov );
}

struct SPIRV_Cross_Input
{
    float2 uv : TEXCOORD0;
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
    uv = stage_input.uv;
    frag_main();
    SPIRV_Cross_Output stage_output;
    stage_output.fragColor = fragColor;
    return stage_output;
}
