#!/bin/bash
#
# Lane BUILD8 -- THE IN-APP ROUND TRIP, through release/NifSkope.exe only.
#
# bungo, 2026-09-10, verbatim: "now export and import of gltf".
#
# For one clip:
#   1  exe  hkx-tsv      original .hkx           -> orig_exe.tsv   (the exe's own reader)
#      py   hkxanim_decode.py                    -> orig_py.tsv    (independent oracle)
#   2  exe  gltf         NIF + clip              -> rt.gltf/.bin
#   3  exe  gltf-import  rt.gltf                 -> rt.hkx  (+ --tsv = the IMPORTED clip,
#                                                            before the writer touches it)
#   4  py   interleaved_decode.py rt.hkx         -> rt_dec.tsv     (independent oracle;
#                                                    the exe's own reader is a SPLINE reader
#                                                    and refuses this class -- gate below)
#   5  tsvcmp orig_py vs rt_imported   = "the imported clip against the original decode"
#      tsvcmp orig_py vs rt_dec        = "written, decoded again"
#
# Bars, pre-registered by lane HKX5b: 1e-4 units, 0.01 degrees.
# Rows are keyed BY BONE (--map-track) and restricted to the rows both sides
# have (--only-common): the glTF cannot carry the 17 Weapon* tracks, which have
# no node in the body NIF, and every one of them is named by the exporter.
#
#   bash scratchpad/build8_20260910/roundtrip.sh
set -u
ROOT=/e/Projects/NifskopeWildWastelandEdition
W=E:/Projects/NifskopeWildWastelandEdition
cd "$ROOT" || exit 9
NS=./release/NifSkope.exe
OUT=$ROOT/scratchpad/build8_20260910/out
LOG=$ROOT/scratchpad/build8_20260910/logs
mkdir -p "$OUT" "$LOG"
NIF=$W/fixtures/human_male_vanilla.nif
BONES=$W/scratchpad/hkx1_20260910/clips/skeleton.hkx
RC=0

leg () {	# leg <tag> <clip.hkx> <extra gltf args> <extra import args>
	local tag="$1" clip="$2" gx="$3" ix="$4"
	echo
	echo "################ $tag"
	echo "--- 1a exe hkx-tsv (the exe's own reader on the ORIGINAL)"
	$NS -no-gui hkx-tsv "$clip" -o "$OUT/${tag}_orig_exe.tsv" 2>&1 | head -4
	echo "    rc=${PIPESTATUS[0]}"
	echo "--- 1b independent spline decode of the ORIGINAL"
	python tests/spells/hkxanim_decode.py "$clip" --out "$OUT/${tag}_orig_py.tsv" 2>&1 | head -4
	echo "--- 1c exe reader vs independent decoder, on the original"
	python scratchpad/hkx5_20260910/tsvcmp.py "$OUT/${tag}_orig_exe.tsv" "$OUT/${tag}_orig_py.tsv" \
		--label "[$tag orig: exe reader vs oracle]" || RC=1
	echo "--- 2 exe gltf export"
	$NS -no-gui gltf "$NIF" -o "$OUT/${tag}_rt.gltf" --clip "$clip" --bones "$BONES" $gx 2>&1 | head -3
	echo "--- 3 exe gltf-import -> .hkx (+ the imported clip as TSV)"
	$NS -no-gui gltf-import "$OUT/${tag}_rt.gltf" -o "$OUT/${tag}_rt.hkx" \
		--bones "$BONES" --tsv "$OUT/${tag}_rt_imported.tsv" $ix 2>&1 | head -12
	echo "    rc=${PIPESTATUS[0]}"
	echo "--- 3b the exe's OWN reader on the written .hkx (a spline reader; expected to refuse)"
	$NS -no-gui hkx-tsv "$OUT/${tag}_rt.hkx" -o "$OUT/${tag}_rt_exe.tsv" 2>&1 | head -2
	echo "--- 4 independent INTERLEAVED decode of the written .hkx"
	python scratchpad/hkx5_20260910/interleaved_decode.py "$OUT/${tag}_rt.hkx" \
		--out "$OUT/${tag}_rt_dec.tsv" 2>&1 | head -6
	echo "--- 5a THE IMPORTED CLIP vs the original decode"
	python scratchpad/hkx5_20260910/tsvcmp.py "$OUT/${tag}_orig_py.tsv" "$OUT/${tag}_rt_imported.tsv" \
		--map-track --only-common --ignore-root --label "[$tag hkx->gltf->import]" || RC=1
	echo "--- 5b WRITTEN AND DECODED AGAIN vs the original decode"
	python scratchpad/hkx5_20260910/tsvcmp.py "$OUT/${tag}_orig_py.tsv" "$OUT/${tag}_rt_dec.tsv" \
		--map-track --only-common --ignore-root --label "[$tag hkx->gltf->hkx->decode]" || RC=1
	echo "--- 5c the two ends of the WRITER alone (imported clip vs its own decode)"
	python scratchpad/hkx5_20260910/tsvcmp.py "$OUT/${tag}_rt_imported.tsv" "$OUT/${tag}_rt_dec.tsv" \
		--label "[$tag writer only]" || RC=1
}

# jog: 23 frames at 30 fps, root motion present and left out by default
leg jog "$W/scratchpad/hkx1_20260910/clips/jog.hkx" "" ""
# mixamo: 93 frames at 60 fps -- --source-rate keeps the 60 fps grid, so the
# comparison is frame for frame and not a resample
leg mixamo "$W/fixtures/Running_To_Slide_And_Back_To_Running.hkx" "" "--source-rate"

echo
echo "ROUNDTRIP-RC=$RC"
exit $RC
