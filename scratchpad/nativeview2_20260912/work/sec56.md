
---

## 5. Mistakes

Both are in `MISTAKES_ENTRIES.md` and were spliced to the TOP of the root
`MISTAKES.md` the moment they were recognised, not at the end of the lane.

1. **I wrote a gate that fed the exe paths it could not open, then read the
   resulting picture as a failure of the code under test.**
   `tests/spells/native_lighting.sh` built `WW_LODGEN_RESOURCES` from
   `ROOT="$(cd "$(dirname "$0")/../.." && pwd)"`, which is `/e/Projects/...`, and
   joined two of those with a semicolon. MSYS2 rescues a single argv or env path
   automatically but never a semicolon-joined LIST — and that is written out in
   full, with the reason, at the top of `tests/spells/_harness.sh`, which this
   spell sources on its first line. The object textures never loaded and gate
   (a) reported all four legacy frames as differing from their baselines:
   `legacy_bto_top.png` at 390,854 B against the baseline's 537,794 B, mean
   |dColour| 30.93 over 1,012,736 pixels.

   What found it was not inspection. I rendered the same frame three times and
   got the same wrong bytes each time — so the render was deterministic and the
   difference had to be in the INPUT — and the census file showed the same two
   shapes on the same program in both runs, which ruled out the change under
   test. After converting each half with `_harness.sh`'s own `winpath`: 14
   checks, 0 failures.

   THE RULE, and it is the one I will carry out of this lane: a harness that
   fails on its FIRST run has two suspects and the harness is the one you wrote
   five minutes ago. Render it twice before blaming the exe. And when a file you
   source carries a warning about exactly the shape of value you are building,
   that warning is addressed to you.

2. **I wrote a file-scope helper as if it were a class member.** The first build
   failed: `error: 'BSLightingShaderProperty* Shape::bslsp' is protected within
   this context`, `src/gl/renderer.cpp:136-138`. `Shape` befriends `Renderer`'s
   MEMBERS, not every function in `renderer.cpp`. Cost one build. Fixed by
   reading the two flags inside `Renderer::setupProgram` — which is a member —
   and passing them to the helper as ints.

Two near-misses worth recording even though they did not become mistakes:

3. **A Bash heredoc halved my backslashes, twice**, writing `fix01.py` and a
   report section, both failing with `unexpected EOF while looking for matching
   quote`. `ww-anchored-hookup` §5a says exactly this in advance: anything
   carrying a backslash goes through the Write or Edit tool. I read that after
   the second failure instead of before the first.

4. **I nearly reported `native_open.sh` as this lane's red.** It failed on the
   new exe, and the truncated log did not name the failing check. The honest
   move was the one the brief's own gate list demanded — run the rung and
   compare — and it took reconstructing the pre-change fragment shader to do it
   properly. The reconstruction came back at 20,045 B / 547 LF, the recorded
   before-size to the byte, which is what made the comparison trustworthy rather
   than approximate.

---

## 6. Skill review

### Skills loaded, and which tree served them

**Every skill this lane used came from the repo tree**,
`E:/Projects/NifskopeWildWastelandEdition/.claude/skills/<name>/SKILL.md`. None
had to be fetched from `E:/Projects/Claude/.claude/skills/`.

| skill | what it was used for |
|---|---|
| `nifskope-ww-build-verify` | the whole build chain: game check as its own command, rename aside, `make`'s own rc, exe-newer-than-sources, the stale-object rule, `make -n`, the `cmp` of the link-time copies |
| `nifskope-ww-render-shot` | every frame in this lane; the absolute-path rule for every `WW_*`, `WW_RENDER_CLEAN`, `WW_WINDOW_AT`, `--port <unused>` |
| `ww-test-harness-add` | §5c, the count floor is the MEASURED green count and never a prediction; the exit-77 SKIP that is never a pass |
| `ww-anchored-hookup` | §5a, the Write-tool rule for backslashes (see mistake 3) |
| `ww-render-arm-isolate` | the own/flat/tilt/tiltw arm structure — one input changed per arm, everything else byte-identical |
| `ww-analytic-fixture-gate` | gate (d): a fixture whose answer is arithmetic before the render |
| `ww-module-off-is-identical` | gate (a) and the `forced == &emptyString` condition on the uniform |
| `ww-silhouette-compare` | the IoU statistics and the reason a coverage mask must be read back per frame |
| `ww-toggle-lit-gate` | the lit-vs-data-view distinction `native_open` (d) rests on |
| `nifskope-ww-lodgen` | the bake layout, `.lodl`/`.lodi`/`.BTR`/`.BTO`, the sheet cache |

### The skill that should have existed and did not

**`ww-msys-path-list`** — nothing in the tree says, as a skill, "a
semicolon-joined path list handed to a Windows exe from an MSYS shell is not
converted; convert each element first". The warning exists as a COMMENT inside
`tests/spells/_harness.sh`, where it is found only by someone already reading
that file top to bottom. I sourced that file and still made the mistake. A
comment in a sourced file is not a skill; it is a note to whoever is already
looking.

### The skill I wrote

I did **not** write a new skill file this lane. Instead the facts went where
they will be read at the moment they are needed, which the charter's amendment
asked for directly:

* `.claude/skills/nifskope-ww-render-shot/SKILL.md` gained
  **"The lighting a built LOD document is photographed under"** at the end of
  "Photographing a BUILT document" — the headlight moves with the camera, the
  two view-to-world light vectors with their numbers, why `diffuse = A + D *
  max(N.L, eps)` plus a tone map means a known-answer gate must be built on
  order and equality and never on a ratio, that terrain LOD is model-space-lit
  so a flat sheet cache is the control arm and not the subject, and to take
  `WW_PROGRAM_CENSUS` with every lighting picture.
* `docs/LODGEN_NATIVE_LODO_LODI.md` gained a **`## Lighting`** section in its
  Viewer part with the measured channel order, the bit-12 rule, the viewer path,
  and the measured fact that the legacy `.BTR` goes to `sk_msn.prog`.

**On reflection that was the wrong call for the path-list lesson**, and it is
the one thing I would do differently: the render-shot skill is about pictures,
and the MSYS list rule is about every harness in the tree. If bungo wants it, the
skill to write is `ww-msys-path-list`, short, with the measured before/after
byte counts from mistake 1 as its worked example.

### The tool this lane leaves behind

`WW_PROGRAM_CENSUS=<absolute path>` in `src/gl/renderer.cpp` is small and
general: one deduped row per `(shape, program)` pair — `shape`, `bsver`, `msn`,
`lodland`, `prog` — with the view-space light on the header line. It is what
turned "the terrain looks wrong" into "the terrain is on the program I thought
it was, and the `.BTR` is not", which is the finding in section 4 point 2 and
was not reachable by looking at pictures. It costs nothing when the variable is
unset.
