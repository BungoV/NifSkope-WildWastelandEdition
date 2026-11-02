// Screen-space ambient obscurance read by cell-lit draws (src/gl/celllights.h, cell_ao.frag).
// Included after cell_lights.glsl.

uniform bool cellAoOn;			// an opaque cell-lit draw with this frame's obscurance (never a blended one)
uniform sampler2D cellAo;		// half the view, top row first, bilinear
uniform vec4 cellAoRect;		// the viewport: x, y, 1 / w, 1 / h
uniform int cellAoEarly;		// lane PERF1: 1 = the obscurance pass (probe 20) returns once it has the normal; 2 = its red (full shading, marked)
#ifdef WW_CELL_RTAO	// lane RTAO1: the ray-traced occlusion past the screen-space reach (half the view, bottom row first: x visibility)
uniform bool cellRtAoOn;
uniform samplerBuffer cellRtAo;
uniform vec4 cellRtAoSize;		// Wh, Hh

// MIRROR src/rtddgi.cpp aoTexel's layout: the fragment's half-size texel (the opaque pass's own pixels, so cellAoOn draws only)
float cellRtAoAt()
{
	if ( !cellRtAoOn )
		return 1.0;
	ivec2 q = ivec2( gl_FragCoord.xy - cellAoRect.xy ) / 2;
	int W = int( cellRtAoSize.x + 0.5 ), H = int( cellRtAoSize.y + 0.5 );
	if ( q.x < 0 || q.y < 0 || q.x >= W || q.y >= H )
		return 1.0;
	return texelFetch( cellRtAo, q.y * W + q.x ).r;
}

// lane RTAO1 (RTREFL1-SPECOCC): the cube share's specular occlusion, before the SSR mix (the screen hits keep theirs)
float cellRtAoSpec()
{
	return cellAoOn ? cellRtAoAt() : 1.0;
}
#endif

// the game multiplies its whole deferred composite by this, before the fog
float cellAoFactor()
{
	if ( !cellAoOn )
		return 1.0;
	vec2 uv = ( gl_FragCoord.xy - cellAoRect.xy ) * cellAoRect.zw;
#ifdef WW_CELL_RTAO
	// lane RTAO1: the two ranges do not overlap (the rays count hits past WW_CELL_RTAO_NEAR only), so they multiply
	return texture( cellAo, vec2( uv.x, 1.0 - uv.y ) ).r * cellRtAoAt();
#else
	return texture( cellAo, vec2( uv.x, 1.0 - uv.y ) ).r;
#endif
}

// the obscurance's opaque pass: view normal and linear depth in game units
vec4 cellAoPassOut( vec3 normalView, vec3 posView )
{
	return vec4( normalize( normalView ), -posView.z * length( cellRow[0].xyz ) );
}
