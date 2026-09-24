#!/bin/bash
#
# WW_WATER_FLOW -- the potential-flow solve inside each water body, and the
# dye that rides on it (lane WATER4).
#
# bungo, 2026-09-10, on the harmonic fill's picture: "That stroke doesn't look
# smooth at all, it's like overlapping circles more like." -- and "Could this
# maybe use a bit of some simulation though?"
#
# The gates were PRE-REGISTERED in scratchpad/lane_water4_report.md section 0
# before any code existed.  They run inside `lodl <copy> --water-mark-selftest`
# (the marking tool's own harness, which now carries them) and this script
# reads them back BY NAME so that a missing gate is a failure, not a pass:
#
#   F1  a channel that narrows to half its width doubles its speed (2.00 +- 5%),
#       and the flux through 10 cross-sections is constant within 3%;
#   F2  the flow parts round an island and rejoins (halves within 2%), mass is
#       balanced at every wet cell to 1e-6, the straight banks are tangent to
#       within 1 degree, and the island's bank direction is compared with the
#       analytic cylinder (mean < 5, max < 15 degrees);
#   F3  a lake with no outlet and no mark: speed exactly 0;
#   F4  a lake with one outlet: nothing points away from it and every
#       streamline reaches it;
#   F5  the Charles after ONE stroke: 0 seam-bounded constant-direction
#       patches (the disc fill had 39), p99 of the adjacent angle difference
#       < 5 degrees (was 40.78), seams < 0.5% (was 2.97%), mean still toward
#       the mouth (cos > 0.9);
#   F6  a synthetic river into a synthetic sea: the plume's 1/8 length and
#       direction predicted from the flow BEFORE the dye, measured after,
#       within 10% / 9 degrees;
#   F7  a dye pin: 1/2 one half-distance downstream, 1/8 at three, 0 upstream;
#   F8  the Charles solves in under 1.0 s with a residual below 1e-8;
#   and the dye plane round trip: written only with a dye mark, read back by
#   the .lodl reader, gone again when the mark is removed (section 7's undo
#   gate, byte for byte).
#
# Then the INDEPENDENT decoder confirms F5 out of the saved file:
# scratchpad/water4_20260910/disc_metric.py (WATER2's lodl_v3_authority.py
# underneath, no writer code) must count 0 patches on the marked body.
#
# USAGE
#   bash tests/spells/water_flow.sh
#   LODL=/path/Commonwealth.lodl bash tests/spells/water_flow.sh
#
# The fixture is the version-3 file lane WATER2 wrote (.gitignore'd, regenerates
# in six seconds with `lodgen ... --water-bodies`).  WW_WATER_MARK_BODY=3 pins
# the Charles so the F5 numbers are the ones the lane report quotes.

set -u

. "$(dirname "$0")/_harness.sh"

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
NS="${EXE:-$ROOT/release/NifSkope.exe}"
LODL="${LODL:-$ROOT/scratchpad/water2_20260909/out/Terrain/Commonwealth.lodl}"
WORK="${WORK:-$ROOT/scratchpad/water4_20260910/work}"
BODY="${BODY:-3}"

[ -x "$NS" ] || { echo "no NifSkope.exe at $NS"; exit 2; }
[ -f "$LODL" ] || { echo "no version-3 .lodl at $LODL (write one with --water-bodies)"; exit 2; }

mkdir -p "$WORK"
rm -f "$WORK"/flow.lodl "$WORK"/flow.lodl.bak-watermark "$WORK"/flow.lodl.tmp-watermark

fails=0
cp "$LODL" "$WORK/flow.lodl" || { echo "FAIL: could not copy the fixture"; exit 1; }
echo "== the flow gates, inside the marking tool's harness, on a copy of $(basename "$LODL") =="
WW_WATER_MARK_BODY="$BODY" "$NS" -no-gui lodl "$WORK/flow.lodl" --water-mark-selftest > "$WORK/flow.txt" 2>&1
rc=$?
cat "$WORK/flow.txt"
[ "$rc" -eq 0 ] || { echo "FAIL: the selftest exited $rc"; fails=$((fails+1)); }

# every registered gate must be PRESENT and green -- a gate that did not run is red
for g in "F1 continuity" "F1 flux" "F2 the flow parts" "F2 mass balance" "F2 straight-bank" \
	"F2 island bank" "F3 a lake" "F4 a lake with one outlet: 0 texels" "F4 every streamline" \
	"F5 no seam-bounded" "F5 the 99th" "F5 seams" "F5 the mean direction" \
	"F6 the plume's 1/8" "F6 the plume's direction" \
	"F7 a dye pin" "F7 upstream" "F7 three half" "F8 the solve" \
	"the river's dye reaches" "near the mouth the weight" "beyond three half-distances" \
	"every dyed texel names the river" "the dyed file re-opens" "the dye words read back" \
	"P8 round trip" "P3 undo"; do
	# lane WATER7, red 2 of BUILD10. The harness prints an INFORMATIONAL line
	# carrying the gate's name above its own verdict line, so `head -1` took
	# the informational one and called a green gate red ("F8 the solve").
	# Only a line that IS a verdict may be considered; water_weights.sh was
	# written this way from the start.
	line="$(grep -a -F "$g" "$WORK/flow.txt" | grep -aE '^  (ok|FAIL) ' | head -1)"
	if [ -z "$line" ]; then
		echo "FAIL: gate '$g' did not run"; fails=$((fails+1))
	elif ! printf '%s' "$line" | grep -aq '^  ok '; then
		echo "FAIL: gate '$g' is red"; fails=$((fails+1))
	fi
done
NF="$(grep -a -c '^  ok   F[1-8]' "$WORK/flow.txt")"
# lane WATER7, red 3 of BUILD10: a floor and a prediction in one document
# contradicted each other. 19 F-gates are registered in the loop above; TWO of
# them are pre-registered as red and are expected to stay red until their own
# causes are answered (F2's island bank, mean 12.01 / max 22.40 deg against
# 5 / 15; F5's p99, 8.44 against 5). 19 - 2 = 17, so 17 is the highest floor
# this spell can carry while both predictions stand, and it is one BELOW a
# green run's count so losing one more gate still goes red.
echo "flow gates green: $NF (floor 17 = 19 registered F-gates - 2 pre-registered red)"
[ "${NF:-0}" -ge 17 ] || { echo "FAIL: only $NF flow gates ran green, floor is 17"; fails=$((fails+1)); }

# the independent decoder, on the MARKED file the selftest left aside
echo
echo "== the independent decoder on the marked file =="
if [ -f "$WORK/flow.lodl.bak-watermark" ]; then
	cp "$WORK/flow.lodl.bak-watermark" "$WORK/charles_marked_v4.lodl"
	python "$ROOT/scratchpad/water4_20260910/disc_metric.py" "$BODY" "$WORK/charles_marked_v4.lodl" \
		| tee "$WORK/disc_v4.txt"
	grep -aq 'patches (>= 64 texels): 0$' "$WORK/disc_v4.txt" \
		|| { echo "FAIL: the independent decoder still counts seam-bounded patches"; fails=$((fails+1)); }
	grep -aq 'distinct directions' "$WORK/disc_v4.txt" \
		|| { echo "FAIL: the independent decoder printed nothing"; fails=$((fails+1)); }
else
	echo "FAIL: the selftest left no marked file beside its copy"; fails=$((fails+1))
fi

echo
if [ "$fails" -eq 0 ]; then
	echo "water_flow.sh PASS"
	exit 0
fi
echo "water_flow.sh FAIL ($fails)"
exit 1
