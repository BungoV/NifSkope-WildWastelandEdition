#!/bin/bash
# Lane SHOWCASE1 -- the decoders and the attribution table.
set -u
L="E:/Projects/NifskopeWildWastelandEdition/scratchpad/showcase1_20260912"
R="/e/Projects/NifskopeWildWastelandEdition"
EXE="$L/ns_run/NifSkope.exe"
ESM="X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm"
LOG="$L/logs"

echo "=== 1. --native-verify on the region's .lodo/.lodi"
"$EXE" -no-gui lodgen "$ESM" --worldspace 3C \
	--native-verify "$L/out/on/native/Commonwealth.lodo" "$L/out/on/native/Commonwealth.lodi" \
	> "$LOG/native_verify.log" 2>&1
echo "rc=$?"; grep -vE "not found in archives" "$LOG/native_verify.log" | tail -6

echo; echo "=== 2. --lodl --verify-only"
"$EXE" -no-gui lodgen "$ESM" --worldspace 3C --lodl "$L/out/lodl" --verify-only \
	> "$LOG/lodl_verify.log" 2>&1
echo "rc=$?"; tail -5 "$LOG/lodl_verify.log"

echo; echo "=== 3. lodgen_vt_check.py header/tiles on the pyramid"
python "$R/tests/spells/lodgen_vt_check.py" header "$L/out/on/mod/Terrain/Commonwealth.VT.2.lodt" 2>&1 | tail -12
python "$R/tests/spells/lodgen_vt_check.py" tiles "$L/out/on/mod/Terrain/Commonwealth.VT.2.lodt" 2>&1 | tail -8
python "$R/tests/spells/lodgen_vt_check.py" header "$L/out/on/mod/Terrain/Commonwealth.VT.4.lodt" 2>&1 | tail -12
python "$R/tests/spells/lodgen_vt_check.py" tiles "$L/out/on/mod/Terrain/Commonwealth.VT.4.lodt" 2>&1 | tail -8

echo; echo "=== 4. heightmap vs the FO4CS installed reference"
V="E:/Projects/Fallout 4 Mods/mods/FO4CS/Textures/Terrain/Commonwealth/Commonwealth_fine.HeightMap.-96.-96.95.95.-8320.44872.dds"
O="$L/out/hm/Textures/Terrain/Commonwealth/Commonwealth.HeightMap.-96.-96.95.95.-8320.44872.dds"
ls -l "$V" "$O" 2>&1
if cmp "$V" "$O"; then echo "cmp: BYTE-IDENTICAL"; else echo "cmp: DIFFERS (see above)"; fi

echo; echo "=== 5. attribution: one chunk (-20,24), which files each switch moves"
cd "$L/out"
for f in one_on/tex/Commonwealth.4.-20.24.DDS one_on/tex/Commonwealth.4.-20.24_msn.DDS \
         one_on/tex/Commonwealth.4.-20.24_data.DDS one_on/obj/Commonwealth.4.-20.24.BTR \
         one_on/obj/Commonwealth.4.-20.24.BTO; do
	b="${f#one_on/}"
	printf '%-42s' "$b"
	for v in off noland noero noao nomsn; do
		if [ ! -f "one_$v/$b" ]; then printf ' %-8s' "absent"
		elif cmp -s "$f" "one_$v/$b"; then printf ' %-8s' "same"
		else printf ' %-8s' "DIFFERS"; fi
	done
	echo
done
echo "columns: off / noland(-LAND1) / noero(-erosion) / noao(-objectAO) / nomsn(-msn cache), each vs all-on"
