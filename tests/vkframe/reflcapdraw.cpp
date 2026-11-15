// Lane VKFAR21 (2026-11-16): the reflection probe G-buffer CAPTURE (cellreflprobe.cpp, cell_reflcap.prog, vkstatus row 113)
// on the present device. BAR FIRST: this file holds the bar and the reds, set before any route code or run, never widened.
// The harness body follows in the next commit (notes/vkfar21/next.md); until then it prints GATE FAILED (never a pass).
//
// Route under test (to be written: vkreflcapture.inc, WwVkPresent::reflCaptureRun): per job (slot, face), a render pass
// with two colour attachments = level 0, layer 6 * slot + face of the SHARED G-buffer cube arrays (sharedTarget type 3,
// 64 x 64 x 6 * 33: rpAlb RGBA8, rpNd RGBA16F; the views made per layer) and one depth attachment (a shared 64 x 64 D24
// 2D target, the twin's own, so GL can read it in the gate), all three cleared like GL's (colour 0, depth 1.0), depth test
// LEQUAL with writes, no cull, no blend, fill; one draw per caster with the reflcap_view_vs / reflcap_view_ps twins
// (vkframe_spirv.sh fxt / fxg lines), its client arrays copied into a host-visible vertex + index ring (location 0 position,
// 1 colour, 2 normal, 7 uv; a missing array reads GL's current value (0, 0, 0, 1) from a constant stream), its b31 slices
// (VS at b287: rpRelFromView, rpClipFromRel, modelViewMatrix, uvScale, uvOffset; PS at b31: rpHasBase, rpHasVc, rpAlphaTest,
// alpha, alphaThreshold) and BaseMap from the GL texture the caster binds (a twin by waterTwin / farTwin, as the relight's
// inputs), viewport 0 0 64 64 (no y flip: GL's face rows from the bottom are the image's rows from the top).
// GL's side is GL's own program (cell_reflcap.prog) with the app's state into GL-owned cube arrays of the same formats and a
// DEPTH_COMPONENT24 renderbuffer, attached per layer with glFramebufferTextureLayer, as the app does.
//
// Fixture: 4 casters, 3 jobs (slot 0 face 0, slot 0 face 3, slot 2 face 1; every other layer is cleared to a marker on both
// sides first and must keep it): A a textured quad with an alpha checker (RGBA8 BaseMap, alpha 0 / 1 in 8-texel squares,
// alphaThreshold 0.5, rpAlphaTest on), uvScale (2, 2), uvOffset (0.25, 0.125); B a vertex-coloured box (no base map, rpHasVc
// on, colours distinct per corner) partly IN FRONT of A (the depth test decides); C a mesh seen from its back (normals point
// away from the probe: the shader turns them); D a caster with normals but no uv (rpHasBase off: grey 0.5).
// Bar (per job; codes as stored):
//   covered = texels whose GL alb.a is 1; covered >= 500 of 4096 (an empty face FAILS).
//   depth: Vulkan's D24 code == GL's on EVERY texel (EXACT; read as 24-bit integers).
//   hit mask: alb.a equal on every texel (EXACT).
//   alb RGB (UNORM8): |vk - gl| <= 1 code on every covered texel; EXACT expected (printed).
//   nd (RGBA16F, normal xyz + distance w): |vk - gl| <= 2 half-float steps per channel on >= 99.9 % of covered texels and
//   relative <= 1e-3 on all of them (the 0.1 % colour bar); texels GL did not cover equal the clear on both sides.
//   untouched layers: both sides hold the marker (EXACT: proves the per-layer view).
// Reds (each must FAIL, never pass; a red that moves nothing prints TESTS NOTHING and fails the gate):
//   1 layer: Vulkan draws job 0 into layer + 1; 2 clip: rpClipFromRel's B term (row 3 z) zero on Vulkan's b287 slice;
//   4 uv: uvOffset zero on Vulkan's slice (the fixture's is not); 8 alpha: rpAlphaTest false on Vulkan's PS slice;
//   16 vc: location 1 fed (1, 1, 1, 1) on Vulkan; 32 depth: Vulkan's depth compare ALWAYS.
// Last line `REFLCAPDRAW GATE OK` or `REFLCAPDRAW GATE FAILED`.
#include <cstdio>

int main()
{
	std::printf( "reflcapdraw: bar only (VKFAR21), no harness body yet\nREFLCAPDRAW GATE FAILED\n" );
	return 1;
}
