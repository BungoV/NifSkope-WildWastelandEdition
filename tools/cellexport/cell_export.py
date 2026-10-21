"""Export a rectangle of Fallout 4 exterior cells as ONE model (lane CELL1, 2026-09-26).

Every placed object (SCOL parts expanded, material swaps applied) at the model
the chosen library names, transforms applied, plus the ground of the rectangle
from the plugin's LAND heights as its own named part. Writes a .glb (primary)
and/or a Wavefront .obj + .mtl, textures converted DDS -> PNG beside them.

  python cell_export.py <esm> <world> x0 y0 x1 y1 <dataRoot> <outStem>
        [--library near|lod0|lod1|lod2|lod3] [--ground <ground.dds>|none]
        [--tex-max 512] [--obj] [--no-glb] [--cache <plugin.pkl>]

--library near  = each base's MODL (the full-detail model)
--library lodN  = the STAT's MNAM slot N (the authored LOD model the object-LOD
                  generator draws at that ring; a placement with an empty slot
                  is DROPPED, exactly as the generator drops it)

Coordinates: glTF Y-up metres (1 game unit = 0.9144/64 m, NifSkope's own
GLTF_METRES_PER_UNIT), origin at the rectangle's centre on the XY plane and
z = 0; the offset is written to asset.extras so a result can be put back.
Game (x, y, z) -> glTF (x, z, -y).

The census it prints (and writes as <outStem>.census.txt) is the export's
account of itself: placements read, dropped by reason, models missing, shapes
skipped by reason, triangles/vertices/materials per part.
"""
import argparse
import json
import math
import os
import re
import struct
import sys
from collections import OrderedDict, defaultdict

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import esm_cells  # noqa: E402
import nif_mesh   # noqa: E402

METRES_PER_UNIT = 0.9144 / 64.0
CELL = 4096.0


def norm_rel(p, kind):
    """A game path as a lower-case path relative to Data\\<kind>\\."""
    p = p.replace('/', '\\').strip().lower().lstrip('\\')
    k = kind.lower() + '\\'
    if p.startswith(k):
        return p[len(k):]
    i = p.find('\\' + k)             # "C:\...\Data\materials\x.bgsm", "data\textures\x.dds"
    if i >= 0:
        return p[i + 1 + len(k):]
    return p


class Library:
    def __init__(self, data_root, plugin, stats):
        self.root = data_root
        self.plugin = plugin
        self.stats = stats
        self.models = {}
        self.mats = {}

    def model(self, rel):
        key = norm_rel(rel, 'meshes')
        if key in self.models:
            return self.models[key]
        path = os.path.join(self.root, 'Meshes', key)
        shapes = None
        if os.path.isfile(path):
            try:
                shapes = nif_mesh.read(path)
            except Exception as e:     # noqa: BLE001 -- counted, never silent
                self.stats['model_unreadable'] += 1
                self.stats['unreadable_list'].append('%s (%s)' % (key, e))
        else:
            self.stats['model_missing'] += 1
            self.stats['missing_list'].append(key)
        self.models[key] = shapes
        return shapes

    def material(self, rel):
        key = norm_rel(rel, 'materials')
        if key in self.mats:
            return self.mats[key]
        path = os.path.join(self.root, 'Materials', key)
        m = None
        if os.path.isfile(path):
            m = nif_mesh.read_material(path)
        self.mats[key] = m
        return m


def swapped(plugin, swap_form, mat):
    if not swap_form or not mat:
        return mat
    k = norm_rel(mat, 'materials')
    for orig, repl in plugin.mswp.get(swap_form, ()):
        if norm_rel(orig, 'materials') == k:
            return repl
    return mat


def to_gltf(p, origin):
    q = (p - origin) * METRES_PER_UNIT
    return np.stack([q[:, 0], q[:, 2], -q[:, 1]], axis=1).astype(np.float32)


def nrm_to_gltf(n):
    return np.stack([n[:, 0], n[:, 2], -n[:, 1]], axis=1).astype(np.float32)


def face_normals_per_vertex(pos, tris):
    n = np.zeros_like(pos)
    a, b, c = pos[tris[:, 0]], pos[tris[:, 1]], pos[tris[:, 2]]
    fn = np.cross(b - a, c - a)
    for k in range(3):
        np.add.at(n, tris[:, k], fn)
    ln = np.linalg.norm(n, axis=1, keepdims=True)
    ln[ln == 0] = 1
    return n / ln


class Bucket:
    def __init__(self, key, texture, alpha, alpha_ref, two_sided):
        self.key, self.texture, self.alpha, self.alpha_ref, self.two_sided = key, texture, alpha, alpha_ref, two_sided
        self.pos, self.nrm, self.uv, self.col, self.idx = [], [], [], [], []
        self.nv = 0

    def add(self, pos, nrm, uv, col, tris):
        self.pos.append(pos); self.nrm.append(nrm); self.uv.append(uv); self.col.append(col)
        self.idx.append(tris + self.nv)
        self.nv += len(pos)

    def arrays(self):
        return (np.concatenate(self.pos), np.concatenate(self.nrm), np.concatenate(self.uv),
                np.concatenate(self.col), np.concatenate(self.idx).astype(np.uint32))


def build_objects(plugin, lib, placements, library, origin, stats):
    buckets = OrderedDict()
    if not placements:
        return buckets
    slot = None if library == 'near' else int(library[3:])
    for pl in placements:
        if slot is None:
            rel = pl['model']
        else:
            rel = pl['mnam'][slot] if pl['mnam'] else ''
            if not rel:
                stats['dropped_empty_lod_slot'] += 1
                continue
        shapes = lib.model(rel)
        if not shapes:
            stats['placements_without_geometry'] += 1
            continue
        stats['placements_drawn'] += 1
        M, T = pl['M'], pl['T']
        for sh in shapes:
            if sh.effect:
                stats['effect_shapes_dropped'] += 1
                continue
            if sh.uv is None:
                stats['shapes_without_uv'] += 1
            mat = swapped(plugin, pl['swap'], sh.material)
            if mat != sh.material:
                stats['material_swaps_applied'] += 1
            tex, at, aref, two, gray = sh.texture0, sh.alpha_test, sh.alpha_ref, False, False
            if mat:
                m = lib.material(mat)
                if m is not None:
                    tex, at, aref, two, gray = (m[0] or tex), m[1], m[2], m[3], m[4]
                else:
                    stats['material_file_missing'] += 1
            texkey = norm_rel(tex, 'textures') if tex else ''
            alpha = 'MASK' if at else 'OPAQUE'
            key = (texkey, alpha, two, gray)
            b = buckets.get(key)
            if b is None:
                b = buckets[key] = Bucket(key, texkey, alpha, aref, two)
                b.gray = gray
                if gray:
                    stats['materials_grayscale_to_palette'] += 1
            wp = sh.pos @ M.T + T
            nrm = sh.nrm
            if nrm is None:
                nrm = face_normals_per_vertex(sh.pos, sh.tris)
            wn = nrm @ M.T
            ln = np.linalg.norm(wn, axis=1, keepdims=True); ln[ln == 0] = 1
            wn = wn / ln
            if np.linalg.det(M) < 0:
                tris = sh.tris[:, ::-1]
            else:
                tris = sh.tris
            uv = sh.uv if sh.uv is not None else np.zeros((len(wp), 2), np.float32)
            if sh.col is not None and sh.vcol_used:
                col = sh.col.copy(); col[:, 3] = 255
            else:
                col = np.full((len(wp), 4), 255, np.uint8)
            b.add(to_gltf(wp, origin), nrm_to_gltf(wn), uv.astype(np.float32), col, tris.astype(np.uint32))
            stats['shapes_drawn'] += 1
    return buckets


def build_ground(plugin, x0, y0, x1, y1, origin, stats):
    nx, ny = (x1 - x0 + 1) * 32 + 1, (y1 - y0 + 1) * 32 + 1
    H = np.full((ny, nx), np.nan)
    for cy in range(y0, y1 + 1):
        for cx in range(x0, x1 + 1):
            h = plugin.land.get((cx, cy))
            if h is None:
                stats['ground_cells_without_land'] += 1
                continue
            r0, c0 = (cy - y0) * 32, (cx - x0) * 32
            H[r0:r0 + 33, c0:c0 + 33] = np.fmax(np.nan_to_num(H[r0:r0 + 33, c0:c0 + 33], nan=-1e30), h)
    H[np.isnan(H) | (H < -1e29)] = 0.0
    ys, xs = np.mgrid[0:ny, 0:nx]
    wx = x0 * CELL + xs * 128.0
    wy = y0 * CELL + ys * 128.0
    pos = np.stack([wx.ravel(), wy.ravel(), H.ravel()], axis=1)
    u = (xs.ravel() / (nx - 1.0))
    v = 1.0 - (ys.ravel() / (ny - 1.0))            # north at the top: the vanilla .BTR's own UVs
    uv = np.stack([u, v], axis=1).astype(np.float32)
    q = np.arange(ny * nx).reshape(ny, nx)
    a, b, c, d = q[:-1, :-1].ravel(), q[:-1, 1:].ravel(), q[1:, 1:].ravel(), q[1:, :-1].ravel()
    tris = np.concatenate([np.stack([a, b, c], 1), np.stack([a, c, d], 1)]).astype(np.uint32)
    nrm = face_normals_per_vertex(pos, tris)
    col = np.full((len(pos), 4), 255, np.uint8)
    return to_gltf(pos, origin), nrm_to_gltf(nrm), uv, col, tris


# ------------------------------------------------------------------ textures

def convert_texture(data_root, rel, out_dir, tex_max, want_alpha, stats, cache, gray=False):
    """gray: the material looks its colour up in a palette ("grayscale to
    palette"); only the red channel is the grey value there, the other channels
    are not colour. The palette row is not read, so such a texture is written
    as plain grey (red channel), never as its raw channels."""
    key = (rel, want_alpha, gray)
    if key in cache:
        return cache[key]
    from PIL import Image
    name = re.sub(r'[^a-z0-9_]+', '_', rel.lower().replace('.dds', '')).strip('_') + ('_grey' if gray else '') + '.png'
    out = os.path.join(out_dir, name)
    src = os.path.join(data_root, 'Textures', rel)
    ok = None
    if os.path.isfile(out):
        ok = name
    elif rel and os.path.isfile(src):
        try:
            im = Image.open(src)
            im.load()
            im = im.convert('RGBA')
            if gray:
                r, g, b_, a_ = im.split()
                im = Image.merge('RGBA', (r, r, r, a_))
            if not want_alpha:
                im = im.convert('RGB')
            w, h = im.size
            f = min(1.0, tex_max / float(max(w, h)))
            if f < 1.0:
                im = im.resize((max(1, int(w * f)), max(1, int(h * f))), Image.LANCZOS)
            im.save(out, optimize=False, compress_level=6)
            ok = name
            stats['textures_converted'] += 1
        except Exception as e:   # noqa: BLE001
            stats['texture_unreadable'] += 1
            stats['texture_unreadable_list'].append('%s (%s)' % (rel, str(e)[:60]))
    elif rel:
        stats['texture_missing'] += 1
        stats['texture_missing_list'].append(rel)
    cache[key] = ok
    return ok


# ------------------------------------------------------------------ writers

def write_glb(path, parts, tex_dir, extras):
    """parts: [(nodeName, [(matName, texPng|None, alpha, aref, twoSided, pos, nrm, uv, col, idx)])]"""
    blob = bytearray()
    views, accessors, meshes, nodes, materials, images, textures = [], [], [], [], [], [], []
    img_index = {}

    def add_view(b, target=None):
        while len(blob) % 4:
            blob.append(0)
        off = len(blob)
        blob.extend(b)
        v = dict(buffer=0, byteOffset=off, byteLength=len(b))
        if target:
            v['target'] = target
        views.append(v)
        return len(views) - 1

    def add_acc(arr, ctype, typ, target, minmax=False, normalized=False):
        vi = add_view(arr.tobytes(), target)
        a = dict(bufferView=vi, componentType=ctype, count=int(arr.shape[0]), type=typ)
        if normalized:
            a['normalized'] = True
        if minmax:
            a['min'] = [float(x) for x in arr.min(0)]
            a['max'] = [float(x) for x in arr.max(0)]
        accessors.append(a)
        return len(accessors) - 1

    for node_name, prims in parts:
        gprims = []
        for (mname, png, alpha, aref, two, pos, nrm, uv, col, idx) in prims:
            mat = dict(name=mname, pbrMetallicRoughness=dict(metallicFactor=0.0, roughnessFactor=0.9))
            if png:
                if png not in img_index:
                    with open(os.path.join(tex_dir, png), 'rb') as fh:
                        iv = add_view(fh.read())
                    images.append(dict(name=png[:-4], mimeType='image/png', bufferView=iv))
                    textures.append(dict(source=len(images) - 1))
                    img_index[png] = len(textures) - 1
                mat['pbrMetallicRoughness']['baseColorTexture'] = dict(index=img_index[png])
            if alpha == 'MASK':
                mat['alphaMode'] = 'MASK'
                mat['alphaCutoff'] = round(aref / 255.0, 4)
            if two:
                mat['doubleSided'] = True
            materials.append(mat)
            attrs = dict(POSITION=add_acc(pos, 5126, 'VEC3', 34962, minmax=True),
                         NORMAL=add_acc(nrm, 5126, 'VEC3', 34962),
                         TEXCOORD_0=add_acc(uv, 5126, 'VEC2', 34962),
                         COLOR_0=add_acc(col, 5121, 'VEC4', 34962, normalized=True))
            ia = add_acc(idx.reshape(-1), 5125, 'SCALAR', 34963)
            gprims.append(dict(attributes=attrs, indices=ia, material=len(materials) - 1))
        meshes.append(dict(name=node_name, primitives=gprims))
        nodes.append(dict(name=node_name, mesh=len(meshes) - 1))
    while len(blob) % 4:
        blob.append(0)
    doc = dict(asset=dict(version='2.0', generator='NifSkope WW tools/cellexport (lane CELL1)', extras=extras),
               scene=0, scenes=[dict(name='chunk', nodes=list(range(len(nodes))))], nodes=nodes,
               meshes=meshes, materials=materials, accessors=accessors, bufferViews=views,
               buffers=[dict(byteLength=len(blob))])
    if images:
        doc['images'] = images
        doc['textures'] = textures
        doc['samplers'] = [dict(magFilter=9729, minFilter=9987, wrapS=10497, wrapT=10497)]
        for t in textures:
            t['sampler'] = 0
    js = json.dumps(doc, separators=(',', ':')).encode('utf-8')
    while len(js) % 4:
        js += b' '
    with open(path, 'wb') as fh:
        fh.write(struct.pack('<III', 0x46546C67, 2, 12 + 8 + len(js) + 8 + len(blob)))
        fh.write(struct.pack('<II', len(js), 0x4E4F534A)); fh.write(js)
        fh.write(struct.pack('<II', len(blob), 0x004E4942)); fh.write(blob)


def write_obj(path, parts, tex_rel):
    mtl = os.path.splitext(path)[0] + '.mtl'
    with open(mtl, 'w', newline='\n') as fm:
        for node_name, prims in parts:
            for (mname, png, alpha, aref, two, *_r) in prims:
                fm.write('newmtl %s\nKd 1 1 1\nKa 0 0 0\nKs 0 0 0\nd 1\nillum 1\n' % mname)
                if png:
                    fm.write('map_Kd %s/%s\n' % (tex_rel, png))
                    if alpha == 'MASK':
                        fm.write('map_d %s/%s\n' % (tex_rel, png))
                fm.write('\n')
    base = 1
    with open(path, 'w', newline='\n') as fo:
        fo.write('# lane CELL1 chunk export; Y-up metres, see the .glb asset.extras for the origin\n')
        fo.write('mtllib %s\n' % os.path.basename(mtl))
        for node_name, prims in parts:
            fo.write('o %s\n' % node_name)
            for (mname, png, alpha, aref, two, pos, nrm, uv, col, idx) in prims:
                fo.write('g %s__%s\nusemtl %s\n' % (node_name, mname, mname))
                fo.write(''.join('v %.5f %.5f %.5f\n' % tuple(p) for p in pos.tolist()))
                fo.write(''.join('vt %.5f %.5f\n' % (u, 1.0 - v) for u, v in uv.tolist()))
                fo.write(''.join('vn %.4f %.4f %.4f\n' % tuple(n) for n in nrm.tolist()))
                t = idx + base
                fo.write(''.join('f %d/%d/%d %d/%d/%d %d/%d/%d\n' % (a, a, a, b, b, b, c, c, c)
                                 for a, b, c in t.tolist()))
                base += len(pos)


# ------------------------------------------------------------------ main

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('esm'); ap.add_argument('world')
    ap.add_argument('x0', type=int); ap.add_argument('y0', type=int)
    ap.add_argument('x1', type=int); ap.add_argument('y1', type=int)
    ap.add_argument('data_root'); ap.add_argument('out_stem')
    ap.add_argument('--library', default='near')
    ap.add_argument('--ground', default='none')
    ap.add_argument('--tex-max', type=int, default=512)
    ap.add_argument('--tex-dir', default=None)
    ap.add_argument('--obj', action='store_true')
    ap.add_argument('--no-glb', action='store_true')
    ap.add_argument('--cache', default=None)
    ap.add_argument('--origin', default=None, help='x,y in game units (default: the rectangle centre); '
                    'use one origin for several exports that must line up')
    a = ap.parse_args()

    stats = defaultdict(int)
    for k in ('missing_list', 'unreadable_list', 'texture_missing_list', 'texture_unreadable_list'):
        stats[k] = []
    plugin = esm_cells.load_cached(a.esm, a.world, a.cache)
    placements, pc = esm_cells.expand(plugin, a.x0, a.y0, a.x1, a.y1)
    if a.library == 'none':                      # ground only
        placements = []
    origin = np.array([(a.x0 + a.x1 + 1) * CELL / 2.0, (a.y0 + a.y1 + 1) * CELL / 2.0, 0.0])
    if a.origin:
        origin[:2] = [float(v) for v in a.origin.split(',')]
    lib = Library(a.data_root, plugin, stats)
    buckets = build_objects(plugin, lib, placements, a.library, origin, stats)

    out_dir = os.path.dirname(os.path.abspath(a.out_stem))
    tex_dir = a.tex_dir or os.path.join(out_dir, 'textures')
    os.makedirs(tex_dir, exist_ok=True)
    tcache = {}
    prims = []
    tri_obj = vert_obj = 0
    for i, (key, b) in enumerate(buckets.items()):
        pos, nrm, uv, col, idx = b.arrays()
        png = convert_texture(a.data_root, b.texture, tex_dir, a.tex_max, b.alpha == 'MASK', stats, tcache,
                              gray=getattr(b, 'gray', False))
        mname = 'm%03d_%s' % (i, os.path.basename(b.texture)[:-4] if b.texture else 'untextured')
        prims.append((mname, png, b.alpha, b.alpha_ref, b.two_sided, pos, nrm, uv, col, idx))
        tri_obj += len(idx); vert_obj += len(pos)
    parts = [('Objects', prims)] if prims else []
    tri_g = vert_g = 0
    if a.ground != 'none':
        gp, gn, guv, gc, gi = build_ground(plugin, a.x0, a.y0, a.x1, a.y1, origin, stats)
        gpng = None
        gsrc = a.ground if os.path.isfile(a.ground) else os.path.join(a.data_root, a.ground)
        if os.path.isfile(gsrc):
            from PIL import Image
            gpng = 'ground_' + re.sub(r'[^a-z0-9_.-]+', '_', os.path.basename(gsrc).lower()).replace('.dds', '') + '.png'
            if not os.path.isfile(os.path.join(tex_dir, gpng)):
                Image.open(gsrc).convert('RGB').save(os.path.join(tex_dir, gpng))
        else:
            stats['ground_texture_missing'] += 1
        parts.append(('Ground', [('ground', gpng, 'OPAQUE', 128, False, gp, gn, guv, gc, gi)]))
        tri_g, vert_g = len(gi), len(gp)

    extras = dict(source='%s %s cells %d,%d..%d,%d' % (os.path.basename(a.esm), a.world, a.x0, a.y0, a.x1, a.y1),
                  library=a.library, metresPerUnit=METRES_PER_UNIT,
                  originGameUnits=[float(x) for x in origin],
                  axes='glTF (x, y, z) = metres * game (x, z, -y) about the origin')
    tex_rel = os.path.relpath(tex_dir, out_dir).replace('\\', '/')
    files = []
    if not a.no_glb:
        write_glb(a.out_stem + '.glb', parts, tex_dir, extras)
        files.append(a.out_stem + '.glb')
    if a.obj:
        write_obj(a.out_stem + '.obj', parts, tex_rel)
        files.append(a.out_stem + '.obj')

    lines = []
    lines.append('export %s library %s, cells %d,%d..%d,%d of %s' % (os.path.basename(a.out_stem), a.library,
                 a.x0, a.y0, a.x1, a.y1, a.world))
    lines.append('plugin: %s' % dict(pc))
    lines.append('placements %d, drawn %d, dropped (empty LOD slot) %d, no geometry %d'
                 % (len(placements), stats['placements_drawn'], stats['dropped_empty_lod_slot'],
                    stats['placements_without_geometry']))
    lines.append('distinct models %d (missing %d, unreadable %d)' % (len(lib.models), stats['model_missing'],
                                                                 stats['model_unreadable']))
    lines.append('shapes drawn %d, effect-shader shapes dropped %d, without UV %d, material swaps applied %d, '
                 'material file missing %d' % (stats['shapes_drawn'], stats['effect_shapes_dropped'],
                                               stats['shapes_without_uv'], stats['material_swaps_applied'],
                                               stats['material_file_missing']))
    lines.append('Objects: %d triangles, %d vertices, %d materials (%d grayscale-to-palette, written grey)'
                 % (tri_obj, vert_obj, len(prims), stats['materials_grayscale_to_palette']))
    lines.append('Ground: %d triangles, %d vertices, cells without LAND %d, ground texture missing %d'
                 % (tri_g, vert_g, stats['ground_cells_without_land'], stats['ground_texture_missing']))
    lines.append('textures: %d converted to PNG (max %d px), missing %d, unreadable %d'
                 % (len({p[1] for p in prims if p[1]}), a.tex_max, stats['texture_missing'],
                    stats['texture_unreadable']))
    for f in files:
        lines.append('file %s %d bytes' % (os.path.basename(f), os.path.getsize(f)))
    for k in ('missing_list', 'unreadable_list', 'texture_missing_list', 'texture_unreadable_list'):
        if stats[k]:
            lines.append('%s (%d): %s' % (k, len(stats[k]), '; '.join(sorted(set(stats[k]))[:40])))
    txt = '\n'.join(lines)
    print(txt)
    with open(a.out_stem + '.census.txt', 'w', newline='\n') as fh:
        fh.write(txt + '\n')
    return 0


if __name__ == '__main__':
    sys.exit(main())
