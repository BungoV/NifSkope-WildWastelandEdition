#version 410 core

/* Lane RTXFIX1 (2026-10-30): the Ray-traced GI probes view, vertex stage. No vertex attributes: gl_VertexID picks the
 * unit sphere's corner (probeMesh, from meshBase), gl_InstanceID the probe (probeList, from listBase: x = the probe's
 * index, y = its state code for the states view). The probe's centre is the volume's own (the grid point + the atlas's
 * relocation offset), so the sphere sits where the read takes the probe, scroll and relocation included. */

#include "uniforms.glsl"

uniform mat4 modelViewMatrix;
uniform samplerBuffer probeMesh;
uniform samplerBuffer probeList;
uniform int meshBase;
uniform int listBase;
uniform float probeRad;
uniform samplerBuffer cellDdgiAtlas;
uniform vec4 cellDdgiOrigin;
uniform vec4 cellDdgiStep;
uniform ivec4 cellDdgiDims;

out vec3 probeN;
out vec3 viewPos;
flat out int probeIdx;
flat out float probeState;

void main()
{
	vec3 m = texelFetch( probeMesh, meshBase + gl_VertexID ).xyz;
	vec4 inst = texelFetch( probeList, listBase + gl_InstanceID );
	int p = int( inst.x + 0.5 );
	int px = p % cellDdgiDims.x;
	int py = ( p / cellDdgiDims.x ) % cellDdgiDims.y;
	int pz = p / ( cellDdgiDims.x * cellDdgiDims.y );
	vec4 pr = texelFetch( cellDdgiAtlas, 192 * cellDdgiDims.w + p );
	vec3 c = cellDdgiOrigin.xyz + vec3( float( px ), float( py ), float( pz ) ) * cellDdgiStep.xyz + pr.xyz;
	vec4 v = modelViewMatrix * vec4( c + m * probeRad, 1.0 );
	probeN = m;
	viewPos = v.xyz;
	probeIdx = p;
	probeState = inst.y;
	gl_Position = projectionMatrix * v;
}
