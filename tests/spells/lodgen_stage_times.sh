#!/bin/bash
#
# THE FOUR STAGE TIMES ON THE COMMAND LINE, and the trees-only rulings that
# ride with them (lane LODUI1, 2026-09-11).
#
# bungo asked for the four stage times of a bake (landscape / meshes /
# textures / impostors). The panel prints them in its result line and the CLI
# prints the same four, from the same formatter (lodgenStageTimeLine), so the
# two cannot word them differently. This spell proves, for each of the four,
# that it is WRITTEN and that it MOVES -- a run where it is above zero and a
# run where it is exactly 0.0 (the three rules of 2026-09-04 21:33).
#
# It also gates the two retirements of the same morning:
#
#   * `--candidates all` refuses BY NAME (bungo 07:0x, "I've only wanted trees
#     for the impostors");
#   * `--candidates trees` lists zero non-tree bases on Sanctuary while
#     `--candidates missing` lists a NAMED non-tree with an empty far slot --
#     both directions, so neither an empty list nor an unfiltered one passes.
#
# Everything is written under the repo's own scratchpad. No installed Data
# folder is touched and no worldspace-wide bake is run.
#
# USAGE
#   bash tests/spells/lodgen_stage_times.sh

set -u

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
NS="${EXE:-$ROOT/release/NifSkope.exe}"
ESM="${ESM:-X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm}"
W="${WORK:-$ROOT/scratchpad/lodui1_20260911/stage_times}"

[ -x "$NS" ] || { echo "no NifSkope.exe at $NS"; exit 2; }
[ -f "$ESM" ] || { echo "no Fallout4.esm at $ESM"; exit 2; }

winpath() {
	case "$1" in
		/[a-zA-Z]/*) local d="${1:1:1}"; printf '%s' "${d}:${1:2}" ;;
		*) printf '%s' "$1" ;;
	esac
}

rm -rf "$W"
mkdir -p "$W/region" "$W/hm" "$W/logs"

CHECKS=0
FAILS=0
ok()  { CHECKS=$((CHECKS+1)); echo "  ok   $1"; }
bad() { CHECKS=$((CHECKS+1)); FAILS=$((FAILS+1)); echo "  FAIL $1"; }
want() { if [ "$1" = "1" ]; then ok "$2"; else bad "$2"; fi; }

# the number after a stage's name on the "stage times:" line
stage() { sed -n 's/.*stage times: .*'"$2"' \([0-9.]*\) s.*/\1/p' "$1" | tail -1; }
# every stage, from one log
stages() {
	echo "landscape=$(stage "$1" landscape) meshes=$(stage "$1" meshes) textures=$(stage "$1" textures) impostors=$(stage "$1" impostors)"
}
gt0() { awk -v v="$1" 'BEGIN { exit !(v+0 > 0) }' && echo 1 || echo 0; }
iszero() { [ "$1" = "0.0" ] && echo 1 || echo 0; }

echo "== A. a region bake: meshes and textures move, landscape does not =="
"$NS" -no-gui lodgen "$ESM" --worldspace 3C --terrain-region -20 24 -20 24 --dim 4 \
	--out-dir "$(winpath "$W/region")" --tex-dir "$(winpath "$W/region/tex")" \
	--native "$(winpath "$W/region")" \
	> "$W/logs/region.log" 2>&1
RC=$?
echo "  rc=$RC"
echo "  $(stages "$W/logs/region.log")"
want "$( [ "$RC" = "0" ] && echo 1 || echo 0 )" "the region bake ran"
want "$( [ -n "$(stage "$W/logs/region.log" landscape)" ] && echo 1 || echo 0 )" \
	"the region bake prints the stage-times line"
want "$(gt0 "$(stage "$W/logs/region.log" meshes)")" "meshes is above zero"
want "$(gt0 "$(stage "$W/logs/region.log" textures)")" "textures is above zero"
want "$(iszero "$(stage "$W/logs/region.log" landscape)")" \
	"landscape is exactly 0.0 -- a region bake writes no landscape file"
want "$(iszero "$(stage "$W/logs/region.log" impostors)")" \
	"impostors is exactly 0.0 with no card library"
# THE FLOOR on the pair: the same run must have written it -- at the ONE ROOT
# the FO4CS target composes. Lane LAYOUT1 (2026-09-16) moved every FO4CS-target
# file under `<out>/FO4CSLOD/<ws>/`; this check still spelled the flat path and
# so went red on every exe after that ruling while the bake was writing the pair
# exactly where the ruling puts it (measured by AUDIT1, 2026-09-17: the same run
# left region/FO4CSLOD/Commonwealth/Commonwealth.lodo and .lodi on disk). The
# helper is the one tests/spells/lodgen_native.sh already uses.
pairdir () {
	if [ -d "$1/FO4CSLOD/Commonwealth" ]; then echo "$1/FO4CSLOD/Commonwealth"
	else echo "$1"; fi
}
haspair () {
	[ -s "$1/Commonwealth.lodo" ] && [ -s "$1/Commonwealth.lodi" ] && echo 1 || echo 0
}
PAIRD="$(pairdir "$W/region")"
echo "    pair directory: $PAIRD"
want "$(haspair "$PAIRD")" \
	"and the run wrote the .lodo/.lodi pair --native asked for"
# the census has to SAY that same root and say nothing landed outside it, so a
# bake that scattered FO4CS files past the root cannot pass this section
want "$(grep -qE "layout .*FO4CSLOD, [0-9]+ file\(s\), 0 outside" "$W/logs/region.log" && echo 1 || echo 0)" \
	"and the census names the one FO4CS root with 0 file(s) outside it"
# REFUTER, in the same run: the very same predicate must read 0 on a tree whose
# .lodo is missing, so this floor cannot pass on a build that writes no pair.
rm -rf "$W/pair_refute"; mkdir -p "$W/pair_refute"
cp "$PAIRD/Commonwealth.lodi" "$W/pair_refute/" 2>/dev/null
want "$( [ "$(haspair "$W/pair_refute")" = "0" ] && echo 1 || echo 0 )" \
	"refuter: the same test reads 0 on a tree whose .lodo is missing"

echo "== B. a heightmap bake: landscape moves, the other three do not =="
"$NS" -no-gui lodgen "$ESM" --worldspace 3C --heightmap "$(winpath "$W/hm")" \
	--heightmap-size 4096 > "$W/logs/hm.log" 2>&1
echo "  $(stages "$W/logs/hm.log")"
want "$(gt0 "$(stage "$W/logs/hm.log" landscape)")" "landscape is above zero"
want "$(iszero "$(stage "$W/logs/hm.log" meshes)")" "meshes falls back to exactly 0.0"
want "$(iszero "$(stage "$W/logs/hm.log" textures)")" "textures falls back to exactly 0.0"

echo "== C. --candidates all is retired and refuses BY NAME =="
"$NS" -no-gui lodgen "$ESM" --worldspace 3C --terrain-region -20 24 -17 27 \
	--list-impostor-candidates --candidates all > "$W/logs/cand_all.log" 2>&1
RCA=$?
cat "$W/logs/cand_all.log" | sed 's/^/    /'
want "$( [ "$RCA" != "0" ] && echo 1 || echo 0 )" "it refuses"
want "$( grep -qi "retired" "$W/logs/cand_all.log" && echo 1 || echo 0 )" \
	"and says why, in words"
# THE FLOOR: the two words that ARE offered still work
"$NS" -no-gui lodgen "$ESM" --worldspace 3C --terrain-region -20 24 -17 27 \
	--list-impostor-candidates --candidates trees 2>/dev/null | tr -d '\r' > "$W/trees.txt"
"$NS" -no-gui lodgen "$ESM" --worldspace 3C --terrain-region -20 24 -17 27 \
	--list-impostor-candidates --candidates missing 2>/dev/null | tr -d '\r' > "$W/missing.txt"
NT="$(grep -cE '^[0-9a-fA-F]{8} ' "$W/trees.txt" || true)"
NM="$(grep -cE '^[0-9a-fA-F]{8} ' "$W/missing.txt" || true)"
echo "  trees: $NT candidates, missing: $NM candidates"
want "$( [ "$NT" -ge 1 ] && [ "$NM" -ge 1 ] && echo 1 || echo 0 )" \
	"and both offered words still list candidates"

echo "== D. trees only: zero non-tree bases, and a named one present when it is off =="
# A base is a tree by the SAME three tests the bake uses: a TREE record, a
# model under a trees folder, or a model file named tree... The list prints the
# model, so the folder/name halves are readable here; a TREE record whose model
# is elsewhere would read as a false positive and is counted separately.
NONTREE="$(awk '{ $1=""; $2=""; m=tolower($0); if (m !~ /\\trees\\/ && m !~ /\/trees\// ) { n=split(m, p, /\\/); f=p[n]; sub(/^ +/, "", f); if (f !~ /^tree/) print }}' "$W/trees.txt" | wc -l)"
echo "  non-tree-looking models in the trees list: $NONTREE"
awk '{ $1=""; $2=""; m=tolower($0); if (m !~ /\\trees\\/ && m !~ /\/trees\// ) { n=split(m, p, /\\/); f=p[n]; sub(/^ +/, "", f); if (f !~ /^tree/) print "    " $0 }}' "$W/trees.txt"
want "$( [ "$NONTREE" = "0" ] && echo 1 || echo 0 )" \
	"every base the trees list names is a tree by folder or by name"
# THE FLOOR, both ways: a NAMED non-tree with an empty far slot, absent from the
# trees list and present in the missing list. Picked from the missing list
# itself so the name is the corpus's, not one typed from memory.
NAMED="$(awk '{ id=$1; $1=""; $2=""; m=tolower($0); if (m !~ /\\trees\\/ && m !~ /\/trees\// ) { n=split(m, p, /\\/); f=p[n]; sub(/^ +/, "", f); if (f !~ /^tree/) { print id; exit } } }' "$W/missing.txt")"
echo "  the named empty-slot non-tree: ${NAMED:-none found}"
if [ -n "$NAMED" ]; then
	grep -q "^$NAMED " "$W/missing.txt" && P=1 || P=0
	grep -q "^$NAMED " "$W/trees.txt" && A=0 || A=1
	want "$P" "it is present with Trees only OFF (--candidates missing)"
	want "$A" "and absent with Trees only ON (--candidates trees)"
else
	bad "a named empty-slot non-tree was found to test both ways"
	bad "(the floor could not be built, so neither direction is proved)"
fi

echo
echo "$CHECKS checks, $FAILS failures"
[ "$FAILS" = "0" ] && { echo "PASS"; exit 0; }
echo "FAIL"
exit 1
