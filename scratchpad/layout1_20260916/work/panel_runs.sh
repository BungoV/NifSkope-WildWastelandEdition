#!/bin/bash
# lane LAYOUT1 (2026-09-16): the panel picture, and WHOSE the one red is.
# Run from GIT BASH only -- the MSYS2 bash boundary drops every variable this
# script sets (measured this lane; see MISTAKES.md), which is why the earlier
# SHOT= run produced no picture and no line in the log.
set -u
ROOT=/e/Projects/NifskopeWildWastelandEdition
cd "$ROOT" || exit 2
# ONLY ucrt64/bin (the Qt DLLs the exe needs).  Adding /c/msys64/usr/bin here
# would change which bash runs the harness, and the MSYS2 bash drops every
# variable set on this side -- which is what ate the first three grabs.
export PATH="/c/msys64/ucrt64/bin:$PATH"
export USER="${USER:-bungo}"
W="$ROOT/scratchpad/layout1_20260916/work/pics"

# 1. the panel of the exe under test, with the grab
rm -f "$W/panel.png"
SHOT="$(cd "$W" && pwd -W)/panel.png" PORT=42307 \
	bash "$ROOT/tests/spells/lod_generation.sh" > "$W/panel_selftest.log" 2>&1
echo "panel  : rc=$?, png $(stat -c%s "$W/panel.png" 2>/dev/null || echo NONE) bytes  $(date +%H:%M:%S)"
grep -a "screenshot \|grab width" "$W/panel_selftest.log" | sed 's/^/  | /'
tail -3 "$W/panel_selftest.log" | sed 's/^/  | /'

# 2. the SAME self-test on the rung exe: does the archives-vs-unpacked red
#    pre-date this lane?  No shot, no gate, nothing but the checks.
EXE="$ROOT/release/NifSkope.before_layout1.exe" PORT=42311 \
	bash "$ROOT/tests/spells/lod_generation.sh" > "$W/panel_rung.log" 2>&1
echo "rung   : rc=$?  $(date +%H:%M:%S)"
grep -a -i "chunk from the\|byte-identical to one built from the unpacked\|near chunk" "$W/panel_rung.log" | sed 's/^/  | /'
tail -3 "$W/panel_rung.log" | sed 's/^/  | /'
echo "panel_runs done $(date +%H:%M:%S)"
