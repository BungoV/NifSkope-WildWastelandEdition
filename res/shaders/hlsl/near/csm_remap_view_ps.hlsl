// Lane VKSHADOW6 (2026-11-16): the sun cascade cache's Scroll remap on the present device, the Vulkan twin of
// lookdev_csmremap.frag. The old static depth at this texel's place in the old window (scratch layer `layer`), mapped
// into the new depth range; cleared depth stays 1, texels the old window never covered get 1. Drawn LEQUAL (GL draws it
// GL_ALWAYS onto a fresh clear before its casters: the same minimum). No y flip: FragCoord.y is GL's window row.
// hlsl profile: ps_6_0
struct RemapPush
{
    int layer, srcX, srcY, srcW, srcH;
    float zScale, zBias;
};
[[vk::push_constant]] RemapPush P;
[[vk::binding(0, 0)]] Texture2DArray<float> srcMap;

float main( float4 fc : SV_Position ) : SV_Depth
{
    const int2 p = int2( fc.xy ) + int2( P.srcX, P.srcY );
    float d = 1.0f;
    if ( p.x >= 0 && p.y >= 0 && p.x < P.srcW && p.y < P.srcH ) {
        const float o = srcMap.Load( int4( p, P.layer, 0 ) );
        d = o >= 1.0f ? 1.0f : clamp( mad( o, P.zScale, P.zBias ), 0.0f, 1.0f );
    }
    return d;
}
