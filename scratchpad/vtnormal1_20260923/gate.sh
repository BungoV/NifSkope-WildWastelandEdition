#!/bin/bash
# VTNORMAL1 gate: the new exe against the rung on the Sanctuary chunk.
# Legs: (a) no cache == rung, every file; (b) cache: only the msn sheets move,
# chunk sheets == rung-with-cache; (c) r bar; (d) density word == its pair;
# (e) half-aux: aux sheets == full sheets' mips 1.., colour untouched; (f) refusals.
set -u
L=/e/Projects/NifskopeWildWastelandEdition/scratchpad/vtnormal1_20260923
LW=E:/Projects/NifskopeWildWastelandEdition/scratchpad/vtnormal1_20260923
PY=/c/Users/bungo/AppData/Local/Programs/Python/Python39/python
MC="E:/Projects/Fallout 4 Mods/mods/Upscaled Terrain Normals/Textures/Terrain/Commonwealth"
WS=mod/FO4CSLOD/Commonwealth
fails=0; oks=0
ok() { echo "ok   $*"; oks=$((oks+1)); }
bad() { echo "FAIL $*"; fails=$((fails+1)); }
same_tree() {   # same_tree A B label
	# the .lodb is the bake's provenance record (exe size, clock, output paths,
	# the census lines): never byte-equal across two exes; lodb_same reads it
	local d; d=$(diff -rq "$L/out/$1" "$L/out/$2" 2>&1 | grep -v 'bake.log' | grep -v 'Commonwealth.lodb' | head -5)
	if [ -z "$d" ]; then ok "$3"; else bad "$3: $d"; fi
	[ -f "$L/out/$1/obj/Commonwealth.lodb" ] && lodb_same "$1" "$2"
}
lodb_same() {   # the record minus the exe line, the clock, the variant's own paths and this lane's census words
	# dropped: exe line, clock, the command line (switch/switches: leg d spells it
	# differently on purpose), and the two census lines of wall times / memory
	norm() { grep -vE '^(lodb|baked|switch|switches)	|^census	(stage times|bake census):' "$L/out/$1/obj/Commonwealth.lodb" | sed -e "s#/out/$1/#/out/VAR/#g" \
		-e 's/ normalMsnCache [0-9]* normalHeights [0-9]* normalMixed [0-9]* msnSheetsRead [0-9]* msnSheetsMissing [0-9]* unitsPerTexel [0-9]* halfAux [01]//'; }
	local d; d=$(diff <(norm "$1") <(norm "$2") | head -4)
	if [ -z "$d" ]; then ok "     .lodb record equal but for exe/clock/paths and the new census words"; else bad ".lodb record differs: $d"; fi
}
cd "$L"
PH="${PHASES:-abcdef}"

if [[ $PH == *a* ]]; then
	EXE=new bash bake.sh new_nocache
	same_tree rung_nocache new_nocache "(a) no cache: every output file byte-identical to the rung"
	grep -o 'normalMsnCache [0-9]* normalHeights [0-9]* normalMixed [0-9]* msnSheetsRead [0-9]* msnSheetsMissing [0-9]*' out/new_nocache/bake.log | sed 's/^/     census: /'
fi

if [[ $PH == *b* || $PH == *c* ]]; then
	EXE=new bash bake.sh new_cache --msn-cache "$MC"
	grep -o 'normalMsnCache [0-9]* normalHeights [0-9]* normalMixed [0-9]* msnSheetsRead [0-9]* msnSheetsMissing [0-9]*' out/new_cache/bake.log | sed 's/^/     census: /'
fi
if [[ $PH == *b* ]]; then
	same_tree rung_cache/tex new_cache/tex "(b) with cache: the chunk sheets are byte-identical to the rung's with the same cache"
	same_tree rung_cache/obj new_cache/obj "(b) with cache: the chunk objects are byte-identical"
	for d in 2 4; do
		out=$($PY cmp_tiles.py "$LW/out/rung_cache/$WS/Commonwealth.VT.$d.lodt" "$LW/out/new_cache/$WS/Commonwealth.VT.$d.lodt")
		echo "$out" | sed 's/^/     /'
		n_msn_same=$(echo "$out" | grep ' msn ' | sed -E 's/.*: ([0-9]+) of ([0-9]+).*/\1/')
		others_bad=$(echo "$out" | grep -v ' msn ' | awk -F': ' '{split($2,a," "); if (a[1]!=a[3]) print}')
		if [ -z "$others_bad" ]; then ok "(b) VT.$d: every non-normal sheet byte-identical to the rung"; else bad "(b) VT.$d non-normal sheets moved: $others_bad"; fi
		if [ "$n_msn_same" = "0" ]; then ok "(b) VT.$d: every tile's normal sheet changed"; else bad "(b) VT.$d: $n_msn_same tiles' normal unchanged"; fi
	done
	"$L/../../release/NifSkope.exe" -no-gui lodgen --lodt-check "$LW/out/new_cache/$WS/Commonwealth.VT.2.lodt" > out/new_cache/lodtcheck.txt 2>&1 \
		&& ok "(b) --lodt-check accepts the cache bake" || bad "(b) --lodt-check refuses the cache bake: $(tail -2 out/new_cache/lodtcheck.txt)"
fi
if [[ $PH == *c* ]]; then
	m=$($PY measure.py "$LW/out/new_cache/$WS/Commonwealth.VT.2.lodt" newL02)
	echo "$m" | sed 's/^/     /'
	rline=$(echo "$m" | grep 'his(row0=north)' | head -1)
	re=$(echo "$rline" | sed -E 's/.*east ([0-9.]+) north.*/\1/'); rn=$(echo "$rline" | sed -E 's/.*north ([0-9.]+)$/\1/')
	if $PY -c "import sys; sys.exit(0 if $re >= 0.88 and $rn >= 0.88 else 1)"; then ok "(c) L02 normal vs his sheet downsampled: r east $re north $rn >= 0.88"; else bad "(c) r east $re north $rn below 0.88"; fi
	m0=$($PY measure.py "$LW/out/rung_nocache/$WS/Commonwealth.VT.2.lodt" rungL02 | grep '^r rungL02 vs his(row0=north)')
	r0e=$(echo "$m0" | sed -E 's/.*east ([0-9.]+) north.*/\1/'); r0n=$(echo "$m0" | sed -E 's/.*north ([0-9.]+)$/\1/')
	if $PY -c "import sys; sys.exit(0 if $r0e < 0.88 or $r0n < 0.88 else 1)"; then ok "(c) the bar fails on the rung (r east $r0e north $r0n)"; else bad "(c) the rung passes the bar: it measures nothing"; fi
	# the transfer row (added after the bar, reported as such): the tile equals
	# lodgen's own BC1 of his downsampled sheet; the rung must fail it
	t=$($PY encceil.py "$LW/out/new_cache/$WS/Commonwealth.VT.2.lodt"); echo "$t" | sed 's/^/     /'
	w=$(echo "$t" | grep 'within' | sed -E 's/.*: ([0-9.]+)$/\1/')
	if $PY -c "import sys; sys.exit(0 if $w >= 0.999 else 1)"; then ok "(c) transfer: $w of L02 normal texels within 1/64 of lodgen-BC1(his downsampled)"; else bad "(c) transfer only $w"; fi
	t0=$($PY encceil.py "$LW/out/rung_nocache/$WS/Commonwealth.VT.2.lodt" | grep 'within' | sed -E 's/.*: ([0-9.]+)$/\1/')
	if $PY -c "import sys; sys.exit(0 if $t0 < 0.5 else 1)"; then ok "(c) transfer fails on the rung ($t0 within 1/64)"; else bad "(c) transfer passes on the rung ($t0)"; fi
fi

if [[ $PH == *d* ]]; then
	EXE=rung bash bake.sh rung_c512 --vt-content 512
	EXE=new bash bake.sh new_d16 --vt-density 16
	same_tree rung_c512 new_d16 "(d) --vt-density 16 == rung --vt-content 512"
	EXE=new bash bake.sh new_d32 --vt-density 32
	same_tree rung_nocache new_d32 "(d) --vt-density 32 == rung default"
	EXE=rung bash bake.sh rung_f1c512 --vt-finest 1 --vt-content 512
	EXE=new bash bake.sh new_d8 --vt-density 8
	same_tree rung_f1c512 new_d8 "(d) --vt-density 8 == rung --vt-finest 1 --vt-content 512"
fi

if [[ $PH == *e* ]]; then
	EXE=new bash bake.sh new_half --vt-half-aux --vt-height
	EXE=new bash bake.sh new_full_h --vt-height
	for d in 2 4; do
		out=$($PY cmp_tiles.py "$LW/out/new_full_h/$WS/Commonwealth.VT.$d.lodt" "$LW/out/new_half/$WS/Commonwealth.VT.$d.lodt" --half)
		echo "$out" | sed 's/^/     /'
		badl=$(echo "$out" | awk -F': ' '{split($2,a," "); if (a[1]!=a[3] || a[3]==0) print}')
		if [ -z "$badl" ]; then ok "(e) VT.$d half-aux: colour whole, every aux sheet == the full sheet's mips 1.."; else bad "(e) VT.$d: $badl"; fi
	done
	"$L/../../release/NifSkope.exe" -no-gui lodgen --lodt-check "$LW/out/new_half/$WS/Commonwealth.VT.2.lodt" > out/new_half/lodtcheck.txt 2>&1 \
		&& ok "(e) --lodt-check accepts a half-aux file" || bad "(e) --lodt-check refuses half-aux: $(tail -2 out/new_half/lodtcheck.txt)"
	"$L/../../release/NifSkope.before_vtnormal1.exe" -no-gui lodgen --lodt-check "$LW/out/new_half/$WS/Commonwealth.VT.2.lodt" > out/new_half/lodtcheck_rung.txt 2>&1 \
		&& bad "(e) the rung's reader ACCEPTS a half-aux file (it would misread it)" || ok "(e) the rung's reader refuses a half-aux file: $(grep -o 'refused[^.]*' out/new_half/lodtcheck_rung.txt | head -1)"
	$PY "$L/../../tests/spells/lodgen_vt_check.py" header "$LW/out/new_half/$WS/Commonwealth.VT.2.lodt" > out/new_half/vtcheck.txt 2>&1; echo "     lodgen_vt_check header: $(grep -c '^ok' out/new_half/vtcheck.txt) ok, $(grep -ciE '^fail' out/new_half/vtcheck.txt) fail"
	sa=$(du -sb out/new_full_h/mod | cut -f1); sb=$(du -sb out/new_half/mod | cut -f1)
	echo "     sizes with height: full $sa half-aux $sb"
	EXE=new bash bake.sh new_half_offcmp
	same_tree rung_nocache new_half_offcmp "(e) half-aux OFF == rung"
fi

if [[ $PH == *f* ]]; then
	X="$L/../../release/NifSkope.exe"; ESM="X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm"
	"$X" -no-gui lodgen "$ESM" --worldspace 3C --vt "$LW/out/refuse" --vt-density 16 --vt-content 512 --vt-estimate > out/refuse1.txt 2>&1; rc=$?
	[ $rc = 2 ] && ok "(f) --vt-density beside --vt-content refused (rc 2): $(head -1 out/refuse1.txt)" || bad "(f) density+content rc $rc"
	"$X" -no-gui lodgen "$ESM" --worldspace 3C --vt "$LW/out/refuse" --vt-density 12 --vt-estimate > out/refuse2.txt 2>&1; rc=$?
	[ $rc = 2 ] && ok "(f) --vt-density 12 refused (rc 2)" || bad "(f) density 12 rc $rc"
	"$X" -no-gui lodgen "$ESM" --worldspace 3C --vt "$LW/out/refuse" --vt-half-aux --vt-mips 1 --vt-border 4 --vt-estimate > out/refuse3.txt 2>&1; rc=$?
	[ $rc = 2 ] && ok "(f) --vt-half-aux with one mip refused (rc 2)" || bad "(f) half-aux mips 1 rc $rc"
fi
echo "RESULT oks $oks fails $fails"
