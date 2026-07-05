#version 410 core

// Minimal particle fragment shader: sample the bound sprite texture and
// multiply by the per-particle vertex colour. Avoids the full lighting /
// material path in default.frag, which is not set up for particles and was
// tinting the sprites incorrectly.

struct Texture {
	vec2 uvCenter;
	vec2 uvScale;
	vec2 uvOffset;
	float uvRotation;
	int coordSet;
	int textureUnit;
};

#include "uniforms.glsl"

uniform sampler2D textureUnits[10];
uniform Texture textures[10];

in vec2 texCoords[9];
in vec4 C;

out vec4 fragColor;

vec4 getTexture( int n )
{
	float	r_c = cos( textures[n].uvRotation );
	float	r_s = sin( textures[n].uvRotation ) * -1.0;
	vec2	offs = texCoords[textures[n].coordSet].st - textures[n].uvCenter;
	offs = vec2( offs.x * r_c - offs.y * r_s, offs.x * r_s + offs.y * r_c );
	offs = offs * textures[n].uvScale + textures[n].uvCenter + textures[n].uvOffset;

	return texture( textureUnits[textures[n].textureUnit - 1], offs );
}

void main()
{
	vec4	t = getTexture( 0 );
	fragColor = t * C;
}
