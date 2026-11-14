// lane VKFAR2 (row L8 slice 1, 2026-11-14): cell_precip_gpu's per-particle work on the present device, which has no
// geometry stage. One thread a particle of the run (gl_VertexID = pcFirst + i): cell_precip_gpu_vs.hlsl's field and cull,
// then cell_precip_gpu_gs.hlsl's per-particle part (the least drawn size, the motion stretch, the axes, the colour, the
// far tier's cell mean, the uv box), into one record a particle in GL's order. A culled particle keeps its slot (c.w 0):
// alpha blending needs GL's order, so nothing is compacted. precip_view_vs.hlsl builds the four corners from the record.
// Slice 1 is unlit and unfogged (lit 1: A = 1, B = 0 with the air off); slice 2a adds the air (wwFogFx, the shared
// lookdev_fog.hlsli as it stands); vkprecip.inc refuses a lit cell until slice 2b.
// cell_precip_field.hlsli is included unchanged (its hashes, roof test); pcParticleVk below is a twin-local copy of its
// pcParticle spelled in NV GL's order of operations. A change to pcParticle there must be mirrored here.
// profile: cs_6_0
#include "WeatherBindingsVk.hlsli"
#define WW_HLSL_SRC 1	// the shared .hlsli files give their readable half
// slice 2a: the air at the particle's centre, as cell_precip_gpu_gs.hlsl reads it outside a fragment stage
#define Sample( s, uv ) SampleLevel( s, uv, 0 )	// GLSL: texture() outside a fragment shader reads level 0
#define SampleCmp SampleCmpLevelZero
static float4 pcFragCoord;
static float3 pcDPx, pcDPy;
#define gl_FragCoord pcFragCoord
#define ddx( p ) pcDPx
#define ddy( p ) pcDPy

#define WW_LOOSE_TYPES 1
#include "cell_precip_field.hlsli"
#include "../air/lookdev_fog.hlsli"
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
	int pcRed;			// not GL's: gates only, 1 the cull radius 0 (cell_precip_gpu_vs.hlsl's PC_CULL_RED), 2 the air dropped
	float4 pcViewport;	// the frame's viewport (x, y, w, h): the centre's window position
#define WW_LOOSE_UNIFORMS 1
#include "../air/lookdev_fog.hlsli"
#undef WW_LOOSE_UNIFORMS
};
#include "cell_precip_field.hlsli"
#include "../air/lookdev_fog.hlsli"

// pcWrap in NV GL's order: the remainder fused, r = fma( -floor( x / L ), L, x )
float pcWrapVk( float x, float L )
{
	precise float f = floor( x / L );
	precise float r = mad( -f, L, x );
	return r >= L ? r - L : max( r, 0.0 );
}

// cell_precip_field.hlsli pcParticle in NV GL's order of operations (its vertex-stage NV_gpu_program5 assembly, driver
// 610.74): mad() where GL fuses, separate precise ops where it keeps a multiply and an add apart, and its reassociations
// ((om pcT) 0.7, (om sway sin) 0.7, the spin's 2^-23). sin / cos take the raw phase, as GL's SIN / COS do.
bool pcParticleVk( uint i, out float3 rel, out float3 vel, out float hs, out float angle, out float2 cellUv )
{
	uint h = pcHash( i ^ pcSeedS );
	float u0 = pcU01( h );
	h = pcHash( h + 0x1B873593u );
	float u1 = pcU01( h );
	h = pcHash( h + 0x1B873593u );
	float u2 = pcU01( h );
	h = pcHash( h + 0x1B873593u );
	int var = pcNCand > 0 ? pcCand[int( h % uint( pcNCand ) )] : pcBest;
	h = pcHash( h + 0x1B873593u );
	float a0 = pcU01( h ) * 6.283185307179586;
	h = pcHash( h + 0x1B873593u );
	precise float spU = float( h >> 8 ) * pcSpin;
	precise float spin = mad( spU, 1.0 / 8388608.0, -pcSpin );
	h = pcHash( h + 0x1B873593u );
	precise float ph = pcU01( h ) * 6.283185307179586;
	precise float om = mad( float( pcHash( h ^ 0x85EBCA6Bu ) >> 8 ), 0.8 / 16777216.0, 0.8 );
	float fall = pcFallU[var];
	precise float ph1 = mad( om, pcT, ph );
	precise float omT = om * pcT;
	precise float ph13 = ph * 1.3;
	precise float ph2 = mad( omT, 0.7, ph13 );
	float s1 = sin( ph1 ), c1 = cos( ph1 ), s2 = sin( ph2 ), c2 = cos( ph2 );
	precise float omS = om * pcSway;
	precise float vx = mad( omS, c1, pcWind.x );
	precise float vyS = omS * s2;
	precise float vy = mad( -vyS, 0.7, pcWind.y );
	vel = float3( vx, vy, -fall );
	float L = pcL;
	precise float bx = mad( u0, L, pcOffXY.x );
	precise float px = mad( s1, pcSway, bx );
	precise float by = mad( u1, L, pcOffXY.y );
	precise float sy = c2 * pcSway;
	precise float py = by + sy;
	precise float pz = mad( u2, L, pcOffZ[var] );
	rel = float3( pcWrapVk( px, L ), pcWrapVk( py, L ), pcWrapVk( pz, L ) ) - 0.5 * L;
	hs = pcHalfU[var];
	precise float an = mad( spin, pcT, a0 );
	angle = an;
	int cell = var % max( pcCells, 1 );
	cellUv = float2( float( cell % max( pcCols, 1 ) ), float( cell / max( pcCols, 1 ) ) ) * pcCellUv;
	if ( pcInner > 0.0 && all( (abs( rel ) < (float3)( 0.5 * pcInner )) ) )
		return false;
	return or( !pcRoofOn, pcRoofOpen( rel ) );
}

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
	bool keep = pcParticleVk( pcFirst + id.x, rel, vel, hs, angle, cellUv );
	float3 vsView = float3( dot( pcRow[0].xyz, rel ), dot( pcRow[1].xyz, rel ), dot( pcRow[2].xyz, rel ) );
	float3 vsVel = float3( dot( pcRow[0].xyz, vel ), dot( pcRow[1].xyz, vel ), dot( pcRow[2].xyz, vel ) );
	precise float vsHalf = hs / rsqrt( dot( pcRow[0].xyz, pcRow[0].xyz ) );	// GL: DIV by RSQ
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
	// in NV GL's geometry-stage order (its NV_gpu_program5 assembly): tl = 1 / RSQ, the alpha's factors regrouped
	float h = vsHalf;
	float k = pcFocal / depth;	// pixels per view unit here
	float2 tr = vsVel.xy * pcShutter;
	precise float tl = rcp( rsqrt( dot( tr, tr ) ) );
	float hd = max( h, pcMinPx / k );
	float4 col = vsColor;
	precise float aDen = hd * mad( hd, 2.0, tl );
	precise float aNum = h * ( h * 4.0 );
	precise float aF = ( aNum / aDen ) * col.a;
	col.a = aF * 0.5;
	float2 ax, ay;
	float hx = hd, hy = hd;
	if ( tl > 0.2 * hd ) {
		ax = tr / tl;
		precise float hxS = mad( tl, 0.5, hd );
		hx = hxS;
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
	// slice 2a: the air, once, at the centre (cell_precip_gpu_gs.hlsl: B = wwFogFx( 0 ), A = wwFogFx( lit ) - B); the
	// window position in NV GL's order (clip y-row first, then x, z, w; the viewport by fma)
	precise float4 cc0 = c.y * pcProj[1];
	precise float4 cc1 = mad( c.x, pcProj[0], cc0 );
	precise float4 cc2 = mad( c.z, pcProj[2], cc1 );
	precise float4 cc = cc2 + pcProj[3];
	precise float2 ndc = mad( cc.xy / cc.w, 0.5, 0.5 );
	precise float2 win = mad( ndc, pcViewport.zw, pcViewport.xy );
	pcFragCoord = float4( win, cc.z / cc.w * 0.5 + 0.5, 1.0 / cc.w );
	float pk = rcp( k );
	pcDPx = float3( pk, 0.0, 0.0 );
	pcDPy = float3( 0.0, pk, 0.0 );
	float3 lit = (float3)( 1.0 );	// slice 2b: vpLight (vkprecip.inc refuses a lit cell until then)
	float3 fogB = wwFogFx( (float3)( 0.0 ), c, false );
	float3 fogA = wwFogFx( lit, c, false ) - fogB;
	if ( ( pcRed & 2 ) != 0 ) {	// red: the air dropped (A = lit, B = 0)
		fogA = lit;
		fogB = (float3)( 0.0 );
	}
	r.a = float4( fogA, 0.0 );
	r.b = float4( fogB, 0.0 );
	pcOut[id.x] = r;
}
