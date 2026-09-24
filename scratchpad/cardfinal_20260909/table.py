#!/usr/bin/env python3
"""The per-base table for the lane report, straight out of measure_perframe.json."""
import json, os

NAMES = {
    '00038599': 'TreeBlasted01', '000393cd': 'TreeBlasted02', '0003a28b': 'TreeHero01',
    '0003e08d': 'TreeBlasted05', '0003e08f': 'TreeMapleblasted07', '0003e0d1': 'TreeMapleForest7',
    '00049532': 'TreeBlasted04', '0004a073': 'TreeMapleForest1', '0004a074': 'TreeMapleForest2',
    '0004a075': 'TreeMapleForest3', '0004d93b': 'TreeMapleblasted01', '000503b6': 'TreeMapleblasted02',
    '000531ae': 'TreeMapleblasted04', '000531b3': 'TreeMapleblasted05', '000a7206': 'TreeBlasted01Lichen',
    '000d9ca8': 'TreeElmForest01', '000d9ca9': 'TreeElmForest02', '0012154f': 'BurntTreeUpright03',
    '00121550': 'BurntTreeUpright02',
}

HERE = os.path.dirname(os.path.abspath(__file__))
d = json.load(open(os.path.join(HERE, 'measure_perframe.json')))
keys = list(d)
before = {r['id']: r for r in d[keys[0]]}          # cards_gap under its own law
beforeNew = {r['id']: r for r in d[keys[1]]}       # cards_gap under the new cap
after = {r['id']: r for r in d[keys[2]]}           # cards_perframe

print('| base | frame B -> A | one view fills x, B -> A | mips B -> A | border alpha B (own cap) -> B (new cap) -> A | frame narrower x / y |')
print('|---|---|---|---|---|---|')
bt = at = 0
for k in sorted(after):
    b, bn, a = before[k], beforeNew[k], after[k]
    bt += b['sheet_area']; at += a['sheet_area']
    fr = '%dx%d' % tuple(b['frame'])
    if b['frame'] != a['frame']:
        fr = '**%dx%d -> %dx%d**' % (b['frame'][0], b['frame'][1], a['frame'][0], a['frame'][1])
    gx = 100 * (1 - a['fit'][0] / a['fit'][2]) if a.get('fit') else -1
    gy = 100 * (1 - a['fit'][1] / a['fit'][3]) if a.get('fit') else -1
    print('| `%s` %s | %s | %.3f -> %.3f | %d -> %d | %d -> %d -> %d | %.1f%% / %.1f%% |'
          % (k, NAMES.get(k, '?'), fr, b['fill_single'][0], a['fill_single'][0],
             b['mips'], a['mips'], b['max_alpha_bleed'], bn['max_alpha_bleed'],
             a['max_alpha_bleed'], gx, gy))
print()
print('sheet area %s -> %s texels (%.1f%%); frame shapes %d -> %d'
      % (format(bt, ','), format(at, ','), 100.0 * (at - bt) / bt,
         len({tuple(r['frame']) for r in before.values()}),
         len({tuple(r['frame']) for r in after.values()})))
print('worst per-frame centring error, texels: %.1f before, %.1f now'
      % (max(max(r['centre_off']) for r in before.values()),
         max(max(r['centre_off']) for r in after.values())))
print('sets with any neighbour alpha at a border: %d / %d / %d  (before under its own cap, before under the new cap, now)'
      % (sum(1 for r in before.values() if r['max_alpha_bleed'] > 0),
         sum(1 for r in beforeNew.values() if r['max_alpha_bleed'] > 0),
         sum(1 for r in after.values() if r['max_alpha_bleed'] > 0)))
print('clamped frames over the library: %d ; frameoff lines per set: %s'
      % (sum(r.get('clamped') or 0 for r in after.values()),
         sorted({r['nframeoff'] for r in after.values()})))
