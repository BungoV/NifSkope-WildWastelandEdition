// Lane VKOBJ5 (plan row G4 round 3): particles_cell.prog's record pass (particles_view.hlsli). One thread a GL vertex of
// glDrawArrays( GL_POINTS, 0, n ): particles.vert, then particles.geom's per-primitive part, one record at the point's own
// index (GL's gl_PrimitiveIDIn), every slot written. The arithmetic is particles_vs.hlsl's and particles_gs.hlsl's as
// written there: a change to either must be mirrored here.
// profile: cs_6_0
#include "particles_view.hlsli"

StructuredBuffer<PfxIn> pfxIn : register(t0, space0);
RWStructuredBuffer<PfxRec> pfxOut : register(u0, space0);

[numthreads(64, 1, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
	uint n = 0, stride = 0;
	pfxIn.GetDimensions(n, stride);	// the host binds exactly n particles
	if (id.x >= n)
		return;
	const PfxIn p = pfxIn[id.x];

	// particles.vert
	float4	v = mul(float4( p.posAngle.xyz, 1.0 ), modelViewMatrix);
	float3	viewDir;
	if ( projectionMatrix[3][3] == 1.0 )
		viewDir = float3(0.0, 0.0, 1.0);	// orthographic view
	else
		viewDir = -v.xyz;
	const float4 color = select((vertexColorOverride > (float4)( 0.0 )), vertexColorOverride, p.color);

	// particles.geom up to its first EmitVertex
	float	sx = p.uvSize.z * particleScale.x;
	float	sy = p.uvSize.z * particleScale.y;
	float	ca = cos( p.posAngle.w );
	float	sa = sin( p.posAngle.w );

	PfxRec r;
	r.v = v;
	r.color = color;
	r.sprite = float4( ca, sa, sx, float( int( id.x ) ) );
	r.viewDirSy = float4( viewDir, sy );
	r.uvBase = float4( particleUV.xy + p.uvSize.xy, 0.0, 0.0 );
	pfxOut[id.x] = r;
}
