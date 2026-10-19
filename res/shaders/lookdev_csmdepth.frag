#version 410 core

// The cascade caster pass (lane CSM1): depth only, no colour attachment.
// Lane TERRSHADOW1: csmAlphaCut > 0 cuts an alpha-tested caster (leaf cards, fences) by its base map's alpha.

uniform sampler2D BaseMap;
uniform float csmAlphaCut;

in vec2 casterUv;

void main()
{
	if ( csmAlphaCut > 0.0 && texture( BaseMap, casterUv ).a < csmAlphaCut )
		discard;
}
