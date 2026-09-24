#!/bin/bash
# Lane GROUND1 gate A3: INCR1's identity gate, re-run with --terrain-object-ao on.
#
# The ledger's dependency map now carries the object-AO row with a 1,458-unit
# reach inside the one-cell widening it already applied (docs/LODGEN_LEDGER_FORMAT.md
# section 2, row 9). If that reach is wrong, a chunk rebaked on its own will
# differ from the same chunk in a full bake, because it will have marched into a
# neighbour the ledger did not think it could see.
#
# --incremental refuses the VT pyramid (a whole-region stage), so this runs the
# STOCK chunk path, which is the other of the two sites the term lands in.
set -u
ROOT=/e/Projects/NifskopeWildWastelandEdition
W="$ROOT/scratchpad/ground1_20260912/work"
ESM="X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm"
DATA="E:/Tools/Fallout 4/DataUnpacked/Data"

run() {  # run <outdir> <extra...>
	local d="$1"; shift
	"$ROOT/release/NifSkope.exe" -no-gui lodgen "$ESM" --worldspace 3C \
		--terrain-region -24 24 -5 43 --dim 4 \
		--out-dir "$W/$d" --data-root "$DATA" --tex-dir "$W/$d/tex" \
		--cover --road-detail 1 --terrain-object-ao "$@" \
		> "$W/$d.log" 2>&1
	echo "$d rc=$?"
}

rm -rf "$W/i3full" "$W/i3incr"
mkdir -p "$W/i3full" "$W/i3incr"

echo "=== A3.1 a full bake with the term on, which also writes the ledger ==="
run i3full
grep -a 'incremental:' "$W/i3full.log" | head -2

echo "=== A3.2 the same bake again, from a copy, incrementally ==="
cp -r "$W/i3full/." "$W/i3incr/"
run i3incr --incremental "$W/i3incr"
grep -a 'incremental:' "$W/i3incr.log" | head -2
diff -r --brief "$W/i3full" "$W/i3incr" --exclude='*.log' > "$W/i3null.diff" 2>&1 \
	&& echo "A3 null run PASS (byte-identical)" \
	|| { echo "A3 null run FAIL"; head -20 "$W/i3null.diff"; }

echo "=== A3.3 lose ONE chunk's output, then rebake it incrementally ==="
# the CENTRE chunk of the 5x5: west -24 step 4 -> -24,-20,-16,-12,-8 ; south
# 24 step 4 -> 24,28,32,36,40.  The centre is (-16,32), which has a full ring of
# neighbours, so the one-cell neighbour rule has something to widen INTO and the
# run still skips the 16 chunks two rings out.  A smaller region cannot test
# this: with 2x2 chunks every chunk is a neighbour of every other and the
# "incremental" run is a full bake wearing a hat.
VICTIM="$W/i3incr/Commonwealth.4.-16.32.BTO"
ls -l "$VICTIM" || { echo "A3.3 victim missing -- NOT MEASURED"; exit 4; }
rm -f "$VICTIM"
run i3incr --incremental "$W/i3incr"
grep -a 'incremental:' "$W/i3incr.log" | head -2
diff -r --brief "$W/i3full" "$W/i3incr" --exclude='*.log' > "$W/i3one.diff" 2>&1 \
	&& echo "A3 one-chunk run PASS (byte-identical to the full bake)" \
	|| { echo "A3 one-chunk run FAIL"; head -20 "$W/i3one.diff"; }
