P = r"E:\Projects\NifskopeWildWastelandEdition\scratchpad\lane_hookcam_report.md"
b = open(P, "rb").read()
cr0, n0 = b.count(b"\r"), len(b)
assert cr0 == 0, "report is LF-only, found %d CR" % cr0
assert b"## Build (BUILD3)" not in b

new = b"""
---

## Build (BUILD3)

Appended by lane BUILD3, 2026-09-10. Nothing above was rewritten. Nothing was
committed (CONSTITUTION 8).

### B.1 The build

`tasklist | grep -i -E "Fallout4|NifSkope"` printed `rc=1` at 00:09:43, before
the build, and again before every launch below. Lane CARDFINAL's `DONE` marker
was on disk (00:07). qmake first, because `src/glview.h` gained members:

    QMAKE-RC=0
    BUILD-RC=0
    release/NifSkope.exe  00:13:19
    release/style.qss     00:13:21   (cmp against res/style.qss: in step)

Zero `error:` lines. 27 objects recompiled -- every translation unit that
includes `glview.h`, plus `lodtfile.o` / `nifcli.o` / `btdterrain.o` for the
header lane WATER2 changed.

**`src/nifskope_ui.cpp` now carries BOTH edit sets in one link:** this lane's
three render-hook hunks (written 23:44:06) and lane CARDFINAL's bake changes.
The 23:41:00 exe carried only CARDFINAL's. Neither was touched to make them fit.

**Lane WATER2 rode along.** Its `src/lodtfile.h` (00:09:52) and
`src/lodtfile.cpp` (00:10:06) were on disk when the build started and compiled
clean -- no error, no warning attributable to them. Its `src/nifcli.cpp` (the
`lodl` subcommand) and `src/btdterrain.cpp` (the plane list) were still at their
committed content at 00:13 and are **not** in this exe; if WATER2 has written
them since, it needs its own build.

Every artefact's mtime in one table (CONSTITUTION 4):

| file | mtime | lane |
|---|---|---|
| `src/lodgen.cpp` | 2026-09-09 23:29:31 | CARDFINAL |
| `src/glview.cpp` | 2026-09-09 23:43:00 | HOOKCAM |
| `src/nifskope_ui.cpp` | 2026-09-09 23:44:06 | HOOKCAM + CARDFINAL |
| `src/lodtfile.h` | 2026-09-10 00:09:52 | WATER2 |
| `src/lodtfile.cpp` | 2026-09-10 00:10:06 | WATER2 |
| `src/glview.h` | 2026-09-10 00:02:19 | HOOKCAM |
| `tests/spells/render_shot.sh` | 2026-09-09 23:53:46 | HOOKCAM |
| **`release/NifSkope.exe`** | **2026-09-10 00:13:19** | this build, sha256 `2a9d72b6c236fbb2...` |
| `scratchpad/cardfinal_20260909/cards_perframe` | 2026-09-09 23:49-23:53 | CARDFINAL |
| `transition_*.png` | 2026-09-10 00:19:41 / 00:20:15 / 00:20:49 | this lane |

The exe is newer than every one of the nine changed files under `src`, `res`,
`tools` and `tests`, checked by sweep and not by the file I care about; and no
`.o` of any translation unit including `glview.h` or `lodtfile.h` is older than
its header.

### B.2 The camera: section 7 is 27 of 27, and every prediction held

`tests/spells/render_shot.sh`: **82 checks, 1 failure**. Section 7 -- the whole
of this lane's gate -- is entirely green. Viewport read back from the PNG:
1507x421, as predicted.

| check | pre-registered | measured |
|---|---|---|
| control, no camera switch, `arm=unpinned` | 230.98 px | **231.0** |
| ortho half-width 1024, eye 500 / 1000 / 2000, views Front (5) and Right (4) | 376.75 px, all six | **376.91**, all six, `upp=1.358991` in each |
| ortho half-width 2048 | 188.38 px | **188.82** |
| ortho half-width 4096 | 94.19 px | **94.50** |
| perspective fov 60, eye 500 | 765.06 px | **765.01** |
| perspective fov 60, eye 1000 | 250.91 px | **251.0** |
| perspective fov 60, eye 2000 | 107.04 px | **107.0** |
| look-at moved 400 units at half-width 1024 | 294.34 px | **294.36** |
| census `upp` against the arithmetic | equal | 1.358991 / 1.358991 |
| census viewport against the PNG | equal | 1507x421 / 1507x421 |
| perspective `upp` moves with distance | 1x / 2x / 4x | 1.371378 / 2.742757 / 5.485513 |
| orthographic `upp` does NOT move with distance | equal | 1.358991 at eye 500 and at 2000 |
| the two look-at files differ | differ | `ea644af68f880bcb` / `726c6a81765ba1fd` |
| `WW_RENDER_ORTHO=-5` refused by name | named | `arm=center/dist/fov/refused-WW_RENDER_ORTHO-not-positive/view`, `persp=1` |
| two runs of one pinned camera | identical | `ea644af68f880bcb` twice |
| the pin on a generated `.lodl` | holds | `upp` 53.085601 at half-width 40000, 106.171201 at 80000 |

The look-at row is the one that was RED on the defect -- two look-ats 400 units
apart produced one file, md5 `fff710bd...`. It is green, and green with the
right number: 294.36 px measured against 400 units / `upp` = 294.34 px.

`release/ww_camera_pin.log` exists after a run, which is the tell the skill
tells lanes to check.

### B.3 The one failure, and it is not the camera

    FAIL and the pixel sampler CAN see a window's pixels
         luminance range 169.847 over 56 samples (bar 295.737)

Section 6's floor for lane OFFSCREEN2's visible control. The bar is
`max(3 * desktop_noise, 15)`, and section 0 measured the desktop noise at
**98.579** at 00:14:24 -- a transient. The same region, the same
`screen_watch.ps1`, the same 200x200 at 2120,200, re-run at 00:17 with nothing
running: **range 0.111** over 110 samples, min 19.677, max 19.788. The skill's
own table gives 0.2 for an untouched region, so section 0's sample was ~500x
resting.

The discriminators, all in the same run: every hidden run measured 0.111-0.233
against that inflated bar and passed; the strobe control, which is on a FIXED
bar of 30, measured 251.314 and passed; `hidden and visible photograph the same
pixels` is green (`fedab869884174fb` both), so the `showCursor` change this lane
made did not reach section 6, which is what this lane's PENDING note flagged as
the thing to suspect.

Reported as a number. **Not re-pinned and not fixed** -- a resuming lane
measures the cause and stops. Entered in `MISTAKES.md`: one bar cannot serve a
floor and a ceiling, because noise moves both the same way and should move them
opposite ways.

### B.4 The reached suite

Run sequentially, one instance at a time, on the 00:13:19 exe.

| harness | result | why it was run |
|---|---|---|
| `render_shot.sh` | 82 checks, **1 failure** (B.3) | the hook this lane changed |
| `lodgen_octahedral.sh` | 85 ok, 0 fail | drives the hook's sibling bake block |
| `lodgen_impostor_cards.sh` | 12 ok, 0 fail | same |
| `lod_generation.sh` | 97 checks, 0 failures | `src/lodgen.cpp` is in this link |
| `lodl_open.sh` | 23 checks, 0 failures | reaches `lodtfile.h`/`.cpp` (WATER2) |
| `btd_terrain.sh` | 13 checks, 0 failures | same |
| `lodgen_identity.sh` | 8 ok, 0 fail | the byte-identity floor |

The three that print `RESULT PASS` and no count had their `ok` lines counted
rather than reported blank. All three match lane CARDFINAL's counts on the
23:41 exe exactly (85/12/8), which is the evidence that this build did not move
the bake.

Not run, with the reason: everything else in `tests/spells`. Nothing in this
link touches the block viewer, the panels, the collision or the NIF writers.

### B.5 The transition render: the pin held, the test cannot be met in this scene

**Library used: `scratchpad/cardfinal_20260909/cards_perframe`** -- lane
CARDFINAL's fresh per-frame library, baked on the 23:41 exe, 20 sidecars, all
three trees present. It was passed explicitly as `argv[1]`; the script's own
first candidate (`cardfinal_20260909/cards`) does not exist, so left to itself
it would have fallen back to `cardfit_20260909/cards_after` as this lane's
section 4 predicted. The report's fallback is therefore superseded: the fresh
library was used.

All three chunks baked (`mesh`, `card`, `ctl`, one `Commonwealth.4.-20.24.BTO`
each), the control library was written with `center` zeroed in 20 sidecars, and
all eighteen frames were shot. **The pin held exactly in every one of them**:

| tree | distance | `upp` measured | `2*tan(30)*eye/421` |
|---|---|---|---|
| `0003a28b` | mid 1873.95 | 5.140 | 5.140 |
| `0003a28b` | ring 7495.80 | 20.559 | 20.559 |
| `0004a074` | mid 1457.66 | 3.998 | 3.998 |
| `0004a074` | ring 5830.63 | 15.992 | 15.992 |
| `00038599` | mid 766.78 | 2.103 | 2.103 |
| `00038599` | ring 3067.13 | 8.412 | 8.412 |

And the gate is **UNMET**: 0 of 6 card rows and 0 of 6 control rows pass.

| tree | dist | arm | texel px | centre off, px | in texels | dx ext | dy ext |
|---|---|---|---|---|---|---|---|
| `0003a28b` | mid | card | 3.21 | 186.52 | 58.1 | 31.96% | 22.38% |
| `0003a28b` | mid | ctl | 3.21 | 186.52 | 58.1 | 31.96% | 22.38% |
| `0003a28b` | ring | card | 0.80 | 150.72 | 187.9 | 37.21% | 45.50% |
| `0003a28b` | ring | ctl | 0.80 | 162.88 | 203.1 | 39.61% | 38.50% |
| `0004a074` | mid | card | 2.03 | 48.79 | 24.1 | 4.72% | 26.81% |
| `0004a074` | mid | ctl | 2.03 | 207.33 | 102.4 | 47.82% | 26.81% |
| `0004a074` | ring | card | 0.51 | 316.00 | 624.2 | 70.28% | 37.84% |
| `0004a074` | ring | ctl | 0.51 | 317.22 | 626.6 | 70.28% | 42.23% |
| `00038599` | mid | card | 0.84 | 565.00 | 669.9 | 27.09% | 0.00% |
| `00038599` | mid | ctl | 0.84 | **12.50** | 14.8 | 12.25% | 0.00% |
| `00038599` | ring | card | 0.21 | 196.00 | 929.5 | 30.04% | 0.00% |
| `00038599` | ring | ctl | 0.21 | 152.50 | 723.2 | 23.37% | 0.00% |

Two rows say the numbers are not about card placement. On `0003a28b` mid the
card and the control are IDENTICAL to two decimals, when they differ by the
whole centre offset. On `00038599` mid the CONTROL is 45x closer than the card.
A control beating the thing it is the floor for is the signature of an
instrument that is not measuring what it names.

**The pictures say why, and the arithmetic confirms it.** All three were opened
(CONSTITUTION 5). None of them contains a single tree: the frames are a thicket
of dead trunks over terrain in the mesh arm, and in the card arm the camera is
INSIDE a neighbouring card -- flat untextured quads filling the whole frame.

Measured, per tree, from the bake's own manifest:

| tree | ref | half-extent | camera distance | nearest other ref | that neighbour, off axis | vfov needed to fit the subject | vfov allowed to exclude the neighbour |
|---|---|---|---|---|---|---|---|
| `0003a28b` | 674 | 1055.3 x 1055.3 | 1874 | 892 | 25.5 deg | **58.8 deg** | **15.1 deg** |
| `0004a074` | 109 | 518.2 x 829.0 | 1458 | 726 | 26.5 deg | **59.3 deg** | **15.8 deg** |
| `00038599` | 264 | 113.5 x 454.1 | 767 | 644 | 40.0 deg | **61.3 deg** | **26.4 deg** |

There is **no field of view that does both**, on any of the three trees, at
either distance, and the gap is a factor of 2.3-3.9. This lane's own PENDING
note offered "re-run with a smaller FOV constant if a frame is crowded"; that
route is closed, and the number above is why. It is not the 1507x421 clamp
either: the mid and ring distances are defined as multiples of the card's OWN
half-extent, so the subject fills a ~60 degree frame by construction whatever
the aspect, while the neighbours sit at 25-40 degrees.

So the finding is about the SCENE, not the pin and not the cards: the most
isolated instance of each of these trees in the placed Sanctuary chunk is not
isolated enough to be photographed alone at the distance at which it turns into
a card. A one-texel transition measurement needs a scene containing ONE ref.
That is a design change to the test and **it was not made here** -- a resuming
lane measures the cause and stops.

**Nothing about the card placement is claimed either way by this round.** The
geometric gate lane CARDFIT3 and lane CARDFINAL shipped
(`scratchpad/cardfinal_20260909/transition_bounds.py`, PASS on 3 bases) remains
the only evidence on that question, and it is a discrimination test, not a
one-texel test, exactly as it says.

Pictures, all three opened and all three showing the thicket:

* `scratchpad/hookcam_20260909/transition_0003a28b.png` (TreeHero01, 1874 units)
* `scratchpad/hookcam_20260909/transition_0004a074.png` (TreeMapleForest2, 1458 units)
* `scratchpad/hookcam_20260909/transition_00038599.png` (TreeBlasted01, 767 units)

Raw frames, camera censuses and `transition.json` in
`scratchpad/hookcam_20260909/trans/`.

### B.6 Documents

1. `WW_CHANGES.md` -- the **STATUS: NOT BUILT** block is replaced by the
   measured one: exe timestamp, the section 7 table, the one red with its cause,
   the suite, and the transition finding. Spliced in binary; CR count 19020
   before and after, asserted by the patch script.
2. `MISTAKES.md` -- two entries added at the top, both found by this build: the
   one-sided control that failed and proved nothing, and the shared noise bar
   that a noisy desktop can defeat. LF-only before and after.
3. This report -- appended, nothing above rewritten.
4. `nifskope-ww-render-shot` -- see B.7.

Uncommitted files in the tree at the end of this lane: 16 modified, 8
untracked. Nothing was committed.

### B.7 Skill review

**Loaded and used, all four the brief named.** `nifskope-ww-resume-pending` --
the read order, qmake-before-make, the exe-newer sweep over EVERY changed file
(which is what caught that nine files and not one had to be newer), the
sequential harness chain with its summary echo, and section 6, "when a gate
fails, measure the cause and STOP", which is the whole shape of B.3 and B.5.
`nifskope-ww-build-verify` -- make's own exit code as the gate, the stylesheet
`cmp`, and "a successful build is not a consistent one", the object-vs-header
sweep. `nifskope-ww-render-shot` -- the switch table and the `upp` arithmetic
that B.5's census column is checked against, and its instruction to verify the
pin by reading `upp` rather than by comparing two files, which is exactly the
check that would have caught the old defect. `ww-texel-picture` -- section 5,
"open the picture before reporting it", which is the only reason B.5 says what
it says instead of "the cards are 600 px out of place".

**The amendment `nifskope-ww-render-shot` still needs.** Its camera section now
opens with *"The pin is CODE ON DISK, not yet compiled as of the 22:04:35 exe of
2026-09-09"*. That is stale as of 00:13:19 and it is the sentence a lane reads
first. It should say the pin is BUILT, name the exe, and carry section 7's
numbers as the known-good values. Lane HOOKCAM wrote that paragraph and owns the
file; this lane leaves it rather than editing another lane's document mid-flight,
and lists it here as owed. **It must land in BOTH trees**, byte-identical.

**A skill this lane wished existed, and the case for writing it.** There is no
skill for "resume a pre-registered gate whose numbers are already written down".
Three separate times this round the right move was to compare a measured number
against a number someone else had committed to in advance, and decide whether a
miss is the code, the instrument or the environment -- and each time the
procedure was re-derived: find the discriminator that separates the three, run
it, and only then write a verdict. The discriminators used here (a fixed-bar
sibling check that passed beside a derived-bar check that failed; a control that
beat the thing it floors; a census value that matched the arithmetic to four
figures while the picture was wrong) are a generalisable set. It is a real
candidate, and it is DECLINED for now for one reason: it is one step away from
`nifskope-ww-resume-pending` section 6, which already says "measure the cause and
STOP", and a second document that says the same thing with more words is the one
that drifts. What this round adds is a worked example, not a procedure, so B.3
and B.5 are the text to lift if it recurs.

**Declined, with the reason.** A skill for "run the transition render". The
script is one lane's, it is run once, and the thing that will recur is not the
running of it but the geometric feasibility check in B.5 -- which belongs in the
script as its first refusal, not in a skill.

### B.8 Owed to bungo

* his open NifSkope, if any, is on the old exe -- **it needs a restart** to get
  the 00:13:19 build (nothing was open during this lane; `rc=1` throughout);
* the one-texel transition picture he asked for is **not delivered**: three
  pictures exist and all three show a thicket, for the measured reason in B.5.
  What is owed is a single-ref scene to shoot it in;
* the `nifskope-ww-render-shot` amendment in B.7, in both trees;
* the section 6 bar in `render_shot.sh`, which will go red again on any noisy
  desktop until the visible control gets a fixed bar.
"""

open(P, "wb").write(b + new)
b2 = open(P, "rb").read()
print("CR %d -> %d, bytes %d -> %d" % (cr0, b2.count(b"\r"), n0, len(b2)))
assert b2.count(b"\r") == 0
assert b2.startswith(b)
print("append-only ok")
