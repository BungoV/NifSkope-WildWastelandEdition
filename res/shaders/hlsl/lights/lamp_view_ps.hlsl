// Lane VKLAMP1 (2026-11-15): the lamps' plain shadow cube faces on the present device (vklamp.inc lampRun), the Vulkan
// twin of cell_shadowdepth_ps.hlsl for solid casters (alpha-tested casters stay on GL, so no alpha branch here):
// depth = distance to the light over its radius, nothing nearer than the near clip casts.
// Lane VKLAMP3: |rel| in NV GL's own order (its program binary: DP3, RSQ, RCP, DIV.SAT). Its DP3 is
// fma(z, z, fma(y, y, x * x)); NV's Vulkan dot() starts at y and left 1-2 float ulps on some texels (174 of 3.0M
// texels 1 D16 unit off). In this order lampview reads every face EXACT.
// profile: ps_6_0
cbuffer LampFace : register(b1, space0)
{
	row_major float4x4 shRelFromView;
	row_major float4x4 shClipFromRel;
	float2 shRadiusNear;	// radius, near clip
};

struct LampIn
{
	float3 shRel : TEXCOORD0;
};

float main( LampIn i ) : SV_Depth
{
	const float3 r = i.shRel;
	precise float dd = mad( r.z, r.z, mad( r.y, r.y, r.x * r.x ) );
	float d = 1.0 / rsqrt( dd );
	if ( d < shRadiusNear.y )
		discard;
	return saturate( d / shRadiusNear.x );
}
