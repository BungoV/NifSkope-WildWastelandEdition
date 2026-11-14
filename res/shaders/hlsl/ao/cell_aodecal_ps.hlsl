// HLSL source of features/ao/cell_aodecal.frag (fragment shader), lane HLSLSRC2 (notes/hlslsrc2/tools/glsl2hlsl.py): one readable file for every
// variant, the GLSL's comments, names and order kept. Loose uniforms are the LooseUniforms cbuffer (b31).
// variants: #if none (the loader's defines; res/shaders/hlsl/variants.txt lists the sets built)
// profile: ps_6_0
#include "AoBindingsVk.hlsli"
#define WW_HLSL_SRC 1	// the shared .hlsli files give their readable half
static float4 gl_FragCoord;
/* One copy's baked AO volume (src/aovolume.h) over the opaque pass. aoMode 0: the copy's own triangles, writing its
 * id (the stencil masks). aoMode 1: the footprint box's back faces; each pixel's receiver is rebuilt from the opaque
 * pass into model space and AO(m, n) multiplied into the target. The lookup must match aovol::lookup term for term. */

static float4 outAo;
static float4 outId;
cbuffer LooseUniforms : register(b31, space0)
{
	int aoMode;
	float aoSelf;		// the copy's index + 1 (mask mode)
	bool aoAdd;			// debug: out = AO - 1, added instead of multiplied
	float4 aoProj;		// P00, P11, P20, P21
	float2 aoSize;		// the target's size
	float aoSc;			// the view's scale
	float4 aoM[3];		// view -> model rows (M | b)
	float3 aoN[3];		// view -> model normal rows
	float3 aoLo;
	float3 aoHi;
	int3 aoDims;
	float aoFade;
	float3 aoC;
	float aoCoef[9];
	float aoRcut;
};

Texture2D<float4> aoGbuf : register(t0, space0); SamplerState _aoGbuf_sampler : register(s0, space0);	// full size: view normal, linear depth (game units); 1e6 = background
Texture3D<float4> aoVol : register(t1, space0); SamplerState _aoVol_sampler : register(s1, space0);	// RGBA32F, dims.x x dims.y x 3 dims.z: slabs k0-3, k4-7, k8

void sh9( float3 d, out float y[9] )
{
	y[0] = 0.282095;
	y[1] = 0.488603 * d.y;
	y[2] = 0.488603 * d.z;
	y[3] = 0.488603 * d.x;
	y[4] = 1.092548 * d.x * d.y;
	y[5] = 1.092548 * d.y * d.z;
	y[6] = 0.315392 * ( 3.0 * d.z * d.z - 1.0 );
	y[7] = 1.092548 * d.x * d.z;
	y[8] = 0.546274 * ( d.x * d.x - d.y * d.y );
}

void voxel( int3 i, float w, inout float K[9] )
{
	float4 a = aoVol.Load( int4(i, 0) );
	float4 b = aoVol.Load( int4(int3( i.xy, i.z + aoDims.z ), 0) );
	float c = aoVol.Load( int4(int3( i.xy, i.z + 2 * aoDims.z ), 0) ).r;
	K[0] += w * a.x; K[1] += w * a.y; K[2] += w * a.z; K[3] += w * a.w;
	K[4] += w * b.x; K[5] += w * b.y; K[6] += w * b.z; K[7] += w * b.w;
	K[8] += w * c;
}

float aoLookup( float3 m, float3 n )
{
	float K[9] = { 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 };
	int3 i0;
	float3 f;
	for ( int a = 0; a < 3; a++ ) {
		float cell = ( aoHi[a] - aoLo[a] ) / float( aoDims[a] );
		float g = clamp( ( m[a] - aoLo[a] ) / cell - 0.5, 0.0, float( aoDims[a] - 1 ) );
		i0[a] = min( int( floor( g ) ), aoDims[a] - 2 );
		f[a] = g - float( i0[a] );
	}
	for ( int dz = 0; dz < 2; dz++ )
		for ( int dy = 0; dy < 2; dy++ )
			for ( int dx = 0; dx < 2; dx++ ) {
				float w = ( dx == 1 ? f.x : 1.0 - f.x ) * ( dy == 1 ? f.y : 1.0 - f.y ) * ( dz == 1 ? f.z : 1.0 - f.z );
				voxel( i0 + int3( dx, dy, dz ), w, K );
			}
	float Y[9];
	sh9( n, Y );
	const float band[9] = { 1.0, 2.0 / 3.0, 2.0 / 3.0, 2.0 / 3.0, 0.25, 0.25, 0.25, 0.25, 0.25 };
	float occ = 0.0;
	for ( int j = 0; j < 9; j++ )
		occ += band[j] * K[j] * 3.5449077 * Y[j];	// 2 sqrt(pi)
	float3 inner3 = min( m - aoLo, aoHi - m );
	float fd = clamp( min( inner3.x, min( inner3.y, inner3.z ) ) / aoFade, 0.0, 1.0 );
	float far = 0.0;
	float3 w = m - aoC;
	float r = max( length( w ), 1e-6 );
	if ( r < aoRcut ) {
		float3 u = w / r;
		float yu[9];
		sh9( u, yu );
		float s = 0.0;
		for ( int j = 0; j < 9; j++ )
			s += yu[j] * aoCoef[j];
		far = min( max( s, 0.0 ) / ( r * r ), 1.0 ) * max( -dot( n, u ), 0.0 );
	}
	return clamp( 1.0 - ( occ * fd + far * ( 1.0 - fd ) ), 0.0, 1.0 );
}

void frag_main()
{
	if ( aoMode == 0 ) {
		outAo = (float4)( 1.0 );
		outId = (float4)( aoSelf );
		return;
	}
	float4 g = aoGbuf.Load( int3(int2( gl_FragCoord.xy ), 0) );
	if ( g.w >= 1.0e5 ) {
		outAo = (float4)( aoAdd ? 0.0 : 1.0 );
		return;
	}
	float2 ndc = gl_FragCoord.xy / aoSize * 2.0 - 1.0;
	float zv = g.w * aoSc;
	float4 pv = float4( zv * ( ndc.x + aoProj.z ) / aoProj.x, zv * ( ndc.y + aoProj.w ) / aoProj.y, -zv, 1.0 );
	float3 m = float3( dot( aoM[0], pv ), dot( aoM[1], pv ), dot( aoM[2], pv ) );
	float3 n = normalize( float3( dot( aoN[0], g.xyz ), dot( aoN[1], g.xyz ), dot( aoN[2], g.xyz ) ) );
	float ao = aoLookup( m, n );
	outAo = (float4)( aoAdd ? ao - 1.0 : ao );
	outId = (float4)( 0.0 );
}

struct SPIRV_Cross_Input
{
    float4 gl_FragCoord : SV_Position;
};

struct SPIRV_Cross_Output
{
    float4 outAo : SV_Target0;
    float4 outId : SV_Target1;
};

SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    gl_FragCoord = stage_input.gl_FragCoord;
    gl_FragCoord.w = 1.0 / gl_FragCoord.w;
    frag_main();
    SPIRV_Cross_Output stage_output;
    stage_output.outAo = outAo;
    stage_output.outId = outId;
    return stage_output;
}
