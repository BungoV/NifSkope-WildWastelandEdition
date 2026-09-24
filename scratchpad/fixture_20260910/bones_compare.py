#!/usr/bin/env python
"""Bone-name set comparison: skeleton.nif vs skeleton.hkx vs an assembled NIF.

Prints, case-insensitively:
  - how many names each file carries
  - names in skeleton.hkx with no node in the NIF (the HKX1 "17 Weapon*" truth)
  - names that differ only by case
  - names in the NIF that are not animation bones
and, when a third file is given, the fixture's own node set against skeleton.nif.
"""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(HERE, "..", "..", "tests", "spells"))
import nifhdr  # noqa: E402
import hkxanim_decode as D  # noqa: E402


def nif_nodes(path):
    h = nifhdr.read(path)
    return [nm for _, t, nm in nifhdr.node_names(h) if t.endswith("Node")]


def hkx_bones(path):
    g = {}
    save = D.validate
    try:
        D.validate = lambda r: g.setdefault("r", r)
        try:
            D.parse_hkx(path)
        except D.Refusal:
            pass
    finally:
        D.validate = save
    return g["r"]["skeletons"][0]["boneNames"]


def main(argv):
    skel_nif, skel_hkx = argv[1], argv[2]
    nifn = nif_nodes(skel_nif)
    hkxn = hkx_bones(skel_hkx)
    print("skeleton.nif  NiNodes: %d" % len(nifn))
    print("skeleton.hkx  bones  : %d" % len(hkxn))
    lown = {n.lower(): n for n in nifn}
    lowh = {n.lower(): n for n in hkxn}
    missing = [lowh[k] for k in lowh if k not in lown]
    print("hkx bones with NO node in skeleton.nif (%d): %s" % (len(missing), sorted(missing)))
    casediff = sorted((lowh[k], lown[k]) for k in lowh if k in lown and lowh[k] != lown[k])
    print("names differing only by case (%d): %s" % (len(casediff), casediff))
    extra = sorted(lown[k] for k in lown if k not in lowh)
    print("skeleton.nif nodes that are NOT animation bones (%d):" % len(extra))
    for i in range(0, len(extra), 6):
        print("    " + ", ".join(extra[i:i + 6]))

    if len(argv) > 3:
        fx = argv[3]
        fn = nif_nodes(fx)
        lowf = {n.lower(): n for n in fn}
        print("\nfixture %s NiNodes: %d" % (os.path.basename(fx), len(fn)))
        onlyskel = sorted(lown[k] for k in lown if k not in lowf)
        onlyfx = sorted(lowf[k] for k in lowf if k not in lown)
        print("  in skeleton.nif, NOT in fixture (%d): %s" % (len(onlyskel), onlyskel))
        print("  in fixture, NOT in skeleton.nif (%d): %s" % (len(onlyfx), onlyfx))
        # animation coverage
        hit = [lowh[k] for k in lowh if k in lowf]
        miss = sorted(lowh[k] for k in lowh if k not in lowf)
        print("  hkx bones matched by a fixture node (case-insensitive): %d / %d" % (len(hit), len(hkxn)))
        print("  hkx bones with no fixture node (%d): %s" % (len(miss), miss))
        dup = {}
        for n in fn:
            dup[n] = dup.get(n, 0) + 1
        d = {k: v for k, v in dup.items() if v > 1}
        print("  duplicate node names in the fixture: %s" % (d if d else "none"))
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
