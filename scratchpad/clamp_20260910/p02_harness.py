# LANE CLAMP: V9a's tinted half becomes byte identity, V9b pins the msn, a
# FLOOR proves the tint is live, and V9c measures WHICH normal is right on the
# direct sheets alone.
#
# Run from the repo root: python scratchpad/clamp_20260910/p02_harness.py
import sys

PATH = 'tests/spells/lodgen_terrain_vt.sh'
raw = open(PATH, 'rb').read()
cr_before = raw.count(b'\r')
s = raw.decode('utf-8')
T = chr(9)


def repl(old, new):
    global s
    if s.count(old) != 1:
        print('anchor count %d for %r' % (s.count(old), old[:70]))
        sys.exit(1)
    s = s.replace(old, new)


# ------------------------------------------------ 1. why the cover-free pair
old1 = '\n'.join([
    '# V9a needs the pair again with the ground-cover tint OFF. The tint is',
    '# weighted by a slope gate that reads the terrain normal, and the ringed',
    '# tile bake and the clamped chunk bake legitimately disagree on that normal',
    '# within one heightfield sample of a chunk boundary (V9b exempts it there).',
    '# With the tint off nothing else in the colour composite can differ, so this',
    '# pair is where byte identity is the honest bar -- and it is the dominantBase',
    '# gate the spec asks for, exactly.'])
new1 = '\n'.join([
    '# V9a needs the pair again with the ground-cover tint OFF, and it still does',
    '# even though BOTH halves are byte identity since 2026-09-10: the two halves',
    '# fail for different reasons. The tint-off pair can only move if dominantBase',
    '# or the paint composite moved (the spec\'s actual gate); the tint-on pair adds',
    '# the slope gate, which reads the terrain normal. Until 2026-09-10 that normal',
    '# legitimately differed within one heightfield sample of a chunk boundary --',
    '# the tile bake rings its heights, the chunk bake clamped them -- and the',
    '# tinted half was a bounded band, not a cmp. Lane CLAMP gave the chunk bake',
    '# the same ring through the same lodgenTerrainFillRing, so the operand is the',
    '# same bytes on both paths and the bar was TIGHTENED to a cmp, not loosened.',
    '# The cover-free pair is also what makes the FLOOR below computable.'])
repl(old1, new1)

# --------------------------------------- 2. the V9a-2 block -> identity + floor
a = s.find(T + '# V9a-2: with the tint ON, the difference must be the boundary normal\'s and')
if a < 0:
    print('V9a-2 start missing'); sys.exit(1)
b = s.find(T + 'ok "V9 the assembled and direct sheets were both produced and compared"')
if b < 0:
    print('V9 tail missing'); sys.exit(1)

lines = [
    T + '# V9a-2: with the tint ON, byte-identical too. Same files, same bar as',
    T + '# V9a-1 -- see the note beside the cover-free bakes above for why both',
    T + '# halves are still asked, and why this one got TIGHTER on 2026-09-10.',
    T + 'csame=1',
    T + 'for c in $CHUNKS; do',
    T + T + 'cmp -s "$W/run1/tex/$c.DDS" "$W/novt/tex/$c.DDS" || { csame=0; say "differs: $c"; }',
    T + 'done',
    T + '[ $csame -eq 1 ] \\',
    T + T + '&& ok "V9a with the ground-cover tint ON the assembled colour sheet is byte-identical to a direct bake, on all four dim-4 chunks" \\',
    T + T + '|| bad "V9a with the ground-cover tint ON the assembled colour sheet is byte-identical to a direct bake"',
    T + '# V9b: the msn is the OPERAND that used to differ (5,524 texels on',
    T + '# Commonwealth.4.-24.24, 2026-09-09), so it is pinned in its own right and',
    T + '# not left to be inferred from the colour.',
    T + 'msame=1',
    T + 'for c in $CHUNKS; do',
    T + T + 'cmp -s "$W/run1/tex/${c}_msn.DDS" "$W/novt/tex/${c}_msn.DDS" \\',
    T + T + T + '|| { msame=0; say "msn differs: $c"; }',
    T + 'done',
    T + '[ $msame -eq 1 ] \\',
    T + T + '&& ok "V9b the assembled and direct _msn sheets are byte-identical (the chunk bake\'s edge clamp is gone)" \\',
    T + T + '|| bad "V9b the assembled and direct _msn sheets are byte-identical"',
    T + '# THE FLOOR under both, and it is not optional: two sheets also compare',
    T + '# equal when the cover pass, the tint or the whole bake is silently inert.',
    T + '# So the tint must be PRESENT -- each path\'s cover sheet must differ from',
    T + '# its OWN cover-free sheet. Measured 2026-09-10: 8 of 8 pairs differ.',
    T + 'tintlive=0',
    T + 'for c in $CHUNKS; do',
    T + T + 'cmp -s "$W/run1/tex/$c.DDS" "$W/run1nc/tex/$c.DDS" || tintlive=$((tintlive + 1))',
    T + T + 'cmp -s "$W/novt/tex/$c.DDS" "$W/novtnc/tex/$c.DDS" || tintlive=$((tintlive + 1))',
    T + 'done',
    T + 'say "cover vs no-cover: $tintlive of 8 sheet pairs differ"',
    T + '[ $tintlive -ge 2 ] \\',
    T + T + '&& ok "FLOOR the ground-cover tint actually moves these sheets, so the two identity bars above were asked of something" \\',
    T + T + '|| bad "FLOOR the ground-cover tint actually moves these sheets"',
    T + '# V9c: WHICH normal is right, measured on the DIRECT sheets alone.',
    T + '#',
    T + '# The ground is continuous, so two chunk sheets meeting at a chunk seam',
    T + '# hold texels 32 world units apart -- the same spacing as two adjacent',
    T + '# columns inside one sheet. The step ACROSS the seam is therefore',
    T + '# comparable with the ordinary interior step, and the ratio is the number.',
    T + '# The control is taken WELL INSIDE the sheet (x = 100, 200, 300, 400), not',
    T + '# one texel in: a clamped bake\'s own edge column is inside the defect, and',
    T + '# using it once made the clamped bake look better (lane VTFIX, mistake 3).',
    T + '#',
    T + '# Pinned from the ringed pyramid\'s own reading, 2026-09-09, BEFORE this',
    T + '# bake existed: E/W ratio 2.75 ringed against 4.07 clamped, N/S 2.87',
    T + '# against 3.78, a sheet\'s own edge step 1.961 against 3.310. The bars sit',
    T + '# between the two, so they discriminate rather than accommodate.',
    T + '"$PY" - "$W/novt/tex" <<\'PYEOF\'',
    'import struct, sys',
    '',
    'def c565(c):',
    '    return (((c >> 11) & 31) * 255 + 15) // 31, (((c >> 5) & 63) * 255 + 31) // 63, ((c & 31) * 255 + 15) // 31',
    '',
    'def bc1(path):',
    '    """Decode mip 0 of a DXT1 DDS. Re-typed from the format on purpose: a check',
    '    that decoded through the writer\'s own code could not fail on the writer."""',
    '    b = open(path, \'rb\').read()',
    '    assert b[:4] == b\'DDS \', path',
    '    h, w = struct.unpack_from(\'<II\', b, 12)',
    '    off = 148 if b[84:88] == b\'DX10\' else 128',
    '    px = [(0, 0, 0)] * (w * h)',
    '    p = off',
    '    for by in range((h + 3) // 4):',
    '        for bx in range((w + 3) // 4):',
    '            c0, c1 = struct.unpack_from(\'<HH\', b, p)',
    '            idx = struct.unpack_from(\'<I\', b, p + 4)[0]',
    '            p += 8',
    '            p0, p1 = c565(c0), c565(c1)',
    '            if c0 > c1:',
    '                pal = [p0, p1, tuple((2 * p0[k] + p1[k]) // 3 for k in range(3)),',
    '                       tuple((p0[k] + 2 * p1[k]) // 3 for k in range(3))]',
    '            else:',
    '                pal = [p0, p1, tuple((p0[k] + p1[k]) // 2 for k in range(3)), (0, 0, 0)]',
    '            for j in range(4):',
    '                for i in range(4):',
    '                    x, y = bx * 4 + i, by * 4 + j',
    '                    if x < w and y < h:',
    '                        px[y * w + x] = pal[(idx >> (2 * (j * 4 + i))) & 3]',
    '    return px, w, h',
    '',
    'RATIO_EW = 3.20     # ringed 2.75, clamped 4.07 (2026-09-09)',
    'RATIO_NS = 3.30     # ringed 2.87, clamped 3.78',
    'EDGE_MAX = 2.60     # a sheet\'s own last-two-columns step: 1.961 vs 3.310',
    'CTL_LO, CTL_HI = 1.20, 2.20   # interior control: 1.803/1.797 E/W, 1.603/1.606 N/S',
    '',
    'd = sys.argv[1]',
    'W = bc1(d + \'/Commonwealth.4.-24.24_msn.DDS\')',
    'E = bc1(d + \'/Commonwealth.4.-20.24_msn.DDS\')',
    'S = W',
    'N = bc1(d + \'/Commonwealth.4.-24.28_msn.DDS\')',
    '',
    'def mad(a, b):',
    '    return sum(max(abs(p[k] - q[k]) for k in range(3)) for p, q in zip(a, b)) / float(len(a))',
    '',
    'def col(t, x):',
    '    px, w, h = t',
    '    return [px[y * w + x] for y in range(h)]',
    '',
    'def row(t, y):',
    '    px, w, h = t',
    '    return [px[y * w + x] for x in range(w)]',
    '',
    'w = W[1]',
    'seam_x = mad(col(W, w - 1), col(E, 0))',
    'ctl_x = sum(mad(col(t, x), col(t, x + 1)) for t in (W, E) for x in (100, 200, 300, 400)) / 8.0',
    'edge_x = (mad(col(W, w - 2), col(W, w - 1)) + mad(col(E, 0), col(E, 1))) / 2.0',
    '# row 0 is NORTH, so the south chunk\'s row 0 meets the north chunk\'s last row',
    'seam_y = mad(row(S, 0), row(N, w - 1))',
    'ctl_y = sum(mad(row(t, y), row(t, y + 1)) for t in (S, N) for y in (100, 200, 300, 400)) / 8.0',
    'edge_y = (mad(row(S, 0), row(S, 1)) + mad(row(N, w - 2), row(N, w - 1))) / 2.0',
    'fails = 0',
    'print(\'       E/W seam %6.3f interior %6.3f ratio %5.2f (edge step %6.3f)\'',
    '      % (seam_x, ctl_x, seam_x / ctl_x if ctl_x else 0.0, edge_x))',
    'print(\'       N/S seam %6.3f interior %6.3f ratio %5.2f (edge step %6.3f)\'',
    '      % (seam_y, ctl_y, seam_y / ctl_y if ctl_y else 0.0, edge_y))',
    '# the floor first: a decode that returned a constant reads 0 everywhere and',
    '# would sail through every ratio below as 0/0',
    'if not (CTL_LO <= ctl_x <= CTL_HI) or not (CTL_LO <= ctl_y <= CTL_HI):',
    '    print(\'       FAIL the interior control is %.3f / %.3f, outside %.2f..%.2f: these\'',
    '          % (ctl_x, ctl_y, CTL_LO, CTL_HI))',
    '    print(\'            sheets are not the terrain the bars were measured on\')',
    '    fails += 1',
    'if ctl_x and seam_x / ctl_x > RATIO_EW:',
    '    print(\'       FAIL E/W seam is %.2fx the interior step, over %.2f (the edge clamp)\'',
    '          % (seam_x / ctl_x, RATIO_EW))',
    '    fails += 1',
    'if ctl_y and seam_y / ctl_y > RATIO_NS:',
    '    print(\'       FAIL N/S seam is %.2fx the interior step, over %.2f\' % (seam_y / ctl_y, RATIO_NS))',
    '    fails += 1',
    'if max(edge_x, edge_y) > EDGE_MAX:',
    '    print(\'       FAIL a sheet\\\'s own edge step is %.3f, over %.2f: its last column\'',
    '          % (max(edge_x, edge_y), EDGE_MAX))',
    '    print(\'            is anomalous against its own neighbours, which is the clamp\')',
    '    fails += 1',
    'sys.exit(1 if fails else 0)',
    'PYEOF',
    T + 'RC=$?; checks=$((checks + 1))',
    T + '[ $RC -eq 0 ] \\',
    T + T + '&& echo "  ok   V9c the direct sheets are continuous ACROSS a chunk seam, against an interior control (the block above)" \\',
    T + T + '|| { fails=$((fails + 1)); echo "  FAIL V9c the direct sheets are continuous ACROSS a chunk seam (the block above)"; }',
    T + '# The _data sheet is NOT pinned to identity and that is stated, not',
    T + '# forgotten: its wetness channel is a flow accumulation over the whole grid',
    T + '# its baker is handed, and a tile\'s grid is not a chunk\'s, so the two paths',
    T + '# cannot agree on it by construction. Lane CLAMP names it as owed.',
    T + 'if cmp -s "$W/run1/tex/$STEM""_data.DDS" "$W/novt/tex/$STEM""_data.DDS"; then',
    T + T + 'say "the _data sheets happen to be identical at this chunk"',
    T + 'else',
    T + T + 'say "the _data sheets differ (expected: the wetness domains are not the same grid)"',
    T + 'fi',
    T,
]
s = s[:a] + '\n'.join(lines) + s[b:]

out = s.encode('utf-8')
if out.count(b'\r') != cr_before:
    print('CR MOVED %d -> %d' % (cr_before, out.count(b'\r'))); sys.exit(1)
open(PATH, 'wb').write(out)
print('ok  CR %d, lines %d' % (cr_before, len(s.split('\n'))))
