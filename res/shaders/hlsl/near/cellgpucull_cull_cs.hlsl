// HLSL source of features/near/cellgpucull_cull.glsl (compute kernel), lane HLSLSRC3, written by hand from the GLSL (the
// translator has no compute stage). GL compiles the .glsl (src/features/near/cellgpucull.cpp); this is its HLSL twin, byte
// for byte on the GPU by tests/hlslmatch/cullmatch.sh. Loose uniforms are the LooseUniforms cbuffer (b31). Matrices hold
// GL's columns as rows (row_major), so a GL product M * v reads mul( v, M ).
// variants: #if none (cellgpucull.cpp compile() passes no defines)
// profile: cs_6_0
#include "cellgpucull_bind.hlsli"

struct Meshlet { float4 sphere; float4 cone; uint4 tri; };
struct Job { row_major float4x4 mv; float4 p; };
WW_CULL_BIND( 0 ) StructuredBuffer<float> hiz : register( t0 );	// HizBuf
WW_CULL_BIND( 1 ) StructuredBuffer<Meshlet> ml : register( t1 );	// MeshletBuf
WW_CULL_BIND( 2 ) StructuredBuffer<Job> jobs : register( t2 );	// JobBuf
WW_CULL_BIND( 3 ) StructuredBuffer<uint2> items : register( t3 );	// ItemBuf
WW_CULL_BIND( 4 ) RWStructuredBuffer<uint> cmds : register( u4 );	// CmdBuf, written only
WW_CULL_BIND( 5 ) RWStructuredBuffer<uint> census : register( u5 );	// CensusBuf, 8 counters
WW_CULL_BIND( 31 ) cbuffer LooseUniforms : register( b31 )
{
	row_major float4x4 proj;
	float4 planes[5];
	uint numItems;
	uint stdPad0;	// std140: vpSize at 152
	int2 vpSize;
	int levels;
	int lvlOff[16];
	int2 lvlDim[16];
	int mode;	// 1 occlusion, 2 cone, 4 frustum, 8 red hard, 16 red nocull, 32 census
	float depthEps;
};
[numthreads( 64, 1, 1 )]
void main( uint3 gl_GlobalInvocationID : SV_DispatchThreadID )
{
	uint i = gl_GlobalInvocationID.x;
	if ( i >= numItems )
		return;
	uint2 it = items[i];
	Meshlet m = ml[it.y];
	Job j = jobs[it.x];
	float3 c = mul( float4( m.sphere.xyz, 1.0 ), j.mv ).xyz;
	float r = m.sphere.w * j.p.x;
	int why = 0;
	if ( ( mode & 4 ) != 0 ) {
		float margin = r * 1.001 + 1e-5 * length( c );
		for ( int k = 0; k < 5; k++ ) {
			if ( dot( planes[k].xyz, c ) + planes[k].w < -margin ) {
				why = 1;
				break;
			}
		}
	}
	if ( why == 0 && ( mode & 2 ) != 0 && j.p.y != 0.0 && m.cone.w <= 1.0 ) {
		float3 a = normalize( mul( m.cone.xyz, (float3x3)j.mv ) ) * j.p.y;
		if ( dot( c, a ) >= m.cone.w * length( c ) + r )
			why = 2;
	}
	if ( why == 0 && ( mode & 1 ) != 0 ) {
		bool front = true;
		float2 lo = float2( 1e30, 1e30 ), hi = float2( -1e30, -1e30 );
		for ( int k = 0; k < 8; k++ ) {
			float3 o = float3( ( k & 1 ) != 0 ? r : -r, ( k & 2 ) != 0 ? r : -r, ( k & 4 ) != 0 ? r : -r );
			float4 q = mul( float4( c + o, 1.0 ), proj );
			if ( q.w <= 0.0 || q.z < -q.w ) {
				front = false;
				break;
			}
			lo = min( lo, q.xy / q.w );
			hi = max( hi, q.xy / q.w );
		}
		if ( front ) {
			float2 p0 = ( lo * 0.5 + 0.5 ) * float2( vpSize ) - 1.0;
			float2 p1 = ( hi * 0.5 + 0.5 ) * float2( vpSize ) + 1.0;
			if ( p1.x >= 0.0 && p1.y >= 0.0 && p0.x < float( vpSize.x ) && p0.y < float( vpSize.y ) ) {
				int2 a = clamp( int2( floor( p0 ) ), int2( 0, 0 ), vpSize - 1 );
				int2 b = clamp( int2( floor( p1 ) ), int2( 0, 0 ), vpSize - 1 );
				int span = max( b.x - a.x, b.y - a.y ) + 1;
				int L = 0;
				while ( L < levels - 1 && ( int( 1 ) << L ) < span )
					L++;
				int2 ta = a >> L, tb = b >> L;
				float h = 0.0;
				for ( int y = ta.y; y <= tb.y; y++ )
					for ( int x = ta.x; x <= tb.x; x++ )
						h = max( h, hiz[lvlOff[L] + y * lvlDim[L].x + x] );
				float z = ( mode & 8 ) != 0 ? c.z : c.z + r;
				float4 q = mul( float4( 0.0, 0.0, z, 1.0 ), proj );
				if ( ( q.z / q.w ) * 0.5 + 0.5 > h + depthEps )
					why = 3;
			}
		}
	}
	if ( ( mode & 16 ) != 0 )
		why = 0;
	uint o = i * 5u;
	cmds[o] = m.tri.y * 3u;
	cmds[o + 1u] = why == 0 ? 1u : 0u;
	cmds[o + 2u] = m.tri.x * 3u;
	cmds[o + 3u] = 0u;
	cmds[o + 4u] = 0u;
	if ( ( mode & 32 ) != 0 ) {
#ifndef CELLGPUCULL_CULL_RED	// the gate's red: the census never counts a culled reason
		InterlockedAdd( census[why], 1u );
#endif
		if ( why == 0 )
			InterlockedAdd( census[4], m.tri.y );
	}
}
