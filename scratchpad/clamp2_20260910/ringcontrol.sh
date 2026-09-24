#!/bin/sh
# LANE CLAMP2 -- the known-answer control for "the cell owns its own rows".
#
# It does NOT live in tests/spells: lodgen_terrain_vt.sh's pre-registered count
# is 35 checks and this lane is not allowed to move that number to make its own
# result look bigger. Folding it in as check 36 is one `ok`/`bad` pair and is
# offered in the lane report, not taken here.
#
# WHAT IT PROVES, and it is exact rather than statistical: a synthetic pair of
# cells whose shared VHGT row disagrees by 72 world units (the 9-unit worst case
# measured on the real y=31|32 seam) is filled through the SHIPPED
# lodgenTerrainFillRing inside the exe, and every sample of the inner unit's
# four boundary rows/columns must be the inner cell's own value EXACTLY, while
# the samples one step beyond must be the neighbour's (so a filler that simply
# stopped ringing would fail too). The bilinear tap the sheets actually read is
# asserted at the same place, so the claim is about a texel's operand.
#
# THE REFUTER: the same pair filled by the OLD south-to-north order, reproduced
# verbatim inside the self-test. It must give the NEIGHBOUR's value on the inner
# boundary -- it must FAIL the bar above. If it passed, the bar would not
# discriminate and the self-test fails on that alone.
#
# USAGE   bash scratchpad/clamp2_20260910/ringcontrol.sh
# Exit 2 = a missing input; 1 = a failed check.

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
NS="${EXE:-$ROOT/release/NifSkope.exe}"
ESM="${ESM:-X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm}"
DATA="${DATA:-E:/Tools/Fallout 4/DataUnpacked/Data}"
W="$ROOT/scratchpad/clamp2_20260910/ringwork"
rm -rf "$W"; mkdir -p "$W/obj" "$W/tex"

checks=0
fails=0
ok()  { checks=$((checks + 1)); echo "  ok   $1"; }
bad() { checks=$((checks + 1)); fails=$((fails + 1)); echo "  FAIL $1"; }

[ -x "$NS" ] || { echo "no NifSkope.exe at $NS"; exit 2; }
[ -f "$ESM" ] || { echo "no ESM at $ESM"; exit 2; }

echo "== preflight =="
if [ "$NS" -nt "$ROOT/src/lodgen.cpp" ]; then
	ok "the exe is newer than src/lodgen.cpp"
else
	bad "the exe is newer than src/lodgen.cpp -- this is the PREVIOUS build's answer"
fi
echo "       exe: $(ls -l --time-style=+%Y-%m-%d\ %H:%M:%S "$NS" | awk '{print $6, $7}')"

echo "== the self-test, on one dim-4 chunk =="
WW_TERRAIN_RING_TEST=1 "$NS" -no-gui lodgen "$ESM" --worldspace 3C \
	--terrain-region -24 24 -21 27 --dim 4 \
	--out-dir "$W/obj" --tex-dir "$W/tex" --data-root "$DATA" \
	> "$W/ring.log" 2>&1
RC=$?
grep -a '^ring:' "$W/ring.log" | sed 's/^/       /'
[ $RC -eq 0 ] && ok "the bake that carries the self-test succeeded (rc 0)" \
	|| bad "the bake that carries the self-test succeeded (got rc $RC)"

# THE FLOOR, first: an env var nobody reads, a self-test that was optimised out
# or a grep against an empty log would leave every bar below passing on nothing.
NRING="$(grep -ac '^ring:' "$W/ring.log")"
echo "       ring: lines in the log: $NRING"
# 12 lines exactly: the header, the synthetic pair, eight assertions, the
# control and the verdict. A failing assertion still prints its line, so the
# count is a floor on the test HAVING RUN and never a restatement of its result.
[ "$NRING" -ge 12 ] \
	&& ok "FLOOR the self-test actually ran and printed its table ($NRING lines)" \
	|| bad "FLOOR the self-test actually ran and printed its table (only $NRING lines)"

grep -qa "ring:   ok   the inner unit's NORTH boundary row" "$W/ring.log" \
	&& ok "the NORTH boundary row is the inner cell's own value, exactly" \
	|| bad "the NORTH boundary row is the inner cell's own value, exactly"
grep -qa "ring:   ok   the inner unit's EAST boundary column" "$W/ring.log" \
	&& ok "the EAST boundary column is the inner cell's own value, exactly" \
	|| bad "the EAST boundary column is the inner cell's own value, exactly"
grep -qa "ring:   ok   the inner unit's SOUTH boundary row" "$W/ring.log" \
	&& ok "the SOUTH boundary row is unchanged and still the inner cell's" \
	|| bad "the SOUTH boundary row is unchanged and still the inner cell's"
grep -qa "ring:   ok   the inner unit's WEST boundary column" "$W/ring.log" \
	&& ok "the WEST boundary column is unchanged and still the inner cell's" \
	|| bad "the WEST boundary column is unchanged and still the inner cell's"
grep -qa "ring:   ok   one grid step BEYOND the north border" "$W/ring.log" \
	&& ok "one step BEYOND the north border is the neighbour's: the ring still fills" \
	|| bad "one step BEYOND the north border is the neighbour's: the ring still fills"
grep -qa "ring:   ok   one grid step BEYOND the east border" "$W/ring.log" \
	&& ok "one step BEYOND the east border is the neighbour's: the ring still fills" \
	|| bad "one step BEYOND the east border is the neighbour's: the ring still fills"
grep -qa "ring:   ok   the bilinear tap ON the north border" "$W/ring.log" \
	&& ok "the TEXEL's own operand on the border is the inner cell's value" \
	|| bad "the TEXEL's own operand on the border is the inner cell's value"
grep -qa "ring:   ok   the bilinear tap half a step beyond it" "$W/ring.log" \
	&& ok "and half a step beyond it is the exact midpoint of the two" \
	|| bad "and half a step beyond it is the exact midpoint of the two"
grep -qa "ring:   ok   CONTROL the old south-to-north order" "$W/ring.log" \
	&& ok "CONTROL the OLD fill order fails the bar above (it is discriminating)" \
	|| bad "CONTROL the OLD fill order fails the bar above (it is discriminating)"
grep -qa 'ring: self-test .* 0 failures, RESULT PASS' "$W/ring.log" \
	&& ok "the self-test's own verdict is PASS" \
	|| bad "the self-test's own verdict is PASS"

echo
echo "$checks checks, $fails failures"
[ $fails -eq 0 ] && echo "RESULT PASS" || echo "RESULT FAIL"
[ $fails -eq 0 ]
