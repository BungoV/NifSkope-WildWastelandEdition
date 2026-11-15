#!/bin/bash
# Lane VKLAMP1 (2026-11-15, VKFAR7 item 3, slice 1): the lamps' plain shadow cubes (celllamps.cpp plain-cube path) on the
# present device (vklamp.inc lampRun, built in with WW_VKPRESENT_NO_QT: the shipped code) against GL's own program
# (lights/cell_shadowdepth.vert + .frag) drawn the way celllamps.cpp draws a cube face, into D16 cube arrays, every texel
# of every layer compared.
# bash tests/vkframe/lampview.sh <out dir> [face]. Needs MSYS2 UCRT64 g++.
# Not while NifSkope or the game runs (a window of ours): the script refuses then.
#
# BAR (stated before the first run, never widened):
# - Per slot and face: EXACT expected (the same vertex products in the same order, the same fragment depth, D16, no
#   bias); else BOUND = max 1 D16 unit and <= 0.05 % of the covered texels differ. Anything else FAILs.
# - covered = either side != 0xFFFF. A face that should hold casters with 0 covered texels FAILs (proves nothing).
#   A cone-skipped (clear-only) face must be all 0xFFFF on BOTH sides: its own verdict CLEAR, else FAIL.
# - Both targets are prefilled with junk depth before each frame, so a skipped clear shows.
# - Frame 2 moves the casters and re-runs with a held slot left untouched (it must keep frame 1's bytes, EXACT), and an
#   unrelated shared target is made between the frames (the framebuffer cache key moves; frame 2 must stay EXACT).
# Reds (each must FAIL, with its differing count printed): face order (face ^ 1's clip), slot offset (layer + 6),
# radius (radius * 1.01), clear skipped (no op clears), mover cleared (a LOAD op set to clear).
# Alpha case (lane VKLAMP2, bar stated before its first run): frame 1's casters with 4 of each slot's random casters
# alpha-tested (cell_shadowdepth shAlphaTest 1, a checker BaseMap with 0 / 1 alpha, threshold 0.5, location 7 UVs).
# Test arm = the app's route: lampRun clears and draws only the solid casters, then GL draws the alpha-tested ones on the
# same layers with NO clear, LEQUAL (cone-skipped faces get none). Reference = GL drawing every caster in order with clears.
# - Per slot and face the same bar as above (EXACT, else BOUND; CLEAR faces all 0xFFFF on both sides).
# - The alpha casters must win > 0 texels against GL's solid-only picture, and the alpha test must discard: the
#   test-off picture must differ from the reference in > 0 texels. Either at 0 FAILs (the case proves nothing).
# Alpha reds (each must FAIL): alpha clear (GL clears the layer before its alpha draw), alpha depth func (GL_ALWAYS, not
# LEQUAL), alpha test off (shAlphaTest 0 on the alpha draw).
# Pool case (lane VKLAMP2, bar stated before its first run): lampRun keeps the casters' meshes on the device keyed by
# (key, generation); a run copies only the meshes it misses. Printed per frame: `LAMPVIEW pool frame<n> copies= hits=
# regen= repacks= redSkips=`.
# - frame 1: copies 4 (the 4 meshes the casters use), hits 0. frame 2 (matrices moved only): copies 0, hits 4.
#   The reds and the alpha case copy 0 meshes.
# - frame 3 (all slots, frame 2's matrices; the plate re-specified in place, same size, new generation): copies 1,
#   regen 1, the per-face bar above.
# - frame 4 (the mover takes a new 81 x 81 plate, 6561 vertices: the positions pass the 64 KiB floor): repacks 1,
#   copies 5 (every mesh of the run again), the per-face bar above.
# - Every frame / alpha verdict line before frame 3 equals the last good run's.
# Pool reds (each its own process, must FAIL): WW_VKLAMP_POOL_RED=1 (key only, the generation not compared): a
# frame3 face FAILs and redSkips >= 1 (the trigger fired); WW_VKLAMP_POOL_RED=2 (the pool grows without copying its
# kept meshes again): a frame4 face FAILs and repacks >= 1.
# Stored case (lane VKLAMP4, bar stated before its first run; celllamps.cpp lampShadowPass, the lamp tier's route): a D16
# 2D array store (3 blocks of 3 layers; shared type 2 on the test arm, GL's own on the reference). Slot 0 = a full lamp
# (6 faces into 2 blocks), slot 3 = a wall lamp (back = face 4: face 5 whole into its block's layer 0, the 4 side faces'
# room halves packed 2 to a layer, x and y splits, with the viewport origin shifted and the scissored clear: vklamp.h's
# lampSides / lampSideHalves / lampPacked / lampWallQuad, the app's helpers). Test arm = the app's route: run A (store key)
# clears and draws the still casters into the store, GL blits store -> cube and draws the wall quads (scissored clear),
# then run B (cube key) LOADs slot 3's mover over all 6 faces, with no read between the runs. Reference = GL all of it.
# Frame S1 stores; frame S2 moves only the mover: no store run, the stored layers must keep their S1 bytes on both arms.
# - Cube faces of slots 0 and 3 and every used store layer: per layer the bar above (EXACT, else BOUND); store layers in
#   S2 EXACT against their own S1 bytes. Each packed half (member 0 and 1 of every side layer) covers > 0 texels on both
#   arms, and the mover wins > 0 texels over the stored-only picture, in S1 and S2. Any of them at 0 FAILs.
# Stored reds (each must FAIL): store origin (the shifted viewport origin dropped), whole-layer clear (the second member's
# CLEAR over the whole layer: it wipes the first member), mover cleared (run B's LOAD ops set to clear).
# Every verdict line before the stored case equals the last good run's (X:/ClaudeOverflow/vklamp3/lampview.log).
# Fence red: the exe re-run with WW_VKFENCE_RED=lamp must end `LAMPVIEW GATE FAILED`.
# Last line: `LAMPVIEW GATE OK` or `LAMPVIEW GATE FAILED`.
set -u
OUT="${1:?out dir}"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
export PATH="/c/msys64/ucrt64/bin:$PATH"
if tasklist | grep -qiE '^(NifSkope|Fallout4)'; then echo "LAMPVIEW GATE FAILED (NifSkope or the game runs: no window of ours now)"; exit 1; fi
mkdir -p "$OUT"
g++ -O2 -std=c++17 -pthread -I"$ROOT/src" "$ROOT/tests/vkframe/lampview.cpp" -o "$OUT/lampview.exe" -lopengl32 -lgdi32 -luser32 \
	|| { echo "LAMPVIEW GATE FAILED (g++)"; exit 1; }
"$OUT/lampview.exe" "$ROOT" "${2:-256}" 2>&1 | tee "$OUT/lampview.txt"
WW_VKFENCE_RED=lamp "$OUT/lampview.exe" "$ROOT" "${2:-256}" > "$OUT/lampview_fencered.txt" 2>&1
if tail -1 "$OUT/lampview_fencered.txt" | grep -q "LAMPVIEW GATE FAILED"; then echo "LAMPVIEW RED fence FAIL (as it must)"
else echo "LAMPVIEW RED fence TESTS NOTHING"; echo "LAMPVIEW GATE FAILED"; exit 1; fi
for R in 1 2; do
	F=$(( R + 2 )); T=$([ "$R" = 1 ] && echo redSkips || echo repacks)
	WW_VKLAMP_POOL_RED=$R "$OUT/lampview.exe" "$ROOT" "${2:-256}" > "$OUT/lampview_poolred$R.txt" 2>&1
	# only faces FAILing past 1 unit count (a 1-unit BOUND breach would show in the good run too)
	nf=$(grep -cE "^LAMPVIEW frame$F slot[0-9]+/face[0-9] FAIL .* [1-9][0-9]* past 1 unit" "$OUT/lampview_poolred$R.txt")
	tr=$(grep -E "^LAMPVIEW pool frame$F " "$OUT/lampview_poolred$R.txt" | grep -oE "$T=[0-9]+" | cut -d= -f2)
	if [ "$nf" -gt 0 ] && [ "${tr:-0}" -ge 1 ]; then echo "LAMPVIEW RED pool$R FAIL (as it must: $nf frame$F faces FAIL past 1 unit, $T=$tr)"
	else echo "LAMPVIEW RED pool$R TESTS NOTHING ($nf frame$F faces FAIL, $T=${tr:-none})"; echo "LAMPVIEW GATE FAILED"; exit 1; fi
done
if tail -1 "$OUT/lampview.txt" | grep -q "LAMPVIEW GATE OK"; then echo "LAMPVIEW GATE OK"; else echo "LAMPVIEW GATE FAILED"; exit 1; fi
