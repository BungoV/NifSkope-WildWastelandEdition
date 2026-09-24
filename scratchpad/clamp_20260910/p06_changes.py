# LANE CLAMP: the WW_CHANGES.md entry.  The file is MIXED; the 2026-09 entries
# at the top are LF-only, so the entry is spliced in as LF and the CR count is
# asserted unchanged at 19,020 (CONSTITUTION rule 8).
import sys

PATH = 'WW_CHANGES.md'
raw = open(PATH, 'rb').read()
cr_before = raw.count(b'\r')
if cr_before != 19020:
    print('CR is %d, not the expected 19,020 -- another lane moved it; refusing'
          % cr_before)
    sys.exit(1)

HEAD = b'# NifSkope \xe2\x80\x94 Wild Wasteland Edition: Change Log\n\n'
if not raw.startswith(HEAD):
    print('header not where expected'); sys.exit(1)

entry = '\n'.join([
    "## 2026-09-10 - the per-chunk terrain bake gets the pyramid's one-cell ring",
    '',
    '`src/lodgen.cpp` (the terrain bake only), `tests/spells/lodgen_terrain_vt.sh`,',
    '`docs/LODGEN_TERRAIN_VT.md`. **WRITTEN, NOT BUILT** -- see the status block at',
    'the end of this entry.',
    '',
    'The defect lane VTFIX named on 2026-09-09 and did not fix: the DIRECT chunk',
    'bake reads its heights out of a grid that stops at the chunk edge, so both of',
    'its neighbourhood operators are served a CLAMP outside -- the chunk\'s own edge',
    'sample repeated outwards, a plateau that is not the ground. The pyramid\'s tile',
    'baker has always baked a one-cell RING and read real neighbours. That is why',
    'the same chunk\'s sheets differed between the two paths (43 colour texels and',
    '5,524 `_msn` texels on Commonwealth.4.-24.24, every one of them within 4 texels',
    'of the chunk boundary), and why the step in the normal ACROSS a chunk seam read',
    '7.312 on the clamped bake against 4.955 on the ringed one while their interior',
    'controls agreed to three digits (1.797 against 1.803). bungo\'s call: give the',
    'direct bake the same ring so both paths are correct AND identical.',
    '',
    '**One home, not a second copy.** Four things the two bakers must never drift',
    'on now live once, at the top of the terrain section:',
    '`lodgenTerrainHeightAt` (the eased reconstruction, already shared),',
    '`lodgenTerrainMsnPixel` (already shared), and new here',
    '`lodgenTerrainFillRing` (fills the (rdim*32+1)^2 grid from a caller-supplied',
    'cell fetch, in the iteration order that decides a shared VHGT edge) and',
    '`lodgenTerrainGridSample` (the plain-bilinear tap). The tap replaced FIVE',
    'byte-for-byte copies of the same seven lines. The per-texel loops stay apart,',
    'deliberately, because their outputs are gated against files measured before',
    'either lane existed.',
    '',
    'The chunk baker now builds `hgt` on `dim + 2` cells and offsets its own',
    'coordinates by 4,096 units into it. The msn central difference and the',
    '2,048-unit AO march read that ring. Two things deliberately did NOT move, and',
    'both are stated rather than left to be discovered:',
    '',
    '* **the PAINT stays scoped to the chunk** -- `cells`, `haveLand`,',
    '  `dominantBase`, the per-quadrant cover constants. Every texel this bake',
    '  writes lands inside the chunk, so a neighbour\'s paint has nothing to',
    '  contribute, and widening the scope would move the dominant base (which is',
    '  exactly what V9a\'s cover-free half exists to catch).',
    '* **the per-sample channels stay on the chunk grid** (`chgt`, a copy of the',
    '  ring\'s middle). `lodgenTerrainChannels` computes wetness as a flow',
    '  accumulation over the WHOLE grid it is handed, so widening that grid moves',
    '  the sheet\'s INTERIOR, not its edge -- and it still would not match the',
    '  pyramid, whose tiles accumulate over a tile-sized grid. The `_data` sheet is',
    '  therefore NOT pinned to identity between the two paths, the harness says so,',
    '  and giving wetness a domain that is not the bake unit is a separate track.',
    '',
    '**The gate got TIGHTER, not looser.** V9a\'s tinted half was a bounded band',
    '(<= 4 texels from the boundary, <= 24/255, <= 0.05% of a sheet) because the',
    'two paths\' normals legitimately disagreed there. It is now a plain `cmp` on',
    'all four fixture chunks, with three things beside it: **V9b** pins the `_msn`',
    'sheets to identity in their own right (they are the operand that used to',
    'differ), **a FLOOR** requires each path\'s cover sheet to differ from its own',
    'cover-free sheet -- two byte-identical files also compare equal when the cover',
    'pass, the tint or the bake is silently inert -- and **V9c** measures WHICH',
    'normal is right, on the direct sheets alone: the step across a chunk seam',
    'against the ordinary step between two adjacent columns taken WELL INSIDE the',
    'sheet (x = 100, 200, 300, 400; a clamped bake\'s own edge column is inside the',
    'defect and using it once reversed the verdict -- MISTAKES.md, lane VTFIX).',
    'Its bars sit BETWEEN the two known readings, so they discriminate: E/W ratio',
    '<= 3.20 (ringed 2.75, clamped 4.07), N/S <= 3.30 (2.87 against 3.78), a',
    'sheet\'s own edge step <= 2.60 (1.961 against 3.310), and the interior control',
    'must land in 1.20..2.20 or the sheets are not the terrain the bars were',
    'measured on.',
    '',
    '**The re-baseline, and the bands it was pre-registered against.** The change',
    'moves the EDGE bytes of every terrain chunk sheet in every worldspace, so the',
    'per-sheet band was written down from the code BEFORE any bake existed, at dim',
    '4 = 32 world units a texel: colour <= 4 texels and `_msn` <= 4 texels (the',
    'normal\'s one-step central difference, 128 units), `_data` <= 64 texels (the AO',
    'march\'s own `dist <= 2048.0f`). Beyond each band the count must be ZERO --',
    'that zero-change interior is the control. `scratchpad/clamp_20260910/edgeband.py`',
    'is the instrument and it was run against two known-answer inputs before it was',
    'believed: two identical sheet sets read 0 everywhere and the FLOOR fired',
    '(rc 1), and the cover-vs-no-cover pair (the ceiling) read 428,272 texels with',
    '416,280 of them beyond the 4-texel band and failed four bars (rc 1). Its',
    'ceiling count reproduces lane VTFIX\'s 207,945 on Commonwealth.4.-24.24 exactly,',
    'through a decoder that shares no code with `lodgenWriteDds`.',
    '',
    '`tests/spells/lodgen_identity.sh` is NOT affected and that is a reading, not an',
    'assumption: it bakes `--objects ... --no-ao` and compares `.bto` files and',
    'manifests, and no terrain sheet is written on that path. The Land VERTEX',
    'channels are untouched too -- `lodgenTerrainChannels` is called a second time,',
    'from the mesh path, and that call was not changed.',
    '',
    '**STATUS: BUILD PENDING.** Nothing here has been compiled. The build slot was',
    'held when the code landed (`scratchpad/water2_20260909/DONE` absent and a',
    '`NifSkope.exe` in the process table), so lane CLAMP ended with the code, the',
    'harness, the document and the instrument on disk and every number above either',
    'quoted from lane VTFIX\'s 2026-09-09 measurement or read off the source. The',
    'resume is `scratchpad/clamp_20260910/PENDING.md`; the before-sheets it diffs',
    'against are already baked, under `scratchpad/clamp_20260910/before/`, from',
    '`release/NifSkope.exe` of 2026-09-10 00:13 (Commonwealth.4.-24.24 with cover',
    'hashes `aee0793ae2c1576b...`, which is byte for byte the file lane VTFIX',
    'measured). Until that resume runs, NOTHING in this entry is a claim about a',
    'built exe.',
    '',
])
out = HEAD + entry.encode('utf-8') + raw[len(HEAD):]
if out.count(b'\r') != cr_before:
    print('CR MOVED %d -> %d' % (cr_before, out.count(b'\r'))); sys.exit(1)
open(PATH, 'wb').write(out)
print('ok  CR %d (unchanged), +%d LF lines' % (cr_before, entry.count('\n')))
