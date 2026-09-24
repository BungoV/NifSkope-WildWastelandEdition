"""Director MISTAKES.md append, 2026-09-11 19:0x (LF-only file, CR asserted 0)."""
P = "E:/Projects/NifskopeWildWastelandEdition/MISTAKES.md"
entry = """## 2026-09-11 19:08 -- director: the 50-percent compaction rule (CONSTITUTION 1b) was missed; bungo saw 68 percent

**What was done.** The session ran from 05:2x to 19:0x, eleven lanes and
some forty rulings, without the director ever preparing a compaction. bungo
asked "68 percent memory, why no compact?".

**What was true instead.** The budget line the director sees reads "14.98
million tokens left" of 15 million, which reads as under one percent used;
the context gauge bungo sees is a different number and is the one rule 1b
names. The director treated its own counter as the gauge and never checked
the one that counts.

**How it was found.** bungo said the number.

**The rule that prevents it.** The director's token counter is NOT the
context gauge. bungo, same minute: "500k is the compact line" -- the
window's 50 percent is 500,000 tokens of context, and that is the number
rule 1b means. The trigger is bungo's displayed number, or, in
his absence, a count of turns: after every lane landing the director asks
itself whether the block is compaction-ready and says so in the reply; at
any ten lane landings without a compaction the block is rewritten complete
and the reply names the compaction as due. The handoff top block is kept
current at every landing so the tick costs nothing (rule 1c's shape).
"""
with open(P, "rb") as f:
    b = f.read()
assert b.count(b"\r") == 0
marker = entry.split("\n", 1)[0].encode()
if b.count(marker) == 0:
    b = b.rstrip(b"\n") + b"\n\n" + entry.encode()
    assert b.count(b"\r") == 0
    with open(P, "wb") as f:
        f.write(b)
    print("appended")
else:
    print("present")
