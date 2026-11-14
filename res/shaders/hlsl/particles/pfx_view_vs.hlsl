// Lane VKOBJ5 (plan row G4 round 3): particles_cell.prog's expand stage (particles_view.hlsli). Drawn as a TRIANGLE_STRIP
// of 4 vertices, one instance a record: SV_InstanceID = the primitive, SV_VertexID = particles.geom's EmitVertex order
// (c11, c01, c10, c00; the volPartDump probe's k = 0..3). GL's clip z is kept (the pipeline's depth-clip-control). The
// outputs are the geom's that particles_cell.frag reads, at the geom's locations. The corner arithmetic is
// particles_gs.hlsl's as written there: a change to it must be mirrored here.
// profile: vs_6_0
#include "particles_view.hlsli"

StructuredBuffer<PfxRec> pfxRec : register(t1, space0);

struct PfxVsOut
{
    WW_LOC(4) float3 ViewDir : TEXCOORD4;
    WW_LOC(6) float2 texCoords[9] : TEXCOORD6;
    WW_LOC(1) float4 C : TEXCOORD1;
    WW_LOC(15) float3 pPos : TEXCOORD15;
    WW_LOC(16) float2 pQ : TEXCOORD16;
    WW_LOC(17) nointerpolation float4 pSprite : TEXCOORD17;
    float4 gl_Position : SV_Position;
};

PfxVsOut main(uint vid : SV_VertexID, uint iid : SV_InstanceID)
{
	const PfxRec r = pfxRec[iid];
	const float4 center = r.v;	// gl_in[0].gl_Position
	const float ca = r.sprite.x, sa = r.sprite.y, sx = r.sprite.z, sy = r.viewDirSy.w;
	const float2 uvBase = r.uvBase.xy;
	PfxVsOut o;
	o.ViewDir = r.viewDirSy.xyz;
	o.C = r.color;
	o.pSprite = r.sprite;

	float2 uv;
	if ( volPartDump > 0 ) {
		// the probe: the centre's texel and point on all four corners, the quad over this particle's 8 pixels
		const int id = int( r.sprite.w );
		uv = uvBase + float2( 0.5, 0.5 ) * particleUV.zw;
		float x0 = float( id * 8 ) / float( volPartDump ) * 2.0 - 1.0;
		float x1 = float( id * 8 + 8 ) / float( volPartDump ) * 2.0 - 1.0;
		for ( int i = 0; i < 9; i++ )
			o.texCoords[i] = uv;
		o.pPos = center.xyz;
		o.pQ = (float2)( 0.0 );
		o.gl_Position = float4( ( vid & 1 ) != 0 ? x1 : x0, ( vid & 2 ) != 0 ? 1.0 : -1.0, 0.0, 1.0 );
		return o;
	}

	// rotated corner offsets: (u,v) in [-1,1] scaled by the sprite half-size
	float4 c;
	float2 q;
	if ( vid == 0 ) {
		c = float4(  ( ca - sa ) * sx, (  sa + ca ) * sy, 0.0, 0.0 );	// c11
		q = float2( 1.0, 1.0 );
	} else if ( vid == 1 ) {
		c = float4(  ( -ca - sa ) * sx, ( -sa + ca ) * sy, 0.0, 0.0 );	// c01
		q = float2( -1.0, 1.0 );
	} else if ( vid == 2 ) {
		c = float4(  ( ca + sa ) * sx, (  sa - ca ) * sy, 0.0, 0.0 );	// c10
		q = float2( 1.0, -1.0 );
	} else {
		c = float4(  ( -ca + sa ) * sx, ( -sa - ca ) * sy, 0.0, 0.0 );	// c00
		q = float2( -1.0, -1.0 );
	}
	uv = uvBase + ( q * 0.5 + 0.5 ) * particleUV.zw;
	for ( int i = 0; i < 9; i++ )
		o.texCoords[i] = uv;
	o.pPos = ( center + c ).xyz;
	o.pQ = q;
	o.gl_Position = mul(( center + c ), projectionMatrix);
	return o;
}
