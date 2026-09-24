"""Director MISTAKES.md append, 2026-09-11 (LF-only file, CR asserted 0)."""
P = "E:/Projects/NifskopeWildWastelandEdition/MISTAKES.md"
entry = """## 2026-09-11 08:5x -- director: told bungo "the blend weights themselves are not stored, only their result"; the .lodl stores them per texel

**What was done.** Explaining how the terrain blends meet the heightmap, the
director said the LAND splat is resolved at bake time into the colour sheets
and that "the blend weights themselves are not stored, only their result", so
a runtime re-blend would need a rebake.

**What was true instead.** `docs/LODGEN_BTD_FORMAT.md` (Tables, Blocks): every
`.lodl` block carries `uint16[N] LTEX alphas` -- five 3-bit layer fields over a
base layer per texel, the quadrant's five strongest ATXT/VTXT layers by peak
opacity as slots into the LTEX table -- for an FO4 LAND source as much as for
a FO76 `.btd`, and the "What it replaces" table says outright that the
per-chunk diffuse DDS is replaced by a "runtime blend from the stored LTEX
alphas". The BAKED sheets (the `.btr` chunk DDS and the optional `.lodt`
pyramid) are the resolved result; the `.lodl` is the weights. Both exist.

**How it was found.** bungo's next question ("They are baked in 76 too?")
forced a read of the `.btd` conversion table, which copies the 3-bit alpha
blocks verbatim -- which they could not be if the `.lodl` held no weights.

**The rule that prevents it.** The third rule of 2026-09-04 21:33, second
time today (see 08:0x): a statement about what one of OUR files holds is made
after reading that file's contract section, not from the neighbouring
contract. `.lodl` and `.lodt` are two contracts; one was read and the other
was assumed.
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
    print("already present")
