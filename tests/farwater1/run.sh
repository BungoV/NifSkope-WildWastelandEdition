#!/bin/bash
# Lane FARWATER1 (part A): a LOD water re-bake keeps the user's marks. No window: `-no-gui` lodgen + lodl only.
#
#   1. bake the Commonwealth .lodl (landscape stage only, ~15 s)                            -> base (unmarked)
#   2. mark it as a user would: `lodl <f> --water-mark-fixture` (a stroke, names, a colour, a still lake, a dye
#      mouth, the dye knob, a class by hand), keep a copy                                    -> marked (the reference)
#   3. GREEN  re-bake over the marked file: marks, names, hand-set fields AND the whole file byte-equal to the
#             reference (the planes re-derive from the carried strokes); no .premarks / .bak-watermark left
#   4. RED    the marked file back, re-bake with WW_LODL_MARK_CARRY_RED=drop (today's writer): MUST FAIL
#   5. REMAP  smaller bodies marked (WW_WATER_MARK_FIXTURE_RANK=$RANK), re-bake with --water-bridge 0 (the classifier
#             renumbers them): every name, hand-set field and stroke follows its body, and the stroke's points lie
#             on its body in the NEW plane (read by lodl_marks.py's own decoder); floor: at least one id changed
#   5b. RED   the same with WW_LODL_MARK_CARRY_RED=byid (the old ids kept): MUST FAIL
#   6. floor  base != reference (the marks move the planes, so a byte-equal re-bake is not "nothing happened")
#   7. part B: grazing_fresnel.py, the far water's reflectance at the rings' grazing angles (a twin, asserted on the GLSL)
# usage: bash tests/farwater1/run.sh [exe] [out dir]      (Fallout4.exe must not run; ESM= overrides the master)
set -u
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
EXE="${1:-$ROOT/release/NifSkope.exe}"
OUT="${2:-$ROOT/tests/farwater1/out}"
ESM="${ESM:-X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm}"
PY="${PY:-python}"
RANK="${RANK:-6}"   # the re-map leg marks the RANK-th river/lake by area (small bodies renumber with --water-bridge 0)
HERE="$(cd "$(dirname "$0")" && pwd)"
unset WW_LODL_MARK_CARRY_RED
tasklist 2>/dev/null | grep -qi fallout4.exe && { echo "Fallout4.exe is running: no bake"; exit 3; }
[ -x "$EXE" ] || { echo "no exe at $EXE"; exit 2; }
mkdir -p "$OUT"
W="$(cygpath -m "$OUT" 2>/dev/null || echo "$OUT")"
F="$W/bake/FO4CSLOD/Commonwealth/Commonwealth.lodl"
fails=0
bake() {   # bake <tag> [env...] -- [lodgen args...]
	local tag=$1; shift
	local envs=()
	while [ $# -gt 0 ] && [ "$1" != -- ]; do envs+=("$1"); shift; done
	[ $# -gt 0 ] && shift
	local s=$(date +%s)
	env "${envs[@]}" "$EXE" -no-gui lodgen "$ESM" --worldspace 3C --lodl "$W/bake" "$@" < /dev/null > "$OUT/$tag.log" 2>&1
	local rc=$?
	echo "[$tag] rc=$rc $(( $(date +%s) - s )) s; $(grep -a 'water marks' "$OUT/$tag.log" | head -1)"
	[ $rc -eq 0 ] || { echo "FAIL bake $tag rc=$rc"; fails=$((fails+1)); }
}
judge() {   # judge <mode> <tag> <must: pass|fail>   (REF= the reference, default the marked file)
	"$PY" -I "$HERE/lodl_marks.py" "$1" "${REF:-$OUT/marked.lodl}" "$F" < /dev/null > "$OUT/judge_$2.txt" 2>&1
	local rc=$?
	grep -a '^PASS\|^FAIL\|^info' "$OUT/judge_$2.txt" | sed "s/^/  [$2] /"
	if { [ "$3" = pass ] && [ $rc -eq 0 ]; } || { [ "$3" = fail ] && [ $rc -ne 0 ]; }; then
		echo "OK   $2: lodl_marks $1 rc=$rc (must $3)"
	else
		echo "FAIL $2: lodl_marks $1 rc=$rc (must $3)"; fails=$((fails+1))
	fi
}
leftovers() {
	local n; n=$(ls "$W/bake/FO4CSLOD/Commonwealth/" | grep -c 'premarks\|bak-watermark\|tmp-watermark')
	if [ "$n" = 0 ]; then echo "OK   $1: no .premarks/.bak-watermark/.tmp-watermark left"
	else echo "FAIL $1: $n leftover file(s): $(ls "$W/bake/FO4CSLOD/Commonwealth/" | tr '\n' ' ')"; fails=$((fails+1)); fi
}
rm -rf "$OUT/bake"
echo "== 1. base bake"
bake base -- ; cp "$F" "$OUT/base.lodl"
echo "== 2. mark it"
"$EXE" -no-gui lodl "$F" --water-mark-fixture < /dev/null > "$OUT/fixture.log" 2>&1
frc=$?; echo "  fixture rc=$frc $(head -c 400 "$OUT/fixture.log")"
[ $frc -eq 0 ] || { echo "FAIL fixture: the marked file did not save and re-open"; fails=$((fails+1)); }
"$EXE" -no-gui lodl "$F" --water-census < /dev/null > "$OUT/census_marked.log" 2>&1
crc=$?; echo "  the marked file opens with the reader: rc=$crc"
[ $crc -eq 0 ] || { echo "FAIL the marked file will not open: $(head -c 300 "$OUT/census_marked.log")"; fails=$((fails+1)); }
rm -f "$F.bak-watermark"; cp "$F" "$OUT/marked.lodl"
"$PY" -I "$HERE/lodl_marks.py" dump "$OUT/marked.lodl" < /dev/null | sed 's/^/  /'
echo "== 3. GREEN re-bake"
bake green -- ; judge equal green pass; leftovers green
echo "== 4. RED re-bake (WW_LODL_MARK_CARRY_RED=drop)"
cp "$OUT/marked.lodl" "$F"; bake red WW_LODL_MARK_CARRY_RED=drop -- ; judge equal red fail
echo "== 5. REMAP: smaller bodies marked (rank $RANK), re-bake with --water-bridge 0"
cp "$OUT/base.lodl" "$F"
WW_WATER_MARK_FIXTURE_RANK=$RANK "$EXE" -no-gui lodl "$F" --water-mark-fixture < /dev/null > "$OUT/fixture_rank.log" 2>&1
echo "  fixture rank $RANK rc=$? $(head -c 300 "$OUT/fixture_rank.log")"
rm -f "$F.bak-watermark"; cp "$F" "$OUT/marked_rank.lodl"
bake remap -- --water-bridge 0
"$PY" -I "$HERE/lodl_marks.py" dump "$F" < /dev/null | head -1 | sed 's/^/  /'
REF="$OUT/marked_rank.lodl" judge remap remap pass; leftovers remap
moved=$(sed -n 's/^info: \([0-9]*\) stroke(s) changed body id/\1/p' "$OUT/judge_remap.txt")
if [ "${moved:-0}" -gt 0 ]; then echo "OK   remap floor: $moved stroke(s) changed body id (the map was needed)"
else echo "FAIL remap floor: no stroke changed body id, the leg proves no re-map (pick another RANK)"; fails=$((fails+1)); fi
echo "== 5b. REMAP RED (WW_LODL_MARK_CARRY_RED=byid: the old ids kept)"
cp "$OUT/marked_rank.lodl" "$F"; bake remapred WW_LODL_MARK_CARRY_RED=byid -- --water-bridge 0
REF="$OUT/marked_rank.lodl" judge remap remapred fail
echo "== 6. floor"
b=$(sha1sum < "$OUT/base.lodl" | cut -c1-12); m=$(sha1sum < "$OUT/marked.lodl" | cut -c1-12)
if [ "$b" != "$m" ]; then echo "OK   floor: base $b != marked $m"; else echo "FAIL floor: base == marked"; fails=$((fails+1)); fi
echo "== 7. part B: the far water's reflectance at grazing angles"
"$PY" -I "$HERE/grazing_fresnel.py" "$ROOT" < /dev/null | sed 's/^/  /'
[ "${PIPESTATUS[0]}" -eq 0 ] || { echo "FAIL grazing_fresnel: the twin no longer matches the GLSL"; fails=$((fails+1)); }
echo
[ $fails -eq 0 ] && { echo "farwater1 marks PASS"; exit 0; }
echo "farwater1 marks FAIL ($fails)"; exit 1
