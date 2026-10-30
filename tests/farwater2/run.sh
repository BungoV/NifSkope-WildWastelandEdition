#!/bin/bash
# Lane FARWATER2: the marking tool's selftest picks a river WITH A MOUTH, and the marking panel's re-bake line.
# No window: `-no-gui` lodgen + lodl only.
#
#   1. bake the Commonwealth .lodl (v4, landscape stage only)                                  -> base
#   2. GREEN  `--water-mark-selftest` on a copy: the river is the largest whose outlet touches it (body 3 on the
#             Commonwealth: it drains into the sea); the mouth, dye and F5 checks are ok, and the FAILs left are
#             exactly the $KNOWN ones (not the body choice: see below; 61 of 64)
#   3. RED    WW_WATER_MARK_BODY_RED=largest (today's pick, body 2: its outlet 19 never touches it): MUST FAIL,
#             with the 8 FAILs of FARWATER1's selftest.log (F2, F5 x3, dye x4)
#   4. SAME   WW_WATER_MARK_BODY_RED=largest + WW_WATER_MARK_BODY=<the green's body> (the pin main already had):
#             the same ok/FAIL lines as the green -- the new rule is main's code on the chosen body
#   5. NOTE   base as <f>, the fixture-marked copy as <f>.premarks: `--water-rebake-note` prints the census line the
#             real re-bake prints (a re-bake over the marked file, its log's "water marks carried:" line), and reads
#             only (both files' sha unchanged); with no .premarks: "none"
#   6. RED    WW_WATER_REBAKE_NOTE_RED=off (the panel's old text, no line): MUST FAIL the census compare
# usage: bash tests/farwater2/run.sh [exe] [out dir]      (Fallout4.exe must not run; ESM= overrides the master)
set -u
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
EXE="${1:-$ROOT/release/NifSkope.exe}"
OUT="${2:-$ROOT/tests/farwater2/out}"
ESM="${ESM:-X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm}"
# The FAILs that are not the body choice, measured 10-30 (notes fix1030b/farwater2/pins.log, the selftest pinned to
# rivers 2, 3, 5, 7, 10 of the v4 bake):
#   F2 island bank      synthetic mask, no body in it: fails on every body (WW_CHANGES "F2's island-bank gate FAILS
#                       as registered", lane WATER4; the writer continues bank texels for that very 12-degree bias)
#   F5 p99              fails on every river (7.0 .. 14.1 against 5; WATER4 "F5's p99 FAILS as registered")
#   X2b source          WATER6/7's ring gate, registered on body 2 (0.644): 0.410 on body 3, 0.438 on 7, -0.003 on 10
# Lane FARWATER3 (10-30): X2b is now the solved net flux (1.000 on bodies 2, 3, 5, 7, 10) and passes; F2 judges the
# WRITTEN direction against the island's own staircase flow (still red: the writer's low-pass, tests/farwater3).
# The green must FAIL exactly these two and nothing else.
KNOWN="${KNOWN:-F2 island bank direction|F5 the 99th percentile of the adjacent angle difference}"
NKNOWN="${NKNOWN:-2}"
unset WW_WATER_MARK_BODY WW_WATER_MARK_BODY_RED WW_WATER_REBAKE_NOTE_RED WW_LODL_MARK_CARRY_RED
tasklist 2>/dev/null | grep -qi fallout4.exe && { echo "Fallout4.exe is running: no bake"; exit 3; }
[ -x "$EXE" ] || { echo "no exe at $EXE"; exit 2; }
mkdir -p "$OUT"
W="$(cygpath -m "$OUT" 2>/dev/null || echo "$OUT")"
F="$W/bake/FO4CSLOD/Commonwealth/Commonwealth.lodl"
fails=0
ok()  { echo "OK   $*"; }
bad() { echo "FAIL $*"; fails=$((fails+1)); }
# status + check text with the timings and iteration counts taken out (they move run to run)
lines() { grep -a '^  ok \|^  FAIL ' "$1" | sed -E 's/[0-9.]+ s\b/T s/g; s/[0-9]+ iterations/N iterations/g; s/residual [0-9.e-]+/residual R/g; s#/lanes/[^/]+/release/#/lanes/TREE/release/#g'; }
selftest() {   # selftest <tag> [env...]
	local tag=$1; shift
	cp "$OUT/base.lodl" "$OUT/st_$tag.lodl"
	env "$@" "$EXE" -no-gui lodl "$W/st_$tag.lodl" --water-mark-selftest < /dev/null > "$OUT/st_$tag.log" 2>&1
	local rc=$?
	rm -f "$OUT/st_$tag.lodl" "$OUT/st_$tag.lodl.bak-watermark"
	echo "[$tag] rc=$rc  $(grep -a 'checks, ' "$OUT/st_$tag.log" | tail -1)  $(grep -a '^river  = ' "$OUT/st_$tag.log")"
	return $rc
}

rm -rf "$OUT/bake"
echo "== 1. base bake"
"$EXE" -no-gui lodgen "$ESM" --worldspace 3C --lodl "$W/bake" < /dev/null > "$OUT/base_bake.log" 2>&1 \
	|| { echo "FAIL base bake rc=$?"; exit 1; }
cp "$F" "$OUT/base.lodl"
python -I "$ROOT/tests/farwater2/lodl_version.py" "$W/base.lodl" < /dev/null

echo "== 2. GREEN selftest (the river with a mouth)"
selftest green
body=$(grep -a '^river  = body ' "$OUT/st_green.log" | sed -E 's/^river  = body ([0-9]+),.*/\1/')
grep -a '^the river: ' "$OUT/st_green.log"
[ -n "$body" ] && grep -aq '^the river: the largest whose outlet touches it, body '"$body"'\b' "$OUT/st_green.log" \
	&& ok "green: the river is body $body, the largest whose outlet touches it" || bad "green: no river with a mouth chosen"
unknown=$(grep -a '^  FAIL ' "$OUT/st_green.log" | grep -avE "$KNOWN")
known=$(grep -a '^  FAIL ' "$OUT/st_green.log" | grep -acE "$KNOWN")
if [ -z "$unknown" ] && [ "$known" = "$NKNOWN" ]; then ok "green: exactly the $NKNOWN FAILs that are not the body choice ($KNOWN)"
else bad "green: $known of the $NKNOWN known FAILs, and beyond them:"; echo "$unknown"; fi
for c in "the river's dye reaches the body it drains into" "near the mouth the weight is above 128" \
	"beyond three half-distances the weight has fallen" "every dyed texel names the river" \
	"F5 the mean direction still points at the mouth" "F5 seams are below 0.5 percent"; do
	grep -aq "^  ok   $c" "$OUT/st_green.log" && ok "green: ok $c" || bad "green: not ok: $c"
done

echo "== 3. RED WW_WATER_MARK_BODY_RED=largest (today's pick)"
selftest red WW_WATER_MARK_BODY_RED=largest
n=$(grep -ac '^  FAIL ' "$OUT/st_red.log")
[ "$n" -ge 1 ] && ok "red fails as it must ($n FAIL; FARWATER1 measured 8 on main's code)" || bad "red: 0 FAIL"
if [ -n "${REF_LOG:-}" ] && [ -f "$REF_LOG" ]; then
	if diff <(lines "$REF_LOG") <(lines "$OUT/st_red.log") > "$OUT/red_vs_ref.diff"; then
		ok "red == the reference log's ok/FAIL lines ($REF_LOG)"
	else bad "red differs from $REF_LOG (see red_vs_ref.diff)"; fi
fi

echo "== 4. SAME: the old pick pinned to body $body"
selftest same WW_WATER_MARK_BODY_RED=largest WW_WATER_MARK_BODY="$body"
if diff <(lines "$OUT/st_green.log") <(lines "$OUT/st_same.log") > "$OUT/green_vs_same.diff"; then
	ok "green == main's code pinned to body $body (ok/FAIL lines identical)"
else bad "green differs from the pinned old pick (see green_vs_same.diff)"; fi
echo "  the red's FAILs that the body choice made (FAIL on the old pick, ok on body $body):"
checks() { grep -a '^  FAIL ' "$1" | sed -E 's/^  FAIL //; s/ ?\(.*$//'; }   # the check's words, its numbers off
nchoice=0
while read -r c; do
	grep -aqF "  ok   $c" "$OUT/st_green.log" && { echo "    body choice: $c"; nchoice=$((nchoice+1)); }
done < <(checks "$OUT/st_red.log")
echo "  ($nchoice of the red's FAILs were the body choice)"
echo "  the red's FAILs that stay on body $body (not the body choice):"
checks "$OUT/st_red.log" | while read -r c; do
	grep -aqF "  FAIL $c" "$OUT/st_green.log" && echo "    stays: $c"
done

echo "== 5. NOTE: the panel's re-bake line"
cp "$OUT/base.lodl" "$OUT/marked.lodl"
"$EXE" -no-gui lodl "$W/marked.lodl" --water-mark-fixture < /dev/null > "$OUT/fixture.log" 2>&1 || bad "fixture rc=$?"
rm -f "$OUT/marked.lodl.bak-watermark"
# the real re-bake over the marked file: its census line is the reference
cp "$OUT/marked.lodl" "$F"
"$EXE" -no-gui lodgen "$ESM" --worldspace 3C --lodl "$W/bake" < /dev/null > "$OUT/rebake.log" 2>&1 || bad "rebake rc=$?"
want=$(grep -a 'water marks carried:' "$OUT/rebake.log" | head -1 | sed -E 's/^ *//; s/; solve:.*$//')
echo "  the bake said: $want"
[ -n "$want" ] || bad "the re-bake printed no census line"
mkdir -p "$OUT/note"
cp "$OUT/base.lodl" "$OUT/note/Commonwealth.lodl"
note_run() {   # note_run <tag> [env...]
	local tag=$1; shift
	env "$@" "$EXE" -no-gui lodl "$W/note/Commonwealth.lodl" --water-rebake-note < /dev/null > "$OUT/note_$tag.log" 2>&1
	grep -a '^re-bake note: ' "$OUT/note_$tag.log" | head -1
}
none=$(note_run none)
echo "  $none"
[[ "$none" == "re-bake note: none"* ]] && ok "note: no .premarks -> none" || bad "note without .premarks: $none"
cp "$OUT/marked.lodl" "$OUT/note/Commonwealth.lodl.premarks"
s0=$(sha1sum "$OUT/note/Commonwealth.lodl" "$OUT/note/Commonwealth.lodl.premarks" | cut -c1-40 | tr '\n' ' ')
got=$(note_run green)
echo "  $got"
s1=$(sha1sum "$OUT/note/Commonwealth.lodl" "$OUT/note/Commonwealth.lodl.premarks" | cut -c1-40 | tr '\n' ' ')
[[ -n "$want" && "$got" == *"Commonwealth.lodl.premarks -- $want" ]] && ok "note == the re-bake's census line" \
	|| bad "note differs from the re-bake's census line"
[ "$s0" = "$s1" ] && ok "note read only (both files' sha unchanged)" || bad "note changed a file"
ls "$OUT/note" | grep -q 'bak-watermark\|tmp-watermark' && bad "note left a file: $(ls "$OUT/note")" || ok "note left no file"
red=$(note_run red WW_WATER_REBAKE_NOTE_RED=off)
echo "  [red] $red"
[[ -n "$want" && "$red" == *"$want" ]] && bad "red note: the old panel must NOT show the line" || ok "red (old panel, no line) fails the census compare as it must"

echo "== result: $fails FAIL"
[ "$fails" = 0 ] && echo "farwater2 PASS" || echo "farwater2 FAIL"
[ "$fails" = 0 ]
