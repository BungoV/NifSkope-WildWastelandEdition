#!/bin/bash
# HLSLBULK1 R5: build the SPIR-V set of the live HLSL path into release/shaders/spirv and run the A4 differential gate on it.
#   bash tools/ww_spirv.sh [root]        (WW_SPIRV=0 skips; WW_SPIRV=1 forces a rebuild)
# The set is the converted HLSL (res/shaders/hlsl) -> DXC -> tests/hlsllive/spvgl.py -> one .spv per stage shader, a per-program
# link check against the real driver (programs.txt), then tests/hlsllive/a4_diff.py --apply: a program whose SPIR-V render is not
# exact (or within the named bound) is moved to the GLSL fallback list. Rebuilt only when an HLSL source or a tool is newer than
# the stamp; R7: and then incrementally (build_all.py: a .spv and a program's link / A4 verdict are redone only when their content key
# changed, DXC runs on every core; a run with no change touches nothing).
# WW_SPIRV=1 forces the check (keys still apply); WW_SPIRV=full redoes everything. The loader (src/gl/glcontext.cpp) reads release/shaders/spirv; with no folder every program stays on the GLSL.
# HLSLFIX2: the set is built in release/wws (the incremental state: .spv + .spv.key, locs.txt, programs.cache,
# a4cache.txt) and copied to release/shaders/spirv. The link step's copyDirs (NifSkope_functions.pri) deletes release/shaders
# and copies res/shaders anew on every link, so state kept there was lost each build: builds 97 and 98 rebuilt all 536 stages
# ("0 kept") and re-rendered every A4 program. The folder name is short on purpose: the longest stage name is 170 characters
# and Windows refuses paths over 259 (release/ww_spirv_work put cell_volair's 9-switch variant at 262: not built, not linked).
ROOT="${1:-$(cd "$(dirname "$0")/.." && pwd)}"
cd "$ROOT" || exit 1
[ "${WW_SPIRV:-auto}" = 0 ] && { echo "spirv: skipped (WW_SPIRV=0)"; exit 0; }
WORK=release/wws
DEST=release/shaders/spirv
STAMP=$WORK/spirv/.ww_stamp
mkdir -p "$WORK/spirv"
# first run with a work folder: start from the set the loader folder still holds (its keys and locations), if any
[ -f "$WORK/spirv/locs.txt" ] || [ ! -f "$DEST/locs.txt" ] || cp -rp "$DEST/." "$WORK/spirv/"
publish() {   # the work set -> the folder the loader reads (after the link step deleted it, or after a rebuild)
	mkdir -p "$DEST" && cp -rp "$WORK/spirv/." "$DEST/" && echo "spirv: set copied to $DEST ($(ls "$DEST" | grep -c '\.spv$') .spv)"
}
need=$([ "${WW_SPIRV:-auto}" = 1 ] || [ "${WW_SPIRV:-auto}" = full ] && echo 1 || echo 0)
[ -f "$STAMP" ] || need=1
[ "$need" = 1 ] || [ -z "$(find res/shaders/hlsl tests/hlsllive -type f \( -name '*.hlsl' -o -name '*.hlsli' -o -name '*.py' -o -name '*.txt' \) -newer "$STAMP" | head -1)" ] || need=1
if [ "$need" = 0 ]; then
	echo "spirv: the set is in step with the HLSL, skipped"
	[ -f "$DEST/.ww_stamp" ] || publish
	exit 0
fi
[ -x /e/Tools/dxc/bin/x64/dxc.exe ] || { echo "spirv: no DXC at E:/Tools/dxc, the programs stay on the GLSL"; exit 0; }
t0=$(date +%s)
FULLARG=$([ "${WW_SPIRV:-auto}" = full ] && echo --full)
python -I -X utf8 tests/hlsllive/build_all.py . "$WORK" $FULLARG > release/ww_spirv_build.log 2>&1 < /dev/null
tail -1 release/ww_spirv_build.log
t1=$(date +%s)
# HLSLFIX2: A4 in WW_SPIRV_A4_JOBS shards (default cores / 2), merged in the serial order; --apply once on the merged verdicts
bash tests/hlsllive/a4_shards.sh . "$WORK" release/ww_spirv_a4.log --apply ${FULLARG:+--fresh} || { echo "spirv: A4 shards failed, stamp not moved"; exit 4; }
tail -3 release/ww_spirv_a4.log
echo "spirv: A1 $(( t1 - t0 )) s, A4 $(( $(date +%s) - t1 )) s"
touch "$STAMP"
publish
echo "spirv: $(( $(date +%s) - t0 )) s"
