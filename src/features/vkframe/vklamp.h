#ifndef WW_VKLAMP_H
#define WW_VKLAMP_H

/* Lane VKLAMP1 (2026-11-15, VKFAR7 item 3 slice 1): the lamps' plain shadow cubes (celllamps.cpp, the plain-cube path, the
 * live default) on the present device. The cube array is already a shared target (type 3, celllamps.cpp shadowAlloc);
 * lampRun draws its ops IN ORDER (one render pass each: a clear op clears the layer to 1, a load op keeps it), each op's
 * solid casters with the twin of cell_shadowdepth (res/shaders/hlsl/lights/lamp_view_vs / _ps), one crossing a pass
 * (the j.fn worker route, as csmRun). Alpha-tested casters stay on GL (after the crossing, no clear).
 * Gate: tests/vkframe/lampview.sh. */

#include <cstdint>
#include <string>
#include <vector>

namespace WwVkPresent
{
//! one render pass: a layer of the cube array (6 * slot + face), cleared or kept, its face block and its commands
struct LampOp
{
	int layer = 0;
	bool clear = true;
	float rel[16] = {};      //!< shRelFromView as GL uploads it (column-major)
	float clip[16] = {};     //!< shClipFromRel (lampFaceClip)
	float rn[2] = {};        //!< shRadiusNear: radius, near clip
	std::uint32_t first = 0, n = 0;  //!< draws [first, first + n)
};

/*! one caster mesh (lane VKLAMP2): lampRun keeps it on the device while (key, gen) match and copies it only on a miss.
 * key = who (the app's shape), gen = its content (the app: the hash GL's own draw takes of the same arrays) */
struct LampMesh
{
	std::uintptr_t key = 0;
	std::uint64_t gen = 0;
	const float * pos = nullptr;          //!< vec3 positions
	std::uint32_t verts = 0;
	const std::uint16_t * idx = nullptr;  //!< triangle list, mesh-local
	std::uint32_t indices = 0;
};

//! the pass: the shared cube array, the casters' meshes, their matrices, the draws and the ordered ops
struct LampRun
{
	unsigned tex = 0;               //!< GL's name of the shared cube array
	std::uintptr_t key = 0;         //!< its sharedTarget key
	int face = 0, layers = 0;
	const LampMesh * meshes = nullptr;
	std::size_t nMeshes = 0;
	const float * mats = nullptr;   //!< one model-view matrix (GL column-major) a caster, by firstInstance
	std::size_t nMats = 0;
	const std::uint32_t * draws = nullptr;  //!< {mesh, matrix} pairs, 2 words each
	std::size_t nDraws = 0;
	std::vector<LampOp> ops;        //!< first / n index the draws
};

//! GL's cube faces (the spec's sc / tc table): face's projection for a light of this radius (celllamps.cpp's numbers)
inline void lampFaceClip( int face, float radius, float clip[16] )
{
	static const float M[6][3] = { { 1, 0, 0 }, { -1, 0, 0 }, { 0, 1, 0 }, { 0, -1, 0 }, { 0, 0, 1 }, { 0, 0, -1 } };
	static const float U[6][3] = { { 0, 0, -1 }, { 0, 0, 1 }, { 1, 0, 0 }, { 1, 0, 0 }, { 1, 0, 0 }, { -1, 0, 0 } };
	static const float V[6][3] = { { 0, -1, 0 }, { 0, -1, 0 }, { 0, 0, 1 }, { 0, 0, -1 }, { 0, -1, 0 }, { 0, -1, 0 } };
	const float n = 1.0f, f = radius * 1.01f + 2.0f;
	const float A = ( f + n ) / ( f - n ), B = -2.0f * f * n / ( f - n );
	for ( int c = 0; c < 3; c++ ) {
		clip[c * 4 + 0] = U[face][c];
		clip[c * 4 + 1] = V[face][c];
		clip[c * 4 + 2] = A * M[face][c];
		clip[c * 4 + 3] = M[face][c];
	}
	clip[12] = clip[13] = clip[15] = 0.0f;
	clip[14] = B;
}

//! view -> light-relative world: rot[j * 3 + row] = the view rotation's (j, row), over the view scale; cam - light
inline void lampRelFromView( const float rot[9], float sc, const float cam[3], const float lpos[3], float rel[16] )
{
	for ( int row = 0; row < 3; row++ ) {
		for ( int j = 0; j < 3; j++ )
			rel[j * 4 + row] = rot[j * 3 + row] / sc;
		rel[12 + row] = cam[row] - lpos[row];
	}
	rel[3] = rel[7] = rel[11] = 0.0f;
	rel[15] = 1.0f;
}

std::string lampRefusal();  //!< why lampRun would refuse on this device (empty: it can run)
/*! GL current, nothing of the pass drawn yet: every op into r.tex in order, GL waits the crossing. false: why; nothing
 * reached r.tex (GL draws the whole pass) */
bool lampRun( const LampRun & r, std::string & why );
void lampOnGl( const char * why );

struct LampStat
{
	int runs = 0, gl = 0, fails = 0, ops = 0, draws = 0, pipelines = 0;
	//! the pool (VKLAMP2): meshes copied / served kept, copies for a new generation, repacks, gates-only red skips
	int copies = 0, hits = 0, regen = 0, repacks = 0, redSkips = 0;
	double copyKb = 0.0;
	double cpuMs = 0.0, maxMs = 0.0, buildMs = 0.0;
	std::string glWhy, failWhy;
};
LampStat takeLampStat();
}  // namespace WwVkPresent

#endif
