# IMPOSTORFIN1 -- text for the director to splice (2026-09-22)

## 1. WW_CHANGES.md entry

### 2026-09-22 -- Impostor cards: vanilla's cut-off by default, the lit card switched on, 512 bakes (lane IMPOSTORFIN1)

- **The default coverage cut is now vanilla's LOD alpha test: 128/255 = 0.502.** The constant is `kImpostorVanillaCut` in `src/gl/impostordraw.h`, used when `alphaThreshold` is negative. The census is `cutoff_census.txt`:
  - It covers 3997 LOD NIFs and 497 .BTO chunks, and none failed to parse.
  - In the chunks, 461 of 980 shapes carry NiAlphaProperty 0x12EC. That is a test with GREATER, no blend, at threshold 128. The other 519 have no alpha property. No other value appears.
  - The per-model tree LOD sources vary between 45 and 128, and each one equals its BGSM alphaTestRef.
  - The old default was the set's floor, 16/255. `WW_IMPOSTOR_ALPHA=0.0627` still draws it.
  - The preview log line now reads `coverage cut: vanilla's LOD alpha test 128/255 = 0.5020 (the set's floor would be ...)`.
- **The card now lights like the mesh path.** IMPOSTORLOOK1's `hookup_cardlight.py` is applied to `res/shaders/impostor_oct.frag`: square-rooted ambient x0.375, square-rooted diffuse, then the tonemap.
  - **Measured, it halves the brightness of the four trees**, from card/mesh 0.94-0.97 to 0.48-0.56. That is luma over the intersection of the mesh and card masks, at the bake directions, at cardRes 512.
  - The rock goes from 1.31 to 0.92.
  - A ruling is owed on whether to keep it.
- The `_n` height<->sway channel swap is PREPARED and not applied: `scratchpad/impostorfin1_20260922/hookup_nswap.py`. Its `--check` passes, with 20 edits over 9 files, and a `--dry` copy was checked for CR, brace and paren deltas. The swap happens only at the three `_n.DDS` encoders, the .lodm files gain `"nlayout": 2`, and the loader refuses an older set by name.
- Build: `release/NifSkope.exe`, 23,811,072 B, 2026-09-22 22:12:21, sha1 bf6aa74900663449de9900b28b1839e07b62474c. The rung was c9db7d09.
  - The build also compiled CELLWORK1's unaccepted working-tree units, because `impostordraw.h` is widely included.
- Gates on that exe:

  | Gate | Result |
  |---|---|
  | `impostor_draw.sh` (fixture blast_n4) | **23 PASS / 1 FAIL**. Row 5, silhouette IoU, reads 0.4979 against a 0.50 floor. The floor was not lowered. The same exe at `WW_IMPOSTOR_ALPHA=0.0627` reads 0.5462 PASS, so the red comes from the ruled cut. Photo is 0.8372, reach 13.7%. |
  | `lodgen_octahedral.sh` | RESULT PASS, 116 ok |
  | `native_lighting.sh` | 21 checks, 0 failures |

- Frame sizes in texels, with `WW_IMPOSTOR_REF` unset (no ladder, aspect rule on):

  | Subject | cardRes 512 | cardRes 256 |
  |---|---|---|
  | blast_n4 | 160x512 | |
  | blast_n8 | 160x512 | |
  | maple_n4 | 240x512 | 128x256 |
  | dead_n4 | 96x512 | |
  | rock_n4 | 512x496 | |

- IoU at the bake directions (harness orbit mean). The columns are the rung exe with its own shader, then the rung exe with the new shader, then the new exe:

  | Subject | Rung, own shader | Rung, new shader | New exe |
  |---|---|---|---|
  | blast_n4 | .898 | .847 | .843 |
  | blast_n8 | .885 | .835 | .835 |
  | maple_n4 | .513 | .488 | .457 |
  | dead_n4 | .779 | .448 | .437 |
  | rock_n4 | .951 | .850 | .853 |

  - The cut alone costs between -0.031 and +0.003.
  - Most of the drop comes from the shader. Part of that drop is the instrument: the mask is colour against the background, so darker card pixels drop out of it.

## 2. MISTAKES.md entries (root file, newest at the top)

- **2026-09-22 IMPOSTORFIN1: I prepended MSYS2 UCRT64's bin to PATH for the gates, and its `python` has no numpy.**
  - Four rows of `impostor_draw.sh` (14, 14a-c), all of `lodgen_octahedral.sh` and all of `native_lighting.sh` died on the import. They read as real failures: "measures units, not 256" and "the gate ran checks, floor 21".
  - The gate prints `0 python is ...` as its first row. Read it.
  - Git-Bash's own `python` (Python39) is the one with numpy. `impostor_draw.sh` adds ucrt64 for g++ by itself.
- **2026-09-22 IMPOSTORFIN1: a rung exe run out of `release/` is not a "before".** `NifSkopeOpenGLContext::updateShaders` (glcontext.cpp:937) loads `applicationDirPath()/shaders`. So `release/NifSkope.before_*.exe` draws with the WORKING TREE's shaders.
  - My first before/after set drew both sides with the new frag.
  - The fix is a rung folder, `scratchpad/impostorfin1_20260922/rung_run/`: the rung exe, the DLLs, and the shaders with the old frag rebuilt byte for byte by `rung_frag.py`, which refuses unless the result is 19978 B, sha1 badf7eb9.
  - Measured, the difference was 0.051 to 0.331 IoU per subject.
- **2026-09-22 IMPOSTORFIN1: `for f in shaders/*; do ... cp release/$f` run from the repo root.** The glob matched nothing, so it ran once with a literal `shaders/*` and copied the NEW frag over the rebuilt old one. `cmp` caught it before any shot was taken.
  - Glob in the directory you mean: `for f in release/shaders/*`.
  - Always `cmp` the one file you meant to differ.

## 3. Skill updates

**nifskope-ww-render-shot** or **ww-reference-card-diagnose**, new section "A BEFORE picture needs the rung's SHADERS too":
- The exe loads `applicationDirPath()/shaders` at run time. A rung exe copied beside the new one draws the new shaders.
- Build a rung folder: the exe, `*.dll`, `qt.conf`, `nif.xml`, `kfm.xml`, `style.qss`, `platforms/`, `imageformats/`, `styles/` and `shaders/`, with every shader the lane changed rebuilt to its pre-lane bytes and checked by sha1.
- Report IoU AND brightness beside every before/after. A card/mesh luma ratio over the mask intersection is 30 lines (`scratchpad/impostorfin1_20260922/tone.py`).
- A colour-against-background mask counts dark card pixels as empty. A darker shader therefore reads as a silhouette loss, so an IoU drop that comes with a shading change is not a shape finding until the tone ratio has been read.

**nifskope-ww-lodgen**, impostor section:
- The draw's default cut is vanilla's 128/255, the fraction of decoded coverage (`kImpostorVanillaCut`). `WW_IMPOSTOR_ALPHA` forces any value, and `0.0627` is the old floor default.
- The harness log line starts `coverage cut: vanilla's LOD alpha test`.
- With `WW_IMPOSTOR_REF` unset and `WW_IMPOSTOR_TILE=512`, the frame sizes are the ones in section 1.
- The `_n` layout-2 swap is ready as `hookup_nswap.py` and waits on a ruling. It is not applied.
