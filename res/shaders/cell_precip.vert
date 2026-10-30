#version 410 core

/* lane WEATHERLOOK1 phase B: one precipitation particle a point (src/gl/cellprecip.cpp; the field is
 * src/gl/precipmath.h). World -> view by pcRow (rows of the view transform, scale in); the quad is built in
 * cell_precip.geom, flat-on to the camera. */

layout ( location = 0 ) in vec3 vertexPosition;	// world
layout ( location = 1 ) in vec4 vertexColor;	// rgb tint, a opacity (a far dot's thinning already in)
layout ( location = 2 ) in vec2 vertexUV;		// the atlas cell's corner (uv)
layout ( location = 3 ) in float vertexAngle;	// spin about the view axis, radians
layout ( location = 4 ) in float particleSize;	// the quad's half size, world units
layout ( location = 5 ) in vec3 vertexVelocity;	// world units / s

uniform vec4 pcRow[3];

out vec3 vsView;
out vec4 vsColor;
out vec2 vsCell;
out float vsAngle;
out float vsHalf;
out vec3 vsVel;

void main()
{
	vec4 w = vec4( vertexPosition, 1.0 );
	vsView = vec3( dot( pcRow[0], w ), dot( pcRow[1], w ), dot( pcRow[2], w ) );
	vsVel = vec3( dot( pcRow[0].xyz, vertexVelocity ), dot( pcRow[1].xyz, vertexVelocity ), dot( pcRow[2].xyz, vertexVelocity ) );
	vsHalf = particleSize * length( pcRow[0].xyz );
	vsColor = vertexColor;
	vsCell = vertexUV;
	vsAngle = vertexAngle;
	gl_Position = vec4( vsView, 1.0 );
}
