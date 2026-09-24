"""Director splice for lane UI5 (CONSTITUTION 8): entry text to the TOP of
WW_CHANGES.md, mistakes to the END of MISTAKES.md, handoff block INTO the
HANDOFF.md lane list; bytes only, CR counts asserted (WW_CHANGES.md mixed at
19,020 CR; the others 0).  Idempotent on the first heading of each text."""
R = "E:/Projects/NifskopeWildWastelandEdition/"
S = R + "scratchpad/ui5_20260910/"

def strip_comment(b):
    if b.startswith(b"<!--"):
        b = b[b.index(b"-->") + 3:].lstrip(b"\r\n")
    return b

def rd(p):
    with open(p, "rb") as f:
        return f.read()

def wr(p, b):
    with open(p, "wb") as f:
        f.write(b)

def first_line(b):
    return b.split(b"\n", 1)[0]

# 1. WW_CHANGES.md
ww = rd(R + "WW_CHANGES.md")
entry = strip_comment(rd(S + "WW_CHANGES_ENTRY.md"))
assert b"\r" not in entry
m1 = first_line(entry)
assert m1.startswith(b"#"), m1
title = b"# NifSkope \xe2\x80\x94 Wild Wasteland Edition: Change Log\n\n"
assert ww.startswith(title)
if ww.count(m1) == 0:
    cr0 = ww.count(b"\r")
    new = title + entry.rstrip(b"\n") + b"\n\n" + ww[len(title):]
    assert new.count(b"\r") == cr0 == 19020
    wr(R + "WW_CHANGES.md", new)
    print("WW_CHANGES.md: spliced", m1[:70], "bytes +", len(new) - len(ww))
else:
    print("WW_CHANGES.md: already present")

# 2. MISTAKES.md
mi = rd(R + "MISTAKES.md")
ent = strip_comment(rd(S + "MISTAKES_ENTRIES.md"))
assert b"\r" not in mi and b"\r" not in ent
m2 = first_line(ent)
assert m2.startswith(b"## "), m2
if mi.count(m2) == 0:
    new = mi.rstrip(b"\n") + b"\n\n" + ent.rstrip(b"\n") + b"\n"
    assert new.count(b"\r") == 0
    wr(R + "MISTAKES.md", new)
    print("MISTAKES.md: appended", ent.count(b"\n## "), "entries, bytes +", len(new) - len(mi))
else:
    print("MISTAKES.md: already present")

# 3. HANDOFF.md -- a new bullet before the DIRECTOR MISTAKE 18:3x bullet
ho = rd(R + "HANDOFF.md")
blk = strip_comment(rd(S + "HANDOFF_BLOCK.md"))
assert b"\r" not in ho and b"\r" not in blk
m3 = first_line(blk)
anchor = b"- DIRECTOR MISTAKE 18:3x (MISTAKES.md): 35 foreign skill directories were\n"
assert ho.count(anchor) == 1, ho.count(anchor)
if ho.count(m3) == 0:
    ins = b"- UI5 block, spliced 2026-09-11 09:0x (lane text verbatim):\n\n" + blk.rstrip(b"\n") + b"\n\n" + anchor
    new = ho.replace(anchor, ins, 1)
    assert new.count(b"\r") == 0
    wr(R + "HANDOFF.md", new)
    print("HANDOFF.md: spliced", m3[:70], "bytes +", len(new) - len(ho))
else:
    print("HANDOFF.md: already present")
