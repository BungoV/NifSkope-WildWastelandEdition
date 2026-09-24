p = 'docs/LODGEN_NATIVE_LODO_LODI.md'
s = open(p, encoding='utf-8', newline='').read()


def rep(o, n, c=1):
    global s
    assert s.count(o) == c, (s.count(o), o[:90])
    s = s.replace(o, n)


rep("## 4. `.lodi` header — 256 bytes at offset 0",
    "## 4. `.lodi` header — 256 bytes at offset 0, **512 on a version-7 file**")

rep("| 0x04 | u32 | **version = 3, or 4 when the file carries aggregates (§4.6), or 5 when it carries the placement-AO blob (§4.7)**; versions 1 and 2 are refused by name |",
    "| 0x04 | u32 | **version = 3, or 4 when the file carries aggregates (§4.6), 5 when it carries the placement-AO blob (§4.7), 6 when it carries the per-vertex AO stream (§4.8), or 7 when it carries a group table (§4.9) or a per-vertex sky stream (§4.10)**; versions 1 and 2 are refused by name |")

rep("| 0x0C | u32 | `headerCrc32` |\n| 0x10 | u64 | `pluginCorpusHash`",
    "| 0x0C | u32 | `headerCrc32` — over `0x10 … headerBytes − 1`, so it covers **256 bytes on a v3…v6 file and 512 on a v7 one**, and a v6 file's CRC is the byte-for-byte same number it was before v7 existed |\n| 0x10 | u64 | `pluginCorpusHash`")

rep("""| **0xFC** | **u32** | **`vertexAoBytes` (v6) — the whole stream, offsets included; must be ≥ 4 × (`instanceCount` + 1)** |
| — | — | the pad starts at 0xF1 on a v5 file, 0xD4 on a v4 file and 0xB0 on a v3 file; a v3 or v4 file carrying anything at 0xE4…0xF0, or a v3…v5 file carrying anything at 0xF4…0xFF, is refused BY VERSION NAME |""",
    """| **0xFC** | **u32** | **`vertexAoBytes` (v6) — the whole stream, offsets included; must be ≥ 4 × (`instanceCount` + 1)** |
| **0x100** | **u64** | **offset: group table (v7, §4.9), written LAST so no existing offset moves** |
| **0x108** | **u32** | **`groupCount` (v7) — the chunks' group counts SUMMED; the reader adds them up itself and refuses a header word that disagrees** |
| **0x10C** | **u16** | **`groupStride` = 2 (v7); any other value is refused by name** |
| **0x110** | **u64** | **offset: per-vertex sky stream (v7, §4.10)** |
| **0x118** | **u32** | **`vertexSkyBytes` (v7) — the whole stream, offsets included; must be ≥ 4 × (`instanceCount` + 1)** |
| 0x11C…0x1FF | — | reserved, zero (v7) |
| — | — | the pad starts at 0xF1 on a v5 file, 0xD4 on a v4 file and 0xB0 on a v3 file; a v3 or v4 file carrying anything at 0xE4…0xF0, or a v3…v5 file carrying anything at 0xF4…0xFF, is refused BY VERSION NAME. **A version-3…6 file carrying anything at 0x100…0x11F is refused by version name too: those versions have a 256-byte header and END at 0x100.** |

**THE HEADER BLOCK GREW, and that is a deviation stated out loud.** The 256-byte
block was FULL: after v6 the only free bytes were 0xF1…0xF3, three of them, where
version 7 needs twenty-four. So the block is 512 bytes **on a version-7 file
only**. Nothing moved to pay for it: 0x100…0xFFF was already zero pad ahead of
the 4096-aligned first payload, and because `headerCrc32` is defined over
`0x10 … headerBytes − 1` rather than over a literal 0x100, every version-3…6
file keeps the exact CRC and the exact bytes it had. `lodiHeaderBytes(version)`
is the one place that decision lives, and the writer's `file.resize()`, the CRC
and the reader's first payload offset all read it rather than a constant.""")

open(p, 'w', encoding='utf-8', newline='').write(s)
print('docs s4 header table done')
