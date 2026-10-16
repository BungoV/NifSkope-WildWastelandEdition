#!/usr/bin/env python3
"""Prove a change touched comments only.

For every changed C++/GLSL/.prog file: strip comments (respecting strings), drop blank lines and trailing
blanks, and require the result to equal the same file at the base revision -- line endings included, so a
CRLF code line that turned LF fails. Python files: ast.dump must match, plus the same code-line compare.

    python3 tests/spells/comments_only_check.py [--base REV] [files...]
    python3 tests/spells/comments_only_check.py --self-test      # the red: one altered token must FAIL

Default base is HEAD~ against the working tree; default files are those changed between the two.
"""
import argparse
import ast
import io
import os
import subprocess
import sys
import tokenize

C_LIKE = {'.c', '.cc', '.cpp', '.cxx', '.h', '.hh', '.hpp', '.inl', '.glsl', '.frag', '.vert', '.geom',
          '.comp', '.tesc', '.tese'}
HASH_COMMENT = {'.prog'}


def strip_c_comments(text):
    """Remove // and /* */ comments; strings, char literals and raw strings are kept verbatim."""
    out = []
    i, n = 0, len(text)
    while i < n:
        c = text[i]
        if c == '/' and i + 1 < n and text[i + 1] == '/':
            # a line comment ends at the newline; a backslash-continued one swallows the next line too
            j = i
            while j < n and text[j] not in '\r\n':
                j += 1
                if text[j - 1] == '\\' and j < n and text[j] in '\r\n':
                    j += 2 if text[j:j + 2] == '\r\n' else 1
            i = j
        elif c == '/' and i + 1 < n and text[i + 1] == '*':
            j = text.find('*/', i + 2)
            if j < 0:
                raise ValueError('unterminated /* comment')
            # a block comment separates tokens; its line breaks stay so the code keeps its line layout
            body = text[i:j + 2]
            out.append(' ' + ''.join('\r\n' if k and body[k - 1] == '\r' else '\n'
                                     for k in range(len(body)) if body[k] == '\n'))
            i = j + 2
        elif c == 'R' and i + 1 < n and text[i + 1] == '"' and (i == 0 or not (text[i - 1].isalnum() or text[i - 1] == '_')):
            k = text.find('(', i + 2)
            delim = text[i + 2:k]
            end = text.find(')' + delim + '"', k)
            if k < 0 or end < 0:
                raise ValueError('unterminated raw string')
            end += len(delim) + 2
            out.append(text[i:end])
            i = end
        elif c in '"\'':
            j = i + 1
            while j < n and text[j] != c:
                if text[j] == '\\':
                    j += 1
                elif text[j] == '\n':
                    break   # a lone apostrophe (digit separator, #error text) ends at the line
                j += 1
            out.append(text[i:j + 1])
            i = j + 1
        else:
            out.append(c)
            i += 1
    return ''.join(out)


def strip_hash_comments(text):
    return '\n'.join(l.split('#', 1)[0] for l in text.split('\n'))


def strip_py_comments(text):
    lines = text.splitlines(keepends=True)
    cuts = {}
    for tok in tokenize.generate_tokens(io.StringIO(text.replace('\r\n', '\n')).readline):
        if tok.type == tokenize.COMMENT:
            cuts[tok.start[0] - 1] = tok.start[1]
    return ''.join(l[:cuts[k]] + ('\r\n' if l.endswith('\r\n') else '\n' if l.endswith('\n') else '')
                   if k in cuts else l for k, l in enumerate(lines))


def code_lines(text):
    """Non-blank lines, trailing blanks removed, each keeping its own line ending."""
    res = []
    for l in text.split('\n'):
        cr = l.endswith('\r')
        body = l[:-1] if cr else l
        body = body.rstrip(' \t')
        if body.strip():
            res.append(body + ('\\r' if cr else ''))
    return res


def stripped(path, data):
    text = data.decode('utf-8')
    ext = os.path.splitext(path)[1].lower()
    if ext in C_LIKE:
        return code_lines(strip_c_comments(text))
    if ext in HASH_COMMENT:
        return code_lines(strip_hash_comments(text))
    if ext == '.py':
        return code_lines(strip_py_comments(text))
    return None


def compare(path, old, new):
    """Return a list of problems (empty = PASS)."""
    probs = []
    if old.startswith(b'\xef\xbb\xbf') != new.startswith(b'\xef\xbb\xbf'):
        probs.append('BOM changed')
    try:
        new.decode('utf-8')
    except UnicodeDecodeError:
        probs.append('not utf-8 any more')
        return probs
    # trimming never adds lines, so neither ending may grow in count (catches an LF line typed into a CRLF file)
    crlf_o, crlf_n = old.count(b'\r\n'), new.count(b'\r\n')
    lf_o, lf_n = old.count(b'\n') - crlf_o, new.count(b'\n') - crlf_n
    if crlf_n > crlf_o or lf_n > lf_o:
        probs.append('line endings: CRLF %d->%d, LF %d->%d' % (crlf_o, crlf_n, lf_o, lf_n))
    a, b = stripped(path, old), stripped(path, new)
    if a is None:
        return ['not a checked type']
    if a != b:
        for k, (x, y) in enumerate(zip(a, b)):
            if x != y:
                probs.append('code differs at stripped line %d: %r != %r' % (k + 1, x[:100], y[:100]))
                break
        else:
            probs.append('stripped line count %d != %d' % (len(a), len(b)))
    if path.endswith('.py'):
        try:
            if ast.dump(ast.parse(old)) != ast.dump(ast.parse(new)):
                probs.append('ast.dump differs')
        except SyntaxError as e:
            probs.append('syntax error: %s' % e)
    return probs


def git(*args):
    return subprocess.run(['git'] + list(args), check=True, capture_output=True).stdout


def self_test():
    """Red: alter one token in a copy of a real comment-heavy file; the check must FAIL. Green: a pure comment edit PASSes."""
    ok = True
    cases = {
        'x.cpp': (b'int a = 1; // one\r\nconst char* s = "http://x"; /* two */\r\n',
                  b'int a = 1;\r\nconst char* s = "http://x";\r\n',
                  b'int a = 2;\r\nconst char* s = "http://x";\r\n'),
        'x.glsl': (b'// a\nfloat f = 0.5; /* b\n c */ float g;\n',
                   b'float f = 0.5; /* b */\n float g;\n',
                   b'float f = 0.6; /* b */\n float g;\n'),
        'x.py': (b'x = 1  # one\n# two\ny = "#no"\n',
                 b'x = 1\ny = "#no"\n',
                 b'x = 1\ny = "#No"\n'),
    }
    for name, (old, green, red) in cases.items():
        g, r = compare(name, old, green), compare(name, old, red)
        print('%s green %s, red %s' % (name, 'PASS' if not g else 'FAIL', 'FAIL (expected)' if r else 'PASS (WRONG)'))
        ok &= not g and bool(r)
    crlf = compare('x.cpp', b'int a;\r\n// c\r\nint b;\r\n', b'int a;\r\n// c\n// d\nint b;\r\n')
    print('crlf->lf red', 'FAIL (expected)' if crlf else 'PASS (WRONG)')
    ok &= bool(crlf)
    # a real tracked file, one token altered in a copy
    real = 'src/gl/celllights.cpp'
    if os.path.exists(real):
        data = open(real, 'rb').read()
        alt = data.replace(b'return', b'retrun', 1)
        r = compare(real, data, alt)
        print('%s one token altered: %s' % (real, 'FAIL (expected)' if r else 'PASS (WRONG)'))
        ok &= bool(r)
    print('SELF-TEST', 'PASS' if ok else 'FAIL')
    return ok


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--base', default='HEAD~')
    ap.add_argument('--head', default=None, help='compare against this revision instead of the working tree')
    ap.add_argument('--self-test', action='store_true')
    ap.add_argument('files', nargs='*')
    o = ap.parse_args()
    if o.self_test:
        sys.exit(0 if self_test() else 1)
    files = o.files
    if not files:
        rng = [o.base] + ([o.head] if o.head else [])
        files = [f for f in git('diff', '--name-only', *rng).decode().split('\n') if f]
    bad = 0
    for f in files:
        old = git('show', '%s:%s' % (o.base, f))
        new = git('show', '%s:%s' % (o.head, f)) if o.head else open(f, 'rb').read()
        probs = compare(f, old, new)
        print('%s %s%s' % ('PASS' if not probs else 'FAIL', f, '' if not probs else ': ' + '; '.join(probs)))
        bad += bool(probs)
    print('%d files, %d FAIL -> %s' % (len(files), bad, 'PASS' if not bad and files else 'FAIL'))
    sys.exit(1 if bad or not files else 0)


if __name__ == '__main__':
    main()
