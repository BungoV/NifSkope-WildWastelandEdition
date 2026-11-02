#ifndef CELLTREEWIND_H
#define CELLTREEWIND_H

/* lane WINDTREE1: the game's tree wind on a loaded cell's Tree_Anim shapes (Todd's treat), one vertex, float32 in the
 * game shader's order. ww_treewind.glsl is the same law op for op (the colour draw); gl/sunshadow.cpp runs this one on
 * the CPU for the shape's shadow casters. Pure: no Qt, unit-tested by tests/treewind.
 *
 * k (per placed shape, set at the cell's build): pivot = the reference's position in the shape's model space (world axes),
 * objSum = the reference's world x + y + z, bound2 = 2 x its model's bound radius x scale (the bend's height unit),
 * amp = the STAT's DNAM leaf amplitude x scale (0 when the model's root is no BSLeafAnimNode), leafFreq = its DNAM leaf
 * frequency (1 then), sumAxis = the reference's rotation x (1, 1, 1) (the game sums the OBJECT normal; the weld's
 * normal is world, so s = dot( n, sumAxis )). w (per frame, the weather's wind): angle, the previous and current phase, min * 300, max * 300,
 * the wind frequency. weight = the vertex alpha (the game's saturate(alpha)); dist = camera to the reference, game units.
 */

#include <cmath>

struct WwTreeWindK
{
	float pivot[3] = { 0.0f, 0.0f, 0.0f };
	float objSum = 0.0f;
	float bound2 = 3.4e38f;
	float amp = 0.0f;
	float leafFreq = 1.0f;
	float sumAxis[3] = { 1.0f, 1.0f, 1.0f };   // R x (1, 1, 1): dot( world normal, it ) = the object normal's x + y + z
};

struct WwTreeWindW
{
	float angle = 0.0f, prevPhase = 0.0f, phase = 0.0f;
	float minS = 0.0f, maxS = 0.0f, freq = 0.0f;
};

//! The leaf amplitude's camera fade: full to 3600, none from 4600 (fLeafAnimDampenDistStart / End).
inline float wwTreeWindAmpFade( float dist )
{
	const float t = ( 4600.0f - dist ) * ( 1.0f / 1000.0f );
	return t < 0.0f ? 0.0f : ( t > 1.0f ? 1.0f : t );
}

//! One vertex: p (model space) and its unit-or-not normal n -> out. `phase` = w.phase (or w.prevPhase, the last frame).
inline void wwTreeWindVertex( const float p[3], const float n0[3], float weight, float dist, const WwTreeWindK & k,
	const WwTreeWindW & w, float phase, float out[3] )
{
	const float a = weight < 0.0f ? 0.0f : ( weight > 1.0f ? 1.0f : weight );
	const float nl = std::sqrt( n0[0] * n0[0] + n0[1] * n0[1] + n0[2] * n0[2] );
	const float inl = nl > 0.0f ? 1.0f / nl : 0.0f;
	const float n[3] = { n0[0] * inl, n0[1] * inl, n0[2] * inl };
	const float s = n[0] * k.sumAxis[0] + n[1] * k.sumAxis[1] + n[2] * k.sumAxis[2];
	const float base = 3.0f * s + k.objSum;
	const float F = k.leafFreq * 20.0f;
	const float A = k.amp * wwTreeWindAmpFade( dist ) * 0.035f;
	const float half = ( w.maxS - w.minS ) * 0.5f;
	const float sp = w.minS + half * std::sin( phase * ( ( F * w.freq ) * 5.0f ) + base );
	const float m = sp * A;
	float off[3];
	for ( int i = 0; i < 3; i++ )
		off[i] = p[i] + ( n[i] * m ) * a - k.pivot[i];
	const float h = ( off[0] + off[1] + off[2] ) / k.bound2;
	const float c = std::sin( ( phase * w.freq ) * F + ( h * -1.5f + k.objSum ) );
	const float g = w.minS + half * ( 0.3f * std::sin( 3.14159274f * c ) + std::sin( 6.28318548f * c ) + 2.0f );
	const float b = ( h * g / 3.0f ) * a;
	const float q[3] = { off[0] + std::cos( w.angle ) * b, off[1] + std::sin( w.angle ) * b, off[2] };
	const float ol = std::sqrt( off[0] * off[0] + off[1] * off[1] + off[2] * off[2] );
	const float ql = std::sqrt( q[0] * q[0] + q[1] * q[1] + q[2] * q[2] );
	const float r = ql > 0.0f ? ol / ql : 0.0f;
	for ( int i = 0; i < 3; i++ )
		out[i] = q[i] * r + k.pivot[i];
}

//! The weld's per-vertex weight: the bitangent's length is 2 + the vertex alpha on a tree-wind shape.
inline float wwTreeWindWeightOf( float bitangentLength )
{
	return bitangentLength > 1.5f ? bitangentLength - 2.0f : 0.0f;
}

#endif
