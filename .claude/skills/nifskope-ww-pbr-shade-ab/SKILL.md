---
name: nifskope-ww-pbr-shade-ab
description: Prove a NifSkope Wild Wasteland Edition renderer change leaves the legacy picture untouched, or aim a PBR gate, with the two PBR harnesses -- tests/spells/pbr_shade_ab.sh (OLD rung folder vs NEW release/, pixel zero set + program/camera/PBRM census) and tests/spells/pbr_r1_gates.sh (the loose PBR test folder: coverage, route vs an independent Python resolver, read-back F0, .nifx round trip, direct link, texture failure, route view). Also the weather preview gates (tests/spells/pbr_wx1_gates.sh: sky dome, sun, clouds, moon) and pbr_fog1_gates.sh (the weather fog: CLI, shader probes, distance scale). Use for every PBR renderer lane (PBRR*), for any change to shaders/, glproperty.cpp, renderer.cpp or the PBRM resolver, and whenever a new toggle must be shown pixel-identical in its OFF state.
---

# NifSkope WW: the PBR shade A/B and the R1 gates

Spec: `docs/NIFSKOPE_PBR_RENDERER.md` s9 (harness), s13 (gates), RULINGS 2026-09-23.
Build first with `nifskope-ww-build-verify`; everything here runs `release/NifSkope.exe`
(and a frozen rung folder) and never builds.

## 0. Before any run
* `tasklist | grep -i -E "Fallout4|NifSkope"` -- no game; no `--port` NifSkope left.
  bungo's own window (no `--port`) is never touched. One harness instance at a time.
* **`--out` is an ABSOLUTE path.** A relative `--out` gives `NO PICTURE` on every run
  (the exe resolves `WW_RENDER_SHOT` against its own cwd). Put it under
  `scratchpad/<lane>/`.
* A rung is a FOLDER, not an exe: shaders load from `applicationDirPath`. Make
  `release/before_<lane>` from the current `release/` (exe, dlls, `shaders/`, xmls,
  `platforms/` ...) BEFORE the first build of the lane, and record its sha1.

## 1. The zero set: `pbr_shade_ab.sh`
```bash
bash tests/spells/pbr_shade_ab.sh --old release/before_<lane> --out "$PWD/scratchpad/<lane>/ab"
bash tests/spells/pbr_shade_ab.sh --old release/before_<lane> --out ".../ab_red" --red shader
```
* Per case OLD three times (the noise bar, a|b and a|c), NEW once. `zero` passes when the
  NEW diff is inside the OLD-vs-OLD noise on pixel count AND max.
* Every pin is set identically in both arms; a case line may override one for both.
  Cases: `tests/spells/pbr_shade_ab_cases.txt`.
* Census: program and camera censuses must be identical. The PBRM census is compared
  whole within one stage; **across stages** (old header `stage=R0`, new `stage=R1`) it
  is compared on the projection `shape/kind/prog/route`, because a new stage writes new
  fields and refusal wording by design. In a Legacy-mode run at R1+ every row must be
  `route=legacy`. Since R3 (lane PBRR3) the DISPLAY DEFAULT is Legacy and PBR (ruling
  Q9): an unpinned run draws a .pbrm-backed shape PBR. The zero set is unaffected
  (vanilla ships no .pbrm), but pin `WW_PBRM_MODE` in any harness that needs Legacy.
* Reds: `--red shader` (legacy frags scaled 0.9 -> zero FAILS on every case with
  pixels) and `--red census` (fo4_default.prog cannot match -> census FAILS).
* Particles: `MPSFireSmall01` / `MPSSmokeFireMed01` / `AttachFXMist01` draw NOTHING in
  this viewer (multi-target emitter not simulated; the mist reaches no pixel) and read
  `PASS-EMPTY`. The cases that guard particle pixels are **ShockHAndLeft.nif** and
  **CryoJet01.nif**. Never count a PASS-EMPTY as particle coverage.

## 2. The R1 gates: `pbr_r1_gates.sh` + `pbr_r1_gates.py`
```bash
python tests/spells/pbr_r1_fixtures.py          # rebuilds tests/fixtures/pbr_data (loose test folder only)
bash tests/spells/pbr_r1_gates.sh --out "$PWD/scratchpad/<lane>/gates"
for r in coverage order order_e f0law nifx; do
  bash tests/spells/pbr_r1_gates.sh --red $r --out "$PWD/scratchpad/<lane>/red_$r"; done
```
* Fixtures are byte copies of the vanilla AC duct connector with the ONE material string
  rewritten to a same-length name. Cases sibling_v6, sibling_v5, direct, nifx, direct_nifx,
  swap, fo76, texfail, legacy; `cases.json` holds each case's intent.
* **The fixture root must be ON THE RESOURCE STACK**: `WW_LODGEN_RESOURCES="<game data>;<pbr_data>"`
  (last wins). NifSkope's NIF-local root is the NIF's OWN directory, not the parent
  of `Meshes`, so a loose folder is otherwise invisible.
* Per case three shots: `_legacy`, `_pbr` (`WW_PBRM_MODE=pbr`) and `_route`
  (`WW_PBRM_ROUTE_VIEW=1`), each with its census.
* The judge's resolver is written from the spec (swap > nifx > direct|sibling > fo76 >
  legacy over the files on disk) and is itself checked against `cases.json` intent, so a
  broken resolver cannot agree with a broken renderer.
* Coverage counts a pixel as drawn when it differs from the flat corner colour AT ALL
  (> 0). A +-6 band read 2.2% of a correctly drawn, darker PBR duct as missing.
* F0 is the value READ BACK from the program (`glGetUniformfv pbrF0`), not the law's
  intent; `f0=unread` means nothing was read.
* Reds: `coverage` (PBR frag discards), `order` (swap<->nifx), `order_e` (nifx<->direct),
  `f0law` (v5 law: v6 twin reads 1.000), `nifx` (judged on `--canonical` output). Each
  must print `RED CONTROL <kind> ... BITES`.

## 2b. The R3 gates: `pbr_r3_gates.sh` + `pbr_r3_gates.py` (lane PBRR3)
```bash
bash tests/spells/pbr_r3_gates.sh --out "$PWD/scratchpad/<lane>/r3"      # rebuilds tests/fixtures/pbr_r3_data
for r in noms nosplit f0law fo4csweight notint q9legacy; do
  bash tests/spells/pbr_r3_gates.sh --red $r --out "$PWD/scratchpad/<lane>/r3_red_$r"; done
```
* Fixture: the vanilla `Preview/PreviewSphere01.nif` (radius 128) with its shader Name
  pointed at a per-case BGSM + sibling .pbrm (constants only), under an ORTHOGRAPHIC
  front camera (`WW_RENDER_ORTHO=300`), so the ring at 0.866 R is exactly 60 degrees.
* The white furnace = `WW_STUDIO_CUBE=<uniform white cube>` + `WW_STUDIO_SUN=0` +
  `WW_EXPOSURE_EV=log2(0.8)`: 1.0 lands at sRGB 231, not on the clip.
* Term isolation pins: `WW_R3_TERM=diffuse|specular` (emission off in both). Reds:
  `WW_R3_RED=noms|nosplit|fo4csweight|notint`, plus `WW_PBRM_F0_LAW=v5` and
  `WW_PBRM_MODE=legacy` (q9). Each must print `RED <kind> ... -> OK`.
* The disk mask is `WW_STUDIO_PROBE=0.5`, and **the probe replaces the lit value BEFORE
  the exposure**: it reads 0.5 x 2^EV, not 0.5 (sRGB 170 at EV log2(0.8)).
* In this DFG the metal furnace (base 1, tint 1) is independent of the view angle
  (A + B = 1 - 0.55 r), so the 60-degree ring only discriminates the dielectric.
* Judges: a numpy bool is never `is False` -- coerce with `bool()` before a red reads
  it, or the red prints `absent -> BROKEN` for a gate that failed.

## 2c. The WX1 gates: `pbr_wx1_gates.sh` + `pbr_wx1_gates.py` (lane PBRWX1)
The weather preview in the Scene popup (Sky dome, Sun disc + light, Clouds, Moon, Game
Day). The judge has its OWN plugin decoder (WTHR/CLMT/IMSP/IMGS/GMST), textbook CIELab
and a DXT5 decode; it never calls the code under test.
```bash
bash tests/spells/pbr_wx1_gates.sh --out "$PWD/scratchpad/<lane>/wx1"            # 71 checks
bash tests/spells/pbr_wx1_gates.sh --out ".../wx1_red_<n>" --red <n>              # must FAIL
bash tests/spells/pbr_wx1_gates.sh --out ... --only sky,skypx,probe,lit,off,persist,live
```
* Sections: `sky` (one `weather --sky` CLI run: colours at hours, sun direction, the
  disc-fade and colour-extension edges, cloud rows, moon at days 4/17/32), `skypx` (the
  dome's zenith looking straight up), `probe` (`WW_LOOKDEV_CLOUDPROBE=layer,u,v`: left
  half colour, right half alpha; texel picked by `pbr_wx1_gates.py --pick 14`, which
  demands a V-flip twin that differs so a flipped V fails), `lit` (Sun on = the arc,
  Sun off = W1's light), `off` (every part OFF = `release/before_<lane>` byte for byte,
  pinned and unpinned), `persist`, `live` (`WW_SCENE_TEST_WEATHER=1`).
* Reds: CLI `WW_LOOKDEV_RED=exegmst rgbblend nonightbranch sunfade2h sunalpha1
  colorext05 phasestuck moonalpha1 nam1ignore speedswap cloudalphaone hourstuck
  cloudgametime todorder`; stage `skyscale skygamma skyswap skyleak sunleak cloudleak
  moonleak`; UI `WW_R2A_RED=nolive nosave`. A red runs only the sections aimed at it.
* **The lookdev echo lives in the PBRM census (`WW_PBRM_CENSUS`), not the program
  census.** `drew=sky:dome(...)` or `drew=sky:refused(<why>)` is the first thing to read
  when a sky picture looks wrong: a refused dome still photographs as the lookdev cube.
* **A fresh settings scope has no archive index.** `GameManager` builds it only while
  Fallout 4 is enabled in Resources, so `get_file` fails on meshes while textures still
  resolve through the resource stack, and `GameManager::folders()` returns EMPTY too.
  Sky meshes therefore fall back to `lodgenResourceSearchPaths()`, whose loose entries
  are `<entry>/Meshes`, `<entry>/Textures` -- join the relative path to the PARENT.
* Moon azimuth = `atan2(x, y) mod 360` (north 0, east 90). The 01:00 moon is el 61.56
  az 180.00; 22:00 is 33.66 / 223.07.
* The probe's measured texel (layer 14, uv 0.52197266 0.13415527) reads colour
  (172,188,195) and alpha 140 = 156 x 0.9; after 100 s it has moved by 0.2125984 in U
  and it wraps at 470.37 s.
* Every PBR gate (`shade_ab`, `r1`..`r4`, `wx1`) refuses a shot only while a NifSkope on ITS OWN
  `--port` is up. They used to match any `--port`, so another lane's harness made them refuse
  and report FAIL. Give each run its own port (`PBR_AB_PORT`, `PBR_R1_PORT` ...).
* **Never edit a driver while bash runs it.** bash reads the script as it goes; an edit
  mid-run gave `FR: command not found` and a syntax error, and the run died with no
  verdict. Copy the script aside first if it must change during a run.
* `( : >> release/NifSkope.exe )` succeeding does NOT mean the exe is free to link: the
  link still died "Permission denied" with a harness on it. Rename it aside whenever any
  NifSkope runs from `release/`.

## 2d. The FOG1 gates: `pbr_fog1_gates.sh` + `pbr_fog1_gates.py` (lane FOG1)
The WTHR fog in the Scene popup (the Fog row). The judge re-derives every number from the
plugin bytes with the WX1 judge's decoder, CIELab and clock (imported, never the app).
```bash
bash tests/spells/pbr_fog1_gates.sh --out "$PWD/scratchpad/<lane>/fog1"           # 59 checks
bash tests/spells/pbr_fog1_gates.sh --out ".../fog1_red_<n>" --red <n>            # must FAIL
```
* Sections: `fog` (`weather --fog --fog-probe "d,z;..."`: FNAM/NAM4 as read, day weight,
  blended FNAM, the 4 colours, cb12[41..46], every probed fragment), `alpha`/`colour`/
  `height` (`WW_LOOKDEV_FOGPROBE=d,z,mode`: every fogged fragment writes the value RAW,
  mode 1 alpha, 2 colour/2, 3 height blend; the judge reads the bottom quarter's dominant
  colour), `geo` (mode 5 writes R = d/4096, G = 0.5 + z/2000: ground at G 127/128, and
  straight down at x 2000 from 1000 then 2000 units R moves 63 -> 126 = the distance
  scale), `sky`, `near`, `seen`, `off` (vs `release/before_fog1`, pinned + unpinned),
  `live` (`WW_SCENE_TEST_FOG=1`), `pics` (not judged).
* Reds: `WW_LOOKDEV_RED=fogext05 fogpower1 fognoblend fognogamma fognonam4 fognear0
  fogmaxclamp fognoescape fogheight0 fogleak fogsky`, `WW_R2A_RED=nolive nosave`.
* The lookdev ground is an ~8192-unit quad (`kGroundHalf` 4096). A top-down framing
  centred beyond it shows the Lookdev cube, not ground -- keep geometry probes inside
  x,y < ~3000. Axis views (VIEW 1..6) are perspective under the pin.
* Vanilla day fog starts at 3000 units, so at view 8 the fog barely shows on the preview
  ground (noon: 0 px differ; night: 1 level). Look at it from `WW_RENDER_DIST=20000`.

## 3. Known gaps
* `-no-gui pbrm-resolve` finds no resources at all (not even vanilla BGSMs) even with
  `WW_LODGEN_RESOURCES`; the route gates use the viewport census. Do not gate on the CLI
  resolve until that is fixed.
* Heredocs corrupt `\n`/`\r` escapes in patch scripts and in the CASES table (`\r` from
  Python stdout on Windows reaches `env` as an argument: rc=127). Write patch scripts with
  the Write tool; strip `\r` from any Python-generated table a shell loop reads.
