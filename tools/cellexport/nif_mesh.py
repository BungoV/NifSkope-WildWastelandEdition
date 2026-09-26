"""Fallout 4 NIF (20.2.0.7, BS 130) -> triangle soup per shape, vectorised (lane CELL1).

Written from the field walk of tests/spells/gltf_nifread.py (which was checked
against release/nif.xml) and extended with what a whole-cell export needs:
  * numpy decode of the vertex buffer by the BSVertexDesc OFFSETS (positions,
    UV1, normals, vertex colours), not a per-vertex Python loop;
  * the shader property's Name (the .bgsm / .bgem the shape draws with);
  * NiAlphaProperty (alpha test / blend and the threshold);
  * the NiAVObject hidden bit (APP_CULLED, flag 0x1) on the shape and its
    ancestors, and nodes named EditorMarker, both skipped;
  * BSMeshLODTriShape: only its LOD0 triangle range is the full-detail mesh.

Every shape comes back in MODEL space (its own and its parents' transforms
applied, v' = t + R * (s * v), R read as the nine stored floats in order, the
convention NifSkope's Transform uses).
"""
import os
import struct

import numpy as np

NODE_TYPES = ('NiNode', 'BSFadeNode', 'BSLeafAnimNode', 'BSTreeNode', 'BSOrderedNode',
              'BSMultiBoundNode', 'BSValueNode', 'BSMasterParticleSystem', 'NiSwitchNode',
              'NiLODNode', 'NiBillboardNode', 'BSDamageStage', 'BSBlastNode', 'BSDebrisNode')
SHAPE_TYPES = ('BSTriShape', 'BSSubIndexTriShape', 'BSMeshLODTriShape')

VA_VERTEX, VA_UV, VA_UV2, VA_NORMALS, VA_TANGENTS, VA_COLORS = 0x1, 0x2, 0x4, 0x8, 0x10, 0x20
VA_SKINNED, VA_EYEDATA, VA_FULLPREC = 0x40, 0x100, 0x400


class NifError(Exception):
    pass


class Shape:
    __slots__ = ('name', 'pos', 'nrm', 'uv', 'col', 'tris', 'material', 'texture0', 'alpha_test',
                 'alpha_blend', 'alpha_ref', 'effect', 'vcol_used')


def _sized(d, o):
    n = struct.unpack_from('<I', d, o)[0]
    return d[o + 4:o + 4 + n].decode('latin-1'), o + 4 + n


def read(path_or_bytes):
    d = path_or_bytes if isinstance(path_or_bytes, (bytes, bytearray)) else open(path_or_bytes, 'rb').read()
    o = d.index(b'\n') + 1
    ver, = struct.unpack_from('<I', d, o); o += 4
    o += 5
    nblocks, = struct.unpack_from('<I', d, o); o += 4
    bsver, = struct.unpack_from('<I', d, o); o += 4
    if ver != 0x14020007 or bsver != 130:
        raise NifError('NIF version %08x BS %d is not Fallout 4' % (ver, bsver))
    for _ in range(4):
        n = d[o]; o += 1 + n
    ntypes, = struct.unpack_from('<H', d, o); o += 2
    types = []
    for _ in range(ntypes):
        s, o = _sized(d, o)
        types.append(s)
    tidx = struct.unpack_from('<%dH' % nblocks, d, o); o += 2 * nblocks
    bsize = struct.unpack_from('<%dI' % nblocks, d, o); o += 4 * nblocks
    nstr, = struct.unpack_from('<I', d, o); o += 8
    strings = []
    for _ in range(nstr):
        s, o = _sized(d, o)
        strings.append(s)
    ngroups, = struct.unpack_from('<I', d, o); o += 4 + 4 * ngroups
    btype, bstart = [], []
    for i in range(nblocks):
        btype.append(types[tidx[i]])
        bstart.append(o)
        o += bsize[i]

    def sref(i):
        return strings[i] if 0 <= i < len(strings) else ''

    def avobject(o):
        name = sref(struct.unpack_from('<i', d, o)[0]); o += 4
        ne, = struct.unpack_from('<I', d, o); o += 4 + 4 * ne + 4
        flags, = struct.unpack_from('<I', d, o); o += 4
        t = np.array(struct.unpack_from('<3f', d, o), dtype=np.float64); o += 12
        r = np.array(struct.unpack_from('<9f', d, o), dtype=np.float64).reshape(3, 3); o += 36
        s, = struct.unpack_from('<f', d, o); o += 4
        o += 4
        return name, flags, t, r, s, o

    nodes, shapes, texsets, shaders, alphas = {}, {}, {}, {}, {}
    for i in range(nblocks):
        tn, st = btype[i], bstart[i]
        if tn in NODE_TYPES:
            name, flags, t, r, s, oo = avobject(st)
            nc, = struct.unpack_from('<I', d, oo); oo += 4
            ch = struct.unpack_from('<%di' % nc, d, oo) if nc else ()
            nodes[i] = dict(name=name, flags=flags, t=t, r=r, s=s, children=ch, parent=None)
        elif tn in SHAPE_TYPES:
            shapes[i] = (tn, st, bsize[i])
        elif tn == 'BSShaderTextureSet':
            oo = st
            n, = struct.unpack_from('<I', d, oo); oo += 4
            tex = []
            for _ in range(n):
                s_, oo = _sized(d, oo)
                tex.append(s_)
            texsets[i] = tex
        elif tn == 'BSLightingShaderProperty':
            oo = st + 4
            name = sref(struct.unpack_from('<i', d, oo)[0]); oo += 4
            ne, = struct.unpack_from('<I', d, oo); oo += 4 + 4 * ne + 4
            f1, f2 = struct.unpack_from('<II', d, oo); oo += 8 + 16
            ts, = struct.unpack_from('<i', d, oo)
            shaders[i] = dict(name=name, texset=ts, source='', effect=False, f1=f1, f2=f2)
        elif tn == 'BSEffectShaderProperty':
            oo = st
            name = sref(struct.unpack_from('<i', d, oo)[0]); oo += 4
            ne, = struct.unpack_from('<I', d, oo); oo += 4 + 4 * ne + 4
            f1, f2 = struct.unpack_from('<II', d, oo); oo += 8 + 16
            src, oo = _sized(d, oo)
            shaders[i] = dict(name=name, texset=-1, source=src, effect=True, f1=f1, f2=f2)
        elif tn == 'NiAlphaProperty':
            oo = st + 4
            ne, = struct.unpack_from('<I', d, oo); oo += 4 + 4 * ne + 4
            fl, th = struct.unpack_from('<HB', d, oo)
            alphas[i] = (fl, th)

    parent = {}
    for i, n in nodes.items():
        for c in n['children']:
            if c >= 0:
                parent[c] = i

    def chain_ok_and_xform(i):
        """(visible, R, t) of block i's parents, root first."""
        R = np.eye(3); T = np.zeros(3)
        p = parent.get(i)
        chain = []
        while p is not None:
            n = nodes[p]
            if n['flags'] & 1 or n['name'].lower() == 'editormarker':
                return False, None, None
            chain.append(n)
            p = parent.get(p)
        for n in reversed(chain):
            T = T + R @ n['t']
            R = R @ (n['r'] * n['s'])
        return True, R, T

    out = []
    for i, (tn, st, size) in shapes.items():
        name, flags, t, r, s, oo = avobject(st)
        if flags & 1:
            continue
        vis, PR, PT = chain_ok_and_xform(i)
        if not vis:
            continue
        oo += 16
        oo += 4                                   # skin
        shader, alpha = struct.unpack_from('<ii', d, oo); oo += 8
        desc, = struct.unpack_from('<Q', d, oo); oo += 8
        ntri, = struct.unpack_from('<I', d, oo); oo += 4
        nv, = struct.unpack_from('<H', d, oo); oo += 2
        dsize, = struct.unpack_from('<I', d, oo); oo += 4
        stride = (desc & 0xF) * 4
        va = (desc >> 44) & 0xFFF
        if nv == 0 or ntri == 0 or dsize != nv * stride + ntri * 6 or not (va & VA_VERTEX):
            continue
        raw = np.frombuffer(d, dtype=np.uint8, count=nv * stride, offset=oo).reshape(nv, stride)
        if va & VA_FULLPREC:
            pos = raw[:, 0:12].copy().view('<f4').reshape(nv, 3).astype(np.float64)
        else:
            pos = raw[:, 0:6].copy().view('<f2').reshape(nv, 3).astype(np.float64)
        uv = nrm = col = None
        if va & VA_UV:
            u0 = ((desc >> 8) & 0xF) * 4
            uv = raw[:, u0:u0 + 4].copy().view('<f2').reshape(nv, 2).astype(np.float32)
        if va & VA_NORMALS:
            n0 = ((desc >> 16) & 0xF) * 4
            nrm = raw[:, n0:n0 + 3].astype(np.float64) / 127.5 - 1.0
        if va & VA_COLORS:
            c0 = ((desc >> 24) & 0xF) * 4
            col = raw[:, c0:c0 + 4].copy()
        oo += nv * stride
        tris = np.frombuffer(d, dtype='<u2', count=ntri * 3, offset=oo).reshape(ntri, 3).astype(np.uint32)
        oo += ntri * 6
        if tn == 'BSMeshLODTriShape':
            lod0, = struct.unpack_from('<I', d, oo)
            if 0 < lod0 <= ntri:
                tris = tris[:lod0]
        if tris.max() >= nv:
            continue
        # model-space transform: parents, then the shape's own
        R = PR @ (r * s)
        T = PT + PR @ t
        sh = Shape()
        sh.name = name
        sh.pos = pos @ R.T + T
        if nrm is not None:
            nn = nrm @ R.T
            ln = np.linalg.norm(nn, axis=1, keepdims=True)
            ln[ln == 0] = 1
            sh.nrm = nn / ln
        else:
            sh.nrm = None
        sh.uv, sh.col, sh.tris = uv, col, tris
        spr = shaders.get(shader)
        sh.material = spr['name'] if spr else ''
        sh.effect = bool(spr and spr['effect'])
        # SLSF2 bit 5 "Vertex_Colors": the colours only tint when the shader says so
        sh.vcol_used = bool(spr and not spr['effect'] and spr['f2'] & 0x20)
        sh.texture0 = ''
        if spr:
            if spr['effect']:
                sh.texture0 = spr['source']
            else:
                tex = texsets.get(spr['texset'])
                sh.texture0 = tex[0] if tex else ''
        a = alphas.get(alpha)
        sh.alpha_test = bool(a and a[0] & 0x200)
        sh.alpha_blend = bool(a and a[0] & 0x1)
        sh.alpha_ref = a[1] if a else 128
        out.append(sh)
    return out


def read_material(path_or_bytes):
    """(diffuse, alphaTest, alphaRef, twoSided, grayscaleToPalette) of a .bgsm / .bgem, or None.

    Base header (Material Editor's BaseMaterialFile): magic, version, tile
    flags, UV offset/scale, alpha, blend mode + src/dst, alpha test ref, alpha
    test, ten bools, refraction power, then (version < 10) env mapping bool +
    mask scale, else depth bias; grayscale-to-palette; (version >= 6) mask
    writes. The first string after it is the diffuse (BGSM) / base texture
    (BGEM)."""
    d = path_or_bytes if isinstance(path_or_bytes, (bytes, bytearray)) else open(path_or_bytes, 'rb').read()
    if len(d) < 64 or d[:4] not in (b'BGSM', b'BGEM'):
        return None
    ver, = struct.unpack_from('<I', d, 4)
    alpha_ref = d[41]
    alpha_test = bool(d[42])
    two_sided = bool(d[49])
    o = 57
    o += 5 if ver < 10 else 1
    gray_to_palette = bool(d[o])
    o += 1
    if ver >= 6:
        o += 1
    try:
        n, = struct.unpack_from('<I', d, o)
        if 0 < n < 512:
            s = d[o + 4:o + 4 + n].split(b'\0')[0].decode('latin-1')
            return s, alpha_test, alpha_ref, two_sided, gray_to_palette
        if n == 0:
            return '', alpha_test, alpha_ref, two_sided, gray_to_palette
    except struct.error:
        pass
    return None
