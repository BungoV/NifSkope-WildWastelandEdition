ENTRY = """
10. **I appended a document bullet through `python -c "..."` in a DOUBLE-QUOTED
   bash string and every backticked code span was deleted.** The shell ran each
   span as command substitution before python saw the text, so
   `docs/LODGEN_BTD_FORMAT.md` gained a bullet reading "the sheet's is an ." and
   "Shader Flags 1 , which is  set with  clear", while the wrapper printed a
   plausible byte count. Found by reading the inserted lines back. **This exact
   entry is already in this ledger, dated 2026-09-11 (lane UI5-HOVERPIC), and I
   had not read that far down the file.** Repaired with a patch script written
   through the Write tool. THE RULE, again: text carrying backticks, quotes or
   `$` goes into a `.py` file written with the Write tool, never into a
   double-quoted `python -c`; and the inserted lines are read back before the
   edit is believed.
"""

import io

ROOT = 'E:/Projects/NifskopeWildWastelandEdition/'
LANE = ROOT + 'scratchpad/nativeview1_20260912/'

# 1) the lane's own copy
p = LANE + 'MISTAKES_ENTRIES.md'
d = open(p, 'rb').read().decode('utf-8')
assert ENTRY not in d
d = d.rstrip('\n') + '\n' + ENTRY
open(p, 'wb').write(d.encode('utf-8'))
print('MISTAKES_ENTRIES.md', len(d))

# 2) the root ledger, inside this lane's section
p2 = ROOT + 'MISTAKES.md'
s = open(p2, 'rb').read().decode('utf-8')
assert ENTRY not in s
anchor = '## 2026-09-12 -- lane NATIVEVIEW1 (the viewer opens the native bake)'
i = s.index(anchor)
j = s.index('\n## ', i + 1)
assert s[:j].rstrip().endswith('what the texture slots seem to need.'), s[j - 80:j]
s = s[:j].rstrip('\n') + '\n' + ENTRY + s[j:]
open(p2, 'wb').write(s.encode('utf-8'))
print('MISTAKES.md', len(s))

# 3) the report's one-line list
p3 = LANE + 'lane_nativeview1_report.md'
r = open(p3, 'rb').read().decode('utf-8')
old = """Nine, written to `MISTAKES.md` at the top the moment each was recognised and"""
new = """Ten, written to `MISTAKES.md` at the top the moment each was recognised and"""
assert r.count(old) == 1
r = r.replace(old, new)
old2 = """9. I built a shader block pointing at an `_msn` without declaring it model-space,
   so the lit land came back 40 percent too dark — the flags were in the bake's
   own `.BTR` the whole time.
"""
new2 = old2 + """10. a document append through `python -c` in a double-quoted bash string lost
   every backticked code span to command substitution — an entry this ledger
   already carried from 2026-09-11, which I had not read that far to find.
"""
assert r.count(old2) == 1
r = r.replace(old2, new2)
open(p3, 'wb').write(r.encode('utf-8'))
print('report', len(r))
