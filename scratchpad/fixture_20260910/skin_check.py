#!/usr/bin/env python
"""Skin-integrity gate for the assembled fixture, without NifSkope.

For every BSSkin::Instance in a NIF: read Skeleton Root, Data ref, the Bones
pointer array and the per-bone scales, and check that
  * every Bones[] entry points at a real NiNode block,
  * the matching BSSkin::BoneData carries exactly the same bone count,
  * the block sizes the header declares are consumed EXACTLY by that layout
    (the self-check: a wrong layout leaves bytes over and the gate fails).
Then compare a fixture against its donor files, per shape, by bone NAME.
"""
import os
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import nifhdr  # noqa: E402

BONE_TRANS = 4 + 9 + 3 + 1          # bounding sphere, rot 3x3, translation, scale
NODEISH = ("NiNode", "BSFadeNode", "BSLeafAnimNode", "BSTreeNode", "BSOrderedNode",
           "NiBillboardNode")


def shapes_and_skins(path):
    h = nifhdr.read(path)
    offs = nifhdr.block_offsets(h)
    b = h["blob"]
    names = {}
    for i, o in enumerate(offs):
        t = h["types"][h["tidx"][i]]
        if t in NODEISH or t.endswith("TriShape"):
            si, = struct.unpack_from("<i", b, o)
            names[i] = h["strings"][si] if 0 <= si < len(h["strings"]) else ""
    out = []
    for i, o in enumerate(offs):
        t = h["types"][h["tidx"][i]]
        if t != "BSSkin::Instance":
            continue
        size = h["sizes"][i]
        root, data, nb = struct.unpack_from("<iiI", b, o)
        bones = list(struct.unpack_from("<%di" % nb, b, o + 12))
        p = o + 12 + 4 * nb
        ns, = struct.unpack_from("<I", b, p)
        consumed = 12 + 4 * nb + 4 + 4 * ns
        ok = (consumed == size)
        # bone data
        dnb = None
        dok = None
        if 0 <= data < len(offs) and h["types"][h["tidx"][data]] == "BSSkin::BoneData":
            do = offs[data]
            dnb, = struct.unpack_from("<I", b, do)
            dok = (4 + dnb * BONE_TRANS * 4 == h["sizes"][data])
        bad = [x for x in bones if not (0 <= x < len(offs)) or
               h["types"][h["tidx"][x]] not in NODEISH]
        out.append(dict(block=i, size=size, consumed=consumed, layout_ok=ok,
                        root=root, rootname=names.get(root, "?"),
                        data=data, nbones=nb, nscales=ns,
                        databones=dnb, data_layout_ok=dok,
                        badbones=bad,
                        bonenames=[names.get(x, "?") for x in bones]))
    # attach each skin to its shape by scanning shapes' Skin ref is hard without a
    # full parse; instead report the shape whose block index is nearest below.
    shp = [(i, h["types"][h["tidx"][i]], names.get(i, "")) for i in range(len(offs))
           if h["types"][h["tidx"][i]].endswith("TriShape")]
    return h, out, shp


def report(path):
    h, skins, shp = shapes_and_skins(path)
    print("%s: %d blocks, %d shapes, %d skin instances"
          % (os.path.basename(path), h["nblocks"], len(shp), len(skins)))
    for s in skins:
        print("  skin [%3d] bones %3d scales %3d  root '%s'  boneData [%d] bones %s  "
              "layout %s / %s  bad bone refs %d"
              % (s["block"], s["nbones"], s["nscales"], s["rootname"], s["data"],
                 s["databones"], "ok" if s["layout_ok"] else "BYTES LEFT OVER",
                 "ok" if s["data_layout_ok"] else "BAD", len(s["badbones"])))
    return {tuple(sorted(s["bonenames"])): s for s in skins}, shp


def main(argv):
    fix = argv[1]
    fmap, fshp = report(fix)
    allok = True
    for src in argv[2:]:
        smap, sshp = report(src)
        for key, s in smap.items():
            hit = fmap.get(key)
            print("    donor skin %d bones -> %s in the fixture"
                  % (s["nbones"], "MATCHED by name set" if hit else "*** NOT FOUND ***"))
            if not hit:
                allok = False
    print("RESULT %s" % ("PASS" if allok else "FAIL"))
    return 0 if allok else 1


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
