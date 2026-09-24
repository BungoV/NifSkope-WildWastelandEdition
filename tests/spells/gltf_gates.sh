#!/bin/bash
# The glTF export gate set (lane HKX4b, 2026-09-10). Contract:
# docs/GLTF_INTERCHANGE.md. Nothing here needs release/NifSkope.exe -- it all
# runs through the standalone release/gltfexport_dump.exe, which links
# src/gltfexport.cpp with no NifModel, no Scene and no GL (skill
# ww-standalone-writer-gate).
#
# Run inside MSYS2 UCRT64 (Qt6Core.dll is on its PATH, Git-Bash's has it not):
#   MSYSTEM=UCRT64 CHERE_INVOKING=1 /c/msys64/usr/bin/bash -lc \
#     'cd /e/Projects/NifskopeWildWastelandEdition && bash tests/spells/gltf_gates.sh'
#
# G1  the exports are written                 (three fixtures)
# G2  gltf_check.py structural validator      0 failures on each
# G2f the FLOOR under G2                      12 mutants, every one refused
# G3  gltf_readback.py against the sources    0 failures on the vanilla donor;
#                                             2 pre-registered on the assembled
#                                             fixture (LLeg_Toe1, section 6 of
#                                             the contract)
# G3f the FLOOR under G3                      7 sabotages, every one red
# G4  Blender's own importer, headless        every mesh back, by name
#
# Exit 0 when every gate is as registered.
set -u
cd /e/Projects/NifskopeWildWastelandEdition || exit 9

OUT=scratchpad/hkx4_20260910/out
MUT=scratchpad/hkx4_20260910/mutants
CLIPS=scratchpad/hkx1_20260910/clips
SKEL="$CLIPS/skeleton.hkx"
FIX=fixtures/human_male_vanilla.nif
MIXAMO=fixtures/Running_To_Slide_And_Back_To_Running.hkx
VANILLA="E:/Tools/Fallout 4/DataUnpacked/Data/meshes/actors/character/characterassets/MaleBody.nif"
BLENDER="E:/Tools/3D/Blender 4.5/blender.exe"
DUMP=release/gltfexport_dump.exe

fails=0
note () { echo; echo "=== $* ==="; }
verdict () { # <label> <expected failures> <actual failures>
	if [ "$2" = "$3" ]; then echo "  PASS  $1  ($3 failures, $2 registered)"
	else echo "  FAIL  $1  ($3 failures, $2 registered)"; fails=$((fails + 1)); fi
}

if [ ! -x "$DUMP" ]; then
	echo "no $DUMP -- run: bash scratchpad/hkx4_20260910/build_dump.sh"
	exit 9
fi
mkdir -p "$OUT"

note "G1 the exports"
"$DUMP" --skeleton "$FIX" --mesh "$FIX" --clip "$CLIPS/jog.hkx" --bones "$SKEL" \
	--name JogForward --out "$OUT/human_male_jog.gltf" > "$OUT/g1_jog.log" 2>&1
echo "  jog     rc=$?  $(tail -1 "$OUT/g1_jog.log")"
"$DUMP" --skeleton "$FIX" --mesh "$FIX" --clip "$MIXAMO" --bones "$SKEL" \
	--name RunningToSlide --root-motion --out "$OUT/human_male_mixamo.gltf" > "$OUT/g1_mixamo.log" 2>&1
echo "  mixamo  rc=$?  $(tail -1 "$OUT/g1_mixamo.log")"
if [ -f "$VANILLA" ]; then
	"$DUMP" --skeleton "$VANILLA" --mesh "$VANILLA" --out "$OUT/vanilla_malebody.gltf" > "$OUT/g1_vanilla.log" 2>&1
	echo "  vanilla rc=$?  $(tail -1 "$OUT/g1_vanilla.log")"
else
	echo "  vanilla SKIPPED: $VANILLA is not on this machine"
fi

note "G2 gltf_check.py"
for f in "$OUT"/human_male_jog.gltf "$OUT"/human_male_mixamo.gltf "$OUT"/vanilla_malebody.gltf; do
	[ -f "$f" ] || continue
	line=$(python tests/spells/gltf_check.py "$f" 2>&1 | grep -E "^[0-9]+ checks")
	n=$(echo "$line" | sed -E 's/.* ([0-9]+) failures.*/\1/')
	echo "  $(basename "$f"): $line"
	verdict "G2 $(basename "$f")" 0 "$n"
done

note "G2f the floor under G2: every mutant must be refused"
rm -rf "$MUT"
python tests/spells/gltf_sabotage.py "$OUT/human_male_jog.gltf" "$MUT" > "$OUT/g2f_make.log" 2>&1
made=0; refused=0
for f in "$MUT"/*.gltf; do
	[ -f "$f" ] || continue
	made=$((made + 1))
	python tests/spells/gltf_check.py "$f" > /dev/null 2>&1 || refused=$((refused + 1))
done
echo "  $made mutants, $refused refused"
verdict "G2f every mutant refused" "$made" "$refused"

note "G3 gltf_readback.py against the NIF and the clip"
line=$(python tests/spells/gltf_readback.py "$OUT/human_male_jog.gltf" --nif "$FIX" \
	--clip "$CLIPS/jog.hkx" --bones "$SKEL" 2>&1 | tee "$OUT/g3_jog.log" | grep -E "^[0-9]+ checks")
echo "  jog:    $line"
verdict "G3 jog (2 registered: LLeg_Toe1)" 2 "$(echo "$line" | sed -E 's/.* ([0-9]+) failures.*/\1/')"
line=$(python tests/spells/gltf_readback.py "$OUT/human_male_mixamo.gltf" --nif "$FIX" \
	--clip "$MIXAMO" --bones "$SKEL" 2>&1 | tee "$OUT/g3_mixamo.log" | grep -E "^[0-9]+ checks")
echo "  mixamo: $line"
verdict "G3 mixamo (2 registered: LLeg_Toe1)" 2 "$(echo "$line" | sed -E 's/.* ([0-9]+) failures.*/\1/')"
if [ -f "$OUT/vanilla_malebody.gltf" ]; then
	line=$(python tests/spells/gltf_readback.py "$OUT/vanilla_malebody.gltf" --nif "$VANILLA" \
		2>&1 | tee "$OUT/g3_vanilla.log" | grep -E "^[0-9]+ checks")
	echo "  vanilla control: $line"
	verdict "G3 vanilla donor (the control)" 0 "$(echo "$line" | sed -E 's/.* ([0-9]+) failures.*/\1/')"
fi

note "G3f the floor under G3: every sabotage must go red"
red=0; tried=0
for k in ibm-transpose joint-shift quat-conjugate frame-shift unit-scale up-axis node-translate; do
	tried=$((tried + 1))
	n=$(python tests/spells/gltf_readback.py "$OUT/human_male_jog.gltf" --nif "$FIX" \
		--clip "$CLIPS/jog.hkx" --bones "$SKEL" --sabotage "$k" 2>&1 \
		| grep -E "^[0-9]+ checks" | sed -E 's/.* ([0-9]+) failures.*/\1/')
	# 2 is the pre-registered LLeg_Toe1 pair; a sabotage must add to it
	if [ "${n:-0}" -gt 2 ]; then red=$((red + 1)); fi
	printf "  %-16s %s failures\n" "$k" "$n"
done
verdict "G3f every sabotage red" "$tried" "$red"

note "G4 Blender's own glTF importer, headless"
if [ -x "$BLENDER" ]; then
	for f in "$OUT"/human_male_jog.gltf "$OUT"/human_male_mixamo.gltf "$OUT"/vanilla_malebody.gltf; do
		[ -f "$f" ] || continue
		"$BLENDER" --background --factory-startup --python tests/spells/gltf_blender_check.py \
			-- "$f" > "$OUT/g4_$(basename "$f" .gltf).log" 2>&1
		rc=$?
		grep -E "^BLENDER (armature|meshes_matched|action|result)" "$OUT/g4_$(basename "$f" .gltf).log" | sed 's/^/    /'
		verdict "G4 $(basename "$f")" 0 "$rc"
	done
else
	echo "  PENDING: Blender is not at $BLENDER on this machine -- gate (d) is owed to bungo"
fi

echo
echo "==== $fails gate(s) not as registered ===="
exit $([ "$fails" = 0 ] && echo 0 || echo 1)
