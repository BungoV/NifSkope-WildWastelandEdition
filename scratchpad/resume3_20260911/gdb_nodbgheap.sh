#!/bin/bash
# RESUME3 gate R2: the ORIGINAL fault, SYMBOLISED, with the WINDOWS DEBUG HEAP
# TURNED OFF.
#
# THE TRAP THAT COST BAKEPERF1 AND NIFPARSE1 THEIR STACK. Windows gives a
# process created by a debugger the DEBUG heap, which validates differently and
# does not fail-fast -- so a heap-corrupting race that is 3-of-5 bare is 0-of-5
# under gdb, and both earlier lanes recorded "it never crashes under gdb" as a
# property of the bug. It is a property of the debugger. `_NO_DEBUG_HEAP=1` in
# the INFERIOR's environment restores the normal heap, and the same run then
# faults in three seconds:
#
#     warning: Critical error detected c0000374
#     Thread 10 "QThread" received signal SIGTRAP
#
# gdb and nm live only inside MSYS2 UCRT64; from Git-Bash `which gdb` is empty.
# Launch this through the MSYS2 login shell.
#
#   ./gdb_nodbgheap.sh [runs] [tag]
set -u
ROOT=/e/Projects/NifskopeWildWastelandEdition
LANE="$ROOT/scratchpad/resume3_20260911"
ESM="X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm"
RUNS=${1:-3}
TAG=${2:-nh}

for i in $(seq 1 "$RUNS"); do
  W="$LANE/gdbrun$i"
  OUT="$LANE/logs/crash_gdb_${TAG}$i.txt"
  rm -rf "$W"; mkdir -p "$W/tex"
  echo "=== $TAG run $i -> $OUT  $(date +%H:%M:%S) ==="
  gdb --batch \
      -ex 'set pagination off' \
      -ex 'set confirm off' \
      -ex 'set environment _NO_DEBUG_HEAP=1' \
      -ex run \
      -ex 'info threads' \
      -ex 'thread apply all bt 20' \
      --args "$ROOT/release/NifSkope.exe" -no-gui lodgen "$ESM" --worldspace 3C \
          --terrain-region -20 24 -9 35 --dim 4 \
          --out-dir "$W" --tex-dir "$W/tex" --native "$W" \
          --arrays --merge --chunk-threads 16 > "$OUT" 2>&1
  echo "gdb rc=$?  $(date +%H:%M:%S)"
  grep -E "Critical error detected|received signal|exited normally" "$OUT" | head -3
  rm -rf "$W"
done
