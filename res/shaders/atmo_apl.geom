#version 410 core

/* lane PERFFIX3 (item 1): the aerial-perspective volume in ONE layered draw. The full-screen quad's two triangles go to
 * every slice of the 3D target (attached layered): invocation k writes layer k and hands the fragment its slice index,
 * the value the per-slice path set as the uniform atApSlice (atmo_ap.frag), so each froxel runs the same maths.
 * 32 invocations = the most slices any tier draws (Ultra 64x64x32, src/gl/atmosphere.cpp buildAp). */

layout ( triangles, invocations = 32 ) in;
layout ( triangle_strip, max_vertices = 3 ) out;

uniform vec3 atApDims;	// width, height, slices

flat out float atApSliceIn;

void main()
{
	if ( float( gl_InvocationID ) >= atApDims.z )
		return;
	for ( int i = 0; i < 3; i++ ) {
		gl_Position = gl_in[i].gl_Position;
		gl_Layer = gl_InvocationID;
		atApSliceIn = float( gl_InvocationID );
		EmitVertex();
	}
	EndPrimitive();
}
