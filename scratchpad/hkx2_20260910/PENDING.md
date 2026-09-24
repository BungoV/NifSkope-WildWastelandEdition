# PENDING — lanes HKX1 + HKX2 (Havok animation: reader, playback, mapping)

Last updated 2026-09-10 by lane **HKX2b**, which resumed HKX2 after it died on a
rate limit at its last step. Nothing here is committed and nothing has been
built.

## State, verified on disk by HKX2b (not taken from the report)

* **Every file the HKX2 report claims exists, exists**, and every C++ file
  passes `g++ -fsyntax-only` with the real `Makefile.Release` flags,
  `ALL-RC=0`, no new warnings:
  `src/hkxplayback.h/.cpp`, `src/hkxplaybacktest.cpp`, `src/hkxanim.h/.cpp`,
  `src/gl/glnode.cpp`, `src/gl/glscene.cpp`, plus
  `tests/spells/hkxanim_play.sh` and `scratchpad/hkx2_20260910/shots.sh`
  (`bash -n` clean).
* **The hook-ups are APPLIED, not pending.** `scratchpad/hkx2_20260910/fixpro.py`
  and `uihooks.py` are the record of edits already in the tree:
  `NifSkope.pro` carries `src/hkxanim.{h,cpp}` + `src/hkxplayback.{h,cpp}` +
  `src/hkxplaybacktest.cpp`; `src/nifskope_ui.cpp` carries the
  `#include "hkxplayback.h"` (:73), `wwHkxAnimHarness( skope )` (:7720),
  the `WW_HKXANIM_CLIP` render hook (:21666) and the Animation-panel rows
  (:26890, :26927). Re-running either script is a no-op at best and a double
  edit at worst — **do not re-run them**.
* `src/nifskope.cpp`'s `+9` lines are **not HKX2's**: they are the water-marking
  dock (`waterMarkInstall`). `src/gltfexport.cpp` is another lane's untracked
  file. The tree holds several uncommitted lanes; touch only the files listed
  here.
* `MISTAKES.md` already carries HKX2's two entries (`:2381`, `:2406`) and the
  `ww-hkx-animation` skill already carries its amendment — HKX2b renumbered it
  to `## 11.` (9 and 10 were taken by lanes HKXCLASS and FIXTURE) and mirrored
  the repo tree to the live tree; the two files are byte-identical, 17,383 bytes,
  CR=0.
* `scratchpad/hkx2_20260910/WW_CHANGES_ENTRY.md` is the ready text for
  `WW_CHANGES.md`. **The director splices it; a lane never edits WW_CHANGES.md**
  (and never with `sed -i`). Its STATUS block still says NOT BUILT and must be
  rewritten with the gate numbers once they exist.

## The identity-map rule, landed by HKX2b (2026-09-10)

`fixtures/Running_To_Slide_And_Back_To_Running.hkx` (the Mixamo clip) was
refused by both decoders — *"binding maps 0 tracks, the animation has 95"* —
because its `hkaAnimationBinding::transformTrackToBoneIndices` is EMPTY. Lane
FIXTURE measured that empty means IDENTITY (75 of 95 tracks match
`skeleton.hkx`'s reference-pose translation at shift 0 and only 18 at any other
shift), and the rule is now in the readers:

| file | change |
|---|---|
| `src/hkxanim.h` | new `HkxAnimClip::trackToBoneIsIdentity` — the fallback NAMES itself (CONSTITUTION 10) |
| `src/hkxanim.cpp` | `validate()` refuses only a NON-EMPTY vector of the wrong length; `decodeClip()` fills the identity map for both arms (no binding, or an empty one) and sets the flag |
| `tests/spells/hkxanim_decode.py` | the same two changes, so the C++ and Python oracles stay a matched pair |
| `src/hkxplayback.cpp` | already carried its half (`identitymap.py`, applied): the consumer's gate is "the skeleton has at least `numTracks` bones", because a clip file usually carries no skeleton at all |

Measured by HKX2b with the Python decoder (no build needed):

* the clip decodes — 93 frames × 95 tracks = **8,835 rows** plus 93 root-motion
  rows, `bone == track` on **all 8,835** (identity), 60 fps, blendHint NORMAL;
* against `fixtures/human_male_vanilla.nif`: **78 of 95** bones match a node
  case-insensitively, **17** do not (the `Weapon*` list), **4** differ only by
  case — the same 78/17/4 the gates pre-registered;
* the floor: `python scratchpad/hkx2_20260910/identity_floor.py` → **6/6 PASS**.
  Empty, correct-length and a permutation are accepted; lengths 94, 96 and 1
  are still refused by name, on the real bytes of the fixture.

**`release/hkxanim_dump.exe` is STALE** (05:10, older than the patch) and still
prints `REFUSED: binding maps 0 tracks, the animation has 95`. Step 4 below
rebuilds it; that rebuild is the C++ side of this proof and is currently the
only unrun part of the identity rule.

## Paste-able resume (one qmake + one make covers HKX1 and HKX2)

```bash
cd /e/Projects/NifskopeWildWastelandEdition

# 0. the game must be down (CONSTITUTION 6)
tasklist | grep -i -E "Fallout4|NifSkope"; echo rc=$?      # must print rc=1

# 1. qmake FIRST. Five NEW sources/headers are in NifSkope.pro (HKX1's
#    src/hkxanim.{h,cpp}, HKX2's src/hkxplayback.{h,cpp} and
#    src/hkxplaybacktest.cpp) and src/gl/glnode.cpp, src/gl/glscene.cpp and
#    src/nifskope_ui.cpp gained a NEW include ("hkxplayback.h") that the frozen
#    dependency lists do not know.
MSYSTEM=UCRT64 CHERE_INVOKING=1 /c/msys64/usr/bin/bash -lc 'export PATH="$PATH:/e/Tools/GIT/cmd"; cd /e/Projects/NifskopeWildWastelandEdition && qmake NifSkope.pro > scratchpad/hkx2_20260910/qmake.log 2>&1; echo QMAKE-RC=$?; make -j2 > scratchpad/hkx2_20260910/build.log 2>&1; rc=$?; grep -E "error:|Error [0-9]" scratchpad/hkx2_20260910/build.log | head -20; echo BUILD-RC=$rc; exit $rc'

# 2. read the dependency back, by object, for the new header
grep -n "hkxplayback\.h" Makefile.Release | cut -c1-160
#   per line number: awk -v s=<n> 'NR<=s && /^GeneratedFiles\/\.obj\/[a-z_]*\.o:/ {last=$0} NR==s {print last}' Makefile.Release
# glnode.o, glscene.o, nifskope_ui.o, hkxplayback.o and hkxplaybacktest.o must
# all name it. src/hkxplayback.h includes src/hkxanim.h, inside src/, so qmake's
# scan reaches it (no lib/ include is involved).

# 3. the exe is newer than EVERY changed file
EXE=release/NifSkope.exe
for f in $(git status --porcelain -- src res tools tests | awk '{print $NF}'); do
  [ -f "$f" ] || continue; [ "$EXE" -nt "$f" ] || echo "STALE vs $f"; done

# 4. HKX1's decoder gates (no NifSkope.exe needed; rebuilds hkxanim_dump.exe,
#    which MUST be rebuilt — the one on disk predates the identity rule)
MSYSTEM=UCRT64 CHERE_INVOKING=1 /c/msys64/usr/bin/bash -lc 'cd /e/Projects/NifskopeWildWastelandEdition && bash scratchpad/hkx1_20260910/build_dump.sh'
python tests/spells/hkxanim_gates.py       # expect 134 checks / 3 fixture failures
python tests/spells/hkxanim_synthetic.py   # expect 29 / 0
python tests/spells/hkxanim_mutate.py      # expect 20 / 0

# 4b. the identity rule, C++ side (gate (g); the Python side already passes)
./release/hkxanim_dump.exe fixtures/Running_To_Slide_And_Back_To_Running.hkx
#   must print 93 frames / 95 tracks and NOT "REFUSED"
./release/hkxanim_dump.exe fixtures/Running_To_Slide_And_Back_To_Running.hkx --out scratchpad/hkx2_20260910/mixamo_cpp.tsv
python - <<'PY'
a=[l.split('\t') for l in open('scratchpad/hkx2_20260910/mixamo_cpp.tsv')
   if not l.startswith('#') and not l.startswith('frame')]
t=[r for r in a if r[1]!='-1']
print('rows',len(t),'bone!=track',sum(1 for r in t if r[1]!=r[2]))   # 8835 and 0
PY
python scratchpad/hkx2_20260910/identity_floor.py   # 6/6 PASS

# 5. HKX2's in-app gates (a,b,c,d,f) — one NifSkope instance, second monitor
bash tests/spells/hkxanim_play.sh          # writes release/ww_hkxanim_test.log
#   free cross-check, same 78/17/4 on the rigged human fixture:
SRC=fixtures/human_male_vanilla.nif bash tests/spells/hkxanim_play.sh

# 6. gate (e), the pictures — sequential, one instance, invisible, and TWICE:
#    the rigged human fixture with a vanilla clip and with the Mixamo clip.
PREFIX=jog    CLIP=scratchpad/hkx1_20260910/clips/jog.hkx \
              FD=0.0333333 NFRAMES=23 bash scratchpad/hkx2_20260910/shots.sh
PREFIX=mixamo CLIP=fixtures/Running_To_Slide_And_Back_To_Running.hkx \
              FD=0.0166667 NFRAMES=93 bash scratchpad/hkx2_20260910/shots.sh
#    -> scratchpad/hkx2_20260910/images/{jog,mixamo}_{bindpose,frame0,frame_half,frame_last}.png
#    shots.sh now defaults SRC to fixtures/human_male_vanilla.nif (a MESH, so a
#    mesh that fails to follow moving bones is visible). Check the framing on
#    the first PNG: WW_RENDER_CENTER=0,0,64 / WW_RENDER_DIST=260 were pinned for
#    the bone-only skeleton.nif and may need one adjustment for the body — set
#    them once and use the SAME values for all eight pictures.
```

## What each gate must produce (pre-registered before the code was written)

| gate | expected |
|---|---|
| (a) | at frames 0, N/2, N-1 the worst translation <= 1e-4, rotation <= 0.01 deg, scale <= 1e-4, over every bound node (jog.hkx binds 78 on skeleton.nif) |
| (a floor) | the SAME comparison against frame 0 while the scene stands at the last frame must exceed those tolerances |
| (b) | after unload, 0 of ~133 nodes differ from the pre-load snapshot, memcmp on all 13 floats |
| (b floor) | while the clip is active, > 0 nodes differ |
| (c) | 78 matched / 17 unmatched / 4 case-folded, and the summary NAMES the unmatched (WeaponBolt first) |
| (c floor) | 95 invented names -> 0 matched, 95 unmatched |
| (d) | on 35CourtSign01.nif: the clip loads, `setActive` returns false, the sentence contains "does not play", 0 nodes changed |
| (e) | four PNGs per clip, >= 3 distinct md5s each (four identical = the pose never reached the rig) |
| (f) | `tests/spells/hkxanim_play.sh` reads (a)-(d) by name out of `release/ww_hkxanim_test.log` and exits non-zero unless it says PASS |
| (g) | NEW, the identity rule: `hkxanim_dump.exe` decodes the Mixamo clip, 8,835 track rows with `bone == track` on every one; `identity_floor.py` 6/6, the wrong-length bindings still refused by name |

## If a gate fails

Measure the cause, write the number, and do NOT land a fix (rule 6 of
`nifskope-ww-resume-pending`). The likeliest failures and their discriminators:

* **(a) fails at frame 0 only** — the transform walk had not run when the
  harness read the nodes. `wwStepTo` logs `scene t=` beside the frame time; if
  they differ, the repaint is the cause, not the pose.
* **(a) fails on rotation by exactly the same amount on every bone** — the
  quaternion convention. HKX1 measured the hkx quaternion maps to the NIF
  rotation matrix DIRECTLY (2.7e-4 direct, 2.0 transposed), so a uniform failure
  means `Matrix::fromQuat` is being fed the transpose.
* **(b) fails on exactly the nodes that were bound** — the restore ran before
  the last pose was applied. `HkxPlayback::restore` is called from `bind()` and
  from `setActive("")`; a repaint between `unload` and the snapshot re-poses.
* **(e) on the Mixamo clip only, all four identical** — that clip is 60 fps, so
  `FD` is 0.0166667; passing jog's 0.0333333 asks for times past the end and the
  playback clamps to the last frame.
* **(g) still REFUSED** — `build_dump.sh` did not recompile; check the exe's
  mtime against `src/hkxanim.cpp`.

## Owed after the build

* `WW_CHANGES.md` entry — the text is ready at
  `scratchpad/hkx2_20260910/WW_CHANGES_ENTRY.md` (the director splices it), with
  the identity-rule section at its end; its STATUS block still says "not built".
* The eight pictures of gate (e) — bungo is owed a picture of the rigged human
  fixture posed by a clip, per CONSTITUTION 5.
* HKX3's remaining work is section 6 of `scratchpad/lane_hkx2_report.md` (the
  Animation Manager dock's own list, an unload row, where the Load button
  finally lives, the panel-style self-test).
* bungo's open NifSkope window needs a restart after the deploy.
