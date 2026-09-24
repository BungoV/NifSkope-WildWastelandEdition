# Skill amendment, delivered as TEXT — `ww-hkx-animation`

**Lane HKX5, 2026-09-10. NOT APPLIED by this lane, deliberately.**

`.claude/skills/ww-hkx-animation/SKILL.md` exists in two trees that drift
(CONSTITUTION 1a) and lanes HKX2b and HKX5 are both alive in it; lane HKX2's
own MISTAKES entry of today is about appending to this exact file without
reading its headings. CONSTITUTION 8: *lanes deliver changelog and handoff
TEXT, the director splices it.* So this is the text.

---

## The line to change (section 7, "The gates")

Current, last line of section 7:

> `hkxanim_dump.exe CLIP --out x.tsv` and `hkxanim_decode.py CLIP --out y.tsv`
> write the same TSV (`frame track bone tx ty tz qx qy qz qw sx sy sz`, root
> motion as track -1). The angle metric is `2*asin(|q1 -+ q2|/2)`, never
> `acos(dot)` (no resolution below 0.03 deg).

Replace the last sentence with:

> The angle metric is **`4*asin(|q1 -+ q2|/2)`**, never `acos(dot)` (which
> agrees exactly but has no resolution below 0.03 deg in float). **The
> `2*asin(...)` this skill carried until 2026-09-10 is HALF the rotation
> angle**: for `q2 = R(theta)*q1` the chord is `|q1 - q2| = 2*sin(theta/4)`,
> so `2*asin(d/2)` evaluates to `theta/2` — a 45-degree difference reads as
> 22.5. Measured with a known-answer control at 0.5 / 5 / 45 / 120 degrees
> (lane HKX5, root `MISTAKES.md`). **Every angle any lane published with the
> old expression is half of the truth**, including lane HKX1's 1e-5 deg
> (C++ vs Python), 2.7e-4 (skeleton.hkx vs skeleton.nif) and the 0.03 deg
> quaternion-unpacker figure of `docs/HKX_ANIMATION_FORMAT.md` section 4.6.

## The files that still compute the wrong figure

Lane HKX5 did not edit another lane's files. These three carry
`2 * asin(...)` and produce halved angles today:

* `tests/spells/hkxanim_decode.py`
* `tests/spells/hkxanim_gates.py`
* `docs/HKX_ANIMATION_FORMAT.md` section 4.6 (the "< 0.03 deg" claim about the
  rsqrt approximation in `unpackSignedQuaternion40`, which is a real
  measurement made with the halved metric and is therefore ~0.06 deg)

Lane HKX5's own `scratchpad/hkx5_20260910/tsvcmp.py` and `make_3bone.py` are
already corrected, and every number in `scratchpad/lane_hkx5_report.md`,
`docs/HKX_WRITE_FORMAT.md` and `docs/GLTF_IMPORT.md` is the corrected one.

## A second line worth adding to section 8 ("Settled, do not re-derive")

> * `hkaInterleavedUncompressedAnimation::transforms` is **frame-major**:
>   element `frame * numberOfTransformTracks + track`, stride 48, and the
>   engine derives the frame count by dividing `transforms.size` by the track
>   count. Read out of `hkaInterleavedUncompressedAnimation::transformTrack`
>   rva `0x01fa1ac0` (the arithmetic at `0x01fa1b05`-`0x01fa1b33`), because
>   the one-track proof clip of section 9 cannot distinguish the two orders.
>   Writing is `docs/HKX_WRITE_FORMAT.md` / `src/hkxwrite.{h,cpp}`.
> * `hkRootLevelContainer::namedVariants` entries are **0x18 bytes**, not the
>   0x20 `docs/HKX_ANIMATION_FORMAT.md` section 1 states: `name` +0,
>   `className` +8, `variant` +0x10. Same fixup run in the shipped `jog.hkx`
>   and in HKXPACK's output.

## Whichever tree is edited, both must end byte-identical

`E:\Projects\Claude\.claude\skills\ww-hkx-animation\SKILL.md` (the live tree,
what the director and account B load) and
`E:\Projects\NifskopeWildWastelandEdition\.claude\skills\ww-hkx-animation\SKILL.md`
(what a lane whose cwd is this repo loads). Say the byte count of both when it
is done — lane HKX2's mistake today was leaving one behind.
