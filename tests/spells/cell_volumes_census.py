#!/usr/bin/env python3
"""The whole-map Space census (lane VOLFIT1): reads every `.space` file under a bake root (the census window's
`space` step, or `volumes` runs) and the step's `space_census*.tsv` rows, and prints the totals and the
borderline list: the rooms whose sky share sits within the band of the sky-share threshold, the buildings bungo's
one question is about. Independent of NifSkope: plain JSON reading.

  python cell_volumes_census.py <root> [--out list.tsv] [--skymax 0.25] [--band 0.05]
"""
import glob
import json
import os
import re
import sys


def main():
    a = sys.argv[1:]
    if not a:
        print(__doc__)
        return 2
    root = a[0]
    out = a[a.index('--out') + 1] if '--out' in a else None
    skymax = float(a[a.index('--skymax') + 1]) if '--skymax' in a else 0.25
    band = float(a[a.index('--band') + 1]) if '--band' in a else 0.05
    files = sorted(glob.glob(os.path.join(root, '**', '*.space'), recursive=True))
    tot = {'files': 0, 'unreadable': 0, 'exterior cells': 0, 'interiors': 0, 'rooms': 0, 'shapes': 0, 'thresholds': 0,
           'doors': 0, 'windows': 0, 'portals': 0, 'openings': 0, 'outside thresholds': 0, 'hand rooms': 0,
           'borderline (file)': 0}
    shape_types = {}
    cover = []
    border = []
    for f in files:
        try:
            sp = json.load(open(f))
        except (OSError, ValueError):
            tot['unreadable'] += 1
            continue
        tot['files'] += 1
        tot['interiors' if sp.get('interior') else 'exterior cells'] += 1
        place = (sp.get('worldspace') or 'Interiors') + '/' + str(sp.get('cell'))
        for r in sp.get('rooms', []):
            tot['rooms'] += 1
            tot['shapes'] += len(r['shapes'])
            for s in r['shapes']:
                shape_types[s['type']] = shape_types.get(s['type'], 0) + 1
            if r['fit'].get('hand'):
                tot['hand rooms'] += 1
            else:
                cover.append(r['fit']['coverage'])
            c = r.get('classify', {})
            if c.get('borderline'):
                tot['borderline (file)'] += 1
            sky = c.get('sky')
            if sky is not None and abs(sky - skymax) <= band:
                border.append((abs(sky - skymax), place, r['room'], sky, c.get('wall'), c.get('openness'), c.get('back'),
                               r['anchor'], len(r['shapes']), r['fit']['coverage'], ' '.join(r.get('evidence', []))))
        for t in sp.get('thresholds', []):
            tot['thresholds'] += 1
            k = {'door': 'doors', 'window': 'windows', 'portal': 'portals'}.get(t['opening'], 'openings')
            tot[k] += 1
            if 'outside' in t['between']:
                tot['outside thresholds'] += 1
    ms, rebuilt, skipped = [], 0, 0
    gi_rows = []   # the GI fed from the Space rooms (WW_CELL_VOLUMES_GI=1 with the bake step): per load
    for tsv in glob.glob(os.path.join(root, 'space_census*.tsv')):
        for i, line in enumerate(open(tsv)):
            w = line.rstrip('\n').split('\t')
            if i == 0 or len(w) < 9:
                continue
            if w[6] == 'skipped':
                skipped += 1
            else:
                rebuilt += 1
            if w[7].lstrip('-').isdigit() and int(w[7]) >= 0:
                ms.append(int(w[7]))
            m = re.search(r'mapped (\d+), unmapped (\d+), interior where the room grid had outdoors (\d+)', w[9]) \
                if len(w) > 9 else None
            if m:
                gi_rows.append((w[0], int(m.group(1)), int(m.group(2)), int(m.group(3))))
    print('SPACE CENSUS %s' % root)
    for k, v in tot.items():
        print('  %-20s %d' % (k, v))
    print('  shape types          %s' % ', '.join('%s %d' % kv for kv in sorted(shape_types.items())))
    if cover:
        cover.sort()
        print('  coverage             min %.3f, median %.3f, under 0.92: %d' % (cover[0], cover[len(cover) // 2],
                                                                            sum(1 for c in cover if c < 0.92)))
    if ms:
        ms.sort()
        print('  stage ms             loads %d (rebuilt %d, skipped %d), sum %d, median %d, max %d' % (
            len(ms), rebuilt, skipped, sum(ms), ms[len(ms) // 2], ms[-1]))
    if gi_rows:
        print('  GI from Space        loads %d: rooms mapped %d, unmapped %d (want 0), wide-door rooms now interior %d' % (
            len(gi_rows), sum(r[1] for r in gi_rows), sum(r[2] for r in gi_rows), sum(r[3] for r in gi_rows)))
        for r in gi_rows:
            if r[2]:
                print('    unmapped: %s %d' % (r[0], r[2]))
    border.sort()
    print('  borderline (sky within %.2f of %.2f): %d' % (band, skymax, len(border)))
    rows = ['place\troom\tsky\twall\topenness\tback\tanchor\tshapes\tcoverage\tevidence']
    for b in border:
        rows.append('%s\t%s\t%.3f\t%s\t%s\t%s\t%s\t%d\t%.3f\t%s' % (
            b[1], b[2], b[3], b[4], b[5], b[6], ','.join('%.0f' % x for x in b[7]), b[8], b[9], b[10]))
    for r in rows[1:21]:
        print('    ' + r)
    if out:
        with open(out, 'w') as f:
            f.write('\n'.join(rows) + '\n')
        print('  list: %s' % out)
    return 0


if __name__ == '__main__':
    sys.exit(main())
