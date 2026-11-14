// lane VKFAR2 (row L8 slice 1, 2026-11-14): cell_precip_gpu's per-particle work on the present device, which has no
// geometry stage. One thread a particle of the run (gl_VertexID = pcFirst + i): cell_precip_gpu_vs.hlsl's field and cull,
// then cell_precip_gpu_gs.hlsl's per-particle part (the least drawn size, the motion stretch, the axes, the colour, the
// far tier's cell mean, the uv box), into one record a particle in GL's order. A culled particle keeps its slot (c.w 0):
// alpha blending needs GL's order, so nothing is compacted. precip_view_vs.hlsl builds the four corners from the record.
// Slice 1 is unlit and unfogged (lit 1, the fog off: A = 1, B = 0); vkprecip.inc refuses a lit cell or the fog.
// The source files are read as they stand (cell_precip_field.hlsli unchanged); the expressions below are theirs.
// profile: cs_6_0
#include "WeatherBindingsVk.hlsli"
#define WW_HLSL_SRC 1	// the shared .hlsli files give their readable half

#define WW_LOOSE_TYPES 1
#include "cell_precip_field.hlsli"
#undef WW_LOOSE_TYPES
cbuffer LooseUniforms : register(b31, space0)
{
#define WW_LOOSE_UNIFORMS 1
#include "cell_precip_field.hlsli"
#undef WW_LOOSE_UNIFORMS
	float4 pcRow[3];		// world -> view rows (scale in)
	row_major float4x4 pcProj;
	float pcFocal;		// pixels per view unit at depth 1
	float pcMinPx;		// the least half size drawn, pixels
	float pcShutter;	// seconds
	float pcThin;		// the far tier's opacity factor
	float4 pcAtlas;		// cell size in uv (x, y), the deepest mip a near quad reads, the mip with one texel a cell
	float2 pcInset;		// half a texel of that deepest mip, in uv
	int pcFar;
	bool pcHasTex;
	uint pcFirst;		// not GL's: the run's first vertex
	uint pcCount;		// not GL's: the run's particles
	int pcRed;			// not GL's: gates only, 1 the cull radius 0 (cell_precip_gpu_vs.hlsl's PC_CULL_RED)
};
#include "cell_precip_field.hlsli"

Texture2D<float4> pcTex : register(t1, space0); SamplerState _pcTex_sampler : register(s1, space0);

struct PrecipRec
{
	float4 c;		// the centre (view), w 1 drawn / 0 culled
	float4 axes;	// ax, ay
	float4 ext;		// hx, hy, the cell's uv corner
	float4 col;
	float4 box;
	float4 a;		// the fogged light per unit albedo (slice 1: 1)
	float4 b;		// the fog's own light (slice 1: 0)
};
RWStructuredBuffer<PrecipRec> pcOut : register(u0, space0);

[numthreads(64, 1, 1)]
void main( uint3 id : SV_DispatchThreadID )
{
	if ( id.x >= pcCount )
		return;
	PrecipRec r = (PrecipRec)0;
	// cell_precip_gpu_vs.hlsl vert_main
	float3 rel, vel;
	float hs, angle;
	float2 cellUv;
	bool keep = pcParticle( pcFirst + id.x, rel, vel, hs, angle, cellUv );
	float3 vsView = float3( dot( pcRow[0].xyz, rel ), dot( pcRow[1].xyz, rel ), dot( pcRow[2].xyz, rel ) );
	float3 vsVel = float3( dot( pcRow[0].xyz, vel ), dot( pcRow[1].xyz, vel ), dot( pcRow[2].xyz, vel ) );
	float vsHalf = hs * length( pcRow[0].xyz );
	float4 vsColor = float4( 1.0, 1.0, 1.0, pcThin );
	float depth = -vsView.z;
	bool cull = or( !keep, depth < 1.0 );
	if ( !cull ) {
		float hd = max( vsHalf, pcMinPx * depth / pcFocal );
		float R = 2.0 * hd + length( vsVel.xy ) * pcShutter;
		if ( ( pcRed & 1 ) != 0 )
			R = 0.0;
		float4 c = mul(float4( vsView, 1.0 ), pcProj);
		float rx = abs( pcProj[0][0] ) * R, ry = abs( pcProj[1][1] ) * R;
		cull = or( or( or( c.x - rx > c.w, c.x + rx < -c.w ), c.y - ry > c.w ), c.y + ry < -c.w );
	}
	// cell_precip_gpu_gs.hlsl geom_main, up to the corners
	if ( cull ) {
		pcOut[id.x] = r;
		return;
	}
	float3 c = vsView;
	if ( depth < 1.0 ) {
		pcOut[id.x] = r;
		return;
	}
	float h = vsHalf;
	float k = pcFocal / depth;	// pixels per view unit here
	float2 tr = vsVel.xy * pcShutter;
	float tl = length( tr );
	float hd = max( h, pcMinPx / k );
	float4 col = vsColor;
	col.a *= 4.0 * h * h / ( ( 2.0 * hd + tl ) * 2.0 * hd );
	float2 ax, ay;
	float hx = hd, hy = hd;
	if ( tl > 0.2 * hd ) {
		ax = tr / tl;
		hx = hd + 0.5 * tl;
	} else {
		ax = float2( cos( angle ), sin( angle ) );
	}
	ay = float2( -ax.y, ax.x );
	float2 cell = pcAtlas.xy;
	r.box = float4( cellUv + pcInset, cellUv + cell - pcInset );
	if ( pcFar != 0 ) {
		// the cell's mean: 16 taps at the one-texel-a-cell mip
		float4 m = (float4)( 0.0 );
		if ( pcHasTex ) {
			for ( int i = 0; i < 4; i++ )
				for ( int j = 0; j < 4; j++ )
					m += pcTex.SampleLevel( _pcTex_sampler, cellUv + cell * ( float2( float( i ), float( j ) ) + 0.5 ) * 0.25, pcAtlas.w );
			m *= 1.0 / 16.0;
		} else {
			m = float4( 1.0, 1.0, 1.0, 0.3 );
		}
		col = float4( col.rgb * m.rgb, col.a * m.a );
	}
	r.c = float4( c, 1.0 );
	r.axes = float4( ax, ay );
	r.ext = float4( hx, hy, cellUv );
	r.col = col;
	r.a = float4( 1.0, 1.0, 1.0, 0.0 );
	pcOut[id.x] = r;
}
