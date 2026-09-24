#!/bin/bash
# RESUME3 gate R2: the ORIGINAL fault, SYMBOLISED, three runs.
#
# NIFPARSE1's gdb3.sh ran the 25-chunk Boston region because that is what caught
# it for BAKEPERF1. Measured here first (17:00-17:07): with the parse lock off,
# Boston at 16 chunk threads ran CLEAN once, while SANCTUARY faulted 3 of 5
# (0xC0000374). So the gdb runs go where the fault actually is.
#
# TRAP, and it cost this lane a run: gdb and nm live ONLY inside MSYS2 UCRT64.
# From Git-Bash `which gdb` is empty and relink_sym.sh's own SYMBOLS= line
# prints 0 on an exe that carries 71,982 symbols. This script must be launched
# through the MSYS2 login shell.
#
#   ./gdb_sanctuary.sh [runs]
set -u
ROOT=/e/Projects/NifskopeWildWastelandEdition
LANE="$ROOT/scratchpad/resume3_20260911"
ESM="X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm"
RUNS=${1:-3}

for i in $(seq 1 "$RUNS"); do
  W="$LANE/gdbrun$i"
  OUT="$LANE/logs/crash_gdb_$i.txt"
  rm -rf "$W"; mkdir -p "$W/tex"
  echo "=== gdb run $i -> $OUT  $(date +%H:%M:%S) ==="
  gdb --batch \
      -ex 'set pagination off' \
      -ex 'set confirm off' \
      -ex run \
      -ex 'info threads' \
      -ex 'thread apply all bt 20' \
      --args "$ROOT/release/NifSkope.exe" -no-gui lodgen "$ESM" --worldspace 3C \
          --terrain-region -20 24 -9 35 --dim 4 \
          --out-dir "$W" --tex-dir "$W/tex" --native "$W" \
          --arrays --merge --chunk-threads 16 > "$OUT" 2>&1
  echo "gdb exit $?  $(date +%H:%M:%S)"
  grep -E "Program received|SIGSEGV|SIGTRAP|Thread [0-9]+ received|exited with code" "$OUT" | head -5
  rm -rf "$W"
done
