# WW_CHANGES.md entry — lane HKX2 (text only; the director splices it)

## Havok animation clips play on the open NIF's bones (2026-09-10, lane HKX2)

bungo's ruling, verbatim: *"in animation workspace, add an option to load a hkx
file with animation, then they get added to the animations list, and if there's
rigged geometry with nodes / bone names that match, they play"*. Lane HKX1 built
the reader and the spline decompressor; this is the half that puts the decoded
pose into the scene graph.

**A loaded clip is an entry in the animations list, and that is the whole of the
transport.** `HkxPlayback::registerInScene` puts the clip's name into
`Scene::animGroups` and its start/end into `Scene::animTags`, which is what
`Scene::timeMin/timeMax` already answer from. Play, pause, loop, reverse, speed,
scrub, "cycle through sequences" and the Timeline dock's ruler therefore drive a
Havok clip with no new transport code at all — four lines instead of a second
player. Selecting one goes through `Scene::setSequence`, so choosing any of the
NIF's own sequences unbinds it.

**The pose is written where a controller writes it.** `Node::transform()` calls
`HkxPlayback::applyLocal()` immediately after `IControllable::transform()` — one
step after the node's own controllers, before its collision body is the first
thing in the frame to ask for a world transform, and before any child is walked.
So a clip WINS over a `NiTransformController` that names the same node instead
of losing to it, no cached world transform can be built from a local that is
about to change, and skinning, bounds, node markers and picking follow with no
changes anywhere else. `Node` gained one `friend class HkxPlayback`, beside the
five controller classes that already write `Node::local`.

**The mapping is case-insensitive and partial, because the files are.**
skeleton.hkx and skeleton.nif disagree in CASE on Head, Spine1, Spine2 and
Weapon, and 17 `Weapon*` bones of the animation skeleton have no node in the
body NIF at all. Measured: 78 matched, 17 unmatched, 4 case-folded. Matched
bones play; the unmatched are NAMED in the summary line, not counted; zero
matches refuses in words and writes nothing. The binding is recomputed at every
`setActive` and at every `Scene::make`, so a clip selected after a different NIF
was opened can never pose nodes that are gone.

**Unloading is exact.** Every node the clip touches has its pre-pose
`Transform` kept by value at bind time and assigned back on unbind — the same
bit patterns, not a re-read of the NIF.

**A clip does not carry its own bone names**, only track -> bone index against a
skeleton it names. Four fallback arms, and the summary line says which one
served: a skeleton already loaded this session (including the file's own), then
`skeleton.hkx` beside the clip or in a `CharacterAssets` folder above it, then
the same walk from the open NIF, then the game archives. The floor is a refusal
that names the skeleton it wanted.

**Root motion is a switch of its own and starts off** (his ruling). On it is
composed OUTSIDE the root bone's own transform, on the node named by the
animation skeleton's root bone, and the summary says which node that is.

New: `src/hkxplayback.{h,cpp}`, `src/hkxplaybacktest.cpp` (the WW_HKXANIM_TEST
harness — its own translation unit, so the 400 lines of gates cost the
31,000-line `nifskope_ui.cpp` three), `tests/spells/hkxanim_play.sh`,
`scratchpad/hkx2_20260910/` (syntax pass, picture script, PENDING).
Changed, counted off `git diff -U0` and no deletions anywhere:
`NifSkope.pro` +3, `src/gl/glscene.h` +10 (the forward declaration and the
`hkx` member), `src/gl/glscene.cpp` +22 (include, constructor, destructor,
`clear`, `make`, `setSequence`), `src/gl/glnode.h` +3 (the friend line),
`src/gl/glnode.cpp` +15 (include and the one call in `Node::transform`), and
`src/nifskope_ui.cpp` +87 across four spots: the include, the 3-line harness
call, 19 lines of `WW_HKXANIM_CLIP` in the render hook so a clip can be
photographed, and 63 lines in the Animation panel — the "Load Animation
(.hkx)…" button, the summary label under it and the Root motion row.

**STATUS: NOT BUILT.** `Fallout4.exe` was up (pid 41056) and no GO file existed,
so under CONSTITUTION rule 6 the lane ended BUILD PENDING. Every new and changed
file passes `g++ -fsyntax-only` with the real `Makefile.Release` flags, RC=0,
with no new warnings. Nothing has been run: gates (a)-(e) are written, ordered
and unrun, and the numbers above for the 78/17/4 mapping are lane HKX1's
measurement, not this lane's. Resume: `scratchpad/hkx2_20260910/PENDING.md`.

## A third-party clip with no bone mapping now plays (2026-09-10, lane HKX2b)

`fixtures/Running_To_Slide_And_Back_To_Running.hkx`, out of bungo's Mixamo
Collection, is a perfectly good FO4 spline clip -- 95 tracks, 93 frames,
60 fps, THREECOMP40 -- and both decoders refused it in one sentence: *"binding
maps 0 tracks, the animation has 95"*. Its `hkaAnimationBinding` carries an
EMPTY `transformTrackToBoneIndices`, and an empty mapping is the IDENTITY map,
not a missing one: track i drives bone i. Lane FIXTURE measured that rather than
assuming it -- frame 0's per-track translation against `skeleton.hkx`'s
reference pose matches on 75 of 95 tracks at shift 0 and on only 18 at any other
shift, and the 20 that differ at shift 0 are the ones that should (`COM` travels
487 units, the 13 `Weapon*`/`Camera` nodes the clip places, `Spine1`, and four
finger tips at float noise).

The rule is now in both readers and in the consumer, and the fallback names
itself: `HkxAnimClip::trackToBoneIsIdentity` is set when the file gave no
mapping, so a derived map is never reported as a stored one. A NON-EMPTY vector
of the wrong length is still refused by name -- that is the floor, and it is run
on the real bytes of the fixture with the binding substituted
(`scratchpad/hkx2_20260910/identity_floor.py`, 6/6: empty, full-length and a
permutation accepted; 94, 96 and 1 refused). Whether the skeleton is big enough
for an identity map -- at least `numTracks` bones -- is the CONSUMER's gate in
`HkxPlayback::bind`, because a clip file usually carries no skeleton at all.

Measured with the Python decoder: the clip decodes to 93 x 95 = 8,835 track rows
plus 93 root-motion rows, with `bone == track` on every one of the 8,835; and
78 of its 95 bones name a node in `fixtures/human_male_vanilla.nif`
case-insensitively, 17 do not (the `Weapon*` list, which lives on a weapon NIF)
and 4 differ only in case -- the same 78 / 17 / 4 the gates pre-registered.

Changed: `src/hkxanim.h` (+6, the flag), `src/hkxanim.cpp` (validate + the
identity fill in `decodeClip`), `tests/spells/hkxanim_decode.py` (the same two,
so the C++ and Python oracles stay a matched pair), `src/hkxplayback.cpp` (its
half was already in). New: `scratchpad/hkx2_20260910/identity_floor.py`.
`scratchpad/hkx2_20260910/shots.sh` now defaults to the rigged human fixture and
takes `PREFIX`, so gate (e) runs once per clip.

**STILL NOT BUILT.** Everything above the C++ side was proved with the Python
decoder, which needs no build; `release/hkxanim_dump.exe` predates the change and
still prints the refusal until `scratchpad/hkx1_20260910/build_dump.sh` is
re-run. Resume: `scratchpad/hkx2_20260910/PENDING.md`.
