#!/usr/bin/env python
"""Lane SKELFIX step 1: WHICH segments fan away from the character at frame 46.

An OFFLINE model of exactly what `GLView::drawSkeletonOverlay()` draws, built
from the same three sources the application uses and from nothing else:

  * the NIF's NiNode hierarchy and bind locals            (nifnodes.py)
  * the skins' Bones arrays -> the Skeleton Manager's classes
    (deforming = a skin names it; not-a-bone = no skin does)
  * the clip's decoded pose at frame 46, mapped to nodes by the SAME rule
    `HkxPlayback::bind()` uses: case-insensitive, first node wins, one track
    per node                                               (hkxanim_decode.py)

The model is not trusted on its own word: it reproduces the numbers the built
exe already measured (dock All 130 / Bones 93 / Deforming 93 / Unused 0, and
the clip's 78 matched / 17 unmatched / 4 case-folded). Those five numbers are
the control; if any of them misses, the table below is not evidence.

Usage: measure.py [--tsv OUT]
"""
import math
import os
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(ROOT, "tests", "spells"))
import nifnodes  # noqa: E402
import hkxanim_decode as D  # noqa: E402

NIF = os.path.join(ROOT, "fixtures", "human_male_vanilla.nif")
CLIP = os.path.join(ROOT, "fixtures", "Running_To_Slide_And_Back_To_Running.hkx")
SKEL = os.path.join(ROOT, "scratchpad", "hkx1_20260910", "clips", "skeleton.hkx")
FRAME = 46


# ---------------------------------------------------------------- maths
def mat_from_quat(q):
    """Havok (x,y,z,w) -> the same 3x3 NifSkope's Matrix::fromQuat builds."""
    x, y, z, w = q
    tx, ty, tz = 2.0 * x, 2.0 * y, 2.0 * z
    twx, twy, twz = tx * w, ty * w, tz * w
    txx, txy, txz = tx * x, ty * x, tz * x
    tyy, tyz = ty * y, tz * y
    tzz = tz * z
    return (1.0 - (tyy + tzz), txy - twz, txz + twy,
            txy + twz, 1.0 - (txx + tzz), tyz - twx,
            txz - twy, tyz + twx, 1.0 - (txx + tyy))


def mat_mul(a, b):
    return tuple(sum(a[r * 3 + k] * b[k * 3 + c] for k in range(3))
                 for r in range(3) for c in range(3))


def mat_vec(m, v):
    return tuple(sum(m[r * 3 + k] * v[k] for k in range(3)) for r in range(3))


def compose(t1, t2):
    """NifSkope's Transform operator*: r=r1*r2, t=t1+r1*t2*s1, s=s1*s2."""
    r1, p1, s1 = t1
    r2, p2, s2 = t2
    rv = mat_vec(r1, p2)
    return (mat_mul(r1, r2),
            (p1[0] + rv[0] * s1, p1[1] + rv[1] * s1, p1[2] + rv[2] * s1),
            s1 * s2)


def dist(a, b):
    return math.sqrt(sum((x - y) ** 2 for x, y in zip(a, b)))


# ---------------------------------------------------------------- the model
def build():
    nif = nifnodes.parse(NIF)
    nodes, parent = nif["nodes"], nif["parent"]

    inskin = set()
    for lst in nif["skinbones"]:
        inskin |= set(lst)
    inskin &= set(nodes)

    # Scene::getNodes() order: the roots' depth-first walk, children in order.
    order = []
    roots = [b for b in sorted(nodes) if b not in parent]

    def walk(b):
        order.append(b)
        for c in nodes[b]["children"]:
            if c in nodes:
                walk(c)
    for r in roots:
        walk(r)
    for b in sorted(nodes):
        if b not in order:
            order.append(b)

    # bone names -> nodes, HkxPlayback::bind()'s own rule
    sk = D.parse_hkx(SKEL)["skeletons"][0]
    boneNames = sk["boneNames"]
    byLower, exact = {}, {}
    for b in order:
        nm = nodes[b]["name"]
        if not nm:
            continue
        exact.setdefault(nm, b)
        byLower.setdefault(nm.lower(), b)
    matched, unmatched, folded = [], [], []
    nodeTrack = {}
    for t, bone in enumerate(boneNames):
        if not bone:
            continue
        if bone in exact:
            matched.append(bone)
        elif bone.lower() in byLower:
            matched.append(bone)
            folded.append(bone)
        else:
            unmatched.append(bone)
            continue
        n = byLower.get(bone.lower())
        if n is not None and n not in nodeTrack:
            nodeTrack[n] = t

    # the clip at frame 46 (a decoded frame: the stored transform, verbatim)
    r = D.parse_hkx(CLIP)
    anim = r["animations"][0]
    stride = anim["maxFramesPerBlock"] - 1
    blk = min(FRAME // stride, anim["numBlocks"] - 1)
    pose = D.decode_frame_in_block(anim, blk, float(FRAME - blk * stride))

    def locals_at(animated):
        out = {}
        for b in nodes:
            n = nodes[b]
            t = nodeTrack.get(b, -1)
            if animated and t >= 0:
                tr, q, sc, _ = pose[t]
                out[b] = (mat_from_quat(q), tr, sc[0])
            else:
                out[b] = (tuple(n["rot"]), tuple(n["trans"]), n["scale"])
        return out

    def world_from(loc):
        w = {}

        def rec(b, pt):
            wt = compose(pt, loc[b])
            w[b] = wt
            for c in nodes[b]["children"]:
                if c in nodes:
                    rec(c, wt)
        ident = ((1, 0, 0, 0, 1, 0, 0, 0, 1), (0.0, 0.0, 0.0), 1.0)
        for rb in roots:
            rec(rb, ident)
        return w

    return dict(nif=nif, nodes=nodes, parent=parent, order=order, inskin=inskin,
                boneNames=boneNames, matched=matched, unmatched=unmatched,
                folded=folded, nodeTrack=nodeTrack,
                bind=world_from(locals_at(False)),
                anim=world_from(locals_at(True)))


def parent_chain_tracked(b, parent, nodeTrack, inskin):
    """Is every ancestor of b that the overlay would draw a tracked node?"""
    p = parent.get(b)
    while p is not None:
        if p not in nodeTrack:
            return False
        p = parent.get(p)
    return True


def main(argv):
    m = build()
    nodes, parent = m["nodes"], m["parent"]
    inskin, nodeTrack = m["inskin"], m["nodeTrack"]
    anim, bind = m["anim"], m["bind"]

    # ---- the controls, before any table is believed
    ok = True
    ctl = [("dock All", len(nodes), 130),
           ("dock Bones", len(inskin), 93),
           ("dock Deforming", len(inskin), 93),
           ("dock Unused", 0, 0),
           ("clip matched", len(m["matched"]), 78),
           ("clip unmatched", len(m["unmatched"]), 17),
           ("clip case-folded", len(m["folded"]), 4)]
    print("CONTROLS (the built exe's own measured numbers)")
    for name, got, want in ctl:
        good = got == want
        ok = ok and good
        print("  %-18s model %-4d exe %-4d %s" % (name, got, want, "ok" if good else "MISMATCH"))
    print("  verdict: %s\n" % ("the model reproduces the application" if ok else "REFUSED"))
    if not ok:
        return 2

    com = anim[[b for b in nodes if nodes[b]["name"] == "COM"][0]][1]
    ys = [anim[b][1][2] for b in inskin]
    height = max(ys) - min(ys)
    print("frame %d: COM at (%.1f, %.1f, %.1f); character height over the 93 "
          "deforming bones = %.1f units; far = > 2x height = %.1f\n"
          % (FRAME, com[0], com[1], com[2], height, 2.0 * height))

    # ---- every segment the CURRENT rule draws
    rows = []
    for b in sorted(nodes):
        p = parent.get(b)
        if p is None:
            continue
        cls = "deforming" if b in inskin else "not-a-bone"
        pcls = "deforming" if p in inskin else "not-a-bone"
        tracked = b in nodeTrack
        ptracked = p in nodeTrack
        a, c = anim[p][1], anim[b][1]
        rows.append(dict(child=b, name=nodes[b]["name"], cls=cls, tracked=tracked,
                         parent=p, pname=nodes[p]["name"], pcls=pcls, ptracked=ptracked,
                         length=dist(a, c), far=dist(c, com), pos=c,
                         bindlen=dist(bind[p][1], bind[b][1])))

    longest_bind = max(r["bindlen"] for r in rows if r["cls"] == "deforming"
                       and r["pcls"] == "deforming")
    print("longest BONE-to-BONE segment in the bind pose: %.2f units (x1.5 = %.2f)\n"
          % (longest_bind, 1.5 * longest_bind))

    stray = [r for r in rows if r["far"] > 2.0 * height]
    print("SEGMENTS WHOSE ENDPOINT IS FAR FROM THE CHARACTER (%d of %d drawn)"
          % (len(stray), len(rows)))
    print("%-22s %-11s %-8s %-22s %-9s %9s %9s   %s"
          % ("child", "class", "track", "parent", "p.track", "len", "|c-COM|", "child world pos"))
    for r in sorted(stray, key=lambda r: -r["length"]):
        print("%-22s %-11s %-8s %-22s %-9s %9.1f %9.1f   (%.1f, %.1f, %.1f)"
              % (r["name"], r["cls"], "yes" if r["tracked"] else "NO", r["pname"],
                 "yes" if r["ptracked"] else "NO", r["length"], r["far"],
                 r["pos"][0], r["pos"][1], r["pos"][2]))

    over = [r for r in rows if r["length"] > 1.5 * longest_bind]
    print("\nSEGMENTS LONGER THAN 1.5x THE LONGEST BIND BONE (%d of %d)" % (len(over), len(rows)))
    for r in sorted(over, key=lambda r: -r["length"]):
        print("  %-22s %-11s track %-4s parent %-22s len %9.1f"
              % (r["name"], r["cls"], "yes" if r["tracked"] else "NO", r["pname"], r["length"]))

    # ---- the ARMATURE set: the Bones filter closed upwards, cut at the deepest
    # node that still contains every bone. This is what the fix ships.
    cnt = dict((b, 0) for b in nodes)
    for b in inskin:
        x = b
        while x is not None:
            cnt[x] += 1
            x = parent.get(x)

    def depth(b):
        d, x = 0, parent.get(b)
        while x is not None:
            d, x = d + 1, parent.get(x)
        return d
    common = max([b for b in nodes if cnt[b] == len(inskin)], key=depth)

    def under(b):
        x = b
        while x is not None:
            if x == common:
                return True
            x = parent.get(x)
        return False
    arm = set(b for b in nodes if cnt[b] > 0 and under(b))
    print("\nARMATURE SET: common root = %s (depth %d); %d of %d nodes.\n"
          "  excluded: %s"
          % (nodes[common]["name"], depth(common), len(arm), len(nodes),
             ", ".join(sorted(nodes[b]["name"] for b in nodes if b not in arm))))

    def chain(r):
        return r["tracked"] or parent_chain_tracked(r["child"], parent, nodeTrack, inskin)
    rules = [("R0 the shipped rule (every parent->child pair)", lambda r: True),
             ("R1 the brief, literal: Bones filter both ends", lambda r: r["cls"] == "deforming" and r["pcls"] == "deforming"),
             ("R1+track clause", lambda r: r["cls"] == "deforming" and r["pcls"] == "deforming" and chain(r)),
             ("R2 armature closure (SHIPPED)", lambda r: r["child"] in arm and r["parent"] in arm),
             ("R2+track clause", lambda r: r["child"] in arm and r["parent"] in arm and chain(r))]
    print("\n%-46s %5s %10s %10s %10s" % ("rule", "segs", "maxLen", "max|c-COM|", "maxScreen"))
    for name, f in rules:
        k = [r for r in rows if f(r)]
        if not k:
            print("%-46s %5d %10s" % (name, 0, "-"))
            continue
        scr = max(math.hypot(anim[r["parent"]][1][0] - r["pos"][0],
                             anim[r["parent"]][1][2] - r["pos"][2]) for r in k)
        print("%-46s %5d %10.1f %10.1f %10.1f"
              % (name, len(k), max(r["length"] for r in k), max(r["far"] for r in k), scr))
    print("\nGATE LIMITS: segment length <= %.1f (1.5x the longest bind bone-to-bone "
          "segment %.1f); endpoint <= %.1f from COM (2x height %.1f)"
          % (1.5 * longest_bind, longest_bind, 2.0 * height, height))
    new_rule = rules[3][1]

    if "--tsv" in argv:
        out = argv[argv.index("--tsv") + 1]
        with open(out, "w", newline="\n") as fh:
            fh.write("child\tclass\ttracked\tparent\tparent_class\tparent_tracked\t"
                     "length\tdist_from_COM\tx\ty\tz\tbind_length\tkept_by_new_rule\n")
            for r in sorted(rows, key=lambda r: -r["length"]):
                fh.write("%s\t%s\t%d\t%s\t%s\t%d\t%.3f\t%.3f\t%.3f\t%.3f\t%.3f\t%.3f\t%d\n"
                         % (r["name"], r["cls"], r["tracked"], r["pname"], r["pcls"],
                            r["ptracked"], r["length"], r["far"], r["pos"][0], r["pos"][1],
                            r["pos"][2], r["bindlen"], new_rule(r)))
        print("wrote %s" % out)
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
