#!/bin/bash
# lane HLSLSRC3 (2026-11-13): the GPU cull kernels (src/features/near/cellgpucull.cpp) against their readable HLSL twins
# (res/shaders/hlsl/near/cellgpucull_<k>_cs.hlsl), on the GPU, byte for byte. No NifSkope: cullmatch.exe is a headless
# Vulkan device of its own. The GLSL side is the runtime's text (cellgpucull.cpp compile(): "#version 430 core", the
# defines, the file) built with glslang's relaxed Vulkan rules (-R: the loose uniforms become the default block, here at
# binding 31 like LooseUniforms). DXC runs -O0 (its optimizer reorders float math; see tests/hlslmatch/run.sh).
#   bash tests/hlslmatch/cullmatch.sh <out dir> [kernel...]   (hiz0 hiz0ms hizn cull bc7; default the four cull kernels)
# Each kernel: green (the HLSL) must PASS, red (-D CELLGPUCULL_<K>_RED) must FAIL. Last line: CULLMATCH GATE OK|FAILED.
# bc7 (lane FO4MATCH1, opt-in): the far-LOD BC7 encoder (src/features/far-lod/lodgengpu.cpp) against
# res/shaders/hlsl/far-lod/lodgen_bc7_cs.hlsl; the GLSL's %TABLES% filled by bc7tables.py as the host does; red LODGEN_BC7_RED.
# Not green yet (11-14, RTX 5070 Ti): pass 0 errors agree on every block, but each module loses a different dynamic
# component write into Fit's ivec4 q[2] / ep[2] (GLSL: ep[1].zw after reconstruct; HLSL: q[1].zw in tryEndpoints),
# so the block bits differ. Kept out of the default list until that is settled; run it by name.
# Needs MSYS2 UCRT64 g++ (Vulkan headers) on PATH.
set -u
OUT="${1:?out dir}"; shift
KS=("$@"); [ ${#KS[@]} -gt 0 ] || KS=(hiz0 hiz0ms hizn cull)
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
DXC="${DXC:-/e/Tools/dxc/bin/x64/dxc.exe}"
G="${GLSLANG:-/e/Tools/RenderDoc/RenderDoc_1.45_64/plugins/spirv/glslangValidator.exe}"
ENV=vulkan1.2
mkdir -p "$OUT"
W() { cygpath -m "$1" 2>/dev/null || echo "$1"; }
g++ -O1 -std=c++17 "$ROOT/tests/hlslmatch/cullmatch.cpp" -o "$OUT/cullmatch.exe" || { echo "CULLMATCH GATE FAILED (g++)"; exit 1; }
# the fill kernel writes the depth image (a multisample image takes no buffer copy)
"$G" --target-env "$ENV" -V -S comp -o "$(W "$OUT/fill.spv")" "$(W "$ROOT/tests/hlslmatch/cullfill.comp")" > /dev/null \
	|| { echo "CULLMATCH GATE FAILED (glslang fill)"; exit 1; }
"$G" --target-env "$ENV" -V -S comp -DMS -o "$(W "$OUT/fillms.spv")" "$(W "$ROOT/tests/hlslmatch/cullfill.comp")" > /dev/null \
	|| { echo "CULLMATCH GATE FAILED (glslang fill ms)"; exit 1; }
bad=0
for k in "${KS[@]}"; do
	src=$k; def=""; hdef=()
	[ "$k" = hiz0ms ] && { src=hiz0; def="#define MS 1"; hdef=(-D MS); }
	RED="CELLGPUCULL_$(echo "$src" | tr a-z A-Z)_RED"
	H="$ROOT/res/shaders/hlsl/near/cellgpucull_${src}_cs.hlsl"
	if [ "$k" = bc7 ]; then
		# the file carries its own #version; a sparse checkout has no lib/: the tables come from the commit then
		RED=LODGEN_BC7_RED
		H="$ROOT/res/shaders/hlsl/far-lod/lodgen_bc7_cs.hlsl"
		TB="$ROOT/lib/libfo76utils/src/bptc-tables.c"
		[ -f "$TB" ] || { TB="$OUT/bptc-tables.c"; "${GIT:-git}" -C "$ROOT" show HEAD:lib/libfo76utils/src/bptc-tables.c > "$TB"; }
		python -I -X utf8 "$ROOT/tests/hlslmatch/bc7tables.py" "$(W "$TB")" "$(W "$ROOT/res/shaders/features/far-lod/lodgen_bc7.glsl")" \
			"$(W "$OUT/$k.comp")" "$(W "$H")" || { echo "CULLMATCH GATE FAILED (bc7 tables)"; exit 1; }
	else
		{ echo "#version 430 core"; [ -n "$def" ] && echo "$def"; cat "$ROOT/res/shaders/features/near/cellgpucull_$src.glsl"; } > "$OUT/$k.comp"
	fi
	"$G" --target-env "$ENV" -V -R --sdub LooseUniforms 0 31 --amb -S comp -o "$(W "$OUT/${k}_glsl.spv")" "$(W "$OUT/$k.comp")" > "$OUT/${k}_glslang.txt" \
		|| { cat "$OUT/${k}_glslang.txt"; echo "CULLMATCH GATE FAILED (glslang $k)"; exit 1; }
	"$DXC" -spirv -fspv-target-env="$ENV" -T cs_6_0 -E main -O0 "${hdef[@]}" -Fo "$(W "$OUT/${k}_hlsl.spv")" "$(W "$H")" || { echo "CULLMATCH GATE FAILED (dxc $k)"; exit 1; }
	"$DXC" -spirv -fspv-target-env="$ENV" -T cs_6_0 -E main -O0 "${hdef[@]}" -D "$RED" -Fo "$(W "$OUT/${k}_red.spv")" "$(W "$H")" || { echo "CULLMATCH GATE FAILED (dxc red $k)"; exit 1; }
	"$DXC" -T cs_6_0 -E main "${hdef[@]}" -Fo "$(W "$OUT/${k}.dxil")" "$(W "$H")" > /dev/null || { echo "CULLMATCH GATE FAILED (dxil $k)"; exit 1; }
	( cd "$OUT" && ./cullmatch.exe "$k" "${k}_glsl.spv" "${k}_hlsl.spv" > "${k}_green.txt" ); g=$?
	( cd "$OUT" && ./cullmatch.exe "$k" "${k}_glsl.spv" "${k}_red.spv" > "${k}_red.txt" ); r=$?
	cat "$OUT/${k}_green.txt"
	echo "red ($RED): $(tail -1 "$OUT/${k}_red.txt")"
	# red must fail as a mismatch (rc 1), never as a setup error (rc 3) or a silent kernel (rc 2)
	if [ $g -eq 0 ] && [ $r -eq 1 ]; then echo "ok   $k"; else echo "FAIL $k (green rc $g, red rc $r)"; bad=1; fi
done
[ $bad = 0 ] && echo "CULLMATCH GATE OK" || { echo "CULLMATCH GATE FAILED"; exit 1; }
