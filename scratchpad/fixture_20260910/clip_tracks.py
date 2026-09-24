#!/usr/bin/env python
"""Decode a clip whose hkaAnimationBinding carries no transformTrackToBoneIndices,
under the IDENTITY assumption (track i -> bone i), and test that assumption
against skeleton.hkx's reference pose.

The test: in a skinned rig every joint's LOCAL translation is fixed by the
skeleton; only Root/COM translate.  So frame 0's per-track translation must
match referencePose[i].translation for the great majority of i under the right
mapping, and must NOT under a shifted one.  The shifted mapping is the control.

Also reports root-motion travel and the per-track quantization tally.
"""
import math
import os
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "..", "tests", "spells"))
import hkxanim_decode as D  # noqa: E402


def parse_nogate(path):
    grabbed = {}
    save = D.validate
    try:
        D.validate = lambda r: grabbed.setdefault("r", r)
        try:
            D.parse_hkx(path)
        except D.Refusal as e:
            print("(reader would refuse: %s)" % e)
    finally:
        D.validate = save
    return grabbed["r"]


def main(clip, skel):
    r = parse_nogate(clip)
    a = r["animations"][0]
    b = r["bindings"][0]
    frames, _ = D.decode(a, False)
    print("clip     %s" % os.path.basename(clip))
    print("  frames %d  tracks %d  duration %.6f s  frameDuration %.6f (%.2f fps)"
          % (a["numFrames"], a["numberOfTransformTracks"], a["duration"],
             a["frameDuration"], 1.0 / a["frameDuration"]))
    print("  blocks %d  maxFramesPerBlock %d  floatTracks %d  blendHint %s  originalSkeletonName %r"
          % (a["numBlocks"], a["maxFramesPerBlock"], a["numberOfFloatTracks"],
             b["blendHint"], b["originalSkeletonName"]))
    print("  transformTrackToBoneIndices len %d  floatTrackToFloatSlotIndices len %d  partitionIndices len %d"
          % (len(b["transformTrackToBoneIndices"]), len(b["floatTrackToFloatSlotIndices"]),
             len(b["partitionIndices"])))
    rm = a["rootMotion"]
    if rm is None:
        print("  rootMotion: none")
    else:
        s = rm["samples"]
        mx = max(max(abs(v) for v in q) for q in s)
        travel = math.dist(s[-1][:3], s[0][:3])
        print("  rootMotion: %d samples, up %s, max |component| %.6f, |last-first| %.6f, yaw span %.6f"
              % (len(s), tuple(round(v, 3) for v in rm["up"][:3]), mx, travel,
                 max(q[3] for q in s) - min(q[3] for q in s)))

    sk = parse_nogate(skel)["skeletons"][0]
    names, ref = sk["boneNames"], sk["referencePose"]
    print("skeleton %s: %r, %d bones" % (os.path.basename(skel), sk["name"], len(names)))

    n = min(len(names), a["numberOfTransformTracks"])
    f0 = frames[0]
    for shift in (0, 1, 2, -1):
        hits = 0
        worst = 0.0
        for i in range(n):
            j = i + shift
            if not (0 <= j < len(ref)):
                continue
            t = f0[i].get("translation") if isinstance(f0[i], dict) else f0[i][0]
            d = math.dist(t[:3], ref[j][0][:3])
            if d <= 1e-3:
                hits += 1
            worst = max(worst, d) if d <= 1e6 else worst
        print("  mapping track i -> bone i%+d : %d / %d translations within 1e-3" % (shift, hits, n))
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1], sys.argv[2]))
