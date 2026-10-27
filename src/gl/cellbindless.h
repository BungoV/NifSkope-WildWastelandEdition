#ifndef CELLBINDLESS_H
#define CELLBINDLESS_H

/* lane BINDLESS2 (bindless step 2 + step 3): Show > "Bindless draws" (WW_BINDLESS, default off).
 * A cell-lit draw (fo4_cell / fo4_cellcsm, FO4 lighting shader) takes its twin fo4_cellbl / fo4_cellcsmbl: the
 * material's seven 2D maps are resident texture handles and the draw's matrices sit in one row of a per-draw buffer
 * (binding 12, a persistent ring), read by uniform wwDrawIdx; its geometry is drawn from one arena per vertex layout
 * with a base vertex. A handle freezes its texture's sampler state (GL_ARB_bindless_texture), so later sets of that
 * texture are skipped and counted ("conflicts": the census gate is 0 of them).
 * Reds: WW_BINDLESS_RED=sampler (the handle samples a nearest, mirrored sampler), =pooloff1 (each pooled draw takes
 * the span allocated before its own), =nopool (handles and the buffer, the shapes' own buffers), =cellname (lane BINDLESS2FIX:
 * the twins not taken as cell programs, so no cell uniform reaches them -- the black, fogless frame of the MERGE1026 hold),
 * =slow (lane BINDLESSPERF1: the per-draw path before it -- two vertex array queries, three binds, a texture query and
 * restore per map; each query waits for the driver's thread, which made the pooled frame slower than the plain one). */

#include "gl/glcontext.hpp"

#include <cstdint>

class Shape;

namespace WwBl
{
enum Slot
{
	BaseMap, NormalMap, GlowMap, BacklightMap, SpecularMap, GreyscaleMap, EnvironmentMap, SlotCount = 8
};
//! the slot of a material sampler name, -1 when the bindless shader keeps it bound
int slotOf( const char * var );

//! the look is on and this context has the extensions (and the program twins linked)
bool on();
//! 0 none, 1 sampler, 2 pooloff1, 3 nopool, 4 cellname, 5 slow
int red();
//! lane BINDLESSPERF1: on, pooling, and not the slow red: a draw binds its own vertex array only when it is not pooled
bool fastOn();
extern GLuint resolvedTex;	// lane BINDLESSPERF1: the 2D texture a bindless sampler's bind chain bound (0: ask GL)

extern bool anyFrozen;	// a texture holds a handle (keeps the off path's test to one load)
extern bool resolving;	// a bindless sampler resolves its bind (glproperty notes the bind's state)

//! a texture with a handle keeps the state it froze with: true skips the sets (a different state is a conflict)
bool frozenSkip( GLuint tex, int wrapS, int wrapT, int minFilter, float aniso );
void noteKey( int wrapS, int wrapT, int minFilter, float aniso );

//! the handle of the 2D texture bound on the active unit (made resident on first use), for slot `slot`
std::uint64_t handleOfBound( NifSkopeOpenGLContext::GLFunctions * f );
void setSlot( int slot, std::uint64_t handle );
void setMatrices( const float * normal9, const float * modelView16 );
//! writes the draw's row and points the program's wwDrawIdx at it
void commit( NifSkopeOpenGLContext::GLFunctions * f, NifSkopeOpenGLContext::Program * prog );

//! the row as staged (the draw packet check compares it)
struct Row
{
	float mv[16];
	float nm[12];
	std::uint64_t tex[8];
};
static_assert( sizeof( Row ) == 176, "std430 WwDraw" );
const Row & staged();

void frame();
void forget( GLuint tex );	// a texture is deleted
void forgetAll();

//! the pooled draw of the shape now drawing: valid while `on`
struct Geo
{
	bool on = false;
	std::uintptr_t idxOff = 0;
	std::int32_t baseVertex = 0;
	unsigned int ownVao = 0;
};
extern Geo geo;
//! binds the shape's pooled geometry (uploading it when new or changed); false = draw with its own vertex array
bool poolBind( const Shape * sh, NifSkopeOpenGLContext::GLFunctions * f, unsigned int ownVao );
void poolEnd( NifSkopeOpenGLContext::GLFunctions * f );	// rebinds the shape's own vertex array (the slow red only)
void drawElements( NifSkopeOpenGLContext::GLFunctions * f, GLsizei count, std::uintptr_t off );
void multiDrawElements( NifSkopeOpenGLContext::GLFunctions * f, const GLsizei * counts, const void * const * offs, GLsizei n );
//! an indirect draw reads the shape's own buffers (its commands hold the shape's own offsets)
void unpoolForIndirect( NifSkopeOpenGLContext::GLFunctions * f );
}

#endif
