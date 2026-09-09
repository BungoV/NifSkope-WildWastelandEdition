# Patch 16 -- the FO4CS handoff package carries the new names, and the two
# writer-change items this lane actually closed are marked closed.
import os
os.chdir(r"E:\Projects\NifskopeWildWastelandEdition")


def load(p):
    b = open(p, "rb").read()
    assert b.count(b"\r") == 0, p
    return b.decode("utf-8")


def save(p, s):
    out = s.encode("utf-8")
    assert out.count(b"\r") == 0
    open(p, "wb").write(out)
    print("OK", p, len(out), "bytes, CR", out.count(b"\r"))


def sub(s, old, new, p, n=1):
    c = s.count(old)
    assert c == n, "%s: %d hits for %r" % (p, c, old[:80])
    return s.replace(old, new)


LAND = [(".lodt", ".lodl"), ("WW_LODT_VERSION", "WW_LODL_VERSION"),
        ("lodt_write.sh", "lodl_write.sh"), ("lodt_open.sh", "lodl_open.sh"),
        ("--lodt ", "--lodl ")]
TEX = [(".lodv", ".lodt"), ("`.lodg`", "`.lodo`"), (".lodg", ".lodo"),
       ("LODGEN_NATIVE_LODG_LODI.md", "LODGEN_NATIVE_LODO_LODI.md")]

P = "scratchpad/handoff_fo4cs/README.md"
s = load(P)
for a, b in LAND + TEX:
    s = s.replace(a, b)
s = sub(s,
        """**bungo's naming ruling, 2026-09-09 ~16:1x**, verbatim: *"lodg sounds better"* —
the FO4CS-native far field is **`.lodo` + `.lodi`**. There is no interim
`.lodo`. **`.bto` stays the stock bake** the engine reads.""",
        """**bungo's FINAL naming ruling, 2026-09-09 ~16:4x**, which supersedes the
~16:1x *"lodg sounds better"*. THE WHOLE FAMILY:

| extension | holds | was called |
|---|---|---|
| `.lodl` | **land** — heights, AO, LTEX blend, colour, water, ground cover, overview | `.lodt` |
| `.lodt` | **terrain textures** — the sheets, one file per pyramid level | `.lodv` |
| `.lodo` | **objects** — the FO4CS-native geometry library | (was to be `.lodg`) |
| `.lodi` | **instances** — the placement tables | `.lodi` |
| `.lodm` | **materials** — the LOD material sidecar | `.lodm` |

`.lodv` and `.lodg` are retired. **`.lodt` is REPURPOSED**, which is why the
texture container took its OWN new magic, `LDTX`: the landscape file keeps the
magic `LODT` (its bytes did not change), and **each reader refuses the other's
file BY NAME** — a yesterday's `.lodt` opened as a terrain texture is told it is
the landscape file and to open it as `.lodl`, and vice versa. **`.bto`/`.btr`
stay the stock bake** the engine reads.

**What an FO4CS reader must change:** the terrain loader's extension, from
`<WS>.lodt` to `<WS>.lodl`. Nothing else — not the magic, not the version, not
one byte of layout.""", P)
save(P, s)

P = "scratchpad/handoff_fo4cs/WRITER_CHANGES_NEEDED.md"
s = load(P)
for a, b in LAND + TEX:
    s = s.replace(a, b)
s = sub(s, "## 1. Two wrong hex comments beside correct constants — `src/lodgen.cpp`",
        "## 1. ~~Two wrong hex comments beside correct constants~~ — **DONE 2026-09-09**\n\n"
        "**Closed by lane RENAME.** All three comments in `src/lodgen.cpp` now match "
        "their own integers, and the computed 32-byte object descriptor "
        "`0x0013F07006543208` is written beside `OBJ_VERTEX_DESC_COLORS`. **No value "
        "moved**; the patch script asserts each of the three integers is still present "
        "exactly once. What follows is the record of what was wrong.\n\n"
        "### `src/lodgen.cpp`", P)
s = sub(s, "**Decides:** whichever lane owns `src/lodgen.cpp` next. Cosmetic to the code,\nload-bearing to a reader.",
        "**Decided and done:** lane RENAME, 2026-09-09.", P)
save(P, s)

for p in ("scratchpad/handoff_fo4cs/README.md",
          "scratchpad/handoff_fo4cs/WRITER_CHANGES_NEEDED.md"):
    for i, ln in enumerate(load(p).split("\n"), 1):
        low = ln.lower()
        if "lodv" in low or ".lodg" in low:
            print("  check %s @%d: %s" % (p, i, ln.strip()[:100]))
