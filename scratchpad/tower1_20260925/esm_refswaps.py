"""TOWER1 (copy of GREY1 esm_swaps.py, refs keyed by REFR form id): material swaps in vanilla Fallout4.esm (read only). STAT/SCOL/MSTT MODL + MODS (base swap), every MSWP
(BNAM original -> SNAM replacement, CNAM colour remapping index), and every REFR's NAME + XMSP (placement swap)
inside the Commonwealth worldspace (WRLD 0x3C children). Layouts: xEdit wbDefinitionsFO4.pas (MSWP l.12437).
-> esm_swaps.pkl"""
import struct, zlib, pickle, collections
ESM = r'X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm'
b = open(ESM, 'rb').read()
base = {}      # fid -> dict(modl, mods)
mswp = {}      # fid -> dict(edid, subs=[(orig, repl, cnam)])
refs = {}      # REFR fid -> (base, xmsp), Commonwealth only


def fields(d):
    o = 0
    big = None
    while o < len(d):
        t = d[o:o + 4]
        n = struct.unpack_from('<H', d, o + 4)[0]
        o += 6
        if t == b'XXXX':
            big = struct.unpack_from('<I', d, o)[0]
            o += n
            continue
        if big is not None:
            n = big
            big = None
        yield t, d[o:o + n]
        o += n


def body(o, sz, fl):
    d = b[o + 24:o + 24 + sz]
    return zlib.decompress(d[4:]) if fl & 0x40000 else d


def walk(o, end, inCW):
    while o < end:
        t = b[o:o + 4]
        if t == b'GRUP':
            sz = struct.unpack_from('<I', b, o + 4)[0]
            lab = struct.unpack_from('<I', b, o + 8)[0]
            gt = struct.unpack_from('<i', b, o + 12)[0]
            cw = inCW or (gt == 1 and lab == 0x3C)
            if gt == 0 and b[o + 8:o + 12] not in (b'STAT', b'SCOL', b'MSTT', b'MSWP', b'WRLD'):
                o += sz
                continue
            if gt == 1 and lab != 0x3C:
                o += sz          # other worldspaces
                continue
            walk(o + 24, o + sz, cw)
            o += sz
            continue
        sz, fl, fid = struct.unpack_from('<III', b, o + 4)
        if t in (b'STAT', b'SCOL', b'MSTT'):
            r = dict(modl=None, mods=0)
            for ft, v in fields(body(o, sz, fl)):
                if ft == b'MODL' and r['modl'] is None:
                    r['modl'] = v.rstrip(b'\0').decode('latin-1')
                elif ft == b'MODS' and not r['mods']:
                    r['mods'] = struct.unpack_from('<I', v)[0]
            base[fid] = r
        elif t == b'MSWP':
            r = dict(edid='', subs=[])
            cur = None
            for ft, v in fields(body(o, sz, fl)):
                if ft == b'EDID':
                    r['edid'] = v.rstrip(b'\0').decode('latin-1')
                elif ft == b'BNAM':
                    cur = [v.rstrip(b'\0').decode('latin-1'), '', None]
                    r['subs'].append(cur)
                elif ft == b'SNAM' and cur is not None:
                    cur[1] = v.rstrip(b'\0').decode('latin-1')
                elif ft == b'CNAM' and cur is not None:
                    cur[2] = struct.unpack_from('<f', v)[0]
            mswp[fid] = r
        elif t == b'REFR' and inCW:
            nm = xm = 0
            for ft, v in fields(body(o, sz, fl)):
                if ft == b'NAME':
                    nm = struct.unpack_from('<I', v)[0]
                elif ft == b'XMSP':
                    xm = struct.unpack_from('<I', v)[0]
            refs[fid] = (nm, xm)
        o += 24 + sz


hdr = struct.unpack_from('<I', b, 4)[0]
walk(24 + hdr, len(b), False)
pickle.dump(dict(base=base, mswp=mswp, refs=refs), open('esm_refswaps.pkl', 'wb'))
print('bases %d (with MODS %d), MSWP %d, CW REFR %d (with XMSP %d)' % (
    len(base), sum(1 for r in base.values() if r['mods']), len(mswp), sum(refs.values()),
    sum(c for (n, x), c in refs.items() if x)))
