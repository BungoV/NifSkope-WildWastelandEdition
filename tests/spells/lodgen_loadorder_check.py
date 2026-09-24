"""Checker for tests/spells/lodgen_loadorder.sh (lane LOADORDER1, 2026-09-24).

Re-derives his MO2 load order INDEPENDENTLY of the exe -- from modlist.txt,
plugins.txt, the mods folder, the overwrite folder and the game Data folder --
and compares it with what `lodgen --mo2-profile ... --print-source` printed.
Prints one `ok   ...` or `FAIL ...` line per check; the spell counts them.

  python lodgen_loadorder_check.py g1 <print-source output> <profile> <mods> <data>
  python lodgen_loadorder_check.py probe <probe output> <field> <expected suffix> <label>
  python lodgen_loadorder_check.py swap <modlist in> <modlist out> <mod A> <mod B>
  python lodgen_loadorder_check.py record <bake-record output> <profile> <mods> <data>
  python lodgen_loadorder_check.py untick <print-source output> <plugins.txt in> <plugins.txt out>
"""
import os
import sys

DLC = ['Fallout4.esm', 'DLCRobot.esm', 'DLCworkshop01.esm', 'DLCCoast.esm', 'DLCworkshop02.esm',
       'DLCworkshop03.esm', 'DLCNukaWorld.esm', 'DLCUltraHighResolution.esm']


def ok(msg):
    print('ok   ' + msg)


def bad(msg):
    print('FAIL ' + msg)


def norm(p):
    return os.path.normcase(os.path.normpath(p.replace('/', os.sep))).lower()


def lines_of(path):
    with open(path, 'rb') as f:
        return f.read().decode('utf-8', 'replace').replace('\r', '').split('\n')


def expected(profile, mods, data):
    """(plugins as full paths, stack entries, enabled mods top-down, disabled mods, n star lines)"""
    ml = lines_of(os.path.join(profile, 'modlist.txt'))
    enabled, disabled = [], []
    for l in ml:
        l = l.strip()
        if not l or l[0] not in '+-' or l.endswith('_separator'):
            continue
        (enabled if l[0] == '+' else disabled).append(l[1:])
    enabled = [m for m in enabled if os.path.isdir(os.path.join(mods, m))]
    overwrite = os.path.normpath(os.path.join(profile, '..', '..', 'overwrite'))
    have_ow = os.path.isdir(overwrite)

    plugins = [os.path.join(data, n) for n in DLC if os.path.isfile(os.path.join(data, n))]
    ccc = os.path.join(data, '..', 'Fallout4.ccc')
    if os.path.isfile(ccc):
        for n in lines_of(ccc):
            n = n.strip()
            if n and os.path.isfile(os.path.join(data, n)) and n.lower() not in {os.path.basename(p).lower() for p in plugins}:
                plugins.append(os.path.join(data, n))
    masters = len(plugins)
    stars = [l.strip()[1:].strip() for l in lines_of(os.path.join(profile, 'plugins.txt')) if l.strip().startswith('*')]
    for n in stars:
        if n.lower() in {os.path.basename(p).lower() for p in plugins}:
            continue
        cands = ([os.path.join(overwrite, n)] if have_ow else []) + [os.path.join(mods, m, n) for m in enabled] + [os.path.join(data, n)]
        hit = next((c for c in cands if os.path.isfile(c)), None)
        plugins.append(hit if hit else '(missing) ' + n)
    stack = [data] + [os.path.join(mods, m) for m in reversed(enabled)] + ([overwrite] if have_ow else [])
    return plugins, stack, enabled, disabled, stars, masters


def field_lines(out, prefix):
    """the values of `<prefix> N: value` lines, in N order"""
    got = {}
    for l in out:
        if l.startswith(prefix + ' ') and ': ' in l:
            head, val = l.split(': ', 1)
            idx = head[len(prefix) + 1:]
            if idx.isdigit():
                got[int(idx)] = val
    return [got[i] for i in sorted(got)]


def g1(outfile, profile, mods, data):
    out = lines_of(outfile)
    plugins, stack, enabled, disabled, stars, masters = expected(profile, mods, data)
    got = field_lines(out, 'plugin')
    print('  expected %d plugins (%d masters + %d plugins.txt stars), exe printed %d' % (len(plugins), masters, len(stars), len(got)))
    if got and os.path.basename(got[0]).lower() == 'fallout4.esm' and os.path.isabs(got[0]):
        ok('G1 plugin 0 is Fallout4.esm as a full path (%s)' % got[0])
    else:
        bad('G1 plugin 0 is not Fallout4.esm as a full path (%s)' % (got[0] if got else 'none'))
    dlc_got = [os.path.basename(p) for p in got[1:8]]
    dlc_want = [os.path.basename(p) for p in plugins[1:8]]
    if dlc_got == dlc_want and all(n.lower().startswith('dlc') for n in dlc_got):
        ok('G1 the DLC masters come next, in the engine order: ' + ' '.join(dlc_got))
    else:
        bad('G1 DLC masters: got %s want %s' % (dlc_got, dlc_want))
    tail = got[masters:]
    star_names = [s for s in stars if s.lower() not in {os.path.basename(p).lower() for p in plugins[:masters]}]
    names_ok = [os.path.basename(p).lower() for p in tail] == [s.lower() for s in star_names]
    exist_ok = all(os.path.isabs(p) and os.path.isfile(p) for p in tail) and len(tail) == len(star_names)
    if names_ok and exist_ok and len(star_names) == 30:
        ok('G1 all 30 enabled plugins follow, in plugins.txt order, as existing full paths')
    else:
        bad('G1 enabled plugins: %d printed after the masters, %d stars, order %s, all exist %s'
            % (len(tail), len(star_names), names_ok, exist_ok))
    if [norm(p) for p in got] == [norm(p) for p in plugins]:
        ok('G1 the whole plugin list equals the independent re-derivation, path for path (%d)' % len(plugins))
    else:
        diff = [(i, a, b) for i, (a, b) in enumerate(zip(got, plugins)) if norm(a) != norm(b)][:3]
        bad('G1 plugin list differs from the re-derivation: %s' % (diff or 'lengths %d vs %d' % (len(got), len(plugins))))
    res = field_lines(out, 'resource')
    if [norm(p) for p in res] == [norm(p) for p in stack]:
        ok('G1 the stack is Data, then the %d enabled mods bottom-up, then overwrite (%d entries)' % (len(enabled), len(stack)))
    else:
        bad('G1 the stack differs from Data + enabled bottom-up + overwrite (%d vs %d entries)' % (len(res), len(stack)))
    dis = {norm(os.path.join(mods, m)) for m in disabled}
    leaked = [r for r in res if norm(r) in dis]
    if res and not leaked:
        ok('G4 no disabled mod (%d of them) is in the stack' % len(disabled))
    else:
        bad('G4 disabled mods in the stack: %s' % (leaked[:3] or 'no stack printed'))


def probe(outfile, fieldname, suffix, label):
    out = lines_of(outfile)
    vals = {l.split(':', 1)[0]: l.split(':', 1)[1].strip() for l in out if ':' in l and not l.startswith(' ')}
    v = vals.get(fieldname, '')
    if suffix == '(no)':
        good = vals.get('found') == 'no'
        shown = 'found: ' + vals.get('found', '?')
    else:
        good = vals.get('found') == 'yes' and norm(v).endswith(norm(suffix))
        shown = '%s: %s' % (fieldname, v)
    (ok if good else bad)('%s (%s)' % (label, shown))


def swap(src, dst, a, b):
    raw = open(src, 'rb').read()
    nl = b'\r\n' if b'\r\n' in raw else b'\n'
    ls = raw.split(nl)
    ia = [i for i, l in enumerate(ls) if l.decode('utf-8', 'replace')[1:] == a]
    ib = [i for i, l in enumerate(ls) if l.decode('utf-8', 'replace')[1:] == b]
    assert len(ia) == 1 and len(ib) == 1, 'each mod once'
    ls[ia[0]], ls[ib[0]] = ls[ib[0]], ls[ia[0]]
    data = nl.join(ls)
    with open(dst, 'wb') as f:
        f.write(data)
    print('  swapped lines %d and %d into %s' % (ia[0] + 1, ib[0] + 1, dst))


def record(outfile, profile, mods, data):
    out = lines_of(outfile)
    plugins = expected(profile, mods, data)[0]
    n = next((l.split()[-1] for l in out if l.startswith('bake-record plugins ')), '?')
    body = '\n'.join(out)
    (ok if n == str(len(plugins)) else bad)('G5 the record lists %s plugins (the load order has %d)' % (n, len(plugins)))
    for esp, mod in (('BNS Trees.esp', 'Boston Natural Surroundings'), ('TrueGrass.esp', 'True Grass')):
        want = os.path.join(mods, mod, esp).replace(os.sep, '/')
        hit = [l for l in out if l.startswith('bake-record plugin ') and norm(l).endswith(norm(want))]
        (ok if hit else bad)('G5 the record carries %s by full path %s' % (esp, want))
    first = next((l for l in out if l.startswith('bake-record plugin 0 ')), '')
    (ok if 'Fallout4.esm' in first and data.replace('\\', '/').lower() in first.replace('\\', '/').lower() else bad)(
        'G5 record plugin 0 is Data/Fallout4.esm')


def refused_ids(path):
    """(masters, records the ESM reader refuses): libfo76utils refuses a raw form ID above
    0x0FFFFFFF outside FD/FE (esmfile.cpp), before any master remap."""
    import struct
    b = open(path, 'rb').read()
    tes4 = struct.unpack_from('<I', b, 4)[0]
    masters, pos = 0, 24
    while pos + 6 <= 24 + tes4:
        ln = struct.unpack_from('<H', b, pos + 4)[0]
        masters += b[pos:pos + 4] == b'MAST'
        pos += 6 + ln
    bad, pos = 0, 0
    while pos + 24 <= len(b):
        size, fid = struct.unpack_from('<I', b, pos + 4)[0], struct.unpack_from('<I', b, pos + 12)[0]
        if b[pos:pos + 4] == b'GRUP':
            pos += 24
            continue
        bad += fid > 0x0FFFFFFF and bool((fid + 0x03000000) & 0xFE000000)
        pos += 24 + size
    return masters, bad


def untick(srcfile, ptxt_in, ptxt_out):
    """write plugins.txt with every plugin the ESM reader refuses unticked (the MO2 way: drop the '*')"""
    got = field_lines(lines_of(srcfile), 'plugin')
    drop = set()
    for p in got:
        if os.path.normcase(os.path.dirname(p)).lower().endswith(os.path.normcase(os.sep + 'data').lower()):
            continue    # the game's own masters
        m, n = refused_ids(p)
        if n:
            drop.add(os.path.basename(p).lower())
            print('  unticked %s: %d records carry a form ID above 0x0FFFFFFF (%d master(s))' % (os.path.basename(p), n, m))
    raw = open(ptxt_in, 'rb').read()
    nl = b'\r\n' if b'\r\n' in raw else b'\n'
    out = [l[1:] if l.startswith(b'*') and l[1:].decode('utf-8', 'replace').strip().lower() in drop else l
           for l in raw.split(nl)]
    with open(ptxt_out, 'wb') as f:
        f.write(nl.join(out))
    print('  %d plugin(s) unticked into %s' % (len(drop), ptxt_out))


if __name__ == '__main__':
    cmd = sys.argv[1]
    if cmd == 'g1':
        g1(*sys.argv[2:6])
    elif cmd == 'probe':
        probe(*sys.argv[2:6])
    elif cmd == 'swap':
        swap(*sys.argv[2:6])
    elif cmd == 'record':
        record(*sys.argv[2:6])
    elif cmd == 'counts':
        pl, st = expected(*sys.argv[2:5])[:2]
        print('%d %d' % (len(pl), len(st)))
    elif cmd == 'untick':
        untick(*sys.argv[2:5])
