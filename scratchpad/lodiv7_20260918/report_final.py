p = 'scratchpad/lodiv7_20260918/lane_lodiv7_report.md'
s = open(p, encoding='utf-8', newline='').read()

# ---------------------------------------------------------------- section 8
A = "`tests/spells/lodi_v7.sh` exists, `bash -n` clean, five gates G1..G5. **It has never run to completion, and"
B = "### 8a. The 18:0x run, and whether it counts as G4/G5. It does NOT."
i, j = s.index(A), s.index(B)

S8 = """`tests/spells/lodi_v7.sh`, five gates G1..G5, **ran to completion twice**: 18:33:28..18:34 (G1..G4) and
18:37..18:56:27 (G1..G4 with `NEIGHBOURS=1`). Both ended `lodi_v7: 12 ok, 0 failed, 0 skipped`. The logs are
`scratchpad/lodiv7_20260918/gate_g1g4.log` and `gate_g5.log`.

| gate | what it asserts | result |
|---|---|---|
| **G1a** the way back | `--lodi-v6` from the NEW exe writes the same bytes the RUNG exe wrote | **ok.** `.lodi` 169,692 B sha1 `8bed3a953a43...`, `.lodo` 6,204,388 B sha1 `fa993ce1d576...`, from both exes. The v7 arm writes `4eb2fc55d5f4...`, 243,420 B |
| **G1b** the rung refuses v7 by name | the pre-lane exe must say what it does not know | **ok**, in the gate's own log now: `native REFUSED Commonwealth.lodi: version 7; this reader knows 3, 4, 5 and 6` |
| **G1c** the two readers agree | C++ and Python decode the same file to the same numbers | **ok.** C++ `groups=588 vertexSkyBytesTotal=53396`, Python the same; the Python reader accepts the pair on 6 checks, 0 failures |
| **G2** the grouping | four pre-registered refuters, each red once, plus the closure | **ok.** 12 refuters green, 7 controls red, 0 failures. The closure: 1,877 architecture boxes rebuilt from the shipped bytes, 41,197 pairs examined, 5,705 touching within 16 u inside one chunk, **0 of those in different groups**; with grouping disabled the same closure cuts all 5,705 |
| **G3** the sky stream | median, correlation, flat-slice subset, per-building variation | **ok.** median 0.38 (p90 13.88, p99 53.56); pearson r 0.9854 against the AO stream's 0.9878 on the same file; 90.72% within 2 on the 194 placements whose slice spans 8 or less; 95.15% of buildings vary across the placement. Controls: shuffled 0.0191, constant 0.0000 |
| **G4** the viewer | `identity` draws the group, `placement` the placement, `sky` names which stream served | **ok**, six checks. `identity` -> "the GROUP (.lodi v7 0x100) on 2446 placements, 588 groups in the file"; `placement` -> the placement identity, 2,446 read, min 0 max 2448 mean 1224.894; `sky` -> "the PER-VERTEX SKY STREAM (.lodi v7 0x110) ... 53349 bytes over 2446 slices"; on a v6 file `sky` names the placement byte; and the stream/byte and identity/placement pairs are each proved to be DIFFERENT pictures |
| **G5** the neighbours | six owner harnesses, standing counts theirs | **three PASS, three FAILED and the failures are the interesting part** -- next table |

### The G5 neighbours, and what their three failures turned out to be

| harness | standing | this run | verdict |
|---|---|---|---|
| `native_open.sh` | 17/0/2 | **PASS** | unaffected |
| `lodl_open.sh` | 23/0 | **PASS** | unaffected |
| `lodgen_slab.sh` | 16/0 | **PASS** | unaffected |
| `lodl_channels.sh` | 48/0 | **11 failures**, then **54 checks 0 failures** after a rebuild | **not a source defect: the shipped exe was half a build old.** s8b |
| `lodgen_native.sh` | RESULT PASS | **2 failures**, both test-side, both now repaired, **not yet re-run** | the version bump owed its neighbours two edits. `j0` accepted a `.lodi` at 3, 4, 5 or 6 and had never heard of 7 (`tests/spells/lodgen_native_fields.py`). `(lodi-wrap)` doctored a v7 file and re-signed only the first 256 bytes of what is now a **512-byte header block**, so the file came back refused for a CRC mismatch instead of for the bounds violation the case exists to provoke -- **a control going red for the wrong reason**, which is worse than one that does not go red. `resign_header_lodi` in `tests/spells/lodgen_native_mutate.py` now reads the version and signs `0x10..0x200` on a v7 file |
| `render_shot.sh` | 82/0 | **2 failures**, cause NOT established | both are luminance-range checks -- "the pixel sampler CAN see a window's pixels", 0.250 against a bar of 15, and the black/white matte, 5.000 against 30. It ran at 18:53 on the **pre-rebuild** exe, whose terrain channel path was the one s8b describes. **I have not re-run it and I am not going to guess** |

**What is owed, stated exactly.** G1, G2 and G3 were proved on the exe built at 10:38; s8b shows that exe
linked one stale object, in the TERRAIN VIEWER and nowhere near the `.lodi` writer or either decoder, so I
expect those three to be untouched -- **and expecting is not measuring**. The rebuilt exe has had `G4`
re-proved on it by the seven pictures (s9, every note line quoted) and `lodl_channels.sh` re-run green, but
**the full gate has NOT been re-run on it**: the attempt at 19:11:54 came back
`SKIP: Fallout4 is up -- no exe runs while the game holds the files`, which is the harness's own rule, not
mine. So `NEIGHBOURS=1 bash tests/spells/lodi_v7.sh` on the rebuilt exe, with `lodgen_native.sh` and
`render_shot.sh` inside it, is the one thing this lane hands over unfinished. **Game up during run.**

"""
s = s[:i] + S8 + s[j:]

# ------------------------------------------------------- new 8b, before s9
C = "**No build is owed.** `find src -newer release/NifSkope.exe` is empty"
D = "## 9. The viewer, as written but not yet photographed"
i2, j2 = s.index(C), s.index(D)

S8B = """### 8b. A green gate on a binary that was half a build old

`lodl_channels.sh` came back with **11 failures against a standing 48/0**, and every one of them was a
channel reading one channel LATE: `mask-r` drew the mask sheet's **G**, `mask-g` drew its **B**, `mask-b`
drew the alpha and reported ABSENT, `emissive` drew the **role-2** model-space-normal sheet, `normal` drew
nothing at all. A clean +1 shift, exactly one enumerator wide -- and this lane had just inserted
`Placement` into the MIDDLE of `enum class LodlChannel`.

**The story that fits and is wrong** is that the insert broke the channel table. It did not: the table is
`{ name, LodlChannel }` pairs looked up by STRING, and `btdterrain.cpp` switches on the enumerators by name.
Source cannot produce this shift. **What does**, measured in two commands rather than reasoned about:

| | |
|---|---|
| the pre-lane exe | `EXE=release/NifSkope.before_lodiv7.exe bash tests/spells/lodl_channels.sh` passes every one of those checks. The shift is in the BINARY, not in the fixture and not in the harness |
| the object file | `GeneratedFiles/.obj/btdterrain.o` **08:40**, `src/lodinative.h` **10:16**. The shipped exe linked a translation unit compiled against the OLD enum, so old ordinals met new ones and every channel from `mask-r` on was read one late |
| the repair | `touch src/btdterrain.cpp` + `mingw32-make -f Makefile.Release -j8`, 19:04:55..19:05:13, with the game down and the check run as its own command first. The old exe is kept as `release/NifSkope.before_btdterrain_rebuild.exe` (sha1 `d7261c9a7b3f...`); the new one is `28ac412c6da9...` |
| the proof it was that | `lodl_channels.sh` on the rebuilt exe: **54 checks, 0 failures** (its standing 48 plus this lane's one channel and three checks). The `ao` picture's third note line reads "terrain AO from the MASK SHEET'S B" again |

**Three rules out of it**, all now in the root `MISTAKES.md`:

1. **Inserting a value into the middle of an enum in a shared header is an ABI change to every translation
   unit that includes it, and the incremental build is not to be trusted with it.** Put the new value at the
   END, or rebuild the includers explicitly. `grep -rl <header> src/*.cpp` and compare each `.o` mtime to the
   header's -- ten seconds, and it is what found this.
2. **A gate that only exercises the files you edited cannot see this.** G1..G4 were green on the wrong
   binary because the lane's OWN translation units were fresh. The neighbour harnesses, with standing counts
   over code the lane never touched, are what caught it. Run them before believing a gate.
3. **`find src -newer <exe>` is not a staleness check.** It passes exactly when every source is older than
   the exe, which is also the state a stale OBJECT produces. Compare objects to headers.

"""
s = s[:i2] + S8B + s[j2:]

# ---------------------------------------------------------------- section 9
E = "**Both files syntax-check and neither has been run**"
F = "## 10. The documentation that landed"
i3, j3 = s.index(E), s.index(F)

S9 = """`lodl_channels.sh` now returns **54 checks, 0 failures** on the rebuilt exe, which is its standing 48 plus
this lane's one channel and three checks, and check **(f)** is among them: on a version-6 fixture `identity`
NAMES its fallback, `sky` NAMES the placement byte, and -- with nothing to fall back FROM -- `identity` and
`placement` are the SAME picture (95,460 B against 95,460 B), where G4 asserts they differ on a v7 file.

### The seven pictures

Taken 19:09:23..19:10:04 by `scratchpad/lodiv7_20260918/pictures.sh`, into
`scratchpad/lodiv7_20260918/images/`, captions in `images/captions.md`. CHANVIEW1 framing throughout
(`WW_RENDER_CENTER=24900,-41300,450`, `WW_RENDER_ORTHO=2600`, `WW_RENDER_VIEW=8`, 1400x1091) -- the camera
`lodl_channels.sh` uses, so a picture here and a picture there are the same view. Every caption's note line
is the VIEWER'S OWN sentence, read back out of that picture's log.

| # | file | the channel, in the run's own words |
|---|---|---|
| 1 | `1_v7_identity.png` 74,512 B | `identity`: the GROUP (.lodi v7 0x100) on 2,446 placements, 588 groups in the file -- whole houses in one flat colour |
| 2 | `2_v7_placement.png` 95,460 B | `placement`: the placement identity, 2,446 read, min 0, max 2448, mean 1224.894 -- what `identity` drew before v7 |
| 3 | `3_v6_identity.png` 95,460 B | `identity` on a v6 file: "no group table in Commonwealth.lodi (a version-6 file), so the PLACEMENT IDENTITY served it on 2446 placements". Byte-for-byte the same size as picture 2, which is the fallback being exactly the old channel |
| 4 | `4_v7_sky.png` 311,486 B | `sky`: the PER-VERTEX SKY STREAM, 53,349 bytes over 2,446 slices, min 0, max 255, mean 119.161 |
| 5 | `5_v6_sky.png` 85,012 B | `sky` on a v6 file: "no per-vertex stream ... so the PLACEMENT BYTE (.lodi 0x11) served it" -- every building one flat tone |
| 6 | `6_v7_ao.png` 351,578 B | `ao`, the control this lane did not touch: ".lodi v6 scene vertex AO used on 2446 placements (53349 bytes, mean 177.0), 0 slices did not match the drawn mesh" |
| 7 | `7_v7_group_largest.png` 34,933 B | the largest group, `identity` narrowed to the one CELL it stands in (`WW_LODI_REGION="5,-11,5,-11"`): "the GROUP ... on 405 placements". 205 of those 405 are the group -- one colour -- and the other 200 belong to 26 other groups and are the other colours |

**Picture 7 is framed, not cropped, and the caption says so.** No shipped knob draws ONE group on its own;
the alternative was a doctored `.lodi`, and a picture of bytes nobody shipped is not evidence. At this ortho
the group runs past the right edge of the frame. 205 + 200 = the 405 the viewer counted, which is the
measurement in `scratchpad/lodiv7_20260918/biggest_group.txt` arriving back through the picture.

"""
s = s[:i3] + S9 + s[j3:]

# --------------------------------------------------------------- section 11
R7 = "| 7 | **The lane is PENDING, not DONE**, on a wedged NifSkope this session has no permission to end. | s8 and `PENDING.md`. |"
N7 = """| 7 | **The shipped exe was half a build old, and only a harness over code this lane never touched caught it.** | s8b. Inserting a value into the middle of an enum renumbered it for every file that includes the header; one object file was not rebuilt, so the terrain viewer read every channel from `mask-r` on one late. G1..G4 were all green on that binary. Rebuilt, re-proved, and three rules written into `MISTAKES.md`. |
| 8 | **One thing is owed and it is an exe run, not a decision:** `NEIGHBOURS=1 bash tests/spells/lodi_v7.sh` on the rebuilt exe, which re-runs G1..G3 and the two neighbours whose failures are already understood or already repaired. | The attempt at 19:11:54 returned `SKIP: Fallout4 is up` -- the harness's rule about the game holding the files. Game up during run; nothing was re-run in a loop to get around it. |"""
assert s.count(R7) == 1
s = s.replace(R7, N7)

# --------------------------------------------------------------- section 12
s = s.replace("## 12. Mistakes, both now in `MISTAKES.md`",
              "## 12. Mistakes, all three now in `MISTAKES.md`")
G = "Two smaller ones, recorded here rather than in the ledger because each was caught inside the lane:"
M3 = """3. **A diagnosis written before the parent chain was read** -- s8a, retracted in full and replaced with
   what was actually measured. The cheap check I skipped was the wedged process's own command line.

4. **A green gate on a binary that was half a build old** -- s8b. The lane's own translation units were
   fresh, so every gate it wrote passed; the defect sat in a file it never edited, compiled before the
   header changed under it.

"""
assert s.count(G) == 1
s = s.replace(G, M3 + G)

# --------------------------------------------------------------- section 14
H = """5. I have no pictures for you and the lane is parked: a test of mine launched NifSkope with no file to open,
   it hung, and I am not allowed to kill it -- close it and everything else is ready to finish."""
N14 = """5. The pictures are in `scratchpad/lodiv7_20260918/images/` -- one of whole houses in single colours, one
   of the old per-placement view beside it, the new sky against the old flat one, and the 205-piece house on
   its own -- and one honest thing left over: the exe I first tested was missing a rebuild of one file, which
   made the terrain colour views read one channel late, so I rebuilt it, re-proved the viewer, and the one
   run still owed is the whole gate on the rebuilt exe, which the harness refused to start while your game
   was up."""
assert s.count(H) == 1
s = s.replace(H, N14)

open(p, 'w', encoding='utf-8', newline='').write(s)
d = open(p, 'rb').read()
print('report updated; CRLF %d; bytes %d' % (d.count(b'\r\n'), len(d)))
