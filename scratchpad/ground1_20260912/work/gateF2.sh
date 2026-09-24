#!/bin/bash
# Lane GROUND1 Part B, gate F2: the switch-off identity and the determinism.
#
#   F2a  this exe with NO erosion token in its argv == the rung's bytes, whole
#        tree, ledger included. Same argv on both exes, so the ledger's switch
#        digest is comparable.
#   F2a2 this exe with `--erosion 0` spelled out == the same bake without it,
#        every file but the ledger, whose digest is SHA-1 over the argument
#        vector by design (docs/LODGEN_LEDGER_FORMAT.md section 3).
#   F2b  the .lodl written with the pass on == the .lodl with it off.
#   F2c  --threads 1 == --threads 16 with the pass on.
#   F2d  the chunk -4,-4 products from a ONE-chunk region == the same chunk's
#        products inside a TWO-BY-TWO-chunk region. The .lodt pyramid and the
#        ledger cover different ground in the two runs and are not compared.
#
# Region bakes only. --road-detail 1 on every one of them.
set -u
ROOT=/e/Projects/NifskopeWildWastelandEdition
W="$ROOT/scratchpad/ground1_20260912/work"
. "$W/bake.sh"

ERO="--erosion 2 --erosion-iterations 4 --erosion-seed 7 --land-detail-source erosion"

fail=0
cmpdir() {   # cmpdir <name> <a> <b>
	local name="$1" a="$2" b="$3"
	diff -rq "$a" "$b" > "$W/$name.diff" 2>&1
	local n
	n=$(grep -c . "$W/$name.diff")
	if [ "$n" -eq 0 ]; then
		echo "$name IDENTICAL"
	else
		echo "$name DIFFERS ($n files) -> $W/$name.diff"
		head -8 "$W/$name.diff"
		fail=1
	fi
}

echo "=== F2a  the same argv on both exes ==="
bake NifSkope.before_ground1.exe f2_rung -4 -4 -1 -1 --threads 4
bake NifSkope.exe                f2_new  -4 -4 -1 -1 --threads 4
cmpdir f2a "$W/f2_rung" "$W/f2_new"

echo "=== F2a2  --erosion 0 spelled out ==="
bake NifSkope.exe f2_off -4 -4 -1 -1 --threads 4 --erosion 0
diff -rq "$W/f2_new" "$W/f2_off" > "$W/f2a2.diff" 2>&1
n=$(grep -c . "$W/f2a2.diff")
only_ledger=$(grep -c . "$W/f2a2.diff")
if [ "$n" -eq 0 ]; then
	echo "f2a2 IDENTICAL (the ledger digest did not move either)"
elif [ "$n" -eq 1 ] && grep -q "Commonwealth.lodb" "$W/f2a2.diff"; then
	echo "f2a2 IDENTICAL except the ledger's switch digest, which is SHA-1 over argv:"
	cat "$W/f2a2.diff"
else
	echo "f2a2 DIFFERS in $n files"; cat "$W/f2a2.diff"; fail=1
fi

echo "=== F2b  the .lodl plane is untouched by the pass ==="
mkdir -p "$W/f2_lodl_off_dir" "$W/f2_lodl_on_dir"
rm -f "$W/f2_lodl_off_dir"/* "$W/f2_lodl_on_dir"/*
bake NifSkope.exe f2_lodl_off -4 -4 -1 -1 --threads 4 --lodl "$W/f2_lodl_off_dir"
bake NifSkope.exe f2_lodl_on  -4 -4 -1 -1 --threads 4 --lodl "$W/f2_lodl_on_dir" $ERO
echo "  .lodl files: $(find "$W/f2_lodl_off_dir" -type f | wc -l) off, $(find "$W/f2_lodl_on_dir" -type f | wc -l) on"
cmpdir f2b "$W/f2_lodl_off_dir" "$W/f2_lodl_on_dir"

echo "=== F2c  1 thread vs 16, pass on ==="
bake NifSkope.exe f2_t1  -4 -4 -1 -1 --threads 1  $ERO
bake NifSkope.exe f2_t16 -4 -4 -1 -1 --threads 16 $ERO
cmpdir f2c "$W/f2_t1" "$W/f2_t16"

echo "=== F2d  one chunk alone vs the same chunk inside 2x2 chunks ==="
bake NifSkope.exe f2_big -4 -4 3 3 --threads 4 $ERO
: > "$W/f2d.diff"
n=0
for f in "$W"/f2_t1/obj/Commonwealth.4.-4.-4.* "$W"/f2_t1/tex/Commonwealth.4.-4.-4*.DDS; do
	[ -f "$f" ] || continue
	g="${f/f2_t1/f2_big}"
	n=$((n+1))
	if [ ! -f "$g" ]; then echo "missing: $g" >> "$W/f2d.diff"
	else cmp -s "$f" "$g" || echo "differs: $(basename "$f")" >> "$W/f2d.diff"; fi
done
echo "  chunk -4,-4 files compared: $n"
if [ "$(grep -c . "$W/f2d.diff")" -eq 0 ] && [ "$n" -gt 0 ]; then
	echo "f2d IDENTICAL"
else
	echo "f2d DIFFERS"; cat "$W/f2d.diff"; fail=1
fi

echo "=== F2 RESULT: $([ $fail -eq 0 ] && echo PASS || echo FAIL) ==="
