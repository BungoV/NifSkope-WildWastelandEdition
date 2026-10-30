#version 410 core

/* lane WEATHERLOOK1 phase B: a precipitation particle's quad, flat-on to the camera (bungo 10-28: every crystal
 * flat-on), turned by its slow spin about the view axis. Coverage is kept whatever the drawn size:
 *   stretch  the particle's travel in the view plane over the shutter (pcShutter, 1/120 s) lengthens the quad along
 *            it (airpart_data.md "draw-time stretch"); the swept area is 2h (2h + travel)
 *   small    a quad under pcMinPx pixels of half size is drawn at pcMinPx
 *   opacity  x 4 h^2 / ((2h' + travel) 2h'), h the true half size, h' the drawn one (the box average of the flake)
 * pcFar = 1: a far dot (precipmath.h far tier): its colour is the cell's mean, read at the 1-texel-a-cell mip */

layout ( points ) in;
layout ( triangle_strip, max_vertices = 4 ) out;

uniform mat4 pcProj;
uniform vec4 pcAtlas;		// cell size in uv (x, y), the deepest mip a near quad reads, the mip with one texel a cell
uniform vec2 pcInset;		// half a texel of that deepest mip, in uv
uniform float pcShutter;	// seconds
uniform float pcFocal;		// pixels per view unit at depth 1
uniform float pcMinPx;		// the least half size drawn, pixels
uniform int pcFar;
uniform bool pcHasTex;
uniform sampler2D pcTex;

in vec3 vsView[];
in vec4 vsColor[];
in vec2 vsCell[];
in float vsAngle[];
in float vsHalf[];
in vec3 vsVel[];

out vec2 gUv;
out vec2 gQ;
out vec3 gView;
flat out vec4 gCol;
flat out vec4 gBox;	// the cell's uv box, inset half a texel of the deepest mip read (no neighbour in a bilinear read)

void main()
{
	vec3 c = vsView[0];
	float depth = -c.z;
	if ( depth < 1.0 )
		return;
	float h = vsHalf[0];
	float k = pcFocal / depth;	// pixels per view unit here
	vec2 tr = vsVel[0].xy * pcShutter;
	float tl = length( tr );
	float hd = max( h, pcMinPx / k );
	vec4 col = vsColor[0];
	col.a *= 4.0 * h * h / ( ( 2.0 * hd + tl ) * 2.0 * hd );
	vec2 ax, ay;
	float hx = hd, hy = hd;
	if ( tl > 0.2 * hd ) {
		ax = tr / tl;
		hx = hd + 0.5 * tl;
	} else {
		ax = vec2( cos( vsAngle[0] ), sin( vsAngle[0] ) );
	}
	ay = vec2( -ax.y, ax.x );
	vec2 cell = pcAtlas.xy;
	gBox = vec4( vsCell[0] + pcInset, vsCell[0] + cell - pcInset );
	if ( pcFar != 0 ) {
		// the cell's mean: 16 taps at the one-texel-a-cell mip (all one texel there; a capped chain averages its 4 x 4)
		vec4 m = vec4( 0.0 );
		if ( pcHasTex ) {
			for ( int i = 0; i < 4; i++ )
				for ( int j = 0; j < 4; j++ )
					m += textureLod( pcTex, vsCell[0] + cell * ( vec2( float( i ), float( j ) ) + 0.5 ) * 0.25, pcAtlas.w );
			m *= 1.0 / 16.0;
		} else {
			m = vec4( 1.0, 1.0, 1.0, 0.3 );
		}
		col = vec4( col.rgb * m.rgb, col.a * m.a );
	}
	gCol = col;
	for ( int v = 0; v < 4; v++ ) {
		vec2 q = vec2( ( v & 1 ) != 0 ? 1.0 : -1.0, ( v & 2 ) != 0 ? 1.0 : -1.0 );
		vec3 p = c + vec3( ax * ( q.x * hx ) + ay * ( q.y * hy ), 0.0 );
		gQ = q;
		gUv = vsCell[0] + cell * ( q * 0.5 + 0.5 );
		gView = p;
		gl_Position = pcProj * vec4( p, 1.0 );
		EmitVertex();
	}
	EndPrimitive();
}
