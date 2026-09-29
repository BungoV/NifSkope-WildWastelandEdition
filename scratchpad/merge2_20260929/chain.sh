#!/bin/bash
# MERGE2 gate chain on the merged exe (run_m2). Each step takes and releases the NifSkope turn itself.
S=/e/Projects/NifskopeWWE-merge2/scratchpad/merge2_20260929
W=/e/Projects/NifskopeWWE-merge2
TL=/e/Projects/NifskopeWWE-terrlive1/scratchpad/terrlive1_20260929
TURN=/e/Projects/NifskopeWWE-fix1/scratchpad/fix1_20260926/turn.sh
cd $S; LW=$(pwd -W)
ts() { date +%H:%M:%S; }
if tasklist //FI "IMAGENAME eq Fallout4.exe" 2>/dev/null | grep -q Fallout4.exe; then echo "GAME UP"; exit 1; fi
echo "== 0 exec check + rule-check on TERRLIVE1's whole_rule .lodr $(ts)"
bash $TURN acquire MERGE2 || exit 1
for i in $(seq 1 45); do
	timeout 600 run_m2/NifSkope.exe -no-gui lodgen --rule-check "$(cygpath -am $TL/whole_rule/mod/FO4CSLOD/Commonwealth/Commonwealth.lodr)" > rc_tl.log 2>&1; rc=$?
	echo "exec try $i rc=$rc $(ts)"; [ $rc -ne 126 ] && break; sleep 60
done
bash $TURN release MERGE2
echo "== 1 LIGHT bake (IDENT2 gates) $(ts)"
WW_LODI_GROUP_DUMP=$LW/dump_m2.txt LIGHT=1 bash bake.sh run_m2 bakes/m_light
B=bakes/m_light/mod/FO4CSLOD/Commonwealth/Commonwealth
I=$W/scratchpad/ident2_20260929
python $I/pixgate.py $B dump_m2.txt > g_pix.out 2>&1; echo "pixgate rc=$?"
python $I/lmchunk.py $B dump_m2.txt > g_lmchunk.out 2>&1; echo "lmchunk rc=$?"
python $W/tests/spells/lodi_occluder_building.py $B --dump dump_m2.txt --json g_occ.json --gate > g_occ.out 2>&1; echo "poke rc=$?"
python $I/coverage.py $B --json g_cov.json > g_cov.out 2>&1; echo "coverage rc=$?"
echo "== 2 Boston full bakes, ways back: TERRLIVE1 head vs merged $(ts)"
bash bake.sh $TL/run_rule bakes/t_off --identity-join proximity --occluder-fit piece
WW_LODI_GROUPS_PER_CHUNK=1 bash bake.sh run_m2 bakes/m_off --identity-join proximity --occluder-fit piece
bash cmp_trees.sh bakes/t_off bakes/m_off off > g_cmp_off.out 2>&1; cat g_cmp_off.out
echo "== 3 whole-map VT-only bakes $(ts)"
bash wbake.sh $S/run_m2 $S/w_off
bash wbake.sh $S/run_m2 $S/w_rule --outside-paint rule
python $TL/edge/gate_off.py $TL/whole_off/mod w_off/mod > g_off_vs_head.out 2>&1; echo "gate_off head-vs-merged OFF rc=$?"
python $TL/edge/gate_off.py $TL/whole_rule/mod w_rule/mod > g_rule_vs_head.out 2>&1; echo "gate_off head-vs-merged RULE rc=$?"
python $TL/edge/gate_off.py w_off/mod w_rule/mod --expect-red > g_off_red.out 2>&1; echo "gate_off red half rc=$?"
python $TL/edge/gate_law2.py w_off/mod/FO4CSLOD/Commonwealth/Commonwealth.VT.8.lodt merged_off > g_law2_off.out 2>&1; echo "edge gate vanilla rc=$?"
python $TL/edge/gate_law2.py w_rule/mod/FO4CSLOD/Commonwealth/Commonwealth.VT.8.lodt merged_rule --rule > g_law2_rule.out 2>&1; echo "edge gate rule rc=$?"
echo "== 4 decal-check + rule-check $(ts)"
bash $TURN acquire MERGE2 || exit 1
timeout 900 run_m2/NifSkope.exe -no-gui lodgen --decal-check "$(cygpath -am w_off/mod/FO4CSLOD/Commonwealth)" > g_decal.out 2>&1; echo "decal-check rc=$?"
timeout 600 run_m2/NifSkope.exe -no-gui lodgen --rule-check "$(cygpath -am w_rule/mod/FO4CSLOD/Commonwealth/Commonwealth.lodr)" > g_rulecheck.out 2>&1; echo "rule-check rc=$?"
bash $TURN release MERGE2
echo "== 5 lodgen_native.sh $(ts)"
bash $TURN acquire MERGE2 || exit 1
cd $W
EXE=$S/run_m2/NifSkope.exe OUT=$S/h_native bash tests/spells/lodgen_native.sh > $S/h_lodgen_native.log 2>&1; echo "lodgen_native rc=$?"
bash $TURN release MERGE2
echo "CHAIN DONE $(ts)"
