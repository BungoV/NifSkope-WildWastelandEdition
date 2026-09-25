#!/bin/bash
# TINT1: re-bake the native object library with BAKE1's object settings (bake1_20260925/bake.sh chunks stage),
# into scratchpad, WITHOUT the terrain virtual texture (no --vt/--vt-height/--vt-density, no --tex-dir so no stock
# chunk sheets) and without the .lodl stage. Kept exactly: --mo2-profile Default, --worldspace 3C, the whole map
# -96 -96 95 95, --dim all, --impostors <BAKE1 cards>, --arrays, --fo4cs-one-root.
# --cover is dropped: "--cover needs --tex-dir; the cover plane lives in the terrain data sheet" (run 1, rc 2).
# usage: bake_objects.sh <exe> <out dir>
set -u
NS="$1"; O="$2"
if tasklist //FI "IMAGENAME eq Fallout4.exe" 2>/dev/null | grep -q Fallout4.exe; then echo GAME UP; exit 1; fi
P="E:/Projects/Fallout 4 Mods/profiles/Default"
CARDS=E:/Projects/NifskopeWWE-bake1/scratchpad/bake1_20260925/cards
mkdir -p "$O/mod" "$O/stock" "$O/logs"
echo "$(date +%H:%M:%S) start $(sha1sum "$NS" | cut -c1-8)" | tee -a "$O/logs/stages.txt"
t0=$(date +%s)
"$NS" -no-gui lodgen --mo2-profile "$P" --worldspace 3C --terrain-region -96 -96 95 95 --dim all \
	--out-dir "$O/stock" --native "$O/mod" --impostors "$CARDS" --arrays --fo4cs-one-root \
	> "$O/logs/chunks.log" 2>&1
rc=$?
echo "$(date +%H:%M:%S) end rc=$rc $(( $(date +%s) - t0 )) s" | tee -a "$O/logs/stages.txt"
exit $rc
