// Screen-space ambient obscurance read by cell-lit draws (src/gl/celllights.h, cell_ao.frag).
// Included after cell_lights.glsl.

uniform bool cellAoOn;			// an opaque cell-lit draw with this frame's obscurance (never a blended one)
uniform sampler2D cellAo;		// half the view, top row first, bilinear
uniform vec4 cellAoRect;		// the viewport: x, y, 1 / w, 1 / h

// the game multiplies its whole deferred composite by this, before the fog
float cellAoFactor()
{
	if ( !cellAoOn )
		return 1.0;
	vec2 uv = ( gl_FragCoord.xy - cellAoRect.xy ) * cellAoRect.zw;
	return texture( cellAo, vec2( uv.x, 1.0 - uv.y ) ).r;
}

// the obscurance's opaque pass: view normal and linear depth in game units
vec4 cellAoPassOut( vec3 normalView, vec3 posView )
{
	return vec4( normalize( normalView ), -posView.z * length( cellRow[0].xyz ) );
}
