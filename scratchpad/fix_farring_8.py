"""FARRING1 step 8: HANDOFF.md -- the lane's own status on its ledger line and
the narrative block above the newest one."""

P = 'HANDOFF.md'
b = open(P, 'rb').read()
assert b.count(b'\r') == 0
s = b.decode('utf-8')


def once(hay, needle):
    n = hay.count(needle)
    assert n == 1, 'anchor matched %d times, want 1:\n%s' % (n, needle[:200])


A = ("gate tests/spells/lodgen_farring.sh), window pid 33152, "
     "cwd E:/Projects/NifskopeWildWastelandEdition (read back).")
once(s, A)
s = s.replace(A, A + " FARRING1 CLOSED **BUILD PENDING**: the wrapper "
    "(bash tools/ww_build.sh) and every NifSkope command are refused to account B, so nothing "
    "was compiled or run — code, harness and docs are on disk and the overseer owns the build "
    "and the gates. Landed: lodgenSimplifyFarRings (after the merge, per (identity, layer) "
    "group, alpha-tested shapes and cards untouched, identity set invariant), the atlas bc1 "
    "flag + the mip-chain alpha fix it needed, CLI --no-simplify/--simplify8|16|32/"
    "--simplify-error/--atlas-bc1/--slot-fallback/--dump-geometry, the panel's Far-ring "
    "simplification rows (both targets) and their WW_LODGEN_TEST counts (number floor 8 -> 12), "
    "tests/spells/lodgen_farring.sh, spec + packing + WW_CHANGES 2026-09-06n. "
    "Report scratchpad/lane_farring_report.md.")

B = "**2026-09-06 — the emissive multiple rides in the `.lodm`, and nothing in\n"
once(s, B)
ADD = """**2026-09-06 — the far rings ship as proxies, and the atlas is DXT1 like
vanilla's.** bungo: "Proxy meshes for the far rings. Every engine since 2017
replaces far clusters with one simplified mesh per cell ... ring 2 and 3 chunks
could ship at a quarter of their triangles with the same textures."
`lodgenSimplifyFarRings` runs last, after the merge, because the merged shape
IS the cluster; ring 0 is never touched, ring 2 keeps 0.35 of its triangles and
ring 3 keeps 0.20. The cut is per (object identity index, texture-array layer)
group, so no collapse crosses an object or a layer, and since meshoptimizer
creates no vertices every channel of the packing contract arrives intact rather
than blended — the identity set of a chunk is INVARIANT under the pass, which
is what lets a manifest row still resolve at ring 3. Alpha-tested shapes and
impostor cards keep every triangle. Measured offline, with no exe: vanilla's
Commonwealth ships 3,708,637 object-LOD triangles at ring 0 across 344 chunks
and only 89,696 at ring 2 across 20 (17.5 a cell against 673.8), it has NO
dim-16 chunk over Sanctuary at all, and **0 of the 19,507 references in that
chunk has a base filling MNAM slot 2** (456 bases in the whole plugin do, and
51 fill slot 3) — so a far ring is empty without `--slot-fallback`, which is
new on the CLI and is what the harness builds rings 2 and 3 with. The atlas:
vanilla's `Commonwealth.Objects.DDS` is **DXT1**, 4096x2048, 13 mips,
5,592,552 bytes, not the BC3 our own comment claimed; `--atlas-bc1` (the stock
target's default in the panel) matches it, and needed a fix in `lodgenWriteDds`
where the mip filter forced BC1 alpha to 255 and would have turned every tree's
leaves back into a solid square below the top mip. New question:
`lodgen --dump-geometry FILE.BTO`. **BUILD PENDING** — this lane could not
compile or run anything; WW_CHANGES 2026-09-06n has every measured number, and
every one of them is of vanilla's files, not of ours.

"""
s = s.replace(B, ADD + B)

out = s.encode('utf-8')
assert out.count(b'\r') == 0
open(P, 'wb').write(out)
print('HANDOFF.md: %d -> %d bytes, CR %d' % (len(b), len(out), out.count(b'\r')))
