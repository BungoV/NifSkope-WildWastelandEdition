#!/usr/bin/env python
"""Amend nifskope-ww-resume-pending SS3 in BOTH skill trees.

They drift and nothing syncs them (CONSTITUTION 1a). The two copies were
byte-identical (7,906 bytes, 2026-09-09 18:51) before this edit, so the same
splice is applied to each and both are re-read to prove they still match.
"""
PATHS = [
    'E:/Projects/Claude/.claude/skills/nifskope-ww-resume-pending/SKILL.md',
    'E:/Projects/NifskopeWildWastelandEdition/.claude/skills/nifskope-ww-resume-pending/SKILL.md',
]

ANCHOR = (b"The `awk` walk is the point: `grep -A3` misses a dependency that sits ten\n"
          b"continuation lines down, and a hand patch that qmake has just thrown away looks\n"
          b"exactly like one that survived unless you name the object it belongs to.\n")

ADD = (b"\n**qmake's scan stops at `src/`** (2026-09-10, lane BUILD4). A new include that\n"
       b"points into a vendored `lib/` tree is STILL absent from the regenerated\n"
       b"dependency block, so re-running qmake does not cure it. `src/lodtfile.cpp`\n"
       b"gained `#include \"esmfile.hpp\"`; after `qmake` its object's block still read\n"
       b"only `src/lodtfile.cpp src/lodtfile.h src/esmdata.h src/io/lodvfile.h`. For a\n"
       b"`lib/` include the object-mtime check from `nifskope-ww-build-verify` is the\n"
       b"only gate -- and if a `lib/` header is itself edited, delete the objects of\n"
       b"every `src/` file that includes it before relinking.\n")

out = []
for p in PATHS:
    b = open(p, 'rb').read()
    assert b.count(b'\r') == 0, '%s is not LF-only' % p
    assert b"qmake's scan stops at" not in b, 'already amended: %s' % p
    assert b.count(ANCHOR) == 1, 'anchor count %d in %s' % (b.count(ANCHOR), p)
    n = b.replace(ANCHOR, ANCHOR + ADD)
    open(p, 'wb').write(n)
    out.append(open(p, 'rb').read())
    print('%-70s %d -> %d bytes' % (p.split('/')[2] + '/...', len(b), len(n)))

assert out[0] == out[1], 'the two trees diverged'
print('both trees byte-identical after the amendment')
