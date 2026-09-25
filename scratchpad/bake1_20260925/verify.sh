#!/bin/bash
# Step 6: verify the whole-map bake in mods\FO4CSLOD with the readers that are not the writer.
# Every section prints a verdict line; the full outputs land in verify/.
L=/e/Projects/NifskopeWWE-bake1/scratchpad/bake1_20260925
S=/e/Projects/NifskopeWWE-bake1/tests/spells
M="E:/Projects/Fallout 4 Mods/mods/FO4CSLOD"
W="$M/FO4CSLOD/Commonwealth"
NS="$L/run/release/NifSkope.exe"
V=$L/verify; mkdir -p $V
echo "== stages"; cat $L/full/logs/stages.txt
echo "== census lines"
grep -aE "^(native|native-cards|bake census|bake-record|card arrays written|arrays written|stage times|vt:)" $L/full/logs/chunks.log | cut -c1-260
grep -ac "Qt Concurrent" $L/full/logs/chunks.log | sed 's/^/Qt Concurrent lines: /'
echo "== native-verify"
"$NS" -no-gui lodgen --mo2-profile "E:/Projects/Fallout 4 Mods/profiles/Default" --worldspace 3C \
	--native-verify "$W/Commonwealth.lodo" "$W/Commonwealth.lodi" > $V/native_verify.txt 2>&1; echo "rc=$?"
grep -aiE "pass|fail|RESULT|cardCount|verdict" $V/native_verify.txt | head -12
echo "== census check (--self-floor)"
python $S/lodgen_census_check.py "$W" --census $L/full/logs/chunks.log --self-floor > $V/census_check.txt 2>&1; echo "rc=$?"
tail -4 $V/census_check.txt
echo "== lodb"
python $S/lodb_read.py "$W/Commonwealth.lodb" > $V/lodb.txt 2>&1; echo "rc=$?"
echo "== lodm"
python $S/lodgen_lodm_check.py "$W" > $V/lodm.txt 2>&1; echo "rc=$?"; tail -3 $V/lodm.txt
echo "== lodl info"
python $S/lodl_open_authority.py "$W/Commonwealth.lodl" info > $V/lodl_info.txt 2>&1; echo "rc=$?"; head -6 $V/lodl_info.txt
echo "== lodt-check per VT level"
for d in 32 16 8 4 2; do
	"$NS" -no-gui lodgen --lodt-check "$W/Commonwealth.VT.$d.lodt" > $V/lodt_check_$d.txt 2>&1
	echo "VT.$d rc=$? $(tail -1 $V/lodt_check_$d.txt | cut -c1-200)"
done
echo "== files"
( cd "$M" && find . -type f -printf '%s %p\n' | sort -k2 ) > $V/files.txt
echo "files $(wc -l < $V/files.txt), bytes $(awk '{s+=$1} END {printf "%d", s}' $V/files.txt)"
awk '{n=$2; sub(/.*\./,"",n); c[n]++; s[n]+=$1} END {for (k in c) printf "  .%s %d files %.2f GB\n", k, c[k], s[k]/1e9}' $V/files.txt | sort
