#!/bin/bash
# HLSLFIX2: the A4 gate in N parallel shards (one hidden GL context per process), then one merge that writes a4.txt / a4cache.txt
# in the serial order and does --apply once.  bash tests/hlsllive/a4_shards.sh <tree> <spv dir> <log> [a4_diff args: --apply --fresh --plant]
# Jobs: WW_SPIRV_A4_JOBS, default cores / 2. A shard that does not end with its "written" line stops the merge (rc 4): the
# previous a4.txt / programs.txt are then left as they were.
TREE="$1"; OUT="$2"; LOG="$3"; shift 3
N="${WW_SPIRV_A4_JOBS:-$(( $(nproc) / 2 ))}"
[ "$N" -ge 1 ] 2>/dev/null || N=1
cd "$TREE" || exit 1
pids=()
for ((i = 0; i < N; i++)); do
  python -I -X utf8 tests/hlsllive/a4_diff.py . "$OUT" --shard "$i/$N" "$@" > "$LOG.shard$i" 2>&1 < /dev/null &
  pids+=($!)
done
bad=0
for ((i = 0; i < N; i++)); do
  wait "${pids[$i]}" || bad=1
  grep -q "^A4 shard $i of $N written" "$LOG.shard$i" || { echo "a4 shard $i of $N failed: $(tail -1 "$LOG.shard$i")"; bad=1; }
done
[ "$bad" = 0 ] || exit 4
python -I -X utf8 tests/hlsllive/a4_diff.py . "$OUT" --merge "$N" "$@" > "$LOG" 2>&1 < /dev/null
