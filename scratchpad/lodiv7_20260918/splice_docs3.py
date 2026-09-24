import io

# ---- the viewer channel table -------------------------------------------------
p = 'docs/LODGEN_NATIVE_LODO_LODI.md'
s = open(p, encoding='utf-8', newline='').read()


def rep(o, n, c=1):
    global s
    assert s.count(o) == c, (s.count(o), o[:90])
    s = s.replace(o, n)


rep("""| `identity` | every placement its own colour, the stock channel-1 hash | `.lodi` instance identity (§4.1c) |
| `identityraw` | that identity's low byte as grey | `.lodi` instance identity & 0xFF |
| `sky` | per-placement sky visibility | `.lodi` instance byte 0x11 |""",
    """| `identity` | **the GROUP**, hashed with the stock channel-1 palette -- one colour a house (v7). On a file with no group table it falls back to the per-placement identity **and the note line says so by name**, rather than drawing the fallback silently | `.lodi` group table (§4.9) |
| `placement` | every placement its own colour -- **what `identity` drew before v7** | `.lodi` instance identity (§4.1c) |
| `identityraw` | that identity's low byte as grey | `.lodi` instance identity & 0xFF |
| `sky` | sky visibility: the **per-vertex stream** on a v7 file (§4.10), the flat per-placement byte on a v6 one. The note line names WHICH served, with its own count -- `per-vertex stream, N bytes over M slices` against `placement byte, N placements` -- and both numbers are read back from what was uploaded | `.lodi` sky stream (§4.10), else instance byte 0x11 |""")

open(p, 'w', encoding='utf-8', newline='').write(s)
print('viewer channel table done')

# ---- the census page ----------------------------------------------------------
p2 = 'docs/LODGEN_CENSUS.md'
s = open(p2, encoding='utf-8', newline='').read()

rep("""| `cover ...` | the ground-cover per-chunk census:""",
    """| `native-group:` / the `groups` and `vertex sky` clauses (lane LODIV7, 2026-09-18) | **The v7 clauses, on the same discipline as the road and object-AO ones: a zero is WRITTEN, never omitted, so a reader can tell "off" from "an older build".** `vertex sky ON: N placements streamed (B bytes, mean M, K at or above 128)`, or `OFF (--lodi-v6)`; and `groups G over P placements (R grouped, largest L, S singleton), A placement(s) whose BASE model path has a \\`C\\` component`, or `OFF (--lodi-v6)`. **The last clause names the COMPONENT the bake actually matched on** (`C`, from `WW_LODI_GROUP_COMPONENT`), not the word `architecture`, because the knob is settable and a census that printed the default while the bake used something else would be telemetry echoing intent instead of truth. New words: `vertexSkyBytes`, `vertexSkyPlacements`, `groups`, `groupedPlacements`, `largestGroup`, `singletonGroups`. HOW THEY MOVE, measured on chunk 4.4.-12 of the Commonwealth: `--lodi-v6` turns both clauses to `OFF` and the pair back to byte-identical v6; raising `WW_LODI_GROUP_TOLERANCE` from 0 to 256 takes `groups` 713 -> 588 (16, the shipped value) -> 527 and `largestGroup` 126 -> 205 -> 288; `WW_LODI_GROUP_SHAPE=sphere` takes `largestGroup` to 1,526; and `WW_LODI_GROUP_GRID` moves NOTHING at all -- 256 and 4096 write the byte-identical `.lodi` -- because the grid is an accelerator and not a rule. Gate: `tests/spells/lodi_v7.sh` G2 and G3, whose four grouping refuters each carry a control that must go red. |
| `cover ...` | the ground-cover per-chunk census:""")

open(p2, 'w', encoding='utf-8', newline='').write(s)
print('census page done')
