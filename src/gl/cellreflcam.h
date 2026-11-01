#pragma once
/* lane PERFFIX2 a (item 4): when the reflection probes' camera probe (gl/cellreflprobe.cpp, slot 0) is drawn and relit.
 * Pure C++ (tests/reflcam).
 *
 * REFLPROBE1 drew one face of the camera probe's G-buffer cube every frame forever (faces cycled) and relit the probe
 * after each, although the cube is light-free and drawn from the statics alone: with nothing changed every frame drew
 * the same face again and relit the same light (Concord 5x5: 9.6-10.4 ms CPU, 0.56-0.67 ms GPU, steady state).
 * Here each face remembers the key it was drawn under (casters, their places, the document's version, textures, the
 * probe's snapped place and box) and only a face whose key moved is drawn again, in the old cycle's order; the probe
 * is relit when a face came in or what the relight reads moved (the placed probes' light hash, plus its own key). The
 * red (perFrame: WW_CELL_REFLPROBE_RED=perframe) is the old schedule exactly. */
#include <cstdint>

namespace WwReflCam
{
//! the camera probe's face to draw this frame (-1: none). faces = faces drawn since the probe was seated (6: whole),
//! camFace the cycle's next face, faceKey what each face was drawn under, key what a face would be drawn under now.
inline int nextFace( int faces, int camFace, const std::uint64_t faceKey[6], std::uint64_t key, bool perFrame )
{
	if ( perFrame || faces < 6 )
		return faces < 6 ? faces : camFace;
	for ( int i = 0; i < 6; i++ )
		if ( faceKey[( camFace + i ) % 6] != key )
			return ( camFace + i ) % 6;
	return -1;
}

//! relight the camera probe this frame: whole, and not lit since its last face, or its light moved (h: the placed
//! probes' hash; hc: h plus what only the camera probe's relight keys on)
inline bool relight( int faces, bool lit, std::uint64_t litHash, std::uint64_t h, std::uint64_t litKey, std::uint64_t hc,
	bool perFrame )
{
	return faces >= 6 && ( !lit || litHash != h || ( !perFrame && litKey != hc ) );
}
}	// namespace WwReflCam
