#ifndef CELLSSGIPLAN_H
#define CELLSSGIPLAN_H

/*! lane PERFFIX2 (item 3): the pure decisions of the cell view's SSGI pass (src/features/gi/cellssgi.cpp,
 *  res/shaders/features/gi/cell_ssgi.frag), so tests/ssgiplan checks them alone (no Qt, no GL).
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

/*! 3c: the interval pyramid (stages 10/11 over the FULL-size depth, whatever the tier) is built only under red `hiz`
 *  (2048): offline its skip cost more than it saved at every tier (lane SSGI2's own live bar "nohiz >= ultra" failed
 *  the same way), and dropping it is exact: a skipped tap was one `len > radius` drops anyway. The full-mask early
 *  out stays without it (fullBreak, exact too) unless red `nohiz` (32), which keeps SSGI2's meaning (neither). */
inline bool wwSsgiHizBuilt( int red )
{
	return ( red & 2048 ) && !( red & 32 );
}

inline bool wwSsgiFullBreak( int red )
{
	return !( red & 32 );
}

/*! 3d: the four tiers (WW/CellSsgiQuality, the presets pick them by name), each a real step in cost: the work size
 *  (full / div), slices x steps per side, and the robust denoise's Poisson taps (FO4CS: 16). Twelve steps on every
 *  tier: offline 8 steps under-count the bounce (0.79 of a full-size 8 x 16 march, 12 steps 0.92). Offline
 *  1080p 5070 Ti, the synthetic room (notes/perffix2/c/RESULTS.md): 0.44 / 0.61 / 0.87 / 1.74 ms against the targets
 *  0.44 / 0.62 / 0.88 / 1.76. Red `oldtiers` (4096): FO4CS's (half 4x8, half 4x12, half 6x12, full 4x8; 16 taps). */
struct WwSsgiTier
{
	int div, slices, steps, robustTaps;
};

inline WwSsgiTier wwSsgiTier( int quality, int red )
{
	static const WwSsgiTier now[4] = { { 4, 2, 12, 8 }, { 4, 3, 12, 16 }, { 4, 6, 12, 16 }, { 2, 4, 12, 12 } };
	static const WwSsgiTier old[4] = { { 2, 4, 8, 16 }, { 2, 4, 12, 16 }, { 2, 6, 12, 16 }, { 1, 4, 8, 16 } };
	const int q = quality < 0 ? 0 : quality > 3 ? 3 : quality;
	return ( red & 4096 ) ? old[q] : now[q];
}

//! the work size of a tier over a W x H frame (rounded up)
inline int wwSsgiWorkDim( int full, int div )
{
	return div <= 1 ? full : ( full + div - 1 ) / div;
}

/*! lane INTGI1 (SSGIHO-INT, a ruling): the march radius. In an interior (not sunlit) under the traced volume there is
 *  no sky visibility for cellSsgiE's hand-over to subtract (every probe ray hits), so the hand-over is by scale: the
 *  probes already hold the occlusion and the bounce of what lies a probe spacing or more away, and the screen keeps
 *  the contact scale under it. The march stops at the volume's smallest spacing (never past `base`); the combine stays
 *  the probe-fill law, which then darkens and adds only that share. Red `nointradius` (16384): `base` everywhere. */
inline float wwSsgiRadius( float base, bool interior, float ddgiSpacing, int red )
{
	if ( ( red & 16384 ) || !interior || !( ddgiSpacing > 0.0f ) )
		return base;
	return ddgiSpacing < base ? ddgiSpacing : base;
}

#endif // CELLSSGIPLAN_H
