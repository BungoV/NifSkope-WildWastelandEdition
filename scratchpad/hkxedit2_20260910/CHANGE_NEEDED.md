# Lane HKXEDIT2 -- changes OTHER lanes' files need (not applied; ww-anchored-hookup section 3)

These are behaviour changes in files this lane does not hold. Each names the
mechanism, the candidates and the gate that would prove it. None is a hook-up
and none is applied by a resume.

## 1. Float tracks through the writer and the reader (src/hkxwrite.cpp, src/hkxanim.cpp)

**What the workspace does now:** float tracks are rows of the document
(`HkxClipDocument::floatTracks`, keys + dense values); `save()` refuses in
words while any exists: *"The document carries N float track(s), which the
.hkx writer does not carry yet"*. The refusal is gated (standalone gate (t)).

**Writer (HKX5's file):** `hkaInterleavedUncompressedAnimation::floats`
(`hkArray<hkReal>` at +0x48) is the float data; by the same accessor law as
`transforms` (frame-major, `floats[frame * numberOfFloatTracks + f]` -- the
engine's `floatTrack` accessor beside `transformTrack` rva `0x01fa1ac0` should
be read to CONFIRM the order, one disassembly, as HKX5 did for transforms),
`hkaAnimation::numberOfFloatTracks` = the count, one extra
`hkaAnnotationTrack` per float track is NOT added (annotationTracks is sized
by transform tracks only in every shipped clip), and the binding's
`floatTrackToFloatSlotIndices` = one `hkInt16` per float track (identity, or
the slot index the user names). Candidate 2: the engine sizes floats by
`numberOfFloatTracks * numFrames` with numFrames derived from transforms --
confirm by the accessor's idiv.

**Reader (HKX1's file):** decode the float blocks of a spline clip (the four
shipped clips: `numberOfFloatTracks` 1, masks after the transform masks in the
same 4-byte table, `floatBlockOffsets[b]` = the float data start) and the
`floats` array of an interleaved clip; hand them out as
`HkxAnimClip::floatTracks[frame][f]` (a new member; `HkxClipDocument::fromClip`
then seeds `floatTracks` from it instead of empty).

**Gate:** a shipped float-track clip (census: 4 furniture/animobject clips)
decoded, saved interleaved, HKXPACK re-read shows `floats numelements =
numFrames`, our reader reads the same values back to 1e-6; the workspace's
Set float key on it round-trips.

## 2. HKXPACK prints empty annotation text for files src/hkxwrite.cpp emits (HKX5's file)

Measured (report section 2a): the writer's LOCAL FIXUP ORDER differs from the
canonical one (16 bytes in the fixup tables of the 109,376-byte jog file);
HKX1's reader and HKXEDIT1's oracle read the names, HKXPACK 0.1.6 does not.
The workspace routes its saves through `Hkx::File` read->write
(`WW_HKXCLIP_CANON`) so its files are canonical. The writer itself could emit
the fixups in the canonical order (payload order of the struct array, strings
after their struct run) -- `tests/spells/hkxfile_oracle.py layout` on a file it
wrote names the chunk. Gate: HKXPACK unpack of a writer file shows the
annotation names; HKX5's 23 gates unchanged. Not done here: HKX5's file, and
the game's own reading of either order is unflown.

## 3. src/ui/widgets/timeline.* retirement (the follow-up)

The old Animation Manager dock stays until the new workspace is proven in the
app (gates a-j run). The follow-up: remove `dTimeline` / `timeline` from
`nifskope.h` / `nifskope_ui.cpp` / `nifskope.cpp`, the `.pro` lines for
`src/ui/widgets/timeline.{h,cpp}`, `timelineedit.cpp`, `timelineviews.cpp`,
`timeline_p.h`, the `WW_CYCLETYPE_TEST` and `WW_HKXANIM_UI_TEST` harnesses
that read its widgets (their checks move to `animws.sh`), and `tlMakeIcon` /
`tlWriteIconSheet` (used by the main toolbar: keep those two in a file of their
own first). What the new dock does NOT yet do that the old one did: edit NIF
keys (values, tangents, easing, CSV, lint) -- those stay in the Blocks tab
until a NIF-key layer is written for the new sheet; the report lists this as
the one divergence bungo decides on.
