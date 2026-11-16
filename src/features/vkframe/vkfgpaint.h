#ifndef WW_VKFGPAINT_H
#define WW_VKFGPAINT_H

/* Lane VKGROUND6 (2026-11-16): the far ground page paint (cellfarground.cpp paintJobs: cell_fgpage, then its BC1 encode
 * cell_fgbc1 a level) on the present device. GL draws the page into an RGBA8 scratch (side x side, mips by
 * glGenerateMipmap), encodes each level into an RG32UI block target and uploads the blocks into the pool's layer; here, in one
 * crossing: the page twin into the route's own SHARED RGBA8 scratch, its mips by a linear blit, per level the encode twin
 * into the route's SHARED RG32UI blocks, then a copy of the blocks into the pool's far twin (BC1, the layer, the level).
 * fgGrid / fgLtex / fgAtlas are read in place when shared, fgHeight is copied (GL-owned), fgDecal through a texel buffer.
 * The route (vkfgpaint.inc) is the next step; this header is the gate's contract (tests/vkframe/fgpaintview.cpp). */

#include <cstdint>
#include <string>

#include "features/vkframe/vkwater.h"
#include "features/vkframe/vkfgpaint_tables.h"

namespace WwVkPresent
{
/*! one page: GL's cell_fgpage (in use, its uniforms set, its inputs on their units), its pixel stage's loader defines
 * (Program::wwDefs, "" is the only key), the page's side (a pool side: 256 / 128 ... ), the pool's far twin key and the
 * layer sched.commit gave; encodeOnly (gate arm E): no page draw, the route's scratch (fgPaintPageTex) as the gate filled it
 * is encoded; red (route reds, the gate's): 1 the blocks into layer + 1 (wrong page), 2 the blocks never copied into the
 * pool (a stale page), 4 the encode skipped (the blocks cleared to 0 and copied) */
struct FgPaintRun
{
	unsigned prog = 0;
	std::string wwDefs;
	int side = 0, layer = 0;
	std::uintptr_t poolKey = 0;
	bool encodeOnly = false;
	int red = 0;
};

struct FgPaintStat
{
	int runs = 0, copies = 0, fails = 0, gl = 0, pipelines = 0;
	double cpuMs = 0.0, maxMs = 0.0, buildMs = 0.0;
	std::string failWhy, glWhy;
};

//! the run on the present device, GL's state already read into s (vkfgpaint.inc)
bool fgPaintRunState( const FgPaintRun & r, WaterRun & s, std::string & why );
//! "" when the device can take a run now
std::string fgPaintRefusal();
void fgPaintOnGl( const char * why );
FgPaintStat takeFgPaintStat();
//! the route's WaterRun (big: the loose blocks), in its own state; GL's thread only
WaterRun & fgPaintScratch();
//! the GL name of the route's shared RGBA8 scratch page of that side (every level), 0 before its first run
unsigned fgPaintPageTex( int side );

//! GL current, r.prog in use with its uniforms and inputs bound: GL's side read, then the run
template <class GL> bool fgPaintRun( const FgPaintRun & r, GL & fn, std::string & why )
{
	WaterRun & s = fgPaintScratch();
	s = WaterRun();
	twinStateOf( &fn, r.prog, s, kT_fgpagePs, int( sizeof( kT_fgpagePs ) / sizeof( kT_fgpagePs[0] ) ), kT_fgpagePsBytes );
	return fgPaintRunState( r, s, why );
}
}  // namespace WwVkPresent

#endif
