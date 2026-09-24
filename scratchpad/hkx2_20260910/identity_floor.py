#!/usr/bin/env python
"""Floor for the identity-map rule (lane HKX2b, 2026-09-10).

The rule, landed in BOTH decoders (src/hkxanim.cpp validate()/decodeClip() and
tests/spells/hkxanim_decode.py validate()): an EMPTY
hkaAnimationBinding::transformTrackToBoneIndices means the IDENTITY map and is
accepted; a NON-EMPTY vector of the wrong length is still refused by name.

A rule that simply deleted the check would also let the fixture through, so the
floor is the same real file with its binding filled in at the wrong length: the
refusal must still fire. Everything below runs on the real bytes of
fixtures/Running_To_Slide_And_Back_To_Running.hkx -- only the binding vector is
substituted.

Run:  python scratchpad/hkx2_20260910/identity_floor.py
"""
import copy
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
sys.path.insert(0, os.path.join(ROOT, "tests", "spells"))
import hkxanim_decode as D  # noqa: E402

CLIP = os.path.join(ROOT, "fixtures", "Running_To_Slide_And_Back_To_Running.hkx")


def parse_nogate(path):
    """Parse without the validation pass, so the raw result can be mutated."""
    grab = {}
    save = D.validate
    try:
        D.validate = lambda r: grab.setdefault("r", r)
        try:
            D.parse_hkx(path)
        except D.Refusal:
            pass
    finally:
        D.validate = save
    return grab["r"]


def run(name, indices, expect_refusal, base):
    r = copy.deepcopy(base)
    r["bindings"][0]["transformTrackToBoneIndices"] = indices
    try:
        D.validate(r)
        msg = None
    except D.Refusal as e:
        msg = str(e)
    ok = (msg is not None) == expect_refusal
    print("%-34s refusal=%-5s expected=%-5s %s%s"
          % (name, msg is not None, expect_refusal, "PASS" if ok else "FAIL",
             "" if msg is None else "  [%s]" % msg))
    return ok


def main():
    base = parse_nogate(CLIP)
    n = base["animations"][0]["numberOfTransformTracks"]
    stored = base["bindings"][0]["transformTrackToBoneIndices"]
    print("%s: %d transform tracks, binding carries %d indices"
          % (os.path.basename(CLIP), n, len(stored)))
    checks = [
        run("empty binding (the rule)", [], False, base),
        run("identity, full length", list(range(n)), False, base),
        run("a real permutation", list(range(n))[::-1], False, base),
        run("length %d (floor)" % (n - 1), list(range(n - 1)), True, base),
        run("length %d (floor)" % (n + 1), list(range(n + 1)), True, base),
        run("length 1 (floor)", [0], True, base),
    ]
    print("%d/%d PASS" % (sum(checks), len(checks)))
    return 0 if all(checks) else 1


if __name__ == "__main__":
    raise SystemExit(main())
