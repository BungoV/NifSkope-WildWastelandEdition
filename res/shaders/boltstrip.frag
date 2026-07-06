#version 410 core

// Textured camera-facing strip for the BSProceduralLightningController preview

uniform sampler2D boltTexture;

in vec4 C;
in vec2 texCoord;

out vec4 fragColor;

void main()
{
	fragColor = texture( boltTexture, texCoord ) * C;
}
