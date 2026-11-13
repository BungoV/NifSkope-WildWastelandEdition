// HLSL source of features/ao/cell_givol.frag (fragment shader), lane HLSLSRC2 (notes/hlslsrc2/tools/glsl2hlsl.py): one readable file for every
// variant, the GLSL's comments, names and order kept. Loose uniforms are the LooseUniforms cbuffer (b31).
// variants: #if none (the loader's defines; res/shaders/hlsl/variants.txt lists the sets built)
// profile: ps_6_0
#include "AoBindingsVk.hlsli"
#define WW_HLSL_SRC 1	// the shared .hlsli files give their readable half
static float4 gl_FragCoord;
/* lane PERF1: the GI volume mask. Each pixel the shape's hull covers (behind its surface: the depth test) rebuilds its
 * surface from the shared opaque g-buffer and tests the point a bias in front of it against the shape; inside, the
 * shape's room + 1 (blended by max). 0: no interior volume, the outdoors (or the room grid, cell_gihalf.frag). */

#include "cell_givol.hlsli"

Texture2D<float4> volGbuf : register(t1, space0); SamplerState _volGbuf_sampler : register(s1, space0);	// view normal, linear depth in game units (1e6 empty)
cbuffer LooseUniforms : register(b31, space0)
{
	int2 volFull;
	float2 volProj;		// the projection's P00, P11
	float4 volRow[3];		// view -> world (cell_lights.glsl cellRow)
	float volBias;		// game units
};


static int vShape;

static float4 volOut;

void frag_main()
{
	int2 f = int2( gl_FragCoord.xy );
	float4 g = volGbuf.Load( int3(f, 0) );
	if ( or( g.w >= 1.0e5, dot( g.xyz, g.xyz ) < 0.5 ) )
		discard;
	float z = -g.w / length( volRow[0].xyz );
	float2 ndc = ( float2( f ) + 0.5 ) / float2( volFull ) * 2.0 - 1.0;
	float4 pv = float4( -z * ndc / volProj, z, 1.0 );
	float3 P = float3( dot( volRow[0], pv ), dot( volRow[1], pv ), dot( volRow[2], pv ) );
	float3 N = normalize( float3( dot( volRow[0].xyz, g.xyz ), dot( volRow[1].xyz, g.xyz ), dot( volRow[2].xyz, g.xyz ) ) );
	float4 a = volShapes.Load( 3 * vShape );
	float4 b = volShapes.Load( 3 * vShape + 1 );
	float4 c = volShapes.Load( 3 * vShape + 2 );
	float3 l = mul(( P + N * volBias - a.xyz ), transpose( volRot( c.xyz ) ));
	if ( !volInside( l, int( a.w + 0.5 ), b.xyz ) )
		discard;
	volOut = (float4)( b.w + 1.0 );
}

struct SPIRV_Cross_Input
{
    nointerpolation int vShape : TEXCOORD0;
    float4 gl_FragCoord : SV_Position;
};

struct SPIRV_Cross_Output
{
    float4 volOut : SV_Target0;
};

SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    gl_FragCoord = stage_input.gl_FragCoord;
    gl_FragCoord.w = 1.0 / gl_FragCoord.w;
    vShape = stage_input.vShape;
    frag_main();
    SPIRV_Cross_Output stage_output;
    stage_output.volOut = volOut;
    return stage_output;
}
