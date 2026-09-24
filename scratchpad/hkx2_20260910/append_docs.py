import io

R = 'E:/Projects/NifskopeWildWastelandEdition/'

# ------------------------------------------------ WW_CHANGES entry (text only)
p = R + 'scratchpad/hkx2_20260910/WW_CHANGES_ENTRY.md'
add = """
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
"""
s = io.open(p, encoding='utf-8', newline='').read()
if '## A third-party clip with no bone mapping now plays' in s:
    print('WW_CHANGES_ENTRY already has the section')
else:
    if not s.endswith('\n'):
        s += '\n'
    io.open(p, 'w', encoding='utf-8', newline='').write(s + add)
    print('WW_CHANGES_ENTRY appended; CR =', open(p, 'rb').read().count(b'\r'))

# ------------------------------------------------------------------ MISTAKES
p = R + 'MISTAKES.md'
entry = """
## 2026-09-10, lane HKX2: a skill section was appended without reading the headings it was joining

**What was done.** HKX2's `skillamend.py` appended its amendment to
`.claude/skills/ww-hkx-animation/SKILL.md` as `## 9. Settled by lane HKX2`,
straight onto the last line of the file. The script's only guard was that its
own heading text was absent.

**What was true instead.** The file already had a `## 9.` (lane HKXCLASS) and a
`## 10.` (lane FIXTURE), landed by two other lanes the same day. The result was
a second `## 9.` at the bottom and, because the append lost its leading blank
line, a heading welded to the previous paragraph. It also went into the repo
skill tree only; the live tree the director loads was left behind
(CONSTITUTION 1a, the two trees drift).

**How it was found.** Lane HKX2b listed the headings of both copies side by side
before believing the amendment had landed.

**The rule that prevents it.** An append to a shared, numbered document reads
the existing headings first and takes the next free number, and it asserts the
blank line it is joining onto -- `core-register-append` says exactly this for
the FO4CS registers and it applies here. And a skill amendment is not finished
until BOTH trees are byte-identical: the amending lane says so with the byte
count, or the director mirrors it and says so.
"""
s = io.open(p, encoding='utf-8', newline='').read()
if 'a skill section was appended without reading the headings' in s:
    print('MISTAKES already has the entry')
else:
    if not s.endswith('\n'):
        s += '\n'
    io.open(p, 'w', encoding='utf-8', newline='').write(s + entry)
    print('MISTAKES appended; CR =', open(p, 'rb').read().count(b'\r'))
