#!/bin/bash
#
# WW_WATER_WEIGHTS -- lane WATER6: what the SOLVER does with what the water
# window stores, and the flow PNG's DirectX convention.
#
# The gates were PRE-REGISTERED in scratchpad/lane_water6_report.md section 0
# before any of the C++ existed.  They run inside
# `lodl <copy> --water-mark-selftest` (the marking tool's own harness, which
# already carries WATER4's F gates) and this script reads them back BY NAME so
# that a gate which did not run is a failure, not a pass:
#
#   X1a  a per-point weight of exactly 1 reproduces the UNWEIGHTED solve word
#        for word -- the floor, and the refuter for the whole weight change;
#   X1b  weights ramped 1 -> 3 change the solve, and the reach reads faster
#        where the weight is (both nibble differences printed);
#   X1c  the weight is LOCAL: some of the body's texels move, not all;
#   X2a  a ONE-POINT curve is consumed (it was skipped before), with the
#        refuter -- the same body with the pin removed -- run FIRST;
#   X2b  and it acts as a SOURCE: the NET OUTWARD FLUX through a thin ring at
#        two pin widths rises by more than 0.5 per texel when the pin is added,
#        with the same ring in the NO-PIN state as its floor (below 0.2).
#        LANE WATER7 replaced the instrument here, red 1 of BUILD10: the old
#        one compared the flow with the straight-line radial over a DISC of
#        four widths and read 0.742 on body 2 against 0.371 on body 3 with the
#        pin behaving identically -- the Charles bends inside four widths, so
#        the cosine fell for a reason that is not about the pin.  Through-flow
#        enters and leaves one ring and cancels in the difference, whatever the
#        channel does, which is why curvature cannot bias this one.  The old
#        disc number is still printed, informational, beside the new one;
#   X3a  an imported raster layer is the AUTHORITY where painted;
#   X3b  the solve fills the rest, and no texel outside the layer carries its
#        word;
#   X3c  it is an authority and not a bake: remove it and every word returns;
#   X5a  the DirectX cardinals: east R255 G128, north R128 G1, west R1 G128,
#        south R128 G255 -- green grows toward the image BOTTOM;
#   X5b  the codec round-trips all 65,536 words;
#   X5c  tests/fixtures/flowmap_directx_4x4.png -- a CHECKED-IN image written
#        from the rule by a script that shares no code with the codec -- decodes
#        to the sixteen documented directions.
#
# X4 (export -> import byte-identical) and X6 (the flipped green is refused)
# live in tests/spells/water_window.sh, gates W5 and W6, and are not repeated
# here; run that one too.
#
# USAGE
#   bash tests/spells/water_weights.sh
#   LODL=/path/Commonwealth.lodl bash tests/spells/water_weights.sh
#
# The fixture is the version-3 file lane WATER2 wrote (.gitignore'd, six
# seconds to regenerate with `lodgen ... --water-bodies`).

set -u

. "$(dirname "$0")/_harness.sh"

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
NS="${EXE:-$ROOT/release/NifSkope.exe}"
LODL="${LODL:-$ROOT/scratchpad/water2_20260909/out/Terrain/Commonwealth.lodl}"
WORK="${WORK:-$ROOT/scratchpad/build10_20260910/work}"

[ -x "$NS" ] || { echo "no NifSkope.exe at $NS"; exit 2; }
[ -f "$LODL" ] || { echo "no version-3 .lodl at $LODL (write one with --water-bodies)"; exit 2; }
[ -f "$ROOT/tests/fixtures/flowmap_directx_4x4.png" ] \
	|| { echo "FAIL: the checked-in DirectX test image is missing"; exit 2; }

mkdir -p "$WORK"
rm -f "$WORK"/weights.lodl "$WORK"/weights.lodl.bak-watermark "$WORK"/weights.lodl.tmp-watermark

fails=0
cp "$LODL" "$WORK/weights.lodl" || { echo "FAIL: could not copy the fixture"; exit 1; }
echo "== lane WATER6's gates, inside the marking tool's harness, on a copy of $(basename "$LODL") =="
WW_WATER_DIRECTX_FIXTURE="$(winpath "$ROOT/tests/fixtures/flowmap_directx_4x4.png")" \
	"$NS" -no-gui lodl "$WORK/weights.lodl" --water-mark-selftest > "$WORK/weights.txt" 2>&1
rc=$?
sed -n '/lane WATER6/,$p' "$WORK/weights.txt"

for g in "X1a FLOOR" "X1b SIGNAL" "X1b it reads faster" "X1c the weight is LOCAL" \
	"X2a REFUTER" "X2a a ONE-POINT curve" "X2b FLOOR" "X2b it acts as a SOURCE" \
	"X3 the raster layer is stored" "X3a the raster is the AUTHORITY" \
	"X3b the solve fills the REST" "X3c the raster is not baked in" \
	"X5a the DirectX cardinals" "X5b the codec round-trips" \
	"X5c the checked-in test image is readable" "X5c the checked-in test image decodes"; do
	line="$(grep -a -F "$g" "$WORK/weights.txt" | grep -a -E '^  (ok|FAIL) ' | head -1)"
	if [ -z "$line" ]; then
		echo "FAIL: gate '$g' did not run"; fails=$((fails+1))
	elif ! printf '%s' "$line" | grep -aq '^  ok '; then
		echo "FAIL: gate '$g' is red"; fails=$((fails+1))
	fi
done

NX="$(grep -a -c '^  ok   X[0-9]' "$WORK/weights.txt")"
# lane WATER7: X2b became TWO gates (the ring, and the no-pin floor beside it),
# so a green run counts 17 where it counted 16 and the floor rises by one with
# it -- still one below a green run, so losing a gate still goes red.
echo "WATER6 gates green: $NX (floor 16)"
[ "${NX:-0}" -ge 16 ] || { echo "FAIL: only $NX of lane WATER6's gates ran green, floor is 16"; fails=$((fails+1)); }

# the whole selftest's own count, so a red elsewhere is not hidden by this spell
grep -a -E '^[0-9]+ checks, [0-9]+ failures' "$WORK/weights.txt" | tail -1
echo "the selftest exited $rc (2 of its checks are WATER4's pre-registered reds: F2 island bank, F5 p99)"

echo
if [ "$fails" -eq 0 ]; then
	echo "water_weights.sh PASS"
	exit 0
fi
echo "water_weights.sh FAIL ($fails)"
exit 1
