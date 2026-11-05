#version 410 core

// lane LIGHTS3 (LENSFLARE1): a lens-flare sprite, added to the tone-mapped frame (cell_lensflare.vert)

uniform sampler2D flareTex;

in vec2 flareUV;
in vec3 flareColor;
out vec4 fragColor;

void main()
{
	fragColor = vec4( texture( flareTex, flareUV ).rgb * flareColor, 0.0 );
}
