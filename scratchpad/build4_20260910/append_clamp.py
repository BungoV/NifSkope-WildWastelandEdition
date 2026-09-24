#!/usr/bin/env python
"""Append lane BUILD4's build section to lane CLAMP's report.

Append-only (CONSTITUTION 8: never rewrite a document a lane is writing into).
LF-only file, CR asserted at 0 on both sides.
"""
import io, os

P = 'E:/Projects/NifskopeWildWastelandEdition/scratchpad/lane_clamp_report.md'
SEC = 'E:/Projects/NifskopeWildWastelandEdition/scratchpad/build4_20260910/clamp_build_section.md'

before = open(P, 'rb').read()
add = open(SEC, 'rb').read()
assert before.count(b'\r') == 0, 'report was LF-only, is not now'
assert add.count(b'\r') == 0, 'the section must be LF-only'
assert b'## Build (BUILD4)' not in before, 'already appended'
assert before.endswith(b'\n')

out = before + b'\n' + add
open(P, 'wb').write(out)

after = open(P, 'rb').read()
assert after.count(b'\r') == 0
assert after.startswith(before), 'the append moved existing bytes'
assert len(after) == len(before) + 1 + len(add)
print('lane_clamp_report.md  %d -> %d bytes, CR %d, LF %d -> %d'
      % (len(before), len(after), after.count(b'\r'),
         before.count(b'\n'), after.count(b'\n')))
