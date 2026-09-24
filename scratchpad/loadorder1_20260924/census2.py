"""Read only: a LOOSE path two enabled mods both ship with DIFFERENT bytes (so a swap flips the bytes, not just the name)."""
import os, hashlib, collections
BASE = 'E:/Projects/Fallout 4 Mods'
MODS = BASE + '/mods'
lines = open(BASE + '/profiles/Default/modlist.txt', encoding='utf-8').read().splitlines()
enabled = [(i, l[1:]) for i, l in enumerate(lines, 1) if l.startswith('+')]
own = collections.defaultdict(list)
for i, m in enabled:
    d = os.path.join(MODS, m)
    for top in ('meshes', 'materials', 'textures', 'Meshes', 'Materials', 'Textures'):
        t = os.path.join(d, top)
        if not os.path.isdir(t):
            continue
        for r, _, fs in os.walk(t):
            for fn in fs:
                full = os.path.join(r, fn)
                own[os.path.relpath(full, d).lower().replace(chr(92), '/')].append((i, m, full))
n = 0
for p, o in sorted(own.items()):
    if len(o) != 2:
        continue
    h = [hashlib.sha1(open(f, 'rb').read()).hexdigest()[:10] for _, _, f in o]
    if h[0] != h[1]:
        n += 1
        if n <= 6:
            print(p, '|', o[0][0], o[0][1], h[0], '|', o[1][0], o[1][1], h[1])
print('loose pairs with different bytes:', n)
