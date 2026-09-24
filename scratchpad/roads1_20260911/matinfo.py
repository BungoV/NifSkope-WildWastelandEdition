"""The two things the ROAD/DECAL test needs that gltf_nifread.py does not keep:
the shader property's NAME (the BGSM/BGEM path a Fallout 4 shape almost always
carries) and its Shader Flags 1 / 2; plus the BGSM's own `bDecal` /`bTwoSided`/
`bAlphaTest`/`iAlphaTestRef`, read field for field.

Field order for the decal group is taken from the same reader as
tools/lod_emission_probe.py's readBgsm (Material::readFile order): after
`iAlphaTestRef` come bAlphaTest, bZBufferWrite, bZBufferTest,
bScreenSpaceReflections, bWetnessControlSSR, **bDecal**, bTwoSided,
bDecalNoFade, bNonOccluder, bRefraction, bRefractionFalloff, fRefractionPower.

Fallout4ShaderPropertyFlags1 (release/nif.xml:7000): bit 26 Decal,
bit 27 Dynamic_Decal, bit 14 Landscape.
"""

import os
import struct

SEP = chr(92)
F4SF1_DECAL = 1 << 26
F4SF1_DYNAMIC_DECAL = 1 << 27


def shader_info(nif, shape):
    """(material name, flags1, flags2) for one shape, or None."""
    i = shape['shader']
    if i < 0 or i >= nif.numBlocks:
        return None
    t = nif.type[i]
    o = nif.start[i]
    if t == 'BSLightingShaderProperty':
        o += 4                                  # Shader Type
    elif t != 'BSEffectShaderProperty':
        return None
    name = nif._str(nif._u32(o)); o += 4
    ne = nif._u32(o); o += 4 + 4 * ne
    o += 4                                      # Controller
    f1 = nif._u32(o); o += 4
    f2 = nif._u32(o)
    return name, f1, f2


def find_material(dataRoot, rel):
    if not rel:
        return None
    p = rel.replace(SEP, '/').lower()
    i = p.find('materials/')
    if i >= 0:
        p = p[i:]
    elif not p.startswith('materials/'):
        p = 'materials/' + p
    full = os.path.join(dataRoot, p.replace('/', os.sep))
    if os.path.isfile(full):
        return full
    cur = dataRoot
    for part in p.split('/'):
        try:
            names = os.listdir(cur)
        except Exception:
            return None
        hit = next((n for n in names if n.lower() == part), None)
        if hit is None:
            return None
        cur = os.path.join(cur, hit)
    return cur if os.path.isfile(cur) else None


class _R(object):
    def __init__(self, b):
        self.b, self.o = b, 0

    def u8(self):
        v = self.b[self.o]; self.o += 1
        return v

    def u32(self):
        v = struct.unpack_from('<I', self.b, self.o)[0]; self.o += 4
        return v

    def f32(self):
        v = struct.unpack_from('<f', self.b, self.o)[0]; self.o += 4
        return v

    def s(self):
        n = self.u32()
        v = self.b[self.o:self.o + n].split(b'\0', 1)[0].decode('latin1')
        self.o += n
        return v


def read_material(path):
    """The decal group plus the alpha test, for BGSM and BGEM alike."""
    b = open(path, 'rb').read()
    if b[:4] not in (b'BGSM', b'BGEM'):
        return None
    kind = b[:4].decode('latin-1')
    r = _R(b[4:])
    v = r.u32()
    r.u32()                                     # bTileU/bTileV packed
    r.f32(); r.f32(); r.f32(); r.f32()          # UV offset / scale
    alpha = r.f32()
    r.u8(); r.u32(); r.u32()                    # bAlphaBlend, src, dst
    alphaTestRef = r.u8()
    alphaTest = r.u8()
    r.u8(); r.u8()                              # bZBufferWrite, bZBufferTest
    r.u8(); r.u8()                              # bSSR, bWetnessControlSSR
    decal = r.u8()
    twoSided = r.u8()
    decalNoFade = r.u8()
    nonOccluder = r.u8()
    return dict(kind=kind, version=v, alpha=alpha, alphaTest=bool(alphaTest),
                alphaTestRef=alphaTestRef, decal=bool(decal),
                twoSided=bool(twoSided), decalNoFade=bool(decalNoFade),
                nonOccluder=bool(nonOccluder))
