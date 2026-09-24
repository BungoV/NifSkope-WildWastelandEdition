# IMPOSTORLIGHT1 -- vanilla's own two-sided flag (and the light-through terms)
# of every material a subject's NEAR model names, read from the BGSM bytes by
# the field order of src/io/materialfile.cpp:91-160 (Material::readFile then
# ShaderMaterial::readFile). No NifSkope code is shared.
#   python material_flags.py <bgsm> [...]
import struct, sys, os

DATA = 'E:/Tools/Fallout 4/DataUnpacked/Data'


def rd(fmt, b, o):
    v = struct.unpack_from('<' + fmt, b, o)
    return v, o + struct.calcsize('<' + fmt)


def bgsm(path):
    b = open(path, 'rb').read()
    if b[:4] != b'BGSM':
        return dict(err='not BGSM (%r)' % b[:4])
    (ver,), o = rd('I', b, 4)
    (tile,), o = rd('I', b, o)
    _, o = rd('5f', b, o)                     # uv offset/scale x4, alpha
    (ablend, asrc, adst), o = rd('BII', b, o)
    (aref, atest, zw, zt, ssr, wssr, decal, two, decalnf, nonocc), o = rd('10B', b, o)
    r = dict(ver=ver, alphaTest=atest, alphaRef=aref, alphaBlend=ablend, twoSided=two)
    (refr, refrf), o = rd('BB', b, o); _, o = rd('f', b, o)
    (envm,), o = rd('B', b, o)
    if ver < 10:
        _, o = rd('f', b, o)
    _, o = rd('B', b, o)                      # grayscale to palette
    if ver >= 6:
        _, o = rd('B', b, o)                  # mask writes
    tex = []
    for _ in range(10 if ver >= 17 else 9):
        (n,), o = rd('I', b, o); tex.append(b[o:o + n].rstrip(b'\0').decode('latin1')); o += n
    (edref,), o = rd('B', b, o)
    if ver >= 8:
        (tr, trthick, trmix), o = rd('3B', b, o)
        r.update(translucency=tr)
    else:
        (rim, rimp, blp, sss, sssr), o = rd('BffBf', b, o)
        r.update(rim=rim, backlightPower=round(blp, 3), subsurface=sss, subsurfaceRolloff=round(sssr, 3))
    (spec,), o = rd('B', b, o)
    (sr, sg, sb, smult, smooth), o = rd('5f', b, o)
    r.update(specMult=round(smult, 3), smoothness=round(smooth, 3))
    r['diffuse'] = tex[0] if tex else ''
    return r


if __name__ == '__main__':
    for p in sys.argv[1:]:
        q = p if os.path.isabs(p) else os.path.join(DATA, p.replace('\\', '/'))
        if not os.path.exists(q):
            print('%-60s MISSING (%s)' % (p, q)); continue
        print('%-60s %s' % (p, bgsm(q)))
