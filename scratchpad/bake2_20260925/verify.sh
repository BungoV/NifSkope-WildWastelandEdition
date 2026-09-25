#!/bin/bash
# BAKE2 verify: BAKE1's verify.sh (G1) generalised to any worldspace and any folder (scratch before install,
# the installed folder after), plus G2 (.lodb plugins), G3 (card bases) and the placement/grey checks.
# usage: verify.sh <lod dir (…/FO4CSLOD/<EDID>)> <EDID> <ws load-order id> <chunks.log> <land.bin> <outdir>
D=$(cygpath -am "$1"); E="$2"; WS="$3"; CL=$(cygpath -am "$4"); LB=$(cygpath -am "$5"); V="$6"   # absolute: the exe does not resolve a relative path
L=/e/Projects/NifskopeWWE-bake2/scratchpad/bake2_20260925
S=/e/Projects/NifskopeWWE-bake2/tests/spells
NS="$L/run/release/NifSkope.exe"
mkdir -p "$V"
echo "== census lines"
grep -aE "^(native|native-cards|bake census|card arrays written|arrays written|stage times|vt:)" "$CL" | cut -c1-200
echo "== native-verify"
"$NS" -no-gui lodgen --mo2-profile "E:/Projects/Fallout 4 Mods/profiles/Default" --worldspace "$WS" \
	--native-verify "$D/$E.lodo" "$D/$E.lodi" > "$V/native_verify.txt" 2>&1; echo "rc=$?"
grep -aiE "pass|fail|RESULT|verdict" "$V/native_verify.txt" | head -8 | cut -c1-200
echo "== census check (--self-floor)"
python $S/lodgen_census_check.py "$D" --census "$CL" --self-floor > "$V/census_check.txt" 2>&1; echo "rc=$?"
tail -3 "$V/census_check.txt" | cut -c1-200
echo "== lodb (G2)"
python $S/lodb_read.py "$D/$E.lodb" > "$V/lodb.txt" 2>&1; echo "rc=$?"
python -c "import json,sys; d=json.load(open(sys.argv[1])); p=d['plugins']; print('G2 lodb plugins', len(p), 'first', p[0].get('name', p[0]) if isinstance(p[0], dict) else p[0])" "$V/lodb.txt" 2>&1 | cut -c1-200
echo "== lodm"
python $S/lodgen_lodm_check.py "$D" > "$V/lodm.txt" 2>&1; echo "rc=$?"; tail -2 "$V/lodm.txt" | cut -c1-200
echo "== lodl info"
python $S/lodl_open_authority.py "$D/$E.lodl" info > "$V/lodl_info.txt" 2>&1; echo "rc=$?"; head -4 "$V/lodl_info.txt" | cut -c1-200
echo "== lodt-check per VT level"
for f in "$D"/$E.VT.*.lodt; do
	b=$(basename "$f"); "$NS" -no-gui lodgen --lodt-check "$f" > "$V/lodt_check_$b.txt" 2>&1
	echo "$b rc=$? $(tail -1 "$V/lodt_check_$b.txt" | cut -c1-160)"
done
echo "== G3 cards"
python $L/g3_cards.py "$D/$E.lodo" 2>&1 | cut -c1-240
echo "== placements / grey"
python $L/checks.py "$D" "$E" "$LB" 2>&1 | head -30
echo "== files"
( cd "$D" && find . -type f -printf '%s %p\n' | sort -k2 ) > "$V/files.txt"
echo "files $(wc -l < "$V/files.txt"), bytes $(awk '{s+=$1} END {printf "%d", s}' "$V/files.txt")"
awk '{n=$2; sub(/.*\./,"",n); c[n]++; s[n]+=$1} END {for (k in c) printf "  .%s %d files %.1f MB\n", k, c[k], s[k]/1e6}' "$V/files.txt" | sort
