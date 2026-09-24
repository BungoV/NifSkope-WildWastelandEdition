# PBRLODFIX1 -- deliverable text (overseer splices; lane edited none of HANDOFF / WW_CHANGES / MISTAKES)

## HANDOFF text

- LANDED PBRLODFIX1 (terrain PBR bake harness 5/14 -> 14/14). CODE REGRESSION, not a stale harness, and not
  VTNORMAL1. Bisect over the release rungs with the harness's own "with PBRM" bake (maskPbrm):
  before_horizon1 6, before_gltfexport1 (09-19 09:00) 6, before_impostorshow (09-19 09:44, = GLTFEXPORT1's
  landing exe eed91448) 0, before_gltfdefaults 0, before_cellview1 0, before_impostorlight1 0, before_defaults2 0,
  before_vtnormal1 0, e5320fdd 0. Cause: GLTFEXPORT1's hook-up put `gltfExportParseFlag()` in nifcli.cpp's
  SHARED flag loop, for EVERY command, above lodgen's `--data-root` (nifcli.cpp, the `lgDataRoot = next()` line)
  and collision's `--skeleton`. The gltf parser owns `--data-root` and `--skeleton` too, so it ate both:
  every `-no-gui lodgen ... --data-root X` since 2026-09-19 ran WITHOUT its loose root (it quietly fell back to
  the resource stack / game manager), and `collision <nif> --skeleton` refused with
  "gltf: --skeleton needs a value". Fix (src/nifcli.cpp only, +5 LF lines): the gltf flag parser runs only when
  the command is `gltf` or `gltf-export`. Only nifcli.o recompiled. Exe 4d30baa9 (rung
  release/before_pbrlodfix1 = e5320fdd). Settings isolation checked: the -no-gui bake reports
  `msnCacheDir (none)`, it does not inherit his profile. bungo: CLI-only change -- his open window needs no
  restart for it; any CLI bake he ran since 09-19 with `--data-root` pointing at a folder the resource stack
  does not also carry should be re-run.
- Gates on 4d30baa9: lodgen_terrain_pbrm 14 checks 0 failures PASS (red = the same harness on the rung
  e5320fdd, the tree minus this fix: 14 checks 5 failures, the same 5); collision --skeleton ok, byte-identical
  to before_gltfexport1, rung SHADOWED; gltf_export_options rc 0, 0 rows not as registered; lodgen_resources
  PASS; lodgen_terrain_vt 45/0 PASS; pbr_shade_ab zero set (old = release/before_pbrlodfix1): 10 cases, 0 failures, 3 empty by the viewer
  -> PASS.

## WW_CHANGES text

- The glTF export options (added 2026-09-19) were read for every command-line command, not only for `gltf`.
  Two of them share a name with older flags: `--data-root` (the LOD generator's loose data folder) and
  `--skeleton` (the collision report). Since 2026-09-19 a command-line LOD bake silently ignored its
  `--data-root`, and `collision <nif> --skeleton` refused to run. Both work again; `gltf` / `gltf-export`
  read their options exactly as before.
- The far-terrain PBR bake gate (`tests/spells/lodgen_terrain_pbrm.sh`) is back to 14 of 14: its test
  materials live in the loose folder `--data-root` names, so it had been failing the PBR checks since then.

## MISTAKES text

### 2026-09-24 PBRLODFIX1 -- a hook-up into a SHARED argument loop shadowed two commands' flags for five days
- What: GLTFEXPORT1's anchored hook-up (2026-09-19) inserted `else if ( gltfExportParseFlag(...) )` into
  nifcli.cpp's one flag loop that every command shares. The parser answers `--data-root` and `--skeleton`,
  both already owned by lodgen and collision further down the same else-if chain, so their lines became dead
  code. Every lodgen CLI bake with `--data-root` since then ran without its loose root; `collision --skeleton`
  refused.
- Why it lived: the gltf gates only ran gltf; the lodgen gates that pass `--data-root "$DATA"` also pass (or
  fall back to) the same Data through the resource stack, so they stayed green. The one gate whose fixture
  lives ONLY under `--data-root` (lodgen_terrain_pbrm) went red and was not run again until 09-24; the brief
  that found it named VTNORMAL1 as prime suspect, and the rung bisect cleared it.
- Rule: a flag parser added to nifcli's shared loop is scoped to its own command (`cmd == ...`) or its tokens
  are grepped against every `t == QLatin1String( "--x" )` in the loop first. A hook-up's gate runs the
  neighbouring commands' harnesses that pass a flag of the same name, not only its own.
- Lane's own slip: progress.md stamps between 04:0x and 04:15 were typed from feel, not read; corrected in
  the file when the build's 04:15:57 exposed it.

## Skill review

- nifskope-ww-lodgen: add under the harness section: "A lodgen gate whose fixture lives only under
  `--data-root` fails with the arm's census word at 0 (maskPbrm 0) -> first check that the CLI actually
  received the flag: an earlier `else if` in nifcli.cpp's shared flag loop can shadow it (PBRLODFIX1,
  gltfExportParseFlag ate `--data-root` 09-19..09-24). `--print-source` does NOT print the data root; the
  bake's census is the only readback." Also: "bisect a lodgen gate over the release rungs with a one-bake
  probe (scratchpad/pbrlodfix1_20260924/probe.sh: the fixture + the ONE bake that carries the signal,
  ~35 s per rung instead of the harness's 77 s)."
- ww-anchored-hookup: add a rule: "an anchor inside nifcli.cpp's shared argument loop is a hook-up into
  EVERY command; scope the new branch to its command, and list the tokens it consumes against the loop's
  existing `t == QLatin1String` lines in the --check output."
- nifskope-ww-build-verify: no change needed; the chain (make rc, MZ, exe -nt source, object list) was
  followed and caught nothing new.
