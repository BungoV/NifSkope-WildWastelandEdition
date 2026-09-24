"""Compare two hkx-tsv dumps bone for bone, frame for frame.

  python tests/spells/gltf_roundtrip_cmp.py ORIG.tsv ROUND.tsv [--tol 0.001]

Both files are the 13 columns `frame track bone tx ty tz qx qy qz qw sx sy sz`
that `NifSkope -no-gui hkx-tsv` writes, so the comparison is made on the exe's
OWN reader at both ends and the glTF in between is the only variable.

Rows are keyed on (frame, bone), which is why the round trip is run with
`--bones skeleton.hkx`: the importer then lays the tracks out in the SAME
skeleton order, and a bone the round trip did not carry is simply absent rather
than shifted. Tracks present in only one file are counted and named, never
silently dropped -- 17 weapon bones have no node in a character nif and are
expected to be missing, and anything else being missing is the finding.

Prints `RT <key> <value>` and exits 1 when a compared bone moved by more than
--tol (translation, in the file's own units) or --qtol (quaternion component).
Lane GLTFEXPORT1, 2026-09-19.
"""
import sys

argv = sys.argv[1:]
if len(argv) < 2:
    print("usage: gltf_roundtrip_cmp.py ORIG.tsv ROUND.tsv [--tol T] [--qtol Q] [--scale-moves]")
    sys.exit(2)


def opt(name, dflt):
    return float(argv[argv.index(name) + 1]) if name in argv else dflt


tol = opt("--tol", 0.001)
qtol = opt("--qtol", 0.002)
stol = opt("--stol", 0.001)


def load(path):
    rows = {}
    with open(path, "r", encoding="utf-8") as fh:
        for line in fh:
            if line.startswith("#") or not line.strip():
                continue
            p = line.split("\t")
            if len(p) < 13:
                continue
            frame, _track, bone = int(p[0]), int(p[1]), int(p[2])
            rows[(frame, bone)] = tuple(float(x) for x in p[3:13])
    return rows


a, b = load(argv[0]), load(argv[1])
print("RT orig_rows %d" % len(a))
print("RT round_rows %d" % len(b))
bones_a = sorted({k[1] for k in a})
bones_b = sorted({k[1] for k in b})
print("RT orig_bones %d" % len(bones_a))
print("RT round_bones %d" % len(bones_b))
only_a = [x for x in bones_a if x not in set(bones_b)]
print("RT bones_only_in_orig %d" % len(only_a))
print("RT bones_only_in_round %d" % len([x for x in bones_b if x not in set(bones_a)]))

shared = [k for k in a if k in b]
print("RT compared_rows %d" % len(shared))
if not shared:
    print("RT result FAIL no row is present in both files")
    sys.exit(1)

wt = wq = ws = 0.0
wtk = wqk = None
smove = 0.0
for k in shared:
    x, y = a[k], b[k]
    dt = max(abs(x[i] - y[i]) for i in range(3))
    dq = max(abs(x[i] - y[i]) for i in range(3, 7))
    ds = max(abs(x[i] - y[i]) for i in range(7, 10))
    if dt > wt:
        wt, wtk = dt, k
    if dq > wq:
        wq, wqk = dq, k
    ws = max(ws, ds)
    smove = max(smove, max(abs(v - 1.0) for v in y[7:10]))

print("RT worst_translation %.6f at frame %s bone %s" % ((wt,) + (wtk or ("-", "-"))))
print("RT worst_rotation %.6f at frame %s bone %s" % ((wq,) + (wqk or ("-", "-"))))
print("RT worst_scale %.6f" % ws)
print("RT round_scale_departure_from_1 %.6f" % smove)

bad = []
if wt > tol:
    bad.append("translation moved by %.6f (tol %.6f)" % (wt, tol))
if wq > qtol:
    bad.append("rotation moved by %.6f (tol %.6f)" % (wq, qtol))
if ws > stol:
    bad.append("scale moved by %.6f (tol %.6f)" % (ws, stol))
if "--scale-moves" in argv and smove <= stol:
    bad.append("no bone's scale left 1.0, so the scale tracks carried nothing")
print("RT result %s" % ("PASS" if not bad else "FAIL " + "; ".join(bad)))
sys.exit(0 if not bad else 1)
