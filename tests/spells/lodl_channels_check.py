#!/usr/bin/env python3
"""The assertions behind `tests/spells/lodl_channels.sh`.

`lodl_channels.sh` takes the renders, one NifSkope at a time; this reads them
back and decides. Nothing here renders and nothing here decodes a `.lodi` or a
`.lodt` -- the reader's table arrives as JSON from `lodl_channels_table.py`, so
there is one reader per format in the gate as well as in the lane.

Every check is printed as `  ok   <sentence>` / `  FAIL <sentence>`, the shape
every other WW spell uses, and the file ends `N checks, M failures` and
`PASS`/`FAIL`.

usage: lodl_channels_check.py <render dir> <table json>
"""
import json
import os
import sys

import numpy as np
from PIL import Image

# `placement` (v7) reads the same per-placement identity `identity` read before
# v7 existed, so it is graded against the same column of the reader's table.
PLACEMENT = {'identity': 'identity', 'placement': 'identity',
             'identityraw': 'identitylow', 'sky': 'sky',
             'ground': 'ground', 'seed': 'seed'}
VERTEX = {'sway': 'sway', 'selfao': 'selfao', 'ao': 'ao'}
TEXEL = {'mask-r': ['mask-r'], 'mask-g': ['mask-g'], 'mask-b': ['mask-b'],
         'mask-a': ['mask-a'], 'normal': ['normal-r', 'normal-g', 'normal-b'],
         'emissive': ['emissive-r']}
ABSENT = ('mask-a', 'emissive')
TEXTURED = ('normal', 'emissive')
ORDER = ['identity', 'placement', 'identityraw', 'sky', 'ground', 'seed', 'sway', 'selfao',
         'ao', 'mask-r', 'mask-g', 'mask-b', 'mask-a', 'emissive', 'normal']
TOL = 1.0                       # the brief's tolerance: "within 1"

checks = [0]
fails = [0]


def check(ok, text):
    checks[0] += 1
    if not ok:
        fails[0] += 1
    print('  %s %s' % ('ok  ' if ok else 'FAIL', text))
    return ok


def px(path):
    return np.asarray(Image.open(path).convert('RGB'), dtype=np.int16)


def notes(log, channel):
    want = 'WW_LODL_CHANNEL=%s:' % channel
    out = []
    with open(log, 'rb') as f:
        for raw in f:
            line = raw.decode('utf-8', 'replace').rstrip('\r\n')
            if want in line or (channel == 'ao' and 'WW_LODL_AO:' in line):
                out.append(line[line.index('WW_LODL'):])
    return out


def refusal(log):
    with open(log, 'rb') as f:
        for raw in f:
            line = raw.decode('utf-8', 'replace').rstrip('\r\n')
            if 'WW_LODL_CHANNEL: REFUSED' in line:
                return line[line.index('WW_LODL'):]
    return ''


def census_means(ns, channel):
    """The CONTENT-TEXEL census the terrain note lines carry -- the population an
    independent reader of the container counts. The other terrain line is the
    per-grid-vertex bilinear resample, which is a DIFFERENT population on
    purpose and is never compared with the reader."""
    out = []
    for n in ns:
        if 'CONTENT texels' in n and 'mean ' in n:
            out.append(float(n[n.rindex('mean ') + 5:].split()[0]))
        if channel == 'normal' and 'content texels a channel' in n:
            for part in n[n.index('channel;') + 8:].split(','):
                if 'mean ' in part:
                    out.append(float(part[part.index('mean ') + 5:].split()[0]))
    return out


def vertex_mean(ns):
    for n in ns:
        if 'CONTENT texels' in n or 'content texels a channel' in n:
            continue
        if 'mean ' in n:
            tail = n[n.rindex('mean ') + 5:].strip().rstrip(';,')
            tail = tail.split()[0].split(',')[0].rstrip(')')
            try:
                return float(tail)
            except ValueError:
                pass
        if 'constant ' in n:
            try:
                return float(n[n.rindex('constant ') + 9:].split()[0])
            except ValueError:
                pass
    return None


def count_of(ns):
    """N placements / vertices / texels the note line says it read."""
    for n in ns:
        for word in ('placements read', 'vertices read', 'bytes,', 'tiles read',
                     'texels of', 'content texels a channel', 'content texels'):
            if word in n:
                head = n[:n.index(word)].rstrip().split()
                for tok in reversed(head):
                    tok = tok.replace(',', '')
                    if tok.isdigit():
                        return int(tok)
    return 0


def main():
    d, tj = sys.argv[1], sys.argv[2]
    table = json.load(open(tj))
    base = px(os.path.join(d, 'default.png'))
    base_tex = px(os.path.join(d, 'default_tex.png'))

    # (0) the comparison itself has to be able to fire
    check(int(np.count_nonzero(np.any(base != base_tex, axis=2))) > 0,
          '(floor) the flat default and the textured default are DIFFERENT '
          'pictures, so a pixel count of 0 means "identical", not "broken reader"')

    for c in ORDER:
        img = os.path.join(d, c + '.png')
        log = os.path.join(d, c + '.log')
        if not os.path.exists(img) or not os.path.exists(log):
            check(False, '%s: the render or its log is missing' % c)
            continue
        ns = notes(log, c)
        ref = base_tex if c in TEXTURED else base
        diff = int(np.count_nonzero(np.any(px(img) != ref, axis=2)))

        # (a) a note line, and it says a population or says absent/constant BY NAME
        if c in ABSENT:
            check(any('ABSENT' in n for n in ns),
                  '(a) %s: the note line says ABSENT by name -- %s'
                  % (c, (ns[0][:110] if ns else 'NO NOTE LINE')))
        else:
            n_read = count_of(ns)
            constant = any('constant ' in n for n in ns)
            check(bool(ns) and (n_read > 0 or constant),
                  '(a) %s: note line present, %d read%s'
                  % (c, n_read, ', constant' if constant else ''))

        # (b) the render MUST differ from the default -- except for the two the
        #     bake does not carry, whose refuter is the opposite
        if c in ABSENT:
            check(diff == 0,
                  '(b) %s: absent, so its render is IDENTICAL to the default '
                  '(%d px differ, must be 0)' % (c, diff))
        else:
            check(diff > 0, '(b) %s: render differs from the default, %d px' % (c, diff))

        # (c) the note line's mean equals the reader's, within 1
        if c in ABSENT:
            check('absent' in table.get(TEXEL[c][0], {}),
                  '(c) %s: the reader agrees it is absent' % c)
        elif c in TEXEL:
            got, want = census_means(ns, c), [table.get(k, {}).get('mean')
                                              for k in TEXEL[c]]
            ok = len(got) == len(want) and all(
                w is not None and abs(g - w) <= TOL for g, w in zip(got, want))
            check(ok, '(c) %s: note census mean %s vs reader %s (tolerance %g)'
                  % (c, ['%.3f' % g for g in got],
                     ['%.3f' % w for w in want if w is not None], TOL))
        else:
            nm = vertex_mean(ns)
            tm = table.get(PLACEMENT.get(c) or VERTEX.get(c), {}).get('mean')
            check(nm is not None and tm is not None and abs(nm - tm) <= TOL,
                  '(c) %s: note mean %s vs reader %s (tolerance %g)'
                  % (c, 'none' if nm is None else '%.3f' % nm,
                     'none' if tm is None else '%.3f' % tm, TOL))

    # (c-floor) the tolerance must still REFUSE a wrong pairing, or it proves nothing
    id_mean = vertex_mean(notes(os.path.join(d, 'identityraw.log'), 'identityraw'))
    check(id_mean is not None and abs(id_mean - table['sky']['mean']) > TOL,
          '(c floor) the same tolerance REFUSES identityraw\'s mean against sky\'s '
          '(%.3f vs %.3f) -- so (c) is not vacuous'
          % (id_mean or -1, table['sky']['mean']))

    # (f) THE VERSION-6 FALLBACK (v7). This fixture is a version-6 pair: it has no
    # group table and no per-vertex sky stream. Both new channels must therefore
    # fall back, and -- the part that matters -- must SAY SO. A viewer that drew
    # the fallback silently would be indistinguishable from one that drew the
    # feature, which is the whole defect class root MISTAKES 05:1x records.
    idn = refusal(os.path.join(d, 'identity.log')) or ''
    if not idn:
        idn = ' '.join(notes(os.path.join(d, 'identity.log'), 'identity') or [])
    check('no group table' in idn,
          '(f) on a version-6 file `identity` NAMES its fallback to the placement '
          'identity -- %s' % (idn[:140] or 'NO NOTE LINE'))
    skn = ' '.join(notes(os.path.join(d, 'sky.log'), 'sky') or [])
    check('PLACEMENT BYTE' in skn or 'no per-vertex stream' in skn,
          '(f) on a version-6 file `sky` NAMES the placement byte as what served it '
          '-- %s' % (skn[:140] or 'NO NOTE LINE'))
    # and with nothing to fall back FROM, the two must be the same picture
    i = open(os.path.join(d, 'identity.png'), 'rb').read()
    pl = open(os.path.join(d, 'placement.png'), 'rb').read()
    check(i == pl, '(f) with no group table, `identity` and `placement` are the SAME '
                   'picture (%d B vs %d B) -- on a version-7 file they must differ, '
                   'which tests/spells/lodi_v7.sh G4 asserts' % (len(i), len(pl)))

    # (d) the way back: WW_LODL_AO=1 with no channel is byte-for-byte `ao`
    a = open(os.path.join(d, 'ao.png'), 'rb').read()
    w = open(os.path.join(d, 'ao_way_back.png'), 'rb').read()
    check(a == w, '(d) WW_LODL_AO=1 with WW_LODL_CHANNEL unset is byte-identical to '
                  'WW_LODL_CHANNEL=ao (%d B vs %d B)' % (len(a), len(w)))

    # (e) an unknown name refuses BY NAME and draws nothing different
    r = refusal(os.path.join(d, 'unknown.log'))
    check('nosuchchannel' in r and 'REFUSED' in r,
          '(e) an unknown name is refused BY NAME -- %s' % (r[:120] or 'NO REFUSAL LINE'))
    check('Known names:' in r,
          '(e) the refusal lists the known names')
    u = open(os.path.join(d, 'unknown.png'), 'rb').read()
    b = open(os.path.join(d, 'default.png'), 'rb').read()
    check(u == b, '(e) the refused render is byte-identical to the default '
                  '(%d B vs %d B)' % (len(u), len(b)))

    print('%d checks, %d failures' % (checks[0], fails[0]))
    print('PASS' if fails[0] == 0 else 'FAIL')
    return 0 if fails[0] == 0 else 1


if __name__ == '__main__':
    sys.exit(main())
