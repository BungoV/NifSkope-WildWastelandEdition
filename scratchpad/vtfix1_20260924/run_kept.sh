#!/bin/bash
# VTFIX1 kept-green sweep on the lane exe: one verdict line per gate into kept.log (full logs beside it).
cd /e/Projects/NifskopeWWE-vtfix1 || exit 2
L=scratchpad/vtfix1_20260924/kept
mkdir -p $L
EXE="$PWD/release/NifSkope.exe"; RUNG="$PWD/release/NifSkope.before_vtfix1.exe"
ESM="X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm"
S="$L/kept.log"
echo "kept sweep $(date '+%Y-%m-%d %H:%M:%S') exe $(sha1sum "$EXE" | cut -c1-8)" > $S
if tasklist | grep -qi Fallout4.exe; then echo "REFUSED: Fallout4 up" >> $S; exit 3; fi
for g in lodgen_terrain_vt lodgen_terrain lodgen_slab lodgen_terrain_pbrm lodgen_incremental lodgen_bakerec lodgen_defaults; do
	t0=$(date +%s)
	EXE="$EXE" RUNG="$RUNG" bash tests/spells/$g.sh > $L/$g.log 2>&1; rc=$?
	echo "$g rc=$rc $(( $(date +%s)-t0 ))s | $(grep -E -i "checks|RESULT|^PASS|^FAIL|failures" $L/$g.log | tail -2 | tr '\n' ' ')" >> $S
done
# .lodt write + ESM cross-check (the retired lodt_write.sh's core), then byte identity against the rung
for e in new rung; do
	X=$EXE; [ $e = rung ] && X=$RUNG
	rm -rf $L/lodt_$e; mkdir -p $L/lodt_$e
	"$X" -no-gui lodgen "$ESM" --worldspace 3C --lodt "$(cd $L/lodt_$e && pwd -W)" > $L/lodt_$e.log 2>&1
	echo "lodt write+cross-check [$e] rc=$?" >> $S
	rm -rf $L/hm_$e; mkdir -p $L/hm_$e
	"$X" -no-gui lodgen "$ESM" --worldspace 3C --heightmap "$(cd $L/hm_$e && pwd -W)" > $L/hm_$e.log 2>&1
	echo "heightmap [$e] rc=$?" >> $S
done
for f in $(cd $L/lodt_new && find . -type f); do cmp -s "$L/lodt_new/$f" "$L/lodt_rung/$f" && echo "lodt identical to rung: $f" >> $S || echo "lodt DIFFERS from rung: $f" >> $S; done
REF="E:/Projects/Fallout 4 Mods/mods/FO4CS/Textures/Terrain/Commonwealth/Commonwealth_fine.HeightMap.-96.-96.95.95.-8320.44872.dds"
for f in $(find $L/hm_new -name "*.dds"); do cmp -s "$f" "$REF" && echo "heightmap byte-identical to the shipped reference: $(basename $f)" >> $S || echo "heightmap vs reference DIFFERS: $(basename $f) $(stat -c %s $f)" >> $S; done
# GUI harness last, own port, second monitor via _harness.sh
PORT=42391 EXE="$EXE" bash tests/spells/lod_generation.sh > $L/lod_generation.log 2>&1
echo "lod_generation rc=$? | $(grep -E "checks|^PASS|^FAIL" $L/lod_generation.log | tail -2 | tr '\n' ' ')" >> $S
echo "sweep end $(date '+%H:%M:%S')" >> $S
