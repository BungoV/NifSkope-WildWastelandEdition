"""Compile-check every Python heredoc embedded in a harness, without running it.

`nifskope-ww-lodgen` says to do this after every edit to a harness: a syntax
error inside a `<<'PYEOF'` block is invisible to `bash -n` and only shows up as
a lost run, minutes into a chain that has already spawned NifSkope.

    python scratchpad/cardortho_20260910/check_harness_py.py <harness.sh> ...
"""
import re
import sys

bad = 0
for path in sys.argv[1:]:
    src = open(path, encoding='utf-8').read()
    blocks = re.findall(r"<<'PYEOF'\n(.*?)\nPYEOF\n", src, re.S)
    print('%s: %d embedded python blocks' % (path, len(blocks)))
    if src.count("<<'PYEOF'") != src.count('\nPYEOF\n'):
        print('  FAIL heredoc markers unbalanced: %d open, %d close'
              % (src.count("<<'PYEOF'"), src.count('\nPYEOF\n')))
        bad += 1
    for k, b in enumerate(blocks):
        try:
            compile(b, '%s <block %d>' % (path, k), 'exec')
            print('  ok   block %d compiles (%d lines)' % (k, b.count('\n') + 1))
        except SyntaxError as e:
            print('  FAIL block %d: %s' % (k, e))
            bad += 1
sys.exit(1 if bad else 0)
