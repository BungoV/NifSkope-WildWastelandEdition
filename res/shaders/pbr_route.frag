#version 410 core

// PBR route debug view (lane PBRR1). The colour is chosen per shape on the CPU
// from the resolved route (Renderer::setupProgramRoute); this pass only lights
// it by N.L so the shape keeps its form.

uniform vec3 routeColor;

in vec3 LightDir;
in mat3 btnMatrix;

out vec4 fragColor;

void main()
{
	vec3 n = normalize( btnMatrix[2] );
	if ( !gl_FrontFacing )
		n = -n;
	float ndl = max( dot( n, normalize( LightDir ) ), 0.0 );
	fragColor = vec4( routeColor * ( 0.4 + 0.6 * ndl ), 1.0 );
}
