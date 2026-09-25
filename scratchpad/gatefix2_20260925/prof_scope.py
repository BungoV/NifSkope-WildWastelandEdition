"""prof_scope.py <export.reg> <out.reg> <scope> [drop-prefix ...]
Rewrite a `reg export` of bungo's NifSkope 2.0 key into a WW_SETTINGS_SCOPE key
(HKCU\\Software\\NifTools\\NifSkope 2.0 <scope>) so a harness can render under a COPY of
his profile without writing to it. Optional drop-prefixes remove whole subkeys / values
(value names matched as '<subkey>/<name>') for bisecting which setting moves a picture."""
import sys
src, dst, scope = sys.argv[1:4]
drops = [a for a in sys.argv[4:] if not a.startswith('+')]
keeps = [a[1:] for a in sys.argv[4:] if a.startswith('+')]  # '+prefix': keep ONLY these values
t = open(src, 'rb').read().decode('utf-16')
base = '[HKEY_CURRENT_USER\\Software\\NifTools\\NifSkope 2.0'
assert t.count(base) >= 1
out = []
key = None
skipping_value = False
for line in t.split('\r\n'):
    if line.startswith('['):
        key = line[len(base):-1].lstrip('\\')
        line = base + ' ' + scope + line[len(base):]
        skipping_value = False
    elif line.startswith('"') and key is not None:
        name = line[1:line.index('"', 1)]
        full = (key + '\\' + name) if key else name
        f2 = full.replace('\\', '/')
        skipping_value = any(f2.startswith(d) for d in drops) or \
            (bool(keeps) and not any(f2.startswith(k) for k in keeps))
        if skipping_value:
            continue
    elif skipping_value and line.startswith('  '):
        continue  # continuation line of a dropped hex value
    else:
        skipping_value = False
    out.append(line)
open(dst, 'wb').write(('\r\n'.join(out)).encode('utf-16'))
print('keys', sum(1 for l in out if l.startswith('[')), 'values', sum(1 for l in out if l.startswith('"')))
