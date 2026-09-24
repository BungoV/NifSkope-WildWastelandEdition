#!/bin/bash
# NIFPARSE1 gate N1: the fault NAMED from a symbolised stack, THREE runs.
#
# Run ONLY on the symbol-bearing exe from relink_sym.sh -- on the stripped exe
# every frame is "?? ()" and that is not a stack (gate N1's floor).
#
# gdb changes the timing: a fault that is 5-of-5 bare can be 0-of-2 under gdb.
# So the run is the BIG region (25 chunks, 16 chunk threads), which is what
# caught it for BAKEPERF1, and it is taken three times.
#
#   ./gdb3.sh [runs]        default 3
set -u
ROOT=/e/Projects/NifskopeWildWastelandEdition
LANE="$ROOT/scratchpad/nifparse1_20260911"
ESM="X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm"
RUNS=${1:-3}

for i in $(seq 1 "$RUNS"); do
  W="$LANE/gdbrun$i"
  OUT="$LANE/crash_gdb_$i.txt"
  rm -rf "$W"; mkdir -p "$W/tex"
  echo "=== gdb run $i -> $OUT ==="
  gdb --batch \
      -ex 'set pagination off' \
      -ex 'set confirm off' \
      -ex run \
      -ex 'info threads' \
      -ex 'thread apply all bt 25' \
      --args "$ROOT/release/NifSkope.exe" -no-gui lodgen "$ESM" --worldspace 3C \
          --terrain-region 0 -12 19 7 --dim 4 \
          --out-dir "$W" --tex-dir "$W/tex" --native "$W" \
          --arrays --merge --threads 16 --chunk-threads 16 > "$OUT" 2>&1
  echo "gdb exit $?"
  # the faulting thread's frames, noise filtered (skill section 4)
  grep -vE "^\[(New )?Thread|^\[Switching" "$OUT" | grep -nE "^#|Program received|SIGSEGV|SIGTRAP|Thread [0-9]+" | head -40
  rm -rf "$W"
done
