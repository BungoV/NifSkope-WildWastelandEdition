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
#ifdef PC_LOOK
	float4 drop;
	float4 thin;	// w gOsc
	float4 thick;
	float4 domL;
	float4 domE;
#endif
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
#ifdef PC_LOOK
	// slice 3 (precip_look_view_vs.hlsl): cell_precip_look's interpolants, the geometry stage's order
	nointerpolation float4 gDrop : TEXCOORD7;
	nointerpolation float gOsc : TEXCOORD8;
	nointerpolation float3 gThin : TEXCOORD9;
	nointerpolation float3 gThick : TEXCOORD10;
	nointerpolation float4 gDomL : TEXCOORD11;
	nointerpolation float3 gDomE : TEXCOORD12;
	nointerpolation float2 gAx : TEXCOORD13;
#endif
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
	// NV GL's geometry-stage order: off = fma( ax, q.x hx, ay (q.y hy) ), clip = ((y M1 + x M0) + z M2) + M3
	precise float2 offY = ay * ( q.y * hy );
	precise float2 off = mad( ax, q.x * hx, offY );
	precise float3 p = c + float3( off, 0.0 );
	VsOut o;
	o.gQ = q;
	precise float2 uq = mad( q, 0.5, 0.5 );
	o.gUv = mad( uq, pcAtlas.xy, r.ext.zw );
	o.gView = p;
	o.gCol = r.col;
	o.gBox = r.box;
	o.gA = r.a.xyz;
	o.gB = r.b.xyz;
#ifdef PC_LOOK
	o.gDrop = r.drop;
	o.gOsc = r.thin.w;
	o.gThin = r.thin.xyz;
	o.gThick = r.thick.xyz;
	o.gDomL = r.domL;
	o.gDomE = r.domE.xyz;
	o.gAx = ax;
#endif
	precise float4 cp0 = p.y * pcProj[1];
	precise float4 cp1 = mad( p.x, pcProj[0], cp0 );
	precise float4 cp2 = mad( p.z, pcProj[2], cp1 );
	precise float4 cp = cp2 + pcProj[3];
	o.pos = r.c.w != 0.0 ? float4( cp.xy, ( cp.z + cp.w ) * 0.5, cp.w ) : float4( 0.0, 0.0, -1.0, 1.0 );
	return o;
}
