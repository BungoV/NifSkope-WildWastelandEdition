"""What frame is a body's inertia expressed in, and does a rebuild keep it?

WHY THIS EXISTS

dyn_inertia +0x20 is an INVERSE INERTIA DIAGONAL and +0x40 is the frame that
diagonal is expressed in. A diagonal without its frame is a different tensor, and
Compile used to leave the frame at the identity on every body it wrote. Nothing
caught it: Havok's own deserializer does not expose the dyn_inertia array, our
solver reproduced vanilla's joint drift either way, and five levels of structural
comparison called the files equivalent. In game it looked like corpses detonating
on death. bungo confirmed the cause on 2026-08-24 by splicing the stored
quaternions back into a rebuilt file.

So the invariant worth measuring is not the quaternion on its own -- it is the
TENSOR, R diag(I) R^T, which is what the physics actually uses. --compare reports
that, the quaternion agreement beside it, and what the tensor error WOULD have
been with the frame dropped, so a passing run can be told apart from a vacuous
one.

  python tools/hkinertia.py FILE.nif [--compare REBUILT.nif] [--exe PATH]
"""
import math
import os
import struct
import subprocess
import sys
import tempfile

EXE = os.environ.get("EXE") or os.path.join(
    os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "release", "NifSkope.exe")


def run(args):
    return subprocess.run([EXE, "-no-gui"] + args, capture_output=True, text=True,
                          errors="replace").stdout


def ragdoll_blob(nif, work, tag):
    """The file's one bhkRagdollSystem, as raw packfile bytes."""
    blk = [int(L.split("]")[0].lstrip("[")) for L in run(["list", nif]).splitlines()
           if L.strip().endswith("bhkRagdollSystem")]
    if len(blk) != 1:
        sys.exit("expected one bhkRagdollSystem in %s, found %d" % (nif, len(blk)))
    out = os.path.join(work, tag + ".bin")
    run(["collision", nif, "--extract", "-b", str(blk[0]), "-o", out])
    if not os.path.exists(out):
        sys.exit("could not extract the ragdoll packfile from " + nif)
    return open(out, "rb").read()


def sections(blob):
    """__data__ section start, and its local fixup map: source -> target."""
    n = struct.unpack_from("<i", blob, 20)[0]
    for i in range(n):
        off = 64 + i * 64
        if blob[off:off + 19].split(b"%c" % 0)[0] == b"__data__":
            v = struct.unpack_from("<7i", blob, off + 20)
            st = v[0]
            loc = {}
            for k in range((v[2] - v[1]) // 8):
                s, t = struct.unpack_from("<ii", blob, st + v[1] + k * 8)
                loc[s] = t
            return st, loc
    sys.exit("no __data__ section")


def bodies(blob):
    """[(inverseInertiaDiagonal, frameQuat_wxyz)] indexed by BODY.

    Indexed by body and not by record: dyn_inertia is addressed through the
    body's own MOTION INDEX at cinfo +0x0c, and the two files permute those
    differently. Comparing record N to record N is how this measurement lied
    twice before it was written down.
    """
    st, loc = sections(blob)
    ia, ca, cc = loc.get(0x30), loc.get(0x40), loc.get(0x50)
    if ia is None or ca is None or cc is None:
        sys.exit("no dyn_inertia / bodyCinfo / constraintCinfo array")
    nb = (cc - ca) // 0x60
    nmot = (ca - ia) // 0x70
    out = []
    for i in range(nb):
        mi = struct.unpack_from("<i", blob, st + ca + i * 0x60 + 0x0c)[0]
        if mi < 0 or mi >= nmot:
            out.append(None)          # a static body has no motion
            continue
        rec = st + ia + mi * 0x70
        inv = struct.unpack_from("<3f", blob, rec + 0x20)
        q = struct.unpack_from("<4f", blob, rec + 0x40)      # x y z w on disk
        out.append((inv, (q[3], q[0], q[1], q[2])))
    return out


def tensor(inv, q):
    d = [1.0 / v if v > 1e-12 else 0.0 for v in inv]
    w, x, y, z = q
    R = [[1 - 2 * (y * y + z * z), 2 * (x * y - z * w), 2 * (x * z + y * w)],
         [2 * (x * y + z * w), 1 - 2 * (x * x + z * z), 2 * (y * z - x * w)],
         [2 * (x * z - y * w), 2 * (y * z + x * w), 1 - 2 * (x * x + y * y)]]
    return [[sum(R[r][k] * d[k] * R[c][k] for k in range(3)) for c in range(3)]
            for r in range(3)]


def relerr(A, B):
    mx = max(abs(A[r][c]) for r in range(3) for c in range(3)) or 1.0
    return max(abs(A[r][c] - B[r][c]) for r in range(3) for c in range(3)) / mx


args = [a for a in sys.argv[1:]]
if not args:
    sys.exit(__doc__)
src = args[0]
cmp_path = None
if "--compare" in args:
    cmp_path = args[args.index("--compare") + 1]

work = tempfile.mkdtemp()
A = bodies(ragdoll_blob(src, work, "a"))
live = [b for b in A if b is not None]
ident = sum(1 for inv, q in live if abs(abs(q[0]) - 1.0) < 1e-6)
print("bodies %d" % len(A))
print("motions %d" % len(live))
print("identity %d" % ident)

if cmp_path:
    B = bodies(ragdoll_blob(cmp_path, work, "b"))
    if len(A) != len(B):
        sys.exit("body counts differ: %d vs %d" % (len(A), len(B)))
    worst_t = worst_flat = 0.0
    worst_dot = 1.0
    paired = 0
    ident_b = 0
    for a, b in zip(A, B):
        if a is None or b is None:
            continue
        paired += 1
        if abs(abs(b[1][0]) - 1.0) < 1e-6:
            ident_b += 1
        TA = tensor(a[0], a[1])
        worst_t = max(worst_t, relerr(TA, tensor(b[0], b[1])))
        # what the error would have been with the frame dropped, so a pass means
        # something: this is the number the old code was scoring
        worst_flat = max(worst_flat, relerr(TA, tensor(a[0], (1.0, 0.0, 0.0, 0.0))))
        worst_dot = min(worst_dot, abs(sum(p * q for p, q in zip(a[1], b[1]))))
    print("paired %d" % paired)
    print("identity-rebuilt %d" % ident_b)
    print("worst-tensor-rel %.3e" % worst_t)
    print("worst-dot %.6f" % worst_dot)
    print("frameless-tensor-rel %.3e" % worst_flat)
