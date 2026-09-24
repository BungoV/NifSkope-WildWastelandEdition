#!/bin/bash
#
# A DEFAULT-SETTINGS SANCTUARY PAIR, AND THE CENSUS CHECKER ON IT
# (lane INCRGATE1, 2026-09-24; docs/FO4CS_IMPROVED_LOD_PLAN.md section 5 rows 26 and 25).
#
# Row 26: "no default-settings Sanctuary pair on disk". This spell bakes one: the
# nine-chunk Sanctuary region (-20,24)..(-9,35) at dim 4 with the FO4CS target and
# NO other switch -- `--native` into the output folder itself, the way the panel
# does -- into a scratch folder that stays out of git (default
# scratchpad/sanctuary_pair_work, whose binaries .gitignore already drops; OUT=
# moves it). The pair is left on disk for R0 gate 1. Gated:
#   (26a) the bake exits 0 and leaves FO4CSLOD/Commonwealth/Commonwealth.lodo/.lodi
#   (26b) the version words read .lodo 4 and .lodi 7 from the bytes
#   (26c) the independent decoder reads the pair (rc 0), and its instanceCount
#         equals the placement rows of the bake's own manifests
#   RED (26r) the same decoder on a copy of the .lodi cut 4,096 bytes short
#         must REFUSE -- or (26c) could not fail on a broken file
#
# Row 25: "the census checker's 59 / 0 / 31 has not been re-run on a v4 pair".
#   (25a) tests/spells/lodgen_census_check.py on this pair with the bake's own log,
#         --self-floor: rc 0, and the ok / RED / not-derivable split printed
#   RED (25r) one claim doctored by hand (instanceCount + 1) must be caught:
#         the checker prints "FLOOR ok" for a doctored run only when it went red
#   RED (25r2) ladderGroup doctored to 4 must be caught: the checker's ladder-OFF
#         rule (added by this lane; the default pair has no ladder, and the census
#         line prints the unused target `group 4` while the file holds 0) must
#         not have turned the word into one that cannot fail
# FIRST RUN (b2, 2026-09-24 21:45 exe): the pair is .lodo 4 6,204,388 B / .lodi 7
#   527,989 B, 3,526 placements in 10 chunks, 0 occluders, 0 cell-line ambiguities;
#   the checker read 38 ok / 0 RED / 32 not-derivable (the v3 figure was 59/0/31)
#   after the ladder-OFF fix; 37 ok / 1 RED (ladderGroup 4 vs 0) / 31 before it.
#
# USAGE
#   bash tests/spells/lodgen_sanctuary_pair.sh
#   EXE=... ESM=... DATA=... OUT=<abs dir> bash tests/spells/lodgen_sanctuary_pair.sh

set -u
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
NS="${EXE:-$ROOT/release/NifSkope.exe}"
ESM="${ESM:-X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm}"
DATA="${DATA:-E:/Tools/Fallout 4/DataUnpacked/Data}"
PY="${PY:-$(command -v python || echo /c/Windows/py)}"
OUT="${OUT:-$ROOT/scratchpad/sanctuary_pair_work}"
fails=0
checks=0
ok() { checks=$((checks + 1)); echo "  ok   $1"; }
bad() { checks=$((checks + 1)); fails=$((fails + 1)); echo "  FAIL $1"; }

[ -x "$NS" ] || { echo "no NifSkope.exe at $NS"; exit 2; }
[ -f "$ESM" ] || { echo "no plugin at $ESM"; exit 2; }
if tasklist //FI "IMAGENAME eq Fallout4.exe" 2>/dev/null | grep -q Fallout4.exe; then
	echo "GAME UP: Fallout4.exe is running; no bake"; exit 3
fi
rm -rf "$OUT"; mkdir -p "$OUT"
OW="$(cd "$OUT" && { pwd -W 2>/dev/null || pwd; })"
PD="$OW/FO4CSLOD/Commonwealth"

echo "== 26. the default-settings Sanctuary bake into $OW"
t0=$(date +%s)
"$NS" -no-gui lodgen "$ESM" --worldspace 3C --terrain-region -20 24 -9 35 --dim 4 \
	--data-root "$DATA" --out-dir "$OW" --native "$OW" > "$OUT/bake.log" 2>&1; rc=$?
echo "  bake rc $rc, wall-clock $(( $(date +%s) - t0 )) s"
[ $rc -eq 0 ] && [ -s "$PD/Commonwealth.lodo" ] && [ -s "$PD/Commonwealth.lodi" ] \
	&& ok "(26a) the bake left FO4CSLOD/Commonwealth/Commonwealth.lodo and .lodi" \
	|| { bad "(26a) the bake left no pair (rc $rc): $(tail -2 "$OUT/bake.log")"; echo "RESULT FAIL"; exit 1; }

VER="$("$PY" -c "import struct,sys;print(*[struct.unpack_from('<I',open(p,'rb').read(8),4)[0] for p in sys.argv[1:]])" \
	"$PD/Commonwealth.lodo" "$PD/Commonwealth.lodi")"
echo "  version words: .lodo ${VER% *}, .lodi ${VER#* }"
[ "$VER" = "4 7" ] && ok "(26b) the pair is .lodo 4 / .lodi 7" || bad "(26b) the pair is not .lodo 4 / .lodi 7 (read '$VER')"

MANS=()
while IFS= read -r m; do MANS+=( --manifest "$m" ); done < <(find "$OW" -name "*.manifest.txt" | sort)
ROWS="$(cat $(find "$OW" -name "*.manifest.txt") 2>/dev/null | grep -cE '^[0-9]+ ')"
"$PY" "$ROOT/tests/spells/lodgen_native_decode.py" "$PD/Commonwealth.lodo" "$PD/Commonwealth.lodi" \
	"${MANS[@]}" > "$OUT/decode.txt" 2>&1; rc=$?
grep -E "^lodo\.(fileBytes|baseCount|clusterCount)|^lodi\.(fileBytes|instanceCount|presentChunks|occluderCount|cellQuantAmbiguous)|checks, " "$OUT/decode.txt" | sed 's/^/    /'
IC="$(sed -n 's/^lodi.instanceCount \([0-9]*\).*/\1/p' "$OUT/decode.txt")"
[ $rc -eq 0 ] && [ -n "$IC" ] && [ "$IC" = "$ROWS" ] \
	&& ok "(26c) the decoder reads the pair; instanceCount $IC = the $((${#MANS[@]} / 2)) manifests' $ROWS rows" \
	|| bad "(26c) decoder rc $rc, instanceCount ${IC:-?} against $ROWS manifest rows"

"$PY" -c "import sys;b=open(sys.argv[1],'rb').read();open(sys.argv[2],'wb').write(b[:-4096])" \
	"$PD/Commonwealth.lodi" "$OW/cut.lodi.x"
"$PY" "$ROOT/tests/spells/lodgen_native_decode.py" "$PD/Commonwealth.lodo" "$OW/cut.lodi.x" > "$OUT/decode_red.txt" 2>&1; rc=$?
rm -f "$OUT/cut.lodi.x"
[ $rc -ne 0 ] && grep -q "REFUSED" "$OUT/decode_red.txt" \
	&& ok "(26r red control) a .lodi cut 4,096 bytes short is refused: $(grep -m1 REFUSED "$OUT/decode_red.txt" | cut -c1-120)" \
	|| bad "(26r red control) the decoder read a truncated .lodi (rc $rc)"

echo "== 25. the census checker on this v4 pair"
"$PY" "$ROOT/tests/spells/lodgen_census_check.py" "$OW" --census "$OW/bake.log" --self-floor > "$OUT/census.txt" 2>&1; rc=$?
grep -E "checks, .*failures|FLOOR|caught|skip " "$OUT/census.txt" | sed 's/^/    /'
[ $rc -eq 0 ] && grep -q "^FLOOR ok" "$OUT/census.txt" \
	&& ok "(25a) the census checker is green on a .lodo 4 / .lodi 7 pair, its own floor caught" \
	|| bad "(25a) the census checker on the v4 pair: rc $rc"
"$PY" "$ROOT/tests/spells/lodgen_census_check.py" "$OW" --census "$OW/bake.log" --quiet \
	--doctor "lodi.instanceCount=$(( ${IC:-0} + 1 ))" > "$OUT/census_red.txt" 2>&1; rc=$?
[ $rc -eq 0 ] && grep -q "^FLOOR ok" "$OUT/census_red.txt" \
	&& ok "(25r red control) instanceCount doctored to $(( ${IC:-0} + 1 )) is caught" \
	|| bad "(25r red control) a doctored instanceCount was not caught (rc $rc): $(tail -1 "$OUT/census_red.txt")"
# the ladder-OFF rule this lane added to the checker (the file carries ladderGroup 0)
# must still see a file that says otherwise
"$PY" "$ROOT/tests/spells/lodgen_census_check.py" "$OW" --census "$OW/bake.log" --quiet \
	--doctor "lodo.ladderGroup=4" > "$OUT/census_red2.txt" 2>&1; rc=$?
[ $rc -eq 0 ] && grep -q "^FLOOR ok" "$OUT/census_red2.txt" \
	&& ok "(25r2 red control) ladderGroup claimed 4 on a ladder-OFF pair is caught" \
	|| bad "(25r2 red control) ladderGroup 4 on a ladder-OFF pair was not caught (rc $rc)"

echo
echo "  pair kept in $PD ($(stat -c %s "$PD/Commonwealth.lodo") + $(stat -c %s "$PD/Commonwealth.lodi") bytes)"
echo "  exe $NS $(date -r "$NS" +%Y-%m-%dT%H:%M:%S)"
echo "$checks checks, $fails failures"
[ $fails -eq 0 ] && echo "RESULT PASS" || echo "RESULT FAIL"
exit $([ $fails -eq 0 ] && echo 0 || echo 1)
