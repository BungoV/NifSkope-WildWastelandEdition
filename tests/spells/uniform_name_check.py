"""lane CLOUDLOOK1: every uniform name handed to a Program setter is a string literal.
The location cache (src/gl/glcontext.cpp UniformLocationMapItem::hashFunction) keys on the name's ADDRESS: a name built
on the heap per call (QByteArray::constData, std::string::c_str, qPrintable) can come back at the same address for a
different name, and the setter then writes the first name's location. cloudvol.cpp set cvTint[0..2] that way: all three
went to cvTint[0].
python -I uniform_name_check.py <tree> [--rev <git rev>]   (--rev reads the files of that commit: the red, origin/main)
A call marked `// uniname-red` on its line is a kept red (WW_..._RED word) and is counted apart."""
import os, re, subprocess, sys

tree = sys.argv[1]
rev = sys.argv[sys.argv.index('--rev') + 1] if '--rev' in sys.argv else None
CALL = re.compile(r'(?:->|\.)\s*(uni(?:[1-4][a-z_]*|Location|Sampler)\w*|setMat3)\s*\(\s*([^,)]*)')
CALL_FREE = re.compile(r'\bsetMat3\s*\(\s*[^,]+,\s*[^,]+,\s*([^,)]*)')
BAD = re.compile(r'constData\s*\(|\.data\s*\(|c_str\s*\(|qPrintable|toLatin1|toUtf8|QByteArray|std::string')


def files():
    if rev:
        out = subprocess.run(['git', '-C', tree, 'ls-tree', '-r', '--name-only', rev, 'src'], capture_output=True,
                             text=True).stdout.split()
        for f in out:
            if f.endswith(('.cpp', '.h', '.hpp')):
                yield f, subprocess.run(['git', '-C', tree, 'show', '%s:%s' % (rev, f)], capture_output=True).stdout.decode(
                    'utf-8', 'replace')
    else:
        for root, _, fs in os.walk(os.path.join(tree, 'src')):
            for f in fs:
                if f.endswith(('.cpp', '.h', '.hpp')):
                    p = os.path.join(root, f)
                    yield os.path.relpath(p, tree).replace('\\', '/'), open(p, 'rb').read().decode('utf-8', 'replace')


bad, kept, calls = [], [], 0
for name, text in files():
    for i, line in enumerate(text.split('\n'), 1):
        for m in list(CALL.finditer(line)) + list(CALL_FREE.finditer(line)):
            calls += 1
            arg = m.group(m.lastindex)
            if BAD.search(arg):
                (kept if 'uniname-red' in line else bad).append('%s:%d: %s' % (name, i, line.strip()[:120]))
for b in bad:
    print('  heap name', b)
for k in kept:
    print('  kept red ', k)
ok = calls > 100 and not bad
print('UNIFORM-NAME-CHECK %s: %d setter calls, %d heap names, %d kept reds%s' % (
    'PASS' if ok else 'FAIL', calls, len(bad), len(kept), ' (rev %s)' % rev if rev else ''))
sys.exit(0 if ok else 1)
