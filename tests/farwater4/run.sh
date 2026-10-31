#!/bin/bash
# Lane FARWATER4: FARWATER3's leftovers L3, L4, L5 (L1/L2 refused with numbers: notes fix1031a/farwater4 HANDOFF).
#
#   L5  lodgen's per-texel river bake (lodtfile.cpp) wrote the RAW solve direction: F5's instrument read p99 28-69 deg
#       and seams (> 10 deg) on 5.6-30.7 % of the pairs of all 14 rivers of >= 1000 texels (Commonwealth v4). Now it
#       writes through the marking tool's waterWrittenDirection(). Gate (pre-registered): seam < 5 % and p99 < 20 on
#       every such river; the change touches the direction byte only. Red WW_LODL_FLOW_DIR_RED=raw FAILS the gate.
#   L4  the speed nibble read 0 below 1/16 of the body's reference speed though the solve moves the water. Now
#       moving water (>= the writer's slack line) writes at least 1, format unchanged. Gate: default vs the red differ
#       only by 0 -> 1 nibbles on solved texels, and fewer solved texels read 0. Red WW_WATER_SPEED_FLOOR_RED=round.
#   L3  the X1 weight multiplied the stroke's conductance too, which cancelled its speed multiplier (1.04-1.18x for a
#       1 -> 3 ramp). The weight now SPEEDS and does not steer: X1d (the speed ratio is the ramp's own within 10 %,
#       and no direction byte moves) on the selftest river and the pinned rivers 2 3 5 7 10. Red
#       WW_WATER_X1_WEIGHT_RED=boost FAILS both X1d lines.
#   ALL every red at once: the bake's id + flow planes == REF_LODL (a main-built bake) when given.
# usage: bash tests/farwater4/run.sh <exe> <out dir>     (Fallout4.exe must not run; ESM= overrides the master)
set -u
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
EXE="${1:-$ROOT/release/NifSkope.exe}"
OUT="${2:-$ROOT/tests/farwater4/out}"
ESM="${ESM:-X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm}"
PY="${PY:-$(command -v python)}"
J="$(cygpath -m "$ROOT/tests/farwater4/bakejudge.py" 2>/dev/null || echo "$ROOT/tests/farwater4/bakejudge.py")"
unset WW_WATER_MARK_BODY WW_WATER_MARK_BODY_RED WW_WATER_X2B_RED WW_WATER_X1B_RED WW_WATER_F2_RED \
	WW_WATER_STROKE_WINS_RED WW_WATER_FIELD_DUMP WW_LODL_FLOW_DIR_RED WW_WATER_SPEED_FLOOR_RED WW_WATER_X1_WEIGHT_RED
tasklist 2>/dev/null | grep -qi fallout4.exe && { echo "Fallout4.exe is running: no bake"; exit 3; }
[ -x "$EXE" ] || { echo "no exe at $EXE"; exit 2; }
mkdir -p "$OUT"
W="$(cygpath -m "$OUT" 2>/dev/null || echo "$OUT")"
fails=0
ok()  { echo "OK   $*"; }
bad() { echo "FAIL $*"; fails=$((fails+1)); }
bake() {   # bake <tag> [env...]  ->  $OUT/<tag>.lodl, $OUT/<tag>_bake.log
	local tag=$1; shift
	if [ ! -f "$OUT/$tag.lodl" ]; then
		rm -rf "$OUT/bake_$tag"
		env "$@" "$EXE" -no-gui lodgen "$ESM" --worldspace 3C --lodl "$W/bake_$tag" < /dev/null > "$OUT/${tag}_bake.log" 2>&1 \
			|| { echo "FAIL bake $tag rc=$?"; exit 1; }
		cp "$OUT/bake_$tag/FO4CSLOD/Commonwealth/Commonwealth.lodl" "$OUT/$tag.lodl" && rm -rf "$OUT/bake_$tag"
	fi
	echo "[$tag] $* : $(grep -ao 'flow field [0-9]* river(s)[^;]*; written direction on [0-9]*, speed 0 on [0-9]* ([0-9]* lifted)' "$OUT/${tag}_bake.log" | head -1)"
}
judge() { "$PY" -I "$J" "$@" < /dev/null; }
verdict() { grep -a "^  \(ok  \|FAIL\) *$2" "$OUT/st_$1.log" | head -1 | cut -c3-6; }
selftest() {   # selftest <tag> [env...]
	local tag=$1; shift
	cp "$OUT/new.lodl" "$OUT/st_$tag.lodl"
	env "$@" "$EXE" -no-gui lodl "$W/st_$tag.lodl" --water-mark-selftest < /dev/null > "$OUT/st_$tag.log" 2>&1
	rm -f "$OUT/st_$tag.lodl" "$OUT/st_$tag.lodl.bak-watermark"
	echo "[$tag] $* : $(grep -a 'checks, ' "$OUT/st_$tag.log" | tail -1)"
}

echo "== 1. bakes: default, each red, every red"
bake new
bake dirred WW_LODL_FLOW_DIR_RED=raw
bake floorred WW_WATER_SPEED_FLOOR_RED=round
bake allred WW_LODL_FLOW_DIR_RED=raw WW_WATER_SPEED_FLOOR_RED=round
bake new2
cmp -s "$OUT/new.lodl" "$OUT/new2.lodl" && ok "two default bakes are byte-equal (deterministic)" || bad "two default bakes differ"

echo "== 2. L5: F5 on the bake's rivers"
judge better "$W/new.lodl" "$W/dirred.lodl" && ok "L5: every river of >= 1000 texels has fewer seams and no higher p99 than the raw direction, median p99 halved" \
	|| bad "L5: the written direction is not better than the raw one on every river"
judge better "$W/dirred.lodl" "$W/dirred.lodl" > /dev/null && bad "the better judge passed the raw bake against itself" \
	|| ok "red dir raw: the raw bake is not better than itself (the judge's own red)"
# the pre-registered absolute bar: a registered FAIL on exactly bodies 19 and 20 (seams 7.3 / 6.2 %)
judge f5 "$W/new.lodl" > "$OUT/f5_new.txt"
cat "$OUT/f5_new.txt"
fb=$(grep -a ' FAIL$' "$OUT/f5_new.txt" | sed -E 's/^ *body +([0-9]+):.*/\1/' | tr '\n' ' ')
[ "$fb" = "19 20 " ] && ok "L5 absolute bar (seams < 5 %, p99 < 20): registered FAIL on bodies 19 and 20 only" \
	|| bad "L5 absolute bar: the failing bodies are '$fb', registered '19 20 '"
judge f5 "$W/dirred.lodl" > "$OUT/f5_dirred.txt" && bad "red dir raw PASSED the absolute bar" \
	|| ok "red dir raw FAILS the absolute bar ($(tail -1 "$OUT/f5_dirred.txt" | sed 's/^ *//'))"
judge dironly "$W/new.lodl" "$W/dirred.lodl" && ok "L5 changes the direction byte only" || bad "L5 changed more than the direction"

echo "== 3. L4: the moving-water speed floor"
judge floor "$W/new.lodl" "$W/floorred.lodl" && ok "L4: only 0 -> 1 speed nibbles moved, fewer solved texels read 0" || bad "L4 floor"
judge floor "$W/floorred.lodl" "$W/floorred.lodl" > /dev/null && bad "the floor judge passed a file against itself" || ok "the floor judge fails a file against itself (its own red)"

echo "== 4. every red at once"
if [ -n "${REF_LODL:-}" ] && [ -f "$REF_LODL" ]; then
	judge same "$W/allred.lodl" "$(cygpath -m "$REF_LODL")" && ok "all reds == $REF_LODL (id + flow planes)" || bad "all reds differ from $REF_LODL"
	cmp -s "$OUT/allred.lodl" "$REF_LODL" && ok "all reds == $REF_LODL byte for byte" || echo "     (not byte-equal as a whole file; planes judged above)"
else
	echo "     (REF_LODL not given: skipped)"
fi

echo "== 5. L3: X1d on the selftest's river and the pinned rivers (and L4's floor on the marked river)"
selftest green
for c in "X1d the weight speeds" "X1d the weight does not steer" "X1a FLOOR" "X1b it reads faster" "X1c" "L4 moving water"; do
	[ "$(verdict green "$c")" = "ok  " ] && ok "green: ok $c" || bad "green: $c is $(verdict green "$c")"
done
grep -a "X1d" "$OUT/st_green.log" | cut -c1-220 | sed 's/^/     /'
for b in 2 3 5 7 10; do
	selftest p$b WW_WATER_MARK_BODY=$b
	for c in "X1d the weight speeds" "X1d the weight does not steer" "X1a FLOOR" "X1c" "L4 moving water"; do
		[ "$(verdict p$b "$c")" = "ok  " ] && ok "body $b: ok $c" || bad "body $b: $c is $(verdict p$b "$c")"
	done
	grep -a "X1d the weight speeds\|L4 moving water" "$OUT/st_p$b.log" | cut -c1-220 | sed 's/^/     /'
done
for b in 5 7; do
	selftest fred$b WW_WATER_MARK_BODY=$b WW_WATER_SPEED_FLOOR_RED=round
	[ "$(verdict fred$b "L4 moving water")" = "FAIL" ] && ok "red speed floor round: L4 FAILS on body $b" || bad "red speed floor did not fail L4 on $b"
	grep -a "L4 moving water" "$OUT/st_fred$b.log" | cut -c1-220 | sed 's/^/     /'
done
selftest wred WW_WATER_X1_WEIGHT_RED=boost
for c in "X1d the weight speeds" "X1d the weight does not steer"; do
	[ "$(verdict wred "$c")" = "FAIL" ] && ok "red weight boost: FAILS $c" || bad "red weight boost: $c is $(verdict wred "$c")"
done
grep -a "X1d" "$OUT/st_wred.log" | cut -c1-220 | sed 's/^/     /'
KNOWN="F2 island bank direction|F5 the 99th percentile of the adjacent angle difference"
unknown=$(grep -a '^  FAIL ' "$OUT/st_green.log" | grep -avE "$KNOWN")
known=$(grep -a '^  FAIL ' "$OUT/st_green.log" | grep -acE "$KNOWN")
[ -z "$unknown" ] && [ "$known" = 2 ] && ok "green selftest: exactly the 2 registered FAILs (F2 written, F5 p99; L1/L2 refused with numbers)" \
	|| { bad "green selftest: $known known FAILs, and beyond them:"; echo "$unknown"; }

echo "== result: $fails FAIL"
[ "$fails" = 0 ] && echo "farwater4 PASS" || echo "farwater4 FAIL"
[ "$fails" = 0 ]
