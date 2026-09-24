#!/usr/bin/env python
"""NiNode hierarchy, local transforms and skin bone lists out of a FO4 NIF.

Enough of NiAVObject / NiNode / BSSkin::Instance to answer, WITHOUT launching
NifSkope: what nodes exist, who is whose parent, what each node's bind local
transform is, and which nodes some skin's Bones array names.

Every parse is SIZE-CHECKED against the header's own block-size table, so a
wrong field width shows up as a refusal rather than as plausible numbers
(CONSTITUTION rule 4: an invariant that fails on broken code).
"""
import os
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "fixture_20260910"))
import nifhdr  # noqa: E402


class Refusal(Exception):
    pass


NODE_TYPES = ("NiNode", "BSFadeNode", "BSLeafAnimNode", "BSTreeNode",
              "BSOrderedNode", "NiBillboardNode", "BSValueNode", "BSMasterParticleSystem")
SHAPE_TYPES = ("BSSubIndexTriShape", "BSTriShape", "BSDynamicTriShape",
               "BSMeshLODTriShape", "NiTriShape", "NiTriStrips")


def _u32(b, p):
    return struct.unpack_from("<I", b, p)[0], p + 4


def _i32(b, p):
    return struct.unpack_from("<i", b, p)[0], p + 4


def parse(path):
    h = nifhdr.read(path)
    b = h["blob"]
    offs = nifhdr.block_offsets(h)
    types = [h["types"][i] for i in h["tidx"]]
    nodes = {}          # block -> dict(name, children, trans, rot, scale)
    skinbones = []      # list of lists of block numbers
    shapeskin = {}      # shape block -> skin instance block

    for i, o in enumerate(offs):
        t = types[i]
        end = o + h["sizes"][i]
        if t in NODE_TYPES or t in SHAPE_TYPES:
            p = o
            si, p = _i32(b, p)
            nex, p = _u32(b, p)
            p += 4 * nex
            ctrl, p = _i32(b, p)
            flags, p = _u32(b, p)
            tr = struct.unpack_from("<3f", b, p); p += 12
            rot = struct.unpack_from("<9f", b, p); p += 36
            sc, = struct.unpack_from("<f", b, p); p += 4
            coll, p = _i32(b, p)
            name = h["strings"][si] if 0 <= si < len(h["strings"]) else ""
            if t in NODE_TYPES:
                nch, p = _u32(b, p)
                ch = list(struct.unpack_from("<%di" % nch, b, p)); p += 4 * nch
                # FO4 (BS 130) NiNode has NO Effects array -- measured: block 0
                # is name+4 extras+ctrl+flags+trs+coll+12 children = 140 bytes
                # exactly, and adding a Num Effects u32 overruns by 4.
                if p != end:
                    raise Refusal("block %d %s: parse ended at +%d, size %d"
                                  % (i, t, p - o, h["sizes"][i]))
                nodes[i] = dict(block=i, type=t, name=name,
                                children=[c for c in ch if c >= 0],
                                trans=tr, rot=rot, scale=sc)
            else:
                shapeskin[i] = (p, name)   # remember where the shape body continues
        elif t == "BSSkin::Instance":
            p = o
            root, p = _i32(b, p)
            data, p = _i32(b, p)
            nb, p = _u32(b, p)
            bones = list(struct.unpack_from("<%di" % nb, b, p)); p += 4 * nb
            nsc, p = _u32(b, p)
            p += 12 * nsc
            if p != end:
                raise Refusal("BSSkin::Instance %d: parse ended at +%d, size %d"
                              % (i, p - o, h["sizes"][i]))
            skinbones.append([x for x in bones if x >= 0])

    # parent from the Children arrays, exactly as skeletonAnalyse() does
    parent = {}
    for blk, n in nodes.items():
        for c in n["children"]:
            if c in nodes:
                parent[c] = blk
    return dict(hdr=h, types=types, nodes=nodes, parent=parent,
                skinbones=skinbones, shapes=shapeskin)


def main(argv):
    r = parse(argv[1])
    print("NiNode-like blocks: %d" % len(r["nodes"]))
    inskin = set()
    for lst in r["skinbones"]:
        inskin |= set(lst)
    print("skins: %d, distinct skin bones: %d" % (len(r["skinbones"]), len(inskin & set(r["nodes"]))))
    roots = [b for b in r["nodes"] if b not in r["parent"]]
    print("roots: %s" % [(b, r["nodes"][b]["name"]) for b in roots])
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
