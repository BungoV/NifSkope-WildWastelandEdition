// lane RTREFL1 / RTREFLUNIT1: the water's read of the ray-traced reflection hits (src/rtddgi/refl.comp, src/rtddgicell.cpp
// rtddgiReflPass). fo4_water.frag includes it under WW_CELL_RTREFL; tests/rtreflunit compiles the same file.
// The hits: half the view, bottom row first, one RGBA32F texel per 2 x 2 block of the full-size ray target (refl.comp
// reflPick); rgb the hit's linear radiance (the frame's units), a its distance (game units), 0 = none (the sky stays).
// Reads viewportDimensions (uniforms.glsl or the unit's own).
uniform bool waterRtOn;		// this frame's hits are bound (src/gl/cellwater.cpp)
uniform samplerBuffer waterRt;
uniform vec4 waterRtSize;	// the half size W, H; the full ray target's W, H (0 0: WW_CELL_RTREFL_TEXEL_RED, the half size scaled)

// this pixel in the view (0..1, bottom row first); bendTop = wwWaterSsrBend's offset, in the march's top-row-first uv
vec2 wwWaterRtView( vec2 fragXY, bool bent, vec2 bendTop )
{
	vec2 rv = clamp( ( fragXY - vec2( viewportDimensions.xy ) ) / vec2( viewportDimensions.zw ), 0.0, 1.0 );
	if ( bent )
		rv = clamp( rv + vec2( bendTop.x, -bendTop.y ), 0.0, 1.0 );
	return rv;
}

// the hits' texel of a view point: its full-size pixel, then that pixel's 2 x 2 block (the kernel's own pairing)
ivec2 wwWaterRtTexel( vec2 rv )
{
	ivec2 rs = max( ivec2( waterRtSize.xy ), ivec2( 1 ) );
	if ( waterRtSize.z > 0.0 && waterRtSize.w > 0.0 ) {
		ivec2 fs = ivec2( waterRtSize.zw );
		ivec2 fp = clamp( ivec2( rv * vec2( fs ) ), ivec2( 0 ), fs - 1 );
		return min( fp / 2, rs - 1 );
	}
	return clamp( ivec2( rv * vec2( rs ) ), ivec2( 0 ), rs - 1 );	// RTREFL1's rule: one texel off past mid-view on odd sizes
}

// rgb the hit's radiance, a 1 = a hit, 0 = none (off, under the surface, or the ray missed)
vec4 wwWaterRtHit( vec2 fragXY, bool bent, vec2 bendTop, bool under )
{
	if ( !waterRtOn || under )
		return vec4( 0.0 );
	ivec2 rs = max( ivec2( waterRtSize.xy ), ivec2( 1 ) );
	ivec2 rp = wwWaterRtTexel( wwWaterRtView( fragXY, bent, bendTop ) );
	vec4 rt = texelFetch( waterRt, rp.y * rs.x + rp.x );
	return rt.a > 0.0 ? vec4( rt.rgb, 1.0 ) : vec4( 0.0 );
}

// a hit takes the sky's place (as is: no tone, no sky visibility); none keeps the sky's bits
vec3 wwWaterRtOver( vec3 sky, vec4 hit )
{
	return hit.a > 0.5 ? hit.rgb : sky;
}
