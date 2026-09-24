"""Two more MISTAKES.md entries, newest at the top (CONSTITUTION rule 2)."""
import sys

P = 'E:/Projects/NifskopeWildWastelandEdition/scratchpad/land1_20260912/MISTAKES_ENTRIES.md'

ANCHOR = """## 2026-09-12 — A refusal list written from the design map, not from the code, refused the default command"""

NEW = """## 2026-09-12 — The test edit never reached the thing under test, and the arm said PASS

Gate B3's job is to prove a dirty rebake is byte-identical to a full bake. Its
`refs` arm moved a `REFR` in an interior cell and passed. It should not have
counted: **the floor printed beside the verdict was exactly one file,
`Commonwealth.lodb` itself.**

The reference that was moved is not drawn in LOD. Most are not — an empty `MNAM`
slot drops a ref at every ring — so the edit moved the input digest and nothing
else. The input digest is **the thing under test**. An arm whose only witness is
the artefact being tested is not a witness, and this one would have been read as
a pass by anyone looking at the verdict line, which is what a verdict line is
for.

The fix was to stop guessing which reference the bake uses and ask it: the base
bake's own `.BTO.manifest.txt` **is** the list of references that reached a
chunk, so `b_pickref.py` intersects that list with the plugin's REFRs in the
cell, and `b_esmedit.py moveid` moves that form id. Re-run, both `refs` arms move
a real `.BTO` and its manifest — and still come back byte-identical.

**Print the floor, not just the verdict.** `equal` is free for a change that
reached nothing, so every identity check needs a separate assertion that its
input moved the output at all — and that assertion has to be *read*, not merely
computed. Two lines of output caught a vacuous arm in a gate that was otherwise
about to be reported as eight of eight.

---

## 2026-09-12 — Four refusal arms, one refusal, and the gate could not tell

Gate B2 asks `--incremental` to refuse in four different ways. For two runs it
reported four refusals that were all **the same refusal**: first every arm
tripped `a different shape` (its region was left over from when the test regions
were 3x3 chunks), then every arm tripped `the switches differ` (it named the
vanilla plugin path, and the plugin path is part of the switch digest — which is
documented, deliberate, and exactly why gate B3 bakes every variant over one live
path).

The failure count made it look like a broken feature. It was a broken gate. And
the worst row was not a failure: the `switches` arm **passed** on the wrong-shape
message, and would have passed on a build that had no switch digest at all.

Two things fixed it, and the second is the general one:

1. every arm asserts the refusal it got is the refusal it **asked for**, by
   phrase, not merely that some refusal happened;
2. the refusal sentence is printed beside every verdict. Four identical
   sentences under four different arm names is unmissable in a way that
   `5 arms, 3 failures` is not.

A third problem only became visible once those two were in place: the
`whole-region` arm was **unreachable**. `--atlas` is not on the switch-digest
skip list, so asking for it against a ledger baked without it refuses for the
switch reason and the whole-region check is never reached. The arm now bakes a
ledger *with* `--atlas` first — the path a person actually walks.

**A refusal gate whose arms can all be satisfied by the same wrong answer is
barely a gate.** Assert on the reason, not on the rc.

---

"""


def main():
    b = open(P, 'rb').read()
    s = b.decode('utf-8')
    n = s.count(ANCHOR)
    assert n == 1, 'anchor matched %d times' % n
    s = s.replace(ANCHOR, NEW + ANCHOR)
    out = s.encode('utf-8')
    open(P, 'wb').write(out)
    print('MISTAKES_ENTRIES.md %d -> %d bytes, CR %d' % (len(b), len(out), out.count(b'\x0d')))
    return 0


if __name__ == '__main__':
    sys.exit(main())
