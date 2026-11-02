"""Lane RTREFL1: judge one `-no-gui rtrefl` stdout against the bars of tests/rtrefl/cli_gate.sh. Exit 1 on a miss."""
import re
import sys

text = open(sys.argv[1], encoding='utf-8', errors='replace').read()


def num(pat):
    m = re.search(pat, text)
    return float(m.group(1)) if m else None


rays = num(r'texels with a ray (\d+)')
twin = num(r'twin hits (\d+)')
g_agree = num(r'rtrefl green: hit agree ([\d.]+) %')
g_col = num(r'rtrefl green: .*?on the twin\'s hits ([\d.]+) %')
r_col = num(r'rtrefl red: .*?on the twin\'s hits ([\d.]+) %')
checks = [
    ('texels with a ray >= 500', rays is not None and rays >= 500, rays),
    ('twin hits >= 500', twin is not None and twin >= 500, twin),
    ('kernel hit agreement >= 99 %', g_agree is not None and g_agree >= 99.0, g_agree),
    ('kernel colour within 5 % >= 95 %', g_col is not None and g_col >= 95.0, g_col),
    ('red colour within 5 % < 50 % (the red fails)', r_col is not None and r_col < 50.0, r_col),
]
ok = True
for name, good, val in checks:
    print('%s %s: %s' % ('PASS' if good else 'FAIL', name, val))
    ok = ok and good
sys.exit(0 if ok else 1)
