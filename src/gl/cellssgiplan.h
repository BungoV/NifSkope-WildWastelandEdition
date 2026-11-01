#ifndef CELLSSGIPLAN_H
#define CELLSSGIPLAN_H

/*! lane PERFFIX2 (item 3): the pure decisions of the cell view's SSGI pass (src/gl/cellssgi.cpp,
 *  res/shaders/cell_ssgi.frag), so tests/ssgiplan checks them alone (no Qt, no GL).
 *  3a: every stage of the pass belongs to one GPU sub-timer group ("| sub ssgi-<name>" in the pass time line). */

//! The sub-timer groups, in PassTime::Sub order from PassTime::SsgiHiz on (cellpasstime.h).
enum WwSsgiSub
{
	WwSsgiSubHiz = 0,		// stages 10, 11: the interval pyramid
	WwSsgiSubMips = 1,		// the donor's mips (glGenerateMipmap, no stage)
	WwSsgiSubEcast = 2,		// stage 8: the emissive cast
	WwSsgiSubEchain = 3,	// the emissive cast's own temporal + denoise (stages 2-7 with emissivePass)
	WwSsgiSubCast = 4,		// stage 1: the cast
	WwSsgiSubTemporal = 5,	// stage 2
	WwSsgiSubDenoise = 6,	// stages 3-7 (SSGI1's 5x5, or the robust denoise + quarter pyramid)
	WwSsgiSubCount = 7
};

//! The group a stage's pass is timed in; `emissive` = the pass belongs to the emissive cast's chain. -1: no stage.
inline int wwSsgiStageSub( int stage, bool emissive )
{
	if ( stage == 10 || stage == 11 )
		return WwSsgiSubHiz;
	if ( stage == 8 )
		return WwSsgiSubEcast;
	if ( stage == 1 )
		return WwSsgiSubCast;
	if ( stage >= 2 && stage <= 7 )
		return emissive ? WwSsgiSubEchain : stage == 2 ? WwSsgiSubTemporal : WwSsgiSubDenoise;
	return -1;
}

inline const char * wwSsgiSubName( int sub )
{
	static const char * const n[WwSsgiSubCount] = { "ssgi-hiz", "ssgi-mips", "ssgi-ecast", "ssgi-echain", "ssgi-cast",
		"ssgi-temporal", "ssgi-denoise" };
	return sub >= 0 && sub < WwSsgiSubCount ? n[sub] : "";
}

/*! 3b: how the emitters reach the bounce. The reds are cellssgi.cpp's bits: 128 `noemissive`, 1024 `emitchain`.
 *  Folded (the default): the ordinary march gathers them from its donor (the scene colour holds the emission), at
 *  SSGI3's fill law like any other radiance: one cast, one temporal, one denoise, and the scene pass writes no
 *  emission target. Chain (red `emitchain`, lane SSGI2's FO4CS row 16): stage 8's importance-sampled cast with its
 *  own chain, the ordinary donor less the emission. None: no emission target (`noemissive`; draws as the fold). */
enum WwSsgiEmissive { WwSsgiEmitFolded = 0, WwSsgiEmitChain = 1 };

inline int wwSsgiEmissive( int red )
{
	return ( red & 1024 ) && !( red & 128 ) ? WwSsgiEmitChain : WwSsgiEmitFolded;
}

//! whether the scene pass writes the emission target (only the chain reads it)
inline bool wwSsgiWantsEmissionTarget( int red )
{
	return wwSsgiEmissive( red ) == WwSsgiEmitChain;
}

#endif // CELLSSGIPLAN_H
