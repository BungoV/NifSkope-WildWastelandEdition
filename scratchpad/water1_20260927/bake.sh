#!/bin/bash
# WATER1: one landscape-file (.lodl) bake of the Commonwealth, timed.
# usage: bake.sh <exe> <out dir> [extra lodgen args...]
# The .lodl is a whole-worldspace file by design (a region bake writes none), so there is no Boston box here:
# the landscape stage is the whole worldspace in tens of seconds.
EXE="$1"; OUT="$2"; shift 2
if tasklist //FI "IMAGENAME eq Fallout4.exe" 2>/dev/null | grep -q Fallout4.exe; then echo "GAME UP"; exit 1; fi
mkdir -p "$OUT"
wp() { echo "$1" | sed -E 's#^/([a-zA-Z])/#\U\1:/#'; }
bash /e/Projects/NifskopeWWE-fix1/scratchpad/fix1_20260926/turn.sh acquire water1 7200 || exit 1
t0=$(date +%s.%N)
"$EXE" -no-gui lodgen --mo2-profile "E:/Projects/Fallout 4 Mods/profiles/Default" --worldspace 3C \
    --lodl "$(wp "$OUT")" "$@" > "$OUT/bake.log" 2>&1
rc=$?
t1=$(date +%s.%N)
bash /e/Projects/NifskopeWWE-fix1/scratchpad/fix1_20260926/turn.sh release water1
L=$(find "$OUT" -name '*.lodl' | head -1)
echo "rc=$rc wall=$(python -c "print(round($t1-$t0,1))")s file=$L size=$(stat -c %s "$L" 2>/dev/null) sha1=$(sha1sum "$L" 2>/dev/null | cut -c1-40)"
grep -E "^lodl:|^  v[0-9]|timing:|water:|landscape|readback|error" "$OUT/bake.log" | head -12
