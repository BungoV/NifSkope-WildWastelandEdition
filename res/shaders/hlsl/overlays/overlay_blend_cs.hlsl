// Lane VKFLIGHT4 (2026-11-16): the view's QPainter overlay (F3 line, pass timers, labels, gizmo) over the Vulkan view.
// GL no longer paints it: QPainter draws into a CPU image (QImage ARGB32 premultiplied, top row first), the drawing
// thread hands the box that holds ink, and this kernel blends it over the view image before the present's flipped blit.
//
// Frame is GL's view resolved into the present ring slot (RGBA8, GL's row 0 at the bottom, so the overlay's row y lands
// on Frame row viewH - 1 - y). Overlay holds the box's pixels, one uint each as QImage stores them (A << 24 | R << 16 |
// G << 8 | B, premultiplied). GL's QPainter blended with ONE, ONE_MINUS_SRC_ALPHA on every channel: the same here.

[[vk::binding( 0, 0 )]] [[vk::image_format( "rgba8" )]] RWTexture2D<float4> Frame;
[[vk::binding( 1, 0 )]] ByteAddressBuffer Overlay;

struct BlendJob
{
	uint x0;     // the box in the overlay (top-left origin), in pixels
	uint y0;
	uint width;
	uint height;
	uint viewH;  // the view's height (the flip)
	uint red;    // gate reds, bits: 1 no flip, 2 red and blue swapped, 4 straight alpha (colour times alpha again)
};
[[vk::push_constant]] BlendJob Job;

[numthreads( 8, 8, 1 )]
void main( uint3 id : SV_DispatchThreadID )
{
	if ( id.x >= Job.width || id.y >= Job.height )
		return;
	const uint p = Overlay.Load( ( id.y * Job.width + id.x ) * 4 );
	if ( p == 0 )
		return;  // no ink: the view stays as GL drew it
	float4 s = float4( float( ( p >> 16 ) & 255u ), float( ( p >> 8 ) & 255u ), float( p & 255u ), float( p >> 24 ) ) / 255.0;
	if ( ( Job.red & 2u ) != 0u )
		s.rgb = s.bgr;
	if ( ( Job.red & 4u ) != 0u )
		s.rgb *= s.a;
	const uint y = Job.y0 + id.y;
	const int2 at = int2( Job.x0 + id.x, ( Job.red & 1u ) != 0u ? y : Job.viewH - 1u - y );
	const float4 d = Frame[at];
	Frame[at] = s + d * ( 1.0 - s.a );
}
