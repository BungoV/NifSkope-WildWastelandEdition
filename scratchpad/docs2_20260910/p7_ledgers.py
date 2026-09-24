# The DOCS2 entry at the top of WW_CHANGES.md (LF, CR count unchanged) and the
# three MISTAKES.md entries this lane owes (LF-only file).
import os
os.chdir(r"E:\Projects\NifskopeWildWastelandEdition")

WW_TITLE = b"# NifSkope \xe2\x80\x94 Wild Wasteland Edition: Change Log\n\n"
WW_ENTRY = """## 2026-09-10 - every contract page's line numbers re-derived from their own anchors, and the FO4CS entry point brought current (lane DOCS2)

Documents only: no source file, harness or build was touched.

**The provenance pass** (`ww-contract-provenance` step 3, script
`scratchpad/docs2_20260910/anchors.py`, generalised from lane RENAME's
`p14_anchors.py` to eight pages and eleven sources). Every `| claim | line |
anchor |` row on `docs/LODGEN_BTD_FORMAT.md`, `LODGEN_CARD_SHEETS.md`,
`LODGEN_LODM_FORMAT.md`, `LODGEN_MANIFEST_FORMAT.md`, `LODGEN_TEXTURE_ARRAYS.md`,
`LODGEN_TERRAIN_VT.md`, `LODGEN_NATIVE_LODO_LODI.md` and
`LODGEN_VERTEX_PACKING.md` was located again by its ANCHOR TEXT in the current
source and rewritten from where it was actually found -- never shifted by a
delta, and never rewritten unless the match was EXACT and UNIQUE.

**243 rows checked: 111 moved, 132 were already right, 0 anchors missing, 0
ambiguous, 5 multi-site rows re-derived by hand.** A second run of the same
script over the written pages reports 0 moved, which is the idempotence check.

| page | rows | moved | already right |
|---|---:|---:|---:|
| `LODGEN_BTD_FORMAT.md` | 66 | 26 | 40 |
| `LODGEN_CARD_SHEETS.md` | 36 | 24 | 12 |
| `LODGEN_LODM_FORMAT.md` | 34 | 22 | 12 |
| `LODGEN_MANIFEST_FORMAT.md` | 14 | 13 | 1 |
| `LODGEN_TEXTURE_ARRAYS.md` | 15 | 7 | 7 (+1 multi-site) |
| `LODGEN_TERRAIN_VT.md` | 23 | 6 | 13 (+4 multi-site) |
| `LODGEN_NATIVE_LODO_LODI.md` | 49 | 4 | 45 |
| `LODGEN_VERTEX_PACKING.md` | 11 | 9 | 2 |

**What moved, and why.** `src/lodgen.cpp` `6d7388c53a13343e` (8,286 lines) /
`64191a7ed236ddb8` (8,619) / `3e4815416faa4aba` (8,850) are all now
`c05fd079655ac03e` (391,673 B, 8,924 lines) -- three vintages of stamp on seven
pages, because lanes CARDWIDTH, CLAMP2b and NATIVE0b each grew the file after a
page was stamped. `src/nifskope_ui.cpp` `dbf4540b51166e31` -> `f5d1e3bf9fab7a27`
(31,409 lines), `src/lodtfile.cpp` `10ad5f58262d8620` -> `601fb65136c8766d`
(3,658), `src/lodtfile.h` `128dcd3f4c011a08` -> `4ffeccdc9b581e5e` (435),
`src/btdterrain.cpp` `cbe06adb7394f59b` -> `35c2adc319852901` on the packing
page. `src/io/lodvfile.{h,cpp}`, `src/io/lodmfile.cpp`, `src/data/niftypes.h`,
`src/hkxanim.{h,cpp}` and the six native files did NOT move (same sha256), which
is why 132 rows needed no change and why `docs/HKX_ANIMATION_FORMAT.md` needed
none at all.

**Seven anchors were repaired rather than renumbered**, because the code they
named had changed shape: the AO row-0 comment and the no-ground-cover comment in
`lodtfile.cpp` now WRAP, so each anchor was trimmed to the line it starts on
(and the ground-cover row was 2424, is 2429); the `I`-line row's
`if ( ... < 8 ) continue;` is now two lines, so the anchor is the `if` alone;
the `coverage` sidecar row carried a literal newline where the C++ `\\n` had
been pasted, which had silently broken that markdown row in two; three
`LODGEN_TEXTURE_ARRAYS.md` anchors are now written VERBATIM by the card-array
pass as well and were lengthened until each names the array pass alone; and
`constexpr std::uint64_t OBJ_VERTEX_DESC` is a prefix of `OBJ_VERTEX_DESC_COLORS`
on the next line. One range END was wrong after the pass and was fixed by hand:
`niftypes.h:1907-1987` was carried by the start's delta and ran into
`ClearAttributeOffsets`; the accessors end at 1979.

Three inline citations outside the footer tables were stale as well:
`src/lodgen.cpp:1357-1358` -> `1358-1359` and `src/lodgen.cpp:57` -> `58` in
`LODGEN_VERTEX_PACKING.md`'s prose, and `src/lodtfile.h:58` -> `159` in
`scratchpad/handoff_fo4cs/WRITER_CHANGES_NEEDED.md`. The writer's default is
still `headerVersion = 2` (`src/lodtfile.h:159`), raised to 3 only under
`--water-bodies` -- read from the source last of all, per the skill's step 4.

**`scratchpad/handoff_fo4cs/README.md` rewritten** as the FO4CS reader's single
entry point: the two changes FO4CS must make up front (the loader reads `.lodl`;
it must accept version 3), the file-family table extended with the v3 water
sections, the `<WS>.water.json` curves file and the native pair AS BUILT, and a
new section 3 carrying the reader checklists as they actually landed -- the
`.lodl` v3 body-table lookup for tint / flow / fog, the body-ID plane as the
mask (nearest, never filtered, no mips) and the flow plane filtered only inside
it, the dye plane's source/weight blend, depth = water height minus terrain,
shore distance as the winter freeze-from-the-shore path, the card rules
(`coverage 16 128 160`, orthographic, gap = max(2, side/16), mips = log2(gap),
per-frame extents and `frameOffset`), the native pair's ten-step draw with its
two deviations, the terrain pyramid's NORTH-UP row order and per-tile CRC, and
the DirectX flow-PNG ruling against `src/watercurves.cpp:932`, which still says
`+green = north`. Section 6 is a placeholder: **a whole-Commonwealth bake has
never been timed by any lane**, and bungo's four GUI stage times (landscape,
meshes, textures, impostors) go in the table there.

`docs/LODGEN_*.md` (8 pages), `scratchpad/handoff_fo4cs/README.md`,
`scratchpad/handoff_fo4cs/WRITER_CHANGES_NEEDED.md`, `WW_CHANGES.md`,
`MISTAKES.md`, `scratchpad/docs2_20260910/`,
`scratchpad/lane_docs2_report.md`.

"""

MISTAKES_ENTRIES = """## 2026-09-10 -- "anchor not found" was my extractor failing, not the anchor (lane DOCS2)

**What was done.** The first run of the generalised anchor pass reported 13
rows as MISSING and 4 as AMBIGUOUS across six contract pages, and the numbers
looked like real rot in the tree.

**What was true.** Eleven of the thirteen were the SCRIPT: an anchor whose
ellipsis sits INSIDE the backticks (`` `const qint16 rect[8] = { ... ` ``)
leaves the span unclosed, so the backtick regex found nothing at all; an anchor
that wraps across two source lines cannot match a per-line scan; and one cell
carried a real newline. `src/io/lodvfile.{h,cpp}` had not changed by one byte
(same sha256 as the page's own stamp), so five of those rows could not have
been stale. Only two were genuine.

**How it was found.** Every MISSING was grepped in the source by hand before
anything was written. Had the pass been applied on that state it would have
left 13 rows unverified while its own summary said the page was checked.

**The rule.** A provenance pass reports MISSING only after its extractor has
been SHOWN to extract that anchor. Hand-check every MISSING and every
AMBIGUOUS against the source before believing either -- and check the source's
hash first: a file whose sha256 matches the page's stamp cannot have moved a
line, so any MISSING on it is the reader's fault by construction.

## 2026-09-10 -- a range's END shifted by its START's delta ran past the code it named (lane DOCS2)

**What was done.** The pass rewrote `| niftypes.h:1899-1979 |` to `1907-1987`
by adding the start's delta (+8) to both ends.

**What was true.** `src/data/niftypes.h` had not changed at all; the row's
START was simply anchored one way and numbered another. The three accessors it
names run 1907..1979, and 1987 is inside `ClearAttributeOffsets` -- a claim
about code the row does not describe.

**How it was found.** Reading the file at the new end, because the source's
hash matched the stamp and a "moved" row on an unmoved file cannot both be
true.

**The rule.** A range's END is re-derived, not carried. A scripted pass may
move a start; the end is a hand check, and a row whose start and end come from
different methods says so.

## 2026-09-10 -- a heredoc ate a line continuation and wrote a literal backslash-n into a script (lane DOCS2)

**What was done.** A patch to the anchor script was applied through a
`python - <<'PYEOF'` heredoc whose replacement text ended a line with a
backslash. The heredoc halved it, and the script came out with `\\n` sitting in
the middle of an `if` -- a syntax error.

**What was true.** `nifskope-ww-lodgen` already says it, in bold: "no text
carrying a backslash or an apostrophe goes through a heredoc at all". The
skill was loaded in this same lane.

**How it was found.** The harness re-read the file after the write and showed
the mangled line; the next run would have failed on it anyway.

**The rule.** The existing one, applied: a patch script that carries a
backslash is written with the Write tool and run, never piped through a
heredoc. Loading the skill is not the same as obeying it.

"""

# --- WW_CHANGES.md, mixed, spliced in binary, CR count pinned
b = open("WW_CHANGES.md", "rb").read()
cr = b.count(b"\r")
assert b.startswith(WW_TITLE)
out = WW_TITLE + WW_ENTRY.encode("utf-8") + b[len(WW_TITLE):]
assert out.count(b"\r") == cr, "CR moved"
open("WW_CHANGES.md", "wb").write(out)
print("WW_CHANGES.md  CR %d -> %d  LF %d -> %d" % (cr, out.count(b"\r"), b.count(b"\n"), out.count(b"\n")))

# --- MISTAKES.md, LF-only, newest at the top under the format paragraph
m = open("MISTAKES.md", "rb").read()
assert m.count(b"\r") == 0
head = b"Newest at the top.\n\n"
i = m.index(head) + len(head)
out = m[:i] + MISTAKES_ENTRIES.encode("utf-8") + m[i:]
assert out.count(b"\r") == 0
open("MISTAKES.md", "wb").write(out)
print("MISTAKES.md    LF %d -> %d, CR 0" % (m.count(b"\n"), out.count(b"\n")))
