#!/bin/bash
# The land-lock gate: a 36-chunk ring-4 native bake on his load order (enough chunks that the vertex-AO loop
# runs on the thread pool; below 32 it is serial), with exe A then exe B; the .lodo/.lodi must be byte-identical.
# usage: [EXTRA="--threads 1"] landlock_gate.sh <exe> <out dir>   (--threads 1 = the serial way back, the byte reference)
EXE="$1"; OUT="$2"
P="E:/Projects/Fallout 4 Mods/profiles/Default"
rm -rf "$OUT"; mkdir -p "$OUT/mod" "$OUT/stock"
t0=$(date +%s)
"$EXE" -no-gui lodgen --mo2-profile "$P" --worldspace 3C --terrain-region -24 8 -1 31 --dim 4 \
	--out-dir "$OUT/stock" --tex-dir "$OUT/stock/textures" --native "$OUT/mod" --fo4cs-one-root ${EXTRA:-} > "$OUT/run.log" 2>&1
rc=$?
echo "$(basename "$OUT") rc=$rc $(( $(date +%s) - t0 )) s"
grep -a "Qt Concurrent\|terminate" "$OUT/run.log" | head -2
grep -a "^native: \|vertexAo\|vertex-ao\|native-vao" "$OUT/run.log" | cut -c1-220 | head -3
( cd "$OUT/mod" && find . -name "*.lodo" -o -name "*.lodi" | sort | xargs sha1sum )
