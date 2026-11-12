// Lane VKFRAME1 round 8 (2026-11-12): the shared math of the volume clouds' composite over the scene (cloud_composite.frag)
// and the lookdev output path it goes out through (lookdev_output.glsl studioOutput). No bindings: NifSkope's shim is
// cloud_front.hlsl over PostFxBindingsVk.hlsli; Fusion Shaders keeps its own. Every expression in GL's order.
#ifndef CLOUD_FRONT_COMMON_HLSLI
#define CLOUD_FRONT_COMMON_HLSLI

// the sRGB OETF
float3 F4FX_LinearToSrgb( float3 c )
{
	c = max( min( c, 1.0f ), 0.0f );	// GL's clamp order (NaN reads 1)
	const float3 t = step( 0.0031308f, c );
	const float3 lo = c * 12.92f;
	const float3 s = lo + t * ( ( 1.055f * pow( c, 1.0f / 2.4f ) - 0.055f ) - lo );
	// at the clamp GL writes 1.0 where Vulkan's tail lands under 1 and stores 1 - 1 ulp (four spellings of the tail and
	// pow as exp2 / log2 all did); pow( 1, y ) = 1, so this select is GL's value
	return select( c >= 1.0f, 1.0f, s );
}

float3 F4FX_AgxContrast( float3 x )
{
	const float3 x2 = x * x;
	const float3 x4 = x2 * x2;
	return 15.5f * x4 * x2 - 40.14f * x4 * x + 31.96f * x4 - 6.868f * x2 * x + 0.4298f * x2 + 0.1191f * x - 0.00232f;
}

// AgX, Benjamin Wrensch's minimal fit (sRGB display out); the matrices as GL's columns
float3 F4FX_AgxDisplay( float3 c )
{
	const float minEv = -12.47393f;
	const float maxEv = 4.026069f;
	c = max( c, 1e-10f );
	c = float3( 0.842479062253094f, 0.0423282422610123f, 0.0423756549057051f ) * c.x
		+ float3( 0.0784335999999992f, 0.878468636469772f, 0.0784336f ) * c.y
		+ float3( 0.0792237451477643f, 0.0791661274605434f, 0.879142973793104f ) * c.z;
	c = clamp( log2( c ), minEv, maxEv );
	c = ( c - minEv ) / ( maxEv - minEv );
	c = F4FX_AgxContrast( c );
	return clamp( float3( 1.19687900512017f, -0.0528968517574562f, -0.0529716355144438f ) * c.x
		+ float3( -0.0980208811401368f, 1.15190312990417f, -0.0980434501171241f ) * c.y
		+ float3( -0.0990297440797205f, -0.0989611768448433f, 1.15107367264116f ) * c.z, 0.0f, 1.0f );
}

// Khronos PBR Neutral (linear in, linear out)
float3 F4FX_PbrNeutral( float3 color )
{
	const float startCompression = 0.8f - 0.04f;
	const float desaturation = 0.15f;
	const float x = min( color.r, min( color.g, color.b ) );
	const float offset = x < 0.08f ? x - 6.25f * x * x : 0.04f;
	color -= offset;
	const float peak = max( color.r, max( color.g, color.b ) );
	if ( peak < startCompression )
		return color;
	const float d = 1.0f - startCompression;
	const float newPeak = 1.0f - d * d / ( peak + d - startCompression );
	color *= newPeak / peak;
	const float g = 1.0f - 1.0f / ( desaturation * ( peak - newPeak ) + 1.0f );
	return lerp( color, newPeak.xxx, g );
}

// lookdev_output.glsl studioOutput: 0 Standard, 1 AgX, 2 PBR Neutral, 3 light as it is
float3 F4FX_StudioOutput( float3 c, float exposure, int viewTransform )
{
	c = max( c, 0.0f ) * exposure;
	if ( viewTransform == 3 )
		return c;
	if ( viewTransform == 1 )
		return F4FX_AgxDisplay( c );
	if ( viewTransform == 2 )
		return F4FX_LinearToSrgb( F4FX_PbrNeutral( c ) );
	return F4FX_LinearToSrgb( c );
}

// a lightning strike's diffusion field with images across the band's boundaries (cloud_composite.frag cvLtField)
float F4FX_CloudLtField( float3 p, float3 d, float4 ltSrc, float4 ltPrm )
{
	float phi = 0.0f, gd = 0.0f;
	for ( int i = 0; i < 3; i++ ) {
		const float zs = i == 0 ? ltSrc.z : ( i == 1 ? ltPrm.y : ltPrm.z ) - ltSrc.z;
		const float3 a = p - float3( ltSrc.xy, zs );
		const float r = sqrt( dot( a, a ) + ltPrm.x );
		const float e = exp( -r * ltSrc.w ) / r;
		const float s = i == 0 ? 1.0f : -1.0f;
		phi += s * e;
		gd += s * ( -e * ( ltSrc.w + 1.0f / r ) / r ) * dot( a, d );
	}
	return max( phi + ltPrm.w * gd, 0.0f );
}

#endif
