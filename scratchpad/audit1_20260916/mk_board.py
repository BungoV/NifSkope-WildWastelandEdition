"""AUDIT1 step 1: the gate board, read out of the logs the runs themselves
wrote.  Counts come from each gate's own summary line where it prints one, and
otherwise from counting its ok/FAIL lines; both are shown so a gate whose
summary and whose lines disagree is visible rather than averaged.
"""
import os
import re
import sys

G = 'E:/Projects/NifskopeWildWastelandEdition/scratchpad/audit1_20260916/gates1'
SECS = {}
for line in open(os.path.dirname(G) + '/gates1_runner.log', encoding='utf-8', errors='replace'):
    m = re.match(r'DONE (\S+) rc=(\d+) secs=(\d+)', line)
    if m:
        SECS[m.group(1)] = (int(m.group(2)), int(m.group(3)))
# the four run one at a time afterwards, plus byte_gate
SECS.update({'lodgen_byte_gate': (1, 1969), 'lodgen_octahedral': (1, 69),
             'lodgen_panel_run': (0, 45), 'lodgen_tree_sway': (0, 8),
             'lodgen_water_subdiv': (0, 4)})

rows = []
for fn in sorted(os.listdir(G)):
    if not fn.endswith('.log'):
        continue
    name = fn[:-4]
    s = open(os.path.join(G, fn), encoding='utf-8', errors='replace').read()
    oks = len(re.findall(r'^\s*(?:ok|OK)\s{2,}', s, re.M))
    fails = len(re.findall(r'^\s*FAIL\s', s, re.M))
    sums = re.findall(r'(\d+) checks?, (\d+) failures?', s)
    tot = sum(int(a) for a, _ in sums)
    tfail = sum(int(b) for _, b in sums)
    verdict = 'FAIL' if 'RESULT FAIL' in s else ('PASS' if 'RESULT PASS' in s or 'PASS' in s else '?')
    rc, secs = SECS.get(name, ('?', '?'))
    rows.append((name, rc, secs, tot, tfail, oks, fails, len(sums), verdict))

print('| gate | rc | s | summary lines (checks/fails) | ok lines | FAIL lines |')
print('|---|---|---|---|---|---|')
for r in rows:
    name, rc, secs, tot, tfail, oks, fails, nsum, verdict = r
    print('| `%s` | %s | %s | %d block(s): %d/%d | %d | %d |'
          % (name.replace('lodgen_', ''), rc, secs, nsum, tot, tfail, oks, fails))
print('')
print('%d gates' % len(rows))
sys.exit(0)
