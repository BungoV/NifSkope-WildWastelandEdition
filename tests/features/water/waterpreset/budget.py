"""lane FIX1101A (bungo 10-31 RULED: "water that looks good and is not expensive to render"): the water preset's GPU cost per
step at 1080p against the budget (Low 0.4, Medium 0.8, High 1.2, Ultra 1.8 ms), from the MEASURED cost files the cost
harness wrote (tests/features/water/watercost/watercost.py: the real programs through the driver on this machine's RTX 5070 Ti; re-run
it after any water shader change and commit its output):
  cost = water share x ( colour pass + water ray pass ) + the march and blurs (half size, the whole view: counted whole)
  sea   (spot 5, no flow: one wave read)          at a frame ALL water (the worst case)
  river (spot 6, flowing: two wave reads a pixel) at 75 % water (REASONED: a river view holds banks and sky)
Red --red today: no preset (every step draws the default rows) and the water ray pass from the full normal (today's
code): every step costs today's default.
usage: python -I budget.py [--red today]"""
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
COST = os.path.join(HERE, '..', 'watercost')
RED = len(sys.argv) > 2 and sys.argv[1] == '--red' and sys.argv[2] == 'today'
BUDGET = {'low': 0.4, 'medium': 0.8, 'high': 1.2, 'ultra': 1.8}
march = float(re.search(r'cell_ssrwater stages 1-4 ([0-9.]+) ms', open(os.path.join(COST, 'cost_ssr_march_1080.txt')).read()).group(1))
fails = 0
for spot, share in (('sea', 1.0), ('river', 0.75)):
    txt = open(os.path.join(COST, 'cost_%s_1080.txt' % spot)).read()
    c = {m.group(1).strip(): float(m.group(2)) for m in re.finditer(r'^COST (.+?)\s+([0-9.]+)\s+\(rounds', txt, re.M)}
    pre = {m.group(1): (float(m.group(2)), float(m.group(3))) for m in
           re.finditer(r'^PRESET (\w+)\s+colour pass ([0-9.]+) ray pass ([0-9.]+)', txt, re.M)}
    for step in ('low', 'medium', 'high', 'ultra'):
        col, ray = pre[step]
        if RED:
            col, ray = c['default + ssr read'], c['ssr ray pass (default rows)']
        full = col + ray + march
        at = share * (col + ray) + march
        ok = at <= BUDGET[step]
        fails += not ok
        print('BUDGET %-5s %-6s %.3f ms at %3.0f %% water (full frame %.3f; colour %.3f, rays %.3f, march %.3f) vs %.1f  %s' % (
            spot, step, at, share * 100, full, col, ray, march, BUDGET[step], 'PASS' if ok else 'FAIL'))
print('VERDICT %s' % ('PASS' if fails == 0 else 'FAIL (%d)' % fails))
