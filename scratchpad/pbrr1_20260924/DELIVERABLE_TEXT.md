# PBRR1 deliverable text

## HANDOFF text

**PBR renderer R1 (detect + load) -- built, gated, NOT committed (lane PBRR1, 2026-09-24 03:30).**
exe `ae101325` (linked 03:04); rung `release/before_pbrr1` = `8485154d` (PBRR0).
- PBRM v6 reader (envelopes 4/5/6). The F0 law is chosen by envelope: v6 = min(weight x ((ior-1)/(ior+1))^2, 1) x tint; v4/v5 = min(f0, 0.16).
- ONE candidate function, `src/io/pbrmresolve.{h,cpp}` (`pbrmResolve`), shared by the viewport (glproperty), lodgen (`lodgenResolveMaterialMask`, report only, legacy mask unchanged) and `-no-gui pbrm-resolve`. Order: swap > .nifx > (direct | sibling) > FO76 BGSM > legacy (+ stem for lodgen).
- `.nifx` parser/writer `src/io/nifxfile.{h,cpp}`: span-preserving, so unknown sections, key order, spacing and CRLF survive byte for byte. New CLI command: `-no-gui nifx <in> [--set node=pbrm] [--remove node] [--canonical] [--out f]`.
- A texture that fails to load aborts the PBR bind. The shape draws legacy, and the census refusal reads `texture load failed: <slot> <path>; PBR binding aborted`.
- The PBR path is enabled and its menu entries are no longer greyed, but the display default stays Legacy (Q9). Auto-replace now defaults on.
- View menu > "PBR Route View" (not persisted) tints each shape by its route: swap magenta, nifx cyan, direct yellow, sibling green, fo76 orange, stem blue, legacy white. The harness pin is `WW_PBRM_ROUTE_VIEW=1`. No Scene popup was made.
- The census header reads `stage=R1`. Rows carry route/path/envelope/f0/refusal. `f0=` is the value read back from the program (`glGetUniformfv pbrF0`); it reads `unread` when nothing was read.
- New pins: `WW_PBRM_ORDER`, `WW_PBRM_F0_LAW` (red controls), `WW_PBRM_SWAP=<mat>><swapmat>;...` and `WW_PBRM_ROUTE_VIEW`. The swap source is the pin only; there is no ESP MSWP reader yet.

Gates:
- `tests/spells/pbr_r1_gates.sh`: 48/0 PASS, and reds coverage/order/order_e/f0law/nifx all bite.
- `pbr_shade_ab.sh --old release/before_pbrr1`: 10 cases, 0 failures (shader red bites 7/7).

Known gaps:
- `-no-gui pbrm-resolve` finds no resources at all, not even vanilla BGSMs, even with `WW_LODGEN_RESOURCES`. The gates use the viewport census instead.
- `tests/spells/lodgen_terrain_pbrm.sh` FAILS 5 of 14 checks (T2a/T2/T3), identically on the PBRR0 rung and on R1. The failure predates this lane and is still open.
- `tests/fixtures/pbr_data` holds vanilla/FO76 copies and is not git-ignored. Never add it; regenerate it with `pbr_r1_fixtures.py`.

Next: bungo's in-viewer look at the route view and PBR mode (restart his window). Then R2.

## WW_CHANGES text

- **PBR materials, stage R1.** NifSkope now finds and loads a PBR material for a shape in this order: a material swap, a `.nifx` sidecar beside the NIF, a `.pbrm` named directly in the shader property or sitting beside its BGSM/BGEM, a Fallout 76 BGSM, and otherwise the legacy material. It reads PBRM versions 4, 5 and 6 (v6 speculars from weight and IOR).
  - The Lighting menu's PBR mode is selectable. The default stays Legacy, so nothing you see changes until you pick it.
  - If a PBR texture cannot load, the shape falls back to legacy, and the census says which texture failed.
  - New View menu entry "PBR Route View" colours every shape by where its material came from.
  - New command line: `NifSkope -no-gui nifx <file.nifx> [--set node=material.pbrm] [--remove node] [--out file]` edits a `.nifx` without disturbing anything else in it.
- Harnesses: `tests/spells/pbr_r1_gates.sh` (R1 gates + five red controls, fixtures from `pbr_r1_fixtures.py`). `pbr_shade_ab.py` compares the PBR census across stages on shape/kind/program/route.

## MISTAKES text

- **2026-09-24, lane PBRR1: a heredoc edit again.** A shell heredoc was used to patch `pbr_r1_gates.sh`, and it turned `\n`/`\r` escapes into literal bytes. A Python table printed on Windows also ended its lines in `\r`, which reached `env` as an argument (rc=127). Rule: patch scripts are written with the Write tool, and a shell loop strips `\r` from any Python-generated table it reads.
- **2026-09-24, lane PBRR1: a colour-band coverage test misread shading as holes.** Gate (a) first called a pixel "covered" when it differed from the background by more than 6. The PBR duct is darker, so 2.2% of its texels came within 6 of the grey background and read as missing (0.978 < 0.99) on a correct render. The background is one flat colour, so the test is now "differs at all" (1.000). The discard red still fails 8 of 9 cases. Rule: coverage is measured against the exact clear colour, not against a tolerance band.
- **2026-09-24, lane PBRR1: a relative `--out` gave no pictures.** The exe resolves `WW_RENDER_SHOT` against its own working directory. Harness `--out` paths are absolute.
- **2026-09-24, lane PBRR1: the fixture root was not on the resource stack.** NifSkope's NIF-local root is the NIF's own directory, not the parent of `Meshes`, so the loose test folder was invisible until it was added to `WW_LODGEN_RESOURCES` (last entry wins).

## Skill review

- **New: `.claude/skills/nifskope-ww-pbr-shade-ab/SKILL.md`**. It covers both PBR harnesses:
  - the rung-folder rule and absolute `--out`;
  - noise bar and pins;
  - cross-stage census projection;
  - reds;
  - the resource-stack rule for loose fixtures;
  - exact-background coverage;
  - read-back F0;
  - the CLI resolve gap.
- **Edited: `nifskope-ww-render-shot`**. Added a note after the switch table: MPS* particle NIFs photograph empty; use ShockHAndLeft.nif / CryoJet01.nif for particle pixels.
- **Proposed** (not written; for whoever fixes it): the `nifskope-ww-lodgen` skill should note that `lodgen_terrain_pbrm.sh` currently fails 5 of 14 on both PBRR0 and R1, so a lane touching lodgen compares old vs new output rather than reading its verdict.
