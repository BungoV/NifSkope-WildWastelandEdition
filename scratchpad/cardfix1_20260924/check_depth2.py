# Is every IMPOSTORDEPTH2 fix script's NEW text present in this worktree? (read-only check)
import re, glob, os, sys
W = 'E:/Projects/NifskopeWWE-cardfix1/'
D = 'E:/Projects/NifskopeWildWastelandEdition/scratchpad/impostordepth2_20260923/'
res = []
def mypatch(path, edits, *a, **k):
    p = W + path
    if not os.path.exists(p):
        res.append((cur, path, 'FILE MISSING')); return
    s = open(p, 'rb').read().decode('utf-8')
    for old, new in edits:
        n_new = s.count(new); n_old = s.count(old)
        res.append((cur, path, 'new=%d old=%d' % (n_new, n_old)))
for f in sorted(glob.glob(D + 'fix*.py')):
    cur = os.path.basename(f)
    src = open(f, encoding='utf-8').read()
    src = re.sub(r'\ndef patch\(', '\ndef _unused_patch(', src)
    g = {'__name__': 'x', 'patch': mypatch}
    src = src.replace("ROOT = 'E:/Projects/NifskopeWildWastelandEdition/'", "ROOT = '" + W + "'")
    try:
        exec(compile(src, f, 'exec'), g)
    except Exception as e:
        res.append((cur, '-', 'EXC %s: %s' % (type(e).__name__, str(e)[:80])))
bad = [r for r in res if not r[2].startswith('new=1') and not r[2].startswith('new=2')]
print(len(res), 'edits checked;', len(bad), 'not present')
for r in bad[:30]: print(r)
