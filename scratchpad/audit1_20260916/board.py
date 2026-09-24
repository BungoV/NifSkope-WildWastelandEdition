# AUDIT1: turn the gate logs into the report's board rows.
# usage: python board.py <gatesdir> [more dirs...]
import os
import re
import sys

for d in sys.argv[1:]:
    tsv = os.path.join(d, 'board.tsv')
    times = {}
    if os.path.exists(tsv):
        for line in open(tsv, encoding='utf-8'):
            f = line.rstrip('\n').split('\t')
            if len(f) >= 3:
                times[f[0]] = (f[1], f[2])
    print('== %s' % d)
    print('%-28s %5s %6s %8s %8s %7s  %s' % ('gate', 'rc', 'secs', 'checks', 'fails', 'skips', 'verdict'))
    for name in sorted(times):
        log = os.path.join(d, name + '.log')
        checks = fails = skips = ''
        verdict = ''
        if os.path.exists(log):
            t = open(log, encoding='utf-8', errors='replace').read()
            ms = re.findall(r'^(\d+) checks?, (\d+) failures?', t, re.M)
            if ms:
                checks = sum(int(a) for a, b in ms)
                fails = sum(int(b) for a, b in ms)
            else:
                ok = len(re.findall(r'^\s*ok\s', t, re.M))
                bad = len(re.findall(r'^\s*FAIL\s', t, re.M))
                checks, fails = ok + bad, bad
            skips = len(re.findall(r'^\s*SKIP\s', t, re.M))
            vs = re.findall(r'^(?:RESULT )?(PASS|FAIL)\s*$', t, re.M)
            verdict = ','.join(vs) if vs else ('rc=%s' % times[name][0])
        print('%-28s %5s %6s %8s %8s %7s  %s'
              % (name, times[name][0], times[name][1], checks, fails, skips, verdict))
    print()
