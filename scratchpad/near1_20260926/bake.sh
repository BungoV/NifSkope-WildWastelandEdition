#!/bin/bash
# near bake of one region: bake.sh <name> <x0> <y0> <x1> <y1> [worldspace hex]   (lane NEAR1)
# writes scratchpad/near1_20260926/<name>/<ws>.near.* and <name>.log; exe = the worktree's (or EXE=)
L=/e/Projects/NifskopeWWE-near1/scratchpad/near1_20260926
NS=${EXE:-/e/Projects/NifskopeWWE-near1/release/NifSkope.exe}
N=$1; shift
WS=${5:-3C}
if tasklist //FI "IMAGENAME eq Fallout4.exe" 2>/dev/null | grep -q Fallout4.exe; then echo GAME UP; exit 1; fi
mkdir -p "$L/$N"
date
REG=()
[ "$1" != "all" ] && REG=(--terrain-region "$1" "$2" "$3" "$4")
( time timeout 7200 "$NS" -no-gui lodgen --mo2-profile "E:/Projects/Fallout 4 Mods/profiles/Default" --worldspace "$WS" \
    "${REG[@]}" --near-library "E:/Projects/NifskopeWWE-near1/scratchpad/near1_20260926/$N" ) > "$L/$N.log" 2>&1
rc=$?
echo "RC=$rc"
grep -E "^near |^error|real" "$L/$N.log"
date
