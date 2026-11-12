#!/bin/bash
# HLSLBULK1 R5: build the SPIR-V set of the live HLSL path into release/shaders/spirv and run the A4 differential gate on it.
#   bash tools/ww_spirv.sh [root]        (WW_SPIRV=0 skips; WW_SPIRV=1 forces a rebuild)
# The set is the converted HLSL (res/shaders/hlsl) -> DXC -> tests/hlsllive/spvgl.py -> one .spv per stage shader, a per-program
# link check against the real driver (programs.txt), then tests/hlsllive/a4_diff.py --apply: a program whose SPIR-V render is not
# exact (or within the named bound) is moved to the GLSL fallback list. Rebuilt only when an HLSL source or a tool is newer than
# the stamp. The loader (src/gl/glcontext.cpp) reads release/shaders/spirv; with no folder every program stays on the GLSL.
ROOT="${1:-$(cd "$(dirname "$0")/.." && pwd)}"
cd "$ROOT" || exit 1
[ "${WW_SPIRV:-auto}" = 0 ] && { echo "spirv: skipped (WW_SPIRV=0)"; exit 0; }
OUT=release/shaders
STAMP=$OUT/spirv/.ww_stamp
need=$([ "${WW_SPIRV:-auto}" = 1 ] && echo 1 || echo 0)
[ -f "$STAMP" ] || need=1
[ "$need" = 1 ] || [ -z "$(find res/shaders/hlsl tests/hlsllive -type f \( -name '*.hlsl' -o -name '*.hlsli' -o -name '*.py' \) -newer "$STAMP" | head -1)" ] || need=1
[ "$need" = 0 ] && { echo "spirv: the set is in step with the HLSL, skipped"; exit 0; }
[ -x /e/Tools/dxc/bin/x64/dxc.exe ] || { echo "spirv: no DXC at E:/Tools/dxc, the programs stay on the GLSL"; exit 0; }
t0=$(date +%s)
python -I -X utf8 tests/hlsllive/build_all.py . "$OUT" > release/ww_spirv_build.log 2>&1 < /dev/null
tail -1 release/ww_spirv_build.log
python -I -X utf8 tests/hlsllive/a4_diff.py . "$OUT" --apply > release/ww_spirv_a4.log 2>&1 < /dev/null
tail -3 release/ww_spirv_a4.log
touch "$STAMP"
echo "spirv: $(( $(date +%s) - t0 )) s"
