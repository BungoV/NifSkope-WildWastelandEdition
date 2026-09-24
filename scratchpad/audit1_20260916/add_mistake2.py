"""AUDIT1: two more entries at the top of MISTAKES.md, newest first, LF-only."""
import io
import os
import tempfile

p = 'E:/Projects/NifskopeWildWastelandEdition/MISTAKES.md'
s = io.open(p, encoding='utf-8', newline='').read()
crs = s.count('\r')

anchor = '## 2026-09-17 16:27 -- lane AUDIT1, a patch script that truncated its target before it failed'
assert s.count(anchor) == 1, s.count(anchor)

entry = '''## 2026-09-17 17:0x -- lane AUDIT1, a switch that takes a value, spelled without one

- 17:05 -- my step-2 script ran the null incremental as `... --native <dir> --incremental`, with
  `--incremental` as the LAST token. `--incremental` takes a directory (`src/nifcli.cpp:7618`,
  `gLgIncremental = next()`), and `next()` returns an empty string at the end of the vector, so the
  flag parsed, set nothing, and the run full-baked for 44 s while I wrote down that it was an
  incremental run. The census said so in words the whole time --
  `native-library-build: rebuilt (not offered: this is not an incremental bake)` and
  `0 replayed from cache` -- and I read past both because the wall clock was close to the full bake's
  and I had expected a null incremental to be no faster. **The rule: when a run is supposed to take a
  DIFFERENT path, the proof is the line where the exe names the path, not the clock.** The product
  half of this -- a valued switch that silently does nothing when its value is missing -- is a
  confirmed bug in the report, with a gate.

## 2026-09-17 17:3x -- lane AUDIT1, CRLF spliced into an LF-only report

- 17:36 -- I generated the report's section-2 tables with a Python script that `print`s to a
  redirected stdout. On Windows that is text mode, so every line arrived CRLF, and splicing the file
  into `lane_audit1_report.md` (LF-only, measured 0 CR before) left 210 CRs in a document that had
  none. Caught by counting the bytes in the same call that spliced -- the count is the check, not the
  look of the file -- and repaired by rewriting the whole file with the CRs removed. **The rule: a
  generator whose output will be spliced writes with `newline=''` (or its output is normalised at the
  splice), and every splice into a mixed-endings tree prints the CR count before and after.**

'''

s = s.replace(anchor, entry + anchor, 1)
assert s.count('\r') == crs
d = os.path.dirname(p)
f = tempfile.NamedTemporaryFile('w', encoding='utf-8', newline='', dir=d, delete=False, suffix='.tmp')
f.write(s)
f.close()
os.replace(f.name, p)
print('MISTAKES.md: CR %d, LF %d, %d bytes' % (s.count('\r'), s.count('\n'), len(s)))
