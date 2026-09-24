"""Director MISTAKES.md append, 2026-09-11 15:5x (LF-only file, CR asserted 0)."""
P = "E:/Projects/NifskopeWildWastelandEdition/MISTAKES.md"
entry = """## 2026-09-11 15:5x -- director: an hour of handoff timestamps were GUESSED, not read, and a lane inherited the wrong clock

**What was done.** From about 15:05 the director labelled rulings and lane
events in HANDOFF.md with times typed from a running sense of "how long
this has taken" -- 15:1x, 15:2x ... 16:5x, 17:1x -- without running `date`.
The lane NIFPARSE1 was told in its launch prompt that it was "launched
15:5x" (it launched about 15:06) and wrote 16:0x / 16:2x / 16:5x into its
own report and PENDING resume from that seed.

**What was true instead.** `date` at the moment of discovery read
2026-09-11 15:54:00. The file-anchored truth: PLAN-FO4CS landed 15:04 (its
document's mtime), NIFPARSE1 launched ~15:06 and ended 15:47 (report mtime),
CARDS-AGG launched ~15:08 (its census timestamps 15:11..15:21), FLAGSCAN1
landed 15:51. Every director label after 15:05 is inflated by up to sixty
minutes; the labels before it (05:2x .. 15:0x) were checked against `date`
calls made earlier in the session and are right.

**How it was found.** A `date` call made to see whether CARDS-AGG was still
alive printed 15:53 while the handoff already said 17:1x.

**Cost and damage.** No file, build or gate was affected; the handoff's
ordering is intact (the labels are monotone); the absolute minutes on about
a dozen ruling paragraphs are wrong, and a lane's report carries the wrong
clock. Corrected by a CLOCK CORRECTION note in the top block rather than by
rewriting a dozen quotations from memory a second time.

**The rule that prevents it.** A timestamp written into a ledger is READ
from the clock (`date +%H:%M`) in the same turn it is written, never
typed from a feeling of elapsed time; a lane's launch prompt carries the
clock-read time or none at all. CONSTITUTION 4's "put the MTIMES in one
table" already applies to the director's own clock.
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
