#!/bin/bash
# Lane FARWATER3: FARWATER2's leftovers in the marking tool's selftest (`lodl <f> --water-mark-selftest`). No window.
#
#   X2b  the one-point pin "acts as a SOURCE": was a sum of UNIT outward directions on a ring, which counted the dead
#        reach the pin leaves (its written direction is the writer's continuation) and read 0.644 / 0.410 / 0.551 /
#        0.438 / -0.003 on rivers 2 / 3 / 5 / 7 / 10. Now the solve's net face flux out of the ring over the pin's
#        injection (WaterMarkDoc::ringFlux; WATER6 section 6's instrument). Green: > 0.9 on all five rivers, and the
#        floor (the same point as an OUTLET pin) < 0. Red WW_WATER_X2B_RED=unit: the old gate, FAILS on body 3.
#   X1b  "it reads faster where the weight is": two causes, measured.
#        (a) a CODE defect, body 10: the stroke runs from the mouth upstream, its source is its own first end and its
#            sink the table's drain contact, both at the mouth; the authority rule ("the stroke wins when it runs
#            against the drain") never looked because one end was the stroke's, and voted with unit vectors (dead
#            water's noise). Fixed in solveBody (WW_WATER_STROKE_WINS_RED=old = the old rule; X1b FAILS on body 10).
#        (b) the GATE, bodies 5 and 7: the stroke's last third lies in water the 4-bit nibble cannot resolve (body 5
#            past the drain contact, slack by the writer's own rule; body 7 a pool at ~1/24 of the body mean): 0.17 /
#            0.33 nibbles in BOTH solves. Now read in the solve's own unquantised speed over the stroke's moving
#            water. Red WW_WATER_X1B_RED=nibble: FAILS on bodies 5 and 7.
#   F2   the island-bank gate judged the RAW solve, which the writer discards at bank texels. Now it judges the
#        WRITTEN word, against the exact flow round the island's own staircase (2x2 subcells): the analytic cylinder
#        is a geometry the mask does not hold (the staircase's own flow is ~8.3 / 15.5 deg off it), and the raw solve
#        sits ~4 / 8 deg from the staircase flow while the written word sits ~18 / 46 (the writer's continuation and
#        low-pass; F5's p99 needs that low-pass). F2 stays a registered FAIL, now attributed to the writer.
#        Red WW_WATER_F2_RED=raw: the old line (raw vs cylinder), FAILS.
#   F5   p99 < 5 deg stays as registered (not a writer defect: notes fix1030c/farwater3 HANDOFF, the twin's sweep).
#   ALL  every red at once == main's code (the ok/FAIL lines of a selftest log of main, REF_LOG=, when given).
# usage: bash tests/farwater3/run.sh [exe] [out dir]      (Fallout4.exe must not run; ESM= overrides the master)
set -u
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
EXE="${1:-$ROOT/release/NifSkope.exe}"
OUT="${2:-$ROOT/tests/farwater3/out}"
ESM="${ESM:-X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm}"
KNOWN="F2 island bank direction|F5 the 99th percentile of the adjacent angle difference"
unset WW_WATER_MARK_BODY WW_WATER_MARK_BODY_RED WW_WATER_X2B_RED WW_WATER_X1B_RED WW_WATER_F2_RED \
	WW_WATER_STROKE_WINS_RED WW_WATER_FIELD_DUMP
tasklist 2>/dev/null | grep -qi fallout4.exe && { echo "Fallout4.exe is running: no bake"; exit 3; }
[ -x "$EXE" ] || { echo "no exe at $EXE"; exit 2; }
mkdir -p "$OUT"
W="$(cygpath -m "$OUT" 2>/dev/null || echo "$OUT")"
fails=0
ok()  { echo "OK   $*"; }
bad() { echo "FAIL $*"; fails=$((fails+1)); }
lines() { grep -a '^  ok \|^  FAIL ' "$1" | sed -E 's/[0-9.]+ s\b/T s/g; s/[0-9]+ iterations/N iterations/g; s/residual [0-9.e-]+/residual R/g; s#/lanes/[^/]+/release/#/lanes/TREE/release/#g'; }
verdict() { grep -a "^  \(ok  \|FAIL\) *$2" "$OUT/st_$1.log" | head -1 | cut -c3-6; }   # verdict <tag> <check words>
selftest() {   # selftest <tag> [env...]
	local tag=$1; shift
	cp "$OUT/base.lodl" "$OUT/st_$tag.lodl"
	env "$@" "$EXE" -no-gui lodl "$W/st_$tag.lodl" --water-mark-selftest < /dev/null > "$OUT/st_$tag.log" 2>&1
	rm -f "$OUT/st_$tag.lodl" "$OUT/st_$tag.lodl.bak-watermark"
	echo "[$tag] $* : $(grep -a 'checks, ' "$OUT/st_$tag.log" | tail -1)  $(grep -a '^river  = ' "$OUT/st_$tag.log" | cut -c1-30)"
}

echo "== 1. base bake"
if [ ! -f "$OUT/base.lodl" ]; then
	rm -rf "$OUT/bake"
	"$EXE" -no-gui lodgen "$ESM" --worldspace 3C --lodl "$W/bake" < /dev/null > "$OUT/base_bake.log" 2>&1 \
		|| { echo "FAIL base bake rc=$?"; exit 1; }
	cp "$OUT/bake/FO4CSLOD/Commonwealth/Commonwealth.lodl" "$OUT/base.lodl" && rm -rf "$OUT/bake"
fi
ls -la "$OUT/base.lodl"

echo "== 2. GREEN: the selftest's own river, and the rivers pinned (2, 3, 5, 7, 10)"
selftest green
unknown=$(grep -a '^  FAIL ' "$OUT/st_green.log" | grep -avE "$KNOWN")
known=$(grep -a '^  FAIL ' "$OUT/st_green.log" | grep -acE "$KNOWN")
[ -z "$unknown" ] && [ "$known" = 2 ] && ok "green: exactly the 2 registered FAILs (F2 written, F5 p99)" \
	|| { bad "green: $known known FAILs, and beyond them:"; echo "$unknown"; }
for b in 2 3 5 7 10; do
	selftest p$b WW_WATER_MARK_BODY=$b
	for c in "X2b FLOOR" "X2b it acts as a SOURCE" "X1b it reads faster" "X1b SIGNAL" "X1a FLOOR" "X1c"; do
		[ "$(verdict p$b "$c")" = "ok  " ] && ok "body $b: ok $c" || bad "body $b: $c is $(verdict p$b "$c")"
	done
	grep -a "X2b it acts as a SOURCE\|X1b it reads faster" "$OUT/st_p$b.log" | cut -c1-150 | sed 's/^/     /'
done
# F2's reference is reachable: the raw solve meets the bar against the staircase's own flow
raw=$(grep -a '^F2 against the staircase.s own flow: the raw solve mean' "$OUT/st_green.log" | sed -E 's/.*mean ([0-9.]+) \/ max ([0-9.]+).*/\1 \2/')
echo "  F2 raw solve vs the staircase flow: $raw"
python -c "import sys; m, M = map(float, sys.argv[1:3]); sys.exit(0 if m < 5 and M < 15 else 1)" $raw \
	&& ok "F2: the reference is reachable (the raw solve passes the bar against it)" || bad "F2: the raw solve fails the staircase reference ($raw)"
grep -aq "^  FAIL F2 island bank direction as WRITTEN" "$OUT/st_green.log" && ok "F2 judges the written word (registered FAIL)" \
	|| bad "F2 does not judge the written word"

echo "== 3. REDS (each must FAIL on its own gate)"
selftest x2red WW_WATER_X2B_RED=unit
[ "$(verdict x2red "X2b it acts as a SOURCE")" = "FAIL" ] && ok "red X2b unit: FAILS on body 3" || bad "red X2b unit did not fail"
for b in 5 7; do
	selftest x1red$b WW_WATER_MARK_BODY=$b WW_WATER_X1B_RED=nibble
	[ "$(verdict x1red$b "X1b it reads faster")" = "FAIL" ] && ok "red X1b nibble: FAILS on body $b" || bad "red X1b nibble did not fail on $b"
done
selftest winsred WW_WATER_MARK_BODY=10 WW_WATER_STROKE_WINS_RED=old
[ "$(verdict winsred "X1b it reads faster")" = "FAIL" ] && ok "red stroke rule old: X1b FAILS on body 10" || bad "red stroke rule did not fail on 10"
grep -a "^X1 the unweighted" "$OUT/st_winsred.log" | cut -c1-200 | sed 's/^/     /'
grep -a "^X1 the unweighted" "$OUT/st_p10.log" | cut -c1-200 | sed 's/^/     /'
grep -aq "the stroke won" "$OUT/st_p10.log" && ok "green body 10: the stroke won" || bad "green body 10: the stroke did not win"
selftest f2red WW_WATER_F2_RED=raw
grep -aq "^  FAIL F2 island bank direction against the analytic cylinder" "$OUT/st_f2red.log" \
	&& ok "red F2 raw: the old line, FAILS" || bad "red F2 raw did not give the old failing line"

echo "== 4. ALL REDS == main's code"
selftest allred WW_WATER_X2B_RED=unit WW_WATER_X1B_RED=nibble WW_WATER_F2_RED=raw WW_WATER_STROKE_WINS_RED=old
n=$(grep -ac '^  FAIL ' "$OUT/st_allred.log")
[ "$n" = 3 ] && ok "all reds: 3 FAIL (F2 raw, F5 p99, X2b unit), as main" || bad "all reds: $n FAIL"
if [ -n "${REF_LOG:-}" ] && [ -f "$REF_LOG" ]; then
	diff <(lines "$REF_LOG") <(lines "$OUT/st_allred.log") > "$OUT/allred_vs_ref.diff" \
		&& ok "all reds == $REF_LOG (ok/FAIL lines)" || bad "all reds differ from $REF_LOG (allred_vs_ref.diff)"
fi

echo "== result: $fails FAIL"
[ "$fails" = 0 ] && echo "farwater3 PASS" || echo "farwater3 FAIL"
[ "$fails" = 0 ]
