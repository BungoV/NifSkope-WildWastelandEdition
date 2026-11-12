
layout( local_size_x = 8, local_size_y = 8 ) in;
layout( std430, binding = 0 ) writeonly buffer HizBuf { float hiz[]; };
#ifdef MS
uniform sampler2DMS depthTex;
#else
uniform sampler2D depthTex;
#endif
uniform int samples;
uniform ivec2 dim;
void main()
{
	ivec2 p = ivec2( gl_GlobalInvocationID.xy );
	if ( p.x >= dim.x || p.y >= dim.y )
		return;
#ifdef MS
	float m = 0.0;
	for ( int s = 0; s < samples; s++ )
		m = max( m, texelFetch( depthTex, p, s ).r );
#else
	float m = texelFetch( depthTex, p, 0 ).r;
#endif
	hiz[p.y * dim.x + p.x] = m;
}
