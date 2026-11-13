// HLSL source of uvedit.frag, translated by notes/hlslsrc1/tools/glsl2hlsl.py (text level: comments, names and
// #if variants kept from the GLSL). Matrices hold GL's columns as rows (row_major blocks), so a GL product A * B
// reads mul(B, A).
// profile: ps_6_0
// variants: #if (the loader's WW_* defines select the variant; built with -D per set)
#include "CoreBindingsVk.hlsli"
#include "GlslCompat.hlsli"
#define WW_STAGE_PS 1

Texture2D<float4> BaseMap; SamplerState _BaseMap_sampler;

//  0 = no color conversion
//  1 = sRGB texture
//  2 = unsigned BC5 normal map
//  3 = signed BC5 normal map
// +4 = disable alpha channel
uniform int textureColorMode;

uniform float2 uvCenter;
uniform float4 uvScaleAndOffset;
uniform float uvRotation;

uniform float2 pixelScale;

uniform float4 gridColors[3];
uniform float3 gridLineWidths;
uniform bool gridEnabled[3];
// zoom-adaptive base subdivision (lines per UV unit, may be < 1 when zoomed
// far out); the finer levels are fixed x8 / x64 multiples of it
uniform float gridBaseDiv;

uniform float4 backgroundColor;
uniform float2 textureColorScale;

// 0 = repeat the image across every tile (legacy default when unset);
// 1 = show only the 0-1 tile, dark outside (Blender default)
uniform int tileMode;
// texture-pixel grid: xy = texture resolution in texels, 0 = disabled;
// z = line alpha, w = fade-in start (view UV span below which it appears)
uniform float4 pixelGrid;

static float2 texCoord;

static float4 fragColor;

// stage I/O: the members (locations, semantics) of the generated port, each under its GLSL declaration's #if
struct SPIRV_Cross_Input
{
    float2 texCoord : TEXCOORD0;
};
struct SPIRV_Cross_Output
{
    float4 fragColor : SV_Target0;
};



float4 srgbCompress( float4 c )
{
	float3	tmp = max( c.rgb, (float3)(0.0) ) + 0.000858025;
	float3	tmp1 = tmp;
	float3	tmp2 = tmp * tmp;
	tmp = rsqrt( tmp ) * tmp1;
	tmp1 = tmp1 * 0.59302883 + 1.42598062;
	tmp2 = tmp2 * -0.18732371 - 0.04110602;
	tmp = ( tmp * -0.79095451 + tmp1 ) * tmp + tmp2;
	return float4( tmp, c.a );
}

float3 drawGridLines( float3 color, float x, float s )
{
	// Blender-style zoom-adaptive grid: the base level rescales with zoom so
	// its lines stay readable at any distance, finer ×8 levels fade in when
	// zoomed (gridEnabled/gridColors alpha are driven host-side)
	float3	divs = max( gridBaseDiv, 1.0e-9 ) * float3( 1.0, 8.0, 64.0 );
	float3	f = (float3)(x) * divs;
	f = abs( f - round( f ) ) / divs * s;
	f = max( f - ( gridLineWidths * 0.5 - 0.5 ), (float3)(0.0) );
	if ( gridEnabled[0] && f.x < 1.0 )
		return lerp( color, gridColors[0].rgb, ( cos( f.x * 3.14159265 ) * 0.5 + 0.5 ) * gridColors[0].a );
	if ( gridEnabled[1] && f.y < 1.0 )
		return lerp( color, gridColors[1].rgb, ( cos( f.y * 3.14159265 ) * 0.5 + 0.5 ) * gridColors[1].a );
	if ( gridEnabled[2] && f.z < 1.0 )
		return lerp( color, gridColors[2].rgb, ( cos( f.z * 3.14159265 ) * 0.5 + 0.5 ) * gridColors[2].a );
	return color;
}

// subtle grid at texture-pixel boundaries; alpha follows how close the pixel
// edge is, so it only firms up when zoomed in enough to matter
float3 drawPixelGrid( float3 color )
{
	if ( or(pixelGrid.x < 0.5, pixelGrid.y < 0.5) )
		return color;
	float2	texel = texCoord.xy * pixelGrid.xy;
	float2	d = abs( texel - round( texel ) );
	// pixel size on screen, in the same units as pixelScale (texels/screenpx)
	float2	px = pixelScale / pixelGrid.xy;
	// fade the whole grid in as pixels grow past a few screen pixels
	float	vis = clamp( ( min( px.x, px.y ) - 2.0 ) * 0.5, 0.0, 1.0 );
	if ( vis <= 0.0 )
		return color;
	float	line_ = 1.0 - smoothstep( 0.0, 1.0, min( d.x * px.x, d.y * px.y ) );
	return lerp( color, (float3)(1.0), line_ * pixelGrid.z * vis );
}

void frag_main()
{
	float4	color = (float4)(0.0);

	bool	inTile = texCoord.x >= -1.0 && texCoord.x <= 2.0 && texCoord.y >= -1.0 && texCoord.y <= 2.0;
	if ( tileMode == 1 )
		inTile = texCoord.x >= 0.0 && texCoord.x <= 1.0 && texCoord.y >= 0.0 && texCoord.y <= 1.0;
	if ( inTile ) {
		float2	offs = texCoord.xy - uvCenter;
		float	r_c = cos( uvRotation );
		float	r_s = sin( uvRotation ) * -1.0;
		offs = float2( offs.x * r_c - offs.y * r_s, offs.x * r_s + offs.y * r_c ) * uvScaleAndOffset.xy;
		offs = offs + uvCenter + uvScaleAndOffset.zw;

		color = BaseMap.Sample(_BaseMap_sampler, offs);

		if ( ( textureColorMode & 2 ) != 0 ) {
			// BC5 normal map
			if ( ( textureColorMode & 1 ) == 0 ) {
				// UNORM
				color = color * 2.0 - 1.0;
			}
			color.b = sqrt( max( 1.0 - dot( color.rg, color.rg ), 0.0 ) );
			color = color * 0.5 + 0.5;
		} else if ( ( textureColorMode & 1 ) != 0 ) {
			// convert to sRGB
			color = srgbCompress( color );
		}

		if ( ( textureColorMode & 4 ) != 0 )
			color.a = 1.0;

		if ( texCoord.x >= 0.0 && texCoord.x <= 1.0 && texCoord.y >= 0.0 && texCoord.y <= 1.0 )
			color.rgb *= textureColorScale.x;
		else
			color.rgb *= textureColorScale.y;
	}

	color = float4( lerp( backgroundColor.rgb, color.rgb, color.a ), 1.0 );

	// When not repeating, confine grid + pixel lines to the 0-1 tile (Blender):
	// outside is plain background, no lines.
	bool inGridTile = tileMode != 1
		|| ( texCoord.x >= 0.0 && texCoord.x <= 1.0 && texCoord.y >= 0.0 && texCoord.y <= 1.0 );
	if ( inGridTile ) {
		color.rgb = drawPixelGrid( color.rgb );
		color.rgb = drawGridLines( drawGridLines( color.rgb, texCoord.x, pixelScale.x ), texCoord.y, pixelScale.y );
	}

	fragColor = color;
}

// entry: copies the stage inputs into the GLSL-named globals, runs the GLSL main, returns the outputs
SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    texCoord = stage_input.texCoord;
    frag_main();
    SPIRV_Cross_Output stage_output;
    stage_output.fragColor = fragColor;
    return stage_output;
}
