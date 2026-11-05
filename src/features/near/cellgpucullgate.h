#ifndef CELLGPUCULLGATE_H
#define CELLGPUCULLGATE_H

/* lane SIGNDROP1 (2026-10-29): which draws of the last frame may stand in the GPU cull's depth prepass
 * (gl/cellgpucull.h, step B). Plain C++ with no Qt or GL, so tests/signdrop1 compiles the same rule.
 *
 * The prepass draws its occluders depth-only (lookdev_csmdepth_q: no texture, no discard), so every covered sample
 * gets the occluder's depth. A draw whose fragment shader may discard has holes the frame shows and the prepass
 * fills: what is seen through them lies "behind" the prepass and the Hi-Z test culls it. The queue's AlphaTest
 * class reads the NIF's NiAlphaProperty only, but for an FO4 lighting shape with a BGSM the material's alpha test
 * wins in setupProgram (renderer.cpp), so a broken wall cut by its BGSM queued Opaque and hid the SALE sign, the
 * shelves and the door behind its hole (Concord c3, notes/triage1029). The gate reads the alphaFlags the draw ran
 * with instead. */

namespace GpuDrivenBGate
{

//! fo4_default.frag (every solid-list program includes it) runs its alpha test, and the decal angle drop, only
//! when alphaFlags is not 0 (bit 3 = blended, bits 0-2 = the test function).
inline bool alphaDiscards( int alphaFlags )
{
	return alphaFlags != 0;
}

//! what the last frame's draw of a shape ran with
struct Seen
{
	bool alphaTestQueue = false;	// queued as AlphaTest (its NiAlphaProperty tests)
	bool occluderProgram = false;	// a program whose depth the prepass reproduces (cellgpucull.cpp vertexProgram)
	bool opaque = false;	// depth write, LEQUAL, offset (1, 2), no blend, coverage or stencil test
	bool cullOn = false, cullBack = false, ccw = true;
	bool discards = false;	// alphaDiscards() of the alphaFlags the draw ran with
};

//! red WW_GPUDRIVEN_B_RED=atocc: the gate before SIGNDROP1, which took the alpha test from the queue only
inline bool occluder( const Seen & s, bool redAtOcc )
{
	return !s.alphaTestQueue && s.occluderProgram && s.opaque && ( !s.cullOn || ( s.cullBack && s.ccw ) )
		&& ( redAtOcc || !s.discards );
}

}

#endif
