// Lane VKOBJ4 gap 1 (2026-11-13): the readback gate's fill pass (blview_colour.inc, the 4-sample arm). One triangle
// that covers the whole frame; GL draws it from this file's SPIR-V (tests/hlsllive/build_spv.py).

float4 main( uint vertex : SV_VertexID ) : SV_Position
{
	// vertices 0, 1, 2 at (-1, -1), (3, -1), (-1, 3): the frame is inside the triangle
	const float2 at = float2( ( vertex & 1 ) * 4.0 - 1.0, ( vertex >> 1 ) * 4.0 - 1.0 );
	return float4( at, 0.0, 1.0 );
}
