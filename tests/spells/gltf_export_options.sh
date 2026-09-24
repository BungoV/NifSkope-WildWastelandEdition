#!/bin/bash
# THE glTF EXPORT OPTIONS GATE (lane GLTFEXPORT1, 2026-09-19).
#
# bungo, 2026-09-19 04:xx, verbatim: "when we export gltf, we need a few toggles
# in there before the export. One would be to only export actual bones on the
# mesh for the visible character."
#
# There is ONE options struct (src/gltfexportopts.h). The dialog's rows and the
# CLI's flags both fill it, which is why this gate can measure the flags and
# speak about the dialog: a row that meant something different from its flag
# would be a second struct, and there is only one. The DIALOG's own gate is
# separate and in-application: WW_GLTF_EXPORT_DIALOG, src/gltfexportdialogtest.cpp.
#
# THE ROW THAT MATTERS MOST IS R0. bungo has not ruled the defaults, so every
# RE-BASED 2026-09-19 09:45. bungo ruled the defaults Blender-ready (skeleton
# auto, joints whole, units game, animation on, bones body, textures copy), so
# R0 now proves byte identity through `--legacy-defaults` -- the way back --
# and R14 measures what a person gets with no flag at all. Every other row
# starts from --legacy-defaults so that it measures ONE option, not six.
#
# The paragraph below is how it stood before the ruling and is kept as the
# record of why the defaults were where they were:
# option ships at the value that reproduces TODAY's output -- and R0 proves
# that by exporting the same NIF through the RUNG exe (release/
# NifSkope.before_gltfexport1.exe, the build this lane started from) and through
# the new one and comparing the bytes. The only licensed difference is the
# buffer's own file name, which is the output path the operator chose, so both
# files have that name normalised out before the compare and the .bin -- where
# every number lives -- is compared raw.
#
# ROWS
#   R0  byte identity            today's flags == the rung exe, byte for byte
#   R1  --skeleton PATH          skeleton_nodes 0 -> 129
#   R2  --joints whole           one joint list, shared by every skin
#   R3  --units game             the SAME bone is 64/0.9144 longer
#   R4  --part A --part B        parts_merged, skins, shapes
#   R5  --bones-only body        helpers_dropped == the measured 15
#   R6  --textures copy          the .dds is beside the .gltf and the URI is relative
#   R7  --root-motion-mode       strip / root / object, read off the Root channel
#   R8  the clip                 tracks driven, with and without the skeleton
#   R9  RED CONTROL              a WRONG skeleton must NAME the bones it lacks
#   R10 the structural validator tests/spells/gltf_check.py on every output
#   R11 HEADLESS BLENDER         one armature, zero loose empties, a vertex moves
#   R12 the round trip           gltf -> gltf-import -> hkx == the clip we began
#                                with, in BOTH unit modes
#   R13 SCALE tracks             the body build cycle is written as non-uniform
#                                scale channels and read back
#   R14 THE RULED DEFAULTS       no option flag at all gives one armature, zero
#                                empties, game units, the clip, 117 nodes, a
#                                copied .dds
#   R15 a STATIC                 `auto` finding nothing falls back and says so,
#                                never fails, and never rigs a clock
#
# NO KHRONOS glTF-VALIDATOR ON THIS MACHINE. Searched 2026-09-19: no
# gltf-validator / gltf_validator on PATH and none under E:/Tools. R10 is our
# own structural validator (tests/spells/gltf_check.py, 12 registered mutants in
# tests/spells/gltf_gates.sh prove it can fail) and R11 is Blender's own
# importer, which is Khronos's reference Python implementation. If the validator
# is ever installed, add it as R10b -- it is not a substitute for R11.
#
# EVERY PATH GIVEN TO THE EXE IS ABSOLUTE AND WINDOWS-SHAPED.
# Run inside MSYS2 UCRT64 (Qt6Core.dll is on its PATH, Git-Bash's is not):
#   MSYSTEM=UCRT64 CHERE_INVOKING=1 /c/msys64/usr/bin/bash -lc \
#     'cd /e/Projects/NifskopeWildWastelandEdition && bash tests/spells/gltf_export_options.sh'
set -u
cd /e/Projects/NifskopeWildWastelandEdition || exit 9

R=E:/Projects/NifskopeWildWastelandEdition
NS="${EXE:-./release/NifSkope.exe}"
OLD="${OLDEXE:-./release/NifSkope.before_gltfexport1.exe}"
CA="${CA:-E:/Tools/Fallout 4/DataUnpacked/Data/Meshes/Actors/Character/CharacterAssets}"
DATA="${DATA:-E:/Tools/Fallout 4/DataUnpacked/Data}"
CLIPS="${CLIPS:-$R/scratchpad/hkx1_20260910/clips}"
OUT="${OUT:-$R/scratchpad/gltfexport1_20260919/gate}"
BLENDER="${BLENDER:-E:/Tools/3D/Blender 4.5/blender.exe}"
# The RACE Bone Scale Data comes out of the SHIPPED Fallout4.esm, which is in
# the game install and not in the unpacked corpus.
ESMROOT="${ESMROOT:-X:/Programs/Steam/steamapps/common/Fallout 4/Data}"
BODY="$CA/MaleBody.nif"
SKEL="$CA/skeleton.nif"
HANDS="$CA/MaleHands.nif"
HEAD="$CA/BaseMaleHead.nif"
CLIP="$CLIPS/jog.hkx"
HKXSKEL="$CLIPS/skeleton.hkx"
# R15's fixture: something that is NOT a character and has no CharacterAssets
# anywhere near it, so `--skeleton auto` must find nothing and fall back.
STATIC="${STATIC:-$DATA/Meshes/SetDressing/AlarmClock/AlarmClock.nif}"

fails=0
note () { echo; echo "=== $* ==="; }
ok ()   { echo "  PASS  $*"; }
bad ()  { echo "  FAIL  $*"; fails=$((fails + 1)); }
eq ()   { # <label> <want> <got>
	if [ "$2" = "$3" ]; then ok "$1  ($3)"; else bad "$1  (want $2, got $3)"; fi
}
m ()    { # <logfile> <key>  -- one value off the exporter's MEASURED line
	grep -m1 "MEASURED" "$1" | tr ' ' '\n' | grep -m1 "^$2=" | cut -d= -f2
}
g ()    { # <gltf> <key> [extra args] -- one value out of the written file
	python tests/spells/gltf_measure.py "$1" "${@:3}" | grep -m1 "^M $2 " | cut -d' ' -f3-
}
# TWO INSTRUMENTS, AND THE DIFFERENCE MATTERS.
#
# `run` starts a row from the PRE-RULING baseline: it passes `--legacy-defaults`
# first, so a row that names one flag measures THAT FLAG and nothing else.
# bungo's ruling of 2026-09-19 09:45 moved six defaults at once; without this,
# "R5 --bones-only body drops 15 helpers" would be comparing two exports that
# also differ in skeleton, joints, units and textures, and would be measuring
# their sum. Flags after `--legacy-defaults` win, which is why it goes first.
#
# `runNew` passes nothing: it is exactly what a person gets from the shipped
# defaults. R14 is the row that measures that, and it is the only row allowed
# to be silent about its flags.
run () { # <logname> <nif> <args...>
	local log="$OUT/$1.log"; shift
	local nif="$1"; shift
	"$NS" -no-gui gltf "$nif" --legacy-defaults "$@" > "$log" 2>&1
	echo "$log"
}
runNew () { # <logname> <args...>
	local log="$OUT/$1.log"; shift
	"$NS" -no-gui gltf "$@" > "$log" 2>&1
	echo "$log"
}

[ -x "$NS" ] || { echo "no NifSkope.exe at $NS"; exit 9; }
[ -f "$BODY" ] || { echo "no vanilla body at $BODY -- this gate needs the unpacked corpus"; exit 9; }
[ -f "$SKEL" ] || { echo "no skeleton at $SKEL"; exit 9; }
rm -rf "$OUT"; mkdir -p "$OUT" "$OUT/tex"

# --------------------------------------------------------------------------
# RE-BASED 2026-09-19 09:45. Until the ruling this row ran with NO flag, and a
# no-flag export was the old export. The defaults are Blender-ready now, so the
# proof moves onto `--legacy-defaults`: the old bytes are still reachable and
# still exactly reproduced, one flag away. The claim is weaker in reach and
# exactly as strong in kind, and R14 is the row that measures the new default.
note "R0 byte identity: --legacy-defaults == the exe this lane started from"
if [ -x "$OLD" ]; then
	run r0_new "$BODY" -o "$OUT/r0_new.gltf" > /dev/null
	"$OLD" -no-gui gltf "$BODY" -o "$OUT/r0_old.gltf" > "$OUT/r0_old.log" 2>&1
	if [ -f "$OUT/r0_new.gltf" ] && [ -f "$OUT/r0_old.gltf" ]; then
		sed 's/r0_new/X/g' "$OUT/r0_new.gltf" > "$OUT/r0_new.norm"
		sed 's/r0_old/X/g' "$OUT/r0_old.gltf" > "$OUT/r0_old.norm"
		if cmp -s "$OUT/r0_new.norm" "$OUT/r0_old.norm"; then ok "the .gltf is identical (buffer name normalised)"
		else bad "the .gltf differs from the rung exe's -- a default moved"; fi
		if cmp -s "$OUT/r0_new.bin" "$OUT/r0_old.bin"; then ok "the .bin is byte-identical, raw"
		else bad "the .bin differs from the rung exe's"; fi
		# THE FLOOR under R0: the same compare must be able to fail. One flag
		# that is NOT today's value, and the files must stop matching.
		run r0_floor "$BODY" --units game -o "$OUT/r0_floor.gltf" > /dev/null
		if cmp -s "$OUT/r0_floor.bin" "$OUT/r0_old.bin"; then
			bad "FLOOR: --units game produced the same bytes, so this compare proves nothing"
		else ok "FLOOR: one non-default flag does change the bytes"; fi
	else
		bad "one of the two exports was not written"
	fi
else
	bad "no rung exe at $OLD -- R0 cannot run, and a missing row is never a pass"
fi

# --------------------------------------------------------------------------
note "R1 --skeleton: the hierarchy the body has none of"
L=$(run r1_none "$BODY" -o "$OUT/r1_none.gltf")
L2=$(run r1_skel "$BODY" --skeleton "$SKEL" -o "$OUT/r1_skel.gltf")
eq "skeleton none contributes no nodes" "0" "$(m "$L" skeleton_nodes)"
eq "skeleton PATH contributes 129 NiNode" "129" "$(m "$L2" skeleton_nodes)"
eq "58 of the body's 59 bones were already in it" "58" "$(m "$L2" matched_by_name)"
echo "  nodes in the file: none $(g "$OUT/r1_none.gltf" nodes) -> skeleton $(g "$OUT/r1_skel.gltf" nodes)"

# --------------------------------------------------------------------------
note "R2 --joints whole: ONE joint list, so Blender builds ONE armature"
L=$(run r2_whole "$BODY" --skeleton "$SKEL" --joints whole --part "$HANDS" -o "$OUT/r2_whole.gltf")
L3=$(run r2_weighted "$BODY" --skeleton "$SKEL" --part "$HANDS" -o "$OUT/r2_weighted.gltf")
eq "every skin shares one list" "yes" "$(g "$OUT/r2_whole.gltf" joints_identical)"
echo "  weighted-only lists: $(g "$OUT/r2_weighted.gltf" joints_lists)   whole: $(g "$OUT/r2_whole.gltf" joints_lists)"
W=$(g "$OUT/r2_whole.gltf" joints_max); V=$(g "$OUT/r2_weighted.gltf" joints_max)
if [ "${W:-0}" -gt "${V:-0}" ]; then ok "the whole list ($W) is larger than the weighted one ($V)"
else bad "the whole list is not larger than the weighted one ($W vs $V)"; fi
echo "  skins_unified=$(m "$L" skins_unified) inverse_binds_derived=$(m "$L" inverse_binds_derived) (weighted run: $(m "$L3" skins_unified))"

# --------------------------------------------------------------------------
note "R3 --units game: the same bone, measured, 64/0.9144 longer"
BONE="${BONE:-LLeg_Calf_skin}"
run r3_m "$BODY" --skeleton "$SKEL" --joints whole -o "$OUT/r3_m.gltf" > /dev/null
run r3_g "$BODY" --skeleton "$SKEL" --joints whole --units game -o "$OUT/r3_g.gltf" > /dev/null
AM=$(g "$OUT/r3_m.gltf" "bone_${BONE}_len" --bone "$BONE")
AG=$(g "$OUT/r3_g.gltf" "bone_${BONE}_len" --bone "$BONE")
echo "  $BONE: metres $AM   game units $AG"
python - "$AM" "$AG" <<'PY'
import sys
want = 64.0 / 0.9144                      # GLTF_UNITS_PER_METRE, src/gltfexportopts.h
try:
    a, b = float(sys.argv[1]), float(sys.argv[2])
except ValueError:
    print("  FAIL  R3 the bone was not found in one of the files"); sys.exit(1)
if a <= 0:
    print("  FAIL  R3 the bone has zero length in the metre file, so the ratio says nothing")
    sys.exit(1)
r = b / a
print("  ratio %.6f, expected %.6f, error %.3g" % (r, want, abs(r - want)))
sys.exit(0 if abs(r - want) < 1e-3 else 1)
PY
if [ $? = 0 ]; then ok "the units row is the measured ratio"; else bad "the units ratio is not 64/0.9144"; fi
eq "the metre file declares its scale to a reader" "0.0142875" "$(m "$(ls "$OUT"/r3_m.log)" metres_per_unit)"
eq "the game-unit file declares scale 1" "1" "$(m "$(ls "$OUT"/r3_g.log)" metres_per_unit)"
echo "  asset.extras.metresPerUnit: metres $(g "$OUT/r3_m.gltf" asset_metres_per_unit)  game $(g "$OUT/r3_g.gltf" asset_metres_per_unit)"

# --------------------------------------------------------------------------
note "R4 --part: several NIFs onto one skeleton, in one file"
if [ -f "$HANDS" ] && [ -f "$HEAD" ]; then
	L=$(run r4 "$BODY" --skeleton "$SKEL" --joints whole --part "$HANDS" --part "$HEAD" -o "$OUT/r4.gltf")
	eq "two extra NIFs merged" "2" "$(m "$L" parts_merged)"
	eq "three skinned shapes, one per part" "3" "$(m "$L" skinned)"
	eq "and still one joint list" "yes" "$(g "$OUT/r4.gltf" joints_identical)"
else
	bad "R4 SKIPPED: $HANDS / $HEAD not on this machine (a SKIP is never a pass)"
fi

# --------------------------------------------------------------------------
note "R5 --bones-only body: the helper rule, counted"
L=$(run r5_all "$BODY" --skeleton "$SKEL" --joints whole --bones-only all -o "$OUT/r5_all.gltf")
L2=$(run r5_body "$BODY" --skeleton "$SKEL" --joints whole --bones-only body -o "$OUT/r5_body.gltf")
eq "all: nothing dropped" "0" "$(m "$L" helpers_dropped)"
eq "body: the 15 measured helpers dropped" "15" "$(m "$L2" helpers_dropped)"
echo "  nodes: all $(g "$OUT/r5_all.gltf" nodes) -> body $(g "$OUT/r5_body.gltf" nodes)"

# --------------------------------------------------------------------------
note "R6 --textures copy: the .dds beside the file, referenced relatively"
L=$(run r6 "$BODY" --textures copy --data-root "$DATA" -o "$OUT/tex/r6.gltf")
NC=$(m "$L" textures_copied); NM=$(m "$L" textures_missing)
echo "  copied=$NC missing=$NM"
if [ "${NC:-0}" -ge 1 ]; then ok "at least one texture was copied"; else bad "nothing was copied"; fi
DDS=$(ls "$OUT/tex"/*.dds 2>/dev/null | wc -l)
eq "the .dds files are on disk beside the .gltf" "$NC" "$DDS"
if python tests/spells/gltf_measure.py "$OUT/tex/r6.gltf" | grep -q "^M images 0"; then
	echo "  (this body's material reaches no image node; the copy is still on disk)"
fi
python - "$OUT/tex/r6.gltf" <<'PY'
import json, os, sys
d = json.load(open(sys.argv[1], encoding="utf-8"))
bad = [i.get("uri", "") for i in d.get("images", [])
       if i.get("uri") and (os.path.isabs(i["uri"]) or i["uri"][1:3] == ":/" or "\\" in i["uri"])]
print("  image URIs: %s" % ([i.get("uri") for i in d.get("images", [])] or "none"))
if bad:
    print("  FAIL  R6 an image URI is absolute: %s" % bad); sys.exit(1)
sys.exit(0)
PY
if [ $? = 0 ]; then ok "no image URI is absolute"; else bad "an image URI is absolute"; fi

# --------------------------------------------------------------------------
note "R7 --root-motion-mode: read off the Root channel, not off the log"
if [ -f "$CLIP" ] && [ -f "$HKXSKEL" ]; then
	for mode in strip root object; do
		run "r7_$mode" "$BODY" --skeleton "$SKEL" --joints whole --clip "$CLIP" \
			--bones "$HKXSKEL" --root-motion-mode "$mode" -o "$OUT/r7_$mode.gltf" > /dev/null
		S=$(g "$OUT/r7_$mode.gltf" anim_Root_translation_max --anim-node Root)
		echo "  $mode: Root travels $S"
		eval "SPAN_$mode=\$S"
	done
	python - "${SPAN_strip:-}" "${SPAN_root:-}" "${SPAN_object:-}" <<'PY'
import sys
s, r, o = sys.argv[1], sys.argv[2], sys.argv[3]
def f(x):
    try: return float(x)
    except ValueError: return None
fs, fr, fo = f(s), f(r), f(o)
bad = []
if fs is None: bad.append("strip wrote no Root channel to read (%s)" % s)
elif fs > 1e-4: bad.append("strip left %.6f of travel on Root" % fs)
if fr is None: bad.append("root wrote no Root channel (%s)" % r)
elif fr < 1e-3: bad.append("root kept only %.6f of travel -- nothing to see" % fr)
if fo is not None and fo > 1e-4: bad.append("object left %.6f on the BONE; it belongs on the object" % fo)
for b in bad: print("  FAIL  R7 " + b)
sys.exit(1 if bad else 0)
PY
	if [ $? = 0 ]; then ok "strip is still, root travels, object leaves the bone still"; else bad "the root-motion modes are not distinct"; fi
	grep -h "baked onto the object node" "$OUT/r7_object.log" | sed 's/^/  /'
else
	bad "R7 SKIPPED: no clip at $CLIP (a SKIP is never a pass)"
fi

# --------------------------------------------------------------------------
note "R8 the clip the viewer is playing, and how many tracks it drives"
if [ -f "$CLIP" ] && [ -f "$HKXSKEL" ]; then
	L=$(run r8_none "$BODY" --clip "$CLIP" --bones "$HKXSKEL" -o "$OUT/r8_none.gltf")
	L2=$(run r8_skel "$BODY" --skeleton "$SKEL" --joints whole --clip "$CLIP" --bones "$HKXSKEL" -o "$OUT/r8_skel.gltf")
	A=$(m "$L" tracks_matched); B=$(m "$L2" tracks_matched)
	echo "  tracks driven: body alone $A of 95   body + skeleton.nif $B of 95"
	eq "today's number, unchanged" "13" "$A"
	# 78 AND NOT 95. The 17 that are still unmatched are WeaponBolt, WeaponTrigger,
	# WeaponMagazine*, WeaponOptics*, WeaponIKTarget* -- bones that live in
	# skeleton.hkx and in a WEAPON's nif, and in NO character skeleton.nif. A
	# character with no weapon has no node for them and inventing one would be
	# inventing a transform. So 78 is the ceiling, and it is a data fact.
	eq "with the skeleton: every character track" "78" "$B"
	eq "the 17 that are left are the weapon's" "17" "$(m "$L2" tracks_unmatched)"
	eq "the file carries an animation" "1" "$(g "$OUT/r8_skel.gltf" animations)"
	echo "  scale channels in the clip export: $(g "$OUT/r8_skel.gltf" scale_channels)"
else
	bad "R8 SKIPPED: no clip at $CLIP"
fi

# --------------------------------------------------------------------------
note "R9 RED CONTROL: the WRONG skeleton must name the bones it does not carry"
L=$(run r9_right "$BODY" --skeleton "$SKEL" --joints whole -o "$OUT/r9_right.gltf")
L2=$(run r9_wrong "$BODY" --skeleton "$HEAD" --joints whole -o "$OUT/r9_wrong.gltf")
eq "the RIGHT skeleton leaves nothing unnamed" "0" "$(m "$L" unmatched_bones)"
N=$(m "$L2" unmatched_bones)
if [ "${N:-0}" -ge 40 ]; then ok "the WRONG skeleton names $N bones it lacks"
else bad "the wrong skeleton named only ${N:-0} bones -- the control does not discriminate"; fi
grep -h "did not carry" "$OUT/r9_wrong.log" | cut -c1-200 | sed 's/^/  /'

# --------------------------------------------------------------------------
note "R10 the structural validator (tests/spells/gltf_check.py)"
for f in "$OUT"/r1_skel.gltf "$OUT"/r2_whole.gltf "$OUT"/r3_g.gltf "$OUT"/r4.gltf \
		 "$OUT"/r5_body.gltf "$OUT"/r8_skel.gltf; do
	[ -f "$f" ] || continue
	line=$(python tests/spells/gltf_check.py "$f" 2>&1 | grep -E "^[0-9]+ checks")
	n=$(echo "$line" | sed -E 's/.* ([0-9]+) failures.*/\1/')
	eq "$(basename "$f")" "0" "${n:-?}"
done
echo "  (no Khronos glTF-Validator is installed on this machine; see the header)"

# --------------------------------------------------------------------------
note "R11 HEADLESS BLENDER: one armature, zero loose empties, a vertex moves"
if [ -x "$BLENDER" ]; then
	"$BLENDER" --background --factory-startup --python tests/spells/gltf_char_blender.py \
		-- "$(cygpath -w "$OUT/r8_skel.gltf" 2>/dev/null || echo "$OUT/r8_skel.gltf")" \
		--expect-armatures 1 --max-empties 0 --frame-n 20 --min-move 0.001 \
		> "$OUT/r11_whole.log" 2>&1
	rc=$?
	grep -E "^BLENDER |^FAIL" "$OUT/r11_whole.log" | sed 's/^/    /'
	if [ $rc = 0 ]; then ok "R11 the character arrives as ONE armature and deforms"
	else bad "R11 Blender's own importer refused the shape of the export"; fi

	# THE FLOOR under R11: the SAME assertion run on the weighted-only export,
	# which is what today does. It must FAIL -- that is the whole of what the
	# `joints whole` option was asked for, and a row that passes both ways is
	# measuring nothing.
	"$BLENDER" --background --factory-startup --python tests/spells/gltf_char_blender.py \
		-- "$(cygpath -w "$OUT/r2_weighted.gltf" 2>/dev/null || echo "$OUT/r2_weighted.gltf")" \
		--expect-armatures 1 --max-empties 0 \
		> "$OUT/r11_floor.log" 2>&1
	frc=$?
	grep -E "^BLENDER (objects|armature)|^FAIL" "$OUT/r11_floor.log" | sed 's/^/    /'
	if [ $frc != 0 ]; then ok "FLOOR: the weighted-only export does NOT satisfy the same assertion"
	else bad "FLOOR: today's export passes the one-armature test too, so R11 proves nothing"; fi
else
	bad "R11 SKIPPED: Blender is not at $BLENDER (a SKIP is never a pass)"
fi

# --------------------------------------------------------------------------
# FINDING (9). The exported clip is read back with `gltf-import`, written as a
# .hkx, and both ends are dumped with the exe's own `hkx-tsv` so that the glTF
# in between is the only variable. Game units AND metres, because the arm that
# makes the round trip work is `asset.extras.metresPerUnit` and a file in one
# unit must not come back in the other.
#
# 78 of the original's 95 bones come back: the 17 weapon bones have no node in
# a character nif (see R8) and are named by the comparison, not swallowed.
note "R12 the round trip: gltf -> gltf-import -> hkx, against the clip we started from"
if [ -f "$CLIP" ] && [ -f "$HKXSKEL" ]; then
	"$NS" -no-gui hkx-tsv "$CLIP" -o "$OUT/r12_orig.tsv" > "$OUT/r12_orig.log" 2>&1
	for u in game metres; do
		uf=""; [ "$u" = game ] && uf="--units game"
		run "r12_$u" "$BODY" --skeleton "$SKEL" --joints whole $uf \
			--clip "$CLIP" --bones "$HKXSKEL" -o "$OUT/r12_$u.gltf" > /dev/null
		"$NS" -no-gui gltf-import "$OUT/r12_$u.gltf" -o "$OUT/r12_$u.hkx" \
			--bones "$HKXSKEL" --tsv "$OUT/r12_$u.tsv" > "$OUT/r12_${u}_import.log" 2>&1
		grep -o "units: [^;]*" "$OUT/r12_${u}_import.log" | sed 's/^/    /'
		if [ -f "$OUT/r12_$u.tsv" ]; then
			python tests/spells/gltf_roundtrip_cmp.py "$OUT/r12_orig.tsv" "$OUT/r12_$u.tsv" \
				> "$OUT/r12_$u.cmp" 2>&1
			rc=$?
			grep -E "^RT (orig_bones|round_bones|bones_only_in_orig|worst_translation|worst_rotation|result)" \
				"$OUT/r12_$u.cmp" | sed 's/^/    /'
			if [ $rc = 0 ]; then ok "R12 $u: the clip survives the round trip"
			else bad "R12 $u: the round trip changed the clip"; fi
		else
			bad "R12 $u: gltf-import wrote no tsv"
		fi
	done

	# THE FLOOR under R12: take the GAME-UNIT file and delete the one field the
	# importer reads the units from. The default (metres) is then applied to a
	# file that is already in game units and the clip must come back wrong. A
	# round trip that passes with the units field removed is not measuring units.
	python - "$OUT/r12_game.gltf" "$OUT/r12_floor.gltf" <<'PYEOF'
import json, sys
d = json.load(open(sys.argv[1]))
d.get("asset", {}).get("extras", {}).pop("metresPerUnit", None)
json.dump(d, open(sys.argv[2], "w"))
PYEOF
	"$NS" -no-gui gltf-import "$OUT/r12_floor.gltf" -o "$OUT/r12_floor.hkx" \
		--bones "$HKXSKEL" --tsv "$OUT/r12_floor.tsv" > "$OUT/r12_floor_import.log" 2>&1
	python tests/spells/gltf_roundtrip_cmp.py "$OUT/r12_orig.tsv" "$OUT/r12_floor.tsv" \
		> "$OUT/r12_floor.cmp" 2>&1
	frc=$?
	grep -E "^RT (worst_translation|result)" "$OUT/r12_floor.cmp" | sed 's/^/    /'
	if [ $frc != 0 ]; then ok "FLOOR: with metresPerUnit deleted the round trip FAILS"
	else bad "FLOOR: the round trip passes without the units field, so R12 proves nothing"; fi
else
	bad "R12 SKIPPED: no clip at $CLIP (a SKIP is never a pass)"
fi

# --------------------------------------------------------------------------
# FINDING (10). The hkx writer carries non-uniform scale, so the exporter must
# write glTF SCALE channels and the importer must read them. The body build
# cycle is the clip that has them: it is scale and nothing else.
#
# It is NOT a game-valid .hkx and is never offered as one -- skeleton.hkx
# carries none of the *_skin bones. The .hkx written here exists only so the
# numbers can be read back out of the importer.
note "R13 SCALE tracks: written non-uniform, and read back"
if [ -f "$ESMROOT/Fallout4.esm" ]; then
	run r13 "$BODY" --skeleton "$SKEL" --joints whole --units game --body-build-cycle \
		--data-root "$ESMROOT" -o "$OUT/r13.gltf" > /dev/null
	SC=$(g "$OUT/r13.gltf" scale_channels)
	echo "  scale channels: $SC     animations: $(g "$OUT/r13.gltf" animations)"
	if [ "${SC:-0}" -ge 40 ]; then ok "R13 the cycle is written as scale channels ($SC)"
	else bad "R13 only ${SC:-0} scale channel(s) were written"; fi
	python tests/spells/gltf_measure.py "$OUT/r13.gltf" --scale-node Belly_skin \
		| grep -E "^M scale_Belly" | sed 's/^/    /'
	NU=$(g "$OUT/r13.gltf" scale_Belly_skin_nonuniform --scale-node Belly_skin)
	if python -c "import sys; sys.exit(0 if float('${NU:-0}') > 0.05 else 1)"; then
		ok "R13 the scale is NON-UNIFORM on the wire (Belly_skin, worst axis gap $NU)"
	else bad "R13 Belly_skin's scale is uniform ($NU), so nothing tests the non-uniform path"; fi

	# and back: no --bones, so the glTF's own node names are the bone names
	"$NS" -no-gui gltf-import "$OUT/r13.gltf" -o "$OUT/r13.hkx" --tsv "$OUT/r13.tsv" \
		> "$OUT/r13_import.log" 2>&1
	if [ -f "$OUT/r13.tsv" ] && [ -f "$OUT/r12_game.tsv" ]; then
		python - "$OUT/r13.tsv" "$OUT/r12_game.tsv" <<'PYEOF'
import sys
def read(p):
    lo = hi = 1.0; nu = 0.0; n = 0
    for line in open(p, encoding="utf-8"):
        if line.startswith("#") or not line.strip():
            continue
        c = line.split("\t")
        if len(c) < 13:
            continue
        s = [float(x) for x in c[10:13]]
        n += 1; lo = min(lo, min(s)); hi = max(hi, max(s)); nu = max(nu, max(s) - min(s))
    return n, lo, hi, nu
n, lo, hi, nu = read(sys.argv[1])
print("    the imported cycle: %d rows, scale %.6f .. %.6f, worst non-uniformity %.6f" % (n, lo, hi, nu))
fn, flo, fhi, fnu = read(sys.argv[2])
print("    FLOOR, the jog clip through the same reader: scale %.6f .. %.6f, non-uniformity %.6f"
      % (flo, fhi, fnu))
bad = []
if nu < 0.05:
    bad.append("the importer did not bring the scale back")
if fnu > 1e-6:
    bad.append("the FLOOR clip has scale too, so the row would pass on any file")
print("    SCALEROW %s" % ("PASS" if not bad else "FAIL " + "; ".join(bad)))
sys.exit(0 if not bad else 1)
PYEOF
		if [ $? = 0 ]; then ok "R13 gltf-import reads the scale channels back, and the floor has none"
		else bad "R13 the scale did not survive the round trip"; fi
	else
		bad "R13 gltf-import wrote no tsv"
	fi
else
	bad "R13 SKIPPED: no Fallout4.esm under $ESMROOT (a SKIP is never a pass)"
fi

# --------------------------------------------------------------------------
# THE RULED DEFAULTS, MEASURED. bungo ruled on 2026-09-19 09:45 that the export
# should be Blender-ready out of the box: skeleton auto, joints whole, units
# game, animation on, bones body only, textures copy. Every other row in this
# file starts from --legacy-defaults and moves ONE thing; this is the only row
# that asks what a person actually gets, and it asks it of the six at once.
#
# --clip/--bones/-o/--data-root are INPUTS, not options: they are the CLI's way
# of saying "a clip is loaded and this is where the game's files are", which in
# the application is just the state of the window.
note "R14 THE RULED DEFAULTS: no option flag at all"
if [ -f "$CLIP" ] && [ -f "$HKXSKEL" ]; then
	L=$(runNew r14 "$BODY" --clip "$CLIP" --bones "$HKXSKEL" --data-root "$DATA" -o "$OUT/tex/r14.gltf")
	grep -E "^  (skeleton|joints|units|bones|textures|animation)" "$L" | sed 's/^/  /'
	eq "it is NOT the pre-ruling export" "0" "$(m "$L" legacy_defaults)"
	eq "the skeleton was auto-found" "129" "$(m "$L" skeleton_nodes)"
	eq "auto did not have to fall back" "0" "$(m "$L" skeleton_auto_missed)"
	eq "one joint list for every skin" "yes" "$(g "$OUT/tex/r14.gltf" joints_identical)"
	eq "game units: 1 glTF unit = 1 NIF unit" "1" "$(m "$L" metres_per_unit)"
	eq "the helper rig is gone" "15" "$(m "$L" helpers_dropped)"
	eq "117 nodes, the body and its bones" "117" "$(g "$OUT/tex/r14.gltf" nodes)"
	eq "the clip is carried" "1" "$(g "$OUT/tex/r14.gltf" animations)"
	eq "78 of 95 tracks driven (the 17 are the weapon's)" "78" "$(m "$L" tracks_matched)"
	NC=$(m "$L" textures_copied)
	if [ "${NC:-0}" -ge 1 ]; then ok "the .dds is copied beside the file ($NC)"
	else bad "no texture was copied, so 'textures copy' is not the default"; fi
	if [ -x "$BLENDER" ]; then
		"$BLENDER" --background --factory-startup --python tests/spells/gltf_char_blender.py \
			-- "$(cygpath -w "$OUT/tex/r14.gltf" 2>/dev/null || echo "$OUT/tex/r14.gltf")" \
			--expect-armatures 1 --max-empties 0 --frame-n 20 --min-move 0.001 \
			> "$OUT/r14_blender.log" 2>&1
		rc=$?
		grep -E "^BLENDER (objects|armature|move_)|^FAIL" "$OUT/r14_blender.log" | sed 's/^/    /'
		if [ $rc = 0 ]; then ok "R14 Blender opens it as ONE armature, no empties, and it deforms"
		else bad "R14 the shipped defaults do NOT give Blender one clean armature"; fi
	else
		bad "R14 SKIPPED the Blender arm: no Blender at $BLENDER"
	fi
else
	bad "R14 SKIPPED: no clip at $CLIP (a SKIP is never a pass)"
fi

# --------------------------------------------------------------------------
# A DEFAULT MUST NOT BE A WALL. `auto` is the default now, and most files are
# not characters. On a static the search ends with nothing, the export falls
# back to the file's own nodes, SAYS SO, and exits 0.
#
# The second half is the one that matters more: the installed game's skeleton
# exists on this machine whatever is open, so the row also proves the alarm
# clock did NOT come out wearing it.
note "R15 a STATIC: auto finds nothing, falls back, says so, and does not fail"
if [ -f "$STATIC" ]; then
	L=$(runNew r15 "$STATIC" --data-root "$DATA" -o "$OUT/r15.gltf")
	rc=$?
	grep -iE "skeleton:|auto found" "$L" | cut -c1-160 | sed 's/^/    /'
	if [ $rc = 0 ] && [ -f "$OUT/r15.gltf" ]; then ok "R15 the export succeeded"
	else bad "R15 the default refused a static (rc=$rc) -- a default became a wall"; fi
	eq "it says the fallback happened" "1" "$(m "$L" skeleton_auto_missed)"
	eq "no skeleton was hung on it" "0" "$(m "$L" skeleton_nodes)"
	N=$(g "$OUT/r15.gltf" nodes)
	if [ "${N:-999}" -lt 60 ]; then ok "R15 the static kept its own nodes ($N), not a human rig"
	else bad "R15 the static came out with $N nodes -- something rigged it"; fi
	# FLOOR: the SAME command on the body does find a skeleton, so this row is
	# not passing because auto-find is simply broken on this machine.
	L2=$(runNew r15_floor "$BODY" --data-root "$DATA" -o "$OUT/tex/r15_floor.gltf")
	eq "FLOOR: the body on the same machine DOES find one" "0" "$(m "$L2" skeleton_auto_missed)"
else
	bad "R15 SKIPPED: no static at $STATIC (a SKIP is never a pass)"
fi

# --------------------------------------------------------------------------
# R16 exists because R14 found a real defect the day the defaults moved: with a
# skeleton supplying the Camera / AnimObject / Weapon nodes AND `bones body`
# dropping them, the clip's channels were remapped to node -1 and the file was
# refused by our own validator ("animation 0 channel 27 drives node -1"). The
# clip is built BEFORE the drop, so the drop now takes those channels with it
# and SAYS how many and which. The floor is a named rung that still has the bug.
note "R16 skeleton + bones body + a clip: the helper's tracks go with the helper"
if [ -f "$CLIP" ] && [ -f "$HKXSKEL" ]; then
	L=$(run r16 "$BODY" --skeleton auto --bones-only body \
		--clip "$CLIP" --bones "$HKXSKEL" --data-root "$DATA" -o "$OUT/tex/r16.gltf")
	grep -E "lost their bone" "$L" | cut -c1-200 | sed 's/^/    /'
	if [ -f "$OUT/tex/r16.gltf" ]; then ok "R16 the export succeeded"
	else bad "R16 the export FAILED -- see $L"; fi
	NT=$(grep -cE "lost their bone" "$L")
	if [ "${NT:-0}" -ge 1 ]; then ok "R16 the dropped tracks are NAMED, not silent"
	else bad "R16 tracks went missing with no line saying so"; fi
	eq "R16 the helper rig is still gone" "15" "$(m "$L" helpers_dropped)"
	eq "R16 nothing drives a node that is not there" "117" "$(g "$OUT/tex/r16.gltf" nodes)"
	eq "R16 the clip is still carried" "1" "$(g "$OUT/tex/r16.gltf" animations)"
	# FLOOR: the rung built before this repair refuses the same command.
	RUNG="${RUNG:-./release/NifSkope.before_gltfdefaults.exe}"
	if [ -x "$RUNG" ]; then
		"$RUNG" -no-gui gltf "$BODY" --legacy-defaults --skeleton auto --bones-only body \
			--clip "$CLIP" --bones "$HKXSKEL" --data-root "$DATA" \
			-o "$OUT/tex/r16_floor.gltf" > "$OUT/r16_floor.log" 2>&1
		frc=$?
		grep -E "drives node" "$OUT/r16_floor.log" | sed 's/^/    /'
		if [ $frc != 0 ]; then ok "FLOOR: the rung before the repair refuses it (rc=$frc)"
		else bad "FLOOR did not fail: the rung exported it too, so R16 measures nothing"; fi
	else
		bad "R16 FLOOR SKIPPED: no rung at $RUNG (a SKIP is never a pass)"
	fi
else
	bad "R16 SKIPPED: no clip at $CLIP (a SKIP is never a pass)"
fi

echo
echo "==== $fails row(s) not as registered ===="
exit $([ "$fails" = 0 ] && echo 0 || echo 1)
