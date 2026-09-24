#!/usr/bin/env python
"""Append lane BUILD4's build section to lane CARDORTHO's report. Append-only."""
P = 'E:/Projects/NifskopeWildWastelandEdition/scratchpad/lane_cardortho_report.md'
SEC = 'E:/Projects/NifskopeWildWastelandEdition/scratchpad/build4_20260910/cardortho_build_section.md'

before = open(P, 'rb').read()
add = open(SEC, 'rb').read()
assert before.count(b'\r') == 0 and add.count(b'\r') == 0, 'both must be LF-only'
assert b'## Build (BUILD4)' not in before, 'already appended'
assert before.endswith(b'\n')

out = before + b'\n---\n\n' + add
open(P, 'wb').write(out)

after = open(P, 'rb').read()
assert after.count(b'\r') == 0
assert after.startswith(before), 'the append moved existing bytes'
print('lane_cardortho_report.md  %d -> %d bytes, CR %d, LF %d -> %d'
      % (len(before), len(after), after.count(b'\r'),
         before.count(b'\n'), after.count(b'\n')))
