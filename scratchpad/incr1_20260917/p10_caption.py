#!/usr/bin/env python
# INCR1 -- picture 2's caption was TYPED, and the first run proved why that is
# a mistake: on a 4-chunk region the one-cell widening reaches every chunk, so
# the census said "4 of 4 dirty, 0 replayed" under a heading that claimed one
# chunk was rebaked and the rest spoke from cache. The picture contradicted its
# own evidence.
#
# The caption now comes OUT OF the census lines it is printed above, so it
# cannot disagree with them.
#
#   python p10_caption.py <pics_compose.py> [--check]
import io
import sys

LF = chr(10)
SP = chr(32)
Q = chr(39)


def ind(n):
    return SP * n


OLD = (
    ind(4) + "d.text((pad, 20), " + Q + "One cell edited, one chunk rebaked" + Q + ", font=fb, fill=TEXT)" + LF
    + ind(4) + "d.text((pad, 54)," + LF
    + ind(11) + Q + "cell (%s) of the plugin was changed; everything else spoke from " + Q + LF
    + ind(11) + Q + "its .lodj cache" + Q + " % cell, font=font(15), fill=DIM)" + LF
)

NEW = (
    ind(4) + "# THE CAPTION IS READ OUT OF THE CENSUS, never typed: a heading that" + LF
    + ind(4) + "# disagrees with the lines underneath it is worse than no picture." + LF
    + ind(4) + "dirty = total = replayed = placements = -1" + LF
    + ind(4) + "for l in lines:" + LF
    + ind(8) + "m = re.search(r" + Q + "incremental: (" + chr(92) + "d+) of (" + chr(92) + "d+) chunks dirty" + Q + ", l)" + LF
    + ind(8) + "if m:" + LF
    + ind(12) + "dirty, total = int(m.group(1)), int(m.group(2))" + LF
    + ind(8) + "m = re.search(r" + Q + "(" + chr(92) + "d+) replayed from cache " + chr(92) + "((" + chr(92) + "d+) placement" + Q + ", l)" + LF
    + ind(8) + "if m:" + LF
    + ind(12) + "replayed, placements = int(m.group(1)), int(m.group(2))" + LF
    + ind(4) + "if replayed > 0:" + LF
    + ind(8) + "head = " + Q + "One cell edited, %d of %d chunks rebaked" + Q + " % (dirty, total)" + LF
    + ind(8) + "sub = (" + Q + "cell (%s) of the plugin changed; the other %d chunk(s) spoke " + Q + LF
    + ind(15) + Q + "from their .lodj cache -- %d placement(s) replayed instead of " + Q + LF
    + ind(15) + Q + "re-derived" + Q + " % (cell, replayed, placements))" + LF
    + ind(4) + "elif dirty >= 0:" + LF
    + ind(8) + "head = " + Q + "One cell edited, the widening reached %d of %d chunks" + Q + " % (dirty, total)" + LF
    + ind(8) + "sub = (" + Q + "cell (%s) changed; on a region this small every chunk is a " + Q + LF
    + ind(15) + Q + "neighbour of it, so nothing was left to replay" + Q + " % cell)" + LF
    + ind(4) + "else:" + LF
    + ind(8) + "head = " + Q + "An incremental run" + Q + LF
    + ind(8) + "sub = " + Q + "cell (%s) of the plugin was changed" + Q + " % cell" + LF
    + ind(4) + "d.text((pad, 20), head, font=fb, fill=TEXT)" + LF
    + ind(4) + "d.text((pad, 54), sub, font=font(15), fill=DIM)" + LF
)


def main():
    p = sys.argv[1]
    raw = io.open(p, "rb").read()
    assert raw.count(chr(13).encode()) == 0, "file has CR, refusing"
    t = raw.decode("utf-8")
    if "THE CAPTION IS READ OUT OF THE CENSUS" in t:
        sys.stdout.write("already patched" + LF)
        return 0
    n = t.count(OLD)
    assert n == 1, "anchor count == %d, want 1" % n
    out = t.replace(OLD, NEW)
    if "import re" not in out:
        a = "import io" + LF
        assert out.count(a) == 1
        out = out.replace(a, a + "import re" + LF)
    assert out.count("import re" + LF) == 1
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
