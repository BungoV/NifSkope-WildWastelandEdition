#!/bin/bash
# BAKE2, ruling (a): the gate that FAILS on the current code and PASSES after. Pre-registered 2026-09-25 before
# the build. One Nuka-World objects bake (bake.sh's chunks flags minus the VT: --vt, --vt-height, --cover write the
# terrain sheets, which the .lodi does not read) with the exe named, into scratch, then widescale_check.py.
#   rung  release/NifSkope.before_bake2.exe  -> the .lodi writer refuses ref 0604DDB9 (8.33): W0/W1 FAIL
#   new   the rebuilt exe                    -> WIDESCALE G2 PASS
# usage: widescale_gate.sh <exe> <name>      (writes <lane>/wsgate/<name>/)
set -u
EXE="$1"; NAME="$2"
L=E:/Projects/NifskopeWWE-bake2/scratchpad/bake2_20260925
P="E:/Projects/Fallout 4 Mods/profiles/Default"
O="$L/wsgate/$NAME"; rm -rf "$O"; mkdir -p "$O/mod" "$O/scratch"
if tasklist //FI "IMAGENAME eq Fallout4.exe" 2>/dev/null | grep -q Fallout4.exe; then echo "GAME UP"; exit 1; fi
t0=$(date +%s); echo "$(date +%H:%M:%S) start $NAME exe $(sha1sum "$EXE" | cut -c1-8)" | tee "$O/time.txt"
"$EXE" -no-gui lodgen --mo2-profile "$P" --worldspace 0600290F --terrain-region -32 -32 32 32 --dim all \
	--out-dir "$O/scratch" --tex-dir "$O/scratch/textures" --native "$O/mod" \
	--impostors "$L/cards" --arrays --fo4cs-one-root > "$O/chunks.log" 2>&1
rc=$?
echo "$(date +%H:%M:%S) end rc=$rc $(( $(date +%s) - t0 )) s" | tee -a "$O/time.txt"
grep -a "refused, not clamped\|could not write\|native-wide-scale" "$O/chunks.log" | cut -c1-240 | head -3
python "$L/widescale_check.py" "$O/mod/FO4CSLOD/NukaWorld/NukaWorld.lodi" "$O/chunks.log" "$L/nw_overscale.txt" $rc
