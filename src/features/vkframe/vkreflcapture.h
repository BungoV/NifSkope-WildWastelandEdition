#ifndef WW_VKREFLCAPTURE_H
#define WW_VKREFLCAPTURE_H

/* Lane VKFAR22 (2026-11-16): the reflection probes' G-buffer capture (cellreflprobe.cpp, GL's cell_reflcap program) on the
 * present device. GL draws, per job (probe slot, cube face), every caster into layer 6 * slot + face of the G-buffer cube
 * arrays (rpAlb RGBA8, rpNd RGBA16F) with a 64 x 64 D24 depth, all cleared first (colour 0, depth 1), depth LEQUAL with
 * writes, no cull, no blend. Here: one render pass a job over 2D views of those layers of the SHARED cube arrays and the
 * route's own shared D24 target, one draw a caster (its client arrays copied into a host ring, its b287 / b31 slices as GL's
 * program holds them when it would draw, BaseMap read in place or through a twin), every job of a run in one crossing.
 * Twins: reflcap_view_vs / reflcap_view_ps (vkframe_spirv.sh fxt / fxg). Gate: tests/vkframe/reflcapdraw.sh. */

#include <cstdint>
#include <string>
#include <vector>

#include "features/vkframe/vkwater.h"
#include "features/vkframe/vkreflcapture_tables.h"

namespace WwVkPresent
{
//! one caster's draw: its client arrays (position 3, colour 4, normal 3, uv 2 floats a vertex; null: absent, GL's current
//! value is read), its triangles, its blocks as GL's program holds them, BaseMap as GL binds it (tex 0: none)
struct ReflCaptureDraw
{
	const float * pos = nullptr;
	const float * col = nullptr;
	const float * nrm = nullptr;
	const float * uv = nullptr;
	int verts = 0;
	const std::uint16_t * idx = nullptr;
	int indices = 0;
	float cur[3][4] = { { 0, 0, 0, 1 }, { 0, 0, 0, 1 }, { 0, 0, 0, 1 } };  //!< GL's current values of locations 1, 2, 7
	std::uint8_t vs[kT_reflcapVsBytes] = {};
	std::uint8_t ps[kT_reflcapPsBytes] = {};
	WaterSlot base;
};

//! one job: the cube arrays' layer (6 * slot + face) and its draws [first, first + count) in the run's list
struct ReflCaptureJob
{
	int layer = 0;
	int first = 0, count = 0;
};

/*! the pass: GL's program (its pixel stage's loader defines: the twin is the define-free build), the shared G-buffer cube
 * arrays' keys, the jobs in GL's order. red (gates only): 1 job 0 into layer + 1, 2 rpClipFromRel's B (column 3, z) zero, 4 uvOffset
 * zero, 8 rpAlphaTest false, 16 location 1 fed (1, 1, 1, 1), 32 depth compare ALWAYS */
struct ReflCaptureRun
{
	unsigned prog = 0;
	std::string wwDefs;
	std::uintptr_t albKey = 0, ndKey = 0;
	std::vector<ReflCaptureJob> jobs;
	std::vector<ReflCaptureDraw> draws;
	int red = 0;
};

struct ReflCaptureStat
{
	int runs = 0, faces = 0, draws = 0, crossings = 0, copies = 0, inPlace = 0, fails = 0, gl = 0, pipelines = 0;
	double cpuMs = 0.0, maxMs = 0.0, buildMs = 0.0;
	std::string failWhy, glWhy;
};

//! the pixel stage's loader defines the twins are built for (Program::wwDefs): none
inline const char * reflCaptureKey()
{
	return "";
}

//! the run on the present device, every draw's GL state already read (reflCaptureDrawOf; vkreflcapture.inc)
bool reflCaptureRun( const ReflCaptureRun & r, std::string & why );
//! "" when the device can take a run now
std::string reflCaptureRefusal();
void reflCaptureOnGl( const char * why );
ReflCaptureStat takeReflCaptureStat();
//! the route's shared 64 x 64 D24 depth (GL's name; 0 before the first run): the gate reads it
unsigned reflCaptureDepthTex();

//! GL current, prog in use with this caster's uniforms set, BaseMap bound: the blocks, BaseMap and the current values into d
template <class GL> void reflCaptureDrawOf( GL & fn, unsigned prog, ReflCaptureDraw & d )
{
	fxPack( &fn, prog, kT_reflcapVs, int( sizeof( kT_reflcapVs ) / sizeof( kT_reflcapVs[0] ) ), d.vs );
	fxPack( &fn, prog, kT_reflcapPs, int( sizeof( kT_reflcapPs ) / sizeof( kT_reflcapPs[0] ) ), d.ps );
	WaterCensus c;
	waterCensusOf( &fn, prog, c );
	d.base = WaterSlot();
	for ( const WaterSlot & s : c.inputs )
		if ( s.name == "BaseMap" )
			d.base = s;
	static const unsigned kLoc[3] = { 1, 2, 7 };
	for ( int i = 0; i < 3; i++ )
		fn.glGetVertexAttribfv( kLoc[i], 0x8626, d.cur[i] );  // GL_CURRENT_VERTEX_ATTRIB
}
}  // namespace WwVkPresent

#endif
