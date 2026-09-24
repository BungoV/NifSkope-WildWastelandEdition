#!/usr/bin/env python
"""PIC-RIVERBED step 2: LTEX -> TXST -> diffuse, with form ids and paths.

Read-only. Prints the chain for every LTEX painted in chunk (-20,20), and the
size/mips/mean of each diffuse DDS on disk.
"""
import json
import os
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, '..', '..'))
sys.path.insert(0, os.path.join(REPO, 'scratchpad', 'splat1_20260911'))
sys.path.insert(0, os.path.join(REPO, 'tests', 'spells'))

import offline_bake as OB                                     # noqa: E402
import splatlib as S                                          # noqa: E402
from lodgen_terrain_model import find_asset, find_material    # noqa: E402

DATA = r'E:/Tools/Fallout 4/DataUnpacked/Data'


def main():
    e = OB.esm()
    comp = json.load(open(os.path.join(HERE, 'locate.json')))['composition']
    order = [c[0] for c in comp]
    # everything painted in the chunk, ordered by coverage in the window then
    # by whole-sheet coverage
    forms = {}
    for f, r in e.ltex.items():
        forms.setdefault(r['edid'], f)

    rows = []
    log = []

    def p(s):
        print(s)
        log.append(s)

    names = order + [n for n in
                     [x[0] for x in json.load(open(os.path.join(HERE, 'locate.json')))['composition']]
                     if n not in order]
    # add every riverbed LTEX in the chunk even if tiny in the window
    for q in json.load(open(os.path.join(HERE, 'locate.json')))['quads']:
        for nm in [q['base']] + [l[0] for l in q['layers']]:
            if nm not in names:
                names.append(nm)

    p('LTEX -> TXST -> diffuse, chunk (-20,20)')
    p('')
    for nm in names:
        f = forms.get(nm)
        if f is None:
            p('%-28s  <no LTEX record>' % nm)
            continue
        rec = e.ltex[f]
        tn = rec['tnam']
        ts = e.txst.get(tn)
        if not ts:
            p('%-28s LTEX %08X  TNAM %08X  <no TXST>' % (nm, f, tn))
            continue
        src = ts['tx00'] or ('MNAM ' + ts['mnam'])
        path = None
        via = 'TX00'
        if ts['tx00']:
            path = find_asset(DATA, ts['tx00'])
            rel = ts['tx00']
        else:
            via = 'MNAM->' + ts['mnam']
            m = find_material(DATA, ts['mnam'])
            rel = None
            if m:
                d = OB._bgsm_first_dds(m)
                if d:
                    rel = d
                    path = find_asset(DATA, d)
        info = ''
        if path and os.path.exists(path):
            d = S.Dds(path)
            a = d.level(0)[:, :, :3]
            info = ('%dx%d  %d mips  %d B  mean RGB %.1f,%.1f,%.1f'
                    % (d.width, d.height, d.maxMip + 1, os.path.getsize(path),
                       a[:, :, 0].mean(), a[:, :, 1].mean(), a[:, :, 2].mean()))
        else:
            info = '<not unpacked>'
        p('%-28s LTEX %08X  TXST %08X  %s' % (nm, f, tn, via))
        p('    %-22s %s' % (rel or '?', info))
        p('    on disk: %s' % (path or '<none>'))
        rows.append(dict(edid=nm, ltex='%08X' % f, txst='%08X' % tn, via=via,
                         rel=rel, path=path, info=info))
        p('')

    with open(os.path.join(HERE, 'chain.json'), 'w') as f:
        json.dump(rows, f, indent=1)
    with open(os.path.join(HERE, 'logs', 's2.log'), 'w') as f:
        f.write('\n'.join(log) + '\n')


if __name__ == '__main__':
    main()
