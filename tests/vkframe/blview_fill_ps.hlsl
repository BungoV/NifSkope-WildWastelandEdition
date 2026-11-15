// Lane VKOBJ4 gap 1 (2026-11-13): the readback gate's fill pass. Runs once per sample (it reads SV_SampleIndex) and
// writes values the gate can predict exactly for every pixel and sample:
//   colour = (x / 256, y / 256, sample, 0.5): each an exact half
//   depth  = j / 4096 with j = (x & 255) + 256 * sample (j < 2048, so a D24 stores exactly j * 4096)
// The stencil comes from the draw (the gate draws once per sample, the sample mask on that sample, the ref sample + 1).

struct FillOut
{
	float4 colour : SV_Target0;
	float depth : SV_Depth;
};

FillOut main( float4 position : SV_Position, uint sampleIndex : SV_SampleIndex )
{
	const uint x = uint( position.x ), y = uint( position.y );  // the pixel (the sample's offset is under one pixel)
	FillOut o;
	o.colour = float4( float( x ) / 256.0, float( y ) / 256.0, float( sampleIndex ), 0.5 );
	o.depth = float( ( x & 255 ) + 256 * sampleIndex ) / 4096.0;
	return o;
}
