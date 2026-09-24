#!/usr/bin/env python
# INCR1 -- leg (h)'s floor tolerated a hard-coded list of census prefixes.
#
# cmd_identical()'s third sub-check asks that every RAW differing line be one
# the mask covers, but it decided that by prefix: a `census` line was allowed
# to differ only when its last field started with `stage times:`.  The FIFTH
# volatile thing -- the `peak working set:` clause of the `bake census:` line
# -- is masked by lodb_read.normalise() and by lodbNormalise(), which is why
# the FIRST sub-check on the same pair says the normalised records are
# byte-identical, and the third one failed anyway.
#
# The fix asks the mask instead of guessing: a census line is a real
# non-determinism only when normalising THAT LINE still leaves the two sides
# different.  Add a sixth volatile thing tomorrow and this floor follows it;
# break the mask and the floor still goes red, which is the point of it.
#
#   python p9_bakerec_legh.py <tests/spells/lodgen_bakerec_gate.py> [--check]
import io
import sys

LF = chr(10)
SP = chr(32)


def ind(n):
    return SP * n


OLD = (
    ind(4) + "# A `census` line may differ ONLY when it is the stage-times wall clock;" + LF
    + ind(4) + "# any other census line differing is a real non-determinism." + LF
    + ind(4) + "census_bad = [i for i in diff if ra[i].split(TAB)[0] == 'census'" + LF
    + ind(18) + "and not ra[i].split(TAB)[-1].startswith('stage times:')]" + LF
    + ind(4) + "ck.check('and they differ only on baked / resource / plugin / stage-times lines'," + LF
    + ind(13) + "all(k in ('baked', 'resource', 'plugin', 'census') for k in kinds)" + LF
    + ind(13) + "and not census_bad," + LF
    + ind(13) + "'kinds that differ: %s%s' % (', '.join(kinds) or 'none'," + LF
    + ind(42) + "'; census lines that are not stage times: %d'" + LF
    + ind(42) + "% len(census_bad) if census_bad else ''))" + LF
)

NEW = (
    ind(4) + "# A `census` line may differ only where the MASK covers it -- the" + LF
    + ind(4) + "# `stage times:` wall clock and the `peak working set:` clause of the" + LF
    + ind(4) + "# `bake census:` line (the fifth volatile thing, lane INCR1). Asking" + LF
    + ind(4) + "# normalise() about the ONE line beats a hard-coded prefix: it follows" + LF
    + ind(4) + "# the mask when the mask grows, and it still goes red for a census line" + LF
    + ind(4) + "# the mask does NOT cover, which is the whole floor." + LF
    + ind(4) + "census_bad = [i for i in diff if ra[i].split(TAB)[0] == 'census'" + LF
    + ind(18) + "and lodb_read.normalise(ra[i]) != lodb_read.normalise(rb[i])]" + LF
    + ind(4) + "ck.check('and they differ only on baked / resource / plugin / masked-census lines'," + LF
    + ind(13) + "all(k in ('baked', 'resource', 'plugin', 'census') for k in kinds)" + LF
    + ind(13) + "and not census_bad," + LF
    + ind(13) + "'kinds that differ: %s%s' % (', '.join(kinds) or 'none'," + LF
    + ind(42) + "'; census lines the mask does not cover: %d'" + LF
    + ind(42) + "% len(census_bad) if census_bad else ''))" + LF
)


def main():
    p = sys.argv[1]
    raw = io.open(p, "rb").read()
    assert raw.count(chr(13).encode()) == 0, "file has CR, refusing"
    t = raw.decode("utf-8")
    if "the mask does not cover" in t:
        sys.stdout.write("already patched" + LF)
        return 0
    n = t.count(OLD)
    assert n == 1, "anchor count == %d, want 1" % n
    out = t.replace(OLD, NEW)
    if "--check" in sys.argv:
        sys.stdout.write("anchor ok, delta %d bytes%s"
                         % (len(out.encode("utf-8")) - len(raw), LF))
        return 0
    io.open(p, "wb").write(out.encode("utf-8"))
    b = io.open(p, "rb").read()
    sys.stdout.write("wrote %d bytes, CR %d, LF %d%s"
                     % (len(b), b.count(chr(13).encode()), b.count(LF.encode()), LF))
    return 0


if __name__ == "__main__":
    sys.exit(main())
