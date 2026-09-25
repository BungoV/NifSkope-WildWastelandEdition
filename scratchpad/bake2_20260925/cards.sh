#!/bin/bash
# BAKE2 tree card bake: tools/bake_impostor_cards.sh's photograph loop, driven from a CANDIDATE FILE (the union of the
# three worldspaces' `--list-impostor-candidates --candidates trees` lists, each over its own LAND extent) instead
# of a Commonwealth-only listing (the tool hard-codes --worldspace 3C and has no --mo2-profile).
# Settings = BAKE1's library.txt: oct 8, ring 0, tile 256, ref 1328.6 (the same size ladder: the largest candidate
# of all three worlds is 1328.6 too), half_aux 0, candidates trees. Meshes resolve through --probe on his MO2
# profile (DATAROOT points at nothing, as in BAKE1); the photograph runs with WW_LODGEN_RESOURCES = the profile's
# 49-entry stack. One GUI at a time across lanes: wait_turn.sh before every launch; unique port.
# usage: cards.sh <candfile> <outdir>
set -u
L=/e/Projects/NifskopeWWE-bake2/scratchpad/bake2_20260925
NS=/e/Projects/NifskopeWWE-bake2/release/NifSkope.exe
P="E:/Projects/Fallout 4 Mods/profiles/Default"
CANDS="$1"; OUT="$2"
OCT=8; RING=0; TILE=256; REF=1328.6
PORT=45963
. /e/Projects/NifskopeWWE-bake2/tests/spells/_harness.sh
RESOURCES="$("$NS" -no-gui lodgen --mo2-profile "$P" --print-source 2>&1 | tr -d '\r' | sed -n 's/^resource [0-9]*: //p' | paste -sd';')"
nres=$(printf '%s' "$RESOURCES" | tr ';' '\n' | grep -c .)
echo "resources: $nres"
[ "$nres" -ge 40 ] || { echo "resource stack too short"; exit 1; }
mkdir -p "$OUT"
printf 'oct %s\nring %s\ntile %s\nref %s\nhalf_aux 0\ncandidates trees\n' $OCT $RING $TILE $REF > "$OUT/library.txt"
W="$(mktemp -d)"; trap 'rm -rf "$W"' EXIT
n=0
while read -r formid extent model; do
	[ -n "$formid" ] || continue
	n=$((n+1))
	[ -f "$OUT/${formid}.txt" ] && { echo "[$n] $formid cached"; continue; }
	rm -rf "$W/ex"; mkdir -p "$W/ex"
	exname="$(basename "${model//\\//}")"
	if ! "$NS" -no-gui lodgen --mo2-profile "$P" --probe "meshes/${model//\\//}" \
		--probe-out "$(winpath "$W/ex")/$exname" >/dev/null 2>&1 || [ ! -f "$W/ex/$exname" ]; then
		echo "[$n] $formid MISSING ($model)"; continue
	fi
	if tasklist //FI "IMAGENAME eq Fallout4.exe" 2>/dev/null | grep -q Fallout4.exe; then echo "GAME UP"; exit 1; fi
	bash "$L/wait_turn.sh" 3600 || exit 1
	rm -rf "$W/bake"; mkdir "$W/bake"
	WW_IMPOSTOR_BAKE="$(winpath "$W/bake")" WW_IMPOSTOR_OCT="$OCT" WW_IMPOSTOR_RING="$RING" WW_IMPOSTOR_TILE="$TILE" \
		WW_IMPOSTOR_REF="$REF" WW_LODGEN_RESOURCES="$RESOURCES" WW_LODGEN_MO2="" \
		"$NS" "$(winpath "$W/ex/$exname")" --port $PORT >/dev/null 2>&1
	base="$(basename "${model//\\//}" .nif | tr 'A-Z' 'a-z')"
	if [ -f "$W/bake/${base}_front.png" ]; then
		mv "$W/bake/${base}_front.png" "$OUT/${formid}_front.png"
		mv "$W/bake/${base}_side.png" "$OUT/${formid}_side.png" 2>/dev/null
		for s in albedo normal gsaos rmaos g e; do
			[ -f "$W/bake/${base}_oct_$s.png" ] && mv "$W/bake/${base}_oct_$s.png" "$OUT/${formid}_oct_$s.png"
		done
		mv "$W/bake/${base}.txt" "$OUT/${formid}.txt"
		echo "[$n] $formid baked ($model)"
	else
		echo "[$n] $formid FAILED ($model)"
	fi
done < "$CANDS"
echo done
