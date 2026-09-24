"""AUDIT1 step 6: the gate board re-run on the FIXED exe, beside step 1's board
on the audited exe, gate for gate.

The before column is not retyped: it is parsed out of the report's own section
1.1 table, so a row that moved cannot move by transcription. The after column is
counted out of the logs the re-run wrote, by the same two instruments section
1.1 used -- each gate's own `N checks, M failures` summary where it prints one,
and the ok/FAIL line counts either way -- because a gate whose summary and whose
lines disagree must stay visible rather than be averaged.

usage: python mk_board_after.py
"""
import os
import re

S = 'E:/Projects/NifskopeWildWastelandEdition/scratchpad/audit1_20260916'
G = os.path.join(S, 'gates_after')
REPORT = os.path.join(S, 'lane_audit1_report.md')

# --- the before board, parsed out of section 1.1 -----------------------------
before = {}
rowre = re.compile(r'^\|\s*`(\w+)`\s*\|\s*(\d+)\s*\|\s*(\d+)\s*\|([^|]*)\|'
                   r'\s*([-\d]+)\s*\|\s*(\d+)\s*\|([^|]*)\|\s*$')
inside = False
for line in open(REPORT, encoding='utf-8', errors='replace'):
    if line.startswith('### 1.1'):
        inside = True
        continue
    if inside and line.startswith('### 1.2'):
        break
    if not inside:
        continue
    m = rowre.match(line.rstrip('\n'))
    if m:
        before[m.group(1)] = {
            'rc': int(m.group(2)), 's': int(m.group(3)),
            'sum': m.group(4).strip(),
            'ok': m.group(5).strip(), 'fail': int(m.group(6)),
            'whose': m.group(7).strip(),
        }

# --- the after board ---------------------------------------------------------
secs = {}
tsv = os.path.join(G, 'board.tsv')
if os.path.exists(tsv):
    for line in open(tsv, encoding='utf-8', errors='replace'):
        p = line.rstrip('\n').split('\t')
        if len(p) == 3:
            secs[p[0]] = (int(p[1]), int(p[2]))

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
    rc, sec = secs.get(name, ('?', '?'))
    short = name.replace('lodgen_', '')
    b = before.get(short)
    rows.append((short, rc, sec, len(sums), tot, tfail, oks, fails, b))

print('| gate | rc before / after | s before / after | checks/fails after | ok / FAIL lines'
      ' before | ok / FAIL lines after | moved |')
print('|---|---|---|---|---|---|---|')
moved = []
for (short, rc, sec, nsum, tot, tfail, oks, fails, b) in rows:
    if b is None:
        print('| `%s` | ? / %s | ? / %s | %d block(s): %d/%d | -- | %d / %d | NOT IN THE BEFORE BOARD |'
              % (short, rc, sec, nsum, tot, tfail, oks, fails))
        continue
    bf = b['fail']
    verdict = 'same'
    if rc != b['rc'] or fails != bf:
        verdict = '**MOVED**'
        moved.append((short, b['rc'], rc, bf, fails))
    print('| `%s` | %d / %s | %d / %s | %d block(s): %d/%d | %s / %d | %d / %d | %s |'
          % (short, b['rc'], rc, b['s'], sec, nsum, tot, tfail,
             b['ok'], bf, oks, fails, verdict))

print('')
print('%d gate(s) in the after board, %d of them moved' % (len(rows), len(moved)))
for (n, rcb, rca, fb, fa) in moved:
    print('  %-16s rc %s -> %s, FAIL lines %d -> %d' % (n, rcb, rca, fb, fa))
