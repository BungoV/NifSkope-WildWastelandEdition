// HLSL source of ww_dither.glsl (include-only GLSL), lane HLSLSRC1 (notes/hlsltools/glsl2hlsl.py): the GLSL's comments,
// names and order kept. Its loose uniforms sit at the top under WW_LOOSE_UNIFORMS: the entry that includes this
// file includes it once more inside its LooseUniforms cbuffer with that defined (HLSL has one cbuffer for them).
#ifdef WW_HLSL_SRC
#if defined(WW_LOOSE_TYPES)	// the types its loose uniforms use

#elif defined(WW_LOOSE_UNIFORMS)
	int wwDitherOn;
	int wwDitherSeed;
#else
/* lane FOGGLOW1 (bungo 10-29, Memory Den fog banding): a display value goes into an 8-bit target (the view, the TAA
 * picture) as one of 256 levels. A smooth dark gradient (the night fog on a wall, the glow round a lamp) then steps in
 * flat plateaus a level apart: the bands and rings. A triangular (TPDF) noise of +-1 level added before the write
 * turns the steps into a fine grain whose mean is the true value (Gjol, "Banding in Games: A Noisy Rant", Playdead,
 * 2016). The noise fades to 0 at exactly 0 and 1, so black stays black and white white.
 * wwDitherOn 0 (WW_CELL_DITHER=0, the red): the value unchanged. wwDitherSeed: the TAA frame index (a camera path
 * renders frame k as k: a repeated frame is the same picture), so TAA averages a different noise every frame. */



// a uniform [0, 1) from a pixel and a seed (an integer hash: no lattice, no pattern a zoom shows)
float wwDitherHash( uint2 p, uint s )
{
	uint h = p.x * 1664525u + p.y * 1013904223u + s * 2654435769u;
	h ^= h >> 16;
	h *= 2246822519u;
	h ^= h >> 13;
	h *= 3266489917u;
	h ^= h >> 16;
	return float( h >> 8 ) * ( 1.0 / 16777216.0 );
}

float3 wwDither8( float3 c, float2 fragCoord )
{
	if ( wwDitherOn == 0 )
		return c;
	uint2 p = uint2( fragCoord );
	uint s = uint( wwDitherSeed );
	float n = wwDitherHash( p, 2u * s ) + wwDitherHash( p, 2u * s + 1u ) - 1.0;	// triangular on (-1, 1)
	float3 w = clamp( min( c, 1.0 - c ) * 255.0, 0.0, 1.0 );
	return c + n * w * ( 1.0 / 255.0 );
}
#endif	// WW_LOOSE_UNIFORMS
#else	// WW_HLSL_SRC: the per-variant files' half (HLSLBULK1, WW_FN_<name> picks functions); it goes with the last of them
// Shared HLSL for res/shaders/ww_dither.glsl (include-only GLSL), HLSLBULK1 round 2 (notes/hlslbulk1/tools/factor.py). No bindings: the
// functions below are the converted ones, they read the declarations of the shader that includes them (its cbuffers,
// resources, statics and LooseUniforms: the per-module shim and the entry own those). Define WW_FN_<name> for each
// wanted function before the #include (and #undef after), or WW_FN_ALL for all of them; a _V<n> suffix is a variant
// of the same GLSL function (a different define set at the entry). Fusion Shaders (F4FS) reuses the math from here.
// Included by: cell_hdr_ps.hlsl, game_taa_ps.hlsl, taa2_ps.hlsl

#if defined(WW_FN_wwDitherHash) || defined(WW_FN_ALL)
float wwDitherHash(uint2 p, uint s)
{
    uint h = ((p.x * 1664525u) + (p.y * 1013904223u)) + (s * 2654435769u);
    h ^= (h >> uint(16));
    h *= 2246822519u;
    h ^= (h >> uint(13));
    h *= 3266489917u;
    h ^= (h >> uint(16));
    return float(h >> uint(8)) * 5.9604644775390625e-08f;
}
#endif

#if defined(WW_FN_wwDither8) || defined(WW_FN_ALL)
float3 wwDither8(float3 c, float2 fragCoord)
{
    if (wwDitherOn == 0)
    {
        return c;
    }
    uint2 p = uint2(fragCoord);
    uint s = uint(wwDitherSeed);
    uint2 nPart4 = p;
    uint nPart3 = 2u * s;
    uint2 nPart2 = p;
    uint nPart = (2u * s) + 1u;
    // triangular on (-1, 1)
    float n = (wwDitherHash(nPart4, nPart3) + wwDitherHash(nPart2, nPart)) - 1.0f;
    float3 w = clamp(min(c, 1.0f.xxx - c) * 255.0f, 0.0f.xxx, 1.0f.xxx);
    return c + ((w * n) * 0.0039215688593685626983642578125f);
}
#endif
#endif	// WW_HLSL_SRC
