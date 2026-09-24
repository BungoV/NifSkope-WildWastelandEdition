# Lane HKX5b — glTF animation IMPORT + the FO4 .hkx WRITER (2026-09-10)

Repo `E:\Projects\NifskopeWildWastelandEdition`, `main`, **nothing committed**
(the brief: "commit NOTHING"). `MISTAKES.md` is the one tracked file modified,
by an append that was byte-verified as append-only.

**Relaunch of lane HKX5, which died on an API rate limit before writing
anything.** Confirmed on the first turn: no `scratchpad/hkx5_20260910/`, no
`src/gltfimport.*`, no `src/hkxwrite.*`. Nothing was resumed; this is a fresh
lane.

bungo's ruling, 2026-09-10 (HANDOFF ~06:5x), verbatim: *"gltf sounds good"*.

**Deliverables on disk**

| file | sha256 (16) | bytes | LF |
|---|---|---|---|
| `src/gltfimport.h` | `0c0da7182b3e965e` | 7,289 | 0 CR |
| `src/gltfimport.cpp` | `a2cec5a5f738ef57` | 43,805 | 0 CR |
| `src/hkxwrite.h` | `fd1a2cbb51dd1b05` | 4,334 | 0 CR |
| `src/hkxwrite.cpp` | `13f0bf2eca5453d5` | 31,802 | 0 CR |
| `tests/hkxwrite_dump.cpp` | `38b312a2e6cf6a9e` | 7,774 | 0 CR |
| `tests/spells/hkxwrite_gates.py` | `4ca1dc6bd7d8dac2` | 10,199 | 0 CR |
| `docs/GLTF_IMPORT.md` | `27c43680183e1d34` | 11,889 | 0 CR |
| `docs/HKX_WRITE_FORMAT.md` | `9114ee555854146c` | 13,525 | 0 CR |
| `release/hkxwrite_dump.exe` | built 13:35, rebuilt after the matcher fix | 787,792 | — |

Scratchpad: `scratchpad/hkx5_20260910/` — `build_dump.sh`, `dump_packfile.py`,
`interleaved_order.py`, `interleaved_decode.py`, `tsvcmp.py`, `make_3bone.py`,
`mutate.py`, `make_flight_files.py`, `hookup.py`, `WW_CHANGES_ENTRY.md`,
`SKILL_AMENDMENT_ww_hkx_animation.md`, `out/`, `flight/`.

---

## 1. The importer, `src/gltfimport.{h,cpp}`

Contract `docs/GLTF_IMPORT.md`. QtCore only (`QJsonDocument` + `niftypes.h`),
so the gate binary links it without NifSkope.

Reads `.gltf` + external `.bin`, `.gltf` with an embedded `data:` base64
buffer, or `.glb` (chunk walk, JSON + BIN). Accessors: every glTF component
type, `normalized` dequantised per the spec, `byteStride` honoured, sparse
refused by name. Nodes: TRS or `matrix` (decomposed, and **refused** if it
mirrors or shears); hierarchy checked for out-of-range children, two parents
and cycles.

**Axis and units.** The exporter (`src/gltfexport.cpp`, lane HKX4) does not
rotate bone data — it writes NIF-space TRS and hangs everything under one
synthetic root `NifSkope_Y_up` rotated −90° about X, with translations
× `0.9144/64`. So the import has two arms and names the one that served:
**consume** that root (by name, or by shape to 1e-5) and promote its children,
or, for a glTF that has none (Blender's own export), **compose +90° about X**
into each scene root's local transform and sampled channels. The two are the
same arithmetic — `Ru⁻¹ · Ru · Tc = Tc` — which is why one importer serves
both. `convertUpAxis=false` is the way back.

**Bone mapping.** Three arms — exact, case-insensitive, partial-and-unique —
**each run to completion over every node before the next begins**, with an
animated node outranking a still one. On the real 95-bone skeleton that gives
74 exact + 4 case-folded (`SPINE1→Spine1`, `SPINE2→Spine2`, `HEAD→Head`,
`WEAPON→Weapon`, exactly lane HKX1's four) + 0 partial, and names the 17
unmatched `Weapon*` bones and all 51 unmatched nodes. Ambiguity is not a match.

**Resampling.** Default 30 fps; `round(span·fps)+1` frames, minimum 2;
`preserveSourceRate` keeps the source grid but only when every used sampler
shares one, and says so in words when it does not. LINEAR (slerp on rotations),
STEP, CUBICSPLINE (the spec's Hermite, evaluated exactly — not pre-lerped).

**Root motion behind a flag**, default off. On: the root node's travel becomes
`HkxAnimClip::rootMotion` and its track is flattened to the reference frame —
the exact inverse of the exporter's `applyRootMotion` arm.

## 2. The writer, `src/hkxwrite.{h,cpp}`

Contract `docs/HKX_WRITE_FORMAT.md`. Emits
`hkaInterleavedUncompressedAnimation` + `hkaAnimationBinding` +
`hkaDefaultAnimatedReferenceFrame` + the empty `hkMemoryResourceContainer`
shipped clips carry, under `hkaAnimationContainer` and `hkRootLevelContainer`.
The binding is written **explicitly**, even when it is the identity — lane
FIXTURE measured that an empty one is the whole reason FO4 tooling refuses the
Mixamo clip.

**Both routes were built, not just route A.** `RouteXmlPack` writes HKXPACK XML
and shells out to `hkxpack-cli.jar pack`; `RouteDirect` (the default) emits the
Havok 2014 packfile itself and needs no Java. The exact command route A runs:

```
java -jar "E:/Tools/Fallout 4/HKXPACK/hkxpack-cli.jar" pack <in.xml> -o <out.hkx>
```

For `jog` the two produce files of **identical length** (109,376 B) that decode
to within **4.7e-10 units / 1.10e-7 degrees** — 902 of 26,220 floats differ by
about one ULP, the cost of route A's decimal text. Route B is bit-exact.

**The element order was measured, not assumed.** Lane HKXCLASS's proof clip has
ONE track, on which frame-major and track-major are the same bytes. The engine's
own `hkaInterleavedUncompressedAnimation::transformTrack` (**rva `0x01fa1ac0`**)
computes `data + 48*(frame*numberOfTransformTracks + track)` and derives the
frame count with `idiv transforms.size / numberOfTransformTracks` — quoted
instruction by instruction in `docs/HKX_WRITE_FORMAT.md` §3.1. Had this been
guessed wrong, every gate would still have passed, because the decoder was
written from the same page.

## 3. THE ROUND-TRIP TABLE

Bars, pre-registered: **1e-4 units** and **0.01 degrees**, per bone per frame.
Angles are `4·asin(|q1∓q2|/2)` — the true rotation angle (see §6, mistake 1).

| gate | what | rows | max abs ΔT (units) | max angle (deg) |
|---|---|---|---|---|
| (a) | RT1 `jog` direct | 2,185 | **0.00e+00** | **0.00e+00** |
| (a) | RT1 `turn` direct | 2,945 | 1.00e-07 | 0.00e+00 |
| (a) | RT1 `twoblock` direct (2 blocks, 311 fr) | 29,545 | 0.00e+00 | 1.15e-07 |
| (a) | RT1 `q48` direct (THREECOMP48) | 29,516 | 1.00e-07 | 0.00e+00 |
| (a) | RT1 `tpose_idle` direct | 188 | 0.00e+00 | 0.00e+00 |
| (a) | RT1 `jog` via HKXPACK | 2,185 | 4.70e-10 | 1.10e-07 |
| (a) | RT1 `turn` via HKXPACK | 2,945 | 1.00e-07 | 1.19e-07 |
| (a) | RT1 `twoblock` via HKXPACK | 29,545 | 1.50e-08 | 1.27e-07 |
| (a) | RT1 `q48` via HKXPACK | 29,516 | 1.00e-07 | **1.62e-07** |
| (a) | RT1 `tpose_idle` via HKXPACK | 188 | 7.50e-09 | 7.62e-08 |
| (b) | RT2 `jog` clip→glTF→import→write | 1,794 | **8.00e-06** | **3.40e-05** |
| (b) | RT2 + root motion extracted | 1,794 | 8.00e-06 | 3.40e-05 |
| (f) | RT1 Mixamo fixture | 8,835 | **0.00e+00** | **0.00e+00** |

**Round trip 1 worst over 64,379 bone-frames and both routes: 1.00e-07 units,
1.62e-07 degrees** — 1,000× and 60,000× inside the bars.
**Round trip 2 worst: 8.00e-06 units, 3.40e-05 degrees**, the cost of the
metres round trip in float32. Its 391 dropped rows are exactly the 17
`Weapon*` bones × 23 frames that lane HKX4's exporter cannot carry (they have
no NiNode on `skeleton.nif`). **Root motion** survives RT2 to **1.50e-05 units,
0.0 degrees of yaw**.

**Resampling, measured separately:** the 60 fps Mixamo clip resampled to 30 fps
gives 47 frames, every one **bit-identical** (0.0 / 0.0) to the corresponding
60 fps frame — the coincident grid points are exact, not merely close.

## 4. The other gates

**(c) the hand-written 3-bone glTF.** `make_3bone.py` writes the file by hand
from the spec + the export convention, and computes the expectation in closed
form (a slerp between rotations about one axis is a linear interpolation of the
ANGLE, so it is not a second copy of the importer's slerp). One channel of each
interpolation in one file: LINEAR rotation, STEP translation, CUBICSPLINE
scale. **31 frames × 3 tracks: worst |ΔT| 0.0, worst angle 4.38e-06 deg, worst
|Δscale| 3.93e-08.**

**(d) corruptions, 25 of them, all refused by name.** 13 of the glTF, judged by
our own importer; 12 of the written `.hkx`, judged by the independent decoder
(lane HKX1's C++ reader refuses every interleaved file by class name and so
cannot tell a good one from a broken one). Every corruption of the `.hkx` is
STRUCTURAL, because a Havok packfile has no checksum. Sample refusals:

* `sampler 0 has interpolation 'BEZIER'; glTF 2.0 defines LINEAR, STEP and CUBICSPLINE`
* `accessor 2 needs 119988 bytes of bufferView 2, which is 36 bytes`
* `node 1 'Root' has a sheared matrix (axes 0 and 1 dot to 0.287348)`
* `hkaAnimation::type is 3; HK_INTERLEAVED_ANIMATION is 1`
* `transforms has 1 elements, not a whole multiple of the 3 transform tracks`
* `the file carries no hkaInterleavedUncompressedAnimation (objects: ...)`

**The floor.** One payload float nudged by 1.0 unit: the decoder **accepts** the
file (no checksum) and the comparator goes **red at max |ΔT| 1.000e+00** — the
proof that the round-trip comparison can fail at all.

**(e) HKXPACK re-reads our own directly-emitted file.** `unpack` of
`a_jog_b.hkx` succeeds and the XML carries
`class="hkaInterleavedUncompressedAnimation" signature="0xa5eff3f2"`,
`transforms numelements="2185"` (95 × 23), `numberOfTransformTracks 95`,
`transformTrackToBoneIndices numelements="95"`, 6 objects; the 48-byte stride
is confirmed against the payload length.

**(f) the Mixamo fixture.** Read through HKX2b's identity-binding rule and
written: 8,835 rows exact, **60 fps PRESERVED** — round trip 1 does not resample
at all, because resampling is an import concern and the writer emits whatever
frame duration the clip carries; the 30 fps figure only appears when the clip
comes in through glTF. The empty `transformTrackToBoneIndices` came out as an
**explicit 95-entry identity map**, and the 93 root-motion samples are carried
with every component still 0.000000, matching lane FIXTURE exactly.

**Total: 23/23**, `python tests/spells/hkxwrite_gates.py`.

## 5. Hook-up: NOT applied

`scratchpad/hkx5_20260910/hookup.py` prints the four `NifSkope.pro` lines it
would add (`src/gltfimport.h` after line 187, `src/hkxwrite.h` after 188,
`src/gltfimport.cpp` after 312, `src/hkxwrite.cpp` after 313), verifies each
anchor is unique and absent, and **exits 3 having written nothing**. Three HKX
lanes are queued on that file. Until it is applied, the two sources build only
through `scratchpad/hkx5_20260910/build_dump.sh` → `release/hkxwrite_dump.exe`,
which is what all 23 gates ran on. `qmake` is owed after the lines land.

## 6. Mistakes (all five are in the root `MISTAKES.md`)

1. **The angle metric read half the angle it claimed.** The
   `ww-hkx-animation` skill's `2·asin(|q1∓q2|/2)` is `theta/2`, not `theta`;
   the correct figure is `4·asin(d/2)`. Found by a positive control built for
   another purpose — the flight marker rotates one bone by a known 45° and the
   comparator said 22.5 — then pinned with a known-answer control at
   0.5/5/45/120°. Every angle in this report is the corrected one.
   **OWED TO THE DIRECTOR: `tests/spells/hkxanim_decode.py`,
   `tests/spells/hkxanim_gates.py` and `docs/HKX_ANIMATION_FORMAT.md` §4.6
   carry the same expression, so lane HKX1's published angle figures are halves
   too.** The amendment text is
   `scratchpad/hkx5_20260910/SKILL_AMENDMENT_ww_hkx_animation.md`; this lane did
   not edit another lane's files or the shared skill.
2. **Bone matching let a partial match outrank a later exact one** —
   `CamTargetParent` took the `CamTarget` bone and the real `CamTarget` node got
   nothing (77 of 78 mapped). Found by gate (b). Fixed: each arm runs to
   completion before the next.
3. **A crash in the floor harness was counted as the floor going red.** A
   `FileNotFoundError` and a verdict are both non-zero exits. The harness now
   requires exit 1, the word FAIL, and no traceback.
4. **`docs/HKX_ANIMATION_FORMAT.md` §1 says `NamedVariant` is 0x20 bytes; it is
   0x18.** Measured on the shipped `jog.hkx` and on HKXPACK's output alike (the
   fixup run `0x10→0x40, 0x18→0x60, 0x28→0x80, 0x30→0x90`). Corrected in
   `docs/HKX_WRITE_FORMAT.md` §5; the read contract is lane HKX1's file.
5. **Two skills the brief named were never loaded** —
   `ww-standalone-writer-gate` and `ww-anchored-hookup` describe exactly the
   standalone-link/independent-decoder/mutation procedure and the refusing
   hook-up this lane re-derived. CONSTITUTION 1a process error.

## 7. What is NOT measured

* **Fallout 4 has never loaded one of these files.** 0 of 15,320 shipped `.hkx`
  use this class. Everything above is the exe's reflection, the exe's
  disassembly, HKXPACK, and read-back — never the running game.
* Nothing was linked into `NifSkope.exe`; there is no UI, no menu item, no
  `.pro` entry. `qmake` has not seen these files.
* `docs/GLTF_INTERCHANGE.md` did not exist while this lane ran (lane HKX4b is
  still writing it). The import convention was derived from
  `src/gltfexport.{h,cpp}` directly and then **held against HKX4b's real,
  freshly built exporter** in gate (b) — which is the stronger check — but the
  two pages have not been read side by side. **Reconciliation is owed** the
  moment `docs/GLTF_INTERCHANGE.md` lands.
* The exporter's default drops root motion into a *sentence* in `extras`, not
  numbers, so a default export cannot round-trip it. Export with
  `--root-motion` to carry it.
* Float tracks (4 shipped clips have one) are refused, not written. Additive
  clips are carried by blend hint but nothing checks what an additive
  interleaved clip does in game.

## 8. THE GAME FLIGHT — for bungo

Two files are ready in `scratchpad/hkx5_20260910/flight/`, both 109,376 bytes:

| file | sha256 (16) | what it is |
|---|---|---|
| `JogForward.hkx` | `83f93b0c2041663e` | the shipped JogForward, rewritten in the new format, content **exact** |
| `JogForward_marked.hkx` | `9c61132b7b3ada12` | the same file with the **head yawed 45°** on every frame |

**Do the MARKED one first.** "Nothing changed" is not proof our file was read;
a turned head can only come from our bytes.

1. Make a new MO2 mod folder, e.g.
   `E:\Projects\Fallout 4 Mods\mods\HkxWriteTest\Meshes\Actors\Character\Animations\MT\Neutral\`,
   copy **`JogForward_marked.hkx`** in and rename it to `JogForward.hkx`.
   Enable it in MO2, above everything. (A loose drop into
   `<Fallout 4>\Data\Meshes\Actors\Character\Animations\MT\Neutral\` works too.)
2. Load a save, go third person, and **jog forward** (not walk, not sprint —
   `MT\Neutral\JogForward`).
   * **Head visibly turned to one side while jogging** → the engine loaded and
     sampled a class no shipped file uses. That is the answer.
   * **Head straight, jog looks normal** → our file was NOT read. Before
     concluding anything about the format, check the mod is actually winning:
     rename the file to something misspelt and see whether anything changes at
     all.
   * **T-pose, a frozen or exploded skeleton, or a crash on load** → the file
     was read and rejected, or our layout is wrong. Say which of the three, and
     if it crashed, the crash log.
3. Then swap in **`JogForward.hkx`** (the exact one), same name and place.
   Jog again. It should be **indistinguishable from vanilla** — that is the
   real target, and any twitch, pop or stutter that step 2 did not have is a
   defect in our bytes, not in the class.
4. Delete the mod folder (or untick it) to go back; the vanilla clip is in the
   BA2 and is not touched.

**What to send back:** which of the three outcomes step 2 gave, and whether
step 3 looked like vanilla. One line each is enough.

**One caution before you fly:** the character skeleton is driven by a behavior
graph, and `MT\Neutral\JogForward` is blended with other clips as you turn. Jog
in a straight line for the read.

## 9. Skill review (CONSTITUTION 1a, the finished-work review)

**Loaded:** `ww-hkx-animation` — carried the whole lane. Its section 9 (the
class verdict, the reflection offsets, the HKXPACK signature) meant the writer
never had to re-derive what to emit, and its section 10 (the FIXTURE rules) is
why gate (f) knew what to look for. Its packfile-section-table note (`0x40 +
u16@0x3e`, never the hardcoded 0x40) saved the decoder from the trap the
collision walker still has.

**Should have been loaded and was not** (mistake 5, in `MISTAKES.md`):
`ww-standalone-writer-gate` — the standalone Qt6Core link with the real
`Makefile.Release` flags, the independent decoder, the mutation controls, the
"what this does not prove" list. Re-derived from scratch.
`ww-anchored-hookup` — the refusing hook-up script with exact-once anchors and
the Write-tool-never-a-heredoc trap. Re-derived from scratch. Both were named
in this lane's own brief.

**Skills that should exist, and the text for them:**

1. **Amend `ww-hkx-animation` section 7 with the corrected angle metric**, and
   add the frame-major law and the 0x18 `NamedVariant` stride to section 8.
   Text written and delivered, not applied:
   `scratchpad/hkx5_20260910/SKILL_AMENDMENT_ww_hkx_animation.md`. This is the
   highest-value item here — the wrong formula has already propagated through
   three lanes and two documents.
2. **Amend `ww-control-calibration` with THE METRIC CONTROL.** That skill builds
   controls for the *signal*; nothing in it says to control the *measuring
   stick*. The rule earned today, in one line: *before quoting any number a
   metric produces, feed the metric a difference whose size you already know
   and check it reports that size — inheriting the formula from a skill, a
   contract or another lane does not exempt it.* A four-point known-answer
   sweep (small, medium, large, very large) costs eight lines and would have
   caught a factor of 2 that survived three lanes.
3. **A skill is NOT needed for "mirror a third-party packer's binary layout".**
   Declining, with the reason: the procedure that mattered — dump the reference
   file's fixup tables, mirror the object order and alignment, read the result
   back with both an independent decoder and the third-party tool — is already
   `ww-standalone-writer-gate` plus this lane's `dump_packfile.py`, which is
   kept in the scratchpad and pointed at from `docs/HKX_WRITE_FORMAT.md` §8.
   Writing a second skill would split the procedure in two.

**A note for whoever writes the next animation lane:** the single most valuable
thing this lane did was refuse to assume the transform element order and spend
one disassembly on it instead. A one-element proof file cannot distinguish two
layouts, and every gate downstream would have agreed with the wrong one,
because the oracle and the writer share the contract page. When a fixture is
degenerate in the dimension under test, say so and go to the engine.


## Build (BUILD7)

Built by lane BUILD7, 2026-09-10, on `main`, **nothing committed**.

**Section 5's hook-up is APPLIED.** `scratchpad/build7_20260910/apply_hookup.py`
imports `EDITS` from this lane's own `hookup.py` rather than retyping the four
anchors, asserts each anchor occurs exactly once and each insertion is absent,
and asserts the byte deltas after: `NifSkope.pro` 18,779 -> 18,859 bytes,
LF 717 -> 721, **CR 0 unchanged**. `src/gltfimport.h`, `src/hkxwrite.h`,
`src/gltfimport.cpp` and `src/hkxwrite.cpp` are now in `HEADERS`/`SOURCES`.
Lane HKX4b's `src/gltfexport.*` was left alone and is NOT in the `.pro`.

**`qmake` + `make -j2`, both RC=0.** `GeneratedFiles/.obj/gltfimport.o`
(216,611 B, 14:02:11) and `hkxwrite.o` (106,984 B, 14:02:14) are built and
linked into `release/NifSkope.exe` (14:04:34, 19,197,952 B). Both objects name
`src/hkxanim.h` in the regenerated dependency lists. So the "no `.pro` entry,
`qmake` has not seen these files" line of section 7 is discharged; everything
else in section 7 stands.

**This lane's 23 gates were NOT re-run.** They run on
`release/hkxwrite_dump.exe` (13:41:42), which the application link does not
touch: the same two translation units, the same flags, a different final
binary. Re-running them would have measured `hkxwrite_dump.exe` a second time,
not the exe. What IS newly true is only that the two files compile and link
inside the application.

**Still nothing user-facing.** No menu item, no dialogue, no UI route reaches
`gltfImportAnimation` or `hkxWriteAnimation` -- linking them in makes them
callable, not reachable. And **Fallout 4 has still never loaded one of these
files**; the two flight files in `scratchpad/hkx5_20260910/flight/` are
unchanged and unflown.

Lane HKX1's gates, rebuilt and re-run in the same build because
`src/hkxanim.cpp` is shared: 134 checks / 3 pre-registered fixture failures,
29 / 0, 20 / 0, and the identity rule 8,835 rows with 0 `bone != track` plus
6/6 on the floor. Lane HKX2's in-app gates: 27 checks, 0 failures, twice.

### Mtimes, in one table

| artefact | mtime | bytes |
|---|---|---|
| `release/NifSkope.exe` | 2026-09-10 14:04:34 | 19,197,952 |
| `release/style.qss` | 2026-09-10 14:04:35 | 11,097 (compares equal to `res/style.qss`) |
| `release/hkxanim_dump.exe` | 2026-09-10 14:05:13 | 526,009 |
| `release/hkxwrite_dump.exe` | 2026-09-10 13:41:42 | 795,378 (lane HKX5's, not rebuilt) |
| `fixtures/human_male_vanilla.nif` | 2026-09-10 05:29:52 | 338,563 |
| `scratchpad/build7_20260910/frames_jog.png` | 2026-09-10 14:14:09 | 602,781 |
| `scratchpad/build7_20260910/frames_mixamo.png` | 2026-09-10 14:14:09 | 626,212 |

### Owed, still, from this lane's own list

* The angle-metric correction (`4*asin`, not `2*asin`) is in the
  `ww-hkx-animation` skill already; **`tests/spells/hkxanim_decode.py`,
  `tests/spells/hkxanim_gates.py` and `docs/HKX_ANIMATION_FORMAT.md` section 4.6
  still carry `2*asin`**, so lane HKX1's published angle figures are still
  halves. BUILD7 did not touch them: they are another lane's files and changing
  the metric would have moved the gate numbers this build had to reproduce.
* `docs/GLTF_IMPORT.md` and lane HKX4b's `docs/GLTF_INTERCHANGE.md` have still
  not been read side by side.


## Build (BUILD8)

Built by lane BUILD8, 2026-09-10, on `main`, **nothing committed**.
bungo's order, verbatim: *"now export and import of gltf"*.

### The exe, and the mtimes in ONE table

| artefact | mtime | bytes |
|---|---|---|
| `release/NifSkope.exe` | 2026-09-10 **14:37:53** | 19,382,272 |
| `release/style.qss` | 14:37:53 | 11,097 (compares equal to `res/style.qss`) |
| `release/hkxanim_dump.exe` | 14:51:01 | 532,675 |
| `release/hkxwrite_dump.exe` | 14:46:32 | 802,556 |
| `release/gltfexport_dump.exe` | 14:46:46 | 802,959 |
| `src/hkxanim.cpp` (BUILD8's interleaved read arm) | 14:42 | 43,891 |
| `src/nifcli.cpp` (the three CLI commands) | 14:36 | 275,399 |
| `scratchpad/build8_20260910/frames_mixamo_original.png` | 14:5x | 1,370x1,420 |
| `scratchpad/build8_20260910/frames_mixamo_roundtrip.png` | 14:5x | 1,370x1,420 |

`qmake` + `make -j2`, both RC=0. The exe is newer than all **72** changed files
under `src/ res/ tools/ tests/ NifSkope.pro`. `Makefile.Release`, read back per
object by name, gives `gltfexport.h` to `gltfexport.o`, `gltfexportnif.o`,
`gltfanim.o` and `nifcli.o`; `gltfimport.h` and `hkxwrite.h` to their own
objects and to `nifcli.o`.

**The first link FAILED** because bungo opened `release/NifSkope.exe` at
14:26:18, mid-lane. The running copy was renamed aside (never killed) as
`release/NifSkope_inuse_44632.exe` and the link re-run. It is in `MISTAKES.md`:
a process check that only echoes its answer is not a gate.

### The CLI, and what had to be written to have one

`scratchpad/hkx4_20260910/hookup.py --apply` -- 11 of 11 anchors, exactly once
each, CR 0 unchanged on all three files. Then TWO hook-ups this lane wrote,
because **lane HKX5b's importer and writer had no route at all**: they were in
`NifSkope.pro` and linked, and nothing called them.

```
gltf        <file.nif>  -o OUT.gltf [--clip C.hkx [--bones S.hkx]] [--root-motion]
gltf-import <in.gltf>   -o OUT.hkx  [--bones S.hkx] [--fps N] [--source-rate]
                                    [--root-motion --root-node Root] [--route a|b] [--tsv T]
hkx-tsv     <in.hkx>    -o OUT.tsv
```

All three are in `NifSkope -no-gui --help`. Scripts:
`scratchpad/build8_20260910/hookup_import.py`, `hookup_rootnode.py`.

### THE ROUND TRIP -- every leg through `release/NifSkope.exe`

Bars pre-registered by lane HKX5b: **1e-4 units, 0.01 degrees**, per bone per
frame, the angle being the corrected `4*asin(|q1-+q2|/2)`. The original is
decoded by `tests/spells/hkxanim_decode.py` and the written file by
`scratchpad/hkx5_20260910/interleaved_decode.py` -- two independent Python
decoders, neither of them our C++.

| leg | rows | max abs dT (units) | max angle (deg) |
|---|---|---|---|
| `JogForward`: the IMPORTED clip vs the original decode | 1,794 | **8.0e-06** | **3.41e-05** |
| `JogForward`: written, decoded again | 1,794 | 8.0e-06 | 3.41e-05 |
| `JogForward` `--root-motion --root-node Root` | 1,794 | 8.0e-06 | 3.41e-05 (root motion 1.5e-05 / 0.0 deg over 165.354 units) |
| Mixamo 60 fps `--source-rate`: the imported clip | 7,254 | **4.4e-05** | **5.21e-06** |
| Mixamo: written, decoded again | 7,254 | 4.4e-05 | 5.21e-06 |
| the WRITER alone (imported clip vs its own decode) | 1,794 / 7,254 | 0.0 | 0.0 |
| the exe's own reader vs the independent decoder, on the written file | 1,794 / 7,254 | 0.0 | 0.0 |
| the exe's own reader vs the oracle, on the ORIGINAL spline clip | 2,185 / 8,835 | 3.6e-06 / 1.5e-05 | 4.70e-06 / 4.83e-06 |

Scale exact on every row. The 391 / 1,581 rows that do not survive are the 17
`Weapon*` tracks with no node in the body NIF, each named by the exporter.
Mesh and skin through the exporter equal the NIF's: 9 shapes, 6,443 vertices,
11,375 triangles, every index identical.

### THE PICTURE

The five frames of lane BUILD7's sheet plus the bind pose, rendered twice from
ONE pinned orthographic camera (`WW_RENDER_VIEW=5 CENTER=0,0,62 ORTHO=80`,
`upp=0.150235`, read back from `release/ww_camera_pin.log` at every grab) --
once from the original clip, once from the clip that went `hkx -> glTF -> hkx`.

| tile | Mixamo: differing pixels of 1,352,217 | worst step | jog |
|---|---|---|---|
| bind (no clip on either side) | **0** | 0 | **0** |
| frame 0 | 11 | 1 | 0 |
| 1/4 | 1 | 1 | 12 (step 1) |
| 1/2 | **0** | 0 | 33 (step 4) |
| 3/4 | 8 | 1 | 170 (step 16) |
| last | 22 | 1 | 52 (step 2) |

**The noise floor is ZERO**: the same clip rendered twice is byte-identical on
all six tiles, so every count above is the round trip's float error surfacing as
sub-pixel antialiasing on a silhouette. Worst case anywhere: 170 pixels of
1,352,217 -- 0.013% -- at 16 of 255 levels.

Sheets: `scratchpad/build8_20260910/frames_{mixamo,jog}_{original,roundtrip}.png`;
amplified difference pictures `images/diff_*.png`.

### Blender 4.5, the third-party leg

`--background --factory-startup`, our `.gltf` in, Blender's own `.gltf` out,
ours in again (`scratchpad/build8_20260910/blender_roundtrip.py`).

* Blender KEEPS our node names, including the synthetic `NifSkope_Y_up`, so our
  importer's first arm serves its file too.
* armature `COM`, 110 bones, **107 of them with a non-zero roll** -- a Blender
  concept glTF has no field for.
* scene unit METRIC, scale 1.0, metres. Nothing changed there.
* **the frame rate is what it changes.** At the factory 24 fps the clip comes
  back re-timed: 18 keys on a 1/24 grid, our importer resamples to 22 frames,
  and the error is **0.29 units / 4.83 degrees**. With the scene rate set to the
  clip's own 30 fps: 23 frames and **1.006e-04 units / 4.68e-04 degrees** --
  21x inside the angle bar and **0.6% OVER** the translation bar. Both are
  Blender's resampling, not ours.

### Gates

| gate | result |
|---|---|
| `bash tests/spells/gltf_gates.sh` | **0 gate(s) not as registered** (G2 0/0/0, G2f 12/12 refused, G3 2/2/0 pre-registered, G3f 7/7 red, G4 Blender 9/9, 9/9, 1/1) |
| `python tests/spells/hkxwrite_gates.py` | **23/23** |
| `python tests/spells/hkxanim_gates.py` | **134 checks / 3 failures**, all three pre-registered (the 17 `Weapon*` bones, the 0.00148 `weapon` pose translation, the furniture T-pose clip) -- on `hkxanim_dump.exe` REBUILT at 14:51:01 |
| `python tests/spells/hkxanim_mutate.py` | 20 / 0 |
| `python tests/spells/hkxanim_synthetic.py` | 29 / 0 |
| `bash tests/spells/hkxanim_play.sh` | 27 / 0 |
| `bash tests/spells/render_shot.sh` | 82 / 0 |
| FLOOR, the new interleaved read arm | one payload float moved by 1.0: the exe's reader reports **1.000e+00 at (frame 1, track 1) and 0 everywhere else** |
| FLOOR, the picture | the same clip rendered twice: 0 differing pixels on all six tiles |

### THE CROSS-LANE CHANGE, named

`src/hkxanim.cpp` is lane HKX2b's file and this lane changed it: lane HKX1's
reader dispatches on the class name and refused every
`hkaInterleavedUncompressedAnimation` -- the only class `src/hkxwrite.cpp` can
write -- so **NifSkope could not open the file its own glTF import had just
written**, and the picture proof (which goes through the render hook, which goes
through this reader) was impossible. The arm reads the `hkArray<hkQsTransform>`
at +0x38, 48 bytes an element, frame-major, with the frame count derived as
`transforms.size / numberOfTransformTracks` (`docs/HKX_WRITE_FORMAT.md` 3.1,
from the engine's own `transformTrack`, rva `0x01fa1ac0`). Additive: every other
class is refused with the identical sentence and the spline path is unchanged.
The pre-change file is kept at
`scratchpad/build8_20260910/hkxanim.cpp.pre-build8`. It is in `MISTAKES.md`.

### The two contract pages, read side by side at last

Lane HKX5b's owed item is discharged. Every source both pages cite was re-hashed
first and every one is unchanged (`src/gltfexport.h` `77e2b2f7e2010ca5`,
`src/gltfexport.cpp` `cbd5ce5e0e569a34`, `src/gltfimport.h` `0c0da7182b3e965e`,
`src/gltfimport.cpp` `a2cec5a5f738ef57`). They AGREE on the unit, the up-axis
node and its quaternion, the quaternion component order, dimensionless scale,
LINEAR being exact for FO4's degree-1 splines, and the root-motion formula.
They DISAGREED on two things, now written into both:

1. **which node carries the root motion** -- the export composes it onto the
   ROOT BONE's node (`Root`), the import defaulted to the single scene root
   (`skeleton.nif`), which drives no bone. The import refused by name until
   `--root-node` existed;
2. **the frame rate** -- the export writes the clip's own, the import defaults
   to 30 for everything, so a default round trip of the 60 fps fixture returns
   47 frames instead of 93.

### What is NOT measured

* **Fallout 4 has still never loaded a file written by `src/hkxwrite.cpp`.** The
  two flight files in `scratchpad/hkx5_20260910/flight/` are unchanged and
  unflown.
* The menu entry `File > Export > .glTF (skeleton, skin, animation)` was not
  exercised by hand (PENDING.md's P6): the exe was kept free of a GUI session.
  **There is no menu route to the IMPORT at all** -- CLI only.
* `tests/spells/hkxanim_decode.py`, `tests/spells/hkxanim_gates.py` and
  `docs/HKX_ANIMATION_FORMAT.md` section 4.6 still carry the `2*asin` angle
  metric, so lane HKX1's published angle figures are still halves. Untouched
  again, for the same reason BUILD7 gave: changing the metric moves the gate
  numbers this build had to reproduce.
* Nothing is committed.
