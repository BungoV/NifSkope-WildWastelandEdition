"""BUILD6: splice scratchpad/native0_20260910/WW_CHANGES_ENTRY.md into WW_CHANGES.md at the
top, marked as built; and mark the handoff README's lane-0 line baked with the measured time.
Byte splices; WW_CHANGES.md CR count must stay 19,020; README stays 0 CR."""
R = 'E:/Projects/NifskopeWildWastelandEdition/'

# ---- WW_CHANGES.md
P = R + 'WW_CHANGES.md'
b = open(P, 'rb').read(); cr0 = b.count(b'\r'); lf0 = b.count(b'\n')
e = open(R + 'scratchpad/native0_20260910/WW_CHANGES_ENTRY.md', 'rb').read().replace(b'\r', b'')
h_old = b"read back independently, not yet built into the exe\n"
h_new = b"read back independently, built into the exe 2026-09-10 (lane BUILD6)\n"
assert e.count(h_old) == 1, e.count(h_old)
e = e.replace(h_old, h_new)
i = e.find(b"**NOT done, and why.**")
assert i > 0
tail = e[i:]
j = tail.find(b"\n\n")
old_par = tail if j < 0 else tail[:j + 1]
new_par = """**Built 2026-09-10 03:57:46 by lane BUILD6** (the hook-up's 4 + 6 sites applied
from `HOOKUP_CHANGE_NEEDED.md` by anchor, 1 of 1 each, 0 CR; `qmake` then `make`,
rc 0; exe sha256 `664e0de4...`). Measured on it: the synthetic fixture written
through the exe is **byte-identical** to the standalone tool's pair and the
decoder reads it **46 checks, 0 failures**; the (0,0) 4x4-cell region at dim 4
(`--no-ao`) writes `Commonwealth.lodo` **5,696,484 B** (2,970 bases, 2,982
meshes, 10,634 clusters, 142,138 triangles; 4 `WrhsLeanTo*_LOD.nif` models
failed to load) and `Commonwealth.lodi` **136,992 B** (3,812 instances, 1
chunk), and at dim 8 / 16 / 32 the `.lodi` is 279,624 / 33,064 / 16,392 B
(8,329 / 549 / 1 instances) beside the same `.lodo`; `--native-verify` accepts
all five pairs; the decoder's ESM leg passes on all four dims (X/Y worst 0.1254
u, rotation worst 0.0048 deg incl. the tree yaw) and its manifest leg passes
base, membership and scale but **fails X/Y at 0.125 u on dim 4 / 8 / 16** (288
/ 1,118 / 92 rows, worst 0.174) -- measured to be the manifest's own
6-significant-digit print (step 0.1 above 10,000 u; with half that step budgeted,
0 coordinates exceed), so the bar, not the writer, and NOT re-pinned by the
build lane. **Lane 0 is baked**: `tests/baselines/stock_baseline.sha256`, 25
files off this exe, `bake-seconds 8` (the harness's REGION SET -- four chunks
incl. the (-32,0) dim-32 bucket-cap chunk plus a 2x2-cell region with arrays
and atlas, AO on -- not a worldspace), `--selftest` 2/2, `--check` **0 differ**;
`PENDING.txt` removed. Still owed: the manifest-leg bar (director), HOOKUP §C
step 5 (the (-32,0) asymmetric-drop proof, not run), the writer-mutation half
of lane 0, the two-instance order fixture, and a consumer in FO4CS. Numbers and
logs: `scratchpad/lane_native0_report.md` "## Build (BUILD6)".
"""
e = e.replace(old_par, new_par.encode('utf-8'))
anchor = b"## 2026-09-10 - marking water direction by hand"
assert b.count(anchor) == 1
if not e.endswith(b"\n"):
    e += b"\n"
b2 = b.replace(anchor, e + b"\n" + anchor)
assert b2.count(b'\r') == cr0 == 19020, (b2.count(b'\r'), cr0)
open(P, 'wb').write(b2)
print('WW_CHANGES.md: native entry spliced at the top; CR', cr0, '->', b2.count(b'\r'), 'LF', lf0, '->', b2.count(b'\n'))

# ---- scratchpad/handoff_fo4cs/README.md
Q = R + 'scratchpad/handoff_fo4cs/README.md'
r = open(Q, 'rb').read(); assert r.count(b'\r') == 0
old = (b"   chunk builder and the `--native` switch are a pending patch\n"
       b"   (`scratchpad/native0_20260910/HOOKUP_CHANGE_NEEDED.md`); lane 0's baseline\n"
       b"   (`tests/spells/lodgen_native_baseline.sh`) is written and not yet baked. The\n")
new = (b"   chunk builder and the `--native` switch were applied and built 2026-09-10\n"
       b"   03:57:46 (lane BUILD6; `--native <dir>` on a `--terrain-region` bake writes\n"
       b"   the pair, `--native-verify` reads one back; the (0,0) 4x4-cell region gives\n"
       b"   a 5,696,484 B `.lodo` and a 136,992 B `.lodi` for 3,812 instances at dim 4).\n"
       b"   Lane 0's baseline (`tests/spells/lodgen_native_baseline.sh`) is BAKED:\n"
       b"   `tests/baselines/stock_baseline.sha256`, 25 files off that exe, `--check` 0\n"
       b"   differ. **The first measured bake time in this tree: 8 s wall** for the\n"
       b"   harness's region set (chunks (-20,24) d4, (-24,24) d8, (-32,16) d16 with slot\n"
       b"   fallback, (-32,0) d32 -- the 42,560-placement bucket-cap chunk -- and the\n"
       b"   2x2-cell region (-20,24)..(-19,25) at d4 with arrays + atlas, AO on) on the\n"
       b"   2026-09-10 03:57:46 exe. That is NOT a whole-worldspace bake; a full\n"
       b"   Commonwealth object bake has not been timed by any lane (bungo's GUI bake\n"
       b"   will be the first number). The\n")
assert r.count(old) == 1, r.count(old)
r2 = r.replace(old, new)
assert r2.count(b'\r') == 0
open(Q, 'wb').write(r2)
print('handoff README: lane-0 line replaced; LF', r.count(b'\n'), '->', r2.count(b'\n'))
