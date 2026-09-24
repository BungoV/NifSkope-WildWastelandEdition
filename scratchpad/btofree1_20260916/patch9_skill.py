# Lane BTOFREE1, 2026-09-16 -- patch 9: the lodgen skill learns which files the
# FO4CS target actually leaves behind. Both copies of the skill (the repo's and
# the one under E:/Projects/Claude) are byte-identical today and both are
# written, so a later lane reading either one gets the same answer.
SECTION = '''## THE TWO BAKE TARGETS, AND WHAT EACH ONE LEAVES ON DISK (2026-09-16, lane BTOFREE1)

There is no `--target` flag. **`--native <dir>` IS the FO4CS target** on the command line; the panel
picks it with `LodgenTargetBox` index 0 (`fo4cs()`). No `--native` at all is the stock engine target.

| target | what lands in the mod folder |
|---|---|
| **FO4CS** (`--native <dir>`) | `.lodl .lodt .lodo .lodi .lodm`, the texture / card arrays, the heightmap DDS, and `<chunk>.BTO.manifest.txt` |
| **stock engine** (no `--native`) | exactly what it always wrote: `.BTR`, `.BTO`, the sidecars, the atlas. **Byte-identical, and that is a hard gate** |

**The `.BTO` is SCAFFOLDING under the FO4CS target, not output.** Five passes read a chunk back --
texture arrays, atlas, `lodgenMergeChunkShapes`, `lodgenSimplifyFarRings`, card arrays -- and each of
them opens `<chunk>.BTO.manifest.txt` beside the file it is reading. So the bake builds the chunks in
`<mod folder>/lodgen_bto_scratch`, runs every read-back there, then moves the **manifests** into
`meshes/terrain/<ws>/` and deletes the chunks and the folder. One function does the teardown for both
front ends: `lodgenDropBtoScratch()` in `src/lodgenchunkpass.cpp`.

* **Way back:** `--keep-bto`, or the panel row *Keep legacy .BTO chunks* (`keepBto`, default OFF,
  shown under FO4CS only). Byte-identical to a bake from before 2026-09-16.
* **The census says which happened**, and `lodgen_btofree.sh` leg (d) reads the numbers:
  `bto built in scratch <dir>, N chunk(s), N dropped, B bytes freed` against
  `bto built in the mod folder, N chunk(s), 0 dropped, 0 bytes freed`.
* **Writing a harness that opens a `.BTO` from a `--native` bake?** Spell `--keep-bto` on that bake
  (`tests/spells/lodgen_native.sh` check 4 does) or read the **manifest**, which is kept either way
  (`lodgen_ladder.sh` does). A harness that just globs `*.BTO` after a default FO4CS bake finds
  nothing and reads like a writer defect.
* Gate: `tests/spells/lodgen_btofree.sh` -- legs (a) default drop, (b) `--keep-bto` == rung,
  (c) stock == rung, (d) the census clause. `RUNG=` names the exe the bytes are pinned to.

'''
TARGETS = ['E:/Projects/NifskopeWildWastelandEdition/.claude/skills/nifskope-ww-lodgen/SKILL.md',
           'E:/Projects/Claude/.claude/skills/nifskope-ww-lodgen/SKILL.md']
ANCHOR = '## The gates (what "works" means here)\n'

for p in TARGETS:
    b = open(p, 'rb').read()
    cr0, lf0, n0 = b.count(b'\r'), b.count(b'\n'), len(b)
    s = b.decode('utf-8')
    n = s.count(ANCHOR)
    assert n == 1, 'anchor count %d in %s' % (n, p)
    s = s.replace(ANCHOR, SECTION + ANCHOR)
    nb = s.encode('utf-8')
    assert nb.count(b'\r') == cr0, 'CR count moved in %s' % p
    open(p, 'wb').write(nb)
    print('%s %d -> %d bytes, CR %d -> %d, LF %d -> %d'
          % (p, n0, len(nb), cr0, nb.count(b'\r'), lf0, nb.count(b'\n')))
print('patch9 ok')
