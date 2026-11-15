#ifndef WW_VKTAAREACTIVE_H
#define WW_VKTAAREACTIVE_H

/* Lane VKFAR7 (2026-11-15): the TAA mask's reactive pass (taainputs.cpp pass 4: the see-through shapes and particle sprites
 * into the mask's R (reactive) and G (composition) by MAX, depth LEQUAL against the frame, no depth writes, GL's polygon
 * offset (-1, -4)) on the present device. The pass queues its draws (taReactiveAdd) and draws them in one crossing a
 * batch (taReactiveFlush: per-draw host slices and sets, fxWork's kind 11). Twins: taareactive_view_vs / _ps
 * (res/shaders/hlsl/post/taa_reactive_*.hlsl). Gate: tests/vkframe/taareactivedraw.sh. */

#include "features/vkframe/vkwater.h"  // WaterSlot, waterCensusOf

#include <cstdint>
#include <string>
#include <vector>

namespace WwVkPresent
{
//! one draw of pass 4, as GL's uniforms and arrays give it
struct TaReactiveDraw
{
	float clipJit[16] = {};               //!< GL's clipJit bytes (column-major, glUniformMatrix4fv transpose false)
	float uvScale[2] = { 1.0f, 1.0f }, uvOffset[2] = {};
	int useTex = 0, hasColor = 0;
	float matAlpha = 1.0f, outRG[2] = {};
	std::vector<float> pos;               //!< 3 a vertex (location 0)
	std::vector<float> col;               //!< 4 a vertex (location 1); empty: (0, 0, 0, 1), as GL's disabled array
	std::vector<float> uv;                //!< 2 a vertex (location 7); empty: 0
	std::vector<std::uint32_t> idx;       //!< triangles
	WaterSlot base;                       //!< useTex: BaseMap (unit 0) and its sampling state (waterCensusOf)
};

//! the pass's targets (GL names) and the gate's knobs
struct TaReactiveRun
{
	unsigned mask = 0;                    //!< RGBA8 one-sample colour (GL's g.mask): copied in, drawn, copied back
	unsigned depth = 0;                   //!< the frame's depth (g.depth) in dsFormat: copied in, read only
	unsigned dsFormat = 0;
	int w = 0, h = 0;
	int maxPer = 0;                       //!< draws a crossing (0: as many as the names allow); gates: 1 = singles
	int red = 0;                          //!< gates only: 1024 draw k binds draw k-1's set (its slice), 2048 draw 1 of each crossing dropped
};

/*! checks a draw and queues it (false: why; GL draws it now, MAX blending keeps the order free). The base map's
 * format / kind is refused here (waterSlotBlock), never at the flush */
bool taReactiveAdd( TaReactiveDraw && d, std::string & why );
int taReactiveQueued();
/*! GL current, the pass's state set: every queued draw into r.mask, then the queue is empty. false: why; nothing reached
 * r.mask (the caller redraws the whole pass on GL) */
bool taReactiveFlush( const TaReactiveRun & r, std::string & why );
void taReactiveDrop();                    //!< the queue emptied, nothing drawn
void taReactiveOnGl( int draws, const char * why );
unsigned taReactiveDepthTwin();           //!< the gate: GL's name of the depth twin the last flush read

struct TaReactiveStat
{
	int draws = 0, crossings = 0, gl = 0, fails = 0, flushes = 0, copies = 0, pipelines = 0;
	double cpuMs = 0.0, maxMs = 0.0, buildMs = 0.0;
	std::string glWhy, failWhy;
};
TaReactiveStat takeTaReactiveStat();
}  // namespace WwVkPresent

#ifndef WW_VKPRESENT_NO_QT
namespace WwVkPresent
{
//! the pass's route line ("vk taareactive:"), one a 5 s window while the view is on Vulkan
void taReactiveSay( bool wanted, const char * glWhy );
}
#endif

#endif
