"""lane FIX1031B (WATERLOOK1 overhaul, LATE ITEM 7): the gated water looks ship ON, and the shipped look stays as the red.

Reads src/gl/celllooks.cpp (a source check: the rows' table is data). PASS = each of the six overhaul rows (Water clear
colour, Water reflections, Water sun glare, Ocean waves, Water soft edge, Shore foam) has defOn true under a NEW settings
key (an old OFF must not stick), and wwLookWaterShippedRed names exactly those six behind WW_CELL_WATER_LOOK_RED=shipped.
Red: main 5422be6c's celllooks.cpp (every row off by default) must FAIL.
python -I defaults_gate.py [--src FILE]
"""
import argparse
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
TREE = os.path.abspath(os.path.join(HERE, '..', '..'))
ap = argparse.ArgumentParser()
ap.add_argument('--src', default=os.path.join(TREE, 'src', 'gl', 'celllooks.cpp'))
a = ap.parse_args()
src = open(a.src, encoding='latin-1').read()
ROWS = {'Water clear colour': 'WaterClear', 'Water reflections': 'WaterRefl', 'Water sun glare': 'WaterGlint',
        'Ocean waves': 'WaterOcean', 'Water soft edge': 'WaterSoftEdge', 'Shore foam': 'ShoreFoam'}
OLD = {'Water clear colour': 'WW/CellWaterClear', 'Water reflections': 'WW/CellWaterRefl',
       'Water sun glare': 'WW/CellWaterGlint', 'Ocean waves': 'WW/CellWaterOcean', 'Water soft edge': 'WW/CellWaterEdge',
       'Shore foam': 'WW/CellWaterFoam'}
ok = True
for label, enum in ROWS.items():
    m = re.search(r'\{ "%s", "([^"]+)"(, (true|false))? \}' % re.escape(label), src)
    on = bool(m and m.group(3) == 'true')
    newkey = bool(m and m.group(1) != OLD[label])
    print('%-20s key %-24s default %s %s' % (label, m.group(1) if m else '?', 'ON' if on else 'off',
                                           'new key' if newkey else 'OLD KEY'))
    ok &= on and newkey
red = re.search(r'bool wwLookWaterShippedRed\( WwLook l \)\s*\{(.*?)\n\}', src, re.S)
names = set(re.findall(r'WwLook::(\w+)', red.group(1))) if red else set()
redok = bool(red) and names == set(ROWS.values()) and 'WW_CELL_WATER_LOOK_RED' in red.group(1) and '"shipped"' in red.group(1)
print('red WW_CELL_WATER_LOOK_RED=shipped covers %s: %s' % (sorted(names) if names else 'nothing', 'ok' if redok else 'NO'))
ok &= redok and '!wwLookWaterShippedRed( l )' in src
print('VERDICT %s' % ('PASS' if ok else 'FAIL'))
sys.exit(0 if ok else 1)
