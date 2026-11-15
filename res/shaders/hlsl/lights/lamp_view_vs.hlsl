// Lane VKLAMP1 (2026-11-15): the lamps' plain shadow cube faces on the present device (vklamp.inc lampRun), the Vulkan
// twin of cell_shadowdepth_vs.hlsl with the same products in the same order. Position at location 0 (binding 0), the
// caster's model-view matrix GL column-major at 1-4 (binding 1, per instance); the face block (b1, one 256-byte block
// a face, dynamic offset) holds shRelFromView, shClipFromRel and shRadiusNear as GL uploads them. GL's clip z in
// [-w, w] stays as it is: the pipeline sets negativeOneToOne (VK_EXT_depth_clip_control).
// profile: vs_6_0
cbuffer LampFace : register(b1, space0)
{
	row_major float4x4 shRelFromView;
	row_major float4x4 shClipFromRel;
	float2 shRadiusNear;	// radius, near clip
};

struct LampIn
{
	[[vk::location(0)]] float3 pos : TEXCOORD0;
	[[vk::location(1)]] float4 mv0 : TEXCOORD1;
	[[vk::location(2)]] float4 mv1 : TEXCOORD2;
	[[vk::location(3)]] float4 mv2 : TEXCOORD3;
	[[vk::location(4)]] float4 mv3 : TEXCOORD4;
};

struct LampOut
{
	float3 shRel : TEXCOORD0;
	float4 pos : SV_Position;
};

LampOut main( LampIn i )
{
	float4x4 modelViewMatrix;
	modelViewMatrix[0] = i.mv0;
	modelViewMatrix[1] = i.mv1;
	modelViewMatrix[2] = i.mv2;
	modelViewMatrix[3] = i.mv3;
	float4 rel = mul(( mul(float4( i.pos, 1.0 ), modelViewMatrix) ), shRelFromView);
	LampOut o;
	o.shRel = rel.xyz;
	o.pos = mul(float4( rel.xyz, 1.0 ), shClipFromRel);
	return o;
}
