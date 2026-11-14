#!/bin/bash
# Lane VKFRAME1 (2026-11-12): compile the bloom chain's Fusion Shaders HLSL kernels (res/shaders/hlsl/post/bloom_*.hlsl) to
# SPIR-V C arrays in src/features/vkframe/bloom_NAME.spv.h, as src/features/gi/rtddgi_spirv.sh does for the volume.
# Commit the headers, so a build needs no shader compiler; run this after editing a bloom .hlsl or BloomCommon.hlsli.
# vulkan1.2 = bakert.cpp's device version. DXC= to override the compiler.
set -eu
ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
DXC="${DXC:-/e/Tools/dxc/bin/x64/dxc.exe}"
HLSL="$ROOT/res/shaders/hlsl/post"
cd "$ROOT/src/features/vkframe"
for k in down hold up src comb back; do
	"$DXC" -spirv -fspv-target-env=vulkan1.2 -T cs_6_0 -E main -Fo "bloom_${k}.spv" "$(cygpath -m "$HLSL/bloom_${k}.hlsl" 2>/dev/null || echo "$HLSL/bloom_${k}.hlsl")"
	python -I "$ROOT/tools/spv_header.py" "bloom_${k}.spv" "vkframe_bloom_${k}_spv" "bloom_${k}.spv.h" < /dev/null
	rm -f "bloom_${k}.spv"
done
# the cell tone map (P6): res/shaders/hlsl/post/tonemap.hlsl -> tonemap.spv.h
for t in tonemap; do
	"$DXC" -spirv -fspv-target-env=vulkan1.2 -T cs_6_0 -E main -Fo "${t}.spv" "$(cygpath -m "$HLSL/${t}.hlsl" 2>/dev/null || echo "$HLSL/${t}.hlsl")"
	python -I "$ROOT/tools/spv_header.py" "${t}.spv" "vkframe_${t}_spv" "${t}.spv.h" < /dev/null
	rm -f "${t}.spv"
done
# the exposure measure (P4): res/shaders/hlsl/exposure/<e>.hlsl -> <e>.spv.h
for e in measure_q; do
	"$DXC" -spirv -fspv-target-env=vulkan1.2 -T cs_6_0 -E main -Fo "${e}.spv" "$(cygpath -m "$ROOT/res/shaders/hlsl/exposure/${e}.hlsl" 2>/dev/null || echo "$ROOT/res/shaders/hlsl/exposure/${e}.hlsl")"
	python -I "$ROOT/tools/spv_header.py" "${e}.spv" "vkframe_${e}_spv" "${e}.spv.h" < /dev/null
	rm -f "${e}.spv"
done
# round 11: the tone pass on the present device's view (vertex + fragment; the name's tail is the profile)
# lane VKSHARE1: the Modern temporal AA on the view (vectors, resolve, RCAS copy-back)
for v in tone_view_vs tone_view_ps taa_view_mv_cs taa_view_resolve_cs taa_view_rcas_ps; do
	"$DXC" -spirv -fspv-target-env=vulkan1.2 -T "${v##*_}_6_0" -E main -Fo "${v}.spv" "$(cygpath -m "$HLSL/${v}.hlsl" 2>/dev/null || echo "$HLSL/${v}.hlsl")"
	python -I "$ROOT/tools/spv_header.py" "${v}.spv" "vkframe_${v}_spv" "${v}.spv.h" < /dev/null
	rm -f "${v}.spv"
done
# lane VKSKY1: the cell sky (atmo_sky.frag's HLSL twin) on the tone frame; registers shifted apart (b 0, t 64, s 128)
for s in sky_view_ps; do
	"$DXC" -spirv -fspv-target-env=vulkan1.2 -T ps_6_0 -E main -fvk-b-shift 0 0 -fvk-t-shift 64 0 -fvk-s-shift 128 0 -fvk-u-shift 192 0 -Fo "${s}.spv" "$(cygpath -m "$ROOT/res/shaders/hlsl/sky/atmo_sky_ps.hlsl" 2>/dev/null || echo "$ROOT/res/shaders/hlsl/sky/atmo_sky_ps.hlsl")"
	python -I "$ROOT/tools/spv_header.py" "${s}.spv" "vkframe_${s}_spv" "${s}.spv.h" < /dev/null
	rm -f "${s}.spv"
done
# lane VKLIGHT1: AO, contact shadows and the SSGI cast (res/shaders/hlsl/vklight), -O0 as tests/vklight/light.sh proved them
for l in ao_gbuf:ao_vk:AO_STAGE=5 ao_mip:ao_vk:AO_STAGE=1 ao_raw:ao_vk:AO_STAGE=2 ao_blur:ao_vk:AO_STAGE=3 ao_blurf:ao_vk:AO_STAGE=4 contact:contact_vk: ssgi:ssgi_vk:; do
	n="${l%%:*}"; r="${l#*:}"; f="${r%%:*}"; d="${r#*:}"
	"$DXC" -spirv -fspv-target-env=vulkan1.2 -T cs_6_0 -E main -O0 ${d:+-D "$d"} -Fo "light_${n}.spv" "$(cygpath -m "$ROOT/res/shaders/hlsl/vklight/${f}.hlsl" 2>/dev/null || echo "$ROOT/res/shaders/hlsl/vklight/${f}.hlsl")"
	python -I "$ROOT/tools/spv_header.py" "light_${n}.spv" "vkframe_light_${n}_spv" "light_${n}.spv.h" < /dev/null
	rm -f "light_${n}.spv"
done
ls -la light_ao_gbuf.spv.h light_ao_mip.spv.h light_ao_raw.spv.h light_ao_blur.spv.h light_ao_blurf.spv.h light_contact.spv.h light_ssgi.spv.h
# lane VKLIGHT2: the reflections (res/shaders/hlsl/vklight/ssr_vk.hlsl, SSR_LIVE) per stage; w = the water look twin
ssr() {
	"$DXC" -spirv -fspv-target-env=vulkan1.2 -T cs_6_0 -E main -DSSR_LIVE $2 -Fo "$1.spv" "$(cygpath -m "$ROOT/res/shaders/hlsl/vklight/ssr_vk.hlsl" 2>/dev/null || echo "$ROOT/res/shaders/hlsl/vklight/ssr_vk.hlsl")"
	python -I "$ROOT/tools/spv_header.py" "$1.spv" "vkframe_$1_spv" "$1.spv.h" < /dev/null
	rm -f "$1.spv"
}
ssr ssr_view_s1 "-DSSR_STAGE=1"
ssr ssr_view_s1w "-DSSR_STAGE=1 -DF4FX_SSR_WATER_LOOK"
ssr ssr_view_s2 "-DSSR_STAGE=2"
ssr ssr_view_s2w "-DSSR_STAGE=2 -DF4FX_SSR_WATER_LOOK"
ssr ssr_view_s3 "-DSSR_STAGE=3"
ssr ssr_view_s4 "-DSSR_STAGE=4"
ssr ssr_view_s5 "-DSSR_STAGE=5"
# lane VKSHADOW1: the sun cascades' queued casters (res/shaders/hlsl/near/csm_view_vs.hlsl), depth only
# lane VKOBJ1: the objects' opaque depth prepass (obj_depth_vs.hlsl)
for c in csm_view_vs obj_depth_vs; do
	"$DXC" -spirv -fspv-target-env=vulkan1.2 -T vs_6_0 -E main -Fo "${c}.spv" "$(cygpath -m "$ROOT/res/shaders/hlsl/near/${c}.hlsl" 2>/dev/null || echo "$ROOT/res/shaders/hlsl/near/${c}.hlsl")"
	python -I "$ROOT/tools/spv_header.py" "${c}.spv" "vkframe_${c}_spv" "${c}.spv.h" < /dev/null
	rm -f "${c}.spv"
done
# lane VKAIR1: the air's sky pass and the clouds' march + resolve on the view, registers shifted as the sky's
fx() {
	"$DXC" -spirv -fspv-target-env=vulkan1.2 -T ps_6_0 -E main -fvk-b-shift 0 0 -fvk-t-shift 64 0 -fvk-s-shift 128 0 -fvk-u-shift 192 0 -Fo "$1.spv" "$(cygpath -m "$ROOT/res/shaders/hlsl/$2.hlsl" 2>/dev/null || echo "$ROOT/res/shaders/hlsl/$2.hlsl")"
	python -I "$ROOT/tools/spv_header.py" "$1.spv" "vkframe_$1_spv" "$1.spv.h" < /dev/null
	rm -f "$1.spv"
}
# lane HLSLSRC2: the views compile their frozen spirv-cross twins (<dir>/<name>.hlsl, the base files' bytes): the readable
# one-file sources took the programs' names, and kT_* in vkpresent.h holds the twins' layout
fx volsky_view_ps air/volsky_view_ps
fx cloud_march_view_ps clouds/cloud_march_view_ps
fx cloud_march_a_view_ps clouds/cloud_march_a_view_ps
fx cloud_resolve_view_ps clouds/cloud_resolve_view_ps
fx cloud_resolve_a_view_ps clouds/cloud_resolve_a_view_ps
# lane VKFAR1: the far bulb dots as instanced quads; the vertex stage with fx()'s shifts (its block is b1)
fxv() {
	"$DXC" -spirv -fspv-target-env=vulkan1.2 -T vs_6_0 -E main -fvk-b-shift 0 0 -fvk-t-shift 64 0 -fvk-s-shift 128 0 -fvk-u-shift 192 0 -Fo "$1.spv" "$(cygpath -m "$ROOT/res/shaders/hlsl/$2.hlsl" 2>/dev/null || echo "$ROOT/res/shaders/hlsl/$2.hlsl")"
	python -I "$ROOT/tools/spv_header.py" "$1.spv" "vkframe_$1_spv" "$1.spv.h" < /dev/null
	rm -f "$1.spv"
}
fxv fardots_view_vs far-lod/cell_fardots_view_vs
fx fardots_view_ps far-lod/fardots_view_ps
# lane VKFAR1 round 2: the far cards' pixel stage (26 textures / texel buffers, b0 + b31), fx()'s shifts; its set is reflected
# from the module, so every declared binding stays in it (-fspv-preserve-bindings) and GL's binding list maps one to one
fxk() {
	"$DXC" -spirv -fspv-target-env=vulkan1.2 -T ps_6_0 -E main -fvk-b-shift 0 0 -fvk-t-shift 64 0 -fvk-s-shift 128 0 -fvk-u-shift 192 0 -fspv-preserve-bindings -Fo "$1.spv" "$(cygpath -m "$ROOT/res/shaders/hlsl/$2.hlsl" 2>/dev/null || echo "$ROOT/res/shaders/hlsl/$2.hlsl")"
	python -I "$ROOT/tools/spv_header.py" "$1.spv" "vkframe_$1_spv" "$1.spv.h" < /dev/null
	rm -f "$1.spv"
}
fxk farcards_view_ps far-lod/farcards_view_ps
# lane VKWATER3: the shaded water (fo4_waterlook, the app's FOGLEAK + FRAME_UBO variant) through fxk(); its vertex stage
# (fo4_default) on its own shifts: its b0 / b1 / b31 / t0 would land on the pixel stage's under fx()'s (blocks at 256 + n:
# b31 at 40 + 31 = 71 was the pixel stage's t7). Both are frozen spirv-cross twins under the view's name (HLSLSRC2's rule):
# vkwater_tables.h holds their layout. fxkw() = fxk() + -fvk-use-dx-position-w: the twins carry spirv-cross's D3D fixup
# (gl_FragCoord.w = 1.0 / gl_FragCoord.w), so SV_Position.w must hold D3D's clip w, not Vulkan's 1 / w (without the flag
# the fixup inverts twice and the scene-depth path's fragW = 1 / gl_FragCoord.w misplaces every scene point)
fxkw() {
	"$DXC" -spirv -fspv-target-env=vulkan1.2 -T ps_6_0 -E main -fvk-b-shift 0 0 -fvk-t-shift 64 0 -fvk-s-shift 128 0 -fvk-u-shift 192 0 -fspv-preserve-bindings -fvk-use-dx-position-w -Fo "$1.spv" "$(cygpath -m "$ROOT/res/shaders/hlsl/$2.hlsl" 2>/dev/null || echo "$ROOT/res/shaders/hlsl/$2.hlsl")"
	python -I "$ROOT/tools/spv_header.py" "$1.spv" "vkframe_$1_spv" "$1.spv.h" < /dev/null
	rm -f "$1.spv"
}
fxkw water_view_ps water/water_view_ps
fxw() {
	"$DXC" -spirv -fspv-target-env=vulkan1.2 -T vs_6_0 -E main -fvk-b-shift 256 0 -fvk-t-shift 110 0 -fvk-s-shift 180 0 -fvk-u-shift 220 0 -fspv-preserve-bindings -Fo "$1.spv" "$(cygpath -m "$ROOT/res/shaders/hlsl/$2.hlsl" 2>/dev/null || echo "$ROOT/res/shaders/hlsl/$2.hlsl")"
	python -I "$ROOT/tools/spv_header.py" "$1.spv" "vkframe_$1_spv" "$1.spv.h" < /dev/null
	rm -f "$1.spv"
}
fxw water_view_vs core/water_view_vs
# lane VKWATER3 item 4: the tessellated water (fo4_waterlookd: fo4_waterdisp.tesc / .tese) as water_view_hs + water_view_ds,
# each on its own shifts (hs blocks at 512 + n, ds at 768 + n, their images / samplers past them) so no binding meets
# another stage's in the one set; its pixel stage (a frozen twin) through fxk()
fxh() {
	"$DXC" -spirv -fspv-target-env=vulkan1.2 -T hs_6_0 -E main -fvk-b-shift 512 0 -fvk-t-shift 576 0 -fvk-s-shift 640 0 -fvk-u-shift 704 0 -fspv-preserve-bindings -Fo "$1.spv" "$(cygpath -m "$ROOT/res/shaders/hlsl/$2.hlsl" 2>/dev/null || echo "$ROOT/res/shaders/hlsl/$2.hlsl")"
	python -I "$ROOT/tools/spv_header.py" "$1.spv" "vkframe_$1_spv" "$1.spv.h" < /dev/null
	rm -f "$1.spv"
}
fxh water_view_hs water/fo4_waterdisp_hs
# the ds with -Gis (no contraction): GL's displaced vertices are unfused (VKWATER3 seed 3: 31 -> 2 D24 units; NV GL's
# sqrt / dot / mix / smoothstep codegen in the HLSL takes it to 0)
fxd() {
	"$DXC" -spirv -fspv-target-env=vulkan1.2 -T ds_6_0 -E main -fvk-b-shift 768 0 -fvk-t-shift 832 0 -fvk-s-shift 896 0 -fvk-u-shift 960 0 -fspv-preserve-bindings -Gis -Fo "$1.spv" "$(cygpath -m "$ROOT/res/shaders/hlsl/$2.hlsl" 2>/dev/null || echo "$ROOT/res/shaders/hlsl/$2.hlsl")"
	python -I "$ROOT/tools/spv_header.py" "$1.spv" "vkframe_$1_spv" "$1.spv.h" < /dev/null
	rm -f "$1.spv"
}
fxd water_view_ds water/fo4_waterdisp_ds
fxkw water_view_d_ps water/water_view_d_ps
# item 4 piece 2: the Medium / ray-pass look (fo4_waterlookm), the probes' looks (fo4_waterlookp, tessellated _dp)
fxkw water_view_m_ps water/water_view_m_ps
fxkw water_view_p_ps water/water_view_p_ps
fxkw water_view_dp_ps water/water_view_dp_ps
# lane VKOBJ4: the shift sets by stage. A pipeline whose vertex stage declares resources takes VS_SHIFT for it: on PS_SHIFT the
# bl vertex stage's b0 (skinning) lands on the pixel stage's b0 (global). fxv() keeps PS_SHIFT (fardots' vertex block is b1).
PS_SHIFT="-fvk-b-shift 0 0 -fvk-t-shift 64 0 -fvk-s-shift 128 0 -fvk-u-shift 192 0"
VS_SHIFT="-fvk-b-shift 256 0 -fvk-t-shift 320 0 -fvk-s-shift 384 0 -fvk-u-shift 448 0"
# the bl twins (res/shaders/hlsl/near, -D WW_VK_BINDLESS), every binding kept: objBlPipe reflects them (vkobjpipe.inc)
fxb() {
	"$DXC" -spirv -fspv-target-env=vulkan1.2 -HV 2021 -T "$3" -E main -D WW_VK_BINDLESS $4 -fspv-preserve-bindings -Fo "$1.spv" "$(cygpath -m "$ROOT/res/shaders/hlsl/near/$2.hlsl" 2>/dev/null || echo "$ROOT/res/shaders/hlsl/near/$2.hlsl")"
	python -I "$ROOT/tools/spv_header.py" "$1.spv" "vkframe_$1_spv" "$1.spv.h" < /dev/null
	rm -f "$1.spv"
}
fxb objbl_vs fo4_defaultbl_vs vs_6_0 "$VS_SHIFT"
fxb objbl_cell_ps fo4_cellbl_ps ps_6_0 "$PS_SHIFT"
fxb objbl_cellcsm_ps fo4_cellcsmbl_ps ps_6_0 "$PS_SHIFT"
# the cell frame's per-sample readback (objBlFrameSamples): its own bindings, no shift
"$DXC" -spirv -fspv-target-env=vulkan1.2 -T cs_6_0 -E main -Fo objbl_samples_cs.spv "$(cygpath -m "$ROOT/res/shaders/hlsl/near/objbl_samples_cs.hlsl" 2>/dev/null || echo "$ROOT/res/shaders/hlsl/near/objbl_samples_cs.hlsl")"
python -I "$ROOT/tools/spv_header.py" objbl_samples_cs.spv vkframe_objbl_samples_cs_spv objbl_samples_cs.spv.h < /dev/null
rm -f objbl_samples_cs.spv
ls -la csm_view_vs.spv.h obj_depth_vs.spv.h sky_view_ps.spv.h bloom_down.spv.h bloom_hold.spv.h bloom_up.spv.h bloom_src.spv.h bloom_comb.spv.h bloom_back.spv.h tonemap.spv.h measure_q.spv.h tone_view_vs.spv.h tone_view_ps.spv.h taa_view_mv_cs.spv.h taa_view_resolve_cs.spv.h taa_view_rcas_ps.spv.h
