"""Compile-check the two Python heredocs inside tests/spells/lodgen_farring.sh
without running the harness (this lane may not run NifSkope).  A syntax error
in an embedded script would otherwise only show up on the overseer's build."""
import re
import sys

src = open('tests/spells/lodgen_farring.sh', encoding='utf-8').read()
blocks = re.findall(r"<<'PYEOF'\n(.*?)\nPYEOF\n", src, re.S)
print('embedded python blocks: %d' % len(blocks))
assert len(blocks) == 2, 'expected two, found %d' % len(blocks)
for k, b in enumerate(blocks):
    compile(b, '<block %d>' % k, 'exec')
    print('  block %d compiles (%d lines)' % (k, b.count('\n') + 1))

# every heredoc opened is closed, and the quoted form is used so the shell
# does not expand $ inside the Python
assert src.count("<<'PYEOF'") == src.count('\nPYEOF\n'), 'heredoc markers unbalanced'
print('heredocs balanced')
