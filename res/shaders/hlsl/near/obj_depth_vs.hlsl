// hlsl profile: vs_6_0
// Lane VKOBJ1 (2026-11-13, plan row G4): the objects' opaque depth prepass on the present device (vkpresent.cpp objRun),
// the Vulkan twin of lookdev_csmdepth_q.vert with the same products in the same order. Position at location 0 (binding 0),
// the occluder's model-view matrix GL column-major at 1-4 (binding 1, per instance), csmClipFromView GL column-major in the
// push block. GL's clip z in [-w, w] stays as it is: the pipeline maps it with VK_EXT_depth_clip_control (negativeOneToOne),
// since a perspective w would round a halved z differently from GL's window transform.
// profile: vs_6_0
struct ObjPush
{
    row_major float4x4 clip;
};
[[vk::push_constant]] ObjPush P;

struct ObjIn
{
    [[vk::location(0)]] float3 pos : TEXCOORD0;
    [[vk::location(1)]] float4 mv0 : TEXCOORD1;
    [[vk::location(2)]] float4 mv1 : TEXCOORD2;
    [[vk::location(3)]] float4 mv2 : TEXCOORD3;
    [[vk::location(4)]] float4 mv3 : TEXCOORD4;
};

float4 main( ObjIn i ) : SV_Position
{
    float4x4 mv;
    mv[0] = i.mv0;
    mv[1] = i.mv1;
    mv[2] = i.mv2;
    mv[3] = i.mv3;
    return mul( mul( float4( i.pos, 1.0f ), mv ), P.clip );
}
