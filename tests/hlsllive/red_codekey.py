"""HLSLFIX2: gate of stamps.code_key (the A4 verdict cache's harness key). On edited copies of the real harness files (folder
CODEKEY_TMP, default tests/hlsllive/_codekey, reused; do not commit it): a comment / docstring / blank-line edit must KEEP the key;
a code edit (a constant, a string, an import) must MOVE it. The second half is the red: a key that ignored code edits would
print CODEKEY FAIL.
   python -I -X utf8 tests/hlsllive/red_codekey.py"""
import os
import sys
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import stamps

FILES = ['a4_diff.py', 'a2_iface.py', 'glctx.py', 'spirv_probe.py', 'remap.py', 'stamps.py']
SAME = [   # (file, old, new): edits that do not change what runs
    ('a4_diff.py', 'import ctypes, json, os, sys, zlib\n', '# a new comment line\nimport ctypes, json, os, sys, zlib  # trailing\n\n'),
    ('a4_diff.py', '"""A4 differential gate (HLSLBULK1 R5)', '"""A4 differential gate (HLSLBULK1 R5, docstring edited)'),
    ('stamps.py', 'def sha(*parts):\n', '\n\ndef sha(*parts):\n'),
]
MOVE = [   # edits that change what runs
    ('a4_diff.py', 'SZ = 32\n', 'SZ = 33\n'),
    ('a4_diff.py', "'%s|%d' % (name, seed)", "'%s|%d ' % (name, seed)"),
    ('a2_iface.py', 'import ', 'import re as _unused_re\nimport ', ),
]


def key_with(edits):
    real = stamps.HERE
    tmp = os.environ.get('CODEKEY_TMP') or os.path.join(real, '_codekey')
    os.makedirs(tmp, exist_ok=True)
    for f in FILES:
        s = open(os.path.join(real, f), encoding='utf-8').read()
        for ef, old, new in edits:
            if ef == f:
                assert s.count(old) >= 1, (f, old)
                s = s.replace(old, new, 1)
        open(os.path.join(tmp, f), 'w', encoding='utf-8', newline='\n').write(s)
    stamps.HERE = tmp
    try:
        return stamps.code_key(FILES)
    finally:
        stamps.HERE = real


base = key_with([])
ok = True
for e in SAME:
    k = key_with([e])
    print('same %-12s %-40r %s' % (e[0], e[1][:40], 'kept' if k == base else 'MOVED'))
    ok &= k == base
for e in MOVE:
    k = key_with([e])
    print('code %-12s %-40r %s' % (e[0], e[1][:40], 'moved' if k != base else 'KEPT'))
    ok &= k != base
print('CODEKEY OK %d edits' % (len(SAME) + len(MOVE)) if ok else 'CODEKEY FAIL')
sys.exit(0 if ok else 1)
