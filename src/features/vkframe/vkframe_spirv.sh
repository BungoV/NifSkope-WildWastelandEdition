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
# lane VKWATER4 (row G5): the ultra ocean, ocean_vk.hlsl stages 1-6 (one line: tests/spvsync/check_spv.py reads it)
for l in ao_gbuf:ao_vk:AO_STAGE=5 ao_mip:ao_vk:AO_STAGE=1 ao_raw:ao_vk:AO_STAGE=2 ao_blur:ao_vk:AO_STAGE=3 ao_blurf:ao_vk:AO_STAGE=4 contact:contact_vk: ssgi:ssgi_vk: aox_z:aox_vk:AOX_KERNEL=1 aox_xe:aox_vk:AOX_KERNEL=2 aox_r8:aox_vk:AOX_KERNEL=3 ocean_spec:ocean_vk:OCEAN_STAGE=1 ocean_fft:ocean_vk:OCEAN_STAGE=2 ocean_finbc:ocean_vk:OCEAN_STAGE=3 ocean_find:ocean_vk:OCEAN_STAGE=4 ocean_mipbc:ocean_vk:OCEAN_STAGE=5 ocean_mipd:ocean_vk:OCEAN_STAGE=6 ocean_finbca:ocean_vk:OCEAN_STAGE=7 ocean_finda:ocean_vk:OCEAN_STAGE=8 ocean_mipbca:ocean_vk:OCEAN_STAGE=9 ocean_mipda:ocean_vk:OCEAN_STAGE=10 cfar_min:contactfar_vk:CFAR_STAGE=1 cfar_march:contactfar_vk:CFAR_STAGE=2 cfar_comp:contactfar_vk:CFAR_STAGE=3; do
	n="${l%%:*}"; r="${l#*:}"; f="${r%%:*}"; d="${r#*:}"
	"$DXC" -spirv -fspv-target-env=vulkan1.2 -T cs_6_0 -E main -O0 ${d:+-D "$d"} -Fo "light_${n}.spv" "$(cygpath -m "$ROOT/res/shaders/hlsl/vklight/${f}.hlsl" 2>/dev/null || echo "$ROOT/res/shaders/hlsl/vklight/${f}.hlsl")"
	python -I "$ROOT/tools/spv_header.py" "light_${n}.spv" "vkframe_light_${n}_spv" "light_${n}.spv.h" < /dev/null
	rm -f "light_${n}.spv"
done
# lane VKFAR3 (A1 b): the screen GI's DDGI read (gihalf_vk.hlsl), the app's variant (cell_gihalf with cascades), -O0 like the rest
"$DXC" -spirv -fspv-target-env=vulkan1.2 -T cs_6_0 -E main -O0 -D WW_CELL_FOGLEAK=1 -D WW_CELL_RTDDGI=1 -D WW_CELL_RTDDGI_CASCADES=1 -D WW_FRAME_UBO=1 -Fo "light_gihalf.spv" "$(cygpath -m "$ROOT/res/shaders/hlsl/vklight/gihalf_vk.hlsl" 2>/dev/null || echo "$ROOT/res/shaders/hlsl/vklight/gihalf_vk.hlsl")"
python -I "$ROOT/tools/spv_header.py" "light_gihalf.spv" "vkframe_light_gihalf_spv" "light_gihalf.spv.h" < /dev/null
rm -f "light_gihalf.spv"
ls -la light_ao_gbuf.spv.h light_ao_mip.spv.h light_ao_raw.spv.h light_ao_blur.spv.h light_ao_blurf.spv.h light_contact.spv.h light_ssgi.spv.h light_aox_z.spv.h light_aox_xe.spv.h light_aox_r8.spv.h light_gihalf.spv.h light_cfar_min.spv.h light_cfar_march.spv.h light_cfar_comp.spv.h
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
# lane VKSHADOW6: the cascade cache's Scroll remap (csm_remap_view_vs / _ps.hlsl); the profile is the name's suffix
# lane VKSHADOW8: the cascades' alpha-tested casters (csm_at_view_vs / _ps.hlsl)
for c in csm_view_vs obj_depth_vs csm_remap_view_vs csm_remap_view_ps csm_at_view_vs csm_at_view_ps csm_sway_view_vs; do
	"$DXC" -spirv -fspv-target-env=vulkan1.2 -T ${c##*_}_6_0 -E main -Fo "${c}.spv" "$(cygpath -m "$ROOT/res/shaders/hlsl/near/${c}.hlsl" 2>/dev/null || echo "$ROOT/res/shaders/hlsl/near/${c}.hlsl")"
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
# lane VKCLOUD2: the march with the fly-through box (the readable cloud_fly / cloud_fly_a sources; kT_fly / kT_fly_a);
# lane VKCLOUD3: through their view entries (NV GL's float forms, WW_VK_NVGL_FORMS)
fx cloud_fly_view_ps clouds/cloud_fly_view_ps
fx cloud_fly_a_view_ps clouds/cloud_fly_a_view_ps
# lane VKCLOUD4: the fly-through box's occupancy bake and distance field (cloudfly.cpp runPass; kT_flybake / kT_flysdf)
fx cloud_flybake_view_ps clouds/cloud_flybake_ps
fx cloud_flysdf_view_ps clouds/cloud_flysdf_ps
# lane VKFAR1: the far bulb dots as instanced quads; the vertex stage with fx()'s shifts (its block is b1)
fxv() {
	"$DXC" -spirv -fspv-target-env=vulkan1.2 -T vs_6_0 -E main -fvk-b-shift 0 0 -fvk-t-shift 64 0 -fvk-s-shift 128 0 -fvk-u-shift 192 0 -Fo "$1.spv" "$(cygpath -m "$ROOT/res/shaders/hlsl/$2.hlsl" 2>/dev/null || echo "$ROOT/res/shaders/hlsl/$2.hlsl")"
	python -I "$ROOT/tools/spv_header.py" "$1.spv" "vkframe_$1_spv" "$1.spv.h" < /dev/null
	rm -f "$1.spv"
}
fxv fardots_view_vs far-lod/cell_fardots_view_vs
fx fardots_view_ps far-lod/fardots_view_ps
# lane VKFAR2 (P4b): the fog bloom's own pass, a hand-kept readable twin of cell_fogbloom (the depth and stencil read per sample)
fx fogbloom_view_ps post/fogbloom_view_ps
# lane VKFAR2 (P8): the clouds in front of the scene (cloud_composite cvFront 2), a frozen copy of the readable source
fx cloud_composite_view_ps clouds/cloud_composite_view_ps
# lane VKFAR1 round 2: the far cards' pixel stage (26 textures / texel buffers, b0 + b31), fx()'s shifts; its set is reflected
# from the module, so every declared binding stays in it (-fspv-preserve-bindings) and GL's binding list maps one to one
fxk() {
	"$DXC" -spirv -fspv-target-env=vulkan1.2 -T ps_6_0 -E main -fvk-b-shift 0 0 -fvk-t-shift 64 0 -fvk-s-shift 128 0 -fvk-u-shift 192 0 -fspv-preserve-bindings -Fo "$1.spv" "$(cygpath -m "$ROOT/res/shaders/hlsl/$2.hlsl" 2>/dev/null || echo "$ROOT/res/shaders/hlsl/$2.hlsl")"
	python -I "$ROOT/tools/spv_header.py" "$1.spv" "vkframe_$1_spv" "$1.spv.h" < /dev/null
	rm -f "$1.spv"
}
fxk farcards_view_ps far-lod/farcards_view_ps
# lane VKFAR2: the far cards' vertex stage shares the pixel stage's set: b0 at 0 (both stages), its own b31 at 32 and
# its texel buffers cardData / cardFrames at 96 / 97, clear of the pixel stage's 31 and 64..89
fxc() {
	"$DXC" -spirv -fspv-target-env=vulkan1.2 -T vs_6_0 -E main -fvk-bind-register b0 0 0 0 -fvk-bind-register b31 0 32 0 -fvk-bind-register t0 0 96 0 -fvk-bind-register t1 0 97 0 -fspv-preserve-bindings -Fo "$1.spv" "$(cygpath -m "$ROOT/res/shaders/hlsl/$2.hlsl" 2>/dev/null || echo "$ROOT/res/shaders/hlsl/$2.hlsl")"
	python -I "$ROOT/tools/spv_header.py" "$1.spv" "vkframe_$1_spv" "$1.spv.h" < /dev/null
	rm -f "$1.spv"
}
fxc farcards_view_vs far-lod/farcards_view_vs
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
# lane VKFAR2: the far LOD rings (terrain chunks, .lodi objects): fo4_cellcsm's FOGLEAK + FRAME_UBO twin through fxk()
# (t0..t46 at 64..110); fo4_default's twin on fxw()'s shifts but t at 120, so its farStream clears the pixel stage's t46
fxk farlod_view_ps core/farlod_view_ps
fxl() {
	"$DXC" -spirv -fspv-target-env=vulkan1.2 -T vs_6_0 -E main -fvk-b-shift 256 0 -fvk-t-shift 120 0 -fvk-s-shift 180 0 -fvk-u-shift 220 0 -fspv-preserve-bindings -Fo "$1.spv" "$(cygpath -m "$ROOT/res/shaders/hlsl/$2.hlsl" 2>/dev/null || echo "$ROOT/res/shaders/hlsl/$2.hlsl")"
	python -I "$ROOT/tools/spv_header.py" "$1.spv" "vkframe_$1_spv" "$1.spv.h" < /dev/null
	rm -f "$1.spv"
}
fxl farlod_view_vs core/water_view_vs
# lane VKFAR2 G2: the sky bodies in the backdrop pass after the sky. The readable sources compile as they stand: the stars
# (lookdev_clouds), the moon globe (lookdev_bill + sky_moon). Pixel stages through fxk(); vertex stages share the set:
# b0 at 0 and their own b31 at 32 (vkskybody.h holds all four blocks' layouts)
fxb() {
	"$DXC" -spirv -fspv-target-env=vulkan1.2 -T vs_6_0 -E main -fvk-bind-register b0 0 0 0 -fvk-bind-register b31 0 32 0 -fspv-preserve-bindings -Fo "$1.spv" "$(cygpath -m "$ROOT/res/shaders/hlsl/$2.hlsl" 2>/dev/null || echo "$ROOT/res/shaders/hlsl/$2.hlsl")"
	python -I "$ROOT/tools/spv_header.py" "$1.spv" "vkframe_$1_spv" "$1.spv.h" < /dev/null
	rm -f "$1.spv"
}
fxb skystars_view_vs clouds/lookdev_clouds_vs
fxk skystars_view_ps clouds/lookdev_clouds_ps
fxb skymoon_view_vs core/lookdev_bill_vs
fxk skymoon_view_ps sky/sky_moon_ps
# lane VKCLOUD4: the cloud map (cloudmap.cpp buildMap, vkcloudmap.inc), the readable sources themselves: the vertex stage
# here (its b31 at 32), the pixel stage on fx()'s shifts (its b31 at 31) below
fxb cloudmap_view_vs clouds/cloudmap_vs
ls -la skystars_view_vs.spv.h skystars_view_ps.spv.h skymoon_view_vs.spv.h skymoon_view_ps.spv.h
# lane VKFAR2 P9: the lens-flare sprites in the tone pass (vkflare.inc): lens_flare.hlsl's pixel entry as it stands, its
# vertex entry through lens_flare_view_vs.hlsl (renamed main). PostFxBindingsVk.hlsli's own bindings, unused ones stripped
fxf() {
	"$DXC" -spirv -fspv-target-env=vulkan1.2 -T $3 -E main -Fo "$1.spv" "$(cygpath -m "$ROOT/res/shaders/hlsl/$2.hlsl" 2>/dev/null || echo "$ROOT/res/shaders/hlsl/$2.hlsl")"
	python -I "$ROOT/tools/spv_header.py" "$1.spv" "vkframe_$1_spv" "$1.spv.h" < /dev/null
	rm -f "$1.spv"
}
fxf lensflare_view_vs post/lens_flare_view_vs vs_6_0
fxf lensflare_view_ps post/lens_flare ps_6_0
ls -la lensflare_view_vs.spv.h lensflare_view_ps.spv.h
# lane VKFAR2 (the froxel item): the air's integrate as compute (res/shaders/hlsl/air/volair_integ_cs.hlsl, vkfog.inc), the
# RGBA16F volume and the f32 red's RGBA32F one
fog() {
	"$DXC" -spirv -fspv-target-env=vulkan1.2 -T cs_6_0 -E main $2 -Fo "$1.spv" "$(cygpath -m "$ROOT/res/shaders/hlsl/air/volair_integ_cs.hlsl" 2>/dev/null || echo "$ROOT/res/shaders/hlsl/air/volair_integ_cs.hlsl")"
	python -I "$ROOT/tools/spv_header.py" "$1.spv" "vkframe_$1_spv" "$1.spv.h" < /dev/null
	rm -f "$1.spv"
}
fog fog_integ_cs ""
fog fog_integ32_cs "-DVOLAIR_INTEG_F32"
ls -la fog_integ_cs.spv.h fog_integ32_cs.spv.h
# lane VKFAR2 P7: cellpost.cpp's depth of field, motion blur and object blur in the tone pass (vkpost7.inc): the kernels as
# they stand, a few twice under their defines (the 4th word, unquoted into dxc)
pfx() {
	"$DXC" -spirv -fspv-target-env=vulkan1.2 -T $3 -E main $4 -Fo "$1.spv" "$(cygpath -m "$ROOT/res/shaders/hlsl/$2.hlsl" 2>/dev/null || echo "$ROOT/res/shaders/hlsl/$2.hlsl")"
	python -I "$ROOT/tools/spv_header.py" "$1.spv" "vkframe_$1_spv" "$1.spv.h" < /dev/null
	rm -f "$1.spv"
}
pfx p7_res post/post7_resolve cs_6_0 ""
pfx p7_dep post/post7_resolve cs_6_0 "-DPOST7_DEPTH"
pfx p7_copy_ps post/post7_copy_ps ps_6_0 ""
pfx p7_dof_down post/dof_down cs_6_0 ""
pfx p7_dof_blur post/dof_blur cs_6_0 ""
pfx p7_dof_comp post/dof_comp cs_6_0 ""
pfx p7_lens_comp post/dof_comp cs_6_0 "-DDOF_LENS"
pfx p7_lens_prep post/doflens_prep cs_6_0 ""
pfx p7_lens_tile post/doflens_tile cs_6_0 ""
pfx p7_lens_gather post/doflens_gather cs_6_0 ""
pfx p7_mblur post/mblur cs_6_0 ""
pfx p7_dofmblur post/mblur cs_6_0 "-DMBLUR_FUSE"
pfx p7_lensmblur post/mblur cs_6_0 "-DMBLUR_FUSE -DDOF_LENS"
pfx p7_mv post/taa_mv cs_6_0 "-DTAA_MV_RG16"
pfx p7_ob_vec post/objblur_vec cs_6_0 ""
pfx p7_ob_tile post/objblur_tile cs_6_0 ""
pfx p7_ob post/objblur cs_6_0 ""
ls -la p7_*.spv.h
# lane VKFAR2 L8 slice 1: rain / snow without a geometry stage (vkprecip.inc): the per-particle compute pass, the instanced
# corners and cell_precip_gpu_ps.hlsl's frozen copy, all on fx()'s shifts (the compute and graphics sets are separate)
fxs() {
	"$DXC" -spirv -fspv-target-env=vulkan1.2 -T cs_6_0 -E main -fvk-b-shift 0 0 -fvk-t-shift 64 0 -fvk-s-shift 128 0 -fvk-u-shift 192 0 -Fo "$1.spv" "$(cygpath -m "$ROOT/res/shaders/hlsl/$2.hlsl" 2>/dev/null || echo "$ROOT/res/shaders/hlsl/$2.hlsl")"
	python -I "$ROOT/tools/spv_header.py" "$1.spv" "vkframe_$1_spv" "$1.spv.h" < /dev/null
	rm -f "$1.spv"
}
fxs precip_view_cs weather/precip_view_cs
# lane VKFAR3: the DDGI atlas read through the present device's view (gate)
fxs ddgi_atlas_fetch_cs rtddgi/ddgi_atlas_fetch_cs
fxv precip_view_vs weather/precip_view_vs
fx precip_view_ps weather/precip_view_ps
# slice 3: the same three with the precipitation look (cell_precip_look.prog)
fxs precip_look_view_cs weather/precip_look_view_cs
fxv precip_look_view_vs weather/precip_look_view_vs
fx precip_look_view_ps weather/precip_look_view_ps
# slice 2b: the record pass lit at the centre (the plain particles' vs / ps unchanged)
fxs precip_lit_view_cs weather/precip_lit_view_cs
ls -la precip_lit_view_cs.spv.h
# lane VKFAR11 (L8 route lift 3): the lit pass in GL's ReSTIR variant (WW_CELL_RESTIR: the lamps' traced ratio)
fxs precip_lit_rt_view_cs weather/precip_lit_rt_view_cs
ls -la precip_lit_rt_view_cs.spv.h
ls -la precip_view_cs.spv.h precip_view_vs.spv.h precip_view_ps.spv.h precip_look_view_cs.spv.h precip_look_view_vs.spv.h precip_look_view_ps.spv.h
# lane VKOBJ4: the shift sets by stage. A pipeline whose vertex stage declares resources takes VS_SHIFT for it: on PS_SHIFT the
# bl vertex stage's b0 (skinning) lands on the pixel stage's b0 (global). fxv() keeps PS_SHIFT (fardots' vertex block is b1).
PS_SHIFT="-fvk-b-shift 0 0 -fvk-t-shift 64 0 -fvk-s-shift 128 0 -fvk-u-shift 192 0"
VS_SHIFT="-fvk-b-shift 256 0 -fvk-t-shift 320 0 -fvk-s-shift 384 0 -fvk-u-shift 448 0"
# the bl twins (res/shaders/hlsl/near, -D WW_VK_BINDLESS), every binding kept: objBlPipe reflects them (vkobjpipe.inc);
# $5: a variant's other loader defines (the pixel twins are readable wrappers of core/fo4_default_ps.hlsl, VKWATER3 G4)
# lane VKSMALL1: named fxn(), not fxb(): a second fxb() here ran every later sky-body style `fxb` line (taareactive) with 2 args
fxn() {
	"$DXC" -spirv -fspv-target-env=vulkan1.2 -HV 2021 -T "$3" -E main -D WW_VK_BINDLESS $4 ${5:-} -fspv-preserve-bindings -Fo "$1.spv" "$(cygpath -m "$ROOT/res/shaders/hlsl/near/$2.hlsl" 2>/dev/null || echo "$ROOT/res/shaders/hlsl/near/$2.hlsl")"
	python -I "$ROOT/tools/spv_header.py" "$1.spv" "vkframe_$1_spv" "$1.spv.h" < /dev/null
	rm -f "$1.spv"
}
fxn objbl_vs fo4_defaultbl_vs vs_6_0 "$VS_SHIFT"
# the bl pair on the app's key (WW_CELL_FOGLEAK + WW_FRAME_UBO, as the loader defines it for GL; no define: the plain variant)
fxn objbl_cell_ps fo4_cellbl_ps ps_6_0 "$PS_SHIFT" "-D WW_CELL_FOGLEAK=1 -D WW_FRAME_UBO=1"
fxn objbl_cellcsm_ps fo4_cellcsmbl_ps ps_6_0 "$PS_SHIFT" "-D WW_CELL_FOGLEAK=1 -D WW_FRAME_UBO=1"
# the all-on route's RT GI variant of the bl pair
fxn objbl_cell_rt_ps fo4_cellbl_ps ps_6_0 "$PS_SHIFT" "-D WW_CELL_FOGLEAK=1 -D WW_FRAME_UBO=1 -D WW_CELL_RTDDGI=1 -D WW_CELL_RTDDGI_CASCADES=1 -D WW_CELL_RESTIR=1"
fxn objbl_cellcsm_rt_ps fo4_cellcsmbl_ps ps_6_0 "$PS_SHIFT" "-D WW_CELL_FOGLEAK=1 -D WW_FRAME_UBO=1 -D WW_CELL_RTDDGI=1 -D WW_CELL_RTDDGI_CASCADES=1 -D WW_CELL_RESTIR=1"
# lane VKWATER3 (G4 by draws): the non-bindless fallbacks fo4_cell / fo4_cellcsm / pbrm_default (res/shaders/hlsl/core) on
# the app's base variant (WW_CELL_FOGLEAK + WW_FRAME_UBO; the app site refuses any other), every binding kept for
# objBlPipe; the frozen pixel stages invert FragCoord.w themselves (-fvk-use-dx-position-w). The alpha tests discard after
# the last texture sample (killPx in the HLSL): a discard before it left NV GL and NV Vulkan different derivatives in part-killed
# quads (3398 values over the bound in the G4 alpha pass, 11-14; demote gave the same 3398)
# $5: a variant's other loader defines (-D WW_X=1 ...), the app site's define key matches them
fxc() {
	"$DXC" -spirv -fspv-target-env=vulkan1.2 -HV 2021 -T "$3" -E main -D WW_CELL_FOGLEAK=1 -D WW_FRAME_UBO=1 $4 ${5:-} -fspv-preserve-bindings -fvk-use-dx-position-w -Fo "$1.spv" "$(cygpath -m "$ROOT/res/shaders/hlsl/core/$2.hlsl" 2>/dev/null || echo "$ROOT/res/shaders/hlsl/core/$2.hlsl")"
	python -I "$ROOT/tools/spv_header.py" "$1.spv" "vkframe_$1_spv" "$1.spv.h" < /dev/null
	rm -f "$1.spv"
}
fxc objcell_vs fo4_default_vs vs_6_0 "$VS_SHIFT"
fxc objcell_cell_ps fo4_cell_ps ps_6_0 "$PS_SHIFT"
fxc objcell_cellcsm_ps fo4_cellcsm_ps ps_6_0 "$PS_SHIFT"
# pbrm_default.prog is fo4_default.vert + pbrm_default.frag: objcell_vs is its vertex twin too
fxc objcell_pbrm_ps pbrm_default_ps ps_6_0 "$PS_SHIFT"
# its cell-lit programs (pbrm_cell.prog, pbrm_cellcsm.prog: pbrm_default.frag with WW_CELLLIGHTS, and WW_SUNSHADOW)
fxc objcell_pbrmcell_ps pbrm_cell_ps ps_6_0 "$PS_SHIFT"
fxc objcell_pbrmcellcsm_ps pbrm_cellcsm_ps ps_6_0 "$PS_SHIFT"
# G4 rest: the cell-lit effects (fo4_effectcell.prog with the air zones, which ship on; fo4_effectdecalcsm.prog, whose code
# the air zones do not touch), fo4_default.vert's programs too
fxc objcell_effectcell_ps fo4_effectcell_ps ps_6_0 "$PS_SHIFT" "-D WW_CELL_AIRZONE=1"
fxc objcell_effectdecalcsm_ps fo4_effectdecalcsm_ps ps_6_0 "$PS_SHIFT"
# lane VKOBJ8: fo4_effectdecal.prog (twin 10), the same decal without the cascades (the cell view's pick while no cascade map is built)
fxc objcell_effectdecal_ps fo4_effectdecal_ps ps_6_0 "$PS_SHIFT"
# the all-on route's RT GI variant (WW_CELL_RTDDGI + RTDDGI_CASCADES + RESTIR) of each twin (pbrm_default's code is the same)
fxc objcell_cell_rt_ps fo4_cell_ps ps_6_0 "$PS_SHIFT" "-D WW_CELL_RTDDGI=1 -D WW_CELL_RTDDGI_CASCADES=1 -D WW_CELL_RESTIR=1"
fxc objcell_cellcsm_rt_ps fo4_cellcsm_ps ps_6_0 "$PS_SHIFT" "-D WW_CELL_RTDDGI=1 -D WW_CELL_RTDDGI_CASCADES=1 -D WW_CELL_RESTIR=1"
fxc objcell_pbrmcell_rt_ps pbrm_cell_ps ps_6_0 "$PS_SHIFT" "-D WW_CELL_RTDDGI=1 -D WW_CELL_RTDDGI_CASCADES=1 -D WW_CELL_RESTIR=1"
fxc objcell_pbrmcellcsm_rt_ps pbrm_cellcsm_ps ps_6_0 "$PS_SHIFT" "-D WW_CELL_RTDDGI=1 -D WW_CELL_RTDDGI_CASCADES=1 -D WW_CELL_RESTIR=1"
fxc objcell_effectcell_rt_ps fo4_effectcell_ps ps_6_0 "$PS_SHIFT" "-D WW_CELL_AIRZONE=1 -D WW_CELL_RTDDGI=1 -D WW_CELL_RTDDGI_CASCADES=1 -D WW_CELL_RESTIR=1"
fxc objcell_effectdecalcsm_rt_ps fo4_effectdecalcsm_ps ps_6_0 "$PS_SHIFT" "-D WW_CELL_RTDDGI=1 -D WW_CELL_RTDDGI_CASCADES=1 -D WW_CELL_RESTIR=1"
fxc objcell_effectdecal_rt_ps fo4_effectdecal_ps ps_6_0 "$PS_SHIFT" "-D WW_CELL_RTDDGI=1 -D WW_CELL_RTDDGI_CASCADES=1 -D WW_CELL_RESTIR=1"
# the cell frame's per-sample readback (objBlFrameSamples): its own bindings, no shift
"$DXC" -spirv -fspv-target-env=vulkan1.2 -T cs_6_0 -E main -Fo objbl_samples_cs.spv "$(cygpath -m "$ROOT/res/shaders/hlsl/near/objbl_samples_cs.hlsl" 2>/dev/null || echo "$ROOT/res/shaders/hlsl/near/objbl_samples_cs.hlsl")"
python -I "$ROOT/tools/spv_header.py" objbl_samples_cs.spv vkframe_objbl_samples_cs_spv objbl_samples_cs.spv.h < /dev/null
rm -f objbl_samples_cs.spv
# lane VKFAR3 (G3): the far ground's ring-0 terrain (vkfarground.inc). The pixel stage is fo4_cellcsmfgl's readable source under
# the app's variant (FOGLEAK + FRAME_UBO) on fxk()'s shifts: images 64..126, samplers 128..190. Its vertex stage is
# water_view_vs's source with t at 200 and s at 216, past both (every declared binding is in the merged set, used or not)
fxg() {
	"$DXC" -spirv -fspv-target-env=vulkan1.2 -T ps_6_0 -E main -fvk-b-shift 0 0 -fvk-t-shift 64 0 -fvk-s-shift 128 0 -fvk-u-shift 192 0 -fspv-preserve-bindings -D WW_CELL_FOGLEAK=1 -D WW_FRAME_UBO=1 -Fo "$1.spv" "$(cygpath -m "$ROOT/res/shaders/hlsl/$2.hlsl" 2>/dev/null || echo "$ROOT/res/shaders/hlsl/$2.hlsl")"
	python -I "$ROOT/tools/spv_header.py" "$1.spv" "vkframe_$1_spv" "$1.spv.h" < /dev/null
	rm -f "$1.spv"
}
fxg fargnd_view_ps far-ground/fo4_cellcsmfgl_ps
fxg fargndp_view_ps far-ground/fo4_cellcsmfgp_ps
fxt() {
	"$DXC" -spirv -fspv-target-env=vulkan1.2 -T vs_6_0 -E main -fvk-b-shift 256 0 -fvk-t-shift 200 0 -fvk-s-shift 216 0 -fvk-u-shift 240 0 -fspv-preserve-bindings -Fo "$1.spv" "$(cygpath -m "$ROOT/res/shaders/hlsl/$2.hlsl" 2>/dev/null || echo "$ROOT/res/shaders/hlsl/$2.hlsl")"
	python -I "$ROOT/tools/spv_header.py" "$1.spv" "vkframe_$1_spv" "$1.spv.h" < /dev/null
	rm -f "$1.spv"
}
fxt fargnd_view_vs core/water_view_vs
ls -la fargnd_view_ps.spv.h fargndp_view_ps.spv.h fargnd_view_vs.spv.h
# lane VKFAR16: the probe relight (cell_reflrelight.prog, cellreflprobe.cpp) on fxg / fxt's shifts (its vertex stage declares
# nothing), and fxr: fxg's flags + the all-on route's RT GI key (WW_CELL_RESTIR + RTDDGI + RTDDGI_CASCADES), the `_rt_ps` twins
fxr() {
	"$DXC" -spirv -fspv-target-env=vulkan1.2 -T ps_6_0 -E main -fvk-b-shift 0 0 -fvk-t-shift 64 0 -fvk-s-shift 128 0 -fvk-u-shift 192 0 -fspv-preserve-bindings -D WW_CELL_FOGLEAK=1 -D WW_FRAME_UBO=1 -D WW_CELL_RESTIR=1 -D WW_CELL_RTDDGI=1 -D WW_CELL_RTDDGI_CASCADES=1 -Fo "$1.spv" "$(cygpath -m "$ROOT/res/shaders/hlsl/$2.hlsl" 2>/dev/null || echo "$ROOT/res/shaders/hlsl/$2.hlsl")"
	python -I "$ROOT/tools/spv_header.py" "$1.spv" "vkframe_$1_spv" "$1.spv.h" < /dev/null
	rm -f "$1.spv"
}
fxg reflrelight_view_ps reflections/cell_reflrelight_ps
fxr reflrelight_view_rt_ps reflections/cell_reflrelight_ps
fxt reflrelight_view_vs reflections/cell_reflrelight_vs
ls -la reflrelight_view_ps.spv.h reflrelight_view_rt_ps.spv.h reflrelight_view_vs.spv.h
# lane VKFAR25: the far LOD rings' RT GI twins from fo4_cellcsm's readable source (the frozen farlod_view_ps is the base key
# only): fxr = the all-on key, fxdg = fxg's flags + RTDDGI + RTDDGI_CASCADES (the Ray-traced GI row without an RT preset).
# Their vertex stage is fargnd_view_vs (t at 200): these pixel stages declare images up to t 127
fxdg() {
	"$DXC" -spirv -fspv-target-env=vulkan1.2 -T ps_6_0 -E main -fvk-b-shift 0 0 -fvk-t-shift 64 0 -fvk-s-shift 128 0 -fvk-u-shift 192 0 -fspv-preserve-bindings -D WW_CELL_FOGLEAK=1 -D WW_FRAME_UBO=1 -D WW_CELL_RTDDGI=1 -D WW_CELL_RTDDGI_CASCADES=1 -Fo "$1.spv" "$(cygpath -m "$ROOT/res/shaders/hlsl/$2.hlsl" 2>/dev/null || echo "$ROOT/res/shaders/hlsl/$2.hlsl")"
	python -I "$ROOT/tools/spv_header.py" "$1.spv" "vkframe_$1_spv" "$1.spv.h" < /dev/null
	rm -f "$1.spv"
}
fxr farlod_view_rt_ps core/fo4_cellcsm_ps
fxdg farlod_view_dg_ps core/fo4_cellcsm_ps
# lane VKGROUND11: the far ground's (fo4_cellcsmfgl, mode 1) RT GI twins, the same keys, vertex stage fargnd_view_vs
fxr fargnd_view_rt_ps far-ground/fo4_cellcsmfgl_ps
fxdg fargnd_view_dg_ps far-ground/fo4_cellcsmfgl_ps
# lane VKGROUND18: far ground mode 2's (fo4_cellcsmfgp, cached pages) RT GI twins, the same keys and vertex stage
fxr fargndp_view_rt_ps far-ground/fo4_cellcsmfgp_ps
fxdg fargndp_view_dg_ps far-ground/fo4_cellcsmfgp_ps
# lane VKFAR21: the probe G-buffer capture (cell_reflcap.prog, cellreflprobe.cpp) on fxg / fxt's shifts; the loader builds it
# with no define and fxg's two defines are inert there (ReflectionsBindingsVk.hlsli has no #if)
fxg reflcap_view_ps reflections/cell_reflcap_ps
fxt reflcap_view_vs reflections/cell_reflcap_vs
ls -la reflcap_view_ps.spv.h reflcap_view_vs.spv.h
# lane VKAO3: the baked AO decals (cell_aodecal.prog, cellaodecalgl.cpp, vkaodecal.inc) on fxg / fxt's shifts; the loader builds
# them with no define and fxg's two defines are inert there (AoBindingsVk.hlsli has no #if). PS b31 at 31, aoGbuf 64 / 128,
# aoVol 65 / 129; VS b31 at 287
fxg aodecal_view_ps ao/cell_aodecal_ps
fxt aodecal_view_vs ao/cell_aodecal_vs
ls -la aodecal_view_ps.spv.h aodecal_view_vs.spv.h
# lane VKAO4: the GI volume mask (cell_givol.prog, ao.cpp giVolMaskPass, vkvolmask.inc) on fxg / fxt's shifts; no define (fxg's two
# are inert: AoBindingsVk.hlsli has no #if). PS b31 at 31, volShapes 64, volGbuf 65 / 129; VS b31 at 287, volShapes 200
fxg volmask_view_ps ao/cell_givol_ps
fxt volmask_view_vs ao/cell_givol_vs
ls -la volmask_view_ps.spv.h volmask_view_vs.spv.h
# lane VKAIR5: the air inject's stage 0 (cell_volair.prog, cellvolair.cpp vaPassInner, vkairinject.inc) on fxg / fxt's shifts. The
# loader's default key adds WW_CELL_AIR_SKYVIEW: fxa = fxg's flags + it, fxar = fxr's flags (the all-on RT GI key) + it
fxa() {
	"$DXC" -spirv -fspv-target-env=vulkan1.2 -T ps_6_0 -E main -fvk-b-shift 0 0 -fvk-t-shift 64 0 -fvk-s-shift 128 0 -fvk-u-shift 192 0 -fspv-preserve-bindings -D WW_CELL_FOGLEAK=1 -D WW_FRAME_UBO=1 -D WW_CELL_AIR_SKYVIEW=1 -Fo "$1.spv" "$(cygpath -m "$ROOT/res/shaders/hlsl/$2.hlsl" 2>/dev/null || echo "$ROOT/res/shaders/hlsl/$2.hlsl")"
	python -I "$ROOT/tools/spv_header.py" "$1.spv" "vkframe_$1_spv" "$1.spv.h" < /dev/null
	rm -f "$1.spv"
}
fxar() {
	"$DXC" -spirv -fspv-target-env=vulkan1.2 -T ps_6_0 -E main -fvk-b-shift 0 0 -fvk-t-shift 64 0 -fvk-s-shift 128 0 -fvk-u-shift 192 0 -fspv-preserve-bindings -D WW_CELL_FOGLEAK=1 -D WW_FRAME_UBO=1 -D WW_CELL_RESTIR=1 -D WW_CELL_RTDDGI=1 -D WW_CELL_RTDDGI_CASCADES=1 -D WW_CELL_AIR_SKYVIEW=1 -Fo "$1.spv" "$(cygpath -m "$ROOT/res/shaders/hlsl/$2.hlsl" 2>/dev/null || echo "$ROOT/res/shaders/hlsl/$2.hlsl")"
	python -I "$ROOT/tools/spv_header.py" "$1.spv" "vkframe_$1_spv" "$1.spv.h" < /dev/null
	rm -f "$1.spv"
}
fxa airinject_view_ps air/cell_volair_ps
fxar airinject_view_rt_ps air/cell_volair_ps
fxt airinject_view_vs air/cell_volair_vs
ls -la airinject_view_ps.spv.h airinject_view_rt_ps.spv.h airinject_view_vs.spv.h
# lane VKGROUND6: the far ground page paint (cell_fgpage.prog) and its BC1 encode (cell_fgbc1.prog), cellfarground.cpp paintJobs,
# vkfgpaint.inc, on fxg / fxt's shifts; the loader builds both with no define (fxg's two are inert: no #if on them in either
# source). One vertex stage (cell_ao.vert's twin, it declares nothing) serves both
fxg fgpage_view_ps far-ground/cell_fgpage_ps
fxg fgbc1_view_ps far-ground/cell_fgbc1_ps
fxt fgpage_view_vs core/cell_ao_vs
ls -la fgpage_view_ps.spv.h fgbc1_view_ps.spv.h fgpage_view_vs.spv.h
# lane VKFAR7: the TAA mask's reactive pass (taainputs.cpp pass 4, vktaareactive.inc): pixel stage on fx()'s shifts (b31 at 31,
# BaseMap at 64 / 128), vertex stage on fxb()'s (its b31 at 32)
fx taareactive_view_ps post/taa_reactive_ps
fxb taareactive_view_vs post/taa_reactive_vs
ls -la taareactive_view_ps.spv.h taareactive_view_vs.spv.h
# lane VKLAMP1: the lamps' plain shadow cube faces (celllamps.cpp, vklamp.inc): both stages on fx()'s shifts (the face block
# b1 at 1, dynamic offset)
fxv lamp_view_vs lights/lamp_view_vs
fx lamp_view_ps lights/lamp_view_ps
ls -la lamp_view_vs.spv.h lamp_view_ps.spv.h
# lane VKCLOUD1: the cloud shadow map (cloudshadow.cpp, vkcloudshadow.inc), the readable source itself on fx()'s shifts
fx cloud_shadowmap_view_ps clouds/cloud_shadowmap_ps
fx cloudmap_view_ps clouds/cloudmap_ps
# lane VKSMALL2: the frame's farthest depth (cellhdr.cpp depthMaxMake, vkdepthmax.inc), the readable source on fx()'s shifts
fx depthmax_view_ps exposure/cell_depthmax_ps
# lane VKSMALL3: the shading rate images (cellvrs.cpp wwCellVrsBuild, vkvrs.inc), the readable source on fx()'s shifts
fx vrs_rate_view_ps near/cell_vrs_rate_ps
# lane VKSMALL4: the soft-effect layer (cellfxres.cpp steps 1 and 3): the downsample is the readable source itself; the
# composite is its twin with both outputs at location 0 (index 0 / 1) for the dual-source blend
fx fxres_down_view_ps particles/cell_fxres_down_ps
fx fxres_comp_view_ps particles/fxres_comp_view_ps
# lane VKWATER4: the water look off (fo4_water.prog), frozen from the old fo4_water_ps FOGLEAK + FRAME_UBO spirv-cross file
fxkw water_view_off_ps water/water_view_off_ps
# lane VKWATER4 (L7): the camera-underwater fog (cell_underwater.prog + its per-sample _ms), frozen twins: both outputs at
# location 0 index 0 / 1 (dual-source blend), SV_SampleIndex in the _ms twin only
fxkw underwater_view_ps water/underwater_view_ps
fxkw underwater_view_ms_ps water/underwater_view_ms_ps
# lane VKOBJ5: particles_cell.prog (res/shaders/hlsl/particles): the record pass and the expand stage (VKFAR2's precip shape,
# VS_SHIFT) and the cell-lit pixel twin on the app's variant (WW_CELL_AIRZONE + WW_CELL_FOGLEAK + WW_FRAME_UBO), every binding kept
fxp() {
	"$DXC" -spirv -fspv-target-env=vulkan1.2 -HV 2021 -T "$3" -E main -D WW_CELL_AIRZONE=1 -D WW_CELL_FOGLEAK=1 -D WW_FRAME_UBO=1 $4 ${5:-} -fspv-preserve-bindings -fvk-use-dx-position-w -Fo "$1.spv" "$(cygpath -m "$ROOT/res/shaders/hlsl/particles/$2.hlsl" 2>/dev/null || echo "$ROOT/res/shaders/hlsl/particles/$2.hlsl")"
	python -I "$ROOT/tools/spv_header.py" "$1.spv" "vkframe_$1_spv" "$1.spv.h" < /dev/null
	rm -f "$1.spv"
}
fxp objpfx_cs pfx_view_cs cs_6_0 "$VS_SHIFT"
fxp objpfx_vs pfx_view_vs vs_6_0 "$VS_SHIFT"
fxp objpfx_cell_ps particles_cell_ps ps_6_0 "$PS_SHIFT"
# round 5: its RT GI variant (the all-on route's WW_CELL_RTDDGI + RTDDGI_CASCADES + RESTIR, as the fxc _rt lines)
fxp objpfx_cell_rt_ps particles_cell_ps ps_6_0 "$PS_SHIFT" "-D WW_CELL_RTDDGI=1 -D WW_CELL_RTDDGI_CASCADES=1 -D WW_CELL_RESTIR=1"
# lane VKOBJ10: taa_objvel.prog (res/shaders/hlsl/post), the TAA inputs' moving-object redraw (taainputs.cpp step 3). The program
# has no loader defines; its pixel stage reads 1 / gl_FragCoord.w (the soft fade): -fvk-use-dx-position-w; every binding kept
fxo() {
	"$DXC" -spirv -fspv-target-env=vulkan1.2 -HV 2021 -T "$3" -E main $4 -fspv-preserve-bindings -fvk-use-dx-position-w -Fo "$1.spv" "$(cygpath -m "$ROOT/res/shaders/hlsl/post/$2.hlsl" 2>/dev/null || echo "$ROOT/res/shaders/hlsl/post/$2.hlsl")"
	python -I "$ROOT/tools/spv_header.py" "$1.spv" "vkframe_$1_spv" "$1.spv.h" < /dev/null
	rm -f "$1.spv"
}
fxo objvel_vs taa_objvel_vs vs_6_0 "$VS_SHIFT"
fxo objvel_ps taa_objvel_ps ps_6_0 "$PS_SHIFT"
# lane VKOBJ18: game_taa_mv.prog's pixel stage, the TAA inputs' camera vectors (taainputs.cpp step 2)
fxo camvel_ps game_taa_mv_ps ps_6_0 "$PS_SHIFT"
# lane VKOBJ15: boltstrip.prog (res/shaders/hlsl/core), the lightning strips (controllers.cpp drawPreview). No loader
# defines; every binding kept
fxe() {
	"$DXC" -spirv -fspv-target-env=vulkan1.2 -HV 2021 -T "$3" -E main $4 -fspv-preserve-bindings -fvk-use-dx-position-w -Fo "$1.spv" "$(cygpath -m "$ROOT/res/shaders/hlsl/core/$2.hlsl" 2>/dev/null || echo "$ROOT/res/shaders/hlsl/core/$2.hlsl")"
	python -I "$ROOT/tools/spv_header.py" "$1.spv" "vkframe_$1_spv" "$1.spv.h" < /dev/null
	rm -f "$1.spv"
}
fxe bolt_view_vs boltstrip_vs vs_6_0 "$VS_SHIFT"
fxe bolt_view_ps boltstrip_ps ps_6_0 "$PS_SHIFT"
# lane VKOBJ18: game_taa_mv.prog's vertex stage (cell_ao.vert, the full-screen quad)
fxe camvel_vs cell_ao_vs vs_6_0 "$VS_SHIFT"
ls -la csm_view_vs.spv.h obj_depth_vs.spv.h sky_view_ps.spv.h bloom_down.spv.h bloom_hold.spv.h bloom_up.spv.h bloom_src.spv.h bloom_comb.spv.h bloom_back.spv.h tonemap.spv.h measure_q.spv.h tone_view_vs.spv.h tone_view_ps.spv.h taa_view_mv_cs.spv.h taa_view_resolve_cs.spv.h taa_view_rcas_ps.spv.h
