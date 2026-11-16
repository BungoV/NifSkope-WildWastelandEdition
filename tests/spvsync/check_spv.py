# Lane RESTIRSPV1: every committed SPIR-V header (src/**/*.spv.h) must be what its generator script makes of its
# source TODAY. A .comp / .glsl edit without `bash src/features/gi/rtddgi_spirv.sh` (or src/features/gi/bakert_spirv.sh) leaves the exe
# running the OLD kernel against a CPU twin with the new rule (skill ww-rtddgi-volume, "a stale header").
#   python -I tests/spvsync/check_spv.py <tree> <scratch dir> [--glslang <exe>] [--committed <name>=<file>]...
# Rebuilds each header from its source into <scratch> with the same tool + flags the generator scripts use, and
# compares the bytes with the committed header (--committed swaps the file compared, for the red).
# rc 0 PASS (every header identical), 1 FAIL (a header differs, a header nobody generates, a script that drifted
# from this table), 77 SKIP (no glslangValidator, or a different glslang version than the headers were made with:
# the bytes then cannot be compared, which is not a pass).
import os, re, shutil, subprocess, sys

# (source dir, kernel, --target-env, --vn) == src/features/gi/bakert_spirv.sh + src/features/gi/rtddgi_spirv.sh
# HLSLRESTIR1: the glslang headers left are restirgi's red (WW_CELL_RESTIRGI_RED=glsl), built from restirgi.comp
SPECS = [('src/features/gi/rtddgi', 'restirgi_glsl', 'vulkan1.2', 'rtddgi_restirgi_glsl_spv'),
	('src/features/gi/rtddgi', 'restirgi_filt_glsl', 'vulkan1.2', 'rtddgi_restirgi_filt_glsl_spv')]
# lane VKCOMPUTE1: kernels converted to HLSL, built by DXC from res/shaders/hlsl/<dir>/<k>.hlsl + tools/spv_header.py
# (rtddgi_spirv.sh's DXC loop; bakert_spirv.sh's `hk <k> <env>` lines)
HLSL_SPECS = [('src/features/gi/rtddgi', k, 'vulkan1.2', 'rtddgi_%s_spv' % k) for k in ('border', 'reloc', 'blend', 'trace', 'airrays', 'restir', 'restirgi')] + [
	('src/features/gi/rtddgi', 'restirgi_filt', 'vulkan1.2', 'rtddgi_restirgi_filt_spv'),
	('src/features/gi/bakert', 'bvh', 'vulkan1.1', 'bakert_bvh_spv'),
	('src/features/gi/bakert', 'rq', 'vulkan1.2', 'bakert_rq_spv'),
	('src/features/gi/bakert', 'rqx', 'vulkan1.2', 'bakert_rqx_spv'),
]
# RTEMIS1 11-09: a header built from another kernel's source under defines (rtddgi_spirv.sh's own line, outside its loop)
FROM = {'restirgi_glsl': ('restirgi', []), 'restirgi_filt_glsl': ('restirgi', ['-DRESTIRGI_FILTER'])}
# HLSLRESTIR1: an HLSL header built from another kernel's .hlsl under defines (rtddgi_spirv.sh's own line after its DXC loop)
HLSL_FROM = {'restirgi_filt': ('restirgi', ['-DRESTIRGI_FILTER'])}
BAKERT_HK = '\t"$DXC" -spirv -fspv-target-env="$2" -T "${T:-cs_6_0}" -E main -O0 -Fo "$1.hlsl.spv" '
# lane VKFRAME1: the bloom chain, built by src/features/vkframe/vkframe_spirv.sh from res/shaders/hlsl/post/<k>.hlsl
VKFRAME_KERNELS = ('down', 'hold', 'up', 'src', 'comb', 'back')	# VKBLOOM1: back = the backdrop's half-float resolve
HLSL_SPECS += [('src/features/vkframe', 'bloom_' + k, 'vulkan1.2', 'vkframe_bloom_%s_spv' % k) for k in VKFRAME_KERNELS]
VKFRAME_POST = ('tonemap',)	# the script's second loop: res/shaders/hlsl/post/<k>.hlsl -> <k>.spv.h
HLSL_SPECS += [('src/features/vkframe', k, 'vulkan1.2', 'vkframe_%s_spv' % k) for k in VKFRAME_POST]
VKFRAME_EXPOSURE = ('measure_q',)	# the script's third loop: res/shaders/hlsl/exposure/<k>.hlsl -> <k>.spv.h
HLSL_SPECS += [('src/features/vkframe', k, 'vulkan1.2', 'vkframe_%s_spv' % k) for k in VKFRAME_EXPOSURE]
HLSL_DIR = {'src/features/gi/rtddgi': 'res/shaders/hlsl/rtddgi', 'src/features/vkframe': 'res/shaders/hlsl/post'}
VKFRAME_VIEW = ('tone_view_vs', 'tone_view_ps',	# the script's fourth loop (round 11): vertex + fragment, post dir
	'taa_view_mv_cs', 'taa_view_resolve_cs', 'taa_view_rcas_ps')	# lane VKSHARE1: TAA on the view
HLSL_SPECS += [('src/features/vkframe', k, 'vulkan1.2', 'vkframe_%s_spv' % k) for k in VKFRAME_VIEW]
HLSL_SRC = dict((k, 'res/shaders/hlsl/exposure') for k in VKFRAME_EXPOSURE)	# a kernel outside its folder's HLSL dir
# lane VKSKY1: the cell sky, the script's fifth loop: res/shaders/hlsl/sky/atmo_sky_ps.hlsl, registers shifted apart
VKFRAME_SKY_ARGS = ['-fvk-b-shift', '0', '0', '-fvk-t-shift', '64', '0', '-fvk-s-shift', '128', '0', '-fvk-u-shift', '192', '0']
VKFRAME_SKY = {'sky_view_ps': ('res/shaders/hlsl/sky', 'atmo_sky_ps')}
HLSL_SPECS += [('src/features/vkframe', k, 'vulkan1.2', 'vkframe_%s_spv' % k) for k in VKFRAME_SKY]
# lane VKLIGHT1: AO / contact / SSGI, the script's sixth loop: res/shaders/hlsl/vklight/<src>.hlsl -D <def>, built -O0
VKFRAME_LIGHT = {'ao_gbuf': ('ao_vk', 'AO_STAGE=5'), 'ao_mip': ('ao_vk', 'AO_STAGE=1'), 'ao_raw': ('ao_vk', 'AO_STAGE=2'),
	'ao_blur': ('ao_vk', 'AO_STAGE=3'), 'ao_blurf': ('ao_vk', 'AO_STAGE=4'), 'contact': ('contact_vk', ''), 'ssgi': ('ssgi_vk', ''),
	'aox_z': ('aox_vk', 'AOX_KERNEL=1'), 'aox_xe': ('aox_vk', 'AOX_KERNEL=2'), 'aox_r8': ('aox_vk', 'AOX_KERNEL=3'),	# lane VKFAR3
	# lane VKWATER4 (row G5): the ultra ocean, ocean_vk.hlsl stages 1-6, and 7-10 (3-6 into the shared arrays in place)
	'ocean_spec': ('ocean_vk', 'OCEAN_STAGE=1'), 'ocean_fft': ('ocean_vk', 'OCEAN_STAGE=2'), 'ocean_finbc': ('ocean_vk', 'OCEAN_STAGE=3'),
	'ocean_find': ('ocean_vk', 'OCEAN_STAGE=4'), 'ocean_mipbc': ('ocean_vk', 'OCEAN_STAGE=5'), 'ocean_mipd': ('ocean_vk', 'OCEAN_STAGE=6'),
	'ocean_finbca': ('ocean_vk', 'OCEAN_STAGE=7'), 'ocean_finda': ('ocean_vk', 'OCEAN_STAGE=8'),
	'ocean_mipbca': ('ocean_vk', 'OCEAN_STAGE=9'), 'ocean_mipda': ('ocean_vk', 'OCEAN_STAGE=10')}
HLSL_SPECS += [('src/features/vkframe', 'light_' + k, 'vulkan1.2', 'vkframe_light_%s_spv' % k) for k in VKFRAME_LIGHT]
# lane VKFAR3 (A1 b): the screen GI's DDGI read, its own line after the light loop (four defines, -O0)
VKFRAME_GIHALF_ARGS = ['-O0', '-D', 'WW_CELL_FOGLEAK=1', '-D', 'WW_CELL_RTDDGI=1', '-D', 'WW_CELL_RTDDGI_CASCADES=1', '-D', 'WW_FRAME_UBO=1']
HLSL_SPECS += [('src/features/vkframe', 'light_gihalf', 'vulkan1.2', 'vkframe_light_gihalf_spv')]
# lane VKLIGHT2: the reflections, the script's `ssr <name> "<defines>"` lines (res/shaders/hlsl/vklight/ssr_vk.hlsl -DSSR_LIVE)
VKFRAME_SSR = {'ssr_view_s1': '-DSSR_STAGE=1', 'ssr_view_s1w': '-DSSR_STAGE=1 -DF4FX_SSR_WATER_LOOK',
	'ssr_view_s2': '-DSSR_STAGE=2', 'ssr_view_s2w': '-DSSR_STAGE=2 -DF4FX_SSR_WATER_LOOK',
	'ssr_view_s3': '-DSSR_STAGE=3', 'ssr_view_s4': '-DSSR_STAGE=4', 'ssr_view_s5': '-DSSR_STAGE=5'}
HLSL_SPECS += [('src/features/vkframe', k, 'vulkan1.2', 'vkframe_%s_spv' % k) for k in VKFRAME_SSR]
# lane VKSHADOW1: the sun cascades' queued casters, the script's csm loop (res/shaders/hlsl/near/<k>.hlsl, vs_6_0)
VKFRAME_CSM = {'csm_view_vs': ('res/shaders/hlsl/near', 'csm_view_vs'),
	'obj_depth_vs': ('res/shaders/hlsl/near', 'obj_depth_vs')}  # lane VKOBJ1: the objects' prepass
HLSL_SPECS += [('src/features/vkframe', k, 'vulkan1.2', 'vkframe_%s_spv' % k) for k in VKFRAME_CSM]
# lane VKAIR1: the air's sky pass and the clouds, the script's `fx <name> <dir/file>` lines (sky's register shifts, ps_6_0)
# lane HLSLSRC2: the views' frozen spirv-cross twins under the view's name (the readable sources took the programs' names;
# vkpresent.h's kT_* tables hold the twins' layout)
VKFRAME_FX = {'volsky_view_ps': 'air/volsky_view_ps', 'cloud_march_view_ps': 'clouds/cloud_march_view_ps',
	'cloud_march_a_view_ps': 'clouds/cloud_march_a_view_ps', 'cloud_resolve_view_ps': 'clouds/cloud_resolve_view_ps',
	'cloud_resolve_a_view_ps': 'clouds/cloud_resolve_a_view_ps',
	'fardots_view_ps': 'far-lod/fardots_view_ps',	# lane VKFAR1
	'fogbloom_view_ps': 'post/fogbloom_view_ps',	# lane VKFAR2: hand-kept readable twin (P4b)
	'cloud_composite_view_ps': 'clouds/cloud_composite_view_ps',	# lane VKFAR2: P8, frozen copy
	'precip_view_ps': 'weather/precip_view_ps',	# lane VKFAR2: L8 slice 1, frozen copy of cell_precip_gpu_ps
	'precip_look_view_ps': 'weather/precip_look_view_ps',	# lane VKFAR2: L8 slice 3, the look over that copy
	'taareactive_view_ps': 'post/taa_reactive_ps'}	# lane VKFAR7: the TAA mask's reactive pass
VKFRAME_FX['lamp_view_ps'] = 'lights/lamp_view_ps'	# lane VKLAMP1: the lamps' plain shadow cube faces
HLSL_SPECS += [('src/features/vkframe', k, 'vulkan1.2', 'vkframe_%s_spv' % k) for k in VKFRAME_FX]
# lane VKFAR1: the far dots' vertex stage, the script's `fxv <name> <dir/file>` lines (fx()'s shifts, vs_6_0)
VKFRAME_FXV = {'fardots_view_vs': 'far-lod/cell_fardots_view_vs',
	'precip_view_vs': 'weather/precip_view_vs',	# lane VKFAR2: L8 slice 1, rain / snow's instanced corners
	'precip_look_view_vs': 'weather/precip_look_view_vs'}	# lane VKFAR2: L8 slice 3, with the look's interpolants
VKFRAME_FXV['lamp_view_vs'] = 'lights/lamp_view_vs'	# lane VKLAMP1: the lamps' plain shadow cube faces
HLSL_SPECS += [('src/features/vkframe', k, 'vulkan1.2', 'vkframe_%s_spv' % k) for k in VKFRAME_FXV]
# lane VKFAR2 L8 slice 1: rain / snow's record pass, the script's `fxs <name> <dir/file>` lines (fx()'s shifts, cs_6_0)
VKFRAME_FXS = {'precip_view_cs': 'weather/precip_view_cs', 'precip_look_view_cs': 'weather/precip_look_view_cs',
	'precip_lit_view_cs': 'weather/precip_lit_view_cs',	# lane VKFAR2: L8 slice 2b, lit at the centre
	'ddgi_atlas_fetch_cs': 'rtddgi/ddgi_atlas_fetch_cs'}	# lane VKFAR3: the DDGI atlas through the present device's view
HLSL_SPECS += [('src/features/vkframe', k, 'vulkan1.2', 'vkframe_%s_spv' % k) for k in VKFRAME_FXS]
# lane VKFAR1 round 2: the far cards' pixel stage, the script's `fxk <name> <dir/file>` lines (fx() + every declared binding kept)
VKFRAME_FXK = {'farcards_view_ps': 'far-lod/farcards_view_ps',
	'farlod_view_ps': 'core/farlod_view_ps',	# lane VKFAR2: the far LOD rings
	'skystars_view_ps': 'clouds/lookdev_clouds_ps', 'skymoon_view_ps': 'sky/sky_moon_ps'}	# lane VKFAR2 G2: the sky bodies
HLSL_SPECS += [('src/features/vkframe', k, 'vulkan1.2', 'vkframe_%s_spv' % k) for k in VKFRAME_FXK]
# lane VKWATER3: the water's pixel twins (frozen, as HLSLSRC2's), the script's `fxkw` lines: fxk() + -fvk-use-dx-position-w
# (their spirv-cross fixup gl_FragCoord.w = 1.0 / gl_FragCoord.w wants D3D's clip w in SV_Position.w)
VKFRAME_FXKW = {'water_view_ps': 'water/water_view_ps',
	'water_view_d_ps': 'water/water_view_d_ps', 'water_view_m_ps': 'water/water_view_m_ps',
	'water_view_p_ps': 'water/water_view_p_ps', 'water_view_dp_ps': 'water/water_view_dp_ps',
	'water_view_off_ps': 'water/water_view_off_ps',	# lane VKWATER4: fo4_water.prog (the water look off)
	'underwater_view_ps': 'water/underwater_view_ps',	# lane VKWATER4 (L7): cell_underwater.prog
	'underwater_view_ms_ps': 'water/underwater_view_ms_ps'}	# and its per-sample cell_underwater_ms.prog
HLSL_SPECS += [('src/features/vkframe', k, 'vulkan1.2', 'vkframe_%s_spv' % k) for k in VKFRAME_FXKW]
# lane VKFAR2: the far cards' vertex stage, the script's `fxc <name> <dir/file>` lines (its own bindings in the cards' set)
VKFRAME_FXC = {'farcards_view_vs': 'far-lod/farcards_view_vs'}	# frozen twin, as HLSLSRC2's
VKFRAME_FXC_ARGS = ['-fvk-bind-register', 'b0', '0', '0', '0', '-fvk-bind-register', 'b31', '0', '32', '0', '-fvk-bind-register',
	't0', '0', '96', '0', '-fvk-bind-register', 't1', '0', '97', '0', '-fspv-preserve-bindings']
HLSL_SPECS += [('src/features/vkframe', k, 'vulkan1.2', 'vkframe_%s_spv' % k) for k in VKFRAME_FXC]
# lane VKWATER3: the water's vertex stage, the script's `fxw <name> <dir/file>` lines (vs_6_0, own shifts, bindings kept)
VKFRAME_FXW_ARGS = ['-fvk-b-shift', '256', '0', '-fvk-t-shift', '110', '0', '-fvk-s-shift', '180', '0', '-fvk-u-shift', '220', '0',
	'-fspv-preserve-bindings']
VKFRAME_FXW = {'water_view_vs': 'core/water_view_vs'}
HLSL_SPECS += [('src/features/vkframe', k, 'vulkan1.2', 'vkframe_%s_spv' % k) for k in VKFRAME_FXW]
# lane VKWATER3 item 4: the tessellated water's hull / domain stages, the script's `fxh` / `fxd <name> <dir/file>` lines
VKFRAME_FXH_ARGS = ['-fvk-b-shift', '512', '0', '-fvk-t-shift', '576', '0', '-fvk-s-shift', '640', '0', '-fvk-u-shift', '704', '0',
	'-fspv-preserve-bindings']
VKFRAME_FXH = {'water_view_hs': 'water/fo4_waterdisp_hs'}
VKFRAME_FXD_ARGS = ['-fvk-b-shift', '768', '0', '-fvk-t-shift', '832', '0', '-fvk-s-shift', '896', '0', '-fvk-u-shift', '960', '0',
	'-fspv-preserve-bindings', '-Gis']
VKFRAME_FXD = {'water_view_ds': 'water/fo4_waterdisp_ds'}
HLSL_SPECS += [('src/features/vkframe', k, 'vulkan1.2', 'vkframe_%s_spv' % k) for k in list(VKFRAME_FXH) + list(VKFRAME_FXD)]
# lane VKFAR2: the far LOD rings' vertex stage, the script's `fxl <name> <dir/file>` lines (fxw's shifts, t at 120)
VKFRAME_FXL_ARGS = ['-fvk-b-shift', '256', '0', '-fvk-t-shift', '120', '0', '-fvk-s-shift', '180', '0', '-fvk-u-shift', '220', '0',
	'-fspv-preserve-bindings']
VKFRAME_FXL = {'farlod_view_vs': 'core/water_view_vs'}
HLSL_SPECS += [('src/features/vkframe', k, 'vulkan1.2', 'vkframe_%s_spv' % k) for k in VKFRAME_FXL]
# lane VKFAR2 G2: the sky bodies' vertex stages, the script's `fxb <name> <dir/file>` lines (b0 at 0, their b31 at 32)
VKFRAME_FXB_ARGS = ['-fvk-bind-register', 'b0', '0', '0', '0', '-fvk-bind-register', 'b31', '0', '32', '0', '-fspv-preserve-bindings']
VKFRAME_FXB = {'skystars_view_vs': 'clouds/lookdev_clouds_vs', 'skymoon_view_vs': 'core/lookdev_bill_vs',
	'taareactive_view_vs': 'post/taa_reactive_vs'}	# lane VKFAR7: the TAA mask's reactive pass
HLSL_SPECS += [('src/features/vkframe', k, 'vulkan1.2', 'vkframe_%s_spv' % k) for k in VKFRAME_FXB]
# lane VKFAR3 G3: the far ground's pixel stages, the script's `fxg <name> <dir/file>` lines (fxk()'s shifts + the app's variant)
VKFRAME_FXG_ARGS = VKFRAME_SKY_ARGS + ['-fspv-preserve-bindings', '-D', 'WW_CELL_FOGLEAK=1', '-D', 'WW_FRAME_UBO=1']
VKFRAME_FXG = {'fargnd_view_ps': 'far-ground/fo4_cellcsmfgl_ps',
	'reflrelight_view_ps': 'reflections/cell_reflrelight_ps',	# lane VKFAR16: the probe relight
	'reflcap_view_ps': 'reflections/cell_reflcap_ps',	# lane VKFAR21: the probe G-buffer capture
	'aodecal_view_ps': 'ao/cell_aodecal_ps',	# lane VKAO3: the baked AO decals
	'volmask_view_ps': 'ao/cell_givol_ps'}	# lane VKAO4: the GI volume mask
HLSL_SPECS += [('src/features/vkframe', k, 'vulkan1.2', 'vkframe_%s_spv' % k) for k in VKFRAME_FXG]
# lane VKFAR16: the `fxr <name> <dir/file>` lines, fxg's flags + the all-on route's RT GI key
VKFRAME_FXR_ARGS = VKFRAME_FXG_ARGS + ['-D', 'WW_CELL_RESTIR=1', '-D', 'WW_CELL_RTDDGI=1', '-D', 'WW_CELL_RTDDGI_CASCADES=1']
VKFRAME_FXR = {'reflrelight_view_rt_ps': 'reflections/cell_reflrelight_ps'}
HLSL_SPECS += [('src/features/vkframe', k, 'vulkan1.2', 'vkframe_%s_spv' % k) for k in VKFRAME_FXR]
# lane VKFAR3 G3: their vertex stage, the script's `fxt <name> <dir/file>` lines (t at 200, s at 216: past the pixel
# stage's declared images 64..126 and samplers 128..190; used or not, every declared binding is in the merged set)
VKFRAME_FXT_ARGS = ['-fvk-b-shift', '256', '0', '-fvk-t-shift', '200', '0', '-fvk-s-shift', '216', '0', '-fvk-u-shift', '240', '0',
	'-fspv-preserve-bindings']
VKFRAME_FXT = {'fargnd_view_vs': 'core/water_view_vs', 'reflrelight_view_vs': 'reflections/cell_reflrelight_vs',
	'reflcap_view_vs': 'reflections/cell_reflcap_vs',	# lane VKFAR21
	'aodecal_view_vs': 'ao/cell_aodecal_vs',	# lane VKAO3
	'volmask_view_vs': 'ao/cell_givol_vs'}	# lane VKAO4
HLSL_SPECS += [('src/features/vkframe', k, 'vulkan1.2', 'vkframe_%s_spv' % k) for k in VKFRAME_FXT]
# lane VKFAR2 P9: the lens-flare sprites, the script's `fxf <name> <dir/file> <profile>` lines (no shifts, no options)
VKFRAME_FXF = {'lensflare_view_vs': ('post/lens_flare_view_vs', 'vs_6_0'), 'lensflare_view_ps': ('post/lens_flare', 'ps_6_0')}
HLSL_SPECS += [('src/features/vkframe', k, 'vulkan1.2', 'vkframe_%s_spv' % k) for k in VKFRAME_FXF]
# lane VKFAR2 (the froxel item): the air's integrate, the script's `fog <name> "<defines>"` lines (cs_6_0, one source)
VKFRAME_FOG_SRC = 'res/shaders/hlsl/air/volair_integ_cs.hlsl'
VKFRAME_FOG = {'fog_integ_cs': '', 'fog_integ32_cs': '-DVOLAIR_INTEG_F32'}
HLSL_SPECS += [('src/features/vkframe', k, 'vulkan1.2', 'vkframe_%s_spv' % k) for k in VKFRAME_FOG]
# lane VKFAR2 P7: cellpost.cpp's post passes in the tone pass, the script's `pfx <name> <dir/file> <profile> "<defines>"` lines
VKFRAME_PFX = {
	'p7_res': ('post/post7_resolve', 'cs_6_0', ''), 'p7_dep': ('post/post7_resolve', 'cs_6_0', '-DPOST7_DEPTH'),
	'p7_copy_ps': ('post/post7_copy_ps', 'ps_6_0', ''), 'p7_dof_down': ('post/dof_down', 'cs_6_0', ''),
	'p7_dof_blur': ('post/dof_blur', 'cs_6_0', ''), 'p7_dof_comp': ('post/dof_comp', 'cs_6_0', ''),
	'p7_lens_comp': ('post/dof_comp', 'cs_6_0', '-DDOF_LENS'), 'p7_lens_prep': ('post/doflens_prep', 'cs_6_0', ''),
	'p7_lens_tile': ('post/doflens_tile', 'cs_6_0', ''), 'p7_lens_gather': ('post/doflens_gather', 'cs_6_0', ''),
	'p7_mblur': ('post/mblur', 'cs_6_0', ''), 'p7_dofmblur': ('post/mblur', 'cs_6_0', '-DMBLUR_FUSE'),
	'p7_lensmblur': ('post/mblur', 'cs_6_0', '-DMBLUR_FUSE -DDOF_LENS'), 'p7_mv': ('post/taa_mv', 'cs_6_0', '-DTAA_MV_RG16'),
	'p7_ob_vec': ('post/objblur_vec', 'cs_6_0', ''), 'p7_ob_tile': ('post/objblur_tile', 'cs_6_0', ''),
	'p7_ob': ('post/objblur', 'cs_6_0', '')}
HLSL_SPECS += [('src/features/vkframe', k, 'vulkan1.2', 'vkframe_%s_spv' % k) for k in VKFRAME_PFX]
# lane VKOBJ4: the bl twins, the script's `fxb <name> <near file> <profile> "$PS_SHIFT|$VS_SHIFT"` lines (-D WW_VK_BINDLESS, every
# binding kept); the vertex stage on its own shifts (VS_SHIFT), the pixel stages on fx()'s
VKFRAME_PS_SHIFT = VKFRAME_SKY_ARGS
VKFRAME_VS_SHIFT = ['-fvk-b-shift', '256', '0', '-fvk-t-shift', '320', '0', '-fvk-s-shift', '384', '0', '-fvk-u-shift', '448', '0']
VKFRAME_FXBL = {'objbl_vs': ('fo4_defaultbl_vs', 'vs_6_0', 'VS_SHIFT', ''), 'objbl_cell_ps': ('fo4_cellbl_ps', 'ps_6_0', 'PS_SHIFT', '-D WW_CELL_FOGLEAK=1 -D WW_FRAME_UBO=1'),
	'objbl_cellcsm_ps': ('fo4_cellcsmbl_ps', 'ps_6_0', 'PS_SHIFT', '-D WW_CELL_FOGLEAK=1 -D WW_FRAME_UBO=1'),
	# VKWATER3 G4: the bl pair on the app's key; its RT GI variant (a 5th quoted field, as fxc's)
	'objbl_cell_rt_ps': ('fo4_cellbl_ps', 'ps_6_0', 'PS_SHIFT', '-D WW_CELL_FOGLEAK=1 -D WW_FRAME_UBO=1 -D WW_CELL_RTDDGI=1 -D WW_CELL_RTDDGI_CASCADES=1 -D WW_CELL_RESTIR=1'),
	'objbl_cellcsm_rt_ps': ('fo4_cellcsmbl_ps', 'ps_6_0', 'PS_SHIFT', '-D WW_CELL_FOGLEAK=1 -D WW_FRAME_UBO=1 -D WW_CELL_RTDDGI=1 -D WW_CELL_RTDDGI_CASCADES=1 -D WW_CELL_RESTIR=1')}
HLSL_SPECS += [('src/features/vkframe', k, 'vulkan1.2', 'vkframe_%s_spv' % k) for k in VKFRAME_FXBL]
# the cell frame's per-sample readback (the script's objbl_samples_cs line: cs_6_0, no shift)
HLSL_SPECS += [('src/features/vkframe', 'objbl_samples_cs', 'vulkan1.2', 'vkframe_objbl_samples_cs_spv')]
HLSL_SRC['objbl_samples_cs'] = 'res/shaders/hlsl/near'
# lane VKWATER3 (G4 by draws): the script's `fxc <name> <core file> <profile> "$PS_SHIFT|$VS_SHIFT"` lines, the non-bindless
# fallbacks on the app's base variant, the frozen FragCoord.w fixup's flag
VKFRAME_FXCELL_ARGS = ['-HV', '2021', '-D', 'WW_CELL_FOGLEAK=1', '-D', 'WW_FRAME_UBO=1']
# (G4 rest: a 5th quoted field, a variant's other defines)
VKFRAME_FXCELL = {'objcell_vs': ('fo4_default_vs', 'vs_6_0', 'VS_SHIFT', ''), 'objcell_cell_ps': ('fo4_cell_ps', 'ps_6_0', 'PS_SHIFT', ''),
	'objcell_cellcsm_ps': ('fo4_cellcsm_ps', 'ps_6_0', 'PS_SHIFT', ''), 'objcell_pbrm_ps': ('pbrm_default_ps', 'ps_6_0', 'PS_SHIFT', ''),
	'objcell_pbrmcell_ps': ('pbrm_cell_ps', 'ps_6_0', 'PS_SHIFT', ''), 'objcell_pbrmcellcsm_ps': ('pbrm_cellcsm_ps', 'ps_6_0', 'PS_SHIFT', ''),
	'objcell_effectcell_ps': ('fo4_effectcell_ps', 'ps_6_0', 'PS_SHIFT', '-D WW_CELL_AIRZONE=1'),
	'objcell_effectdecalcsm_ps': ('fo4_effectdecalcsm_ps', 'ps_6_0', 'PS_SHIFT', '')}
# the all-on route's RT GI variant of each twin (pbrm_default's code is the same)
VKFRAME_FXCELL_RT = '-D WW_CELL_RTDDGI=1 -D WW_CELL_RTDDGI_CASCADES=1 -D WW_CELL_RESTIR=1'
VKFRAME_FXCELL.update({'objcell_cell_rt_ps': ('fo4_cell_ps', 'ps_6_0', 'PS_SHIFT', VKFRAME_FXCELL_RT),
	'objcell_cellcsm_rt_ps': ('fo4_cellcsm_ps', 'ps_6_0', 'PS_SHIFT', VKFRAME_FXCELL_RT),
	'objcell_pbrmcell_rt_ps': ('pbrm_cell_ps', 'ps_6_0', 'PS_SHIFT', VKFRAME_FXCELL_RT),
	'objcell_pbrmcellcsm_rt_ps': ('pbrm_cellcsm_ps', 'ps_6_0', 'PS_SHIFT', VKFRAME_FXCELL_RT),
	'objcell_effectcell_rt_ps': ('fo4_effectcell_ps', 'ps_6_0', 'PS_SHIFT', '-D WW_CELL_AIRZONE=1 ' + VKFRAME_FXCELL_RT),
	'objcell_effectdecalcsm_rt_ps': ('fo4_effectdecalcsm_ps', 'ps_6_0', 'PS_SHIFT', VKFRAME_FXCELL_RT)})
HLSL_SPECS += [('src/features/vkframe', k, 'vulkan1.2', 'vkframe_%s_spv' % k) for k in VKFRAME_FXCELL]
# lane VKOBJ5 (G4 round 3): the script's `fxp` lines, particles_cell.prog's record pass, expand stage and cell-lit pixel twin
# (res/shaders/hlsl/particles) on the app's variant
VKFRAME_FXP_ARGS = ['-HV', '2021', '-D', 'WW_CELL_AIRZONE=1', '-D', 'WW_CELL_FOGLEAK=1', '-D', 'WW_FRAME_UBO=1']
# (round 5: a 4th field, a variant's other defines: the RT GI pixel twin, as VKFRAME_FXCELL_RT)
VKFRAME_FXP = {'objpfx_cs': ('pfx_view_cs', 'cs_6_0', 'VS_SHIFT', ''), 'objpfx_vs': ('pfx_view_vs', 'vs_6_0', 'VS_SHIFT', ''),
	'objpfx_cell_ps': ('particles_cell_ps', 'ps_6_0', 'PS_SHIFT', ''),
	'objpfx_cell_rt_ps': ('particles_cell_ps', 'ps_6_0', 'PS_SHIFT', VKFRAME_FXCELL_RT)}
HLSL_SPECS += [('src/features/vkframe', k, 'vulkan1.2', 'vkframe_%s_spv' % k) for k in VKFRAME_FXP]
DEFAULT_G = 'E:/Tools/RenderDoc/RenderDoc_1.45_64/plugins/spirv/glslangValidator.exe'
DEFAULT_DXC = 'E:/Tools/dxc/bin/x64/dxc.exe'


def script_ok(tree):
	"""The generator scripts still say what SPECS says (one table, checked against the scripts it mirrors)."""
	bad = []
	b = open(os.path.join(tree, 'src/features/gi/bakert_spirv.sh'), encoding='utf-8', errors='replace').read()
	r = open(os.path.join(tree, 'src/features/gi/rtddgi_spirv.sh'), encoding='utf-8', errors='replace').read()
	# the glslang loop: a `for` line followed by the "$G" compile line (none once every kernel builds from HLSL)
	m = re.search(r'^for k in ([a-z ]+); do\n\t"\$G" ', r, re.M)
	loop = m.group(1).split() if m else []
	if loop and '"$G" --target-env vulkan1.2 -V --vn "rtddgi_${k}_spv" -o "${k}.spv.h" "${k}.comp"' not in r:
		bad.append('src/features/gi/rtddgi_spirv.sh: its compile line changed')
	for d, k, env, vn in SPECS:
		if d == 'src/features/gi/bakert':
			if '"$G" --target-env %s -V --vn %s -o %s.spv.h %s.comp' % (env, vn, k, k) not in b:
				bad.append('src/features/gi/bakert_spirv.sh: no line for %s (%s)' % (k, env))
		elif k in FROM:
			src, defs = FROM[k]
			if ' '.join(['"$G" --target-env', env, '-V'] + defs + ['--vn', vn, '-o', k + '.spv.h', src + '.comp']) not in r:
				bad.append('src/features/gi/rtddgi_spirv.sh: no line for %s (%s from %s.comp)' % (k, ' '.join(defs), src))
		elif k not in loop:
			bad.append('src/features/gi/rtddgi_spirv.sh: %s not in its kernel loop' % k)
	known = set(k for d, k, e, v in SPECS if d == 'src/features/gi/rtddgi')
	for k in loop:
		if k not in known:
			bad.append('src/features/gi/rtddgi_spirv.sh: kernel %s is not in this checker' % k)
	m = re.search(r'^for k in ([a-z ]+); do\n\tT=.*\n\t"\$DXC" -spirv -fspv-target-env=vulkan1\.2 -T "\$\{T:-cs_6_0\}" -E main -O0 ', r, re.M)
	hloop = m.group(1).split() if m else []
	if sorted(hloop) != sorted(k for d, k, e, v in HLSL_SPECS if d == 'src/features/gi/rtddgi' and k not in HLSL_FROM):
		bad.append('src/features/gi/rtddgi_spirv.sh: its DXC loop (%s) is not this checker\'s HLSL kernels' % ' '.join(hloop))
	for d, k, env, vn in HLSL_SPECS:
		if k in HLSL_FROM:
			src, defs = HLSL_FROM[k]
			line = ' '.join(['"$DXC" -spirv -fspv-target-env=' + env, '-T cs_6_5 -E main -O0'] + defs + ['-Fo', k + '.hlsl.spv'])
			hdr = ' '.join(['python -I "$ROOT/tools/spv_header.py"', k + '.hlsl.spv', vn, k + '.spv.h'])
			if line not in r or hdr not in r:
				bad.append('src/features/gi/rtddgi_spirv.sh: no DXC + spv_header lines for %s (%s from %s.hlsl)' % (k, ' '.join(defs), src))
	bk = [k for d, k, e, v in HLSL_SPECS if d == 'src/features/gi/bakert']
	if bk and BAKERT_HK not in b:
		bad.append('src/features/gi/bakert_spirv.sh: its hk compile line changed')
	for d, k, env, vn in HLSL_SPECS:
		if d == 'src/features/gi/bakert' and not re.search(r'^hk %s %s(\s|$)' % (k, re.escape(env)), b, re.M):
			bad.append('src/features/gi/bakert_spirv.sh: no `hk %s %s` line' % (k, env))
	for k, env in re.findall(r'^hk ([a-z]+) (\S+)', b, re.M):
		if k not in bk:
			bad.append('src/features/gi/bakert_spirv.sh: hk %s is not in this checker' % k)
	vf = open(os.path.join(tree, 'src/features/vkframe/vkframe_spirv.sh'), encoding='utf-8', errors='replace').read()
	m = re.search(r'^for k in ([a-z ]+); do\n\t"\$DXC" -spirv -fspv-target-env=vulkan1\.2 -T cs_6_0 -E main ', vf, re.M)
	vloop = m.group(1).split() if m else []
	if sorted(vloop) != sorted(VKFRAME_KERNELS):
		bad.append('src/features/vkframe/vkframe_spirv.sh: its DXC loop (%s) is not this checker\'s bloom kernels' % ' '.join(vloop))
	m = re.search(r'^for t in ([a-z ]+); do\n\t"\$DXC" -spirv -fspv-target-env=vulkan1\.2 -T cs_6_0 -E main ', vf, re.M)
	ploop = m.group(1).split() if m else []
	if sorted(ploop) != sorted(VKFRAME_POST):
		bad.append('src/features/vkframe/vkframe_spirv.sh: its post loop (%s) is not this checker\'s post kernels' % ' '.join(ploop))
	m = re.search(r'^for e in ([a-z_ ]+); do\n\t"\$DXC" -spirv -fspv-target-env=vulkan1\.2 -T cs_6_0 -E main ', vf, re.M)
	eloop = m.group(1).split() if m else []
	if sorted(eloop) != sorted(VKFRAME_EXPOSURE):
		bad.append('src/features/vkframe/vkframe_spirv.sh: its exposure loop (%s) is not this checker\'s exposure kernels' % ' '.join(eloop))
	m = re.search(r'^for v in ([a-z_ ]+); do\n\t"\$DXC" -spirv -fspv-target-env=vulkan1\.2 -T "\$\{v##\*_\}_6_0" -E main ', vf, re.M)
	wloop = m.group(1).split() if m else []
	if sorted(wloop) != sorted(VKFRAME_VIEW):
		bad.append('src/features/vkframe/vkframe_spirv.sh: its view loop (%s) is not this checker\'s view shaders' % ' '.join(wloop))
	m = re.search(r'^for s in ([a-z_ ]+); do\n\t"\$DXC" -spirv -fspv-target-env=vulkan1\.2 -T ps_6_0 -E main ' + re.escape(' '.join(VKFRAME_SKY_ARGS)) + ' ', vf, re.M)
	sloop = m.group(1).split() if m else []
	if sorted(sloop) != sorted(VKFRAME_SKY):
		bad.append('src/features/vkframe/vkframe_spirv.sh: its sky loop (%s) is not this checker\'s sky shaders' % ' '.join(sloop))
	m = re.search(r'^for l in ([A-Za-z_0-9:= ]+); do\n\tn=.*\n\t"\$DXC" -spirv -fspv-target-env=vulkan1\.2 -T cs_6_0 -E main -O0 \$\{d:\+-D "\$d"\} ', vf, re.M)
	lloop = dict((i.split(':')[0], (i.split(':')[1], i.split(':', 2)[2])) for i in m.group(1).split()) if m else {}
	if lloop != VKFRAME_LIGHT:
		bad.append('src/features/vkframe/vkframe_spirv.sh: its light loop (%s) is not this checker\'s light kernels' % ' '.join(sorted(lloop)))
	if '"$DXC" -spirv -fspv-target-env=vulkan1.2 -T cs_6_0 -E main ' + ' '.join(VKFRAME_GIHALF_ARGS) + ' -Fo "light_gihalf.spv" ' not in vf:
		bad.append('src/features/vkframe/vkframe_spirv.sh: its gihalf line is not this checker\'s')
	if '\t"$DXC" -spirv -fspv-target-env=vulkan1.2 -T cs_6_0 -E main -DSSR_LIVE $2 -Fo "$1.spv" ' not in vf:
		bad.append('src/features/vkframe/vkframe_spirv.sh: its ssr() compile line changed')
	rlines = dict(re.findall(r'^ssr ([a-z0-9_]+) "([^"]*)"$', vf, re.M))
	if rlines != VKFRAME_SSR:
		bad.append('src/features/vkframe/vkframe_spirv.sh: its ssr lines (%s) are not this checker\'s reflections' % ' '.join(sorted(rlines)))
	m = re.search(r'^for c in ([a-z_ ]+); do\n\t"\$DXC" -spirv -fspv-target-env=vulkan1\.2 -T vs_6_0 -E main -Fo "\$\{c\}\.spv" "\$\(cygpath -m "\$ROOT/res/shaders/hlsl/near/', vf, re.M)
	cloop = m.group(1).split() if m else []
	if sorted(cloop) != sorted(VKFRAME_CSM):
		bad.append('src/features/vkframe/vkframe_spirv.sh: its csm loop (%s) is not this checker\'s cascade shaders' % ' '.join(cloop))
	if '\t"$DXC" -spirv -fspv-target-env=vulkan1.2 -T ps_6_0 -E main ' + ' '.join(VKFRAME_SKY_ARGS) + ' -Fo "$1.spv" ' not in vf:
		bad.append('src/features/vkframe/vkframe_spirv.sh: its fx() compile line changed')
	flines = dict(re.findall(r'^fx ([a-z0-9_]+) ([A-Za-z0-9_/+-]+)$', vf, re.M))
	if flines != VKFRAME_FX:
		bad.append('src/features/vkframe/vkframe_spirv.sh: its fx lines (%s) are not this checker\'s air and cloud passes' % ' '.join(sorted(flines)))
	if '\t"$DXC" -spirv -fspv-target-env=vulkan1.2 -T vs_6_0 -E main ' + ' '.join(VKFRAME_SKY_ARGS) + ' -Fo "$1.spv" ' not in vf:
		bad.append('src/features/vkframe/vkframe_spirv.sh: its fxv() compile line changed')
	vlines = dict(re.findall(r'^fxv ([a-z0-9_]+) ([A-Za-z0-9_/+-]+)$', vf, re.M))
	if vlines != VKFRAME_FXV:
		bad.append('src/features/vkframe/vkframe_spirv.sh: its fxv lines (%s) are not this checker\'s vertex stages' % ' '.join(sorted(vlines)))
	if '\t"$DXC" -spirv -fspv-target-env=vulkan1.2 -T ps_6_0 -E main ' + ' '.join(VKFRAME_SKY_ARGS + ['-fspv-preserve-bindings']) + ' -Fo "$1.spv" ' not in vf:
		bad.append('src/features/vkframe/vkframe_spirv.sh: its fxk() compile line changed')
	klines = dict(re.findall(r'^fxk ([a-z0-9_]+) ([A-Za-z0-9_/+-]+)$', vf, re.M))
	if klines != VKFRAME_FXK:
		bad.append('src/features/vkframe/vkframe_spirv.sh: its fxk lines (%s) are not this checker\'s far cards' % ' '.join(sorted(klines)))
	if '\t"$DXC" -spirv -fspv-target-env=vulkan1.2 -T ps_6_0 -E main ' + ' '.join(VKFRAME_SKY_ARGS + ['-fspv-preserve-bindings', '-fvk-use-dx-position-w']) + ' -Fo "$1.spv" ' not in vf:
		bad.append('src/features/vkframe/vkframe_spirv.sh: its fxkw() compile line changed')
	kwlines = dict(re.findall(r'^fxkw ([a-z0-9_]+) ([A-Za-z0-9_/+-]+)$', vf, re.M))
	if kwlines != VKFRAME_FXKW:
		bad.append('src/features/vkframe/vkframe_spirv.sh: its fxkw lines (%s) are not this checker\'s water pixel twins' % ' '.join(sorted(kwlines)))
	if '\t"$DXC" -spirv -fspv-target-env=vulkan1.2 -T vs_6_0 -E main ' + ' '.join(VKFRAME_FXC_ARGS) + ' -Fo "$1.spv" ' not in vf:
		bad.append('src/features/vkframe/vkframe_spirv.sh: its fxc() compile line changed')
	clines = dict(re.findall(r'^fxc ([a-z0-9_]+) ([A-Za-z0-9_/+-]+)$', vf, re.M))
	if clines != VKFRAME_FXC:
		bad.append('src/features/vkframe/vkframe_spirv.sh: its fxc lines (%s) are not this checker\'s far-card vertex stages' % ' '.join(sorted(clines)))
	if '\t"$DXC" -spirv -fspv-target-env=vulkan1.2 -T vs_6_0 -E main ' + ' '.join(VKFRAME_FXW_ARGS) + ' -Fo "$1.spv" ' not in vf:
		bad.append('src/features/vkframe/vkframe_spirv.sh: its fxw() compile line changed')
	wlines = dict(re.findall(r'^fxw ([a-z0-9_]+) ([A-Za-z0-9_/+-]+)$', vf, re.M))
	if wlines != VKFRAME_FXW:
		bad.append('src/features/vkframe/vkframe_spirv.sh: its fxw lines (%s) are not this checker\'s water vertex stages' % ' '.join(sorted(wlines)))
	for fn, prof, args, want in (('fxh', 'hs_6_0', VKFRAME_FXH_ARGS, VKFRAME_FXH), ('fxd', 'ds_6_0', VKFRAME_FXD_ARGS, VKFRAME_FXD)):
		if '\t"$DXC" -spirv -fspv-target-env=vulkan1.2 -T %s -E main ' % prof + ' '.join(args) + ' -Fo "$1.spv" ' not in vf:
			bad.append('src/features/vkframe/vkframe_spirv.sh: its %s() compile line changed' % fn)
		got = dict(re.findall(r'^%s ([a-z0-9_]+) ([A-Za-z0-9_/+-]+)$' % fn, vf, re.M))
		if got != want:
			bad.append('src/features/vkframe/vkframe_spirv.sh: its %s lines (%s) are not this checker\'s water tessellation stages'
			           % (fn, ' '.join(sorted(got))))
	if '\t"$DXC" -spirv -fspv-target-env=vulkan1.2 -T vs_6_0 -E main ' + ' '.join(VKFRAME_FXL_ARGS) + ' -Fo "$1.spv" ' not in vf:
		bad.append('src/features/vkframe/vkframe_spirv.sh: its fxl() compile line changed')
	llines = dict(re.findall(r'^fxl ([a-z0-9_]+) ([A-Za-z0-9_/+-]+)$', vf, re.M))
	if llines != VKFRAME_FXL:
		bad.append('src/features/vkframe/vkframe_spirv.sh: its fxl lines (%s) are not this checker\'s far LOD vertex stages' % ' '.join(sorted(llines)))
	if '\t"$DXC" -spirv -fspv-target-env=vulkan1.2 -T ps_6_0 -E main ' + ' '.join(VKFRAME_FXG_ARGS) + ' -Fo "$1.spv" ' not in vf:
		bad.append('src/features/vkframe/vkframe_spirv.sh: its fxg() compile line changed')
	glines = dict(re.findall(r'^fxg ([a-z0-9_]+) ([A-Za-z0-9_/+-]+)$', vf, re.M))
	if glines != VKFRAME_FXG:
		bad.append('src/features/vkframe/vkframe_spirv.sh: its fxg lines (%s) are not this checker\'s far-ground pixel stages' % ' '.join(sorted(glines)))
	if '\t"$DXC" -spirv -fspv-target-env=vulkan1.2 -T ps_6_0 -E main ' + ' '.join(VKFRAME_FXR_ARGS) + ' -Fo "$1.spv" ' not in vf:
		bad.append('src/features/vkframe/vkframe_spirv.sh: its fxr() compile line changed')
	rlines = dict(re.findall(r'^fxr ([a-z0-9_]+) ([A-Za-z0-9_/+-]+)$', vf, re.M))
	if rlines != VKFRAME_FXR:
		bad.append('src/features/vkframe/vkframe_spirv.sh: its fxr lines (%s) are not this checker\'s RT GI pixel twins' % ' '.join(sorted(rlines)))
	if '\t"$DXC" -spirv -fspv-target-env=vulkan1.2 -T vs_6_0 -E main ' + ' '.join(VKFRAME_FXT_ARGS) + ' -Fo "$1.spv" ' not in vf:
		bad.append('src/features/vkframe/vkframe_spirv.sh: its fxt() compile line changed')
	tlines = dict(re.findall(r'^fxt ([a-z0-9_]+) ([A-Za-z0-9_/+-]+)$', vf, re.M))
	if tlines != VKFRAME_FXT:
		bad.append('src/features/vkframe/vkframe_spirv.sh: its fxt lines (%s) are not this checker\'s far-ground vertex stages' % ' '.join(sorted(tlines)))
	if '\t"$DXC" -spirv -fspv-target-env=vulkan1.2 -T vs_6_0 -E main ' + ' '.join(VKFRAME_FXB_ARGS) + ' -Fo "$1.spv" ' not in vf:
		bad.append('src/features/vkframe/vkframe_spirv.sh: its fxb() compile line changed')
	blines = dict(re.findall(r'^fxb ([a-z0-9_]+) ([A-Za-z0-9_/+-]+)$', vf, re.M))
	if blines != VKFRAME_FXB:
		bad.append('src/features/vkframe/vkframe_spirv.sh: its fxb lines (%s) are not this checker\'s sky-body vertex stages' % ' '.join(sorted(blines)))
	if '\t"$DXC" -spirv -fspv-target-env=vulkan1.2 -T $3 -E main -Fo "$1.spv" ' not in vf:
		bad.append('src/features/vkframe/vkframe_spirv.sh: its fxf() compile line changed')
	flines = {a: (b, c) for a, b, c in re.findall(r'^fxf ([a-z0-9_]+) ([A-Za-z0-9_/+-]+) ([vp]s_6_0)$', vf, re.M)}
	if flines != VKFRAME_FXF:
		bad.append('src/features/vkframe/vkframe_spirv.sh: its fxf lines (%s) are not this checker\'s lens-flare stages' % ' '.join(sorted(flines)))
	if '\t"$DXC" -spirv -fspv-target-env=vulkan1.2 -T cs_6_0 -E main $2 -Fo "$1.spv" "$(cygpath -m "$ROOT/' + VKFRAME_FOG_SRC + '" ' not in vf:
		bad.append('src/features/vkframe/vkframe_spirv.sh: its fog() compile line changed')
	glines = dict(re.findall(r'^fog ([a-z0-9_]+) "([^"]*)"$', vf, re.M))
	if glines != VKFRAME_FOG:
		bad.append('src/features/vkframe/vkframe_spirv.sh: its fog lines (%s) are not this checker\'s air integrate kernels' % ' '.join(sorted(glines)))
	if '\t"$DXC" -spirv -fspv-target-env=vulkan1.2 -T $3 -E main $4 -Fo "$1.spv" ' not in vf:
		bad.append('src/features/vkframe/vkframe_spirv.sh: its pfx() compile line changed')
	plines = {a: (b, c, e) for a, b, c, e in re.findall(r'^pfx ([a-z0-9_]+) ([A-Za-z0-9_/+-]+) ([cvp]s_6_0) "([^"]*)"$', vf, re.M)}
	if plines != VKFRAME_PFX:
		bad.append('src/features/vkframe/vkframe_spirv.sh: its pfx lines (%s) are not this checker\'s P7 post passes' % ' '.join(sorted(plines)))
	if '\t"$DXC" -spirv -fspv-target-env=vulkan1.2 -T cs_6_0 -E main ' + ' '.join(VKFRAME_SKY_ARGS) + ' -Fo "$1.spv" ' not in vf:
		bad.append('src/features/vkframe/vkframe_spirv.sh: its fxs() compile line changed')
	slines = dict(re.findall(r'^fxs ([a-z0-9_]+) ([A-Za-z0-9_/+-]+)$', vf, re.M))
	if slines != VKFRAME_FXS:
		bad.append('src/features/vkframe/vkframe_spirv.sh: its fxs lines (%s) are not this checker\'s record passes' % ' '.join(sorted(slines)))
	for n, a in (('PS_SHIFT', VKFRAME_PS_SHIFT), ('VS_SHIFT', VKFRAME_VS_SHIFT)):
		if '\n%s="%s"\n' % (n, ' '.join(a)) not in vf:
			bad.append('src/features/vkframe/vkframe_spirv.sh: its %s changed' % n)
	if '\t"$DXC" -spirv -fspv-target-env=vulkan1.2 -HV 2021 -T "$3" -E main -D WW_VK_BINDLESS $4 ${5:-} -fspv-preserve-bindings -Fo "$1.spv" ' not in vf:
		bad.append('src/features/vkframe/vkframe_spirv.sh: its fxb() compile line changed')
	blines = dict((k, (f, p, s, x)) for k, f, p, s, x in
	              re.findall(r'^fxb ([a-z0-9_]+) ([a-z0-9_]+) ([vp]s_6_0) "\$([A-Z_]+)"(?: "([^"]*)")?$', vf, re.M))
	if blines != VKFRAME_FXBL:
		bad.append('src/features/vkframe/vkframe_spirv.sh: its fxb lines (%s) are not this checker\'s bl twins' % ' '.join(sorted(blines)))
	if '\t"$DXC" -spirv -fspv-target-env=vulkan1.2 -HV 2021 -T "$3" -E main -D WW_CELL_FOGLEAK=1 -D WW_FRAME_UBO=1 $4 ${5:-} -fspv-preserve-bindings ' \
			'-fvk-use-dx-position-w -Fo "$1.spv" ' not in vf:
		bad.append('src/features/vkframe/vkframe_spirv.sh: its fxc() compile line changed')
	clines = dict((k, (f, p, s, x)) for k, f, p, s, x in
	              re.findall(r'^fxc ([a-z0-9_]+) ([a-z0-9_]+) ([vp]s_6_0) "\$([A-Z_]+)"(?: "([^"]*)")?$', vf, re.M))
	if clines != VKFRAME_FXCELL:
		bad.append('src/features/vkframe/vkframe_spirv.sh: its fxc lines (%s) are not this checker\'s cell fallbacks' % ' '.join(sorted(clines)))
	if '\t"$DXC" -spirv -fspv-target-env=vulkan1.2 -HV 2021 -T "$3" -E main -D WW_CELL_AIRZONE=1 -D WW_CELL_FOGLEAK=1 -D WW_FRAME_UBO=1 $4 ${5:-} ' \
			'-fspv-preserve-bindings -fvk-use-dx-position-w -Fo "$1.spv" "$(cygpath -m "$ROOT/res/shaders/hlsl/particles/$2.hlsl" ' not in vf:
		bad.append('src/features/vkframe/vkframe_spirv.sh: its fxp() compile line changed')
	plines = dict((k, (f, p, s, x)) for k, f, p, s, x in
	              re.findall(r'^fxp ([a-z0-9_]+) ([a-z0-9_]+) ([cvp]s_6_0) "\$([A-Z_]+)"(?: "([^"]*)")?$', vf, re.M))
	if plines != VKFRAME_FXP:
		bad.append('src/features/vkframe/vkframe_spirv.sh: its fxp lines (%s) are not this checker\'s particle twins' % ' '.join(sorted(plines)))
	return bad


def tool_stamp(g):
	out = subprocess.run([g, '--version'], capture_output=True, text=True, timeout=60).stdout
	m = re.search(r'Glslang Version:\s*(\d+):(\S+)', out)
	return (m.group(1) + m.group(2)) if m else None


def first_diff(a, b):
	wa = re.findall(rb'0x[0-9a-f]{8}', a)
	wb = re.findall(rb'0x[0-9a-f]{8}', b)
	for i in range(min(len(wa), len(wb))):
		if wa[i] != wb[i]:
			return 'words %d vs %d, first differing word %d' % (len(wa), len(wb), i)
	return 'words %d vs %d, common prefix equal' % (len(wa), len(wb))


def main(argv):
	if len(argv) < 3:
		print('usage: check_spv.py <tree> <scratch> [--glslang exe] [--committed name=file]...')
		return 2
	tree, scratch = argv[1], argv[2]
	g = None
	swap = {}
	i = 3
	while i < len(argv):
		if argv[i] == '--glslang':
			g = argv[i + 1]; i += 2
		elif argv[i] == '--committed':
			n, f = argv[i + 1].split('=', 1); swap[n] = f; i += 2
		else:
			print('unknown arg', argv[i]); return 2
	os.makedirs(scratch, exist_ok=True)
	fails, skips = [], []
	for s in script_ok(tree):
		fails.append(s)
	# every committed SPIR-V header has a generator line
	listed = set('%s/%s.spv.h' % (d, k) for d, k, e, v in SPECS + HLSL_SPECS)
	for root, dirs, files in os.walk(os.path.join(tree, 'src')):
		for f in files:
			if f.endswith('.spv.h'):
				rel = os.path.relpath(os.path.join(root, f), tree).replace('\\', '/')
				if rel not in listed:
					fails.append('%s: no generator in this checker (add it to SPECS and its tools/*_spirv.sh)' % rel)
	for n in listed:
		if not os.path.isfile(os.path.join(tree, n)):
			fails.append('%s: listed but not on disk' % n)
	for n in swap:
		if not any(n == '%s.spv.h' % k for d, k, e, v in SPECS + HLSL_SPECS):
			fails.append('--committed %s: no such header' % n)
	g = g or os.environ.get('GLSLANG') or (DEFAULT_G if os.path.isfile(DEFAULT_G) else None) \
		or shutil.which('glslangValidator')
	if not SPECS:   # every kernel builds from HLSL: no glslang header to compare
		stamp = None
	elif not g or not os.path.isfile(g):
		skips.append('no glslangValidator (set GLSLANG=<exe>; RenderDoc ships one under plugins/spirv)')
		stamp = None
	else:
		stamp = tool_stamp(g)
		print('glslang %s stamp %s' % (g, stamp))
		if not stamp:
			skips.append('cannot read the glslang version of %s' % g)
	if stamp:
		for d, k, env, vn in SPECS:
			name = '%s.spv.h' % k
			committed = swap.get(name, os.path.join(tree, d, name))
			if not os.path.isfile(committed):
				continue
			c = open(committed, 'rb').read()
			cs = c.split(b'\n', 1)[0].strip().lstrip(b'/').strip().decode('ascii', 'replace')
			if cs != stamp:
				skips.append('%s: made by glslang %s, this machine has %s (bytes not comparable)' % (name, cs, stamp))
				continue
			out = os.path.join(scratch, d.replace('/', '_') + '_' + name)
			if os.path.exists(out):
				os.remove(out)
			src, defs = FROM.get(k, (k, []))
			r = subprocess.run([g, '--target-env', env, '-V'] + defs + ['--vn', vn, '-o', out, src + '.comp'],
			                   cwd=os.path.join(tree, d), capture_output=True, text=True, timeout=300)
			if r.returncode != 0 or not os.path.isfile(out):
				fails.append('%s: glslang rc %d: %s' % (name, r.returncode, (r.stdout + r.stderr).strip()[-300:]))
				continue
			a = open(out, 'rb').read()
			if a == c:
				print('OK    %s/%s %d B' % (d, name, len(c)))
			else:
				why = 'line endings only' if a.replace(b'\r\n', b'\n') == c.replace(b'\r\n', b'\n') else first_diff(a, c)
				fails.append('%s/%s: STALE, rebuilt %d B vs committed %d B (%s); run tools/%s_spirv.sh'
				             % (d, name, len(a), len(c), why, 'bakert' if d == 'src/features/gi/bakert' else 'rtddgi'))
	dxc = os.environ.get('DXC') or (DEFAULT_DXC if os.path.isfile(DEFAULT_DXC) else None) or shutil.which('dxc')
	if not dxc or not os.path.isfile(dxc):
		skips.append('no dxc (set DXC=<exe>): the HLSL kernels are not compared')
	else:
		for d, k, env, vn in HLSL_SPECS:
			name = '%s.spv.h' % k
			committed = swap.get(name, os.path.join(tree, d, name))
			spv = os.path.join(scratch, 'hlsl_' + k + '.spv')
			out = os.path.join(scratch, 'hlsl_' + name)
			for f in (spv, out):
				if os.path.exists(f):
					os.remove(f)
			hs, hdefs = HLSL_FROM.get(k, (k, []))
			src = os.path.join(tree, HLSL_SRC.get(k, HLSL_DIR.get(d, 'res/shaders/hlsl/' + os.path.basename(d))), hs + '.hlsl')
			if k in VKFRAME_SKY:
				src = os.path.join(tree, VKFRAME_SKY[k][0], VKFRAME_SKY[k][1] + '.hlsl')
				hdefs = VKFRAME_SKY_ARGS
			lk = k[6:] if k.startswith('light_') and d == 'src/features/vkframe' else None
			if lk in VKFRAME_LIGHT:
				src = os.path.join(tree, 'res/shaders/hlsl/vklight', VKFRAME_LIGHT[lk][0] + '.hlsl')
				hdefs = ['-O0'] + (['-D', VKFRAME_LIGHT[lk][1]] if VKFRAME_LIGHT[lk][1] else [])
			if lk == 'gihalf':
				src = os.path.join(tree, 'res/shaders/hlsl/vklight/gihalf_vk.hlsl')
				hdefs = VKFRAME_GIHALF_ARGS
			if k in VKFRAME_CSM:
				src = os.path.join(tree, VKFRAME_CSM[k][0], VKFRAME_CSM[k][1] + '.hlsl')
			if k in VKFRAME_SSR:
				src = os.path.join(tree, 'res/shaders/hlsl/vklight/ssr_vk.hlsl')
				hdefs = ['-DSSR_LIVE'] + VKFRAME_SSR[k].split()
			if k in VKFRAME_FX:
				src = os.path.join(tree, 'res/shaders/hlsl', VKFRAME_FX[k] + '.hlsl')
				hdefs = VKFRAME_SKY_ARGS
			if k in VKFRAME_FXV:
				src = os.path.join(tree, 'res/shaders/hlsl', VKFRAME_FXV[k] + '.hlsl')
				hdefs = VKFRAME_SKY_ARGS
			if k in VKFRAME_FXS:
				src = os.path.join(tree, 'res/shaders/hlsl', VKFRAME_FXS[k] + '.hlsl')
				hdefs = VKFRAME_SKY_ARGS
			if k in VKFRAME_FXK:
				src = os.path.join(tree, 'res/shaders/hlsl', VKFRAME_FXK[k] + '.hlsl')
				hdefs = VKFRAME_SKY_ARGS + ['-fspv-preserve-bindings']
			if k in VKFRAME_FXKW:
				src = os.path.join(tree, 'res/shaders/hlsl', VKFRAME_FXKW[k] + '.hlsl')
				hdefs = VKFRAME_SKY_ARGS + ['-fspv-preserve-bindings', '-fvk-use-dx-position-w']
			if k in VKFRAME_FXC:
				src = os.path.join(tree, 'res/shaders/hlsl', VKFRAME_FXC[k] + '.hlsl')
				hdefs = VKFRAME_FXC_ARGS
			if k in VKFRAME_FXW:
				src = os.path.join(tree, 'res/shaders/hlsl', VKFRAME_FXW[k] + '.hlsl')
				hdefs = VKFRAME_FXW_ARGS
			if k in VKFRAME_FXL:
				src = os.path.join(tree, 'res/shaders/hlsl', VKFRAME_FXL[k] + '.hlsl')
				hdefs = VKFRAME_FXL_ARGS
			if k in VKFRAME_FOG:
				src = os.path.join(tree, VKFRAME_FOG_SRC)
				hdefs = VKFRAME_FOG[k].split()
			if k in VKFRAME_FXB:
				src = os.path.join(tree, 'res/shaders/hlsl', VKFRAME_FXB[k] + '.hlsl')
				hdefs = VKFRAME_FXB_ARGS
			if k in VKFRAME_FXBL:
				src = os.path.join(tree, 'res/shaders/hlsl/near', VKFRAME_FXBL[k][0] + '.hlsl')
				hdefs = ['-HV', '2021', '-D', 'WW_VK_BINDLESS'] + (VKFRAME_VS_SHIFT if VKFRAME_FXBL[k][2] == 'VS_SHIFT' else VKFRAME_PS_SHIFT) \
					+ VKFRAME_FXBL[k][3].split() + ['-fspv-preserve-bindings']
			if k in VKFRAME_FXCELL:
				src = os.path.join(tree, 'res/shaders/hlsl/core', VKFRAME_FXCELL[k][0] + '.hlsl')
			if k in VKFRAME_FXP:  # lane VKOBJ5 (G4 round 3): particles_cell's twins
				src = os.path.join(tree, 'res/shaders/hlsl/particles', VKFRAME_FXP[k][0] + '.hlsl')
			if k in VKFRAME_FXG:
				src = os.path.join(tree, 'res/shaders/hlsl', VKFRAME_FXG[k] + '.hlsl')
				hdefs = VKFRAME_FXG_ARGS
			if k in VKFRAME_FXR:
				src = os.path.join(tree, 'res/shaders/hlsl', VKFRAME_FXR[k] + '.hlsl')
				hdefs = VKFRAME_FXR_ARGS
			if k in VKFRAME_FXT:
				src = os.path.join(tree, 'res/shaders/hlsl', VKFRAME_FXT[k] + '.hlsl')
				hdefs = VKFRAME_FXT_ARGS
			if k in VKFRAME_FXF:  # the profile is the table's (lens_flare.hlsl holds both stages)
				src = os.path.join(tree, 'res/shaders/hlsl', VKFRAME_FXF[k][0] + '.hlsl')
				hdefs = []
				m = re.match(r'([vp]s_6_0)', VKFRAME_FXF[k][1])
			elif k in VKFRAME_PFX:  # the profile and defines are the table's (one source, several kernels)
				src = os.path.join(tree, 'res/shaders/hlsl', VKFRAME_PFX[k][0] + '.hlsl')
				hdefs = VKFRAME_PFX[k][2].split()
				m = re.match(r'([cvp]s_6_0)', VKFRAME_PFX[k][1])
			elif k in VKFRAME_FXH or k in VKFRAME_FXD or k in VKFRAME_FXCELL or k in VKFRAME_FXP:
				m = None
			else:
				m = re.search(r'hlsl profile: ([cvp]s_6_[0-9])', open(src, encoding='utf-8', errors='replace').read())
			if k in VKFRAME_FXV or k in VKFRAME_FXC or k in VKFRAME_FXW or k in VKFRAME_FXL or k in VKFRAME_FXB or k in VKFRAME_FXT:
				m = re.match(r'(vs_6_0)', 'vs_6_0')
			if k in VKFRAME_FXH:
				src = os.path.join(tree, 'res/shaders/hlsl', VKFRAME_FXH[k] + '.hlsl')
				hdefs = VKFRAME_FXH_ARGS
				m = re.match(r'(hs_6_0)', 'hs_6_0')
			if k in VKFRAME_FXD:
				src = os.path.join(tree, 'res/shaders/hlsl', VKFRAME_FXD[k] + '.hlsl')
				hdefs = VKFRAME_FXD_ARGS
				m = re.match(r'(ds_6_0)', 'ds_6_0')
			if k in VKFRAME_FXG or k in VKFRAME_FXR:
				m = re.match(r'(ps_6_0)', 'ps_6_0')
			if k in VKFRAME_FXS:
				m = re.match(r'(cs_6_0)', 'cs_6_0')
			if k in VKFRAME_FXBL:
				m = re.match(r'(.*)', VKFRAME_FXBL[k][1])
			if k in VKFRAME_FXCELL:
				hdefs = VKFRAME_FXCELL_ARGS + (VKFRAME_VS_SHIFT if VKFRAME_FXCELL[k][2] == 'VS_SHIFT' else VKFRAME_PS_SHIFT) \
					+ VKFRAME_FXCELL[k][3].split() + ['-fspv-preserve-bindings', '-fvk-use-dx-position-w']
				m = re.match(r'(.*)', VKFRAME_FXCELL[k][1])
			if k in VKFRAME_FXP:
				hdefs = VKFRAME_FXP_ARGS + (VKFRAME_VS_SHIFT if VKFRAME_FXP[k][2] == 'VS_SHIFT' else VKFRAME_PS_SHIFT) \
					+ VKFRAME_FXP[k][3].split() + ['-fspv-preserve-bindings', '-fvk-use-dx-position-w']
				m = re.match(r'(.*)', VKFRAME_FXP[k][1])
			opt = [] if d == 'src/features/vkframe' else ['-O0']   # vkframe_spirv.sh builds optimised
			r = subprocess.run([dxc, '-spirv', '-fspv-target-env=' + env, '-T', m.group(1) if m else 'ps_6_0' if k in VKFRAME_SKY or k in VKFRAME_FX or k in VKFRAME_FXK or k in VKFRAME_FXKW else 'cs_6_0', '-E', 'main']
			                   + opt + hdefs + ['-Fo', spv, src], capture_output=True, text=True, timeout=300)
			if r.returncode == 0:
				r = subprocess.run([sys.executable, '-I', os.path.join(tree, 'tools/spv_header.py'), spv, vn, out],
				                   capture_output=True, text=True, timeout=60, stdin=subprocess.DEVNULL)
			if r.returncode != 0 or not os.path.isfile(out):
				fails.append('%s: dxc / spv_header rc %d: %s' % (name, r.returncode, (r.stdout + r.stderr).strip()[-300:]))
				continue
			a, c = open(out, 'rb').read(), open(committed, 'rb').read()
			if a == c:
				print('OK    %s/%s %d B (HLSL)' % (d, name, len(c)))
			else:
				fails.append('%s/%s: STALE, rebuilt %d B vs committed %d B (%s); run %s'
				             % (d, name, len(a), len(c), first_diff(a, c), 'src/features/vkframe/vkframe_spirv.sh'
				                if d == 'src/features/vkframe' else 'src/features/gi/%s_spirv.sh' % os.path.basename(d)))
	for f in fails:
		print('FAIL  ' + f)
	for s in skips:
		print('SKIP  ' + s)
	if fails:
		print('SPVSYNC FAIL %d' % len(fails)); return 1
	if skips:
		print('SPVSYNC SKIP %d' % len(skips)); return 77
	print('SPVSYNC PASS %d' % len(SPECS + HLSL_SPECS)); return 0


if __name__ == '__main__':
	sys.exit(main(sys.argv))
