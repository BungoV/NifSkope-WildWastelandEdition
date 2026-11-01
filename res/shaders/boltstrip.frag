#version 410 core

// Textured camera-facing strip for the BSProceduralLightningController preview

uniform sampler2D boltTexture;

// lane FXPALETTE1: the effect shader's greyscale to palette (fo4_effectshader.frag); both off = the plain strip
uniform sampler2D GreyscaleMap;
uniform bool greyscaleColor;
uniform bool greyscaleAlpha;
uniform float paletteRow;	// the effect's base colour R, the palette row

in vec4 C;
in vec2 texCoord;

out vec4 fragColor;

void main()
{
	vec4 tex = texture( boltTexture, texCoord );
	fragColor = tex * C;
	if ( greyscaleColor )
		fragColor.rgb = textureLod( GreyscaleMap, vec2( tex.g, paletteRow ), 0.0 ).rgb;
	if ( greyscaleAlpha )
		fragColor.a = textureLod( GreyscaleMap, vec2( tex.a, C.a ), 0.0 ).a;
}
