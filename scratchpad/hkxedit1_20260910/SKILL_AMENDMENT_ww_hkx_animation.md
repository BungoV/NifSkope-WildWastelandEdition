
## 13. The generic packfile layer (lane HKXEDIT1, 2026-09-10)

Any .hkx -- clip, skeleton, ragdoll, behaviour graph -- is now a typed object
graph in C++ (`src/hkxfile.{h,cpp}`, `Hkx::File::read` / `write`) and in
Python (`tests/spells/hkxfile_oracle.py`), both driven by
`res/hkclasses_fo4.json`. Contract: `docs/HKX_PACKFILE_MODEL.md`. Do not
re-derive any of this.

* **The class database is the exe's, extracted, not typed.**
  `python tools/hkclassdb_extract.py` (1 s) rebuilds `res/hkclasses_fo4.json`
  from the 1.10.155 exe + symbol TSV: 908 registered classes (+35 variants /
  unregistered), every member's offset / type / subtype / flags / class /
  enum, every enum's items, and the class SIGNATURE computed from
  `hkClass::writeSignature` -- 908/908 equal to HKXPACK's. The hkClass
  OBJECTS are not on disk: the tool emulates each `dynamic initializer for
  'XClass''` with capstone; the member `enum*` slots are zero on disk and
  patched by `... 'XClass_Members''` initializers, emulated too. A class
  layout question is one JSON lookup now, never a fresh `hkclass_reflect.py`
  run; `hkclass_reflect.py`'s plausibility break over-reads by one row (s9).
* **Read / write / edit any file without the exe:** `release/hkxfile_gate.exe
  --db res/hkclasses_fo4.json dump|roundtrip|census|edit|get FILE` (built by
  `scratchpad/hkxedit1_20260910/build_gate.sh`, Qt6Core only, run from the
  MSYS2 shell), and `python tests/spells/hkxfile_oracle.py dump|roundtrip|
  layout|census`. `layout` prints the first chunks where the writer's rule
  and the file disagree -- it is how every layout rule was found; use it
  before touching the writer.
* **The round-trip gate is the whole archive**: `python tests/spells/hkxfile_gates.py`
  (109 checks, 39 s) -- 15,278 of 15,320 files byte-identical by both
  readers, 42 `hclClothSetupContainer` files refused (the exe lacks the
  class). The set lives in `scratchpad/hkxedit1_20260910/census_hkx/`
  (gitignored; `extract_census.py` regenerates it in 13 s). A writer change
  that is not 15,278/15,278 is a regression, whatever its author says.
* **Layout rules settled** (each cost a mismatch class): objects and array
  payloads 16-aligned before; a payload of STRUCTS is not padded after, one
  of POINTERS or plain values is, one of STRING POINTERS is not and its
  strings sit at even offsets with one 16-pad after the run; a direct string
  pads to 16 after itself; hkRelArray payloads live inside the object's
  chunk after the body, 16-aligned; global fixups follow the flush order
  (a pointer inside an earlier member's array precedes a later direct
  pointer member); objects are written depth-first from the root.
* **A shipped file's shell-facing path is a bash comment when it starts with
  `#`.** The gate binary addresses fields as `#2.numFrames`; quote every
  argument you pass through `bash -lc`, or the edit silently applies nothing
  and the "wrote" line reads as success (MISTAKES.md 2026-09-10 entry 3).
* **The Blocks tab:** `HkxModel : BaseModel` (`src/hkxmodel.{h,cpp}`), the
  KfmModel idiom; `animFile()` hands the document's bytes to
  `hkxAnimLoadPackfile` -- the workspace's reader on the saved bytes, no
  second decoder. Hook-up NOT applied at the time of writing
  (`scratchpad/hkxedit1_20260910/hookup.py`, `PENDING.md`).
