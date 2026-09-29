#!/bin/bash
# IDENT2 job 3, after bakes4: the C++ reader on the v13 refusal files and the old v12 file, lodgen_native.sh, the pictures.
H=/e/Projects/NifskopeWWE-ident2/scratchpad/ident2_20260929; cd $H
TURN=/e/Projects/NifskopeWWE-fix1/scratchpad/fix1_20260926/turn.sh
ESM="X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm"
NS=$H/run_v13/NifSkope.exe
if tasklist //FI "IMAGENAME eq Fallout4.exe" 2>/dev/null | grep -q Fallout4.exe; then echo "GAME UP"; exit 1; fi
bash $TURN acquire IDENT2 7200 || exit 1
echo "== native-verify $(date +%H:%M:%S)"
LODO=$H/b_v13/mod/FO4CSLOD/Commonwealth/Commonwealth.lodo
for f in mut_v13/*.lodi b_v13/mod/FO4CSLOD/Commonwealth/Commonwealth.lodi; do
  "$NS" -no-gui lodgen "$ESM" --worldspace 3C --native-verify "$LODO" "$H/$f" > nv.tmp 2>&1; echo "$f rc=$? $(grep -m1 -E 'REFUSED|error' nv.tmp | cut -c1-200)"
done
"$NS" -no-gui lodgen "$ESM" --worldspace 3C --native-verify "$H/b_fp3/mod/FO4CSLOD/Commonwealth/Commonwealth.lodo" "$H/b_fp3/mod/FO4CSLOD/Commonwealth/Commonwealth.lodi" > nv.tmp 2>&1; echo "b_fp3 v12 rc=$? $(grep -m1 -E 'REFUSED|error' nv.tmp | cut -c1-200)"
rm -f nv.tmp
echo "== lodgen_native.sh $(date +%H:%M:%S)"
cd /e/Projects/NifskopeWWE-ident2
EXE=$PWD/release/NifSkope.exe OUT=$H/h_native_v13 bash tests/spells/lodgen_native.sh > $H/h_lodgen_native_v13.log 2>&1; echo "lodgen_native rc=$?"
bash $TURN release IDENT2
cd $H
echo "== pics $(date +%H:%M:%S)"
bash pics_v13.sh
echo "== post done $(date +%H:%M:%S)"
