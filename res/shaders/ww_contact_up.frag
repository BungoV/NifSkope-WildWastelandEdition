#version 410 core

// lane PERFFIX2 (item 7): the half-size contact mask (Low / Medium, and every SHADOWPAR1 tier) brought to full size
// with a depth-aware bilinear upsample, so a silhouette's shadow does not bleed onto the surface behind it (the
// receivers used to read the half-size mask with a plain bilinear tap: halos). A mask texel counts only as far as
// its receiver's depth (the full-size texel its march started from) agrees with this pixel's own, within `tolerance`
// x depth (lane GAPLOOK1's far resolve rule, ww_contact_farcomp.frag); where no texel agrees, the one nearest in depth.

uniform sampler2D gbuf;		// the full-size opaque pass (.a linear depth; 1e6 none)
uniform sampler2D mask;		// the half-size contact mask (R8)
uniform ivec2 fullSize;
uniform ivec2 maskSize;
uniform float maskScale;	// full pixels per mask pixel (2)
uniform float tolerance;

out float fragMask;

void main()
{
	ivec2 fq = ivec2( gl_FragCoord.xy );
	float z = texelFetch( gbuf, fq, 0 ).a;
	vec2 mp = gl_FragCoord.xy / maskScale - 0.5;
	ivec2 base = ivec2( floor( mp ) );
	vec2 fr = mp - vec2( base );
	if ( z >= 9.0e5 || z <= 0.0 ) {
		fragMask = texelFetch( mask, clamp( ivec2( floor( gl_FragCoord.xy / maskScale ) ), ivec2( 0 ), maskSize - 1 ), 0 ).r;
		return;
	}
	float wsum = 0.0, acc = 0.0, best = 1e30, bestM = 1.0;
	for ( int j = 0; j < 2; j++ ) {
		for ( int i = 0; i < 2; i++ ) {
			ivec2 c = clamp( base + ivec2( i, j ), ivec2( 0 ), maskSize - 1 );
			float m = texelFetch( mask, c, 0 ).r;
			ivec2 rq = clamp( ivec2( floor( ( vec2( c ) + 0.5 ) * maskScale ) ), ivec2( 0 ), fullSize - 1 );
			float cz = texelFetch( gbuf, rq, 0 ).a;
			float dz = abs( cz - z );
			if ( dz < best ) {
				best = dz;
				bestM = m;
			}
			float bw = ( i == 0 ? 1.0 - fr.x : fr.x ) * ( j == 0 ? 1.0 - fr.y : fr.y );
			float w = bw * clamp( 1.0 - dz / ( z * tolerance ), 0.0, 1.0 );
			wsum += w;
			acc += w * m;
		}
	}
	fragMask = wsum > 1e-4 ? acc / wsum : bestM;
}
