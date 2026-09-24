"""Director splice for any landed lane (CONSTITUTION 8).
usage: python scratchpad/splice_lane_docs.py <lane_dir> [handoff_label]
  <lane_dir>/WW_CHANGES_ENTRY.md  -> TOP of WW_CHANGES.md (below the title)
  <lane_dir>/MISTAKES_ENTRIES.md  -> END of MISTAKES.md
  <lane_dir>/HANDOFF_BLOCK.md     -> HANDOFF.md, before the DIRECTOR MISTAKE 18:3x bullet
Bytes only; CR counts asserted (WW_CHANGES.md mixed at 19,020; others 0).
Idempotent on each text's first heading/bullet line. Leading HTML comment stripped."""
import sys, os
R = "E:/Projects/NifskopeWildWastelandEdition/"
S = R + sys.argv[1].rstrip("/\\") + "/"
label = sys.argv[2] if len(sys.argv) > 2 else os.path.basename(S.rstrip("/"))

def strip_comment(b):
    if b.startswith(b"<!--"):
        b = b[b.index(b"-->") + 3:].lstrip(b"\r\n")
    # a lane may open with a level-1 title line addressed to the director
    # ("# Entry for WW_CHANGES.md -- lane X ..."); drop it, keep the entry
    if b.startswith(b"# ") and b"\n" in b:
        first, rest = b.split(b"\n", 1)
        if b"director" in first.lower() or b"lane " in first.lower():
            b = rest.lstrip(b"\r\n")
    # a parenthesised note to the director ("(For the director to splice. ...)")
    # runs to its first blank line; drop it, keep the entry
    while b.startswith(b"("):
        k = b.find(b"\n\n")
        assert k >= 0, b[:80]
        b = b[k + 2:].lstrip(b"\r\n")
    # a horizontal rule under the note
    while b.startswith(b"---"):
        b = b.split(b"\n", 1)[1].lstrip(b"\r\n")
    # any other plain lead paragraph(s) addressed to the director: drop until
    # the first line that opens an entry (# heading, bold lead, or a bullet)
    while b and not (b.startswith(b"#") or b.startswith(b"**") or b.startswith(b"- ")):
        k = b.find(b"\n\n")
        assert k >= 0, b[:80]
        b = b[k + 2:].lstrip(b"\r\n")
        while b.startswith(b"---"):
            b = b.split(b"\n", 1)[1].lstrip(b"\r\n")
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
p = S + "WW_CHANGES_ENTRY.md"
if os.path.exists(p):
    ww = rd(R + "WW_CHANGES.md")
    entry = strip_comment(rd(p))
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
else:
    print("WW_CHANGES.md: no entry file")

# 2. MISTAKES.md
p = S + "MISTAKES_ENTRIES.md"
if os.path.exists(p):
    mi = rd(R + "MISTAKES.md")
    ent = strip_comment(rd(p))
    assert b"\r" not in mi and b"\r" not in ent
    if not ent.startswith(b"## "):
        k = ent.find(b"\n## ")
        if k >= 0:
            # a plain lead paragraph addressed to the director: drop up to the first entry heading
            ent = ent[k + 1:]
        else:
            # bold-led entries with no heading of their own: give the batch the ledger's heading form
            assert ent.startswith(b"**"), ent[:80]
            ent = b"## 2026-09-11 -- lane " + label.encode() + b" (entries in the lane's own words)\n\n" + ent
    m2 = first_line(ent)
    assert m2.startswith(b"## "), m2
    if mi.count(m2) == 0:
        new = mi.rstrip(b"\n") + b"\n\n" + ent.rstrip(b"\n") + b"\n"
        assert new.count(b"\r") == 0
        wr(R + "MISTAKES.md", new)
        print("MISTAKES.md: appended", 1 + ent.count(b"\n## "), "entries, bytes +", len(new) - len(mi))
    else:
        print("MISTAKES.md: already present")
else:
    print("MISTAKES.md: no entries file")

# 3. HANDOFF.md
p = S + "HANDOFF_BLOCK.md"
if os.path.exists(p):
    ho = rd(R + "HANDOFF.md")
    blk = strip_comment(rd(p))
    assert b"\r" not in ho and b"\r" not in blk
    m3 = first_line(blk)
    anchor = b"- DIRECTOR MISTAKE 18:3x (MISTAKES.md): 35 foreign skill directories were\n"
    assert ho.count(anchor) == 1, ho.count(anchor)
    if ho.count(m3) == 0:
        lead = b"" if blk.startswith(b"- ") else b"- " + label.encode() + b" block, spliced (lane text verbatim):\n\n"
        ins = lead + blk.rstrip(b"\n") + b"\n\n" + anchor
        new = ho.replace(anchor, ins, 1)
        assert new.count(b"\r") == 0
        wr(R + "HANDOFF.md", new)
        print("HANDOFF.md: spliced", m3[:70], "bytes +", len(new) - len(ho))
    else:
        print("HANDOFF.md: already present")
else:
    print("HANDOFF.md: no block file")
