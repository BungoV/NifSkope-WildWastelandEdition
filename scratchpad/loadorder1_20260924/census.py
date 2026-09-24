"""Lane LOADORDER1 gate picker, read only. Prints a few candidate paths:
  overlap  <path> <modA line#> <modB line#>   a relpath two ENABLED mods both ship
  disabled <path> <mod>                       a relpath only a DISABLED mod ships (not Data, not an enabled mod)
  bns / tg <path>                             a real mesh in BNS Trees - Main.ba2 / TrueGrass - Main.ba2
"""
import os, struct, sys, collections

BASE = 'E:/Projects/Fallout 4 Mods'
PROF = BASE + '/profiles/Default'
MODS = BASE + '/mods'
DATA = 'X:/Programs/Steam/steamapps/common/Fallout 4/Data'
DIRS = ('meshes', 'textures', 'materials')


def ba2_names(p):
    with open(p, 'rb') as f:
        _, ver, typ, n, nto = struct.unpack('<4sI4sIQ', f.read(24))
        f.seek(nto)
        out = []
        for i in range(n):
            l = struct.unpack('<H', f.read(2))[0]
            out.append(f.read(l).decode('latin1').lower().replace(chr(92), '/'))
    return out


def mod_paths(d):
    s = set()
    if not os.path.isdir(d):
        return s
    for top in os.listdir(d):
        if top.lower() in DIRS and os.path.isdir(os.path.join(d, top)):
            for r, _, fs in os.walk(os.path.join(d, top)):
                for fn in fs:
                    s.add(os.path.relpath(os.path.join(r, fn), d).lower().replace(chr(92), '/'))
        elif top.lower().endswith('.ba2'):
            try:
                s.update(ba2_names(os.path.join(d, top)))
            except Exception as e:
                print('warn', top, e)
    return s


lines = open(PROF + '/modlist.txt', encoding='utf-8').read().splitlines()
enabled, disabled = [], []
for i, l in enumerate(lines, 1):
    if l.startswith('+') and not l.endswith('_separator'):
        enabled.append((i, l[1:]))
    elif l.startswith('-') and not l.endswith('_separator'):
        disabled.append((i, l[1:]))

owner = collections.defaultdict(list)
for i, m in enabled:
    for p in mod_paths(os.path.join(MODS, m)):
        owner[p].append(i)
data = set()
for fn in os.listdir(DATA):
    if fn.lower().endswith('.ba2'):
        data.update(ba2_names(os.path.join(DATA, fn)))

ov = [(p, o) for p, o in owner.items() if len(o) == 2 and p.startswith(('meshes/', 'materials/'))]
print('overlaps (2 enabled mods, meshes/materials):', len(ov))
byPair = collections.Counter(tuple(o) for _, o in ov)
for pair, c in byPair.most_common(6):
    ex = next(p for p, o in ov if tuple(o) == pair)
    print('overlap', ex, pair, c, 'names:', lines[pair[0] - 1], '|', lines[pair[1] - 1], '| inData', ex in data)

nd = 0
for i, m in disabled:
    only = sorted(p for p in mod_paths(os.path.join(MODS, m))
                  if p not in owner and p not in data and p.startswith(('meshes/', 'textures/', 'materials/')))
    if only:
        nd += 1
        if nd <= 5:
            print('disabled', only[0], '| line', i, m, '| only-here', len(only))
    both = sorted(p for p in mod_paths(os.path.join(MODS, m)) if p in owner and p.startswith('meshes/'))
    if both and nd <= 5:
        print('disabled-and-enabled', both[0], '| disabled line', i, m, '| enabled lines', owner[both[0]])

for tag, mod, arc, pref in (('bns', 'Boston Natural Surroundings', 'BNS Trees - Main.ba2', 'meshes/bns/lod/'),
                            ('tg', 'True Grass', 'TrueGrass - Main.ba2', 'meshes/')):
    p = os.path.join(MODS, mod, arc)
    names = [n for n in ba2_names(p) if n.startswith(pref) and n.endswith('.nif')] if os.path.isfile(p) else []
    pick = [n for n in names if 'aspen01_lod_0' in n] or names
    print(tag, pick[0] if pick else '(none)', '| owners', owner.get(pick[0]) if pick else '', '| of', len(names))
