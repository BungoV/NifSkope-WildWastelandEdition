#!/usr/bin/env python
"""BUILD1, finished-work skill review (CONSTITUTION 1a): add the
build-consistency section to nifskope-ww-build-verify in the LIVE skill tree.

The chain in that skill proves make succeeded and the exe is newer than the
sources. It does not prove the build is CONSISTENT, and on 2026-09-09 that gap
shipped a crashing exe: qmake's dependency list for btdterrain.o never named
src/lodtfile.h, so a class that grew three members left one translation unit
believing in the old layout.

The repo tree <repo>/.claude/skills holds only ww-control-calibration, so there
is no second copy of this file to keep in step (verified 2026-09-09).

LF-only file; the CR count must stay 0.
"""
import os

P = r'E:\Projects\Claude\.claude\skills\nifskope-ww-build-verify\SKILL.md'
b = open(P, 'rb').read()
assert b.count(b'\r') == 0, b.count(b'\r')

SECTION = b"""## A successful build is not a consistent one (2026-09-09)

`make` exiting 0 and `test exe -nt source` passing both held while the linked
exe carried a translation unit two hours stale, compiled against a class that
had since grown three members. Every `-no-gui lodt` run segfaulted; four gates
run before it were green because none of them reached that reader.

The cause is that **qmake's dependency lists are frozen when the Makefile is
generated.** `Makefile.Release` named `src/lodtfile.h` for `nifcli.o`,
`lodgenmanager.o` and `lodtfile.o` and not for `btdterrain.o`, because
`btdterrain.cpp` began including it after the last qmake run -- so make had no
reason to rebuild it, and said nothing.

After ANY change to a header, before running a gate:

```bash
H=src/<the header>.h
grep -rln "#include \\"$(basename $H)\\"" src/          # every TU that includes it
for f in <those>; do o=GeneratedFiles/.obj/$(basename $f .cpp).o; \\
  [ "$o" -nt "$H" ] && echo "ok   $o" || echo "STALE $o"; done
```

A `.o` older than a header it includes is a stale build, whatever make says.
Delete that object and re-link (`rm GeneratedFiles/.obj/<name>.o && make -j2`).
If the include is NEW, re-run qmake as well, or add the dependency to
`Makefile.Release` by hand as a stopgap and say that the qmake run is owed --
the file is generated and the hand edit does not survive.

The class of symptom to expect: a segfault with no output, in a path whose own
source did not change, on inputs that worked yesterday -- including files
written in the format's OLD version, because the corruption is the caller's
stack object, not the data.

"""

ANCHOR = b'## The verdict is a number, then a picture'
assert b.count(ANCHOR) == 1, b.count(ANCHOR)
b = b.replace(ANCHOR, SECTION + ANCHOR)
assert b.count(b'\r') == 0
open(P, 'wb').write(b)
print('patched %s, %d bytes, CR %d' % (P, len(b), b.count(b'\r')))
