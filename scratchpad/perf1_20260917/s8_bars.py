#!/usr/bin/env python3
"""PERF1 step 8 -- turn the four measured bakes into bars.json.

Reads `s8_bars.txt`, which s8_measure.sh wrote, and nothing else. Every number
on a bar therefore comes out of a bake this lane ran on this machine, and the
file it came from is named in the chart's subtitle.
"""
import json
import os
import re

HERE = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.join(HERE, 's8_bars.txt')

STAGES = ('landscape', 'meshes', 'textures', 'impostors')

rows = {}
tag = None
for line in open(SRC, encoding='utf-8', errors='replace'):
    m = re.match(r'TAG=(\S+) REGION=(\S+)\(.*WALL_MS=(\d+)', line)
    if m:
        tag = m.group(1)
        rows[tag] = {'wall': int(m.group(3)) / 1000.0}
        continue
    if tag and 'stage times:' in line:
        for s in STAGES:
            mm = re.search(r'%s ([0-9.]+) s' % s, line)
            if mm:
                rows[tag][s] = float(mm.group(1))
        mm = re.search(r'library: census ([0-9.]+) s, models ([0-9.]+) s, '
                       r'ladder ([0-9.]+) s', line)
        if mm:
            rows[tag]['models'] = float(mm.group(2))
            rows[tag]['ladder'] = float(mm.group(3))

spec = []
for rn, human, cells in (('A', 'region A -- 9 chunks, --terrain-region -24 16 -13 27 --dim 4', '12x12'),
                         ('B', 'region B -- 16 chunks, --terrain-region -24 16 -9 31 --dim 4', '16x16')):
    b = rows.get('b8_%s_before' % rn)
    a = rows.get('b8_%s_after' % rn)
    if not b or not a:
        print('missing rows for region', rn, sorted(rows))
        continue
    stages = [(s, b.get(s, 0.0), a.get(s, 0.0)) for s in STAGES]
    stages.append(('WALL', b['wall'], a['wall']))
    spec.append({
        'title': 'FO4CS bake stage times, %s' % human,
        'sub': 'shipped defaults both sides (--threads 0 --chunk-threads 1 '
               '--road-detail 1); before = release/NifSkope.before_perf1.exe, '
               'after = this lane; measured in s8_bars.txt, seconds',
        'rows': stages,
        'file': 'stage_times_region_%s.png' % rn,
    })

json.dump(spec, open(os.path.join(HERE, 'bars.json'), 'w'), indent=1)
for s in spec:
    print(s['file'], [(r[0], r[1], r[2]) for r in s['rows']])
