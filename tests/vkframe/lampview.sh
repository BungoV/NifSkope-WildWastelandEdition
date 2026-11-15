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
if tail -1 "$OUT/lampview.txt" | grep -q "LAMPVIEW GATE OK"; then echo "LAMPVIEW GATE OK"; else echo "LAMPVIEW GATE FAILED"; exit 1; fi
