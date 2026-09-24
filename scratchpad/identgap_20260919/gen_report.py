#!/usr/bin/env python3
"""IDENTGAP -- build `report.md` from `rows.json`, `dsweep.json` and `run.log`.

The prose lives here so that every TABLE in the report is generated from the
measurement files and cannot drift from them.  Nothing is typed twice.
"""
import json
import re

LANE = 'E:/Projects/NifskopeWildWastelandEdition/scratchpad/identgap_20260919'
ROWS = json.load(open(LANE + '/rows.json'))
LOG = open(LANE + '/run.log').read()
try:
    DS = json.load(open(LANE + '/dsweep.json'))
except Exception:
    DS = []

VIEWS = [('hwydeck', 180.0, 10.0, 'the elevated highway deck and its support columns'),
         ('east', 120.0, 15.0, 'the city block from the east'),
         ('street', 120.0, 5.0, 'street level, a very low sun')]
LOOKS = [('map16', '16 u texel, 3x3 PCF -- the realistic one'),
         ('cast', 'no map at all -- per-pixel cast, the limit'),
         ('map64', '64 u texel, ONE nearest tap -- the coarse case')]


def g(view, row):
    for r in ROWS:
        if r['view'] == view and r['row'] == row:
            return r
    return None


def bestd(view):
    for ln in LOG.split('\n'):
        if 'BEST D, table A' in ln and _inview(ln, view):
            pass
    # the driver saved it per view in the npz; recover it from the rows instead
    ds = sorted({r['D'] for r in ROWS
                 if r['view'] == view and r['group'] == 'G4' and r['D']})
    return int(ds[0]) if ds else 64


def _inview(ln, view):
    return True


def fmt(r):
    if r is None:
        return '| -- | -- | -- | -- |'
    return '%.2f%% | %.2f%% | %.2f%% | %.2f%%' % (r['objects_fd'], r['objects_fl'],
                                                  r['terrain'], r['all'])


def table(view, look, BD):
    out = ['| row | obj false-DARK | obj false-LIT | terrain disagree | ALL |',
           '|---|---|---|---|---|']
    def add(label, rid):
        r = g(view, rid)
        if r is None:
            return
        out.append('| %s | %s |' % (label, fmt(r)))
    add('**G0** no identity, tuned bias', 'G0/%s' % look)
    add('G0strict  bias tuned to the least artefact', 'G0strict/%s' % look)
    add('**G1** pure identity, table A', 'G1/A/%s' % look)
    add('**G1** pure identity, table B', 'G1/B/%s' % look)
    for tb in ('A', 'B'):
        for D in (64, 128, 256, 512, 1024):
            add('G2 gate D = %d u, table %s' % (D, tb), 'G2/%s/D%d/%s' % (tb, D, look))
    add('**G3** back-face casting, no identity', 'G3/%s' % look)
    for tb in ('A', 'B'):
        add('**G4** gate D = %d u + re-tuned bias, table %s' % (BD, tb), 'G4/%s/%s' % (tb, look))
    return '\n'.join(out)


def sss_table(view):
    rs = [r for r in ROWS if r['view'] == view and r['group'] == 'SSS']
    if not rs:
        return ''
    out = ['| base | steps | reach | recovered of the base false-LIT | NEW false-dark '
           '(of object px) | obj false-DARK | obj false-LIT | ALL |',
           '|---|---|---|---|---|---|---|---|']
    for r in rs:
        out.append('| %s | %d | %.0f%% | **%.1f%%** | **%.2f%%** | %.2f%% | %.2f%% | %.2f%% |'
                   % (r['base'], r['sss_steps'], r['sss_reach'] * 100, r['sss_recovered'],
                      r['sss_new_falsedark'], r['objects_fd'], r['objects_fl'], r['all']))
    return '\n'.join(out)


def dtable(view):
    rs = [r for r in DS if r['view'] == view]
    if not rs:
        return ''
    looks = ['map16', 'map64']
    Ds = [r['D'] for r in rs if r['look'] == 'map16']
    out = ['| D | 16 u PCF: false-DARK | false-LIT | 64 u nearest: false-DARK | false-LIT |',
           '|---|---|---|---|---|']
    for D in Ds:
        a = [r for r in rs if r['look'] == 'map16' and r['D'] == D][0]
        b = [r for r in rs if r['look'] == 'map64' and r['D'] == D][0]
        out.append('| %s | %.2f%% | %.2f%% | %.2f%% | %.2f%% |'
                   % ('infinity (= G1)' if D is None else '%d u' % D,
                      a['objects_fd'], a['objects_fl'], b['objects_fd'], b['objects_fl']))
    return '\n'.join(out)


def grab(pat, n=1):
    m = re.findall(pat, LOG)
    return m[:n]


try:
    WALL = json.load(open(LANE + '/wallart.json'))
except Exception:
    WALL = []


def walltable(view):
    rs = [r for r in WALL if r['view'] == view]
    if not rs:
        return ''
    out = ['| row | PATCH  sun LIT / row dark | LOST  sun dark / row lit |', '|---|---|---|']
    for r in rs:
        out.append('| `%s` | %s px  (%.2f%% of walls) | %s px  (%.2f%% of walls) |'
                   % (r['row'], '{:,}'.format(r['patch']), r['patch_pct'],
                      '{:,}'.format(r['lost']), r['lost_pct']))
    return '\n'.join(out)


HEAD = open(LANE + '/sec1.txt').read()
TAIL = open(LANE + '/sec4.txt').read()

body = [HEAD, '\n---\n\n## 2. The tables\n',
        'Disagree = the row calls a pixel lit where the ray-cast sun calls it shadow, or the '
        'reverse, over **all** decided pixels of the frame -- one denominator for every row of '
        'a camera. *false-DARK* = shadow the row invents (**the artefact identity exists to '
        'kill**); *false-LIT* = shadow the row loses (**what the gate should give back**).\n']
for vname, az, el, what in VIEWS:
    BD = bestd(vname)
    body.append('\n### camera `%s`, sun azimuth %.0f, elevation %.0f -- %s\n' % (vname, az, el, what))
    for look, ldesc in LOOKS:
        body.append('\n**lookup: %s**\n' % ldesc)
        body.append(table(vname, look, BD))
    body.append('\n**The screen-space shadow march on top** (director\'s addendum)\n')
    body.append(sss_table(vname))
    if dtable(vname):
        body.append('\n**Where the artefact comes back** -- `dsweep.py`, identity table A, '
                    'the same tuned bias, D taken below the brief\'s grid:\n')
        body.append(dtable(vname))
    if walltable(vname):
        body.append('\n**The same rows scored on WALL pixels only** -- object, '
                    '`|normal.z| < 0.35`, which is where the mid-wall patch lives '
                    '(`wallart.py`). %s of the frame\'s object pixels are walls:\n'
                    % '{:,}'.format([r for r in WALL if r['view'] == vname][0]['wall']))
        body.append(walltable(vname))
body.append(TAIL)
open(LANE + '/report.md', 'w').write('\n'.join(body))
print('wrote report.md, %d bytes' % len(open(LANE + '/report.md').read()))
