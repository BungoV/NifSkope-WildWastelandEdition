#version 410 core

/* lane PERFFIX3 (item 8): the FO4CS bloom's final tent, once per PIXEL (src/features/post/cellbloom.cpp combine). cell_hdr.frag
 * runs per sample, but the tent's uv is ( floor( gl_FragCoord ) + 0.5 ) / frame for every sample of a pixel, so its
 * 9 (27 with the glare's dispersion) bilinear taps gave the same value up to 4 times at 4x MSAA. This pass writes that
 * tent at its pixel into a full-size RGBA32F target (the same expression, the same uniforms' names and values), and the
 * tone map reads it with one texelFetch. It runs only while the tent's lod is the same at every sample of the frame
 * (no Fog Bloom Boost per sample under Bloom Mip): alpha holds the lod it used. cellbloom.cpp runs it only with the
 * glare's dispersion (27 taps): the plain 9-tap tent per sample costs about one RGBA32F fetch per sample (measured,
 * tests/features/post/bloomcomb). Exactness gate: tests/features/post/bloomcomb/bloomcomb_check.py. */

#include "cell_bloom.glsl"

uniform sampler2D cellIsFoBloom;	// the chain's top (the tone map's own sampler, unit, filter and mips)
uniform vec4 cellIsFoBloomParams;	// radius (pixels), bloom weight, dispersion, the Bloom Mip mode (cell_hdr.frag)
uniform vec2 cellIsFoBloomFrame;	// the frame's size

out vec4 fragColor;

void main()
{
	vec2 uv = ( floor( gl_FragCoord.xy ) + 0.5 ) / cellIsFoBloomFrame;
	// cell_hdr.frag's bloomLod with the gain at 1 (no fog bloom per sample): 2 for the red wide, else 0
	float bloomLod = cellIsFoBloomParams.w < 0.0 ? 2.0 : 0.0;
	if ( bloomLod > 0.0 )
		fragColor = vec4( bloomTentDispersedLod( cellIsFoBloom, uv, cellIsFoBloomParams.x, cellIsFoBloomParams.z, bloomLod ),
			bloomLod );
	else
		fragColor = vec4( bloomTentDispersed( cellIsFoBloom, uv, cellIsFoBloomParams.x, cellIsFoBloomParams.z ), 0.0 );
}
