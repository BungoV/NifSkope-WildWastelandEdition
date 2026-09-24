P = r"E:/Projects/NifskopeWildWastelandEdition/MISTAKES.md"
b = open(P, "rb").read()
print("BEFORE  CR=%d LF=%d bytes=%d" % (b.count(b"\r"), b.count(b"\n"), len(b)))
anchor = b"Newest at the top." + b"\r\n\r\n"
assert b.count(anchor) == 1, "header anchor not unique"
i = b.find(anchor) + len(anchor)
assert b[i:i + 3] == b"## ", "insertion point is not the top of the newest entry"
assert b"GATEFIX1" not in b, "this splice has already been run"

entry = """## 2026-09-19 -- GATEFIX1 -- a BEFORE run taken from another lane's leftover log

I edited `tests/spells/native_lighting.sh` before I had produced a baseline run
of it myself, then used `release/ww_native_lighting.log` -- written at 17:11:28
by a run this lane did not make -- as the "before" half of the before/after pair
the brief asked for.

What was true: the numbers happen to be sound. That log is on the same exe
(16:48:03, sha1 072d78f8) and its three failures match HARNESSWIN2's reported
numbers exactly. But that is luck. A leftover log carries no proof of which exe,
which fixtures or which arguments produced it, and this gate's whole defect was
that its inputs had silently changed underneath it.

Found while writing the report section and having to say where the before
numbers came from.

The rule: capture the BEFORE run, yourself, before the first edit to a gate
file -- even when a log with the right-looking numbers is already on disk. The
`lodgen_octahedral.sh` half of the same lane did this correctly and it cost one
command.

## 2026-09-19 -- GATEFIX1 -- a normal dotted into a light in the wrong axis order

Diagnosing gate (b)'s remaining reds, I decoded the terrain normal sheet as
(R, G, B) = (east, up, north) -- which is right -- and then fed that vector
straight into the checker's light `Lw`, which is world (east, north, up). Up and
north were multiplied by each other's components. The probe printed
"up mean -0.1262" for a terrain sheet, and its N.L figures were used for a first
conclusion that the sheet had gone flat.

What was true: the sheet's up component averages +0.90 with sd 0.10 and its N.L
spread is 0.218 -- a strong slope signal, the opposite of flat. The sheet was
never the problem, and a report built on that first pass would have accused the
bake.

Found only because a terrain normal map whose average normal points DOWNWARD is
absurd on its face, not because anything in the code complained.

The rule: when two vectors meet, write the axis order of each in a comment
beside the line that multiplies them, and check the result against a component
whose sign is known in advance. A terrain sheet's mean "up" is positive; if the
arithmetic says otherwise, the arithmetic is wrong before the data is.

"""
ins = entry.replace("\n", "\r\n").encode("utf-8")
out = b[:i] + ins + b[i:]
open(P, "wb").write(out)
c = open(P, "rb").read()
print("AFTER   CR=%d LF=%d bytes=%d" % (c.count(b"\r"), c.count(b"\n"), len(c)))
assert c.count(b"\r") == c.count(b"\n"), "mixed endings introduced"
print("OK")
