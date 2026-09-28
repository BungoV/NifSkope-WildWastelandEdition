#!/bin/bash
# GROUND1: the C++ reader on the good pairs and on every doctored copy redread.py wrote, under one turn.
# usage: verify.sh <exe> <on .lodo> <off dir> <red dir>
set -u
NS=$1; LODO=$2; OFF=$3; RED=$4
if tasklist //FI "IMAGENAME eq Fallout4.exe" 2>/dev/null | grep -q Fallout4.exe; then echo "GAME UP"; exit 1; fi
bash /e/Projects/NifskopeWWE-fix1/scratchpad/fix1_20260926/turn.sh acquire ground1 7200 || exit 1
run() { echo "== $1"; WW_SETTINGS_SCOPE=ground1 timeout 600 "$NS" -no-gui lodgen --native-verify "$2" "$3" 2>&1 | tr -d '\r' | grep -iE "refus|error|fail|ok|verif|version|ground" | head -4; echo "rc=${PIPESTATUS[0]}"; }
run on_good "$LODO" "${LODO%.lodo}.lodi"
run off_good "$OFF/Commonwealth.lodo" "$OFF/Commonwealth.lodi"
for f in "$RED"/*.lodi; do run "$(basename "$f" .lodi)" "$LODO" "$f"; done
bash /e/Projects/NifskopeWWE-fix1/scratchpad/fix1_20260926/turn.sh release ground1
