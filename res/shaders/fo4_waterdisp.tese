#version 410 core

// lane WATERLOOK1 step 2 (src/gl/cellocean.h): each vertex of the split water moved by the wave spectrum's heights and
// chop (water_ocean.glsl wwWaterOceanDisp, the same cascades, offsets and gains the normals read), full out to
// waterOceanDisp.x, none past .y. Positions are interpolated in clip space (exact: a linear map of the view position)
// and the move added through the projection; the fragment reads the spectrum at the undisplaced point (wwOcBase), as the
// normals are defined there (Tessendorf's Lagrangian grid).

layout ( triangles, fractional_odd_spacing, ccw ) in;

in vec3 tcLightDir[];
in vec3 tcViewDir[];
in vec2 tcTexCoord[];
in mat3 tcBtn[];
flat in vec4 tcA[];
in vec4 tcC[];
flat in vec4 tcD[];
in float tcRawAlpha[];
flat in mat3 tcRefl[];

out vec3 LightDir;
out vec3 ViewDir;
out vec2 texCoord;
out mat3 btnMatrix;
flat out vec4 A;
out vec4 C;
flat out vec4 D;
out float rawVertexAlpha;
flat out mat3 reflMatrix;
out vec2 wwOcBase;	// the undisplaced world xy (u)

#include "uniforms.glsl"

uniform vec4 cellRow[3];	// world = dot( row, ( posView, 1 ) ) (cell_lights.glsl)

#include "water_info.glsl"
#include "water_ocean.glsl"

const float kOcCellEdge = 4096.0;	// u: the cell view's water quad (src/cellview.cpp), split at most waterOceanDisp.w times

void main()
{
	vec3 w = gl_TessCoord;
	precise vec4 clip = w.x * gl_in[0].gl_Position + w.y * gl_in[1].gl_Position + w.z * gl_in[2].gl_Position;
	precise vec3 pv = -( w.x * tcViewDir[0] + w.y * tcViewDir[1] + w.z * tcViewDir[2] );
	LightDir = w.x * tcLightDir[0] + w.y * tcLightDir[1] + w.z * tcLightDir[2];
	texCoord = w.x * tcTexCoord[0] + w.y * tcTexCoord[1] + w.z * tcTexCoord[2];
	btnMatrix = w.x * tcBtn[0] + w.y * tcBtn[1] + w.z * tcBtn[2];
	A = tcA[0];
	C = w.x * tcC[0] + w.y * tcC[1] + w.z * tcC[2];
	D = tcD[0];
	rawVertexAlpha = w.x * tcRawAlpha[0] + w.y * tcRawAlpha[1] + w.z * tcRawAlpha[2];
	reflMatrix = tcRefl[0];

	vec4 p = vec4( pv, 1.0 );
	vec3 world = vec3( dot( cellRow[0], p ), dot( cellRow[1], p ), dot( cellRow[2], p ) );
	wwOcBase = world.xy;
	float dist = length( pv );
	float fade = 1.0 - smoothstep( waterOceanDisp.x, waterOceanDisp.y, dist );
	if ( waterOceanOn && projectionMatrix[3][3] != 1.0 && fade > 0.0 ) {
		// the vertices' spacing here: the split's target in pixels, or the finest a GPU splits a quad
		float pxu = projectionMatrix[1][1] * 0.5 * float( viewportDimensions.w ) / max( dist, 1.0 );
		float step = max( waterOceanDisp.z / pxu, kOcCellEdge / waterOceanDisp.w );
		vec3 dw = fade * wwWaterOceanDisp( world.xy, step );
		// world -> view: cellRow's 3 x 3 is R^T / s, so R s dw = ( sum row_k dw_k ) s^2
		vec3 dv = ( cellRow[0].xyz * dw.x + cellRow[1].xyz * dw.y + cellRow[2].xyz * dw.z ) / dot( cellRow[0].xyz, cellRow[0].xyz );
		pv += dv;
		clip += projectionMatrix * vec4( dv, 0.0 );
	}
	ViewDir = -pv;
	gl_Position = clip;
}
