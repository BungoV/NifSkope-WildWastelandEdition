import struct, sys, os, math, glob

# Batch validator: decode every bhkPhysicsSystem under a directory, and for
# each NON-identity body transform applied to a convex shape, score candidate
# transform formulas by how far the result lands outside the union AABB of
# the file's identity-body compressed-mesh collision (which is known-correct).

ROOT = sys.argv[1]
LIMIT = int(sys.argv[2]) if len(sys.argv) > 2 else 200

def parse_nif_blobs(path):
    try:
        data = open(path, 'rb').read()
    except OSError:
        return []
    if b'bhkPhysicsSystem' not in data:
        return []
    try:
        nl = data.index(b'\x0a'); pos = nl+1
        pos += 4+1+4
        nb, = struct.unpack_from('<I', data, pos); pos += 4
        bsver, = struct.unpack_from('<I', data, pos); pos += 4
        for i in range(4 if bsver == 130 else 3):
            slen = data[pos]; pos += 1+slen
        ntypes = struct.unpack_from('<H', data, pos)[0]; pos += 2
        btypes = []
        for i in range(ntypes):
            slen = struct.unpack_from('<I', data, pos)[0]; pos += 4
            btypes.append(data[pos:pos+slen]); pos += slen
        tidx = list(struct.unpack_from(f'<{nb}H', data, pos)); pos += nb*2
        bsize = list(struct.unpack_from(f'<{nb}I', data, pos)); pos += nb*4
        nstr = struct.unpack_from('<I', data, pos)[0]; pos += 4+4
        for i in range(nstr):
            slen = struct.unpack_from('<I', data, pos)[0]; pos += 4+slen
        ngrp = struct.unpack_from('<I', data, pos)[0]; pos += 4+ngrp*4
        out = []
        off = pos
        for i in range(nb):
            if btypes[tidx[i]] == b'bhkPhysicsSystem':
                blen, = struct.unpack_from('<I', data, off)
                out.append(bytes(data[off+4:off+4+blen]))
            off += bsize[i]
        return out
    except Exception:
        return []

def quat_mat(qx,qy,qz,qw):
    return [[1-2*(qy*qy+qz*qz), 2*(qx*qy-qz*qw), 2*(qx*qz+qy*qw)],
            [2*(qx*qy+qz*qw), 1-2*(qx*qx+qz*qz), 2*(qy*qz-qx*qw)],
            [2*(qx*qz-qy*qw), 2*(qy*qz+qx*qw), 1-2*(qx*qx+qy*qy)]]

# candidate formulas: name -> (useConjugate, transpose, translatePre)
CANDS = []
for conj in (0, 1):
    for tr in (0, 1):
        for pre in (0, 1):
            CANDS.append((f"{'conj' if conj else 'q'}/{'T' if tr else 'R'}/{'preT' if pre else 'postT'}", conj, tr, pre))

def analyze(blob):
    try:
        if len(blob) < 0x100 or struct.unpack_from('<I', blob, 0)[0] != 0x57E0E057:
            return None
        aS, lF, gF, vF, eO = struct.unpack_from('<5i', blob, 0x40+2*0x40+20)
        cS, cL = struct.unpack_from('<2i', blob, 0x40+20)
        cn = {}
        p = cS
        while p < cS+cL-5 and blob[p+4] == 9:
            e = blob.index(b'\x00', p+5); cn[p+5-cS] = blob[p+5:e].decode(); p = e+1
        objs = {}
        p = aS+vF
        while p+12 <= aS+eO and p+12 <= len(blob):
            s, sec, c = struct.unpack_from('<iii', blob, p); p += 12
            if s != -1: objs[s] = cn.get(c, '?')
        loc = {}
        p = aS+lF
        while p+8 <= aS+gF:
            s, d = struct.unpack_from('<ii', blob, p); p += 8
            if s != -1: loc[s] = d
        glo = {}
        p = aS+gF
        while p+12 <= aS+vF:
            s, sec, d = struct.unpack_from('<iii', blob, p); p += 12
            if s != -1: glo[s] = d

        # reference AABB: union of CMSD global domains
        ref = None
        for o, c in objs.items():
            if c == 'hknpCompressedMeshShapeData':
                mn = struct.unpack_from('<3f', blob, aS+o+0x20)
                mx = struct.unpack_from('<3f', blob, aS+o+0x30)
                if ref is None:
                    ref = [list(mn), list(mx)]
                else:
                    for k in range(3):
                        ref[0][k] = min(ref[0][k], mn[k]); ref[1][k] = max(ref[1][k], mx[k])
        if ref is None:
            return None

        cinfos = loc.get(0x40)
        if cinfos is None:
            return None
        nbod, = struct.unpack_from('<I', blob, aS+0x48)
        if nbod > 512:
            return None
        results = []
        for i in range(nbod):
            c = aS + cinfos + i*0x60
            shp = glo.get(cinfos + i*0x60, -1)
            if shp < 0 or objs.get(shp) != 'hknpConvexPolytopeShape':
                continue
            T = struct.unpack_from('<3f', blob, c+0x30)
            q = struct.unpack_from('<4f', blob, c+0x40)
            qn = math.sqrt(sum(v*v for v in q))
            if abs(qn - 1.0) > 0.01:
                continue
            ident = abs(q[3]) > 0.9999 and max(abs(v) for v in T) < 1e-5
            if ident:
                continue
            B = aS + shp
            cnt, roff = struct.unpack_from('<2H', blob, B+0x30)
            if cnt == 0 or cnt > 1024:
                continue
            pv = B+0x30+roff
            verts = [struct.unpack_from('<3f', blob, pv+k*16) for k in range(cnt)]
            scores = {}
            for name, conj, tr, pre in CANDS:
                qx,qy,qz,qw = q
                if conj:
                    qx,qy,qz = -qx,-qy,-qz
                R = quat_mat(qx,qy,qz,qw)
                if tr:
                    R = [[R[j][k] for j in range(3)] for k in range(3)]
                over = 0.0
                for v in verts:
                    if pre:
                        u = (v[0]+T[0], v[1]+T[1], v[2]+T[2])
                        w = tuple(u[0]*R[0][k]+u[1]*R[1][k]+u[2]*R[2][k] for k in range(3))
                    else:
                        w = tuple(v[0]*R[0][k]+v[1]*R[1][k]+v[2]*R[2][k]+T[k] for k in range(3))
                    for k in range(3):
                        over += max(0.0, ref[0][k]-w[k]) + max(0.0, w[k]-ref[1][k])
                scores[name] = over / cnt * 69.99125
            results.append(scores)
        return results
    except Exception:
        return None

files = []
for dirpath, dirs, names in os.walk(ROOT):
    for n in names:
        if n.lower().endswith('.nif'):
            files.append(os.path.join(dirpath, n))
    if len(files) > LIMIT*8:
        break

totals = {name: 0.0 for name, *_ in CANDS}
counts = 0
perfile = []
checked = 0
for f in files:
    if checked >= LIMIT:
        break
    blobs = parse_nif_blobs(f)
    if not blobs:
        continue
    checked += 1
    for blob in blobs:
        res = analyze(blob)
        if not res:
            continue
        for scores in res:
            counts += 1
            for name, v in scores.items():
                totals[name] += v
            best = min(scores, key=scores.get)
            perfile.append((os.path.basename(f), best, round(scores[best],1),
                            round(scores['q/R/postT'],1), round(scores['q/T/postT'],1)))

print(f"scanned {checked} files with physics, {counts} non-identity convex bodies")
print("\ntotal overshoot per formula (game units, lower = better):")
for name in sorted(totals, key=totals.get):
    print(f"  {name:15} {totals[name]:12.1f}")
print("\nsample of per-body best formulas:")
for row in perfile[:25]:
    print("  ", row)
