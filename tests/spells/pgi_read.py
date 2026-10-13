#!/usr/bin/env python3
"""Lane PGI0 (2026-10-13): the independent Python reader of `.pgi` ("Precomputed GI", src/pgi_format.h), its gates,
the synthetic fixtures, and the size census of the four encodings (notes/pgi1/PLAN.md section 5.3).

Written from the format text, not from the C++: its own CRC (zlib), its own record layouts (numpy dtypes), its own
horizon arithmetic, its own `.tbk` v5 parser.

  pgi_read.py dump <file.pgi>
  pgi_read.py roundtrip <exe> <tbk dir> <work> [--interior <name>] [--red shuffle|horz8]
        exe pgiwrite on the bake, then per sector: every chunk rebuilt into the bake's own bytes and compared bit for
        bit (gate F1), HORZ = 16 azimuths for every surfel and equal to this reader's self-plane horizon (gate H0),
        exe pgicheck's chunk lines = this reader's (gate X1). PASS / FAIL line.
  pgi_read.py fixtures <exe> <out dir>
        synthetic .tbk v5 sectors (no game data) -> exe pgiwrite -> good fixtures; then the bad ones, derived here.
  pgi_read.py refusal <exe> <fixture dir> [--red acceptall]
        gate V1: the good fixtures accepted, every bad one refused for its own reason, by BOTH readers.
  pgi_read.py census <out.tsv> <label>=<tbk dir>[,interior] ...
        size of each sector file under each encoding (raw .pgi v1, packed 1024, packed 256, 1 m bricks; each also
        deflated), MB a sector.
  pgi_read.py all <exe> <work> <tbk dir>...
        every gate green, then every red, one verdict line.
"""

import math
import os
import struct
import subprocess
import sys
import zlib

import numpy as np

MAGIC = 0x31494750          # 'PGI1'
VERSION = 1
AZ = 16
CHUNK_DEFLATED = 1


def fourcc(s):
    return struct.unpack('<I', s.encode('ascii'))[0]


def idname(i):
    return struct.pack('<I', i).decode('ascii', 'replace')


SECT, SURF, HORZ, PROB, LINK, ROOM, EMIT = (fourcc(s) for s in ('SECT', 'SURF', 'HORZ', 'PROB', 'LINK', 'ROOM', 'EMIT'))

HEADER = struct.Struct('<IHHIiiIIffIQQII')     # 64 B
DIRENT = struct.Struct('<IHHIIII')             # 24 B
assert HEADER.size == 64 and DIRENT.size == 24

SECTOR = np.dtype([('x', '<i4'), ('y', '<i4'), ('bakever', '<u4'), ('content', '<u4'), ('flags', '<u4'),
                   ('kind', '<u4'), ('res', '<u4', 2)])
SURFEL = np.dtype([('pos', '<f4', 3), ('nrm', '<i2', 3), ('alb', 'u1', 3), ('bin', 'u1'), ('pad', 'u1', 2),
                   ('samples', '<u4'), ('res', '<u4')])
PROBE = np.dtype([('pos', '<f4', 3), ('off', '<u4'), ('cnt', '<u4'), ('scale', '<f4'), ('cov', '<f4'), ('unl', '<f4'),
                  ('sky', '<f4', 8), ('dist', '<f4', 8), ('rms', '<f4', 8), ('cls', '<u4'), ('lvl', '<u4'),
                  ('pres', '<u4', 2), ('skytint', 'u1', (8, 3)), ('room', '<u4', 2)])
LINKR = np.dtype([('delta', '<i2', 3), ('dir', '<i2', 2), ('w', '<u2'), ('side', 'u1'), ('tint', 'u1', 3),
                  ('door', '<u4')])
ROOMBOX = np.dtype([('room', '<u4'), ('lo', '<f4', 3), ('hi', '<f4', 3), ('res', '<u4')])
EMITR = np.dtype([('surfel', '<u4'), ('le', '<f4', 3)])
RECORD = {SECT: SECTOR, SURF: SURFEL, PROB: PROBE, LINK: LINKR, ROOM: ROOMBOX, EMIT: EMITR}
assert (SECTOR.itemsize, SURFEL.itemsize, PROBE.itemsize, LINKR.itemsize, ROOMBOX.itemsize, EMITR.itemsize) == \
    (32, 32, 176, 20, 32, 16)

# the bake's own `.tbk` v5 (src/probebake.cpp), parsed here independently of the .pgi
TBK_MAGIC = 0x314B4254
TBK_HEAD = struct.Struct('<IIIiifIIII6I')


class Refused(Exception):
    def __init__(self, why):
        Exception.__init__(self, why)
        self.why = why


def read_pgi(path_or_bytes):
    b = path_or_bytes if isinstance(path_or_bytes, (bytes, bytearray)) else open(path_or_bytes, 'rb').read()
    if len(b) < 64:
        raise Refused('short')
    (magic, ver, kind, whash, cx, cy, iplug, iform, cell, voxel, tool, ihash, ohash, nch, hcrc) = HEADER.unpack_from(b, 0)
    if magic != MAGIC:
        raise Refused('magic')
    if ver != VERSION:
        raise Refused('version')
    if kind not in (1, 2, 3):
        raise Refused('kind')
    dend = 64 + 24 * nch
    if dend > len(b):
        raise Refused('short')
    z = bytearray(b[:64])
    z[60:64] = b'\0\0\0\0'
    if zlib.crc32(bytes(b[64:dend]), zlib.crc32(bytes(z))) & 0xffffffff != hcrc:
        raise Refused('header-crc')
    chunks = []
    for i in range(nch):
        cid, grp, fl, off, ss, rs, crc = DIRENT.unpack_from(b, 64 + 24 * i)
        if off < dend or off % 4 or off + ss > len(b) or (not (fl & CHUNK_DEFLATED) and ss != rs):
            raise Refused('chunk-range')
        stored = bytes(b[off:off + ss])
        if zlib.crc32(stored) & 0xffffffff != crc:
            raise Refused('chunk-crc')
        data = zlib.decompress(stored) if fl & CHUNK_DEFLATED else stored
        if len(data) != rs:
            raise Refused('chunk-range')
        if cid in RECORD and rs % RECORD[cid].itemsize:
            raise Refused('record-size')
        if cid == HORZ:
            if rs < 8:
                raise Refused('horizon')
            naz, src, cnt = struct.unpack_from('<HHI', data, 0)
            if naz != AZ or 8 + cnt * 2 * naz != rs:
                raise Refused('horizon')
        chunks.append(dict(id=cid, group=grp, flags=fl, crc=crc, size=rs, data=data))
    head = dict(kind=kind, version=ver, world=whash, cx=cx, cy=cy, iplug=iplug, iform=iform, cell=cell, voxel=voxel,
                tool=tool, input=ihash, options=ohash, chunks=nch)
    return head, chunks


def groups(chunks):
    """{group: {id: array or (head, records)}} for the known ids; unknown ids skipped."""
    g = {}
    for c in chunks:
        d = g.setdefault(c['group'], {})
        if c['id'] in RECORD:
            d[c['id']] = np.frombuffer(c['data'], RECORD[c['id']])
        elif c['id'] == HORZ:
            naz, src, cnt = struct.unpack_from('<HHI', c['data'], 0)
            d[HORZ] = (naz, src, np.frombuffer(c['data'], np.uint8, cnt * 2 * naz, 8).reshape(cnt, 2, naz))
    return g


def read_tbk(path):
    b = open(path, 'rb').read()
    h = TBK_HEAD.unpack_from(b, 0)
    magic, ver, kind, cx, cy, cell, ns, npr, nl, flags = h[:10]
    res = h[10:16]
    if magic != TBK_MAGIC or ver != 5 or kind != 1 or res[0] != 0:
        raise ValueError('%s: not a .tbk v5' % path)
    nbox, nem = res[1], res[3]
    o = 64
    t = dict(head=b[:64], cx=cx, cy=cy, ver=ver, content=res[2], flags=flags, kind=kind, cell=cell)
    for name, n, sz in (('surf', ns, 32), ('probe', npr, 144), ('link', nl, 12), ('lext', nl, 8), ('pext', npr, 32),
                        ('box', nbox, 32), ('emit', nem, 16)):
        t[name] = b[o:o + n * sz]
        t['n_' + name] = n
        o += n * sz
    if o != len(b):
        raise ValueError('%s: size %d, the counts say %d' % (path, len(b), o))
    return t


def tbk_files(d):
    return sorted(os.path.join(d, f) for f in os.listdir(d) if f.startswith('sector_') and f.endswith('.tbk'))


def self_plane(nrm):
    """HORZ source 1, from the format text: per azimuth k (k x 22.5 deg from +X toward +Y) the elevation of the
    surfel's own plane: up-facing atan(-t / nz), vertical -90 facing / +90 behind, down-facing +90; u8 code."""
    n = nrm.astype(np.float64)
    out = np.empty((len(n), AZ), np.uint8)
    for k in range(AZ):
        a = k * (2.0 * math.pi / AZ)
        t = n[:, 0] * math.cos(a) + n[:, 1] * math.sin(a)
        with np.errstate(divide='ignore', invalid='ignore'):
            up = np.degrees(np.arctan(-t / n[:, 2])) if len(n) else np.zeros(0)
        deg = np.where(n[:, 2] > 0, up, np.where(n[:, 2] < 0, 90.0, np.where(t > 0, -90.0, 90.0)))
        v = np.floor((deg + 90.0) * (255.0 / 180.0) + 0.5)
        out[:, k] = np.clip(v, 0, 255).astype(np.uint8)
    return out


def interleave(a, sa, b, sb, n):
    x = np.frombuffer(a, np.uint8).reshape(n, sa)
    y = np.frombuffer(b, np.uint8).reshape(n, sb)
    return np.concatenate([x, y], axis=1).tobytes()


def split(rec, n, sa, sb):
    r = np.frombuffer(rec, np.uint8).reshape(n, sa + sb)
    return r[:, :sa].tobytes(), r[:, sa:].tobytes()


def rebuild_tbk(sec, gr):
    """The bake's sector file, rebuilt from one group's chunks."""
    s = gr[SECT][0]
    surf = gr[SURF].tobytes()
    pr, lk = gr[PROB], gr[LINK]
    probe, pext = split(pr.tobytes(), len(pr), 144, 32)
    link, lext = split(lk.tobytes(), len(lk), 12, 8)
    res = (0, len(gr[ROOM]), int(s['content']), len(gr[EMIT]), 0, 0)
    head = TBK_HEAD.pack(TBK_MAGIC, int(s['bakever']), int(s['kind']), int(s['x']), int(s['y']), sec['cell'],
                         len(gr[SURF]), len(pr), len(lk), int(s['flags']), *res)
    return head + surf + probe + link + lext + pext + gr[ROOM].tobytes() + gr[EMIT].tobytes()


def run(cmd):
    r = subprocess.run(cmd, capture_output=True, text=True, timeout=1800)
    return r.returncode, r.stdout + r.stderr


def exe_check(exe, files, red=''):
    """exe pgicheck: {file name: ('ok', [(id, group, bytes, crc)]) or ('refused', why)}."""
    cmd = [os.path.abspath(exe), '-no-gui', 'pgicheck'] + [os.path.abspath(f) for f in files]
    if red:
        cmd += ['--red', red]
    rc, out = run(cmd)
    res, cur = {}, None
    for line in out.splitlines():
        p = line.split()
        if len(p) >= 3 and p[0] == 'pgicheck' and p[1] != 'chunk':
            cur = p[1].rstrip(':')
            res[cur] = ('refused', p[3]) if p[2] == 'refused' else ('ok', [])
        elif len(p) >= 8 and p[:2] == ['pgicheck', 'chunk'] and cur:
            res[cur][1].append((p[2], int(p[4]), int(p[6]), int(p[8], 16)))
    return res


# ---------------------------------------------------------------- gates


def roundtrip(exe, tbkdir, work, interior=None, red=''):
    """Gates F1 (bit-exact per chunk and per file), H0 (16 azimuths, self-plane equal), X1 (exe check = this reader).
    Returns (ok, lines)."""
    os.makedirs(work, exist_ok=True)
    for f in os.listdir(work):
        if f.endswith('.pgi'):
            os.remove(os.path.join(work, f))
    cmd = [os.path.abspath(exe), '-no-gui', 'pgiwrite', '--tbk', os.path.abspath(tbkdir), '--out', os.path.abspath(work)]
    if interior:
        cmd += ['--interior', interior]
    if red:
        cmd += ['--red', red]
    rc, out = run(cmd)
    lines = [l for l in out.splitlines() if l.startswith('pgi')]
    fails = []
    if rc != 0:
        fails.append('pgiwrite rc %d: %s' % (rc, out.strip()[-300:]))
    tbks = {}
    for f in tbk_files(tbkdir):
        t = read_tbk(f)
        tbks[(t['cx'], t['cy'])] = t
    pgis = sorted(os.path.join(work, f) for f in os.listdir(work) if f.endswith('.pgi'))
    n = dict(files=len(pgis), sectors=0, chunks=0, chunks_equal=0, files_equal=0, surfels=0, horz_ok=0, horz_equal=0,
             links=0, probes=0)
    seen = set()
    ours = {}
    for p in pgis:
        try:
            head, chunks = read_pgi(p)
        except Refused as e:
            fails.append('%s refused by the Python reader: %s' % (os.path.basename(p), e.why))
            continue
        ours[os.path.basename(p)] = [(idname(c['id']), c['group'], c['size'], c['crc']) for c in chunks]
        for gi, gr in sorted(groups(chunks).items()):
            s = gr[SECT][0]
            key = (int(s['x']), int(s['y']))
            t = tbks.get(key)
            if t is None:
                fails.append('%s group %d: sector %s has no .tbk' % (os.path.basename(p), gi, key))
                continue
            seen.add(key)
            n['sectors'] += 1
            ns = t['n_surf']
            n['surfels'] += ns
            n['links'] += t['n_link']
            n['probes'] += t['n_probe']
            # F1, per chunk: each chunk's bytes against the bake's own
            want = {
                SECT: (t['ver'], t['content'], t['flags'], t['kind'], t['cx'], t['cy']),
                SURF: t['surf'],
                PROB: interleave(t['probe'], 144, t['pext'], 32, t['n_probe']),
                LINK: interleave(t['link'], 12, t['lext'], 8, t['n_link']),
                ROOM: t['box'],
                EMIT: t['emit'],
            }
            got = {
                SECT: (int(s['bakever']), int(s['content']), int(s['flags']), int(s['kind']), int(s['x']), int(s['y'])),
                SURF: gr[SURF].tobytes(), PROB: gr[PROB].tobytes(), LINK: gr[LINK].tobytes(),
                ROOM: gr[ROOM].tobytes(), EMIT: gr[EMIT].tobytes(),
            }
            for cid in want:
                n['chunks'] += 1
                if want[cid] == got[cid]:
                    n['chunks_equal'] += 1
                else:
                    fails.append('%s sector %s: %s differs from the .tbk' % (os.path.basename(p), key, idname(cid)))
            if rebuild_tbk(head, gr) == open(os.path.join(tbkdir, 'sector_%+05d_%+05d.tbk' % key), 'rb').read():
                n['files_equal'] += 1
            else:
                fails.append('%s sector %s: the rebuilt .tbk is not the bake\'s bytes' % (os.path.basename(p), key))
            # H0: 16 azimuths, one record per surfel, the self-plane horizon this reader computes
            naz, src, hz = gr[HORZ]
            if naz == AZ and len(hz) == ns:
                n['horz_ok'] += 1
                mine = self_plane(np.frombuffer(t['surf'], SURFEL, ns)['nrm'])
                if np.array_equal(hz[:, 0, :], mine) and np.all(hz[:, 1, :] == 255) and src == 1:
                    n['horz_equal'] += 1
                else:
                    d = np.abs(hz[:, 0, :].astype(int) - mine.astype(int))
                    fails.append('%s sector %s: HORZ differs from the self-plane horizon at %d of %d values (max %d)'
                                 % (os.path.basename(p), key, int((d > 0).sum()), d.size, int(d.max()) if d.size else 0))
            else:
                fails.append('%s sector %s: HORZ %d azimuths x %d records for %d surfels' % (os.path.basename(p), key, naz, len(hz), ns))
    missing = set(tbks) - seen
    if missing:
        fails.append('%d sectors of the bake not in any .pgi' % len(missing))
    # X1: the C++ checker (src/pgi_format.h checkFile) agrees with this reader chunk by chunk
    theirs = exe_check(exe, pgis) if pgis else {}
    agree = 0
    for name, ch in ours.items():
        th = theirs.get(name)
        if th and th[0] == 'ok' and th[1] == ch:
            agree += 1
        else:
            fails.append('%s: exe pgicheck %s, this reader ok with %d chunks' % (name, th[0] if th else 'silent', len(ch)))
    n['check_agree'] = agree
    lines.append('roundtrip %s: files %d, sectors %d (of %d), chunks bit-exact %d/%d, rebuilt .tbk byte-identical %d/%d, '
                 'HORZ 16 x surfels %d/%d, HORZ = self-plane %d/%d, exe pgicheck agrees %d/%d; surfels %d probes %d links %d'
                 % (os.path.basename(os.path.normpath(tbkdir)), n['files'], n['sectors'], len(tbks), n['chunks_equal'],
                    n['chunks'], n['files_equal'], n['sectors'], n['horz_ok'], n['sectors'], n['horz_equal'], n['sectors'],
                    agree, len(ours), n['surfels'], n['probes'], n['links']))
    ok = not fails and n['sectors'] == len(tbks) and n['sectors'] > 0
    return ok, lines + ['  FAIL ' + f for f in fails[:12]]


# ---------------------------------------------------------------- synthetic fixtures (no game data)


def synth_tbk(path, cx, cy, seed, nprobe=3, nsurf=48):
    rng = np.random.default_rng(seed)
    cell = 70.0
    base = np.array([cx * 4096.0, cy * 4096.0, 0.0])
    cells = set()
    while len(cells) < nsurf:
        c = tuple(int(v) for v in rng.integers([4, 4, 0], [14, 14, 5]))
        cells.add((c[0], c[1], c[2], int(rng.integers(0, 6))))
    keys = sorted(cells, key=lambda k: (k[2], k[1], k[0], k[3]))
    origin = np.floor(base / cell).astype(int)
    s = np.zeros(nsurf, SURFEL)
    for i, (x, y, z, b) in enumerate(keys):
        s[i]['pos'] = (origin + [x, y, z]) * cell + rng.uniform(5, 65, 3)
        ax, neg = b // 2, b % 2
        v = rng.normal(0, 0.25, 3)
        v[ax] = -1.0 if neg else 1.0
        v /= np.linalg.norm(v)
        s[i]['nrm'] = np.round(v * 32767).astype(np.int16)
        s[i]['alb'] = rng.integers(0, 256, 3)
        s[i]['bin'] = b
        s[i]['samples'] = rng.integers(1, 400)
    p = np.zeros(nprobe, PROBE)
    lks = []
    for i in range(nprobe):
        pc = origin + [int(rng.integers(5, 13)), int(rng.integers(5, 13)), 2]
        p[i]['pos'] = pc * cell + 35.0
        pick = sorted(rng.choice(nsurf, int(rng.integers(8, 20)), replace=False))
        p[i]['off'] = len(lks)
        p[i]['cnt'] = len(pick)
        p[i]['scale'] = rng.uniform(0.01, 0.2)
        p[i]['cov'] = rng.uniform(0.3, 1.0)
        p[i]['unl'] = rng.uniform(0, 0.2)
        p[i]['sky'] = rng.uniform(0, 1, 8)
        p[i]['dist'] = rng.uniform(70, 3000, 8)
        p[i]['rms'] = rng.uniform(0, 500, 8)
        p[i]['cls'] = int(rng.integers(0, 4))
        p[i]['lvl'] = int(rng.integers(0, 3))
        p[i]['skytint'] = rng.integers(128, 256, (8, 3))
        p[i]['room'] = (int(rng.integers(1, 3)), 0xFFFFFFFF)
        for j in pick:
            l = np.zeros(1, LINKR)[0]
            sk = np.floor(s[j]['pos'].astype(np.float32) / np.float32(cell)).astype(int)
            pk = np.floor(p[i]['pos'].astype(np.float32) / np.float32(cell)).astype(int)
            l['delta'] = sk - pk
            l['dir'] = rng.integers(-32767, 32768, 2)
            l['w'] = rng.integers(1, 65536)
            l['side'] = s[j]['bin']
            l['tint'] = (255, 255, 255) if rng.random() < 0.7 else tuple(rng.integers(60, 255, 3))
            l['door'] = 0 if rng.random() < 0.85 else int(rng.integers(1, 2 ** 24))
            lks.append(l)
    lk = np.array(lks, LINKR)
    bx = np.zeros(1, ROOMBOX)
    bx[0]['room'] = 1
    bx[0]['lo'] = base + [100, 100, 0]
    bx[0]['hi'] = base + [900, 900, 300]
    em = np.zeros(2, EMITR)
    em['surfel'] = [3, nsurf - 1]
    em['le'] = rng.uniform(0, 8, (2, 3))
    probe, pext = split(p.tobytes(), nprobe, 144, 32)
    link, lext = split(lk.tobytes(), len(lk), 12, 8)
    head = TBK_HEAD.pack(TBK_MAGIC, 5, 1, cx, cy, cell, nsurf, nprobe, len(lk), 3, 0, 1, 1 | 2 | 4 | 8 | 16, 2, 0, 0)
    open(path, 'wb').write(head + s.tobytes() + probe + link + lext + pext + bx.tobytes() + em.tobytes())


def patch_header(b, **kw):
    """Re-sign the header after a change (so only the intended field is wrong)."""
    b = bytearray(b)
    f = list(HEADER.unpack_from(b, 0))
    names = ['magic', 'version', 'kind', 'world', 'cx', 'cy', 'iplug', 'iform', 'cell', 'voxel', 'tool', 'input',
             'options', 'chunks', 'crc']
    for k, v in kw.items():
        f[names.index(k)] = v
    f[14] = 0
    HEADER.pack_into(b, 0, *f)
    z = bytes(b[:64])
    f[14] = zlib.crc32(bytes(b[64:64 + 24 * f[13]]), zlib.crc32(z)) & 0xffffffff
    HEADER.pack_into(b, 0, *f)
    return bytes(b)


def fixtures(exe, out):
    os.makedirs(out, exist_ok=True)
    src = os.path.join(out, 'src')
    for d in ('ext', 'int'):
        os.makedirs(os.path.join(src, d), exist_ok=True)
    synth_tbk(os.path.join(src, 'ext', 'sector_+0002_-0003.tbk'), 2, -3, 1)
    synth_tbk(os.path.join(src, 'int', 'sector_+0000_+0000.tbk'), 0, 0, 2)
    synth_tbk(os.path.join(src, 'int', 'sector_+0001_+0000.tbk'), 1, 0, 3, nprobe=2, nsurf=24)
    for f in os.listdir(out):
        if f.endswith('.pgi'):
            os.remove(os.path.join(out, f))
    rc1, o1 = run([os.path.abspath(exe), '-no-gui', 'pgiwrite', '--tbk', os.path.join(src, 'ext'), '--out', out,
                   '--world', 'SynthWorld', '--options', 'fixture'])
    rc2, o2 = run([os.path.abspath(exe), '-no-gui', 'pgiwrite', '--tbk', os.path.join(src, 'int'), '--out', out,
                   '--interior', 'SynthInterior', '--options', 'fixture'])
    if rc1 or rc2:
        print(o1, o2)
        return False
    good = open(os.path.join(out, '2_-3.pgi'), 'rb').read()
    h, ch = read_pgi(good)
    # a deflated copy: same content, SURF stored as a zlib stream (flag bit 0); the format allows it
    dfl = deflate_chunk(good, SURF)
    open(os.path.join(out, 'good_deflated_surf.pgi'), 'wb').write(dfl)
    bad = {
        'bad_magic.pgi': patch_header(good, magic=fourcc('PGI2')),
        'bad_version.pgi': patch_header(good, version=2),
        'bad_kind.pgi': patch_header(good, kind=9),
        'bad_short.pgi': good[:40],
        'bad_truncated.pgi': good[:len(good) - 64],
        'bad_header_crc.pgi': good[:20] + bytes([good[20] ^ 1]) + good[21:],
        'bad_chunk_crc.pgi': good[:-8] + bytes([good[-8] ^ 0x40]) + good[-7:],
        'bad_horizon8.pgi': horizon8(good),
    }
    for name, b in bad.items():
        open(os.path.join(out, name), 'wb').write(b)
    open(os.path.join(out, 'EXPECT.txt'), 'w', newline='\n').write(
        '# lane PGI0 fixtures: synthetic (tests/spells/pgi_read.py fixtures), no game data.\n'
        '# file  expected (ok | the refusal both readers must give)\n'
        '2_-3.pgi ok\nSynthInterior.pgi ok\ngood_deflated_surf.pgi ok\n'
        'bad_magic.pgi magic\nbad_version.pgi version\nbad_kind.pgi kind\nbad_short.pgi short\n'
        'bad_truncated.pgi chunk-range\nbad_header_crc.pgi header-crc\nbad_chunk_crc.pgi chunk-crc\n'
        'bad_horizon8.pgi horizon\n')
    return True


def rebuild_file(head_bytes, entries, bodies):
    """A file from a header (bytes), directory entries [(id, group, flags, rawSize)] and stored bodies."""
    n = len(entries)
    off = 64 + 24 * n
    dirb, body = b'', b''
    for (cid, grp, fl, rs), d in zip(entries, bodies):
        dirb += DIRENT.pack(cid, grp, fl, off, len(d), rs, zlib.crc32(d) & 0xffffffff)
        pad = (-len(d)) % 4
        body += d + b'\0' * pad
        off += len(d) + pad
    b = bytearray(head_bytes[:64]) + dirb + body
    return patch_header(bytes(b), chunks=n)


def deflate_chunk(good, which):
    h, ch = read_pgi(good)
    entries, bodies = [], []
    for c in ch:
        if c['id'] == which:
            entries.append((c['id'], c['group'], CHUNK_DEFLATED, c['size']))
            bodies.append(zlib.compress(c['data'], 9))
        else:
            entries.append((c['id'], c['group'], 0, c['size']))
            bodies.append(c['data'])
    return rebuild_file(good, entries, bodies)


def horizon8(good):
    h, ch = read_pgi(good)
    entries, bodies = [], []
    for c in ch:
        d = c['data']
        if c['id'] == HORZ:
            naz, src, cnt = struct.unpack_from('<HHI', d, 0)
            recs = np.frombuffer(d, np.uint8, cnt * 32, 8).reshape(cnt, 2, 16)[:, :, ::2]
            d = struct.pack('<HHI', 8, src, cnt) + recs.tobytes()
        entries.append((c['id'], c['group'], 0, len(d)))
        bodies.append(d)
    return rebuild_file(good, entries, bodies)


def refusal(exe, fdir, red=''):
    expect = {}
    for line in open(os.path.join(fdir, 'EXPECT.txt')):
        p = line.split()
        if len(p) == 2 and not line.startswith('#'):
            expect[p[0]] = p[1]
    files = [os.path.join(fdir, f) for f in sorted(expect)]
    theirs = exe_check(exe, files, red)
    fails, py_ok, exe_ok = [], 0, 0
    for f in sorted(expect):
        want = expect[f]
        try:
            read_pgi(os.path.join(fdir, f))
            mine = 'ok'
        except Refused as e:
            mine = e.why
        th = theirs.get(f, ('silent', None))
        exe_says = 'ok' if th[0] == 'ok' else th[1] if th[0] == 'refused' else 'silent'
        py_ok += mine == want
        exe_ok += exe_says == want
        if mine != want or exe_says != want:
            fails.append('%s: want %s, Python %s, exe %s' % (f, want, mine, exe_says))
    line = 'refusal%s: %d fixtures; Python reader right on %d, exe pgicheck right on %d' % (
        ' RED ' + red if red else '', len(expect), py_ok, exe_ok)
    return not fails, [line] + ['  FAIL ' + f for f in fails]


# ---------------------------------------------------------------- the size census


def f16(x):
    return np.asarray(x, np.float32).astype(np.float16)


def rgb9e5(c):
    c = np.clip(np.asarray(c, np.float64), 0, 65408.0)
    m = np.maximum(c.max(axis=1), 1e-30)
    e = np.maximum(-16, np.floor(np.log2(m))) + 16
    sc = np.exp2(e - 15 - 9)
    q = np.clip(np.floor(c / sc[:, None] + 0.5), 0, 511).astype(np.uint32)
    return (q[:, 0] | (q[:, 1] << 9) | (q[:, 2] << 18) | (e.astype(np.uint32) << 27)).astype('<u4')


def unpack_dirs(d):
    x = d[:, 0] / 32767.0
    y = d[:, 1] / 32767.0
    z = 1.0 - np.abs(x) - np.abs(y)
    neg = z < 0
    xs, ys = x.copy(), y.copy()
    x = np.where(neg, (1.0 - np.abs(ys)) * np.where(xs >= 0, 1, -1), xs)
    y = np.where(neg, (1.0 - np.abs(xs)) * np.where(ys >= 0, 1, -1), ys)
    v = np.stack([x, y, z], 1)
    return v / np.linalg.norm(v, axis=1)[:, None]


def resolve(t):
    """Per link: the surfel index it names (-1 = the file lacks it), by the probe's cell + delta and the side bin."""
    cell = np.float32(t['cell'])
    s = np.frombuffer(t['surf'], SURFEL[[n for n in SURFEL.names]] if False else SURFEL, t['n_surf'])
    p = np.frombuffer(interleave(t['probe'], 144, t['pext'], 32, t['n_probe']), PROBE, t['n_probe'])
    lk = np.frombuffer(interleave(t['link'], 12, t['lext'], 8, t['n_link']), LINKR, t['n_link'])
    sk = np.floor(s['pos'].astype(np.float32) / cell).astype(np.int64)
    pk = np.floor(p['pos'].astype(np.float32) / cell).astype(np.int64)
    owner = np.repeat(np.arange(len(p)), p['cnt'].astype(np.int64))
    lkey = pk[owner] + lk['delta'].astype(np.int64)

    def code(k, b):
        return ((k[:, 0] + 2 ** 20) << 44) | ((k[:, 1] + 2 ** 20) << 23) | ((k[:, 2] + 2 ** 18) << 3) | b.astype(np.int64)
    sc = code(sk, s['bin'])
    order = np.argsort(sc)
    lc = code(lkey, lk['side'])
    pos = np.searchsorted(sc[order], lc)
    pos = np.clip(pos, 0, max(len(sc) - 1, 0))
    hit = (sc[order][pos] == lc) if len(sc) else np.zeros(len(lc), bool)
    idx = np.where(hit, order[pos] if len(sc) else -1, -1)
    return s, p, lk, owner, idx


def encodings(t):
    """{encoding: {chunk: bytes}} for one sector, built from its real values (so deflate sees real entropy)."""
    s, p, lk, owner, idx = resolve(t)
    ns, npr, nl = len(s), len(p), len(lk)
    em = np.frombuffer(t['emit'], EMITR, t['n_emit'])
    rooms = t['box']
    horz = bytes(32 * ns)   # HORZ is counted at its full 32 B a surfel and never deflated (PGI1's traced values)
    sect = bytes(32)
    raw = dict(SECT=sect, SURF=t['surf'], HORZ=horz, PROB=interleave(t['probe'], 144, t['pext'], 32, npr),
               LINK=interleave(t['link'], 12, t['lext'], 8, nl), ROOM=rooms, EMIT=t['emit'])

    def packed(cap):
        keep = np.ones(nl, bool)
        if cap < 1024:
            for i in range(npr):
                a, c = int(p['off'][i]), int(p['cnt'][i])
                if c > cap:
                    w = lk['w'][a:a + c]
                    drop = np.argsort(-w.astype(np.int64), kind='stable')[cap:]
                    keep[a + drop] = False
        used = np.zeros(ns, bool)
        used[idx[keep & (idx >= 0)]] = True
        used[em['surfel'][em['surfel'] < ns]] = True
        remap = np.cumsum(used) - 1
        nsu = int(used.sum())
        # SURF 32 + HORZ 32 (the plan's packing; SURF keeps the bake's 32 B record)
        surf = np.frombuffer(t['surf'], np.uint8).reshape(ns, 32)[used].tobytes()
        # PROB 64: pos f32x3 | link offset u32 | count u16 | unlinked f16 | scale f32 | sky u8[8] | sky tint rgb8[8] | rooms u32[2]
        cnt = np.zeros(npr, np.int64)
        np.add.at(cnt, owner[keep], 1)
        off = np.concatenate([[0], np.cumsum(cnt)[:-1]])
        P = np.zeros(npr, np.dtype([('pos', '<f4', 3), ('off', '<u4'), ('cnt', '<u2'), ('unl', '<f2'), ('scale', '<f4'),
                                    ('sky', 'u1', 8), ('tint', 'u1', 24), ('room', '<u4', 2)]))
        assert P.dtype.itemsize == 64
        P['pos'], P['off'], P['cnt'], P['unl'], P['scale'] = p['pos'], off, cnt, f16(p['unl']), p['scale']
        P['sky'] = np.clip(np.round(p['sky'] * 255), 0, 255)
        P['tint'] = p['skytint'].reshape(npr, 24)
        P['room'] = p['room']
        # LINK 8: surfel index u24 | tint/door slot u8 | solid angle f16 | direction oct u8x2
        kl = lk[keep]
        ki = idx[keep]
        combo = (kl['tint'][:, 0].astype(np.int64) << 56) | (kl['tint'][:, 1].astype(np.int64) << 48) | \
            (kl['tint'][:, 2].astype(np.int64) << 40) | kl['door'].astype(np.int64)
        uc, slot = np.unique(combo, return_inverse=True)
        L = np.zeros(len(kl), np.dtype([('idx', '<u4'), ('omega', '<f2'), ('dir', 'u1', 2)]))
        L['idx'] = (np.where(ki >= 0, remap[np.maximum(ki, 0)], 0xFFFFFF).astype(np.uint32) & 0xFFFFFF) | \
            (np.minimum(slot, 255).astype(np.uint32) << 24)
        L['omega'] = f16(kl['w'] / 65535.0 * p['scale'][owner[keep]])
        L['dir'] = np.clip(np.round((kl['dir'] / 32767.0 + 1.0) * 127.5), 0, 255)
        E = np.zeros(len(em), np.dtype([('i', '<u4'), ('le', '<u4')]))
        E['i'] = remap[np.minimum(em['surfel'], max(ns - 1, 0))] if ns else 0
        E['le'] = rgb9e5(em['le']) if len(em) else 0
        tints = np.zeros(len(uc), np.dtype([('tint', 'u1', 4), ('door', '<u4')])).tobytes()   # 8 B a slot
        out = dict(SECT=sect, SURF=surf, HORZ=bytes(32 * nsu), PROB=P.tobytes(), LINK=L.tobytes(), TINT=tints,
                   ROOM=rooms, EMIT=E.tobytes())
        return out, kl, ki, owner[keep], nsu

    a, kl, ki, ko, nsu = packed(1024)
    b, _, _, _, _ = packed(256)
    # (c) 1 m bricks (lane BRICK1's gather at 70 units): surfels grouped by cell and the normal's dominant axis;
    # per probe one term per brick its links reach: brick index u32 + W[6] f16 (16 B, the plan's BRIK)
    nf = s['nrm'].astype(np.float64)
    ax = np.argmax(np.abs(nf), 1)
    face = 2 * ax + (nf[np.arange(ns), ax] < 0)
    key = np.floor(s['pos'].astype(np.float32) / np.float32(70.0)).astype(np.int64)
    bcode = ((key[:, 0] + 2 ** 20) << 44) | ((key[:, 1] + 2 ** 20) << 23) | ((key[:, 2] + 2 ** 18) << 3) | face
    ub, brick = np.unique(bcode, return_inverse=True)
    ok = ki >= 0
    term = ko[ok].astype(np.int64) * (len(ub) + 1) + brick[ki[ok]]
    ut, tinv = np.unique(term, return_inverse=True)
    d = unpack_dirs(kl['dir'][ok].astype(np.float64))
    om = kl['w'][ok] / 65535.0 * p['scale'][ko[ok]]
    W = np.zeros((len(ut), 6))
    for k in range(6):
        axv = np.zeros(3)
        axv[k // 2] = 1.0 if k % 2 == 0 else -1.0
        np.add.at(W[:, k], tinv, om * np.maximum(d @ axv, 0))
    T = np.zeros(len(ut), np.dtype([('b', '<u4'), ('w', '<f2', 6)]))
    T['b'] = ut % (len(ub) + 1)
    T['w'] = f16(W)
    # brick table: first surfel u32, count u16, face u8, pad (8 B); the probe's term range rides in PROB's link fields
    BT = np.zeros(len(ub), np.dtype([('first', '<u4'), ('n', '<u2'), ('face', 'u1'), ('pad', 'u1')]))
    BT['n'] = np.bincount(brick, minlength=len(ub))
    BT['first'] = np.concatenate([[0], np.cumsum(BT['n'])[:-1]])
    BT['face'] = (ub & 7)
    c = dict(a)
    del c['LINK']
    c['BRIK'] = T.tobytes()
    c['BTAB'] = BT.tobytes()
    stats = dict(surfels=ns, probes=npr, links=nl, links256=int(len(b['LINK']) // 8), bricks=len(ub), terms=len(ut),
                 unresolved=int((idx < 0).sum()))
    return dict(raw=raw, packed1024=a, packed256=b, bricks1m=c), stats


def enc_size(chunks, deflate):
    n = 64 + 24 * len(chunks)
    for k, v in chunks.items():
        if deflate and k != 'HORZ' and len(v) > 64:
            n += min(len(v), len(zlib.compress(v, 9)))
        else:
            n += len(v)
        n += (-n) % 4
    return n


ENCS = ('raw', 'packed1024', 'packed256', 'bricks1m')


def census(out_tsv, specs):
    rows = []
    for spec in specs:
        label, rest = spec.split('=', 1)
        parts = rest.split(',')
        d, kind = parts[0], (parts[1] if len(parts) > 1 else 'exterior')
        own = None
        if len(parts) > 2:   # x0:x1:y0:y1, the cells the tile was asked for (its neighbour files left out)
            own = [int(v) for v in parts[2].split(':')]
        for f in tbk_files(d):
            t = read_tbk(f)
            if own and not (own[0] <= t['cx'] <= own[1] and own[2] <= t['cy'] <= own[3]):
                continue
            encs, st = encodings(t)
            r = dict(label=label, kind=kind, sector='%d,%d' % (t['cx'], t['cy']), tbk=os.path.getsize(f), **st)
            for e in ENCS:
                r[e] = enc_size(encs[e], False)
                r[e + '_z'] = enc_size(encs[e], True)
            rows.append(r)
            print('census %s %s: %s' % (label, r['sector'], ' '.join('%s %.2f/%.2f' % (e, r[e] / 1e6, r[e + '_z'] / 1e6)
                                                                       for e in ENCS)), flush=True)
    cols = ['label', 'kind', 'sector', 'tbk', 'surfels', 'probes', 'links', 'links256', 'bricks', 'terms', 'unresolved'] + \
        [e + s for e in ENCS for s in ('', '_z')]
    with open(out_tsv, 'w', newline='\n') as fo:
        fo.write('\t'.join(cols) + '\n')
        for r in rows:
            fo.write('\t'.join(str(r[c]) for c in cols) + '\n')
    return rows


# ---------------------------------------------------------------- main


def main(argv):
    if len(argv) < 2:
        print(__doc__)
        return 2
    cmd = argv[1]
    if cmd == 'dump':
        head, chunks = read_pgi(argv[2])
        print('pgi', head)
        for c in chunks:
            print('chunk %s group %d bytes %d crc %08x flags %d' % (idname(c['id']), c['group'], c['size'], c['crc'], c['flags']))
        return 0
    if cmd == 'roundtrip':
        a = argv[2:]
        interior = a[a.index('--interior') + 1] if '--interior' in a else None
        red = a[a.index('--red') + 1] if '--red' in a else ''
        ok, lines = roundtrip(a[0], a[1], a[2], interior, red)
        print('\n'.join(lines))
        print('roundtrip', 'PASS' if ok else 'FAIL')
        return 0 if ok else 1
    if cmd == 'fixtures':
        return 0 if fixtures(argv[2], argv[3]) else 1
    if cmd == 'refusal':
        red = argv[argv.index('--red') + 1] if '--red' in argv else ''
        ok, lines = refusal(argv[2], argv[3], red)
        print('\n'.join(lines))
        print('refusal', 'PASS' if ok else 'FAIL')
        return 0 if ok else 1
    if cmd == 'census':
        census(argv[2], argv[3:])
        return 0
    if cmd == 'all':
        exe, work = argv[2], argv[3]
        verdict = True
        out = []
        for i, d in enumerate(argv[4:]):
            interior = None
            if d.endswith(',interior'):
                d = d[:-len(',interior')]
                interior = 'interior'
            ok, lines = roundtrip(exe, d, os.path.join(work, 'rt%d' % i), interior)
            out += ['GREEN ' + l for l in lines] + ['GREEN roundtrip %s' % ('PASS' if ok else 'FAIL')]
            verdict &= ok
            for red in ('shuffle', 'horz8'):
                ok, lines = roundtrip(exe, d, os.path.join(work, 'rt%d_%s' % (i, red)), interior, red)
                out += ['RED %s %s' % (red, l) for l in lines]
                out += ['RED %s roundtrip %s (must FAIL)' % (red, 'PASS' if ok else 'FAIL')]
                verdict &= not ok
        fx = os.path.join(work, 'fixtures')
        okf = fixtures(exe, fx)
        ok, lines = refusal(exe, fx)
        out += ['GREEN ' + l for l in lines] + ['GREEN refusal %s' % ('PASS' if ok and okf else 'FAIL')]
        verdict &= ok and okf
        ok, lines = refusal(exe, fx, 'acceptall')
        out += ['RED acceptall ' + lines[0], 'RED acceptall refusal %s (must FAIL)' % ('PASS' if ok else 'FAIL')]
        verdict &= not ok
        print('\n'.join(out))
        print('pgi gates: %s' % ('PASS (greens pass, reds fail)' if verdict else 'FAIL'))
        return 0 if verdict else 1
    print(__doc__)
    return 2


if __name__ == '__main__':
    sys.exit(main(sys.argv))
