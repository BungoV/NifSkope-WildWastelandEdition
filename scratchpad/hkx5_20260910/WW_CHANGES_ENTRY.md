<!-- Lane HKX5, 2026-09-10.  TEXT ONLY, for the director to splice into
     WW_CHANGES.md.  NEVER sed -i that file: it is mixed CRLF/LF and stays so;
     splice it in binary and match the neighbouring lines' endings. -->

### glTF animation IMPORT and the FO4 .hkx WRITER (lane HKX5, 2026-09-10)

`src/gltfimport.{h,cpp}` reads one animation out of a `.gltf` (+`.bin`), an
embedded-base64 `.gltf` or a `.glb` and produces lane HKX1's clip type;
`src/hkxwrite.{h,cpp}` writes that clip back out as a Fallout 4 `.hkx`. With
the two, an animation authored anywhere Blender can export goes into the game's
own format, and a shipped clip comes back out of it. Contracts:
`docs/GLTF_IMPORT.md` and `docs/HKX_WRITE_FORMAT.md`. Neither file is in
`NifSkope.pro` yet — the hook-up is a refusing script,
`scratchpad/hkx5_20260910/hookup.py`, because three HKX lanes are queued on
that file. They build through `scratchpad/hkx5_20260910/build_dump.sh` into
`release/hkxwrite_dump.exe`, which is what the gates ran on.

**The clip is written as `hkaInterleavedUncompressedAnimation`**, the class lane
HKXCLASS found registered in the shipped exe. **Two routes, one API:** the
default emits the Havok 2014 packfile directly (no Java), and the alternative
writes HKXPACK XML and runs `hkxpack-cli.jar pack`. For `jog` both produce a
109,376-byte file, and decoding both agrees to **4.7e-10 units / 1.10e-7
degrees** — route A's only cost is its decimal text. Cost of the format: 48
bytes per bone per frame, so `jog` goes from 12,288 bytes compressed to 109,376
uncompressed.

**The element order was measured, not assumed.** Lane HKXCLASS's proof clip has
one track, on which frame-major and track-major are the same bytes. The engine's
own `hkaInterleavedUncompressedAnimation::transformTrack` (rva `0x01fa1ac0`)
computes `data + 48 * (frame * numberOfTransformTracks + track)` and derives the
frame count by dividing `transforms.size` by the track count — both are now
laws the writer obeys and the decoder gates on.

**Round trip 1** (shipped clip → decode → write interleaved → decode) over five
fixtures and both routes: worst **1.0e-07 units**, worst **1.62e-07 degrees**,
64,379 bone-frames. **Round trip 2** (clip → glTF through lane HKX4's exporter →
import → write → decode): **8.0e-06 units, 3.40e-05 degrees** over the 1,794
bone-frames the exporter carries; the 391 rows it drops are the 17 `Weapon*`
bones that have no node on `skeleton.nif`. Root motion survives the same loop to
**1.5e-05 units and 0.0 degrees of yaw**.

**The Mixamo fixture is now writable.** Lane FIXTURE found the Mixamo clip is
refused only because its `transformTrackToBoneIndices` is empty. Read with
HKX2b's identity rule and written by this writer, it comes out with an explicit
95-entry binding, its 60 fps intact, its 93 all-zero root-motion samples
carried, and 8,835 rows bit-identical to the source.

**Resampling** is exact where the grids meet: the 60 fps fixture resampled to 30
gives 47 frames, every one **bit-identical** to the matching 60 fps frame.
A hand-written 3-bone glTF exercising LINEAR, STEP and CUBICSPLINE in one file
imports to its hand-computed values to **0.0 units / 4.38e-06 degrees /
3.93e-08 scale**.

**Gates: 23/23**, `tests/spells/hkxwrite_gates.py`. 13 corruptions of a glTF and
12 of a written `.hkx` are each refused by a sentence naming the field and the
value; because a Havok packfile has no checksum, the floor is a flipped payload
float, which the decoder accepts and the comparator catches at 1.0 unit — the
proof the round-trip gate can go red at all. HKXPACK re-reads our own
directly-emitted file and sees the interleaved class, signature `0xa5eff3f2`,
2,185 transforms and a 95-track binding.

**Three defects were caught by the gates and fixed, and one contract number
corrected** (all four in `MISTAKES.md`): the bone matcher let a partial match
outrank a later exact one (`CamTargetParent` stole `CamTarget`); the floor
harness counted a crash as a red gate; the quaternion angle metric inherited
from the `ww-hkx-animation` skill reports **half** the true angle, proven by a
known-answer control at 0.5/5/45/120 degrees — **lane HKX1's published angle
figures and the skill's section 7 carry the same factor of 2 and are owed an
amendment**; and `docs/HKX_ANIMATION_FORMAT.md`'s `NamedVariant` stride is
0x18, not the 0x20 it states.

**NOT MEASURED: Fallout 4 has never loaded one of these files.** No shipped
`.hkx` uses this class (0 of 15,320). The flight is two files in
`scratchpad/hkx5_20260910/flight/` — an exact rewrite of `JogForward` and the
same file with the head yawed 45 degrees as the positive control.
