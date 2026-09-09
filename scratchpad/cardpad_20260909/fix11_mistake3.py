#!/usr/bin/env python3
"""CARDPAD -- the third entry: a shell script was edited while it was running."""

P = 'MISTAKES.md'
b = open(P, 'rb').read()
cr0 = b.count(b'\r')
s = b.decode('utf-8')

ANCHOR = "## 2026-09-09 -- lane CARDPAD changed a metric and kept its old control"
assert s.count(ANCHOR) == 1

NEW = """## 2026-09-09 -- lane CARDPAD edited a shell script while that script was running

- **What was done:** `tools/bake_impostor_cards.sh` was patched (a comment block
  describing the spacing law) at 22:11:57 while the same script was mid-run, baking
  tree 14 of 19 in a job started at 22:05.
- **What was true instead:** bash reads a script by BYTE OFFSET as it executes. An
  edit that changes the file's length under a running shell can make it resume at
  the wrong offset and execute half a line. This one was 259 bytes added to a
  header comment the shell had consumed ten minutes earlier, so it was harmless --
  but that was luck, not a check: nothing in the edit knew where the shell was.
- **How it was found:** by reading the mtime table at the end of the lane and seeing
  the driver newer than the exe, then comparing it against the bake's start time.
  The bake did complete: 19 of 19 sets, no zero-length files, no failure lines.
- **The rule:** a file that a running job is EXECUTING is not editable, comments
  included. Before patching anything under `tools/` or `tests/`, check no job is
  running it -- the same check the build chain already makes for the exe -- and if
  one is, queue the edit until it lands.

"""

s = s.replace(ANCHOR, NEW + ANCHOR)
nb = s.encode('utf-8')
assert nb.count(b'\r') == cr0
open(P, 'wb').write(nb)
print('MISTAKES.md: %d -> %d bytes' % (len(b), len(nb)))
