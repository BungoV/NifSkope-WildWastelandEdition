"""Compile every <<'PYEOF' block of a harness (skill nifskope-ww-lodgen: a broken block reads like a failing check)."""
import re
import sys

bad = 0
for path in sys.argv[1:]:
    s = open(path, encoding='utf-8').read()
    for i, blk in enumerate(re.findall(r"<<'PYEOF'\n(.*?)\nPYEOF", s, re.S)):
        try:
            compile(blk, '<%s block %d>' % (path, i), 'exec')
        except SyntaxError as e:
            print('%s block %d line %s: %s' % (path, i, e.lineno, e.msg))
            bad += 1
print('blocks bad: %d' % bad)
sys.exit(1 if bad else 0)
