#!/bin/bash
# ---------------------------------------------------------------------------
# impostor_aa.sh -- the card bake's ANTI-ALIASING gate.
#
# Lane IMPOSTORAA1, 2026-09-23. bungo, 2026-09-22: "Also, are the bakes for
# these anti aliased?" and then "Preferable solution would be a 2x render then
# downscale to achieve AA". The ruled design: every octahedral frame renders
# OFFSCREEN at exactly 2x the frame's inner size, independent of the window and
# of the user's MSAA setting, and is reduced 2:1 by a 2x2 box (coverage = the
# mean of the four samples; colour and data coverage-weighted).
#
# RAISED 2x -> 4x by lane IMPOSTORTEAR1, 2026-09-23 (director, on the
# recommendation bungo was given): the 2x design reproduces its own 2x2
# estimator exactly and that estimator reads 1.27 texels on
# lodgen_octahedral F1 against a bar of 1.0; 4x4 reads 0.68. The bake is now a
# 4x4 box (WW_IMPOSTOR_AA=K picks K, 2 = IMPOSTORAA1's bake, 0 = the window);
# the sidecar's `aa K ...` line names K and impostor_cube_coverage.py builds
# the ideal KxK estimator from it, so FACT 1 is the same fact at the new K.
# A row below asserts the default bake says `aa 4`.
#
# TWO FACTS, each with a floor that must fire in the same run.
#
# FACT 1 -- THE KNOWN ANSWER. A 512-unit cube photographed orthographically has
#   a convex silhouette, so every frame texel's true coverage is an AREA
#   computable without the application (tests/spells/impostor_cube_coverage.py,
#   which shares no line with the bake). A correct 2x bake reproduces the IDEAL
#   2x2 ESTIMATOR -- the same polygon point-sampled at the four sample centres --
#   and that estimator's own error against the area truth is the design's
#   ceiling (ww-downsample-gate section 2).
#     bar:   bake vs the ideal 2x2 estimator, mean <= 4 levels
#            edge error vs the area truth <= the ceiling x 1.10
#     floor: the same exe with WW_IMPOSTOR_AA=0 (the old window-grab path,
#            byte-identical to the rung at 560x560) must read > 20 levels
#            against the estimator.
#   MEASURED 2026-09-23 on release/NifSkope.exe 00:21:11 (sha1 74e317f9):
#     new arm   1.71 levels vs the estimator, edge error 41.9 (ceiling 42.5)
#     rung      71.1 levels vs the estimator, edge error 66.0 (89574e81)
#
# FACT 2 -- WINDOW-SIZE INDEPENDENCE. The same cube baked in a 560x560 window
#   and in a 1400x1000 window (WW_IMPOSTOR_WINDOW, read by the bake) must give
#   BYTE-IDENTICAL octahedral sheets (albedo, normal, gsaos, g).
#     floor: the fallback WW_IMPOSTOR_AA=0 at the same two sizes must DIFFER --
#            proof that the knob reaches the window and that a window-grab bake
#            is window-dependent. On the rung the knob does not exist, the floor
#            pair comes out identical, and the row fails: the rung cannot even be
#            asked the question, which is the defect.
#   The legacy front/side cards (<id>_front.png, _side.png, and the `front` /
#   `side` sidecar lines) are NOT part of this lane and are still window-based;
#   they are excluded by name.
#
# Usage:   bash tests/spells/impostor_aa.sh            (release/NifSkope.exe)
#          EXE=release/NifSkope.before_impostoraa1.exe bash tests/spells/impostor_aa.sh
# ---------------------------------------------------------------------------
set -u
. "$(dirname "$0")/_harness.sh"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
NS="${EXE:-$ROOT/release/NifSkope.exe}"
case "$NS" in /*) ;; *) NS="$ROOT/$NS" ;; esac
W="$ROOT/release/impostor_aa_tmp"
LOG="$ROOT/release/ww_impostor_aa.log"
rm -rf "$W"; mkdir -p "$W"; : > "$LOG"
checks=0; fails=0
say() { echo "$*" | tee -a "$LOG"; }
ok()  { checks=$((checks+1)); say "  ok    $*"; }
bad() { checks=$((checks+1)); fails=$((fails+1)); say "  FAIL  $*"; }

say "impostor_aa gate $(date '+%Y-%m-%d %H:%M:%S')"
say "exe $NS  $(ls -l --time-style=+%Y-%m-%d_%H:%M:%S "$NS" | awk '{print $5, $6}')  sha1 $(sha1sum "$NS" | cut -c1-8)"

# python WITH numpy + PIL (MSYS2's has neither; Git-Bash's user install does)
PY=""
for c in /c/Users/bungo/AppData/Local/Programs/Python/Python39/python python python3; do
	if command -v "$c" >/dev/null 2>&1 && "$c" -c "import numpy, PIL" >/dev/null 2>&1; then PY="$c"; break; fi
done
[ -n "$PY" ] || { say "FAIL: no python with numpy and PIL"; exit 1; }

tasklist | grep -qi "Fallout4" && { say "REFUSED: Fallout4.exe is up"; exit 1; }

CUBE="$W/cube512.nif"
"$NS" -no-gui new -o "$(winpath "$CUBE")" --cube --size 512 >/dev/null 2>&1
[ -s "$CUBE" ] && ok "the 512-unit cube fixture was written" || { bad "the cube fixture was not written"; exit 1; }

bake() { # bake <dir> <port> [ENV...]
	local n="$1" o="$W/$1" p="$2"; shift 2
	mkdir -p "$o"
	env "$@" WW_IMPOSTOR_BAKE="$(winpath "$o")" WW_IMPOSTOR_OCT=4 WW_IMPOSTOR_TILE=512 \
		timeout 600 "$NS" "$(winpath "$CUBE")" --port "$p" >/dev/null 2>&1
	if [ -s "$o/cube512_oct_albedo.png" ]; then
		say "  baked $n: $(grep -E '^aa ' "$o/cube512.txt" || echo 'no aa line')"
	else
		say "  baked $n: NO SHEET"
	fi
}
bake new560  45961 WW_IMPOSTOR_WINDOW=560x560
bake new1400 45962 WW_IMPOSTOR_WINDOW=1400x1000
bake old560  45963 WW_IMPOSTOR_WINDOW=560x560  WW_IMPOSTOR_AA=0
bake old1400 45964 WW_IMPOSTOR_WINDOW=1400x1000 WW_IMPOSTOR_AA=0

say "FACT 1: the known-answer cube"
K_NEW="$(grep -E '^aa ' "$W/new560/cube512.txt" 2>/dev/null | awk '{print $2}')"
[ "$K_NEW" = "4" ] 	&& ok "the default bake is the 4x4 offscreen arm (sidecar: aa $K_NEW)" 	|| bad "the default bake is not the 4x4 offscreen arm (sidecar aa factor: ${K_NEW:-none})"
score() { "$PY" "$ROOT/tests/spells/impostor_cube_coverage.py" "$W/$1/cube512.txt" "$W/$1/cube512_oct_albedo.png" 256 256 2>&1 | tail -4; }
S_NEW="$(score new560)"; S_OLD="$(score old560)"
say "$(echo "$S_NEW" | sed 's/^/    new: /')"
say "$(echo "$S_OLD" | sed 's/^/    AA=0: /')"
num() { echo "$1" | grep "$2" | sed -E 's/.*mean ([0-9.]+).*/\1/'; }
V_NEW="$(num "$S_NEW" 'bake vs the ideal')"; E_NEW="$(num "$S_NEW" 'edge coverage error')"
C_NEW="$(num "$S_NEW" 'estimator vs truth')"; V_OLD="$(num "$S_OLD" 'bake vs the ideal')"
awk -v v="${V_NEW:-999}" 'BEGIN{exit !(v <= 4.0)}' \
	&& ok "the bake reproduces the ideal KxK estimator on the cube's edges ($V_NEW levels, bar 4)" \
	|| bad "the bake does not reproduce the ideal KxK estimator on the cube's edges (${V_NEW:-none} levels, bar 4)"
awk -v e="${E_NEW:-999}" -v c="${C_NEW:-0}" 'BEGIN{exit !(c > 0 && e <= 1.10 * c)}' \
	&& ok "the edge coverage error sits at the design ceiling ($E_NEW levels against a ceiling of $C_NEW)" \
	|| bad "the edge coverage error is above the design ceiling (${E_NEW:-none} levels against a ceiling of ${C_NEW:-none}, bar x1.10)"
awk -v v="${V_OLD:-0}" 'BEGIN{exit !(v > 20.0)}' \
	&& ok "FLOOR: the window-grab fallback is refused by the same bar ($V_OLD levels against the estimator)" \
	|| bad "FLOOR: the window-grab fallback is NOT refused by the bar (${V_OLD:-none} levels) -- the row cannot fail"

say "FACT 2: window-size independence"
SHEETS="cube512_oct_albedo.png cube512_oct_normal.png cube512_oct_gsaos.png cube512_oct_g.png"
same() { local d=0 s; for s in $SHEETS; do [ -s "$W/$1/$s" ] && [ -s "$W/$2/$s" ] || { d=$((d+100)); continue; }; cmp -s "$W/$1/$s" "$W/$2/$s" || d=$((d+1)); done; echo $d; }
D_NEW="$(same new560 new1400)"; D_OLD="$(same old560 old1400)"
[ "$D_NEW" = "0" ] \
	&& ok "the four octahedral sheets are byte-identical baked in a 560x560 and a 1400x1000 window" \
	|| bad "the octahedral sheets depend on the window size ($D_NEW of 4 differ between 560x560 and 1400x1000; 100+ = a sheet missing)"
[ "$D_OLD" -ge 1 ] 2>/dev/null && [ "$D_OLD" -lt 100 ] \
	&& ok "FLOOR: the window-grab fallback's sheets DO differ between the two windows ($D_OLD of 4), so the knob reaches the window" \
	|| bad "FLOOR: the window-grab fallback's sheets do not differ between the two windows ($D_OLD) -- the window knob is not honoured, the row cannot fail"

say "$checks checks, $fails failures"
[ "$fails" = "0" ] && { say "PASS"; exit 0; } || { say "FAIL"; exit 1; }
