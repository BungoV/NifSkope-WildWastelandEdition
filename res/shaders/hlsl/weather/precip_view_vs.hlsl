// lane VKFAR2 (row L8 slice 1, 2026-11-14): cell_precip_gpu_gs.hlsl's four corners on the present device (no geometry
// stage): one instance a particle (its record from precip_view_cs.hlsl), four triangle-strip vertices, GL's corner order
// and the geometry stage's expressions. The outputs are precip_view_ps.hlsl's inputs, declared in the same order (the
// locations follow the order). A culled particle's corners all sit behind the near plane: nothing drawn.
// GL's clip depth (-w .. w) is moved to Vulkan's (0 .. w) as the far dots do; y is not flipped (GL's rows from the
// bottom are the image's rows from the top in memory, the viewport says the same).
// profile: vs_6_0
cbuffer PrecipView : register(b1, space0)
{
	row_major float4x4 pcProj;	// GL's pcProj (the compute pass's bytes)
	float4 pcAtlas;			// GL's pcAtlas: xy one cell in uv
	int pcRed;				// gates only: 1 the strip's last two corners swapped
};

struct PrecipRec
{
	float4 c;
	float4 axes;
	float4 ext;
	float4 col;
	float4 box;
	float4 a;
	float4 b;
};
StructuredBuffer<PrecipRec> pcRec : register(t1, space0);

struct VsOut
{
	float2 gUv : TEXCOORD0;
	float2 gQ : TEXCOORD1;
	float3 gView : TEXCOORD2;
	nointerpolation float4 gCol : TEXCOORD3;
	nointerpolation float4 gBox : TEXCOORD4;
	nointerpolation float3 gA : TEXCOORD5;
	nointerpolation float3 gB : TEXCOORD6;
	float4 pos : SV_Position;
};

VsOut main( uint corner : SV_VertexID, uint inst : SV_InstanceID )
{
	PrecipRec r = pcRec[inst];
	int v = int( corner );
	if ( ( pcRed & 1 ) != 0 && v >= 2 )
		v = 5 - v;
	float2 q = float2( ( v & 1 ) != 0 ? 1.0 : -1.0, ( v & 2 ) != 0 ? 1.0 : -1.0 );
	float3 c = r.c.xyz;
	float2 ax = r.axes.xy, ay = r.axes.zw;
	float hx = r.ext.x, hy = r.ext.y;
	float3 p = c + float3( ax * ( q.x * hx ) + ay * ( q.y * hy ), 0.0 );
	VsOut o;
	o.gQ = q;
	o.gUv = r.ext.zw + pcAtlas.xy * ( q * 0.5 + 0.5 );
	o.gView = p;
	o.gCol = r.col;
	o.gBox = r.box;
	o.gA = r.a.xyz;
	o.gB = r.b.xyz;
	float4 cp = mul(float4( p, 1.0 ), pcProj);
	o.pos = r.c.w != 0.0 ? float4( cp.xy, ( cp.z + cp.w ) * 0.5, cp.w ) : float4( 0.0, 0.0, -1.0, 1.0 );
	return o;
}
