#ifndef CELLSHADOWOCC_H
#define CELLSHADOWOCC_H

/*! lane PCMB1 part B: the sun casters' receiver cull, pure (tests/features/near/shadowocc runs it).
 *
 *  Every reader of the sun's cascades (the colour pass, the air, the reflections' relight, the DDGI probes) picks the
 *  cascade by the main camera's planar view depth (ww_sunshadow.glsl csmSelect), so cascade i is only ever read for
 *  receivers whose depth lies in its slab [nearDepth, farDepth] (cascade 0 also behind the camera: no near side). A
 *  caster whose shadow volume -- its bounding sphere swept to infinity along the direction its shadow falls -- never
 *  enters that slab writes texels no receiver reads: dropping it leaves every shaded pixel as it was. The camera's side
 *  planes are NOT used: the relight and the probes read the cascades off screen. */
namespace WwShadowOcc
{

/*! True = the sphere (centre c, radius r; view space, the camera looks down -z) swept along d (the direction the shadow
 *  falls, any length) can reach depth -z in [nearDepth, farDepth]. Conservative: false only when it provably cannot. */
inline bool reachesSlab( const float c[3], float r, const float d[3], float nearDepth, float farDepth )
{
	const float depth = -c[2], dd = -d[2];	// the depth and its rate along the shadow
	if ( depth - r > farDepth && dd >= 0.0f )
		return false;	// beyond the slab and the shadow runs away from the camera (or level)
	if ( depth + r < nearDepth && dd <= 0.0f )
		return false;	// in front of the slab and the shadow runs toward the camera (or level)
	return true;
}

/*! The view-space direction a cascade's shadows fall: the gradient of its light-space depth, row 2 of the column-major
 *  view -> clip matrix (the depth test keeps the smaller depth, so a caster shades what lies at greater depth). */
inline void shadowDir( const float * clipFromView, float d[3] )
{
	d[0] = clipFromView[2];
	d[1] = clipFromView[6];
	d[2] = clipFromView[10];
}

} // namespace WwShadowOcc

#endif
