"""ROADS1's two skill amendments, applied to the REPO tree
(`<repo>/.claude/skills`). The director mirrors them to the live tree
(CONSTITUTION 1a: the two trees drift and nothing syncs them).

  1. nifskope-ww-vanilla-compare  gains step 1a: pick the tile by PROJECTING the
     thing under test onto every candidate, never by inheriting one.
  2. nifskope-ww-lodgen           gains an editing trap: a spell that shells out
     to `python` measures whichever python is on the PATH.
"""

import io

VC = '.claude/skills/nifskope-ww-vanilla-compare/SKILL.md'
LG = '.claude/skills/nifskope-ww-lodgen/SKILL.md'

STEP1A = """## 1a. Pick the tile by PROJECTING the thing under test (2026-09-11, lane ROADS1)

Step 1 says: rank candidates on vanilla's own files. This says the other half:
**when the comparison is about a THING — roads, rubble, a building family, a
water body — rank the candidates by where that thing actually IS, and never
inherit a tile from the lane before you.**

Lane ROADS1's brief pointed at lane TERRAIN-R's picture of chunk (-20,24) and at
its sentence *"most of it is the roads ... vanilla carries and we do not"*.
Chunk (-20,24) contains **zero road triangles**. The Sanctuary loop road is one
chunk SOUTH. Four lines found it, before any measurement was believed:

```python
# every placement of the family, projected onto the dim-4 chunk grid
c = collections.Counter()
for p in placements_of_the_family:
    for s in model_shapes(p):
        v = world_space(s, p)
        for cx in range(floor(v.x.min()/4096), floor(v.x.max()/4096) + 1):
            for cy in range(floor(v.y.min()/4096), floor(v.y.max()/4096) + 1):
                c[((cx // 4) * 4, (cy // 4) * 4)] += len(s.triangles)
# -> (-16,16) 284532   (-20,20) 86770   (-16,24) 63703   (-20,24) 0
```

The bounding box is enough; the point is which chunk, not which texel. Do it
BEFORE choosing the fixture, and print the whole histogram in the report, so the
choice is a measurement and the rejected candidates are visible.

**The failure it prevents is not "a worse tile" — it is a FALSE CAUSE.** A tile
with none of the thing in it will still show a difference against vanilla (ours
did: mean 19.96 of 255), and that difference will be attributed to the thing
because that is what the lane was looking for. Projecting first is what keeps
"most of it is the roads" from becoming a fact by repetition.

"""

TRAP = """* **A spell that shells out to `python` measures whichever `python` is on the
  PATH, and MSYS2's has no `numpy`.** Run the harness chain through
  `MSYSTEM=UCRT64 ... /c/msys64/usr/bin/bash -lc` and `tests/spells/lodl_open.sh`
  reports `23 checks, 1 failures`: the failing line is
  `whole-worldspace render: 89019 bytes, coverage , luminance SD ` with BOTH
  numbers empty, above a `ModuleNotFoundError: No module named 'numpy'` in the
  traceback — which reads exactly like a render regression in the tree. The same
  exe from the Git-Bash shell, whose `python` is
  `/c/Users/bungo/AppData/Local/Programs/Python/Python39/python`, reads
  `coverage 0.0951, luminance SD 38.86` and 23/0. **Build the chain with the
  interpreter named**, or read an empty number as a missing module before
  reading it as a defect (2026-09-11, lane ROADS1).
"""


def main():
    s = io.open(VC, encoding='utf-8', newline='').read()
    a = '## 2. Serve OUR files to the renderer without touching the game data\n'
    assert s.count(a) == 1
    s = s.replace(a, STEP1A + a)
    io.open(VC, 'w', encoding='utf-8', newline='').write(s)

    t = io.open(LG, encoding='utf-8', newline='').read()
    b = '* Python on Windows needs `C:/...` paths, not `/c/...`.\n'
    assert t.count(b) == 1, t.count(b)
    t = t.replace(b, b + TRAP)
    io.open(LG, 'w', encoding='utf-8', newline='').write(t)

    for f in (VC, LG):
        raw = open(f, 'rb').read()
        print('%s  %d bytes  CR %d  LF %d' % (f, len(raw), raw.count(b'\r'),
                                              raw.count(b'\n')))


if __name__ == '__main__':
    main()
