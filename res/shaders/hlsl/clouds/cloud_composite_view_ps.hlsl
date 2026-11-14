// Lane VKFAR2 (row P8, 2026-11-14): the clouds in front of the scene on the present device (vkcloudfront.inc, cvFront 2) compile
// this frozen copy of cloud_composite_ps.hlsl (the view's SPIR-V and kT_cloudfront stay put while the readable source moves).
// HLSL source of features/clouds/cloud_composite.frag (fragment shader), lane HLSLSRC2 (notes/hlslsrc2/tools/glsl2hlsl.py): one readable file for every
// variant, the GLSL's comments, names and order kept. Loose uniforms are the LooseUniforms cbuffer (b31).
// variants: #if none (the loader's defines; res/shaders/hlsl/variants.txt lists the sets built)
// profile: ps_6_0
#include "CloudsBindingsVk.hlsli"
#define WW_HLSL_SRC 1	// the shared .hlsli files give their readable half
// inlined (no readable .hlsli yet): lookdev_output.glsl
#ifndef WW_GLSL_SIZE_Texture2D_float4_
#define WW_GLSL_SIZE_Texture2D_float4_
int2 textureSize(Texture2D<float4> t, int lod) { uint w, h, l; t.GetDimensions(uint(lod), w, h, l); return int2(w, h); }
#endif
static float4 gl_FragCoord;
/* lane CLOUD2: the volume clouds over the sky, as the 2D layers composite (SRC_ALPHA in display space): the history
 * upsampled bilinear, its light un-premultiplied, through the view transform. */

Texture2D<float4> cvHist : register(t0, space0); SamplerState _cvHist_sampler : register(s0, space0);	// rgb = light (premultiplied by the opacity), a = transmittance
cbuffer LooseUniforms : register(b31, space0)
{
	float4 cvRect;	// viewport x, y, 1 / w, 1 / h
	float sceneExposure;
	int viewTransform;
	// lane CLOUD2B: cvFront = 1 draws over the scene after it (the sky had its pass first): only where the clouds' mean depth
	// (cvHistDepth, km) is in front of the scene's (cvScene window depth; cvSceneZ as cloud_march.frag), never over the sky
	int cvFront;
	float4 cvSceneZ;
	float4 cvProj;	// P00, P11, P20, P21
	// lane CLOUD2C: cvFront = 2 is that pass INSIDE the cell's linear frame, before the tone map: the light as it is (no
	// exposure, no view transform), premultiplied (the caller blends ONE, ONE_MINUS_SRC_ALPHA), so the tone map treats the
	// clouds in front of the terrain as it treats the terrain
	/* lane CLOUD6f (RDR2 slide 40: a lightning strike is a point light inside the cloud, added to the scattering): during a
	 * storm's flash the light of a point source inside the Low band reaches the eye from the cloud at its mean depth
	 * (cvHistDepth): (1 - T) x cvLtCol x max(phi + 3 g D grad(phi).d, 0), phi the diffusion Green's function with images
	 * across the band's extrapolated boundaries. No density read; cvLtOn 0 (no storm, between flashes, the switch off)
	 * skips it, so the output is today's. Twin: src/features/clouds/cloudlightning.h fieldAt. */
	int cvLtOn;
	float4 cvLtSrc;	// the strike, km: world x, y, height; w = 1 / delta (1 / km)
	float4 cvLtPrm;	// r0^2 (km^2), the image planes' doubled heights 2 (base - z_b), 2 (top + z_b), 3 g D (km)
	float3 cvLtCol;	// the flash's light: colour x level x power x w / (4 pi) / (4 pi D) x the Low band's tint
	row_major float3x3 cvCam;	// world columns: right, up, back (cloud_march.frag)
	float3 cvCamKm;	// world x, y km, height km
};

Texture2D<float4> cvHistDepth : register(t1, space0); SamplerState _cvHistDepth_sampler : register(s1, space0);
Texture2D<float4> cvScene : register(t2, space0); SamplerState _cvScene_sampler : register(s2, space0);

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

float cvLtField( float3 p, float3 d )
{
	float phi = 0.0, gd = 0.0;
	for ( int i = 0; i < 3; i++ ) {
		float zs = i == 0 ? cvLtSrc.z : ( i == 1 ? cvLtPrm.y : cvLtPrm.z ) - cvLtSrc.z;
		float3 a = p - float3( cvLtSrc.xy, zs );
		float r = sqrt( dot( a, a ) + cvLtPrm.x );
		float e = exp( -r * cvLtSrc.w ) / r;
		float s = i == 0 ? 1.0 : -1.0;
		phi += s * e;
		gd += s * ( -e * ( cvLtSrc.w + 1.0 / r ) / r ) * dot( a, d );
	}
	return max( phi + cvLtPrm.w * gd, 0.0 );
}

// the flash's light at this pixel (premultiplied, as the history's), a = the clouds' opacity
float3 cvLtLight( float2 uv, float a )
{
	// the nearest cloudy depth of the bilinear footprint (an edge texel of no cloud holds 1e4)
	float4 q = cvHistDepth.GatherRed( _cvHistDepth_sampler, uv );
	float dm = min( min( q.x, q.y ), min( q.z, q.w ) );
	if ( dm >= 1e3 )
		return (float3)( 0.0 );
	float2 ndc = uv * 2.0 - 1.0;
	float3 d = normalize( mul(float3( ( ndc.x + cvProj.z ) / cvProj.x, ( ndc.y + cvProj.w ) / cvProj.y, -1.0 ), cvCam) );
	const float kRbKm = 6360.0;
	float3 e = float3( d.xy * dm, kRbKm + cvCamKm.z + d.z * dm );
	float3 p = float3( cvCamKm.xy + d.xy * dm, length( e ) - kRbKm );
	return a * cvLtCol * cvLtField( p, d );
}

void frag_main()
{
	float2 uv = ( gl_FragCoord.xy - cvRect.xy ) * cvRect.zw;
	if ( cvFront >= 1 ) {
		float z = cvScene.Sample( _cvScene_sampler, uv ).r;
		if ( z >= 1.0 )
			discard;
		float2 ndc = uv * 2.0 - 1.0;
		float3 v = float3( ( ndc.x + cvProj.z ) / cvProj.x, ( ndc.y + cvProj.w ) / cvProj.y, -1.0 );
		float tScene = length( v ) * cvSceneZ.y / ( 2.0 * z - 1.0 + cvSceneZ.x ) * cvSceneZ.z;
		int2 hs = textureSize( cvHistDepth, 0 );
		float dm = cvHistDepth.Load( int3(clamp( int2( uv * float2( hs ) ), (int2)( 0 ), hs - 1 ), 0) ).r;
		if ( cvSceneZ.w > 0.5 && dm >= tScene )
			discard;
	}
	float4 h = cvHist.Sample( _cvHist_sampler, uv );
	float a = clamp( 1.0 - h.a, 0.0, 1.0 );
	if ( a <= 1e-4 )
		discard;
	if ( cvLtOn != 0 )
		h.rgb += cvLtLight( uv, a );
	fragColor = cvFront == 2 ? float4( max( h.rgb, (float3)( 0.0 ) ), a ) : float4( studioOutput( h.rgb / a ), a );
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
