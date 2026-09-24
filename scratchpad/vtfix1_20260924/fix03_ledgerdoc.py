"""VTFIX1 fix03 (PARKED for the director / INCRGATE1: docs/LODGEN_LEDGER_FORMAT.md is not this lane's file). Row 8a, the generator word. (3) LODGEN_PARITY's terrain-identity line matches the code; the VT doc's stale
nifcli anchors and the maskRules census note; (4) the ledger doc's generator row. Anchor count == 1, CR count
unchanged per file."""
R = 'E:/Projects/NifskopeWWE-vtfix1/docs/'


def patch(name, subs):
    p = R + name
    with open(p, 'rb') as f:
        src = f.read()
    cr = src.count(b'\r')
    nl = b'\r\n' if cr and cr * 2 > src.count(b'\n') else b'\n'
    for old, new, label in subs:
        o = old.encode('utf-8').replace(b'\n', nl)
        n = new.encode('utf-8').replace(b'\n', nl)
        c = src.count(o)
        assert c == 1, '%s %s: anchor count %d' % (name, label, c)
        src = src.replace(o, n)
        print('applied', name, label)
    grow = src.count(b'\r') - cr
    assert grow == 0 or nl == b'\r\n', '%s: CR count moved in an LF file' % name
    data = src
    with open(p, 'wb') as f:
        f.write(data)
    print(name, 'CR', cr, '->', data.count(b'\r'))


patch('LODGEN_LEDGER_FORMAT.md', [(
"""| 8 | the switches (section 3) | everywhere | whole region |
""",
"""| 8 | the switches (section 3) | everywhere | whole region |
| 8a | **the generator itself**: the sha1 of the running executable's bytes, fed FIRST into every chunk's `inputs` digest as `generator <sha1>;` (`lodgenGeneratorIdentity()`, `lodgen.cpp`; lane VTFIX1, 2026-09-24) | everywhere | whole region |
""", 'row 8a'), (
"""Row 9 does not widen the map beyond what rows 5—7 already ask for: it reads the
""",
"""Row 8a is there because rows 1—8 describe what the chunk READS and nothing
describes the program that turns those reads into bytes. The defaults live in
three places (nifcli's `lg*` locals, `lodgen.h`'s option initialisers, the `g_*`
globals in `lodgen.cpp`), so a default flip changes no argument and no input:
before VTFIX1, `--incremental` on the new exe called every chunk clean and kept
the old exe's bytes. The same was true of any code change that moves bytes with
no default touched at all. The exe hash covers both without anyone bumping a
constant. It can only over-rebake: a rebuild that moves no output byte still
dirties every chunk once, which is this ledger's stated direction. An exe that
cannot be read gets a word that never matches a stored ledger. The consequence
for a gate: **two bakes from DIFFERENT exes now always write different `inputs`
digests**, so a byte comparison of `.lodb` files across a rung and a new exe
is expected to differ. Same-exe comparisons (section 1's determinism, the
`--vt`/`--native` pairs below) are unaffected. The census reports such a
chunk as `inputs moved`. It has no separate "generator changed" reason yet;
that belongs to `nifcli.cpp`'s comparison (the dirty list at the ledger read).
Gate: `tests/spells/lodgen_vtfix.sh` G3, which flips a default inside a copy
of the exe and requires the incremental run to equal a full flipped bake. On the
pre-VTFIX1 exe it reported `0 of 2 chunks dirty` and left 4 stale files.

Row 9 does not widen the map beyond what rows 5—7 already ask for: it reads the
""", 'row 8a prose')])
