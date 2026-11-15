// Lane VKLAMP1 (2026-11-15): the lamps' plain shadow cube faces on the present device (vklamp.inc lampRun), the Vulkan
// twin of cell_shadowdepth_ps.hlsl for solid casters (alpha-tested casters stay on GL, so no alpha branch here):
// depth = distance to the light over its radius, nothing nearer than the near clip casts.
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
	float d = length( i.shRel );
	if ( d < shRadiusNear.y )
		discard;
	return clamp( d / shRadiusNear.x, 0.0, 1.0 );
}
