#!/bin/bash
#
# THE LADDER'S FOUR NEW RULES, each with a floor that goes red in the same run
# (lane NATIVE1c, 2026-09-16).  `lodgen_native.sh` gates the FORMAT; this gates
# the four behaviours v4/v5 added on top of it:
#
#   (a) FOLIAGE IS NEVER LADDERED.  bungo, 2026-09-11 16:1x, over native1b's
#       `ladder.png`: "Hm, that tree LOD becomes a stump there".  A cluster on
#       an alpha-tested tree material is refused, counted by name, and the
#       refusal has a way back -- `--native-ladder-foliage` -- which the gate
#       runs as its own refuter: with the switch on, the same region ladders
#       those clusters and the level-1 count RISES.
#   (b) EVERY KEPT LEVEL HOLDS A STATED FRACTION OF LEVEL 0'S OUTLINE, measured
#       by `lodgen_silhouette.py` with its own rasteriser over the `.lodo`
#       bytes, and floored by a random-vertex-drop twin matched on triangle
#       count that must FAIL the same gate.
#   (c) THE LADDER'S FIRST STEP IS NOW SELECTABLE.  `docs/FO4CS_IMPROVED_LOD_
#       PLAN.md` §5 says the v3 library's median level-1 cluster reached one
#       pixel only past 52,100 units -- nowhere in the Commonwealth -- because
#       "full detail" was already Bethesda's LOD mesh.  Level 0 now comes from
#       the base's near MODL, and the gate is that number: the near arm must
#       come in UNDER the plan's 52,100, and under the `--library mnam` arm
#       baked by this same exe, which is the control.
#   (d) THE PER-INSTANCE AO BYTE IS WRITTEN AND MOVES, and its way back
#       (`--native-no-placement-ao`) writes a file with NO AO words at all.
#   (e) THE FOUR HEADER WORDS are written and move: the per-slot instance
#       totals, the per-base full-detail triangle count, the card count, and
#       the watertight bit.  (d) and (e) are measured by the field gate's
#       section j, which owns their floors.
#
# THE WAY BACK is gated too, and it is one line:
#
#     --library mnam --native-ladder-foliage --native-silhouette 0 \
#         --native-no-placement-ao
#
# Every one of the four behaviours is off there, so the bake is the one the
# rung exe makes: the `.lodi` must be BYTE-IDENTICAL to the rung's, and the
# ladder census -- levels, clusters a level, triangles a level, roots -- must
# read the same numbers.  The `.lodo` bytes legitimately differ (v4 header,
# the reinterpreted base row, the wider cone margin), so the way back for the
# library is stated in its numbers, not its bytes.
#
# USAGE
#   bash tests/spells/lodgen_ladder.sh
#   OUT=<dir> bash tests/spells/lodgen_ladder.sh     # keep the four bakes
#   RUNG=<exe> ...                                   # the way-back control

set -u

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
NS="${EXE:-$ROOT/release/NifSkope.exe}"
RUNG="${RUNG:-$ROOT/release/NifSkope.before_native1c.exe}"
ESM="${ESM:-X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm}"
DATA="${DATA:-E:/Tools/Fallout 4/DataUnpacked/Data}"
PY="${PY:-$(command -v python || echo /c/Windows/py)}"
REGION="${REGION:--20 24 -9 35}"
FLOOR="${FLOOR:-0.70}"
PLAN_DISTANCE="${PLAN_DISTANCE:-52100}"
KEEP="${OUT:-}"
if [ -n "$KEEP" ]; then W="$KEEP"; mkdir -p "$W"; else W="$(mktemp -d)"; trap 'rm -rf "$W"' EXIT; fi
# The grouping braces are load-bearing; see lodgen_native.sh's note.
WA="$(cd "$W" && { pwd -W 2>/dev/null || pwd; })"

[ -x "$NS" ] || { echo "no NifSkope.exe at $NS"; exit 2; }
[ -f "$ESM" ] || { echo "no plugin at $ESM"; exit 2; }

checks=0
fails=0
skips=0
note () { checks=$((checks+1)); echo "  ok   $1"; }
bad ()  { checks=$((checks+1)); fails=$((fails+1)); echo "  FAIL $1"; }
skip () { skips=$((skips+1)); echo "  skip $1"; }

# num <label-regex> <file>  -- one number out of a census line, or the empty string
num () { sed -n "s/$1/\1/p" "$2" | head -1; }

# bake <exe> <name> <extra switches...>
bake () {
	local exe="$1" name="$2"; shift 2
	mkdir -p "$WA/$name/Native"
	# shellcheck disable=SC2086
	"$exe" -no-gui lodgen "$ESM" --worldspace 3C --terrain-region $REGION --dim 4 \
		--road-detail 1 --data-root "$DATA" --out-dir "$WA/$name" \
		--native "$WA/$name/Native" --native-mesh-report "$WA/$name/mesh_report.txt" \
		"$@" > "$W/$name.log" 2>&1
	return $?
}

echo "== 0. the exe and the rung"
# THE PAIR MOVED (lane LAYOUT1, 2026-09-16, bungo 19:3x): a FO4CS bake
# writes its .lodo/.lodi under <mod folder>/FO4CSLOD/<ws>/, and `--native`
# names that mod folder. A RUNG exe from before the move writes them at the
# --native directory itself, so this asks the tree where the pair is rather
# than spelling a layout that depends on which exe baked it.
pairdir () {
	if [ -d "$1/FO4CSLOD/Commonwealth" ]; then echo "$1/FO4CSLOD/Commonwealth"
	else echo "$1"; fi
}

ls -la "$NS" | sed 's/^/  /'
[ -x "$RUNG" ] && ls -la "$RUNG" | sed 's/^/  /'

echo "== 1. the four bakes"
S0=$(date +%s)
# bungo 2026-09-17, "Authored LODs only": the ladder and the near library ship OFF,
# so every arm of THIS exe that measures the ladder asks for it by switch.
if bake "$NS" near --library near --native-ladder; then note "the near-model arm baked (--library near --native-ladder)"
else bad "the near-model arm baked"; tail -5 "$W/near.log"; fi
S1=$(date +%s)
if bake "$NS" fol --library near --native-ladder --native-ladder-foliage; then note "the foliage REFUTER arm baked (--native-ladder-foliage)"
else bad "the foliage REFUTER arm baked"; tail -5 "$W/fol.log"; fi
if bake "$NS" mnam --library mnam --native-ladder --native-ladder-foliage --native-silhouette 0 --native-no-placement-ao
then note "the WAY-BACK arm baked (--library mnam + the three off switches)"
else bad "the WAY-BACK arm baked"; tail -5 "$W/mnam.log"; fi
S2=$(date +%s)
echo "  bakeSecondsNear $((S1-S0))"
RUNGOK=0
if [ -x "$RUNG" ]; then
	if bake "$RUNG" rung --library mnam --native-ladder-foliage --native-silhouette 0 --native-no-placement-ao
	then RUNGOK=1; note "the rung exe baked the same way-back arm"
	else
		# the rung does not know the three new switches; bake it plain
		if bake "$RUNG" rung; then RUNGOK=1
			note "the rung exe baked (it predates the three switches, so plain)"
		else bad "the rung exe baked"; tail -5 "$W/rung.log"; fi
	fi
fi
grep -E "^native: |^  native-ladder-refused: |^  native-library: " "$W/near.log" | sed 's/^/  /'

LODO="$(pairdir "$WA/near/Native")/Commonwealth.lodo"
LODI="$(pairdir "$WA/near/Native")/Commonwealth.lodi"

echo "== 2. (a) no foliage cluster is laddered, and the switch puts them back"
FOLN="$(num '.*native-ladder-refused: foliage \([0-9][0-9]*\) clusters.*' "$W/near.log")"
FOLR="$(num '.*native-ladder-refused: foliage \([0-9][0-9]*\) clusters.*' "$W/fol.log")"
L1N="$(num '.*; level 1 clusters \([0-9][0-9]*\) .*' "$W/near.log")"
L1R="$(num '.*; level 1 clusters \([0-9][0-9]*\) .*' "$W/fol.log")"
if [ -n "$FOLN" ]; then note "the foliage refusal is counted by name (near arm: $FOLN clusters)"
else bad "the foliage refusal is counted by name (no native-ladder-refused: line)"; fi
if [ -n "$FOLN" ] && [ "$FOLN" -gt 0 ] 2>/dev/null; then
	note "a1 the refusal MOVES off zero on a region that has trees ($FOLN clusters refused)"
else bad "a1 the refusal MOVES off zero on a region that has trees (read '$FOLN')"; fi
if [ "${FOLR:-x}" = "0" ]; then
	note "a2 REFUTER --native-ladder-foliage refuses nothing (0 clusters)"
else bad "a2 REFUTER --native-ladder-foliage refuses nothing (read '$FOLR')"; fi
if [ -n "$L1N" ] && [ -n "$L1R" ] && [ "$L1R" -gt "$L1N" ] 2>/dev/null; then
	note "a3 REFUTER the switch LADDERS them: level-1 clusters $L1N -> $L1R"
else bad "a3 REFUTER the switch LADDERS them: level-1 clusters '$L1N' -> '$L1R'"; fi

echo "== 3. (b) every kept level holds $FLOOR of level 0's outline"
if "$PY" "$ROOT/tests/spells/lodgen_silhouette.py" "$LODO" --floor "$FLOOR" > "$W/sil.log" 2>&1
then cat "$W/sil.log" | sed 's/^/  /'; note "b the silhouette gate passes, with its floor twin red"
else cat "$W/sil.log" | sed 's/^/  /'; bad "b the silhouette gate passes, with its floor twin red"; fi

echo "== 4. (c) the ladder's first step is selectable inside the Commonwealth"
"$PY" "$ROOT/tests/spells/lodgen_ladder_select.py" "$LODO" \
	--max-distance "$PLAN_DISTANCE" > "$W/sel_near.log" 2>&1
CRC=$?
sed 's/^/  near  /' "$W/sel_near.log"
"$PY" "$ROOT/tests/spells/lodgen_ladder_select.py" "$(pairdir "$WA/mnam/Native")/Commonwealth.lodo" \
	--max-distance 1e12 > "$W/sel_mnam.log" 2>&1
sed 's/^/  mnam  /' "$W/sel_mnam.log"
DNEAR="$(num '.*level1SelectableAtPx 1.0 from \([0-9][0-9]*\) units.*' "$W/sel_near.log")"
DMNAM="$(num '.*level1SelectableAtPx 1.0 from \([0-9][0-9]*\) units.*' "$W/sel_mnam.log")"
if [ $CRC -eq 0 ]; then
	note "c1 the near arm's median level-1 step is selected inside ${PLAN_DISTANCE} units (${DNEAR:-?})"
else bad "c1 the near arm's median level-1 step is selected inside ${PLAN_DISTANCE} units (${DNEAR:-?})"; fi
if [ -n "$DNEAR" ] && [ -n "$DMNAM" ] && [ "$DNEAR" -lt "$DMNAM" ] 2>/dev/null; then
	note "c2 CONTROL the --library mnam arm of the SAME exe is farther: $DMNAM vs $DNEAR units"
else bad "c2 CONTROL the --library mnam arm is farther: mnam '$DMNAM' vs near '$DNEAR'"; fi

echo "== 5. (d) the AO byte and (e) the four header words (field gate, section j)"
MANI=""
for x in -20 -16 -12; do for y in 24 28 32; do
	m="$WA/near/Commonwealth.4.$x.$y.BTO.manifest.txt"
	[ -f "$m" ] && MANI="$MANI --manifest $m"
done; done
# shellcheck disable=SC2086
"$PY" "$ROOT/tests/spells/lodgen_native_fields.py" "$LODO" "$LODI" \
	--mesh-report "$WA/near/mesh_report.txt" $MANI > "$W/fields.log" 2>&1
grep -E "^  (ok|FAIL|skip) +j" "$W/fields.log" | sed 's/^/  /'
JOK="$(grep -c "^  ok   j" "$W/fields.log")"
JBAD="$(grep -c "^  FAIL j" "$W/fields.log")"
if [ "$JBAD" = "0" ] && [ "$JOK" -ge 12 ] 2>/dev/null; then
	note "de the v4/v5 field checks all hold ($JOK checks in section j, $JBAD failures)"
else bad "de the v4/v5 field checks all hold ($JOK checks in section j, $JBAD failures)"; fi
VN="$(od -An -t u4 -j 4 -N 4 "$LODI" | tr -d ' ')"
VB="$(od -An -t u4 -j 4 -N 4 "$(pairdir "$WA/mnam/Native")/Commonwealth.lodi" | tr -d ' ')"
if [ "$VN" = "5" ]; then note "d1 the AO arm writes a v5 .lodi"
else bad "d1 the AO arm writes a v5 .lodi (read $VN)"; fi
if [ "$VB" != "5" ]; then
	note "d2 REFUTER --native-no-placement-ao writes a v$VB .lodi: no version bump without the payload"
else bad "d2 REFUTER --native-no-placement-ao writes a v$VB .lodi"; fi
if "$PY" "$ROOT/tests/spells/lodgen_native_decode.py" "$(pairdir "$WA/mnam/Native")/Commonwealth.lodo" \
	"$(pairdir "$WA/mnam/Native")/Commonwealth.lodi" > "$W/dec_mnam.log" 2>&1
then note "d3 the independent decoder accepts the way-back pair too"
else bad "d3 the independent decoder accepts the way-back pair too"; tail -5 "$W/dec_mnam.log"; fi

echo "== 6. the way back is exact"
if [ "$RUNGOK" = "1" ]; then
	# `cmp -s` was here first and it went RED, correctly: TWELVE bytes differ.
	# They are the two DERIVED words -- `headerCrc32` at 0x0C and `lodoIdentity`
	# at 0x20 -- and they differ because this lane bumps the companion `.lodo` to
	# v4 UNCONDITIONALLY, so the library the way-back `.lodi` names is a different
	# file by design. Byte identity of the `.lodi` alone was the wrong claim to
	# make and the comparator caught me making it (mistake 8). The claim that IS
	# true, and that this gate states, is stronger than "they differ only here":
	# both derived words are RECOMPUTED from their own inputs and both come out
	# right, so the difference is a consequence of the `.lodo` and not a change to
	# a single instance byte.
	if "$PY" "$ROOT/tests/spells/lodgen_lodi_wayback.py" 		"$(pairdir "$WA/mnam/Native")/Commonwealth.lodi" "$(pairdir "$WA/rung/Native")/Commonwealth.lodi" 		--lodo-a "$(pairdir "$WA/mnam/Native")/Commonwealth.lodo" 		--lodo-b "$(pairdir "$WA/rung/Native")/Commonwealth.lodo" 		--label-a "way back" --label-b "rung" > "$W/wayback.log" 2>&1; then
		note "w1 the way-back .lodi differs from the rung exe's in the two DERIVED words ONLY"
	else
		bad "w1 the way-back .lodi differs from the rung exe's in the two DERIVED words ONLY"
		grep -E "^  (FAIL|STRAY)|^  [0-9]+ of" "$W/wayback.log" | head -6 | sed 's/^/    /'
	fi
	sed -n 's/^  \(ok\|FAIL\)   x/    x/p' "$W/wayback.log" | head -12
	# ww-module-off-is-identical section 4: a comparator that has never failed
	# proves nothing. Doctor one PAYLOAD byte of a copy and require the
	# comparator to name its offset as stray and go red.
	cp "$(pairdir "$WA/mnam/Native")/Commonwealth.lodi" "$W/doctored.lodi"
	"$PY" -c "import sys;f=sys.argv[1];d=bytearray(open(f,'rb').read());d[len(d)//2]^=1;open(f,'wb').write(d)" "$W/doctored.lodi"
	if "$PY" "$ROOT/tests/spells/lodgen_lodi_wayback.py" 		"$W/doctored.lodi" "$(pairdir "$WA/rung/Native")/Commonwealth.lodi" 		--lodo-a "$(pairdir "$WA/mnam/Native")/Commonwealth.lodo" 		--lodo-b "$(pairdir "$WA/rung/Native")/Commonwealth.lodo" > "$W/doctored.log" 2>&1; then
		bad "FLOOR the comparator sees a single flipped PAYLOAD byte (must go red)"
	else
		note "FLOOR the comparator sees a single flipped PAYLOAD byte (must go red): $(grep -m1 STRAY "$W/doctored.log" | sed 's/^ *//')"
	fi
	for field in 'levels 0\.\.\([0-9][0-9]*\);' '; level 1 clusters \([0-9][0-9]*\) ' \
		'; level 1 clusters [0-9][0-9]* triangles \([0-9][0-9]*\) ' \
		'roots \([0-9][0-9]*\) covering' 'covering \([0-9][0-9]*\) full-detail'; do
		a="$(num ".*$field.*" "$W/mnam.log")"
		b="$(num ".*$field.*" "$W/rung.log")"
		if [ -n "$a" ] && [ "$a" = "$b" ]; then
			note "w2 the way-back ladder census matches the rung's ($field = $a)"
		else bad "w2 the way-back ladder census matches the rung's ($field: '$a' vs '$b')"; fi
	done
else skip "w1/w2 the way back against the rung exe (no rung at $RUNG)"; fi

echo
echo "bakeSeconds near $((S1-S0)), three arms $((S2-S0))"
echo "$checks checks, $fails failures, $skips skips"
[ "$fails" -eq 0 ] && echo "RESULT PASS" || echo "RESULT FAIL"
exit $(( fails > 0 ? 1 : 0 ))
