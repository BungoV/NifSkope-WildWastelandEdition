"""Director splice for lane WATER8 / WATER8-GATE (CONSTITUTION 8): the lane's
entry text goes to the TOP of WW_CHANGES.md, its mistakes to the END of
MISTAKES.md, its handoff block INTO the HANDOFF.md lane list, each by bytes,
each with the CR count asserted unchanged (WW_CHANGES.md is mixed, 19,020 CR;
the others are LF-only, 0 CR).  Idempotent: refuses if a marker is present."""
import sys
R = "E:/Projects/NifskopeWildWastelandEdition/"
S = R + "scratchpad/water8_20260910/"

def strip_comment(b):
    # the lane's files open with an HTML comment for the director; drop it
    if b.startswith(b"<!--"):
        end = b.index(b"-->") + 3
        b = b[end:].lstrip(b"\r\n")
    return b

def rd(p):
    with open(p, "rb") as f:
        return f.read()

def wr(p, b):
    with open(p, "wb") as f:
        f.write(b)

# 1. WW_CHANGES.md -- entry at the top, below the title line
ww = rd(R + "WW_CHANGES.md")
entry = strip_comment(rd(S + "WW_CHANGES_ENTRY.md"))
assert b"\r" not in entry, "entry carries CR"
marker = b"### The Water tool moves into the LOD Generation panel, on the right"
assert entry.startswith(marker), entry[:80]
title = b"# NifSkope \xe2\x80\x94 Wild Wasteland Edition: Change Log\n\n"
assert ww.startswith(title), ww[:60]
if ww.count(marker) == 0:
    cr0 = ww.count(b"\r")
    new = title + entry.rstrip(b"\n") + b"\n\n" + ww[len(title):]
    assert new.count(b"\r") == cr0 == 19020, (new.count(b"\r"), cr0)
    wr(R + "WW_CHANGES.md", new)
    print("WW_CHANGES.md: entry spliced at top, CR", cr0, "bytes +", len(new) - len(ww))
else:
    print("WW_CHANGES.md: marker already present, nothing written")

# 2. MISTAKES.md -- append
mi = rd(R + "MISTAKES.md")
ent = strip_comment(rd(S + "MISTAKES_ENTRIES.md"))
assert b"\r" not in mi and b"\r" not in ent
m2 = b"## 2026-09-11 -- lane WATER8: a finished build left BUILDING up and DONE unwritten"
assert ent.startswith(m2), ent[:80]
if mi.count(m2) == 0:
    new = mi.rstrip(b"\n") + b"\n\n" + ent.rstrip(b"\n") + b"\n"
    assert new.count(b"\r") == 0
    wr(R + "MISTAKES.md", new)
    print("MISTAKES.md: two entries appended, bytes +", len(new) - len(mi))
else:
    print("MISTAKES.md: marker already present, nothing written")

# 3. HANDOFF.md -- the lane's block goes right after the RESUME paragraph's last line
ho = rd(R + "HANDOFF.md")
blk = strip_comment(rd(S + "HANDOFF_BLOCK.md"))
assert b"\r" not in ho and b"\r" not in blk
m3 = b"**WATER8 LANDED AND IS NOW GATED, EXE FREE.**"
assert blk.startswith(m3), blk[:80]
anchor = b"  open; the 21:02 exe carries WATER8 and needs a restart once gated.\n"
assert ho.count(anchor) == 1, ho.count(anchor)
if ho.count(m3) == 0:
    ins = anchor + b"- WATER8 + WATER8-GATE block, spliced 2026-09-11 06:5x (lane text verbatim):\n\n" + blk.rstrip(b"\n") + b"\n\n"
    new = ho.replace(anchor, ins, 1)
    assert new.count(b"\r") == 0
    wr(R + "HANDOFF.md", new)
    print("HANDOFF.md: block spliced, bytes +", len(new) - len(ho))
else:
    print("HANDOFF.md: marker already present, nothing written")
