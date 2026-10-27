#ifndef CELLGPUCULL_H
#define CELLGPUCULL_H

/* lane GPUDRIVEN1 (2026-10-22): GPU CULLING, STEP B (notes/perfresearch1/GPUDRIVEN1.md; the niagara / meshoptimizer way,
 * both MIT).
 *
 * On the main camera walk, after the CPU cull (gl/cellcull.h) and the step A queues (gl/cellqueue.h):
 *  1. every solid one-sided or two-sided shape the last frame drew opaque goes into a depth prepass of its own
 *     (the step A geometry pool, the same projection and polygon offset, the main frame's size, samples and format);
 *  2. a compute pass builds a max-depth pyramid (Hi-Z) from it;
 *  3. a compute pass tests every cluster (a meshlet: up to 128 triangles in a row inside one placement) of every
 *     shape against the frustum, its normal cone (one-sided shapes) and the Hi-Z, and writes one indirect draw
 *     command per cluster (instanceCount 0 when culled);
 *  4. each shape then draws its clusters with ONE glMultiDrawElementsIndirect on its own element buffer.
 * Conservative by construction: a cluster is culled only when every point of its sphere lies farther than the farthest
 * prepass depth under its screen box (the frontmost surface is never culled), when all of its triangles face away,
 * or when it lies wholly outside a frustum plane. Anything whose vertices move in the shader (skinned, grass, far
 * sway) or whose program, blend, depth or coverage state was not seen opaque last frame is drawn as before.
 *
 * Switches: WW_GPUDRIVEN_B=0 (the step A path, unchanged), WW_GPUDRIVEN_B_CONE=0, WW_GPUDRIVEN_B_OCC=0,
 * WW_GPUDRIVEN_B_FRUSTUM=0. Reds (WW_GPUDRIVEN_B_RED): hard = the occlusion test reads the sphere's centre depth
 * instead of its nearest (culls what peeks out: the pixel gate must FAIL); nocull = all of the work, nothing culled
 * (the exact pixels at the new cost). WW_GPUDRIVEN_CENSUS=1 adds "gpudriven b:" lines: shapes, occluders, clusters,
 * culled by frustum / cone / occlusion, triangles, draws, and the prepass / Hi-Z / cull GPU ms. */

#include <QtGlobal>

class Node;
class NifSkopeOpenGLContext;
class Scene;
class Shape;

namespace GpuDrivenB
{

bool on();

//! Scene::drawShapes: the walk that draws the camera's frame (AO, reflections and probes have their own walks).
void cameraWalk( bool on );

//! BSShape::drawShapes, queuing into Opaque / TwoSided / AlphaTest: what the culling needs that is not public.
struct Note
{
	int queue = 0;
	bool twoSided = false, swaying = false, fullLod = false;
	const void * program = nullptr;	// the program the shape drew with last
};
void note( Shape * sh, const Note & n );

//! GpuDriven::mainFlush, before the queues draw: the prepass, the Hi-Z and the cull. After them: finish.
void prepare( Scene * scene );
void finish();

//! BSShape::drawShapes, after setupProgram: remember the program and the GL state the shape draws with.
void learn( Shape * sh, const void * program );

/*! BSShape::drawShapes, instead of the CPU cull's draw: true when the shape was drawn here (its clusters, or its CPU
 *  ranges when they do not fall on cluster bounds). False: not a prepared shape, draw as before. */
bool draw( Scene * scene, Shape * sh, qsizetype numTriangles );
//! lane BINDLESSMDI1: draw() would take the shape (no side effect)
bool takes( const Shape * sh );

}

#endif
