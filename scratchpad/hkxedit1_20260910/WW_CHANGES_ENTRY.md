## 2026-09-10 -- lane HKXEDIT1: an .hkx as an editable block tree (the raw layer)

bungo, verbatim: "Just make hkx fully editable in our nifskope". This lane is
the generic packfile layer under that: every Havok 2014 packfile the game's
own class registry describes -- clips, skeletons, ragdolls, behaviour graphs
-- read into typed objects, shown as blocks with editable fields, and written
back byte-identically when nothing changed. Contract:
`docs/HKX_PACKFILE_MODEL.md`. Report: `scratchpad/lane_hkxedit1_report.md`.

**The class database** (`res/hkclasses_fo4.json`, by `tools/hkclassdb_extract.py`)
is the FO4 1.10.155 exe's own hkClass reflection: the 908 registered classes'
run-time-built hkClass objects recovered by emulating their dynamic
initializers (capstone), the member arrays read raw, the enum pointers
recovered from a second set of initializers, and every class's SIGNATURE
computed from `hkClass::writeSignature`'s disassembly -- equal to HKXPACK's
stored value on 908 of 908 classes both know, which proves every field that
enters the hash. HKXPACK is the second oracle: 906 classes agree in every
field, 2 differ (HKXPACK's extra `padding` member on two cloth structs; the
exe wins), 35 exe-only, 0 HKXPACK-only, 0 self-check problems.

**The model** (`src/hkxfile.{h,cpp}`, QtCore only): no per-class code; hkArray,
hkRelArray, strings, object pointers, enums/flags, inline structs and C
arrays are the only branches; struct holes kept verbatim, every serialised
field re-encoded. The writer's layout rules were read off the files
themselves (struct payloads unpadded, pointer and plain payloads padded,
string-array strings packed at even offsets then padded once, direct strings
padded after, global fixups in flush order, hkRelArray payloads inside the
object's chunk).

**Round trip:** all 15,320 `.hkx` in `Fallout4 - Animations.ba2`: 15,278
parsed and 15,278 byte-identical through BOTH the C++ model (3.0 s) and the
independent Python oracle (`tests/spells/hkxfile_oracle.py`, 29.8 s); 42
refused by name (`hclClothSetupContainer`, a class the exe does not
register). 105 distinct classes seen.

**The Blocks-tab model** (`src/hkxmodel.{h,cpp}`): `HkxModel : BaseModel`, the
KfmModel idiom -- objects as blocks, members as rows, arrays, compounds,
enums and flags through the NifDelegate's own editors, pointers as links,
undo/redo on its own stack, save through the writer; `animFile()` hands the
document's bytes to the same reader the workspace uses.

**Gates** (`tests/spells/hkxfile_gates.py`, 109 checks, 0 failures, 38.9 s):
(a) the census above by both readers, (b) five edits on a copy of jog.hkx
(int, float, string, enum, array length) by both writers -> identical bytes,
exactly the five fields changed on reload, (c) HKXPACK unpacks every file we
wrote and shows the edits, (d) the database's counts, the eleven animation
classes' measured layouts, the twelve shipped signatures, (e) 20 corruptions
refused by name by both readers. Standalone binary `release/hkxfile_gate.exe`.

**NOT built into NifSkope.exe, NOT flown:** the hook-up
(`scratchpad/hkxedit1_20260910/hookup.py`, --check 10/10 anchors, not
applied: NifSkope.pro, nifskope.h, nifskope.cpp, nifskope_ui.cpp are other
lanes' files) and the WW_HKXMODEL_TEST harness stub
(`src/hkxmodeltest.cpp`, `tests/spells/hkxmodel_test.sh`) wait for the
resume (`PENDING.md`). Owed: a QUndoGroup so Edit > Undo reaches the .hkx
document; a null-vs-"" string edit from a cell; the blob row's editing
(HKXEDIT2's layer).
