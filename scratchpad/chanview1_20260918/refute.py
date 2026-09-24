#!/usr/bin/env python3
"""Lane CHANVIEW1 step 3 -- the refuter, MEASURED.

For every channel: the pixel difference between its render and the DEFAULT
render of the same framing (the floor that fails on an unwired channel, root
MISTAKES 05:1x), and the note line's mean beside the `.lodi`/`.lodo`/sheet
reader's mean from step 1.

A channel the bake does not carry (`mask-a`, `emissive` here) MUST come back
identical to the default -- that is its refuter, and it is stated as such
rather than dressed up as a difference.

usage: refute.py <refuter dir> <table json> [<table json> ...]
"""
import json
import os
import sys

import numpy as np
from PIL import Image

# channel -> (the note-line key to grep, the table row it must equal, kind)
PER_PLACEMENT = {
    'identity': 'identity',
    'identityraw': 'identitylow',
    'sky': 'sky',
    'ground': 'ground',
    'seed': 'seed',
}
PER_VERTEX = {
    'sway': 'sway',
    'selfao': 'selfao',
    'ao': 'ao',
}
PER_TEXEL = {
    'mask-r': ['mask-r'],
    'mask-g': ['mask-g'],
    'mask-b': ['mask-b'],
    'mask-a': ['mask-a'],
    'normal': ['normal-r', 'normal-g', 'normal-b'],
    'emissive': ['emissive-r'],
}
ABSENT = ('mask-a', 'emissive')
ORDER = ['identity', 'identityraw', 'sky', 'ground', 'seed', 'sway', 'selfao',
         'ao', 'mask-r', 'mask-g', 'mask-b', 'mask-a', 'emissive', 'normal']


def px(path):
    return np.asarray(Image.open(path).convert('RGB'), dtype=np.int16)


def note_of(log, channel):
    """The channel's own note line, out of the render log."""
    want = 'WW_LODL_CHANNEL=%s:' % channel
    out = []
    with open(log, 'rb') as f:
        for raw in f:
            line = raw.decode('utf-8', 'replace').rstrip('\r\n')
            if want in line or (channel == 'ao' and 'WW_LODL_AO:' in line):
                out.append(line[line.index('WW_LODL'):])
    return out


def texel_means(notes, channel):
    """The CONTENT-TEXEL census the terrain note lines carry, which is the
    number an independent reader of the container counts (the vertex resample
    on the other line is deliberately a different population)."""
    out = []
    for n in notes:
        if 'CONTENT texels' in n and 'mean ' in n:
            out.append(float(n[n.rindex('mean ') + 5:].split()[0]))
        if channel == 'normal' and 'content texels a channel' in n:
            tail = n[n.index('channel;') + 8:]
            for part in tail.split(','):
                if 'mean ' in part:
                    out.append(float(part[part.index('mean ') + 5:].split()[0]))
                elif 'constant ' in part:
                    out.append(float(part[part.index('constant ') + 9:].split()[0]))
    return out


def mean_from(notes):
    for n in notes:
        if 'CONTENT texels' in n or 'content texels a channel' in n:
            continue        # the census line, not the uploaded-vertex line
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


def main():
    d = sys.argv[1]
    table = {}
    for j in sys.argv[2:]:
        table.update(json.load(open(j)))

    base = px(os.path.join(d, 'default.png'))
    base_tex = px(os.path.join(d, 'default_tex.png'))
    rows = []
    for c in ORDER:
        img = os.path.join(d, c + '.png')
        if not os.path.exists(img):
            rows.append((c, 'NO PICTURE', '', '', ''))
            continue
        ref = base_tex if c in ('normal', 'emissive') else base
        a = px(img)
        diff = int(np.count_nonzero(np.any(a != ref, axis=2)))
        notes = note_of(os.path.join(d, c + '.log'), c)
        if c == 'ao':
            notes = [n for n in notes if 'v6 scene vertex AO' in n] or notes
        nm = mean_from(notes)
        if c in PER_TEXEL:
            got = texel_means(notes, c)
            want = [table.get(k, {}).get('mean') for k in PER_TEXEL[c]]
            nm = got[0] if got else nm
            tm = want[0] if want else None
        else:
            key = PER_PLACEMENT.get(c) or PER_VERTEX.get(c)
            tm = table.get(key, {}).get('mean')
            got = want = None
        if c in ABSENT:
            verdict = 'ABSENT ok' if diff == 0 and notes else 'ABSENT BUT NOT SAID'
        elif diff == 0:
            verdict = 'NOT WIRED'
        elif nm is None:
            verdict = 'NO NOTE MEAN'
        elif tm is None:
            verdict = 'no reader mean'
        elif c in PER_TEXEL:
            bad = [k for k, g, w in zip(PER_TEXEL[c], got, want)
                   if w is None or abs(g - w) > 1.0]
            verdict = 'ok (%d texel channel(s))' % len(got) if not bad                 else 'MEAN MISMATCH ' + ','.join(bad)
        elif abs(nm - tm) <= 1.0:
            verdict = 'ok'
        else:
            verdict = 'MEAN MISMATCH'
        rows.append((c, diff, nm, tm, verdict))

    w = max(len(r[0]) for r in rows)
    print('%-*s %12s %14s %14s  %s' % (w, 'channel', 'px differ', 'note mean',
                                       'reader mean', 'verdict'))
    for c, diff, nm, tm, v in rows:
        print('%-*s %12s %14s %14s  %s'
              % (w, c, diff,
                 '-' if nm is None else '%.3f' % nm,
                 '-' if tm is None else '%.3f' % tm, v))

    # the controls that are not channels
    for name, ref in (('unknown', 'default'), ('ao_way_back', 'ao')):
        p = os.path.join(d, name + '.png')
        if os.path.exists(p):
            same = open(p, 'rb').read() == open(os.path.join(d, ref + '.png'), 'rb').read()
            print('%-*s %12s   %s' % (w, name, 'bytes ==' if same else 'BYTES DIFFER',
                                      'identical to %s.png' % ref))


if __name__ == '__main__':
    main()
