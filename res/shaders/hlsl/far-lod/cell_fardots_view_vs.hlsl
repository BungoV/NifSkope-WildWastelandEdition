// Lane VKFAR1 (2026-11-13, row G3): the far bulb dots on the present device's view (vkpresent.cpp farDotsRun), the
// vertex stage of cell_fardots.vert drawn as instanced quads: one instance a bulb (per-instance position + intensity),
// four strip vertices its corners. A GL point of gl_PointSize s covers the window square of side s around its centre and
// gives the fragment gl_PointCoord (0..1, origin top left); the quad's corners carry wwPointCoord the same way, so
// cell_fardots_ps.hlsl reads it unchanged. GLSL drew size 0 (nothing) where the fade is 0: the quad collapses there.
// GL clip z in [-w, w] -> Vulkan's [0, w]. No geometry stage (the present device does not enable it).
// profile: vs_6_0
#include "FarLodBindingsVk.hlsli"
cbuffer FarDotsView : register(b1, space0)
{
    float4 dotRow[3];
    row_major float4x4 dotProj;
    float3 dotCam;	// the camera, world
    int dotRed;	// 1 = never faded (debug)
    float2 dotBand;
    float dotFocal;	// f, pixels
    float dotBulb;	// the bulb radius, world units
    int4 dotViewport;	// GL's viewport: x, y, width, height (the point size is in its pixels)
};

struct VsIn
{
    WW_LOC(0) float3 vertexPosition : TEXCOORD0;	// per instance: the bulb, world
    WW_LOC(1) float3 vertexColor : TEXCOORD1;	// per instance: its intensity I
    uint corner : SV_VertexID;	// 0..3, a triangle strip
};

struct VsOut
{
    WW_LOC(0) float2 wwPointCoord : TEXCOORD0;	// cell_fardots_ps.hlsl's input order
    WW_LOC(1) float3 dotValue : TEXCOORD1;
    WW_LOC(2) float dotSigma : TEXCOORD2;
    WW_LOC(3) float3 dotView : TEXCOORD3;
    float4 pos : SV_Position;
};

// NVIDIA GL's lowering of dot() and mat4 * vec4 as mad chains (the window centre is snapped to 1/256 px, so a last
// bit of the clip position moves a dot by a grid step)
float FarDot4(float4 a, float4 b)
{
    return mad(a.z, b.z, mad(a.y, b.y, mad(a.x, b.x, a.w * b.w)));
}

float4 FarProj(float3 v)
{
    return mad(dotProj[2], v.z, mad(dotProj[0], v.x, dotProj[1] * v.y)) + dotProj[3];
}

VsOut main(VsIn i)
{
    VsOut o;
    float4 w = float4(i.vertexPosition, 1.0f);
    float3 v = float3(FarDot4(dotRow[0], w), FarDot4(dotRow[1], w), FarDot4(dotRow[2], w));
    float4 c = FarProj(v);
    o.dotView = v;
    float d = max(length(i.vertexPosition - dotCam), 1.0f);
    float fade = (dotRed & 1) != 0 ? 1.0f : smoothstep(dotBand.x, dotBand.y, d);
    o.dotSigma = max(0.60000002384185791015625f, (dotFocal * dotBulb) / d);
    float k = dotFocal / d;
    o.dotValue = (((fade * i.vertexColor) * k) * k) / ((6.283185482025146484375f * o.dotSigma) * o.dotSigma).xxx;
    float size = fade > 0.0f ? min(2.0f * ceil(3.0f * o.dotSigma) + 1.0f, 63.0f) : 0.0f;
    float2 corner = float2(float(i.corner & 1u), float(i.corner >> 1));
    // the square of side size around the centre, in window pixels (y up = NDC y up). GL snaps the point's centre to the
    // 1/256 pixel grid and measures gl_PointCoord from it; size is an odd whole number, so centre +- size / 2 lies on
    // that grid too and the rasterizer's own snap (vkfardots.inc refuses a device with other than 8 subpixel bits)
    // leaves the corners where they are: build them in window pixels, never through NDC (a last bit there moved a
    // corner by 1/256 px).
    float2 vp = float2(max(dotViewport.z, 1), max(dotViewport.w, 1));
    // GL's viewport transform, unfused (a fused or (ndc + 1) form moved 2x more centres a grid step)
    precise float2 cw = (c.xy / c.w) * (vp * 0.5f) + (float2(dotViewport.xy) + vp * 0.5f);
    float2 q = round(cw * 256.0f) / 256.0f + float2(corner.x - 0.5f, 0.5f - corner.y) * size;
    o.wwPointCoord = corner;
    o.pos = float4(((q - float2(dotViewport.xy)) / vp * 2.0f - 1.0f) * c.w, (c.z + c.w) * 0.5f, c.w);
    return o;
}
