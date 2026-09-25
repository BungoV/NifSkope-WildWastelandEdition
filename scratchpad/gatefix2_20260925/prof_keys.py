"""prof_keys.py <export.reg> [subkey-prefix] -- count values per subkey, or list value names under a prefix."""
import sys, collections
t = open(sys.argv[1], 'rb').read().decode('utf-16')
pref = sys.argv[2] if len(sys.argv) > 2 else None
k = None
c = collections.Counter()
for l in t.split('\r\n'):
    if l.startswith('['):
        k = l.split('NifSkope 2.0', 1)[1].rstrip(']').lstrip('\\').replace('\\', '/')
    elif l.startswith('"'):
        name = l[1:l.index('"', 1)]
        if pref is None:
            c[k] += 1
        elif (k + '/' + name).startswith(pref):
            print(k + '/' + name, l[l.index('=') + 1:][:80])
for kk, v in c.items():
    print(repr(kk), v)
