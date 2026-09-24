import re, sys
for name in sys.argv[1:]:
    s = open(name, encoding='utf-8').read()
    blks = re.findall(r"<<'PYEOF'\n(.*?)\nPYEOF", s, re.S)
    bad = 0
    for i, blk in enumerate(blks):
        try:
            compile(blk, '<block %d>' % i, 'exec')
        except SyntaxError as e:
            bad += 1
            print('%s block %d line %s: %s' % (name, i, e.lineno, e.msg))
    print('%s: %d blocks, %d bad' % (name, len(blks), bad))
