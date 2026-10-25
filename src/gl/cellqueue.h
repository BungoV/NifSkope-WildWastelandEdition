#ifndef CELLQUEUE_H
#define CELLQUEUE_H

/* lane GPUDRIVEN1 (2026-10-22): RENDER QUEUES, STEP A (the Division 2 way, notes/perfresearch1/GPUDRIVEN1.md).
 *
 * The CPU cull stays. What it keeps goes into queues by render state, and a queue is issued as one unit:
 *  - the sun cascades: solid one-sided, solid two-sided and alpha-tested casters draw from one shared geometry
 *    pool (positions + first UV set, 16-bit indices with a base vertex), their model-view matrices in one buffer
 *    per frame, read per draw through baseInstance, and each solid queue is ONE indirect multi-draw per cascade.
 *    A depth-only LEQUAL pass keeps the nearest depth whatever the order, so the queues are exact by construction.
 *    CPU-skinned casters keep today's per-draw path.
 *  - the main walks (opaque, AO, reflections): opaque, two-sided, alpha-tested, and the rest (skinned, blended,
 *    decals, x-ray, hidden triangles), sorted and counted; the flush draws them in tree order (lane GPUFIX1:
 *    equal-depth ties then match the queues off). Blended shapes stay in the sorted pass.
 *
 * Switches: WW_GPUDRIVEN=0 (all of it off, the 4.2 context again), WW_GPUDRIVEN_CSM=0, WW_GPUDRIVEN_MAIN=0.
 * Reds (WW_GPUDRIVEN_RED, comma list): perdraw = today's per-draw paths and their cost on the new context;
 * order = a fixed-seed shuffle inside every queue (the main pass must change, the cascades must not);
 * queueorder = the main pass drawn queue by queue (before lane GPUFIX1: moves equal-depth pixels).
 * WW_GPUDRIVEN_CENSUS=1 prints queue sizes, calls and commands, and the textures bound with more than one sampler
 * state (what bindless textures, a later step, must keep apart). */

#include <QString>

#include <cstdint>
#include <vector>

class NifSkopeOpenGLContext;
class Node;
class NodeList;
class QSurfaceFormat;
class Scene;
class Shape;

namespace GpuDriven
{

bool on();
bool csm();
bool mainPass();
bool redOrder();
bool census();

//! GLView: ask for a 4.6 core context when on and the driver makes one (else the format is left at 4.2).
void requestContext( QSurfaceFormat & fmt );

// ---- the main walks

enum Queue { Opaque, TwoSided, AlphaTest, Rest, QueueCount };

//! Scene::collectShapes: arm the queues for one walk; false when off, selecting, or already armed (nested walk).
bool mainBegin( Scene * scene );
//! BSShape::drawShapes: true when the shape was queued (the caller returns and is drawn again at the flush).
bool mainArmed();
bool mainTake( Node * shape, int queue );
//! Draw every taken shape in tree order (the reds: queue by queue, shuffled under order), and disarm.
void mainFlush( Scene * scene, NodeList * secondPass );

// ---- the cascades' shared geometry pool

struct PoolEntry
{
	std::int32_t baseVertex = 0;
	std::uint32_t firstIndex = 0;
	std::uint32_t numIndices = 0;
	bool uv = false;	// the first UV set is in the pool
};

struct DrawCmd
{
	std::uint32_t count, instanceCount, firstIndex;
	std::int32_t baseVertex;
	std::uint32_t baseInstance;
};

//! Once per frame, before poolEnsure: the context's pool, its stale sweep and compaction. False: no pool here.
bool poolBegin( NifSkopeOpenGLContext * r );
//! The shape's geometry in the pool (uploaded when new or changed); false when it cannot go in.
bool poolEnsure( NifSkopeOpenGLContext * r, const Shape * sh, PoolEntry & out );
//! Upload the frame's matrices (16 floats each, column-major) and commands, bind the pool's vertex array and
//! the indirect buffer. Attributes: 0 position, 7 first UV set, 8-11 the per-draw matrix (divisor 1).
void poolBind( NifSkopeOpenGLContext * r, const std::vector<float> & matrices, const std::vector<DrawCmd> & cmds );
//! Rebind the vertex array (after another path bound its own).
void poolRebind( NifSkopeOpenGLContext * r );
//! commands [first, first + n) of the uploaded set: one indirect multi-draw (or one call per command without it)
void poolDraw( NifSkopeOpenGLContext * r, std::size_t first, std::size_t n );
void poolUnbind( NifSkopeOpenGLContext * r );
QString poolCensus( NifSkopeOpenGLContext * r );

// ---- census

//! BSShaderLightingProperty::bind: the wrap mode a texture was bound with (census only).
void samplerCensus( unsigned int texture, int wrapMode );
//! One line to stderr when it changed since the last one of the same tag.
void censusLine( const char * tag, const QString & line );

}

#endif
