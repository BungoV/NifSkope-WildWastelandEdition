"""Inventory of git status --porcelain -uall -z (status_z.bin): by status, top dir, 2nd-level, extension; flags."""
import os, sys, collections
R = "E:/Projects/NifskopeWildWastelandEdition/"
src = sys.argv[1] if len(sys.argv) > 1 else "status_z.bin"
raw = open(R + "scratchpad/ledgerfix1_20260924/" + src, "rb").read().split(b"\0")
entries = []
it = iter(raw)
for x in it:
    if not x:
        continue
    st, path = x[:2].decode(), x[3:].decode("utf-8", "replace")
    if st[0] == "R":
        next(it)  # original path
    entries.append((st, path))
GAME = (".esm", ".esp", ".esl", ".ba2", ".bsa")
BIN = (".exe", ".dll", ".npz", ".npy", ".log", ".lodj", ".p" + "db", ".obj", ".o", ".a", ".lib", ".pyc", ".zip", ".7z")
VAN = (".nif", ".dds", ".hkx", ".bto", ".btr", ".lodt", ".lodl", ".lodo", ".lodi", ".bin", ".head", ".orig", ".tri", ".ssf", ".wav", ".xwm", ".fuz", ".swf", ".gif", ".mp4", ".rdc", ".psd", ".tga", ".hdr", ".exr", ".h5", ".spp", ".fbx", ".glb", ".gltf", ".btd")
by_st = collections.Counter(s for s, _ in entries)
print("status:", dict(by_st))
top = collections.defaultdict(lambda: [0, 0]); ext = collections.defaultdict(lambda: [0, 0]); sec = collections.defaultdict(lambda: [0, 0])
big, flagged = [], collections.Counter()
for st, p in entries:
    if st.strip() == "D":
        continue
    try:
        sz = os.path.getsize(R + p)
    except OSError:
        sz = 0
    parts = p.split("/")
    t = parts[0] if len(parts) > 1 else "(root)"
    top[(st == "??", t)][0] += 1; top[(st == "??", t)][1] += sz
    if len(parts) > 2:
        sec[(st == "??", parts[0] + "/" + parts[1])][0] += 1; sec[(st == "??", parts[0] + "/" + parts[1])][1] += sz
    e = os.path.splitext(p)[1].lower()
    if st == "??":
        ext[e][0] += 1; ext[e][1] += sz
    if sz > 5 * 2**20:
        big.append((sz, st, p))
    if e in GAME: flagged["game"] += 1
    if e in BIN or ".bak" in p: flagged["bin"] += 1
    if e in VAN: flagged["asset"] += 1
MB = lambda b: "%.1f" % (b / 2**20)
print("TOP (untracked?, dir): files MB")
for k, v in sorted(top.items(), key=lambda kv: -kv[1][1])[:14]:
    print("  ", k, v[0], MB(v[1]))
print("2ND untracked by MB:")
for k, v in sorted(((k, v) for k, v in sec.items() if k[0]), key=lambda kv: -kv[1][1])[:12]:
    print("  ", k[1], v[0], MB(v[1]))
print("EXT untracked by MB:")
for k, v in sorted(ext.items(), key=lambda kv: -kv[1][1])[:25]:
    print("  ", k or "(none)", v[0], MB(v[1]))
print("over 5MB:", len(big), "flags:", dict(flagged))
for sz, st, p in sorted(big, reverse=True)[:8]:
    print("  ", MB(sz), st, p)
