"""Crude but useful: brace/paren/bracket balance over a C++ file, skipping
comments and string/char literals.  A well-formed translation unit ends at 0
on all three.  This is not a compiler; it catches the class of edit mistake a
patch script makes (a snippet pasted one brace short) and nothing else."""
import sys

for path in sys.argv[1:]:
    s = open(path, encoding='utf-8').read()
    i = 0
    n = len(s)
    depth = {'{': 0, '(': 0, '[': 0}
    pairs = {'}': '{', ')': '(', ']': '['}
    line = 1
    firstNeg = None
    while i < n:
        c = s[i]
        if c == '\n':
            line += 1
            i += 1
            continue
        if c == '/' and i + 1 < n and s[i + 1] == '/':
            while i < n and s[i] != '\n':
                i += 1
            continue
        if c == '/' and i + 1 < n and s[i + 1] == '*':
            i += 2
            while i + 1 < n and not (s[i] == '*' and s[i + 1] == '/'):
                if s[i] == '\n':
                    line += 1
                i += 1
            i += 2
            continue
        if c in '"\'':
            q = c
            i += 1
            while i < n and s[i] != q:
                if s[i] == '\\':
                    i += 1
                elif s[i] == '\n':
                    line += 1
                i += 1
            i += 1
            continue
        if c in depth:
            depth[c] += 1
        elif c in pairs:
            depth[pairs[c]] -= 1
            if depth[pairs[c]] < 0 and firstNeg is None:
                firstNeg = (c, line)
        i += 1
    print('%-28s braces %+d parens %+d brackets %+d%s'
          % (path, depth['{'], depth['('], depth['['],
             '' if firstNeg is None else '  first unmatched %s at line %d' % firstNeg))
