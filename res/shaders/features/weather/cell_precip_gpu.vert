#version 410 core

/* lane PERFFIX3 D (2026-11-01): one precipitation particle a point, computed here from gl_VertexID (cell_precip_field.glsl,
 * the GPU twin of src/features/weather/precipmath.h particleAt): no CPU field, no upload. Then the frustum cull: a particle whose drawn
 * quad (cell_precip_gpu.geom: flat-on, so every corner sits at the centre's depth) cannot touch the view is flagged and
 * the geometry shader emits nothing and lights nothing.
 * Red WW_CELL_PRECIP_RED=cpufield: the old program (cell_precip.prog, the CPU field). */

#include "cell_precip_field.glsl"

uniform vec4 pcRow[3];		// world -> view rows (scale in); only the rotation is used: the eye is the view origin
uniform mat4 pcProj;
uniform float pcFocal;		// pixels per view unit at depth 1
uniform float pcMinPx;		// the least half size drawn, pixels
uniform float pcShutter;	// seconds
uniform float pcThin;		// how many real particles one stands for (far tier), the opacity factor

out vec3 vsView;
out vec4 vsColor;
out vec2 vsCell;
out float vsAngle;
out float vsHalf;
out vec3 vsVel;
out float vsCull;
#ifdef PC_TEST
out vec3 vsRel;
#endif

void main()
{
	vec3 rel, vel;
	float hs, angle;
	vec2 cellUv;
	bool keep = pcParticle( uint( gl_VertexID ), rel, vel, hs, angle, cellUv );
	vsView = vec3( dot( pcRow[0].xyz, rel ), dot( pcRow[1].xyz, rel ), dot( pcRow[2].xyz, rel ) );
	vsVel = vec3( dot( pcRow[0].xyz, vel ), dot( pcRow[1].xyz, vel ), dot( pcRow[2].xyz, vel ) );
	vsHalf = hs * length( pcRow[0].xyz );
	vsColor = vec4( 1.0, 1.0, 1.0, pcThin );
	vsCell = cellUv;
	vsAngle = angle;
#ifdef PC_TEST
	vsRel = rel;
#endif
	// the cull: the quad's corners lie within R = 2 hd + travel of the centre, in the centre's depth plane
	float depth = -vsView.z;
	bool cull = !keep || depth < 1.0;
	if ( !cull ) {
		float hd = max( vsHalf, pcMinPx * depth / pcFocal );
		float R = 2.0 * hd + length( vsVel.xy ) * pcShutter;
#ifdef PC_CULL_RED
		R = 0.0;	// tests/features/weather/precipgpu red: the quad's extent ignored (edge particles lost)
#endif
		vec4 c = pcProj * vec4( vsView, 1.0 );
		float rx = abs( pcProj[0][0] ) * R, ry = abs( pcProj[1][1] ) * R;
		cull = c.x - rx > c.w || c.x + rx < -c.w || c.y - ry > c.w || c.y + ry < -c.w;
	}
	vsCull = cull ? 1.0 : 0.0;
	gl_Position = vec4( vsView, 1.0 );
}
