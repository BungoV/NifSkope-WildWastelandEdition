
layout( local_size_x = 8, local_size_y = 8 ) in;
layout( std430, binding = 0 ) buffer HizBuf { float hiz[]; };
uniform int srcOff;
uniform int dstOff;
uniform ivec2 srcDim;
uniform ivec2 dstDim;
void main()
{
	ivec2 p = ivec2( gl_GlobalInvocationID.xy );
	if ( p.x >= dstDim.x || p.y >= dstDim.y )
		return;
	ivec2 a = p * 2;
	ivec2 b = min( a + 1, srcDim - 1 );
	float m = max( max( hiz[srcOff + a.y * srcDim.x + a.x], hiz[srcOff + a.y * srcDim.x + b.x] ),
		max( hiz[srcOff + b.y * srcDim.x + a.x], hiz[srcOff + b.y * srcDim.x + b.x] ) );
	hiz[dstOff + p.y * dstDim.x + p.x] = m;
}
