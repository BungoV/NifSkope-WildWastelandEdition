import re, sys, glob, os
os.chdir(r'E:/Projects/NifskopeWildWastelandEdition')
bad = 0
for f in sorted(glob.glob('tests/spells/lodgen_*.sh')):
    s = open(f, encoding='utf-8', errors='replace').read()
    blocks = re.findall(r"<<'PYEOF'\n(.*?)\nPYEOF", s, re.S)
    ok = 0
    for i, blk in enumerate(blocks):
        try:
            compile(blk, '<%s block %d>' % (f, i), 'exec')
            ok += 1
        except SyntaxError as e:
            bad += 1
            print('SYNTAX %s block %d line %s: %s' % (f, i, e.lineno, e.msg))
    print('%-40s %d/%d py blocks ok' % (os.path.basename(f), ok, len(blocks)))
# standalone python helpers too
for f in sorted(glob.glob('tests/spells/lodgen_*.py')) + ['tests/spells/lodb_read.py', 'tests/spells/lodj_read.py']:
    try:
        compile(open(f, encoding='utf-8', errors='replace').read(), f, 'exec')
    except SyntaxError as e:
        bad += 1
        print('SYNTAX %s line %s: %s' % (f, e.lineno, e.msg))
print('BADBLOCKS', bad)
