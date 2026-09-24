#!/usr/bin/env python3
"""The FLOOR under tests/spells/gltf_check.py (CONSTITUTION 4).

A structural validator that has never been shown failing is not a validator.
This writes a deliberately broken copy of a .gltf -- JSON only, the .bin is
reused untouched -- for each way the writer could plausibly go wrong, so the
gate can be watched going red before it is believed going green.

  buffer-length     buffers[0].byteLength += 4      -> the .bin size check
  view-overrun      the last bufferView runs 64 B past the buffer
  view-misalign     a bufferView byteOffset += 2    -> the 4-byte alignment
  accessor-count    an accessor's count += 1        -> count * size != length
  accessor-min      a POSITION accessor's min[0] += 1 -> min/max recomputation
  drop-min          delete a POSITION accessor's min/max -> the spec's demand
  joint-out-of-range  a skin joint index -> len(nodes)
  two-parents       give an existing node a second parent
  unreachable       drop a child from the scene root
  unnormalised      scale a node's rotation quaternion by 1.5
  sampler-mismatch  an animation sampler's output -> an accessor of another count
  bad-path          an animation channel path -> "colour"

Usage:
  gltf_sabotage.py IN.gltf OUTDIR            writes one file per kind, prints them
  gltf_sabotage.py IN.gltf OUTDIR --kind K   just that one

The runner (`tests/spells/gltf_gates.sh`) writes them all and asserts that
`gltf_check.py` exits non-zero on each and zero on the untouched original.
"""
import json
import os
import shutil
import sys

KINDS = ("buffer-length", "view-overrun", "view-misalign", "accessor-count",
         "accessor-min", "drop-min", "joint-out-of-range", "two-parents",
         "unreachable", "unnormalised", "sampler-mismatch", "bad-path")


def position_accessor(g):
    for m in g.get("meshes", []):
        a = m["primitives"][0]["attributes"].get("POSITION")
        if a is not None:
            return a
    return None


def apply(g, kind):
    """Break g in place. Returns a sentence naming what was broken, or None
    when this file cannot carry that defect (no animation, no skin, ...)."""
    if kind == "buffer-length":
        g["buffers"][0]["byteLength"] += 4
        return "buffers[0].byteLength += 4"
    if kind == "view-overrun":
        v = g["bufferViews"][-1]
        v["byteLength"] += 64
        return "the last bufferView is 64 bytes longer than the buffer allows"
    if kind == "view-misalign":
        for v in g["bufferViews"]:
            if v.get("byteOffset", 0) > 0:
                v["byteOffset"] += 2
                return "a bufferView byteOffset is no longer 4-byte aligned"
        return None
    if kind == "accessor-count":
        g["accessors"][0]["count"] += 1
        return "accessors[0].count += 1"
    if kind == "accessor-min":
        a = position_accessor(g)
        if a is None:
            return None
        g["accessors"][a]["min"][0] += 1.0
        return "a POSITION accessor's min[0] is a metre off the bytes"
    if kind == "drop-min":
        a = position_accessor(g)
        if a is None:
            return None
        g["accessors"][a].pop("min", None)
        g["accessors"][a].pop("max", None)
        return "a POSITION accessor has no min/max"
    if kind == "joint-out-of-range":
        if not g.get("skins"):
            return None
        g["skins"][0]["joints"][0] = len(g["nodes"])
        return "a skin joint is not a node index"
    if kind == "two-parents":
        for i, nd in enumerate(g["nodes"]):
            kids = nd.get("children")
            if kids:
                for j, other in enumerate(g["nodes"]):
                    if j != i and other.get("children") and other["children"][0] != kids[0]:
                        other["children"].append(kids[0])
                        return "node %d now has two parents" % kids[0]
        return None
    if kind == "unreachable":
        root = g["scenes"][g.get("scene", 0)]["nodes"][0]
        kids = g["nodes"][root].get("children")
        if not kids:
            return None
        dropped = kids.pop()
        return "node %d and its subtree are unreachable from the scene" % dropped
    if kind == "unnormalised":
        for nd in g["nodes"]:
            if "rotation" in nd:
                nd["rotation"] = [x * 1.5 for x in nd["rotation"]]
                return "a node rotation is not a unit quaternion"
        return None
    if kind == "sampler-mismatch":
        if not g.get("animations"):
            return None
        sm = g["animations"][0]["samplers"][0]
        want = g["accessors"][sm["output"]]["count"]
        for i, a in enumerate(g["accessors"]):
            if a["count"] != want:
                sm["output"] = i
                return "an animation sampler has %d inputs and %d outputs" % (want, a["count"])
        return None
    if kind == "bad-path":
        if not g.get("animations"):
            return None
        g["animations"][0]["channels"][0]["target"]["path"] = "colour"
        return "an animation channel drives the path 'colour'"
    raise SystemExit("unknown kind %r" % kind)


def main(argv):
    if len(argv) < 3:
        print(__doc__)
        return 2
    src, outdir = argv[1], argv[2]
    only = argv[argv.index("--kind") + 1] if "--kind" in argv else None
    os.makedirs(outdir, exist_ok=True)
    base = os.path.splitext(os.path.basename(src))[0]
    binsrc = os.path.join(os.path.dirname(os.path.abspath(src)),
                          json.load(open(src, encoding="utf-8"))["buffers"][0]["uri"])
    made = 0
    for kind in ([only] if only else KINDS):
        g = json.load(open(src, encoding="utf-8"))
        what = apply(g, kind)
        if what is None:
            print("SKIP %-20s this file cannot carry that defect" % kind)
            continue
        out = os.path.join(outdir, "%s.%s.gltf" % (base, kind))
        # every mutant keeps the writer's own buffer uri, so one .bin serves all
        g["buffers"][0]["uri"] = os.path.basename(binsrc)
        with open(out, "w", encoding="utf-8", newline="\n") as fh:
            json.dump(g, fh)
        dst = os.path.join(outdir, os.path.basename(binsrc))
        if not os.path.exists(dst) or os.path.getmtime(dst) < os.path.getmtime(binsrc):
            shutil.copyfile(binsrc, dst)
        print("%-20s %s  ->  %s" % (kind, what, out))
        made += 1
    return 0 if made else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv))
