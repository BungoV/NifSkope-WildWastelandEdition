# Lane HKXEDIT1 -- the RAW layer: an .hkx as an editable block tree (2026-09-10)

Repo `E:\Projects\NifskopeWildWastelandEdition`, main, **nothing committed**
(the brief). bungo's ruling, verbatim: *"Just make hkx fully editable in our
nifskope"*. This lane is the raw layer: a generic packfile object model over
the exe's own class reflection, a byte-identical writer, and a Blocks-tab model
over it. HKXEDIT2 (the animation layer) and HKXEDIT3 (round trip + vocabulary
gates) build on it. **NifSkope.exe was neither built nor launched by this lane;
the hook-up is a refusing script, not applied.** State: BUILD PENDING
(`scratchpad/hkxedit1_20260910/PENDING.md`).

Skills invoked: `ww-hkx-animation`, `reverse-engineering`, `behaivor-graph`,
`nif`, `ww-contract-provenance`, `ww-control-calibration`,
`ww-standalone-writer-gate`, `ww-anchored-hookup`, `nifskope-ww-resume-pending`,
`ww-test-harness-add`.

Deliverables (sha256 16, bytes; every text file LF-only by Python byte count):

| file | sha256 | bytes | what |
|---|---|---|---|
| `tools/hkclassdb_extract.py` | `4867772b3f3fc694` | 31,805 | the class-database extractor (exe + Todd's treat symbols -> JSON, HKXPACK cross-check, self-check) |
| `res/hkclasses_fo4.json` | `d1ae65608fb37280` | 1,511,662 | the database: 943 classes, 908 registered |
| `src/hkxfile.h` / `.cpp` | `77bb498a9649a858` / `82f22b12266063bb` | 10,433 / 54,306 | `Hkx::ClassDb`, `Hkx::File` read / write / find / resize / addObject, dump |
| `src/hkxmodel.h` / `.cpp` | `79801bd14b1a2183` / `d389bb0c8cc3f161` | 5,570 / 26,715 | `HkxModel : BaseModel`, undo, save, `animFile()` |
| `src/hkxmodeltest.cpp`, `tests/spells/hkxmodel_test.sh` | `6d7a75bf19ed9165`, `9bf802bc82f6d699` | 9,000, 1,932 | WW_HKXMODEL_TEST, the in-app gate (written, NOT run) |
| `tests/hkxfile_gate.cpp` -> `release/hkxfile_gate.exe` (16:00:55) | `484c34e3a7fd4131` | 11,125 | the standalone Qt6Core gate binary |
| `tests/spells/hkxfile_oracle.py` | `7efba5383ca37874` | 40,498 | the independent Python reader / writer / layout diff / census |
| `tests/spells/hkxfile_gates.py` | `daf60171208152f0` | 19,674 | gates (a)-(e); run 2 = 109 checks, 0 failures |
| `docs/HKX_PACKFILE_MODEL.md` | `01c30dc61954fea4` | 18,385 | the contract, provenance pass 13/13 anchors, 0 moved on the second run |
| `scratchpad/hkxedit1_20260910/` | | | `hookup.py` (--check 10/10), `PENDING.md`, `WW_CHANGES_ENTRY.md`, `MISTAKES_ENTRIES.md` (appended), `SKILL_AMENDMENT_ww_hkx_animation.md` (appended to the repo skill), `extract_census.py`, `build_gate.sh`, `sx.sh`, `stamps.py`, census reports and gate runs |

## 1. The class database (res/hkclasses_fo4.json)

`tools/hkclassdb_extract.py` reads the 1.10.155 exe
(Todd's treat, sha256
`886d67fc955be02d`) and its symbol dump:

* `hkBuiltinTypeRegistry::StaticLinkedClasses` (rva `0x02e40880`): **908**
  hkClass pointers, every one of which is built at run time by a
  `dynamic initializer for 'XClass''` -- the on-disk `.data` page holds no
  bytes for the object, so the initializer is EMULATED (capstone, a register
  and absolute-address stack tracker over ~20 straight-line instructions up
  to the call to `hkClass::hkClass(this, name, parent, objectSize,
  numIfaces, enums, numEnums, members, numMembers, defaults, attributes,
  flags, version)`). 952 such initializers exist; 950 call the constructor;
  943 distinct names (the four meta classes hkClass / hkClassMember /
  hkClassEnum / hkClassEnumItem are built 2-4 times: the REGISTERED object is
  the class, the older variants are recorded under `variants`).
* Member arrays (`XClass_Members` / `X::Members`, 0x28-byte hkClassMember
  records) are read raw; their `enum*` slot is ZERO on disk and written at run
  time by a second initializer (`... 'XClass_Members''`: 178 `mov
  rax,[rip+ptr]; mov [rip+slot], rax` pairs), emulated the same way.
  hkClassEnum objects are const (`.rdata`). Each member records `enumClass`,
  the class that DECLARES its enum.
* **The signature is computed, not copied.** `hkClass::getSignature` (rva
  `0x014fff50`) = `~crc32` over `hkClass::writeSignature` (`0x014fffe0`) of
  the class then each parent; re-implemented from the disassembly (the
  contract section 1 has the byte recipe), it reproduces HKXPACK's stored
  signature for **908 of 908** classes -- the proof that every field entering
  the hash (names, types, subtypes, flags, array sizes, enum items) was read
  right. It also caught the emulator's first defect (Mistakes 4).

Cross-check against HKXPACK 0.1.6-beta's `classxml/` (908 classes): **906
agree in every field**; 2 disagree (`hclVolumeConstraintMx{Apply,Frame}SingleData`:
HKXPACK lists a 4th member `padding` the exe does not declare; the signatures
still agree, so HKXPACK's extra row is its own; THE EXE WINS); 35 exe-only
(`hkx*` scene classes, `hkMonitorStream*`, `hkOstream`); 0 HKXPACK-only.
Self-check (offset + size inside the class size and past the parent's,
parents resolve, no cycles): **0 problems** over 943 classes once the
empty-base-class rule was applied (header layout byte 3 `emptyBaseClassOpt =
1`: a parent with no members and objectSize 1 occupies no bytes in the
child -- the four `hkcdDynamicTreeDynamicStorage0*` classes).

Tables: `scratchpad/hkxedit1_20260910/classdb_crosscheck.tsv` (one row per
class), `classdb_selfcheck.txt`, `classdb_extract.log`.

## 2. The packfile model (src/hkxfile.{h,cpp}) and its oracle

`Hkx::ClassDb` loads the JSON (linking parents, member classes, enums;
flattening the member list parent-first; found beside the exe, at
`WW_HKCLASSDB`, or under `res/`). `Hkx::File::read` walks every virtual
fixup's object from its class's member list -- no per-class code; the type
branches are the container types Havok itself special-cases: hkArray (local
fixup to a 16-byte header's payload), hkRelArray (u16 size + u16 offset from
the member's own address, payload INSIDE the object's chunk), hkStringPtr /
char* (local fixup; null vs "" distinguished), object pointers (global fixup
to an object start), enums / flags (storage from the subtype), inline structs
and C arrays. Plain-typed array payloads (hkUint8 spline data, hkInt16 bone
maps, hkUint32 block offsets, hkQsTransform poses) stay packed bytes. Every
struct keeps its HOLE bytes (vtable slot, refcount, padding) so they go back
verbatim; every serialised field is re-encoded from the decoded value (reals
by their exact 32-bit pattern). Unmodelled and refused by name: `hkVariant`
with a class fixup, `hkHomogeneousArray`, `hkSimpleArray` in data, hkRelArray
of pointers / strings / inside an array element, pointers with local fixups,
C arrays of hkArray -- none occur in the census.

`Hkx::File::write` lays the file out canonically (contract section 3):
objects depth-first from the root, each object's extras in member order
(parents' members first), pointees visited after the whole object, unreached
objects in file order; chunk alignment rules measured on the files
themselves; local fixups in write order, global fixups in flush order,
virtual fixups in object order; class names in the file's order plus
first-use appends (with the database's signature); the header's first 0x50
bytes kept verbatim with the contents offsets patched. Editing API: `find`
by path (`#2.annotationTracks[0].annotations[1].text`), `resizeArray`,
`addObject`, `defaultValue` (identity transforms, null strings, empty
arrays).

The INDEPENDENT oracle `tests/spells/hkxfile_oracle.py` (Python, same JSON,
no code shared) does the same and adds `layout` (the chunk-by-chunk diff
that found every rule) and `census`.

## 3. Round trip numbers

Census set: every `.hkx` in `Fallout4 - Animations.ba2` (15,320 files,
extracted by `scratchpad/hkxedit1_20260910/extract_census.py` in 13 s to
`census_hkx/` (gitignored), `census_manifest.tsv` with sizes and sha256).

| reader / writer | files | parsed | byte-identical | mismatched | refused | time |
|---|---|---|---|---|---|---|
| C++ `hkxfile_gate census` (gates run 2) | 15,320 | 15,278 | **15,278** | **0** | 42 | 3.0 s |
| Python oracle (run 3 / gates run 2) | 15,320 | 15,278 | **15,278** | **0** | 42 | 29.8 s |

The 42 refusals are one class, by both readers with the same sentence:
`hclClothSetupContainer is not in the class database (943 classes)` -- 42
cloth-setup files (`Meshes/Actors/Character/CharacterAssets/Hair/...`,
`...Cloth...`) carry a class the 1.10.155 exe does not register and HKXPACK
lacks too; refused by name, never guessed. 105 distinct classes occur across
the parsed files (`census_py_run3.tsv.classes.tsv`).

The seven HKX1 fixtures (jog, tpose_idle, twoblock, q48, skeleton, lossless,
turn) round-trip byte-identically through both; skeleton.hkx is the one with
hkRelArray (ragdoll capsule shapes), 83 objects of 12 classes.

Layout rules the census FOUND (each one mismatch class in
`census_py_run1.tsv` / `run2` before its fix; `layout` named the chunk):
a payload of struct elements is not padded after (jog: annotationTracks ends
0xac8 and track 0's "" trackName is AT 0xac8); a payload of pointers or plain
values is padded to 16 (skeleton: referencedObjects ends 0x4af8, name at
0x4b00); a payload of string pointers is not padded and its strings sit at
EVEN offsets, one 16-pad after the run (AlienRootBehavior eventNames[0] 9
bytes at 27736, [1] at 27746, [2] at 27756); a direct string member pads to
16 after itself; global fixups follow the flush order (skeleton
hknpRagdollData: bodyCinfos[i].shape before its own `skeleton` at +0x88);
hkRelArray payloads sit after the body, 16-aligned, in member order; objects
pad to 16 after the body. Run 1 (before any rule): 14,833 identical / 445
mismatched; run 2: 14,762 / 516 (the string-array rule half applied); run 3:
15,278 / 0.

## 4. The gates (pre-registered; `tests/spells/hkxfile_gates.py`, run 2)

`scratchpad/hkxedit1_20260910/gates_run2.txt`: **109 checks, 0 failures,
38.9 s** (run 1: 15 failures, every one a gate defect -- Mistakes 3 and the
two mutation sites -- not a model defect).

| gate | what | result |
|---|---|---|
| (a) | byte-identical read->write over the census by BOTH readers; the seven fixtures by both | 15,278 / 15,278 each, 0 mismatched, the same 42 refused for the same reason |
| (b) | edit->save->reload on a copy of jog.hkx: numFrames 23->24 (int), frameDuration -> 0.0416667 (float), originalSkeletonName -> "EditedRoot" (string), blendHint NORMAL->ADDITIVE (enum), blockOffsets 1->3 (array length); the oracle writes it AND the C++ model writes it; each file re-read and diffed field by field against the original tree (holes included) | the two writers produce the SAME bytes; exactly the five edits differ (`#2.numFrames`, `#2.frameDuration`, `#4.originalSkeletonName`, `#4.blendHint`, `#2.blockOffsets.{n,vals}`), everything else identical; the C++ reader reads the five values back; FLOOR: a nudged `duration` is the one field the diff reports |
| (c) | HKXPACK unpacks what we wrote | jog_edit (both writers) and jog: rc 0; the XML shows `numFrames 24`, `EditedRoot`, `ADDITIVE` |
| (d) | the class database | 908 registered; 0 self-check problems; 908/908 signatures equal HKXPACK's; 0 HKXPACK-only; the eleven animation classes' objectSize + member offsets equal HKXCLASS's / HKX1's measurements (hkaSplineCompressedAnimation 0xb0 and its 13 members, interleaved 0x58 transforms +0x38 floats +0x48, lossless 0xe0, predictive 0xc0, quantized 0x58, referencePose 0x40, hkaAnimation's 6, hkaSkeleton 0x88 + 8, hkaAnimationBinding 0x58 + 6, hkaDefaultAnimatedReferenceFrame's 4, hkaAnnotationTrack 0x18 + 2); the twelve shipped signatures equal the files'; every member fits its class (an independent pass, 0 violations) |
| (e) | 20 corruptions of jog.hkx refused by NAME by both readers | 20/20: magic, fileVersion, bytesInPointer, numSections, contents section, predicate padding, section tag, data start, local / virtual table offsets, class-name separator, class name, a string fixup onto non-zero bytes, a global fixup to a non-object, a virtual fixup to a non-name, a section index, an oversize pointer array, an unconsumed local fixup, an array size past the payload, non-zero bytes on a null array; the stated limit (a flipped payload float is a value, not a refusal -- no checksum) shown accepted |
| (f) | the standalone binary (Qt6Core, ww-standalone-writer-gate) | `release/hkxfile_gate.exe` (`build_gate.sh`, BUILD-RC=0): dump / roundtrip / census / edit / get / classdb; runs (a) (b) (e) with no exe |
| (g) | WW_HKXMODEL_TEST (`src/hkxmodeltest.cpp`, `tests/spells/hkxmodel_test.sh`) | WRITTEN and syntax-checked (SYNTAX-RC=0), NOT RUN: pre-registered checks (a) the tree's model is an HkxModel with 6 blocks in file order, (b) numFrames edit / undo / redo / clean->dirty, (c) unedited save byte-identical, edited save differs inside the animation object and reloads with 24, (d) animFile() decodes 23 frames |

`ww-control-calibration` did not apply: the oracle here is a second decoder
plus shipped bytes, not a vanilla-vs-ours scalar.

## 5. The Blocks-tab model (src/hkxmodel.{h,cpp})

`HkxModel : BaseModel`, the KfmModel idiom (the base of NifModel and
KfmModel, so the block list, the block details tree and the NifDelegate
editors -- ValueEdit, enum combo, flag check list -- work unchanged; the
delegate guards every NifModel-specific path with `inherits("NifModel")`, read
in `src/model/nifdelegate.cpp`). Each object is a top item named by its class
(shown "class [i]"); each serialised member a child; arrays as array items
with one child per element; inline structs, C arrays, matrices and
hkQsTransform (Translation / Rotation / Scale) as compounds; enums and flags
registered with `NifValue::registerEnumType` under `hkEnum <Name>@…` /
`hkFlags <Name>@…` (flag options are BIT INDICES, as NifCheckBoxList expects);
pointers as `tLink` shown "i (class)"; strings as `tSizedString`;
`hkArray<hkUint8>` as one read-only byte-blob row (the spline data, 68 KB, is
not 68,000 rows). Edits: `setData` wraps `BaseModel::setData` in
`HkxSetValueCommand` on `HkxModel::undoStack`; `setArraySize` inserts default
elements / removes the tail as `HkxArrayResizeCommand`; `addObject(className)`
appends a zero-filled block. Save: every item is synced back into the
`Hkx::File` the file was read into (the layout store: object order,
class-name order, header, capacities, holes; struct children skip ignored
members in step) and written -- an unmodified document saves byte-identical
(gate (g)'s check (c) pre-registers it in the app; gate (b) proves the same
mechanism through the File API).

**The animation workspace reads a clip from an open document by decoding
its bytes:** `HkxModel::animFile()` = `hkxAnimLoadPackfile( toBytes() )`, the
reader `HkxPlayback::load` uses on a disk file. No second decoder, no clip
type exposed by the model; an edit is visible to the playback after a
re-decode (the hook-up hands the bytes to the Files-tab animation route at
load; re-decoding on `dataChanged` is HKXEDIT2's).

Known limits, stated: a non-null "" string cannot be made null from a cell
(a null string that is left empty stays null); the blob row is not editable
here; hkInt8 shows as an unsigned byte (NifValue has no signed byte); a new
struct-array element is zero-filled with identity transforms; the undo stack
is the model's own (section 6).

## 6. Hook-up (NOT applied) and what it wires

`scratchpad/hkxedit1_20260910/hookup.py` (ww-anchored-hookup): 10 edits over
four files, `--check` = every anchor `count=1` (NifSkope.pro x3: the two
headers, the three sources, `res/hkclasses_fo4.json` copied beside the exe
with nif.xml; `src/nifskope.h` x2: `class HkxModel;`, `HkxModel * hkx /
hkxEmpty`; `src/nifskope.cpp` x4, CRLF: the include, construction beside
`kfmEmpty`, the `.hkx` branch of `load()` mirroring the `.kfm` branch -- the
NIF in the viewport stays, the block views bind to `hkx`, the bytes ALSO go
to `wwFilesTabOpenAnimation` so the clip plays on that NIF, and a NIF loaded
afterwards takes the views back -- and the `.hkx` branch of `saveFile()`;
`src/nifskope_ui.cpp` x1: the harness call). `--apply` refuses unless all
match once and asserts CR counts unchanged. Not applied: those files are held
by other lanes' pending patches.

Owed, named: **Edit > Undo does not reach the .hkx document** -- the window's
actions are created from `nif->undoStack` (`src/nifskope_ui.cpp:23693`); a
`QUndoGroup` over `nif->undoStack` and `hkx->undoStack` is the fix (one edit
in nifskope_ui.cpp, not this lane's file). Until then Ctrl+Z on an .hkx
document undoes the NIF; the model's own stack works (the harness calls it
directly).

## 7. Docs

`docs/HKX_PACKFILE_MODEL.md` under `ww-contract-provenance`: the two stamp
tables (seven sources, sha256 / bytes / lines, written by `stamps.py`), a
claim table of 13 anchors, each matching exactly once on the second run (0
moved); sections: the class database and its provenance (EXE / HKXPACK
authorities, rvas), what is generic and what is special-cased (the type
table), the layout rules with the FILE and offset that taught each, the
model, refusals, the gate table. The `ww-hkx-animation` skill gained section
13 (repo tree; the director applies it to the live tree).

## 8. What HKXEDIT2 needs from this

* `HkxModel::animFile()` for frames; `HkxModel::file()` / `toBytes()` for the
  raw graph; `Hkx::File::find(path)` for any field by name; `resizeArray` /
  `addObject` / `defaultValue` to grow arrays and add objects (an interleaved
  clip = `addObject("hkaInterleavedUncompressedAnimation")`, its
  `transforms` array resized to tracks x frames, the container's
  `animations[0]` pointer re-pointed).
* The blob row (`hkArray<hkUint8> data`) is read-only in the raw layer; the
  animation layer replaces it (or the whole object with an interleaved one,
  which the writer already emits -- HKX5).
* A `dataChanged` -> re-decode hook, so a raw edit shows in the clip view.
* The QUndoGroup (section 6).
* The census refusal (`hclClothSetupContainer`) is a class the runtime lacks;
  if cloth files matter, the class comes from a different Havok build, not
  from this exe.

## 9. Mistakes (appended to MISTAKES.md, five entries)

1. A Bash heredoc halved the backslashes again (a `"\\n"` in a print became
   a newline); repaired with Edit; the fifth recording of the same trap.
2. `cat -A` through the tool was trusted about line endings (no `^M`
   shown); the four nifskope.cpp anchors were written LF and counted 0; the
   Python byte probe showed CRLF; per-file EOL table in the script.
3. The gate wrapper passed `#2.numFrames=24` to bash unquoted -- a comment;
   the C++ edit applied nothing and printed "wrote"; the per-field diff
   floor caught it. Quote everything; a "wrote" line is intent.
4. The initializer emulator missed the `mov rax, rsp` shape and declared 30
   classes enum-less; only the independent signature hash (878/908) said so.
5. The syntax-flag string was copied a third time instead of shared.

## 10. PENDING (nifskope-ww-resume-pending)

`scratchpad/hkxedit1_20260910/PENDING.md`: process check, `hookup.py
--apply`, **qmake before make** (three new SOURCES, two HEADERS, the JSON
copy line), the dependency read-back on `hkxmodel.o`, the exe-newer sweep,
`bash tests/spells/hkxmodel_test.sh` (first run ever), `python
tests/spells/hkxfile_gates.py` again, the four documents. Note: `release/
NifSkope.exe` is 15:52:46 today, relinked by another lane during this one;
nothing of this lane is in it.

## 11. Finished-work skill review

Loaded and used in earnest: `ww-hkx-animation` (s9 the registry walk and
the class census that sized this lane, s3 the 0x3e trap), `reverse-engineering`
(the Todd's treat tooling's disassembly, the hkClassMember layout, the toolchain page's warning
that hkClass objects are not on disk -- which is why the initializers were
emulated rather than the objects read), `behaivor-graph` (HKXPACK's jar,
whose `classxml/` was the second oracle), `nif` (the BaseModel / NifItem /
NifValue / NifDelegate idiom the model follows, read from kfmmodel.cpp and
nifdelegate.cpp), `ww-contract-provenance` (stamps.py is its scripted
pass), `ww-standalone-writer-gate` (the gate binary's shape and the
mutation tier), `ww-anchored-hookup` (hookup.py, the CR assert, the marker),
`nifskope-ww-resume-pending` (PENDING.md), `ww-test-harness-add` (the
harness stub's shape, the stack-QFile rule, the 1.5 s wait, the SKIP rule).
`ww-control-calibration`: did not apply, said so.

Should have existed / written now: **the ww-hkx-animation section 13**
(appended): the class database as one lookup, the gate binary and the oracle
as tools, the `layout` command as the way to find a writer rule, the whole-
archive round trip as the regression gate, the bash-comment trap. Two
procedures this lane re-derived that a skill would have saved: (1) reading
a RUN-TIME-BUILT Havok object out of the exe by emulating its dynamic
initializer -- now `tools/hkclassdb_extract.py`'s `emulate_initializer`, and
worth a `reverse-engineering` amendment (the toolchain page says the objects
are empty; it can now say how to get them); (2) the shell-quoting of
`#`-prefixed arguments through `bash -lc`. Declined: the 900-character flag
string (Mistakes 5) -- a shared file, whoever next touches all three copies.

## Build (BUILD11)

**The hook-up needed finishing, twice.** `hookup.py` (check) printed all ten
anchors `count=1` exactly as `PENDING.md` predicted. `--apply` wrote
`NifSkope.pro` and `src/nifskope.h` and then died:

    AssertionError: CR count moved in ...\src\nifskope.cpp

The script's own defect, not the tree's: `src/nifskope.cpp` is mixed, the four
regions it edits are CRLF, and the script converts its inserted text to CRLF --
so the CR count MUST grow by exactly the CRs of that text
(`ww-anchored-hookup` s1), not stay equal. Finished with
`scratchpad/build11_20260910/hookup1_rest.py`, which imports this script's own
`EDITS` table (nothing retyped) and applies only the two files it never wrote:
`src/nifskope.cpp` 440,714 -> 442,322 (+1,608), CR 9,520 -> 9,556 (+36,
expected +36), 6 markers; `src/nifskope_ui.cpp` +119, CR 0, 1 marker.

Then the syntax pass found what no lane could have seen alone: the inserted
code needed INCLUDES that no hook-up carried. `src/nifskope_ui.cpp:24298`
`invalid use of incomplete type 'class HkxModel'` -- the
`WW_ANIMWS_HKXMODEL` branch reads `hkx->undoStack` and `nifskope.h` only
forward-declares the class. Fixed with one line, marked `(lane BUILD11)`.

**The owed QUndoGroup was NOT written, because it already exists.** This
report's section 6 owes "a QUndoGroup so Edit > Undo reaches the .hkx
document". Lane HKXEDIT2's hook-up does exactly that and it is in this build:
`src/nifskope_ui.cpp` now creates the window's actions from
`wwAnimUndoGroup()->createUndoAction( ... )` / `createRedoAction( ... )`
instead of `nif->undoStack`, and the dock block calls
`animws->installUndoGroup( nif->undoStack, hkx->undoStack )`, which adds both
stacks to that one group (`src/animworkspace.cpp:61` and `:207`), the active
one following keyboard focus. A second group would have been a duplicate.

**Gates.**

* `python tests/spells/hkxfile_gates.py`: **109 checks, 0 failures, 37.3 s**,
  re-derived here on the 15,320-file census set (already on disk, nothing
  regenerated). The driver `release/hkxfile_gate.exe` (16:00:55) is newer than
  `src/hkxfile.cpp` (15:48:07) -- the gate-driver rule, lane BUILD8.
* `bash tests/spells/hkxmodel_test.sh`, **first run ever: 3 checks, 1 failure.**

**The one red, with its mechanism, and NOT fixed here.** The harness reports

    ok   the document loaded
    ok   the Block Details view `tree` exists
    FAIL (a) the tree's model is an HkxModel after an .hkx load
    the tree's model is NifModel

The document really does load: the harness's `ok` is the `ok` that
`NifSkope::load()` emits, which is `HkxModel::loadFromFile( fname )`'s return,
and `load()` does run `tree->setModel( hkx )`. What takes it back is the very
next statement, `emit completeLoading( ok, fname )` ->
`NifSkope::onLoadComplete` -> `swapModels()` (`src/nifskope.cpp:7478`).
`swapModels` knows exactly two states:

    if ( tree->model() == nif ) { ... park on nifEmpty ... }
    else                       { ... tree->setModel( nif ) ... }

With the tree on `hkx`, the else branch fires and puts `nif` back. The `.kfm`
route this was modelled on does not hit it because a .kfm has its OWN view,
`kfmtree`, which `swapModels` names explicitly; the .hkx route reuses `tree`,
which `swapModels` owns. The shape of the cure is plain (a third state, or the
route re-asserting itself after `completeLoading`), but it is a behaviour
change in another lane's file and a build lane's product is a verdict, not a
cure (`nifskope-ww-resume-pending` s6). **The refuter, if this is wrong:** the
tree would have been left on `nifEmpty`, not `nif` -- `onLoadBegin` parks it
there -- and the harness printed `NifModel`.

Everything after (a) in that harness is therefore unreached: 6 blocks in file
order, numFrames 23 -> 24 with undo/redo, the byte-identical unedited save, and
`animFile()` decoding 23 frames are all still unproven IN THE APPLICATION. They
are proven outside it by `hkxfile_gates.py`'s 109/0.

### The one table of clocks

| artefact | time |
|---|---|
| `release/NifSkope.exe` (this build) | 2026-09-10 **17:08:39**, 20,693,504 B |
| the exe this replaced (lane BUILD10) | 16:45:53, 20,007,936 B |
| `release/style.qss` (copied at link time) | 17:08:39, equal to `res/style.qss` |
| `release/hkclasses_fo4.json` | 17:08:39, 1,511,662 B |
| `release/hkx_annotation_vocabulary.txt` | 17:08:39, 55,299 B |
| bungo's own window, opened mid-lane | started 17:05:44, pid 700, no `--port` |

qmake ran before make; `DEPCHECK missing=0` over eleven objects; the three WW
defines are each once in `Makefile.Release` and on every compile line; the
exe-newer sweep is 1 stale of 114 paths and the exception is
`src/watermark.cpp`, lane WATER7's live edit in this shared tree, which is NOT
in this exe. `WW_HKXANIM_UI`, `WW_HKXCLIP_CANON` and `WW_ANIMWS_HKXMODEL` are
new or newly-read flags, so the objects of every translation unit that reads
one were deleted before make (the BUILD9 DEFINES trap: make compares mtimes,
not flags).

**Skipped harnesses, named with the reason** (`nifskope-ww-resume-pending` s5):
`loaded_nifs.sh`, `top_bar.sh` and `ui_align.sh` -- this build renamed no
user-visible string and moved no bar, so the sibling-label class of red
(BUILD9 s10) cannot have been introduced; `lodgen_*`, `water_*`, `lodl/lodt`
and the impostor gates -- nothing in these three lanes reaches the generator,
the water tool or the terrain readers. `WW_POSEDRAW_TEST` was left alone: it
was already failing at "clicking a bone did not make it the active object" on
both fixtures BEFORE lane SKELFIX, and SKELOVERLAY's report says one run on the
70-bone facial rig is what settles it.
