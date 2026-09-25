#!/bin/bash
# TINT1: is a difference against BAKE1 the CODE or run-to-run noise? One small object bake (cells -20,17..-17,20,
# which hold the Concord water tower, --dim all, the same cards) with a given exe into <out>.
# usage: detrun.sh <exe> <out dir>
NS="$1"; O="$2"; mkdir -p "$O/logs"
if tasklist //FI "IMAGENAME eq Fallout4.exe" 2>/dev/null | grep -q Fallout4.exe; then echo GAME UP; exit 1; fi
"$NS" -no-gui lodgen --mo2-profile "E:/Projects/Fallout 4 Mods/profiles/Default" --worldspace 3C \
  --terrain-region -20 17 -17 20 --dim all --out-dir "$O/stock" --native "$O/mod" \
  --impostors E:/Projects/NifskopeWWE-bake1/scratchpad/bake1_20260925/cards --arrays --fo4cs-one-root > "$O/logs/chunks.log" 2>&1
echo "rc=$?"
