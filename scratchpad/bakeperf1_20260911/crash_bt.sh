#!/bin/bash
# BAKEPERF1: name the crash instead of guessing at it.
#
# The release exe carries no -g, so the stack is addresses plus whatever the
# export table gives -- which for a C++ binary is still enough to name the
# function, because the symbols are in the .exe's own symbol table unless it
# was stripped. `info threads` + `thread apply all bt` says WHICH thread and
# WHERE.
set -u
ROOT=/e/Projects/NifskopeWildWastelandEdition
OUT="$ROOT/scratchpad/bakeperf1_20260911/crash_gdb.txt"
ESM="X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm"
W="E:/Projects/NifskopeWildWastelandEdition/scratchpad/bakeperf1_20260911/crashrun"
rm -rf "$W"; mkdir -p "$W/tex"

gdb --batch \
    -ex 'set pagination off' \
    -ex 'set confirm off' \
    -ex run \
    -ex 'info threads' \
    -ex 'thread apply all bt 25' \
    --args "$ROOT/release/NifSkope.exe" -no-gui lodgen "$ESM" --worldspace 3C \
        --terrain-region 0 -12 19 7 --dim 4 \
        --out-dir "$W" --tex-dir "$W/tex" --native "$W" \
        --arrays --merge --threads 16 > "$OUT" 2>&1
echo "gdb exit $?"
tail -80 "$OUT"
