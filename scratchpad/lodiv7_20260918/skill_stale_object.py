p = '.claude/skills/nifskope-ww-build-verify/SKILL.md'
s = open(p, encoding='utf-8', newline='').read()

ANCHOR = "  names them in one line.\n\n## Whose NifSkope is that? (2026-09-09, lane OFFSCREEN)"
assert s.count(ANCHOR) == 1

NEW = """  names them in one line.
* **`test exe -nt source` cannot see a STALE OBJECT, and one cost a lane a green gate on the
  wrong binary** (2026-09-18, lane LODIV7, root `MISTAKES.md`). That gate compares SOURCES to the
  exe and passes exactly when every source is older -- which is also true when an object file was
  never rebuilt. A lane inserted a value into the MIDDLE of `enum class LodlChannel`
  (`src/lodinative.h`), which renumbers every value after it; `GeneratedFiles/.obj/btdterrain.o`
  was not recompiled, so that translation unit compared against the OLD ordinals while the freshly
  built lookup returned the new ones, and the terrain viewer read every channel from `mask-r` on
  ONE LATE: `mask-r` drew the sheet's G, `emissive` drew the role-2 normal sheet, `normal` drew
  nothing. G1..G4 all passed, because the lane's own translation units WERE fresh.

  The check, ten seconds, after any edit to a header other files include:

  ```bash
  for f in $(grep -rl "<header>.h" src/ | grep '\\.cpp$'); do
      b=$(basename "$f" .cpp)
      ls -l --time-style=+%H:%M "GeneratedFiles/.obj/$b.o" 2>/dev/null
  done
  ls -l --time-style=+%H:%M src/<header>.h
  ```

  Any `.o` older than the header is stale: `touch` its `.cpp` and rebuild. **Adding an enumerator
  at the END renumbers nothing and avoids the whole class.** And the thing that CAUGHT it was a
  neighbour harness with a standing count over code the lane never touched -- run those before
  believing a gate that only exercises the files you edited.

## Whose NifSkope is that? (2026-09-09, lane OFFSCREEN)"""

s = s.replace(ANCHOR, NEW)
open(p, 'w', encoding='utf-8', newline='').write(s)
d = open(p, 'rb').read()
print('skill updated; CRLF %d; bytes %d' % (d.count(b'\r\n'), len(d)))
