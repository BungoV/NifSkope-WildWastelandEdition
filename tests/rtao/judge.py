"""Lane RTAO1: judge one `-no-gui rtao` stdout against the bars of tests/rtao/cli_gate.sh. Exit 1 on a miss."""
import re
import sys

text = open(sys.argv[1], encoding='utf-8', errors='replace').read()


def num(pat):
    m = re.search(pat, text)
    return float(m.group(1)) if m else None


tex = num(r'texels with a surface (\d+)')
sagree = num(r'surface agree ([\d.]+) %')
vis_t = num(r'mean visibility twin ([\d.]+)')
g_over = num(r'rtao green: texels over 1/n ([\d.]+) %')
g_mean = num(r'rtao green: .*?\|d\| mean ([\d.]+)')
r_over = num(r'rtao red: texels over 1/n ([\d.]+) %')
r_mean = num(r'rtao red: .*?\|d\| mean ([\d.]+)')
checks = [
    ('texels with a surface >= 5000', tex is not None and tex >= 5000, tex),
    ('surface agree >= 99.9 %', sagree is not None and sagree >= 99.9, sagree),
    ('mean twin visibility <= 0.99 (the scene occludes)', vis_t is not None and vis_t <= 0.99, vis_t),
    ('kernel over 1/n <= 0.5 %', g_over is not None and g_over <= 0.5, g_over),
    ('kernel |d| mean <= 0.005', g_mean is not None and g_mean <= 0.005, g_mean),
    ('red misses the over bar (> 0.5 %)', r_over is not None and r_over > 0.5, r_over),
    ('red misses the mean bar (> 0.005)', r_mean is not None and r_mean > 0.005, r_mean),
]
ok = True
for name, good, val in checks:
    print('%s %s: %s' % ('PASS' if good else 'FAIL', name, val))
    ok = ok and good
sys.exit(0 if ok else 1)
