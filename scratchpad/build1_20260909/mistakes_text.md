## 2026-09-09 -- a stale object file put a crashing reader in the shipped exe

- **Lane BUILD1 built the NOPROMPT and TERRAINFIX sources together, watched
  `make` exit 0 and the exe come out newer than all five changed sources, and
  called the build good. It was carrying one translation unit compiled two
  hours earlier against a different class layout.** `src/btdterrain.cpp`
  includes `lodtfile.h` and puts a `LodtFile` on the stack in
  `lodtReadWorldInfo`; TERRAINFIX added three members to that class for header
  version 2, so the object grew, and `LodtFile::open()` -- rebuilt, in
  `lodtfile.o` -- wrote past the end of the caller's smaller stack object.
  Every `-no-gui lodt` run segfaulted (rc 139) on version 1 and version 2 files
  alike. The cause was qmake's generated dependency list: `Makefile.Release`
  names `src/lodtfile.h` for `nifcli.o`, `lodgenmanager.o` and `lodtfile.o` and
  NOT for `btdterrain.o`, because the Makefile was generated before
  btdterrain.cpp began including it, so make had no reason to rebuild it
  (`btdterrain.o` 15:30 against every other object at 17:09).
  Found by `lodt_open.sh`, which was in the gate list only because the header
  changed -- the four gates run before it never touch that reader and were all
  green. Fixed by dropping the object and relinking; the missing dependency was
  added to `Makefile.Release` by hand as a stopgap, and a qmake re-run is owed
  before that file is regenerated.
  Rule: `make` exiting 0 and an exe newer than the sources do NOT prove the
  build is consistent. When a HEADER gains or loses a member, list every
  translation unit that includes it and check each object's mtime against the
  header's; a `.o` older than a header it includes is a stale build, whatever
  make says.

## 2026-09-09 -- three harnesses were shipped as gates without ever being run once

- **Lanes NOPROMPT and TERRAINFIX both ended BUILD PENDING with new harnesses
  described as "written and `bash -n` clean". None of the three new sections
  could pass on its first real run.** `render_shot.sh` built its dirty fixture
  with `-no-gui set -f Name -v <text>`, which the CLI refuses -- a block's Name
  is a `tStringIndex` and `NifValue::setFromString` parses that as a NUMBER --
  and read the name back with `get -f Name`, which prints the index, not the
  string; the fixture step died at "could not rename the shape".
  `lodgen_terrain.sh` rung 4 copied rung 3's `--terrain-region -20 24 -19 25`,
  a quarter of the dim-4 chunk it then asked the pyramid for, so the pyramid
  had one tile row, wrote no sheet, and the check read a missing file.
  `lodt_write.sh` asserted the version-1 fallback file was byte-identical to
  the version-2 one past the header, which the format forbids: the block
  directory stores each payload's offset ABSOLUTE, so all 48,960 of them move
  by the same eight bytes (50,657 differing bytes, every one inside the
  directory).
  Rule: `bash -n` is a syntax check, not a run, and a harness that has never
  executed once is not a gate -- say "written, never run" and never "the gate
  is". And an invariant is derived from the contract document (here
  `docs/LODGEN_BTD_FORMAT.md`, "Block directory": uint64 offset, absolute),
  not from the shape the fix was hoped to have.
