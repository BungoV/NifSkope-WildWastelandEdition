## 2026-09-10 — qmake's dependency scan does not follow into `lib/`, so a new include there is invisible to `make` even after a qmake run (lane BUILD4)

**What was done.** Lane WATER2 added `#include "esmfile.hpp"` to
`src/lodtfile.cpp`. Following `nifskope-ww-resume-pending` §3, lane BUILD4 ran
`qmake NifSkope.pro` before `make -j2` precisely so that new cross-include would
be picked up, then read the regenerated dependency back per object as the skill's
`awk` walk instructs.

**What was true instead.** Even AFTER the qmake run, `Makefile.Release`'s block
for `lodtfile.o` names only

```
GeneratedFiles/.obj/lodtfile.o: src/lodtfile.cpp src/lodtfile.h \
    src/esmdata.h src/io/lodvfile.h
```

`lib/libfo76utils/src/esmfile.hpp` is absent. The only two places the file is
named in the whole makefile are the project-wide HEADERS list and
`esmfile.o`'s own rule. qmake's scanner walks `src/` but not the vendored
`lib/` trees, so **re-running qmake is not a cure for a new include that points
into `lib/`** — which is the one thing the skill's rule promised it would fix.

**How it was found.** The skill's own read-back step, run as written rather than
assumed to have worked. Harmless on the night: `lodtfile.o` is 01:01:01 and
`esmfile.hpp` is 2026-08-31, so the object is newer than the header and the link
is consistent. It is latent, not live: the next edit to `esmfile.hpp` will leave
`lodtfile.o` stale and `make` will say nothing, which is exactly the
2026-09-09 `btdterrain.o` failure (a segfault with no output in a path whose own
source did not change).

**The rule.** After adding an include that points into `lib/`, the object-level
check from `nifskope-ww-build-verify` ("A successful build is not a consistent
one") is the ONLY gate — a qmake run does not stand in for it. Check the object
against the header by mtime, and if a `lib/` header is edited, delete the objects
of every `src/` file that includes it before relinking. The
`nifskope-ww-resume-pending` §3 text has been amended to say that qmake's scan
stops at `src/`.
