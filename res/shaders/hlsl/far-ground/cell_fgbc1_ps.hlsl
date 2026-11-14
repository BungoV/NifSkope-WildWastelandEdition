// HLSL source of features/far-ground/cell_fgbc1.frag (fragment shader), lane HLSLSRC2 (notes/hlslsrc2/tools/glsl2hlsl.py): one readable file for every
// variant, the GLSL's comments, names and order kept. Loose uniforms are the LooseUniforms cbuffer (b31).
// variants: #if none (the loader's defines; res/shaders/hlsl/variants.txt lists the sets built)
// profile: ps_6_0
#include "FarGroundBindingsVk.hlsli"
#define WW_HLSL_SRC 1	// the shared .hlsli files give their readable half
static float4 gl_FragCoord;
/* lane FARPAGES1: BC1 (DXT1) on the GPU, one fragment a 4x4 block of fgSrc's level fgSrcLevel. Out: x = colour0 |
 * colour1 << 16 (RGB565, colour0 > colour1: the 4-colour mode), y = 2-bit indices, texel (x, y) at bit 2 (4 y + x).
 * Endpoints: the block's box, its diagonal picked by the covariance signs against green, inset by 1/16 (van Waveren). */

Texture2D<float4> fgSrc : register(t0, space0); SamplerState _fgSrc_sampler : register(s0, space0);
cbuffer LooseUniforms : register(b31, space0)
{
	int fgSrcLevel;
	int fgBc1Red;			// test only: 1 swaps the endpoints without the indices (a broken encoder)
};


static uint2 fragBlock;

uint to565( float3 c )
{
	uint3 q = uint3( floor( clamp( c, 0.0, 1.0 ) * float3( 31.0, 63.0, 31.0 ) + 0.5 ) );
	return ( q.r << 11 ) | ( q.g << 5 ) | q.b;
}

float3 from565( uint v )
{
	return float3( float( ( v >> 11 ) & 31u ) / 31.0, float( ( v >> 5 ) & 63u ) / 63.0, float( v & 31u ) / 31.0 );
}

void frag_main()
{
	int2 b = int2( gl_FragCoord.xy ) * 4;
	float3 px[16];
	float3 lo = (float3)( 1.0 ), hi = (float3)( 0.0 );
	for ( int i = 0; i < 16; i++ ) {
		px[i] = fgSrc.Load( int3(b + int2( i & 3, i >> 2 ), fgSrcLevel) ).rgb;
		lo = min( lo, px[i] );
		hi = max( hi, px[i] );
	}
	float3 mid = ( lo + hi ) * 0.5;
	float crg = 0.0, cbg = 0.0;
	for ( int i = 0; i < 16; i++ ) {
		float3 t = px[i] - mid;
		crg += t.r * t.g;
		cbg += t.b * t.g;
	}
	float3 inset = ( hi - lo ) / 16.0;
	lo += inset;
	hi -= inset;
	float3 e0 = hi, e1 = lo;
	if ( crg < 0.0 ) {
		e0.r = lo.r;
		e1.r = hi.r;
	}
	if ( cbg < 0.0 ) {
		e0.b = lo.b;
		e1.b = hi.b;
	}
	uint c0 = to565( e0 ), c1 = to565( e1 );
	if ( c0 < c1 ) {
		uint t = c0;
		c0 = c1;
		c1 = t;
	}
	uint bits = 0u;
	if ( c0 != c1 ) {
		// the 4 colours lie on one line, so the nearest by projection is the nearest (no 4-way search)
		float3 p0 = from565( c0 ), p1 = from565( c1 );
		float3 ax = p0 - p1;
		float inv = 3.0 / dot( ax, ax );
		for ( int i = 0; i < 16; i++ ) {
			int q = int( clamp( dot( px[i] - p1, ax ) * inv + 0.5, 0.0, 3.0 ) );	// 0 = p1 .. 3 = p0
			uint idx = q == 0 ? 1u : ( q == 1 ? 3u : ( q == 2 ? 2u : 0u ) );
			bits |= idx << uint( 2 * i );
		}
	}
	if ( fgBc1Red != 0 && c0 != c1 ) {
		uint t = c0;
		c0 = c1;
		c1 = t;
	}
	fragBlock = uint2( c0 | ( c1 << 16 ), bits );
}

struct SPIRV_Cross_Input
{
    float4 gl_FragCoord : SV_Position;
};

struct SPIRV_Cross_Output
{
    uint2 fragBlock : SV_Target0;
};

SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    gl_FragCoord = stage_input.gl_FragCoord;
    gl_FragCoord.w = 1.0 / gl_FragCoord.w;
    frag_main();
    SPIRV_Cross_Output stage_output;
    stage_output.fragBlock = fragBlock;
    return stage_output;
}
