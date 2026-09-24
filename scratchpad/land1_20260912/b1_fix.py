"""Two corrections to report section B1, marked as corrections rather than made
quietly.  B1 was written BEFORE the code, which was right; two of its claims did
not survive contact with the code and with the gate, and a report that edited
them out would be claiming a foresight this lane did not have.
"""
import sys

REP = 'E:/Projects/NifskopeWildWastelandEdition/scratchpad/lane_land1_report.md'

OLD_LIST = """3. **a whole-region consumer is requested** (`--atlas`, `--arrays`, the merge,
   `--impostors`) -- a filtered `writtenBto` would corrupt it;"""

NEW_LIST = """3. **a whole-region consumer is requested** (`--atlas`, `--arrays`, the merge,
   `--impostors`) -- a filtered `writtenBto` would corrupt it;"""

OLD_FLOOR = """**If the ledger's digests are computed from anything the bake does not actually
read, or miss anything it does, the byte-identity gate passes by luck on the
edits I happened to choose and fails on bungo's.** The guard is that the gate's
four edit kinds are chosen to hit four DIFFERENT rows of the B1.3 table --
height (`cells`), a moved object (`refs`), a mesh edited in place (`assets`), and
a switch change (`switches`) -- and that a FLOOR arm must fire: with the AO reach
deliberately halved, the dirty set is too small and the gate must go RED. A gate
that cannot fail is not a gate."""

NEW_FLOOR = """**If the ledger's digests are computed from anything the bake does not actually
read, or miss anything it does, the byte-identity gate passes by luck on the
edits I happened to choose and fails on bungo's.** The guard is that the gate's
edit kinds are chosen to hit DIFFERENT rows of the B1.3 table -- a height
(`cells`), a moved object (`refs`), an edit on the region's own border, and a
deleted output -- and that every arm carries a FLOOR: `incr == full` is
trivially true for an edit that reached nothing, so each arm separately asserts
that the edit moved the full bake's bytes at all, and an arm whose floor is
empty is printed VACUOUS, never PASS. A gate that cannot fail is not a gate.

> **What this section claimed and did not deliver, stated plainly.** B1 promised
> a floor arm built by halving the AO reach in the digest and watching the gate
> go red. That arm was NOT built: it needs a second exe compiled with a
> deliberately wrong constant, and this lane had one build slot for Part B. The
> refuter is therefore **only partly retired**. What stands in its place is
> weaker and is not pretended otherwise: the per-arm floor above (which proves
> each edit reached the output), the census line beside every verdict (which
> proves the incremental run did not secretly do a full bake), and the fact that
> the gate caught two real defects this lane would otherwise have shipped -- see
> B3. **The `assets` row of the dependency map has no gate arm at all** and is
> listed under "what was not measured".

### B1.6 CORRECTION -- two claims above did not survive the code

B1 was written before the implementation, deliberately. Two of its statements
are wrong and are corrected here rather than edited out of the text above.

1. **The refusal list in B1.4 names the merge. The merge is not refused.**
   `lodgenMergeChunkShapes` and `lodgenSimplifyFarRings` are both
   `for ( path : btoPaths )` loops that open one `.BTO`, rewrite it and save it
   with no state carried between files, so a filtered list gives each rebaked
   chunk exactly the treatment a full run would. The merge is **on by default**,
   so shipping that list would have made `--incremental` refuse every command
   anybody would ever type -- and it did, until gate B3's first arm failed. Only
   `--atlas`, `--arrays` and `--impostors` are refused. Full write-up in
   `MISTAKES_ENTRIES.md`.

2. **B1.4 item 4 calls a missing output a refusal. It is not.** A file the
   ledger claims that is absent or edited marks that **chunk** dirty and it
   rebakes; the run proceeds. Refusing there would mean a user who deleted one
   `.BTO` could never use the fast path again. Gate arm `A/lost` measures
   exactly this: one deleted `.BTO`, `1 output lost, 5 by neighbour`, 6 of 25
   dirty, and the file comes back byte for byte."""


def main():
    b = open(REP, 'rb').read()
    assert b.count(b'\r') == 0, 'report is not LF-only'
    s = b.decode('utf-8')
    assert s.count(OLD_FLOOR) == 1, 'floor paragraph matched %d' % s.count(OLD_FLOOR)
    s = s.replace(OLD_FLOOR, NEW_FLOOR)
    out = s.encode('utf-8')
    assert out.count(b'\r') == 0
    open(REP, 'wb').write(out)
    print('report %d -> %d bytes, CR 0' % (len(b), len(out)))
    return 0


if __name__ == '__main__':
    sys.exit(main())
