// lane BLOOM1b (src/gl/cellbloom.h): FO4CS's bloom helpers, as res/Post/BloomDownCS.hlsl and BloomUpCS.hlsl
// write them, shared by the chain's passes and the tone map's final combine (cell_hdr.frag)

// FP16 maximum: the pyramid is half float, as FO4CS forces it
const float kBloomMaxFinite = 65504.0;

// F4FX_SanitizeBloomSample: a NaN tap reads 0 (by its bits, as FO4CS reads them), anything else clamps to 0..65504
vec3 bloomSanitize( vec3 c )
{
	uvec3 bits = floatBitsToUint( c );
	uvec3 e = bits & 0x7F800000u;
	uvec3 m = bits & 0x007FFFFFu;
	if ( ( e.x == 0x7F800000u && m.x != 0u ) || ( e.y == 0x7F800000u && m.y != 0u ) || ( e.z == 0x7F800000u && m.z != 0u ) )
		return vec3( 0.0 );
	vec3 bounded = sign( c ) * min( abs( c ), vec3( kBloomMaxFinite ) );
	return clamp( bounded, vec3( 0.0 ), vec3( kBloomMaxFinite ) );
}

// F4FX_Luminance
float bloomLuma( vec3 c )
{
	return max( dot( c, vec3( 0.2126, 0.7152, 0.0722 ) ), 0.0001 );
}

// F4FX_UpsampleTent: 9 taps, 4 / 2 2 2 2 / 1 1 1 1 over 16, at the bloom texel x the radius
vec3 bloomTent( sampler2D m, vec2 uv, vec2 texel, float radiusPixels )
{
	vec2 o = texel * max( radiusPixels, 0.0 );
	vec3 c = bloomSanitize( textureLod( m, uv, 0.0 ).rgb ) * 4.0;
	c += bloomSanitize( textureLod( m, uv + vec2( -o.x, 0.0 ), 0.0 ).rgb ) * 2.0;
	c += bloomSanitize( textureLod( m, uv + vec2( o.x, 0.0 ), 0.0 ).rgb ) * 2.0;
	c += bloomSanitize( textureLod( m, uv + vec2( 0.0, -o.y ), 0.0 ).rgb ) * 2.0;
	c += bloomSanitize( textureLod( m, uv + vec2( 0.0, o.y ), 0.0 ).rgb ) * 2.0;
	c += bloomSanitize( textureLod( m, uv + vec2( -o.x, -o.y ), 0.0 ).rgb );
	c += bloomSanitize( textureLod( m, uv + vec2( o.x, -o.y ), 0.0 ).rgb );
	c += bloomSanitize( textureLod( m, uv + vec2( -o.x, o.y ), 0.0 ).rgb );
	c += bloomSanitize( textureLod( m, uv + vec2( o.x, o.y ), 0.0 ).rgb );
	return c * ( 1.0 / 16.0 );
}

// lane GAPLOOK1 (BLOOMMIP): the same tent read lod levels down the chain top's own mips (the blur chosen per pixel)
vec3 bloomTentLod( sampler2D m, vec2 uv, vec2 texel, float radiusPixels, float lod )
{
	vec2 o = texel * exp2( lod ) * max( radiusPixels, 0.0 );
	vec3 c = bloomSanitize( textureLod( m, uv, lod ).rgb ) * 4.0;
	c += bloomSanitize( textureLod( m, uv + vec2( -o.x, 0.0 ), lod ).rgb ) * 2.0;
	c += bloomSanitize( textureLod( m, uv + vec2( o.x, 0.0 ), lod ).rgb ) * 2.0;
	c += bloomSanitize( textureLod( m, uv + vec2( 0.0, -o.y ), lod ).rgb ) * 2.0;
	c += bloomSanitize( textureLod( m, uv + vec2( 0.0, o.y ), lod ).rgb ) * 2.0;
	c += bloomSanitize( textureLod( m, uv + vec2( -o.x, -o.y ), lod ).rgb );
	c += bloomSanitize( textureLod( m, uv + vec2( o.x, -o.y ), lod ).rgb );
	c += bloomSanitize( textureLod( m, uv + vec2( -o.x, o.y ), lod ).rgb );
	c += bloomSanitize( textureLod( m, uv + vec2( o.x, o.y ), lod ).rgb );
	return c * ( 1.0 / 16.0 );
}

vec3 bloomTentDispersedLod( sampler2D m, vec2 uv, float radiusPixels, float dispersion, float lod )
{
	vec2 texel = 1.0 / vec2( max( textureSize( m, 0 ), ivec2( 1 ) ) );
	vec2 radial = uv - 0.5;
	float len = length( radial );
	vec2 d = len > 1e-5 ? ( radial / len ) * texel * max( dispersion, 0.0 ) : vec2( 0.0 );
	vec3 g = bloomTentLod( m, uv, texel, radiusPixels, lod );
	if ( d == vec2( 0.0 ) )
		return g;
	return vec3( bloomTentLod( m, uv + d, texel, radiusPixels, lod ).r, g.g, bloomTentLod( m, uv - d, texel, radiusPixels, lod ).b );
}

// BloomUpCS's dispersion: red and blue tents pushed apart along the radial from the frame's centre, green the anchor
vec3 bloomTentDispersed( sampler2D m, vec2 uv, float radiusPixels, float dispersion )
{
	vec2 texel = 1.0 / vec2( max( textureSize( m, 0 ), ivec2( 1 ) ) );
	vec2 radial = uv - 0.5;
	float len = length( radial );
	vec2 d = len > 1e-5 ? ( radial / len ) * texel * max( dispersion, 0.0 ) : vec2( 0.0 );
	vec3 g = bloomTent( m, uv, texel, radiusPixels );
	if ( d == vec2( 0.0 ) )
		return g;
	vec3 r = bloomTent( m, uv + d, texel, radiusPixels );
	vec3 b = bloomTent( m, uv - d, texel, radiusPixels );
	return vec3( r.r, g.g, b.b );
}
