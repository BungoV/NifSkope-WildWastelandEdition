#pragma once

/* Lane VKOBJ12 (2026-11-16): the TAA inputs' step 3 (the movers' own motion, taa_objvel) on the present device, into the
 * three targets wwTaaInputsPass shares while the view is on (g.vel RG16F, g.mask RGBA8, g.depth). Plain data, no Qt: the
 * app fills one ObjVelDraw a mover and pass, in GL's order, and tries the whole list before any GL draw; a refusal draws
 * nothing on Vulkan (GL's loop runs as before). Included by vkpresent.cpp's vkobjvel.inc and by src/features/post/taainputs.cpp. */
#include <cstdint>
#include <string>
#include <vector>

namespace WwVkPresent
{
struct ObjVelDraw
{
	const float * pos = nullptr;    //!< nv x 3: attribute 0 (rigid: the shape's own; skinned: this frame's CPU skin)
	const float * prev = nullptr;   //!< nv x 3: last frame's CPU skin (velMode 1); null = pos
	const float * color = nullptr;  //!< nv x 4: attribute 1 (fxMode & 1), null = none
	const float * uv = nullptr;     //!< nv x 2: GL's attribute 7 (BaseMap's coordinates), null = none
	int nv = 0;
	const std::uint16_t * idx = nullptr;  //!< triangles, GL_UNSIGNED_SHORT
	int nIdx = 0;
	float clipJit[16] = {}, clipCur[16] = {}, clipPrev[16] = {}, modelView[16] = {};  //!< as glUniformMatrix4fv takes them
	float normalMatrix[9] = {};  //!< as glUniformMatrix3fv takes it
	int velMode = 0, alphaTest = 0, fxMode = 0, covMode = 0;
	float alpha = 1.0f, alphaThreshold = 0.0f, covThreshold = 0.5f;
	float uvScale[2] = { 1.0f, 1.0f }, uvOffset[2] = { 0.0f, 0.0f };
	unsigned baseTex = 0;  //!< GL's texture on BaseMap's unit (0: none bound, never read)
	int pass = 0;          //!< 0: vector + own-motion flag (att0 RGBA, att1 B); 1: the reactive mark only (att1 R)
};

struct ObjVelRun
{
	std::vector<ObjVelDraw> draws;
	std::uintptr_t velKey = 0, maskKey = 0, depthKey = 0;  //!< the sharedTarget keys of g.vel, g.mask, g.depth
	int w = 0, h = 0;
	/* gate reds (tests/vkframe/objveldraw.cpp; the app passes 0): 1 prev = cur on a skinned draw, 2 pass 1 with pass 0's
	 * masks (writes the vector), 3 no depth bias, 4 clipPrev written one row (16 bytes) late */
	int red = 0;
};

struct ObjVelStat
{
	int runs = 0, draws = 0, crossings = 0, gl = 0, fails = 0, pipelines = 0, twins = 0;
	double cpuMs = 0.0, maxMs = 0.0, buildMs = 0.0;
	std::string glWhy, failWhy;
};

//! empty when the route can run in this context; else why not
std::string objVelRefusal();
//! the whole list on Vulkan, one crossing (GL waits it); false: nothing drawn, why names the refusal
bool objVelRun( const ObjVelRun & r, std::string & why );
//! the app drew the pass on GL (why: the refusal)
void objVelOnGl( const char * why );
ObjVelStat takeObjVelStat();
}  // namespace WwVkPresent
