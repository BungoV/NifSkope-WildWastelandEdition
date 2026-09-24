"""Director MISTAKES.md append, 2026-09-11 (LF-only file, CR asserted 0)."""
P = "E:/Projects/NifskopeWildWastelandEdition/MISTAKES.md"
entry = """## 2026-09-11 08:0x -- director: proposed "hemi-octahedral tree sheets" as a new performance item; the shipped sheets have been hemi-octahedral since the octahedral bake landed

**What was done.** Asked for performance ideas for the improved LOD, the
director listed "hemi-octahedral card sheets for trees" as item 4 of five,
with pros and cons, as if the sheets were full-sphere octahedral today.

**What was true instead.** `docs/LODGEN_IMPOSTOR_SPEC.md` ("Frames ... on the
grid's VERTICES under the hemi-octahedral mapping", the mapping with the four
corners at exact horizon and the centre at the top) and
`docs/LODGEN_CARD_SHEETS.md` section 3 say the same. bungo caught it: "isn't
our sheet already hemi-octahedral?"

**How it was found.** By him, one message later; then one grep of the two
contract pages.

**The rule that prevents it.** THE THIRD RULE OF 2026-09-04 21:33: check our
own tree before quoting a document -- and before proposing a feature. A
proposal list about our own formats is written AFTER a grep of the contract
pages for each item, and each item says whether it is new, partly there, or
already shipped.
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
