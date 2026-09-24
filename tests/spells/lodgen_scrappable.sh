#!/usr/bin/env bash
# lodgen_scrappable.sh -- the `.lodi` v9 workshop-scrappable bit (bit 6).
#
# WHAT THIS GATE IS FOR. A placement the player can scrap at a workshop is a
# placement that will not be there, and a far field that keeps drawing it -- and
# keeps casting its shadow -- is wrong about a settlement from the first hour of
# a save onwards. Bit 6 of the instance flags word says which placements those
# are. The bit is the one piece of lane HORIZON3 that lane HORIZONOUT kept when
# bungo dropped the baked-horizon route on 2026-09-19; it answers a need of its
# own and never depended on the horizon.
#
# THE LEGS
#
#   G1 THE CENSUS THE BAKE WROTE. `native-scrappable: ON: N of M` is read back
#      out of the bake log, and N is the number every other leg has to meet.
#
#   G2 THE RULE, RE-DERIVED. `lodgen_scrappable_rule.py` reads the three clauses
#      out of `Fallout4.esm` (COBJ scrap recipes with their FormLists, XPRM box
#      build areas linked to a workshop bench, the UnscrappableObject keyword)
#      and the ANSWER out of bit 6 of the written file, and requires them to
#      agree PLACEMENT FOR PLACEMENT, not only in total. It shares no code with
#      the bake.
#
#   G3 FIVE NAMED EXAMPLES EACH WAY, printed with their REFR, base, editor id
#      and position, and for the refusals the clause that failed -- so a person
#      can check one by hand in the Creation Kit.
#
#   G4 THE RED CONTROL. `lodgen_scrappable_flip.py` copies the `.lodi`, flips
#      ONE flags byte, and the count must move by exactly one. A count that
#      cannot move was never reading those bytes.
#
#   G5 THE RETIRED VERSION STILL OPENS. A `.lodi` version 8 -- the baked-horizon
#      file that `release/NifSkope.before_horizonout.exe` still writes -- must
#      open on this exe, name its horizon stream in the dump, and not crash.
#      The reader's tolerance is the promise that a v8 file met in the wild is
#      opened honestly rather than refused (`src/lodifile.h:231`).
#
# FIXTURES. A `--scrappable` bake of the measured urban region (cells 0 -12 to
# 11 -1, dim 4) and lane HORIZON1's v8 bake. Large and untracked; when one is
# absent the gate SKIPS with the path NAMED and does not pass.
#
# ONE NifSkope at a time, second monitor, never a desktop capture.
#
# USAGE
#   bash tests/spells/lodgen_scrappable.sh
#   EXE=<exe> NAT=<prefix> V8=<prefix> EXPECT=14 bash tests/spells/lodgen_scrappable.sh

set -u

. "$(dirname "$0")/_harness.sh" 2>/dev/null || true
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
EXE="${EXE:-$ROOT/release/NifSkope.exe}"
PY="${PY:-$(command -v python || echo /c/Windows/py)}"
ESM="${ESM:-X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm}"

LANE="$ROOT/scratchpad/horizonout_20260919"
NAT="${NAT:-$LANE/scrap/nat/FO4CSLOD/Commonwealth/Commonwealth}"
BAKELOG="${BAKELOG:-$LANE/scrap/bake.log}"
V8="${V8:-$ROOT/scratchpad/horizon1_20260918/v8/nat/FO4CSLOD/Commonwealth/Commonwealth}"
OUT="${OUT:-$ROOT/scratchpad/lodgen_scrappable_gate}"
EXPECT="${EXPECT:-14}"

pass=0; fail=0
ck () {  # ck <name> <rc> <detail>
	if [ "$2" -eq 0 ]; then echo "  ok   $1 -- $3"; pass=$((pass+1));
	else echo "  FAIL $1 -- $3"; fail=$((fail+1)); fi
}

[ -x "$EXE" ] || { echo "SKIP: no NifSkope.exe at $EXE"; exit 2; }
for f in "$NAT.lodi" "$ESM" "$ROOT/tests/spells/lodgen_scrappable_rule.py" \
         "$ROOT/tests/spells/lodgen_scrappable_flip.py"; do
	[ -f "$f" ] || { echo "SKIP: fixture missing -- $f"; exit 2; }
done
if tasklist 2>/dev/null | grep -qi -E "Fallout4"; then
	echo "SKIP: Fallout4 is up -- no exe runs while the game holds the files"; exit 2
fi

if ! type winpath >/dev/null 2>&1; then
	winpath() {
		case "$1" in
			/[a-zA-Z]/*) local d="${1:1:1}"; printf '%s' "${d}:${1:2}" ;;
			*) printf '%s' "$1" ;;
		esac
	}
fi
mkdir -p "$OUT"; rm -f "$OUT"/*.log "$OUT"/*.lodi

echo "exe   $EXE  ($(stat -c '%y %s B' "$EXE" 2>/dev/null))"
echo "file  $NAT.lodi  ($(stat -c '%s B' "$NAT.lodi" 2>/dev/null))"

# --------------------------------------------------------------- G1
echo "== G1 the census the bake wrote"
CEN=""
if [ -f "$BAKELOG" ]; then
	CEN="$(tr -d '\r' < "$BAKELOG" | grep -m1 "native-scrappable: ")"
fi
if [ -n "$CEN" ]; then
	echo "  $CEN" | cut -c1-200
	N="$(printf '%s' "$CEN" | sed -n 's/.*ON: \([0-9][0-9]*\) of .*/\1/p')"
	[ -n "$N" ]; ck "the census line is ON and carries a count" $? "scrappablePlacements $N"
	[ "$N" = "$EXPECT" ]; ck "the census count is the standing $EXPECT" $? "read $N"
else
	echo "  (no bake log at $BAKELOG -- G1 reads the file only)"
	N="$EXPECT"
fi

# --------------------------------------------------------------- G2 + G3
echo "== G2/G3 the rule re-derived from the plugin, and named examples"
"$PY" "$ROOT/tests/spells/lodgen_scrappable_rule.py" "$(winpath "$NAT")" \
	--esm "$ESM" --expect "$EXPECT" --examples 5 > "$OUT/rule.log" 2>&1
rrc=$?
grep -E "^(region|  clause|  RULE|  FILE|OK:|FAIL:|  DISAGREE)" "$OUT/rule.log" | head -20
ck "the plugin rule and the file agree placement for placement" $rrc "$OUT/rule.log"

ny="$(grep -cE "^  #[0-9]+ .*area [0-9A-F]" "$OUT/rule.log")"
[ "$ny" -ge 5 ]; ck "five placements that ARE scrappable are named" $? "$ny printed"
nn="$(grep -cE "^  #[0-9]+ .*-- (no scrap recipe|base is Unscrappable|outside every)" "$OUT/rule.log")"
[ "$nn" -ge 5 ]; ck "five that are NOT are named with the clause that failed" $? "$nn printed"
grep -E "^  #[0-9]+ " "$OUT/rule.log" | head -10 | cut -c1-140

# --------------------------------------------------------------- G4
echo "== G4 the red control: one flag byte moves, the count moves"
"$PY" "$ROOT/tests/spells/lodgen_scrappable_flip.py" "$(winpath "$NAT.lodi")" \
	"$(winpath "$OUT/flipped.lodi")" > "$OUT/flip.log" 2>&1
frc=$?
grep -E "^(instance|  bytes|  chunk|  index|  header|  count|RED CONTROL)" "$OUT/flip.log"
ck "flipping one flags byte moves the count by exactly one" $frc "$OUT/flip.log"

# --------------------------------------------------------------- G5
echo "== G5 a version-8 file met in the wild still opens"
if [ -f "$V8.lodi" ]; then
	timeout 300 "$EXE" -no-gui lodgen "$(winpath "$V8.lodo")" \
		--native-verify "$(winpath "$V8.lodo")" "$(winpath "$V8.lodi")" \
		> "$OUT/v8.log" 2>&1
	vrc=$?
	ck "the v8 bake opens on this exe" $vrc "rc $vrc"
	grep -qE "^lodi version 8$" "$OUT/v8.log"
	ck "it is read AS version 8" $? "$(grep -m1 '^lodi version' "$OUT/v8.log")"
	grep -qE "^lodi vertexHorizonBytes [1-9]" "$OUT/v8.log"
	ck "the retired horizon stream is NAMED rather than silently dropped" $? \
		"$(grep -m1 '^lodi vertexHorizonBytes' "$OUT/v8.log")"
else
	echo "  (no v8 fixture at $V8.lodi -- G5 cannot run)"
	fail=$((fail+1))
fi

echo
echo "$pass checks passed, $fail failed"
[ "$fail" -eq 0 ] || exit 1
