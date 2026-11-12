// Lane VKFRAME1 (raster tone map gate): lays a multisample attachment out as W*S x H (column x * S + s), the way
// tonemap_gl_ref.py's UNPACK program does on GL, so tonemapmatch.cpp compares the fragment pass with the same bits.
[[vk::binding( 0, 0 )]] Texture2DMS<float4> UnpackSrc : register( t0 );
[[vk::binding( 1, 0 )]] [[vk::image_format( "rgba32f" )]] RWTexture2D<float4> UnpackDst : register( u0 );

[numthreads( 8, 8, 1 )]
void main( uint3 id : SV_DispatchThreadID )
{
	uint w, h, S;
	UnpackSrc.GetDimensions( w, h, S );
	if ( any( id.xy >= uint2( w, h ) ) )
		return;
	for ( uint s = 0; s < S; s++ )
		UnpackDst[uint2( id.x * S + s, id.y )] = UnpackSrc.Load( int2( id.xy ), s );
}
