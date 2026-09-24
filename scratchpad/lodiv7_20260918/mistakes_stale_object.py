ENTRY = """## 2026-09-18 19:0x -- a green gate on a binary that was half a build old

- 2026-09-18 10:38 (lane LODIV7) -- the lane inserted `Placement` into the MIDDLE
  of `enum class LodlChannel` (src/lodinative.h), which renumbers every value
  after it, and rebuilt. G1..G4 passed on the exe that came out. The pictures and
  the whole viewer half of the lane were about to be taken with it.
- G5 then ran the neighbours and `lodl_channels.sh` came back **11 failures
  against a standing 48/0**: `mask-r` drew the mask sheet's **G**, `mask-g` drew
  its **B**, `mask-b` drew the alpha (reported ABSENT), `emissive` drew the
  **role-2** model-space-normal sheet, and `normal` drew nothing. A clean +1
  shift, exactly one enumerator wide.
- **The story that fits and is wrong:** "inserting into the enum broke the name
  table". The table is `{ name, LodlChannel }` pairs looked up by STRING
  (src/lodinative.cpp) and it is right; `btdterrain.cpp` switches on the
  enumerators BY NAME and it is right too. Source cannot produce this shift.
- **What produced it, measured, two commands:**
  * `tests/spells/lodl_channels.sh` re-run with `EXE=release/NifSkope.before_lodiv7.exe`
    -- the pre-lane exe -- passed every one of those checks. So the shift lives in
    the binary, not in the fixture and not in the harness.
  * `ls -l GeneratedFiles/.obj/btdterrain.o src/lodinative.h` --
    **`btdterrain.o` 08:40, `src/lodinative.h` 10:16**. The shipped exe linked a
    translation unit compiled against the OLD enum. The old code compared against
    the old ordinals while the new `lodlChannelFromEnv` returned the new ones, so
    every channel from `mask-r` on was read one late. `find src -newer release/NifSkope.exe`
    was empty and said nothing about it: the SOURCE was older than the exe; the
    OBJECT was not.
- The repair was `touch src/btdterrain.cpp` and `mingw32-make -f Makefile.Release -j8`.
  `lodl_channels.sh` then returned **54 checks, 0 failures**, and the `ao` picture's
  third note line reads "terrain AO from the MASK SHEET'S B" again.
- The rules:
  * **Inserting a value into the middle of an enum in a shared header is an ABI
    change to every translation unit that includes it. Do not trust the
    incremental build with it.** Rebuild the includers explicitly, or add the new
    value at the END where nothing renumbers. `grep -rl <header> src/*.cpp` then
    compare each `.o` mtime against the header's is a ten-second check and it is
    the one that found this.
  * **A gate that only exercises the files you edited cannot see this class of
    defect.** G1..G4 were all green on the wrong binary because the lane's own
    translation units WERE fresh. The neighbour harnesses -- the ones with a
    standing count, covering code the lane did not touch -- are what caught it.
    **Run them before believing a gate.**
  * **`find src -newer <exe>` is not a staleness check.** It compares sources to
    the exe and passes exactly when every source is older, which is the state a
    stale object also produces. Compare OBJECTS to HEADERS.

"""

p = 'MISTAKES.md'
s = open(p, encoding='utf-8', newline='').read()
HEAD = "Newest at the top.\n\n"
assert s.count(HEAD) == 1
s = s.replace(HEAD, HEAD + ENTRY)
open(p, 'w', encoding='utf-8', newline='').write(s)
d = open(p, 'rb').read()
print('MISTAKES entry written; CRLF %d; bytes %d' % (d.count(b'\r\n'), len(d)))
