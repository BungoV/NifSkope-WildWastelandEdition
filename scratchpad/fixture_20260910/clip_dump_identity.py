#!/usr/bin/env python
"""Dump a clip whose hkaAnimationBinding carries an EMPTY
transformTrackToBoneIndices, under the identity mapping, to the same TSV the
repo's two decoders write (frame track bone tx ty tz qx qy qz qw sx sy sz;
root motion as track -1).

This does NOT change the reader: it bypasses hkxanim_decode.validate() only for
the one binding rule, so lane FIXTURE can hand HKX2 the decoded rows without
touching src/hkxanim.cpp (one lane per file).
"""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "..", "tests", "spells"))
import hkxanim_decode as D  # noqa: E402


def main(clip, skel, out):
    def nogate(p):
        g = {}
        save = D.validate
        try:
            D.validate = lambda r: g.setdefault("r", r)
            try:
                D.parse_hkx(p)
            except D.Refusal:
                pass
        finally:
            D.validate = save
        return g["r"]

    a = nogate(clip)["animations"][0]
    rm = a["rootMotion"]
    names = nogate(skel)["skeletons"][0]["boneNames"]
    frames, _ = D.decode(a, False)
    n = 0
    with open(out, "w") as f:
        f.write("frame\ttrack\tbone\ttx\tty\ttz\tqx\tqy\tqz\tqw\tsx\tsy\tsz\n")
        for fi, row in enumerate(frames):
            if rm is not None:
                s = rm["samples"][fi]
                f.write("%d\t-1\t<rootmotion>\t%.6f\t%.6f\t%.6f\t0\t0\t0\t%.6f\t1\t1\t1\n"
                        % (fi, s[0], s[1], s[2], s[3]))
                n += 1
            for t, (tr, ro, sc, _q) in enumerate(row):
                bn = names[t] if t < len(names) else "<track%d>" % t
                f.write("%d\t%d\t%s\t%.6f\t%.6f\t%.6f\t%.6f\t%.6f\t%.6f\t%.6f\t%.6f\t%.6f\t%.6f\n"
                        % (fi, t, bn, tr[0], tr[1], tr[2], ro[0], ro[1], ro[2], ro[3],
                           sc[0], sc[1], sc[2]))
                n += 1
    print("wrote %s: %d rows (%d frames x %d tracks + %d root-motion rows)"
          % (out, n, len(frames), a["numberOfTransformTracks"],
             len(frames) if rm is not None else 0))
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1], sys.argv[2], sys.argv[3]))
