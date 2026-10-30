#!/usr/bin/env python3
"""Lane FARWATER1: the user's water marks in a .lodl, read with no code shared with src/watermark.cpp.

usage: lodl_marks.py dump <file>
       lodl_marks.py equal <ref.lodl> <new.lodl>     the marks are byte-equal (strokes, names, hand-set fields)
       lodl_marks.py remap <ref.lodl> <new.lodl>     body ids may differ: the marks follow the bodies by name
Prints one PASS/FAIL line a check and a last line `lodl_marks <mode> PASS|FAIL (n fail)`; exit 0 only on PASS.
Header fields (spec_water.md 3, src/watermark.cpp kOff*): 0x44 sections, 0xA0 body table u64, 0xA8 count, 0xAC stride,
0xB0 name blob u64, 0xB8 its length, 0xE8 stroke store u64, 0xF0 its length."""
import hashlib, struct, sys, zlib

USER = 0x01 | 0x04 | 0x20      # the table bits the marking panel owns: user-edited, colour, class by hand
KIND = {0: 'stroke', 1: 'pin', 2: 'barrier', 3: 'merge', 4: 'source', 5: 'outlet', 6: 'zeroflow', 7: 'dyepin',
        8: 'dyeknob', 9: 'dyemouth', 10: 'raster'}


def read(path):
    b = open(path, 'rb').read()
    sect = struct.unpack_from('<I', b, 0x44)[0]
    o_body, n_body, stride = struct.unpack_from('<QII', b, 0xA0)
    o_name, n_name = struct.unpack_from('<QI', b, 0xB0)
    o_str, n_str = struct.unpack_from('<QI', b, 0xE8)
    names = b[o_name:o_name + n_name] if o_name else b''
    bodies = []
    for i in range(n_body):
        r = b[o_body + i * stride:o_body + (i + 1) * stride]
        bid, cls, flags, wh, form, area = struct.unpack_from('<HBBfII', r, 0)
        colour = tuple(r[36:40])
        noff = struct.unpack_from('<I', r, 44)[0]
        name = ''
        if noff and noff < len(names):
            name = names[noff:names.index(b'\0', noff)].decode('utf-8')
        bodies.append(dict(id=bid, cls=cls, flags=flags, form=form, area=area, colour=colour, name=name))
    store = b[o_str:o_str + n_str]
    strokes = []
    if len(store) >= 4:
        cnt = struct.unpack_from('<I', store, 0)[0]
        at = 4
        for _ in range(cnt):
            ln, body, kind, fl, speed, width, n = struct.unpack_from('<IHBBffH', store, at)
            pts = [struct.unpack_from('<ff', store, at + 20 + 8 * k) for k in range(n)]
            strokes.append(dict(body=body, kind=kind, flags=fl, speed=speed, width=width, pts=pts,
                                tail=store[at + 20 + 8 * n:at + ln]))
            at += ln
    return dict(bytes=b, sha=hashlib.sha1(b).hexdigest()[:12], sect=sect, bodies=bodies, names=names,
                store=store, strokes=strokes)


def body_at(d, wx, wy):
    """The body id under a world point, read off the file's own body-ID plane (header 0x08 cell min x/y, 0xBC its
    samples a cell, 0xC0 its tiled store: head tilesX tilesY edge bps dir data; a dir entry offset u64, zlib size u32
    (0 = a uniform tile, the value in the next u32), raw size u32)."""
    b = d['bytes']
    minx, miny = struct.unpack_from('<ii', b, 0x08)
    rate = struct.unpack_from('<I', b, 0xBC)[0]
    o_id = struct.unpack_from('<Q', b, 0xC0)[0]
    tx_n, ty_n, e, bps = struct.unpack_from('<IIII', b, o_id)
    dir_at = struct.unpack_from('<Q', b, o_id + 16)[0]
    px = int((wx - minx * 4096.0) // (4096.0 / rate))
    py = int((wy - miny * 4096.0) // (4096.0 / rate))
    tx, ty = px // e, py // e
    if px < 0 or py < 0 or tx >= tx_n or ty >= ty_n:
        return 0
    off, csz, usz = struct.unpack_from('<QII', b, dir_at + (ty * tx_n + tx) * 16)
    if not csz:
        return usz & 0xFFFF
    cache = d.setdefault('tiles', {})
    raw = cache.get((tx, ty))
    if raw is None:
        raw = cache[(tx, ty)] = zlib.decompress(b[off:off + csz])
    return struct.unpack_from('<H', raw, ((py % e) * e + px % e) * 2)[0]


def user_view(bd):
    """What the panel owns on one body: the bits, and each field only where its bit says it is the user's."""
    f = bd['flags'] & USER
    return (f, bd['cls'] if f & 0x20 else None, bd['colour'] if f & 0x04 else None,
            bd['form'] if f & 0x01 else None, bd['name'])


def dump(path):
    d = read(path)
    ub = [b for b in d['bodies'] if (b['flags'] & USER) or b['name']]
    print(f'{path}: sha {d["sha"]} sections 0x{d["sect"]:x} bodies {len(d["bodies"])} '
          f'strokes {len(d["strokes"])} ({len(d["store"])} bytes) names blob {len(d["names"])} bytes')
    for b in ub:
        print(f'  body {b["id"]}: {user_view(b)}')
    for k, s in enumerate(d['strokes']):
        print(f'  stroke {k}: {KIND.get(s["kind"], s["kind"])} body {s["body"]} flags {s["flags"]} speed {s["speed"]:g} '
              f'width {s["width"]:g} pts {len(s["pts"])} tail {len(s["tail"])}')
    return d


def main():
    mode = sys.argv[1]
    if mode == 'dump':
        dump(sys.argv[2])
        return 0
    ref, new = dump(sys.argv[2]), dump(sys.argv[3])
    fails = []

    def check(ok, what):
        print(('PASS ' if ok else 'FAIL ') + what)
        if not ok:
            fails.append(what)

    # the floor: a reference with no marks would let any re-bake pass
    rnamed = [b for b in ref['bodies'] if b['name']]
    check(len(ref['strokes']) >= 4 and len(rnamed) >= 2,
          f'floor: the reference carries marks ({len(ref["strokes"])} strokes, {len(rnamed)} named bodies; need 4, 2)')
    if mode == 'equal':
        check(new['store'] == ref['store'], f'stroke store byte-equal ({len(new["store"])} vs {len(ref["store"])} bytes)')
        check(new['names'] == ref['names'], f'name blob byte-equal ({len(new["names"])} vs {len(ref["names"])} bytes)')
        diff = [i for i, (a, b) in enumerate(zip(ref['bodies'], new['bodies'])) if user_view(a) != user_view(b)]
        check(len(ref['bodies']) == len(new['bodies']) and not diff,
              f'hand-set body fields equal on every body ({len(diff)} differ of {len(ref["bodies"])})')
        check(new['sha'] == ref['sha'], f'whole file byte-equal, planes re-derived from the marks (sha {new["sha"]} vs {ref["sha"]})')
    elif mode == 'remap':
        byname = {b['name']: b for b in new['bodies'] if b['name']}
        check(sorted(byname) == sorted(b['name'] for b in rnamed),
              f'every name found a body ({sorted(byname)} vs {sorted(b["name"] for b in rnamed)})')
        for b in rnamed:
            nb = byname.get(b['name'])
            check(nb is not None and user_view(nb) == user_view(b),
                  f'body "{b["name"]}": hand-set fields equal (old id {b["id"]}, new id {nb["id"] if nb else "-"})')
        check(len(new['strokes']) == len(ref['strokes']),
              f'every stroke carried ({len(new["strokes"])} of {len(ref["strokes"])})')
        oldname = {b['id']: b['name'] for b in ref['bodies']}
        newname = {b['id']: b['name'] for b in new['bodies']}
        moved = 0
        for k, (a, b) in enumerate(zip(ref['strokes'], new['strokes'])):
            same = (a['kind'], a['flags'], a['speed'], a['width'], a['pts'], a['tail']) == \
                   (b['kind'], b['flags'], b['speed'], b['width'], b['pts'], b['tail'])
            moved += a['body'] != b['body']
            nm = oldname.get(a['body'], '')
            follows = (a['body'] == 0) == (b['body'] == 0) and (not nm or newname.get(b['body']) == nm)
            check(same and follows, f'stroke {k} ({KIND.get(a["kind"])}): points and fields equal, body {a["body"]} -> '
                  f'{b["body"]}{" named " + repr(nm) if nm else ""}')
            # WHERE: a stroke's points sat on its body in the old file; they must sit on its body in the new one
            if a['kind'] == 0 and a['pts']:
                on_old = sum(body_at(ref, x, y) == a['body'] for x, y in a['pts'])
                on_new = sum(body_at(new, x, y) == b['body'] for x, y in b['pts'])
                check(on_old == len(a['pts']) and on_new == len(b['pts']),
                      f'stroke {k}: its {len(a["pts"])} points lie on its body, old file {on_old}, new file {on_new} '
                      f'(new plane says {[body_at(new, x, y) for x, y in b["pts"]]})')
        print(f'info: {moved} stroke(s) changed body id')
    print(f'lodl_marks {mode} {"PASS" if not fails else "FAIL"} ({len(fails)} fail)')
    return 0 if not fails else 1


if __name__ == '__main__':
    sys.exit(main())
