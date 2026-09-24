#!/bin/bash
# CELLVIEW3: is THIS lane's exe's stock object bake byte-identical to the exe the
# lane started from?
#
# tests/spells/lodgen_native_baseline.sh --check compares against a baseline
# written on 2026-09-10 and names 6 changed files -- and it names the SAME 6 on
# the rung this lane rung aside before touching anything, so that gate cannot
# tell this lane's change from nine days of other lanes'. This one can: the same
# four chunks and the region, baked twice, once per exe, hashed and diffed.
#
# Both runs are `-no-gui` bakes. No window is opened, so no Recent Files list is
# written -- the rule the brief states is about GUI runs of an old rung.
set -u
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
ESM="${ESM:-X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm}"
DATA="${DATA:-E:/Tools/Fallout 4/DataUnpacked/Data}"
# an ABSOLUTE, Windows-shaped path: the first run wrote its chunks relative to
# the exe's own cwd and every save failed by name, which the hash list then
# reported as "1 file, identical" -- a green with nothing behind it.
W="E:/Projects/NifskopeWildWastelandEdition/scratchpad/cellview3_20260919/bake_ab"
rm -rf "$W"; mkdir -p "$W"

run() {   # run <tag> <exe>
	local tag=$1 ns=$2
	mkdir -p "$W/$tag/chunks" "$W/$tag/region"
	"$ns" -no-gui lodgen "$ESM" --worldspace 3C --objects -20 24 --dim 4 --identity \
		--data-root "$DATA" -o "$W/$tag/chunks/c4.bto"   > "$W/$tag/b1.log" 2>&1
	"$ns" -no-gui lodgen "$ESM" --worldspace 3C --objects -24 24 --dim 8 --identity \
		--data-root "$DATA" -o "$W/$tag/chunks/c8.bto"   > "$W/$tag/b2.log" 2>&1
	"$ns" -no-gui lodgen "$ESM" --worldspace 3C --objects -32 16 --dim 16 --identity \
		--slot-fallback --data-root "$DATA" -o "$W/$tag/chunks/c16.bto" > "$W/$tag/b3.log" 2>&1
	"$ns" -no-gui lodgen "$ESM" --worldspace 3C --terrain-region -20 24 -19 25 --dim 4 \
		--identity --arrays --atlas --data-root "$DATA" --out-dir "$W/$tag/region" \
		> "$W/$tag/b4.log" 2>&1
	( cd "$W/$tag" && find chunks region -type f | sort | xargs sha256sum ) > "$W/$tag.sha"
}

run before "$ROOT/release/NifSkope.before_cellview3.exe"
run after  "$ROOT/release/NifSkope.exe"

nb=$(wc -l < "$W/before.sha"); na=$(wc -l < "$W/after.sha")
echo "before: $nb files"
echo "after:  $na files"
# a floor, so an empty comparison can never read as a pass
if [ "$nb" -lt 8 ] || [ "$na" -lt 8 ]; then
	echo "REFUSED: fewer than 8 outputs hashed -- the bakes did not run"
	exit 2
fi
if diff -q "$W/before.sha" "$W/after.sha" > /dev/null; then
	echo "PASS every stock bake output is byte-identical across the two exes"
	exit 0
fi
echo "FAIL the bakes differ:"
diff "$W/before.sha" "$W/after.sha" | head -20
exit 1
